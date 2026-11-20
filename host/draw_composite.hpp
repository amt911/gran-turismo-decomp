// CPU compositing of the mod draw layer and the scene grade onto an RGB image: what the frame dumps (GT_FRAME_DUMP) write, so the checks see
// what a mod drew. The windows composite the same layer on the GPU / in SDL (lcs patch 0071), from the same rasterized picture.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gt {

// Blends a straight-alpha RGBA picture (the same size) over the RGB image in place: dst = src * a + dst * (1 - a), rounded to nearest.
void blend_rgba_over_rgb(std::vector<std::uint8_t> &rgb, const std::uint8_t *rgba, std::uint32_t width, std::uint32_t height);

} // namespace gt
