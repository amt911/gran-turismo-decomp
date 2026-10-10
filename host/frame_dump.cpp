// Frame dumps for both executables (GT_FRAME_DUMP), taken from the guest display framebuffer the
// kernel is about to present (host/gt_kernel.cpp).
#include "frame_dump.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace gt {

// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N, default 60): every Nth presented frame is written as
// <dir>/frame_NNNNNN.ppm from the guest display framebuffer. The images are game output: keep the
// directory out of git (recomp/ is ignored).
void frame_dump(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height) {
    static const char *dump_dir = std::getenv("GT_FRAME_DUMP");
    static const std::uint64_t every = [] {
        const char *text = std::getenv("GT_FRAME_EVERY");
        const auto value = text != nullptr ? std::strtoull(text, nullptr, 10) : 60ull;
        return value == 0u ? 1ull : value;
    }();
    static std::uint64_t presented = 0u;
    if (dump_dir == nullptr || (presented++ % every) != 0u) return;
    if (width == 0u || height == 0u || buffer_width == 0u) return;
    const std::uint32_t bytes_per_pixel = pixel_format == 3u ? 4u : 2u;  // 0 565, 1 5551, 2 4444, 3 8888
    auto &memory = runtime.memory();
    if (!memory.contains(frame_buffer, static_cast<std::size_t>(buffer_width) * height * bytes_per_pixel)) return;
    std::filesystem::create_directories(dump_dir);
    char name[32];
    std::snprintf(name, sizeof name, "frame_%06llu.ppm", static_cast<unsigned long long>(presented - 1u));
    std::FILE *out = std::fopen((std::filesystem::path(dump_dir) / name).c_str(), "wb");
    if (out == nullptr) return;
    std::fprintf(out, "P6\n%u %u\n255\n", width, height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t address = frame_buffer + (y * buffer_width + x) * bytes_per_pixel;
            std::uint8_t rgb[3];
            if (pixel_format == 3u) {
                const std::uint32_t p = memory.load32(address);
                rgb[0] = static_cast<std::uint8_t>(p); rgb[1] = static_cast<std::uint8_t>(p >> 8u);
                rgb[2] = static_cast<std::uint8_t>(p >> 16u);
            } else {
                const std::uint32_t p = memory.load16(address);
                std::uint32_t r = 0, g = 0, b = 0;
                if (pixel_format == 0u) { r = (p & 31u) << 3u; g = ((p >> 5u) & 63u) << 2u; b = ((p >> 11u) & 31u) << 3u; }
                else if (pixel_format == 1u) { r = (p & 31u) << 3u; g = ((p >> 5u) & 31u) << 3u; b = ((p >> 10u) & 31u) << 3u; }
                else { r = (p & 15u) << 4u; g = ((p >> 4u) & 15u) << 4u; b = ((p >> 8u) & 15u) << 4u; }
                rgb[0] = static_cast<std::uint8_t>(r); rgb[1] = static_cast<std::uint8_t>(g); rgb[2] = static_cast<std::uint8_t>(b);
            }
            std::fwrite(rgb, 1u, 3u, out);
        }
    }
    std::fclose(out);
}

} // namespace gt
