// Main-session batch 1609: 30 functions re-drafted from the batch-11 drafts that mismatched the gate or were skipped twice.
// Structured C from tools.disasm: each delay slot runs before its call or jump (a `beqz` slot runs on both paths), v0 / v1 /
// f0 end as the asm leaves them on every exit (a `lui v0` the body does not reuse is kept), and every argument register a
// callee reads is set before the `jal`. Addresses are the relocated ones tools.disasm prints. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1609 {
namespace {

using namespace gt::game::hot;
using u32 = std::uint32_t;

// v0 / v1 / f0 of a call stay in the context as the callee left them: a callee that never writes v0 hands the caller's value on.
inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call with a0-a3 set as the original sets them before the `jal`, in order; a register not listed keeps its value.
inline u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address,
                    std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result.v0;
}
// The same with $f12-$f15 as well.
inline u32 call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address,
                     std::initializer_list<u32> arguments, std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result.v0;
}
inline void put8(psprecomp::Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}

// s0 / s1 are callee-saved: the bodies that set them put the caller's values back before they return.
class SavedRegisters {
public:
    explicit SavedRegisters(psprecomp::AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

// --- tagged objects: a tag byte at +4 and a vtable word at +0, then a tail call on the object + 8 ----------------------------

// `sb` stores the tag at +4; the word at +4 is read back with its upper 24 bits cleared (`ins`), so it is the tag, and stored.
// v0 is the vtable and v1 the object; a2 is left as the tag the stored word holds.
void tagged_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 tag, u32 vtable, u32 tail) {
    const u32 self = ctx.gpr[4];
    put8(rt, self + 0x4u, tag);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = self;
    put(rt, self, vtable);
    ctx.gpr[6] = tag;
    put(rt, self + 0x4u, tag);
    tail_call(ctx, tail, {self + 0x8u});
}
// 0x08A67244: tag 0x27, vtable 0x08CD2EA8, tail call func_08B88F44.
void func_08A67244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tagged_object(rt, ctx, 0x27u, 0x08CD2EA8u, 0x08B88F44u);
}
// 0x08A67454: tag 0x28, vtable 0x08CD2ED0, tail call func_08A90480.
void func_08A67454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tagged_object(rt, ctx, 0x28u, 0x08CD2ED0u, 0x08A90480u);
}

// --- v = func_08A5D444(a0, a1), then a tail call with (a1, v) ----------------------------------------------------------------

// 0x08A80288 / 0x08A86C50: s0 = a1 is kept across the call; the tail callee gets s0 and the call's answer.
void swap_into_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 tail) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    u32 answer;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = second;
        answer = call_set(rt, ctx, 0x08A5D444u, {first, second});
    }
    tail_call(ctx, tail, {second, answer});
}
void func_08A80288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_into_tail(rt, ctx, 0x08A8DC28u); }
void func_08A86C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_into_tail(rt, ctx, 0x08A8DBF8u); }

// --- a call with a second argument, then the object itself as the result -----------------------------------------------------

// s0 = a0; the call gets (a0, second) and a2/a3 as they came; v0 = the object. `second` is computed by the caller, before the
// argument registers are written.
void call_with_second_then_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, u32 second) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[5] = second;
    (void)call_set(rt, ctx, target, {self, second});
    ctx.gpr[2] = self;
}
// 0x08AA5FA8 (a0 = object, a1 = value): a1 & 0xFF goes to func_08AA5D90. 0x08AA602C: a1 & 0xFFFF goes to func_08AA5DB4.
// 0x08C31B00: func_08AA9360(a0, a1 & 0xFFFF) runs, then v0 = a0. 0x08C31C08: func_08AA9480 with a1 & 0xFF.
void func_08C31C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_with_second_then_self(rt, ctx, 0x08AA9480u, ctx.gpr[5] & 0xFFu);
}
// 0x08C72C44 (a0 = object, a1): func_08B5C450(a0, a1 + 0x1C) runs, then v0 = a0.
void func_08C72C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_with_second_then_self(rt, ctx, 0x08B5C450u, ctx.gpr[5] + 0x1Cu);
}

// 0x08BB8B40 / 0x08BB8B6C / 0x08BB8B98 / 0x08BB8D28 / 0x08BB8E40 (a0, a1): func_088F19DC(a0, a1, N) runs, then v0 = a0.
void call_with_third_then_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 third) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    (void)call_set(rt, ctx, 0x088F19DCu, {self, second, third});
    ctx.gpr[2] = self;
}
void func_08BB8B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third_then_self(rt, ctx, 0x1u); }
void func_08BB8B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third_then_self(rt, ctx, 0x4u); }
void func_08BB8B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third_then_self(rt, ctx, 0x1u); }
void func_08BB8D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third_then_self(rt, ctx, 0x8u); }
void func_08BB8E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third_then_self(rt, ctx, 0x40u); }

// --- the object and its first word / vtable slot -----------------------------------------------------------------------------

// 0x08ABCC1C (a0 = object): the delay slot of the first `beqz` sets a1 = a0 on both paths, and the delay slot of the second sets
// a0 = the first word. A null object returns with v0 untouched; a zero first word returns with v0 = 0. Otherwise v0 = the
// vtable and slot 0x28 is tail-called with (first word, object).
void func_08ABCC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    ctx.gpr[5] = self;
    if (self == 0u) return;
    const u32 child = word(rt, self);
    ctx.gpr[2] = child;
    ctx.gpr[4] = child;
    if (child == 0u) return;
    const u32 table = word(rt, child);
    ctx.gpr[2] = table;
    tail_call(ctx, word(rt, table + 0x28u), {child, self});
}
// 0x08AC2BAC (a0 = an argument): lui v0 is overwritten by the singleton word 0x08CFFCA0. The delay slot sets a1 = the
// argument and a0 = the singleton on both paths. A zero singleton returns with v0 = 0; otherwise slot 0x5C of its vtable is
// tail-called with (singleton, argument).
void func_08AC2BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[4];
    const u32 instance = word(rt, 0x08CFFCA0u);
    ctx.gpr[2] = instance;
    ctx.gpr[4] = instance;
    ctx.gpr[5] = argument;
    if (instance == 0u) return;
    const u32 table = word(rt, instance);
    ctx.gpr[2] = table;
    tail_call(ctx, word(rt, table + 0x5Cu), {instance, argument});
}

// --- the singleton's slot ----------------------------------------------------------------------------------------------------

// 0x08AD08A4 shape: the first call runs under a 0x10 frame with no arguments set; then `lui v0` (0x08D00000 is the
// high half of the singleton 0x08CFFAAC, relocated) overwrites v0, the singleton is loaded from v0 + LO, its vtable goes to
// v1, and slot `slot` is tail-called with the singleton. v0 is left as the lui set it.
void singleton_slot_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 first_call, u32 slot) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_set(rt, ctx, first_call, {});
    }
    ctx.gpr[2] = 0x08D00000u;
    const u32 object = word(rt, 0x08CFFAACu);
    ctx.gpr[4] = object;
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    tail_call(ctx, word(rt, table + slot), {object});
}
void func_08AD08A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    singleton_slot_tail(rt, ctx, 0x08B2F060u, 0x20u);
}

// 0x08B193CC (a0 = object, a1): s0 = a0 is kept; func_08B1935C(a0, a1) answers, then func_08B18C78(object, answer) is a tail call.

// 0x08B2E030: a 64-bit value. func_08B2DF58() answers v1:v0 (the low word in v0, the high in v1); the low word gets 0x7E90
// added, and v1 gets the carry out of that add. v0 and v1 both return.

// 0x08918340 (a0 = object, f12-f15 = four values): the four floats go to sp+0..+0xC, then func_08927758(a0[0xC], &floats).
void func_08918340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 member = word(rt, ctx.gpr[4] + 0xCu);
    put_float(rt, frame.at(0x0u), ctx.fpr[12]);
    put_float(rt, frame.at(0x4u), ctx.fpr[13]);
    put_float(rt, frame.at(0x8u), ctx.fpr[14]);
    put_float(rt, frame.at(0xCu), ctx.fpr[15]);
    (void)call_set(rt, ctx, 0x08927758u, {member, frame.at(0u)});
}

// 0x0891A508 (a0 = object): func_08926C2C(object) runs, then 10.0f (`lui at,0x4120`) goes to object + 0xA8; v0 = the call's result.
void func_0891A508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        (void)call_set(rt, ctx, 0x08926C2Cu, {self});
    }
    ctx.fpr[0] = 10.0f;
    put_float(rt, self + 0xA8u, ctx.fpr[0]);
}

// 0x08A8DBF8 / 0x08A8DC28 / 0x08A8DC58 / 0x08A8DC88 (a0, a1): func_08A8DB94(&local, a0, a1, N) fills a 0x20 local, which
// func_08A5B9EC(&local) releases; v0 = that second call's result.
void build_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 kind) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A8DB94u, {frame.at(0u), ctx.gpr[4], ctx.gpr[5], kind});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08A8DBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x2u); }
void func_08A8DC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x4u); }
void func_08A8DC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x6u); }
void func_08A8DC88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x0u); }

// 0x08AA5DB4 (a0 = object, a1 = value): the 16-bit value goes big-endian into a two-byte stack buffer (v1 = the value, v0 = its
// high byte before the call), and func_08AA5D2C(object, buffer, 2) runs.
void func_08AA5DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 value = ctx.gpr[5] & 0xFFFFu;
    ctx.gpr[3] = value;
    ctx.gpr[2] = value >> 8;
    put8(rt, frame.at(0x1u), value >> 8);
    put8(rt, frame.at(0x0u), value);
    (void)call_set(rt, ctx, 0x08AA5D2Cu, {ctx.gpr[4], frame.at(0u), 0x2u});
}

// 0x088D0674 (a0 = object): func_088D04FC(object + 0x98, object + 0x88) runs, then func_088D04F0(object + 0x88) is a tail call.
void func_088D0674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 second = ctx.gpr[4] + 0x88u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = second;
        (void)call_set(rt, ctx, 0x088D04FCu, {ctx.gpr[4] + 0x98u, second});
    }
    tail_call(ctx, 0x088D04F0u, {second});
}

// 0x0888B85C (a0, a1, a2): a 0x1C-byte record at sp gets 0x1C, a1 at +8 and a2 at +0xC; func_088936A4(a0[0x18], &record) runs
// and its result is the return value. The delay slot loads a0[0x18] before the jump; v0 = 0x1C is the last value set before it.

// 0x08A98250 (a0 = object): func_08A5D5A0(object) runs; then the object's first word becomes the vtable 0x08CD4730, and v0 is
// that vtable too (the `lui v0` / `addiu` pair is the last write to v0).
void func_08A98250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        (void)call_set(rt, ctx, 0x08A5D5A0u, {self, ctx.gpr[5]});
    }
    put(rt, self, 0x08CD4730u);
    ctx.gpr[2] = 0x08CD4730u;
}

// 0x08AD96D8 (f12 = a value): func_08AD95A0(f12) runs, then func_08AD95EC(0, f12) with the same value (kept in f20, which
// comes back); v0 = the second call's result.
void func_08AD96D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float value = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[20] = value;
    (void)call_setf(rt, ctx, 0x08AD95A0u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]}, {value});
    (void)call_setf(rt, ctx, 0x08AD95ECu, {0u}, {ctx.fpr[20]});
    ctx.fpr[20] = saved_f20;
}

} // namespace
} // namespace new_batch_main_1609
} // namespace gt::game

#define NEW_MAIN_1609(address, name, kind) \
    GT_DECOMP(address, gt::game::new_batch_main_1609::name, gt::ReturnKind::kind)

NEW_MAIN_1609(0x08A67244u, func_08A67244, Word);
NEW_MAIN_1609(0x08A67454u, func_08A67454, Word);
NEW_MAIN_1609(0x08A80288u, func_08A80288, Word);
NEW_MAIN_1609(0x08A86C50u, func_08A86C50, Word);
NEW_MAIN_1609(0x08ABCC1Cu, func_08ABCC1C, Word);
NEW_MAIN_1609(0x08AC2BACu, func_08AC2BAC, Word);
NEW_MAIN_1609(0x08AD08A4u, func_08AD08A4, Word);
NEW_MAIN_1609(0x08BB8B40u, func_08BB8B40, Word);
NEW_MAIN_1609(0x08BB8B6Cu, func_08BB8B6C, Word);
NEW_MAIN_1609(0x08BB8B98u, func_08BB8B98, Word);
NEW_MAIN_1609(0x08BB8D28u, func_08BB8D28, Word);
NEW_MAIN_1609(0x08BB8E40u, func_08BB8E40, Word);
NEW_MAIN_1609(0x08C31C08u, func_08C31C08, Word);
NEW_MAIN_1609(0x08C72C44u, func_08C72C44, Word);
NEW_MAIN_1609(0x088D0674u, func_088D0674, Word);
NEW_MAIN_1609(0x08918340u, func_08918340, Word);
NEW_MAIN_1609(0x0891A508u, func_0891A508, Word);
NEW_MAIN_1609(0x08A8DBF8u, func_08A8DBF8, Word);
NEW_MAIN_1609(0x08A8DC28u, func_08A8DC28, Word);
NEW_MAIN_1609(0x08A8DC58u, func_08A8DC58, Word);
NEW_MAIN_1609(0x08A8DC88u, func_08A8DC88, Word);
NEW_MAIN_1609(0x08A98250u, func_08A98250, Word);
NEW_MAIN_1609(0x08AA5DB4u, func_08AA5DB4, Word);
NEW_MAIN_1609(0x08AD96D8u, func_08AD96D8, Word);
