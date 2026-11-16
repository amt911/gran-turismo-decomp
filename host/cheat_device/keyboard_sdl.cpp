#include "cheat_device/keyboard_sdl.hpp"

#include <SDL.h>

#include <array>

namespace gt::cheat_device {

std::vector<KeyBinding> keys_pressed_since_last_poll() {
    static std::array<bool, SDL_NUM_SCANCODES> was{};
    int count = 0;
    const Uint8 *state = SDL_GetKeyboardState(&count);
    std::vector<KeyBinding> pressed;
    if (state == nullptr) return pressed;
    const SDL_Keymod held = SDL_GetModState();
    unsigned mods = 0;
    if (held & KMOD_CTRL) mods |= kCtrl;
    if (held & KMOD_SHIFT) mods |= kShift;
    if (held & KMOD_ALT) mods |= kAlt;
    for (int code = 0; code < count && code < SDL_NUM_SCANCODES; ++code) {
        const bool down = state[code] != 0;
        const bool edge = down && !was[static_cast<std::size_t>(code)];
        was[static_cast<std::size_t>(code)] = down;
        if (!edge) continue;
        const auto scancode = static_cast<SDL_Scancode>(code);
        if (scancode >= SDL_SCANCODE_LCTRL && scancode <= SDL_SCANCODE_RGUI) continue;  // a modifier alone is no hotkey
        const char *name = SDL_GetScancodeName(scancode);
        if (name == nullptr || *name == '\0') continue;
        pressed.push_back(KeyBinding{name, mods});
    }
    return pressed;
}

bool key_held(const KeyBinding &binding) {
    if (binding.key.empty()) return false;
    const SDL_Keycode code = SDL_GetKeyFromName(binding.key.c_str());
    if (code == SDLK_UNKNOWN) return false;
    const SDL_Scancode scancode = SDL_GetScancodeFromKey(code);
    int count = 0;
    const Uint8 *state = SDL_GetKeyboardState(&count);
    if (state == nullptr || scancode <= SDL_SCANCODE_UNKNOWN || scancode >= count || state[scancode] == 0) return false;
    const SDL_Keymod held = SDL_GetModState();
    unsigned mods = 0;
    if (held & KMOD_CTRL) mods |= kCtrl;
    if (held & KMOD_SHIFT) mods |= kShift;
    if (held & KMOD_ALT) mods |= kAlt;
    return (mods & binding.mods) == binding.mods;
}

}  // namespace gt::cheat_device
