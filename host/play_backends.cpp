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
#include "host_hooks.hpp"
#include "mod_loader.hpp"
#include "mod_services.hpp"
#include "mod_menu.hpp"
#include "pad_menu.hpp"
#include "pad_script.hpp"

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
const lcs::HostExtraPage kModsPage{"MODS", &gt::mods::menu::count, &mods_page_row, &gt::mods::menu::toggle, &gt::mods::menu::adjust};

// The pad's way into the page (host/pad_menu.hpp): runs inside the game's pad read, once per vblank for the edges.
lcs::HostSettingsKey overlay_key(gt::MenuKey key) {
    switch (key) {
    case gt::MenuKey::TogglePage: return lcs::HostSettingsKey::TogglePage;
    case gt::MenuKey::Up: return lcs::HostSettingsKey::Up;
    case gt::MenuKey::Down: return lcs::HostSettingsKey::Down;
    case gt::MenuKey::Left: return lcs::HostSettingsKey::Left;
    case gt::MenuKey::Right: return lcs::HostSettingsKey::Right;
    case gt::MenuKey::Activate: return lcs::HostSettingsKey::Activate;
    case gt::MenuKey::Close: return lcs::HostSettingsKey::Close;
    }
    return lcs::HostSettingsKey::Close;
}
const char *key_name(gt::MenuKey key) {
    static const char *const names[] = {"chord", "up", "down", "left", "right", "activate", "close"};
    return names[static_cast<int>(key)];
}

// GT_PAD_MENU_CHORD (default SELECT+START, names as GT_PAD uses them) changes the chord; a bad value is reported and ignored.
gt::PadMenuConfig pad_menu_config() {
    gt::PadMenuConfig config;
    if (const char *text = std::getenv("GT_PAD_MENU_CHORD"); text != nullptr && *text != '\0') {
        std::string error;
        if (const std::uint32_t mask = gt::parse_pad_chord(text, error); mask != 0) config.chord = mask;
        else std::cerr << "[mods] GT_PAD_MENU_CHORD ignored: " << error << "\n";
    }
#ifdef GT_PAD_MENU_SABOTAGE_IGNORE_CHORD  // verification only (tools/mod_cheats_check.sh --sabotage-chord must FAIL)
    config.ignore_chord = true;
#endif
    return config;
}

void pad_menu_filter(std::uint64_t vblank, std::uint32_t &buttons, std::uint8_t &analog_x, std::uint8_t &analog_y) {
    static gt::PadMenuFilter filter(pad_menu_config());
    static std::uint64_t polled = ~std::uint64_t{0};
    if (vblank != polled) {  // the pad is read by several threads per vblank: the edges count once
        polled = vblank;
        const bool was_open = lcs::host_settings_open();
        const gt::PadMenuKeys keys = filter.poll(buttons, was_open);
        for (std::size_t i = 0; i < keys.count; ++i) {
            std::cerr << "[padmenu] " << key_name(keys.keys[i]) << " at vblank " << vblank << "\n";
            lcs::host_settings_handle(overlay_key(keys.keys[i]));
        }
        if (const bool open = lcs::host_settings_open(); open != was_open)
            std::cerr << "[padmenu] menu " << (open ? "opened" : "closed") << " at vblank " << vblank << "\n";
    }
    const bool open = lcs::host_settings_open();
    buttons = filter.visible(buttons, open);
    if (open) analog_x = analog_y = 128u;  // centred stick: the game sees nothing
}

void mods_vblank_tick(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx, std::uint64_t) {
    gt::mods::services::run_vblank_tick(runtime, ctx);
}
} // namespace

void gt::frontend_load_mods(psprecomp::Runtime &runtime, const char *argv0, bool allowed) {
    if (!allowed) {
        std::cerr << "[mods] not loaded: --decomp diff and synthetic cases run the unmodded game\n";
        return;
    }
    const auto report = gt::mods::load_mods(runtime, gt::mods::mods_dir(argv0));
    gt::mods::menu::set_entries(report.entries);
    lcs::host_settings_set_extra_page(&kModsPage);
    if (!report.entries.empty()) {  // no mods installed (or GT_NO_MODS): the pad and the vblank are the game's, untouched
        gt::set_pad_filter(&pad_menu_filter);
        gt::set_vblank_tick(&mods_vblank_tick);
    }
    gt::mods::menu::start_test_toggle_from_env(&gt::pad_script_vblanks);
    if (report.loaded.empty() && report.skipped.empty()) return;  // no mods directory, or nothing in it: say nothing
    std::cerr << "[mods] loaded:";
    for (const auto &id : report.loaded) std::cerr << ' ' << id;
    std::cerr << " skipped:";
    for (const auto &why : report.skipped) std::cerr << " [" << why << ']';
    std::cerr << "\n";
}
