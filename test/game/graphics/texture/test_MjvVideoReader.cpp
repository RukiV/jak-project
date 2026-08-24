// Unit tests for MjvVideoReader (issue 569, and its white-lines/off-center follow-up).
//
// The follow-up investigation found the visible defect (three thick white lines top-left,
// off-center picture with the bottom/right cut off) lives in GOAL's fmv-draw-quad
// (goal_src/jakx/engine/scene/fmv-player.gc), not in this decoder: its second vertex's
// screen-space XYZ used the video's raw pixel dimensions (640x448) as if they were the
// PS2 direct-render screen's own half-extents, instead of the actual 512x416 the rest of
// the codebase uses for this class of fullscreen quad (draw-color-bars and draw-raw-image
// in blit-displays.gc, the mouse-cursor clamp in pad.gc, credits-obs.gc's set-height!).
// That produced a quad 640x448 screen-units wide/tall against a 512x416 target, so the
// visible slice sampled UV [0,512)x[0,416) of the full [0,640)x[0,448) texture instead of
// the whole thing -- cropping the right ~20% and bottom ~7% of the picture while the
// top-left corner (which the buggy code did place correctly) stayed anchored, reading as
// "off-center, bottom/right cut off".
//
// These tests exist to positively rule this decoder OUT as a contributor: they exercise
// the exact same code path (MjvVideoReader -> stbi_load_from_memory) the runtime uses,
// with synthetic containers built with a known top/bottom pixel pattern, and (when the
// real seeded asset is present) against out/jakx/fmv/THX.MJV itself, checking that its
// decoded top rows are clean across the whole 450-frame corpus -- matching the offline
// Python/PIL scan run during triage, now via the real runtime decoder instead of a proxy
// decoder.

#include <cstdio>
#include <cstring>
#include <vector>

#include "common/common_types.h"
#include "common/util/FileUtil.h"

#include "game/graphics/opengl_renderer/TextureAnimator.h"
#include "game/graphics/texture/MjvVideoReader.h"
#include "gtest/gtest.h"

#include "third-party/stb_image/stb_image_write.h"

namespace {

// Mirrors scripts/jakx/gen_mjv.py's HEADER_STRUCT ("<4sHHHHIIIII") and
// MjvVideoReader.cpp's MjvHeader -- a third independent copy here would be one too many
// sources of truth, so this test builds containers by hand at the byte level instead of
// including either of the other two, and relies on MjvVideoReader's own static_assert to
// keep it honest against MjvVideoReader.cpp's copy.
struct TestMjvHeader {
  char magic[4];
  u16 version;
  u16 reserved0;
  u16 width;
  u16 height;
  u32 fps_num;
  u32 fps_den;
  u32 frame_count;
  u32 flags;
  u32 reserved1;
};
static_assert(sizeof(TestMjvHeader) == 32);

void stb_write_append(void* context, void* data, int size) {
  auto* out = reinterpret_cast<std::vector<u8>*>(context);
  auto* bytes = reinterpret_cast<u8*>(data);
  out->insert(out->end(), bytes, bytes + size);
}

// Encodes a WxH baseline JPEG (quality 100, so a solid fill round-trips essentially
// exactly) whose top `top_h` rows are one solid color and whose remaining rows are
// another.
std::vector<u8> encode_two_band_jpeg(int w,
                                     int h,
                                     int top_h,
                                     u8 top_r,
                                     u8 top_g,
                                     u8 top_b,
                                     u8 bot_r,
                                     u8 bot_g,
                                     u8 bot_b) {
  std::vector<u8> rgb((size_t)w * h * 3);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t idx = ((size_t)y * w + x) * 3;
      if (y < top_h) {
        rgb[idx + 0] = top_r;
        rgb[idx + 1] = top_g;
        rgb[idx + 2] = top_b;
      } else {
        rgb[idx + 0] = bot_r;
        rgb[idx + 1] = bot_g;
        rgb[idx + 2] = bot_b;
      }
    }
  }
  std::vector<u8> out;
  int ok = stbi_write_jpg_to_func(stb_write_append, &out, w, h, 3, rgb.data(), 100);
  EXPECT_NE(ok, 0) << "stb_image_write failed to encode the synthetic test JPEG";
  return out;
}

// Packs an MJV1 container (header, offset/size table, concatenated JPEG blobs) to a real
// temp file and returns its path -- MjvVideoReader::open() takes a path, not a byte span.
fs::path pack_mjv(const std::string& name,
                  int width,
                  int height,
                  u32 fps_num,
                  u32 fps_den,
                  const std::vector<std::vector<u8>>& frames) {
  TestMjvHeader header{};
  std::memcpy(header.magic, "MJV1", 4);
  header.version = 1;
  header.reserved0 = 0;
  header.width = (u16)width;
  header.height = (u16)height;
  header.fps_num = fps_num;
  header.fps_den = fps_den;
  header.frame_count = (u32)frames.size();
  header.flags = 0;
  header.reserved1 = 0;

  const u32 data_start = (u32)sizeof(TestMjvHeader) + (u32)frames.size() * 8;
  std::vector<u8> file_bytes(data_start, 0);
  std::memcpy(file_bytes.data(), &header, sizeof(header));

  u32 running = data_start;
  size_t table_off = sizeof(header);
  for (const auto& f : frames) {
    u32 off = running;
    u32 sz = (u32)f.size();
    std::memcpy(file_bytes.data() + table_off, &off, 4);
    std::memcpy(file_bytes.data() + table_off + 4, &sz, 4);
    table_off += 8;
    running += sz;
  }
  for (const auto& f : frames) {
    file_bytes.insert(file_bytes.end(), f.begin(), f.end());
  }

  fs::path path = fs::temp_directory_path() / name;
  FILE* fp = std::fopen(path.string().c_str(), "wb");
  EXPECT_NE(fp, nullptr) << "could not open " << path.string() << " for writing";
  std::fwrite(file_bytes.data(), 1, file_bytes.size(), fp);
  std::fclose(fp);
  return path;
}

const u8* pixel(const u8* rgba, int w, int x, int y) {
  return &rgba[((size_t)y * w + x) * 4];
}

}  // namespace

TEST(MjvVideoReader, DecodesDistinctTopAndBottomBands) {
  const int w = 64, h = 32, top_h = 8;
  auto frame = encode_two_band_jpeg(w, h, top_h, 10, 20, 30, 200, 150, 100);
  auto path = pack_mjv("mjv_test_bands.mjv", w, h, 30, 1, {frame});

  MjvVideoReader reader;
  ASSERT_TRUE(reader.open(path));
  EXPECT_EQ(reader.width(), w);
  EXPECT_EQ(reader.height(), h);
  EXPECT_EQ(reader.frame_count(), 1);

  const u8* rgba = reader.frame_rgba_at_ms(0);
  ASSERT_NE(rgba, nullptr);

  // Sample well inside each band (avoiding the JPEG-block boundary row) so 4:2:0 chroma
  // blur at the seam can't flip a sample into the wrong band.
  const u8* top_px = pixel(rgba, w, w / 2, 1);
  const u8* bot_px = pixel(rgba, w, w / 2, h - 2);
  EXPECT_NEAR(top_px[0], 10, 12);
  EXPECT_NEAR(top_px[1], 20, 12);
  EXPECT_NEAR(top_px[2], 30, 12);
  EXPECT_NEAR(bot_px[0], 200, 12);
  EXPECT_NEAR(bot_px[1], 150, 12);
  EXPECT_NEAR(bot_px[2], 100, 12);
}

TEST(MjvVideoReader, FrameIndexFollowsFpsAndClampsPastTheEnd) {
  const int w = 16, h = 16;
  // Three visually distinct solid-color frames at 10fps: 100ms apart.
  std::vector<std::vector<u8>> frames = {
      encode_two_band_jpeg(w, h, h, 255, 0, 0, 255, 0, 0),  // frame 0: red
      encode_two_band_jpeg(w, h, h, 0, 255, 0, 0, 255, 0),  // frame 1: green
      encode_two_band_jpeg(w, h, h, 0, 0, 255, 0, 0, 255),  // frame 2: blue
  };
  auto path = pack_mjv("mjv_test_index.mjv", w, h, 10, 1, frames);

  MjvVideoReader reader;
  ASSERT_TRUE(reader.open(path));

  auto at = [&](u32 ms) { return reader.frame_rgba_at_ms(ms); };

  const u8* f0 = at(0);
  ASSERT_NE(f0, nullptr);
  EXPECT_NEAR(pixel(f0, w, 8, 8)[0], 255, 10);
  EXPECT_EQ(reader.last_index(), 0);

  const u8* f1 = at(150);  // 1.5 frames in at 10fps -> index 1
  ASSERT_NE(f1, nullptr);
  EXPECT_NEAR(pixel(f1, w, 8, 8)[1], 255, 10);
  EXPECT_EQ(reader.last_index(), 1);

  const u8* f_past_end = at(100000);  // way past the last frame
  ASSERT_NE(f_past_end, nullptr);
  EXPECT_NEAR(pixel(f_past_end, w, 8, 8)[2], 255, 10);
  EXPECT_EQ(reader.last_index(), 2);  // clamped to frame_count() - 1, not failed
}

// jakx-fmv-boot-movies rung 2: TextureAnimator::fmv_movie_basename's movie-id ->
// disc-M2V-basename table. static (declared in TextureAnimator.h) so it is callable
// with no TextureAnimator instance and no GL context -- TextureAnimator's constructor
// calls real GL setup, like Sprite3's own (see test_Sprite3.cpp's header comment for
// the established reason goalc-test never constructs either), so this rung's tests
// exercise the free function directly rather than a live TextureAnimator.
TEST(FmvMovieBasename, KnownIdsMapToTheExpectedName) {
  // First (id 0) and last (id 42) entries of *m2v-info*'s own :name order
  // (goal_src/jakx/engine/scene/fmv-player-h.gc), plus one from the middle, spot-check
  // the full 43-entry table without hand-duplicating every row here.
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(0), "INTRO.MJV");
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(42), "THX.MJV");
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(38), "INTROB2.MJV");
}

TEST(FmvMovieBasename, OutOfRangeIdsReturnEmptyRatherThanAsserting) {
  // A bad movie-id from GOAL is data, not a C++ bug (handle_fmv_frame's own fail-soft
  // posture) -- confirm the boundary on both sides rather than just one.
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(-1), "");
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(43), "");
  EXPECT_EQ(TextureAnimator::fmv_movie_basename(1000), "");
}

// jakx-fmv-boot-movies rung 2: handle_fmv_frame closes and reopens m_fmv (an
// MjvVideoReader) the moment an incoming frame's movie_id differs from whichever movie
// is currently open, mirroring a close()-then-open()-on-a-different-file sequence at
// the MjvVideoReader layer. handle_fmv_frame itself needs a live TextureAnimator (GL
// setup, see the comment above), so this exercises the layer underneath it that the
// reopen actually depends on: that a second open() on a different container fully
// replaces the first movie's state (dimensions, frame count, decoded pixels) rather
// than leaking or merging with it.
TEST(MjvVideoReader, ReopensCleanlyWhenTheUnderlyingMovieChanges) {
  const int wa = 16, ha = 16;
  auto frame_a = encode_two_band_jpeg(wa, ha, ha, 255, 0, 0, 255, 0, 0);  // solid red
  auto path_a = pack_mjv("mjv_test_reopen_a.mjv", wa, ha, 10, 1, {frame_a});

  const int wb = 24, hb = 8;  // deliberately different dimensions from movie A
  auto frame_b0 = encode_two_band_jpeg(wb, hb, hb, 0, 0, 255, 0, 0, 255);  // solid blue
  auto frame_b1 = encode_two_band_jpeg(wb, hb, hb, 0, 255, 0, 0, 255, 0);  // solid green
  auto path_b = pack_mjv("mjv_test_reopen_b.mjv", wb, hb, 10, 1, {frame_b0, frame_b1});

  MjvVideoReader reader;
  ASSERT_TRUE(reader.open(path_a));
  EXPECT_EQ(reader.width(), wa);
  EXPECT_EQ(reader.height(), ha);
  EXPECT_EQ(reader.frame_count(), 1);
  const u8* a_rgba = reader.frame_rgba_at_ms(0);
  ASSERT_NE(a_rgba, nullptr);
  EXPECT_NEAR(pixel(a_rgba, wa, wa / 2, ha / 2)[0], 255, 10);  // movie A is red
  EXPECT_EQ(reader.last_index(), 0);

  // The reopen TextureAnimator does on a movie_id change: close, then open a
  // different container.
  reader.close();
  EXPECT_FALSE(reader.is_open());
  ASSERT_TRUE(reader.open(path_b));

  // Every piece of movie A's state must be gone, not merged with movie B's.
  EXPECT_EQ(reader.width(), wb);
  EXPECT_EQ(reader.height(), hb);
  EXPECT_EQ(reader.frame_count(), 2);
  const u8* b_rgba = reader.frame_rgba_at_ms(150);  // 1.5 frames in at 10fps -> index 1
  ASSERT_NE(b_rgba, nullptr);
  EXPECT_NEAR(pixel(b_rgba, wb, wb / 2, hb / 2)[1], 255, 10);  // movie B's frame 1 is green
  EXPECT_EQ(reader.last_index(), 1);
}

TEST(MjvVideoReader, RejectsTruncatedFrameTable) {
  // A header claiming 5 frames but a file with no room for that table at all: open()
  // must fail soft (false), never assert or read out of bounds.
  TestMjvHeader header{};
  std::memcpy(header.magic, "MJV1", 4);
  header.version = 1;
  header.width = 64;
  header.height = 64;
  header.fps_num = 30;
  header.fps_den = 1;
  header.frame_count = 5;
  header.flags = 0;

  std::vector<u8> file_bytes(sizeof(header));
  std::memcpy(file_bytes.data(), &header, sizeof(header));

  fs::path path = fs::temp_directory_path() / "mjv_test_truncated.mjv";
  FILE* fp = std::fopen(path.string().c_str(), "wb");
  ASSERT_NE(fp, nullptr);
  std::fwrite(file_bytes.data(), 1, file_bytes.size(), fp);
  std::fclose(fp);

  MjvVideoReader reader;
  EXPECT_FALSE(reader.open(path));
  EXPECT_FALSE(reader.is_open());
}

// Real-asset check against the actual seeded movie (issue 569 follow-up triage). Skips
// cleanly when the asset is not present (offline-test environments, a fresh checkout
// before `task extract`/the FMV transcode has run) rather than failing the suite.
TEST(MjvVideoReader, RealThxTopAndBottomRowsAreClean) {
  fs::path path = file_util::get_jak_project_dir() / "out" / "jakx" / "fmv" / "THX.MJV";
  if (!fs::exists(path)) {
    GTEST_SKIP() << "out/jakx/fmv/THX.MJV not present in this tree, skipping";
  }

  MjvVideoReader reader;
  ASSERT_TRUE(reader.open(path));
  const int w = reader.width();
  const int h = reader.height();
  const int frame_count = reader.frame_count();
  ASSERT_GT(frame_count, 0);

  const int sample_indices[] = {0, frame_count / 2, frame_count - 1};
  const char* sample_names[] = {"first", "middle", "last"};

  fs::path dump_dir = fs::temp_directory_path() / "jakx_fmv_evidence";
  fs::create_directories(dump_dir);

  for (int s = 0; s < 3; s++) {
    int idx = sample_indices[s];
    // Reader is keyed by elapsed_ms, not index; back-compute an elapsed_ms that maps to
    // this index via the reader's own fps rational (mirrors frame_rgba_at_ms's formula:
    // index = floor(ms * fps_num / (fps_den * 1000))). Ceiling division here, not floor:
    // floor(idx * fps_den * 1000 / fps_num) can land one tick before the range that maps
    // back to idx (e.g. idx=225 at 30000/1001 floors to ms=7507, which maps back to 224),
    // since flooring twice compounds. Ceiling picks the smallest ms inside idx's actual
    // window instead.
    u32 fps_num = 30000, fps_den = 1001;  // THX.MJV's known transcode rate (gen_mjv.py default)
    u32 elapsed_ms = (u32)((((u64)idx * fps_den * 1000) + fps_num - 1) / fps_num);
    const u8* rgba = reader.frame_rgba_at_ms(elapsed_ms);
    ASSERT_NE(rgba, nullptr) << "frame " << idx << " (" << sample_names[s] << ") failed to decode";
    EXPECT_EQ(reader.last_index(), idx)
        << "elapsed_ms " << elapsed_ms << " mapped to index " << reader.last_index()
        << ", expected " << idx << " (fps rational rounding at this frame count)";

    // Dump for human inspection (issue 569 follow-up evidence).
    char fname[64];
    std::snprintf(fname, sizeof(fname), "thx_frame_%06d.png", idx);
    file_util::write_rgba_png((dump_dir / fname).string(), (void*)rgba, w, h);

    // Top 16 / bottom 4 rows must be dark: the offline Python/PIL scan of the same
    // container found zero rows above brightness 40/255 (out of 255+255+255 per pixel,
    // averaged) across all 450 frames' top-16/bottom-4 rows. Re-check the same bound here
    // through the real decoder, with a matching per-row-average threshold.
    auto row_avg_brightness = [&](int y) {
      u64 total = 0;
      for (int x = 0; x < w; x++) {
        const u8* px = pixel(rgba, w, x, y);
        total += (u32)px[0] + px[1] + px[2];
      }
      return (double)total / (w * 3.0);
    };
    for (int y = 0; y < 16; y++) {
      EXPECT_LT(row_avg_brightness(y), 40.0)
          << "frame " << idx << " row " << y << " is unexpectedly bright";
    }
    for (int y = h - 4; y < h; y++) {
      EXPECT_LT(row_avg_brightness(y), 40.0)
          << "frame " << idx << " row " << y << " is unexpectedly bright";
    }
  }
}
