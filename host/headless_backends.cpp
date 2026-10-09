// Headless implementations of the lcs-recomp host backends that lcs_profile.cpp / lcs_ge_exec.cpp
// call (window, input, audio output, GPU backend, software renderer, FPS overlay, LCS menu and
// language hooks). Phase 2a runs GT without graphics, audio or input; Phase 2b replaces these with
// real backends. Declarations come from the pinned lcs-recomp headers (MIT); bodies are ours.

#include "display_window.hpp"
#include "ge_gpu_backend.hpp"
#include "ge_renderer.hpp"
#include "lcs_audio_output.hpp"
#include "lcs_fps_overlay.hpp"
#include "lcs_frame_limit.hpp"
#include "lcs_lang.hpp"
#include "lcs_menu.hpp"
#include "lcs_render_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace lcs {

// --- window and input: no window; never closed; no buttons pressed, analog stick centred ---
bool display_window_closed() { return false; }
HostInputState display_window_input() { return {}; }
void display_window_pump() {}
// GT_FRAME_DUMP=<dir> (+ GT_FRAME_EVERY=N, default 60): every Nth presented frame is written as
// <dir>/frame_NNNNNN.ppm from the guest display framebuffer. The images are game output: keep the
// directory out of git (recomp/ is ignored).
void display_window_present(psprecomp::Runtime &runtime, std::uint32_t frame_buffer, std::uint32_t buffer_width,
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
void display_window_present_rgba(std::span<const std::byte>, std::uint32_t, std::uint32_t) {}

// --- audio output: samples are produced by the HLE and dropped here ---
void audio_output_submit(std::span<const std::int16_t>, std::uint32_t, bool, std::uint32_t,
                         std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t) {}
void audio_output_advance(std::uint64_t) {}
void audio_output_reset_channel(std::uint32_t) {}

// --- GPU backend: none active, so the software rasterizer (ge_renderer.cpp) draws into guest VRAM
// and the profile never waits on GPU frames. Every hook answers "no GPU" ---
void ge_gpu_backend_accumulate_color_triangles(const GeGpuDrawDescriptor &, std::span<const GeGpuVertex>) noexcept {}
bool ge_gpu_backend_accumulate_hardware_packed_0115(const GeGpuDrawDescriptor &, const GeGpuHardwareTransform &,
                                                    std::span<const std::byte>, std::uint32_t,
                                                    std::span<const std::uint32_t>) noexcept { return false; }
void ge_gpu_backend_accumulate_hardware_triangles(const GeGpuDrawDescriptor &, const GeGpuHardwareTransform &,
                                                  std::span<const GeGpuVertex>, std::span<const std::uint32_t>) noexcept {}
bool ge_gpu_backend_adopt_shared_texture(const GeGpuDrawDescriptor &) noexcept { return false; }
std::uint32_t ge_gpu_backend_display_framebuffer() noexcept { return 0u; }
void ge_gpu_backend_display_logical_size(std::uint32_t &width, std::uint32_t &height) noexcept { width = 480u; height = 272u; }
bool ge_gpu_backend_graphics_ready() noexcept { return false; }
bool ge_gpu_backend_is_framebuffer_feedback_texture(const GeGpuDrawDescriptor &) noexcept { return false; }
void ge_gpu_backend_note_through_extent(const GeGpuDrawDescriptor &, float, float) noexcept {}
void ge_gpu_backend_observe_camera(const std::array<float, 12> &, const std::array<float, 16> &,
                                   const std::array<float, 6> &, const std::array<float, 3> &,
                                   const GeGpuDrawDescriptor &, std::uint32_t) noexcept {}
std::uint32_t ge_gpu_backend_owned_framebuffer() noexcept { return 0u; }
void ge_gpu_backend_prepare_texture_keys(GeGpuDrawDescriptor &) noexcept {}
void ge_gpu_backend_record_draw(const GeGpuDrawDescriptor &) noexcept {}
bool ge_gpu_backend_stage_vertices(const GeGpuDrawDescriptor &, std::span<const GeGpuVertex>) noexcept { return false; }
bool ge_gpu_backend_texture_available(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_texture_needed(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_texture_signature_needed(const GeGpuDrawDescriptor &) noexcept { return false; }
bool ge_gpu_backend_transfer_ready() noexcept { return false; }
bool ge_gpu_backend_upload_decoded_texture_chain_packed(const GeGpuDrawDescriptor &, std::uint32_t, std::uint32_t,
                                                        std::uint32_t, std::vector<std::byte>) noexcept { return false; }
bool ge_gpu_backend_upload_decoded_texture(const GeGpuDrawDescriptor &, std::uint32_t, std::uint32_t,
                                           std::span<const std::byte>) noexcept { return false; }
GeGpuWidescreenHud ge_gpu_backend_widescreen_hud(const GeGpuDrawDescriptor &) noexcept { return {}; }
void fps_overlay_observe_draw(const GeGpuDrawDescriptor &, std::uint32_t) noexcept {}
bool lcs_menu_command() { return false; }
// lcs_render_config.cpp needs SDL; headless uses the default configuration.
const LcsConfiguration &lcs_render_configuration() {
    static const LcsConfiguration configuration{};
    return configuration;
}
bool ge_gpu_backend_active() noexcept { return false; }
bool ge_gpu_backend_finish_color_frame(std::uint64_t) noexcept { return false; }
std::span<const std::byte> ge_gpu_backend_game_frame_rgba() noexcept { return {}; }
void ge_gpu_backend_mark_window_presented() noexcept {}
bool ge_gpu_backend_presents_directly() noexcept { return false; }
GeGpuBackendReport ge_gpu_backend_report() { return {}; }
void ge_gpu_backend_set_display_framebuffer(std::uint32_t, std::uint32_t, std::uint32_t) noexcept {}

// --- overlay and LCS-only hooks: nothing to draw; the LCS language patches must NOT touch GT ---
void fps_overlay_render_frame(std::uint32_t, std::uint32_t, std::uint32_t) noexcept {}
void lcs_install_lang(psprecomp::Runtime &) {}
void lcs_menu_init(psprecomp::Runtime &) {}
void lcs_menu_enqueue() {}
void lcs_set_ge_command(std::uint32_t) {}

// --- timing: real sleeps, so vblank pacing and guest time stay meaningful ---
void precise_sleep_until(std::chrono::steady_clock::time_point deadline) noexcept {
    std::this_thread::sleep_until(deadline);
}

} // namespace lcs
