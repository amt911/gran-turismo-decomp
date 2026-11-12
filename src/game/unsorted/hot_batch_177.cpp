// Hot batch 177: twelve-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_177 {
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
// `find(self, key)`; the word at `offset` of what it returns, 0 when it returns nothing.
void find_member_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 find, u32 key, u32 offset) {
    const u32 found = call(rt, ctx, find, {ctx.gpr[4], key});
    u32 value = 0u;
    if (found != 0u) value = word(rt, found + offset);
    ctx.gpr[2] = value;
}
// `find(self, key)`; sets the byte at `offset` of what it returns to 1 (v0 is 1 then, else 0).
void find_and_mark(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 find, u32 key, u32 offset) {
    const u32 found = call(rt, ctx, find, {ctx.gpr[4], key});
    if (found != 0u) {
        ctx.gpr[2] = 1u;
        set_byte(rt, found + offset, 1u);
    }
}
void base_then_state_two(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B0CE58u, {self});
    put(rt, self, 2u);
    ctx.gpr[2] = self;
}
// `callee(self, key)`; returns self.
void call_with_key(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 callee, u32 key) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, callee, {self, key});
    ctx.gpr[2] = self;
}
// `0x08B88F44(self, word(source + 0x14) + offset)`; returns self.
void copy_from_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B88F44u, {self, word(rt, ctx.gpr[5] + 0x14u) + offset});
    ctx.gpr[2] = self;
}
// `0x08AF1378(word(self + 4))`; returns that member plus `offset`.
void member_plus(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AF1378u, {word(rt, self + 4u)});
    ctx.gpr[2] = word(rt, self + 4u) + offset;
}
// When `enable` is set, member 4 receives `make(self)`.
void set_member_from(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 make) {
    const u32 self = ctx.gpr[4];
    if (ctx.gpr[5] != 0u) put(rt, self + 4u, call(rt, ctx, make, {self}));
}
// `0x08AE3BE8(word(self + 0x98), &{kind, value})`.
void post_command(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 kind, u32 slot) {
    const u32 value = ctx.gpr[5];
    put(rt, slot, kind);
    put(rt, slot + 4u, value);
    call(rt, ctx, 0x08AE3BE8u, {word(rt, ctx.gpr[4] + 0x98u), slot});
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

void func_08805B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); for (;;) { call_here(rt, ctx, 0x08B1A844u); if (G[2] != 0u) return; call_here(rt, ctx, 0x08B1A7A4u); if (G[2] != 0u) return; } }
void func_0881FAF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[3] = high_half(0x08CE2AD0u); G[4] <<= 4; put(rt, 0x08CE2AD0u, G[2]); call_here(rt, ctx, 0x08B6461Cu); G[3] = high_half(0x08CE2ACCu); put(rt, 0x08CE2ACCu, G[2]); }
void func_0885D5B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0u; G[3] = 0u; G[6] = 2u; do { G[2] = load8(G[4] + 0x40u); G[3] += 1u; G[4] += 1u; G[2] = (G[2] ^ 1u) == 0u ? 1u : 0u; G[5] += G[2]; } while (G[3] != G[6]); G[2] = G[5]; }
void func_0885E22C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x088CBFD4u); G[4] = G[2]; G[5] = G[16]; call_here(rt, ctx, 0x088CB430u); } G[16] = s0; }
void func_0885E570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] << 6; G[3] = word(rt, G[4]); G[5] = (G[5] << 4) + G[2] + 0x4120u; G[3] += G[5]; G[2] = load8(G[3] + 0x18u); G[4] = 0xFFFFFFFFu; if (G[2] == 0u) G[4] = sext(load16(G[3] + 0x1Eu), 16u); G[2] = G[4]; }
void func_0885E61C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 6; G[6] = word(rt, G[4]); G[2] = (G[5] << 4) + G[3] + 0x4120u; G[6] += G[2]; G[3] = word(rt, G[6] + 0x24u); G[6] += G[3]; G[2] = sext(load8(G[6] + 4u), 8u); G[3] = ~G[2]; if (G[3] == 0u) G[2] = G[5]; }
void func_08875584(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load8(G[5]), 8u); GuestFrame frame(ctx, 0x10u); G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x0887552Cu); G[3] = G[2]; } G[2] = G[3]; }
void func_088755B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load8(G[5]), 8u); GuestFrame frame(ctx, 0x10u); G[3] = 1u; if (G[2] != 0u) { call_here(rt, ctx, 0x0887552Cu); G[3] = G[2]; } G[2] = G[3]; }
void func_08884758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x0886B2C4u); if (static_cast<std::int32_t>(G[2]) >= 0) store16(G[16] + 0x12u, G[2]); } G[16] = s0; }
void func_0888B640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0xFu; put(rt, frame.at(8u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(0xCu), G[6]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_0888B85C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x1Cu; put(rt, frame.at(8u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(0xCu), G[6]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_0888BAC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u); G[2] = 0x16u; put(rt, frame.at(8u), G[5]); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(0xCu), G[6]); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x088936A4u); }
void func_0888F390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u);  call_here(rt, ctx, 0x088D9FD8u); G[4] = 4u; G[16] = G[2]; call_here(rt, ctx, 0x088EF80Cu); G[2] += G[16]; } G[16] = s0; }
void func_08890418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x0888F550u); G[5] = __builtin_bswap32(word(rt, G[2] + 0xB4u)); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08893E30u); }
void func_08890448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; call_here(rt, ctx, 0x0888F550u); G[5] = __builtin_bswap32(word(rt, G[2] + 0xACu)); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08894264u); }
void func_088B16A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = 2u; G[3] = 2u; if (G[4] == G[2]) { G[2] = G[3]; return; } G[2] = 3u; G[3] = 0u; if (G[4] != G[2]) { G[2] = 1u; if (G[4] != G[2]) { G[2] = G[3]; return; } } G[3] = 1u; G[2] = 1u; }
void func_088C541C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[6] = 0u; if (G[2] != 0u && static_cast<std::int32_t>(G[5]) >= 0) { G[3] = (G[5] << 2) + G[2]; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 0xCu)) ? 1u : 0u; if (G[2] != 0u) G[6] = word(rt, G[3]); } else if (G[2] != 0u) { G[3] = G[5] << 2; } G[2] = G[6]; }
void func_088C544C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[6] = 0u; if (G[2] != 0u && static_cast<std::int32_t>(G[5]) >= 0) { G[3] = (G[5] << 2) + G[2]; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(word(rt, G[4] + 0xCu)) ? 1u : 0u; if (G[2] != 0u) G[6] = word(rt, G[3]); } else if (G[2] != 0u) { G[3] = G[5] << 2; } G[2] = G[6]; }
void func_088C95D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; store8(G[4] + 8u, 1u); G[3] = 0xFFFFFFFFu; G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x10u, G[5]); G[5] = 0xFFFFFFFFu; put(rt, G[4] + 0xCu, G[6]); put(rt, G[4], G[5]); put(rt, G[4] + 0x30u, G[2]); put(rt, G[4] + 0x34u, G[3]); put(rt, G[4] + 0x28u, 0u); }
void func_088C9600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x24u, G[2]); store8(G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x28u, 0u); put(rt, G[4], G[2]); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x1Cu, G[2]); put(rt, G[4] + 0x20u, G[2]); }
void func_088D6928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[6] & 0xFFu; G[4] = word(rt, G[4] + 0x14Cu); call_here(rt, ctx, 0x088F8D58u); if (G[2] != 0u) store8(G[2] + 0xCu, G[16]); } G[16] = s0; }
void func_088F7528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0xFDu); G[6] = G[2] - 1u; if (G[2] == 0u) return; G[2] = load8(G[4] + 0xFCu) + 1u; store8(G[4] + 0xFDu, G[6]); G[3] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; G[2] &= 0xFFu; if (G[3] == 0u) G[2] = 0u; store8(G[4] + 0xFCu, G[2]); }
void func_0890010C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; ctx.fpr[15] = ctx.fpr[1] - ctx.fpr[15]; ctx.fpr[12] = ctx.fpr[1] - ctx.fpr[12]; ctx.fpr[13] = ctx.fpr[1] - ctx.fpr[13]; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[15]); ctx.fpr[0] = ctx.fpr[1] - ctx.fpr[13]; ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[12]; ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[1]); ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[14]); ctx.fpr[0] = ctx.fpr[13] + ctx.fpr[0]; }
void func_08909B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[5]); call_here(rt, ctx, 0x08AF26F0u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08909AF0u); }
void func_08911214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[4] + 0xCu; G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[2] = 7u; put(rt, G[4] + 0xCu, G[1]); put(rt, G[4], 7u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 8u, 0u); }
void func_08916A50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 4u; call_here(rt, ctx, 0x0890B2F4u); store8(G[16] + 0x14u, 0u); put(rt, G[16] + 0x10u, 0u); } G[16] = s0; }
void func_08918340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[4] = word(rt, G[4] + 0xCu); put(rt, frame.at(0u), bits_of(ctx.fpr[12])); put(rt, frame.at(4u), bits_of(ctx.fpr[13])); put(rt, frame.at(8u), bits_of(ctx.fpr[14])); put(rt, frame.at(0xCu), bits_of(ctx.fpr[15])); call_here(rt, ctx, 0x08927758u); }
void func_0891A660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x44u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[4] = word(rt, G[16] + 0x1F8u); } G[16] = s0; jump(ctx, 0x089262ECu); }
void func_089280F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089277B4u, 0x10u); ctx.fpr[2] = float_at(rt, G[2] + 0x90u); G[2] = high_half(0x08C8A884u); ctx.fpr[1] = float_at(rt, 0x08C8A884u); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[2]))); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; }
void func_08928124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x089277B4u, 0x10u); ctx.fpr[0] = float_at(rt, G[2] + 0x94u); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[0]))); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; }
void func_08928D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08927120u, 0x10u); G[1] = 0x3C8EFA35u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[1] = float_at(rt, G[2] + 0x80u); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); }
void func_0892A424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; put(rt, G[4], 0u); G[4] += 4u; call_here(rt, ctx, 0x0892A414u); G[4] = G[16] + 0x10u; } G[16] = s0; jump(ctx, 0x0892A3E4u); }
void func_08930110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 found;
    {
        GuestFrame frame(ctx, 0x10u);
        found = call(rt, ctx, 0x089363FCu, {self});
    }
    tail_call(ctx, 0x089364ACu, {found, self, 0x26u});
}
void func_08930140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 found;
    {
        GuestFrame frame(ctx, 0x10u);
        found = call(rt, ctx, 0x089363FCu, {self});
    }
    tail_call(ctx, 0x089364ACu, {found, self, 0u});
}
void func_089324F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = G[7] - 1u; G[7] = static_cast<std::int32_t>(G[7]) < 2 ? 1u : 0u; G[6] -= G[5]; G[2] = G[8] - 1u; if (G[7] != 0u) { G[2] = G[5]; return; } const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[6]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[2] = ctx.lo; divide(ctx, G[2], G[3]); G[2] = ctx.lo; G[5] += G[2]; G[2] = G[5]; }
void func_08936154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08935D44u); call_here(rt, ctx, 0x089363FCu); G[4] = word(rt, G[2] + 0x1ECu); G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08937D90u); }
void func_08936238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08935D44u); call_here(rt, ctx, 0x089363FCu); G[4] = word(rt, G[2] + 0x1ECu); G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08937D90u); }
void func_089614A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08961474u); G[3] = load8(G[16] + 0x48u); G[2] = (G[2] | G[3]) != 0u ? 1u : 0u; } G[16] = s0; }
void func_08977760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x19Cu; G[5] = 0u; G[3] = 0u; G[6] = 4u; do { G[2] = word(rt, G[4]); G[3] += 1u; G[4] += 4u; G[2] = G[2] != 0u ? 1u : 0u; G[5] += G[2]; } while (G[3] != G[6]); G[2] = G[5]; }
void func_0898D9E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[5]) + G[7]; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; put(rt, G[5], G[3]); if (G[2] == 0u) { G[3] -= G[6]; put(rt, G[5], G[3]); } const bool nonnegative = static_cast<std::int32_t>(G[3]) >= 0; G[6] += G[3]; if (nonnegative) return; put(rt, G[5], G[6]); }
void func_0899D0D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x124u); if (!(ctx.fpr[12] < ctx.fpr[1])) ctx.fpr[1] = ctx.fpr[12]; ctx.fpr[0] = float_at(rt, G[4] + 0x128u); if (!(ctx.fpr[0] < ctx.fpr[1])) ctx.fpr[0] = ctx.fpr[1]; put(rt, G[4] + 0x148u, bits_of(ctx.fpr[0])); }
void func_089A8D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x64u); G[2] = G[4]; G[6] = G[5]; G[4] = G[3]; G[5] = G[2]; if (G[3] == 0u) return; G[2] = word(rt, G[3]); G[25] = word(rt, G[2] + 0xCu); jump(ctx, G[25]); }
void func_089BA9B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x4Cu); if (G[3] != 0u) { G[2] = word(rt, G[3] + 0x60u); for (;;) { G[2] &= 8u; if (G[2] != 0u) break; G[3] = word(rt, G[3] + 0x4Cu); if (G[3] == 0u) break; G[2] = word(rt, G[3] + 0x60u); } } G[2] = G[3]; }
void func_089D72D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5] << 2; G[3] = (G[5] << 4) - G[6]; G[2] = 0x08CDE8CCu; G[3] += G[2]; ctx.fpr[0] = float_at(rt, G[3] + 4u); put(rt, G[4] + 0x18u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[3] + 8u); put(rt, G[4] + 4u, G[5]); put(rt, G[4] + 0x1Cu, bits_of(ctx.fpr[1])); }
void func_089FB344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x089F9F7Cu); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089FB25Cu); }
void func_08A075E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x13Cu); G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; G[3] = ctx.fpr[1] == ctx.fpr[0] ? 1u : 0u; G[2] = ins(word(rt, G[4] + 0x1A0u), G[3], 3u); put(rt, G[4] + 0x1A0u, G[2]); }
void func_08A0B09C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x089F9F7Cu); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A0B050u); }
void func_08A17B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = G[5]; call_with(rt, ctx, 0x089670A0u, 0x10u); G[3] = G[2] - 2u < 2u ? 1u : 0u; G[2] = 2u; G[4] = 1u; if (G[3] == 0u) G[2] = 1u; }
void func_08A23FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy_from_member(rt, ctx, 0x68u);
}
void func_08A24008(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy_from_member(rt, ctx, 0x64u);
}
void func_08A4CF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A50CB0u, {self});
    ctx.gpr[2] = word(rt, self + 0x74u) - 1u;
    put(rt, self + 0x74u, ctx.gpr[2]);
}
void func_08A75D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; if (G[4] != G[5]) call_here(rt, ctx, 0x08C252FCu); G[2] = G[16]; } G[16] = s0; }
void func_08A76C88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_key(rt, ctx, 0x08A84EE8u, 0x08CF6ED8u);
}
void func_08A76CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_key(rt, ctx, 0x08A84EE8u, 0x08CF6ED8u);
}
void func_08A76E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_key(rt, ctx, 0x08A6972Cu, 0x08CF6FDCu);
}
void func_08A8DBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[5]; G[7] = 2u; G[5] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8DB94u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_08A8DC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[5]; G[7] = 4u; G[5] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8DB94u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_08A8DC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[5]; G[7] = 6u; G[5] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8DB94u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_08A8DC88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = G[5]; G[7] = 0u; G[5] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A8DB94u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); }
void func_08AA1E74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08AA1C2Cu); ctx.fpr[0] = float_at(rt, frame.at(0u)); put(rt, G[16], bits_of(ctx.fpr[0])); } G[16] = s0; }
void func_08AA5DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[3] = G[5] & 0xFFFFu; G[2] = G[3] >> 8; G[5] = frame.at(0u); G[6] = 2u; store8(frame.at(1u), G[2]); store8(frame.at(0u), G[3]); call_here(rt, ctx, 0x08AA5D2Cu); }
void func_08AA643C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = 2u; call_here(rt, ctx, 0x08AA61D8u); G[2] = load8(frame.at(1u)); G[3] = load8(frame.at(0u)); G[2] = (G[2] << 8) | G[3]; }
void func_08ABBB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08C905F4u); if (G[4] != 0u) { G[2] = word(rt, G[4]); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 0x30u); jump(ctx, G[25]); return; } } G[2] = 0x08C905F4u; }
void func_08ABCE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08ABCD8Cu); call_here(rt, ctx, 0x08ABBBB8u); G[2] = high_half(0x08CFFAA8u); G[4] = word(rt, 0x08CFFAA8u); call_here(rt, ctx, 0x08ABC584u); } jump(ctx, 0x08ABC69Cu); }
void func_08AC0A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[4]; G[3] = G[5]; G[10] = G[6]; G[11] = G[7]; G[4] = 0x08CFFB80u; G[9] = G[8] & 0xFFu; G[5] = G[2]; G[6] = G[3]; G[7] = G[10]; G[8] = G[11]; jump(ctx, 0x08AC0F08u); }
void func_08AC22F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; put(rt, G[4] + 0xCu, G[5]); G[5] = 0u; call_here(rt, ctx, 0x08AC221Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08AC223Cu); }
void func_08AC8BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 8u; call_here(rt, ctx, 0x08AD5B80u); put(rt, G[16] + 4u, G[16]); put(rt, G[16], 0u); } G[16] = s0; }
void func_08AC8CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] = 0x08CFFFC8u; call_here(rt, ctx, 0x08B2B6F4u); G[4] = G[2]; call_here(rt, ctx, 0x08B35254u); G[4] = 0x08D00000u; } jump(ctx, 0x08AD34E0u); }
void func_08AD39E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = G[5] < word(rt, G[4] + 0x18u) ? 1u : 0u; G[3] = G[5] << 2; if (G[2] != 0u) { G[2] = word(rt, G[4] + 0x1Cu) + G[3]; G[2] = word(rt, G[2]); return; } } G[2] = 0u; }
void func_08ADA6E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_member_word(rt, ctx, 0x08AD9E74u, 0x08C90C58u, 0x28u);
}
void func_08ADA718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_member_word(rt, ctx, 0x08AD9E74u, 0x08C90C60u, 0x2Cu);
}
void func_08ADAB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_member_word(rt, ctx, 0x08AD9E74u, 0x08C90CC4u, 0x40u);
}
void func_08ADAEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_and_mark(rt, ctx, 0x08AD9E74u, 0x08C90CECu, 0x14u);
}
void func_08ADCD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90F9Cu});
    s32 value = -1;
    if (found != 0u) value = static_cast<std::int16_t>(half_at(rt, found + 0x12u));
    ctx.gpr[2] = static_cast<u32>(value);
}
void func_08AE1D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_member_word(rt, ctx, 0x08ADDCD4u, 0x08C913CCu, 0xECu);
}
void func_08AE2980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x50u); G[3] = G[5]; G[2] = 0x15u; G[4] = word(rt, G[4] + 0x98u); G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); call_here(rt, ctx, 0x08AE85E8u); }
void func_08AE2C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    post_command(rt, ctx, 6u, frame.at(0x0u));
}
void func_08AE2C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    post_command(rt, ctx, 7u, frame.at(0x0u));
}
void func_08AE30E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[4] = sext(load16(G[4] + 0x1Cu), 16u); call_here(rt, ctx, 0x08ADA9A0u); G[2] = load8(G[2] + G[16]); } G[16] = s0; }
void func_08AECFA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x2094u) + G[5]; G[2] = word(rt, G[4] + 0x2098u); G[6] = word(rt, G[4] + 0x209Cu); if (G[2] == 0u) return; divide(ctx, G[3], G[2]); G[6] -= G[5]; G[7] = ctx.hi; G[2] -= G[7]; if (static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2])) G[2] = G[6]; }
void func_08AEDAC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    find_and_mark(rt, ctx, 0x08AEDA20u, 0x08C92350u, 0x3Au);
}
void func_08AF1430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    member_plus(rt, ctx, 0x2F0u);
}
void func_08AF1460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    member_plus(rt, ctx, 0x300u);
}
void func_08AF4DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_key(rt, ctx, 0x08B4E734u, 0x08D47080u);
}
void func_08B02FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load16(G[5] + 0xEAu), 16u); G[3] = sext(load16(G[6] + 0xEAu), 16u); G[7] = word(rt, G[4] + 0x7Cu); G[5] = (G[2] << 3) - (G[2] << 1); G[6] = (G[3] << 3) - (G[3] << 1); G[2] <<= 1; G[3] <<= 1; G[6] += G[7]; G[5] += G[7]; jump(ctx, 0x08B02F64u); }
void func_08B08D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = G[2] + 0x1A8u; G[2] = (word(rt, G[3]) ^ 2u) != 0u ? 1u : 0u; if (G[2] != 0u) return; put(rt, G[3], G[2]); G[2] = 1u; }
void func_08B0F8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[7] &= 0xFFu; G[8] = G[7]; G[7] = G[6]; G[6] = frame.at(0u); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08B0F840u); }
void func_08B1202C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x1840u); G[6] = G[5]; G[7] = 0u; G[5] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B26B74u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B24DB4u); }
void func_08B15A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0DF38u); G[4] = G[16] + 0xF8u; call_here(rt, ctx, 0x08B15988u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B15A6Cu); }
void func_08B169CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    base_then_state_two(rt, ctx);
}
void func_08B16A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    base_then_state_two(rt, ctx);
}
void func_08B16ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    base_then_state_two(rt, ctx);
}
void func_08B1BEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    set_member_from(rt, ctx, 0x08B1BE30u);
}
void func_08B1BEEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    set_member_from(rt, ctx, 0x08B1BE50u);
}
void func_08B1BF1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; if (G[5] != 0u) { call_here(rt, ctx, 0x08B1BE60u); put(rt, G[16] + 8u, G[2]); } } G[16] = s0; }
void func_08B1C2BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B1C2B4u); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 8u, G[2]); put(rt, G[16] + 4u, G[2]); } G[16] = s0; }
void func_08B21C74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[4] + 4u; G[16] = G[4]; put(rt, G[4] + 0xF0u, G[2]); call_here(rt, ctx, 0x08B21C64u); put(rt, G[16] + 0xF4u, G[2]); } G[16] = s0; }
void func_08B24CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x18u); G[2] = word(rt, G[4] + 0xCu); G[3] = word(rt, G[4] + 4u); G[4] = G[5] - G[2]; G[3] += G[4]; G[2] = G[5] < G[3] ? 1u : 0u; if (G[2] != 0u) { G[2] = G[3] - G[5]; G[4] -= G[2]; } G[2] = G[4]; }
void func_08B27684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load16(G[4] + 0x3Eu); if (G[2] == 0u) return; G[2] = load16(G[4] + 0x3Cu); jump(ctx, G[2] != 0u ? 0x08B275A4u : 0x08B27488u); }
void func_08B28D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B34888u); G[4] = G[16]; store8(G[16] + 0x28u, 0u); store8(G[16] + 0x29u, 0u); } G[16] = s0; jump(ctx, 0x08B34B24u); }
void func_08B2BB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x40u); G[7] = 0x08B2B8D0u; G[3] = G[4]; G[6] = 8u; if (G[2] == 0u) return; G[5] = word(rt, G[4] + 0x48u); G[4] = word(rt, G[4] + 0x44u); put(rt, G[3] + 0x40u, 0u); jump(ctx, 0x08B7A3BCu); }
void func_08B3F910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[4] += 0x10u; put(rt, frame.at(0u), 0u); G[6] = 6u; store16(frame.at(4u), 0u); call_here(rt, ctx, 0x08B36170u); } G[2] = G[2] != 0u ? 1u : 0u; }
void func_08B40C18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x10u; if (G[8] != G[2]) return; G[7] = word(rt, G[7] + 8u); G[2] = 1u; if (G[7] == 0u) { put(rt, G[4] + 0x944u, 0u); return; } if (G[7] == G[2]) put(rt, G[4] + 0x944u, G[7]); }
void func_08B420D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0xFFFFFFFFu; G[16] = G[4]; call_here(rt, ctx, 0x08B41AF4u); G[2] = 1u; store8(G[16] + 0xB9u, 0u); } G[16] = s0; }
void func_08B462F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x76u; G[5] = G[4] + 0x70u; G[4] = G[16]; call_here(rt, ctx, 0x08B3F8C0u); G[2] = G[16]; } G[16] = s0; }
void func_08B47CE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); G[6] = G[4]; G[2] = 1u; G[3] = word(rt, G[4] + 0x20u); G[4] = G[5]; if (G[3] == 0u) return; G[5] = word(rt, G[6] + 0x24u); call_here(rt, ctx, G[3]); }
void func_08B52238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDAC68u; G[3] = 0x08CDAC78u; put(rt, G[4] + 0x14u, G[7]); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); put(rt, G[4] + 0x1Cu, G[6]); put(rt, G[4] + 0x20u, G[5]); put(rt, G[4] + 0x18u, G[5]); store8(G[4] + 0x24u, 0u); }
void func_08B57714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0x5Cu); G[7] = G[5] >> 24; G[2] = 0x5C000000u | (G[5] & 0xFFFFFFu); G[3] = 0x5D000000u | (G[7] & 0xFFu); G[7] = G[6] + 8u; put(rt, G[4] + 0x244u, G[5]); put(rt, G[6], G[2]); put(rt, G[6] + 4u, G[3]); put(rt, G[4] + 0x5Cu, G[7]); }
void func_08B58AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0x08D6CE00u; G[8] = word(rt, G[5] + 0x5Cu); G[2] = word(rt, G[5] + 0xA8u); G[6] = ~G[4]; G[3] = 0xE8000000u | (G[6] & 0xFFFFFFu); G[7] = G[8] + 4u; put(rt, G[5] + 0xA8u, G[4]); put(rt, G[8], G[3]); put(rt, G[5] + 0x5Cu, G[7]); }
void func_08B65C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B62100u, 0x10u); G[2] = word(rt, G[2]); G[3] = 0u; if (G[2] != 0u) G[3] = word(rt, G[2]); G[2] = G[3]; }
void func_08B71E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0u), G[4]); G[4] = frame.at(0u); put(rt, frame.at(4u), G[5]); put(rt, frame.at(8u), G[6]); put(rt, frame.at(0x10u), G[8]); put(rt, frame.at(0x14u), G[9]); call_here(rt, ctx, 0x08B72030u); }
void func_08B76910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x18u; call_here(rt, ctx, 0x08B35DA0u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B76904u); }
void func_08B8A6FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 2u; if (G[7] != 0u) { G[2] = load16(G[4]); store16(G[7], G[2]); } G[4] = G[3]; G[7] += 2u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08B8A7F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 2u; if (G[7] != 0u) { G[2] = load16(G[4]); store16(G[7], G[2]); } G[4] = G[3]; G[7] += 2u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08B92B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); } G[4] += 8u; } while (G[5] != G[7]); }
void func_08B92BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); } G[4] += 8u; } while (G[5] != G[7]); }
void func_08B9A594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); } G[4] = G[3]; G[2] += 4u; } while (G[3] != G[5]); } }
void func_08BA52B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); } G[4] = G[3]; G[2] += 4u; } while (G[3] != G[5]); } }
void func_08BBBE94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[4]); ctx.fpr[1] = float_at(rt, G[4] + 4u); ctx.fpr[4] = float_at(rt, G[5] + 4u); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[3] = float_at(rt, G[5] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[4]); ctx.fpr[0] = float_at(rt, G[4] + 8u); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[1]; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[0]; }
void func_08BDD948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); } G[4] += 2u; } while (G[5] != G[7]); }
void func_08BDD9AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); } G[4] += 2u; } while (G[5] != G[7]); }
void func_08BDDB70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); } G[4] += 2u; } while (G[5] != G[7]); }
void func_08BDDC38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load8(G[6]); store8(G[4], G[2]); G[3] = load8(G[6] + 1u); store8(G[4] + 1u, G[3]); } G[4] += 2u; } while (G[5] != G[7]); }
void func_08BEF08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); } G[4] = G[3]; G[2] += 4u; } while (G[3] != G[5]); } }
void func_08BEF0BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); } G[4] = G[3]; G[2] += 4u; } while (G[3] != G[5]); } }
void func_08BEF178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[2] != 0u) { ctx.fpr[0] = float_at(rt, G[4]); put(rt, G[2], bits_of(ctx.fpr[0])); } G[4] = G[3]; G[2] += 4u; } while (G[3] != G[5]); } }
void func_08BF0E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[5] == 0u) return; do { G[2] += 1u; if (G[4] != 0u) { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[6] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[1])); } G[4] += 8u; } while (G[5] != G[2]); }
void func_08BF111C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; if (G[5] == 0u) return; do { G[2] += 1u; if (G[4] != 0u) { ctx.fpr[0] = float_at(rt, G[6]); put(rt, G[4], bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[6] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[1])); } G[4] += 8u; } while (G[5] != G[2]); }
void func_08BF8DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[7] != 0u) { G[2] = word(rt, G[4]); put(rt, G[7], G[2]); } G[4] = G[3]; G[7] += 4u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08BF8E3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[7] != 0u) { G[2] = word(rt, G[4]); put(rt, G[7], G[2]); } G[4] = G[3]; G[7] += 4u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08BF930C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (static_cast<std::int32_t>(G[5]) <= 0) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); } G[4] += 8u; } while (G[5] != G[7]); }
void func_08C1F830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08A61624u); G[4] = G[2]; G[5] = G[16] + 8u; } G[16] = s0; jump(ctx, 0x08A7DF34u); }
void func_08C21C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; call_here(rt, ctx, 0x08A6158Cu); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A61E3Cu); }
void func_08C2AA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 4u; if (G[7] != 0u) { G[2] = word(rt, G[4]); put(rt, G[7], G[2]); } G[4] = G[3]; G[7] += 4u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08C318A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B8C5D4u); ctx.fpr[12] = ctx.fpr[0]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08C31878u); }
void func_08C31B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08C31474u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08C31B00u); }
void func_08C31C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08C314BCu); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08C31C08u); }
void func_08C31D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B8C590u); G[5] = G[2]; G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08C31D10u); }
void func_08C3E19C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[6] + 4u); G[2] = G[4]; G[6] = word(rt, G[6]); if (G[5] == 0u) return; G[3] = 0u; do { G[3] += 1u; put(rt, G[2], G[6]); put(rt, G[2] + 4u, G[7]); G[2] += 8u; } while (G[3] != G[5]); }
void func_08C46DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u); G[5] = sext(G[5], 16u); put(rt, frame.at(0x1Cu), G[7]); put(rt, frame.at(0x20u), G[8]); put(rt, frame.at(0x24u), G[9]); put(rt, frame.at(0x28u), G[10]); put(rt, frame.at(0x2Cu), G[11]); call_here(rt, ctx, 0x08C46DA4u); }
void func_08C46FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 2u; if (G[7] != 0u) { G[2] = load16(G[4]); store16(G[7], G[2]); } G[4] = G[3]; G[7] += 2u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08C4ADF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load16(G[6]); G[3] = load16(G[6] + 2u); store16(G[4], G[2]); store16(G[4] + 2u, G[3]); } G[4] += 4u; } while (G[5] != G[7]); }
void func_08C4AF2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { do { G[3] = G[4] + 2u; if (G[7] != 0u) { G[2] = load16(G[4]); store16(G[7], G[2]); } G[4] = G[3]; G[7] += 2u; } while (G[3] != G[5]); } G[2] = G[7]; }
void func_08C4B20C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = 0u; if (G[5] == 0u) return; do { G[7] += 1u; if (G[4] != 0u) { G[2] = load16(G[6]); G[3] = load16(G[6] + 2u); store16(G[4], G[2]); store16(G[4] + 2u, G[3]); } G[4] += 4u; } while (G[5] != G[7]); }
void func_08C54D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x78u); put(rt, G[4] + 0x78u, G[5]); G[5] = 0u; call_here(rt, ctx, 0x08C54CFCu); G[2] = G[16]; } G[16] = s0; }
void func_08C5889C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[7]; G[16] = G[6]; G[6] -= G[5]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[16]; } G[16] = s0; }
void func_08C588D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[8]; G[16] = G[6]; G[6] -= G[5]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[16]; } G[16] = s0; }
void func_08C6F7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x20u); call_here(rt, ctx, 0x08B79698u); G[5] = word(rt, G[16] + 0x20u); G[4] = G[2]; } G[16] = s0; jump(ctx, 0x08B7EF8Cu); }
void func_08C71A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B70F28u); G[6] = G[2]; G[4] = G[16]; G[7] = G[3]; } G[16] = s0; jump(ctx, 0x08C717C8u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_177
} // namespace gt::game

#define HOT177(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_177::name, gt::ReturnKind::kind)
HOT177(0x08805B90u, func_08805B90, Word);
HOT177(0x0881FAF8u, func_0881FAF8, Word);
HOT177(0x0885D5B0u, func_0885D5B0, Word);
HOT177(0x0885E22Cu, func_0885E22C, Word);
HOT177(0x0885E570u, func_0885E570, Word);
HOT177(0x0885E61Cu, func_0885E61C, Word);
HOT177(0x08875584u, func_08875584, Word);
HOT177(0x088755B4u, func_088755B4, Word);
HOT177(0x08884758u, func_08884758, Word);
HOT177(0x0888B640u, func_0888B640, Word);
// HOT177(0x0888B85Cu, func_0888B85C, Word);  // no comparable synthetic case yet
HOT177(0x0888BAC0u, func_0888BAC0, Word);
HOT177(0x0888F390u, func_0888F390, Word);
HOT177(0x08890418u, func_08890418, Word);
HOT177(0x08890448u, func_08890448, Word);
HOT177(0x088B16A4u, func_088B16A4, Word);
HOT177(0x088C541Cu, func_088C541C, Word);
HOT177(0x088C544Cu, func_088C544C, Word);
HOT177(0x088C95D0u, func_088C95D0, Word);
HOT177(0x088C9600u, func_088C9600, Word);
HOT177(0x088D6928u, func_088D6928, Word);
HOT177(0x088F7528u, func_088F7528, Word);
HOT177(0x0890010Cu, func_0890010C, Float);
// HOT177(0x08909B30u, func_08909B30, Word);  // no comparable synthetic case yet
HOT177(0x08911214u, func_08911214, Word);
HOT177(0x08916A50u, func_08916A50, Word);
// HOT177(0x08918340u, func_08918340, Word);  // no comparable synthetic case yet
// HOT177(0x0891A660u, func_0891A660, Word);  // no comparable synthetic case yet
HOT177(0x089280F4u, func_089280F4, Float);
HOT177(0x08928124u, func_08928124, Float);
HOT177(0x08928D44u, func_08928D44, Float);
HOT177(0x0892A424u, func_0892A424, Word);
HOT177(0x08930110u, func_08930110, Word);
HOT177(0x08930140u, func_08930140, Word);
HOT177(0x089324F8u, func_089324F8, Word);
HOT177(0x08936154u, func_08936154, Word);
HOT177(0x08936238u, func_08936238, Word);
HOT177(0x089614A8u, func_089614A8, Word);
HOT177(0x08977760u, func_08977760, Word);
HOT177(0x0898D9E8u, func_0898D9E8, Word);
HOT177(0x0899D0D0u, func_0899D0D0, Word);
HOT177(0x089A8D3Cu, func_089A8D3C, Word);
HOT177(0x089BA9B4u, func_089BA9B4, Word);
HOT177(0x089D72D0u, func_089D72D0, Word);
HOT177(0x089FB344u, func_089FB344, Word);
HOT177(0x08A075E4u, func_08A075E4, Word);
HOT177(0x08A0B09Cu, func_08A0B09C, Word);
HOT177(0x08A17B38u, func_08A17B38, Word);
HOT177(0x08A23FD8u, func_08A23FD8, Word);
HOT177(0x08A24008u, func_08A24008, Word);
HOT177(0x08A4CF7Cu, func_08A4CF7C, Word);
HOT177(0x08A75D88u, func_08A75D88, Word);
HOT177(0x08A76C88u, func_08A76C88, Word);
HOT177(0x08A76CB8u, func_08A76CB8, Word);
HOT177(0x08A76E0Cu, func_08A76E0C, Word);
// HOT177(0x08A8DBF8u, func_08A8DBF8, Word);  // no comparable synthetic case yet
// HOT177(0x08A8DC28u, func_08A8DC28, Word);  // no comparable synthetic case yet
// HOT177(0x08A8DC58u, func_08A8DC58, Word);  // no comparable synthetic case yet
// HOT177(0x08A8DC88u, func_08A8DC88, Word);  // no comparable synthetic case yet
HOT177(0x08AA1E74u, func_08AA1E74, Word);
// HOT177(0x08AA5DB4u, func_08AA5DB4, Word);  // no comparable synthetic case yet
// HOT177(0x08AA643Cu, func_08AA643C, Word);  // no comparable synthetic case yet
HOT177(0x08ABBB60u, func_08ABBB60, Word);
// HOT177(0x08ABCE64u, func_08ABCE64, Word);  // no comparable synthetic case yet
HOT177(0x08AC0A14u, func_08AC0A14, Word);
HOT177(0x08AC22F4u, func_08AC22F4, Word);
HOT177(0x08AC8BACu, func_08AC8BAC, Word);
HOT177(0x08AC8CF0u, func_08AC8CF0, Word);
HOT177(0x08AD39E4u, func_08AD39E4, Word);
HOT177(0x08ADA6E8u, func_08ADA6E8, Word);
HOT177(0x08ADA718u, func_08ADA718, Word);
HOT177(0x08ADAB0Cu, func_08ADAB0C, Word);
HOT177(0x08ADAEBCu, func_08ADAEBC, Word);
HOT177(0x08ADCD54u, func_08ADCD54, Word);
HOT177(0x08AE1D20u, func_08AE1D20, Word);
HOT177(0x08AE2980u, func_08AE2980, Word);
HOT177(0x08AE2C08u, func_08AE2C08, Word);
HOT177(0x08AE2C38u, func_08AE2C38, Word);
HOT177(0x08AE30E0u, func_08AE30E0, Word);
HOT177(0x08AECFA4u, func_08AECFA4, Word);
HOT177(0x08AEDAC4u, func_08AEDAC4, Word);
HOT177(0x08AF1430u, func_08AF1430, Word);
HOT177(0x08AF1460u, func_08AF1460, Word);
HOT177(0x08AF4DC4u, func_08AF4DC4, Word);
HOT177(0x08B02FA4u, func_08B02FA4, Word);
HOT177(0x08B08D54u, func_08B08D54, Word);
HOT177(0x08B0F8F0u, func_08B0F8F0, Word);
// HOT177(0x08B1202Cu, func_08B1202C, Word);  // no comparable synthetic case yet
HOT177(0x08B15A98u, func_08B15A98, Word);
HOT177(0x08B169CCu, func_08B169CC, Word);
HOT177(0x08B16A8Cu, func_08B16A8C, Word);
HOT177(0x08B16ABCu, func_08B16ABC, Word);
HOT177(0x08B1BEBCu, func_08B1BEBC, Word);
HOT177(0x08B1BEECu, func_08B1BEEC, Word);
HOT177(0x08B1BF1Cu, func_08B1BF1C, Word);
HOT177(0x08B1C2BCu, func_08B1C2BC, Word);
HOT177(0x08B21C74u, func_08B21C74, Word);
HOT177(0x08B24CF0u, func_08B24CF0, Word);
HOT177(0x08B27684u, func_08B27684, Word);
HOT177(0x08B28D94u, func_08B28D94, Word);
HOT177(0x08B2BB44u, func_08B2BB44, Word);
HOT177(0x08B3F910u, func_08B3F910, Word);
HOT177(0x08B40C18u, func_08B40C18, Word);
HOT177(0x08B420D0u, func_08B420D0, Word);
HOT177(0x08B462F8u, func_08B462F8, Word);
HOT177(0x08B47CE0u, func_08B47CE0, Word);
HOT177(0x08B52238u, func_08B52238, Word);
HOT177(0x08B57714u, func_08B57714, Word);
HOT177(0x08B58AA0u, func_08B58AA0, Word);
HOT177(0x08B65C84u, func_08B65C84, Word);
HOT177(0x08B71E98u, func_08B71E98, Word);
HOT177(0x08B76910u, func_08B76910, Word);
HOT177(0x08B8A6FCu, func_08B8A6FC, Word);
HOT177(0x08B8A7F8u, func_08B8A7F8, Word);
HOT177(0x08B92B90u, func_08B92B90, Word);
HOT177(0x08B92BF8u, func_08B92BF8, Word);
HOT177(0x08B9A594u, func_08B9A594, Word);
HOT177(0x08BA52B8u, func_08BA52B8, Word);
HOT177(0x08BBBE94u, func_08BBBE94, Float);
HOT177(0x08BDD948u, func_08BDD948, Word);
HOT177(0x08BDD9ACu, func_08BDD9AC, Word);
HOT177(0x08BDDB70u, func_08BDDB70, Word);
HOT177(0x08BDDC38u, func_08BDDC38, Word);
// HOT177(0x08BEF08Cu, func_08BEF08C, Word);  // no comparable synthetic case yet
HOT177(0x08BEF0BCu, func_08BEF0BC, Word);
HOT177(0x08BEF178u, func_08BEF178, Word);
HOT177(0x08BF0E04u, func_08BF0E04, Word);
HOT177(0x08BF111Cu, func_08BF111C, Word);
HOT177(0x08BF8DE8u, func_08BF8DE8, Word);
HOT177(0x08BF8E3Cu, func_08BF8E3C, Word);
HOT177(0x08BF930Cu, func_08BF930C, Word);
HOT177(0x08C1F830u, func_08C1F830, Word);
HOT177(0x08C21C38u, func_08C21C38, Word);
HOT177(0x08C2AA50u, func_08C2AA50, Word);
// HOT177(0x08C318A0u, func_08C318A0, Word);  // no comparable synthetic case yet
// HOT177(0x08C31B2Cu, func_08C31B2C, Word);  // no comparable synthetic case yet
// HOT177(0x08C31C34u, func_08C31C34, Word);  // no comparable synthetic case yet
// HOT177(0x08C31D3Cu, func_08C31D3C, Word);  // no comparable synthetic case yet
HOT177(0x08C3E19Cu, func_08C3E19C, Word);
HOT177(0x08C46DACu, func_08C46DAC, Word);
HOT177(0x08C46FA4u, func_08C46FA4, Word);
HOT177(0x08C4ADF0u, func_08C4ADF0, Word);
HOT177(0x08C4AF2Cu, func_08C4AF2C, Word);
HOT177(0x08C4B20Cu, func_08C4B20C, Word);
HOT177(0x08C54D44u, func_08C54D44, Word);
HOT177(0x08C5889Cu, func_08C5889C, Word);
HOT177(0x08C588D4u, func_08C588D4, Word);
HOT177(0x08C6F7E8u, func_08C6F7E8, Word);
HOT177(0x08C71A4Cu, func_08C71A4C, Word);
