// gt_play's own host hooks: setting up lcs-recomp's real backends (MIT, compiled from the pinned
// checkout) in the order its LCSNative main uses, and the few parts of its host interface GT must not
// take from the LCS implementation. Design: docs/superpowers/specs/2026-10-10-playable-window-design.md.

#include "display_phase.hpp"
#include "frame_log.hpp"
#include "ge_trace.hpp"
#ifdef GT_MOD_DOWNLOADER
#include "mod_store.hpp"
#endif
#include "display_window.hpp"
#include "frontend.hpp"
#include "ge_gpu_backend.hpp"
#include "lcs_audio_output.hpp"
#include "lcs_lang.hpp"
#include "lcs_render_config.hpp"
#include "lcs_display_menu.hpp"
#include "cheat_device/device.hpp"
#include "cheat_device/keyboard_sdl.hpp"
#include "host_hooks.hpp"
#include "mod_loader.hpp"
#include "mod_services.hpp"
#include "mod_menu.hpp"
#include "pad_menu.hpp"
#include "pad_script.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <cstdlib>
#include <thread>
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
    lcs::host_set_debug_key_handler(&gt::request_ge_trace);  // F8: trace the next frame's GE lists to a file (host/ge_trace.hpp)
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

// The cheat device (host/cheat_device/device.hpp, spec 2026-11-11-cheat-device-design.md) is the F9 page: a delegated page
// (lcs patch 0067) whose keys and view are the device's. Its Mods category is the old F9 MODS list (host/mod_menu.hpp).
namespace device_ns = gt::cheat_device;
device_ns::Device &device() {
    static device_ns::Device d(device_ns::ModRows{&gt::mods::menu::count, &gt::mods::menu::row, &gt::mods::menu::adjust});
    return d;
}
std::uint64_t device_now() { return gt::pad_script_vblanks(); }
gt::MenuKey menu_key(lcs::HostSettingsKey key) {
    switch (key) {
    case lcs::HostSettingsKey::Up: return gt::MenuKey::Up;
    case lcs::HostSettingsKey::Down: return gt::MenuKey::Down;
    case lcs::HostSettingsKey::Left: return gt::MenuKey::Left;
    case lcs::HostSettingsKey::Right: return gt::MenuKey::Right;
    case lcs::HostSettingsKey::Activate: return gt::MenuKey::Activate;
    default: return gt::MenuKey::Close;
    }
}
bool device_page_key(lcs::HostSettingsKey key) { return device().handle_key(menu_key(key), device_now()); }
void device_page_view(lcs::HostSettingsView &view) {
    const device_ns::View v = device().view(static_cast<std::size_t>(lcs::kViewRows), device_now());
    std::snprintf(view.title, sizeof view.title, "%s", v.title.c_str());
    std::snprintf(view.footer, sizeof view.footer, "%s", v.footer.c_str());
    view.row_count = static_cast<int>(v.rows.size());
    view.selected = v.selected;
    for (std::size_t i = 0; i < v.rows.size(); ++i) std::snprintf(view.rows[i], sizeof view.rows[i], "%s", v.rows[i].c_str());
}
// With the pad filter installed (a v2 mod is loaded) the host reads the keyboard each vblank and owns the open key; without it
// the overlay keeps F9 as before (the device's open key cannot be rebound then: there is no other action to clash with).
const lcs::HostExtraPage kDevicePage{"CHEATS", nullptr, nullptr, nullptr, nullptr, &device_page_key, &device_page_view, true};
const lcs::HostExtraPage kDevicePageOverlayF9{"CHEATS", nullptr, nullptr, nullptr, nullptr, &device_page_key, &device_page_view, false};

void toggle_device() {
    if (!lcs::host_settings_open()) device().open();
    lcs::host_settings_handle(lcs::HostSettingsKey::TogglePage);
}

// Actions fired by a hotkey or from the menu run on the next vblank tick (mod API v4, host/mod_services.hpp).
void fire_action(const std::string &id) {
    std::cerr << "[device] action " << id << " at vblank " << device_now() << "\n";
    gt::mods::services::queue_action(id);
}

// The mods' v4 entries, then their actions (a category lists its rows first, its actions after), go into the device; a default hotkey that does not parse, or that another action holds,
// leaves the action without it (logged, never "last one wins").
void declare_mod_actions() {
    namespace services = gt::mods::services;
    const auto &entries = services::entries();
    for (std::size_t i = 0; i < entries.size(); ++i)
        device().add_entry(entries[i].category,
                           device_ns::Entry{entries[i].mod->id + "." + entries[i].id, device_ns::EntryKind::Row, "",
                                            [i] {
                                                char text[device_ns::kWidth + 1];
                                                return services::entry_text(i, text, sizeof text) ? std::string(text) : std::string();
                                            },
                                            [i](int direction) { services::entry_activate(i, direction); },
                                            {}});
    for (const services::ActionDecl &a : services::actions()) {
        std::string error;
        const auto key = device_ns::parse_key(a.key, error);
        if (!key) std::cerr << "[device] " << a.id << ": default key '" << a.key << "' ignored: " << error << "\n";
        error.clear();
        const auto pad = device_ns::parse_pad(a.pad, error);
        if (!pad) std::cerr << "[device] " << a.id << ": default pad '" << a.pad << "' ignored: " << error << "\n";
        error.clear();
        if (!device().add_action(a.id, a.label, a.category, key.value_or(device_ns::KeyBinding{}), pad.value_or(device_ns::PadBinding{}),
                                 false, error) ||
            !error.empty())
            std::cerr << "[device] " << a.id << ": " << error << "\n";
    }
}

// The keyboard hotkeys and the key capture of the Controls page, once per vblank.
void poll_device_keyboard(std::uint64_t vblank) {
    for (const device_ns::KeyBinding &key : device_ns::keys_pressed_since_last_poll()) {
        if (device().capturing()) {
            // Return / Escape drive the menu itself (Return started the capture, Escape cancels it): never captured.
            if (key.key == "Return" || key.key == "Escape" || key.key == "Keypad Enter") continue;
            if (device().capture_key(key, vblank)) continue;
        }
        const std::optional<std::string> id = device().action_for_key(key);
        if (!id) continue;
        if (*id == device_ns::kOpenAction) toggle_device();
        else if (!lcs::host_settings_open()) fire_action(*id);
    }
}

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
    // GT_PAD_MENU_HOLD_BACK=<vblanks> (default 8; 0 = off) is for the identity checks: a run with the pad menu installed must be compared with a run without it,
    // and the hold-back delays a lone Select/Start by design (docs/MODS.md); everything else in those runs is the mod's own effect.
    if (const char *text = std::getenv("GT_PAD_MENU_HOLD_BACK"); text != nullptr && *text != '\0') config.hold_back = static_cast<unsigned>(std::strtoul(text, nullptr, 10));
#ifdef GT_PAD_MENU_SABOTAGE_IGNORE_CHORD  // verification only (tools/mod_cheats_check.sh --sabotage-chord must FAIL)
    config.ignore_chord = true;
#endif
#ifdef GT_PAD_MENU_SABOTAGE_NO_REPLAY  // verification only (tools/mod_reset_check.sh --tap --sabotage-notap with an API-4 mod must FAIL)
    config.no_replay = true;
#endif
    return config;
}

// GT_PAD_LOG=1: one line per press and release of Select, the camera button that reset-to-track's hold binding shares ("PADSEL down
// step=<vblank>", "PADSEL up step=<vblank> tap=<0|1>": tap = the game saw the press at all, late or not). tools/mod_reset_check.py reads
// them: a held Select that fired the reset must be hidden (tap=0), a short press must reach the game (tap=1).
void log_select_press(const gt::PadMenuFilter &filter, std::uint32_t raw, std::uint64_t vblank) {
    static const bool log = std::getenv("GT_PAD_LOG") != nullptr;
    static bool down = false, seen = false;
    if (!log) return;
    const bool now = (raw & gt::kPadSelect) != 0;
    if (now && !down) {
        seen = false;
        std::cerr << "[padmenu] PADSEL down step=" << vblank << "\n";
    }
    seen = seen || (filter.visible(raw, lcs::host_settings_open()) & gt::kPadSelect) != 0;  // the release poll carries the late tap
    if (!now && down) std::cerr << "[padmenu] PADSEL up step=" << vblank << " tap=" << (seen ? 1 : 0) << "\n";
    down = now;
}

// Plain statics: guest threads are scheduled cooperatively on one OS thread, so the pad reads never overlap.
void pad_menu_filter(std::uint64_t vblank, std::uint32_t &buttons, std::uint8_t &analog_x, std::uint8_t &analog_y) {
    static gt::PadMenuFilter filter(pad_menu_config());
    static std::uint64_t polled = ~std::uint64_t{0};
    if (vblank != polled) {  // the pad is read by several threads per vblank: the edges count once
        polled = vblank;
        if (std::getenv("GT_PAD_MENU_CHORD") == nullptr) filter.set_chord(device().pad_binding(device_ns::kOpenAction).buttons);
        static std::vector<gt::PadHotkeyMask> hotkeys;  // the other bindings' buttons are held back like the chord's
        if (std::vector<gt::PadHotkeyMask> now = device().pad_hotkeys(); now != hotkeys) {
            hotkeys = now;
            filter.set_hotkeys(std::move(now));
        }
        const bool was_open = lcs::host_settings_open();
        const gt::PadMenuKeys keys = filter.poll(buttons, was_open);
        if (device().capturing()) {
            device().poll_pad(buttons, vblank);  // the Controls page captures a combination: the buttons are not menu keys
        } else {
            for (std::size_t i = 0; i < keys.count; ++i) {
                std::cerr << "[padmenu] " << key_name(keys.keys[i]) << " at vblank " << vblank << "\n";
                if (keys.keys[i] == gt::MenuKey::TogglePage && !was_open) device().open();
                lcs::host_settings_handle(overlay_key(keys.keys[i]));
            }
        }
        poll_device_keyboard(vblank);
        if (!lcs::host_settings_open())
            for (const std::string &id : device().poll_pad_hotkeys(buttons))
                if (id != device_ns::kOpenAction) fire_action(id);  // the chord above opens the device
        if (const bool open = lcs::host_settings_open(); open != was_open)
            std::cerr << "[padmenu] menu " << (open ? "opened" : "closed") << " at vblank " << vblank << "\n";
        log_select_press(filter, buttons, vblank);
    }
    const bool open = lcs::host_settings_open();
#ifdef GT_PAD_MENU_SABOTAGE_NO_HIDE  // verification only (tools/mod_cheats_check.sh --sabotage-hide must FAIL): the game keeps seeing the pad
    return;
#endif
    buttons = filter.visible(buttons, open);
    if (open) analog_x = analog_y = 128u;  // centred stick: the game sees nothing
}

void mods_vblank_tick(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx, std::uint64_t vblank) {
    gt::mods::services::run_vblank_tick(runtime, ctx, vblank);
}
bool overlay_open() { return lcs::host_settings_open(); }
} // namespace

void gt::frontend_load_mods(psprecomp::Runtime &runtime, const char *argv0, bool allowed) {
    if (!allowed) {
        std::cerr << "[mods] not loaded: --decomp diff and synthetic cases run the unmodded game\n";
        return;
    }
    const auto report = gt::mods::load_mods(runtime, gt::mods::mods_dir(argv0));
    gt::mods::menu::set_entries(report.entries);
    {
        declare_mod_actions();
        const std::filesystem::path bindings = gt::mods::mods_dir(argv0).parent_path() / "bindings.toml";
        for (const std::string &warning : device().use_bindings_file(bindings)) std::cerr << "[device] " << warning << "\n";
        device().set_fire(&fire_action);
        device().set_log([](const std::string &line) { std::cerr << "[device] " << line << " at vblank " << device_now() << "\n"; });
        lcs::host_settings_set_extra_page(gt::mods::services::any_registered() ? &kDevicePage : &kDevicePageOverlayF9);
    }
#ifdef GT_MOD_DOWNLOADER
    // GET MODS rows on the same page (host/mod_store.hpp). GT_MODS_GET_TEST=<id> (verification only): a detached thread
    // checks the index and installs that mod as Enter would, logging `[mods] get <id>: <row>`.
    gt::mods::store::configure(gt::mods::mods_dir(argv0));
    if (const char *id = std::getenv("GT_MODS_GET_TEST"); id != nullptr && *id != '\0') {
        std::thread([wanted = std::string(id)] {
            namespace store = gt::mods::store;
            store::activate(0);
            store::wait_idle();
            std::cerr << "[mods] get index: " << store::row(0, 60) << "\n";
            const std::size_t row = store::row_of(wanted);
            if (row != 0) {
                store::activate(row);
                store::wait_idle();
            }
            std::cerr << "[mods] get " << wanted << ": " << (row != 0 ? store::row(row, 200) : "not in the index") << "\n";
        }).detach();
    }
#endif
    gt::mods::services::set_menu_open_query(&overlay_open);
    // Only when a loaded mod registered menu rows or a tick (API v2; a switched-off one counts: the pad chord is how it is switched on):
    // with no such mod (none installed, GT_NO_MODS, only API-1 mods) the pad and the vblank are the game's, untouched.
    if (gt::mods::services::any_registered()) {
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
