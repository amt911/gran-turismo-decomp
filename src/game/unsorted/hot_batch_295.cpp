// Hot batch 295: seventy-six-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (a triple search, weight scatter, constructors, buffer growth, a tree copy and teardown).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_295 {
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

void func_08AFC230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[23] = G[4]; G[19] = G[6]; G[18] = G[7]; put(rt, frame.at(0u), G[5]); G[2] = 0u; bool found = false; if (static_cast<std::int32_t>(G[4]) > 0) { for (G[21] = 0u; static_cast<std::int32_t>(G[21]) < static_cast<std::int32_t>(G[23]) && !found; ) { if (G[21] == G[18] || G[21] == G[19]) { G[21] += 1u; continue; } G[30] = G[21] + 1u; for (G[17] = G[30]; static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[23]) && !found; ) { if (G[17] == G[18] || G[17] == G[19]) { G[17] += 1u; continue; } G[20] = G[17] + 1u; G[22] = G[23]; for (G[16] = G[20]; G[16] != G[22]; G[16] += 1u) { G[8] = G[16]; if (G[16] == G[18]) continue; G[4] = word(rt, frame.at(0u)); G[5] = G[19]; G[6] = G[21]; G[7] = G[17]; G[9] = G[18]; if (G[16] == G[19]) continue; call_here(rt, ctx, 0x08AFBFECu); if (G[2] != 0u) { found = true; break; } } G[17] = G[20]; } G[21] = G[30]; } } G[2] = found ? 1u : 0u; G[23] = word(rt, frame.at(0x2Cu)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[30] = word(rt, frame.at(0x30u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08B058A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[21] = G[5]; G[19] = G[4]; put_float(rt, frame.at(0x20u), F[20]); F[20] = F[12]; G[2] = word(rt, G[4] + 0x34u); G[18] = word(rt, G[4] + 0x24u); G[2] <<= 1; G[20] = G[18] + G[2]; G[17] = word(rt, G[4] + 0xCu); if (G[18] != G[20]) { G[22] = 0xFFFFFFFFu; do { G[2] = sext(load16(G[18]), 16); G[16] = word(rt, G[21]); F[12] = F[20]; G[3] = G[2] << 8; G[2] <<= 4; G[3] -= G[2]; G[16] += G[3]; G[5] = G[17]; G[6] = G[17] + 0xCu; G[4] = G[16]; G[18] += 2u; call_here(rt, ctx, 0x08AFEC54u); G[17] += 0x18u; store16(G[16] + 0xE4u, G[22]); } while (G[20] != G[18]); } G[2] = word(rt, G[19] + 0x38u); G[8] = word(rt, G[19] + 0x18u); G[2] <<= 2; G[9] = G[8] + G[2]; G[7] = word(rt, G[19]); while (G[8] != G[9]) { G[4] = sext(load16(G[8]), 16); G[5] = word(rt, G[21] + 0x60u); G[6] = sext(load16(G[8] + 2u), 16); G[3] = G[4] << 4; G[4] <<= 2; G[3] -= G[4]; G[2] = G[3] << 3; G[2] -= G[3]; G[5] += G[2]; G[5] += 0x34u; G[4] = G[8] + 2u; if (static_cast<std::int32_t>(G[6]) > 0) { G[3] = 0u; do { F[0] = float_at(rt, G[7] + 0x60u); G[3] += 1u; G[7] += 0x70u; put_float(rt, G[5], F[0]); G[2] = sext(load16(G[4]), 16); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] += 4u; } while (G[2] != 0u); } G[8] += 4u; } F[20] = float_at(rt, frame.at(0x20u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[22] = word(rt, frame.at(0x18u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } } }
void func_08B11BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; G[20] = G[7]; G[19] = G[5]; G[18] = G[6]; call_here(rt, ctx, 0x08B11830u); G[4] = G[16] + 0xA0u; call_here(rt, ctx, 0x08B0D800u); G[2] = 0x08CD7D60u; put(rt, G[16], G[2]); G[2] = 0x08CD7E40u; G[17] = 0xFFFFFFFFu; put(rt, G[16] + 0xA0u, G[2]); G[4] = G[16] + 0x1B8u; put(rt, G[16] + 0x1ACu, G[18]); put(rt, G[16] + 0x1B4u, G[17]); put(rt, G[16] + 0x1B0u, 0u); call_here(rt, ctx, 0x08B28DCCu); G[4] = G[16] + 0x1E0u; call_here(rt, ctx, 0x08B28DCCu); put(rt, G[16] + 0x260u, G[17]); G[4] = G[16] + 0x280u; put(rt, G[16] + 0x238u, G[17]); put(rt, G[16] + 0x25Cu, G[17]); put(rt, G[16] + 0x230u, 0u); put(rt, G[16] + 0x234u, 0u); put(rt, G[16] + 0x23Cu, 0u); put(rt, G[16] + 0x240u, 0u); put(rt, G[16] + 0x244u, 0u); put(rt, G[16] + 0x248u, 0u); put(rt, G[16] + 0x24Cu, 0u); put(rt, G[16] + 0x250u, 0u); put(rt, G[16] + 0x254u, 0u); store8(G[16] + 0x258u, 0u); put(rt, G[16] + 0x264u, 0u); put(rt, G[16] + 0x268u, 0u); put(rt, G[16] + 0x26Cu, 0u); put(rt, G[16] + 0x270u, 0u); put(rt, G[16] + 0x274u, 0u); put(rt, G[16] + 0x278u, 0u); store8(G[16] + 0x27Cu, 0u); call_here(rt, ctx, 0x08B2BD24u); G[5] = 0x08CB4B44u; G[4] = G[16] + 0x4D8u; put(rt, G[16] + 0x2D0u, 0u); put(rt, G[16] + 0x4D4u, 0u); call_here(rt, ctx, 0x08B2EDD4u); G[5] = G[19]; put(rt, G[16] + 0x10u, G[20]); G[4] = G[16] + 0xACu; G[6] = 0x80u; call_here(rt, ctx, 0x08B7C830u); G[4] = G[16] + 0x12Cu; G[5] = 0u; G[6] = 0x80u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x2D4u; G[5] = 0u; G[6] = 0x200u; G[16] = word(rt, frame.at(0x0u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } jump(ctx, 0x08B36018u); } }
void func_08B396D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); G[17] = G[4]; G[18] = G[5]; G[16] = G[6]; call_here(rt, ctx, 0x08B3A1C4u); G[2] = word(rt, G[17] + 4u); if (G[2] != 0u) { G[3] = word(rt, G[16]); G[2] = 1u; if (G[3] == G[2]) { G[4] = G[18]; call_here(rt, ctx, 0x08B39148u); G[19] = frame.at(8u); put(rt, G[16], G[2]); G[5] = G[18]; G[6] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08B396D4u); G[2] = word(rt, frame.at(0xCu)); if (G[2] != 0u) { G[4] = 0x24u; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[5] = 2u; G[6] = G[17]; G[7] = G[19]; G[16] = G[2]; call_here(rt, ctx, 0x08B38C78u); G[2] = 0x08CDA8F8u; put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[16]); if (G[16] != 0u) { G[2] = word(rt, G[16] + 4u); G[2] += 1u; put(rt, G[16] + 4u, G[2]); } G[2] = 0x08CDA908u; put(rt, frame.at(0u), G[2]); if (G[17] != frame.at(0u)) { G[4] = word(rt, G[17] + 4u); G[3] = word(rt, frame.at(4u)); if (G[4] != 0u) { G[2] = word(rt, G[4] + 4u); G[2] -= 1u; put(rt, G[4] + 4u, G[2]); if (G[2] == 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[3] = word(rt, frame.at(4u)); } put(rt, G[17] + 4u, G[3]); if (G[3] != 0u) { G[2] = word(rt, G[3] + 4u); G[2] += 1u; put(rt, G[3] + 4u, G[2]); } } G[4] = frame.at(0u); call_here(rt, ctx, 0x08C50CCCu); } G[4] = G[19]; call_here(rt, ctx, 0x08C50CCCu); } } G[2] = G[17]; G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); }  } }
void func_08B77524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[16] = G[6]; G[5] = 2u; G[2] = 0xFFFFFFF7u; bool fail = false; if (G[4] == 1u) { call_here(rt, ctx, 0x08C7A918u); G[4] = G[2]; } else if (G[4] == 2u) { call_here(rt, ctx, 0x08C7A920u); G[4] = G[2]; } else if (static_cast<std::int32_t>(G[4]) < 2 && G[4] == 0u) { call_here(rt, ctx, 0x08C7A910u); G[4] = G[2]; } if (static_cast<std::int32_t>(G[4]) < 0) { G[2] = 0xFFFFFFF7u; fail = true; } if (!fail) { G[17] = G[16]; if (G[4] == 1u) call_here(rt, ctx, 0x08C7A918u); else if (G[4] == 2u) call_here(rt, ctx, 0x08C7A920u); else if (G[4] == 0u) call_here(rt, ctx, 0x08C7A910u); else G[2] = G[4]; G[6] = G[18]; G[7] = sra(G[18], 31); G[4] = G[2]; G[8] = G[17]; call_here(rt, ctx, 0x08C7A880u); } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); }  } }
void func_08B7EE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = G[4] + 0x40u; G[3] = word(rt, G[4] + 0x30u); G[19] = G[4]; if (G[3] != G[2]) { G[18] = word(rt, G[4] + 0x34u); call_here(rt, ctx, 0x08B796C4u); G[5] = word(rt, G[19] + 0x30u); G[20] = G[18] << 1; G[4] = G[2]; G[6] = G[20]; call_here(rt, ctx, 0x08B35EB8u); G[17] = G[2]; if (G[2] == 0u) G[2] = 0xFFFFFFFFu; else { G[16] = G[2] + G[18]; G[6] = G[18]; G[4] = G[16]; G[5] = G[2]; call_here(rt, ctx, 0x08B79A88u); put(rt, G[19] + 0x34u, G[20]); G[2] = 0u; put(rt, G[19], G[16]); put(rt, G[19] + 0x30u, G[17]); } } else { call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = 0x400u; call_here(rt, ctx, 0x08B35DACu); G[3] = G[2]; if (G[2] == 0u) G[2] = 0xFFFFFFFFu; else { G[2] = 0x400u; put(rt, G[19] + 0x34u, G[2]); G[6] = G[3] + 0x3FDu; G[4] = G[19]; put(rt, G[19] + 0x30u, G[3]); G[5] = 2u; G[7] = 0xFFFFFFFFu; do { G[3] = load8(G[4] + 0x42u); G[2] = G[6] + G[5]; G[5] -= 1u; store8(G[2], G[3]); G[4] -= 1u; } while (G[5] != G[7]); put(rt, G[19], G[6]); G[2] = 0u; } } G[19] = word(rt, frame.at(0xCu)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08BA8C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC1918u; G[17] = G[4] + 0x28u; G[18] = G[4]; put(rt, G[4], G[2]); if (G[17] != 0u) { G[16] = G[4] + 0x4Cu; if (G[16] != G[17]) { G[19] = frame.at(1u); G[16] -= 0xCu; while (true) { G[4] = word(rt, G[16]); G[5] = word(rt, G[16] + 4u); G[6] = G[19]; call_here(rt, ctx, 0x08BA76B8u); G[2] = word(rt, G[16]); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08B35254u); if (G[16] == G[17]) break; G[16] -= 0xCu; } } } G[16] = word(rt, G[18] + 0x1Cu); G[2] = 0x08CC18E0u; put(rt, G[18], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[18] + 8u); G[5] = word(rt, G[18] + 0xCu); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8E5D8u); G[4] = word(rt, G[18] + 8u); if (G[4] != 0u) call_here(rt, ctx, 0x08B35254u); G[2] = 0x08CBF518u; put(rt, G[18], G[2]); G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08C2605C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x8u), G[18]); G[20] = G[4]; G[16] = G[5]; G[17] = G[6]; G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[19] = G[2]; G[4] = G[2] + 0x10u; G[5] = G[16] + 0x10u; if (G[4] != 0u) { call_here(rt, ctx, 0x08A6972Cu); G[4] = G[19] + 0x14u; G[5] = G[16] + 0x14u; call_here(rt, ctx, 0x08A76ABCu); } put(rt, G[19] + 0xCu, 0u); G[2] = word(rt, G[16]); G[5] = word(rt, G[16] + 0xCu); put(rt, G[19] + 4u, G[17]); put(rt, G[19], G[2]); put(rt, G[19] + 8u, 0u); if (G[5] != 0u) { G[4] = G[20]; G[6] = G[19]; call_here(rt, ctx, 0x08C2605Cu); put(rt, G[19] + 0xCu, G[2]); } G[17] = word(rt, G[16] + 8u); G[18] = G[19]; while (G[17] != 0u) { G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[4] = G[2] + 0x10u; G[5] = G[17] + 0x10u; if (G[4] != 0u) { call_here(rt, ctx, 0x08A6972Cu); G[4] = G[16] + 0x14u; G[5] = G[17] + 0x14u; call_here(rt, ctx, 0x08A76ABCu); } put(rt, G[16] + 0xCu, 0u); G[4] = G[20]; G[6] = G[16]; G[2] = word(rt, G[17]); G[3] = word(rt, G[17] + 0xCu); put(rt, G[16] + 4u, G[18]); G[5] = G[3]; put(rt, G[16] + 8u, 0u); put(rt, G[16], G[2]); put(rt, G[18] + 8u, G[16]); G[18] = G[16]; if (G[3] != 0u) { call_here(rt, ctx, 0x08C2605Cu); put(rt, G[16] + 0xCu, G[2]); } G[2] = word(rt, G[17] + 8u); G[17] = G[2]; } G[2] = G[19]; G[20] = word(rt, frame.at(0x10u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_295
} // namespace gt::game

#define HOT295(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_295::name, gt::ReturnKind::kind)
HOT295(0x08AFC230u, func_08AFC230, Word);
HOT295(0x08B058A0u, func_08B058A0, Word);
HOT295(0x08B11BF0u, func_08B11BF0, Word);
HOT295(0x08B396D4u, func_08B396D4, Word);
HOT295(0x08B77524u, func_08B77524, Word);
HOT295(0x08B7EE5Cu, func_08B7EE5C, Word);
// HOT295(0x08BA8C48u, func_08BA8C48, Word);  // no comparable synthetic case yet
// HOT295(0x08C2605Cu, func_08C2605C, Word);  // no comparable synthetic case yet
