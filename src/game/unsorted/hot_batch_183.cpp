// Hot batch 183: eighteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_183 {
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

void func_088095C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[7] = 1u; G[16] = G[4]; G[8] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08808F2Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_0881AE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0881AD9Cu); G[5] = G[2] + 1u; G[4] = G[16]; G[6] = 1u; G[3] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x0881ACC0u); G[3] = 1u; } G[2] = G[3]; } G[16] = s0; }
void func_0882A2D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[4] = frame.at(0u); if (G[6] != 0u) return; call_here(rt, ctx, 0x088297A8u); G[2] = word(rt, frame.at(0u)); G[4] = 0x08C9A2F8u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_0882EA2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = high_half(0x08CE0A00u); G[5] = word(rt, 0x08CE0A00u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_0884C4E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBD348u; G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; put(rt, G[16], G[2]); put(rt, G[16] + 0x18u, G[4]); put(rt, G[16] + 0x1Cu, G[5]); store16(G[16] + 0x22u, 0u); store16(G[16] + 0x20u, 0u); } G[16] = s0; }
void func_0885E4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] << 6; G[3] = word(rt, G[4]); G[5] = (G[5] << 4) + G[2] + 0x4120u; G[3] += G[5]; G[2] = load8(G[3] + 0x17u); G[4] = 0u; if (G[2] != 0u) { G[2] = load8(G[3] + 0x18u); G[4] = 1u; if (G[2] != 0u) G[4] = 0u; else { G[2] = load8(G[3] + 0x1Cu); if (G[2] == 0u) G[4] = 0u; } } G[2] = G[4]; }
void func_088663EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B35254u); put(rt, G[16], 0u); G[2] = 0u; G[4] = 0xFFFFFFFFu; G[3] = 0x40u; do { G[2] += 1u; put(rt, G[16] + 4u, G[4]); G[16] += 4u; } while (G[2] != G[3]); } G[16] = s0; }
void func_0886AEDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[3] = 0u; if (G[5] != 0u) { call_here(rt, ctx, 0x0886A948u); G[4] = G[2]; G[5] = G[16]; call_here(rt, ctx, 0x08B7B8C4u); G[3] = G[2] == 0u ? 1u : 0u; } G[2] = G[3]; } G[16] = s0; }
void func_0886BD00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[5] = word(rt, G[5]) + 0x64u; G[4] = word(rt, G[4] + 4u) + 0x88u; call_here(rt, ctx, 0x0887D364u); G[4] = word(rt, G[16] + 4u); G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B1FC48u); }
void func_08875878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[5] = 0u; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x0886B010u); G[5] = 0u; if (G[2] != 0u) { G[4] = word(rt, G[16]); tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x0886B060u); }
void func_08875974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x0886BAECu); G[3] = 1u; if (G[2] == 0u) { G[4] = word(rt, G[16]); call_here(rt, ctx, 0x0886B120u); G[3] = G[2] != 0u ? 1u : 0u; } G[2] = G[3]; } G[16] = s0; }
void func_088765D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; put(rt, G[4], G[5]); G[4] += 0x10u; put(rt, G[16] + 4u, 0u); put(rt, G[16] + 8u, 0u); call_here(rt, ctx, 0x08AD07B8u); G[4] = G[16] + 0x3E0u; call_here(rt, ctx, 0x08AD04ACu); G[4] = G[16] + 0x814u; call_here(rt, ctx, 0x0887BE64u); G[4] = G[16] + 0x82Cu; } G[16] = s0; jump(ctx, 0x088763E8u); }
void func_08891708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); jump(ctx, 0x088916C8u); }
void func_08893C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 8u); G[4] = G[3]; if (G[3] != 0u) { call_here(rt, ctx, 0x08B35254u); G[2] = word(rt, G[16]); put(rt, G[2] + 8u, 0u); G[3] = word(rt, G[16]); put(rt, G[3] + 4u, 0u); } } G[16] = s0; }
void func_08898EE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x088F8884u); G[2] = 0x08CC2020u; put(rt, G[16], G[2]); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08897B8Cu); G[2] = 0x08CC2190u; put(rt, G[16], G[2]); store8(G[16] + 0x50u, 0u); } G[16] = s0; }
void func_088A2544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 4u) + 4u; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16] + 4u); put(rt, G[16] + 0xCu, G[17]); G[4] += 4u; put(rt, G[16] + 8u, 0u); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_088A2A1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5]; G[3] = word(rt, G[4] + 0xCu) + G[5]; G[7] = word(rt, G[4] + 4u); G[4] = sext(load8(G[3]), 8u); G[5] = G[6]; G[3] = ~G[4]; if (G[3] != 0u) G[2] = G[4]; G[4] = (G[2] << 7) - (G[2] << 2) + G[7]; G[2] <<= 2; call_with(rt, ctx, 0x088A2934u, 0x10u); }
void func_088A5AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[7] = G[5]; G[2] = word(rt, G[3] + 0x4C8u); G[8] = G[2] + 0x1C30u; G[6] = G[2] + 0x1B30u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); }
void func_088A627C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4]); G[2] = load8(G[3] + 0x4CCu); G[16] = G[4]; if (G[2] != 0u) { store8(G[3] + 0x4CCu, 0u); call_here(rt, ctx, 0x088A5A0Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088A61A4u); G[3] = word(rt, G[16]); G[2] = 1u; store8(G[3] + 0x4CCu, 1u); } G[16] = s0; }
void func_088AC0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[1] = 0x427B53D2u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = std::bit_cast<float>(bits_of(ctx.fpr[12]) & 0x7FFFFFFFu); if (ctx.fpr[0] < ctx.fpr[1]) { ctx.fpr[0] = ctx.fpr[12]; return; } ctx.fpr[0] = 0.0f; const bool negative = ctx.fpr[12] < ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[1]; if (negative) { G[1] = 0xC27B53D2u; ctx.fpr[12] = std::bit_cast<float>(G[1]); } ctx.fpr[0] = ctx.fpr[12]; }
void func_088C6304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[16] = G[4]; G[3] = word(rt, G[5] + 8u); put(rt, G[4] + 4u, G[3]); ctx.fpr[0] = float_at(rt, G[5]); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); G[7] = word(rt, G[5] + 0x10u); G[6] = word(rt, G[2] + 0xCu); G[5] = word(rt, G[5] + 4u); call_here(rt, ctx, 0x088C5BF0u); store8(G[16] + 0x14u, G[2]); } G[16] = s0; }
void func_088E51D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x088E511Cu); G[5] = G[16]; if (G[2] != 0u) { G[4] = word(rt, G[17]); call_here(rt, ctx, 0x088ECB44u); } G[2] = word(rt, G[17]); store8(G[2] + 0x514u, 0u); } G[16] = s0; G[17] = s1; }
void func_088E82E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; if (G[5] != G[2]) { G[2] = (G[5] << 2) + G[4]; G[5] = word(rt, G[2] + 0x2A4u); G[2] = G[5]; return; } G[5] = 0u; G[3] = 0u; G[6] = 3u; do { G[2] = word(rt, G[4] + 0x2A4u); G[3] += 1u; G[4] += 4u; G[5] += G[2]; } while (G[3] != G[6]); G[2] = G[5]; }
void func_088E8688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x2E4u); if (G[2] == 0u) return; G[2] = word(rt, G[4] + 0x24u); if (G[2] == 0u) return; G[2] = word(rt, G[4] + 0x1Cu); if (G[2] != 0u) return; call_here(rt, ctx, 0x088E7EA0u); G[3] = 1u; store8(G[2] + 5u, 1u); }
void func_088EAA7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x088EAFA8u); G[2] = 0x08CC3800u; put(rt, G[16], G[2]); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = 5u; call_here(rt, ctx, 0x08B0A458u); put(rt, G[16] + 0x538u, G[2]); } G[16] = s0; }
void func_088F1310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[4] + 4u; G[16] = G[4]; G[3] = 2u; do { G[3] -= 1u; put(rt, G[2], 0u); put(rt, G[2] + 4u, 0u); G[2] += 8u; } while (G[3] != 0u); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x088F0FBCu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x088F1308u); }
void func_088F77AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[1] = -ctx.fpr[13]; if (!(ctx.fpr[12] < ctx.fpr[1])) { ctx.fpr[0] = 0.0f; if (ctx.fpr[12] < ctx.fpr[0]) ctx.fpr[12] = ctx.fpr[1]; else if (ctx.fpr[12] < ctx.fpr[13]) ctx.fpr[12] = ctx.fpr[13]; } ctx.fpr[0] = ctx.fpr[12]; }
void func_088F7BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[6] & 0xFFu; G[6] = G[5]; G[16] = G[4]; G[5] = G[4] + 0xECu; if (G[2] != 0u) store8(G[4] + 0xFDu, 0u); call_here(rt, ctx, 0x088F8734u); G[2] = word(rt, G[16] + 0x100u) | 0x800u; put(rt, G[16] + 0x100u, G[2]); } G[16] = s0; }
void func_0892981C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 8u); G[4] = frame.at(0u); G[5] = G[2]; call_here(rt, ctx, 0x08928194u); ctx.fpr[2] = float_at(rt, frame.at(4u)); ctx.fpr[1] = float_at(rt, frame.at(8u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); put(rt, G[16] + 4u, bits_of(ctx.fpr[2])); put(rt, G[16], bits_of(ctx.fpr[0])); put(rt, G[16] + 8u, bits_of(ctx.fpr[1])); } G[16] = s0; }
void func_0892A37C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] += 4u; G[16] = G[5]; G[2] = word(rt, G[5]); put(rt, G[17], G[2]); G[5] = word(rt, G[5] + 4u); call_here(rt, ctx, 0x0892A1F4u); G[5] = word(rt, G[16] + 8u); G[4] = G[17] + 0x10u; } G[16] = s0; G[17] = s1; jump(ctx, 0x0892A2C0u); }
void func_0893205C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x18u); if (G[2] != 0u) { G[3] = word(rt, G[2]); G[2] = G[3]; G[16] = s0; return; } G[6] = word(rt, G[4] + 8u); G[7] = word(rt, G[4] + 0xCu); call_here(rt, ctx, 0x08931FF0u); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x18u); G[3] = word(rt, G[2]); } G[2] = G[3]; } G[16] = s0; }
void func_08936894(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = (G[6] << 2) + G[4]; G[6] = G[6] < 0x2Cu ? 1u : 0u; if (G[6] != 0u) { G[2] = word(rt, G[2] + 0x13Cu); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08938684u); G[2] = G[2] + 1u != 0u ? 1u : 0u; return; } } G[2] = 0u; }
void func_08939FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC6808u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08B28DCCu); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0xCu, G[2]); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 4u, G[2]); put(rt, G[16] + 8u, G[2]); } G[16] = s0; }
void func_08947B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CF2EC0u); ctx.fpr[1] = float_at(rt, 0x08CF2EC0u); G[3] = word(rt, G[4] + 8u); G[4] = 0x64u; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[12]); G[6] = G[3] + 0x778u; G[2] = sext(load8(G[6] + 5u), 8u); G[3] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[1])); ctx.fpr[0] = std::bit_cast<float>(G[3]); G[2] += G[3]; G[3] = static_cast<std::int32_t>(G[2]) < 0x65 ? 1u : 0u; G[5] = sext(G[2], 8u); G[2] = static_cast<std::int32_t>(G[2]) < -0x64 ? 1u : 0u; if (G[3] != 0u) { G[4] = 0xFFFFFF9Cu; if (G[2] == 0u) G[4] = G[5]; } store8(G[6] + 5u, G[4]); }
void func_0896457C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); if (G[5] == 0u) { call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; }
void func_0896716C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[4] = 1u; if (G[2] == 0u) G[4] = 0u; else { G[3] = word(rt, G[16] + 0x80u); G[2] = word(rt, G[3] + 0x30u) & 0x400u; if (G[2] == 0u) G[4] = 0u; } G[2] = G[4]; } G[16] = s0; }
void func_089671B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[4] = 1u; if (G[2] == 0u) G[4] = 0u; else { G[3] = word(rt, G[16] + 0x80u); G[2] = word(rt, G[3] + 0x30u) & 0x800u; if (G[2] == 0u) G[4] = 0u; } G[2] = G[4]; } G[16] = s0; }
void func_0896D65C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4] + 0x24u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AACu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08978574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x18Cu); G[5] = G[4] + 0x18Cu; G[6] = 0u; if (G[2] != G[5]) { G[3] = 0u; do { G[2] = word(rt, G[2]); G[3] += 1u; } while (G[5] != G[2]); G[6] = G[3]; } G[3] = word(rt, G[4] + 0x184u); G[4] += 0x184u; G[2] = 0u; if (G[3] != G[4]) { do { G[3] = word(rt, G[3]); G[2] += 1u; } while (G[3] != G[4]); } G[2] += G[6]; }
void func_0897E404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089BD4DCu); G[4] = G[2]; G[5] = G[16]; } G[16] = s0; if (G[2] != 0u) jump(ctx, 0x089A2BF8u); }
void func_0898949C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = word(rt, G[4] + 0x1E0u); G[4] = 0x08CA39B4u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089CBFD4u); }
void func_089894E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = word(rt, G[4] + 0x1E0u); G[4] = 0x08CA39B4u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089CC7A8u); }
void func_089A6EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF4994u); G[18] = G[4]; G[4] = 0x08CF4994u; G[17] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4994u; put(rt, G[18] + 0x18u, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089A6F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF4994u); G[18] = G[4]; G[4] = 0x08CF4994u; G[17] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4994u; put(rt, G[18] + 0x1Cu, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089ADF28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[1] = 0x42700000u; ctx.fpr[12] = 60.0f; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_089B1CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x30u; G[7] = G[6]; G[6] = G[5]; G[17] = G[4]; G[5] = G[16]; call_here(rt, ctx, 0x089B1874u); G[4] = G[17]; G[5] = G[16]; G[6] = G[17] + 0x18u; } G[16] = s0; G[17] = s1; jump(ctx, 0x089AFA10u); }
void func_089B1FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x30u; G[7] = G[5]; G[5] = G[17]; G[16] = G[4]; G[6] = 0u; call_here(rt, ctx, 0x089B1874u); G[4] = G[16]; G[5] = G[17]; G[6] = G[16] + 0x18u; } G[16] = s0; G[17] = s1; jump(ctx, 0x089AFA10u); }
void func_089B34B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0xF4u); G[17] = G[5] & 0xFFu; if (G[16] != 0u) { G[4] = G[16]; do { G[5] = G[17]; call_here(rt, ctx, 0x08BFD1D0u); G[16] = word(rt, G[16] + 0x54u); G[4] = G[16]; } while (G[16] != 0u); } } G[16] = s0; G[17] = s1; }
void func_089C1FDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5]); const bool same = ctx.fpr[1] == ctx.fpr[0]; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[13]); if (same) return; ctx.fpr[0] = ctx.fpr[1] + ctx.fpr[12]; ctx.fpr[1] = std::bit_cast<float>(bits_of(ctx.fpr[12]) & 0x7FFFFFFFu); put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[2] = float_at(rt, G[5]); ctx.fpr[0] = ctx.fpr[2] - ctx.fpr[0]; ctx.fpr[0] = std::bit_cast<float>(bits_of(ctx.fpr[0]) & 0x7FFFFFFFu); if (ctx.fpr[0] <= ctx.fpr[1]) put(rt, G[4], bits_of(ctx.fpr[2])); }
void func_089C6B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCD190u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x188u; call_here(rt, ctx, 0x08AFB0F8u); G[4] = G[16]; call_here(rt, ctx, 0x089AE074u); G[4] = G[16]; G[5] = 0x260u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_089CC678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A5B828u); G[3] = 0x08D74FDCu; G[2] = 0x08CCF240u; put(rt, G[16] + 0x14u, G[3]); put(rt, G[16], G[2]); put(rt, G[16] + 0xCu, G[3]); put(rt, G[16] + 0x10u, G[3]); } G[16] = s0; }
void func_089CD978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[6] = 0u; G[2] = word(rt, G[4] + 0x120u); G[7] = 0u; G[8] = frame.at(0u); if (G[2] != 0u) { G[7] = frame.at(0u); G[8] = 0u; } call_here(rt, ctx, 0x0896A35Cu); ctx.fpr[0] = float_at(rt, frame.at(0u)); } }
void func_089CE1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x178u); G[3] = (G[2] << 4) - (G[2] << 2) + 0x160u; G[2] <<= 2; G[16] = G[4] + G[3]; call_here(rt, ctx, 0x089CE170u); G[3] = word(rt, G[16]) + (G[2] << 2); G[2] <<= 2; G[2] = word(rt, G[3]); } G[16] = s0; }
void func_089CEFE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x178u); G[3] = (G[2] << 4) - (G[2] << 2) + 0x160u; G[2] <<= 2; G[16] = G[4] + G[3]; call_here(rt, ctx, 0x089CE170u); G[3] = word(rt, G[16]) + (G[2] << 2); G[2] <<= 2; G[2] = word(rt, G[3]); } G[16] = s0; }
void func_089F59B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[6] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x089F5948u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A0BEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[6] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A0BE48u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A0E8D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CC6BD0u; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[16], G[2]); put(rt, G[16] + 0x18u, G[1]); store8(G[16] + 0x1Du, 0u); put(rt, G[16] + 0x14u, 0u); store8(G[16] + 0x1Cu, 0u); } G[16] = s0; }
void func_08A0F16C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1A4u); G[18] = word(rt, G[3] + 0x1A8u); call_here(rt, ctx, G[2]); G[2] ^= 1u; G[4] = G[16]; G[25] = G[18]; G[5] = G[2] & 0xFFu; } G[16] = s0; G[18] = s2; jump(ctx, G[25]); }
void func_08A11C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[6] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A11BF0u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A128FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[3] = word(rt, G[4] + 8u); G[5] = word(rt, G[4] + 0xCu); G[2] = word(rt, G[6] + 0x10u); G[4] = word(rt, G[4]) + 1u; G[3] += G[5]; G[5] = G[3] - G[2]; G[2] = G[3] < G[2] ? 1u : 0u; put(rt, G[6], G[4]); put(rt, G[6] + 8u, G[3]); if (G[2] != 0u) return; G[2] = word(rt, G[6] + 4u); put(rt, G[6] + 8u, G[5]); G[2] += 1u; put(rt, G[6] + 4u, G[2]); }
void func_08A13654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x10u; G[17] = G[5] & 0xFFu; G[16] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08B34888u); G[4] = G[18]; store8(G[16] + 0x1F6u, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B34B24u); }
void func_08A13B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x10u; G[17] = G[5]; G[16] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08B34888u); G[4] = G[18]; put(rt, G[16] + 0x784u, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B34B24u); }
void func_08A1AA24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x68u; G[17] = G[5]; G[16] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[18]; put(rt, G[16] + 0x64u, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_08A21A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A983D8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A223C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBE7F8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08AB86A8u); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x18u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A29A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AACu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A2B890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = high_half(0x08D69174u); G[5] = word(rt, 0x08D69174u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A30E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AACu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A31B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CFF348u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A35040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[16] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CACEC8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[16]; } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, 0x08A3A480u); }
void func_08A352F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFu; G[16] = word(rt, G[4] + 0x18u); G[4] = 0x08CACEC8u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A3A4A0u); }
void func_08A38128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD16E0u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = 1u; call_here(rt, ctx, 0x08A37DD0u); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x9DCu; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A39E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCC730u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08C1B4A0u); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x14Cu; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A3D188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD1A10u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x40u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A4CEDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[4] + 0x20u); ctx.fpr[1] = float_at(rt, G[4] + 0x50u); ctx.fpr[3] = float_at(rt, G[4] + 0x48u); G[1] = 0x41600000u; ctx.fpr[0] = 14.0f; ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[3]; ctx.fpr[0] = float_at(rt, G[4] + 0x28u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[12]; ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; put(rt, G[4] + 0x28u, bits_of(ctx.fpr[0])); G[2] = ctx.fpr[0] < ctx.fpr[1] ? 1u : 0u; }
void func_08A4DF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[18] = G[5]; G[17] = word(rt, G[4]); call_here(rt, ctx, 0x08B1AC1Cu); G[3] = word(rt, G[16]) - G[17]; put(rt, G[18], G[3]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A550A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB75B8u; G[16] = G[4]; G[4] += 0x38u; put(rt, G[16], G[2]); G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C1D728u); G[2] = 0x08CB7440u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A7D4E4u); }
void func_08A5D3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCB870u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08A69D68u); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x18u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A5EB24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[6] = word(rt, G[4] + 0x18u); G[7] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A603F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 0x14u); G[5] = G[2] < 1u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A6043C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 0x14u); G[5] = 0u - G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5FA8Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A60484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 0x14u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5FA8Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A6089C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 0x14u); G[5] = ~G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5FA8Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A64C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2868u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A69D68u); G[2] = 0x08CD3638u; put(rt, G[16], G[2]); G[4] = G[16]; G[5] = 0xCu; } G[16] = s0; jump(ctx, 0x08A5BB84u); }
void func_08A66D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2E08u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A75A88u); G[2] = 0x08CD3638u; put(rt, G[16], G[2]); G[4] = G[16]; G[5] = 0x14u; } G[16] = s0; jump(ctx, 0x08A5BB84u); }
void func_08A6D378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CFF348u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A71AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = 1u; G[16] = G[4]; store8(frame.at(0u), G[5]); G[2] = word(rt, G[3] + 0x10Cu); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); G[3] = 1u; const bool one = G[2] == G[3]; G[2] = 2u; if (!one) put(rt, G[16] + 0x14u, 2u); } G[16] = s0; }
void func_08A73C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load16(G[4] + 0x14u), 16u); G[5] = G[2] < 1u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A73C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load16(G[4] + 0x14u), 16u); G[5] = 0u - G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A73CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load16(G[4] + 0x14u), 16u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A74104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load16(G[4] + 0x14u), 16u); G[5] = ~G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A83CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[6] = word(rt, G[4] + 0x18u); G[7] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A83160u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A864B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 0x14u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A8B180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); G[6] = word(rt, G[5] + 0x18u); G[7] = word(rt, G[5] + 0x1Cu); call_here(rt, ctx, 0x08A8B098u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08A8BAE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[6] = word(rt, G[4] + 0x18u); G[7] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8B098u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A8BD1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[16] = G[4]; G[7] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8B098u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A8C174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x14u; G[2] = G[4] + 0x20u; G[5] = word(rt, G[16] + 4u); G[3] = G[5] - 4u; G[4] = G[2]; if (G[2] != G[3]) { G[5] = word(rt, G[5] - 4u); call_here(rt, ctx, 0x08A5BAD8u); } G[5] = word(rt, G[16] + 4u); G[4] = G[5] - 4u; put(rt, G[16] + 4u, G[4]); } G[16] = s0; jump(ctx, 0x08A5B9ECu); }
void func_08A8C6E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A9402C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A719F0u); G[2] = 0x08CD3B28u; put(rt, G[16], G[2]); G[4] = G[16] + 0x1Cu; call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08A94CE8u); put(rt, G[16] + 0x24u, 0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A93DD4u); }
void func_08A96A10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load8(G[4] + 0x14u), 8u); G[5] = 0u - G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A96A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load8(G[4] + 0x14u), 8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A96E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = sext(load8(G[4] + 0x14u), 8u); G[5] = ~G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A97728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CB0000u; G[16] = G[4]; G[3] = load8(G[5] + 0x14u); G[5] = 0x08CB22F8u; if (G[3] == 0u) { G[2] = 0x08CB0000u; G[5] = 0x08CB2300u; } G[4] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B85EB8u); G[2] = G[16]; } G[16] = s0; }
void func_08A97F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load8(G[4] + 0x14u); G[5] = G[2] < 1u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AA634C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 4u; call_here(rt, ctx, 0x08AA61D8u); G[3] = load8(frame.at(1u)); G[2] = load8(frame.at(0u)); G[4] = load8(frame.at(2u)); G[5] = load8(frame.at(3u)); G[3] <<= 8; G[2] |= G[3]; G[4] <<= 16; G[2] |= G[4]; G[5] <<= 24; G[2] |= G[5]; }
void func_08AA9070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[16] = word(rt, G[4] + 4u); G[4] = 0x08CB30B0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[16]; } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, 0x08C31394u); }
void func_08AA9360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFFFu; G[16] = word(rt, G[4] + 4u); G[4] = 0x08CB30B0u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C3132Cu); }
void func_08AA9480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFu; G[16] = word(rt, G[4] + 4u); G[4] = 0x08CB30B0u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C31314u); }
void func_08AA95A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFu; G[16] = word(rt, G[4] + 4u); G[4] = 0x08CB30B0u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C312F4u); }
void func_08AAE358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[3] = word(rt, G[5] + 0xCu); G[5] = frame.at(0u); if (G[3] != 0u) { G[2] = word(rt, G[3] + 4u); if (G[2] == 0u) G[3] = 0u; } put(rt, frame.at(0u), G[3]); call_here(rt, ctx, 0x08AAE320u); G[2] = G[16]; } G[16] = s0; }
void func_08AB0B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load16(G[4] + 0x14u); G[5] = G[2] < 1u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB0B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load16(G[4] + 0x14u); G[5] = 0u - G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB0BC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load16(G[4] + 0x14u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB0FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load16(G[4] + 0x14u); G[5] = ~G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB22E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load8(G[4] + 0x14u); G[5] = G[2] < 1u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB2328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load8(G[4] + 0x14u); G[5] = 0u - G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB2370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load8(G[4] + 0x14u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB2790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = load8(G[4] + 0x14u); G[5] = ~G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB481C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD4A58u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B1E97Cu); G[2] = 0x08D74FDCu; put(rt, G[16] + 0x24u, G[2]); } G[16] = s0; }
void func_08AB70A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = word(rt, G[4] + 0xCu); G[4] = 0x08CB4250u; if (G[16] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08AAE4E4u); }
void func_08ABB250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08ABCD64u); G[2] = 0x08CD60C8u; put(rt, G[16], G[2]); G[4] = high_half(0x08D77184u); G[3] = G[16] + 0xCu; G[2] = word(rt, 0x08D77184u); put(rt, G[3] + 4u, G[16]); put(rt, G[16] + 0xCu, G[2]); put(rt, 0x08D77184u, G[3]); } G[16] = s0; }
void func_08ACA4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; if (G[5] != 0u) { call_here(rt, ctx, 0x08ACA4BCu); G[4] = 0x08D003E8u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B2EEB8u); G[2] = 0u; return; } } G[2] = 0x80001000u; }
void func_08ACA53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; if (G[5] != 0u) { call_here(rt, ctx, 0x08ACA4BCu); G[4] = 0x08D003E8u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B2EE2Cu); G[2] = 0u; return; } } G[2] = 0x80001000u; }
void func_08ACC0F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08ACC0ECu); G[3] = word(rt, G[16] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16]); call_here(rt, ctx, G[2]); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_08ACC5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x60u; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); call_here(rt, ctx, 0x08C7B180u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08B34B24u); G[2] = G[16] < 1u ? 1u : 0u; } G[16] = s0; G[17] = s1; }
void func_08ACC6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08ACC6B4u); G[3] = word(rt, G[16] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16]); call_here(rt, ctx, G[2]); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_08ACD4A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] + 0x84u; G[4] = G[17]; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[2] = 1u; G[4] = G[17]; store8(G[16] + 0xE2u, 1u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[16] + 0x58u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B28C44u); }
void func_08ACD86C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] + 0x80u; G[4] = G[17]; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[2] = 1u; G[4] = G[17]; store8(G[16] + 0xCAu, 1u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[16] + 0x54u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B28C44u); }
void func_08AD2060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x29Cu); call_here(rt, ctx, 0x08AD5EC8u); G[4] = G[16]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08AD203Cu); G[4] = word(rt, G[16] + 0x29Cu); } G[16] = s0; G[17] = s1; jump(ctx, 0x08AD5ED0u); }
void func_08AE68D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store16(G[4], 0u); G[2] = G[4] + 0x1Au; G[3] = 0x17u; store16(G[4] + 2u, 0u); do { G[3] -= 1u; store16(G[2], 0u); G[2] += 2u; } while (static_cast<std::int32_t>(G[3]) >= 0); G[5] = 0x1000u; G[2] = 0x3E8u; G[3] = 3u; do { G[3] -= 1u; store16(G[4] + 0x22u, G[5]); store16(G[4] + 0x24u, G[2]); G[4] += 0xCu; } while (static_cast<std::int32_t>(G[3]) >= 0); }
void func_08AE6E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[16] + 4u); for (;;) { G[2] = static_cast<std::int32_t>(word(rt, G[3] + 8u)) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; if (G[3] == 0u) break; call_here(rt, ctx, 0x08AE6DCCu); G[3] = word(rt, G[16] + 4u); } } G[16] = s0; }
void func_08AE74F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x18u; G[3] = word(rt, G[16] + 4u); for (;;) { G[2] = static_cast<std::int32_t>(word(rt, G[3] + 8u)) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; if (G[3] == 0u) break; call_here(rt, ctx, 0x08AE740Cu); G[3] = word(rt, G[16] + 4u); } } G[16] = s0; }
void func_08AEFD48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AEB808u); G[3] = 0u; G[2] = word(rt, G[16] + 0x3Cu) - 1u; if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = word(rt, G[16] + 0x3Cu); G[3] = G[2] - 1u; } put(rt, G[16] + 0x3Cu, G[3]); call_here(rt, ctx, 0x08AEB82Cu); } G[16] = s0; }
void func_08B084A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); store8(G[4] + 0xACu, 0u); for (std::uint32_t offset = 4u; offset < 0x2Cu; offset += 4u) put(rt, G[4] + offset, 0u); call_here(rt, ctx, 0x08B08450u); G[2] = 1u; }
void func_08B094AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D4744Cu); G[3] = word(rt, 0x08D4744Cu); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[3]; G[16] = high_half(0x08D47448u); put(rt, 0x08D4744Cu, 0u); if (G[3] != 0u) { call_here(rt, ctx, 0x08B1B99Cu); G[4] = word(rt, 0x08D47448u); call_here(rt, ctx, 0x08B35254u); put(rt, 0x08D47448u, 0u); } } G[16] = s0; }
void func_08B125A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x1E0u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x25Cu, G[2]); G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); G[2] = 1u; } G[16] = s0; G[17] = s1; }
void func_08B17A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08D62BECu); G[16] = G[4]; G[4] = 0x08D62BECu; call_here(rt, ctx, 0x08B34888u); G[4] = 0x08D62C78u; G[5] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08B1FBECu); G[4] = 0x08D62BECu; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34B24u); }
void func_08B19244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[2] = word(rt, G[5]); G[3] = word(rt, G[2] + 0xCu); G[4] = frame.at(0u); call_here(rt, ctx, G[3]); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B18A58u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B18A98u); G[2] = G[16]; } G[16] = s0; }
void func_08B1B620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[4]; G[4] = 0x15180u; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[6])) * 0x15180; ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[3] = sra(G[5], 31u); G[6] = ctx.lo; G[4] = G[6] + G[5]; G[8] = G[4] < G[6] ? 1u : 0u; G[7] = ctx.hi; G[6] = 0xFFFF5740u; G[5] = G[7] + G[3] + G[8]; G[2] = G[4] + G[6]; G[8] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u + G[8]; }
void func_08B23BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD95E0u; G[16] = G[4]; G[5] = G[16] + 0x2Cu; put(rt, G[4], G[2]); G[4] = 0x08D68C8Cu; call_here(rt, ctx, 0x08B1FA64u); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B286BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFC5u; G[3] = 0xFFFFFFEEu; store8(G[5], G[2]); G[2] = 0xFFFFFFF7u; store8(G[5] + 1u, G[3]); G[3] = 0xFFFFFFFFu; store8(G[5] + 3u, G[3]); store8(G[5] + 2u, G[2]); G[2] = 0u - word(rt, G[4] + 0x6Cu); G[3] = G[2] >> 24; G[4] = (G[2] >> 8) & 0xFFu; G[6] = (G[2] >> 16) & 0xFFu; store8(G[5] + 7u, G[3]); store8(G[5] + 5u, G[4]); store8(G[5] + 6u, G[6]); store8(G[5] + 4u, G[2]); }
void func_08B2B188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B2B118u); G[4] = G[2]; G[5] = G[16]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x18u); tail = true; } } G[16] = s0; if (tail) jump(ctx, G[25]); }
void func_08B2B490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D68CC8u); G[17] = G[4]; G[4] = 0x08D68CC8u; call_here(rt, ctx, 0x08B34888u); G[3] = high_half(0x08D68CF4u); G[2] = word(rt, 0x08D68CF4u) + G[17]; G[4] = 0x08D68CC8u; put(rt, 0x08D68CF4u, G[2]); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34B24u); }
void func_08B2D2CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = 0x03416102u; G[6] = frame.at(0u); G[7] = 4u; G[8] = 0u; G[9] = 0u; put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08C7A828u); if (static_cast<std::int32_t>(G[2]) >= 0) return; G[4] = 7u; G[3] = high_half(0x08D68CF8u); put(rt, 0x08D68CF8u, 7u); }
void func_08B32598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDA730u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B3252Cu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08B31918u); G[2] = 0x08CDA7A8u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B33294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D691D8u); G[16] = 0x08D691D8u; G[3] = word(rt, G[16] + 8u); G[4] = G[3]; if (G[3] != 0u) { G[2] = 1u; store8(G[16] + 0xCu, 1u); call_here(rt, ctx, 0x08B2E224u); G[4] = word(rt, G[16] + 8u); call_here(rt, ctx, 0x08B2E28Cu); put(rt, G[16] + 8u, 0u); } } G[16] = s0; }
void func_08B34228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B34DA8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B34D40u); G[3] = word(rt, 0x08D69270u); G[4] = frame.at(0u); G[2] = high_half(0x08CE0DA0u); G[3] = 0u - G[3]; put(rt, 0x08CE0DA0u, G[3]); call_here(rt, ctx, 0x08B34D80u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B34DA0u); }
void func_08B3DFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = G[6]; G[6] = G[7]; G[16] = G[4]; call_here(rt, ctx, 0x08B3E0ACu); G[2] = 0x08CDA960u; put(rt, G[16] + 0x2Cu, G[17]); put(rt, G[16], G[2]); } G[16] = s0; G[17] = s1; }
void func_08B46270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B2E05Cu); G[6] = 0x3E8u; G[8] = word(rt, G[16] + 0x4890u); G[9] = word(rt, G[16] + 0x4894u); G[7] = 0u; G[4] = G[2] - G[8]; G[10] = G[2] < G[4] ? 1u : 0u; G[5] = G[3] - G[9] - G[10]; call_here(rt, ctx, 0x08B6F518u); } G[16] = s0; }
void func_08B4D1BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = (G[6] << 2) + G[4]; ctx.fpr[1] = float_at(rt, G[6]); G[2] = (G[5] << 2) + G[4]; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[12]); ctx.fpr[0] = float_at(rt, G[2]); G[5] = ((G[5] + 2u) << 2) + G[4]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[6] + 8u); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[12]; put(rt, G[5], bits_of(ctx.fpr[0])); }
void func_08B531B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[16]; call_here(rt, ctx, 0x08C53CC4u); G[2] = word(rt, G[16]); G[4] = G[2]; } G[16] = s0; if (G[2] != 0u) jump(ctx, 0x08B5314Cu); }
void func_08B55630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = (G[4] & 7u) << 14; G[5] = (G[5] & 3u) << 9; G[6] = (G[6] & 3u) << 7; G[7] = (G[7] & 3u) << 5; G[8] = ((G[8] & 3u) | 4u) << 2; G[9] &= 3u; G[2] |= G[5] | G[6] | G[7] | G[8] | G[9]; }
void func_08B584F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0x08D6CE00u; G[2] = high_half(0x08D6CE00u); G[5] = (G[5] & 0xFFu) << 8; G[6] = ((G[6] & 0xFFu) << 16) | G[5]; G[3] = word(rt, G[7] + 0xF8u); G[4] &= 0xFFu; G[6] |= G[4]; G[2] = 0x30Au; put(rt, G[7] + 0x104u, G[6]); if (G[3] != G[2]) return; G[2] = word(rt, G[7] + 0x6Cu) | 0x800u; put(rt, G[7] + 0x6Cu, G[2]); }
void func_08B64188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08CE1F30u); G[16] = word(rt, 0x08CE1F30u); if (G[16] != 0u) { do { G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08C7AAB8u); G[16] = word(rt, G[16] + 4u); } while (G[16] != 0u); put(rt, 0x08CE1F30u, 0u); } } G[16] = s0; G[17] = s1; }
void func_08B6F4D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 0.0f; if (!(ctx.fpr[12] < ctx.fpr[0])) { jump(ctx, 0x08B6F37Cu); return; } GuestFrame frame(ctx, 0x10u); ctx.fpr[12] = -ctx.fpr[12]; call_here(rt, ctx, 0x08B6F37Cu); G[2] = 0u - G[2]; G[6] = G[2] != 0u ? 1u : 0u; G[3] = 0u - G[3] - G[6]; }
void func_08B78808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[16] = G[5]; put(rt, frame.at(0u), G[4]); call_here(rt, ctx, 0x08B35F60u); G[4] = G[16]; G[5] = frame.at(8u); G[3] = 1u; put(rt, frame.at(4u), G[2]); put(rt, frame.at(8u), frame.at(0u)); put(rt, frame.at(0xCu), 1u); put(rt, frame.at(0x10u), G[2]); call_here(rt, ctx, 0x08B79074u); } G[16] = s0; }
void func_08B7ADCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[2] = word(rt, G[5]); G[5] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B7ADCCu); } G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B354A4u); }
void func_08B85394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CF6ED8u; G[16] = G[4] + 0x14u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A9B748u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08B85C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB6ED0u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x14u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x18u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08B88E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08B88CD4u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08B97408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[2] = word(rt, G[4] + 0x48u); G[16] = G[4]; if (G[2] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x20u); call_here(rt, ctx, G[2]); G[3] = G[2] - word(rt, G[16] + 0x48u); } G[2] = G[3]; } G[16] = s0; }
void func_08BA5F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[2] = word(rt, G[4] + 0x94u); G[16] = G[4]; if (G[2] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x20u); call_here(rt, ctx, G[2]); G[3] = G[2] - word(rt, G[16] + 0x94u); } G[2] = G[3]; } G[16] = s0; }
void func_08BDAFF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08BDAFB4u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C0C2C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCFC90u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x30u; call_here(rt, ctx, 0x089F9B74u); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x089F9B74u); G[4] = G[16]; call_here(rt, ctx, 0x089D6684u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08C16084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] == G[5]) return; do { G[3] = G[4] + 0x10u; if (G[2] != 0u) { ctx.fpr[3] = float_at(rt, G[4] + 0xCu); ctx.fpr[0] = float_at(rt, G[4]); ctx.fpr[1] = float_at(rt, G[4] + 4u); ctx.fpr[2] = float_at(rt, G[4] + 8u); put(rt, G[2], bits_of(ctx.fpr[0])); put(rt, G[2] + 4u, bits_of(ctx.fpr[1])); put(rt, G[2] + 8u, bits_of(ctx.fpr[2])); put(rt, G[2] + 0xCu, bits_of(ctx.fpr[3])); } G[4] = G[3]; G[2] += 0x10u; } while (G[5] != G[3]); }
void func_08C1A62C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; ctx.fpr[12] = 0.0f; call_here(rt, ctx, 0x08C1A5ECu); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C1A7B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08B84C84u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C1A7FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08B84C34u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C1A844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08B84E84u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C23038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = G[5]; G[16] = G[5]; call_here(rt, ctx, 0x08A6158Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A7C3A4u); G[5] = word(rt, frame.at(0u)); G[4] = G[16]; call_here(rt, ctx, 0x08A62C54u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08C3BDB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD5E30u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0xCu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[16]; G[5] = 0x14u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08C3D02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x08B84ED4u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C55C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4]; G[7] = word(rt, G[5]); G[8] = word(rt, G[5] + 4u); G[5] = G[6]; G[2] = word(rt, G[7] - 0xCu); put(rt, G[4], G[7]); G[2] += G[4]; put(rt, G[2], G[8]); G[6] = word(rt, G[4]); put(rt, G[4] + 4u, 0u); G[4] = word(rt, G[6] - 0xCu) + G[3]; call_with(rt, ctx, 0x08C54A7Cu, 0x10u); }
void func_08C55D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB174u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CDB088u; G[4] += 8u; put(rt, G[16] + 8u, G[2]); put(rt, G[16] + 4u, 0u); call_here(rt, ctx, 0x08B6543Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08C5A69C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB870u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B61BC8u); G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08C5AADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB7C0u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B61BC8u); G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08C5D858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5D8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5D910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = G[6] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[6]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5D96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = G[6] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[6]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5FB50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB7C0u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B61BC8u); G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08C756C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[2] = word(rt, G[3] - 4u); G[16] = G[4]; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08C75524u); G[3] = word(rt, G[16]); G[2] = G[3] + G[17]; } G[16] = s0; G[17] = s1; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_183
} // namespace gt::game

#define HOT183(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_183::name, gt::ReturnKind::kind)
HOT183(0x088095C0u, func_088095C0, Word);
HOT183(0x0881AE5Cu, func_0881AE5C, Word);
HOT183(0x0882A2D8u, func_0882A2D8, Word);
HOT183(0x0882EA2Cu, func_0882EA2C, Word);
HOT183(0x0884C4E4u, func_0884C4E4, Word);
HOT183(0x0885E4DCu, func_0885E4DC, Word);
HOT183(0x088663ECu, func_088663EC, Word);
HOT183(0x0886AEDCu, func_0886AEDC, Word);
// HOT183(0x0886BD00u, func_0886BD00, Word);  // no comparable synthetic case yet
HOT183(0x08875878u, func_08875878, Word);
HOT183(0x08875974u, func_08875974, Word);
HOT183(0x088765D4u, func_088765D4, Word);
HOT183(0x08891708u, func_08891708, Word);
HOT183(0x08893C78u, func_08893C78, Word);
HOT183(0x08898EE8u, func_08898EE8, Word);
HOT183(0x088A2544u, func_088A2544, Word);
// HOT183(0x088A2A1Cu, func_088A2A1C, Word);  // no comparable synthetic case yet
HOT183(0x088A5AF4u, func_088A5AF4, Word);
// HOT183(0x088A627Cu, func_088A627C, Word);  // no comparable synthetic case yet
HOT183(0x088AC0C4u, func_088AC0C4, Float);
HOT183(0x088C6304u, func_088C6304, Word);
HOT183(0x088E51D0u, func_088E51D0, Word);
HOT183(0x088E82E4u, func_088E82E4, Word);
HOT183(0x088E8688u, func_088E8688, Word);
HOT183(0x088EAA7Cu, func_088EAA7C, Word);
HOT183(0x088F1310u, func_088F1310, Word);
HOT183(0x088F77ACu, func_088F77AC, Float);
HOT183(0x088F7BE8u, func_088F7BE8, Word);
HOT183(0x0892981Cu, func_0892981C, Word);
// HOT183(0x0892A37Cu, func_0892A37C, Word);  // no comparable synthetic case yet
HOT183(0x0893205Cu, func_0893205C, Word);
HOT183(0x08936894u, func_08936894, Word);
HOT183(0x08939FA4u, func_08939FA4, Word);
HOT183(0x08947B50u, func_08947B50, Word);
HOT183(0x0896457Cu, func_0896457C, Word);
HOT183(0x0896716Cu, func_0896716C, Word);
HOT183(0x089671B4u, func_089671B4, Word);
HOT183(0x0896D65Cu, func_0896D65C, Word);
// HOT183(0x08978574u, func_08978574, Word);  // no comparable synthetic case yet
HOT183(0x0897E404u, func_0897E404, Word);
HOT183(0x0898949Cu, func_0898949C, Word);
// HOT183(0x089894E4u, func_089894E4, Word);  // no comparable synthetic case yet
HOT183(0x089A6EF0u, func_089A6EF0, Word);
HOT183(0x089A6F84u, func_089A6F84, Word);
HOT183(0x089ADF28u, func_089ADF28, Word);
// HOT183(0x089B1CC8u, func_089B1CC8, Word);  // no comparable synthetic case yet
// HOT183(0x089B1FCCu, func_089B1FCC, Word);  // no comparable synthetic case yet
HOT183(0x089B34B0u, func_089B34B0, Word);
HOT183(0x089C1FDCu, func_089C1FDC, Word);
// HOT183(0x089C6B90u, func_089C6B90, Word);  // no comparable synthetic case yet
HOT183(0x089CC678u, func_089CC678, Word);
HOT183(0x089CD978u, func_089CD978, Float);
HOT183(0x089CE1F4u, func_089CE1F4, Word);
HOT183(0x089CEFE8u, func_089CEFE8, Word);
HOT183(0x089F59B8u, func_089F59B8, Word);
HOT183(0x08A0BEB8u, func_08A0BEB8, Word);
HOT183(0x08A0E8D0u, func_08A0E8D0, Word);
HOT183(0x08A0F16Cu, func_08A0F16C, Word);
HOT183(0x08A11C60u, func_08A11C60, Word);
HOT183(0x08A128FCu, func_08A128FC, Word);
// HOT183(0x08A13654u, func_08A13654, Word);  // no comparable synthetic case yet
// HOT183(0x08A13B98u, func_08A13B98, Word);  // no comparable synthetic case yet
HOT183(0x08A1AA24u, func_08A1AA24, Word);
HOT183(0x08A21A04u, func_08A21A04, Word);
HOT183(0x08A223C4u, func_08A223C4, Word);
HOT183(0x08A29A4Cu, func_08A29A4C, Word);
HOT183(0x08A2B890u, func_08A2B890, Word);
HOT183(0x08A30E98u, func_08A30E98, Word);
HOT183(0x08A31B0Cu, func_08A31B0C, Word);
HOT183(0x08A35040u, func_08A35040, Word);
HOT183(0x08A352F4u, func_08A352F4, Word);
HOT183(0x08A38128u, func_08A38128, Word);
HOT183(0x08A39E1Cu, func_08A39E1C, Word);
HOT183(0x08A3D188u, func_08A3D188, Word);
HOT183(0x08A4CEDCu, func_08A4CEDC, Word);
HOT183(0x08A4DF6Cu, func_08A4DF6C, Word);
HOT183(0x08A550A0u, func_08A550A0, Word);
HOT183(0x08A5D3C4u, func_08A5D3C4, Word);
HOT183(0x08A5EB24u, func_08A5EB24, Word);
HOT183(0x08A603F4u, func_08A603F4, Word);
HOT183(0x08A6043Cu, func_08A6043C, Word);
HOT183(0x08A60484u, func_08A60484, Word);
HOT183(0x08A6089Cu, func_08A6089C, Word);
HOT183(0x08A64C38u, func_08A64C38, Word);
HOT183(0x08A66D24u, func_08A66D24, Word);
HOT183(0x08A6D378u, func_08A6D378, Word);
HOT183(0x08A71AB8u, func_08A71AB8, Word);
HOT183(0x08A73C54u, func_08A73C54, Word);
HOT183(0x08A73C9Cu, func_08A73C9C, Word);
HOT183(0x08A73CE4u, func_08A73CE4, Word);
HOT183(0x08A74104u, func_08A74104, Word);
HOT183(0x08A83CC8u, func_08A83CC8, Word);
HOT183(0x08A864B0u, func_08A864B0, Word);
HOT183(0x08A8B180u, func_08A8B180, Word);
HOT183(0x08A8BAE4u, func_08A8BAE4, Word);
HOT183(0x08A8BD1Cu, func_08A8BD1C, Word);
HOT183(0x08A8C174u, func_08A8C174, Word);
HOT183(0x08A8C6E4u, func_08A8C6E4, Word);
HOT183(0x08A9402Cu, func_08A9402C, Word);
HOT183(0x08A96A10u, func_08A96A10, Word);
HOT183(0x08A96A58u, func_08A96A58, Word);
HOT183(0x08A96E78u, func_08A96E78, Word);
HOT183(0x08A97728u, func_08A97728, Word);
HOT183(0x08A97F54u, func_08A97F54, Word);
// HOT183(0x08AA634Cu, func_08AA634C, Word);  // no comparable synthetic case yet
// HOT183(0x08AA9070u, func_08AA9070, Word);  // no comparable synthetic case yet
// HOT183(0x08AA9360u, func_08AA9360, Word);  // no comparable synthetic case yet
HOT183(0x08AA9480u, func_08AA9480, Word);
// HOT183(0x08AA95A0u, func_08AA95A0, Word);  // no comparable synthetic case yet
HOT183(0x08AAE358u, func_08AAE358, Word);
HOT183(0x08AB0B34u, func_08AB0B34, Word);
HOT183(0x08AB0B7Cu, func_08AB0B7C, Word);
HOT183(0x08AB0BC4u, func_08AB0BC4, Word);
HOT183(0x08AB0FE4u, func_08AB0FE4, Word);
HOT183(0x08AB22E0u, func_08AB22E0, Word);
HOT183(0x08AB2328u, func_08AB2328, Word);
HOT183(0x08AB2370u, func_08AB2370, Word);
HOT183(0x08AB2790u, func_08AB2790, Word);
HOT183(0x08AB481Cu, func_08AB481C, Word);
// HOT183(0x08AB70A4u, func_08AB70A4, Word);  // no comparable synthetic case yet
HOT183(0x08ABB250u, func_08ABB250, Word);
HOT183(0x08ACA4F4u, func_08ACA4F4, Word);
HOT183(0x08ACA53Cu, func_08ACA53C, Word);
// HOT183(0x08ACC0F4u, func_08ACC0F4, Word);  // no comparable synthetic case yet
// HOT183(0x08ACC5DCu, func_08ACC5DC, Word);  // no comparable synthetic case yet
// HOT183(0x08ACC6BCu, func_08ACC6BC, Word);  // no comparable synthetic case yet
// HOT183(0x08ACD4A8u, func_08ACD4A8, Word);  // no comparable synthetic case yet
// HOT183(0x08ACD86Cu, func_08ACD86C, Word);  // no comparable synthetic case yet
HOT183(0x08AD2060u, func_08AD2060, Word);
HOT183(0x08AE68D8u, func_08AE68D8, Word);
HOT183(0x08AE6E4Cu, func_08AE6E4C, Word);
HOT183(0x08AE74F4u, func_08AE74F4, Word);
HOT183(0x08AEFD48u, func_08AEFD48, Word);
HOT183(0x08B084A8u, func_08B084A8, Word);
// HOT183(0x08B094ACu, func_08B094AC, Word);  // no comparable synthetic case yet
HOT183(0x08B125A8u, func_08B125A8, Word);
HOT183(0x08B17A38u, func_08B17A38, Word);
HOT183(0x08B19244u, func_08B19244, Word);
HOT183(0x08B1B620u, func_08B1B620, DoubleWord);
HOT183(0x08B23BA8u, func_08B23BA8, Word);
HOT183(0x08B286BCu, func_08B286BC, Word);
HOT183(0x08B2B188u, func_08B2B188, Word);
HOT183(0x08B2B490u, func_08B2B490, Word);
// HOT183(0x08B2D2CCu, func_08B2D2CC, Word);  // no comparable synthetic case yet
// HOT183(0x08B32598u, func_08B32598, Word);  // no comparable synthetic case yet
// HOT183(0x08B33294u, func_08B33294, Word);  // no comparable synthetic case yet
HOT183(0x08B34228u, func_08B34228, Word);
HOT183(0x08B3DFD0u, func_08B3DFD0, Word);
HOT183(0x08B46270u, func_08B46270, DoubleWord);
HOT183(0x08B4D1BCu, func_08B4D1BC, Word);
HOT183(0x08B531B4u, func_08B531B4, Word);
HOT183(0x08B55630u, func_08B55630, Word);
HOT183(0x08B584F4u, func_08B584F4, Word);
// HOT183(0x08B64188u, func_08B64188, Word);  // no comparable synthetic case yet
HOT183(0x08B6F4D0u, func_08B6F4D0, DoubleWord);
HOT183(0x08B78808u, func_08B78808, Word);
HOT183(0x08B7ADCCu, func_08B7ADCC, Word);
HOT183(0x08B85394u, func_08B85394, Word);
HOT183(0x08B85C70u, func_08B85C70, Word);
HOT183(0x08B88E10u, func_08B88E10, Word);
HOT183(0x08B97408u, func_08B97408, Word);
HOT183(0x08BA5F14u, func_08BA5F14, Word);
HOT183(0x08BDAFF4u, func_08BDAFF4, Word);
HOT183(0x08C0C2C4u, func_08C0C2C4, Word);
// HOT183(0x08C16084u, func_08C16084, Word);  // no comparable synthetic case yet
HOT183(0x08C1A62Cu, func_08C1A62C, Word);
HOT183(0x08C1A7B4u, func_08C1A7B4, Word);
HOT183(0x08C1A7FCu, func_08C1A7FC, Word);
HOT183(0x08C1A844u, func_08C1A844, Word);
HOT183(0x08C23038u, func_08C23038, Word);
HOT183(0x08C3BDB0u, func_08C3BDB0, Word);
HOT183(0x08C3D02Cu, func_08C3D02C, Word);
// HOT183(0x08C55C24u, func_08C55C24, Word);  // no comparable synthetic case yet
HOT183(0x08C55D88u, func_08C55D88, Word);
HOT183(0x08C5A69Cu, func_08C5A69C, Word);
HOT183(0x08C5AADCu, func_08C5AADC, Word);
HOT183(0x08C5D858u, func_08C5D858, Word);
HOT183(0x08C5D8B4u, func_08C5D8B4, Word);
HOT183(0x08C5D910u, func_08C5D910, Word);
HOT183(0x08C5D96Cu, func_08C5D96C, Word);
HOT183(0x08C5FB50u, func_08C5FB50, Word);
HOT183(0x08C756C4u, func_08C756C4, Word);
