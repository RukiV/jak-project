#include "FormRegressionTest.h"

#include "gtest/gtest.h"

// Pins the bare-srl byte-extraction fold requested by forge issue #319.
//
// car-textures' (method 11 car-rgb-color) extracts four bytes from a packed
// rgba word. The top three bytes compile to dsll32+dsrl32 funnel-shift pairs,
// which the decompiler folds into nested (shr (shl ...)) forms. The lowest
// byte compiles to a single bare 32-bit `srl` (nothing to shift left), which
// the expression matcher had no rule for: it survived as an inline-assembly
// `.srl` form that goalc rejects outright ("No method or function named .srl
// for type int"). This pins that a bare srl with a known shift amount renders
// as the same shr form its three siblings produce.
TEST_F(FormRegressionTestJakX, FoldsBareSrlByteExtraction) {
  std::string func =
      "    sll r0, r0, 0\n"
      "    srl v0, a0, 24\n"
      "    jr ra\n"
      "    daddu sp, sp, r0";
  std::string type = "(function uint int)";
  std::string expected = "(shr arg0 24)";
  test_with_expr(func, type, expected);
}
