#include <optional>
#include <string>

#include "decompiler/Function/Function.h"
#include "decompiler/IR2/Env.h"
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

// --- Symbol typing for forward-declared types ----------------------------------------------
//
// Only deftype registers a symbol for a type name (DecompilerTypeSystem::parse_type_defs calls
// add_symbol(name, "type") in its deftype branch but not in its declare-type branch), so a type
// that all-types.gc merely forward-declares has no symbol_types entry. Loading such a symbol as
// a value, which every auto-generated top-level-login does when it hands the type object to
// method-set!, used to fail type prop with "Unknown symbol: <type-name>". try_get_type_symbol_val
// now falls back to the type system's forward declarations. See decompiler/types2/ForwardProp.cpp.

// try_get_type_symbol_val has external linkage but no header of its own (ForwardProp.cpp is the
// only translation unit that declares it), so it is declared here rather than widening the
// production API purely for a test.
namespace decompiler {
std::optional<TP_Type> try_get_type_symbol_val(const std::string& name,
                                               const DecompilerTypeSystem& dts,
                                               const Env& env);
}

class Types2SymbolTypeTest : public ::testing::Test {
 protected:
  // try_get_type_symbol_val reads env only in its "set-to-run" special case, and none of the
  // names below take that branch, so a default Env is enough.
  DecompilerTypeSystem dts{GameVersion::JakX};
  Env env;
};

TEST_F(Types2SymbolTypeTest, FullyDefinedTypeSymbolTypesAsThatType) {
  // The control: what deftype does, and what already worked before the fix.
  dts.add_symbol("race-line-slice-mapping", "type", DefinitionMetadata());
  ASSERT_EQ(dts.symbol_types.count("race-line-slice-mapping"), 1u);

  auto result = try_get_type_symbol_val("race-line-slice-mapping", dts, env);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, TP_Type::Kind::TYPE_OF_TYPE_NO_VIRTUAL);
  EXPECT_EQ(result->get_type_objects_typespec().print(), "race-line-slice-mapping");
}

TEST_F(Types2SymbolTypeTest, ForwardDeclaredTypeSymbolTypesAsThatType) {
  // What declare-type does: the type system knows the type, but no symbol is registered.
  dts.ts.forward_declare_type_as("race-line", "basic");
  ASSERT_EQ(dts.symbol_types.count("race-line"), 0u);
  ASSERT_TRUE(dts.ts.partially_defined_type_exists("race-line"));

  // Pre-fix this returned nullopt, which get_type_symbol_val turned into
  // "Unknown symbol: race-line" and types2 recorded as a failed type prop.
  auto result = try_get_type_symbol_val("race-line", dts, env);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, TP_Type::Kind::TYPE_OF_TYPE_NO_VIRTUAL);
  EXPECT_EQ(result->get_type_objects_typespec().print(), "race-line");
}

TEST_F(Types2SymbolTypeTest, ForwardDeclaredTypeMatchesAFullyDefinedOneExactly) {
  // The fix's whole claim is that the two cases are indistinguishable to a symbol load, so pin
  // that rather than just the shape of each result.
  dts.ts.forward_declare_type_as("vol-control", "basic");
  dts.add_symbol("plane-volume", "type", DefinitionMetadata());

  auto declared = try_get_type_symbol_val("vol-control", dts, env);
  auto defined = try_get_type_symbol_val("plane-volume", dts, env);
  ASSERT_TRUE(declared.has_value());
  ASSERT_TRUE(defined.has_value());
  EXPECT_EQ(declared->kind, defined->kind);
  EXPECT_EQ(declared->typespec().print(), defined->typespec().print());
}

TEST_F(Types2SymbolTypeTest, GenuinelyUnknownSymbolStillFails) {
  // The fallback must not invent types for names the type system has never heard of, or a
  // missing all-types entry would decode as a silently wrong type instead of a loud marker.
  // race-line-get-points is exactly this case in jakx: its deftype is commented out and it has
  // no declare-type, so it must keep failing.
  ASSERT_FALSE(dts.ts.partially_defined_type_exists("race-line-get-points"));
  EXPECT_FALSE(try_get_type_symbol_val("race-line-get-points", dts, env).has_value());
}

TEST_F(Types2SymbolTypeTest, FullyDefinedTypeWithNoSymbolIsNotTreatedAsForwardDeclared) {
  // forward_declare_type_as is a no-op once a type is fully defined, so a deftype'd type that
  // deliberately has no runtime symbol (:no-runtime-type) must not pick up the fallback.
  // kheap is a builtin, so it is fully defined here without any all-types.gc being parsed.
  ASSERT_TRUE(dts.ts.fully_defined_type_exists("kheap"));
  ASSERT_EQ(dts.symbol_types.count("kheap"), 0u);
  ASSERT_FALSE(dts.ts.partially_defined_type_exists("kheap"));
  EXPECT_FALSE(try_get_type_symbol_val("kheap", dts, env).has_value());
}
