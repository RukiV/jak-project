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

// issue #602 step 1: register the running game's symbol-string table base (the value of
// its *symbol-string* fixed symbol, e.g. jakx::SymbolString.offset) so the crash
// handler's receiver dump can resolve a type tag to a type name through the same
// indirection sym_to_string_ptr() uses (symbol_string_base + symbol_offset - s7_offset
// holds a Ptr<String>; see format_receiver() in goal_crash_map.cpp). SymbolString is
// namespaced per game (jak2::/jak3::/jakx:: each have their own, jak1 has none), so this
// is the one jakx-specific touchpoint into this otherwise game-agnostic common file:
// jakx registers its base once at kscheme init (game/kernel/jakx/kscheme.cpp,
// InitHeapAndSymbol()) and every other game simply never calls this, leaving the
// default of 0, which format_receiver() treats as "skip name resolution, print the tag
// only" rather than misreading a table that was never set up for that game.
void goal_crash_map_set_symbol_string_base(u32 symbol_string_base);

// issue #716/#723: register the goal-relative address of the running game's process-tree
// root (e.g. jakx::intern_from_c(-1, 0, "*active-pool*")->value(), read once after the
// kernel DGO's top-level code -- which is what actually assigns *active-pool* a value --
// has run: game/kernel/jakx/kscheme.cpp's InitHeapAndSymbol() is the one call site). The
// crash handler walks this tree at report time (dump_process_pool_threads() in
// goal_crash_map.cpp) the same way the kernel's own dispatcher does
// (goal_src/*/kernel/gkernel.gc's execute-process-tree/search-process-tree: recurse
// child/brother, a node with process-mask bit 8 clear is a leaf `process`) and prints
// every leaf's main-thread/top-thread saved pc/sp -- issue #716's standing hypothesis is a
// suspended thread whose saved context resumes into unpopulated symbol space, so this is
// meant to name the culprit thread directly from the crash block instead of a follow-up
// probe. 0 (the default) means "not registered", which the walk treats as "skip the
// sweep" rather than misreading address 0 as a real tree root. Like
// goal_crash_map_set_symbol_string_base(), this is the one other jakx-specific
// touchpoint into this otherwise game-agnostic file; a game that never registers a root
// just gets no sweep, never a fault.
void goal_crash_map_set_process_pool_root(u32 process_pool_root);

// issue #716/#723: register the [lo, hi) goal-relative bounds of the running game's
// symbol table (jakx::SymbolTable2.offset / jakx::LastSymbol.offset, set at the same
// point InitHeapAndSymbol() sets up s7 -- see that function for why the sweep needs this
// distinguished from "unmapped" rather than folded into it: the issue's own forensic
// finding was that the observed garbage pc resolves to unpopulated space specifically
// *inside* this table, not merely off the GOAL code map). lo == hi (the 0,0 default)
// disables the check, since a real table is never zero-width.
void goal_crash_map_set_symbol_table_region(u32 lo, u32 hi);

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

// test seam (issue #602 step 1): forwards to the crash handler's pure access-kind
// decision (format_access_kind() in goal_crash_map.cpp), which maps a Windows
// EXCEPTION_ACCESS_VIOLATION's ExceptionInformation[0] to "reading" (0), "writing" (1),
// or "executing" (8, the DEP/execute-prevention case): the rip == fault-address dispatch
// fault this lane adds a receiver dump for is always an execute violation, and the
// previous code printed "writing" for it because it only distinguished nonzero (write)
// from zero (read).
const char* goal_crash_map_format_access_kind_for_test(u64 info0);

// test seam (issue #602 step 1): forwards to the crash handler's pure receiver-dump
// formatter (format_receiver() in goal_crash_map.cpp). base/window_size stand in for
// g_ee_main_mem/EE_MAIN_MEM_SIZE: every read format_receiver() performs is checked
// against window_size explicitly (not SEH alone), so a fabricated small buffer here
// proves the same bound the real 128MB mapping gets. s7_offset/symbol_string_base stand
// in for the real s7.offset and a registered goal_crash_map_set_symbol_string_base()
// value; rip/r15 stand in for the faulting context's Rip/R15. No behavior change from
// the crash-handler path.
void goal_crash_map_format_receiver_for_test(const char* name,
                                             u64 value,
                                             const u8* base,
                                             u64 window_size,
                                             u32 s7_offset,
                                             u32 symbol_string_base,
                                             u64 rip,
                                             u64 r15,
                                             char* out,
                                             size_t out_size);

// test seam (issue #716/#723): forwards to the crash handler's pure per-thread line
// formatter (format_thread_line() in goal_crash_map.cpp), taking the values a caller
// would already have bounded-read out of a thread object's `pc`/`sp` fields (raw
// goal-relative offsets, like every other struct field this file reads -- pc is never
// given the register-style dual absolute/raw reading format_reg() and format_receiver()
// use, since it is read from memory, not a live register) plus the raw quadword sitting
// at [sp] (have_ra false if that read was out of window). pc, and a [sp] quadword that
// does resolve to a plausible absolute GOAL address, are both classified against the real
// object map (lookup(), so this seam takes g_objs_mutex the way
// goal_crash_map_format_reg_for_test() does) and the registered symbol-table region.
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
                                                size_t out_size);
