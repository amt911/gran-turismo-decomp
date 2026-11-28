// New batch 7: small functions with branches, loops and indirect calls that the extra seeds made emittable, written by hand from
// `tools.disasm` and verified by synthetic cases (some through corpus recipes, config/synth_corpus/<ADDRESS>.txt).
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in are set where the asm sets
// them (a callee may read them) and put back at the end; `v0` ends as the asm leaves it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_7 {
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
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// s0-s3 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 4u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 4u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[4];
};

// --- two variants of one gated call ---------------------------------------------------------------------------------
// The word at 0x08D740F8 gates a call of `target` with the table at 0x08D74100 in front of the caller's arguments and the constant
// 0x80420100 last; the function returns that last argument, or the call's result when the gate is open. The first variant opens the gate
// for the value 1 only, the second for any nonzero value (the asm compares with `bne` against 1, or tests with `beqz`).
constexpr u32 kGateWord = 0x08D740F8u;
constexpr u32 kTable = 0x08D74100u;
constexpr u32 kLastArgument = 0x80420100u;

void gated_by_one(Runtime &rt, AllegrexContext &ctx, u32 target) {
    GuestFrame frame(ctx, 0x10u);
    u32 result = kLastArgument;
    if (word(rt, kGateWord) == 1u) result = call(rt, ctx, target, {kTable, result});
    ctx.gpr[2] = result;
}
void gated_by_nonzero_with_two_arguments(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    u32 result = kLastArgument;
    if (word(rt, kGateWord) != 0u) result = call(rt, ctx, target, {kTable, first, second, result});
    ctx.gpr[2] = result;
}
void func_08B843F8(Runtime &rt, AllegrexContext &ctx) { gated_by_one(rt, ctx, 0x08C7B0D8u); }
void func_08B83EB8(Runtime &rt, AllegrexContext &ctx) { gated_by_nonzero_with_two_arguments(rt, ctx, 0x08C7B080u); }
void func_08B841D0(Runtime &rt, AllegrexContext &ctx) { gated_by_nonzero_with_two_arguments(rt, ctx, 0x08C7B060u); }

// --- lookups that act on a field of the object found -------------------------------------------------------------------------------
// 0x08AE1BF0: looks the name 0x08C9137C up (0x08ADDCD4(a0, name)); an object whose halfword at +0x22 is not negative is passed on
// (0x08ADAEBC); v0 = the last value computed (the lookup's answer, the halfword, or the call's result).
void func_08AE1BF0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C9137Cu});
    if (found == 0u) return;
    const s32 index = signed_half_at(rt, found + 0x22u);
    ctx.gpr[2] = static_cast<u32>(index);
    if (index < 0) return;
    call(rt, ctx, 0x08ADAEBCu, {static_cast<u32>(index)});
}
// 0x08AE1C2C and 0x08AE1C74: the same with another name and callee, returning -1 when nothing is found, 0 for a negative index and
// the call's result otherwise.
void lookup_index_then_call(Runtime &rt, AllegrexContext &ctx, u32 name, u32 callee) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], name});
    u32 result = 0xFFFFFFFFu;
    if (found != 0u) {
        const s32 index = signed_half_at(rt, found + 0x22u);
        ctx.gpr[2] = static_cast<u32>(index);
        result = 0u;
        if (index >= 0) result = call(rt, ctx, callee, {static_cast<u32>(index)});
    }
    ctx.gpr[2] = result;
}
void func_08AE1C2C(Runtime &rt, AllegrexContext &ctx) { lookup_index_then_call(rt, ctx, 0x08C9138Cu, 0x08ADAA90u); }
void func_08AE1C74(Runtime &rt, AllegrexContext &ctx) { lookup_index_then_call(rt, ctx, 0x08C9139Cu, 0x08ADAB0Cu); }

// --- objects with a held pointer ---------------------------------------------------------------------------------------------------
// 0x08884FA0 (a0 = object, a1 = a value): when the object at +0x40 exists, its word +0x4C is passed with the value's truth to
// 0x08AFBDB0 and the value is stored at +0x70 of the object (the store sits in the call's delay slot, so it happens first).
// v0 = the call's result, or the word at +0x40 (0) when there is none.
void func_08884FA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = value;
    const u32 held = word(rt, self + 0x40u);
    ctx.gpr[2] = held;
    if (held != 0u) {
        const u32 argument = word(rt, held + 0x4Cu);
        put(rt, self + 0x70u, value);
        call(rt, ctx, 0x08AFBDB0u, {argument, value != 0u ? 1u : 0u});
    }
}
// 0x08ABD144 (a0 = first node of a list: function pointer, argument, next): calls every node's function with its argument.
void func_08ABD144(Runtime &rt, AllegrexContext &ctx) {
    u32 node = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = node;
    if (node == 0u) return;
    do {
        call_indirect(rt, ctx, word(rt, node), {word(rt, node + 4u)});
        node = word(rt, node + 8u);
        ctx.gpr[16] = node;
    } while (node != 0u);
}
// 0x0889D940 (a0 = object holding {a value, a pointer to a helper object}): when the helper exists, its first virtual is called with the
// value, then both fields are cleared.
void func_0889D940(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 helper = word(rt, self + 4u);
    ctx.gpr[3] = helper;
    if (helper == 0u) return;
    const u32 function = word(rt, word(rt, helper));
    call_indirect(rt, ctx, function, {helper, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// --- constructors and destructors that pass their arguments on -----------------------------------------------------------------------
// 0x08808908 and 0x0881BF1C (a0 = this, a1 = an argument): 0x08A76AB4(this, 0x08CF6ED8), then the work goes on in a tail call with
// (this, argument).
void call_then_tail_with_argument(Runtime &rt, AllegrexContext &ctx, u32 tail_target) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = self;
        call(rt, ctx, 0x08A76AB4u, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, tail_target, {self, argument});
}
void func_08808908(Runtime &rt, AllegrexContext &ctx) { call_then_tail_with_argument(rt, ctx, 0x0880881Cu); }
void func_0881BF1C(Runtime &rt, AllegrexContext &ctx) { call_then_tail_with_argument(rt, ctx, 0x0881BCECu); }

// 0x08B23D28 (a0 = this): the vtable word is stored, 0x08B1FA64(0x08D68C8C, this + 0x2C) runs and the base at this + 4 finishes (0x08B28DC4).
void func_08B23D28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CD95E0u);
        ctx.gpr[2] = 0x08CD95E0u;
        call(rt, ctx, 0x08B1FA64u, {0x08D68C8Cu, self + 0x2Cu});
    }
    tail_call(ctx, 0x08B28DC4u, {self + 4u});
}

// 0x08931EC0 and 0x08931F04 (the same code twice; a0 = this, a1 = an argument): the vtable word 0x08CC67A8 is stored, 0x0893168C(this) runs,
// then 0x08931E54(this, argument) finishes.
void construct_with_argument(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = argument;
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC67A8u);
        ctx.gpr[2] = 0x08CC67A8u;
        call(rt, ctx, 0x0893168Cu);
    }
    tail_call(ctx, 0x08931E54u, {self, argument});
}
void func_08931EC0(Runtime &rt, AllegrexContext &ctx) { construct_with_argument(rt, ctx); }
void func_08931F04(Runtime &rt, AllegrexContext &ctx) { construct_with_argument(rt, ctx); }

// 0x08931BB8 (a0 = this): the vtable word 0x08CC67A8 is stored, a block of 0xA0 bytes is obtained (0x08937AF4) and kept at this + 4,
// 0x0893168C(this) runs and 0x08931B58(this) finishes.
void func_08931BB8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC67A8u);
        ctx.gpr[2] = 0x08CC67A8u;
        const u32 block = call(rt, ctx, 0x08937AF4u, {0xA0u});
        put(rt, self + 4u, block);
        call(rt, ctx, 0x0893168Cu, {self});
    }
    tail_call(ctx, 0x08931B58u, {self});
}

// 0x08938E68 (a0 = this): the vtable word 0x08CC67F8 is stored, the member at this + 0xA0 is set up (0x08939FA4), the fields +4, +0x1C and the
// byte +0x20 are cleared and 0x08938E50(this) finishes.
void func_08938E68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC67F8u);
        ctx.gpr[2] = 0x08CC67F8u;
        call(rt, ctx, 0x08939FA4u, {self + 0xA0u});
        put(rt, self + 4u, 0u);
        put(rt, self + 0x1Cu, 0u);
        rt.memory().store8(self + 0x20u, 0u);
    }
    tail_call(ctx, 0x08938E50u, {self});
}

// 0x08B32AD0 (a0 = this, a1 passed on): 0x08B22C74(this, a1, 0x08C93B34) runs, then the vtable word 0x08CDA820 is stored; v0 = that word.
void func_08B32AD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B22C74u, {self, second, 0x08C93B34u});
    put(rt, self, 0x08CDA820u);
    ctx.gpr[2] = 0x08CDA820u;
}

// 0x08B1C7E4 (a0 = this, a1 passed on): the vtable word 0x08CD9340 is stored, 0x08B1C68C(this, a1, -1, -1) runs and the member at this + 4
// finishes (0x08B34DA0).
void func_08B1C7E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CD9340u);
        ctx.gpr[2] = 0x08CD9340u;
        call(rt, ctx, 0x08B1C68Cu, {self, second, 0xFFFFFFFFu, 0xFFFFFFFFu});
    }
    tail_call(ctx, 0x08B34DA0u, {self + 4u});
}

// 0x0889D910 (a0 = pointer to an object): 0x08B344E0(object) runs, the object's word +0x30 is cleared and 0x08B3457C(object) finishes.
void func_0889D910(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = object;
        call(rt, ctx, 0x08B344E0u, {object});
        put(rt, object + 0x30u, 0u);
    }
    tail_call(ctx, 0x08B3457Cu, {object});
}

} // namespace
} // namespace new_batch_7
} // namespace gt::game

#define NEW7(address, name, kind) GT_DECOMP(address, gt::game::new_batch_7::name, gt::ReturnKind::kind)
NEW7(0x08808908u, func_08808908, Word);
NEW7(0x0881BF1Cu, func_0881BF1C, Word);
NEW7(0x08884FA0u, func_08884FA0, Word);
NEW7(0x0889D910u, func_0889D910, Word);
NEW7(0x0889D940u, func_0889D940, Word);
NEW7(0x08931BB8u, func_08931BB8, Word);
NEW7(0x08931EC0u, func_08931EC0, Word);
NEW7(0x08931F04u, func_08931F04, Word);
NEW7(0x08938E68u, func_08938E68, Word);
NEW7(0x08ABD144u, func_08ABD144, Word);
NEW7(0x08AE1BF0u, func_08AE1BF0, Word);
NEW7(0x08AE1C2Cu, func_08AE1C2C, Word);
NEW7(0x08AE1C74u, func_08AE1C74, Word);
NEW7(0x08B1C7E4u, func_08B1C7E4, Word);
NEW7(0x08B23D28u, func_08B23D28, Word);
NEW7(0x08B32AD0u, func_08B32AD0, Word);
NEW7(0x08B83EB8u, func_08B83EB8, Word);
NEW7(0x08B841D0u, func_08B841D0, Word);
NEW7(0x08B843F8u, func_08B843F8, Word);
