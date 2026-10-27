// Hot batch 172: eight-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, chain walks, small float helpers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_172 {
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

void func_0885D52C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x16u); G[5] = G[4] + 0x50u; G[4] += 0x120u; if (G[2] == 0u) return; jump(ctx, 0x088A6C18u); }
void func_0885E144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; G[5] = G[6]; call_with(rt, ctx, 0x08891124u, 0x10u); }
void func_0885EEC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x3F80u; call_with(rt, ctx, 0x08891838u, 0x10u); }
void func_0885EEEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x3F58u; call_with(rt, ctx, 0x08865F44u, 0x10u); }
void func_0885EF24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x3F58u; call_with(rt, ctx, 0x08865F60u, 0x10u); }
void func_0885FC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0xFFFFFFFFu; G[4] = word(rt, G[4]); call_with(rt, ctx, 0x0885FAA4u, 0x10u); }
void func_08860FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0xFFFFFFFFu; G[4] = word(rt, G[4]); call_with(rt, ctx, 0x08860C80u, 0x10u); }
void func_0886A9C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (G[5] << 2) + G[4]; G[5] = G[5] < 8u ? 1u : 0u; G[3] = 0u; if (G[5] != 0u) G[3] = word(rt, G[2] + 0x24u); G[2] = G[3]; }
void func_0886B180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u) + 0x88u; call_with(rt, ctx, 0x0887DF4Cu, 0x10u); }
void func_0886B1AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[25] = word(rt, G[2] + 0x74u); G[2] += 0x74u; if (G[25] == 0u) return; G[4] = word(rt, G[2] + 4u); jump(ctx, G[25]); }
void func_0886BC5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u); call_with(rt, ctx, 0x08B1FE38u, 0x10u); G[2] = word(rt, G[2]); }
void func_0887F7C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0x74u) + 4u; call_with(rt, ctx, 0x08886DE0u, 0x10u); }
void func_0887F870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0x74u) + 0x14u; call_with(rt, ctx, 0x0886F834u, 0x10u); }
void func_08884720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[3] = 0x89u; put(rt, G[4] + 0xCu, 0x89u); G[2] = (G[2] & ~6u) | 9u; put(rt, G[4] + 0x10u, G[2]); store16(G[4] + 0x12u, 0u); }
void func_08887074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC1860u; put(rt, G[4], G[2]); G[2] = 1u; put(rt, G[4] + 4u, G[5]); put(rt, G[4] + 8u, 1u); store8(G[4] + 0xCu, 0u); }
void func_0888A93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B35FD8u, 0x10u); G[2] = G[2] == 0u ? 1u : 0u; }
void func_0888A998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x1C35u); G[3] = 0xFFFFFFFFu; if (G[2] == 0u) { G[2] = 0x48190000u; G[3] = 0x481907FFu; } G[2] = (G[3] ^ G[5]) != 0u ? 1u : 0u; }
void func_0888FBD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = 1u; put(rt, frame.at(0u), 1u); call_here(rt, ctx, 0x0888F558u); }
void func_088915A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x14u; call_with(rt, ctx, 0x08891290u, 0x10u); }
void func_08891624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]) + 0x14u; call_with(rt, ctx, 0x08891290u, 0x10u); }
void func_08893260(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] << 2; G[3] = (((G[5] << 7) - G[2] + G[5]) << 3) + G[4]; G[2] = word(rt, G[3] + 0x41Cu); }
void func_08893280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[2] = (((G[5] << 7) - G[3] + G[5]) << 3) + G[4]; put(rt, G[2] + 0x41Cu, G[6]); }
void func_0889347C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[13] = ctx.fpr[12] - ctx.fpr[13]; G[4] += 0x10000u; G[2] = 1u; put(rt, G[4] - 0x7BA4u, bits_of(ctx.fpr[12])); store8(G[4] - 0x7BB8u, 1u); put(rt, G[4] - 0x7BB4u, bits_of(ctx.fpr[13])); }
void func_08898254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x20u; call_with(rt, ctx, 0x08896FB8u, 0x10u); G[2] += 0x80u; }
void func_088A6088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = sext(load8(G[2] + 8u), 8u); G[4] = (G[3] << 9) - (G[3] << 7) + G[2] + 0x38u; jump(ctx, 0x08936184u); }
void func_088A60A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = sext(load8(G[2] + 8u), 8u); G[4] = (G[3] << 9) - (G[3] << 7) + G[2] + 0x38u; jump(ctx, 0x08935C2Cu); }
void func_088A60C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = sext(load8(G[2] + 8u), 8u); G[4] = (G[3] << 9) - (G[3] << 7) + G[2] + 0x38u; jump(ctx, 0x08935CCCu); }
void func_088A6184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = sext(load8(G[2] + 8u), 8u); G[4] = (G[3] << 9) - (G[3] << 7) + G[2] + 0x38u; jump(ctx, 0x08936268u); }
void func_088A78B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[5] = word(rt, G[5]); call_with(rt, ctx, 0x088A76F8u, 0x10u); }
void func_088B13A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; if (G[5] == 0u) return; G[2] = word(rt, G[5]); G[25] = word(rt, G[2] + 4u); jump(ctx, G[25]); }
void func_088B1888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x088B3D20u, 0x10u); G[2] += 0x49u; }
void func_088C53FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; if (G[5] == 0u) return; G[2] = word(rt, G[5]); G[25] = word(rt, G[2] + 4u); jump(ctx, G[25]); }
void func_088C579C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 1u; G[4] = word(rt, G[4] + 4u); call_with(rt, ctx, 0x088C557Cu, 0x10u); }
void func_088C57FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 1u; G[4] = word(rt, G[4] + 4u); call_with(rt, ctx, 0x088C55A0u, 0x10u); }
void func_088D1DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[6] = word(rt, G[5] + 8u); G[3] = word(rt, G[5] + 4u); put(rt, G[4], G[2]); G[2] = G[4]; put(rt, G[4] + 4u, G[3]); put(rt, G[4] + 8u, G[6]); }
void func_088D9B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 5; G[2] = (((G[5] << 7) - G[3] - G[5]) << 3); G[6] = word(rt, G[4] + 0xC0u); G[2] = G[6] + G[2]; }
void func_088DAA64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 5; G[2] = (((G[5] << 7) - G[3] - G[5]) << 3); G[4] = word(rt, G[4] + 0xC0u) + G[2]; jump(ctx, 0x088E845Cu); }
void func_088E469C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[6] = 0u; G[7] = 0u; G[2] = word(rt, G[4]); G[8] = 1u; G[25] = word(rt, G[2] + 0x7Cu); G[9] = 0xFFFFFFFFu; jump(ctx, G[25]); }
void func_088E46BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[6] = 0u; G[7] = 0u; G[2] = word(rt, G[4]); G[8] = 0u; G[25] = word(rt, G[2] + 0x7Cu); G[9] = 0xFFFFFFFFu; jump(ctx, G[25]); }
void func_088E4C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5]; G[5] = word(rt, G[4]); G[3] = G[2] << 7; G[2] = (G[2] << 4) + G[3] + G[5] + 0x218u; }
void func_088EF7EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; if (G[5] == 0u) return; G[2] = word(rt, G[5]); G[25] = word(rt, G[2] + 4u); jump(ctx, G[25]); }
void func_088F12C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x088F1288u, 0x10u); G[2] = 0u; }
void func_088F5BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[6] + 0x104u, 0u); G[2] = word(rt, G[5]); put(rt, G[6] + 0x108u, G[2]); G[2] = word(rt, G[5]); if (G[2] != 0u) put(rt, G[2] + 0x104u, G[6]); put(rt, G[5], G[6]); }
void func_08907634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[4] = __builtin_bswap32(__builtin_bswap32(word(rt, G[4] + 4u)) + G[5]); put(rt, G[2] + 4u, G[4]); G[4] = __builtin_bswap32(G[4]); jump(ctx, 0x0890815Cu); }
void func_08925BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x30u); G[3] = word(rt, G[4] + 0x38u); G[5] = ~G[5]; G[2] &= G[5]; G[5] &= G[3]; put(rt, G[4] + 0x38u, G[5]); put(rt, G[4] + 0x30u, G[2]); }
void func_08928154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089277B4u, 0x10u); ctx.fpr[0] = float_at(rt, G[2] + 0x98u); }
void func_08928174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089277B4u, 0x10u); ctx.fpr[0] = float_at(rt, G[2] + 0x9Cu); }
void func_0892A958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4] + 4u; G[3] = 0u; if (static_cast<std::int32_t>(G[4]) >= 0) { G[3] = sra(G[2], 3u); G[2] = 0x1000u; if (static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3])) G[3] = G[2]; } G[2] = G[3]; }
void func_0892F848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = 0u; if (G[2] == G[6]) { G[2] = word(rt, G[4] + 0xCu) ^ G[7]; G[3] = G[2] == 0u ? 1u : 0u; } G[2] = G[3]; }
void func_08930844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 1u; G[5] = 0u; call_with(rt, ctx, 0x089306DCu, 0x10u); }
void func_089311CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089363FCu, 0x10u); G[2] += 0xCu; }
void func_08931ADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < 0x10 ? 1u : 0u; G[3] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[4] + 4u) + G[5]; G[3] = load8(G[2] + 0x78u); } G[2] = G[3]; }
void func_08932494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0x180u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] == 0u ? 1u : 0u; }
void func_08935F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08935D44u); G[2] = word(rt, frame.at(0x14u)); }
void func_089396D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[6] = word(rt, G[4] + 0x18u); G[2] = word(rt, G[3] + 0x18u) * G[5] + 8u + G[6]; }
void func_0894CF78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x0894CF40u, 0x10u); G[2] += 0x14ACu; }
void func_0894D2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4] ^ 4u; G[3] = 2u; if (G[4] != 2u) G[3] = G[5] != 0u ? 0u : 4u; G[2] = G[3]; }
void func_089525A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = load16(G[2] + 0x500u); store8(G[2] + 0x50Fu, 0u); store16(G[2] + 0x500u, 0u); store16(G[2] + 0x502u, G[3]); store8(G[2] + 0x50Eu, 0u); store8(G[2] + 0x50Au, 0u); }
void func_0895832C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0x20u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] == 0u ? 1u : 0u; }
void func_08959180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x0894CF40u, 0x10u); G[2] += 0x2028u; }
void func_0895F174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 0x10u); put(rt, G[5], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[2] + 0x14u); put(rt, G[5] + 4u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[2] + 0x18u); put(rt, G[5] + 8u, bits_of(ctx.fpr[0])); }
void func_089892EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t old = load8(G[4] + 0x1E9u); G[5] &= 0xFFu; G[2] = 1u; if (old == G[5]) return; store8(G[4] + 0x1B4u, 1u); store8(G[4] + 0x1E9u, G[5]); }
void func_0899FCC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x13Cu) & 8u; if (G[2] != 0u) return; jump(ctx, 0x0899FAB0u); }
void func_089C172C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; put(rt, G[7] + 0x78u, G[5]); G[4] = G[5]; G[8] = G[7] + 0x8Cu; G[5] = G[7] + 0x80u; G[6] = G[7] + 0x84u; G[7] += 0x88u; jump(ctx, 0x0896A35Cu); }
void func_089D842C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x8Cu); put(rt, G[4] + (G[2] == 0u ? 0x84u : 0x88u), bits_of(ctx.fpr[12])); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x198u); jump(ctx, G[25]); }
void func_089D844C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x8Cu); put(rt, G[4] + (G[2] == 0u ? 0x88u : 0x84u), bits_of(ctx.fpr[12])); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x198u); jump(ctx, G[25]); }
void func_089E0550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[6]); G[5] = (G[5] << 3) + 0x1E0u; G[2] = G[4] + G[5]; put(rt, G[2] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[6] + 4u); put(rt, G[2] + 0xCu, bits_of(ctx.fpr[0])); jump(ctx, 0x089E00E4u); }
void func_08A1C0FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B2D584u, 0x10u); G[2] = 0u; }
void func_08A34D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[0] = ctx.fpr[12]; ctx.fpr[12] = ctx.fpr[13]; ctx.fpr[13] = ctx.fpr[14]; G[4] = G[5]; ctx.fpr[14] = ctx.fpr[0]; G[5] = 0x7FFFFFFFu; jump(ctx, 0x08ABC4C4u); }
void func_08A3A480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x18u); G[3] = G[4]; G[4] = G[2]; if (G[2] == 0u) return; put(rt, G[3] + 0x18u, 0u); jump(ctx, 0x08ABCC48u); }
void func_08A4CFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[7] & 0xFFu; G[8] &= 0xFFu; G[7] = G[8]; if (G[2] == 0u) { G[6] = G[8]; jump(ctx, 0x08A4E58Cu); } else { jump(ctx, 0x08A4E634u); } }
void func_08A4DD4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xA4u); put(rt, G[4] + 0xA4u, 0u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08B35254u); }
void func_08AB2BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08AB2B50u, 0x10u); G[2] = sext(G[2], 8u); }
void func_08AB94A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD5F38u; put(rt, G[4], G[2]); store8(G[4] + 0xDu, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); store8(G[4] + 0xCu, 0u); }
void func_08AB9ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[6] != 0u) store8(G[6], 0u); if (G[7] != 0u) put(rt, G[7], 0u); if (G[8] != 0u) put(rt, G[8], 0u); }
void func_08ABA848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); store8(G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4], 0u); }
void func_08AC0880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[3] = G[5]; G[4] = 0x08CFFB80u; G[7] = G[6]; G[5] = G[2]; G[6] = G[3]; jump(ctx, 0x08AC2C8Cu); }
void func_08AC3340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CFFCB0u); G[2] = word(rt, 0x08CFFCB0u); G[4] = G[2]; if (G[2] == 0u) return; put(rt, 0x08CFFCB0u, 0u); jump(ctx, 0x08AC3324u); }
void func_08AC8D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D00000u; call_with(rt, ctx, 0x08AD343Cu, 0x10u); }
void func_08AD11C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 4u); ctx.fpr[1] = float_at(rt, G[4] + 0x18u); if (!(ctx.fpr[0] < ctx.fpr[1])) ctx.fpr[0] = ctx.fpr[1]; put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); }
void func_08AD3CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[5] == 0u) return; G[2] = word(rt, G[4]); G[5] += G[2]; if (G[2] == 0u) return; put(rt, G[4], G[5]); }
void func_08AD7314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[4], 0x3F800000u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); }
void func_08AD7CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; store8(G[4] + 5u, 0xFFu); put(rt, G[4] + 8u, 0x3F800000u); put(rt, G[4] + 0xCu, 0u); store8(G[4] + 4u, 0xFFu); }
void func_08AD9E2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08CDFB48u; call_with(rt, ctx, 0x08AD9E08u, 0x10u); }
void func_08ADF9DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; call_with(rt, ctx, 0x08ADF94Cu, 0x10u); }
void func_08ADFB5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; call_with(rt, ctx, 0x08ADFACCu, 0x10u); }
void func_08ADFC20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; call_with(rt, ctx, 0x08ADFB98u, 0x10u); }
void func_08AE30C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = sext(load16(G[4] + 0x1Cu), 16u); call_with(rt, ctx, 0x08ADA9A0u, 0x10u); G[2] = load8(G[2]); }
void func_08AE5BDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x34u); call_here(rt, ctx, G[2]); }
void func_08AE9EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D40A3Cu); G[5] = word(rt, 0x08D40A3Cu); G[3] = ((((G[4] << 3) - G[4]) << 3)) + G[5]; G[2] = sext(load8(G[3] + 4u), 8u); }
void func_08AFA478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x2Cu; call_with(rt, ctx, 0x08B529C8u, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08AFA498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x2Cu; call_with(rt, ctx, 0x08B52A48u, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08AFA6AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x2Cu; call_with(rt, ctx, 0x08B52D24u, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08B096BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0x24u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] == 0u ? 1u : 0u; }
void func_08B0B1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = __builtin_bswap32(word(rt, G[4])); G[5] += G[2]; G[3] = __builtin_bswap32(G[5]); if (G[2] != 0u) put(rt, G[4], G[3]); }
void func_08B0B34C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = 0u; G[4] = G[3]; if (G[3] == 0u) return; jump(ctx, 0x08B0B2E0u); }
void func_08B0B428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; call_with(rt, ctx, 0x08B0B34Cu, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08B0C358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); put(rt, G[4] + 8u, 0u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08B0EA58u); }
void func_08B1FDB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = 0u; if (G[3] == 0u) return; do { G[3] = word(rt, G[3] + 8u); ++G[2]; } while (G[3] != 0u); }
void func_08B28DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 8u); ctx.fpr[0] = float_at(rt, G[4]); G[2] = G[4]; ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[1]; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[13]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[12]; put(rt, G[4], bits_of(ctx.fpr[0])); }
void func_08B2C484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[6] + 4u); put(rt, G[5] + 4u, G[3]); G[2] = G[3] < word(rt, G[4] + 8u) ? 1u : 0u; if (G[2] != 0u) put(rt, G[3], G[5]); }
void func_08B3E714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x14u, 0u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); store8(G[4] + 8u, 0u); store8(G[4] + 9u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); }
void func_08B4889C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[5]; G[4] = G[5]; G[5] = G[6]; if (G[2] == 0u) { G[2] = 0u; return; } jump(ctx, 0x08B48848u); }
void func_08B51754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x3Eu; G[4] = word(rt, G[4] + 0xC20u); call_with(rt, ctx, 0x08B52148u, 0x10u); }
void func_08B56A10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CB61BCu); ctx.fpr[0] = float_at(rt, 0x08CB61B8u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, 0x08CB61BCu); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); }
void func_08B62178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, 0x08CE0E04u); G[2] = G[3] == 0u ? 0x08D721A8u : 0u; }
void func_08B6EE10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[8] = G[4]; G[4] = G[5] & 0x7FFFFFFFu; G[7] &= 0x80000000u; G[9] = G[4] | G[7]; G[2] = G[8]; G[3] = G[9]; }
void func_08B733F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[25] = word(rt, G[4] + 8u); G[5] = G[4]; G[4] = 1u; if (G[25] == 0u) return; jump(ctx, G[25]); }
void func_08B8A624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 4u); G[4] = G[6]; call_here(rt, ctx, G[2]); }
void func_08C524CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 8u); ctx.fpr[0] = 0.0f; if (ctx.fpr[1] <= ctx.fpr[0]) ctx.fpr[1] = std::bit_cast<float>(bits_of(ctx.fpr[1]) ^ 0x80000000u); ctx.fpr[0] = ctx.fpr[1]; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_172
} // namespace gt::game

#define HOT172(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_172::name, gt::ReturnKind::kind)
HOT172(0x0885D52Cu, func_0885D52C, Word);
HOT172(0x0885E144u, func_0885E144, Word);
HOT172(0x0885EEC0u, func_0885EEC0, Word);
HOT172(0x0885EEECu, func_0885EEEC, Word);
HOT172(0x0885EF24u, func_0885EF24, Word);
HOT172(0x0885FC2Cu, func_0885FC2C, Word);
// HOT172(0x08860FD8u, func_08860FD8, Word);  // no comparable synthetic case yet
HOT172(0x0886A9C0u, func_0886A9C0, Word);
HOT172(0x0886B180u, func_0886B180, Word);
HOT172(0x0886B1ACu, func_0886B1AC, Word);
HOT172(0x0886BC5Cu, func_0886BC5C, Word);
// HOT172(0x0887F7C8u, func_0887F7C8, Word);  // no comparable synthetic case yet
HOT172(0x0887F870u, func_0887F870, Word);
HOT172(0x08884720u, func_08884720, Word);
HOT172(0x08887074u, func_08887074, Word);
HOT172(0x0888A93Cu, func_0888A93C, Word);
HOT172(0x0888A998u, func_0888A998, Word);
// HOT172(0x0888FBD8u, func_0888FBD8, Word);  // no comparable synthetic case yet
HOT172(0x088915A8u, func_088915A8, Word);
HOT172(0x08891624u, func_08891624, Word);
HOT172(0x08893260u, func_08893260, Word);
HOT172(0x08893280u, func_08893280, Word);
HOT172(0x0889347Cu, func_0889347C, Word);
HOT172(0x08898254u, func_08898254, Word);
HOT172(0x088A6088u, func_088A6088, Word);
HOT172(0x088A60A8u, func_088A60A8, Word);
HOT172(0x088A60C8u, func_088A60C8, Word);
HOT172(0x088A6184u, func_088A6184, Word);
HOT172(0x088A78B4u, func_088A78B4, Word);
HOT172(0x088B13A4u, func_088B13A4, Word);
HOT172(0x088B1888u, func_088B1888, Word);
HOT172(0x088C53FCu, func_088C53FC, Word);
HOT172(0x088C579Cu, func_088C579C, Word);
HOT172(0x088C57FCu, func_088C57FC, Word);
HOT172(0x088D1DACu, func_088D1DAC, Word);
HOT172(0x088D9B9Cu, func_088D9B9C, Word);
HOT172(0x088DAA64u, func_088DAA64, Word);
HOT172(0x088E469Cu, func_088E469C, Word);
// HOT172(0x088E46BCu, func_088E46BC, Word);  // no comparable synthetic case yet
HOT172(0x088E4C08u, func_088E4C08, Word);
HOT172(0x088EF7ECu, func_088EF7EC, Word);
HOT172(0x088F12C0u, func_088F12C0, Word);
HOT172(0x088F5BB8u, func_088F5BB8, Word);
HOT172(0x08907634u, func_08907634, Word);
HOT172(0x08925BCCu, func_08925BCC, Word);
HOT172(0x08928154u, func_08928154, Float);
HOT172(0x08928174u, func_08928174, Float);
HOT172(0x0892A958u, func_0892A958, Word);
HOT172(0x0892F848u, func_0892F848, Word);
// HOT172(0x08930844u, func_08930844, Word);  // no comparable synthetic case yet
HOT172(0x089311CCu, func_089311CC, Word);
HOT172(0x08931ADCu, func_08931ADC, Word);
HOT172(0x08932494u, func_08932494, Word);
HOT172(0x08935F44u, func_08935F44, Word);
HOT172(0x089396D8u, func_089396D8, Word);
HOT172(0x0894CF78u, func_0894CF78, Word);
HOT172(0x0894D2C0u, func_0894D2C0, Word);
HOT172(0x089525A0u, func_089525A0, Word);
HOT172(0x0895832Cu, func_0895832C, Word);
HOT172(0x08959180u, func_08959180, Word);
HOT172(0x0895F174u, func_0895F174, Word);
HOT172(0x089892ECu, func_089892EC, Word);
HOT172(0x0899FCC8u, func_0899FCC8, Word);
HOT172(0x089C172Cu, func_089C172C, Word);
HOT172(0x089D842Cu, func_089D842C, Word);
HOT172(0x089D844Cu, func_089D844C, Word);
HOT172(0x089E0550u, func_089E0550, Word);
// HOT172(0x08A1C0FCu, func_08A1C0FC, Word);  // no comparable synthetic case yet
HOT172(0x08A34D78u, func_08A34D78, Word);
HOT172(0x08A3A480u, func_08A3A480, Word);
HOT172(0x08A4CFCCu, func_08A4CFCC, Word);
HOT172(0x08A4DD4Cu, func_08A4DD4C, Word);
HOT172(0x08AB2BCCu, func_08AB2BCC, Word);
HOT172(0x08AB94A4u, func_08AB94A4, Word);
HOT172(0x08AB9ED0u, func_08AB9ED0, Word);
HOT172(0x08ABA848u, func_08ABA848, Word);
HOT172(0x08AC0880u, func_08AC0880, Word);
// HOT172(0x08AC3340u, func_08AC3340, Word);  // no comparable synthetic case yet
HOT172(0x08AC8D84u, func_08AC8D84, Word);
HOT172(0x08AD11C4u, func_08AD11C4, Word);
HOT172(0x08AD3CDCu, func_08AD3CDC, Word);
HOT172(0x08AD7314u, func_08AD7314, Word);
HOT172(0x08AD7CA0u, func_08AD7CA0, Word);
HOT172(0x08AD9E2Cu, func_08AD9E2C, Word);
HOT172(0x08ADF9DCu, func_08ADF9DC, Word);
HOT172(0x08ADFB5Cu, func_08ADFB5C, Word);
HOT172(0x08ADFC20u, func_08ADFC20, Word);
// HOT172(0x08AE30C0u, func_08AE30C0, Word);  // no comparable synthetic case yet
HOT172(0x08AE5BDCu, func_08AE5BDC, Word);
HOT172(0x08AE9EB8u, func_08AE9EB8, Word);
HOT172(0x08AFA478u, func_08AFA478, Word);
// HOT172(0x08AFA498u, func_08AFA498, Word);  // no comparable synthetic case yet
// HOT172(0x08AFA6ACu, func_08AFA6AC, Word);  // no comparable synthetic case yet
HOT172(0x08B096BCu, func_08B096BC, Word);
HOT172(0x08B0B1D8u, func_08B0B1D8, Word);
// HOT172(0x08B0B34Cu, func_08B0B34C, Word);  // no comparable synthetic case yet
HOT172(0x08B0B428u, func_08B0B428, Word);
HOT172(0x08B0C358u, func_08B0C358, Word);
// HOT172(0x08B1FDB0u, func_08B1FDB0, Word);  // no comparable synthetic case yet
HOT172(0x08B28DE4u, func_08B28DE4, Word);
HOT172(0x08B2C484u, func_08B2C484, Word);
HOT172(0x08B3E714u, func_08B3E714, Word);
HOT172(0x08B4889Cu, func_08B4889C, Word);
HOT172(0x08B51754u, func_08B51754, Word);
HOT172(0x08B56A10u, func_08B56A10, Word);
HOT172(0x08B62178u, func_08B62178, Word);
HOT172(0x08B6EE10u, func_08B6EE10, DoubleWord);
HOT172(0x08B733F0u, func_08B733F0, Word);
HOT172(0x08B8A624u, func_08B8A624, Word);
HOT172(0x08C524CCu, func_08C524CC, Float);
