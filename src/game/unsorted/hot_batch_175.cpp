// Hot batch 175: eleven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_175 {
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
// Store `tag` as the byte at +4 and return the word there masked to its low byte (what the asm re-reads and writes back).
u32 tag_byte(psprecomp::Runtime &rt, u32 self, u32 tag) {
    set_byte(rt, self + 4u, tag);
    return word(rt, self + 4u) & 0xFFu;
}
// `0x08A9FCD8(word(word(self + 0x14) + 4), &slot)` with the slot cleared first; the raw result.
u32 query_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    put(rt, slot, 0u);
    return call(rt, ctx, 0x08A9FCD8u, {word(rt, word(rt, ctx.gpr[4] + 0x14u) + 4u), slot});
}
// `callee(self, word(source + offset))`; returns self.
void call_with_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 callee, u32 offset) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, callee, {self, word(rt, ctx.gpr[5] + offset)});
    ctx.gpr[2] = self;
}
// A byte stored in a frame slot, then the virtual at `slot_offset` of the object's vtable called as (self, &byte, 1).
void call_virtual_with_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable_offset, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 vtable = word(rt, self);
    set_byte(rt, slot, ctx.gpr[5]);
    call_indirect(rt, ctx, word(rt, vtable + vtable_offset), {self, slot, 1u});
}
// A command {kind, value} posted to the object at word(self + 0x18) (0x088936A4).
void post_kind(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 kind, u32 slot) {
    put(rt, slot + 8u, ctx.gpr[5]);
    put(rt, slot, kind);
    call(rt, ctx, 0x088936A4u, {word(rt, ctx.gpr[4] + 0x18u), slot});
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

void func_08805774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08AB7754u, 0x10u); G[2] = high_half(0x08D69524u); G[5] = word(rt, 0x08D69524u); G[4] = 0x08D6950Cu; jump(ctx, 0x08B49414u); }
void func_088057CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B1E278u); call_here(rt, ctx, 0x08B1DF7Cu); call_here(rt, ctx, 0x08B2E250u); } jump(ctx, 0x08B2B2E4u); }
void func_08817934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 4u); G[4] = (G[2] << 2) + 0x08CDC5FCu; G[2] = G[2] < 4u ? 1u : 0u; G[3] = 0u; if (G[2] != 0u) G[3] = word(rt, G[4]); G[2] = G[3]; }
void func_0881AA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x088A6C78u, 0x10u); G[3] = (G[2] << 5) - (G[2] << 1); G[2] = G[3] + 0x320u; }
void func_0884F734(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B35254u, 0x10u); G[3] = high_half(0x08CE2CECu); G[2] = word(rt, 0x08CE2CECu) - 1u; put(rt, 0x08CE2CECu, G[2]); }
void func_0885C1D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = word(rt, G[4]); G[2] = G[5] << 6; G[5] <<= 3; G[2] -= G[5]; G[3] = word(rt, G[8]); G[4] = (G[2] << 3) - G[2] + G[3]; G[5] = G[6]; G[6] = G[7]; jump(ctx, 0x0885D868u); }
void func_0885F004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] &= 0xFFu; G[3] = word(rt, G[2] + 0x5ACu); G[4] = G[3]; if (G[3] == 0u) return; jump(ctx, 0x088D6918u); }
void func_0886A9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5]; G[5] = G[4] + 0x64u; G[4] = G[3]; if (G[3] == 0u) return; G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x20u); G[25] = G[2]; jump(ctx, G[25]); }
void func_0887ED1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[5] = word(rt, G[5] + 0x74u); call_here(rt, ctx, 0x08882B20u); G[2] = G[16]; } G[16] = s0; }
void func_0888B670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x10u; put(rt, frame.at(8u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_0888B69C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    post_kind(rt, ctx, 0x11u, frame.at(0x0u));
}
void func_0888B71C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    post_kind(rt, ctx, 0x22u, frame.at(0x0u));
}
void func_0888C0B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x20u; put(rt, frame.at(8u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_08893B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; G[4] = static_cast<std::int32_t>(G[5]) < 0xB ? 1u : 0u; if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = 0x08CDD054u; G[3] = (G[5] << 2) + G[2]; if (G[4] != 0u) G[6] = word(rt, G[3]); } G[2] = G[6]; }
void func_0889496C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = word(rt, G[4] + 0x54u); G[3] = word(rt, G[4] + 0x58u); G[4] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); call_here(rt, ctx, 0x088B160Cu); }
void func_0889658C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = word(rt, G[4] + 0x58u); G[3] = word(rt, G[4] + 0x5Cu); G[4] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); call_here(rt, ctx, 0x088B160Cu); }
void func_088970EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::int32_t>(G[5]) < 0x65 ? 1u : 0u; G[3] = static_cast<std::int32_t>(G[5]) < 0 ? 1u : 0u; G[6] = 0x64u; G[5] = sext(G[5], 8u); if (G[2] != 0u) { G[6] = 0u; if (G[3] == 0u) G[6] = G[5]; } G[3] = word(rt, G[4]); G[2] = G[6] + 1u; store8(G[3] + 0xADu, G[2]); }
void func_088A2898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(4u), G[6]); G[6] = frame.at(0u); store8(frame.at(8u), 0u); store8(frame.at(9u), 0u); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x088A26E0u); }
void func_088B04F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[16] = G[4]; call_here(rt, ctx, 0x08B93CD8u); G[2] = G[16]; } G[16] = s0; }
void func_088BF94C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5] + 0x24u); G[3] = G[4] & 0xFFu; G[4] = G[5]; store8(G[2] + 0x15u, G[3]); put(rt, G[2] + 0x84u, 0u); store8(G[2] + 0x10u, G[3]); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x54u, 0u); put(rt, G[2] + 0x7Cu, 0u); put(rt, G[2] + 0x80u, 0u); jump(ctx, 0x088BF82Cu); }
void func_088CEA34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[2] = 0x78u; put(rt, G[4] + 0xE8u, G[1]); put(rt, G[4] + 0x130u, G[2]); put(rt, G[4] + 0x134u, 0u); put(rt, G[4] + 0x128u, 0u); put(rt, G[4] + 0x12Cu, 0u); put(rt, G[4] + 0x138u, 0u); put(rt, G[4] + 0x13Cu, 0u); }
void func_088CFA28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 base = call(rt, ctx, 0x088CF978u, {self});
    ctx.gpr[2] = word(rt, self + 0xB4u) + base;
}
void func_088D0590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[3] = 0u; G[5] = 0x10u; do { ctx.fpr[0] = float_at(rt, G[2]); G[3] += 1u; put(rt, G[2] + 0x40u, bits_of(ctx.fpr[0])); G[2] += 4u; } while (G[3] != G[5]); G[5] = G[4] + 0x88u; G[4] += 0x90u; jump(ctx, 0x088D04FCu); }
void func_088D097C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 extra = word(rt, self + 0x70u);
    put(rt, self + 0x78u, call(rt, ctx, 0x088D0974u, {self, ctx.gpr[5], extra}));
}
void func_088D2674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x23Cu; G[3] = 0u; G[6] = 0x10u; do { G[2] = word(rt, G[4]); G[3] += 1u; G[4] += 0xCu; put(rt, G[5], G[2]); G[5] += 4u; } while (G[3] != G[6]); }
void func_088D6958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x088F8D58u, {word(rt, ctx.gpr[4] + 0x14Cu)});
    u32 value = 0u;
    if (found != 0u) value = byte_at(rt, found + 0xCu);
    ctx.gpr[2] = value;
}
void func_088DA550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 5; G[2] = (G[5] << 7) - G[3]; G[4] = word(rt, G[4] + 0xC0u); G[2] = (G[2] - G[5]) << 3; G[3] = ~G[6]; if (G[3] == 0u) G[6] = G[5]; G[4] += G[2]; G[5] = G[7]; jump(ctx, 0x088E7EDCu); }
void func_088E48B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = load8(G[4] + 0x52Cu); G[6] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[4] + 0x500u); G[3] = word(rt, G[4] + 0x504u); G[6] = 1u; put(rt, G[5], G[2]); put(rt, G[5] + 4u, G[3]); } G[2] = G[6]; }
void func_088E4B08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); put(rt, G[2] + 0xB0u, G[5]); G[3] = word(rt, G[4]); put(rt, G[3] + 0xB4u, G[6]); G[2] = word(rt, G[4]); put(rt, G[2] + 0xB8u, G[7]); G[3] = word(rt, G[4]); put(rt, G[3] + 0xBCu, G[8]); G[2] = word(rt, G[4]); put(rt, G[2] + 0xC0u, G[9]); }
void func_088E511C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0xFFFFFFFFu; G[3] = word(rt, G[4] + 0x548u); G[5] = 1u; if (G[3] != G[2]) { G[2] = word(rt, G[4] + 0x54Cu); if (G[3] == G[2]) { G[2] = G[5]; return; } } G[5] = 0u; G[2] = 0u; }
void func_088E84B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2E4u); put(rt, G[4] + 0x2D8u, G[6]); put(rt, G[4] + 0x2DCu, G[7]); if (G[3] == 0u) return; G[2] = word(rt, G[4] + 0x1Cu); if (G[2] != 0u) return; put(rt, G[3] + 0x18u, G[6]); put(rt, G[3] + 0x1Cu, G[7]); }
void func_088EB35C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x44u); call_here(rt, ctx, G[2]); } G[2] = 1u; }
void func_088ECA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x11Cu); G[2] = G[5] << 1; G[5] += G[4]; G[6] = G[2] + G[3]; if (G[3] != 0u) { G[2] = 0xFFFFFFFFu; store16(G[6], G[2]); G[3] = word(rt, G[4] + 0x11Cu); store8(G[3] + 0x1Au, 0u); } store8(G[5] + 0x4B8u, 0u); }
void func_08900DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x44u); G[3] = G[4]; G[6] = G[5] << 3; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] = 0u; if (G[2] != 0u) { G[2] = G[6] + word(rt, G[3] + 0x48u); G[4] = word(rt, G[2]); } G[2] = G[4]; }
void func_089099FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[4] = 0u; if (G[2] != 0u) { G[3] = (G[5] << 1) + G[2]; G[2] = G[5] < load16(G[2]) ? 1u : 0u; if (G[2] != 0u) G[4] = load16(G[3] + 2u); } G[2] = G[4]; }
void func_0890B428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0x5Cu); G[3] = G[5] << 2; G[2] = (G[5] << 4) - G[3]; G[6] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[2] += G[4]; G[3] = 0u; if (G[6] != 0u) G[3] = G[2] + 0x44u; G[2] = G[3]; }
void func_08910838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = (G[4] << 5) - (G[4] << 3); G[4] <<= 3; G[2] = 0x08CDD81Cu; G[3] = G[3] + G[2] + G[5]; G[3] = sext(load8(G[3]), 8u); ctx.fpr[1] = std::bit_cast<float>(G[3]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); }
void func_0891A898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5] + 0xD4u); G[5] += 0xD4u; G[2] = G[4]; put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); }
void func_0891A8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_member(rt, ctx, 0x0891A898u, 0x1F8u);
}
void func_0891A9B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5] + 0xC4u); G[5] += 0xC4u; G[2] = G[4]; put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); }
void func_0891A9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_member(rt, ctx, 0x0891A9B4u, 0x1F8u);
}
void func_089264E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[6] = 1u; G[16] = G[4]; call_here(rt, ctx, 0x08926488u); put(rt, G[16] + 0x2Cu, 0u); } G[16] = s0; }
void func_089277BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x089277B4u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 8u); G[25] = G[2]; } jump(ctx, G[25]); }
void func_089279B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x089277B4u); G[4] = G[2]; G[5] = G[16]; } G[16] = s0; jump(ctx, 0x08926AB8u); }
void func_089279E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x089277B4u); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x2Cu); G[25] = G[2]; } jump(ctx, G[25]); }
void func_08927EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x1FB0u); G[4] = G[5]; G[5] = G[2]; call_with(rt, ctx, 0x088DF13Cu, 0x10u); G[2] = word(rt, G[2] + 0x54u) != 0u ? 1u : 0u; }
void func_089281B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5] + 0x20F4u); G[5] += 0x20F4u; G[2] = G[4]; put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[0])); }
void func_0892A92C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = static_cast<std::int32_t>(G[4]) < 0 ? 1u : 0u; G[2] = G[4] + 2u; G[4] -= 2u; if (G[3] != 0u) G[2] = G[4]; G[2] = sra(G[2], 2u); G[3] = 0xFFFFE000u; G[5] = static_cast<std::int32_t>(G[2]) < 0x2000 ? 1u : 0u; G[4] = 0x2000u; if (static_cast<std::int32_t>(G[2]) < -0x2000) G[2] = G[3]; if (G[5] == 0u) G[2] = G[4]; }
void func_089312B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = word(rt, G[2] + 0x1ECu); G[5] = G[16]; } G[16] = s0; jump(ctx, 0x08937D90u); }
void func_0895EDD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x40C90FDBu; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[14] = psp_mul(ctx.fpr[14], ctx.fpr[0]); ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[13]); ctx.fpr[12] = psp_mul(ctx.fpr[14], ctx.fpr[12]); ctx.fpr[14] = psp_mul(ctx.fpr[14], ctx.fpr[12]); ctx.fpr[13] = ctx.fpr[14] / ctx.fpr[13]; put(rt, G[5], bits_of(ctx.fpr[14])); put(rt, G[4], bits_of(ctx.fpr[13])); }
void func_0896147C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08961468u, {self});
    ctx.gpr[2] = 1u;
    set_byte(rt, self + 0x48u, 1u);
}
void func_089833D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B2E05Cu); put(rt, G[16] + 0xD8u, G[2]); put(rt, G[16] + 0xDCu, G[3]); } G[16] = s0; }
void func_089CDFC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x178u); G[3] = (G[2] << 4) - (G[2] << 2) + 0x160u; G[2] <<= 2; G[4] += G[3]; G[5] = word(rt, G[4]); G[2] = sra(word(rt, G[4] + 4u) - G[5], 2u); }
void func_089CE0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x188u); call_here(rt, ctx, 0x089CDFC4u); G[16] = G[16] != 0u ? 1u : 0u; G[2] += G[16]; } G[16] = s0; }
void func_089E6B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x180u); G[3] = (G[2] << 4) - (G[2] << 2) + 0x160u; G[2] <<= 2; G[4] += G[3]; G[5] = word(rt, G[4] + 8u); G[2] = sra(word(rt, G[4] + 0xCu) - G[5], 2u); }
void func_089FAEC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4]); G[3] = 1u; G[2] = sra(word(rt, G[4] + 4u) - G[5], 5u); if (G[2] != G[3]) { G[2] = word(rt, G[4] + 0xCu); return; } G[2] = word(rt, G[5] + 0x10u); }
void func_08A0AD70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_member(rt, ctx, 0x089F9ED4u, 0x10u);
}
void func_08A1B34C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08ACB0D8u); G[4] = G[16] + 8u; put(rt, G[16] + 8u, 0u); } G[16] = s0; jump(ctx, 0x08ACA948u); }
void func_08A32A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[3] = G[4] + 0x408u; put(rt, G[4] + 0x408u, 0u); G[6] = G[5]; G[4] = G[3]; store8(G[2], 0u); G[5] = 0u; put(rt, G[2] + 4u, 0u); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 8u, 0u); jump(ctx, 0x08C1ABE0u); }
void func_08A3A4A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0x18u); G[2] = G[4]; G[3] = G[5] & 0xFFu; G[5] = G[3]; G[4] = G[6]; store8(G[2] + 0x2Cu, G[3]); if (G[6] == 0u) return; jump(ctx, 0x08ABCBECu); }
void func_08A61714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08AA64DCu); put(rt, G[16] + 8u, G[2]); } G[16] = s0; }
void func_08A644FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4] + 4u, G[5]); G[2] = G[4]; G[5] = word(rt, G[4] + 4u); G[3] = 0x08CD2868u; put(rt, G[4], G[3]); G[5] &= 0xFFu; G[4] += 8u; put(rt, G[2] + 4u, G[5]); G[5] = G[6]; jump(ctx, 0x08A6972Cu); }
void func_08A677E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 tag = tag_byte(rt, self, 0x2Bu);
    put(rt, self + 8u, ctx.gpr[6]);
    put(rt, self + 0xCu, ctx.gpr[7]);
    put(rt, self + 4u, tag);
    put(rt, self, 0x08CD2F48u);
    ctx.gpr[2] = 0x08CD2F48u;
}
void func_08A679B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 tag = tag_byte(rt, self, 0x2Du);
    set_byte(rt, self + 0xCu, ctx.gpr[6]);
    put(rt, self + 4u, tag);
    put(rt, self, 0x08CD2F98u);
    put(rt, self + 8u, ctx.gpr[5]);
    ctx.gpr[2] = 0x08CD2F98u;
}
void func_08A67DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 tag = tag_byte(rt, self, 0x31u);
    put(rt, self + 0xCu, ctx.gpr[6]);
    put(rt, self + 4u, tag);
    put(rt, self, 0x08CD3038u);
    put(rt, self + 8u, ctx.gpr[5]);
    ctx.gpr[2] = 0x08CD3038u;
}
void func_08A68620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 tag = tag_byte(rt, self, 0x3Bu);
    put(rt, self + 8u, ctx.gpr[6]);
    put(rt, self + 0xCu, ctx.gpr[7]);
    put(rt, self + 4u, tag);
    put(rt, self, 0x08CD32D8u);
    ctx.gpr[2] = 0x08CD32D8u;
}
void func_08A68700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 tag = tag_byte(rt, self, 0x3Cu);
    put(rt, self + 8u, ctx.gpr[6]);
    put(rt, self + 0xCu, ctx.gpr[7]);
    put(rt, self + 4u, tag);
    put(rt, self, 0x08CD3410u);
    ctx.gpr[2] = 0x08CD3410u;
}
void func_08A68D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_virtual_with_byte(rt, ctx, 0x108u, frame.at(0x0u));
}
void func_08A6986C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = raw & 0xFFFFu;
}
void func_08A69898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = raw & 0xFFu;
}
void func_08A69914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(raw)));
}
void func_08A69940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(raw)));
}
void func_08A69D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[3] - 1u; put(rt, G[4], G[2]); G[2] = 1u; if (G[3] != G[2]) return; jump(ctx, 0x08A69CBCu); }
void func_08A72600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08A5D444u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A8DC58u); }
void func_08A80288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08A5D444u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A8DC28u); }
void func_08A86C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08A5D444u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A8DBF8u); }
void func_08A98290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08A5D444u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A8DC88u); }
void func_08A9E32C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[6]; G[4] = word(rt, G[4] + 0x20u); call_here(rt, ctx, 0x08BEED1Cu); G[2] = G[16]; } G[16] = s0; }
void func_08AA18F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_virtual_with_byte(rt, ctx, 0x10Cu, frame.at(0x0u));
}
void func_08AA199C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_virtual_with_byte(rt, ctx, 0x10Cu, frame.at(0x0u));
}
void func_08AAE224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_member(rt, ctx, 0x08A6A40Cu, 0x10u);
}
void func_08ABA410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08AD3C38u); G[4] = G[2]; G[5] = G[16]; } G[16] = s0; jump(ctx, 0x08AD3CB0u); }
void func_08ABA57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD3B00u); G[4] = 0x08CFFA54u; call_here(rt, ctx, 0x08B2B6F4u); G[4] = G[2]; } jump(ctx, 0x08B35254u); }
void func_08AC2BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, 0x08CFFCA0u); G[5] = G[4]; G[4] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x5Cu); jump(ctx, G[25]); }
void func_08AD0878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B2F0D4u); G[2] = high_half(0x08CFFAACu); G[4] = word(rt, 0x08CFFAACu); G[3] = word(rt, G[4]); G[25] = word(rt, G[3] + 0x24u); } jump(ctx, G[25]); }
void func_08AD0FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[4] + 0x1Cu, G[1]); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, G[1]); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, G[1]); put(rt, G[4] + 0x14u, G[1]); put(rt, G[4] + 0x18u, G[1]); }
void func_08AD8308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[2] = 0xC8u; put(rt, G[4] + 0x2Cu, G[2]); put(rt, G[4] + 0x38u, G[1]); put(rt, G[4] + 0x40u, G[5]); store8(G[4] + 0x44u, 0u); put(rt, G[4] + 0x30u, 0u); put(rt, G[4] + 0x34u, G[1]); put(rt, G[4] + 0x3Cu, 0u); }
void func_08ADA488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5]; G[7] = G[6]; G[5] = G[4]; G[6] = G[2]; G[4] = 0xFFFFFFFFu; call_with(rt, ctx, 0x08ADA094u, 0x10u); }
void func_08AE3110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 1u; G[4] = sext(load16(G[4] + 0x1Cu), 16u); store8(frame.at(0u), 0u); call_here(rt, ctx, 0x08ADACC8u); G[2] = load8(frame.at(0u)); }
void func_08AE3C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08AE2C98u, {ctx.gpr[4], 0u});
    u32 value = 0u;
    if (found != 0u) value = word(rt, found + 0xCu);
    ctx.gpr[2] = value;
}
void func_08AECFE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2094u); G[2] = G[4]; G[4] = word(rt, G[4] + 0x2098u); G[3] += G[5]; if (G[4] == 0u) return; divide(ctx, G[3], G[4]); G[5] = ctx.hi; G[2] += G[5] + 0x20A0u; }
void func_08AF2B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], G[5]); G[4] = G[5]; call_with(rt, ctx, 0x08AF296Cu, 0x10u); G[5] = word(rt, G[2]); G[6] = word(rt, G[2] + 4u); G[4] = G[2]; jump(ctx, 0x08BD3210u); }
void func_08AF7C8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = (G[4] << 4) + 0x08D471C4u; G[2] = high_half(0x08D471C0u); put(rt, G[4] + 0xCu, G[8]); put(rt, G[4], G[5]); put(rt, G[4] + 4u, G[6]); put(rt, G[4] + 8u, G[7]); put(rt, 0x08D471C0u, 0u); }
void func_08AFABF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0x2Cu; put(rt, G[5], G[2]); ctx.fpr[0] = float_at(rt, G[4] + 8u); ctx.fpr[1] = float_at(rt, G[4] + 4u); put(rt, G[5] + 4u, 0u); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[0]))); ctx.fpr[3] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[1]))); put(rt, G[5] + 8u, 0u); put(rt, G[5] + 0x10u, bits_of(ctx.fpr[2])); put(rt, G[5] + 0xCu, bits_of(ctx.fpr[3])); }
void func_08B09264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    put(rt, self + 0x24u, 0u);
    for (const u32 offset : {0u, 4u, 8u, 0xCu, 0x10u, 0x14u, 0x18u, 0x1Cu, 0x20u}) put(rt, self + offset, 0u);
}
void func_08B09538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = high_half(0x08D47348u); const std::uint32_t handler = word(rt, 0x08D47348u); G[2] = high_half(0x08D47344u); if (handler == 0u) return; G[3] = word(rt, 0x08D47344u); put(rt, G[4] + 0x1Cu, G[3]); G[25] = word(rt, 0x08D47348u); jump(ctx, G[25]); }
void func_08B0C208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; store8(G[4] + 0x14u, 1u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); store8(G[4] + 0x15u, 0u); store8(G[4] + 0x16u, 0u); put(rt, G[4] + 0x1Cu, 0u); }
void func_08B0DBCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4] + 0xF8u; put(rt, G[4] + 0x54u, G[3]); G[2] = 0x08B1094Cu; G[3] = 0x08B10590u; put(rt, G[4] + 0x6Cu, G[2]); put(rt, G[4] + 0x74u, G[3]); put(rt, G[4] + 0x78u, 0u); put(rt, G[4] + 0x70u, 0u); }
void func_08B10448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B1039Cu); G[4] = word(rt, G[16] + 0x90u); store8(G[16] + 0x9Cu, 0u); } G[16] = s0; jump(ctx, 0x08B2E214u); }
void func_08B1257C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = G[4] + 1u; G[4] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08B12268u); G[3] = word(rt, frame.at(0u)); G[2] += G[3]; }
void func_08B15A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4] + 0xF8u; put(rt, G[4] + 0x54u, G[3]); G[2] = 0x08B10588u; G[3] = 0x08B10590u; put(rt, G[4] + 0x6Cu, G[2]); put(rt, G[4] + 0x74u, G[3]); put(rt, G[4] + 0x78u, 0u); put(rt, G[4] + 0x70u, 0u); }
void func_08B1A088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B34888u); G[4] = G[16]; store8(G[16] + 0x2Cu, 0u); } G[16] = s0; jump(ctx, 0x08B34B24u); }
void func_08B1B5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[7] = (G[4] << 8) - (G[4] << 4); G[4] <<= 4; G[3] = (G[5] << 6) - (G[5] << 2); G[5] <<= 2; G[2] = (G[7] << 4) - G[7] + G[3] + G[6]; }
void func_08B1BFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x98u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_08B21458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = sra(load8(G[4] + 4u) + 7u, 3u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] += G[5]; if (G[2] != 0u) G[3] = load8(G[4] + 5u); } G[2] = G[3]; }
void func_08B24AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[5] = word(rt, G[4] + 0xCu); G[4] = word(rt, G[4] + 0x18u); G[3] += G[5]; G[2] = G[4] < G[3] ? 1u : 0u; if (G[2] != 0u) { G[2] = G[3] - G[4]; G[5] -= G[2]; } G[2] = G[5]; }
void func_08B273A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2Cu); G[2] = G[3] < word(rt, G[4] + 0x24u) ? 1u : 0u; G[6] = 0u; G[7] = G[3] + 2u; if (G[2] != 0u) { store16(G[3], G[5]); G[6] = 1u; put(rt, G[4] + 0x2Cu, G[7]); } G[2] = G[6]; }
void func_08B27874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x7Cu); G[7] = word(rt, G[4] + 0x74u); G[6] = load8(G[4] + 0x90u); G[3] = G[2] < G[5] ? 1u : 0u; if (G[5] == 0u) G[5] = G[2]; if (G[3] == 0u) G[2] = G[5]; G[2] += G[7]; store8(G[4] + 0x84u, G[6]); put(rt, G[4] + 0x78u, G[2]); put(rt, G[4] + 0x80u, G[7]); }
void func_08B2C42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[7] = word(rt, G[5] + 4u); G[4] = G[5] + G[6]; G[3] = G[7] < G[3] ? 1u : 0u; G[2] = G[4]; if (G[3] != 0u) put(rt, G[7], G[4]); put(rt, G[4] + 4u, G[7]); put(rt, G[4], G[5]); put(rt, G[5] + 4u, G[4]); }
void func_08B2C458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5] + 4u); G[3] = word(rt, G[4] + 4u); put(rt, G[4] + 0xCu, G[3]); put(rt, G[3], 0u); put(rt, G[3] + 4u, G[5]); G[2] = G[5] < word(rt, G[4] + 8u) ? 1u : 0u; if (G[2] != 0u) put(rt, G[5], G[3]); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_175
} // namespace gt::game

#define HOT175(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_175::name, gt::ReturnKind::kind)
// HOT175(0x08805774u, func_08805774, Word);  // no comparable synthetic case yet
HOT175(0x088057CCu, func_088057CC, Word);
HOT175(0x08817934u, func_08817934, Word);
HOT175(0x0881AA18u, func_0881AA18, Word);
HOT175(0x0884F734u, func_0884F734, Word);
// HOT175(0x0885C1D0u, func_0885C1D0, Word);  // no comparable synthetic case yet
HOT175(0x0885F004u, func_0885F004, Word);
HOT175(0x0886A9E0u, func_0886A9E0, Word);
HOT175(0x0887ED1Cu, func_0887ED1C, Word);
// HOT175(0x0888B670u, func_0888B670, Word);  // no comparable synthetic case yet
HOT175(0x0888B69Cu, func_0888B69C, Word);
HOT175(0x0888B71Cu, func_0888B71C, Word);
HOT175(0x0888C0B8u, func_0888C0B8, Word);
HOT175(0x08893B80u, func_08893B80, Word);
HOT175(0x0889496Cu, func_0889496C, Word);
HOT175(0x0889658Cu, func_0889658C, Word);
HOT175(0x088970ECu, func_088970EC, Word);
HOT175(0x088A2898u, func_088A2898, Word);
HOT175(0x088B04F4u, func_088B04F4, Word);
// HOT175(0x088BF94Cu, func_088BF94C, Word);  // no comparable synthetic case yet
HOT175(0x088CEA34u, func_088CEA34, Word);
HOT175(0x088CFA28u, func_088CFA28, Word);
HOT175(0x088D0590u, func_088D0590, Word);
HOT175(0x088D097Cu, func_088D097C, Word);
HOT175(0x088D2674u, func_088D2674, Word);
HOT175(0x088D6958u, func_088D6958, Word);
HOT175(0x088DA550u, func_088DA550, Word);
HOT175(0x088E48B0u, func_088E48B0, Word);
HOT175(0x088E4B08u, func_088E4B08, Word);
HOT175(0x088E511Cu, func_088E511C, Word);
HOT175(0x088E84B8u, func_088E84B8, Word);
HOT175(0x088EB35Cu, func_088EB35C, Word);
HOT175(0x088ECA54u, func_088ECA54, Word);
HOT175(0x08900DE8u, func_08900DE8, Word);
HOT175(0x089099FCu, func_089099FC, Word);
HOT175(0x0890B428u, func_0890B428, Word);
HOT175(0x08910838u, func_08910838, Float);
HOT175(0x0891A898u, func_0891A898, Word);
HOT175(0x0891A8C4u, func_0891A8C4, Word);
HOT175(0x0891A9B4u, func_0891A9B4, Word);
HOT175(0x0891A9E0u, func_0891A9E0, Word);
HOT175(0x089264E8u, func_089264E8, Word);
HOT175(0x089277BCu, func_089277BC, Word);
HOT175(0x089279B4u, func_089279B4, Word);
HOT175(0x089279E0u, func_089279E0, Word);
HOT175(0x08927EACu, func_08927EAC, Word);
HOT175(0x089281B8u, func_089281B8, Word);
HOT175(0x0892A92Cu, func_0892A92C, Word);
HOT175(0x089312B0u, func_089312B0, Word);
HOT175(0x0895EDD8u, func_0895EDD8, Word);
HOT175(0x0896147Cu, func_0896147C, Word);
HOT175(0x089833D0u, func_089833D0, Word);
HOT175(0x089CDFC4u, func_089CDFC4, Word);
HOT175(0x089CE0F8u, func_089CE0F8, Word);
HOT175(0x089E6B00u, func_089E6B00, Word);
HOT175(0x089FAEC0u, func_089FAEC0, Word);
HOT175(0x08A0AD70u, func_08A0AD70, Word);
HOT175(0x08A1B34Cu, func_08A1B34C, Word);
HOT175(0x08A32A14u, func_08A32A14, Word);
HOT175(0x08A3A4A0u, func_08A3A4A0, Word);
// HOT175(0x08A61714u, func_08A61714, Word);  // no comparable synthetic case yet
HOT175(0x08A644FCu, func_08A644FC, Word);
HOT175(0x08A677E4u, func_08A677E4, Word);
HOT175(0x08A679B8u, func_08A679B8, Word);
HOT175(0x08A67DB8u, func_08A67DB8, Word);
HOT175(0x08A68620u, func_08A68620, Word);
HOT175(0x08A68700u, func_08A68700, Word);
HOT175(0x08A68D78u, func_08A68D78, Word);
HOT175(0x08A6986Cu, func_08A6986C, Word);
HOT175(0x08A69898u, func_08A69898, Word);
HOT175(0x08A69914u, func_08A69914, Word);
HOT175(0x08A69940u, func_08A69940, Word);
HOT175(0x08A69D3Cu, func_08A69D3C, Word);
// HOT175(0x08A72600u, func_08A72600, Word);  // no comparable synthetic case yet
// HOT175(0x08A80288u, func_08A80288, Word);  // no comparable synthetic case yet
// HOT175(0x08A86C50u, func_08A86C50, Word);  // no comparable synthetic case yet
// HOT175(0x08A98290u, func_08A98290, Word);  // no comparable synthetic case yet
HOT175(0x08A9E32Cu, func_08A9E32C, Word);
HOT175(0x08AA18F0u, func_08AA18F0, Word);
HOT175(0x08AA199Cu, func_08AA199C, Word);
HOT175(0x08AAE224u, func_08AAE224, Word);
HOT175(0x08ABA410u, func_08ABA410, Word);
HOT175(0x08ABA57Cu, func_08ABA57C, Word);
// HOT175(0x08AC2BACu, func_08AC2BAC, Word);  // no comparable synthetic case yet
// HOT175(0x08AD0878u, func_08AD0878, Word);  // no comparable synthetic case yet
HOT175(0x08AD0FACu, func_08AD0FAC, Word);
HOT175(0x08AD8308u, func_08AD8308, Word);
HOT175(0x08ADA488u, func_08ADA488, Word);
HOT175(0x08AE3110u, func_08AE3110, Word);
HOT175(0x08AE3C58u, func_08AE3C58, Word);
HOT175(0x08AECFE4u, func_08AECFE4, Word);
HOT175(0x08AF2B70u, func_08AF2B70, Word);
HOT175(0x08AF7C8Cu, func_08AF7C8C, Word);
HOT175(0x08AFABF4u, func_08AFABF4, Word);
HOT175(0x08B09264u, func_08B09264, Word);
HOT175(0x08B09538u, func_08B09538, Word);
HOT175(0x08B0C208u, func_08B0C208, Word);
HOT175(0x08B0DBCCu, func_08B0DBCC, Word);
HOT175(0x08B10448u, func_08B10448, Word);
HOT175(0x08B1257Cu, func_08B1257C, Word);
HOT175(0x08B15A6Cu, func_08B15A6C, Word);
HOT175(0x08B1A088u, func_08B1A088, Word);
HOT175(0x08B1B5E0u, func_08B1B5E0, Word);
HOT175(0x08B1BFACu, func_08B1BFAC, Word);
HOT175(0x08B21458u, func_08B21458, Word);
HOT175(0x08B24AE8u, func_08B24AE8, Word);
HOT175(0x08B273A0u, func_08B273A0, Word);
HOT175(0x08B27874u, func_08B27874, Word);
HOT175(0x08B2C42Cu, func_08B2C42C, Word);
HOT175(0x08B2C458u, func_08B2C458, Word);
