// Hot batch 259: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (tail calls with an adjusted argument, delay loops in closed form, float setters that raise a dirty flag and a
// scaled truncation).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_259 {
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

void func_0889367C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[4] = word(rt, G[4]); jump(ctx, 0x08893258u); } }
void func_08896EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = G[4] + 8u; jump(ctx, 0x08896ED8u); } }
void func_088B18B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { jump(ctx, 0x088B18B0u); } }
void func_089260F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[4] = word(rt, G[4] + 0x1E0u); jump(ctx, 0x0892779Cu); } }
void func_089272F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = G[4] + 0x1FD4u; jump(ctx, 0x08925BA4u); } }
void func_08AC3384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[4] = word(rt, G[4]); jump(ctx, 0x08AC328Cu); } }
void func_08B097A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[5] = G[5] & 0xFFu; jump(ctx, 0x08B09520u); } }
void func_08B1BE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = G[5]; jump(ctx, 0x08B1BE28u); } }
void func_08B2DBE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = 6u; jump(ctx, 0x08B2DBD8u); } }
void func_08C53F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = G[4] + 0x24u; jump(ctx, 0x08B6166Cu); } }
void func_08B64344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = 0x08D70000u; G[4] = word(rt, 0x08D721B8u); G[5] = 1u; jump(ctx, 0x08C7AAC0u); } }
void func_089961CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x20u, 0u); } }
void func_08AC69F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[1] = 0x472C4400u; F[0] = std::bit_cast<float>(G[1]); F[12] = F[12] * F[0]; F[1] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[12]))); put(rt, G[4] + 0x5A4u, std::bit_cast<std::uint32_t>(F[1])); } }
void func_08ACCD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5]; G[7] = word(rt, G[7]); G[5] = word(rt, G[5] + 0xB8u); G[4] = word(rt, G[2] + 0xB0u); jump(ctx, 0x08C7AD78u); } }
void func_088E89CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[4]; G[3] = 1u; store8(G[4] + 0x14u, 1u); G[4] = G[4] + 0x30u; store8(G[2] + 0x13u, 1u); jump(ctx, 0x088D08B0u); } }
void func_08C01DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { const bool same = G[4] == G[5]; G[4] = G[5] - G[4]; if (!same) G[2] = G[4] + 1u; } }
void func_08C249F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { const bool same = G[4] == G[5]; G[4] = G[5] - G[4]; if (!same) G[2] = G[4] + 1u; } }
void func_08C24A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { const bool same = G[4] == G[5]; G[4] = G[5] - G[4]; if (!same) G[2] = G[4] + 1u; } }
void func_08965BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[0] = float_at(rt, G[4] + 0x70u); if (!(F[0] == F[12])) { G[2] = word(rt, G[4] + 0xE4u); put_float(rt, G[4] + 0x70u, F[12]); G[2] = G[2] | 4u; put(rt, G[4] + 0xE4u, G[2]); } } }
void func_08965C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[0] = float_at(rt, G[4] + 0x74u); if (!(F[0] == F[12])) { G[2] = word(rt, G[4] + 0xE4u); put_float(rt, G[4] + 0x74u, F[12]); G[2] = G[2] | 4u; put(rt, G[4] + 0xE4u, G[2]); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_259
} // namespace gt::game

#define HOT259(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_259::name, gt::ReturnKind::kind)
HOT259(0x0889367Cu, func_0889367C, Word);
HOT259(0x08896EE0u, func_08896EE0, Word);
HOT259(0x088B18B8u, func_088B18B8, Word);
HOT259(0x089260F4u, func_089260F4, Word);
HOT259(0x089272F4u, func_089272F4, Word);
HOT259(0x08AC3384u, func_08AC3384, Word);
HOT259(0x08B097A4u, func_08B097A4, Word);
HOT259(0x08B1BE30u, func_08B1BE30, Word);
HOT259(0x08B2DBE8u, func_08B2DBE8, Word);
HOT259(0x08C53F8Cu, func_08C53F8C, Word);
HOT259(0x08B64344u, func_08B64344, Word);
HOT259(0x089961CCu, func_089961CC, Word);
HOT259(0x08AC69F4u, func_08AC69F4, Word);
// HOT259(0x08ACCD54u, func_08ACCD54, Word);  // no comparable synthetic case yet
HOT259(0x088E89CCu, func_088E89CC, Word);
HOT259(0x08C01DDCu, func_08C01DDC, Word);
HOT259(0x08C249F8u, func_08C249F8, Word);
HOT259(0x08C24A18u, func_08C24A18, Word);
HOT259(0x08965BE8u, func_08965BE8, Word);
HOT259(0x08965C14u, func_08965C14, Word);
