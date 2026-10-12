// Small image helpers for the frame dumps (host/frame_dump.cpp): a box downsample of an RGBA frame
// and a binary PPM writer. No runtime dependency, so host/tests/image_ppm_test.cpp tests them alone.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace gt {

// Box-downsamples an RGBA frame (width x height) to RGB out_width x out_height; empty if rgba is too short.
std::vector<std::uint8_t> downsample_rgba_box(std::span<const std::byte> rgba, std::uint32_t width, std::uint32_t height,
                                              std::uint32_t out_width, std::uint32_t out_height);

// Writes a binary P6 PPM; false on a size mismatch or an IO error.
bool write_rgb_ppm(const std::filesystem::path &path, std::span<const std::uint8_t> rgb, std::uint32_t width,
                   std::uint32_t height);

} // namespace gt
