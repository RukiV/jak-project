#include "FormRegressionTest.h"

#include "gtest/gtest.h"

using namespace decompiler;

// Pins the base-plus-constant-stride pointer arithmetic recognizer added to
// TypeFieldLookup.cpp's try_reverse_lookup_array_like. car-tables' real
// *car-upgrade-info* extern used to be typed (pointer car-upgrade-info-array),
// a plain (non-boxed) pointer to a reference-typed structure whose only field
// is a nested inline array (car-upgrade-info-array -> upgrade-info-array x15
// -> upgrade-info x4). all-types now types that extern as the plain structure
// (the raw code stores the label straight into the symbol), so these tests
// declare their own pointer-typed symbol over the same structure rather than
// depend on a config entry that can move under them. get_deref_info models a non-boxed pointer to a
// reference type as an array of POINTER_SIZE (4-byte) slots, so any offset
// that happens to divide evenly by 4 looked like a clean array index before
// this fix, only to fail deref_matches once the actual load/store size
// didn't match (the offset 4 case below); any offset that does not divide
// evenly by 4 failed the array-style match outright (the offset 5 case
// below). Both used to fall through to a bare "(pointer car-upgrade-info-array)"
// with no field recognized, which decompiled the store as raw pointer math
// ((s.b! (+ v1 4) ...)), which goalc then rejects: "Cannot do math on a
// (pointer car-upgrade-info-array)." Both should now resolve through the
// nested inline arrays into upgrade-info's own fields. The instruction
// shape (load the symbol into v1, addiu a value into a0, store relative to
// v1) mirrors car-tables_disasm.gc's real top-level-login exactly, down to
// using addiu rather than daddiu for the byte value: the wider daddiu form
// hits an unrelated pre-existing "is_var" assertion in this test harness.
TEST_F(FormRegressionTestJakX, PointerStrideResolvesDerefMatchesFailure) {
  dts->add_symbol("*pointer-stride-test-info*",
                  TypeSpec("pointer", {TypeSpec("car-upgrade-info-array")}), DefinitionMetadata());
  // sb a0, 4(v1) : offset 4 is a multiple of POINTER_SIZE, so the
  // array-style reading treats it as "index 1 of an array of pointers"
  // before failing deref_matches on the 1-byte store size.
  std::string func =
      "    sll r0, r0, 0\n"
      "    lw v1, *pointer-stride-test-info*(s7)\n"
      "    addiu a0, r0, 5\n"
      "    sb a0, 4(v1)\n"
      "    or v0, r0, r0\n"
      "    jr ra\n"
      "    daddu sp, sp, r0";
  std::string type = "(function none)";
  std::string expected =
      "(begin\n"
      "  (let ((v1-0 *pointer-stride-test-info*))\n"
      "    (set! (-> v1-0 0 data 0 data 0 base) (the-as uint 5))\n"
      "    )\n"
      "  0\n"
      "  (none)\n"
      "  )";
  test_with_expr(func, type, expected);
}

TEST_F(FormRegressionTestJakX, PointerStrideResolvesOffsetIntoElement) {
  dts->add_symbol("*pointer-stride-test-info*",
                  TypeSpec("pointer", {TypeSpec("car-upgrade-info-array")}), DefinitionMetadata());
  // sb a0, 5(v1) : offset 5 does not divide evenly by POINTER_SIZE, so the
  // array-style reading fails outright (offset_into_elt != 0) before this
  // fix ever tried the pointee's own fields.
  std::string func =
      "    sll r0, r0, 0\n"
      "    lw v1, *pointer-stride-test-info*(s7)\n"
      "    addiu a0, r0, 5\n"
      "    sb a0, 5(v1)\n"
      "    or v0, r0, r0\n"
      "    jr ra\n"
      "    daddu sp, sp, r0";
  std::string type = "(function none)";
  std::string expected =
      "(begin\n"
      "  (let ((v1-0 *pointer-stride-test-info*))\n"
      "    (set! (-> v1-0 0 data 0 data 0 max) (the-as uint 5))\n"
      "    )\n"
      "  0\n"
      "  (none)\n"
      "  )";
  test_with_expr(func, type, expected);
}
