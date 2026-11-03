// Hot batch 292: seventy-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a hex key parse, a bit-stream header, a double unpack, task teardown and a heap sift).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_292 {
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

void func_08ACA754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); G[17] = G[6]; G[16] = G[5]; if (G[5] == 0u || G[6] == 0u) G[2] = 0x80001000u; else { G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[2] -= 0x22u; G[2] = G[2] < 2u ? 1u : 0u; bool bad = true; if (G[2] != 0u) { G[5] = 0x08CB49BCu; G[4] = G[16]; G[6] = 2u; call_here(rt, ctx, 0x08B7C598u); bad = G[2] != 0u; } if (bad) G[2] = 0x80001004u; else { G[4] = G[17]; G[5] = 0u; G[6] = 0x10u; G[18] = G[16] + 2u; call_here(rt, ctx, 0x08B36018u); G[16] = 0u; while (true) { G[4] = G[18]; call_here(rt, ctx, 0x08B35F60u); G[2] = G[16] < G[2] ? 1u : 0u; G[3] = G[18] + G[16]; if (G[2] == 0u) break; G[4] = sext(load8(G[3]), 8); call_here(rt, ctx, 0x08B7E8A4u); G[4] = G[2] & 0xFFu; G[2] = G[4] - 0x30u; G[3] = G[4] - 0x61u; G[5] = G[2] & 0xFFu; G[3] &= 0xFFu; G[2] = G[5] < 0xAu ? 1u : 0u; G[3] = G[3] < 6u ? 1u : 0u; if (G[2] != 0u) G[4] = G[5]; else { G[2] = G[4] - 0x57u; if (G[3] != 0u) G[4] = G[2] & 0xFFu; } G[2] = G[16] ^ 1u; G[3] = G[16] >> 1; G[2] &= 1u; G[5] = G[17] + G[3]; G[16] += 1u; G[3] = G[4] << 4; if (G[2] != 0u) G[4] = G[3] & 0xFFu; G[2] = load8(G[5]); G[2] = G[4] + G[2]; store8(G[5], G[2]); } } } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_08B279BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08B27874u); do { G[4] = G[16]; G[5] = load8(G[16] + 0x58u); G[6] = 1u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[4] = G[16]; G[5] = 0u; G[6] = 2u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[4] = G[16]; call_here(rt, ctx, 0x08B2797Cu); if (G[2] == 0u) { G[2] = 0u; break; } G[17] = word(rt, G[16] + 0x64u); G[4] = G[16]; G[5] = G[17]; G[6] = 8u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[4] = G[16]; G[5] = G[17] >> 8; G[6] = 8u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[18] = ~G[17]; G[4] = G[16]; G[5] = G[18]; G[6] = 8u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[4] = G[16]; G[5] = G[18] >> 8; G[6] = 8u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { G[2] = 0u; break; } G[2] = 1u; if (G[17] == 0u) break; G[18] = 0u; bool ok = true; do { G[2] = word(rt, G[16] + 0x5Cu); G[4] = G[16]; G[6] = 8u; G[2] += G[18]; G[5] = load8(G[2]); G[18] += 1u; call_here(rt, ctx, 0x08B278A0u); if (G[2] == 0u) { ok = false; break; } } while (G[17] != G[18]); G[2] = ok ? 1u : 0u; } while (false); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); }  } }
void func_08B31E80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x90u); put(rt, frame.at(0x88u), G[18]); put(rt, frame.at(0x84u), G[17]); put(rt, frame.at(0x80u), G[16]); put(rt, frame.at(0x8Cu), G[31]); G[18] = G[5]; G[17] = G[6]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x64u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[6] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x10u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = 0u; bool query = false; if (G[2] != 0u) { if (G[18] == 1u) { G[4] = 0xBu; query = true; } else if (G[18] == 2u) { G[4] = 0xCu; query = true; } } if (query) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08C7AF20u); G[6] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { G[6] = 1u; for (std::uint32_t i = 0; i < 0x10u; ++i) store8(G[17] + i, load8(frame.at(i))); } } } G[2] = G[6]; G[18] = word(rt, frame.at(0x88u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); }  } }
void func_08B72314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = word(rt, G[4] + 4u); G[8] = word(rt, G[4]); G[10] = G[5]; G[3] = G[2] >> 31; G[9] = G[2]; G[7] = (G[2] >> 20) & 0x7FFu; put(rt, G[5] + 4u, G[3]); G[9] &= 0xFFFFFu; if (G[7] == 0u) { G[2] = G[8] | G[9]; if (G[2] == 0u) { G[2] = 2u; put(rt, G[5], G[2]); return; } G[5] = 0x0FFFFFFFu; G[3] = G[8] >> 24; G[7] = G[9] << 8; G[7] |= G[3]; G[2] = 0xFFFFFC02u; G[3] = 3u; G[4] = G[5] < G[7] ? 1u : 0u; G[6] = G[8] << 8; put(rt, G[10] + 8u, G[2]); put(rt, G[10], G[3]); if (G[4] == 0u) { G[8] = G[5]; G[3] = 0xFFFFFC02u; do { G[2] = G[6] >> 31; G[5] = G[7] << 1; G[5] |= G[2]; G[4] = G[6] << 1; G[2] = G[8] < G[5] ? 1u : 0u; G[6] = G[4]; G[7] = G[5]; G[3] -= 1u; } while (G[2] == 0u); put(rt, G[10] + 8u, G[3]); } put(rt, G[10] + 0x10u, G[6]); put(rt, G[10] + 0x14u, G[7]); return; } G[2] = 0x7FFu; if (G[7] != G[2]) { G[2] = G[8] >> 24; G[5] = G[9] << 8; G[5] |= G[2]; G[6] = 0x10000000u; G[7] -= 0x3FFu; G[4] = G[8] << 8; G[3] = G[5] | G[6]; G[2] = 3u; put(rt, G[10] + 0x10u, G[4]); put(rt, G[10] + 0x14u, G[3]); put(rt, G[10] + 8u, G[7]); put(rt, G[10], G[2]); return; } G[2] = G[8] | G[9]; G[4] = 0x80000u; if (G[2] == 0u) { G[2] = 4u; put(rt, G[5], G[2]); return; } G[2] = 0u; G[3] = G[9] & G[4]; G[2] |= G[3]; if (G[2] != 0u) put(rt, G[5], 0u); else { G[2] = 1u; put(rt, G[5], G[2]); } put(rt, G[10] + 0x10u, G[8]); put(rt, G[10] + 0x14u, G[9]); } }
void func_08BD6274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC55C8u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x10u); put(rt, G[4], G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08B34888u); G[2] = word(rt, G[16] + 0x2Cu); G[4] = G[16]; if (G[2] == 0u) { G[18] = G[16] + 0x34u; call_here(rt, ctx, 0x08B34B24u); G[4] = word(rt, G[18] + 4u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x34u); call_here(rt, ctx, G[2]); } put(rt, G[18] + 4u, 0u); put(rt, G[16] + 0x34u, 0u); G[4] = G[16]; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); } else { G[2] = word(rt, G[16] + 0x2Cu); G[19] = 0u; G[18] = 0u; G[2] -= 1u; put(rt, G[16] + 0x2Cu, G[2]); G[3] = word(rt, G[16] + 0x2Cu); if (G[3] == 0u) { G[2] = word(rt, G[16] + 0x34u); if (G[2] != 0u) { G[19] = word(rt, G[16] + 0x38u); G[18] = G[2]; put(rt, G[16] + 0x34u, 0u); put(rt, G[16] + 0x38u, 0u); } G[2] = load8(G[16] + 0x30u); if (G[2] != 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08B34C1Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08B34B24u); if (G[18] != 0u) { G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 8u); G[4] = G[18]; call_here(rt, ctx, G[2]); } G[4] = G[19]; if (G[19] != 0u) { G[3] = word(rt, G[19]); G[2] = word(rt, G[3]); G[5] = G[18]; call_here(rt, ctx, G[2]); } } G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08BAEF88u); } }
void func_08BD64D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5648u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x10u); put(rt, G[4], G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08B34888u); G[2] = word(rt, G[16] + 0x2Cu); G[4] = G[16]; if (G[2] == 0u) { G[18] = G[16] + 0x34u; call_here(rt, ctx, 0x08B34B24u); G[4] = word(rt, G[18] + 4u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x34u); call_here(rt, ctx, G[2]); } put(rt, G[18] + 4u, 0u); put(rt, G[16] + 0x34u, 0u); G[4] = G[16]; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); } else { G[2] = word(rt, G[16] + 0x2Cu); G[19] = 0u; G[18] = 0u; G[2] -= 1u; put(rt, G[16] + 0x2Cu, G[2]); G[3] = word(rt, G[16] + 0x2Cu); if (G[3] == 0u) { G[2] = word(rt, G[16] + 0x34u); if (G[2] != 0u) { G[19] = word(rt, G[16] + 0x38u); G[18] = G[2]; put(rt, G[16] + 0x34u, 0u); put(rt, G[16] + 0x38u, 0u); } G[2] = load8(G[16] + 0x30u); if (G[2] != 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08B34C1Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08B34B24u); if (G[18] != 0u) { G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 8u); G[4] = G[18]; call_here(rt, ctx, G[2]); } G[4] = G[19]; if (G[19] != 0u) { G[3] = word(rt, G[19]); G[2] = word(rt, G[3]); G[5] = G[18]; call_here(rt, ctx, G[2]); } } G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08BAEF88u); } }
void func_08BDA87C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[23] = G[5]; G[2] = G[23] << 1; G[22] = G[6]; G[20] = G[4]; G[17] = G[2] + 2u; G[3] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[6] = G[23]; G[5] = word(rt, G[7] + 4u); G[9] = word(rt, G[7]); store8(frame.at(0x10u), G[8]); put(rt, frame.at(4u), G[5]); put(rt, frame.at(0u), G[9]); if (G[3] != 0u) { G[21] = G[23]; do { G[2] = G[17] << 3; G[16] = G[20] + G[2]; G[18] = word(rt, G[16]); G[5] = word(rt, G[16] - 8u); G[19] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x08B35FD8u); G[3] = G[21] << 3; G[5] = G[3] + G[20]; if (static_cast<std::int32_t>(G[2]) < 0) { G[19] = G[17] - 1u; G[2] = G[19] << 3; G[16] = G[20] + G[2]; G[18] = word(rt, G[16]); } G[2] = word(rt, G[16] + 4u); G[3] = G[19] << 1; G[17] = G[3] + 2u; G[4] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[22]) ? 1u : 0u; put(rt, G[5] + 4u, G[2]); G[21] = G[19]; put(rt, G[5], G[18]); } while (G[4] != 0u); G[6] = G[19]; } G[4] = G[20]; if (G[22] == G[17]) { G[2] = G[22] << 3; G[2] += G[20]; G[4] = word(rt, G[2] - 4u); G[5] = word(rt, G[2] - 8u); G[3] = G[6] << 3; G[3] += G[20]; put(rt, G[3] + 4u, G[4]); G[6] = G[22] - 1u; put(rt, G[3], G[5]); G[4] = G[20]; } G[5] = G[6]; G[8] = G[16]; G[6] = G[23]; G[7] = frame.at(0u); call_here(rt, ctx, 0x08BDA784u); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_292
} // namespace gt::game

#define HOT292(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_292::name, gt::ReturnKind::kind)
HOT292(0x08ACA754u, func_08ACA754, Word);
HOT292(0x08B279BCu, func_08B279BC, Word);
HOT292(0x08B31E80u, func_08B31E80, Word);
HOT292(0x08B72314u, func_08B72314, Word);
// HOT292(0x08BD6274u, func_08BD6274, Word);  // no comparable synthetic case yet
// HOT292(0x08BD64D4u, func_08BD64D4, Word);  // no comparable synthetic case yet
HOT292(0x08BDA87Cu, func_08BDA87C, Word);
