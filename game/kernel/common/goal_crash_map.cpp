#include "goal_crash_map.h"

#include <algorithm>
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
constexpr u64 PROCESS_MAIN_THREAD_OFF = 0x34;   // process :offset-assert 56
constexpr u64 PROCESS_TOP_THREAD_OFF = 0x38;    // process :offset-assert 60
constexpr u64 THREAD_PC_OFF = 0x14;             // thread pc, 6th field after the type tag
constexpr u64 THREAD_SP_OFF = 0x18;             // thread sp, 7th field after the type tag

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

// resolve rip to its containing module and print "native: <basename>+0xOFFSET" when it
// is not GOAL code (issue #122: previously every native fault, e.g. one landing inside
// gk.exe itself or a system DLL, printed nothing past the raw rip). Static, fixed-size
// buffers only, matching the rest of this handler; no dynamic allocation.
// GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT so a fault handler never perturbs the
// module refcount. No SEH guard here (unlike the raw GOAL-heap reads above): these
// calls walk loader/PEB bookkeeping, not memory a wild GOAL pointer could have
// corrupted, so they are not expected to re-fault the way a corrupt GOAL pointer chain
// could.
void print_native_rip(u64 rip) {
  HMODULE mod = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          (LPCWSTR)(uintptr_t)rip, &mod) ||
      !mod) {
    fprintf(stderr, "native: unresolved\n");
    return;
  }

  MODULEINFO mod_info;
  wchar_t path[MAX_PATH];
  if (!K32GetModuleInformation(GetCurrentProcess(), mod, &mod_info, sizeof(mod_info)) ||
      !GetModuleFileNameW(mod, path, MAX_PATH)) {
    fprintf(stderr, "native: unresolved\n");
    return;
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

  char line[128];
  format_native_rip(narrow_name, (u64)(uintptr_t)mod_info.lpBaseOfDll, rip, line, sizeof(line));
  fprintf(stderr, "%s\n", line);
}

thread_local bool g_in_handler = false;
// (no saved previous filter: the vectored handler coexists with any SEH chain)

LONG WINAPI goal_crash_filter(EXCEPTION_POINTERS* info) {
  const auto* er = info->ExceptionRecord;
  const auto* ctx = info->ContextRecord;

  // only report faults; pass breakpoints/single-steps straight through so
  // debugger workflows (including hardware watchpoints) stay clean, and guard
  // against re-entry from the handler's own SEH probes
  if (g_in_handler || (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                       er->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                       er->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO &&
                       er->ExceptionCode != EXCEPTION_STACK_OVERFLOW)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  g_in_handler = true;

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

  // general-purpose registers, each read both as an absolute pointer and as a raw goal
  // offset, with the one that carries the faulting address marked (issue #376). r15 is
  // the GOAL base and r13 the current process in this ABI, so both symbolize as
  // themselves; the rest is what the faulting instruction was actually working with.
  {
    const u64 base_addr_r = (u64)(uintptr_t)base;
    struct {
      const char* name;
      u64 value;
    } regs[] = {{"rax", ctx->Rax}, {"rbx", ctx->Rbx}, {"rcx", ctx->Rcx}, {"rdx", ctx->Rdx},
                {"rsi", ctx->Rsi}, {"rdi", ctx->Rdi}, {"rbp", ctx->Rbp}, {"r8", ctx->R8},
                {"r9", ctx->R9},   {"r10", ctx->R10}, {"r11", ctx->R11}, {"r12", ctx->R12},
                {"r13", ctx->R13}, {"r14", ctx->R14}, {"r15", ctx->R15}};
    fprintf(stderr, "registers:\n");
    char line[160];
    for (const auto& r : regs) {
      format_reg(r.name, r.value, base ? base_addr_r : 0, mem_size, fault_addr, line, sizeof(line));
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
  const u64 base_addr = (u64)(uintptr_t)base;
  if (base && rip >= base_addr && rip < base_addr + mem_size) {
    u32 goal_ip = (u32)(rip - base_addr);
    const ObjRec* o = lookup(goal_ip);
    if (o) {
      fprintf(stderr, "GOAL code: %s+%#x [%#x,+%#x) (goal %#x)\n", o->name, goal_ip - o->start,
              o->start, o->extent, goal_ip);
    } else {
      fprintf(stderr, "GOAL code: unmapped object (goal %#x)\n", goal_ip);
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

  // issue #716/#723: suspended-thread sweep. g_process_pool_root is 0 (the walk's own
  // no-op guard) unless the running game registered one (jakx only today, same posture
  // as g_symbol_string_base above).
  if (base && g_process_pool_root) {
    dump_process_pool_threads(g_process_pool_root, base, mem_size, base_addr, g_symtab_lo,
                              g_symtab_hi, s7.offset);
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
