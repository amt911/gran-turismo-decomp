// Hot batch 296: seventy-six and seventy-seven-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (a tree copy, an insertion sort of strings, script bridges and a state setup).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_296 {
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

void func_08C27F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x8u), G[18]); G[20] = G[4]; G[16] = G[5]; G[17] = G[6]; G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[19] = G[2]; G[4] = G[2] + 0x10u; G[5] = G[16] + 0x10u; if (G[4] != 0u) { call_here(rt, ctx, 0x08A6972Cu); G[4] = G[19] + 0x14u; G[5] = G[16] + 0x14u; call_here(rt, ctx, 0x08A5D424u); } put(rt, G[19] + 0xCu, 0u); G[2] = word(rt, G[16]); G[5] = word(rt, G[16] + 0xCu); put(rt, G[19] + 4u, G[17]); put(rt, G[19], G[2]); put(rt, G[19] + 8u, 0u); if (G[5] != 0u) { G[4] = G[20]; G[6] = G[19]; call_here(rt, ctx, 0x08C27F70u); put(rt, G[19] + 0xCu, G[2]); } G[17] = word(rt, G[16] + 8u); G[18] = G[19]; while (G[17] != 0u) { G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[4] = G[2] + 0x10u; G[5] = G[17] + 0x10u; if (G[4] != 0u) { call_here(rt, ctx, 0x08A6972Cu); G[4] = G[16] + 0x14u; G[5] = G[17] + 0x14u; call_here(rt, ctx, 0x08A5D424u); } put(rt, G[16] + 0xCu, 0u); G[4] = G[20]; G[6] = G[16]; G[2] = word(rt, G[17]); G[3] = word(rt, G[17] + 0xCu); put(rt, G[16] + 4u, G[18]); G[5] = G[3]; put(rt, G[16] + 8u, 0u); put(rt, G[16], G[2]); put(rt, G[18] + 8u, G[16]); G[18] = G[16]; if (G[3] != 0u) { call_here(rt, ctx, 0x08C27F70u); put(rt, G[16] + 0xCu, G[2]); } G[2] = word(rt, G[17] + 8u); G[17] = G[2]; } G[2] = G[19]; G[20] = word(rt, frame.at(0x10u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_08C2E7C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[30] = G[5]; G[22] = G[6]; G[21] = G[4]; if (G[5] != G[4]) { G[5] = G[4] + 4u; G[18] = G[5]; if (G[30] != G[5]) { G[23] = frame.at(4u); do { G[4] = G[23]; call_here(rt, ctx, 0x08A76ABCu); G[3] = word(rt, G[22]); G[4] = G[22]; G[5] = G[23]; G[2] = word(rt, G[3] + 8u); G[6] = G[21]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[2] = G[18] - G[21]; G[19] = sra(G[2], 2); G[20] = G[18] + 4u; if (static_cast<std::int32_t>(G[19]) > 0) { G[16] = G[20]; G[17] = 0u; G[16] -= 4u; while (true) { G[18] -= 4u; G[17] += 1u; G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, G[18]); call_here(rt, ctx, 0x08A5BAD8u); } if (G[17] == G[19]) break; G[16] -= 4u; } } G[18] = G[20]; if (G[21] != G[23]) { G[5] = word(rt, frame.at(4u)); G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } } else { G[4] = frame.at(0u); G[5] = G[23]; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[18]; G[5] = frame.at(0u); G[6] = G[22]; call_here(rt, ctx, 0x08C2E724u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[18] += 4u; } G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[18]; } while (G[30] != G[18]); } } G[30] = word(rt, frame.at(0x30u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[23] = word(rt, frame.at(0x2Cu)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08826A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[21] = G[4]; G[18] = G[7]; if (G[6] == 0u) { G[17] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[17]; G[19] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x0885BBF0u); G[17] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[20] = frame.at(4u); G[5] = G[18] + 4u; G[4] = G[20]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A184u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; G[5] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x0885C978u); } G[5] = ~G[17]; G[5] >>= 31; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0882D9F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[4]; G[20] = G[7]; G[19] = G[6]; if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(4u); G[4] = G[17]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[17]; G[17] = frame.at(8u); G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x088A79ACu); G[16] = word(rt, G[20]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[6] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x08861C2Cu); G[2] = static_cast<std::int32_t>(G[19]) < 2 ? 1u : 0u; G[6] = 0u; if (G[2] == 0u) { G[16] = word(rt, G[20] + 4u); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x08857EE4u); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x088A7548u); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0882E25C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[4]; G[20] = G[7]; G[19] = G[6]; if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(4u); G[4] = G[17]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[17]; G[17] = frame.at(8u); G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x088A79ACu); G[16] = word(rt, G[20]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[6] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x0885FF1Cu); G[2] = static_cast<std::int32_t>(G[19]) < 2 ? 1u : 0u; G[6] = 0u; if (G[2] == 0u) { G[16] = word(rt, G[20] + 4u); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x08857EE4u); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x088A7548u); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08892544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[18] = G[4]; call_here(rt, ctx, 0x08892474u); G[4] = G[18]; if (G[2] == 0u) { call_here(rt, ctx, 0x08891290u); G[3] = 0x48190000u; if (G[2] != 0u) { G[2] = word(rt, G[18] + 0x74u); G[3] = 0x481907FFu; G[2] <<= 2; G[2] += G[18]; G[5] = word(rt, G[2] + 0x50u); G[4] = G[18]; if (G[5] != G[3]) { call_here(rt, ctx, 0x0889144Cu); G[4] = word(rt, G[18] + 0xCu); G[5] = word(rt, G[18] + 0x1Cu); call_here(rt, ctx, 0x088984D4u); G[3] = word(rt, G[18] + 0x74u); put(rt, G[18] + 0x28u, G[2]); G[3] <<= 2; G[3] += G[18]; G[6] = word(rt, G[3] + 0x38u); G[4] = word(rt, G[3] + 0x44u); G[5] = word(rt, G[3] + 0x2Cu); call_here(rt, ctx, 0x08890288u); G[3] = word(rt, G[18] + 0x74u); G[5] = 0u; G[17] = word(rt, G[18] + 0x28u); G[3] <<= 2; G[3] += G[18]; G[4] = word(rt, G[3] + 0x44u); call_here(rt, ctx, 0x08890C5Cu); G[16] = G[2]; G[2] = word(rt, G[18] + 0x74u); G[5] = 0u; G[2] <<= 2; G[2] += G[18]; G[4] = word(rt, G[2] + 0x44u); call_here(rt, ctx, 0x08890B20u); G[6] = G[2]; G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08896070u); G[2] = word(rt, G[18] + 0x74u); G[16] = word(rt, G[18]); G[2] <<= 2; G[2] += G[18]; G[4] = word(rt, G[2] + 0x44u); call_here(rt, ctx, 0x0888F548u); G[7] = word(rt, G[18] + 0x1Cu); G[8] = word(rt, G[18] + 4u); G[9] = word(rt, G[18] + 0xCu); G[4] = G[16]; G[5] = G[2]; G[6] = 0u; call_here(rt, ctx, 0x08892468u); G[4] = word(rt, G[18] + 0x28u); tail = true; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x088963C4u); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_296
} // namespace gt::game

#define HOT296(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_296::name, gt::ReturnKind::kind)
// HOT296(0x08C27F70u, func_08C27F70, Word);  // no comparable synthetic case yet
HOT296(0x08C2E7C4u, func_08C2E7C4, Word);
HOT296(0x08826A4Cu, func_08826A4C, Word);
HOT296(0x0882D9F8u, func_0882D9F8, Word);
HOT296(0x0882E25Cu, func_0882E25C, Word);
HOT296(0x08892544u, func_08892544, Word);
