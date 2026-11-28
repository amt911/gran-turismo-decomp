// Main-session batch 2075: thunks, constructors and destructors re-drafted from `tools.disasm` after the earlier drafts
// mismatched or never compared. The two adjustor thunks and the small destructors are straightforward; the fixes against
// the old drafts are the relocated `lui` halves that are also used as call arguments (`lui a2, 0xA` in 0x08ABA064 /
// 0x08AB9F60 relocates to 0x08D70000, the high half of the 0x08D77180 list head, not 0x000A0000), and a1 as the previous
// callee left it (0x08931F48 / 0x08931F9C pass it on to 0x08931D70 untouched). Every call carries the registers the
// original set before the `jal`; the delay-slot stores happen before the call; v0 is what the asm leaves on every exit.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2075 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0, v1, f0 and a1 hold what the callee left, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// The same with float arguments ($f12, $f13, ...).
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// `lw v0, slot(v1); jalr v0`: the callee starts with v0 = its own address.
u32 call_target(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}

// Callee-saved registers a body keeps values in (bit n = s_n), put back on scope exit.
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 mask;
    u32 saved[8];
    SavedRegisters(AllegrexContext &c, u32 m) : ctx(c), mask(m) {
        for (u32 i = 0; i < 8; ++i) saved[i] = c.gpr[16 + i];
    }
    ~SavedRegisters() {
        for (u32 i = 0; i < 8; ++i)
            if (mask & (1u << i)) ctx.gpr[16 + i] = saved[i];
    }
};
constexpr u32 kS0 = 0x1u;
constexpr u32 kS0S1 = 0x3u;
constexpr u32 kS0S1S2 = 0x7u;
constexpr u32 kS0S2S3 = 0xDu;

void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x08C7A724: a0 is shifted by -8 and the real body at 0x08C7A730 runs (tail call); v0 is untouched.

// 0x08C7A710 (a0 = object): v0 = *(*(a0) - 0xC) (the virtual-base offset), a0 += v0, tail call 0x08C7A730.

// 0x08B557B0: the four float arguments are copied into a 0x20-byte frame and handed to 0x08B5A878 as two pointers
// (sp, sp+8); v0 = its result.
void func_08B557B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put_float(rt, frame.at(0x0u), ctx.fpr[12]);
    put_float(rt, frame.at(0x4u), ctx.fpr[13]);
    put_float(rt, frame.at(0x8u), ctx.fpr[14]);
    put_float(rt, frame.at(0xCu), ctx.fpr[15]);
    call(rt, ctx, 0x08B5A878u, {frame.at(0x0u), frame.at(0x8u)});
}

// 0x08ADF83C (a0, f12): found = 0x08ADDD88(a0, 0x08C91130, f12); when it is non-null, 0x08ADF670(found, 0, f12);
// v0 = the second call's result, or 0 when the lookup found nothing. f20 holds f12 across the first call.
void func_08ADF83C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float argument = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    u32 result = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.fpr[20] = argument;
        const u32 found = call_float(rt, ctx, 0x08ADDD88u, {self, 0x08C91130u}, {argument});
        ctx.fpr[12] = argument;
        result = found;
        if (found != 0u) {
            result = call_float(rt, ctx, 0x08ADF670u, {found, 0u}, {argument});
        }
    }
    ctx.fpr[20] = saved_f20;
    ctx.gpr[2] = result;
}

// 0x0899485C / 0x089947C4 (a0 = object): the word at +0x11C / +0x118, read between the lock and the unlock of
// 0x08CF4470 (the load sits in the unlock call's delay slot).
void locked_word_reader(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    u32 value;
    {
        SavedRegisters saved(ctx, kS0S1S2);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = 0x08CF0000u;
        ctx.gpr[17] = self;
        call(rt, ctx, 0x08B344E0u, {0x08CF4470u});
        value = word(rt, self + offset);
        ctx.gpr[18] = value;
        call(rt, ctx, 0x08B3457Cu, {0x08CF4470u});
    }
    ctx.gpr[2] = value;
}
void func_0899485C(Runtime &rt, AllegrexContext &ctx) { locked_word_reader(rt, ctx, 0x11Cu); }
void func_089947C4(Runtime &rt, AllegrexContext &ctx) { locked_word_reader(rt, ctx, 0x118u); }

// 0x089B7124 (a0 = this): vtable 0x08CCAC10, a second vtable 0x08CCAE6C at +0x38, 0x08A5B9EC on +0x134, +0x130 and
// +0x12C in that order, then 0x089A29E4(this) finishes (tail call).

// 0x08ACC93C: a0 = *(a0), then the body at 0x08ACC8F0 runs (tail call); v0 is untouched.

// 0x08931F48 / 0x08931F9C: vtable 0x08CC67A8, 0x0893168C(this, a1, a2, a3), then 0x08931D70(this, a1, a2, a3) with a1
// as the first callee left it (the original never reloads a1) and v0 = the first call's result.
void initialise_three_arguments(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    {
        SavedRegisters saved(ctx, kS0S2S3);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[18] = second;
        ctx.gpr[19] = third;
        put(rt, self, 0x08CC67A8u);
        call(rt, ctx, 0x0893168Cu, {self, first, second, third});
    }
    tail_call(ctx, 0x08931D70u, {self, ctx.gpr[5], second, third});
}
void func_08931F48(Runtime &rt, AllegrexContext &ctx) { initialise_three_arguments(rt, ctx); }
void func_08931F9C(Runtime &rt, AllegrexContext &ctx) { initialise_three_arguments(rt, ctx); }

// 0x08ABA064 (a0 = this): vtable 0x08CD5FB8, a list node at +4 linked in front of the global list head 0x08D77180 (its
// link word at +4, its own address at +8), then 0x08C3C138(this + 0xC, this, 0x08D70000) — the relocated `lui a2, 0xA`
// half is what a2 holds at the call — word(this + 0x18) = -1 and v0 = -1.
void func_08ABA064(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD5FB8u);
    const u32 node = self + 0x4u;
    const u32 previous = word(rt, 0x08D77180u);
    ctx.gpr[3] = previous;
    put(rt, node + 4u, self);
    put(rt, node, previous);
    put(rt, 0x08D77180u, node);
    const u32 relocated_high = (0x08D77180u >> 16) << 16;  // what the relocated `lui a2, 0xA` holds
    call(rt, ctx, 0x08C3C138u, {self + 0xCu, self, relocated_high});
    put(rt, self + 0x18u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08B2C9B0 (a0 = this, a1, a2): vtable 0x08CDA710, 0x08B2C2C8(this + 0x58), then 0x08B2C8A0(this, a1, a2) (tail call).
void func_08B2C9B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    {
        SavedRegisters saved(ctx, kS0S1S2);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = first;
        ctx.gpr[18] = second;
        put(rt, self, 0x08CDA710u);
        call(rt, ctx, 0x08B2C2C8u, {self + 0x58u});
    }
    tail_call(ctx, 0x08B2C8A0u, {self, first, second});
}

// 0x088C6454 (a0 = this): vtable 0x08CC3348, then a 0x18-byte block from 0x08B62344(0x18) with vtable 0x08CC3420, four
// zero words, a zero byte at +0x14, stored at word(this + 4); v0 = the block.
void func_088C6454(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CC3348u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3420u);
    put(rt, block + 0x4u, 0u);
    put(rt, block + 0x8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put_byte(rt, block + 0x14u, 0u);
    put(rt, self + 0x4u, block);
    ctx.gpr[2] = block;
}

// 0x08B1CF70 (a0 = this, a1, a2): vtable 0x08CD9350, 0x08B28DCC(this + 4, a1, a2), then a1 at +0x2C, a2 at +0x30 and a
// zero at +0x34; v0 = the constructor's result.
void func_08B1CF70(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx, kS0S1S2);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = first;
    ctx.gpr[18] = second;
    put(rt, self, 0x08CD9350u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x4u, first, second});
    put(rt, self + 0x2Cu, first);
    put(rt, self + 0x30u, second);
    put(rt, self + 0x34u, 0u);
}

// 0x08845A78 (a0 = this): vtable 0x08CBD678, then the block at word(this + 0x18) is destroyed through the first slot of
// its vtable (call with the block and word(this + 0x14)) and both words are cleared, then 0x08A780B0(this) finishes (tail
// call). With no block, v0 stays the vtable word.
void func_08845A78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result = 0x08CBD678u;
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 slot = self + 0x14u;
        ctx.gpr[17] = slot;
        put(rt, self, 0x08CBD678u);
        const u32 owned = word(rt, slot + 4u);
        ctx.gpr[3] = owned;
        if (owned != 0u) {
            const u32 destroy = word(rt, word(rt, owned));
            result = call_target(rt, ctx, destroy, {owned, word(rt, slot)});
            put(rt, slot + 4u, 0u);
            put(rt, slot, 0u);
        }
    }
    ctx.gpr[2] = result;
    tail_call(ctx, 0x08A780B0u, {self});
}

// The object a slot holds (at +offset+4, companion value at +offset) is released through the first virtual of its
// vtable, called with the object and the companion word; both words are then cleared.
void release_member(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset) {
    const u32 object = word(rt, self + offset + 4u);
    ctx.gpr[3] = object;
    if (object == 0u) return;
    const u32 vtable = word(rt, object);
    ctx.gpr[3] = vtable;
    call_target(rt, ctx, word(rt, vtable), {object, word(rt, self + offset)});
    put(rt, self + offset + 4u, 0u);
    put(rt, self + offset, 0u);
}

// 0x08A1C49C (a0 = this): vtable 0x08CD0B38, the member at +0x14 released (object at +0x18), then 0x08A780B0(this).

// 0x08AB9F60 (a0 = this, a1 = a value): vtable 0x08CD5FB8, the list node at +4 linked in front of the global list head
// 0x08D77180, 0x08C3C138(this + 0xC, this, 0x08D70000) (the relocated `lui a2, 0xA` half), then a1 at +0x18;
// v0 = the call's result.
void func_08AB9F60(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx, kS0S1);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = value;
    put(rt, self, 0x08CD5FB8u);
    const u32 node = self + 0x4u;
    ctx.gpr[2] = node;
    const u32 previous = word(rt, 0x08D77180u);
    put(rt, node + 4u, self);
    put(rt, node, previous);
    put(rt, 0x08D77180u, node);
    const u32 relocated_high = (0x08D77180u >> 16) << 16;  // what the relocated `lui a2, 0xA` holds
    call(rt, ctx, 0x08C3C138u, {self + 0xCu, self, relocated_high});
    put(rt, self + 0x18u, value);
}

// 0x08B3A584 (a0, a1 passed on): 0x08B3DF80(this, a1, 2, 1) builds it; when the hook word 0x08D694F0 is set it runs
// (this, a1, 1) as a tail call. v0 is the relocated `lui v0, 0x9` half 0x08D70000 either way.
void func_08B3A584(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x08B3DF80u, {self, argument, 2u, 1u});
    }
    const u32 hook = word(rt, 0x08D694F0u);
    ctx.gpr[2] = ((0x08D694F0u + (1u << 15)) >> 16) << 16;  // the relocated `lui v0, 0x9` high half
    if (hook != 0u) tail_call(ctx, hook, {self, argument, 1u});
}

// 0x08A23E9C (a0 = this): vtable 0x08CD06F8, 0x08A23A2C(this), the member at +0x14 released (object at +0x18), then
// 0x08A780B0(this). v0 is the first call's result when there is no member to release.

// 0x0887EA1C (a0 = this): vtable 0x08CC10C8, the member built with 0x0886AA0C(this, 2), the object held at +0x70
// released, then 0x0886A858(this) finishes (tail call).
void func_0887EA1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 slot = self + 0x70u;
        ctx.gpr[17] = slot;
        put(rt, self, 0x08CC10C8u);
        result = call(rt, ctx, 0x0886AA0Cu, {self, 2u});
        const u32 owned = word(rt, slot + 4u);
        ctx.gpr[3] = owned;
        if (owned != 0u) {
            result = call_target(rt, ctx, word(rt, word(rt, owned)), {owned, word(rt, slot)});
            put(rt, slot + 4u, 0u);
            put(rt, slot, 0u);
        }
    }
    ctx.gpr[2] = result;
    tail_call(ctx, 0x0886A858u, {self});
}

// 0x08C73A28 (a0 = holder of the buffer, a1 = a character, a2 = an offset): looks the byte up in the rest of the buffer
// (0x08B79998) and answers its index relative to the buffer, or -1 when the offset is past the length stored at
// buffer - 0xC or the byte is not there. The character is sign-extended (`seb a1`), the bound is unsigned (`sltu`).
void func_08C73A28(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 character = ctx.gpr[5];
    const u32 offset = ctx.gpr[6];
    u32 result = 0xFFFFFFFFu;
    {
        SavedRegisters saved(ctx, kS0);
        GuestFrame frame(ctx, 0x10u);
        const u32 buffer = word(rt, holder);
        ctx.gpr[16] = buffer;
        const u32 length = word(rt, buffer - 0xCu);
        if (offset < length) {
            const u32 found = call(rt, ctx, 0x08B79998u,
                                   {buffer + offset, static_cast<u32>(static_cast<std::int8_t>(character)), length - offset});
            if (found != 0u) result = found - buffer;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08B2FEB4 (a0 = this): vtable 0x08CDA720, the global word 0x08D69160 cleared in the first call's delay slot,
// 0x08B35214(this) answers an object that 0x08B344E0 initializes; then 0x08B2F1B4(this + 0x964C),
// 0x08B35238(0), 0x08B3457C(first) and 0x08B28DC4(this + 0x8FC) (tail call).
void func_08B2FEB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 first;
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CDA720u);
        put(rt, 0x08D69160u, 0u);
        first = call(rt, ctx, 0x08B35214u, {self});
        ctx.gpr[17] = first;
        call(rt, ctx, 0x08B344E0u, {first});
        call(rt, ctx, 0x08B2F1B4u, {self + 0x964Cu});
        call(rt, ctx, 0x08B35238u, {0u});
        call(rt, ctx, 0x08B3457Cu, {first});
    }
    tail_call(ctx, 0x08B28DC4u, {self + 0x8FCu});
}

// 0x08B118A4 (a0 = this): vtable 0x08CD7CA8, then each member initialized in turn: +4 (with this), +0x14, +0x40, +0x4C,
// +0x58, +0x64; the word at +0x10 and the four words +0x90..+0x9C cleared. v0 = the last initializer's result.
void func_08B118A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD7CA8u);
    call(rt, ctx, 0x08C4C88Cu, {self + 0x4u, self});
    put(rt, self + 0x10u, 0u);
    call(rt, ctx, 0x08B28D34u, {self + 0x14u});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x40u});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x4Cu});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x58u});
    call(rt, ctx, 0x08B28D34u, {self + 0x64u});
    put_byte(rt, self + 0x9Cu, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x98u, 0u);
}

// 0x089BB87C (a0 = this, a1 = 0x08CF6FDC for the subobject build): the subobject is built first, then vtable 0x08CB8F58
// at +0, 0x08CB90F4 at +0x38, the words +0x3C..+0x54 cleared, +0x58 initialized, +0x5C cleared and the low ten bits of
// the word at +0x60 dropped (`ins v0, zero, 0, 10`); v0 = that word.
void func_089BB87C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A7E88Cu, {self, 0x08CF6FDCu});
    put(rt, self, 0x08CB8F58u);
    put(rt, self + 0x38u, 0x08CB90F4u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    call(rt, ctx, 0x08A696E4u, {self + 0x58u});
    put(rt, self + 0x5Cu, 0u);
    const u32 flags = word(rt, self + 0x60u) & ~((1u << 10) - 1u);
    put(rt, self + 0x60u, flags);
    ctx.gpr[2] = flags;
}

// 0x088F200C (a0 = this): vtable 0x08CC3B48, 0x088F1C98(this), the member at +8 released (object at +0xC), then the
// base vtable 0x08CBF518 goes to *this; v0 = 0x08CBF518.

// 0x0886F1F4 (a0 = this): vtable 0x08CBF7D8, the member built twice (0x0886AA0C(this, 5), then (this, 2)), the object
// held at +0x70 released, then 0x0886A858(this) finishes (tail call).
void func_0886F1F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 slot = self + 0x70u;
        ctx.gpr[17] = slot;
        put(rt, self, 0x08CBF7D8u);
        call(rt, ctx, 0x0886AA0Cu, {self, 5u});
        result = call(rt, ctx, 0x0886AA0Cu, {self, 2u});
        const u32 owned = word(rt, slot + 4u);
        ctx.gpr[3] = owned;
        if (owned != 0u) {
            result = call_target(rt, ctx, word(rt, word(rt, owned)), {owned, word(rt, slot)});
            put(rt, slot + 4u, 0u);
            put(rt, slot, 0u);
        }
    }
    ctx.gpr[2] = result;
    tail_call(ctx, 0x0886A858u, {self});
}

// 0x089CC068 (a0 passed to 0x089CC040, which answers an object in the local at sp + 4): 0x08B1ABB0(object) answers the
// result; then the block at object - 0xC is freed: nothing when it is the shared empty block 0x08D74FD0, otherwise only
// when 0x08B60804(object - 4, -1) is <= 0 (0x08B8542C(block, sp)). v0 = that first result.
void func_089CC068(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        SavedRegisters saved(ctx, kS0S1);
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x089CC040u, {frame.at(0x4u), self});
        result = call(rt, ctx, 0x08B1ABB0u, {word(rt, frame.at(0x4u))});
        ctx.gpr[17] = result;
        const u32 pointer = word(rt, frame.at(0x4u));
        const u32 block = pointer - 0xCu;
        ctx.gpr[16] = block;
        if (block != 0x08D74FD0u &&
            static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call(rt, ctx, 0x08B8542Cu, {block, frame.at(0x0u)});
        }
    }
    ctx.gpr[2] = result;
}

// 0x08A6B518 (a0 = this): vtable 0x08CD2338, 0x08A6AFBC(this), then the block held at +0x1C is freed the same way as in
// func_089CC068 (block = word(this + 0x1C) - 0xC, skipped when it is 0x08D74FD0, freed when 0x08B60804(...) <= 0), and
// 0x08A72308(this) finishes; v0 = its result.
void func_08A6B518(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CD2338u);
    call(rt, ctx, 0x08A6AFBCu, {self});
    const u32 pointer = word(rt, self + 0x1Cu);
    const u32 block = pointer - 0xCu;
    ctx.gpr[16] = block;
    if (block != 0x08D74FD0u &&
        static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 0x4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {block, frame.at(0x0u)});
    }
    call(rt, ctx, 0x08A72308u, {self});
}

// 0x088E642C (a0 = this holding the object, a2 and a3 passed on): the object's member at +0xEC is locked, its virtual at
// +0x50 (called with the object and -1) answers a record that receives a2 at +0x30 and a3 at +0x34 (skipped when the
// answer is null, `beql`); the object's member at +0xEC is unlocked (0x08B3457C, tail call).

// 0x08B21BF8 (a0 = this): vtable 0x08CD9538, the member at +4 initialized, +0x2C / +0x30 cleared, 0x08C4FB18(this+0x34,
// this) and 0x08C4FB30(this + 0x40), then the shared block address 0x08D68B7C at +0x60 and +0x5C and zeros at +0x6C,
// +0x64 and +0x68; v0 = 0x08D68B7C.
void func_08B21BF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD9538u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x4u});
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x30u, 0u);
    call(rt, ctx, 0x08C4FB18u, {self + 0x34u, self});
    call(rt, ctx, 0x08C4FB30u, {self + 0x40u});
    put(rt, self + 0x60u, 0x08D68B7Cu);
    put(rt, self + 0x6Cu, 0u);
    put(rt, self + 0x5Cu, 0x08D68B7Cu);
    put(rt, self + 0x64u, 0u);
    put(rt, self + 0x68u, 0u);
    ctx.gpr[2] = 0x08D68B7Cu;
}

} // namespace
} // namespace new_batch_main_2075
} // namespace gt::game

#define NEW2075(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2075::name, gt::ReturnKind::kind)
NEW2075(0x08B557B0u, func_08B557B0, Word);
NEW2075(0x08ADF83Cu, func_08ADF83C, Word);
NEW2075(0x0899485Cu, func_0899485C, Word);
NEW2075(0x089947C4u, func_089947C4, Word);
NEW2075(0x08931F48u, func_08931F48, Word);
NEW2075(0x08931F9Cu, func_08931F9C, Word);
NEW2075(0x08ABA064u, func_08ABA064, Word);
NEW2075(0x08B2C9B0u, func_08B2C9B0, Word);
NEW2075(0x088C6454u, func_088C6454, Word);
NEW2075(0x08B1CF70u, func_08B1CF70, Word);
NEW2075(0x08845A78u, func_08845A78, Word);
NEW2075(0x08AB9F60u, func_08AB9F60, Word);
NEW2075(0x08B3A584u, func_08B3A584, Word);
NEW2075(0x0887EA1Cu, func_0887EA1C, Word);
NEW2075(0x08C73A28u, func_08C73A28, Word);
NEW2075(0x08B2FEB4u, func_08B2FEB4, Word);
NEW2075(0x08B118A4u, func_08B118A4, Word);
NEW2075(0x089BB87Cu, func_089BB87C, Word);
NEW2075(0x0886F1F4u, func_0886F1F4, Word);
NEW2075(0x089CC068u, func_089CC068, Word);
NEW2075(0x08A6B518u, func_08A6B518, Word);
NEW2075(0x08B21BF8u, func_08B21BF8, Word);
