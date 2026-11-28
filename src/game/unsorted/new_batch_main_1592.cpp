// Main-session batch 1592 (drafted, unverified; structured C from tools.disasm).
// Functions: 088D2DE0 (angle wrap), 089202E4, 08A9CC80, 08AA2E28, 08BE78E8, 08998560, 08875234, 08A8C72C, 08A8C828, 08A6397C.
// Skipped this pass (not drafted): 08A7E4F0/08A7E5E8 (tree copy), 08B1C590, 089E67FC, 089DCD6C, 0881CAEC, 089841D0,
// and the seven guarded-static chains 08B9363C/08B95B58/08B95D48/08B96C44/08B980D0/08BA4064/08BA6A68.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1592 {
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

// A guest call with float arguments ($f12-$f15 in order) as well; keeps v1 and f0 like the call above.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// The callee-saved registers s0-s7 (the body keeps them; a nested callee sees the caller's values).
struct SavedCalleeSaved {
    AllegrexContext &ctx;
    u32 saved[8];
    explicit SavedCalleeSaved(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedCalleeSaved() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

// f20 and f21 as the asm saves them (088D... 089202E4 spills both).
struct SavedFloats {
    AllegrexContext &ctx;
    float f20;
    float f21;
    explicit SavedFloats(AllegrexContext &c) : ctx(c), f20(c.fpr[20]), f21(c.fpr[21]) {}
    ~SavedFloats() {
        ctx.fpr[20] = f20;
        ctx.fpr[21] = f21;
    }
};

// The asm's `ins target, source, pos, size`: the low `size` bits of source go to [pos, pos+size) of target.
u32 insert_bits(u32 target, u32 source, unsigned pos, unsigned size) {
    const u32 mask = ((1u << size) - 1u) << pos;
    return (target & ~mask) | ((source << pos) & mask);
}

float bits_float(u32 bits) { return std::bit_cast<float>(bits); }

// 0x088D2DE0 (f12 = angle): wraps the angle into [-pi, pi) by +-2pi (f0 = the result); outside [-3pi, 3pi] it tail-calls the
// modulo function 0x088D2D88 with f12 unchanged. v0 unused.
void func_088D2DE0(Runtime &rt, AllegrexContext &ctx) {
    (void)rt;
    const float pi = bits_float(0x40490FDBu);
    const float neg_pi = bits_float(0xC0490FDBu);
    const float two_pi = bits_float(0x40C90FDBu);
    const float three_pi = bits_float(0x4116CBE4u);
    const float neg_three_pi = bits_float(0xC116CBE4u);
    const float value = ctx.fpr[12];
    if (value < neg_pi) {
        const float shifted = value + two_pi;
        if (value < neg_three_pi) {
            tail_call(ctx, 0x088D2D88u, {});
            return;
        }
        ctx.fpr[0] = shifted;
        return;
    }
    if (pi <= value) {
        const float shifted = value - two_pi;
        if (three_pi < value) {
            tail_call(ctx, 0x088D2D88u, {});
            return;
        }
        ctx.fpr[0] = shifted;
        return;
    }
    ctx.fpr[0] = value;
}

// 0x089202E4 (a0 = self): f21 = (first * -pi) * 0.5 when vtable[0x1C] of the object at self+8 returns non-zero (first = the
// float that vtable[0x30] returned); the flag byte at self+0x10 follows vtable[0x28]; the float at self+0xC then wins over
// f21 when that byte is set, and f21 = 0 when the global byte 0x08CF28A0 is set. f0 = f21.

// 0x08A9CC80 (a0 = self, a1 = arg): fills the holders at self+0x4C and self+0x48 and drains the two temporary lists: each
// non-sentinel head gets 0x08B60804(head-4, -1) and, when that is <= 0, 0x08B8542C(node, slot).
void func_08A9CC80(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 sentinel = 0x08D74FD0u;
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 holder_a = self + 0x4Cu;
    const u32 holder_b = self + 0x48u;
    const u32 slot_a = frame.at(4u);
    const u32 slot_b = frame.at(8u);
    call(rt, ctx, 0x08B344E0u, {holder_a});
    call(rt, ctx, 0x08AA45F8u, {slot_a});
    call(rt, ctx, 0x08AA51E0u, {slot_b, slot_a, arg});
    call(rt, ctx, 0x08B855E0u, {holder_b, slot_b});
    {
        const u32 head = word(rt, slot_b);
        const u32 node = head - 0xCu;
        if (node != sentinel) {
            const s32 r = static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu}));
            if (r <= 0) {
                call(rt, ctx, 0x08B8542Cu, {node, frame.at(1u)});
            }
        }
    }
    {
        const u32 head = word(rt, slot_a);
        const u32 node = head - 0xCu;
        if (node != sentinel) {
            const s32 r = static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu}));
            if (r <= 0) {
                call(rt, ctx, 0x08B8542Cu, {node, frame.at(0u)});
            }
        }
    }
    call(rt, ctx, 0x08B3457Cu, {holder_a});
}

// 0x08AA2E28 (a0 = self, a1 = count): runs 0x08AABA8C twice per element (the first only after the first element), feeds the
// second answer and the index to 0x08AAB8A0, then ends the list with 0x08AAB540 and 0x08B88F44(self, list); it drains the
// temporary list at sp+4 and returns self in v0.
void func_08AA2E28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 sentinel = 0x08D74FD0u;
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 slot = frame.at(4u);
    call(rt, ctx, 0x08AAB5DCu, {slot});
    for (s32 i = 0; i < count; ++i) {
        if (i > 0) {
            call(rt, ctx, 0x08AABA8Cu, {slot, 0x08CB2B58u});
        }
        const u32 answer = call(rt, ctx, 0x08AABA8Cu, {slot, 0x08CB2B5Cu});
        call(rt, ctx, 0x08AAB8A0u, {answer, static_cast<u32>(i)});
    }
    const u32 list = call(rt, ctx, 0x08AAB540u, {slot});
    call(rt, ctx, 0x08B88F44u, {self, list});
    const u32 head = word(rt, slot);
    const u32 node = head - 0xCu;
    if (node != sentinel) {
        const s32 r = static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu}));
        if (r <= 0) {
            call(rt, ctx, 0x08B8542Cu, {node, frame.at(0u)});
        }
    }
    ctx.gpr[2] = self;
}

// 0x08BE78E8 (a0 = dst, a1 = count, a2 = source element): each destination element is 0x60 bytes. Non-null elements get the
// source's two words and five floats, the vtable 0x08CCFF88, then two 0x0896418C copies (at +0x20 and +0x40) with the words
// at +0x30 and +0x50 copied from the source.
void func_08BE78E8(Runtime &rt, AllegrexContext &ctx) {
    u32 dst = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 src = ctx.gpr[6];
    if (count == 0u) {
        return;
    }
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 vtable = 0x08CCFF88u;
    u32 i = 0;
    do {
        const u32 element = dst;
        i += 1u;
        if (element != 0u) {
            put(rt, element + 4u, word(rt, src + 4u));
            put(rt, element + 8u, word(rt, src + 8u));
            put_float(rt, element + 0xCu, float_at(rt, src + 0xCu));
            put_float(rt, element + 0x10u, float_at(rt, src + 0x10u));
            put_float(rt, element + 0x14u, float_at(rt, src + 0x14u));
            put_float(rt, element + 0x18u, float_at(rt, src + 0x18u));
            put_float(rt, element + 0x1Cu, float_at(rt, src + 0x1Cu));
            put(rt, element, vtable);
            call(rt, ctx, 0x0896418Cu, {element + 0x20u, src + 0x20u});
            put(rt, element + 0x20u + 0x10u, word(rt, src + 0x20u + 0x10u));
            call(rt, ctx, 0x0896418Cu, {element + 0x40u, src + 0x40u});
            put(rt, element + 0x40u + 0x10u, word(rt, src + 0x40u + 0x10u));
        }
        dst += 0x60u;
    } while (i != count);
}

// 0x08998560 (a0 = self): a constructor: vtable 0x08CCBE20, base 0x089946BC, a flags word at +0x12C (bits 0-2 cleared, bits 3-4
// set to 2), the vtable 0x08CCC078 at +0x38, the floats 0.1 and 10.0 at +0x124/+0x128, two zeroed 12-byte records at +0x158,
// the word pair from 0x08C8C260 at +0x170/+0x174, 1.0 at +0x178, then 0x08A76AA0(+0x180) and 0x089D8A54(+0x184).
void func_08998560(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x089946BCu, {self});
    put(rt, self, 0x08CCBE20u);
    // ins a0, zero, 0, 3 then ins a0, v1(=2), 3, 2
    const u32 flags = insert_bits(insert_bits(word(rt, self + 0x12Cu), 0u, 0u, 3u), 2u, 3u, 2u);
    put(rt, self + 0x38u, 0x08CCC078u);
    put_float(rt, self + 0x124u, bits_float(0x3E99999Au));
    put_float(rt, self + 0x128u, bits_float(0x41200000u));
    put(rt, self + 0x12Cu, flags);
    put(rt, self + 0x120u, 0u);
    for (u32 offset = 0x130u; offset <= 0x154u; offset += 4u) {
        put(rt, self + offset, 0u);
    }
    for (u32 k = 0; k < 2u; ++k) {
        const u32 record = self + 0x158u + 12u * k;
        put(rt, record, 0u);
        put(rt, record + 4u, 0u);
        put(rt, record + 8u, 0u);
    }
    const u32 global = 0x08C8C260u;
    const u32 second = word(rt, global + 4u);
    const u32 first = word(rt, global);
    put(rt, self + 0x174u, second);
    put(rt, self + 0x170u, first);
    put_float(rt, self + 0x178u, 1.0f);
    put(rt, self + 0x17Cu, 0u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x180u});
    call(rt, ctx, 0x089D8A54u, {self + 0x184u});
    put(rt, self + 0x188u, 0u);
}

// 0x08875234 (a0 = self): releases the refcounted entries of the table at base+0x830 (walking down from base+0x858): each
// entry's count at +4 drops, and on reaching zero its vtable slot +8 runs, then the count at +8 drops with slot +0xC. Then
// 0x0887BEB0, 0x08AD0574 and 0x08AD0700 run, and base goes to the tail call 0x08B64654.

// Shared body of 0x08A8C72C (holder at self+0x34) and 0x08A8C828 (holder at self+0x3C): for each element of the array between
// the table's bounds, 0x08A65A54(arg, element) and 0x08A8C5D0(self, *element, that result); then the node at the holder's
// +4 is freed with 0x08B5C094, 0x08C2AA40, 0x08A5BC54 (the range, when the begin is non-null), 0x08A69D68 and 0x08A5BC54.
void collect_members(Runtime &rt, AllegrexContext &ctx, u32 holder_offset) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 holder = self + holder_offset;
    const u32 table = word(rt, holder + 4u) + 0xCu;
    for (u32 i = 0;;) {
        const u32 first = word(rt, table);
        const u32 last = word(rt, table + 4u);
        const u32 count = static_cast<u32>(static_cast<s32>(last - first) >> 2);
        const bool more = i < count;
        const u32 element = first + (i << 2);
        i += 1u;
        if (!more) {
            break;
        }
        const u32 result = call(rt, ctx, 0x08A65A54u, {arg, element});
        call(rt, ctx, 0x08A8C5D0u, {self, word(rt, element), result});
    }
    const u32 node = word(rt, holder + 4u);
    call(rt, ctx, 0x08B5C094u, {node});
    call(rt, ctx, 0x08C2AA40u, {word(rt, node + 0xCu), word(rt, node + 0x10u), frame.at(0u)});
    const u32 begin = word(rt, node + 0xCu);
    if (begin != 0u) {
        const u32 end = word(rt, node + 0x14u);
        const u32 span = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        call(rt, ctx, 0x08A5BC54u, {begin, span << 2, 4u});
    }
    call(rt, ctx, 0x08A69D68u, {node + 8u});
    call(rt, ctx, 0x08A5BC54u, {node, 0x18u, 4u});
}


// 0x08A6397C (a0 = self): stores the vtable 0x08CCB980, then while the array at self+0x24 is not empty, the last element's
// vtable slots +0xC and +4 run and the end pointer steps back by 4. Then 0x08A72B94(self+0x3C-word, self), 0x08A5B9EC on
// self+0x54 and self+0x4C, 0x08C1FBC4 over the array, 0x08A5BC54 on the array (when begin is non-null), 0x08A5B930 on the
// word at +0x1C (when non-null), and 0x08A780B0(self).

} // namespace
} // namespace new_batch_main_1592
} // namespace gt::game

#define NEW1592(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1592::name, gt::ReturnKind::kind)
NEW1592(0x088D2DE0u, func_088D2DE0, Float);
NEW1592(0x08A9CC80u, func_08A9CC80, Void);
NEW1592(0x08AA2E28u, func_08AA2E28, Word);
NEW1592(0x08BE78E8u, func_08BE78E8, Void);
NEW1592(0x08998560u, func_08998560, Void);
