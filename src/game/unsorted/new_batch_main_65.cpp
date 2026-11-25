// Main-session batch 65 (DRAFT, unverified): large functions written as structured C from tools.disasm. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_65 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 rotr(u32 value, unsigned amount) { return (value >> amount) | (value << (32u - amount)); }

// 0x08C49F74 (a0 = first, a1 = last, a2 = destination): copies each 0xF0-byte element of [first, last) to the destination, skipping a null
// destination slot, and steps both by 0xF0. v0 = the destination after the last element. (An element is one run of 0xF0 bytes: the asm
// copies it in pieces — words, unaligned words, halves — that cover 0x00-0xEF once.)

// 0x08B2936C (a0 = 16 input words, a1 = 64 output bytes): the Salsa20 core. The state is the input; ten double rounds (column round,
// then row round; rotations 7, 9, 13, 18 as rotr 25, 23, 19, 14) run on it, then each output word is state + input, stored as four bytes
// little-endian, the input word read again at that moment. v0 = sp + 0x40 of the original's frame (the loop's last address).
void func_08B2936C(Runtime &rt, AllegrexContext &ctx) {
    const u32 input = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);
    u32 x[16];
    for (u32 i = 0; i < 16u; ++i) x[i] = word(rt, input + 4u * i);
    for (s32 round = 10; round != 0; --round) {
        x[4] ^= rotr(x[0] + x[12], 25);
        x[8] ^= rotr(x[4] + x[0], 23);
        x[12] ^= rotr(x[8] + x[4], 19);
        x[0] ^= rotr(x[12] + x[8], 14);
        x[9] ^= rotr(x[5] + x[1], 25);
        x[13] ^= rotr(x[9] + x[5], 23);
        x[1] ^= rotr(x[13] + x[9], 19);
        x[5] ^= rotr(x[1] + x[13], 14);
        x[14] ^= rotr(x[10] + x[6], 25);
        x[2] ^= rotr(x[14] + x[10], 23);
        x[6] ^= rotr(x[2] + x[14], 19);
        x[10] ^= rotr(x[6] + x[2], 14);
        x[3] ^= rotr(x[15] + x[11], 25);
        x[7] ^= rotr(x[3] + x[15], 23);
        x[11] ^= rotr(x[7] + x[3], 19);
        x[15] ^= rotr(x[11] + x[7], 14);
        x[1] ^= rotr(x[0] + x[3], 25);
        x[2] ^= rotr(x[1] + x[0], 23);
        x[3] ^= rotr(x[2] + x[1], 19);
        x[0] ^= rotr(x[3] + x[2], 14);
        x[6] ^= rotr(x[5] + x[4], 25);
        x[7] ^= rotr(x[6] + x[5], 23);
        x[4] ^= rotr(x[7] + x[6], 19);
        x[5] ^= rotr(x[4] + x[7], 14);
        x[11] ^= rotr(x[10] + x[9], 25);
        x[8] ^= rotr(x[11] + x[10], 23);
        x[9] ^= rotr(x[8] + x[11], 19);
        x[10] ^= rotr(x[9] + x[8], 14);
        x[12] ^= rotr(x[15] + x[14], 25);
        x[13] ^= rotr(x[12] + x[15], 23);
        x[14] ^= rotr(x[13] + x[12], 19);
        x[15] ^= rotr(x[14] + x[13], 14);
    }
    u32 last_input = 0u;
    for (u32 i = 0; i < 16u; ++i) {
        last_input = word(rt, input + 4u * i);
        const u32 sum = x[i] + last_input;
        for (u32 b = 0; b < 4u; ++b) rt.memory().store8(output + 4u * i + b, static_cast<std::uint8_t>(sum >> (8u * b)));
    }
    ctx.gpr[3] = last_input;
    ctx.gpr[2] = frame.at(0x40u);
}

} // namespace
} // namespace new_batch_main_65
} // namespace gt::game

#define NEW65(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_65::name, gt::ReturnKind::kind)
NEW65(0x08B2936Cu, func_08B2936C, Word);
