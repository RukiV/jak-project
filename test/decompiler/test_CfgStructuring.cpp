/*!
 * Tests for the two structuring gaps closed by find_goto_forward (CfgVtx.cpp) and by
 * dead_code_is_only_var_defs (cfg_builder.cpp).
 *
 * The CFG tests here run only the block-finding + build_cfg part of the pipeline, so they
 * exercise the matcher itself rather than anything downstream of it. Each "green" case has a
 * matching "control" case that pins a shape the new matcher must NOT take over, since
 * find_goto_forward is the last matcher in the driver's cascade and its whole safety
 * argument is that the matchers ahead of it still win.
 */

#include "FormRegressionTest.h"

#include "decompiler/Disasm/InstructionParser.h"
#include "decompiler/Function/BasicBlocks.h"
#include "decompiler/Function/CfgVtx.h"
#include "decompiler/Function/Function.h"
#include "decompiler/IR2/AtomicOp.h"
#include "decompiler/IR2/Form.h"
#include "decompiler/ObjectFile/LinkedObjectFile.h"
#include "decompiler/analysis/cfg_builder.h"
#include "gtest/gtest.h"

using namespace decompiler;

namespace {

/*!
 * Build a CFG from hand-written MIPS. No type system, no atomic ops: just blocks and
 * structuring, which is all the matcher under test can see.
 */
class CfgStructuringTest : public ::testing::Test {
 protected:
  struct Built {
    std::unique_ptr<LinkedObjectFile> file;
    std::unique_ptr<Function> func;
    std::shared_ptr<ControlFlowGraph> cfg;

    bool resolved() const { return cfg->is_fully_resolved(); }
    std::string form() const { return cfg->to_form_string(); }
  };

  static Built build(const std::string& code) {
    InstructionParser parser;
    auto program = parser.parse_program(code, {});

    Built out;
    out.file = std::make_unique<LinkedObjectFile>(GameVersion::Jak1);
    out.file->words_by_seg.resize(3);
    out.file->labels = program.labels;

    out.func = std::make_unique<Function>(0, int(program.instructions.size()), GameVersion::Jak1);
    out.func->instructions = program.instructions;
    out.func->guessed_name.set_as_global("test-function");
    out.func->basic_blocks = find_blocks_in_function(*out.file, 0, *out.func);
    out.func->analyze_prologue(*out.file);
    out.cfg = build_cfg(*out.file, 0, *out.func, {}, {}, GameVersion::Jak1);
    return out;
  }
};

/*!
 * The shape jakx's get-car-skel-part compiles to: a bottom-tested loop whose body has a
 * conditional early-exit arm, and where the compiler laid that arm physically BEFORE the
 * arm that continues the loop. Block 2 is an unconditional forward goto to block 6, and
 * block 3 (the code right after it) is live because block 1 branches there.
 *
 * B0 -> B4          entry jump to the loop test
 * B1 -> B3 / B2     body test: branch = continue, fallthrough = found
 * B2 -> B6          found: unconditional forward goto out of the loop  <-- the shape
 * B3 -> B4          continue
 * B4 -> B1 / B5     loop test
 * B5 -> B6
 */
constexpr const char* kForwardGotoLiveFallthrough =
    "    sll r0, r0, 0\n"
    "    or v0, r0, r0\n"  // B0
    "    addiu v1, r0, 0\n"
    "    beq r0, r0, L4\n"
    "    sll r0, r0, 0\n"
    "L2:\n"
    "    beq s7, a0, L3\n"  // B1
    "    or a1, s7, r0\n"
    "    addiu v0, r0, 1\n"  // B2
    "    beq r0, r0, L6\n"
    "    sll r0, r0, 0\n"
    "L3:\n"
    "    daddiu v1, v1, 1\n"  // B3
    "L4:\n"
    "    slti a1, v1, 5\n"  // B4
    "    bne a1, r0, L2\n"
    "    sll r0, r0, 0\n"
    "    or a2, s7, r0\n"  // B5
    "L6:\n"
    "    jr ra\n"  // B6
    "    daddu sp, sp, r0";

TEST_F(CfgStructuringTest, ForwardGotoWithLiveFallthroughResolves) {
  auto built = build(kForwardGotoLiveFallthrough);
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  // the forward goto out of the loop became a Break with no unreachable block, pointing at
  // the block it jumps to.
  EXPECT_NE(built.form().find("(break 6"), std::string::npos) << built.form();
  EXPECT_NE(built.form().find("no-unreachable"), std::string::npos) << built.form();
}

/*!
 * Control: the SAME loop, but with the goto's destination reachable only from the goto
 * itself. find_goto_forward has to decline here, because deleting the goto's edge would
 * leave its own destination with an empty pred list, and every "is this dead code" test in
 * CfgVtx.cpp reads an empty pred list as "unreachable". This is the guard that jakx's
 * load-game-text-info proved was needed: without it that function structures and then dies
 * in clean_up_return_final with a dead-code region full of real calls.
 *
 * B0 -> B2          entry jump, skipping over B1
 * B1 -> B3          unconditional forward goto; B1 is only reached from B2
 * B2 -> B1 / B3     test
 * B3                end
 */
constexpr const char* kForwardGotoSinglePredDestination =
    "    sll r0, r0, 0\n"
    "    or v0, r0, r0\n"  // B0
    "    beq r0, r0, L2\n"
    "    sll r0, r0, 0\n"
    "L1:\n"
    "    addiu v0, r0, 1\n"  // B1: only pred is B2's branch
    "    beq r0, r0, L3\n"   //     forward goto; its dest B3 has other preds
    "    sll r0, r0, 0\n"
    "L2:\n"
    "    beq s7, a0, L1\n"  // B2
    "    or a1, s7, r0\n"
    "L3:\n"
    "    jr ra\n"  // B3
    "    daddu sp, sp, r0";

TEST_F(CfgStructuringTest, ForwardGotoDeclinedWhenItWouldOrphanItsDestination) {
  // B0's own goto (to L2) is the one whose destination has a single predecessor. Whatever
  // else happens, the matcher must never produce a Break for it, because B2 would then look
  // dead to find_goto_end / find_goto_not_end.
  auto built = build(kForwardGotoSinglePredDestination);
  EXPECT_EQ(built.form().find("(break 2"), std::string::npos)
      << "took over a goto whose destination has only that one predecessor:\n"
      << built.form();
  // not a vacuous assertion: the OTHER forward goto in this function (B1 -> B3, whose
  // destination keeps B2 as a predecessor) is taken, so the matcher did run here.
  EXPECT_NE(built.form().find("(break 3"), std::string::npos) << built.form();
}

/*!
 * Control: a forward goto out of a loop whose fall-through neighbor is genuinely dead, and
 * whose destination is an ordinary block rather than the function end. This is
 * find_goto_not_end's shape and it must keep it, because that matcher folds the dead block
 * into the Break (Break::unreachable_block) instead of pretending to fall through to it.
 *
 * B0 -> B3          entry jump to the loop test
 * B1 -> B4          unconditional forward goto out of the loop
 * B2                dead: nothing branches here
 * B3 -> B1 / B4     loop test
 * B4                ordinary block, so this is not a goto-to-end
 */
constexpr const char* kForwardGotoDeadFallthrough =
    "    sll r0, r0, 0\n"
    "    or v0, r0, r0\n"  // B0
    "    beq r0, r0, L3\n"
    "    sll r0, r0, 0\n"
    "L1:\n"
    "    daddiu v0, v0, 1\n"  // B1
    "    beq r0, r0, L4\n"
    "    sll r0, r0, 0\n"
    "    addiu v1, r0, 0\n"  // B2: dead code, two register writes
    "    addiu v1, r0, 1\n"
    "L3:\n"
    "    slti a1, v0, 5\n"  // B3
    "    bne a1, r0, L1\n"
    "    sll r0, r0, 0\n"
    "L4:\n"
    "    addiu v1, r0, 7\n"  // B4
    "    jr ra\n"
    "    daddu sp, sp, r0";

TEST_F(CfgStructuringTest, ForwardGotoWithDeadFallthroughStaysWithGotoNotEnd) {
  auto built = build(kForwardGotoDeadFallthrough);
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  // find_goto_not_end swallows the dead block, so this Break carries an unreachable block
  // rather than the no-unreachable marker find_goto_forward would have produced.
  EXPECT_NE(built.form().find("(break 4"), std::string::npos) << built.form();
  EXPECT_EQ(built.form().find("no-unreachable"), std::string::npos) << built.form();
}

/*!
 * Control: the same idea but jumping to the function's early-exit block, which is
 * find_goto_end's shape. It must stay a return, not become a labelled goto.
 */
TEST_F(CfgStructuringTest, ForwardGotoToEndWithDeadFallthroughStaysWithGotoEnd) {
  auto built = build(
      "    sll r0, r0, 0\n"
      "    or v0, r0, r0\n"
      "    beq r0, r0, L3\n"
      "    sll r0, r0, 0\n"
      "L1:\n"
      "    daddiu v0, v0, 1\n"
      "    beq r0, r0, L4\n"
      "    sll r0, r0, 0\n"
      "    addiu v1, r0, 0\n"
      "    addiu v1, r0, 1\n"
      "L3:\n"
      "    slti a1, v0, 5\n"
      "    bne a1, r0, L1\n"
      "    sll r0, r0, 0\n"
      "L4:\n"
      "    jr ra\n"
      "    daddu sp, sp, r0");
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  EXPECT_NE(built.form().find("return-from-function"), std::string::npos) << built.form();
  EXPECT_EQ(built.form().find("break"), std::string::npos) << built.form();
}

/*!
 * Control: an ordinary if/else, which the compiler emits with a NOP in the condition
 * branch's delay slot. Its first arm ends in an unconditional forward branch to the join
 * and is followed by a live block (the else arm), which is superficially the same pattern
 * find_goto_forward matches - so this pins that find_cond_w_else still claims it. It does,
 * because find_goto_forward runs only after every other matcher has declined.
 */
TEST_F(CfgStructuringTest, PlainIfElseIsStillACondWithElse) {
  auto built = build(
      "    sll r0, r0, 0\n"
      "    beq s7, a0, L1\n"
      "    sll r0, r0, 0\n"
      "    addiu v0, r0, 1\n"
      "    beq r0, r0, L2\n"
      "    sll r0, r0, 0\n"
      "L1:\n"
      "    addiu v0, r0, 2\n"
      "L2:\n"
      "    addiu v1, r0, 7\n"
      "    jr ra\n"
      "    daddu sp, sp, r0");
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  EXPECT_NE(built.form().find("cond"), std::string::npos) << built.form();
  EXPECT_EQ(built.form().find("break"), std::string::npos) << built.form();
}

/*!
 * Control: a plain cond with no else - the condition branch sets the result register to #f
 * in its delay slot, the single body falls straight into the join. find_cond_n_else claims
 * this, and it is the matcher get-car-skel-part narrowly misses, so pinning it here proves
 * the new last-resort matcher did not move that boundary.
 */
TEST_F(CfgStructuringTest, PlainCondNoElseIsStillACondNoElse) {
  auto built = build(
      "    sll r0, r0, 0\n"
      "    beq s7, a0, L1\n"
      "    or v0, s7, r0\n"
      "    addiu v0, r0, 1\n"
      "L1:\n"
      "    addiu v1, r0, 7\n"
      "    jr ra\n"
      "    daddu sp, sp, r0");
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  EXPECT_NE(built.form().find("cond"), std::string::npos) << built.form();
  EXPECT_EQ(built.form().find("break"), std::string::npos) << built.form();
}

/*!
 * Control: a plain bottom-tested while loop, with no early exit at all. Nothing here should
 * become a Break.
 */
TEST_F(CfgStructuringTest, PlainWhileLoopIsUnaffected) {
  auto built = build(
      "    sll r0, r0, 0\n"
      "    addiu v1, r0, 0\n"
      "    beq r0, r0, L2\n"
      "    sll r0, r0, 0\n"
      "L1:\n"
      "    daddiu v1, v1, 1\n"
      "L2:\n"
      "    slti a1, v1, 5\n"
      "    bne a1, r0, L1\n"
      "    sll r0, r0, 0\n"
      "    jr ra\n"
      "    daddu sp, sp, r0");
  EXPECT_TRUE(built.resolved()) << "CFG did not resolve:\n" << built.cfg->to_dot();
  EXPECT_NE(built.form().find("while"), std::string::npos) << built.form();
  EXPECT_EQ(built.form().find("break"), std::string::npos) << built.form();
}

/*!
 * Tests for the dead-code rule a break's unreachable region has to satisfy before
 * clean_up_break_final will drop it.
 */
class DeadCodeAfterBreakTest : public ::testing::Test {
 protected:
  FormPool pool;

  RegisterAccess reg(Reg::Gpr r, int idx) {
    return RegisterAccess(AccessMode::WRITE, Register(Reg::GPR, r), idx);
  }

  /*! (set! <reg> <int>) - a plain dead register definition. */
  FormElement* var_def(Reg::Gpr r, int idx, int value) {
    return pool.alloc_element<SetVarElement>(reg(r, idx), pool.form<SimpleAtomElement>(value), true,
                                             TypeSpec("int"));
  }

  /*! (set! <reg> (call!)) - a register definition whose source has a side effect. */
  FormElement* call_def(Reg::Gpr r, int idx) {
    m_call_ops.push_back(std::make_unique<CallOp>(idx));
    return pool.alloc_element<SetVarElement>(
        reg(r, idx), pool.form<FunctionCallElement>(m_call_ops.back().get()), true,
        TypeSpec("none"));
  }

  Form* region(const std::vector<FormElement*>& elts) {
    return pool.alloc_sequence_form(nullptr, elts);
  }

 private:
  std::vector<std::unique_ptr<CallOp>> m_call_ops;
};

/*!
 * The jakx command-get-process shape: five dead register definitions in a row. The old rule
 * only accepted one (plus trailing empties), so this region threw
 * "failed to recognize dead code after break".
 */
TEST_F(DeadCodeAfterBreakTest, RunOfVarDefsIsDroppable) {
  EXPECT_TRUE(dead_code_is_only_var_defs(
      region({var_def(Reg::V1, 31, 0), var_def(Reg::V1, 32, 0), var_def(Reg::V1, 33, 0),
              var_def(Reg::V1, 34, 0), var_def(Reg::V1, 29, 0)})));
}

TEST_F(DeadCodeAfterBreakTest, SingleVarDefIsDroppable) {
  EXPECT_TRUE(dead_code_is_only_var_defs(region({var_def(Reg::V1, 0, 0)})));
}

/*! A region with a call in it is NOT droppable, even though it is unreachable. */
TEST_F(DeadCodeAfterBreakTest, RegionContainingACallIsNotDroppable) {
  EXPECT_FALSE(dead_code_is_only_var_defs(
      region({var_def(Reg::V1, 0, 0), call_def(Reg::V0, 1), var_def(Reg::V1, 2, 0)})));
}

/*! Anything that is not a register definition at top level is not droppable either. */
TEST_F(DeadCodeAfterBreakTest, RegionWithANonVarDefElementIsNotDroppable) {
  EXPECT_FALSE(dead_code_is_only_var_defs(
      region({var_def(Reg::V1, 0, 0), pool.alloc_element<ConstantTokenElement>("something")})));
}

TEST_F(DeadCodeAfterBreakTest, EmptyOrNullRegionIsNotDroppable) {
  EXPECT_FALSE(dead_code_is_only_var_defs(nullptr));
  EXPECT_FALSE(dead_code_is_only_var_defs(region({})));
}

}  // namespace

/*!
 * The rule above, driven through the real pipeline rather than called directly: a loop with
 * an unconditional break whose dead tail is TWO register definitions. Before the rule was
 * widened, clean_up_break_final threw "failed to recognize dead code after break" on this,
 * build_initial_forms swallowed the exception, and the function produced no forms at all.
 */
TEST_F(FormRegressionTestJak1, BreakWithARunOfDeadVarDefs) {
  std::string func =
      "    sll r0, r0, 0\n"
      "    or v0, r0, r0\n"
      "    beq r0, r0, L3\n"
      "    sll r0, r0, 0\n"
      "L1:\n"
      "    daddiu v0, v0, 1\n"
      "    beq r0, r0, L4\n"
      "    sll r0, r0, 0\n"
      "    addiu v1, r0, 0\n"  // dead
      "    addiu v1, r0, 1\n"  // dead
      "L3:\n"
      "    slti a1, v0, 5\n"
      "    bne a1, r0, L1\n"
      "    sll r0, r0, 0\n"
      "L4:\n"
      // an ordinary block here, so the break's destination is a label rather than the
      // function's early-exit block: that keeps this on clean_up_break_final's path
      // instead of clean_up_return_final's, which has the same narrow rule and is
      // deliberately left alone by this change.
      "    addiu v1, r0, 7\n"
      "    jr ra\n"
      "    daddu sp, sp, r0";
  std::string type = "(function int)";
  // the two dead register definitions are gone; everything reachable survives.
  std::string expected =
      "(begin"
      "  (set! v0-0 0)"
      "  (while (<.si v0-0 5)"
      "    (begin (set! v0-0 (+ v0-0 1)) (goto cfg-4))"
      "    )"
      "  (label cfg-4)"
      "  (set! v1-2 7)"
      "  (ret-value v0-0)"
      "  )";
  test_no_expr(func, type, expected);
}
