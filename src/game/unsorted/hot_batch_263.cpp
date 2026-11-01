// Hot batch 263: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (jump tables on a code, a bit-by-bit callback dispatch, a float formula around two calls, a resource open
// and calls with a zeroed float argument).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_263 {
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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08918BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { GuestFrame frame(ctx, 0x10u); F[12] = 0.0f; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); G[4] = G[4] + 0x20Cu; call_here(rt, ctx, 0x08926384u); for (const std::uint32_t off : {0x268u, 0x2C4u, 0x2F0u, 0x31Cu, 0x348u, 0x374u}) { F[12] = 0.0f; G[4] = G[16] + off; call_here(rt, ctx, 0x08926384u); } F[12] = 0.0f; G[4] = G[16] + 0x3A0u; G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08926384u); } }
void func_08926140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[1] = F[12] * F[0]; G[1] = 0x3C8EFA35u; F[12] = std::bit_cast<float>(G[1]); put(rt, frame.at(0u), G[31]); F[12] = F[1] * F[12]; call_here(rt, ctx, 0x08B66604u); G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); F[0] = F[12] / F[0]; G[1] = 0x3ED1F42Cu; F[1] = std::bit_cast<float>(G[1]); G[1] = 0x4013CD46u; F[2] = std::bit_cast<float>(G[1]); F[0] = F[0] * F[1]; F[0] = F[0] * F[2]; F[12] = F[12] / F[0]; call_here(rt, ctx, 0x08B65F0Cu); F[1] = F[0] + F[0]; G[1] = 0x42652EE1u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[1] * F[0]; } }
void func_08B59118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto clz = [](std::uint32_t x) { return x == 0u ? 32u : static_cast<std::uint32_t>(__builtin_clz(x)); }; GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(4u), G[17]); G[3] = word(rt, G[4] + 0x6Cu); G[16] = G[3] & G[5]; G[18] = G[4]; if (G[16] != 0u) { G[2] = ~G[5] & G[3]; G[3] = 0x08CE0000u; put(rt, G[4] + 0x6Cu, G[2]); G[17] = 0x08CE0E54u; G[2] = clz(G[16]); do { G[3] = G[2] << 2; G[3] = G[17] - G[3]; G[5] = word(rt, G[3] - 4u); G[2] = G[16] << (G[2] & 31u); G[16] = G[2] << 1; G[4] = G[18]; G[17] = G[3] - 4u; call_here(rt, ctx, G[5]); G[2] = clz(G[16]); } while (G[16] != 0u); } G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08949D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[3] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[4] + 8u); F[0] = 0.0f; G[4] = load8(G[3] + 0x270u); G[2] = G[4] < 0xCu ? 1u : 0u; G[6] = G[3] + 0x270u; if (G[2] == 0u) return; G[2] = G[4] << 2; G[3] = 0x08C8B3B8u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08949D78u: F[0] = float_at(rt, G[5] + 0x600u); break; case 0x08949D80u: F[0] = float_at(rt, G[5] + 0x5FCu); break; case 0x08949D88u: F[0] = float_at(rt, G[5] + 0x600u); F[1] = float_at(rt, G[5] + 0x5FCu); G[1] = 0x3F000000u; F[2] = std::bit_cast<float>(G[1]); F[1] = F[1] + F[0]; F[0] = F[1] * F[2]; break; case 0x08949DA4u: F[2] = float_at(rt, G[5] + 0x600u); F[0] = float_at(rt, G[5] + 0x5FCu); F[1] = float_at(rt, G[6] + 8u); F[0] = F[0] - F[2]; F[1] = F[1] * F[0]; F[0] = F[2] + F[1]; break; default: break; } } }
void func_08B2F774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5] < 0xEu ? 1u : 0u; G[3] = 0x08C90000u; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = G[5] << 2; G[3] = 0x08C93A78u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); if (G[4] == 0x08B2F798u) G[2] = 0xDu; else if (G[4] >= 0x08B2F7A0u && G[4] <= 0x08B2F800u && ((G[4] - 0x08B2F7A0u) & 7u) == 0u) G[2] = (G[4] - 0x08B2F7A0u) >> 3; else G[2] = 0u; } }
void func_0890AD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = 0x08CF0000u; put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(4u), G[17]); G[17] = G[6]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, G[5], 0u); G[5] = G[6]; G[2] = word(rt, 0x08CE9F68u); G[4] = G[2]; if (G[2] == 0u) G[16] = 0u; else { call_here(rt, ctx, 0x08AB7EC4u); G[3] = G[2]; G[5] = G[17]; if (G[2] == 0u) G[16] = 0u; else { G[2] = 1u; put(rt, G[16], G[2]); G[2] = 0x20u; put(rt, G[16] + 4u, G[3]); put(rt, G[16] + 8u, G[2]); G[4] = word(rt, 0x08CE9F68u); call_here(rt, ctx, 0x08AB7F5Cu); put(rt, G[18], G[2]); } } G[2] = G[16]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08B56E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5] < 0x10u ? 1u : 0u; G[6] = G[4]; if (G[2] == 0u) { G[2] = 0u; return; } G[3] = 0x08C95674u; G[2] = (G[5] << 2) + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B56E50u: G[2] = word(rt, G[6] + 0xD00u); return; case 0x08B56E58u: G[2] = word(rt, G[6] + 0xD04u); return; case 0x08B56E60u: G[2] = word(rt, G[6] + 0xD08u); return; case 0x08B56E68u: G[2] = word(rt, G[6] + 0xD0Cu); return; case 0x08B56E80u: G[2] = G[6] + 0x700u; return; case 0x08B56EB8u: G[2] = G[6] + 0x8C0u; return; default: break; } std::uint32_t target = 0u; switch (G[4]) { case 0x08B56E70u: target = 0x08B56C34u; break; case 0x08B56E78u: target = 0x08B56C94u; break; case 0x08B56E88u: target = 0x08B56F34u; break; case 0x08B56E90u: target = 0x08B56CF4u; break; case 0x08B56E98u: target = 0x08B56A88u; break; case 0x08B56EA0u: target = 0x08B56D4Cu; break; case 0x08B56EA8u: target = 0x08B56DC0u; break; case 0x08B56EB0u: target = 0x08B56B10u; break; default: G[2] = 0u; return; } G[4] = G[6]; jump(ctx, target); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_263
} // namespace gt::game

#define HOT263(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_263::name, gt::ReturnKind::kind)
HOT263(0x08918BB8u, func_08918BB8, Word);
HOT263(0x08926140u, func_08926140, Float);
HOT263(0x08B59118u, func_08B59118, Word);
HOT263(0x08949D40u, func_08949D40, Float);
HOT263(0x08B2F774u, func_08B2F774, Word);
HOT263(0x0890AD68u, func_0890AD68, Word);
HOT263(0x08B56E20u, func_08B56E20, Word);
