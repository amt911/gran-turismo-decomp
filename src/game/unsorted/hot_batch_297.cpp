// Hot batch 297: seventy-seven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (a cost product, a spline sample, a code-table search, a capped 64-bit read and state resets).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_297 {
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

void func_088CFB14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[17] = G[5]; G[16] = G[4]; put_float(rt, frame.at(0x18u), F[22]); put_float(rt, frame.at(0x14u), F[21]); put_float(rt, frame.at(0x10u), F[20]); G[5] = word(rt, G[4] + 0x10u); F[20] = float_at(rt, G[4] + 0x144u); G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088DA788u); F[12] = F[20]; F[13] = float_at(rt, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x088CFA9Cu); F[22] = F[0]; F[12] = float_at(rt, G[16] + 0x140u); G[4] = G[16]; call_here(rt, ctx, 0x088CF414u); F[12] = F[22]; G[4] = G[16]; put_float(rt, G[16] + 0x114u, F[0]); call_here(rt, ctx, 0x088CF4ACu); put_float(rt, G[16] + 0x120u, F[0]); G[2] = word(rt, G[17]); G[3] = word(rt, G[2] + 0x1Cu); G[4] = G[17]; call_here(rt, ctx, G[3]); F[12] = F[0]; G[4] = G[16]; call_here(rt, ctx, 0x088CF4F8u); put_float(rt, G[16] + 0x118u, F[0]); G[4] = G[16]; call_here(rt, ctx, 0x088CF554u); F[2] = float_at(rt, G[16] + 0x120u); F[1] = float_at(rt, G[16] + 0x114u); F[3] = float_at(rt, G[16] + 0x118u); put_float(rt, G[16] + 0x11Cu, F[0]); F[1] = F[1] * F[2]; F[2] = 0.0f; F[1] = F[1] * F[3]; F[21] = F[0] * F[1]; G[1] = 0x447A0000u; F[0] = 1000.0f; G[1] = 0x3C88AB7Cu; F[1] = std::bit_cast<float>(G[1]); F[0] = F[21] * F[0]; F[20] = F[0] * F[1]; G[4] = G[17]; if (F[2] < F[20]) { G[2] = word(rt, G[17]); G[3] = word(rt, G[2] + 0x1Cu); call_here(rt, ctx, G[3]); F[14] = F[0]; F[12] = F[21]; F[13] = F[22]; G[4] = G[16]; call_here(rt, ctx, 0x088CF5F4u); F[0] = F[20]; } else { F[20] = F[2]; put(rt, G[16] + 0xBCu, 0u); F[0] = F[20]; } F[22] = float_at(rt, frame.at(0x18u)); F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } } }
void func_088D12AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x4u), G[17]); G[20] = G[6]; G[19] = G[5]; G[18] = G[7]; G[2] = word(rt, G[4] + 0x18u); G[16] = G[4]; do { if (G[2] != 0u) break; G[4] = G[5]; G[5] = G[6]; call_here(rt, ctx, 0x088DA588u); G[17] = G[2]; if (G[2] == 0u) break; G[2] = word(rt, G[2] + 0x10u); F[12] = 0.0f; if (static_cast<std::int32_t>(G[2]) <= 0) break; F[13] = float_at(rt, G[16]); G[4] = G[18]; G[5] = 0u; call_here(rt, ctx, 0x088FB100u); F[2] = float_at(rt, G[17] + 0x10u); G[1] = 0x40666666u; F[3] = std::bit_cast<float>(G[1]); F[1] = static_cast<float>(std::bit_cast<std::int32_t>(F[2])); G[1] = 0x40400000u; F[2] = 3.0f; F[1] = F[1] / F[3]; F[1] = F[1] * F[2]; if (!(F[0] <= F[1])) break; G[16] = G[19] + 4u; G[4] = G[16]; call_here(rt, ctx, 0x088D6358u); G[3] = 1u; if (G[2] == G[3]) { put(rt, G[17] + 0x10u, 0u); break; } G[4] = G[16]; call_here(rt, ctx, 0x088D6358u); G[3] = 6u; if (G[2] != G[3]) break; G[5] = G[20]; G[4] = G[19]; call_here(rt, ctx, 0x088DB2C8u); if (G[2] == 0u) break; G[4] = G[19]; call_here(rt, ctx, 0x088D9F80u); G[17] = G[2]; if (static_cast<std::int32_t>(G[2]) <= 0) break; G[16] = 0u; do { G[5] = G[16]; G[4] = G[19]; G[16] += 1u; call_here(rt, ctx, 0x088DA588u); if (G[2] != 0u) put(rt, G[2] + 0x10u, 0u); } while (G[17] != G[16]); } while (false); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); }  } } }
void func_088DE740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x0u), G[16]); G[5] = 0u; G[6] = 0u; G[7] = 0xFFFFFFFFu; G[20] = 1u; G[19] = 0u; G[18] = 0u; G[17] = G[4]; call_here(rt, ctx, 0x088D6FD4u); while (true) { G[4] = G[17]; call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = 1u; G[6] = 0u; G[4] = G[17]; G[18] += 1u; if (G[2] == 0u) break; G[2] = word(rt, G[17] + 0xC0u); G[16] = G[19] + G[2]; call_here(rt, ctx, 0x088DE24Cu); G[3] = load8(G[16] + 0x2B4u); G[4] = G[16]; G[19] += 0x2F8u; if (G[3] != 0u) { call_here(rt, ctx, 0x088E85E8u); G[4] = G[17]; if (G[2] != 0u) { put(rt, G[16] + 0x2C8u, G[20]); G[6] = word(rt, G[17] + 0x90u); G[7] = word(rt, G[17] + 0x94u); call_here(rt, ctx, 0x088D9C24u); put(rt, G[16] + 0x2D0u, G[2]); put(rt, G[16] + 0x2D4u, G[3]); store8(G[16] + 0x2B4u, 0u); } } G[4] = G[17]; G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x088D9BBCu); } G[4] = word(rt, G[17] + 0x594u); if (G[4] != 0u) call_here(rt, ctx, 0x088E7690u); G[4] = G[17]; G[5] = 3u; call_here(rt, ctx, 0x088DC1B8u); G[4] = word(rt, G[17] + 0x148u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x6Cu); call_here(rt, ctx, G[2]); G[4] = word(rt, G[17] + 0x14Cu); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x18u); call_here(rt, ctx, G[2]); } store8(G[17] + 0x21u, 0u); store8(G[17] + 0x1Eu, 0u); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_088EAC34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x260u); put(rt, frame.at(0x254u), G[21]); put(rt, frame.at(0x250u), G[20]); put(rt, frame.at(0x258u), G[31]); put(rt, frame.at(0x24Cu), G[19]); put(rt, frame.at(0x248u), G[18]); put(rt, frame.at(0x244u), G[17]); put(rt, frame.at(0x240u), G[16]); G[20] = G[4]; G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x9Cu); G[21] = G[5] & 0xFFu; call_here(rt, ctx, G[2]); G[5] = 0u; G[6] = 0x234u; G[4] = frame.at(0u); G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B36018u); G[4] = word(rt, G[20] + 0xA8u); call_here(rt, ctx, 0x08B08A88u); G[4] = word(rt, G[20] + 0xA8u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08B089F8u); G[5] = 0x08C89F2Cu; G[6] = 0x20u; G[4] = frame.at(5u); store8(frame.at(4u), G[2]); call_here(rt, ctx, 0x08B7C830u); call_here(rt, ctx, 0x088D4808u); G[4] = word(rt, G[20] + 0xA8u); G[18] = G[2]; call_here(rt, ctx, 0x08B08AC0u); G[5] = G[2]; G[6] = G[18]; G[4] = frame.at(0x25u); call_here(rt, ctx, 0x08B79A88u); G[4] = word(rt, G[20] + 0xA8u); put(rt, frame.at(0x228u), G[18]); call_here(rt, ctx, 0x08B08A70u); G[17] = word(rt, G[20] + 0x538u); store16(frame.at(0x22Cu), G[2]); put(rt, frame.at(0x230u), G[20]); G[16] = word(rt, G[20] + 0xA8u); G[2] = word(rt, G[17]); G[4] = G[16]; G[19] = word(rt, G[2] + 0x28u); call_here(rt, ctx, 0x08B08AC0u); G[7] = G[2]; G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[16]; G[8] = G[18]; G[9] = G[21]; call_here(rt, ctx, G[19]); G[3] = 0u; if (G[2] != 0u) { G[3] = 1u; put(rt, G[20] + 0x574u, 0u); put(rt, G[20] + 0x560u, 0u); put(rt, G[20] + 0x564u, 0u); put(rt, G[20] + 0x568u, 0u); put(rt, G[20] + 0x570u, 0u); } } G[2] = G[3]; G[21] = word(rt, frame.at(0x254u)); G[20] = word(rt, frame.at(0x250u)); G[19] = word(rt, frame.at(0x24Cu)); G[18] = word(rt, frame.at(0x248u)); G[17] = word(rt, frame.at(0x244u)); G[16] = word(rt, frame.at(0x240u)); }  } }
void func_08907910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[6] = word(rt, G[4]); F[2] = F[12]; if (static_cast<std::int32_t>(G[6]) < 2) { G[2] = 1u; if (G[6] == G[2]) { G[2] = G[6] - 1u; G[3] = G[2] << 1; G[7] = G[6] - 2u; } else { G[3] = G[2] << 1; G[7] = G[2] - 1u; } } else { F[0] = float_at(rt, G[4] + 0x14u); G[7] = 0u; G[3] = 2u; G[5] = G[4]; if (F[12] < F[0]) { G[2] = 0u; } else { G[2] = 1u; bool hit = false; while (true) { G[2] += 1u; if (G[6] == G[2]) break; F[0] = float_at(rt, G[5] + 0x24u); const bool less = F[2] < F[0]; G[5] += 0x10u; if (less) { hit = true; break; } } if (hit) { G[3] = G[2] << 1; G[7] = G[2] - 1u; } else { G[2] = G[6] - 1u; G[3] = G[2] << 1; G[7] = G[6] - 2u; } } } G[2] = G[7] << 4; G[5] = G[3] << 3; G[2] += G[4]; G[3] = G[5] + G[4]; F[12] = float_at(rt, G[2] + 4u); F[1] = float_at(rt, G[3] + 4u); F[0] = 0.0f; F[8] = F[1] - F[12]; G[2] = G[7] << 1; if (F[8] == F[0]) { G[1] = 0x3F800000u; F[8] = 1.0f; } G[2] <<= 3; G[2] += G[4]; F[7] = float_at(rt, G[2] + 8u); F[3] = float_at(rt, G[3] + 8u); F[0] = F[2] - F[12]; F[6] = float_at(rt, G[2] + 0x10u); F[3] = F[3] - F[7]; F[2] = float_at(rt, G[3] + 0xCu); F[0] = F[0] / F[8]; G[1] = 0xC0000000u; F[5] = -2.0f; F[3] = F[3] / F[8]; F[1] = F[6] * F[5]; F[4] = F[6] + F[2]; F[1] = F[1] - F[2]; G[1] = 0x40400000u; F[2] = 3.0f; F[5] = F[3] * F[5]; F[3] = F[3] * F[2]; F[4] = F[4] + F[5]; F[1] = F[1] + F[3]; F[4] = F[0] * F[4]; F[1] = F[1] + F[4]; F[1] = F[0] * F[1]; F[6] = F[6] + F[1]; F[0] = F[0] * F[6]; F[0] = F[8] * F[0]; F[0] = F[7] + F[0]; } }
void func_089382B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[24] = 9u; G[25] = 0u; G[21] = 0x21u; G[20] = 1u; G[19] = G[6]; G[18] = G[5]; G[2] = word(rt, G[4] + 0x10u); G[16] = word(rt, G[4] + 0x18u); G[17] = word(rt, G[2] + 4u); bool found = false; while (true) { G[2] = G[20] << (G[24] & 31u); G[2] -= 1u; G[2] &= G[18]; G[4] = 0u; G[3] = 0u; G[10] = G[2]; G[11] = G[24]; G[15] = G[24]; G[13] = G[17]; G[14] = 0xFFFFFFFFu; while (true) { G[2] = G[14] + G[13]; G[12] = sra(G[2], 1); G[7] = G[12] << 3; G[7] += G[16]; G[3] = load8(G[7]); G[4] = word(rt, G[7] + 4u); G[8] = G[4]; G[6] = G[3]; G[9] = G[6]; G[2] = G[6] < G[11] ? 1u : 0u; G[3] = load8(G[7] + 1u); if (G[8] == G[10] && G[6] == G[11]) { store8(G[19], G[3]); found = true; break; } if (G[2] != 0u || (G[9] == G[11] && G[8] < G[10])) G[14] = G[12]; else G[13] = G[12]; G[2] = G[14] + 1u; if (G[13] == G[2]) break; } if (found) break; G[4] = G[24] + 1u; G[2] = G[4] < G[24] ? 1u : 0u; G[5] = G[2] + G[25]; G[24] = G[4]; G[25] = G[5]; if (G[21] == G[4] && G[5] == 0u) { G[15] = 0u; break; } } G[2] = G[15]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_0893923C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[24] = 9u; G[25] = 0u; G[21] = 0x21u; G[20] = 1u; G[19] = G[6]; G[18] = G[5]; G[2] = word(rt, G[4] + 0xCu); G[16] = word(rt, G[4] + 0x14u); G[17] = word(rt, G[2] + 4u); bool found = false; while (true) { G[2] = G[20] << (G[24] & 31u); G[2] -= 1u; G[2] &= G[18]; G[4] = 0u; G[3] = 0u; G[10] = G[2]; G[11] = G[24]; G[15] = G[24]; G[13] = G[17]; G[14] = 0xFFFFFFFFu; while (true) { G[2] = G[14] + G[13]; G[12] = sra(G[2], 1); G[7] = G[12] << 3; G[7] += G[16]; G[3] = load8(G[7]); G[4] = word(rt, G[7] + 4u); G[8] = G[4]; G[6] = G[3]; G[9] = G[6]; G[2] = G[6] < G[11] ? 1u : 0u; G[3] = load8(G[7] + 1u); if (G[8] == G[10] && G[6] == G[11]) { store8(G[19], G[3]); found = true; break; } if (G[2] != 0u || (G[9] == G[11] && G[8] < G[10])) G[14] = G[12]; else G[13] = G[12]; G[2] = G[14] + 1u; if (G[13] == G[2]) break; } if (found) break; G[4] = G[24] + 1u; G[2] = G[4] < G[24] ? 1u : 0u; G[5] = G[2] + G[25]; G[24] = G[4]; G[25] = G[5]; if (G[21] == G[4] && G[5] == 0u) { G[15] = 0u; break; } } G[2] = G[15]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08939044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x120u); put(rt, frame.at(0x100u), G[16]); put(rt, frame.at(0x10Cu), G[19]); put(rt, frame.at(0x108u), G[18]); put(rt, frame.at(0x104u), G[17]); put(rt, frame.at(0x110u), G[31]); G[16] = frame.at(0x80u); G[4] = G[16]; G[19] = G[5]; G[18] = G[7]; G[17] = G[6]; call_here(rt, ctx, 0x08B0EC60u); G[16] = frame.at(0x80u); G[8] = frame.at(0x0u); G[6] = frame.at(0xB0u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[8], G[2]); G[16] += 0x10u; G[8] += 0x10u; put(rt, G[8] - 0xCu, G[3]); put(rt, G[8] - 8u, G[4]); put(rt, G[8] - 4u, G[5]); } while (G[16] != G[6]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); put(rt, G[8] + 8u, G[2]); put(rt, G[8], G[3]); put(rt, G[8] + 4u, G[4]); G[7] = 0u; G[3] = word(rt, frame.at(0x14u)); G[2] = G[7] < G[3] ? 1u : 0u; bool run = true; if (G[2] != 0u) run = false; else { G[16] = frame.at(0xC0u); if (G[3] == G[7]) { G[2] = word(rt, frame.at(0x10u)); G[2] = G[18] < G[2] ? 1u : 0u; if (G[2] != 0u) run = false; } } if (!run) G[17] = 0u; else { G[7] = G[18]; G[6] = G[17]; G[5] = G[19]; G[4] = G[16]; G[8] = 1u; call_here(rt, ctx, 0x08B10170u); G[6] = frame.at(0x40u); G[7] = frame.at(0xF0u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6], G[2]); G[16] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[16] != G[7]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); put(rt, G[6] + 8u, G[2]); put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[4]); } G[2] = G[17]; G[16] = word(rt, frame.at(0x100u)); G[19] = word(rt, frame.at(0x10Cu)); G[18] = word(rt, frame.at(0x108u)); G[17] = word(rt, frame.at(0x104u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_297
} // namespace gt::game

#define HOT297(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_297::name, gt::ReturnKind::kind)
// HOT297(0x088CFB14u, func_088CFB14, Float);  // no comparable synthetic case yet
HOT297(0x088D12ACu, func_088D12AC, Word);
// HOT297(0x088DE740u, func_088DE740, Word);  // no comparable synthetic case yet
// HOT297(0x088EAC34u, func_088EAC34, Word);  // no comparable synthetic case yet
HOT297(0x08907910u, func_08907910, Float);
HOT297(0x089382B0u, func_089382B0, Word);
HOT297(0x0893923Cu, func_0893923C, Word);
HOT297(0x08939044u, func_08939044, Word);
