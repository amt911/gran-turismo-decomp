// Hot batch 174: ten-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_174 {
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
// `0x08A9FCD8(word(self + 0x14), &slot)` with the slot cleared first; the raw result.
u32 query_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    put(rt, slot, 0u);
    return call(rt, ctx, 0x08A9FCD8u, {word(rt, ctx.gpr[4] + 0x14u), slot});
}
// `0x08B7E4CC(word(self + 0x14), 0, a1)`; the raw result.
u32 query_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return call(rt, ctx, 0x08B7E4CCu, {word(rt, ctx.gpr[4] + 0x14u), 0u, ctx.gpr[5]});
}
u32 sign8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sign16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// `source(self)`, its result stored at self + offset.
void store_result(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 source, u32 offset) {
    const u32 self = ctx.gpr[4];
    put(rt, self + offset, call(rt, ctx, source, {self}));
}
// The virtual at `slot` of the vtable of `object` called on it.
void call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 object, u32 slot) {
    call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
}
// `0x08927E44(word(table + 8 + index * 4))`, then the word at `offset` of the result.
void table_entry_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 entry = ctx.gpr[5] * 4u + ctx.gpr[4];
    const u32 found = call(rt, ctx, 0x08927E44u, {word(rt, entry + 8u)});
    ctx.gpr[2] = word(rt, found + offset);
}
// A command {kind} posted to the object at word(self + 0x18) (0x088936A4).
void post_kind(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 kind, u32 slot) {
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

void func_08817960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08817924u, 0u);
}
void func_088212D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load16(G[4]); G[3] = 0u; G[4] += 2u; if (G[2] != 0u) { do { G[2] = load16(G[4]); G[3] += 1u; G[4] += 2u; } while (G[2] != 0u); } G[2] = G[3]; }
void func_08829320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; } jump(ctx, 0x08B08AE0u); }
void func_0883B820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[7] = frame.at(4u); call_here(rt, ctx, 0x0883B6D4u); G[4] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x0892C880u); }
void func_0885CBA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = load8(G[4] + 0x17u); if (G[2] != 0u) return; const bool set = load8(G[4] + 0x16u) != 0u; G[2] = 1u; if (set) return; store8(G[4] + 0x15u, 1u); }
void func_0885E524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 6; G[2] = word(rt, G[4]); G[5] = (G[5] << 4) + G[3] + 0x4120u; G[2] += G[5]; G[3] = load8(G[2] + 0x17u); G[2] += 6u; if (G[3] == 0u) G[2] = 0u; }
void func_0885F028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x5ACu); G[4] = G[3]; if (G[3] == 0u) return; jump(ctx, 0x088D6984u); }
void func_0885F048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x5ACu); G[4] = G[3]; if (G[3] == 0u) return; jump(ctx, 0x088D6994u); }
void func_088756C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[3] + 0x20u); call_here(rt, ctx, G[2]); G[2] += 1u; }
void func_0887AA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = 0x200000u; G[3] = 0x8000u; G[2] = 0x18u; if (static_cast<std::int32_t>(G[5]) > 0x18) G[5] = 0x18u; if (G[6] != 0u) G[8] = G[6]; if (G[7] != 0u) G[3] = G[7]; put(rt, G[4] + 0x20u, G[3]); put(rt, G[4] + 0x18u, G[5]); put(rt, G[4] + 0x1Cu, G[8]); }
void func_0888B224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    post_kind(rt, ctx, 1u, frame.at(0x0u));
}
void func_0888B488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    post_kind(rt, ctx, 0x19u, frame.at(0x0u));
}
void func_0888B800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    post_kind(rt, ctx, 0x27u, frame.at(0x0u));
}
void func_08891424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x481907FFu; put(rt, G[4] + 0x54u, G[2]); store8(G[4] + 0x6Cu, 0u); put(rt, G[4] + 0x50u, G[2]); put(rt, G[4] + 0x5Cu, 0u); put(rt, G[4] + 0x60u, 0u); store8(G[4] + 0x68u, 0u); store8(G[4] + 0x69u, 0u); }
void func_08891460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08891424u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x0889144Cu); }
void func_0889349C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4] + 0x30u); G[2] = 0u; if (G[3] == 0u) return; G[4] = word(rt, G[4] + 0x34u); call_here(rt, ctx, G[3]); }
void func_088A3F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x310u); G[3] = (G[2] << 2) + G[4]; G[6] = G[2] + 1u; G[2] = static_cast<std::int32_t>(G[2]) < 4 ? 1u : 0u; if (G[2] == 0u) return; put(rt, G[3] + 0x300u, G[5]); put(rt, G[4] + 0x310u, G[6]); }
void func_088AD9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x41Cu, bits_of(ctx.fpr[12])); G[2] = high_half(0x08CDD104u); put(rt, G[4] + 0x420u, bits_of(ctx.fpr[13])); put(rt, G[4] + 0x424u, bits_of(ctx.fpr[14])); put(rt, G[4] + 0x428u, bits_of(ctx.fpr[15])); put(rt, G[4] + 0x42Cu, bits_of(ctx.fpr[16])); ctx.fpr[0] = psp_mul(float_at(rt, 0x08CDD104u), ctx.fpr[17]); put(rt, G[4] + 0x430u, bits_of(ctx.fpr[0])); jump(ctx, 0x088AD76Cu); }
void func_088C7920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[3] = float_at(rt, G[4] + 8u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[5] + 0xCu, G[1]); put(rt, G[5], bits_of(ctx.fpr[1])); put(rt, G[5] + 4u, bits_of(ctx.fpr[2])); put(rt, G[5] + 8u, bits_of(ctx.fpr[3])); }
void func_088C98F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[3] = float_at(rt, G[4] + 8u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, G[5] + 0xCu, G[1]); put(rt, G[5], bits_of(ctx.fpr[1])); put(rt, G[5] + 4u, bits_of(ctx.fpr[2])); put(rt, G[5] + 8u, bits_of(ctx.fpr[3])); }
void func_088CE544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 4u); G[5] = 0u; if (G[4] != 0u) G[5] = word(rt, G[4] + 0x38u); { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), 0u); } G[2] = G[5]; G[3] = 0u; }
void func_088E4380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; G[5] = 4u; do { G[2] = word(rt, G[4]) + G[3]; G[3] += 1u; store8(G[2] + 0x4ACu, 0u); } while (G[3] != G[5]); G[2] = word(rt, G[4]); store8(G[2] + 0x52Bu, 0u); }
void func_088E48E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[6] = 0u; G[2] = word(rt, G[4] + 0x200u); G[7] = 0u; if (G[2] != 0u) { G[6] = word(rt, G[4] + 0x208u); G[7] = word(rt, G[4] + 0x20Cu); } G[2] = G[6]; G[3] = G[7]; }
void func_088E4924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; } jump(ctx, 0x08B08AC0u); }
void func_088E4A30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = load8(G[4] + 0x554u); G[3] = 0u; if (G[2] == 0u) { G[2] = load8(G[4] + 0x525u); if (G[2] != 0u) { G[2] = 0u; return; } } G[3] = 1u; G[2] = 1u; }
void func_088E4A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_virtual(rt, ctx, word(rt, ctx.gpr[4]), 0x58u);
}
void func_088E4A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[4] = word(rt, G[4]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x5Cu); call_here(rt, ctx, G[2]); }
void func_088E50F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 0xFFFFFFFFu; G[3] = word(rt, G[4] + 0x548u); G[5] = 0u; if (G[3] != G[2]) { G[2] = word(rt, G[4] + 0x54Cu) ^ G[3]; G[5] = G[2] == 0u ? 1u : 0u; } G[2] = G[5]; }
void func_088E89A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x2E4u); G[2] = 1u; if (G[5] == 0u) return; G[3] = word(rt, G[4] + 0x1Cu); if (G[3] != G[2]) return; G[2] = sext(load8(G[5] + 0x3Cu), 8u); put(rt, G[4] + 0x2B8u, G[2]); }
void func_088E89E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x13u); if (G[2] == 0u) return; G[2] = load8(G[4] + 0x14u); if (G[2] == 0u) return; store8(G[4] + 0x14u, 0u); store8(G[4] + 0x13u, 0u); }
void func_088EABA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA4u); call_here(rt, ctx, G[2]); }
void func_088EB6C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_virtual(rt, ctx, word(rt, ctx.gpr[4] + 0x538u), 0xE0u);
}
void func_088EB6EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_virtual(rt, ctx, word(rt, ctx.gpr[4] + 0x538u), 0xE4u);
}
void func_088ECB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x11Cu); G[5] = (G[5] << 1) + G[2]; G[3] = 1u; if (G[2] != 0u) { G[2] = sext(load16(G[5]), 16u) + 1u; G[3] = G[2] != 0u ? 1u : 0u; } G[2] = G[3]; }
void func_088ED494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store8(G[4], 0u); G[2] = 0u; store8(G[4] + 1u, 0u); store8(G[4] + 2u, 0u); store8(G[4] + 3u, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0xCu, 0u); }
void func_088EF448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x088EF828u); store8(G[16] + 0x92u, 0u); } G[16] = s0; }
void func_088F0F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; G[7] = G[8] & 0xFFu; call_with(rt, ctx, 0x088DA6A8u, 0x10u); }
void func_088F1C98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = load8(G[3] + 0x9C4u); if (G[2] == 0u) return; store8(G[3] + 0x9C4u, 0u); G[4] = word(rt, G[4] + 8u) + 0x844u; jump(ctx, 0x088F60DCu); }
void func_089000CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08963D70u, 0x4B8u);
}
void func_0891156C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; G[1] = 0x3F000000u; ctx.fpr[0] = 0.5f; store8(G[4] + 0x14u, 1u); put(rt, G[4] + 0x10u, G[1]); put(rt, G[4], bits_of(ctx.fpr[12])); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); }
void func_08917980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    table_entry_word(rt, ctx, 0x6Cu);
}
void func_089179A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    table_entry_word(rt, ctx, 0x78u);
}
void func_089179D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    table_entry_word(rt, ctx, 0x84u);
}
void func_08918040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 2) + G[4]; G[4] = word(rt, G[5] + 8u); call_with(rt, ctx, 0x089277B4u, 0x10u); G[2] += 0x10u; }
void func_08918148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 3u; G[4] = word(rt, G[4] + 0xCu); call_with(rt, ctx, 0x089277A4u, 0x10u); G[2] = load8(G[2] + 0xC0u) != 0u ? 1u : 0u; }
void func_089200CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08920064u, 0x244u);
}
void func_08926AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x1E4u); G[4] += 0x1E4u; put(rt, G[5], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[4] + 4u); put(rt, G[5] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[4] + 8u); put(rt, G[5] + 8u, bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[4] + 0xCu); put(rt, G[5] + 0xCu, bits_of(ctx.fpr[0])); }
void func_0892B240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4] - 8u; G[4] = G[4] < 8u ? 1u : 0u; G[5] = G[2] < 8u ? 1u : 0u; G[3] = 0u; if (G[4] == 0u) { G[3] = 1u; G[2] = 0xFFFFFFFFu; if (G[5] == 0u) G[3] = G[2]; } G[2] = G[3]; }
void func_0892B724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0xCu, G[6]); put(rt, G[4] + 0x10u, G[5]); store8(G[4] + 0x17u, 0u); store8(G[4] + 4u, 0u); put(rt, G[4] + 8u, G[5]); put(rt, G[4], 0u); store8(G[4] + 0x14u, 0u); store8(G[4] + 0x15u, 0u); store8(G[4] + 0x16u, 0u); }
void func_089324B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[2] = 0xFFFFFFFFu; G[5] = 0u; if (G[3] == G[2]) { G[2] = word(rt, G[4] + 0xCu); if (G[2] == G[3]) { G[2] = 0u; return; } } G[5] = 1u; G[2] = 1u; }
void func_08932570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x100u); G[2] = 0xFFFFFFFFu; G[5] = 0u; if (G[3] == G[2]) { G[2] = word(rt, G[4] + 0x104u); if (G[2] == G[3]) G[5] = 1u; } G[2] = (G[5] ^ 1u) & 1u; }
void func_0893651C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = (G[6] << 2) + G[4]; G[3] = word(rt, G[6] + 0x8Cu); G[2] = 0u; G[4] = G[3]; if (G[3] == 0u) return; jump(ctx, 0x08938018u); }
void func_0894F130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[1] = std::bit_cast<float>(G[4]); ctx.fpr[2] = 1.0f; ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[4])); G[1] = 0x3E8E38E4u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[2] / ctx.fpr[0]; }
void func_08954AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[3] = load8(G[2] + 0x778u); G[2] = 0x08CF334Cu; G[4] = (G[3] << 5) - (G[3] << 2) + G[2]; G[3] <<= 2; ctx.fpr[0] = float_at(rt, G[4] + 0x10u); }
void func_089771F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[2] = ((((G[5] << 4) - G[3] - G[5]) << 1) + G[6]) << 2; G[2] += G[4]; ctx.fpr[0] = float_at(rt, G[2] + 0x94u); }
void func_08977290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[2] = ((((G[5] << 4) - G[3] - G[5]) << 1) + G[6]) << 2; G[2] += G[4]; ctx.fpr[0] = float_at(rt, G[2] + 0xECu); }
void func_0898D9C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x4A8u); ctx.fpr[12] = float_at(rt, G[4] + 0x4B8u); ctx.fpr[1] = float_at(rt, G[4] + 0x4ACu); ctx.fpr[14] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[0]))); ctx.fpr[15] = 0.0f; ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[1]))); ctx.fpr[12] = ctx.fpr[14] - ctx.fpr[12]; ctx.fpr[13] = ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[0]; jump(ctx, 0x08995064u); }
void func_089E4EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x19Cu); G[2] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[3]) < 0) return; G[4] = word(rt, G[4] + 0x158u); if (static_cast<std::int32_t>(G[4]) <= 0) return; divide(ctx, G[3], G[4]); G[2] = ctx.lo; }
void func_089E4EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x19Cu); G[2] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[3]) < 0) return; G[4] = word(rt, G[4] + 0x158u); if (static_cast<std::int32_t>(G[4]) <= 0) return; divide(ctx, G[3], G[4]); G[2] = ctx.hi; }
void func_089E5814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x180u); G[5] <<= 2; G[2] = (G[3] << 4) - (G[3] << 2) + G[4]; G[3] = word(rt, G[2] + 0x168u) + G[5]; G[2] = word(rt, G[3]); }
void func_089E5A1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x180u); G[5] <<= 2; G[2] = (G[3] << 4) - (G[3] << 2) + G[4]; G[3] = word(rt, G[2] + 0x168u) + G[5]; G[2] = word(rt, G[3]); }
void func_08A0ADE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = frame.at(4u); G[7] = frame.at(8u); call_here(rt, ctx, 0x08A0AD9Cu); ctx.fpr[0] = float_at(rt, frame.at(0u)); }
void func_08A0AE10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = frame.at(4u); G[7] = frame.at(8u); call_here(rt, ctx, 0x08A0AD9Cu); ctx.fpr[0] = float_at(rt, frame.at(4u)); }
void func_08A0AE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = frame.at(4u); G[7] = frame.at(8u); call_here(rt, ctx, 0x08A0AD9Cu); ctx.fpr[0] = float_at(rt, frame.at(8u)); }
void func_08A24A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0x14u); jump(ctx, 0x08A24A2Cu); }
void func_08A24B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4] + 0x14u); jump(ctx, 0x08A24AE4u); }
void func_08A4CE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08A4CDF4u, 0x3Cu);
}
void func_08A4CE60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08A4CDF4u, 0x40u);
}
void func_08A4CEB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    store_result(rt, ctx, 0x08A4CDF4u, 0x44u);
}
void func_08A65658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 9u; store8(G[4] + 4u, 9u); G[2] = 0x08CD29F8u; G[3] = word(rt, G[4] + 4u) & 0xFFu; put(rt, G[4] + 8u, bits_of(ctx.fpr[12])); put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); }
void func_08A68B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x41u; store8(G[4] + 4u, 0x41u); G[2] = 0x08CD35E8u; G[3] = word(rt, G[4] + 4u) & 0xFFu; store8(G[4] + 8u, G[5]); put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); }
void func_08A6C028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = sign8(raw);
}
void func_08A6C050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = sign16(raw);
}
void func_08A6C0C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = raw & 0xFFu;
}
void func_08A6C0E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 raw = query_slot(rt, ctx, frame.at(0x0u));
    ctx.gpr[2] = raw & 0xFFFFu;
}
void func_08A6C188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = query_member(rt, ctx);
    ctx.gpr[2] = sign8(raw);
}
void func_08A6C1B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = query_member(rt, ctx);
    ctx.gpr[2] = raw & 0xFFu;
}
void func_08A6C1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = query_member(rt, ctx);
    ctx.gpr[2] = sign16(raw);
}
void func_08A6C200(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 raw = query_member(rt, ctx);
    ctx.gpr[2] = raw & 0xFFFFu;
}
void func_08AC08D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[5]; G[5] = frame.at(0u); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08AC0880u); }
void func_08AC2F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[5] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[4]); G[3] = word(rt, G[2]); G[25] = word(rt, G[3] + 0x34u); G[4] = G[2]; jump(ctx, G[25]); }
void func_08AC2FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[5] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[4]); G[3] = word(rt, G[2]); G[25] = word(rt, G[3] + 0x3Cu); G[4] = G[2]; jump(ctx, G[25]); }
void func_08AC718C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AC7140u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_08ACA438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[4]; G[6] = G[5]; G[4] = G[2]; if (G[2] == 0u) { G[2] = 0x80001002u; return; } G[5] = word(rt, G[3] + 4u); jump(ctx, 0x08ACA378u); }
void func_08ACBA58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = load8(G[4] + 0xA4u); G[3] = 1u; if (G[2] == 0u) { G[2] = load8(G[4] + 0xA5u); if (G[2] == 0u) { G[2] = 1u; return; } } G[3] = 0u; G[2] = 0u; }
void func_08ACCD34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D00408u); put(rt, 0x08D00408u, G[6]); G[8] = G[7]; G[7] = 0x08D00408u; G[6] = word(rt, G[5] + 0xD0u); G[4] = word(rt, G[5] + 0xB4u); G[5] = word(rt, G[5] + 0xBCu); jump(ctx, 0x08C7AD20u); }
void func_08AD08D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B1A8D4u); call_here(rt, ctx, 0x08B1AB08u); } G[2] = 0u; }
void func_08AD45C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 4) + 0x90u; ctx.fpr[0] = std::bit_cast<float>(trunc_w_s(ctx.fpr[12])); G[2] = G[4] + G[5]; G[3] = G[2] + 0xCu; G[6] &= 0xFFu; put(rt, G[2] + 0xCu, G[6]); put(rt, G[3] + 0xCu, bits_of(ctx.fpr[13])); put(rt, G[3] + 4u, bits_of(ctx.fpr[0])); jump(ctx, 0x08AD44B4u); }
void func_08AEB7BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D41AE0u; G[5] = 1u; G[6] = 0u; call_with(rt, ctx, 0x08C7A8B0u, 0x10u); }
void func_08AEE5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C924B4u; G[6] = 6u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] < 1u ? 1u : 0u; }
void func_08AEE5C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C924BCu; G[6] = 4u; call_with(rt, ctx, 0x08B36170u, 0x10u); G[2] = G[2] < 1u ? 1u : 0u; }
void func_08AFACA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AFAC58u); G[4] = G[16] + 0x2Cu; } G[16] = s0; jump(ctx, 0x08B52910u); }
void func_08B0997C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[2] = 1u; G[5] = G[5] < 4u ? 1u : 0u; if (G[6] == 0u) G[6] = G[2]; if (G[5] == 0u) return; G[2] = word(rt, G[4]) + G[3]; put(rt, G[2] + 0xC4u, G[6]); }
void func_08B0D800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD7B98u; put(rt, G[4], G[2]); G[5] = high_half(0x08D7734Cu); G[3] = G[4] + 4u; G[2] = word(rt, 0x08D7734Cu); put(rt, G[3] + 4u, G[4]); put(rt, G[4] + 4u, G[2]); put(rt, 0x08D7734Cu, G[3]); }
void func_08B114A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x08B11434u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B10A40u); }
void func_08B11A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0CE24u); put(rt, G[16] + 0x3Cu, 0u); } G[16] = s0; }
void func_08B120E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08C7B200u); G[4] = G[16] + 0x4D8u; } G[16] = s0; jump(ctx, 0x08B2EE2Cu); }
void func_08B208C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[3] = 0u; if (G[2] != 0u) { G[4] += 0xCu; do { G[2] = word(rt, G[4]); G[3] += 1u; G[4] += 8u; } while (G[2] != 0u); } G[2] = G[3]; }
void func_08B27F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] + 0x2ABCu; G[3] = G[5] + 0x200u; G[6] = G[5] + 0x33Cu; G[7] = G[5] + 0x26FCu; put(rt, G[4] + 0x50u, G[2]); put(rt, G[4] + 0x34u, G[3]); put(rt, G[4] + 0x40u, G[6]); put(rt, G[4] + 0x48u, G[7]); put(rt, G[4] + 0x14u, G[5]); }
void func_08B28068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0x60u); G[3] = 0xFFFDu; G[4] = 0xFFFFu; G[2] = G[3] < G[6] - 1u ? 1u : 0u; if (G[2] != 0u) G[6] = G[4]; G[3] = G[5] < G[6] ? 1u : 0u; G[2] = G[5]; if (G[3] == 0u) G[2] = G[6]; }
void func_08B2B3F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDA6C0u; put(rt, G[4], G[2]); G[5] = high_half(0x08D773F4u); G[3] = G[4] + 4u; G[2] = word(rt, 0x08D773F4u); put(rt, G[3] + 4u, G[4]); put(rt, G[4] + 4u, G[2]); put(rt, 0x08D773F4u, G[3]); }
void func_08B2C4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 member = word(rt, self + 4u);
    call(rt, ctx, 0x08B2C2E0u, {self});
    ctx.gpr[2] = member;
}
void func_08B2E964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D690C4u; call_with(rt, ctx, 0x08B346BCu, 0x10u); G[2] = 0x1046Au; }
void func_08B33ACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B33A7Cu, 0x10u); if (G[2] == 0u) return; jump(ctx, 0x08B33948u); }
void func_08B388D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = 0x08CB5584u; call_with(rt, ctx, 0x08B3A3D0u, 0x10u); G[2] = 0xEu; }
void func_08B3F8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4] + 0x10u; G[2] = load8(G[5]) | load8(G[5] + 1u) << 8 | load8(G[5] + 2u) << 16 | load8(G[5] + 3u) << 24; for (unsigned i = 0u; i < 4u; ++i) store8(G[6] + i, G[2] >> (8u * i)); G[3] = load8(G[5] + 4u); store8(G[6] + 4u, G[3]); G[2] = load8(G[5] + 5u); store8(G[6] + 5u, G[2]); }
void func_08B402AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B40238u); store8(G[16] + 0xB6u, G[2]); } G[16] = s0; }
void func_08B4891C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[5] + G[2]; if (G[2] != 0u) put(rt, G[4], G[3]); G[2] = word(rt, G[4] + 4u); G[5] += G[2]; if (G[2] != 0u) put(rt, G[4] + 4u, G[5]); }
void func_08B4A610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[0] = float_at(rt, G[4] + 8u); ctx.fpr[1] = ctx.fpr[1] / ctx.fpr[12]; ctx.fpr[2] = ctx.fpr[2] / ctx.fpr[12]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[12]; put(rt, G[4], bits_of(ctx.fpr[1])); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); put(rt, G[4] + 4u, bits_of(ctx.fpr[2])); }
void func_08B551A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D6CF38u); if (G[4] != 0u) { ctx.fpr[0] = float_at(rt, 0x08D6CF38u); put(rt, G[4], bits_of(ctx.fpr[0])); } if (G[5] == 0u) return; ctx.fpr[0] = float_at(rt, 0x08D6CF3Cu); put(rt, G[5], bits_of(ctx.fpr[0])); }
void func_08B55678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x08D6CE00u; G[6] = word(rt, G[3] + 0x5Cu); G[2] = 0x12000000u | (G[4] & 0xFFFFFFu); G[5] = G[6] + 4u; put(rt, G[3] + 0x2DCu, G[4]); put(rt, G[6], G[2]); put(rt, G[3] + 0x5Cu, G[5]); }
void func_08B577D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (((G[6] << 1) + G[6] + G[5]) << 24) | (G[7] & 0xFFFFFFu); G[6] = word(rt, G[4] + 0x5Cu); G[3] = G[6] + 4u; put(rt, G[6], G[2]); put(rt, G[4] + 0x5Cu, G[3]); }
void func_08B577F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = bits_of(ctx.fpr[12]); G[7] = word(rt, G[4] + 0x5Cu); G[2] = G[3] >> 8; G[5] = ((G[6] + G[5]) << 24) | (G[2] & 0xFFFFFFu); G[3] = G[7] + 4u; put(rt, G[7], G[5]); put(rt, G[4] + 0x5Cu, G[3]); }
void func_08B59F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B59ED0u, {self});
    ctx.gpr[2] = half_at(rt, self + 0x208u);
}
void func_08B5C3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[5] = 1u; put(rt, G[4], G[2]); G[4] = G[2]; call_with(rt, ctx, 0x08B6085Cu, 0x10u); }
void func_08B5C450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); G[5] = 1u; put(rt, G[4], G[2]); G[4] = G[2]; call_with(rt, ctx, 0x08B6085Cu, 0x10u); }
void func_08B646A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[7] - G[6]; G[2] = G[6] < G[8] ? 1u : 0u; G[3] = G[2] != 0u ? frame.at(0u) : frame.at(0x10u); put(rt, frame.at(0x10u), G[8]); put(rt, frame.at(0u), G[6]); G[2] = word(rt, G[3]); }
void func_08B655C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08D72B98u; G[5] = 1u; call_with(rt, ctx, 0x08B60804u, 0x10u); G[2] += 4u; }
void func_08B807D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4] + 0x08C96BD9u; G[4] = G[4] < 0x100u ? 1u : 0u; G[2] = 0u; if (G[4] == 0u) return; G[2] = load8(G[3]) & 8u; }
void func_08B84698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = high_half(0x08D740F8u); G[2] = word(rt, 0x08D740F8u); G[3] = 0x80420000u; G[6] = 1u; G[4] = 0x80420100u; if (G[2] == G[6]) { put(rt, 0x08D740F8u, 0u); G[4] = 0u; } G[2] = G[4]; }
void func_08B89DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u) + G[5]; G[6] = word(rt, G[4] + 0xCu); G[7] = G[2] < 0x64u ? 1u : 0u; G[3] = G[2] - 0x64u; if (G[7] == 0u) G[2] = G[3]; G[6] -= G[5]; put(rt, G[4] + 8u, G[2]); put(rt, G[4] + 0xCu, G[6]); }
void func_08BA5290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[5] == 0u) return; do { const bool skip = G[4] == 0u; G[2] += 1u; if (!skip) { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); } G[4] += 4u; } while (G[5] != G[2]); }
void func_08BA52E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[5] == 0u) return; do { const bool skip = G[4] == 0u; G[2] += 1u; if (!skip) { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); } G[4] += 4u; } while (G[5] != G[2]); }
void func_08BAD950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5]); put(rt, G[2], bits_of(ctx.fpr[0])); ctx.fpr[0] = float_at(rt, G[5] + 4u); G[3] = word(rt, G[4]); put(rt, G[3] + 4u, bits_of(ctx.fpr[0])); G[2] = word(rt, G[4]); ctx.fpr[0] = float_at(rt, G[5] + 8u); put(rt, G[2] + 8u, bits_of(ctx.fpr[0])); }
void func_08BD2A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2]); put(rt, G[4], bits_of(ctx.fpr[0])); G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); }
void func_08BEF150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[5] == 0u) return; do { const bool skip = G[4] == 0u; G[2] += 1u; if (!skip) { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); } G[4] += 4u; } while (G[5] != G[2]); }
void func_08BF8BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; if (G[5] == 0u) return; for (;;) { divide(ctx, G[2], G[5]); G[3] = ctx.hi; G[2] = G[5]; if (G[3] == 0u) return; G[5] = G[3]; } }
void func_08BF8D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; if (G[5] == 0u) return; do { const bool skip = G[4] == 0u; G[3] += 1u; if (!skip) { G[2] = word(rt, G[6]); put(rt, G[4], G[2]); } G[4] += 4u; } while (G[5] != G[3]); }
void func_08BF9218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0u; if (G[5] == 0u) return; do { const bool skip = G[4] == 0u; G[3] += 1u; if (!skip) { G[2] = word(rt, G[6]); put(rt, G[4], G[2]); } G[4] += 4u; } while (G[5] != G[3]); }
void func_08C1A114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[5] &= 0xFFu; G[4] = G[2]; if (G[2] == 0u) return; G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0xCu); jump(ctx, G[25]); }
void func_08C3CF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = word(rt, G[6]); if (G[5] == 0u) return; G[3] = 0u; do { G[3] += 1u; put(rt, G[2], G[6]); G[2] += 4u; } while (G[3] != G[5]); }
void func_08C3D074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[6]); G[2] = G[4]; if (G[5] == 0u) return; G[3] = 0u; do { G[3] += 1u; put(rt, G[2], G[6]); G[2] += 4u; } while (G[5] != G[3]); }
void func_08C3FB94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2]); put(rt, G[4], bits_of(ctx.fpr[0])); G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); G[2] = word(rt, G[5]); ctx.fpr[0] = float_at(rt, G[2] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); }
void func_08C503AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = word(rt, G[6]); if (G[5] == 0u) return; G[3] = 0u; do { G[3] += 1u; put(rt, G[2], G[6]); G[2] += 4u; } while (G[3] != G[5]); }
void func_08C55D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], 0x08CDB174u); G[3] = 0x08CDB088u; G[2] = G[4]; G[4] += 8u; put(rt, G[2] + 8u, G[3]); put(rt, G[2] + 4u, 0u); jump(ctx, 0x08B6543Cu); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_174
} // namespace gt::game

#define HOT174(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_174::name, gt::ReturnKind::kind)
HOT174(0x08817960u, func_08817960, Word);
HOT174(0x088212D0u, func_088212D0, Word);
HOT174(0x08829320u, func_08829320, Word);
HOT174(0x0883B820u, func_0883B820, Word);
HOT174(0x0885CBA4u, func_0885CBA4, Word);
HOT174(0x0885E524u, func_0885E524, Word);
HOT174(0x0885F028u, func_0885F028, Word);
HOT174(0x0885F048u, func_0885F048, Word);
HOT174(0x088756C8u, func_088756C8, Word);
HOT174(0x0887AA74u, func_0887AA74, Word);
HOT174(0x0888B224u, func_0888B224, Word);
HOT174(0x0888B488u, func_0888B488, Word);
HOT174(0x0888B800u, func_0888B800, Word);
HOT174(0x08891424u, func_08891424, Word);
HOT174(0x08891460u, func_08891460, Word);
HOT174(0x0889349Cu, func_0889349C, Word);
HOT174(0x088A3F98u, func_088A3F98, Word);
HOT174(0x088AD9B8u, func_088AD9B8, Word);
HOT174(0x088C7920u, func_088C7920, Word);
HOT174(0x088C98F0u, func_088C98F0, Word);
HOT174(0x088CE544u, func_088CE544, Word);
HOT174(0x088E4380u, func_088E4380, Word);
HOT174(0x088E48E8u, func_088E48E8, DoubleWord);
HOT174(0x088E4924u, func_088E4924, Word);
HOT174(0x088E4A30u, func_088E4A30, Word);
HOT174(0x088E4A58u, func_088E4A58, Word);
// HOT174(0x088E4A80u, func_088E4A80, Word);  // no comparable synthetic case yet
HOT174(0x088E50F4u, func_088E50F4, Word);
HOT174(0x088E89A4u, func_088E89A4, Word);
HOT174(0x088E89E4u, func_088E89E4, Word);
// HOT174(0x088EABA0u, func_088EABA0, Word);  // no comparable synthetic case yet
HOT174(0x088EB6C4u, func_088EB6C4, Word);
HOT174(0x088EB6ECu, func_088EB6EC, Word);
HOT174(0x088ECB1Cu, func_088ECB1C, Word);
HOT174(0x088ED494u, func_088ED494, Word);
HOT174(0x088EF448u, func_088EF448, Word);
HOT174(0x088F0F24u, func_088F0F24, Word);
HOT174(0x088F1C98u, func_088F1C98, Word);
HOT174(0x089000CCu, func_089000CC, Word);
HOT174(0x0891156Cu, func_0891156C, Word);
HOT174(0x08917980u, func_08917980, Word);
HOT174(0x089179A8u, func_089179A8, Word);
HOT174(0x089179D0u, func_089179D0, Word);
HOT174(0x08918040u, func_08918040, Word);
HOT174(0x08918148u, func_08918148, Word);
HOT174(0x089200CCu, func_089200CC, Word);
HOT174(0x08926AB8u, func_08926AB8, Word);
HOT174(0x0892B240u, func_0892B240, Word);
HOT174(0x0892B724u, func_0892B724, Word);
HOT174(0x089324B4u, func_089324B4, Word);
HOT174(0x08932570u, func_08932570, Word);
HOT174(0x0893651Cu, func_0893651C, Word);
HOT174(0x0894F130u, func_0894F130, Float);
HOT174(0x08954AB8u, func_08954AB8, Float);
HOT174(0x089771F4u, func_089771F4, Float);
HOT174(0x08977290u, func_08977290, Float);
HOT174(0x0898D9C0u, func_0898D9C0, Word);
HOT174(0x089E4EA0u, func_089E4EA0, Word);
HOT174(0x089E4EC8u, func_089E4EC8, Word);
HOT174(0x089E5814u, func_089E5814, Word);
HOT174(0x089E5A1Cu, func_089E5A1C, Word);
HOT174(0x08A0ADE8u, func_08A0ADE8, Float);
HOT174(0x08A0AE10u, func_08A0AE10, Float);
HOT174(0x08A0AE38u, func_08A0AE38, Float);
HOT174(0x08A24A4Cu, func_08A24A4C, Word);
HOT174(0x08A24B04u, func_08A24B04, Word);
HOT174(0x08A4CE38u, func_08A4CE38, Word);
HOT174(0x08A4CE60u, func_08A4CE60, Word);
HOT174(0x08A4CEB4u, func_08A4CEB4, Word);
HOT174(0x08A65658u, func_08A65658, Word);
HOT174(0x08A68B54u, func_08A68B54, Word);
HOT174(0x08A6C028u, func_08A6C028, Word);
HOT174(0x08A6C050u, func_08A6C050, Word);
HOT174(0x08A6C0C0u, func_08A6C0C0, Word);
HOT174(0x08A6C0E8u, func_08A6C0E8, Word);
HOT174(0x08A6C188u, func_08A6C188, Word);
HOT174(0x08A6C1B0u, func_08A6C1B0, Word);
HOT174(0x08A6C1D8u, func_08A6C1D8, Word);
HOT174(0x08A6C200u, func_08A6C200, Word);
HOT174(0x08AC08D4u, func_08AC08D4, Word);
// HOT174(0x08AC2F90u, func_08AC2F90, Word);  // no comparable synthetic case yet
HOT174(0x08AC2FB8u, func_08AC2FB8, Word);
HOT174(0x08AC718Cu, func_08AC718C, Word);
HOT174(0x08ACA438u, func_08ACA438, Word);
HOT174(0x08ACBA58u, func_08ACBA58, Word);
// HOT174(0x08ACCD34u, func_08ACCD34, Word);  // no comparable synthetic case yet
// HOT174(0x08AD08D8u, func_08AD08D8, Word);  // no comparable synthetic case yet
HOT174(0x08AD45C4u, func_08AD45C4, Word);
HOT174(0x08AEB7BCu, func_08AEB7BC, Word);
HOT174(0x08AEE5A0u, func_08AEE5A0, Word);
HOT174(0x08AEE5C8u, func_08AEE5C8, Word);
HOT174(0x08AFACA4u, func_08AFACA4, Word);
HOT174(0x08B0997Cu, func_08B0997C, Word);
HOT174(0x08B0D800u, func_08B0D800, Word);
// HOT174(0x08B114A4u, func_08B114A4, Word);  // no comparable synthetic case yet
HOT174(0x08B11A98u, func_08B11A98, Word);
// HOT174(0x08B120E0u, func_08B120E0, Word);  // no comparable synthetic case yet
HOT174(0x08B208C8u, func_08B208C8, Word);
HOT174(0x08B27F4Cu, func_08B27F4C, Word);
HOT174(0x08B28068u, func_08B28068, Word);
HOT174(0x08B2B3F8u, func_08B2B3F8, Word);
HOT174(0x08B2C4DCu, func_08B2C4DC, Word);
HOT174(0x08B2E964u, func_08B2E964, Word);
HOT174(0x08B33ACCu, func_08B33ACC, Word);
HOT174(0x08B388D4u, func_08B388D4, Word);
HOT174(0x08B3F8C0u, func_08B3F8C0, Word);
HOT174(0x08B402ACu, func_08B402AC, Word);
HOT174(0x08B4891Cu, func_08B4891C, Word);
HOT174(0x08B4A610u, func_08B4A610, Word);
HOT174(0x08B551A8u, func_08B551A8, Word);
HOT174(0x08B55678u, func_08B55678, Word);
HOT174(0x08B577D0u, func_08B577D0, Word);
HOT174(0x08B577F8u, func_08B577F8, Word);
HOT174(0x08B59F44u, func_08B59F44, Word);
HOT174(0x08B5C3F0u, func_08B5C3F0, Word);
HOT174(0x08B5C450u, func_08B5C450, Word);
HOT174(0x08B646A8u, func_08B646A8, Word);
HOT174(0x08B655C4u, func_08B655C4, Word);
HOT174(0x08B807D4u, func_08B807D4, Word);
HOT174(0x08B84698u, func_08B84698, Word);
HOT174(0x08B89DACu, func_08B89DAC, Word);
HOT174(0x08BA5290u, func_08BA5290, Word);
HOT174(0x08BA52E8u, func_08BA52E8, Word);
HOT174(0x08BAD950u, func_08BAD950, Word);
HOT174(0x08BD2A58u, func_08BD2A58, Word);
HOT174(0x08BEF150u, func_08BEF150, Word);
HOT174(0x08BF8BD4u, func_08BF8BD4, Word);
HOT174(0x08BF8D94u, func_08BF8D94, Word);
HOT174(0x08BF9218u, func_08BF9218, Word);
HOT174(0x08C1A114u, func_08C1A114, Word);
HOT174(0x08C3CF6Cu, func_08C3CF6C, Word);
HOT174(0x08C3D074u, func_08C3D074, Word);
HOT174(0x08C3FB94u, func_08C3FB94, Word);
HOT174(0x08C503ACu, func_08C503AC, Word);
// HOT174(0x08C55D4Cu, func_08C55D4C, Word);  // no comparable synthetic case yet
