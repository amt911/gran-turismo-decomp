// New batch 11: the 80 smallest functions of the b11 candidate list (23 of 44, 24 of 48 and 33 of 52 bytes), written by hand
// from `tools.disasm` (relocations applied, every branch read as the target it prints) as structured C. Delay slots are read as
// "before the branch / call" — a `bnel` / `beql` one only when the branch is taken. `call_set` writes the argument registers the
// original sets before the `jal`, `call_setf` also the float ones; a register it leaves alone keeps whatever the context holds,
// as the original leaves the caller's value there. The callee-saved registers a body writes are set where the asm sets them (a
// callee may read them) and put back before the return or the tail call (`SavedRegisters`). `v0` (and `v1` / `f0` where a caller
// can read them) ends as the asm leaves it on every path, a callee that never writes v0 included. The frame of each body is the
// original's `addiu sp, sp, -N`. No names: none has evidence.
//
// Not written, not registered (see the report of the batch): func_08B5A960, func_08B5A994, func_08B5A9C8 and func_08B5A9FC, whose
// body ends with a `jal` that has no epilogue and whose return address is the next function's prologue (the compiler dropped the
// epilogue because 0x08B62498 never returns) — a tail call models that only by assuming the callee never returns, while the
// recompiled reference falls through into the next body. func_08930DD8 is already written as `get_car_sell_price` in
// src/game/specdb/spec_db.cpp.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_11 {
namespace {

using namespace gt::game::hot;

// v0 / v1 / f0 of a call stay in the context like the callee left them: a callee that never writes v0 hands the caller's own
// value on, and a caller reads v1 after a `jal` that returns a 64-bit value.
inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                             std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                               std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A guest call with a0-a3 / t0-t3 set as the original sets them before the `jal`, in order.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                              std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// The same with $f12-$f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                             std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
// Allegrex `seb`: the low byte, sign-extended to 32 bits.
inline std::uint32_t sext8(std::uint32_t value) {
    const std::uint32_t byte = value & 0xFFu;
    return (byte & 0x80u) != 0u ? byte | 0xFFFFFF00u : byte;
}
// Allegrex `wsbh`: the two bytes of each half swap places.
inline std::uint32_t swap_half_bytes(std::uint32_t value) {
    return ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu);
}

// s0 / s1 are callee-saved and compared: the bodies that use them set them like the original and the caller's values come back.
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
    std::uint32_t s0_;
    std::uint32_t s1_;
};

// --- tagged objects: a byte tag at +4, a vtable word at +0, then a tail call on the object + 8 ---------------------------------

// 0x08A66AD0 (a0 = object): tag byte +4 = 0x22, then the word +4 is reloaded with its upper 24 bits cleared (`ins`), vtable
// 0x08CD2DE0 at +0, and the object + 8 goes to func_08A6972C (tail call). v0 = the vtable word.
void func_08A66AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put8(rt, self + 0x4u, 0x22u);
    ctx.gpr[2] = 0x08CD2DE0u;
    put(rt, self, 0x08CD2DE0u);
    put(rt, self + 0x4u, 0x22u);  // the reloaded word keeps only the byte just stored
    tail_call(ctx, 0x08A6972Cu, {self + 0x8u});
}
// 0x08A67244: the same with tag 0x27, vtable 0x08CD2EA8 and func_08B88F44.
void func_08A67244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put8(rt, self + 0x4u, 0x27u);
    ctx.gpr[2] = 0x08CD2EA8u;
    put(rt, self, 0x08CD2EA8u);
    put(rt, self + 0x4u, 0x27u);
    tail_call(ctx, 0x08B88F44u, {self + 0x8u});
}
// 0x08A67454: the same with tag 0x28, vtable 0x08CD2ED0 and func_08A90480.
void func_08A67454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put8(rt, self + 0x4u, 0x28u);
    ctx.gpr[2] = 0x08CD2ED0u;
    put(rt, self, 0x08CD2ED0u);
    put(rt, self + 0x4u, 0x28u);
    tail_call(ctx, 0x08A90480u, {self + 0x8u});
}

// --- func_08A5D444(a0, a1) then a tail call with the two swapped -----------------------------------------------------------

// 0x08A72600, 0x08A80288, 0x08A86C50 and 0x08A98290 (a0, a1): v0 = func_08A5D444(a0, a1); then func_TAIL(a1, v0) as a tail call.
void swap_into_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t tail) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    std::uint32_t answer;
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

// --- one virtual call on a held object ------------------------------------------------------------------------------------

// 0x08ABCC1C (a0 = object): with the object and its first word not zero, slot 0x28 of that word's vtable is tail-called with
// a0 = the first word and a1 = the object; v0 = that vtable. A null object returns with v0 untouched, a zero first word with 0.
void func_08ABCC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (self == 0u) return;
    const std::uint32_t child = word(rt, self);
    ctx.gpr[2] = child;
    if (child == 0u) return;
    const std::uint32_t table = word(rt, child);
    ctx.gpr[2] = table;
    tail_call(ctx, word(rt, table + 0x28u), {child, self});
}
// 0x08AC2BAC (a0 = an argument): the singleton word 0x08CFFCA0, when not zero, gets slot 0x5C of its vtable tail-called with the
// singleton as a0 and the argument as a1; with no singleton v0 = 0 and the function returns.
void func_08AC2BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argument = ctx.gpr[4];
    const std::uint32_t instance = word(rt, 0x08CFFCA0u);
    ctx.gpr[2] = instance;
    if (instance == 0u) return;
    const std::uint32_t table = word(rt, instance);
    ctx.gpr[2] = table;
    tail_call(ctx, word(rt, table + 0x5Cu), {instance, argument});
}
// 0x08AD0878: func_08B2F0D4() runs under a 0x10 frame, then slot 0x24 of the vtable of the singleton word 0x08CFFAAC is
// tail-called with the singleton; v0 is the first call's result (this body never writes it). 0x08AD08A4 is the same through
// func_08B2F060 and slot 0x20.
void call_singleton_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first_call,
                         std::uint32_t slot) {
    std::uint32_t object;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_set(rt, ctx, first_call, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    object = word(rt, 0x08CFFAACu);
    tail_call(ctx, word(rt, word(rt, object) + slot), {object});
}
void func_08AD08A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_singleton_slot(rt, ctx, 0x08B2F060u, 0x20u);
}

// --- a call with one argument replaced, then the object itself as the result -----------------------------------------------

// 0x08BB8B40 / 0x08BB8B6C / 0x08BB8B98 / 0x08BB8D28 / 0x08BB8E40 (a0, a1): func_088F19DC(a0, a1, N) runs, then v0 = a0.
void call_with_third(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t third) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    (void)call_set(rt, ctx, 0x088F19DCu, {self, second, third});
    ctx.gpr[2] = self;
}
void func_08BB8B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third(rt, ctx, 0x1u); }
void func_08BB8B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third(rt, ctx, 0x4u); }
void func_08BB8B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third(rt, ctx, 0x1u); }
void func_08BB8D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third(rt, ctx, 0x8u); }
void func_08BB8E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_third(rt, ctx, 0x40u); }
// 0x08C31B00 (a0, a1): func_08AA9360(a0, a1 & 0xFFFF) runs, then v0 = a0. 0x08C31C08: func_08AA9480 with a1 & 0xFF.
void call_with_masked_then_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target,
                                std::uint32_t mask) {
    const std::uint32_t self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    (void)call_set(rt, ctx, target, {self, ctx.gpr[5] & mask});
    ctx.gpr[2] = self;
}
void func_08C31B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_with_masked_then_self(rt, ctx, 0x08AA9360u, 0xFFFFu);
}
void func_08C31C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_with_masked_then_self(rt, ctx, 0x08AA9480u, 0xFFu);
}

// --- a stack record handed to a callee -------------------------------------------------------------------------------------

// 0x0888B85C (a0, a1, a2): a 0x1C-byte record at sp gets 0x1C, a1 at +8 and a2 at +0xC; func_088936A4(a0[0x18], &record) runs
// and its result is the return value.
void func_0888B85C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    put(rt, frame.at(0x0u), 0x1Cu);
    put(rt, frame.at(0x8u), ctx.gpr[5]);
    put(rt, frame.at(0xCu), ctx.gpr[6]);
    (void)call_set(rt, ctx, 0x088936A4u, {word(rt, ctx.gpr[4] + 0x18u), frame.at(0u)});
}
// 0x088D0674 (a0 = object): func_088D04FC(object + 0x98, object + 0x88) runs, then func_088D04F0(object + 0x88) is a tail call.
void func_088D0674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[4] + 0x88u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = second;
        (void)call_set(rt, ctx, 0x088D04FCu, {ctx.gpr[4] + 0x98u, second});
    }
    tail_call(ctx, 0x088D04F0u, {second});
}
// 0x08918340 (a0 = object, f12-f15 = four values): the four floats go to sp+0..+0xC, then func_08927758(a0[0xC], &floats).
void func_08918340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t member = word(rt, ctx.gpr[4] + 0xCu);
    put_float(rt, frame.at(0x0u), ctx.fpr[12]);
    put_float(rt, frame.at(0x4u), ctx.fpr[13]);
    put_float(rt, frame.at(0x8u), ctx.fpr[14]);
    put_float(rt, frame.at(0xCu), ctx.fpr[15]);
    (void)call_set(rt, ctx, 0x08927758u, {member, frame.at(0u)});
}
// 0x0891A508 (a0 = object): func_08926C2C(object) runs, then 10.0f goes to object + 0xA8; v0 = the call's result.
void func_0891A508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        (void)call_set(rt, ctx, 0x08926C2Cu, {self});
    }
    ctx.fpr[0] = 10.0f;  // `lui at, 0x4120` / `mtc1`
    put_float(rt, self + 0xA8u, ctx.fpr[0]);
}

// --- stack helpers ---------------------------------------------------------------------------------------------------------

// 0x08A8DBF8 / 0x08A8DC28 / 0x08A8DC58 / 0x08A8DC88 (a0, a1): func_08A8DB94(&local, a0, a1, N) fills a 0x20 local, which
// func_08A5B9EC(&local) releases; v0 = that second call's result.
void build_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A8DB94u, {frame.at(0u), ctx.gpr[4], ctx.gpr[5], kind});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08A8DBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x2u); }
void func_08A8DC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x4u); }
void func_08A8DC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x6u); }
void func_08A8DC88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_and_release(rt, ctx, 0x0u); }
// 0x08AA5DB4 (a0 = object, a1 = value): the 16-bit value goes big-endian into a two-byte stack buffer, which func_08AA5D2C
// receives with a length of 2.
void func_08AA5DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t value = ctx.gpr[5] & 0xFFFFu;
    put8(rt, frame.at(0x1u), value >> 8);
    put8(rt, frame.at(0x0u), value);
    (void)call_set(rt, ctx, 0x08AA5D2Cu, {ctx.gpr[4], frame.at(0u), 0x2u});
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
// 0x08B102D0 (a0 = object): func_08B2E924(object) runs, then slot 0x3C of the object's vtable is a tail call with the object
// (v0 = the vtable when it starts).
void func_08B102D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        (void)call_set(rt, ctx, 0x08B2E924u, {self});
    }
    const std::uint32_t table = word(rt, self);
    ctx.gpr[2] = table;
    tail_call(ctx, word(rt, table + 0x3Cu), {self});
}
// 0x08B1202C (a0, a1): func_08B26B74(&buffer[0x1840], a0, a1, 0) fills the buffer, which func_08B24DB4(&buffer) then releases.
void func_08B1202C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x1840u);
    (void)call_set(rt, ctx, 0x08B26B74u, {frame.at(0u), ctx.gpr[4], ctx.gpr[5], 0u});
    (void)call_set(rt, ctx, 0x08B24DB4u, {frame.at(0u)});
}
// 0x08B28CD4 (a0 = object): func_08B34880(object), func_08B28C04(object), then func_08B34B1C(object) as a tail call.
void func_08B28CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        (void)call_set(rt, ctx, 0x08B34880u, {self});
        (void)call_set(rt, ctx, 0x08B28C04u, {self});
    }
    tail_call(ctx, 0x08B34B1Cu, {self});
}

// --- plain loops and copies ------------------------------------------------------------------------------------------------

// 0x08BEF08C (a0 = source, a1 = end, a2 = destination): copies floats from source to destination while the source advances
// (the source pointer is bumped in the delay slot of the null-destination test, so a null destination only skips the store
// once, and the destination still advances). v0 = the advanced destination.
void func_08BEF08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4];
    const std::uint32_t end = ctx.gpr[5];
    std::uint32_t dest = ctx.gpr[6];
    ctx.gpr[2] = dest;
    if (source == end) return;
    std::uint32_t next = source;
    for (;;) {
        next += 4u;  // `addiu v1, v1, 4` sits in the delay slot of `beqz dest` and runs either way
        if (dest != 0u) put_float(rt, dest, float_at(rt, source));
        source = next;
        dest += 4u;
        if (next == end) break;
    }
    ctx.gpr[2] = dest;
}
// 0x088D5AC8 (a0 = holder, a1-a3 = three 16-bit values): each value is byte-swapped into the word the holder points at, at
// +0x30, +0x32 and +0x34 (the word is reloaded before each store, so a store that changes it is seen); v0 = the last word.
void func_088D5AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = swap_half_bytes(ctx.gpr[5] & 0xFFFFu);
    const std::uint32_t second = swap_half_bytes(ctx.gpr[6] & 0xFFFFu);
    const std::uint32_t third = swap_half_bytes(ctx.gpr[7] & 0xFFFFu);
    put16(rt, word(rt, self) + 0x30u, first);
    put16(rt, word(rt, self) + 0x32u, second);
    const std::uint32_t holder = word(rt, self);
    put16(rt, holder + 0x34u, third);
    ctx.gpr[2] = holder;
}
// 0x088D5D54 (a0 = holder, a1-a3 = three outputs): the halfwords at +0x30, +0x32 and +0x34 of the word the holder points at
// are byte-swapped out to a1, a2 and a3; v0 = the last one.
void func_088D5D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put16(rt, ctx.gpr[5], swap_half_bytes(half_u(rt, word(rt, self) + 0x30u)));
    put16(rt, ctx.gpr[6], swap_half_bytes(half_u(rt, word(rt, self) + 0x32u)));
    ctx.gpr[2] = swap_half_bytes(half_u(rt, word(rt, self) + 0x34u));
    put16(rt, ctx.gpr[7], ctx.gpr[2]);
}
// 0x08917F78 (a0 = object, a1 = base, a2 = index): func_08928194(object, base[index * 4][8]) runs, then v0 = object.
void func_08917F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t entry = ctx.gpr[5] + (ctx.gpr[6] << 2);
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    (void)call_set(rt, ctx, 0x08928194u, {self, word(rt, entry + 0x8u), entry});
    ctx.gpr[2] = self;
}
void func_08AC0194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[2] = 0x08CD6450u;
        put(rt, self, 0x08CD6450u);
        (void)call_set(rt, ctx, 0x08AC00D8u, {self});
    }
    tail_call(ctx, 0x08B28BE0u, {self + 0x4u});
}
// 0x08B48BFC (a0 = holder): v0 = one of three words next to 0x08CB5660 by the holder's first word: 1 -> 0x08CB5660,
// 0 -> 0x08CB5668, anything else -> 0x08CB5664.
void func_08B48BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t selector = word(rt, ctx.gpr[4]);
    std::uint32_t result;
    if (selector == 0u) {
        result = 0x08CB5668u;
    } else if (selector == 0x1u) {
        result = 0x08CB5660u;
    } else {
        result = 0x08CB5664u;
    }
    ctx.gpr[2] = result;
}
// 0x08B76A8C (a0 = holder): with the holder's word zero v0 = 0 and the function returns; otherwise func_08B76A84() runs and
// func_08B35254(its result) is a tail call.
void func_08B76A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = word(rt, self);
        ctx.gpr[2] = value;
        if (value == 0u) return;
        value = call_set(rt, ctx, 0x08B76A84u, {self});
    }
    tail_call(ctx, 0x08B35254u, {value});
}
// 0x08BBD380 (a0 = output slot, a1, a2, a3): func_08BBD2DC(a1, (s8) a2, a3) runs under a 0x10 frame and its result goes to
// a0's word; v0 = it.
void func_08BBD380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t slot = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = slot;
    const std::uint32_t answer = call_set(rt, ctx, 0x08BBD2DCu, {ctx.gpr[5], sext8(ctx.gpr[6]), ctx.gpr[7]});
    put(rt, slot, answer);
    ctx.gpr[2] = answer;
}

// --- a virtual call through a register argument ----------------------------------------------------------------------------

} // namespace
} // namespace new_batch_11
} // namespace gt::game

#define NEW11(address, name, kind) GT_DECOMP(address, gt::game::new_batch_11::name, gt::ReturnKind::kind)
NEW11(0x08A66AD0u, func_08A66AD0, Word);
NEW11(0x08A67244u, func_08A67244, Word);
NEW11(0x08A67454u, func_08A67454, Word);
NEW11(0x08A80288u, func_08A80288, Word);
NEW11(0x08A86C50u, func_08A86C50, Word);
NEW11(0x08ABCC1Cu, func_08ABCC1C, Word);
NEW11(0x08AC2BACu, func_08AC2BAC, Word);
NEW11(0x08AD08A4u, func_08AD08A4, Word);
NEW11(0x08BB8B40u, func_08BB8B40, Word);
NEW11(0x08BB8B6Cu, func_08BB8B6C, Word);
NEW11(0x08BB8B98u, func_08BB8B98, Word);
NEW11(0x08BB8D28u, func_08BB8D28, Word);
NEW11(0x08BB8E40u, func_08BB8E40, Word);
NEW11(0x08C31B00u, func_08C31B00, Word);
NEW11(0x08C31C08u, func_08C31C08, Word);
NEW11(0x0888B85Cu, func_0888B85C, Word);
NEW11(0x088D0674u, func_088D0674, Word);
NEW11(0x08918340u, func_08918340, Word);
NEW11(0x0891A508u, func_0891A508, Word);
NEW11(0x08A8DBF8u, func_08A8DBF8, Word);
NEW11(0x08A8DC28u, func_08A8DC28, Word);
NEW11(0x08A8DC58u, func_08A8DC58, Word);
NEW11(0x08A8DC88u, func_08A8DC88, Word);
NEW11(0x08AA5DB4u, func_08AA5DB4, Word);
NEW11(0x08AD96D8u, func_08AD96D8, Word);
NEW11(0x08B102D0u, func_08B102D0, Word);
NEW11(0x08B1202Cu, func_08B1202C, Word);
NEW11(0x08B28CD4u, func_08B28CD4, Word);
NEW11(0x08BEF08Cu, func_08BEF08C, Word);
NEW11(0x088D5AC8u, func_088D5AC8, Word);
NEW11(0x088D5D54u, func_088D5D54, Word);
NEW11(0x08917F78u, func_08917F78, Word);
NEW11(0x08AC0194u, func_08AC0194, Word);
NEW11(0x08B48BFCu, func_08B48BFC, Word);
NEW11(0x08B76A8Cu, func_08B76A8C, Word);
NEW11(0x08BBD380u, func_08BBD380, Word);
