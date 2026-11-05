// Mod menu model: the list the in-game MODS page shows and the toggle it calls (host/play_backends.cpp registers it with the
// overlay). Toggling flips the mod's `active` gate at once and saves `enabled =` to its mod.toml. Thread-safe.
#pragma once

#include "mod_loader.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace gt::mods::menu {

constexpr std::size_t kRowWidth = 40;

// Replaces the list (the loader's report entries). Entries point at mods that live until the process ends.
void set_entries(std::vector<ModEntry> entries);

// Number of rows: one per mod, or 1 ("NO MODS INSTALLED") when there are none.
std::size_t count();

// `<name padded> ON|OFF|ERROR`, at most kRowWidth chars; Error rows carry the loader's reason cut to fit; a failed manifest
// write adds `SAVE FAILED`.
std::string row(std::size_t index);

// Flips mod `index` (Active <-> Inactive) and rewrites its manifest. The flag flips even if the write fails (the row then
// shows SAVE FAILED). False for an Error row, an out-of-range index or the "no mods" row. Logs `[mods] <id> -> ON|OFF`.
bool toggle(std::size_t index);

// `GT_MOD_MENU_TEST_TOGGLE=<mod id>@<seconds>` (verification only): a detached thread toggles that mod once after the delay.
void start_test_toggle_from_env();

} // namespace gt::mods::menu
