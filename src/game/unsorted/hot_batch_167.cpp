// Hot batch 167: three-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters, setters, flag tests and tail calls that set up their arguments in the delay slot). No
// names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_167 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
bool positive(std::uint32_t value) { return static_cast<std::int32_t>(value) > 0; }

} // namespace

#define G ctx.gpr

// Tail calls with arguments set up in the delay slot.
void func_0880456C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = 0u; G[5] = 0xFFFFu; jump(ctx, 0x0880451Cu); }
void func_0880B774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5]); G[4] += 0x30u; jump(ctx, 0x08869244u); }
void func_0885E164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; G[5] = G[6]; jump(ctx, 0x088912CCu); }
void func_0885EC8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[7] = 1u; jump(ctx, 0x0885DFF4u); }
void func_08869230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] += 0x1B0u; G[6] = 1u; jump(ctx, 0x0886A074u); }
void func_088852C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0xCu); G[5] &= 0xFFu; jump(ctx, 0x08884DA0u); }
void func_088852D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0xCu); G[6] &= 0xFFu; jump(ctx, 0x08884DF4u); }
void func_08892468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[10] = 1u; jump(ctx, 0x088921E0u); }
void func_088A5814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[5] = word(rt, G[5]); jump(ctx, 0x088A5660u); }
void func_088D47F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[5] = 0x118u; jump(ctx, 0x08B29D9Cu); }
void func_088D4810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[5] = 0x60u; jump(ctx, 0x08B29D9Cu); }
void func_0892A68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rt.memory().store16(G[4], 0u); G[4] += 4u; jump(ctx, 0x0892A604u); }
void func_0892B74C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 1u; G[6] = 7u; jump(ctx, 0x0892B724u); }
void func_089301B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4]; G[4] = 0u; jump(ctx, 0x089300CCu); }
void func_0894020C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); put(rt, G[2] + 0x714u, bits_of(ctx.fpr[12])); jump(ctx, 0x089401FCu); }
void func_089A6A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 0x08CF6ED8u; jump(ctx, 0x08A76AB4u); }
void func_089B17AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[4] + 0x18u; G[5] = G[4] + 0x30u; jump(ctx, 0x089B14F0u); }
void func_089C1AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = float_at(rt, G[4] + 0xA0u); G[5] = G[4] + 0x90u; jump(ctx, 0x089C1A1Cu); }
void func_089E0270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(G[4] + 0x20Bu, static_cast<std::uint8_t>(G[6]));
    rt.memory().store8(G[4] + 0x20Au, static_cast<std::uint8_t>(G[5]));
    jump(ctx, 0x089E0188u);
}
void func_089E027C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(G[4] + 0x209u, static_cast<std::uint8_t>(G[6]));
    rt.memory().store8(G[4] + 0x208u, static_cast<std::uint8_t>(G[5]));
    jump(ctx, 0x089E0188u);
}
void func_08A1B3B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[5] = word(rt, G[4] + 8u); jump(ctx, 0x08ACAFFCu); }
void func_08AB3DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5]); G[4] += 0x14u; jump(ctx, 0x08B1EB50u); }
void func_08ABF7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[5]; G[5] = 0u; jump(ctx, 0x08ABF770u); }
void func_08ABFA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = word(rt, G[4] + 0xCu); jump(ctx, 0x08B1FA64u); }
void func_08ABFE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = word(rt, G[4] + 0xCu); jump(ctx, 0x08ABFDD0u); }
void func_08AFEA88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[4]; G[5] = G[4] + 0x5Cu; jump(ctx, 0x08AFE3F8u); }
void func_08B00764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[7] = G[4]; G[8] = G[4] + 0x20u; jump(ctx, 0x08B0066Cu); }
void func_08B0DF0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = word(rt, G[4] + 0x38u); jump(ctx, 0x08B10C74u); }
void func_08B0F82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 0u; G[6] = 0u; jump(ctx, 0x08B0F824u); }
void func_08B2B3EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4]; G[4] = 1u; jump(ctx, 0x08B2B37Cu); }
void func_08B35E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; G[5] = G[6]; jump(ctx, 0x08B35DDCu); }
void func_08B35EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] = G[5]; G[5] = G[6]; jump(ctx, 0x08B35E3Cu); }
void func_08B48A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], G[5]); G[4] = G[5]; jump(ctx, 0x08B489D0u); }
void func_08B4B858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = G[5]; G[5] = G[4]; jump(ctx, 0x08B4B7B4u); }
void func_08B59C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x210u, G[6]); put(rt, G[4] + 0x20Cu, G[5]); jump(ctx, 0x08B59C14u); }
void func_08B76904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] = 0u; G[7] = 0u; jump(ctx, 0x08B768A4u); }
void func_08C54D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x10u, G[5]); G[5] = word(rt, G[4] + 0x14u); jump(ctx, 0x08C54CFCu); }
void func_08C54D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[5] = G[2] | G[5]; jump(ctx, 0x08C54CFCu); }
// A COW string's length word (-0xC from its characters) as the extra argument.
void func_08C73718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5]); G[7] = word(rt, G[5] - 0xCu); jump(ctx, 0x08C735D0u); }
void func_08C75504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[6]); G[7] = word(rt, G[6] - 0xCu); jump(ctx, 0x08C752BCu); }
void func_08C7631C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[7]); G[8] = word(rt, G[7] - 0xCu); jump(ctx, 0x08C75FF8u); }

// Stores.
void func_0885F460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); rt.memory().store8(G[2] + 0x5AAu, 0u); }
void func_088E4120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); rt.memory().store8(G[2] + 0x524u, 0u); }
void func_089178A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); rt.memory().store8(G[2] + 0x204Cu, 0u); }
void func_08B09918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); rt.memory().store8(G[2] + 0x11Cu, 0u); }
void func_08893244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x30u, G[5]); put(rt, G[4] + 0x34u, G[6]); }
void func_08B2933C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x24u, G[7]); put(rt, G[4] + 0x20u, G[6]); }
void func_0889362C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; put(rt, G[4] + 0x2Cu, 1u); }
void func_088CF0E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; put(rt, G[4] + 0x98u, 1u); }
void func_0891A7CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x18u, bits_of(ctx.fpr[13])); put(rt, G[4] + 0x14u, bits_of(ctx.fpr[12])); }
void func_08946710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; rt.memory().store16(G[4], static_cast<std::uint16_t>(G[5])); }
void func_0894671C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; put(rt, G[4] + 4u, G[5]); }
void func_08946728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; put(rt, G[4] + 8u, G[5]); }
void func_0894CCAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CF2F08u); put(rt, 0x08CF2F08u, 0u); }
void func_08A38BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x20u); put(rt, G[4] + 0x2Cu, bits_of(ctx.fpr[0])); }
void func_08AC3280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0xCu, bits_of(ctx.fpr[12])); }
void func_08ACA300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; put(rt, G[4] + 0x220u, 0u); }
void func_08ACA3C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; put(rt, G[4], G[5]); }
void func_08ACA3D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; put(rt, G[4] + 4u, 0u); }
void func_08AF4E9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rt.memory().store8(G[4] + 0x425u, 0u); rt.memory().store8(G[4] + 0x424u, 0u); }
void func_08B093C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CE0A24u); put(rt, 0x08CE0A24u, G[5]); }
void func_08B1BC8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); put(rt, G[4] + 4u, G[2]); }
void func_08B1E2A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D68AE0u); rt.memory().store8(0x08D68AE0u, 0u); }
void func_08B61528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rt.memory().store8(G[4] + 4u, 0u); put(rt, G[4], 0u); }
void func_08B6202C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CE0E04u); put(rt, 0x08CE0E04u, 0u); }

// Getters and tests.
void func_088E77B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = positive(word(rt, G[4] + 0x14u)) ? 1u : 0u; }
void func_088EF918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = positive(sext(rt.memory().load8(G[4] + 0xCBu), 8u)) ? 1u : 0u; }
void func_088FDA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = __builtin_bswap32(word(rt, G[4] + 0x10u)); }  // wsbw
void func_08910C18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDD8A0u); ctx.fpr[0] = float_at(rt, 0x08CDD8A0u); }
void func_08910C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDD89Cu); ctx.fpr[0] = float_at(rt, 0x08CDD89Cu); }
void func_08910C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDD898u); ctx.fpr[0] = float_at(rt, 0x08CDD898u); }
void func_08910C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDD894u); ctx.fpr[0] = float_at(rt, 0x08CDD894u); }
void func_089316D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = rt.memory().load16(G[3] + 0x58u); }
void func_08931970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[4] + 0x14u); }
void func_08950770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = rt.memory().load8(G[4] + 0x13u) != 0u ? 1u : 0u; }
void func_0895DC6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CDE234u); G[2] = rt.memory().load8(0x08CDE234u); }
void func_0899D044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4] + 0x13Cu) >> 1) & 1u; }
void func_0899D3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x13Cu) & 1u; }
void func_08A76DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0u; G[3] = 0u; }
void func_08A76DEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0u; G[3] = 0u; }
void func_08A76E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 0u; G[3] = 0u; }
void func_08AC2230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (word(rt, G[4] + 0x20u) >> 1) & 1u; }
void func_08AC26BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = rt.memory().load16(G[3] + 4u); }
void func_08AE4BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4] != 0u ? G[4] : G[5]; }  // movn
void func_08AF7FEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(rt.memory().load16(G[4] + 0xEu), 16u) >> 31; }
void func_08B1F4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); }
void func_08B1F4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); }
void func_08B47B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4] + 0x18u + G[5]; }
void func_08B47BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = rt.memory().load16(G[4]) + 0x1Cu; }

#undef G

} // namespace hot_batch_167
} // namespace gt::game

#define HOT167(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_167::name, gt::ReturnKind::kind)
HOT167(0x0880456Cu, func_0880456C, Word);
// HOT167(0x0880B774u, func_0880B774, Word);  // no comparable synthetic case yet
HOT167(0x0885E164u, func_0885E164, Word);
HOT167(0x0885EC8Cu, func_0885EC8C, Word);
// HOT167(0x08869230u, func_08869230, Word);  // no comparable synthetic case yet
HOT167(0x088852C4u, func_088852C4, Word);
HOT167(0x088852D0u, func_088852D0, Word);
// HOT167(0x08892468u, func_08892468, Word);  // no comparable synthetic case yet
HOT167(0x088A5814u, func_088A5814, Word);
HOT167(0x088D47F4u, func_088D47F4, Word);
HOT167(0x088D4810u, func_088D4810, Word);
HOT167(0x0892A68Cu, func_0892A68C, Word);
HOT167(0x0892B74Cu, func_0892B74C, Word);
HOT167(0x089301B8u, func_089301B8, Word);
HOT167(0x0894020Cu, func_0894020C, Word);
HOT167(0x089A6A7Cu, func_089A6A7C, Word);
// HOT167(0x089B17ACu, func_089B17AC, Word);  // no comparable synthetic case yet
HOT167(0x089C1AD4u, func_089C1AD4, Word);
HOT167(0x089E0270u, func_089E0270, Word);
HOT167(0x089E027Cu, func_089E027C, Word);
HOT167(0x08A1B3B8u, func_08A1B3B8, Word);
HOT167(0x08AB3DF8u, func_08AB3DF8, Word);
HOT167(0x08ABF7D4u, func_08ABF7D4, Word);
// HOT167(0x08ABFA0Cu, func_08ABFA0C, Word);  // no comparable synthetic case yet
// HOT167(0x08ABFE64u, func_08ABFE64, Word);  // no comparable synthetic case yet
HOT167(0x08AFEA88u, func_08AFEA88, Word);
HOT167(0x08B00764u, func_08B00764, Word);
// HOT167(0x08B0DF0Cu, func_08B0DF0C, Word);  // no comparable synthetic case yet
HOT167(0x08B0F82Cu, func_08B0F82C, Word);
HOT167(0x08B2B3ECu, func_08B2B3EC, Word);
HOT167(0x08B35E30u, func_08B35E30, Word);
HOT167(0x08B35EB8u, func_08B35EB8, Word);
HOT167(0x08B48A68u, func_08B48A68, Word);
HOT167(0x08B4B858u, func_08B4B858, Word);
HOT167(0x08B59C90u, func_08B59C90, Word);
HOT167(0x08B76904u, func_08B76904, Word);
HOT167(0x08C54D74u, func_08C54D74, Word);
HOT167(0x08C54D80u, func_08C54D80, Word);
HOT167(0x08C73718u, func_08C73718, Word);
// HOT167(0x08C75504u, func_08C75504, Word);  // no comparable synthetic case yet
HOT167(0x08C7631Cu, func_08C7631C, Word);
HOT167(0x0885F460u, func_0885F460, Word);
HOT167(0x088E4120u, func_088E4120, Word);
HOT167(0x089178A0u, func_089178A0, Word);
HOT167(0x08B09918u, func_08B09918, Word);
HOT167(0x08893244u, func_08893244, Word);
HOT167(0x08B2933Cu, func_08B2933C, Word);
HOT167(0x0889362Cu, func_0889362C, Word);
HOT167(0x088CF0E0u, func_088CF0E0, Word);
HOT167(0x0891A7CCu, func_0891A7CC, Word);
HOT167(0x08946710u, func_08946710, Word);
HOT167(0x0894671Cu, func_0894671C, Word);
HOT167(0x08946728u, func_08946728, Word);
HOT167(0x0894CCACu, func_0894CCAC, Word);
HOT167(0x08A38BD4u, func_08A38BD4, Word);
HOT167(0x08AC3280u, func_08AC3280, Word);
HOT167(0x08ACA300u, func_08ACA300, Word);
HOT167(0x08ACA3C8u, func_08ACA3C8, Word);
HOT167(0x08ACA3D4u, func_08ACA3D4, Word);
HOT167(0x08AF4E9Cu, func_08AF4E9C, Word);
HOT167(0x08B093C8u, func_08B093C8, Word);
HOT167(0x08B1BC8Cu, func_08B1BC8C, Word);
HOT167(0x08B1E2A8u, func_08B1E2A8, Word);
HOT167(0x08B61528u, func_08B61528, Word);
HOT167(0x08B6202Cu, func_08B6202C, Word);
HOT167(0x088E77B4u, func_088E77B4, Word);
HOT167(0x088EF918u, func_088EF918, Word);
HOT167(0x088FDA54u, func_088FDA54, Word);
HOT167(0x08910C18u, func_08910C18, Float);
HOT167(0x08910C30u, func_08910C30, Float);
HOT167(0x08910C48u, func_08910C48, Float);
HOT167(0x08910C60u, func_08910C60, Float);
HOT167(0x089316D0u, func_089316D0, Word);
HOT167(0x08931970u, func_08931970, DoubleWord);
HOT167(0x08950770u, func_08950770, Word);
HOT167(0x0895DC6Cu, func_0895DC6C, Word);
HOT167(0x0899D044u, func_0899D044, Word);
HOT167(0x0899D3CCu, func_0899D3CC, Word);
HOT167(0x08A76DC8u, func_08A76DC8, DoubleWord);
HOT167(0x08A76DECu, func_08A76DEC, DoubleWord);
HOT167(0x08A76E00u, func_08A76E00, DoubleWord);
HOT167(0x08AC2230u, func_08AC2230, Word);
HOT167(0x08AC26BCu, func_08AC26BC, Word);
HOT167(0x08AE4BA4u, func_08AE4BA4, Word);
HOT167(0x08AF7FECu, func_08AF7FEC, Word);
HOT167(0x08B1F4DCu, func_08B1F4DC, Word);
HOT167(0x08B1F4E8u, func_08B1F4E8, Word);
HOT167(0x08B47B48u, func_08B47B48, Word);
HOT167(0x08B47BB4u, func_08B47BB4, Word);
