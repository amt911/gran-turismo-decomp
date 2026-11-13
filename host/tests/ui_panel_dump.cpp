// Writes the host UI as PNGs for a look and for the PR: the cheat device (Race page; Controls with a refusal and a capture
// waiting), the F9 MODS page and the F10 settings page, from the real Device / ui_row / lcs overlay code. The images hold only
// our own UI text on a flat grey backdrop (the panel is translucent), never game pixels. Usage: ui_panel_dump <dir>
#include "cheat_device/device.hpp"
#include "lcs_display_menu.hpp"
#include "lcs_host_style.hpp"
#include "ui_row.hpp"
#include "ui_view.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace lcs {
void display_window_request_fullscreen(bool) noexcept {}
}

namespace {

using namespace gt::cheat_device;
constexpr std::uint32_t kTriangle = 0x1000u;

std::uint32_t crc32(const std::uint8_t *data, std::size_t n, std::uint32_t crc = 0) {
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

void put32(std::vector<std::uint8_t> &out, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<std::uint8_t>(v >> s));
}

void chunk(std::vector<std::uint8_t> &png, const char *type, const std::vector<std::uint8_t> &body) {
    put32(png, static_cast<std::uint32_t>(body.size()));
    std::vector<std::uint8_t> typed(type, type + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    png.insert(png.end(), typed.begin(), typed.end());
    put32(png, crc32(typed.data(), typed.size()));
}

// RGB PNG, stored (uncompressed) deflate blocks: no zlib needed for a screenshot.
bool write_png(const std::filesystem::path &path, const std::vector<std::uint8_t> &rgb, int w, int h) {
    std::vector<std::uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + static_cast<std::size_t>(y) * w * 3, rgb.begin() + static_cast<std::size_t>(y + 1) * w * 3);
    }
    std::vector<std::uint8_t> z{0x78, 0x01};
    for (std::size_t at = 0; at < raw.size();) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - at);
        z.push_back(at + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n));
        z.push_back(static_cast<std::uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + n));
        at += n;
    }
    std::uint32_t a = 1, b = 0;
    for (const std::uint8_t byte : raw) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    put32(z, (b << 16) | a);
    std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr;
    put32(ihdr, static_cast<std::uint32_t>(w));
    put32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::FILE *file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
    std::fclose(file);
    return ok;
}

// The panel over a flat blue-grey backdrop (it is translucent in the game), cropped to its height.
bool dump(const std::filesystem::path &path, const lcs::HostSettingsView &view) {
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(lcs::kSettingsOverlayWidth) * lcs::kSettingsOverlayHeight * 4u);
    lcs::rasterize_settings_overlay(view, rgba.data(), lcs::kSettingsOverlayWidth, lcs::kSettingsOverlayHeight);
    const int w = static_cast<int>(lcs::kSettingsOverlayWidth) + 48;
    const int h = (lcs::host_style::layout(view).height * lcs::host_style::kScale) + 48;
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * h * 3u);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            std::uint8_t *out = &rgb[(static_cast<std::size_t>(y) * w + x) * 3u];
            const int shade = 70 + (y * 40) / h;  // a calm gradient, so the translucency reads
            out[0] = static_cast<std::uint8_t>(shade - 10);
            out[1] = static_cast<std::uint8_t>(shade + 6);
            out[2] = static_cast<std::uint8_t>(shade + 24);
            const int px = x - 24, py = y - 24;
            if (px < 0 || py < 0 || px >= static_cast<int>(lcs::kSettingsOverlayWidth) || py >= static_cast<int>(lcs::kSettingsOverlayHeight)) continue;
            const std::uint8_t *p = &rgba[(static_cast<std::size_t>(py) * lcs::kSettingsOverlayWidth + px) * 4u];
            for (int c = 0; c < 3; ++c) out[c] = static_cast<std::uint8_t>((p[c] * p[3] + out[c] * (255 - p[3])) / 255);
        }
    return write_png(path, rgb, w, h);
}

struct Mods {
    std::vector<std::string> rows{"Analog pedals                ON", "Cheats                       OFF"};
    ModRows source() {
        return ModRows{[this] { return rows.size(); }, [this](std::size_t i) { return rows[i]; }, [](std::size_t, int) { return true; }};
    }
};

lcs::HostSettingsView panel_view(Device &device, std::uint64_t now) {
    lcs::HostSettingsView view{};
    gt::ui::fill_view(view, device.view(lcs::kViewRows, now));
    view.panel = true;
    return view;
}

const char *const kMods[] = {"Analog pedals   ON", "Cheat device   ON", "Draw distance   OFF", "Old camera ERROR no entry point",
                             "GET MODS  3 AVAILABLE", "Replay cam 1.2.0 UPDATE 1.0.0", "Ghost cars 0.9.1 INSTALL", "Pedals+ 2.0.0 UP TO DATE"};

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const std::filesystem::path dir = argv[1];
    std::filesystem::create_directories(dir);
    int bad = 0;
    const auto emit = [&](const char *name, const lcs::HostSettingsView &view) {
        if (!dump(dir / name, view)) {
            std::fprintf(stderr, "could not write %s\n", name);
            ++bad;
        }
    };

    Mods mods;
    Device device(mods.source());
    std::string error;
    int slow = 0, weather = 0;
    bool freeze = true;
    const char *kSlow[] = {"OFF", "0.5x", "0.25x"};
    const char *kWeather[] = {"Clear", "Rain", "Dusk"};
    int grip = 70;
    Entry race{"", EntryKind::Choice, "Slow motion", [&] { return std::string(kSlow[slow]); }, [&](int d) { slow = (slow + (d < 0 ? 2 : 1)) % 3; }, {}};
    device.add_entry("Race", race);
    device.add_entry("Race", Entry{"", EntryKind::Toggle, "Freeze AI", [&] { return std::string(freeze ? "ON" : "OFF"); }, [&](int) { freeze = !freeze; }, {}});
    device.add_entry("Race", Entry{"", EntryKind::Row, "", [&] { return "Grip                 " + std::to_string(grip) + "%"; }, [&](int d) { grip += d * 10; }, {}});
    device.add_entry("Race", Entry{"", EntryKind::Choice, "Weather", [&] { return std::string(kWeather[weather]); }, [&](int) { weather = (weather + 1) % 3; }, {}});
    device.add_action("cheats.nitro", "Nitro", "Race", KeyBinding{"N", kCtrl}, PadBinding{gt::kPadL | kTriangle, 0}, false, error);
    device.add_action("cheats.repair", "Repair car", "Race", KeyBinding{"R", kCtrl}, PadBinding{}, false, error);
    device.open();
    device.handle_key(gt::MenuKey::Close, 1);     // root
    device.handle_key(gt::MenuKey::Activate, 1);  // Race
    device.handle_key(gt::MenuKey::Down, 1);
    emit("device_race.png", panel_view(device, 1));

    // Controls > Nitro > Pad: a capture, a refused combination, a capture waiting again
    device.handle_key(gt::MenuKey::Close, 2);
    device.handle_key(gt::MenuKey::Down, 2);
    device.handle_key(gt::MenuKey::Down, 2);
    device.handle_key(gt::MenuKey::Activate, 2);  // Controls
    device.handle_key(gt::MenuKey::Down, 2);
    device.handle_key(gt::MenuKey::Activate, 2);  // Nitro's page
    device.handle_key(gt::MenuKey::Down, 2);      // Pad
    device.handle_key(gt::MenuKey::Activate, 2);
    device.poll_pad(gt::kPadCross, 2);
    device.poll_pad(gt::kPadSelect | gt::kPadStart, 3);
    device.poll_pad(0, 4);  // refused: SELECT+START opens the device
    device.handle_key(gt::MenuKey::Activate, 5);  // capture again, the refusal still shown
    emit("device_controls_capture.png", panel_view(device, 5));

    // F9 MODS and F10 settings share one registration, as in the game.
    static const lcs::HostExtraPage page{"MODS", []() -> std::size_t { return std::size(kMods); },
                                         [](std::size_t i, char *out, std::size_t cap) { std::snprintf(out, cap, "%s", kMods[i]); },
                                         [](std::size_t) { return true; }, nullptr, nullptr, nullptr, false,
                                         [](std::size_t i, lcs::HostViewRow &out) { gt::ui::fill_item(out, gt::ui::from_text(kMods[i], false)); }};
    lcs::host_settings_set_extra_page(&page);
    lcs::host_settings_handle(lcs::HostSettingsKey::TogglePage);
    lcs::host_settings_handle(lcs::HostSettingsKey::Down);
    emit("f9_mods.png", lcs::host_settings_view());
    lcs::host_settings_handle(lcs::HostSettingsKey::Close);
    lcs::host_settings_handle(lcs::HostSettingsKey::Toggle);
    emit("f10_settings.png", lcs::host_settings_view());
    for (const char *name : {"device_race.png", "device_controls_capture.png", "f9_mods.png", "f10_settings.png"})
        if (!std::filesystem::exists(dir / name) || std::filesystem::file_size(dir / name) < 10000) {
            std::fprintf(stderr, "%s missing or empty\n", name);
            ++bad;
        }
    return bad == 0 ? 0 : 1;
}
