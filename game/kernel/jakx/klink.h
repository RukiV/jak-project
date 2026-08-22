#pragma once

#include "common/common_types.h"

#include "game/kernel/common/Ptr.h"
#include "game/kernel/common/kmalloc.h"

namespace jakx {
// Distinguishes an OpenGOAL-toolchain object file (this repo's own goalc output,
// v3 link format) from an original retail object file (v5 link format, not yet
// decompiled). Exposed here so kdgo.cpp can make the same call link_control::
// jakx_begin makes, without duplicating the one-word sniff.
bool is_opengoal_object(void* data);

Ptr<uint8_t> link_and_exec(Ptr<uint8_t> data,
                           const char* name,
                           int32_t size,
                           Ptr<kheapinfo> heap,
                           uint32_t flags,
                           bool jump_from_c_to_goal);
u64 link_and_exec_wrapper(u64* args);
u32 link_busy();
void link_reset();
uint64_t link_begin(u64* args);
uint64_t link_resume();
}  // namespace jakx