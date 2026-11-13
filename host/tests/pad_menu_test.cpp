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
        for (std::uint32_t raw : {0u, kPadCross, kPadCross | kPadSquare, kPadL | kPadR, kPadL, 0u}) same = same && d.step(raw) == raw;
        check(same && d.keys.empty() && !d.open, "closed menu: identity for every other button (L+R, Cross, Square pass unchanged)");
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
        d.step(0);
        check(d.step(kPadStart) == 0u, "Start pressed alone is held back (the hold-back window), not lost");
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
        check(d.step(kPadSelect) == 0u && d.step(kChord) == 0u && d.open, "Select, then Start a frame later: fires, the game saw neither");
    }
    {  // another chord, and the sabotage
        std::string error;
        const std::uint32_t lr = parse_pad_chord("L+R", error);
        check(lr == (kPadL | kPadR) && error.empty(), "parse: L+R");
        Driver d(PadMenuConfig{lr});
        check(!d.open && d.step(kChord) == kChord && !d.open, "chord L+R: Select+Start passes through and does not open");
        Driver d2(PadMenuConfig{lr});
        check(d2.step(kPadL | kPadR) == 0u && d2.open, "chord L+R opens it");
        Driver sab(PadMenuConfig{kChord, 18, 4, 8, true});
        check(sab.step(kChord) == kChord && !sab.open && sab.keys.empty(), "ignore_chord: never opens, nothing held back");
    }
    {
        std::string error;
        check(parse_pad_chord("select+start", error) == (kPadSelect | kPadStart), "parse: lower case, SELECT+START");
        check(parse_pad_chord("FOO", error) == 0u && error.find("FOO") != std::string::npos, "parse: unknown name");
        error.clear();
        check(parse_pad_chord("", error) == 0u && !error.empty(), "parse: empty");
    }
    {  // the hold-back window: staggered chords, plain taps and holds of Select and Start
        const auto run = [](std::vector<std::uint32_t> raws, bool *opened = nullptr) {
            Driver d;
            std::vector<std::uint32_t> seen;
            for (const std::uint32_t r : raws) seen.push_back(d.step(r));
            if (opened != nullptr) *opened = d.open;
            return seen;
        };
        for (unsigned gap : {1u, 3u, 7u}) {  // the second button arrives inside the window: either order, the game sees neither
            for (const std::uint32_t first : {kPadSelect, kPadStart}) {
                std::vector<std::uint32_t> raws;
                for (unsigned i = 0; i < gap; ++i) raws.push_back(first);
                for (int i = 0; i < 4; ++i) raws.push_back(kChord);
                raws.push_back(0);
                bool opened = false;
                const auto seen = run(raws, &opened);
                bool none = true;
                for (const std::uint32_t v : seen) none = none && v == 0u;
                check(opened && none, "stagger inside the window: the menu opens and the game sees neither button");
            }
        }
        {  // the second button arrives after the window: the first went through (late), the chord still opens the menu
            std::vector<std::uint32_t> raws(9, kPadSelect);
            raws.push_back(kChord);
            bool opened = false;
            const auto seen = run(raws, &opened);
            check(opened && seen[7] == 0u && seen[8] == kPadSelect, "stagger of 9 polls: Select is forwarded from the 9th poll (late) ...");
        }
        {  // a plain Select tap of 3 polls comes back late with the same length (reset-to-track still sees a tap)
            const auto seen = run({kPadSelect, kPadSelect, kPadSelect, 0, 0, 0, 0, 0, 0});
            check(seen == std::vector<std::uint32_t>({0, 0, 0, kPadSelect, kPadSelect, kPadSelect, 0, 0, 0}), "Select tap: held back, then replayed with its real length (3)");
        }
        {  // the no-replay sabotage config (verification only): the same tap never reaches the game
            gt::PadMenuConfig config;
            config.no_replay = true;
            Driver d(config);
            d.filter.set_hotkeys({gt::PadHotkeyMask{kPadSelect, 30}});  // reset-to-track's SELECT@30
            std::uint32_t seen = 0;
            for (const std::uint32_t raw : {kPadSelect, kPadSelect, kPadSelect, 0u, 0u, 0u, 0u}) seen |= d.step(raw);
            check(seen == 0u, "no_replay: a hotkey's Select tap inside the window is swallowed");
            seen = 0;
            for (const std::uint32_t raw : {kPadStart, 0u, 0u, 0u}) seen |= d.step(raw);
            check(seen == kPadStart, "no_replay: a chord-only button (Start) is still replayed: the menus keep working");
        }
        {  // a one-poll tap
            const auto seen = run({kPadStart, 0, 0, 0});
            check(seen == std::vector<std::uint32_t>({0, kPadStart, 0, 0}), "Start tap of 1 poll: replayed 1 poll later, length 1");
        }
        {  // a hold longer than the window: forwarded from the end of the window until released (reset-to-track measures its hold from there)
            std::vector<std::uint32_t> raws(20, kPadSelect);
            raws.push_back(0);
            const auto seen = run(raws);
            check(seen[7] == 0u && seen[8] == kPadSelect && seen[19] == kPadSelect && seen[20] == 0u, "Select hold: delayed by the window, released with the real release");
        }
        {  // a Select pressed while the menu is open is not replayed when the menu closes
            Driver d;
            d.step(kChord);
            d.step(0);
            d.step(kPadSelect);
            d.step(0);
            d.step(0);
            d.step(kChord);
            const std::uint32_t after = d.step(0);
            check(!d.open && after == 0u && d.step(0) == 0u && d.step(0) == 0u, "nothing is replayed after the menu closes");
        }
    }
    {  // the cheat device's other hotkeys (nitro on L+TRIANGLE, reset on SELECT held 30): held back like the chord
        constexpr std::uint32_t kTri = 0x1000u;
        Driver d;
        d.filter.set_hotkeys({{gt::kPadL | kTri, 0}, {gt::kPadSelect, 30}});
        std::uint32_t seen = 0;
        for (int i = 0; i < 3; ++i) seen |= d.step(gt::kPadL);
        check(seen == 0u, "a lone L of a hotkey is held back");
        check(d.step(gt::kPadL | kTri) == 0u && d.step(gt::kPadL | kTri) == 0u && d.step(0) == 0u,
              "L+TRIANGLE completes: the game never sees L nor TRIANGLE");
        std::vector<std::uint32_t> late;
        for (int i = 0; i < 4; ++i) late.push_back(d.step(i < 2 ? gt::kPadL : 0u));
        check(late[0] == 0u && late[1] == 0u && late[2] == gt::kPadL && late[3] == gt::kPadL && d.step(0) == 0u,
              "L tapped for 2 polls, no TRIANGLE: the game gets it late, 2 polls long");
        seen = 0;
        for (int i = 0; i < 12; ++i) seen |= d.step(gt::kPadL);
        check((seen & gt::kPadL) != 0u, "L held past 8 polls goes through (the throttle-like case)");
        d.step(0);
        seen = 0;
        for (int i = 0; i < 40; ++i) seen |= d.step(gt::kPadSelect);
        check(seen == 0u && d.step(0) == 0u, "SELECT held 40 polls (the reset): the game never sees it, nothing replayed");
        std::vector<std::uint32_t> tap;
        for (int i = 0; i < 6; ++i) tap.push_back(d.step(i < 3 ? gt::kPadSelect : 0u));
        check(tap[0] == 0u && tap[2] == 0u && tap[3] == gt::kPadSelect && tap[5] == gt::kPadSelect,
              "a short SELECT (3 polls) reaches the game late as a 3-poll tap (the camera button)");
    }
    if (failures == 0) std::puts("pad_menu tests passed");
    return failures == 0 ? 0 : 1;
}
