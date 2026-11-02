// Hot batch 281: seventy-two-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a guarded read through a cache, a vector insert, a bounding box of four rotated corners).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_281 {
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

void func_08938984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); G[22] = 0u; G[18] = G[6]; G[17] = G[4]; G[3] = word(rt, G[4] + 4u); G[4] = word(rt, G[3] + 8u); G[2] = G[5] < G[4] ? 1u : 0u; G[16] = G[5]; if (G[2] != 0u) { G[22] = word(rt, G[3] + 0xCu); G[19] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[2] != 0u) { G[2] = G[5] << 3; G[2] += G[3]; G[19] = word(rt, G[2] + 0x10u); } } G[21] = G[17] + 0x3Cu; G[4] = G[21]; G[20] = G[17] + 0x28u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[16]; G[7] = G[18]; call_here(rt, ctx, 0x0893A61Cu); const bool cached = G[2] != 0u; G[2] = G[16] << 3; if (!cached) { G[5] = word(rt, G[17] + 4u); G[6] = word(rt, G[17] + 0x24u); G[2] += G[5]; G[3] = load16(G[5] + 4u); G[4] = word(rt, G[2] + 0x14u); G[3] &= 1u; G[5] = G[6] + G[4]; if (G[3] != 0u) { G[4] = G[17]; G[6] = G[18]; call_here(rt, ctx, 0x08938940u); } else { G[4] = G[18]; G[6] = G[22]; call_here(rt, ctx, 0x08B79A88u); } G[4] = G[20]; G[5] = G[19]; G[6] = G[16]; G[7] = G[18]; call_here(rt, ctx, 0x0893A4ACu); } G[4] = G[21]; call_here(rt, ctx, 0x08B3457Cu); } G[2] = G[22]; G[22] = word(rt, frame.at(0x18u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); }  } }
void func_089A2074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x38u), G[31]); G[17] = G[4]; G[16] = G[5]; bool in_range = false; if (static_cast<std::int32_t>(G[5]) >= 0) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; in_range = G[2] != 0u; } if (in_range) { G[5] = word(rt, G[17] + 0xA8u); G[3] = G[16] << 5; G[2] = G[16] << 3; G[2] += G[3]; G[5] += G[2]; G[6] = G[5]; G[4] = G[17] + 0xA8u; G[5] += 0x28u; call_here(rt, ctx, 0x08BF5450u); } else { G[16] = G[17] + 0xA8u; G[4] = frame.at(0u); call_here(rt, ctx, 0x089D76A8u); G[5] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[17] = 0x08CD0000u; G[4] = 0x08CCE6E0u; put(rt, frame.at(0u), G[4]); put(rt, frame.at(0x20u), 0u); put(rt, frame.at(0x24u), 0u); if (G[5] != G[2]) { F[0] = float_at(rt, frame.at(0xCu)); if (G[5] != 0u) { F[1] = float_at(rt, frame.at(0x10u)); G[2] = word(rt, frame.at(4u)); put_float(rt, G[5] + 0xCu, F[0]); F[0] = float_at(rt, frame.at(0x14u)); put_float(rt, G[5] + 0x10u, F[1]); put_float(rt, G[5] + 0x14u, F[0]); F[1] = float_at(rt, frame.at(0x18u)); F[0] = float_at(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(8u)); put(rt, G[5] + 4u, G[2]); put(rt, G[5] + 8u, G[3]); put_float(rt, G[5] + 0x18u, F[1]); put_float(rt, G[5] + 0x1Cu, F[0]); F[0] = float_at(rt, frame.at(0x24u)); F[1] = float_at(rt, frame.at(0x20u)); put(rt, G[5], G[4]); put_float(rt, G[5] + 0x24u, F[0]); put_float(rt, G[5] + 0x20u, F[1]); } G[5] = word(rt, G[16] + 4u); G[5] += 0x28u; put(rt, G[16] + 4u, G[5]); } else { G[4] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF5120u); } G[2] = 0x08CCE6E0u; G[4] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x089D6684u); } G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } } }
void func_089A91D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { int tail = 0; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[31]); G[2] = 1u; if (G[4] == G[2]) { G[2] = 0xFFFFu; G[4] = 0x08CF4ACCu; if (G[5] == G[2]) { G[4] = 0x08CF4ACCu; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF4AA4u; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF4A7Cu; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF4A54u; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF4A2Cu; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF4A04u; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF49DCu; call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF49C0u; G[5] = 0x08CA5070u; G[7] = 0x089A88C4u; G[8] = 0x089B2E88u; G[9] = 0x089A88E8u; G[6] = 0x180u; tail = 1; } } else if (G[4] == 0u) { G[2] = 0xFFFFu; G[4] = 0x08CF49DCu; if (G[5] == G[2]) { G[4] = 0x08CF49DCu; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4A04u; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4A2Cu; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4A54u; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4A7Cu; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4AA4u; call_here(rt, ctx, 0x08B28DC4u); G[4] = 0x08CF4ACCu; tail = 2; } }  } if (tail == 1) jump(ctx, 0x08A92114u); else if (tail == 2) jump(ctx, 0x08B28DC4u); } }
void func_08A153C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { GuestFrame frame(ctx, 0x20u); G[2] = word(rt, G[4]); G[1] = 0x3F800000u; F[10] = 1.0f; F[9] = 0.0f; if (G[2] == 0u) { F[0] = float_at(rt, G[4] + 8u); F[10] = float_at(rt, G[4] + 4u); F[9] = -F[0]; } F[7] = float_at(rt, G[5]); F[3] = float_at(rt, G[7]); F[8] = float_at(rt, G[6]); F[4] = float_at(rt, G[8]); F[0] = F[7] + F[3]; G[1] = 0x3F000000u; F[1] = 0.5f; F[2] = F[8] + F[4]; G[1] = 0x497423F0u; F[6] = std::bit_cast<float>(G[1]); F[0] = F[0] * F[1]; G[1] = 0xC97423F0u; F[5] = std::bit_cast<float>(G[1]); F[2] = F[2] * F[1]; put_float(rt, G[5], F[6]); G[2] = frame.at(0u); G[3] = frame.at(0x20u); put_float(rt, G[7], F[5]); put_float(rt, frame.at(8u), F[3]); put_float(rt, G[6], F[6]); put_float(rt, frame.at(0x10u), F[0]); put_float(rt, frame.at(0x14u), F[4]); put_float(rt, frame.at(0x18u), F[7]); put_float(rt, frame.at(0x1Cu), F[2]); put_float(rt, frame.at(0u), F[0]); put_float(rt, G[8], F[5]); put_float(rt, frame.at(4u), F[8]); put_float(rt, frame.at(0xCu), F[2]); do { F[1] = float_at(rt, G[2]); F[0] = float_at(rt, G[2] + 4u); F[4] = float_at(rt, G[5]); F[3] = F[10] * F[1]; F[2] = F[9] * F[0]; F[1] = F[9] * F[1]; F[0] = F[10] * F[0]; G[2] += 8u; F[3] = F[3] - F[2]; const bool below = F[3] < F[4]; F[1] = F[1] + F[0]; if (below) put_float(rt, G[5], F[3]); F[0] = float_at(rt, G[7]); if (F[0] < F[3]) put_float(rt, G[7], F[3]); F[0] = float_at(rt, G[6]); if (F[1] < F[0]) put_float(rt, G[6], F[1]); F[0] = float_at(rt, G[8]); if (F[0] < F[1]) put_float(rt, G[8], F[1]); } while (G[3] != G[2]); } } }
void func_08A5B2D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[20] = G[5]; G[2] = word(rt, G[4] + 0x3Cu); G[18] = G[4]; if (G[2] != 0u) { G[19] = 0u; G[22] = G[4] + 0x38u; G[21] = G[5] + 0x18u; G[30] = 0x08CB0000u; G[23] = 0x08CB0000u; do { G[16] = word(rt, G[18] + 0x38u); G[4] = 0x08CAFB04u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x120u); G[4] = G[16]; call_here(rt, ctx, G[2]); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[19])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); G[2] = word(rt, G[20] + 0x1Cu); G[19] += 1u; G[16] = ctx.lo; G[7] = G[2] + G[16]; if (G[2] == 0u) { G[17] = word(rt, G[20] + 0x18u); G[2] = 0x08CB0000u; G[4] = 0x08CAFB0Cu; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17] + 0xCu); G[7] = G[2] + G[16]; } G[4] = frame.at(0u); G[5] = G[22]; G[6] = G[21]; call_here(rt, ctx, 0x08A57B54u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAFBB4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A56974u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, G[18] + 0x3Cu); G[2] = G[19] < G[2] ? 1u : 0u; } while (G[2] != 0u); } G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A75DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[17] = G[4]; G[18] = word(rt, G[4] + 4u); G[4] = word(rt, G[4]); G[2] = G[18] - G[4]; G[5] = sra(G[2], 2); G[3] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[2] = 0x08D70000u; if (G[3] == 0u) { G[21] = 0x08D74FD0u; G[2] = G[21] + 0xCu; G[19] = G[5] - 1u; put(rt, frame.at(8u), G[2]); G[20] = frame.at(8u); if (static_cast<std::int32_t>(G[19]) > 0) { G[16] = 0u; G[18] = 0x08CB0000u; while (true) { G[2] = G[16] << 2; G[2] = G[4] + G[2]; G[5] = word(rt, G[2]); G[4] = G[20]; G[16] += 1u; G[5] += 4u; call_here(rt, ctx, 0x08B89054u); G[4] = G[20]; G[5] = 0x08CB0DD0u; G[6] = 2u; if (G[19] == G[16]) break; if (static_cast<std::int32_t>(G[16]) > 0) call_here(rt, ctx, 0x08BEED1Cu); G[4] = word(rt, G[17]); } G[18] = word(rt, G[17] + 4u); } G[16] = frame.at(4u); G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08A69654u); G[5] = G[16]; G[4] = G[18] - 4u; call_here(rt, ctx, 0x08A6A23Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[4] = word(rt, frame.at(8u)); G[16] = G[4] - 0xCu; if (G[21] != G[16]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[17] = word(rt, frame.at(0x14u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A7CD30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[20] = frame.at(0xCu); G[4] = G[20]; G[18] = G[7]; G[16] = G[6]; call_here(rt, ctx, 0x08A7C3A4u); G[2] = 1u; if (G[16] == G[2]) { G[2] = 2u; G[17] = word(rt, frame.at(0xCu)); G[16] = frame.at(8u); if (G[17] == 0u) { G[4] = 0x08CB1360u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = G[18]; G[4] = G[16]; call_here(rt, ctx, 0x08A92548u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A7CCB8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 2u; G[19] = word(rt, frame.at(0xCu)); if (G[16] == G[2]) { if (G[19] == 0u) { G[4] = 0x08CB1360u; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[18]); if (G[17] == 0u) { G[4] = 0x08CB1350u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0x5Cu); G[4] = G[16]; call_here(rt, ctx, G[3]); G[4] = frame.at(0u); G[5] = G[18] + 4u; call_here(rt, ctx, 0x08A92548u); G[4] = G[19]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08A7CB20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A87558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); G[5] = 0x08CB194Cu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[3] = 0x08C8F4F4u; G[7] = word(rt, 0x08C8F4F4u); G[8] = word(rt, G[3] + 4u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[6] = frame.at(0x10u); G[5] = 0x08CF6FC0u; put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08C2A53Cu); G[2] = 0x08C8F4ECu; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8F4ECu); G[4] = G[18]; G[6] = frame.at(8u); G[5] = 0x08CF6F9Cu; put(rt, frame.at(8u), G[7]); put(rt, frame.at(0xCu), G[8]); call_here(rt, ctx, 0x08C2A53Cu); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_281
} // namespace gt::game

#define HOT281(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_281::name, gt::ReturnKind::kind)
HOT281(0x08938984u, func_08938984, Word);
HOT281(0x089A2074u, func_089A2074, Word);
HOT281(0x089A91D4u, func_089A91D4, Word);
HOT281(0x08A153C4u, func_08A153C4, Word);
HOT281(0x08A5B2D0u, func_08A5B2D0, Word);
HOT281(0x08A75DB8u, func_08A75DB8, Word);
HOT281(0x08A7CD30u, func_08A7CD30, Word);
HOT281(0x08A87558u, func_08A87558, Word);
