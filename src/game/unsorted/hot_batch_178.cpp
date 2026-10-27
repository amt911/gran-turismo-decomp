// Hot batch 178: thirteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_178 {
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

void func_0881FB28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CE2ACCu); G[8] = word(rt, 0x08CE2ACCu); G[3] = word(rt, G[5]); G[2] = word(rt, G[5] + 0xCu); G[6] = word(rt, G[5] + 4u); G[7] = word(rt, G[5] + 8u); G[4] = (G[4] << 4) + G[8]; put(rt, G[4] + 0xCu, G[2]); put(rt, G[4], G[3]); put(rt, G[4] + 4u, G[6]); put(rt, G[4] + 8u, G[7]); }
void func_08835300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A105B8u); G[2] = word(rt, G[16] + 0x60u) & 0x200u; if (G[2] != 0u) put(rt, G[16] + 0x78u, 0u); } G[16] = s0; }
void func_08835978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBDCD8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x18u; } G[16] = s0; jump(ctx, 0x0890FA6Cu); }
void func_0885D9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0u; G[3] = 0u; G[7] = 0xFFFFFFFFu; G[6] = 2u; do { G[2] = word(rt, G[4] + 0x118u); G[3] += 1u; G[4] += 4u; G[2] = (G[2] ^ G[7]) != 0u ? 1u : 0u; G[5] += G[2]; } while (G[3] != G[6]); G[2] = G[5]; }
void func_0885DA7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (G[5] << 2) + G[4]; G[6] = word(rt, G[2] + 0x118u); G[3] = G[5] << 6; G[2] = 0xFFFFFFFFu; G[5] <<= 4; G[3] -= G[5]; G[4] += 0xB8u; G[5] = 0u; if (G[6] != G[2]) G[5] = G[4] + G[3]; G[2] = G[5]; }
void func_0885F3C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x0885F3C0u); G[5] = word(rt, G[16]); G[4] = word(rt, G[5] + 0x5ACu); G[5] += 0x44BCu; } G[16] = s0; jump(ctx, 0x088D8B20u); }
void func_08863D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088638B0u); G[5] = word(rt, G[16]); G[4] = word(rt, G[5] + 0x5ACu); G[5] += 0x44DCu; } G[16] = s0; jump(ctx, 0x088D8CA8u); }
void func_088646E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08863D8Cu); G[5] = word(rt, G[16]); G[4] = word(rt, G[5] + 0x5ACu); G[5] += 0x44A0u; } G[16] = s0; jump(ctx, 0x088D8F8Cu); }
void func_08864A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x0886473Cu); G[5] = word(rt, G[16]); G[4] = word(rt, G[5] + 0x5ACu); G[5] += 0x44A0u; } G[16] = s0; jump(ctx, 0x088D8F8Cu); }
void func_088659D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08864B60u); G[5] = word(rt, G[16]); G[4] = word(rt, G[5] + 0x5ACu); G[5] += 0x44DCu; } G[16] = s0; jump(ctx, 0x088D8CA8u); }
void func_0886AFC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[5] <<= 2; G[3] = load8(G[2] + 0x68u); G[8] = G[5] + G[2]; if (G[3] != 0u) return; put(rt, G[8] + 0x50u, G[6]); G[2] = word(rt, G[4] + 4u); G[3] = G[2] + 0x60u; if (G[7] == 0u) G[7] = G[3]; G[2] += G[5]; put(rt, G[2] + 0x58u, G[7]); }
void func_08884D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x40u); put(rt, G[4] + 0x15Cu, 0u); put(rt, G[4] + 0x164u, 0u); if (G[3] == 0u) return; G[2] = word(rt, G[4] + 0x44u); G[5] = 0u; G[4] = G[3]; G[6] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08AF80BCu); }
void func_08884D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = static_cast<std::int32_t>(G[5]) < 0x400 ? 1u : 0u; G[4] += 0x198u; G[2] = 0u; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = (sra(G[5], 5u) << 2) + G[4]; if (G[3] == 0u) { G[2] = 0u; return; } G[2] = sra(word(rt, G[2]), G[5] & 31u) & 1u; }
void func_08884EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x78u); G[6] = word(rt, G[4] + 0x44u); G[3] = G[4]; G[2] = (G[2] & ~0xFFFu) | (G[5] & 0xFFFu); G[5] = sra(G[2] << 20, 20u); G[4] = G[6]; put(rt, G[3] + 0x78u, G[2]); if (G[6] == 0u) return; jump(ctx, 0x08AF8FF0u); }
void func_088852DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); G[3] = G[2] + 0x50u; G[2] = word(rt, G[3] + 0x14u); if (static_cast<std::int32_t>(G[5]) < 0) { put(rt, G[3] + 0x10u, 0u); G[2] &= ~1u; } else { put(rt, G[3] + 0x10u, G[5]); G[2] |= 1u; } put(rt, G[3] + 0x14u, G[2]); }
void func_088854A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, word(rt, G[4] + 0xCu) + 0x40u); G[3] = 0u; G[6] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[2] + 0x30u); G[6] = word(rt, G[2] + 0x14u); G[3] = load8(G[2] + 0x13u); } G[2] = G[3]; G[3] = G[6]; }
void func_0888B828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x17u; put(rt, frame.at(0x10u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(8u), G[6]); put(rt, frame.at(0xCu), G[7]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_08891574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]) + 0x14u; call_here(rt, ctx, 0x08891460u); G[2] = word(rt, G[16]); G[4] = word(rt, G[2] + 0x8Cu); } G[16] = s0; jump(ctx, 0x088DA7E8u); }
void func_08894680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08898EE8u); G[2] = 0x08CC1E08u; put(rt, G[16], G[2]); put(rt, G[16] + 0x54u, 0u); } G[16] = s0; }
void func_088947C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[6]; G[2] = load8(G[6]) | load8(G[6] + 1u) << 8 | load8(G[6] + 2u) << 16 | load8(G[6] + 3u) << 24; put(rt, G[4] + 0x54u, G[2]); call_here(rt, ctx, 0x08898230u); G[2] += G[16]; } G[16] = s0; }
void func_088B6664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x088B5D24u); put(rt, G[16] + 0x198u, 0u); put(rt, G[16] + 0x18Cu, 0u); put(rt, G[16] + 0x194u, 0u); put(rt, G[16] + 0x190u, 0u); } G[16] = s0; }
void func_088D0AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x24u); if (G[2] == 0u) return; G[2] = load8(G[4] + 0x198u); G[3] = G[2] < 2u ? 1u : 0u; store8(G[4] + 0x199u, G[2]); if (G[3] != 0u) { store8(G[4] + 0x198u, 0u); return; } G[2] = 1u; store8(G[4] + 0x198u, 1u); }
void func_088D6B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = 1u; G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E49D8u); G[3] = G[2]; } G[2] = G[3]; }
void func_088D9EF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E511Cu); G[3] = G[2]; } G[2] = G[3]; }
void func_088D9F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E7004u); G[3] = G[2]; } G[2] = G[3]; }
void func_088E412C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[4] &= 0xFFu; if (G[4] != 0u) { call_here(rt, ctx, 0x08B09760u); G[2] = 1u; } else { call_here(rt, ctx, 0x08B0976Cu); } }
void func_088E49EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = G[16]; call_here(rt, ctx, 0x08B0A3A4u); G[2] = 1u; } G[16] = s0; }
void func_088E52CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x088E511Cu); if (G[2] != 0u) G[2] = 1u; store8(G[16] + 0x19u, G[2]); } G[16] = s0; }
void func_088F0F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load16(G[4]); G[3] = 0u; if (G[2] == 0u) { G[2] = load8(G[4] + 2u); G[3] = 1u; if (G[2] == 0u) { G[2] = load8(G[4] + 3u) & 8u; if (G[2] != 0u) { G[2] = G[3]; return; } } G[3] = 0u; } G[2] = G[3]; }
void func_08900E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x44u); G[3] = G[6] << 3; G[5] <<= 2; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; ctx.fpr[0] = 0.0f; if (G[2] == 0u) return; G[2] = word(rt, G[4] + 0x48u) + G[3]; G[3] = word(rt, G[2] + 4u) + G[5]; ctx.fpr[0] = float_at(rt, G[3]); }
void func_089099B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[1] = 0x7F7FFFFFu; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (G[2] == G[5]) return; put(rt, G[4] + 4u, G[1]); put(rt, G[4], G[5]); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0xCu, G[1]); put(rt, G[4] + 8u, G[1]); }
void func_0890E370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); G[2] = G[4]; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[12]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 4u); ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[0]); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[1]); put(rt, G[4] + 8u, bits_of(ctx.fpr[12])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); }
void func_089167B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = G[4] + 0x60u; G[16] = G[4]; ctx.fpr[12] = float_at(rt, G[4] + 0x18u); call_here(rt, ctx, 0x089164E8u); ctx.fpr[0] = float_at(rt, G[16] + 0x18u); put(rt, G[16] + 0xA0u, bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_08917E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[5] = G[4]; G[4] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x089271E8u); G[2] = word(rt, G[16] + 0xCu); store8(G[2] + 0x1FCFu, 0u); } G[16] = s0; }
void func_08917F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = (G[6] << 2) + G[5]; G[16] = G[4]; G[5] = word(rt, G[6] + 8u); call_here(rt, ctx, 0x08928194u); G[2] = G[16]; } G[16] = s0; }
void func_08917FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = (G[6] << 2) + G[5]; G[16] = G[4]; G[5] = word(rt, G[6] + 8u); call_here(rt, ctx, 0x089281B8u); G[2] = G[16]; } G[16] = s0; }
void func_0892125C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0xCu; G[3] = G[4] + 0x1Cu; for (std::uint32_t offset = 0xCu; offset < 0x2Cu; offset += 4u) put(rt, G[4] + offset, 0u); G[2] = 1u; store8(G[4] + 0x2Cu, 1u); }
void func_08925B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08925B68u); put(rt, G[16] + 4u, G[17]); } G[16] = s0; G[17] = s1; }
void func_08927758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08927120u); G[3] = word(rt, G[2]); G[5] = G[16]; G[25] = word(rt, G[3] + 0x20u); G[4] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08931C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67A8u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08931B20u); G[4] = word(rt, G[16] + 4u); } G[16] = s0; jump(ctx, 0x08937B04u); }
void func_08939BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = 1u; G[5] = word(rt, G[3] + 8u); G[4] += 0xA0u; if (G[5] != G[2]) return; G[2] = word(rt, G[3] + 0xCu) & 0xFu; G[6] = 0xAu; if (G[2] != G[5]) return; G[5] = word(rt, G[3] + 0x18u); jump(ctx, 0x0893A31Cu); }
void func_0893A068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC6808u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08939FECu); G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_0893A0D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 8u)) ? 1u : 0u; G[3] = 0u; if (G[2] == 0u || static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0u; return; } G[3] = word(rt, G[4] + 0x10u); G[2] = word(rt, G[4] + 0xCu); ctx.lo = G[3]; const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[3] = ctx.lo; G[2] = G[3]; }
void func_08940120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); ctx.fpr[1] = 60.0f; G[1] = 0x40C90FDBu; ctx.fpr[2] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(float_at(rt, G[3] + 0x590u), ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[2]; G[2] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[2]); store16(G[3] + 0x58Eu, G[2]); }
void func_08941750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = psp_mul(float_at(rt, G[4]), ctx.fpr[13]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[6]); ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[0]); put(rt, G[6], bits_of(ctx.fpr[13])); ctx.fpr[0] = psp_mul(float_at(rt, G[5]), ctx.fpr[12]); put(rt, G[5], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[7]); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[1]); put(rt, G[7], bits_of(ctx.fpr[12])); }
void func_08944388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] &= 0xFFu; G[3] = G[6] & 0xFFu; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; G[6] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[6] != 0u) G[4] = G[3]; if (G[2] != 0u) G[4] = G[5] & 0xFFu; G[3] = G[4] - G[5]; divide(ctx, G[3], G[7]); G[2] = (G[4] - ctx.hi) & 0xFFu; }
void func_08958940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08958904u); ctx.fpr[0] = 0.0f; if (G[2] != 0u) { G[2] = word(rt, G[16] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 0x77Cu); } } G[16] = s0; }
void func_08961374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); for (;;) { if (G[3] == G[4]) return; G[2] = word(rt, G[3] + 8u); if (G[5] == G[2]) { G[5] = G[3]; jump(ctx, 0x08BDFFD0u); return; } G[3] = word(rt, G[3]); } }
void func_0897E39C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x20Cu); G[1] = 0x3F666666u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[2] = float_at(rt, G[4] + 0x208u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[0]); G[2] = ctx.fpr[1] < ctx.fpr[2] ? 1u : 0u; }
void func_0897E3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x089BD4DCu); G[4] = G[2]; G[5] = 0u; } if (G[2] == 0u) return; jump(ctx, 0x089A2BF8u); }
void func_0898D8A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x4C0u); G[3] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[4] + 0x4BCu); G[5] = G[2] - G[5]; if (static_cast<std::int32_t>(G[2]) > 0) { put(rt, G[4] + 0x4BCu, G[5]); G[3] = 1u; if (static_cast<std::int32_t>(G[5]) <= 0) { store8(G[4] + 0x4C0u, 0u); put(rt, G[4] + 0x4BCu, 0u); } } } G[2] = G[3]; }
void func_089A4250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x170u; G[4] += 0x16Cu; call_here(rt, ctx, 0x08BF63A8u); G[2] = (G[2] ^ G[16]) != 0u ? 1u : 0u; } G[16] = s0; }
void func_089BAC4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089C9C34u); G[4] = frame.at(0u); G[16] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_089BB23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089C9C34u); G[4] = frame.at(0u); G[16] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_089C19E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0x90u; if (G[2] != G[5]) { ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 0xCu); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[3] = float_at(rt, G[5] + 8u); put(rt, G[4] + 0x90u, bits_of(ctx.fpr[0])); put(rt, G[2] + 0xCu, bits_of(ctx.fpr[1])); put(rt, G[2] + 4u, bits_of(ctx.fpr[2])); put(rt, G[2] + 8u, bits_of(ctx.fpr[3])); } put(rt, G[4] + 0xA0u, bits_of(ctx.fpr[12])); }
void func_089E4DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 0x154u)) ? 1u : 0u; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x158u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; ctx.lo = G[6]; if (G[2] != 0u) { const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[4]); ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[2] = ctx.lo; return; } } G[2] = 0xFFFFFFFFu; }
void func_089F0A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A5B828u); G[2] = 0x08CCFA50u; put(rt, G[16], G[2]); G[4] = G[16] + 0xCu; } G[16] = s0; jump(ctx, 0x08B0B06Cu); }
void func_08A21E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBE7F8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08AB8644u); }
void func_08A2A188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CBBAB0u; put(rt, G[16], G[2]); put(rt, G[16] + 0x14u, 0u); } G[16] = s0; }
void func_08A36DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1C30u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08B487ACu); }
void func_08A398F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CCC730u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08C1B4D8u); }
void func_08A3CB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1A10u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; } G[16] = s0; jump(ctx, 0x08B28D34u); }
void func_08A40F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x30u; G[4] += 0x2Cu; call_here(rt, ctx, 0x08C1B84Cu); G[16] ^= G[2]; G[2] = G[16] != 0u ? 1u : 0u; } G[16] = s0; }
void func_08A4CF24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x2Cu); ctx.fpr[2] = float_at(rt, G[4] + 0x4Cu); ctx.fpr[1] = float_at(rt, G[4] + 0x54u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[12]; ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; put(rt, G[4] + 0x2Cu, bits_of(ctx.fpr[0])); G[2] = ctx.fpr[0] < ctx.fpr[1] ? 1u : 0u; }
void func_08A50A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0x14u; G[3] = G[4] + 0x18u; for (std::uint32_t offset = 0u; offset < 0x14u; offset += 4u) put(rt, G[4] + offset, 0u); put(rt, G[2] + 0x10u, G[3]); put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, G[3]); }
void func_08A6AF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08C8EDC4u; G[6] = frame.at(0u); G[16] = G[4]; call_here(rt, ctx, 0x08B85EB8u); G[2] = G[16]; } G[16] = s0; }
void func_08AA45F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CB2D80u; G[6] = frame.at(0u); G[16] = G[4]; call_here(rt, ctx, 0x08B85EB8u); G[2] = G[16]; } G[16] = s0; }
void func_08A8DB94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[8] = G[7]; G[16] = G[4]; G[7] = G[6]; G[6] = G[5] + 0x20u; call_here(rt, ctx, 0x08A8D308u); G[2] = G[16]; } G[16] = s0; }
void func_08A9E1B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4] + 0x24u) + 7u) & ~7u; G[3] = G[2] + 8u; put(rt, G[4] + 0x24u, G[3]); G[4] = word(rt, G[2]); G[5] = word(rt, G[2] + 4u); call_with(rt, ctx, 0x08B71EC8u, 0x10u); }
void func_08AA95E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] <<= 2; G[16] = G[4]; G[5] = word(rt, G[5] + 0x38u) + G[6]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[16]; } G[16] = s0; }
void func_08AB2B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A717ECu); G[2] = 0x08CD5010u; put(rt, G[16], G[2]); put(rt, G[16] + 0x24u, 0u); } G[16] = s0; }
void func_08AB54CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A70B1Cu); G[2] = 0x08CD5240u; put(rt, G[16], G[2]); put(rt, G[16] + 0x1Cu, 0u); } G[16] = s0; }
void func_08AB5EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A717ECu); G[2] = 0x08CD5128u; put(rt, G[16], G[2]); put(rt, G[16] + 0x24u, 0u); } G[16] = s0; }
void func_08AC0160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6450u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08AC00D8u); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08B28BE0u); }
void func_08AC263C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x20u) & ~3u; G[3] = 0x08CD65F0u; put(rt, G[4] + 0x20u, G[2]); put(rt, G[4], G[3]); for (std::uint32_t offset = 8u; offset < 0x20u; offset += 4u) put(rt, G[4] + offset, 0u); }
void func_08AC2F5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[6] = 1u; G[5] = G[2]; if (G[2] == 0u) return; G[3] = word(rt, G[4]); G[2] = 1u; store8(G[4] + 8u, 1u); G[2] = word(rt, G[3]); G[25] = word(rt, G[2] + 0x30u); G[4] = G[3]; jump(ctx, G[25]); }
void func_08AC69C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08CFFD90u; G[3] = 0u; G[6] = 6u; for (;;) { G[2] = word(rt, G[5]); G[3] += 1u; if (G[4] == G[2]) { G[4] = word(rt, G[5] + 8u); jump(ctx, 0x08B28C44u); return; } G[5] += 0x1Cu; if (G[3] == G[6]) break; } jump(ctx, 0x08B1B99Cu); }
void func_08AD393C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[2] = 0u; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = G[5] < word(rt, G[4] + 0x10u) ? 1u : 0u; G[3] = G[5] << 4; G[4] = (G[5] << 6) - G[3]; if (G[2] == 0u) return; G[2] = word(rt, G[6] + 0x14u) + G[4]; }
void func_08AE5BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x34u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[2] = word(rt, G[16] + 0x34u); G[4] = G[16]; call_here(rt, ctx, G[2]); } G[16] = s0; }
void func_08AE87C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x20u; G[5] = 0u; G[2] = sext(load16(G[4]), 16u); for (;;) { G[5] += 1u; G[3] = static_cast<std::int32_t>(G[5]) < 4 ? 1u : 0u; G[4] += 0xCu; G[6] = 1u; if (G[2] != 0u) break; if (G[3] == 0u) { G[6] = 0u; break; } G[2] = sext(load16(G[4]), 16u); } G[2] = G[6]; }
void func_08AEDC44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[5] = 0x08C92370u; call_here(rt, ctx, 0x08AEDA20u); if (G[2] != 0u) store8(G[2] + 0x3Bu, G[16]); } G[16] = s0; }
void func_08AEDCF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] = 0u; call_here(rt, ctx, 0x08AEDA20u); G[4] = G[2]; G[2] = 0u; G[3] = 0u; if (G[4] == 0u) return; G[2] = word(rt, G[4] + 0x60A8u); G[3] = word(rt, G[4] + 0x60ACu); }
void func_08AEEE1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; G[8] = G[7]; G[6] = G[5]; G[7] = G[2]; G[5] = 0u; G[9] = 0u; G[10] = 0u; call_with(rt, ctx, 0x08AEED10u, 0x10u); }
void func_08AF5668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[5] = load16(G[4] + 0x9Au); G[4] = word(rt, G[4] + 0xA0u); call_here(rt, ctx, 0x08C3FE50u); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) G[3] = word(rt, G[2] + 4u); G[2] = G[3]; }
void func_08AF7FF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5] + 0x30u; put(rt, G[5] + 0xCu, G[6]); G[2] = G[5]; G[3] = load16(G[4] + 0x20u) << 2; G[6] += G[3]; put(rt, G[5] + 0x14u, G[6]); G[3] = load16(G[4] + 0x1Eu); put(rt, G[5], G[4]); G[3] <<= 2; G[6] += G[3]; put(rt, G[5] + 0x10u, G[6]); }
void func_08AFB4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AFB360u); G[2] = 0x08CD77F0u; put(rt, G[16], G[2]); G[4] = G[16] + 0x2Cu; } G[16] = s0; jump(ctx, 0x08B52B48u); }
void func_08B09320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0xD4u; for (std::uint32_t offset = 0u; offset < 0x2Cu; offset += 4u) put(rt, G[2] + offset, 0u); }
void func_08B09360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x08D69170u; call_here(rt, ctx, 0x08B31AB4u); G[2] = G[2] != 0u ? 1u : 0u; store8(G[16] + 0x11Du, G[2]); } G[16] = s0; }
void func_08B0B120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0xCu); store8(G[16] + 8u, 0u); call_here(rt, ctx, 0x08B35254u); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0xCu, 0u); } G[16] = s0; }
void func_08B0CE24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[5] = 0u; put(rt, G[4], 0u); G[4] = 0u; put(rt, G[2] + 0x10u, 0u); put(rt, G[2] + 0x18u, 0u); G[4] = G[2] + 0x24u; put(rt, G[2] + 0x14u, 0u); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0x20u, 0u); jump(ctx, 0x08B17E84u); }
void func_08B10048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[7] &= 0xFFu; G[8] = G[7]; G[7] = frame.at(0u); for (std::uint32_t offset = 0u; offset < 0x10u; offset += 4u) put(rt, frame.at(offset), 0u); call_here(rt, ctx, 0x08B0F840u); }
void func_08B1029C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x38u); G[16] = G[4]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B0DF0Cu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B0DEB0u); }
void func_08B14EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; call_here(rt, ctx, 0x08C7A868u); G[4] = G[2]; } if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = 0u; return; } jump(ctx, 0x08B1830Cu); }
void func_08B14F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; call_here(rt, ctx, 0x08C7A878u); G[4] = G[2]; } if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = 0u; return; } jump(ctx, 0x08B1830Cu); }
void func_08B15988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0C304u); G[2] = 0x08B15FC0u; put(rt, G[16] + 0x18u, G[2]); put(rt, G[16] + 0x1Cu, 0u); } G[16] = s0; }
void func_08B15C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0E008u); G[2] = 0x08CD9088u; put(rt, G[16], G[2]); G[4] = G[16] + 0xF8u; } G[16] = s0; jump(ctx, 0x08B15C30u); }
void func_08B17C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B11830u); G[2] = 0x08CD91C8u; put(rt, G[16], G[2]); G[4] = G[16] + 0xA0u; } G[16] = s0; jump(ctx, 0x08C4C82Cu); }
void func_08B18B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[4] == G[5]) return; G[6] = G[4]; do { G[4] = sext(load8(G[6]), 8u); G[3] = G[2] << 3; G[2] = (G[2] << 1) + G[3] + G[4]; G[6] += 1u; G[2] -= 0x30u; } while (G[6] != G[5]); }
void func_08B1935C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B19354u); if (G[2] != 0u) return; } G[2] = 0u; }
void func_08B1D7BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = 0x08D68A94u; call_with(rt, ctx, 0x08B1DEB4u, 0x10u); G[3] = 0u; if (G[2] != 0u) G[3] = word(rt, G[2]); G[2] = G[3] != 0u ? 1u : 0u; }
void func_08B1FDD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 4u); G[5] = G[16]; if (G[16] != 0u) call_here(rt, ctx, 0x08B1FC48u); G[2] = G[16]; } G[16] = s0; }
void func_08B1FE04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 8u); G[5] = G[16]; if (G[16] != 0u) call_here(rt, ctx, 0x08B1FC48u); G[2] = G[16]; } G[16] = s0; }
void func_08B272A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; G[6] = 0xFFFFFFFFu; G[5] = 0x200u; do { G[2] = word(rt, G[4] + 0x14u) + G[3]; G[3] += 2u; store16(G[2], G[6]); } while (G[3] != G[5]); G[2] = 0xFFFF8000u; G[3] = 0x7FFFu; store16(G[4] + 0x1Eu, G[2]); store16(G[4] + 0x1Cu, G[3]); }
void func_08B28E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[2] = ctx.fpr[12] / ctx.fpr[13]; ctx.fpr[0] = 0.0f; G[4] &= 0xFFu; const bool negative = ctx.fpr[12] < ctx.fpr[0]; ctx.fpr[1] = std::bit_cast<float>(trunc_w_s(ctx.fpr[2])); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[1]))); ctx.fpr[0] = psp_mul(ctx.fpr[13], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[12] - ctx.fpr[0]; if (G[4] == 0u) return; if (negative) ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[13]; }
void func_08B2BB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x40u); G[8] = 1u; G[3] = G[4]; G[7] = 0x08B2B8ECu; G[6] = 8u; if (G[2] == G[8]) return; G[5] = word(rt, G[4] + 0x48u); G[4] = word(rt, G[4] + 0x44u); put(rt, G[3] + 0x40u, G[8]); jump(ctx, 0x08B7A3BCu); }
void func_08B318E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[4] = 0x08CB549Cu; G[5] = 1u; G[7] = 0u; call_with(rt, ctx, 0x08C7A930u, 0x10u); G[3] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) G[2] = 0u; }
void func_08B32EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B219B0u); G[2] = 0x08CDA840u; put(rt, G[16], G[2]); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08B34DA8u); }
void func_08B331B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B34DA8u); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 8u, 0u); store8(G[16] + 0xCu, 0u); put(rt, G[16] + 0x10u, 0u); } G[16] = s0; }
void func_08B3E744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5] + 0x14u); G[3] = word(rt, G[5]); G[6] = word(rt, G[5] + 4u); G[7] = word(rt, G[5] + 0xCu); G[8] = word(rt, G[5] + 0x10u); put(rt, G[4] + 0x14u, G[2]); put(rt, G[4], G[3]); put(rt, G[4] + 4u, G[6]); put(rt, G[4] + 0xCu, G[7]); put(rt, G[4] + 0x10u, G[8]); store8(G[4] + 8u, 0u); store8(G[4] + 9u, 0u); }
void func_08B40A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(8u), G[6]); G[7] = frame.at(0u); G[6] = 4u; G[8] = 0xCu; G[9] = 1u; put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08B40950u); }
void func_08B40B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = sext(G[5], 16u); G[2] = static_cast<std::int32_t>(G[5]) < 0x20 ? 1u : 0u; G[6] = sext(G[6], 16u); G[3] = 0u; if (G[2] != 0u) { G[2] = (G[5] << 1) + G[4]; G[3] = 1u; if (static_cast<std::int32_t>(G[6]) >= 0) { store16(G[2] + 0x948u, G[6]); G[2] = G[3]; return; } G[3] = 0u; } G[2] = G[3]; }
void func_08B46628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0xFFFFFFFFu; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA4u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[4] = G[16] + 0x2Cu; } G[16] = s0; jump(ctx, 0x08B1FDA4u); }
void func_08B48E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0x08CB0000u; if (G[4] == 0u) { G[3] = 0x08CB5708u; G[2] = G[3]; return; } G[3] = 0x08CB56FCu; G[2] = 1u; if (G[4] == G[2]) { G[2] = G[3]; return; } G[2] = 0x08CB0000u; G[3] = 0x08CB5704u; G[2] = G[3]; }
void func_08B48E44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0x08CB0000u; if (G[4] == 0u) { G[3] = 0x08CB5710u; G[2] = G[3]; return; } G[3] = 0x08CB570Cu; G[2] = 1u; if (G[4] == G[2]) { G[2] = G[3]; return; } G[2] = 0x08CB0000u; G[3] = 0x08CB5704u; G[2] = G[3]; }
void func_08B48E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0x08CB0000u; if (G[4] == 0u) { G[3] = 0x08CB5710u; G[2] = G[3]; return; } G[3] = 0x08CB56FCu; G[2] = 1u; if (G[4] == G[2]) { G[2] = G[3]; return; } G[2] = 0x08CB0000u; G[3] = 0x08CB5704u; G[2] = G[3]; }
void func_08B55EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x50u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B55E30u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B566E0u); } G[16] = s0; }
void func_08B63614(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; G[7] = 0u; do { G[3] = load8(G[4]); G[4] += 1u; G[2] = (G[3] & 0x7Fu) << (G[6] & 31u); G[3] = sext(G[3], 8u); G[7] |= G[2]; G[6] += 7u; } while (static_cast<std::int32_t>(G[3]) < 0); G[2] = G[4]; put(rt, G[5], G[7]); }
void func_08B727A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; G[7] = 0u; do { G[3] = load8(G[4]); G[4] += 1u; G[2] = (G[3] & 0x7Fu) << (G[6] & 31u); G[3] = sext(G[3], 8u); G[7] |= G[2]; G[6] += 7u; } while (static_cast<std::int32_t>(G[3]) < 0); G[2] = G[4]; put(rt, G[5], G[7]); }
void func_08B75D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; G[7] = 0u; do { G[3] = load8(G[4]); G[4] += 1u; G[2] = (G[3] & 0x7Fu) << (G[6] & 31u); G[3] = sext(G[3], 8u); G[7] |= G[2]; G[6] += 7u; } while (static_cast<std::int32_t>(G[3]) < 0); G[2] = G[4]; put(rt, G[5], G[7]); }
void func_08B76A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); if (G[2] != 0u) { call_here(rt, ctx, 0x08B76A84u); G[4] = G[2]; tail = true; } } if (tail) jump(ctx, 0x08B35254u); }
void func_08B815FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[5] = 1u; call_here(rt, ctx, 0x08B82260u); G[3] = 1u; put(rt, G[2] + 0x10u, 1u); put(rt, G[2] + 0x14u, G[16]); } G[16] = s0; }
void func_08B8E770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) { G[4] += 8u; } else { G[2] = word(rt, G[4]); G[3] = word(rt, G[4] + 4u); put(rt, G[7], G[2]); put(rt, G[7] + 4u, G[3]); G[4] += 8u; } G[7] += 8u; } while (G[5] != G[4]); } G[2] = G[7]; }
void func_08BAA4AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[16] = G[4]; call_here(rt, ctx, 0x088CE544u); ctx.fpr[0] = float_at(rt, G[16] + 0x10Cu); put(rt, frame.at(0xCu), bits_of(ctx.fpr[0])); G[5] = word(rt, frame.at(0xCu)); } G[16] = s0; G[3] = G[5]; }
void func_08BAD978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5]); put(rt, G[2], bits_of(ctx.fpr[0])); G[3] = word(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[3] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, G[5] + 8u); G[2] = word(rt, G[4]); put(rt, G[2] + 8u, bits_of(ctx.fpr[0])); G[3] = word(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[3] + 0xCu, bits_of(ctx.fpr[0])); }
void func_08BAE330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[0] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[3] = float_at(rt, G[4] + 8u); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 4u); ctx.fpr[2] = ctx.fpr[2] - ctx.fpr[1]; put(rt, G[4] + 4u, bits_of(ctx.fpr[2])); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[3] = ctx.fpr[3] - ctx.fpr[0]; put(rt, G[4] + 8u, bits_of(ctx.fpr[3])); }
void func_08BD2A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0x10u; offset += 4u) { G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + offset); put(rt, G[4] + offset, bits_of(ctx.fpr[0])); } }
void func_08BD2B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[6]); G[6] = 0u; put(rt, frame.at(0x1Cu), G[7]); put(rt, frame.at(0x20u), G[8]); put(rt, frame.at(0x24u), G[9]); put(rt, frame.at(0x28u), G[10]); put(rt, frame.at(0x2Cu), G[11]); call_here(rt, ctx, 0x08BBD0E4u); }
void func_08BDD978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) { G[4] += 2u; } else { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); G[4] += 2u; } G[7] += 2u; } while (G[5] != G[4]); } G[2] = G[7]; }
void func_08BDD9DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) { G[4] += 2u; } else { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); G[4] += 2u; } G[7] += 2u; } while (G[5] != G[4]); } G[2] = G[7]; }
void func_08BF0C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { if (G[2] == 0u) { G[4] += 8u; } else { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[1])); G[4] += 8u; } G[2] += 8u; } while (G[5] != G[4]); } }
void func_08C01F20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { G[2] = G[5] - G[4]; G[5] = G[6] + G[2]; do { if (G[3] != 0u) { G[2] = load8(G[4]); store8(G[3], G[2]); } G[3] += 1u; G[4] += 1u; } while (G[3] != G[5]); } G[2] = G[3]; }
void func_08C1B4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08C1B474u); G[2] = 0x08CD1E60u; put(rt, G[16], G[2]); G[4] = G[16] + 0x110u; } G[16] = s0; jump(ctx, 0x08B28DCCu); }
void func_08C24838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = sext(G[6], 8u); G[6] = G[7]; call_here(rt, ctx, 0x08C19348u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C3181C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08C31414u); G[6] = G[2]; G[4] = G[16]; G[7] = G[3]; } G[16] = s0; jump(ctx, 0x08C317F4u); }
void func_08C319A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08C31504u); G[6] = G[2]; G[4] = G[16]; G[7] = G[3]; } G[16] = s0; jump(ctx, 0x08C3197Cu); }
void func_08C3FBBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (std::uint32_t offset = 0u; offset < 0x10u; offset += 4u) { G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + offset); put(rt, G[4] + offset, bits_of(ctx.fpr[0])); } }
void func_08C40904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; if (G[4] == G[5]) return; G[2] = word(rt, G[6]); for (;;) { G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); put(rt, G[7], G[2]); put(rt, G[7] + 4u, G[3]); put(rt, G[7] + 8u, G[4]); G[7] += 0xCu; if (G[5] == G[7]) break; G[2] = word(rt, G[6]); } }
void func_08C4AD60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) { G[4] += 4u; } else { G[2] = load16(G[4]); G[3] = load16(G[4] + 2u); store16(G[7], G[2]); store16(G[7] + 2u, G[3]); G[4] += 4u; } G[7] += 4u; } while (G[5] != G[4]); } G[2] = G[7]; }
void func_08C55560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu) + G[16]; call_here(rt, ctx, G[5]); G[2] = G[16]; } G[16] = s0; }
void func_08C55594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu) + G[16]; call_here(rt, ctx, G[5]); G[2] = G[16]; } G[16] = s0; }
void func_08C6FE74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu) + G[16]; call_here(rt, ctx, G[5]); G[2] = G[16]; } G[16] = s0; }
void func_08C6FEA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu) + G[16]; call_here(rt, ctx, G[5]); G[2] = G[16]; } G[16] = s0; }
void func_08C747B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = sext(G[6], 8u); G[6] = G[7]; call_here(rt, ctx, 0x08C746F0u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C747E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = sext(G[6], 8u); G[6] = G[7]; call_here(rt, ctx, 0x08C746F0u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C7481C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[16] = G[4]; G[5] = 0u; G[4] = 0u; call_here(rt, ctx, 0x08C746F0u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C74850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[16] = G[4]; G[5] = 0u; G[4] = 0u; call_here(rt, ctx, 0x08C746F0u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C74968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C7488Cu); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C74ACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C7488Cu); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C7499C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6] + G[5]; G[6] = G[7]; call_here(rt, ctx, 0x08C7488Cu); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C749D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6] + G[5]; G[6] = G[7]; call_here(rt, ctx, 0x08C7488Cu); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C766B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C765E8u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C7671C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C765E8u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C7682C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C76750u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C76A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; call_here(rt, ctx, 0x08C76750u); put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C766E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4]); G[2] = word(rt, G[6] - 4u); if (static_cast<std::int32_t>(G[2]) < 0) put(rt, G[6] - 4u, 0u); G[3] = word(rt, G[5]); G[2] = word(rt, G[3] - 4u); put(rt, G[4], G[3]); if (static_cast<std::int32_t>(G[2]) < 0) put(rt, G[3] - 4u, 0u); put(rt, G[5], G[6]); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_178
} // namespace gt::game

#define HOT178(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_178::name, gt::ReturnKind::kind)
// HOT178(0x0881FB28u, func_0881FB28, Word);  // no comparable synthetic case yet
HOT178(0x08835300u, func_08835300, Word);
HOT178(0x08835978u, func_08835978, Word);
HOT178(0x0885D9E4u, func_0885D9E4, Word);
HOT178(0x0885DA7Cu, func_0885DA7C, Word);
// HOT178(0x0885F3C8u, func_0885F3C8, Word);  // no comparable synthetic case yet
// HOT178(0x08863D58u, func_08863D58, Word);  // no comparable synthetic case yet
// HOT178(0x088646E4u, func_088646E4, Word);  // no comparable synthetic case yet
// HOT178(0x08864A40u, func_08864A40, Word);  // no comparable synthetic case yet
// HOT178(0x088659D0u, func_088659D0, Word);  // no comparable synthetic case yet
HOT178(0x0886AFC8u, func_0886AFC8, Word);
HOT178(0x08884D38u, func_08884D38, Word);
HOT178(0x08884D6Cu, func_08884D6C, Word);
HOT178(0x08884EB4u, func_08884EB4, Word);
HOT178(0x088852DCu, func_088852DC, Word);
HOT178(0x088854A8u, func_088854A8, DoubleWord);
HOT178(0x0888B828u, func_0888B828, Word);
HOT178(0x08891574u, func_08891574, Word);
HOT178(0x08894680u, func_08894680, Word);
HOT178(0x088947C4u, func_088947C4, Word);
HOT178(0x088B6664u, func_088B6664, Word);
HOT178(0x088D0AD8u, func_088D0AD8, Word);
HOT178(0x088D6B38u, func_088D6B38, Word);
HOT178(0x088D9EF8u, func_088D9EF8, Word);
HOT178(0x088D9F2Cu, func_088D9F2C, Word);
HOT178(0x088E412Cu, func_088E412C, Word);
HOT178(0x088E49ECu, func_088E49EC, Word);
HOT178(0x088E52CCu, func_088E52CC, Word);
HOT178(0x088F0F68u, func_088F0F68, Word);
HOT178(0x08900E14u, func_08900E14, Float);
HOT178(0x089099B8u, func_089099B8, Word);
HOT178(0x0890E370u, func_0890E370, Word);
// HOT178(0x089167B4u, func_089167B4, Word);  // no comparable synthetic case yet
HOT178(0x08917E90u, func_08917E90, Word);
// HOT178(0x08917F78u, func_08917F78, Word);  // no comparable synthetic case yet
HOT178(0x08917FACu, func_08917FAC, Word);
HOT178(0x0892125Cu, func_0892125C, Word);
HOT178(0x08925B70u, func_08925B70, Word);
HOT178(0x08927758u, func_08927758, Word);
HOT178(0x08931C38u, func_08931C38, Word);
HOT178(0x08939BC0u, func_08939BC0, Word);
HOT178(0x0893A068u, func_0893A068, Word);
HOT178(0x0893A0D0u, func_0893A0D0, Word);
HOT178(0x08940120u, func_08940120, Word);
HOT178(0x08941750u, func_08941750, Word);
HOT178(0x08944388u, func_08944388, Word);
HOT178(0x08958940u, func_08958940, Float);
// HOT178(0x08961374u, func_08961374, Word);  // no comparable synthetic case yet
HOT178(0x0897E39Cu, func_0897E39C, Word);
HOT178(0x0897E3D0u, func_0897E3D0, Word);
HOT178(0x0898D8A0u, func_0898D8A0, Word);
HOT178(0x089A4250u, func_089A4250, Word);
HOT178(0x089BAC4Cu, func_089BAC4C, Word);
HOT178(0x089BB23Cu, func_089BB23C, Word);
HOT178(0x089C19E8u, func_089C19E8, Word);
HOT178(0x089E4DE0u, func_089E4DE0, Word);
HOT178(0x089F0A28u, func_089F0A28, Word);
HOT178(0x08A21E6Cu, func_08A21E6C, Word);
HOT178(0x08A2A188u, func_08A2A188, Word);
HOT178(0x08A36DA4u, func_08A36DA4, Word);
HOT178(0x08A398F4u, func_08A398F4, Word);
HOT178(0x08A3CB7Cu, func_08A3CB7C, Word);
HOT178(0x08A40F7Cu, func_08A40F7C, Word);
HOT178(0x08A4CF24u, func_08A4CF24, Word);
HOT178(0x08A50A7Cu, func_08A50A7C, Word);
HOT178(0x08A6AF88u, func_08A6AF88, Word);
HOT178(0x08AA45F8u, func_08AA45F8, Word);
// HOT178(0x08A8DB94u, func_08A8DB94, Word);  // no comparable synthetic case yet
HOT178(0x08A9E1B0u, func_08A9E1B0, Word);
HOT178(0x08AA95E8u, func_08AA95E8, Word);
HOT178(0x08AB2B1Cu, func_08AB2B1C, Word);
HOT178(0x08AB54CCu, func_08AB54CC, Word);
HOT178(0x08AB5EC8u, func_08AB5EC8, Word);
// HOT178(0x08AC0160u, func_08AC0160, Word);  // no comparable synthetic case yet
HOT178(0x08AC263Cu, func_08AC263C, Word);
HOT178(0x08AC2F5Cu, func_08AC2F5C, Word);
// HOT178(0x08AC69C0u, func_08AC69C0, Word);  // no comparable synthetic case yet
HOT178(0x08AD393Cu, func_08AD393C, Word);
HOT178(0x08AE5BA8u, func_08AE5BA8, Word);
HOT178(0x08AE87C4u, func_08AE87C4, Word);
HOT178(0x08AEDC44u, func_08AEDC44, Word);
HOT178(0x08AEDCF4u, func_08AEDCF4, DoubleWord);
// HOT178(0x08AEEE1Cu, func_08AEEE1C, Word);  // no comparable synthetic case yet
HOT178(0x08AF5668u, func_08AF5668, Word);
HOT178(0x08AF7FF8u, func_08AF7FF8, Word);
HOT178(0x08AFB4E8u, func_08AFB4E8, Word);
HOT178(0x08B09320u, func_08B09320, Word);
HOT178(0x08B09360u, func_08B09360, Word);
HOT178(0x08B0B120u, func_08B0B120, Word);
HOT178(0x08B0CE24u, func_08B0CE24, Word);
HOT178(0x08B10048u, func_08B10048, Word);
HOT178(0x08B1029Cu, func_08B1029C, Word);
// HOT178(0x08B14EC0u, func_08B14EC0, Word);  // no comparable synthetic case yet
// HOT178(0x08B14F64u, func_08B14F64, Word);  // no comparable synthetic case yet
HOT178(0x08B15988u, func_08B15988, Word);
HOT178(0x08B15C84u, func_08B15C84, Word);
HOT178(0x08B17C44u, func_08B17C44, Word);
HOT178(0x08B18B98u, func_08B18B98, Word);
HOT178(0x08B1935Cu, func_08B1935C, Word);
HOT178(0x08B1D7BCu, func_08B1D7BC, Word);
HOT178(0x08B1FDD0u, func_08B1FDD0, Word);
HOT178(0x08B1FE04u, func_08B1FE04, Word);
HOT178(0x08B272A0u, func_08B272A0, Word);
HOT178(0x08B28E04u, func_08B28E04, Float);
HOT178(0x08B2BB10u, func_08B2BB10, Word);
HOT178(0x08B318E4u, func_08B318E4, Word);
HOT178(0x08B32EC4u, func_08B32EC4, Word);
HOT178(0x08B331B0u, func_08B331B0, Word);
HOT178(0x08B3E744u, func_08B3E744, Word);
HOT178(0x08B40A40u, func_08B40A40, Word);
HOT178(0x08B40B48u, func_08B40B48, Word);
HOT178(0x08B46628u, func_08B46628, Word);
HOT178(0x08B48E10u, func_08B48E10, Word);
HOT178(0x08B48E44u, func_08B48E44, Word);
HOT178(0x08B48E78u, func_08B48E78, Word);
HOT178(0x08B55EC0u, func_08B55EC0, Word);
HOT178(0x08B63614u, func_08B63614, Word);
HOT178(0x08B727A4u, func_08B727A4, Word);
HOT178(0x08B75D14u, func_08B75D14, Word);
// HOT178(0x08B76A8Cu, func_08B76A8C, Word);  // no comparable synthetic case yet
HOT178(0x08B815FCu, func_08B815FC, Word);
HOT178(0x08B8E770u, func_08B8E770, Word);
HOT178(0x08BAA4ACu, func_08BAA4AC, DoubleWord);
HOT178(0x08BAD978u, func_08BAD978, Word);
HOT178(0x08BAE330u, func_08BAE330, Word);
HOT178(0x08BD2A80u, func_08BD2A80, Word);
HOT178(0x08BD2B14u, func_08BD2B14, Word);
HOT178(0x08BDD978u, func_08BDD978, Word);
HOT178(0x08BDD9DCu, func_08BDD9DC, Word);
HOT178(0x08BF0C9Cu, func_08BF0C9C, Word);
HOT178(0x08C01F20u, func_08C01F20, Word);
HOT178(0x08C1B4D8u, func_08C1B4D8, Word);
HOT178(0x08C24838u, func_08C24838, Word);
// HOT178(0x08C3181Cu, func_08C3181C, Word);  // no comparable synthetic case yet
// HOT178(0x08C319A4u, func_08C319A4, Word);  // no comparable synthetic case yet
HOT178(0x08C3FBBCu, func_08C3FBBC, Word);
HOT178(0x08C40904u, func_08C40904, Word);
HOT178(0x08C4AD60u, func_08C4AD60, Word);
// HOT178(0x08C55560u, func_08C55560, Word);  // no comparable synthetic case yet
// HOT178(0x08C55594u, func_08C55594, Word);  // no comparable synthetic case yet
// HOT178(0x08C6FE74u, func_08C6FE74, Word);  // no comparable synthetic case yet
// HOT178(0x08C6FEA8u, func_08C6FEA8, Word);  // no comparable synthetic case yet
HOT178(0x08C747B4u, func_08C747B4, Word);
HOT178(0x08C747E8u, func_08C747E8, Word);
HOT178(0x08C7481Cu, func_08C7481C, Word);
HOT178(0x08C74850u, func_08C74850, Word);
HOT178(0x08C74968u, func_08C74968, Word);
HOT178(0x08C74ACCu, func_08C74ACC, Word);
HOT178(0x08C7499Cu, func_08C7499C, Word);
HOT178(0x08C749D0u, func_08C749D0, Word);
// HOT178(0x08C766B4u, func_08C766B4, Word);  // no comparable synthetic case yet
HOT178(0x08C7671Cu, func_08C7671C, Word);
HOT178(0x08C7682Cu, func_08C7682C, Word);
HOT178(0x08C76A38u, func_08C76A38, Word);
HOT178(0x08C766E8u, func_08C766E8, Word);
