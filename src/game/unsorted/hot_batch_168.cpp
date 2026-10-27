// Hot batch 168: four-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters, setters, small tests, float helpers and tail calls that set up their arguments). No names:
// none has evidence. A few entries of the function table that hold two functions are left out.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_168 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
// `wsbh`: the bytes swapped within each halfword.
constexpr std::uint32_t wsbh(std::uint32_t value) { return ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0885D54C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x16u); G[2] = G[3] != 0u ? G[4] + 0x120u : 0u; }
void func_0885D55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x16u); G[2] = G[3] != 0u ? G[4] + 0x50u : 0u; }
void func_0885DFE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5] + G[4]; G[2] = 1u; store8(G[4] + 0x3E8Cu, 1u); }
void func_0885E6D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x5CCu; G[5] = 0u; jump(ctx, 0x08917848u); }
void func_08869F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u) + 0x4238u; G[6] &= 0xFFFFu; jump(ctx, 0x0886B4FCu); }
void func_0886B30C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u) + 0x20Cu; G[6] = 0xFEu; jump(ctx, 0x08B7C830u); }
void func_088728B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4] + 0xCu; G[4] = word(rt, G[4] + 0x5Cu); jump(ctx, 0x08872834u); }
void func_08875860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[5]; G[5] = 0x08C9FF6Cu; jump(ctx, 0x088756F0u); }
void func_0887C8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u); G[6] &= 0xFFu; G[7] &= 0xFFu; jump(ctx, 0x0887C894u); }
void func_088807F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = word(rt, G[4] + 8u); G[4] = word(rt, G[4] + 0xCu); jump(ctx, 0x08880760u); }
void func_08884FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[4] + 0x170u, 0x3F800000u); }
void func_08887094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[5] <<= 3; G[2] += G[5]; }
void func_088884C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); G[5] <<= 3; G[2] += G[5]; }
void func_088A5CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[4] = word(rt, G[2] + 0x4C8u) + 0x690u; jump(ctx, 0x08944AACu); }
void func_088AA6C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x4Cu) != 0u ? 1u : 0u; }
void func_088C7834(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4] + 8u, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4], 0u); }
void func_088C97B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CE6DE0u); G[2] = word(rt, 0x08CE6DE0u) != 0u ? 1u : 0u; }
void func_088CEF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0xD4u); ctx.fpr[0] = float_at(rt, G[4] + 0xD0u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; }
void func_088D4D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] += G[2]; store8(G[5] + 0x58u, G[6]); }
void func_088D4E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] += G[2]; G[2] = load8(G[5] + 0x61u); }
void func_088D9C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[6] + 5u; G[4] = G[2] < G[6] ? 1u : 0u; G[3] = G[4] + G[7]; }
void func_088DA4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x130u); G[2] = word(rt, G[5] + 0x20u); G[3] = word(rt, G[5] + 0x24u); }
void func_088E4744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] += G[2]; G[2] = sext(load8(G[5] + 0x4A8u), 8u); }
void func_088E4A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x530u) != 0u ? 1u : 0u; }
void func_088EF50C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x92u); G[2] = G[3] != 0u ? G[4] : 0u; }
void func_088EF924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xC4u) - 2u; G[2] = G[2] < 2u ? 1u : 0u; }
void func_08909AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[14] = float_at(rt, G[5] + 8u); ctx.fpr[12] = float_at(rt, G[5]); ctx.fpr[13] = float_at(rt, G[5] + 4u); jump(ctx, 0x08909A5Cu); }
void func_08915D8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4] + 0x10u) ^ 2u) == 0u ? 1u : 0u; }
void func_08917E70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = (G[6] << 2) + G[4]; G[4] = word(rt, G[6] + 8u); jump(ctx, 0x089281E4u); }
void func_08917F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); store8(G[2] + 0x2180u, 0u); G[4] = word(rt, G[4] + 8u); jump(ctx, 0x08927278u); }
void func_0891A854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = std::bit_cast<float>(G[5]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[5])); put(rt, G[4] + 0x200u, bits_of(ctx.fpr[0])); }
void func_0892A794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 8u, 0u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); jump(ctx, 0x0892A698u); }
void func_0892B7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 4u) ^ 1u; store8(G[4] + 4u, G[2]); }
void func_0892F868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x10u); G[3] = 0u; G[2] = word(rt, G[5]); }
void func_08931900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 4u); G[2] = word(rt, G[5] + 8u); G[3] = word(rt, G[5] + 0xCu); }
void func_0893388C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CF2968u); G[4] = word(rt, 0x08CF2968u); put(rt, 0x08CF2968u, 0u); jump(ctx, 0x08B64654u); }
void func_08938EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[5] <<= 4; G[2] += G[5]; }
void func_089401FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 0x714u); put(rt, G[2] + 0x718u, bits_of(ctx.fpr[0])); }
void func_08944378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (load16(G[4] + 0x128u) ^ 0x202u) == 0u ? 1u : 0u; }
void func_0894D074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4] + 0xDCu) ^ 1u) == 0u ? 1u : 0u; }
void func_089557AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = (load8(G[3] + 0x74Bu) >> 1) & 1u; }
void func_089772C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x1C0u, bits_of(ctx.fpr[13])); put(rt, G[4] + 0x1B8u, G[5]); put(rt, G[4] + 0x1BCu, bits_of(ctx.fpr[12])); }
void func_089BE874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x2A8u, G[5]); G[4] += 0x268u; G[5] = G[6]; jump(ctx, 0x08B7BB6Cu); }
void func_08A0F0BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x1A8u); G[5] = 0u; jump(ctx, G[25]); }
void func_08A0F15C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x1A8u); G[5] = 1u; jump(ctx, G[25]); }
void func_08A142B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5]); G[4] += 0x10Du; G[6] = 0x80u; jump(ctx, 0x08B7C830u); }
void func_08A1C1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; jump(ctx, 0x08B2D61Cu); }
void func_08A34D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[1] = 0x3F800000u; ctx.fpr[14] = 1.0f; ctx.fpr[13] = 0.0f; jump(ctx, 0x08A34D78u); }
void func_08A4CF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x74u) + 1u; put(rt, G[4] + 0x74u, G[2]); jump(ctx, 0x08A50B8Cu); }
void func_08A4DB50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = 0u; G[7] = 0u; G[8] = 0u; jump(ctx, 0x08A4D79Cu); }
void func_08A4DD3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[5] = 0u; G[4] = word(rt, G[2] + 0x18u); jump(ctx, 0x08AF6CE0u); }
void func_08A76744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 8u, 0u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); jump(ctx, 0x08A760B4u); }
void func_08AB87C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); store8(G[2] + 0x27Au, G[5]); put(rt, G[2] + 0x268u, bits_of(ctx.fpr[12])); }
void func_08AB9C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4] + 8u; G[4] = 0x08CFF9BCu; jump(ctx, 0x08B1F9B4u); }
void func_08AB9C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4] + 8u; G[4] = 0x08CFF9BCu; jump(ctx, 0x08B1F7B4u); }
void func_08ABD78C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = high_half(0x08CDC588u); G[2] = word(rt, 0x08CDC588u); G[3] = word(rt, 0x08CDC58Cu); }
void func_08ABF6E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0xCu, G[5]); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); }
void func_08AC2900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[3] != 0u ? G[3] + 4u : 0u; }
void func_08AC2EDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4] + 8u, 0u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); }
void func_08AC3270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; put(rt, G[4] + 8u, G[5]); }
void func_08ACA3B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; put(rt, G[4] + 4u, 0u); put(rt, G[4], 0u); }
void func_08AD791C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x14u); ctx.fpr[14] = psp_mul(ctx.fpr[0], ctx.fpr[14]); ctx.fpr[13] = psp_mul(ctx.fpr[0], ctx.fpr[13]); jump(ctx, 0x08AD78C8u); }
void func_08AD9768(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CDFB3Cu); G[2] = word(rt, 0x08CDFB3Cu); put(rt, 0x08CDFB3Cu, G[4]); }
void func_08AD9E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x08D405D0u; G[2] = word(rt, G[3] + 4u); }
void func_08AECEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x209Cu); G[2] = word(rt, G[4] + 0x2098u) - G[3]; }
void func_08AECFD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2094u); G[2] = G[4] + G[3] + 0x20A0u; }
void func_08AF42FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x34u, bits_of(ctx.fpr[14])); put(rt, G[4] + 0x2Cu, bits_of(ctx.fpr[12])); put(rt, G[4] + 0x30u, bits_of(ctx.fpr[13])); }
void func_08AF430C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x28u, bits_of(ctx.fpr[14])); put(rt, G[4] + 0x20u, bits_of(ctx.fpr[12])); put(rt, G[4] + 0x24u, bits_of(ctx.fpr[13])); }
void func_08AF431C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x1Cu, bits_of(ctx.fpr[14])); put(rt, G[4] + 0x14u, bits_of(ctx.fpr[12])); put(rt, G[4] + 0x18u, bits_of(ctx.fpr[13])); }
void func_08AF4B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[0] = float_at(rt, G[4] + 0x14u); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; }
void func_08AF70B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load16(G[4] + 0x14u) - 1u; store16(G[4] + 0x14u, G[2]); }
void func_08AFC9F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x73484453u; put(rt, G[4], G[2]); }
void func_08AFCA04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x53484453u; put(rt, G[4], G[2]); }
void func_08B08998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] &= 0xFFu; G[2] = G[5] != 0u ? G[4] + 0x44u : 0u; }
void func_08B089DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = 2u; put(rt, G[3] + 0x1A8u, 2u); }
void func_08B09908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = 1u; store8(G[3] + 0x11Cu, 1u); }
void func_08B1093C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[5] + 0xA8u, 2u); G[2] = 0u; }
void func_08B1212C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x228u) << 16; G[3] = 0u; }
void func_08B1A8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 1u; G[2] = high_half(0x08D68A24u); store8(0x08D68A24u, 1u); }
void func_08B1EB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = G[2] + 1u; put(rt, G[4] + 4u, G[3]); }
void func_08B28FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[13] = ctx.fpr[13] - ctx.fpr[12]; ctx.fpr[14] = psp_mul(ctx.fpr[14], ctx.fpr[13]); ctx.fpr[0] = ctx.fpr[12] + ctx.fpr[14]; }
void func_08B31950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[5], 1u); G[2] = 1u; }
void func_08B37B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[3] = G[2] + 1u; put(rt, G[4] + 0x10u, G[3]); }
void func_08B3F7B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] >> 16; store16(G[4] + 6u, G[2]); store16(G[4] + 4u, G[5]); }
void func_08B3FA3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = wsbh(G[5] & 0xFFFFu); store16(G[4] + 2u, G[5]); }
void func_08B3FA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = wsbh(load16(G[4] + 2u)) & 0xFFFFu; }
void func_08B40148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x20u); jump(ctx, G[25]); }
void func_08B60890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; jump(ctx, 0x08B7E820u); }
void func_08B644AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 1u) - 1u; store8(G[4] + 1u, G[2]); jump(ctx, 0x08B64344u); }
void func_08B64CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0x08C96BD8u + 1u; }
void func_08B663A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = bits_of(ctx.fpr[12]) & 0x7FFFFFFFu; ctx.fpr[0] = std::bit_cast<float>(G[2]); }
void func_08B779D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x2000u; G[2] = 0u; put(rt, G[5] + 4u, 0x2000u); }
void func_08BABC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] &= 0xFFu; G[6] &= 0xFFu; G[5] = G[6] - G[5]; jump(ctx, 0x08894CC8u); }
void func_08BABCA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = sext(G[5], 8u); G[6] = sext(G[6], 8u); G[5] = G[6] - G[5]; jump(ctx, 0x08894CC8u); }
void func_08C6F4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[4] + 0x20u); G[4] = G[5]; G[5] = 1u; jump(ctx, 0x08B79618u); }
void func_08C76264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[6] -= G[5]; G[5] -= G[2]; jump(ctx, 0x08C75FF8u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_168
} // namespace gt::game

#define HOT168(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_168::name, gt::ReturnKind::kind)
HOT168(0x0885D54Cu, func_0885D54C, Word);
HOT168(0x0885D55Cu, func_0885D55C, Word);
HOT168(0x0885DFE4u, func_0885DFE4, Word);
HOT168(0x0885E6D0u, func_0885E6D0, Word);
HOT168(0x08869F64u, func_08869F64, Word);
HOT168(0x0886B30Cu, func_0886B30C, Word);
// HOT168(0x088728B0u, func_088728B0, Word);  // no comparable synthetic case yet
HOT168(0x08875860u, func_08875860, Word);
HOT168(0x0887C8F0u, func_0887C8F0, Word);
HOT168(0x088807F8u, func_088807F8, Word);
HOT168(0x08884FE4u, func_08884FE4, Word);
HOT168(0x08887094u, func_08887094, Word);
HOT168(0x088884C4u, func_088884C4, Word);
// HOT168(0x088A5CC0u, func_088A5CC0, Word);  // no comparable synthetic case yet
HOT168(0x088AA6C8u, func_088AA6C8, Word);
HOT168(0x088C7834u, func_088C7834, Word);
HOT168(0x088C97B4u, func_088C97B4, Word);
HOT168(0x088CEF88u, func_088CEF88, Float);
HOT168(0x088D4D04u, func_088D4D04, Word);
HOT168(0x088D4E24u, func_088D4E24, Word);
HOT168(0x088D9C24u, func_088D9C24, DoubleWord);
HOT168(0x088DA4B8u, func_088DA4B8, DoubleWord);
HOT168(0x088E4744u, func_088E4744, Word);
HOT168(0x088E4A20u, func_088E4A20, Word);
HOT168(0x088EF50Cu, func_088EF50C, Word);
HOT168(0x088EF924u, func_088EF924, Word);
// HOT168(0x08909AF0u, func_08909AF0, Word);  // no comparable synthetic case yet
HOT168(0x08915D8Cu, func_08915D8C, Word);
HOT168(0x08917E70u, func_08917E70, Word);
HOT168(0x08917F44u, func_08917F44, Word);
HOT168(0x0891A854u, func_0891A854, Word);
HOT168(0x0892A794u, func_0892A794, Word);
HOT168(0x0892B7D0u, func_0892B7D0, Word);
HOT168(0x0892F868u, func_0892F868, DoubleWord);
HOT168(0x08931900u, func_08931900, DoubleWord);
HOT168(0x0893388Cu, func_0893388C, Word);
HOT168(0x08938EF0u, func_08938EF0, Word);
HOT168(0x089401FCu, func_089401FC, Word);
HOT168(0x08944378u, func_08944378, Word);
HOT168(0x0894D074u, func_0894D074, Word);
HOT168(0x089557ACu, func_089557AC, Word);
HOT168(0x089772C0u, func_089772C0, Word);
HOT168(0x089BE874u, func_089BE874, Word);
HOT168(0x08A0F0BCu, func_08A0F0BC, Word);
HOT168(0x08A0F15Cu, func_08A0F15C, Word);
HOT168(0x08A142B8u, func_08A142B8, Word);
// HOT168(0x08A1C1E0u, func_08A1C1E0, Word);  // no comparable synthetic case yet
HOT168(0x08A34D98u, func_08A34D98, Word);
HOT168(0x08A4CF6Cu, func_08A4CF6C, Word);
HOT168(0x08A4DB50u, func_08A4DB50, Word);
HOT168(0x08A4DD3Cu, func_08A4DD3C, Word);
HOT168(0x08A76744u, func_08A76744, Word);
HOT168(0x08AB87C0u, func_08AB87C0, Word);
HOT168(0x08AB9C28u, func_08AB9C28, Word);
HOT168(0x08AB9C38u, func_08AB9C38, Word);
HOT168(0x08ABD78Cu, func_08ABD78C, DoubleWord);
HOT168(0x08ABF6E0u, func_08ABF6E0, Word);
HOT168(0x08AC2900u, func_08AC2900, Word);
HOT168(0x08AC2EDCu, func_08AC2EDC, Word);
HOT168(0x08AC3270u, func_08AC3270, Word);
HOT168(0x08ACA3B8u, func_08ACA3B8, Word);
HOT168(0x08AD791Cu, func_08AD791C, Word);
HOT168(0x08AD9768u, func_08AD9768, Word);
HOT168(0x08AD9E64u, func_08AD9E64, Word);
HOT168(0x08AECEBCu, func_08AECEBC, Word);
HOT168(0x08AECFD4u, func_08AECFD4, Word);
HOT168(0x08AF42FCu, func_08AF42FC, Word);
HOT168(0x08AF430Cu, func_08AF430C, Word);
HOT168(0x08AF431Cu, func_08AF431C, Word);
HOT168(0x08AF4B0Cu, func_08AF4B0C, Float);
HOT168(0x08AF70B4u, func_08AF70B4, Word);
HOT168(0x08AFC9F4u, func_08AFC9F4, Word);
HOT168(0x08AFCA04u, func_08AFCA04, Word);
HOT168(0x08B08998u, func_08B08998, Word);
HOT168(0x08B089DCu, func_08B089DC, Word);
HOT168(0x08B09908u, func_08B09908, Word);
HOT168(0x08B1093Cu, func_08B1093C, Word);
HOT168(0x08B1212Cu, func_08B1212C, DoubleWord);
HOT168(0x08B1A8D4u, func_08B1A8D4, Word);
HOT168(0x08B1EB40u, func_08B1EB40, Word);
HOT168(0x08B28FCCu, func_08B28FCC, Float);
HOT168(0x08B31950u, func_08B31950, Word);
HOT168(0x08B37B34u, func_08B37B34, Word);
HOT168(0x08B3F7B4u, func_08B3F7B4, Word);
HOT168(0x08B3FA3Cu, func_08B3FA3C, Word);
HOT168(0x08B3FA54u, func_08B3FA54, Word);
HOT168(0x08B40148u, func_08B40148, Word);
HOT168(0x08B60890u, func_08B60890, Word);
HOT168(0x08B644ACu, func_08B644AC, Word);
HOT168(0x08B64CC0u, func_08B64CC0, Word);
HOT168(0x08B663A4u, func_08B663A4, Float);
HOT168(0x08B779D8u, func_08B779D8, Word);
HOT168(0x08BABC90u, func_08BABC90, Word);
HOT168(0x08BABCA0u, func_08BABCA0, Word);
HOT168(0x08C6F4CCu, func_08C6F4CC, Word);
// HOT168(0x08C76264u, func_08C76264, Word);  // no comparable synthetic case yet
