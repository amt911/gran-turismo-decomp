#include "cheat_device/bindings.hpp"

#include "pad_sequence.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <sstream>

namespace gt::cheat_device {
namespace {

std::string upper(std::string text) {
    for (char &c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

std::string trim(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

std::vector<std::string> split_plus(const std::string &text) {
    std::vector<std::string> parts;
    std::string part;
    for (const char c : text) {
        if (c == '+') {
            parts.push_back(trim(part));
            part.clear();
        } else {
            part += c;
        }
    }
    parts.push_back(trim(part));
    return parts;
}

std::string button_names(std::uint32_t buttons) {
    static const char *kNames[] = {"SELECT", "START", "UP", "RIGHT", "DOWN", "LEFT", "L", "R", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE"};
    std::string out;
    for (const char *name : kNames) {
        if ((buttons & pad_button_bits(name)) == 0u) continue;
        if (!out.empty()) out += '+';
        out += name;
    }
    return out;
}

}  // namespace

std::optional<KeyBinding> parse_key(const std::string &text, std::string &error) {
    KeyBinding binding;
    if (trim(text).empty()) return binding;
    const std::vector<std::string> parts = split_plus(text);
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string mod = upper(parts[i]);
        if (mod == "CTRL") binding.mods |= kCtrl;
        else if (mod == "SHIFT") binding.mods |= kShift;
        else if (mod == "ALT") binding.mods |= kAlt;
        else {
            error = "unknown modifier '" + parts[i] + "'";
            return std::nullopt;
        }
    }
    binding.key = parts.back();
    if (binding.key.empty()) {
        error = "no key after the modifiers";
        return std::nullopt;
    }
    return binding;
}

std::string print_key(const KeyBinding &binding) {
    if (binding.key.empty()) return "";
    std::string out;
    if (binding.mods & kCtrl) out += "Ctrl+";
    if (binding.mods & kShift) out += "Shift+";
    if (binding.mods & kAlt) out += "Alt+";
    return out + binding.key;
}

std::optional<PadBinding> parse_pad(const std::string &text, std::string &error) {
    PadBinding binding;
    std::string combo = trim(text);
    if (combo.empty()) return binding;
    if (const auto at = combo.find('@'); at != std::string::npos) {
        const std::string hold = trim(combo.substr(at + 1));
        if (hold.empty() || !std::all_of(hold.begin(), hold.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
            error = "hold time must be a number of vblanks";
            return std::nullopt;
        }
        binding.hold = static_cast<unsigned>(std::stoul(hold));
        combo = combo.substr(0, at);
    }
    for (const std::string &part : split_plus(combo)) {
        const std::uint32_t bit = pad_button_bits(upper(part));
        if (bit == 0u) {
            error = "unknown button '" + part + "'";
            return std::nullopt;
        }
        binding.buttons |= bit;
    }
    return binding;
}

std::string print_pad(const PadBinding &binding) {
    if (binding.buttons == 0u) return "";
    std::string out = button_names(binding.buttons);
    if (binding.hold != 0u) out += "@" + std::to_string(binding.hold);
    return out;
}

const Action *BindingTable::find(const std::string &id) const {
    for (const Action &a : actions_)
        if (a.id == id) return &a;
    return nullptr;
}

std::string BindingTable::clash_key(const std::string &id, const KeyBinding &key) const {
    if (key.key.empty()) return "";
    for (const Action &a : actions_)
        if (a.id != id && upper(a.key.key) == upper(key.key) && a.key.mods == key.mods) return a.label;
    return "";
}

std::string BindingTable::clash_pad(const std::string &id, const PadBinding &pad) const {
    if (pad.buttons == 0u) return "";
    for (const Action &a : actions_)
        if (a.id != id && a.pad == pad) return a.label;
    return "";
}

bool BindingTable::add_action(const std::string &id, const std::string &label, KeyBinding key, PadBinding pad, bool required,
                              std::string &error) {
    if (id.empty() || find(id) != nullptr) {
        error = id.empty() ? "empty action id" : "action '" + id + "' declared twice";
        return false;
    }
    if (const std::string other = clash_key(id, key); !other.empty()) {
        error = "default key " + print_key(key) + " is already " + other;
        return false;
    }
    if (const std::string other = clash_pad(id, pad); !other.empty()) {
        error = "default pad " + print_pad(pad) + " is already " + other;
        return false;
    }
    actions_.push_back(Action{id, label, key, pad, required, key, pad});
    // A binding the player saved before this action was declared (load_text ran first) applies now.
    std::string pending;
    for (const char *field : {".key", ".pad"}) {
        const auto it = unknown_lines_.find(id + field);
        if (it == unknown_lines_.end()) continue;
        pending += it->second + "\n";
        unknown_lines_.erase(it);
    }
    if (!pending.empty()) load_text(pending, deferred_warnings_);
    return true;
}

bool BindingTable::set_key(const std::string &id, const KeyBinding &key, std::string &error) {
    Action *action = const_cast<Action *>(find(id));
    if (action == nullptr) {
        error = "no action '" + id + "'";
        return false;
    }
    if (const std::string other = clash_key(id, key); !other.empty()) {
        error = print_key(key) + " is already " + other;
        return false;
    }
    if (action->required && key.key.empty() && action->pad.buttons == 0u) {
        error = action->label + " needs at least one binding";
        return false;
    }
    action->key = key;
    return true;
}

bool BindingTable::set_pad(const std::string &id, const PadBinding &pad, std::string &error) {
    Action *action = const_cast<Action *>(find(id));
    if (action == nullptr) {
        error = "no action '" + id + "'";
        return false;
    }
    if (std::popcount(pad.buttons) == 1 && pad.hold == 0u) {
        error = "a single button needs another one held with it, or a hold time (it already does something in a race)";
        return false;
    }
    if (const std::string other = clash_pad(id, pad); !other.empty()) {
        error = print_pad(pad) + " is already " + other;
        return false;
    }
    if (action->required && pad.buttons == 0u && action->key.key.empty()) {
        error = action->label + " needs at least one binding";
        return false;
    }
    action->pad = pad;
    return true;
}

bool BindingTable::reset(const std::string &id) {
    Action *action = const_cast<Action *>(find(id));
    if (action == nullptr) return false;
    action->key = action->default_key;
    action->pad = action->default_pad;
    return true;
}

void BindingTable::reset_all() {
    for (Action &a : actions_) {
        a.key = a.default_key;
        a.pad = a.default_pad;
    }
}

std::vector<std::string> BindingTable::take_warnings() {
    std::vector<std::string> out;
    out.swap(deferred_warnings_);
    return out;
}

const Action *BindingTable::by_key(const KeyBinding &key) const {
    if (key.key.empty()) return nullptr;
    for (const Action &a : actions_)
        if (upper(a.key.key) == upper(key.key) && a.key.mods == key.mods) return &a;
    return nullptr;
}

std::string BindingTable::to_text() const {
    std::ostringstream out;
    out << "# Cheat device hotkeys (written by the game; edit from the Controls page). \"\" = no binding.\n";
    for (const Action &a : actions_) {
        if (!(a.key == a.default_key)) out << a.id << ".key = \"" << print_key(a.key) << "\"\n";
        if (!(a.pad == a.default_pad)) out << a.id << ".pad = \"" << print_pad(a.pad) << "\"\n";
    }
    for (const auto &[name, line] : unknown_lines_) out << line << "\n";
    return out.str();
}

void BindingTable::load_text(const std::string &text, std::vector<std::string> &warnings) {
    std::istringstream in(text);
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;
        const auto eq = trimmed.find('=');
        const std::string where = "bindings line " + std::to_string(number) + ": ";
        if (eq == std::string::npos) {
            warnings.push_back(where + "no '='");
            continue;
        }
        const std::string name = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));
        if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
            warnings.push_back(where + "the value must be in quotes");
            continue;
        }
        value = value.substr(1, value.size() - 2);
        const auto dot = name.rfind('.');
        const std::string field = dot == std::string::npos ? "" : name.substr(dot);
        const std::string id = dot == std::string::npos ? name : name.substr(0, dot);
        if (field != ".key" && field != ".pad") {
            warnings.push_back(where + "expected <action>.key or <action>.pad");
            continue;
        }
        if (find(id) == nullptr) {
            unknown_lines_[name] = trimmed;
            continue;
        }
        std::string error;
        bool ok = false;
        if (field == ".key") {
            if (const auto key = parse_key(value, error)) ok = set_key(id, *key, error);
        } else if (const auto pad = parse_pad(value, error)) {
            ok = set_pad(id, *pad, error);
        }
        if (!ok) warnings.push_back(where + error + " (default kept)");
    }
}

std::vector<std::string> PadHotkeys::poll(const BindingTable &table, std::uint32_t raw) {
    std::vector<const Action *> ready;
    for (const Action &a : table.actions()) {
        if (a.pad.buttons == 0u) continue;
        const bool down = (raw & a.pad.buttons) == a.pad.buttons;
        if (!down) {
            held_[a.id] = 0;
            fired_[a.id] = false;
            continue;
        }
        const unsigned held = ++held_[a.id];
        if (fired_[a.id]) continue;
        if (a.pad.hold == 0u ? (previous_ & a.pad.buttons) != a.pad.buttons : held >= a.pad.hold) ready.push_back(&a);
    }
    previous_ = raw;
    std::vector<std::string> fired;
    for (const Action *a : ready) {
        // The longest combination wins: skip one whose buttons sit inside another ready (or already held) combination.
        const bool inner = std::any_of(table.actions().begin(), table.actions().end(), [&](const Action &b) {
            return &b != a && b.pad.buttons != a->pad.buttons && (b.pad.buttons & a->pad.buttons) == a->pad.buttons &&
                   (raw & b.pad.buttons) == b.pad.buttons;
        });
        fired_[a->id] = true;
        if (!inner) fired.push_back(a->id);
    }
    return fired;
}

PadCapture::State PadCapture::poll(std::uint32_t raw) {
    if (raw != 0u) {
        idle_ = 0;
        seen_ |= raw;
        ++held_;
        return State::Waiting;
    }
    if (seen_ != 0u) {
        result_ = PadBinding{seen_, std::popcount(seen_) == 1 && held_ >= long_hold_ ? long_hold_ : 0u};
        return State::Done;
    }
    return ++idle_ >= timeout_ ? State::TimedOut : State::Waiting;
}

}  // namespace gt::cheat_device
