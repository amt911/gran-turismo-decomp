// Hot batch 203: thirty-eight-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_203 {
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

// The scaled twin of a bound member call: the first {function +0x14, adjustment +0x18} pair is called on a1 (+ the adjustment; an
// odd one marks a virtual slot) with f12 untouched, its result is the object the second pair (+0x24 / +0x28) is resolved on and
// that one is tail called with (this', result) and f12 = the original scale.
void call_member_scaled_then_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 base = ctx.gpr[5];
    const float scale = ctx.fpr[12];
    u32 target = 0u;
    u32 second_this = 0u;
    u32 result = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        const u32 adjust = word(rt, self + 0x18u);
        const u32 adjusted = base + static_cast<u32>(static_cast<s32>(adjust) >> 1);
        const u32 function = word(rt, self + 0x14u);
        u32 callee = function;
        if ((adjust & 1u) != 0u) callee = word(rt, word(rt, adjusted) + function);
        result = call_virtual(rt, ctx, callee, {adjusted, base, function});
        const u32 second_adjust = word(rt, self + 0x28u);
        const u32 shifted = static_cast<u32>(static_cast<s32>(second_adjust) >> 1);
        second_this = result + shifted;
        if ((second_adjust & 1u) != 0u) {
            const u32 second_function = word(rt, self + 0x24u);
            ctx.gpr[3] = second_function;
            ctx.gpr[2] = word(rt, second_this) + second_function;
            target = word(rt, ctx.gpr[2]);
        } else {
            target = word(rt, self + 0x24u);
            ctx.gpr[3] = shifted;
            ctx.gpr[2] = 0u;
        }
        ctx.gpr[25] = target;
        ctx.fpr[12] = scale;
    }
    tail_call(ctx, target, {second_this, result});
}
// Finds the record of 0xA4 bytes (count at +0x1480) whose name (the record's first bytes) equals a1 (0x08B35FD8 answers 0 for
// equal) and answers the address of its member at `field`; 0 when none matches.
void find_record_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 field) {
    const u32 table = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, table + 0x1480u));
    ctx.gpr[2] = 0u;
    // (the original also tests the index < 0, which cannot happen: it counts up from 0 and stops at `count`.)
    for (s32 index = 0; index < count; ++index) {
        const u32 offset = static_cast<u32>(index) * 0xA4u;
        if (call_args(rt, ctx, 0x08B35FD8u, {table + offset, key}) == 0u) {
            ctx.gpr[2] = table + offset + field;
            return;
        }
        ctx.gpr[2] = 0u;
    }
}
// For each of the object's entries (virtual +0x1C8 gives the count, asked again every round, so it runs `count` rounds) the
// 0x60-byte record at (*(self + 0xA8) + index * 0x60 + `member`) is assigned from a1 (0x089641C4, plus the word at +0x10)
// unless it is a1 itself.
void assign_records(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 member) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    u32 offset = 0u;
    s32 index = 0;
    for (;;) {
        const s32 count = static_cast<s32>(call_virtual(rt, ctx, word(rt, word(rt, self) + 0x1C8u), {self}));
        const bool more = index < count;
        ++index;
        ctx.gpr[2] = more ? 1u : 0u;
        if (!more) break;
        const u32 slot = word(rt, self + 0xA8u) + offset + member;
        ctx.gpr[4] = slot;
        ctx.gpr[5] = source;
        offset += 0x60u;
        if (source != slot) {
            call_args(rt, ctx, 0x089641C4u, {slot, source});
            put(rt, slot + 0x10u, word(rt, source + 0x10u));
        }
    }
}
// `a2 > 0`: 0x088435A8(frame +0, a1) fills a scratch handle, 0x08843404(handle) finds a record, and the virtual `slot` of the object
// at *a3 (a null one is reported by 0x08A5BB7C, as the handle is) is stored in it at `offset` (a word, or a byte); scratch released.
void store_answer_in_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot, u32 offset, bool as_byte) {
    const u32 count = ctx.gpr[6];
    const u32 list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = frame.at(0u);
    if (static_cast<s32>(count) <= 0) return;
    call_args(rt, ctx, 0x088435A8u, {frame.at(0u)});
    const u32 handle = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08C9C7CCu;
    if (handle == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    const u32 record = call_args(rt, ctx, 0x08843404u, {handle});
    const u32 object = word(rt, list);
    ctx.gpr[4] = 0x08C9C7C4u;
    if (object == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    const u32 answer = call_virtual(rt, ctx, word(rt, word(rt, object) + slot), {object});
    if (as_byte) rt.memory().store8(record + offset, static_cast<std::uint8_t>(answer));
    else put(rt, record + offset, answer);
    call_args(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
// Copies the scratch string `temp` into `destination` (0x08A5BAD8) unless it is the same one, then releases it (0x08A5B9EC).
void assign_string_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 destination, u32 temp) {
    ctx.gpr[4] = destination;
    if (destination != temp) call_args(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temp)});
    call_args(rt, ctx, 0x08A5B9ECu, {temp});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08841D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0x10u); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B8C4A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_scaled_then_tail(rt, ctx);
}
void func_089C7090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = G[4]; G[4] = G[19]; G[17] = frame.at(4u); call_here(rt, ctx, 0x089C5850u); G[4] = 0x08CA63A0u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x089C4FD4u); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08977AE4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C70314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[3] = word(rt, G[5]); store8(G[4], 0u); G[2] = word(rt, G[3] - 0xCu); put(rt, G[17] + 4u, G[5]); G[2] = G[5] + G[2]; G[4] = word(rt, G[2] + 0x70u); G[16] = G[5]; if (G[4] != 0u) { G[2] = word(rt, G[2] + 0x14u); if (G[2] == 0u) { call_here(rt, ctx, 0x08C70238u); G[3] = word(rt, G[16]); } } G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] + G[2]; G[3] = word(rt, G[2] + 0x14u); G[4] = G[2]; G[5] = G[3] | 4u; if (G[3] != 0u) tail = true; else { G[2] = 1u; store8(G[17], G[2]); } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08C54CFCu); }
void func_08C5DCF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[7] = G[7] != 0u ? 1u : 0u; G[17] = G[4]; G[2] = 0x08CDB7F0u; G[20] = G[5]; G[19] = G[6]; put(rt, G[4] + 4u, G[7]); G[4] = G[6]; put(rt, G[17], G[2]); put(rt, G[17] + 8u, 0u); call_here(rt, ctx, 0x08B35F60u); G[18] = G[2] + 1u; G[4] = G[18]; call_here(rt, ctx, 0x08B6461Cu); G[16] = G[2]; G[4] = G[2]; G[5] = G[19]; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); put(rt, G[17] + 0x10u, G[16]); G[5] = G[20]; G[4] = G[17]; call_here(rt, ctx, 0x08B610C0u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08BDDAD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = G[4] + 4u; G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[8] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[4] = G[16]; if (static_cast<std::int32_t>(G[8]) > 0) { G[7] = 0u; do { G[2] = load8(G[6]); G[7] += 1u; store8(G[4], G[2]); G[3] = load8(G[6] + 1u); G[6] += 2u; store8(G[4] + 1u, G[3]); G[4] += 2u; } while (G[8] != G[7]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8CF98u); G[3] = word(rt, G[17] + 4u); G[2] = G[18] - G[16]; G[2] &= ~1u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB7EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    find_record_member(rt, ctx, 0x80u);
}
void func_08841D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0x14u); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08841E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0x68u); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08841F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0xD8u); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088420B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0x64u); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08842908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[5] = word(rt, G[2] + 0x6Cu); G[4] = frame.at(0u); G[5] = __builtin_bswap32(G[5]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08BFFE28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; G[3] = word(rt, G[4] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[5] + G[2]; G[2] = G[3] & 1u; G[6] = word(rt, G[16] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[2] = word(rt, G[2]); } else G[2] = G[6]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x28u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x24u); G[2] = word(rt, G[4]); G[2] += G[3]; G[25] = word(rt, G[2]); } else { G[25] = word(rt, G[16] + 0x24u); G[4] = G[3] + G[5]; } ctx.fpr[12] = ctx.fpr[20]; target = G[25]; ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; jump(ctx, target); }
void func_08C0B5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; G[3] = word(rt, G[4] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[5] + G[2]; G[2] = G[3] & 1u; G[6] = word(rt, G[16] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[2] = word(rt, G[2]); } else G[2] = G[6]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x28u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x24u); G[2] = word(rt, G[4]); G[2] += G[3]; G[25] = word(rt, G[2]); } else { G[25] = word(rt, G[16] + 0x24u); G[4] = G[3] + G[5]; } ctx.fpr[12] = ctx.fpr[20]; target = G[25]; ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; jump(ctx, target); }
void func_08C1015C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_scaled_then_tail(rt, ctx);
}
void func_08A0255C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = G[4]; G[4] = G[19]; G[17] = frame.at(4u); call_here(rt, ctx, 0x08A01338u); G[4] = 0x08CA8B14u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A00B0Cu); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A025F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = G[4]; G[4] = G[19]; G[17] = frame.at(4u); call_here(rt, ctx, 0x08A01338u); G[4] = 0x08CA8B14u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A0095Cu); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C703AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[3] = word(rt, G[5]); store8(G[4], 0u); G[2] = word(rt, G[3] - 0xCu); put(rt, G[17] + 4u, G[5]); G[2] = G[5] + G[2]; G[4] = word(rt, G[2] + 0x70u); G[16] = G[5]; if (G[4] != 0u) { G[2] = word(rt, G[2] + 0x14u); if (G[2] == 0u) { call_here(rt, ctx, 0x08C70238u); G[3] = word(rt, G[16]); } } G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] + G[2]; G[3] = word(rt, G[2] + 0x14u); G[4] = G[2]; G[5] = G[3] | 4u; if (G[3] != 0u) tail = true; else { G[2] = 1u; store8(G[17], G[2]); } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08C54CFCu); }
void func_08C5DDD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[7] = G[7] != 0u ? 1u : 0u; G[17] = G[4]; G[2] = 0x08CDB7F0u; G[20] = G[5]; G[19] = G[6]; put(rt, G[4] + 4u, G[7]); G[4] = G[6]; put(rt, G[17], G[2]); put(rt, G[17] + 8u, 0u); call_here(rt, ctx, 0x08B35F60u); G[18] = G[2] + 1u; G[4] = G[18]; call_here(rt, ctx, 0x08B6461Cu); G[16] = G[2]; G[4] = G[2]; G[5] = G[19]; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); put(rt, G[17] + 0x10u, G[16]); G[5] = G[20]; G[4] = G[17]; call_here(rt, ctx, 0x08B610C0u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08BDDBA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = G[4] + 4u; G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[8] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[4] = G[16]; if (static_cast<std::int32_t>(G[8]) > 0) { G[7] = 0u; do { G[2] = load8(G[6]); G[7] += 1u; store8(G[4], G[2]); G[3] = load8(G[6] + 1u); G[6] += 2u; store8(G[4] + 1u, G[3]); G[4] += 2u; } while (G[8] != G[7]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8CFE0u); G[3] = word(rt, G[17] + 4u); G[2] = G[18] - G[16]; G[2] &= ~1u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AB7F5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    find_record_member(rt, ctx, 0xA0u);
}
void func_08843708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_answer_in_record(rt, ctx, 0x30u, 0x81u, true);
}
void func_088435E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_answer_in_record(rt, ctx, 0x3Cu, 0x84u, false);
}
void func_08A05490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_records(rt, ctx, 0x20u);
}
void func_089CD7EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14Cu); ctx.fpr[2] = ctx.fpr[13]; ctx.fpr[1] = std::fabs(ctx.fpr[14]); G[2] &= 0x20u; ctx.fpr[13] = ctx.fpr[12] - ctx.fpr[13]; bool slow = false; if (G[2] != 0u) { ctx.fpr[0] = std::fabs(ctx.fpr[13]); const bool big = ctx.fpr[1] < ctx.fpr[0]; ctx.fpr[0] = 0.0f; if (!big) slow = true; } if (slow) { const bool up = ctx.fpr[0] < ctx.fpr[13]; ctx.fpr[15] = ctx.fpr[1] * ctx.fpr[15]; if (up) ctx.fpr[13] = ctx.fpr[2] + ctx.fpr[15]; else ctx.fpr[13] = ctx.fpr[2] - ctx.fpr[15]; ctx.fpr[0] = ctx.fpr[12] - ctx.fpr[13]; ctx.fpr[0] = std::fabs(ctx.fpr[0]); if (ctx.fpr[0] <= ctx.fpr[15]) ctx.fpr[13] = ctx.fpr[12]; ctx.fpr[0] = ctx.fpr[13]; return; } G[1] = 0x3C23D70Au; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[13] = ctx.fpr[13] * ctx.fpr[15]; ctx.fpr[1] = std::fabs(ctx.fpr[13]); if (ctx.fpr[1] < ctx.fpr[0]) ctx.fpr[13] = ctx.fpr[12]; else ctx.fpr[13] = ctx.fpr[2] + ctx.fpr[13]; ctx.fpr[0] = ctx.fpr[13]; }
void func_0882F708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EBD0u); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0880D148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = G[5]; G[6] = frame.at(1u); G[16] = frame.at(4u); G[5] = sext(load8(G[5]), 8); G[17] = word(rt, G[4] + 4u); G[4] = G[16]; G[5] ^= 0x2Fu; G[5] = G[5] < 1u ? 1u : 0u; G[5] = G[2] + G[5]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x089A33F0u); G[3] = word(rt, frame.at(4u)); G[17] = G[2]; G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A05640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_records(rt, ctx, 0x40u);
}
void func_089E5B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x150u); ctx.fpr[2] = ctx.fpr[13]; ctx.fpr[1] = std::fabs(ctx.fpr[14]); G[2] &= 0x10u; ctx.fpr[13] = ctx.fpr[12] - ctx.fpr[13]; bool slow = false; if (G[2] != 0u) { ctx.fpr[0] = std::fabs(ctx.fpr[13]); const bool big = ctx.fpr[1] < ctx.fpr[0]; ctx.fpr[0] = 0.0f; if (!big) slow = true; } if (slow) { const bool up = ctx.fpr[0] < ctx.fpr[13]; ctx.fpr[15] = ctx.fpr[1] * ctx.fpr[15]; if (up) ctx.fpr[13] = ctx.fpr[2] + ctx.fpr[15]; else ctx.fpr[13] = ctx.fpr[2] - ctx.fpr[15]; ctx.fpr[0] = ctx.fpr[12] - ctx.fpr[13]; ctx.fpr[0] = std::fabs(ctx.fpr[0]); if (ctx.fpr[0] <= ctx.fpr[15]) ctx.fpr[13] = ctx.fpr[12]; ctx.fpr[0] = ctx.fpr[13]; return; } G[1] = 0x3C23D70Au; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[13] = ctx.fpr[13] * ctx.fpr[15]; ctx.fpr[1] = std::fabs(ctx.fpr[13]); if (ctx.fpr[1] < ctx.fpr[0]) ctx.fpr[13] = ctx.fpr[12]; else ctx.fpr[13] = ctx.fpr[2] + ctx.fpr[13]; ctx.fpr[0] = ctx.fpr[13]; }
void func_0883916C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08838FBCu); G[4] = 0x08C9BBF8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08838DC4u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08931970u); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0880D1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = G[5]; G[6] = frame.at(1u); G[16] = frame.at(4u); G[5] = sext(load8(G[5]), 8); G[17] = word(rt, G[4] + 4u); G[4] = G[16]; G[5] ^= 0x2Fu; G[5] = G[5] < 1u ? 1u : 0u; G[5] = G[2] + G[5]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x089A32C4u); G[3] = word(rt, frame.at(4u)); G[17] = G[2]; G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_0880B938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; G[2] = 0x08D00000u; G[3] = word(rt, 0x08CFFAACu); { GuestFrame frame(ctx, 0x20u); G[1] = 0x3C888889u; ctx.fpr[2] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); G[1] = 0x3D4CCCCEu; ctx.fpr[20] = std::bit_cast<float>(G[1]); ctx.fpr[0] = float_at(rt, G[3] + 0xF0u); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); G[1] = 0x358637BDu; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[0]; G[16] = G[4]; if (ctx.fpr[1] < ctx.fpr[2]) ctx.fpr[20] = ctx.fpr[2]; else { const bool lt = ctx.fpr[1] < ctx.fpr[20]; ctx.fpr[2] = ctx.fpr[1]; if (lt) ctx.fpr[20] = ctx.fpr[2]; } call_here(rt, ctx, 0x08B344E0u); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08869444u); G[4] = G[16]; ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; jump(ctx, 0x08B3457Cu); }
void func_08824DA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); G[17] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08C99B4Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); G[18] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); G[4] = 0x08C99B24u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x0885BE34u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08833CAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(4u); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088DE9CCu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088437A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_answer_in_record(rt, ctx, 0x30u, 0x80u, true);
}
void func_08843838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_answer_in_record(rt, ctx, 0x3Cu, 0x7Cu, false);
}
void func_08864AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[5] = G[6]; G[17] = G[4]; G[4] = G[18]; G[16] = G[6]; call_here(rt, ctx, 0x08891124u); G[4] = G[17]; G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08864A74u); G[4] = word(rt, G[17]); G[5] = G[16]; G[4] += 0x3F80u; call_here(rt, ctx, 0x08891C54u); G[4] = word(rt, G[17]); G[5] = G[18]; G[6] = G[16]; G[4] += 0x3F80u; call_here(rt, ctx, 0x08891798u); G[3] = 1u; if (G[2] == 0u) { G[4] = word(rt, G[17]); G[4] += 0x3F80u; call_here(rt, ctx, 0x088916C0u); G[3] = 0u; } } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0886AB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = 3u; G[17] = 0u; G[16] = G[4]; store8(G[4] + 0x20u, 0u); G[2] = word(rt, G[16]); for (;;) { G[5] = G[17]; G[4] = G[16]; G[3] = word(rt, G[2] + 8u); G[17] += 1u; G[6] = 0u; call_here(rt, ctx, G[3]); if (G[17] == G[18]) break; G[2] = word(rt, G[16]); } G[4] = word(rt, G[16] + 0x1Cu); G[17] = G[16] + 4u; if (G[4] != 0u) { call_here(rt, ctx, 0x0886B2F4u); G[4] = word(rt, G[16] + 0x1Cu); if (G[16] == G[2]) { G[5] = 0u; call_here(rt, ctx, 0x0886B2E8u); G[4] = word(rt, G[16] + 0x1Cu); G[17] = G[16] + 4u; } G[5] = G[17]; call_here(rt, ctx, 0x0886BD00u); } G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x0886B5A4u); }
void func_08877CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08875584u); G[4] = word(rt, G[17]); G[16] = G[2]; call_here(rt, ctx, 0x0886AFA8u); G[4] = G[2] + 4u; G[5] = G[16]; call_here(rt, ctx, 0x0887CB3Cu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } } G[16] = s0; G[17] = s1; }
void func_088CF414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[4] + 0x14u); ctx.fpr[1] = 0.0f; if (!(ctx.fpr[12] <= ctx.fpr[2])) { ctx.fpr[3] = float_at(rt, G[4] + 0x24u); if (!(ctx.fpr[3] <= ctx.fpr[12])) { ctx.fpr[0] = float_at(rt, G[4] + 0x20u); G[1] = 0x3F800000u; ctx.fpr[1] = std::bit_cast<float>(G[1]); const bool c = ctx.fpr[0] < ctx.fpr[12]; ctx.fpr[4] = ctx.fpr[3] - ctx.fpr[0]; ctx.fpr[3] = ctx.fpr[12] - ctx.fpr[0]; if (c) { G[1] = 0xBF800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[4]; ctx.fpr[0] = ctx.fpr[3] * ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[0] + ctx.fpr[1]; } else { ctx.fpr[0] = float_at(rt, G[4] + 0x1Cu); ctx.fpr[3] = ctx.fpr[12] - ctx.fpr[2]; const bool c2 = ctx.fpr[0] <= ctx.fpr[12]; ctx.fpr[2] = ctx.fpr[0] - ctx.fpr[2]; if (!c2) { ctx.fpr[1] = float_at(rt, G[4] + 0x18u); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[3] * ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; } } } } ctx.fpr[0] = ctx.fpr[1]; }
void func_088DD7D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 0u; G[18] = 0u; G[17] = G[4]; for (;;) { G[4] = G[17]; call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = G[18]; G[4] = G[17] + 4u; if (G[2] == 0u) break; call_here(rt, ctx, 0x088D636Cu); G[4] = word(rt, G[17] + 0xC0u); G[16] = G[2]; G[4] += G[19]; call_here(rt, ctx, 0x088E85E8u); G[7] = G[2]; G[5] = G[18]; G[4] = G[17]; G[6] = G[16]; G[8] = 0u; call_here(rt, ctx, 0x088DD144u); G[18] += 1u; G[19] += 0x2F8u; } G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x088E96F4u); }
void func_088E5234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x20u); G[20] = G[4]; G[19] = G[6]; G[18] = G[5]; G[17] = G[9]; G[16] = G[8]; call_here(rt, ctx, 0x088E511Cu); G[5] = G[18]; G[6] = G[16]; G[7] = G[17]; G[9] = G[19]; G[8] = 1u; if (G[2] != 0u) { G[4] = word(rt, G[20]); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x7Cu); target = G[25]; } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; if (target != 0u) jump(ctx, target); }
void func_088E6548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 1u; G[18] = 4u; G[17] = G[4]; G[16] = 0u; G[4] = word(rt, G[4]); G[4] += 0xECu; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17]); for (;;) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[2] = word(rt, G[17]); G[2] = G[16] + G[2]; store8(G[2] + 0x4ACu, G[19]); } G[16] += 1u; if (G[16] == G[18]) break; G[4] = word(rt, G[17]); } G[2] = word(rt, G[17]); G[3] = 1u; store8(G[2] + 0x52Bu, G[3]); G[4] = word(rt, G[17]); G[4] += 0xECu; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_088EE154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4] + 0xC4u; G[16] = G[4]; G[2] = word(rt, G[16] + 0x11Cu); G[4] = G[17]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B344E0u); G[6] = word(rt, G[16] + 0x11Cu); G[2] = load8(G[6] + 0x1Au); G[4] = G[17]; if (G[2] == 0u) { G[2] = load8(G[6] + 0x19u); G[5] = load8(G[6] + 0x18u); G[3] = G[2] << 1; G[2] += 1u; G[2] &= 3u; G[3] += G[6]; if (G[2] != G[5]) { store16(G[3] + 0x10u, G[18]); store8(G[6] + 0x19u, G[2]); } } tail = true; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08B3457Cu); }
void func_088F1000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x80u); G[18] = G[4]; G[4] = G[5]; G[17] = G[6]; G[16] = G[5]; put_float(rt, frame.at(0x70u), ctx.fpr[20]); G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0x1Cu); ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, G[2]); G[3] = 0x44u; put(rt, frame.at(8u), G[17]); G[4] = G[2]; G[5] = G[16]; put(rt, frame.at(0xCu), G[18]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); put(rt, frame.at(0u), G[3]); put(rt, frame.at(4u), 0u); put(rt, frame.at(0x14u), 0u); G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 8u); G[6] = frame.at(0u); call_here(rt, ctx, G[2]); ctx.fpr[12] = float_at(rt, frame.at(0x34u)); ctx.fpr[13] = float_at(rt, frame.at(0x38u)); ctx.fpr[12] = -ctx.fpr[12]; call_here(rt, ctx, 0x08B4A430u); ctx.fpr[20] = float_at(rt, frame.at(0x70u)); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088FC6FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x28u); G[16] = word(rt, G[2] + 4u); put(rt, G[2], 0u); put(rt, G[2] + 4u, 0u); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); target = G[25]; } } } } G[16] = s0; if (target != 0u) jump(ctx, target); }
void func_089107A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B664A0u); ctx.fpr[1] = ctx.fpr[0]; G[1] = 0xC3000000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[5] = 0x7Fu; bool conv = false; if (ctx.fpr[1] < ctx.fpr[0]) { ctx.fpr[1] = ctx.fpr[0]; conv = true; } else { G[1] = 0x42FE0000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); conv = ctx.fpr[1] < ctx.fpr[0]; G[2] = G[16] << 5; } if (conv) { G[2] = trunc_w_s(ctx.fpr[1]); ctx.fpr[1] = std::bit_cast<float>(G[2]); G[5] = sext(G[2] & 0xFFu, 8); G[2] = G[16] << 5; } G[4] = G[16] << 3; G[3] = 0x08CE0000u; G[2] -= G[4]; G[3] = 0x08CDD81Cu; G[2] += G[3]; G[2] += G[17]; store8(G[2], G[5]); } G[16] = s0; G[17] = s1; }
void func_089341A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[2] = 0xFFFFFFFFu; G[18] = G[4]; G[3] = 1u; G[16] = word(rt, G[4] + 0xF0u); G[17] = word(rt, G[4] + 0xF4u); if (G[16] != G[2] || G[17] != G[2]) { call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x089368DCu); G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; G[8] = frame.at(0u); call_here(rt, ctx, 0x08936584u); G[2] = load8(frame.at(5u)); G[3] = 1u; store8(G[18] + 0x15Du, G[2]); } } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08938770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x089386DCu); G[6] = G[2]; G[2] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; if (G[6] != G[2]) { G[7] = 1u; for (;;) { G[5] = G[6] + 1u; G[3] = G[5] << 3; bool have = false; if (static_cast<std::int32_t>(G[5]) >= 0) { G[4] = word(rt, G[16] + 4u); G[2] = word(rt, G[4] + 8u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] += G[4]; if (G[2] != 0u) { G[3] = word(rt, G[3] + 0x10u); have = true; } } if (!have) G[3] = 0xFFFFFFFFu; if (G[3] != G[17]) break; G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[6] = G[5]; if (G[2] == 0u) break; G[7] += 1u; } } G[2] = G[7]; } G[16] = s0; G[17] = s1; }
void func_08939920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[4]; G[2] = word(rt, G[4] + 4u); G[17] = word(rt, G[2] + 0x14u); G[19] = G[5]; G[2] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[17]) > 0) { G[18] = 0xFFFFFFFFu; G[2] = G[18] + G[17]; for (;;) { G[16] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[4] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x089398DCu); G[3] = G[2] < G[19] ? 1u : 0u; if (G[2] == G[19]) { G[2] = G[16]; break; } if (G[3] == 0u) G[17] = G[16]; else G[18] = G[16]; G[2] = G[18] + 1u; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; const bool more = G[2] != 0u; G[2] = G[18] + G[17]; if (!more) { G[2] = 0xFFFFFFFFu; break; } } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_089399B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x20u); G[21] = G[4]; G[20] = G[5]; G[18] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08939920u); G[3] = G[2]; G[2] = 0xFFFFFFFFu; G[17] = 0u; if (G[3] != G[2]) { G[18] = G[3]; G[19] = G[3] + 1u; for (;;) { G[16] = G[18] - 1u; G[4] = G[21]; G[5] = G[16]; call_here(rt, ctx, 0x089398DCu); G[17] += 1u; if (G[20] != G[2]) break; if (G[19] == G[17]) break; G[18] = G[16]; } } G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
void func_0893A414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[16] = G[5]; G[19] = G[6]; call_here(rt, ctx, 0x08939F4Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x0893A174u); G[16] = G[2]; G[5] = G[2]; G[2] = 0xFFFFFFFFu; G[4] = G[17]; G[18] = 0u; if (G[16] != G[2]) { G[18] = 1u; call_here(rt, ctx, 0x0893A3DCu); G[6] = word(rt, G[17] + 4u); G[4] = G[19]; G[5] = G[2]; call_here(rt, ctx, 0x08B79A88u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x0893A13Cu); } G[4] = G[17]; call_here(rt, ctx, 0x08939F54u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08940290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = word(rt, G[4] + 8u); G[2] = load8(G[5] + 8u); store8(G[16] + 8u, G[2]); G[4] = word(rt, G[4]); G[18] = sext(load8(G[5] + 0xAu), 8); call_here(rt, ctx, 0x0894D050u); G[3] = 0u; if (G[2] == 0u) { G[2] = 0xFFFFFFFFu; G[3] = G[18] & 0xFFu; if (G[18] == G[2]) G[3] = load8(G[16] + 8u); } store8(G[16] + 9u, G[3]); G[2] = 1u; G[5] = sext(load8(G[17] + 5u), 8); if (G[5] == G[2]) { G[2] = 3u; store8(G[16] + 6u, G[5]); } else { G[2] = 3u; if (G[5] == G[2]) store8(G[16] + 6u, G[5]); else if (G[5] != 0u) { G[2] = 2u; store8(G[16] + 6u, G[2]); } else { G[2] = 2u; store8(G[16] + 6u, 0u); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08949B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDE508u; G[4] += G[2]; G[3] = load8(G[4] + 0x102u); if (G[3] != 0u) { G[2] = load8(G[4] + 0x104u); G[4] = load8(G[4] + 0x106u); ctx.fpr[0] = 0.0f; ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[3] = std::bit_cast<float>(G[4]); const bool neg = ctx.fpr[12] < ctx.fpr[0]; ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[2])); G[1] = 0x3C23D70Au; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[4] = static_cast<float>(static_cast<std::int32_t>(G[4])); G[1] = 0x3E8E38E4u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[2] = ctx.fpr[2] * ctx.fpr[0]; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[4] = ctx.fpr[4] * ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[2]; if (neg) ctx.fpr[12] = -ctx.fpr[12]; const bool in = ctx.fpr[12] < ctx.fpr[4]; ctx.fpr[0] = ctx.fpr[12] * ctx.fpr[0]; if (in) { ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[4]; ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[0]; return; } } G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); }
void func_0895BC94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0x08CF0000u; G[2] = load8(0x08CF35ECu); G[1] = 0x3C23D70Au; ctx.fpr[6] = std::bit_cast<float>(G[1]); if (G[2] != 0u) { G[2] = 0x08CE0000u; return; } G[2] = load8(0x08CDE505u); G[3] = load8(0x08CDE504u); ctx.fpr[2] = std::bit_cast<float>(G[2]); ctx.fpr[3] = std::bit_cast<float>(G[3]); G[2] = load8(0x08CDE4F9u); G[3] = load8(0x08CDE4F8u); ctx.fpr[5] = std::bit_cast<float>(G[2]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[2]))); ctx.fpr[4] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[3]))); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[3] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[6]; ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[6]; ctx.fpr[2] = ctx.fpr[2] * ctx.fpr[6]; ctx.fpr[3] = ctx.fpr[3] * ctx.fpr[6]; put_float(rt, 0x08CDE500u, ctx.fpr[0]); put_float(rt, 0x08CDE4FCu, ctx.fpr[1]); put_float(rt, 0x08CDE4F4u, ctx.fpr[2]); put_float(rt, 0x08CDE4F0u, ctx.fpr[3]); G[2] = 1u; G[3] = 0x08CE0000u; store8(0x08CF35ECu, G[2]); }
void func_0896444C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[18] = G[4]; G[17] = word(rt, 0x08CFFAACu); G[2] = 0x08D00000u; G[19] = word(rt, G[17] + 0x18u); if (static_cast<std::int32_t>(G[5]) > 0) { G[16] = word(rt, G[6]); G[4] = 0x08CA230Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x18u, G[2]); } G[4] = frame.at(0u); G[5] = G[19]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_089644E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); if (G[5] == 0u) { G[2] = 0x08CF0000u; G[5] = load8(0x08CF38F0u); G[5] = G[5] != 0u ? 1u : 0u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[5] == G[2]) { G[16] = word(rt, G[6]); G[4] = 0x08CA230Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = G[2] != 0u ? 1u : 0u; G[3] = 0x08CF0000u; store8(0x08CF38F0u, G[2]); } } } G[16] = s0; }
void func_0896A860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x08966174u); G[4] = G[17]; G[7] = G[16]; G[8] = G[18]; G[5] = 0u; G[6] = 0u; if (G[2] != 0u) { if (G[16] != 0u) { G[2] = word(rt, G[17] + 0x80u); ctx.fpr[0] = float_at(rt, G[2]); put_float(rt, G[16], ctx.fpr[0]); } if (G[18] != 0u) { G[2] = word(rt, G[17] + 0x80u); ctx.fpr[0] = float_at(rt, G[2] + 4u); put_float(rt, G[18], ctx.fpr[0]); } G[16] = s0; G[17] = s1; G[18] = s2; return; } } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x0896A35Cu); }
void func_0899AC9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static constexpr std::uint32_t off[4] = {0x120u, 0x122u, 0x124u, 0x126u}; const std::uint32_t v[4] = {G[5], G[6], G[7], G[8]}; for (int i = 0; i < 4; ++i) { if (static_cast<std::int32_t>(v[i]) < 0) continue; G[2] = sext(load16(G[4] + off[i]), 16); if (v[i] == G[2]) continue; G[2] = word(rt, G[4] + 0xE4u); store16(G[4] + off[i], v[i]); G[2] |= 8u; put(rt, G[4] + 0xE4u, G[2]); } }
void func_089B30F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[5]; G[16] = G[4]; G[4] = frame.at(0u); ctx.fpr[3] = float_at(rt, G[6] + 4u); ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[1] = float_at(rt, G[6] + 8u); ctx.fpr[0] = float_at(rt, G[6] + 0xCu); G[6] = frame.at(0u); ctx.fpr[1] = ctx.fpr[2] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[3] + ctx.fpr[0]; put_float(rt, frame.at(0u), ctx.fpr[2]); put_float(rt, frame.at(8u), ctx.fpr[1]); put_float(rt, frame.at(0xCu), ctx.fpr[0]); put_float(rt, frame.at(4u), ctx.fpr[3]); call_here(rt, ctx, 0x08B4AD64u); G[4] = frame.at(8u); G[5] = G[17]; G[6] = G[4]; call_here(rt, ctx, 0x08B4AD64u); ctx.fpr[2] = float_at(rt, frame.at(0u)); ctx.fpr[3] = float_at(rt, frame.at(4u)); ctx.fpr[1] = float_at(rt, frame.at(8u)); ctx.fpr[0] = float_at(rt, frame.at(0xCu)); put_float(rt, G[16], ctx.fpr[2]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[3]; put_float(rt, G[16] + 4u, ctx.fpr[3]); put_float(rt, G[16] + 0xCu, ctx.fpr[0]); put_float(rt, G[16] + 8u, ctx.fpr[1]); } G[16] = s0; G[17] = s1; }
void func_089C83A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); G[4] = G[16] + 0x18u; if (G[16] != 0u) { G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C00FECu); G[5] = word(rt, G[16] + 8u); G[4] = G[16]; call_here(rt, ctx, 0x08C012FCu); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); } G[2] = word(rt, G[17] + 0x30u); G[4] = G[17] + 0x30u; if (G[2] == G[4]) { put(rt, G[17] + 0x2Cu, 0u); G[16] = s0; G[17] = s1; return; } G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[2] + 8u); G[16] = G[2]; G[4] = G[2]; put(rt, G[17] + 0x2Cu, G[3]); call_here(rt, ctx, 0x08B5C094u); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_089FD040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CC7808u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CC79A4u; G[4] = G[4] + 0x68u; put(rt, G[16] + 0x38u, G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, G[16] + 0x64u); G[2] = 0x08D74FD0u; G[5] = 0xFFFFFFFFu; G[17] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[17] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[16]; call_here(rt, ctx, 0x089BBAC8u); G[4] = G[16]; G[5] = 0x6Cu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[16] = s0; G[17] = s1; }
void func_089FD794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CCFD88u; G[19] = G[4] + 4u; G[17] = G[4] + 8u; put(rt, G[4], G[2]); G[2] = word(rt, G[19] + 0xCu); G[18] = G[4]; if (G[2] != G[17]) { G[16] = G[2]; G[2] = word(rt, G[2] + 0x14u); for (;;) { G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[4] = G[16]; call_here(rt, ctx, 0x08B5F100u); G[16] = G[2]; if (G[17] == G[16]) break; G[2] = word(rt, G[2] + 0x14u); } } G[5] = word(rt, G[19] + 8u); G[4] = G[19]; call_here(rt, ctx, 0x08BF913Cu); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B64654u); }
void func_08A028F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CB9A00u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CB9C58u; put(rt, G[16] + 0x38u, G[2]); G[4] = G[4] + 0x10Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x108u; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, G[16] + 0x104u); G[2] = 0x08D74FD0u; G[17] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[17] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[16] + 0xE8u; call_here(rt, ctx, 0x089B8884u); G[4] = G[16]; call_here(rt, ctx, 0x0896BC0Cu); } G[16] = s0; G[17] = s1; }
void func_08A25C7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 list = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (count <= 0) return;
    const u32 object = word(rt, list);
    ctx.gpr[4] = 0x08CAAFBCu;
    if (object == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    call_virtual(rt, ctx, word(rt, word(rt, object) + 0x54u), {object});
    ctx.fpr[12] = ctx.fpr[0];
    call_args(rt, ctx, 0x08B48F54u, {0x08D6950Cu});
    const float rounded = ctx.fpr[0] + std::bit_cast<float>(0x3F000000u);
    const u32 whole = static_cast<u32>(trunc_w_s(rounded));
    ctx.fpr[0] = std::bit_cast<float>(whole);
    call_args(rt, ctx, 0x08A85B1Cu, {frame.at(0u), whole});
    assign_string_and_release(rt, ctx, destination, frame.at(0u));
}
void func_08A2F3FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = call_args(rt, ctx, 0x08B099C8u, {word(rt, 0x08CF6648u)});
    ctx.gpr[5] = 0x08CF6ED8u;
    if (owner != 0u) {
        call_args(rt, ctx, 0x08A31D60u, {owner});
        call_args(rt, ctx, 0x08A32174u, {frame.at(4u)});
        assign_string_and_release(rt, ctx, destination, frame.at(4u));
    } else {
        call_args(rt, ctx, 0x08A76AACu, {frame.at(0u)});
        assign_string_and_release(rt, ctx, destination, frame.at(0u));
    }
}
void func_08A4064C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[16] = frame.at(4u); G[4] = load16(G[5] + 0x1Au); G[2] = word(rt, G[5] + 0x14u); G[5] = G[6]; G[6] = 4u; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[16]; G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A5767C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = G[5]; G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0x28u); G[16] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[16]; G[4] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x08A56884u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAF778u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[17]; G[2] = word(rt, G[3] + 0xCu); G[5] = G[16]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = G[17]; call_here(rt, ctx, 0x08A78490u); } G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A5E164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[5]; G[18] = word(rt, G[7]); G[16] = word(rt, G[4] + 0x18u); G[17] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB0080u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[18]; call_here(rt, ctx, G[2]); G[6] = G[16] + G[2]; G[4] = G[6] < G[16] ? 1u : 0u; G[7] = G[17] + G[3]; G[7] = G[4] + G[7]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A5E1FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[5]; G[18] = word(rt, G[7]); G[16] = word(rt, G[4] + 0x18u); G[17] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB0080u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[18]; call_here(rt, ctx, G[2]); G[6] = G[16] - G[2]; G[4] = G[16] < G[6] ? 1u : 0u; G[7] = G[17] - G[3]; G[7] = G[7] - G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A6AC10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[4]; G[4] = 0x18u; G[18] = G[5]; G[5] = 4u; G[17] = frame.at(4u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB6B88u; put(rt, G[16], G[2]); G[5] = G[18] + 0x14u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A6972Cu); G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A86078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); ctx.fpr[0] = float_at(rt, G[4] + 0x14u); G[16] = word(rt, G[7]); G[4] = 0x08CB181Cu; ctx.fpr[20] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = std::bit_cast<float>(G[2]); ctx.fpr[12] = ctx.fpr[20]; ctx.fpr[13] = static_cast<float>(static_cast<std::int32_t>(G[2])); call_here(rt, ctx, 0x08B67720u); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; }
void func_08A8DAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[18] = G[6]; G[17] = G[4]; G[7] = G[17] + 0x20u; G[16] = G[4] + 0x14u; G[4] = G[16]; G[6] = G[7]; G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[5] = G[3]; if (G[3] != G[2]) { G[4] = G[3]; G[5] = G[7]; if (G[3] != 0u) { call_here(rt, ctx, 0x08AAE3A8u); G[3] = word(rt, G[16] + 4u); } G[3] += 4u; put(rt, G[16] + 4u, G[3]); } else { call_here(rt, ctx, 0x08C2B298u); } G[4] = G[17]; G[5] = G[19]; G[6] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08A8D574u); }
void func_08A927B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(4u); G[18] = frame.at(8u); G[17] = G[4]; G[4] = G[19]; G[16] = G[7]; call_here(rt, ctx, 0x08A92548u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08A92548u); G[5] = word(rt, frame.at(4u)); G[2] = word(rt, frame.at(8u)); G[4] = frame.at(0u); G[5] ^= G[2]; G[5] = G[5] == 0u ? 1u : 0u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A92850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(4u); G[18] = frame.at(8u); G[17] = G[4]; G[4] = G[19]; G[16] = G[7]; call_here(rt, ctx, 0x08A92548u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08A92548u); G[5] = word(rt, frame.at(4u)); G[2] = word(rt, frame.at(8u)); G[4] = frame.at(0u); G[5] ^= G[2]; G[5] = G[5] != 0u ? 1u : 0u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AA08B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; ctx.fpr[12] = 0.0f; { GuestFrame frame(ctx, 0x30u); G[19] = G[4]; G[4] = frame.at(0u); G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08A87EECu); G[4] = frame.at(0u); G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; call_here(rt, ctx, 0x08AA0690u); const bool ok = G[2] != 0u; G[2] = word(rt, frame.at(0u)); G[16] = 0u; if (ok) { G[16] = 1u; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A87824u); put_float(rt, G[19], ctx.fpr[0]); } else G[16] = 0u; } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AA6908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5]; G[18] = G[5]; call_here(rt, ctx, 0x08AA65E4u); G[17] = G[2]; G[5] = G[2]; G[2] = 0x08D74FD0u; G[3] = 0x08D74FDCu; G[4] = G[16]; G[6] = 0u; put(rt, G[16], G[3]); if (G[17] != 0u) { call_here(rt, ctx, 0x08B86674u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] - 4u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08B88FF8u); G[3] = word(rt, G[16]); } G[4] = G[18]; G[5] = G[3]; G[6] = G[17]; call_here(rt, ctx, 0x08AA61D8u); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AA961C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD5358u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x38u; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x38u); call_here(rt, ctx, 0x08C2FBF8u); G[2] = word(rt, G[17] + 0x38u); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 2); G[5] <<= 2; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[4] = G[17] + 0x20u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C32A04u); G[4] = G[17] + 8u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C3295Cu); G[4] = G[17] + 4u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08B64654u); } G[16] = s0; G[17] = s1; }
void func_08ABBE60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08D70000u; G[20] = G[5]; G[19] = G[4]; G[18] = G[6]; G[16] = word(rt, 0x08D7718Cu); G[17] = G[7]; bool found = false; while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[5] = G[19]; G[6] = G[20]; G[3] = word(rt, G[4]); G[7] = G[18]; G[2] = word(rt, G[3] + 0x38u); G[8] = G[17]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[2] = 1u; found = true; break; } G[16] = word(rt, G[16]); } if (!found) { store8(G[19], 0u); G[2] = 0u; } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08AD44B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x85u); ctx.fpr[1] = float_at(rt, G[4] + 0xDCu); G[2] <<= 4; G[2] += 0x90u; G[2] = G[4] + G[2]; G[5] = G[2] + 0xCu; ctx.fpr[0] = float_at(rt, G[5] + 4u); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); bool upd = true; if (ctx.fpr[1] == ctx.fpr[2]) { ctx.fpr[1] = float_at(rt, G[5] + 0xCu); ctx.fpr[0] = float_at(rt, G[4] + 0xE0u); G[6] = load8(G[4] + 0x84u); if (ctx.fpr[0] == ctx.fpr[1]) { G[3] = word(rt, G[2] + 0xCu); G[2] = load8(G[4] + 0xE4u); if (G[2] == G[3]) { G[3] = load8(G[4] + 0xE5u); G[2] = G[6] & 0xFFu; if (G[3] == G[2]) upd = false; } } } else G[6] = load8(G[4] + 0x84u); if (!upd) return; put_float(rt, G[4] + 0xDCu, ctx.fpr[2]); G[2] = 0x08AD479Cu; ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put_float(rt, G[4] + 0xE0u, ctx.fpr[0]); G[3] = word(rt, G[5]); put(rt, G[4] + 0x80u, G[2]); store8(G[4] + 0xE4u, G[3]); store8(G[4] + 0xE5u, G[6]); }
void func_08AE006C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const s32 x = static_cast<s32>(ctx.gpr[5]);
    const s32 y = static_cast<s32>(ctx.gpr[6]);
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call_args(rt, ctx, 0x08ADDCD4u, {owner, 0x08C9114Cu});
    ctx.gpr[6] = object;
    ctx.gpr[2] = 0x1000u;
    const u32 clamped_x = static_cast<u32>(x < -0x1000 ? -0x1000 : (x < 0x1000 ? x : 0x1000));
    const u32 clamped_y = static_cast<u32>(y < -0x1000 ? -0x1000 : (y < 0x1000 ? y : 0x1000));
    if (object != 0u) {
        ctx.gpr[4] = object;
        ctx.gpr[5] = 1u;
        ctx.gpr[2] = sext(load16(object + 0x24u), 16);
        bool same = false;
        if (ctx.gpr[2] == clamped_x) {
            ctx.gpr[2] = sext(load16(object + 0x26u), 16);
            same = ctx.gpr[2] == clamped_y;
        }
        if (!same) {
            store16(object + 0x24u, clamped_x);
            store16(object + 0x26u, clamped_y);
            call_args(rt, ctx, 0x08ADDF88u, {object, 1u});
        }
    }
}
void func_08AE3B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x50u); G[17] = word(rt, G[5] + 4u); G[2] = word(rt, G[4] + 0xCu); G[5] = G[17] != 0u ? (G[2] | 4u) : (G[2] & ~4u); G[16] = 0u; if (G[2] != G[5]) { G[3] = word(rt, G[4] + 0x18u); put(rt, G[4] + 0xCu, G[5]); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[16] = G[3]; G[18] = 0x15u; while (G[16] != 0u) { put(rt, frame.at(0u), G[18]); G[5] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[4] = sext(load16(G[16] + 0x1Au), 16); call_here(rt, ctx, 0x08AE85E8u); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[16] = G[3]; } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AEDB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0x08C92358u; G[16] = G[6]; call_here(rt, ctx, 0x08AEDA20u); G[7] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2] + 0xCu) & 0x80u; G[3] = 0x16A2u; if (G[2] != 0u) { std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[16])) * static_cast<std::int32_t>(G[3]); G[2] = static_cast<std::uint32_t>(p); p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[17])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[16] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 12); G[3] = ctx.lo; G[17] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 12); } G[3] = 0x1000u; G[4] = 0x1000u; G[2] = static_cast<std::int32_t>(G[17]) < -0x1000 ? 1u : 0u; G[8] = static_cast<std::int32_t>(G[16]) < -0x1000 ? 1u : 0u; G[3] = static_cast<std::int32_t>(G[17]) < 0x1000 ? G[17] : 0x1000u; G[6] = static_cast<std::int32_t>(G[16]) < 0x1000 ? G[16] : 0x1000u; if (G[2] != 0u) G[3] = 0xFFFFF000u; if (G[8] != 0u) G[6] = 0xFFFFF000u; store16(G[7] + 0x42u, G[6]); store16(G[7] + 0x40u, G[3]); } } G[16] = s0; G[17] = s1; }
void func_08AEFD90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const s32 index = static_cast<s32>(ctx.gpr[4]);
    const u32 report = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (index >= 0) {
        const s32 last = static_cast<s32>(word(rt, 0x08D41AD0u + 0x544u) - 1u);
        if (!(last < index)) {
            const u32 record = word(rt, 0x08D41AD0u + 0x540u) + (static_cast<u32>(index) << 6);
            if ((word(rt, record + 0xCu) & 1u) != 0u) {
                ctx.gpr[2] = record;
                return;
            }
        }
    }
    if (report != 0u && (word(rt, 0x08CDFB3Cu) & 1u) != 0u)
        call_args(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, 0xF1Fu, 0x08C925B8u, report, static_cast<u32>(index)});
    ctx.gpr[2] = 0u;
}
void func_08AF61A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[2] = word(rt, G[4] + 0x9Cu); G[3] = G[5]; if (G[2] == 0u) return; ctx.fpr[1] = 0.0f; G[1] = 0x3F800000u; ctx.fpr[4] = std::bit_cast<float>(G[1]); ctx.fpr[6] = ctx.fpr[1]; ctx.fpr[5] = ctx.fpr[1]; if (G[7] != 0u) { ctx.fpr[4] = float_at(rt, G[7] + 0xCu); ctx.fpr[5] = float_at(rt, G[7]); ctx.fpr[6] = float_at(rt, G[7] + 4u); ctx.fpr[1] = float_at(rt, G[7] + 8u); } ctx.fpr[0] = 0.0f; ctx.fpr[3] = ctx.fpr[0]; ctx.fpr[2] = ctx.fpr[0]; if (G[8] != 0u) { ctx.fpr[0] = float_at(rt, G[8] + 8u); ctx.fpr[2] = float_at(rt, G[8]); ctx.fpr[3] = float_at(rt, G[8] + 4u); } G[5] = load16(G[4] + 0x8Cu); G[9] = G[6]; G[8] = G[3]; G[6] = frame.at(0xCu); G[7] = frame.at(0u); put_float(rt, frame.at(0xCu), ctx.fpr[5]); put_float(rt, frame.at(0x10u), ctx.fpr[6]); put_float(rt, frame.at(0x14u), ctx.fpr[1]); put_float(rt, frame.at(0x18u), ctx.fpr[4]); put_float(rt, frame.at(0u), ctx.fpr[2]); put_float(rt, frame.at(4u), ctx.fpr[3]); put_float(rt, frame.at(8u), ctx.fpr[0]); call_here(rt, ctx, 0x08AF5D60u); } }
void func_08B09B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = 0x08D40000u; G[16] = G[4]; G[4] = 0x08D473C0u; G[19] = G[5]; G[18] = G[6]; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[16]); G[16] = G[2] + 0xD4u; G[3] = load8(G[16] + 0x29u); G[4] = G[16]; bool ok = false; if (G[3] != 0u) { call_here(rt, ctx, 0x08B09530u); G[4] = G[18]; G[5] = G[16]; if (G[2] == G[19]) { call_here(rt, ctx, 0x08B09298u); ok = true; } } G[4] = 0x08D473C0u; call_here(rt, ctx, 0x08B3457Cu); G[2] = ok ? 1u : 0u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B0F448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x60u); G[16] = frame.at(0x10u); G[9] = G[8] & 0xFFu; G[8] = G[7]; G[7] = frame.at(0u); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08B0F264u); G[6] = G[17]; G[7] = frame.at(0x40u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6], G[2]); G[16] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[16] != G[7]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); put(rt, G[6] + 8u, G[2]); G[2] = G[17]; put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[4]); } G[16] = s0; G[17] = s1; }
void func_08B0F570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x60u); G[16] = frame.at(0x10u); G[8] = G[7] & 0xFFu; G[7] = G[6]; G[6] = frame.at(0u); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08B0F4E0u); G[6] = G[17]; G[7] = frame.at(0x40u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6], G[2]); G[16] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[16] != G[7]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); put(rt, G[6] + 8u, G[2]); G[2] = G[17]; put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[4]); } G[16] = s0; G[17] = s1; }
void func_08B10E58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D40000u; G[16] = G[4]; G[3] = load8(0x08D47494u); if (G[3] == 0u) { G[16] = s0; G[17] = s1; return; } G[17] = 0x08D40000u; G[4] = 0x08D47468u; call_here(rt, ctx, 0x08B34888u); G[4] = 0x08D40000u; G[2] = 0x08D4745Cu; G[5] = word(rt, G[2] + 4u); G[6] = word(rt, G[16] + 0x10u); while (G[5] != 0u) { G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0x10u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) break; G[5] = word(rt, G[5] + 8u); } G[4] = 0x08D4745Cu; G[6] = G[16] + 4u; call_here(rt, ctx, 0x08B1FC10u); G[4] = 0x08D47468u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34B24u); }
void func_08B10FE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = 0x08D40000u; G[2] = load8(0x08D47494u); if (G[2] == 0u) { G[2] = 0x08D40000u; G[16] = s0; G[17] = s1; jump(ctx, 0x08B2E064u); return; } G[16] = word(rt, 0x08D47460u); G[2] = 0x08D70000u; while (G[16] != 0u) { G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08B105D0u); G[2] = word(rt, G[16] + 8u); G[16] = G[2]; } G[2] = 0x08D70000u; G[16] = word(rt, 0x08D77350u); while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); call_here(rt, ctx, 0x08B10F7Cu); G[16] = word(rt, G[16]); } call_here(rt, ctx, 0x08B174D4u); store8(0x08D47494u, 0u); } G[16] = s0; G[17] = s1; }
void func_08B1B6A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[3] = 0x91A2B3C5u; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) >> 31); G[2] = 0u; G[3] = ctx.hi; G[3] += G[4]; G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 11); G[7] = G[3] - G[5]; G[6] = G[7]; G[3] = G[6] << 8; G[7] <<= 4; G[3] -= G[7]; G[5] = G[3] << 4; G[5] -= G[3]; G[7] = G[4] - G[5]; G[3] = 0x88888889u; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[7])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[7]) >> 31); G[6] &= 0xFFu; G[2] = (G[2] & ~0xFFu) | G[6]; G[3] = ctx.hi; G[3] += G[7]; G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 5); G[3] -= G[4]; G[5] = G[3] << 2; G[4] = G[3] << 6; G[4] -= G[5]; G[4] = G[7] - G[4]; G[3] &= 0xFFu; G[2] = (G[2] & ~0xFF00u) | (G[3] << 8); G[4] &= 0xFFu; G[2] = (G[2] & ~0xFF0000u) | (G[4] << 16); G[2] &= 0x00FFFFFFu; }
void func_08B2D584(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = 0x08D70000u; G[2] = load8(0x08D68D4Cu); G[17] = 0x08D70000u; if (G[2] != 0u) { do { G[4] = 0x64u; call_here(rt, ctx, 0x08C7A988u); G[3] = load8(0x08D68D4Cu); G[17] = 0x08D70000u; } while (G[3] != 0u); } G[4] = 0x08CB542Cu; G[5] = 0x03415002u; G[6] = 0x08D68D48u; G[7] = 4u; G[8] = 0u; G[9] = 0u; G[16] = 0x08D70000u; call_here(rt, ctx, 0x08C7A820u); call_here(rt, ctx, 0x08B2D45Cu); G[4] = word(rt, 0x08D68D44u); if (G[4] != 0u) { call_here(rt, ctx, 0x08B2EE2Cu); G[4] = word(rt, 0x08D68D44u); call_here(rt, ctx, 0x08B64654u); } G[4] = word(rt, 0x08D68D48u); } G[16] = s0; G[17] = s1; jump(ctx, 0x08C7AA00u); }
void func_08B40718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x12Cu; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xBCu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[16]; if (G[2] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x48u); call_here(rt, ctx, G[2]); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xA4u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[2] = word(rt, G[16] + 0xB0u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x4Cu); call_here(rt, ctx, G[3]); } } store8(G[16] + 0xB7u, 0u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_08B41288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x40u); G[16] = G[4]; G[4] = frame.at(0u); G[17] = word(rt, G[5]); call_here(rt, ctx, 0x08B09F98u); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B09528u); G[5] = frame.at(0u); G[4] = G[17] + 0x10u; call_here(rt, ctx, 0x08B09298u); G[6] = load8(G[16] + 0xACu); G[4] = G[17]; G[5] = 0x100u; call_here(rt, ctx, 0x08B089A8u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x98u); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x38u, G[2]); G[4] = G[17]; G[5] = load8(G[16] + 0xACu); call_here(rt, ctx, 0x08B08998u); G[4] = G[2]; G[5] = G[16] + 0xBB8u; call_here(rt, ctx, 0x08B3F7CCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B09290u); } G[16] = s0; G[17] = s1; }
void func_08B489D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x33305452u; G[3] = word(rt, G[4]); G[17] = G[4]; if (G[3] == G[2]) { G[2] = word(rt, G[4] + 4u); G[18] = G[5] - G[2]; put(rt, G[4] + 4u, G[18]); if (G[18] != 0u) { G[2] = word(rt, G[4] + 0xCu); G[19] = 0u; if (static_cast<std::int32_t>(G[2]) > 0) { G[16] = G[17] + 0x10u; do { G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08B48944u); G[2] = word(rt, G[17] + 0xCu); G[19] += 1u; G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] += 0x10u; } while (G[2] != 0u); } } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B55860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[9] = 0x08D6CE00u; G[10] = 0xB1000000u; G[2] = G[4] >> 24; G[11] = 0xB0000000u; G[10] = (G[10] & ~0x00FF0000u) | ((G[2] & 0xFFu) << 16); G[2] = 3u; G[3] = G[6]; G[8] = G[5]; G[11] = (G[11] & 0xFF000000u) | (G[4] & 0x00FFFFFFu); G[12] = 3u; G[6] = 8u; if (G[5] != G[2]) { G[12] = 4u; G[6] = 0x10u; } G[4] = word(rt, G[9] + 0x27Cu); G[5] = word(rt, G[9] + 0x278u); G[7] = word(rt, G[9] + 0x5Cu); G[2] = G[3] + G[6]; G[3] = word(rt, G[9] + 0x25Cu); G[6] = word(rt, G[9] + 0x6Cu); G[2] -= 1u; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> (G[12] & 31u)); G[3] = (G[3] & ~3u) | (G[8] & 3u); G[2] &= 0x3Fu; G[4] |= 0x30u; G[5] |= 0x30u; G[6] |= 0x80u; G[8] = G[7] + 8u; store8(G[9] + 0x258u, G[2]); put(rt, G[9] + 0x25Cu, G[3]); put(rt, G[9] + 0x27Cu, G[4]); put(rt, G[9] + 0x278u, G[5]); put(rt, G[9] + 0x6Cu, G[6]); put(rt, G[7], G[11]); put(rt, G[7] + 4u, G[10]); put(rt, G[9] + 0x5Cu, G[8]); }
void func_08B61484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x20u); G[21] = 4u; G[20] = G[6]; G[19] = 0xFFFFFFFFu; G[18] = G[4]; G[17] = G[5]; G[16] = G[6]; G[5] = G[17]; for (;;) { G[6] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08B7E89Cu); if (G[2] != G[19]) { G[16] -= G[2]; G[17] += G[2]; if (G[16] == 0u) break; G[5] = G[17]; continue; } call_here(rt, ctx, 0x08B77D40u); G[3] = word(rt, G[2]); G[5] = G[17]; if (G[3] != G[21]) break; } G[2] = G[20] - G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
void func_08B639D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[4] = G[16]; G[3] = word(rt, G[5]); G[5] = word(rt, G[6]); G[2] = word(rt, G[3] + 8u); put(rt, frame.at(0u), G[5]); call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[16]; G[6] = frame.at(0u); G[7] = 1u; if (G[2] != 0u) { G[2] = word(rt, frame.at(0u)); G[3] = word(rt, G[2]); put(rt, frame.at(0u), G[3]); } G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0x10u); call_here(rt, ctx, G[2]); G[3] = 0u; if (G[2] != 0u) { G[2] = word(rt, frame.at(0u)); G[3] = 1u; put(rt, G[18], G[2]); } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B7748C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[13] = G[5]; G[3] = G[5] & 3u; G[5] = 1u; G[12] = 0u; put(rt, frame.at(0x18u), G[6]); put(rt, frame.at(0x1Cu), G[7]); put(rt, frame.at(0x20u), G[8]); put(rt, frame.at(0x24u), G[9]); put(rt, frame.at(0x28u), G[10]); put(rt, frame.at(0x2Cu), G[11]); if (G[3] == 1u) G[12] = 2u; else if (G[3] == 0u) G[12] = 1u; else { G[5] = 2u; if (G[3] == 2u) G[12] = 3u; } G[24] = G[13] & 8u; G[15] = G[12] | 0x100u; G[14] = G[13] & 0x200u; if (G[24] != 0u) G[12] = G[15]; G[11] = G[13] & 0x400u; G[8] = G[14] | G[12]; G[9] = G[13] & 0x800u; G[2] = G[11] | G[8]; G[3] = G[9] | G[2]; G[10] = G[13] & 0x4000u; G[7] = G[3] | 4u; if (G[10] != 0u) G[3] = G[7]; G[6] = 0x1A4u; G[5] = G[3]; call_here(rt, ctx, 0x08C7A870u); } }
void func_08B7BBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; } G[16] = s0; jump(ctx, 0x08B7BC20u); }
void func_08B90DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = G[4] + 4u; G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[8] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 3); G[4] = G[16]; if (static_cast<std::int32_t>(G[8]) > 0) { G[7] = 0u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[7] += 1u; put(rt, G[4], G[2]); G[6] += 8u; put(rt, G[4] + 4u, G[3]); G[4] += 8u; } while (G[8] != G[7]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B90DA0u); G[3] = word(rt, G[17] + 4u); G[2] = G[18] - G[16]; G[2] &= ~7u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08BB1544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D70000u; G[17] = 0x08D70000u; G[16] = 0x08D75F50u; G[3] = sext(load8(0x08D75F50u), 8); G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[3] = 0x08D70000u; G[5] = 0x08D75F68u; G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x08D75F5Cu; G[3] = 0x1001u; put(rt, G[2] + 0x18u, G[3]); put(rt, G[2] + 0xCu, G[5]); put(rt, G[2] + 0x10u, 0u); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0x14u, G[5]); put(rt, G[2] + 0x1Cu, 0u); put(rt, G[2] + 0x20u, 0u); put(rt, G[2] + 0x24u, 0u); put(rt, 0x08D75F5Cu, 0u); call_here(rt, ctx, 0x08B64494u); G[4] = 0x088B276Cu; call_here(rt, ctx, 0x08B77B1Cu); } } G[2] = 0x08D75F5Cu; } G[16] = s0; G[17] = s1; }
void func_08C3BE94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[3] = 0x08CD5E58u; G[16] = G[4]; G[5] = 0xFFFFFFFFu; G[2] = word(rt, G[4] + 0x14u); G[4] = G[2] - 4u; G[17] = G[2] - 0xCu; G[2] = 0x08D74FD0u; put(rt, G[16], G[3]); if (G[17] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = 0x08CD5E30u; put(rt, G[16], G[2]); G[4] = G[16] + 0xCu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[16]; G[5] = 0x18u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[16] = s0; G[17] = s1; }
void func_08C3E450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[6] = G[5] - G[4]; G[2] = static_cast<std::int32_t>(G[6]) < 0x10 ? 1u : 0u; G[19] = G[4]; if (G[2] == 0u) { G[2] = G[4] - G[5]; G[2] += G[6]; G[16] = G[5]; G[18] = G[6] + G[2]; G[17] = G[6] - 8u; do { G[16] -= 8u; G[2] = word(rt, G[19]); G[3] = word(rt, G[19] + 4u); G[8] = word(rt, G[16]); G[9] = word(rt, G[16] + 4u); put(rt, G[16], G[2]); G[6] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[17]) >> 3); G[18] -= 8u; put(rt, G[16] + 4u, G[3]); G[4] = G[19]; G[5] = 0u; G[10] = G[16]; call_here(rt, ctx, 0x08C3E380u); G[2] = static_cast<std::int32_t>(G[18]) < 0x10 ? 1u : 0u; G[17] -= 8u; } while (G[2] == 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C4B174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = G[4] + 4u; G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[8] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 2); G[4] = G[16]; if (static_cast<std::int32_t>(G[8]) > 0) { G[7] = 0u; do { G[2] = load16(G[6]); G[3] = load16(G[6] + 2u); G[7] += 1u; store16(G[4], G[2]); G[6] += 4u; store16(G[4] + 2u, G[3]); G[4] += 4u; } while (G[8] != G[7]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08C4B164u); G[3] = word(rt, G[17] + 4u); G[2] = G[18] - G[16]; G[2] &= ~3u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C50DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[4]; G[4] = 0x24u; G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; G[19] = G[2]; call_here(rt, ctx, 0x08B38C78u); G[2] = 0x08CDA8F8u; put(rt, G[20], G[2]); put(rt, G[20] + 4u, G[19]); if (G[19] != 0u) { G[2] = word(rt, G[19] + 4u) + 1u; put(rt, G[19] + 4u, G[2]); } G[2] = 0x08CDA908u; put(rt, G[20], G[2]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08C733EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4] + 8u); G[2] = word(rt, G[4] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; G[5] = G[3] + 1u; if (G[2] != 0u) { G[2] = load8(G[3]); put(rt, G[4] + 8u, G[5]); G[2] += 1u; } else { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x28u); call_here(rt, ctx, G[2]); G[2] += 1u; } G[3] = 0xFFFFFFFFu; if (G[2] == 0u) { G[2] = G[3]; G[16] = s0; return; } G[3] = word(rt, G[16] + 8u); G[2] = word(rt, G[16] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; G[4] = G[16]; if (G[2] != 0u) { G[3] = load8(G[3]); G[2] = G[3]; G[16] = s0; return; } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x24u); G[25] = G[2]; } G[16] = s0; jump(ctx, G[25]); }
void func_08C763A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[19] = G[6]; G[18] = G[5]; G[16] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[17] = G[2]; G[20] = 0x08D77498u; G[3] = G[20] + 0xCu; put(rt, G[16], G[3]); G[4] = G[16]; G[2] = word(rt, G[19]); G[5] = word(rt, G[2] - 0xCu); G[5] = G[17] + G[5]; call_here(rt, ctx, 0x08C75840u); G[4] = G[16]; G[5] = G[18]; G[6] = G[17]; call_here(rt, ctx, 0x08C75AF8u); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x08C75D90u); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08C792EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5] + 4u; G[18] = G[5] + 8u; G[17] = G[4] + 8u; G[16] = G[4]; G[3] = word(rt, G[5] + 4u); G[7] = word(rt, G[5]); G[6] = word(rt, G[5] + 0xCu); G[2] = word(rt, G[3] - 0xCu); put(rt, G[4] + 4u, 0u); G[2] = G[4] + G[2]; G[3] = word(rt, G[7] - 0xCu); G[4] = word(rt, G[5] + 8u); G[3] = G[16] + G[3]; put(rt, G[2], G[4]); G[4] = G[17]; put(rt, G[16], G[7]); put(rt, G[3], G[6]); call_here(rt, ctx, 0x08C771D4u); G[2] = word(rt, G[16]); G[5] = G[17]; G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C54A7Cu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_203
} // namespace gt::game

#define HOT203(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_203::name, gt::ReturnKind::kind)
// HOT203(0x08841D04u, func_08841D04, Word);  // no comparable synthetic case yet
HOT203(0x08B8C4A8u, func_08B8C4A8, Word);
// HOT203(0x089C7090u, func_089C7090, Word);  // no comparable synthetic case yet
HOT203(0x08C70314u, func_08C70314, Word);
HOT203(0x08C5DCF8u, func_08C5DCF8, Word);
HOT203(0x08BDDAD8u, func_08BDDAD8, Word);
HOT203(0x08AB7EC4u, func_08AB7EC4, Word);
// HOT203(0x08841D9Cu, func_08841D9C, Word);  // no comparable synthetic case yet
// HOT203(0x08841E34u, func_08841E34, Word);  // no comparable synthetic case yet
// HOT203(0x08841F80u, func_08841F80, Word);  // no comparable synthetic case yet
// HOT203(0x088420B0u, func_088420B0, Word);  // no comparable synthetic case yet
// HOT203(0x08842908u, func_08842908, Word);  // no comparable synthetic case yet
HOT203(0x08BFFE28u, func_08BFFE28, Word);
HOT203(0x08C0B5DCu, func_08C0B5DC, Word);
HOT203(0x08C1015Cu, func_08C1015C, Word);
HOT203(0x08A0255Cu, func_08A0255C, Word);
// HOT203(0x08A025F4u, func_08A025F4, Word);  // no comparable synthetic case yet
HOT203(0x08C703ACu, func_08C703AC, Word);
HOT203(0x08C5DDD8u, func_08C5DDD8, Word);
HOT203(0x08BDDBA0u, func_08BDDBA0, Word);
HOT203(0x08AB7F5Cu, func_08AB7F5C, Word);
HOT203(0x08843708u, func_08843708, Word);
HOT203(0x088435E8u, func_088435E8, Word);
HOT203(0x08A05490u, func_08A05490, Word);
HOT203(0x089CD7ECu, func_089CD7EC, Float);
// HOT203(0x0882F708u, func_0882F708, Word);  // no comparable synthetic case yet
// HOT203(0x0880D148u, func_0880D148, Word);  // no comparable synthetic case yet
HOT203(0x08A05640u, func_08A05640, Word);
HOT203(0x089E5B5Cu, func_089E5B5C, Float);
// HOT203(0x0883916Cu, func_0883916C, Word);  // no comparable synthetic case yet
// HOT203(0x0880D1E0u, func_0880D1E0, Word);  // no comparable synthetic case yet
// HOT203(0x0880B938u, func_0880B938, Word);  // no comparable synthetic case yet
HOT203(0x08824DA0u, func_08824DA0, Word);
// HOT203(0x08833CACu, func_08833CAC, Word);  // no comparable synthetic case yet
HOT203(0x088437A0u, func_088437A0, Word);
HOT203(0x08843838u, func_08843838, Word);
HOT203(0x08864AC8u, func_08864AC8, Word);
HOT203(0x0886AB7Cu, func_0886AB7C, Word);
// HOT203(0x08877CCCu, func_08877CCC, Word);  // no comparable synthetic case yet
HOT203(0x088CF414u, func_088CF414, Float);
HOT203(0x088DD7D8u, func_088DD7D8, Word);
HOT203(0x088E5234u, func_088E5234, Word);
// HOT203(0x088E6548u, func_088E6548, Word);  // no comparable synthetic case yet
HOT203(0x088EE154u, func_088EE154, Word);
HOT203(0x088F1000u, func_088F1000, Word);
HOT203(0x088FC6FCu, func_088FC6FC, Word);
HOT203(0x089107A0u, func_089107A0, Word);
HOT203(0x089341A0u, func_089341A0, Word);
HOT203(0x08938770u, func_08938770, Word);
HOT203(0x08939920u, func_08939920, Word);
HOT203(0x089399B8u, func_089399B8, Word);
HOT203(0x0893A414u, func_0893A414, Word);
HOT203(0x08940290u, func_08940290, Word);
HOT203(0x08949B04u, func_08949B04, Float);
HOT203(0x0895BC94u, func_0895BC94, Word);
HOT203(0x0896444Cu, func_0896444C, Word);
HOT203(0x089644E4u, func_089644E4, Word);
HOT203(0x0896A860u, func_0896A860, Word);
HOT203(0x0899AC9Cu, func_0899AC9C, Word);
HOT203(0x089B30F8u, func_089B30F8, Word);
// HOT203(0x089C83A4u, func_089C83A4, Word);  // no comparable synthetic case yet
// HOT203(0x089FD040u, func_089FD040, Word);  // no comparable synthetic case yet
// HOT203(0x089FD794u, func_089FD794, Word);  // no comparable synthetic case yet
// HOT203(0x08A028F4u, func_08A028F4, Word);  // no comparable synthetic case yet
HOT203(0x08A25C7Cu, func_08A25C7C, Word);
HOT203(0x08A2F3FCu, func_08A2F3FC, Word);
// HOT203(0x08A4064Cu, func_08A4064C, Word);  // no comparable synthetic case yet
// HOT203(0x08A5767Cu, func_08A5767C, Word);  // no comparable synthetic case yet
// HOT203(0x08A5E164u, func_08A5E164, Word);  // no comparable synthetic case yet
// HOT203(0x08A5E1FCu, func_08A5E1FC, Word);  // no comparable synthetic case yet
HOT203(0x08A6AC10u, func_08A6AC10, Word);
// HOT203(0x08A86078u, func_08A86078, Word);  // no comparable synthetic case yet
// HOT203(0x08A8DAB4u, func_08A8DAB4, Word);  // no comparable synthetic case yet
// HOT203(0x08A927B8u, func_08A927B8, Word);  // no comparable synthetic case yet
// HOT203(0x08A92850u, func_08A92850, Word);  // no comparable synthetic case yet
HOT203(0x08AA08B8u, func_08AA08B8, Word);
// HOT203(0x08AA6908u, func_08AA6908, Word);  // no comparable synthetic case yet
HOT203(0x08AA961Cu, func_08AA961C, Word);
// HOT203(0x08ABBE60u, func_08ABBE60, Word);  // no comparable synthetic case yet
HOT203(0x08AD44B4u, func_08AD44B4, Word);
HOT203(0x08AE006Cu, func_08AE006C, Word);
HOT203(0x08AE3B24u, func_08AE3B24, Word);
HOT203(0x08AEDB60u, func_08AEDB60, Word);
HOT203(0x08AEFD90u, func_08AEFD90, Word);
HOT203(0x08AF61A4u, func_08AF61A4, Word);
HOT203(0x08B09B5Cu, func_08B09B5C, Word);
HOT203(0x08B0F448u, func_08B0F448, Word);
HOT203(0x08B0F570u, func_08B0F570, Word);
HOT203(0x08B10E58u, func_08B10E58, Word);
// HOT203(0x08B10FE8u, func_08B10FE8, Word);  // no comparable synthetic case yet
HOT203(0x08B1B6A8u, func_08B1B6A8, Word);
// HOT203(0x08B2D584u, func_08B2D584, Word);  // no comparable synthetic case yet
HOT203(0x08B40718u, func_08B40718, Word);
HOT203(0x08B41288u, func_08B41288, Word);
HOT203(0x08B489D0u, func_08B489D0, Word);
HOT203(0x08B55860u, func_08B55860, Word);
// HOT203(0x08B61484u, func_08B61484, Word);  // no comparable synthetic case yet
HOT203(0x08B639D8u, func_08B639D8, Word);
HOT203(0x08B7748Cu, func_08B7748C, Word);
HOT203(0x08B7BBF4u, func_08B7BBF4, Word);
HOT203(0x08B90DB0u, func_08B90DB0, Word);
HOT203(0x08BB1544u, func_08BB1544, Word);
HOT203(0x08C3BE94u, func_08C3BE94, Word);
HOT203(0x08C3E450u, func_08C3E450, Word);
HOT203(0x08C4B174u, func_08C4B174, Word);
HOT203(0x08C50DA8u, func_08C50DA8, Word);
HOT203(0x08C733ECu, func_08C733EC, Word);
HOT203(0x08C763A4u, func_08C763A4, Word);
// HOT203(0x08C792ECu, func_08C792EC, Word);  // no comparable synthetic case yet
