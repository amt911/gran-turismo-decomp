// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N): write every Nth presented guest frame as a PPM.
#pragma once

#include "psprecomp/runtime.hpp"

#include <cstdint>

namespace gt {

void frame_dump(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height);

} // namespace gt
