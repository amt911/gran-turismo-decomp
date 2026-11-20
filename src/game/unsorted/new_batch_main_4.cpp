// Main-session batch 4: 80 short functions the extra seeds made emittable, written as structured C from
// `tools.disasm`. Three shapes dominate:
//   * a singleton accessor: a getter fills a guard object on the stack, the object's pointer is word(sp), a null one
//     trips the (empty) assert at 0x08A5BB7C, `action` runs on it and the guard's destructor (0x08A5B9EC) releases it;
//   * a "walk a global list and call a virtual on each node" loop (0x08ABA360 and friends);
//   * a constructor/destructor that stores a vtable word and ends in a tail call.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_4 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// s0 is callee-saved: the original keeps a value in it across the calls and restores the caller's.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};

// The singleton accessor: getter(sp) fills the guard, word(sp) is the object, a null one trips the assert, `action`
// runs on it and the guard's destructor releases it. v0 = the destructor's result.
void singleton_action(Runtime &rt, AllegrexContext &ctx, u32 getter, u32 assert_string, u32 action, u32 action_offset) {
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, getter, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_string});
    call(rt, ctx, action, {object + action_offset});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// The same, but the action's a0 is the word at object + offset (a member pointer), not the address itself.
void singleton_action_pointer(Runtime &rt, AllegrexContext &ctx, u32 getter, u32 assert_string, u32 action,
                              u32 action_offset) {
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, getter, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_string});
    call(rt, ctx, action, {word(rt, object + action_offset)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// A global list walk: for each node, call the virtual at `vtable_offset` of the object the node's `object_offset`
// word points to, with that object in a0; the next node is the word at `next_offset`.
void walk_list(Runtime &rt, AllegrexContext &ctx, u32 head, u32 object_offset, u32 vtable_offset, u32 next_offset) {
    GuestFrame frame(ctx, 0x10u);
    u32 node = word(rt, head);
    while (node != 0u) {
        const u32 object = word(rt, node + object_offset);
        const u32 target = word(rt, word(rt, object) + vtable_offset);
        call_indirect(rt, ctx, target, {object});
        node = word(rt, node + next_offset);
    }
}

// A 16-byte (four float) copy loop: while source != end, copy the block to destination unless it is null.
void copy_blocks(Runtime &rt, AllegrexContext &ctx) {
    u32 source = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (source == end) {
        ctx.gpr[2] = destination;
        return;
    }
    do {
        const u32 next = source + 0x10u;
        if (destination != 0u) {
            const float f0 = float_at(rt, source);
            const float f1 = float_at(rt, source + 4u);
            const float f2 = float_at(rt, source + 8u);
            const float f3 = float_at(rt, source + 0xCu);
            put_float(rt, destination, f0);
            put_float(rt, destination + 4u, f1);
            put_float(rt, destination + 8u, f2);
            put_float(rt, destination + 0xCu, f3);
        }
        source = next;
        destination += 0x10u;
    } while (source != end);
    ctx.gpr[2] = destination;
}

// A "remove every node of a circular list" loop: the list head is word(this) and the sentinel is this itself.
void clear_circular_list(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 node = word(rt, self);
    if (node == self) return;
    u32 next;
    do {
        next = word(rt, node);
        call(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        node = next;
    } while (self != next);
}

// --- the singleton accessors ---

// 0x088094DC (a0 = destination): a getter fills a temporary at sp; unless the destination is that temporary, the
// temporary's word is copied into it; then the temporary is released. a1/a2/a3/t0 go to the getter as they came in.
void func_088094DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = destination;
    call(rt, ctx, 0x08808F2Cu, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], 1u, 0x08808640u});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08809528: the same with a3 = 0 and the other getter callback 0x088085B8.
void func_08809528(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = destination;
    call(rt, ctx, 0x08808F2Cu, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], 0u, 0x088085B8u});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08B32598 (a0 = object): a constructor in two steps — vtable 0x08CDA730, the base at +0x18, then vtable 0x08CDA7A8
// and a tail call with the object.

// 0x08B33294: when the global 0x08D691D8's +8 word exists, flag +0xC, release it twice and clear the slot.

// 0x08B56674 (a1 = a 4x4 matrix): loads it into the VFPU as 16 scalars (S000..S033, column by column) and tail-calls
// 0x08B5661C with the arguments untouched.
void func_08B56674(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5];
    for (unsigned column = 0u; column < 4u; ++column)
        for (unsigned row = 0u; row < 4u; ++row)
            gt::vfpu::load_scalar(rt, ctx, column + row * 0x20u, source + 0x10u * column + 4u * row);
    tail_call(ctx, 0x08B5661Cu, {});
}

// 0x08B590D0: resets the graphics library's state — 0x08C7AC48(0), then 0x08C7AC20(word(0x08D6CE34), 0) when that
// word exists, and the word is cleared.

// 0x08B62450 (a0 = an index, a1 = a base): for index < 2, call the handler at word(base - 0x2C) with base + 0x20 when
// it exists, then tail-call 0x08B61DF4 with base + 0x20. Any other index takes the original's error path, which calls
// the handler at word(base - 0x24) and then traps (break 0) — it never returns.
void func_08B62450(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[4];
    const u32 base = ctx.gpr[5];
    const u32 self = base + 0x20u;
    {
        GuestFrame frame(ctx, 0x10u);
        if (index >= 2u) {
            call(rt, ctx, 0x08B64564u, {word(rt, base - 0x30u + 0xCu)});
            return;
        }
        const u32 handler = word(rt, base - 0x30u + 4u);
        if (handler != 0u) call_indirect(rt, ctx, handler, {self});
    }
    tail_call(ctx, 0x08B61DF4u, {self});
}

// 0x08B64188: releases every node of the global list 0x08CE1F30 (the next node is at +4) and clears the head.

// 0x08C15F9C (a0 = source, a1 = end, a2 = destination): copies 16-byte blocks until source == end, skipping the copy
// when the destination is null; v0 = the final destination.
void func_08C15F9C(Runtime &rt, AllegrexContext &ctx) { copy_blocks(rt, ctx); }

// 0x08C16084: the same loop with the comparison's operands swapped (a1 != v1 instead of v1 != a1).
void func_08C16084(Runtime &rt, AllegrexContext &ctx) { copy_blocks(rt, ctx); }

// 0x08C55C24 (a0 = object, a1 = a source object, a2 = an argument): stores the source's vtable word at *this and its
// +4 word at this + vtable[-0xC], clears +4, then calls 0x08C54A7C(this + vtable[-0xC], a2, vtable).
void func_08C55C24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 vtable = word(rt, source);
    const u32 second = word(rt, source + 4u);
    const u32 offset = word(rt, vtable - 0xCu);
    put(rt, self, vtable);
    put(rt, self + offset, second);
    const u32 stored = word(rt, self);
    put(rt, self + 4u, 0u);
    const u32 inner = word(rt, stored - 0xCu);
    call(rt, ctx, 0x08C54A7Cu, {self + inner, argument, stored});
}

// 0x08C754BC (a0, a1, a2 = a container, a3 = a count, t0 = a limit): when the container's size (word(a2)[-0xC]) is at
// least the count, tail-calls 0x08C752BC with the container advanced by the count and the length min(limit, size-count).
// Otherwise the original reports the error through 0x08B5AEDC, which does not return.

// 0x08C762D4: the same shape with a3 = a container, t0 = a count, t1 = a limit, and a five-argument tail call.

// 0x088B13CC (a0 = object, a1 = a byte): builds a {0x08CC3020, 7, byte} block on the stack and calls the virtual at
// +0x30 of the object's vtable with (object, object[+4], block).

// 0x088B160C: the same with the block {0x08CC3050, 5, word a1}.

// 0x08994DC4 (a0 = object): a destructor — two vtable words, two member releases, then a tail call with the object.
void func_08994DC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC8EC0u);
        put(rt, self + 0x38u, 0x08CC9118u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x11Cu});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x118u});
    }
    tail_call(ctx, 0x08A0298Cu, {self});
}

// 0x089E8848 (a0, a1, a2): 0x089BD668(a0, a1, a2) gives the second argument of a tail call to 0x089E8408.
void func_089E8848(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    u32 result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call(rt, ctx, 0x089BD668u, {first, second, third});
    }
    tail_call(ctx, 0x089E8408u, {first, result, second, third});
}

// 0x08A135B8 (a0 = object): locks object + 0x10, reads the word at +0x1C0 (in the second call's delay slot, so before
// it), unlocks and returns it.
void func_08A135B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 inner = self + 0x10u;
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B34888u, {inner});
    const u32 value = word(rt, self + 0x1C0u);
    call(rt, ctx, 0x08B34B24u, {inner});
    ctx.gpr[2] = value;
}

// 0x08A139E4: the same with the byte at +0x782.

// 0x08A17CD8 (a0 = a holder, a1..a3, t0, t1): when word(word(a0)) is zero the tail call to 0x08966B20 gets
// (a1, a2, a3, t0, t1), otherwise (a1, t0, t1, a2, a3).
void func_08A17CD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 fifth = ctx.gpr[9];
    const u32 flag = word(rt, word(rt, ctx.gpr[4]));
    if (flag == 0u) {
        tail_call(ctx, 0x08966B20u, {first, second, third, fourth, fifth});
        return;
    }
    tail_call(ctx, 0x08966B20u, {first, fourth, fifth, second, third});
}

// 0x08A35114 (a0 = object): 0x08A34E98(object, 0), 0x08A35088(object), then a tail call with the word at +0x1C (a
// null one trips the assert).

// 0x08A35848 (a0 = object): a destructor — vtable 0x08CCE588, the base at 0x08A35114, three member releases, then a
// tail call with the object.
void func_08A35848(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CCE588u);  // the first jal's delay slot
        call(rt, ctx, 0x08A35114u, {self});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x1Cu});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x18u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x14u});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// 0x08A66DB8 (a0 = object): a constructor — byte 0x24 at +4 (its low byte kept in the word), vtable 0x08CD2E30, the
// member at +8 constructed, +0x14 cleared.
void func_08A66DB8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    rt.memory().store8(self + 4u, 0x24u);
    const u32 flags = word(rt, self + 4u) & 0xFFu;  // ins $v1, $zero, 8, 24
    put(rt, self, 0x08CD2E30u);
    put(rt, self + 4u, flags);
    call(rt, ctx, 0x08A75D04u, {self + 8u});
    put(rt, self + 0x14u, 0u);
}

// 0x08ABA360: for every node of the global list 0x08CFFA24 (next at +8), calls the virtual at +0x10 of the vtable of
// the object at node[0], with that object in a0.

// 0x08ABA3AC: the same over 0x08CFFA28 (next at +4) with the virtual at +0x14.

// 0x08ABB298: the same over 0x08D77184 (object at +4, next at +0) with the virtual at +0x64, then a tail call to
// 0x08ABA360.

// 0x08ABCDE0: the same over 0x08D77190 (object at +4, next at +0) with the virtual at +0x8, then a tail call to
// 0x08ABB298.

// 0x08ACAC0C (a0 = object): reads a 0x40-byte block into the frame with 0x08ACA30C and, when that did not fail,
// passes it on to 0x08ACAA20(object, block, object, 8, object + 0x220).

// 0x08ACBBA8 (a0 = object): locks object + 0xA8, asks 0x08C7B1E8 for a value, unlocks, and returns the negation of
// its sign bit.

// 0x08B0D8C0: for every node of the global list 0x08D7734C (object at +4, next at +0), calls the virtual at +0xC.

// 0x08B0D90C: the same with the virtual at +0x8.

// 0x08B17240 (a0 = object): locks 0x08D61100, stores 0x08D51100 at +0x10, calls 0x08B181E0(object), then unlocks
// 0x08D61100 in a tail call.

// 0x08B1A174 (a0 = object): a tail call to 0x08B1A16C with object + 0x10.
void func_08B1A174(Runtime &rt, AllegrexContext &ctx) { tail_call(ctx, 0x08B1A16Cu, {ctx.gpr[4] + 0x10u}); }

// 0x08B1AB08: locks 0x08D689E4, sets the byte at 0x08D68A2D and, when 0x08D68A2E was clear, calls 0x08B1A9E4; then
// unlocks 0x08D689E4 in a tail call.

// 0x08B2D78C (a0 = a handle): 0x08C7A828(a0, 0x03416102, {1}, 4, 0, 0); a negative answer sets 0x08D68CF8 to 7.

// 0x08B2D7D8: the same with the block holding 2.

// 0x08B3EA3C (a0 = object, a2/a3 = a pair): appends the pair to the 100-entry table at +0x640 (count) / +0x644
// (total), wrapping the count, then tail-calls 0x08B3E908 with the object, the new count and the pair.
void func_08B3EA3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    u32 count = word(rt, self + 0x640u);
    if (static_cast<s32>(count) >= 0x64) {
        put(rt, self + 0x640u, 0u);
        count = 0u;
    }
    const u32 total = word(rt, self + 0x644u);
    const u32 slot = self + (count << 3u);
    count += 1u;
    const u32 next_total = total + 1u;
    const bool under = static_cast<s32>(total) < 0x64;
    put(rt, slot, first);
    put(rt, slot + 4u, second);
    put(rt, self + 0x640u, count);
    if (under) put(rt, self + 0x644u, next_total);
    tail_call(ctx, 0x08B3E908u, {self, count, first, second, self, next_total});
}

// 0x08B58B10 (a0 = a kind byte): kind 0 tail-calls 0x08C7AC08(0, 0, 0, 1); kind 1 tail-calls it with the graphics
// library's 0x08D6CE80/0x08D6CE98/0x08D6CE84; any other kind returns.

// 0x08B5A4FC: resets the graphics library's state block 0x08D6CE00 and clears the byte at 0x08D6E0F0.

// 0x08C00980 (a0 = a circular list's sentinel): releases every node (0x08A5BC54(node, 0xC, 4)) until the sentinel.

// 0x08C2AB00: the same loop.

// 0x08C2AB4C: the same loop.

// 0x08C2AB98: the same loop.

} // namespace
} // namespace new_batch_main_4
} // namespace gt::game

#define MAIN4(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_4::name, gt::ReturnKind::kind)
MAIN4(0x088094DCu, func_088094DC, Word);
MAIN4(0x08809528u, func_08809528, Word);
MAIN4(0x08994DC4u, func_08994DC4, Word);
MAIN4(0x089E8848u, func_089E8848, Word);
MAIN4(0x08A135B8u, func_08A135B8, Word);
MAIN4(0x08A17CD8u, func_08A17CD8, Word);
MAIN4(0x08A35848u, func_08A35848, Word);
MAIN4(0x08A66DB8u, func_08A66DB8, Word);
MAIN4(0x08B1A174u, func_08B1A174, Word);
MAIN4(0x08B3EA3Cu, func_08B3EA3C, Word);
MAIN4(0x08B56674u, func_08B56674, Word);
MAIN4(0x08B62450u, func_08B62450, Word);
MAIN4(0x08C15F9Cu, func_08C15F9C, Word);
MAIN4(0x08C16084u, func_08C16084, Word);
MAIN4(0x08C55C24u, func_08C55C24, Word);
