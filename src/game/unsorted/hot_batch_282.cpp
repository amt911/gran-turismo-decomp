// Hot batch 282: seventy-two-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a shared-count release loop, table destructors, a slot scan and stream defaults).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_282 {
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

void func_08AC0F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { int tail = 0; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); G[21] = G[4]; G[4] = 0u; G[20] = G[9] & 0xFFu; G[19] = G[7]; G[18] = G[8]; G[16] = G[6]; call_here(rt, ctx, 0x08B0EA48u); G[17] = G[2]; G[6] = G[2] + 0xF8u; G[5] = G[16]; G[4] = G[2]; if (G[2] != 0u) { store8(G[6] + 0x30u, 0u); G[2] = word(rt, G[19]); G[3] = word(rt, G[19] + 4u); store8(G[6] + 0x14u, G[20]); put(rt, G[6] + 0x28u, G[2]); G[2] = 1u; put(rt, G[6] + 0x2Cu, G[3]); store8(G[17] + 0x68u, G[2]); put(rt, G[17] + 0x7Cu, 0u); G[7] = word(rt, G[18] + 0xCu); G[2] = word(rt, G[18]); G[3] = word(rt, G[18] + 4u); G[6] = word(rt, G[18] + 8u); put(rt, G[17] + 0x6Cu, G[2]); put(rt, G[17] + 0x70u, G[3]); put(rt, G[17] + 0x74u, G[6]); put(rt, G[17] + 0x78u, G[7]); call_here(rt, ctx, 0x08B0DE84u); G[4] = G[21]; G[5] = G[17]; tail = 1; } else { G[25] = word(rt, G[18] + 8u); G[5] = 0u; G[2] = G[18] + 8u; if (G[25] != 0u) { G[4] = word(rt, G[2] + 4u); tail = 2; } } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail == 1) jump(ctx, 0x08B0E560u); else if (tail == 2) jump(ctx, G[25]); } }
void func_08AE3708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x4u), G[17]); G[3] = word(rt, G[4] + 0x6Cu); G[16] = G[4]; if (static_cast<std::int32_t>(G[3]) < 0) { G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CDFB3Cu); G[2] &= 1u; if (G[2] != 0u) { G[4] = word(rt, 0x08CDFB40u); G[5] = 0x08C9145Cu; G[7] = 0x08C91508u; G[6] = 0x275u; call_here(rt, ctx, 0x08AD83B8u); } G[4] = 1u; } else { G[2] = sext(load8(G[4] + 0x5Au), 8); if (G[2] != 0u) { G[2] = G[3] - 1u; put(rt, G[16] + 0x6Cu, G[2]); G[4] = 1u; } else { G[2] = word(rt, G[4] + 0x10u); G[17] = word(rt, G[2] + 0x10u); G[4] = sext(load16(G[17] + 0xEAu), 16); G[5] = G[3] + G[16]; bool slot = true; if (static_cast<std::int32_t>(G[4]) >= 0) { call_here(rt, ctx, 0x08AE1D20u); G[3] = word(rt, G[17] + 0xECu); G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] = 0u; if (G[3] == 0u) slot = false; else { F[2] = float_at(rt, G[17] + 0xF0u); F[0] = float_at(rt, 0x08CB6124u); G[3] = word(rt, G[16] + 0x4Cu); G[4] = word(rt, G[16] + 0x10u); F[2] = F[2] * F[0]; G[2] = static_cast<std::uint32_t>(trunc_w_s(F[2])); F[1] = std::bit_cast<float>(G[2]); G[3] -= G[2]; put(rt, G[16] + 0x4Cu, G[3]); G[2] = word(rt, G[4] + 0xCu); G[2] |= 0x100u; put(rt, G[4] + 0xCu, G[2]); G[3] = word(rt, G[16] + 0x6Cu); G[5] = G[3] + G[16]; } } if (slot) { G[2] = load8(G[5] + 0x70u); G[4] = sext(G[2], 8); G[2] -= 1u; bool take = static_cast<std::int32_t>(G[4]) < 0; if (!take) { store8(G[5] + 0x70u, G[2]); take = static_cast<std::int32_t>(G[4]) > 0; } if (take) { G[2] = G[3] << 2; G[2] += G[16]; G[3] = word(rt, G[2] + 0x74u); G[4] = 1u; put(rt, G[16] + 0x48u, G[3]); } else { G[2] = G[3] - 1u; put(rt, G[16] + 0x6Cu, G[2]); G[4] = 1u; } } } } G[2] = G[4]; G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); }  } } }
void func_08B41834(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[2] = G[8] < 0x18u ? 1u : 0u; G[17] = G[4]; G[16] = G[7]; if (G[2] == 0u) { G[3] = word(rt, G[7] + 0xCu); G[2] = G[8] - 0x18u; if (G[3] == G[2]) { G[4] = word(rt, G[5] + 0x10u); G[5] = 0x18u; call_here(rt, ctx, 0x08B47B54u); G[4] = G[17] + 0x454u; G[8] = word(rt, G[4] + 0x480u); G[5] = G[2]; G[9] = load16(G[16] + 8u); if (G[8] != 0u) { G[3] = G[4]; G[6] = G[4]; G[7] = 0u; bool found = false; do { G[2] = load8(G[3] + 0x400u); G[3] += 1u; if (G[2] != 0u) { G[2] = word(rt, G[6] + 0x200u); if (G[9] == G[2]) { G[2] = G[7] << 2; found = static_cast<std::int32_t>(G[7]) >= 0; break; } } G[7] += 1u; G[6] += 4u; } while (G[8] != G[7]); if (found) { G[2] += G[4]; G[2] = word(rt, G[2]); if (G[2] != 0u) { G[4] = word(rt, G[2]); if (G[4] != 0u) { G[2] = load8(G[4] + 0x15u); G[6] = word(rt, G[16] + 0x10u); if (G[2] == 0u) { G[7] = word(rt, G[16] + 0x14u); tail = true; } else { G[7] = word(rt, G[16] + 0x14u); G[3] = word(rt, G[4] + 0x1Cu); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; if (G[2] != 0u) tail = true; else if (G[3] == G[7]) { G[2] = word(rt, G[4] + 0x18u); G[2] = G[2] < G[6] ? 1u : 0u; if (G[2] != 0u) tail = true; } } } } } } } } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x08B3E670u); } }
void func_08B60910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[31]); G[16] = G[4]; G[4] = word(rt, G[4] + 8u); if (G[4] == 0u) { G[4] = 0x44u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CDBAB8u; G[4] = G[2]; store8(G[2] + 0x11u, 0u); store8(G[2] + 0x12u, 0u); put(rt, G[2] + 0x30u, 0u); put(rt, G[2] + 0x34u, 0u); put(rt, G[2], G[3]); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, 0u); store8(G[2] + 0x10u, 0u); put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x24u, 0u); put(rt, G[2] + 0x28u, 0u); put(rt, G[2] + 0x2Cu, 0u); store8(G[2] + 0x43u, 0u); put(rt, G[16] + 8u, G[2]); } G[2] = 0x2Eu; G[3] = 0x08C90000u; store8(G[4] + 0x11u, G[2]); G[7] = 0x08C95FF4u; G[2] = 0x2Cu; G[4] = word(rt, G[16] + 8u); const std::uint32_t text = load8(G[7]) | (load8(G[7] + 1u) << 8) | (load8(G[7] + 2u) << 16) | (load8(G[7] + 3u) << 24); G[6] = 0x08C95DF8u; store8(G[4] + 0x12u, G[2]); G[9] = 0x08CE0000u; G[2] = word(rt, G[16] + 8u); G[5] = text; G[4] = text; G[8] = 0xBu; put(rt, G[2] + 0x30u, G[5]); G[7] = 0u; G[5] = word(rt, G[16] + 8u); put(rt, G[2] + 0x24u, G[6]); put(rt, G[5] + 0x34u, G[4]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, 0u); put(rt, G[2] + 0x14u, G[6]); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, G[6]); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x28u, 0u); put(rt, G[2] + 0x2Cu, 0u); do { G[2] = word(rt, 0x08CE0E18u); G[3] = word(rt, G[16] + 8u); G[2] += G[7]; G[4] = load8(G[2]); G[3] = G[7] + G[3]; G[7] += 1u; store8(G[3] + 0x38u, G[4]); } while (G[7] != G[8]); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B60C98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[31]); G[16] = G[4]; G[4] = word(rt, G[4] + 8u); if (G[4] == 0u) { G[4] = 0x44u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CDBAC8u; G[4] = G[2]; store8(G[2] + 0x11u, 0u); store8(G[2] + 0x12u, 0u); put(rt, G[2] + 0x30u, 0u); put(rt, G[2] + 0x34u, 0u); put(rt, G[2], G[3]); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, 0u); store8(G[2] + 0x10u, 0u); put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x24u, 0u); put(rt, G[2] + 0x28u, 0u); put(rt, G[2] + 0x2Cu, 0u); store8(G[2] + 0x43u, 0u); put(rt, G[16] + 8u, G[2]); } G[2] = 0x2Eu; G[3] = 0x08C90000u; store8(G[4] + 0x11u, G[2]); G[7] = 0x08C95FF4u; G[2] = 0x2Cu; G[4] = word(rt, G[16] + 8u); const std::uint32_t text = load8(G[7]) | (load8(G[7] + 1u) << 8) | (load8(G[7] + 2u) << 16) | (load8(G[7] + 3u) << 24); G[6] = 0x08C95DF8u; store8(G[4] + 0x12u, G[2]); G[9] = 0x08CE0000u; G[2] = word(rt, G[16] + 8u); G[5] = text; G[4] = text; G[8] = 0xBu; put(rt, G[2] + 0x30u, G[5]); G[7] = 0u; G[5] = word(rt, G[16] + 8u); put(rt, G[2] + 0x24u, G[6]); put(rt, G[5] + 0x34u, G[4]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, 0u); put(rt, G[2] + 0x14u, G[6]); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, G[6]); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x28u, 0u); put(rt, G[2] + 0x2Cu, 0u); do { G[2] = word(rt, 0x08CE0E18u); G[3] = word(rt, G[16] + 8u); G[2] += G[7]; G[4] = load8(G[2]); G[3] = G[7] + G[3]; G[7] += 1u; store8(G[3] + 0x38u, G[4]); } while (G[7] != G[8]); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B8F680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[19] = G[5]; G[17] = G[4]; if (G[5] != G[4]) { G[18] = 1u; do { G[16] = word(rt, G[17] + 0xCu); G[3] = G[16] + 4u; if (G[16] != 0u) { G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[18]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[18]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[16] = word(rt, G[17] + 0x4u); G[3] = G[16] + 4u; if (G[16] != 0u) { G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[18]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[18]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[17] += 0x10u; } while (G[17] != G[19]); } G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08BDD140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5EE0u; G[21] = G[4] + 0x148u; G[18] = G[4]; put(rt, G[4], G[2]); if (G[21] != 0u && G[21] != G[4] + 0x1CCu) { G[16] = G[18] + 0x1CCu; G[17] = G[18] + 0x1A0u; G[19] = 3u; do { G[2] = word(rt, G[17]); G[16] -= 0x2Cu; G[19] -= 1u; G[3] = word(rt, G[2]); G[4] = G[16]; G[17] -= 0x2Cu; call_here(rt, ctx, G[3]); } while (G[19] != 0u); } G[20] = G[18] + 0xC4u; G[2] = G[18] + 0x40u; if (G[20] != 0u && G[20] != G[21]) { G[16] = G[21]; G[17] = G[18] + 0x11Cu; G[19] = 3u; do { G[2] = word(rt, G[17]); G[16] -= 0x2Cu; G[19] -= 1u; G[3] = word(rt, G[2]); G[4] = G[16]; G[17] -= 0x2Cu; call_here(rt, ctx, G[3]); } while (G[19] != 0u); } G[2] = G[18] + 0x40u; if (G[2] != 0u && G[2] != G[20]) { G[16] = G[20]; G[17] = G[18] + 0x98u; G[19] = 3u; do { G[2] = word(rt, G[17]); G[16] -= 0x2Cu; G[19] -= 1u; G[3] = word(rt, G[2]); G[4] = G[16]; G[17] -= 0x2Cu; call_here(rt, ctx, G[3]); } while (G[19] != 0u); } G[4] = G[18]; call_here(rt, ctx, 0x08925F98u); G[4] = G[18]; G[21] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x8u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); } }
void func_08BE9708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = G[5]; G[5] = 4u; G[19] = G[4]; G[4] = 0x20u; G[18] = frame.at(4u); G[17] = frame.at(0xCu); put_float(rt, frame.at(0x3Cu), F[23]); put_float(rt, frame.at(0x38u), F[22]); put_float(rt, frame.at(0x34u), F[21]); put_float(rt, frame.at(0x30u), F[20]); F[20] = float_at(rt, G[2] + 0xCu); F[21] = float_at(rt, G[2]); F[22] = float_at(rt, G[2] + 4u); F[23] = float_at(rt, G[2] + 8u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A5B828u); G[3] = G[16] + 0x10u; G[2] = 0x08CCDDB0u; put(rt, G[16], G[2]); G[4] = G[18]; G[5] = frame.at(0u); put_float(rt, G[3] + 0xCu, F[20]); put_float(rt, G[3] + 4u, F[22]); put_float(rt, G[3] + 8u, F[23]); put_float(rt, G[16] + 0x10u, F[21]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A5BAB8u); G[5] = 4u; G[4] = 0x18u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB6ED0u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[18]; call_here(rt, ctx, 0x08A9B764u); G[5] = frame.at(8u); put(rt, G[16] + 0xCu, 0u); G[4] = G[17]; put(rt, frame.at(8u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[19]; F[23] = float_at(rt, frame.at(0x3Cu)); F[22] = float_at(rt, frame.at(0x38u)); F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_282
} // namespace gt::game

#define HOT282(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_282::name, gt::ReturnKind::kind)
// HOT282(0x08AC0F08u, func_08AC0F08, Word);  // no comparable synthetic case yet
HOT282(0x08AE3708u, func_08AE3708, Word);
HOT282(0x08B41834u, func_08B41834, Word);
HOT282(0x08B60910u, func_08B60910, Word);
HOT282(0x08B60C98u, func_08B60C98, Word);
HOT282(0x08B8F680u, func_08B8F680, Word);
// HOT282(0x08BDD140u, func_08BDD140, Word);  // no comparable synthetic case yet
HOT282(0x08BE9708u, func_08BE9708, Word);
