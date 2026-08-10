// Unit tests for the jakx sprite adgif slot-4 TEST_1/CLAMP_1/ZBUF_1 routing fix (issue #145).
//
// Sprite3's real handle_test/handle_clamp/do_block_common are instance methods that need a live
// Sprite3, but Sprite3's constructor calls opengl_setup(), which issues real GL calls
// (glGenBuffers, glGenVertexArrays, ...). goalc-test never creates a GL context, so glad's
// function pointers are null until gladLoadGL runs, and calling any of them would segfault the
// whole test binary, not just fail one test. The pure decode/predicate/default-mode logic this
// fix introduces is exposed as free functions for exactly that reason (see the doc comments in
// Sprite3.h and background_common.h); these tests exercise those free functions directly.

#include "common/dma/gs.h"

#include "game/graphics/opengl_renderer/background/background_common.h"
#include "game/graphics/opengl_renderer/sprite/Sprite3.h"
#include "gtest/gtest.h"

// #145: jakx's particle adgifs put the GS TEST_1 register in adgif slot 4 where jak1/2/3 put
// ZBUF_1. 0x51001 is jakx's no-z-write idiom (alpha test NEVER + FB_ONLY): nothing should be
// discarded (aref_first stays 0) and there should be no double draw.
TEST(Sprite3DecodeTest1, NoZWriteIdiomPayload) {
  DrawMode mode;
  sprite3_decode_test1(0x51001, mode);

  EXPECT_TRUE(mode.get_at_enable());
  EXPECT_EQ(mode.get_alpha_test(), DrawMode::AlphaTest::NEVER);
  EXPECT_EQ(mode.get_alpha_fail(), GsTest::AlphaFail::FB_ONLY);
  EXPECT_FALSE(mode.get_depth_write_enable());

  auto settings = compute_alpha_test_draw_settings(mode);
  EXPECT_EQ(settings.double_draw.kind, DoubleDrawKind::NONE);
  EXPECT_FLOAT_EQ(settings.double_draw.aref_first, 0.f);
}

// #145: the engine's canonical sprite test payload, asserted verbatim against jak2/jak3's direct
// setup blob at Sprite3.cpp:187-188 (address 0x47 = GsRegisterAddress::TEST_1). This is the
// AFAIL_NO_DEPTH_WRITE double-draw idiom: depth writes stay on for the first pass, and a second
// pass redraws with a raised alpha floor.
TEST(Sprite3DecodeTest1, CanonicalIdiomPayload) {
  DrawMode mode;
  sprite3_decode_test1(0x5126B, mode);

  EXPECT_EQ(mode.get_aref(), 38);
  EXPECT_EQ(mode.get_alpha_test(), DrawMode::AlphaTest::GEQUAL);
  EXPECT_EQ(mode.get_alpha_fail(), GsTest::AlphaFail::FB_ONLY);
  EXPECT_TRUE(mode.get_zt_enable());
  EXPECT_EQ(mode.get_depth_test(), GsTest::ZTest::GEQUAL);
  EXPECT_TRUE(mode.get_depth_write_enable());

  auto settings = compute_alpha_test_draw_settings(mode);
  EXPECT_EQ(settings.double_draw.kind, DoubleDrawKind::AFAIL_NO_DEPTH_WRITE);
  EXPECT_FLOAT_EQ(settings.double_draw.aref_second, 38.f / 127.f);
}

// Locks m_default_mode (Sprite3.cpp:148-156, built by sprite3_default_mode()) against drift: it
// is exactly the canonical TEST_1 payload's decode, with alpha_blend forced to
// SRC_DST_SRC_DST (TEST_1 carries no blend info; that comes from the ALPHA_1 register instead)
// and depth-write cleared (the constructor calls disable_depth_write() and never re-enables it,
// unlike a live 0x5126b adgif, whose alpha test kind implies depth-write should be on).
TEST(Sprite3DecodeTest1, DefaultModeIsCanonicalDecodeWithDepthWriteCleared) {
  DrawMode expected;
  sprite3_decode_test1(0x5126B, expected);
  expected.set_alpha_blend(DrawMode::AlphaBlend::SRC_DST_SRC_DST);
  expected.disable_depth_write();

  EXPECT_EQ(expected, sprite3_default_mode());
}

// #145 revert of 8ae7ce942: both TEST_1 payloads that used to be misrouted into handle_clamp
// (before do_block_common routed slot 4 by register address) must fail the narrowed whitelist.
// This is what makes handle_clamp's ASSERT_MSG fire if a TEST_1 payload ever reaches it again.
// 0x51001 is the sharper regression guard: its low nibble happens to decode to a value the
// pre-#145 relaxed wms/wmt range check accepted.
TEST(Sprite3ClampWhitelist, RejectsMisroutedTest1Payloads) {
  EXPECT_FALSE(sprite3_clamp_value_is_valid(0x5126b));
  EXPECT_FALSE(sprite3_clamp_value_is_valid(0x51001));
}

// Sanity complement: the four legitimate CLAMP_1 combinations (wms/wmt each CLAMP or REPEAT)
// still pass, so the narrowing didn't overcorrect.
TEST(Sprite3ClampWhitelist, AcceptsTheFourValidCombinations) {
  EXPECT_TRUE(sprite3_clamp_value_is_valid(0));
  EXPECT_TRUE(sprite3_clamp_value_is_valid(1));
  EXPECT_TRUE(sprite3_clamp_value_is_valid(0b100));
  EXPECT_TRUE(sprite3_clamp_value_is_valid(0b101));
}
