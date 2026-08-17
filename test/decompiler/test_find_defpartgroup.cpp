#include "FormRegressionTest.h"

#include "common/goos/PrettyPrinter.h"
#include "common/util/BitUtils.h"

#include "decompiler/IR2/Form.h"
#include "decompiler/ObjectFile/LinkedWord.h"
#include "decompiler/analysis/find_defpartgroup.h"
#include "gtest/gtest.h"

using namespace decompiler;

namespace {
int label_index(const LinkedObjectFile& file, const std::string& name) {
  for (size_t i = 0; i < file.labels.size(); i++) {
    if (file.labels[i].name == name) {
      return int(i);
    }
  }
  ADD_FAILURE() << "label " << name << " not found";
  return -1;
}
}  // namespace

// Pins the jakx fallback recognizer added in find_defpartgroup.cpp: when an
// unrelated instruction later in the same top-level function makes
// types2::run bail with "Failed to guess label use" (types2.cpp), the whole
// function's types_succeeded goes false, ir2_build_expressions never runs on
// it, and a defpartgroup id-table store is left as three separate unfolded
// statements (a value def, a base def, and a StorePlainDeref) instead of one
// folded set!. do_expressions = false leaves top_form exactly in that raw,
// un-expression-converted state (the same state a whole-function
// types_succeeded bailout leaves behind), so this constructs the shape by
// hand and checks that run_defpartgroup still recovers a proper defpartgroup
// form from it.
TEST_F(FormRegressionTestJakX, RecognizesUnfoldedGroupIdTableStore) {
  std::string func =
      "    sll r0, r0, 0\n"
      "    lui v1, L100\n"
      "    ori v1, v1, L100\n"
      "    lw a0, *part-group-id-table*(s7)\n"
      "    sw v1, 32(a0)\n"  // id 5: (array T) has a 12-byte header, 12 + 5*4 = 32
      "    jr ra\n"
      "    daddu sp, sp, r0";

  TestSettings settings;
  settings.version = GameVersion::JakX;
  // L100 gets registered as a string purely so the old type-analysis pass
  // used by this test harness (run_type_analysis_ir2) has SOME known label
  // type to resolve v1's load against; its string bytes are abandoned below
  // once L100 gets repointed at the hand-built static structure. The
  // recognizer fallback under test never consults this declared type, only
  // the raw label index, so the mismatch does not affect what is pinned.
  settings.strings = {{"L100", "unused"}, {"L102", "group-test"}};
  settings.do_expressions = false;

  auto test = make_function(func, dts->parse_type_spec("(function none)"), settings);
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->func.ir2.top_form);

  // Point L100 at a hand-built sparticle-launch-group static structure
  // instead of the harmless code position the parser gave it, mirroring the
  // static data segment of a real object file. Layout matches
  // read_static_group_data in decompiler/analysis/find_defpartgroup.cpp.
  int name_label = label_index(test->file, "L102");
  ASSERT_GE(name_label, 0);

  auto& words = test->file.words_by_seg.at(1);
  int struct_start_word = int(words.size());

  LinkedWord type_tag(0);
  type_tag.set_to_symbol(LinkedWord::TYPE_PTR, "sparticle-launch-group");
  words.push_back(type_tag);       // [0] type ptr
  words.push_back(LinkedWord(0));  // [1] duration<<16 | len (len = 0, no parts)
  words.push_back(LinkedWord(0));  // [2] flags<<16 | linger

  LinkedWord name_word(0);
  name_word.set_to_pointer(LinkedWord::PTR, name_label);
  words.push_back(name_word);  // [3] name string ptr

  LinkedWord array_word(0);
  // reuses the string label; never actually dereferenced since len == 0.
  array_word.set_to_pointer(LinkedWord::PTR, name_label);
  words.push_back(array_word);  // [4] elt array ptr

  for (int i = 0; i < 3; i++) {
    words.push_back(LinkedWord(0));  // [5..7] rot, default (0, 0, 0)
  }
  u32 one_bits;
  float one_f = 1.0f;
  memcpy(&one_bits, &one_f, 4);
  for (int i = 0; i < 3; i++) {
    words.push_back(LinkedWord(one_bits));  // [8..10] scale, default (1, 1, 1)
  }
  int word_idx = struct_start_word + 11;
  int aligned = align4(word_idx);
  for (int i = word_idx; i < aligned; i++) {
    words.push_back(LinkedWord(0));  // padding before bounds
  }
  for (int i = 0; i < 4; i++) {
    words.push_back(LinkedWord(0));  // bounds (0, 0, 0, 0)
  }

  for (auto& label : test->file.labels) {
    if (label.name == "L100") {
      label.target_segment = 1;
      label.offset = 4 * (struct_start_word + 1);
    }
  }

  std::unordered_map<u32, std::string> part_group_table;
  run_defpartgroup(test->func, part_group_table);

  EXPECT_EQ(part_group_table.count(5), 1u);
  if (part_group_table.count(5)) {
    EXPECT_EQ(part_group_table.at(5), "group-test");
  }

  // top_form still wraps the epilogue's return statement alongside the
  // rewritten defpartgroup, and the exact printed shape of that return isn't
  // what this test is pinning, so check the rewritten content by substring
  // instead of an exact structural match against the whole top_form.
  auto actual_text = test->func.ir2.top_form->to_form(test->func.ir2.env).print();

  bool recognized = actual_text.find("(defpartgroup group-test") != std::string::npos &&
                    actual_text.find(":id 5") != std::string::npos &&
                    actual_text.find(":bounds (static-bspherem 0 0 0 0)") != std::string::npos;
  if (!recognized) {
    printf("Got:\n%s\n", actual_text.c_str());
  }
  EXPECT_TRUE(recognized);

  // the specific bug this pins: before the fix, the store's base and value
  // stay unfolded and the raw fallback printer leaks them as bare tokens
  // instead of the recognizer ever collapsing them into one defpartgroup.
  EXPECT_EQ(actual_text.find("*part-group-id-table*"), std::string::npos);
  EXPECT_EQ(actual_text.find("L100"), std::string::npos);
}
