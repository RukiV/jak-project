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
  render_state->texture_pool->move_existing_to_vram(m_gpu_tex, fbp);
  m_offscreen_mode = true;
}
