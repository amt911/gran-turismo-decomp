// Hot batch 284: seventy-three-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (list drains, constructors, array fills and two-argument setters).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_284 {
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

void func_0897B5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[31]); G[20] = G[4] + 0x20u; G[18] = G[4]; G[4] = G[20]; G[19] = G[5]; G[17] = G[18] + 0x18u; G[16] = 0u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[18] + 0x18u); if (G[2] != G[17]) { G[16] = word(rt, G[17] + 4u); G[2] = G[16] + 8u; if (G[19] != G[2]) { G[5] = word(rt, G[16] + 8u); G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[16] = word(rt, G[17] + 4u); G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[18] + 0x18u); G[18] = 0x08CA0000u; bool stop = false; if (G[17] != G[2]) { do { G[16] = word(rt, G[19]); G[4] = 0x08CA2F7Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[6] = word(rt, G[17] + 4u); G[4] = G[16]; G[2] = word(rt, G[3] + 0x110u); G[5] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); if (G[2] == 0u) { G[16] = 1u; stop = true; break; } G[16] = word(rt, G[17] + 4u); G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[17]); } while (G[2] != G[17]); } if (!stop) G[16] = 1u; } G[4] = G[20]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_089A3744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = G[5]; G[18] = G[4]; G[16] = G[6]; call_here(rt, ctx, 0x0896F1BCu); G[17] = G[2]; if (G[2] == 0u) { G[6] = word(rt, G[16] + 0x24u); G[2] = 1u; if (G[6] == G[2]) { G[4] = G[18]; call_here(rt, ctx, 0x089A3130u); G[6] = G[18]; G[4] = frame.at(0u); G[5] = G[19]; if (G[2] != 0u) G[6] = G[2]; call_here(rt, ctx, 0x08A11BF0u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA4D34u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x108u); G[17] = 0u; call_here(rt, ctx, G[2]); G[3] = 2u; G[4] = frame.at(0u); if (G[2] != 0u) G[17] = G[3]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 2u; if (G[6] == G[2]) { G[4] = G[18]; call_here(rt, ctx, 0x089A3130u); G[6] = G[18]; G[4] = frame.at(0u); G[5] = G[19]; if (G[2] != 0u) G[6] = G[2]; call_here(rt, ctx, 0x08A0BE48u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA4D44u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x108u); call_here(rt, ctx, G[2]); G[3] = 2u; G[4] = frame.at(0u); if (G[2] != 0u) G[17] = G[3]; call_here(rt, ctx, 0x08A5B9ECu); } } } G[2] = G[17]; G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_089A5BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; call_here(rt, ctx, 0x089A2528u); G[2] = 0x08CBAD20u; put(rt, G[16], G[2]); G[2] = 0x08CBAF78u; G[6] = frame.at(0u); put(rt, G[16] + 0x38u, G[2]); G[4] = G[16] + 0x128u; G[5] = 0x08CA4F10u; put(rt, G[16] + 0x120u, 0u); put(rt, G[16] + 0x124u, 0u); call_here(rt, ctx, 0x08B85EB8u); G[4] = 0x08D74FDCu; G[2] = word(rt, G[16] + 0x138u); G[2] &= ~0xFu; G[2] |= 0x10u; G[2] &= ~(3u << 5); put(rt, G[16] + 0x138u, G[2]); G[3] = G[2]; put(rt, G[16] + 0x130u, G[4]); G[7] = G[16] + 0x13Cu; put(rt, G[16] + 0x12Cu, G[4]); G[5] = 0x08CF6ED8u; G[4] = G[16] + 0x150u; put(rt, G[16] + 0x134u, 0u); G[6] = load8(0x08CF38F0u); G[2] = 0x08CF0000u; put(rt, G[16] + 0x13Cu, G[7]); G[3] = (G[3] & ~(1u << 7)) | ((G[6] & 1u) << 7); G[3] |= 0x100u; G[3] &= ~(1u << 9); put(rt, G[16] + 0x138u, G[3]); put(rt, G[7] + 4u, G[7]); put(rt, G[16] + 0x144u, 0u); put(rt, G[16] + 0x148u, 0u); put(rt, G[16] + 0x14Cu, 0u); call_here(rt, ctx, 0x089F09DCu); G[2] = G[16] + 0x154u; G[3] = G[16] + 0x16Cu; G[6] = G[16] + 0x158u; G[7] = G[16] + 0x170u; put(rt, G[2] + 0x14u, 0u); G[4] = G[16]; G[5] = 0u; put(rt, G[2] + 0x10u, G[6]); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, G[6]); put(rt, G[3] + 0x10u, G[7]); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 0xCu, G[7]); G[2] = word(rt, G[16] + 0x110u); G[2] |= 1u; put(rt, G[16] + 0x110u, G[2]); call_here(rt, ctx, 0x0896A824u); G[2] = word(rt, G[16] + 0x60u); G[2] |= 8u; put(rt, G[16] + 0x60u, G[2]); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_089F12C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[23] = G[6]; G[22] = G[4]; G[21] = 0u; G[19] = G[7]; G[20] = G[19]; G[18] = G[22] + 0x20u; G[17] = G[23] << 2; G[16] = G[5]; call_here(rt, ctx, 0x089FD978u); G[2] = 0x08CBA5F8u; G[5] = G[16]; put(rt, G[22], G[2]); G[4] = G[22] + 0x1Cu; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[17]; G[5] = 4u; put(rt, G[22] + 0x20u, 0u); G[16] = frame.at(4u); put(rt, G[18] + 4u, 0u); put(rt, G[18] + 8u, 0u); call_here(rt, ctx, 0x08A5BC20u); G[19] = G[2]; G[2] += G[17]; put(rt, G[18] + 8u, G[2]); G[4] = G[16]; put(rt, G[22] + 0x20u, G[19]); put(rt, G[18] + 4u, G[19]); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[19]; G[5] = G[23]; G[6] = G[16]; G[7] = frame.at(0u); call_here(rt, ctx, 0x08B85AF4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, G[22] + 0x20u); G[17] += G[19]; put(rt, G[18] + 4u, G[17]); if (static_cast<std::int32_t>(G[23]) > 0) { while (true) { G[2] = G[21] << 2; G[2] += G[19]; G[21] += 1u; G[4] = G[2]; if (G[20] != G[2]) { G[5] = word(rt, G[20]); call_here(rt, ctx, 0x08A5BAD8u); } const bool more = G[23] != G[21]; G[20] += 4u; if (!more) break; G[19] = word(rt, G[22] + 0x20u); } } G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A03ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[1] = 0x3F800000u; F[0] = 1.0f; G[2] = 0xFFFFFFFFu; G[22] = G[5]; put_float(rt, G[4], F[0]); put(rt, G[4] + 0x10u, G[2]); put_float(rt, G[4] + 0xCu, F[0]); put_float(rt, G[4] + 8u, F[0]); put_float(rt, G[4] + 4u, F[0]); G[16] = word(rt, G[5]); G[19] = G[4]; if (G[16] == 0u) { G[4] = 0x08CA8D38u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x7Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[20] = G[2]; if (G[2] != 0u) { G[23] = frame.at(0u); G[18] = 0u; G[21] = 0x08CB0000u; G[17] = G[19]; do { G[16] = word(rt, G[22]); G[4] = 0x08CA8D38u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = G[16]; G[6] = G[18]; G[3] = word(rt, G[2] + 0x84u); G[4] = frame.at(0u); G[18] += 1u; call_here(rt, ctx, G[3]); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA8D38u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17], F[0]); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[17] += 4u; } while (G[20] != G[18]); } G[2] = G[19]; G[22] = word(rt, frame.at(0x28u)); G[19] = word(rt, frame.at(0x1Cu)); G[23] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_08A27FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); G[16] = frame.at(8u); G[5] = 0xEu; G[23] = G[4]; G[4] = G[16]; G[20] = frame.at(4u); call_here(rt, ctx, 0x08ABD5ECu); G[19] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A98A70u); G[18] = G[16]; if (static_cast<std::int32_t>(G[19]) > 0) { G[22] = frame.at(0u); G[17] = 0u; G[21] = 0x08CB0000u; do { G[2] = word(rt, frame.at(4u)); G[4] = 0x08CAB890u; G[16] = G[2] + 0x14u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[18]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[4] = G[16]; G[5] = G[3]; G[6] = frame.at(0u); if (G[3] == G[2]) call_here(rt, ctx, 0x08B8A03Cu); else { G[4] = G[3]; G[5] = frame.at(0u); if (G[3] != 0u) { call_here(rt, ctx, 0x08A76ABCu); G[3] = word(rt, G[16] + 4u); } G[3] += 4u; put(rt, G[16] + 4u, G[3]); } G[17] += 1u; G[4] = G[22]; G[18] += 4u; call_here(rt, ctx, 0x08A5B9ECu); } while (G[19] != G[17]); } G[5] = word(rt, frame.at(4u)); if (G[23] != G[20]) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x5Cu)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); }  } }
void func_08A36240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[18] = G[5]; G[17] = G[7]; put_float(rt, frame.at(0x20u), F[20]); if (static_cast<std::int32_t>(G[6]) > 0) { G[2] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[20] = 1.0f; if (G[2] == 0u) { G[16] = word(rt, G[7] + 4u); if (G[16] == 0u) { G[4] = 0x08CACEB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08CACEB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[4] = frame.at(8u); G[2] = word(rt, G[3] + 0xCu); G[17] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08A354ECu); G[16] = word(rt, frame.at(4u)); F[12] = F[20]; if (G[16] == 0u) { G[4] = 0x08CACF04u; call_here(rt, ctx, 0x08A5BB7Cu); } F[12] = F[20]; G[5] = word(rt, frame.at(8u)); G[4] = G[16]; call_here(rt, ctx, 0x08A34FB8u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } F[20] = float_at(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_08A392C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[2] = 1u; G[18] = G[5]; G[17] = G[7]; put_float(rt, frame.at(0x24u), F[21]); put_float(rt, frame.at(0x20u), F[20]); if (G[6] == G[2]) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } F[21] = 0.0f; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } else { G[2] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[20] = 1.0f; F[21] = 0.0f; if (G[2] == 0u) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); F[21] = F[0]; if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } } G[5] = G[18]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A391F0u); G[16] = word(rt, frame.at(0u)); F[12] = F[21]; if (G[16] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } F[12] = F[21]; F[13] = F[20]; G[4] = G[16]; call_here(rt, ctx, 0x08A38F98u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_284
} // namespace gt::game

#define HOT284(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_284::name, gt::ReturnKind::kind)
// HOT284(0x0897B5C0u, func_0897B5C0, Word);  // no comparable synthetic case yet
HOT284(0x089A3744u, func_089A3744, Word);
HOT284(0x089A5BCCu, func_089A5BCC, Word);
HOT284(0x089F12C4u, func_089F12C4, Word);
// HOT284(0x08A03ED8u, func_08A03ED8, Word);  // no comparable synthetic case yet
HOT284(0x08A27FB0u, func_08A27FB0, Word);
HOT284(0x08A36240u, func_08A36240, Word);
// HOT284(0x08A392C8u, func_08A392C8, Word);  // no comparable synthetic case yet
