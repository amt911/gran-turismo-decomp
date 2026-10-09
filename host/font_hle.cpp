// sceLibFont HLE: GT renders its text atlas at boot from the PSP firmware fonts (PGF). Glyphs come
// from the user's own PGF files (GT_FONT_DIR, default: PPSSPP's flash0/font), never from the repo.
// Font indexes, styles and return values follow what PPSSPP reports for this game; structures as
// documented by PPSSPP's sceFont.

#include "font_hle.hpp"
#include "pgf.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <unordered_map>

namespace gt::font {
namespace {

constexpr std::uint32_t kErrorInvalidParameter = 0x80460003u;
constexpr std::uint32_t kErrorHandlerOpenFailed = 0x80460005u;

constexpr std::uint16_t kSansSerif = 1u, kSerif = 2u;
constexpr std::uint16_t kRegular = 1u, kItalic = 2u, kBold = 5u, kBoldItalic = 6u, kDemiBold = 103u;
constexpr std::uint16_t kJapanese = 1u, kLatin = 2u, kKorean = 3u;

struct Entry {
    const char *file;
    std::uint16_t family, style, language;
    std::int32_t size;  // h and v, 26.6
    const char *name;
};

// The firmware font list, in the index order sceFontOpen uses.
constexpr std::array<Entry, 18> kFonts{{
    {"jpn0.pgf", kSansSerif, kDemiBold, kJapanese, 0x288, "FTT-NewRodin Pro DB"},
    {"ltn0.pgf", kSansSerif, kRegular, kLatin, 0x288, "FTT-NewRodin Pro Latin"},
    {"ltn1.pgf", kSerif, kRegular, kLatin, 0x288, "FTT-Matisse Pro Latin"},
    {"ltn2.pgf", kSansSerif, kItalic, kLatin, 0x288, "FTT-NewRodin Pro Latin"},
    {"ltn3.pgf", kSerif, kItalic, kLatin, 0x288, "FTT-Matisse Pro Latin"},
    {"ltn4.pgf", kSansSerif, kBold, kLatin, 0x288, "FTT-NewRodin Pro Latin"},
    {"ltn5.pgf", kSerif, kBold, kLatin, 0x288, "FTT-Matisse Pro Latin"},
    {"ltn6.pgf", kSansSerif, kBoldItalic, kLatin, 0x288, "FTT-NewRodin Pro Latin"},
    {"ltn7.pgf", kSerif, kBoldItalic, kLatin, 0x288, "FTT-Matisse Pro Latin"},
    {"ltn8.pgf", kSansSerif, kRegular, kLatin, 0x1C0, "FTT-NewRodin Pro Latin"},
    {"ltn9.pgf", kSerif, kRegular, kLatin, 0x1C0, "FTT-Matisse Pro Latin"},
    {"ltn10.pgf", kSansSerif, kItalic, kLatin, 0x1C0, "FTT-NewRodin Pro Latin"},
    {"ltn11.pgf", kSerif, kItalic, kLatin, 0x1C0, "FTT-Matisse Pro Latin"},
    {"ltn12.pgf", kSansSerif, kBold, kLatin, 0x1C0, "FTT-NewRodin Pro Latin"},
    {"ltn13.pgf", kSerif, kBold, kLatin, 0x1C0, "FTT-Matisse Pro Latin"},
    {"ltn14.pgf", kSansSerif, kBoldItalic, kLatin, 0x1C0, "FTT-NewRodin Pro Latin"},
    {"ltn15.pgf", kSerif, kBoldItalic, kLatin, 0x1C0, "FTT-Matisse Pro Latin"},
    {"kr0.pgf", kSansSerif, kRegular, kKorean, 0x288, "AsiaNHH(512Johab)"},
}};

std::filesystem::path font_directory() {
    const char *dir = std::getenv("GT_FONT_DIR");
    return dir != nullptr ? std::filesystem::path(dir) : std::filesystem::path("/usr/share/ppsspp/assets/flash0/font");
}

// PGF files are parsed once, on first use.
const pgf::Font *load_font(std::size_t index) {
    static std::array<std::unique_ptr<pgf::Font>, kFonts.size()> fonts;
    static std::array<bool, kFonts.size()> tried{};
    if (index >= kFonts.size()) return nullptr;
    if (!tried[index]) {
        tried[index] = true;
        const auto path = font_directory() / kFonts[index].file;
        std::ifstream in(path, std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        auto font = std::make_unique<pgf::Font>();
        if (!bytes.empty() && font->load(std::move(bytes))) fonts[index] = std::move(font);
        else std::cerr << "[font] cannot load " << path << " (set GT_FONT_DIR to a flash0/font directory)\n";
    }
    return fonts[index].get();
}

struct OpenFont {
    std::size_t index;
    std::uint32_t library;
};

std::unordered_map<std::uint32_t, OpenFont> g_fonts;              // handle -> font
std::unordered_map<std::uint32_t, std::uint32_t> g_alt_char;     // library -> alternate char
std::uint32_t g_next_handle = 0x0BF00000u;                        // opaque handles, never dereferenced

void store_float(psprecomp::GuestMemory &memory, std::uint32_t address, float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, 4);
    memory.store32(address, bits);
}

void write_style(psprecomp::GuestMemory &memory, std::uint32_t at, const Entry &entry) {
    memory.zero(at, 168u);
    store_float(memory, at + 0u, static_cast<float>(entry.size) / 64.0f);
    store_float(memory, at + 4u, static_cast<float>(entry.size) / 64.0f);
    store_float(memory, at + 8u, 128.0f);
    store_float(memory, at + 12u, 128.0f);
    memory.store16(at + 20u, entry.family);
    memory.store16(at + 22u, entry.style);
    memory.store16(at + 26u, entry.language);
    memory.store16(at + 28u, 1u);  // region
    memory.store16(at + 30u, 1u);  // country
    for (std::size_t i = 0; entry.name[i] != '\0' && i < 63u; ++i)
        memory.store8(at + 32u + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(entry.name[i]));
    for (std::size_t i = 0; entry.file[i] != '\0' && i < 63u; ++i)
        memory.store8(at + 96u + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(entry.file[i]));
}

std::optional<pgf::Glyph> find_glyph(const pgf::Font &font, std::uint32_t library, std::uint32_t code) {
    if (auto glyph = font.glyph(code)) return glyph;
    const auto alt = g_alt_char.find(library);
    return font.glyph(alt != g_alt_char.end() ? alt->second : 0x5Fu);
}

// One pixel of intensity `value` (0..255) into a glyph image buffer.
void put_pixel(psprecomp::GuestMemory &memory, std::uint32_t buffer, std::uint32_t bytes_per_line,
               std::uint32_t format, std::uint32_t x, std::uint32_t y, std::uint8_t value) {
    switch (format) {
    case 0u:    // 4 bpp, even x in the low nibble
    case 1u: {  // 4 bpp, even x in the high nibble
        const std::uint32_t at = buffer + y * bytes_per_line + x / 2u;
        const std::uint8_t old = memory.load8(at);
        const bool high = (x & 1u) != format;
        memory.store8(at, high ? static_cast<std::uint8_t>((value & 0xF0u) | (old & 0x0Fu))
                               : static_cast<std::uint8_t>((old & 0xF0u) | (value >> 4u)));
        break;
    }
    case 2u: memory.store8(buffer + y * bytes_per_line + x, value); break;
    case 3u:
        for (std::uint32_t i = 0; i < 3u; ++i) memory.store8(buffer + y * bytes_per_line + x * 3u + i, value);
        break;
    case 4u: memory.store32(buffer + y * bytes_per_line + x * 4u, value * 0x01010101u); break;
    default: break;
    }
}

} // namespace

void install_hle(psprecomp::Runtime &runtime) {
    // sceFontNewLib(params, error) -> library handle
    runtime.register_hle("sceLibFont", 0x67F17ED7u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        if (rt.memory().contains(ctx.gpr[5], 4u)) rt.memory().store32(ctx.gpr[5], 0u);
        ctx.set_gpr(2, g_next_handle);
        g_next_handle += 0x100u;
    });
    // sceFontDoneLib(library)
    runtime.register_hle("sceLibFont", 0x574B6FBCu,
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.set_gpr(2, 0u); });
    // sceFontSetAltCharacterCode(library, code)
    runtime.register_hle("sceLibFont", 0xEE232411u, [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        g_alt_char[ctx.gpr[4]] = ctx.gpr[5];
        ctx.set_gpr(2, 0u);
    });
    // sceFontFindOptimumFont(library, style, error) -> font index: the closest size among the fonts
    // whose set fields (family, style, language, name, file) match.
    runtime.register_hle("sceLibFont", 0x099EF33Cu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        auto &memory = rt.memory();
        const std::uint32_t style = ctx.gpr[5];
        if (!memory.contains(style, 168u)) { ctx.set_gpr(2, kErrorInvalidParameter); return; }
        std::uint32_t h_bits = memory.load32(style);
        float wanted_h;
        std::memcpy(&wanted_h, &h_bits, 4);
        const std::uint16_t family = memory.load16(style + 20u);
        const std::uint16_t font_style = memory.load16(style + 22u);
        const std::uint16_t language = memory.load16(style + 26u);
        const std::string name = memory.read_c_string(style + 32u, 64u);
        const std::string file = memory.read_c_string(style + 96u, 64u);
        std::uint32_t best = 0u;
        float best_distance = 1e30f;
        bool found = false;
        for (std::size_t i = 0; i < kFonts.size(); ++i) {
            const Entry &e = kFonts[i];
            if ((family != 0u && family != e.family) || (font_style != 0u && font_style != e.style) ||
                (language != 0u && language != e.language) || (!name.empty() && name != e.name) ||
                (!file.empty() && file != e.file))
                continue;
            const float distance = wanted_h > 0.0f ? std::fabs(wanted_h - static_cast<float>(e.size) / 64.0f) : 0.0f;
            if (!found || distance < best_distance) {
                best = static_cast<std::uint32_t>(i);
                best_distance = distance;
                found = true;
            }
        }
        if (memory.contains(ctx.gpr[6], 4u)) memory.store32(ctx.gpr[6], 0u);
        ctx.set_gpr(2, best);
    });
    // sceFontOpen(library, index, mode, error) -> font handle
    runtime.register_hle("sceLibFont", 0xA834319Du, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const std::uint32_t index = ctx.gpr[5];
        const bool loaded = load_font(index) != nullptr;
        if (rt.memory().contains(ctx.gpr[7], 4u))
            rt.memory().store32(ctx.gpr[7], loaded ? 0u : kErrorHandlerOpenFailed);
        if (!loaded) { ctx.set_gpr(2, 0u); return; }
        const std::uint32_t handle = g_next_handle;
        g_next_handle += 0x100u;
        g_fonts[handle] = OpenFont{index, ctx.gpr[4]};
        ctx.set_gpr(2, handle);
    });
    // sceFontClose(font) / sceFontFlush(font)
    runtime.register_hle("sceLibFont", 0x3AEA8CB6u, [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        g_fonts.erase(ctx.gpr[4]);
        ctx.set_gpr(2, 0u);
    });
    runtime.register_hle("sceLibFont", 0x02D7F94Bu,
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.set_gpr(2, 0u); });
    // sceFontGetFontInfo(font, info): metrics as integers (26.6) then floats, bitmap maxima, style.
    runtime.register_hle("sceLibFont", 0x0DA7535Eu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        auto &memory = rt.memory();
        const auto open = g_fonts.find(ctx.gpr[4]);
        const std::uint32_t info = ctx.gpr[5];
        if (open == g_fonts.end() || !memory.contains(info, 264u)) { ctx.set_gpr(2, kErrorInvalidParameter); return; }
        const pgf::Header &h = load_font(open->second.index)->header();
        const std::int32_t metrics[10]{h.max_size[0], h.max_size[1], h.max_ascender, h.max_descender, h.max_left_x,
                                       h.max_base_y, h.min_center_x, h.max_top_y, h.max_advance[0], h.max_advance[1]};
        for (std::uint32_t i = 0; i < 10u; ++i) {
            memory.store32(info + i * 4u, static_cast<std::uint32_t>(metrics[i]));
            store_float(memory, info + 40u + i * 4u, static_cast<float>(metrics[i]) / 64.0f);
        }
        memory.store16(info + 80u, h.max_glyph_width);
        memory.store16(info + 82u, h.max_glyph_height);
        memory.store32(info + 84u, static_cast<std::uint32_t>(h.glyph_count));
        memory.store32(info + 88u, 0u);  // shadow map length (PPSSPP reports 0)
        write_style(memory, info + 92u, kFonts[open->second.index]);
        memory.store32(info + 260u, h.bpp);
        ctx.set_gpr(2, 0u);
    });
    // sceFontGetCharInfo(font, code, info)
    runtime.register_hle("sceLibFont", 0xDCC80C2Fu, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        auto &memory = rt.memory();
        const auto open = g_fonts.find(ctx.gpr[4]);
        const std::uint32_t info = ctx.gpr[6];
        if (open == g_fonts.end() || !memory.contains(info, 60u)) { ctx.set_gpr(2, kErrorInvalidParameter); return; }
        memory.zero(info, 60u);
        const auto glyph = find_glyph(*load_font(open->second.index), open->second.library, ctx.gpr[5]);
        if (glyph) {
            const std::int32_t values[14]{glyph->width, glyph->height, glyph->left, glyph->top,
                                          glyph->dimension_width, glyph->dimension_height, glyph->y_adjust_h,
                                          glyph->y_adjust_h - glyph->dimension_height, glyph->x_adjust_h,
                                          glyph->y_adjust_h, glyph->x_adjust_v, glyph->y_adjust_v,
                                          glyph->advance_h, glyph->advance_v};
            for (std::uint32_t i = 0; i < 14u; ++i) memory.store32(info + i * 4u, static_cast<std::uint32_t>(values[i]));
            memory.store16(info + 56u, static_cast<std::uint16_t>(glyph->shadow_flags));
            memory.store16(info + 58u, static_cast<std::uint16_t>(glyph->shadow_id));
        }
        ctx.set_gpr(2, 0u);
    });
    // sceFontGetCharGlyphImage_Clip(font, code, image, clip x, clip y, clip w, clip h)
    runtime.register_hle("sceLibFont", 0xCA1E6945u, [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        auto &memory = rt.memory();
        const auto open = g_fonts.find(ctx.gpr[4]);
        const std::uint32_t image = ctx.gpr[6];
        if (open == g_fonts.end() || !memory.contains(image, 24u)) { ctx.set_gpr(2, kErrorInvalidParameter); return; }
        const auto clip_x = static_cast<std::int32_t>(ctx.gpr[7]);
        const auto clip_y = static_cast<std::int32_t>(ctx.gpr[8]);
        const auto clip_w = static_cast<std::int32_t>(ctx.gpr[9]);
        const auto clip_h = static_cast<std::int32_t>(ctx.gpr[10]);
        const std::uint32_t format = memory.load32(image);
        const auto x0 = static_cast<std::int32_t>(memory.load32(image + 4u)) >> 6;
        const auto y0 = static_cast<std::int32_t>(memory.load32(image + 8u)) >> 6;
        const std::int32_t buffer_width = memory.load16(image + 12u);
        const std::int32_t buffer_height = memory.load16(image + 14u);
        const std::uint32_t bytes_per_line = memory.load16(image + 16u);
        const std::uint32_t buffer = memory.load32(image + 20u);
        const pgf::Font &font = *load_font(open->second.index);
        const auto glyph = find_glyph(font, open->second.library, ctx.gpr[5]);
        // GT_FONT_LOG=1: the first glyph image requests (format, position, buffer, clip).
        static const bool log = std::getenv("GT_FONT_LOG") != nullptr;
        static int logged = 0;
        if (log && logged++ < 40)
            std::cerr << "[font] glyph " << ctx.gpr[5] << " font " << open->second.index << " format " << format
                      << " pos " << x0 << "," << y0 << " buffer " << buffer_width << "x" << buffer_height << " bpl "
                      << bytes_per_line << " @" << std::hex << buffer << std::dec << " clip " << clip_x << ","
                      << clip_y << " " << clip_w << "x" << clip_h << (glyph ? "" : " NO GLYPH") << "\n";
        if (!glyph || !memory.contains(buffer, bytes_per_line * static_cast<std::uint32_t>(buffer_height))) {
            ctx.set_gpr(2, 0u);
            return;
        }
        const std::vector<std::uint8_t> pixels = font.bitmap(*glyph);
        for (std::int32_t y = 0; y < glyph->height; ++y) {
            for (std::int32_t x = 0; x < glyph->width; ++x) {
                const std::int32_t px = x0 + x, py = y0 + y;
                if (px < clip_x || py < clip_y || px >= clip_x + clip_w || py >= clip_y + clip_h) continue;
                if (px < 0 || py < 0 || px >= buffer_width || py >= buffer_height) continue;
                const std::uint8_t nibble = pixels[static_cast<std::size_t>(y * glyph->width + x)];
                put_pixel(memory, buffer, bytes_per_line, format, static_cast<std::uint32_t>(px),
                          static_cast<std::uint32_t>(py), static_cast<std::uint8_t>(nibble * 17u));
            }
        }
        ctx.set_gpr(2, 0u);
    });
}

} // namespace gt::font
