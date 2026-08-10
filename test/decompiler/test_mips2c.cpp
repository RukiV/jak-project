#include "decompiler/analysis/mips2c.h"
#include "gtest/gtest.h"

using namespace decompiler;

TEST(DecompilerMips2C, GoalToCNameExistingBehavior) {
  // dashes become underscores, and !/?/* are dropped.
  EXPECT_EQ(goal_to_c_name("foreground-draw-hud"), "foreground_draw_hud");
  EXPECT_EQ(goal_to_c_name("debug-line-clip?"), "debug_line_clip");
  EXPECT_EQ(goal_to_c_name("cspace<-parented-transformq-joint!"),
            "cspace_parented_transformq_joint");
}

TEST(DecompilerMips2C, GoalToCNameAngleBracket) {
  // '<' must be dropped like !/?/*, not converted to an underscore, so that
  // "adgif-shader<-texture-with-update!" produces the same namespace name jak1, jak2
  // and jak3 all land: "adgif_shader_texture_with_update". Regression coverage for the
  // mips2c-texture item of #133: the generator used to emit
  // "adgif_shader<_texture_with_update", which is not a valid C++ identifier.
  EXPECT_EQ(goal_to_c_name("adgif-shader<-texture-with-update!"),
            "adgif_shader_texture_with_update");
}

TEST(DecompilerMips2C, GoalToCNamePlus) {
  // '+' is promised exactly once across every game's hacks.jsonc corpus
  // ("generic-no-light+envmap"), and the landed hand splice at
  // game/mips2c/jakx_functions/generic_effect.cpp names its namespace
  // "generic_no_light_envmap", i.e. '+' folds to an underscore like '-' does.
  EXPECT_EQ(goal_to_c_name("generic-no-light+envmap"), "generic_no_light_envmap");
}
