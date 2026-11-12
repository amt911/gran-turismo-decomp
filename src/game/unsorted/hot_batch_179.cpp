// Hot batch 179: fourteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_179 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load16(address); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Families of identical shape: each registration keeps its own thin function (and its own frame), the constants differ.
// Release the object a member points at (`release(member)`), then clear the member.
void release_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset, u32 release) {
    const u32 self = ctx.gpr[4];
    const u32 member = word(rt, self + offset);
    ctx.gpr[2] = member;
    if (member != 0u) {
        call(rt, ctx, release, {member});
        put(rt, self + offset, 0u);
    }
}
// Base constructor, then the vtable; v0 is left holding the vtable.
void construct_with_vtable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 base, u32 vtable) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, base, {self});
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}
// `0x08966174(self)`, then a signed half of the state at self + 0x80 (0 when the check fails).
void state_half(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    s32 value = 0;
    if (call(rt, ctx, 0x08966174u, {self}) != 0u) value = static_cast<std::int16_t>(half_at(rt, word(rt, self + 0x80u) + offset));
    ctx.gpr[2] = static_cast<u32>(value);
}
// `0x08A53224(self, &slot)` with the slot holding the global at `address`; returns self.
void report_global(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address, u32 slot) {
    const u32 self = ctx.gpr[4];
    put(rt, slot, word(rt, address));
    call(rt, ctx, 0x08A53224u, {self, slot});
    ctx.gpr[2] = self;
}

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

void func_08818264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); put(rt, frame.at(0u), G[5]); G[2] = 2u; G[5] = frame.at(0u); store8(frame.at(4u), 2u); store8(frame.at(5u), G[6]); store8(frame.at(6u), G[7]); store8(frame.at(7u), G[8]); put(rt, frame.at(8u), G[9]); call_here(rt, ctx, 0x08818168u); }
void func_0881A774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x30u, 0x08B35254u);
}
void func_088221F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    report_global(rt, ctx, 0x08CE2B10u, frame.at(0x0u));
}
void func_08824774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    report_global(rt, ctx, 0x08CE2B3Cu, frame.at(0x0u));
}
void func_08829490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    report_global(rt, ctx, 0x08CE2B8Cu, frame.at(0x0u));
}
void func_08854C74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x08A76ACCu, 0x08CBD238u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x14u, 0u);
}
void func_0885D210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[5] = word(rt, G[4] + 4u); G[6] = 0u; if (static_cast<std::int32_t>(G[5]) > 0) { G[4] = word(rt, G[4]); G[3] = 0u; do { G[2] = load8(G[4] + 0x15u); G[3] += 1u; G[4] += 0x188u; G[2] = G[2] < 1u ? 1u : 0u; G[6] += G[2]; } while (G[3] != G[5]); } G[2] = G[6]; }
void func_0886FA58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x0887E1E0u, 0x08CBF840u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0xCu, 0u);
}
void func_0886FDB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x0887E1E0u, 0x08CBF890u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0xCu, 0u);
}
void func_0887E4F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x0886AD04u, 0x08CC1020u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x70u, 0u);
}
void func_0887E750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x0886AD04u, 0x08CC1058u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x70u, 0u);
}
void func_0887E880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x0886AD04u, 0x08CC1090u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x70u, 0u);
}
void func_0887F37C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 0x20u); G[3] = 0u; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x74u); call_here(rt, ctx, 0x08883010u); G[3] = 1u; if (G[2] == 0u) G[3] = 0u; } G[2] = G[3]; }
void func_0888B29C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x1Bu; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(8u), 0u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); store8(frame.at(0x14u), 0u); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_0888FE08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 aligned = (__builtin_bswap32(word(rt, self + 0xCu)) + 0xFu) & ~0xFu;
    put(rt, out, __builtin_bswap32(word(rt, self)) - aligned);
    const u32 next = word(rt, self + 8u);
    put(rt, self, __builtin_bswap32(aligned));
    ctx.gpr[2] = aligned + __builtin_bswap32(next);
}
void func_0888FE40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 aligned = (__builtin_bswap32(word(rt, self + 0xCu)) + 0xFu) & ~0xFu;
    put(rt, out, __builtin_bswap32(word(rt, self)) - aligned);
    const u32 next = word(rt, self + 8u);
    put(rt, self, __builtin_bswap32(aligned));
    ctx.gpr[2] = aligned + __builtin_bswap32(next);
}
void func_088910E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; G[4] = word(rt, G[4] + 0x8Cu); call_here(rt, ctx, 0x088DAD00u); G[4] = word(rt, G[16] + 0x94u); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x20u); } G[16] = s0; jump(ctx, G[25]); }
void func_08891D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] &= 0xFFu; G[2] = 1u; if (G[5] != G[2]) call_here(rt, ctx, 0x088918F8u); else call_here(rt, ctx, 0x08891C5Cu); }
void func_08893160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0x10000u; put(rt, G[2] - 0x7BB4u, 0u); G[3] = G[4]; G[5] = 4u; put(rt, G[4] + 0x30u, 0u); put(rt, G[4] + 0x34u, 0u); store8(G[2] - 0x7BB8u, 0u); do { G[5] -= 1u; put(rt, G[3] + 0x41Cu, 0u); G[3] += 0x3E8u; } while (G[5] != 0u); }
void func_088936F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4]); G[3] = G[5] << 2; G[2] = (G[5] << 7) - G[3]; G[4] = sext(load8(G[6]), 8u); G[2] = (G[2] + G[5]) << 3; G[3] = 0x14Cu; ctx.lo = G[2]; const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * 0x14C; ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[2] = ctx.lo + G[6] + 0x38u; }
void func_08894E8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = __builtin_bswap32(G[5]); G[2] = word(rt, G[4]); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08894B60u); put(rt, G[2] + 0x18u, G[16]); } } G[16] = s0; }
void func_088BCA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = word(rt, G[4] + 0x24u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[1] = float_at(rt, G[3] + 0x520u); put(rt, G[2] + 0xA8u, G[1]); put(rt, G[2] + 0x34u, bits_of(ctx.fpr[1])); store8(G[2] + 0x95u, 0u); put(rt, G[2] + 0x24u, bits_of(ctx.fpr[1])); put(rt, G[2] + 0x28u, 0u); put(rt, G[2] + 0x2Cu, 0u); put(rt, G[2] + 0x30u, 0u); store8(G[2] + 0x3Au, 0u); }
void func_088C55C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] = 4u; call_here(rt, ctx, 0x088C541Cu); if (G[2] == 0u) { G[2] = 0u; return; } G[2] = word(rt, G[2]); G[3] = static_cast<std::int32_t>(G[2]) < 7 ? 1u : 0u; G[2] -= 6u; if (G[3] == 0u) return; G[2] = 0u; }
void func_088D08E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; store8(G[4] + 0x25u, 1u); G[6] &= 0xFFu; put(rt, G[4] + 0x74u, G[5]); if (G[6] != G[2]) return; G[2] = word(rt, G[4] + 0x7Cu); store8(G[4] + 0x27u, G[6]); G[2] = G[5] < G[2] ? 1u : 0u; put(rt, G[4] + 0x194u, G[5]); if (G[2] == 0u) return; put(rt, G[4] + 0x7Cu, G[5]); }
void func_088D1DCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[8] = G[2] ^ G[7]; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; G[3] = 0u; if (G[2] != 0u) { put(rt, G[4] + 8u, G[7]); put(rt, G[4], G[5]); put(rt, G[4] + 4u, G[6]); G[2] = G[3]; return; } G[3] = 1u; G[2] = 0xFFFFFFFFu; if (G[8] != 0u) G[3] = G[2]; G[2] = G[3]; }
void func_088D5FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08931014u); G[6] = G[2]; G[4] = G[16]; G[7] = G[3]; G[8] = 0u; } G[16] = s0; jump(ctx, 0x088D5ED0u); }
void func_088DA3EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[3] = 1u; G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E672Cu); G[3] = G[2]; } G[2] = G[3]; }
void func_088DA6A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[7] &= 0xFFu; G[2] = word(rt, G[4] + 0x134u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x94u); call_here(rt, ctx, G[3]); } } G[2] = 1u; }
void func_088DC180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[5] + G[4]; G[3] = sext(load8(G[8] + 0x58u), 8u); G[2] = ~G[7]; if (G[2] == 0u) G[7] = G[5]; if (G[3] == G[6]) return; G[2] = sext(load8(G[8] + 0x54u), 8u); G[4] += G[2]; if (G[7] != G[2]) { store8(G[4] + 0x54u, G[2]); store8(G[4] + 0x58u, 0u); store8(G[8] + 0x54u, G[7]); } store8(G[8] + 0x58u, G[6]); }
void func_088DC1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x30u); G[6] = G[4]; if (G[3] != G[5]) { G[3] = G[5]; put(rt, G[6] + 0x30u, G[5]); } G[2] = word(rt, G[6] + 0x130u); store8(G[2] + 1u, G[3]); G[3] = word(rt, G[6] + 0x640u); G[4] = G[3]; if (G[3] == 0u) return; G[5] = load8(G[6] + 0x30u); jump(ctx, 0x088E649Cu); }
void func_088E4EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = 3u; } if (G[2] == 0u) { G[2] = 0x5Au; return; } jump(ctx, 0x08B099A4u); }
void func_088E7228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = G[4]; G[5] = 0u; G[2] = word(rt, G[4] + 8u); G[4] = G[2]; if (G[2] != 0u) { G[5] = word(rt, G[3] + 4u); call_here(rt, ctx, 0x088DF13Cu); G[5] = word(rt, G[2] + 0x7Cu); } G[2] = G[5]; }
void func_088E750C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = G[4]; G[5] = 0u; G[2] = word(rt, G[4] + 8u); G[4] = G[2]; if (G[2] != 0u) { G[5] = word(rt, G[3] + 4u); call_here(rt, ctx, 0x088DD0E4u); G[5] = G[2]; } G[2] = G[5]; }
void func_088E7EDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2E4u); G[5] = sext(G[5], 8u); G[6] = sext(G[6], 8u); store8(G[4] + 0x2A1u, G[5]); store8(G[4] + 0x2A0u, G[6]); if (G[3] == 0u) return; G[2] = word(rt, G[4] + 0x1Cu); if (G[2] != 0u) return; store8(G[3] + 0x3Du, G[6]); G[2] = word(rt, G[4] + 0x2E4u); store8(G[2] + 0x3Eu, G[5]); }
void func_088E82AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x1Cu); G[3] = 9u; store8(G[4] + 0x12u, 0u); store8(G[4] + 0x2B7u, 0u); put(rt, G[4] + 8u, 9u); if (G[2] != 0u) return; G[2] = word(rt, G[4] + 0x2E4u); if (G[2] == 0u) return; store8(G[2] + 3u, 0u); G[2] = word(rt, G[4] + 0x2E4u); put(rt, G[2] + 0x38u, 9u); }
void func_088EF8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u);  call_here(rt, ctx, 0x088F0FF8u); G[16] = G[2]; call_here(rt, ctx, 0x088F0ED8u); G[16] = G[16] + G[2] + 0x18u; G[2] = G[16] & ~0xFu; } G[16] = s0; }
void func_088F1288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); G[5] = G[4] + 0x14u; G[6] = frame.at(0u); put(rt, G[4], 0u); G[2] = 0x08BB8D54u; G[4] += 4u; put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08BB8D60u); }
void func_088F40CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC3C58u; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); put(rt, G[16], G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08AF20E8u); }
void func_088F5C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[6] + 0x104u); G[2] = word(rt, G[6] + 0x108u); if (G[3] != 0u) put(rt, G[3] + 0x108u, G[2]); else put(rt, G[5], G[2]); G[3] = word(rt, G[6] + 0x108u); if (G[3] != 0u) { G[2] = word(rt, G[6] + 0x104u); put(rt, G[3] + 0x104u, G[2]); } put(rt, G[6] + 0x104u, 0u); put(rt, G[6] + 0x108u, 0u); }
void func_088F8A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = static_cast<std::int32_t>(G[5]) < 4 ? 1u : 0u; G[4] += 0x2Cu; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = sra(G[5], 5u) << 2; G[3] = 1u; G[4] += G[2]; G[3] = 1u << (G[5] & 31u); if (G[6] == 0u) return; G[2] = word(rt, G[4]) | G[3]; put(rt, G[4], G[2]); }
void func_088FDF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x4u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35254u); put(rt, G[16] + 0x4u, 0u); } } G[16] = s0; }
void func_0890599C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x0u, 0x08B59ADCu);
}
void func_08906304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[3] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[4] = float_at(rt, G[5] + 8u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[3]); ctx.fpr[1] = float_at(rt, G[4] + 8u); ctx.fpr[3] = float_at(rt, G[4] + 0xCu); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[4]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; }
void func_08915DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[6] = 0u; if (static_cast<std::int32_t>(G[2]) <= 0) return; G[3] = G[4]; do { G[2] = word(rt, G[3] + 4u); G[6] += 1u; G[2] += G[5]; put(rt, G[3] + 4u, G[2]); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(word(rt, G[4])) ? 1u : 0u; G[3] += 4u; } while (G[2] != 0u); }
void func_0891FF94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x1FCu); G[3] = (1u << (G[5] & 31u)) & 0x3C198u; G[5] = G[5] < 0x12u ? 1u : 0u; G[2] = 0u; if (G[5] != 0u && G[3] != 0u) return; G[2] = 1u; }
void func_08927A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[5] = G[16]; call_here(rt, ctx, 0x08927A0Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B507B8u); G[2] = G[16]; } G[16] = s0; }
void func_08929D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B35FD8u); G[3] = 0u; if (G[2] == 0u) G[3] = word(rt, G[16] + 4u); G[2] = G[3]; } G[16] = s0; }
void func_08931990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5] << 1; G[5] = static_cast<std::int32_t>(G[5]) < 0x10 ? 1u : 0u; G[2] = 0u; if (G[5] == 0u) return; G[3] = word(rt, G[4] + 4u) + G[6]; G[2] = load16(G[3] + 0x16u); G[4] = G[2] << 4; G[2] = (G[2] << 2) + G[4]; G[3] = G[2] << 2; G[2] += G[3]; }
void func_08931B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x18u, 0x08937B04u);
}
void func_08938F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x4u, 0x08B35254u);
}
void func_08938F50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x1Cu, 0x08B35254u);
}
void func_0893A104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 8u)) ? 1u : 0u; G[3] = 0u; if (G[2] == 0u || static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0u; return; } G[2] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[4] + 0xCu); ctx.lo = G[2]; const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[2] = ctx.lo; G[3] = G[2] + 4u; G[2] = G[3]; }
void func_0893A13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x0893A104u); G[5] = G[2]; G[3] = word(rt, G[2]); G[2] = 0x7FFFFFFFu; G[4] = G[3] + 1u; if (G[3] != G[2]) put(rt, G[5], G[4]); }
void func_0893A3A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 8u)) ? 1u : 0u; G[3] = 0u; if (G[2] == 0u || static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0u; return; } G[2] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[4] + 0xCu); ctx.lo = G[2]; const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[2] = ctx.lo; G[3] = G[2] + 8u; G[2] = G[3]; }
void func_0893A3DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 8u)) ? 1u : 0u; G[3] = 0u; if (G[2] == 0u || static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0u; return; } G[2] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[4] + 0xCu); ctx.lo = G[2]; const std::int64_t sum = static_cast<std::int64_t>((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) + static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(sum); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sum) >> 32); G[2] = ctx.lo; G[3] = G[2] + 0x10u; G[2] = G[3]; }
void func_08958AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08958904u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; if (G[2] != 0u) { G[2] = word(rt, G[16] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 0x778u); } } G[16] = s0; }
void func_08959F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 8u)) ? 1u : 0u; if (G[2] == 0u) return; call_here(rt, ctx, 0x08959180u); G[3] = 3u; put(rt, G[2] + 0xA0u, 3u); }
void func_08966ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    state_half(rt, ctx, 0x28u);
}
void func_08966F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    state_half(rt, ctx, 0x2Au);
}
void func_08966F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    state_half(rt, ctx, 0x2Cu);
}
void func_08966F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    state_half(rt, ctx, 0x2Eu);
}
void func_089671FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[1] = 0x3F000000u; ctx.fpr[0] = 0.5f; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x80u); ctx.fpr[0] = float_at(rt, G[2] + 0x20u); } } G[16] = s0; }
void func_08967234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[1] = 0x3F000000u; ctx.fpr[0] = 0.5f; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x80u); ctx.fpr[0] = float_at(rt, G[2] + 0x24u); } } G[16] = s0; }
void func_089775D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x1B0u); ctx.fpr[0] = std::bit_cast<float>(G[2]); G[3] = G[2] >> 1; G[4] = (G[2] & 1u) | G[3]; ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[2])); if (static_cast<std::int32_t>(G[2]) < 0) { ctx.fpr[0] = std::bit_cast<float>(G[4]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[4])); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[1]; } G[1] = 0x42700000u; ctx.fpr[0] = 60.0f; ctx.fpr[0] = ctx.fpr[1] / ctx.fpr[0]; }
void func_089893DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 member = word(rt, ctx.gpr[4] + 0x1E0u);
    {
        GuestFrame frame(ctx, 0x10u);
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
    }
    tail_call(ctx, 0x089CBF68u, {member});
}
void func_08989414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 member = word(rt, ctx.gpr[4] + 0x1E0u);
    {
        GuestFrame frame(ctx, 0x10u);
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
    }
    tail_call(ctx, 0x089CBF70u, {member});
}
void func_08A3533C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x18u); G[4] = 0x08CACEC8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A3A6FCu); }
void func_0899166C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x08A76ACCu, 0x08CCC620u);
    set_byte(rt, self + 0x18u, 0u);
    put(rt, self + 0x14u, 0u);
}
void func_089A0120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x13Cu); G[3] = G[2] | 0x10u; G[2] &= 8u; G[7] = 0u; if (G[2] != 0u) { put(rt, G[4] + 0x13Cu, G[3]); call_here(rt, ctx, 0x0899FD68u); G[7] = 2u; } G[2] = G[7]; }
void func_089CFC5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089CEFE8u); G[3] = G[2]; G[2] = word(rt, G[16] + 0x120u); ctx.fpr[0] = float_at(rt, G[3] + (G[2] != 0u ? 0x18u : 0x1Cu)); } G[16] = s0; }
void func_089CFC94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089CEFE8u); G[3] = G[2]; G[2] = word(rt, G[16] + 0x120u); ctx.fpr[0] = float_at(rt, G[3] + (G[2] != 0u ? 0x1Cu : 0x18u)); } G[16] = s0; }
void func_089F9CA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089F9BFCu); G[3] = word(rt, G[2]); G[2] = (G[16] << 4) - (G[16] << 2) + G[3]; } G[16] = s0; }
void func_08A12AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, 0x44u, 0x08B35254u);
}
void func_08A1845C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[7] = frame.at(4u); G[8] = frame.at(8u); G[9] = frame.at(0xCu); put(rt, frame.at(0u), bits_of(ctx.fpr[12])); put(rt, frame.at(4u), bits_of(ctx.fpr[13])); put(rt, frame.at(8u), bits_of(ctx.fpr[14])); put(rt, frame.at(0xCu), bits_of(ctx.fpr[15])); call_here(rt, ctx, 0x08A18270u); }
void func_08A26D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B28DCCu); put(rt, G[16] + 0x34u, 0u); put(rt, G[16] + 0x28u, 0u); store8(G[16] + 0x2Cu, 0u); store8(G[16] + 0x2Du, 0u); put(rt, G[16] + 0x30u, 0u); } G[16] = s0; }
void func_08A31880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    construct_with_vtable(rt, ctx, 0x08A76ACCu, 0x08CD14C0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x14u, 0u);
}
void func_08A3420C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x14u); G[16] = G[4]; if (G[2] == 0u) call_here(rt, ctx, 0x08A341A4u); G[2] = word(rt, G[16] + 0x14u); G[4] = G[2]; } G[16] = s0; jump(ctx, 0x08B2E214u); }
void func_08A37E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = 0u; call_here(rt, ctx, 0x08A37DD0u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AB4u); } G[16] = s0; }
void func_08A380F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD16E0u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = 1u; call_here(rt, ctx, 0x08A37DD0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A4D764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x58u) & 0x200u; if (G[2] == 0u) return; ctx.fpr[12] = float_at(rt, G[4] + 0x68u); G[1] = 0x41600000u; ctx.fpr[0] = 14.0f; ctx.fpr[1] = float_at(rt, G[4] + 0x24u); ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[0]; put(rt, G[4] + 0x28u, 0u); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[1]); jump(ctx, 0x08A4CF24u); }
void func_08A50250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = G[5]; G[2] = 0x80460008u; G[4] = G[5]; G[5] = 0u; if (G[3] == 0u) return; G[7] = 0u; call_here(rt, ctx, 0x08B0C5C0u); G[2] = 0u; }
void func_08A5356C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = G[6]; G[16] = G[4] + 0x3Cu; G[4] += 0x38u; call_here(rt, ctx, 0x08C1D1ACu); G[16] ^= G[2]; G[2] = G[16] != 0u ? 1u : 0u; } G[16] = s0; }
void func_08A645EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[5] = 0x1Cu; G[16] = G[4]; call_here(rt, ctx, 0x08A644FCu); G[2] = 0x08CD2CF0u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08A64840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2B38u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A64554u); G[4] = G[16]; G[5] = 0xCu; } G[16] = s0; jump(ctx, 0x08A5BB84u); }
void func_08A64898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[5] = 0x11u; G[16] = G[4]; call_here(rt, ctx, 0x08A644FCu); G[2] = 0x08CD2B38u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08A64B44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = G[5]; G[5] = 2u; G[16] = G[4]; call_here(rt, ctx, 0x08A644FCu); G[2] = 0x08CD28E0u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08A76B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A5B828u); G[2] = 0x08CD3C58u; put(rt, G[16], G[2]); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0xCu, 0u); } G[16] = s0; }
void func_08AA1EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08AA1DE8u); G[2] = word(rt, frame.at(0u)); G[3] = word(rt, frame.at(4u)); put(rt, G[16], G[2]); put(rt, G[16] + 4u, G[3]); } G[16] = s0; }
void func_08AAE250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0xCu); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A5B930u); put(rt, G[16] + 0xCu, 0u); } } G[16] = s0; }
void func_08ABB7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5]; G[7] = G[6]; if (G[4] == 0u) return; G[2] = word(rt, G[4]); G[5] = G[4]; G[6] = G[3]; G[4] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x60u); jump(ctx, G[25]); }
void func_08ABC958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5]; G[7] = G[6] & 0xFFu; if (G[4] == 0u) { G[2] = 0u; return; } G[2] = word(rt, G[4]); G[5] = G[4]; G[6] = G[3]; G[4] = G[2]; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x1Cu); jump(ctx, G[25]); }
void func_08ABCA10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5]; G[7] = G[6] & 0xFFu; if (G[4] == 0u) { G[2] = 0u; return; } G[2] = word(rt, G[4]); G[5] = G[4]; G[6] = G[3]; G[4] = G[2]; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x34u); jump(ctx, G[25]); }
void func_08ABCBB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5]; G[7] = G[6]; if (G[4] == 0u) return; G[2] = word(rt, G[4]); G[5] = G[4]; G[6] = G[3]; G[4] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x20u); jump(ctx, G[25]); }
void func_08AC1028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[9] &= 0xFFu; G[3] = word(rt, G[8] + 4u); G[2] = word(rt, G[8]); G[8] = frame.at(0u); put(rt, frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08AC0F08u); }
void func_08AC11E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[8] &= 0xFFu; G[3] = word(rt, G[7] + 4u); G[2] = word(rt, G[7]); G[7] = frame.at(0u); put(rt, frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08AC1060u); }
void func_08AC4658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08C7ACD8u); G[4] = 0x08CFFCB8u; call_here(rt, ctx, 0x08B2EE2Cu); G[4] = 0x08CFFCF4u; call_here(rt, ctx, 0x08B2B6F4u); G[4] = G[2]; } jump(ctx, 0x08B35254u); }
void func_08AC9208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AD55ECu); put(rt, G[16] + 0x2Cu, G[16]); put(rt, G[16] + 0x30u, 0u); store8(G[16] + 0x34u, 0u); store8(G[16] + 0x35u, 0u); put(rt, G[16] + 0x28u, 0u); } G[16] = s0; }
void func_08ACA378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] < word(rt, G[4] + 0x220u) ? 1u : 0u; G[2] = 0x80000000u; G[7] = 0x80001000u; if (G[3] != 0u) { G[3] = G[5] << 6; G[2] = (G[5] << 2) + G[3] + G[4]; if (G[6] != 0u) { put(rt, G[6], G[2]); G[7] = 0u; } } G[2] = G[7]; }
void func_08AD22D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08AD2060u); G[4] = G[16] + 0x2A4u; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C3DDB8u); }
void func_08AD430C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x64u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0x1Cu); } G[16] = s0; jump(ctx, G[25]); }
void func_08AD43C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AD023Cu); G[2] = 0x08CD6E20u; put(rt, G[16], G[2]); G[3] = high_half(0x08D404DCu); put(rt, 0x08D404DCu, G[16]); } G[16] = s0; }
void func_08ADA968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C90C84u; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD9E74u); G[3] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[2] + 0x38u); G[2] = word(rt, G[2] + 0x28u); G[3] = G[2] + G[3]; } G[2] = G[3]; }
void func_08ADA9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C90C8Cu; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD9E74u); G[3] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[2] + 0x3Cu); G[2] = word(rt, G[2] + 0x28u); G[3] = G[2] + G[3]; } G[2] = G[3]; }
void func_08ADAA90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C90CACu; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD9E74u); G[3] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[2] + 0x40u); G[2] = word(rt, G[2] + 0x2Cu); G[3] = G[2] - G[3]; } G[2] = G[3]; }
void func_08AE4144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08CB6128u); G[16] = G[4]; ctx.fpr[12] = float_at(rt, 0x08CB6128u); call_here(rt, ctx, 0x08AE4120u); G[3] = word(rt, G[16] + 0x4Cu) + G[2]; put(rt, G[16] + 0x4Cu, G[3]); } G[16] = s0; }
void func_08AEB970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); if (static_cast<std::int32_t>(G[4]) < 0) return; if (G[5] == 0u) call_here(rt, ctx, 0x08C7AA58u); else call_here(rt, ctx, 0x08C7AA90u); }
void func_08AF0978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C926ACu; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AEFD90u); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) { G[3] = word(rt, G[2] + 0x24u); G[2] = word(rt, G[2] + 0x20u); G[3] = G[2] - G[3]; } G[2] = G[3]; }
void func_08AF3270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[13] = ctx.fpr[13] + ctx.fpr[14]; ctx.fpr[15] = ctx.fpr[15] + ctx.fpr[16]; ctx.fpr[0] = 0.5f; G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[12]); ctx.fpr[15] = psp_mul(ctx.fpr[15], ctx.fpr[0]); ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[0]); ctx.fpr[0] = psp_mul(ctx.fpr[13], ctx.fpr[15]); ctx.fpr[13] = ctx.fpr[13] - ctx.fpr[15]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[13] / ctx.fpr[0]; }
void func_08AFAC20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; ctx.fpr[12] = float_at(rt, G[4] + 0x20u); ctx.fpr[13] = float_at(rt, G[4] + 0x24u); call_here(rt, ctx, 0x08B58960u); G[4] = G[16] + 0x2Cu; call_here(rt, ctx, 0x08B52A08u); G[2] = G[2] != 0u ? 1u : 0u; } G[16] = s0; }
void func_08B0A0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B09264u); G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B09298u); }
void func_08B0CBCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD7B50u; G[3] = 1u; put(rt, G[4], G[2]); store8(G[4] + 0x14u, 1u); put(rt, G[4] + 0x1Cu, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); store8(G[4] + 0x15u, 0u); store8(G[4] + 0x16u, 0u); put(rt, G[4] + 0x18u, 0u); }
void func_08B0DD50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[8] = G[5]; G[4] = word(rt, G[4] + 0x38u); G[5] = G[2]; G[7] = G[6]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x74u); G[6] = G[8]; call_here(rt, ctx, G[2]); }
void func_08B0DE3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x8u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0x10u); } G[16] = s0; jump(ctx, G[25]); }
void func_08B103EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x90u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B2E28Cu); put(rt, G[16] + 0x90u, 0u); } } G[16] = s0; }
void func_08B10598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x8u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0x14u); } G[16] = s0; jump(ctx, G[25]); }
void func_08B105D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x18u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); } G[16] = s0; jump(ctx, G[25]); }
void func_08B1213C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B343D0u); G[4] = 0u; G[5] = 0u; if (static_cast<std::int32_t>(G[3]) >= 0) { G[4] = G[2]; G[5] = G[3]; } G[2] = G[4]; G[3] = G[5]; }
void func_08B13358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] += 0x4D8u; call_here(rt, ctx, 0x08B2EEB8u); G[4] = 0x08CB4B88u; G[5] = 0u; call_here(rt, ctx, 0x08B1D7F0u); G[4] = G[2]; } jump(ctx, 0x08C7B220u); }
void func_08B19FF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[4] + 0x18u; G[3] = word(rt, G[4] + 0x18u); G[16] = G[4]; if (G[3] != 0u) { G[4] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[4] = G[16] + 0x10u; } G[16] = s0; jump(ctx, 0x08B19FECu); }
void func_08B218F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08B35FD8u); G[3] = 0u; if (G[2] == 0u) G[3] = word(rt, G[16] + 4u); G[2] = G[3]; } G[16] = s0; }
void func_08B22C04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B219B0u); G[2] = 0x08CD95C0u; put(rt, G[16], G[2]); put(rt, G[16] + 8u, 0u); put(rt, G[16] + 4u, 0u); } G[16] = s0; }
void func_08B2C2E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 4u, 0u); G[2] = G[4]; G[3] = 0u; put(rt, G[4] + 8u, 0u); G[5] = 0x10u; put(rt, G[4] + 0xCu, 0u); do { G[3] += 1u; put(rt, G[2] + 0x10u, 0u); G[2] += 4u; } while (G[3] != G[5]); G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x54u, G[2]); put(rt, G[4] + 0x50u, 0u); }
void func_08B2C4A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 32u - static_cast<std::uint32_t>(std::countl_zero(G[5] - 1u)); G[3] = 32u; G[6] = G[2] < 5u ? 1u : 0u; G[4] = 0u; if (G[5] != 0u) { G[2] -= 5u; G[3] = G[2] < 0x10u ? 1u : 0u; G[4] = 0xFu; if (G[3] != 0u) G[4] = G[2]; if (G[6] != 0u) G[4] = 0u; } G[2] = G[4]; }
void func_08B2F17C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[5] = (G[5] & 0xFFu) ^ 1u; G[4] = 0x08CB5490u; G[7] = 0u; call_with(rt, ctx, 0x08C7A930u, 0x10u); G[3] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) G[2] = 0u; }
void func_08B2F1B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x0u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x08C7A938u); put(rt, G[16] + 0x0u, 0u); } } G[16] = s0; }
void func_08B2FE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0xCu); G[4] = G[2]; G[3] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08C7AE80u); G[3] = static_cast<std::int32_t>(G[2]) >= 0 ? 1u : 0u; } G[2] = G[3]; }
void func_08B2FE7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 5u); G[16] = G[4]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B2F44Cu); G[2] = 1u; store8(G[16] + 6u, 1u); } } G[16] = s0; }
void func_08B31918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x0u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x08C7A938u); put(rt, G[16] + 0x0u, 0u); } } G[16] = s0; }
void func_08B32A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = 0x08C93B34u; G[16] = G[4]; call_here(rt, ctx, 0x08B22C74u); G[2] = 0x08CDA820u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08B40BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[8] < 4u ? 1u : 0u; G[2] = G[7]; if (G[8] != 0u) return; G[25] = word(rt, G[4] + 0x24u); G[6] = G[7] + 0x14u; G[3] = G[7] + 0x10u; if (G[25] == 0u) return; G[8] = word(rt, G[4] + 0x28u); G[7] = load16(G[3] + 2u); G[4] = word(rt, G[2] + 0xCu); G[5] = load16(G[2] + 0x10u); jump(ctx, G[25]); }
void func_08B40BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xCu; if (G[8] != G[2]) return; G[5] = load16(G[7] + 8u); G[2] = G[5] < 0x20u ? 1u : 0u; G[5] = sext(G[5], 16u); if (G[2] == 0u) return; G[2] = sext(load16(G[7] + 0xAu), 16u); G[6] = G[2]; if (static_cast<std::int32_t>(G[2]) < 0) return; jump(ctx, 0x08B40B48u); }
void func_08B4BE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B4BDB0u); G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B4B6B0u); } G[16] = s0; }
void func_08B4BF18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B4BEC8u); G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B4B6B0u); } G[16] = s0; }
void func_08B4C030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B4BFE0u); G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B4B6B0u); } G[16] = s0; }
void func_08B62884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[5] + 4u); G[4] = 0u; if (G[3] != G[2]) { G[2] = 0u; return; } G[2] = 6u; put(rt, G[7] + 4u, 6u); G[4] = 1u; G[2] = 8u; put(rt, G[7] + 0xCu, 8u); G[2] = 1u; put(rt, G[7], G[6]); }
void func_08B92B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 8u; if (G[7] != 0u) { G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } G[4] = G[6]; G[7] += 8u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08B92BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 8u; if (G[7] != 0u) { G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } G[4] = G[6]; G[7] += 8u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08B93AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { if (static_cast<std::int32_t>(word(rt, G[3] + 0x10u)) < static_cast<std::int32_t>(G[5])) { G[2] = word(rt, G[3] + 0xCu); } else { G[2] = word(rt, G[3] + 8u); G[4] = G[3]; } G[3] = G[2]; if (G[2] == 0u) break; } } G[2] = G[4]; }
void func_08BABCBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = sext(load8(G[5]), 8u); G[17] = G[5]; call_here(rt, ctx, 0x08894D88u); G[16] += G[2]; store8(G[17], G[16]); } G[16] = s0; G[17] = s1; }
void func_08BABCF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = sext(load8(G[5]), 8u); G[17] = G[5]; call_here(rt, ctx, 0x08894D88u); G[16] += G[2]; store8(G[17], G[16]); } G[16] = s0; G[17] = s1; }
void func_08BBB848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 4u; if (G[7] != 0u) { G[2] = load16(G[4] + 2u); G[3] = load16(G[4]); store16(G[7] + 2u, G[2]); store16(G[7], G[3]); } G[4] = G[6]; G[7] += 4u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08BDD8D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 2u; if (G[7] != 0u) { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); } G[4] = G[6]; G[7] += 2u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08BDD910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 2u; if (G[7] != 0u) { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); } G[4] = G[6]; G[7] += 2u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08BDDA3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 2u; if (G[7] != 0u) { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); } G[4] = G[6]; G[7] += 2u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08BDDAA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 2u; if (G[7] != 0u) { G[2] = load8(G[4]); store8(G[7], G[2]); G[3] = load8(G[4] + 1u); store8(G[7] + 1u, G[3]); } G[4] = G[6]; G[7] += 2u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08BE9188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[7] = 0u; ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 4u); G[8] = 0u; G[5] = frame.at(4u); put(rt, frame.at(4u), bits_of(ctx.fpr[0])); put(rt, frame.at(0u), bits_of(ctx.fpr[1])); call_here(rt, ctx, 0x08969D2Cu); }
void func_08BE953C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[7] = frame.at(4u); G[8] = frame.at(0u); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 4u); G[6] = 0u; G[5] = 0u; put(rt, frame.at(4u), bits_of(ctx.fpr[0])); put(rt, frame.at(0u), bits_of(ctx.fpr[1])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BE9574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[7] = 0u; ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 4u); G[8] = 0u; G[5] = frame.at(4u); put(rt, frame.at(4u), bits_of(ctx.fpr[0])); put(rt, frame.at(0u), bits_of(ctx.fpr[1])); call_here(rt, ctx, 0x0896A9C4u); }
void func_08BF0DCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 8u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[1])); } G[4] = G[3]; G[2] += 8u; } while (G[5] != G[3]); } }
void func_08BF114C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 8u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[1])); } G[4] = G[3]; G[2] += 8u; } while (G[5] != G[3]); } }
void func_08C01EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { G[2] = G[5] - G[4]; G[5] = G[6] + G[2]; G[6] = G[4]; do { G[6] += 1u; if (G[3] != 0u) { G[2] = load8(G[4]); store8(G[3], G[2]); } G[3] += 1u; G[4] = G[6]; } while (G[3] != G[5]); } G[2] = G[3]; }
void func_08C01F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { G[2] = G[5] - G[4]; G[5] = G[6] + G[2]; G[6] = G[4]; do { G[6] += 1u; if (G[3] != 0u) { G[2] = load8(G[4]); store8(G[3], G[2]); } G[3] += 1u; G[4] = G[6]; } while (G[3] != G[5]); } G[2] = G[3]; }
void func_08C066F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 8u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 4u); put(rt, G[2] + 4u, bits_of(ctx.fpr[1])); } G[4] = G[3]; G[2] += 8u; } while (G[5] != G[3]); } }
void func_08C1A1E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B1E2D4u); G[2] = 0x08CD1D40u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C1A254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B1E2D4u); G[2] = 0x08CD1DA0u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C1A28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B1E2D4u); G[2] = 0x08CD1DB8u; put(rt, G[16], G[2]); G[4] = G[16]; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; jump(ctx, 0x08B1E1C0u); }
void func_08C24A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[6]; if (G[4] != G[5]) { G[2] = G[5] - G[4]; G[5] = G[6] + G[2]; G[6] = G[4]; do { G[6] += 1u; if (G[3] != 0u) { G[2] = load8(G[4]); store8(G[3], G[2]); } G[3] += 1u; G[4] = G[6]; } while (G[3] != G[5]); } G[2] = G[3]; }
void func_08C2B064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { if (word(rt, G[3] + 0x10u) < G[5]) { G[2] = word(rt, G[3] + 0xCu); } else { G[2] = word(rt, G[3] + 8u); G[4] = G[3]; } G[3] = G[2]; if (G[2] == 0u) break; } } G[2] = G[4]; }
void func_08C2B09C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { if (!(G[5] < word(rt, G[3] + 0x10u))) { G[2] = word(rt, G[3] + 0xCu); } else { G[2] = word(rt, G[3] + 8u); G[4] = G[3]; } G[3] = G[2]; if (G[2] == 0u) break; } } G[2] = G[4]; }
void func_08C39AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x108u); G[6] = 1u; call_here(rt, ctx, G[2]); G[3] = sext(load8(frame.at(0u)), 8u); G[2] ^= 1u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; }
void func_08C39CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x108u); G[6] = 1u; call_here(rt, ctx, G[2]); G[3] = load8(frame.at(0u)); G[2] ^= 1u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; }
void func_08C3C4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[7] = G[6]; G[6] = 8u; G[16] = G[4]; call_here(rt, ctx, 0x08B24954u); G[2] = 0x08CD6350u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C3CC64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[7] = G[6]; G[6] = 0x1000u; G[16] = G[4]; call_here(rt, ctx, 0x08B24954u); G[2] = 0x08CD6708u; put(rt, G[16], G[2]); } G[16] = s0; }
void func_08C3EB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 8u; if (G[7] != 0u) { G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } G[4] = G[6]; G[7] += 8u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C46744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[1] = psp_mul(ctx.fpr[2], float_at(rt, G[5])); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = psp_mul(ctx.fpr[2], float_at(rt, G[5] + 4u)); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = psp_mul(ctx.fpr[2], float_at(rt, G[5] + 8u)); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[0]); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[2])); }
void func_08C4B040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 4u; if (G[7] != 0u) { G[2] = load16(G[4] + 2u); G[3] = load16(G[4]); store16(G[7] + 2u, G[2]); store16(G[7], G[3]); } G[4] = G[6]; G[7] += 4u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C4B23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 4u; if (G[7] != 0u) { G[2] = load16(G[4] + 2u); G[3] = load16(G[4]); store16(G[7] + 2u, G[2]); store16(G[7], G[3]); } G[4] = G[6]; G[7] += 4u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C4F134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 8u; if (G[7] != 0u) { G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } G[4] = G[6]; G[7] += 8u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C4F17C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 8u; if (G[7] != 0u) { G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } G[4] = G[6]; G[7] += 8u; } while (G[5] != G[6]); } G[2] = G[7]; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_179
} // namespace gt::game

#define HOT179(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_179::name, gt::ReturnKind::kind)
HOT179(0x08818264u, func_08818264, Word);
HOT179(0x0881A774u, func_0881A774, Word);
HOT179(0x088221F4u, func_088221F4, Word);
HOT179(0x08824774u, func_08824774, Word);
HOT179(0x08829490u, func_08829490, Word);
HOT179(0x08854C74u, func_08854C74, Word);
HOT179(0x0885D210u, func_0885D210, Word);
HOT179(0x0886FA58u, func_0886FA58, Word);
HOT179(0x0886FDB0u, func_0886FDB0, Word);
HOT179(0x0887E4F0u, func_0887E4F0, Word);
HOT179(0x0887E750u, func_0887E750, Word);
HOT179(0x0887E880u, func_0887E880, Word);
HOT179(0x0887F37Cu, func_0887F37C, Word);
HOT179(0x0888B29Cu, func_0888B29C, Word);
HOT179(0x0888FE08u, func_0888FE08, Word);
HOT179(0x0888FE40u, func_0888FE40, Word);
// HOT179(0x088910E4u, func_088910E4, Word);  // no comparable synthetic case yet
HOT179(0x08891D28u, func_08891D28, Word);
HOT179(0x08893160u, func_08893160, Word);
HOT179(0x088936F8u, func_088936F8, Word);
HOT179(0x08894E8Cu, func_08894E8C, Word);
HOT179(0x088BCA5Cu, func_088BCA5C, Word);
HOT179(0x088C55C4u, func_088C55C4, Word);
HOT179(0x088D08E0u, func_088D08E0, Word);
HOT179(0x088D1DCCu, func_088D1DCC, Word);
HOT179(0x088D5FA4u, func_088D5FA4, Word);
HOT179(0x088DA3ECu, func_088DA3EC, Word);
HOT179(0x088DA6A8u, func_088DA6A8, Word);
HOT179(0x088DC180u, func_088DC180, Word);
// HOT179(0x088DC1B8u, func_088DC1B8, Word);  // no comparable synthetic case yet
HOT179(0x088E4EC0u, func_088E4EC0, Word);
HOT179(0x088E7228u, func_088E7228, Word);
HOT179(0x088E750Cu, func_088E750C, Word);
HOT179(0x088E7EDCu, func_088E7EDC, Word);
HOT179(0x088E82ACu, func_088E82AC, Word);
HOT179(0x088EF8C4u, func_088EF8C4, Word);
HOT179(0x088F1288u, func_088F1288, Word);
HOT179(0x088F40CCu, func_088F40CC, Word);
HOT179(0x088F5C54u, func_088F5C54, Word);
HOT179(0x088F8A20u, func_088F8A20, Word);
HOT179(0x088FDF98u, func_088FDF98, Word);
HOT179(0x0890599Cu, func_0890599C, Word);
HOT179(0x08906304u, func_08906304, Float);
HOT179(0x08915DB4u, func_08915DB4, Word);
HOT179(0x0891FF94u, func_0891FF94, Word);
// HOT179(0x08927A88u, func_08927A88, Word);  // no comparable synthetic case yet
HOT179(0x08929D98u, func_08929D98, Word);
HOT179(0x08931990u, func_08931990, Word);
HOT179(0x08931B20u, func_08931B20, Word);
HOT179(0x08938F18u, func_08938F18, Word);
HOT179(0x08938F50u, func_08938F50, Word);
HOT179(0x0893A104u, func_0893A104, Word);
HOT179(0x0893A13Cu, func_0893A13C, Word);
HOT179(0x0893A3A4u, func_0893A3A4, Word);
HOT179(0x0893A3DCu, func_0893A3DC, Word);
HOT179(0x08958AA0u, func_08958AA0, Float);
HOT179(0x08959F1Cu, func_08959F1C, Word);
HOT179(0x08966ED0u, func_08966ED0, Word);
HOT179(0x08966F08u, func_08966F08, Word);
HOT179(0x08966F40u, func_08966F40, Word);
HOT179(0x08966F78u, func_08966F78, Word);
HOT179(0x089671FCu, func_089671FC, Float);
HOT179(0x08967234u, func_08967234, Float);
HOT179(0x089775D8u, func_089775D8, Float);
HOT179(0x089893DCu, func_089893DC, Word);
HOT179(0x08989414u, func_08989414, Word);
HOT179(0x08A3533Cu, func_08A3533C, Word);
HOT179(0x0899166Cu, func_0899166C, Word);
HOT179(0x089A0120u, func_089A0120, Word);
HOT179(0x089CFC5Cu, func_089CFC5C, Float);
HOT179(0x089CFC94u, func_089CFC94, Float);
HOT179(0x089F9CA8u, func_089F9CA8, Word);
HOT179(0x08A12AE0u, func_08A12AE0, Word);
HOT179(0x08A1845Cu, func_08A1845C, Word);
HOT179(0x08A26D68u, func_08A26D68, Word);
HOT179(0x08A31880u, func_08A31880, Word);
HOT179(0x08A3420Cu, func_08A3420C, Word);
HOT179(0x08A37E90u, func_08A37E90, Word);
HOT179(0x08A380F0u, func_08A380F0, Word);
HOT179(0x08A4D764u, func_08A4D764, Word);
HOT179(0x08A50250u, func_08A50250, Word);
HOT179(0x08A5356Cu, func_08A5356C, Word);
HOT179(0x08A645ECu, func_08A645EC, Word);
HOT179(0x08A64840u, func_08A64840, Word);
HOT179(0x08A64898u, func_08A64898, Word);
HOT179(0x08A64B44u, func_08A64B44, Word);
HOT179(0x08A76B04u, func_08A76B04, Word);
HOT179(0x08AA1EACu, func_08AA1EAC, Word);
HOT179(0x08AAE250u, func_08AAE250, Word);
HOT179(0x08ABB7B8u, func_08ABB7B8, Word);
HOT179(0x08ABC958u, func_08ABC958, Word);
HOT179(0x08ABCA10u, func_08ABCA10, Word);
HOT179(0x08ABCBB4u, func_08ABCBB4, Word);
// HOT179(0x08AC1028u, func_08AC1028, Word);  // no comparable synthetic case yet
HOT179(0x08AC11E4u, func_08AC11E4, Word);
// HOT179(0x08AC4658u, func_08AC4658, Word);  // no comparable synthetic case yet
HOT179(0x08AC9208u, func_08AC9208, Word);
HOT179(0x08ACA378u, func_08ACA378, Word);
// HOT179(0x08AD22D4u, func_08AD22D4, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT179(0x08AD430Cu, func_08AD430C, Word);
HOT179(0x08AD43C0u, func_08AD43C0, Word);
HOT179(0x08ADA968u, func_08ADA968, Word);
HOT179(0x08ADA9A0u, func_08ADA9A0, Word);
HOT179(0x08ADAA90u, func_08ADAA90, Word);
HOT179(0x08AE4144u, func_08AE4144, Word);
HOT179(0x08AEB970u, func_08AEB970, Word);
HOT179(0x08AF0978u, func_08AF0978, Word);
HOT179(0x08AF3270u, func_08AF3270, Float);
HOT179(0x08AFAC20u, func_08AFAC20, Word);
HOT179(0x08B0A0CCu, func_08B0A0CC, Word);
HOT179(0x08B0CBCCu, func_08B0CBCC, Word);
// HOT179(0x08B0DD50u, func_08B0DD50, Word);  // no comparable synthetic case yet
HOT179(0x08B0DE3Cu, func_08B0DE3C, Word);
HOT179(0x08B103ECu, func_08B103EC, Word);
HOT179(0x08B10598u, func_08B10598, Word);
HOT179(0x08B105D0u, func_08B105D0, Word);
// HOT179(0x08B1213Cu, func_08B1213C, DoubleWord);  // no comparable synthetic case yet
// HOT179(0x08B13358u, func_08B13358, Word);  // no comparable synthetic case yet
// HOT179(0x08B19FF4u, func_08B19FF4, Word);  // no comparable synthetic case yet
HOT179(0x08B218F0u, func_08B218F0, Word);
HOT179(0x08B22C04u, func_08B22C04, Word);
HOT179(0x08B2C2E0u, func_08B2C2E0, Word);
HOT179(0x08B2C4A4u, func_08B2C4A4, Word);
HOT179(0x08B2F17Cu, func_08B2F17C, Word);
HOT179(0x08B2F1B4u, func_08B2F1B4, Word);
HOT179(0x08B2FE44u, func_08B2FE44, Word);
HOT179(0x08B2FE7Cu, func_08B2FE7C, Word);
HOT179(0x08B31918u, func_08B31918, Word);
HOT179(0x08B32A98u, func_08B32A98, Word);
HOT179(0x08B40BA8u, func_08B40BA8, Word);
HOT179(0x08B40BE0u, func_08B40BE0, Word);
HOT179(0x08B4BE00u, func_08B4BE00, Word);
HOT179(0x08B4BF18u, func_08B4BF18, Word);
HOT179(0x08B4C030u, func_08B4C030, Word);
HOT179(0x08B62884u, func_08B62884, Word);
HOT179(0x08B92B58u, func_08B92B58, Word);
HOT179(0x08B92BC0u, func_08B92BC0, Word);
HOT179(0x08B93AB0u, func_08B93AB0, Word);
HOT179(0x08BABCBCu, func_08BABCBC, Word);
HOT179(0x08BABCF4u, func_08BABCF4, Word);
HOT179(0x08BBB848u, func_08BBB848, Word);
HOT179(0x08BDD8D8u, func_08BDD8D8, Word);
HOT179(0x08BDD910u, func_08BDD910, Word);
HOT179(0x08BDDA3Cu, func_08BDDA3C, Word);
HOT179(0x08BDDAA0u, func_08BDDAA0, Word);
HOT179(0x08BE9188u, func_08BE9188, Word);
HOT179(0x08BE953Cu, func_08BE953C, Word);
HOT179(0x08BE9574u, func_08BE9574, Word);
HOT179(0x08BF0DCCu, func_08BF0DCC, Word);
HOT179(0x08BF114Cu, func_08BF114C, Word);
HOT179(0x08C01EA4u, func_08C01EA4, Word);
HOT179(0x08C01F80u, func_08C01F80, Word);
HOT179(0x08C066F4u, func_08C066F4, Word);
HOT179(0x08C1A1E4u, func_08C1A1E4, Word);
HOT179(0x08C1A254u, func_08C1A254, Word);
HOT179(0x08C1A28Cu, func_08C1A28C, Word);
HOT179(0x08C24A38u, func_08C24A38, Word);
HOT179(0x08C2B064u, func_08C2B064, Word);
HOT179(0x08C2B09Cu, func_08C2B09C, Word);
HOT179(0x08C39AF8u, func_08C39AF8, Word);
HOT179(0x08C39CD8u, func_08C39CD8, Word);
HOT179(0x08C3C4E0u, func_08C3C4E0, Word);
HOT179(0x08C3CC64u, func_08C3CC64, Word);
HOT179(0x08C3EB40u, func_08C3EB40, Word);
HOT179(0x08C46744u, func_08C46744, Word);
HOT179(0x08C4B040u, func_08C4B040, Word);
HOT179(0x08C4B23Cu, func_08C4B23C, Word);
HOT179(0x08C4F134u, func_08C4F134, Word);
HOT179(0x08C4F17Cu, func_08C4F17C, Word);
