// Hot batch 262: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (guarded static initialisers, a destructor over an empty array, a jump table that picks a string, a float
// pass-through and a box built around a point).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_262 {
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

void func_08A64D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[2] = 0x08D00000u; G[17] = 0x08D00000u; put(rt, frame.at(0u), G[16]); G[16] = 0x08CFF270u; put(rt, frame.at(8u), G[31]); G[3] = sext(load8(0x08CFF270u), 8u); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); const bool first = G[2] != 0u; G[2] = 0x08CFF288u; if (first) { G[4] = 0x08CFF288u; call_here(rt, ctx, 0x08A64D64u); G[4] = G[16]; call_here(rt, ctx, 0x08B64494u); G[4] = 0x08A64E14u; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08CFF288u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08A66110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[2] = 0x08D00000u; G[17] = 0x08D00000u; put(rt, frame.at(0u), G[16]); G[16] = 0x08CFF260u; put(rt, frame.at(8u), G[31]); G[3] = sext(load8(0x08CFF260u), 8u); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); const bool first = G[2] != 0u; G[2] = 0x08CFF298u; if (first) { G[4] = 0x08CFF298u; call_here(rt, ctx, 0x08A660ECu); G[4] = G[16]; call_here(rt, ctx, 0x08B64494u); G[4] = 0x08A6619Cu; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08CFF298u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08A66204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[2] = 0x08D00000u; G[17] = 0x08D00000u; put(rt, frame.at(0u), G[16]); G[16] = 0x08CFF258u; put(rt, frame.at(8u), G[31]); G[3] = sext(load8(0x08CFF258u), 8u); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); const bool first = G[2] != 0u; G[2] = 0x08CFF2A0u; if (first) { G[4] = 0x08CFF2A0u; call_here(rt, ctx, 0x08A661E0u); G[4] = G[16]; call_here(rt, ctx, 0x08B64494u); G[4] = 0x08A66290u; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08CFF2A0u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08A68C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[2] = 0x08D00000u; G[17] = 0x08D00000u; put(rt, frame.at(0u), G[16]); G[16] = 0x08CFF210u; put(rt, frame.at(8u), G[31]); G[3] = sext(load8(0x08CFF210u), 8u); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); const bool first = G[2] != 0u; G[2] = 0x08CFF2E8u; if (first) { G[4] = 0x08CFF2E8u; call_here(rt, ctx, 0x08A68C18u); G[4] = G[16]; call_here(rt, ctx, 0x08B64494u); G[4] = 0x08A68CC8u; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08CFF2E8u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08B469E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = 0x08CDAAB8u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B46880u); G[3] = G[16] + 0x489Cu; G[2] = G[16] + 0x494Cu; if (G[3] != 0u) { const bool empty = G[3] == G[2]; G[2] = 8u; if (!empty) G[2] = 0xFFFFFFFFu; } G[4] = G[16] + 0x48u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08B486A8u); } }
void func_088D2BC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put_float(rt, frame.at(0x10u), F[20]); F[20] = F[12]; put(rt, frame.at(4u), G[31]); G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088DB5C4u); F[0] = F[20]; if (G[2] == 0u) { G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x10u)); return; } G[4] = word(rt, G[16]); call_here(rt, ctx, 0x088DB5C4u); F[12] = F[20]; G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x10u)); G[4] = G[2]; } jump(ctx, 0x088FB0C8u); } }
void func_08B48D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = word(rt, G[4] + 0x10u); G[2] = G[3] < 5u ? 1u : 0u; const bool in_table = G[2] != 0u; G[2] = 0x08CB0000u; if (!in_table) { G[2] = 0x08CB56D0u; return; } G[2] = G[3] << 2; G[3] = 0x08C95448u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B48D94u: G[2] = 0x08CB56C4u; break; case 0x08B48DA8u: G[2] = 0x08CB56D4u; break; case 0x08B48DB4u: G[2] = 0x08CB56DCu; break; case 0x08B48DC0u: G[2] = 0x08CB56E0u; break; default: G[2] = 0x08CB56D0u; break; } } }
void func_08899B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[31]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); F[1] = float_at(rt, frame.at(0u)); F[2] = float_at(rt, frame.at(4u)); F[3] = float_at(rt, frame.at(8u)); G[1] = 0x41000000u; F[0] = std::bit_cast<float>(G[1]); F[5] = F[1] + F[0]; F[6] = F[3] + F[0]; F[4] = F[2] + F[0]; F[1] = F[1] - F[0]; F[2] = F[2] - F[0]; F[3] = F[3] - F[0]; G[4] = frame.at(0xCu); G[5] = frame.at(0x1Cu); put_float(rt, frame.at(0xCu), F[1]); put_float(rt, frame.at(0x10u), F[2]); put_float(rt, frame.at(0x14u), F[3]); put_float(rt, frame.at(0x1Cu), F[5]); put_float(rt, frame.at(0x20u), F[4]); put_float(rt, frame.at(0x24u), F[6]); call_here(rt, ctx, 0x08B54474u); G[2] = G[2] != 0u ? 1u : 0u; } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_262
} // namespace gt::game

#define HOT262(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_262::name, gt::ReturnKind::kind)
HOT262(0x08A64D88u, func_08A64D88, Word);
HOT262(0x08A66110u, func_08A66110, Word);
HOT262(0x08A66204u, func_08A66204, Word);
HOT262(0x08A68C3Cu, func_08A68C3C, Word);
HOT262(0x08B469E0u, func_08B469E0, Word);
// HOT262(0x088D2BC8u, func_088D2BC8, Float);  // no comparable synthetic case yet
HOT262(0x08B48D68u, func_08B48D68, Word);
HOT262(0x08899B38u, func_08899B38, Word);
