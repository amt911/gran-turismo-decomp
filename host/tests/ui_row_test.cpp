#include "ui_row.hpp"

#include <cstdio>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}
using namespace gt::ui;
}  // namespace

int main() {
    Row r = from_text("Analog pedals            ON", false);
    check(r.kind == RowKind::Toggle && r.on && r.label == "Analog pedals", "ON is a toggle");
    r = from_text("Cheats OFF", false);
    check(r.kind == RowKind::Toggle && !r.on && r.label == "Cheats", "OFF is a toggle");
    r = from_text("Cheats ON SAVE FAILED", false);
    check(r.kind == RowKind::Toggle && r.on && r.hint == "Could not save", "a failed save is a hint");
    r = from_text("Grip   70%", true);
    check(r.kind == RowKind::Percent && r.percent == 70 && r.value == "70%", "NN% is a bar");
    r = from_text("Boost 250%", true);
    check(r.percent == 100, "the bar saturates at 100");
    r = from_text("Weather   Rain", true);
    check(r.kind == RowKind::Choice && r.value == "Rain", "an adjustable value is a choice");
    r = from_text("Weather   Rain", false);
    check(r.kind == RowKind::Text && r.value == "Rain", "a value that cannot be adjusted is text");
    r = from_text("Broken mod ERROR no entry point", false);
    check(r.kind == RowKind::Pill && r.tone == Tone::Red && r.value == "ERROR" && r.hint == "no entry point" && r.label == "Broken mod",
          "a loader error is a red pill with the reason");
    r = from_text("Nice mod 1.2.0 UPDATE 1.0.0", false);
    check(r.kind == RowKind::Pill && r.tone == Tone::Amber && r.value == "UPDATE" && r.label == "Nice mod" &&
              r.hint == "v1.2.0, installed 1.0.0",
          "a store row: name, version hint, state pill");
    r = from_text("Nice mod 1.2.0 UP TO DATE", false);
    check(r.tone == Tone::Green && r.value == "UP TO DATE", "up to date is green");
    r = from_text("Nice mod 1.2.0 INSTALL", false);
    check(r.tone == Tone::Amber && r.value == "INSTALL", "install is amber");
    r = from_text("GET MODS  ERROR offline", false);
    check(r.kind == RowKind::Pill && r.tone == Tone::Red && r.label == "GET MODS" && r.hint == "offline", "GET MODS error");
    r = from_text("GET MODS  3 AVAILABLE", false);
    check(r.kind == RowKind::Text && r.label == "GET MODS" && r.value == "3 AVAILABLE", "GET MODS count");
    r = from_text("NO MODS INSTALLED", false);
    check(r.kind == RowKind::Text && r.label == "NO MODS INSTALLED" && r.value.empty(), "no mods");
    check(pretty_binding("L+TRIANGLE") == "L + Triangle", "pad chip");
    check(pretty_binding("Ctrl+N") == "Ctrl + N", "key chip");
    check(pretty_binding("F7") == "F7", "a lone key");
    check(pretty_binding("L+TRIANGLE@30") == "L + Triangle (hold)", "a hold");
    check(upper_binding("SELECT+START") == "SELECT + START", "footer binding");
    Row v = from_value("Slow motion", "0.5x", true);
    check(v.kind == RowKind::Choice, "choice from a value");
    check(from_value("Freeze AI", "ON", true).kind == RowKind::Toggle && from_value("Freeze AI", "ON", true).on &&
              from_value("Freeze AI", "OFF", false).kind == RowKind::Toggle && !from_value("Freeze AI", "OFF", false).on,
          "an entry's ON / OFF value is a toggle");
    check(from_value("Grip", "30%", true).kind == RowKind::Percent && from_value("Grip", "30%", true).percent == 30, "an entry's NN% value is a bar");
    check(from_value("A", "", true).kind == RowKind::Text, "no value, no choice");
    if (failures == 0) std::puts("ui_row: all tests passed");
    return failures == 0 ? 0 : 1;
}
