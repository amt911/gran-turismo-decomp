// ---- from main-session batch 297 (draft 4ba64c8c0089cec0980629754bdfc78364b50e62) ----
// New batch (main session 297): two large hand-written functions, written from `tools.disasm`. Delay slots are read as
// "before the call / branch" (including the branch-likely nullified ones), `v0` is set where the asm leaves it, and
// every callee-saved register the original keeps a value in (s0-s7, fp, f20-f21) is written where the asm writes it and
// put back at the end. The register `$a1` is read in a few places without a preceding load (`andi v0, $a1, 2`); those
// reads model the register, not a variable the asm loaded.
// Audits: `tools.sp_loads` lists the seven `lw $s2, 0x54($sp)` reloads of func_0897C698; they read back the state word
// the function stores at sp + 0x54 (modelled as `state_slot`), not a callee-saved save.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_297 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call that keeps v0 / v1 / f0: the caller sees the callee's answers.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// The argument registers are set like the original sets them before the `jal` (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
// `lb`: the byte sign-extended.
inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them (a callee may read them) and the
// caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// f20-f31 are callee-saved too; the versions that touch them put them back at the end.
class SavedFpr {
public:
    explicit SavedFpr(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFpr() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFpr(const SavedFpr &) = delete;
    SavedFpr &operator=(const SavedFpr &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x088BFE70 (a0 = the object, a1 = the two-float position that is read and steered): the steering/obstacle pass.
// The record at *(a0 + 0x24) holds the axis state (byte +0x10, +0x15, +0x51, +0x94, +0x96), the motion weight
// (float +0x20), the distance band (floats +0x58 / +0x5C), the turn-back rate (float +0x7C) and the last steering
// offsets (floats +0x80 / +0x84). *(a0 + 8) is the motion holder (+0x10..0x18 source point, +0x4F4 rate), *(a0 + 0x14)
// the object whose byte +8 / +9 gate the band and the reaction. The first 0x088BF1FC call fills sp + 0xC (the source
// point handed to both calls); the second fills sp + 4 (its x) and sp + 8 (its y). v0 is 0, or the obstacle result when
// the axis kind at +0x96 is none of 1, 2, 5.

// 0x0897C698 (a0 = the handler, a1 = the index, a2 = the held-button object, a3 = the record whose word +0x138 holds
// the option bits): spawns one holder per button that went down or up, from four 8-byte entry tables at 0x08CDE83C
// (four entries), 0x08CDE85C (four), and the two effect tables at 0x08CDE7C8 / 0x08CC62C8 (each ending on the sentinel
// 0x00FFFFFF in the word at the cursor, the mask and held id twelve and eight bytes below it). A holder is a 0x40-byte
// object with vtable 0x08CC9758 or a 0x3C-byte one with vtable 0x08CC9888, built by 0x089D6378(holder, a2, a3, id,
// a1 | 0x10, 0, f12, f13) with the two floats 0x089AA95C answered, wrapped by 0x089D5E8C into a stack slot and handed to
// 0x0897783C before 0x08A5B9EC releases the wrapper. v0 is the state word at sp + 0x54 (1 once a holder was spawned), or
// 1 from the axis-table hit at 0x08CDE83C.
} // namespace
} // namespace new_batch_main_297
} // namespace gt::game

#define MAIN297(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_297::name, gt::ReturnKind::kind)

// ---- from main-session batch 298 (draft 6ae91ee3f322dc5c66c6ea2755ed1309b2d40533) ----
// New batch 298 (main session): two functions no route reaches, written by hand from `tools.disasm` as structured C.
// This is a draft batch: the differential gate has not run on it.
//
//   0x088107A4  formats a value into the object's text slot and draws its digits as quads (a quad atlas reached
//               through the object's +0x224 word), then ends in a tail call.
//   0x088E379C  the destructor body of a large object: releases every owned pointer, tears the element array at
//               +0xC0 down, drops the members' reference counts and destroys the trailing member objects.
//
// Skipped on purpose — 0x08C5EB14 (429 instructions): a libstdc++-shaped numeric-extraction state machine whose
// blocks are entered from the middle through shared labels (two stream-buffer iterators kept in -1-sentinel state,
// several loops with more than one entry edge each). A structured rewrite of that shape would be a guess, not a
// decompilation, so it is left recompiled.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_298 {
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
// A call that leaves v0/v1/f0 as the callee left them (the batch files' convention).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The FPU's callee-saved f20/f21: the originals keep them on their own stack across every call.
struct FloatSaved {
    AllegrexContext &ctx;
    float f20;
    float f21;
    explicit FloatSaved(AllegrexContext &c) : ctx(c), f20(c.fpr[20]), f21(c.fpr[21]) {}
    ~FloatSaved() {
        ctx.fpr[20] = f20;
        ctx.fpr[21] = f21;
    }
};
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// The compiler's signed division by a constant (`mult` + `mfhi`, then the `sra` and the sign correction the
// original applies): 0x66666667 with a shift of 2 is `value / 10`, 0x2AAAAAAB with a shift of 0 is `value / 6`.
s32 divide_magic(s32 value, u32 magic, unsigned shift) {
    const s32 high = static_cast<s32>((static_cast<std::int64_t>(value) * static_cast<s32>(magic)) >> 32);
    return (high >> shift) - (value < 0 ? 1 : 0);
}

// ---- 0x088107A4 ----

// 0x089DB078 (a0 = the drawing context, a1 = the quad's slot index, a2, a3, f12-f15 = the four words at
// `floats_at`): it writes into a0's display list and reads neither a2 nor a3, so the values passed there cannot
// change the result (the original leaves the previous row in a2 on the 8/0xC/0x10 blocks, the row count in a3).
void draw_quad(Runtime &rt, AllegrexContext &ctx, u32 object, u32 index, u32 floats_at, u32 a2, u32 a3) {
    ctx.gpr[6] = a2;
    ctx.gpr[7] = a3;
    (void)call_result(rt, ctx, 0x089DB078u, {object, index, a2, a3},
                      {float_at(rt, floats_at), float_at(rt, floats_at + 4u), float_at(rt, floats_at + 8u),
                       float_at(rt, floats_at + 0xCu)});
}
// The quad atlas' row count: the float at +8, through the original's `trunc.w.s`.
u32 atlas_rows(Runtime &rt, u32 atlas) { return static_cast<u32>(trunc_w_s(float_at(rt, atlas + 8u))); }
// The atlas keeps its rows in reverse digit order: the row of `digit` is at (digit - (rows - 1)) * 16.
u32 digit_row(u32 atlas, u32 digit, u32 rows) { return atlas + ((digit - (rows - 1u)) << 4); }
// The digit rows of 0x088107A4 keep their four words 0x300 bytes into the row block.
u32 digit_quads(u32 atlas, u32 digit, u32 rows) { return digit_row(atlas, digit, rows) + 0x300u; }
// A zero digit draws the first row of the constant glyph table 0x08CE29F0 instead of an atlas row (its four words
// are at the row itself, not 0x300 into it).
u32 zero_else_digit_quads(u32 atlas, u32 digit, u32 rows) {
    return digit == 0u ? 0x08CE29F0u : digit_quads(atlas, digit, rows);
}

// 0x088107A4 (a0 = the drawing context, a1 = the object whose value is drawn, a2 = the value, a3 = the value
// compared with a2 when a2 is not negative, t0/t1 = two limits): when 0x089DDC94 does not report 0x18 entries the
// object's text slot is rebuilt through 0x089DE054/0x089DE1BC and twelve characters are formatted into it
// (0x0880E024/0x0880E0F8); then the value's digits are drawn as quads at slots 0, 2, ... 0x12 and the function ends
// in a tail call (0x089DB5CC, or 0x089DB078 on the saturated path). The magnitude picks the path: above
// 0x481907FE the seven quads come from the atlas' fixed rows, otherwise the digits are decomposed in base 10 (base
// 6 for one group) with the saturation limits 5,999,999 / 599,999.
void func_088107A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const s32 value = static_cast<s32>(ctx.gpr[6]);
    const u32 compared = ctx.gpr[7];
    const s32 high_limit = static_cast<s32>(ctx.gpr[8]);
    const s32 low_limit = static_cast<s32>(ctx.gpr[9]);
    GuestFrame frame(ctx, 0x50u);
    FloatSaved floats(ctx);
    put(rt, frame.at(0u), compared);  // $sp + 0 keeps a3 for the tail call's compare
    if (object == 0u) return;         // v0 keeps what the callee was called with
    const s32 remaining = high_limit - low_limit - 1;
    if (low_limit > 0 && remaining <= 0) {
        const u32 flags = word(rt, object + 0xE0u) & ~(1u << 14);
        put(rt, object + 0xE0u, flags);
        ctx.gpr[2] = flags;
        return;
    }
    const u32 text = self + 0x224u;
    const s32 magnitude = value >= 0 ? value : static_cast<s32>(0u - static_cast<u32>(value));  // `max a2, -a2`
    if (call(rt, ctx, 0x089DDC94u, {object}) != 0x18u) {
        put(rt, object + 0xE8u, 6u);
        call(rt, ctx, 0x089DE054u, {object, 0x18u});
        call(rt, ctx, 0x089DE1BCu, {object, 0x18u});
        const float f21 = 0.0f;
        float f20 = call_result(rt, ctx, 0x0880E024u, {text, object, 0xCu, 0x30u}, {0.0f, 0.0f}).f0 + f21;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0xAu, 0x30u}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x14u, 0x27u}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x8u, 0x30u}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x6u, 0x30u}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x16u, 0x2Eu}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x4u, 0x30u}, {f20, f21}).f0;
        f20 += call_result(rt, ctx, 0x0880E024u, {text, object, 0x2u, 0x30u}, {f20, f21}).f0;
        (void)call_result(rt, ctx, 0x0880E024u, {text, object, 0x0u, 0x30u}, {f20, f21});
        // 0x0880E0F8 takes the width, a scale and a step in f12-f14 and returns its width in f0.
        const float last = call_result(rt, ctx, 0x0880E0F8u, {text, object, 0x10u, 0x30u}, {-25.0f, 1.0f, 0.9f}).f0;
        const float previous = call_result(rt, ctx, 0x0880E0F8u, {text, object, 0xEu, 0x30u}, {last, 1.0f, 0.9f}).f0;
        (void)call_result(rt, ctx, 0x0880E0F8u, {text, object, 0x12u, 0x3Au}, {previous, 1.0f, 0.9f});
    }
    put(rt, object + 0xE0u, word(rt, object + 0xE0u) | 0x4000u);
    if (static_cast<s32>(0x481907FEu) < magnitude) {
        // Saturated path: the atlas' seven fixed rows (its four words at the row itself), then two rows of the
        // constant glyph table 0x08CE29F0 and a tail call that draws the third.
        call(rt, ctx, 0x089DB5CCu, {object, word(rt, self + 0x23Cu)});
        u32 rows = 0u;
        for (u32 index = 0u; index != 0xEu; index += 2u) {
            const u32 atlas = word(rt, text);
            rows = atlas_rows(rt, atlas);
            draw_quad(rt, ctx, object, index, digit_row(atlas, 0x2Du, rows), rows, ctx.gpr[7]);
        }
        draw_quad(rt, ctx, object, 0xEu, 0x08CE29F0u, rows, ctx.gpr[7]);
        draw_quad(rt, ctx, object, 0x10u, 0x08CE29F0u, rows, ctx.gpr[7]);
        ctx.fpr[12] = float_at(rt, 0x08CE29F0u);
        ctx.fpr[13] = float_at(rt, 0x08CE29F0u + 4u);
        ctx.fpr[14] = float_at(rt, 0x08CE29F0u + 8u);
        ctx.fpr[15] = float_at(rt, 0x08CE29F0u + 0xCu);
        tail_call(ctx, 0x089DB078u, {object, 0x12u, rows, ctx.gpr[7]});
        return;
    }
    const u32 atlas = word(rt, text);
    const u32 rows = atlas_rows(rt, atlas);
    // Group one (slots 0..0xC): decimal digits, saturated at 5,999,999 / 599,999; the last group of six is base 6.
    u32 digit;
    u32 s1;
    if (magnitude > 5999999) {
        s1 = 599999u;
        digit = 9u;
    } else {
        s1 = static_cast<u32>(divide_magic(magnitude, 0x66666667u, 2u));
        digit = static_cast<u32>(magnitude) - s1 * 10u;
    }
    draw_quad(rt, ctx, object, 0u, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s1), 0x66666667u, 2u));
        digit = s1 - quotient * 10u;
        s1 = quotient;
        draw_quad(rt, ctx, object, 2u, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    u32 s0;
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s1), 0x66666667u, 2u));
        digit = s1 - quotient * 10u;
        s0 = quotient;
        draw_quad(rt, ctx, object, 4u, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    u32 s2;
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s0), 0x66666667u, 2u));
        digit = s0 - quotient * 10u;
        s2 = quotient;
        draw_quad(rt, ctx, object, 6u, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s2), 0x2AAAAAABu, 0u));
        digit = s2 - quotient * 6u;
        s0 = quotient;
        draw_quad(rt, ctx, object, 8u, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s0), 0x66666667u, 2u));
        digit = s0 - quotient * 10u;
        s1 = quotient;
        draw_quad(rt, ctx, object, 0xAu, digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    {
        const u32 quotient = static_cast<u32>(divide_magic(static_cast<s32>(s1), 0x66666667u, 2u));
        digit = s1 - quotient * 10u;
        draw_quad(rt, ctx, object, 0xCu, zero_else_digit_quads(atlas, digit, rows), digit_row(atlas, digit, rows), rows);
    }
    // Group two (slots 0xE..0x12): `remaining`, saturated at 9 / 99.
    {
        u32 tens = 9u;
        s0 = 9u;
        if (remaining < 100) {
            const u32 quotient = static_cast<u32>(divide_magic(remaining, 0x66666667u, 2u));
            tens = static_cast<u32>(remaining) - quotient * 10u;
            s0 = quotient;
        }
        draw_quad(rt, ctx, object, 0xEu, digit_quads(atlas, tens, rows), digit_row(atlas, tens, rows), rows);
    }
    draw_quad(rt, ctx, object, 0x10u, zero_else_digit_quads(atlas, s0, rows), digit_row(atlas, s0, rows), rows);
    // The last row is the atlas' fixed 0x3B row, whose four words sit at the row itself; a2 carries the row count.
    draw_quad(rt, ctx, object, 0x12u, atlas + ((0x3Bu - rows) << 4), rows, rows);
    // The tail call: the table at self + 0x23C, offset by a2's sign and by a2 matching a3.
    const u32 base = word(rt, self + 0x23Cu);
    u32 argument;
    if (value < 0) argument = base + 0x40u;
    else if (static_cast<u32>(value) == word(rt, frame.at(0u))) argument = base + 0x60u;
    else argument = base;
    ctx.gpr[6] = rows;
    ctx.gpr[7] = word(rt, frame.at(0u));
    tail_call(ctx, 0x089DB5CCu, {object, argument});
}

// ---- 0x088E379C ----

// The compiler's counted-object release, inlined at every member here: the count at +4 drops; the reference that
// took it to 0 disposes of the contents through vtable slot 2 (offset 8) and then drops the count at +8; the one
// that takes that to 0 calls vtable slot 3 (offset 0xC). The PSP does this with ll/sc; one core makes it a plain
// read-modify-write (src/core/refcount.cpp).
u32 decrement(Runtime &rt, u32 address) {
    const u32 before = word(rt, address);
    put(rt, address, before - 1u);
    return before;
}
u32 virtual_function(Runtime &rt, u32 object, u32 offset) { return word(rt, word(rt, object) + offset); }
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (decrement(rt, object + 4u) != 1u) return;
    call_indirect(rt, ctx, virtual_function(rt, object, 8u), {object});
    if (decrement(rt, object + 8u) != 1u) return;
    call_indirect(rt, ctx, virtual_function(rt, object, 0xCu), {object});
}
// `lw a0, off(a0); beql a0, zero, skip; jal release`: frees the member when it is not null and clears it.
void release_member(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset, u32 release) {
    const u32 member = word(rt, self + offset);
    if (member == 0u) return;
    call(rt, ctx, release, {member});
    put(rt, self + offset, 0u);
}
// The two-call variant: a teardown of the member and then the free.
void destroy_member(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset, u32 destroy, u32 release) {
    const u32 member = word(rt, self + offset);
    if (member == 0u) return;
    call(rt, ctx, destroy, {member});
    call(rt, ctx, release, {member});
    put(rt, self + offset, 0u);
}

// One element (0x2F8 bytes) of the array at +0xC0: the object at +0x2B0, the member at +0x2E8, and either a direct
// teardown or the counted one when the object's +0x2C count is not zero.
void destroy_element(Runtime &rt, AllegrexContext &ctx, u32 element) {
    const u32 object = word(rt, element + 0x2B0u);
    put(rt, element + 0x2E8u, 0x08CC1AA8u);
    call(rt, ctx, 0x08B34888u, {object});
    if (word(rt, object + 0x2Cu) == 0u) {
        call(rt, ctx, 0x08B34B24u, {object});
        const u32 holder = word(rt, object + 0x34u);
        const u32 other = word(rt, object + 0x38u);
        if (other != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, other)), {other, holder});
            put(rt, object + 0x38u, 0u);
            put(rt, object + 0x34u, 0u);
        }
        call(rt, ctx, 0x08B28BE0u, {object});
        call(rt, ctx, 0x08B64654u, {object});
        return;
    }
    const u32 count = word(rt, object + 0x2Cu) - 1u;
    put(rt, object + 0x2Cu, count);
    u32 owned = 0u;
    u32 inner = 0u;
    if (count == 0u) {
        const u32 holder = word(rt, object + 0x34u);
        if (holder != 0u) {
            inner = word(rt, object + 0x38u);
            owned = holder;
            put(rt, object + 0x34u, 0u);
            put(rt, object + 0x38u, 0u);
        }
        if (rt.memory().load8(object + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {object});
    }
    call(rt, ctx, 0x08B34B24u, {object});
    if (owned != 0u) call_indirect(rt, ctx, virtual_function(rt, owned, 8u), {owned});
    if (inner != 0u) call_indirect(rt, ctx, virtual_function(rt, inner, 0u), {inner, owned});
}

// 0x088E379C (a0 = the object): the destructor body. Two base-part destructors, every owned pointer released, the
// element array at +0xC0 (four 0x2F8-byte elements filled through 0x088E8184, then every element the count at -4
// announces destroyed backwards), the shared members' reference counts dropped, two allocator-carrying members
// released through 0x08B8FE44 + 0x08B35254, and the trailing member objects destroyed. v0 = the last callee's.

} // namespace
} // namespace new_batch_main_298
} // namespace gt::game

#define NEW298(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_298::name, gt::ReturnKind::kind)
NEW298(0x088107A4u, func_088107A4, Word);

// ---- from main-session batch 299 (draft 78c314497340baaf60968ad6fcb6a697cb6a3762) ----
// New batch main 299: two 500-instruction bodies written by hand from `tools.disasm` as structured C. Both are drafts: no
// gate has run them yet (the input shapes they need are written next to the batch's PR).
//
// Delay slots are read as "before the branch / call"; the callee-saved registers the original keeps values in are set where
// the asm sets them and put back at the end; v0 ends as the asm leaves it. No names: none has evidence.
//
// 0x0888329C: `self` (a0) gets the resource at a1 applied to its per-object table, through the object func_0886F8D4 answers
// (see the comment on the function). 0x08A425E4: the serializer of a DOM node into an XML-like string (see its comment).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_299 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// --- shared helpers ------------------------------------------------------------------------------------------------------
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the caller's registers as after the original's `jal`.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with its argument registers set the way the original sets them before the `jal` (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments ($f12, $f13, ...) for a callee that reads them.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call, the `lw v1, 0(object); lw v0, slot(v1); jalr v0` shape: v1 = the vtable, v0 = the function, then the call.
inline u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// Four floats copied as words (the asm's four `lwc1` / `swc1` pairs).
inline void copy4(Runtime &rt, u32 to, u32 from) {
    for (u32 i = 0u; i < 4u; ++i) put(rt, to + 4u * i, word(rt, from + 4u * i));
}
// Allegrex `ins`: `size` bits of `value` (from bit 0) into `target` at `position`.
inline u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = ((1u << size) - 1u) << position;
    return (target & ~mask) | ((value << position) & mask);
}
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
inline s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and $fp are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// $f20 / $f21, which func_0888329C saves and restores.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx), f20_(ctx.fpr[20]), f21_(ctx.fpr[21]) {}
    ~SavedFloats() {
        ctx_.fpr[20] = f20_;
        ctx_.fpr[21] = f21_;
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float f20_;
    float f21_;
};

// --- 0x0888329C ----------------------------------------------------------------------------------------------------------
// One float row of the resource object: the `lw v1, 0(object); lw v0, 0xC4(v1); jalr v0` shape with a0 = the object,
// a1 = 0 and a2 = the record's signed halfword index; the answer is a pointer to four floats.
s32 signed_index(Runtime &rt, u32 address) { return signed_half(rt, address); }

u32 row_of(Runtime &rt, AllegrexContext &ctx, u32 object, u32 index) {
    return call_slot(rt, ctx, object, 0xC4u, {object, 0u, index});
}
// The one-time halfword fill of a record: up to four entries of the name table at `table`, looked up through the object's
// slot (`0x54` or `0x60`), stored at fields+2, +4, +6 and +8. A null entry or an empty name ends the fill.
void fill_halfwords(Runtime &rt, AllegrexContext &ctx, u32 object, u32 fields, u32 table, u32 slot) {
    for (u32 i = 0u; i < 4u; ++i) {
        const u32 entry = word(rt, table + 4u * i);
        if (entry == 0u) return;
        if (static_cast<std::int8_t>(rt.memory().load8(entry)) == 0) return;
        put16(rt, fields + 2u + 2u * i, call_slot(rt, ctx, object, slot, {object, entry}));
    }
}

// 0x0888329C (self = a0, resource = a1). A null a1 returns at once (v0 untouched). Otherwise func_0886F8D4(resource) answers
// an object, and a null answer returns too (v0 = 0). In this build both func_0886F8D4 and func_0886F8DC are `jr ra; move v0,
// zero` stubs, so the body below never runs; it is still the original's, and a mod that fills the stubs would exercise it.
// The 19 words at 0x08C899B4 (four quads, then three words) go to the frame; the index picks a 0x26-byte record at
// self + 0x1710 whose byte +0xC gates the one-time halfword fill above (the object's slot 0x40 > 0 selects slot 0x54, else
// slot 0x44 > 0 selects slot 0x60). The record's halfword indices are then read through slot 0xC4 (four floats each):
// +0xE / +0x10 / +0x12 are three axes (the second and third normalised, then crossed), +0x1A / +0x1C / +0x1E a position
// (the cross of the second and third goes to self + 0xC3C, the position's w into self + 0xC90), +0x20 the minimum and
// maximum of the second and third components at self + 0xC6C..0xC80, +0x22 a count in bits 10-13 of self + 0x16C0,
// +0x24 a four-float block at self + 0xCD0, and +0xA / +0x12 (stepping two bytes per wheel) four wheel bases that fill
// self + 0x1060 / 0x1074 and 0xDA8. v0 = self's flag word +0x16E0 with bits 0x1C set.
void func_0888329C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 resource = ctx.gpr[5];
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0x160u);
    ctx.fpr[21] = 0.0f;
    if (resource == 0u) return;
    const u32 object = call_set(rt, ctx, 0x0886F8D4u, {resource});
    if (object == 0u) return;
    for (u32 i = 0u; i < 0x40u; i += 4u) put(rt, frame.at(0xD0u + i), word(rt, 0x08C899B4u + i));
    for (u32 i = 0u; i < 3u; ++i) {
        put(rt, frame.at(0xD0u + 0x40u + 4u * i), word(rt, 0x08C899B4u + 0x40u + 4u * i));
    }
    u32 index = call_set(rt, ctx, 0x0886F8DCu, {resource});
    const u32 record = self + index * 38u + 0x1710u;
    if (byte_u(rt, record + 0xCu) == 0u) {
        index = call_set(rt, ctx, 0x0886F8DCu, {resource});
        const u32 again = self + index * 38u + 0x1710u;
        if (byte_u(rt, again + 0xCu) == 0u) {
            put8(rt, again + 0xCu, 1u);
            // a1 is still the resource here: the stub func_0886F8DC does not write it.
            if (static_cast<s32>(call_slot(rt, ctx, object, 0x40u, {object, resource})) > 0) {
                fill_halfwords(rt, ctx, object, again + 0xCu, frame.at(0xD0u), 0x54u);
            } else if (static_cast<s32>(call_slot(rt, ctx, object, 0x44u, {object, resource})) > 0) {
                fill_halfwords(rt, ctx, object, again + 0xCu, frame.at(0xD0u), 0x60u);
            }
        }
    }
    // s2 = the record's field pointer, which the wheels loop later steps by two bytes.
    u32 fields = record + 0xCu;
    copy4(rt, frame.at(0x40u), row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 2u))));
    copy4(rt, frame.at(0x30u), row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 4u))));
    copy4(rt, frame.at(0x20u), row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 6u))));
    put_float(rt, frame.at(0x4Cu), 1.0f);   // the first row's w
    put_float(rt, frame.at(0x3Cu), 0.0f);   // the second row's w, out of its length
    const float first_length = std::sqrt(call_setf(rt, ctx, 0x08B4A744u, {frame.at(0x30u)}, {}).f0);
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x30u)}, {1.0f / first_length});
    put_float(rt, frame.at(0x2Cu), 0.0f);   // the third row's w, out of its length
    const float second_length = std::sqrt(call_setf(rt, ctx, 0x08B4A744u, {frame.at(0x20u)}, {}).f0);
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x20u)}, {1.0f / second_length});
    (void)call_set(rt, ctx, 0x08B4A82Cu, {frame.at(0u), frame.at(0x20u), frame.at(0x30u)});
    // The 4x4 at +0x50: the cross, then the second, third and first axes as rows.
    put_float(rt, frame.at(0x50u), float_at(rt, frame.at(0u)));
    put_float(rt, frame.at(0x54u), float_at(rt, frame.at(4u)));
    put_float(rt, frame.at(0x58u), float_at(rt, frame.at(8u)));
    put_float(rt, frame.at(0x5Cu), 0.0f);
    copy4(rt, frame.at(0x60u), frame.at(0x20u));
    copy4(rt, frame.at(0x70u), frame.at(0x30u));
    copy4(rt, frame.at(0x80u), frame.at(0x40u));
    copy4(rt, frame.at(0x10u), frame.at(0u));
    put_float(rt, frame.at(0x1Cu), 0.0f);
    (void)call_set(rt, ctx, 0x08BA53CCu, {self + 0xC18u, frame.at(0x50u)});
    (void)call_set(rt, ctx, 0x08B4F5E0u, {self + 0xC08u, frame.at(0x50u)});
    put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 3u);
    // The position: three rows of slot 0xC4, the first axis' w forced to 1 and the second's to 0 for the lengths.
    if (signed_index(rt, fields + 0x1Au) >= 0) {
        copy4(rt, frame.at(0u), row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x1Au))));
        copy4(rt, frame.at(0x10u), row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x1Cu))));
        const u32 third = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x1Eu)));
        put_float(rt, frame.at(0x20u), float_at(rt, third));
        put_float(rt, frame.at(0x24u), float_at(rt, third + 4u));
        put_float(rt, frame.at(0x28u), float_at(rt, third + 8u));
        put_float(rt, frame.at(0x2Cu), float_at(rt, third + 0xCu));
        put_float(rt, self + 0xC90u, float_at(rt, frame.at(0xCu)) + 1.0f);
        put_float(rt, frame.at(0xCu), 1.0f);
        put_float(rt, frame.at(0x1Cu), 0.0f);
        const float third_length = std::sqrt(call_setf(rt, ctx, 0x08B4A744u, {frame.at(0x10u)}, {}).f0);
        (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x10u)}, {1.0f / third_length});
        put_float(rt, frame.at(0x2Cu), 0.0f);
        const float fourth_length = std::sqrt(call_setf(rt, ctx, 0x08B4A744u, {frame.at(0x20u)}, {}).f0);
        (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x20u)}, {1.0f / fourth_length});
        (void)call_set(rt, ctx, 0x08B4A82Cu, {frame.at(0x50u), frame.at(0x20u), frame.at(0x10u)});
        // The 4x4 at +0x90: that cross, then the second axis, the first and the position row.
        put_float(rt, frame.at(0xA0u), float_at(rt, frame.at(0x20u)));
        put_float(rt, frame.at(0xA4u), float_at(rt, frame.at(0x24u)));
        put_float(rt, frame.at(0xA8u), float_at(rt, frame.at(0x28u)));
        put_float(rt, frame.at(0xB0u), float_at(rt, frame.at(0x10u)));
        put_float(rt, frame.at(0xACu), float_at(rt, frame.at(0x2Cu)));
        put_float(rt, frame.at(0xB8u), float_at(rt, frame.at(0x18u)));
        put_float(rt, frame.at(0xB4u), float_at(rt, frame.at(0x14u)));
        put_float(rt, frame.at(0xC0u), float_at(rt, frame.at(0u)));
        put_float(rt, frame.at(0x90u), float_at(rt, frame.at(0x50u)));
        put_float(rt, frame.at(0xBCu), float_at(rt, frame.at(0x1Cu)));
        put_float(rt, frame.at(0xC8u), float_at(rt, frame.at(8u)));
        put_float(rt, frame.at(0x94u), float_at(rt, frame.at(0x54u)));
        put_float(rt, frame.at(0xC4u), float_at(rt, frame.at(4u)));
        put_float(rt, frame.at(0x40u), float_at(rt, frame.at(0x50u)));
        put_float(rt, frame.at(0x44u), float_at(rt, frame.at(0x54u)));
        put_float(rt, frame.at(0x98u), float_at(rt, frame.at(0x58u)));
        put_float(rt, frame.at(0x48u), float_at(rt, frame.at(0x58u)));
        put_float(rt, frame.at(0x4Cu), 0.0f);
        put_float(rt, frame.at(0x9Cu), 0.0f);
        put_float(rt, frame.at(0xCCu), float_at(rt, frame.at(0xCu)));
        (void)call_set(rt, ctx, 0x088822CCu, {frame.at(0x30u), frame.at(0x90u)});
        const u32 tag_block = self + 0xC3Cu;
        put_float(rt, tag_block, float_at(rt, frame.at(0x30u)));
        put_float(rt, tag_block + 0xCu, float_at(rt, frame.at(0x3Cu)));
        put_float(rt, tag_block + 4u, float_at(rt, frame.at(0x34u)));
        put_float(rt, tag_block + 8u, float_at(rt, frame.at(0x38u)));
        put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 0x40u);
    }
    // +8: the first two components scaled by pi/180 into self + 0xC60 / 0xC68 (the raw ones stay on the frame).
    if (signed_index(rt, fields + 8u) >= 0) {
        const u32 row = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 8u)));
        const float degrees = std::bit_cast<float>(0x3C8EFA35u);
        put_float(rt, frame.at(0x50u), float_at(rt, row));
        put_float(rt, self + 0xC60u, float_at(rt, row) * degrees);
        put_float(rt, frame.at(0x54u), float_at(rt, row + 4u));
        put_float(rt, self + 0xC68u, float_at(rt, row + 4u) * degrees);
        put_float(rt, frame.at(0x58u), float_at(rt, row + 8u));
        put_float(rt, frame.at(0x5Cu), float_at(rt, row + 0xCu));
        put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 0x2000u);
    }
    // +0x20: the second and third components clamped below at zero into self + 0xC7C / 0xC80, the first into +0xC6C/0xC70.
    if (signed_index(rt, fields + 0x20u) >= 0) {
        const u32 row = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x20u)));
        const float x = float_at(rt, row);
        put_float(rt, frame.at(0x50u), x);
        const float y = float_at(rt, row + 4u);
        put_float(rt, frame.at(0x54u), y);
        const float z = float_at(rt, row + 8u);
        put_float(rt, frame.at(0x58u), z);
        const float w = float_at(rt, row + 0xCu);
        put_float(rt, self + 0xC70u, x);
        put_float(rt, frame.at(0x5Cu), w);
        put_float(rt, self + 0xC6Cu, x);
        put_float(rt, self + 0xC7Cu, 0.0f < y ? y : 0.0f);
        put_float(rt, self + 0xC80u, 0.0f < z ? z : 0.0f);
        put_float(rt, self + 0xC88u, w);
        put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 0x780u);
    }
    // +0x22: the second component rounded into bits 10-13 of self + 0x16C0, the first into self + 0xC8C.
    if (signed_index(rt, fields + 0x22u) >= 0) {
        const u32 row = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x22u)));
        const float x = float_at(rt, row);
        const float y = float_at(rt, row + 4u);
        put_float(rt, frame.at(0x50u), x);
        put_float(rt, frame.at(0x54u), y);
        put_float(rt, frame.at(0x58u), float_at(rt, row + 8u));
        const s32 rounded = trunc_w_s(y + 0.5f);
        put_float(rt, self + 0xC8Cu, x);
        put(rt, self + 0x16C0u, insert_bits(word(rt, self + 0x16C0u), static_cast<u32>(rounded), 10u, 4u));
        put_float(rt, frame.at(0x5Cu), float_at(rt, row + 0xCu));
        put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 0x1800u);
    }
    // +0x24: four floats into self + 0xCD0.
    if (signed_index(rt, fields + 0x24u) >= 0) {
        const u32 row = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x24u)));
        const u32 block = self + 0xCD0u;
        put_float(rt, block, float_at(rt, row));
        put_float(rt, block + 4u, float_at(rt, row + 4u));
        put_float(rt, block + 8u, float_at(rt, row + 8u));
        put_float(rt, block + 0xCu, float_at(rt, row + 0xCu));
        put(rt, self + 0x16E0u, word(rt, self + 0x16E0u) | 0x4000u);
    }
    // Four wheels: indices +0xA and +0x12 of the record (two bytes further every wheel) are the wheel basis and its
    // reference point; func_08BA5174 stores the basis at self + 0x1074 + 0x24i, the reference row stays on the frame.
    const float degrees = std::bit_cast<float>(0x3C8EFA35u);
    for (u32 wheel = 0u; wheel < 4u; ++wheel) {
        const u32 basis = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0xAu)));
        copy4(rt, frame.at(0x50u), basis);
        const u32 reference = row_of(rt, ctx, object, static_cast<u32>(signed_index(rt, fields + 0x12u)));
        fields += 2u;
        copy4(rt, frame.at(0x40u), reference);
        put_float(rt, frame.at(0x4Cu), float_at(rt, reference + 0xCu));
        (void)call_set(rt, ctx, 0x08BA5174u, {self + 0x1074u + 0x24u * wheel, frame.at(0x50u)});
        const float x = float_at(rt, frame.at(0x40u));
        const float y = float_at(rt, frame.at(0x44u));
        const float z = float_at(rt, frame.at(0x48u));
        const float angle = call_setf(rt, ctx, 0x08B6728Cu, {self + 0x1074u + 0x24u * wheel},
                                      {float_at(rt, self + 0x16FCu + 4u * wheel) +
                                           ((x + float_at(rt, self + 0x170Cu + 4u * wheel)) * 0.5f) *
                                               float_at(rt, self + 0x16F8u),
                                       360.0f})
                                  .f0;
        put_float(rt, self + 0x16FCu + 4u * wheel, angle);
        put_float(rt, self + 0x170Cu + 4u * wheel, x);
        put_float(rt, self + 0x1060u + 0x24u * wheel, angle * degrees);
        put_float(rt, self + 0x1068u + 0x24u * wheel, y * degrees);
        put_float(rt, self + 0x1064u + 0x24u * wheel, z * degrees);
        put_float(rt, self + 0x106Cu + 0x24u * wheel, x * degrees);
        put_float(rt, self + 0xDA8u + 0xB8u * wheel, x * degrees);
        put_float(rt, frame.at(0x40u), angle);
    }
    const u32 flags = word(rt, self + 0x16E0u) | 0x1Cu;
    put(rt, self + 0x16E0u, flags);
    ctx.gpr[2] = flags;
}

// --- 0x08A425E4 ----------------------------------------------------------------------------------------------------------
// The empty game string: its rep sits at 0x08D74FD0, its data pointer 0xC bytes in.
constexpr u32 kEmptyStringRep = 0x08D74FD0u;

// Releases a game string holder: nothing for the empty string; otherwise the use count at data-4 goes down and, when it
// reached zero, the allocation at data-0xC is freed (func_08B8542C with a nothrow tag somewhere on the frame).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 tag) {
    const u32 data = word(rt, holder);
    if (data == kEmptyStringRep + 0xCu) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, tag});
}

// The `lw v1, 0(child); lw v0, 0xC(v1); jalr v0` shape that answers a child's name into `out_slot`: a0 = the answer's slot,
// a1 = the child.
u32 call_name(Runtime &rt, AllegrexContext &ctx, u32 child, u32 out_slot) {
    ctx.gpr[3] = word(rt, child);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    return call_set(rt, ctx, ctx.gpr[2], {out_slot, child});
}

// 0x08A425E4 (out = a0, node = a1, name = a2, depth = a3): serializes `node` into the game string at `out` (v0 = out). The
// name is a game string: when its rep's first word is zero (an empty name) the node is a container and only its children are
// written, each through `name = child->slot 0xC`, func_08A4101C(node, name) and func_08A42370(found, name, depth + 1).
// With a name, `depth` spaces of indentation are built at the frame +0x54 and moved into the holder at +0x48 (only when
// depth > 0), then "<" + name, then every attribute of func_08A41554(node): " " + name, "=\"", its value
// (func_08A40FB0(node, name)->+0x44) and "\""; then, if the node's own value (+0x44) or its children (func_08A416BC) are
// not empty, ">", the value, and one recursive write per child (with "\n" after the value and the indentation before the
// closing tag when there are children) followed by "</" + name + ">\n"; when both are empty, " />\n" instead. Every
// temporary string is released through its use count, and the "HObject" / "MDomNodeList" / "MDomNode" / "HArray" checks are
// func_08A5BB7C calls (a `jr ra` stub in this build, so the code continues with the null pointer, as the original does).
} // namespace
} // namespace new_batch_main_299
} // namespace gt::game

#define MAIN299(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_299::name, gt::ReturnKind::kind)
MAIN299(0x0888329Cu, func_0888329C, Word);

// ---- from main-session batch 300 (draft ebbf21f03c2a094e1225537488ba53ef57b21c99) ----
// New batch main-300 (draft, unverified): structured C from tools.disasm, in the style of new_batch_3.cpp.
//
// The batch named three functions. One is drafted here; two were skipped this pass, with what they are written down so the
// next attempt does not start from zero (both are far too entangled to transcribe by hand without a case the gate can run):
//  - 0x088877DC (460 insns, a0/a1/a2): a range insert into a container of 8-byte elements whose second word is a reference
//    counted pointer — the release sequence is the shared_ptr one (atomic use count at +4, weak count at +8, the virtual
//    dispose at vtable +8 and destroy at +0xC), the incoming range is a virtual container (a2: slots 0x18/0x1C/0x20/0x24)
//    and the work is handed to 0x08BA7B0C / 0x08BA82A8 / 0x088875CC. The refcount transfers and the copies interleave in a
//    way that decides the container's final contents, so a draft would be a guess.
//  - 0x08C5C4B4 (495 insns, eight arguments a0-a3/t0-t3): a merge over two {data, limit} ranges with a dynamic alloca of
//    4 * t1 entries, per-position virtual fetches (vtable slots 0x24/0x28) and a comparator, answering a two-word range.
// 0x08C518D4 is drafted below: libstdc++'s _Rb_tree::_M_insert_unique_(hint, value), whose source-level shape is known from
// its siblings (the tree_insert_hint family of hot_batch_103/123); the inlined key order is decoded from this function's own
// compare blocks.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_300 {
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

// The callee-saved registers the prologue saves (s0-s2) and the epilogue restores.
struct Saved {
    AllegrexContext &ctx;
    u32 regs[3];
    explicit Saved(AllegrexContext &c) : ctx(c), regs{c.gpr[16], c.gpr[17], c.gpr[18]} {}
    ~Saved() { for (unsigned i = 0; i < 3; ++i) ctx.gpr[16 + i] = regs[i]; }
};

// The tree's key, 16 bytes (at node + 0x10; the value argument of the insert starts with it): a flag byte at +0, the bytes
// themselves at +4, a signed length at +8 and a word at +0xC that 0x08C51360 copies along but the order never reads.
// The order the compare blocks of 0x08C518D4 implement: the flag first (0 below non-zero), then the length (signed, the
// shorter key first, whatever its bytes say), and only when the lengths are equal the bytes (unsigned, first difference).
bool key_less(Runtime &rt, u32 left, u32 right) {
    const u32 left_flag = rt.memory().load8(left);
    const u32 right_flag = rt.memory().load8(right);
    if (left_flag == 0u && right_flag != 0u) return true;
    if (left_flag != 0u && right_flag == 0u) return false;
    const s32 left_length = static_cast<s32>(word(rt, left + 8u));
    const s32 right_length = static_cast<s32>(word(rt, right + 8u));
    if (left_length != right_length) return left_length < right_length;
    if (left_length <= 0) return false;
    const u32 left_data = word(rt, left + 4u);
    const u32 right_data = word(rt, right + 4u);
    for (s32 i = 0; i < left_length; ++i) {
        const u32 a = rt.memory().load8(left_data + static_cast<u32>(i));
        const u32 b = rt.memory().load8(right_data + static_cast<u32>(i));
        if (a < b) return true;
        if (b < a) return false;
    }
    return false;
}

// 0x08C518D4 (a0 = tree, a1 = hint, a2 = value): libstdc++'s _Rb_tree::_M_insert_unique_(hint, value) for the tree whose
// nodes are 0x20 bytes — left +0, right +4, parent +8, colour +0xC, the key at +0x10 — with the node count at tree +0x14 and
// end() the node at tree +4; the code reads the leftmost node at tree +0xC and the rightmost at tree +0x10. The arms are
// libstdc++'s: a hint at end() inserts after the rightmost node when the key is above it, else goes through the plain
// insert_unique; a key below the hint's takes the predecessor (0x08B5F174) — below it too, insert before the predecessor when
// it has no right child, else before the hint; a key above the hint's takes the successor (0x08B5F100) the same way; an equal
// key answers the hint. 0x08C51360(tree, x, y, value) links a new node, bumps the count and returns it (v1 = the new count);
// 0x08C515B4(tree, value) is the plain insert_unique and returns {v0 = iterator, v1 = inserted}.
// v0 = the iterator on every path. v1 is the callee's on the insert_unique paths (the original copies that iterator into it)
// and the predecessor's/successor's leftover otherwise; no caller reads it (0x08B3D8E4 is the one that calls this), so the
// registration compares v0 only.
void func_08C518D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 tree = ctx.gpr[4];
    const u32 hint = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const auto insert_at = [&](u32 x, u32 y) { return call(rt, ctx, 0x08C51360u, {tree, x, y, value}); };
    const auto insert_unique = [&] {
        const u32 node = call(rt, ctx, 0x08C515B4u, {tree, value});
        ctx.gpr[3] = node;  // the original copies the returned iterator into v1
        return node;
    };
    u32 result;
    if (hint == tree + 4u) {
        const u32 rightmost = word(rt, tree + 0x10u);
        if (word(rt, tree + 0x14u) != 0u && key_less(rt, rightmost + 0x10u, value)) result = insert_at(0u, rightmost);
        else result = insert_unique();
    } else if (key_less(rt, value, hint + 0x10u)) {
        if (hint == word(rt, tree + 0xCu)) {
            result = insert_at(hint, hint);
        } else {
            const u32 before = call(rt, ctx, 0x08B5F174u, {hint});
            if (!key_less(rt, before + 0x10u, value)) result = insert_unique();
            else if (word(rt, before + 0xCu) != 0u) result = insert_at(hint, hint);
            else result = insert_at(0u, before);
        }
    } else if (key_less(rt, hint + 0x10u, value)) {
        if (hint == word(rt, tree + 0x10u)) {
            result = insert_at(0u, hint);
        } else {
            const u32 after = call(rt, ctx, 0x08B5F100u, {hint});
            if (!key_less(rt, value, after + 0x10u)) result = insert_unique();
            else if (word(rt, hint + 0xCu) != 0u) result = insert_at(after, after);
            else result = insert_at(0u, hint);
        }
    } else {
        result = hint;  // equal keys: the hint is what the original answers
    }
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_main_300
} // namespace gt::game

#define NEW300(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_300::name, gt::ReturnKind::kind)
NEW300(0x08C518D4u, func_08C518D4, Word);

// ---- from main-session batch 301 (draft a8edf057bd86ca28f799d3e249f59e0784f30957) ----
// New batch main 301: two functions no route reaches, written by hand from `tools/disasm` as structured C. This is a
// draft batch, not yet verified by the differential gate.
//
// Both take pointers the synthetic generator cannot shape from the code alone: the shapes for them are in
// The input shapes the synthetic generator needs for them travel with the batch (the format of
// config/synth_shapes.txt, one line per address): 0x08B6D6B4's weight array and 0x08C17E04's comparator.
// Audits: `tools.frame_audit` 0 wrong, `tools.literal_check` 0 missing, `tools.arg_reads` clean, and `tools.sp_loads`
// reports two `lw $s0` the C++ models as the comparator copies' end pointer (0x08C18064 reads the heap copy's +0x40,
// 0x08C183C4 the per-element copy's +0x40; s0 is a scratch register there, not a saved one).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_301 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call with a float argument in $f12 that answers in $f0: the PSP's `x * 2^n` (0x08B6ECC4) and its floor
// (0x08B663B4), both decompiled already. The integer argument rides in a0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, s32 argument, float value) {
    const GuestResult result = call_guest(rt, ctx, target, {static_cast<u32>(argument)}, {value});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// The original's element copies (`lwc1`/`swc1` and `lw`/`sw` pairs): whole words, no float conversion.
void copy_words(Runtime &rt, u32 to, u32 from, u32 bytes) {
    for (u32 offset = 0u; offset < bytes; offset += 4u) put(rt, to + offset, word(rt, from + offset));
}

// 0x08C17E04's comparator, a 0x4C-byte object: +0x00 a word, +0x04..+0x30 floats with two flag bytes at
// +0x10/+0x11, +0x34/+0x38 words, +0x3C/+0x40/+0x44 a vector of 16-byte items (begin/end/capacity) whose storage
// 0x08A5BC20(bytes, 4) allocates and 0x08C15F9C(begin, end, storage, tag) fills, and +0x48 one more word.
// A by-value copy of it (0x08C17E24-0x08C17F44 and the same block four times more): every field but the two bytes
// +0x12/+0x13, which the original leaves as the stack had them.
void copy_comparator(Runtime &rt, AllegrexContext &ctx, u32 to, u32 from, u32 tag) {
    put(rt, to, word(rt, from));
    for (u32 offset = 0x04u; offset <= 0x0Cu; offset += 4u) put(rt, to + offset, word(rt, from + offset));
    put8(rt, to + 0x10u, byte_u(rt, from + 0x10u));
    put8(rt, to + 0x11u, byte_u(rt, from + 0x11u));
    for (u32 offset = 0x14u; offset <= 0x38u; offset += 4u) put(rt, to + offset, word(rt, from + offset));
    const u32 begin = word(rt, from + 0x3Cu);
    const u32 end = word(rt, from + 0x40u);
    const u32 bytes = (end - begin) & 0xFFFFFFF0u;  // `ins rt, zero, 0, 4`
    put(rt, to + 0x3Cu, 0u);
    put(rt, to + 0x40u, 0u);
    put(rt, to + 0x44u, 0u);
    const u32 storage = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    put(rt, to + 0x44u, storage + bytes);
    put(rt, to + 0x3Cu, storage);
    put(rt, to + 0x40u, storage);
    put(rt, to + 0x40u, call(rt, ctx, 0x08C15F9Cu, {begin, end, storage, tag}));
    put(rt, to + 0x48u, word(rt, from + 0x48u));
}
// 0x08BF5B28(begin, end, tag) closes the vector and 0x08A5BC54(begin, (capacity - begin) & ~0xF, 4) frees a non-empty
// storage; v0 ends as the last of those two calls left it.
void destroy_comparator(Runtime &rt, AllegrexContext &ctx, u32 object, u32 tag) {
    call(rt, ctx, 0x08BF5B28u, {word(rt, object + 0x3Cu), word(rt, object + 0x40u), tag});
    const u32 begin = word(rt, object + 0x3Cu);
    if (begin == 0u) return;
    const u32 bytes = static_cast<u32>(static_cast<s32>(word(rt, object + 0x44u) - begin) >> 4) << 4;
    call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
}

// 0x08C17E04 (a0 = first, a1 = middle, a2 = last, a3 = compare): libstdc++'s heap selection for std::partial_sort over
// 0x2C-byte elements whose comparator is passed by value. The heap [first, middle) is built with
// 0x08C174D4(first, hole, length, &value, &compare) from the last parent down (each call gets its own deep copy of the
// comparator); every element of [middle, last) that 0x08A15D54(compare, element, first) puts before the top replaces it
// (the top moves into the element's place) and sifts down from 0; finally 0x08C178B0(first, middle, &compare) sorts the
// heap. The count is (middle - first) / 0x2C through the compiler's inverse 0xBA2E8BA3 (low half only, as the asm's
// `mult`/`mflo`). v0 = the last call's result, as the original leaves it.
void func_08C17E04(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 middle = ctx.gpr[5];
    const u32 last = ctx.gpr[6];
    const u32 compare = ctx.gpr[7];
    GuestFrame frame(ctx, 0x250u);
    // Between the calls the comparator copies live at sp + 0x194 (the heap's), sp + 0xB0 (per heap step), sp + 0x148
    // (per element of [middle, last)) and sp + 0x64 (its working copy); the elements' values at sp + 0x38 and sp + 0xC.
    // The byte addresses sp + 0..sp + 9 are the allocator tags the original hands to each copy and destroy call.
    const s32 length = static_cast<s32>(static_cast<u32>(static_cast<s32>(middle - first) >> 2) * 0xBA2E8BA3u);
    copy_comparator(rt, ctx, frame.at(0x194u), compare, frame.at(9u));
    if (length >= 2) {
        const s32 top = (length - 2) / 2;
        for (s32 step = 0; step <= top; ++step) {
            const u32 parent = static_cast<u32>(top - step);
            copy_words(rt, frame.at(0x38u), first + parent * 0x2Cu, 0x2Cu);
            copy_comparator(rt, ctx, frame.at(0xB0u), frame.at(0x194u), frame.at(8u));
            call(rt, ctx, 0x08C174D4u, {first, parent, static_cast<u32>(length), frame.at(0x38u), frame.at(0xB0u)});
            destroy_comparator(rt, ctx, frame.at(0xB0u), frame.at(7u));
        }
    }
    destroy_comparator(rt, ctx, frame.at(0x194u), frame.at(6u));
    for (u32 element = middle; element < last; element += 0x2Cu) {
        if (call(rt, ctx, 0x08A15D54u, {compare, element, first}) == 0u) continue;
        // The element's old value is saved first: the top moves into its place, then the value sifts down from 0.
        copy_words(rt, frame.at(0xCu), element, 0x2Cu);
        copy_comparator(rt, ctx, frame.at(0x148u), compare, frame.at(5u));
        copy_words(rt, element, first, 0x2Cu);
        copy_comparator(rt, ctx, frame.at(0x64u), frame.at(0x148u), frame.at(4u));
        call(rt, ctx, 0x08C174D4u, {first, 0u, static_cast<u32>(length), frame.at(0xCu), frame.at(0x64u)});
        destroy_comparator(rt, ctx, frame.at(0x64u), frame.at(3u));
        destroy_comparator(rt, ctx, frame.at(0x148u), frame.at(2u));
    }
    copy_comparator(rt, ctx, frame.at(0xFCu), compare, frame.at(1u));
    call(rt, ctx, 0x08C178B0u, {first, middle, frame.at(0xFCu)});
    destroy_comparator(rt, ctx, frame.at(0xFCu), frame.at(0u));
}

// 0x08B6D6B4 (a0 = weights, a1 = out, a2 = bit, a3 = count, t0 = table index, t1 = values): a rolling-digit value. The
// `count` weights against the `count` int values ending at bit / 8 are convolved into rows + 1 sums
// (rows = the table's 0x08C9664C entry); the top row is carried down in base 256 into a digit array; the value is
// scaled by 2^bit, split into a wheel position (0..7) and a fraction, the fraction is carried back into the digits, the
// digits go through the 2^exponent scaling and the coefficient series at 0x08C96620, and the table index picks the three
// output floats: 0 leaves only out[0] (the sum of the series), 3 the accumulated wheel, 1, 2 and above 3 the pairs.
// v0 = the wheel position's low three bits.

} // namespace
} // namespace new_batch_main_301
} // namespace gt::game

#define NEWMAIN301(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_301::name, gt::ReturnKind::kind)
NEWMAIN301(0x08C17E04u, func_08C17E04, Word);

// ---- from main-session batch 304 (draft eeb193e6d7d498ff4c27b1e9e476da4f43b482c7) ----
// Main-session batch 304: two long functions (555 instructions each) of the game's container glue, written as structured C
// from tools.disasm.
//
// 0x08AAA2C0 builds a named registry: it clears the map living inline at this + 8 and inserts 17 entries, each a string
// temporary (a key) turned into a node {descriptor, 0} by func_08C338D4(this + 8, key) + func_08B62344(8), then walks the
// second name table under the count 0x08CDF7C0, filling every node's second word with its index.
//
// 0x08BB43C8 inserts a counted pointer (`value`) into a vector of counted pointers before `position`. The elements are
// use-counted nodes (count at +0x10, payload at +0xC); the storage of the vector and the nodes come from pool allocators:
// the vector's buffer from the first-fit pool behind func_08BB6964 (state 0x08D75F5C, banked in hot_batch_113, whose
// first-fit/grow body the original inlines here), and a released node's payload / the node itself from the size-class
// registry behind func_08BB1F48 (banked in hot_batch_81, hot_batch_92 and hot_batch_113) through func_08BB1D0C.
// The inlined allocate is done by calling func_08BB6964 (the same body); what differs from the original is only v0 inside
// pool_row on a cache hit, and no caller there leaves v0 unread: each goes on to another call first.
//
// Neither function has been verified by a gate yet: both read containers whose invariants the synthetic generator cannot
// build (an empty std::map header, a vector whose {begin, end, capacity} agree), so the shape lines in
// /tmp/claude-1000/shapes_main304.txt are best effort and the gate may report them "not comparable".
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_304 {
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

// -------------------------------------------------------------------------------------------------------------------------
// 0x08AAA2C0: the named registry's constructor.

// Releases the shared string whose representation pointer sits at `text_slot` (the libstdc++ copy-on-write layout: the rep is
// 0xC bytes before the data, its count at -4): the shared empty rep 0x08D74FD0 is left alone, otherwise func_08B60804(&count,
// -1) steps the count and func_08B8542C(rep, `scratch`) frees the rep when the old count was not positive.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 text_slot, u32 scratch) {
    const u32 text = word(rt, text_slot);
    const u32 rep = text - 0xCu;
    if (rep == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call(rt, ctx, 0x08B8542Cu, {rep, scratch});
}

// One entry of the registry: the string at `name_slot` (a slot of the static name table) is copied into the temporary at
// `text_slot` (its scratch byte at `scratch`), func_08C338D4(map, text) answers the map slot the name belongs to, an 8-byte
// node takes `descriptor` and 0 and is stored into that slot, and the temporary is released (its own release scratch byte is
// 0x12 below the constructor's: 0x23 and 0x11 in the first block, 0x13 and 0x1 in the last).
void add_entry(Runtime &rt, AllegrexContext &ctx, u32 map, u32 name_slot, u32 descriptor, u32 text_slot, u32 scratch) {
    (void)call(rt, ctx, 0x08B85EB8u, {text_slot, word(rt, name_slot), scratch});
    const u32 cell = call(rt, ctx, 0x08C338D4u, {map, text_slot});
    const u32 node = call(rt, ctx, 0x08B62344u, {8u});
    put(rt, node, descriptor);
    put(rt, node + 4u, 0u);
    put(rt, cell, node);
    release_string(rt, ctx, text_slot, scratch - 0x12u);
}

// 0x08AAA2C0 (this = a0): func_08AA97D4(this) resets this + 8's map, then the 17 name entries of the table 0x08CDF7C4 ..
// 0x08CDF804 are inserted in ascending address order (each with its own descriptor word 0x08CD53A8 + 0x20 * i, its own
// string temporary at frame + 0x68 - 4 * i and its own scratch byte at frame + 0x23 - i for the constructor, frame + 0x11 - i
// for the release), and finally the index loop over the second table 0x08CFF7D8 (count re-read from 0x08CDF7C0 every round)
// writes the position into each node's second word.
// v0 = 0 on every exit: the count it loads when that is already 0, the `sltu` that ends the loop otherwise.

// -------------------------------------------------------------------------------------------------------------------------
// 0x08BB43C8: the vector insert of a counted pointer.

// The size-class registry behind func_08BB1F48 (its rows at registry + 0x24 .. + 0x28, the row of the last size asked for
// cached at +0x34): the row for `size`, looked up by the binary search func_08BB1D0C with the comparator 0x08BB1B40 when the
// cache holds another size. `size` is written at `key_slot` as the original does.
u32 pool_row(Runtime &rt, AllegrexContext &ctx, u32 key_slot, u32 size) {
    const u32 registry = call(rt, ctx, 0x08BB1F48u);
    put(rt, key_slot, size);
    u32 row = word(rt, registry + 0x34u);
    if (row == 0u || word(rt, row + 0x10u) != size) {
        row = word(rt, call(rt, ctx, 0x08BB1D0Cu, {word(rt, registry + 0x24u), word(rt, registry + 0x28u), key_slot, 0x08BB1B40u}));
        put(rt, registry + 0x34u, row);
    }
    return row;
}

// Puts `item` on the free chain of `row`: the row's +0x1C count goes down, the chain head (+0xC) takes it and its first word
// keeps the old head. v0 = the new count.
u32 pool_push(Runtime &rt, u32 row, u32 item) {
    const u32 count = word(rt, row + 0x1Cu);
    const u32 head = word(rt, row + 0xCu);
    put(rt, row + 0x1Cu, count - 1u);
    put(rt, item, head);
    put(rt, row + 0xCu, item);
    return count - 1u;
}

// The release of one counted node: its count (+0x10) drops by one and, when that reached zero, its payload (+0xC) goes back
// to the 4-byte class, the word is cleared, func_08BB1FF0(node) runs (the chain release) and the node itself goes to the
// 0x14-byte class. v0 = the stepped count, or the last class count - 1.
u32 release_counted(Runtime &rt, AllegrexContext &ctx, u32 key_slot, u32 node) {
    const u32 count = word(rt, node + 0x10u) - 1u;
    put(rt, node + 0x10u, count);
    if (count != 0u) return count;
    const u32 payload = word(rt, node + 0xCu);
    const u32 payload_row = pool_row(rt, ctx, key_slot, 4u);
    if (payload != 0u) (void)pool_push(rt, payload_row, payload);
    put(rt, node + 0xCu, 0u);
    (void)call(rt, ctx, 0x08BB1FF0u, {node});
    const u32 node_row = pool_row(rt, ctx, key_slot, 0x14u);
    return pool_push(rt, node_row, node);
}

// The block pool's first-use set-up behind the guard byte 0x08D75F50: the state block at 0x08D75F5C gets its empty ring
// (head and the block vector's begin both point at state + 0xC), the growth size 0x1001 and zeroed counters.
void pool_setup(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 state = 0x08D75F5Cu;
    constexpr u32 guard = 0x08D75F50u;
    if (static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(guard))) != 0) return;
    if (call(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    put(rt, state + 0x18u, 0x1001u);
    put(rt, state, 0u);
    put(rt, state + 4u, 0u);
    put(rt, state + 8u, 0u);
    put(rt, state + 0x14u, 0x08D75F68u);
    put(rt, state + 0x1Cu, 0u);
    put(rt, state + 0x20u, 0u);
    put(rt, state + 0x24u, 0u);
    put(rt, state + 0xCu, 0x08D75F68u);
    put(rt, state + 0x10u, 0u);
    (void)call(rt, ctx, 0x08B64494u, {guard});
    (void)call(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
}

// The block pool's free step (state 0x08D75F5C): `block` (its size in 8-byte units at +4) goes back into the address-ordered
// ring at the head (+0x14), merged with the following / preceding block when they touch, with the in-use counter (+0x20)
// lowered and the head moved to the block it was inserted after.
void pool_release(Runtime &rt, u32 block) {
    constexpr u32 state = 0x08D75F5Cu;
    u32 at = word(rt, state + 0x14u);
    for (;;) {
        const bool below = at < block;
        const u32 next = word(rt, at);
        if (below && block < next) break;
        if (at < next) {
            at = next;
            continue;
        }
        if (below || block < next) break;
        at = next;
    }
    put(rt, state + 0x20u, word(rt, state + 0x20u) - word(rt, block + 4u));
    const u32 size = word(rt, block + 4u);
    const u32 after = word(rt, at);
    if (block + (size << 3u) != after) {
        put(rt, block, after);
    } else {
        put(rt, block + 4u, size + word(rt, after + 4u));
        put(rt, block, word(rt, after));
    }
    const u32 at_size = word(rt, at + 4u);
    if (block != at + (at_size << 3u)) {
        put(rt, at, block);
    } else {
        put(rt, at + 4u, at_size + word(rt, block + 4u));
        put(rt, at, word(rt, block));
    }
    put(rt, state + 0x14u, at);
}

// 0x08BB43C8 (vector = a0, position = a1, value = a2): inserts *value (a counted pointer, `value` points at the slot holding
// it) into the vector of counted pointers before `position`.
// With room (end != capacity) the last element is copied to [end] and end grows by one word (a null end sets it to 4), the
// bits are shifted right from the last element down to position + 1 — each step increments the moved node's count and
// releases the overwritten one (the loop runs ((end - 8) - position) / 4 times) — then the node's count is taken for the new
// slot, the old element at `position` released and the later reference of the inserted value dropped again.
// Without room: the capacity is 0x3FFFFFFF-reported (func_08B5ADDC(0x08CA0A48)) first, otherwise 1 element for an empty
// vector, twice the size when that did not overflow, the maximum when it did; the buffer comes from the block pool
// (func_08BB6964, the allocate body the original inlines), the head [begin, position) is copied with counts taken, the new
// element stored, the tail [position, end) copied behind it, the old range released (func_08BB24B0) and the old buffer given
// back to the pool (set up on first use). v0 = the new end of storage, or the leftover the original's last step leaves.
void func_08BB43C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const u32 key_slot = frame.at(4u);
    const u32 bytes_slot = frame.at(0x10u);
    const u32 end = word(rt, vector + 4u);
    u32 result = word(rt, vector + 8u);
    if (end == result) {
        u32 size = static_cast<u32>(static_cast<s32>(end - word(rt, vector)) >> 2);
        if (size == 0x3FFFFFFFu) size = call(rt, ctx, 0x08B5ADDCu, {0x08CA0A48u});
        u32 bytes;
        if (size == 0u) {
            bytes = 4u;
        } else {
            const u32 doubled = size << 1u;
            bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2u;
        }
        put(rt, bytes_slot, bytes);
        const u32 fresh = call(rt, ctx, 0x08BB6964u, {bytes});
        const u32 cursor = call(rt, ctx, 0x08BB2620u, {word(rt, vector), position, fresh});
        if (cursor != 0u) {
            const u32 item = word(rt, value);
            put(rt, cursor, item);
            if (item != 0u) put(rt, item + 0x10u, word(rt, item + 0x10u) + 1u);
        }
        const u32 new_end = call(rt, ctx, 0x08BB2620u, {position, end, cursor + 4u});
        (void)call(rt, ctx, 0x08BB24B0u, {word(rt, vector), end});
        const u32 old = word(rt, vector);
        if (old != 0u) {
            pool_setup(rt, ctx);
            pool_release(rt, old - 8u);
        }
        put(rt, vector + 4u, new_end);
        result = fresh + bytes;
        put(rt, vector + 8u, result);
        put(rt, vector, fresh);
        ctx.gpr[2] = result;
        return;
    }
    if (end == 0u) {
        put(rt, vector + 4u, 4u);
    } else {
        const u32 last = word(rt, end - 4u);
        put(rt, end, last);
        if (last != 0u) {
            result = word(rt, last + 0x10u) + 1u;
            put(rt, last + 0x10u, result);
        }
        put(rt, vector + 4u, end + 4u);
    }
    const u32 item = word(rt, value);
    if (item != 0u) {
        result = word(rt, item + 0x10u) + 1u;
        put(rt, item + 0x10u, result);
    }
    const u32 new_end = word(rt, vector + 4u);
    const u32 shifted = (new_end - 8u) - position;
    result = shifted;
    const s32 steps = static_cast<s32>(shifted) >> 2;
    if (steps > 0) {
        u32 source = new_end - 0xCu;
        u32 target = new_end - 8u;
        for (s32 left = steps; left > 0; --left) {
            const u32 moved = word(rt, source);
            if (moved != 0u) put(rt, moved + 0x10u, word(rt, moved + 0x10u) + 1u);
            const u32 overwritten = word(rt, target);
            put(rt, target, moved);
            if (overwritten != 0u) result = release_counted(rt, ctx, key_slot, overwritten);
            source -= 4u;
            target -= 4u;
        }
    }
    if (item != 0u) {
        result = word(rt, item + 0x10u) + 1u;
        put(rt, item + 0x10u, result);
    }
    const u32 previous = word(rt, position);
    put(rt, position, item);
    if (previous != 0u) result = release_counted(rt, ctx, key_slot, previous);
    if (item != 0u) result = release_counted(rt, ctx, key_slot, item);
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_304
} // namespace gt::game

#define NEW304(address, name, kind) GT_DECOMP(address, gt::game::new_batch_304::name, gt::ReturnKind::kind)
NEW304(0x08BB43C8u, func_08BB43C8, Word);

