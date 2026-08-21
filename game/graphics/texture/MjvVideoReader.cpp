#include "MjvVideoReader.h"

#include <cstring>

#include "common/log/log.h"
#include "common/util/Timer.h"

// STB_IMAGE_IMPLEMENTATION is already defined once, in third-party/stb_image/stb_image.cpp,
// which is linked into the runtime target (game/CMakeLists.txt) -- this file only needs the
// declarations.
#include "third-party/stb_image/stb_image.h"

namespace {
// Mirrors scripts/jakx/gen_mjv.py's HEADER_STRUCT ("<4sHHHHIIIII") field-for-field. Natural
// x86-64 struct layout already matches that little-endian, no-padding layout (every
// multi-byte field lands on its own alignment boundary by construction, per that script's
// header comment), so no #pragma pack is needed; the static_assert below is what actually
// guarantees it rather than trusting the reasoning.
struct MjvHeader {
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
static_assert(sizeof(MjvHeader) == 32);

constexpr int kMaxDimension = 2048;  // matches gen_mjv.py's MAX_DIMENSION
}  // namespace

bool MjvVideoReader::open(const fs::path& path) {
  close();

  if (!fs::exists(path)) {
    lg::warn("[fmv] {} not found", path.string());
    return false;
  }

  std::vector<u8> file;
  try {
    file = file_util::read_binary_file(path);
  } catch (const std::exception& e) {
    lg::warn("[fmv] {} could not be read: {}", path.string(), e.what());
    return false;
  }

  if (file.size() < sizeof(MjvHeader)) {
    lg::warn("[fmv] {} is too short for an MJV1 header ({} B)", path.string(), file.size());
    return false;
  }

  MjvHeader header;
  memcpy(&header, file.data(), sizeof(MjvHeader));

  if (memcmp(header.magic, "MJV1", 4) != 0) {
    lg::warn("[fmv] {} has the wrong magic", path.string());
    return false;
  }
  if (header.version != 1) {
    lg::warn("[fmv] {} has unsupported version {}", path.string(), header.version);
    return false;
  }
  if (header.flags != 0) {
    lg::warn("[fmv] {} has unsupported flags 0x{:x}", path.string(), header.flags);
    return false;
  }
  if (header.width == 0 || header.width > kMaxDimension || header.height == 0 ||
      header.height > kMaxDimension) {
    lg::warn("[fmv] {} has invalid dimensions {}x{}", path.string(), header.width, header.height);
    return false;
  }
  if (header.frame_count == 0) {
    lg::warn("[fmv] {} has zero frames", path.string());
    return false;
  }
  if (header.fps_num == 0 || header.fps_den == 0) {
    lg::warn("[fmv] {} has an invalid fps rational {}/{}", path.string(), header.fps_num,
             header.fps_den);
    return false;
  }

  const u64 table_bytes = (u64)header.frame_count * 8;
  const u64 data_start = sizeof(MjvHeader) + table_bytes;
  if (data_start > file.size()) {
    lg::warn("[fmv] {} header + frame table ({} B) exceeds file size ({} B)", path.string(),
             data_start, file.size());
    return false;
  }

  std::vector<FrameEntry> frames;
  frames.reserve(header.frame_count);
  const u8* table = file.data() + sizeof(MjvHeader);
  for (u32 i = 0; i < header.frame_count; i++) {
    u32 off, size;
    memcpy(&off, table + (size_t)i * 8 + 0, 4);
    memcpy(&size, table + (size_t)i * 8 + 4, 4);
    if (off < data_start || (u64)off + size > file.size()) {
      lg::warn("[fmv] {} frame {} is out of bounds (offset {} size {}, file is {} B)",
               path.string(), i, off, size, file.size());
      return false;
    }
    frames.push_back({off, size});
  }

  m_file = std::move(file);
  m_frames = std::move(frames);
  m_width = header.width;
  m_height = header.height;
  m_fps_num = header.fps_num;
  m_fps_den = header.fps_den;
  m_decoded_index = -1;
  m_rgba.assign((size_t)m_width * m_height * 4, 0);
  m_open = true;
  return true;
}

void MjvVideoReader::close() {
  m_file.clear();
  m_file.shrink_to_fit();
  m_frames.clear();
  m_frames.shrink_to_fit();
  m_rgba.clear();
  m_rgba.shrink_to_fit();
  m_open = false;
  m_width = 0;
  m_height = 0;
  m_fps_num = 0;
  m_fps_den = 0;
  m_decoded_index = -1;
}

const u8* MjvVideoReader::frame_rgba_at_ms(u32 elapsed_ms) {
  if (!m_open) {
    return nullptr;
  }

  // index = clamp(ms * fps_num / (fps_den * 1000), 0, frame_count - 1), fmv-design.md
  // section 4. fps_num/fps_den are both nonzero and frame_count > 0 here: open() rejects
  // any container that fails those checks.
  u64 index = ((u64)elapsed_ms * m_fps_num) / ((u64)m_fps_den * 1000);
  if (index >= (u64)m_frames.size()) {
    index = m_frames.size() - 1;
  }

  if (!decode_index((int)index)) {
    return nullptr;
  }
  return m_rgba.data();
}

bool MjvVideoReader::decode_index(int index) {
  if (index == m_decoded_index) {
    return true;  // decode-if-changed cache
  }

  const auto& entry = m_frames[index];
  Timer decode_timer;
  int w, h, comp;
  u8* decoded =
      stbi_load_from_memory(m_file.data() + entry.offset, (int)entry.size, &w, &h, &comp, 4);
  double decode_us = decode_timer.getUs();
  if (!decoded) {
    lg::warn("[fmv] frame {} failed to decode: {}", index, stbi_failure_reason());
    return false;
  }
  if (w != m_width || h != m_height) {
    lg::warn("[fmv] frame {} decoded to {}x{}, expected {}x{}", index, w, h, m_width, m_height);
    stbi_image_free(decoded);
    return false;
  }

  memcpy(m_rgba.data(), decoded, m_rgba.size());
  stbi_image_free(decoded);
  m_decoded_index = index;
  // Instrumentation for issue 569 L2 acceptance (f): the real stb_image decode time,
  // checked against the design's proxy estimate and its 3ms threading threshold.
  lg::info("[fmv] decode frame {} took {:.1f} us", index, decode_us);
  return true;
}
