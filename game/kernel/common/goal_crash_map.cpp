#include "goal_crash_map.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "common/goal_constants.h"

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
      fprintf(stderr, " (%s %#llx)", er->ExceptionInformation[0] ? "writing" : "reading",
              (unsigned long long)er->ExceptionInformation[1]);
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
    u64 fault_addr = 0;
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2 &&
        er->ExceptionInformation[1] != (ULONG_PTR)-1) {
      fault_addr = (u64)er->ExceptionInformation[1];
    }
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
