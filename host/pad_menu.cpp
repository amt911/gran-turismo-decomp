#include "pad_menu.hpp"

#include "pad_sequence.hpp"

#include <cctype>

namespace gt {

std::uint32_t parse_pad_chord(const std::string &text, std::string &error) {
    std::uint32_t mask = 0;
    std::string name;
    const auto flush = [&]() {
        if (name.empty()) return true;
        const std::uint32_t bits = pad_button_bits(name);
        if (bits == 0) {
            error = "unknown button '" + name + "'";
            return false;
        }
        mask |= bits;
        name.clear();
        return true;
    };
    for (const char c : text) {
        if (c == '+' || c == ',' || c == ' ') {
            if (!flush()) return 0;
        } else {
            name += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    if (!flush()) return 0;
    if (mask == 0) error = "empty chord";
    return mask;
}

void PadMenuFilter::set_hotkeys(std::vector<PadHotkeyMask> hotkeys) {
    hotkeys_ = std::move(hotkeys);
    hotkey_down_.assign(hotkeys_.size(), false);
    hotkey_held_.assign(hotkeys_.size(), 0u);
}

// Polls a held-back button waits before it goes through: the chord's and a press hotkey's `hold_back`, a hold hotkey's `hold`.
unsigned PadMenuFilter::window(unsigned bit) const {
    unsigned w = config_.hold_back;
    for (const PadHotkeyMask &h : hotkeys_)
        if ((h.buttons & (1u << bit)) != 0 && h.hold > w) w = h.hold;
    return w;
}

std::uint32_t PadMenuFilter::fire_hotkeys(std::uint32_t raw, bool menu_open) {
    std::uint32_t fired = 0;
    for (std::size_t i = 0; i < hotkeys_.size(); ++i) {
        const PadHotkeyMask &h = hotkeys_[i];
        const bool down = h.buttons != 0 && (raw & h.buttons) == h.buttons;
        hotkey_held_[i] = down ? hotkey_held_[i] + 1 : 0u;
        const bool completes = h.hold == 0 ? down && !hotkey_down_[i] : hotkey_held_[i] == h.hold;
        hotkey_down_[i] = down;
        if (completes && !menu_open) fired |= h.buttons;
    }
    return fired;
}

// The hold-back window of the chord's and the hotkeys' buttons (see pad_menu.hpp). `age_` counts the polls a pending button has been
// down, which is also the length of the late tap it gets if it is released first.
void PadMenuFilter::update_hold_back(std::uint32_t raw, std::uint32_t pressed, std::uint32_t fired, bool menu_open) {
    replay_ = 0;
    if (config_.hold_back == 0) return;
    std::uint32_t held = config_.ignore_chord ? 0u : config_.chord;
    for (const PadHotkeyMask &h : hotkeys_) held |= h.buttons;
    for (unsigned bit = 0; bit < 16; ++bit) {
        const std::uint32_t mask = 1u << bit;
        if ((held & mask) == 0) continue;
        if ((fired & mask) != 0 || menu_open) {  // its combination completed, or the menu is up: nothing is replayed or held back
            pending_ &= ~mask;
            replay_left_[bit] = 0;
            continue;
        }
        if (replay_left_[bit] != 0) {
            replay_ |= mask;
            --replay_left_[bit];
        }
        if ((pressed & mask) != 0) {
            pending_ |= mask;
            age_[bit] = 0;
        } else if ((pending_ & mask) != 0) {
            if ((raw & mask) == 0) {  // released inside the window: give the game the press late, as long as it really lasted
                pending_ &= ~mask;
                replay_left_[bit] = age_[bit] + 1;  // age counts the polls after the first
                if (replay_left_[bit] != 0) {
                    replay_ |= mask;
                    --replay_left_[bit];
                }
            } else if (++age_[bit] >= window(bit)) {  // the window is over: the press goes through from now on
                pending_ &= ~mask;
            }
        }
    }
}

PadMenuKeys PadMenuFilter::poll(std::uint32_t raw, bool menu_open) {
    PadMenuKeys out;
    const std::uint32_t pressed = raw & ~previous_;
    previous_ = raw;
    const bool chord_down = config_.chord != 0 && (raw & config_.chord) == config_.chord && !config_.ignore_chord;
    const bool chord_fire = chord_down && !chord_was_down_;
    chord_was_down_ = chord_down;

    if (menu_open) suppressed_ |= pressed;  // pressed while the menu was up: the game never sees this press, nor its release
    if (chord_fire) {
        suppressed_ |= config_.chord;
        out.push(MenuKey::TogglePage);
    }
    const std::uint32_t fired = fire_hotkeys(raw, menu_open) | (chord_fire ? config_.chord : 0u);
    suppressed_ |= fired;
    update_hold_back(raw, pressed, fired, menu_open);

    static constexpr std::uint32_t kDirections[4] = {kPadUp, kPadDown, kPadLeft, kPadRight};
    static constexpr MenuKey kDirectionKeys[4] = {MenuKey::Up, MenuKey::Down, MenuKey::Left, MenuKey::Right};
    for (std::size_t i = 0; i < 4; ++i) {
        if (!menu_open || chord_fire || (raw & kDirections[i]) == 0) {
            held_[i] = 0;
            continue;
        }
        if (held_[i] == 0 && (pressed & kDirections[i]) == 0) continue;  // held since before the menu opened: wait for a new press
        const unsigned n = held_[i]++;
        const bool fire = n == 0 || (n >= config_.repeat_delay && config_.repeat_period != 0 &&
                                     (n - config_.repeat_delay) % config_.repeat_period == 0);
        if (fire) out.push(kDirectionKeys[i]);
    }
    if (menu_open && !chord_fire) {
        if (pressed & kPadCross) out.push(MenuKey::Activate);
        if (pressed & kPadCircle) out.push(MenuKey::Close);
    }
    suppressed_ &= raw;  // a released button is visible again
    return out;
}

}  // namespace gt
