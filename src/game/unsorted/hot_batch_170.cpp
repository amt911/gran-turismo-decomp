// Hot batch 170: six-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters, setters, bit tests, guarded loads and tail calls that set up their arguments).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_170 {
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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0881AA7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[2] << 5; G[2] = (G[2] << 1) + G[3] + 0x30u; }
void func_0885E6F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x5CCu; G[5] &= 0xFFu; G[6] &= 0xFFu; G[7] &= 0xFFu; jump(ctx, 0x08918298u); }
void func_08869D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u) + 0x4238u; G[6] = 0x08C9F654u; G[5] = 0u; jump(ctx, 0x0886B9ACu); }
void func_0886B344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 4u); G[2] = word(rt, G[5] + 4u); G[3] = word(rt, G[5]); put(rt, G[6] + 0x78u, G[2]); put(rt, G[6] + 0x74u, G[3]); }
void func_08881D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xCu); G[5] &= 0xFFu; G[2] = ins(word(rt, G[3] + 0x16ECu), G[5], 1u); put(rt, G[3] + 0x16ECu, G[2]); }
void func_08885288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xCu); G[5] &= 0xFFu; G[2] = ins(word(rt, G[3] + 0x78u), G[5], 14u); put(rt, G[3] + 0x78u, G[2]); }
void func_08891080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x9Cu, G[9]); put(rt, G[4] + 0x8Cu, G[5]); put(rt, G[4] + 0x90u, G[6]); put(rt, G[4] + 0x94u, G[7]); put(rt, G[4] + 0x98u, G[8]); }
void func_08891278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x14u, G[9]); put(rt, G[4] + 4u, G[5]); put(rt, G[4] + 8u, G[6]); put(rt, G[4] + 0xCu, G[7]); put(rt, G[4] + 0x10u, G[8]); }
void func_08891488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x6Au) ^ 1u; G[2] = 0xFFFFFFFFu; G[4] = 2u; if (G[3] == 0u) G[2] = 2u; }
void func_08896D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; store8(G[4], 0xFFu); store8(G[4] + 3u, 0u); store8(G[4] + 1u, 0u); store8(G[4] + 2u, 0u); }
void func_088A3DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x18u); G[2] = 0u; if (G[3] != 0u) G[2] = word(rt, G[3] + 0xCu); }
void func_088AF2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0x10u); G[2] = G[5] << 10; G[5] = (G[5] << 6) + G[2]; G[4] += G[5]; jump(ctx, 0x088AB45Cu); }
void func_088B18C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] < 4u ? 1u : 0u; if (G[2] == 0u) G[5] = 0u; G[5] = (G[5] << 2) + G[4]; G[2] = word(rt, G[5] + 0x2CCu); }
void func_088B3D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = ((G[4] + 0x1Fu) >> 5) << 2; G[2] = G[4] + G[3] + 6u; }
void func_088C547C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[5] = (G[5] << 2) + G[2]; G[3] = word(rt, G[5]); G[2] = word(rt, G[3]); }
void func_088C5494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[5] = (G[5] << 2) + G[2]; G[2] = word(rt, G[5]) + 0x40u; }
void func_088D04FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5] + 4u); ctx.fpr[1] = float_at(rt, G[5]); G[2] = G[4]; put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); put(rt, G[4], bits_of(ctx.fpr[1])); }
void func_088DAAC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x130u); G[4] = word(rt, G[3] + 0x3Cu); G[2] = (word(rt, G[3] + 0x38u) | G[4]) != 0u ? 1u : 0u; }
void func_088DAADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x130u); G[4] = word(rt, G[3] + 0x44u); G[2] = (word(rt, G[3] + 0x40u) | G[4]) != 0u ? 1u : 0u; }
void func_088E43A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x18u); G[25] = G[2]; G[5] &= 0xFFu; jump(ctx, G[25]); }
void func_088E444C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x20u); G[25] = G[2]; jump(ctx, G[25]); }
void func_088E472C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = (G[5] << 3) + G[2]; G[2] = word(rt, G[5] + 0x468u); G[3] = word(rt, G[5] + 0x46Cu); }
void func_088E4860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = (G[5] << 3) + G[2]; G[2] = word(rt, G[5] + 0x488u); G[3] = word(rt, G[5] + 0x48Cu); }
void func_088E845C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2E4u); G[2] = word(rt, G[4] + 0x2E0u); if (G[3] != 0u) G[2] = word(rt, G[3] + 0x30u); }
void func_088E8A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 2) + G[4]; G[2] = word(rt, G[5] + 0x2A4u) + 1u; put(rt, G[5] + 0x2A4u, G[2]); }
void func_088EEE98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = G[5] << 7; G[5] = (G[5] << 4) + G[3]; G[2] += G[5]; }
void func_088F1B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x9C8u); put(rt, G[4] + 0x9C8u, G[5]); G[2] = (G[2] ^ G[5]) != 0u ? 1u : 0u; store8(G[4] + 0x9CCu, G[2]); }
void func_088F89FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (((G[5] << 1) + G[6]) << 2) + G[4]; G[2] = word(rt, G[5] + 4u); }
void func_08900F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[5] += G[2]; if (G[2] != 0u) put(rt, G[4] + 4u, G[5]); }
void func_08905984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; if (G[4] == 0u) return; G[5] = word(rt, G[4] + 0x68u); jump(ctx, 0x08AF8174u); }
void func_08915D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x10u) ^ 2u; G[2] = 8u; G[4] = 5u; if (G[3] != 0u) G[2] = 5u; }
void func_08917ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const bool negative = static_cast<std::int32_t>(G[6]) < 0; G[4] = word(rt, G[4] + 8u); jump(ctx, negative ? 0x089281E4u : 0x08927550u); }
void func_089273D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[4] = load8(G[4] + 0x204Cu); G[3] = G[2] + 0x2010u; G[2] += 0x1FD4u; if (G[4] != 0u) G[2] = G[3]; }
void func_089298A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3DCCCCCDu; ctx.fpr[0] = std::bit_cast<float>(0x3DCCCCCDu); put(rt, G[4] + 4u, 0x3DCCCCCDu); put(rt, G[4], 0u); }
void func_0892A58C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load16(G[5]); G[2] = G[4]; G[4] += 4u; store16(G[2], G[3]); G[5] = word(rt, G[5] + 4u); jump(ctx, 0x0892A454u); }
void func_0892B410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); put(rt, G[4], G[5]); G[2] = ~G[2] & G[5]; put(rt, G[4] + 4u, G[2]); }
void func_0892B428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = 1u << (G[5] & 31u); G[2] = (G[2] & G[3]) != 0u ? 1u : 0u; }
void func_0892C778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = G[5] << 9; G[5] = (G[5] << 6) + G[3]; G[2] += G[5]; }
void func_0892C790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = G[5] << 9; G[5] = (G[5] << 6) + G[3]; G[2] += G[5]; }
void func_0892F878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[5] = (G[5] << 4) + G[2]; G[2] = word(rt, G[5] + 8u); G[3] = word(rt, G[5] + 0xCu); }
void func_089311B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[5]; G[7] = G[6]; G[5] = G[4]; G[6] = G[2]; G[4] = 0x22u; jump(ctx, 0x089310A0u); }
void func_089318C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = load8(G[3] + 0x89u); G[4] = G[2] << 3; G[2] = (G[2] << 1) + G[4]; }
void func_08938E50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x10u, 0u); }
void func_08947AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = high_half(0x08CF2EC4u); ctx.fpr[1] = float_at(rt, 0x08CF2EC4u); ctx.fpr[0] = float_at(rt, G[3] + 0x778u); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; }
void func_089557CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = sext(load16(G[3] + 0x74Cu), 16u); if (static_cast<std::int32_t>(G[2]) <= 0) store16(G[3] + 0x74Cu, G[5]); }
void func_0899D0B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x13Cu); put(rt, G[4] + 0x12Cu, bits_of(ctx.fpr[12])); G[2] |= 0x40u; put(rt, G[4] + 0x13Cu, G[2]); put(rt, G[4] + 0x124u, bits_of(ctx.fpr[12])); }
void func_0899D14C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x13Cu); put(rt, G[4] + 0x130u, bits_of(ctx.fpr[12])); G[2] |= 0x40u; put(rt, G[4] + 0x13Cu, G[2]); put(rt, G[4] + 0x128u, bits_of(ctx.fpr[12])); }
void func_089BAF28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CF6FDCu); G[2] = word(rt, G[4] + 0x58u); G[4] = word(rt, 0x08CF6FDCu); G[2] = (G[2] ^ G[4]) != 0u ? 1u : 0u; }
void func_089C5700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x08D74FDCu; G[2] = G[4]; put(rt, G[4], G[3]); }
void func_089E0428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 3) + 0x1E0u; G[2] = G[4] + G[5]; put(rt, G[2] + 0xCu, bits_of(ctx.fpr[13])); put(rt, G[2] + 8u, bits_of(ctx.fpr[12])); jump(ctx, 0x089E00E4u); }
void func_089FB374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0xFFFFFFFFu; put(rt, G[4] + 0xCu, G[3]); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); jump(ctx, 0x089FB344u); }
void func_08A38B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x18u); ctx.fpr[1] = float_at(rt, G[4] + 0x1Cu); put(rt, G[4] + 0x40u, 0u); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; put(rt, G[4] + 0x20u, bits_of(ctx.fpr[0])); }
void func_08A4DD0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = word(rt, G[2] + 0x1Cu); G[3] = load16(G[3] + 0xCu); ctx.fpr[1] = std::bit_cast<float>(G[3]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); }
void func_08A4DD24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = word(rt, G[2] + 0x1Cu); G[3] = load16(G[3] + 0xEu); ctx.fpr[1] = std::bit_cast<float>(G[3]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); }
void func_08A4F76C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; if (G[5] == 0u) return; jump(ctx, 0x08C7AC80u); }
void func_08A50EA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[16] = 0.0f; G[1] = 0x3F800000u; ctx.fpr[18] = 1.0f; ctx.fpr[19] = ctx.fpr[18]; ctx.fpr[17] = ctx.fpr[16]; jump(ctx, 0x08A50CF4u); }
void func_08A5D4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CF6FDCu); G[2] = word(rt, G[4] + 0xCu); G[4] = word(rt, 0x08CF6FDCu); G[2] = (G[2] ^ G[4]) != 0u ? 1u : 0u; }
void func_08A70424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x14u); G[2] = 4u; if (G[3] != 0u) G[2] = word(rt, G[3] + 0x14u); }
void func_08A71064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x14u); G[2] = 4u; if (G[3] != 0u) G[2] = word(rt, G[3] + 0x14u); }
void func_08A76764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[3] = 1u; store8(G[4] + 4u, 1u); G[4] += 8u; put(rt, G[2], 0u); jump(ctx, 0x08A68FD4u); }
void func_08A9032C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[6]; G[8] = G[7]; G[6] = G[5]; G[7] = G[2]; G[5] = 1u; jump(ctx, 0x08A90034u); }
void func_08A99680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[6] = word(rt, G[4] + 0x18u); G[5] <<= 2; G[4] = G[2]; G[5] = G[2] + G[5]; jump(ctx, 0x08C2E514u); }
void func_08A9E20C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); put(rt, G[4] + 0x24u, G[6]); G[2] = word(rt, G[3] + 0x20u); G[25] = G[2]; jump(ctx, G[25]); }
void func_08ABCF2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD62B8u; put(rt, G[4], G[2]); G[3] = high_half(0x08CFFAACu); put(rt, 0x08CFFAACu, 0u); jump(ctx, 0x08ABD254u); }
void func_08ABE760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[8] = G[5]; G[3] = sra(G[4], 31u); G[7] = G[6]; G[5] = G[3]; G[6] = G[8]; jump(ctx, 0x08ABE53Cu); }
void func_08AC0AAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[6]; G[8] = G[7] & 0xFFu; G[6] = G[5]; G[7] = G[2]; G[5] = 0u; jump(ctx, 0x08AC0A14u); }
void func_08AC1060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[7]; G[9] = G[8] & 0xFFu; G[7] = G[6]; G[8] = G[2]; G[6] = 0u; jump(ctx, 0x08AC0F08u); }
void func_08AC2864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0xFFFFFFFFu; if (G[4] != 0u) G[2] = load8(G[4]); }
void func_08AC287C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0xFFFFFFFFu; if (G[4] != 0u) G[2] = load8(G[4] + 1u); }
void func_08AC28B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0u; if (G[4] != 0u) G[2] = sext(load16(G[4] + 4u), 16u); }
void func_08AC28D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0u; if (G[4] != 0u) G[2] = word(rt, G[4] + 4u); }
void func_08AC28E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); ctx.fpr[0] = 0.0f; if (G[4] != 0u) ctx.fpr[0] = float_at(rt, G[4] + 4u); }
void func_08AC4464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; G[3] = 0u; put(rt, G[4] + 0x120u, 0u); put(rt, G[4] + 0x124u, 0u); G[4] += 8u; jump(ctx, 0x08B0C778u); }
void func_08ACA4BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4]) + 0xBCAAB0AAu) == 0u ? 1u : 0u; }
void func_08ACC8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[7] = G[6]; G[8] = 0u; G[6] = G[5]; G[5] = 0u; jump(ctx, 0x08ACC704u); }
void func_08ACC8D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[8] = G[6]; G[7] = 0u; G[6] = G[5]; G[5] = 1u; jump(ctx, 0x08ACC704u); }
void func_08AD4430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD6E20u; put(rt, G[4], G[2]); G[3] = high_half(0x08D404DCu); put(rt, 0x08D404DCu, 0u); jump(ctx, 0x08ACFFC4u); }
void func_08ADD2AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x10u, 0u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); }
void func_08AF6E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x30u); G[2] = 1u; if (G[3] != 0u) G[2] = load16(G[3]); }
void func_08B089BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[5] == 0u) return; G[4] = word(rt, G[4]); jump(ctx, 0x08B088F0u); }
void func_08B08A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; if (G[5] != 0u) return; G[2] = word(rt, G[4]); G[2] = sext(load16(G[2] + 0x3Cu), 16u); }
void func_08B08D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[5] == 0u) return; G[4] = word(rt, G[4]); jump(ctx, 0x08B08B60u); }
void func_08B0C43C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = 0u; if (G[3] != 0u) G[2] = word(rt, G[3] + 0x38u); }
void func_08B19154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], G[5]); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); jump(ctx, 0x08B18D64u); }
void func_08B1BAA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = 1u << (G[5] & 31u); G[2] = (G[2] & G[3]) != 0u ? 1u : 0u; }
void func_08B1C140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = 1u << (G[5] & 31u); G[2] = (G[2] & G[3]) != 0u ? 1u : 0u; }
void func_08B2B6F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); }
void func_08B35220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, 0x08D694C4u, G[4]); G[3] = high_half(0x08D694C0u); G[2] = 0u; put(rt, 0x08D694C0u, G[5]); }
void func_08B47B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[5] != 0u) { G[2] = load16(G[4]); put(rt, G[5], G[2]); } G[2] = G[4] + 0x18u; }
void func_08B52308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 3) + G[4]; store16(G[5] + 0x4Cu, G[8]); put(rt, G[5] + 0x48u, G[6]); store8(G[4] + 0x24u, G[7]); }
void func_08B52560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; put(rt, G[4], 0u); G[4] = G[5]; G[5] = G[6]; G[6] = G[2] + 0x18u; jump(ctx, 0x08B5596Cu); }
void func_08B55570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFu; G[3] = G[2] < G[4] ? 1u : 0u; if (G[3] == 0u) G[2] = G[4]; G[4] = high_half(0x08D6D0C0u); put(rt, 0x08D6D0C0u, G[2]); }
void func_08B5C218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = 0x08D6E118u; G[5] = 0x08C95D4Cu; G[6] = 0u; jump(ctx, 0x08B61B90u); }
void func_08B815E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = static_cast<std::uint32_t>(std::countr_zero(G[3])); G[3] >>= (G[2] & 31u); put(rt, G[4], G[3]); }
void func_08BA4D08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0u; G[3] = 0u; }
void func_08BACB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC22D8u; put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[5]); store8(G[4] + 8u, 0u); }
void func_08BBC044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CE1248u; G[4] = (G[4] << 2) + G[2]; G[2] = word(rt, G[4]); }
void func_08BD2C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; if (G[4] == 0u) return; jump(ctx, 0x08AF75A0u); }
void func_08BD2D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 0u; if (G[4] == 0u) return; jump(ctx, 0x08AF8AB0u); }
void func_08BD2DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5]; if (static_cast<std::int32_t>(G[5]) >= 0) return; G[4] = word(rt, G[4]); jump(ctx, 0x08AF2954u); }
void func_08BD5AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BD5B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BD5B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BD5B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BD5B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BD5BEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[3] = G[2] + G[4]; if (G[2] != 0u) put(rt, G[5], G[3]); }
void func_08BDCCA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[3] = bits_of(ctx.fpr[12]); }
void func_08C45E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[3] = bits_of(ctx.fpr[12]); }
void func_08C75158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[8] = sext(G[6], 8u); G[7] = G[5]; G[6] = word(rt, G[2] - 0xCu); G[5] = 0u; jump(ctx, 0x08C74FACu); }
void func_08C751B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[8] = sext(G[7], 8u); G[7] = G[6]; G[5] -= G[2]; G[6] = 0u; jump(ctx, 0x08C74FACu); }
void func_08C76328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[7]); G[2] = word(rt, G[4]); G[6] -= G[5]; G[8] = word(rt, G[7] - 0xCu); G[5] -= G[2]; jump(ctx, 0x08C75FF8u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_170
} // namespace gt::game

#define HOT170(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_170::name, gt::ReturnKind::kind)
HOT170(0x0881AA7Cu, func_0881AA7C, Word);
HOT170(0x0885E6F8u, func_0885E6F8, Word);
HOT170(0x08869D58u, func_08869D58, Word);
HOT170(0x0886B344u, func_0886B344, Word);
HOT170(0x08881D28u, func_08881D28, Word);
HOT170(0x08885288u, func_08885288, Word);
HOT170(0x08891080u, func_08891080, Word);
HOT170(0x08891278u, func_08891278, Word);
HOT170(0x08891488u, func_08891488, Word);
HOT170(0x08896D7Cu, func_08896D7C, Word);
HOT170(0x088A3DB8u, func_088A3DB8, Word);
HOT170(0x088AF2F8u, func_088AF2F8, Word);
HOT170(0x088B18C0u, func_088B18C0, Word);
HOT170(0x088B3D20u, func_088B3D20, Word);
HOT170(0x088C547Cu, func_088C547C, Word);
HOT170(0x088C5494u, func_088C5494, Word);
HOT170(0x088D04FCu, func_088D04FC, Word);
HOT170(0x088DAAC4u, func_088DAAC4, Word);
HOT170(0x088DAADCu, func_088DAADC, Word);
// HOT170(0x088E43A8u, func_088E43A8, Word);  // no comparable synthetic case yet
// HOT170(0x088E444Cu, func_088E444C, Word);  // no comparable synthetic case yet
HOT170(0x088E472Cu, func_088E472C, DoubleWord);
HOT170(0x088E4860u, func_088E4860, DoubleWord);
HOT170(0x088E845Cu, func_088E845C, Word);
HOT170(0x088E8A0Cu, func_088E8A0C, Word);
HOT170(0x088EEE98u, func_088EEE98, Word);
HOT170(0x088F1B60u, func_088F1B60, Word);
HOT170(0x088F89FCu, func_088F89FC, Word);
HOT170(0x08900F7Cu, func_08900F7C, Word);
HOT170(0x08905984u, func_08905984, Word);
HOT170(0x08915D9Cu, func_08915D9C, Word);
HOT170(0x08917ECCu, func_08917ECC, Word);
HOT170(0x089273D8u, func_089273D8, Word);
HOT170(0x089298A8u, func_089298A8, Word);
// HOT170(0x0892A58Cu, func_0892A58C, Word);  // no comparable synthetic case yet
HOT170(0x0892B410u, func_0892B410, Word);
HOT170(0x0892B428u, func_0892B428, Word);
HOT170(0x0892C778u, func_0892C778, Word);
HOT170(0x0892C790u, func_0892C790, Word);
HOT170(0x0892F878u, func_0892F878, DoubleWord);
HOT170(0x089311B4u, func_089311B4, Word);
HOT170(0x089318C4u, func_089318C4, Word);
HOT170(0x08938E50u, func_08938E50, Word);
HOT170(0x08947AA0u, func_08947AA0, Float);
HOT170(0x089557CCu, func_089557CC, Word);
HOT170(0x0899D0B0u, func_0899D0B0, Word);
HOT170(0x0899D14Cu, func_0899D14C, Word);
HOT170(0x089BAF28u, func_089BAF28, Word);
HOT170(0x089C5700u, func_089C5700, Word);
HOT170(0x089E0428u, func_089E0428, Word);
HOT170(0x089FB374u, func_089FB374, Word);
HOT170(0x08A38B14u, func_08A38B14, Word);
HOT170(0x08A4DD0Cu, func_08A4DD0C, Float);
HOT170(0x08A4DD24u, func_08A4DD24, Float);
HOT170(0x08A4F76Cu, func_08A4F76C, Word);
HOT170(0x08A50EA8u, func_08A50EA8, Word);
HOT170(0x08A5D4F4u, func_08A5D4F4, Word);
HOT170(0x08A70424u, func_08A70424, Word);
HOT170(0x08A71064u, func_08A71064, Word);
HOT170(0x08A76764u, func_08A76764, Word);
HOT170(0x08A9032Cu, func_08A9032C, Word);
HOT170(0x08A99680u, func_08A99680, Word);
HOT170(0x08A9E20Cu, func_08A9E20C, Word);
HOT170(0x08ABCF2Cu, func_08ABCF2C, Word);
HOT170(0x08ABE760u, func_08ABE760, Word);
HOT170(0x08AC0AACu, func_08AC0AAC, Word);
HOT170(0x08AC1060u, func_08AC1060, Word);
HOT170(0x08AC2864u, func_08AC2864, Word);
HOT170(0x08AC287Cu, func_08AC287C, Word);
HOT170(0x08AC28B8u, func_08AC28B8, Word);
HOT170(0x08AC28D0u, func_08AC28D0, Word);
HOT170(0x08AC28E8u, func_08AC28E8, Float);
HOT170(0x08AC4464u, func_08AC4464, Word);
HOT170(0x08ACA4BCu, func_08ACA4BC, Word);
// HOT170(0x08ACC8C0u, func_08ACC8C0, Word);  // no comparable synthetic case yet
// HOT170(0x08ACC8D8u, func_08ACC8D8, Word);  // no comparable synthetic case yet
HOT170(0x08AD4430u, func_08AD4430, Word);
HOT170(0x08ADD2ACu, func_08ADD2AC, Word);
HOT170(0x08AF6E90u, func_08AF6E90, Word);
HOT170(0x08B089BCu, func_08B089BC, Word);
HOT170(0x08B08A0Cu, func_08B08A0C, Word);
HOT170(0x08B08D30u, func_08B08D30, Word);
HOT170(0x08B0C43Cu, func_08B0C43C, Word);
HOT170(0x08B19154u, func_08B19154, Word);
HOT170(0x08B1BAA4u, func_08B1BAA4, Word);
HOT170(0x08B1C140u, func_08B1C140, Word);
HOT170(0x08B2B6F4u, func_08B2B6F4, Word);
HOT170(0x08B35220u, func_08B35220, Word);
HOT170(0x08B47B28u, func_08B47B28, Word);
HOT170(0x08B52308u, func_08B52308, Word);
HOT170(0x08B52560u, func_08B52560, Word);
HOT170(0x08B55570u, func_08B55570, Word);
HOT170(0x08B5C218u, func_08B5C218, Word);
HOT170(0x08B815E4u, func_08B815E4, Word);
HOT170(0x08BA4D08u, func_08BA4D08, DoubleWord);
HOT170(0x08BACB08u, func_08BACB08, Word);
HOT170(0x08BBC044u, func_08BBC044, Word);
HOT170(0x08BD2C50u, func_08BD2C50, Word);
HOT170(0x08BD2D48u, func_08BD2D48, Word);
HOT170(0x08BD2DDCu, func_08BD2DDC, Word);
HOT170(0x08BD5AF4u, func_08BD5AF4, Word);
HOT170(0x08BD5B0Cu, func_08BD5B0C, Word);
HOT170(0x08BD5B24u, func_08BD5B24, Word);
HOT170(0x08BD5B3Cu, func_08BD5B3C, Word);
HOT170(0x08BD5B54u, func_08BD5B54, Word);
HOT170(0x08BD5BECu, func_08BD5BEC, Word);
HOT170(0x08BDCCA0u, func_08BDCCA0, DoubleWord);
HOT170(0x08C45E00u, func_08C45E00, DoubleWord);
HOT170(0x08C75158u, func_08C75158, Word);
HOT170(0x08C751B0u, func_08C751B0, Word);
// HOT170(0x08C76328u, func_08C76328, Word);  // no comparable synthetic case yet
