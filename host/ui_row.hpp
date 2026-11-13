// Host UI rows (lcs patch 0069): what the cheat device, the F9 MODS page and the F10 page draw, as data instead of a 40-character
// string. Pure logic (no lcs, no SDL): host/ui_view.cpp copies it into the overlay's structs; CTest drives it.
//
// Mods (API v4) hand the device a finished text row ("Slow motion  ON", "Grip  70%", "Weather  Rain"), so the kind is inferred
// from the text: ON / OFF is a toggle, NN% a percentage bar, anything else a choice (when the row can be adjusted) or plain text.
#pragma once

#include <string>

namespace gt::ui {

enum class RowKind { Text, Toggle, Choice, Percent, Chip, Submenu, Pill };
enum class Tone { Grey, Green, Amber, Red };

struct Row {
    RowKind kind = RowKind::Text;
    Tone tone = Tone::Grey;
    bool on = false;
    int percent = 0;
    std::string label, value, hint;
};

// A finished text row. `adjustable`: Left / Right change it (a value that is not ON / OFF / NN% is then a choice, drawn with
// arrows). Mod-list rows ("name  ON", "name  ERROR reason", "name 1.2.0 UPDATE 1.0") become toggles and pills with a hint line.
Row from_text(const std::string &text, bool adjustable);

// `label` + `value` with the kind inferred from the value (a device entry of kind Toggle / Choice / Number).
Row from_value(const std::string &label, const std::string &value, bool adjustable);

// A binding as printed by the bindings table ("L+TRIANGLE", "Ctrl+N", "F7") for a chip: "L + Triangle", "Ctrl + N", "F7".
std::string pretty_binding(const std::string &text);
// The same in capitals for the footer: "SELECT + START".
std::string upper_binding(const std::string &text);

}  // namespace gt::ui
