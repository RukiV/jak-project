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

#include "common/common_types.h"

// record one linked object's GOAL-space start and size in bytes (call at link time,
// before exec). A record with extent 0 can never match a lookup; see the tie-break
// note on lookup() in goal_crash_map.cpp for how bring-up stub objects (which log a
// shared start with whatever loads next, extent 0 or not) are handled (issue #117).
void goal_crash_map_record(u32 goal_addr, const char* name, u32 extent);

// install the fault handler; call once after GOAL main memory is mapped
void goal_crash_map_install();
