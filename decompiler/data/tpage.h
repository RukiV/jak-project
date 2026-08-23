#pragma once
#include <string>
#include <unordered_set>
#include <vector>

#include "decompiler/data/TextureDB.h"

namespace decompiler {
struct ObjectFileData;

struct TPageResultStats {
  int total_textures = 0;
  int successful_textures = 0;
  int num_px = 0;
};

TPageResultStats process_tpage(ObjectFileData& data,
                               TextureDB& texture_db,
                               const fs::path& output_path,
                               const std::unordered_set<std::string>& animated_textures,
                               bool save_pngs);

// Same as process_tpage, but for a texture-page GOAL object that isn't its own standalone
// tpage-* object file: jakx embeds its car texture pages directly inside the car's art-group
// object instead (see level.gc's jakx-only art-group tpage registration branch). word_offset is
// the index of the embedded texture-page's type tag word within data's segment 0, as found by
// find_embedded_tpages.
TPageResultStats process_embedded_tpage(ObjectFileData& data,
                                        TextureDB& texture_db,
                                        const fs::path& output_path,
                                        const std::unordered_set<std::string>& animated_textures,
                                        bool save_pngs,
                                        int word_offset);

// Find texture-page objects embedded in data's art-group(s), by walking each art-group's
// element array the same way level.gc's jakx-only art-group tpage registration branch does at
// runtime. Returns the word_offset of each one, suitable for process_embedded_tpage.
std::vector<int> find_embedded_tpages(ObjectFileData& data);
}  // namespace decompiler
