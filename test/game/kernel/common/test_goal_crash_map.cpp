// Unit tests for the goal crash map's bounded, latest-wins lookup (issue #117, the
// #132 follow-up). goal_crash_map_lookup_for_test() is a thin test seam declared in
// goal_crash_map.h: lookup() itself is file-static in goal_crash_map.cpp, so the seam
// just forwards to it under the same mutex goal_crash_map_record() uses.
//
// Each TEST below claims its own disjoint slice of GOAL-space addresses so the
// process-global record set (which only grows; there is no reset seam) can't let one
// test's records leak into another's lookups.

#include "game/kernel/common/goal_crash_map.h"
#include "gtest/gtest.h"

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
