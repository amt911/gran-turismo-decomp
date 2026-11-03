// Hot batch 294: seventy-six-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (a cursor step, a masked byte unpack, wrapped searches, list builders and a path string).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_294 {
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

void func_08927D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[17] = G[5]; G[16] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0xCu); G[5] = 1u; call_here(rt, ctx, G[2]); bool go = G[2] != 0u; if (!go) { G[4] = G[16]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0xCu); G[5] = 64u; call_here(rt, ctx, G[2]); go = G[2] != 0u; } if (go) { G[2] = word(rt, G[16] + 0x1FB0u); if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = G[16]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0xCu); G[5] = 64u; call_here(rt, ctx, G[2]); G[3] = word(rt, G[17] + 8u); if (G[2] != 0u) G[3] -= 1u; else G[3] += 1u; put(rt, G[17] + 8u, G[3]); G[4] = word(rt, G[17] + 4u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[2] == 0u) put(rt, G[17] + 8u, 0u); else if (static_cast<std::int32_t>(G[3]) < 0) { G[2] = G[4] - 1u; put(rt, G[17] + 8u, G[2]); } G[4] = G[16]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0xCu); G[5] = 64u; call_here(rt, ctx, G[2]); G[7] = G[2]; G[4] = G[16]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08927BE4u); if (G[2] != 0u) { G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089274C8u); } store8(G[16] + 0x1FCDu, 0u); put(rt, G[16] + 0x1FC8u, 0u); store8(G[16] + 0x1FCCu, 0u); G[2] = word(rt, G[17] + 8u); put(rt, G[17] + 0xCu, G[2]); } } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_089396F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x0u), G[16]); G[18] = G[5] + 1u; G[17] = G[6]; G[3] = load8(G[5]); G[2] = word(rt, G[4] + 4u); G[7] = G[3] >> 6; G[16] = word(rt, G[2] + 0x18u); G[5] = G[3] & 0x3Fu; if (G[7] == 1u) { G[4] = G[6]; G[5] = G[18]; G[6] = G[16]; tail = true; } else if (G[7] == 2u) { call_here(rt, ctx, 0x089396D8u); G[5] = G[2]; G[4] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x08B79A88u); G[2] = static_cast<std::int32_t>(G[16]) < 0 ? 1u : 0u; G[3] = G[16] + 7u; if (G[2] == 0u) G[3] = G[16]; G[3] = sra(G[3], 3); G[2] = G[16] & 7u; G[7] = G[18] + G[3]; G[2] = G[2] != 0u ? 1u : 0u; G[7] += G[2]; if (static_cast<std::int32_t>(G[16]) > 0) { G[6] = 0u; do { G[2] = static_cast<std::int32_t>(G[6]) < 0 ? 1u : 0u; G[3] = G[6] + 7u; if (G[2] == 0u) G[3] = G[6]; G[4] = sra(G[6], 31); G[3] = sra(G[3], 3); G[4] >>= 29; G[3] = G[18] + G[3]; G[5] = load8(G[3]); G[2] = G[6] + G[4]; G[2] &= 7u; G[2] -= G[4]; G[5] = sra(G[5], G[2] & 31u); G[5] &= 1u; G[3] = G[17] + G[6]; G[6] += 1u; if (G[5] != 0u) { G[2] = load8(G[7]); G[7] += 1u; store8(G[3], G[2]); } } while (G[16] != G[6]); } } else if (G[7] == 0u) { call_here(rt, ctx, 0x089396D8u); G[5] = G[2]; G[4] = G[17]; G[6] = G[16]; tail = true; } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x08B79A88u); } }
void func_089E7D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[18]); G[19] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x089E4EA0u); G[4] = G[17]; G[20] = G[2]; call_here(rt, ctx, 0x089E4EC8u); G[16] = G[2]; while (true) { G[2] = ~G[16]; G[2] >>= 31; const bool in_range = G[2] != 0u; if (static_cast<std::int32_t>(G[19]) <= 0) { if (in_range) { G[6] = G[16]; G[5] = G[20]; G[4] = G[17]; call_here(rt, ctx, 0x089E4DE0u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x089E7C4Cu); G[2] = 1u; } else G[2] = 0u; break; } if (!in_range) { G[2] = 0u; break; } G[2] = word(rt, G[17] + 0x158u); G[16] += 1u; if (G[16] == G[2]) { G[2] = word(rt, G[17] + 0x150u); G[2] &= 2u; if (G[2] == 0u) { G[16] = 0xFFFFFFFFu; continue; } G[16] = 0u; } else if (static_cast<std::int32_t>(G[16]) < 0) continue; G[4] = G[17]; G[5] = G[20]; G[6] = G[16]; call_here(rt, ctx, 0x089E4DE0u); G[5] = G[2]; G[4] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x089E6C80u); if (G[2] != 0u) continue; G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x089E7520u); if (G[2] == 0u) continue; G[4] = G[17]; G[19] -= 1u; call_here(rt, ctx, 0x089E4EC8u); if (G[2] == G[16]) G[16] = 0xFFFFFFFFu; } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_089E7F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[18]); G[19] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x089E4EA0u); G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x089E4EC8u); G[20] = G[2]; while (true) { G[2] = ~G[16]; G[2] >>= 31; const bool in_range = G[2] != 0u; if (static_cast<std::int32_t>(G[19]) <= 0) { if (in_range) { G[6] = G[20]; G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x089E4DE0u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x089E7C4Cu); G[2] = 1u; } else G[2] = 0u; break; } if (!in_range) { G[2] = 0u; break; } G[2] = word(rt, G[17] + 0x154u); G[16] += 1u; if (G[16] == G[2]) { G[2] = word(rt, G[17] + 0x150u); G[2] &= 2u; if (G[2] == 0u) { G[16] = 0xFFFFFFFFu; continue; } G[16] = 0u; } else if (static_cast<std::int32_t>(G[16]) < 0) continue; G[4] = G[17]; G[5] = G[16]; G[6] = G[20]; call_here(rt, ctx, 0x089E4DE0u); G[5] = G[2]; G[4] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x089E6C80u); if (G[2] != 0u) continue; G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x089E7520u); if (G[2] == 0u) continue; G[4] = G[17]; G[19] -= 1u; call_here(rt, ctx, 0x089E4EA0u); if (G[2] == G[16]) G[16] = 0xFFFFFFFFu; } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_08A42370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x10u), G[16]); G[3] = 0x08D74FD0u; G[2] = G[3] + 0xCu; G[30] = 0x08CB0000u; G[23] = G[6]; G[22] = G[7]; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[5]; G[18] = G[4]; G[17] = 0u; put(rt, G[4], G[2]); while (true) { G[4] = G[19]; call_here(rt, ctx, 0x08A40BF8u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[6] = G[17]; G[4] = G[21]; G[5] = G[19]; G[17] += 1u; if (G[2] == 0u) break; call_here(rt, ctx, 0x08A41258u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CADBFCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; G[6] = G[23]; G[7] = G[22]; G[4] = G[20]; call_here(rt, ctx, 0x08A425E4u); G[4] = G[18]; G[5] = G[20]; call_here(rt, ctx, 0x08B89054u); G[4] = word(rt, frame.at(8u)); G[16] = G[4] - 0xCu; if (G[16] != 0x08D74FD0u) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); } G[2] = G[18]; G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A4B700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[5] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[18] = G[4]; G[17] = G[6]; if (G[5] == 0u) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CAED38u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); if (G[16] == 0u) { G[4] = 0x08CAED38u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[4] = frame.at(0xCu); G[2] = word(rt, G[3] + 0xCu); G[16] = frame.at(4u); call_here(rt, ctx, G[2]); G[4] = word(rt, frame.at(8u)); G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08B0EFACu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = 0x08D74FD0u; G[4] = word(rt, frame.at(0xCu)); G[16] = G[4] - 0xCu; if (G[16] != 0x08D74FD0u) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = word(rt, frame.at(8u)); G[16] = G[4] - 0xCu; if (G[16] != 0x08D74FD0u) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A74B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = frame.at(4u); G[20] = G[4]; G[4] = G[19]; G[18] = G[5]; call_here(rt, ctx, 0x08AAB5DCu); G[4] = G[19]; G[5] = 0x08CB0C94u; call_here(rt, ctx, 0x08AABA8Cu); G[16] = word(rt, G[18] + 0x14u); G[17] = G[2]; if (G[16] == 0u) { G[4] = 0x08CB0CA4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = word(rt, G[16] + 0x14u); if (G[4] != 0u) { G[2] = word(rt, G[4] + 4u); if (G[2] == 0u) G[4] = 0u; } call_here(rt, ctx, 0x08A7BDB0u); G[4] = G[17]; G[5] = G[2]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CB0CACu; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x08A5D454u); G[4] = G[16]; G[5] = G[2]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CB0CB0u; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[19]; call_here(rt, ctx, 0x08AAB540u); G[5] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08B88F44u); G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; if (G[16] != 0x08D74FD0u) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[2] = G[20]; G[19] = word(rt, frame.at(0x1Cu)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_294
} // namespace gt::game

#define HOT294(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_294::name, gt::ReturnKind::kind)
// HOT294(0x08927D14u, func_08927D14, Word);  // no comparable synthetic case yet
// HOT294(0x089396F8u, func_089396F8, Word);  // no comparable synthetic case yet
HOT294(0x089E7D1Cu, func_089E7D1C, Word);
HOT294(0x089E7F74u, func_089E7F74, Word);
HOT294(0x08A42370u, func_08A42370, Word);
HOT294(0x08A4B700u, func_08A4B700, Word);
// HOT294(0x08A74B34u, func_08A74B34, Word);  // no comparable synthetic case yet
