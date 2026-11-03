// Hot batch 301: seventy-eight-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (a vector push, a script bridge, a destructor, a string-array reset and a string lookup).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_301 {
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

void func_08975D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x38u), G[31]); G[17] = G[4]; G[16] = G[5]; bool push = true; if (static_cast<std::int32_t>(G[5]) >= 0) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[5] = word(rt, G[17] + 0xA8u); push = false; G[2] = G[16] << 6; G[3] = G[16] << 4; G[2] -= G[3]; G[5] += G[2]; G[6] = G[5]; G[4] = G[17] + 0xA8u; G[5] += 0x30u; call_here(rt, ctx, 0x08BED6FCu); } } if (push) { G[16] = G[17] + 0xA8u; G[4] = frame.at(0u); call_here(rt, ctx, 0x089D76A8u); G[5] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[17] = 0x08CD0000u; G[4] = 0x08CCDE00u; put(rt, frame.at(0u), G[4]); put(rt, frame.at(0x20u), 0u); put(rt, frame.at(0x24u), 0u); put(rt, frame.at(0x28u), 0u); put(rt, frame.at(0x2Cu), 0u); if (G[5] != G[2]) { F[0] = float_at(rt, frame.at(0xCu)); if (G[5] != 0u) { F[1] = float_at(rt, frame.at(0x10u)); G[2] = word(rt, frame.at(4u)); put_float(rt, G[5] + 0xCu, F[0]); F[0] = float_at(rt, frame.at(0x14u)); put_float(rt, G[5] + 0x10u, F[1]); put_float(rt, G[5] + 0x14u, F[0]); F[1] = float_at(rt, frame.at(0x18u)); F[0] = float_at(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(8u)); put(rt, G[5] + 4u, G[2]); put(rt, G[5] + 8u, G[3]); put_float(rt, G[5] + 0x18u, F[1]); put_float(rt, G[5] + 0x1Cu, F[0]); F[0] = float_at(rt, frame.at(0x20u)); put(rt, G[5], G[4]); put_float(rt, G[5] + 0x20u, F[0]); F[1] = float_at(rt, frame.at(0x24u)); put_float(rt, G[5] + 0x24u, F[1]); F[0] = float_at(rt, frame.at(0x28u)); put_float(rt, G[5] + 0x28u, F[0]); F[1] = float_at(rt, frame.at(0x2Cu)); put_float(rt, G[5] + 0x2Cu, F[1]); G[5] = word(rt, G[16] + 4u); } G[5] += 0x30u; put(rt, G[16] + 4u, G[5]); } else { G[4] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BED390u); } G[2] = 0x08CCDE00u; G[4] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x089D6684u); } G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } } }
void func_0898B0E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(0xCu); G[18] = G[4]; G[4] = G[20]; G[19] = frame.at(4u); G[17] = frame.at(8u); put_float(rt, frame.at(0x34u), F[21]); put_float(rt, frame.at(0x30u), F[20]); call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(0xCu)); G[4] = 0x08CA39F0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[17]; G[5] = 2u; F[21] = float_at(rt, G[16] + 0x1F0u); F[20] = float_at(rt, G[16] + 0x1ECu); call_here(rt, ctx, 0x08A98C1Cu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, G[18]); G[4] = 0x08CA39E8u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17]); F[12] = F[20]; G[4] = G[19]; G[16] = word(rt, G[2] + 0x88u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[17]; G[5] = 0u; G[6] = G[19]; call_here(rt, ctx, G[16]); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, G[18]); G[4] = 0x08CA39E8u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17]); F[12] = F[21]; G[4] = frame.at(0u); G[16] = word(rt, G[2] + 0x88u); call_here(rt, ctx, 0x08A87EECu); G[6] = frame.at(0u); G[4] = G[17]; G[5] = 1u; call_here(rt, ctx, G[16]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_0898D6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); G[19] = 0x08D74FD0u; G[17] = G[4]; G[2] = 0x08CBC030u; G[3] = 0x08CBC26Cu; G[4] = word(rt, G[17] + 0x1F4u); put(rt, G[17], G[2]); G[16] = G[4] - 0xCu; put(rt, G[17] + 0x38u, G[3]); if (G[16] != G[19]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0x3u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[17] + 0x1E0u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CCE158u; put(rt, G[17] + 0xF0u, G[2]); G[16] = G[17] + 0x1D4u; G[6] = frame.at(2u); G[4] = word(rt, G[17] + 0x1D4u); G[5] = word(rt, G[16] + 4u); G[18] = G[17] + 0xF0u; call_here(rt, ctx, 0x08BEEA84u); G[2] = word(rt, G[16] + 8u); G[4] = word(rt, G[17] + 0x1D4u); G[2] -= G[4]; G[2] = sra(G[2], 2); if (G[4] != 0u) { G[5] = G[2] << 2; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[16] = G[17] + 0x1BCu; G[4] = word(rt, G[17] + 0x1BCu); G[5] = word(rt, G[16] + 4u); G[6] = frame.at(1u); call_here(rt, ctx, 0x08BEEA84u); G[2] = word(rt, G[16] + 8u); G[4] = word(rt, G[17] + 0x1BCu); G[2] -= G[4]; G[2] = sra(G[2], 2); if (G[4] != 0u) { G[5] = G[2] << 2; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[4] = word(rt, G[17] + 0x120u); G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[19] != G[16]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[18]; call_here(rt, ctx, 0x0897D548u); G[4] = G[17]; call_here(rt, ctx, 0x0896BC0Cu); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); }  } }
void func_0899000C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x10u), G[16]); G[23] = G[4]; G[3] = G[23] + 0x130u; G[20] = frame.at(4u); G[19] = 0x80u; G[18] = 0u; G[17] = G[23] + 0x228u; G[2] = word(rt, G[4] + 0xE0u); G[4] = 1u; put(rt, G[23] + 0x220u, 0u); G[30] = (G[2] >> 14) & 1u; G[2] &= ~(1u << 14); put(rt, G[23] + 0xE0u, G[2]); G[2] = 0x08D70000u; G[22] = 0x08D74FD0u; put(rt, G[23] + 0x224u, 0u); G[21] = G[22] + 0xCu; put(rt, G[23] + 0x4A8u, 0u); put(rt, G[23] + 0x4ACu, 0u); put(rt, G[23] + 0x4B0u, 0u); put(rt, G[23] + 0x4B4u, G[4]); put(rt, G[23] + 0x4B8u, 0u); put(rt, G[23] + 0x4BCu, 0u); store8(G[23] + 0x4C0u, 0u); store8(G[3] + 0xC4u, G[4]); store8(G[3] + 0x38u, 0u); put(rt, G[3] + 0x3Cu, 0u); store8(G[3] + 0x36u, G[4]); G[3] = 0x40000u; G[2] = word(rt, G[23] + 0xE0u); put(rt, G[23] + 0x120u, G[4]); G[2] |= G[3]; put(rt, G[23] + 0xE0u, G[2]); G[4] = G[17]; do { G[5] = G[20]; put(rt, frame.at(4u), G[21]); call_here(rt, ctx, 0x08B855E0u); G[18] += 1u; G[2] = word(rt, frame.at(4u)); G[5] = 0xFFFFFFFFu; G[17] += 4u; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[16] != G[22]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[17]; } while (G[18] != G[19]); G[2] = word(rt, G[23] + 0xE0u); G[2] = (G[2] & ~(1u << 14)) | ((G[30] & 1u) << 14); put(rt, G[23] + 0xE0u, G[2]); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A2CF30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[18] = G[4]; if (static_cast<std::int32_t>(G[5]) > 0) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CAC140u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[5] = G[16]; G[4] = frame.at(0xCu); G[3] = word(rt, G[2] + 0xCu); G[16] = frame.at(0x10u); call_here(rt, ctx, G[3]); G[5] = word(rt, frame.at(0xCu)); G[4] = G[16]; call_here(rt, ctx, 0x08C1A21Cu); G[4] = G[16]; call_here(rt, ctx, 0x08C1A0C4u); G[17] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08C1A2D4u); G[16] = frame.at(4u); if (G[17] == 0u) { G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; } else { G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(8u); G[6] = frame.at(2u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B8542Cu); } } } G[4] = word(rt, frame.at(0xCu)); G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_301
} // namespace gt::game

#define HOT301(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_301::name, gt::ReturnKind::kind)
HOT301(0x08975D44u, func_08975D44, Word);
// HOT301(0x0898B0E0u, func_0898B0E0, Word);  // no comparable synthetic case yet
HOT301(0x0898D6D8u, func_0898D6D8, Word);
// HOT301(0x0899000Cu, func_0899000C, Word);  // no comparable synthetic case yet
HOT301(0x08A2CF30u, func_08A2CF30, Word);
