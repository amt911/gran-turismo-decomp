// gt_play's own host hooks: setting up lcs-recomp's real backends (MIT, compiled from the pinned
// checkout) in the order its LCSNative main uses, and the few parts of its host interface GT must not
// take from the LCS implementation. Design: docs/superpowers/specs/2026-10-10-playable-window-design.md.

#include "display_phase.hpp"
#include "frame_log.hpp"
#include "display_window.hpp"
#include "frontend.hpp"
#include "ge_gpu_backend.hpp"
#include "lcs_audio_output.hpp"
#include "lcs_lang.hpp"
#include "lcs_render_config.hpp"
#include "lcs_display_menu.hpp"
#include "mod_loader.hpp"
#include "mod_menu.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace lcs {

// lcs_lang.cpp patches LCS's text tables in guest memory: never on GT.
void lcs_install_lang(psprecomp::Runtime &) {}

} // namespace lcs

// Settings come from gt_play.ini next to the executable (config/gt_play.ini, copied by CMake;
// PSPRECOMP_CONFIG overrides; lcs-recomp's format).
void gt::frontend_init(const std::filesystem::path &executable_directory) {
    const std::string config = (executable_directory / "gt_play.ini").string();
    setenv("PSPRECOMP_CONFIG", config.c_str(), 0);
    const std::string saves = (executable_directory / "savedata").string();  // kept between sessions
    setenv("GT_SAVEDATA_DIR", saves.c_str(), 0);
    setenv("PSPRECOMP_WINDOW_TITLE", "Gran Turismo", 0);  // lcs-recomp's default names LCS (our patch 0016)
    setenv("GT_FRAMESKIP", "1", 0);  // keep the PSP's pace on slow CPUs (host/gt_kernel.cpp, frameskip_enabled)
    lcs::initialize_lcs_render_configuration(executable_directory);
    for (const std::string &warning : lcs::lcs_render_configuration().warnings)  // lcs-recomp keeps them silent
        std::cerr << "[config] " << warning << "\n";
    lcs::display_window_init();
    // The screen-side times (VK_KHR_present_wait, when the device has it): the vblank pacer locks to them
    // (host/display_phase.hpp), and GT_FRAME_LOG records them.
    lcs::ge_gpu_backend_set_display_hook([](std::uint64_t present_id) {
        gt::display_phase().on_shown(std::chrono::steady_clock::now());
        if (gt::frame_log() != nullptr) gt::frame_log()->add_display(gt::frame_log_now_us(), present_id);
    });
}

void gt::frontend_attach() {
    std::string error;
    if (!lcs::initialize_ge_gpu_backend(error)) std::cerr << "[ge] GPU backend unavailable: " << error << "\n";
    lcs::display_window_attach_gpu_backend();
}

void gt::frontend_shutdown() { lcs::audio_output_shutdown(); }

namespace {
// The MODS page of the host overlay (F9): rows and toggles come from the mod menu model.
void mods_page_row(std::size_t index, char *out, std::size_t capacity) {
    std::snprintf(out, capacity, "%s", gt::mods::menu::row(index).c_str());
}
const lcs::HostExtraPage kModsPage{"MODS", &gt::mods::menu::count, &mods_page_row, &gt::mods::menu::toggle};
} // namespace

void gt::frontend_load_mods(psprecomp::Runtime &runtime, const char *argv0, bool allowed, std::string &fatal) {
    if (!allowed) {
        std::cerr << "[mods] not loaded: --decomp diff and synthetic cases run the unmodded game\n";
        return;
    }
    const auto report = gt::mods::load_mods(runtime, gt::mods::mods_dir(argv0), fatal);
    gt::mods::menu::set_entries(report.entries);
    lcs::host_settings_set_extra_page(&kModsPage);
    gt::mods::menu::start_test_toggle_from_env();
    if (report.loaded.empty() && report.skipped.empty()) return;  // no mods directory, or nothing in it: say nothing
    std::cerr << "[mods] loaded:";
    for (const auto &id : report.loaded) std::cerr << ' ' << id;
    std::cerr << " skipped:";
    for (const auto &why : report.skipped) std::cerr << " [" << why << ']';
    std::cerr << "\n";
}
