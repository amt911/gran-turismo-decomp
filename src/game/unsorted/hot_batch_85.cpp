// Hot batch 85: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with
// --decomp off; batch 84 = PR #206 excluded with --exclude-ref origin/decomp/hot-batch-84, and no address that #206 writes, registers or leaves unregistered is taken here). Before reading any function its
// address was searched for in src/ and in the branch of batch 84. The pool of hot singles is nearly drained (the hot ones left are VFPU, multi-entry or tangled), so this batch takes medium functions
// (10-60 instructions) the routes reach a few times and callees of already-decompiled functions; each was read by hand word by word with `tools/disasm.py` (branch targets and relocations resolved by the
// tool, every delay slot part of its branch's path).
//
//  - settings loaders that turn the bytes of the settings table at 0x08CDE508 into floats (func_089571E0, func_08958354, func_08954F2C, func_0894F438);
//  - a 64-bit counter wait (func_08B1C590), vector erases (func_08BBB6F8, func_08C49B00, func_08C46DDC), a lookup in a sorted table (func_08C40C90, func_08C40B9C), a final insertion sort (func_08BDA64C),
//    a tree insert (func_08B93F38), a tree swap (func_08C1CE44), constructors and forwarders (func_08BD3F30, func_08BD3650, func_088744FC, func_08884420, func_088FA158, func_088F9970, ...).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx.
// Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's; a callee that sets no v0 hands the previous one on).
//
// Written but not registered (their comments and the notes at the registrations say why): func_08B1C590 (every case reaches a thread wait), func_08A4D508 / func_08AD5460 / func_08B32228 (every case reaches
// an import the harness does not replay or a sleep), func_08BBB6F8 / func_08C49B00 / func_08C1CE44 (the compared cases changed no memory: erases at the end of an array, swaps of empty trees) and func_08BEE82C /
// func_08BDA64C / func_08947A30 (every compared call took the early exit; the loop, the insertion sort and the formula never ran).
//
// Compared on their main path but with a side path read from the asm only (each comment lists what the cases did not take): func_08C48120 (the element address: every index was -1), func_08BB3AE4 (the append in
// place: the array was always full), func_08B64CD0 (a1 != 0), func_08B93F38 (the key comparison), func_08B5D1E0 (the release), func_088A79AC (kind != 0), func_08C40554 (one round of the loop only),
// func_08A62D08 (the grow path), func_08ADDE30 (the failure exit), func_08B1AEAC (the exhausted count) and func_08AD97D8 (indexes of 3 or more).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_85 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

inline float fbits(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// cvt.s.w of an integer register (mtc1 + cvt.s.w).
inline float to_float(std::uint32_t value) { return static_cast<float>(static_cast<std::int32_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B1C590(this, -, lo, hi): the signed 64-bit amount (lo = a2, hi = a3) is first adjusted: a positive amount (hi > 0, or hi == 0 with lo != 0) stays; a zero or negative one is taken from the
// counter stored at this[0x10] / this[0x14] (stored - amount, the high word minus the borrow `stored < difference`). Under the lock func_08B34D40(this + 4) / func_08B34D80(this + 4) a record on the stack
// (made by func_08B28D34, destroyed by func_08B28BE0) is linked at this[0x18] with the amount (+0x30 / +0x34) and the previous link (+0x2C) when the stored counter is below the amount (signed high word,
// then unsigned low word); after the unlock the record waits (func_08B28CD4) when it was linked. v0 / v1 = the (adjusted) low / high words. Siblings func_08B1C820 (batch 83) and the forwarders of
// batch 57 and hot_leaves.
// Never compared (every case it gets reaches a thread wait, see the note at its registration): written from the asm only.
void func_08B1C590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t lo = ctx.gpr[6], hi = ctx.gpr[7];
    {
        GuestFrame frame(ctx, 0x60u);
        const std::uint32_t record = frame.at(0u);
        ctx.gpr[4] = record;
        (void)call_keep(rt, ctx, 0x08B28D34u, {record});
        ctx.gpr[4] = self + 4u;
        (void)call_keep(rt, ctx, 0x08B34D40u, {self + 4u});
        bool subtract;
        if (signed_of(hi) > 0) {
            subtract = false;
            ctx.gpr[3] = word(rt, self + 0x14u);
        } else if (hi != 0u) {
            subtract = true;
            ctx.gpr[2] = word(rt, self + 0x10u);
        } else if (lo != 0u) {
            subtract = false;
            ctx.gpr[3] = word(rt, self + 0x14u);
        } else {
            subtract = true;
            ctx.gpr[2] = word(rt, self + 0x10u);
        }
        if (subtract) {
            const std::uint32_t low = ctx.gpr[2];
            const std::uint32_t high = word(rt, self + 0x14u);
            const std::uint32_t difference = low - lo;
            const std::uint32_t borrow = low < difference ? 1u : 0u;
            const std::uint32_t new_high = (high - hi) - borrow;
            ctx.gpr[4] = difference;
            ctx.gpr[5] = new_high;
            ctx.gpr[6] = borrow;
            lo = difference;
            hi = new_high;
            ctx.gpr[3] = word(rt, self + 0x14u);
        }
        const std::uint32_t stored_high = ctx.gpr[3];
        bool linked = signed_of(stored_high) < signed_of(hi);
        ctx.gpr[2] = linked ? 1u : 0u;
        if (!linked && stored_high == hi) {
            linked = word(rt, self + 0x10u) < lo;
            ctx.gpr[2] = linked ? 1u : 0u;
        }
        if (linked) {
            const std::uint32_t previous = word(rt, self + 0x18u);
            ctx.gpr[2] = previous;
            put(rt, self + 0x18u, record);
            put(rt, record + 0x30u, lo);
            put(rt, record + 0x34u, hi);
            put(rt, record + 0x2Cu, previous);
        }
        ctx.gpr[4] = self + 4u;
        (void)call_keep(rt, ctx, 0x08B34D80u, {self + 4u});
        if (linked) {
            ctx.gpr[4] = record;
            (void)call_keep(rt, ctx, 0x08B28CD4u, {record});
        }
        ctx.gpr[4] = record;
        (void)call_keep(rt, ctx, 0x08B28BE0u, {record});
        ctx.gpr[2] = lo;
        ctx.gpr[3] = hi;
    }
}

// func_089571E0(out): reads the settings table at 0x08CDE508 (bytes): out[0] = byte0 * 0.1f; out[4] = byte1 / byte0 and out[8] = byte2 / byte0 when byte0 != 0 (else both 0); out[0xC] = 10.0f / byte5 (1.0f
// when byte5 is 0); out[0x10] = 10.0f / byte6 (0.25f when byte6 is 0). v0 = the table address.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): byte 0 of the table equal to 0 (out[4] = out[8] = 0: `beqz` to 08957250), byte 5 equal to 0 (out[0xC] = 1.0f) and byte 6 equal to 0 (out[0x10] = 0.25f); the compared tables have all three non-zero.
void func_089571E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t table = 0x08CDE508u;
    const std::uint32_t out = ctx.gpr[4];
    put_float(rt, out, psp_mul(to_float(byte_u(rt, table)), fbits(0x3DCCCCCDu)));
    const std::uint32_t count = byte_u(rt, table);
    if (count != 0u) {
        put_float(rt, out + 4u, to_float(byte_u(rt, table + 1u)) / to_float(count));
        put_float(rt, out + 8u, to_float(byte_u(rt, table + 2u)) / to_float(byte_u(rt, table)));
    } else {
        put(rt, out + 4u, 0u);
        put(rt, out + 8u, 0u);
    }
    const std::uint32_t five = byte_u(rt, table + 5u);
    put_float(rt, out + 0xCu, five != 0u ? fbits(0x41200000u) / to_float(five) : fbits(0x3F800000u));
    const std::uint32_t six = byte_u(rt, table + 6u);
    put_float(rt, out + 0x10u, six != 0u ? fbits(0x41200000u) / to_float(six) : fbits(0x3E800000u));
    ctx.gpr[2] = table;
}

// func_08958354(): six records of six bytes at 0x08CDE1B4 become six 24-byte records at 0x08CF3440: +0 = byte0 (a byte store), +4 = byte1 * 0.001f, +8 = byte2 / 100, +0xC = byte3 * 0.001f,
// +0x10 = byte4 * 0.2777778f, +0x14 = byte5 * 0.2777778f. v0 = byte0 of the last record.
// Branches the routes do not take: none (a counted loop of six rounds).
void func_08958354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = 0x08CDE1B4u, target = 0x08CF3440u;
    const float milli = fbits(0x3A83126Fu), hundred = fbits(0x42C80000u), ninth = fbits(0x3E8E38E4u);
    std::uint32_t first = 0u;
    for (int record = 0; record < 6; ++record) {
        const std::uint32_t b2 = byte_u(rt, source + 2u), b1 = byte_u(rt, source + 1u), b3 = byte_u(rt, source + 3u), b4 = byte_u(rt, source + 4u), b5 = byte_u(rt, source + 5u);
        first = byte_u(rt, source);
        put_float(rt, target + 4u, psp_mul(to_float(b1), milli));
        put_float(rt, target + 0xCu, psp_mul(to_float(b3), milli));
        put_float(rt, target + 0x10u, psp_mul(to_float(b4), ninth));
        put_float(rt, target + 0x14u, psp_mul(to_float(b5), ninth));
        rt.memory().store8(target, static_cast<std::uint8_t>(first));
        put_float(rt, target + 8u, to_float(b2) / hundred);
        source += 6u;
        target += 0x18u;
    }
    ctx.gpr[2] = first;
}

// func_08954F2C(): derived values from the settings table (0x08CDE508) into 0x08CF3430: +0 = 1.0f / ((byte 0xF8 or 1.0f) * 9800.0f); +4 = 1.0f / (((byte 0xF9 or 1.0f) * 0.1f) * 100.0f); +8 = byte 0xFA *
// 0.2777778f; +0xC = byte 0xFB * 0.01f. v0 = byte 0xFB.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): byte 0xF8 equal to 0 and byte 0xF9 equal to 0 (the factor stays 1.0f: the `beqz` to 08954F60 / 08954F88); the compared tables have both non-zero.
void func_08954F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t table = 0x08CDE508u, target = 0x08CF3430u;
    const float one = fbits(0x3F800000u);
    float first = one, second = one;
    const std::uint32_t b_f8 = byte_u(rt, table + 0xF8u);
    if (b_f8 != 0u) first = to_float(b_f8);
    put_float(rt, target, one / psp_mul(first, fbits(0x46192000u)));
    const std::uint32_t b_f9 = byte_u(rt, table + 0xF9u);
    if (b_f9 != 0u) second = to_float(b_f9);
    const float scaled = psp_mul(psp_mul(second, fbits(0x3DCCCCCDu)), fbits(0x42C80000u));
    const std::uint32_t b_fb = byte_u(rt, table + 0xFBu);
    const std::uint32_t b_fa = byte_u(rt, table + 0xFAu);
    put_float(rt, target + 8u, psp_mul(to_float(b_fa), fbits(0x3E8E38E4u)));
    put_float(rt, target + 4u, one / scaled);
    put_float(rt, target + 0xCu, psp_mul(to_float(b_fb), fbits(0x3C23D70Au)));
    ctx.gpr[2] = b_fb;
}

// func_0894F438(): the settings table (0x08CDE508) bytes 0x1B8-0x1C3 into 0x08CF2F54: +0, +0xC, +0x18, +0x24 = bytes 0x1B8, 0x1BB, 0x1BE, 0x1C1 as words; +4, +8, +0x10, +0x14, +0x1C, +0x20, +0x28, +0x2C =
// bytes 0x1B9, 0x1BA, 0x1BC, 0x1BD, 0x1BF, 0x1C0, 0x1C2, 0x1C3 divided by 100.0f. v0 = 0x08CF2F54.
// Branches the routes do not take: none (a straight line).
void func_0894F438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t table = 0x08CDE508u, target = 0x08CF2F54u;
    const float hundred = fbits(0x42C80000u);
    const std::uint32_t whole[4] = {byte_u(rt, table + 0x1B8u), byte_u(rt, table + 0x1BBu), byte_u(rt, table + 0x1BEu), byte_u(rt, table + 0x1C1u)};
    const std::uint32_t fraction_offsets[8] = {0x1B9u, 0x1BAu, 0x1BCu, 0x1BDu, 0x1BFu, 0x1C0u, 0x1C2u, 0x1C3u};
    const std::uint32_t fraction_targets[8] = {4u, 8u, 0x10u, 0x14u, 0x1Cu, 0x20u, 0x28u, 0x2Cu};
    float values[8];
    for (int i = 0; i < 8; ++i) values[i] = to_float(byte_u(rt, table + fraction_offsets[i])) / hundred;
    put(rt, target, whole[0]);
    put(rt, target + 0xCu, whole[1]);
    put(rt, target + 0x18u, whole[2]);
    put(rt, target + 0x24u, whole[3]);
    for (int i = 0; i < 8; ++i) put_float(rt, target + fraction_targets[i], values[i]);
    ctx.gpr[2] = target;
}

// func_08C48120(out, cache, source): cache[4] is decremented; source[8] is an entry whose halfword +0xE8 (signed) is `index`, source[0xC] a second object. A second object gets the index too (halfword +0xE8) and,
// for index == -1, cache[2] (halfword) = ((object - *cache[8]) >> 4) * 0xEEEEEEEF; without one the cache's halfword 0 takes the index and, for -1, halfword 2 too. out = { +4 = source[4], +8 = the
// element address *cache[8] + index * 0xF0 (0 for a negative index), +0xC = the second object }. v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): index != -1 on every compared case: the element address (index * 0xF0, `bltz` not taken) is never computed (the answer's +8 is always 0), and the `bne` of the second-object case that skips the `(object - base) >> 4` fixup is not taken either.
void func_08C48120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], cache = ctx.gpr[5], source = ctx.gpr[6];
    const std::uint32_t entry = word(rt, source + 8u);
    const std::uint32_t counter = word(rt, cache + 4u);
    const std::uint32_t second = word(rt, source + 0xCu);
    const std::int32_t index = half_at(rt, entry + 0xE8u);
    put(rt, cache + 4u, counter - 1u);
    if (second != 0u) {
        rt.memory().store16(second + 0xE8u, static_cast<std::uint16_t>(index));
        if (index == -1) {
            const std::uint32_t base = word(rt, word(rt, cache + 8u));
            const std::uint32_t quotient = static_cast<std::uint32_t>(signed_of(second - base) >> 4);
            rt.memory().store16(cache + 2u, static_cast<std::uint16_t>(quotient * 0xEEEEEEEFu));
        }
    } else {
        rt.memory().store16(cache, static_cast<std::uint16_t>(index));
        if (index == -1) rt.memory().store16(cache + 2u, static_cast<std::uint16_t>(index));
    }
    std::uint32_t element = 0u;
    if (index >= 0) element = word(rt, word(rt, cache + 8u)) + (static_cast<std::uint32_t>(index) << 8) - (static_cast<std::uint32_t>(index) << 4);
    const std::uint32_t kept = word(rt, source + 4u);
    put(rt, out + 0xCu, second);
    put(rt, out + 4u, kept);
    put(rt, out + 8u, element);
    ctx.gpr[3] = kept;
    ctx.gpr[2] = out;
}

// func_08BD3F30(this, a1, a2, -, t0 (five words), t1): this[0] = 0x08CC54E0, this[4] = a1, bytes this[0x24] = a2 and this[0x25] = 0, then the five words of *t0 go to +0xC..+0x1C, t1 to +0x20, the
// word at +0x24 (with the bytes just stored) gets bit 16 set and bit 17 cleared, +0x28 = 0. v0 = 0x10000.
// Branches the routes do not take: none (a straight line).
void func_08BD3F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], flag = ctx.gpr[6], source = ctx.gpr[8], extra = ctx.gpr[9];
    put(rt, self, 0x08CC54E0u);
    put(rt, self + 4u, a1);
    const std::uint32_t w4 = word(rt, source + 0x10u), w0 = word(rt, source), w1 = word(rt, source + 4u), w2 = word(rt, source + 8u), w3 = word(rt, source + 0xCu);
    rt.memory().store8(self + 0x24u, static_cast<std::uint8_t>(flag));
    rt.memory().store8(self + 0x25u, 0u);
    std::uint32_t bits = word(rt, self + 0x24u);
    put(rt, self + 0xCu, w0);
    bits = (bits | 0x10000u) & ~0x20000u;
    put(rt, self + 0x10u, w1);
    put(rt, self + 0x14u, w2);
    put(rt, self + 0x18u, w3);
    put(rt, self + 0x1Cu, w4);
    put(rt, self + 0x20u, extra);
    put(rt, self + 0x24u, bits);
    put(rt, self + 0x28u, 0u);
    ctx.gpr[2] = 0x10000u;
}

// func_08BD3650(this, a1, a2, -, t0, t1): the same shape with two words: this[0] = 0x08CC54A8, bytes this[0x14] = a2 and this[0x15] = 0, the word +0x14 gets bit 16 set and bit 17 cleared, this[4] = a1,
// +0xC = t0, +0x10 = t1, +0x18 = 0. v0 = 0x08CC54A8.
// Branches the routes do not take: none (a straight line).
void func_08BD3650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], flag = ctx.gpr[6], first = ctx.gpr[8], second = ctx.gpr[9];
    rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(flag));
    rt.memory().store8(self + 0x15u, 0u);
    std::uint32_t bits = word(rt, self + 0x14u);
    put(rt, self, 0x08CC54A8u);
    bits = (bits | 0x10000u) & ~0x20000u;
    put(rt, self + 4u, a1);
    put(rt, self + 0xCu, first);
    put(rt, self + 0x10u, second);
    put(rt, self + 0x14u, bits);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = 0x08CC54A8u;
    ctx.gpr[6] = bits;
}

// func_08B9B6E0(out, source): the three words that *source points at are copied to out[0], out[4], out[8] (the pointer is read again before each word). v0 = *source.
// Branches the routes do not take: none (a straight line).
void func_08B9B6E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5];
    std::uint32_t pointer = word(rt, source);
    put(rt, out, word(rt, pointer));
    pointer = word(rt, source);
    put(rt, out + 4u, word(rt, pointer + 4u));
    pointer = word(rt, source);
    const std::uint32_t last = word(rt, pointer + 8u);
    put(rt, out + 8u, last);
    ctx.gpr[2] = pointer;
    ctx.fpr[0] = fbits(last);
}

// func_08B3A410(out, range, step): a range {start = range[0], length = range[4]} is aligned to multiples of step: a step below 2 copies it; otherwise out[0] = start rounded up to a multiple of step and out[4]
// = what is left of the length after the rounding (0 when the rounding passes the end), rounded down to a multiple of step. v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a step below 2 (the copy of the range) and a length shorter than the rounding (`movn`: left = 0); the compared ranges all have a step of 2 or more and room after the rounding.
void func_08B3A410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], range = ctx.gpr[5], step = ctx.gpr[6];
    if (step < 2u) {
        const std::uint32_t length = word(rt, range + 4u), start = word(rt, range);
        ctx.gpr[2] = length;
        ctx.gpr[3] = start;
        put(rt, out + 4u, length);
        put(rt, out, start);
    } else {
        const std::uint32_t start = word(rt, range), length = word(rt, range + 4u);
        const std::uint32_t aligned = step * ((step + start - 1u) / step);
        const std::uint32_t skipped = aligned - start;
        std::uint32_t left = length - skipped;
        if (length < skipped) left = 0u;
        put(rt, out, aligned);
        left = step * (left / step);
        put(rt, out + 4u, left);
        ctx.gpr[2] = left;
        ctx.gpr[3] = aligned;
    }
    ctx.gpr[7] = out;
    ctx.gpr[2] = out;
}

// func_08BD2CB8(this, delta): v0 = *this; a non-zero value gets delta added and stored back and func_088FBACC(the new value) runs as a tail call; zero leaves with v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): *this == 0 (the `beqz` to 08BD2CD4: v0 = 0, no store, no tail call).
void func_08BD2CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = word(rt, self);
    const std::uint32_t updated = value + ctx.gpr[5];
    ctx.gpr[2] = value;
    ctx.gpr[3] = updated;
    ctx.gpr[6] = self;
    if (value == 0u) {
        ctx.gpr[4] = updated;
        return;
    }
    put(rt, self, updated);
    tail_call(ctx, 0x088FBACCu, {updated});
}

// func_08B52D70(a0, a1, a2): a tail call of func_08B54F24(0x08D695C8, a0, a1, a2); v0 / v1 stay a0 / a1.
// Branches the routes do not take: none (a straight line).
void func_08B52D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    ctx.gpr[2] = a0;
    ctx.gpr[3] = a1;
    tail_call(ctx, 0x08B54F24u, {0x08D695C8u, a0, a1, a2});
}

// func_08936268(this): v0 = 0; when the signed byte this[0x10] is not -1, a tail call of func_08936238(this, that byte). (Sibling of func_08935F64, batch 64: here a2 holds the byte as well.)
// Branches the routes do not take: the byte -1 (v0 = 0, no tail call).
void func_08936268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, self + 0x10u)));
    ctx.gpr[6] = value;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0u;
    if (value == 0xFFFFFFFFu) return;
    tail_call(ctx, 0x08936238u, {self, value});
}

// func_08C1CE44(a, b): swaps the contents of two tree headers (root +8, leftmost +0xC, rightmost +0x10, count +0x14); the roots' parent links (root[4]) are repointed at the new header's sentinel (header + 4),
// and an empty tree takes the other's root and gets its own sentinel as leftmost / rightmost with no root. v0 = the old count of b.
// Never compared (see the note at its registration): written from the asm only.
void func_08C1CE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    const std::uint32_t root_a = word(rt, a + 8u);
    if (root_a == 0u) {
        const std::uint32_t root_b = word(rt, b + 8u);
        if (root_b != 0u) {
            put(rt, a + 8u, root_b);
            put(rt, a + 0xCu, word(rt, b + 0xCu));
            put(rt, a + 0x10u, word(rt, b + 0x10u));
            put(rt, root_b + 4u, a + 4u);
            put(rt, b + 0x10u, b + 4u);
            put(rt, b + 8u, 0u);
            put(rt, b + 0xCu, b + 4u);
        }
    } else if (word(rt, b + 8u) == 0u) {
        put(rt, b + 8u, root_a);
        put(rt, b + 0xCu, word(rt, a + 0xCu));
        put(rt, b + 0x10u, word(rt, a + 0x10u));
        put(rt, root_a + 4u, b + 4u);
        put(rt, a + 0x10u, a + 4u);
        put(rt, a + 8u, 0u);
        put(rt, a + 0xCu, a + 4u);
    } else {
        const std::uint32_t root_b = word(rt, b + 8u);
        put(rt, a + 8u, root_b);
        put(rt, b + 8u, root_a);
        const std::uint32_t left_a = word(rt, a + 0xCu), left_b = word(rt, b + 0xCu);
        put(rt, a + 0xCu, left_b);
        put(rt, b + 0xCu, left_a);
        const std::uint32_t right_b = word(rt, b + 0x10u), right_a = word(rt, a + 0x10u);
        put(rt, a + 0x10u, right_b);
        put(rt, b + 0x10u, right_a);
        put(rt, word(rt, a + 8u) + 4u, a + 4u);
        put(rt, word(rt, b + 8u) + 4u, b + 4u);
    }
    const std::uint32_t count_a = word(rt, a + 0x14u), count_b = word(rt, b + 0x14u);
    put(rt, a + 0x14u, count_b);
    put(rt, b + 0x14u, count_a);
    ctx.gpr[2] = count_b;
    ctx.gpr[3] = count_a;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08C40554(this, source, count): func_08C402FC(this, source, count) first; then, when source[0] (the element array) is not 0 and count is not 0, count elements of 32 bytes are built in place: func_08AF7684(array +
// i * 32, this) for i in 0..count-1 (the array pointer is read again each round). v0 = source[0] (a null array or an empty count) or the last element call's v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): source[0] == 0 (the `j` to 08C405C0 with v0 = 0), count == 0 (leaves with v0 = source[0]) and more than one element: every compared call has count 1, so the loop runs one round and the `bnel` back to 08C40590 (with the array pointer read again) never loops.
void func_08C40554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5], count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08C402FCu, {self, source, count});
    ctx.gpr[2] = word(rt, source);
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (count == 0u) return;
    std::uint32_t index = 0u;
    do {
        ctx.gpr[4] = ctx.gpr[2] + (index << 5);
        ++index;
        ctx.gpr[5] = self;
        (void)call_keep(rt, ctx, 0x08AF7684u, {ctx.gpr[4], self});
        if (count == index) break;
        ctx.gpr[2] = word(rt, source);
    } while (true);
}

// func_08BF933C(this, begin, end): this[0] = (end - begin) >> 3 (the number of 8-byte items), this[4] = 0, this[8] = 0; func_08BF9270(count, 0) answers a pair (v0, v1) that is stored as this[4] and this[8]
// (v0 = this[8], v1 = this[4]); when v1 > 0 the tail call func_08BF930C(v0, v1, begin, this) copies the items; otherwise it leaves with v0 / v1 of the first call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): v1 <= 0 (the `blez` to 08BF93B4: returns v0 / v1 of func_08BF9270 without the tail call); every compared call has v1 > 0.
void func_08BF933C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], begin = ctx.gpr[5];
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(ctx.gpr[6] - begin) >> 3);
    std::uint32_t first, second;
    {
        GuestFrame frame(ctx, 0x30u);
        ctx.gpr[6] = count;
        ctx.gpr[4] = count;
        ctx.gpr[5] = 0u;
        put(rt, self, count);
        put(rt, self + 4u, 0u);
        put(rt, self + 8u, 0u);
        const GuestResult result = call_keep(rt, ctx, 0x08BF9270u, {count, 0u}, {});
        first = result.v0;
        second = result.v1;
        put(rt, self + 4u, second);
        put(rt, self + 8u, first);
        ctx.gpr[4] = first;
        ctx.gpr[5] = second;
        ctx.gpr[6] = begin;
        ctx.gpr[7] = self;
        if (signed_of(second) <= 0) return;
    }
    tail_call(ctx, 0x08BF930Cu, {first, second, begin, self});
}

// func_088FA158(out, a1): builds a 32-byte object (func_08B62344(0x20)): vtable 0x08CC40C0, +4 = 0, +8 = 0, +0xC = 0, +0x10 = 0, +0x14 = 0, +0x18 = 0, +0x1C = a1; out[0] = the object. A function-local static
// (guard byte 0x08D76150, object 0x08D76158) is built on first use: when the guard byte is 0 and func_08B6441C(guard) lets this caller build it, the static gets vtable 0x08CC40E0 and func_08B64494(guard)
// releases the guard. out[4] = 0x08D76158. v0 = the guard byte, or the last call's v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte already set (the first `bnez` to 088FA204) and func_08B6441C answering 0 (the `beqz` to 088FA204): every compared call builds the static.
void func_088FA158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08D76150u, object = 0x08D76158u;
    const std::uint32_t out = ctx.gpr[4], a1 = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x20u});
        put(rt, block, 0x08CC40C0u);
        put(rt, block + 0xCu, 0u);
        put(rt, block + 8u, 0u);
        put(rt, block + 4u, 0u);
        put(rt, block + 0x10u, 0u);
        put(rt, block + 0x18u, 0u);
        put(rt, block + 0x14u, 0u);
        put(rt, block + 0x1Cu, a1);
        put(rt, out, block);
        ctx.gpr[3] = 0x08D70000u;
        ctx.gpr[5] = block + 0x10u;
        ctx.gpr[6] = 0x08D70000u;
        const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        ctx.gpr[2] = flag;
        if (flag == 0u) {
            ctx.gpr[4] = guard;
            if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                ctx.gpr[5] = 0x08D70000u;
                ctx.gpr[2] = 0x08CC40E0u;
                put(rt, object, 0x08CC40E0u);
                ctx.gpr[4] = guard;
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        put(rt, out + 4u, object);
    }
}

// func_088F9970(this, a1): this[0] = 0x08CC3F08, this[4] = 0; a 24-byte object (func_08B62344(0x18)) with vtable 0x08CC3FD8, +4 = 0x08CC4004, +8 = 0, +0xC = 0, +0x10 = 0, byte +0x14 = 0 is stored at this[8]. A
// function-local static (guard byte 0x08D76140, object 0x08D7614C) is built on first use: when the guard byte is 0 and func_08B6441C(guard) lets this caller build it, the static gets vtable 0x08CC4090 and
// func_08B64494(guard) releases the guard. this[0xC] = 0x08D7614C and the object's +0x10 = a1. v0 = the object.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte already set (the first `bnez` to 088F9A2C) and func_08B6441C answering 0 (the `beqz` to 088F9A2C): every compared call builds the static.
void func_088F9970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08D76140u, object = 0x08D7614Cu;
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC3F08u);
    put(rt, self + 4u, 0u);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3FD8u);
    put(rt, block + 4u, 0x08CC4004u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    rt.memory().store8(block + 0x14u, 0u);
    put(rt, self + 8u, block);
    ctx.gpr[5] = 0x08D70000u;
    ctx.gpr[4] = 0x08D70000u;
    const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[2] = flag;
    if (flag == 0u) {
        ctx.gpr[4] = guard;
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            ctx.gpr[2] = 0x08CC4090u;
            put(rt, object, 0x08CC4090u);
            ctx.gpr[4] = guard;
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        }
    }
    const std::uint32_t current = word(rt, self + 8u);
    put(rt, self + 0xCu, object);
    put(rt, current + 0x10u, a1);
    ctx.gpr[2] = current;
}

// func_088744FC(this) and func_08884420(this): the same shape: func_0887E1E0(this), this[0] = the vtable; a block (func_08B62344(SIZE)) is initialised by func_08874338 / func_08884314 and stored at this[0xC]; a
// function-local static (guard byte, object) is built on first use: when the guard byte is 0 and func_08B6441C(guard) lets this caller build it, the static gets its vtable and func_08B64494(guard) releases
// the guard; this[0x10] = the static's address. func_08884420 then tail-calls func_088824F8(this, 0).
struct StaticObjectShape {
    std::uint32_t vtable, size, initialiser, guard, object, object_vtable;
};

void static_object_constructor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const StaticObjectShape &shape) {
    const std::uint32_t self = ctx.gpr[4];
    (void)call_keep(rt, ctx, 0x0887E1E0u, {self});
    put(rt, self, shape.vtable);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {shape.size});
    (void)call_keep(rt, ctx, shape.initialiser, {block});
    put(rt, self + 0xCu, block);
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[2] = 0x08D70000u;
    const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, shape.guard)));
    ctx.gpr[5] = flag;
    ctx.gpr[4] = shape.guard;
    if (flag == 0u) {
        const std::uint32_t built = call_keep(rt, ctx, 0x08B6441Cu, {shape.guard});
        ctx.gpr[5] = 0x08D70000u;
        ctx.gpr[4] = shape.guard;
        if (built != 0u) {
            ctx.gpr[2] = shape.object_vtable;
            put(rt, shape.object, shape.object_vtable);
            (void)call_keep(rt, ctx, 0x08B64494u, {shape.guard});
        }
    }
    put(rt, self + 0x10u, shape.object);
}

// func_088744FC(this): the shape above with vtable 0x08CBFA98, a 28-byte block (func_08874338), guard 0x08D753C8, object 0x08D753D0 (vtable 0x08CBFAC0). v0 as the last instructions leave it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C answering 0 (the `beqz` to 08874590, nothing built); the guard-set path is taken by some calls.
void func_088744FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    static_object_constructor(rt, ctx, {0x08CBFA98u, 0x1Cu, 0x08874338u, 0x08D753C8u, 0x08D753D0u, 0x08CBFAC0u});
}

// func_08884420(this): the shape above with vtable 0x08CC1460, a 0x1980-byte block (func_08884314), guard 0x08D75E38, object 0x08D75E40 (vtable 0x08CC14C0); then func_088824F8(this, 0) as a tail call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C answering 0 (the `beqz` to 088844B8, nothing built); the guard-set path is taken by some calls.
void func_08884420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        static_object_constructor(rt, ctx, {0x08CC1460u, 0x1980u, 0x08884314u, 0x08D75E38u, 0x08D75E40u, 0x08CC14C0u});
    }
    tail_call(ctx, 0x088824F8u, {self, 0u});
}

// func_08AD07B8(this): func_08B0CC04(this + 0x248), this[0x360] = 0, this[0x364] = 0, then a tail call of func_08B1A5B4(this + 0x368).
// Branches the routes do not take: none (a straight line).
void func_08AD07B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = self + 0x248u;
        (void)call_keep(rt, ctx, 0x08B0CC04u, {self + 0x248u});
        ctx.gpr[4] = self + 0x368u;
        put(rt, self + 0x360u, 0u);
        put(rt, self + 0x364u, 0u);
    }
    tail_call(ctx, 0x08B1A5B4u, {self + 0x368u});
}

// func_0886FC04(this): func_0887E1E0(this), this[0] = 0x08CBF868, this[0x10] = 0, this[0xC] = 0. v0 = 0x08CBF868.
// Branches the routes do not take: none (a straight line).
void func_0886FC04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x0887E1E0u, {self});
    put(rt, self, 0x08CBF868u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0xCu, 0u);
    ctx.gpr[2] = 0x08CBF868u;
}

// func_088691D8(this): func_08869CCC(this + 0x1B0, 0), func_08869CE8(this + 0x1B0, 0), then a tail call of func_08869148(this).
// Branches the routes do not take: none (a straight line).
void func_088691D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x08869CCCu, {self + 0x1B0u, 0u});
        ctx.gpr[4] = self + 0x1B0u;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x08869CE8u, {self + 0x1B0u, 0u});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08869148u, {self});
}

// func_08AF4408(this, a1, a2, a3): func_08AF43B4(this, a1, a2, a3), func_08AF43D0(this, a2) and a tail call of func_08AF43EC(this, a3).
// Branches the routes do not take: none (a straight line).
void func_08AF4408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08AF43B4u, {self});
        ctx.gpr[4] = self;
        ctx.gpr[5] = a2;
        (void)call_keep(rt, ctx, 0x08AF43D0u, {self, a2});
        ctx.gpr[4] = self;
        ctx.gpr[5] = a3;
    }
    tail_call(ctx, 0x08AF43ECu, {self, a3});
}

// func_088A3FF0(this): this[0x2FC] = func_088A6164(this + 8); func_088A60E8(this + 8, this + 0x1E8, 0x80) fills the buffer; this[0x30] = this + 8. v0 = the second call's.
// Branches the routes do not take: none (a straight line).
void func_088A3FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t inner = self + 8u;
    ctx.gpr[4] = inner;
    const std::uint32_t handle = call_keep(rt, ctx, 0x088A6164u, {inner});
    ctx.gpr[4] = inner;
    put(rt, self + 0x2FCu, handle);
    ctx.gpr[5] = self + 0x1E8u;
    ctx.gpr[6] = 0x80u;
    (void)call_keep(rt, ctx, 0x088A60E8u, {inner, self + 0x1E8u, 0x80u});
    put(rt, self + 0x30u, inner);
}

// func_08B7E518(a0, a1, a2): a tail call of func_08B7E564(func_08B796C4(a0, a1, a2), a0, a1, a2).
// Branches the routes do not take: none (a straight line).
void func_08B7E518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, 0x08B796C4u, {a0, a1, a2});
    }
    tail_call(ctx, 0x08B7E564u, {result, a0, a1, a2});
}

// func_08BD96E0(this): func_08909D98(this[0xC]); func_08BBD3B4(this[0x10], this[0xC]); then a tail call of func_08BBEA18(this[0x10] + 0x98, this).
// Branches the routes do not take: none (a straight line).
void func_08BD96E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t item = word(rt, self + 0xCu);
        ctx.gpr[4] = item;
        (void)call_keep(rt, ctx, 0x08909D98u, {item});
        ctx.gpr[4] = word(rt, self + 0x10u);
        ctx.gpr[5] = item;
        (void)call_keep(rt, ctx, 0x08BBD3B4u, {word(rt, self + 0x10u), item});
        target = word(rt, self + 0x10u) + 0x98u;
        ctx.gpr[4] = target;
        ctx.gpr[5] = self;
    }
    tail_call(ctx, 0x08BBEA18u, {target, self});
}

// func_08C46DDC(this, at): the 16-bit item at `at` is removed from the array whose end is this[4]: the items after it (this[4] - (at + 2) bytes) are moved down by func_08B79BAC(at, at + 2, bytes) unless
// `at + 2` is the end; this[4] -= 2. v0 = at.
// Branches the routes do not take: none that the compared cases skip (the `beq` took both ways).
void func_08C46DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], at = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t next = at + 2u;
    std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[2] = next;
    ctx.gpr[3] = end;
    ctx.gpr[4] = at;
    ctx.gpr[5] = next;
    ctx.gpr[6] = end - next;
    if (next != end) {
        (void)call_keep(rt, ctx, 0x08B79BACu, {at, next, end - next});
        end = word(rt, self + 4u);
    }
    put(rt, self + 4u, end - 2u);
    ctx.gpr[2] = at;
}

// func_08C40C90(tree, index, key) and func_08C40B9C(tree, index, key): the key is kept on the stack; end = tree + index * 8; for a non-null tree, found = LOOKUP(tree, end, &key) (func_08C40C04 / func_08C40B10);
// a result other than `end` whose first word passes func_08B35FD8(found[0], key) == 0 is the answer, else 0 (a null tree answers 0 too).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null tree (the `beqz` to 08C40CDC / 08C40BE8), a lookup that answers `end` (the `beq`) and func_08B35FD8 answering non-zero (the final `beqz`): every compared call finds the entry (answer = the entry).
void lookup_in_tree(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t lookup) {
    const std::uint32_t tree = ctx.gpr[4], key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t end = tree + (ctx.gpr[5] << 3);
    put(rt, frame.at(0u), key);
    ctx.gpr[5] = end;
    ctx.gpr[6] = frame.at(0u);
    std::uint32_t found = 0u;
    if (tree != 0u) {
        const std::uint32_t result = call_keep(rt, ctx, lookup, {tree, end, frame.at(0u)});
        if (result != end) {
            ctx.gpr[4] = word(rt, result);
            ctx.gpr[5] = word(rt, frame.at(0u));
            const std::uint32_t verdict = call_keep(rt, ctx, 0x08B35FD8u, {word(rt, result), word(rt, frame.at(0u))});
            ctx.gpr[2] = result;
            if (verdict == 0u) found = result;
        }
    }
    ctx.gpr[2] = found;
}
void func_08C40C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_in_tree(rt, ctx, 0x08C40C04u); }
void func_08C40B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_in_tree(rt, ctx, 0x08C40B10u); }

// func_08B1AEAC(dst, source, count): the stream state on the stack holds `source`; up to `count` 16-bit items are read with func_08B1AE1C(&state, 1) into dst (a zero item is stored and ends the loop with
// v0 = 0); v0 = dst when count items were read without a zero (also when count <= 0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a count exhausted without a zero item (the `bgtz` falling to 08B1AEF8: v0 = dst) and a count <= 0 at entry; every compared call leaves through the zero item (v0 = 0).
void func_08B1AEAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4], source = ctx.gpr[5];
    std::int32_t remaining = signed_of(ctx.gpr[6]);
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), source);
    std::uint32_t at = dst;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = 1u;
    while (remaining > 0) {
        --remaining;
        const std::uint32_t item = call_keep(rt, ctx, 0x08B1AE1Cu, {frame.at(0u), 1u}) & 0xFFFFu;
        ctx.gpr[2] = item;
        rt.memory().store16(at, static_cast<std::uint16_t>(item));
        at += 2u;
        if (item == 0u) return;
        ctx.gpr[4] = frame.at(0u);
        ctx.gpr[5] = 1u;
    }
    ctx.gpr[2] = dst;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08C4C9DC(this): func_08B2B6A4(this), this[0] = 0x08CD7EB0 and a tail call of func_08C4C964(this). (The code after its `j` belongs to other functions.)
// Branches the routes do not take: none (a straight line).
void func_08C4C9DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B2B6A4u, {self});
        ctx.gpr[2] = 0x08CD7EB0u;
        put(rt, self, 0x08CD7EB0u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08C4C964u, {self});
}

// func_08B2A594(this, limit, source, length, extra): answers 0 at once when limit < length + 8 (unsigned); otherwise func_08B79A88(this + 8, source, length) runs and then func_08B2A438(this, length, extra, 0)
// with f12 = 0x3EA770CA and f13 = 0x3F5BC524; v0 = 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): limit < length + 8 (the `bnez` to 08B2A608: v0 = 0 without any call); every compared call takes the copy.
void func_08B2A594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], limit = ctx.gpr[5], source = ctx.gpr[6], length = ctx.gpr[7], extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0x10u);
    const bool too_small = limit < length + 8u;
    ctx.gpr[2] = too_small ? 1u : 0u;
    ctx.gpr[5] = source;
    ctx.gpr[6] = length;
    ctx.gpr[4] = self + 8u;
    ctx.gpr[3] = 0u;
    if (too_small) {
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_keep(rt, ctx, 0x08B79A88u, {self + 8u, source, length});
    ctx.gpr[4] = self;
    ctx.gpr[5] = length;
    ctx.gpr[6] = extra;
    ctx.fpr[12] = fbits(0x3EA770CAu);
    ctx.fpr[13] = fbits(0x3F5BC524u);
    ctx.gpr[7] = 0u;
    (void)call_keep(rt, ctx, 0x08B2A438u, {self, length, extra, 0u}, {fbits(0x3EA770CAu), fbits(0x3F5BC524u)});
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// func_08B93F38(this, force_left, parent, key): a tree node insertion: a 24-byte node (func_08B35DA0) takes the key (+0x10 = *key, byte +0x14 = key byte 4) and func_08B5F2B8(left, node, parent, this + 4)
// links it; this[0x14] counts one more. `left` is 1 when force_left is non-zero, the parent is the header (this + 4), or *key < parent[0x10] (signed), else 0. v0 = the node.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): force_left != 0 (the `bnez` to 08B93F8C), the comparison `*key < parent[0x10]` (the parent is always the header, so `beq a2, s1` takes the left flag), and a failed allocation (slot == 0 is impossible here: node + 0x10).
void func_08B93F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], force_left = ctx.gpr[5], parent = ctx.gpr[6], key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t header = self + 4u;
    std::uint32_t left;
    ctx.gpr[4] = 0x18u;
    if (force_left != 0u) {
        left = 1u;
    } else if (parent == header) {
        left = 1u;
    } else {
        const std::uint32_t mine = word(rt, key), theirs = word(rt, parent + 0x10u);
        ctx.gpr[2] = signed_of(mine) < signed_of(theirs) ? 1u : 0u;
        ctx.gpr[3] = theirs;
        left = ctx.gpr[2] != 0u ? 1u : 0u;
    }
    const std::uint32_t node = call_keep(rt, ctx, 0x08B35DA0u, {0x18u});
    const std::uint32_t slot = node + 0x10u;
    ctx.gpr[8] = slot;
    if (slot != 0u) {
        const std::uint32_t tag = byte_u(rt, key + 4u);
        const std::uint32_t value = word(rt, key);
        ctx.gpr[3] = tag;
        ctx.gpr[2] = value;
        put(rt, node + 0x10u, value);
        rt.memory().store8(slot + 4u, static_cast<std::uint8_t>(tag));
    }
    ctx.gpr[4] = left;
    ctx.gpr[5] = node;
    ctx.gpr[6] = parent;
    ctx.gpr[7] = header;
    (void)call_keep(rt, ctx, 0x08B5F2B8u, {left, node, parent, header});
    const std::uint32_t count = word(rt, self + 0x14u);
    ctx.gpr[3] = count + 1u;
    ctx.gpr[2] = node;
    put(rt, self + 0x14u, count + 1u);
}

// func_08BDA64C(begin, end): a final insertion sort of 8-byte items: a range shorter than 0x88 bytes is a tail call of func_08BDA574(begin, end); otherwise func_08BDA574(begin, begin + 0x80) sorts the head and
// every later item is inserted by func_08BDA4F0(item_address, &copy_of_item, next_address), the item copied to the stack first. v0 = the last call's.
// Never compared (see the note at its registration): written from the asm only.
void func_08BDA64C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = ctx.gpr[4], end = ctx.gpr[5];
    if (signed_of(end - begin) < 0x88) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08BDA574u, {begin, end});
        return;
    }
    {
        GuestFrame frame(ctx, 0x20u);
        std::uint32_t item = begin + 0x80u;
        ctx.gpr[2] = 0u;
        ctx.gpr[5] = item;
        (void)call_keep(rt, ctx, 0x08BDA574u, {begin, item});
        if (end != item) {
            do {
                const std::uint32_t low = word(rt, item), high = word(rt, item + 4u);
                ctx.gpr[2] = low;
                ctx.gpr[3] = high;
                const std::uint32_t here = item;
                item += 8u;
                put(rt, frame.at(0u), low);
                put(rt, frame.at(4u), high);
                ctx.gpr[4] = here;
                ctx.gpr[5] = frame.at(0u);
                ctx.gpr[6] = item;
                (void)call_keep(rt, ctx, 0x08BDA4F0u, {here, frame.at(0u), item});
            } while (end != item);
        }
    }
}

// func_08AD97D8(index): the object at 0x08D405D0 has a current index at +0x14: for index < 3, v0 = the old one, and a different index is stored (before func_08AE2160(index) runs, as the delay slot of the call
// does); for larger indexes an error is logged by func_08AD83B8(the word at 0x08CDFB40, 0x08C9085C, 0x43C, 0x08C90A18, index) when bit 0 of the word at 0x08CDFB3C is set. v0 = -1 in that case.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an index of 3 or more (the `beqz v1` to 08AD981C: the error log by func_08AD83B8 when bit 0 of the word at 0x08CDFB3C is set, else v0 = -1); every compared call has an index below 3.
void func_08AD97D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t state = 0x08D405D0u;
    const std::uint32_t index = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = state;
    const std::uint32_t previous = word(rt, state + 0x14u);
    if (index < 3u) {
        ctx.gpr[3] = 1u;
        if (previous != index) {
            put(rt, state + 0x14u, index);
            (void)call_keep(rt, ctx, 0x08AE2160u, {index, state});
        }
        ctx.gpr[2] = previous;
        return;
    }
    const std::uint32_t flags = word(rt, 0x08CDFB3Cu);
    ctx.gpr[3] = flags & 1u;
    ctx.gpr[5] = 0x08C9085Cu;
    ctx.gpr[7] = 0x08C90A18u;
    ctx.gpr[8] = index;
    ctx.gpr[6] = 0x43Cu;
    if ((flags & 1u) != 0u) {
        ctx.gpr[2] = 0x08CD0000u;
        (void)call_keep(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C9085Cu, 0x43Cu, 0x08C90A18u, index});
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08BBB6F8(this, dst, src): erase of 4-byte items (two halfwords) from the array whose end is this[4]: the items from src to the end are copied down to dst one halfword pair at a time, then
// func_08BBB6E8(dst + moved bytes, this[4], &frame) runs and this[4] -= (src - dst) & ~3. v0 = dst.
// Never compared (see the note at its registration): written from the asm only.
void func_08BBB6F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], dst = ctx.gpr[5];
    std::uint32_t src = ctx.gpr[6];
    const std::uint32_t source_start = src;
    GuestFrame frame(ctx, 0x20u);
    const std::int32_t items = signed_of(word(rt, self + 4u) - src) >> 2;
    std::uint32_t at = dst;
    if (items > 0) {
        std::uint32_t done = 0u;
        do {
            const std::uint32_t first = rt.memory().load16(src), second = rt.memory().load16(src + 2u);
            ++done;
            rt.memory().store16(at, static_cast<std::uint16_t>(first));
            src += 4u;
            rt.memory().store16(at + 2u, static_cast<std::uint16_t>(second));
            at += 4u;
        } while (static_cast<std::uint32_t>(items) != done);
        ctx.gpr[7] = done;
    }
    ctx.gpr[4] = at;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08BBB6E8u, {at, end, frame.at(0u)});
    const std::uint32_t bytes = (source_start - dst) & ~3u;
    put(rt, self + 4u, word(rt, self + 4u) - bytes);
    ctx.gpr[2] = dst;
}

// func_08C49B00(this, dst, src): the same for 8-byte items (four halfwords): the items from src to this[4] move down to dst, func_08C470F4(dst + moved bytes, this[4], &frame) runs and this[4] -= (src - dst) & ~7.
// v0 = dst.
// Never compared (see the note at its registration): written from the asm only.
void func_08C49B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], dst = ctx.gpr[5];
    std::uint32_t src = ctx.gpr[6];
    const std::uint32_t source_start = src;
    GuestFrame frame(ctx, 0x20u);
    const std::int32_t items = signed_of(word(rt, self + 4u) - src) >> 3;
    std::uint32_t at = dst;
    if (items > 0) {
        std::uint32_t done = 0u;
        do {
            std::uint32_t part[4];
            for (std::uint32_t i = 0; i < 4u; ++i) part[i] = rt.memory().load16(src + 2u * i);
            ++done;
            for (std::uint32_t i = 0; i < 4u; ++i) rt.memory().store16(at + 2u * i, static_cast<std::uint16_t>(part[i]));
            src += 8u;
            at += 8u;
        } while (static_cast<std::uint32_t>(items) != done);
        ctx.gpr[8] = done;
    }
    ctx.gpr[4] = at;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08C470F4u, {at, end, frame.at(0u)});
    const std::uint32_t bytes = (source_start - dst) & ~7u;
    put(rt, self + 4u, word(rt, self + 4u) - bytes);
    ctx.gpr[2] = dst;
}

// func_08BB3AE4(this, at, value): this = {begin +0, end +4, capacity +8} of 4-byte items. When the array is full (end == capacity) or `at` is not the end, func_08BB35FC(this, at, value) inserts; otherwise the
// word *value is stored at the end (when the end is not null) and the end moves on by 4. v0 = begin (re-read after the call) + ((at - begin) >> 2) * 4.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the append in place (`end != capacity` and `end == at`: 08BB3B28 stores *value at the end, with the null-end case): the array is full in every compared call, so func_08BB35FC always runs.
void func_08BB3AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], at = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t begin = word(rt, self);
    std::uint32_t end = word(rt, self + 4u);
    const std::uint32_t capacity = word(rt, self + 8u);
    const std::uint32_t index = static_cast<std::uint32_t>(signed_of(at - begin) >> 2);
    ctx.gpr[7] = end;
    ctx.gpr[3] = capacity;
    ctx.gpr[9] = begin;
    if (end == capacity || end != at) {
        (void)call_keep(rt, ctx, 0x08BB35FCu, {self, at, value, end});
        begin = word(rt, self);
        ctx.gpr[9] = begin;
    } else {
        std::uint32_t next = end + 4u;
        ctx.gpr[2] = next;
        if (end != 0u) {
            const std::uint32_t item = word(rt, value);
            ctx.gpr[2] = item;
            put(rt, end, item);
            end = word(rt, self + 4u);
            begin = word(rt, self);
            ctx.gpr[7] = end;
            ctx.gpr[9] = begin;
            next = end + 4u;
            ctx.gpr[2] = next;
        }
        put(rt, self + 4u, next);
    }
    ctx.gpr[2] = begin + (index << 2);
}

// func_08A03B64(this): func_08A76ACC(this), this[0] = 0x08CBBE10, the floats this[0x20], this[0x24], this[0x28], this[0x2C] = 1.0f and this[0x30] = -1. v0 = -1.
// Branches the routes do not take: none (a straight line).
void func_08A03B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBBE10u);
    const std::uint32_t grid = self + 0x20u;
    ctx.gpr[3] = grid;
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.fpr[0] = fbits(0x3F800000u);
    put(rt, grid + 0xCu, 0x3F800000u);
    put(rt, grid + 8u, 0x3F800000u);
    put(rt, grid + 4u, 0x3F800000u);
    put(rt, self + 0x20u, 0x3F800000u);
    put(rt, grid + 0x10u, 0xFFFFFFFFu);
}

// func_08947A30(object) -> float: a byte of 7 at object[0] runs func_089475F0(object) first and answers ((F2EA8 + F2EA8) + F2EA4) / 0x3F3C49BA (the floats at 0x08CF2EA8 and 0x08CF2EA4); another kind
// answers 13.5948f (0x41598A02) when the byte at +0x12 is 4, else 0.0f.
// Never compared (see the note at its registration): written from the asm only.
void func_08947A30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t kind = byte_u(rt, object);
    ctx.gpr[3] = kind;
    ctx.gpr[2] = 7u;
    ctx.fpr[0] = fbits(0x41598A02u);
    if (kind == 7u) {
        (void)call_keep(rt, ctx, 0x089475F0u, {object});
        const float first = float_at(rt, 0x08CF2EA8u), second = float_at(rt, 0x08CF2EA4u);
        float sum = first + first;
        sum = sum + second;
        ctx.fpr[0] = sum / fbits(0x3F3C49BAu);
        return;
    }
    const std::uint32_t other = byte_u(rt, object + 0x12u);
    ctx.gpr[3] = other;
    ctx.gpr[2] = 4u;
    if (other != 4u) ctx.fpr[0] = 0.0f;
}

// func_088A6508(this, a1) and func_088A6494(this, a1): func_0892F95C(this[0][0x4C0], K, a1) answers a pair (v0, v1) (K = 0x1A / 0x19); unless both words are -1 the object at this[0] + this[0][8] * 0x180 + 0x38 is handed on
// to func_089348D0(object, 0 when v0 == -1 else K, v0, v1) and the answer is 1; both words -1 give 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_0892F95C answering a pair whose first word is -1 (the `bne` not taken: a1 = 0 for the call) and the pair (-1, -1) (answer 0 without a call); every compared call has a first word other than -1.
void lookup_then_apply(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] = a1;
    ctx.gpr[5] = kind;
    const std::uint32_t owner = word(rt, self);
    ctx.gpr[2] = owner;
    const GuestResult pair = call_keep(rt, ctx, 0x0892F95Cu, {word(rt, owner + 0x4C0u), kind, a1}, {});
    ctx.gpr[4] = 0xFFFFFFFFu;
    ctx.gpr[6] = pair.v0;
    ctx.gpr[7] = pair.v1;
    std::uint32_t result = kind;
    if (pair.v0 == 0xFFFFFFFFu) {
        ctx.gpr[5] = 0u;
        result = 0u;
        if (pair.v1 == 0xFFFFFFFFu) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    const std::uint32_t current = word(rt, self);
    const std::uint32_t slot = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, current + 8u)));
    ctx.gpr[3] = slot;
    const std::uint32_t object = current + (slot << 9) - (slot << 7) + 0x38u;
    ctx.gpr[4] = object;
    ctx.gpr[2] = current;
    (void)call_keep(rt, ctx, 0x089348D0u, {object, result, pair.v0, pair.v1});
    ctx.gpr[5] = 1u;
    ctx.gpr[2] = 1u;
}
void func_088A6508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_then_apply(rt, ctx, 0x1Au); }
void func_088A6494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_then_apply(rt, ctx, 0x19u); }

// func_088D88A8(a0, a1, a2, a3, t0, -, t2): func_088D876C(a0, a1, a2, a3), func_088D8074(a0, t2, a2, a3), then a tail call of func_088D80C0(a0, t0).
// Branches the routes do not take: none (a straight line).
void func_088D88A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t2 = ctx.gpr[10];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_keep(rt, ctx, 0x088D876Cu, {a0});
        ctx.gpr[4] = a0;
        ctx.gpr[5] = t2;
        ctx.gpr[6] = a2;
        ctx.gpr[7] = a3;
        (void)call_keep(rt, ctx, 0x088D8074u, {a0, t2, a2, a3});
        ctx.gpr[4] = a0;
        ctx.gpr[5] = t0;
    }
    tail_call(ctx, 0x088D80C0u, {a0, t0});
}

// func_08B8A9E0(begin, end): for each 24-byte record from begin to end: func_08B8A644(record[8], record[0xC], &frame) runs, then the buffer record[8] (when it is not null) is freed by func_08A5BC54(record[8], size
// rounded down to an even number, 4), size being record[0x10] - record[8]. v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a record whose buffer (record[8]) is null (the `beqz v0` back to 08B8A9FC without the free call).
void func_08B8A9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t record = ctx.gpr[4];
    const std::uint32_t end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    while (record != end) {
        const std::uint32_t buffer = word(rt, record + 8u), second = word(rt, record + 0xCu);
        ctx.gpr[4] = buffer;
        ctx.gpr[5] = second;
        ctx.gpr[6] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08B8A644u, {buffer, second, frame.at(0u)});
        const std::uint32_t again = word(rt, record + 8u);
        std::uint32_t size = word(rt, record + 0x10u);
        record += 0x18u;
        ctx.gpr[6] = 4u;
        size = static_cast<std::uint32_t>(signed_of(size - again) >> 1);
        ctx.gpr[4] = again;
        ctx.gpr[2] = again;
        ctx.gpr[5] = size << 1;
        if (again == 0u) continue;
        (void)call_keep(rt, ctx, 0x08A5BC54u, {again, size << 1, 4u});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08BEE82C(list, key): every node of the circular list whose head is `list` (links at +0, payload at +8) for which func_089FD6E0(key, node[8]) answers non-zero is unlinked by func_08B5C094(node), its
// payload destroyed by func_08A5B9EC(node + 8) and the 12-byte node freed by func_08A5BC54(node, 0xC, 4); the next link is read before the test. v0 = the head (an empty list leaves with list[0], the head
// itself).
// Never compared (see the note at its registration): written from the asm only.
void func_08BEE82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = word(rt, head);
    ctx.gpr[2] = first;
    if (first == head) return;
    std::uint32_t node = first, next;
    do {
        ctx.gpr[5] = word(rt, node + 8u);
        ctx.gpr[4] = key;
        next = word(rt, node);
        if (call_keep(rt, ctx, 0x089FD6E0u, {key, word(rt, node + 8u)}) != 0u) {
            ctx.gpr[4] = node;
            (void)call_keep(rt, ctx, 0x08B5C094u, {node});
            ctx.gpr[4] = node + 8u;
            (void)call_keep(rt, ctx, 0x08A5B9ECu, {node + 8u});
            ctx.gpr[4] = node;
            ctx.gpr[5] = 0xCu;
            ctx.gpr[6] = 4u;
            (void)call_keep(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        }
        node = next;
        ctx.gpr[2] = next;
    } while (head != next);
}

// func_08C203C8(this, a1, a2): three temporaries on the stack: func_08A76ABC(tmp0, a2), func_08BF7C60(tmp1, tmp0), func_08A758BC(tmp2, a1, this, tmp1), func_08A7C5C8(this, tmp2), and the destructors
// func_08A5B9EC(tmp2), func_08A5B9EC(tmp1), func_08A5B9EC(tmp0). v0 = the last call's.
// Branches the routes do not take: none (a straight line).
void func_08C203C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t tmp0 = frame.at(0u), tmp1 = frame.at(4u), tmp2 = frame.at(8u);
    ctx.gpr[4] = tmp0;
    ctx.gpr[5] = a2;
    (void)call_keep(rt, ctx, 0x08A76ABCu, {tmp0, a2});
    ctx.gpr[4] = tmp1;
    ctx.gpr[5] = tmp0;
    (void)call_keep(rt, ctx, 0x08BF7C60u, {tmp1, tmp0});
    ctx.gpr[6] = self;
    ctx.gpr[7] = tmp1;
    ctx.gpr[5] = a1;
    ctx.gpr[4] = tmp2;
    (void)call_keep(rt, ctx, 0x08A758BCu, {tmp2, a1, self, tmp1});
    ctx.gpr[5] = tmp2;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08A7C5C8u, {self, tmp2});
    ctx.gpr[4] = tmp2;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {tmp2});
    ctx.gpr[4] = tmp1;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {tmp1});
    ctx.gpr[4] = tmp0;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {tmp0});
}

// func_08B5D1E0(this, other): a reference-counted pointer assignment: func_08B6085C(*other, 1) takes a reference on the new object, func_08B60804(old, -1) drops one from the old (old = *this); when that answers 1
// and old is not null the old object is released (func_08B5CC1C(old), func_08B64654(old)); *this = *other. v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B60804 answering 1 for a non-null old object (the release by func_08B5CC1C + func_08B64654); the answer is never 1 in the compared calls.
void func_08B5D1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 1u;
    (void)call_keep(rt, ctx, 0x08B6085Cu, {word(rt, other), 1u});
    const std::uint32_t old = word(rt, self);
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = old;
    const std::uint32_t answer = call_keep(rt, ctx, 0x08B60804u, {old, 0xFFFFFFFFu});
    ctx.gpr[3] = 1u;
    if (answer == 1u && old != 0u) {
        ctx.gpr[4] = old;
        (void)call_keep(rt, ctx, 0x08B5CC1Cu, {old});
        ctx.gpr[4] = old;
        (void)call_keep(rt, ctx, 0x08B64654u, {old});
    }
    const std::uint32_t pointer = word(rt, other);
    ctx.gpr[3] = pointer;
    put(rt, self, pointer);
    ctx.gpr[2] = self;
}

// func_08AD04AC(this): fills a settings record: this[0] = -2, words +0x420, +0x424, +0x428 = 1, float +0x42C = 4.0f, bytes +0x10 and +0xC = 1, word +8 = 0x20, bytes +4, +6, +0xD, +0xE, +0xF = 0, words
// +0x414, +0x418, +0x41C, +0x430 = 0; func_08B2DE38(tmp, 0x20) and func_08B37640(this + 0x11, 0x08CB49E0, tmp) fill the name buffer; five bytes from 0x08CB4A00 are copied to this + 0x211. v0 = the
// first four of those bytes as a word, v1 = the fifth.
// Branches the routes do not take: none (a straight line).
void func_08AD04AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    put(rt, self, 0xFFFFFFFEu);
    put(rt, self + 0x428u, 1u);
    put(rt, self + 0x420u, 1u);
    put(rt, self + 0x424u, 1u);
    rt.memory().store8(self + 0x10u, 1u);
    put_float(rt, self + 0x42Cu, fbits(0x40800000u));
    rt.memory().store8(self + 0xCu, 1u);
    put(rt, self + 8u, 0x20u);
    rt.memory().store8(self + 4u, 0u);
    rt.memory().store8(self + 6u, 0u);
    rt.memory().store8(self + 0xDu, 0u);
    rt.memory().store8(self + 0xEu, 0u);
    rt.memory().store8(self + 0xFu, 0u);
    put(rt, self + 0x414u, 0u);
    put(rt, self + 0x418u, 0u);
    put(rt, self + 0x41Cu, 0u);
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = 0x20u;
    ctx.gpr[6] = 1u;
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 0x20u;
    put(rt, self + 0x430u, 0u);
    (void)call_keep(rt, ctx, 0x08B2DE38u, {frame.at(0u), 0x20u});
    ctx.gpr[5] = 0x08CB49E0u;
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B37640u, {self + 0x11u, 0x08CB49E0u, frame.at(0u)});
    constexpr std::uint32_t source = 0x08CB4A00u;
    std::uint32_t four = 0u;
    for (std::uint32_t i = 0; i < 4u; ++i) four |= static_cast<std::uint32_t>(rt.memory().load8(source + i)) << (8u * i);
    for (std::uint32_t i = 0; i < 4u; ++i) rt.memory().store8(self + 0x211u + i, static_cast<std::uint8_t>(four >> (8u * i)));
    const std::uint32_t fifth = rt.memory().load8(source + 4u);
    rt.memory().store8(self + 0x211u + 4u, static_cast<std::uint8_t>(fifth));
    ctx.gpr[5] = source;
    ctx.gpr[4] = self + 0x211u;
    ctx.gpr[3] = fifth;
    ctx.gpr[2] = four;
}

// func_08B7DCA4(a0, a1): a tail call of func_08B7DCE4(a0, a1, func_08B796C4() + 0x5C, 1). (The code after its `j` is another entry point of the same blob.)
// Branches the routes do not take: none (a straight line).
void func_08B7DCA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    std::uint32_t reentrant;
    {
        GuestFrame frame(ctx, 0x10u);
        reentrant = call_keep(rt, ctx, 0x08B796C4u, {a0, a1});
    }
    tail_call(ctx, 0x08B7DCE4u, {a0, a1, reentrant + 0x5Cu, 1u});
}

// func_088A79AC(out, kind): out[0] = a new 0x4D8-byte object: kind (a byte) != 0 builds it with func_08B2B3EC(0x4D8) + func_088A73E4 and gives it vtable 0x08CC2BE8, else func_08B62344(0x4D8) +
// func_088A7310; then a function-local static (guard byte 0x08D75F28, object 0x08D75F30) is built on first use: when the guard byte is 0 and func_08B6441C(guard) lets this caller build it, the static gets
// vtable 0x08CC2C00 and func_08B64494(guard) releases the guard; out[4] = the static's address; a tail call of func_088A719C(out). v0 as the last instructions leave it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): kind != 0 (func_08B2B3EC + func_088A73E4 and the vtable 0x08CC2BE8: the compared calls all use func_08B62344 + func_088A7310) and func_08B6441C answering 0 (the fall to 088A7A2C); the guard-set path is taken by some calls.
void func_088A79AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08D75F28u, object = 0x08D75F30u;
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t kind = ctx.gpr[5] & 0xFFu;
    ctx.gpr[5] = kind;
    {
        GuestFrame frame(ctx, 0x10u);
        std::uint32_t block;
        if (kind != 0u) {
            ctx.gpr[4] = 0x4D8u;
            const std::uint32_t raw = call_keep(rt, ctx, 0x08B2B3ECu, {0x4D8u});
            ctx.gpr[4] = raw;
            block = raw;
            (void)call_keep(rt, ctx, 0x088A73E4u, {raw});
            ctx.gpr[2] = 0x08CC2BE8u;
            put(rt, block, 0x08CC2BE8u);
        } else {
            ctx.gpr[4] = 0x4D8u;
            const std::uint32_t raw = call_keep(rt, ctx, 0x08B62344u, {0x4D8u});
            ctx.gpr[4] = raw;
            block = raw;
            (void)call_keep(rt, ctx, 0x088A7310u, {raw});
        }
        ctx.gpr[3] = 0x08D70000u;
        put(rt, out, block);
        const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        ctx.gpr[2] = flag;
        if (flag == 0u) {
            ctx.gpr[4] = guard;
            if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                ctx.gpr[3] = 0x08D70000u;
                ctx.gpr[2] = 0x08CC2C00u;
                put(rt, object, 0x08CC2C00u);
                ctx.gpr[4] = guard;
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            } else {
                ctx.gpr[2] = 0x08D70000u;
            }
        } else {
            ctx.gpr[2] = 0x08D70000u;
        }
        put(rt, out + 4u, object);
        ctx.gpr[4] = out;
    }
    tail_call(ctx, 0x088A719Cu, {out});
}

// func_08A4D508(): the address of a lazily built singleton: the word at 0x08CF68E8 is returned (v0) when it is set; otherwise a 0xA8-byte block (func_08B62344) is initialised by func_08A4D420, stored in
// the word and func_08A4CD1C(block) runs; then func_08A4DDB8(*0x08CF68E8, 0x08CAF14C) as a tail call (the result of that call is v0).
// Never compared (see the note at its registration): written from the asm only.
void func_08A4D508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t slot = 0x08CF68E8u;
    std::uint32_t current = word(rt, slot);
    ctx.gpr[2] = current;
    if (current != 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = 0xA8u;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0xA8u});
        ctx.gpr[4] = block;
        (void)call_keep(rt, ctx, 0x08A4D420u, {block});
        ctx.gpr[4] = block;
        put(rt, slot, block);
        (void)call_keep(rt, ctx, 0x08A4CD1Cu, {block});
        current = word(rt, slot);
        ctx.gpr[4] = current;
        ctx.gpr[5] = 0x08CAF14Cu;
    }
    tail_call(ctx, 0x08A4DDB8u, {current, 0x08CAF14Cu});
}

// func_08AD5460(this, a1): func_08AD5394(this), then func_08B10204(tmp, a1, 1) fills a 0x3C-byte record on the stack (at +0x40), which is copied to +0 (16 bytes at a time, then the last 12). A record whose
// first word is not 0 answers 0; otherwise this[0xC] (byte) = 1, this[8] = record[0x10] and this[4] = record[0x20], and the answer is 1.
// Never compared (see the note at its registration): written from the asm only.
void func_08AD5460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t scratch = frame.at(0x40u), record = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08AD5394u, {self});
    ctx.gpr[6] = 1u;
    ctx.gpr[5] = a1;
    ctx.gpr[4] = scratch;
    (void)call_keep(rt, ctx, 0x08B10204u, {scratch, a1, 1u});
    for (std::uint32_t round = 0; round < 3u; ++round) {
        const std::uint32_t from = scratch + round * 16u, to = record + round * 16u;
        const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 12u);
        put(rt, to, w0);
        put(rt, to + 4u, w1);
        put(rt, to + 8u, w2);
        put(rt, to + 12u, w3);
    }
    {
        const std::uint32_t from = scratch + 0x30u, to = record + 0x30u;
        const std::uint32_t w2 = word(rt, from + 8u), w0 = word(rt, from), w1 = word(rt, from + 4u);
        put(rt, to + 8u, w2);
        put(rt, to, w0);
        put(rt, to + 4u, w1);
    }
    std::uint32_t answer = 0u;
    if (word(rt, record) == 0u) {
        const std::uint32_t at_0x20 = word(rt, record + 0x20u), at_0x10 = word(rt, record + 0x10u);
        rt.memory().store8(self + 0xCu, 1u);
        answer = 1u;
        put(rt, self + 8u, at_0x10);
        put(rt, self + 4u, at_0x20);
    }
    ctx.gpr[5] = answer;
    ctx.gpr[2] = answer;
}

// func_08B32228(a0): v0 = 1 at once when the byte at 0x08D6916C is set; otherwise func_08B2EEB8(0x08D69124) runs and func_08B321C8(a0) answers: non-zero runs func_08B2EEB8(0x08D69110) and answers 1,
// zero runs func_08B2EE2C(0x08D69124) and answers 0.
// Never compared (see the note at its registration): written from the asm only.
void func_08B32228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t flag = byte_u(rt, 0x08D6916Cu);
    ctx.gpr[3] = flag;
    ctx.gpr[4] = 0x08D69124u;
    if (flag != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = 1u;
    (void)call_keep(rt, ctx, 0x08B2EEB8u, {0x08D69124u});
    ctx.gpr[4] = a0;
    if (call_keep(rt, ctx, 0x08B321C8u, {a0}) != 0u) {
        ctx.gpr[4] = 0x08D69110u;
        (void)call_keep(rt, ctx, 0x08B2EEB8u, {0x08D69110u});
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[4] = 0x08D69124u;
    (void)call_keep(rt, ctx, 0x08B2EE2Cu, {0x08D69124u});
    ctx.gpr[2] = 0u;
}

// func_08B3FF98(this): resets a state record: this[0x93C] = -1, [0x910..0x92C] = 0 (eight words), [0xC0] = -1, bytes [0xAD], [0xB6]..[0xBA], [0x936] = 0, words [0xB0], [0xBC], [0xC4], [0x930], [0x940], [0x944],
// [0x988], [0x98C] = 0, halfwords [0xB4], [0x934], [0x938] = 0; func_08B3EAE4(this + 0x154), memset(this + 0x454, 0, 0x200), memset(this + 0x654, 0, 0x200), memset(this + 0x854, 0, 0x80),
// this[0x8D8] = 0 and a tail call of func_08B3EBFC(this + 0x8DC). v0 = the last memset's.
// Branches the routes do not take: none (a straight line).
void func_08B3FF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[5] = 0u;
        put(rt, self + 0x93Cu, 0xFFFFFFFFu);
        for (const std::uint32_t offset : {0x910u, 0x914u, 0x918u, 0x91Cu, 0x920u, 0x924u, 0x928u, 0x92Cu}) put(rt, self + offset, 0u);
        ctx.gpr[4] = self + 0x154u;
        put(rt, self + 0xC0u, 0xFFFFFFFFu);
        rt.memory().store8(self + 0xADu, 0u);
        put(rt, self + 0xB0u, 0u);
        rt.memory().store16(self + 0xB4u, 0u);
        for (const std::uint32_t offset : {0xB6u, 0xB7u, 0xB8u, 0xB9u, 0xBAu}) rt.memory().store8(self + offset, 0u);
        put(rt, self + 0xBCu, 0u);
        put(rt, self + 0xC4u, 0u);
        put(rt, self + 0x930u, 0u);
        rt.memory().store16(self + 0x934u, 0u);
        rt.memory().store8(self + 0x936u, 0u);
        rt.memory().store16(self + 0x938u, 0u);
        put(rt, self + 0x940u, 0u);
        put(rt, self + 0x944u, 0u);
        put(rt, self + 0x988u, 0u);
        put(rt, self + 0x98Cu, 0u);
        (void)call_keep(rt, ctx, 0x08B3EAE4u, {self + 0x154u});
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = 0x200u;
        (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x454u, 0u, 0x200u});
        (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x654u, 0u, 0x200u});
        (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x854u, 0u, 0x80u});
        put(rt, self + 0x454u + 0x484u, 0u);
        ctx.gpr[4] = self + 0x8DCu;
    }
    tail_call(ctx, 0x08B3EBFCu, {self + 0x8DCu});
}

// func_08B64CD0(this, a1, a2, a3): this[0] = 0x08CDC170, this[4] = (a3 != 0), the byte this[0xC] = (a1 != 0 && (a2 & 0xFF) != 0), this[0x10] = this[0x14] = 0, this[0x18] = a1 or, for a1 == 0,
// func_08B64CC0(); memset(this + 0x1D, 0, 0x100), this[0x1C] = 0, memset(this + 0x11D, 0, 0x100), this[0x21D] = 0. v0 = the last memset's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a1 != 0 (this[0x18] = a1 and the flag byte = (a2 & 0xFF) != 0: the `beqz a1` not taken) and a3 == 0 (this[4] = 0); every compared call has a1 == 0 (this[0x18] = func_08B64CC0(), flag byte 0) and a3 != 0.
void func_08B64CD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t a3 = ctx.gpr[7] != 0u ? 1u : 0u;
    ctx.gpr[7] = a3;
    ctx.gpr[6] = flag;
    ctx.gpr[3] = 1u;
    put(rt, self + 4u, a3);
    put(rt, self, 0x08CDC170u);
    const std::uint32_t enabled = (a1 != 0u && flag != 0u) ? 1u : 0u;
    ctx.gpr[3] = enabled;
    rt.memory().store8(self + 0xCu, static_cast<std::uint8_t>(enabled));
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    std::uint32_t handle = a1;
    if (a1 == 0u) {
        handle = call_keep(rt, ctx, 0x08B64CC0u, {});
        ctx.gpr[5] = handle;
    }
    put(rt, self + 0x18u, handle);
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0x100u;
    (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x1Du, 0u, 0x100u});
    rt.memory().store8(self + 0x1Cu, 0u);
    (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x11Du, 0u, 0x100u});
    rt.memory().store8(self + 0x21Du, 0u);
}

// func_08A62D08(this, value): a 12-byte node (func_08A5BC20(0xC, 4)) = { +0 = 0x08CD3660, +4 = value, +8 = this[0x3C][0x1C] } is appended to the pointer array at this + 0x24 {begin +0, end +4, capacity
// +8}: a full array grows through func_08C22ED8(array, end, &node), otherwise the node pointer is stored at the end (when it is not null) and the end moves on by 4; then func_08A615D4(this, 3).
// v0 = the number of entries before the append.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a full array (the `beq a3, v1` to 08A62D98: func_08C22ED8 grows it) and a null end pointer (`beql a3, zero`); every compared call stores at a non-null end below the capacity.
void func_08A62D08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t owner = word(rt, self + 0x3Cu);
    ctx.gpr[2] = owner;
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0xCu;
    const std::uint32_t shared = word(rt, owner + 0x1Cu);
    const std::uint32_t node = call_keep(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
    put(rt, node + 4u, value);
    const std::uint32_t array = self + 0x24u;
    put(rt, node, 0x08CD3660u);
    put(rt, node + 8u, shared);
    std::uint32_t end = word(rt, array + 4u);
    const std::uint32_t begin = word(rt, self + 0x24u), capacity = word(rt, array + 8u);
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    put(rt, frame.at(0u), node);
    ctx.gpr[4] = array;
    ctx.gpr[5] = end;
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[7] = end;
    ctx.gpr[8] = array;
    ctx.gpr[9] = node;
    if (end == capacity) {
        (void)call_keep(rt, ctx, 0x08C22ED8u, {array, end, frame.at(0u)});
    } else {
        if (end != 0u) {
            put(rt, end, node);
            end = word(rt, array + 4u);
        }
        put(rt, array + 4u, end + 4u);
    }
    ctx.gpr[4] = self;
    ctx.gpr[5] = 3u;
    (void)call_keep(rt, ctx, 0x08A615D4u, {self, 3u});
    ctx.gpr[2] = count;
}

// func_08ADDE30(this): sets up a pool at 0x08D40674: memset(pool, 0, 0x2BC); func_08AD8960(this, 0x08C9100C, &count, 0x20) answers the number of blocks; pool[0x2C] = pool[0x30] = 0x1000 and 32 slots
// (+0x34, +0x38 = 0x1000, +0x134 = 0, +0x138 = count - 1, 8 bytes apart); pool[0x28] = count; a buffer func_08AD84B4(count << 8, 1) goes to pool[0x24]; without one the answer is 0; otherwise two list heads
// (pool + 4 and pool + 0x14) are made empty and func_08AD8C18(pool + 0x14, buffer, count, 0x100) fills the free list, pool[0] = 1 and the answer is 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08AD84B4 answering 0 (the `beqz` to 08ADDF20: v0 = 0 before the two list heads and func_08AD8C18); every compared call gets a buffer.
void func_08ADDE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t pool = 0x08D40674u;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0x2BCu;
    (void)call_keep(rt, ctx, 0x08B36018u, {pool, 0u, 0x2BCu});
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0x08C9100Cu;
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[7] = 0x20u;
    (void)call_keep(rt, ctx, 0x08AD8960u, {self, 0x08C9100Cu, frame.at(0u), 0x20u});
    const std::uint32_t count = word(rt, frame.at(0u));
    put(rt, pool + 0x30u, 0x1000u);
    put(rt, pool + 0x2Cu, 0x1000u);
    for (std::uint32_t slot = 0; slot < 32u; ++slot) {
        put(rt, pool + slot * 8u + 0x34u, 0x1000u);
        put(rt, pool + slot * 8u + 0x38u, 0x1000u);
        put(rt, pool + slot * 8u + 0x134u, 0u);
        put(rt, pool + slot * 8u + 0x138u, count - 1u);
    }
    put(rt, pool + 0x28u, count);
    ctx.gpr[4] = count << 8;
    ctx.gpr[5] = 1u;
    const std::uint32_t buffer = call_keep(rt, ctx, 0x08AD84B4u, {count << 8, 1u});
    put(rt, pool + 0x24u, buffer);
    ctx.gpr[3] = 0u;
    if (buffer == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t head_a = pool + 4u, head_b = pool + 0x14u;
    ctx.gpr[8] = 0xFFFFFFFFu;
    ctx.gpr[5] = buffer;
    const std::uint32_t pool_count = word(rt, pool + 0x28u);
    put(rt, pool + 4u, head_a);
    ctx.gpr[4] = head_b;
    ctx.gpr[6] = pool_count;
    ctx.gpr[7] = 0x100u;
    put(rt, head_a + 0xCu, 0u);
    put(rt, head_b + 8u, 0xFFFFFFFFu);
    put(rt, head_a + 4u, head_a);
    put(rt, head_a + 8u, 0xFFFFFFFFu);
    put(rt, pool + 0x14u, head_b);
    put(rt, head_b + 4u, head_b);
    ctx.gpr[3] = head_b;
    put(rt, head_b + 0xCu, 0u);
    (void)call_keep(rt, ctx, 0x08AD8C18u, {head_b, buffer, pool_count, 0x100u});
    put(rt, pool, 1u);
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

}  // namespace
}  // namespace hot_batch_85
}  // namespace gt::game

#define HOT85(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_85::name, gt::ReturnKind::kind)

// Not registered: every case reaches a wait (kernel isolation: sleep would block): compared 0, not comparable in all cases.
// HOT85(0x08B1C590u, func_08B1C590, DoubleWord);
HOT85(0x089571E0u, func_089571E0, Word);
HOT85(0x08958354u, func_08958354, Word);
HOT85(0x08954F2Cu, func_08954F2C, Word);
HOT85(0x0894F438u, func_0894F438, Word);
HOT85(0x08C48120u, func_08C48120, Word);
HOT85(0x08BD3F30u, func_08BD3F30, Word);
HOT85(0x08BD3650u, func_08BD3650, Word);
HOT85(0x08B9B6E0u, func_08B9B6E0, Word);
HOT85(0x08B3A410u, func_08B3A410, Word);
HOT85(0x08BD2CB8u, func_08BD2CB8, Word);
HOT85(0x08B52D70u, func_08B52D70, Word);
HOT85(0x08936268u, func_08936268, Word);
// Not registered: every compared case swaps two empty trees (no memory changed).
// HOT85(0x08C1CE44u, func_08C1CE44, Word);
HOT85(0x08C40554u, func_08C40554, Word);
HOT85(0x08BF933Cu, func_08BF933C, DoubleWord);
HOT85(0x088FA158u, func_088FA158, Word);
HOT85(0x088F9970u, func_088F9970, Word);
HOT85(0x088744FCu, func_088744FC, Word);
HOT85(0x08884420u, func_08884420, Word);
HOT85(0x08AD07B8u, func_08AD07B8, Word);
HOT85(0x0886FC04u, func_0886FC04, Word);
HOT85(0x088691D8u, func_088691D8, Word);
HOT85(0x08AF4408u, func_08AF4408, Word);
HOT85(0x088A3FF0u, func_088A3FF0, Word);
HOT85(0x08B7E518u, func_08B7E518, Word);
HOT85(0x08BD96E0u, func_08BD96E0, Word);
HOT85(0x08C46DDCu, func_08C46DDC, Word);
HOT85(0x08C40C90u, func_08C40C90, Word);
HOT85(0x08C40B9Cu, func_08C40B9C, Word);
HOT85(0x08B1AEACu, func_08B1AEAC, Word);
HOT85(0x08C4C9DCu, func_08C4C9DC, Word);
HOT85(0x08B2A594u, func_08B2A594, Word);
HOT85(0x08B93F38u, func_08B93F38, Word);
// Not registered: every compared range was shorter than 0x88 bytes (only the tail call to func_08BDA574 ran; the insertion loop never did).
// HOT85(0x08BDA64Cu, func_08BDA64C, Word);
HOT85(0x08AD97D8u, func_08AD97D8, Word);
// Not registered: the compared cases erase at the end of the array (nothing moves, no memory changed).
// HOT85(0x08BBB6F8u, func_08BBB6F8, Word);
// Not registered: the compared cases erase at the end of the array (nothing moves, no memory changed).
// HOT85(0x08C49B00u, func_08C49B00, Word);
HOT85(0x08BB3AE4u, func_08BB3AE4, Word);
HOT85(0x08A03B64u, func_08A03B64, Word);
// Not registered: every compared object has a kind other than 7 (the formula after func_089475F0 never ran).
// HOT85(0x08947A30u, func_08947A30, Float);
HOT85(0x088A6508u, func_088A6508, Word);
HOT85(0x088A6494u, func_088A6494, Word);
HOT85(0x088D88A8u, func_088D88A8, Word);
HOT85(0x08B8A9E0u, func_08B8A9E0, Word);
// Not registered: every compared call had an empty list (the loop that tests, unlinks and frees nodes never ran).
// HOT85(0x08BEE82Cu, func_08BEE82C, Word);
HOT85(0x08C203C8u, func_08C203C8, Word);
HOT85(0x08B5D1E0u, func_08B5D1E0, Word);
HOT85(0x08AD04ACu, func_08AD04AC, Word);
HOT85(0x08B7DCA4u, func_08B7DCA4, Word);
HOT85(0x088A79ACu, func_088A79AC, Word);
// Not registered: its one call per route is not comparable (import stub 0x08C7A900, ModuleMgrForUser).
// HOT85(0x08A4D508u, func_08A4D508, Word);
// Not registered: its two calls per route are not comparable (kernel isolation: sleep would block).
// HOT85(0x08AD5460u, func_08AD5460, Word);
// Not registered: its one call per route is not comparable (import stub 0x08C7B168, sceUtility).
// HOT85(0x08B32228u, func_08B32228, Word);
HOT85(0x08B3FF98u, func_08B3FF98, Word);
HOT85(0x08B64CD0u, func_08B64CD0, Word);
HOT85(0x08A62D08u, func_08A62D08, Word);
HOT85(0x08ADDE30u, func_08ADDE30, Word);
