#include "pgf.hpp"

#include <algorithm>
#include <cstring>

namespace gt::pgf {
namespace {

constexpr std::size_t kHeaderSize = 392u;
constexpr std::uint32_t kFlagRowsMask = 0x03u;
constexpr std::uint32_t kFlagHorizontalRows = 0x01u;
constexpr std::uint32_t kFlagDimensionIndex = 0x04u;
constexpr std::uint32_t kFlagXAdjustIndex = 0x08u;
constexpr std::uint32_t kFlagYAdjustIndex = 0x10u;
constexpr std::uint32_t kFlagAdvanceIndex = 0x20u;

template <typename T> T read(const std::vector<std::uint8_t> &b, std::size_t at) {
    T value{};
    std::memcpy(&value, &b[at], sizeof value);
    return value;
}

std::string read_name(const std::vector<std::uint8_t> &b, std::size_t at) {
    std::string name;
    for (std::size_t i = 0; i < 64u && b[at + i] != 0u; ++i) name.push_back(static_cast<char>(b[at + i]));
    return name;
}

// LSB-first bit fields; reads past the end yield zeros.
struct BitReader {
    const std::vector<std::uint8_t> &bytes;
    std::uint64_t bit;
    std::uint32_t take(int count) {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i, ++bit) {
            const std::uint64_t byte = bit / 8u;
            if (byte < bytes.size() && ((bytes[byte] >> (bit % 8u)) & 1u) != 0u) value |= 1u << i;
        }
        return value;
    }
};

std::size_t packed_bytes(std::int64_t entries, std::int64_t bits) {
    return static_cast<std::size_t>(((entries * bits + 31) & ~std::int64_t{31}) / 8);
}

} // namespace

bool Font::load(std::vector<std::uint8_t> bytes) {
    bytes_ = std::move(bytes);
    const auto &b = bytes_;
    if (b.size() < kHeaderSize || std::memcmp(&b[4], "PGF0", 4) != 0) return false;
    Header h;
    h.revision = read<std::int32_t>(b, 8);
    h.char_map_length = read<std::int32_t>(b, 16);
    h.glyph_count = read<std::int32_t>(b, 20);
    const auto map_bpe = read<std::int32_t>(b, 24);
    const auto pointer_bpe = read<std::int32_t>(b, 28);
    h.bpp = b[34];
    h.h_size = read<std::int32_t>(b, 36);
    h.v_size = read<std::int32_t>(b, 40);
    h.h_resolution = read<std::int32_t>(b, 44);
    h.v_resolution = read<std::int32_t>(b, 48);
    h.font_name = read_name(b, 53);
    h.font_type = read_name(b, 117);
    h.first_glyph = read<std::uint16_t>(b, 182);
    h.last_glyph = read<std::uint16_t>(b, 184);
    h.max_ascender = read<std::int32_t>(b, 212);
    h.max_descender = read<std::int32_t>(b, 216);
    h.max_left_x = read<std::int32_t>(b, 220);
    h.max_base_y = read<std::int32_t>(b, 224);
    h.min_center_x = read<std::int32_t>(b, 228);
    h.max_top_y = read<std::int32_t>(b, 232);
    h.max_advance[0] = read<std::int32_t>(b, 236);
    h.max_advance[1] = read<std::int32_t>(b, 240);
    h.max_size[0] = read<std::int32_t>(b, 244);
    h.max_size[1] = read<std::int32_t>(b, 248);
    h.max_glyph_width = read<std::uint16_t>(b, 252);
    h.max_glyph_height = read<std::uint16_t>(b, 254);
    const auto shadow_length = read<std::int32_t>(b, 364);
    const auto shadow_bpe = read<std::int32_t>(b, 368);
    if (h.revision == 3 || h.char_map_length < 0 || h.glyph_count < 0 || map_bpe <= 0 || map_bpe > 32 ||
        pointer_bpe <= 0 || pointer_bpe > 32 || shadow_length < 0)
        return false;

    std::size_t at = read<std::uint16_t>(b, 2);  // header size
    for (int table = 0; table < 4; ++table) {
        const std::size_t length = b[258u + static_cast<std::size_t>(table)];
        if (at + length * 8u > b.size()) return false;
        for (std::size_t i = 0; i < length; ++i, at += 8u) {
            tables_[table][0].push_back(read<std::int32_t>(b, at));
            tables_[table][1].push_back(read<std::int32_t>(b, at + 4u));
        }
    }
    at += packed_bytes(shadow_length, shadow_bpe);
    const std::size_t map_bytes = packed_bytes(h.char_map_length, map_bpe);
    const std::size_t pointer_bytes = packed_bytes(h.glyph_count, pointer_bpe);
    if (at + map_bytes + pointer_bytes > b.size()) return false;
    BitReader map{b, static_cast<std::uint64_t>(at) * 8u};
    for (std::int32_t i = 0; i < h.char_map_length; ++i) char_map_.push_back(map.take(map_bpe));
    at += map_bytes;
    BitReader pointers{b, static_cast<std::uint64_t>(at) * 8u};
    for (std::int32_t i = 0; i < h.glyph_count; ++i) char_pointers_.push_back(pointers.take(pointer_bpe));
    at += pointer_bytes;
    glyph_data_ = at;
    header_ = std::move(h);
    return true;
}

std::optional<Glyph> Font::glyph(std::uint32_t code) const {
    if (code < header_.first_glyph) return std::nullopt;
    const std::uint32_t slot = code - header_.first_glyph;
    if (slot >= char_map_.size() || char_map_[slot] >= char_pointers_.size()) return std::nullopt;
    BitReader in{bytes_, static_cast<std::uint64_t>(glyph_data_) * 8u +
                             static_cast<std::uint64_t>(char_pointers_[char_map_[slot]]) * 32u};
    if (in.bit + 96u > bytes_.size() * 8u) return std::nullopt;
    Glyph g;
    (void)in.take(14);  // block size
    g.width = static_cast<std::int32_t>(in.take(7));
    g.height = static_cast<std::int32_t>(in.take(7));
    g.left = static_cast<std::int32_t>(in.take(7));
    if (g.left >= 64) g.left -= 128;
    g.top = static_cast<std::int32_t>(in.take(7));
    if (g.top >= 64) g.top -= 128;
    g.flags = in.take(6);
    g.shadow_flags = in.take(2) << 5u;
    g.shadow_flags |= in.take(2) << 3u;
    g.shadow_flags |= in.take(3);
    g.shadow_id = in.take(9);
    const auto metric = [this, &in](std::uint32_t flags, std::uint32_t indexed, int table, std::int32_t &first,
                                    std::int32_t &second) {
        if ((flags & indexed) != 0u) {
            const std::size_t index = in.take(8);
            first = index < tables_[table][0].size() ? tables_[table][0][index] : 0;
            second = index < tables_[table][1].size() ? tables_[table][1][index] : 0;
        } else {
            first = static_cast<std::int32_t>(in.take(32));
            second = static_cast<std::int32_t>(in.take(32));
        }
    };
    metric(g.flags, kFlagDimensionIndex, 0, g.dimension_width, g.dimension_height);
    metric(g.flags, kFlagXAdjustIndex, 1, g.x_adjust_h, g.x_adjust_v);
    metric(g.flags, kFlagYAdjustIndex, 2, g.y_adjust_h, g.y_adjust_v);
    metric(g.flags, kFlagAdvanceIndex, 3, g.advance_h, g.advance_v);
    g.bitmap_bit = in.bit;
    return g;
}

std::vector<std::uint8_t> Font::bitmap(const Glyph &glyph) const {
    const std::size_t pixels = static_cast<std::size_t>(std::max(0, glyph.width) * std::max(0, glyph.height));
    std::vector<std::uint8_t> out(pixels, 0u);
    const bool rows = (glyph.flags & kFlagRowsMask) == kFlagHorizontalRows;
    BitReader in{bytes_, glyph.bitmap_bit};
    std::size_t index = 0;
    // Nibble RLE: n < 8 repeats the next value n + 1 times; n >= 8 is followed by 16 - n values.
    while (index < pixels && in.bit + 8u < bytes_.size() * 8u) {
        const std::uint32_t nibble = in.take(4);
        const std::uint32_t count = nibble < 8u ? nibble + 1u : 16u - nibble;
        std::uint32_t value = nibble < 8u ? in.take(4) : 0u;
        for (std::uint32_t i = 0; i < count && index < pixels; ++i, ++index) {
            if (nibble >= 8u) value = in.take(4);
            const std::size_t x = rows ? index % static_cast<std::size_t>(glyph.width) : index / static_cast<std::size_t>(glyph.height);
            const std::size_t y = rows ? index / static_cast<std::size_t>(glyph.width) : index % static_cast<std::size_t>(glyph.height);
            out[y * static_cast<std::size_t>(glyph.width) + x] = static_cast<std::uint8_t>(value);
        }
    }
    return out;
}

} // namespace gt::pgf
