// Hot batch 300: seventy-eight-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (lazy child creation, a box overlap test, a slot scan, a pointer fix-up chain, a vector resize and a bit-masked copy).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_300 {
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

void func_0886B7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[18] = G[4]; G[17] = word(rt, G[18] + 4u); G[2] = word(rt, G[17] + 0x28u); G[4] = 0x78u; if (G[2] == 0u) { call_here(rt, ctx, 0x08B62344u); G[5] = 0x08C9F6B4u; G[16] = G[2]; G[4] = G[2]; G[6] = G[18]; call_here(rt, ctx, 0x0886F5FCu); put(rt, G[17] + 0x28u, G[16]); G[5] = 2u; G[2] = word(rt, G[18] + 4u); G[4] = word(rt, G[2] + 0x28u); call_here(rt, ctx, 0x0886A9A0u); G[16] = G[2] + 0xCu; G[3] = 0x08C90000u; G[9] = 0x08C90000u; G[8] = 0x08C89130u; G[12] = 0x08C89124u; G[5] = 0x08C89118u; G[7] = frame.at(0xCu); G[6] = frame.at(0u); G[4] = G[16]; if (G[2] != 0u) { G[10] = word(rt, G[8] + 8u); G[2] = word(rt, 0x08C89130u); G[11] = word(rt, G[12] + 8u); G[3] = word(rt, G[8] + 4u); G[8] = word(rt, 0x08C89124u); G[9] = word(rt, G[12] + 4u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); put(rt, frame.at(8u), G[10]); put(rt, frame.at(0xCu), G[8]); put(rt, frame.at(0x10u), G[9]); put(rt, frame.at(0x14u), G[11]); call_here(rt, ctx, 0x08AF4408u); G[1] = 0x42700000u; F[12] = 60.0f; G[4] = G[16]; call_here(rt, ctx, 0x08AF4458u); } G[2] = word(rt, G[18] + 4u); G[5] = 1u; G[4] = word(rt, G[2] + 0x28u); call_here(rt, ctx, 0x0886ADECu); G[17] = word(rt, G[18] + 4u); } G[2] = word(rt, G[17] + 0x2Cu); G[4] = 0x78u; if (G[2] == 0u) { call_here(rt, ctx, 0x08B62344u); G[5] = 0x08C9F6B4u; G[16] = G[2]; G[4] = G[2]; G[6] = G[18]; call_here(rt, ctx, 0x0886E7A8u); put(rt, G[17] + 0x2Cu, G[16]); G[5] = 1u; G[2] = word(rt, G[18] + 4u); G[4] = word(rt, G[2] + 0x2Cu); call_here(rt, ctx, 0x0886ADECu); } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } } }
void func_0889DEE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x58u), G[31]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); G[21] = G[6]; G[20] = frame.at(0x18u); G[19] = frame.at(0xCu); G[18] = frame.at(0x24u); G[17] = G[5]; G[5] = G[18]; G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x14u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[2] = word(rt, G[16]); G[4] = G[16]; G[3] = word(rt, G[2] + 0x10u); G[5] = frame.at(0u); call_here(rt, ctx, G[3]); F[2] = float_at(rt, frame.at(0x24u)); F[1] = float_at(rt, frame.at(0x28u)); F[0] = float_at(rt, frame.at(0x2Cu)); F[2] = -F[2]; F[1] = -F[1]; F[0] = -F[0]; G[6] = frame.at(0u); G[4] = G[17]; G[5] = G[17]; put_float(rt, frame.at(0x24u), F[2]); put_float(rt, frame.at(0x28u), F[1]); put_float(rt, frame.at(0x2Cu), F[0]); call_here(rt, ctx, 0x08B4AA68u); G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x08B4BAA8u); G[3] = word(rt, G[16]); G[4] = G[16]; G[5] = G[19]; G[2] = word(rt, G[3] + 0x2Cu); G[6] = G[20]; call_here(rt, ctx, G[2]); G[2] = 0u; G[3] = 3u; F[2] = float_at(rt, G[21]); for (;;) { F[0] = float_at(rt, G[19]); F[1] = float_at(rt, G[17]); G[2] += 1u; F[0] = F[0] - F[2]; G[17] += 4u; G[19] += 4u; bool c = F[1] < F[0]; G[21] += 4u; if (c) { G[2] = 0u; break; } F[0] = float_at(rt, G[20]); F[0] = F[2] + F[0]; c = F[0] < F[1]; G[20] += 4u; if (c) { G[2] = 0u; break; } if (G[2] == G[3]) { G[2] = 1u; break; } F[2] = float_at(rt, G[21]); } G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); }  } } }
void func_088E672C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); G[19] = G[5] & 0xFFu; G[17] = G[4]; G[16] = 1u; call_here(rt, ctx, 0x088E511Cu); if (G[2] == 0u) { G[2] = G[16]; } else { G[4] = word(rt, G[17]); G[4] += 0xECu; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, G[2]); if (G[2] == 0u) { G[4] = word(rt, G[17]); G[16] = 0u; G[4] += 0xECu; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; } else { G[20] = load8(G[2] + 0xAu); G[4] = word(rt, G[17]); G[18] = 0u; G[16] = 0u; G[21] = 4u; G[3] = word(rt, G[4]); bool full = true; for (;;) { G[2] = word(rt, G[3] + 0x50u); G[5] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[2]; G[4] = word(rt, G[17]); if (G[2] != 0u) { bool count = true; if (G[19] == 0u) { G[2] = load8(G[2] + 7u); count = G[2] != 0u; } if (count) { G[18] += 1u; G[2] = load8(G[4] + 0x52Bu); G[5] = G[16] + G[4]; bool next = false; if (G[2] != 0u) { G[2] = sext(load8(G[5] + 0x4ACu), 8); next = G[2] == 0u; } if (!next) { G[2] = load8(G[3] + 0xAu); if (G[2] != G[20]) { G[3] = 0u; full = false; break; } } } } G[16] += 1u; if (G[16] == G[21]) break; G[3] = word(rt, G[4]); } if (full) G[3] = 1u; G[2] = word(rt, G[4] + 0x550u); G[16] = 0u; G[4] += 0xECu; G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) G[16] = G[3]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; } } G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_08905BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; G[17] = word(rt, G[4]); put(rt, G[16], G[4]); G[17] = G[16] - G[17]; G[4] = G[16] + 0x4u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0x8u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0xCu; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0x10u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0x14u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0x18u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2C68u); G[4] = G[16] + 0x4Cu; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BF0u); G[4] = G[16] + 0x64u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2C94u); G[4] = G[16] + 0x68u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2C94u); G[4] = G[16] + 0x58u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CB8u); G[4] = G[16] + 0x40u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2C08u); G[4] = G[16] + 0x28u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x2Cu; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x30u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x34u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x38u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x3Cu; G[5] = G[17]; call_here(rt, ctx, 0x08BD2CDCu); G[4] = G[16] + 0x74u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2D00u); G[4] = G[16] + 0x78u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2BD8u); G[4] = G[16] + 0x7Cu; G[5] = G[17]; call_here(rt, ctx, 0x08BD2C20u); G[4] = G[16] + 0x80u; G[5] = G[17]; call_here(rt, ctx, 0x08BD2D24u); G[4] = G[16] + 0x84u; G[5] = G[17]; G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } jump(ctx, 0x08BD2C38u); } }
void func_0892A454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x20u), G[16]); G[17] = frame.at(4u); G[19] = G[4]; G[4] = G[17]; G[18] = G[5]; G[16] = word(rt, G[5]); call_here(rt, ctx, 0x0892A424u); G[7] = word(rt, G[19] + 4u); G[6] = word(rt, G[19]); G[3] = 0xB6DB6DB7u; G[2] = G[7] - G[6]; G[2] = sra(G[2], 2); { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); } G[2] = ctx.lo; G[4] = G[16] < G[2] ? 1u : 0u; G[5] = G[7]; if (G[4] != 0u) { G[5] = G[16] << 5; G[2] = G[16] << 2; G[5] -= G[2]; G[5] = G[6] + G[5]; G[4] = G[19]; G[6] = G[7]; call_here(rt, ctx, 0x08BDEE7Cu); } else { G[6] = G[16] - G[2]; G[7] = G[17]; G[4] = G[19]; call_here(rt, ctx, 0x08BDF434u); } G[4] = word(rt, frame.at(0x14u)); G[5] = word(rt, frame.at(0x18u)); G[6] = frame.at(1u); call_here(rt, ctx, 0x08BDD7C8u); G[4] = word(rt, frame.at(0x14u)); if (G[4] != 0u) call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, frame.at(8u)); G[5] = word(rt, frame.at(0xCu)); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BDD7B8u); G[4] = word(rt, frame.at(8u)); if (G[4] != 0u) call_here(rt, ctx, 0x08B35254u); G[2] = word(rt, G[18]); if (G[2] != 0u) { G[16] = 0u; G[17] = 0u; do { G[3] = word(rt, G[18] + 4u); G[2] = G[16] << 2; G[4] = word(rt, G[19]); G[2] += G[3]; G[5] = word(rt, G[2]); G[4] = G[17] + G[4]; G[16] += 1u; call_here(rt, ctx, 0x0892A37Cu); G[2] = word(rt, G[18]); G[2] = G[16] < G[2] ? 1u : 0u; G[17] += 0x1Cu; } while (G[2] != 0u); } G[17] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08938808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x0u), G[16]); G[18] = G[5] + 1u; G[17] = G[6]; G[3] = load8(G[5]); G[2] = word(rt, G[4] + 4u); G[7] = G[3] >> 6; G[16] = word(rt, G[2] + 0xCu); G[2] = 1u; G[3] &= 0x3Fu; if (G[7] == 1u) { G[4] = G[6]; G[5] = G[18]; G[6] = G[16]; tail = true; } else if (G[7] == 0u) { G[2] = 2u; G[5] = word(rt, G[4] + 0x20u); G[4] = G[6]; G[6] = G[16]; ctx.lo = G[5]; { std::int64_t acc = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo); acc += static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[16]); ctx.lo = static_cast<std::uint32_t>(acc); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(acc) >> 32); } G[5] = ctx.lo; tail = true; } else if (G[7] == 2u) { G[5] = word(rt, G[4] + 0x20u); G[4] = G[6]; G[6] = G[16]; ctx.lo = G[5]; { std::int64_t acc = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo); acc += static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[16]); ctx.lo = static_cast<std::uint32_t>(acc); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(acc) >> 32); } G[5] = ctx.lo; call_here(rt, ctx, 0x08B79A88u); G[2] = static_cast<std::int32_t>(G[16]) < 0 ? 1u : 0u; G[3] = G[16] + 7u; if (G[2] == 0u) G[3] = G[16]; G[3] = sra(G[3], 3); G[2] = G[16] & 7u; G[7] = G[18] + G[3]; G[2] = G[2] != 0u ? 1u : 0u; G[7] += G[2]; if (static_cast<std::int32_t>(G[16]) > 0) { G[6] = 0u; G[2] = static_cast<std::int32_t>(G[6]) < 0 ? 1u : 0u; do { G[3] = G[6] + 7u; if (G[2] == 0u) G[3] = G[6]; G[4] = sra(G[6], 31); G[3] = sra(G[3], 3); G[4] >>= 29; G[3] = G[18] + G[3]; G[5] = load8(G[3]); G[2] = G[6] + G[4]; G[2] &= 7u; G[2] -= G[4]; G[5] = sra(G[5], G[2] & 31u); G[5] &= 1u; G[3] = G[17] + G[6]; G[6] += 1u; if (G[5] != 0u) { G[2] = load8(G[7]); G[7] += 1u; store8(G[3], G[2]); } G[2] = static_cast<std::int32_t>(G[6]) < 0 ? 1u : 0u; } while (G[16] != G[6]); } } else { G[2] = 2u; } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x08B79A88u); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_300
} // namespace gt::game

#define HOT300(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_300::name, gt::ReturnKind::kind)
HOT300(0x0886B7E8u, func_0886B7E8, Word);
// HOT300(0x0889DEE4u, func_0889DEE4, Word);  // no comparable synthetic case yet
HOT300(0x088E672Cu, func_088E672C, Word);
// HOT300(0x08905BE0u, func_08905BE0, Word);  // no comparable synthetic case yet
// HOT300(0x0892A454u, func_0892A454, Word);  // no comparable synthetic case yet
HOT300(0x08938808u, func_08938808, Word);
