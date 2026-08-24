#include "FormRegressionTest.h"

#include "gtest/gtest.h"

// Pins the movz conditional-move rewrite requested by forge issue #320.
//
// A general movz select (`movz dst, src, cond`: dst gets src when cond is
// zero, else keeps its value) survived expression conversion as a raw asm
// element and printed `.movz`, because OpenGoalMapping had a row for its
// sibling MOVN ("move-if-not-zero") but none for MOVZ. eye's boolean-idiom
// sites fold via convert_cmov_1 and never reach the table; sparticle/
// ripple/water carry the genuine select shape. This pins that a movz with a
// known condition register rewrites to the move-if-zero macro call, mirroring
// what MOVN already produces with move-if-not-zero.
TEST_F(FormRegressionTestJakX, RewritesMovzSelectToMoveIfZero) {
  std::string func =
      "    sll r0, r0, 0\n"
      "    addiu v0, r0, 32\n"
      "    addiu v1, r0, 7\n"
      "    movz v0, v1, a0\n"
      "    jr ra\n"
      "    daddu sp, sp, r0";
  std::string type = "(function int int)";
  std::string expected =
      "(begin (let ((v0-0 32) (v1-0 7)) (move-if-zero v0-1 v1-0 arg0 v0-0)) v0-1)";
  test_with_expr(func, type, expected);
}
