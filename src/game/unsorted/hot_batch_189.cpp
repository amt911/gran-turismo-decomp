// Hot batch 189: twenty-four-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_189 {
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
// Construct copies over [first, last) into `dest` with `construct(dest, source)` (a null destination slot is skipped), `step`
// bytes per element; v0 is the end of the destination.
void construct_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 construct, u32 step) {
    u32 source = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    u32 dest = ctx.gpr[6];
    if (source != last) {
        do {
            const u32 element = source;
            source += step;
            if (dest != 0u) call(rt, ctx, construct, {dest, element});
            dest += step;
        } while (source != last);
    }
    ctx.gpr[2] = dest;
}
// Destroy the elements of [first, last), `step` bytes apart: `finish(word(e + a), word(e + b), &slot)` then release the word at +a.
void destroy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 finish, u32 first_offset, u32 second_offset, u32 step, u32 slot) {
    u32 cursor = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    while (cursor != last) {
        call(rt, ctx, finish, {word(rt, cursor + first_offset), word(rt, cursor + second_offset), slot});
        const u32 member = word(rt, cursor + first_offset);
        ctx.gpr[2] = member;
        cursor += step;
        if (member != 0u) call(rt, ctx, 0x08B35254u, {member});
    }
}
// Follow `advance` from the object at word(self + member) while bit 12 of its word +0xE0 is set; v0 is where it stopped.
void follow_flagged(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 member, u32 advance) {
    u32 object = word(rt, ctx.gpr[4] + member);
    if (object != 0u) {
        while ((word(rt, object + 0xE0u) & 0x1000u) != 0u) {
            object = call(rt, ctx, advance, {object});
            if (object == 0u) break;
        }
    }
    ctx.gpr[2] = object;
}
// Release a counted shared string: 0x08B60804(rep - 4, -1) leaving it alive when it returns > 0, else 0x08C74884(rep, &slot).
void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    const u32 characters = word(rt, ctx.gpr[4]);
    const u32 rep = characters - 0xCu;
    ctx.gpr[2] = 0x08D77498u;
    if (rep != 0x08D77498u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {characters - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08C74884u, {rep, slot});
    }
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088040AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 0x08B7684Cu; G[4] = 0x08C7C430u; G[5] = 0x08CE1404u; call_here(rt, ctx, 0x08B7684Cu); G[4] = high_half(0x08C7C42Cu); G[3] = word(rt, 0x08C7C42Cu); G[2] = 0u; G[25] = 0u; if (G[3] != 0u) G[4] = 0x08C7C42Cu; } }
void func_0880BAD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B344E0u); ctx.fpr[12] = ctx.fpr[20]; G[5] = word(rt, G[17]); G[6] = word(rt, G[18]); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08869BB4u); G[4] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; ctx.fpr[20] = f20; jump(ctx, 0x08B3457Cu); }
void func_0881A664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x1Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x0881A5B4u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08844530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x1Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08844478u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08845694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x1Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x088455D4u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088294D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[4] + 0x18u) + 0x8u; G[4] = G[2]; if (G[2] != G[5]) { G[5] = word(rt, G[5]); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x088294D0u); G[3] = word(rt, G[16]); G[4] = G[2]; G[5] = 0u; if (G[3] != 0u) G[5] = G[3] + 0x14u; } G[16] = s0; G[17] = s1; jump(ctx, 0x0885EFE0u); }
void func_08829538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[4] + 0x18u) + 0xCu; G[4] = G[2]; if (G[2] != G[5]) { G[5] = word(rt, G[5]); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x088294D0u); G[3] = word(rt, G[16]); G[4] = G[2]; G[5] = 0u; if (G[3] != 0u) G[5] = G[3] + 0x18u; } G[16] = s0; G[17] = s1; jump(ctx, 0x0885EFECu); }
void func_08829D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088E0C50u); } G[16] = s0; }
void func_08829DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EE70u); G[4] = G[2]; call_here(rt, ctx, 0x08891574u); } G[16] = s0; }
void func_0882A278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885E710u); G[4] = G[2]; call_here(rt, ctx, 0x08917E90u); } G[16] = s0; }
void func_0885E5BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] << 6; G[4] = word(rt, G[4]); G[5] <<= 4; G[5] += G[2]; G[5] += 0x4120u; G[3] = G[4] + G[5]; G[2] = load8(G[3] + 0x18u); G[5] = 0xFFFFFFFFu; if (G[2] == 0u) { G[3] = sext(load16(G[3] + 0x1Eu), 16u); G[5] = word(rt, G[4] + 0x3F50u); G[4] = G[3] << 6; G[3] <<= 3; G[4] -= G[3]; G[3] = word(rt, G[5]); G[2] = G[4] << 3; G[2] -= G[4]; G[3] += G[2]; G[5] = word(rt, G[3] + 0x2Cu); G[2] = 0xFFFFFFFFu; if (G[5] == G[2]) G[5] = word(rt, G[3] + 0x1Cu); } G[2] = G[5]; }
void func_0887E430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC1020u; G[17] = G[4] + 0x70u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x70u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x70u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x0886A858u); }
void func_0887E630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC0FE8u; G[17] = G[4] + 0x70u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x70u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x70u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x0886A858u); }
void func_0887E690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC1058u; G[17] = G[4] + 0x70u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x70u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x70u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x0886A858u); }
void func_0887E7C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC1090u; G[17] = G[4] + 0x70u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x70u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x70u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x0886A858u); }
void func_08884DF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u << (G[5] & 31u); G[3] = sra(G[5], 5u) << 2; G[7] = ~G[2]; G[6] &= 0xFFu; G[2] = G[4] + 0x198u; G[3] += G[2]; if (G[6] == 0u) { G[2] = word(rt, G[3]); G[2] &= G[7]; put(rt, G[3], G[2]); return; } if (static_cast<std::int32_t>(G[5]) < 0) return; G[6] = static_cast<std::int32_t>(G[5]) < 0x400 ? 1u : 0u; G[4] = G[3]; G[3] = 1u << (G[5] & 31u); if (G[6] == 0u) return; G[2] = word(rt, G[4]); G[2] |= G[3]; put(rt, G[4], G[2]); }
void func_08884E54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool second = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0u; G[16] = G[4]; if (static_cast<std::int32_t>(G[17]) < 0) G[5] = 1u; else { call_here(rt, ctx, 0x08884DA0u); G[4] = G[16]; G[5] = G[17]; G[6] = 1u; second = true; } } G[16] = s0; G[17] = s1; jump(ctx, second ? 0x08884DF4u : 0x08884DA0u); }
void func_0888BA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0xB0u); G[18] = G[5]; G[5] = 1u; G[17] = G[6]; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x088D7744u); G[4] = word(rt, G[16] + 0x18u); G[5] = frame.at(0u); G[2] = 0x15u; put(rt, frame.at(8u), G[18]); put(rt, frame.at(0xCu), G[17]); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x088936A4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0889095C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x0888F548u); G[3] = G[2]; G[2] = __builtin_bswap32(word(rt, G[2] + 4u)); const bool swapped = G[2] != 0u; G[2] = word(rt, G[3] + 0xDCu); if (swapped) G[3] = __builtin_bswap32(G[2]); else { G[3] = word(rt, G[16]); G[2] = __builtin_bswap32(G[2]); G[3] = G[2] + G[3]; } G[2] = (G[17] << 5) + G[3]; } G[16] = s0; G[17] = s1; }
void func_08890AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x0888F548u); G[3] = G[2]; G[2] = __builtin_bswap32(word(rt, G[2] + 4u)); const bool swapped = G[2] != 0u; G[2] = word(rt, G[3] + 0xECu); if (swapped) G[3] = __builtin_bswap32(G[2]); else { G[3] = word(rt, G[16]); G[2] = __builtin_bswap32(G[2]); G[3] = G[2] + G[3]; } G[2] = (G[17] << 5) + G[3]; } G[16] = s0; G[17] = s1; }
void func_08890B44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x0888F548u); G[3] = G[2]; G[2] = __builtin_bswap32(word(rt, G[2] + 4u)); const bool swapped = G[2] != 0u; G[2] = word(rt, G[3] + 0xF4u); if (swapped) G[3] = __builtin_bswap32(G[2]); else { G[3] = word(rt, G[16]); G[2] = __builtin_bswap32(G[2]); G[3] = G[2] + G[3]; } G[2] = (G[17] << 4) + G[3]; } G[16] = s0; G[17] = s1; }
void func_08893DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] + 4u; G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = G[17]; G[3] = word(rt, G[2] + 4u); put(rt, G[5], G[3]); G[2] = word(rt, G[16]); G[6] = word(rt, G[2] + 4u); G[5] = word(rt, G[2] + 8u); G[6] <<= 5; call_here(rt, ctx, 0x08B79A88u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 4u) << 5; G[2] = G[17] + G[2]; } G[16] = s0; G[17] = s1; }
void func_08893E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5] + 4u; G[4] = word(rt, G[5]); G[3] = word(rt, G[17]); G[5] = G[16]; put(rt, G[3] + 4u, G[4]); G[2] = word(rt, G[17]); G[6] = word(rt, G[2] + 4u); G[4] = word(rt, G[2] + 8u); G[6] <<= 5; call_here(rt, ctx, 0x08B79A88u); G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 4u) << 5; G[2] = G[16] + G[2]; } G[16] = s0; G[17] = s1; }
void func_088A5F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x088A5EE0u); G[4] = G[16]; G[18] = G[2]; G[19] = G[3]; call_here(rt, ctx, 0x088A5EE0u); G[4] = 0u; if (G[2] != G[18] || G[3] != G[19]) G[4] = 1u; G[2] = (G[4] ^ 1u) & 1u; } G[16] = s0; G[18] = s2; G[19] = s3; }
void func_088BBCF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x088BBC40u); ctx.fpr[0] = 0.0f; if (G[2] != 0u) { G[4] = G[16]; G[5] = 1u; if (G[17] != 0u) { call_here(rt, ctx, 0x088BBC70u); ctx.fpr[0] = float_at(rt, G[2] + 0x14u); put_float(rt, G[17], ctx.fpr[0]); } G[2] = word(rt, G[16] + 0x24u); ctx.fpr[0] = float_at(rt, G[2] + 0x1Cu); } } G[16] = s0; G[17] = s1; }
void func_088BBD58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x088BBC40u); ctx.fpr[0] = 0.0f; if (G[2] != 0u) { G[4] = G[16]; G[5] = 0u; if (G[17] != 0u) { call_here(rt, ctx, 0x088BBC70u); ctx.fpr[0] = float_at(rt, G[2] + 0x14u); put_float(rt, G[17], ctx.fpr[0]); } G[2] = word(rt, G[16] + 0x24u); ctx.fpr[0] = float_at(rt, G[2] + 0x18u); } } G[16] = s0; G[17] = s1; }
void func_088CF108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[5] = word(rt, G[4] + 0x10u); G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088DA788u); G[4] = G[2]; G[3] = word(rt, G[2] + 0x14u); G[2] = word(rt, G[2] + 0x18u); G[5] = 1u; if (G[3] != G[2]) G[5] = 0u; else { ctx.fpr[2] = float_at(rt, G[4]); ctx.fpr[1] = float_at(rt, G[4] + 0xCu); G[1] = 0x41A00000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; if (ctx.fpr[0] < ctx.fpr[1]) G[5] = 0u; } G[2] = G[5]; } }
void func_088D0F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[6] + 0x14Cu); G[2] = 0u; if (G[3] != 0u) { G[3] = word(rt, G[3] + 0x40u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x1Cu); call_here(rt, ctx, G[2]); G[4] = G[2]; if (G[2] != 0u) { G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x28u); call_here(rt, ctx, G[2]); } else G[2] = 0u; } } } }
void func_088D60B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[16] = high_half(0x08CE9BE8u); G[6] = 0x08CE9BE8u; G[7] = word(rt, G[4]); G[16] = 0x08CE9BE8u; G[4] = word(rt, G[7] + 0x28u); G[2] = word(rt, G[7] + 0x2Cu); G[7] = 0x40u; G[4] = __builtin_bswap32(G[4]); G[2] = __builtin_bswap32(G[2]); G[5] = G[4]; G[4] = 0u; G[4] |= G[2]; G[5] |= G[3]; call_here(rt, ctx, 0x0892FFE0u); if (G[2] == 0u) G[16] = 0u; G[2] = G[16]; } G[16] = s0; }
void func_088DA9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x088D9F80u); G[4] = G[16] << 5; G[3] = G[16] << 7; G[3] -= G[4]; G[3] -= G[16]; G[4] = G[3] << 3; G[3] = 0xFFFFFFFFu; if (G[2] != 0u) { G[2] = word(rt, G[17] + 0xC0u); G[2] = G[4] + G[2]; G[4] = word(rt, G[2] + 0x1FCu); G[3] = static_cast<std::int32_t>(G[4]) > static_cast<std::int32_t>(G[3]) ? G[4] : G[3]; } G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_088DE874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x088D9E58u); if (G[2] == 0u) { G[2] = load8(G[16] + 0x20u); G[4] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x088DC48Cu); G[4] = G[16]; call_here(rt, ctx, 0x088D9E58u); } } else { G[2] = load8(G[16] + 0x21u); G[4] = G[16]; if (G[2] != 0u) call_here(rt, ctx, 0x088DE740u); } } G[16] = s0; }
void func_088E4754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4]) + 0x1C8u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[17]); G[16] += G[2]; G[18] = sext(load8(G[16] + 0x460u), 8u); store8(G[16] + 0x460u, 0u); G[4] = word(rt, G[17]) + 0x1C8u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088E4AA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x38u); call_here(rt, ctx, G[2]); G[3] = 0u; if (G[2] != 0u) { G[4] = word(rt, G[16]); G[2] = load8(G[4] + 0x525u); G[3] = 1u; if (G[2] == 0u) G[3] = 0u; else { G[2] = load8(G[4] + 0x526u); if (G[2] == 0u) G[3] = 0u; } } G[2] = G[3]; } G[16] = s0; }
void func_088EB3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xBCu); call_here(rt, ctx, G[2]); const bool ok = G[2] != 0u; G[2] = 0xFFFFFFFFu; G[4] = 0u; if (ok) { G[3] = word(rt, G[16] + 0x558u); if (G[3] != G[2]) { G[4] = 1u; G[3] = word(rt, G[16] + 0x530u); G[2] = 6u; if (G[3] != G[2]) G[4] = 0u; } } G[2] = G[4]; } G[16] = s0; }
void func_088EB754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = 0xFFFFFFFFu; G[2] = G[5]; G[16] = G[4]; bool load = true; if (G[5] == G[17]) { G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x90u); call_here(rt, ctx, G[2]); G[4] = 0u; load = G[2] != G[17]; } if (load) { G[2] <<= 2; G[2] += G[16]; G[4] = word(rt, G[2] + 0x5A8u); } G[2] = G[4]; } G[16] = s0; G[17] = s1; }
void func_088ECC74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[6]); G[3] = G[4]; G[5] = frame.at(8u); G[6] = frame.at(0xCu); put(rt, frame.at(4u), G[7]); do { G[2] = load8(G[3] + 0x4ACu); store8(G[5], G[2]); G[5] += 1u; G[3] += 1u; } while (G[5] != G[6]); G[4] = word(rt, G[4] + 0x538u); G[8] = frame.at(0u); G[5] = 0u; G[3] = word(rt, G[4]); G[6] = 0xFFFFFFFFu; G[7] = 0xCu; G[2] = word(rt, G[3] + 0xF4u); G[9] = 0x10u; call_here(rt, ctx, G[2]); } }
void func_088EE660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B09818u); G[4] = G[16]; if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x118u); call_here(rt, ctx, 0x088EEEB8u); G[4] = G[16]; call_here(rt, ctx, 0x088EE1ECu); G[2] = 6u; } else { G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 0x10u); call_here(rt, ctx, G[3]); G[2] = 0u; } } G[16] = s0; }
void func_088EF210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B083E4u); G[2] = 0x08CC3A38u; G[4] = G[16] + 0x40u; put(rt, G[16], G[2]); G[5] = 0u; G[6] = 0x120u; put(rt, G[16] + 0x38u, 0u); store8(G[16] + 0x280u, 0u); call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x160u; G[5] = 0u; G[6] = 0x120u; call_here(rt, ctx, 0x08B36018u); G[3] = 0xFFFFFFFFu; store8(G[16] + 0x40u, G[3]); } G[16] = s0; }
void func_088F2D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = sext(load16(G[4] + 8u), 16u); G[6] = 0u; if (static_cast<std::int32_t>(G[5]) >= 0) { G[4] = word(rt, G[4] + 4u); G[1] = 0x4B189680u; ctx.fpr[2] = std::bit_cast<float>(G[1]); do { G[2] = G[4] + (G[5] << 6); ctx.fpr[1] = float_at(rt, G[2] + 0x10u); ctx.fpr[0] = float_at(rt, G[2] + 0xCu); G[3] = word(rt, G[2]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); G[3] = sra(G[3] << 19, 19u); if (ctx.fpr[0] < ctx.fpr[2]) { ctx.fpr[2] = ctx.fpr[0]; G[6] = G[5]; } G[5] = G[3]; } while (static_cast<std::int32_t>(G[3]) >= 0); } G[2] = G[6]; }
void func_088F32A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x1ACu); ctx.fpr[4] = float_at(rt, G[6]); ctx.fpr[3] = float_at(rt, G[6] + 4u); ctx.fpr[1] = float_at(rt, G[2] + 4u); ctx.fpr[0] = float_at(rt, G[2]); ctx.fpr[5] = float_at(rt, G[2] + 8u); ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[1]); ctx.fpr[0] = psp_mul(ctx.fpr[4], ctx.fpr[0]); ctx.fpr[2] = float_at(rt, G[6] + 8u); ctx.fpr[1] = float_at(rt, G[2] + 0xCu); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[5]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; ctx.fpr[1] = -ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[1] = ctx.fpr[1] / ctx.fpr[0]; ctx.fpr[4] = psp_mul(ctx.fpr[4], ctx.fpr[1]); put_float(rt, G[5], ctx.fpr[4]); ctx.fpr[0] = float_at(rt, G[6] + 4u); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); put_float(rt, G[5] + 4u, ctx.fpr[0]); ctx.fpr[2] = float_at(rt, G[6] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]); put_float(rt, G[5] + 8u, ctx.fpr[1]); }
void func_088F66F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[4] = float_at(rt, G[5]); ctx.fpr[5] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[4] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[4], ctx.fpr[3]); ctx.fpr[2] = psp_mul(ctx.fpr[5], ctx.fpr[2]); ctx.fpr[6] = float_at(rt, G[5] + 8u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2]; ctx.fpr[0] = psp_mul(ctx.fpr[6], ctx.fpr[0]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[1]; ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[1]); ctx.fpr[3] = ctx.fpr[3] - ctx.fpr[4]; put_float(rt, G[5], ctx.fpr[3]); ctx.fpr[0] = float_at(rt, G[4] + 4u); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[5]; put_float(rt, G[5] + 4u, ctx.fpr[0]); ctx.fpr[2] = float_at(rt, G[4] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[6]; put_float(rt, G[5] + 8u, ctx.fpr[1]); }
void func_088FFFF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[5] + 4u); G[5] = frame.at(4u); put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08BD2A80u); G[4] = G[17] + 0x10u; G[2] = word(rt, G[16] + 8u); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08BD2A58u); G[2] = sext(load16(G[16]), 16u); store16(G[17] + 0x1Cu, G[2]); G[3] = load16(G[16] + 2u); store16(G[17] + 0x1Eu, G[3]); } G[16] = s0; G[17] = s1; }
void func_08905F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x08905F30u); G[4] = word(rt, G[16] + 8u); G[5] = G[17]; call_here(rt, ctx, 0x08905F30u); G[4] = word(rt, G[16] + 0xCu); G[5] = G[17]; call_here(rt, ctx, 0x08905F30u); G[4] = word(rt, G[16] + 0x10u); G[5] = G[17]; call_here(rt, ctx, 0x08905F30u); G[4] = word(rt, G[16] + 0x14u); G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08905F30u); }
void func_08905FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x08905FB4u); G[4] = word(rt, G[16] + 8u); G[5] = G[17]; call_here(rt, ctx, 0x08905FB4u); G[4] = word(rt, G[16] + 0xCu); G[5] = G[17]; call_here(rt, ctx, 0x08905FB4u); G[4] = word(rt, G[16] + 0x10u); G[5] = G[17]; call_here(rt, ctx, 0x08905FB4u); G[4] = word(rt, G[16] + 0x14u); G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08905FB4u); }
void func_08916408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x38u; G[17] = G[4]; G[4] = G[18]; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[2] = G[17] + 0x1Cu; ctx.fpr[0] = float_at(rt, G[16]); ctx.fpr[2] = float_at(rt, G[16] + 4u); ctx.fpr[1] = float_at(rt, G[16] + 8u); put_float(rt, G[17] + 0x1Cu, ctx.fpr[0]); G[4] = G[18]; put_float(rt, G[2] + 8u, ctx.fpr[1]); put_float(rt, G[2] + 4u, ctx.fpr[2]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089200F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = G[5] - 0x1Au; G[2] = G[2] < 4u ? 1u : 0u; G[3] = word(rt, G[4] + 0x244u); G[4] = G[3]; if (G[3] == 0u) G[2] = 0u; else { bool call = G[2] != 0u; if (!call) { G[2] = 0x1Fu; call = G[5] == G[2]; if (!call) { G[2] = 0x20u; call = G[5] == G[2]; if (!call) G[2] = 1u; } } if (call) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0xC0u); call_here(rt, ctx, G[2]); G[2] = G[2] != 0u ? 1u : 0u; } } } }
void func_08927E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089273D8u); G[3] = word(rt, G[2]); G[4] = G[2]; G[2] = word(rt, G[3] + 0xCu); G[5] = 0x20u; call_here(rt, ctx, G[2]); G[4] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x2050u); G[4] = 1u; G[2] += 1u; G[3] = static_cast<std::int32_t>(G[2]) < 2 ? 1u : 0u; put(rt, G[16] + 0x2050u, G[2]); if (G[3] == 0u) put(rt, G[16] + 0x2050u, 0u); } G[2] = G[4]; } G[16] = s0; }
void func_0892BC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC6740u; G[17] = G[4] + 0x260u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x260u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x260u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B22550u); }
void func_08932BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 0xFFFFFFFFu; G[16] = G[4]; G[3] = word(rt, G[4] + 0xC8u); G[4] = 0xFFFFFFFFu; bool run = G[3] != G[2]; if (!run) { G[2] = word(rt, G[16] + 0xCCu); run = G[2] != G[3]; } if (run) { call_here(rt, ctx, 0x089363FCu); G[6] = word(rt, G[16] + 0xC8u); G[7] = word(rt, G[16] + 0xCCu); G[4] = G[2]; G[8] = frame.at(0u); call_here(rt, ctx, 0x08936584u); G[4] = load8(frame.at(0x6u)); } G[2] = G[4]; } G[16] = s0; }
void func_08932CE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x40u); G[2] = 0xFFFFFFFFu; G[16] = G[4]; G[3] = word(rt, G[4] + 0x40u); G[4] = 0xFFFFFFFFu; bool run = G[3] != G[2]; if (!run) { G[2] = word(rt, G[16] + 0x44u); run = G[2] != G[3]; } if (run) { call_here(rt, ctx, 0x089363FCu); G[6] = word(rt, G[16] + 0x40u); G[7] = word(rt, G[16] + 0x44u); G[4] = G[2]; G[8] = frame.at(0u); call_here(rt, ctx, 0x08936584u); G[4] = load8(frame.at(0x22u)); } G[2] = G[4]; } G[16] = s0; }
void func_0893C2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x110u); if (G[2] == 0u) { G[3] = load16(G[5] + 0x2A6u); G[2] = 0x51EB851Fu; const std::uint64_t wide = static_cast<std::uint64_t>(G[3]) * G[2]; ctx.lo = static_cast<std::uint32_t>(wide); ctx.hi = static_cast<std::uint32_t>(wide >> 32); G[3] = ctx.hi >> 5; store8(G[5] + 0x140Cu, G[3]); } else store8(G[5] + 0x140Cu, G[2]); G[4] = load8(G[4] + 0x111u); if (G[4] == 0u) { G[3] = load16(G[5] + 0x23Cu); G[2] = 0x10624DD3u; G[3] += 0x384u; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[3] = ctx.hi >> 6; store8(G[5] + 0x140Du, G[3]); } else store8(G[5] + 0x140Du, G[4]); }
void func_08941978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[5] + 3u); G[1] = 0x3E800000u; ctx.fpr[2] = std::bit_cast<float>(G[1]); G[6] &= 0xFFu; ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); put_float(rt, G[4] + 0x40u, ctx.fpr[0]); ctx.fpr[3] = psp_mul(ctx.fpr[0], ctx.fpr[12]); G[5] = load8(G[5] + 3u); ctx.fpr[1] = std::bit_cast<float>(G[5]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[5])); G[1] = 0x411CCCCDu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); put_float(rt, G[4] + 0x44u, ctx.fpr[0]); if (G[6] != 0u) { put_float(rt, G[4] + 0x44u, ctx.fpr[12]); put_float(rt, G[4] + 0x40u, ctx.fpr[3]); } }
void func_08947BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08947B98u); } G[2] = high_half(0x08CF2EBCu); ctx.fpr[2] = float_at(rt, 0x08CF2EBCu); G[1] = 0x3F800000u; ctx.fpr[3] = std::bit_cast<float>(G[1]); const bool top = ctx.fpr[2] <= ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[0]; if (!top) { G[2] = high_half(0x08CF2EB8u); ctx.fpr[0] = float_at(rt, 0x08CF2EB8u); ctx.fpr[3] = 0.0f; const bool bottom = ctx.fpr[1] <= ctx.fpr[0]; ctx.fpr[2] = ctx.fpr[2] - ctx.fpr[0]; ctx.fpr[0] = ctx.fpr[1] - ctx.fpr[0]; if (!bottom) ctx.fpr[3] = ctx.fpr[0] / ctx.fpr[2]; } ctx.fpr[0] = ctx.fpr[3]; }
void func_089797C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] += 0x190u; G[16] = 0u; G[3] = 4u; for (;;) { G[2] = word(rt, G[4] + 0xCu); if (G[2] == 0u) { G[4] += 0xCu; if (G[4] != G[5]) { G[5] = word(rt, G[5]); call_here(rt, ctx, 0x08A5BAD8u); } G[2] = G[16]; break; } G[16] += 1u; G[4] += 4u; if (G[16] == G[3]) { G[16] = 0xFFFFFFFFu; G[2] = G[16]; break; } } } G[16] = s0; }
void func_0897D8D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB8E48u; G[3] = G[16] + 0x14u; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put(rt, G[16], G[2]); put_float(rt, G[16] + 0x14u, ctx.fpr[0]); put_float(rt, G[3] + 0x20u, ctx.fpr[0]); put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 0x18u, 0u); put(rt, G[3] + 4u, 0u); put_float(rt, G[3] + 0x10u, ctx.fpr[0]); put(rt, G[3] + 0x1Cu, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 0x14u, 0u); } G[16] = s0; }
void func_08993F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0x08CC89D0u; G[5] = 0x08CC8C28u; G[16] = G[4]; G[2] = word(rt, G[4] + 0x144u); put(rt, G[16], G[3]); G[4] = G[2]; put(rt, G[16] + 0x38u, G[5]); if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16]; call_here(rt, ctx, 0x08A0DD40u); G[4] = G[16]; G[5] = 0x150u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_0899A300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089FB918u); G[2] = 0x08CCBBA8u; put(rt, G[16], G[2]); G[2] = 0x08CCBE00u; put(rt, G[16] + 0x38u, G[2]); G[4] = G[16]; G[5] = 0u; put(rt, G[16] + 0x120u, 0u); put(rt, G[16] + 0x124u, 0u); put(rt, G[16] + 0x128u, 0u); put(rt, G[16] + 0x12Cu, 0u); put(rt, G[16] + 0x130u, 0u); put(rt, G[16] + 0x134u, 0u); put(rt, G[16] + 0x138u, 0u); } G[16] = s0; jump(ctx, 0x089689C8u); }
void func_0899BC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08973B94u); G[2] = 0x08CCBA90u; G[3] = G[16] + 0x2Cu; put(rt, G[16], G[2]); G[4] = G[16] + 0x38u; put(rt, G[16] + 0x28u, 0u); put(rt, G[16] + 0x2Cu, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 4u, 0u); call_here(rt, ctx, 0x08A76AA0u); G[5] = 0x08CF6ED8u; G[4] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08A61BC0u); put(rt, G[16] + 0x40u, 0u); } G[16] = s0; }
void func_089CE03C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x089CDFC4u); G[3] = G[2]; G[4] = 0u; if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = word(rt, G[16] + 0x14Cu) & 4u; G[4] = 1u; if (G[2] == 0u) { G[4] = 0u; if (static_cast<std::int32_t>(G[17]) >= 0) { G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] != 0u) G[4] = 1u; } } } G[2] = G[4]; } G[16] = s0; G[17] = s1; }
void func_089CE564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6] & 0xFFu; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CE508u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089CE6F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6] & 0xFFu; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CE694u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089CEB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6] & 0xFFu; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CEAE8u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089CECD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6] & 0xFFu; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CEC74u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089CEE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6] & 0xFFu; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CEE00u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089CE9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = high_half(0x08CF5134u); G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x089CE964u); G[4] = 0x08CF5134u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_089FCE28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x6Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x089FCD60u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x089BAC44u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A2DCE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x1Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A2DC28u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A0085C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    follow_flagged(rt, ctx, 0xF4u, 0x08968E68u);
}
void func_08A0095C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    follow_flagged(rt, ctx, 0xF8u, 0x08965BCCu);
}
void func_08A00B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    follow_flagged(rt, ctx, 0xF4u, 0x08968A38u);
}
void func_08A0AF00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x30u); G[5] = frame.at(4u); G[6] = frame.at(8u); G[16] = G[4]; G[4] = frame.at(0u); put(rt, frame.at(0x20u), bits_of(f20)); ctx.fpr[20] = float_at(rt, G[16] + 0xCu); call_here(rt, ctx, 0x08B2A0ECu); ctx.fpr[1] = float_at(rt, frame.at(4u)); ctx.fpr[2] = float_at(rt, frame.at(8u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x10u, G[2]); put_float(rt, G[16], ctx.fpr[0]); put_float(rt, G[16] + 4u, ctx.fpr[1]); put_float(rt, G[16] + 8u, ctx.fpr[2]); put_float(rt, G[16] + 0xCu, ctx.fpr[20]); } G[16] = s0; ctx.fpr[20] = f20; }
void func_08A0CFC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A02B98u); G[2] = 0x08CBA830u; G[3] = word(rt, G[16] + 0x110u); put(rt, G[16], G[2]); G[2] = 0x08CBAA88u; G[3] |= 1u; put(rt, G[16] + 0x38u, G[2]); G[4] = G[16]; G[5] = 1u; put(rt, G[16] + 0x110u, G[3]); store16(G[16] + 0x118u, 0u); store16(G[16] + 0x11Au, 0u); store16(G[16] + 0x11Cu, 0u); store16(G[16] + 0x11Eu, 0u); } G[16] = s0; jump(ctx, 0x0896A824u); }
void func_08A330E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1900u; G[4] = G[16] + 0x14u; G[3] = G[16] + 0x18u; put(rt, G[16], G[2]); store8(G[4] + 3u, 0u); store8(G[3] + 3u, 0u); store8(G[3] + 1u, 0u); store8(G[3] + 2u, 0u); store8(G[16] + 0x18u, 0u); store16(G[16] + 0x14u, 0u); put(rt, G[16] + 0x1Cu, 0u); store16(G[4] + 6u, 0u); store16(G[4] + 2u, 0u); store16(G[4] + 4u, 0u); } G[16] = s0; }
void func_08A332D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] += 0x14u; call_here(rt, ctx, 0x08B1B40Cu); } G[3] = 0x92492493u; G[2] += 2u; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[5] = sra(G[2], 31u); G[3] = ctx.hi + G[2]; G[3] = sra(G[3], 2u) - G[5]; G[4] = (G[3] << 3) - G[3]; G[2] -= G[4]; G[3] = high_half(0x08CDED2Cu); G[2] <<= 2; G[3] = 0x08CDED2Cu; G[2] += G[3]; G[2] = word(rt, G[2]); }
void func_08A444FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[6]; G[18] = G[5]; G[17] = G[7] & 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBE340u; store8(G[16] + 0x1Cu, G[17]); put(rt, G[16], G[2]); put(rt, G[16] + 0x14u, G[18]); put(rt, G[16] + 0x18u, G[19]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A4CD1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x08CF68F0u; call_here(rt, ctx, 0x08B2EEB8u); G[4] = 0x2Cu; call_here(rt, ctx, 0x08B62344u); G[17] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08A50A7Cu); G[4] = G[17]; put(rt, G[16] + 0x78u, G[17]); call_here(rt, ctx, 0x08A50400u); G[4] = 0x1000u; call_here(rt, ctx, 0x08B6461Cu); put(rt, G[16] + 0x98u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08A50FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08CF6D34u); G[16] = G[4]; G[4] = 0x08CF6D34u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[16]); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08C7AC68u); G[2] = word(rt, G[16] + 4u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08C7AC68u); G[4] = 0x08CF6D34u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_08A56824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; G[16] = word(rt, G[4] + 0x14u); G[4] = 0x08CAF798u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[25] = word(rt, G[2] + 0x12Cu); G[6] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, G[25]); }
void func_08A5EB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[8] = G[4]; G[2] = word(rt, G[4] + 0x18u); G[3] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); G[6] = G[2] + 1u; G[5] = G[6] < G[2] ? 1u : 0u; G[7] = G[5] + G[3]; put(rt, G[8] + 0x18u, G[6]); put(rt, G[8] + 0x1Cu, G[7]); call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A83D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[8] = G[4]; G[2] = word(rt, G[4] + 0x18u); G[3] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); G[6] = G[2] + 1u; G[5] = G[6] < G[2] ? 1u : 0u; G[7] = G[5] + G[3]; put(rt, G[8] + 0x18u, G[6]); put(rt, G[8] + 0x1Cu, G[7]); call_here(rt, ctx, 0x08A83160u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A5EC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[8] = G[4]; G[6] = word(rt, G[4] + 0x18u); G[7] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); G[2] = G[6] + 1u; G[5] = G[2] < G[6] ? 1u : 0u; G[3] = G[5] + G[7]; put(rt, G[8] + 0x18u, G[2]); put(rt, G[8] + 0x1Cu, G[3]); call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A690E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x1Cu; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A68D48u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A7026Cu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A6B5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x24u; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A6B590u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A71E10u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A7107C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; call_here(rt, ctx, 0x08A71064u); G[4] = G[16]; G[5] = frame.at(0u); G[6] = 1u; G[7] = 0xFFFFFFFFu; if (G[2] == 0u) { G[3] = word(rt, G[16]); store8(frame.at(0u), 0u); G[2] = word(rt, G[3] + 0x108u); call_here(rt, ctx, G[2]); G[3] = load8(frame.at(0u)); G[2] ^= 1u; G[7] = 0xFFFFFFFFu; if (G[2] == 0u) G[7] = G[3]; } G[2] = G[7]; } G[16] = s0; }
void func_08A83DD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[8] = G[4]; G[6] = word(rt, G[4] + 0x18u); G[7] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); G[2] = G[6] + 1u; G[5] = G[2] < G[6] ? 1u : 0u; G[3] = G[5] + G[7]; put(rt, G[8] + 0x18u, G[2]); put(rt, G[8] + 0x1Cu, G[3]); call_here(rt, ctx, 0x08A83160u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A89DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x18u; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A89D64u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A8C628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[3] = word(rt, G[5]); G[5] = word(rt, G[4] + 0x78u); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[16] + 0x30u); G[4] = 0x08CB1CC0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A907CCu); }
void func_08AA0CAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); if (G[5] == 0u) { G[4] = 0u; call_here(rt, ctx, 0x08B77D5Cu); } G[2] = 1u; if (G[5] == G[2]) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CB28D0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[2]; call_here(rt, ctx, 0x08B77D5Cu); } } G[16] = s0; }
void func_08AA6178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[5]); G[5] = word(rt, G[2] - 0xCu); call_here(rt, ctx, 0x08AA5E90u); G[5] = word(rt, G[16]); G[4] = G[17]; G[2] = word(rt, G[5] - 0xCu); G[6] = G[2]; tail = G[2] != 0u; } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08AA5D2Cu); }
void func_08AB022C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x18u; G[17] = G[5] & 0xFFFFu; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AAFBF8u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB19B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x18u; G[17] = G[5] & 0xFFu; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AB137Cu); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB2F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x28u; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AB2B1Cu); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A70EC8u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB5A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x20u; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AB54CCu); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A7026Cu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB634C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x28u; G[17] = G[5]; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AB5EC8u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A70EC8u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB836C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 4u); if (G[5] == 0u) return; G[3] = sext(load8(G[5]), 8u); G[2] = 0x20u; if (G[3] == 0u) return; for (;;) { bool space = G[3] == G[2]; G[5] += 1u; if (!space) { G[2] = 9u; space = G[3] == G[2]; G[2] = 0xAu; if (!space) { space = G[3] == G[2]; G[2] = 0xDu; if (!space) { if (G[3] != G[2]) return; } } } put(rt, G[4] + 4u, G[5]); G[3] = sext(load8(G[5]), 8u); G[2] = 0x20u; if (G[3] == 0u) return; } }
void func_08ABA0BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD5FB8u; G[6] = high_half(0x08D77180u); G[17] = G[5]; G[16] = G[4]; G[5] = G[16]; put(rt, G[4], G[2]); G[2] = G[4] + 4u; G[3] = word(rt, 0x08D77180u); put(rt, G[2] + 4u, G[4]); G[4] += 0xCu; put(rt, G[16] + 4u, G[3]); put(rt, 0x08D77180u, G[2]); call_here(rt, ctx, 0x08C3C138u); put(rt, G[16] + 0x18u, G[17]); } G[16] = s0; G[17] = s1; }
void func_08AC2EEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 4u); G[5] = G[2]; if (G[2] != 0u) { G[6] = word(rt, G[4]); G[2] = word(rt, G[6]); G[3] = word(rt, G[2] + 0x34u); G[4] = G[6]; call_here(rt, ctx, G[3]); G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x38u); G[5] = word(rt, G[16] + 4u); call_here(rt, ctx, G[2]); put(rt, G[16] + 4u, 0u); } store8(G[16] + 8u, 0u); } G[16] = s0; }
void func_08AC7428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17] + 0x2Cu); G[5] = G[16]; G[16] = 1u; call_here(rt, ctx, 0x08C7AB58u); G[4] = G[17]; if (static_cast<std::int32_t>(G[2]) < 0) { G[2] = 1u; store8(G[17] + 0x28u, G[2]); G[16] = 0u; } call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08AD6C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD6EE8u; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[4] + 0x30u, ctx.fpr[0]); put(rt, G[4], G[2]); put(rt, G[4] + 0x4u, 0u); put(rt, G[4] + 0x8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); store8(G[4] + 0x1Cu, 0u); store8(G[4] + 0x1Du, 0u); store8(G[4] + 0x1Eu, 0u); store8(G[4] + 0x1Fu, 0u); store8(G[4] + 0x20u, 0u); store8(G[4] + 0x21u, 0u); store8(G[4] + 0x22u, 0u); store8(G[4] + 0x23u, 0u); store16(G[4] + 0x24u, 0u); put(rt, G[4] + 0x28u, 0u); put(rt, G[4] + 0x2Cu, 0u); }
void func_08AD8754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = high_half(0x08C9089Cu); G[17] = high_half(0x08C908A0u); G[16] = G[5]; G[5] = G[16]; for (;;) { G[6] = 0x08C9089Cu; G[7] = 0x08C908A0u; call_here(rt, ctx, 0x08AD85F0u); G[3] = sext(load8(G[2]), 8u); G[4] = G[2]; if (G[3] == 0u) break; G[3] = sext(load8(G[16]), 8u); G[5] = G[16]; if (G[3] != 0u) break; } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AD8D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5] - 2u; G[2] = G[2] < 0xBu ? 1u : 0u; G[17] = G[4]; G[16] = G[5]; if (G[2] != 0u) { call_here(rt, ctx, 0x08AEB808u); G[2] = sext(load16(G[17]), 16u); G[3] = 1u << (G[16] & 31u); G[2] |= G[3]; store16(G[17], G[2]); call_here(rt, ctx, 0x08AEB82Cu); } } G[16] = s0; G[17] = s1; }
void func_08ADAEEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0x08C90CF8u; G[16] = G[4]; call_here(rt, ctx, 0x08AD9E74u); G[3] = 0u; if (G[2] != 0u) { G[2] = sext(load8(G[2] + 0x14u), 8u); G[4] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x08ADAB0Cu); G[3] = static_cast<std::int32_t>(G[2]) > 0 ? 0u : 1u; } } G[2] = G[3]; } G[16] = s0; }
void func_08ADB33C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = word(rt, G[4] + 8u); G[7] = G[4]; G[4] = 0x10u; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[8]) ? 1u : 0u; G[3] = G[7] + G[4]; while (G[2] != 0u) { G[2] = word(rt, G[3]); G[4] += 8u; if (G[2] == G[5]) { if (G[6] != 0u) { G[2] = word(rt, G[3] + 4u); put(rt, G[6], G[2]); } G[2] = G[7] + G[4]; return; } G[2] = word(rt, G[3] + 4u); G[4] += G[2]; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[8]) ? 1u : 0u; G[3] = G[7] + G[4]; } if (G[6] != 0u) put(rt, G[6], 0u); G[2] = 0u; }
void func_08ADEB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D40674u); G[5] = 0x08D40674u; G[3] = word(rt, G[5] + 0x2B4u); G[6] = G[4]; if (G[3] != G[4]) { G[2] = high_half(0x08CDFB3Cu); G[3] = word(rt, 0x08CDFB3Cu); G[2] = high_half(0x08C91034u); put(rt, G[5] + 0x2B4u, G[4]); G[3] &= 4u; G[5] = 0x08C91034u; if (G[3] != 0u) { G[4] = 0x08C91038u; if (G[6] == 0u) { G[2] = high_half(0x08C91048u); G[5] = 0x08C91048u; } call_here(rt, ctx, 0x08AD8464u); } } } }
void func_08AE6E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 8u); if (static_cast<std::int32_t>(G[2]) < 0) G[3] = 0u; G[4] = G[3]; while (G[3] != 0u) { G[2] = sext(load16(G[4] + 0x14u), 16u); if (G[2] == G[5]) { call_here(rt, ctx, 0x08AE6DCCu); break; } G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; } } }
void func_08AEFA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = frame.at(4u); call_here(rt, ctx, 0x08AEF8E4u); ctx.fpr[0] = 0.0f; G[3] = word(rt, frame.at(0u)); if (G[2] != 0u) { G[2] = word(rt, frame.at(4u)); G[3] <<= 5; G[3] += G[2]; G[2] = 0x08D41AD0u; G[3] <<= 1; G[3] += G[2]; G[3] = sext(load16(G[3] + 0x3A4u), 16u); G[2] = high_half(0x08CB6160u); ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = float_at(rt, 0x08CB6160u); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); } } }
void func_08AF10C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[8] = G[5]; G[7] = G[4]; G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[4] + 8u); G[6] = frame.at(0u); G[5] = G[3]; put_float(rt, frame.at(4u), ctx.fpr[12]); put(rt, frame.at(0u), G[8]); if (G[3] == G[2]) call_here(rt, ctx, 0x08C3EBD8u); else { G[2] = word(rt, frame.at(4u)); if (G[3] != 0u) { put(rt, G[3], G[8]); put(rt, G[3] + 4u, G[2]); G[3] = word(rt, G[4] + 4u); } G[3] += 8u; put(rt, G[7] + 4u, G[3]); } } }
void func_08AF8A50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x2Cu); G[4] = word(rt, G[2] + 0x30u); call_here(rt, ctx, 0x08B537BCu); G[3] = word(rt, G[16]); G[2] = G[3] & 0x20u; G[2] = G[3]; if ((G[3] & 0x20u) != 0u) { G[4] = word(rt, G[16] + 0x10u); call_here(rt, ctx, 0x08B5281Cu); G[4] = word(rt, G[16] + 0x1Cu); call_here(rt, ctx, 0x08B5281Cu); G[3] = word(rt, G[16]); G[2] = G[3]; } G[3] &= 0xFDFu; G[2] = (G[2] & ~0xFFFu) | (G[3] & 0xFFFu); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08AFBF8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[7] = float_at(rt, G[5]); ctx.fpr[6] = float_at(rt, G[4]); ctx.fpr[4] = float_at(rt, G[4] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[2] = float_at(rt, G[4] + 8u); ctx.fpr[1] = float_at(rt, G[5] + 4u); ctx.fpr[5] = psp_mul(ctx.fpr[0], ctx.fpr[6]); ctx.fpr[3] = psp_mul(ctx.fpr[2], ctx.fpr[7]); ctx.fpr[0] = psp_mul(ctx.fpr[4], ctx.fpr[0]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[1]); ctx.fpr[4] = psp_mul(ctx.fpr[4], ctx.fpr[7]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[6]); ctx.fpr[3] = ctx.fpr[3] - ctx.fpr[5]; ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[2]; ctx.fpr[5] = float_at(rt, G[6] + 4u); ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[4]; ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[5]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[2] = float_at(rt, G[6] + 8u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; }
void func_08B014BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load16(G[4] + 0x6Cu), 16u); G[6] = G[4] + 0x6Cu; store16(G[5] + 0x52u, G[2]); G[2] = 0xFFFFFFFFu; if (G[5] != 0u) { G[2] = word(rt, G[4] + 0x74u); G[3] = word(rt, G[2]); G[2] = 0x3CF3CF3Du; G[3] = G[5] - G[3]; G[3] = sra(G[3], 2u); const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[3] = ctx.lo; G[2] = sext(G[3] & 0xFFFFu, 16u); } G[3] = sext(load16(G[6]), 16u); G[4] = sext(G[2] & 0xFFFFu, 16u); G[2] = 0xFFFFFFFFu; if (G[3] == G[2]) store16(G[6] + 2u, G[4]); G[2] = word(rt, G[6] + 4u); store16(G[6], G[4]); G[2] += 1u; put(rt, G[6] + 4u, G[2]); }
void func_08B0A034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x40u); G[16] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; call_here(rt, ctx, 0x08B09F98u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B09438u); G[4] = G[17]; G[5] = frame.at(0u); G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B096DCu); G[3] = G[2]; } G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_08B0A5F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08D473E8u); G[16] = G[4]; G[4] = 0x08D473E8u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[16]); G[16] = word(rt, G[2] + 0x110u); G[4] = 0x08D473E8u; if (G[16] != 0u) { G[2] = load8(G[16] + 0xACu); if (G[2] == 0u) G[16] = 0u; } call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08B1205C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; const std::uint32_t n = G[4]; auto lt = [](std::uint32_t a, std::int32_t b) { return static_cast<std::int32_t>(a) < b ? 1u : 0u; }; G[2] = lt(n, 0x20); G[5] = lt(n, 0x40); G[3] = 0x20u; if (G[2] != 0u) { G[2] = G[3]; return; } G[2] = lt(n, 0x80); G[3] = 0x40u; if (G[5] != 0u) { G[2] = G[3]; return; } G[5] = lt(n, 0x100); G[3] = 0x80u; if (G[2] != 0u) { G[2] = G[3]; return; } G[2] = lt(n, 0x200); G[3] = 0x100u; if (G[5] != 0u) { G[2] = G[3]; return; } G[5] = lt(n, 0x400); G[3] = 0x200u; if (G[2] != 0u) { G[2] = G[3]; return; } G[4] = lt(n, 0x800); G[3] = 0x400u; if (G[5] != 0u) { G[2] = G[3]; return; } G[3] = 0x800u; G[2] = 0x1000u; if (G[4] == 0u) G[3] = G[2]; G[2] = G[3]; }
void func_08B165AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[6]; G[18] = G[5]; G[17] = G[7]; G[16] = G[4]; call_here(rt, ctx, 0x08B11830u); G[2] = 0x08CD90B8u; put(rt, G[16] + 0x10u, G[17]); put(rt, G[16], G[2]); put(rt, G[16] + 0xA0u, G[18]); put(rt, G[16] + 0xA4u, G[19]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B17178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD91A0u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x19B0u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x1984u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x1958u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x192Cu; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x1900u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x18D4u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B17FD8u); }
void func_08B18D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4]); put(rt, G[4] + 4u, 0u); G[7] = 0u; for (;;) { G[6] = G[5] + 0xCu; G[2] = word(rt, G[6] + 8u); G[2] = G[7] + G[2]; put(rt, G[4] + 4u, G[2]); G[3] = word(rt, G[5] + 0x30u); G[7] = G[2]; if (G[3] == 0u) break; G[5] = G[3]; } if (G[2] == 0u) { put(rt, G[4] + 8u, 0u); return; } put(rt, G[4] + 8u, G[5]); G[2] = word(rt, G[6]); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0xCu, G[2]); G[3] = word(rt, G[6] + 4u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, G[3]); }
void func_08B285C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0x8000u; G[2] = G[3] < G[5] ? 1u : 0u; G[16] = G[5]; if (G[2] != 0u) G[16] = G[3]; call_here(rt, ctx, 0x08B28068u); G[2] <<= 2; G[3] = 0xAAAAAAABu; G[2] += 2u; const std::uint64_t wide = static_cast<std::uint64_t>(G[2]) * G[3]; ctx.lo = static_cast<std::uint32_t>(wide); ctx.hi = static_cast<std::uint32_t>(wide >> 32); G[4] = G[16] << 1; G[2] = ctx.hi >> 1; G[2] += 1u; G[2] += G[4]; G[2] <<= 1; G[2] = G[16] + G[2]; } G[16] = s0; }
void func_08B2C720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[5] = word(rt, G[5] + 4u); G[5] -= G[16]; call_here(rt, ctx, 0x08B2C4A4u); G[4] = word(rt, G[16] + 8u); G[2] <<= 2; G[2] += G[17]; G[3] = word(rt, G[16] + 0xCu); if (G[4] != 0u) put(rt, G[4] + 0xCu, G[3]); else put(rt, G[2] + 0x10u, G[3]); if (G[3] != 0u) put(rt, G[3] + 8u, G[4]); } G[16] = s0; G[17] = s1; }
void func_08B2DD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CE0C78u); G[2] = word(rt, 0x08CE0C78u); G[17] = high_half(0x08D68D64u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08C7AF68u); G[3] = 0xFFFFFFFFu; put(rt, 0x08CE0C78u, G[3]); } G[2] = word(rt, 0x08D68D64u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x08C7A9C0u); put(rt, 0x08D68D64u, 0u); } } G[16] = s0; G[17] = s1; }
void func_08B321C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D6916Cu); G[4] = 0x40000u; G[2] = load8(0x08D6916Cu); G[5] = 0x11u; G[6] = 0u; G[7] = 0x11u; G[8] = 0u; G[3] = 1u; if (G[2] == 0u) { call_here(rt, ctx, 0x08C7ADF8u); G[3] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = 1u; store8(0x08D6916Cu, G[2]); G[3] = 1u; } } G[2] = G[3]; } G[16] = s0; }
void func_08B324CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D69168u); G[3] = word(rt, 0x08D69168u); if (G[3] != 0u) { G[2] = 1u; if (G[3] == G[2]) call_here(rt, ctx, 0x08B32350u); else { G[2] = 2u; if (G[3] == G[2]) call_here(rt, ctx, 0x08B32460u); } } put(rt, 0x08D69168u, 0u); } G[16] = s0; }
void func_08B343D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B3433Cu); G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = word(rt, frame.at(8u)); G[4] = word(rt, frame.at(0xCu)); G[6] = word(rt, frame.at(4u)); std::uint64_t wide = static_cast<std::uint64_t>(G[2]) * G[4]; ctx.lo = static_cast<std::uint32_t>(wide); ctx.hi = static_cast<std::uint32_t>(wide >> 32); G[2] = ctx.lo; G[3] = ctx.hi; wide = static_cast<std::uint64_t>(G[2]) * G[6]; ctx.lo = static_cast<std::uint32_t>(wide); ctx.hi = static_cast<std::uint32_t>(wide >> 32); G[5] = ctx.hi; G[4] = ctx.lo; ctx.lo = G[5]; const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32 | ctx.lo) + static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(G[6])) * static_cast<std::int32_t>(G[3])); ctx.lo = static_cast<std::uint32_t>(accumulator); ctx.hi = static_cast<std::uint32_t>(accumulator >> 32); G[5] = ctx.lo; } G[2] = G[4]; G[3] = G[5]; } }
void func_08B39484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDA928u; put(rt, G[4], G[2]); G[6] = 0x08CDA908u; put(rt, G[4] + 4u, 0u); G[2] = 0x08CDA8F8u; store16(G[4] + 8u, 0u); G[3] = word(rt, G[5] + 4u); put(rt, G[4] + 0xCu, G[2]); put(rt, G[4] + 0x10u, G[3]); if (G[3] != 0u) { G[2] = word(rt, G[3] + 4u) + 1u; put(rt, G[3] + 4u, G[2]); } G[2] = 0x08CDA8E8u; put(rt, G[4] + 0x1Cu, G[6]); put(rt, G[4] + 0xCu, G[2]); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x14u, G[6]); put(rt, G[4] + 0x20u, 0u); }
void func_08B3F980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B3F910u); G[4] = G[16]; G[5] = G[17]; G[6] = 4u; if (G[2] != 0u) call_here(rt, ctx, 0x08B3F964u); else { G[4] = G[16] + 4u; G[5] = G[17] + 4u; call_here(rt, ctx, 0x08B36170u); G[2] = G[2] == 0u ? 1u : 0u; } } G[16] = s0; G[17] = s1; }
void func_08B52148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = sext(G[5] & 0xFFu, 8u); G[16] = G[4]; G[4] = G[16]; for (;;) { G[5] = G[17]; call_here(rt, ctx, 0x08B520F8u); G[4] = G[16]; if (G[2] != 0u) { G[2] = 1u; break; } call_here(rt, ctx, 0x08B51F74u); if (G[2] != 0u) { G[2] = 0u; break; } G[4] = G[16]; } } G[16] = s0; G[17] = s1; }
void func_08B521A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; for (;;) { G[4] = G[17]; call_here(rt, ctx, 0x08B51F38u); G[3] = G[2]; if (G[2] == 0u) { G[2] = 0u; break; } G[2] = load8(G[2] + 0xCu); G[4] = 0u; if (G[2] != 0u) G[4] = sext(load8(G[3] + 4u), 8u); store8(G[16], G[4]); if (G[4] != 0u) { G[2] = 1u; break; } } } G[16] = s0; G[17] = s1; }
void func_08B527B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[18] = G[6]; G[17] = G[5]; call_here(rt, ctx, 0x08B59AC8u); G[16] = G[2]; if (G[2] != 0u) { G[5] = G[18]; G[6] = G[17]; G[4] = G[2]; if (G[18] != 0u) call_here(rt, ctx, 0x08B79A88u); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B54884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x14u); if (G[3] == 0u) { G[3] = 0u; G[2] = 0u; return; } G[2] = word(rt, G[3] + 8u); G[4] = G[3]; G[6] = 0u; if (G[5] != G[2]) { for (;;) { G[2] = G[5] < G[2] ? 1u : 0u; if (G[2] == 0u) G[3] = word(rt, G[3] + 0x10u); else { G[3] = word(rt, G[3] + 0x14u); G[6] = G[4]; } G[4] = G[3]; if (G[3] == 0u) { G[3] = G[6]; break; } G[2] = word(rt, G[3] + 8u); if (G[5] == G[2]) break; } } G[2] = G[3]; }
void func_08B5F1F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0xCu); G[2] = word(rt, G[6] + 8u); put(rt, G[4] + 0xCu, G[2]); if (G[2] != 0u) put(rt, G[2] + 4u, G[4]); G[2] = word(rt, G[4] + 4u); put(rt, G[6] + 4u, G[2]); G[3] = word(rt, G[5]); if (G[4] == G[3]) put(rt, G[5], G[6]); else { G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 8u); if (G[4] == G[2]) put(rt, G[3] + 8u, G[6]); else put(rt, G[3] + 0xCu, G[6]); } put(rt, G[4] + 4u, G[6]); put(rt, G[6] + 8u, G[4]); }
void func_08B5F258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 8u); G[2] = word(rt, G[6] + 0xCu); put(rt, G[4] + 8u, G[2]); if (G[2] != 0u) put(rt, G[2] + 4u, G[4]); G[2] = word(rt, G[4] + 4u); put(rt, G[6] + 4u, G[2]); G[3] = word(rt, G[5]); if (G[4] == G[3]) put(rt, G[5], G[6]); else { G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 0xCu); if (G[4] == G[2]) put(rt, G[3] + 0xCu, G[6]); else put(rt, G[3] + 8u, G[6]); } put(rt, G[4] + 4u, G[6]); put(rt, G[6] + 0xCu, G[4]); }
void func_08B64FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CDC170u);
        call(rt, ctx, 0x08B61BC8u, {self + 8u});
        const u32 flag = load8(self + 0xCu);
        ctx.gpr[2] = flag;
        if (flag != 0u) {
            const u32 member = word(rt, self + 0x18u);
            if (member != 0u) call(rt, ctx, 0x08B61FC0u, {member});
        }
    }
    tail_call(ctx, 0x08B5C280u, {self});
}
void func_08B65038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CDC170u);
        call(rt, ctx, 0x08B61BC8u, {self + 8u});
        const u32 flag = load8(self + 0xCu);
        ctx.gpr[2] = flag;
        if (flag != 0u) {
            const u32 member = word(rt, self + 0x18u);
            if (member != 0u) call(rt, ctx, 0x08B61FC0u, {member});
        }
    }
    tail_call(ctx, 0x08B5C280u, {self});
}
void func_08B653DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0x18u); G[18] = G[4]; while (G[17] != 0u) { G[4] = G[17] + 0xCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; if (G[2] != 0u) break; G[16] = word(rt, G[17]); call_here(rt, ctx, 0x08B64654u); G[17] = G[16]; } put(rt, G[18] + 0x18u, 0u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B768A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0xFFFFFFFFu; G[2] = 0x7FAu; const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; put(rt, G[5] + 0xCu, G[4]); put(rt, G[5], G[3]); put(rt, G[5] + 4u, G[6]); put(rt, G[5] + 8u, G[7]); put(rt, G[5] + 0x10u, G[2]); call_here(rt, ctx, 0x08C7A888u); G[5] = high_half(0x08D72CF8u); G[3] = word(rt, 0x08D72CF8u); G[4] = G[2]; put(rt, G[16] + 0x14u, G[3]); G[3] = high_half(0x08D72CECu); put(rt, 0x08D72CF8u, G[16]); put(rt, 0x08D72CECu, G[2]); } G[16] = s0; jump(ctx, 0x08C7A8A8u); }
void func_08BA5FA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[2] = load16(G[4] + 0xEu) & 1u; G[16] = G[5]; if (G[2] == 0u) { call_here(rt, ctx, 0x08AF802Cu); G[5] = G[2]; G[4] = G[16]; G[25] = G[17]; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, G[25]); }
void func_08BDD878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    destroy_range(rt, ctx, 0x08BDD7D8u, 4u, 8u, 0x10u, frame.at(0x0u));
}
void func_08BDDC68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[5]; G[16] = G[4]; while (G[16] != G[17]) { G[4] = word(rt, G[16] + 0x4u); G[5] = word(rt, G[16] + 0x8u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BDD7D8u); G[2] = word(rt, G[16] + 0x4u); G[16] += 0x10u; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08B35254u); } } G[16] = s0; G[17] = s1; }
void func_08C0DC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    destroy_range(rt, ctx, 0x08BE5888u, 0u, 4u, 0xCu, frame.at(0x0u));
}
void func_08C0DCF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    destroy_range(rt, ctx, 0x08BE5888u, 0u, 4u, 0xCu, frame.at(0x0u));
}
void func_08BF8E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = word(rt, G[5]); G[9] = word(rt, G[5] + 4u); G[5] = G[4] - 8u; for (;;) { G[7] = word(rt, G[5]); G[2] = word(rt, G[8] + 0x134u); G[3] = word(rt, G[7] + 0x134u); G[6] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; bool shift = G[2] != 0u; if (!shift) { if (G[6] != 0u) break; G[3] = word(rt, G[5] + 4u); G[2] = static_cast<std::int32_t>(G[9]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] == 0u) break; put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[7]); } else { G[3] = word(rt, G[5] + 4u); put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[7]); } G[4] = G[5]; G[5] -= 8u; } put(rt, G[4], G[8]); put(rt, G[4] + 4u, G[9]); }
void func_08C02658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_range(rt, ctx, 0x08A1A718u, 4u);
}
void func_08C06694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_range(rt, ctx, 0x0896418Cu, 0x10u);
}
void func_08C124BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_range(rt, ctx, 0x089FA280u, 0x10u);
}
void func_08C0E814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[6]; if (G[4] != G[5]) { G[2] = G[4]; G[16] = G[4]; do { G[4] = G[2]; G[5] = G[17]; if (G[17] != G[2]) call_here(rt, ctx, 0x08C0E654u); G[2] = G[16] + 0xCu; G[16] = G[2]; } while (G[2] != G[18]); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C1EC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[17] = G[5]; if (G[5] != 0u) { do { G[5] = word(rt, G[17] + 0xCu); G[4] = G[18]; call_here(rt, ctx, 0x08C1EC60u); G[16] = word(rt, G[17] + 8u); G[4] = G[17]; G[5] = 0x18u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[17] = G[16]; } while (G[16] != 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C1F510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4]); G[18] = G[4]; if (G[17] != G[4]) { G[16] = word(rt, G[17]); for (;;) { G[4] = G[17] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0xCu; G[6] = 4u; G[17] = G[16]; call_here(rt, ctx, 0x08A5BC54u); if (G[18] == G[16]) break; G[16] = word(rt, G[17]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C2AF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4]); G[18] = G[4]; if (G[17] != G[4]) { G[16] = word(rt, G[17]); for (;;) { G[4] = G[17] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0xCu; G[6] = 4u; G[17] = G[16]; call_here(rt, ctx, 0x08A5BC54u); if (G[18] == G[16]) break; G[16] = word(rt, G[17]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C2F9F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4]); G[18] = G[4]; if (G[17] != G[4]) { G[16] = word(rt, G[17]); for (;;) { G[4] = G[17] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0xCu; G[6] = 4u; G[17] = G[16]; call_here(rt, ctx, 0x08A5BC54u); if (G[18] == G[16]) break; G[16] = word(rt, G[17]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C35220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4]); G[18] = G[4]; if (G[17] != G[4]) { G[16] = word(rt, G[17]); for (;;) { G[4] = G[17] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0xCu; G[6] = 4u; G[17] = G[16]; call_here(rt, ctx, 0x08A5BC54u); if (G[18] == G[16]) break; G[16] = word(rt, G[17]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C25238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_range(rt, ctx, 0x08A6972Cu, 4u);
}
void func_08C30DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; G[16] = G[6]; if (G[4] != G[5]) { do { G[5] = G[17]; G[4] = G[16]; G[17] += 0x4u; if (G[16] != 0u) call_here(rt, ctx, 0x08B88F44u); G[16] += 0x4u; } while (G[18] != G[17]); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C2BB2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A6972Cu); G[2] = word(rt, G[17]); G[3] = word(rt, G[17] + 4u); G[18] = G[16] + 4u; put(rt, G[16] + 4u, G[2]); G[4] = G[16] + 0xCu; G[5] = G[17] + 8u; put(rt, G[18] + 4u, G[3]); call_here(rt, ctx, 0x08B88F44u); G[2] = word(rt, G[17] + 0xCu); put(rt, G[18] + 0xCu, G[2]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C3E7E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4] - 8u; ctx.fpr[1] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[6] + 4u); const bool less = ctx.fpr[1] < ctx.fpr[0]; G[5] = word(rt, G[5]); if (less) { G[7] = G[4] - 0xCu; for (;;) { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[8] = G[6]; put(rt, G[4], G[2]); G[6] -= 8u; put(rt, G[4] + 4u, G[3]); G[4] = G[8]; ctx.fpr[0] = float_at(rt, G[7]); const bool more = ctx.fpr[1] < ctx.fpr[0]; G[7] -= 8u; if (!more) break; } G[4] = G[8]; } put_float(rt, G[4] + 4u, ctx.fpr[1]); put(rt, G[4], G[5]); }
void func_08C3EB78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[6]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = word(rt, G[16]); for (;;) { G[5] = G[17]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3]); G[16] += 8u; call_here(rt, ctx, G[2]); if (G[18] == G[16]) break; G[4] = word(rt, G[16]); } } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C3F180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7000u; G[17] = G[4] + 4u; G[16] = G[4]; put(rt, G[4], G[2]); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 4u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 4u, 0u); } G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08AF20E8u); }
void func_08C422AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] -= G[4]; { GuestFrame frame(ctx, 0x10u); G[3] = sra(G[5], 2u); G[8] = G[4]; store8(frame.at(0u), G[7]); if (static_cast<std::int32_t>(G[3]) > 0) { G[6] = word(rt, G[6]); G[7] = sra(G[3], 1u); do { G[2] = G[7] << 2; G[5] = G[2] + G[8]; G[4] = word(rt, G[5]); G[3] -= G[7]; G[2] = load16(G[4] + 4u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[3] -= 1u; if (G[2] != 0u) G[8] = G[5] + 4u; else G[3] = G[7]; G[7] = sra(G[3], 1u); } while (static_cast<std::int32_t>(G[3]) > 0); } G[2] = G[8]; } }
void func_08C4AFB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[11] = G[5]; G[8] = G[4]; G[9] = G[6]; if (G[4] != G[5]) { G[10] = G[4]; do { G[10] += 0x18u; if (G[9] != 0u) { G[7] = word(rt, G[8] + 0x14u); G[2] = word(rt, G[8]); G[3] = word(rt, G[8] + 4u); G[4] = word(rt, G[8] + 8u); G[5] = word(rt, G[8] + 0xCu); G[6] = word(rt, G[8] + 0x10u); put(rt, G[9], G[2]); put(rt, G[9] + 4u, G[3]); put(rt, G[9] + 8u, G[4]); put(rt, G[9] + 0xCu, G[5]); put(rt, G[9] + 0x10u, G[6]); put(rt, G[9] + 0x14u, G[7]); } G[8] = G[10]; G[9] += 0x18u; } while (G[10] != G[11]); } G[2] = G[9]; }
void func_08C7452C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 8u); G[16] = G[4]; if (static_cast<std::int32_t>(G[2]) < 0) { G[6] = 0u; tail = true; } else { G[2] = 0x08D77498u; const bool shared = G[4] == G[2]; G[2] = G[16] + 0xCu; if (!shared) { G[4] += 8u; G[5] = 1u; call_here(rt, ctx, 0x08B6085Cu); G[2] = G[16] + 0xCu; } } } G[16] = s0; if (tail) jump(ctx, 0x08C74458u); }
void func_08C75770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    release_shared(rt, ctx, frame.at(0x0u));
}
void func_08C757D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    release_shared(rt, ctx, frame.at(0x0u));
}
void func_08C76274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    u32 extra;
    {
        GuestFrame frame(ctx, 0x20u);
        extra = call(rt, ctx, 0x08B35F60u, {fourth});
    }
    tail_call(ctx, 0x08C75FF8u, {first, second, third, fourth, extra});
}
void func_08C78514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0xCu; call_here(rt, ctx, 0x08C7835Cu); G[5] = 0u; const bool ok = G[2] != 0u; G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; if (!ok) { G[5] = word(rt, G[4] + 0x14u); G[5] |= 4u; } } G[16] = s0; jump(ctx, 0x08C54CFCu); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_189
} // namespace gt::game

#define HOT189(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_189::name, gt::ReturnKind::kind)
HOT189(0x088040ACu, func_088040AC, Word);
// HOT189(0x0880BAD8u, func_0880BAD8, Word);  // no comparable synthetic case yet
HOT189(0x0881A664u, func_0881A664, Word);
HOT189(0x08844530u, func_08844530, Word);
HOT189(0x08845694u, func_08845694, Word);
HOT189(0x088294D8u, func_088294D8, Word);
HOT189(0x08829538u, func_08829538, Word);
// HOT189(0x08829D98u, func_08829D98, Word);  // no comparable synthetic case yet
// HOT189(0x08829DF8u, func_08829DF8, Word);  // no comparable synthetic case yet
// HOT189(0x0882A278u, func_0882A278, Word);  // no comparable synthetic case yet
HOT189(0x0885E5BCu, func_0885E5BC, Word);
HOT189(0x0887E430u, func_0887E430, Word);
HOT189(0x0887E630u, func_0887E630, Word);
HOT189(0x0887E690u, func_0887E690, Word);
HOT189(0x0887E7C0u, func_0887E7C0, Word);
HOT189(0x08884DF4u, func_08884DF4, Word);
HOT189(0x08884E54u, func_08884E54, Word);
HOT189(0x0888BA60u, func_0888BA60, Word);
HOT189(0x0889095Cu, func_0889095C, Word);
HOT189(0x08890AC0u, func_08890AC0, Word);
HOT189(0x08890B44u, func_08890B44, Word);
// HOT189(0x08893DD0u, func_08893DD0, Word);  // no comparable synthetic case yet
HOT189(0x08893E30u, func_08893E30, Word);
HOT189(0x088A5F00u, func_088A5F00, Word);
// HOT189(0x088BBCF8u, func_088BBCF8, Float);  // no comparable synthetic case yet
// HOT189(0x088BBD58u, func_088BBD58, Float);  // no comparable synthetic case yet
HOT189(0x088CF108u, func_088CF108, Word);
HOT189(0x088D0F38u, func_088D0F38, Word);
HOT189(0x088D60B4u, func_088D60B4, Word);
HOT189(0x088DA9E0u, func_088DA9E0, Word);
// HOT189(0x088DE874u, func_088DE874, Word);  // a synthetic case mismatched: to fix
// HOT189(0x088E4754u, func_088E4754, Word);  // no comparable synthetic case yet
HOT189(0x088E4AA8u, func_088E4AA8, Word);
HOT189(0x088EB3B0u, func_088EB3B0, Word);
HOT189(0x088EB754u, func_088EB754, Word);
HOT189(0x088ECC74u, func_088ECC74, Word);
HOT189(0x088EE660u, func_088EE660, Word);
HOT189(0x088EF210u, func_088EF210, Word);
// HOT189(0x088F2D38u, func_088F2D38, Word);  // no comparable synthetic case yet
HOT189(0x088F32A4u, func_088F32A4, Word);
HOT189(0x088F66F8u, func_088F66F8, Word);
HOT189(0x088FFFF8u, func_088FFFF8, Word);
// HOT189(0x08905F54u, func_08905F54, Word);  // no comparable synthetic case yet
HOT189(0x08905FD8u, func_08905FD8, Word);
HOT189(0x08916408u, func_08916408, Word);
HOT189(0x089200F4u, func_089200F4, Word);
HOT189(0x08927E4Cu, func_08927E4C, Word);
HOT189(0x0892BC60u, func_0892BC60, Word);
HOT189(0x08932BFCu, func_08932BFC, Word);
HOT189(0x08932CE0u, func_08932CE0, Word);
HOT189(0x0893C2C0u, func_0893C2C0, Word);
HOT189(0x08941978u, func_08941978, Word);
HOT189(0x08947BA4u, func_08947BA4, Float);
HOT189(0x089797C0u, func_089797C0, Word);
HOT189(0x0897D8D0u, func_0897D8D0, Word);
HOT189(0x08993F6Cu, func_08993F6C, Word);
HOT189(0x0899A300u, func_0899A300, Word);
HOT189(0x0899BC2Cu, func_0899BC2C, Word);
HOT189(0x089CE03Cu, func_089CE03C, Word);
HOT189(0x089CE564u, func_089CE564, Word);
HOT189(0x089CE6F0u, func_089CE6F0, Word);
HOT189(0x089CEB44u, func_089CEB44, Word);
HOT189(0x089CECD0u, func_089CECD0, Word);
HOT189(0x089CEE5Cu, func_089CEE5C, Word);
HOT189(0x089CE9B8u, func_089CE9B8, Word);
HOT189(0x089FCE28u, func_089FCE28, Word);
HOT189(0x08A2DCE0u, func_08A2DCE0, Word);
HOT189(0x08A0085Cu, func_08A0085C, Word);
HOT189(0x08A0095Cu, func_08A0095C, Word);
HOT189(0x08A00B0Cu, func_08A00B0C, Word);
HOT189(0x08A0AF00u, func_08A0AF00, Word);
HOT189(0x08A0CFC4u, func_08A0CFC4, Word);
HOT189(0x08A330E0u, func_08A330E0, Word);
HOT189(0x08A332D0u, func_08A332D0, Word);
HOT189(0x08A444FCu, func_08A444FC, Word);
// HOT189(0x08A4CD1Cu, func_08A4CD1C, Word);  // no comparable synthetic case yet
HOT189(0x08A50FCCu, func_08A50FCC, Word);
HOT189(0x08A56824u, func_08A56824, Word);
HOT189(0x08A5EB6Cu, func_08A5EB6C, Word);
HOT189(0x08A83D10u, func_08A83D10, Word);
HOT189(0x08A5EC30u, func_08A5EC30, Word);
HOT189(0x08A690E4u, func_08A690E4, Word);
HOT189(0x08A6B5E0u, func_08A6B5E0, Word);
HOT189(0x08A7107Cu, func_08A7107C, Word);
HOT189(0x08A83DD4u, func_08A83DD4, Word);
HOT189(0x08A89DA8u, func_08A89DA8, Word);
HOT189(0x08A8C628u, func_08A8C628, Word);
HOT189(0x08AA0CACu, func_08AA0CAC, Word);
// HOT189(0x08AA6178u, func_08AA6178, Word);  // no comparable synthetic case yet
HOT189(0x08AB022Cu, func_08AB022C, Word);
HOT189(0x08AB19B0u, func_08AB19B0, Word);
HOT189(0x08AB2F10u, func_08AB2F10, Word);
HOT189(0x08AB5A08u, func_08AB5A08, Word);
HOT189(0x08AB634Cu, func_08AB634C, Word);
HOT189(0x08AB836Cu, func_08AB836C, Word);
HOT189(0x08ABA0BCu, func_08ABA0BC, Word);
HOT189(0x08AC2EECu, func_08AC2EEC, Word);
// HOT189(0x08AC7428u, func_08AC7428, Word);  // no comparable synthetic case yet
HOT189(0x08AD6C34u, func_08AD6C34, Word);
HOT189(0x08AD8754u, func_08AD8754, Word);
HOT189(0x08AD8D54u, func_08AD8D54, Word);
HOT189(0x08ADAEECu, func_08ADAEEC, Word);
HOT189(0x08ADB33Cu, func_08ADB33C, Word);
HOT189(0x08ADEB40u, func_08ADEB40, Word);
HOT189(0x08AE6E94u, func_08AE6E94, Word);
HOT189(0x08AEFA5Cu, func_08AEFA5C, Float);
HOT189(0x08AF10C4u, func_08AF10C4, Word);
HOT189(0x08AF8A50u, func_08AF8A50, Word);
HOT189(0x08AFBF8Cu, func_08AFBF8C, Float);
HOT189(0x08B014BCu, func_08B014BC, Word);
HOT189(0x08B0A034u, func_08B0A034, Word);
HOT189(0x08B0A5F0u, func_08B0A5F0, Word);
HOT189(0x08B1205Cu, func_08B1205C, Word);
HOT189(0x08B165ACu, func_08B165AC, Word);
HOT189(0x08B17178u, func_08B17178, Word);
HOT189(0x08B18D64u, func_08B18D64, Word);
HOT189(0x08B285C8u, func_08B285C8, Word);
HOT189(0x08B2C720u, func_08B2C720, Word);
// HOT189(0x08B2DD88u, func_08B2DD88, Word);  // no comparable synthetic case yet
HOT189(0x08B321C8u, func_08B321C8, Word);
HOT189(0x08B324CCu, func_08B324CC, Word);
// HOT189(0x08B343D0u, func_08B343D0, DoubleWord);  // no comparable synthetic case yet
HOT189(0x08B39484u, func_08B39484, Word);
HOT189(0x08B3F980u, func_08B3F980, Word);
HOT189(0x08B52148u, func_08B52148, Word);
HOT189(0x08B521A8u, func_08B521A8, Word);
HOT189(0x08B527B4u, func_08B527B4, Word);
HOT189(0x08B54884u, func_08B54884, Word);
HOT189(0x08B5F1F8u, func_08B5F1F8, Word);
HOT189(0x08B5F258u, func_08B5F258, Word);
HOT189(0x08B64FC4u, func_08B64FC4, Word);
HOT189(0x08B65038u, func_08B65038, Word);
HOT189(0x08B653DCu, func_08B653DC, Word);
HOT189(0x08B768A4u, func_08B768A4, Word);
HOT189(0x08BA5FA0u, func_08BA5FA0, Word);
HOT189(0x08BDD878u, func_08BDD878, Word);
// HOT189(0x08BDDC68u, func_08BDDC68, Word);  // no comparable synthetic case yet
HOT189(0x08C0DC90u, func_08C0DC90, Word);
HOT189(0x08C0DCF0u, func_08C0DCF0, Word);
HOT189(0x08BF8E6Cu, func_08BF8E6C, Word);
HOT189(0x08C02658u, func_08C02658, Word);
HOT189(0x08C06694u, func_08C06694, Word);
HOT189(0x08C124BCu, func_08C124BC, Word);
// HOT189(0x08C0E814u, func_08C0E814, Word);  // no comparable synthetic case yet
HOT189(0x08C1EC60u, func_08C1EC60, Word);
// HOT189(0x08C1F510u, func_08C1F510, Word);  // no comparable synthetic case yet
// HOT189(0x08C2AF5Cu, func_08C2AF5C, Word);  // no comparable synthetic case yet
// HOT189(0x08C2F9F0u, func_08C2F9F0, Word);  // no comparable synthetic case yet
// HOT189(0x08C35220u, func_08C35220, Word);  // no comparable synthetic case yet
HOT189(0x08C25238u, func_08C25238, Word);
// HOT189(0x08C30DACu, func_08C30DAC, Word);  // no comparable synthetic case yet
HOT189(0x08C2BB2Cu, func_08C2BB2C, Word);
HOT189(0x08C3E7E0u, func_08C3E7E0, Word);
// HOT189(0x08C3EB78u, func_08C3EB78, Word);  // no comparable synthetic case yet
HOT189(0x08C3F180u, func_08C3F180, Word);
HOT189(0x08C422ACu, func_08C422AC, Word);
HOT189(0x08C4AFB4u, func_08C4AFB4, Word);
HOT189(0x08C7452Cu, func_08C7452C, Word);
HOT189(0x08C75770u, func_08C75770, Word);
HOT189(0x08C757D8u, func_08C757D8, Word);
HOT189(0x08C76274u, func_08C76274, Word);
HOT189(0x08C78514u, func_08C78514, Word);
