#pragma once

#include <optional>

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/DirectRenderer.h"
#include "game/graphics/opengl_renderer/opengl_utils.h"

/*!
 * Renderer for Jak X's per-car paint composite buckets (issue 649).
 *
 * car-info-full-method-16 (car-textures.gc:583-748) emits a raw retail-style GS
 * packet chain into one of these buckets per car slot: set-display-gs-state
 * (display.gc:449, jak2 display.gc:537-550 decompiled) redirects FRAME/SCISSOR to
 * the car's own diffuse texture in VRAM, car-info-full-method-20/21 draw
 * adgif+sprite paint layers onto it (the same adgif-tmpl/sprite-tmpl shape every
 * game's DirectRenderer already parses for HUD sprites), and
 * reset-display-gs-state (display.gc:142-194) switches FRAME back to the screen
 * at the very end. DirectRenderer's own handle_frame is a no-op, so none of this
 * currently lands anywhere; the destination texture is never written and the
 * merc render of the car body samples its uncomposited base (the white/checker
 * panels in issue 649).
 *
 * ProgressRenderer (game/graphics/opengl_renderer/ProgressRenderer.h, registered
 * for jak2/jak3's minimap in this file) is the sibling precedent for a DirectRenderer
 * subclass that overrides handle_frame to detect a FRAME switch and rebind an
 * FBO-backed texture in its place. CarPaintRenderer follows the same shape, but
 * the minimap has one fixed destination address and size where a car's paint
 * texture varies by vehicle and level, so the FBO here is sized and reclaimed
 * dynamically from the SCISSOR register each time FRAME changes, instead of
 * being fixed at construction.
 *
 * These buckets are also TEX-category, and from the very first frame of boot
 * (before any car exists) they carry PC_PORT-marked texture-upload packets
 * (TextureUploadHandler.cpp, same marker: VifCode::Kind::PC_PORT, immediate 12)
 * ahead of the raw GS composite. DirectRenderer::render_vif expects every
 * transfer to be a DIRECT/NOP/FLUSHA vifcode and asserts otherwise
 * (DirectRenderer.cpp, get_direct_qwc_or_nop); SkipRenderer used to drop these
 * buckets whole, uploads included (issue 649's PR 661/662 history). render() is
 * overridden here to keep dropping just the PC_PORT-marked transfers, byte for
 * byte what SkipRenderer discarded, while every genuine direct/adgif transfer
 * still flows to DirectRenderer's normal parse.
 */
class CarPaintRenderer : public DirectRenderer {
 public:
  // jakx's own reset-display-gs-state (display.gc:142-194) sets FRAME back to fbp
  // 406: the raw bytes it writes at the FRAME_1 A+D pair decode to 0x80196, whose
  // low 9 bits (the fbp field) are 0x196 = 406. This is the same mismatch already
  // noted in OpenGLRenderer.cpp's ProgressRenderer registration comment
  // (ProgressRenderer::kScreenFbp is jak2/jak3's 408, not jakx's 406). Any other
  // fbp seen in one of these buckets is a car's own paint destination texture.
  static constexpr int kScreenFbp = 406;

  CarPaintRenderer(const std::string& name, int my_id, int batch_size);
  void init_textures(TexturePool& texture_pool, GameVersion version) override;
  void render(DmaFollower& dma, SharedRenderState* render_state, ScopedProfilerNode& prof) override;
  void handle_frame(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof) override;
  void pre_render() override;
  void post_render() override;

 private:
  GpuTexture* m_gpu_tex = nullptr;
  std::optional<FramebufferTexturePair> m_fb;
  std::optional<FramebufferTexturePairContext> m_fb_ctxt;
  u32 m_current_fbp = kScreenFbp;
};
