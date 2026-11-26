// Main-session batch 1610: 30 functions drafted by hand from `tools.disasm` as structured C (draft only, unverified; the gate
// decides). Six keep their earlier drafts (func_08B102D0, func_08B1202C, func_08B28CD4, func_08BEF08C, func_088D5AC8,
// func_088D5D54) where the asm agrees with them. func_08ADB0D8 is re-drafted: its v0 is the value of the first lookup, not
// the block the second call receives. setjmp-like bodies are not drafted.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1610 {
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
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
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

// --- forwarders and small stores -------------------------------------------------------------------------------------------

// 0x08AD9708 (f12 = value): func_2D55A0(value) runs, then func_2D55EC(1, value) answers in f0.
void func_08AD9708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float saved_f20 = ctx.fpr[20];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[20] = ctx.fpr[12];
    (void)call_keep(rt, ctx, 0x08AD95A0u, {}, {});
    (void)call_setf(rt, ctx, 0x08AD95ECu, {1u}, {ctx.fpr[20]});
    ctx.fpr[20] = saved_f20;
}

// 0x08AF4B1C (a0 = object): slot 8 of the object's vtable runs with a0 = a stack temporary, then func_08AF4B0C(temporary).
void func_08AF4B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[5] = self;
    const std::uint32_t slot = word(rt, word(rt, self) + 0x8u);
    (void)call_set(rt, ctx, slot, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08AF4B0Cu, {frame.at(0u)});
}

// 0x08B2BCA0 (a0 = object): func_08B344E0(object) and func_08B2BC5C(object) run, then func_08B3457C(object) is a tail call.

// 0x08AC1130 (a0-a2 pass through, a3 = a byte): four zero words at a stack block, then func_08AC1060(a0, a1, a2, block, byte).

// func_X(a0 = source, a1 = output): func_X2(a0, a1) answers in v0, which is stored at a1 (a halfword or a byte); v0 = a0.
void call_then_store(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, bool halfword) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = out;
    (void)call_keep(rt, ctx, target, {self, out});
    if (halfword) {
        put16(rt, out, ctx.gpr[2]);
    } else {
        put8(rt, out, ctx.gpr[2]);
    }
    ctx.gpr[2] = self;
}
// 0x08AA646C (a0 = source, a1 = output halfword): func_2A243C(a0, a1) answers; the halfword goes to a1; v0 = a0.
// 0x08AA6538 (a0 = source, a1 = output byte): func_2A24DC(a0, a1) answers; the byte goes to a1; v0 = a0.

// 0x08AC1164 (t0 = pointer to two words, a0-a3 pass through, t1 = a byte): the two words go to a stack block with zeros at +8 and
// +0xC, then func_08AC0F08(a0, a1, a2, a3, block, byte).

// 0x08B09618 (a0 = object, a1 = argument): func_08B09264(object) runs, then func_08B0957C(object, argument) is a tail call.
void func_08B09618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        (void)call_keep(rt, ctx, 0x08B09264u, {self});
    }
    tail_call(ctx, 0x08B0957Cu, {self, argument});
}

// 0x08B0FBC8 (a0-a2 pass through, a3 = pointer to two words, t0 = a byte): the two words go to a stack block with zeros at 0 and 4,
// then func_08B0F840(a0, a1, a2, block, byte).

// 0x0880BF34 (a0 = object): func_08A5B9EC(object + 0x208), func_088697E8(object + 0x30) and func_0880BEE8(object + 0x28) run,
// then func_08B28DC4(object) is a tail call.

// 0x08ADB0D8 (no arguments): func_08AEB808() runs; s0 = func_08AD8C5C(0x08D40610, 0), func_08AEB82C() runs, and v0 = s0.
void func_08ADB0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08AEB808u, {});
    ctx.gpr[16] = call_set(rt, ctx, 0x08AD8C5Cu, {0x08D40610u, 0u});
    (void)call_keep(rt, ctx, 0x08AEB82Cu, {});
    ctx.gpr[2] = ctx.gpr[16];
}

// 0x08AF75DC (a0 = object): func_08C3FF20(base, base + (index << 5), block) with base = object[+0x1C], index = the halfword at +0x16
// and block = { 0x08AF7290, 0 } on the stack.
void func_08AF75DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = half_u(rt, self + 0x16u);
    const std::uint32_t base = word(rt, self + 0x1Cu);
    const std::uint32_t block = frame.at(0u);
    put(rt, block, 0x08AF7290u);
    put(rt, block + 4u, 0u);
    (void)call_set(rt, ctx, 0x08C3FF20u, {base, base + (index << 5), block});
}

// func_X(a0 = object, a1 = argument): func_Y(object, 0x08CF6ED8) runs, then func_Z(object, argument) is a tail call.
void call_const_then_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first_call, std::uint32_t tail) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = other;
        ctx.gpr[17] = self;
        (void)call_set(rt, ctx, first_call, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, tail, {self, other});
}
// 0x088588AC (a0 = object, a1 = argument): func_08A76AB4(object, 0x08CF6ED8) runs, then func_08858784(object, argument) is a tail call.
// 0x08974918: func_089D8A60(object, 0x08CF6ED8) runs, then func_0897482C(object, argument) is a tail call.
// 0x0897DBDC: func_08A76AB4(object, 0x08CF6ED8) runs, then func_0897DAA4(object, argument) is a tail call.
// 0x089898DC: func_08969190(object, 0x08CF6ED8) runs, then func_08989620(object, argument) is a tail call.
// 0x08992860: func_08A0D868(object, 0x08CF6ED8) runs, then func_08992754(object, argument) is a tail call.
// 0x08995594: func_089948F4(object, 0x08CF6ED8) runs, then func_089953B4(object, argument) is a tail call.

// 0x0890F838 (a0 = first, a1 = second, a3 = third): func_0890F664(first, second, third) runs, func_08AB8774(second, 0) runs,
// then func_08AB8768(second) is a tail call with f12 = 30.0.

// 0x08917F78 (a0 = object, a1 = table, a2 = index): func_08928194(object, word at table + index * 4 + 8) runs; v0 = object.
void func_08917F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t entry = ctx.gpr[5] + (ctx.gpr[6] << 2);
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    (void)call_keep(rt, ctx, 0x08928194u, {self, word(rt, entry + 8u)});
    ctx.gpr[2] = self;
}

// 0x08AC0194 (a0 = object): object[0] = 0x08CD6450, func_08AC00D8(object) runs, then func_08B28BE0(object + 4) is a tail call.
void func_08AC0194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CD6450u);
        (void)call_set(rt, ctx, 0x08AC00D8u, {self});
    }
    tail_call(ctx, 0x08B28BE0u, {self + 4u});
}

// 0x08B48BFC (a0 = pointer to a word): the word 0 gives 0x08CB5668, 1 gives 0x08CB5660, any other value 0x08CB5664; v0 = that.
void func_08B48BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kind = word(rt, ctx.gpr[4]);
    if (kind == 0u) {
        ctx.gpr[2] = 0x08CB5668u;
    } else if (kind == 1u) {
        ctx.gpr[2] = 0x08CB5660u;
    } else {
        ctx.gpr[2] = 0x08CB5664u;
    }
}

// 0x08B76A8C (a0 = pointer to a word): v0 = the word; a non-null word goes to func_08B76A84(pointer), then func_08B35254(answer)
// is a tail call. A null word returns as is.

// 0x08BBD380 (a0 = output pointer, a1 = argument, a2 = a byte, a3 = argument): func_08BBD2DC(a1, sign-extended byte, a3) answers;
// the answer is stored at the output pointer and v0 = that answer.
void func_08BBD380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = out;
    const std::uint32_t result = call_set(rt, ctx, 0x08BBD2DCu, {ctx.gpr[5], sext8(ctx.gpr[6]), ctx.gpr[7]});
    put(rt, out, result);
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

} // namespace
} // namespace new_batch_main_1610
} // namespace gt::game

#define NEW1610(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1610::name, gt::ReturnKind::kind)
NEW1610(0x08AD9708u, func_08AD9708, Float);
NEW1610(0x08AF4B1Cu, func_08AF4B1C, Word);
NEW1610(0x08B09618u, func_08B09618, Word);
NEW1610(0x08ADB0D8u, func_08ADB0D8, Word);
NEW1610(0x08AF75DCu, func_08AF75DC, Word);
NEW1610(0x08917F78u, func_08917F78, Word);
NEW1610(0x08AC0194u, func_08AC0194, Word);
NEW1610(0x08B48BFCu, func_08B48BFC, Word);
NEW1610(0x08BBD380u, func_08BBD380, Word);
NEW1610(0x08B102D0u, func_08B102D0, Word);
NEW1610(0x08B1202Cu, func_08B1202C, Word);
NEW1610(0x08B28CD4u, func_08B28CD4, Word);
NEW1610(0x08BEF08Cu, func_08BEF08C, Word);
NEW1610(0x088D5AC8u, func_088D5AC8, Word);
NEW1610(0x088D5D54u, func_088D5D54, Word);
