// The keyboard side of the cheat device's hotkeys: the keys that went down since the last poll, as SDL scancode names (the
// names bindings.toml uses: "F9", "R", "Keypad 5") with the Ctrl/Shift/Alt held. Polled once per vblank from the pad read
// (host/play_backends.cpp). SDL's key state array is updated by the window thread; reading it here is the same benign race
// the reset-to-track mod had (a press seen one vblank late at worst).
#pragma once

#include "cheat_device/bindings.hpp"

#include <vector>

namespace gt::cheat_device {

std::vector<KeyBinding> keys_pressed_since_last_poll();
// The key of `binding` is down now with at least its modifiers (v6 action_held). False for an empty or unknown key name.
bool key_held(const KeyBinding &binding);

}  // namespace gt::cheat_device
