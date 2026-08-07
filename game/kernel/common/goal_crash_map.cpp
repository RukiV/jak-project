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
#endif

namespace {

struct ObjRec {
  u32 start;
  char name[32];
};

// sorted by start; guarded because link and crash can race in principle
std::vector<ObjRec> g_objs;
std::mutex g_objs_mutex;

const ObjRec* lookup(u32 goal_addr) {
  // callable from the crash handler: no locking (a torn read of a vector that only
  // grows is survivable here, and taking a lock inside a fault handler is worse)
  const ObjRec* best = nullptr;
  for (const auto& r : g_objs) {
    if (r.start <= goal_addr && (!best || r.start > best->start)) {
      best = &r;
    }
  }
  return best;
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

// DBG v2/v3: best-effort dump of a goal-space memory window via SEH. A stack
// buffer of 256 bytes (0x100) is enough for both the small (32-byte) and the
// object (0x100-byte) windows dumped below.
void dump_goal_window(const u8* base, u64 mem_size, const char* tag, u32 goal_off, int nbytes) {
  if (!base || goal_off >= mem_size || nbytes < 0 || nbytes > 256) {
    fprintf(stderr, "DBG win[%s]: goal %#x nbytes %d out of range\n", tag, goal_off, nbytes);
    return;
  }
  u8 b[256] = {0};
  __try {
    for (int i = 0; i < nbytes; i++) {
      b[i] = base[goal_off + i];
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // unreadable tail; zeros mark the bad bytes
  }
  fprintf(stderr, "DBG win[%s]: goal %#x n=%d:", tag, goal_off, nbytes);
  for (int i = 0; i < nbytes; i++) {
    fprintf(stderr, " %02x", b[i]);
  }
  fprintf(stderr, "\n");
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
    fprintf(stderr, " (%s %#llx)", er->ExceptionInformation[0] ? "writing" : "reading",
            (unsigned long long)er->ExceptionInformation[1]);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "DEBUG ee_main_mem=%#llx\n", (unsigned long long)(uintptr_t)base);
  {
    HMODULE h = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)rip, &h);
    fprintf(stderr, "DEBUG module=%p modoff=%#llx\n", (void*)h,
            (unsigned long long)(rip - (u64)(uintptr_t)h));
  }

  // DBG v2: full GPR set + rip at the fault. Discriminates "r15 corrupted to a
  // small GOAL constant (Model A)" from "r15 intact / symbol cell overwritten",
  // and shows every register the dispatch could have derived the target from.
  fprintf(stderr,
          "DBG regs: rax=%#llx rbx=%#llx rcx=%#llx rdx=%#llx rsi=%#llx rdi=%#llx "
          "rbp=%#llx rsp=%#llx\n",
          (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rbx,
          (unsigned long long)ctx->Rcx, (unsigned long long)ctx->Rdx,
          (unsigned long long)ctx->Rsi, (unsigned long long)ctx->Rdi,
          (unsigned long long)ctx->Rbp, (unsigned long long)ctx->Rsp);
  fprintf(stderr,
          "DBG regs: r8=%#llx r9=%#llx r10=%#llx r11=%#llx r12=%#llx r13=%#llx "
          "r14=%#llx r15=%#llx rip=%#llx\n",
          (unsigned long long)ctx->R8, (unsigned long long)ctx->R9,
          (unsigned long long)ctx->R10, (unsigned long long)ctx->R11,
          (unsigned long long)ctx->R12, (unsigned long long)ctx->R13,
          (unsigned long long)ctx->R14, (unsigned long long)ctx->R15,
          (unsigned long long)ctx->Rip);
  {
    const u8* ip = (const u8*)rip;
    u8 ibytes[16] = {0};
    __try {
      for (int i = 0; i < 16; i++) {
        ibytes[i] = ip[i];
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      // fault address may be unreadable; zeros mark the unreadable tail
    }
    fprintf(stderr, "DBG inst:");
    for (int i = 0; i < 16; i++) {
      fprintf(stderr, " %02x", ibytes[i]);
    }
    fprintf(stderr, "\n");
  }

  // symbolize rip if it is in GOAL memory
  const u64 base_addr = (u64)(uintptr_t)base;
  u32 goal_ip = 0;
  if (base && rip >= base_addr && rip < base_addr + mem_size) {
    goal_ip = (u32)(rip - base_addr);
    const ObjRec* o = lookup(goal_ip);
    if (o) {
      fprintf(stderr, "GOAL code: %s+%#x (goal %#x)\n", o->name, goal_ip - o->start, goal_ip);
    } else {
      fprintf(stderr, "GOAL code: unmapped object (goal %#x)\n", goal_ip);
    }
  }

  // DBG v2/v3: memory windows at the addresses implicated by this crash signature:
  //  - 0xe8b954   : the f2d symbol value (sp-process-block-2d trampoline region)
  //  - 0x13c4ec5  : the mystery base added to f2d (crash target = f2d + base)
  //  - goal_ip    : the actual crash site (for this crash 0x2250819)
  //  - 0xe8b9d4   : the sp-particle-system-2d object ('sys' from the PROBE lines),
  //                 checking for a stomped func field (sys+0x80 should be f2d)
  //  - 0xe8bb10   : the sp-process-block object ('cpu' from the PROBE lines)
  // Each is dumped as a raw window so we can see code vs. data vs. zeros.
  dump_goal_window(base, mem_size, "f2d-tramp", 0xe8b954, 32);
  dump_goal_window(base, mem_size, "mystery-base", 0x13c4ec5, 32);
  if (goal_ip) {
    dump_goal_window(base, mem_size, "crash-site", goal_ip, 32);
  }
  dump_goal_window(base, mem_size, "sys-object", 0xe8b9d4, 0x100);
  dump_goal_window(base, mem_size, "cpu-object", 0xe8bb10, 0x100);

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
          fprintf(stderr, "  [rsp+%#llx] %s+%#x (goal %#x)\n", (unsigned long long)d, o->name,
                  g - o->start, g);
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

void goal_crash_map_record(u32 goal_addr, const char* name) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  ObjRec r;
  r.start = goal_addr;
  std::snprintf(r.name, sizeof(r.name), "%s", name ? name : "?");
  g_objs.push_back(r);
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
