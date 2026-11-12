// Cheat device (spec docs/superpowers/specs/2026-11-11-cheat-device-design.md, §2): actions and their hotkeys, owned by the host.
// Pure logic: no SDL (keys are SDL key *names*, resolved by the caller), no guest memory, so CTest drives it
// (host/tests/cheat_device_test.cpp). A mod declares an action; the player binds it to a keyboard key and/or a pad combination,
// from the device's Controls page; the table refuses a binding another action already uses (never "last one wins").
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gt::cheat_device {

enum KeyMods : unsigned { kCtrl = 1u, kShift = 2u, kAlt = 4u };

struct KeyBinding {
    std::string key;    // an SDL key name ("F9", "R", "Keypad 5"); empty = no keyboard binding
    unsigned mods = 0;  // KeyMods held with it
    bool operator==(const KeyBinding &) const = default;
};

struct PadBinding {
    std::uint32_t buttons = 0;  // PSP button bits (host/pad_menu.hpp kPad*), all held together; 0 = no pad binding
    unsigned hold = 0;          // vblanks the combination must be held (0 = fires when it completes)
    bool operator==(const PadBinding &) const = default;
};

// "Ctrl+Shift+F5", "R": empty text = cleared. nullopt and `error` set for an unknown modifier or a missing key.
std::optional<KeyBinding> parse_key(const std::string &text, std::string &error);
std::string print_key(const KeyBinding &binding);
// "L+TRIANGLE", "SELECT@30" (held 30 vblanks): button names as GT_PAD scripts use them.
std::optional<PadBinding> parse_pad(const std::string &text, std::string &error);
std::string print_pad(const PadBinding &binding);

struct Action {
    std::string id;     // "<mod>.<action>", e.g. "cheats.nitro"
    std::string label;  // shown in the Controls page
    KeyBinding default_key;
    PadBinding default_pad;
    bool required = false;  // at least one binding must stay (the device's own open combination)
    KeyBinding key;
    PadBinding pad;
};

class BindingTable {
public:
    // Declares an action with its defaults (refused: empty or duplicate id, or defaults that clash with another action).
    bool add_action(const std::string &id, const std::string &label, KeyBinding key, PadBinding pad, bool required,
                    std::string &error);
    // Changes one binding; refused with the reason (and nothing changed) when another action uses it, when a pad binding is a
    // single button pressed (no modifier, no hold: every lone button means something in a race), or when it would leave a
    // required action with no binding at all.
    bool set_key(const std::string &id, const KeyBinding &key, std::string &error);
    bool set_pad(const std::string &id, const PadBinding &pad, std::string &error);
    bool reset(const std::string &id);  // back to the defaults
    void reset_all();
    const Action *find(const std::string &id) const;
    const std::vector<Action> &actions() const { return actions_; }

    // The action a keyboard key (with its modifiers) fires, if any.
    const Action *by_key(const KeyBinding &key) const;

    // bindings.toml: one `<id>.key = "..."` / `<id>.pad = "..."` line per binding that differs from its default. Lines for
    // actions no mod declares now are kept as they are (a mod switched off keeps the player's bindings). A bad line is skipped
    // with a warning; a clash keeps the default and warns.
    std::string to_text() const;
    void load_text(const std::string &text, std::vector<std::string> &warnings);
    // Warnings of saved lines applied late (their action was declared after load_text), taken once.
    std::vector<std::string> take_warnings();

private:
    std::string clash_key(const std::string &id, const KeyBinding &key) const;  // the other action's label, or ""
    std::string clash_pad(const std::string &id, const PadBinding &pad) const;
    std::vector<Action> actions_;
    std::map<std::string, std::string> unknown_lines_;
    std::vector<std::string> deferred_warnings_;  // "<id>.key" -> the line, for ids not declared now
};

// Pad hotkeys, once per vblank with the raw buttons: the actions whose combination completed (or reached its hold time) on
// this poll. A combination fires once per press; the longest combination that completes wins over the ones inside it (L+R+X
// does not also fire L+R).
class PadHotkeys {
public:
    std::vector<std::string> poll(const BindingTable &table, std::uint32_t raw);

private:
    std::uint32_t previous_ = 0;
    std::map<std::string, unsigned> held_;   // vblanks each action's combination has been down
    std::map<std::string, bool> fired_;      // already fired during this press
};

// Captures a new pad binding from what the player presses (Controls page, "Press the new combination..."): the union of the
// buttons held, done when all are released. One button held for `long_hold` vblanks or more becomes a hold binding. Gives up
// after `timeout` vblanks with nothing pressed.
class PadCapture {
public:
    explicit PadCapture(unsigned long_hold = 30, unsigned timeout = 600) : long_hold_(long_hold), timeout_(timeout) {}
    enum class State { Waiting, Done, TimedOut };
    State poll(std::uint32_t raw);
    PadBinding result() const { return result_; }

private:
    unsigned long_hold_, timeout_;
    unsigned idle_ = 0, held_ = 0;
    std::uint32_t seen_ = 0;
    PadBinding result_{};
};

}  // namespace gt::cheat_device
