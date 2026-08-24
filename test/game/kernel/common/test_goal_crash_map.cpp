// Unit tests for the goal crash map's bounded, latest-wins lookup (issue #117, the
// #132 follow-up). goal_crash_map_lookup_for_test() is a thin test seam declared in
// goal_crash_map.h: lookup() itself is file-static in goal_crash_map.cpp, so the seam
// just forwards to it under the same mutex goal_crash_map_record() uses.
//
// Each TEST below claims its own disjoint slice of GOAL-space addresses so the
// process-global record set (which only grows; there is no reset seam) can't let one
// test's records leak into another's lookups.

#include <cstring>
#include <string>
#include <vector>

#include "game/kernel/common/goal_crash_map.h"
#include "gtest/gtest.h"

namespace {
// little-endian helpers for building a fake GOAL-memory window (issue #602 step 1's
// receiver-dump tests below): the real window is g_ee_main_mem, a raw byte buffer
// addressed the same way regardless of host endianness assumptions, so tests build one
// explicitly rather than relying on struct layout/aliasing.
void write_u32(std::vector<u8>& buf, u64 off, u32 v) {
  buf[off + 0] = (u8)(v >> 0);
  buf[off + 1] = (u8)(v >> 8);
  buf[off + 2] = (u8)(v >> 16);
  buf[off + 3] = (u8)(v >> 24);
}

void write_cstr(std::vector<u8>& buf, u64 off, const char* s) {
  size_t len = std::strlen(s);
  std::memcpy(buf.data() + off, s, len + 1);  // include the terminator
}
}  // namespace

TEST(GoalCrashMap, AttributesInsideExtent) {
  const u32 base = 0x00100000;
  goal_crash_map_record(base, "obj-a", 0x100);
  goal_crash_map_record(base + 0x200, "obj-b", 0x100);

  EXPECT_STREQ(goal_crash_map_lookup_for_test(base), "obj-a");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x50), "obj-a");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0xff), "obj-a");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x200), "obj-b");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x250), "obj-b");
}

// pre-#132, the lookup only checked "start <= goal_addr" and took the earliest match,
// so an address sitting in the unclaimed space between two objects was misattributed to
// whichever object happened to start earliest. #132 added the "< start + extent" bound,
// so a gap address now matches nothing.
TEST(GoalCrashMap, GapBetweenObjectsMatchesNothing) {
  const u32 base = 0x00200000;
  goal_crash_map_record(base, "obj-a", 0x100);          // covers [base, base+0x100)
  goal_crash_map_record(base + 0x300, "obj-b", 0x100);  // covers [base+0x300, base+0x400)

  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x100), nullptr);  // right at obj-a's edge
  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x180), nullptr);  // mid-gap
  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x2ff), nullptr);  // right before obj-b
}

// the #115 capture: a heap data pointer sitting past the end of the last object's real
// extent must not fall back to matching that object.
TEST(GoalCrashMap, PastLastObjectExtentMatchesNothing) {
  const u32 base = 0x00300000;
  goal_crash_map_record(base, "obj-a", 0x100);
  goal_crash_map_record(base + 0x100, "obj-last", 0x50);  // covers [base+0x100, base+0x150)

  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x150), nullptr);   // right at the edge
  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x2812), nullptr);  // #115-sized overshoot
}

// the #115 stub collision: two records sharing a start (not just a zero-extent stub,
// but any tie) must resolve to whichever was pushed last, since that is what is
// actually resident at that heap cursor.
TEST(GoalCrashMap, SharedStartLatestWins) {
  const u32 base = 0x00400000;
  goal_crash_map_record(base, "first", 0x40);
  goal_crash_map_record(base, "second", 0x40);

  EXPECT_STREQ(goal_crash_map_lookup_for_test(base), "second");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x10), "second");
}

// a zero-extent record (a bring-up stub that allocated nothing) can never match a
// lookup, whether queried alone or with nothing else recorded around it: goal_addr <
// start + 0 never holds.
TEST(GoalCrashMap, ZeroExtentRecordNeverMatches) {
  const u32 base = 0x00500000;
  goal_crash_map_record(base, "stub", 0);

  EXPECT_EQ(goal_crash_map_lookup_for_test(base), nullptr);
  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 1), nullptr);
}

// bounds-based attribution (no shared start) must not depend on the order records were
// pushed in. The same three-object layout is recorded forwards in one address window
// and backwards in a disjoint one; both must resolve identically.
TEST(GoalCrashMap, RecordOrderIndependenceForBounds) {
  const u32 fwd_base = 0x00600000;
  goal_crash_map_record(fwd_base, "obj-a", 0x100);
  goal_crash_map_record(fwd_base + 0x200, "obj-b", 0x100);
  goal_crash_map_record(fwd_base + 0x400, "obj-c", 0x100);

  const u32 rev_base = 0x00700000;
  goal_crash_map_record(rev_base + 0x400, "obj-c", 0x100);
  goal_crash_map_record(rev_base + 0x200, "obj-b", 0x100);
  goal_crash_map_record(rev_base, "obj-a", 0x100);

  for (const u32 base : {fwd_base, rev_base}) {
    EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x50), "obj-a");
    EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x250), "obj-b");
    EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x450), "obj-c");
    EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x180), nullptr);  // gap a/b
    EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x500), nullptr);  // past obj-c
  }
}

// issue #594/#595 generalization: a reused level heap can place a new, larger
// object's start at or below several old objects' starts while still
// overlapping them (no removal step exists yet for a freed level heap). The
// pre-#594 tie-break ("highest start address wins") let a stale record with a
// higher start beat a fresher record that starts lower but still covers the
// query address. Recency (push order), not start address, must decide.
TEST(GoalCrashMap, RecencyBeatsHigherStaleStartOnReuse) {
  const u32 base = 0x00800000;
  goal_crash_map_record(base, "old-obj1", 0x100);          // stale: [base, base+0x100)
  goal_crash_map_record(base + 0x100, "old-obj2", 0x100);  // stale: [base+0x100, base+0x200)
  goal_crash_map_record(base, "new-obj", 0x300);           // fresh, reused heap: [base, base+0x300)

  // base+0x150 falls inside both the stale old-obj2 and the fresh new-obj; the
  // fresh record must win even though old-obj2's start address is higher.
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x150), "new-obj");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x50), "new-obj");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(base + 0x250), "new-obj");
}

// issue #595: the debug segment gets a second, independently-addressed record
// per object, distinguished only by the "(debug)" name suffix jakx_finish
// appends (klink.cpp) when code_infos[DEBUG_SEGMENT] is non-empty. The map
// itself has no special-casing for this: it is just two ordinary,
// non-overlapping records (main segment on the level/global heap, debug
// segment on kdebugheap, a different address range entirely) that must
// resolve independently.
TEST(GoalCrashMap, DebugSegmentRecordIsIndependentOfMainSegment) {
  const u32 main_base = 0x00900000;
  const u32 debug_base = 0x00a00000;  // stands in for a kdebugheap address
  goal_crash_map_record(main_base, "menu", 0x200);
  goal_crash_map_record(debug_base, "menu(debug)", 0x1000);

  EXPECT_STREQ(goal_crash_map_lookup_for_test(main_base + 0x10), "menu");
  EXPECT_STREQ(goal_crash_map_lookup_for_test(debug_base + 0x10), "menu(debug)");
  // an address between the two heaps' regions matches neither.
  EXPECT_EQ(goal_crash_map_lookup_for_test(main_base + 0x200), nullptr);
}

// issue #594: an object whose main segment allocates nothing (kdgo.cpp no
// longer records anything for it pre-link, and jakx_finish only records
// main/debug segments when code_infos[...].size is nonzero) must produce no
// record at all, not a phantom whole-file-sized one. This is the map-level
// half of that contract: recording nothing for such an object means a lookup
// anywhere near where it would have loaded matches nothing.
TEST(GoalCrashMap, ZeroSizeMainSegmentProducesNoRecordToLookUp) {
  const u32 base = 0x00b00000;
  // deliberately not recording anything for "empty-main-segment" object here,
  // mirroring jakx_finish's `if (main_seg.size) { record(...) }` guard.
  EXPECT_EQ(goal_crash_map_lookup_for_test(base), nullptr);
  EXPECT_EQ(goal_crash_map_lookup_for_test(base + 0x1000), nullptr);
}

// issue #122: format_native_rip() is the pure half of the "rip is not GOAL code"
// reporting path (goal_crash_map.cpp), split out from the Windows-only module
// resolution (GetModuleHandleExW et al) specifically so this arithmetic-and-snprintf
// part stays unit-testable without a live fault or a live module.
TEST(GoalCrashMap, FormatNativeRipComputesOffsetFromModuleBase) {
  char buf[128] = {};
  goal_crash_map_format_native_rip_for_test("gk.exe", 0x140000000ULL, 0x140001234ULL, buf,
                                            sizeof(buf));
  EXPECT_STREQ(buf, "native: gk.exe+0x1234");
}

// note: %#llx's alternate form omits the "0x" prefix specifically at value 0 (C99
// 7.19.6.1p6), so a rip landing exactly on the module base prints "+0", not "+0x0".
// Same convention already in use for the GOAL-code "+offset" fields elsewhere in this
// file (e.g. "GOAL code: %s+%#x"), so this is not a special case to work around.
TEST(GoalCrashMap, FormatNativeRipZeroOffsetAtModuleBase) {
  char buf[128] = {};
  goal_crash_map_format_native_rip_for_test("ntdll.dll", 0x7ffc00000000ULL, 0x7ffc00000000ULL, buf,
                                            sizeof(buf));
  EXPECT_STREQ(buf, "native: ntdll.dll+0");
}

// issue #376: the register lines. A raw 32-bit goal offset (what a 32-bit load leaves
// in a register) symbolizes through the same object map, and the register that carries
// the faulting address, in either reading, is marked with the distance to the fault.
TEST(GoalCrashMap, FormatRegRawOffsetSymbolizesAndMarksFault) {
  const u32 obj = 0x00700000;
  goal_crash_map_record(obj, "obj-r", 0x100);
  const u64 base_addr = 0x1000000000ull;
  const u64 mem_size = 0x8000000ull;
  char line[160];
  goal_crash_map_format_reg_for_test("r9", obj + 0x10, base_addr, mem_size,
                                     base_addr + obj + 0x10 + 0x14, line, sizeof(line));
  EXPECT_NE(std::string(line).find("r9  0x0000000000700010"), std::string::npos) << line;
  EXPECT_NE(std::string(line).find("(goal-rel 0x700010 obj-r+0x10)"), std::string::npos) << line;
  EXPECT_NE(std::string(line).find("<- fault address is this + 0x14"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatRegAbsolutePointerIsFaultAddress) {
  const u32 obj = 0x00710000;
  goal_crash_map_record(obj, "obj-s", 0x100);
  const u64 base_addr = 0x1000000000ull;
  const u64 mem_size = 0x8000000ull;
  char line[160];
  goal_crash_map_format_reg_for_test("rax", base_addr + obj + 0x20, base_addr, mem_size,
                                     base_addr + obj + 0x20, line, sizeof(line));
  EXPECT_NE(std::string(line).find("(goal 0x710020 obj-s+0x20)"), std::string::npos) << line;
  EXPECT_NE(std::string(line).find("<- fault address"), std::string::npos) << line;
  EXPECT_EQ(std::string(line).find("is this +"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatRegOutsideGoalMemoryHasNoAnnotation) {
  const u64 base_addr = 0x1000000000ull;
  const u64 mem_size = 0x8000000ull;
  char line[160];
  goal_crash_map_format_reg_for_test("rcx", 0x7ff6deadbeefull, base_addr, mem_size, 0, line,
                                     sizeof(line));
  EXPECT_STREQ(line, "  rcx 0x00007ff6deadbeef");
}

// issue #602 step 1: ExceptionInformation[0] is 0 (read), 1 (write), or 8 (DEP/execute).
// The previous code folded 8 into "writing" because it only checked truthiness.
TEST(GoalCrashMap, FormatAccessKindMapsReadWriteExecute) {
  EXPECT_STREQ(goal_crash_map_format_access_kind_for_test(0), "reading");
  EXPECT_STREQ(goal_crash_map_format_access_kind_for_test(1), "writing");
  EXPECT_STREQ(goal_crash_map_format_access_kind_for_test(8), "executing");
}

// undocumented values fall back to the old default rather than asserting or printing
// garbage; nothing in the codebase should ever pass one, but the crash handler is not
// the place to assume that.
TEST(GoalCrashMap, FormatAccessKindFallsBackToReadingForUnknownValues) {
  EXPECT_STREQ(goal_crash_map_format_access_kind_for_test(2), "reading");
  EXPECT_STREQ(goal_crash_map_format_access_kind_for_test(0xffffffffull), "reading");
}

// issue #602 step 1: the receiver dump. A fake GOAL-memory window is built by hand
// (write_u32/write_cstr above), reproducing the same indirection sym_to_string_ptr()
// uses (game/kernel/jakx/kscheme.h): symbol_string_base + type->symbol - s7_offset holds
// a Ptr<String> whose chars start 4 bytes past its own value. The receiver register
// value is passed as a raw goal-relative offset (r15 is set far outside the window, so
// the absolute-pointer reading in format_receiver() cannot match and it falls through to
// the raw-offset reading, exactly like a `mov r9d, [...]` 32-bit load would leave in a
// register).
TEST(GoalCrashMap, FormatReceiverResolvesValidTypeAndConfirmsMethodSlot) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 receiver = 0x1004;  // candidate & 7 == 4: plausible basic pointer
  const u32 tag = 0x1804;       // same alignment: plausible type pointer
  const u32 symbol_offset = 0x50;
  const u32 s7_offset = 0x10;
  const u32 symbol_string_base = 0x900;
  // sym_to_string_ptr()'s formula: symbol_string_base + symbol_offset - s7_offset
  const u32 name_ptr_addr = symbol_string_base + symbol_offset - s7_offset;  // 0x940
  const u32 str_ptr = 0x1200;
  const u32 slot_val = 0x3000;  // method-12 function's raw goal offset
  const u64 r15 = 0x5000000000ull;
  const u64 rip = r15 + slot_val;  // call landed exactly at [tag+0x40] + r15

  write_u32(mem, receiver - 4, tag);       // type tag at [receiver - 4]
  write_u32(mem, tag + 0, symbol_offset);  // Type::symbol at [tag + 0]
  write_u32(mem, name_ptr_addr, str_ptr);  // the symbol-string-table slot
  write_cstr(mem, str_ptr + 4, "process-tree");
  write_u32(mem, tag + 0x40, slot_val);  // Type::get_method(12) at [tag + 0x40]

  char out[256];
  goal_crash_map_format_receiver_for_test("rdi", receiver, mem.data(), window_size, s7_offset,
                                          symbol_string_base, rip, r15, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("recv rdi"), std::string::npos) << line;
  EXPECT_NE(line.find("(goal 0x1004)"), std::string::npos) << line;
  EXPECT_NE(line.find("tag 0x1804"), std::string::npos) << line;
  EXPECT_NE(line.find("type-name \"process-tree\""), std::string::npos) << line;
  EXPECT_NE(line.find("method slot +0x40 (index 12): 0x3000 vs rip-r15 0x3000"), std::string::npos)
      << line;
  EXPECT_NE(line.find("MATCH"), std::string::npos) << line;
}

// a register value that is not itself a plausible basic pointer (misaligned here: 0x1000
// & 7 == 0, not BASIC_OFFSET's 4) must produce a graceful label and read nothing past
// [receiver - 4], since it never gets that far.
TEST(GoalCrashMap, FormatReceiverRejectsImplausibleRegisterValue) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  char out[256];
  goal_crash_map_format_receiver_for_test("rsi", 0x1000, mem.data(), window_size, 0x10, 0x900, 0,
                                          0x5000000000ull, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("(not a plausible basic pointer)"), std::string::npos) << line;
  EXPECT_EQ(line.find("tag"), std::string::npos) << line;
}

// the receiver value is plausible but the type tag it points at is not (misaligned):
// must stop after printing the tag, never attempt name resolution.
TEST(GoalCrashMap, FormatReceiverRejectsImplausibleTypeTag) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 receiver = 0x1004;
  write_u32(mem, receiver - 4, 0x1805);  // 0x1805 & 7 == 5: not a plausible type pointer

  char out[256];
  goal_crash_map_format_receiver_for_test("rdi", receiver, mem.data(), window_size, 0x10, 0x900, 0,
                                          0x5000000000ull, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("tag 0x1805"), std::string::npos) << line;
  EXPECT_NE(line.find("(not a plausible type pointer)"), std::string::npos) << line;
  EXPECT_EQ(line.find("type-name"), std::string::npos) << line;
}

// the symbol-string-table indirection resolves to an address at or past window_size:
// bounded_read_u32's explicit size check (off + 4 > window_size) must reject it rather
// than reading past the fabricated window, and the receiver dump degrades to "type name
// unresolved" instead of a wrong or out-of-bounds read.
TEST(GoalCrashMap, FormatReceiverNameLookupPastWindowIsGraceful) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 receiver = 0x1004;
  const u32 tag = 0x1804;
  const u32 symbol_offset = 0x50;
  const u32 s7_offset = 0x10;
  // symbol_string_base chosen so name_ptr_addr = 0x1ff0 + 0x50 - 0x10 = 0x2030, which is
  // past window_size (0x2000): the very last valid 4-byte read starts at 0x1ffc.
  const u32 symbol_string_base = 0x1ff0;

  write_u32(mem, receiver - 4, tag);
  write_u32(mem, tag + 0, symbol_offset);
  // deliberately nothing written at 0x2030: it is outside mem's 0x2000 bytes and would
  // be a heap-buffer overflow to even address, let alone write

  char out[256];
  goal_crash_map_format_receiver_for_test("rdi", receiver, mem.data(), window_size, s7_offset,
                                          symbol_string_base, 0, 0x5000000000ull, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("tag 0x1804"), std::string::npos) << line;
  EXPECT_NE(line.find("(type name unresolved)"), std::string::npos) << line;
  EXPECT_EQ(line.find("type-name"), std::string::npos) << line;
}

// symbol_string_base of 0 means "no game has registered a table" (every game but jakx,
// or jakx before InitHeapAndSymbol() runs): must skip name resolution entirely rather
// than treating 0 as a real table base and reading near the start of GOAL memory.
TEST(GoalCrashMap, FormatReceiverSkipsNameResolutionWhenNoTableRegistered) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 receiver = 0x1004;
  const u32 tag = 0x1804;
  write_u32(mem, receiver - 4, tag);
  write_u32(mem, tag + 0, 0x50);  // a symbol offset that would otherwise resolve

  char out[256];
  goal_crash_map_format_receiver_for_test("rdi", receiver, mem.data(), window_size, 0x10,
                                          /*symbol_string_base=*/0, 0, 0x5000000000ull, out,
                                          sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("(type name unresolved)"), std::string::npos) << line;
  EXPECT_EQ(line.find("type-name"), std::string::npos) << line;
}

// issue #716/#723: the suspended-thread sweep's per-thread line. A pc that resolves
// through the real object map prints the same "name+offset [start,+extent)" shape the
// stack scan and rip line already use.
TEST(GoalCrashMap, FormatThreadLineResolvesPcAgainstObjectMap) {
  const u32 base = 0x00c00000;
  goal_crash_map_record(base, "gkernel", 0x5900);

  char out[256];
  goal_crash_map_format_thread_line_for_test("target", "main-thread", base + 0x54, 0x1000, false, 0,
                                             /*base_addr=*/0, /*mem_size=*/0, /*symtab_lo=*/0,
                                             /*symtab_hi=*/0, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("thread target main-thread:"), std::string::npos) << line;
  EXPECT_NE(line.find("pc (goal 0xc00054) gkernel+0x54 [0xc00000,+0x5900)"), std::string::npos)
      << line;
  EXPECT_NE(line.find("sp (goal 0x1000)"), std::string::npos) << line;
  EXPECT_NE(line.find("[sp] (out of window)"), std::string::npos) << line;
  EXPECT_EQ(line.find("FLAG"), std::string::npos) << line;
}

// a pc that lands inside the registered symbol-table region (and matches no recorded
// object -- the two are not mutually exclusive in general, but the symbol table is never
// itself a recorded GOAL object) is flagged distinctly from a merely-unmapped address:
// issue #716's own forensic finding was exactly this distinction.
TEST(GoalCrashMap, FormatThreadLineFlagsPcInSymbolTableRegion) {
  char out[256];
  goal_crash_map_format_thread_line_for_test("garage-turntable-3", "main-thread", 0x187e01, 0,
                                             false, 0, 0, 0, /*symtab_lo=*/0x180000,
                                             /*symtab_hi=*/0x190000, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("pc (goal 0x187e01) <- IN SYMBOL TABLE REGION (FLAG)"), std::string::npos)
      << line;
}

// a pc that resolves to no recorded object and falls outside the registered symbol-table
// region (or no region was ever registered, the 0/0 default) is flagged as generically
// unmapped -- the #716 log's own "goal 0" fault, which sits below the symbol table's real
// start (SymbolTable2 is never address 0) and so must not be misreported as "in the
// symbol table" just because it is still unpopulated space.
TEST(GoalCrashMap, FormatThreadLineFlagsUnmappedPcOutsideSymbolTable) {
  char out[256];
  goal_crash_map_format_thread_line_for_test("target", "main-thread", 0, 0, false, 0, 0, 0,
                                             /*symtab_lo=*/0x180000, /*symtab_hi=*/0x190000, out,
                                             sizeof(out));
  std::string line(out);
  // %#x's alternate form omits the "0x" prefix at value 0 (same C99 7.19.6.1p6 convention
  // the FormatNativeRip zero-offset test above already covers), so this is "goal 0)", not
  // "goal 0x0)".
  EXPECT_NE(line.find("pc (goal 0) <- UNMAPPED (FLAG)"), std::string::npos) << line;
}

// the [sp] quadword is only classified when it resolves to a plausible absolute GOAL
// address (base_addr <= ra < base_addr+mem_size); most stack slots are not pointers at
// all, and the stack scan above this function in the real handler already establishes
// that convention (it silently skips anything outside that range rather than flagging
// it) -- this test is the boundary that lets it flag correctly when it IS a pointer.
TEST(GoalCrashMap, FormatThreadLineClassifiesPlausibleReturnAddressSlot) {
  const u32 obj = 0x00d00000;
  goal_crash_map_record(obj, "main", 0x100);
  const u64 base_addr = 0x2000000000ull;
  const u64 mem_size = 0x8000000ull;

  char out[256];
  goal_crash_map_format_thread_line_for_test("target", "top-thread", /*pc=*/0xdeadbe, /*sp=*/0x2000,
                                             /*have_ra=*/true,
                                             /*ra=*/base_addr + obj + 0x10, base_addr, mem_size,
                                             /*symtab_lo=*/0, /*symtab_hi=*/0, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("[sp] 0x0000002000d00010 (goal 0xd00010) main+0x10 [0xd00000,+0x100)"),
            std::string::npos)
      << line;
}

// a [sp] quadword that does not look like an absolute GOAL address at all (ordinary
// stack noise: a small integer, a native pointer outside the mapping, ...) gets no
// classification and no flag, matching the stack scan's own selectivity.
TEST(GoalCrashMap, FormatThreadLineDoesNotClassifyImplausibleReturnAddressSlot) {
  // pc registered as a real object too, so this test isolates the [sp]/ra behavior: with
  // pc resolved cleanly, any stray "(goal" or "FLAG" in the line can only have come from
  // the (implausible) ra reading, not pc's own classification.
  const u32 obj = 0x00e00000;
  goal_crash_map_record(obj, "main", 0x100);

  char out[256];
  goal_crash_map_format_thread_line_for_test("target", "main-thread", /*pc=*/obj + 0x10,
                                             /*sp=*/0x2000, /*have_ra=*/true, /*ra=*/12,
                                             /*base_addr=*/0x2000000000ull,
                                             /*mem_size=*/0x8000000ull, /*symtab_lo=*/0,
                                             /*symtab_hi=*/0, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("pc (goal 0xe00010) main+0x10 [0xe00000,+0x100)"), std::string::npos) << line;
  EXPECT_NE(line.find("[sp] 0x000000000000000c"), std::string::npos) << line;
  // nothing follows the raw [sp] hex value: no "(goal ...)" classification and no FLAG,
  // since 12 is nowhere near the [base_addr, base_addr+mem_size) window.
  EXPECT_TRUE(line.ends_with("[sp] 0x000000000000000c")) << line;
}

// the method-slot cross-check must report a mismatch (no "MATCH") when rip - r15 does
// not equal the value at [tag + 0x40] -- e.g. a different register held the actual
// dispatching receiver, or the fault was not a dispatch fault at all.
TEST(GoalCrashMap, FormatReceiverMethodSlotMismatchIsNotReportedAsMatch) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 receiver = 0x1004;
  const u32 tag = 0x1804;
  const u64 r15 = 0x5000000000ull;
  write_u32(mem, receiver - 4, tag);
  write_u32(mem, tag + 0x40, 0x3000);
  const u64 rip = r15 + 0x4000;  // does not match the 0x3000 slot value

  char out[256];
  goal_crash_map_format_receiver_for_test("rsi", receiver, mem.data(), window_size, 0x10, 0, rip,
                                          r15, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("method slot +0x40 (index 12): 0x3000 vs rip-r15 0x4000"), std::string::npos)
      << line;
  EXPECT_EQ(line.find("MATCH"), std::string::npos) << line;
}

// issue #716 round 2: the symbol-slot namer. A candidate inside [symtab_lo, symtab_hi)
// resolves its name through the same symbol_string_base + candidate - s7_offset
// indirection sym_to_string_ptr()/format_receiver() use, and is reported "bound" when its
// own value slot (candidate - 1, Symbol4<T>::value()'s byte-precise convention) holds
// something other than the slot's own address -- the VM's unbound-symbol sentinel.
TEST(GoalCrashMap, FormatSymbolSlotNamesBoundSlot) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 symtab_lo = 0x1000;
  const u32 symtab_hi = 0x1900;
  const u32 s7_offset = 0x1500;
  const u32 symbol_string_base = 0x1000;
  const u32 candidate = 0x1600;  // inside [symtab_lo, symtab_hi)
  const u32 name_ptr_addr = symbol_string_base + candidate - s7_offset;  // 0x1100
  const u32 str_ptr = 0x1200;

  write_u32(mem, name_ptr_addr, str_ptr);
  write_cstr(mem, str_ptr + 4, "teleport");
  write_u32(mem, candidate - 1, 0x12345678);  // bound: value != candidate

  char out[128];
  bool ok = goal_crash_map_format_symbol_slot_for_test(candidate, mem.data(), window_size,
                                                       symtab_lo, symtab_hi, s7_offset,
                                                       symbol_string_base, out, sizeof(out));
  ASSERT_TRUE(ok);
  EXPECT_STREQ(out, "symbol slot 'teleport' (bound)");
}

// an interned-but-never-assigned symbol's own value defaults to its own address (the
// VM's unbound sentinel, not 0): the namer must report that as "unbound", not crash on
// the self-referential read or misreport it as bound.
TEST(GoalCrashMap, FormatSymbolSlotNamesUnboundSlot) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 symtab_lo = 0x1000;
  const u32 symtab_hi = 0x1900;
  const u32 s7_offset = 0x1500;
  const u32 symbol_string_base = 0x1000;
  const u32 candidate = 0x1600;
  const u32 name_ptr_addr = symbol_string_base + candidate - s7_offset;
  const u32 str_ptr = 0x1200;

  write_u32(mem, name_ptr_addr, str_ptr);
  write_cstr(mem, str_ptr + 4, "some-unbound-sym");
  write_u32(mem, candidate - 1, candidate);  // unbound: value == candidate (self-reference)

  char out[128];
  bool ok = goal_crash_map_format_symbol_slot_for_test(candidate, mem.data(), window_size,
                                                       symtab_lo, symtab_hi, s7_offset,
                                                       symbol_string_base, out, sizeof(out));
  ASSERT_TRUE(ok);
  EXPECT_STREQ(out, "symbol slot 'some-unbound-sym' (unbound)");
}

// a candidate outside the registered symbol-table region (including the [0,0) disabled
// default) is not a symbol slot at all -- must not attempt the string-table indirection.
TEST(GoalCrashMap, FormatSymbolSlotOutsideRegionReturnsFalse) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  char out[128] = "untouched";

  EXPECT_FALSE(goal_crash_map_format_symbol_slot_for_test(
      0x1950, mem.data(), window_size, 0x1000, 0x1900, 0x1500, 0x1000, out, sizeof(out)));
  EXPECT_FALSE(goal_crash_map_format_symbol_slot_for_test(0x1600, mem.data(), window_size,
                                                          /*symtab_lo=*/0, /*symtab_hi=*/0, 0x1500,
                                                          0x1000, out, sizeof(out)));
  EXPECT_STREQ(out, "untouched");
}

// symbol_string_base of 0 means no game has registered a table (every game but jakx, or
// jakx before InitHeapAndSymbol() runs): must skip resolution entirely, matching
// format_receiver()'s own posture for the same 0 default.
TEST(GoalCrashMap, FormatSymbolSlotNoTableRegisteredReturnsFalse) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  char out[128];

  EXPECT_FALSE(goal_crash_map_format_symbol_slot_for_test(
      0x1600, mem.data(), window_size, 0x1000, 0x1900, 0x1500,
      /*symbol_string_base=*/0, out, sizeof(out)));
}

// the string-table indirection resolving past window_size must degrade gracefully (no
// out-of-bounds read), mirroring FormatReceiverNameLookupPastWindowIsGraceful above.
TEST(GoalCrashMap, FormatSymbolSlotGracefulWhenNamePastWindow) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 symtab_lo = 0x1000;
  const u32 symtab_hi = 0x1ff0;
  const u32 s7_offset = 0x1000;
  // symbol_string_base chosen so name_ptr_addr = 0x1ff0 + 0x50 - 0x10... simpler: push it
  // straight past window_size.
  const u32 symbol_string_base = 0x1ff0;
  const u32 candidate = 0x1050;  // name_ptr_addr = 0x1ff0 + 0x1050 - 0x1000 = 0x2040 > window

  char out[128];
  EXPECT_FALSE(goal_crash_map_format_symbol_slot_for_test(candidate, mem.data(), window_size,
                                                          symtab_lo, symtab_hi, s7_offset,
                                                          symbol_string_base, out, sizeof(out)));
}

// issue #716 round 4: the heap scan's type-hierarchy walk. A tag equal to process_addr
// itself matches immediately, before any parent read -- proven by leaving the parent
// field un-populated (zeroed) and still getting a match.
TEST(GoalCrashMap, TypeIsProcessSubtypeDirectMatch) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  const u32 process_addr = 0x1004;
  EXPECT_TRUE(goal_crash_map_type_is_process_subtype_for_test(process_addr, mem.data(), window_size,
                                                              process_addr));
}

// a genuine subtype (child -> mid -> process, three hops) must be found by climbing
// Type::parent.
TEST(GoalCrashMap, TypeIsProcessSubtypeViaParentChain) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  const u32 process_addr = 0x1004;
  const u32 mid_addr = 0x1104;
  const u32 child_addr = 0x1204;
  write_u32(mem, child_addr + 4, mid_addr);    // child.parent = mid
  write_u32(mem, mid_addr + 4, process_addr);  // mid.parent = process

  EXPECT_TRUE(goal_crash_map_type_is_process_subtype_for_test(child_addr, mem.data(), window_size,
                                                              process_addr));
}

// a chain that never reaches process_addr (terminates at a self-parented root, the
// hierarchy's own convention for "no more ancestors") must return false, not loop.
TEST(GoalCrashMap, TypeIsProcessSubtypeUnrelatedChainNoMatch) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  const u32 process_addr = 0x1004;
  const u32 root_addr = 0x1304;
  const u32 leaf_addr = 0x1404;
  write_u32(mem, leaf_addr + 4, root_addr);  // leaf.parent = root
  write_u32(mem, root_addr + 4, root_addr);  // root.parent = root (self-parented root)

  EXPECT_FALSE(goal_crash_map_type_is_process_subtype_for_test(leaf_addr, mem.data(), window_size,
                                                               process_addr));
}

// a 2-cycle that never reaches process_addr must not hang the walk; MAX_TYPE_PARENT_HOPS
// bounds it and the walk returns false once the budget runs out.
TEST(GoalCrashMap, TypeIsProcessSubtypeCycleIsBounded) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  const u32 process_addr = 0x1004;
  const u32 a_addr = 0x1504;
  const u32 b_addr = 0x1604;
  write_u32(mem, a_addr + 4, b_addr);  // a.parent = b
  write_u32(mem, b_addr + 4, a_addr);  // b.parent = a (2-cycle, never reaches process_addr)

  EXPECT_FALSE(goal_crash_map_type_is_process_subtype_for_test(a_addr, mem.data(), window_size,
                                                               process_addr));
}

// process_addr == 0 means "not registered" and must disable the check unconditionally,
// even for a tag that would otherwise match trivially.
TEST(GoalCrashMap, TypeIsProcessSubtypeDisabledWhenProcessAddrIsZero) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);
  EXPECT_FALSE(goal_crash_map_type_is_process_subtype_for_test(0x1004, mem.data(), window_size, 0));
}

// issue #716 round 4: the rreg line. Each of the 7 slots is classified independently:
// a raw zero flags "(ZERO)"; a value resolving (absolute or raw-offset) to a recorded
// object prints its name; one landing in the registered symbol-table region is flagged;
// one that resolves to neither is flagged "UNMAPPED".
TEST(GoalCrashMap, FormatRregLineClassifiesEachSlot) {
  const u32 obj = 0x00f00000;
  goal_crash_map_record(obj, "cpu-thread-owner", 0x100);
  const u64 base_addr = 0x3000000000ull;
  const u64 mem_size = 0x8000000ull;
  const u32 symtab_lo = 0x180000;
  const u32 symtab_hi = 0x190000;

  u64 rreg[7] = {
      0,                       // [0] ZERO
      base_addr + obj + 0x10,  // [1] resolves via lookup() (absolute reading)
      0x185000,                // [2] in symbol-table region (raw-offset reading)
      0x00c50000,              // [3] plausible GOAL address, unmapped (disjoint from every
                               //     other test's recorded ranges in this file)
      obj + 0x50,              // [4] resolves via lookup() (raw-offset reading)
      0,                       // [5] ZERO again
      0x185500,                // [6] in symbol-table region again
  };

  char out[512];
  goal_crash_map_format_rreg_line_for_test(rreg, base_addr, mem_size, symtab_lo, symtab_hi, out,
                                           sizeof(out));
  std::string line(out);
  // don't assert the exact zero-padded hex text for value 0 (printf's alternate-form
  // "0x" prefix is specifically suppressed at value 0, per the same C99 7.19.6.1p6
  // convention already documented for %#x elsewhere in this file); just confirm the
  // flag landed right after the "[0]=" slot marker.
  EXPECT_NE(line.find("[0]="), std::string::npos) << line;
  EXPECT_NE(line.find("(ZERO)"), std::string::npos) << line;
  EXPECT_NE(line.find("[1]="), std::string::npos) << line;
  EXPECT_NE(line.find("(cpu-thread-owner+0x10)"), std::string::npos) << line;
  EXPECT_NE(line.find("(SYMBOL TABLE, FLAG)"), std::string::npos) << line;
  EXPECT_NE(line.find("(UNMAPPED, FLAG)"), std::string::npos) << line;
  EXPECT_NE(line.find("(cpu-thread-owner+0x50)"), std::string::npos) << line;
}

// issue #716 round 4: the raw stack window's GOAL-range half. A value inside a recorded
// object's extent names it; a value in GOAL range but past every recorded extent still
// reports as GOAL space, just unmapped; a value outside [base_addr, base_addr+mem_size)
// is not a plausible GOAL address at all and the function returns false (the caller
// falls back to host-module attribution in that case).
TEST(GoalCrashMap, FormatStackGoalAttributionResolvesRecordedObject) {
  const u32 obj = 0x00d10000;
  goal_crash_map_record(obj, "gkernel", 0x100);
  const u64 base_addr = 0x4000000000ull;
  const u64 mem_size = 0x8000000ull;

  char out[160];
  EXPECT_TRUE(goal_crash_map_format_stack_goal_attribution_for_test(
      base_addr + obj + 0x20, base_addr, mem_size, out, sizeof(out)));
  std::string line(out);
  EXPECT_NE(line.find("gkernel+0x20 [0xd10000,+0x100) (goal 0xd10020)"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatStackGoalAttributionUnmappedStillInGoalRange) {
  const u64 base_addr = 0x4000000000ull;
  const u64 mem_size = 0x8000000ull;

  char out[160];
  EXPECT_TRUE(goal_crash_map_format_stack_goal_attribution_for_test(
      base_addr + 0x00e20000, base_addr, mem_size, out, sizeof(out)));
  std::string line(out);
  EXPECT_NE(line.find("(goal 0xe20000, unmapped)"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatStackGoalAttributionFalseOutsideGoalRange) {
  const u64 base_addr = 0x4000000000ull;
  const u64 mem_size = 0x8000000ull;

  char out[160] = "untouched";
  EXPECT_FALSE(goal_crash_map_format_stack_goal_attribution_for_test(0x7ff600000000ull, base_addr,
                                                                     mem_size, out, sizeof(out)));
  EXPECT_STREQ(out, "untouched");
}

// issue #716 round 5: the pp thread-field classifier. A raw zero is flagged distinctly
// (round 1/2's "zero where a code pointer belongs" class); a value resolving through the
// real object map names it (what a healthy suspend-hook/resume-hook should do); a value
// resolving through the registered symbol-table region instead is exactly the
// discriminator this dump exists to catch -- a hook corrupted to #f or another symbol;
// anything else is flagged unmapped.
TEST(GoalCrashMap, FormatThreadFieldFlagsZero) {
  char out[256];
  goal_crash_map_format_thread_field_for_test("resume-hook", 0, nullptr, 0, 0, 0, 0, 0, out,
                                              sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("resume-hook"), std::string::npos) << line;
  EXPECT_NE(line.find("(goal 0)"), std::string::npos) << line;
  EXPECT_NE(line.find("ZERO"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatThreadFieldResolvesRealFunction) {
  const u32 obj = 0x00b10000;
  goal_crash_map_record(obj, "gkernel", 0x5900);

  char out[256];
  goal_crash_map_format_thread_field_for_test("resume-hook", obj + 0x270, nullptr, 0x8000000, 0, 0,
                                              0, 0, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("resume-hook"), std::string::npos) << line;
  EXPECT_NE(line.find("gkernel+0x270"), std::string::npos) << line;
  EXPECT_EQ(line.find("FLAG"), std::string::npos) << line;
}

// the exact discriminator this feature exists for: a hook field that resolves not to a
// function but to a named symbol-table slot (e.g. corrupted to hold #f).
TEST(GoalCrashMap, FormatThreadFieldFlagsSymbolTableSlot) {
  const u64 window_size = 0x2000;
  std::vector<u8> mem(window_size, 0);

  const u32 symtab_lo = 0x1000;
  const u32 symtab_hi = 0x1900;
  const u32 s7_offset = 0x1500;
  const u32 symbol_string_base = 0x1000;
  const u32 candidate = 0x1600;
  const u32 name_ptr_addr = symbol_string_base + candidate - s7_offset;
  const u32 str_ptr = 0x1200;
  write_u32(mem, name_ptr_addr, str_ptr);
  write_cstr(mem, str_ptr + 4, "#f");
  write_u32(mem, candidate - 1, candidate);  // unbound (self-referential): #f's own shape

  char out[256];
  goal_crash_map_format_thread_field_for_test("suspend-hook", candidate, mem.data(), window_size,
                                              symtab_lo, symtab_hi, s7_offset, symbol_string_base,
                                              out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("suspend-hook"), std::string::npos) << line;
  EXPECT_NE(line.find("symbol slot '#f'"), std::string::npos) << line;
}

TEST(GoalCrashMap, FormatThreadFieldFlagsUnmapped) {
  char out[256];
  goal_crash_map_format_thread_field_for_test("previous", 0x00c99999, nullptr, 0x8000000, 0, 0, 0,
                                              0, out, sizeof(out));
  std::string line(out);
  EXPECT_NE(line.find("previous"), std::string::npos) << line;
  EXPECT_NE(line.find("UNMAPPED (FLAG)"), std::string::npos) << line;
}

// issue #716 round 6: the dispatch-order walk must reproduce execute-process-tree's own
// pre-order (recurse fully into a node's child, and everything under it, before
// touching its brother) -- proven against a small hand-built tree where a naive
// "push child then brother" stack (the round-1 sweep's own order, a latent mismatch
// this test pins down) would visit these leaves in a different order than the real
// dispatcher does.
//
// tree shape (child/brother links):
//   root.child = c1;  c1.brother = c2;  c2.brother = c3;  c1.child = c1a
// root and c1 are pools (mask bit set, not collected); c1a, c2, c3 are leaves.
// execute-process-tree's own recursion visits: root, then fully into c1's subtree
// (c1, then c1's own child c1a) before c1's sibling c2, then c2's sibling c3 --
// collected leaf order: c1a, c2, c3.
TEST(GoalCrashMap, WalkActivePoolDispatchOrderMatchesExecuteProcessTreePreOrder) {
  const u64 window_size = 0x3000;
  std::vector<u8> mem(window_size, 0);

  const u32 root = 0x1004;
  const u32 c1 = 0x1104;
  const u32 c1a = 0x1204;
  const u32 c2 = 0x1304;
  const u32 c3 = 0x1404;
  constexpr u32 MASK_OFF = 0x4;
  constexpr u32 BROTHER_OFF = 0x14;
  constexpr u32 CHILD_OFF = 0x18;
  constexpr u32 PROCESS_TREE_BIT = 0x100;

  write_u32(mem, root + MASK_OFF, PROCESS_TREE_BIT);
  write_u32(mem, root + CHILD_OFF, c1);

  write_u32(mem, c1 + MASK_OFF, PROCESS_TREE_BIT);
  write_u32(mem, c1 + CHILD_OFF, c1a);
  write_u32(mem, c1 + BROTHER_OFF, c2);

  write_u32(mem, c1a + MASK_OFF, 0);  // leaf

  write_u32(mem, c2 + MASK_OFF, 0);  // leaf
  write_u32(mem, c2 + BROTHER_OFF, c3);

  write_u32(mem, c3 + MASK_OFF, 0);  // leaf

  u32 collected[8] = {0};
  int n = goal_crash_map_walk_active_pool_dispatch_order_for_test(root, mem.data(), window_size,
                                                                  /*false_addr=*/0, collected, 8);
  ASSERT_EQ(n, 3);
  EXPECT_EQ(collected[0], c1a);
  EXPECT_EQ(collected[1], c2);
  EXPECT_EQ(collected[2], c3);
}

// max_count bounds the OUTPUT array; a corrupt or oversized tree must not overflow it.
TEST(GoalCrashMap, WalkActivePoolDispatchOrderRespectsMaxCount) {
  const u64 window_size = 0x3000;
  std::vector<u8> mem(window_size, 0);

  const u32 root = 0x2004;
  const u32 c1 = 0x2104;
  const u32 c2 = 0x2204;
  constexpr u32 MASK_OFF = 0x4;
  constexpr u32 BROTHER_OFF = 0x14;
  constexpr u32 CHILD_OFF = 0x18;

  write_u32(mem, root + MASK_OFF, 0x100);
  write_u32(mem, root + CHILD_OFF, c1);
  write_u32(mem, c1 + MASK_OFF, 0);  // leaf
  write_u32(mem, c1 + BROTHER_OFF, c2);
  write_u32(mem, c2 + MASK_OFF, 0);  // leaf

  u32 collected[1] = {0};
  int n = goal_crash_map_walk_active_pool_dispatch_order_for_test(root, mem.data(), window_size, 0,
                                                                  collected, 1);
  EXPECT_EQ(n, 1);
  EXPECT_EQ(collected[0], c1);
}

// issue #716 round 7: the DR7 bit math. From a clean (zero) DR7, arming DR0 as an
// execute breakpoint must set exactly L0 (bit 0) and G0 (bit 1), and leave R/W0
// (bits 16-17) and LEN0 (bits 18-19) at 00 -- execute, 1 byte, the only valid LEN for
// an execute breakpoint per the Intel/AMD spec, and the coordinator's own explicit care
// point.
TEST(GoalCrashMap, ComputeDr7ForDr0ExecuteSetsExactlyTheRightBits) {
  const u64 dr7 = goal_crash_map_compute_dr7_for_dr0_execute_for_test(0);
  EXPECT_EQ(dr7 & 0x3ull, 0x3ull) << "L0 and G0 must both be set";
  EXPECT_EQ((dr7 >> 16) & 0x3ull, 0ull) << "R/W0 must be 00 (execute-only)";
  EXPECT_EQ((dr7 >> 18) & 0x3ull, 0ull) << "LEN0 must be 00 (1 byte, the only valid LEN "
                                           "for an execute breakpoint)";
}

// arming must not disturb whatever bits already belong to DR1-DR3 (modeled here as the
// upper/other bits of a nonzero starting DR7).
TEST(GoalCrashMap, ComputeDr7ForDr0ExecutePreservesOtherDebugRegisterBits) {
  const u64 existing = 0x0000000Cull;  // stand-in "DR1 already configured" bits
  const u64 dr7 = goal_crash_map_compute_dr7_for_dr0_execute_for_test(existing);
  EXPECT_EQ(dr7 & existing, existing) << "pre-existing bits must survive arming DR0";
  EXPECT_EQ(dr7 & 0x3ull, 0x3ull);
}

// disarming must clear exactly L0/G0 and nothing else -- DR0's own address (Dr0 itself,
// not modeled in DR7) and any DR1-DR3 configuration must survive.
TEST(GoalCrashMap, ComputeDr7WithDr0DisabledClearsOnlyL0G0) {
  const u64 armed = goal_crash_map_compute_dr7_for_dr0_execute_for_test(0x0000000Cull);
  const u64 disarmed = goal_crash_map_compute_dr7_with_dr0_disabled_for_test(armed);
  EXPECT_EQ(disarmed & 0x3ull, 0ull) << "L0 and G0 must both be cleared";
  EXPECT_EQ(disarmed & 0x0000000Cull, 0x0000000Cull) << "unrelated bits must survive";
}

// issue #716 round 7: the dispatch discriminator. Only EXCEPTION_SINGLE_STEP (the fixed
// NTSTATUS 0x80000004) with rip exactly equal to a nonzero armed address counts as
// "ours" -- proven against the near-miss cases that a real reproduction cannot
// exercise directly (a different exception code at the armed address; the right code
// at a different address; the right code and address but never armed at all).
TEST(GoalCrashMap, IsSymbolBreakpointHitMatchesOnlyExactCodeAndAddress) {
  constexpr unsigned long kSingleStep = 0x80000004UL;
  constexpr unsigned long kAccessViolation = 0xC0000005UL;
  const unsigned long long armed = 0x00007ff600123456ull;

  EXPECT_TRUE(goal_crash_map_is_symbol_breakpoint_hit_for_test(kSingleStep, armed, armed));
  EXPECT_FALSE(goal_crash_map_is_symbol_breakpoint_hit_for_test(kAccessViolation, armed, armed))
      << "wrong exception code must not match even at the armed address";
  EXPECT_FALSE(goal_crash_map_is_symbol_breakpoint_hit_for_test(kSingleStep, armed + 8, armed))
      << "right code at the wrong address must not match";
  EXPECT_FALSE(goal_crash_map_is_symbol_breakpoint_hit_for_test(kSingleStep, armed, 0))
      << "armed_host_addr == 0 means never armed, and must never match, even if some "
         "real rip happened to be exactly 0";
}

// issue #716 round 8: the dispatch-target guard shared by call_goal()/call_goal_on_stack()
// (game/kernel/common/kscheme.cpp) and the site-specific wrappers layered on top
// (call_goal_function_by_name, KernelDispatch's dispatcher_func). A target must be
// flagged invalid whether it is the raw zero GOAL never links to, or s7_offset itself
// -- GOAL's #f IS the address of the s7 symbol, not integer 0, the trap this whole
// investigation keeps re-deriving.
TEST(GoalCrashMap, DispatchTargetIsInvalidFlagsZeroAndSymbolFalse) {
  const u32 s7_offset = 0x147d21;
  EXPECT_TRUE(goal_crash_map_dispatch_target_is_invalid(0, s7_offset))
      << "raw zero must be flagged";
  EXPECT_TRUE(goal_crash_map_dispatch_target_is_invalid(s7_offset, s7_offset))
      << "s7's own offset (#f) must be flagged, distinctly from zero";
}

// a real function offset -- anything other than 0 or s7_offset -- must never be flagged,
// or every legitimate dispatch in the game would be silently skipped.
TEST(GoalCrashMap, DispatchTargetIsInvalidPassesRealFunctionOffsets) {
  const u32 s7_offset = 0x147d21;
  EXPECT_FALSE(goal_crash_map_dispatch_target_is_invalid(0x187e01, s7_offset));
  EXPECT_FALSE(goal_crash_map_dispatch_target_is_invalid(1, s7_offset))
      << "the smallest nonzero, non-s7 offset must still pass";
}

// s7_offset itself defaults to 0 before kscheme_init_globals_common() runs (see
// kscheme.cpp); a target of 0 must still be flagged in that state, since a dispatch to
// literal address 0 is never valid regardless of whether s7 has been finalized yet.
TEST(GoalCrashMap, DispatchTargetIsInvalidFlagsZeroEvenWithUninitializedS7) {
  EXPECT_TRUE(goal_crash_map_dispatch_target_is_invalid(0, 0));
}
