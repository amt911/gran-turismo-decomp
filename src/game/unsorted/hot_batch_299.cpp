// Hot batch 299: seventy-seven and seventy-eight-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (script bridges, a millisecond setter, list erasure and a deferred call queue).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_299 {
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

void func_0881829C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 4 ? 1u : 0u; G[17] = G[7]; if (G[6] == 0u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08817FCCu); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08C98D6Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08817A54u); G[4] = frame.at(0u); G[21] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[20] = G[2]; if (G[16] == 0u) { G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); } G[19] = 0u; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = word(rt, G[17] + 8u); G[18] = G[2]; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x44u); call_here(rt, ctx, G[2]); G[19] = G[2]; } G[16] = word(rt, G[17] + 0xCu); if (G[16] == 0u) { G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = word(rt, G[21]); G[4] = G[21]; G[6] = G[20]; G[7] = G[18]; G[8] = G[19]; G[9] = G[2]; call_here(rt, ctx, 0x08818264u); } G[17] = word(rt, frame.at(0x14u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08827758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[21] = G[4]; G[18] = G[7]; if (G[6] == 0u) { G[17] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[17]; G[19] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x0885C2C0u); G[17] = G[2]; G[2] = 0xFFFFFFFFu; G[20] = frame.at(4u); if (G[17] != G[2]) { G[5] = G[18] + 4u; G[4] = G[20]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A184u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; G[5] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x0885C978u); } G[5] = G[17] + 1u; G[5] = G[5] != 0u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_088331F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[18] = G[4]; G[17] = G[5]; put_float(rt, frame.at(0x20u), F[20]); if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x54u); G[18] = frame.at(8u); call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = G[18]; F[20] = F[0]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[1] = 0x447A0000u; F[0] = 1000.0f; F[0] = F[20] * F[0]; F[1] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[0]))); put_float(rt, G[2] + 0x648u, F[1]); } else { G[17] = frame.at(4u); G[4] = G[17]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDECu); F[0] = float_at(rt, G[2] + 0x648u); G[1] = 0x447A0000u; F[12] = 1000.0f; G[4] = frame.at(0u); F[1] = static_cast<float>(std::bit_cast<std::int32_t>(F[0])); F[12] = F[1] / F[12]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(0u)); if (G[18] != frame.at(0u)) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } F[20] = float_at(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_08C2BD64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[22] = G[6]; G[20] = G[4]; G[2] = word(rt, G[4] + 0xCu); G[19] = G[5]; bool all = false; if (G[2] == G[5]) { G[23] = G[4] + 4u; all = G[6] == G[23]; } if (all) { G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C2BA70u); put(rt, G[20] + 0x10u, G[23]); put(rt, G[20] + 0x14u, 0u); put(rt, G[20] + 0xCu, G[23]); put(rt, G[20] + 8u, 0u); } else { G[2] = 0x08D70000u; if (G[22] != G[5]) { G[30] = 0x08D74FD0u; G[21] = G[5]; G[23] = G[20] + 4u; do { G[4] = G[19]; call_here(rt, ctx, 0x08B5F100u); G[4] = G[21]; G[5] = G[23]; G[19] = G[2]; call_here(rt, ctx, 0x08B5F444u); G[16] = G[2]; G[2] = word(rt, G[2] + 0x1Cu); G[5] = 0xFFFFFFFFu; G[21] = G[19]; G[17] = G[2] - 0xCu; G[4] = G[2] - 4u; G[18] = G[16] + 0x10u; if (G[17] != G[30]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[4] = G[16]; G[5] = 0x24u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[20] + 0x14u); G[2] -= 1u; put(rt, G[20] + 0x14u, G[2]); } while (G[22] != G[19]); } } G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08BD5F20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); G[20] = 0u; G[19] = G[7]; G[17] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4]); G[21] = word(rt, G[6]); G[18] = word(rt, G[6] + 4u); if (G[4] != 0u) { G[4] += 0x6Cu; call_here(rt, ctx, 0x08B34888u); G[4] = word(rt, G[17]); G[5] = G[16]; call_here(rt, ctx, 0x08BD5D54u); G[16] = G[2]; G[2] = word(rt, G[17]); G[2] += 0x98u; if (G[2] != G[16]) { G[2] = load8(G[16] + 0x14u); if (G[2] != 0u) { G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = G[2]; G[2] = 0x08CC55F8u; put(rt, G[3], G[2]); G[4] = G[16] + 0x18u; G[5] = G[3] + 4u; put(rt, G[3] + 8u, G[19]); put(rt, G[3] + 0x10u, G[18]); put(rt, G[3] + 0xCu, G[21]); put(rt, G[3] + 4u, 0u); G[2] = word(rt, G[4] + 4u); put(rt, G[4] + 4u, G[3]); if (G[2] != 0u) put(rt, G[2] + 4u, G[3]); else put(rt, G[16] + 0x18u, G[3]); put(rt, G[5], 0u); G[20] = 1u; G[2] = word(rt, G[4] + 8u); G[2] += 1u; put(rt, G[4] + 8u, G[2]); } else { G[2] = G[18] & 1u; G[3] = sra(G[18], 1); if (G[2] == 0u) { G[2] = sra(G[18], 1); G[4] = G[2] + G[19]; G[2] = G[21]; } else { G[4] = G[19] + G[3]; G[2] = word(rt, G[4]); G[2] += G[21]; G[2] = word(rt, G[2]); } G[20] = 1u; call_here(rt, ctx, G[2]); } } G[4] = G[17]; call_here(rt, ctx, 0x08BBCFA0u); } G[2] = G[20]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_299
} // namespace gt::game

#define HOT299(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_299::name, gt::ReturnKind::kind)
HOT299(0x0881829Cu, func_0881829C, Word);
HOT299(0x08827758u, func_08827758, Word);
// HOT299(0x088331F4u, func_088331F4, Word);  // no comparable synthetic case yet
HOT299(0x08C2BD64u, func_08C2BD64, Word);
HOT299(0x08BD5F20u, func_08BD5F20, Word);
