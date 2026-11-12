// Hot batch 173: nine-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, small float helpers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_173 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load16(address); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Families of identical shape: each registration keeps its own thin function (and its own frame), the constants differ.
u32 sign8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sign16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// `0x08A2DF..`: the result of `0x08B09A04(word(0x08CF6648))` becomes the argument of the tail call.
u32 global_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
}
// Store the halfword at `offset` when it differs from a1 and flag the change (bit 3 of word +0xE4).
void set_half_flagged(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 current = sign16(half_at(rt, self + offset));
    ctx.gpr[2] = current;
    if (current != value) {
        const u32 flags = word(rt, self + 0xE4u) | 8u;
        set_half(rt, self + offset, value);
        put(rt, self + 0xE4u, flags);
        ctx.gpr[2] = flags;
    }
}
// word(self + 0x48) of a non-null `self`; the tail call happens only when that is non-zero.
u32 inner_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (self == 0u) return 0u;
    ctx.gpr[2] = word(rt, self + 0x48u);
    return ctx.gpr[2];
}
// Low nibble of word +0xE0 replaced by `(flag ? bit : 0) | (old & keep)`.
void set_nibble(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 bit, u32 keep) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const u32 state = word(rt, self + 0xE0u);
    const u32 value = (flag == 0u ? 0u : bit) | (state & keep);
    ctx.gpr[3] = state & keep;
    ctx.gpr[2] = value;
    put(rt, self + 0xE0u, (state & ~0xFu) | (value & 0xFu));
}

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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08805AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B19FA4u); G[4] = G[2]; call_here(rt, ctx, 0x08B19F60u); } jump(ctx, 0x08805C0Cu); }
void func_08809B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x118u); put(rt, G[2] + 8u, bits_of(ctx.fpr[12])); G[3] = word(rt, G[4] + 0x118u); put(rt, G[3] + 0xCu, bits_of(ctx.fpr[13])); G[2] = word(rt, G[4] + 0x118u); put(rt, G[2] + 0x10u, bits_of(ctx.fpr[14])); G[3] = word(rt, G[4] + 0x118u); put(rt, G[3] + 0x14u, bits_of(ctx.fpr[15])); }
void func_0880AA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4] + 8u, 0u); G[2] = 0u; G[3] = 4u; do { G[2] += 1u; put(rt, G[4] + 0xCu, 0u); G[4] += 4u; } while (G[2] != G[3]); }
void func_0880B72C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x30u; G[5] = word(rt, G[5]); call_with(rt, ctx, 0x08869230u, 0x10u); G[2] = 1u; }
void func_0881AF04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = static_cast<std::int32_t>(G[5]) < 0x1E ? 1u : 0u; if (static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0u; return; } G[2] = (G[5] << 3) + G[4] + 0x354u; if (G[3] != 0u) return; G[2] = 0u; }
void func_0885C164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4]); G[2] = G[5] << 6; G[5] <<= 3; G[2] -= G[5]; G[3] = word(rt, G[6]); G[4] = (G[2] << 3) - G[2] + G[3]; jump(ctx, 0x0885D52Cu); }
void func_0885C290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4]); G[2] = G[5] << 6; G[5] <<= 3; G[2] -= G[5]; G[3] = word(rt, G[6]); G[4] = (G[2] << 3) - G[2] + G[3]; jump(ctx, 0x0885D9E4u); }
void func_0885D508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x1Cu, G[5]); G[2] = G[5] == 0xFFFFFFFFu ? 3u : 1u; put(rt, G[4] + 0x4Cu, G[2]); }
void func_0885E54C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 6; G[2] = word(rt, G[4]); G[5] = (G[5] << 4) + G[3] + 0x4120u; G[2] += G[5]; G[3] = load8(G[2] + 0x17u); if (G[3] == 0u) G[2] = 0u; }
void func_0885EA04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = (G[5] << 1) + G[2]; G[3] = sext(load16(G[5] + 0x3E84u), 16u); G[2] = 0xFFFFFFFFu; if (G[3] == G[2]) store16(G[5] + 0x3E84u, G[6]); }
void func_08866490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] < 0x40u ? 1u : 0u; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = G[5] << 2; G[4] += G[2]; if (G[3] == 0u) return; G[2] = 0xFFFFFFFFu; put(rt, G[4] + 4u, G[2]); }
void func_08869E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u) + 0x4238u; call_with(rt, ctx, 0x0886B010u, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_0886BB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = G[2] + 0x154u; G[2] = word(rt, G[3]) - 1u; put(rt, G[3], G[2]); G[2] = 1u; }
void func_0886BC38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = G[2] + 0x154u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; }
void func_088723B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x5Cu); ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); G[2] = G[3] + 0x18u; put(rt, G[3] + 0x18u, bits_of(ctx.fpr[1])); put(rt, G[2] + 8u, bits_of(ctx.fpr[0])); put(rt, G[2] + 4u, bits_of(ctx.fpr[2])); }
void func_08874BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = word(rt, G[4] + 8u); G[4] = word(rt, G[4] + 0xCu); call_with(rt, ctx, 0x08874820u, 0x10u); }
void func_0887CB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); G[2] = G[3] + 0x3ECu; put(rt, G[3] + 0x3ECu, bits_of(ctx.fpr[1])); put(rt, G[2] + 8u, bits_of(ctx.fpr[0])); put(rt, G[2] + 4u, bits_of(ctx.fpr[2])); }
void func_08886DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = word(rt, G[4] + 8u); G[4] = word(rt, G[4] + 0xCu); call_with(rt, ctx, 0x08886B00u, 0x10u); }
void func_0888FDE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x0888F550u, 0x10u); G[2] = __builtin_bswap32(word(rt, G[2] + 0xD8u)); }
void func_08890B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08890AC0u, 0x10u); G[2] = __builtin_bswap32(word(rt, G[2])); }
void func_088910C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 4u); G[5] = word(rt, G[4]); G[4] += 8u; call_with(rt, ctx, 0x08890288u, 0x10u); }
void func_08892114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 8u; call_with(rt, ctx, 0x0888F550u, 0x10u); G[2] = __builtin_bswap32(word(rt, G[2] + 0xCu)); }
void func_0889854C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x088F8A14u, 0x10u); G[2] = (G[2] ^ 1u) == 0u ? 1u : 0u; }
void func_088C5088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[7] = G[4]; G[3] = G[5]; G[25] = word(rt, G[2] + 0x50u); G[5] = G[6]; G[4] = G[3]; G[6] = G[7]; G[7] = 8u; jump(ctx, G[25]); }
void func_088C557C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088C54ACu, {ctx.gpr[4], ctx.gpr[6] + 1u, ctx.gpr[5]});
}
void func_088C55A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088C54ACu, {ctx.gpr[4], ctx.gpr[6] + 4u, ctx.gpr[5]});
}
void func_088D0AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x4Cu); G[5] = 1u; G[3] = G[2] - 1u; if (static_cast<std::int32_t>(G[2]) > 0) { put(rt, G[4] + 0x4Cu, G[3]); if (G[3] == 0u) { G[2] = G[5]; return; } } G[5] = 0u; G[2] = 0u; }
void func_088D9FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x640u); G[3] = G[4] + 4u; if (G[5] == 0u) G[5] = G[3]; G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x088E49E4u); }
void func_088ECA80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x53Cu); G[3] = word(rt, G[4] + 0x540u); G[6] = 0u; G[5] = G[2] << 5; if (G[2] != G[3]) { G[2] = word(rt, G[4] + 0x544u); G[6] = G[2] + G[5]; } G[2] = G[6]; }
void func_088ECAA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x53Cu) + 1u; G[3] = sra(G[2], 31u) >> 27; G[2] = ((G[2] + G[3]) & 0x1Fu) - G[3]; put(rt, G[4] + 0x53Cu, G[2]); }
void func_088F1098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[2] << 3; G[7] = G[4] + 4u + G[3]; G[2] += 1u; put(rt, G[4], G[2]); put(rt, G[7] + 4u, G[6]); put(rt, G[7], G[5]); }
void func_088F1AC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x9BCu); ctx.fpr[2] = float_at(rt, G[4] + 0x9C0u); ctx.fpr[0] = float_at(rt, G[4] + 0x9B8u); put(rt, G[4] + 0x9BCu, bits_of(ctx.fpr[12])); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; put(rt, G[4] + 0x9C0u, 0u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; put(rt, G[4] + 0x9B8u, bits_of(ctx.fpr[0])); }
void func_088F6C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); jump(ctx, 0x088F6AE4u); }
void func_088FD9AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, 0x08CE9EF8u) << 2; G[3] = __builtin_bswap32(word(rt, G[4] + 0x14u)) + G[5]; G[2] = __builtin_bswap32(word(rt, G[3])); }
void func_08905E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 inner = inner_object(rt, ctx);
    if (inner != 0u) tail_call(ctx, 0x08AF7A70u, {inner});
}
void func_08905F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 inner = inner_object(rt, ctx);
    if (inner != 0u) tail_call(ctx, 0x08AF70C4u, {inner});
}
void func_08905FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 inner = inner_object(rt, ctx);
    if (inner != 0u) tail_call(ctx, 0x08AF7030u, {inner});
}
void func_089060A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 inner = inner_object(rt, ctx);
    if (inner != 0u) tail_call(ctx, 0x08AF7214u, {inner});
}
void func_08910EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; put(rt, G[4] + 4u, G[2]); put(rt, G[4] + 0x14u, 0u); store8(G[4], 0u); store8(G[4] + 1u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); }
void func_08928194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] + 0x20E4u; ctx.fpr[2] = float_at(rt, G[2] + 8u); ctx.fpr[1] = float_at(rt, G[2] + 4u); ctx.fpr[0] = float_at(rt, G[2]); G[2] = G[4]; put(rt, G[4] + 4u, bits_of(ctx.fpr[1])); put(rt, G[4], bits_of(ctx.fpr[0])); put(rt, G[4] + 8u, bits_of(ctx.fpr[2])); }
void func_08930864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0u; G[5] = 0u; call_with(rt, ctx, 0x089306DCu, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08931AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 1; G[5] = static_cast<std::int32_t>(G[5]) < 0x10 ? 1u : 0u; G[2] = 0u; if (G[5] == 0u) return; G[2] = word(rt, G[4] + 4u) + G[3]; G[2] = sext(load16(G[2] + 0x38u), 16u); }
void func_08945630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x20u); ctx.fpr[0] = 0.0f; G[2] = ctx.fpr[1] == ctx.fpr[0] ? 0u : 1u; }
void func_08948B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load8(G[4] + 3u), 8u); store8(G[4] + 0x16u, G[5]); G[2] = static_cast<std::int32_t>(G[2]) < 2 ? 1u : 0u; store8(G[4] + 0x17u, G[6]); if (G[2] == 0u) return; G[2] = 2u; store8(G[4] + 3u, 2u); }
void func_089593B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; G[3] = 3u; do { G[2] += 1u; put(rt, G[4] + 0x7Cu, 0u); put(rt, G[4] + 0x8Cu, 0u); G[4] += 4u; } while (G[2] != G[3]); }
void func_089596B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08959180u, 0x10u); G[3] = 2u; store8(G[2] + 0x78u, 2u); }
void func_08966050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_nibble(rt, ctx, 8u, 7u);
}
void func_08966074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_nibble(rt, ctx, 4u, 0xBu);
}
void func_089772F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x1C8u); put(rt, G[5], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 0x1CCu); put(rt, G[6], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[4] + 0x1D0u); put(rt, G[7], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 0x1D4u); put(rt, G[8], bits_of(ctx.fpr[1])); }
void func_08985D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = call(rt, ctx, 0x08B7DF7Cu, {ctx.gpr[4], 0u, 0xAu});
    ctx.gpr[2] = sign8(raw);
}
void func_08985D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = call(rt, ctx, 0x08B7E4CCu, {ctx.gpr[4], 0u, 0xAu});
    ctx.gpr[2] = raw & 0xFFu;
}
void func_08985D5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = call(rt, ctx, 0x08B7DF7Cu, {ctx.gpr[4], 0u, 0xAu});
    ctx.gpr[2] = sign16(raw);
}
void func_08985D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = call(rt, ctx, 0x08B7E4CCu, {ctx.gpr[4], 0u, 0xAu});
    ctx.gpr[2] = raw & 0xFFFFu;
}
void func_089C4FDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089C4FD4u, 0x10u); G[4] = G[2]; G[5] = 0xFFFFFFFFu; jump(ctx, 0x089785BCu); }
void func_089F107C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] <<= 2; G[2] = word(rt, G[4] + 0x20u) + G[5]; G[4] = G[2]; if (G[2] == G[6]) return; G[5] = word(rt, G[6]); jump(ctx, 0x08A5BAD8u); }
void func_08A0B0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[3] = 0xFFFFFFFFu; put(rt, G[4] + 0x10u, G[3]); put(rt, G[4], G[1]); put(rt, G[4] + 0xCu, G[1]); put(rt, G[4] + 8u, G[1]); put(rt, G[4] + 4u, G[1]); jump(ctx, 0x08A0B09Cu); }
void func_08A0B180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08A0B114u, 0x10u); G[2] = (G[2] ^ 1u) & 0xFFu; }
void func_08A0D024(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_half_flagged(rt, ctx, 0x11Eu);
}
void func_08A0D4B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_half_flagged(rt, ctx, 0x118u);
}
void func_08A0D4D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_half_flagged(rt, ctx, 0x11Au);
}
void func_08A0D4F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_half_flagged(rt, ctx, 0x11Cu);
}
void func_08A2DF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = global_object(rt, ctx);
    }
    tail_call(ctx, 0x08B3027Cu, {object});
}
void func_08A2DF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = global_object(rt, ctx);
    }
    tail_call(ctx, 0x08B2FE7Cu, {object});
}
void func_08A2DF84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = global_object(rt, ctx);
    }
    tail_call(ctx, 0x08B30F84u, {object});
}
void func_08A2DFA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = global_object(rt, ctx);
    }
    tail_call(ctx, 0x08B2F44Cu, {object});
}
void func_08A44710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[3] = G[4]; G[4] = G[2]; if (G[2] == 0u) return; G[6] = word(rt, G[3] + 0x18u) - 1u; jump(ctx, 0x08B7C830u); }
void func_08A64CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4] + 4u, 0u); G[2] = 0x08CD2890u; G[3] = word(rt, G[4] + 4u) & 0xFFu; put(rt, G[4] + 8u, G[5]); put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); }
void func_08A6C004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); put(rt, frame.at(0u), 0u); G[4] = word(rt, G[4] + 0x14u); call_here(rt, ctx, 0x08A9FB90u); }
void func_08A6C110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); put(rt, frame.at(0u), 0u); G[4] = word(rt, G[4] + 0x14u); call_here(rt, ctx, 0x08A9FCD8u); }
void func_08A8AA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 raw = call(rt, ctx, 0x08B71DC8u, {word(rt, self + 0x18u), word(rt, self + 0x1Cu)});
    ctx.gpr[2] = sign8(raw);
}
void func_08A8AA6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 raw = call(rt, ctx, 0x08B71DC8u, {word(rt, self + 0x18u), word(rt, self + 0x1Cu)});
    ctx.gpr[2] = sign16(raw);
}
void func_08A8AAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 raw = call(rt, ctx, 0x08B6F110u, {word(rt, self + 0x18u), word(rt, self + 0x1Cu)});
    ctx.gpr[2] = raw & 0xFFu;
}
void func_08A8AAF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 raw = call(rt, ctx, 0x08B6F110u, {word(rt, self + 0x18u), word(rt, self + 0x1Cu)});
    ctx.gpr[2] = raw & 0xFFFFu;
}
void func_08AA5D90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); store8(frame.at(0u), G[5]); G[6] = 1u; G[5] = frame.at(0u); call_here(rt, ctx, 0x08AA5D2Cu); }
void func_08AB9120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = word(rt, G[5] + 4u); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); G[2] = word(rt, G[5] + 4u); G[3] = word(rt, G[5]); put(rt, G[4] + 0xCu, G[2]); put(rt, G[4] + 8u, G[3]); }
void func_08AB9144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(4u), G[6]); call_here(rt, ctx, 0x08AB9120u); }
void func_08AC2894(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[3] = 1u; if (G[4] != 0u) { G[2] = word(rt, G[4] + 4u); if (G[2] != 0u) { G[2] = G[3]; return; } } G[3] = 0u; G[2] = 0u; }
void func_08AC2CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[3] = 0xFFFFFFFFu; put(rt, G[4] + 4u, G[3]); G[5] = 0u; G[4] += 0xCu; put(rt, G[2], G[3]); G[6] = 0x180u; put(rt, G[2] + 8u, 0u); jump(ctx, 0x08B36018u); }
void func_08AC2CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[4]; G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) < 0) return; G[2] = 0xFFFFFFFFu; put(rt, G[3], G[2]); jump(ctx, 0x08C7A860u); }
void func_08AC4440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; G[3] = 0u; put(rt, G[4] + 0x120u, 0u); put(rt, G[4] + 0x124u, 0u); put(rt, G[4] + 0x128u, 0u); put(rt, G[4] + 0x12Cu, 0u); put(rt, G[4] + 0x130u, 0u); G[4] += 8u; jump(ctx, 0x08B0C99Cu); }
void func_08AC447C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = sext(G[4], 8u); G[2] = (G[4] - 0x41u) & 0xFFu; G[4] = (G[4] - 0x30u) & 0xFFu; G[4] = G[4] < 0xAu ? 1u : 0u; G[2] = G[2] < 0x1Au ? 1u : 0u; G[2] |= G[4]; }
void func_08AC939C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D00178u; call_with(rt, ctx, 0x08B2B6F4u, 0x10u); G[4] = G[2]; jump(ctx, 0x08B35254u); }
void func_08AC9A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B1CE38u, 0x10u); G[4] = G[2]; G[5] = G[3]; jump(ctx, 0x08AC99F4u); }
void func_08AD203C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] + 0x14u; G[2] = word(rt, G[3] + 8u); G[5] = G[2]; if (G[2] == 0u) return; put(rt, G[3] + 8u, 0u); G[4] = word(rt, G[4] + 0x29Cu); jump(ctx, 0x08AD6398u); }
void func_08AD9E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(word(rt, G[4]))) * 0x0019660D; ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[3] = ctx.lo + 1u; G[2] = (G[3] >> 12) & 0x7FFFu; put(rt, G[4], G[3]); }
void func_08AE4120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[4] + 0x50u); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[5])); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); G[2] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[1] = std::bit_cast<float>(G[2]); }
void func_08AE65FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFF80u; G[3] = 0x40u; store8(G[4] + 5u, 0x40u); store8(G[4] + 3u, 0x80u); store8(G[4], 0u); store8(G[4] + 1u, 0u); store8(G[4] + 2u, 0x80u); store8(G[4] + 4u, 0x40u); }
void func_08AE9E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08D40A3Cu); G[7] = word(rt, 0x08D40A3Cu); G[2] = ((G[4] << 3) - G[4]) << 3; G[7] += G[2]; put(rt, G[7] + 0x14u, G[6]); put(rt, G[7] + 0x10u, G[5]); }
void func_08AEB798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D41ADCu); G[5] = 1u; G[4] = word(rt, 0x08D41ADCu); call_with(rt, ctx, 0x08C7AA08u, 0x10u); }
void func_08AEB7E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D41AE0u; G[5] = 1u; call_with(rt, ctx, 0x08C7A898u, 0x10u); }
void func_08AFAC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); put(rt, G[4] + 4u, 0u); G[5] = 0u; G[6] = 0u; put(rt, G[4] + 8u, 0u); G[7] = 0u; G[25] = word(rt, G[3] + 0x34u); G[8] = 0u; jump(ctx, G[25]); }
void func_08AFEA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[5] + 0xCu); G[6] = word(rt, G[5]); G[7] = word(rt, G[5] + 4u); G[8] = word(rt, G[5] + 8u); put(rt, G[4] + 0xCu, G[3]); put(rt, G[4], G[6]); put(rt, G[4] + 4u, G[7]); put(rt, G[4] + 8u, G[8]); jump(ctx, 0x08AFEA88u); }
void func_08B0976C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D473E8u; call_with(rt, ctx, 0x08B3453Cu, 0x10u); G[2] &= 0xFFu; }
void func_08B099A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[5] = G[5] < 4u ? 1u : 0u; G[2] = 1u; if (G[5] == 0u) return; G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2] + 0xC4u); }
void func_08B09C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[5] = G[4] + 0x2Cu; G[4] = 0x08D47414u; if (G[2] == 0u) return; jump(ctx, 0x08B1F7B4u); }
void func_08B0A1CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[5] = G[4] + 0x2Cu; G[4] = 0x08D47414u; if (G[2] == 0u) return; jump(ctx, 0x08B1F9B4u); }
void func_08B120BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0x80A00032u; G[3] = 3u; if (G[4] == G[2]) { G[2] = G[3]; return; } G[3] = G[4] == 0u ? 0u : 2u; G[2] = G[3]; }
void func_08B18BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4]; G[4] = G[5]; if (G[5] == 0u) return; G[2] = word(rt, G[5]); G[25] = word(rt, G[2] + 0x1Cu); G[5] = word(rt, G[3] + 4u); jump(ctx, G[25]); }
void func_08B218A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B21894u, 0x10u); G[3] = load16(G[2]); G[2] += G[3]; }
void func_08B26C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; store16(G[4] + 4u, G[5]); store16(G[4] + 0xAu, G[2]); store16(G[4] + 0xEu, 0u); put(rt, G[4], 0u); store16(G[4] + 6u, G[2]); store16(G[4] + 8u, G[2]); store16(G[4] + 0xCu, 0u); }
void func_08B29348(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x20u) + 1u; put(rt, G[4] + 0x20u, G[2]); if (G[2] != 0u) return; G[2] = word(rt, G[4] + 0x24u) + 1u; put(rt, G[4] + 0x24u, G[2]); }
void func_08B2E2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4] + 0x3Fu; G[4] = static_cast<std::int32_t>(G[5]) < 0x10 ? 1u : 0u; G[3] = static_cast<std::int32_t>(G[5]) < 0x71 ? 1u : 0u; G[2] = 0x6Fu; if (G[3] == 0u) G[5] = G[2]; if (G[4] != 0u) G[5] = 0x10u; G[2] = G[5]; }
void func_08B2F5AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 1u; if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 0x8F8u)) ? 1u : 0u; if (G[2] != 0u) { G[2] = G[3]; return; } } G[3] = 0u; G[2] = 0u; }
void func_08B322A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08C7ADF0u, 0x10u); G[3] = high_half(0x08D6916Cu); store8(0x08D6916Cu, 0u); }
void func_08B3E6E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = 1u; if (G[2] != 0u) { G[2] = word(rt, G[4] + 0xCu); if (G[2] != 0u) { G[2] = G[3]; return; } } G[3] = 0u; G[2] = 0u; }
void func_08B3EBD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[5] << 5; G[3] = G[5] < 0x20u ? 1u : 0u; G[5] <<= 3; G[2] -= G[5]; G[5] = 0u; if (G[3] != 0u) G[5] = G[4] + G[2]; G[2] = G[5]; }
void func_08B3ED00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 handler = word(rt, self + 0xCu);
    ctx.gpr[2] = self;
    ctx.gpr[25] = handler;
    if (handler != 0u) tail_call(ctx, handler, {ctx.gpr[5], ctx.gpr[6], word(rt, self + 0x14u)});
}
void func_08B3ED24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[25] = word(rt, G[4] + 0x10u); G[2] = G[4]; G[4] = G[5]; G[5] = G[6]; if (G[25] == 0u) return; G[6] = word(rt, G[2] + 0x14u); jump(ctx, G[25]); }
void func_08B3F940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x10u; G[6] = 6u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] < 1u ? 1u : 0u; }
void func_08B40D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xCu; if (G[8] != G[2]) return; G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x40u); jump(ctx, G[25]); }
void func_08B4D05C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 4u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); }
void func_08B5284C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0x20u; offset += 4u) put(rt, G[4] + offset, 0u); }
void func_08B571D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08D6CE00u; G[5] = word(rt, G[2] + 0x27Cu) | G[4]; G[3] = word(rt, G[2] + 0x6Cu) | 0x80u; put(rt, G[2] + 0x6Cu, G[3]); put(rt, G[2] + 0x27Cu, G[5]); }
void func_08B5DF1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B5DE48u, 0x10u); G[2] = 0x08D726D8u; }
void func_08B62318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B62100u, 0x10u); G[2] = word(rt, G[2] + 4u) != 0u ? 1u : 0u; }
void func_08B83B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; if (G[6] == 0u) return; do { G[3] += 1u; G[7] = G[3] < G[6] ? 1u : 0u; store8(G[4], G[5]); G[4] += 1u; } while (G[7] != 0u); }
void func_08B8A6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load16(G[6]); if (G[5] == G[4]) return; G[2] = G[4]; do { store16(G[4], G[3]); G[4] = G[2] + 2u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08BA5240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[6]); if (G[5] == G[4]) return; G[2] = G[4]; do { put(rt, G[4], bits_of(ctx.fpr[0])); G[4] = G[2] + 4u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08BAA4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CC1AA8u); if (G[4] == G[5]) return; G[2] = 0x08CC1AA8u; put(rt, G[4] + 0xA4u, G[2]); for (;;) { G[4] += 0xACu; if (G[5] == G[4]) break; put(rt, G[4] + 0xA4u, G[2]); } }
void func_08BAAD78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CC1AA8u); if (G[4] == G[5]) return; G[2] = 0x08CC1AA8u; put(rt, G[4] + 0xA4u, G[2]); for (;;) { G[4] += 0xACu; if (G[5] == G[4]) break; put(rt, G[4] + 0xA4u, G[2]); } }
void func_08BE8E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[5] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x08966E24u); }
void func_08BE93A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[5] = 0u; call_here(rt, ctx, 0x0896A860u); ctx.fpr[0] = float_at(rt, frame.at(0u)); }
void func_08BE93C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 0u; call_here(rt, ctx, 0x0896A860u); ctx.fpr[0] = float_at(rt, frame.at(0u)); }
void func_08BEF068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[6]); if (G[5] == G[4]) return; G[2] = G[4]; do { put(rt, G[4], bits_of(ctx.fpr[0])); G[4] = G[2] + 4u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08BF8E18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[6]); if (G[5] == G[4]) return; G[2] = G[4]; do { put(rt, G[4], G[6]); G[4] = G[2] + 4u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08C01EDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = sext(load8(G[6]), 8u); if (G[5] == G[4]) return; G[2] = G[4]; do { store8(G[4], G[3]); G[4] = G[2] + 1u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08C3D0A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[6]); if (G[5] == G[4]) return; G[2] = G[4]; do { put(rt, G[4], G[6]); G[4] = G[2] + 4u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08C3FBF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0x20u; offset += 4u) put(rt, G[4] + offset, G[5]); }
void func_08C4AF08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = sext(load16(G[6]), 16u); if (G[5] == G[4]) return; G[2] = G[4]; do { store16(G[4], G[3]); G[4] = G[2] + 2u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08C709B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], 0x08CDBD94u); G[3] = 0x08CDB088u; G[2] = G[4]; G[4] += 4u; put(rt, G[2] + 4u, G[3]); jump(ctx, 0x08B6543Cu); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_173
} // namespace gt::game

#define HOT173(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_173::name, gt::ReturnKind::kind)
// HOT173(0x08805AF8u, func_08805AF8, Word);  // no comparable synthetic case yet
HOT173(0x08809B8Cu, func_08809B8C, Word);
HOT173(0x0880AA94u, func_0880AA94, Word);
// HOT173(0x0880B72Cu, func_0880B72C, Word);  // no comparable synthetic case yet
HOT173(0x0881AF04u, func_0881AF04, Word);
HOT173(0x0885C164u, func_0885C164, Word);
HOT173(0x0885C290u, func_0885C290, Word);
HOT173(0x0885D508u, func_0885D508, Word);
HOT173(0x0885E54Cu, func_0885E54C, Word);
HOT173(0x0885EA04u, func_0885EA04, Word);
HOT173(0x08866490u, func_08866490, Word);
HOT173(0x08869E84u, func_08869E84, Word);
HOT173(0x0886BB1Cu, func_0886BB1C, Word);
HOT173(0x0886BC38u, func_0886BC38, Word);
HOT173(0x088723B8u, func_088723B8, Word);
// HOT173(0x08874BE0u, func_08874BE0, Word);  // no comparable synthetic case yet
HOT173(0x0887CB18u, func_0887CB18, Word);
HOT173(0x08886DE0u, func_08886DE0, Word);
HOT173(0x0888FDE4u, func_0888FDE4, Word);
// HOT173(0x08890B20u, func_08890B20, Word);  // no comparable synthetic case yet
HOT173(0x088910C0u, func_088910C0, Word);
HOT173(0x08892114u, func_08892114, Word);
HOT173(0x0889854Cu, func_0889854C, Word);
HOT173(0x088C5088u, func_088C5088, Word);
HOT173(0x088C557Cu, func_088C557C, Word);
HOT173(0x088C55A0u, func_088C55A0, Word);
HOT173(0x088D0AB4u, func_088D0AB4, Word);
HOT173(0x088D9FB4u, func_088D9FB4, Word);
HOT173(0x088ECA80u, func_088ECA80, Word);
HOT173(0x088ECAA4u, func_088ECAA4, Word);
HOT173(0x088F1098u, func_088F1098, Word);
HOT173(0x088F1AC4u, func_088F1AC4, Word);
HOT173(0x088F6C54u, func_088F6C54, Word);
HOT173(0x088FD9ACu, func_088FD9AC, Word);
HOT173(0x08905E90u, func_08905E90, Word);
HOT173(0x08905F30u, func_08905F30, Word);
HOT173(0x08905FB4u, func_08905FB4, Word);
HOT173(0x089060A4u, func_089060A4, Word);
HOT173(0x08910EE0u, func_08910EE0, Word);
HOT173(0x08928194u, func_08928194, Word);
// HOT173(0x08930864u, func_08930864, Word);  // no comparable synthetic case yet
HOT173(0x08931AFCu, func_08931AFC, Word);
HOT173(0x08945630u, func_08945630, Word);
HOT173(0x08948B54u, func_08948B54, Word);
HOT173(0x089593B8u, func_089593B8, Word);
HOT173(0x089596B0u, func_089596B0, Word);
HOT173(0x08966050u, func_08966050, Word);
HOT173(0x08966074u, func_08966074, Word);
HOT173(0x089772F4u, func_089772F4, Word);
HOT173(0x08985D14u, func_08985D14, Word);
HOT173(0x08985D38u, func_08985D38, Word);
HOT173(0x08985D5Cu, func_08985D5C, Word);
HOT173(0x08985D80u, func_08985D80, Word);
HOT173(0x089C4FDCu, func_089C4FDC, Word);
HOT173(0x089F107Cu, func_089F107C, Word);
HOT173(0x08A0B0CCu, func_08A0B0CC, Word);
HOT173(0x08A0B180u, func_08A0B180, Word);
HOT173(0x08A0D024u, func_08A0D024, Word);
HOT173(0x08A0D4B0u, func_08A0D4B0, Word);
HOT173(0x08A0D4D4u, func_08A0D4D4, Word);
HOT173(0x08A0D4F8u, func_08A0D4F8, Word);
HOT173(0x08A2DF3Cu, func_08A2DF3C, Word);
HOT173(0x08A2DF60u, func_08A2DF60, Word);
HOT173(0x08A2DF84u, func_08A2DF84, Word);
HOT173(0x08A2DFA8u, func_08A2DFA8, Word);
HOT173(0x08A44710u, func_08A44710, Word);
HOT173(0x08A64CA4u, func_08A64CA4, Word);
HOT173(0x08A6C004u, func_08A6C004, Word);
HOT173(0x08A6C110u, func_08A6C110, Word);
HOT173(0x08A8AA48u, func_08A8AA48, Word);
HOT173(0x08A8AA6Cu, func_08A8AA6C, Word);
HOT173(0x08A8AAD0u, func_08A8AAD0, Word);
HOT173(0x08A8AAF4u, func_08A8AAF4, Word);
HOT173(0x08AA5D90u, func_08AA5D90, Word);
HOT173(0x08AB9120u, func_08AB9120, Word);
HOT173(0x08AB9144u, func_08AB9144, Word);
HOT173(0x08AC2894u, func_08AC2894, Word);
HOT173(0x08AC2CB8u, func_08AC2CB8, Word);
HOT173(0x08AC2CDCu, func_08AC2CDC, Word);
// HOT173(0x08AC4440u, func_08AC4440, Word);  // no comparable synthetic case yet
HOT173(0x08AC447Cu, func_08AC447C, Word);
HOT173(0x08AC939Cu, func_08AC939C, Word);
HOT173(0x08AC9A68u, func_08AC9A68, Word);
HOT173(0x08AD203Cu, func_08AD203C, Word);
HOT173(0x08AD9E08u, func_08AD9E08, Word);
HOT173(0x08AE4120u, func_08AE4120, Word);
HOT173(0x08AE65FCu, func_08AE65FC, Word);
HOT173(0x08AE9E94u, func_08AE9E94, Word);
HOT173(0x08AEB798u, func_08AEB798, Word);
HOT173(0x08AEB7E4u, func_08AEB7E4, Word);
HOT173(0x08AFAC58u, func_08AFAC58, Word);
HOT173(0x08AFEA94u, func_08AFEA94, Word);
HOT173(0x08B0976Cu, func_08B0976C, Word);
HOT173(0x08B099A4u, func_08B099A4, Word);
HOT173(0x08B09C54u, func_08B09C54, Word);
HOT173(0x08B0A1CCu, func_08B0A1CC, Word);
HOT173(0x08B120BCu, func_08B120BC, Word);
HOT173(0x08B18BCCu, func_08B18BCC, Word);
HOT173(0x08B218A4u, func_08B218A4, Word);
HOT173(0x08B26C84u, func_08B26C84, Word);
HOT173(0x08B29348u, func_08B29348, Word);
HOT173(0x08B2E2C0u, func_08B2E2C0, Word);
HOT173(0x08B2F5ACu, func_08B2F5AC, Word);
// HOT173(0x08B322A0u, func_08B322A0, Word);  // no comparable synthetic case yet
HOT173(0x08B3E6E8u, func_08B3E6E8, Word);
HOT173(0x08B3EBD0u, func_08B3EBD0, Word);
HOT173(0x08B3ED00u, func_08B3ED00, Word);
HOT173(0x08B3ED24u, func_08B3ED24, Word);
HOT173(0x08B3F940u, func_08B3F940, Word);
HOT173(0x08B40D30u, func_08B40D30, Word);
HOT173(0x08B4D05Cu, func_08B4D05C, Word);
HOT173(0x08B5284Cu, func_08B5284C, Word);
HOT173(0x08B571D0u, func_08B571D0, Word);
HOT173(0x08B5DF1Cu, func_08B5DF1C, Word);
HOT173(0x08B62318u, func_08B62318, Word);
HOT173(0x08B83B00u, func_08B83B00, Word);
// HOT173(0x08B8A6D8u, func_08B8A6D8, Word);  // no comparable synthetic case yet
HOT173(0x08BA5240u, func_08BA5240, Word);
// HOT173(0x08BAA4E0u, func_08BAA4E0, Word);  // no comparable synthetic case yet
HOT173(0x08BAAD78u, func_08BAAD78, Word);
HOT173(0x08BE8E4Cu, func_08BE8E4C, Word);
HOT173(0x08BE93A4u, func_08BE93A4, Float);
HOT173(0x08BE93C8u, func_08BE93C8, Float);
HOT173(0x08BEF068u, func_08BEF068, Word);
HOT173(0x08BF8E18u, func_08BF8E18, Word);
HOT173(0x08C01EDCu, func_08C01EDC, Word);
HOT173(0x08C3D0A4u, func_08C3D0A4, Word);
HOT173(0x08C3FBF0u, func_08C3FBF0, Word);
HOT173(0x08C4AF08u, func_08C4AF08, Word);
HOT173(0x08C709B8u, func_08C709B8, Word);
