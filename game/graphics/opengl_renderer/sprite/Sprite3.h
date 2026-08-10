
#pragma once

#include <map>

#include "common/dma/gs.h"
#include "common/math/Vector.h"
#include "common/versions/versions.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/DirectRenderer.h"
#include "game/graphics/opengl_renderer/background/background_common.h"
#include "game/graphics/opengl_renderer/sprite/GlowRenderer.h"
#include "game/graphics/opengl_renderer/sprite/sprite_common.h"

// #145 test seam: JakX's particle adgifs put the GS TEST_1 register in adgif slot 4 (where
// jak1/2/3 put ZBUF_1); do_block_common routes it to Sprite3::handle_test, which decodes it via
// sprite3_decode_test1 below. That decode, the narrowed CLAMP_1 whitelist predicate, and the
// pure builder for m_default_mode are free functions (not Sprite3 members) so test_Sprite3.cpp
// can exercise them without constructing a Sprite3: its constructor calls opengl_setup(), which
// issues real GL calls that segfault in the test binary (goalc-test never creates a GL context,
// so glad's function pointers are null until gladLoadGL runs).

// Pure decode of adgif slot 4's TEST_1 register into a DrawMode. Mirrors GlowRenderer's TEST_1
// handling (GlowRenderer.cpp:511-534) plus one addition: TEST_1 carries no z-write-mask bit the
// way ZBUF_1's zmsk does (Sprite3::handle_zbuf), so depth_write_enable follows the alpha test
// kind instead (an alpha test of NEVER is jakx's no-z-write idiom, payload 0x51001).
void sprite3_decode_test1(u64 val, DrawMode& mode);

// Acceptance predicate for adgif slot 4's CLAMP_1 payload: the strict whitelist from before
// 8ae7ce942's wms/wmt range-decode relaxation, which was widened based on a TEST_1 payload
// misrouted here before slot 4 was routed by register address (#145). REGION_* modes (wms/wmt >=
// 2) stay fatal, since the renderer cannot express them (#53 slice 4).
bool sprite3_clamp_value_is_valid(u64 val);

// Pure builder for m_default_mode (Sprite3.cpp:148-156), the baseline do_block_common resets
// m_current_mode to before decoding each sprite's adgif.
DrawMode sprite3_default_mode();

// #145 amendment: a live jungle boot hit an all-zero adgif slot 4 (register address 0, data 0)
// right after eco-blue/eco-yellow bring-up art-group load failures, and the fatal else below
// caught it. Zero/zero means the shader was never written -- an unpopulated adgif cache entry
// from a spawner whose art group failed to load -- and is a distinct, recoverable case from a
// genuinely unknown nonzero address. do_block_common's slot-4 dispatch needs a live Sprite3 (see
// the seam note above), so the address/data classification itself is pulled out into this pure
// function for the same reason sprite3_decode_test1 and sprite3_clamp_value_is_valid are: so
// test_Sprite3.cpp can exercise the SKIP_ZERO routing without a GL context.
enum class Sprite3AdgifSlot4Kind { ZBUF, TEST, CLAMP, SKIP_ZERO, FATAL };
Sprite3AdgifSlot4Kind sprite3_classify_adgif_slot4(u64 addr, u64 data);

class Sprite3 : public BucketRenderer {
 public:
  Sprite3(const std::string& name, int my_id);
  void render(DmaFollower& dma, SharedRenderState* render_state, ScopedProfilerNode& prof) override;
  void draw_debug_window() override;
  static constexpr int SPRITES_PER_CHUNK = 48;

 private:
  void render_jak1(DmaFollower& dma, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void render_jak2(DmaFollower& dma, SharedRenderState* render_state, ScopedProfilerNode& prof);

  // jakx bring-up (#53 slice 1): same empty-bucket peek guard and first-data probe as
  // Generic2BucketRenderer (#57 rung 4). One warning per renderer if an unrecognized
  // empty shape is skipped; one line on the first frame this bucket carries real data.
  // Retire once #53's slices land and the jakx sprite shape is proven.
  bool m_jakx_shape_warned = false;
  bool m_jakx_data_seen = false;

  void opengl_setup();
  void opengl_setup_normal();
  void opengl_setup_distort();

  bool render_direct(DmaFollower& dma, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void render_distorter(DmaFollower& dma,
                        SharedRenderState* render_state,
                        ScopedProfilerNode& prof);
  void distort_dma(GameVersion version, DmaFollower& dma, ScopedProfilerNode& prof);
  void distort_setup(ScopedProfilerNode& prof);
  void distort_setup_instanced(ScopedProfilerNode& prof);
  void distort_draw(SharedRenderState* render_state, ScopedProfilerNode& prof);
  void distort_draw_instanced(SharedRenderState* render_state, ScopedProfilerNode& prof);
  void distort_draw_common(SharedRenderState* render_state, ScopedProfilerNode& prof);
  void distort_setup_framebuffer_dims(SharedRenderState* render_state);
  void handle_sprite_frame_setup(DmaFollower& dma,
                                 GameVersion version,
                                 SharedRenderState* render_state,
                                 ScopedProfilerNode& prof);
  void render_3d(DmaFollower& dma);
  void render_2d_group0(DmaFollower& dma,
                        SharedRenderState* render_state,
                        ScopedProfilerNode& prof);
  void render_fake_shadow(DmaFollower& dma);
  void render_2d_group1(DmaFollower& dma,
                        SharedRenderState* render_state,
                        ScopedProfilerNode& prof);
  enum SpriteMode { Mode2D = 1, ModeHUD = 2, Mode3D = 3 };
  void do_block_common(SpriteMode mode,
                       u32 count,
                       SharedRenderState* render_state,
                       ScopedProfilerNode& prof);

  void update_mode_from_alpha1(u64 val, DrawMode& mode);
  void handle_tex0(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void handle_tex1(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  // void handle_mip(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void handle_zbuf(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void handle_test(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void handle_clamp(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);
  void handle_alpha(u64 val, SharedRenderState* render_state, ScopedProfilerNode& prof);

  void flush_sprites(SharedRenderState* render_state, ScopedProfilerNode& prof, bool double_draw);

  GlowRenderer m_glow_renderer;
  void glow_dma_and_draw(DmaFollower& dma,
                         SharedRenderState* render_state,
                         ScopedProfilerNode& prof);

  struct SpriteDistorterSetup {
    GifTag gif_tag;
    GsZbuf zbuf;
    u64 zbuf_addr;
    GsTex0 tex0;
    u64 tex0_addr;
    GsTex1 tex1;
    u64 tex1_addr;
    u64 miptbp;
    u64 miptbp_addr;
    u64 clamp;
    u64 clamp_addr;
    GsAlpha alpha;
    u64 alpha_addr;
  };
  static_assert(sizeof(SpriteDistorterSetup) == (7 * 16));

  struct SpriteDistorterSineTables {
    Vector4f entry[128];
    math::Vector<u32, 4> ientry[9];
    GifTag gs_gif_tag;
    math::Vector<u32, 4> color;
  };
  static_assert(sizeof(SpriteDistorterSineTables) == (0x8b * 16));

  struct SpriteDistortFrameData {
    math::Vector3f xyz;  // position
    float num_255;       // always 255.0
    math::Vector2f st;   // texture coords
    float num_1;         // always 1.0
    u32 flag;            // 'resolution' of the sprite
    Vector4f rgba;       // ? (doesn't seem to be color)
  };
  static_assert(sizeof(SpriteDistortFrameData) == 16 * 3);

  struct SpriteDistortVertex {
    math::Vector3f xyz;
    math::Vector2f st;
  };

  struct SpriteDistortInstanceData {
    math::Vector4f x_y_z_s;     // position, S-texture coord
    math::Vector4f sx_sy_sz_t;  // scale, T-texture coord
  };

  struct {
    GLuint vao;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLuint fbo;
    GLuint fbo_texture;
    int fbo_width = 640;
    int fbo_height = 480;
  } m_distort_ogl;

  struct {
    GLuint vao;
    GLuint vertex_buffer;    // contains vertex data for each possible sprite resolution (3-11)
    GLuint instance_buffer;  // contains all instance specific data for each sprite per frame
    float last_aspect_x = -1.0;
    float last_aspect_y = -1.0;
    bool vertex_data_changed = false;
  } m_distort_instanced_ogl;

  struct {
    int total_sprites;
    int total_tris;
  } m_distort_stats;

  std::vector<SpriteDistortVertex> m_sprite_distorter_vertices;
  std::vector<u32> m_sprite_distorter_indices;
  SpriteDistorterSetup m_sprite_distorter_setup;  // direct data
  math::Vector4f m_sprite_distorter_sine_tables_aspect;
  SpriteDistorterSineTables m_sprite_distorter_sine_tables;
  std::vector<SpriteDistortFrameData> m_sprite_distorter_frame_data;
  std::vector<SpriteDistortVertex> m_sprite_distorter_vertices_instanced;
  std::map<int, std::vector<SpriteDistortInstanceData>> m_sprite_distorter_instances_by_res;

  u64 m_sprite_direct_setup[3 * 16 / 8];
  SpriteFrameData m_frame_data;  // qwa: 980
  Sprite3DMatrixData m_3d_matrix_data;
  SpriteHudMatrixData m_hud_matrix_data;
  DirectRenderer m_direct;

  SpriteVecData2d m_vec_data_2d[SPRITES_PER_CHUNK];
  AdGifData m_adgif[SPRITES_PER_CHUNK];

  struct DebugStats {
    // per-frame tally of 2d sprites by (flag & 0x30) >> 4; jakx draws nonzero
    // classes without jak3's corner swap, suspected in the #65 black squares
    int flag_counts[4] = {0, 0, 0, 0};
    // per-bucket snapshot taken in flush_sprites before the bucket list clears,
    // so the debug window can finger which texture the black sprites use (#65)
    struct BucketDebug {
      int tbp = 0;
      int sprites = 0;
      float rgba[4] = {0, 0, 0, 0};
    };
    std::vector<BucketDebug> buckets;
    int blocks_2d_grp0 = 0;
    int count_2d_grp0 = 0;
    int blocks_2d_grp1 = 0;
    int count_2d_grp1 = 0;
    // per-frame count of sprites skipped for an all-zero adgif slot 4 (#145 amendment): an
    // unpopulated shader from a bring-up missing-art spawner, see sprite3_classify_adgif_slot4's
    // SKIP_ZERO case above for the full rationale
    int zero_adgif_skips = 0;
  } m_debug_stats;

  bool m_enable_distort_instancing = true;
  bool m_enable_culling = true;
  // #65 triage: skip 2d sprites whose (flag & 0x30) is nonzero, to isolate the
  // flagged classes jakx draws without jak3's corner swap
  bool m_hide_flagged_2d = false;
  bool m_enable_glow = true;

  bool m_2d_enable = true;
  bool m_3d_enable = true;
  bool m_distort_enable = true;

  struct SpriteVertex3D {
    math::Vector4f xyz_sx;              // position + x scale
    math::Vector4f quat_sy;             // quaternion + y scale
    math::Vector4f rgba;                // color
    math::Vector<u16, 2> flags_matrix;  // flags + matrix... split
    math::Vector<u16, 4> info;
    math::Vector<u8, 4> pad;
  };
  static_assert(sizeof(SpriteVertex3D) == 64);

  std::vector<SpriteVertex3D> m_vertices_3d;

  struct {
    GLuint vertex_buffer;
    GLuint vao;
    GLuint index_buffer;
  } m_ogl;

  DrawMode m_current_mode, m_default_mode;
  u32 m_current_tbp = 0;

  struct Bucket {
    std::vector<u32> ids;
    u32 offset_in_idx_buffer = 0;
    u64 key = -1;
  };

  std::map<u64, Bucket> m_sprite_buckets;
  std::vector<Bucket*> m_bucket_list;

  u64 m_last_bucket_key = UINT64_MAX;
  Bucket* m_last_bucket = nullptr;

  u64 m_sprite_idx = 0;

  std::vector<u32> m_index_buffer_data;
};
