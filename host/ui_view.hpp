// Copies the host's UI data (host/ui_row.hpp, the cheat device's View) into the lcs overlay's structs (lcs patch 0069).
#pragma once

#include "cheat_device/device.hpp"
#include "lcs_display_menu.hpp"
#include "ui_row.hpp"

namespace gt::ui {

void fill_item(lcs::HostViewRow &out, const Row &row);
// The panel's whole view: styled rows, crumb, position, message, binding, capture flag, and the text form for the 5x7 fallback.
void fill_view(lcs::HostSettingsView &view, const cheat_device::View &v);

}  // namespace gt::ui
