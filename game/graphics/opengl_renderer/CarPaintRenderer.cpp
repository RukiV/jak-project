#include "CarPaintRenderer.h"

CarPaintRenderer::CarPaintRenderer(const std::string& name, int my_id, int batch_size)
    : DirectRenderer(name, my_id, batch_size) {}

void CarPaintRenderer::init_textures(TexturePool& texture_pool, GameVersion version) {
  // 1x1 placeholder: the real destination size is only known once the first
  // composite's set-display-gs-state packet (FRAME + SCISSOR) arrives in
  // handle_frame, so the FBO is resized there instead.
  m_fb.emplace(1, 1, GL_UNSIGNED_INT_8_8_8_8_REV);
  TextureInput in;
  in.gpu_texture = m_fb->texture();
  in.w = 1;
  in.h = 1;
  in.debug_page_name = "PC-CAR-PAINT";
  in.debug_name = m_name;
  in.id = texture_pool.allocate_pc_port_texture(version);
  m_gpu_tex = texture_pool.give_texture(in);
}

void CarPaintRenderer::render(DmaFollower& dma,
                              SharedRenderState* render_state,
                              ScopedProfilerNode& prof) {
  pre_render();
  reset_state();

  // Same walk as DirectRenderer::render, except transfers whose vif codes are
  // PC_PORT-marked are skipped instead of handed to render_vif: TEX buckets
  // carry PC_PORT uploads from frame one; skipping preserves pre-reland
  // behavior, see issue 649.
  while (dma.current_tag_offset() != render_state->next_bucket && !dma.ended()) {
    auto data = dma.read_and_advance();
    bool is_pc_port = data.vifcode0().kind == VifCode::Kind::PC_PORT ||
                      data.vifcode1().kind == VifCode::Kind::PC_PORT;
    if (data.size_bytes && m_enabled && !is_pc_port) {
      render_vif(data.vif0(), data.vif1(), data.data, data.size_bytes, render_state, prof);
    }

    if (dma.current_tag_offset() == render_state->default_regs_buffer) {
      dma.read_and_advance();  // cnt
      ASSERT(dma.current_tag().kind == DmaTag::Kind::RET);
      dma.read_and_advance();  // ret
    }
  }

  if (m_enabled) {
    flush_pending(render_state, prof);
  }
  post_render();
  glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void CarPaintRenderer::pre_render() {
  m_current_fbp = kScreenFbp;
}

void CarPaintRenderer::post_render() {
  m_fb_ctxt.reset();
  m_offscreen_mode = false;
}

void CarPaintRenderer::handle_frame(u64 val,
                                    SharedRenderState* render_state,
                                    ScopedProfilerNode& prof) {
  GsFrame f(val);
  u32 fbp = f.fbp();
  if (fbp == m_current_fbp) {
    return;
  }
  flush_pending(render_state, prof);
  m_prim_gl_state_needs_gl_update = true;
  m_current_fbp = fbp;

  if (fbp == kScreenFbp) {
    m_fb_ctxt.reset();
    m_offscreen_mode = false;
    return;
  }

  // set-display-gs-state's scissor is always anchored at the origin
  // (display.gc:449/jak2 display.gc:540: scax0/scay0 are never set, so they read
  // 0; scax1/scay1 are w-1/h-1), so this recovers the exact pixel size of the
  // destination texture that FRAME just pointed at.
  int w = int(m_scissor.scax1) - int(m_scissor.scax0) + 1;
  int h = int(m_scissor.scay1) - int(m_scissor.scay0) + 1;
  if (w < 1) {
    w = 1;
  }
  if (h < 1) {
    h = 1;
  }
  if (w != m_fb->width() || h != m_fb->height()) {
    m_fb.emplace(w, h, GL_UNSIGNED_INT_8_8_8_8_REV);
    render_state->texture_pool->update_gl_texture(m_gpu_tex, w, h, m_fb->texture());
  }
  m_fb_ctxt.emplace(*m_fb);
  // claim the car's own VRAM slot with our FBO texture so the merc render of the
  // car body, which samples that same tbp moments later in the same frame, picks
  // up the freshly painted composite instead of the static base texture.
  // move_existing_to_vram's slot index is a tbp (TexturePool.cpp:94-97, :273
  // index by dest[0]), but fbp is a word address divided by 32 (GOAL passes
  // (shr dest 5) as fbp, car-textures.gc:612; same convention as
  // ProgressRenderer's kMinimapVramAddr 4032 == kMinimapFbp 126 * 32), so it
  // has to be shifted back up to land in the right slot.
  render_state->texture_pool->move_existing_to_vram(m_gpu_tex, fbp << 5);
  m_offscreen_mode = true;
}
