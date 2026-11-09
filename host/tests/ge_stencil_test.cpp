// Unit tests for the software GE rasterizer's stencil (lcs-recomp's ge_renderer.cpp, GT patch 0063). On the PSP the
// stencil is the framebuffer's alpha: the test (0x24 on, 0xDC func/ref/mask) compares (ref & mask) with (alpha & mask),
// and 0xDD picks the op written to the alpha on stencil fail / depth fail / pass (keep, zero, replace with ref, invert,
// incr, decr). GT's Time Trial ghost needs it: a 4x2 pattern sprite writes 0xFF into the alpha of every other pixel
// (colour masked off), then the ghost is drawn with "alpha & 0x80 == 0x80", so only the pattern's pixels show it.
#include "ge_renderer.hpp"

#include "psprecomp/guest_memory.hpp"

#include <array>
#include <memory>
#include <cstdio>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kFramebuffer = 0x04000000u;  // VRAM, 8888, 512 pixels per row
constexpr std::uint32_t kDepth = 0x04110000u;
constexpr std::uint32_t kVertices = 0x08900000u;
constexpr std::uint32_t kRed = 0xFF0000FFu;
constexpr std::uint32_t kBlue = 0x00FF0000u;  // alpha 0: outside the pattern
constexpr std::uint32_t kBlueMarked = 0xFFFF0000u;  // alpha 0xFF: inside the pattern

std::uint32_t pixel(psprecomp::GuestMemory &memory, std::uint32_t x, std::uint32_t y) {
    return memory.load32(kFramebuffer + (y * 512u + x) * 4u);
}

struct Draw {
    std::uint32_t stencil_test = 0u;  // 0xDC
    std::uint32_t stencil_op = 0u;    // 0xDD
    bool stencil_on = true;
    std::uint32_t color_mask = 0u;    // 0xE8
    bool depth_never = false;
    bool triangles = false;
};

// Columns 0-7 of rows 0-7: even columns hold kBlueMarked, odd ones kBlue. A red 8x8 rectangle (sprite, or two
// through-mode triangles) is drawn over it with `d`.
std::unique_ptr<psprecomp::GuestMemory> draw(const Draw &d) {
    auto owned = std::make_unique<psprecomp::GuestMemory>();
    psprecomp::GuestMemory &memory = *owned;
    for (std::uint32_t y = 0; y < 16u; ++y)
        for (std::uint32_t x = 0; x < 16u; ++x)
            memory.store32(kFramebuffer + (y * 512u + x) * 4u, (x % 2u) == 0u ? kBlueMarked : kBlue);
    // Vertex: colour 8888, position s16 x/y/z (through mode), 12 bytes.
    const auto vertex = [](std::uint32_t x, std::uint32_t y) { return std::array<std::uint32_t, 3>{kRed, x | (y << 16u), 0u}; };
    std::array<std::array<std::uint32_t, 3>, 6> list{};
    std::uint32_t count = 2u, primitive = 6u;
    if (d.triangles) {
        list = {vertex(0, 0), vertex(8, 0), vertex(0, 8), vertex(8, 0), vertex(8, 8), vertex(0, 8)};
        count = 6u;
        primitive = 3u;
    } else {
        list[0] = vertex(0, 0);
        list[1] = vertex(8, 8);
    }
    for (std::uint32_t i = 0; i < count; ++i)
        for (std::uint32_t w = 0; w < 3u; ++w) memory.store32(kVertices + (i * 3u + w) * 4u, list[i][w]);
    std::array<std::uint32_t, 256> commands{};
    const auto set = [&commands](std::uint32_t op, std::uint32_t arg) { commands[op] = (op << 24u) | arg; };
    set(0x12u, 0x80011Cu);  // vertex type: through mode
    set(0x9Cu, 0x000000u);
    set(0x9Du, 512u);
    set(0xD2u, 3u);         // 8888
    set(0xD4u, 0u);
    set(0xD5u, 479u | (271u << 10u));
    set(0x24u, d.stencil_on ? 1u : 0u);
    set(0xDCu, d.stencil_test);
    set(0xDDu, d.stencil_op);
    set(0xE8u, d.color_mask);
    set(0xE9u, 0u);
    if (d.depth_never) {
        set(0x9Eu, kDepth & 0xFFFFFFu);
        set(0x9Fu, 512u);
        set(0x23u, 1u);
        set(0xDEu, 0u);  // NEVER
    }
    lcs::GeTransformState transform;
    lcs::reset_ge_transform_state(transform);
    lcs::GeRenderStats stats;
    std::string error;
    if (!lcs::render_ge_primitive(memory, commands, transform, kVertices, 0u, (primitive << 16u) | count, stats, error))
        std::fprintf(stderr, "render_ge_primitive: %s\n", error.c_str());
    lcs::flush_ge_deferred_rasterization(memory);
    return owned;
}
} // namespace

int main() {
    for (const bool triangles : {false, true}) {
        const char *const path = triangles ? "triangles" : "sprite";
        std::fprintf(stderr, "-- %s\n", path);
        // The ghost: alpha & 0x80 == 0x80 (EQUAL), ops keep. Only the marked pixels take the colour.
        auto m = draw({.stencil_test = 0x808002u, .triangles = triangles});
        check((pixel(*m, 2, 2) & 0xFFFFFFu) == (kRed & 0xFFFFFFu), "stencil EQUAL: a marked pixel is drawn");
        check(pixel(*m, 3, 2) == kBlue, "stencil EQUAL: an unmarked pixel keeps its colour and alpha");
        check(pixel(*m, 2, 2) >> 24u == 0xFFu, "op keep on pass: the alpha stays the stencil");

        // Stencil off: the old behaviour, every pixel drawn.
        m = draw({.stencil_test = 0x808002u, .stencil_on = false, .triangles = triangles});
        check((pixel(*m, 3, 2) & 0xFFFFFFu) == (kRed & 0xFFFFFFu), "stencil off: every pixel drawn");

        // The pattern sprite: ALWAYS, pass op REPLACE with ref 0xFF, colour masked off: the alpha becomes 0xFF only.
        m = draw({.stencil_test = 0xFFFF01u, .stencil_op = 0x020200u, .color_mask = 0xFFFFFFu, .triangles = triangles});
        check(pixel(*m, 3, 2) == (kBlue | 0xFF000000u), "replace on pass writes ref into the alpha, colour masked");

        // Stencil fail op: NEVER with fail op ZERO clears the alpha and draws nothing.
        m = draw({.stencil_test = 0xFFFF00u, .stencil_op = 0x000001u, .triangles = triangles});
        check(pixel(*m, 2, 2) == (kBlueMarked & 0x00FFFFFFu), "fail op zero clears the alpha, colour kept");

        // Depth fail op: stencil passes, depth NEVER, zfail op INVERT: alpha 0xFF -> 0x00, 0x00 -> 0xFF.
        m = draw({.stencil_test = 0x000001u, .stencil_op = 0x000300u, .depth_never = true, .triangles = triangles});
        check(pixel(*m, 2, 2) == (kBlueMarked & 0x00FFFFFFu) && pixel(*m, 3, 2) == (kBlue | 0xFF000000u),
              "zfail op invert flips the alpha, colour kept");

        // Pass ops INCR / DECR saturate.
        m = draw({.stencil_test = 0x000001u, .stencil_op = 0x040000u, .triangles = triangles});
        check(pixel(*m, 2, 2) >> 24u == 0xFFu && pixel(*m, 3, 2) >> 24u == 0x01u, "incr saturates at 0xFF");
        m = draw({.stencil_test = 0x000001u, .stencil_op = 0x050000u, .triangles = triangles});
        check(pixel(*m, 2, 2) >> 24u == 0xFEu && pixel(*m, 3, 2) >> 24u == 0x00u, "decr saturates at 0");
    }
    if (failures == 0) std::puts("ge_stencil: all checks passed");
    return failures == 0 ? 0 : 1;
}
