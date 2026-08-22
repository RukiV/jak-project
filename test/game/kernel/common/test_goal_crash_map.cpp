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
