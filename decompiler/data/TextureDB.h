#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "common/custom_data/Tfrag3Data.h"
#include "common/util/FileUtil.h"

namespace decompiler {
struct ResolvedTextureData {
  u16 w;
  u16 h;
  std::vector<u32> rgba;
};

struct TextureDB {
  TextureDB();
  struct TextureData {
    u16 w, h;
    std::string name;
    u32 page;
    u32 dest = -1;
    std::vector<u32> rgba_bytes;
    u32 num_mips = -1;
  };

  std::map<u32, TextureData> textures;
  std::unordered_map<u32, std::string> tpage_names;
  std::unordered_map<std::string, std::set<u32>> texture_ids_per_level;
  std::optional<fs::path> merge_texture_dir;
  std::optional<fs::path> replace_texture_dir;

  // special textures for animation.
  std::map<u32, tfrag3::IndexTexture> index_textures_by_combo_id;

  std::unordered_map<std::string, u32> animated_tex_output_to_anim_slot;

  ResolvedTextureData resolve_texture(u32 id) const;

  // Looks up an animated-texture-output slot, trying the tpage-qualified key
  // ("tpage-name/texture-name") first and falling back to the bare texture name. jakx qualifies
  // some slot names because a debug_name can repeat across several tpages where only one of
  // them should redirect to the anim slot (menu2-pris/iscreen-video-dest vs the same bare name
  // in rustyh-alpha, rustyh-vis-alpha and garageb-alpha, issue 762); jak2/jak3's tables are
  // bare-name only and always match on the fallback, unchanged from before.
  std::optional<u32> lookup_animated_tex_output_slot(const std::string& tpage_name,
                                                     const std::string& name) const;

  static constexpr int kPlaceholderWhiteTexturePage = INT16_MAX;
  static constexpr int kPlaceholderWhiteTextureId = 0;

  void add_texture(u32 tpage,
                   u32 texid,
                   const std::vector<u32>& data,
                   u16 w,
                   u16 h,
                   const std::string& tex_name,
                   const std::string& tpage_name,
                   const std::vector<std::string>& level_names,
                   u32 num_mips,
                   u32 dest);

  void add_index_texture(u32 tpage,
                         u32 texid,
                         const std::vector<u8>& index_data,
                         const std::array<math::Vector4<u8>, 256>& clut,
                         u16 w,
                         u16 h,
                         const std::string& tex_name,
                         const std::string& tpage_name,
                         const std::vector<std::string>& level_names);

  void merge_textures(const fs::path& base_path);
  void replace_textures(const fs::path& path);
  void merge_texture(u32 id, std::vector<u32>& rgba) const;
  std::optional<ResolvedTextureData> replace_texture(u32 id) const;

  std::string generate_texture_dest_adjustment_table() const;
};

// used by decompiler for texture macros
struct TexInfo {
  std::string name;
  std::string tpage_name;
  u32 idx;
};
}  // namespace decompiler
