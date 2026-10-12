// Small image helpers for the frame dumps: see image_ppm.hpp.
#include "image_ppm.hpp"

#include <cstdio>

namespace gt {

// Each output pixel averages the source pixels its box covers: columns [floor(ox*w/ow),
// ceil((ox+1)*w/ow)) and the same for rows, rounded to nearest. Alpha is dropped.
std::vector<std::uint8_t> downsample_rgba_box(std::span<const std::byte> rgba, std::uint32_t width, std::uint32_t height,
                                              std::uint32_t out_width, std::uint32_t out_height) {
    if (width == 0u || height == 0u || out_width == 0u || out_height == 0u ||
        rgba.size() < static_cast<std::size_t>(width) * height * 4u)
        return {};
    std::vector<std::uint8_t> out(static_cast<std::size_t>(out_width) * out_height * 3u);
    for (std::uint32_t oy = 0; oy < out_height; ++oy) {
        const std::uint64_t y0 = static_cast<std::uint64_t>(oy) * height / out_height;
        const std::uint64_t y1 = (static_cast<std::uint64_t>(oy + 1u) * height + out_height - 1u) / out_height;
        for (std::uint32_t ox = 0; ox < out_width; ++ox) {
            const std::uint64_t x0 = static_cast<std::uint64_t>(ox) * width / out_width;
            const std::uint64_t x1 = (static_cast<std::uint64_t>(ox + 1u) * width + out_width - 1u) / out_width;
            std::uint64_t sum[3] = {0u, 0u, 0u};
            for (std::uint64_t y = y0; y < y1; ++y)
                for (std::uint64_t x = x0; x < x1; ++x)
                    for (int c = 0; c < 3; ++c) sum[c] += std::to_integer<std::uint8_t>(rgba[(y * width + x) * 4u + c]);
            const std::uint64_t count = (y1 - y0) * (x1 - x0);
            for (int c = 0; c < 3; ++c)
                out[(static_cast<std::size_t>(oy) * out_width + ox) * 3u + c] = static_cast<std::uint8_t>((sum[c] + count / 2u) / count);
        }
    }
    return out;
}

bool write_rgb_ppm(const std::filesystem::path &path, std::span<const std::uint8_t> rgb, std::uint32_t width,
                   std::uint32_t height) {
    if (rgb.size() != static_cast<std::size_t>(width) * height * 3u) return false;
    std::FILE *out = std::fopen(path.c_str(), "wb");
    if (out == nullptr) return false;
    std::fprintf(out, "P6\n%u %u\n255\n", width, height);
    const bool written = std::fwrite(rgb.data(), 1u, rgb.size(), out) == rgb.size();
    return std::fclose(out) == 0 && written;
}

} // namespace gt
