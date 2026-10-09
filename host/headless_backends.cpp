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

#include <thread>

namespace lcs {

// --- window and input: no window; never closed; no buttons pressed, analog stick centred ---
bool display_window_closed() { return false; }
HostInputState display_window_input() { return {}; }
void display_window_pump() {}
void display_window_present(psprecomp::Runtime &, std::uint32_t, std::uint32_t, std::uint32_t,
                            std::uint32_t, std::uint32_t) {}
void display_window_present_rgba(std::span<const std::byte>, std::uint32_t, std::uint32_t) {}

// --- audio output: samples are produced by the HLE and dropped here ---
void audio_output_submit(std::span<const std::int16_t>, std::uint32_t, bool, std::uint32_t,
                         std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t) {}
void audio_output_advance(std::uint64_t) {}
void audio_output_reset_channel(std::uint32_t) {}

// --- GPU backend: none active, so the profile never waits on GPU frames ---
bool ge_gpu_backend_active() noexcept { return false; }
bool ge_gpu_backend_finish_color_frame(std::uint64_t) noexcept { return false; }
std::span<const std::byte> ge_gpu_backend_game_frame_rgba() noexcept { return {}; }
void ge_gpu_backend_mark_window_presented() noexcept {}
bool ge_gpu_backend_presents_directly() noexcept { return false; }
GeGpuBackendReport ge_gpu_backend_report() { return {}; }
void ge_gpu_backend_set_display_framebuffer(std::uint32_t, std::uint32_t, std::uint32_t) noexcept {}

// --- software renderer: GE lists are still walked (lcs_ge_exec.cpp) so FINISH/SIGNAL and sync
// behave as on hardware; primitives are accepted and not rasterised ---
void update_ge_transform_state(GeTransformState &, std::uint32_t, std::uint32_t) noexcept {}
bool render_ge_primitive(psprecomp::GuestMemory &, const std::array<std::uint32_t, 256> &,
                         const GeTransformState &, std::uint32_t, std::uint32_t, std::uint32_t,
                         GeRenderStats &, std::string &, std::uint32_t, std::uint64_t, std::uint64_t,
                         std::uint64_t) {
    return true;
}
void flush_ge_deferred_rasterization(psprecomp::GuestMemory &) {}

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
