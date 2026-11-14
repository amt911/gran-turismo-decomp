#include "ui_view.hpp"

#include <algorithm>
#include <cstdio>

namespace gt::ui {

void fill_item(lcs::HostViewRow &out, const Row &row) {
    out = lcs::HostViewRow{};
    switch (row.kind) {
    case RowKind::Text: out.kind = lcs::HostRowKind::Text; break;
    case RowKind::Toggle: out.kind = lcs::HostRowKind::Toggle; break;
    case RowKind::Choice: out.kind = lcs::HostRowKind::Choice; break;
    case RowKind::Percent: out.kind = lcs::HostRowKind::Percent; break;
    case RowKind::Chip: out.kind = lcs::HostRowKind::Chip; break;
    case RowKind::Submenu: out.kind = lcs::HostRowKind::Submenu; break;
    case RowKind::Pill: out.kind = lcs::HostRowKind::Pill; break;
    }
    switch (row.tone) {
    case Tone::Grey: out.tone = lcs::HostPillTone::Grey; break;
    case Tone::Green: out.tone = lcs::HostPillTone::Green; break;
    case Tone::Amber: out.tone = lcs::HostPillTone::Amber; break;
    case Tone::Red: out.tone = lcs::HostPillTone::Red; break;
    }
    out.on = row.on;
    out.percent = row.percent;
    std::snprintf(out.label, sizeof out.label, "%s", row.label.c_str());
    std::snprintf(out.value, sizeof out.value, "%s", row.value.c_str());
    std::snprintf(out.hint, sizeof out.hint, "%s", row.hint.c_str());
}

void fill_view(lcs::HostSettingsView &view, const cheat_device::View &v) {
    // The 5x7 fallback draws title / rows / footer; the styled look draws the rest.
    std::snprintf(view.title, sizeof view.title, "%s", v.title.c_str());
    std::snprintf(view.footer, sizeof view.footer, "%s", v.footer.c_str());
    view.row_count = static_cast<int>(std::min<std::size_t>(v.rows.size(), lcs::kViewRows));
    view.selected = v.selected;
    for (int i = 0; i < view.row_count; ++i) {
        std::snprintf(view.rows[i], sizeof view.rows[i], "%s", v.rows[static_cast<std::size_t>(i)].c_str());
        if (static_cast<std::size_t>(i) < v.items.size()) fill_item(view.items[i], v.items[static_cast<std::size_t>(i)]);
    }
    view.styled = v.items.size() == v.rows.size();
    std::snprintf(view.title, sizeof view.title, "%s", view.styled ? "CHEAT DEVICE" : v.title.c_str());
    std::snprintf(view.crumb, sizeof view.crumb, "%s", v.crumb.c_str());
    // A refusal reads "Refused: <reason>" (a failed save already says what it is).
    const bool refused = v.message_is_error && v.message.rfind("NOT SAVED", 0) != 0;
    std::snprintf(view.message, sizeof view.message, "%s%s", refused ? "Refused: " : "", v.message.c_str());
    std::snprintf(view.binding, sizeof view.binding, "%s", v.binding.c_str());
    view.message_is_error = v.message_is_error;
    view.capturing = v.capturing;
    view.position = v.position;
    view.count = v.count;
}

}  // namespace gt::ui
