// Cheat device (spec docs/superpowers/specs/2026-11-11-cheat-device-design.md): the menu tree built from the host and the
// mods, the actions and their bindings, and the capture of new bindings from the Controls page. Pure logic (no SDL, no lcs,
// no guest memory): host/play_backends.cpp feeds it keys, pad samples and the vblank count, draws its view in the lcs overlay
// (a delegated page, patch 0067) and runs the actions it fires. One lock: the window thread (keyboard), a guest thread (pad)
// and the render thread (view) call it.
#pragma once

#include "cheat_device/bindings.hpp"
#include "cheat_device/menu.hpp"
#include "pad_menu.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace gt::cheat_device {

// The categories under the root, in this order; a category with no entry is not shown (Mods and Controls always are).
inline const char *const kCategories[] = {"Race", "Car", "Money & garage", "Camera", "Mods", "Controls", "Settings"};
constexpr const char *kOpenAction = "device.open";
constexpr std::uint64_t kMessageVblanks = 180;  // a refused binding's reason stays 3 s in the title line
constexpr unsigned kCancelHold = 30;           // CIRCLE held this many vblanks cancels a pad capture
constexpr std::size_t kWidth = 40;             // characters of a row

// The F9 MODS list as it is today (host/mod_menu.hpp): the device shows it as the Mods category.
struct ModRows {
    std::function<std::size_t()> count;
    std::function<std::string(std::size_t)> row;
    std::function<bool(std::size_t, int)> adjust;
};

struct View {
    std::string title, footer;
    std::vector<std::string> rows;
    int selected = 0;
};

class Device {
public:
    explicit Device(ModRows mods);

    // Declares an action (BindingTable::add_action) and shows it as an Action entry in `category`; Enter there, or its hotkey,
    // calls the fire callback with its id. A clashing default is not an error here: the action is declared unbound and
    // `error` says which action holds the binding (the caller logs it). False only for a bad or duplicate id.
    bool add_action(const std::string &id, const std::string &label, const std::string &category, KeyBinding key, PadBinding pad,
                    bool required, std::string &error);
    void add_entry(const std::string &category, Entry entry);  // an entry a mod or the host adds (toggle, choice, row...)
    void set_fire(std::function<void(const std::string &)> fire);
    // A line per binding made or refused from Controls ("bound <id> pad L+TRIANGLE", "refused <id> key R: ..."); the in-game checks read it.
    void set_log(std::function<void(const std::string &)> log);
    // bindings.toml: loaded now (warnings returned) and saved after every change made from the Controls page.
    std::vector<std::string> use_bindings_file(const std::filesystem::path &path);

    // The overlay opened the device: the tree is rebuilt (mods' lists may have changed); the first open lands on Mods.
    void open();
    // Up/Down/Left/Right/Activate/Close; false when the device closes (Close at the root).
    bool handle_key(MenuKey key, std::uint64_t now);
    View view(std::size_t rows, std::uint64_t now);  // builds the tree on the first call if open() was not called

    // Captures: a pad sample each vblank, a keyboard key when pressed. True when a capture took the input.
    bool capturing() const;
    void poll_pad(std::uint32_t raw, std::uint64_t now);
    bool capture_key(const KeyBinding &key, std::uint64_t now);

    // Hotkeys outside the menu: the action a key fires, and the actions a pad sample fires (PadHotkeys).
    std::optional<std::string> action_for_key(const KeyBinding &key) const;
    std::vector<std::string> poll_pad_hotkeys(std::uint32_t raw);
    std::uint32_t bound_pad_buttons() const;
    std::vector<PadHotkeyMask> pad_hotkeys() const;  // every pad binding but the open combination (the filter's chord)  // union of every pad binding (the host holds them back from the game)
    std::string binding_text(const std::string &id) const;  // "F9 / SELECT+START" (keyboard / pad; "-" for none)
    PadBinding pad_binding(const std::string &id) const;    // {} for an unknown id

private:
    void rebuild_locked();
    void open_locked();
    Entry controls_locked();
    void say_locked(std::string message, std::uint64_t now);
    void changed_locked(std::uint64_t now);
    std::string binding_text_locked(const std::string &id) const;

    mutable std::mutex mutex_;
    ModRows mods_;
    BindingTable bindings_;
    PadHotkeys hotkeys_;
    std::vector<std::pair<std::string, Entry>> entries_;  // category, entry
    std::vector<std::pair<std::string, std::string>> action_category_;  // action id, category
    std::function<void(const std::string &)> fire_, log_;
    std::uint32_t capture_ignored_ = 0;  // buttons held when the capture started (the CROSS that started it): ignored until released
    bool capture_fresh_ = false;
    std::filesystem::path bindings_path_;
    Navigator nav_{Entry{"root", EntryKind::Submenu, "Cheats", {}, {}, {}}};
    bool opened_once_ = false;
    std::string message_;
    std::uint64_t message_until_ = 0, now_ = 0;
    // A capture in progress: which action and which kind (keyboard or pad).
    std::string capture_id_;
    bool capture_pad_ = false;
    std::optional<PadCapture> pad_capture_;
    unsigned cancel_held_ = 0;
};

}  // namespace gt::cheat_device
