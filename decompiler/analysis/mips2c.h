#pragma once

#include <string>
#include <vector>

#include "common/versions/versions.h"

namespace decompiler {
class Function;

void run_mips2c(Function* f, GameVersion version);
void run_mips2c_jump_table(Function* f,
                           const std::vector<int>& jump_table_locations,
                           GameVersion version);

// Convert a GOAL symbol name to a valid C++ variable name. Exposed for unit testing.
std::string goal_to_c_name(const std::string& name);
}  // namespace decompiler
