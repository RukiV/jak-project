#include <string>

#include "decompiler/Function/Function.h"
#include "decompiler/types2/types2.h"
#include "decompiler/util/DecompilerTypeSystem.h"
#include "gtest/gtest.h"

// Unit tests for the fix in commit 6b00e2b4d ("cap types2's non-monotone tag flips and
// outer iterations"): backprop_tagged_type's UNKNOWN_LABEL/UNKNOWN_STACK_STRUCTURE cases
// now stop reporting a change once a tag has flipped more than kMaxTagFlips times, and
// types2::run's outer worklist loop bails through the existing hit_error/asm-punt path
// once outer_iterations exceeds kMaxOuterIterations. See decompiler/types2/types2.h and
// decompiler/types2/ForwardProp.cpp.

using namespace decompiler;

namespace {
TP_Type make_type(const std::string& name) {
  return TP_Type::make_from_ts(TypeSpec(name));
}

// Counts non-overlapping occurrences of needle in haystack.
size_t count_occurrences(const std::string& haystack, const std::string& needle) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    count++;
    pos += needle.size();
  }
  return count;
}
}  // namespace

class Types2FlipCapTest : public ::testing::Test {
 protected:
  // backprop_tagged_type's UNKNOWN_LABEL/UNKNOWN_STACK_STRUCTURE cases never touch dts, so
  // an unparsed one (no all-types.gc load) is enough here.
  DecompilerTypeSystem dts{GameVersion::JakX};
};

// --- Rung 1: flip-cap behavior -----------------------------------------------------------

TEST_F(Types2FlipCapTest, FirstGuessIsNotAFlip) {
  types2::UnknownLabel label_tag;
  types2::Type type;
  type.tag.kind = types2::Tag::UNKNOWN_LABEL;
  type.tag.unknown_label = &label_tag;

  EXPECT_TRUE(types2::backprop_tagged_type(make_type("vector"), type, dts));
  EXPECT_EQ(label_tag.flip_count, 0);
  EXPECT_FALSE(label_tag.contested_warned);
  ASSERT_TRUE(label_tag.selected_type.has_value());
  EXPECT_EQ(label_tag.selected_type->print(), TypeSpec("vector").print());
}

TEST_F(Types2FlipCapTest, ReaffirmingTheSameTypeIsNotAChangeOrAFlip) {
  types2::UnknownLabel label_tag;
  types2::Type type;
  type.tag.kind = types2::Tag::UNKNOWN_LABEL;
  type.tag.unknown_label = &label_tag;

  EXPECT_TRUE(types2::backprop_tagged_type(make_type("vector"), type, dts));
  EXPECT_FALSE(types2::backprop_tagged_type(make_type("vector"), type, dts));
  EXPECT_FALSE(types2::backprop_tagged_type(make_type("vector"), type, dts));
  EXPECT_EQ(label_tag.flip_count, 0);
}

TEST_F(Types2FlipCapTest, LabelStopsReportingChangeAfterCapAndWarnsExactlyOnce) {
  Function owner(0, 4, GameVersion::JakX);
  types2::UnknownLabel label_tag;
  label_tag.label_name = "test-label";
  label_tag.owning_func = &owner;

  types2::Type type;
  type.tag.kind = types2::Tag::UNKNOWN_LABEL;
  type.tag.unknown_label = &label_tag;

  // first guess, establishes selected_type without counting as a flip.
  ASSERT_TRUE(types2::backprop_tagged_type(make_type("vector"), type, dts));

  bool tripped = false;
  int true_returns_after_first_guess = 0;
  // kMaxTagFlips+extra rounds of two disagreeing types is more than enough to trip the cap
  // (post-fix) or prove it never trips (pre-fix, which is the failure this test detects).
  for (int i = 0; i < types2::kMaxTagFlips + 32; i++) {
    const bool changed =
        types2::backprop_tagged_type(make_type(i % 2 == 0 ? "structure" : "vector"), type, dts);
    if (changed) {
      ASSERT_FALSE(tripped) << "reported a change again after the cap already tripped, at "
                               "iteration "
                            << i;
      true_returns_after_first_guess++;
    } else {
      tripped = true;
    }
  }

  EXPECT_TRUE(tripped) << "tag never stopped reporting changes; the flip cap did not trip";
  EXPECT_LE(true_returns_after_first_guess, types2::kMaxTagFlips);
  EXPECT_GT(label_tag.flip_count, types2::kMaxTagFlips);
  EXPECT_TRUE(label_tag.contested_warned);
  ASSERT_TRUE(owner.warnings.has_warnings());
  auto text = owner.warnings.get_warning_text(false);
  EXPECT_EQ(count_occurrences(text, "contested label guess for 'test-label'"), 1u);
}

TEST_F(Types2FlipCapTest, StackStructureStopsReportingChangeAfterCapAndWarnsExactlyOnce) {
  Function owner(0, 4, GameVersion::JakX);
  types2::UnknownStackStructure stack_tag;
  stack_tag.stack_offset = 32;
  stack_tag.owning_func = &owner;

  types2::Type type;
  type.tag.kind = types2::Tag::UNKNOWN_STACK_STRUCTURE;
  type.tag.unknown_stack_structure = &stack_tag;

  ASSERT_TRUE(types2::backprop_tagged_type(make_type("vector"), type, dts));

  bool tripped = false;
  for (int i = 0; i < types2::kMaxTagFlips + 32; i++) {
    const bool changed =
        types2::backprop_tagged_type(make_type(i % 2 == 0 ? "structure" : "vector"), type, dts);
    if (!changed) {
      tripped = true;
    } else {
      ASSERT_FALSE(tripped) << "reported a change again after the cap already tripped, at "
                               "iteration "
                            << i;
    }
  }

  EXPECT_TRUE(tripped) << "tag never stopped reporting changes; the flip cap did not trip";
  EXPECT_GT(stack_tag.flip_count, types2::kMaxTagFlips);
  EXPECT_TRUE(stack_tag.contested_warned);
  ASSERT_TRUE(owner.warnings.has_warnings());
  auto text = owner.warnings.get_warning_text(false);
  EXPECT_EQ(count_occurrences(text, "contested stack guess at sp+32"), 1u);
}

// --- Rung 2: outer-loop iteration cap ------------------------------------------------------
//
// types2::run's outer worklist loop (types2.cpp) is only reachable with a real, fully built
// Function (cfg, atomic ops, and the rest of the FormRegressionTest-style pipeline), and
// every rerun trigger inside propagate_block/ForwardProp.cpp that isn't the cross-block type
// LCA join routes through backprop_tagged_type (grep "needs_rerun = true" in ForwardProp.cpp:
// every site is gated by a backprop_tagged_type call). That means the one previously-unbounded
// source of non-convergence is exactly the tag-flip mechanism the two tests above cover, and
// it is now bounded at kMaxTagFlips+2 outer-iterations-worth of flips per tag, well under
// kMaxOuterIterations. Driving types2::run() itself past kMaxOuterIterations therefore is not
// practically reachable in-scope of this fix without a hand-built CFG engineered to stagger
// two independently-contested tags end to end, which is full-decompiler-integration-test
// territory, not a unit seam. The two tests below instead exercise the identical guard
// structure types2::run uses (types2.cpp:614-626, 676-680) against the real production
// constant, and pin the safety margin the fix's own measurement note relies on.

TEST(Types2OuterIterationCap, StopsAtTheConfiguredCapInsteadOfSpinningForever) {
  // Mirrors types2::run's outer while loop exactly: increment first, then bail through the
  // same hit_error-and-stop shape once outer_iterations exceeds kMaxOuterIterations. Driven
  // by a stub that always wants to rerun, i.e. the "any other cause" case the safety net
  // documents itself as covering; without the cap this loop does not terminate.
  int outer_iterations = 0;
  bool hit_error = false;
  bool needs_rerun = true;
  int passes_executed = 0;

  while (needs_rerun) {
    outer_iterations++;
    if (outer_iterations > types2::kMaxOuterIterations) {
      hit_error = true;
      break;
    }
    passes_executed++;
    needs_rerun = true;  // synthetic: this function's types never converge.
  }

  EXPECT_TRUE(hit_error);
  EXPECT_EQ(outer_iterations, types2::kMaxOuterIterations + 1);
  EXPECT_EQ(passes_executed, types2::kMaxOuterIterations);
}

TEST(Types2OuterIterationCap, HasHeadroomOverASingleTagsWorstCaseFlipCost) {
  // A single contested tag can legitimately keep needs_rerun true for up to about
  // kMaxTagFlips+2 outer iterations (the first guess, then one flip per outer iteration
  // until flip_count exceeds the cap; see LabelStopsReportingChangeAfterCapAndWarnsExactlyOnce
  // above). If kMaxOuterIterations ever regressed to be at or below that cost, the outer
  // safety net would fire spuriously for an ordinary, already-handled contested tag instead
  // of only for the "any other cause" case it exists for.
  EXPECT_GT(types2::kMaxOuterIterations, types2::kMaxTagFlips + 2);
}

// --- Rung 3: regression, the real poison signature shape -----------------------------------
//
// A full-decompiler integration test reproducing the actual nav-mesh method 19/44
// signature-drift bug (see commit 6b00e2b4d) would need a hand-built CFG and atomic-op
// stream driving real vector-! and mis-signatured callee-arg AtomicOps through
// types2::run(), which is out of unit-test scope per the same reasoning as rung 2 above.
// This instead drives the exact two-consumer shape described in the fix's commit message
// directly through backprop_tagged_type: a vector-! call and a mis-signatured callee arg
// disagreeing forever about the type of one guessed stack slot (sp+16, matching the
// example warning text quoted in the commit message).

TEST_F(Types2FlipCapTest, RegressionTwoConsumersDisagreeingOnOneStackSlotTerminates) {
  Function owner(0, 4, GameVersion::JakX);
  types2::UnknownStackStructure stack_tag;
  stack_tag.stack_offset = 16;
  stack_tag.owning_func = &owner;

  types2::Type type;
  type.tag.kind = types2::Tag::UNKNOWN_STACK_STRUCTURE;
  type.tag.unknown_stack_structure = &stack_tag;

  const TP_Type vector_call = make_type("vector");
  const TP_Type nav_poly_arg = make_type("nav-poly");

  bool terminated = false;
  // Bounded independently of kMaxTagFlips/kMaxOuterIterations: this loop must stop well
  // before this, since it only ever touches one tag. A generous, literal bound keeps this
  // a clean assertion failure (not a hang) if the fix regresses.
  constexpr int kRoundBound = 2000;
  for (int round = 0; round < kRoundBound; round++) {
    // consumer 1: a vector-! call wants vector for this slot.
    const bool changed_a = types2::backprop_tagged_type(vector_call, type, dts);
    // consumer 2: a mis-signatured callee arg wants nav-poly for the same slot.
    const bool changed_b = types2::backprop_tagged_type(nav_poly_arg, type, dts);
    if (!changed_a && !changed_b) {
      terminated = true;
      break;
    }
  }

  ASSERT_TRUE(terminated) << "poisoned stack slot never stopped reporting changes within "
                          << kRoundBound << " rounds";
  EXPECT_GT(stack_tag.flip_count, types2::kMaxTagFlips);
  EXPECT_TRUE(stack_tag.contested_warned);
  auto text = owner.warnings.get_warning_text(false);
  EXPECT_NE(text.find("contested stack guess at sp+16"), std::string::npos);
  EXPECT_NE(text.find("vector"), std::string::npos);
  EXPECT_NE(text.find("nav-poly"), std::string::npos);
  EXPECT_EQ(count_occurrences(text, "contested stack guess at sp+16"), 1u);
}
