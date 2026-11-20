// Main-session batch 1: twelve- to twenty-four-instruction functions written as structured C from `tools.disasm` and
// verified by synthetic cases. Three shapes:
//  * destructors: the vtable word goes to *this, one to four member destructors run on `this + offset`, then the base
//    destructor is reached by a tail call of `this` (frame popped and saved registers put back first, in the `j`'s delay slot);
//  * the "destroy the owned block, then tail-call the base" shape: the word at `owned + 4` is the block's vtable and its
//    first slot destroys it, the owner's pointer at `this + offset` as the second argument;
//  * constructors and getters: a vtable word at *this, member constructors, sometimes a second vtable word at `this + 0x38`,
//    and two locked word reads.
// Every call passes the argument registers the original sets before the `jal` (a0-a3 in order); the ones it leaves alone keep
// the caller's value, which is what the callee saw. A `jal`'s delay slot runs before the call, so the stores that sit there (a
// vtable word, a list link) happen first. v0 is set as the original leaves it on every exit path. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// v0 / v1 / f0 of the callee stay in the context: the next instruction of the original sees them, and a callee that sets no
// v0 hands the caller's own v0 on (the caller's v0 is what call_guest starts the callee with).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments, {}).v0;
}
// A guest call with the argument registers set the way the original sets them before the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments);
}

// s0-s3 are callee-saved: the original keeps `this` and its arguments in them across calls and puts the caller's values back
// before it returns or tail-calls.
struct SavedS {
    AllegrexContext &ctx;
    u32 s0, s1, s2, s3;
    explicit SavedS(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]), s2(c.gpr[18]), s3(c.gpr[19]) {}
    ~SavedS() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
        ctx.gpr[19] = s3;
    }
};

// f20 likewise: the original parks the float argument in it and reloads f12 from it before it leaves.
struct SavedF {
    AllegrexContext &ctx;
    float f20;
    explicit SavedF(AllegrexContext &c) : ctx(c), f20(c.fpr[20]) {}
    ~SavedF() { ctx.fpr[20] = f20; }
};

// 0x08B557B0 (12): copies f12-f15 into a 0x20-byte frame and passes the frame and its upper half by address, so the callee
// reads the four floats as two pairs. v0 = the callee's result.
void func_08B557B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put_float(rt, frame.at(0x0u), ctx.fpr[12]);
    put_float(rt, frame.at(0x4u), ctx.fpr[13]);
    put_float(rt, frame.at(0x8u), ctx.fpr[14]);
    put_float(rt, frame.at(0xCu), ctx.fpr[15]);
    call_set(rt, ctx, 0x08B5A878u, {frame.at(0x0u), frame.at(0x8u)});
}

// 0x08ADF83C (17): found = 0x08ADDD88(a0, 0x08C91130, f12); when it is non-null, 0x08ADF670(found, 0, f12). v0 = the second
// call's result, or 0 (the first's, which was null) when it is null.
void func_08ADF83C(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const float f12 = ctx.fpr[12];
    u32 found;
    {
        SavedF saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        ctx.fpr[20] = f12;
        found = call_set(rt, ctx, 0x08ADDD88u, {a0, 0x08C91130u});
    }
    ctx.gpr[4] = found;
    ctx.gpr[5] = 0u;
    ctx.fpr[12] = f12;
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08ADF670u, {found, 0u});
}

// 0x089947C4 (19) and 0x0899485C (19): the word at +0x118 / +0x11C, read between a lock and an unlock of 0x08CF4470
// (the load sits in the second call's delay slot, so it happens before it).
void locked_word_reader(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B344E0u, {0x08CF4470u});
    const u32 value = word(rt, self + offset);
    call_set(rt, ctx, 0x08B3457Cu, {0x08CF4470u});
    ctx.gpr[2] = value;
}
void func_089947C4(Runtime &rt, AllegrexContext &ctx) { locked_word_reader(rt, ctx, 0x118u); }
void func_0899485C(Runtime &rt, AllegrexContext &ctx) { locked_word_reader(rt, ctx, 0x11Cu); }

// 0x08931F48 (21) and 0x08931F9C (21): vtable 0x08CC67A8, 0x0893168C(this, a1, a2, a3) — which only clears
// word(this + 0x18), so a1 still holds its argument afterwards — then a tail call of 0x08931D70(this, a1, a2, a3).
void initialise_three_arguments(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[18] = second;
        ctx.gpr[19] = third;
        put(rt, self, 0x08CC67A8u);
        call_set(rt, ctx, 0x0893168Cu, {self, ctx.gpr[5], second, third});
    }
    tail_call(ctx, 0x08931D70u, {self, ctx.gpr[5], second, third});
}
void func_08931F48(Runtime &rt, AllegrexContext &ctx) { initialise_three_arguments(rt, ctx); }
void func_08931F9C(Runtime &rt, AllegrexContext &ctx) { initialise_three_arguments(rt, ctx); }

// 0x089B7124 (21): vtable 0x08CCAC10, second vtable 0x08CCAE6C at +0x38, 0x08A5B9EC on +0x134, +0x130 and +0x12C,
// tail 0x089A29E4(this).
void func_089B7124(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CCAC10u);
        put(rt, self + 0x38u, 0x08CCAE6Cu);
        call_set(rt, ctx, 0x08A5B9ECu, {self + 0x134u});
        call_set(rt, ctx, 0x08A5B9ECu, {self + 0x130u});
        call_set(rt, ctx, 0x08A5B9ECu, {self + 0x12Cu});
    }
    tail_call(ctx, 0x089A29E4u, {self});
}

// 0x088C6454 (22): vtable 0x08CC3348, then a 0x18-byte block from 0x08B62344(0x18) filled with vtable 0x08CC3420, five zero
// words, a zero byte at +0x14, and stored at word(this + 4). v0 = the block.
void func_088C6454(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC3348u);
    const u32 block = call_set(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3420u);
    put(rt, block + 0x4u, 0u);
    put(rt, block + 0x8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    rt.memory().store8(block + 0x14u, 0u);
    put(rt, self + 0x4u, block);
    ctx.gpr[2] = block;
}

// 0x08ABA064 (22): vtable 0x08CD5FB8, a list node at this + 4 linked in front of word(0x08D77180) (the link store is the
// jal's delay slot), 0x08C3C138(this + 0xC, this, a2), then word(this + 0x18) = -1 and v0 = -1.
void func_08ABA064(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CD5FB8u);
    const u32 node = self + 0x4u;
    const u32 previous = word(rt, 0x08D77180u);
    put(rt, node + 0x4u, self);
    put(rt, self + 0x4u, previous);
    put(rt, 0x08D77180u, node);
    call_set(rt, ctx, 0x08C3C138u, {self + 0xCu, self, 0x000A0000u});
    put(rt, self + 0x18u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08B1CF70 (22): vtable 0x08CD9350, 0x08B28DCC(this + 4, a1, a2), then the two arguments at +0x2C / +0x30 and a zero at
// +0x34. v0 = the constructor's result.
void func_08B1CF70(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = first;
        ctx.gpr[18] = second;
        put(rt, self, 0x08CD9350u);
        call_set(rt, ctx, 0x08B28DCCu, {self + 0x4u, first, second});
        put(rt, self + 0x2Cu, first);
        put(rt, self + 0x30u, second);
        put(rt, self + 0x34u, 0u);
    }
}

// 0x08B2C9B0 (22): vtable 0x08CDA710, 0x08B2C2C8(this + 0x58), then a tail call of 0x08B2C8A0(this, a1, a2).
void func_08B2C9B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = first;
        ctx.gpr[18] = second;
        put(rt, self, 0x08CDA710u);
        call_set(rt, ctx, 0x08B2C2C8u, {self + 0x58u});
    }
    tail_call(ctx, 0x08B2C8A0u, {self, first, second});
}

// 0x08845A78 (24): vtable store, then the block at word(this + offset + 4) is destroyed through the first slot of its vtable
// (a0 = block, a1 = word(this + offset)) and both words are cleared, then a tail call of the base destructor with this. Nothing
// held leaves v0 = the table word (the asm's `lui`/`addiu` pair, never overwritten).
void func_08845A78(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kVtable = 0x08CBD678u;
    constexpr u32 kOffset = 0x14u;
    const u32 self = ctx.gpr[4];
    u32 result = kVtable;
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + kOffset;
        put(rt, self, kVtable);
        const u32 owned = word(rt, self + kOffset + 4u);
        ctx.gpr[3] = owned;
        ctx.gpr[4] = owned;
        if (owned != 0u) {
            const u32 table = word(rt, owned);
            const u32 destroy = word(rt, table);
            ctx.gpr[3] = table;
            ctx.gpr[2] = destroy;
            result = call_indirect(rt, ctx, destroy, {owned, word(rt, self + kOffset)});
            put(rt, self + kOffset + 4u, 0u);
            put(rt, self + kOffset, 0u);
        }
    }
    ctx.gpr[2] = result;
    tail_call(ctx, 0x08A780B0u, {self});
}

} // namespace
} // namespace new_batch_main_1
} // namespace gt::game

#define MAIN1(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1::name, gt::ReturnKind::kind)
MAIN1(0x08B557B0u, func_08B557B0, Word);
MAIN1(0x08ADF83Cu, func_08ADF83C, Word);
MAIN1(0x089947C4u, func_089947C4, Word);
MAIN1(0x0899485Cu, func_0899485C, Word);
MAIN1(0x08931F48u, func_08931F48, Word);
MAIN1(0x08931F9Cu, func_08931F9C, Word);
MAIN1(0x089B7124u, func_089B7124, Word);
MAIN1(0x088C6454u, func_088C6454, Word);
MAIN1(0x08ABA064u, func_08ABA064, Word);
MAIN1(0x08B1CF70u, func_08B1CF70, Word);
MAIN1(0x08B2C9B0u, func_08B2C9B0, Word);
MAIN1(0x08845A78u, func_08845A78, Word);
