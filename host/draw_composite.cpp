#include "draw_composite.hpp"

namespace gt {

void blend_rgba_over_rgb(std::vector<std::uint8_t> &rgb, const std::uint8_t *rgba, std::uint32_t width, std::uint32_t height) {
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (rgba == nullptr || rgb.size() < pixels * 3u) return;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint32_t a = rgba[i * 4u + 3u];
        if (a == 0u) continue;
        for (std::size_t c = 0; c < 3u; ++c) {
            std::uint8_t &d = rgb[i * 3u + c];
            d = static_cast<std::uint8_t>((rgba[i * 4u + c] * a + d * (255u - a) + 127u) / 255u);
        }
    }
}

} // namespace gt
