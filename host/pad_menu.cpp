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
