// GT_GE_LOG=<vblank>: logs every draw of the GE lists enqueued during that vblank (host/ge_trace.cpp)
// — texture address/format/size, CLUT, blending — to see what the software renderer is asked for.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>

namespace gt {

// Called for every sceGeListEnQueue with the current vblank count; logs only the wanted vblank.
void trace_ge_list(const psprecomp::GuestMemory &memory, std::uint32_t list, std::uint64_t vblank);

} // namespace gt
