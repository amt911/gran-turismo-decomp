#include "ge_trace.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace gt {

namespace {
std::atomic<bool> g_requested{false};  // set by request_ge_trace() (the F8 key, on the window thread)
}

void request_ge_trace() noexcept { g_requested.store(true, std::memory_order_release); }

void trace_ge_list(const psprecomp::GuestMemory &memory, std::uint32_t list, std::uint64_t vblank) {
    static const long long wanted = [] {
        const char *text = std::getenv("GT_GE_LOG");
        return text != nullptr ? std::strtoll(text, nullptr, 0) : -1ll;
    }();
    static const bool detail_env = std::getenv("GT_GE_LOG_DETAIL") != nullptr;
    // F8: every list of the next vblank that submits one goes, with the detail, to ge_trace_vblank<N>.txt in the working directory.
    static std::FILE *hot_file = nullptr;
    static std::uint64_t hot_vblank = 0u;
    if (hot_file != nullptr && vblank != hot_vblank) {
        std::fclose(hot_file);
        hot_file = nullptr;
        std::fprintf(stderr, "[ge] F8 trace of vblank %llu written\n", static_cast<unsigned long long>(hot_vblank));
    }
    if (hot_file == nullptr && g_requested.exchange(false, std::memory_order_acq_rel)) {
        char name[64];
        std::snprintf(name, sizeof name, "ge_trace_vblank%llu.txt", static_cast<unsigned long long>(vblank));
        hot_file = std::fopen(name, "w");
        hot_vblank = vblank;
        std::fprintf(stderr, "[ge] F8: tracing vblank %llu to %s%s\n", static_cast<unsigned long long>(vblank), name,
                     hot_file != nullptr ? "" : " (cannot open: not traced)");
    }
    std::FILE *out = stderr;
    bool detail = detail_env;
    if (hot_file != nullptr) {
        out = hot_file;
        detail = true;
    } else {
        // The first vblank at or after GT_GE_LOG that submits a list: GT doesn't submit one every vblank.
        static long long traced = -1ll;
        if (wanted < 0 || static_cast<long long>(vblank) < wanted) return;
        if (traced < 0) traced = static_cast<long long>(vblank);
        if (static_cast<long long>(vblank) != traced) return;
    }
    std::uint32_t base = 0u, tex_low = 0u, tex_high = 0u, tbw = 0u, size = 0u, format = 0u, clut_low = 0u,
                  clut_high = 0u, clut_mode = 0u, vtype = 0u, fb = 0u, mode = 0u;
    bool texture = false, blend = false;
    std::uint32_t vaddr = 0u, tfunc = 0u, atest = 0u, ztest_on = 0u, zfunc = 0u, atest_on = 0u, blend_mode = 0u;
    std::uint32_t trx_src = 0u, trx_src_w = 0u, trx_dst = 0u, trx_dst_w = 0u, trx_pos = 0u, trx_dpos = 0u, trx_size = 0u;
    std::vector<std::uint32_t> stack;
    std::array<std::uint32_t, 256> state{};
    std::array<bool, 256> sent{};               // commands this list sent (state[] of the others is 0, not the GE's)
    std::array<std::uint32_t, 256> printed{};   // state at the previous prim's `set` line
    std::array<bool, 256> printed_sent{};
    std::uint32_t pc = list & 0x0FFFFFFFu;
    std::fprintf(out, "[ge] list %08x (vblank %llu)\n", list, static_cast<unsigned long long>(vblank));
    for (int steps = 0; steps < 200000 && memory.contains(pc, 4u); ++steps) {
        const std::uint32_t word = memory.load32(pc);
        pc += 4u;
        const std::uint32_t op = word >> 24u, arg = word & 0xFFFFFFu;
        state[op] = arg;  // every command's last argument, for GT_GE_LOG_DETAIL
        sent[op] = true;
        const std::uint32_t target = (base | arg) & 0x0FFFFFFCu;
        switch (op) {
        case 0x10: base = (arg << 8u) & 0x0F000000u; break;
        case 0x08: pc = target; break;                              // JUMP
        case 0x0A: stack.push_back(pc); pc = target; break;         // CALL
        case 0x0B: if (stack.empty()) return; pc = stack.back(); stack.pop_back(); break;  // RET
        case 0x0C: case 0x0F: return;                                // END / FINISH
        case 0x12: vtype = arg; break;
        case 0x01: vaddr = (base | arg) & 0x0FFFFFFFu; break;
        case 0xC9: tfunc = arg; break;
        case 0xDB: atest = arg; break;
        case 0x22: atest_on = arg & 1u; break;
        case 0x23: ztest_on = arg & 1u; break;
        case 0xDE: zfunc = arg; break;
        case 0xDF: blend_mode = arg; break;

        case 0x1E: texture = (arg & 1u) != 0u; break;
        case 0x21: blend = (arg & 1u) != 0u; break;
        case 0x9C: fb = (fb & 0xFF000000u) | arg; break;
        case 0xA0: tex_low = arg; break;
        case 0xA8: tbw = arg & 0x7FFu; tex_high = (arg >> 16u) & 0xFFu; break;
        case 0xB0: clut_low = arg; break;
        case 0xB1: clut_high = (arg >> 16u) & 0xFFu; break;
        case 0xB8: size = arg; break;
        case 0xC2: mode = arg; break;
        case 0xC3: format = arg & 0xFu; break;
        case 0xC5: clut_mode = arg; break;
        case 0xB2: trx_src = (trx_src & 0xFF000000u) | arg; break;
        case 0xB3: trx_src = (trx_src & 0x00FFFFFFu) | ((arg >> 16u) & 0xFFu) << 24u; trx_src_w = arg & 0xFFFFu; break;
        case 0xB4: trx_dst = (trx_dst & 0xFF000000u) | arg; break;
        case 0xB5: trx_dst = (trx_dst & 0x00FFFFFFu) | ((arg >> 16u) & 0xFFu) << 24u; trx_dst_w = arg & 0xFFFFu; break;
        case 0xEB: trx_pos = arg; break;
        case 0xEC: trx_dpos = arg; break;
        case 0xEE: trx_size = arg; break;
        case 0xEA:                                                   // TRXKICK
            std::fprintf(out, "[ge] transfer %s %08x w%u (%u,%u) -> %08x w%u (%u,%u) size %ux%u\n",
                         (arg & 1u) != 0u ? "32bpp" : "16bpp", trx_src, trx_src_w, trx_pos & 0x3FFu,
                         (trx_pos >> 10u) & 0x3FFu, trx_dst, trx_dst_w, trx_dpos & 0x3FFu, (trx_dpos >> 10u) & 0x3FFu,
                         (trx_size & 0x3FFu) + 1u, ((trx_size >> 10u) & 0x3FFu) + 1u);
            break;
        case 0x04: {                                                 // PRIM
            const std::uint32_t primitive = (arg >> 16u) & 7u, count = arg & 0xFFFFu;
            static std::vector<std::uint32_t> seen;
            const std::uint32_t address = (tex_high << 24u) | tex_low;
            if (texture && format == 4u && std::find(seen.begin(), seen.end(), address) == seen.end()) {
                seen.push_back(address);
                const std::uint32_t bytes = tbw * (1u << ((size >> 8u) & 0xFu)) / 2u;
                std::uint32_t nonzero = 0u;
                for (std::uint32_t i = 0; i < bytes && memory.contains(address + i, 1u); ++i)
                    nonzero += memory.load8(address + i) != 0u;
                const std::uint32_t clut = (clut_high << 24u) | clut_low;
                std::fprintf(out, "[ge] T4 texture %08x: %u of %u bytes non-zero; clut[0..3] %08x %08x %08x %08x\n",
                             address, nonzero, bytes, memory.load32(clut), memory.load32(clut + 4u),
                             memory.load32(clut + 8u), memory.load32(clut + 60u));
                // GT_GE_DUMP_DIR=<dir>: the texture decoded (swizzled T4 + 8888 CLUT) as <dir>/tex_ADDR.ppm,
                // composited on magenta so transparent texels show.
                if (const char *dir = std::getenv("GT_GE_DUMP_DIR"); dir != nullptr && tbw != 0u) {
                    const std::uint32_t w = 1u << (size & 0xFu), h = 1u << ((size >> 8u) & 0xFu);
                    const bool swizzled = (mode & 1u) != 0u;
                    char name[256];
                    std::snprintf(name, sizeof name, "%s/tex_%08x.ppm", dir, address);
                    if (std::FILE *out = std::fopen(name, "wb")) {
                        std::fprintf(out, "P6\n%u %u\n255\n", w, h);
                        const std::uint32_t row_bytes = tbw / 2u;
                        for (std::uint32_t y = 0; y < h; ++y) {
                            for (std::uint32_t x = 0; x < w; ++x) {
                                std::uint32_t offset = y * row_bytes + x / 2u;
                                if (swizzled) {  // 16-byte x 8-row blocks
                                    const std::uint32_t bx = (x / 2u) / 16u, by = y / 8u;
                                    offset = (by * (row_bytes / 16u) + bx) * 128u + (y % 8u) * 16u + (x / 2u) % 16u;
                                }
                                const std::uint32_t byte = memory.load8(address + offset);
                                const std::uint32_t index = (x & 1u) ? (byte >> 4u) : (byte & 15u);
                                const std::uint32_t c = memory.load32(clut + index * 4u);
                                const std::uint32_t a = c >> 24u;
                                const auto mix = [&](std::uint32_t v, std::uint32_t bg) { return static_cast<unsigned char>((v * a + bg * (255u - a)) / 255u); };
                                const unsigned char px[3] = {mix(c & 0xFFu, 255u), mix((c >> 8u) & 0xFFu, 0u), mix((c >> 16u) & 0xFFu, 255u)};
                                std::fwrite(px, 1u, 3u, out);
                            }
                        }
                        std::fclose(out);
                    }
                }
            }
            static int vertex_dumps = 0;
            if (texture && format == 4u && primitive == 6u && vertex_dumps++ < 3) {
                std::fprintf(out, "[ge]   tfunc %06x atest %u (%06x) ztest %u func %u blend %06x\n", tfunc, atest_on,
                             atest, ztest_on, zfunc, blend_mode);
                std::fprintf(out, "[ge]   vtype %06x vaddr %08x words:", vtype, vaddr);
                for (std::uint32_t i = 0; i < 12u && memory.contains(vaddr + i * 4u, 4u); ++i)
                    std::fprintf(out, " %08x", memory.load32(vaddr + i * 4u));
                std::fprintf(out, "\n");
            }
            // GT_GE_LOG_DETAIL: the state a reflection or lighting difference hides in. Commands are the last ones this
            // list sent: one only an earlier list sent prints 0 here although the GE still holds it.
            if (detail)
                std::fprintf(out,
                             "[ge]   vtype %06x light %u tfunc %06x envcolor %06x texmap %06x texproj %u texshade %06x lightmode %06x "
                             "blend %06x fixA %06x fixB %06x atest %u/%06x ztest %u/%u zmask %u ambient %06x/%02x\n",
                             state[0x12], state[0x17] & 1u, state[0xC9], state[0xCA], state[0xC0], (state[0xC0] >> 8u) & 3u, state[0xC1],
                             state[0x5E], state[0xDF], state[0xE0], state[0xE1], state[0x22] & 1u, state[0xDB],
                             state[0x23] & 1u, state[0xDE], state[0xE7], state[0x55], state[0x58] & 0xFFu);
            // GT_GE_LOG_DETAIL / F8: every render-state command sent in this list that changed since the previous prim,
            // as `op=arg` (hex). Flow, address, prim and matrix/bone data commands are left out.
            if (detail) {
                const auto skipped = [](std::uint32_t c) {
                    return c <= 0x11u || c == 0x2Au || c == 0x2Bu || (c >= 0x3Au && c <= 0x43u) || c == 0xEAu;
                };
                std::fprintf(out, "[ge]   set");
                for (std::uint32_t c = 0; c < 256u; ++c) {
                    if (!sent[c] || skipped(c) || (printed_sent[c] && printed[c] == state[c])) continue;
                    std::fprintf(out, " %02x=%06x", c, state[c]);
                }
                std::fprintf(out, "\n");
                printed = state;
                printed_sent = sent;
            }
            std::fprintf(out,
                         "[ge] prim %u n %u through %u fb %06x tex %d @%08x fmt %u tbw %u %ux%u swz %u clut "
                         "@%08x mode %06x blend %d\n",
                         primitive, count, (vtype >> 23u) & 1u, fb, texture ? 1 : 0, (tex_high << 24u) | tex_low,
                         format, tbw, 1u << (size & 0xFu), 1u << ((size >> 8u) & 0xFu), mode & 1u,
                         (clut_high << 24u) | clut_low, clut_mode, blend ? 1 : 0);
            break;
        }
        default: break;
        }
    }
}

} // namespace gt
