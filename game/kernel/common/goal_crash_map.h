#pragma once

/*!
 * @file goal_crash_map.h
 * Crash-time symbolication for GOAL code. The runtime records each linked object's
 * start address and extent in GOAL space; an unhandled-fault handler then prints the
 * faulting address as object+offset, the current GOAL process (name, state, heap
 * usage), and a scan of stack quadwords that land in the GOAL code arena (a poor
 * man's GOAL backtrace). Everything prints to stderr with no allocation, then the
 * fault continues to the default handler / debugger.
 *
 * Motivated by the slice-5/6 jakx bring-up sessions, where reconstructing exactly
 * this information by hand through lldb cost hours per crash (issue #34).
 */

#include <cstddef>

#include "common/common_types.h"

// Record one linked object's GOAL-space start and size in bytes. goal_addr should be
// the target heap's cursor at link time (heap->current.offset), not always
// kglobalheap: level-heap links would otherwise record a stale global offset and
// produce misattributed crash maps (issue #58). A record with extent 0 can never
// match a lookup; see the tie-break note on lookup() in goal_crash_map.cpp for how
// bring-up stub objects (which log a shared start with whatever loads next, extent 0
// or not) are handled (issue #117).
void goal_crash_map_record(u32 goal_addr, const char* name, u32 extent);

// install the fault handler; call once after GOAL main memory is mapped
void goal_crash_map_install();

// test seam (issue #117): runs the same bounded, latest-wins lookup the crash handler
// uses internally against the recorded objects, so test_goal_crash_map.cpp can exercise
// it directly without a live fault. Returns the matching record's name, or nullptr if
// goal_addr lands in a gap, past every extent, or only reaches a zero-extent record. No
// behavior change from the crash-handler path; see lookup() in goal_crash_map.cpp.
const char* goal_crash_map_lookup_for_test(u32 goal_addr);

// test seam (issue #122): forwards to the crash handler's pure module-relative-address
// formatter (format_native_rip() in goal_crash_map.cpp, produces "native: <basename>+
// offset"), so test_goal_crash_map.cpp can exercise the formatting without going
// through the Windows-only module resolution (GetModuleHandleExW et al) that feeds it
// in the real handler, and without a live fault. No behavior change from the
// crash-handler path.
void goal_crash_map_format_native_rip_for_test(const char* module_basename,
                                               u64 module_base,
                                               u64 rip,
                                               char* out,
                                               size_t out_size);

// test seam (issue #376): forwards to the crash handler's pure register-line formatter
// (format_reg() in goal_crash_map.cpp): "  <name> 0x<value>" plus a goal-address reading
// symbolized through the recorded objects (absolute r15-relative pointer, or a raw
// 32-bit goal offset) and a marker when the value, in either reading, is the faulting
// address or sits less than 0x1000 below it. base_addr 0 disables the absolute reading
// and fault_addr 0 disables the marker. No behavior change from the crash-handler path.
void goal_crash_map_format_reg_for_test(const char* name,
                                        u64 value,
                                        u64 base_addr,
                                        u64 mem_size,
                                        u64 fault_addr,
                                        char* out,
                                        size_t out_size);
