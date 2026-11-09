// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N): write every Nth presented guest frame as a PPM;
// GT_GPU_DUMP=<dir>: the GPU gate's paired frames.
#pragma once

#include "psprecomp/runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace gt {

// present_vblank: the vblank the present was requested at (the log line carries it: the label `vblank` is read when the GE worker dumps, which
// lags the game under load, so only present_vblank pairs the same game frame in two runs).
void frame_dump(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height, std::uint64_t present_vblank);

// GT_GPU_DUMP=<dir>: paired soft_/gpu_ frames for the GPU gate (see frame_dump.cpp).
void gpu_frame_dump(psprecomp::Runtime &runtime, std::span<const std::byte> gpu_rgba, std::uint32_t gpu_width,
                    std::uint32_t gpu_height, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                    std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height, bool gpu_ready,
                    std::uint64_t vblank, std::uint64_t gpu_vblank);

} // namespace gt
