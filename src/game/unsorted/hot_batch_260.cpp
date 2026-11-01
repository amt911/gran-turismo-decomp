// Hot batch 260: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (string scans, list walks and inserts, a bit set/clear, a jump table on a code, a display-list word, a
// wide-char store and calls through saved registers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_260 {
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

void func_08A66810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] = 8u; put(rt, frame.at(4u), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A5BC14u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08A667ECu); G[2] = G[16]; G[16] = word(rt, frame.at(0u)); } }
void func_08B907C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[4] + 0x20u; for (std::uint32_t off = 0u; off <= 0x20u; off += 4u) put(rt, G[4] + off, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); } }
void func_0892FFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = G[5]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x089368DCu); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08AC44A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { for (;;) { if (!(G[4] < G[5])) { G[4] = 0u; break; } G[2] = sext(load8(G[4]), 8u); G[4] = G[4] + 1u; if (G[2] == 0u) break; } G[2] = G[4]; } }
void func_08AD16C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); G[2] = 0xFFFFFFFFu; G[4] = G[16]; put(rt, G[16] + 0x1C0u, 0u); call_here(rt, ctx, 0x08AD1528u); G[4] = G[16]; G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08AD1660u); } }
void func_08B18B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { if (G[4] != 0u) { G[3] = sext(load8(G[4]), 8u); for (;;) { if (G[3] == 0u) { G[4] = 0u; break; } G[2] = 0x2Eu; if (G[3] == G[2]) break; G[2] = 0x5Bu; if (G[3] == G[2]) break; G[4] = G[4] + 1u; G[3] = sext(load8(G[4]), 8u); } } G[2] = G[4]; } }
void func_08B1FE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = word(rt, G[4] + 4u); if (G[2] != 0u) { G[3] = 0u; if (G[5] != 0u) { for (;;) { G[2] = word(rt, G[2] + 8u); G[3] = G[3] + 1u; if (G[2] == 0u) break; if (G[5] == G[3]) break; } } } } }
void func_08B2E064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[31]); call_here(rt, ctx, 0x08C7A9E8u); } G[3] = 0x80020064u; G[4] = 0x7D0u; if (G[2] == G[3]) { jump(ctx, 0x08B32A34u); return; } jump(ctx, 0x08C7A988u); } }
void func_08AD9CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[4] < 0x12u ? 1u : 0u; G[3] = 0x08C90000u; if (G[2] == 0u) { G[2] = G[5] << 1; return; } G[2] = G[4] << 2; G[3] = 0x08C90AD8u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08AD9D00u: G[2] = G[5] << 4; break; case 0x08AD9D08u: G[2] = G[5] << 2; break; case 0x08AD9D10u: G[2] = G[7]; break; default: G[2] = G[5] << 1; break; } } }
void func_08AF8624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = load8(G[4] + 7u); G[3] = G[4]; G[4] = G[5]; G[6] = G[2]; if (G[2] == 0u) return; G[8] = word(rt, G[3]); G[5] = word(rt, G[3] + 0x28u); G[7] = 0xFFFFFFFFu; G[8] = G[8] & 0xFFFu; G[2] = G[8] & 0x80u; G[8] = (G[8] >> 8) & 1u; if (G[2] != 0u) G[7] = load8(G[3] + 6u); jump(ctx, 0x08AFCC0Cu); } }
void func_08B54940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[4] = G[4] + 0x14u; for (;;) { G[6] = word(rt, G[4]); G[7] = G[6] + 0x10u; if (G[6] == 0u) break; G[2] = word(rt, G[5] + 8u); G[3] = word(rt, G[6] + 8u); G[4] = G[6] + 0x14u; G[2] = G[2] < G[3] ? 1u : 0u; if (G[2] == 0u) G[4] = G[7]; } put(rt, G[4], G[5]); put(rt, G[5] + 0x18u, G[4]); put(rt, G[5] + 0xCu, 0u); put(rt, G[5] + 0x10u, 0u); put(rt, G[5] + 0x14u, 0u); put(rt, G[5], 0u); } }
void func_08B55924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto clz = [](std::uint32_t x) { return x == 0u ? 32u : static_cast<std::uint32_t>(__builtin_clz(x)); }; G[7] = 0x08D6CE00u; G[6] = G[6] - 1u; G[8] = word(rt, G[7] + 0x5Cu); G[2] = 0x20u; G[6] = clz(G[6]); G[4] = G[4] + 0xB8u; G[5] = G[5] - 1u; G[6] = G[2] - G[6]; G[4] = G[4] << 24; G[5] = clz(G[5]); G[4] = (G[4] & ~0xFF00u) | ((G[6] & 0xFFu) << 8); G[2] = G[2] - G[5]; G[4] = (G[4] & ~0xFFu) | (G[2] & 0xFFu); G[3] = G[8] + 4u; put(rt, G[8], G[4]); put(rt, G[7] + 0x5Cu, G[3]); } }
void func_08B21758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[7] = G[4]; G[6] = G[6] & 0xFFu; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = sra(G[5], 3u); G[8] = G[4] + G[2]; G[2] = load8(G[7] + 4u); G[4] = G[5] & 7u; G[3] = 1u; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] = G[3] << (G[4] & 31u); if (G[2] == 0u) return; G[2] = load8(G[8] + 5u); G[2] = G[3] | G[2]; G[3] = G[2] - G[3]; if (G[6] == 0u) G[2] = G[3]; store8(G[8] + 5u, G[2]); } }
void func_08B53210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[2] = 0u; if (G[4] == 0u) return; if (G[8] != 0u) { G[2] = G[7] < G[8] ? 1u : 0u; G[8] = G[4] < G[8] ? 1u : 0u; if (G[2] != 0u) { if (G[8] == 0u) G[5] = G[6]; G[2] = G[5] + G[4]; return; } G[2] = G[4] < G[7] ? 1u : 0u; if (G[2] == 0u) G[6] = G[5]; G[2] = G[6] + G[4]; return; } G[2] = G[7] < G[8] ? 1u : 0u; if (G[6] == 0u) { G[2] = G[4] + G[5]; return; } G[2] = G[4] < G[7] ? 1u : 0u; if (G[2] == 0u) G[6] = G[5]; G[2] = G[6] + G[4]; } }
void func_08B54810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[5] = word(rt, G[4] + 0x18u); G[6] = G[4]; if (G[5] != 0u) { G[2] = word(rt, G[5] + 0x14u); put(rt, G[4] + 0x18u, G[2]); G[2] = G[5]; return; } G[2] = word(rt, G[4] + 0x10u); G[4] = G[2] << 2; G[3] = G[2] << 5; G[3] = G[3] - G[4]; G[4] = G[2] + 1u; G[2] = static_cast<std::int32_t>(G[2]) < 0x200 ? 1u : 0u; G[3] = G[3] + G[6]; if (G[2] != 0u) { put(rt, G[6] + 0x10u, G[4]); G[5] = G[3] + 0x1Cu; } G[2] = G[5]; } }
void func_08AD8BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = sext(load8(G[4]), 8u); if (G[2] == 0u) { G[3] = 0u; G[2] = 0u; return; } G[5] = G[2]; for (;;) { G[3] = (G[5] ^ 0x3Fu) == 0u ? 1u : 0u; G[2] = (G[5] ^ 0x2Au) == 0u ? 1u : 0u; G[2] = G[2] | G[3]; G[4] = G[4] + 1u; G[3] = 1u; if (G[2] != 0u) break; G[2] = sext(load8(G[4]), 8u); G[5] = G[2]; if (G[2] == 0u) { G[3] = 0u; break; } } G[2] = G[3]; } }
void func_08AE35BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = word(rt, G[4] + 0x18u); G[2] = word(rt, G[3] + 8u); if (static_cast<std::int32_t>(G[2]) < 0) G[3] = 0u; G[4] = G[3]; while (G[3] != 0u) { G[2] = word(rt, G[4] + 8u); G[3] = 1u; if (G[2] == G[5]) { G[2] = G[3]; return; } G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; } G[3] = 0u; G[2] = 0u; } }
void func_08B80948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = frame.at(0u); if (G[5] != 0u) G[2] = G[5]; if (G[6] == 0u) { G[4] = 0u; G[2] = 0u; return; } G[4] = 0xFFFFFFFEu; if (G[7] != 0u) { G[3] = load8(G[6]); store16(G[2], G[3]); G[2] = load8(G[6]); G[4] = 1u; if (G[2] == 0u) { G[4] = 0u; G[2] = 0u; return; } } G[2] = G[4]; } }
void func_08932598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[31]); G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = word(rt, G[16] + 0x40u); G[7] = word(rt, G[16] + 0x44u); G[8] = frame.at(0u); call_here(rt, ctx, 0x08936584u); G[2] = load8(frame.at(0x23u)); G[16] = word(rt, frame.at(0x30u)); G[2] = (G[2] - 1u) & 0xFFu; G[2] = G[2] < 2u ? 1u : 0u; } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_260
} // namespace gt::game

#define HOT260(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_260::name, gt::ReturnKind::kind)
HOT260(0x08A66810u, func_08A66810, Word);
HOT260(0x08B907C4u, func_08B907C4, Word);
HOT260(0x0892FFA0u, func_0892FFA0, Word);
HOT260(0x08AC44A0u, func_08AC44A0, Word);
HOT260(0x08AD16C0u, func_08AD16C0, Word);
HOT260(0x08B18B58u, func_08B18B58, Word);
HOT260(0x08B1FE38u, func_08B1FE38, Word);
// HOT260(0x08B2E064u, func_08B2E064, Word);  // no comparable synthetic case yet
HOT260(0x08AD9CD4u, func_08AD9CD4, Word);
HOT260(0x08AF8624u, func_08AF8624, Word);
HOT260(0x08B54940u, func_08B54940, Word);
HOT260(0x08B55924u, func_08B55924, Word);
HOT260(0x08B21758u, func_08B21758, Word);
HOT260(0x08B53210u, func_08B53210, Word);
HOT260(0x08B54810u, func_08B54810, Word);
HOT260(0x08AD8BD0u, func_08AD8BD0, Word);
HOT260(0x08AE35BCu, func_08AE35BC, Word);
HOT260(0x08B80948u, func_08B80948, Word);
HOT260(0x08932598u, func_08932598, Word);
