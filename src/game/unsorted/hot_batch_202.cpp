// Hot batch 202: thirty-seven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_202 {
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

using u32 = std::uint32_t;
using s32 = std::int32_t;
// A `jal` with a0.. loaded from `arguments` first (the registers the original sets before it); v0 / v1 / f0 as the callee left them.
u32 call_args(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    call_here(rt, ctx, target);
    return ctx.gpr[2];
}
// A `lw v0, slot(vtable); jalr v0` call: v0 holds the target when the callee starts.
u32 call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = target;
    call_here(rt, ctx, target);
    return ctx.gpr[2];
}
// A bound call through two {function, adjustment} pairs of the object at a0: the first (+0x14 / +0x18, an odd adjustment marks a
// virtual slot, the signed rest shifts `this`) is called on the object a2 points to, its result is the object the second pair
// (+0x1C / +0x20) is resolved on, and that one is tail called with (this', result).
void call_member_then_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 target = 0u;
    u32 second_this = 0u;
    u32 result = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 adjust = word(rt, self + 0x18u);
        const u32 function = word(rt, self + 0x14u);
        const u32 object = word(rt, ctx.gpr[6]);
        const u32 adjusted = object + static_cast<u32>(static_cast<s32>(adjust) >> 1);
        u32 callee = function;
        if ((adjust & 1u) != 0u) callee = word(rt, word(rt, adjusted) + function);
        ctx.gpr[3] = adjust & 1u;
        ctx.gpr[5] = adjust;
        ctx.gpr[6] = object;
        ctx.gpr[7] = function;
        ctx.gpr[8] = callee;
        ctx.gpr[4] = adjusted;
        call_here(rt, ctx, callee);
        result = ctx.gpr[2];
        const u32 second_adjust = word(rt, self + 0x20u);
        const u32 shifted = static_cast<u32>(static_cast<s32>(second_adjust) >> 1);
        second_this = result + shifted;
        if ((second_adjust & 1u) != 0u) {
            const u32 second_function = word(rt, self + 0x1Cu);
            ctx.gpr[3] = second_function;
            ctx.gpr[2] = word(rt, second_this) + second_function;
            target = word(rt, ctx.gpr[2]);
        } else {
            target = word(rt, self + 0x1Cu);
            ctx.gpr[3] = shifted;
            ctx.gpr[2] = 0u;
        }
        ctx.gpr[25] = target;
    }
    tail_call(ctx, target, {second_this, result});
}
// Releases the string whose body pointer is `string` (the empty shared body 0x08D75298 is never released): one reference dropped
// (0x08B60804), the body freed (0x08B93554) when none is left.
void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 string, u32 frame_slot) {
    const u32 body = string - 0xCu;
    ctx.gpr[2] = 0x08D75298u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = string - 4u;
    if (body == 0x08D75298u) return;
    const u32 references = call_args(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = body;
    ctx.gpr[5] = frame_slot;
    if (static_cast<s32>(references) <= 0) call_args(rt, ctx, 0x08B93554u, {body, frame_slot});
}
// A temporary string is built (0x08AB85F0 / 0x08AB856C), `0x088755B4(self, text)` answers, and 0x0886B47C(*self, kind, !answer)
// takes the verdict; the string is released.
void apply_string_verdict(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 kind) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 text = frame.at(4u);
    call_args(rt, ctx, 0x08AB85F0u, {text});
    const u32 name = call_args(rt, ctx, 0x08AB856Cu, {text});
    const u32 found = call_args(rt, ctx, 0x088755B4u, {self, name});
    call_args(rt, ctx, 0x0886B47Cu, {word(rt, self), kind, (found ^ 1u) & 0xFFu});
    release_string(rt, ctx, word(rt, text), frame.at(0u));
}
// `count == 1`: `0x089DB3B4(frame +0, a1)` fills a scratch slot with a handle; the handle takes `receive(handle, a3)`, the slot is
// read again and `0x089DB14C(handle, a, b, c)` finishes; each null handle is reported (0x08A5BB7C) as it is dereferenced; the scratch
// is released (0x08A5B9EC).
void handle_two_steps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 receive, u32 flag_a, u32 flag_b, u32 flag_c) {
    const u32 key = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = frame.at(0u);
    if (count != 1u) return;
    call_args(rt, ctx, 0x089DB3B4u, {frame.at(0u), key});
    u32 handle = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08CA752Cu;
    if (handle == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    call_args(rt, ctx, receive, {handle, list});
    handle = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08CA752Cu;
    if (handle == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    call_args(rt, ctx, 0x089DB14Cu, {handle, flag_a, flag_b, flag_c});
    call_args(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
// In place over [a1, a2): each byte goes through the virtual `slot` of the object a0 (a char conversion); v0 = a2.
void convert_bytes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    const u32 facet = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = first < last ? 1u : 0u;
    if (first < last) {
        for (u32 at = first; at != last; ++at) {
            const u32 character = sext(rt.memory().load8(at), 8);
            const u32 converted = call_virtual(rt, ctx, word(rt, word(rt, facet) + slot), {facet, character});
            rt.memory().store8(at, static_cast<std::uint8_t>(converted));
        }
    }
    ctx.gpr[2] = last;
}
// A vector of words at self + `offset` (begin / end pointers) resized to a1 elements: 0x08B85798 trims the tail when it is longer,
// 0x08B85F30 appends the difference, filling with a temporary built at frame +0 (0x08A76AA0, released after).
void resize_word_vector(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 vector = self + offset;
    call_args(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    const u32 begin = word(rt, self + offset);
    const u32 end = word(rt, vector + 4u);
    const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    if (count < size) {
        call_args(rt, ctx, 0x08B85798u, {vector, begin + count * 4u, end, frame.at(0u), size});
    } else {
        call_args(rt, ctx, 0x08B85F30u, {vector, end, count - size, frame.at(0u), size});
    }
    call_args(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
// A flag byte at (*a0) + a1 + 0x4B4 that is `expected`: 0x08B344E0 on the sub-object at +0xEC, the owner's virtual +0x50 finds the
// slot, the flag and its mirror become `value`; then 0x08B3457C is tail called on the sub-object. v0 stays index + *a0 otherwise.
void toggle_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 expected, u32 value) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 base = word(rt, self);
        ctx.gpr[2] = index + base;
        if (static_cast<u32>(rt.memory().load8(index + base + 0x4B4u)) == expected) {
            call_args(rt, ctx, 0x08B344E0u, {base + 0xECu});
            const u32 owner = word(rt, self);
            const u32 slot = call_virtual(rt, ctx, word(rt, word(rt, owner) + 0x50u), {owner, 0xFFFFFFFFu});
            if (slot != 0u) {
                rt.memory().store8(slot + index, static_cast<std::uint8_t>(value));
                ctx.gpr[2] = index + word(rt, self);
                rt.memory().store8(ctx.gpr[2] + 0x4B4u, static_cast<std::uint8_t>(value));
            }
            ctx.gpr[4] = word(rt, self) + 0xECu;
            tail = true;
        }
    }
    if (tail) tail_call(ctx, 0x08B3457Cu, {ctx.gpr[4]});
}
// Drops one strong reference of the counted object (count +4, weak count +8; virtual +8 disposes, virtual +0xC destroys). Returns
// the destroy target when both counts hit zero (0 otherwise); v0 as the original leaves it.
u32 release_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 counted) {
    if (counted == 0u) return 0u;
    const u32 uses = word(rt, counted + 4u);
    put(rt, counted + 4u, uses - 1u);
    ctx.gpr[2] = 1u;
    if (uses != 1u) return 0u;
    call_virtual(rt, ctx, word(rt, word(rt, counted) + 8u), {counted});
    const u32 weak = word(rt, counted + 8u);
    put(rt, counted + 8u, weak - 1u);
    ctx.gpr[2] = 1u;
    if (weak != 1u) return 0u;
    ctx.gpr[2] = word(rt, counted);
    const u32 destroy = word(rt, ctx.gpr[2] + 0xCu);
    ctx.gpr[25] = destroy;
    return destroy;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08BEAF2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_tail(rt, ctx);
}
void func_08BE65E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] == G[5]) { G[2] = G[7]; return; } G[9] = 0x08CCDE00u; G[8] = G[4]; do { if (G[7] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0xCu); ctx.fpr[1] = float_at(rt, G[4] + 0x10u); G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4] + 8u); ctx.fpr[2] = float_at(rt, G[4] + 0x14u); ctx.fpr[3] = float_at(rt, G[4] + 0x18u); ctx.fpr[4] = float_at(rt, G[4] + 0x1Cu); put(rt, G[6] + 4u, G[2]); put(rt, G[6] + 8u, G[3]); put_float(rt, G[6] + 0xCu, ctx.fpr[0]); put_float(rt, G[6] + 0x10u, ctx.fpr[1]); put_float(rt, G[6] + 0x14u, ctx.fpr[2]); put_float(rt, G[6] + 0x18u, ctx.fpr[3]); put_float(rt, G[6] + 0x1Cu, ctx.fpr[4]); put(rt, G[6], G[9]); ctx.fpr[0] = float_at(rt, G[4] + 0x20u); put_float(rt, G[6] + 0x20u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] + 0x24u); put_float(rt, G[6] + 0x24u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[4] + 0x28u); put_float(rt, G[6] + 0x28u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] + 0x2Cu); put_float(rt, G[6] + 0x2Cu, ctx.fpr[1]); } G[4] = G[8] + 0x30u; G[8] = G[4]; G[7] += 0x30u; G[6] += 0x30u; } while (G[4] != G[5]); G[2] = G[7]; }
void func_08B64DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    convert_bytes(rt, ctx, 0x8u);
}
void func_08B16824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA0u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[4] = G[2]; G[17] = G[2]; call_here(rt, ctx, 0x08B17A98u); G[3] = word(rt, G[18] + 0x7Cu); G[6] = G[2]; G[5] = G[2]; G[2] = 0xAu; G[4] = G[16]; if (G[3] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xC0u); call_here(rt, ctx, G[2]); } else { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xC4u); G[5] = G[6]; call_here(rt, ctx, G[2]); } put(rt, G[18] + 0xA8u, G[2]); G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B17AA8u); }
void func_08A5DD38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[4] = frame.at(0u); G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A5DCF8u); G[17] = word(rt, frame.at(0u)); G[4] = 0x08CB00B4u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CB0080u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[17]; G[7] = G[3]; call_here(rt, ctx, 0x08A5D8D4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; }
void func_089F1550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4] + 0x20u; G[16] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; call_here(rt, ctx, 0x08A76AA0u); G[5] = G[17] << 2; G[2] = word(rt, G[16] + 0x20u); G[3] = word(rt, G[18] + 4u); G[4] = G[18]; G[5] = G[2] + G[5]; G[2] = G[3] - G[2]; G[8] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 2); G[2] = G[17] < G[8] ? 1u : 0u; G[6] = G[3]; G[7] = frame.at(0u); if (G[2] != 0u) call_here(rt, ctx, 0x08B85798u); else { G[6] = G[17] - G[8]; G[4] = G[18]; G[5] = G[3]; call_here(rt, ctx, 0x08B85F30u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089B9794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = G[4]; G[4] = G[19]; G[17] = frame.at(4u); call_here(rt, ctx, 0x089B9660u); G[4] = 0x08CA5B6Cu; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x70u); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08877964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    apply_string_verdict(rt, ctx, 3u);
}
void func_0884BA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(4u); call_here(rt, ctx, 0x08847008u); G[4] = 0x08C9CBBCu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08846E50u); G[4] = G[2]; call_here(rt, ctx, 0x088D47F4u); G[6] = G[2]; G[4] = G[17]; G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C0F7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_tail(rt, ctx);
}
void func_08C2DCF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_tail(rt, ctx);
}
void func_08BECEAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] == G[5]) { G[2] = G[7]; return; } G[9] = 0x08CCDE00u; G[8] = G[4]; do { if (G[7] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0xCu); ctx.fpr[1] = float_at(rt, G[4] + 0x10u); G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4] + 8u); ctx.fpr[2] = float_at(rt, G[4] + 0x14u); ctx.fpr[3] = float_at(rt, G[4] + 0x18u); ctx.fpr[4] = float_at(rt, G[4] + 0x1Cu); put(rt, G[6] + 4u, G[2]); put(rt, G[6] + 8u, G[3]); put_float(rt, G[6] + 0xCu, ctx.fpr[0]); put_float(rt, G[6] + 0x10u, ctx.fpr[1]); put_float(rt, G[6] + 0x14u, ctx.fpr[2]); put_float(rt, G[6] + 0x18u, ctx.fpr[3]); put_float(rt, G[6] + 0x1Cu, ctx.fpr[4]); put(rt, G[6], G[9]); ctx.fpr[0] = float_at(rt, G[4] + 0x20u); put_float(rt, G[6] + 0x20u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] + 0x24u); put_float(rt, G[6] + 0x24u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[4] + 0x28u); put_float(rt, G[6] + 0x28u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] + 0x2Cu); put_float(rt, G[6] + 0x2Cu, ctx.fpr[1]); } G[4] = G[8] + 0x30u; G[8] = G[4]; G[7] += 0x30u; G[6] += 0x30u; } while (G[4] != G[5]); G[2] = G[7]; }
void func_08B64E74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    convert_bytes(rt, ctx, 0x10u);
}
void func_08B16B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA0u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[4] = G[2]; G[17] = G[2]; call_here(rt, ctx, 0x08B17A98u); G[3] = word(rt, G[18] + 0x7Cu); G[6] = G[2]; G[5] = G[2]; G[2] = 0x8u; G[4] = G[16]; if (G[3] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xB8u); call_here(rt, ctx, G[2]); } else { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xBCu); G[5] = G[6]; call_here(rt, ctx, G[2]); } put(rt, G[18] + 0xA8u, G[2]); G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B17AA8u); }
void func_08A82FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[4] = frame.at(0u); G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A82F70u); G[17] = word(rt, frame.at(0u)); G[4] = 0x08CB16C0u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CB168Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[17]; G[7] = G[3]; call_here(rt, ctx, 0x08A82C24u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; }
void func_08B861C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    resize_word_vector(rt, ctx, 0x14u);
}
void func_08A024C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = G[4]; G[4] = G[19]; G[17] = frame.at(4u); call_here(rt, ctx, 0x08A01338u); G[4] = 0x08CA8B14u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0xF8u); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_088781B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    apply_string_verdict(rt, ctx, 4u);
}
void func_0884BB28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(4u); call_here(rt, ctx, 0x08847008u); G[4] = 0x08C9CBBCu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08846E50u); G[4] = G[2]; call_here(rt, ctx, 0x088D4810u); G[6] = G[2]; G[4] = G[17]; G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08817A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[2] = 0xFFFFu; G[5] = 0x08C87DD0u; G[4] = 0x08CE2AA4u; G[6] = 5u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08B2095Cu); G[4] = 0x08CE2A9Cu; G[5] = 0x08C87DA8u; G[6] = 5u; call_here(rt, ctx, 0x08B2095Cu); G[4] = 0x08CE2A80u; G[5] = 0x08C98D3Cu; G[7] = 0x08817A1Cu; G[8] = 0x08818C2Cu; G[9] = 0x08817A40u; G[6] = 0x1Cu; target = 0x08A92114u; } } } if (target != 0u) jump(ctx, target); }
void func_0881A7AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[3] = 0xFFFFFFFFu; G[17] = G[5]; G[2] = 0xFFFFFFFFu; G[16] = G[4]; G[5] = 0u; G[6] = 0x190u; put(rt, G[4] + 0x14u, G[3]); put(rt, G[16], G[17]); put(rt, G[4] + 0x10u, G[2]); G[4] = G[4] + 0x34u; put(rt, G[16] + 4u, 0u); put(rt, G[16] + 8u, 0u); put(rt, G[16] + 0xCu, 0u); call_here(rt, ctx, 0x08B36018u); G[6] = 0x190u; G[4] = G[16] + 0x1C4u; G[5] = 0u; call_here(rt, ctx, 0x08B36018u); G[3] = G[17] << 5; G[17] <<= 1; G[17] += G[3]; G[5] = G[17]; G[4] = 0x10u; call_here(rt, ctx, 0x08B2B4D8u); put(rt, G[16] + 0x30u, G[2]); G[6] = G[17]; G[4] = G[2]; G[5] = 0u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B36018u); }
void func_0882FB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = load8(G[16] + 0x5BAu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088333C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[5] = G[2] + 0xCu; G[4] = frame.at(0u); call_here(rt, ctx, 0x08848DB8u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088512FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088547C4u); G[4] = 0x08C9D3BCu; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5AE4u); ctx.fpr[12] = ctx.fpr[0]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0883C758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x1A0u); G[17] = frame.at(8u); G[18] = G[4]; G[4] = G[17]; G[16] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { call_here(rt, ctx, 0x0893600Cu); G[4] = G[16]; call_here(rt, ctx, 0x0883B848u); G[6] = G[2]; G[7] = G[3]; G[4] = G[17]; call_here(rt, ctx, 0x08935B10u); G[4] = G[17]; call_here(rt, ctx, 0x08932988u); G[5] = G[2] != 0u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08932448u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0885C2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = word(rt, G[4]); G[2] = word(rt, G[4] + 4u); if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = 0u; G[16] = 0u; G[19] = 0xFFFFFFFFu; G[2] = word(rt, G[4]); for (;;) { G[17] += 1u; G[2] += G[16]; G[3] = load8(G[2] + 0x16u); G[16] += 0x188u; if (G[3] != 0u) { G[2] = word(rt, G[2] + 0x1Cu); G[5] = G[2]; if (G[2] != G[19]) { G[4] = word(rt, G[18] + 4u); call_here(rt, ctx, 0x0885EC98u); } } G[4] = word(rt, G[18]); G[2] = word(rt, G[4] + 4u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = word(rt, G[4]); } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_0885D07C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[6] = word(rt, G[4]); G[2] = word(rt, G[6] + 4u); G[18] = G[4]; if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = 0u; G[16] = 0u; G[2] = word(rt, G[6]); for (;;) { G[17] += 1u; G[4] = G[2] + G[16]; G[3] = load8(G[4] + 0x16u); G[16] += 0x188u; if (G[3] != 0u) { G[3] = word(rt, G[4] + 0x1Cu); G[2] = 0xFFFFFFFFu; G[5] = G[3]; if (G[3] != G[2]) { G[6] = word(rt, G[4] + 0x20u); G[4] = word(rt, G[18] + 4u); G[2] = ~(0u | G[6]); if (G[2] == 0u) G[6] = G[3]; call_here(rt, ctx, 0x0885EA04u); } } G[6] = word(rt, G[18]); G[2] = word(rt, G[6] + 4u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = word(rt, G[6]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08869254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x08C9F618u; G[5] = 0x100u; call_here(rt, ctx, 0x08ABF3CCu); G[4] = 0x08C9F628u; put(rt, G[16] + 0x1BCu, G[2]); G[5] = 0x100u; call_here(rt, ctx, 0x08ABF3CCu); G[5] = word(rt, G[16] + 0x1BCu); put(rt, G[16] + 0x1C0u, G[2]); G[2] = word(rt, G[5]); G[3] = word(rt, G[2] + 0xCu); G[4] = G[5]; call_here(rt, ctx, G[3]); G[4] = word(rt, G[16] + 0x1C0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[4] = word(rt, G[16] + 0x1BCu); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x30u); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = word(rt, G[16] + 0x1C0u); G[2] = word(rt, G[4]); G[5] = 0u; G[25] = word(rt, G[2] + 0x30u); target = G[25]; } G[16] = s0; jump(ctx, target); }
void func_08869BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[17] = G[5]; G[16] = G[6]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, 0x08B344E0u); G[5] = G[17]; G[4] = G[18] + 0x1B0u; G[6] = G[16]; call_here(rt, ctx, 0x0886A020u); G[4] = G[2]; G[5] = 4u; if (G[2] != 0u) { call_here(rt, ctx, 0x0886A9A0u); G[4] = G[2]; call_here(rt, ctx, 0x08B93734u); ctx.fpr[12] = 0.0f; ctx.fpr[13] = ctx.fpr[20]; G[4] = G[2]; ctx.fpr[14] = ctx.fpr[12]; if (G[2] != 0u) call_here(rt, ctx, 0x0887051Cu); } G[4] = G[18]; ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_0886CB54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x30u); G[18] = G[6]; put(rt, frame.at(0x10u), G[5]); G[5] = frame.at(0x10u); G[16] = word(rt, G[4] + 4u); G[17] = G[16] + 0x1DCu; G[4] = G[17]; call_here(rt, ctx, 0x08B93AE8u); G[16] += 0x1E0u; G[7] = G[2]; G[4] = G[17]; G[5] = G[2]; G[6] = frame.at(0u); bool ins = true; if (G[16] != G[2]) { G[2] = word(rt, G[2] + 0x10u); G[3] = word(rt, frame.at(0x10u)); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) put(rt, frame.at(0u), G[3]); else ins = false; } else { G[3] = word(rt, frame.at(0x10u)); put(rt, frame.at(0u), G[3]); } if (ins) { store8(frame.at(4u), 0u); call_here(rt, ctx, 0x08B940D8u); G[7] = G[2]; } store8(G[7] + 0x14u, G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08870218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBF948u; G[17] = G[4] + 0xCu; G[16] = G[4]; G[3] = word(rt, G[4] + 0xCu); put(rt, G[4], G[2]); G[2] = word(rt, G[3] + 0x80u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x2Cu); call_here(rt, ctx, G[3]); G[2] = word(rt, G[16] + 0xCu); put(rt, G[2] + 0x80u, 0u); } G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0xCu); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0xCu, 0u); } G[2] = 0x08CBF428u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08876D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 text = frame.at(4u);
    call_args(rt, ctx, 0x08AB85F0u, {text});
    const u32 name = call_args(rt, ctx, 0x08AB856Cu, {text});
    const u32 resolved = call_args(rt, ctx, 0x08B77BFCu, {name});
    const u32 object = call_args(rt, ctx, 0x0886AFA8u, {word(rt, self)});
    call_args(rt, ctx, 0x0887C968u, {object + 4u, resolved != 0u ? 1u : 0u});
    release_string(rt, ctx, word(rt, text), frame.at(0u));
}
void func_08896D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x64u; G[2] = G[4]; store8(G[4] + 0xCu, G[3]); G[3] = 0x6Du; G[5] = 0x30u; G[6] = 0x31u; G[7] = 0x62u; G[8] = 0u; G[9] = 0u; store8(G[2] + 0x10u, G[3]); G[3] = 0x32u; put(rt, G[4], G[8]); put(rt, G[4] + 4u, G[9]); G[4] += 0x20u; store8(G[2] + 0x17u, G[6]); store8(G[2] + 0x18u, G[7]); store8(G[2] + 0x1Au, G[5]); store8(G[2] + 0x1Bu, G[3]); store8(G[2] + 0x8u, 0u); store8(G[2] + 0x9u, 0u); store8(G[2] + 0xAu, 0u); store8(G[2] + 0xBu, 0u); store8(G[2] + 0xDu, G[5]); store8(G[2] + 0xEu, G[5]); store8(G[2] + 0xFu, G[6]); store8(G[2] + 0x11u, G[5]); store8(G[2] + 0x12u, G[5]); store8(G[2] + 0x13u, G[6]); store8(G[2] + 0x14u, G[7]); store8(G[2] + 0x15u, G[5]); store8(G[2] + 0x16u, G[5]); store8(G[2] + 0x19u, G[5]); store8(G[2] + 0x1Cu, 0u); store8(G[2] + 0x1Du, 0u); store8(G[2] + 0x1Eu, 0u); store8(G[2] + 0x1Fu, 0u); jump(ctx, 0x08896D7Cu); }
void func_088D6B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = true; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4] + 4u; G[16] = G[4]; G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E5218u); if (G[2] == 0u) tail = false; } if (tail) { G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x088D4988u); G[4] = G[17]; call_here(rt, ctx, 0x088D4CDCu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x088DC974u); G[5] = word(rt, G[16] + 0x130u); G[4] = G[17]; G[5] += 0x5Cu; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x088D5028u); }
void func_088D7658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; bool tail = false; { GuestFrame frame(ctx, 0x20u); G[19] = G[4]; G[18] = G[8]; G[17] = G[7]; G[2] = word(rt, G[4] + 0x640u); G[16] = G[6]; if (G[2] != 0u) { call_here(rt, ctx, 0x088D9B9Cu); G[3] = word(rt, G[2] + 0x1Cu); G[5] = G[2]; G[2] = 1u; G[6] = G[16]; G[7] = G[17]; G[8] = G[18]; if (G[3] == G[2]) { G[4] = word(rt, G[19] + 0x640u); G[5] = word(rt, G[5] + 0x2E4u); tail = true; } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; if (tail) jump(ctx, 0x088E46DCu); }
void func_088DD984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0xFFFFFFFFu; G[16] = G[4]; G[2] = word(rt, G[4] + 0x640u); if (G[2] != 0u) { call_here(rt, ctx, 0x088D9F80u); G[7] = G[2]; G[5] = 0u; G[6] = 0u; bool found = false; for (;;) { G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = word(rt, G[16] + 0xC0u); G[2] += G[6]; G[3] = word(rt, G[2] + 0x2E4u); G[6] += 0x2F8u; if (G[3] != 0u) { G[2] = word(rt, G[16] + 0x130u); G[4] = load8(G[3] + 2u); G[3] = load8(G[2]); if (G[3] == G[4]) { const bool last = G[17] == 0u; G[17] -= 1u; if (last) { found = true; break; } } } G[5] += 1u; } if (!found) G[5] = 0xFFFFFFFFu; } G[2] = G[5]; } G[16] = s0; G[17] = s1; }
void func_088E59A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 4u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x10u); call_here(rt, ctx, G[3]); G[2] = word(rt, G[16] + 4u); put(rt, G[2] + 0x530u, 0u); } G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); put(rt, G[3] + 0x530u, 0u); G[2] = word(rt, G[16]); G[4] = word(rt, G[2] + 0xA8u); call_here(rt, ctx, 0x08B08D54u); G[2] = word(rt, G[16]); G[5] = 0u; G[4] = word(rt, G[2] + 0xA8u); call_here(rt, ctx, 0x08B08D84u); G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x38u); call_here(rt, ctx, G[2]); G[2] = G[2] != 0u ? 1u : 0u; } G[16] = s0; }
void func_088E6864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    toggle_flag(rt, ctx, 0u, 1u);
}
void func_088E68F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    toggle_flag(rt, ctx, 1u, 0u);
}
void func_088F65E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); put_float(rt, G[4] + 0x18u, ctx.fpr[12]); ctx.fpr[12] = ctx.fpr[13]; put_float(rt, G[4] + 0x20u, ctx.fpr[13]); call_here(rt, ctx, 0x08B70F28u); G[6] = 0x54442D18u; G[7] = 0x400921FBu; G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71394u); G[7] = 0x40668000u; G[6] = 0u; G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71720u); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71EC8u); ctx.fpr[20] = ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[0]; call_here(rt, ctx, 0x08B662C4u); ctx.fpr[12] = ctx.fpr[20]; put_float(rt, G[16] + 0x24u, ctx.fpr[0]); call_here(rt, ctx, 0x08B66524u); put_float(rt, G[16] + 0x28u, ctx.fpr[0]); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; }
void func_088FC630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 target = 0u;
    u32 counted = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        counted = word(rt, self + 0x14u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x14u, 0u);
        target = release_counted(rt, ctx, counted);
    }
    if (target != 0u) tail_call(ctx, target, {counted});
}
void func_08904970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x40u); G[17] = frame.at(0xCu); G[8] = G[7]; G[16] = G[4] + 0x20u; G[7] = G[6]; G[6] = G[17]; G[19] = G[4]; G[4] = G[16]; G[18] = G[5]; G[5] = frame.at(0u); call_here(rt, ctx, 0x088FB8F8u); G[4] = G[19]; G[5] = G[18]; G[6] = G[16]; G[8] = G[17]; G[7] = frame.at(0u); G[9] = 0u; G[11] = 0u; G[3] = 0u; if (G[2] != 0u) { G[10] = load16(G[19] + 0x4Cu); G[10] -= 1u; call_here(rt, ctx, 0x089041C8u); G[3] = G[2]; } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08909A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); ctx.fpr[1] = ctx.fpr[12]; G[16] = G[4]; ctx.fpr[2] = ctx.fpr[13]; G[3] = word(rt, G[4]); ctx.fpr[3] = ctx.fpr[14]; if (G[3] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 4u); G[2] = 0u; G[4] = G[3]; bool same = ctx.fpr[0] == ctx.fpr[1]; if (same) { ctx.fpr[0] = float_at(rt, G[16] + 8u); same = ctx.fpr[2] == ctx.fpr[0]; if (same) { ctx.fpr[0] = float_at(rt, G[16] + 0xCu); same = ctx.fpr[3] == ctx.fpr[0]; } } if (!same) { put_float(rt, G[16] + 4u, ctx.fpr[1]); put_float(rt, G[16] + 8u, ctx.fpr[2]); put_float(rt, G[16] + 0xCu, ctx.fpr[3]); if (G[3] != 0u) { call_here(rt, ctx, 0x08909B60u); G[2] = word(rt, G[2]); } put(rt, G[16] + 0x10u, G[2]); } } } G[16] = s0; }
void func_0890F10C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x0890EFC0u); G[16] = word(rt, G[16] + 4u); G[3] = G[16] + 4u; if (G[16] != 0u) { G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); target = G[25]; } } } } G[16] = s0; if (target != 0u) jump(ctx, target); }
void func_089386DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x08938684u); G[3] = G[2]; G[2] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; if (G[3] != G[2]) { G[7] = G[3]; G[6] = 0u; G[8] = G[3] + 1u; for (;;) { G[5] = G[7] - 1u; G[3] = G[5] << 3; bool have = false; if (static_cast<std::int32_t>(G[5]) >= 0) { G[4] = word(rt, G[17] + 4u); G[2] = word(rt, G[4] + 8u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] += G[4]; if (G[2] != 0u) { G[3] = word(rt, G[3] + 0x10u); have = true; } } if (!have) G[3] = 0xFFFFFFFFu; G[6] += 1u; if (G[3] != G[16]) break; if (G[8] == G[6]) break; G[7] = G[5]; } } G[2] = G[7]; } G[16] = s0; G[17] = s1; }
void func_0893D0F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[2] <<= 2; G[2] += G[4]; ctx.fpr[0] = float_at(rt, G[2] + 0x1374u); ctx.fpr[1] = float_at(rt, G[2] + 0x136Cu); G[3] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[13]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; if (G[3] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0x1394u); ctx.fpr[2] = 0.0f; const bool zero = ctx.fpr[0] == ctx.fpr[2]; ctx.fpr[12] = ctx.fpr[0] * ctx.fpr[12]; if (!zero) { if (ctx.fpr[2] <= ctx.fpr[12]) { if (G[5] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0x13Cu); ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[0]; ctx.fpr[0] = -ctx.fpr[0]; } else { ctx.fpr[0] = float_at(rt, G[4] + 0x138u); ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[0]; } } else { if (G[5] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0x138u); ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[0]; ctx.fpr[0] = -ctx.fpr[0]; } else { ctx.fpr[0] = float_at(rt, G[4] + 0x13Cu); ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[0]; } } ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; } } ctx.fpr[0] = ctx.fpr[1]; }
void func_089BA264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(8u); call_here(rt, ctx, 0x089BA1E4u); G[4] = 0x08CA5C04u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x114u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = frame.at(4u); put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089BCF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = frame.at(4u); G[16] = G[4]; call_here(rt, ctx, 0x089BA9B4u); G[7] = 0x08CA5DA4u; G[6] = G[2]; G[4] = G[17]; G[5] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x089BCE20u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089BAF40u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } } G[16] = s0; G[17] = s1; }
void func_089BEE8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = 1u; G[17] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08CA5E6Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2] != 0u ? 1u : 0u; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x089BEE4Cu); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA5EB4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); store8(G[16] + 0x2ACu, G[18]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089CBAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A0D658u); G[2] = 0x08CC9FD8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x12Cu; G[5] = G[16] + 0x138u; G[6] = G[16] + 0x144u; G[7] = G[16] + 0x150u; G[3] = 0x08CCA230u; G[2] = 2u; put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0x120u, G[2]); put(rt, G[16] + 0x124u, 0u); put(rt, G[16] + 0x128u, 0u); put(rt, G[16] + 0x12Cu, 0u); put(rt, G[4] + 0x8u, 0u); put(rt, G[4] + 0x4u, 0u); put(rt, G[5] + 0x8u, 0u); put(rt, G[5] + 0x4u, 0u); put(rt, G[16] + 0x138u, 0u); put(rt, G[16] + 0x144u, 0u); put(rt, G[6] + 0x8u, 0u); put(rt, G[6] + 0x4u, 0u); put(rt, G[7] + 0x8u, 0u); put(rt, G[7] + 0x4u, 0u); put(rt, G[16] + 0x150u, 0u); put(rt, G[16] + 0x160u, 0u); put(rt, G[16] + 0x15Cu, 0u); } G[16] = s0; }
void func_089DD4EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    handle_two_steps(rt, ctx, 0x089DCE68u, 0u, 0u, 1u);
}
void func_089DEF28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    handle_two_steps(rt, ctx, 0x089DEBD4u, 0u, 1u, 0u);
}
void func_089DEFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    handle_two_steps(rt, ctx, 0x089DEBD4u, 1u, 0u, 0u);
}
void func_08A02A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = 0x08CF0000u; G[16] = G[4]; call_here(rt, ctx, 0x0896BDF8u); G[2] = 0x08CB9A00u; put(rt, G[16], G[2]); G[2] = 0x08CB9C58u; G[4] = G[16] + 0xE8u; G[5] = G[16]; put(rt, G[16] + 0x38u, G[2]); call_here(rt, ctx, 0x089B883Cu); G[2] = 0x08D74FDCu; put(rt, G[16] + 0x104u, G[2]); G[4] = G[16] + 0x108u; G[5] = 0x08CF6ED8u; call_here(rt, ctx, 0x089E4C14u); G[5] = 0x08CF6ED8u; G[4] = G[16] + 0x10Cu; call_here(rt, ctx, 0x089E4C14u); G[2] = word(rt, G[16] + 0x110u); put(rt, G[16] + 0x114u, 0u); G[2] &= ~1u; G[2] |= 6u; G[2] &= ~8u; put(rt, G[16] + 0x110u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08A21EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[2] = 0xFFFFu; G[4] = 0x08CF64A8u; G[5] = 0x08CAAA50u; G[7] = 0x08A21E3Cu; G[8] = 0x08A2310Cu; G[9] = 0x08A21E60u; G[6] = 0x18u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08A92114u); G[4] = 0x08CF64CCu; G[5] = 0x08C8E518u; G[6] = 0xAu; call_here(rt, ctx, 0x08B2095Cu); G[4] = 0x08CF64C4u; G[5] = 0x08C8E4D8u; G[6] = 8u; target = 0x08B2095Cu; } } } if (target != 0u) jump(ctx, target); }
void func_08A322A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4]); G[7] = G[4]; G[3] = 1u; if (G[2] == 0u) { G[2] = G[3]; return; } if (static_cast<std::int32_t>(G[6]) <= 0) { G[4] = word(rt, G[7] + 4u); G[2] = G[7] + G[4]; G[3] = 1u; store8(G[2] + 8u, 0u); G[2] = G[3]; return; } G[4] = word(rt, G[4] + 4u); G[2] = static_cast<std::int32_t>(G[4]) < 0x3FF ? 1u : 0u; G[3] = 0u; if (G[2] == 0u) { store8(G[7] + 0x407u, 0u); G[2] = G[3]; return; } G[9] = 0u; G[2] = G[9] + G[5]; for (;;) { G[3] = load8(G[2]); G[9] += 1u; G[2] = G[7] + G[4]; G[4] += 1u; G[8] = static_cast<std::int32_t>(G[4]) < 0x3FF ? 1u : 0u; store8(G[2] + 8u, G[3]); put(rt, G[7] + 4u, G[4]); if (G[6] == G[9]) { G[2] = G[7] + G[4]; G[3] = 1u; store8(G[2] + 8u, 0u); G[2] = G[3]; return; } if (G[8] == 0u) { G[3] = 0u; store8(G[7] + 0x407u, 0u); G[2] = G[3]; return; } G[2] = G[9] + G[5]; } }
void func_08A356E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[17] = G[5]; if (G[6] == G[2]) { G[16] = word(rt, G[7]); G[4] = 0x08CACEB8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = frame.at(0u); G[17] = G[2] & 0xFFu; call_here(rt, ctx, 0x08A354ECu); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CACF04u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08A352F4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; }
void func_08A3D588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); if (static_cast<std::int32_t>(G[5]) > 0) { G[16] = word(rt, G[6]); G[4] = 0x08CAD7CCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(4u)); G[4] = 0x08D69524u; call_here(rt, ctx, 0x08B49814u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } } G[16] = s0; }
void func_08A40B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[2] = 0xFFFFu; G[4] = 0x08CF6864u; G[5] = 0x08CADB58u; G[7] = 0x08A40AF0u; G[8] = 0x08A42210u; G[9] = 0x08A40B14u; G[6] = 0x48u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08A92114u); G[4] = 0x08CF684Cu; G[5] = 0x08CADB64u; G[7] = 0x08A40B28u; G[8] = 0x08A42364u; G[9] = 0x08A40B4Cu; G[6] = 0x20u; target = 0x08A92114u; } } } if (target != 0u) jump(ctx, target); }
void func_08A50104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[4] = 0x118u; G[18] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B62344u); G[17] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08B0CC04u); G[4] = G[17]; G[5] = G[16]; G[6] = 0u; G[7] = 0u; call_here(rt, ctx, 0x08B0C99Cu); if (static_cast<std::int32_t>(G[2]) >= 0) put(rt, G[18], 0u); else { G[2] = 0x80460005u; G[4] = G[17]; put(rt, G[18], G[2]); if (G[17] != 0u) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 4u); G[17] = 0u; call_here(rt, ctx, G[2]); } } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A57548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x28u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[5] = G[16]; G[4] = frame.at(0u); if (G[2] == 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08A76DF8u); ctx.fpr[20] = ctx.fpr[0]; } else { call_here(rt, ctx, 0x08A56884u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAF778u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[20] = ctx.fpr[0]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } ctx.fpr[0] = ctx.fpr[20]; ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; }
void func_08A5D9D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = 0u; G[7] = 0u; G[17] = word(rt, G[4] + 0x1Cu); G[16] = word(rt, G[4] + 0x18u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08B6EE4Cu); G[3] = G[17] << 31; G[4] = G[16]; G[5] = G[17]; if (static_cast<std::int32_t>(G[2]) > 0) call_here(rt, ctx, 0x08B6EE80u); else { G[6] = G[16] >> 1; G[6] = G[3] | G[6]; G[7] = G[17] >> 1; G[8] = G[16] & 1u; G[9] = 0u; G[4] = G[8] | G[6]; G[5] = G[9] | G[7]; call_here(rt, ctx, 0x08B6EE80u); G[4] = G[2]; G[5] = G[3]; G[6] = G[2]; G[7] = G[3]; call_here(rt, ctx, 0x08B71328u); } } G[16] = s0; G[17] = s1; }
void func_08A81F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[6] = G[5]; G[16] = G[4]; G[5] = G[16]; G[17] = G[4] + 0x14u; G[4] = G[18]; call_here(rt, ctx, 0x08A818A0u); G[5] = G[18]; G[2] = word(rt, frame.at(4u)); G[4] = G[17]; if (G[2] != 0u) { G[16] += 0x30u; call_here(rt, ctx, 0x08C2973Cu); G[3] = word(rt, G[17] + 0x14u); G[4] = frame.at(0u); if (G[3] == 0u) { call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A997D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = word(rt, G[4] + 0x18u); G[16] = word(rt, G[4] + 0x14u); G[18] = G[5]; if (G[16] != G[17]) { G[2] = G[17] - G[16]; G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 2); G[2] = 1u; G[6] = 0u; if (G[3] != G[2]) { do { G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[6] += 1u; } while (G[3] != G[2]); G[6] <<= 1; } G[4] = G[16]; G[5] = G[17]; G[7] = G[18]; call_here(rt, ctx, 0x08C2EF40u); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; tail = true; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08C2E8F4u); }
void func_08AA0984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[4]; G[4] = frame.at(0u); G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08A85C88u); G[4] = frame.at(0u); G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; call_here(rt, ctx, 0x08AA0690u); const bool ok = G[2] != 0u; G[2] = word(rt, frame.at(0u)); G[16] = 0u; if (ok) { G[16] = 1u; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A855E8u); put(rt, G[19], G[2]); } else G[16] = 0u; } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AAE994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD5618u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x3Cu; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x3Cu); call_here(rt, ctx, 0x08C2ADCCu); G[2] = word(rt, G[17] + 0x3Cu); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] = G[5] - G[2]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 2); G[5] <<= 2; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[4] = G[17] + 0x24u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C2AE10u); G[4] = G[17]; call_here(rt, ctx, 0x08AAE904u); G[4] = G[17]; G[5] = 0x48u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[16] = s0; G[17] = s1; }
void func_08ABFDD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0xCu; G[16] = G[4]; G[4] = G[18]; G[17] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[5] = word(rt, G[16] + 4u); if (G[5] != 0u) { G[3] = word(rt, G[17] + 0x10u); for (;;) { G[2] = word(rt, G[5] + 0x10u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] = G[16]; if (G[2] != 0u) break; G[5] = word(rt, G[5] + 8u); if (G[5] == 0u) break; } } G[4] = G[16]; G[6] = G[17]; call_here(rt, ctx, 0x08B1FA04u); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_08AC1AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = load16(G[4] + 0x3Eu); G[2] &= 1u; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x38u); G[5] = 0u; call_here(rt, ctx, 0x08AC22ECu); G[2] = 0u; } else { G[8] = load16(G[4] + 0x3Cu); G[2] = 0u; if (G[8] != 0u) { G[4] = word(rt, G[4] + 0x38u); G[2] = word(rt, G[4] + 8u); G[6] = G[4] + 0x34u; bool hit = G[5] == G[2]; if (!hit) { G[7] = G[4] + 0x2Cu; G[3] = 0u; for (;;) { G[3] += 1u; G[2] = G[3] < G[8] ? 1u : 0u; if (G[2] == 0u) break; G[4] = G[7]; G[7] += 0x2Cu; G[2] = word(rt, G[6]); G[6] += 0x2Cu; if (G[2] == G[5]) { hit = true; break; } } } if (hit) { G[5] = 0u; call_here(rt, ctx, 0x08AC22ECu); G[2] = 1u; } else G[2] = 0u; } } } }
void func_08AC1EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD6530u; G[17] = G[5]; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = G[4] + 4u; call_here(rt, ctx, 0x08AC263Cu); store16(G[16] + 0x3Cu, 0u); G[2] = word(rt, G[16] + 0x3Cu); put(rt, G[16] + 0x30u, G[17]); G[2] &= ~(1u << 16); put(rt, G[16] + 0x3Cu, G[2]); G[2] = 0x45565346u; put(rt, G[16] + 0x38u, 0u); put(rt, G[16] + 0x40u, 0u); G[3] = word(rt, G[17]); bool clear = true; if (G[3] == G[2]) { G[3] = load16(G[17] + 8u); G[2] = 0xFFFEu; if (G[3] == G[2]) { G[2] = word(rt, G[17] + 0xCu); G[2] = G[17] + G[2]; put(rt, G[16] + 0x34u, G[2]); clear = false; } } if (clear) { put(rt, G[16] + 0x34u, 0u); put(rt, G[16] + 0x30u, 0u); } } G[16] = s0; G[17] = s1; }
void func_08AC391C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[4] + 0x9DCu; call_here(rt, ctx, 0x08AC38ACu); G[3] = G[2]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0xC98u, G[2]); G[2] = 0u; } else { G[2] = sext(load8(G[16] + 0xCA8u), 8); G[4] = G[16]; G[5] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0xC9Cu); if (G[3] != G[2]) { put(rt, G[16] + 0xC9Cu, G[3]); put(rt, G[16] + 0xCA0u, 0u); put(rt, G[16] + 0xCA4u, 0u); } call_here(rt, ctx, 0x08AC30B4u); G[5] = G[2]; } else { G[2] = word(rt, G[16] + 0xC98u); G[4] = G[2] + 1u; if (static_cast<std::int32_t>(G[2]) >= 0) { ctx.lo = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) / static_cast<std::int32_t>(G[3])); ctx.hi = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) % static_cast<std::int32_t>(G[3])); G[5] = ctx.hi; } } G[2] = 1u; put(rt, G[16] + 0xC98u, G[5]); } } G[16] = s0; }
void func_08AF8590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; std::uint32_t target = 0x08B567F4u; { GuestFrame frame(ctx, 0x10u); G[18] = 0x08D40000u; G[2] = 0x08D472ACu; G[16] = G[5]; G[3] = load8(G[2] + 0x50u); G[2] = G[4]; G[4] = G[5]; if (G[3] != 0u) { G[3] = G[6] << 7; G[17] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[6]) >= 0) { G[2] = word(rt, G[2] + 0x9Cu); G[2] += G[3]; G[17] = sext(load16(G[2] + 0xAu), 16); } call_here(rt, ctx, 0x08B569A0u); G[4] = 0x08D472ACu; G[5] = G[17]; call_here(rt, ctx, 0x08AFBD3Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B567F4u); target = 0x08B56940u; } } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, target); }
void func_08B09D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = 0x08D40000u; G[17] = G[4]; G[4] = 0x08D473C0u; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[5] = G[16]; G[2] = word(rt, G[17]); G[16] = G[2] + 0xD4u; G[3] = load8(G[16] + 0x29u); G[4] = G[16]; bool ok = false; if (G[3] != 0u) { call_here(rt, ctx, 0x08B096DCu); G[4] = G[16]; if (G[2] != 0u) ok = true; } if (ok) { call_here(rt, ctx, 0x08B09264u); store8(G[16] + 0x28u, 0u); G[4] = 0x08D473C0u; store8(G[16] + 0x29u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[2] = 1u; } else { G[4] = 0x08D473C0u; call_here(rt, ctx, 0x08B3457Cu); G[2] = 0u; } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B09FA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x60u); G[2] = 0x08D40000u; G[18] = frame.at(0x20u); G[17] = G[4]; G[4] = G[18]; G[3] = word(rt, 0x08D47348u); G[16] = G[5]; if (G[3] != 0u) { call_here(rt, ctx, 0x08B09F98u); G[4] = G[17]; G[5] = G[16]; G[6] = G[18]; call_here(rt, ctx, 0x08B09B5Cu); G[2] = 3u; G[3] = 0xFFFFFFFFu; put(rt, frame.at(0u), G[2]); G[4] = frame.at(0u); G[2] = 0x28u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), G[18]); put(rt, frame.at(0x18u), G[2]); put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); put(rt, frame.at(0xCu), G[3]); put(rt, frame.at(0x1Cu), 0u); call_here(rt, ctx, 0x08B09538u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B0B508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[17] = 0u; G[16] = G[4] + 0x20u; G[3] = word(rt, G[4] + 0xCu); for (;;) { G[2] = __builtin_bswap32(G[3]); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[17] += 1u; if (G[2] == 0u) break; G[2] = word(rt, G[16] + 0xCu); G[2] = __builtin_bswap32(G[2]); G[4] = 0x10u; if (G[2] == 0u) { G[2] = word(rt, G[16] + 4u); G[5] = word(rt, G[16] + 8u); G[2] = __builtin_bswap32(G[2]); G[5] = __builtin_bswap32(G[5]); G[2] = __builtin_bswap32(G[2]); G[5] += 0xFu; put(rt, G[16] + 0xCu, G[2]); G[5] &= ~0xFu; call_here(rt, ctx, 0x08B2B4D8u); G[2] = __builtin_bswap32(G[2]); put(rt, G[16] + 4u, G[2]); G[3] = word(rt, G[18] + 0xCu); } G[16] += 0x10u; } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B0B59C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[17] = 0u; G[19] = 0u; G[16] = G[4] + 0x20u; G[3] = word(rt, G[4] + 0xCu); for (;;) { G[2] = __builtin_bswap32(G[3]); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[17] += 1u; if (G[2] == 0u) break; G[2] = word(rt, G[16] + 0xCu); G[2] = __builtin_bswap32(G[2]); if (G[2] == 0u) { G[16] += 0x10u; continue; } G[4] = word(rt, G[16] + 4u); G[4] = __builtin_bswap32(G[4]); call_here(rt, ctx, 0x08B35254u); G[2] = word(rt, G[16] + 0xCu); put(rt, G[16] + 0xCu, G[19]); G[2] = __builtin_bswap32(__builtin_bswap32(G[2])); put(rt, G[16] + 4u, G[2]); G[3] = word(rt, G[18] + 0xCu); G[16] += 0x10u; } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B10DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[4] + 0x14u; G[18] = G[5] + 0x3Cu; G[17] = G[4]; G[4] = G[20]; G[16] = G[5]; call_here(rt, ctx, 0x08B34888u); G[19] = G[16] + 4u; G[5] = G[18]; G[4] = G[17] + 0x40u; call_here(rt, ctx, 0x08B1FCD4u); G[4] = G[19]; call_here(rt, ctx, 0x08B34888u); G[2] = 1u; put(rt, G[16] + 0xA4u, G[2]); G[5] = G[18]; G[4] = G[17] + 0x4Cu; call_here(rt, ctx, 0x08B1FBECu); G[4] = G[19]; call_here(rt, ctx, 0x08B34B24u); G[4] = G[17] + 0x64u; call_here(rt, ctx, 0x08B28C74u); G[4] = G[20]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; jump(ctx, 0x08B34B24u); }
void func_08B12C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[16] + 0x1B4u); call_here(rt, ctx, 0x08B33A24u); G[4] = word(rt, G[16] + 0x23Cu); call_here(rt, ctx, 0x08B154C0u); G[4] = word(rt, G[16] + 0x248u); call_here(rt, ctx, 0x08B154C0u); G[4] = word(rt, G[16] + 0x24Cu); call_here(rt, ctx, 0x08B154C0u); G[4] = word(rt, G[16] + 0x268u); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[16] + 0x26Cu); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[16] + 0x270u); call_here(rt, ctx, 0x08B154C0u); call_here(rt, ctx, 0x08B3377Cu); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x260u, G[2]); put(rt, G[16] + 0x4D4u, 0u); put(rt, G[16] + 0x1B4u, G[2]); put(rt, G[16] + 0x238u, G[2]); put(rt, G[16] + 0x248u, 0u); put(rt, G[16] + 0x24Cu, 0u); store8(G[16] + 0x258u, 0u); put(rt, G[16] + 0x25Cu, G[2]); put(rt, G[16] + 0x264u, 0u); put(rt, G[16] + 0x268u, 0u); put(rt, G[16] + 0x26Cu, 0u); put(rt, G[16] + 0x2D0u, 0u); } G[16] = s0; }
void func_08B17604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08D59100u; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); G[2] = 0x8000u; G[16] = G[4]; put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08B18264u); G[2] = 0x08CD91A0u; put(rt, G[16], G[2]); G[4] = G[16] + 0x18D4u; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x1900u; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x192Cu; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x1958u; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x1984u; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x19B0u; call_here(rt, ctx, 0x08B28D34u); put(rt, G[16] + 0x19FCu, 0u); store8(G[16] + 0x19DCu, 0u); store8(G[16] + 0x19DDu, 0u); put(rt, G[16] + 0x19E4u, 0u); put(rt, G[16] + 0x19E8u, 0u); put(rt, G[16] + 0x19ECu, 0u); put(rt, G[16] + 0x19F0u, 0u); } G[16] = s0; }
void func_08B17698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[2] = 0xFFFFu; G[4] = 0x08D62B2Cu; G[5] = 0x08B17044u; G[6] = 0u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08B1A8E4u); G[4] = 0x08D6112Cu; call_here(rt, ctx, 0x08B17604u); G[4] = 0x08D61100u; target = 0x08B28D34u; } } else { const bool zero = G[4] == 0u; G[4] = 0x08D60000u; if (zero) { G[2] = 0xFFFFu; G[4] = 0x08D61100u; if (G[5] == G[2]) { call_here(rt, ctx, 0x08B28BE0u); G[4] = 0x08D6112Cu; target = 0x08B17178u; } } } } if (target != 0u) jump(ctx, target); }
void func_08B1AC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[9] = G[5]; G[3] = G[5] < 0x800u ? 1u : 0u; G[13] = 3u; G[3] = G[13] - G[3]; G[8] = G[5] & 0x3Fu; G[2] = 0xFFFFFFC0u; G[5] = G[5] < 0x80u ? 1u : 0u; G[7] = (G[9] >> 6) & 0x1Fu; G[11] = G[3] - G[5]; G[7] |= G[2]; G[3] = 0xFFFFFF80u; G[2] = 2u; G[8] |= G[3]; G[10] = G[4]; G[12] = G[6] & 0xFFu; if (G[11] == G[2]) { store8(G[4], G[7]); store8(G[4] + 1u, G[8]); G[2] = G[10] + G[11]; } else { G[4] = 0xFFFFFF80u; G[3] = (G[9] >> 12) & 0xFu; G[5] = (G[9] >> 6) & 0x3Fu; G[2] = 0xFFFFFFE0u; G[3] |= G[2]; G[5] |= G[4]; if (G[11] == G[13]) { store8(G[10], G[3]); store8(G[10] + 1u, G[5]); store8(G[10] + 2u, G[8]); G[2] = G[10] + G[11]; } else { G[2] = 1u; if (G[11] == G[2]) store8(G[10], G[9]); G[2] = G[10] + G[11]; } } if (G[12] == 0u) G[2] = G[10]; }
void func_08B2DC98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; call_here(rt, ctx, 0x08C7A888u); G[4] = G[2]; G[2] = 0x000F0000u; G[3] = 0x08D70000u; G[2] = G[17] & G[2]; put(rt, 0x08D68D5Cu, G[17]); if (G[2] != 0u) { G[3] = 0x08D70000u; G[2] = word(rt, 0x08D68D60u); G[2] += 1u; put(rt, 0x08D68D60u, G[2]); } call_here(rt, ctx, 0x08C7A8A8u); G[2] = 0x08D70000u; G[16] = word(rt, 0x08D773F8u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 4u); for (;;) { G[2] = word(rt, G[3] + 8u); G[4] = G[17]; if (G[2] != 0u) { G[5] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } G[16] = word(rt, G[16]); if (G[16] == 0u) break; G[3] = word(rt, G[16] + 4u); } } G[2] = 0u; } G[16] = s0; G[17] = s1; }
void func_08B3433C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[5] = 0x02425818u; G[16] = G[4]; G[2] = frame.at(4u); put(rt, G[4], 0u); G[4] = 0x08CB5504u; put(rt, G[16] + 4u, 0u); G[6] = frame.at(0u); G[7] = 4u; put(rt, G[16] + 8u, 0u); G[8] = 0u; G[9] = 0u; put(rt, G[16] + 0xCu, 0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08C7A820u); G[5] = word(rt, frame.at(8u)); if (G[2] == 0u) { G[6] = word(rt, frame.at(0x10u)); G[4] = word(rt, frame.at(0x14u)); G[3] = word(rt, frame.at(4u)); put(rt, G[16] + 4u, G[5]); put(rt, G[16], G[3]); put(rt, G[16] + 0xCu, G[4]); put(rt, G[16] + 8u, G[6]); } } G[16] = s0; }
void func_08B3ED84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; G[2] = word(rt, G[5]); { GuestFrame frame(ctx, 0x10u); G[3] = G[4]; G[16] = 0xFFFFFFFFu; if (G[2] != 0u) { G[2] = G[2] < 0x4FDu ? 1u : 0u; if (G[2] != 0u) { G[16] = 0u; G[8] = 0x20u; G[6] = G[4]; bool call = false; G[2] = word(rt, G[6]); for (;;) { G[7] = G[16] << 3; G[6] += 0x18u; G[4] = G[16] << 5; if (G[2] == 0u) { G[2] = 0x1Fu; if (G[16] == G[2]) G[16] = 0xFFFFFFFFu; else call = true; break; } G[16] += 1u; if (G[16] == G[8]) { G[7] = 0x100u; G[4] = 0x400u; call = true; break; } G[2] = word(rt, G[6]); } if (call) { G[4] -= G[7]; G[4] = G[3] + G[4]; call_here(rt, ctx, 0x08B3E744u); } } } G[2] = G[16]; } G[16] = s0; }
void func_08B4135C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0xCu; G[6] = G[10]; G[16] = G[4]; G[4] = G[4] + 0x8DCu; G[7] = G[11]; if (G[8] == G[2]) { call_here(rt, ctx, 0x08B3F098u); G[17] = G[2]; G[4] = G[16]; G[5] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x8Cu); call_here(rt, ctx, G[2]); put(rt, G[17] + 0x30u, G[2]); G[4] = G[16]; G[5] = G[17]; put(rt, G[17] + 0x34u, G[3]); G[6] = 0x2711u; G[7] = frame.at(0u); G[8] = 0xCu; G[9] = 1u; put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); call_here(rt, ctx, 0x08B40950u); } } } G[16] = s0; G[17] = s1; }
void func_08B439A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[4] + 0x8DCu; G[17] = 0u; call_here(rt, ctx, 0x08B3F3C8u); G[3] = G[2]; if (G[2] != 0u) { G[6] = 0u; G[7] = 0u; G[5] = word(rt, G[3]); for (;;) { G[4] = load16(G[5] + 2u); G[2] = G[6] < G[4] ? 1u : 0u; G[3] = word(rt, G[3] + 8u); if (G[2] != 0u) { G[7] = load16(G[5]); G[6] = G[4]; } if (G[3] == 0u) break; G[5] = word(rt, G[3]); } G[17] = G[7]; } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x90u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] == G[17]) { G[4] = G[16]; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B437C0u); }
void func_08B4AB04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[6]); ctx.fpr[4] = float_at(rt, G[6] + 4u); ctx.fpr[1] = float_at(rt, G[5] + 0x0u); ctx.fpr[2] = float_at(rt, G[5] + 0xCu); ctx.fpr[0] = float_at(rt, G[5] + 0x18u); ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[3]; ctx.fpr[2] = ctx.fpr[2] * ctx.fpr[4]; ctx.fpr[3] = float_at(rt, G[6] + 8u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[3]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put_float(rt, G[4] + 0x0u, ctx.fpr[1]); ctx.fpr[3] = float_at(rt, G[6]); ctx.fpr[4] = float_at(rt, G[6] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 0x4u); ctx.fpr[2] = float_at(rt, G[5] + 0x10u); ctx.fpr[1] = float_at(rt, G[5] + 0x1Cu); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[3]; ctx.fpr[2] = ctx.fpr[2] * ctx.fpr[4]; ctx.fpr[3] = float_at(rt, G[6] + 8u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; put_float(rt, G[4] + 0x4u, ctx.fpr[0]); ctx.fpr[3] = float_at(rt, G[6]); ctx.fpr[4] = float_at(rt, G[6] + 4u); ctx.fpr[1] = float_at(rt, G[5] + 0x8u); ctx.fpr[2] = float_at(rt, G[5] + 0x14u); ctx.fpr[0] = float_at(rt, G[5] + 0x20u); ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[3]; ctx.fpr[2] = ctx.fpr[2] * ctx.fpr[4]; ctx.fpr[3] = float_at(rt, G[6] + 8u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[3]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put_float(rt, G[4] + 0x8u, ctx.fpr[1]); }
void func_08B52A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[17] = G[5]; call_here(rt, ctx, 0x08B54808u); G[6] = word(rt, G[16] + 4u); G[3] = word(rt, G[16] + 8u); G[2] = 2u; G[4] = G[6] ^ 3u; G[3] += 0x3Fu; G[11] = 4u; if (G[4] != 0u) G[11] = G[2]; G[3] &= ~0x3Fu; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[11])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[9] = word(rt, G[16] + 0x1Cu); G[5] = word(rt, G[16]); G[8] = word(rt, G[16] + 0x18u); G[4] = G[17]; G[11] = ctx.lo; G[7] = 0u; G[10] = 0u; call_here(rt, ctx, 0x08B5236Cu); G[4] = G[17]; G[5] = 5u; G[6] = 1u; call_here(rt, ctx, 0x08B52560u); G[4] = G[17]; G[5] = 6u; G[6] = 1u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B52560u); }
void func_08B61674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B61534u); const bool busy = G[2] != 0u; G[2] = 0u; if (busy) { G[2] = load8(G[16] + 4u); G[17] = 0u; if (G[2] != 0u) { G[18] = 4u; call_here(rt, ctx, 0x08B77D40u); for (;;) { put(rt, G[2], 0u); G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08B77E28u); G[17] = G[2]; if (G[2] == 0u) break; call_here(rt, ctx, 0x08B77D40u); G[3] = word(rt, G[2]); if (G[3] != G[18]) break; } } put(rt, G[16], 0u); G[2] = G[17] == 0u ? G[16] : 0u; } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B796E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 0x08CB0000u; G[3] = 0x08CB5E98u; G[18] = G[5]; G[5] = G[3]; G[17] = G[4]; G[4] = G[6]; G[16] = G[6]; if (G[6] != 0u) { call_here(rt, ctx, 0x08B35FD8u); G[5] = 0x08C96E5Cu; G[4] = G[16]; bool set = true; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35FD8u); G[3] = 0u; if (G[2] != 0u) set = false; } if (set) { put(rt, G[17] + 0x34u, G[16]); G[3] = 0x08CB5E98u; put(rt, G[17] + 0x30u, G[18]); } } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B7A0A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4] + 0x37Cu); call_here(rt, ctx, 0x08C7A888u); G[6] = G[2]; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[16]) >> 31); G[2] >>= 27; G[3] = G[16] + G[2]; G[3] &= 0x1Fu; G[3] -= G[2]; G[4] = 0x08D72D1Cu; G[3] <<= 2; G[5] = G[3] + G[4]; for (;;) { G[2] = word(rt, G[5] + 0x84u); if (G[16] == G[2]) { put(rt, G[5] + 0x84u, 0u); G[4] = G[6]; put(rt, G[5] + 4u, 0u); call_here(rt, ctx, 0x08C7A8A8u); break; } G[4] = word(rt, G[4]); G[5] = G[3] + G[4]; if (G[4] == 0u) { G[16] = 0xFFFFFFFFu; G[4] = G[6]; call_here(rt, ctx, 0x08C7A8A8u); break; } } G[2] = G[16]; } G[16] = s0; }
void func_08B7B804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B796C4u); G[5] = sext(load16(G[18] + 0xEu), 16); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08B807FCu); G[3] = G[2]; G[2] = 0xFFFFFFFFu; const bool fail = G[3] == G[2]; G[2] = load16(G[18] + 0xCu); if (fail) G[2] &= ~(1u << 12); else { put(rt, G[18] + 0x50u, G[3]); G[2] |= 0x1000u; } store16(G[18] + 0xCu, G[2]); G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B89F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[4]; G[4] = 0x18u; G[17] = G[5]; G[5] = 4u; G[18] = G[7]; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A80214u); G[2] = 0x08CD45C8u; G[17] = frame.at(4u); put(rt, G[16], G[2]); G[4] = G[17]; G[5] = frame.at(0u); put(rt, G[16] + 0x14u, G[18]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A5D414u); G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08A7C5C8u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B8A72C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; if (G[5] == G[4]) { G[2] = 1u; if (G[7] == G[6]) G[2] = 0u; return; } if (G[7] == G[6]) { G[2] = 0u; return; } G[4] = load16(G[4]); G[3] = load16(G[6]); G[2] = G[4] < G[3] ? 1u : 0u; if (G[2] != 0u) { G[2] = 1u; return; } G[2] = G[3] < G[4] ? 1u : 0u; G[8] += 2u; if (G[2] != 0u) { G[2] = 0u; return; } for (;;) { const bool end = G[5] == G[8]; G[6] += 2u; if (end) { G[2] = 1u; if (G[7] == G[6]) G[2] = 0u; return; } if (G[7] == G[6]) { G[2] = 0u; return; } G[3] = load16(G[8]); G[2] = load16(G[6]); G[4] = G[2] < G[3] ? 1u : 0u; G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[2] = 1u; return; } G[8] += 2u; if (G[4] != 0u) { G[2] = 0u; return; } } }
void func_08B92D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x20u); G[17] = G[5]; G[2] = G[17] - G[4]; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 3); G[2] = static_cast<std::int32_t>(G[2]) < 0x11 ? 1u : 0u; if (G[2] != 0u) { G[5] = G[17]; tail = true; } else { G[16] = G[4] + 0x80u; G[5] = G[16]; call_here(rt, ctx, 0x08B92C28u); G[5] = G[16]; while (G[17] != G[16]) { G[3] = word(rt, G[5] + 4u); G[2] = word(rt, G[5]); G[4] = G[16]; G[5] = frame.at(0u); G[16] += 8u; put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); call_here(rt, ctx, 0x08B92A3Cu); G[5] = G[16]; } } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B92C28u); }
void func_08BC1DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[6] = frame.at(1u); G[17] = G[4]; G[16] = word(rt, G[17] + 4u); G[4] = G[18]; if (G[16] != 0u) { G[5] = word(rt, G[17] + 8u); call_here(rt, ctx, 0x08B93CD8u); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08BC1CB4u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } put(rt, G[17] + 4u, 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08BD3088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x20u); G[21] = G[6]; G[20] = G[4]; G[16] = G[5]; G[17] = word(rt, G[7] + 4u); G[18] = load8(G[7] + 8u); G[19] = word(rt, G[7]); if (G[5] != G[6]) { G[7] = load16(G[16]); for (;;) { G[4] = word(rt, G[17]); G[16] += 2u; G[5] = G[19]; G[6] = G[18]; G[8] = G[17]; call_here(rt, ctx, 0x08AFA208u); if (G[21] == G[16]) break; G[7] = load16(G[16]); } } store8(G[20] + 8u, G[18]); G[2] = G[20]; put(rt, G[20], G[19]); put(rt, G[20] + 4u, G[17]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
void func_08BDB740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; G[5] = G[5] - G[4]; { GuestFrame frame(ctx, 0x20u); G[2] = static_cast<std::int32_t>(G[5]) < 0x10 ? 1u : 0u; G[19] = G[4]; if (G[2] == 0u) { G[20] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 3); G[2] = G[20] - 2u; G[3] = G[2] >> 31; G[3] += G[2]; G[18] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[2] = G[18] << 3; G[16] = G[4] + G[2]; G[17] = 0u; for (;;) { G[8] = word(rt, G[16]); G[9] = word(rt, G[16] + 4u); G[5] = G[18] - G[17]; G[4] = G[19]; G[6] = G[20]; call_here(rt, ctx, 0x08BDB668u); G[16] -= 8u; if (G[17] == G[18]) break; G[17] += 1u; } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08BDB9EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; G[6] = G[5] - G[4]; { GuestFrame frame(ctx, 0x20u); G[2] = static_cast<std::int32_t>(G[6]) < 0x10 ? 1u : 0u; G[19] = G[4]; if (G[2] == 0u) { G[2] = G[4] - G[5]; G[2] = G[6] + G[2]; G[16] = G[5]; G[18] = G[6] + G[2]; G[17] = G[6] - 8u; do { G[16] -= 8u; G[2] = word(rt, G[19]); G[3] = word(rt, G[19] + 4u); G[8] = word(rt, G[16]); G[9] = word(rt, G[16] + 4u); put(rt, G[16], G[2]); G[6] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[17]) >> 3); G[18] -= 8u; put(rt, G[16] + 4u, G[3]); G[4] = G[19]; G[5] = 0u; call_here(rt, ctx, 0x08BDB668u); G[2] = static_cast<std::int32_t>(G[18]) < 0x10 ? 1u : 0u; G[17] -= 8u; } while (G[2] == 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08BE654C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD0000u; if (G[5] == 0u) return; G[11] = 0x08CCDE00u; G[8] = 0u; G[10] = G[6] + 0x20u; G[9] = G[6] + 0x28u; G[7] = G[4]; do { G[8] += 1u; if (G[4] != 0u) { ctx.fpr[0] = float_at(rt, G[6] + 0xCu); ctx.fpr[1] = float_at(rt, G[6] + 0x10u); G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6] + 8u); ctx.fpr[2] = float_at(rt, G[6] + 0x14u); ctx.fpr[3] = float_at(rt, G[6] + 0x18u); ctx.fpr[4] = float_at(rt, G[6] + 0x1Cu); put(rt, G[7] + 4u, G[2]); put(rt, G[7] + 8u, G[3]); put_float(rt, G[7] + 0xCu, ctx.fpr[0]); put_float(rt, G[7] + 0x10u, ctx.fpr[1]); put_float(rt, G[7] + 0x14u, ctx.fpr[2]); put_float(rt, G[7] + 0x18u, ctx.fpr[3]); put_float(rt, G[7] + 0x1Cu, ctx.fpr[4]); put(rt, G[7], G[11]); ctx.fpr[0] = float_at(rt, G[6] + 0x20u); put_float(rt, G[7] + 0x20u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[10] + 4u); put_float(rt, G[7] + 0x24u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[6] + 0x28u); put_float(rt, G[7] + 0x28u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[9] + 4u); put_float(rt, G[7] + 0x2Cu, ctx.fpr[1]); } G[4] += 0x30u; G[7] += 0x30u; } while (G[5] != G[8]); }
void func_08C75E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = sext(G[5] & 0xFFu, 8); G[17] = G[4]; G[6] = word(rt, G[4]); G[3] = word(rt, G[6] - 0xCu); G[2] = word(rt, G[6] - 8u); G[16] = G[3] + 1u; G[2] = G[2] < G[16] ? 1u : 0u; G[5] = G[16]; bool grow = G[2] != 0u; if (!grow) { G[2] = word(rt, G[6] - 4u); grow = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; } if (grow) { call_here(rt, ctx, 0x08C75840u); G[6] = word(rt, G[17]); G[3] = word(rt, G[6] - 0xCu); G[2] = G[17]; } G[3] = G[6] + G[3]; store8(G[3], G[18]); G[3] = 0x08CE0000u; G[4] = word(rt, G[17]); put(rt, G[4] - 4u, 0u); G[5] = G[4] + G[16]; put(rt, G[4] - 0xCu, G[16]); G[4] = load8(0x08CDBE28u); store8(G[5], G[4]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C799C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5] + 4u; G[18] = G[5] + 8u; G[17] = G[4] + 4u; G[16] = G[4]; G[8] = word(rt, G[5]); G[3] = word(rt, G[5] + 4u); G[9] = word(rt, G[5] + 0xCu); G[6] = word(rt, G[8] - 0xCu); G[2] = word(rt, G[3] - 0xCu); G[7] = word(rt, G[5] + 8u); G[6] = G[4] + G[6]; G[2] = G[4] + G[2]; put(rt, G[2], G[7]); put(rt, G[4], G[8]); G[4] = G[17]; put(rt, G[6], G[9]); call_here(rt, ctx, 0x08C771D4u); G[2] = word(rt, G[16]); G[5] = G[17]; G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C54A7Cu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_202
} // namespace gt::game

#define HOT202(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_202::name, gt::ReturnKind::kind)
HOT202(0x08BEAF2Cu, func_08BEAF2C, Word);
HOT202(0x08BE65E0u, func_08BE65E0, Word);
HOT202(0x08B64DB0u, func_08B64DB0, Word);
HOT202(0x08B16824u, func_08B16824, Word);
HOT202(0x08A5DD38u, func_08A5DD38, Word);
HOT202(0x089F1550u, func_089F1550, Word);
// HOT202(0x089B9794u, func_089B9794, Word);  // no comparable synthetic case yet
HOT202(0x08877964u, func_08877964, Word);
// HOT202(0x0884BA94u, func_0884BA94, Word);  // no comparable synthetic case yet
HOT202(0x08C0F7E8u, func_08C0F7E8, Word);
HOT202(0x08C2DCF8u, func_08C2DCF8, Word);
HOT202(0x08BECEACu, func_08BECEAC, Word);
HOT202(0x08B64E74u, func_08B64E74, Word);
HOT202(0x08B16B1Cu, func_08B16B1C, Word);
HOT202(0x08A82FB0u, func_08A82FB0, Word);
HOT202(0x08B861C0u, func_08B861C0, Word);
// HOT202(0x08A024C8u, func_08A024C8, Word);  // no comparable synthetic case yet
HOT202(0x088781B4u, func_088781B4, Word);
// HOT202(0x0884BB28u, func_0884BB28, Word);  // no comparable synthetic case yet
HOT202(0x08817A68u, func_08817A68, Word);
HOT202(0x0881A7ACu, func_0881A7AC, Word);
// HOT202(0x0882FB48u, func_0882FB48, Word);  // no comparable synthetic case yet
// HOT202(0x088333C0u, func_088333C0, Word);  // no comparable synthetic case yet
// HOT202(0x088512FCu, func_088512FC, Word);  // no comparable synthetic case yet
HOT202(0x0883C758u, func_0883C758, Word);
HOT202(0x0885C2C8u, func_0885C2C8, Word);
// HOT202(0x0885D07Cu, func_0885D07C, Word);  // no comparable synthetic case yet
HOT202(0x08869254u, func_08869254, Word);
// HOT202(0x08869BB4u, func_08869BB4, Word);  // no comparable synthetic case yet
// HOT202(0x0886CB54u, func_0886CB54, Word);  // no comparable synthetic case yet
HOT202(0x08870218u, func_08870218, Word);
HOT202(0x08876D70u, func_08876D70, Word);
HOT202(0x08896D94u, func_08896D94, Word);
HOT202(0x088D6B6Cu, func_088D6B6C, Word);
HOT202(0x088D7658u, func_088D7658, Word);
HOT202(0x088DD984u, func_088DD984, Word);
// HOT202(0x088E59A0u, func_088E59A0, Word);  // no comparable synthetic case yet
HOT202(0x088E6864u, func_088E6864, Word);
HOT202(0x088E68F8u, func_088E68F8, Word);
HOT202(0x088F65E4u, func_088F65E4, Word);
HOT202(0x088FC630u, func_088FC630, Word);
HOT202(0x08904970u, func_08904970, Word);
HOT202(0x08909A5Cu, func_08909A5C, Word);
HOT202(0x0890F10Cu, func_0890F10C, Word);
HOT202(0x089386DCu, func_089386DC, Word);
HOT202(0x0893D0F4u, func_0893D0F4, Word);
// HOT202(0x089BA264u, func_089BA264, Word);  // no comparable synthetic case yet
HOT202(0x089BCF5Cu, func_089BCF5C, Word);
// HOT202(0x089BEE8Cu, func_089BEE8C, Word);  // no comparable synthetic case yet
HOT202(0x089CBAB4u, func_089CBAB4, Word);
HOT202(0x089DD4ECu, func_089DD4EC, Word);
HOT202(0x089DEF28u, func_089DEF28, Word);
HOT202(0x089DEFBCu, func_089DEFBC, Word);
HOT202(0x08A02A24u, func_08A02A24, Word);
HOT202(0x08A21EE4u, func_08A21EE4, Word);
HOT202(0x08A322A8u, func_08A322A8, Word);
HOT202(0x08A356E0u, func_08A356E0, Word);
HOT202(0x08A3D588u, func_08A3D588, Word);
HOT202(0x08A40B58u, func_08A40B58, Word);
HOT202(0x08A50104u, func_08A50104, Word);
HOT202(0x08A57548u, func_08A57548, Float);
HOT202(0x08A5D9D4u, func_08A5D9D4, DoubleWord);
HOT202(0x08A81F80u, func_08A81F80, Word);
// HOT202(0x08A997D0u, func_08A997D0, Word);  // no comparable synthetic case yet
HOT202(0x08AA0984u, func_08AA0984, Word);
// HOT202(0x08AAE994u, func_08AAE994, Word);  // no comparable synthetic case yet
// HOT202(0x08ABFDD0u, func_08ABFDD0, Word);  // no comparable synthetic case yet
HOT202(0x08AC1AB4u, func_08AC1AB4, Word);
HOT202(0x08AC1EACu, func_08AC1EAC, Word);
HOT202(0x08AC391Cu, func_08AC391C, Word);
HOT202(0x08AF8590u, func_08AF8590, Word);
HOT202(0x08B09D50u, func_08B09D50, Word);
HOT202(0x08B09FA0u, func_08B09FA0, Word);
HOT202(0x08B0B508u, func_08B0B508, Word);
HOT202(0x08B0B59Cu, func_08B0B59C, Word);
// HOT202(0x08B10DC4u, func_08B10DC4, Word);  // no comparable synthetic case yet
HOT202(0x08B12C14u, func_08B12C14, Word);
HOT202(0x08B17604u, func_08B17604, Word);
HOT202(0x08B17698u, func_08B17698, Word);
HOT202(0x08B1AC90u, func_08B1AC90, Word);
HOT202(0x08B2DC98u, func_08B2DC98, Word);
// HOT202(0x08B3433Cu, func_08B3433C, Word);  // no comparable synthetic case yet
HOT202(0x08B3ED84u, func_08B3ED84, Word);
HOT202(0x08B4135Cu, func_08B4135C, Word);
HOT202(0x08B439A4u, func_08B439A4, Word);
HOT202(0x08B4AB04u, func_08B4AB04, Word);
HOT202(0x08B52A64u, func_08B52A64, Word);
HOT202(0x08B61674u, func_08B61674, Word);
HOT202(0x08B796E8u, func_08B796E8, Word);
HOT202(0x08B7A0A8u, func_08B7A0A8, Word);
HOT202(0x08B7B804u, func_08B7B804, Word);
HOT202(0x08B89F70u, func_08B89F70, Word);
HOT202(0x08B8A72Cu, func_08B8A72C, Word);
HOT202(0x08B92D1Cu, func_08B92D1C, Word);
HOT202(0x08BC1DFCu, func_08BC1DFC, Word);
// HOT202(0x08BD3088u, func_08BD3088, Word);  // no comparable synthetic case yet
HOT202(0x08BDB740u, func_08BDB740, Word);
HOT202(0x08BDB9ECu, func_08BDB9EC, Word);
HOT202(0x08BE654Cu, func_08BE654C, Word);
HOT202(0x08C75E68u, func_08C75E68, Word);
// HOT202(0x08C799C4u, func_08C799C4, Word);  // no comparable synthetic case yet
