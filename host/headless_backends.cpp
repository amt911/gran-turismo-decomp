// Headless implementations of the lcs-recomp host backends that lcs_profile.cpp / lcs_ge_exec.cpp
// call (window, input, audio output, GPU backend, software renderer, FPS overlay, LCS menu and
// language hooks). Phase 2a runs GT without graphics, audio or input; Phase 2b replaces these with
// real backends. Declarations come from the pinned lcs-recomp headers (MIT); bodies are ours.

#include "display_window.hpp"
#include "frontend.hpp"
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
#include <utility>
#include <vector>

namespace lcs {

// --- window and input: no window; never closed; analog stick centred; no buttons (scripted presses,
// GT_PAD, are added by the kernel for both executables: host/pad_script.cpp) ---
bool display_window_closed() { return false; }
HostInputState display_window_input() { return HostInputState{}; }
void display_window_pump() {}
// Frames are dumped (GT_FRAME_DUMP) by the kernel before it presents: host/frame_dump.cpp.
void display_window_present(psprecomp::Runtime &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                            std::uint32_t) {}
void display_window_present_rgba(std::span<const std::byte>, std::uint32_t, std::uint32_t) {}

// --- audio output: samples are produced by the HLE and dropped here ---
void audio_output_submit(std::span<const std::int16_t>, std::uint32_t, bool, std::uint32_t,
                         std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t) {}
void audio_output_advance(std::uint64_t) {}
void audio_output_reset_channel(std::uint32_t) {}
void audio_output_take_cuts(std::uint64_t &underruns, std::uint64_t &dropped_blocks) { underruns = dropped_blocks = 0u; }

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
void ge_gpu_backend_note_guest_write(std::uint32_t, std::uint32_t) noexcept {}
std::uint64_t ge_gpu_backend_write_clock() noexcept { return 0u; }
void ge_gpu_backend_finalize_texture_key(GeGpuDrawDescriptor &) noexcept {}
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
bool rendering_keep_temporal_jitter() noexcept { return true; }  // no GPU path here (GT patch 0048)
bool ge_gpu_backend_active() noexcept { return false; }
bool ge_gpu_backend_finish_color_frame(std::uint64_t) noexcept { return false; }
GeGpuPerf ge_gpu_backend_take_perf() noexcept { return {}; }
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

// Headless: no configuration, window, GPU backend or audio device to set up.
void gt::frontend_init(const std::filesystem::path &) {}
void gt::frontend_attach() {}
void gt::frontend_shutdown() {}
void gt::frontend_load_mods(psprecomp::Runtime &, const char *, bool) {}
