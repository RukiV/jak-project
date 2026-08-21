#pragma once

#include <vector>

#include "common/common_types.h"
#include "common/util/FileUtil.h"

/*!
 * Runtime reader for the MJV1 FMV container produced offline by
 * scripts/jakx/gen_mjv.py (issue 569, Road B lane L0). See that script's top-of-file
 * comment for the authoritative container spec (32-byte header, frame_count * 8-byte
 * offset/size table, concatenated baseline-JPEG blobs); this class is the runtime
 * mirror of it, not a second source of truth.
 *
 * Stateless with respect to playback time: the GOAL fmv-player process owns the clock
 * and asks for a frame by elapsed milliseconds every displayed frame (texture-anim
 * code 87, TextureAnimator::handle_fmv_frame). This class only owns the file bytes,
 * the frame table, and a one-frame RGBA8 decode cache.
 *
 * Every validation failure -- missing file, bad magic/version/flags, invalid
 * dimensions, a truncated or out-of-bounds frame table, a corrupt JPEG blob -- fails
 * soft (open() returns false, frame_rgba_at_ms() returns nullptr) and never asserts:
 * a bad or missing movie file is user/build data, not a game bug (fmv-design.md
 * section 4).
 *
 * Threading: none in v1. U4's proxy measurement (a comparable single-threaded
 * baseline-JPEG decoder) put decode time 150-190x under the ~33ms frame budget, so
 * decoding synchronously on the render thread avoids a whole class of torn-frame bugs
 * for a cost that should not matter. handle_fmv_frame instruments the real stb_image
 * decode time (see MjvVideoReader.cpp) so that estimate is checked against measurement
 * rather than trusted; the API here is shaped so that adding a decode thread later
 * would be internal to this class alone.
 */
class MjvVideoReader {
 public:
  bool open(const fs::path& path);
  void close();
  bool is_open() const { return m_open; }

  int width() const { return m_width; }
  int height() const { return m_height; }
  int frame_count() const { return (int)m_frames.size(); }

  // Maps elapsed playback time to a frame index using the container's own fps
  // rational, decodes that frame's JPEG blob to RGBA8 (decode-if-changed: a repeat
  // request for the same index reuses the last decode instead of re-running
  // stb_image), and returns a pointer to the w*h*4 decode buffer. Past the last
  // frame, clamps to frame_count() - 1 rather than failing -- the source's own last
  // frame is measured flat black, so an overrun just holds an invisible frame.
  // Returns nullptr if the reader is not open or the frame failed to decode.
  const u8* frame_rgba_at_ms(u32 elapsed_ms);
  int last_index() const { return m_decoded_index; }

 private:
  struct FrameEntry {
    u32 offset;
    u32 size;
  };

  bool decode_index(int index);

  std::vector<u8> m_file;            // whole container, held for the life of open()
  std::vector<FrameEntry> m_frames;  // parsed offset/size table
  std::vector<u8> m_rgba;            // one w*h*4 decode buffer

  bool m_open = false;
  int m_width = 0;
  int m_height = 0;
  u32 m_fps_num = 0;
  u32 m_fps_den = 0;
  int m_decoded_index = -1;  // decode-if-changed cache; -1 = nothing decoded yet
};
