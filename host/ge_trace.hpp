// GT_GE_LOG=<vblank>: logs every draw of the GE lists enqueued during that vblank (host/ge_trace.cpp)
// — texture address/format/size, CLUT, blending — to see what the software renderer is asked for.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>

namespace gt {

// Called for every sceGeListEnQueue with the current vblank count; logs only the wanted vblank.
void trace_ge_list(const psprecomp::GuestMemory &memory, std::uint32_t list, std::uint64_t vblank);

// F8 in gt_play: trace every list of the next vblank that submits one, with GT_GE_LOG_DETAIL's state, to
// ge_trace_vblank<N>.txt in the working directory. Safe to call from any thread.
void request_ge_trace() noexcept;

} // namespace gt
