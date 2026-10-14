// Frame dumps for both executables (GT_FRAME_DUMP, GT_GPU_DUMP), taken from the guest display framebuffer the
// kernel is about to present (host/gt_kernel.cpp).
#include "frame_dump.hpp"

#include "image_ppm.hpp"
#include "pad_script.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
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
// <dir>/frame_NNNNNN.ppm from the guest display framebuffer, and stderr says at which vblank: GT_PAD times
// count vblank waits, which run ahead of presented frames during loads. The images are game output: keep the
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
    std::fprintf(stderr, "[frame-dump] frame %06llu vblank %llu\n", static_cast<unsigned long long>(presented - 1u),
                 static_cast<unsigned long long>(pad_script_vblanks()));
}

// GT_GPU_DUMP=<dir> (+ GT_FRAME_EVERY, GT_GPU_DUMP_FROM=N to start at frame N, GT_GPU_DUMP_FULL=1 to keep the GPU
// frame at its internal resolution — tools/gpu_compare.py then can't pair it): for the GPU gate (tools/gpu_gate.sh).
// Every Nth frame the GPU backend finished, gpu_NNNNNN.ppm (the backend's frame, box-downsampled to the display size) and
// soft_NNNNNN.ppm (the software rasterizer's guest framebuffer for the SAME frame — run with
// PSPRECOMP_GE_GPU_DUAL=1, lcs-recomp patch 0020, so it keeps drawing). The backend hands back a frame
// one present late (it completes the previous submit, then submits the current one), so the software
// image is kept for one call and paired with the next GPU frame. An empty `gpu_rgba` (a frame the GPU never
// drew, `gpu_ready` false while the frame is new — `not_new` false) writes the software image alone, which tools/gpu_compare.py reports as missing. Game output:
// keep it out of git.
void gpu_frame_dump(psprecomp::Runtime &runtime, std::span<const std::byte> gpu_rgba, std::uint32_t gpu_width,
                    std::uint32_t gpu_height, std::uint32_t frame_buffer, std::uint32_t buffer_width,
                    std::uint32_t pixel_format, std::uint32_t width, std::uint32_t height, bool gpu_ready,
                    std::uint64_t vblank, std::uint64_t gpu_vblank) {
    static const char *dump_dir = std::getenv("GT_GPU_DUMP");
    static std::uint64_t frames = 0u;
    struct Captured {
        std::vector<std::uint8_t> soft;
        std::uint32_t width{}, height{};
        std::uint64_t number{};
    };
    static std::map<std::uint64_t, Captured> pending;  // software frames waiting for their GPU twin, by vblank
    if (dump_dir == nullptr || !gpu_ready) return;
    // Called on every present that closes a GPU frame (the game flipped). The software framebuffer shown
    // now is that frame; the GPU image arrives a present or more later (readback), tagged with the vblank
    // it was closed at, and is paired with the software frame kept for that vblank.
    static const std::uint64_t dump_from = [] {  // GT_GPU_DUMP_FROM=N: nothing before frame N (consecutive frames)
        const char *text = std::getenv("GT_GPU_DUMP_FROM");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    if (frames < dump_from) {
        ++frames;
        return;
    }
    if ((frames++ % dump_every()) == 0u)
        pending[vblank] = {guest_rgb(runtime, frame_buffer, buffer_width, pixel_format, width, height), width,
                           height, frames - 1u};
    if (!gpu_rgba.empty()) {
        if (const auto twin = pending.find(gpu_vblank); twin != pending.end()) {
            const Captured &c = twin->second;
            std::filesystem::create_directories(dump_dir);
            (void)write_rgb_ppm(numbered(dump_dir, "soft", c.number), c.soft, c.width, c.height);
            static const bool full = std::getenv("GT_GPU_DUMP_FULL") != nullptr;  // the internal resolution, not 480x272
            const std::uint32_t out_width = full ? gpu_width : c.width, out_height = full ? gpu_height : c.height;
            const std::vector<std::uint8_t> gpu = downsample_rgba_box(gpu_rgba, gpu_width, gpu_height, out_width, out_height);
            if (!gpu.empty()) (void)write_rgb_ppm(numbered(dump_dir, "gpu", c.number), gpu, out_width, out_height);
            std::fprintf(stderr, "[gpu-dump] frame %06llu vblank %llu\n", static_cast<unsigned long long>(c.number),
                         static_cast<unsigned long long>(gpu_vblank));  // GT_GE_LOG=<vblank> traces that frame
        }
        pending.erase(pending.begin(), pending.upper_bound(gpu_vblank));  // older ones will never come
    }
    while (pending.size() > 8u) pending.erase(pending.begin());
}

} // namespace gt
