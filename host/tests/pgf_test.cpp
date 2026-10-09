// Unit tests for the PGF font reader (host/pgf.cpp). The font is synthetic, written bit by bit here
// — no firmware data. Plain executable, run by CTest.
#include "pgf.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// LSB-first bit writer, the order PGF stores its bit fields in.
struct BitWriter {
    std::vector<std::uint8_t> bytes;
    std::size_t bit = 0;
    void put(std::uint32_t value, int bits) {
        for (int i = 0; i < bits; ++i, ++bit) {
            if (bit / 8u >= bytes.size()) bytes.push_back(0u);
            if ((value >> i) & 1u) bytes[bit / 8u] |= static_cast<std::uint8_t>(1u << (bit % 8u));
        }
    }
};

void put16(std::vector<std::uint8_t> &b, std::size_t at, std::uint16_t v) { std::memcpy(&b[at], &v, 2); }
void put32(std::vector<std::uint8_t> &b, std::size_t at, std::int32_t v) { std::memcpy(&b[at], &v, 4); }

// One glyph for 'A' (3x2, H rows, every metric through an index table); 'B' maps to no glyph.
std::vector<std::uint8_t> sample_font() {
    std::vector<std::uint8_t> f(392u, 0u);
    put16(f, 2, 392);
    std::memcpy(&f[4], "PGF0", 4);
    put32(f, 8, 2);        // revision
    put32(f, 16, 2);       // char map length ('A', 'B')
    put32(f, 20, 1);       // char pointer length (1 glyph)
    put32(f, 24, 8);       // char map bits per entry
    put32(f, 28, 8);       // char pointer bits per entry
    f[34] = 4;             // bpp
    std::memcpy(&f[53], "Test Sans", 9);
    put16(f, 182, 'A');    // first glyph
    put16(f, 184, 'B');    // last glyph
    put32(f, 212, 900);    // max ascender
    put32(f, 216, -200);   // max descender
    put32(f, 236, 1500);   // max advance h
    put32(f, 244, 1400);   // max size h
    put16(f, 252, 23);     // max glyph width
    put16(f, 254, 20);     // max glyph height
    f[258] = 1; f[259] = 1; f[260] = 1; f[261] = 1;  // one entry per metric table
    put32(f, 368, 16);     // shadow map bpe (no shadow map)
    auto pair = [&f](std::int32_t a, std::int32_t b) {
        const std::size_t at = f.size();
        f.resize(at + 8u);
        put32(f, at, a);
        put32(f, at + 4u, b);
    };
    pair(192, 128);   // dimension table: width, height (26.6)
    pair(64, -32);    // x adjust: H, V
    pair(640, 0);     // y adjust: H, V
    pair(1088, 1280); // advance: H, V
    BitWriter map;
    map.put(0, 8);    // 'A' -> glyph 0
    map.put(255, 8);  // 'B' -> beyond the pointer table: no glyph
    map.put(0, 16);   // pad to 32 bits
    f.insert(f.end(), map.bytes.begin(), map.bytes.end());
    BitWriter pointers;
    pointers.put(0, 8);
    pointers.put(0, 24);
    f.insert(f.end(), pointers.bytes.begin(), pointers.bytes.end());
    BitWriter glyph;
    glyph.put(0, 14);     // size
    glyph.put(3, 7);      // w
    glyph.put(2, 7);      // h
    glyph.put(1, 7);      // left
    glyph.put(126, 7);    // top = -2
    glyph.put(0x3D, 6);   // H rows | every metric indexed
    glyph.put(0, 2); glyph.put(0, 2); glyph.put(0, 3);  // shadow flags
    glyph.put(0, 9);      // shadow id
    glyph.put(0, 8); glyph.put(0, 8); glyph.put(0, 8); glyph.put(0, 8);  // dim, x, y, advance indexes
    glyph.put(1, 4); glyph.put(15, 4);                     // run: 2 x 15
    glyph.put(12, 4);                                      // 4 literals
    glyph.put(0, 4); glyph.put(8, 4); glyph.put(1, 4); glyph.put(2, 4);
    glyph.put(0, 32);
    f.insert(f.end(), glyph.bytes.begin(), glyph.bytes.end());
    return f;
}

} // namespace

int main() {
    gt::pgf::Font font;
    check(font.load(sample_font()), "synthetic font loads");
    check(font.header().first_glyph == 'A' && font.header().max_glyph_width == 23, "header fields");
    check(font.header().max_ascender == 900 && font.header().max_descender == -200, "header metrics");
    check(font.header().glyph_count == 1, "glyph count");
    const auto a = font.glyph('A');
    check(a.has_value(), "'A' has a glyph");
    if (a) {
        check(a->width == 3 && a->height == 2 && a->left == 1 && a->top == -2, "bitmap box");
        check(a->dimension_width == 192 && a->dimension_height == 128, "dimension from the table");
        check(a->x_adjust_h == 64 && a->x_adjust_v == -32, "x adjust from the table");
        check(a->y_adjust_h == 640 && a->advance_h == 1088 && a->advance_v == 1280, "y adjust and advance");
        const std::vector<std::uint8_t> expected{15, 15, 0, 8, 1, 2};
        check(font.bitmap(*a) == expected, "RLE bitmap decodes row by row");
    }
    check(!font.glyph('B').has_value(), "a char mapped past the pointer table has no glyph");
    check(!font.glyph('Z').has_value(), "a char outside the map has no glyph");
    check(!font.glyph(' ').has_value(), "a char below the first glyph has no glyph");
    auto broken = sample_font();
    broken[4] = 'X';
    gt::pgf::Font bad;
    check(!bad.load(broken), "wrong magic is rejected");
    if (failures == 0) std::puts("pgf: all tests passed");
    return failures == 0 ? 0 : 1;
}
