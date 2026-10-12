// Frame dumps for both executables (GT_FRAME_DUMP, GT_GPU_DUMP), taken from the guest display framebuffer the
// kernel is about to present (host/gt_kernel.cpp).
#include "frame_dump.hpp"

#include "image_ppm.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace gt {

// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N, default 60): every Nth presented frame is written as
// <dir>/frame_NNNNNN.ppm from the guest display framebuffer. The images are game output: keep the
// directory out of git (recomp/ is ignored).
namespace {

std::uint64_t dump_every() {
    static const std::uint64_t every = [] {
        const char *text = std::getenv("GT_FRAME_EVERY");
        const auto value = text != nullptr ? std::strtoull(text, nullptr, 10) : 60ull;
        return value == 0u ? 1ull : value;
    }();
    return every;
}

// The guest framebuffer as RGB (formats: 0 565, 1 5551, 2 4444, 3 8888); empty if out of RAM.
std::vector<std::uint8_t> guest_rgb(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                                    std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height) {
    if (width == 0u || height == 0u || buffer_width == 0u) return {};
    const std::uint32_t bytes_per_pixel = pixel_format == 3u ? 4u : 2u;
    auto &memory = runtime.memory();
    if (!memory.contains(frame_buffer, static_cast<std::size_t>(buffer_width) * height * bytes_per_pixel)) return {};
    std::vector<std::uint8_t> rgb;
    rgb.reserve(static_cast<std::size_t>(width) * height * 3u);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t address = frame_buffer + (y * buffer_width + x) * bytes_per_pixel;
            std::uint32_t r = 0, g = 0, b = 0;
            if (pixel_format == 3u) {
                const std::uint32_t p = memory.load32(address);
                r = p & 255u; g = (p >> 8u) & 255u; b = (p >> 16u) & 255u;
            } else {
                const std::uint32_t p = memory.load16(address);
                if (pixel_format == 0u) { r = (p & 31u) << 3u; g = ((p >> 5u) & 63u) << 2u; b = ((p >> 11u) & 31u) << 3u; }
                else if (pixel_format == 1u) { r = (p & 31u) << 3u; g = ((p >> 5u) & 31u) << 3u; b = ((p >> 10u) & 31u) << 3u; }
                else { r = (p & 15u) << 4u; g = ((p >> 4u) & 15u) << 4u; b = ((p >> 8u) & 15u) << 4u; }
            }
            rgb.push_back(static_cast<std::uint8_t>(r));
            rgb.push_back(static_cast<std::uint8_t>(g));
            rgb.push_back(static_cast<std::uint8_t>(b));
        }
    }
    return rgb;
}

std::filesystem::path numbered(const char *directory, const char *prefix, std::uint64_t index) {
    char name[40];
    std::snprintf(name, sizeof name, "%s_%06llu.ppm", prefix, static_cast<unsigned long long>(index));
    return std::filesystem::path(directory) / name;
}

} // namespace

// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N, default 60): every Nth presented frame is written as
// <dir>/frame_NNNNNN.ppm from the guest display framebuffer. The images are game output: keep the
// directory out of git (recomp/ is ignored).
void frame_dump(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height) {
    static const char *dump_dir = std::getenv("GT_FRAME_DUMP");
    static std::uint64_t presented = 0u;
    if (dump_dir == nullptr || (presented++ % dump_every()) != 0u) return;
    const std::vector<std::uint8_t> rgb = guest_rgb(runtime, frame_buffer, buffer_width, pixel_format, width, height);
    if (rgb.empty()) return;
    std::filesystem::create_directories(dump_dir);
    (void)write_rgb_ppm(numbered(dump_dir, "frame", presented - 1u), rgb, width, height);
}

// GT_GPU_DUMP=<dir> (+ GT_FRAME_EVERY): for the GPU gate (tools/gpu_gate.sh). Every Nth frame the GPU
// backend finished, soft_NNNNNN.ppm (the software rasterizer's guest framebuffer — run with
// PSPRECOMP_GE_GPU_SKIP_SOFTWARE_RASTER=0 so it keeps drawing) and gpu_NNNNNN.ppm (the backend's frame,
// box-downsampled to the same size) from the same frame. An empty `gpu_rgba` writes the soft image only,
// so tools/gpu_compare.py reports the GPU frame as missing. Game output: keep it out of git.
void gpu_frame_dump(psprecomp::Runtime &runtime, std::span<const std::byte> gpu_rgba, std::uint32_t gpu_width,
                    std::uint32_t gpu_height, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                    std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height) {
    static const char *dump_dir = std::getenv("GT_GPU_DUMP");
    static std::uint64_t frames = 0u;
    if (dump_dir == nullptr || (frames++ % dump_every()) != 0u) return;
    const std::vector<std::uint8_t> soft = guest_rgb(runtime, frame_buffer, buffer_width, pixel_format, width, height);
    if (soft.empty()) return;
    std::filesystem::create_directories(dump_dir);
    (void)write_rgb_ppm(numbered(dump_dir, "soft", frames - 1u), soft, width, height);
    const std::vector<std::uint8_t> gpu = downsample_rgba_box(gpu_rgba, gpu_width, gpu_height, width, height);
    if (!gpu.empty()) (void)write_rgb_ppm(numbered(dump_dir, "gpu", frames - 1u), gpu, width, height);
}

} // namespace gt
