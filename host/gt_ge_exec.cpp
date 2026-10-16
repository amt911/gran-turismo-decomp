// gt_ge_exec.cpp — GE display-list executor for gt_native.
//
// Derived from lcs-recomp's lcs/host/lcs_ge_exec.cpp at commit ab91e87fe38f3efc5e4443069e7ce6b5a43e615b
// (https://github.com/elmasas/lcs-recomp), MIT License, reproduced below as required. Changes for
// Gran Turismo are ours and listed in git history (first: GE block transfers, TRXKICK); the namespace
// stays `lcs` so the pinned lcs-recomp host headers keep matching.
//
// MIT License
// 
// Copyright (c) 2026 PSPRecomp contributors
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// LCS (GTA: Liberty City Stories) kernel HLE.

#include "lcs_ge_exec.hpp"

#include "ge_gpu_backend.hpp"
#include "ge_renderer.hpp"
#include "ge_state_revision.hpp"
#include "lcs_menu.hpp"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <tuple>
#include <mutex>
#include <map>
#include <span>
#include <string>

namespace lcs {
namespace {

// Reads display-list words through a raw pointer into the current 4 KiB block: GuestMemory::load32 resolves the
// address on every call, and a frame's lists are thousands of words, walked twice (prescan_ge_list on the game's
// thread, then execute_ge_list_rendered on the GE thread).
class ListReader {
public:
    explicit ListReader(const psprecomp::GuestMemory &memory) : memory_(memory) {}
    // False when the word is outside guest memory.
    bool read(std::uint32_t address, std::uint32_t &word) {
        const std::uint32_t base = address & ~0xFFFu;
        if (base != block_base_ || block_ == nullptr) {
            block_base_ = base;
            block_ = memory_.raw_pointer(base, 0x1000u);
        }
        if (block_ == nullptr || (address & 0xFFFu) > 0xFFCu) {  // past a region's end, or straddling blocks
            if (!memory_.contains(address, 4u)) return false;
            word = memory_.load32(address);
            return true;
        }
        const std::uint8_t *at = block_ + (address & 0xFFFu);
        word = static_cast<std::uint32_t>(at[0]) | (static_cast<std::uint32_t>(at[1]) << 8u) |
               (static_cast<std::uint32_t>(at[2]) << 16u) | (static_cast<std::uint32_t>(at[3]) << 24u);
        return true;
    }

private:
    const psprecomp::GuestMemory &memory_;
    const std::uint8_t *block_ = nullptr;
    std::uint32_t block_base_ = 0xFFFFFFFFu;
};

std::array<std::uint32_t, 256> ge_commands{};
gt::GeStateRevision ge_revision;  // bumped when a draw-state register changes (host/ge_state_revision.hpp)
GeTransformState ge_transform{};
std::uint32_t ge_base_register = 0u;
std::uint32_t ge_offset_address = 0u;
std::uint32_t ge_vertex_address = 0u;
std::uint32_t ge_index_address = 0u;
bool ge_finish_seen = false;
std::uint32_t ge_finish_arg = 0u;

// GE block transfer (sceGuCopyImage): TRXSBP/TRXSBW source, TRXDBP/TRXDBW destination (low 24 bits of
// the address; bits 16-23 of the *BW command are the high byte, bits 0-15 the buffer width in
// pixels), TRXPOS/TRXDPOS the corners, TRXSIZE the size minus one, TRXKICK bit 0 = 32-bit pixels.
// GT copies every text glyph into a small texture this way right before drawing it.
// GT_GPU_STATS=1: block transfers per (source, destination, size), printed at exit, to see which
// framebuffer copies the GPU backend misses (it never sees these memory copies).
void note_block_transfer_stats(std::uint32_t source, std::uint32_t destination, std::uint32_t width,
                               std::uint32_t height, std::uint32_t bytes_per_pixel) {
    static const bool enabled = [] { const char *v = std::getenv("GT_GPU_STATS"); return v != nullptr && *v != '0'; }();
    if (!enabled) return;
    struct Stats {
        std::mutex mutex;
        std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::uint64_t> counts;
        ~Stats() {
            for (const auto &[key, count] : counts) {
                const auto &[from, to, w, h, bpp] = key;
                std::fprintf(stderr, "[gpu-stats] transfer %08X -> %08X %ux%u %ubpp: %llu\n", from, to, w, h, bpp * 8u,
                             static_cast<unsigned long long>(count));
            }
        }
    };
    static Stats stats;
    std::lock_guard<std::mutex> guard(stats.mutex);
    ++stats.counts[{source, destination, width, height, bytes_per_pixel}];
}

void execute_block_transfer(psprecomp::GuestMemory &memory, std::uint32_t kick) {
    const auto full = [](std::uint32_t low, std::uint32_t width_command) {
        return (((width_command >> 16u) & 0xFFu) << 24u | (low & 0x00FFFFFFu)) & 0x0FFFFFFFu;
    };
    const std::uint32_t source = full(ge_commands[0xB2u], ge_commands[0xB3u]);
    const std::uint32_t source_width = ge_commands[0xB3u] & 0xFFFFu;
    const std::uint32_t destination = full(ge_commands[0xB4u], ge_commands[0xB5u]);
    const std::uint32_t destination_width = ge_commands[0xB5u] & 0xFFFFu;
    const std::uint32_t sx = ge_commands[0xEBu] & 0x3FFu, sy = (ge_commands[0xEBu] >> 10u) & 0x3FFu;
    const std::uint32_t dx = ge_commands[0xECu] & 0x3FFu, dy = (ge_commands[0xECu] >> 10u) & 0x3FFu;
    const std::uint32_t width = (ge_commands[0xEEu] & 0x3FFu) + 1u;
    const std::uint32_t height = ((ge_commands[0xEEu] >> 10u) & 0x3FFu) + 1u;
    const std::uint32_t bytes_per_pixel = (kick & 1u) != 0u ? 4u : 2u;
    note_block_transfer_stats(source, destination, width, height, bytes_per_pixel);
    std::array<std::uint8_t, 1024u * 4u> row{};
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t from = source + ((sy + y) * source_width + sx) * bytes_per_pixel;
        const std::uint32_t to = destination + ((dy + y) * destination_width + dx) * bytes_per_pixel;
        const std::size_t length = static_cast<std::size_t>(width) * bytes_per_pixel;
        if (!memory.contains(from, length) || !memory.contains(to, length)) return;
        memory.copy_out(from, std::span<std::uint8_t>(row.data(), length));
        memory.copy_in(to, std::span<const std::uint8_t>(row.data(), length));
    }
    // The GPU backend caches textures per frame: tell it which bytes this copy rewrote (GT copies each
    // text glyph into the same small texture right before drawing it).
    lcs::ge_renderer_forget_content_hashes();  // the transfer may have rewritten a texture or palette
    lcs::ge_gpu_backend_note_guest_write(destination + (dy * destination_width + dx) * bytes_per_pixel,
                                         ((height - 1u) * destination_width + width) * bytes_per_pixel);
}

constexpr std::uint32_t kMaxCommands = 2'000'000u;

std::uint32_t relative_address(std::uint32_t data24) {
    const std::uint32_t extended = ((ge_base_register & 0x000F0000u) << 8u) | (data24 & 0x00FFFFFFu);
    return (ge_offset_address + extended) & 0x0FFFFFFFu;
}

}  // namespace

void execute_ge_list_rendered(psprecomp::GuestMemory &memory, std::uint32_t list_address) {
    lcs::ge_renderer_forget_content_hashes();  // texture/palette hashes are remembered only within a list
    ge_revision.on_list_start();  // the caches keyed by the revision start over with the list
    ge_finish_seen = false;
    ge_finish_arg = 0u;
    if (list_address == 0u) return;

    std::array<std::uint32_t, 16> call_stack{};
    std::uint32_t call_depth = 0u;
    std::uint32_t pc = list_address;
    ListReader reader(memory);

    for (std::uint32_t executed = 0u; executed < kMaxCommands; ++executed) {
        std::uint32_t word = 0u;
        if (!reader.read(pc, word)) break;
        const std::uint32_t op_pc = pc;
        pc = (pc + 4u) & 0x0FFFFFFFu;
        const std::uint32_t command = word >> 24u;
        const std::uint32_t data = word & 0x00FFFFFFu;
        ge_revision.on_command(command, ge_commands[command], data);
        ge_commands[command] = data;

        if (command >= 0x2Au && command <= 0x3Fu)
            update_ge_transform_state(ge_transform, command, data);

        switch (command) {
        case 0x00u:
            break;
        case 0x01u:
            ge_vertex_address = relative_address(data);
            break;
        case 0x02u:
            ge_index_address = relative_address(data);
            break;
        case 0x10u:
            ge_base_register = data;
            break;
        case 0x13u:
            ge_offset_address = data << 8u;
            break;
        case 0x08u:
            pc = relative_address(data);
            break;
        case 0x09u:
            break;
        case 0x0Au:
            if (call_depth < call_stack.size()) call_stack[call_depth++] = pc;
            pc = relative_address(data);
            break;
        case 0x0Bu:
            if (call_depth != 0u) pc = call_stack[--call_depth];
            break;
        case 0x0Fu:
            ge_finish_seen = true;
            ge_finish_arg = data;
            break;
        case 0x0Cu:
            flush_ge_deferred_rasterization(memory);
            return;
        case 0xEAu:
            flush_ge_deferred_rasterization(memory);  // earlier draws read the old contents
            execute_block_transfer(memory, data);
            ge_revision.on_block_transfer();  // guest memory changed under the cached texture state
            break;
        case 0x04u: {
            GeRenderStats stats{};
            std::string error;
            lcs_set_ge_command(op_pc);
            // The revision lets the renderer reuse the draw state, lighting and transform of the previous draw
            // (docs/superpowers/specs/2026-10-16-ge-fast-path-design.md).
            // GT_GE_NO_REVISION=1 passes none (the caches never hit), to measure what they save.
            static const bool no_revision = std::getenv("GT_GE_NO_REVISION") != nullptr;
            const std::uint64_t revision = no_revision ? 0u : ge_revision.value();
            if (!render_ge_primitive(memory, ge_commands, ge_transform, ge_vertex_address,
                                     ge_index_address, data, stats, error, 1u, revision, revision, revision)) {
                flush_ge_deferred_rasterization(memory);
                return;
            }
            ge_vertex_address = stats.next_vertex_address;
            ge_index_address = stats.next_index_address;
            break;
        }
        default:
            break;
        }
    }
    flush_ge_deferred_rasterization(memory);
}

GeListPrescan prescan_ge_list(psprecomp::GuestMemory &memory, std::uint32_t list_address) {
    GeListPrescan result{};
    if (list_address == 0u) return result;
    std::uint32_t base_register = ge_base_register;
    std::uint32_t offset_address = ge_offset_address;
    const auto relative = [&](std::uint32_t data24) {
        const std::uint32_t extended = ((base_register & 0x000F0000u) << 8u) | (data24 & 0x00FFFFFFu);
        return (offset_address + extended) & 0x0FFFFFFFu;
    };
    std::array<std::uint32_t, 16> call_stack{};
    std::uint32_t call_depth = 0u;
    std::uint32_t pc = list_address;
    ListReader reader(memory);
    for (std::uint32_t executed = 0u; executed < kMaxCommands; ++executed) {
        std::uint32_t word = 0u;
        if (!reader.read(pc, word)) break;
        pc = (pc + 4u) & 0x0FFFFFFFu;
        const std::uint32_t command = word >> 24u;
        const std::uint32_t data = word & 0x00FFFFFFu;
        switch (command) {
        case 0x10u: base_register = data; break;
        case 0x13u: offset_address = data << 8u; break;
        case 0x08u: pc = relative(data); break;
        case 0x0Au:
            if (call_depth < call_stack.size()) call_stack[call_depth++] = pc;
            pc = relative(data);
            break;
        case 0x0Bu:
            if (call_depth != 0u) pc = call_stack[--call_depth];
            break;
        case 0x0Fu:
            result.finished = true;
            result.finish_argument = data;
            break;
        case 0x0Cu:
            return result;
        default:
            break;
        }
    }
    return result;
}

bool rendered_list_finished() { return ge_finish_seen; }
std::uint32_t rendered_finish_argument() { return ge_finish_arg; }

std::uint32_t rendered_render_target() {
    return 0x04000000u | (ge_commands[0x9Cu] & 0x001FFFF0u);
}

std::uint32_t rendered_render_stride() { return ge_commands[0x9Du] & 0x7FCu; }

}
