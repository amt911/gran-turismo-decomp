// Hot batch 285: seventy-three-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (renderer defaults, state marks, a three-part number comparison and stream defaults).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_285 {
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

void func_08A50B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); G[16] = G[4]; G[17] = 0x08CF0000u; put(rt, G[4] + 0x84u, 0u); G[4] = 0x8002u; put(rt, G[16] + 0x88u, 0u); put(rt, G[16] + 0x8Cu, 0u); store16(G[16] + 0x90u, 0u); call_here(rt, ctx, 0x08B58874u); G[4] = 2u; call_here(rt, ctx, 0x08B58874u); G[4] = 0x6Du; call_here(rt, ctx, 0x08B54FB8u); G[1] = 0x3F800000u; F[12] = 1.0f; G[4] = 0x44u; call_here(rt, ctx, 0x08B58718u); G[4] = 0x2u; G[5] = 0x8000u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x0u; G[5] = 0x1u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x1u; G[5] = 0x0u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x5u; G[5] = 0x1u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x6u; G[5] = 0x1u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x4u; G[5] = 0x1u; call_here(rt, ctx, 0x08B55AACu); G[4] = 0x3u; G[5] = 0x1u; call_here(rt, ctx, 0x08B55AACu); call_here(rt, ctx, 0x08B55B90u); G[2] = word(rt, 0x08CF68E8u); G[4] = frame.at(0u); G[5] = frame.at(4u); G[3] = word(rt, G[2] + 0x6Cu); G[6] = frame.at(8u); G[7] = frame.at(0xCu); if (G[3] != 0u) call_here(rt, ctx, 0x08B569A0u); else { call_here(rt, ctx, 0x08B551D0u); F[0] = float_at(rt, frame.at(8u)); G[2] = word(rt, 0x08CF68E8u); F[1] = static_cast<float>(std::bit_cast<std::int32_t>(F[0])); F[3] = float_at(rt, G[2] + 8u); F[0] = float_at(rt, frame.at(0xCu)); F[1] = F[1] / F[3]; F[2] = static_cast<float>(std::bit_cast<std::int32_t>(F[0])); put_float(rt, G[16] + 0x7Cu, F[1]); G[2] = word(rt, 0x08CF68E8u); F[0] = float_at(rt, G[2] + 0xCu); F[2] = F[2] / F[0]; put_float(rt, G[16] + 0x80u, F[2]); } G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } } }
void func_08ADF670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x0u), G[16]); F[0] = 0.0f; G[18] = G[5]; G[17] = G[4]; if (F[0] < F[12]) { G[5] = 0u; G[6] = 1u; call_here(rt, ctx, 0x08AE0EB0u); } else { G[4] = sext(load16(G[4] + 0x22u), 16); G[5] = 0u; if (static_cast<std::int32_t>(G[4]) >= 0) { G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08ADAFACu); } call_here(rt, ctx, 0x08AD97CCu); bool marks = G[2] != 0u; if (!marks) { call_here(rt, ctx, 0x08AEB954u); G[16] = G[2]; call_here(rt, ctx, 0x08AD9798u); if (G[16] != G[2]) marks = true; else { G[2] = sext(load16(G[17] + 0x22u), 16); G[4] = G[17]; if (static_cast<std::int32_t>(G[2]) >= 0) marks = true; else { G[5] = G[18]; call_here(rt, ctx, 0x08ADE0E4u); } } } if (marks) { G[16] = G[17] + 0x8Au; if (G[18] != 0u) { G[2] = sext(load8(G[16] + 1u), 8); G[2] &= 0x10u; if (G[2] == 0u) { call_here(rt, ctx, 0x08AEB808u); G[2] = load8(G[17] + 0x8Au); G[2] |= 0x10u; store8(G[17] + 0x8Au, G[2]); G[3] = load8(G[16] + 1u); G[3] |= 0x10u; store8(G[16] + 1u, G[3]); call_here(rt, ctx, 0x08AEB82Cu); } } G[2] = sext(load8(G[16] + 1u), 8); G[2] &= 2u; if (G[2] == 0u) { call_here(rt, ctx, 0x08AEB808u); G[2] = load8(G[17] + 0x8Au); G[2] |= 0x2u; store8(G[17] + 0x8Au, G[2]); G[3] = load8(G[16] + 1u); G[3] |= 0x2u; store8(G[16] + 1u, G[3]); call_here(rt, ctx, 0x08AEB82Cu); } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } } }
void func_08B42850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x550u); put(rt, frame.at(0x53Cu), G[19]); put(rt, frame.at(0x538u), G[18]); put(rt, frame.at(0x544u), G[31]); put(rt, frame.at(0x540u), G[20]); put(rt, frame.at(0x534u), G[17]); put(rt, frame.at(0x530u), G[16]); G[2] = 0x10u; G[6] = G[10]; G[19] = G[7]; G[7] = G[11]; G[18] = G[4]; G[4] += 0x8DCu; if (G[8] == G[2]) { call_here(rt, ctx, 0x08B3F098u); G[20] = G[2]; G[5] = 0u; G[4] = G[18]; if (G[2] != 0u) { G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x8Cu); call_here(rt, ctx, G[2]); G[4] = frame.at(0u); G[17] = G[3]; G[16] = G[2]; call_here(rt, ctx, 0x08B47AB8u); G[2] = 0x2712u; G[5] = 0x20u; G[4] = frame.at(0u); put(rt, frame.at(0x14u), G[2]); call_here(rt, ctx, 0x08B47AD4u); G[4] = frame.at(0u); G[5] = 0u; call_here(rt, ctx, 0x08B47B40u); put(rt, G[2] + 8u, 0u); G[4] = G[18]; G[5] = G[20]; put(rt, G[2] + 0xCu, 0u); G[6] = frame.at(0u); G[7] = 1u; put(rt, G[2] + 0x10u, 0u); put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2], 0u); put(rt, G[2] + 4u, 0u); G[10] = word(rt, G[19] + 8u); G[11] = word(rt, G[19] + 0xCu); put(rt, G[2] + 0x10u, G[16]); put(rt, G[2] + 0x14u, G[17]); put(rt, G[2] + 8u, G[10]); put(rt, G[2] + 0xCu, G[11]); G[12] = word(rt, G[19] + 8u); G[13] = word(rt, G[19] + 0xCu); G[8] = G[16] - G[12]; G[3] = G[16] < G[8] ? 1u : 0u; G[9] = G[17] - G[13]; G[9] -= G[3]; put(rt, G[2] + 0x18u, G[8]); put(rt, G[2] + 0x1Cu, G[9]); call_here(rt, ctx, 0x08B42618u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B47ACCu); } } G[19] = word(rt, frame.at(0x53Cu)); G[18] = word(rt, frame.at(0x538u)); G[20] = word(rt, frame.at(0x540u)); G[17] = word(rt, frame.at(0x534u)); G[16] = word(rt, frame.at(0x530u)); }  } }
void func_08B46C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[7] = 0x7A120u; G[22] = G[6]; G[21] = G[4] + 0x70u; G[19] = G[5] & 0xFFFFu; G[8] = 0x1Eu; G[18] = G[4]; G[9] = 5u; G[10] = 0u; G[5] = G[19]; G[6] = 0x2000u; G[2] = load8(G[4] + 0x8Cu); G[4] = G[21]; if (G[2] != 0u) { call_here(rt, ctx, 0x08C7AE58u); G[20] = G[2]; G[4] = G[18]; G[5] = 0x2400u; if (static_cast<std::int32_t>(G[2]) < 0) { G[2] = 0x8041070Au; G[4] = G[19]; G[5] = 0u; if (G[20] == G[2]) { call_here(rt, ctx, 0x08C7AE00u); } G[2] = 0u; } else { G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0xA0u); call_here(rt, ctx, G[2]); G[17] = G[2] + 4u; put(rt, G[2], G[20]); G[5] = G[21]; G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08B3F8C0u); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x08B3FA3Cu); G[2] = 1u; store8(G[16] + 0x30u, G[2]); G[4] = G[18]; G[5] = G[16]; store8(G[16] + 0x31u, G[2]); call_here(rt, ctx, 0x08B46904u); G[4] = G[22]; G[5] = G[17]; call_here(rt, ctx, 0x08B3F7CCu); G[2] = 1u; } } else G[2] = 0u; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[20] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B60DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[31]); G[16] = G[4]; G[4] = word(rt, G[4] + 8u); if (G[4] == 0u) { G[4] = 0x68u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CDBAD8u; G[4] = G[2]; store8(G[2] + 0x24u, 0u); store8(G[2] + 0x25u, 0u); put(rt, G[2], G[3]); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, 0u); store8(G[2] + 0x10u, 0u); put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2] + 0x20u, 0u); store8(G[2] + 0x64u, 0u); put(rt, G[16] + 8u, G[2]); } G[2] = 0x2Eu; store8(G[4] + 0x24u, G[2]); G[2] = 0x08C95DFCu; G[3] = word(rt, G[16] + 8u); put(rt, G[4] + 8u, G[2]); G[2] = 0x2Cu; G[5] = 0u; G[7] = 0x08CE0000u; store8(G[3] + 0x25u, G[2]); G[6] = 0x24u; put(rt, G[4] + 0xCu, 0u); store8(G[4] + 0x10u, 0u); do { G[2] = word(rt, 0x08CE0E10u); G[3] = word(rt, G[16] + 8u); G[2] += G[5]; G[4] = load8(G[2]); G[3] = G[5] + G[3]; G[5] += 1u; store8(G[3] + 0x26u, G[4]); } while (G[5] != G[6]); G[5] = 0u; G[6] = 0x1Au; do { G[2] = word(rt, 0x08CE0E14u); G[3] = word(rt, G[16] + 8u); G[2] += G[5]; G[4] = load8(G[2]); G[3] = G[5] + G[3]; G[5] += 1u; store8(G[3] + 0x4Au, G[4]); } while (G[5] != G[6]); G[4] = word(rt, G[16] + 8u); G[2] = 0x08CB5980u; put(rt, G[4] + 0x14u, G[2]); G[3] = 5u; put(rt, G[4] + 0x20u, G[3]); G[2] = 0x08CB5988u; G[3] = 4u; put(rt, G[4] + 0x18u, G[3]); put(rt, G[4] + 0x1Cu, G[2]); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B72440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto negative = [&](bool neg) { G[2] = neg ? 0xFFFFFFFFu : 1u; }; G[3] = word(rt, G[4]); G[7] = G[5]; if (G[3] < 2u) { G[2] = 1u; return; } G[6] = word(rt, G[5]); if (G[6] < 2u) { G[2] = 1u; return; } if (G[3] == 4u) { if (G[6] == G[3]) { G[3] = word(rt, G[5] + 4u); G[2] = word(rt, G[4] + 4u); G[2] = G[3] - G[2]; } else { G[2] = word(rt, G[4] + 4u); negative(G[2] != 0u); } return; } if (G[6] == 4u) { G[2] = word(rt, G[7] + 4u); negative(G[2] == 0u); return; } if (G[3] == 2u) { if (G[6] == 2u) { G[2] = 0u; return; } G[2] = word(rt, G[7] + 4u); negative(G[2] == 0u); return; } if (G[6] == 2u) { G[2] = word(rt, G[4] + 4u); negative(G[2] != 0u); return; } G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[5] + 4u); if (G[3] != G[2]) { negative(G[3] != 0u); return; } G[6] = word(rt, G[4] + 8u); G[5] = word(rt, G[5] + 8u); if (static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[6])) { negative(G[3] != 0u); return; } if (static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[5])) { negative(G[3] == 0u); return; } G[5] = word(rt, G[4] + 0x14u); G[6] = word(rt, G[7] + 0x10u); G[7] = word(rt, G[7] + 0x14u); if (G[7] < G[5]) { negative(G[3] != 0u); return; } G[4] = word(rt, G[4] + 0x10u); if (G[7] == G[5] && G[6] < G[4]) { negative(G[3] != 0u); return; } if (G[5] < G[7]) { negative(G[3] == 0u); return; } if (G[5] == G[7] && G[4] < G[6]) { negative(G[3] == 0u); return; } G[2] = 0u; } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_285
} // namespace gt::game

#define HOT285(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_285::name, gt::ReturnKind::kind)
HOT285(0x08A50B8Cu, func_08A50B8C, Word);
HOT285(0x08ADF670u, func_08ADF670, Word);
HOT285(0x08B42850u, func_08B42850, Word);
HOT285(0x08B46C78u, func_08B46C78, Word);
HOT285(0x08B60DB8u, func_08B60DB8, Word);
HOT285(0x08B72440u, func_08B72440, Word);
