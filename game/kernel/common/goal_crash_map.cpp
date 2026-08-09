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
//  - latest-wins tie-break: changed from "r.start > best->start" to
//    "r.start >= best->start", so on a shared start address the later-pushed
//    record beats the earlier one instead of losing to it. This is the fix for
//    the #115 stub collision: target-death, gun-util and menu are bring-up stubs
//    that allocate nothing, so each logs the same heap cursor as whatever loads
//    next; drawable loaded right after them at that same cursor, and the old
//    earliest-wins rule let the first stub steal every drawable frame. Chosen over
//    an explicit "skip zero-extent records" filter because it also covers the
//    case where a stub's logged extent is a small nonzero administrative size (its
//    own link header/table, with no code data) rather than exactly 0: either way,
//    whatever was pushed last at a given cursor is what is actually resident
//    there, so it should always win the tie, not just when extent happens to be 0.
const ObjRec* lookup(u32 goal_addr) {
  // callable from the crash handler: no locking (a torn read of a vector that only
  // grows is survivable here, and taking a lock inside a fault handler is worse)
  const ObjRec* best = nullptr;
  for (const auto& r : g_objs) {
    if (r.start <= goal_addr && goal_addr < r.start + r.extent &&
        (!best || r.start >= best->start)) {
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
      fprintf(stderr, "GOAL code: %s+%#x (goal %#x)\n", o->name, goal_ip - o->start, goal_ip);
    } else {
      fprintf(stderr, "GOAL code: unmapped object (goal %#x)\n", goal_ip);
    }
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

void goal_crash_map_record(u32 goal_addr, const char* name, u32 extent) {
  std::lock_guard<std::mutex> lock(g_objs_mutex);
  ObjRec r;
  r.start = goal_addr;
  r.extent = extent;
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
