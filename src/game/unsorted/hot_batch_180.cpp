// Hot batch 180: fifteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_180 {
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

void func_0880A574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0896BDF8u); G[2] = 0x08CBA128u; G[3] = 0x08CB9EF0u; put(rt, G[16] + 0x38u, G[2]); put(rt, G[16], G[3]); } G[16] = s0; }
void func_0880D6AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0896BDF8u); G[2] = 0x08CB9788u; G[3] = 0x08CB9550u; put(rt, G[16] + 0x38u, G[2]); put(rt, G[16], G[3]); } G[16] = s0; }
void func_089A2564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A0D658u); G[2] = 0x08CBAD00u; G[3] = 0x08CBAAA8u; put(rt, G[16] + 0x38u, G[2]); put(rt, G[16], G[3]); } G[16] = s0; }
void func_0881FC44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = high_half(0x08CE2ACCu); G[3] = high_half(0x08CE2AD0u); G[5] = word(rt, 0x08CE2ACCu); G[6] = word(rt, 0x08CE2AD0u); G[4] = frame.at(0u); G[8] = 0x0881FB5Cu; G[7] = 0x10u; put(rt, frame.at(8u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x0881FB90u); }
void func_0883A49C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CE2C20u); G[2] = load8(0x08CE2C20u); if (G[2] != 0u) { call_here(rt, ctx, 0x08930908u); call_here(rt, ctx, 0x08930910u); store8(0x08CE2C20u, 0u); } } G[16] = s0; }
void func_08858870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08858830u); G[2] = word(rt, frame.at(0u)); G[4] = 0x08C9E5D0u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_08858958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08858830u); G[2] = word(rt, frame.at(0u)); G[4] = 0x08C9E5D0u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_0885BE34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] &= 0xFFu; put(rt, G[2] + 0x78u, 0u); G[3] = word(rt, G[4]); store8(G[3] + 0x16u, G[5]); G[2] = word(rt, G[4]); if (G[5] != 0u) { G[3] = 1u; store8(G[2] + 0x17u, 1u); G[4] = word(rt, G[4]); store8(G[4] + 0x15u, 0u); } else { store8(G[2] + 0x17u, 0u); } }
void func_0886B130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); const bool ordered = ctx.fpr[13] <= ctx.fpr[14]; G[3] = G[2] + 0x15Cu; ctx.fpr[0] = float_at(rt, G[3] + 0x18u); put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 8u, bits_of(ctx.fpr[0])); put(rt, G[3] + 0x1Cu, bits_of(ctx.fpr[14])); put(rt, G[3] + 0x10u, bits_of(ctx.fpr[12])); put(rt, G[3] + 0x14u, bits_of(ctx.fpr[13])); if (ordered) put(rt, G[3] + 0x18u, bits_of(ctx.fpr[12])); G[2] = word(rt, G[3] + 0x20u) | 3u; put(rt, G[3] + 0x20u, G[2]); }
void func_088757B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = 0x08C9FF90u; { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x088756F0u); G[4] = G[2]; G[5] = 2u; } if (G[2] == 0u) return; jump(ctx, 0x0886A9A0u); }
void func_088758C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (load8(G[4] + 0x25u) << 3) + 0x10u; G[3] = G[4] + G[2]; G[2] = word(rt, G[3] + 4u); G[5] = G[4]; if (G[2] == 0u) return; G[2] = word(rt, G[2] + 8u); put(rt, G[4] + 0x28u, G[2]); G[3] = word(rt, G[3] + 4u); G[4] = word(rt, G[3] + 4u); G[2] += G[4]; put(rt, G[5] + 0x2Cu, G[2]); }
void func_0887BE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CBFE58u; put(rt, G[4], G[2]); G[3] = 0u; G[2] = G[4]; put(rt, G[4] + 0x14u, 0u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[4] = 4u; do { G[3] += 1u; put(rt, G[2] + 4u, G[1]); G[2] += 4u; } while (G[3] != G[4]); }
void func_0887E978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[16] = G[4]; G[4] += 0x64u; call_here(rt, ctx, 0x0887C5A4u); ctx.fpr[12] = ctx.fpr[20]; G[4] = word(rt, G[16] + 0x70u); } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, 0x0888119Cu); }
void func_0888FE78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x0888F550u); G[3] = word(rt, G[2] + 0xDCu); G[16] <<= 5; G[3] = __builtin_bswap32(G[3]) + G[16]; G[2] = __builtin_bswap32(word(rt, G[3] + 0x10u)); } G[16] = s0; }
void func_08891140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0xACu); G[7] = G[4]; G[4] += 8u; if (G[2] == 0u) return; G[2] = word(rt, G[7] + 0xA4u); G[3] = word(rt, G[7] + 0xA8u); put(rt, G[7] + 0xA4u, 0u); G[5] = G[2]; G[6] = G[3]; put(rt, G[7], G[2]); put(rt, G[7] + 4u, G[3]); put(rt, G[7] + 0xA8u, 0u); jump(ctx, 0x0888F53Cu); }
void func_08891290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x6Cu); G[2] = 1u; G[5] = 1u; if (G[3] == G[2]) { G[2] = (word(rt, G[4] + 0x74u) << 2) + G[4]; G[3] = 0x481907FFu; G[4] = word(rt, G[2] + 0x50u); if (G[4] != G[3]) { G[2] = G[5]; return; } } G[5] = 0u; G[2] = 0u; }
void func_08892BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[6] = 0x26D0u; G[16] = G[4]; call_here(rt, ctx, 0x08B36018u); G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; put(rt, G[16] + 0x22A8u, G[4]); put(rt, G[16] + 0x22ACu, G[5]); } G[16] = s0; }
void func_08895544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = word(rt, G[4] + 0x14u); if (G[3] != G[2]) call_here(rt, ctx, 0x088951FCu); else call_here(rt, ctx, 0x088950E0u); }
void func_08895A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = word(rt, G[4] + 0x14u); if (G[3] != G[2]) call_here(rt, ctx, 0x088955D4u); else call_here(rt, ctx, 0x08894F64u); }
void func_0889968C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[6] = word(rt, G[5] + 0xB04u); G[5] = 1u; G[3] = word(rt, G[6]); G[2] = word(rt, G[3] + 0x74u); G[4] = G[6]; call_here(rt, ctx, G[2]); put(rt, G[16], bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_0889A054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[6] = word(rt, G[5] + 0xB04u); G[5] = 0u; G[3] = word(rt, G[6]); G[2] = word(rt, G[3] + 0x74u); G[4] = G[6]; call_here(rt, ctx, G[2]); put(rt, G[16], bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_088D43A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[6] = 0x78u; G[16] = G[4]; call_here(rt, ctx, 0x08B36018u); G[3] = 0xFFFFFFFFu; store8(G[16] + 0x7Au, G[3]); store8(G[16] + 0x78u, 0u); store8(G[16] + 0x79u, 0u); } G[16] = s0; }
void func_088D59B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = 0xFFFFFFFFu; G[6] = 0xFFFFFFFFu; store8(G[3] + 0x4Eu, 0u); G[5] = 0u; G[7] = 4u; G[3] = 0x40u; do { G[2] = word(rt, G[4]) + G[3]; G[5] += 1u; store16(G[2], G[6]); G[3] += 2u; } while (G[5] != G[7]); }
void func_088DC784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[5]; G[8] = G[4]; if (static_cast<std::int32_t>(G[5]) < 0) return; G[6] = 0u; G[9] = 1u; do { G[5] = word(rt, G[8] + 0x130u); G[2] = 1u << (G[6] & 31u); G[6] += 1u; G[3] = load8(G[5] + 0xEu); G[4] = static_cast<std::int32_t>(G[7]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[2] |= G[3]; store8(G[5] + 0xEu, G[2]); } while (G[4] == 0u); }
void func_088E4B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[4] = G[5]; G[16] = word(rt, G[2] + 0x124u); G[6] = G[16] << 1; if (G[16] != 0u) { G[5] = word(rt, G[2] + 0x120u); call_here(rt, ctx, 0x08B79A88u); } G[2] = G[16]; } G[16] = s0; }
void func_088EBCFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[6] + 0x538u); put(rt, G[6] + 0x55Cu, G[5]); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x74u); G[16] = G[6]; call_here(rt, ctx, G[2]); put(rt, G[16] + 0x11Cu, G[2]); } G[16] = s0; }
void func_088ECAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B08AC0u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x088D5028u); }
void func_088F0FBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[6] = 0x40u; G[16] = G[4]; call_here(rt, ctx, 0x08B36018u); G[3] = 0xFFFFFFFFu; store16(G[16] + 0x34u, G[3]); store16(G[16] + 0x1Cu, G[3]); store16(G[16] + 0x1Eu, G[3]); } G[16] = s0; }
void func_0891352C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; call_here(rt, ctx, 0x08912F40u); G[4] = G[16] + 0xB0u; call_here(rt, ctx, 0x08AB93B4u); store8(G[16] + 0x29u, 0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B3457Cu); }
void func_0891FF58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = word(rt, G[4] + 0x1FCu); G[3] = (1u << (G[4] & 31u)) & 0x3003C198u; G[2] = 0x3003C198u; G[4] = G[4] < 0x1Eu ? 1u : 0u; put(rt, G[5] + 0xACu, 0u); if (G[4] == 0u) return; G[2] = 0x500u; if (G[3] == 0u) return; put(rt, G[5] + 0xACu, G[2]); }
void func_08930074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089364ACu); }
void func_08931BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67A8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08931B20u); G[4] = word(rt, G[16] + 4u); call_here(rt, ctx, 0x08937B04u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08938FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67F8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08938F18u); G[4] = G[16]; call_here(rt, ctx, 0x08938F50u); G[4] = G[16] + 0xA0u; } G[16] = s0; jump(ctx, 0x0893A068u); }
void func_0893A02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC6808u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08939FECu); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_0895064C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = sext(G[4], 8u); G[3] = 0x08CDE0A0u; G[5] = 0u; G[6] = 8u; G[2] = sext(load8(G[3]), 8u); for (;;) { G[3] += 1u; if (G[2] == G[4]) { G[2] = sext(G[5], 8u); return; } G[5] += 1u; if (G[5] == G[6]) break; G[2] = sext(load8(G[3]), 8u); } G[2] = G[4]; }
void func_0898D8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 0u; if (G[5] == 0u) return; if (static_cast<std::int32_t>(G[5]) >= 0) { call_here(rt, ctx, 0x0898D8A0u); return; } G[5] = 0u - G[5]; call_here(rt, ctx, 0x0898D854u); }
void func_0899D06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x124u); if (!(ctx.fpr[12] < ctx.fpr[1])) ctx.fpr[1] = ctx.fpr[12]; ctx.fpr[0] = float_at(rt, G[4] + 0x128u); if (!(ctx.fpr[0] < ctx.fpr[1])) ctx.fpr[0] = ctx.fpr[1]; G[2] = word(rt, G[4] + 0x13Cu); put(rt, G[4] + 0x120u, bits_of(ctx.fpr[0])); G[2] |= 0x60u; put(rt, G[4] + 0x13Cu, G[2]); }
void func_0899D108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x124u); if (!(ctx.fpr[12] < ctx.fpr[1])) ctx.fpr[1] = ctx.fpr[12]; ctx.fpr[0] = float_at(rt, G[4] + 0x128u); if (!(ctx.fpr[0] < ctx.fpr[1])) ctx.fpr[0] = ctx.fpr[1]; G[2] = word(rt, G[4] + 0x13Cu); put(rt, G[4] + 0x14Cu, bits_of(ctx.fpr[0])); G[2] |= 0x40u; put(rt, G[4] + 0x13Cu, G[2]); }
void func_089AB3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089AB370u); G[2] = word(rt, frame.at(0u)); G[4] = 0x08CA50C4u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_089C1674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCEFF8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_089C4180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0897B108u); G[2] = 0x08CCD338u; G[4] = G[16] + 0x208u; put(rt, G[16], G[2]); call_here(rt, ctx, 0x08B22A74u); G[4] = G[16] + 0x240u; } G[16] = s0; jump(ctx, 0x08A187BCu); }
void func_089C4AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B55100u); put(rt, G[16], G[2]); call_here(rt, ctx, 0x08B55108u); put(rt, G[17], G[2]); } G[16] = s0; G[17] = s1; }
void func_089D027C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x089BD668u); G[5] = G[2]; G[4] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089CFE68u); }
void func_089D7294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = 0x08CDE8CCu; G[5] = (G[3] << 4) - (G[3] << 2) + G[2]; G[3] = word(rt, G[5]); ctx.fpr[16] = float_at(rt, G[4] + 0x1Cu); ctx.fpr[13] = 0.0f; G[1] = 0x3F800000u; ctx.fpr[14] = 1.0f; G[25] = G[3]; ctx.fpr[15] = float_at(rt, G[4] + 0x18u); jump(ctx, G[25]); }
void func_089E4B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCF8B0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x089FE3F4u); G[4] = G[16]; G[5] = 0x20u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_089E556C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load16(G[4] + 0x128u), 16u); ctx.fpr[3] = float_at(rt, G[4] + 0x160u); ctx.fpr[4] = 0.0f; ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[2])); G[2] = word(rt, G[4] + 0x154u); G[3] = G[2] - 1u; ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = ctx.fpr[3] + ctx.fpr[2]; if (static_cast<std::int32_t>(G[2]) > 0) { ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[4] = ctx.fpr[3] + ctx.fpr[0]; } ctx.fpr[0] = ctx.fpr[4]; }
void func_089E55A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load16(G[4] + 0x12Au), 16u); ctx.fpr[3] = float_at(rt, G[4] + 0x164u); ctx.fpr[4] = 0.0f; ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[2])); G[2] = word(rt, G[4] + 0x158u); G[3] = G[2] - 1u; ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = ctx.fpr[3] + ctx.fpr[2]; if (static_cast<std::int32_t>(G[2]) > 0) { ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[4] = ctx.fpr[3] + ctx.fpr[0]; } ctx.fpr[0] = ctx.fpr[4]; }
void func_089FE390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCFD98u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[16]; G[5] = 0x14u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A035DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A02B98u); G[2] = 0x08CBB300u; G[3] = 0x08CBB0A8u; put(rt, G[16] + 0x38u, G[2]); put(rt, G[16], G[3]); } G[16] = s0; }
void func_08A1804C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[10] = G[5]; G[4] = G[5]; G[3] = word(rt, G[2]); G[9] = G[7]; G[5] = G[7]; G[8] = 0u; G[7] = 0u; if (G[3] != 0u) { G[4] = G[10]; G[8] = 0u; } else { G[5] = G[6]; G[6] = G[9]; } jump(ctx, 0x0896A35Cu); }
void func_08A20A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD0918u; put(rt, G[16], G[2]); put(rt, G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x18u, 0u); } G[16] = s0; }
void func_08A34F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x18u); G[4] = 0x08CACEC8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A3A788u); } G[16] = s0; }
void func_08A35088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CACEC8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); ctx.fpr[12] = 0.0f; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A3A4CCu); }
void func_08A37D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD16E0u; put(rt, G[16], G[2]); G[5] = 0x12345678u; G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08B1EDE0u); }
void func_08A38BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[4] + 0x14u); ctx.fpr[0] = float_at(rt, G[4] + 0x18u); G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; const bool empty = ctx.fpr[0] <= ctx.fpr[2]; ctx.fpr[3] = ctx.fpr[0] - ctx.fpr[2]; if (!empty) { ctx.fpr[1] = float_at(rt, G[4] + 0x1Cu); ctx.fpr[0] = float_at(rt, G[4] + 0x20u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[2]; ctx.fpr[1] = ctx.fpr[0] / ctx.fpr[3]; } ctx.fpr[0] = ctx.fpr[1]; }
void func_08A38F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; put(rt, G[4] + 0x14u, bits_of(ctx.fpr[12])); ctx.fpr[12] = 0.0f; put(rt, G[4] + 0x18u, bits_of(ctx.fpr[13])); call_here(rt, ctx, 0x08A38D28u); G[4] = G[16]; call_here(rt, ctx, 0x08A38BE0u); put(rt, G[16] + 0x2Cu, bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_08A3EA24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD0F70u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = 0u; G[6] = 0x82u; } G[16] = s0; jump(ctx, 0x08B36018u); }
void func_08A3EADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1080u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = 0u; G[6] = 0x24u; } G[16] = s0; jump(ctx, 0x08B36018u); }
void func_08A44460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBE340u; put(rt, G[16], G[2]); store8(G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x18u, 0u); } G[16] = s0; }
void func_08A4455C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 0x1Cu); G[16] = G[4]; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x14u); put(rt, G[16] + 0x14u, 0u); call_here(rt, ctx, 0x08B35254u); store8(G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x18u, 0u); } } G[16] = s0; }
void func_08A527E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[3] + 0x124u); G[16] = G[5]; call_here(rt, ctx, G[2]); G[2] -= 1u; G[16] += G[2]; G[2] = ~G[2]; G[2] &= G[16]; } G[16] = s0; }
void func_08A68E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[5] = word(rt, G[5]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x108u); G[6] = word(rt, G[5] - 0xCu); call_here(rt, ctx, G[2]); G[4] = G[16]; G[5] = 0xAu; } G[16] = s0; jump(ctx, 0x08A68D78u); }
void func_08A7CB20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A91A38u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A7C5C8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A9AAD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4] + 0x14u; G[3] = word(rt, G[4] + 0x14u); G[2] = word(rt, G[7] + 4u); G[6] = G[5]; G[5] <<= 2; G[2] = sra(G[2] - G[3], 2u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] += G[3]; G[4] = G[7]; if (G[2] == 0u) return; jump(ctx, 0x08C2E0ACu); }
void func_08A9E268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[16] = G[4]; G[3] = word(rt, G[5] + 0x24u); G[2] = G[3] + 4u; put(rt, G[5] + 0x24u, G[2]); G[5] = word(rt, G[3]); call_here(rt, ctx, 0x08B85EB8u); G[2] = G[16]; } G[16] = s0; }
void func_08AA6298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x08AA623Cu); put(rt, G[16], G[2]); G[2] = G[17]; put(rt, G[16] + 4u, G[3]); } G[16] = s0; G[17] = s1; }
void func_08ABDCBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = G[4]; G[6] = 0x10u; G[4] = 0x08CFFB2Cu; G[16] = 0x08CFFB2Cu; call_here(rt, ctx, 0x08B7C830u); G[2] = high_half(0x08CFFB24u); put(rt, 0x08CFFB24u, G[16]); } G[16] = s0; }
void func_08AC0124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6450u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AC00D8u); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08ACA460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x43554F56u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; put(rt, G[16] + 4u, 0u); call_here(rt, ctx, 0x08ACA300u); G[4] = G[16] + 0x22Cu; } G[16] = s0; jump(ctx, 0x08ACA3B8u); }
void func_08AD1DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 0x5Cu); G[16] = G[4]; if (G[2] == 0u) { G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x54u); call_here(rt, ctx, G[3]); } put(rt, G[16] + 0x44u, 0u); } G[16] = s0; }
void func_08AD1DEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6C70u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AD1DB0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB8888u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08AD5358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x10u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08ADBA04u); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x10u, G[2]); } } G[16] = s0; }
void func_08AD589C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6EA0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AD5834u); G[4] = G[16]; call_here(rt, ctx, 0x08AD7DF8u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08AE4B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AE4A7Cu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08AE35BCu); G[3] = G[2]; G[2] = G[3] == 0u ? 0u : 3u; } G[16] = s0; }
void func_08AE9EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D40A3Cu); G[3] = word(rt, 0x08D40A3Cu); G[5] = (((G[4] << 3) - G[4]) << 3) + G[3]; G[6] = word(rt, G[5] + 0x30u); G[7] = word(rt, G[5] + 0x34u); G[2] = word(rt, G[5] + 0x28u); G[3] = word(rt, G[5] + 0x2Cu); G[4] = G[2] < G[6] ? 1u : 0u; G[3] -= G[7]; G[2] -= G[6]; G[3] -= G[4]; }
void func_08AEEEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; G[3] = G[7]; G[11] = G[8]; G[10] = G[9]; G[6] = G[5]; G[7] = G[2]; G[8] = G[3]; G[9] = G[11]; G[5] = 2u; call_with(rt, ctx, 0x08AEED10u, 0x10u); }
void func_08AF569C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AF5668u); G[3] = G[2] << 7; G[4] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) G[4] = word(rt, G[16] + 0x9Cu) + G[3]; G[2] = G[4]; } G[16] = s0; }
void func_08AFB02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD76F8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AFAC58u); G[2] = 0x08CBF428u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08AFB160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD78A0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AFAC7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08AFB12Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08AFB204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7848u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AFAC7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08AFB12Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08AFB360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD7798u; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[4], G[2]); put(rt, G[4] + 0x24u, G[1]); for (std::uint32_t offset = 4u; offset < 0x24u; offset += 4u) put(rt, G[4] + offset, 0u); }
void func_08B08AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[5] = 0u; G[6] = 0x1ACu; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B36018u); G[3] = word(rt, G[16]); G[2] = 0u; put(rt, G[3] + 0x1A8u, 0u); } G[16] = s0; }
void func_08B0E31C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 4u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17]; store8(G[16] + 0xE8u, 0u); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34B24u); }
void func_08B0EC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B17F10u); call_here(rt, ctx, 0x08B17F00u); call_here(rt, ctx, 0x08B10FE8u); call_here(rt, ctx, 0x08B17F08u); } jump(ctx, 0x08B17F18u); }
void func_08B10474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[16] = G[4]; store8(G[4] + 0x9Cu, 1u); G[4] += 0x64u; call_here(rt, ctx, 0x08B28C74u); G[4] = word(rt, G[16] + 0x90u); call_here(rt, ctx, 0x08B2E224u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B103ECu); }
void func_08B10A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0xA8u); G[16] = G[4]; if (G[2] != 0u) call_here(rt, ctx, 0x08B106ACu); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0x20u); } G[16] = s0; jump(ctx, G[25]); }
void func_08B11E3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; G[6] = word(rt, G[16] + 0x264u); G[4] = word(rt, G[4] + 0x268u); call_here(rt, ctx, 0x08B36018u); G[6] = word(rt, G[16] + 0x264u); G[4] = word(rt, G[16] + 0x26Cu); G[5] = 0u; } G[16] = s0; jump(ctx, 0x08B36018u); }
void func_08B1222C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; if (G[6] == 0u) return; G[2] = high_half(0x08C92BC0u); G[9] = 0x08C92BC0u; G[7] = 0u; do { G[2] = G[5] + G[7]; G[3] = load8(G[2]); G[4] = G[8] + G[7]; G[7] += 1u; G[3] += G[9]; G[2] = load8(G[3]); store8(G[4], G[2]); } while (G[6] != G[7]); }
void func_08B19390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08B1935Cu); G[5] = G[2]; G[4] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B18BCCu); }
void func_08B19FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B34888u); G[2] = 1u; G[4] = G[16]; store8(G[16] + 0x2Cu, 1u); call_here(rt, ctx, 0x08B34CA0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B34B24u); }
void func_08B1C104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 1u << (G[5] & 31u); G[6] = word(rt, G[4]); G[3] = G[7] | G[6]; G[5] = 0u; put(rt, G[4], G[3]); G[2] = 1u; }
void func_08B1C324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 1u << (G[5] & 31u); G[6] = word(rt, G[4]); G[3] = G[7] | G[6]; G[5] = 0u; put(rt, G[4], G[3]); G[2] = 1u; }
void func_08B1C7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9340u; G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B1C68Cu); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08B34DA0u); }
void func_08B1EFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), bits_of(f21)); ctx.fpr[21] = ctx.fpr[12]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[13]; call_here(rt, ctx, 0x08B1EF70u); ctx.fpr[20] = ctx.fpr[20] - ctx.fpr[21]; ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[21] + ctx.fpr[20]; } ctx.fpr[21] = f21; ctx.fpr[20] = f20; }
void func_08B2171C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B21710u); G[6] = load16(G[17]); G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B79A88u); }
void func_08B21858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B217A0u); G[6] = load16(G[17]); G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B79A88u); }
void func_08B21CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D68B7Cu); G[4] = 0x08D68B7Cu; G[5] = 0u; G[6] = 0u; G[7] = 0u; G[8] = 0u; call_here(rt, ctx, 0x08B215B8u); G[4] = 0x08D68B7Cu; } G[16] = s0; jump(ctx, 0x08B21C74u); }
void func_08B28F50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[0] = float_at(rt, G[4] + 4u); ctx.fpr[13] = psp_mul(ctx.fpr[12], ctx.fpr[13]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[12]); ctx.fpr[2] = float_at(rt, G[4] + 8u); G[2] = G[4]; put(rt, G[4], 0u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[13] = psp_mul(ctx.fpr[0], ctx.fpr[13]); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[13]; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[12]; put(rt, G[4] + 8u, bits_of(ctx.fpr[2])); }
void func_08B2C3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDA710u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B2C2E0u); G[4] = G[16] + 0x58u; call_here(rt, ctx, 0x08B2C2D8u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B41320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 0x18u; G[4] += 0x8DCu; if (G[8] != G[2]) return; G[6] = word(rt, G[7] + 8u); G[7] = word(rt, G[7] + 0xCu); call_here(rt, ctx, 0x08B3F098u); G[3] = G[2]; if (G[2] == 0u) return; G[2] = 1u; store8(G[3] + 5u, 1u); }
void func_08B41F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFFFu; G[5] = sext(G[17], 16u); G[16] = G[4]; call_here(rt, ctx, 0x08B41E58u); G[4] = G[16] + 0x8DCu; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3F4D0u); }
void func_08B46880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x4898u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08C7AEE8u); G[3] = 0xFFFFFFFFu; put(rt, G[16] + 0x4898u, G[3]); } } G[16] = s0; }
void func_08B51F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B51EE8u); G[3] = G[16] + 4u; if (G[2] == 0u) { G[2] = 1u; store8(G[16] + 0x24u, 1u); G[3] = 0u; } G[2] = G[3]; } G[16] = s0; }
void func_08B56384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0xD10u); G[2] = word(rt, G[4] + 0xD18u); G[3] = word(rt, G[4] + 0xD1Cu); G[5] <<= 2; G[8] = word(rt, G[4] + 0xD20u); G[9] = word(rt, G[4] + 0xD24u); G[5] += G[4]; G[6] = G[4]; G[4] = word(rt, G[5] + 0xD00u); G[2] |= G[8]; G[3] |= G[9]; put(rt, G[6] + 0xD18u, G[2]); G[5] = G[4]; put(rt, G[6] + 0xD1Cu, G[3]); jump(ctx, 0x08B57F2Cu); }
void func_08B5B48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAE40u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5B4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAE40u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5E9F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAEC8u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5EA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAEC8u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5EE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAEE0u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5EE50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAEE0u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C7464Cu);  } G[16] = s0; G[17] = s1; }
void func_08B5C018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; G[8] = G[5]; if (G[4] == G[6]) return; G[3] = word(rt, G[6] + 4u); G[5] = word(rt, G[5] + 4u); G[4] = word(rt, G[4] + 4u); put(rt, G[7] + 4u, G[3]); G[2] = word(rt, G[8] + 4u); put(rt, G[3], G[7]); put(rt, G[6] + 4u, G[2]); put(rt, G[5], G[6]); put(rt, G[8] + 4u, G[4]); put(rt, G[4], G[8]); }
void func_08B617A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B61664u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B61484u); }
void func_08B77A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D72D08u); G[3] = word(rt, 0x08D72D08u); G[4] = G[3]; if (G[3] != 0u) { call_here(rt, ctx, 0x08C7A978u); put(rt, 0x08D72D08u, 0u); } } G[16] = s0; }
void func_08B78040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B77F84u); }
void func_08B78750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B7865Cu); }
void func_08B79794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B796E8u); }
void func_08B831BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08D740ECu); G[6] = G[4] << 2; G[8] = 0x08D740ECu; GuestFrame frame(ctx, 0x10u); G[3] = G[6] + G[8]; G[7] = 0x80440000u; G[6] = G[4] < 4u ? 1u : 0u; G[2] = 0x80440010u; if (G[6] == 0u) return; G[4] = word(rt, G[3]); call_here(rt, ctx, 0x08C7ABC0u); }
void func_08B842AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = high_half(0x08D740F8u); G[6] = word(rt, 0x08D740F8u); GuestFrame frame(ctx, 0x10u); G[3] = 0x80420000u; G[5] = G[4]; G[4] = 0x08D74100u; G[2] = 0x80420100u; if (G[6] == 0u) return; call_here(rt, ctx, 0x08C7B070u); }
void func_08B88B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B1E334u); G[2] = 0x08CBCD78u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08B9EE08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0140u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 4u); put(rt, G[2] + 4u, G[4]); } G[16] = s0; }
void func_08B9EE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0058u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 4u); put(rt, G[2] + 4u, G[4]); } G[16] = s0; }
void func_08B9EE80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC02C0u; put(rt, G[2], G[3]); ctx.fpr[0] = float_at(rt, G[16] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_08B9EEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0010u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 4u); put(rt, G[2] + 4u, G[4]); } G[16] = s0; }
void func_08B9EEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0EB8u; put(rt, G[2], G[3]); G[4] = load8(G[16] + 4u); store8(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BB9AA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC3CC8u; put(rt, G[2], G[3]); ctx.fpr[0] = float_at(rt, G[16] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_08BC33B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC4F98u; put(rt, G[2], G[3]); G[4] = load8(G[16] + 4u); store8(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BC33EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC5100u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 4u); put(rt, G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BC3428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC5070u; put(rt, G[2], G[3]); G[4] = load16(G[16] + 4u); store16(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BC3464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC5028u; put(rt, G[2], G[3]); G[4] = load8(G[16] + 4u); store8(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BC34A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC50B8u; put(rt, G[2], G[3]); G[4] = load16(G[16] + 4u); store16(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BC34DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC4FE0u; put(rt, G[2], G[3]); G[4] = load8(G[16] + 4u); store8(G[2] + 4u, G[4]); } G[16] = s0; }
void func_08BE72E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC6CE0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08965A3Cu); G[4] = G[16]; G[5] = 0x34u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08BE90D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; ctx.fpr[1] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5]); G[5] = frame.at(0u); put(rt, frame.at(4u), bits_of(ctx.fpr[1])); put(rt, frame.at(0u), bits_of(ctx.fpr[0])); call_here(rt, ctx, 0x089762F4u); G[2] = G[16]; } G[16] = s0; }
void func_08BE9218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[6] = G[16] + 4u; G[4] = G[5]; G[7] = 0u; G[5] = G[16]; G[8] = 0u; call_here(rt, ctx, 0x08969B98u); G[2] = G[16]; } G[16] = s0; }
void func_08BE93EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[7] = G[16]; G[4] = G[5]; G[8] = G[16] + 4u; G[6] = 0u; G[5] = 0u; call_here(rt, ctx, 0x0896A35Cu); G[2] = G[16]; } G[16] = s0; }
void func_08BE9428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[6] = G[16] + 4u; G[4] = G[5]; G[7] = 0u; G[5] = G[16]; G[8] = 0u; call_here(rt, ctx, 0x0896A35Cu); G[2] = G[16]; } G[16] = s0; }
void func_08C1A47C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B1E334u); G[2] = 0x08CD1D40u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C1A4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B1E334u); G[2] = 0x08CD0E18u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C2D8E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD4610u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A86D24u); G[4] = G[16]; G[5] = 0x14u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08C306D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB7440u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A7D4E4u); G[4] = G[16]; G[5] = 0x38u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08C3C1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B1E334u); G[2] = 0x08CD1DB8u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C3DF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B1E334u); G[2] = 0x08CD1D88u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C5250C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[1] = -1.0f; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; if (ctx.fpr[12] < ctx.fpr[1]) ctx.fpr[12] = ctx.fpr[1]; else if (ctx.fpr[0] < ctx.fpr[12]) ctx.fpr[12] = ctx.fpr[0]; jump(ctx, 0x08B49D98u); }
void func_08C58B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 8u); GuestFrame frame(ctx, 0x10u); G[2] = 0u; G[3] = word(rt, G[5] + 0x30u); put(rt, frame.at(0u), G[3]); G[4] = load8(frame.at(0u)); G[5] = load8(frame.at(1u)); G[6] = load8(frame.at(2u)); G[3] = load8(frame.at(3u)); G[2] = G[4] | G[5] << 8 | G[6] << 16 | G[3] << 24; }
void func_08C58B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 8u); GuestFrame frame(ctx, 0x10u); G[2] = 0u; G[3] = word(rt, G[5] + 0x34u); put(rt, frame.at(0u), G[3]); G[4] = load8(frame.at(0u)); G[5] = load8(frame.at(1u)); G[6] = load8(frame.at(2u)); G[3] = load8(frame.at(3u)); G[2] = G[4] | G[5] << 8 | G[6] << 16 | G[3] << 24; }
void func_08C58DBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 8u); GuestFrame frame(ctx, 0x10u); G[2] = 0u; G[3] = word(rt, G[5] + 0x30u); put(rt, frame.at(0u), G[3]); G[4] = load8(frame.at(0u)); G[5] = load8(frame.at(1u)); G[6] = load8(frame.at(2u)); G[3] = load8(frame.at(3u)); G[2] = G[4] | G[5] << 8 | G[6] << 16 | G[3] << 24; }
void func_08C58DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 8u); GuestFrame frame(ctx, 0x10u); G[2] = 0u; G[3] = word(rt, G[5] + 0x34u); put(rt, frame.at(0u), G[3]); G[4] = load8(frame.at(0u)); G[5] = load8(frame.at(1u)); G[6] = load8(frame.at(2u)); G[3] = load8(frame.at(3u)); G[2] = G[4] | G[5] << 8 | G[6] << 16 | G[3] << 24; }
void func_08C75648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[3] - 4u); G[16] = G[4]; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08C75524u); G[3] = word(rt, G[16]); G[2] = G[3]; } G[16] = s0; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_180
} // namespace gt::game

#define HOT180(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_180::name, gt::ReturnKind::kind)
HOT180(0x0880A574u, func_0880A574, Word);
HOT180(0x0880D6ACu, func_0880D6AC, Word);
HOT180(0x089A2564u, func_089A2564, Word);
HOT180(0x0881FC44u, func_0881FC44, Word);
HOT180(0x0883A49Cu, func_0883A49C, Word);
HOT180(0x08858870u, func_08858870, Word);
HOT180(0x08858958u, func_08858958, Word);
HOT180(0x0885BE34u, func_0885BE34, Word);
HOT180(0x0886B130u, func_0886B130, Word);
// HOT180(0x088757B0u, func_088757B0, Word);  // no comparable synthetic case yet
HOT180(0x088758C0u, func_088758C0, Word);
HOT180(0x0887BE64u, func_0887BE64, Word);
// HOT180(0x0887E978u, func_0887E978, Word);  // no comparable synthetic case yet
HOT180(0x0888FE78u, func_0888FE78, Word);
HOT180(0x08891140u, func_08891140, Word);
HOT180(0x08891290u, func_08891290, Word);
HOT180(0x08892BA4u, func_08892BA4, Word);
HOT180(0x08895544u, func_08895544, Word);
HOT180(0x08895A88u, func_08895A88, Word);
// HOT180(0x0889968Cu, func_0889968C, Word);  // no comparable synthetic case yet
// HOT180(0x0889A054u, func_0889A054, Word);  // no comparable synthetic case yet
HOT180(0x088D43A8u, func_088D43A8, Word);
HOT180(0x088D59B8u, func_088D59B8, Word);
// HOT180(0x088DC784u, func_088DC784, Word);  // no comparable synthetic case yet
HOT180(0x088E4B80u, func_088E4B80, Word);
HOT180(0x088EBCFCu, func_088EBCFC, Word);
HOT180(0x088ECAD0u, func_088ECAD0, Word);
HOT180(0x088F0FBCu, func_088F0FBC, Word);
// HOT180(0x0891352Cu, func_0891352C, Word);  // no comparable synthetic case yet
HOT180(0x0891FF58u, func_0891FF58, Word);
HOT180(0x08930074u, func_08930074, Word);
HOT180(0x08931BFCu, func_08931BFC, Word);
HOT180(0x08938FCCu, func_08938FCC, Word);
HOT180(0x0893A02Cu, func_0893A02C, Word);
HOT180(0x0895064Cu, func_0895064C, Word);
HOT180(0x0898D8D4u, func_0898D8D4, Word);
HOT180(0x0899D06Cu, func_0899D06C, Word);
HOT180(0x0899D108u, func_0899D108, Word);
HOT180(0x089AB3B0u, func_089AB3B0, Word);
HOT180(0x089C1674u, func_089C1674, Word);
HOT180(0x089C4180u, func_089C4180, Word);
HOT180(0x089C4AA0u, func_089C4AA0, Word);
HOT180(0x089D027Cu, func_089D027C, Word);
HOT180(0x089D7294u, func_089D7294, Word);
HOT180(0x089E4B7Cu, func_089E4B7C, Word);
HOT180(0x089E556Cu, func_089E556C, Float);
HOT180(0x089E55A8u, func_089E55A8, Float);
HOT180(0x089FE390u, func_089FE390, Word);
HOT180(0x08A035DCu, func_08A035DC, Word);
HOT180(0x08A1804Cu, func_08A1804C, Word);
HOT180(0x08A20A68u, func_08A20A68, Word);
HOT180(0x08A34F08u, func_08A34F08, Word);
HOT180(0x08A35088u, func_08A35088, Word);
HOT180(0x08A37D94u, func_08A37D94, Word);
HOT180(0x08A38BE0u, func_08A38BE0, Float);
HOT180(0x08A38F98u, func_08A38F98, Word);
HOT180(0x08A3EA24u, func_08A3EA24, Word);
HOT180(0x08A3EADCu, func_08A3EADC, Word);
HOT180(0x08A44460u, func_08A44460, Word);
HOT180(0x08A4455Cu, func_08A4455C, Word);
HOT180(0x08A527E8u, func_08A527E8, Word);
HOT180(0x08A68E00u, func_08A68E00, Word);
// HOT180(0x08A7CB20u, func_08A7CB20, Word);  // no comparable synthetic case yet
HOT180(0x08A9AAD4u, func_08A9AAD4, Word);
HOT180(0x08A9E268u, func_08A9E268, Word);
// HOT180(0x08AA6298u, func_08AA6298, Word);  // no comparable synthetic case yet
HOT180(0x08ABDCBCu, func_08ABDCBC, Word);
// HOT180(0x08AC0124u, func_08AC0124, Word);  // no comparable synthetic case yet
HOT180(0x08ACA460u, func_08ACA460, Word);
HOT180(0x08AD1DB0u, func_08AD1DB0, Word);
HOT180(0x08AD1DECu, func_08AD1DEC, Word);
HOT180(0x08AD5358u, func_08AD5358, Word);
HOT180(0x08AD589Cu, func_08AD589C, Word);
HOT180(0x08AE4B68u, func_08AE4B68, Word);
HOT180(0x08AE9EFCu, func_08AE9EFC, DoubleWord);
// HOT180(0x08AEEEB8u, func_08AEEEB8, Word);  // no comparable synthetic case yet
HOT180(0x08AF569Cu, func_08AF569C, Word);
HOT180(0x08AFB02Cu, func_08AFB02C, Word);
HOT180(0x08AFB160u, func_08AFB160, Word);
HOT180(0x08AFB204u, func_08AFB204, Word);
HOT180(0x08AFB360u, func_08AFB360, Word);
HOT180(0x08B08AE0u, func_08B08AE0, Word);
// HOT180(0x08B0E31Cu, func_08B0E31C, Word);  // no comparable synthetic case yet
// HOT180(0x08B0EC2Cu, func_08B0EC2C, Word);  // no comparable synthetic case yet
// HOT180(0x08B10474u, func_08B10474, Word);  // no comparable synthetic case yet
HOT180(0x08B10A40u, func_08B10A40, Word);
HOT180(0x08B11E3Cu, func_08B11E3C, Word);
HOT180(0x08B1222Cu, func_08B1222C, Word);
HOT180(0x08B19390u, func_08B19390, Word);
// HOT180(0x08B19FB0u, func_08B19FB0, Word);  // no comparable synthetic case yet
HOT180(0x08B1C104u, func_08B1C104, Word);
HOT180(0x08B1C324u, func_08B1C324, Word);
HOT180(0x08B1C7A8u, func_08B1C7A8, Word);
HOT180(0x08B1EFBCu, func_08B1EFBC, Float);
HOT180(0x08B2171Cu, func_08B2171C, Word);
HOT180(0x08B21858u, func_08B21858, Word);
HOT180(0x08B21CA4u, func_08B21CA4, Word);
HOT180(0x08B28F50u, func_08B28F50, Word);
HOT180(0x08B2C3F0u, func_08B2C3F0, Word);
HOT180(0x08B41320u, func_08B41320, Word);
// HOT180(0x08B41F70u, func_08B41F70, Word);  // no comparable synthetic case yet
HOT180(0x08B46880u, func_08B46880, Word);
HOT180(0x08B51F38u, func_08B51F38, Word);
// HOT180(0x08B56384u, func_08B56384, Word);  // no comparable synthetic case yet
HOT180(0x08B5B48Cu, func_08B5B48C, Word);
HOT180(0x08B5B4F4u, func_08B5B4F4, Word);
HOT180(0x08B5E9F8u, func_08B5E9F8, Word);
HOT180(0x08B5EA48u, func_08B5EA48, Word);
HOT180(0x08B5EE00u, func_08B5EE00, Word);
HOT180(0x08B5EE50u, func_08B5EE50, Word);
// HOT180(0x08B5C018u, func_08B5C018, Word);  // no comparable synthetic case yet
// HOT180(0x08B617A8u, func_08B617A8, Word);  // no comparable synthetic case yet
HOT180(0x08B77A54u, func_08B77A54, Word);
HOT180(0x08B78040u, func_08B78040, Word);
HOT180(0x08B78750u, func_08B78750, Word);
HOT180(0x08B79794u, func_08B79794, Word);
HOT180(0x08B831BCu, func_08B831BC, Word);
HOT180(0x08B842ACu, func_08B842AC, Word);
HOT180(0x08B88B64u, func_08B88B64, Word);
HOT180(0x08B9EE08u, func_08B9EE08, Word);
HOT180(0x08B9EE44u, func_08B9EE44, Word);
HOT180(0x08B9EE80u, func_08B9EE80, Word);
HOT180(0x08B9EEBCu, func_08B9EEBC, Word);
HOT180(0x08B9EEF8u, func_08B9EEF8, Word);
HOT180(0x08BB9AA8u, func_08BB9AA8, Word);
HOT180(0x08BC33B0u, func_08BC33B0, Word);
HOT180(0x08BC33ECu, func_08BC33EC, Word);
HOT180(0x08BC3428u, func_08BC3428, Word);
HOT180(0x08BC3464u, func_08BC3464, Word);
HOT180(0x08BC34A0u, func_08BC34A0, Word);
HOT180(0x08BC34DCu, func_08BC34DC, Word);
HOT180(0x08BE72E0u, func_08BE72E0, Word);
HOT180(0x08BE90D8u, func_08BE90D8, Word);
HOT180(0x08BE9218u, func_08BE9218, Word);
HOT180(0x08BE93ECu, func_08BE93EC, Word);
HOT180(0x08BE9428u, func_08BE9428, Word);
HOT180(0x08C1A47Cu, func_08C1A47C, Word);
HOT180(0x08C1A4FCu, func_08C1A4FC, Word);
HOT180(0x08C2D8E0u, func_08C2D8E0, Word);
HOT180(0x08C306D0u, func_08C306D0, Word);
HOT180(0x08C3C1F4u, func_08C3C1F4, Word);
HOT180(0x08C3DF3Cu, func_08C3DF3C, Word);
HOT180(0x08C5250Cu, func_08C5250C, Word);
HOT180(0x08C58B24u, func_08C58B24, Word);
HOT180(0x08C58B60u, func_08C58B60, Word);
HOT180(0x08C58DBCu, func_08C58DBC, Word);
HOT180(0x08C58DF8u, func_08C58DF8, Word);
HOT180(0x08C75648u, func_08C75648, Word);
