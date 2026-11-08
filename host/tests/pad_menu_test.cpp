// Tests for the pad menu logic (host/pad_menu.cpp): chord, navigation, hiding of the pad from the game.
#include "pad_menu.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

using gt::MenuKey;
using gt::PadMenuFilter;
using gt::PadMenuKeys;

constexpr std::uint32_t kChord = gt::kPadSelect | gt::kPadStart;

// Drives a filter the way play_backends.cpp does: poll with the open state, apply TogglePage/Close to it, then view.
struct Driver {
    PadMenuFilter filter;
    bool open = false;
    std::vector<MenuKey> keys;
    explicit Driver(gt::PadMenuConfig config = {}) : filter(config) {}
    std::uint32_t step(std::uint32_t raw) {
        const PadMenuKeys k = filter.poll(raw, open);
        for (std::size_t i = 0; i < k.count; ++i) {
            keys.push_back(k.keys[i]);
            if (k.keys[i] == MenuKey::TogglePage) open = !open;
            if (k.keys[i] == MenuKey::Close) open = false;
        }
        return filter.visible(raw, open);
    }
};
std::size_t count_of(const std::vector<MenuKey> &keys, MenuKey key) {
    std::size_t n = 0;
    for (const MenuKey k : keys) n += k == key ? 1 : 0;
    return n;
}
}  // namespace

int main() {
    using namespace gt;
    {  // closed, no chord: buttons pass through unchanged
        Driver d;
        bool same = true;
        for (std::uint32_t raw : {0u, kPadCross, kPadCross | kPadSquare, kPadSelect, kPadStart, kPadSelect, 0u, kPadL | kPadR}) same = same && d.step(raw) == raw;
        check(same && d.keys.empty() && !d.open, "closed menu: identity, no keys (Select alone, Start alone, L+R are the game's)");
    }
    {  // the chord opens the menu; the game sees nothing; the chord buttons stay hidden until released; the same chord closes
        Driver d;
        check(d.step(kChord | kPadCross) == 0u && d.open && count_of(d.keys, MenuKey::TogglePage) == 1, "chord opens: even the held throttle is hidden while open");
        for (int i = 0; i < 100; ++i) d.step(kChord | kPadCross);
        check(d.open && count_of(d.keys, MenuKey::TogglePage) == 1, "chord held for 100 polls fires once");
        check(d.step(kPadCross) == 0u && d.open, "chord released, throttle still held: still open, still hidden");
        check(count_of(d.keys, MenuKey::Activate) == 0, "a button held from before the menu opened is not an Activate");
        d.step(0);
        d.step(kChord);
        check(!d.open && count_of(d.keys, MenuKey::TogglePage) == 2, "the chord again closes");
        check(d.step(kChord | kPadCross) == kPadCross, "after closing: the chord buttons stay hidden while held; the throttle is visible");
        check(d.step(kPadStart | kPadCross) == kPadCross && d.step(kPadCross) == kPadCross, "releasing the chord one button at a time: still hidden");
        check(d.step(kPadStart) == kPadStart, "released, then pressed again alone (Start is the game's pause again)");
    }
    {  // navigation: D-pad, repeat, Cross activates, Left/Right, Circle closes; presses made while open stay hidden after
        Driver d;
        d.step(kChord);
        d.step(0);
        d.keys.clear();
        for (int i = 0; i < 30; ++i) d.step(kPadDown);
        check(count_of(d.keys, MenuKey::Down) == 4, "Down held 30 polls: once, then repeats at 18, 22, 26");
        d.step(0);
        d.keys.clear();
        d.step(kPadUp);
        d.step(0);
        d.step(kPadLeft);
        d.step(0);
        d.step(kPadRight);
        d.step(0);
        d.step(kPadCross);
        d.step(kPadCross);
        d.step(0);
        check(d.keys.size() == 4 && d.keys[0] == MenuKey::Up && d.keys[1] == MenuKey::Left && d.keys[2] == MenuKey::Right && d.keys[3] == MenuKey::Activate,
              "Up, Left, Right, Cross (once, not while held) give Up, Left, Right, Activate");
        check(d.step(kPadCircle | kPadCross) == 0u && !d.open && d.keys.back() == MenuKey::Close, "Circle closes the menu");
        check(d.step(kPadCircle | kPadCross) == 0u, "Circle and Cross pressed while open stay hidden after it closed ...");
        check(d.step(kPadCross) == 0u && d.step(0) == 0u && d.step(kPadCross) == kPadCross, "... until released");
    }
    {  // a direction held while the menu opens does not act until it is pressed again
        Driver d;
        d.step(kChord | kPadDown);
        d.step(kPadDown);
        d.step(kPadDown);
        check(count_of(d.keys, MenuKey::Down) == 0, "Down held across the opening: no key");
        d.step(0);
        d.step(kPadDown);
        check(count_of(d.keys, MenuKey::Down) == 1, "pressed again: key");
    }
    {  // pressing the chord's buttons one frame apart: the first leaks (documented), the second fires and hides both
        Driver d;
        check(d.step(kPadSelect) == kPadSelect && d.step(kChord) == 0u && d.open, "Select, then Start a frame later: fires on the second");
    }
    {  // another chord, and the sabotage
        std::string error;
        const std::uint32_t lr = parse_pad_chord("L+R", error);
        check(lr == (kPadL | kPadR) && error.empty(), "parse: L+R");
        Driver d(PadMenuConfig{lr});
        check(d.step(kChord) == kChord && !d.open && d.step(kPadL | kPadR) == 0u && d.open, "chord L+R: Select+Start no longer opens it");
        Driver sab(PadMenuConfig{kChord, 18, 4, true});
        check(sab.step(kChord) == kChord && !sab.open && sab.keys.empty(), "ignore_chord: never opens");
    }
    {
        std::string error;
        check(parse_pad_chord("select+start", error) == (kPadSelect | kPadStart), "parse: lower case, SELECT+START");
        check(parse_pad_chord("FOO", error) == 0u && error.find("FOO") != std::string::npos, "parse: unknown name");
        error.clear();
        check(parse_pad_chord("", error) == 0u && !error.empty(), "parse: empty");
    }
    if (failures == 0) std::puts("pad_menu tests passed");
    return failures == 0 ? 0 : 1;
}
