// Tests for the cheat device core (host/cheat_device/): menu navigation, binding parse/print, conflicts, pad hotkeys, capture,
// bindings.toml round trip. Plain executable, run by CTest.
#include "cheat_device/bindings.hpp"
#include "cheat_device/menu.hpp"
#include "pad_menu.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
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

using namespace gt::cheat_device;
constexpr std::uint32_t kTriangle = 0x1000u;

void test_menu() {
    int slow = 0;  // 0 off, 1 0.5x, 2 0.25x
    bool frozen = false;
    int jumps = 0;
    const char *kSlow[] = {"OFF", "0.5x", "0.25x"};
    Entry race{"race", EntryKind::Submenu, "Race", {}, {}, {}};
    race.children.push_back(Entry{"slow", EntryKind::Choice, "Slow motion", [&] { return std::string(kSlow[slow]); },
                                  [&](int d) { slow = (slow + (d < 0 ? 2 : 1)) % 3; }, {}});
    race.children.push_back(Entry{"freeze", EntryKind::Toggle, "Freeze AI", [&] { return std::string(frozen ? "ON" : "OFF"); },
                                  [&](int) { frozen = !frozen; }, {}});
    race.children.push_back(Entry{"jump", EntryKind::Action, "Jump", {}, [&](int) { ++jumps; }, {}});
    Entry root{"root", EntryKind::Submenu, "Cheats", {}, {}, {}};
    root.children.push_back(race);
    root.children.push_back(Entry{"mods", EntryKind::Submenu, "Mods", {}, {}, {}});
    Navigator nav(root);

    check(nav.path() == "Cheats", "starts at the root");
    check(nav.rows(5).size() == 2 && nav.rows(5)[0].selected && nav.rows(5)[0].submenu, "root rows: Race selected, a submenu");
    nav.enter();
    check(nav.path() == "Cheats > Race", "Enter opens a submenu");
    nav.right();
    check(slow == 1 && nav.rows(5)[0].value == "0.5x", "Right steps a choice forward");
    nav.left();
    nav.left();
    check(slow == 2, "Left steps back, wrapping");
    nav.down();
    nav.enter();
    check(frozen, "Enter flips a toggle");
    nav.down();
    nav.enter();
    nav.right();  // an action ignores Left/Right
    check(jumps == 1, "Enter fires an action once; Right does not");
    nav.down();
    check(nav.cursor() == 0, "Down wraps to the top");
    nav.up();
    check(nav.cursor() == 2, "Up wraps to the bottom");
    check(nav.back() && nav.path() == "Cheats", "Back goes up");
    check(!nav.back(), "Back at the root returns false (the caller closes)");
    nav.enter();
    check(nav.cursor() == 2, "the cursor of a page is remembered");
    check(nav.rows(2).size() == 2 && nav.rows(2)[1].selected && nav.rows(2)[1].title == "Jump", "rows scroll to keep the cursor visible");
}

void test_parse_print() {
    std::string error;
    auto key = parse_key("Ctrl+Shift+F5", error);
    check(key && key->key == "F5" && key->mods == (kCtrl | kShift) && print_key(*key) == "Ctrl+Shift+F5", "key round trip");
    check(!parse_key("Hyper+F5", error) && error.find("Hyper") != std::string::npos, "unknown modifier refused");
    check(parse_key("", error) && parse_key("", error)->key.empty(), "empty key = cleared");
    auto pad = parse_pad("l+triangle", error);
    check(pad && pad->buttons == (gt::kPadL | kTriangle) && print_pad(*pad) == "L+TRIANGLE", "pad round trip, case-insensitive");
    auto hold = parse_pad("SELECT@30", error);
    check(hold && hold->buttons == gt::kPadSelect && hold->hold == 30u && print_pad(*hold) == "SELECT@30", "hold round trip");
    check(!parse_pad("SELECT+JUMP", error) && error.find("JUMP") != std::string::npos, "unknown button refused");
    check(!parse_pad("SELECT@soon", error), "bad hold refused");
}

BindingTable table_with_defaults() {
    BindingTable t;
    std::string error;
    t.add_action("device.open", "Open the cheat device", KeyBinding{"F9", 0}, PadBinding{gt::kPadSelect | gt::kPadStart, 0}, true, error);
    t.add_action("reset.reset", "Reset to track", KeyBinding{"R", 0}, PadBinding{gt::kPadSelect, 30}, false, error);
    t.add_action("cheats.nitro", "Nitro", KeyBinding{"N", kCtrl}, PadBinding{gt::kPadL | kTriangle, 0}, false, error);
    return t;
}

void test_conflicts() {
    BindingTable t = table_with_defaults();
    std::string error;
    check(!t.add_action("cheats.nitro", "Nitro again", {}, {}, false, error), "a duplicate id is refused");
    check(!t.add_action("x.y", "Clash", KeyBinding{"F9", 0}, {}, false, error) && error.find("Open the cheat device") != std::string::npos,
          "a default that clashes is refused, naming the owner");
    check(!t.set_key("cheats.nitro", KeyBinding{"r", 0}, error) && error.find("Reset to track") != std::string::npos,
          "rebinding to a key another action uses is refused (case-insensitive), naming it");
    check(t.find("cheats.nitro")->key == (KeyBinding{"N", kCtrl}), "a refused rebinding changes nothing");
    check(!t.set_pad("cheats.nitro", PadBinding{gt::kPadSelect | gt::kPadStart, 0}, error), "the open combination cannot be taken");
    check(!t.set_pad("cheats.nitro", PadBinding{gt::kPadCross, 0}, error), "a single race button pressed alone is refused");
    check(t.set_pad("cheats.nitro", PadBinding{gt::kPadCross, 20}, error), "a single button with a hold time is accepted");
    check(t.set_key("device.open", KeyBinding{}, error), "a required action may lose one binding");
    check(!t.set_pad("device.open", PadBinding{}, error), "but not the last one");
    check(t.by_key(KeyBinding{"n", kCtrl}) == t.find("cheats.nitro") && t.by_key(KeyBinding{"N", 0}) == nullptr, "by_key honours modifiers");
    t.reset_all();
    check(t.find("device.open")->key.key == "F9", "reset_all restores the defaults");
}

void test_pad_hotkeys() {
    BindingTable t = table_with_defaults();
    PadHotkeys hk;
    check(hk.poll(t, gt::kPadL).empty(), "half a combination fires nothing");
    auto fired = hk.poll(t, gt::kPadL | kTriangle);
    check(fired.size() == 1 && fired[0] == "cheats.nitro", "the combination fires when it completes");
    check(hk.poll(t, gt::kPadL | kTriangle).empty(), "held: fires once per press");
    hk.poll(t, 0);
    check(hk.poll(t, gt::kPadL | kTriangle).size() == 1, "released and pressed again: fires again");
    hk.poll(t, 0);
    std::vector<std::string> all;
    for (int i = 0; i < 40; ++i) for (auto &id : hk.poll(t, gt::kPadSelect)) all.push_back(id);
    check(all.size() == 1 && all[0] == "reset.reset", "a hold binding fires once, after its hold time");
    hk.poll(t, 0);
    all.clear();
    for (int i = 0; i < 40; ++i) for (auto &id : hk.poll(t, i == 0 ? gt::kPadSelect : gt::kPadSelect | gt::kPadStart)) all.push_back(id);
    check(all.size() == 1 && all[0] == "device.open", "SELECT+START opens the device and does not also count as holding SELECT");
}

void test_capture() {
    PadCapture c(30, 100);
    check(c.poll(gt::kPadL) == PadCapture::State::Waiting, "capture waits while buttons are down");
    c.poll(gt::kPadL | kTriangle);
    check(c.poll(0) == PadCapture::State::Done && c.result() == (PadBinding{gt::kPadL | kTriangle, 0}), "the union of the buttons held");
    PadCapture h(30, 100);
    for (int i = 0; i < 31; ++i) h.poll(gt::kPadSelect);
    check(h.poll(0) == PadCapture::State::Done && h.result() == (PadBinding{gt::kPadSelect, 30}), "one button held long = a hold binding");
    PadCapture idle(30, 5);
    PadCapture::State s = PadCapture::State::Waiting;
    for (int i = 0; i < 5; ++i) s = idle.poll(0);
    check(s == PadCapture::State::TimedOut, "nothing pressed: the capture times out");
}

void test_text_round_trip() {
    BindingTable t = table_with_defaults();
    std::string error;
    t.set_key("cheats.nitro", KeyBinding{"F6", kAlt}, error);
    std::vector<std::string> warnings;
    BindingTable later;
    later.load_text("gone.mod.key = \"F12\"\n" + t.to_text() + "cheats.nitro.pad = \"L+JUMP\"\nbroken line\n", warnings);
    for (const Action &a : t.actions()) later.add_action(a.id, a.label, a.default_key, a.default_pad, a.required, error);
    check(later.find("cheats.nitro")->key == (KeyBinding{"F6", kAlt}), "a saved binding applies when its action is declared later");
    check(later.to_text().find("gone.mod.key = \"F12\"") != std::string::npos, "lines of mods not loaded now are kept");
    check(warnings.size() == 1 && warnings[0].find("no '='") != std::string::npos, "a bad line is a warning, not a stop");
    const std::vector<std::string> late = later.take_warnings();
    check(late.size() == 1 && late[0].find("JUMP") != std::string::npos, "a bad saved line applied late warns too");
    BindingTable clash = table_with_defaults();
    warnings.clear();
    clash.load_text("cheats.nitro.key = \"R\"\n", warnings);
    check(clash.find("cheats.nitro")->key == (KeyBinding{"N", kCtrl}) && warnings.size() == 1, "a clashing saved binding keeps the default and warns");
    check(table_with_defaults().to_text().find(".key") == std::string::npos, "defaults are not written");
}

std::string slurp(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void test_file_round_trip() {
    const auto dir = std::filesystem::temp_directory_path() / "cheat_device_test_rt";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    BindingTable t = table_with_defaults();
    std::string error;
    t.set_key("cheats.nitro", KeyBinding{"F6", kAlt}, error);
    check(t.save_file(dir / "bindings.toml", error), "save_file succeeds");
    check(!std::filesystem::exists(dir / "bindings.toml.tmp"), "no temporary file is left behind");
    BindingTable later = table_with_defaults();
    std::vector<std::string> warnings;
    later.load_file(dir / "bindings.toml", warnings);
    check(later.find("cheats.nitro")->key == (KeyBinding{"F6", kAlt}) && warnings.empty(), "load_file restores the saved binding");
    BindingTable none = table_with_defaults();
    none.load_file(dir / "missing.toml", warnings);
    check(warnings.empty() && none.find("cheats.nitro")->key == (KeyBinding{"N", kCtrl}), "a missing file is no warning, defaults stay");
    std::filesystem::remove_all(dir);
}

void test_failed_save_keeps_old_file() {
    const auto dir = std::filesystem::temp_directory_path() / "cheat_device_test_ro";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    BindingTable t = table_with_defaults();
    std::string error;
    t.set_key("cheats.nitro", KeyBinding{"F6", kAlt}, error);
    t.save_file(dir / "bindings.toml", error);
    const std::string before = slurp(dir / "bindings.toml");
    std::filesystem::permissions(dir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    t.set_key("cheats.nitro", KeyBinding{"F7", 0}, error);
    error.clear();
    const bool saved = t.save_file(dir / "bindings.toml", error);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    check(!saved && !error.empty(), "a save into a read-only folder fails with a reason");
    check(slurp(dir / "bindings.toml") == before, "and leaves the old file as it was");
    std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
    test_menu();
    test_parse_print();
    test_conflicts();
    test_pad_hotkeys();
    test_capture();
    test_text_round_trip();
    test_file_round_trip();
    test_failed_save_keeps_old_file();
    if (failures == 0) std::puts("cheat_device: all tests passed");
    return failures == 0 ? 0 : 1;
}
