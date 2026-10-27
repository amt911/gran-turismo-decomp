// Hot batch 176: eleven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_176 {
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
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap). The
// callers rule out a zero divisor.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
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

void func_08B3E6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[2] + 0x58u); G[4] = word(rt, G[4] + 4u); call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] != 0u ? 1u : 0u; }
void func_08B3F478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x12u; call_here(rt, ctx, 0x08B3FA98u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B3E788u); }
void func_08B40B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[25] = word(rt, G[4] + 0x98Cu); G[3] = G[4]; G[4] = G[7]; if (G[25] == 0u) return; G[2] = 0x348u; if (G[8] != G[2]) return; G[5] = word(rt, G[3] + 0x988u); jump(ctx, G[25]); }
void func_08B48814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[8] = 0x08B4880Cu; G[4] = G[5]; G[7] = 0x10u; G[5] = G[2] + 0x10u; if (G[2] == 0u) { G[2] = 0u; return; } G[6] = word(rt, G[2] + 0xCu); jump(ctx, 0x08B77C14u); }
void func_08B52910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B59ADCu); put(rt, G[16], 0u); } G[16] = s0; }
void func_08B54858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[5] + 0x14u, 0u); put(rt, G[5] + 0xCu, 0u); put(rt, G[5] + 8u, 0u); put(rt, G[5] + 4u, 0u); put(rt, G[5], 0u); put(rt, G[5] + 0x10u, 0u); put(rt, G[5] + 0x18u, 0u); G[2] = word(rt, G[4] + 0x18u); put(rt, G[5] + 0x14u, G[2]); put(rt, G[4] + 0x18u, G[5]); }
void func_08B57D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08D6CE00u; G[6] = word(rt, G[5] + 0x5Cu); G[2] = word(rt, G[5] + 0x190u); G[3] = 0xCF000000u | (G[4] & 0xFFFFFFu); G[7] = G[6] + 4u; put(rt, G[6], G[3]); put(rt, G[5] + 0x190u, G[4]); put(rt, G[5] + 0x5Cu, G[7]); }
void func_08B60864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; G[5] = G[6]; call_with(rt, ctx, 0x08B7BB64u, 0x10u); G[3] = G[2] != 0u ? 1u : 0u; G[2] = sra(G[2], 30u) | G[3]; }
void func_08B608A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = G[9]; G[16] = G[4]; call_here(rt, ctx, 0x08C7464Cu); G[2] = G[16]; } G[16] = s0; }
void func_08B72004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[4]); G[4] = frame.at(0u); put(rt, frame.at(4u), G[5]); put(rt, frame.at(8u), G[6]); put(rt, frame.at(0xCu), G[7]); call_here(rt, ctx, 0x08B72564u); }
void func_08B72A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[4] -= 1u; call_here(rt, ctx, 0x08B7721Cu); G[3] = word(rt, frame.at(8u)); } if (G[2] == 0u) G[3] = 0u; G[2] = G[3]; }
void func_08B79048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; } G[16] = s0; jump(ctx, 0x08B78F3Cu); }
void func_08B79698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u) - 1u; put(rt, G[4] + 4u, G[2]); if (static_cast<std::int32_t>(G[2]) < 0) { jump(ctx, 0x08B7B298u); return; } G[3] = word(rt, G[4]); G[2] = load8(G[3]); G[3] += 1u; put(rt, G[4], G[3]); }
void func_08B7B898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = sext(load16(G[16] + 0xEu), 16u); } G[16] = s0; jump(ctx, 0x08B805F8u); }
void func_08B82318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (G[5] == 0u) return; G[2] = word(rt, G[5] + 4u); G[4] = word(rt, G[4] + 0x4Cu); G[2] = (G[2] << 2) + G[4]; G[3] = word(rt, G[2]); put(rt, G[5], G[3]); put(rt, G[2], G[5]); }
void func_08B82B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4] << 6; G[7] = 0x08D73CD4u; G[3] = G[5] + G[7]; G[6] = 0x80440000u; G[5] = G[4] < 8u ? 1u : 0u; G[2] = 0x80440005u; if (G[5] == 0u) return; G[2] = word(rt, G[3] + 0x1Cu); }
void func_08B83B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[6] == 0u) return; do { G[3] = load8(G[5]); G[7] += 1u; G[8] = G[7] < G[6] ? 1u : 0u; store8(G[4], G[3]); G[5] += 1u; G[4] += 1u; } while (G[8] != 0u); }
void func_08B8A6AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 2u; } else { G[2] = load16(G[4]); store16(G[3], G[2]); G[4] += 2u; } G[3] += 2u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08B8E87C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08B92974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; if (G[5] == G[4]) return; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); G[4] = G[7] + 8u; G[7] = G[4]; } while (G[5] != G[4]); }
void func_08BA5264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { if (G[2] == 0u) { G[4] += 4u; } else { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); G[4] += 4u; } G[2] += 4u; } while (G[5] != G[4]); } }
void func_08BB0DF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (;;) { if (G[4] == G[5]) { G[2] = G[6]; return; } G[3] = word(rt, G[4]); G[2] = word(rt, G[7]); G[4] += 4u; if (G[3] != G[2]) { put(rt, G[6], G[3]); G[6] += 4u; } } }
void func_08BB8A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (;;) { if (G[4] == G[5]) { G[2] = G[6]; return; } G[3] = sext(load16(G[4]), 16u); G[2] = word(rt, G[7]); G[4] += 2u; if (G[3] != G[2]) { store16(G[6], G[3]); G[6] += 2u; } } }
void func_08BDDA10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; if (G[5] == G[4]) return; do { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); G[4] = G[7] + 2u; G[7] = G[4]; } while (G[5] != G[4]); }
void func_08BDDA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; if (G[5] == G[4]) return; do { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); G[4] = G[7] + 2u; G[7] = G[4]; } while (G[5] != G[4]); }
void func_08BE91C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[5] = 0u; G[7] = 0u; G[8] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x08969D2Cu); }
void func_08BE91EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 0u; G[7] = 0u; G[8] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x08969D2Cu); }
void func_08BE94E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[5] = 0u; G[7] = 0u; G[8] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BE9510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 0u; G[7] = 0u; G[8] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BE9684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[8] = frame.at(0u); G[5] = 0u; G[6] = 0u; G[7] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BE96B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[7] = frame.at(0u); G[5] = 0u; G[6] = 0u; G[8] = 0u; put(rt, frame.at(0u), bits_of(ctx.fpr[12])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BE96DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[16]; } G[16] = s0; }
void func_08BEF114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { if (G[2] == 0u) { G[4] += 4u; } else { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); G[4] += 4u; } G[2] += 4u; } while (G[5] != G[4]); } }
void func_08BF0DA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; if (G[5] == G[4]) return; do { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[6] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[1])); G[4] = G[2] + 8u; G[2] = G[4]; } while (G[5] != G[4]); }
void func_08BF7AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08A76ABCu); G[2] = G[16]; } G[16] = s0; }
void func_08BF7BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08A0FD38u); G[2] = G[16]; } G[16] = s0; }
void func_08BF8DBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08BF91AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08C01E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const bool none = G[5] == 0u; G[5] += G[4]; if (none) return; do { if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); } G[4] += 1u; } while (G[5] != G[4]); }
void func_08C01F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const bool none = G[5] == 0u; G[5] += G[4]; if (none) return; do { if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); } G[4] += 1u; } while (G[5] != G[4]); }
void func_08C12194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x089FA280u); G[2] = G[16]; } G[16] = s0; }
void func_08C1A13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); ctx.fpr[0] = 0.0f; G[4] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 8u); G[25] = G[3]; jump(ctx, G[25]); }
void func_08C1A18C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x14u); G[2] = 0u; G[4] = G[3]; if (G[3] == 0u) return; G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 8u); G[25] = G[2]; jump(ctx, G[25]); }
void func_08C1FBD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08C31490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFFFu; G[16] = G[4]; call_here(rt, ctx, 0x08AA2BCCu); G[2] = G[16]; } G[16] = s0; }
void func_08C314D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08AA2B88u); G[2] = G[16]; } G[16] = s0; }
void func_08C31550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = sext(G[5], 16u); G[16] = G[4]; call_here(rt, ctx, 0x08AA2028u); G[2] = G[16]; } G[16] = s0; }
void func_08C31598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = sext(G[5], 8u); G[16] = G[4]; call_here(rt, ctx, 0x08AA206Cu); G[2] = G[16]; } G[16] = s0; }
void func_08C31B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFFFu; G[16] = G[4]; call_here(rt, ctx, 0x08AA9360u); G[2] = G[16]; } G[16] = s0; }
void func_08C31B84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = sext(G[5], 16u); G[16] = G[4]; call_here(rt, ctx, 0x08AA93F0u); G[2] = G[16]; } G[16] = s0; }
void func_08C31C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08AA9480u); G[2] = G[16]; } G[16] = s0; }
void func_08C31C8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = sext(G[5], 8u); G[16] = G[4]; call_here(rt, ctx, 0x08AA9510u); G[2] = G[16]; } G[16] = s0; }
void func_08C31D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFu; G[16] = G[4]; call_here(rt, ctx, 0x08AA95A0u); G[2] = G[16]; } G[16] = s0; }
void func_08C37AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); GuestFrame frame(ctx, 0x20u); G[6] = 1u; store8(frame.at(0u), G[5]); G[2] = word(rt, G[3] + 0x108u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); }
void func_08C37C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); GuestFrame frame(ctx, 0x20u); G[6] = 1u; store8(frame.at(0u), G[5]); G[2] = word(rt, G[3] + 0x108u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); }
void func_08C49904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08C499C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 2u; } else { G[2] = load16(G[4]); store16(G[3], G[2]); G[4] += 2u; } G[3] += 2u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08C499FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { do { if (G[3] == 0u) { G[4] += 4u; } else { G[2] = word(rt, G[4]); put(rt, G[3], G[2]); G[4] += 4u; } G[3] += 4u; } while (G[5] != G[4]); } G[2] = G[3]; }
void func_08C4B014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; if (G[5] == G[4]) return; do { G[2] = load16(G[6]); G[3] = load16(G[6] + 2u); store16(G[4], G[2]); store16(G[4] + 2u, G[3]); G[4] = G[7] + 4u; G[7] = G[4]; } while (G[5] != G[4]); }
void func_08C6F530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x20u); call_here(rt, ctx, 0x08B79698u); put(rt, G[16] + 0x24u, G[2]); } G[16] = s0; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_176
} // namespace gt::game

#define HOT176(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_176::name, gt::ReturnKind::kind)
HOT176(0x08B3E6BCu, func_08B3E6BC, Word);
HOT176(0x08B3F478u, func_08B3F478, Word);
HOT176(0x08B40B7Cu, func_08B40B7C, Word);
HOT176(0x08B48814u, func_08B48814, Word);
HOT176(0x08B52910u, func_08B52910, Word);
HOT176(0x08B54858u, func_08B54858, Word);
HOT176(0x08B57D40u, func_08B57D40, Word);
HOT176(0x08B60864u, func_08B60864, Word);
HOT176(0x08B608A0u, func_08B608A0, Word);
HOT176(0x08B72004u, func_08B72004, Word);
HOT176(0x08B72A2Cu, func_08B72A2C, Word);
HOT176(0x08B79048u, func_08B79048, Word);
HOT176(0x08B79698u, func_08B79698, Word);
HOT176(0x08B7B898u, func_08B7B898, Word);
HOT176(0x08B82318u, func_08B82318, Word);
HOT176(0x08B82B28u, func_08B82B28, Word);
HOT176(0x08B83B24u, func_08B83B24, Word);
HOT176(0x08B8A6ACu, func_08B8A6AC, Word);
HOT176(0x08B8E87Cu, func_08B8E87C, Word);
HOT176(0x08B92974u, func_08B92974, Word);
HOT176(0x08BA5264u, func_08BA5264, Word);
HOT176(0x08BB0DF4u, func_08BB0DF4, Word);
HOT176(0x08BB8A38u, func_08BB8A38, Word);
HOT176(0x08BDDA10u, func_08BDDA10, Word);
HOT176(0x08BDDA74u, func_08BDDA74, Word);
HOT176(0x08BE91C0u, func_08BE91C0, Word);
HOT176(0x08BE91ECu, func_08BE91EC, Word);
HOT176(0x08BE94E4u, func_08BE94E4, Word);
HOT176(0x08BE9510u, func_08BE9510, Word);
HOT176(0x08BE9684u, func_08BE9684, Word);
HOT176(0x08BE96B0u, func_08BE96B0, Word);
// HOT176(0x08BE96DCu, func_08BE96DC, Word);  // no comparable synthetic case yet
HOT176(0x08BEF114u, func_08BEF114, Word);
HOT176(0x08BF0DA0u, func_08BF0DA0, Word);
// HOT176(0x08BF7AB0u, func_08BF7AB0, Word);  // no comparable synthetic case yet
// HOT176(0x08BF7BACu, func_08BF7BAC, Word);  // no comparable synthetic case yet
HOT176(0x08BF8DBCu, func_08BF8DBC, Word);
HOT176(0x08BF91ACu, func_08BF91AC, Word);
HOT176(0x08C01E78u, func_08C01E78, Word);
HOT176(0x08C01F54u, func_08C01F54, Word);
// HOT176(0x08C12194u, func_08C12194, Word);  // no comparable synthetic case yet
HOT176(0x08C1A13Cu, func_08C1A13C, Float);
HOT176(0x08C1A18Cu, func_08C1A18C, Word);
HOT176(0x08C1FBD4u, func_08C1FBD4, Word);
HOT176(0x08C31490u, func_08C31490, Word);
HOT176(0x08C314D8u, func_08C314D8, Word);
HOT176(0x08C31550u, func_08C31550, Word);
HOT176(0x08C31598u, func_08C31598, Word);
// HOT176(0x08C31B00u, func_08C31B00, Word);  // no comparable synthetic case yet
HOT176(0x08C31B84u, func_08C31B84, Word);
// HOT176(0x08C31C08u, func_08C31C08, Word);  // no comparable synthetic case yet
HOT176(0x08C31C8Cu, func_08C31C8C, Word);
HOT176(0x08C31D10u, func_08C31D10, Word);
HOT176(0x08C37AD4u, func_08C37AD4, Word);
HOT176(0x08C37C38u, func_08C37C38, Word);
HOT176(0x08C49904u, func_08C49904, Word);
HOT176(0x08C499C0u, func_08C499C0, Word);
HOT176(0x08C499FCu, func_08C499FC, Word);
HOT176(0x08C4B014u, func_08C4B014, Word);
HOT176(0x08C6F530u, func_08C6F530, Word);
