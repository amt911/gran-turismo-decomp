// PGF (PSP firmware font) reader for the sceLibFont HLE (host/font_hle.cpp). Pure: unit-tested in
// host/tests/pgf_test.cpp. Layout as documented by PPSSPP's PGF reader (rev 2 fonts; rev 3
// compressed char maps are not handled).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gt::pgf {

struct Header {
    std::int32_t revision{};
    std::int32_t char_map_length{};
    std::int32_t glyph_count{};       // char pointer table length
    std::uint8_t bpp{};
    std::int32_t h_size{}, v_size{}, h_resolution{}, v_resolution{};
    std::string font_name;
    std::string font_type;
    std::uint16_t first_glyph{};
    std::uint16_t last_glyph{};
    std::int32_t max_ascender{}, max_descender{}, max_left_x{}, max_base_y{}, min_center_x{}, max_top_y{};
    std::int32_t max_advance[2]{};
    std::int32_t max_size[2]{};
    std::uint16_t max_glyph_width{}, max_glyph_height{};
};

// Metrics are 26.6 fixed point, as PGF stores them.
struct Glyph {
    std::int32_t width{}, height{}, left{}, top{};
    std::uint32_t flags{};
    std::uint32_t shadow_flags{}, shadow_id{};
    std::int32_t dimension_width{}, dimension_height{};
    std::int32_t x_adjust_h{}, x_adjust_v{};
    std::int32_t y_adjust_h{}, y_adjust_v{};
    std::int32_t advance_h{}, advance_v{};
    std::uint64_t bitmap_bit{};  // where the RLE bitmap starts in the glyph data
};

class Font {
public:
    bool load(std::vector<std::uint8_t> bytes);
    const Header &header() const noexcept { return header_; }
    std::optional<Glyph> glyph(std::uint32_t code) const;
    // The glyph's pixels, row by row (width * height values 0..15).
    std::vector<std::uint8_t> bitmap(const Glyph &glyph) const;

private:
    std::vector<std::uint8_t> bytes_;
    Header header_;
    std::vector<std::int32_t> tables_[4][2];  // dimension, x adjust, y adjust, advance: [H/W, V/H]
    std::vector<std::uint32_t> char_map_;
    std::vector<std::uint32_t> char_pointers_;
    std::size_t glyph_data_{};
};

} // namespace gt::pgf
