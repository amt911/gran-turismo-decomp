// New batch main 2042 (draft only): five functions drafted before that mismatched the original in the gate, rewritten
// by hand from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary, v0 as the original
// leaves it on every exit path, delay slots before the call / branch, callee-saved registers untouched so they come
// back as they went in). Draft batch, not verified: nothing here is claimed done until the differential gate passes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2042 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

struct CallResult {
    u32 v0;
    float f0;
};
// v0 and f0 of the callee as the next instruction of the original sees them.
CallResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return {result.v0, result.f0};
}

s32 signed_word(Runtime &rt, u32 address) { return static_cast<s32>(word(rt, address)); }

// The original copies a record with `lw`/`sw` in 4-word blocks and a two-word tail.
void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 at = 0u; at < bytes; at += 4u) put(rt, destination + at, word(rt, source + at));
}

// 0x0887C0A8 (a0 = state, a1 = owner): two lookups through owner+0x60's +0xF0 table become the floats at +8 / +0xC;
// owner+0x60's +0xF4 word is diffed against the state's old +0x18 (only-this-side bits at +0x20, shared at +0x1C), the
// three per-slot counters at +0x28..+0x30 are refreshed, the four +0x34..+0x40 counters are clamped to 60, and the two
// deltas are squared / 60, clamped to 0.25 and signed into +0x10 / +0x14. The second delta (i = 1: +0x3C - +0x34) is
// v0; the sign comes from `movz v0, -1, t0` — -1 for i = 0, +1 for i = 1 (the old draft had it the other way, hence
// v0 = -60 where the original answers 60).
void func_0887C0A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);

    const u32 table = word(rt, word(rt, owner + 0x60u) + 0xF0u);
    const u32 first = call(rt, ctx, 0x08B21590u, {table, 0u});
    put_float(rt, self + 0x08u, call_result(rt, ctx, 0x0887BF80u, {first, 0x7Fu, 0x7Fu, 0x28u}).f0);
    const u32 second = call(rt, ctx, 0x08B21590u, {table, 1u});
    put_float(rt, self + 0x0Cu, call_result(rt, ctx, 0x0887BF80u, {second, 0x7Fu, 0x7Fu, 0x28u}).f0);

    const u32 previous = word(rt, self + 0x18u);
    const u32 flags = call(rt, ctx, 0x08B214D8u, {word(rt, word(rt, owner + 0x60u) + 0xF4u), 0u});
    const u32 changed = previous ^ flags;
    const u32 shared = flags & changed;
    put(rt, self + 0x20u, changed & ~flags);
    put(rt, self + 0x18u, flags);
    put(rt, self + 0x1Cu, shared);
    put(rt, self + 0x24u, 0u);
    // Three flag words: a shared bit forces the slot to 0x1C and marks +0x24; a slot only this side owns counts down
    // to zero and is then set to 9.
    for (u32 i = 0u; i < 3u; ++i) {
        const u32 mask = word(rt, 0x08C8983Cu + 4u * i);
        const u32 own = mask & flags;
        if ((shared & mask) != 0u) {
            put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
            put(rt, self + 0x28u + 4u * i, 0x1Cu);
        } else if (own != 0u) {
            const u32 left = word(rt, self + 0x28u + 4u * i) - 1u;
            put(rt, self + 0x28u + 4u * i, left);
            if (static_cast<s32>(left) <= 0) {
                put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
                put(rt, self + 0x28u + 4u * i, 9u);
            }
        }
    }
    // Bits 4..7 of the flags: bump the counter at +0x34..+0x40, capped at 60 (`min`, signed).
    for (u32 bit = 4u; bit < 8u; ++bit) {
        const u32 address = self + 0x34u + 4u * (bit - 4u);
        u32 value = 0u;
        if ((flags & (1u << bit)) != 0u) {
            const s32 bumped = signed_word(rt, address) + 1;
            value = static_cast<u32>(bumped < 0x3C ? bumped : 0x3C);
        }
        put(rt, address, value);
    }
    // Two signed deltas: i = 0 is +0x38 - +0x40, i = 1 is +0x3C - +0x34; squared / 60, at least 0.25, sign restored.
    s32 last = 0;
    for (u32 i = 0u; i < 2u; ++i) {
        const s32 high = signed_word(rt, self + 0x40u - 4u * i);
        const s32 low = signed_word(rt, self + 0x38u - 4u * i);
        const s32 delta = (i == 0u) ? (low - high) : (high - low);
        last = delta;
        put(rt, self + 0x10u + 4u * i, 0u);
        if (delta != 0) {
            float value = static_cast<float>(delta) / 60.0f;
            value = value * value;
            if (value < 0.25f) value = 0.25f;
            value = delta < 0 ? value * -1.0f : value * 1.0f;
            put_float(rt, self + 0x10u + 4u * i, value);
        }
    }
    ctx.gpr[2] = static_cast<u32>(last);
}

// 0x08950438 (self = a0, flag_a = a1, flag_b = a2, flag_c = a3, state = t0): reads (self + 0xC) + 0x270's mode byte,
// runs self's time / count / two float getters, sets the state word, and picks the three byte flags from windows of
// the time (1000 / 300 / 500 / 1000) and the floats (0.1f / 0.7f / 0.0f / the second float). Every exit answers 1:
// the old draft returned 0 on the window-miss paths, which the gate saw as v0 1 against 0.
void func_08950438(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag_a = ctx.gpr[5];
    const u32 flag_b = ctx.gpr[6];
    const u32 flag_c = ctx.gpr[7];
    const u32 state = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);

    const u32 block = word(rt, self + 0x0Cu) + 0x270u;
    const CallResult timing = call_result(rt, ctx, 0x08949E8Cu, {self});
    const u32 mode = rt.memory().load8(block + 0x10u);
    const u32 count = call(rt, ctx, 0x0895534Cu, {self});
    const float first = call_result(rt, ctx, 0x08955960u, {self}).f0;
    const float second = call_result(rt, ctx, 0x0895596Cu, {self}).f0;
    const s32 time = static_cast<s32>(timing.v0);

    if (state != 0u) put(rt, state, 1u);

    u32 window = (1u << 16) | 0x869Fu;
    if (count != 0u && mode != count && call(rt, ctx, 0x08955760u, {self}) != 0u)
        window = call(rt, ctx, 0x0895DD1Cu, {self, count});

    const bool small = static_cast<s32>(count) < 2;
    u32 threshold;
    if (!small) {
        threshold = call(rt, ctx, 0x0895DCB8u, {self, count});
    } else if (count != 1u) {
        threshold = 0xFFFE0000u | 0x7961u;
    } else {
        if (static_cast<s32>(window - 0x3E8u) < time && 0.1f < first) rt.memory().store8(flag_a, count);
        if (time < static_cast<s32>(0xFFFE7579u)) rt.memory().store8(flag_b, 1u);
        ctx.gpr[2] = 1u;
        return;
    }

    if (static_cast<s32>(window - 0x12Cu) < time && 0.7f < first) rt.memory().store8(flag_a, 1u);
    if (0.0f < first) {
        if (time < static_cast<s32>(threshold)) rt.memory().store8(flag_b, 1u);
    } else if (second == 0.0f) {
        if (time < static_cast<s32>(threshold + 0x1F4u)) rt.memory().store8(flag_b, 1u);
    } else {
        if (time < static_cast<s32>(threshold + 0x3E8u)) rt.memory().store8(flag_b, 1u);
        if (!small) rt.memory().store8(flag_c, 1u);
    }
    ctx.gpr[2] = 1u;
}

// 0x088865C8 (a0 = object): constructor — a vtable at +0x50 over a 0xEC block, a second vtable at +0, a 0x10 node handed
// to 08B8E380, the 3.0f / 1/60 / 1.0f fields, a 0x20-word zero block at +0x198, sixteen 6-byte records at +0x2B8, three
// container initialisers and four zeroed words. v0 ends as 2 (the last clear loop's counter). The old draft wrote only
// two of the four zeros at block+0x14..+0x1C, leaving block+0x1C / +0x20 as stale heap (the gate's byte at the block's
// +0x1C was 0x31 where the original had 0).
void func_088865C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);

    put(rt, self + 0x50u, 0x08CC1798u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    const u32 inner = self + 0x50u;
    const u32 cleared = word(rt, inner + 0x14u) & ~3u;
    put(rt, inner + 0x10u, 0u);
    put(rt, inner + 0x14u, cleared);

    const u32 block = call(rt, ctx, 0x08B62344u, {0xECu});
    const u32 table = 0x08D75298u + 0xCu;
    put(rt, block + 0x00u, 0x08CC1800u);
    put(rt, block + 0x04u, 0u);
    put(rt, block + 0x08u, 0u);
    put(rt, block + 0x0Cu, table);
    put(rt, block + 0x10u, table);
    for (u32 at = 0x14u; at <= 0x20u; at += 4u) put(rt, block + at, 0u);
    rt.memory().store8(block + 0x24u, 0u);
    rt.memory().store8(block + 0x25u, 0u);
    put(rt, block + 0x00u, 0x08CC17C0u);
    put(rt, block + 0x24u, word(rt, block + 0x24u) & ~(1u << 16u));
    put(rt, self + 0x68u, block);

    const u32 node = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, node + 0x00u, 0x08CC1840u);
    put(rt, node + 0x04u, 1u);
    put(rt, node + 0x08u, 1u);
    put(rt, node + 0x0Cu, block);
    put(rt, self + 0x68u + 4u, node);
    call(rt, ctx, 0x08B8E380u, {self + 0x68u + 4u, block, block, 1u, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10],
                                 ctx.gpr[11]});

    put_float(rt, self + 0x160u, 3.0f);
    put_float(rt, self + 0x168u, std::bit_cast<float>(0x3C888889u));
    put_float(rt, self + 0x170u, 1.0f);
    u32 flags = word(rt, self + 0x78u) >> 12;
    flags = (flags << 12) | 0x1000u;
    flags &= ~(7u << 13u);
    put(rt, self + 0x78u, flags);
    put(rt, self + 0x188u, table);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x74u, 0u);
    rt.memory().store16(self + 0x7Au, 0u);
    rt.memory().store16(self + 0x9Au, 0u);
    put(rt, self + 0x15Cu, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x16Cu, 0u);
    put(rt, self + 0x184u, table);
    put(rt, self + 0x18Cu, 0u);
    put(rt, self + 0x190u, 0u);
    put(rt, self + 0x194u, 0u);
    for (u32 i = 0u; i < 0x20u; ++i) put(rt, self + 0x198u + 4u * i, 0u);
    for (u32 i = 0u; i < 0x10u; ++i) {
        const u32 record = self + 0x2B8u + 6u * i;
        rt.memory().store8(record, 0u);
        rt.memory().store16(record + 2u, 0xFFFFFFFFu);
        rt.memory().store16(record + 4u, 0xFFFFFFFFu);
    }
    call(rt, ctx, 0x08B4E648u, {self});
    call(rt, ctx, 0x08B945C4u, {self + 0x188u, 0x08C89A48u, 0u});
    call(rt, ctx, 0x08884DA0u, {self, 1u});
    put(rt, self + 0x174u, 0u);
    put(rt, self + 0x17Cu, 0u);
    put(rt, self + 0x174u + 4u, 0u);
    put(rt, self + 0x17Cu + 4u, 0u);
    ctx.gpr[2] = 2u;
}

// The original's atomic refcount decrement (`ll`/`sc`, single-threaded here so the `sc` always lands): answers the
// value before the decrement.
bool release_count(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old == 1u;
}

// 0x08917300 (a0 = container, a1 = count): resets the container through 08BDC338 and releases the temporary it returned
// on the frame; then fills the array at +0 with `count` 8-byte pairs — an object from alloc(0x18) built by 08916A50
// (its answer is discarded: the original keeps the *allocation* in s0 and hands that to 08B8E380 and to the node at
// +0xC; the old draft used 08916A50's return, which is 1, and then stored through it, faulting at address 1) and a
// 0x10 node (vtable 0x08CC5890, counts 1/1, +0xC = the object). Each new object chains the previous one at +0 and the
// node it replaced is released (count at +4, virtuals at +8/+0xC when it reaches 1). v0 ends as &array[count-1]. The
// two `lw $s0, 0xC($sp)` / `lw $s0, 4($sp)` the original makes are values a callee stored on the frame (the temporary
// and the replaced node), which the body reads back through frame.at(), not unmodelled register reloads.

// 0x0897B364 (a0 = object, a1 = a pointer stored at +0x14): constructor — two vtables, a self-linked pair at +0x18, two
// 0x58-byte tables copied from .rodata into +0x94 and +0xEC, the descriptor fields at +0x14A..+0x158, four one-argument
// initialisers at +0x19C..+0x1A8 and blocks of zeroed / 1.0f fields. v0 ends as the 0xFFFFFFFF stored at +0x1F0. The
// mask stored at +0x17C is 0x00FFFFFF (`lui 0xFF; ori 0xFFFF`), not 0xFFFFFFFF: the gate's byte at +0x17F was FF where
// the original had 0.
void func_0897B364(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);

    call(rt, ctx, 0x08A76ACCu, {self});
    const u32 link = self + 0x18u;
    const u32 slot = self + 0x74u;
    put(rt, self + 0x00u, 0x08CBB740u);
    put(rt, self + 0x18u, link);
    put(rt, self + 0x14u, argument);
    put(rt, link + 4u, link);
    call(rt, ctx, 0x08B28DCCu, {self + 0x20u});
    call(rt, ctx, 0x08B28D34u, {self + 0x48u});
    put(rt, slot + 4u, 0u);
    put(rt, slot + 0u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, slot + 0x10u, 0u);
    rt.memory().store8(self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    copy_words(rt, self + 0x94u, 0x08C8BEACu, 0x50u + 8u);
    copy_words(rt, self + 0xECu, 0x08C8BE54u, 0x50u + 8u);
    rt.memory().store16(self + 0x14Au, rt.memory().load16(0x08C8BE50u));
    put(rt, self + 0x14Cu, word(rt, 0x08C8BE48u));
    rt.memory().store16(self + 0x154u, rt.memory().load16(0x08C8BE44u));
    put(rt, self + 0x150u, word(rt, 0x08C8BE48u + 4u));
    put(rt, self + 0x158u, word(rt, 0x08C8BE40u));
    put(rt, self + 0x168u, 0x0Au);
    rt.memory().store16(self + 0x174u, 0x14u);
    put(rt, self + 0x16Cu, 0x12Cu);
    rt.memory().store16(self + 0x176u, 6u);
    put_float(rt, self + 0x170u, 1.0f);
    put(rt, self + 0x17Cu, 0x00FFFFFFu);
    put(rt, self + 0x144u, 0u);
    rt.memory().store8(self + 0x148u, 0u);
    rt.memory().store8(self + 0x149u, 0u);
    rt.memory().store16(self + 0x156u, 0u);
    put(rt, self + 0x15Cu, 0u);
    rt.memory().store8(self + 0x160u, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x178u, 0u);
    put(rt, self + 0x180u, 0u);
    put(rt, self + 0x184u, self + 0x184u);
    put(rt, self + 0x184u + 4u, self + 0x184u);
    put(rt, self + 0x18Cu, self + 0x18Cu);
    put(rt, self + 0x18Cu + 4u, self + 0x18Cu);
    put(rt, self + 0x194u, 0u);
    rt.memory().store8(self + 0x198u, 0u);
    for (u32 i = 0u; i < 4u; ++i) call(rt, ctx, 0x08A76AA0u, {self + 0x19Cu + 4u * i});
    put_float(rt, self + 0x1D4u, 1.0f);
    put(rt, self + 0x1ACu, 0u);
    put(rt, self + 0x1B0u, 0u);
    rt.memory().store8(self + 0x1B4u, 0u);
    rt.memory().store8(self + 0x1B5u, 0u);
    put(rt, self + 0x1B8u, 0u);
    put_float(rt, self + 0x1BCu, 1.0f);
    put(rt, self + 0x1C0u, 0u);
    put_float(rt, self + 0x1C4u, 1.0f);
    put(rt, self + 0x1C8u, 0u);
    put(rt, self + 0x1CCu, 0u);
    put_float(rt, self + 0x1D0u, 1.0f);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1E0u + 4u, 0u);
    put(rt, self + 0x1E0u + 8u, 0u);
    put(rt, self + 0x1E0u + 0xCu, 0u);
    put(rt, self + 0x1E0u + 0x10u, 0xFFFFFFFFu);
    rt.memory().store8(self + 0x200u, 0u);
    rt.memory().store8(self + 0x201u, 0u);
    rt.memory().store8(self + 0x202u, 0u);
    rt.memory().store8(self + 0x203u, 0u);
    rt.memory().store8(self + 0x204u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

} // namespace
} // namespace new_batch_main_2042
} // namespace gt::game

#define NEW2042(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2042::name, gt::ReturnKind::kind)
NEW2042(0x08950438u, func_08950438, Word);
NEW2042(0x088865C8u, func_088865C8, Word);
NEW2042(0x0887C0A8u, func_0887C0A8, Word);
NEW2042(0x0897B364u, func_0897B364, Word);
