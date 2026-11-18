// Unit tests for the mod draw layer's rasterizer and picture store (lcs patch 0071): pixel checks of each primitive in the 480 x 272
// logical space, scaling to the output size, the worker contract, and the CPU composite the frame dumps use.
#include "draw_composite.hpp"
#include "lcs_display_menu.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace lcs {
void display_window_request_fullscreen(bool) {}  // lcs_display_menu.cpp calls it from the F10 page; no window here
}  // namespace lcs

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

struct Picture {
    std::uint32_t w, h;
    std::vector<std::uint8_t> px;
    const std::uint8_t *at(std::uint32_t x, std::uint32_t y) const { return &px[(static_cast<std::size_t>(y) * w + x) * 4u]; }
};

Picture raster(const std::vector<lcs::HostDrawCmd> &list, std::uint32_t w, std::uint32_t h) {
    Picture p{w, h, std::vector<std::uint8_t>(static_cast<std::size_t>(w) * h * 4u, 0xEE)};
    lcs::host_draw_rasterize(list.data(), list.size(), p.px.data(), w, h);
    return p;
}

lcs::HostDrawCmd rect(float x, float y, float w, float h, std::uint32_t rgba) {
    lcs::HostDrawCmd c;
    c.kind = lcs::HostDrawKind::Rect;
    c.a = x, c.b = y, c.c = w, c.d = h;
    c.rgba[0] = rgba >> 24, c.rgba[1] = rgba >> 16, c.rgba[2] = rgba >> 8, c.rgba[3] = rgba;
    return c;
}
lcs::HostDrawCmd line(float x0, float y0, float x1, float y1, float width, std::uint32_t rgba) {
    lcs::HostDrawCmd c = rect(0, 0, 0, 0, rgba);
    c.kind = lcs::HostDrawKind::Line;
    c.a = x0, c.b = y0, c.c = x1, c.d = y1, c.e = width;
    return c;
}
lcs::HostDrawCmd text(float x, float y, float size, std::uint32_t rgba, const char *s) {
    lcs::HostDrawCmd c = rect(0, 0, 0, 0, rgba);
    c.kind = lcs::HostDrawKind::Text;
    c.a = x, c.b = y, c.c = size;
    std::strncpy(c.text, s, sizeof c.text - 1);
    return c;
}

bool is_pixel(const std::uint8_t *p, int r, int g, int b, int a) { return p[0] == r && p[1] == g && p[2] == b && p[3] == a; }
}  // namespace

// `mod_draw_raster_tests bench`: the cost of rasterizing a telemetry-sized pass (200 primitives: 60 text lines, 100 rects, 40 lines) at each output size.
int bench() {
    std::vector<lcs::HostDrawCmd> list;
    for (int i = 0; i < 60; ++i) list.push_back(text(10.0f + (i % 6) * 75.0f, 4.0f + (i / 6) * 26.0f, 14.0f, 0xFFFFFFFFu, "Speed 123 km/h"));
    for (int i = 0; i < 100; ++i) list.push_back(rect(5.0f + (i % 20) * 22.0f, 200.0f + (i / 20) * 12.0f, 18.0f, 8.0f, 0x30A0FF90u));
    for (int i = 0; i < 40; ++i) list.push_back(line(10.0f + i * 11.0f, 260.0f, 20.0f + i * 11.0f, 190.0f, 2.0f, 0x00FF00FFu));
    for (const std::uint32_t w : {480u, 960u, 1920u, 3840u}) {
        const std::uint32_t h = w * 272u / 480u;
        std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 4u);
        lcs::host_draw_rasterize(list.data(), list.size(), px.data(), w, h);  // warm up: glyph cache
        double total = 0, worst = 0;
        const int runs = 30;
        for (int i = 0; i < runs; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            lcs::host_draw_rasterize(list.data(), list.size(), px.data(), w, h);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total += ms, worst = ms > worst ? ms : worst;
        }
        std::printf("%u x %u: 200 primitives, mean %.2f ms, worst %.2f ms over %d passes\n", w, h, total / runs, worst, runs);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "bench") return bench();
    // The picture starts cleared: nothing drawn is fully transparent (the 0xEE fill proves it was written).
    {
        const Picture p = raster({}, 480, 272);
        check(is_pixel(p.at(0, 0), 0, 0, 0, 0) && is_pixel(p.at(479, 271), 0, 0, 0, 0), "an empty list clears the picture");
    }
    {  // rect: exact edges at 1x, opaque and half transparent
        const Picture p = raster({rect(10, 20, 30, 40, 0xFF0000FFu), rect(100, 100, 10, 10, 0x00FF0080u)}, 480, 272);
        check(is_pixel(p.at(10, 20), 255, 0, 0, 255) && is_pixel(p.at(39, 59), 255, 0, 0, 255), "opaque rect covers its corners");
        check(is_pixel(p.at(9, 20), 0, 0, 0, 0) && is_pixel(p.at(40, 20), 0, 0, 0, 0) && is_pixel(p.at(10, 19), 0, 0, 0, 0) &&
                  is_pixel(p.at(10, 60), 0, 0, 0, 0), "and nothing outside it");
        const std::uint8_t *h = p.at(105, 105);
        check(h[0] == 0 && h[1] == 255 && h[2] == 0 && h[3] == 128, "a half transparent rect keeps its colour and alpha 128");
    }
    {  // order: a later command draws over an earlier one
        const Picture p = raster({rect(0, 0, 50, 50, 0xFF0000FFu), rect(10, 10, 10, 10, 0x0000FFFFu)}, 480, 272);
        check(is_pixel(p.at(15, 15), 0, 0, 255, 255) && is_pixel(p.at(5, 5), 255, 0, 0, 255), "draw order = call order");
    }
    {  // line: the centre of a thick horizontal line is solid, a pixel well off it is empty, the caps are round
        const Picture p = raster({line(50, 100, 150, 100, 4, 0xFFFFFFFFu)}, 480, 272);
        check(is_pixel(p.at(100, 99), 255, 255, 255, 255) && is_pixel(p.at(100, 100), 255, 255, 255, 255), "line core");
        check(p.at(100, 94)[3] == 0 && p.at(100, 106)[3] == 0, "line width: pixels beyond 2 units are empty");
        check(p.at(151, 100)[3] > 100 && p.at(154, 100)[3] == 0, "round cap past the end point");
        const Picture d = raster({line(0, 0, 100, 100, 3, 0xFF0000FFu)}, 480, 272);
        check(d.at(50, 50)[0] == 255 && d.at(50, 50)[3] == 255 && d.at(50, 60)[3] == 0, "diagonal line");
    }
    {  // scaling: the same list at 2x covers twice the pixels, anti-aliased edges stay inside the 1-pixel band
        const Picture p = raster({rect(10, 20, 30, 40, 0xFF0000FFu)}, 960, 544);
        check(is_pixel(p.at(20, 40), 255, 0, 0, 255) && is_pixel(p.at(79, 119), 255, 0, 0, 255), "2x: corners of the scaled rect");
        check(p.at(19, 40)[3] == 0 && p.at(80, 40)[3] == 0, "2x: and nothing outside");
        const Picture f = raster({rect(10.5f, 20, 10, 10, 0xFF0000FFu)}, 480, 272);
        check(f.at(10, 25)[3] > 100 && f.at(10, 25)[3] < 155, "a half-pixel edge is anti-aliased (alpha near 128)");
    }
    {  // text: pixels appear inside the line box and only there (needs the UI font; without it the text is skipped)
        const Picture p = raster({text(20, 30, 16, 0xFFFFFFFFu, "Speed 123")}, 480, 272);
        int inside = 0, outside = 0;
        for (std::uint32_t y = 0; y < 272; ++y)
            for (std::uint32_t x = 0; x < 480; ++x)
                if (p.at(x, y)[3] != 0) (x >= 18 && x < 130 && y >= 28 && y < 52 ? inside : outside)++;
        if (lcs::host_style_font_available()) {
            check(inside > 150 && outside == 0, "text draws inside its line box and nowhere else");
            const Picture big = raster({text(20, 30, 16, 0xFFFFFFFFu, "Speed 123")}, 1920, 1088);
            int big_inside = 0;
            for (std::uint32_t y = 0; y < 1088; ++y)
                for (std::uint32_t x = 0; x < 1920; ++x)
                    if (big.at(x, y)[3] != 0) ++big_inside;
            check(big_inside > inside * 8, "text is rasterized at the output size (sharp), not scaled up: ~16x the covered pixels at 4x");
        } else {
            std::fprintf(stderr, "note: UI font missing, text pixel checks skipped\n");
            check(inside == 0, "without the UI font the text is skipped");
        }
    }
    {  // the picture store: the worker contract (synchronous mode: the request rasterizes on this thread)
        lcs::host_draw_set_synchronous(true);
        check(!lcs::host_draw_active() && lcs::host_draw_request(lcs::host_draw_size(480)).version == 0u, "no list: no layer");
        const std::vector<lcs::HostDrawCmd> list{rect(0, 0, 480, 272, 0x102030FFu)};
        lcs::host_draw_submit(list.data(), list.size());
        check(lcs::host_draw_active(), "a list makes the layer active");
        const lcs::SettingsOverlaySize size = lcs::host_draw_size(960.0f);
        check(size.width == 960u && size.height == 544u, "draw size keeps 480 x 272");
        const lcs::SettingsOverlayPicture a = lcs::host_draw_request(size);
        check(a.version != 0u && a.width == 960u && a.height == 544u, "a request returns a picture of the size asked");
        std::vector<std::uint8_t> px(static_cast<std::size_t>(a.width) * a.height * 4u);
        check(lcs::host_draw_copy(a.version, px.data(), px.size()) && px[0] == 0x10 && px[1] == 0x20 && px[2] == 0x30 && px[3] == 0xFF, "the copy is the picture");
        check(lcs::host_draw_request(size).version == a.version, "the same list and size twice rasterize once");
        check(!lcs::host_draw_copy(a.version, px.data(), px.size() - 1u), "a too small buffer is refused");
        const std::vector<lcs::HostDrawCmd> list2{rect(0, 0, 480, 272, 0x405060FFu)};
        lcs::host_draw_submit(list2.data(), list2.size());
        const lcs::SettingsOverlayPicture b = lcs::host_draw_request(size);
        check(b.version != a.version && !lcs::host_draw_copy(a.version, px.data(), px.size()), "a new list is a new version; the old one is gone");
        lcs::host_draw_submit(nullptr, 0);
        check(!lcs::host_draw_active() && lcs::host_draw_request(size).version == 0u, "an empty submit removes the layer");
        lcs::host_draw_set_synchronous(false);
    }
    {  // the CPU composite the frame dumps use
        std::vector<std::uint8_t> rgb{100, 100, 100, 10, 20, 30, 200, 200, 200};
        const std::uint8_t layer[12] = {255, 0, 0, 255, 0, 0, 255, 128, 0, 0, 0, 0};
        gt::blend_rgba_over_rgb(rgb, layer, 3, 1);
        check(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0, "an opaque pixel replaces the image");
        check(rgb[3] == 5 && rgb[4] == 10 && rgb[5] == 143, "a half transparent blue pixel blends 50 / 50 (rounded)");
        check(rgb[6] == 200 && rgb[7] == 200 && rgb[8] == 200, "a transparent pixel leaves the image alone (byte-identical)");
    }
    {  // the scene grade (lcs patch 0072)
        std::vector<std::uint8_t> ramp;
        for (int v = 0; v < 256; ++v) ramp.insert(ramp.end(), {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(255 - v), static_cast<std::uint8_t>((v * 7) & 255)});
        std::vector<std::uint8_t> same = ramp;
        lcs::host_scene_grade_apply_rgb(lcs::HostSceneGrade{}, same.data(), 256);
        check(same == ramp, "the neutral grade is the identity, byte for byte");
        check(lcs::host_scene_grade_neutral(lcs::host_scene_grade()), "the stored grade starts neutral");
        std::vector<std::uint8_t> half = ramp;
        lcs::host_scene_grade_apply_rgb(lcs::HostSceneGrade{0.5f, 0.5f, 0.5f, 1.0f, 1.0f}, half.data(), 256);
        bool halved = true;
        for (std::size_t i = 0; i < ramp.size(); ++i) halved = halved && half[i] == static_cast<std::uint8_t>(ramp[i] / 255.0f * 0.5f * 255.0f + 0.5f);
        check(halved && half[255 * 3] == 128 && half[0] == 0, "a 0.5 tint halves every channel (rounded to nearest)");
        std::vector<std::uint8_t> bright = {200, 100, 10};
        lcs::host_scene_grade_apply_rgb(lcs::HostSceneGrade{1, 1, 1, 2.0f, 1}, bright.data(), 1);
        check(bright[0] == 255 && bright[1] == 200 && bright[2] == 20, "brightness 2 doubles and clamps at 255");
        std::vector<std::uint8_t> grey = {255, 0, 0};
        lcs::host_scene_grade_apply_rgb(lcs::HostSceneGrade{1, 1, 1, 1, 0.0f}, grey.data(), 1);
        check(grey[0] == grey[1] && grey[1] == grey[2] && grey[0] == 54, "saturation 0 gives the Rec.709 luma (0.2126 * 255 = 54)");
        std::vector<std::uint8_t> tint = {255, 255, 255};
        lcs::host_scene_grade_apply_rgb(lcs::HostSceneGrade{1.0f, 0.0f, 0.5f, 1.0f, 1.0f}, tint.data(), 1);
        check(tint[0] == 255 && tint[1] == 0 && tint[2] == 128, "a tint scales each channel");
        lcs::host_scene_grade_set(lcs::HostSceneGrade{0.25f, 1, 1, 1, 1});
        check(lcs::host_scene_grade().r == 0.25f && !lcs::host_scene_grade_neutral(lcs::host_scene_grade()), "set / get");
        lcs::host_scene_grade_set(lcs::HostSceneGrade{});
    }
    if (failures == 0) std::puts("mod_draw_raster tests passed");
    return failures == 0 ? 0 : 1;
}
