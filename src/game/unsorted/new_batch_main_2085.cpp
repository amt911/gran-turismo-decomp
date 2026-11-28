// New batch main 2085 (functions the main session drafted by hand from tools.disasm). Structured C: named locals,
// G[n] only at the call boundary. Delays that matter (a member loaded after the branch is decided, a store in the
// jal's delay slot) are written in the order the asm performs them; branch-likely delays run only when taken.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/game/hot_v2.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2085 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_set(rt, ctx, address, arguments).v0;
}
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// 0x08897C94 (a0 = this): 0x08B62344(0xB8) allocates the object, 0x0894BF50 / 0x08B36018(0, 0x8C) /
// 0x0894BEFC initialise the block at +0x8C (the two 0x0894Bxxx calls bracket the clear), then the fields at
// +0x9C..+0xB7 are set (-1 at +0x9C, zero down to +0xB7), *this = the object and +4 gets the lazily-initialised
// static 0x08D75EF0 (guard byte 0x08D75EE8, 0x08B6441C / 0x08B64494, vtable word 0x08CC1FF0). The work then goes on
// in 0x08896E30(this + 8).
void func_08897C94(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        const u32 object = call(rt, ctx, 0x08B62344u, {0xB8u});
        const u32 block = object + 0x8Cu;
        call(rt, ctx, 0x0894BF50u, {block});
        call(rt, ctx, 0x08B36018u, {object, 0u, 0x8Cu});
        call(rt, ctx, 0x0894BEFCu, {block});
        put(rt, object + 0x9Cu, 0xFFFFFFFFu);
        put(rt, object + 0xA0u, 0u);
        put(rt, object + 0xA4u, 0u);
        put(rt, object + 0xA8u, 0u);
        put(rt, object + 0xACu, 0u);
        for (u32 offset = 0xB0u; offset < 0xB8u; ++offset) put8(rt, object + offset, 0u);
        put(rt, self, object);
        if (byte_at(rt, 0x08D75EE8u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D75EE8u}) != 0u) {
            put(rt, 0x08D75EF0u, 0x08CC1FF0u);
            call(rt, ctx, 0x08B64494u, {0x08D75EE8u});
        }
        put(rt, self + 4u, 0x08D75EF0u);
    }
    tail_call(ctx, 0x08896E30u, {self + 8u});
}

// 0x08909520 (a0 = table holder, a1 = key): 0x08909468() answers a best match directly (its nonzero v0 is the
// answer and the flag byte is 1). Otherwise the array at +0x14 (count words at +0x10, entries 0x40 bytes) is scanned:
// each entry goes to 0x08908F70(entry, a1), whose v1 is the score compared as a float against 0x7F7FFFFF and then
// against the best so far; the entry with the lowest score is v0 (0 if none compared lower). v1 is the byte flag read
// back out of the frame word the asm stores it in.
void func_08909520(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const GuestResult first = call_set(rt, ctx, 0x08909468u);
    u32 flag = 1u;
    u32 answer = first.v0;
    if (first.v0 == 0u) {
        flag = 0u;
        if (static_cast<s32>(word(rt, self + 0x10u)) > 0) {
            float best = std::bit_cast<float>(0x7F7FFFFFu);
            u32 best_entry = 0u;
            u32 index = 0u;
            for (;;) {
                const u32 entry = word(rt, self + 0x14u) + (index << 6u);
                const GuestResult score = call_set(rt, ctx, 0x08908F70u, {entry, key});
                put(rt, frame.at(0x14u), score.v1);
                index += 1u;
                const float value = float_at(rt, frame.at(0x14u));
                put(rt, frame.at(0x10u), score.v0);
                const bool lower = value < best;
                put(rt, frame.at(0x20u), score.v0);
                put(rt, frame.at(0x24u), score.v1);
                if (lower) {
                    best = value;
                    best_entry = entry;
                }
                if (static_cast<s32>(index) >= static_cast<s32>(word(rt, self + 0x10u))) break;
            }
            answer = best_entry;
        }
    }
    put8(rt, frame.at(4u), flag);
    ctx.gpr[2] = answer;
    ctx.gpr[3] = word(rt, frame.at(4u));
}

// 0x0887DD3C (a0 = this, a1 stored at +0x60): +0x50 / +0x54 cleared, 0x08B62344(0x28) builds an object with vtable
// 0x08CC0F88, the static 0x08D752A4 at +0xC / +0x10, zero words at +0x14..+0x20 and the bytes +0x24/+0x25 cleared (the
// word at +0x24 then has bit 16 cleared by `ins`). A second 0x08B62344(0x10) object (vtable 0x08CC0FC8, 1 at +4 and +8,
// the first object at +0xC) goes into +0x5C and 0x08B8E380(this + 0x5C, first, first) links it; +0x58 = the first.
void func_0887DD3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    const u32 first = call(rt, ctx, 0x08B62344u, {0x28u});
    put(rt, first, 0x08CC0F88u);
    put(rt, first + 4u, 0u);
    put(rt, first + 8u, 0u);
    for (u32 index = 0u; index < 2u; ++index) put(rt, first + 0xCu + index * 4u, 0x08D752A4u);
    for (u32 index = 0u; index < 2u; ++index) {
        put(rt, first + 0x14u + index * 8u, 0u);
        put(rt, first + 0x18u + index * 8u, 0u);
    }
    put8(rt, first + 0x24u, 0u);
    put8(rt, first + 0x25u, 0u);
    put(rt, first + 0x24u, word(rt, first + 0x24u) & ~0x10000u);
    put(rt, self + 0x58u, first);
    const u32 second = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, second, 0x08CC0FC8u);
    put(rt, second + 4u, 1u);
    put(rt, second + 8u, 1u);
    put(rt, second + 0xCu, first);
    put(rt, self + 0x5Cu, second);
    call(rt, ctx, 0x08B8E380u, {self + 0x5Cu, first, first});
    put(rt, self + 0x60u, argument);
}

// 0x0887D5F4 (a0 = this): 0x0887D3BC(this) runs, the holder at +0x5C is released (ll/sc count at +4, virtual +8, weak
// count at +8, virtual +0xC) and the holder at +0x54 is released the same way, its last virtual left as the tail call
// (`jr $t9`).
void func_0887D5F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 second = 0u;
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x0887D3BCu, {self});
        const u32 first = word(rt, self + 0x5Cu);
        if (first != 0u) hot_v2::release_holder(rt, ctx, first, false);
        second = word(rt, self + 0x54u);
        if (second != 0u) target = hot_v2::release_holder(rt, ctx, second, true);
    }
    if (target != 0u) tail_call(ctx, target, {second});
}

// 0x0896167C (a0 = this, a1 = source holder): 0x08B344E0(this + 0x18) locks, +0xC takes the source's first word, the
// source's control block (+4) gains a use when it is not the one already stored at +0x10; the old block is released
// (virtual +8 / +0xC on its last use and weak count) and the new one stored. The virtual at +0xC of *this runs, and
// 0x08B3457C(this + 0x18) unlocks in the tail.

// 0x088711E8 (a0 = this): 0x0887E1E0(this) runs, vtable 0x08CBF948 at *this, then 0x08B62344(0xC0): +0x80 / +0x84
// zero, 0x08D752A4 at +0x88, +0x8C..+0x94 cleared, 0x08D752A4 at +0x8 of the three 0xC-byte records from +0x98,
// 0x08B4E648 on the object and on +0x40, the three byte heads at +0x98 cleared. *this + 0xC = the object and +0x10 gets
// the lazily-initialised static 0x08D75368 (guard 0x08D75360, vtable 0x08CBF970). v0 on the already-initialised paths
// is the relocated high half 0x08D70000 the `lui` left, otherwise 0x08B64494's answer.
void func_088711E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x0887E1E0u, {self});
    put(rt, self, 0x08CBF948u);
    const u32 object = call(rt, ctx, 0x08B62344u, {0xC0u});
    put(rt, object + 0x80u, 0u);
    put(rt, object + 0x84u, 0u);
    put(rt, object + 0x88u, 0x08D752A4u);
    put8(rt, object + 0x8Cu, 0u);
    put(rt, object + 0x90u, 0u);
    put(rt, object + 0x94u, 0u);
    for (u32 index = 0u; index < 3u; ++index) put(rt, object + 0x98u + index * 0xCu + 8u, 0x08D752A4u);
    call(rt, ctx, 0x08B4E648u, {object});
    call(rt, ctx, 0x08B4E648u, {object + 0x40u});
    for (u32 index = 0u; index < 3u; ++index) put8(rt, object + 0x98u + index * 0xCu, 0u);
    put(rt, self + 0xCu, object);
    u32 v0 = 0x08D70000u;
    if (byte_at(rt, 0x08D75360u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D75360u}) != 0u) {
        put(rt, 0x08D75368u, 0x08CBF970u);
        v0 = call(rt, ctx, 0x08B64494u, {0x08D75360u});
    }
    put(rt, self + 0x10u, 0x08D75368u);
    ctx.gpr[2] = v0;
}

// 0x08884208 (a0 = this): clears +0x4C / +0x50, 0x088A0BD8(this + 0x54, this + 0x130, 0, f12 = 0), 0x0889FBA0 on
// +0x130 and +0xC08, 1.0f at +0x16E8, +0x16EC's low two bits cleared with bit 2 set, -1 at +0x16F0, zeros at +0x16E0 /
// +0x16E4 / +0x16F4 / +0x16F8. Sixteen 0x26-byte rows from +0x171C (a zero byte, eighteen 0xFFFF halfwords), zeros at
// +0x40..+0x48 and four records of 0xB8 bytes from +0xDC4 (byte 1) with +0x16FC / +0x170C cleared each time. v0 ends as
// the walked pointer this + 0x2E0.
void func_08884208(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 list = self + 0x130u;
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x50u, 0u);
    call_floats(rt, ctx, 0x088A0BD8u, {self + 0x54u, list, 0u}, {0.0f});
    call(rt, ctx, 0x0889FBA0u, {list});
    call(rt, ctx, 0x0889FBA0u, {self + 0xC08u});
    put_float(rt, self + 0x16E8u, 1.0f);
    put(rt, self + 0x16ECu, (word(rt, self + 0x16ECu) & ~3u) | 4u);
    put(rt, self + 0x16F0u, 0xFFFFFFFFu);
    put(rt, self + 0x16E0u, 0u);
    put(rt, self + 0x16E4u, 0u);
    put(rt, self + 0x16F4u, 0u);
    put(rt, self + 0x16F8u, 0u);
    for (u32 row = 0u; row < 0x10u; ++row) {
        const u32 base = self + 0x171Cu + row * 0x26u;
        put8(rt, base, 0u);
        for (u32 index = 0u; index < 0x12u; ++index) put16(rt, base + 2u + index * 2u, 0xFFFFu);
    }
    for (u32 index = 0u; index < 3u; ++index) put(rt, self + 0x40u + index * 4u, 0u);
    for (u32 index = 0u; index < 4u; ++index) {
        put8(rt, self + 0xDC4u + index * 0xB8u, 1u);
        put(rt, self + 0x16FCu + index * 4u, 0u);
        put(rt, self + 0x170Cu + index * 4u, 0u);
    }
    ctx.gpr[2] = self + 0x2E0u;
}

// 0x0885BA8C (a0 = this): *this is an owner whose four members +0x7C / +0x80 / +0x84 / +0x88 are destroyed (0x088D5CF0,
// 0x08894028, 0x08893CC0, 0x088D435C) and freed (0x08B64654), each one only when nonzero. The list at *(owner) is
// popped from its end (count at list - 4, entries 0x188 bytes) with 0x0885D3A0 until the owner's head equals the entry
// just destroyed; 0x08B61FC0 frees the block before the last entry (entry - 8). The owner is freed and 0x08B28DC4 runs on
// this + 8 in the tail.

// 0x08AB7FF4 (a0 = table, a1 = key, a2 / a3 passed through): the signed count at +0x1480 bounds a scan of 0xA4-byte
// entries (0x08B35FD8(this + i * 0xA4, key)) looking for the first zero answer. No entry matched (or a negative count)
// answers 1. The match's 0x20-byte block at entry + 0x80 is copied to the stack, 0x08B29798(stack, a2, a3) and
// 0x08B29E88(0, a2, a3) run; when the latter equals the word at entry + 0xA0 the answer is 1 (the block was left as it
// was), otherwise 0x08B29798 runs again and the answer is 0.
void func_08AB7FF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    if (static_cast<s32>(word(rt, self + 0x1480u)) <= 0) {
        ctx.gpr[2] = 1u;
        return;
    }
    const s32 count = static_cast<s32>(word(rt, self + 0x1480u));
    u32 index = 0u;
    u32 offset = 0u;
    while (call(rt, ctx, 0x08B35FD8u, {self + offset, key}) != 0u) {
        index += 1u;
        offset += 0xA4u;
        if (index == static_cast<u32>(count)) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
    const u32 source = self + offset + 0x80u;
    if (source == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 destination = self + offset + 0xA0u;
    call(rt, ctx, 0x08B29930u, {frame.at(0u), source, 0x20u});
    call(rt, ctx, 0x08B29798u, {frame.at(0u), second, third});
    if (call(rt, ctx, 0x08B29E88u, {0u, second, third}) == word(rt, destination)) {
        ctx.gpr[2] = 1u;
        return;
    }
    call(rt, ctx, 0x08B29798u, {frame.at(0u), second, third});
    ctx.gpr[2] = 0u;
}

// 0x089C8A00 (a0 = result holder, a1 = text, a2 = argument): 0x08B85EB8(text at +4, 0x08CA6654, flag at +1) builds a
// temporary and 0x089C7234(+8, a1, &temporary) a name; the temporary is released (not the shared empty 0x08D74FD0:
// 0x08B60804(&temporary - 4, -1) then, when that is not positive, 0x08B8542C(temporary - 0xC, scratch)). The object at
// +8 is asserted (0x08A5BB7C at 0x08CA665C) and asked through +0x108 with a2; the member at a1 + 0x3C is asserted (line
// 0x08CA6384), 0x08A7CE50(member, &name), 0x089B3840(a0, &name) and 0x08A5B9EC(&name) run; v0 = a0.

// 0x08B0ED20 (a0 = self, a1 = array of pointers, a2 = output byte, a3 = value): answers 0 when the array is empty.
// Each entry goes through a stack descriptor {a2, a3, 0} (0x08B23928(descriptor), 0x08B23A70(descriptor, self)) and
// 0x08B0EC60(stack + 0x50, descriptor's first word); the 15 words it produced are copied down to stack + 0x10 and, when
// the first one is zero, v0 = a2. Otherwise the next pointer is taken, repeating while nonzero; a null ends with v0 = 0.

// 0x0886F710 (a0 = this): 0x0886AD04(this), vtable 0x08CBF7D8 at *this, 0x08B62344(0x1D0) initialised by 0x0887347C
// and 0x0886FC04(+0x64): vtable 0x08CD71E8 at +0x90, 1.0f at +0x84, 0x08D752A4 at +0x130 / +0x134, zero at +0x78,
// byte +0x7C, +0x80, byte +0x138, +0x13C, byte +0x140; the object goes to +0x70 and the lazily-initialised static
// 0x08D752F8 (guard 0x08D752F0, vtable 0x08CBF810) to +0x74. 0x0886AAA4(this, object) is the tail call.
void func_0886F710(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 object = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x0886AD04u, {self});
        put(rt, self, 0x08CBF7D8u);
        object = call(rt, ctx, 0x08B62344u, {0x1D0u});
        call(rt, ctx, 0x0887347Cu, {object});
        call(rt, ctx, 0x0886FC04u, {object + 0x64u});
        put(rt, object + 0x90u, 0x08CD71E8u);
        put_float(rt, object + 0x84u, 1.0f);
        put(rt, object + 0x130u, 0x08D752A4u);
        put(rt, object + 0x134u, 0x08D752A4u);
        put(rt, object + 0x78u, 0u);
        put8(rt, object + 0x7Cu, 0u);
        put(rt, object + 0x80u, 0u);
        put8(rt, object + 0x138u, 0u);
        put(rt, object + 0x13Cu, 0u);
        put8(rt, object + 0x140u, 0u);
        put(rt, self + 0x70u, object);
        if (byte_at(rt, 0x08D752F0u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D752F0u}) != 0u) {
            put(rt, 0x08D752F8u, 0x08CBF810u);
            call(rt, ctx, 0x08B64494u, {0x08D752F0u});
        }
        put(rt, self + 0x74u, 0x08D752F8u);
    }
    tail_call(ctx, 0x0886AAA4u, {self, object});
}

// 0x08869A10 (a0 = this): 0x08869D70(this + 0x1B0), 0x0887B1EC(this + 0x74), 0x0886A2EC(this + 0x1B0), vtable
// 0x08CD70F8 at +0x1A0 and 0x08B8E4D0(+0x1A4, +0x1A8, stack); the +0x1A4 owner is freed (0x08B35254) and vtable
// 0x08CD7118 stored. 0x0890B62C(+0x90) runs, the holder at +0x84 is destroyed through its first vtable entry (a1 =
// +0x80) and both words cleared, then 0x0887AD7C(+0x74), 0x08B28BE0(+0x48) and 0x08B9355C(+0x30, *(+0x38)) run; the
// object at +0x2C loses its weak count (ll/sc at +8) and its +0xC virtual runs on zero, and 0x08B28DC4(this) ends it.

// 0x08909EB0 (a0 = this): the object at +8 is passed to 0x08AF8AB0(object, 0) when nonzero, the holder at +0xC is
// released and the holder at +4 released with its last virtual left as the tail call.
void func_08909EB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 second = 0u;
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 extra = word(rt, self + 8u);
        if (extra != 0u) call(rt, ctx, 0x08AF8AB0u, {extra, 0u});
        const u32 first = word(rt, self + 0xCu);
        if (first != 0u) hot_v2::release_holder(rt, ctx, first, false);
        second = word(rt, self + 4u);
        if (second != 0u) target = hot_v2::release_holder(rt, ctx, second, true);
    }
    if (target != 0u) tail_call(ctx, target, {second});
}

// 0x08ADCA78 (a0 = lookup object, a1 = signed index, a2 = output): 0x08ADB278(a0, 0x08C90F50) answers an object whose
// +0x24 is the table (0x38-byte entries) and +0x30 the count; a null object, empty table, negative index or index past
// count - 1 answers 0. With a null output the answer is 1; otherwise the entry (+8) is copied into nine words: two
// signed bytes from +0x10 / +0x11, the word +0x14, the two spans +0x2C - +0x24 (the first one -1 when +0x2C is
// negative, and then +0xC from +0x28 instead of +0x30), +0x18, +0x1C, +0x38, and +0x34 scaled by the signed byte
// +0x11 unless +0x10 is 4 or 6.
void func_08ADCA78(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90F50u});
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 table = word(rt, found + 0x24u);
    if (table == 0u || static_cast<s32>(index) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (static_cast<s32>(word(rt, found + 0x30u) - 1u) < static_cast<s32>(index)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 entry = table + static_cast<u32>(index) * 0x38u;
    if (out == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 at = entry + 8u;
    const s32 kind = signed_byte_at(rt, at + 8u);
    put(rt, out, static_cast<u32>(kind));
    put(rt, out + 4u, static_cast<u32>(signed_byte_at(rt, at + 9u)));
    put(rt, out + 8u, word(rt, at + 0xCu));
    const s32 span = static_cast<s32>(word(rt, at + 0x24u));
    if (span < 0) {
        put(rt, out + 0x10u, 0xFFFFFFFFu);
        put(rt, out + 0xCu, word(rt, at + 0x20u) - word(rt, at + 0x1Cu));
    } else {
        put(rt, out + 0x10u, static_cast<u32>(span) - word(rt, at + 0x1Cu));
        put(rt, out + 0xCu, word(rt, at + 0x28u) - word(rt, at + 0x1Cu));
    }
    put(rt, out + 0x14u, word(rt, at + 0x10u));
    put(rt, out + 0x18u, word(rt, at + 0x14u));
    put(rt, out + 0x1Cu, word(rt, at + 0x30u));
    u32 scaled = word(rt, at + 0x2Cu);
    if (kind != 4 && kind != 6) {
        // `mult` / `mflo`: the low 32 bits of the signed product, without C++ signed-overflow UB.
        const auto product = static_cast<std::int64_t>(static_cast<s32>(scaled)) * signed_byte_at(rt, at + 9u);
        scaled = static_cast<u32>(static_cast<std::uint64_t>(product) & 0xFFFFFFFFu);
    }
    put(rt, out + 0x20u, scaled);
    ctx.gpr[2] = 1u;
}

// 0x0889A2A4 (a0 = this): 0x08B569A0() starts it. Two passes (index 0 / 1) each take the object at this + 0x40 (the
// second at this + 0x50); when it and its +0x68 answer, 0x08AF8904(*(+0x68), +0x68) runs and two 0xD4-byte rows from
// this + 0x70 + pass * 0x1A8: the object at +0xB04 is asked through +0x40 with 2 * pass + row, and when the row's
// +0xD8 byte is set and the answer's +0x24 virtual is nonzero, 0x08B568DC(row + 0x14) and
// 0x08AF9E88(*(+0x68), row, +0x68, 0) run. 0x08AF8434(*(+0x68)) closes each pass; 0x08B56940() is the tail call.
void func_0889A2A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x30u);
        call(rt, ctx, 0x08B569A0u);
        for (u32 pass = 0u; pass != 2u; ++pass) {
            const u32 object = word(rt, self + (pass != 0u ? 0x10u : 0u) + 0x40u);
            if (object == 0u) continue;
            const u32 state = word(rt, object + 0x68u);
            if (state == 0u) continue;
            call(rt, ctx, 0x08AF8904u, {word(rt, state), state});
            const u32 row_base = self + pass * 0x1A8u + 0x70u;
            for (u32 row = 0u; row != 2u; ++row) {
                const u32 holder = word(rt, self + 0xB04u);
                const u32 answer = call_virtual(rt, ctx, word(rt, word(rt, holder) + 0x40u), {holder, pass * 2u + row}).v0;
                const u32 at = row_base + row * 0xD4u;
                if (byte_at(rt, at + 0xD8u) == 0u) continue;
                if (call_virtual(rt, ctx, word(rt, word(rt, answer) + 0x24u), {answer}).v0 == 0u) continue;
                call(rt, ctx, 0x08B568DCu, {at + 0x14u});
                call(rt, ctx, 0x08AF9E88u, {word(rt, state), row, state, 0u});
            }
            call(rt, ctx, 0x08AF8434u, {word(rt, state)});
        }
    }
    tail_call(ctx, 0x08B56940u, {});
}

// Bound-call node constructor (0x08BF3848 / 0x08BEA858 / 0x08C06C3C, the family of hot batch 161): a0 = out,
// a1 = text, a2 / a3 / t0 = three two-word pairs. 0x08A69B3C(stack + 8, text) builds a temporary, 0x08A5BC20(0x2C, 4)
// allocates the node and 0x08A98220(node, temporary) initialises it; the vtable goes in word 0 and the pairs at
// +0x14/+0x18, +0x1C/+0x20, +0x24/+0x28. The original tests the first pair against 0x08B867AC / 0 and stores the same
// words on every path. 0x08A69D68(temporary, pair[0]), the node into the stack word, 0x08A5D414(&slot, &node),
// 0x08A7C5C8(out, &slot) and 0x08A5B9EC(&slot) finish it.
void bound_node(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    const u32 first_word = word(rt, pair);
    const u32 second_word = word(rt, pair + 4u);
    const u32 third_first = word(rt, third);
    const u32 fourth_first = word(rt, fourth);
    const u32 third_second = word(rt, third + 4u);
    const u32 fourth_second = word(rt, fourth + 4u);
    call(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    const u32 node = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    call(rt, ctx, 0x08A98220u, {node, frame.at(8u)});
    put(rt, node, vtable);
    put(rt, node + 0x18u, second_word);
    put(rt, node + 0x14u, first_word);
    put(rt, node + 0x20u, third_second);
    put(rt, node + 0x1Cu, third_first);
    put(rt, node + 0x28u, fourth_second);
    put(rt, node + 0x24u, fourth_first);
    call(rt, ctx, 0x08A69D68u, {frame.at(8u), first_word});
    put(rt, frame.at(0u), node);
    call(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    call(rt, ctx, 0x08A7C5C8u, {self, frame.at(4u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

void func_08BEA858(Runtime &rt, AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDC90u); }

// 0x08BDD140 (a0 = this): vtable 0x08CC5EE0 at *this, then three runs of three 0x2C-byte embedded objects destroyed
// through their own first vtable entry (a0 = the object), from the top down: this + 0x1A0 / 0x174 / 0x148, this + 0x11C
// / 0xF0 / 0xC4 and this + 0x98 / 0x6C / 0x40. The guards compare those ends with the next run's end (the values are
// distinct, so the runs always happen). 0x08925F98(this) runs and 0x08B64654(this) is the tail call.
void func_08BDD140(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        put(rt, self, 0x08CC5EE0u);
        if (self + 0x148u != 0u && self + 0x1CCu != self + 0x148u) {
            for (u32 index = 0u; index < 3u; ++index) {
                const u32 slot = self + 0x1A0u - index * 0x2Cu;
                call_virtual(rt, ctx, word(rt, word(rt, slot)), {slot});
            }
        }
        const u32 second_end = self + 0xC4u;
        if (second_end != 0u && second_end != self + 0x148u) {
            for (u32 index = 0u; index < 3u; ++index) {
                const u32 slot = self + 0x11Cu - index * 0x2Cu;
                call_virtual(rt, ctx, word(rt, word(rt, slot)), {slot});
            }
        }
        const u32 third_end = self + 0x40u;
        if (third_end != 0u && third_end != second_end) {
            for (u32 index = 0u; index < 3u; ++index) {
                const u32 slot = self + 0x98u - index * 0x2Cu;
                call_virtual(rt, ctx, word(rt, word(rt, slot)), {slot});
            }
        }
        call(rt, ctx, 0x08925F98u, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x088EB410 (a0 = this): a 0x1B-byte descriptor on the stack (twelve 0xFFFF halfwords and three zero bytes) and a
// 0x120-byte buffer at stack + 0x20. The host at +0x538 is asked through +0x68 with the signed half at +0x57C,
// 0x08CA0AD8, the descriptor and 1, then through +0x70 with the answer; the results land at +0x55C and +0x11C. The
// buffer is cleared (0x08B36018, 0x120) and the same two calls run with the half at +0x580, 0x08CA0B14 and the buffer;
// the answers land at +0x558 and feed 0x088ED6F4(this, second answer) and the +0x84 virtual with the sign-extended
// halfword of the first one.
void func_088EB410(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x150u);
    const u32 descriptor = frame.at(0u);
    const u32 buffer = frame.at(0x20u);
    for (u32 offset = 0u; offset < 0x18u; offset += 2u) put16(rt, descriptor + offset, 0xFFFFu);
    put8(rt, descriptor + 0x18u, 0u);
    put8(rt, descriptor + 0x19u, 0u);
    put8(rt, descriptor + 0x1Au, 0u);
    const u32 host = word(rt, self + 0x538u);
    const GuestResult first = call_virtual(rt, ctx, word(rt, word(rt, host) + 0x68u),
                                            {host, static_cast<u32>(signed_half_at(rt, self + 0x57Cu)), 0x08CA0AD8u, 0u,
                                             descriptor, 1u});
    put(rt, self + 0x55Cu, first.v0);
    const u32 host2 = word(rt, self + 0x538u);
    const GuestResult second = call_virtual(rt, ctx, word(rt, word(rt, host2) + 0x70u), {host2, first.v0});
    put(rt, self + 0x11Cu, second.v0);
    call(rt, ctx, 0x08B36018u, {buffer, 0u, 0x120u});
    const u32 host3 = word(rt, self + 0x538u);
    const GuestResult third = call_virtual(rt, ctx, word(rt, word(rt, host3) + 0x68u),
                                            {host3, static_cast<u32>(signed_half_at(rt, self + 0x580u)), 0x08CA0B14u, 0u,
                                             buffer, 1u});
    put(rt, self + 0x558u, third.v0);
    const u32 host4 = word(rt, self + 0x538u);
    const GuestResult fourth = call_virtual(rt, ctx, word(rt, word(rt, host4) + 0x70u), {host4, third.v0});
    call(rt, ctx, 0x088ED6F4u, {self, fourth.v0});
    const u32 host5 = word(rt, self + 0x538u);
    call_virtual(rt, ctx, word(rt, word(rt, host5) + 0x84u),
                  {host5, static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(third.v0)))});
}

} // namespace
} // namespace new_batch_main_2085
} // namespace gt::game

#define NEW2085(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2085::name, gt::ReturnKind::kind)

NEW2085(0x08897C94u, func_08897C94, Word);
NEW2085(0x08909520u, func_08909520, Word);
NEW2085(0x0887DD3Cu, func_0887DD3C, Word);
NEW2085(0x0887D5F4u, func_0887D5F4, Word);
NEW2085(0x088711E8u, func_088711E8, Word);
NEW2085(0x08884208u, func_08884208, Word);
NEW2085(0x08AB7FF4u, func_08AB7FF4, Word);
NEW2085(0x0886F710u, func_0886F710, Word);
NEW2085(0x08909EB0u, func_08909EB0, Word);
NEW2085(0x08ADCA78u, func_08ADCA78, Word);
NEW2085(0x0889A2A4u, func_0889A2A4, Word);
NEW2085(0x08BEA858u, func_08BEA858, Word);
NEW2085(0x08BDD140u, func_08BDD140, Word);
NEW2085(0x088EB410u, func_088EB410, Word);
