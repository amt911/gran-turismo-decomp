// Hot batch 181: sixteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_181 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Families of identical shape: each registration keeps its own thin function (and its own frame), the constants differ.
void init_with_source(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    u32 value = 0u;
    ctx.gpr[2] = 0u;
    put(rt, self, source);
    if (source != 0u) value = call(rt, ctx, 0x0885EE58u, {source});
    put(rt, self + 4u, value);
    set_byte(rt, self + 8u, 0u);
}
// Spin until `ready(word(word(self) + 0x5AC))` is non-zero, sleeping (0x08B2E2B8, 1000) between tries.
void wait_until(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 ready) {
    const u32 self = ctx.gpr[4];
    while (call(rt, ctx, ready, {word(rt, word(rt, self) + 0x5ACu)}) == 0u) call(rt, ctx, 0x08B2E2B8u, {0x3E8u});
}
void clear_low_bits_and_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 aligned = word(rt, self + 0x18u) & ~3u;
    put(rt, self, 0x08CC2F78u);
    put(rt, self + 0x18u, aligned);
    put(rt, self + 0x28u, 0u);
    for (const u32 offset : {4u, 8u, 0xCu, 0x10u, 0x14u, 0x1Cu, 0x20u, 0x24u}) put(rt, self + offset, 0u);
    ctx.gpr[2] = 0x08CC2F78u;
}
// Dispatch through the virtual at +8 of the object `0x088DA5F4(word(self + 4))` returns; the result is its `this`.
u32 dispatch_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 &object) {
    object = call(rt, ctx, 0x088DA5F4u, {word(rt, ctx.gpr[4] + 4u)});
    if (object == 0u) return 0u;
    return word(rt, word(rt, object) + 8u);
}
void set_flag_and_half(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 half_offset, u32 flag) {
    const u32 self = ctx.gpr[4];
    const u32 half = call(rt, ctx, 0x0892A958u, {ctx.gpr[5]});
    if (half != 0u) {
        const u32 flags = byte_at(rt, self) | flag;
        set_half(rt, self + half_offset, half);
        set_byte(rt, self, flags);
        ctx.gpr[2] = flags;
    }
}
void construct_with_two_vtables(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 base, u32 mode, u32 vtable, u32 vtable_38) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, base, {self, mode});
    put(rt, self + 0x38u, vtable_38);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable_38;
}
// vtable store, then the two members at `first_offset` and `first_offset - 4` released; the caller tail-calls the base.
void release_members(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable, u32 first_offset, u32 first_release, u32 second_release) {
    const u32 self = ctx.gpr[4];
    put(rt, self, vtable);
    call(rt, ctx, first_release, {self + first_offset});
    call(rt, ctx, second_release, {self + first_offset - 4u});
}
void construct_with_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self + 0x14u, value);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}
void copy_member_after_base(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    call(rt, ctx, 0x08A7A3D4u, {self});
    if (source != 0u) {
        ctx.gpr[2] = word(rt, source + 0x14u);
        put(rt, self + 0x14u, ctx.gpr[2]);
    }
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

// Lock, unlock `object` (0x08B344E0 / 0x08B3457C).
void lock_cycle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 object) {
    call(rt, ctx, 0x08B344E0u, {object});
    call(rt, ctx, 0x08B3457Cu, {object});
}
// Base constructor 0x08A76ACC, then a small value at +0x14 and the vtable (v0 keeps the vtable).
void construct_with_small_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 value, u32 vtable, unsigned width) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    ctx.gpr[2] = vtable;
    if (width == 2u) rt.memory().store16(self + 0x14u, static_cast<std::uint16_t>(value));
    else rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(value));
    put(rt, self, vtable);
}
// An empty (zeroed) range builder: +0, +4, byte +8 cleared, then `build(self, word(source + 0xC) + offset, 0)`; returns self.
void build_from_source(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 build) {
    const u32 self = ctx.gpr[4];
    const u32 begin = word(rt, ctx.gpr[5] + 0xCu) + ctx.gpr[6];
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    rt.memory().store8(self + 8u, 0u);
    call(rt, ctx, build, {self, begin, 0u});
    ctx.gpr[2] = self;
}
// `target(word(self), word(self + 4), word(self + 8), word(self + 0xC))`, then 0x08B52958(self); returns 1.
void run_command(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = word(rt, self + 4u);
    const u32 a2 = word(rt, self + 8u);
    const u32 a3 = word(rt, self + 0xCu);
    call(rt, ctx, target, {word(rt, self), a1, a2, a3});
    call(rt, ctx, 0x08B52958u, {self});
    ctx.gpr[2] = 1u;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0880AA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    init_with_source(rt, ctx);
}
void func_0880DB8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    init_with_source(rt, ctx);
}
void func_0881A880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = G[7]; G[16] = G[4]; G[4] = G[6]; call_here(rt, ctx, 0x089301C4u); G[3] = G[2] << 5; G[4] = word(rt, G[16] + 0x30u); G[2] = (G[2] << 1) + G[3] + G[4]; } G[16] = s0; }
void func_0883A45C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CE2C20u); G[2] = load8(0x08CE2C20u); if (G[2] == 0u) { call_here(rt, ctx, 0x08930864u); call_here(rt, ctx, 0x08930844u); G[3] = 1u; store8(0x08CE2C20u, 1u); } } G[16] = s0; }
void func_0885C250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] << 6; G[5] <<= 3; G[8] = word(rt, G[4]); G[2] -= G[5]; G[4] = G[2] << 3; G[3] = word(rt, G[8]); G[4] -= G[2]; G[5] = G[6]; G[4] += G[3]; G[6] = G[7]; call_with(rt, ctx, 0x0885D950u, 0x10u); }
void func_0885E7DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    wait_until(rt, ctx, 0x088D9FACu);
}
void func_0885E878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    wait_until(rt, ctx, 0x088D7FB4u);
}
void func_0885E8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    wait_until(rt, ctx, 0x088D7FF8u);
}
void func_088676B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[1] = 0x3F000000u; ctx.fpr[0] = psp_mul(ctx.fpr[14], 0.5f); ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[13]; ctx.fpr[1] = -ctx.fpr[0]; if (ctx.fpr[0] < ctx.fpr[12]) ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[14]; else if (ctx.fpr[12] < ctx.fpr[1]) ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[14]; ctx.fpr[0] = ctx.fpr[12]; }
void func_08881D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[17] = G[6] & 0xFFu; call_here(rt, ctx, 0x0888195Cu); G[5] = G[2]; G[4] = G[16]; G[6] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08881D40u); }
void func_08881FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[2] = word(rt, G[4] + 0xCu); G[3] = word(rt, G[2] + 0x4Cu); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x20u); call_here(rt, ctx, G[2]); G[5] = G[2]; } G[2] = G[5]; }
void func_088824B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[5] = word(rt, G[16] + 8u); G[4] = word(rt, G[4] + 0xCu); call_here(rt, ctx, 0x0888244Cu); G[3] = word(rt, G[16] + 0xCu); put(rt, G[3] + 0x16F8u, bits_of(ctx.fpr[20])); } G[16] = s0; ctx.fpr[20] = f20; }
void func_08884B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, self + 0x48u);
    ctx.gpr[2] = object;
    if (object != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        put(rt, self + 0x48u, 0u);
    }
}
void func_08885B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[4] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[16] + 8u); call_here(rt, ctx, 0x08885994u); ctx.fpr[12] = ctx.fpr[20]; G[4] = word(rt, G[16] + 0xCu); } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, 0x08884C80u); }
void func_088899D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], 0x08CC1950u); G[3] = G[4]; G[6] = G[4] + 0xCu; G[2] = 0x08CC198Cu; put(rt, G[3] + 8u, G[2]); G[4] = G[6]; put(rt, G[3] + 4u, 0u); put(rt, G[3] + 0xCu, 0u); put(rt, G[6] + 4u, 0u); put(rt, G[6] + 8u, 0u); put(rt, G[3] + 0x18u, G[5]); G[5] = 0x10u; jump(ctx, 0x08BA9878u); }
void func_088A36C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[3] = 0u; G[2] = word(rt, G[4] + 0x1E4u); G[4] = G[2]; if (G[2] != 0u) { G[2] = 0u; if (G[5] != 0u) G[2] = word(rt, G[5] + 0x1E4u); G[5] = G[2]; call_here(rt, ctx, 0x088AB5B4u); G[3] = G[2]; } G[2] = G[3]; }
void func_088B11D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    clear_low_bits_and_zero(rt, ctx);
}
void func_088B1214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    clear_low_bits_and_zero(rt, ctx);
}
void func_088BBE0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; call_here(rt, ctx, 0x088BBC40u); G[4] = G[16]; ctx.fpr[0] = 0.0f; if (G[2] != 0u) { call_here(rt, ctx, 0x088BBDB8u); ctx.fpr[0] = float_at(rt, G[2] + 0x14u); } } G[16] = s0; }
void func_088CE770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    u32 target;
    {
        GuestFrame frame(ctx, 0x10u);
        target = dispatch_target(rt, ctx, object);
    }
    if (object == 0u) return;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object, 0u});
}
void func_088CE7B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 object;
    u32 target;
    {
        GuestFrame frame(ctx, 0x10u);
        target = dispatch_target(rt, ctx, object);
    }
    if (object == 0u) return;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object, 1u});
}
void func_088CF2E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 1000.0f; ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[0]; G[1] = 0x3C88AB7Cu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[2] = float_at(rt, G[4] + 0x154u); ctx.fpr[0] = float_at(rt, G[4] + 0xCCu); ctx.fpr[3] = float_at(rt, G[4] + 0xD4u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; put(rt, G[4] + 0xCCu, bits_of(ctx.fpr[0])); ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[1]; ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[12]); ctx.fpr[3] = ctx.fpr[3] + ctx.fpr[2]; put(rt, G[4] + 0xD4u, bits_of(ctx.fpr[3])); }
void func_088D0934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x70u); G[2] = G[5] << 2; G[7] = G[2] + G[4]; G[3] = G[6] - G[3]; put(rt, G[7] + 0x100u, G[6]); G[4] = G[3]; put(rt, G[7] + 0xC0u, G[3]); if (static_cast<std::int32_t>(G[5]) > 0) { G[2] = word(rt, G[7] + 0xBCu); G[4] = G[3] - G[2]; } G[2] = G[4] < word(rt, G[7] + 0x144u) ? 1u : 0u; if (G[2] != 0u) put(rt, G[7] + 0x144u, G[4]); }
void func_088D4D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = 1u; G[7] = G[4]; store8(G[3] + 0x4Eu, 1u); G[6] = 0u; G[8] = 4u; do { G[2] = word(rt, G[7]); G[4] = load16(G[5]); G[3] = (G[6] << 1) + G[2]; G[6] += 1u; store16(G[3] + 0x40u, G[4]); G[5] += 2u; } while (G[6] != G[8]); }
void func_088E4BC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[5] = (G[5] << 4) + (G[5] << 7) + G[3] + 0x218u; const bool active = static_cast<std::int32_t>(sext(load16(G[5] + 2u), 16u)) > 0; G[2] = 1u; if (!active) return; store8(G[5] + 0x88u, 1u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x51Cu); store8(G[5] + 0x89u, 0u); put(rt, G[5] + 0x84u, G[2]); }
void func_088EBCBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[6] + 0x538u); put(rt, G[6] + 0x558u, G[5]); G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x74u); G[16] = G[6]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x088ED6F4u); }
void func_088ECB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[8] = frame.at(0u); G[6] = 0xFFFFFFFFu; put(rt, frame.at(0u), 0u); G[7] = 0xEu; G[9] = 4u; store16(frame.at(0u), G[5]); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xF4u); G[5] = 0u; call_here(rt, ctx, G[2]); }
void func_088ECEAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[8] = frame.at(0u); G[7] = 0xAu; G[2] = load8(G[5]); G[6] = load8(G[5] + 2u); G[9] = 4u; store8(frame.at(0u), G[2]); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xF4u); G[5] = 2u; call_here(rt, ctx, G[2]); }
void func_088F2BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC3C58u; G[16] = G[4]; G[4] = word(rt, G[4] + 4u); put(rt, G[16], G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = G[16]; call_here(rt, ctx, 0x08AF20E8u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_088F6804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = psp_mul(float_at(rt, G[5]), ctx.fpr[12]); ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[3] = float_at(rt, G[4] + 8u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = psp_mul(ctx.fpr[12], float_at(rt, G[5] + 4u)); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[0]; put(rt, G[4] + 4u, bits_of(ctx.fpr[2])); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); ctx.fpr[3] = ctx.fpr[3] + ctx.fpr[12]; put(rt, G[4] + 8u, bits_of(ctx.fpr[3])); }
void func_089076F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = __builtin_bswap32(word(rt, G[4] + 4u)); G[3] = 0u; if (G[4] == G[2]) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(__builtin_bswap32(word(rt, G[4] + 0x18u))) ? 1u : 0u; G[6] = G[5] << 5; if (G[2] != 0u) { G[2] = __builtin_bswap32(word(rt, G[4] + 0x1Cu)) + G[6]; G[3] = __builtin_bswap32(word(rt, G[2] + 4u)); } } G[2] = G[3]; }
void func_08915CB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[5] + 8u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[1] = float_at(rt, G[4]); ctx.fpr[3] = ctx.fpr[3] - ctx.fpr[0]; ctx.fpr[5] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[4] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[4] + 8u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[5]; ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[4]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; put(rt, G[4], bits_of(ctx.fpr[1])); put(rt, G[4] + 8u, bits_of(ctx.fpr[0])); put(rt, G[4] + 4u, bits_of(ctx.fpr[2])); }
void func_08916138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[5] = 0x08CA1A38u; G[16] = G[4]; G[4] = G[2]; if (G[2] != 0u) { put(rt, G[16], G[2]); call_here(rt, ctx, 0x08AF6EA8u); put(rt, G[16] + 0xCu, G[2]); } } G[16] = s0; }
void func_08929980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0892B440u); G[4] = G[16] + 0x2Cu; put(rt, G[16] + 0x20u, 0u); put(rt, G[16] + 0x24u, 0u); put(rt, G[16] + 0x28u, 0u); call_here(rt, ctx, 0x089298A8u); put(rt, G[16] + 0x38u, 0u); put(rt, G[16] + 0x34u, 0u); } G[16] = s0; }
void func_0892AA38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    set_flag_and_half(rt, ctx, 0xAu, 0x4u);
}
void func_0892AACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    set_flag_and_half(rt, ctx, 0xEu, 0x10u);
}
void func_0892AB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    set_flag_and_half(rt, ctx, 0xCu, 0x8u);
}
void func_089363BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF298Cu); G[2] = word(rt, 0x08CF298Cu); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); put(rt, 0x08CF298Cu, 0u); } } G[16] = s0; }
void func_08939FEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08939F4Cu, {self});
        const u32 member = word(rt, self + 0x10u);
        ctx.gpr[2] = member;
        if (member != 0u) {
            call(rt, ctx, 0x08B35254u, {member});
            put(rt, self + 0x10u, 0u);
        }
    }
    tail_call(ctx, 0x08939F54u, {self});
}
void func_08945F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08945EF8u); G[2] = high_half(0x08D6950Cu); G[4] = word(rt, 0x08D6950Cu); G[3] = 1u; ctx.fpr[1] = ctx.fpr[0]; if (G[4] != G[3]) return; G[1] = 0x3F200000u; ctx.fpr[0] = 0.625f; ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[1]; }
void func_08976830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_two_vtables(rt, ctx, 0x08993DECu, 1u, 0x08CCCD10u, 0x08CCCF68u);
}
void func_08976D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_two_vtables(rt, ctx, 0x089FE458u, 1u, 0x08CCA998u, 0x08CCABF0u);
}
void func_08977610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 60.0f; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); G[1] = 0x4F000000u; ctx.fpr[1] = 2147483648.0f; ctx.fpr[0] = ctx.fpr[12] - ctx.fpr[1]; const bool high = ctx.fpr[1] <= ctx.fpr[12]; G[3] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[12])); ctx.fpr[12] = std::bit_cast<float>(G[3]); if (high) { G[3] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[3]); G[2] = 0x80000000u; G[3] |= G[2]; } put(rt, G[4] + 0x1B0u, G[3]); }
void func_0899D38C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0897006Cu); G[4] = G[16]; ctx.fpr[12] = float_at(rt, G[16] + 0x120u); call_here(rt, ctx, 0x0899D0D0u); ctx.fpr[12] = float_at(rt, G[16] + 0x120u); G[4] = G[16]; call_here(rt, ctx, 0x0899D108u); G[2] = 1u; } G[16] = s0; }
void func_089B5048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089FF16Cu); G[2] = 0x08CCAE88u; G[3] = 0x08CCB068u; put(rt, G[16], G[2]); put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0x9Cu, 0u); } G[16] = s0; }
void func_089B8260(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x124u); if (G[2] != 0u) { G[2] = load8(G[4] + 0x158u); if (G[2] != 0u) { G[6] = 1u; call_here(rt, ctx, 0x089B80ACu); G[2] = 2u; return; } } } jump(ctx, 0x0896FE3Cu); }
void func_089BCC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08BFF624u); G[3] = word(rt, G[2] + 0x14u); G[4] = G[16] + 0x38u; if (G[3] != 0u) { G[16] = s0; return; } } G[16] = s0; jump(ctx, 0x0897D66Cu); }
void func_089BF170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x2B0u; G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17] + 0x264u; } G[16] = s0; G[17] = s1; }
void func_089BF8EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x2B0u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16] + 0x22Cu; call_here(rt, ctx, 0x08A19688u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_089D8EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08C04F64u); G[3] = word(rt, G[2] + 0x14u); G[4] = G[16]; if (G[3] != 0u) { G[16] = s0; return; } } G[16] = s0; jump(ctx, 0x08A10154u); }
void func_089ED020(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_two_vtables(rt, ctx, 0x08993DECu, 0u, 0x08CC8C48u, 0x08CC8EA0u);
}
void func_089ED544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_two_vtables(rt, ctx, 0x089FE458u, 0u, 0x08CC8758u, 0x08CC89B0u);
}
void func_08A10154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08C149ACu); G[3] = word(rt, G[2] + 0x14u); G[4] = G[16]; if (G[3] != 0u) { G[16] = s0; return; } } G[16] = s0; jump(ctx, 0x089BCC58u); }
void func_08A20AA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    call(rt, ctx, 0x08A20858u, {self});
    const u32 result = call(rt, ctx, 0x08AB9EF0u, {source});
    put(rt, self + 0x14u, result);
    ctx.gpr[2] = result != 0u ? 1u : 0u;
}
void func_08A3A6FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); ctx.fpr[12] = 0.0f; G[2] = word(rt, G[4] + 0x14u); G[16] = G[4]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A3A4CCu); G[4] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08ABC92Cu); put(rt, G[16] + 0x14u, 0u); } } G[16] = s0; }
void func_08A3A788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 0u; G[2] = word(rt, G[4] + 0x18u); G[7] = 0u; G[3] = 0u; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08ABC8B4u); G[3] = load8(frame.at(0u)); } G[2] = G[3]; }
void func_08A41088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    const u32 tail = self + 0x14u;
    ctx.gpr[3] = tail;
    ctx.gpr[2] = 0x08CB89B8u;
    put(rt, self, 0x08CB89B8u);
    put(rt, self + 0x14u, 0u);
    put(rt, tail + 8u, 0u);
    put(rt, tail + 4u, 0u);
}
void func_08A4F580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDF56Cu); G[7] = word(rt, 0x08CDF56Cu); G[6] = 0u; if (static_cast<std::int32_t>(G[7]) > 0) { G[4] = 0x08CDF572u; G[5] = 0u; do { G[2] = load16(G[4]); G[3] = load16(G[4] - 2u); G[5] += 1u; G[4] += 6u; G[2] -= G[3]; G[6] += G[2]; } while (G[5] != G[7]); } G[2] = G[6]; }
void func_08A4FFF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x80460006u; G[16] = G[5]; if (G[5] != 0u) { G[4] = word(rt, G[5]); call_here(rt, ctx, 0x08B35254u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[2] = 0u; } } G[16] = s0; }
void func_08A58014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        release_members(rt, ctx, 0x08CB7730u, 0x18u, 0x08A5B9ECu, 0x08A5B9ECu);
    }
    tail_call(ctx, 0x08A780B0u, {self});
}
void func_08A5F3F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_value(rt, ctx, 0x08CBDFD0u);
}
void func_08A5F5E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy_member_after_base(rt, ctx);
}
void func_08A61280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2468u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x18u; call_here(rt, ctx, 0x08C1F510u); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A710DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 handle = call(rt, ctx, 0x08A7107Cu, {self});
    call(rt, ctx, 0x08A70D40u, {self, handle});
    ctx.gpr[2] = handle;
}
void func_08A72D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_small_value(rt, ctx, static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(ctx.gpr[5]))), 0x08CD1FB0u, 2u);
}
void func_08A75524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        release_members(rt, ctx, 0x08CD3AE8u, 0x18u, 0x08A5B9ECu, 0x08A69D68u);
    }
    tail_call(ctx, 0x08A982D4u, {self});
}
void func_08A7B2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        release_members(rt, ctx, 0x08CD2758u, 0x1Cu, 0x08A5B9ECu, 0x08A5B9ECu);
    }
    tail_call(ctx, 0x08A5D354u, {self});
}
void func_08A80CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        release_members(rt, ctx, 0x08CCFA70u, 0x18u, 0x08A5B9ECu, 0x08A5B9ECu);
    }
    tail_call(ctx, 0x08A780B0u, {self});
}
void func_08A84A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        release_members(rt, ctx, 0x08CD2648u, 0x1Cu, 0x08A5B9ECu, 0x08A5B9ECu);
    }
    tail_call(ctx, 0x08A5D354u, {self});
}
void func_08A84D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_value(rt, ctx, 0x08CBEA58u);
}
void func_08A85674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy_member_after_base(rt, ctx);
}
void func_08A879A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x08A7A3D4u); if (G[16] != 0u) { ctx.fpr[0] = float_at(rt, G[16] + 0x14u); put(rt, G[17] + 0x14u, bits_of(ctx.fpr[0])); } } G[16] = s0; G[17] = s1; }
void func_08A8A690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD3438u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x1Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A5D354u); }
void func_08A8DB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A75BDCu); G[4] = G[16]; G[5] = frame.at(0u); G[6] = 1u; call_here(rt, ctx, 0x08A8DAB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A75A88u); } G[16] = s0; }
void func_08A95AAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_small_value(rt, ctx, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[5]))), 0x08CD21A8u, 1u);
}
void func_08A9BCB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    lock_cycle(rt, ctx, self + 0x4Cu);
    ctx.gpr[2] = self + 0x3Cu;
}
void func_08A9BE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    lock_cycle(rt, ctx, self + 0x4Cu);
    ctx.gpr[2] = self + 0x48u;
}
void func_08A9C778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[6]; call_here(rt, ctx, 0x08A9C70Cu); G[4] = G[16]; G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, G[2]); G[3] = 1u; } G[2] = G[3]; } G[16] = s0; }
void func_08AA191C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = 1u; G[16] = G[5]; G[2] = word(rt, G[3] + 0x110u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); G[3] = sext(load8(frame.at(0u)), 8u) != 0u ? 1u : 0u; store8(G[16], G[3]); } G[16] = s0; }
void func_08AA5DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[7] = 4u; do { G[2] = G[6] << 3; G[3] = G[6] + frame.at(0u); G[2] = G[5] >> (G[2] & 31u); G[6] += 1u; store8(G[3], G[2]); } while (G[6] != G[7]); G[5] = frame.at(0u); G[6] = 4u; call_here(rt, ctx, 0x08AA5D2Cu); }
void func_08AAFBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_small_value(rt, ctx, ctx.gpr[5] & 0xFFFFu, 0x08CD3838u, 2u);
}
void func_08AB137C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    construct_with_small_value(rt, ctx, ctx.gpr[5] & 0xFFu, 0x08CD3728u, 1u);
}
void func_08ABBB20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x1Cu); call_here(rt, ctx, G[2]); return; } } G[2] = 0u; }
void func_08AC2DEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4] + 4u); G[16] = G[4]; if (static_cast<std::int32_t>(G[3]) < 0) { call_here(rt, ctx, 0x08AC2D6Cu); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) G[3] = word(rt, G[16] + 4u); } G[2] = G[3]; } G[16] = s0; }
void func_08AC7508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B344E0u, {self});
    const u32 flag = load8(self + 0x29u);
    call(rt, ctx, 0x08B3457Cu, {self});
    ctx.gpr[2] = flag;
}
void func_08AD380C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08AD3638u, {self});
        member = word(rt, self + 0x30u);
        ctx.gpr[2] = member;
    }
    if (member == 0u) return;
    tail_call(ctx, 0x08B64654u, {member});
}
void func_08AD3B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08D404D0u); G[2] = word(rt, 0x08D404D0u) - 1u; const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D404CCu); put(rt, 0x08D404D0u, G[2]); if (G[2] == 0u) { G[4] = word(rt, 0x08D404CCu); call_here(rt, ctx, 0x08B35254u); put(rt, 0x08D404CCu, 0u); } } G[16] = s0; }
void func_08ADAB3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08C90CCCu; GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AD9E74u); G[4] = G[2]; G[2] = 0u; if (G[4] == 0u) return; G[2] = word(rt, G[4] + 0x3Cu); G[3] = word(rt, G[4] + 0x2Cu); G[4] = word(rt, G[4] + 0x40u); G[3] -= G[2]; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[4]) ? G[3] : G[4]; }
void func_08AE4488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(4u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), 0u); G[2] = word(rt, G[4] + 0x24u); G[4] = sext(load16(G[4] + 0x1Au), 16u); G[3] = sext(load16(G[2]), 16u); put(rt, frame.at(0xCu), G[6]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08AE85E8u); }
void func_08AEB8C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; if (G[4] != 0u) { G[2] = word(rt, G[4]); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08C7A9E0u); G[4] = G[16]; call_here(rt, ctx, 0x08AD85C8u); } } G[16] = s0; }
void func_08AF2978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x08AF296Cu); ctx.fpr[12] = ctx.fpr[20]; G[5] = G[16]; G[4] = G[2]; } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, 0x08AF10C4u); }
void func_08AF9DF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x40u); G[6] = frame.at(0x10u); G[2] = load16(G[4] + 0x12u); G[4] = word(rt, G[4] + 0x34u); put(rt, frame.at(0x10u), G[5]); G[2] <<= 3; G[5] = G[4] + G[2]; call_here(rt, ctx, 0x08C42424u); }
void func_08AFB2E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD76F8u);
        call(rt, ctx, 0x08AFAC58u, {self});
        put(rt, self, 0x08CBF428u);
        ctx.gpr[2] = 0x08CBF428u;
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_08AFCE94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], G[5]); if (G[5] == 0u) return; G[2] = word(rt, G[5] + 4u); put(rt, G[4] + 4u, G[2]); G[3] = word(rt, G[5] + 8u); put(rt, G[4] + 8u, G[3]); G[2] = word(rt, G[5] + 0x10u); put(rt, G[4] + 0xCu, G[2]); G[3] = word(rt, G[5] + 0x14u); put(rt, G[4] + 0x10u, G[3]); G[2] = word(rt, G[5] + 0x18u); put(rt, G[4] + 0x14u, G[2]); G[3] = word(rt, G[5] + 0x1Cu); put(rt, G[4] + 0x18u, G[3]); }
void func_08B02F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[5]; if (G[5] == G[6]) { G[2] = G[5]; return; } G[3] = load16(G[5] + 2u) << 18; G[2] = load16(G[6] + 2u) << 18; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[5] = G[6]; G[6] = G[8]; } jump(ctx, 0x08B02D8Cu); }
void func_08B18B18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B18A98u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B18A58u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08B1A02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17]; G[16] = load8(G[17] + 0x2Cu); call_here(rt, ctx, 0x08B34B24u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08B1B268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = 0u; G[2] = G[8] + G[6]; for (;;) { G[7] = sra(G[2], 1u); G[3] = (G[7] << 1) + G[5]; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(load16(G[3])) ? 1u : 0u; if (G[2] == 0u) G[8] = G[7]; else G[6] = G[7]; G[2] = static_cast<std::int32_t>(G[8] + 1u) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = G[8] + G[6]; } G[2] = G[8]; }
void func_08B1CB80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 length = word(rt, self + 4u);
    const u32 end = word(rt, self) + length;
    store8(end - 1u, 0u);
    put(rt, self + 8u, call(rt, ctx, 0x08B35F60u, {word(rt, self)}));
    ctx.gpr[2] = self;
}
void func_08B23D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD95E0u; G[16] = G[4]; G[5] = G[16] + 0x2Cu; put(rt, G[4], G[2]); G[4] = 0x08D68C8Cu; call_here(rt, ctx, 0x08B1FA64u); G[4] = G[16] + 4u; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_08B273CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0x11Eu; G[16] = G[4]; G[4] = word(rt, G[4] + 0x40u); call_here(rt, ctx, 0x08B26CA8u); G[4] = word(rt, G[16] + 0x48u); G[5] = 0x1Eu; call_here(rt, ctx, 0x08B26CA8u); G[4] = word(rt, G[16] + 0x50u); G[5] = 0x13u; } G[16] = s0; jump(ctx, 0x08B26CA8u); }
void func_08B2797C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[2] = load8(G[4] + 0x84u); G[3] = 1u; G[6] = sext(8u - G[2], 8u); if (G[2] != 0u) { call_here(rt, ctx, 0x08B278A0u); G[3] = G[2]; } G[2] = G[3]; }
void func_08B2A330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] += G[4]; G[4] = G[7] - 1u; if (G[7] == 0u) return; G[2] = G[6] + G[4]; G[7] = G[2] + 1u; do { G[5] = sext(load8(G[6]), 8u); G[2] = load8(G[8]); G[3] = G[6] + 1u; G[4] = G[8] + 1u; store8(G[6], G[2]); G[6] = G[3]; store8(G[8], G[5]); G[8] = G[4]; } while (G[3] != G[7]); }
void func_08B2A370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = G[7]; G[2] = G[5] - G[3]; ctx.fpr[1] = std::bit_cast<float>(G[2]); G[8] = G[2] >> 1; G[3] = (G[2] & 1u) | G[8]; ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); if (static_cast<std::int32_t>(G[2]) < 0) { ctx.fpr[1] = std::bit_cast<float>(G[3]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[0]; } ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); G[8] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[8]); jump(ctx, 0x08B2A330u); }
void func_08B2BEE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[5]); G[3] = G[5] + 4u; G[2] = G[4] & ~3u; G[3] += G[2]; if (G[3] != G[6]) { G[5] = 0u; G[2] = 0u; return; } G[2] = word(rt, G[3]); G[3] = word(rt, G[3] + 4u); G[2] = G[4] + G[2] + 4u; put(rt, G[5], G[2]); put(rt, G[5] + 4u, G[3]); G[2] = G[5]; }
void func_08B2E1AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (self == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B1B8F8u, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_08B2E9F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D690F8u); G[2] = word(rt, 0x08D690F8u); G[4] = G[2]; if (G[2] != 0u) { G[2] = high_half(0x08D690F4u); store8(0x08D690F4u, 0u); call_here(rt, ctx, 0x08B1B99Cu); put(rt, 0x08D690F8u, 0u); } } G[16] = s0; }
void func_08B2F0D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08D6915Cu); G[2] = load8(0x08D6915Cu); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D69158u); if (G[2] != 0u) { G[4] = word(rt, 0x08D69158u); store8(0x08D6915Cu, 0u); call_here(rt, ctx, 0x08B1B99Cu); put(rt, 0x08D69158u, 0u); } } G[16] = s0; }
void func_08B2F808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] != G[2]) return; G[2] = 0xFFFFu; G[4] = 0x1800u; if (G[3] != G[2]) return; G[5] = 1u; call_here(rt, ctx, 0x08B2F17Cu); G[3] = high_half(0x08D69164u); put(rt, 0x08D69164u, G[2]); }
void func_08B336F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D69208u); G[2] = word(rt, 0x08D69208u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x08C7B138u); G[4] = word(rt, 0x08D69208u); call_here(rt, ctx, 0x08C7A9C0u); put(rt, 0x08D69208u, 0u); } } G[16] = s0; }
void func_08B3A3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x40u); G[2] = frame.at(0x24u); put(rt, frame.at(0x24u), G[5]); G[5] = G[2]; put(rt, frame.at(0x28u), G[6]); put(rt, frame.at(0x2Cu), G[7]); put(rt, frame.at(0x30u), G[8]); put(rt, frame.at(0x34u), G[9]); put(rt, frame.at(0x38u), G[10]); put(rt, frame.at(0x3Cu), G[11]); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08B3A6ACu); }
void func_08B3E06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x28u); G[6] = G[2]; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x2Cu); G[5] = word(rt, G[16] + 0x20u); call_here(rt, ctx, 0x08B7E89Cu); put(rt, G[16] + 0x28u, 0u); } G[2] = 0u; } G[16] = s0; }
void func_08B3F838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0x10u; store8(G[4] + 0x10u, 0u); for (std::uint32_t offset = 0u; offset < 0x10u; offset += 2u) store16(G[4] + offset, 0u); for (std::uint32_t offset = 0x11u; offset < 0x16u; ++offset) store8(G[4] + offset, 0u); }
void func_08B425D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[6] &= 0xFFFFu; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B3FF44u); G[4] = G[16]; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x34u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); } G[16] = s0; }
void func_08B462B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B2E05Cu); G[4] = G[2]; G[6] = word(rt, G[16] + 0x4890u); G[7] = word(rt, G[16] + 0x4894u); G[2] -= G[6]; G[8] = G[4] < G[2] ? 1u : 0u; G[3] = G[3] - G[7] - G[8]; } G[16] = s0; }
void func_08B47AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x08B47AD4u); G[5] = G[17]; G[6] = load16(G[16]); G[4] = G[16] + 0x18u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B79A88u); }
void func_08B47BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    if (out != 0u) put(rt, out, call(rt, ctx, 0x08B47BB4u, {self}));
    ctx.gpr[2] = self;
}
void func_08B529C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    run_command(rt, ctx, 0x08B58D08u);
}
void func_08B52A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    run_command(rt, ctx, 0x08B58C9Cu);
}
void func_08B56900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (word(rt, G[4] + 0xD10u) << 2) + 0xD00u + G[4]; G[6] = word(rt, G[5]) - 0x40u; put(rt, G[5], G[6]); G[2] = word(rt, G[4] + 0xD18u); G[3] = word(rt, G[4] + 0xD1Cu); G[6] = word(rt, G[4] + 0xD20u); G[7] = word(rt, G[4] + 0xD24u); G[2] |= G[6]; G[3] |= G[7]; put(rt, G[4] + 0xD18u, G[2]); put(rt, G[4] + 0xD1Cu, G[3]); }
void func_08B87A70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CB8BD8u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x18u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x14u});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}
void func_08B9CFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    build_from_source(rt, ctx, 0x08B9CF6Cu);
}
void func_08B9D1B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    build_from_source(rt, ctx, 0x08B9D15Cu);
}
void func_08B9D3DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    build_from_source(rt, ctx, 0x08B9D388u);
}
void func_08BAC1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 8u); ctx.fpr[1] = float_at(rt, G[5] + 4u); put(rt, frame.at(0u), bits_of(ctx.fpr[0])); G[5] = frame.at(0u); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; put(rt, frame.at(4u), bits_of(ctx.fpr[1])); put(rt, frame.at(8u), bits_of(ctx.fpr[2])); put(rt, frame.at(0xCu), G[1]); call_here(rt, ctx, 0x08B5A7F0u); }
void func_08BDFFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 object = ctx.gpr[5];
    const u32 first = word(rt, object);
    call(rt, ctx, 0x08B5C094u, {object});
    call(rt, ctx, 0x08B35254u, {object});
    ctx.gpr[2] = first;
}
void func_08BE9464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[8] = frame.at(4u); G[5] = 0u; G[7] = 0u; call_here(rt, ctx, 0x0896A35Cu); G[1] = 0x3F000000u; ctx.fpr[1] = 0.5f; ctx.fpr[0] = float_at(rt, frame.at(4u)); ctx.fpr[2] = float_at(rt, frame.at(0u)); } ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; }
void func_08BE94A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[7] = frame.at(4u); G[6] = 0u; G[8] = 0u; call_here(rt, ctx, 0x0896A35Cu); G[1] = 0x3F000000u; ctx.fpr[1] = 0.5f; ctx.fpr[0] = float_at(rt, frame.at(4u)); ctx.fpr[2] = float_at(rt, frame.at(0u)); } ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; }
void func_08C0C384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x089FA280u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x089FA8B4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089F9B74u); G[2] = G[16]; } G[16] = s0; }
void func_08C1A5EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[16] = G[4]; call_here(rt, ctx, 0x08C1A5ACu); G[2] = 0x08CD1D58u; put(rt, G[16] + 0x10u, bits_of(ctx.fpr[20])); put(rt, G[16], G[2]); } G[16] = s0; ctx.fpr[20] = f20; }
void func_08C1FC50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 0xCu; if (G[7] != 0u) { G[2] = word(rt, G[4] + 8u); G[3] = word(rt, G[4]); G[4] = word(rt, G[4] + 4u); put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[4]); } G[4] = G[6]; G[7] += 0xCu; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C3C800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 0xCu; if (G[7] != 0u) { G[2] = word(rt, G[4] + 8u); G[3] = word(rt, G[4]); G[4] = word(rt, G[4] + 4u); put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[4]); } G[4] = G[6]; G[7] += 0xCu; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C46324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[4] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[4]); ctx.fpr[2] = float_at(rt, G[4] + 4u); ctx.fpr[3] = float_at(rt, G[4] + 8u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[4]); ctx.fpr[5] = float_at(rt, G[5] + 8u); ctx.fpr[1] = float_at(rt, G[4] + 0xCu); ctx.fpr[4] = float_at(rt, G[5] + 0xCu); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[5]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[4]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; }
void func_08C470B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[6] = G[4] + 6u; if (G[7] != 0u) { G[2] = load16(G[4] + 4u); G[3] = load16(G[4]); G[4] = load16(G[4] + 2u); store16(G[7] + 4u, G[2]); store16(G[7], G[3]); store16(G[7] + 2u, G[4]); } G[4] = G[6]; G[7] += 6u; } while (G[5] != G[6]); } G[2] = G[7]; }
void func_08C49F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] != 0u) { G[2] = load16(G[8]); G[3] = load16(G[8] + 2u); G[4] = load16(G[8] + 4u); store16(G[7], G[2]); store16(G[7] + 2u, G[3]); store16(G[7] + 4u, G[4]); } G[8] += 6u; G[7] += 6u; } while (G[5] != G[8]); } G[2] = G[7]; }
void func_08C50CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[2] = 0x08CDA8F8u; put(rt, G[4], G[2]); if (G[3] == 0u) return; G[2] = word(rt, G[3] + 4u) - 1u; G[4] = G[3]; put(rt, G[3] + 4u, G[2]); if (G[2] != 0u) return; G[2] = word(rt, G[3]); G[25] = word(rt, G[2] + 4u); jump(ctx, G[25]); }
void func_08C74EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    u32 length;
    {
        GuestFrame frame(ctx, 0x10u);
        length = call(rt, ctx, 0x08B35F60u, {second});
    }
    tail_call(ctx, 0x08C74DD8u, {self, second, length});
}
void func_08C74F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C74DD8u); }
void func_08C75170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = sext(G[5], 8u); G[8] = G[5]; G[5] = 0u; G[16] = G[4]; G[6] = word(rt, G[2] - 0xCu); G[7] = 1u; call_here(rt, ctx, 0x08C74FACu); G[2] = G[16]; } G[16] = s0; }
void func_08C75684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[3] - 4u); G[16] = G[4]; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08C75524u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] - 0xCu) + G[3]; } G[16] = s0; }
void func_08C75C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C75AF8u); }
void func_08C75C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C75AF8u); }
void func_08C76554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[16] = G[6]; call_here(rt, ctx, 0x08C7464Cu); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08C75D90u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_181
} // namespace gt::game

#define HOT181(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_181::name, gt::ReturnKind::kind)
HOT181(0x0880AA54u, func_0880AA54, Word);
HOT181(0x0880DB8Cu, func_0880DB8C, Word);
HOT181(0x0881A880u, func_0881A880, Word);
// HOT181(0x0883A45Cu, func_0883A45C, Word);  // no comparable synthetic case yet
HOT181(0x0885C250u, func_0885C250, Word);
HOT181(0x0885E7DCu, func_0885E7DC, Word);
HOT181(0x0885E878u, func_0885E878, Word);
HOT181(0x0885E8DCu, func_0885E8DC, Word);
HOT181(0x088676B0u, func_088676B0, Float);
HOT181(0x08881D84u, func_08881D84, Word);
HOT181(0x08881FF0u, func_08881FF0, Word);
HOT181(0x088824B8u, func_088824B8, Word);
HOT181(0x08884B98u, func_08884B98, Word);
// HOT181(0x08885B50u, func_08885B50, Word);  // no comparable synthetic case yet
HOT181(0x088899D4u, func_088899D4, Word);
HOT181(0x088A36C0u, func_088A36C0, Word);
HOT181(0x088B11D4u, func_088B11D4, Word);
HOT181(0x088B1214u, func_088B1214, Word);
// HOT181(0x088BBE0Cu, func_088BBE0C, Float);  // no comparable synthetic case yet
HOT181(0x088CE770u, func_088CE770, Word);
HOT181(0x088CE7B0u, func_088CE7B0, Word);
HOT181(0x088CF2E0u, func_088CF2E0, Word);
HOT181(0x088D0934u, func_088D0934, Word);
HOT181(0x088D4D48u, func_088D4D48, Word);
HOT181(0x088E4BC8u, func_088E4BC8, Word);
HOT181(0x088EBCBCu, func_088EBCBC, Word);
HOT181(0x088ECB44u, func_088ECB44, Word);
HOT181(0x088ECEACu, func_088ECEAC, Word);
HOT181(0x088F2BA8u, func_088F2BA8, Word);
HOT181(0x088F6804u, func_088F6804, Word);
HOT181(0x089076F0u, func_089076F0, Word);
HOT181(0x08915CB4u, func_08915CB4, Word);
HOT181(0x08916138u, func_08916138, Word);
HOT181(0x08929980u, func_08929980, Word);
HOT181(0x0892AA38u, func_0892AA38, Word);
HOT181(0x0892AACCu, func_0892AACC, Word);
HOT181(0x0892AB0Cu, func_0892AB0C, Word);
HOT181(0x089363BCu, func_089363BC, Word);
HOT181(0x08939FECu, func_08939FEC, Word);
HOT181(0x08945F2Cu, func_08945F2C, Float);
HOT181(0x08976830u, func_08976830, Word);
HOT181(0x08976D54u, func_08976D54, Word);
HOT181(0x08977610u, func_08977610, Word);
HOT181(0x0899D38Cu, func_0899D38C, Word);
HOT181(0x089B5048u, func_089B5048, Word);
HOT181(0x089B8260u, func_089B8260, Word);
HOT181(0x089BCC58u, func_089BCC58, Word);
// HOT181(0x089BF170u, func_089BF170, Word);  // no comparable synthetic case yet
HOT181(0x089BF8ECu, func_089BF8EC, Word);
HOT181(0x089D8EB8u, func_089D8EB8, Word);
HOT181(0x089ED020u, func_089ED020, Word);
HOT181(0x089ED544u, func_089ED544, Word);
HOT181(0x08A10154u, func_08A10154, Word);
HOT181(0x08A20AA4u, func_08A20AA4, Word);
HOT181(0x08A3A6FCu, func_08A3A6FC, Word);
HOT181(0x08A3A788u, func_08A3A788, Word);
HOT181(0x08A41088u, func_08A41088, Word);
HOT181(0x08A4F580u, func_08A4F580, Word);
HOT181(0x08A4FFF8u, func_08A4FFF8, Word);
HOT181(0x08A58014u, func_08A58014, Word);
HOT181(0x08A5F3F8u, func_08A5F3F8, Word);
HOT181(0x08A5F5E4u, func_08A5F5E4, Word);
// HOT181(0x08A61280u, func_08A61280, Word);  // no comparable synthetic case yet
HOT181(0x08A710DCu, func_08A710DC, Word);
HOT181(0x08A72D38u, func_08A72D38, Word);
HOT181(0x08A75524u, func_08A75524, Word);
HOT181(0x08A7B2C8u, func_08A7B2C8, Word);
HOT181(0x08A80CF8u, func_08A80CF8, Word);
HOT181(0x08A84A98u, func_08A84A98, Word);
HOT181(0x08A84D10u, func_08A84D10, Word);
HOT181(0x08A85674u, func_08A85674, Word);
HOT181(0x08A879A8u, func_08A879A8, Word);
// HOT181(0x08A8A690u, func_08A8A690, Word);  // no comparable synthetic case yet
// HOT181(0x08A8DB4Cu, func_08A8DB4C, Word);  // no comparable synthetic case yet
HOT181(0x08A95AACu, func_08A95AAC, Word);
HOT181(0x08A9BCB0u, func_08A9BCB0, Word);
HOT181(0x08A9BE00u, func_08A9BE00, Word);
HOT181(0x08A9C778u, func_08A9C778, Word);
HOT181(0x08AA191Cu, func_08AA191C, Word);
HOT181(0x08AA5DE4u, func_08AA5DE4, Word);
HOT181(0x08AAFBF8u, func_08AAFBF8, Word);
HOT181(0x08AB137Cu, func_08AB137C, Word);
HOT181(0x08ABBB20u, func_08ABBB20, Word);
HOT181(0x08AC2DECu, func_08AC2DEC, Word);
HOT181(0x08AC7508u, func_08AC7508, Word);
HOT181(0x08AD380Cu, func_08AD380C, Word);
HOT181(0x08AD3B00u, func_08AD3B00, Word);
HOT181(0x08ADAB3Cu, func_08ADAB3C, Word);
HOT181(0x08AE4488u, func_08AE4488, Word);
HOT181(0x08AEB8C8u, func_08AEB8C8, Word);
// HOT181(0x08AF2978u, func_08AF2978, Word);  // no comparable synthetic case yet
HOT181(0x08AF9DF0u, func_08AF9DF0, Word);
HOT181(0x08AFB2E4u, func_08AFB2E4, Word);
HOT181(0x08AFCE94u, func_08AFCE94, Word);
HOT181(0x08B02F64u, func_08B02F64, Word);
// HOT181(0x08B18B18u, func_08B18B18, Word);  // no comparable synthetic case yet
// HOT181(0x08B1A02Cu, func_08B1A02C, Word);  // no comparable synthetic case yet
HOT181(0x08B1B268u, func_08B1B268, Word);
HOT181(0x08B1CB80u, func_08B1CB80, Word);
HOT181(0x08B23D68u, func_08B23D68, Word);
HOT181(0x08B273CCu, func_08B273CC, Word);
HOT181(0x08B2797Cu, func_08B2797C, Word);
HOT181(0x08B2A330u, func_08B2A330, Word);
HOT181(0x08B2A370u, func_08B2A370, Word);
HOT181(0x08B2BEE0u, func_08B2BEE0, Word);
HOT181(0x08B2E1ACu, func_08B2E1AC, Word);
// HOT181(0x08B2E9F8u, func_08B2E9F8, Word);  // no comparable synthetic case yet
// HOT181(0x08B2F0D4u, func_08B2F0D4, Word);  // no comparable synthetic case yet
HOT181(0x08B2F808u, func_08B2F808, Word);
// HOT181(0x08B336F0u, func_08B336F0, Word);  // no comparable synthetic case yet
HOT181(0x08B3A3D0u, func_08B3A3D0, Word);
HOT181(0x08B3E06Cu, func_08B3E06C, Word);
HOT181(0x08B3F838u, func_08B3F838, Word);
HOT181(0x08B425D8u, func_08B425D8, Word);
HOT181(0x08B462B8u, func_08B462B8, DoubleWord);
HOT181(0x08B47AE8u, func_08B47AE8, Word);
HOT181(0x08B47BC0u, func_08B47BC0, Word);
HOT181(0x08B529C8u, func_08B529C8, Word);
HOT181(0x08B52A08u, func_08B52A08, Word);
HOT181(0x08B56900u, func_08B56900, Word);
HOT181(0x08B87A70u, func_08B87A70, Word);
HOT181(0x08B9CFC0u, func_08B9CFC0, Word);
HOT181(0x08B9D1B0u, func_08B9D1B0, Word);
HOT181(0x08B9D3DCu, func_08B9D3DC, Word);
HOT181(0x08BAC1E8u, func_08BAC1E8, Word);
HOT181(0x08BDFFD0u, func_08BDFFD0, Word);
HOT181(0x08BE9464u, func_08BE9464, Float);
HOT181(0x08BE94A4u, func_08BE94A4, Float);
// HOT181(0x08C0C384u, func_08C0C384, Word);  // no comparable synthetic case yet
HOT181(0x08C1A5ECu, func_08C1A5EC, Word);
HOT181(0x08C1FC50u, func_08C1FC50, Word);
HOT181(0x08C3C800u, func_08C3C800, Word);
HOT181(0x08C46324u, func_08C46324, Float);
HOT181(0x08C470B4u, func_08C470B4, Word);
HOT181(0x08C49F24u, func_08C49F24, Word);
HOT181(0x08C50CCCu, func_08C50CCC, Word);
HOT181(0x08C74EE4u, func_08C74EE4, Word);
HOT181(0x08C74F6Cu, func_08C74F6C, Word);
HOT181(0x08C75170u, func_08C75170, Word);
HOT181(0x08C75684u, func_08C75684, Word);
HOT181(0x08C75C14u, func_08C75C14, Word);
HOT181(0x08C75C54u, func_08C75C54, Word);
HOT181(0x08C76554u, func_08C76554, Word);
