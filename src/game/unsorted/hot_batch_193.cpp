// Hot batch 193: twenty-eight-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_193 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A virtual returning four packed bytes: the result is bounced through the frame slot and reassembled (it comes back unchanged).
void bounce_packed_result(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable_offset, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 packed = call_indirect(rt, ctx, word(rt, word(rt, self) + vtable_offset), {self});
    put(rt, slot, packed);
    ctx.gpr[2] = rt.memory().load8(slot) | (rt.memory().load8(slot + 1u) << 8) | (rt.memory().load8(slot + 2u) << 16) | (rt.memory().load8(slot + 3u) << 24);
}
// `0x08B35F60(a2) < limit` selects the tail call target(a0-relative table slot, a2); v0 is that comparison.
bool length_fits(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 limit) {
    const bool fits = call(rt, ctx, 0x08B35F60u, {ctx.gpr[6]}) < limit;
    ctx.gpr[2] = fits ? 1u : 0u;
    return fits;
}
// Copy `count` objects (`step` = 0x20 apart) with 0x0896418C(dest, source) and also the word at +0x10; null slots skipped.
void construct_with_extra_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 dest = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    if (count == 0u) return;
    u32 done = 0u;
    do {
        ++done;
        if (dest != 0u) {
            call(rt, ctx, 0x0896418Cu, {dest, source});
            ctx.gpr[2] = word(rt, source + 0x10u);
            put(rt, dest + 0x10u, ctx.gpr[2]);
        }
        dest += 0x20u;
    } while (count != done);
}
// Skip the characters 0x08B38938(self) returns while they are blanks (space, tab, line breaks).
void skip_blanks(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    for (;;) {
        const u32 c = call(rt, ctx, 0x08B38938u, {self});
        if (c != 0xCu && c != 0x20u && c != 0xAu && c != 0xDu && c != 9u && c != 0xBu) break;
    }
}
// `read(a, b)` with the errno slot (0x08D774A8) cleared first; on -1 the slot's value (when set) is stored through `out`.
void call_with_errno(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const u32 out = ctx.gpr[4];
    put(rt, 0x08D774A8u, 0u);
    const u32 result = call(rt, ctx, target, arguments);
    if (result == 0xFFFFFFFFu) {
        const u32 error = word(rt, 0x08D774A8u);
        if (error != 0u) put(rt, out, error);
    }
    ctx.gpr[2] = result;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088184F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBE1F0u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_0881C378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBE450u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_088390FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBDAB8u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08843B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBD898u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08858A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBD128u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08A2D5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBE6A0u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08A31A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD14C0u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08A37928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD17F0u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 0x4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08C58A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    bounce_packed_result(rt, ctx, 0x24u, frame.at(0x0u));
}
void func_08C58A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    bounce_packed_result(rt, ctx, 0x28u, frame.at(0x0u));
}
void func_08C58CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    bounce_packed_result(rt, ctx, 0x24u, frame.at(0x0u));
}
void func_08C58D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    bounce_packed_result(rt, ctx, 0x28u, frame.at(0x0u));
}
void func_08B11918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7CA8u; G[17] = 0x08CD7C98u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B10F7Cu); G[4] = G[16] + 0x64u; call_here(rt, ctx, 0x08B28BE0u); put(rt, G[16] + 0x58u, G[17]); G[4] = G[16] + 0x58u; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x4Cu, G[17]); G[4] = G[16] + 0x4Cu; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x40u, G[17]); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 0x14u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B28BE0u); }
void func_08B11988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7CA8u; G[17] = 0x08CD7C98u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B10F7Cu); G[4] = G[16] + 0x64u; call_here(rt, ctx, 0x08B28BE0u); put(rt, G[16] + 0x58u, G[17]); G[4] = G[16] + 0x58u; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x4Cu, G[17]); G[4] = G[16] + 0x4Cu; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x40u, G[17]); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 0x14u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B28BE0u); }
void func_08B10CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[4] + 0x14u; G[18] = G[5] + 4u; G[17] = G[4]; G[4] = G[19]; G[16] = G[5]; call_here(rt, ctx, 0x08B34888u); G[4] = G[18]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17] + 0x58u; G[5] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08B1FCD4u); G[4] = G[18]; put(rt, G[16] + 0xA4u, 0u); call_here(rt, ctx, 0x08B34B24u); G[4] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B34B24u); }
void func_08B11434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[4] + 0x14u; G[18] = G[5] + 4u; G[17] = G[4]; G[4] = G[19]; G[16] = G[5]; call_here(rt, ctx, 0x08B34888u); G[4] = G[18]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17] + 0x4Cu; G[5] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08B1FCD4u); G[4] = G[18]; put(rt, G[16] + 0xA4u, 0u); call_here(rt, ctx, 0x08B34B24u); G[4] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B34B24u); }
void func_08B0D510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, 0x08D7734Cu); G[17] = 0u; while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x44u); call_here(rt, ctx, G[2]); G[4] = G[2] + 0x3FFu; G[6] = G[4] < G[2] ? 1u : 0u; G[5] = G[6] + G[3]; G[16] = word(rt, G[16]); G[2] = G[5] << 22; G[4] >>= 10; G[4] |= G[2]; G[17] += G[4]; } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08B0D790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, 0x08D7734Cu); G[17] = 0u; while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x40u); call_here(rt, ctx, G[2]); G[4] = G[2] + 0x3FFu; G[6] = G[4] < G[2] ? 1u : 0u; G[5] = G[6] + G[3]; G[16] = word(rt, G[16]); G[2] = G[5] << 22; G[4] >>= 10; G[4] |= G[2]; G[17] += G[4]; } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08AF2500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[16] = word(rt, G[4] + 4u); G[17] = word(rt, G[4] + 8u); G[18] = G[6]; if (G[17] != G[16]) { G[4] = G[16]; do { G[4] = word(rt, G[4]); G[5] = G[19]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); G[6] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[16] + 4u; G[16] = G[4]; } while (G[17] != G[4]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AF25D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[16] = word(rt, G[4] + 4u); G[17] = word(rt, G[4] + 8u); G[18] = G[6]; if (G[17] != G[16]) { G[4] = G[16]; do { G[4] = word(rt, G[4]); G[5] = G[19]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[6] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[16] + 4u; G[16] = G[4]; } while (G[17] != G[4]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A8BB2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[7] = 0x3FF00000u; G[17] = G[5]; G[16] = G[4]; G[5] = word(rt, G[16] + 0x1Cu); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x08B71328u); put(rt, G[16] + 0x18u, G[2]); G[4] = frame.at(0u); G[6] = G[2]; put(rt, G[16] + 0x1Cu, G[3]); G[7] = G[3]; call_here(rt, ctx, 0x08A8B098u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A8BB9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[7] = 0x3FF00000u; G[17] = G[5]; G[16] = G[4]; G[5] = word(rt, G[16] + 0x1Cu); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x08B712B0u); put(rt, G[16] + 0x18u, G[2]); G[4] = frame.at(0u); G[6] = G[2]; put(rt, G[16] + 0x1Cu, G[3]); G[7] = G[3]; call_here(rt, ctx, 0x08A8B098u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A5A28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[5] + 0x1Cu); G[17] = G[6]; G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); G[4] = 0x08CAFA9Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0xCu); } G[3] = word(rt, G[17]); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x34u); call_here(rt, ctx, G[2]); store8(G[16], G[2]); } } G[16] = s0; G[17] = s1; }
void func_08AAC738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[5] + 0x1Cu); G[17] = G[6]; G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); G[4] = 0x08CB32D4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0xCu); } G[3] = word(rt, G[17]); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x30u); call_here(rt, ctx, G[2]); store8(G[16], G[2]); } } G[16] = s0; G[17] = s1; }
void func_08A565C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[6]; G[18] = G[5]; G[17] = G[4]; G[16] = word(rt, G[4] + 0x14u); G[4] = 0x08CAF798u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[25] = word(rt, G[2] + 0x144u); G[6] = G[18]; G[7] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, G[25]); }
void func_08A5670C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[6]; G[18] = G[5]; G[17] = G[4]; G[16] = word(rt, G[4] + 0x14u); G[4] = 0x08CAF798u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[25] = word(rt, G[2] + 0x134u); G[6] = G[18]; G[7] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, G[25]); }
void func_08A04F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[4] = G[2] - 1u; } else { G[2] = 0u; G[4] = word(rt, G[4] + 0xA4u); } G[2] = word(rt, G[17]); G[3] = G[4] << 7; G[4] <<= 5; G[3] -= G[4]; G[2] += G[3]; G[2] += 0x40u; } G[16] = s0; G[17] = s1; }
void func_08A051A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[4] = G[2] - 1u; } else { G[2] = 0u; G[4] = word(rt, G[4] + 0xA4u); } G[2] = word(rt, G[17]); G[3] = G[4] << 7; G[4] <<= 5; G[3] -= G[4]; G[2] += G[3]; G[2] += 0x20u; } G[16] = s0; G[17] = s1; }
void func_08974718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[4] = G[2] - 1u; } else { G[2] = 0u; G[4] = word(rt, G[4] + 0xA4u); } G[2] = word(rt, G[17]); G[3] = G[4] << 6; G[4] <<= 4; G[3] -= G[4]; G[2] += G[3]; G[2] += 0x28u; } G[16] = s0; G[17] = s1; }
void func_089749A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[4] = G[2] - 1u; } else { G[2] = 0u; G[4] = word(rt, G[4] + 0xA4u); } G[2] = word(rt, G[17]); G[3] = G[4] << 6; G[4] <<= 4; G[3] -= G[4]; G[2] += G[3]; G[2] += 0x20u; } G[16] = s0; G[17] = s1; }
void func_089A0B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[2] = G[2] - 1u; } else { G[2] = 0u; G[2] = word(rt, G[4] + 0xA4u); } G[4] = word(rt, G[17]); G[3] = G[2] << 5; G[2] <<= 3; G[2] += G[3]; G[2] += G[4]; ctx.fpr[0] = float_at(rt, G[2] + 0x24u); } G[16] = s0; G[17] = s1; }
void func_089A0DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[16] = G[4]; if (load8(G[4] + 0x8Cu) != 0u) { G[2] = load8(G[4] + 0x8Cu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0xA4u); G[2] -= G[3]; G[2] = G[2] - 1u; } else { G[2] = 0u; G[2] = word(rt, G[4] + 0xA4u); } G[4] = word(rt, G[17]); G[3] = G[2] << 5; G[2] <<= 3; G[2] += G[3]; G[2] += G[4]; ctx.fpr[0] = float_at(rt, G[2] + 0x20u); } G[16] = s0; G[17] = s1; }
void func_089F7BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = 0u; G[16] = G[4]; for (;;) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x1C8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] = G[17] << 6; G[5] = G[18]; G[17] += 1u; if (G[2] == 0u) break; G[4] = word(rt, G[16] + 0xA8u); G[4] = G[3] + G[4]; G[4] += 0x20u; call_here(rt, ctx, 0x089FB570u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089F7C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = 0u; G[16] = G[4]; for (;;) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x1C8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] = G[17] << 6; G[5] = G[18]; G[17] += 1u; if (G[2] == 0u) break; G[4] = word(rt, G[16] + 0xA8u); G[4] = G[3] + G[4]; G[4] += 0x30u; call_here(rt, ctx, 0x089FB570u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A55B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[5] + 0x1Cu); G[17] = G[6]; G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); G[4] = 0x08CAF6F4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0xCu); } G[3] = word(rt, G[17]); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x38u); call_here(rt, ctx, G[2]); store16(G[16], G[2]); } } G[16] = s0; G[17] = s1; }
void func_08AABDA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[5] + 0x1Cu); G[17] = G[6]; G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); G[4] = 0x08CB31E0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0xCu); } G[3] = word(rt, G[17]); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x48u); call_here(rt, ctx, G[2]); store16(G[16], G[2]); } } G[16] = s0; G[17] = s1; }
void func_0881799C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x30u); G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B1B420u); G[3] = word(rt, frame.at(0x10u)); G[4] = G[2] % 0x10000u; G[2] >>= 16; G[3] = G[4] | (G[2] << 16); put(rt, frame.at(0u), G[3]); G[2] = sext(load16(frame.at(0u)), 16u); G[3] = load8(frame.at(2u)); G[4] = load8(frame.at(3u)); put(rt, G[16], G[2]); put(rt, G[17], G[3]); put(rt, G[18], G[4]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0884C664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[8]; G[19] = G[7]; G[18] = G[6]; G[17] = G[9]; G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBD348u; store16(G[16] + 0x22u, G[17]); put(rt, G[16], G[2]); put(rt, G[16] + 0x18u, G[18]); put(rt, G[16] + 0x1Cu, G[19]); store16(G[16] + 0x20u, G[20]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_0885BEE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[9] = G[4]; if (G[5] == 0u) { G[2] = word(rt, G[4]); store8(G[2] + 0x19u, 0u); return; } G[2] = word(rt, G[4]); G[6] = G[5]; G[8] = G[5] + 0x80u; G[7] = word(rt, G[2] + 0x88u); do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6]); put(rt, G[7] + 4u, G[2]); G[2] = 1u; put(rt, G[7], G[3]); G[4] = word(rt, G[9]); store8(G[4] + 0x19u, G[2]); }
void func_08871568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); G[2] += 4u; G[3] = word(rt, G[2] + 0x20u); if (ctx.fpr[14] < ctx.fpr[13]) { ctx.fpr[0] = float_at(rt, G[2] + 0x18u); put_float(rt, G[2] + 0x1Cu, ctx.fpr[14]); G[3] |= 3u; put_float(rt, G[2] + 0x10u, ctx.fpr[12]); put_float(rt, G[2] + 0x14u, ctx.fpr[13]); put(rt, G[2] + 0x20u, G[3]); put_float(rt, G[2] + 8u, ctx.fpr[0]); put(rt, G[2] + 0xCu, 0u); return; } put_float(rt, G[2] + 8u, ctx.fpr[12]); G[3] = G[3] >> 2 << 2; put(rt, G[2] + 0xCu, 0u); G[4] = word(rt, G[2] + 8u); G[5] = word(rt, G[2] + 0xCu); put(rt, G[2] + 0x20u, G[3]); put(rt, G[2] + 0x10u, G[4]); put(rt, G[2] + 0x14u, G[5]); put(rt, G[2] + 0x18u, G[4]); put(rt, G[2] + 0x1Cu, G[5]); }
void func_08875330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[4] = word(rt, 0x08C891F0u); G[18] = G[5]; bool found = false; if (G[4] != 0u) { G[16] = 0x08C89204u; G[17] = 0x08C891F0u; do { G[5] = G[18]; call_here(rt, ctx, 0x08B7B8C4u); if (G[2] == 0u) { found = true; break; } G[4] = word(rt, G[16]); G[17] += 0x14u; G[16] += 0x14u; } while (G[4] != 0u); } if (!found) G[17] = 0u; G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08879FC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x0886B2F4u); G[5] = 0x08CA008Cu; G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x0886A950u); G[4] = G[16]; if (G[2] != 0u) call_here(rt, ctx, 0x0886AB7Cu); } G[4] = G[17]; G[5] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08879E14u); }
void func_0887E908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC10C8u; G[5] = 2u; G[17] = G[4] + 0x70u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x0886AA0Cu); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x70u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x70u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x0886A858u); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_088822CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = G[5]; G[16] = G[4]; ctx.fpr[0] = float_at(rt, G[5] + 0x10u); G[5] = frame.at(0u); put_float(rt, G[4], ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[2] + 0x14u); put(rt, frame.at(0xCu), 0u); put_float(rt, G[4] + 4u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[2] + 0x18u); put_float(rt, G[4] + 8u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[2] + 0x1Cu); put_float(rt, G[4] + 0xCu, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[2] + 0x38u); ctx.fpr[1] = float_at(rt, G[2] + 0x30u); ctx.fpr[2] = float_at(rt, G[2] + 0x34u); put_float(rt, frame.at(8u), ctx.fpr[0]); put_float(rt, frame.at(0u), ctx.fpr[1]); put_float(rt, frame.at(4u), ctx.fpr[2]); call_here(rt, ctx, 0x08B4A7ECu); ctx.fpr[0] = -ctx.fpr[0]; put_float(rt, G[16] + 0xCu, ctx.fpr[0]); } G[16] = s0; }
void func_08891650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[16] = G[4]; if (G[2] != 0u) { G[3] = word(rt, G[4] + 0xA4u); if (G[3] != 0u) { G[2] = word(rt, G[4] + 0xA8u); put(rt, G[4], G[3]); put(rt, G[4] + 0xA4u, 0u); put(rt, G[4] + 4u, G[2]); put(rt, G[4] + 0xA8u, 0u); } G[2] = load8(G[16] + 0xADu); if (G[2] != 0u) { G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08B35254u); store8(G[16] + 0xADu, 0u); } store8(G[16] + 0x10u, 0u); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); store8(G[16] + 0x11u, 0u); } } G[16] = s0; }
void func_08892F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 text = ctx.gpr[6];
    bool fits;
    {
        GuestFrame frame(ctx, 0x10u);
        fits = length_fits(rt, ctx, 0x7Fu);
    }
    if (fits) tail_call(ctx, 0x08B7BB6Cu, {base + (index << 7u) + 0x230Cu, text});
}
void func_08892FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 text = ctx.gpr[6];
    bool fits;
    {
        GuestFrame frame(ctx, 0x10u);
        fits = length_fits(rt, ctx, 7u);
    }
    if (fits) tail_call(ctx, 0x08B7BB6Cu, {base + (index << 3u) + 0x260Cu, text});
}
void func_088983DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC20A8u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x38u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) { G[2] = word(rt, G[3]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[2] = word(rt, G[16] + 0x30u); put(rt, G[16] + 0x38u, 0u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } put(rt, G[16] + 0x30u, 0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x088F897Cu); }
void func_08898744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[5] = G[6]; G[18] = G[4]; G[17] = G[6]; call_here(rt, ctx, 0x0894D934u); G[3] = word(rt, G[16]); G[5] = G[2]; G[4] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[6] = frame.at(0u); call_here(rt, ctx, G[2]); G[16] = G[2]; G[4] = G[18]; G[5] = G[17]; G[6] = frame.at(0u); call_here(rt, ctx, 0x0894D9A0u); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088AC66C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[2] = G[4]; G[4] += 8u; G[5] = G[4]; G[6] = frame.at(0u); G[18] = 8u; G[17] = 0u; G[16] = G[2] + 0x74u; put_float(rt, frame.at(0u), ctx.fpr[12]); put_float(rt, frame.at(4u), ctx.fpr[13]); call_here(rt, ctx, 0x08B4AC0Cu); do { G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); G[17] += 1u; call_here(rt, ctx, 0x08B4AC0Cu); G[16] += 0x44u; } while (G[17] != G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088B66E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = load8(G[5]); G[7] = G[4]; G[8] = G[4] + 0x88u; G[2] = G[5]; G[3] = G[2] << 6; G[2] <<= 3; G[9] = G[2] + G[3]; G[3] = G[5]; G[2] = 0xFFu; G[4] += 0x90u; if (G[3] != G[2]) { G[4] = load8(G[6] + 1u); G[6] = G[7] + 0xA4u; G[2] = G[4] - 1u; if (G[4] == 0u) { G[2] = G[6] + G[9]; G[3] = word(rt, G[2] + 0x40u); G[2] = G[3] - 1u; } G[3] = G[6] + G[9]; G[2] <<= 3; G[4] = G[3] + G[2]; G[2] = G[4]; return; } G[2] = load16(G[6]) ^ 0x1FFu; if (G[2] != 0u) G[4] = G[8]; G[2] = G[4]; }
void func_088E6A9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x10u); G[19] = G[7]; G[18] = G[6]; G[16] = G[4]; G[4] = word(rt, G[4]) + 0xECu; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, G[2]); if (G[2] != 0u) { put(rt, G[2] + 0x28u, G[18]); put(rt, G[2] + 0x2Cu, G[19]); } G[4] = word(rt, G[16]) + 0xECu; } G[16] = s0; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_088E6F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = 0u; G[17] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4]) + 0xECu; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) G[18] = sext(load16(G[2] + 0x1Cu), 16u); G[4] = word(rt, G[17]) + 0xECu; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088F1CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC3B48u; G[17] = G[4] + 8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x088F1C98u); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 8u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); } put(rt, G[16] + 8u, 0u); G[2] = 0x08CBF518u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_088F6A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[4] + 4u); ctx.fpr[5] = float_at(rt, G[4]); ctx.fpr[4] = float_at(rt, G[4] + 8u); ctx.fpr[1] = ctx.fpr[3] * ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[5] * ctx.fpr[5]; ctx.fpr[2] = ctx.fpr[4] * ctx.fpr[4]; ctx.fpr[12] = ctx.fpr[12] * ctx.fpr[14]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; G[1] = 0x3F800000u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[0] = __builtin_sqrtf(ctx.fpr[0]); ctx.fpr[12] = ctx.fpr[12] * ctx.fpr[0]; ctx.fpr[0] = 0.0f; ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[1]; const bool eq = ctx.fpr[12] == ctx.fpr[0]; ctx.fpr[14] = ctx.fpr[14] * ctx.fpr[13]; if (!eq) { ctx.fpr[0] = ctx.fpr[4] / ctx.fpr[12]; ctx.fpr[1] = ctx.fpr[5] / ctx.fpr[12]; ctx.fpr[3] = ctx.fpr[3] / ctx.fpr[12]; put_float(rt, G[4] + 8u, ctx.fpr[0]); put_float(rt, G[4], ctx.fpr[1]); put_float(rt, G[4] + 4u, ctx.fpr[3]); } ctx.fpr[0] = ctx.fpr[3] - ctx.fpr[14]; put_float(rt, G[4] + 4u, ctx.fpr[0]); }
void func_08906160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 handle;
    {
        GuestFrame frame(ctx, 0x10u);
        for (const u32 offset : {4u, 8u, 0xCu, 0x10u, 0x14u, 0x18u}) call(rt, ctx, 0x0890610Cu, {word(rt, self + offset)});
        handle = word(rt, self + 0x84u);
        ctx.gpr[2] = handle;
    }
    if (handle != 0u) tail_call(ctx, 0x08AF7B0Cu, {handle, 0xFFFFFFFFu});
}
void func_08909928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x08AF12A4u); G[2] = word(rt, G[16] + 8u); G[5] = G[17]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08AF12A4u); G[2] = word(rt, G[16] + 0xCu); G[5] = G[17]; G[4] = G[2]; tail = G[2] != 0u; } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08AF12A4u); }
void func_0890B19C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; if (G[4] == G[2]) { G[16] = 0x08CDD7E8u; G[2] = 0xFFFFu; G[17] = G[16] + 8u; if (G[5] == G[2]) { G[4] = G[16]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08BDABCCu); G[4] = G[16]; G[5] = G[17]; G[6] = G[16]; tail = true; } } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08BDA64Cu); }
void func_08911ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[17] = G[6]; call_here(rt, ctx, 0x08911D88u); G[3] = word(rt, G[16]); G[4] = G[3]; if (G[3] != 0u) { G[2] = word(rt, G[17] + 4u); G[5] = 0u; G[7] = G[2]; G[6] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0xCu); G[25] = G[2]; target = G[2]; } } } G[16] = s0; G[17] = s1; if (target != 0u) jump(ctx, target); }
void func_08938018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[4] + 4u); G[3] = word(rt, G[7] + 4u); G[6] = 0u; if (static_cast<std::int32_t>(G[3]) <= 0) { G[2] = 0u; return; } for (;;) { G[2] = (G[6] << 3) + G[7]; G[4] = word(rt, G[2] + 0x14u); if (G[4] == G[5]) { G[2] = (G[6] << 3) + G[7]; G[4] = word(rt, G[2] + 0x10u); G[3] <<= 3; G[2] = G[7] + 0x10u; G[2] += G[3]; G[2] += G[4]; G[2] += 2u; return; } G[6] += 1u; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[4] = 0xFFFFFFFFu; if (G[2] == 0u) break; } G[2] = 0u; }
void func_08939828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t fp = G[30]; { GuestFrame frame(ctx, 0x30u); G[30] = G[29]; G[18] = G[6]; G[17] = G[4]; G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 0x18u) + 0x1Eu; G[2] = G[2] >> 4 << 4; G[29] -= G[2]; G[6] = G[29]; call_here(rt, ctx, 0x089393B4u); G[4] = G[17]; G[5] = G[29]; G[6] = G[18]; call_here(rt, ctx, 0x089396F8u); G[29] = G[30]; } G[16] = s0; G[17] = s1; G[18] = s2; G[30] = fp; }
void func_0893A2AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08939F4Cu); G[2] = word(rt, G[16] + 8u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = 0u; G[18] = 0xFFFFFFFFu; G[5] = 0u; do { G[4] = G[16]; call_here(rt, ctx, 0x0893A104u); put(rt, G[2], G[18]); G[17] += 1u; G[2] = word(rt, G[16] + 8u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = G[17]; } while (G[2] != 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08939F54u); }
void func_08947C04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[16] = G[4]; put(rt, frame.at(0x14u), bits_of(f21)); call_here(rt, ctx, 0x08947B98u); ctx.fpr[21] = ctx.fpr[0]; ctx.fpr[12] = -ctx.fpr[20]; G[4] = G[16]; call_here(rt, ctx, 0x08947B50u); const bool lt = ctx.fpr[21] < ctx.fpr[20]; ctx.fpr[0] = ctx.fpr[20] - ctx.fpr[21]; ctx.fpr[12] = 0.0f; if (lt) ctx.fpr[20] = ctx.fpr[20] - ctx.fpr[0]; else ctx.fpr[12] = ctx.fpr[21] - ctx.fpr[20]; G[4] = G[16]; call_here(rt, ctx, 0x08947B44u); ctx.fpr[0] = ctx.fpr[20]; } G[16] = s0; ctx.fpr[20] = f20; ctx.fpr[21] = f21; }
void func_0896B1F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[7] = frame.at(0u); G[17] = G[5]; G[8] = frame.at(4u); G[6] = 0u; G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x0896A35Cu); ctx.fpr[0] = float_at(rt, frame.at(0u)); ctx.fpr[1] = float_at(rt, G[17]); ctx.fpr[2] = float_at(rt, frame.at(4u)); G[3] = word(rt, G[16] + 0xE0u); ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[0]; G[2] = 0x100000u; G[3] |= G[2]; put_float(rt, G[16] + 0xBCu, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[17] + 4u); put(rt, G[16] + 0xE0u, G[3]); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[2]; put_float(rt, G[16] + 0xC0u, ctx.fpr[0]); } G[16] = s0; G[17] = s1; }
void func_0899C2CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x114u); G[16] = G[5]; call_here(rt, ctx, G[2]); G[17] = G[2]; G[5] = G[16]; G[4] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x08A69654u); G[4] = G[17]; G[5] = G[18] + 0x24u; G[6] = frame.at(0u); call_here(rt, ctx, 0x08A7BFD0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089AC0A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[2] = 0xFFFFFFFFu; ctx.fpr[1] = ctx.fpr[13] + ctx.fpr[15]; ctx.fpr[0] = ctx.fpr[12] + ctx.fpr[14]; G[5] = word(rt, G[4]); put_float(rt, frame.at(0x10u), ctx.fpr[12]); G[2] = 0x08C90000u; G[7] = frame.at(0u); put_float(rt, frame.at(0x18u), ctx.fpr[0]); G[6] = 4u; G[8] = 0u; put_float(rt, frame.at(0x1Cu), ctx.fpr[1]); G[9] = 0u; put_float(rt, frame.at(0u), ctx.fpr[12]); put_float(rt, frame.at(4u), ctx.fpr[13]); put_float(rt, frame.at(8u), ctx.fpr[0]); put_float(rt, frame.at(0xCu), ctx.fpr[13]); put_float(rt, frame.at(0x14u), ctx.fpr[1]); G[3] = word(rt, G[5] + 0x10Cu); G[5] = word(rt, 0x08C8CC14u); call_here(rt, ctx, G[3]); } }
void func_089B4098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A02B98u); G[2] = 0x08CBB320u; put(rt, G[16], G[2]); G[3] = 0x08D74FD0u + 0xCu; G[2] = 0x08CBB578u; G[5] = 0x08CF6ED8u; put(rt, G[16] + 0x38u, G[2]); G[4] = G[16] + 0x11Cu; put(rt, G[16] + 0x118u, G[3]); call_here(rt, ctx, 0x08A03794u); G[4] = G[16] + 0x120u; call_here(rt, ctx, 0x08B487ACu); G[2] = word(rt, G[16] + 0x60u) | 8u; put(rt, G[16] + 0x60u, G[2]); } G[16] = s0; }
void func_089B601C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[17] = 0u; G[2] = word(rt, G[4] + 0x124u); G[4] = G[5]; if (G[2] != 0u) { call_here(rt, ctx, 0x089AAC8Cu); G[4] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x089B5E10u); G[17] = G[2]; G[4] = G[16]; G[5] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x089A301Cu); } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089BF71C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x2B0u; G[16] = G[4]; G[4] = G[17]; G[18] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CA5EA4u; G[16] = word(rt, G[16] + 0x264u); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x78u); G[5] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089E63F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[16] = frame.at(4u); G[18] = G[5]; G[5] = frame.at(0u); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0u), G[6]); call_here(rt, ctx, 0x08969188u); G[4] = G[17]; G[5] = G[18]; G[6] = G[16]; call_here(rt, ctx, 0x089E6218u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, G[17] + 0x19Cu); G[2] = 2u; put(rt, G[17] + 0x1D8u, G[2]); put(rt, G[17] + 0x1A4u, G[3]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089F9BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = 0x08CF5B18u; G[3] = sext(load8(0x08CF5B18u), 8u); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x08CF5B20u; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08CF5B20u, 0u); call_here(rt, ctx, 0x08B64494u); G[4] = 0x089F9D1Cu; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08CF5B20u; } G[16] = s0; G[17] = s1; }
void func_08A10268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0x08CB9110u; G[5] = 0x08CB92DCu; G[16] = G[4]; G[2] = word(rt, G[4] + 0x74u); put(rt, G[16], G[3]); G[4] = G[2]; put(rt, G[16] + 0x38u, G[5]); if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16] + 0x70u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x6Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x64u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089BBAC8u); }
void func_08A15534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x3Cu); G[3] = word(rt, G[4] + 0x40u); ctx.fpr[5] = 0.0f; G[6] = 0u; G[4] = G[3] - 0x10u; if (G[2] != G[3]) { ctx.fpr[3] = float_at(rt, G[3] - 0x10u); ctx.fpr[4] = float_at(rt, G[4] + 8u); ctx.fpr[0] = ctx.fpr[4] - ctx.fpr[3]; if (ctx.fpr[5] < ctx.fpr[0]) { ctx.fpr[1] = float_at(rt, G[4] + 0xCu); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[0] = ctx.fpr[1] - ctx.fpr[2]; if (ctx.fpr[5] < ctx.fpr[0]) { G[6] = 1u; if (G[4] != G[5]) { put_float(rt, G[5] + 0xCu, ctx.fpr[1]); put_float(rt, G[5], ctx.fpr[3]); put_float(rt, G[5] + 4u, ctx.fpr[2]); put_float(rt, G[5] + 8u, ctx.fpr[4]); } } } } G[2] = G[6]; }
void func_08A202F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[16] = G[7]; call_here(rt, ctx, 0x08AC32A4u); G[4] = G[2]; G[5] = G[16]; G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08AC3360u); G[3] = G[2]; } G[4] = frame.at(0u); G[5] = G[3]; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A25428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); if (static_cast<std::int32_t>(G[5]) > 0) { G[5] = G[6]; call_here(rt, ctx, 0x08A6C410u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAAFC4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A6BFFCu); G[4] = G[2]; call_here(rt, ctx, 0x08B497A0u); G[4] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = 0x08D6950Cu; G[5] = G[16]; call_here(rt, ctx, 0x08B49414u); } } G[16] = s0; }
void func_08A39B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A39AF0u); G[4] = 0x08CAD480u; G[2] = word(rt, frame.at(0u)); G[17] = G[2] + 0x124u; G[16] = G[2] + 0x14u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; put(rt, G[16] + 0xCu, 0u); put(rt, G[16] + 4u, 0u); put(rt, G[16] + 8u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A57714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[7]; G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB7730u; put(rt, G[16], G[2]); G[5] = G[17]; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A52A98u); G[5] = G[18]; G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08A56010u); put(rt, G[16] + 0x1Cu, G[19]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A59DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[5] + 0x1Cu); G[17] = G[6]; G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); G[4] = 0x08CAFA24u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0xCu); } G[2] = word(rt, G[17]); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x54u); call_here(rt, ctx, G[3]); put_float(rt, G[16], ctx.fpr[0]); } } G[16] = s0; G[17] = s1; }
void func_08A5A764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[4] = 0x14u; G[17] = G[5]; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[2] = 0x08CD1F30u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08A76AA0u); G[2] = 0x08CD22B8u; put(rt, G[16], G[2]); G[4] = G[16] + 8u; G[5] = frame.at(0u); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x08A76AACu); G[2] = G[16]; store8(G[16] + 0xCu, 0u); put(rt, G[16] + 0x10u, 0u); } G[16] = s0; G[17] = s1; }
void func_08A7677C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = word(rt, G[4] + 8u); G[4] = G[5]; if (G[17] != 0u) { G[16] = word(rt, G[17]); call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[2] = word(rt, G[16] + 0x108u); G[4] = G[17]; G[5] = G[18]; G[25] = G[2]; target = G[2]; } } G[16] = s0; G[17] = s1; G[18] = s2; if (target != 0u) jump(ctx, target); }
void func_08A77BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[4] = frame.at(0u); G[17] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CB0E38u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xE8u); G[5] = G[17]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; }
void func_08A88F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[17] = 0u; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x120u); G[16] = G[5]; call_here(rt, ctx, G[2]); G[4] = G[18] + 0x20u; G[6] = G[16]; G[7] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08B0C5C0u); G[4] = word(rt, G[18] + 0x18u) % 2u; if (G[4] != 0u) { put(rt, G[18] + 0x13Cu, 0u); put(rt, G[18] + 0x140u, 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A8B098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[5] = 4u; G[16] = G[4]; G[4] = 0x20u; G[19] = G[7]; G[18] = G[6]; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[6] = G[18]; G[7] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A8A888u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AAD510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[17] = G[4]; call_here(rt, ctx, 0x08AACE40u); G[16] = G[17] + 0x68u; G[4] = G[17] + 0x84u; call_here(rt, ctx, 0x08B28DC4u); G[5] = word(rt, G[16] + 8u); G[4] = G[16]; call_here(rt, ctx, 0x08C345A8u); if (G[17] != 0u && G[17] != G[16]) { G[17] = 0x1Au; G[16] -= 4u; do { G[17] -= 1u; G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[16] -= 4u; } while (G[17] != 0u); } } G[16] = s0; G[17] = s1; }
void func_08ABAD70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {0x08CFFA2Cu});
    u32 created = 0u;
    if (word(rt, 0x08CFFA54u + 0x18u) != 0u) {
        created = call(rt, ctx, 0x08B2B7D8u, {0x08CFFA54u, 0u});
        if (created != 0u) call(rt, ctx, 0x08ABA848u, {created});
    }
    call(rt, ctx, 0x08B3457Cu, {0x08CFFA2Cu});
    ctx.gpr[2] = created;
}
void func_08ABC318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[16] = word(rt, 0x08D7718Cu); G[17] = G[5] % 0x100u; bool hit = false; if (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); for (;;) { G[5] = G[18]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x24u); G[6] = G[17]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { hit = true; break; } G[16] = word(rt, G[16]); if (G[16] == 0u) break; G[4] = word(rt, G[16] + 4u); } } if (!hit) G[2] = 0u; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08ABFE9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0xCu; G[17] = G[5]; G[16] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B1FBACu); G[4] = G[16]; G[5] = G[17]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B1FA64u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08ABFDD0u); } G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_08AC38AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x18Cu; G[16] = G[4]; G[4] = G[17]; G[18] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[2] = load8(G[16] + 0x1B4u); if (G[2] != 0u) { call_here(rt, ctx, 0x08AC381Cu); G[4] = G[16]; call_here(rt, ctx, 0x08AC2DECu); G[18] = G[2]; } G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AC77B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 8u; G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B34888u); ctx.fpr[0] = 0.0f; ctx.fpr[1] = float_at(rt, G[17] + 0x560u); G[4] = G[16]; if (ctx.fpr[1] == ctx.fpr[0]) store8(G[17] + 0x566u, 0u); G[16] = word(rt, G[17] + 0x56Cu); put(rt, G[17] + 0x56Cu, 0u); call_here(rt, ctx, 0x08B34B24u); G[4] = G[16]; if (G[16] != 0u) call_here(rt, ctx, 0x08AC69C0u); put(rt, G[17] + 0x5A4u, 0u); } G[16] = s0; G[17] = s1; }
void func_08ACE96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[3] = 0x800u; G[4] = 3u; G[16] = G[5]; store8(G[5] + 0xE8u, G[2]); G[5] = 0xFFFFFFFEu; G[2] = 0x08ACD7E8u; put(rt, frame.at(4u), G[3]); put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[6] = G[2]; G[4] = frame.at(8u); G[5] = frame.at(0u); G[7] = 0u; call_here(rt, ctx, 0x08B1B92Cu); put(rt, G[16] + 0xE4u, G[2]); } G[16] = s0; }
void func_08ACEA1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[3] = 0x2800u; G[4] = 3u; G[16] = G[5]; store8(G[5] + 0xD4u, G[2]); G[5] = 0u; G[2] = 0x08ACDBF4u; put(rt, frame.at(4u), G[3]); put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[6] = G[2]; G[4] = frame.at(8u); G[5] = frame.at(0u); G[7] = 0u; call_here(rt, ctx, 0x08B1B92Cu); put(rt, G[16] + 0xD0u, G[2]); } G[16] = s0; }
void func_08AD3A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[6]; call_here(rt, ctx, 0x08AD39E4u); if (G[2] != 0u && static_cast<std::int32_t>(G[16]) >= 0) { G[3] = (G[16] << 2) + G[2]; G[2] = word(rt, G[2] + 0x10u); G[2] = G[16] < G[2] ? 1u : 0u; G[4] = G[17]; if (G[2] != 0u) { G[5] = word(rt, G[3] + 0x14u); tail = true; } } if (!tail) G[2] = 0u; } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08AD393Cu); }
void func_08AD8F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[3] = G[5] != 0u ? 1u : 0u; G[7] = G[5]; G[16] = G[4]; G[5] = 0u; G[4] = word(rt, G[4]); G[2] = G[4] != 0u ? 1u : 0u; G[2] = G[2] & G[3]; G[17] = G[6]; if (G[2] != 0u) { G[2] = word(rt, G[16] + 4u); G[2] = G[4] + G[2]; put(rt, G[7], G[2]); G[4] = G[2]; call_here(rt, ctx, 0x08B36018u); } G[2] = word(rt, G[16] + 4u); G[2] += G[17]; put(rt, G[16] + 4u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08ADEA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[3] = 0x08D40674u; G[2] = G[4] << 2; G[8] = G[4]; G[4] = G[4] < 0x20u ? 1u : 0u; G[2] += G[3]; if (G[4] != 0u) { put(rt, G[2] + 0x234u, G[5]); } else { G[2] = 0x08CE0000u; G[3] = word(rt, 0x08CDFB3Cu) % 2u; G[7] = 0x08C9101Cu; G[5] = 0x08C90FE4u; G[6] = 0x2C9u; if (G[3] != 0u) { G[4] = word(rt, 0x08CDFB40u); call_here(rt, ctx, 0x08AD83B8u); } } } }
void func_08AE3BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0x08C91518u; G[16] = word(rt, G[17]); call_here(rt, ctx, 0x08AE2C98u); G[3] = G[16] < 0x16u ? 1u : 0u; if (G[2] != 0u) { G[4] = G[2]; G[6] = G[16] << 2; G[5] = G[17]; G[2] = G[6] + 0x08CDFCA0u; if (G[3] != 0u) { G[2] = word(rt, G[2]); call_here(rt, ctx, G[2]); } } } G[16] = s0; G[17] = s1; }
void func_08AFAAAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[6]; G[18] = G[5] % 0x100u; G[17] = G[7]; G[16] = G[4] + 0x2Cu; G[6] = word(rt, G[4] + 8u); G[5] = word(rt, G[4] + 4u); G[4] = G[16]; call_here(rt, ctx, 0x08B52CDCu); G[4] = G[16]; G[6] = G[17]; G[7] = G[18]; G[8] = G[19]; G[5] = 0u; call_here(rt, ctx, 0x08B52D50u); G[2] = G[2] != 0u ? 1u : 0u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B0F1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x110u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0E0CCu); G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x08B0EA48u); G[16] = G[2]; G[4] = G[2]; G[17] = 0x15u; if (G[2] != 0u) { G[2] = 6u; put(rt, G[16] + 0x7Cu, G[2]); call_here(rt, ctx, 0x08B0DE3Cu); G[4] = G[16]; G[17] = word(rt, G[16] + 0xA8u); call_here(rt, ctx, 0x08B0EA58u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0DDF0u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08B15020(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = 0xFFFFFFFFu; G[16] = G[5]; G[2] = word(rt, G[5] + 0xCu); G[5] = G[6]; if (G[2] != 0u) { G[2] = 2u; put(rt, G[16] + 4u, G[2]); } else { G[4] = word(rt, G[16] + 0x10u); G[6] = word(rt, G[6] + 4u); G[5] = word(rt, G[5]); call_here(rt, ctx, 0x08C7A818u); G[17] = G[2]; G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) < 0) { call_here(rt, ctx, 0x08B1830Cu); put(rt, G[16] + 4u, G[2]); } } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08B1830C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 0x80020000u; G[2] = 0x80020321u; G[3] = 4u; if (G[4] == G[2]) { G[2] = G[3]; return; } const auto a = static_cast<std::int32_t>(G[4]); if (static_cast<std::int32_t>(G[2]) < a) { G[2] = a < static_cast<std::int32_t>(0x80020323u) ? 1u : 0u; G[3] = 0x15u; if (G[2] != 0u) { G[2] = G[3]; return; } G[2] = static_cast<std::int32_t>(0x80020324u) < a ? 1u : 0u; G[3] = 0xAu; if (G[2] == 0u) { G[2] = G[3]; return; } G[2] = 0x80020325u; G[3] = 2u; if (G[4] != G[2]) G[3] = 0x15u; G[2] = G[3]; return; } G[2] = 0x80020193u; G[3] = 0xAu; if (G[4] == G[2]) { G[2] = G[3]; return; } G[2] = 0x80020320u; if (G[4] != G[2]) G[3] = 0x15u; G[2] = G[3]; }
void func_08B195EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = G[5]; G[16] = G[4]; G[5] = word(rt, G[5]); G[8] = G[7]; bool ok = false; if (G[5] != 0u) { G[7] = G[6]; G[4] = frame.at(0u); ok = G[6] != 0u; } if (ok) { G[6] = word(rt, G[2] + 4u); call_here(rt, ctx, 0x08B194E0u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B18A58u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B18A98u); } else { put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); store8(G[16] + 8u, 0u); } G[2] = G[16]; } G[16] = s0; }
void func_08B2B118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, 0x08D773F4u); G[18] = G[4]; bool hit = false; if (G[17] != 0u) { G[16] = word(rt, G[17] + 4u); for (;;) { G[5] = G[18]; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x10u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { hit = true; break; } G[17] = word(rt, G[17]); if (G[17] == 0u) break; G[16] = word(rt, G[17] + 4u); } } if (!hit) G[16] = 0u; G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B2B37C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[16] = word(rt, 0x08D773F4u); G[17] = G[5]; bool hit = false; if (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); for (;;) { G[5] = G[18]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x14u); G[6] = G[17]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { hit = true; break; } G[16] = word(rt, G[16]); if (G[16] == 0u) break; G[4] = word(rt, G[16] + 4u); } } if (!hit) G[2] = 0u; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B2E13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, 0x08D68D88u); G[5] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[6]) <= 0) { G[2] = G[5]; return; } G[8] = 0x08D68DC0u; G[7] = 0u; G[2] = G[6] + G[7]; for (;;) { G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[3] = G[5] << 2; G[2] = G[5] << 4; G[2] -= G[3]; G[2] += G[8]; G[3] = word(rt, G[2] + 4u); G[3] = G[4] < G[3] ? 1u : 0u; if (G[3] != 0u) G[6] = G[5]; else { G[2] = word(rt, G[2] + 8u); G[2] = G[4] < G[2] ? 1u : 0u; G[7] = G[5] + 1u; if (G[2] != 0u) { G[2] = G[5]; return; } } G[2] = static_cast<std::int32_t>(G[7]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = G[6] + G[7]; } G[5] = ~G[7]; G[2] = G[5]; }
void func_08B2ECF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] != G[2]) return; G[2] = 0xFFFFu; G[5] = 2u; G[6] = 0x100u; G[4] = 0x08D69124u; if (G[3] != G[2]) return; call_here(rt, ctx, 0x08B2EE10u); G[4] = 0x08D69110u; G[5] = 2u; G[6] = 0x101u; call_here(rt, ctx, 0x08B2EE10u); G[4] = 0x08D690FCu; G[5] = 2u; G[6] = 0x102u; } jump(ctx, 0x08B2EE10u); }
void func_08B38A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    skip_blanks(rt, ctx);
}
void func_08B40080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0x08B41AD0u; G[16] = G[4]; G[4] = word(rt, G[4] + 0xB0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x74u); G[6] = G[16]; call_here(rt, ctx, G[2]); G[4] = word(rt, G[16] + 0xB0u); G[6] = G[16]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x78u); G[5] = 0x08B447B0u; call_here(rt, ctx, G[2]); G[4] = word(rt, G[16] + 0xB0u); G[6] = G[16]; G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x6Cu); G[5] = 0x08B45EB8u; } G[16] = s0; jump(ctx, G[25]); }
void func_08B42060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 0x58u; G[5] = G[7] + 0x1Au; G[17] = G[7]; G[16] = G[4]; if (G[8] == G[2]) { G[2] = word(rt, G[4] + 0xB0u); G[3] = word(rt, G[2]); G[6] = word(rt, G[3] + 0x50u); G[4] = G[2]; call_here(rt, ctx, G[6]); G[5] = load16(G[17] + 8u); G[4] = G[16]; call_here(rt, ctx, 0x08B41F70u); G[4] = G[16]; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B402ACu); }
void func_08B42D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = word(rt, G[4]); put(rt, frame.at(8u), 0u); put(rt, frame.at(0u), 0u); store16(frame.at(8u), G[5]); put(rt, frame.at(4u), 0u); G[2] = word(rt, G[3] + 0xA0u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[2] ^= 1u; store8(frame.at(0u), G[2]); G[4] = G[16]; G[6] = frame.at(0u); G[2] = 0xFFFFFFFFu; G[5] = 0x4E28u; G[7] = 0xCu; G[8] = 1u; G[9] = 1u; G[10] = 0u; G[11] = 0u; put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08B42A90u); } G[16] = s0; }
void func_08B463D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 8u; G[18] = G[5]; G[17] = 0u; G[16] = G[4] + 0x489Cu; G[4] = G[16]; bool hit = false; for (;;) { G[5] = G[18]; call_here(rt, ctx, 0x08B3F964u); G[17] += 1u; G[16] += 0x16u; if (G[2] != 0u) { hit = true; break; } G[4] = G[16]; if (G[17] == G[19]) break; } G[2] = hit ? 1u : 0u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B4845C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; std::uint32_t target; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[5]); G[5] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0xA4u); target = G[25]; } else { G[2] = word(rt, G[16] + 0x40u); G[5] = G[2]; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x28u); call_here(rt, ctx, 0x08B2CE7Cu); } G[4] = word(rt, G[17] + 0x28u); G[5] = G[16]; target = 0x08B2CE7Cu; } } G[16] = s0; G[17] = s1; jump(ctx, target); }
void func_08B485C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAB78u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0xCu; put(rt, G[16] + 4u, 0u); put(rt, G[16] + 8u, 0u); call_here(rt, ctx, 0x08C52464u); G[4] = G[16] + 0x2Cu; put(rt, G[16] + 0x18u, 0u); put(rt, G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x20u, 0u); put(rt, G[16] + 0x24u, 0u); put(rt, G[16] + 0x28u, 0u); call_here(rt, ctx, 0x08C52494u); G[2] = 0u; G[3] = 0u; put(rt, G[16] + 0x40u, G[2]); put(rt, G[16] + 0x44u, G[3]); put(rt, G[16] + 0x38u, 0u); store8(G[16] + 0x3Cu, 0u); } G[16] = s0; }
void func_08B4EB70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x40u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B4D24Cu); ctx.fpr[2] = float_at(rt, frame.at(0xCu)); ctx.fpr[3] = float_at(rt, frame.at(0x18u)); ctx.fpr[4] = float_at(rt, frame.at(4u)); ctx.fpr[5] = float_at(rt, frame.at(0x10u)); ctx.fpr[6] = float_at(rt, frame.at(0x1Cu)); ctx.fpr[7] = float_at(rt, frame.at(8u)); ctx.fpr[8] = float_at(rt, frame.at(0x14u)); ctx.fpr[1] = float_at(rt, frame.at(0x20u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); put_float(rt, G[16] + 0xCu, ctx.fpr[2]); put_float(rt, G[16], ctx.fpr[0]); put_float(rt, G[16] + 0x20u, ctx.fpr[1]); put_float(rt, G[16] + 0x18u, ctx.fpr[3]); put_float(rt, G[16] + 4u, ctx.fpr[4]); put_float(rt, G[16] + 0x10u, ctx.fpr[5]); put_float(rt, G[16] + 0x1Cu, ctx.fpr[6]); put_float(rt, G[16] + 8u, ctx.fpr[7]); put_float(rt, G[16] + 0x14u, ctx.fpr[8]); } G[16] = s0; }
void func_08B52958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = word(rt, G[4] + 0x14u); G[17] = word(rt, G[4] + 0x10u); G[19] = word(rt, G[4] + 0x1Cu); G[16] = word(rt, G[4] + 0x18u); G[5] = G[18]; G[19] -= G[18]; G[16] -= G[17]; G[4] = G[17]; G[6] = G[16]; G[7] = G[19]; call_here(rt, ctx, 0x08B58A54u); G[4] = G[17]; G[5] = G[18]; G[6] = G[16]; G[7] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B58978u); }
void func_08B805F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_errno(rt, ctx, 0x08B77954u, {ctx.gpr[5]});
}
void func_08B82E54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[6] = 0xFF80u; G[8] = G[4]; G[9] = G[4] < 4u ? 1u : 0u; G[10] = G[5] - 0x40u; G[4] = 0x80440000u; G[2] = G[6] < G[10] ? 1u : 0u; G[6] = 0x80440010u; if (G[9] != 0u) { G[3] = 0x80440000u; G[4] = G[5] % 0x40u; G[6] = 0x80440011u; if (G[2] == 0u) { G[9] = 0x08D70000u; G[7] = G[8] << 2; G[2] = 0x80440000u; G[8] = 0x08D740ECu; G[3] = G[7] + G[8]; G[6] = 0x80440011u; if (G[4] == 0u) { G[4] = word(rt, G[3]); call_here(rt, ctx, 0x08C7ABD8u); G[6] = G[2]; } } } G[2] = G[6]; } }
void func_08BA0304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08BA0210u); G[4] = G[16]; call_here(rt, ctx, 0x08BA0140u); put(rt, G[16] + 4u, 0u); G[5] = G[16]; G[4] = G[17]; put(rt, G[17], 0u); G[6] = 0u; put(rt, G[17] + 4u, 0u); store8(G[17] + 8u, 0u); call_here(rt, ctx, 0x08B9E548u); G[2] = 1u; store8(G[17] + 8u, G[2]); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08BA616C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[16] = word(rt, G[4] + 8u); G[4] = G[16]; if (G[16] != 0u) { G[5] = word(rt, G[2] + 0x10u); call_here(rt, ctx, 0x08AF98F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AF7FECu); G[4] = G[16]; G[5] = 0u; if (G[2] == 0u) { call_here(rt, ctx, 0x08AF9010u); G[5] = word(rt, G[16] + 0x68u); G[4] = word(rt, G[5]); G[6] = 0u; G[7] = 0u; tail = true; } } } G[16] = s0; if (tail) jump(ctx, 0x08AF93DCu); }
void func_08BAC53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[12] = G[5]; G[10] = G[4]; G[9] = G[6]; if (G[4] != G[5]) { G[11] = G[4]; do { G[11] += 0x20u; if (G[9] != 0u) { G[2] = word(rt, G[10]); G[3] = word(rt, G[10] + 4u); G[4] = word(rt, G[10] + 8u); G[5] = word(rt, G[10] + 0xCu); G[6] = word(rt, G[10] + 0x10u); G[7] = word(rt, G[10] + 0x14u); G[8] = word(rt, G[10] + 0x18u); put(rt, G[9], G[2]); put(rt, G[9] + 4u, G[3]); put(rt, G[9] + 8u, G[4]); put(rt, G[9] + 0xCu, G[5]); put(rt, G[9] + 0x10u, G[6]); put(rt, G[9] + 0x14u, G[7]); put(rt, G[9] + 0x18u, G[8]); G[2] = load8(G[10] + 0x1Cu); store8(G[9] + 0x1Cu, G[2]); } G[10] = G[11]; G[9] += 0x20u; } while (G[12] != G[11]); } G[2] = G[9]; }
void func_08BAC618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[12] = G[5]; G[10] = G[4]; G[9] = G[6]; if (G[4] != G[5]) { G[11] = G[4]; do { G[11] += 0x20u; if (G[9] != 0u) { G[2] = word(rt, G[10]); G[3] = word(rt, G[10] + 4u); G[4] = word(rt, G[10] + 8u); G[5] = word(rt, G[10] + 0xCu); G[6] = word(rt, G[10] + 0x10u); G[7] = word(rt, G[10] + 0x14u); G[8] = word(rt, G[10] + 0x18u); put(rt, G[9], G[2]); put(rt, G[9] + 4u, G[3]); put(rt, G[9] + 8u, G[4]); put(rt, G[9] + 0xCu, G[5]); put(rt, G[9] + 0x10u, G[6]); put(rt, G[9] + 0x14u, G[7]); put(rt, G[9] + 0x18u, G[8]); G[2] = load8(G[10] + 0x1Cu); store8(G[9] + 0x1Cu, G[2]); } G[10] = G[11]; G[9] += 0x20u; } while (G[12] != G[11]); } G[2] = G[9]; }
void func_08BBA7E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[12] = G[5]; G[8] = G[4]; G[10] = G[6]; if (G[4] != G[5]) { G[11] = G[4]; do { G[11] += 0x20u; if (G[10] != 0u) { G[9] = word(rt, G[8] + 0x1Cu); G[2] = word(rt, G[8]); G[3] = word(rt, G[8] + 4u); G[4] = word(rt, G[8] + 8u); G[5] = word(rt, G[8] + 0xCu); G[6] = word(rt, G[8] + 0x10u); G[7] = word(rt, G[8] + 0x14u); G[8] = word(rt, G[8] + 0x18u); put(rt, G[10], G[2]); put(rt, G[10] + 4u, G[3]); put(rt, G[10] + 8u, G[4]); put(rt, G[10] + 0xCu, G[5]); put(rt, G[10] + 0x10u, G[6]); put(rt, G[10] + 0x14u, G[7]); put(rt, G[10] + 0x18u, G[8]); put(rt, G[10] + 0x1Cu, G[9]); } G[8] = G[11]; G[10] += 0x20u; } while (G[12] != G[11]); } G[2] = G[10]; }
void func_08BBAF0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[12] = G[5]; G[8] = G[4]; G[10] = G[6]; if (G[4] != G[5]) { G[11] = G[4]; do { G[11] += 0x20u; if (G[10] != 0u) { G[9] = word(rt, G[8] + 0x1Cu); G[2] = word(rt, G[8]); G[3] = word(rt, G[8] + 4u); G[4] = word(rt, G[8] + 8u); G[5] = word(rt, G[8] + 0xCu); G[6] = word(rt, G[8] + 0x10u); G[7] = word(rt, G[8] + 0x14u); G[8] = word(rt, G[8] + 0x18u); put(rt, G[10], G[2]); put(rt, G[10] + 4u, G[3]); put(rt, G[10] + 8u, G[4]); put(rt, G[10] + 0xCu, G[5]); put(rt, G[10] + 0x10u, G[6]); put(rt, G[10] + 0x14u, G[7]); put(rt, G[10] + 0x18u, G[8]); put(rt, G[10] + 0x1Cu, G[9]); } G[8] = G[11]; G[10] += 0x20u; } while (G[12] != G[11]); } G[2] = G[10]; }
void func_08BFA300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[5] - G[4]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 3); if (static_cast<std::int32_t>(G[5]) > 0) { G[2] = word(rt, G[6]); G[9] = word(rt, G[2] + 0x134u); for (;;) { G[7] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[2] = G[7] << 3; G[8] = G[2] + G[4]; G[3] = word(rt, G[8]); G[2] = word(rt, G[3] + 0x134u); G[3] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[9]) ? 1u : 0u; G[2] = static_cast<std::int32_t>(G[9]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; bool right = false; if (G[2] == 0u) { G[5] -= G[7]; if (G[3] != 0u) right = true; else { G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[8] + 4u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; right = G[2] == 0u; } } if (right) { G[7] = G[5] - 1u; G[4] = G[8] + 8u; } G[5] = G[7]; if (static_cast<std::int32_t>(G[7]) <= 0) break; } } G[2] = G[4]; }
void func_08BFA370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[5] - G[4]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 3); if (static_cast<std::int32_t>(G[5]) > 0) { G[2] = word(rt, G[6]); G[9] = word(rt, G[2] + 0x134u); for (;;) { G[7] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[2] = G[7] << 3; G[8] = G[2] + G[4]; G[3] = word(rt, G[8]); G[2] = word(rt, G[3] + 0x134u); G[3] = static_cast<std::int32_t>(G[9]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[9]) ? 1u : 0u; G[5] -= G[7]; bool right = G[2] != 0u; if (!right && G[3] == 0u) { G[2] = word(rt, G[8] + 4u); G[3] = word(rt, G[6] + 4u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; right = G[2] != 0u; } if (right) { G[7] = G[5] - 1u; G[4] = G[8] + 8u; } G[5] = G[7]; if (static_cast<std::int32_t>(G[7]) <= 0) break; } } G[2] = G[4]; }
void func_08C0D8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    construct_with_extra_word(rt, ctx);
}
void func_08C0DB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[17] = G[6]; if (G[4] != G[5]) { G[16] = G[4]; G[18] = G[4]; do { G[4] = G[16]; G[5] = G[17]; if (G[17] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } G[16] = G[18] + 0x20u; G[18] = G[16]; } while (G[16] != G[19]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C47034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[11] = G[5]; G[10] = G[6]; if (G[4] != G[5]) { G[8] = G[4]; for (;;) { G[6] = G[4]; if (G[10] != 0u) { G[7] = G[10]; G[9] = G[4] + 0x50u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[9]); G[2] = word(rt, G[6]); put(rt, G[7], G[2]); } G[8] += 0x54u; if (G[11] == G[8]) { G[10] += 0x54u; break; } G[10] += 0x54u; G[4] = G[8]; } } G[2] = G[10]; }
void func_08C70078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] - 0xCu); G[2] = G[4] + G[2]; G[4] = word(rt, G[2] + 0x78u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x30u); G[16] = G[6]; call_here(rt, ctx, G[2]); if (G[16] != G[2]) { G[2] = word(rt, G[17]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[17] + G[4]; G[5] = word(rt, G[4] + 0x14u) | 1u; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08C54CFCu); }
void func_08C73368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[5] + 0x1Cu; G[16] = G[5]; G[5] = G[18]; G[19] = G[4]; G[17] = G[6]; call_here(rt, ctx, 0x08B5C450u); G[4] = G[16]; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[5] = G[17]; call_here(rt, ctx, G[2]); G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x08B5D1E0u); G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_193
} // namespace gt::game

#define HOT193(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_193::name, gt::ReturnKind::kind)
HOT193(0x088184F0u, func_088184F0, Word);
HOT193(0x0881C378u, func_0881C378, Word);
// HOT193(0x088390FCu, func_088390FC, Word);  // no comparable synthetic case yet
HOT193(0x08843B60u, func_08843B60, Word);
HOT193(0x08858A54u, func_08858A54, Word);
HOT193(0x08A2D5A0u, func_08A2D5A0, Word);
HOT193(0x08A31A18u, func_08A31A18, Word);
HOT193(0x08A37928u, func_08A37928, Word);
HOT193(0x08C58A20u, func_08C58A20, Word);
HOT193(0x08C58A90u, func_08C58A90, Word);
HOT193(0x08C58CB8u, func_08C58CB8, Word);
HOT193(0x08C58D28u, func_08C58D28, Word);
HOT193(0x08B11918u, func_08B11918, Word);
HOT193(0x08B11988u, func_08B11988, Word);
// HOT193(0x08B10CC4u, func_08B10CC4, Word);  // no comparable synthetic case yet
// HOT193(0x08B11434u, func_08B11434, Word);  // no comparable synthetic case yet
// HOT193(0x08B0D510u, func_08B0D510, Word);  // no comparable synthetic case yet
// HOT193(0x08B0D790u, func_08B0D790, Word);  // no comparable synthetic case yet
HOT193(0x08AF2500u, func_08AF2500, Word);
HOT193(0x08AF25D0u, func_08AF25D0, Word);
HOT193(0x08A8BB2Cu, func_08A8BB2C, Word);
HOT193(0x08A8BB9Cu, func_08A8BB9C, Word);
// HOT193(0x08A5A28Cu, func_08A5A28C, Word);  // no comparable synthetic case yet
// HOT193(0x08AAC738u, func_08AAC738, Word);  // no comparable synthetic case yet
HOT193(0x08A565C0u, func_08A565C0, Word);
HOT193(0x08A5670Cu, func_08A5670C, Word);
HOT193(0x08A04F44u, func_08A04F44, Word);
HOT193(0x08A051A4u, func_08A051A4, Word);
HOT193(0x08974718u, func_08974718, Word);
HOT193(0x089749A4u, func_089749A4, Word);
HOT193(0x089A0B28u, func_089A0B28, Float);
HOT193(0x089A0DB4u, func_089A0DB4, Float);
HOT193(0x089F7BD4u, func_089F7BD4, Word);
HOT193(0x089F7C44u, func_089F7C44, Word);
HOT193(0x08A55B48u, func_08A55B48, Word);
HOT193(0x08AABDA4u, func_08AABDA4, Word);
HOT193(0x0881799Cu, func_0881799C, Word);
HOT193(0x0884C664u, func_0884C664, Word);
HOT193(0x0885BEE0u, func_0885BEE0, Word);
HOT193(0x08871568u, func_08871568, Word);
HOT193(0x08875330u, func_08875330, Word);
// HOT193(0x08879FC8u, func_08879FC8, Word);  // no comparable synthetic case yet
// HOT193(0x0887E908u, func_0887E908, Word);  // no comparable synthetic case yet
HOT193(0x088822CCu, func_088822CC, Word);
HOT193(0x08891650u, func_08891650, Word);
HOT193(0x08892F74u, func_08892F74, Word);
HOT193(0x08892FE4u, func_08892FE4, Word);
HOT193(0x088983DCu, func_088983DC, Word);
HOT193(0x08898744u, func_08898744, Word);
HOT193(0x088AC66Cu, func_088AC66C, Word);
HOT193(0x088B66E8u, func_088B66E8, Word);
// HOT193(0x088E6A9Cu, func_088E6A9C, Word);  // no comparable synthetic case yet
// HOT193(0x088E6F94u, func_088E6F94, Word);  // no comparable synthetic case yet
// HOT193(0x088F1CC0u, func_088F1CC0, Word);  // no comparable synthetic case yet
HOT193(0x088F6A74u, func_088F6A74, Word);
HOT193(0x08906160u, func_08906160, Word);
// HOT193(0x08909928u, func_08909928, Word);  // no comparable synthetic case yet
HOT193(0x0890B19Cu, func_0890B19C, Word);
HOT193(0x08911ED8u, func_08911ED8, Word);
HOT193(0x08938018u, func_08938018, Word);
// HOT193(0x08939828u, func_08939828, Word);  // no comparable synthetic case yet
HOT193(0x0893A2ACu, func_0893A2AC, Word);
HOT193(0x08947C04u, func_08947C04, Float);
HOT193(0x0896B1F0u, func_0896B1F0, Word);
HOT193(0x0899C2CCu, func_0899C2CC, Word);
HOT193(0x089AC0A0u, func_089AC0A0, Word);
HOT193(0x089B4098u, func_089B4098, Word);
HOT193(0x089B601Cu, func_089B601C, Word);
// HOT193(0x089BF71Cu, func_089BF71C, Word);  // no comparable synthetic case yet
// HOT193(0x089E63F0u, func_089E63F0, Word);  // no comparable synthetic case yet
HOT193(0x089F9BFCu, func_089F9BFC, Word);
HOT193(0x08A10268u, func_08A10268, Word);
HOT193(0x08A15534u, func_08A15534, Word);
HOT193(0x08A202F0u, func_08A202F0, Word);
HOT193(0x08A25428u, func_08A25428, Word);
// HOT193(0x08A39B70u, func_08A39B70, Word);  // no comparable synthetic case yet
HOT193(0x08A57714u, func_08A57714, Word);
HOT193(0x08A59DC4u, func_08A59DC4, Word);
HOT193(0x08A5A764u, func_08A5A764, Word);
HOT193(0x08A7677Cu, func_08A7677C, Word);
HOT193(0x08A77BE4u, func_08A77BE4, Word);
HOT193(0x08A88F54u, func_08A88F54, Word);
HOT193(0x08A8B098u, func_08A8B098, Word);
// HOT193(0x08AAD510u, func_08AAD510, Word);  // no comparable synthetic case yet
HOT193(0x08ABAD70u, func_08ABAD70, Word);
HOT193(0x08ABC318u, func_08ABC318, Word);
HOT193(0x08ABFE9Cu, func_08ABFE9C, Word);
// HOT193(0x08AC38ACu, func_08AC38AC, Word);  // no comparable synthetic case yet
// HOT193(0x08AC77B0u, func_08AC77B0, Word);  // no comparable synthetic case yet
// HOT193(0x08ACE96Cu, func_08ACE96C, Word);  // no comparable synthetic case yet
HOT193(0x08ACEA1Cu, func_08ACEA1C, Word);
HOT193(0x08AD3A8Cu, func_08AD3A8C, Word);
HOT193(0x08AD8F88u, func_08AD8F88, Word);
HOT193(0x08ADEA5Cu, func_08ADEA5C, Word);
HOT193(0x08AE3BE8u, func_08AE3BE8, Word);
HOT193(0x08AFAAACu, func_08AFAAAC, Word);
HOT193(0x08B0F1F4u, func_08B0F1F4, Word);
HOT193(0x08B15020u, func_08B15020, Word);
HOT193(0x08B1830Cu, func_08B1830C, Word);
HOT193(0x08B195ECu, func_08B195EC, Word);
HOT193(0x08B2B118u, func_08B2B118, Word);
HOT193(0x08B2B37Cu, func_08B2B37C, Word);
HOT193(0x08B2E13Cu, func_08B2E13C, Word);
HOT193(0x08B2ECF0u, func_08B2ECF0, Word);
HOT193(0x08B38A14u, func_08B38A14, Word);
// HOT193(0x08B40080u, func_08B40080, Word);  // no comparable synthetic case yet
HOT193(0x08B42060u, func_08B42060, Word);
HOT193(0x08B42D10u, func_08B42D10, Word);
HOT193(0x08B463D4u, func_08B463D4, Word);
HOT193(0x08B4845Cu, func_08B4845C, Word);
HOT193(0x08B485C8u, func_08B485C8, Word);
HOT193(0x08B4EB70u, func_08B4EB70, Word);
HOT193(0x08B52958u, func_08B52958, Word);
HOT193(0x08B805F8u, func_08B805F8, Word);
HOT193(0x08B82E54u, func_08B82E54, Word);
HOT193(0x08BA0304u, func_08BA0304, Word);
HOT193(0x08BA616Cu, func_08BA616C, Word);
HOT193(0x08BAC53Cu, func_08BAC53C, Word);
HOT193(0x08BAC618u, func_08BAC618, Word);
HOT193(0x08BBA7E4u, func_08BBA7E4, Word);
HOT193(0x08BBAF0Cu, func_08BBAF0C, Word);
HOT193(0x08BFA300u, func_08BFA300, Word);
HOT193(0x08BFA370u, func_08BFA370, Word);
HOT193(0x08C0D8F8u, func_08C0D8F8, Word);
HOT193(0x08C0DB4Cu, func_08C0DB4C, Word);
HOT193(0x08C47034u, func_08C47034, Word);
// HOT193(0x08C70078u, func_08C70078, Word);  // no comparable synthetic case yet
// HOT193(0x08C73368u, func_08C73368, Word);  // no comparable synthetic case yet
