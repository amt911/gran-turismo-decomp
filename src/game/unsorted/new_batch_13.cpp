// New batch 13 (drafted by the delegate LLM, reviewed against the asm): the twenty-three to twenty-five instruction functions
// of `drafts/b13_candidates.txt` that the emitter batches wrote register-level and never registered, rewritten here as
// structured C (named locals, if/while, `G[n]` only at the call boundary) so the synthetic gate can verify them.
// Every body is read from `tools.disasm` address by address; a delay slot runs before its branch or call (a branch-likely
// delay slot only when taken), the callee-saved registers are set where the asm sets them and put back by an RAII helper
// (a callee may read them), frames are the original's `addiu sp, sp, -N`, and `v0` (with `v1` / `f0` where the code after a
// call reads them) ends as the asm leaves it on every path. No names: none of these has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_13 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

// A `jal`: afterwards `v0` holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same where the code after the call reads `v1` or `f0`: those are the callee's (a callee that never writes v0 hands the
// caller's own v0 on, which `call_guest` starts it with).
GuestResult call_keeping(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                         std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate (the disasm prints the unrelocated half).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
s32 sign_extend8(u32 value) { return static_cast<s8>(value & 0xFFu); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
// Allegrex `wsbw`: the four bytes of a word reversed (records in this engine are stored big-endian).
u32 wsbw(u32 value) { return __builtin_bswap32(value); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// Saves the caller's callee-saved registers selected by `mask` (bit n = s_n) and restores them on scope exit.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, u32 mask) : ctx_(ctx), mask_(mask) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i)
            if (mask_ & (1u << i)) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 mask_;
    u32 saved_[8];
};

// The same for one float register (f20 and up are callee-saved).
class SavedFloat {
public:
    SavedFloat(AllegrexContext &ctx, unsigned reg) : ctx_(ctx), reg_(reg), value_(ctx.fpr[reg]) {}
    ~SavedFloat() { ctx_.fpr[reg_] = value_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned reg_;
    float value_;
};

// --- 92-byte functions ----------------------------------------------------------------------------------------------

// 0x08B340D0: a counter at 0x08D69274: while it is positive, the block (0x08CB54FC, 0x02415822, &local, 4, 0, 0) is finished by
// 0x08C7A820 and the counter's value is released by 0x08C7A9C0, then the counter is cleared.

// 0x08B3EA88 (a0 = object, a1 = a value): index = trunc(100 * a1 / word(a0 + 0x644)) clamped to [.., count - 1], 1 when not
// positive; v0 / v1 = the pair at a0 + 0x320 + index * 8 (the arithmetic is the FPU's: divide, multiply, truncate toward zero).
void func_08B3EA88(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 count = static_cast<s32>(word(rt, self + 0x644u));
    const s32 value = static_cast<s32>(ctx.gpr[5]);
    ctx.fpr[1] = static_cast<float>(value);
    ctx.fpr[0] = static_cast<float>(value);
    ctx.fpr[2] = static_cast<float>(count);
    ctx.fpr[1] = 100.0f;
    ctx.fpr[0] = psp_mul(ctx.fpr[0] / ctx.fpr[2], ctx.fpr[1]);
    const s32 scaled = trunc_w_s(ctx.fpr[0]);
    ctx.fpr[0] = std::bit_cast<float>(static_cast<u32>(scaled));  // `trunc.w.s` leaves the integer's bits in f0 (`mfc1`)
    s32 index;
    if (scaled > 0) {
        index = scaled < count ? scaled : count - 1;
    } else {
        index = 1;
    }
    const u32 element = self + static_cast<u32>(index) * 8u;
    ctx.gpr[2] = word(rt, element + 0x320u);
    ctx.gpr[3] = word(rt, element + 0x324u);
}

// 0x08B4257C (a0 = object, a2 passed on): 0x08B3FF90(&local, a2) and 0x08B3FA3C(&local, 0x4E9C) fill a local block, then the
// object's helper at +0xB0 runs its virtual +0x60 with (helper, 0x2714, self + 0x990, 0x348, &local, 1).
void func_08B4257C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 third = ctx.gpr[6];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = self;
    const u32 local = frame.at(0u);
    call(rt, ctx, 0x08B3FF90u, {local, third});
    call(rt, ctx, 0x08B3FA3Cu, {local, 0x00004E9Cu});
    const u32 helper = word(rt, self + 0xB0u);
    const u32 function = word(rt, word(rt, helper) + 0x60u);
    call_indirect(rt, ctx, function, {helper, 0x00002714u, self + 0x990u, 0x00000348u, local, 1u});
}

// 0x08B8DA64 (a1 = a slot holding {a value, a pointer}): when the pointer exists, its first virtual is called with the helper and
// the held value and both words are cleared; the slot is then released by 0x08B64654 (a tail call). A null slot returns the
// caller's v0 untouched (the asm's early exit writes no v0).
void func_08B8DA64(Runtime &rt, AllegrexContext &ctx) {
    const u32 slot = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, 1u);  // s0
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = slot;
        if (slot == 0u) return;
        const u32 held = word(rt, slot + 4u);
        if (held != 0u) {
            const u32 function = word(rt, word(rt, held));
            call_indirect(rt, ctx, function, {held, word(rt, slot)});
            put(rt, slot + 4u, 0u);
            put(rt, slot, 0u);
        }
    }
    tail_call(ctx, 0x08B64654u, {slot});
}

// 0x08C15DB8 (a0 = this): constructor - vtable word 0x08CD04B0, field +0x14 cleared, two members set up (0x08B28BE0 at +0xD8 and
// +0xAC), 0x08A1A9A0 at +0x18, the base (0x08A2A4B0), and a release of 0x1B0 bytes with alignment 4 as a tail call.

// 0x08C47150 (a0 = container with an end pointer at +4, a1 = an element): when the container's end is not a1 + 4, the range
// [a1 + 4, end) is moved down over a1 (0x08B79BAC), then the end shrinks by one element; v0 = a1.
void func_08C47150(Runtime &rt, AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 element = ctx.gpr[5];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = element;
    ctx.gpr[17] = container;
    const u32 next = element + 4u;
    const u32 end = word(rt, container + 4u);
    if (end != next) call(rt, ctx, 0x08B79BACu, {element, next, end - next});
    put(rt, container + 4u, word(rt, container + 4u) - 4u);
    ctx.gpr[2] = element;
}

// 0x08C4AD04 (a0 = source, a1 = end, a2 = destination): copies records of 0x70 bytes from [a0, a1) to a2 and answers the end of
// the destination. A null destination skips its first record (the asm tests the destination at the loop head only once) and
// writes from 0x70 on; v0 = a2 + records * 0x70.

// An adjustor call: 0x08C5477C(self + the vtable's -0xC word, 0xA) answers a value that 0x08B5FB4C(self, a1, a2, value) gets as
// its fourth argument (a tail call).
void adjusted_tail(Runtime &rt, AllegrexContext &ctx, u32 tail_target) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    u32 adjusted;
    {
        SavedRegisters saved(ctx, 7u);  // s0, s1, s2
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = third;
        ctx.gpr[17] = second;
        ctx.gpr[16] = self;
        adjusted = call(rt, ctx, 0x08C5477Cu, {self + word(rt, word(rt, self) - 0xCu), 0x0000000Au});
    }
    tail_call(ctx, tail_target, {self, second, third, adjusted});
}

// A table lookup by key: 0x08B5C2B0(key) answers an index; `self` holds the entries at +4 and the count at +8. The entry is
// answered when the index is in range and set, otherwise 0x08B5A9FC throws (the original has no code after it).
void facet(Runtime &rt, AllegrexContext &ctx, u32 key) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 index = call(rt, ctx, 0x08B5C2B0u, {key});
    ctx.gpr[4] = index;
    const u32 table = word(rt, self);
    const u32 entries = word(rt, table + 4u);
    ctx.gpr[3] = entries;
    if (index < word(rt, table + 8u)) {
        const u32 entry = word(rt, entries + index * 4u);
        ctx.gpr[2] = entry;
        if (entry != 0u) return;
    }
    call(rt, ctx, 0x08B5A9FCu, {index});
}
void func_08C5AC7C(Runtime &rt, AllegrexContext &ctx) { facet(rt, ctx, 0x08D77484u); }
void func_08C5AD90(Runtime &rt, AllegrexContext &ctx) { facet(rt, ctx, 0x08D7747Cu); }
void func_08C5ADEC(Runtime &rt, AllegrexContext &ctx) { facet(rt, ctx, 0x08D77480u); }

// A call through a {word, byte} pair: the pair at a1 is copied into a local (word at +0, byte at +4) and the target gets
// (a0, &local, a2, a3 sign-extended to a byte). Afterwards the callee's v1 low byte lands at local + 4 and is read back as the
// word starting there (the rest of those four bytes is the frame's stale content, as in the original), s0 keeps v0, and
// v0 = s0 / v1 = that word.
void call_through_pair(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = static_cast<u32>(sign_extend8(ctx.gpr[7]));
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[7] = fourth;
    const u32 local = frame.at(8u);
    rt.memory().store8(frame.at(0xCu), rt.memory().load8(pair + 4u));
    put(rt, local, word(rt, pair));
    const GuestResult result = call_keeping(rt, ctx, target, {self, local, third, fourth});
    ctx.gpr[16] = result.v0;                            // s0
    put(rt, frame.at(0x14u), result.v1);                // the callee's v1
    rt.memory().store8(frame.at(4u), rt.memory().load8(frame.at(0x14u)));
    ctx.gpr[2] = ctx.gpr[16];
    ctx.gpr[3] = word(rt, frame.at(4u));
}

// --- 96-byte functions ----------------------------------------------------------------------------------------------

// 0x0880BAD8 (a0 = object, a1 = a slot, a2 = a slot, f12 = a scale): 0x08B344E0 locks the object, 0x08869BB4(self + 0x30, the
// words at a1 and a2) gets the scale as f12, and 0x08B3457C unlocks it (a tail call). f20 carries the scale over the calls.

// Three functions (a0 unused, a1 = a slot, a2 = a slot): 0x088297A8 fills a local smart pointer; the object it holds is
// 0x088294D0's argument, the local is released (0x08A5B9EC), and the object goes through a converter and then the function's own
// second step. A null object trips the assert at 0x08A5BB7C with 0x08C9A2F8 first.
void slot_conversion(Runtime &rt, AllegrexContext &ctx, u32 convert, u32 finish) {
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x088297A8u, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 found = call(rt, ctx, 0x088294D0u, {object});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, convert, {found});
    call(rt, ctx, finish, {ctx.gpr[2]});
}

// Three functions (a0 = destination slot, a1 = a count, a2 = a key): for a positive count the key goes through 0x0883B848 or
// 0x0883B950 (which answers a pair in v0/v1), the pair through the row lookup, and the result is wrapped into a local that the
// destination receives (0x08A5BAD8) unless it is the local itself; the local is then released.
void keyed_lookup(Runtime &rt, AllegrexContext &ctx, u32 pair_of, u32 lookup) {
    const u32 destination = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = destination;
    if (count > 0) {
        const GuestResult pair = call_keeping(rt, ctx, pair_of, {key, count});
        const u32 value = call(rt, ctx, lookup, {pair.v0, pair.v1});
        call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), value});
        if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
}
void func_0883D8C8(Runtime &rt, AllegrexContext &ctx) { keyed_lookup(rt, ctx, 0x0883B848u, 0x08930E28u); }
void func_0883F8E8(Runtime &rt, AllegrexContext &ctx) { keyed_lookup(rt, ctx, 0x0883B950u, 0x0893067Cu); }

// 0x088824F8 (a0 = object with a handler at +0xC, a1 = a block of four floats or 0): bit 0x10 of the handler's word 0x16E0 is set
// and the four floats go to the handler's 0xDA8 / 0xE60 / 0xF18 / 0xFD0; a null block clears the bit instead.
void func_088824F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 handler = word(rt, self + 0xCu);
    if (source == 0u) {
        const u32 flags = word(rt, handler + 0x16E0u);
        ctx.gpr[2] = flags & ~0x10u;  // `ins v0, zero, 4, 1`
        ctx.gpr[3] = handler;
        put(rt, handler + 0x16E0u, flags & ~0x10u);
        return;
    }
    put(rt, handler + 0x16E0u, word(rt, handler + 0x16E0u) | 0x10u);
    put_float(rt, word(rt, self + 0xCu) + 0xDA8u, float_at(rt, source));
    put_float(rt, word(rt, self + 0xCu) + 0xE60u, float_at(rt, source + 4u));
    put_float(rt, word(rt, self + 0xCu) + 0xF18u, float_at(rt, source + 8u));
    ctx.fpr[0] = float_at(rt, source + 0xCu);
    ctx.gpr[2] = word(rt, self + 0xCu);
    ctx.gpr[3] = word(rt, self + 0xCu);  // v1 = the handler, as the third store's `lw v1, 0xC(a2)` leaves it
    put_float(rt, word(rt, self + 0xCu) + 0xFD0u, ctx.fpr[0]);
}

// 0x088909BC (a0 = object, a1 = an index): 0x0888F550(self) answers a record; a nonzero word +4 selects the byte at +0xDC alone,
// a zero one adds word(*self) to it; the answer is that byte-reversed value plus index * 32.
void func_088909BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = index;
    const u32 record = call(rt, ctx, 0x0888F550u, {self});
    const u32 selector = wsbw(word(rt, record + 4u));
    ctx.gpr[2] = selector;
    const u32 base = wsbw(word(rt, record + 0xDCu));
    ctx.gpr[3] = base;
    ctx.gpr[2] = base + (selector == 0u ? word(rt, self) : 0u) + (index << 5);
}

// 0x08893DD0 (a0 = holder, a1 = an output record): the count at word(*a0) + 4 is stored at a1 and that many 32-byte rows of
// word(*a0) + 8 are copied after it (0x08B79A88); v0 = a1 + 4 + count * 32.
void func_08893DD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = out + 4u;
    const u32 container = word(rt, self);
    put(rt, out, word(rt, container + 4u));
    call(rt, ctx, 0x08B79A88u, {out + 4u, word(rt, container + 8u), word(rt, container + 4u) << 5});
    const u32 row_count = word(rt, word(rt, self) + 4u);
    ctx.gpr[2] = out + 4u + (row_count << 5);
}

// 0x088BBCF8 (a0 unused, a1 = an object, a2 = an output float or 0): 0x088BBC40(a1) decides; when it answers, an output is asked
// for through 0x088BBC70(a1, 1) and the object's float field copied to it, and f0 = the float at word(a1 + 0x24) + 0x1C. Nothing
// found leaves f0 = 0.
void float_field_copy(Runtime &rt, AllegrexContext &ctx, u32 selector, u32 field) {
    const u32 object = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = object;
    ctx.gpr[17] = out;
    const u32 found = call(rt, ctx, 0x088BBC40u, {object});
    ctx.fpr[0] = 0.0f;
    if (found == 0u) return;
    if (out != 0u) {
        const u32 entry = call(rt, ctx, 0x088BBC70u, {object, selector});
        put_float(rt, out, float_at(rt, entry + 0x14u));
    }
    const u32 holder = word(rt, object + 0x24u);
    ctx.gpr[2] = holder;
    ctx.fpr[0] = float_at(rt, holder + field);
}
void func_088BBD58(Runtime &rt, AllegrexContext &ctx) { float_field_copy(rt, ctx, 0u, 0x18u); }

// 0x088D2BC8 (a0 = holder, f12 = a scale): 0x088DB5C4(word(a0)) decides; when it answers, 0x088DB5C4(word(a0)) is called again
// and the answer goes on to 0x088FB0C8(a0 = answer, f12 = the scale) as a tail call. f0 = the scale on the way out.
void func_088D2BC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    SavedRegisters saved(ctx, 1u);  // s0
    SavedFloat saved_f20(ctx, 20u);
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[20] = value;
    ctx.gpr[16] = self;
    const u32 found = call(rt, ctx, 0x088DB5C4u, {word(rt, self)});
    ctx.fpr[0] = ctx.fpr[20];
    if (found == 0u) return;
    const u32 target = call(rt, ctx, 0x088DB5C4u, {word(rt, self)});
    ctx.fpr[12] = ctx.fpr[20];
    ctx.gpr[2] = target;  // v0 survives to the tail call
    tail_call(ctx, 0x088FB0C8u, {target});
}

// 0x088DE874 (a0 = object): 0x088D9E58(self) selects the byte to test - a zero answer tests +0x20 (0x088DC48C), a nonzero one
// +0x21 (0x088DE740) - and 0x088D9E58(self) runs again.
void func_088DE874(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 first = call(rt, ctx, 0x088D9E58u, {self});
    if (first == 0u) {
        if (rt.memory().load8(self + 0x20u) != 0u) call(rt, ctx, 0x088DC48Cu, {self});
    } else {
        if (rt.memory().load8(self + 0x21u) != 0u) call(rt, ctx, 0x088DE740u, {self});
    }
    call(rt, ctx, 0x088D9E58u, {self});
}

// 0x088E4754 (a0 = object holding a lock at +0, a1 = an offset): the lock at word(self) + 0x1C8 is taken (0x08B344E0), the signed
// byte at a1 + word(self) + 0x460 is read and cleared, the lock is released (0x08B3457C); v0 = that byte, sign-extended.

// 0x088F2D38 (a0 = an array header {unused, rows at +4, first index at +8 as a halfword}): walks the rows of 64 bytes: a row's
// product of its floats at +0xC and +0x10 replaces the smallest so far (start 1e7) and the row's index is the answer; the next
// index is the row's word +0 sign-extended from 13 bits, and the walk ends on a negative one. v0 = the smallest row's index.
void func_088F2D38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    s32 index = signed_half_at(rt, self + 8u);
    if (index < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 rows = word(rt, self + 4u);
    s32 best = 0;
    ctx.fpr[2] = 10000000.0f;
    for (;;) {
        const u32 row = rows + (static_cast<u32>(index) * 64u);  // `sll v0, index, 6`: 64-byte rows
        ctx.fpr[1] = float_at(rt, row + 0x10u);
        ctx.fpr[0] = float_at(rt, row + 0xCu);
        const s32 next = static_cast<s32>(word(rt, row) << 19) >> 19;
        ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]);
        if (ctx.fpr[0] < ctx.fpr[2]) {
            ctx.fpr[2] = ctx.fpr[0];
            best = index;
        }
        ctx.gpr[3] = static_cast<u32>(next);
        ctx.gpr[5] = static_cast<u32>(next);
        if (next < 0) break;
        index = next;
    }
    ctx.gpr[2] = static_cast<u32>(best);
}

// 0x08905F54 (a0 = object, a1 passed on): the words at +4, +8, +0xC and +0x10 are each handed to 0x08905F30 with a1, and the
// word at +0x14 goes to it as a tail call (each read happens after the previous call, as in the asm).
void func_08905F54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        for (u32 offset = 4u; offset <= 0x10u; offset += 4u) call(rt, ctx, 0x08905F30u, {word(rt, self + offset), argument});
    }
    tail_call(ctx, 0x08905F30u, {word(rt, self + 0x14u), argument});
}

// 0x08978080 (a1 = a count, a2 = a slot): a count of zero or less answers 1. Otherwise the object at word(a2) - assert
// 0x08CA2F84 when it is null - answers its virtual +0x54 as a number of minutes: v0 = trunc(f0 * 60), which 0x089C4558 gets
// as its argument (a tail call).

// 0x08A4CD1C (a0 = object): 0x08B2EEB8(0x08CF68F0) sets a resource up, 0x08B62344(0x2C) answers a block that 0x08A50A7C
// initializes and the object keeps at +0x78, then 0x08A50400 runs it and 0x08B6461C(0x1000) answers a second block kept at +0x98.

// 0x08AA6178 (a0 = object, a1 = a slot): 0x08AA5E90(self, word(word(slot) - 0xC)) runs first; then the word at
// word(slot) - 0xC is the third argument of the tail call 0x08AA5D2C(self, word(slot), it). A zero third argument answers 0.
void func_08AA6178(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    u32 object;
    u32 offset;
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self;
        ctx.gpr[16] = slot;
        call(rt, ctx, 0x08AA5E90u, {self, word(rt, word(rt, slot) - 0xCu)});
        object = word(rt, slot);
        offset = word(rt, object - 0xCu);
        ctx.gpr[2] = offset;
        if (offset == 0u) return;
    }
    tail_call(ctx, 0x08AA5D2Cu, {self, object, offset});
}

// 0x08AC7428 (a0 = object, a1 = an argument): the lock is taken (0x08B344E0(self)), 0x08C7AB58(word(self + 0x2C), a1) decides:
// a negative answer marks the object (byte +0x28 = 1) and answers 0, anything else answers 1; the lock is released.

// 0x08B098A8 (a0 = object): while the word at 0x08D4744C is nonzero, 0x08B2EFDC(-1, -1) runs and, if the word is still nonzero,
// 0x08B09824(self) runs; the word is the loop's only condition.

// 0x08B2DD88: a handle at 0x08CE0C78 that is not negative is closed (0x08C7AF68) and set to -1, then a positive counter at
// 0x08D68D64 is released (0x08C7A9C0) and cleared.

// 0x08B343D0: 0x08B3433C(&local) fills a 16-byte block; for a negative answer v0 / v1 = -1, otherwise v0 / v1 is a 64-bit
// product of the block's words (word +8 * word +0xC, then that low word times word +4 with word +0xC * the high word added).

// 0x08B58E84 (a0 = object or 0): 0x08C7AAD8(a0) and 0x08C7AC48(0) set the graphics state up; the object is the argument's, or the
// one at 0x08D6CE00 + 0x2D4 when it is null. 0x08C7AC40(word(object + 8), 0, word(0x08D6CE00 + 0x30), 0) answers a handle that
// 0x08D6CE00 + 0x34 keeps.

// 0x08BDDC68 (a0 = first record, a1 = end): each 16-byte record of [a0, a1) is handed to 0x08BDD7D8(word +4, word +8, &local)
// and the record's word +4, when it is still set, released by 0x08B35254.

// Five functions (a0 = the list's sentinel): every node before the sentinel has its object at +8 released (0x08A5B9EC) and the
// node itself freed by 0x08A5BC54(node, 0xC, 4), until the walk returns to the sentinel.
void destroy_list_nodes(Runtime &rt, AllegrexContext &ctx) {
    const u32 sentinel = ctx.gpr[4];
    SavedRegisters saved(ctx, 7u);  // s0, s1, s2
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = sentinel;
    u32 node = word(rt, sentinel);
    ctx.gpr[17] = node;
    u32 next = word(rt, node);
    for (;;) {
        ctx.gpr[16] = next;
        call(rt, ctx, 0x08A5B9ECu, {node + 8u});
        call(rt, ctx, 0x08A5BC54u, {node, 0x0000000Cu, 4u});
        node = next;
        ctx.gpr[17] = node;
        if (node == sentinel) break;
        next = word(rt, node);
    }
}

// 0x08C0E814 (a0 = first element, a1 = end, a2 = an element to skip): every 12-byte element of [a0, a1) except a2 goes to
// 0x08C0E654(element, a2); v0 = the end of the walk.

// 0x08C30DAC (a0 = first source word, a1 = end, a2 = destination or 0): each source word goes to 0x08B88F44(destination, word)
// and the destination advances by four regardless; v0 = a2 + 4 * words.

// 0x08C3EB78 (a0 = first slot, a1 = end, a2 = an argument): each 8-byte slot's object at +0 answers its first virtual, called
// with (object, a2); v0 = a2.

// 0x08C4B568 (a0 = first record, a1 = end, a2 = destination or 0): copies records of 0x18 bytes from [a0, a1) to a2 (a null
// destination skips the copy of its first record) and answers the end of the destination.
void func_08C4B568(Runtime &rt, AllegrexContext &ctx) {
    u32 source = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (source != end) {
        for (;;) {
            const u32 next = source + 0x18u;
            if (destination != 0u) {
                const u32 w0 = word(rt, source);
                const u32 w1 = word(rt, source + 4u);
                const u32 w2 = word(rt, source + 8u);
                const u32 w3 = word(rt, source + 0xCu);
                const u32 w4 = word(rt, source + 0x10u);
                const u32 w5 = word(rt, source + 0x14u);
                put(rt, destination, w0);
                put(rt, destination + 4u, w1);
                put(rt, destination + 8u, w2);
                put(rt, destination + 0xCu, w3);
                put(rt, destination + 0x10u, w4);
                put(rt, destination + 0x14u, w5);
            }
            source = next;
            destination += 0x18u;
            if (end == next) break;
        }
    }
    ctx.gpr[2] = destination;
}

// Four functions (a0 = a 3-word container): the container is emptied into a local {unused, word +0, word +4, word +8} and the
// target gets (word +0, word +4, &local); the word at +0 the call leaves in the local is released by 0x08B35254 when it is set.
void container_step(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 first = word(rt, self);
    const u32 second = word(rt, self + 4u);
    const u32 third = word(rt, self + 8u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, frame.at(4u), first);
    put(rt, frame.at(8u), second);
    put(rt, frame.at(0xCu), third);
    call(rt, ctx, target, {first, second, frame.at(0u)});
    const u32 leftover = word(rt, frame.at(4u));
    ctx.gpr[2] = leftover;  // `lw v0, 0x4(sp)` before the release branch
    if (leftover != 0u) call(rt, ctx, 0x08B35254u, {leftover});
}
void func_08C4B800(Runtime &rt, AllegrexContext &ctx) { container_step(rt, ctx, 0x08C498D4u); }
void func_08C4B860(Runtime &rt, AllegrexContext &ctx) { container_step(rt, ctx, 0x08C499B0u); }

// 0x08C769D8 (a0 = an output word, a1 = a slot, a2 = an offset, a3 = a count, t0 = an argument): when the object at word(a1)
// holds at least `offset` bytes, `count` bytes from it are handed to 0x08C76750(base + offset, offset + base + count, t0) and
// its answer is stored at a0; a range past the object's end throws 0x08B5AEDC(0x08CB594C).
void func_08C769D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    const u32 offset = ctx.gpr[6];
    u32 count = ctx.gpr[7];
    const u32 extra = ctx.gpr[8];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = out;
    const u32 base = word(rt, word(rt, slot));
    const u32 size = word(rt, base - 0xCu);
    if (size < offset) {
        call(rt, ctx, 0x08B5AEDCu, {0x08CB594Cu});
        return;
    }
    const u32 remaining = size - offset;
    if (count > remaining) count = remaining;  // `movz a3, remaining, (count < remaining)`
    put(rt, out, call(rt, ctx, 0x08C76750u, {base + offset, offset + base + count, extra}));
}

// --- 100-byte functions ---------------------------------------------------------------------------------------------

// 0x08805B1C (a0 = object, a1 = an argument): 0x08B2E434(self, a1), then 0x08B36264(word(self), a1), then 0x08805C70(),
// 0x08B19FA4() and 0x08B19F10(its answer, &local, 0) with two cleared words in front of it.

// 0x08805C0C: the teardown sequence 0x08B1F12C, 0x08AD0878, 0x08B0EC2C, 0x08AD0830, 0x08B340D0, 0x08B336F0, 0x08B2DD88,
// 0x08B33294, 0x08AD0828, 0x08AD08D0, then 0x08AD096C as a tail call - all of them without arguments.

// 0x0881B950 (a0, a1 = two objects, a2, a3 = two output words): 0x0881B820(first, second, 1) is stored at a2 and
// 0x0881B820(second, first, 1) at a3.

// 0x0884D550 (a0 passed on, a1 = a slot): 0x0884D3D8(a0, a1) answers an object; the slot's object - assert 0x08C9D158 when
// null - answers its virtual +0xDC with (object, 0), and the pair (the first answer, that one) goes to 0x088972BC as a tail call.

// 0x0885ECF4 (a0 = object, a1 = an argument, a2 = an index; negative when not given): a negative index is looked up first by
// 0x088F8A14(word(word(self) + 0x5B4)) and a still negative one answers 0. 0x088F8D0C(that, a1, index) then answers the entry;
// v0 = entry + 0x20, or 0 when there is none.
void func_0885ECF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    s32 index = static_cast<s32>(ctx.gpr[6]);
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = argument;
    if (index < 0) {
        index = static_cast<s32>(call(rt, ctx, 0x088F8A14u, {word(rt, word(rt, self) + 0x5B4u)}));
        if (index < 0) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    const u32 entry = call(rt, ctx, 0x088F8D0Cu, {word(rt, word(rt, self) + 0x5B4u), argument, static_cast<u32>(index)});
    ctx.gpr[2] = entry != 0u ? entry + 0x20u : 0u;
}

// 0x08860AD8 (a0 = object, a1 = an argument, a2 = a block or 0): when the block exists and its signed byte +3 is not -1,
// 0x08860AD0(self, a1, that code) runs; then 0x088D7560(word(word(self) + 0x5AC), a1, the block) is the tail call.
void func_08860AD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 block = ctx.gpr[6];
    {
        SavedRegisters saved(ctx, 7u);  // s0, s1, s2
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = argument;
        ctx.gpr[17] = self;
        ctx.gpr[16] = block;
        if (block != 0u) {
            const s32 code = sign_extend8(rt.memory().load8(block + 3u));
            if (code != -1) call(rt, ctx, 0x08860AD0u, {self, argument, static_cast<u32>(code)});
        }
    }
    tail_call(ctx, 0x088D7560u, {word(rt, word(rt, self) + 0x5ACu), argument, block});
}

// Two functions (a0 = this): destructor - vtable word 0x08CC16E8, the base setup 0x0888598C, the object held at +0xC released
// twice (0x08885CEC and 0x08B64654) and the field cleared, then the base vtable word 0x08CBF428.
void destroy_held_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CC16E8u);
    ctx.gpr[2] = 0x08CC16E8u;
    call(rt, ctx, 0x0888598Cu, {self});
    const u32 held = word(rt, self + 0xCu);
    ctx.gpr[16] = held;
    if (held != 0u) {
        call(rt, ctx, 0x08885CECu, {held});
        call(rt, ctx, 0x08B64654u, {held});
    }
    put(rt, self + 0xCu, 0u);
    put(rt, self, 0x08CBF428u);
    ctx.gpr[2] = 0x08CBF428u;
}

// 0x0889722C (a0 = holder, a1 and a2 = two values): a1 lands at word(a0) + 0xAB and a2 at +0xA8, each 10 below 10, its low
// byte below 201 and 200 from 201 on (the `movn` pair); the six bytes between the two are cleared.
void func_0889722C(Runtime &rt, AllegrexContext &ctx) {
    const s32 first = static_cast<s32>(ctx.gpr[5]);
    const s32 second = static_cast<s32>(ctx.gpr[6]);
    const u32 first_byte = ctx.gpr[5] & 0xFFu;
    const u32 second_byte = ctx.gpr[6] & 0xFFu;
    u32 at_offset_3;
    if (first < 10) {
        at_offset_3 = 10u;
    } else {
        at_offset_3 = 200u;
        if (first < 201) at_offset_3 = first_byte;  // `movn a3, a1 & 0xFF, (a1 < 201)`
    }
    u32 at_offset_0;
    if (second < 10) {
        at_offset_0 = 10u;
    } else {
        at_offset_0 = 200u;
        if (second < 201) at_offset_0 = second_byte;  // `movn a1, a2 & 0xFF, (a2 < 201)`
    }
    const u32 base = word(rt, ctx.gpr[4]);
    const u32 record = base + 0xA8u;
    rt.memory().store8(record + 3u, at_offset_3);
    rt.memory().store8(record, at_offset_0);
    rt.memory().store8(record + 7u, 0u);
    rt.memory().store8(record + 1u, 0u);
    rt.memory().store8(record + 2u, 0u);
    rt.memory().store8(record + 4u, 0u);
    rt.memory().store8(record + 5u, 0u);
    rt.memory().store8(record + 6u, 0u);
    ctx.gpr[2] = record;
}

// 0x088B0754 (a0 = this): the sub-object at +0x20 is set up (0x08B34888), the bytes at +8 are prepared from word +0x10
// (0x08B9355C), the four words +0x10..+0x1C describe an empty buffer of self + 0xC, and 0x08B34B24(self + 0x20) finishes.
void func_088B0754(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, 7u);  // s0, s1, s2
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = self + 0x20u;
        ctx.gpr[17] = self + 8u;
        ctx.gpr[16] = self;
        const u32 base = self + 8u;
        call(rt, ctx, 0x08B34888u, {self + 0x20u});
        call(rt, ctx, 0x08B9355Cu, {base, word(rt, base + 8u)});
        put(rt, base + 0x10u, self + 0xCu);
        put(rt, base + 0x14u, 0u);
        put(rt, base + 0xCu, self + 0xCu);
        put(rt, base + 8u, 0u);
    }
    tail_call(ctx, 0x08B34B24u, {self + 0x20u});
}

// 0x088F9C80 (a0 = holder of a table, a1 = a key): the table's records are at word(holder) + 4 .. + 8, four bytes each with a
// signed halfword key at +0 and a value at +2. v0 = the value (its low 17 bits sign-extended) of the first record whose key is
// `key`, -1 when none is.
void func_088F9C80(Runtime &rt, AllegrexContext &ctx) {
    const u32 key = ctx.gpr[5];
    const u32 container = word(rt, ctx.gpr[4]);
    const u32 end = word(rt, container + 8u);
    u32 record = word(rt, container + 4u);
    if (record == end) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    for (;;) {
        ctx.gpr[3] = record + 4u;
        if (static_cast<u32>(signed_half_at(rt, record)) == key) {
            const u32 value = rt.memory().load16(record + 2u);
            ctx.gpr[2] = static_cast<u32>(static_cast<s32>(value << 17) >> 17);
            return;
        }
        const u32 next = record + 4u;
        if (end == next) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        record = next;
    }
}

// 0x088FD9F0 (a0 = a header {count, base}, a1 = an addend): the base word at +4 is a big-endian offset, its own value plus the
// addend, and then for every index below a big-endian count the big-endian word of the table at base + index * 4 is increased by
// the addend (the engine's out-of-order record array; entries that are zero stay zero).
void func_088FD9F0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 addend = ctx.gpr[5];
    put(rt, self + 4u, wsbw(wsbw(word(rt, self + 4u)) + addend));
    u32 count_word = word(rt, self);
    for (u32 index = 0u; wsbw(count_word) > index; ++index) {
        const u32 at = index * 4u + wsbw(word(rt, self + 4u));
        const u32 entry = wsbw(word(rt, at));
        if (entry == 0u) continue;
        put(rt, at, wsbw(addend + entry));
        count_word = word(rt, self);
    }
    ctx.gpr[2] = 0u;  // the loop test's `sltu` result on the way out
}

// 0x0891C840 (a0 = object): 0x08926C2C(self), then three blank records at +0x1F8, +0x204 and +0x210 (the third with 1.0 at +8)
// and the counters at +0x238 and +0x23C cleared; v0 = self + 0x1F8.
void func_0891C840(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08926C2Cu, {self});
    const u32 first = self + 0x1F8u;
    const u32 second = self + 0x204u;
    const u32 third = self + 0x210u;
    ctx.fpr[0] = 1.0f;
    put(rt, first + 8u, 0u);
    put(rt, first + 4u, 0u);
    put(rt, second + 8u, 0u);
    put(rt, second + 4u, 0u);
    put_float(rt, third + 8u, 1.0f);
    put(rt, third + 4u, 0u);
    put(rt, first, 0u);
    put(rt, second, 0u);
    put(rt, self + 0x23Cu, 0u);
    put(rt, third, 0u);
    put(rt, self + 0x238u, 0u);
    ctx.gpr[2] = first;
}

// 0x089405AC (a0, a1, a2 passed on): 0x0894052C(a0, a1, a2), 0x08940328(a0, a1, a2), 0x08940290(a0, a1), then
// 0x08940218(a0, a1, 1) as a tail call.

// 0x08961310 (a0 = a list's sentinel): every node before the sentinel hands its object at +8 to that object's virtual +0x10
// (called with the object), then 0x08BBC0A0(sentinel) runs and the sentinel is made an empty list.

// 0x08974E9C (a0 = object): vtable word 0x08CCCF88, second vtable word 0x08CCD174 at +0x38, 0x08BE5BC0(word +0xA8, word +0xAC,
// &local), the word at +0xA8 released by 0x08B35254 when it is set, then 0x089D8D30(self).
void func_08974E9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 1u);  // s0
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 first = word(rt, self + 0xA8u);
    const u32 second = word(rt, self + 0xACu);
    put(rt, self, 0x08CCCF88u);
    ctx.gpr[2] = 0x08CCCF88u;
    put(rt, self + 0x38u, 0x08CCD174u);
    call(rt, ctx, 0x08BE5BC0u, {first, second, frame.at(0u)});
    const u32 held = word(rt, self + 0xA8u);
    if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
    call(rt, ctx, 0x089D8D30u, {self});
}

// Two functions (a0 = a destination slot): 0x0898E170 / 0x08A354EC fills a local, whose object (assert 0x08CA3CB8 / 0x08CACF04
// when null) is assigned into the destination unless the destination is the local itself, and the local is released.
void assign_from_local(Runtime &rt, AllegrexContext &ctx, u32 fill, u32 assert_text, u32 offset) {
    const u32 destination = ctx.gpr[4];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = destination;
    call(rt, ctx, fill, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_text});
    if (destination != object + offset)
        call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, object + offset)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x0899315C (a0 = this): destructor - vtable word 0x08CCC098, second vtable word 0x08CCC2F0 at +0x38, the members at +0x130
// and +0x12C released by 0x08A5B930 when set, then 0x08A0DD40(self) as a tail call.
void func_0899315C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, 1u);  // s0
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CCC098u);
        put(rt, self + 0x38u, 0x08CCC2F0u);
        const u32 first = word(rt, self + 0x130u);
        ctx.gpr[2] = first;
        if (first != 0u) call(rt, ctx, 0x08A5B930u, {first});
        const u32 second = word(rt, self + 0x12Cu);
        ctx.gpr[2] = second;
        if (second != 0u) call(rt, ctx, 0x08A5B930u, {second});
    }
    tail_call(ctx, 0x08A0DD40u, {self});
}

// 0x08A23F00 (a0 = this): vtable word 0x08CD06F8, the base setup 0x08A23A2C(self), the helper held at +0x18 (with the word at
// +0x14) released through its first virtual and both words cleared, then 0x08A780B0(self) as a tail call.

// 0x08A4D37C: the singleton at 0x08CF68E8, when it is set: 0x08A4DD4C(it), the word is cleared and the instance (read again)
// goes to 0x08A4D308 and then to 0x08B64654 as a tail call (the release chain).

// 0x08A6538C (a0 = this, a2 passed on): the class word 7 is stored at +4 and the table word 0x08CD29A8 at +0, then the members at
// +8 (0x08A6972C) and +0xC (0x08A75D04 with a2) are set up and 0x08A5D438(self + 0x18) is the tail call.

// 0x08A78D20 (a0 = a destination slot, a1 = a slot holding the object): the object at word(a1) - assert 0x08CB0E38 when null -
// goes to 0x08A77444(&local, object); the destination receives the local's word unless it is the local itself, and the local is
// released.

// 0x08AA5D2C (a0 = holder, a1 and a2 passed on): the object at word(holder) - assert 0x08CB2EB8 when null - answers its virtual
// +0x10C, which is the tail call with (object, a1, a2); v0 = the virtual's address, as the `jr $t9` leaves it.
void func_08AA5D2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    u32 object;
    u32 function;
    {
        SavedRegisters saved(ctx, 7u);  // s0, s1, s2
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = third;
        ctx.gpr[17] = argument;
        object = word(rt, self);
        ctx.gpr[16] = object;
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2EB8u});
        function = word(rt, word(rt, object) + 0x10Cu);
    }
    ctx.gpr[2] = function;
    tail_call(ctx, function, {object, argument, third});
}

// 0x08AC4398: the block at 0x08CFFCB0 is created once: when it is unset, 0x08B62344(8) answers a block, 0x08AC42FC(block) runs
// and the block is kept, then 0x08AC3308(block) is the tail call. A block already there answers the caller's v0 untouched.
void func_08AC4398(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kSingleton = 0x08CFFCB0u;
    u32 block;
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = high_half(kSingleton);
        block = word(rt, kSingleton);
        ctx.gpr[2] = block;
        if (block != 0u) return;
        block = call(rt, ctx, 0x08B62344u, {8u});
        ctx.gpr[16] = block;
        call(rt, ctx, 0x08AC42FCu, {block});
        put(rt, kSingleton, block);
    }
    tail_call(ctx, 0x08AC3308u, {block});
}

} // namespace
} // namespace new_batch_13
} // namespace gt::game

#define NEW13(address, name, kind) GT_DECOMP(address, gt::game::new_batch_13::name, gt::ReturnKind::kind)
NEW13(0x08B3EA88u, func_08B3EA88, DoubleWord);
NEW13(0x08B4257Cu, func_08B4257C, Word);
NEW13(0x08B8DA64u, func_08B8DA64, Word);
NEW13(0x08C47150u, func_08C47150, Word);
NEW13(0x08C5AC7Cu, func_08C5AC7C, Word);
NEW13(0x08C5AD90u, func_08C5AD90, Word);
NEW13(0x08C5ADECu, func_08C5ADEC, Word);
NEW13(0x0883D8C8u, func_0883D8C8, Word);
NEW13(0x0883F8E8u, func_0883F8E8, Word);
NEW13(0x088824F8u, func_088824F8, Word);
NEW13(0x088909BCu, func_088909BC, Word);
NEW13(0x08893DD0u, func_08893DD0, Word);
NEW13(0x088BBD58u, func_088BBD58, Float);
NEW13(0x088D2BC8u, func_088D2BC8, Float);
NEW13(0x088DE874u, func_088DE874, Word);
NEW13(0x088F2D38u, func_088F2D38, Word);
NEW13(0x08905F54u, func_08905F54, Word);
NEW13(0x08AA6178u, func_08AA6178, Word);
NEW13(0x08C4B568u, func_08C4B568, Word);
NEW13(0x08C4B800u, func_08C4B800, Word);
NEW13(0x08C4B860u, func_08C4B860, Word);
NEW13(0x08C769D8u, func_08C769D8, Word);
NEW13(0x0885ECF4u, func_0885ECF4, Word);
NEW13(0x08860AD8u, func_08860AD8, Word);
NEW13(0x0889722Cu, func_0889722C, Word);
NEW13(0x088B0754u, func_088B0754, Word);
NEW13(0x088F9C80u, func_088F9C80, Word);
NEW13(0x088FD9F0u, func_088FD9F0, Word);
NEW13(0x0891C840u, func_0891C840, Word);
NEW13(0x08974E9Cu, func_08974E9C, Word);
NEW13(0x0899315Cu, func_0899315C, Word);
NEW13(0x08AA5D2Cu, func_08AA5D2C, Word);
NEW13(0x08AC4398u, func_08AC4398, Word);
