// Hot batch 182: seventeen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_182 {
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

void func_0880BC34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0896BDF8u); G[2] = 0x08CBA148u; G[3] = 0x08CBA380u; put(rt, G[16], G[2]); put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0xECu, 0u); put(rt, G[16] + 0xE8u, 0u); } G[16] = s0; }
void func_08821514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08857618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 1u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08821A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x110u); G[6] = 0x80u; G[16] = G[4]; G[2] = sext(load8(G[5]), 8u); G[4] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x08B1AEACu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x088219F8u); } } G[16] = s0; }
void func_0885C848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = G[4]; G[8] = 0u; G[3] = word(rt, G[2] + 0x30u); G[4] = (G[3] << 3) + G[2]; G[3] = static_cast<std::int32_t>(G[3]) < 8 ? 1u : 0u; if (G[3] != 0u) { put(rt, G[4] + 0x38u, G[6]); G[8] = 1u; put(rt, G[4] + 0x3Cu, G[7]); G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0x30u) + 1u; put(rt, G[3] + 0x30u, G[2]); } G[2] = G[8]; }
void func_0885CB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[5]) < 0) return; G[6] = word(rt, G[4]); G[3] = G[5] << 3; G[2] = (G[5] << 6) - G[3]; G[4] = word(rt, G[6] + 4u); G[3] = (G[2] << 3) - G[2]; G[4] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; G[2] = 0xFFFFFFFFu; if (G[4] == 0u) return; G[2] = word(rt, word(rt, G[6]) + G[3] + 0x1Cu); }
void func_0885D56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x16u); G[6] = G[4]; G[7] = 0u; if (G[2] != 0u) { G[2] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; if (static_cast<std::int32_t>(G[5]) >= 0) { G[4] = G[5] + G[4]; if (G[2] != 0u) { G[4] = load8(G[4] + 0x40u); G[2] = (G[5] << 6) - (G[5] << 4) + G[6]; if (G[4] != 0u) G[7] = G[2] + 0x58u; } } } G[2] = G[7]; }
void func_0885E31C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16]); call_here(rt, ctx, G[2]); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_0885E68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[7]; G[16] = G[6]; G[4] = word(rt, G[4]) + 0x3F80u; call_here(rt, ctx, 0x08891644u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08891854u); }
void func_0885EE7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4]) + 0x3F80u; call_here(rt, ctx, 0x08892138u); put(rt, G[16], G[2]); G[4] = word(rt, G[17]) + 0x3F80u; } G[16] = s0; G[17] = s1; jump(ctx, 0x088915E0u); }
void func_08881D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 3; G[2] = (G[5] << 5) - G[3] - G[5]; G[5] = G[5] < 4u ? 1u : 0u; G[3] = G[2] << 3; G[6] &= 0xFFu; if (G[5] == 0u) return; G[2] = word(rt, G[4] + 0xCu) + G[3]; store8(G[2] + 0xDC4u, G[6]); G[3] = word(rt, G[4] + 0xCu); G[2] = word(rt, G[3] + 0x16E0u) | 0x20u; put(rt, G[3] + 0x16E0u, G[2]); }
void func_0888832C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[8] = G[4] + 8u; G[3] = 0x08CC18E0u; put(rt, G[4], G[3]); G[4] = G[8]; put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[8] + 4u, 0u); put(rt, G[8] + 8u, 0u); put(rt, G[2] + 0x14u, G[5]); G[5] = 0x10u; put(rt, G[2] + 0x24u, G[7]); put(rt, G[2] + 0x20u, G[6]); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 0x1Cu, 0u); jump(ctx, 0x08BA81B4u); }
void func_08893AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x120u, 0u); store16(G[4], 0u); store8(G[4] + 2u, 0u); put(rt, G[4] + 8u, 0u); store16(G[4] + 0xCu, 0u); store16(G[4] + 0xEu, 0u); put(rt, G[4] + 0x10u, 0u); store16(G[4] + 0x14u, 0u); store8(G[4] + 0x138u, 0u); store8(G[4] + 0x139u, 0u); store8(G[4] + 0x124u, 0u); for (std::uint32_t offset = 0x143u; offset < 0x148u; ++offset) store8(G[4] + offset, 0u); }
void func_08898E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC2020u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x20u; call_here(rt, ctx, 0x088978B4u); G[2] = 0x08CC3E60u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_0889D674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16]); call_here(rt, ctx, G[2]); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_088A9A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0xCu); G[16] = G[4]; if (G[2] == 0u) { G[3] = word(rt, G[4] + 8u); if (G[3] != 0u) { G[4] = word(rt, G[3] + 0xCu); G[5] = word(rt, G[16] + 4u); call_here(rt, ctx, 0x0893C780u); put(rt, G[16] + 0xCu, G[2]); } } } G[16] = s0; }
void func_088B5DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[13] = float_at(rt, G[4] + 8u); ctx.fpr[12] = float_at(rt, G[4]); { GuestFrame frame(ctx, 0x10u); ctx.fpr[13] = -ctx.fpr[13]; ctx.fpr[12] = -ctx.fpr[12]; call_here(rt, ctx, 0x08B4A430u); G[1] = 0x4622F983u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); } G[2] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[2]); G[2] = sext(G[2], 16u); }
void func_088B5E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[13] = float_at(rt, G[4] + 4u); ctx.fpr[12] = float_at(rt, G[4]); { GuestFrame frame(ctx, 0x10u); ctx.fpr[13] = -ctx.fpr[13]; ctx.fpr[12] = -ctx.fpr[12]; call_here(rt, ctx, 0x08B4A430u); G[1] = 0x4622F983u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); } G[2] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[2]); G[2] = sext(G[2], 16u); }
void func_088E8518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x30u; G[2] = load8(G[4] + 0x26u); G[5] &= 0xFFu; if (G[2] == 0u) call_here(rt, ctx, 0x088D08B8u); G[4] = G[16]; G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; } G[16] = s0; jump(ctx, 0x088E84B8u); }
void func_088F4A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16]); call_here(rt, ctx, G[2]); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_088F64D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0xCu; offset += 4u) { ctx.fpr[0] = float_at(rt, G[4] + offset); if (ctx.fpr[12] < ctx.fpr[0]) put(rt, G[4] + offset, bits_of(ctx.fpr[12])); } }
void func_088F67C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[4] + 4u); ctx.fpr[0] = 0.0f; if (ctx.fpr[3] == ctx.fpr[0]) return; ctx.fpr[0] = float_at(rt, G[4]); ctx.fpr[1] = float_at(rt, G[4] + 8u); ctx.fpr[2] = float_at(rt, G[4] + 0xCu); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[13]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[0] = -ctx.fpr[0]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[3]; }
void func_089059D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x58u; call_here(rt, ctx, 0x0890599Cu); for (std::uint32_t offset = 0x68u; offset < 0x98u; offset += 0x10u) { G[4] = G[16] + offset; call_here(rt, ctx, 0x0890599Cu); } G[4] = G[16] + 0x98u; } G[16] = s0; jump(ctx, 0x0890599Cu); }
void func_089060C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x089060A4u); for (std::uint32_t offset = 8u; offset < 0x14u; offset += 4u) { G[4] = word(rt, G[16] + offset); call_here(rt, ctx, 0x089060A4u); } G[4] = word(rt, G[16] + 0x14u); } G[16] = s0; jump(ctx, 0x089060A4u); }
void func_089163C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[4] = 0x08CF205Cu; ctx.fpr[3] = float_at(rt, G[5] + 0xCu); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 4u); ctx.fpr[2] = float_at(rt, G[5] + 8u); G[5] = frame.at(0u); put(rt, frame.at(0u), bits_of(ctx.fpr[0])); put(rt, frame.at(4u), bits_of(ctx.fpr[1])); put(rt, frame.at(8u), bits_of(ctx.fpr[2])); put(rt, frame.at(0xCu), bits_of(ctx.fpr[3])); call_here(rt, ctx, 0x0891635Cu); }
void func_08917C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = word(rt, G[5] + 4u); if (static_cast<std::int32_t>(G[8]) <= 0) { G[4] = word(rt, G[5] + 8u); G[2] = G[4]; return; } G[3] = word(rt, G[5]); G[4] = 0u; for (;;) { G[2] = word(rt, G[3]); if (G[2] == G[6]) { G[2] = word(rt, G[3] + 4u); if (G[2] == G[7]) { G[2] = G[4]; return; } } G[4] += 1u; G[3] += 8u; if (G[4] == G[8]) break; } G[4] = word(rt, G[5] + 8u); G[2] = G[4]; }
void func_0892025C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08925FE4u); G[2] = 0x08CC5F60u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 8u, G[17]); } G[16] = s0; G[17] = s1; jump(ctx, 0x0892020Cu); }
void func_089202A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[0] = -ctx.fpr[15]; const bool below = ctx.fpr[12] < ctx.fpr[0]; ctx.fpr[2] = ctx.fpr[15] + ctx.fpr[12]; if (!below) { ctx.fpr[2] = ctx.fpr[12] - ctx.fpr[15]; if (!(ctx.fpr[15] < ctx.fpr[12])) { ctx.fpr[0] = ctx.fpr[14]; return; } } G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[1] = ctx.fpr[0] - ctx.fpr[15]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; ctx.fpr[0] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[14] = psp_mul(ctx.fpr[0], ctx.fpr[13]); ctx.fpr[0] = ctx.fpr[14]; }
void func_089300CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[5] = G[16]; call_here(rt, ctx, 0x089366ECu); G[4] = G[2]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089380D8u); }
void func_08931044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[5] = G[16]; call_here(rt, ctx, 0x089366ECu); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) { G[2] = word(rt, G[2] + 4u); G[3] = word(rt, G[2] + 4u); } G[2] = G[3]; } G[16] = s0; }
void func_08931B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67A8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = 0xA0u; call_here(rt, ctx, 0x08937AF4u); put(rt, G[16] + 4u, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x0893168Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08931B58u); }
void func_08937BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[2] = 0x08CC67C8u; put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08938E48u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08937E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[2] = 0x08CC67D8u; put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08938E48u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_089383E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = (G[5] & 0xFFu) << 1; G[3] = word(rt, G[4] + 0x14u) + G[2]; G[2] = load8(G[3] + 1u); G[3] = load8(G[3]); if (G[2] != 0u) { store8(G[6], G[3]); return; } call_here(rt, ctx, 0x089382B0u); G[2] &= 0xFFu; }
void func_089385F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 4u); G[2] = 0x08CC67E8u; put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08938E48u); G[4] = G[16] + 0x28u; } G[16] = s0; jump(ctx, 0x0893A068u); }
void func_08938940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x1F0u); G[17] = G[6]; G[6] = frame.at(0u); G[16] = G[4]; call_here(rt, ctx, 0x08938428u); G[4] = G[16]; G[5] = frame.at(0u); G[6] = G[17]; call_here(rt, ctx, 0x08938808u); } G[16] = s0; G[17] = s1; }
void func_08938EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67F8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0xA0u; call_here(rt, ctx, 0x08939FA4u); put(rt, G[16] + 4u, 0u); G[4] = G[16]; put(rt, G[16] + 0x1Cu, 0u); store8(G[16] + 0x20u, 0u); } G[16] = s0; jump(ctx, 0x08938E50u); }
void func_08938F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67F8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08938F18u); G[4] = G[16]; call_here(rt, ctx, 0x08938F50u); G[4] = G[16] + 0xA0u; call_here(rt, ctx, 0x0893A068u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08939370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = (G[5] & 0xFFu) << 1; G[3] = word(rt, G[4] + 0x10u) + G[2]; G[2] = load8(G[3] + 1u); G[3] = load8(G[3]); if (G[2] != 0u) { store8(G[6], G[3]); return; } call_here(rt, ctx, 0x0893923Cu); G[2] &= 0xFFu; }
void func_08939898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[6] = word(rt, G[4] + 4u); G[3] = word(rt, G[6] + 8u); G[8] = G[5] << 3; if (G[3] != G[2]) { call_here(rt, ctx, 0x08938EF0u); G[2] = word(rt, G[2] + 4u); return; } G[2] = word(rt, G[4] + 8u) + G[8]; G[2] = word(rt, G[2] + 4u); }
void func_089398DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[6] = word(rt, G[4] + 4u); G[3] = word(rt, G[6] + 8u); G[8] = G[5] << 3; if (G[3] != G[2]) { call_here(rt, ctx, 0x08938EF0u); G[2] = word(rt, G[2] + 0u); return; } G[2] = word(rt, G[4] + 8u) + G[8]; G[2] = word(rt, G[2] + 0u); }
void func_0893B9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = load8(G[5]); G[3] = load8(G[4]); G[2] = 0xFFu; if (G[3] < G[6]) return; G[7] = G[3] + 1u; if (G[3] != G[2]) { store8(G[5], G[7]); return; } G[2] = G[6] - 1u; if (G[6] == 0u) G[2] = 0xFFFFFFFFu; store8(G[4], G[2]); }
void func_089479EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[0] = std::bit_cast<float>(G[4]); G[1] = 0x42440000u; ctx.fpr[2] = 49.0f; ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[4])); G[4] = static_cast<std::int32_t>(G[4]) < 0x65 ? 1u : 0u; ctx.fpr[0] = ctx.fpr[2]; if (G[4] == 0u) { G[1] = 0x47BA6236u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; if (ctx.fpr[2] < ctx.fpr[0]) ctx.fpr[0] = ctx.fpr[2]; } ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); }
void func_0894A5BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x0894A520u); G[1] = 0x40800000u; ctx.fpr[3] = 4.0f; ctx.fpr[1] = float_at(rt, G[16] + 0x6DCu); ctx.fpr[2] = float_at(rt, G[16] + 0x6F8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[3]); ctx.fpr[1] = ctx.fpr[1] / ctx.fpr[0]; put(rt, G[16] + 0x6C0u, bits_of(ctx.fpr[1])); } G[16] = s0; }
void func_08957AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); ctx.fpr[1] = ctx.fpr[12]; ctx.fpr[0] = 0.0f; ctx.fpr[12] = float_at(rt, G[2] + 0x1410u); const bool low = ctx.fpr[1] <= ctx.fpr[12]; ctx.fpr[2] = float_at(rt, G[2] + 0x1414u); if (low) return; const bool high = ctx.fpr[2] <= ctx.fpr[1]; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[2] = ctx.fpr[2] - ctx.fpr[12]; ctx.fpr[12] = ctx.fpr[1] - ctx.fpr[12]; if (high) return; ctx.fpr[0] = ctx.fpr[12] / ctx.fpr[2]; }
void func_08965A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = 0x08CC6970u; G[3] = word(rt, G[4] + 0x20u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089BA380u); }
void func_08966174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966168u); G[4] = G[16]; G[3] = 1u; if (G[2] == 0u) { call_here(rt, ctx, 0x08965CE8u); G[3] = G[2] != 0u ? 1u : 0u; } G[2] = G[3]; } G[16] = s0; }
void func_089899A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x100u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_08991B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089FF16Cu); G[2] = 0x08CCC310u; G[3] = 0x08CCC4F0u; put(rt, G[16], G[2]); put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0xA0u, 0u); put(rt, G[16] + 0x9Cu, 0u); } G[16] = s0; }
void func_089B4D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCB198u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CCB3D4u; G[4] += 0x22Cu; put(rt, G[16] + 0x38u, G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089E2430u); }
void func_089B932C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089BB804u); G[2] = 0x08CCA7C8u; put(rt, G[16], G[2]); G[2] = 0x08CCA978u; put(rt, G[16] + 0x38u, G[2]); G[5] = G[16]; G[4] = G[16] + 0x64u; } G[16] = s0; jump(ctx, 0x089B883Cu); }
void func_089B994C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCA7C8u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CCA978u; G[4] += 0x64u; put(rt, G[16] + 0x38u, G[2]); call_here(rt, ctx, 0x089B8884u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089BBAC8u); }
void func_089DA90C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x170u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_089DB588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x190u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_089F51E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x089E4AECu); G[2] = 0x08CCFB80u; put(rt, G[16], G[2]); G[3] = word(rt, G[17] + 0x14Cu); put(rt, G[16] + 0x20u, G[3]); } G[16] = s0; G[17] = s1; }
void func_089FFDA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC73A8u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CC7588u; G[4] += 0x78u; put(rt, G[16] + 0x38u, G[2]); call_here(rt, ctx, 0x089B8884u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A101F8u); }
void func_08A0761C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x16Cu); G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; G[3] = 1u; if (!(ctx.fpr[0] == ctx.fpr[1])) G[3] = 0u; else { ctx.fpr[0] = float_at(rt, G[4] + 0x18Cu); if (!(ctx.fpr[0] == ctx.fpr[1])) G[3] = 0u; } G[2] = ins(word(rt, G[4] + 0x1A0u), G[3], 4u); put(rt, G[4] + 0x1A0u, G[2]); }
void func_08A18088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[10] = G[5]; G[4] = G[5]; G[3] = word(rt, G[2]); G[8] = G[6]; G[2] = G[6]; G[5] = 0u; G[6] = 0u; G[9] = G[7]; if (G[3] != 0u) { G[4] = G[10]; G[5] = 0u; G[6] = 0u; } else { G[7] = G[2]; G[8] = G[9]; } jump(ctx, 0x0896A35Cu); }
void func_08A19020(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = G[6]; G[16] = G[4] + 0x14u; G[4] = frame.at(0u); call_here(rt, ctx, 0x089BB3A8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A1F130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A21550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD0808u; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[16] + 0x1Cu, G[1]); put(rt, G[16], G[2]); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x18u, 0u); } G[16] = s0; }
void func_08A215F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x08A2159Cu); G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08ABC318u); put(rt, G[17] + 0x14u, G[2]); G[2] = G[2] != 0u ? 1u : 0u; } G[16] = s0; G[17] = s1; }
void func_08A21AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD0808u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A2159Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x20u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A237A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[6] = 0u; G[2] = word(rt, G[4] + 0x6Cu); G[16] = G[4]; if (G[2] != 0u) { call_here(rt, ctx, 0x08AB9144u); G[4] = word(rt, G[16] + 0x6Cu); call_here(rt, ctx, 0x08B35254u); put(rt, G[16] + 0x6Cu, 0u); } } G[16] = s0; }
void func_08A2741C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A29A94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 1u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A29AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A30D18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A30EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A31AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A3AEA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBB9A0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A3A6FCu); G[4] = G[16] + 0x34u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A3EB94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1190u; put(rt, G[16], G[2]); G[4] = G[16] + 0x18u; G[5] = 0u; G[6] = 0u; G[7] = 0u; G[8] = 0u; } G[16] = s0; jump(ctx, 0x08B215B8u); }
void func_08A4F4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD0000u; G[7] = word(rt, 0x08CDF56Cu); G[2] = high_half(0x08CDF574u); if (static_cast<std::int32_t>(G[7]) <= 0) return; G[4] = 0x08CDF574u; G[6] = 0u; G[5] = 0u; do { G[2] = load16(G[4] - 2u); G[3] = load16(G[4] - 4u); G[5] += 1u; store16(G[4], G[6]); G[2] -= G[3]; G[6] += G[2]; G[4] += 6u; } while (G[5] != G[7]); }
void func_08A50CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, 0x08CF68E8u); { GuestFrame frame(ctx, 0x10u); G[4] = word(rt, G[3] + 0x6Cu); if (G[4] != 0u) call_here(rt, ctx, 0x08B56940u); G[4] = 0x8002u; call_here(rt, ctx, 0x08B54FB8u); G[4] = 2u; call_here(rt, ctx, 0x08B54FB8u); G[4] = 0x6Du; } jump(ctx, 0x08B58874u); }
void func_08A5F5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x14u); G[4] = G[16]; call_here(rt, ctx, 0x08B71CE0u); G[4] = G[2]; G[5] = G[3]; G[6] = 0u; G[7] = 0x41F00000u; if (static_cast<std::int32_t>(G[16]) < 0) call_here(rt, ctx, 0x08B71328u); } G[16] = s0; }
void func_08A97534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = sext(load8(G[4] + 0x14u), 8u); G[4] = G[16]; call_here(rt, ctx, 0x08B71CE0u); G[4] = G[2]; G[5] = G[3]; G[6] = 0u; G[7] = 0x40700000u; if (static_cast<std::int32_t>(G[16]) < 0) call_here(rt, ctx, 0x08B71328u); } G[16] = s0; }
void func_08A6D290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB7328u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B88F44u); }
void func_08A73420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = sext(load16(G[5] + 0x14u), 16u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A7334Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08A744AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A725A0u); G[2] = 0x08CD3A18u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A76ABCu); }
void func_08A74A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A80214u); G[2] = 0x08CD3A58u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A90480u); }
void func_08A74EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A86BDCu); G[2] = 0x08CD3AA0u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A90480u); }
void func_08A7B500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A5D5A0u); G[2] = 0x08CD40C8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A7C0F0u); }
void func_08A7B604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = G[6]; G[16] = G[4] + 0x14u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A7C3A4u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A887F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; ctx.fpr[12] = float_at(rt, G[4] + 0x14u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A88D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x120u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08B0C56Cu); G[4] = word(rt, G[16] + 0x140u); G[3] = word(rt, G[16] + 0x13Cu) - G[4]; G[2] -= G[3]; } G[16] = s0; }
void func_08A88DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x120u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08B0C778u); G[3] = 2u; put(rt, G[16] + 0x14u, 2u); put(rt, G[16] + 0x140u, 0u); put(rt, G[16] + 0x13Cu, 0u); } G[16] = s0; }
void func_08A89528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A894C8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A89D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB7218u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B88F44u); }
void func_08A919F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08A5D5A0u); G[2] = 0x08CD43D8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A92500u); }
void func_08A939B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB208Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xD8u); G[25] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08A942A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD3B28u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A93704u); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x1Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A72308u); }
void func_08A955F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A951B8u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A95680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = 0x08CBE0E0u; G[3] = word(rt, G[4] + 0x14u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08B35254u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A98A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB7108u; G[3] = G[16] + 0x14u; put(rt, G[16], G[2]); put(rt, G[16] + 0x14u, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 4u, 0u); store8(G[16] + 0x20u, 0u); } G[16] = s0; }
void func_08A9E138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A9E820u); G[2] = 0x08CD48E0u; put(rt, G[16], G[2]); put(rt, G[16] + 0x20u, G[17]); put(rt, G[16] + 0x24u, 0u); } G[16] = s0; G[17] = s1; }
void func_08AA2028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = sext(G[5], 16u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A7334Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AA206C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = sext(G[5], 8u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A960C0u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AA2854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x0u); G[4] = 0x08CB2B50u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = word(rt, G[2] + 0x58u); G[25] = G[5]; } G[16] = s0; jump(ctx, G[25]); }
void func_08AA2964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x0u); G[4] = 0x08CB2B50u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x48u); G[4] = G[16]; call_here(rt, ctx, G[2]); } G[16] = s0; }
void func_08AA29A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x0u); G[4] = 0x08CB2B50u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); } G[16] = s0; }
void func_08AA29EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x0u); G[4] = 0x08CB2B50u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x40u); G[25] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08AA2B88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] &= 0xFFu; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB19B0u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AA2BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] &= 0xFFFFu; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB022Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AAE2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = 0x08CDFA60u; G[6] = frame.at(0u); G[16] = G[4]; G[2] = (word(rt, G[5] + 0x14u) << 2) + G[3]; G[5] = word(rt, G[2]); call_here(rt, ctx, 0x08B85EB8u); G[2] = G[16]; } G[16] = s0; }
void func_08AAFD80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = sext(load16(G[4] + 0x14u), 16u); G[4] = G[16]; call_here(rt, ctx, 0x08B71CE0u); G[4] = G[2]; G[5] = G[3]; G[6] = 0u; G[7] = 0x40F00000u; if (static_cast<std::int32_t>(G[16]) < 0) call_here(rt, ctx, 0x08B71328u); } G[16] = s0; }
void func_08AB1504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = sext(load8(G[4] + 0x14u), 8u); G[4] = G[16]; call_here(rt, ctx, 0x08B71CE0u); G[4] = G[2]; G[5] = G[3]; G[6] = 0u; G[7] = 0x40700000u; if (static_cast<std::int32_t>(G[16]) < 0) call_here(rt, ctx, 0x08B71328u); } G[16] = s0; }
void func_08AB0300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = load16(G[5] + 0x14u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB022Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AB1A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = load8(G[5] + 0x14u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB19B0u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08AB3E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] += 0x24u; G[16] = G[5]; call_here(rt, ctx, 0x08B855E0u); G[4] = G[17] + 0x14u; G[5] = word(rt, G[16]); call_here(rt, ctx, 0x08B1EA48u); G[2] &= 0xFFu; } G[16] = s0; G[17] = s1; }
void func_08ABFC48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[7]; G[16] = G[4]; call_here(rt, ctx, 0x08C3C654u); put(rt, G[16] + 0x10u, G[17]); put(rt, G[16] + 0xCu, G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AC2460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[4] + 0x14u); G[18] = word(rt, G[2] + 0xCu); call_here(rt, ctx, 0x08AC26BCu); G[5] = G[2]; G[4] = G[16]; G[25] = G[18]; } G[16] = s0; G[18] = s2; jump(ctx, G[25]); }
void func_08AC322C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 4u); G[16] = G[4]; if (G[2] != 0u) { G[4] = word(rt, G[4]); G[2] = 1u; store8(G[16] + 5u, 1u); store8(G[16] + 4u, 0u); call_here(rt, ctx, 0x08B1B99Cu); put(rt, G[16], 0u); } G[2] = 1u; } G[16] = s0; }
void func_08AC43FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6690u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B0C808u); G[2] = 0x08CD6718u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08ACA3F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x80001002u; G[16] = G[4]; G[3] = word(rt, G[4]); G[4] = G[3]; if (G[3] != 0u) { call_here(rt, ctx, 0x08ACA3B0u); G[3] = word(rt, G[16] + 4u); G[2] = G[3] < G[2] ? 1u : 0u; } } G[16] = s0; }
void func_08ACB788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[7] = G[4] + 0x60u; G[8] = G[5] + 0x40u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6]); put(rt, G[7], G[2]); }
void func_08AD5394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AD5358u); G[2] = load8(G[16] + 0xCu); if (G[2] != 0u) { G[4] = word(rt, G[16] + 4u); call_here(rt, ctx, 0x08B35254u); store8(G[16] + 0xCu, 0u); put(rt, G[16] + 8u, 0u); put(rt, G[16] + 4u, 0u); } } G[16] = s0; }
void func_08AD690C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B2B6F4u); G[4] = G[2]; call_here(rt, ctx, 0x08B35254u); G[2] = word(rt, G[16] + 0x20u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08B64654u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08C3E128u); }
void func_08AD89EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; G[2] = load8(G[5]); if (G[2] != 0u) { G[6] -= 1u; if (static_cast<std::int32_t>(G[6]) > 0) { for (;;) { store8(G[4], G[2]); G[5] += 1u; G[7] += 1u; G[2] = load8(G[5]); G[3] = static_cast<std::int32_t>(G[7]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[4] += 1u; if (G[2] == 0u || G[3] == 0u) break; } } } store8(G[4], 0u); }
void func_08ADAAC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C90CB4u; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD9E74u); G[4] = G[2]; G[2] = 0u; if (G[4] == 0u) return; G[3] = word(rt, G[4] + 0x2Cu); G[2] = word(rt, G[4] + 0x38u); G[4] = word(rt, G[4] + 0x40u); G[2] = G[3] - G[2]; G[3] -= G[4]; if (static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2])) G[2] = G[3]; }
void func_08AED98C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[3] = G[7]; G[2] = G[6]; G[10] = G[8]; G[6] = G[5]; G[7] = G[2]; G[11] = G[9]; G[8] = G[3]; G[5] = 0u; G[9] = 0u; G[2] = 0xFFFFFFFFu; put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08AED688u); }
void func_08AF322C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = -2.0f; ctx.fpr[15] = psp_mul(ctx.fpr[15], ctx.fpr[0]); ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[0]); ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[12]; ctx.fpr[14] = ctx.fpr[14] + ctx.fpr[14]; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[15] = ctx.fpr[15] + ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[0]; ctx.fpr[13] = ctx.fpr[13] + ctx.fpr[0]; ctx.fpr[14] = ctx.fpr[14] - ctx.fpr[0]; put(rt, G[4] + 0x10u, bits_of(ctx.fpr[15])); put(rt, G[4] + 4u, bits_of(ctx.fpr[12])); put(rt, G[4] + 8u, bits_of(ctx.fpr[13])); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[14])); }
void func_08B088AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4]); G[4] = G[16] + 0x10u; if (G[16] == 0u) { G[16] = s0; return; } call_here(rt, ctx, 0x08B09290u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B08B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x1ACu; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2] + 0x10u; call_here(rt, ctx, 0x08B09F98u); put(rt, G[17], G[16]); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B08AE0u); }
void func_08B09660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = 0x10u; G[17] = G[5]; G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08B36018u); G[4] = G[16]; G[5] = G[17]; G[6] = 0x10u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B7C830u); }
void func_08B0C7C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7B80u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B0C778u); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x08B0DDF0u); G[4] = G[16]; call_here(rt, ctx, 0x08B0C1D8u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B0E8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7C68u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD7C98u; G[4] += 0xF8u; put(rt, G[16] + 0xF8u, G[2]); call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B0DDDCu); }
void func_08B0E920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7C68u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD7C98u; G[4] += 0xF8u; put(rt, G[16] + 0xF8u, G[2]); call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B0DDDCu); }
void func_08B106AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4] + 0x54u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[4] = G[2]; tail = G[2] != 0u; } } if (tail) jump(ctx, 0x08B0C290u); }
void func_08B17CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD91C8u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD7C98u; G[4] += 0xA0u; put(rt, G[16] + 0xA0u, G[2]); call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B11918u); }
void func_08B19F60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = load8(G[4] + 0x60u); if (G[2] != 0u) { store8(G[4] + 0x60u, 0u); call_here(rt, ctx, 0x08B28C74u); G[4] = word(rt, G[16] + 0x5Cu); call_here(rt, ctx, 0x08B1B99Cu); put(rt, G[16] + 0x5Cu, 0u); } } G[16] = s0; }
void func_08B1BB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x411Au; put(rt, G[4] + 0x48u, G[2]); put(rt, G[4] + 0x4Cu, 0u); put(rt, G[4] + 0x40u, 0u); put(rt, G[4] + 0x44u, 0u); G[5] = 1u; G[3] = 0u; G[2] = 2u; put(rt, G[4], 2u); put(rt, G[4] + 0xCu, 1u); put(rt, G[4] + 4u, 1u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); }
void func_08B1C764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9340u; G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B1C68Cu); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B34DA0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B22AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9588u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD95A0u; G[4] += 0x2Cu; put(rt, G[16] + 0x2Cu, G[2]); call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_08B32350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, 0x08CE0C88u); G[4] = G[3]; if (static_cast<std::int32_t>(G[3]) >= 0) call_here(rt, ctx, 0x08C7AF28u); call_here(rt, ctx, 0x08C7AF40u); call_here(rt, ctx, 0x08C7AF58u); G[4] = 0x08D690FCu; } jump(ctx, 0x08B2EE2Cu); }
void func_08B32994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDA730u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B3252Cu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08B31918u); G[2] = 0x08CDA7A8u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08B3E8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; G[6] = 0x320u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x320u; G[5] = 0u; G[6] = 0x320u; call_here(rt, ctx, 0x08B36018u); put(rt, G[16] + 0x644u, 0u); put(rt, G[16] + 0x640u, 0u); } G[16] = s0; }
void func_08B3FF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6] & 0xFFFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B3F838u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B3FA3Cu); G[4] = G[16]; G[5] = 2u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3F7A8u); }
void func_08B404C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0xCCu; G[2] = word(rt, G[4] + 4u); if (G[2] != 0u) { call_here(rt, ctx, 0x08B2C4DCu); } G[2] = word(rt, G[16] + 0xC8u); if (G[2] != 0u) put(rt, G[16] + 0xC8u, 0u); } G[16] = s0; }
void func_08B44818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDA9A0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B404C8u); G[4] = G[16] + 0x12Cu; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0xCCu; call_here(rt, ctx, 0x08B2C3BCu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B08420u); }
void func_08B49814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B497A0u); G[3] = G[2]; G[2] = G[2] < 0xEu ? 1u : 0u; G[4] = 0u; if (G[2] != 0u) { put(rt, G[16], G[3]); G[4] = 1u; } G[2] = G[4]; } G[16] = s0; }
void func_08B4AC9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[3] = float_at(rt, G[6] + 4u); ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[2] = float_at(rt, G[5] + 0xCu); ctx.fpr[3] = float_at(rt, G[6] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 4u); ctx.fpr[1] = float_at(rt, G[6]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[3]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); }
void func_08B59B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[5]); G[8] = (G[3] << 4) + G[4]; G[2] = 0xFFu; G[9] = G[7] & 0xFFu; if (G[3] == G[2]) store8(G[5] + 1u, G[9]); else store8(G[8] + 2u, G[9]); G[2] = (G[7] << 4) + G[4]; store8(G[2] + 3u, G[3]); G[3] = 0xFFFFFFFFu; store8(G[2], G[6]); store8(G[2] + 2u, G[3]); store8(G[5], G[9]); }
void func_08B59B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = load8(G[5] + 1u); G[3] = (G[8] << 4) + G[4]; G[2] = 0xFFu; G[9] = G[7] & 0xFFu; if (G[8] == G[2]) store8(G[5], G[9]); else store8(G[3] + 3u, G[9]); G[2] = (G[7] << 4) + G[4]; G[3] = 0xFFFFFFFFu; store8(G[2] + 3u, G[3]); store8(G[2], G[6]); store8(G[2] + 2u, G[8]); store8(G[5] + 1u, G[9]); }
void func_08B6475C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDC110u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08B7831C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[3] = word(rt, G[2] + 0x38u); if (G[3] == 0u) { call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; call_here(rt, ctx, 0x08B78430u); } G[2] = sext(load16(G[16] + 0xEu), 16u); } G[16] = s0; }
void func_08B853DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x14u); G[4] = 0x08C97B80u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x18u); G[25] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08B86D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x14u); G[4] = 0x08C97DC4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x120u); G[25] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08B87440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB8400u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x38u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CB7440u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A7D4E4u); }
void func_08B886C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x100u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_08B8E67C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; if (G[16] != 0u) { call_here(rt, ctx, 0x0890AFC0u); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08B9AE20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBFF08u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x0887C8E8u); G[2] = 0x08CD7118u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08BABEFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC2020u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x20u; call_here(rt, ctx, 0x088978B4u); G[2] = 0x08CC3E60u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08BB9298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; if (G[16] != 0u) { call_here(rt, ctx, 0x088F40CCu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08BBBF50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0x10u; offset += 4u) { ctx.fpr[0] = psp_mul(float_at(rt, G[5] + offset), float_at(rt, G[6] + offset)); put(rt, G[4] + offset, bits_of(ctx.fpr[0])); } }
void func_08BBC0A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); call_here(rt, ctx, 0x08B35254u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08BC3328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x10u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC5190u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[2] + 8u, G[4]); put(rt, G[2] + 0xCu, G[5]); } G[16] = s0; }
void func_08BC336C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x10u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC5148u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[2] + 8u, G[4]); put(rt, G[2] + 0xCu, G[5]); } G[16] = s0; }
void func_08BCF5D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08BBD250u); G[5] = frame.at(0u); G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08BCE37Cu); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B18A98u); G[2] = G[16]; } G[16] = s0; }
void func_08BDBDA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; if (G[16] != 0u) { call_here(rt, ctx, 0x08910D2Cu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08BDBF9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0xCu); G[4] = G[16]; if (G[16] != 0u) { call_here(rt, ctx, 0x08916AB8u); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08BE571C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x100u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_08BE6200(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x140u; G[4] = G[16]; G[17] = G[5]; if (G[5] != G[16]) { call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_08BE73DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = 0x08CC9628u; G[3] = word(rt, G[4] + 0x24u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08965A3Cu); }
void func_08BE8B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = 0x08CC9628u; G[3] = word(rt, G[4] + 0x24u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08965A3Cu); }
void func_08BE8BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = 0x08CC9628u; G[3] = word(rt, G[4] + 0x24u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08965A3Cu); }
void func_08C19BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; if (G[16] != 0u) { call_here(rt, ctx, 0x08A24B9Cu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08C1A4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[6] = 1u; G[16] = G[4]; call_here(rt, ctx, 0x08C1A47Cu); G[2] = 0x08CD1DD0u; put(rt, G[16] + 0x18u, bits_of(ctx.fpr[20])); put(rt, G[16], G[2]); } G[16] = s0; ctx.fpr[20] = f20; }
void func_08C1A538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6] & 0xFFu; G[6] = 1u; G[16] = G[4]; call_here(rt, ctx, 0x08C1A4FCu); G[2] = 0x08CD1DE8u; store8(G[16] + 0x18u, G[17]); put(rt, G[16], G[2]); } G[16] = s0; G[17] = s1; }
void func_08C1DD08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD1F18u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CD1F30u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08A5B9ECu); }
void func_08C1EB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD22B8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CD1F30u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08A5B9ECu); }
void func_08C211D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[2] = word(rt, G[4] + 8u); G[4] = frame.at(0u); G[5] = G[2]; call_here(rt, ctx, 0x08A5FA8Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A61584u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08C21258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[6] = word(rt, G[4] + 8u); G[7] = word(rt, G[4] + 0xCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8B098u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A61584u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08C28558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD4168u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CD1F30u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08A5B9ECu); }
void func_08C29860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD41C8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CD1F30u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08A5B9ECu); }
void func_08C2AD44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = G[16] + 0u; do { call_here(rt, ctx, 0x08A5B9ECu); G[16] += 4u; G[4] = G[16] + 0u; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2ADCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = G[16] + 4u; do { call_here(rt, ctx, 0x08A5B9ECu); G[16] += 0xCu; G[4] = G[16] + 4u; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2DE90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD48A8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CBE7C8u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08C35198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = G[16] + 4u; do { call_here(rt, ctx, 0x08A5B9ECu); G[16] += 0xCu; G[4] = G[16] + 4u; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C351DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = G[16] + 0u; do { call_here(rt, ctx, 0x08A5B9ECu); G[16] += 4u; G[4] = G[16] + 0u; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C3BACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[4] != G[5]) { G[4] = G[16] + 0u; do { call_here(rt, ctx, 0x08A5B9ECu); G[16] += 4u; G[4] = G[16] + 0u; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C3C740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD64B8u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD64E8u; G[4] += 0xF8u; put(rt, G[16] + 0xF8u, G[2]); call_here(rt, ctx, 0x08B0C23Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B0DDDCu); }
void func_08C4D06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x10u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD92C8u; put(rt, G[2], G[3]); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[2] + 8u, G[4]); put(rt, G[2] + 0xCu, G[5]); } G[16] = s0; }
void func_08C4E810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0xCu); G[4] = G[16]; if (G[16] != 0u) { call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08C54880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0x20u; G[2] = load8(G[4] + 0x75u); G[16] = G[4]; if (G[2] == 0u) { call_here(rt, ctx, 0x08C5477Cu); G[3] = 1u; store8(G[16] + 0x75u, 1u); store8(G[16] + 0x74u, G[2]); } G[2] = sext(load8(G[16] + 0x74u), 8u); } G[16] = s0; }
void func_08C5D3C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB838u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB838u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB800u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C5D650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB800u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C5D9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = G[6] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 4u, G[6]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5DA20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = G[6] != 0u ? 1u : 0u; G[2] = 0x08CDB798u; G[17] = G[4]; put(rt, G[4] + 4u, G[6]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60DB8u);  } G[16] = s0; G[17] = s1; }
void func_08C5E0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDB870u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C5E2F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB870u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C5E348(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = G[7] != 0u ? 1u : 0u; G[2] = 0x08CDB870u; G[17] = G[4]; put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C5E3A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDB7C0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C5E3F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] = G[5] != 0u ? 1u : 0u; G[2] = 0x08CDB7C0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B5C230u); put(rt, G[17] + 8u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C7089C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; G[3] = word(rt, G[5]); G[8] = word(rt, G[5] + 4u); G[5] = G[6]; G[2] = word(rt, G[3] - 0xCu); put(rt, G[4], G[3]); G[2] += G[4]; put(rt, G[2], G[8]); G[3] = word(rt, G[4]); G[4] = word(rt, G[3] - 0xCu) + G[7]; call_with(rt, ctx, 0x08C54A7Cu, 0x10u); }
void func_08C709F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDBD94u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CDB088u; G[4] += 4u; put(rt, G[16] + 4u, G[2]); call_here(rt, ctx, 0x08B6543Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08C723C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0xAu; G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu) + G[16]; call_here(rt, ctx, 0x08C5477Cu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08C72194u); G[4] = G[2]; } G[16] = s0; jump(ctx, 0x08C7030Cu); }
void func_08C73F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D77498u; G[16] = G[4]; if (G[4] != G[2]) { G[4] += 8u; G[5] = 1u; call_here(rt, ctx, 0x08B6085Cu); } G[2] = G[16] + 0xCu; } G[16] = s0; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_182
} // namespace gt::game

#define HOT182(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_182::name, gt::ReturnKind::kind)
HOT182(0x0880BC34u, func_0880BC34, Word);
HOT182(0x08821514u, func_08821514, Word);
HOT182(0x08857618u, func_08857618, Word);
HOT182(0x08821A98u, func_08821A98, Word);
HOT182(0x0885C848u, func_0885C848, Word);
HOT182(0x0885CB60u, func_0885CB60, Word);
HOT182(0x0885D56Cu, func_0885D56C, Word);
HOT182(0x0885E31Cu, func_0885E31C, Word);
HOT182(0x0885E68Cu, func_0885E68C, Word);
HOT182(0x0885EE7Cu, func_0885EE7C, Word);
HOT182(0x08881D40u, func_08881D40, Word);
HOT182(0x0888832Cu, func_0888832C, Word);
HOT182(0x08893AC8u, func_08893AC8, Word);
HOT182(0x08898E64u, func_08898E64, Word);
HOT182(0x0889D674u, func_0889D674, Word);
HOT182(0x088A9A0Cu, func_088A9A0C, Word);
HOT182(0x088B5DE0u, func_088B5DE0, Word);
HOT182(0x088B5E24u, func_088B5E24, Word);
HOT182(0x088E8518u, func_088E8518, Word);
HOT182(0x088F4A40u, func_088F4A40, Word);
HOT182(0x088F64D4u, func_088F64D4, Word);
HOT182(0x088F67C0u, func_088F67C0, Float);
HOT182(0x089059D4u, func_089059D4, Word);
HOT182(0x089060C8u, func_089060C8, Word);
HOT182(0x089163C4u, func_089163C4, Word);
HOT182(0x08917C2Cu, func_08917C2C, Word);
HOT182(0x0892025Cu, func_0892025C, Word);
HOT182(0x089202A0u, func_089202A0, Float);
HOT182(0x089300CCu, func_089300CC, Word);
HOT182(0x08931044u, func_08931044, Word);
HOT182(0x08931B74u, func_08931B74, Word);
HOT182(0x08937BB8u, func_08937BB8, Word);
HOT182(0x08937E78u, func_08937E78, Word);
HOT182(0x089383E4u, func_089383E4, Word);
HOT182(0x089385F4u, func_089385F4, Word);
// HOT182(0x08938940u, func_08938940, Word);  // no comparable synthetic case yet
HOT182(0x08938EACu, func_08938EAC, Word);
HOT182(0x08938F88u, func_08938F88, Word);
HOT182(0x08939370u, func_08939370, Word);
HOT182(0x08939898u, func_08939898, Word);
HOT182(0x089398DCu, func_089398DC, Word);
HOT182(0x0893B9C8u, func_0893B9C8, Word);
HOT182(0x089479ECu, func_089479EC, Float);
HOT182(0x0894A5BCu, func_0894A5BC, Word);
HOT182(0x08957AD4u, func_08957AD4, Float);
HOT182(0x08965A80u, func_08965A80, Word);
HOT182(0x08966174u, func_08966174, Word);
HOT182(0x089899A8u, func_089899A8, Word);
HOT182(0x08991B54u, func_08991B54, Word);
// HOT182(0x089B4D94u, func_089B4D94, Word);  // no comparable synthetic case yet
HOT182(0x089B932Cu, func_089B932C, Word);
HOT182(0x089B994Cu, func_089B994C, Word);
HOT182(0x089DA90Cu, func_089DA90C, Word);
HOT182(0x089DB588u, func_089DB588, Word);
HOT182(0x089F51E4u, func_089F51E4, Word);
HOT182(0x089FFDA0u, func_089FFDA0, Word);
HOT182(0x08A0761Cu, func_08A0761C, Word);
HOT182(0x08A18088u, func_08A18088, Word);
// HOT182(0x08A19020u, func_08A19020, Word);  // no comparable synthetic case yet
HOT182(0x08A1F130u, func_08A1F130, Word);
HOT182(0x08A21550u, func_08A21550, Word);
// HOT182(0x08A215F0u, func_08A215F0, Word);  // no comparable synthetic case yet
HOT182(0x08A21AB4u, func_08A21AB4, Word);
HOT182(0x08A237A8u, func_08A237A8, Word);
HOT182(0x08A2741Cu, func_08A2741C, Word);
HOT182(0x08A29A94u, func_08A29A94, Word);
HOT182(0x08A29AD8u, func_08A29AD8, Word);
HOT182(0x08A30D18u, func_08A30D18, Word);
HOT182(0x08A30EE0u, func_08A30EE0, Word);
HOT182(0x08A31AC8u, func_08A31AC8, Word);
HOT182(0x08A3AEA4u, func_08A3AEA4, Word);
HOT182(0x08A3EB94u, func_08A3EB94, Word);
HOT182(0x08A4F4D8u, func_08A4F4D8, Word);
HOT182(0x08A50CB0u, func_08A50CB0, Word);
HOT182(0x08A5F5A0u, func_08A5F5A0, DoubleWord);
HOT182(0x08A97534u, func_08A97534, DoubleWord);
HOT182(0x08A6D290u, func_08A6D290, Word);
HOT182(0x08A73420u, func_08A73420, Word);
HOT182(0x08A744ACu, func_08A744AC, Word);
HOT182(0x08A74A08u, func_08A74A08, Word);
HOT182(0x08A74EBCu, func_08A74EBC, Word);
HOT182(0x08A7B500u, func_08A7B500, Word);
HOT182(0x08A7B604u, func_08A7B604, Word);
HOT182(0x08A887F4u, func_08A887F4, Word);
HOT182(0x08A88D0Cu, func_08A88D0C, Word);
HOT182(0x08A88DA4u, func_08A88DA4, Word);
HOT182(0x08A89528u, func_08A89528, Word);
HOT182(0x08A89D64u, func_08A89D64, Word);
HOT182(0x08A919F4u, func_08A919F4, Word);
HOT182(0x08A939B0u, func_08A939B0, Word);
HOT182(0x08A942A0u, func_08A942A0, Word);
HOT182(0x08A955F8u, func_08A955F8, Word);
HOT182(0x08A95680u, func_08A95680, Word);
HOT182(0x08A98A2Cu, func_08A98A2C, Word);
HOT182(0x08A9E138u, func_08A9E138, Word);
HOT182(0x08AA2028u, func_08AA2028, Word);
HOT182(0x08AA206Cu, func_08AA206C, Word);
HOT182(0x08AA2854u, func_08AA2854, Word);
HOT182(0x08AA2964u, func_08AA2964, Word);
HOT182(0x08AA29A8u, func_08AA29A8, Word);
HOT182(0x08AA29ECu, func_08AA29EC, Word);
HOT182(0x08AA2B88u, func_08AA2B88, Word);
HOT182(0x08AA2BCCu, func_08AA2BCC, Word);
HOT182(0x08AAE2D4u, func_08AAE2D4, Word);
HOT182(0x08AAFD80u, func_08AAFD80, DoubleWord);
HOT182(0x08AB1504u, func_08AB1504, DoubleWord);
HOT182(0x08AB0300u, func_08AB0300, Word);
HOT182(0x08AB1A84u, func_08AB1A84, Word);
// HOT182(0x08AB3E04u, func_08AB3E04, Word);  // no comparable synthetic case yet
HOT182(0x08ABFC48u, func_08ABFC48, Word);
HOT182(0x08AC2460u, func_08AC2460, Word);
HOT182(0x08AC322Cu, func_08AC322C, Word);
HOT182(0x08AC43FCu, func_08AC43FC, Word);
HOT182(0x08ACA3F4u, func_08ACA3F4, Word);
HOT182(0x08ACB788u, func_08ACB788, Word);
HOT182(0x08AD5394u, func_08AD5394, Word);
HOT182(0x08AD690Cu, func_08AD690C, Word);
HOT182(0x08AD89ECu, func_08AD89EC, Word);
HOT182(0x08ADAAC8u, func_08ADAAC8, Word);
HOT182(0x08AED98Cu, func_08AED98C, Word);
HOT182(0x08AF322Cu, func_08AF322C, Word);
HOT182(0x08B088ACu, func_08B088AC, Word);
HOT182(0x08B08B1Cu, func_08B08B1C, Word);
HOT182(0x08B09660u, func_08B09660, Word);
HOT182(0x08B0C7C4u, func_08B0C7C4, Word);
HOT182(0x08B0E8DCu, func_08B0E8DC, Word);
HOT182(0x08B0E920u, func_08B0E920, Word);
HOT182(0x08B106ACu, func_08B106AC, Word);
HOT182(0x08B17CBCu, func_08B17CBC, Word);
HOT182(0x08B19F60u, func_08B19F60, Word);
HOT182(0x08B1BB00u, func_08B1BB00, Word);
HOT182(0x08B1C764u, func_08B1C764, Word);
HOT182(0x08B22AF0u, func_08B22AF0, Word);
// HOT182(0x08B32350u, func_08B32350, Word);  // no comparable synthetic case yet
// HOT182(0x08B32994u, func_08B32994, Word);  // no comparable synthetic case yet
HOT182(0x08B3E8B4u, func_08B3E8B4, Word);
HOT182(0x08B3FF44u, func_08B3FF44, Word);
HOT182(0x08B404C8u, func_08B404C8, Word);
HOT182(0x08B44818u, func_08B44818, Word);
HOT182(0x08B49814u, func_08B49814, Word);
HOT182(0x08B4AC9Cu, func_08B4AC9C, Word);
HOT182(0x08B59B38u, func_08B59B38, Word);
HOT182(0x08B59B7Cu, func_08B59B7C, Word);
HOT182(0x08B6475Cu, func_08B6475C, Word);
HOT182(0x08B7831Cu, func_08B7831C, Word);
HOT182(0x08B853DCu, func_08B853DC, Word);
HOT182(0x08B86D74u, func_08B86D74, Word);
HOT182(0x08B87440u, func_08B87440, Word);
HOT182(0x08B886C4u, func_08B886C4, Word);
HOT182(0x08B8E67Cu, func_08B8E67C, Word);
HOT182(0x08B9AE20u, func_08B9AE20, Word);
HOT182(0x08BABEFCu, func_08BABEFC, Word);
HOT182(0x08BB9298u, func_08BB9298, Word);
HOT182(0x08BBBF50u, func_08BBBF50, Word);
// HOT182(0x08BBC0A0u, func_08BBC0A0, Word);  // no comparable synthetic case yet
HOT182(0x08BC3328u, func_08BC3328, Word);
HOT182(0x08BC336Cu, func_08BC336C, Word);
HOT182(0x08BCF5D8u, func_08BCF5D8, Word);
HOT182(0x08BDBDA8u, func_08BDBDA8, Word);
HOT182(0x08BDBF9Cu, func_08BDBF9C, Word);
HOT182(0x08BE571Cu, func_08BE571C, Word);
HOT182(0x08BE6200u, func_08BE6200, Word);
HOT182(0x08BE73DCu, func_08BE73DC, Word);
HOT182(0x08BE8B34u, func_08BE8B34, Word);
HOT182(0x08BE8BCCu, func_08BE8BCC, Word);
HOT182(0x08C19BA8u, func_08C19BA8, Word);
HOT182(0x08C1A4B8u, func_08C1A4B8, Word);
HOT182(0x08C1A538u, func_08C1A538, Word);
HOT182(0x08C1DD08u, func_08C1DD08, Word);
HOT182(0x08C1EB1Cu, func_08C1EB1C, Word);
HOT182(0x08C211D0u, func_08C211D0, Word);
// HOT182(0x08C21258u, func_08C21258, Word);  // no comparable synthetic case yet
HOT182(0x08C28558u, func_08C28558, Word);
HOT182(0x08C29860u, func_08C29860, Word);
HOT182(0x08C2AD44u, func_08C2AD44, Word);
HOT182(0x08C2ADCCu, func_08C2ADCC, Word);
HOT182(0x08C2DE90u, func_08C2DE90, Word);
HOT182(0x08C35198u, func_08C35198, Word);
HOT182(0x08C351DCu, func_08C351DC, Word);
HOT182(0x08C3BACCu, func_08C3BACC, Word);
HOT182(0x08C3C740u, func_08C3C740, Word);
HOT182(0x08C4D06Cu, func_08C4D06C, Word);
HOT182(0x08C4E810u, func_08C4E810, Word);
HOT182(0x08C54880u, func_08C54880, Word);
HOT182(0x08C5D3C8u, func_08C5D3C8, Word);
HOT182(0x08C5D420u, func_08C5D420, Word);
HOT182(0x08C5D5F8u, func_08C5D5F8, Word);
HOT182(0x08C5D650u, func_08C5D650, Word);
HOT182(0x08C5D9C8u, func_08C5D9C8, Word);
HOT182(0x08C5DA20u, func_08C5DA20, Word);
HOT182(0x08C5E0C8u, func_08C5E0C8, Word);
HOT182(0x08C5E2F0u, func_08C5E2F0, Word);
HOT182(0x08C5E348u, func_08C5E348, Word);
HOT182(0x08C5E3A0u, func_08C5E3A0, Word);
HOT182(0x08C5E3F8u, func_08C5E3F8, Word);
HOT182(0x08C7089Cu, func_08C7089C, Word);
HOT182(0x08C709F0u, func_08C709F0, Word);
// HOT182(0x08C723C8u, func_08C723C8, Word);  // no comparable synthetic case yet
HOT182(0x08C73F80u, func_08C73F80, Word);
