// The pad's way into the host overlay (F9 MODS page): a chord opens and closes it, the D-pad moves, Cross activates, Left/Right
// change a value, Circle closes. Pure logic (no SDL, no guest memory) so CTest drives it (host/tests/pad_menu_test.cpp);
// host/play_backends.cpp wires it to the kernel's pad read (host/host_hooks.hpp) and to lcs::host_settings_handle.
//
// Stagger: a human does not press two buttons in one vblank. A chord button that is pressed alone is held back from the game for
// `hold_back` vblanks: if the rest of the chord arrives inside the window the game never sees either button; if the button is released
// inside it the game gets the press late, with its real duration (a Select tap is still a tap for reset-to-track); if the window runs out
// the game gets the press from then on. Other buttons are never delayed.
//
// While the menu is open the game sees no buttons and a centred stick. A button pressed while the menu was open, and the chord
// buttons themselves, stay hidden from the game until they are released (no stray Circle or Start after the menu closes); a
// button already held when the menu opened (the throttle) is visible again as soon as it closes. With the menu closed and no
// chord the buttons pass through unchanged.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace gt {

constexpr std::uint32_t kPadSelect = 0x0001u, kPadStart = 0x0008u, kPadUp = 0x0010u, kPadRight = 0x0020u, kPadDown = 0x0040u,
                        kPadLeft = 0x0080u, kPadL = 0x0100u, kPadR = 0x0200u, kPadCircle = 0x2000u, kPadCross = 0x4000u, kPadSquare = 0x8000u;

// What the filter asks the overlay to do (lcs::HostSettingsKey, which host/play_backends.cpp maps these to).
enum class MenuKey { TogglePage, Up, Down, Left, Right, Activate, Close };

struct PadMenuKeys {
    std::array<MenuKey, 8> keys{};
    std::size_t count = 0;
    void push(MenuKey key) {
        if (count < keys.size()) keys[count++] = key;
    }
};

struct PadMenuConfig {
    std::uint32_t chord = kPadSelect | kPadStart;  // all held together (the press that completes it fires)
    unsigned repeat_delay = 18;                    // polls a direction is held before it repeats
    unsigned repeat_period = 4;                    // polls between repeats
    unsigned hold_back = 8;                        // vblanks (~130 ms) a lone chord button is held back
    bool ignore_chord = false;                     // verification only (GT_PAD_MENU_SABOTAGE_IGNORE_CHORD): the chord never fires
};

// "SELECT+START", "L+R+START": names as the GT_PAD script uses them; 0 (and error filled) for an unknown name or an empty list.
std::uint32_t parse_pad_chord(const std::string &text, std::string &error);

class PadMenuFilter {
public:
    explicit PadMenuFilter(PadMenuConfig config = {}) : config_(config) {}

    // Once per vblank, with the raw buttons the game would read and whether the overlay is open now: the overlay keys to apply, in order.
    PadMenuKeys poll(std::uint32_t raw, bool menu_open);
    // The buttons the game gets for `raw` with the overlay in state `menu_open` (after the keys of poll were applied). Cheap and
    // repeatable: several threads read the pad within one vblank.
    std::uint32_t visible(std::uint32_t raw, bool menu_open) const {
        return menu_open ? 0u : ((raw & ~suppressed_ & ~pending_) | replay_);
    }

private:
    void update_hold_back(std::uint32_t raw, std::uint32_t pressed, bool chord_fire, bool menu_open);
    PadMenuConfig config_;
    std::uint32_t previous_ = 0;
    std::uint32_t suppressed_ = 0;
    bool chord_was_down_ = false;
    std::uint32_t pending_ = 0;  // chord buttons held back right now (still down, window not over)
    std::uint32_t replay_ = 0;   // chord buttons whose late tap is being given to the game this vblank
    std::array<unsigned, 16> age_{};     // polls each pending button has been down
    std::array<unsigned, 16> replay_left_{};  // polls of late tap still to give, per button
    std::array<unsigned, 4> held_{};  // polls Up, Down, Left, Right have been held while the menu was open
};

}  // namespace gt
