#include "mod_menu.hpp"

#include "mod_hooks.hpp"
#include "mod_manifest.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>

namespace gt::mods::menu {
namespace {

struct Row {
    ModEntry entry;
    bool save_failed = false;
};

std::mutex g_mutex;
std::vector<Row> g_rows;

}  // namespace

void set_entries(std::vector<ModEntry> entries) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_rows.clear();
    for (auto &e : entries) g_rows.push_back(Row{std::move(e), false});
}

std::size_t count() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_rows.empty() ? 1 : g_rows.size();
}

std::string row(std::size_t index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_rows.empty()) return index == 0 ? "NO MODS INSTALLED" : "";
    if (index >= g_rows.size()) return "";
    const Row &r = g_rows[index];
    std::string name = r.entry.name;
    if (r.entry.state == ModState::Error) {
        // "<name> ERROR <reason>", the name cut to 12 and the reason to what is left.
        if (name.size() > 12) name.resize(12);
        std::string out = name + " ERROR";
        if (!r.entry.reason.empty() && out.size() + 1 < kRowWidth) out += " " + r.entry.reason.substr(0, kRowWidth - out.size() - 1);
        return out;
    }
    std::string tail = r.entry.state == ModState::Active ? "ON" : "OFF";
    if (r.save_failed) tail += " SAVE FAILED";
    const std::size_t room = kRowWidth - 1 - tail.size();
    if (name.size() > room) name.resize(room);
    name.append(room - name.size(), ' ');
    return name + " " + tail;
}

bool toggle(std::size_t index) {
    // The flag flips under the lock; the file is written outside it (the renderer reads rows under the same lock every
    // frame, so disk I/O must not hold it). Saves are serialised and write the state current at save time.
    std::filesystem::path manifest;
    std::string id;
    bool turn_on = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index >= g_rows.size()) return false;
        Row &r = g_rows[index];
        if (r.entry.state == ModState::Error || r.entry.mod == nullptr) return false;
        turn_on = r.entry.state == ModState::Inactive;
        set_active(r.entry.mod, turn_on);
        r.entry.state = turn_on ? ModState::Active : ModState::Inactive;
        manifest = r.entry.manifest;
        id = r.entry.mod->id;
    }
    std::fprintf(stderr, "[mods] %s -> %s\n", id.c_str(), turn_on ? "ON" : "OFF");
    static std::mutex save_mutex;
    std::lock_guard<std::mutex> save(save_mutex);
    bool state_now = turn_on;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index < g_rows.size()) state_now = g_rows[index].entry.state == ModState::Active;
    }
    std::string error;
    const bool saved = write_enabled(manifest, state_now, error);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index < g_rows.size()) g_rows[index].save_failed = !saved;
    }
    if (!saved) std::fprintf(stderr, "[mods] %s: could not save mod.toml: %s\n", id.c_str(), error.c_str());
    return true;
}

void start_test_toggle_from_env() {
    const char *spec = std::getenv("GT_MOD_MENU_TEST_TOGGLE");
    if (spec == nullptr || *spec == '\0') return;
    const std::string text = spec;
    const std::size_t at = text.rfind('@');
    if (at == std::string::npos || at == 0) {
        std::fprintf(stderr, "[mods] GT_MOD_MENU_TEST_TOGGLE ignored: expected <mod id>@<seconds>\n");
        return;
    }
    const std::string id = text.substr(0, at);
    char *rest = nullptr;
    const double seconds = std::strtod(text.c_str() + at + 1, &rest);
    if (rest == text.c_str() + at + 1 || seconds < 0) {
        std::fprintf(stderr, "[mods] GT_MOD_MENU_TEST_TOGGLE ignored: bad seconds\n");
        return;
    }
    std::thread([id, seconds] {
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
        std::size_t index = static_cast<std::size_t>(-1);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (std::size_t i = 0; i < g_rows.size(); ++i)
                if (g_rows[i].entry.mod != nullptr && g_rows[i].entry.mod->id == id) index = i;
        }
        if (index == static_cast<std::size_t>(-1) || !toggle(index))
            std::fprintf(stderr, "[mods] test toggle: no toggleable mod '%s'\n", id.c_str());
    }).detach();
}

}  // namespace gt::mods::menu
