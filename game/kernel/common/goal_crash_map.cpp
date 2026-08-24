#include "goal_crash_map.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "common/goal_constants.h"

#include "game/kernel/common/kscheme.h"
#include "game/runtime.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// psapi.h needs windows.h's typedefs (BOOL, DWORD, LPVOID, ...) already in scope, so it
// stays in its own include block: a blank line keeps clang-format from alphabetizing it
// ahead of windows.h.
#include <psapi.h>
#endif

namespace {

struct ObjRec {
  u32 start;
  u32 extent;  // bytes actually claimed on the heap; can be 0 for an object that
               // allocated nothing (a bring-up stub, see lookup() below)
  char name[32];
};

// sorted by start; guarded because link and crash can race in principle
std::vector<ObjRec> g_objs;
std::mutex g_objs_mutex;

// issue #602 step 1: the registered game's symbol-string table base, set by
// goal_crash_map_set_symbol_string_base() (jakx only today; see the doc comment on that
// declaration in goal_crash_map.h). 0 until registration, which format_receiver() below
// treats as "skip type-name resolution" rather than a valid table. No lock: written once
// at kscheme init, long before the fault handler can run, and read-only after that, the
// same reasoning g_objs's lookup() uses for skipping a lock in the fault path.
u32 g_symbol_string_base = 0;

// issue #716/#723: the running game's process-tree root and symbol-table bounds, set by
// goal_crash_map_set_process_pool_root() / goal_crash_map_set_symbol_table_region() (see
// those declarations in goal_crash_map.h for the full doc comment). Same reasoning as
// g_symbol_string_base above for skipping a lock: written once, long before the fault
// handler can run, read-only after that.
u32 g_process_pool_root = 0;
u32 g_symtab_lo = 0;
u32 g_symtab_hi = 0;

// issue #716 round 4: the running game's `process` type address, set by
// goal_crash_map_set_process_type() (see the header doc comment). Lets the crash
// handler's heap scan identify a process object by its type tag resolving to `process`
// or a descendant, instead of relying on the object being linked into *active-pool*'s
// tree -- a process mid-teardown or sitting in a dead pool has a live, correctly-tagged
// object with no tree link at all. Same posture as g_process_pool_root: 0 means
// "not registered", the scan's own no-op guard.
u32 g_process_type_addr = 0;

// issue #716 round 7: the ABSOLUTE HOST address of s7 (the #f slot) once a hardware
// execute breakpoint has been armed on it (goal_crash_map_arm_symbol_breakpoint()), 0
// otherwise. Round 6 proved every surviving-structure walk comes up empty because the
// crash lives inside a teardown window (*active-pool* down to one process, pp not even
// in it) -- nothing left standing after the fact can name the caller. This is the
// instrument that catches the jump AT instruction zero, before any of it: goal_crash_filter()
// checks a hit's rip against this address to distinguish "our armed breakpoint fired" from
// any other EXCEPTION_SINGLE_STEP.
u64 g_symbol_breakpoint_host_addr = 0;

// issue #117: two independent fixes over the old "start <= goal_addr, earliest
// record wins" scan.
//
//  - bounded match: goal_addr must now fall inside [start, start + extent), not
//    merely past start. An address past an object's real extent (the #115 capture
//    had a heap data pointer sitting 0x2812 bytes past gstate's 2224-byte code
//    segment) no longer matches that object. A record with extent 0 is a strict
//    side effect of this: goal_addr < start + 0 can never hold, so such a record
//    can never match anything.
//  - latest-wins tie-break: among every record whose range contains goal_addr,
//    the one pushed most recently always wins. g_objs is append-only in push
//    order, so a plain "last match found during the forward scan wins" is
//    exactly "most recently recorded" (issue #594/#595, level-heap reuse: a
//    level heap that gets freed and reused for a different level has no
//    removal step yet, so its old objects' records are still in g_objs when the
//    new level's objects get recorded on top of them). This originally shipped
//    (#117) as "r.start >= best->start", which only shadows correctly on an
//    exact shared start address: the #115 stub collision (target-death,
//    gun-util and menu are bring-up stubs that allocate nothing, so each logs
//    the same heap cursor as whatever loads next; drawable loaded right after
//    them at that same cursor) is such a case. But a reused heap's new object
//    does not have to share its stale predecessors' exact start address to
//    overlap them: a smaller old record sitting at a HIGHER start than a
//    larger new record that also covers it would win under "highest start
//    wins" even though it is the stale one. Preferring recency over start
//    address handles both the exact-tie case and this general one the same
//    way, since whatever was pushed last at an address is what is actually
//    resident there.
const ObjRec* lookup(u32 goal_addr) {
  // callable from the crash handler: no locking (a torn read of a vector that only
  // grows is survivable here, and taking a lock inside a fault handler is worse)
  const ObjRec* best = nullptr;
  for (const auto& r : g_objs) {
    if (r.start <= goal_addr && goal_addr < r.start + r.extent) {
      best = &r;
    }
  }
  return best;
}

// pure formatting for the "rip is not GOAL code" case: given a module's basename and
// base address plus the faulting rip, write "native: <basename>+0xOFFSET". No OS calls
// here (that part, module resolution via GetModuleHandleExW et al, is Windows-only and
// lives below in goal_crash_filter's else-branch); this half is plain arithmetic and
// snprintf, so unlike the fault handler around it, it is trivially unit-testable
// without a live fault or even a live module (issue #122).
void format_native_rip(const char* module_basename,
                       u64 module_base,
                       u64 rip,
                       char* out,
                       size_t out_size) {
  std::snprintf(out, out_size, "native: %s+%#llx", module_basename,
                (unsigned long long)(rip - module_base));
}

// pure decision for the access-kind word in the exception line (issue #602 step 1).
// ExceptionInformation[0] is documented (EXCEPTION_RECORD, MSDN) as 0 for a read
// violation, 1 for a write violation, and 8 for a DEP/execute-prevention violation. The
// previous code only distinguished write (nonzero) from read (zero), so an execute
// fault printed "writing" -- misleading for exactly the case this lane adds a receiver
// dump for: rip landing on the fault address (goal_crash_filter's rip == fault_addr
// check) is the execute-fault tell regardless of what ExceptionInformation[0] says, but
// the label printed next to it should say so too, not "writing". Falls back to
// "reading" for any other value, matching the old default.
const char* format_access_kind(u64 info0) {
  if (info0 == 1) {
    return "writing";
  }
  if (info0 == 8) {
    return "executing";
  }
  return "reading";
}

// pure formatting for one general-purpose register in the crash report. Two readings of
// the value are offered because GOAL code holds pointers both ways: a 64-bit absolute
// address (r15 + goal offset, the form the addressing modes use) and a raw 32-bit goal
// offset (what a `mov r9d, [..]` load of a symbol or field leaves in the register). Each
// reading that lands in GOAL memory is symbolized through the same object map as rip.
// The fault-address hint is the point of the whole line: a register whose value (in
// either reading) is the faulting address, or sits a small distance below it, is the
// base of the access that faulted, which turns "reading 0x...14b1" from an unattributed
// number into "the value held in r9 plus 0" (issue #376, where the same bad address
// recurred across builds with no way to tell which pointer carried it).
void format_reg(const char* name,
                u64 value,
                u64 base_addr,
                u64 mem_size,
                u64 fault_addr,
                char* out,
                size_t out_size) {
  int n = std::snprintf(out, out_size, "  %-3s %#018llx", name, (unsigned long long)value);
  if (n < 0 || (size_t)n >= out_size) {
    return;
  }
  auto append = [&](const char* fmt, auto... args) {
    if ((size_t)n < out_size) {
      int m = std::snprintf(out + n, out_size - n, fmt, args...);
      if (m > 0) {
        n += m;
      }
    }
  };
  auto symbolize = [&](u32 goal_addr) {
    const ObjRec* o = lookup(goal_addr);
    if (o) {
      append(" %s+%#x", o->name, goal_addr - o->start);
    }
  };
  if (base_addr && value >= base_addr && value < base_addr + mem_size) {
    u32 g = (u32)(value - base_addr);
    append(" (goal %#x", g);
    symbolize(g);
    append("%s", ")");
  } else if (value && value < mem_size) {
    append(" (goal-rel %#llx", (unsigned long long)value);
    symbolize((u32)value);
    append("%s", ")");
  }
  if (fault_addr) {
    // absolute reading first, then the raw-offset reading (r15-relative)
    u64 candidates[2] = {value, base_addr ? base_addr + value : 0};
    for (u64 c : candidates) {
      if (!c) {
        continue;
      }
      if (c == fault_addr) {
        append("%s", "  <- fault address");
        break;
      }
      if (fault_addr > c && fault_addr - c < 0x1000) {
        append("  <- fault address is this + %#llx", (unsigned long long)(fault_addr - c));
        break;
      }
    }
  }
}

// explicit-bounds-checked reads for the receiver dump below (issue #602 step 1), used
// instead of (and at the real call site, in addition to) the SEH-guarded
// safe_read_u32/safe_read_str further down: this needs to be provably safe against a
// small fabricated test buffer too, and a small buffer overrun is frequently NOT an
// access violation at all (it just reads adjacent heap memory), so SEH alone would not
// make "never reads outside the given window" a testable claim. Every read here is
// preceded by an explicit size check against window_size, with an overflow-safe form of
// the check (off + n < off catches off wrapping near UINT64_MAX) since off is derived
// from arithmetic on speculative, possibly-corrupt GOAL values.
bool bounded_read_u32(const u8* base, u64 window_size, u64 off, u32* out) {
  if (off + 4 < off || off + 4 > window_size) {
    return false;
  }
  u32 v;
  std::memcpy(&v, base + off, sizeof(v));
  *out = v;
  return true;
}

// issue #716/#723: same shape as bounded_read_u32 above, for the [sp] "return address
// slot" read (an 8-byte native x86-64 stack slot, since the thread's `sp` field is the
// process's own paused native rsp -- see format_thread_line()'s doc comment) instead of a
// 4-byte GOAL field.
bool bounded_read_u64(const u8* base, u64 window_size, u64 off, u64* out) {
  if (off + 8 < off || off + 8 > window_size) {
    return false;
  }
  u64 v;
  std::memcpy(&v, base + off, sizeof(v));
  *out = v;
  return true;
}

bool bounded_read_str(const u8* base, u64 window_size, u64 off, char* out, size_t out_size) {
  if (off > window_size || out_size == 0) {
    return false;
  }
  size_t i = 0;
  for (; i + 1 < out_size && off + i < window_size; i++) {
    char c = (char)base[off + i];
    if (!c) {
      break;
    }
    out[i] = c;
  }
  out[i] = 0;
  return i > 0;
}

// issue #602 step 1: the receiver dump for the rip == fault-address dispatch-fault case
// (an indirect call/jump landed in unmapped or non-code memory: goal_crash_filter's
// method-dispatch residual, gkdis-F60-execute-process-tree.txt in the issue -- `call r9`
// where r9 = [[rdi-4] + 0x40] + r15). GOAL method dispatch through a basic object loads
// the type tag from [receiver - 4], then the method-12 function pointer from
// [tag + 0x40] -- game/kernel/jakx/kscheme.h's Type struct starts its method table at
// +0x10 (new_method) with a 4-byte stride per Ptr<Function> slot, so index 12 lands at
// 0x10 + 12*4 = 0x40 -- adds r15, and calls it with the receiver in rdi (a0). A
// corrupted a0/a1 is the natural suspect for a fault of this shape, so this reads what
// each candidate register resolves to: the type tag at [reg - 4], and if that tag itself
// looks like a plausible basic pointer, the type's name via the same symbol-string-table
// indirection sym_to_string_ptr() uses (game/kernel/jakx/kscheme.h): symbol_string_base
// + type->symbol - s7_offset holds a Ptr<String>, whose chars start 4 bytes past its own
// value (String::len, game/kernel/common/kscheme.h, is the only preceding field).
// symbol_string_base is 0 (skip name resolution, print the tag only) unless a game has
// registered one via goal_crash_map_set_symbol_string_base() -- jakx is the only
// registrant today, see that declaration's doc comment -- so a game that hasn't opted in
// just gets the tag without a name, never a fault or a wrong-game misread.
//
// "Plausible basic pointer" (both for the receiver value and for the type tag) means:
// nonzero, less than window_size, and (candidate & OFFSET_MASK) == BASIC_OFFSET, i.e.
// offset mod 8 == 4 -- the same check game/kernel/jakx/kscheme.cpp's own type-validity
// tests use (e.g. `(type.offset & OFFSET_MASK) != BASIC_OFFSET` => invalid), verified
// against OFFSET_MASK=7 (game/kernel/common/kscheme.h) and BASIC_OFFSET=4
// (common/goal_constants.h) rather than assumed.
//
// The register value itself is read the same dual way format_reg() above does: an
// absolute r15-relative pointer first, then a raw 32-bit goal offset, matching how a
// register can hold either depending on what instruction last wrote it.
void format_receiver(const char* name,
                     u64 value,
                     const u8* base,
                     u64 window_size,
                     u32 s7_offset,
                     u32 symbol_string_base,
                     u64 rip,
                     u64 r15,
                     char* out,
                     size_t out_size) {
  int n = std::snprintf(out, out_size, "  recv %-3s %#018llx", name, (unsigned long long)value);
  if (n < 0 || (size_t)n >= out_size) {
    return;
  }
  auto append = [&](const char* fmt, auto... args) {
    if ((size_t)n < out_size) {
      int m = std::snprintf(out + n, out_size - n, fmt, args...);
      if (m > 0) {
        n += m;
      }
    }
  };

  u32 candidate = 0;
  bool have_candidate = false;
  if (r15 && value >= r15 && value < r15 + window_size) {
    candidate = (u32)(value - r15);
    have_candidate = true;
  } else if (value < window_size) {
    candidate = (u32)value;
    have_candidate = true;
  }
  if (!have_candidate || (candidate & OFFSET_MASK) != BASIC_OFFSET) {
    append("%s", "  (not a plausible basic pointer)");
    return;
  }
  append(" (goal %#x)", candidate);

  u32 tag = 0;
  if (!bounded_read_u32(base, window_size, (u64)candidate - 4, &tag)) {
    append("%s", "  (type tag out of window)");
    return;
  }
  bool tag_plausible = tag != 0 && tag < window_size && (tag & OFFSET_MASK) == BASIC_OFFSET;
  append("  tag %#x%s", tag, tag_plausible ? "" : " (not a plausible type pointer)");
  if (!tag_plausible) {
    return;
  }

  u32 symbol_offset = 0;
  bool have_name = false;
  char type_name[64] = {0};
  if (bounded_read_u32(base, window_size, (u64)tag, &symbol_offset) && symbol_string_base) {
    s64 name_ptr_addr = (s64)symbol_string_base + (s64)symbol_offset - (s64)s7_offset;
    if (name_ptr_addr >= 0) {
      u32 str_ptr = 0;
      if (bounded_read_u32(base, window_size, (u64)name_ptr_addr, &str_ptr) && str_ptr &&
          str_ptr < window_size) {
        have_name =
            bounded_read_str(base, window_size, (u64)str_ptr + 4, type_name, sizeof(type_name));
      }
    }
  }
  if (have_name) {
    append(" type-name \"%s\"", type_name);
  } else {
    append("%s", " (type name unresolved)");
  }

  // method index implied by the faulting dispatch (issue #602 step 1): [tag + 0x40] is
  // method slot 12 (see the function doc comment above); when it equals rip - r15, this
  // register's tag is confirmed as the actual dispatching receiver, not just a
  // plausible-looking bystander value.
  u32 slot_val = 0;
  if (bounded_read_u32(base, window_size, (u64)tag + 0x40, &slot_val)) {
    u64 rip_rel = (r15 && rip >= r15) ? (rip - r15) : 0;
    bool match = r15 && rip >= r15 && slot_val == (u32)rip_rel;
    append("  method slot +0x40 (index 12): %#x vs rip-r15 %#llx%s", slot_val,
           (unsigned long long)rip_rel,
           match ? " <- MATCH (this is the dispatching receiver)" : "");
  }
}

// issue #716 round 2: name a symbol-table slot address, the same string-table
// indirection format_receiver() above uses for a type's name (symbol_string_base +
// candidate - s7_offset holds a Ptr<String>, chars starting 4 bytes past its own value --
// jakx::sym_to_string_ptr()). Round 1's stack-of-blocks arithmetic (0x147d21 + 0x400e0 =
// 0x187e01, the #716 crash constant across both the garage-turntable and the "target"
// occurrence families) named 0x147d21 as this build's s7 and 0x400e0 as a stable
// slot-relative offset, meaning the crash's real target is not a random unmapped address
// but a SPECIFIC symbol slot -- this is the one-line verdict that names it: "jump target
// = symbol slot 'NAME' (bound|unbound)".
//
// Bound-ness: a GOAL symbol's value lives at candidate-1 (Symbol4<T>::value(), the same
// byte-precise -1 convention every other symbol read/write in this codebase already uses,
// e.g. kscheme.cpp's `ds_symbol->value() = ...`). An interned-but-never-`(define)`d
// symbol's value defaults to its OWN address as the VM's unbound sentinel (not 0), so
// "bound" is exactly "the value slot holds something other than the slot's own address".
bool format_symbol_slot(u32 candidate,
                        const u8* base,
                        u64 window_size,
                        u32 symtab_lo,
                        u32 symtab_hi,
                        u32 s7_offset,
                        u32 symbol_string_base,
                        char* out,
                        size_t out_size) {
  if (candidate < symtab_lo || candidate >= symtab_hi || !symbol_string_base) {
    return false;
  }
  s64 name_ptr_addr = (s64)symbol_string_base + (s64)candidate - (s64)s7_offset;
  if (name_ptr_addr < 0) {
    return false;
  }
  u32 str_ptr = 0;
  if (!bounded_read_u32(base, window_size, (u64)name_ptr_addr, &str_ptr) || !str_ptr ||
      str_ptr >= window_size) {
    return false;
  }
  char name[64] = {0};
  if (!bounded_read_str(base, window_size, (u64)str_ptr + 4, name, sizeof(name))) {
    return false;
  }
  u32 value = 0;
  bool have_value =
      (candidate >= 1) && bounded_read_u32(base, window_size, (u64)candidate - 1, &value);
  bool bound = have_value && value != candidate;
  std::snprintf(out, out_size, "symbol slot '%s' (%s)", name, bound ? "bound" : "unbound");
  return true;
}

// issue #716/#723: field offsets for the GOAL process-pool walk below, all "the
// deftype's :offset-assert value minus 4" -- the same conversion the existing pp fields
// just above (pp+0 is process-tree's `name`, offset-assert 4; pp+0x70 is process's
// `heap-top`, offset-assert 116) already establish: every offset in
// goal_src/*/kernel/gkernel-h.gc's deftype forms is measured from the object's true
// start (the type tag, 4 bytes before the conventional basic pointer), while every
// GOAL-space address this file works with is itself already a basic pointer. thread's
// pc/sp are not overridden by cpu-thread, so these two offsets apply to both a process's
// main-thread and its top-thread alike.
constexpr u64 PROCESS_TREE_MASK_OFF = 0x4;      // process-tree :offset-assert 8
constexpr u64 PROCESS_TREE_CHILD_OFF = 0x18;    // process-tree :offset-assert 28
constexpr u64 PROCESS_TREE_BROTHER_OFF = 0x14;  // process-tree :offset-assert 24
constexpr u64 PROCESS_NAME_OFF = 0x0;           // process-tree :offset-assert 4
constexpr u64 PROCESS_STATUS_OFF = 0x2C;        // process :offset-assert 48
constexpr u64 PROCESS_MAIN_THREAD_OFF = 0x34;   // process :offset-assert 56
constexpr u64 PROCESS_TOP_THREAD_OFF = 0x38;    // process :offset-assert 60
constexpr u64 THREAD_PC_OFF = 0x14;             // thread pc, 6th field after the type tag
constexpr u64 THREAD_SP_OFF = 0x18;             // thread sp, 7th field after the type tag
// issue #716 round 5: the rest of thread's own field layout (goal_src/*/kernel/gkernel-h.gc's
// `(deftype thread (basic) (name process previous suspend-hook resume-hook pc sp
// stack-top stack-size))`), same pointer-relative conversion as every other offset in
// this file. Round 3's disassembly proved the initializers for suspend-hook/resume-hook
// are parity-correct with jak3 (nothing to restore), which leaves "corrupted after
// construction" as the live theory; dumping every field directly off pp's own thread(s)
// -- no scan needed, pp is already in hand as ctx->R13 -- either names the clobbered
// field or clears all of them.
constexpr u64 THREAD_NAME_OFF = 0x0;
constexpr u64 THREAD_PROCESS_OFF = 0x4;
constexpr u64 THREAD_PREVIOUS_OFF = 0x8;
constexpr u64 THREAD_SUSPEND_HOOK_OFF = 0xC;
constexpr u64 THREAD_RESUME_HOOK_OFF = 0x10;
constexpr u64 THREAD_STACK_TOP_OFF = 0x1C;
constexpr u64 THREAD_STACK_SIZE_OFF = 0x20;
// cpu-thread extends thread (9 fields, ending at pointer-relative 0x24) with
// `(rreg uint64 7)`: the 7 general-purpose registers thread-suspend's asm body backs up
// wholesale (goal_src/*/kernel/gkernel.gc: `(set! (-> this rreg N) temp)` x7) and
// thread-resume restores wholesale on the way back into user code. Round 1's sweep only
// covered pc/sp; a bad value here is what actually lands in a real x86-64 register on
// resume.
constexpr u64 CPU_THREAD_RREG_OFF = 0x24;
constexpr int CPU_THREAD_RREG_COUNT = 7;

// a Type's `parent` field, C++-side game/kernel/jakx/kscheme.h's `Type::parent` at
// struct offset 4 (right after `symbol` at 0) -- the SAME layout the GOAL side's
// `deftype`s always use once you're already holding a basic pointer (a type's own
// address IS a basic pointer: format_receiver() above already asserts
// `(tag & OFFSET_MASK) == BASIC_OFFSET` before trusting it as one).
constexpr u64 TYPE_PARENT_OFF = 0x4;
// bounds the ancestor walk below against a corrupt or cyclic type chain; jakx's real
// type hierarchy is nowhere near this deep.
constexpr int MAX_TYPE_PARENT_HOPS = 24;
// caps how many heap-scan matches get a full per-process/per-thread dump, so a
// corrupted or unusually large heap can't flood the report; the total match count is
// still printed either way.
constexpr int MAX_HEAP_SCAN_REPORTS = 48;

// process-mask bit 8 (goal_src/*/kernel/gkernel-h.gc's `(process-tree 8)` defenum entry):
// set on a pool/container node (*active-pool*, *camera-pool*, ...), clear on a leaf
// `process` instance. gkernel.gc's own search-process-tree tests the identical bit the
// identical way -- `(not (logtest? (-> tree mask) (process-mask process-tree)))` is "this
// is a leaf" -- so this walk visits the tree exactly the way the kernel's own dispatcher
// does, not an approximation of it.
constexpr u32 PROCESS_TREE_MASK_BIT = 0x100;

// a corrupted tree (or, for that matter, a correct but unusually large one) must not
// loop or overflow the fixed work stack below inside a fault handler; both the visit
// budget and the work-stack array share this bound.
constexpr int MAX_POOL_WALK_NODES = 1024;
// issue #716 round 6: how many leaf process addresses walk_active_pool_dispatch_order()
// collects into its caller-provided fixed array; comfortably above any realistic live
// process count for this game, bounding the array itself, not just the tree walk that
// fills it.
constexpr int MAX_DISPATCH_ORDER_COLLECT = 512;

// issue #716/#723: format one line of the suspended-thread sweep for a single thread
// already read off a process's main-thread/top-thread field. pc is classified against
// the real object map the same way rip is (lookup(), hence this function -- like
// format_reg() above -- needs g_objs_mutex held by its caller/test seam) and, failing
// that, against the registered symbol-table region: issue #716's standing hypothesis is
// exactly a thread whose saved context resumes into that region rather than real code,
// so distinguishing "known-unpopulated symbol space" from "just never recorded" is the
// point of this line. The [sp] quadword (a native x86-64 stack slot: a suspended GOAL
// thread's `sp` is the process's own paused native rsp, the live version of which is
// exactly what the stack scan further down reads off ctx->Rsp) gets the same
// classification, but only when it resolves to a plausible absolute GOAL address first --
// mirroring the stack scan's own selectivity (it silently skips any quadword that isn't
// in GOAL range rather than flagging it), since most stack slots are not pointers at all
// and flagging every one would bury the real signal in noise.
void format_thread_line(const char* proc_name,
                        const char* role,
                        u32 pc,
                        u32 sp,
                        bool have_ra,
                        u64 ra,
                        u64 base_addr,
                        u64 mem_size,
                        u32 symtab_lo,
                        u32 symtab_hi,
                        char* out,
                        size_t out_size) {
  int n = std::snprintf(out, out_size, "  thread %s %s:", proc_name, role);
  if (n < 0 || (size_t)n >= out_size) {
    return;
  }
  auto append = [&](const char* fmt, auto... args) {
    if ((size_t)n < out_size) {
      int m = std::snprintf(out + n, out_size - n, fmt, args...);
      if (m > 0) {
        n += m;
      }
    }
  };
  auto classify = [&](u32 addr) {
    const ObjRec* o = lookup(addr);
    if (o) {
      append(" %s+%#x [%#x,+%#x)", o->name, addr - o->start, o->start, o->extent);
    } else if (addr >= symtab_lo && addr < symtab_hi) {
      append("%s", " <- IN SYMBOL TABLE REGION (FLAG)");
    } else {
      append("%s", " <- UNMAPPED (FLAG)");
    }
  };

  append(" pc (goal %#x)", pc);
  classify(pc);
  append("  sp (goal %#x)", sp);
  if (!have_ra) {
    append("%s", "  [sp] (out of window)");
  } else {
    append("  [sp] %#018llx", (unsigned long long)ra);
    if (ra >= base_addr && ra < base_addr + mem_size) {
      u32 g = (u32)(ra - base_addr);
      append(" (goal %#x)", g);
      classify(g);
    }
  }
}

// issue #716/#723: walk the live GOAL process pool from its registered root (jakx's
// *active-pool*, see goal_crash_map_set_process_pool_root()'s doc comment) the same way
// the kernel's own dispatcher does: recurse child/brother, and a node is a leaf `process`
// rather than a pool/container exactly when PROCESS_TREE_MASK_BIT is clear. For every
// leaf with at least one thread, print its main-thread and (if distinct) top-thread saved
// pc/sp -- issue #716's standing hypothesis in one line per thread. An explicit work
// stack instead of recursion: a fault handler is the wrong place to trust the C++ call
// stack has headroom, and MAX_POOL_WALK_NODES bounds it regardless of tree shape. Every
// read is the same bounds-checked-against-window_size read the rest of this file already
// uses in the fault path (format_receiver() above), so this needs no additional SEH
// guarding beyond what bounded_read_u32/bounded_read_u64 already provide.
// GOAL's #f is not the integer 0: it is the address of the s7 symbol itself
// (common/symbols.h: "FIX_SYM_FALSE = 0x0 ... this is equal to the $s7 register"). Every
// process-tree pointer field (parent/brother/child) that `new process-tree` initializes
// to #f (goal_src/*/kernel/gkernel.gc) therefore stores s7.offset, not 0, and a plain
// "is this field nonzero" check follows #f as though it were a real node -- reading
// whatever memory happens to sit near the symbol table and silently derailing the walk.
// Both sentinels have to be checked.
bool is_present_ptr(u32 addr, u32 false_addr) {
  return addr != 0 && addr != false_addr;
}

// issue #716 round 5: classify one raw thread/cpu-thread field value for the pp
// thread-field dump below. A stored field is always a raw goal-relative offset (never a
// live register), so no register-style dual reading is needed. Tries, in order: the
// real code/object map (lookup(), what a real function-pointer hook like suspend-hook or
// resume-hook should resolve to), then the registered symbol-table region
// (format_symbol_slot(), what a corrupted hook holding #f or an unbound symbol resolves
// to instead -- this is the exact discriminator the dump exists to surface), then a bare
// UNMAPPED flag. A raw zero gets its own explicit tag rather than falling through to
// "unmapped", since 0 is a distinct, meaningful reading (round 1/2's "zero where a code
// pointer belongs" class, separate from #f).
void format_thread_field(const char* field_name,
                         u32 value,
                         const u8* base,
                         u64 window_size,
                         u32 symtab_lo,
                         u32 symtab_hi,
                         u32 s7_offset,
                         u32 symbol_string_base,
                         char* out,
                         size_t out_size) {
  int n = std::snprintf(out, out_size, "    %-14s (goal %#x)", field_name, value);
  if (n < 0 || (size_t)n >= out_size) {
    return;
  }
  auto append = [&](const char* fmt, auto... args) {
    if ((size_t)n < out_size) {
      int m = std::snprintf(out + n, out_size - n, fmt, args...);
      if (m > 0) {
        n += m;
      }
    }
  };
  if (value == 0) {
    append("%s", " <- ZERO");
    return;
  }
  const ObjRec* o = lookup(value);
  if (o) {
    append(" %s+%#x [%#x,+%#x)", o->name, value - o->start, o->start, o->extent);
    return;
  }
  char slot[96];
  if (format_symbol_slot(value, base, window_size, symtab_lo, symtab_hi, s7_offset,
                         symbol_string_base, slot, sizeof(slot))) {
    append(" <- %s", slot);
    return;
  }
  append("%s", " <- UNMAPPED (FLAG)");
}

// issue #716 round 5/6: dump every field of a process's own thread(s) directly -- no
// scan, just the fixed offsets off an already-known process address (pp, or one found
// by the dispatch-order walk below). Round 4's raw stack window showed 256 bytes of
// zeros under a lone return-into-thread-suspend at [rsp+0]: exactly the shape of a
// suspended thread's own shallow stack, consistent with the kernel dispatcher having
// switched onto some process's saved thread context and called a hook field holding #f
// instead of resuming through the saved pc. Round 3 already proved the initializers
// (cpu-thread's `new`, `activate`) are byte-identical to jak3, so if a hook is bad here
// it was corrupted after construction, not left unset -- naming WHICH field is bad turns
// "something clobbers this thread" into a targeted hunt for the writer.
//
// Round 6: prints BOTH main-thread and top-thread unconditionally, explicitly noting
// when they are the same address, rather than silently deduplicating (round 5's own
// version skipped top-thread whenever it matched main-thread, which is an inference,
// not a verification, of the standing "this codebase's documented reading rule: pp
// names the PREVIOUS process for a dispatch-time fault, not the one actually
// mid-dispatch" trap this round is working around).
void dump_process_thread_fields(u32 proc,
                                const char* proc_label,
                                const u8* base,
                                u64 window_size,
                                u32 symtab_lo,
                                u32 symtab_hi,
                                u32 s7_offset,
                                u32 symbol_string_base,
                                u32 false_addr) {
  fprintf(stderr, "%s thread fields (direct, no scan):\n", proc_label);
  u32 main_thread = 0;
  u32 top_thread = 0;
  bool have_main =
      bounded_read_u32(base, window_size, (u64)proc + PROCESS_MAIN_THREAD_OFF, &main_thread);
  bool have_top =
      bounded_read_u32(base, window_size, (u64)proc + PROCESS_TOP_THREAD_OFF, &top_thread);
  struct {
    const char* role;
    u32 addr;
    bool valid;
  } threads[2] = {
      {"main-thread", main_thread, have_main && is_present_ptr(main_thread, false_addr)},
      {"top-thread", top_thread, have_top && is_present_ptr(top_thread, false_addr)},
  };
  bool printed_any = false;
  for (const auto& t : threads) {
    if (!t.valid || t.addr >= window_size || (t.addr & OFFSET_MASK) != BASIC_OFFSET) {
      continue;
    }
    printed_any = true;
    fprintf(stderr, "  %s (goal %#x)%s:\n", t.role, t.addr,
            (t.addr == main_thread && t.addr == top_thread && std::string(t.role) == "top-thread")
                ? " -- same address as main-thread, verified not inferred"
                : "");
    struct {
      const char* name;
      u64 off;
    } fields[] = {
        {"name", THREAD_NAME_OFF},
        {"process", THREAD_PROCESS_OFF},
        {"previous", THREAD_PREVIOUS_OFF},
        {"suspend-hook", THREAD_SUSPEND_HOOK_OFF},
        {"resume-hook", THREAD_RESUME_HOOK_OFF},
        {"pc", THREAD_PC_OFF},
        {"sp", THREAD_SP_OFF},
        {"stack-top", THREAD_STACK_TOP_OFF},
    };
    for (const auto& f : fields) {
      u32 v = 0;
      if (!bounded_read_u32(base, window_size, (u64)t.addr + f.off, &v)) {
        fprintf(stderr, "    %-14s (out of window)\n", f.name);
        continue;
      }
      char line[256];
      format_thread_field(f.name, v, base, window_size, symtab_lo, symtab_hi, s7_offset,
                          symbol_string_base, line, sizeof(line));
      fprintf(stderr, "%s\n", line);
    }
    u32 stack_size = 0;
    if (bounded_read_u32(base, window_size, (u64)t.addr + THREAD_STACK_SIZE_OFF, &stack_size)) {
      fprintf(stderr, "    %-14s %d (decimal)\n", "stack-size", (int)stack_size);
    }
  }
  if (!printed_any) {
    fprintf(stderr, "  (%s has no valid thread to dump)\n", proc_label);
  }
}

// issue #716 round 6: the standing "pp names the previous process, not the one actually
// mid-dispatch" reading trap for this codebase. Walks *active-pool* in the SAME pre-order
// execute-process-tree() itself uses (goal_src/*/kernel/gkernel.gc: visit a node, then
// recurse fully into its child -- and everything under that child -- before touching its
// brother). This is the OPPOSITE work-stack push order from dump_process_pool_threads()
// above (push child then brother, so brother pops first): that function only needs EVERY
// leaf visited once, in any order, so the mismatch never mattered before now, but finding
// "the process the dispatcher visits right after pp" needs the real order. Collects up to
// max_count leaf process addresses into out_processes, in dispatch-visitation order;
// returns how many were collected (never more than max_count, so a very large or
// corrupt-and-cyclic tree cannot make this unbounded -- MAX_POOL_WALK_NODES still caps
// total nodes visited too).
int walk_active_pool_dispatch_order(u32 root,
                                    const u8* base,
                                    u64 window_size,
                                    u32 false_addr,
                                    u32* out_processes,
                                    int max_count) {
  if (!root || root >= window_size || (root & OFFSET_MASK) != BASIC_OFFSET) {
    return 0;
  }
  u32 work[MAX_POOL_WALK_NODES];
  int work_count = 0;
  work[work_count++] = root;
  int visited = 0;
  int collected = 0;
  while (work_count > 0 && visited < MAX_POOL_WALK_NODES && collected < max_count) {
    u32 node = work[--work_count];
    visited++;
    if (!node || node >= window_size || (node & OFFSET_MASK) != BASIC_OFFSET) {
      continue;
    }
    u32 mask = 0;
    if (bounded_read_u32(base, window_size, node + PROCESS_TREE_MASK_OFF, &mask) &&
        !(mask & PROCESS_TREE_MASK_BIT)) {
      out_processes[collected++] = node;
    }
    u32 child = 0;
    u32 brother = 0;
    bool have_child = bounded_read_u32(base, window_size, node + PROCESS_TREE_CHILD_OFF, &child) &&
                      is_present_ptr(child, false_addr) && child < window_size;
    bool have_brother =
        bounded_read_u32(base, window_size, node + PROCESS_TREE_BROTHER_OFF, &brother) &&
        is_present_ptr(brother, false_addr) && brother < window_size;
    // brother pushed first (deeper in the stack) so child pops next: matches
    // execute-process-tree's own "fully recurse into child before touching brother".
    if (have_brother && work_count < MAX_POOL_WALK_NODES) {
      work[work_count++] = brother;
    }
    if (have_child && work_count < MAX_POOL_WALK_NODES) {
      work[work_count++] = child;
    }
  }
  return collected;
}

void dump_process_pool_threads(u32 root,
                               const u8* base,
                               u64 window_size,
                               u64 base_addr,
                               u32 symtab_lo,
                               u32 symtab_hi,
                               u32 false_addr) {
  if (!root || root >= window_size || (root & OFFSET_MASK) != BASIC_OFFSET) {
    return;
  }
  fprintf(stderr, "suspended-thread sweep (process pool from *active-pool* = goal %#x):\n", root);
  u32 work[MAX_POOL_WALK_NODES];
  int work_count = 0;
  work[work_count++] = root;
  int visited = 0;
  bool printed_any = false;
  while (work_count > 0 && visited < MAX_POOL_WALK_NODES) {
    u32 node = work[--work_count];
    visited++;
    if (!node || node >= window_size || (node & OFFSET_MASK) != BASIC_OFFSET) {
      continue;
    }
    u32 mask = 0;
    if (bounded_read_u32(base, window_size, node + PROCESS_TREE_MASK_OFF, &mask) &&
        !(mask & PROCESS_TREE_MASK_BIT)) {
      // a leaf process: dump its threads.
      char pname[48] = "?";
      u32 name_ptr = 0;
      if (bounded_read_u32(base, window_size, node + PROCESS_NAME_OFF, &name_ptr) &&
          is_present_ptr(name_ptr, false_addr) && name_ptr < window_size) {
        bounded_read_str(base, window_size, (u64)name_ptr + 4, pname, sizeof(pname));
      }
      u32 main_thread = 0;
      u32 top_thread = 0;
      bool have_main =
          bounded_read_u32(base, window_size, node + PROCESS_MAIN_THREAD_OFF, &main_thread);
      bool have_top =
          bounded_read_u32(base, window_size, node + PROCESS_TOP_THREAD_OFF, &top_thread);
      struct {
        const char* role;
        u32 addr;
        bool valid;
      } threads[2] = {
          {"main-thread", main_thread, have_main && is_present_ptr(main_thread, false_addr)},
          {"top-thread", top_thread,
           have_top && is_present_ptr(top_thread, false_addr) && top_thread != main_thread},
      };
      for (const auto& t : threads) {
        if (!t.valid || t.addr >= window_size || (t.addr & OFFSET_MASK) != BASIC_OFFSET) {
          continue;
        }
        u32 pc = 0;
        u32 sp = 0;
        bounded_read_u32(base, window_size, (u64)t.addr + THREAD_PC_OFF, &pc);
        bounded_read_u32(base, window_size, (u64)t.addr + THREAD_SP_OFF, &sp);
        u64 ra = 0;
        bool have_ra =
            is_present_ptr(sp, false_addr) && bounded_read_u64(base, window_size, sp, &ra);
        char line[256];
        format_thread_line(pname, t.role, pc, sp, have_ra, ra, base_addr, window_size, symtab_lo,
                           symtab_hi, line, sizeof(line));
        fprintf(stderr, "%s\n", line);
        printed_any = true;
      }
    }

    // every node, leaf or pool, still carries child/brother links (search-process-tree
    // recurses regardless of the mask bit too).
    u32 child = 0;
    u32 brother = 0;
    if (bounded_read_u32(base, window_size, node + PROCESS_TREE_CHILD_OFF, &child) &&
        is_present_ptr(child, false_addr) && child < window_size &&
        work_count < MAX_POOL_WALK_NODES) {
      work[work_count++] = child;
    }
    if (bounded_read_u32(base, window_size, node + PROCESS_TREE_BROTHER_OFF, &brother) &&
        is_present_ptr(brother, false_addr) && brother < window_size &&
        work_count < MAX_POOL_WALK_NODES) {
      work[work_count++] = brother;
    }
  }
  if (!printed_any) {
    fprintf(stderr, "  (no threads found, visited %d node(s))\n", visited);
  }
}

// issue #716 round 4: does `tag`'s own type, or any ancestor of it, equal
// process_addr? Climbs Type::parent (TYPE_PARENT_OFF) up to MAX_TYPE_PARENT_HOPS times.
// Every dereference is the same bounded read the rest of this file already trusts
// against a fabricated or real window; a corrupt/cyclic chain just runs out its hop
// budget and returns false rather than looping the fault handler. A self-parented node
// (the root of the hierarchy, `object`/`basic`, whose own parent field either points to
// itself or is otherwise not making progress) also stops the climb, since one more hop
// would just repeat the same non-match forever.
bool type_is_process_subtype(u32 tag, const u8* base, u64 window_size, u32 process_addr) {
  if (!process_addr) {
    return false;
  }
  u32 cur = tag;
  for (int hop = 0; hop < MAX_TYPE_PARENT_HOPS; hop++) {
    if (!cur || cur >= window_size || (cur & OFFSET_MASK) != BASIC_OFFSET) {
      return false;
    }
    if (cur == process_addr) {
      return true;
    }
    u32 parent = 0;
    if (!bounded_read_u32(base, window_size, (u64)cur + TYPE_PARENT_OFF, &parent)) {
      return false;
    }
    if (parent == cur) {
      return false;
    }
    cur = parent;
  }
  return false;
}

// issue #716 round 4: one compact line naming every one of a cpu-thread's 7 saved
// general-purpose registers (rreg), each classified the same way format_reg() above
// classifies a live register: dual reading (an absolute r15-relative pointer, or a raw
// 32-bit goal offset), then lookup() against the real object map, then the registered
// symbol-table region, then a bare "== 0" flag for a raw zero that resolves to neither
// (a plausible "code pointer that was never set" reading distinct from #f, since #f is
// s7 -- a nonzero address -- not 0; see is_present_ptr()'s doc comment above). This is
// what thread-resume actually restores into real registers on resume, so it is the
// direct completion of round 1's pc/sp-only coverage.
void format_rreg_line(const u64* rreg,
                      u64 base_addr,
                      u64 mem_size,
                      u32 symtab_lo,
                      u32 symtab_hi,
                      char* out,
                      size_t out_size) {
  int n = std::snprintf(out, out_size, "  rreg:");
  if (n < 0 || (size_t)n >= out_size) {
    return;
  }
  auto append = [&](const char* fmt, auto... args) {
    if ((size_t)n < out_size) {
      int m = std::snprintf(out + n, out_size - n, fmt, args...);
      if (m > 0) {
        n += m;
      }
    }
  };
  for (int i = 0; i < CPU_THREAD_RREG_COUNT; i++) {
    u64 value = rreg[i];
    append(" [%d]=%#018llx", i, (unsigned long long)value);
    u32 candidate = 0;
    bool have_candidate = false;
    if (base_addr && value >= base_addr && value < base_addr + mem_size) {
      candidate = (u32)(value - base_addr);
      have_candidate = true;
    } else if (value && value < mem_size) {
      candidate = (u32)value;
      have_candidate = true;
    }
    if (!have_candidate) {
      if (value == 0) {
        append("%s", "(ZERO)");
      }
      continue;
    }
    const ObjRec* o = lookup(candidate);
    if (o) {
      append("(%s+%#x)", o->name, candidate - o->start);
    } else if (candidate >= symtab_lo && candidate < symtab_hi) {
      append("%s", "(SYMBOL TABLE, FLAG)");
    } else {
      append("%s", "(UNMAPPED, FLAG)");
    }
  }
}

// issue #716 round 4: pure GOAL-range attribution for one raw stack quadword (the raw
// stack window below prints every slot, unlike the older stack-scan section above which
// only prints ones that resolve). Returns false (out untouched) if value is not a
// plausible absolute GOAL address; the caller falls back to host-module attribution in
// that case.
bool format_stack_goal_attribution(u64 value,
                                   u64 base_addr,
                                   u64 mem_size,
                                   char* out,
                                   size_t out_size) {
  if (!base_addr || value < base_addr || value >= base_addr + mem_size) {
    return false;
  }
  u32 g = (u32)(value - base_addr);
  const ObjRec* o = lookup(g);
  if (o) {
    std::snprintf(out, out_size, "%s+%#x [%#x,+%#x) (goal %#x)", o->name, g - o->start, o->start,
                  o->extent, g);
  } else {
    std::snprintf(out, out_size, "(goal %#x, unmapped)", g);
  }
  return true;
}

// issue #716 round 6: fixed-size, statically-allocated scratch buffer the heap-scan
// thread writes its findings into. NO CRT stdio from the scan thread (fprintf/printf
// take an internal per-FILE lock on the stream; round 5's dedicated-thread fix made the
// crash survive far longer -- 60+ seconds instead of dying in ~10 -- but the scan's own
// output, and even its timeout message, never appeared, which is the signature of that
// lock being contended with the handler thread rather than free) and NO heap allocation
// (a crash handler's worst possible place to need one). The handler thread is the only
// one that ever calls fprintf on this data, and only after WaitForSingleObject returns
// (or times out), entirely off the scan thread's own critical path. Plain (non-atomic)
// size_t is deliberate: the scan thread is this buffer's sole writer for the life of one
// scan, and the handler thread only reads it after the scan thread has been joined or
// abandoned, so there is no concurrent access to synchronize -- adding an atomic here
// would be the fault-handler equivalent of a lock this fix exists to remove.
constexpr size_t HEAP_SCAN_BUFFER_SIZE = 64 * 1024;
char g_heap_scan_buffer[HEAP_SCAN_BUFFER_SIZE];
size_t g_heap_scan_buffer_used = 0;

// appends text into g_heap_scan_buffer, silently truncating (never past capacity, never
// undefined behavior, matching this file's "must never crash" posture even for its own
// scratch space) rather than growing it.
void heap_scan_buffer_append(const char* text) {
  size_t len = std::strlen(text);
  if (g_heap_scan_buffer_used >= HEAP_SCAN_BUFFER_SIZE - 1) {
    return;
  }
  size_t room = HEAP_SCAN_BUFFER_SIZE - 1 - g_heap_scan_buffer_used;
  size_t n = len < room ? len : room;
  std::memcpy(g_heap_scan_buffer + g_heap_scan_buffer_used, text, n);
  g_heap_scan_buffer_used += n;
  g_heap_scan_buffer[g_heap_scan_buffer_used] = 0;
}

void heap_scan_buffer_append_line(const char* text) {
  heap_scan_buffer_append(text);
  heap_scan_buffer_append("\n");
}

// issue #716 round 4/6: dump every process-typed object found by scanning GOAL memory
// directly for a type tag that resolves to `process` or a subtype
// (type_is_process_subtype() above), rather than by following *active-pool*'s tree links
// the way dump_process_pool_threads() above does. This is what closes round 1's two
// blind spots at once: a process the tree walk can't reach (mid-teardown, still in a
// dead pool, or simply never linked) still has a live, correctly-tagged object sitting
// in memory, and this scan finds it regardless; and it dumps the full rreg array
// (format_rreg_line() above), not just pc/sp. Every 8-byte-aligned position in the
// 128MB window is a candidate type-tag slot (a valid basic pointer's tag sits 4 bytes
// before it, so the candidate object address is always tag_addr + 4, itself 8-aligned
// exactly when the scan position is); this is O(window_size / 8) bounded reads, no
// different in kind from the stack scan above, just over a much larger range. Every
// line of output goes through heap_scan_buffer_append_line() above, not fprintf --
// see that function's doc comment for why.
void heap_scan_processes(const u8* base,
                         u64 window_size,
                         u64 base_addr,
                         u32 process_type_addr,
                         u32 symtab_lo,
                         u32 symtab_hi,
                         u32 false_addr) {
  if (!process_type_addr) {
    return;
  }
  heap_scan_buffer_append_line("heap scan (process objects found by type tag, not tree-linked):");
  int found = 0;
  int reported = 0;
  char line[512];
  for (u64 p = 0; p + 4 <= window_size; p += 8) {
    u32 tag = 0;
    if (!bounded_read_u32(base, window_size, p, &tag)) {
      continue;
    }
    if (!tag || !type_is_process_subtype(tag, base, window_size, process_type_addr)) {
      continue;
    }
    u32 node = (u32)p + 4;
    found++;
    if (reported >= MAX_HEAP_SCAN_REPORTS) {
      continue;
    }
    reported++;

    char pname[48] = "?";
    u32 name_ptr = 0;
    if (bounded_read_u32(base, window_size, node + PROCESS_NAME_OFF, &name_ptr) &&
        is_present_ptr(name_ptr, false_addr) && name_ptr < window_size) {
      bounded_read_str(base, window_size, (u64)name_ptr + 4, pname, sizeof(pname));
    }
    u32 status = 0;
    bool have_status = bounded_read_u32(base, window_size, node + PROCESS_STATUS_OFF, &status);
    char status_slot[96] = {0};
    bool status_named =
        have_status && is_present_ptr(status, false_addr) &&
        format_symbol_slot(status, base, window_size, symtab_lo, symtab_hi, false_addr,
                           g_symbol_string_base, status_slot, sizeof(status_slot));

    std::snprintf(line, sizeof(line), "  process %#010x \"%s\" status %s", node, pname,
                  status_named ? status_slot : (have_status ? "(unresolved)" : "(unreadable)"));
    heap_scan_buffer_append_line(line);

    u32 main_thread = 0;
    u32 top_thread = 0;
    bool have_main =
        bounded_read_u32(base, window_size, node + PROCESS_MAIN_THREAD_OFF, &main_thread);
    bool have_top = bounded_read_u32(base, window_size, node + PROCESS_TOP_THREAD_OFF, &top_thread);
    struct {
      const char* role;
      u32 addr;
      bool valid;
    } threads[2] = {
        {"main-thread", main_thread, have_main && is_present_ptr(main_thread, false_addr)},
        {"top-thread", top_thread,
         have_top && is_present_ptr(top_thread, false_addr) && top_thread != main_thread},
    };
    for (const auto& t : threads) {
      if (!t.valid || t.addr >= window_size || (t.addr & OFFSET_MASK) != BASIC_OFFSET) {
        continue;
      }
      u32 pc = 0;
      u32 sp = 0;
      bounded_read_u32(base, window_size, (u64)t.addr + THREAD_PC_OFF, &pc);
      bounded_read_u32(base, window_size, (u64)t.addr + THREAD_SP_OFF, &sp);
      u64 ra = 0;
      bool have_ra = is_present_ptr(sp, false_addr) && bounded_read_u64(base, window_size, sp, &ra);
      format_thread_line(pname, t.role, pc, sp, have_ra, ra, base_addr, window_size, symtab_lo,
                         symtab_hi, line, sizeof(line));
      heap_scan_buffer_append_line(line);

      u64 rreg[CPU_THREAD_RREG_COUNT] = {0};
      bool have_all_rreg = true;
      for (int i = 0; i < CPU_THREAD_RREG_COUNT; i++) {
        if (!bounded_read_u64(base, window_size, (u64)t.addr + CPU_THREAD_RREG_OFF + (u64)i * 8,
                              &rreg[i])) {
          have_all_rreg = false;
          break;
        }
      }
      if (have_all_rreg) {
        char rline[512];
        format_rreg_line(rreg, base_addr, window_size, symtab_lo, symtab_hi, rline, sizeof(rline));
        std::snprintf(line, sizeof(line), "  %s %s", t.role, rline);
        heap_scan_buffer_append_line(line);
      }
    }
  }
  std::snprintf(line, sizeof(line), "  (%d process object(s) found, %d printed)", found, reported);
  heap_scan_buffer_append_line(line);
}

// issue #716 round 7: DR7 bit layout for arming DR0 as a hardware EXECUTE breakpoint
// (Intel SDM Vol. 3B, 17.2.4 / AMD APM Vol. 2, 13.1.3): bit 0 (L0) and bit 1 (G0) enable
// DR0 locally and globally; bits 16-17 (R/W0) select the trigger condition, 00 =
// execute-only (the coordinator's own care point: a data READ of s7+0 must never fire
// this, and 00 is the only R/W encoding that guarantees that); bits 18-19 (LEN0) must be
// 00 for an execute breakpoint specifically -- the Intel SDM calls out that any other
// LEN with R/W=00 is undefined behavior, not just "wrong". Pure bit math, independent of
// any live thread/CPU state, so it is unit-testable directly; the live half (does the
// CPU actually honor it) is proven only by the reproduction itself. Preserves whatever
// bits already belong to DR1-DR3 in existing_dr7, only ever touching the DR0 fields.
constexpr u64 DR7_L0_BIT = 1ULL << 0;
constexpr u64 DR7_G0_BIT = 1ULL << 1;
constexpr u64 DR7_RW0_MASK = 0x3ULL << 16;
constexpr u64 DR7_LEN0_MASK = 0x3ULL << 18;

u64 compute_dr7_for_dr0_execute(u64 existing_dr7) {
  u64 dr7 = existing_dr7;
  dr7 |= (DR7_L0_BIT | DR7_G0_BIT);
  dr7 &= ~(DR7_RW0_MASK | DR7_LEN0_MASK);  // R/W0 = 00 (execute), LEN0 = 00 (1 byte)
  return dr7;
}

// the DR7 bit math that DISARMS DR0 (clears L0/G0 so it can never fire again), used the
// moment a hit is confirmed (the coordinator's own care point: clear DR0 inside the
// handler before doing any further work) and by the arm path's own cleanup on failure.
u64 compute_dr7_with_dr0_disabled(u64 existing_dr7) {
  return existing_dr7 & ~(DR7_L0_BIT | DR7_G0_BIT);
}

// issue #716 round 7: the dispatch discriminator goal_crash_filter() uses to tell "our
// own armed DR0 fired" apart from any other EXCEPTION_SINGLE_STEP (a real debugger's
// single-step, or a hardware watchpoint someone else set) -- factored out so it is
// testable without a live fault, a real thread, or debug registers at all.
// 0x80000004UL is STATUS_SINGLE_STEP/EXCEPTION_SINGLE_STEP, a fixed NTSTATUS value
// across every Windows SDK version; spelled out numerically here (rather than via the
// <windows.h> macro) so this function has no Windows-header dependency and can live in
// this file's portable section next to the DR7 bit math above. armed_host_addr of 0
// means "not armed" (goal_crash_map_arm_symbol_breakpoint() never ran, or failed), which
// this correctly never matches since a real rip is never exactly 0.
constexpr unsigned long EXCEPTION_SINGLE_STEP_CODE = 0x80000004UL;

bool is_symbol_breakpoint_hit(unsigned long exception_code, u64 rip, u64 armed_host_addr) {
  return exception_code == EXCEPTION_SINGLE_STEP_CODE && armed_host_addr != 0 &&
         rip == armed_host_addr;
}

#ifdef _WIN32

// SEH-guarded reads so a corrupt pointer chain cannot re-fault inside the handler.
// These live in their own functions because __try cannot share a frame with C++
// unwinding.
u32 safe_read_u32(const u8* base, u64 off) {
  __try {
    return *(const u32*)(base + off);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

bool safe_read_str(const u8* base, u64 off, char* out, size_t out_size) {
  __try {
    const char* src = (const char*)(base + off);
    size_t i = 0;
    for (; i + 1 < out_size && src[i]; i++) {
      out[i] = src[i];
    }
    out[i] = 0;
    return i > 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out[0] = 0;
    return false;
  }
}

// resolve addr to its containing module and write "native: <basename>+0xOFFSET" via
// format_native_rip() (issue #122; factored out for issue #716 round 4's raw stack
// window below, which needs this same resolution for up to 32 addresses, not just rip).
// Static, fixed-size buffers only, matching the rest of this handler; no dynamic
// allocation. GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT so a fault handler never
// perturbs the module refcount. No SEH guard here (unlike the raw GOAL-heap reads
// elsewhere in this file): these calls walk loader/PEB bookkeeping, not memory a wild
// GOAL pointer could have corrupted, so they are not expected to re-fault the way a
// corrupt GOAL pointer chain could. Returns false (out untouched) if addr is not inside
// any currently-loaded module.
bool resolve_native_address(u64 addr, char* out, size_t out_size) {
  HMODULE mod = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          (LPCWSTR)(uintptr_t)addr, &mod) ||
      !mod) {
    return false;
  }

  MODULEINFO mod_info;
  wchar_t path[MAX_PATH];
  if (!K32GetModuleInformation(GetCurrentProcess(), mod, &mod_info, sizeof(mod_info)) ||
      !GetModuleFileNameW(mod, path, MAX_PATH)) {
    return false;
  }

  // basename only: walk to the last path separator
  const wchar_t* base_name = path;
  for (const wchar_t* p = path; *p; p++) {
    if (*p == L'\\' || *p == L'/') {
      base_name = p + 1;
    }
  }

  char narrow_name[64];
  size_t i = 0;
  for (; i + 1 < sizeof(narrow_name) && base_name[i]; i++) {
    narrow_name[i] = (char)base_name[i];
  }
  narrow_name[i] = 0;

  format_native_rip(narrow_name, (u64)(uintptr_t)mod_info.lpBaseOfDll, addr, out, out_size);
  return true;
}

void print_native_rip(u64 rip) {
  char line[128];
  if (!resolve_native_address(rip, line, sizeof(line))) {
    fprintf(stderr, "native: unresolved\n");
    return;
  }
  fprintf(stderr, "%s\n", line);
}

// issue #716 round 7: arm DR0 as a hardware execute breakpoint on host_addr, on the
// CALLING thread. Debug registers are per-thread state, so this must run on the actual
// EE/GOAL thread (goal_crash_map_arm_symbol_breakpoint() below documents the call site:
// kscheme.cpp's InitHeapAndSymbol(), which runs ON that thread as part of normal kernel
// boot, not from a helper thread). GetCurrentThread() returns a pseudo-handle that
// Get/SetThreadContext will not reliably accept for debug-register work; DuplicateHandle
// with DUPLICATE_SAME_ACCESS turns it into a real handle referencing this same thread,
// which is the standard, documented way to read/write your OWN thread's debug registers
// without needing to suspend yourself (SetThreadContext on a running thread's own debug
// registers, called BY that thread, is exactly the "self breakpoint" pattern this is).
bool arm_dr0_execute_breakpoint_on_current_thread(u64 host_addr) {
  HANDLE self;
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self, 0,
                       FALSE, DUPLICATE_SAME_ACCESS)) {
    return false;
  }
  CONTEXT ctx = {};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  bool ok = GetThreadContext(self, &ctx) != 0;
  if (ok) {
    ctx.Dr0 = host_addr;
    ctx.Dr6 = 0;
    ctx.Dr7 = compute_dr7_for_dr0_execute(ctx.Dr7);
    ok = SetThreadContext(self, &ctx) != 0;
  }
  CloseHandle(self);
  return ok;
}

// issue #716 round 7: DR0 gets disarmed by directly mutating the live
// EXCEPTION_POINTERS context inside handle_symbol_breakpoint_hit() below (the
// coordinator's own care point: clear DR0 before doing any further work), not by a
// GetThreadContext/SetThreadContext round trip like the arm path above -- the context
// Windows hands the handler on a breakpoint hit IS the one restored on
// EXCEPTION_CONTINUE_EXECUTION, so mutating it in place is both correct and simpler
// than re-fetching a possibly-stale copy.

thread_local bool g_in_handler = false;
// (no saved previous filter: the vectored handler coexists with any SEH chain)

// issue #716 round 5: process-wide (NOT thread_local, unlike g_in_handler above) guard
// for the dedicated heap-scan thread below. VEH is process-wide -- if the scan thread
// itself ever faults, goal_crash_filter() runs again on THAT thread, where g_in_handler
// (thread_local) reads false, so without this it would look like a brand-new,
// unrelated crash and recurse into printing a second full report (and spawning a
// second scan thread). This flag is best-effort synchronization only (a crash-time
// "don't recurse" signal, not a correctness-critical data structure), hence a plain
// atomic bool rather than a lock a fault handler would rather not take.
std::atomic<bool> g_in_heap_scan_thread{false};

// issue #716 round 5: the heap scan runs here, on a dedicated OS thread with its own,
// generous stack, instead of inline in goal_crash_filter(). Round 4's raw stack window
// showed why inline was wrong: 256 bytes of zeros under a lone return into
// thread-suspend at [rsp+0] is a suspended GOAL thread's own shallow stack (thread-suspend
// copies only [sp, stack-top) -- often as little as 256 bytes, per the `(new 'process
// 'cpu-thread arg0 'trans 256 ...)`-style allocations throughout gkernel.gc), and the
// fault handler runs ON that same native stack (Windows delivers exceptions on the
// faulting thread). heap_scan_processes()'s own frame plus everything it calls does not
// reliably fit in what is left of a stack that small -- identical truncation regardless
// of how the round-4 scan was bounded, and the round-4 __try/__except never firing, both
// point at stack exhaustion rather than a data-dependent fault. A fresh thread sidesteps
// this entirely; WaitForSingleObject with a timeout is the belt to CreateThread's own
// stack-size braces, in case the scan itself hangs rather than crashes.
struct HeapScanThreadArgs {
  const u8* base;
  u64 window_size;
  u64 base_addr;
  u32 process_type_addr;
  u32 symtab_lo;
  u32 symtab_hi;
  u32 false_addr;
};

DWORD WINAPI heap_scan_thread_proc(LPVOID param) {
  g_in_heap_scan_thread = true;
  // issue #716 round 6: written before anything else, so the buffer distinguishes
  // "thread never ran" (buffer stays whatever run_heap_scan_on_dedicated_thread() reset
  // it to -- empty) from "thread ran and hung mid-scan" (buffer starts with this line,
  // then has whatever heap_scan_processes() got through before the timeout).
  heap_scan_buffer_append_line("[heap-scan-thread: started]");
  const auto* args = (const HeapScanThreadArgs*)param;
  // issue #716 round 6 postmortem (crashdump11/12): the lock-free buffer fixed the
  // "output never appears" symptom (the "heap scan: attempting" marker, printed and
  // flushed by the HANDLER thread before this thread even starts, now reliably reaches
  // the log), but the process still dies with nothing past that marker -- no buffer
  // content, no timeout message, nothing. That is not a stdio lock (this thread does no
  // stdio at all); it is a real fault on THIS thread taking the WHOLE PROCESS down.
  // goal_crash_filter()'s own VEH sees g_in_heap_scan_thread and correctly declines to
  // recurse into a second report, but VEH declining is not the same as the fault being
  // HANDLED -- Windows' default behavior for a truly unhandled exception is to
  // terminate the whole process, not just this thread, which kills the handler thread
  // sitting in WaitForSingleObject before it can ever print this buffer. A __try here
  // gives frame-based SEH a chance to actually catch it (VEH runs first, declines, then
  // frame-based SEH on this thread's own stack -- this __try -- runs next), turning
  // "the whole process vanishes" into "this thread returns early and the buffer, however
  // far it got, still gets printed."
  __try {
    heap_scan_processes(args->base, args->window_size, args->base_addr, args->process_type_addr,
                        args->symtab_lo, args->symtab_hi, args->false_addr);
    heap_scan_buffer_append_line("[heap-scan-thread: finished]");
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    heap_scan_buffer_append_line("[heap-scan-thread: FAULTED mid-scan, caught here]");
  }
  g_in_heap_scan_thread = false;
  return 0;
}

// 8 MB: comfortably larger than any GOAL process stack (the largest seen in
// goal_src/*/kernel/gkernel-h.gc's DPROCESS_STACK_SIZE/PROCESS_STACK_SIZE constants is a
// few tens of KB) and in line with a normal Win32 thread's default stack, so this is
// "give it a real stack", not a tuned/fragile number.
constexpr SIZE_T HEAP_SCAN_THREAD_STACK_SIZE = 8 * 1024 * 1024;
constexpr DWORD HEAP_SCAN_THREAD_TIMEOUT_MS = 8000;

// issue #716 round 6: the ONLY fprintf(stderr, ...) in the whole heap-scan path now
// happens here, on the handler thread, after the scan thread has been joined or
// abandoned -- never on the scan thread itself. See heap_scan_buffer_append()'s doc
// comment for why (round 5's dedicated-thread fix survived far longer but still never
// printed anything, including its own timeout message, which is the CRT stdio stream
// lock's signature, not stack exhaustion's).
void run_heap_scan_on_dedicated_thread(const u8* base,
                                       u64 window_size,
                                       u64 base_addr,
                                       u32 process_type_addr,
                                       u32 symtab_lo,
                                       u32 symtab_hi,
                                       u32 false_addr) {
  // issue #716 round 6: printed and flushed from the HANDLER thread, before the scan
  // thread even exists, and independent of the buffer below -- so this line (and
  // everything printed above it in the report) is guaranteed to reach the log even if
  // the scan thread's own fault, hang, or unclean process death means its buffer
  // contents never get printed at all. Round 5's version only ever had ONE source of
  // truth (the buffer, printed after the wait); this line means "the scan was at least
  // attempted" is never lost.
  fprintf(stderr, "heap scan: attempting (dedicated thread, %lu ms budget)...\n",
          (unsigned long)HEAP_SCAN_THREAD_TIMEOUT_MS);
  fflush(stderr);

  g_heap_scan_buffer_used = 0;
  g_heap_scan_buffer[0] = 0;
  HeapScanThreadArgs args{base,      window_size, base_addr, process_type_addr,
                          symtab_lo, symtab_hi,   false_addr};
  HANDLE h =
      CreateThread(nullptr, HEAP_SCAN_THREAD_STACK_SIZE, heap_scan_thread_proc, &args, 0, nullptr);
  if (!h) {
    fprintf(stderr, "  (could not create heap scan thread, error %#lx)\n", GetLastError());
    return;
  }
  DWORD wait_result = WaitForSingleObject(h, HEAP_SCAN_THREAD_TIMEOUT_MS);
  if (g_heap_scan_buffer_used > 0) {
    fprintf(stderr, "%s", g_heap_scan_buffer);
  } else {
    fprintf(stderr, "  (heap scan thread wrote nothing at all -- it never ran)\n");
  }
  if (wait_result == WAIT_TIMEOUT) {
    fprintf(stderr,
            "  (heap scan thread did not finish within %lu ms -- abandoning it, rest of report "
            "intact; the buffer above is a TRUNCATED partial result)\n",
            (unsigned long)HEAP_SCAN_THREAD_TIMEOUT_MS);
  } else if (wait_result != WAIT_OBJECT_0) {
    fprintf(stderr, "  (heap scan thread wait failed, error %#lx)\n", GetLastError());
  }
  CloseHandle(h);
}

// issue #716 round 7: the instrument itself. Fires when DR0 (armed on s7's absolute host
// address by goal_crash_map_arm_symbol_breakpoint()) matches the faulting rip exactly --
// caught BEFORE the CPU executes a single byte at that address, unlike every prior
// round's report, which only ever reconstructed the crash after the fact from whatever
// survived the write-fault a few instructions later. Round 6 proved that reconstruction
// has a hard floor: at fault time *active-pool* holds one process and pp is not even in
// it, so no walk of surviving structures can name the caller. This is the only path left
// that can.
//
// First action, before anything else, is disarming DR0 by mutating the live
// EXCEPTION_POINTERS context directly (the coordinator's own care point) -- not calling
// disarm_dr0_on_current_thread(), which would re-fetch a possibly-stale context via
// GetThreadContext; the context Windows handed this handler in `info` IS the one that
// gets restored on EXCEPTION_CONTINUE_EXECUTION, so mutating it directly is both the
// correct and the only way to guarantee DR0 is off before this thread runs another
// instruction.
void handle_symbol_breakpoint_hit(EXCEPTION_POINTERS* info) {
  auto* ctx = info->ContextRecord;
  ctx->Dr7 = compute_dr7_with_dr0_disabled(ctx->Dr7);
  ctx->Dr6 = 0;

  const u8* base = g_ee_main_mem;
  const u64 mem_size = EE_MAIN_MEM_SIZE;
  const u64 base_addr = (u64)(uintptr_t)base;
  const u64 rip = ctx->Rip;
  const u64 rsp = ctx->Rsp;

  fprintf(stderr, "\n-------- GOAL SYMBOL BREAKPOINT HIT --------\n");
  fprintf(stderr,
          "DR0 armed at s7+0 = host %#llx; hit at rip=%#llx (PRISTINE -- no byte at this "
          "address has executed yet; DR0 disarmed just now, before this line printed)\n",
          (unsigned long long)g_symbol_breakpoint_host_addr, (unsigned long long)rip);
  fprintf(stderr, "rsp: %#llx (rsp mod 16 = %llu)\n", (unsigned long long)rsp,
          (unsigned long long)(rsp % 16));

  // [rsp+0]: if the transfer that landed us here was a CALL, this is the return address
  // -- the caller, named directly. If it was a JMP, it is still whatever the innermost
  // live frame is, so it gets attributed either way rather than assumed away.
  u64 top_of_stack = 0;
  bool have_top = false;
  __try {
    top_of_stack = *(const u64*)rsp;
    have_top = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    have_top = false;
  }
  if (!have_top) {
    fprintf(stderr, "[rsp+0]: (unreadable)\n");
  } else {
    char attrib[160] = {0};
    bool resolved = false;
    if (base) {
      resolved =
          format_stack_goal_attribution(top_of_stack, base_addr, mem_size, attrib, sizeof(attrib));
    }
    if (!resolved) {
      resolved = resolve_native_address(top_of_stack, attrib, sizeof(attrib));
    }
    fprintf(stderr, "[rsp+0] (the caller's return address if this was a CALL): %#018llx%s%s\n",
            (unsigned long long)top_of_stack, resolved ? "  " : "", resolved ? attrib : "");
  }

  // every register, pristine -- before any of the garbage-decode side effects the
  // eventual write-fault report captures a few instructions later.
  fprintf(stderr, "registers (pristine, at the moment of arrival):\n");
  {
    struct {
      const char* name;
      u64 value;
    } regs[] = {{"rax", ctx->Rax}, {"rbx", ctx->Rbx}, {"rcx", ctx->Rcx}, {"rdx", ctx->Rdx},
                {"rsi", ctx->Rsi}, {"rdi", ctx->Rdi}, {"rbp", ctx->Rbp}, {"r8", ctx->R8},
                {"r9", ctx->R9},   {"r10", ctx->R10}, {"r11", ctx->R11}, {"r12", ctx->R12},
                {"r13", ctx->R13}, {"r14", ctx->R14}, {"r15", ctx->R15}};
    for (const auto& r : regs) {
      char line[224];
      format_reg(r.name, r.value, base ? base_addr : 0, mem_size, /*fault_addr=*/0, line,
                 sizeof(line));
      size_t n = std::strlen(line);
      if (base && n < sizeof(line)) {
        u32 candidate = 0;
        bool have_candidate = false;
        if (base_addr && r.value >= base_addr && r.value < base_addr + mem_size) {
          candidate = (u32)(r.value - base_addr);
          have_candidate = true;
        } else if (r.value && r.value < mem_size) {
          candidate = (u32)r.value;
          have_candidate = true;
        }
        if (have_candidate) {
          char slot[96];
          if (format_symbol_slot(candidate, base, mem_size, g_symtab_lo, g_symtab_hi, s7.offset,
                                 g_symbol_string_base, slot, sizeof(slot))) {
            std::snprintf(line + n, sizeof(line) - n, "  <- %s", slot);
          }
        }
      }
      fprintf(stderr, "%s\n", line);
    }
  }

  // the raw 32-quadword window, pristine, same shape as the round-4 raw stack window.
  if (base) {
    fprintf(stderr, "raw stack window (32 quadwords at rsp, pristine):\n");
    for (int i = 0; i < 32; i++) {
      u64 d = (u64)i * 8;
      u64 v = 0;
      bool ok = false;
      __try {
        v = *(const u64*)(rsp + d);
        ok = true;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
      }
      if (!ok) {
        fprintf(stderr, "  [rsp+%#llx] (unreadable)\n", (unsigned long long)d);
        break;
      }
      char attrib[160] = {0};
      if (format_stack_goal_attribution(v, base_addr, mem_size, attrib, sizeof(attrib))) {
        fprintf(stderr, "  [rsp+%#llx] %#018llx  %s\n", (unsigned long long)d,
                (unsigned long long)v, attrib);
      } else if (resolve_native_address(v, attrib, sizeof(attrib))) {
        fprintf(stderr, "  [rsp+%#llx] %#018llx  %s\n", (unsigned long long)d,
                (unsigned long long)v, attrib);
      } else {
        fprintf(stderr, "  [rsp+%#llx] %#018llx\n", (unsigned long long)d, (unsigned long long)v);
      }
    }
  }

  fprintf(stderr,
          "-----------------------------------\n"
          "(DR0 disarmed; resuming execution at the same rip -- the standing write-fault a "
          "few instructions later, if the #716 pattern holds, will still produce the usual "
          "full GOAL CRASH REPORT below)\n");
  fflush(stderr);
}

LONG WINAPI goal_crash_filter(EXCEPTION_POINTERS* info) {
  const auto* er = info->ExceptionRecord;
  const auto* ctx = info->ContextRecord;

  // issue #716 round 7: is this EXCEPTION_SINGLE_STEP our own armed DR0, and not some
  // unrelated single-step/hardware-watchpoint event (a real debugger attached
  // separately, for instance)? Checked before the general dispatch below so the
  // existing "pass breakpoints/single-steps straight through" posture is preserved for
  // every OTHER single-step case -- only an exact rip match against the address this
  // file itself armed is ours to handle. is_symbol_breakpoint_hit() is the pure,
  // tested discriminator (EXCEPTION_SINGLE_STEP's numeric value matches
  // is_symbol_breakpoint_hit()'s own EXCEPTION_SINGLE_STEP_CODE constant; both are the
  // fixed NTSTATUS 0x80000004).
  const bool is_our_breakpoint =
      is_symbol_breakpoint_hit(er->ExceptionCode, ctx->Rip, g_symbol_breakpoint_host_addr);

  // only report faults (plus our own armed breakpoint above); pass every other
  // breakpoint/single-step straight through so debugger workflows (including hardware
  // watchpoints) stay clean, and guard against re-entry from the handler's own SEH
  // probes. g_in_heap_scan_thread (issue #716 round 5) covers the one case
  // g_in_handler (thread_local) cannot: VEH is process-wide, so if the dedicated
  // heap-scan thread itself faults, this filter runs again on THAT thread, where
  // g_in_handler reads false; without the second check it would look like an unrelated
  // new crash and recurse into a second full report.
  if (g_in_handler || g_in_heap_scan_thread.load() ||
      (!is_our_breakpoint && er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
       er->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
       er->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO &&
       er->ExceptionCode != EXCEPTION_STACK_OVERFLOW)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  g_in_handler = true;

  if (is_our_breakpoint) {
    handle_symbol_breakpoint_hit(info);
    g_in_handler = false;
    // resume execution: hardware execute breakpoints report rip AT the not-yet-executed
    // instruction (they are trap-like, not fault-like -- nothing has run yet), so this
    // continues exactly where the CPU was about to go, now with DR0 disarmed. The
    // existing write-fault a few instructions later, if the standing #716 pattern
    // holds, then triggers the normal EXCEPTION_ACCESS_VIOLATION path below via a
    // separate invocation of this same filter.
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  const u8* base = g_ee_main_mem;
  const u64 mem_size = EE_MAIN_MEM_SIZE;
  const u64 rip = ctx->Rip;

  fprintf(stderr, "\n-------- GOAL CRASH REPORT --------\n");
  fprintf(stderr, "exception %#lx at rip=%#llx", er->ExceptionCode, (unsigned long long)rip);
  // hoisted to function scope (was block-local): the register block below and the
  // receiver dump further down both need it, and the register block no longer
  // recomputes its own copy.
  u64 fault_addr = 0;
  if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
    // issue #122: Windows reports a #GP-class fault (a misaligned SSE access, e.g. a
    // movaps against an 8-byte-aligned pointer, is the common cause) as an access
    // violation with ExceptionInformation[1] == -1, i.e. no real faulting address at
    // all. The old unconditional "reading 0xffffffffffffffff" label sent a real
    // investigation chasing a null-pointer theory for two rounds before a standalone
    // probe proved this is what Windows prints for misaligned movaps (info0=0,
    // info1=-1), not an actual read of that address. Raw info0/info1 stay printed
    // either way so nothing is hidden.
    if (er->ExceptionInformation[1] == (ULONG_PTR)-1) {
      fprintf(stderr,
              " (no faulting address: #GP-class fault, commonly a misaligned SSE access; "
              "info0=%#llx info1=%#llx)",
              (unsigned long long)er->ExceptionInformation[0],
              (unsigned long long)er->ExceptionInformation[1]);
    } else {
      fault_addr = (u64)er->ExceptionInformation[1];
      // issue #602 step 1: ExceptionInformation[0] is 0/1/8 (read/write/execute-DEP),
      // not a bool; format_access_kind() names all three instead of folding execute
      // into "writing".
      fprintf(stderr, " (%s %#llx)", format_access_kind((u64)er->ExceptionInformation[0]),
              (unsigned long long)fault_addr);
    }
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "rsp: %#llx (rsp mod 16 = %llu)\n", (unsigned long long)ctx->Rsp,
          (unsigned long long)(ctx->Rsp % 16));
  // issue #716 round 2: print s7 itself, not just registers derived from it -- every
  // "symbol slot" line below is only interpretable relative to this value (s7+0 is #f
  // itself, per common/symbols.h's FIX_SYM_FALSE = 0 convention), and the round-1/round-2
  // arithmetic hinges on knowing this build's actual s7, not an assumed one.
  fprintf(stderr, "s7: goal %#x [%#x, %#x)\n", s7.offset, g_symtab_lo, g_symtab_hi);

  // issue #716 round 2: hoisted above the register loop (was declared just before the
  // rip symbolization further down) so the loop can use it too, for the same dual
  // absolute/raw-offset reading format_reg() already does internally -- resolving each
  // register to a candidate symbol-table slot needs that resolved address, and
  // format_symbol_slot() itself is deliberately kept pure (no register-ABI knowledge), so
  // that resolution is redone here rather than threaded through format_reg()'s signature.
  const u64 base_addr = (u64)(uintptr_t)base;
  auto append_symbol_slot = [&](u64 value, char* line_buf, size_t line_buf_size) {
    u32 candidate = 0;
    bool have_candidate = false;
    if (base_addr && value >= base_addr && value < base_addr + mem_size) {
      candidate = (u32)(value - base_addr);
      have_candidate = true;
    } else if (value && value < mem_size) {
      candidate = (u32)value;
      have_candidate = true;
    }
    if (!have_candidate) {
      return;
    }
    size_t n = std::strlen(line_buf);
    if (n >= line_buf_size) {
      return;
    }
    char slot[96];
    if (format_symbol_slot(candidate, base, mem_size, g_symtab_lo, g_symtab_hi, s7.offset,
                           g_symbol_string_base, slot, sizeof(slot))) {
      std::snprintf(line_buf + n, line_buf_size - n, "  <- %s", slot);
    }
  };

  // general-purpose registers, each read both as an absolute pointer and as a raw goal
  // offset, with the one that carries the faulting address marked (issue #376). r15 is
  // the GOAL base and r13 the current process in this ABI, so both symbolize as
  // themselves; the rest is what the faulting instruction was actually working with.
  {
    struct {
      const char* name;
      u64 value;
    } regs[] = {{"rax", ctx->Rax}, {"rbx", ctx->Rbx}, {"rcx", ctx->Rcx}, {"rdx", ctx->Rdx},
                {"rsi", ctx->Rsi}, {"rdi", ctx->Rdi}, {"rbp", ctx->Rbp}, {"r8", ctx->R8},
                {"r9", ctx->R9},   {"r10", ctx->R10}, {"r11", ctx->R11}, {"r12", ctx->R12},
                {"r13", ctx->R13}, {"r14", ctx->R14}, {"r15", ctx->R15}};
    fprintf(stderr, "registers:\n");
    char line[224];
    for (const auto& r : regs) {
      format_reg(r.name, r.value, base ? base_addr : 0, mem_size, fault_addr, line, sizeof(line));
      // issue #716 round 2: name the symbol slot a register resolves to, if any (e.g. the
      // 0x187e01/0x147d21 constant-callee bystanders across every #716 occurrence).
      append_symbol_slot(r.value, line, sizeof(line));
      fprintf(stderr, "%s\n", line);
    }
  }

  // receiver dump (issue #602 step 1): rip == fault_addr means an indirect call/jump
  // landed in unmapped or non-code memory -- the execute-fault tell regardless of what
  // ExceptionInformation[0] said -- which is exactly the shape of the method-dispatch
  // residual this issue tracks (see format_receiver()'s doc comment above for the full
  // dispatch shape). Dump what a0/a1 (rdi/rsi) resolve to as candidate receivers.
  // __try here even though format_receiver() is already bounds-checked internally: this
  // is the one call site working against the real 128MB mapping instead of a fabricated
  // test buffer, and the rest of this handler wraps its own speculative reads the same
  // belt-and-suspenders way.
  if (base && fault_addr && rip == fault_addr) {
    fprintf(stderr, "receiver dump (rip == fault address, indirect dispatch):\n");
    struct {
      const char* name;
      u64 value;
    } cand_regs[] = {{"rdi", ctx->Rdi}, {"rsi", ctx->Rsi}};
    char rline[256];
    for (const auto& r : cand_regs) {
      __try {
        format_receiver(r.name, r.value, base, mem_size, s7.offset, g_symbol_string_base, rip,
                        ctx->R15, rline, sizeof(rline));
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::snprintf(rline, sizeof(rline), "  recv %-3s (receiver dump faulted)", r.name);
      }
      fprintf(stderr, "%s\n", rline);
    }
  }

  // symbolize rip if it is in GOAL memory
  //
  // skew note (issue #117, from the #115 attribution): the recorded start is the
  // heap cursor at record time, but link_and_exec places the object at the next
  // 16-byte-aligned address, not at the raw cursor. So every "+offset" printed below
  // is inflated by align16(cursor) - cursor relative to the object's true base: 7
  // bytes on the #115 main.o, 0 on every other object in that capture (the cursor
  // was already 16-aligned). This is a small, bounded skew (0-15 bytes), not a
  // lookup bug; it is not corrected here, only documented so it is not re-derived.
  // base_addr itself is hoisted above the register loop now (issue #716 round 2).
  if (base && rip >= base_addr && rip < base_addr + mem_size) {
    u32 goal_ip = (u32)(rip - base_addr);
    const ObjRec* o = lookup(goal_ip);
    if (o) {
      fprintf(stderr, "GOAL code: %s+%#x [%#x,+%#x) (goal %#x)\n", o->name, goal_ip - o->start,
              o->start, o->extent, goal_ip);
    } else {
      fprintf(stderr, "GOAL code: unmapped object (goal %#x)\n", goal_ip);
    }
    // issue #716 round 2: the one-line verdict. Round 1's arithmetic showed the crash's
    // real target is a specific symbol-table slot, not a random unmapped address; this
    // names it directly off the same rip this block just symbolized, e.g. "jump target =
    // symbol slot 'teleport' (unbound)".
    char slot[96];
    if (format_symbol_slot(goal_ip, base, mem_size, g_symtab_lo, g_symtab_hi, s7.offset,
                           g_symbol_string_base, slot, sizeof(slot))) {
      fprintf(stderr, "jump target = %s\n", slot);
    }
  } else {
    print_native_rip(rip);
  }

  // the current GOAL process from r13 (jakx raw offsets: name ptr at +0, state at
  // +68 with the state's name symbol-string reachable at its +0, heap-top at +0x70,
  // heap-cur at +0x74; other game versions print raw values only, still useful)
  const u64 pp = ctx->R13;
  if (base && pp && pp < mem_size) {
    char pname[48] = "?";
    u32 name_ptr = safe_read_u32(base, pp + 0);
    if (name_ptr && name_ptr < mem_size) {
      safe_read_str(base, name_ptr + 4, pname, sizeof(pname));
    }
    u32 heap_top = safe_read_u32(base, pp + 0x70);
    u32 heap_cur = safe_read_u32(base, pp + 0x74);
    fprintf(stderr, "pp: #x%llx \"%s\" heap-cur #x%x heap-top #x%x (span %lld, used %lld)\n",
            (unsigned long long)pp, pname, heap_cur, heap_top, (long long)((s64)heap_top - (s64)pp),
            (long long)((s64)heap_cur - (s64)pp));

    // issue #716 round 6: this codebase's documented reading trap for dispatch-time
    // faults -- pp is written once, before the dispatcher calls into a process, and is
    // NOT updated if the fault actually belongs to whatever got dispatched next; round
    // 5's finding that pp ("target")'s own thread state is perfectly healthy is exactly
    // what that trap predicts when pp is the previous, not the faulting, process. So:
    // walk *active-pool* in dispatch order, find pp in that sequence, and dump the same
    // per-thread field block for pp AND for the one or two processes the dispatcher
    // would have visited right after it -- cheap (a handful of tree-node reads, not a
    // scan) since walk_active_pool_dispatch_order() is bounded the same way the round-1
    // sweep already is.
    dump_process_thread_fields((u32)pp, "pp (dispatch-order PREVIOUS -- may not be the fault)",
                               base, mem_size, g_symtab_lo, g_symtab_hi, s7.offset,
                               g_symbol_string_base, s7.offset);
    if (g_process_pool_root) {
      u32 order[MAX_DISPATCH_ORDER_COLLECT];
      int n_order = walk_active_pool_dispatch_order(g_process_pool_root, base, mem_size, s7.offset,
                                                    order, MAX_DISPATCH_ORDER_COLLECT);
      int pp_index = -1;
      for (int i = 0; i < n_order; i++) {
        if (order[i] == (u32)pp) {
          pp_index = i;
          break;
        }
      }
      if (pp_index < 0) {
        fprintf(stderr,
                "dispatch order: pp not found among %d process(es) walked from *active-pool* "
                "(mid-teardown, dead-pool, or unlinked)\n",
                n_order);
      } else {
        for (int k = 1; k <= 2; k++) {
          int idx = pp_index + k;
          if (idx >= n_order) {
            fprintf(stderr, "dispatch order: no NEXT #%d after pp (pp was last of %d walked)\n", k,
                    n_order);
            break;
          }
          char label[64];
          std::snprintf(label, sizeof(label), "dispatch-order NEXT #%d after pp", k);
          dump_process_thread_fields(order[idx], label, base, mem_size, g_symtab_lo, g_symtab_hi,
                                     s7.offset, g_symbol_string_base, s7.offset);
        }
      }
    }
  }

  // GOAL "backtrace": stack quadwords that point into GOAL memory, symbolized
  if (base) {
    fprintf(stderr, "stack scan (GOAL-range quadwords):\n");
    int printed = 0;
    for (u64 d = 0; d < 512 && printed < 12; d += 8) {
      u64 v = 0;
      __try {
        v = *(const u64*)(ctx->Rsp + d);
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        break;
      }
      if (v > base_addr && v < base_addr + mem_size) {
        u32 g = (u32)(v - base_addr);
        const ObjRec* o = lookup(g);
        if (o) {
          fprintf(stderr, "  [rsp+%#llx] %s+%#x [%#x,+%#x) (goal %#x)\n", (unsigned long long)d,
                  o->name, g - o->start, o->start, o->extent, g);
          printed++;
        }
      }
    }
    if (!printed) {
      fprintf(stderr, "  (none)\n");
    }
  }

  // issue #716 round 4: raw stack window. The stack scan above is a filtered view (GOAL
  // addresses only, first match wins the line); this prints every one of the next 32
  // quadwords unconditionally, with BOTH attributions tried -- GOAL-range (b) and, when
  // that misses, host-module (native module + RVA, resolve_native_address() above) --
  // since a return address into a mips2c or kernel C++ trampoline is exactly the kind of
  // frame the GOAL-only filter above was hiding (round 3's own false lead, gkernel+0xf54,
  // came from over-trusting that filtered view). Bounded to a fixed 32-slot read, each
  // one SEH-guarded independently so one bad page stops the window rather than the
  // report.
  if (base) {
    fprintf(stderr, "raw stack window (32 quadwords at rsp):\n");
    for (int i = 0; i < 32; i++) {
      u64 d = (u64)i * 8;
      u64 v = 0;
      bool ok = false;
      __try {
        v = *(const u64*)(ctx->Rsp + d);
        ok = true;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
      }
      if (!ok) {
        fprintf(stderr, "  [rsp+%#llx] (unreadable)\n", (unsigned long long)d);
        break;
      }
      char attrib[160] = {0};
      if (format_stack_goal_attribution(v, base_addr, mem_size, attrib, sizeof(attrib))) {
        fprintf(stderr, "  [rsp+%#llx] %#018llx  %s\n", (unsigned long long)d,
                (unsigned long long)v, attrib);
      } else if (resolve_native_address(v, attrib, sizeof(attrib))) {
        fprintf(stderr, "  [rsp+%#llx] %#018llx  %s\n", (unsigned long long)d,
                (unsigned long long)v, attrib);
      } else {
        fprintf(stderr, "  [rsp+%#llx] %#018llx\n", (unsigned long long)d, (unsigned long long)v);
      }
    }
  }

  // issue #716/#723: suspended-thread sweep. g_process_pool_root is 0 (the walk's own
  // no-op guard) unless the running game registered one (jakx only today, same posture
  // as g_symbol_string_base above).
  if (base && g_process_pool_root) {
    dump_process_pool_threads(g_process_pool_root, base, mem_size, base_addr, g_symtab_lo,
                              g_symtab_hi, s7.offset);
  }

  // issue #716 round 4/5: heap scan. g_process_type_addr is 0 (no-op guard) unless the
  // running game registered one; unlike the sweep above, this does not depend on
  // reachability from *active-pool* at all, so it is not gated on g_process_pool_root.
  // Runs on a dedicated thread (run_heap_scan_on_dedicated_thread() above) rather than
  // inline: round 4 measured this exact scan failing to complete on THIS thread's own
  // stack (a suspended GOAL thread's shallow native stack -- see that function's doc
  // comment), truncating the report before a single result printed, with no SEH
  // exception ever raised to catch. A fresh thread's own stack sidesteps that; the
  // timeout inside run_heap_scan_on_dedicated_thread() is the remaining belt in case the
  // scan hangs instead.
  if (base && g_process_type_addr) {
    run_heap_scan_on_dedicated_thread(base, mem_size, base_addr, g_process_type_addr, g_symtab_lo,
                                      g_symtab_hi, s7.offset);
    fflush(stderr);
  }

  fprintf(stderr, "-----------------------------------\n");
  fflush(stderr);

  g_in_handler = false;
  return EXCEPTION_CONTINUE_SEARCH;
}

#endif  // _WIN32

}  // namespace

void goal_crash_map_record(u32 goal_addr, const char* name, u32 extent) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  ObjRec r;
  r.start = goal_addr;
  r.extent = extent;
  std::snprintf(r.name, sizeof(r.name), "%s", name ? name : "?");
  g_objs.push_back(r);
}

const char* goal_crash_map_lookup_for_test(u32 goal_addr) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  const ObjRec* o = lookup(goal_addr);
  return o ? o->name : nullptr;
}

void goal_crash_map_format_native_rip_for_test(const char* module_basename,
                                               u64 module_base,
                                               u64 rip,
                                               char* out,
                                               size_t out_size) {
  format_native_rip(module_basename, module_base, rip, out, out_size);
}

void goal_crash_map_format_reg_for_test(const char* name,
                                        u64 value,
                                        u64 base_addr,
                                        u64 mem_size,
                                        u64 fault_addr,
                                        char* out,
                                        size_t out_size) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  format_reg(name, value, base_addr, mem_size, fault_addr, out, out_size);
}

const char* goal_crash_map_format_access_kind_for_test(u64 info0) {
  return format_access_kind(info0);
}

void goal_crash_map_format_receiver_for_test(const char* name,
                                             u64 value,
                                             const u8* base,
                                             u64 window_size,
                                             u32 s7_offset,
                                             u32 symbol_string_base,
                                             u64 rip,
                                             u64 r15,
                                             char* out,
                                             size_t out_size) {
  format_receiver(name, value, base, window_size, s7_offset, symbol_string_base, rip, r15, out,
                  out_size);
}

void goal_crash_map_format_thread_line_for_test(const char* proc_name,
                                                const char* role,
                                                u32 pc,
                                                u32 sp,
                                                bool have_ra,
                                                u64 ra,
                                                u64 base_addr,
                                                u64 mem_size,
                                                u32 symtab_lo,
                                                u32 symtab_hi,
                                                char* out,
                                                size_t out_size) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  format_thread_line(proc_name, role, pc, sp, have_ra, ra, base_addr, mem_size, symtab_lo,
                     symtab_hi, out, out_size);
}

bool goal_crash_map_format_symbol_slot_for_test(u32 candidate,
                                                const u8* base,
                                                u64 window_size,
                                                u32 symtab_lo,
                                                u32 symtab_hi,
                                                u32 s7_offset,
                                                u32 symbol_string_base,
                                                char* out,
                                                size_t out_size) {
  return format_symbol_slot(candidate, base, window_size, symtab_lo, symtab_hi, s7_offset,
                            symbol_string_base, out, out_size);
}

bool goal_crash_map_type_is_process_subtype_for_test(u32 tag,
                                                     const u8* base,
                                                     u64 window_size,
                                                     u32 process_addr) {
  return type_is_process_subtype(tag, base, window_size, process_addr);
}

void goal_crash_map_format_rreg_line_for_test(const u64* rreg,
                                              u64 base_addr,
                                              u64 mem_size,
                                              u32 symtab_lo,
                                              u32 symtab_hi,
                                              char* out,
                                              size_t out_size) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  format_rreg_line(rreg, base_addr, mem_size, symtab_lo, symtab_hi, out, out_size);
}

bool goal_crash_map_format_stack_goal_attribution_for_test(u64 value,
                                                           u64 base_addr,
                                                           u64 mem_size,
                                                           char* out,
                                                           size_t out_size) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  return format_stack_goal_attribution(value, base_addr, mem_size, out, out_size);
}

void goal_crash_map_format_thread_field_for_test(const char* field_name,
                                                 u32 value,
                                                 const u8* base,
                                                 u64 window_size,
                                                 u32 symtab_lo,
                                                 u32 symtab_hi,
                                                 u32 s7_offset,
                                                 u32 symbol_string_base,
                                                 char* out,
                                                 size_t out_size) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  format_thread_field(field_name, value, base, window_size, symtab_lo, symtab_hi, s7_offset,
                      symbol_string_base, out, out_size);
}

int goal_crash_map_walk_active_pool_dispatch_order_for_test(u32 root,
                                                            const u8* base,
                                                            u64 window_size,
                                                            u32 false_addr,
                                                            u32* out_processes,
                                                            int max_count) {
  return walk_active_pool_dispatch_order(root, base, window_size, false_addr, out_processes,
                                         max_count);
}

u64 goal_crash_map_compute_dr7_for_dr0_execute_for_test(u64 existing_dr7) {
  return compute_dr7_for_dr0_execute(existing_dr7);
}

u64 goal_crash_map_compute_dr7_with_dr0_disabled_for_test(u64 existing_dr7) {
  return compute_dr7_with_dr0_disabled(existing_dr7);
}

bool goal_crash_map_is_symbol_breakpoint_hit_for_test(unsigned long exception_code,
                                                      unsigned long long rip,
                                                      unsigned long long armed_host_addr) {
  return is_symbol_breakpoint_hit(exception_code, rip, armed_host_addr);
}

void goal_crash_map_set_symbol_string_base(u32 symbol_string_base) {
  g_symbol_string_base = symbol_string_base;
}

void goal_crash_map_set_process_pool_root(u32 process_pool_root) {
  g_process_pool_root = process_pool_root;
}

void goal_crash_map_set_symbol_table_region(u32 lo, u32 hi) {
  g_symtab_lo = lo;
  g_symtab_hi = hi;
}

void goal_crash_map_set_process_type(u32 process_type_addr) {
  g_process_type_addr = process_type_addr;
}

// issue #716 round 8: see header.
bool goal_crash_map_dispatch_target_is_invalid(u32 target_offset, u32 s7_offset) {
  return target_offset == 0 || target_offset == s7_offset;
}

// issue #716 round 8: see header.
void goal_crash_map_report_blocked_dispatch(const char* site,
                                            u32 target_offset,
                                            u32 s7_offset,
                                            const char* requested_name) {
  const char* what = (target_offset == s7_offset && s7_offset != 0) ? "#f (s7)" : "0";
  if (requested_name) {
    fprintf(stderr,
            "goal-crash-map: BLOCKED dispatch at %s: target=%s (raw %#x), requested symbol "
            "\"%s\" -- skipping instead of jumping\n",
            site, what, target_offset, requested_name);
  } else {
    fprintf(stderr,
            "goal-crash-map: BLOCKED dispatch at %s: target=%s (raw %#x) -- skipping "
            "instead of jumping\n",
            site, what, target_offset);
  }
  fflush(stderr);
}

bool goal_crash_map_arm_symbol_breakpoint() {
#ifdef _WIN32
  if (!g_ee_main_mem || !s7.offset) {
    return false;
  }
  const u64 host_addr = (u64)(uintptr_t)g_ee_main_mem + s7.offset;
  if (!arm_dr0_execute_breakpoint_on_current_thread(host_addr)) {
    return false;
  }
  g_symbol_breakpoint_host_addr = host_addr;
  fprintf(stderr, "goal-crash-map: DR0 armed on s7+0 (host %#llx, goal-rel %#x)\n",
          (unsigned long long)host_addr, s7.offset);
  fflush(stderr);
  return true;
#else
  return false;
#endif
}

void goal_crash_map_install() {
#ifdef _WIN32
  // vectored, not SetUnhandledExceptionFilter: VEH fires ahead of all frame-based
  // SEH, so no later filter installation or in-frame handler can eat the report;
  // this handler only logs and always continues the search
  AddVectoredExceptionHandler(0, goal_crash_filter);
  fprintf(stderr, "goal-crash-map: handler installed\n");
  fflush(stderr);
#endif
  // POSIX: not yet implemented; the Linux port has native backtrace habits already
}
