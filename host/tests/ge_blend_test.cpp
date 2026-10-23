// Unit tests for the software GE rasterizer's blend factors (lcs-recomp's ge_renderer.cpp, GT patch 0056). Plain
// executable, run by CTest. Expected values follow PPSSPP's software GPU (GPU/Software/DrawPixel.cpp): the doubled
// factors are 2*A (not clamped: the sum is) and 1 - min(2*A, 1), not min(1, 2*(1 - A)).
#include "ge_renderer.hpp"

#include "psprecomp/guest_memory.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
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
constexpr std::uint32_t kVertices = 0x08900000u;

// One 8x8 through-mode rectangle of colour `source` (0xAABBGGRR) over a framebuffer filled with `destination`, blended
// with `mode` (0xDF: A in bits 0-3, B in 4-7, equation 8-10); returns the pixel at (2, 2).
std::uint32_t draw(std::uint32_t source, std::uint32_t destination, std::uint32_t mode, std::uint32_t fix_b = 0u) {
    psprecomp::GuestMemory memory;
    for (std::uint32_t y = 0; y < 16u; ++y)
        for (std::uint32_t x = 0; x < 16u; ++x) memory.store32(kFramebuffer + (y * 512u + x) * 4u, destination);
    // Vertex: colour 8888, position s16 x/y/z (vtype 0x80011C: through mode), 12 bytes.
    const std::array<std::uint32_t, 6> vertices{source, 0u | (0u << 16u), 0u, source, 8u | (8u << 16u), 0u};
    for (std::uint32_t i = 0; i < vertices.size(); ++i) memory.store32(kVertices + i * 4u, vertices[i]);
    std::array<std::uint32_t, 256> commands{};
    const auto set = [&commands](std::uint32_t op, std::uint32_t arg) { commands[op] = (op << 24u) | arg; };
    set(0x12u, 0x80011Cu);   // vertex type
    set(0x9Cu, 0x000000u);   // framebuffer address
    set(0x9Du, 512u);        // framebuffer width
    set(0xD2u, 3u);          // 8888
    set(0xD4u, 0u);          // scissor 0,0 - 479,271
    set(0xD5u, 479u | (271u << 10u));
    set(0x21u, 1u);          // blend on
    set(0xDFu, mode);
    set(0xE0u, 0xFFFFFFu);
    set(0xE1u, fix_b);
    set(0xE8u, 0u);          // write every colour bit
    set(0xE9u, 0u);
    lcs::GeTransformState transform;
    lcs::reset_ge_transform_state(transform);
    lcs::GeRenderStats stats;
    std::string error;
    const bool ok = lcs::render_ge_primitive(memory, commands, transform, kVertices, 0u, (6u << 16u) | 2u, stats, error);
    lcs::flush_ge_deferred_rasterization(memory);
    if (!ok) std::fprintf(stderr, "render_ge_primitive: %s\n", error.c_str());
    return memory.load32(kFramebuffer + (2u * 512u + 2u) * 4u);
}

bool near(std::uint32_t pixel, std::uint32_t red) {
    const int r = static_cast<int>(pixel & 0xFFu);
    if (std::abs(r - static_cast<int>(red)) <= 2) return true;
    std::fprintf(stderr, "pixel %08x, expected red %02x\n", pixel, red);
    return false;
}
} // namespace

int main() {
    // GT's PS2-style alpha: 0x80 is opaque. 2*A ~ 1 and 1 - min(2*A, 1) = 0: the layer covers what is behind it.
    // min(1, 2*(1 - A)) kept ~100 % of the background and added the layer on top (Trial Mountain's see-through hills).
    {
        const std::uint32_t pixel = draw(0x80404040u, 0xFFFFFFFFu, 0x076u);
        check(near(pixel, 0x40u), "double src alpha / double inv src alpha, A=0x80: the source covers the destination");
    }
    {  // 2*A is not clamped to 1: A=0xC0 brightens the source by 1.5
        const std::uint32_t pixel = draw(0xC0404040u, 0xFF000000u, 0x076u);
        check(near(pixel, 0x60u), "double src alpha, A=0xC0: source x1.5, destination x0");
    }
    {  // the destination-alpha pair: B = 1 - min(2*Ad, 1)
        const std::uint32_t pixel = draw(0xFF404040u, 0x80FFFFFFu, 0x092u);
        check(near(pixel, 0x40u), "double inv dst alpha, Ad=0x80: the destination is gone");
    }
    {  // A = 2*Ad, not clamped
        const std::uint32_t pixel = draw(0xFF404040u, 0xC0000000u, 0x0A8u, 0u);
        check(near(pixel, 0x60u), "double dst alpha, Ad=0xC0: source x1.5");
    }
    if (failures == 0) std::puts("ge_blend: all tests passed");
    return failures == 0 ? 0 : 1;
}
