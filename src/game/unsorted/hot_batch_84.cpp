// Hot batch 84: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off;
// batch 83 = PR #204 excluded with --exclude-ref origin/decomp/hot-batch-83, and no address that #204 writes is taken here). Before reading any function its address was searched for in src/ and in the branch of batch 83.
// The pool of hot singles is nearly drained (the first 80 functions by calls are done, written-and-unregistered, import stubs or VFPU), so most of this batch is the next tier: functions the routes call a few
// times each. Candidates without VFPU, without imports that the harness cannot replay, without variadic spills and without multi-entry blobs; each was read by hand word by word with `tools/disasm.py` (branch
// targets and relocations resolved by the tool, every delay slot part of its branch's path).
//
//  - the menu-object cluster (func_0880B5E8 .. func_0880BF70, func_0886xxxx, func_0887xxxx): constructors, a locked setter chain (func_0880BA98 -> func_0886978C -> func_0886E008 -> func_0886AA0C), a tree insert;
//  - strings: operator+ (func_08BEEED0), find (func_08C1C89C), find_first_not_of (func_08C24530), the allocator (func_08B644BC), guarded static holders (func_08894444, func_08893F7C);
//  - small floats and vectors (func_08C46508, func_08BAE7A0, func_08B6E4FC, func_08B6E268, func_08966BF0, func_08966E24, func_08870870), destructors and holders (func_08AA96B4, func_08A8A464, func_08C283A8, ...).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest
// copies from ctx. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's; a callee that sets no v0 hands the previous one on).
//
// Written but not registered (the commented HOT84 lines say why): func_08881C38 (its tail call reaches the Void-registered, nested func_088A599C = CarParameter::set_color, whose decompiled run leaves a different v0: the full gate mismatched on v0), func_08B6E4FC, func_08B6E268, func_0887BC90, func_08875038 and func_0886C31C were compared 0 times (the last five) in the fast gate (their callers did not run in the
// routes' window); func_089C4C20 stops every case at a blocking import; func_08A1F520 (only op 0) and func_08BDD7D8 (only the empty range) were compared on their early exit only, so their bodies are unverified.
// Compared on a side path only, kept registered (their comments list what was read from the asm only): func_0886E008 (flag != 0 side) and func_08BF8A7C (answer 1 only).
// s0 / s1 are loaded with the original's values around calls in func_0880BF70, func_08869988 and func_08B1BC98: a callee
// (the lock created by func_08B28DCC) stores the caller's s0, which the harness saw as 306 bytes of RAM (FINDINGS).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_84 {
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
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// lwl / lwr pair at `address` + 3 / + 0: the little-endian word that starts at an unaligned address.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return static_cast<std::uint32_t>(rt.memory().load8(address)) | static_cast<std::uint32_t>(rt.memory().load8(address + 1u)) << 8 |
           static_cast<std::uint32_t>(rt.memory().load8(address + 2u)) << 16 | static_cast<std::uint32_t>(rt.memory().load8(address + 3u)) << 24;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// func_08AF219C(this, a1): the graphics state is switched around a draw: func_08B56A00(1), func_08B569A0(), func_08B567F4(frame) fill a 0x40-byte block, whose float at +0x38 is multiplied by this's float at
// +0x20 (f1 = this[0x20], f0 = frame[0x38], f0 = f0 * f1) and func_08B568DC(frame) takes it back; func_08B56A00(0); func_08AF22A4(this, a1); func_08B56A00(1); func_08B56940(); func_08B56A00(0); and when the
// byte this[0x24] is non-zero func_08B58218(4). v0 is that last call's, or the byte (0).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08AF219C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[4] = 1u;
    (void)call_keep(rt, ctx, 0x08B56A00u, {1u});
    (void)call_keep(rt, ctx, 0x08B569A0u, {1u});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B567F4u, {frame.at(0u)});
    const float scale = float_at(rt, self + 0x20u);
    const float value = float_at(rt, frame.at(0x38u));
    ctx.fpr[1] = scale;
    ctx.fpr[0] = psp_mul(value, scale);
    put_float(rt, frame.at(0x38u), ctx.fpr[0]);
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B568DCu, {frame.at(0u)});
    ctx.gpr[4] = 0u;
    (void)call_keep(rt, ctx, 0x08B56A00u, {0u});
    ctx.gpr[5] = a1;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08AF22A4u, {self, a1});
    ctx.gpr[4] = 1u;
    (void)call_keep(rt, ctx, 0x08B56A00u, {1u});
    (void)call_keep(rt, ctx, 0x08B56940u, {1u});
    ctx.gpr[4] = 0u;
    (void)call_keep(rt, ctx, 0x08B56A00u, {0u});
    ctx.gpr[2] = rt.memory().load8(self + 0x24u);
    ctx.gpr[4] = 4u;
    if (ctx.gpr[2] != 0u) (void)call_keep(rt, ctx, 0x08B58218u, {4u});
}

// func_08A8A464(this, out): the child c = this[0x18] (null: func_08A5BB7C(D_08CB1B8C) runs first, then the original goes on and reads through the null pointer) answers through its vtable slot 0x70
// (holder at the frame, c, this + 0x1C); unless `out` is that holder func_08A5BAD8(out, holder[0]) copies it; func_08A5B9EC(holder) destroys the holder. v0 is that last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the child is never null (that path reads through address 0 right after func_08A5BB7C) and `out` is never the holder (the copy always runs).
void func_08A8A464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t child = word(rt, self + 0x18u);
    ctx.gpr[4] = 0x08CB1B8Cu;
    if (child == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB1B8Cu});
    const std::uint32_t vtable = word(rt, child);
    const std::uint32_t function = word(rt, vtable + 0x70u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = child;
    ctx.gpr[6] = self + 0x1Cu;
    (void)call_keep(rt, ctx, function, {frame.at(0u), child, self + 0x1Cu});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) {
        ctx.gpr[5] = word(rt, frame.at(0u));
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, ctx.gpr[5]});
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_0886A8B8(this, index, value): this[index + 4] (word offset 0x10 + index * 4) = value, then for the eight children this[0x24 + 4 * i]: a non-null child c gets its vtable slot 0x14
// called with (c, index, value). v0 / v1 as the last iteration leaves them (the child, or the call's).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0886A8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], value = ctx.gpr[6];
    ctx.gpr[2] = self + (index << 2);
    put(rt, self + (index << 2) + 0x10u, value);
    for (std::uint32_t i = 0; i < 8u; ++i) {
        const std::uint32_t child = word(rt, self + 0x24u + (i << 2));
        ctx.gpr[2] = child;
        ctx.gpr[4] = child;
        ctx.gpr[5] = index;
        ctx.gpr[6] = value;
        if (child != 0u) {
            const std::uint32_t vtable = word(rt, child);
            const std::uint32_t function = word(rt, vtable + 0x14u);
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {child, index, value});
        }
    }
}

// func_08BEEED0(out, text, other): out is built as a string: n = func_08B35F60(text); out[0] = D_08D74FD0 + 0xC; func_08B864EC(out, n + other's representation length (the word before *other)),
// func_08BEED1C(out, text, n) and func_08B89054(out, other) append; v0 = out.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08BEEED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], text = ctx.gpr[5], other = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = text;
    const std::uint32_t length = call_keep(rt, ctx, 0x08B35F60u, {text});
    put(rt, out, 0x08D74FD0u + 0xCu);
    ctx.gpr[2] = word(rt, other);
    const std::uint32_t total = length + word(rt, ctx.gpr[2] - 0xCu);
    ctx.gpr[4] = out;
    ctx.gpr[5] = total;
    (void)call_keep(rt, ctx, 0x08B864ECu, {out, total});
    ctx.gpr[4] = out;
    ctx.gpr[5] = text;
    ctx.gpr[6] = length;
    (void)call_keep(rt, ctx, 0x08BEED1Cu, {out, text, length});
    ctx.gpr[4] = out;
    ctx.gpr[5] = other;
    (void)call_keep(rt, ctx, 0x08B89054u, {out, other});
    ctx.gpr[2] = out;
}

// func_08966BF0(this, f12, f13): when func_08966174(this) answers non-zero, the object this[0x80] gets the float f12 at +0x10 and f13 at +0x14, each clamped: a value that is not >= 0 (a negative or a NaN) is stored as 0.0.
// v0 = this[0x80] as the last load leaves it (0 when func_08966174 answered 0), f0 = 0.0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08966174 always answers non-zero (the early exit with v0 = 0 is not taken) and both floats are always >= 0 (the clamp to 0.0 for a negative or NaN value is not taken).
void func_08966BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float first = ctx.fpr[12], second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x20u);
    if (call_keep(rt, ctx, 0x08966174u, {self}) == 0u) return;
    ctx.fpr[0] = 0.0f;
    ctx.gpr[2] = word(rt, self + 0x80u);
    put_float(rt, ctx.gpr[2] + 0x10u, 0.0f <= first ? first : 0.0f);
    ctx.fpr[0] = 0.0f;
    ctx.gpr[2] = word(rt, self + 0x80u);
    put_float(rt, ctx.gpr[2] + 0x14u, 0.0f <= second ? second : 0.0f);
}

// func_0886BF44(this, name): walks the list this[1][1] (node = {object, -, next}): for each node (object s0, next s1) the object is removed by func_0886AB7C(s0) unless a name is given and func_08B7B8C4(s0's vtable
// slot 0xC answer, name) answers non-zero (a different name). v0 = 0 on the way out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a name is always given (the remove-everything path of a null name is not taken).
void func_0886BF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], name = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t node = word(rt, word(rt, self + 4u) + 4u);
    ctx.gpr[2] = node;
    if (node == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    for (;;) {
        const std::uint32_t object = word(rt, node);
        const std::uint32_t next = word(rt, node + 8u);
        bool remove = true;
        if (name != 0u) {
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 0xCu);
            ctx.gpr[3] = vtable;
            ctx.gpr[4] = object;
            const std::uint32_t own = call_keep(rt, ctx, function, {object});
            ctx.gpr[4] = own;
            ctx.gpr[5] = name;
            remove = call_keep(rt, ctx, 0x08B7B8C4u, {own, name}) == 0u;
        }
        if (remove) {
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x0886AB7Cu, {object});
        }
        ctx.gpr[2] = next;
        if (next == 0u) break;
        node = next;
    }
    ctx.gpr[2] = 0u;
}

// func_08B8E0D8(this): this[0] = D_08CBEDF8; a child c = this[0x38], when set, gets its vtable slot 4 called with c; this[0] = D_08CB7440 is stored before func_08A7D4E4(this) and the tail call is func_08A5BC54(this, 0x3C, 4).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the child this[0x38] is always set (the path without it is not taken).
void func_08B8E0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0x08CBEDF8u;
        const std::uint32_t child = word(rt, self + 0x38u);
        ctx.gpr[3] = child;
        put(rt, self, 0x08CBEDF8u);
        ctx.gpr[4] = child;
        if (child != 0u) {
            const std::uint32_t vtable = word(rt, child);
            const std::uint32_t function = word(rt, vtable + 4u);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {child});
        }
        ctx.gpr[2] = 0x08CB7440u;
        ctx.gpr[4] = self;
        put(rt, self, 0x08CB7440u);
        (void)call_keep(rt, ctx, 0x08A7D4E4u, {self});
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0x3Cu;
        ctx.gpr[6] = 4u;
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x3Cu, 4u});
}

// func_0886AA0C(this, index): for index < 8 and a non-null child c = this[0x24 + 4 * index]: its vtable slot 0x14 is called with (c, n, 0) for n = 0, 1, 2, then func_0887E1C8(c, 0) and this[0x24 + 4 * index] = 0.
// v0 = 0 for an index of 8 or more, else the slot's address (this + index * 4) the last store used.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the index is always below 8 (the `v0 = 0` exit of an index of 8 or more is not taken) and the child is never null (the straight store of 0 is not taken).
void func_0886AA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = index < 8u ? 1u : 0u;
    if (index >= 8u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t slot = self + (index << 2);
    ctx.gpr[3] = slot;
    const std::uint32_t child = word(rt, slot + 0x24u);
    ctx.gpr[2] = child;
    if (child != 0u) {
        for (std::uint32_t n = 0; n < 3u; ++n) {
            const std::uint32_t object = word(rt, slot + 0x24u);
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 0x14u);
            ctx.gpr[4] = object;
            ctx.gpr[5] = n;
            ctx.gpr[6] = 0u;
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            (void)call_keep(rt, ctx, function, {object, n, 0u});
        }
        const std::uint32_t object = word(rt, slot + 0x24u);
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x0887E1C8u, {object, 0u});
    }
    ctx.gpr[2] = slot;
    put(rt, slot + 0x24u, 0u);
}

// func_08AA19DC(this, value): the 16-bit `value` (sign-extended) is stored big-endian in a 2-byte frame buffer and this's vtable slot 0x10C is called with (this, buffer, 2); v0 is its.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08AA19DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::int32_t value = static_cast<std::int16_t>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store8(frame.at(0u), static_cast<std::uint8_t>(value >> 8));
    rt.memory().store8(frame.at(1u), static_cast<std::uint8_t>(value));
    const std::uint32_t vtable = word(rt, self);
    const std::uint32_t function = word(rt, vtable + 0x10Cu);
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[6] = 2u;
    ctx.gpr[7] = 0xFFFFFFF8u;
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {self, frame.at(0u), 2u});
}

// func_08BAEEBC(object): s0 = object[0x10], key = object[0xC], lock = s0 + 0x6C, table = s0 + 8: func_08B34888(lock), func_08B2CE7C(table, key), then when func_08B2D1A8(table) equals s0[0x68] also
// func_08B34CA0(lock); the tail call is func_08B34B24(lock).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08BAEEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    std::uint32_t lock;
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t owner = word(rt, object + 0x10u);
        const std::uint32_t key = word(rt, object + 0xCu);
        lock = owner + 0x6Cu;
        const std::uint32_t table = owner + 8u;
        ctx.gpr[4] = lock;
        (void)call_keep(rt, ctx, 0x08B34888u, {lock});
        ctx.gpr[5] = key;
        ctx.gpr[4] = table;
        (void)call_keep(rt, ctx, 0x08B2CE7Cu, {table, key});
        ctx.gpr[4] = table;
        const std::uint32_t answer = call_keep(rt, ctx, 0x08B2D1A8u, {table});
        ctx.gpr[3] = word(rt, owner + 0x68u);
        ctx.gpr[4] = lock;
        if (answer == ctx.gpr[3]) (void)call_keep(rt, ctx, 0x08B34CA0u, {lock});
        ctx.gpr[4] = lock;
    }
    tail_call(ctx, 0x08B34B24u, {lock});
}

// func_08B6E4FC(lo, hi): a double's bits: v0 = 1 when the value is a NaN (the low word is non-zero or the high word without its sign bit exceeds 0x7FF00000... computed as the sign of 0x7FF00000 - ((lo != 0) | (hi & 0x7FFFFFFF))), else 0.
// Branches: none (a straight line).
void func_08B6E4FC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t lo = ctx.gpr[4], hi = ctx.gpr[5];
    const std::uint32_t folded = (((0u - lo) | lo) >> 31) | (hi & 0x7FFFFFFFu);
    ctx.gpr[4] = folded;
    ctx.gpr[5] = hi & 0x7FFFFFFFu;
    ctx.gpr[2] = (0x7FF00000u - folded) >> 31;
}

// func_08B6E268(lo, hi): the absolute value of a double: v0 = lo, v1 = hi without its sign bit.
// Branches: none (a straight line).
void func_08B6E268(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t lo = ctx.gpr[4], hi = ctx.gpr[5] & 0x7FFFFFFFu;
    ctx.gpr[7] = hi;
    ctx.gpr[2] = lo;
    ctx.gpr[3] = hi;
}

// func_0889FBA0(this): four 16-byte records from this + 0x138 are reset (word 0, float 1.0, word 0, byte 0 at +0xC), the busy loops count down without effect, this[0xAC4] (byte) = 0xFF, the word this[0xAC0] gets
// its top 24 bits set (low byte kept), this[0xAC5] (byte) = 0. v0 = the new word, v1 = a1 = -1.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0889FBA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint32_t record = self + 0x138u + (i << 4);
        put(rt, record, 0u);
        put_float(rt, record + 4u, 1.0f);
        put(rt, record + 8u, 0u);
        rt.memory().store8(record + 0xCu, 0u);
    }
    ctx.fpr[0] = 1.0f;
    const std::uint32_t old = word(rt, self + 0xAC0u);
    rt.memory().store8(self + 0xAC4u, 0xFFu);
    const std::uint32_t updated = (old & 0xFFu) | 0xFFFFFF00u;
    put(rt, self + 0xAC0u, updated);
    rt.memory().store8(self + 0xAC5u, 0u);
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = updated;
}

// func_08C20060(-, key): the object o = *func_08A6158C(key) and the argument b = func_08A61594(key); an o of 0 first runs func_08A5BB7C(D_08CB0284); the tail call is o's vtable slot 0xC4 with (o, b).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object is never null (that path calls func_08A5BB7C and then reads through address 0).
void func_08C20060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t object, argument, function;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = key;
        const std::uint32_t holder = call_keep(rt, ctx, 0x08A6158Cu, {key});
        ctx.gpr[4] = key;
        argument = call_keep(rt, ctx, 0x08A61594u, {key});
        object = word(rt, holder);
        ctx.gpr[4] = 0x08CB0284u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
        const std::uint32_t vtable = word(rt, object);
        function = word(rt, vtable + 0xC4u);
        ctx.gpr[2] = vtable;
        ctx.gpr[4] = object;
        ctx.gpr[5] = argument;
        ctx.gpr[25] = function;
    }
    tail_call(ctx, function, {object, argument});
}

// func_08BE4120(this, target): nothing (v0 untouched) when target is 0. Otherwise under the lock this + 0xC (func_08B344E0 .. func_08B3457C as the tail call) the list this[2] (node: key +0x14, next +8) is searched for
// the key `target`; the first node that holds it is passed to func_08BE3DA8(this, node).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): target is never 0 (the early return), the list is never empty and the first node always holds the target: the walk to the next node, the end of the list and the not-found tail are read from the asm only.
void func_08BE4120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], target = ctx.gpr[5];
    if (target == 0u) return;
    const std::uint32_t lock = self + 0xCu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = lock;
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t node = word(rt, self + 8u);
        ctx.gpr[5] = node;
        if (node != 0u) {
            ctx.gpr[2] = word(rt, node + 0x14u);
            for (;;) {
                if (target == ctx.gpr[2]) {
                    ctx.gpr[4] = self;
                    ctx.gpr[5] = node;
                    (void)call_keep(rt, ctx, 0x08BE3DA8u, {self, node});
                    break;
                }
                node = word(rt, node + 8u);
                ctx.gpr[5] = node;
                if (node == 0u) break;
                ctx.gpr[2] = word(rt, node + 0x14u);
            }
        }
        ctx.gpr[4] = lock;
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_08B644BC(size): the C++ allocator: n = size or 1; func_08B35DA0(n) answers the block; while it answers 0 the new-handler word at 0x08D721B4 is called when set (otherwise v0 = 0) and the allocation retried.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): size is never 0 (n = 1 is not taken) and the allocation never fails: the new-handler loop (handler call, retry, v0 = 0 without a handler) is read from the asm only.
void func_08B644BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t size = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t count = size != 0u ? size : 1u;
    ctx.gpr[4] = count;
    std::uint32_t block = call_keep(rt, ctx, 0x08B35DA0u, {count});
    while (block == 0u) {
        const std::uint32_t handler = word(rt, 0x08D721B4u);
        ctx.gpr[2] = handler;
        if (handler == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        (void)call_keep(rt, ctx, handler, {});
        ctx.gpr[4] = count;
        block = call_keep(rt, ctx, 0x08B35DA0u, {count});
    }
}

// func_08B0EB48(x, y, z): the 0x100-byte scratch at the frame is set up by func_08B0E0CC and func_08B0EA48(scratch, x) answers an object o. A non-null o gets func_08B0DD50(o, y, z) (its answer is the result r), then
// its vtable slot 0xC with (o, 1), its vtable slot 0x10 with (o), and func_08B0EA58(o); a null o leaves r = 0. func_08B0DDF0(scratch) closes the scratch; v0 = r.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B0EA48 always answers an object (the null object, v0 = 0, is not taken).
void func_08B0EB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4], y = ctx.gpr[5], z = ctx.gpr[6];
    GuestFrame frame(ctx, 0x110u);
    std::uint32_t result = 0u;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B0E0CCu, {frame.at(0u)});
    ctx.gpr[5] = x;
    ctx.gpr[4] = frame.at(0u);
    const std::uint32_t object = call_keep(rt, ctx, 0x08B0EA48u, {frame.at(0u), x});
    ctx.gpr[6] = z;
    ctx.gpr[5] = y;
    ctx.gpr[4] = object;
    if (object != 0u) {
        result = call_keep(rt, ctx, 0x08B0DD50u, {object, y, z});
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0xCu);
        ctx.gpr[6] = vtable;
        ctx.gpr[3] = function;
        ctx.gpr[5] = 1u;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function, {object, 1u});
        const std::uint32_t second = word(rt, object);
        const std::uint32_t function2 = word(rt, second + 0x10u);
        ctx.gpr[5] = second;
        ctx.gpr[2] = function2;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function2, {object});
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, 0x08B0EA58u, {object});
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B0DDF0u, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// func_08AA96B4(this): a destructor: this[0] = D_08CD5358; func_08C2FBF8(this[0x38], this[0x3C], &scratch); the block this[0x38] .. this[0x40] (when this[0x38] is set) is freed by func_08A5BC54(block, size & ~3, 4);
// func_08C32A04(this + 0x20, this[0x28]), func_08C3295C(this + 8, this[0x10]) and func_08A5B9EC(this + 4); v0 is that last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the block this[0x38] is always null, so the func_08A5BC54 free of the block is read from the asm only.
void func_08AA96B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CD5358u);
    const std::uint32_t array = self + 0x38u;
    ctx.gpr[2] = 0x08CD5358u;
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[5] = word(rt, array + 4u);
    (void)call_keep(rt, ctx, 0x08C2FBF8u, {word(rt, self + 0x38u), ctx.gpr[5], frame.at(0u)});
    const std::uint32_t block = word(rt, self + 0x38u);
    const std::uint32_t size = static_cast<std::uint32_t>((static_cast<std::int32_t>(word(rt, array + 8u) - block) >> 2) << 2);
    ctx.gpr[4] = block;
    ctx.gpr[5] = size;
    ctx.gpr[6] = 4u;
    ctx.gpr[2] = block;
    if (block != 0u) (void)call_keep(rt, ctx, 0x08A5BC54u, {block, size, 4u});
    ctx.gpr[4] = self + 0x20u;
    (void)call_keep(rt, ctx, 0x08C32A04u, {self + 0x20u, word(rt, self + 0x28u)});
    ctx.gpr[4] = self + 8u;
    (void)call_keep(rt, ctx, 0x08C3295Cu, {self + 8u, word(rt, self + 0x10u)});
    ctx.gpr[4] = self + 4u;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {self + 4u});
}

// func_08A1F520(-, op, args): op 0: func_08A4D2F0(D_08CF68E8) (v0 is its). op other than 1: v0 = 1. op 1: the object c = args[0] (null: func_08A5BB7C(D_08CAA480) first, then a load through address 0) writes through its vtable slot 0xC
// a string handle into the frame (frame + 4, c), func_08A4D2F0(D_08CF68E8, handle) runs and the handle's representation (handle - 0xC), unless it is the shared empty one at D_08D74FD0, loses a reference
// func_08B60804(handle - 4, -1) and is released by func_08B8542C(representation, frame) when the count was not positive.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every compared call has op 0 (only the first `if` runs): op 1 (the string-handle body) and the other ops are read from the asm only, so the function is NOT registered (it was compared on its early exit only).
void func_08A1F520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t op = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (op == 0u) {
        ctx.gpr[2] = 0x00020000u;
        ctx.gpr[4] = word(rt, 0x08CF68E8u);
        (void)call_keep(rt, ctx, 0x08A4D2F0u, {word(rt, 0x08CF68E8u)});
        return;
    }
    ctx.gpr[2] = 1u;
    if (op != 1u) return;
    const std::uint32_t object = word(rt, args);
    ctx.gpr[4] = 0x08CAA480u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[5] = object;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, function, {frame.at(4u), object});
    ctx.gpr[4] = word(rt, 0x08CF68E8u);
    ctx.gpr[5] = word(rt, frame.at(4u));
    (void)call_keep(rt, ctx, 0x08A4D2F0u, {ctx.gpr[4], ctx.gpr[5]});
    const std::uint32_t handle = word(rt, frame.at(4u));
    const std::uint32_t representation = handle - 0xCu;
    ctx.gpr[3] = handle;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = handle - 4u;
    if (representation == 0x08D74FD0u) return;
    const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {handle - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = representation;
    ctx.gpr[5] = frame.at(0u);
    if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
}

// func_08979A6C(this): this[0x1AC] = 0 and this[0x198] (byte) = 0 before func_0897999C(this); func_0897851C(this); the object func_08977260(this) answers gets its vtable slot 0x198 with (object); this[0x7C] = this[0x74] and
// this[0x80] = this[0x78]; under the lock this + 0x20 (func_08B344E0 .. func_08B3457C) func_08BEDD64(this + 0x18) runs and this[0x18] = this[0x1C] = this + 0x18; the tail call is func_08A5BC6C.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08979A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 0x20u, list = self + 0x18u;
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self + 0x1ACu, 0u);
        rt.memory().store8(self + 0x198u, 0u);
        (void)call_keep(rt, ctx, 0x0897999Cu, {self});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0897851Cu, {self});
        ctx.gpr[4] = self;
        const std::uint32_t object = call_keep(rt, ctx, 0x08977260u, {self});
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x198u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function, {object});
        ctx.gpr[3] = self + 0x74u;
        ctx.gpr[2] = word(rt, self + 0x74u);
        ctx.gpr[5] = word(rt, self + 0x78u);
        put(rt, self + 0x7Cu, ctx.gpr[2]);
        put(rt, self + 0x80u, ctx.gpr[5]);
        ctx.gpr[4] = lock;
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        ctx.gpr[4] = list;
        (void)call_keep(rt, ctx, 0x08BEDD64u, {list});
        put(rt, self + 0x18u, list);
        ctx.gpr[4] = lock;
        put(rt, list + 4u, list);
        (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
    }
    tail_call(ctx, 0x08A5BC6Cu, {});
}

// func_089C4C20(-, a1, out): *out = 0; func_08B10204(temp, a1, 1) fills a 0x3C-byte record that is copied, and when its first word is 0 *out = its word +0x10 and the answer is its word +0x20; a non-zero first word answers 0.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_089C4C20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a1 = ctx.gpr[5], out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    put(rt, out, 0u);
    ctx.gpr[4] = frame.at(0x40u);
    ctx.gpr[6] = 1u;
    (void)call_keep(rt, ctx, 0x08B10204u, {frame.at(0x40u), a1, 1u});
    for (std::uint32_t i = 0; i < 3u; ++i) {
        for (std::uint32_t j = 0; j < 4u; ++j) put(rt, frame.at(i * 16u + j * 4u), word(rt, frame.at(0x40u + i * 16u + j * 4u)));
    }
    put(rt, frame.at(0x38u), word(rt, frame.at(0x78u)));
    put(rt, frame.at(0x30u), word(rt, frame.at(0x70u)));
    put(rt, frame.at(0x34u), word(rt, frame.at(0x74u)));
    std::uint32_t answer;
    if (word(rt, frame.at(0u)) != 0u) {
        answer = 0u;
    } else {
        ctx.gpr[2] = word(rt, frame.at(0x10u));
        answer = word(rt, frame.at(0x20u));
        put(rt, out, ctx.gpr[2]);
    }
    ctx.gpr[5] = answer;
    ctx.gpr[2] = answer;
}

// func_08966E24(this, first, second): when func_08966174(this) answers non-zero, the object o = this[0x80] takes the float *first at +0 when it differs from o's (a NaN counts as different) and the float *second at +4 likewise;
// each store sets bit 2 of this[0xE4]. A null pointer skips its float. v0: 0 when func_08966174 answered 0, else what the last executed load / or left.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08966174 always answers non-zero (the early exit with v0 = 0 is not taken) and both pointers are always non-null (the skips of a null pointer are not taken).
void func_08966E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (call_keep(rt, ctx, 0x08966174u, {self}) == 0u) return;
    if (first != 0u) {
        const std::uint32_t object = word(rt, self + 0x80u);
        ctx.gpr[2] = object;
        ctx.fpr[1] = float_at(rt, first);
        ctx.fpr[0] = float_at(rt, object);
        if (!(ctx.fpr[0] == ctx.fpr[1])) {
            put_float(rt, object, ctx.fpr[1]);
            ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
            put(rt, self + 0xE4u, ctx.gpr[2]);
        }
    }
    if (second == 0u) return;
    const std::uint32_t object = word(rt, self + 0x80u);
    ctx.gpr[2] = object;
    ctx.fpr[1] = float_at(rt, second);
    ctx.fpr[0] = float_at(rt, object + 4u);
    if (ctx.fpr[0] == ctx.fpr[1]) return;
    put_float(rt, object + 4u, ctx.fpr[1]);
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    put(rt, self + 0xE4u, ctx.gpr[2]);
}

// func_08894444(this): a block of 0x24 bytes from func_08B62344 (its word +0x1C = 0) is stored at this[0]; the guarded static at 0x08D75EC8 (guard byte; func_08B6441C / func_08B64494) gets its word 0x08D75ED0 =
// D_08CC1DD8 on the first call; this[4] = 0x08D75ED0 and the tail call is func_0889406C(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C always answers non-zero (the other answer, which skips the initialisation, is not taken); both values of the guard byte are taken.
void func_08894444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = 0x24u;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x24u});
        put(rt, block + 0x1Cu, 0u);
        put(rt, self, block);
        ctx.gpr[3] = 0x000A0000u;
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int8_t>(rt.memory().load8(0x08D75EC8u)));
        if (ctx.gpr[2] == 0u) {
            ctx.gpr[4] = 0x08D75EC8u;
            const std::uint32_t taken = call_keep(rt, ctx, 0x08B6441Cu, {0x08D75EC8u});
            ctx.gpr[5] = 0x000A0000u;
            if (taken != 0u) {
                put(rt, 0x08D75ED0u, 0x08CC1DD8u);
                ctx.gpr[4] = 0x08D75EC8u;
                (void)call_keep(rt, ctx, 0x08B64494u, {0x08D75EC8u});
            }
        }
        put(rt, self + 4u, 0x08D75ED0u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x0889406Cu, {self});
}

// func_08893F7C(this): a block of 0xC bytes from func_08B62344 (zeroed) is stored at this[0]; the guarded static at 0x08D75EB8 gets its word 0x08D75EC0 = D_08CC1DA8 on the first call; this[4] = 0x08D75EC0 and the tail
// call is func_08893BAC(this, 0x40).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C always answers non-zero (the other answer, which skips the initialisation, is not taken); both values of the guard byte are taken.
void func_08893F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = 0xCu;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0xCu});
        put(rt, block, 0u);
        put(rt, block + 4u, 0u);
        put(rt, block + 8u, 0u);
        put(rt, self, block);
        ctx.gpr[5] = 0x000A0000u;
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int8_t>(rt.memory().load8(0x08D75EB8u)));
        if (ctx.gpr[2] == 0u) {
            ctx.gpr[4] = 0x08D75EB8u;
            const std::uint32_t taken = call_keep(rt, ctx, 0x08B6441Cu, {0x08D75EB8u});
            ctx.gpr[5] = 0x000A0000u;
            if (taken != 0u) {
                put(rt, 0x08D75EC0u, 0x08CC1DA8u);
                ctx.gpr[4] = 0x08D75EB8u;
                (void)call_keep(rt, ctx, 0x08B64494u, {0x08D75EB8u});
            }
        }
        put(rt, self + 4u, 0x08D75EC0u);
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0x40u;
    }
    tail_call(ctx, 0x08893BACu, {self, 0x40u});
}

// func_08BF8A7C(this, object): s = object's vtable slot 0x114 called with (object); 0 when s is 0 or func_089BD608(s) answers s itself; 1 when func_089BD4DC(s) answers 0; otherwise whether func_089BD4DC(s) equals this[0x1C].
// v0 = v1 = that answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object's slot answers are never 0, func_089BD608(s) never equals s and func_089BD4DC(s) never answers 0, so the answers 0 and 1 of those exits are read from the asm only; the final comparison with this[0x1C] always found them equal (answer 1; the `!=` answer 0 is not taken).
void func_08BF8A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x114u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const std::uint32_t s = call_keep(rt, ctx, function, {object});
    std::uint32_t answer;
    ctx.gpr[4] = s;
    if (s == 0u) {
        answer = 0u;
    } else if (call_keep(rt, ctx, 0x089BD608u, {s}) == s) {
        ctx.gpr[4] = s;
        answer = 0u;
    } else {
        ctx.gpr[4] = s;
        if (call_keep(rt, ctx, 0x089BD4DCu, {s}) == 0u) {
            answer = 1u;
        } else {
            ctx.gpr[4] = s;
            const std::uint32_t other = call_keep(rt, ctx, 0x089BD4DCu, {s});
            ctx.gpr[3] = word(rt, self + 0x1Cu);
            ctx.gpr[2] = other ^ ctx.gpr[3];
            answer = ctx.gpr[2] < 1u ? 1u : 0u;
        }
    }
    ctx.gpr[3] = answer;
    ctx.gpr[2] = answer;
}

// func_08BDD7D8(first, last): for each 0x1C-byte element from first to last (exclusive; nothing when first == last): func_08BDD7C8(e[0x10], e[0x14], frame + 1) and, when e[0x10] is set, func_08B35254(e[0x10]); then
// func_08BDD7B8(e[4], e[8], frame) and, when e[4] is set, func_08B35254(e[4]). v0 is the last call's (or the last load).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every compared call has first == last (an empty range, the early return): the loop body is read from the asm only, so the function is NOT registered (compared on its early exit only).
void func_08BDD7D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (first == last) return;
    std::uint32_t cursor = first, field = first + 4u;
    do {
        cursor += 0x1Cu;
        ctx.gpr[4] = word(rt, field + 0xCu);
        ctx.gpr[5] = word(rt, field + 0x10u);
        ctx.gpr[6] = frame.at(1u);
        (void)call_keep(rt, ctx, 0x08BDD7C8u, {ctx.gpr[4], ctx.gpr[5], frame.at(1u)});
        ctx.gpr[2] = word(rt, field + 0xCu);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[4] = ctx.gpr[2];
            (void)call_keep(rt, ctx, 0x08B35254u, {ctx.gpr[2]});
        }
        ctx.gpr[4] = word(rt, field);
        ctx.gpr[5] = word(rt, field + 4u);
        ctx.gpr[6] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08BDD7B8u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
        ctx.gpr[2] = word(rt, field);
        field += 0x1Cu;
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[4] = ctx.gpr[2];
            (void)call_keep(rt, ctx, 0x08B35254u, {ctx.gpr[2]});
        }
    } while (last != cursor);
}

// func_08B1BC98(this): func_08B28DCC(this), func_08B1BB00(this + 0x28), func_08B1BC10(this + 0x78); this[0x84], [0x88], [0x8C], [0x90] = 0; func_08B1AFB8(this + 0x98); the tail call is func_08B1AFF4(this + 0x98, a1, 0x411A, 0).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B1BC98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t inner = self + 0x98u;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t saved_s0 = ctx.gpr[16];
        ctx.gpr[16] = self;  // as in func_0880BF70: the original keeps this in s0 across func_08B28DCC
        (void)call_keep(rt, ctx, 0x08B28DCCu, {self});
        ctx.gpr[4] = self + 0x28u;
        (void)call_keep(rt, ctx, 0x08B1BB00u, {self + 0x28u});
        ctx.gpr[4] = self + 0x78u;
        (void)call_keep(rt, ctx, 0x08B1BC10u, {self + 0x78u});
        ctx.gpr[4] = inner;
        put(rt, self + 0x84u, 0u);
        put(rt, self + 0x88u, 0u);
        put(rt, self + 0x8Cu, 0u);
        put(rt, self + 0x90u, 0u);
        (void)call_keep(rt, ctx, 0x08B1AFB8u, {inner});
        ctx.gpr[4] = inner;
        ctx.gpr[6] = 0x411Au;
        ctx.gpr[7] = 0u;
        ctx.gpr[16] = saved_s0;
    }
    tail_call(ctx, 0x08B1AFF4u, {inner, ctx.gpr[5], 0x411Au, 0u});
}

// func_08A20D94(-, a1, count, args): f20 = 0.0; when count > 0 the object c = args[0] (null: func_08A5BB7C(D_08CAA7EC) first, then a load through address 0) answers a float through its vtable slot 0x54 (kept as f20). Then
// func_08A20CC8(holder, a1) fills a holder at the frame whose pointer h (null: func_08A5BB7C(D_08CAA7F4) first, then a load through address 0) is passed to func_08A209A4(h) with f12 = f20, and func_08A5B9EC(holder) destroys the holder.
// v0 is that last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count is always positive (count <= 0, f20 = 0.0, is not taken) and neither the object nor the holder is ever null.
void func_08A20D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a1 = ctx.gpr[5], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    float scale = 0.0f;
    if (static_cast<std::int32_t>(count) > 0) {
        const std::uint32_t object = word(rt, args);
        ctx.gpr[4] = 0x08CAA7ECu;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7ECu});
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x54u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        scale = call_keep(rt, ctx, function, {object}, {}).f0;
    }
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = a1;
    (void)call_keep(rt, ctx, 0x08A20CC8u, {frame.at(0u), a1});
    const std::uint32_t holder = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08CAA7F4u;
    if (holder == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7F4u});
    ctx.fpr[12] = scale;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A209A4u, {holder}, {scale});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08C46508(a, b): a vector of three floats: a[i] += b[i] (a[0] first, then a[1], then a[2]).
// Branches: none (a straight line).
void func_08C46508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    const float f1 = float_at(rt, b);
    const float f0 = float_at(rt, a);
    float f2 = float_at(rt, a + 4u);
    float f3 = float_at(rt, a + 8u);
    put_float(rt, a, f0 + f1);
    const float g1 = float_at(rt, b + 4u);
    f2 += g1;
    put_float(rt, a + 4u, f2);
    const float g0 = float_at(rt, b + 8u);
    f3 += g0;
    put_float(rt, a + 8u, f3);
    ctx.fpr[0] = g0;
    ctx.fpr[1] = g1;
    ctx.fpr[2] = f2;
    ctx.fpr[3] = f3;
}

// func_08BAE7A0(out, x, y, z): out[0] = (float) of the 32-bit integer stored at x, out[1] = (float) of the one at y, out[2] = (float) of the one at z.
// Branches: none (a straight line).
void func_08BAE7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], x = ctx.gpr[5], y = ctx.gpr[6], z = ctx.gpr[7];
    put_float(rt, out, static_cast<float>(signed_of(word(rt, x))));
    put_float(rt, out + 4u, static_cast<float>(signed_of(word(rt, y))));
    const std::uint32_t last = word(rt, z);
    ctx.fpr[1] = fbits(last);
    ctx.fpr[0] = static_cast<float>(signed_of(last));
    put_float(rt, out + 8u, ctx.fpr[0]);
}

// func_08B9D834(this, object, base, index): this[0] = this[4] = 0, this[8] (byte) = 0 and func_08B9D7E0(this, base + object[0xC] + index * 116, 0) (an index of 29 words); v0 = this.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B9D834(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5], base = ctx.gpr[6], index = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t element = (base + word(rt, object + 0xCu)) + (((index << 5) - (index << 2)) + index) * 4u;
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    ctx.gpr[5] = element;
    ctx.gpr[6] = 0u;
    rt.memory().store8(self + 8u, 0u);
    (void)call_keep(rt, ctx, 0x08B9D7E0u, {self, element, 0u});
    ctx.gpr[2] = self;
}

// func_08A81BF4(this, a1): func_08A818A0(holder, this, a1) fills a holder at the frame; when holder[0] is non-zero func_08C29370(this + 0x14, holder) answers a pointer and the result is whether it differs from this + 0x18;
// func_08A5B9EC(holder) destroys the holder. v0 = the result (0 or 1).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): holder[0] is always non-zero (the answer 0 without the func_08C29370 call is not taken) and the found pointer always differs from this + 0x18 (the answer is always 1).
void func_08A81BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = a1;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = self;
    (void)call_keep(rt, ctx, 0x08A818A0u, {frame.at(0u), self, a1});
    std::uint32_t result = 0u;
    ctx.gpr[4] = self + 0x14u;
    ctx.gpr[2] = word(rt, frame.at(0u));
    ctx.gpr[5] = frame.at(0u);
    if (ctx.gpr[2] != 0u) {
        const std::uint32_t found = call_keep(rt, ctx, 0x08C29370u, {self + 0x14u, frame.at(0u)});
        ctx.gpr[3] = (self + 0x18u) ^ found;
        result = ctx.gpr[3] != 0u ? 1u : 0u;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// func_088AA698(this): func_0896275C(this[0]) and then the tail call func_0896275C(this[0] + 0x10).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_088AA698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t second;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = word(rt, self);
        (void)call_keep(rt, ctx, 0x0896275Cu, {ctx.gpr[4]});
        second = word(rt, self) + 0x10u;
        ctx.gpr[4] = second;
    }
    tail_call(ctx, 0x0896275Cu, {second});
}

// func_088A3750(this): func_088AB024(this + 0x270), func_088AA698(this + 0x270), this[0x18] = 0; v0 = 1.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_088A3750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t part = self + 0x270u;
    ctx.gpr[4] = part;
    (void)call_keep(rt, ctx, 0x088AB024u, {part});
    ctx.gpr[4] = part;
    (void)call_keep(rt, ctx, 0x088AA698u, {part});
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = 1u;
}

// func_08881C38(this, a1): v = this[0xC][0x16F0]; a negative v ends (v0 = v). Otherwise o = func_0886B080(this[8]) and the tail call is o's vtable slot 0xC with (o, v, a1).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the value this[0xC][0x16F0] is never negative (the early exit with v0 = that value is not taken).
void func_08881C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    std::uint32_t object, function, index;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[3] = word(rt, self + 0xCu);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x16F0u);
        if (signed_of(ctx.gpr[2]) < 0) return;
        ctx.gpr[4] = word(rt, self + 8u);
        object = call_keep(rt, ctx, 0x0886B080u, {ctx.gpr[4]});
        const std::uint32_t vtable = word(rt, object);
        ctx.gpr[6] = vtable;
        ctx.gpr[3] = word(rt, self + 0xCu);
        function = word(rt, vtable + 0x30u);
        index = word(rt, ctx.gpr[3] + 0x16F0u);
        ctx.gpr[5] = index;
        ctx.gpr[6] = a1;
        ctx.gpr[4] = object;
        ctx.gpr[25] = function;
    }
    tail_call(ctx, function, {object, index, a1});
}

// func_0894D094(this, a1, a2, a3): this[0xDC] = max(a2, 0) (signed) is stored while func_0894D038(this, a3 & 0xFF) runs; this[0x20] = 1 when a1 is non-zero, else 0; then func_08958BE4(this), func_089593DC(this, -1),
// func_08959F54(this, -1), func_0894D114(this, this + 0xE0) and the tail call func_0894CCAC (this and this + 0xE0 are still in a0 / a1).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a1 is always 0 (this[0x20] = 1 and v0 = 1 are not taken).
void func_0894D094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5], a3 = ctx.gpr[7];
    const std::int32_t clamped = std::max(signed_of(ctx.gpr[6]), 0);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0u;
        ctx.gpr[6] = static_cast<std::uint32_t>(clamped);
        ctx.gpr[5] = a3 & 0xFFu;
        put(rt, self + 0xDCu, static_cast<std::uint32_t>(clamped));
        (void)call_keep(rt, ctx, 0x0894D038u, {self, a3 & 0xFFu});
        ctx.gpr[4] = self;
        put(rt, self + 0x20u, 0u);
        if (flag != 0u) {
            ctx.gpr[2] = 1u;
            put(rt, self + 0x20u, 1u);
        }
        (void)call_keep(rt, ctx, 0x08958BE4u, {self});
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0xFFFFFFFFu;
        (void)call_keep(rt, ctx, 0x089593DCu, {self, 0xFFFFFFFFu});
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0xFFFFFFFFu;
        (void)call_keep(rt, ctx, 0x08959F54u, {self, 0xFFFFFFFFu});
        ctx.gpr[4] = self;
        ctx.gpr[5] = self + 0xE0u;
        (void)call_keep(rt, ctx, 0x0894D114u, {self, self + 0xE0u});
    }
    tail_call(ctx, 0x0894CCACu, {});
}

// func_08C283A8(this): func_08A76AA0(holder) fills a holder at the frame; the object s = this[8] (null: func_08A5BB7C(D_08CB14E0) first, then a load through address 0) gets func_08A640EC(s, holder); unless this + 4 is the holder
// func_08A5BAD8(this + 4, holder[0]) copies it; func_08A5B9EC(holder) destroys it. v0 is that last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object is never null (that path reads through address 0) and this + 4 is never the holder (the copy by func_08A5BAD8 always runs).
void func_08C283A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    const std::uint32_t object = word(rt, self + 8u);
    ctx.gpr[4] = 0x08CB14E0u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB14E0u});
    ctx.gpr[4] = object;
    ctx.gpr[5] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A640ECu, {object, frame.at(0u)});
    ctx.gpr[3] = self + 4u;
    ctx.gpr[4] = self + 4u;
    if (self + 4u != frame.at(0u)) {
        ctx.gpr[5] = word(rt, frame.at(0u));
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {self + 4u, ctx.gpr[5]});
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08C1C89C(this, ch, pos): std::string::find(char, pos): s = this[0] (the characters; the size is the word before them at s - 0xC); -1 when pos >= size; otherwise func_08B79998(s + pos, (signed char) ch, size - pos)
// searches and the answer is the offset of the hit from s, or -1 when nothing is found.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): pos is always below the size and the character is always found (the answer -1 is read from the asm only).
void func_08C1C89C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], ch = ctx.gpr[5], pos = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t characters = word(rt, self);
    const std::uint32_t size = word(rt, characters - 0xCu);
    ctx.gpr[2] = pos;
    ctx.gpr[3] = size;
    ctx.gpr[4] = characters + pos;
    ctx.gpr[6] = size - pos;
    ctx.gpr[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ch)));
    if (pos < size) {
        const std::uint32_t hit = call_keep(rt, ctx, 0x08B79998u, {characters + pos, ctx.gpr[5], size - pos});
        ctx.gpr[4] = hit - characters;
        if (hit != 0u) {
            ctx.gpr[2] = ctx.gpr[4];
            return;
        }
    }
    ctx.gpr[4] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08C24530(this, set, pos, count): std::string::find_first_not_of(set, pos, count): from pos on, while the character s[i] (s = this[0], size = the word before it) is found in the `count` bytes at `set`
// (func_08B79998(set, (signed char) s[i], count) answers non-zero), i advances; the first i whose character is not found is the answer. -1 when pos is not below the size or every character is found.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): pos is always below the size and the first character checked is the answer or the string ends right after it (the -1 exit after a longer walk is read from the asm only).
void func_08C24530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], set = ctx.gpr[5], count = ctx.gpr[7];
    std::uint32_t index = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t characters = word(rt, self);
    ctx.gpr[3] = characters;
    ctx.gpr[2] = word(rt, characters - 0xCu);
    if (index < ctx.gpr[2]) {
        for (;;) {
            ctx.gpr[2] = index + characters;
            const std::int8_t ch = static_cast<std::int8_t>(rt.memory().load8(index + characters));
            ctx.gpr[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(ch));
            ctx.gpr[4] = set;
            ctx.gpr[6] = count;
            const std::uint32_t found = call_keep(rt, ctx, 0x08B79998u, {set, ctx.gpr[5], count});
            ctx.gpr[2] = index;
            if (found == 0u) return;
            characters = word(rt, self);
            ctx.gpr[3] = characters;
            ++index;
            ctx.gpr[2] = word(rt, characters - 0xCu);
            const bool more = index < ctx.gpr[2];
            ctx.gpr[2] = index + characters;
            if (!more) break;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08A03BB8(this, other): func_08A76ACC(this); this[0] = D_08CBBE10; func_0896418C(this + 0x20, other); this[0x30] = other[0x10]. v0 is func_0896418C's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08A03BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08A76ACCu, {self});
    ctx.gpr[2] = 0x08CBBE10u;
    put(rt, self, 0x08CBBE10u);
    ctx.gpr[4] = self + 0x20u;
    ctx.gpr[5] = other;
    (void)call_keep(rt, ctx, 0x0896418Cu, {self + 0x20u, other});
    ctx.gpr[2] = word(rt, other + 0x10u);
    put(rt, self + 0x30u, ctx.gpr[2]);
}

// func_089B82A0(this): when this[0x124] is 0 the tail call is func_0896F9DC. Otherwise a zero byte this[0x158] runs func_089B7574 and a non-zero one func_089B80AC (both with the same registers); v0 = 2.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0x124] is always non-zero (the tail call func_0896F9DC is not taken) and the byte this[0x158] is always 0 (func_089B80AC is not taken).
void func_089B82A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = word(rt, self + 0x124u);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[2] = rt.memory().load8(self + 0x158u);
            ctx.gpr[6] = 0u;
            if (ctx.gpr[2] == 0u) (void)call_keep(rt, ctx, 0x089B7574u, {});
            else (void)call_keep(rt, ctx, 0x089B80ACu, {});
            ctx.gpr[2] = 2u;
            return;
        }
    }
    tail_call(ctx, 0x0896F9DCu, {});
}

// func_088B07B8(this): this[0] = this[4] = 0, this[8] = 0 ... a list head set up: this[0xC] = this[0x10] = 0 (words +4 / +8 of the record at +8), this[0x14] = this[0x18] = this + 0xC, this[0x1C] = 0; the tail call is func_08B28D34(this + 0x20).
// Branches: none (a straight line).
void func_088B07B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = self + 8u;
    ctx.gpr[5] = self + 0xCu;
    ctx.gpr[2] = self;
    put(rt, self, 0u);
    ctx.gpr[4] = self + 0x20u;
    put(rt, self + 4u, 0u);
    put(rt, self + 0x18u, self + 0xCu);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, self + 0xCu);
    tail_call(ctx, 0x08B28D34u, {self + 0x20u});
}

// func_0887EE40(this, a1, a2, a3): v0 = whether func_088841D0(this[0x74], a1, a2 & 0xFF, a3 & 0xFF) answers non-zero.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_088841D0 always answers non-zero (v0 = 1; the answer 0 is not taken).
void func_0887EE40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] &= 0xFFu;
    ctx.gpr[7] &= 0xFFu;
    ctx.gpr[4] = word(rt, self + 0x74u);
    const std::uint32_t answer = call_keep(rt, ctx, 0x088841D0u, {ctx.gpr[4], a1, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = answer != 0u ? 1u : 0u;
}

// func_0887BC90(this, a1): func_0887BB3C(this[1]) and the tail call func_0887B768(this[1], a1).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0887BC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    std::uint32_t inner;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = word(rt, self + 4u);
        (void)call_keep(rt, ctx, 0x0887BB3Cu, {ctx.gpr[4]});
        inner = word(rt, self + 4u);
        ctx.gpr[4] = inner;
        ctx.gpr[5] = a1;
    }
    tail_call(ctx, 0x0887B768u, {inner, a1});
}

// func_08875038(out, a1): a block of 0x858 bytes from func_08B62344 is built by func_088765D4(block, a1) and kept in out[0]. v0 is func_088765D4's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08875038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[4] = 0x858u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x858u});
    ctx.gpr[5] = a1;
    ctx.gpr[4] = block;
    (void)call_keep(rt, ctx, 0x088765D4u, {block, a1});
    put(rt, out, block);
}

// func_08870870(this, f12): the object o = this[0xC] takes the float f12 at +0x94; func_08A517B4(frame + 0x10, o, 3) fills four floats that are copied to the frame, func_08B4A6A0(frame, 57.2957...f = 0x42652EE0)
// works on that copy and o[0x90] = the copy's float +4. v0 = o.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08870870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float angle = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[6] = 3u;
    ctx.gpr[2] = word(rt, self + 0xCu);
    put_float(rt, ctx.gpr[2] + 0x94u, angle);
    ctx.gpr[4] = frame.at(0x10u);
    ctx.gpr[5] = word(rt, self + 0xCu);
    (void)call_keep(rt, ctx, 0x08A517B4u, {frame.at(0x10u), ctx.gpr[5], 3u});
    for (std::uint32_t i = 0; i < 4u; ++i) put(rt, frame.at(i * 4u), word(rt, frame.at(0x10u + i * 4u)));
    ctx.fpr[12] = fbits(0x42652EE0u);
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B4A6A0u, {frame.at(0u)}, {fbits(0x42652EE0u)});
    ctx.gpr[2] = word(rt, self + 0xCu);
    ctx.fpr[0] = float_at(rt, frame.at(4u));
    put_float(rt, ctx.gpr[2] + 0x90u, ctx.fpr[0]);
}

// func_0886E008(this, flag): with flag != 0: when func_0886A9A0(this, 6) answers 0, func_0886AAA4(this, this[0x1C] + 0x28) runs first; with flag == 0: when func_0886A9A0(this, 6) answers non-zero, func_0886AA0C(this, 6)
// runs first. The tail call is func_088738DC(this[0x1C] + 0x28, flag) (the word at +0x70 re-read after the calls).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): flag is always non-zero and func_0886A9A0 always answers 0 (the func_0886AAA4 path): flag == 0 with its func_0886AA0C call, and an answer of non-zero, are read from the asm only (side paths).
void func_0886E008(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5];
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[5] = 6u;
        if (flag != 0u) {
            ctx.gpr[4] = self;
            const std::uint32_t answer = call_keep(rt, ctx, 0x0886A9A0u, {self, 6u});
            ctx.gpr[4] = self;
            if (answer == 0u) {
                ctx.gpr[5] = word(rt, self + 0x70u);
                ctx.gpr[5] += 0x28u;
                (void)call_keep(rt, ctx, 0x0886AAA4u, {self, ctx.gpr[5]});
                ctx.gpr[4] = word(rt, self + 0x70u);
                goto done;
            }
        } else {
            const std::uint32_t answer = call_keep(rt, ctx, 0x0886A9A0u, {self, 6u});
            ctx.gpr[4] = self;
            ctx.gpr[5] = 6u;
            if (answer != 0u) (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 6u});
        }
        ctx.gpr[4] = word(rt, self + 0x70u);
    done:
        ctx.gpr[5] = flag;
        target = ctx.gpr[4] + 0x28u;
        ctx.gpr[4] = target;
    }
    tail_call(ctx, 0x088738DCu, {target, flag});
}

// func_0886C31C(this, key): m = this[1]; the tree at m + 0x1DC (end node m + 0x1E0) is searched by func_08B93AE8(tree, &key); a node whose key (+0x10) is not above `key`... when the key is before the found node
// (or the found node is the end) func_08B940D8(tree, node, {key, 0}) inserts it and its node is used. v0 = the byte at +0x14 of the node.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): not reached by any compared case (compared 0 in the fast gate), so it is NOT registered.
void func_0886C31C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0x10u), key);
    ctx.gpr[5] = frame.at(0x10u);
    const std::uint32_t map = word(rt, self + 4u);
    const std::uint32_t tree = map + 0x1DCu;
    ctx.gpr[4] = tree;
    std::uint32_t node = call_keep(rt, ctx, 0x08B93AE8u, {tree, frame.at(0x10u)});
    ctx.gpr[7] = node;
    ctx.gpr[4] = tree;
    ctx.gpr[5] = node;
    ctx.gpr[6] = frame.at(0u);
    bool insert;
    if (map + 0x1E0u == node) {
        ctx.gpr[3] = key;
        insert = true;
    } else {
        ctx.gpr[2] = word(rt, node + 0x10u);
        ctx.gpr[3] = key;
        insert = signed_of(key) < signed_of(ctx.gpr[2]);
        ctx.gpr[2] = insert ? 1u : 0u;
    }
    if (insert) {
        put(rt, frame.at(0u), key);
        rt.memory().store8(frame.at(4u), 0u);
        node = call_keep(rt, ctx, 0x08B940D8u, {tree, node, frame.at(0u)});
        ctx.gpr[7] = node;
    }
    ctx.gpr[2] = rt.memory().load8(node + 0x14u);
}

// func_0886ADEC(this, flag): for i = 0, 1, 2: this's vtable slot 2 is called with (this, i, func_0886AF70(this[7], i)); then func_0886BD48(this[7], this + 4), o = func_0886B438(this[7]),
// func_0887D33C(o, this + 0x64) and this[0x20] (byte) = flag & 0xFF. v0 is func_0887D33C's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0886ADEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0; i < 3u; ++i) {
        const std::uint32_t vtable = word(rt, self);
        ctx.gpr[2] = vtable;
        ctx.gpr[4] = word(rt, self + 0x1Cu);
        ctx.gpr[5] = i;
        const std::uint32_t function = word(rt, vtable + 8u);
        const std::uint32_t part = call_keep(rt, ctx, 0x0886AF70u, {ctx.gpr[4], i});
        ctx.gpr[6] = part;
        ctx.gpr[5] = i;
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, function, {self, i, part});
    }
    ctx.gpr[2] = word(rt, self);
    ctx.gpr[4] = word(rt, self + 0x1Cu);
    ctx.gpr[5] = self + 4u;
    (void)call_keep(rt, ctx, 0x0886BD48u, {ctx.gpr[4], self + 4u});
    ctx.gpr[4] = word(rt, self + 0x1Cu);
    const std::uint32_t object = call_keep(rt, ctx, 0x0886B438u, {ctx.gpr[4]});
    ctx.gpr[4] = object;
    ctx.gpr[5] = self + 0x64u;
    (void)call_keep(rt, ctx, 0x0887D33Cu, {object, self + 0x64u});
    rt.memory().store8(self + 0x20u, static_cast<std::uint8_t>(flag));
}

// func_08869B28(this, a1, a2, f12): under the lock this (func_08B344E0 .. the tail call func_08B3457C(this)): o = func_0886A020(this + 0x1B0, a1, a2); a non-null o goes through func_0886A9A0(o, 4) and func_08B93734, and a
// non-null result p gets func_08870870(p, f12). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object func_0886A020 answers and the one func_08B93734 answers are never null (the exits of a null one are not taken).
void func_08869B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    const float angle = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_keep(rt, ctx, 0x08B344E0u, {self});
        ctx.gpr[5] = a1;
        ctx.gpr[4] = self + 0x1B0u;
        ctx.gpr[6] = a2;
        const std::uint32_t object = call_keep(rt, ctx, 0x0886A020u, {self + 0x1B0u, a1, a2});
        ctx.gpr[4] = object;
        ctx.gpr[5] = 4u;
        if (object != 0u) {
            const std::uint32_t found = call_keep(rt, ctx, 0x0886A9A0u, {object, 4u});
            ctx.gpr[4] = found;
            const std::uint32_t part = call_keep(rt, ctx, 0x08B93734u, {found});
            ctx.gpr[4] = part;
            ctx.fpr[12] = angle;
            if (part != 0u) (void)call_keep(rt, ctx, 0x08870870u, {part}, {angle});
        }
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B3457Cu, {self});
}

// func_08869988(this): a constructor: func_08B28DCC(this), func_088B07B8(this + 0x28), func_0887AF08(this + 0x74), func_08B04D98(this + 0x80, 0, 0, 0), func_0890BD30(this + 0x90); this[0x68] = D_08CD70F8, this[0x69] = this[0x6A] =
// this[0x6B] = 0 (the words at +0x1A0 .. +0x1AC), func_08B8EA8C(this + 0x1A4, 8), func_0886A658(this + 0x1B0) and this[0x1C4] (byte) = 1; v0 = 1.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08869988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t saved_s0 = ctx.gpr[16];
    ctx.gpr[16] = self;  // the original keeps this in s0; the lock created by func_08B28DCC stores the caller's s0 (see func_0880BF70)
    (void)call_keep(rt, ctx, 0x08B28DCCu, {self});
    ctx.gpr[4] = self + 0x28u;
    (void)call_keep(rt, ctx, 0x088B07B8u, {self + 0x28u});
    ctx.gpr[4] = self + 0x74u;
    (void)call_keep(rt, ctx, 0x0887AF08u, {self + 0x74u});
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0u;
    ctx.gpr[5] = 0u;
    ctx.gpr[4] = self + 0x80u;
    (void)call_keep(rt, ctx, 0x08B04D98u, {self + 0x80u, 0u, 0u, 0u});
    ctx.gpr[4] = self + 0x90u;
    (void)call_keep(rt, ctx, 0x0890BD30u, {self + 0x90u});
    ctx.gpr[2] = 0x08CD70F8u;
    ctx.gpr[3] = self + 0x1A4u;
    put(rt, self + 0x1A0u, 0x08CD70F8u);
    ctx.gpr[4] = self + 0x1A4u;
    ctx.gpr[5] = 8u;
    put(rt, self + 0x1A4u, 0u);
    put(rt, self + 0x1A8u, 0u);
    put(rt, self + 0x1ACu, 0u);
    (void)call_keep(rt, ctx, 0x08B8EA8Cu, {self + 0x1A4u, 8u});
    ctx.gpr[4] = self + 0x1B0u;
    (void)call_keep(rt, ctx, 0x0886A658u, {self + 0x1B0u});
    ctx.gpr[2] = 1u;
    rt.memory().store8(self + 0x1C4u, 1u);
    ctx.gpr[16] = saved_s0;
}

// func_0886978C(this, a1): under the lock this (func_08B344E0 .. the tail call func_08B3457C(this)): o = func_0886A020(this + 0x1B0, D_08C9F650, 0); a non-null o gets func_0886E008(o, a1). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_0886A020 always answers an object (the null answer is not taken).
void func_0886978C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B344E0u, {self});
        ctx.gpr[5] = 0x08C9F650u;
        ctx.gpr[4] = self + 0x1B0u;
        ctx.gpr[6] = 0u;
        const std::uint32_t object = call_keep(rt, ctx, 0x0886A020u, {self + 0x1B0u, 0x08C9F650u, 0u});
        ctx.gpr[4] = object;
        ctx.gpr[5] = a1;
        if (object != 0u) (void)call_keep(rt, ctx, 0x0886E008u, {object, a1});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B3457Cu, {self});
}

// func_0880BF70(this, a1): func_08B28DCC(this); func_0880BEC4(this + 0x28, func_089BD4DC(a1)); func_08869988(this + 0x30); this[0x81] = a1 and this[0x80] = 0 (words +0x204 / +0x200); the tail call is func_08A76AA0(this + 0x208).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
// s0 / s1 are set to this / a1 around the calls like the original keeps them: with the entry values in them the first gate run mismatched on both cases (a callee stored s0, 306 bytes of RAM, FINDINGS);
// they are restored before the tail call because the original restores them from its frame.
void func_0880BF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t saved_s0 = ctx.gpr[16], saved_s1 = ctx.gpr[17];
        ctx.gpr[16] = self;
        ctx.gpr[17] = a1;
        (void)call_keep(rt, ctx, 0x08B28DCCu, {self});
        ctx.gpr[4] = a1;
        const std::uint32_t value = call_keep(rt, ctx, 0x089BD4DCu, {a1});
        ctx.gpr[5] = value;
        ctx.gpr[4] = self + 0x28u;
        (void)call_keep(rt, ctx, 0x0880BEC4u, {self + 0x28u, value});
        ctx.gpr[4] = self + 0x30u;
        (void)call_keep(rt, ctx, 0x08869988u, {self + 0x30u});
        put(rt, self + 0x204u, a1);
        ctx.gpr[4] = self + 0x208u;
        put(rt, self + 0x200u, 0u);
        ctx.gpr[16] = saved_s0;
        ctx.gpr[17] = saved_s1;
    }
    tail_call(ctx, 0x08A76AA0u, {self + 0x208u});
}

// func_0880BB94(this, p, q, f12): under the lock this (func_08B344E0 .. the tail call func_08B3457C(this)): func_08869B28(this + 0x30, *p, *q) with f12 passed on.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0880BB94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[5], q = ctx.gpr[6];
    const float angle = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_keep(rt, ctx, 0x08B344E0u, {self});
        ctx.fpr[12] = angle;
        ctx.gpr[5] = word(rt, p);
        ctx.gpr[6] = word(rt, q);
        ctx.gpr[4] = self + 0x30u;
        (void)call_keep(rt, ctx, 0x08869B28u, {self + 0x30u, ctx.gpr[5], ctx.gpr[6]}, {angle});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B3457Cu, {self});
}

// func_0880BA98(this, a1): under the lock this (func_08B344E0 .. the tail call func_08B3457C(this)): func_0886978C(this + 0x30, a1).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0880BA98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B344E0u, {self});
        ctx.gpr[4] = self + 0x30u;
        ctx.gpr[5] = a1;
        (void)call_keep(rt, ctx, 0x0886978Cu, {self + 0x30u, a1});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B3457Cu, {self});
}

// func_0880B5E8(this, a1): func_0886923C(this + 0x30, this + 0x28), func_088692E8(this + 0x30) and this[0x80] (word +0x200) = a1. v0 is func_088692E8's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_0880B5E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[4] = self + 0x30u;
    ctx.gpr[5] = self + 0x28u;
    (void)call_keep(rt, ctx, 0x0886923Cu, {self + 0x30u, self + 0x28u});
    ctx.gpr[4] = self + 0x30u;
    (void)call_keep(rt, ctx, 0x088692E8u, {self + 0x30u});
    put(rt, self + 0x200u, a1);
}

}  // namespace
}  // namespace hot_batch_84
}  // namespace gt::game

#define HOT84(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_84::name, gt::ReturnKind::kind)
#define HOT84N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_84::name, gt::ReturnKind::kind)
HOT84(0x08AF219Cu, func_08AF219C, Word);
HOT84(0x08A8A464u, func_08A8A464, Word);
HOT84(0x0886A8B8u, func_0886A8B8, Word);
HOT84(0x08BEEED0u, func_08BEEED0, Word);
HOT84(0x08966BF0u, func_08966BF0, Word);
HOT84(0x0886BF44u, func_0886BF44, Word);
HOT84(0x08B8E0D8u, func_08B8E0D8, Word);
HOT84(0x0886AA0Cu, func_0886AA0C, Word);
HOT84(0x08AA19DCu, func_08AA19DC, Word);
HOT84(0x08BAEEBCu, func_08BAEEBC, Word);
// Not registered (compared 0 in the fast gate): HOT84(0x08B6E4FCu, func_08B6E4FC, Word);
// Not registered (compared 0 in the fast gate): HOT84(0x08B6E268u, func_08B6E268, DoubleWord);
HOT84(0x0889FBA0u, func_0889FBA0, Word);
HOT84(0x08C20060u, func_08C20060, Word);
HOT84(0x08BE4120u, func_08BE4120, Word);
HOT84(0x08B644BCu, func_08B644BC, Word);
HOT84(0x08B0EB48u, func_08B0EB48, Word);
HOT84(0x08AA96B4u, func_08AA96B4, Word);
// Not registered (compared only on its op 0 early exit): HOT84(0x08A1F520u, func_08A1F520, Word);
HOT84(0x08979A6Cu, func_08979A6C, Word);
// Not registered (every case stops at a blocking import (not comparable)): HOT84(0x089C4C20u, func_089C4C20, Word);
HOT84(0x08966E24u, func_08966E24, Word);
HOT84(0x08894444u, func_08894444, Word);
HOT84(0x08893F7Cu, func_08893F7C, Word);
HOT84(0x08BF8A7Cu, func_08BF8A7C, Word);
// Not registered (compared only on its empty-range early exit): HOT84(0x08BDD7D8u, func_08BDD7D8, Word);
HOT84(0x08B1BC98u, func_08B1BC98, Word);
HOT84(0x08A20D94u, func_08A20D94, Word);
HOT84(0x08C46508u, func_08C46508, Word);
HOT84(0x08BAE7A0u, func_08BAE7A0, Word);
HOT84(0x08B9D834u, func_08B9D834, Word);
HOT84(0x08A81BF4u, func_08A81BF4, Word);
HOT84N(0x088AA698u, func_088AA698, Word);
HOT84(0x088A3750u, func_088A3750, Word);
// Not registered (the tail call reaches func_088A599C, CarParameter::set_color, registered Void and nested: its decompiled run leaves the entry v0 where the original leaves 1, and this function hands that v0 on; 3 full-gate cases mismatched on v0): HOT84(0x08881C38u, func_08881C38, Word);
HOT84(0x0894D094u, func_0894D094, Word);
HOT84(0x08C283A8u, func_08C283A8, Word);
HOT84(0x08C1C89Cu, func_08C1C89C, Word);
HOT84(0x08C24530u, func_08C24530, Word);
HOT84(0x08A03BB8u, func_08A03BB8, Word);
HOT84(0x089B82A0u, func_089B82A0, Word);
HOT84N(0x088B07B8u, func_088B07B8, Word);
HOT84(0x0887EE40u, func_0887EE40, Word);
// Not registered (compared 0 in the fast gate): HOT84(0x0887BC90u, func_0887BC90, Word);
// Not registered (compared 0 in the fast gate (its callers never ran in the routes' window)): HOT84(0x08875038u, func_08875038, Word);
HOT84N(0x08870870u, func_08870870, Word);
HOT84N(0x0886E008u, func_0886E008, Word);
// Not registered (compared 0 in the fast gate): HOT84(0x0886C31Cu, func_0886C31C, Word);
HOT84(0x0886ADECu, func_0886ADEC, Word);
HOT84N(0x08869B28u, func_08869B28, Word);
HOT84N(0x08869988u, func_08869988, Word);
HOT84N(0x0886978Cu, func_0886978C, Word);
HOT84(0x0880BF70u, func_0880BF70, Word);
HOT84(0x0880BB94u, func_0880BB94, Word);
HOT84(0x0880BA98u, func_0880BA98, Word);
HOT84(0x0880B5E8u, func_0880B5E8, Word);
