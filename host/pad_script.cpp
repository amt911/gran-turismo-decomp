// GT_PAD="<vblank>:<BUTTON>[:<frames>][,...]" (e.g. "2700:START,3000:CROSS:600"): scripted PSP pad
// presses, each held for <frames> vblank waits (default 6), counted from the first one. The kernel
// ORs them into the host's input, so they work in gt_native (no other input) and gt_play (added to
// the keyboard/gamepad).
#include "pad_script.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace gt {
namespace {

// Vblank waits so far: ticked by the main thread, read by frame dumps on the GE worker thread too.
std::atomic<std::uint64_t> g_vblanks{0u};

struct PadPress {
    std::uint64_t vblank;
    std::uint32_t buttons;
    std::uint64_t frames;  // how long it is held (default 6)
};

std::uint32_t pad_button(const std::string &name) {
    static const std::pair<const char *, std::uint32_t> kButtons[] = {
        {"SELECT", 0x0001u}, {"START", 0x0008u}, {"UP", 0x0010u}, {"RIGHT", 0x0020u},
        {"DOWN", 0x0040u}, {"LEFT", 0x0080u}, {"L", 0x0100u}, {"R", 0x0200u},
        {"TRIANGLE", 0x1000u}, {"CIRCLE", 0x2000u}, {"CROSS", 0x4000u}, {"SQUARE", 0x8000u},
    };
    for (const auto &[label, bit] : kButtons)
        if (name == label) return bit;
    std::fprintf(stderr, "GT_PAD: unknown button '%s'\n", name.c_str());
    return 0u;
}

const std::vector<PadPress> &pad_script() {
    static const std::vector<PadPress> script = [] {
        std::vector<PadPress> presses;
        const char *text = std::getenv("GT_PAD");
        if (text == nullptr) return presses;
        std::string item;
        for (const char *c = text;; ++c) {
            if (*c == ',' || *c == '\0') {
                const auto colon = item.find(':');
                if (colon != std::string::npos) {  // <vblank>:<BUTTON>[:<frames held>]
                    const std::string rest = item.substr(colon + 1u);
                    const auto second = rest.find(':');
                    const std::uint64_t frames = second == std::string::npos ? 6u : std::strtoull(rest.c_str() + second + 1u, nullptr, 10);
                    presses.push_back({std::strtoull(item.c_str(), nullptr, 10), pad_button(rest.substr(0u, second)), frames});
                }
                item.clear();
                if (*c == '\0') break;
            } else {
                item.push_back(*c);
            }
        }
        return presses;
    }();
    return script;
}

} // namespace

void pad_script_tick() { g_vblanks.fetch_add(1u, std::memory_order_relaxed); }

std::uint64_t pad_script_vblanks() { return g_vblanks.load(std::memory_order_relaxed); }

std::uint32_t scripted_buttons() {
    std::uint32_t buttons = 0u;
    const std::uint64_t now = g_vblanks.load(std::memory_order_relaxed);
    for (const PadPress &press : pad_script())
        if (now >= press.vblank && now < press.vblank + press.frames) buttons |= press.buttons;
    return buttons;
}

} // namespace gt
