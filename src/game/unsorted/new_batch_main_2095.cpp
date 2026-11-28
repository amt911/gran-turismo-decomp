// New batch, main session 2095: six functions redrafted from the asm (tools.disasm) after their earlier drafts mismatched
// the gate or were skipped twice. Structured C: named locals, G[n] only at the call boundary, v0 as the original leaves
// it on every exit. Draft only: the differential gate decides.
// 0x08A09AFC and 0x08A09824 are not here (sorted inserts through several callees whose semantics are not settled).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2095 {
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

// A guest call whose result is v0:v1; v1 is left in $v1 (ctx.gpr[3]) as the original leaves it.
u32 call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result.v0;
}

// A guest call with float arguments ($f12...) and a float result ($f0).
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

u32 unsigned_byte(Runtime &rt, u32 a) { return rt.memory().load8(a); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The original's ll/sc add on a reference count: returns the value before the add.
u32 atomic_add(Runtime &rt, u32 address, u32 delta) {
    const u32 before = word(rt, address);
    put(rt, address, before + delta);
    return before;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x088FED3C (161 insns): replaces the reference held at self+0x14 by the pair the callee 0x08963418 fills (16-byte
// output on the stack): the new block's count is raised, the old one's dropped (its two virtual releases run when it
// reaches zero), then the self+0x1C slot is refreshed the same way and self+0xB is set to 1.
// Frame 0x40. Stack: 0x00/0x04 = the callee's input pair, 0x08..0x14 = its output.

// ---------------------------------------------------------------------------------------------------------------------
// 0x089248A8 (162 insns): the object's constructor-like setup: writes the vtables of its sub-objects (each initialised by
// 0x089263EC first), runs the 0x08926398 resets of two 3-entry tables, and calls the 0x08926488 / 0x08926514 helpers.
// Finally it runs the vtable methods of the two tables with the object pointers and clears the 0x1D0 block.
// Returns self + 0x1D0 in v0. Frame 0x40. a2 and a3 (saved to the stack by the original) are read back by its
// stores.
void func_089248A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 arg_a2 = ctx.gpr[6];
    const u32 arg_a3 = ctx.gpr[7];
    call(rt, ctx, 0x08925FE4u, {self});
    put(rt, self, 0x08CC5FC0u);

    const u32 vt_b = 0x08CC61C0u;
    call(rt, ctx, 0x089263ECu, {self + 0x10u});
    put(rt, self + 0x10u, vt_b);
    call(rt, ctx, 0x089263ECu, {self + 0x3Cu});
    put(rt, self + 0x3Cu, vt_b);
    call(rt, ctx, 0x089263ECu, {self + 0x68u});
    put(rt, self + 0x68u, vt_b);
    call(rt, ctx, 0x089263ECu, {self + 0x94u});
    put(rt, self + 0x94u, vt_b);
    call(rt, ctx, 0x089263ECu, {self + 0xC0u});
    put(rt, self + 0xC0u, vt_b);
    call(rt, ctx, 0x089263ECu, {self + 0xECu});
    put(rt, self + 0xECu, vt_b);

    const u32 vt_c = 0x08CC6198u;
    call(rt, ctx, 0x089263ECu, {self + 0x118u});
    put(rt, self + 0x118u, vt_c);
    const u32 vt_d = 0x08CC6150u;
    call(rt, ctx, 0x089263ECu, {self + 0x144u});
    put(rt, self + 0x144u, vt_d);
    call(rt, ctx, 0x089263ECu, {self + 0x174u});
    put(rt, self + 0x174u, vt_c);
    call(rt, ctx, 0x089263ECu, {self + 0x1A0u});
    put(rt, self + 0x1A0u, vt_d);

    for (u32 i = 0u; i < 3u; ++i) call(rt, ctx, 0x08926398u, {self + 0x1E0u + 0x2Cu * i});
    for (u32 i = 0u; i < 3u; ++i) call(rt, ctx, 0x08926398u, {self + 0x264u + 0x2Cu * i});

    const u32 mt = 0x08CDDC60u;
    put(rt, self + 0x08u, arg_a2);
    put(rt, self + 0x0Cu, arg_a3);
    call(rt, ctx, 0x08926488u, {self + 0x10u, mt, 1u});
    call(rt, ctx, 0x08926488u, {self + 0x3Cu, mt, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xC0u, mt, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xECu, mt, 1u});

    if (arg_a3 == 2u) {
        call(rt, ctx, 0x08926514u, {self + 0x118u, 0x08CDDC18u, 1u});
        call(rt, ctx, 0x08926514u, {self + 0x174u, 0x08CDDC18u, 1u});
    } else {
        call(rt, ctx, 0x08926514u, {self + 0x118u, 0x08CDDC30u, 1u});
        call(rt, ctx, 0x08926514u, {self + 0x174u, 0x08CDDC30u, 1u});
    }

    for (u32 i = 0u; i < 3u; ++i) {
        const u32 first = self + 0x1E0u + 0x2Cu * i;
        const u32 second = self + 0x264u + 0x2Cu * i;
        call_indirect(rt, ctx, word(rt, word(rt, first) + 0x08u), {first, 0x08CDDC00u, 1u});
        call_indirect(rt, ctx, word(rt, word(rt, second) + 0x08u), {second, 0x08CDDBE8u, 1u});
    }

    const u32 block = self + 0x1D0u;
    put(rt, self + 0x1D0u, 0u);
    put(rt, block + 0x0Cu, 0u);
    put(rt, block + 0x04u, 0u);
    put(rt, block + 0x08u, 0u);
    ctx.gpr[2] = block;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08AFF7E0 (164 insns): transforms a 4x4 block of the source at a2 << 9 into the object's list. The four floats of
// a matrix from 0x08BAC230 are negated into a frame vector, four callees combine them, the 4D cross-product-like
// expression of two 4-float vectors is built and passed on, and the object's list at +0xD8 gets one entry per row
// (0x08C45D84 / 0x08C45F1C / 0x08C46A5C). A non-null a3 receives the last result's three words; v0 is the first of
// them when a3 is set, else the loop end.
// Frame 0xE0. Stack: 0x00 = 0x08BAC230's result, 0x04 = the list cursor (its address is passed), 0x08/0x0C = the entry,
// 0x10..0x14 = 0x08C45E94's input, 0x18..0x1C = its result, 0x20..0x2C = intermediate pairs, 0x30..0x3C = a vector.
void func_08AFF7E0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xE0u);
    const u32 self = ctx.gpr[4];
    u32 out_row = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 s1 = (a2 << 9) + 0x80u;

    const u32 p0 = call(rt, ctx, 0x08BAC230u, {a3});
    put(rt, frame.at(0x00u), p0);
    put_float(rt, frame.at(0x3Cu), -float_at(rt, p0));
    put_float(rt, frame.at(0x40u), -float_at(rt, p0 + 0x04u));
    put_float(rt, frame.at(0x44u), -float_at(rt, p0 + 0x08u));
    put_float(rt, frame.at(0x48u), float_at(rt, p0 + 0x0Cu));

    const u32 r0 = call_pair(rt, ctx, 0x08BAC270u, {self + 0x20u, t0});
    const u32 r1 = ctx.gpr[3];
    put(rt, frame.at(0x20u), r0);
    put(rt, frame.at(0x24u), r1);

    const u32 q0 = call_pair(rt, ctx, 0x08BAC25Cu, {frame.at(0x3Cu), frame.at(0x20u)});
    const u32 q1 = ctx.gpr[3];
    put(rt, frame.at(0x28u), q0);
    put(rt, frame.at(0x2Cu), q1);

    call(rt, ctx, 0x08BAC428u, {frame.at(0x4Cu), word(rt, q1), word(rt, q1 + 0x04u)});
    call(rt, ctx, 0x08BAC284u, {frame.at(0x30u), q0, frame.at(0x4Cu)});

    const u32 u0 = call_pair(rt, ctx, 0x08BAC248u, {frame.at(0x3Cu), self});
    const u32 u1 = ctx.gpr[3];
    put(rt, frame.at(0x10u), u0);
    put(rt, frame.at(0x14u), u1);

    const u32 rv0 = call_pair(rt, ctx, 0x08C45E94u, {frame.at(0x10u), frame.at(0x30u)});
    const u32 rv1 = ctx.gpr[3];

    // The two vectors at rv0 (the first word is Y, the second X): their products give the four floats of the row.
    const u32 X = word(rt, rv0 + 0x04u);
    const u32 Y = word(rt, rv0);
    const float x0 = float_at(rt, X + 0x00u);
    const float x4 = float_at(rt, X + 0x04u);
    const float x8 = float_at(rt, X + 0x08u);
    const float x12 = float_at(rt, X + 0x0Cu);
    const float y0 = float_at(rt, Y + 0x00u);
    const float y4 = float_at(rt, Y + 0x04u);
    const float y8 = float_at(rt, Y + 0x08u);
    const float y12 = float_at(rt, Y + 0x0Cu);

    const float y0x0 = psp_mul(y0, x0);
    const float y8x4 = psp_mul(y8, x4);
    const float y0x8 = psp_mul(y0, x8);
    const float y4x0 = psp_mul(y4, x0);
    float f3 = psp_mul(y12, x12);
    float f2 = psp_mul(y4, x8);
    float f1 = psp_mul(y8, x0);
    float f0 = psp_mul(y0, x4);
    f3 = f3 - y0x0;
    const float y0x12 = psp_mul(y0, x12);
    f2 = f2 - y8x4;
    f1 = f1 - y0x8;
    f0 = f0 - y4x0;
    const float y4x4 = psp_mul(y4, x4);
    const float y4x12 = psp_mul(y4, x12);
    const float y8x12 = psp_mul(y8, x12);
    const float y12x0 = psp_mul(y12, x0);
    const float y12x4 = psp_mul(y12, x4);
    f3 = f3 - y4x4;
    f2 = f2 + y0x12;
    f1 = f1 + y4x12;
    f0 = f0 + y8x12;
    const float y8x8 = psp_mul(y8, x8);
    const float y12x8 = psp_mul(y12, x8);
    f2 = f2 + y12x0;
    f1 = f1 + y12x4;
    f3 = f3 - y8x8;
    f0 = f0 + y12x8;

    put_float(rt, frame.at(0x4Cu), f2);
    put_float(rt, frame.at(0x50u), f1);
    put_float(rt, frame.at(0x54u), f0);
    put_float(rt, frame.at(0x58u), f3);
    call(rt, ctx, 0x08C46184u, {frame.at(0x5Cu), frame.at(0x4Cu), rv1, rv0});

    const u32 block = word(rt, self + 0xD8u) + s1;
    const u32 masked = word(rt, block + 0x1FCu) & 0xF0u;
    const u32 end = block + masked;
    put(rt, frame.at(0x04u), block);
    if (end != block) {
        u32 next = 0u;
        do {
            const u32 p = call(rt, ctx, 0x08C45D84u, {frame.at(0x04u)});
            const u32 e0 = call_pair(rt, ctx, 0x08C45F1Cu, {frame.at(0x5Cu), p});
            const u32 e1 = ctx.gpr[3];
            put(rt, frame.at(0x08u), e0);
            put(rt, frame.at(0x0Cu), e1);
            const u32 row_out = out_row;
            out_row += 0x10u;
            call(rt, ctx, 0x08C46A5Cu, {row_out, word(rt, frame.at(0x08u)), word(rt, frame.at(0x0Cu))});
            next = word(rt, frame.at(0x04u)) + 0x10u;
            put(rt, frame.at(0x04u), next);
        } while (next != end);
    }
    ctx.gpr[2] = masked != 0u ? end : 0u;
    if (t1 != 0u) {
        put(rt, t1 + 0x04u, word(rt, frame.at(0x34u)));
        put(rt, t1, word(rt, frame.at(0x30u)));
        put(rt, t1 + 0x08u, word(rt, frame.at(0x38u)));
        ctx.gpr[2] = word(rt, frame.at(0x30u));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08AB7988 (165 insns): appends or refreshes records of 0xA4 bytes: for each of the source's records (count at +0x1480),
// the object's records are scanned while 0x08B35FD8 says they match; the first one that does not match gets the
// record's tail (0x80..0xA4) copied over it, and when all match the record is appended (0x80 bytes by 0x08B7C830, then
// the tail) and the object's count rises. Records are 0xA4 bytes; counts at +0x1480; objects with 0x20 or more records
// are left alone. v0 is 0 when the source has records, else unchanged.
void copy_tail(Runtime &rt, u32 dst, u32 rec) {
    for (u32 k = 0u; k < 0x20u; k += 4u) put(rt, dst + 0x80u + k, word(rt, rec + 0x80u + k));
    put(rt, dst + 0xA0u, word(rt, rec + 0xA0u));
}

void func_08AB7988(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    if (static_cast<s32>(word(rt, src + 0x1480u)) <= 0) return;
    u32 i = 0u;
    do {
        const u32 rec = src + i * 0xA4u;
        const u32 n = word(rt, self + 0x1480u);
        if (static_cast<s32>(n) < 0x20) {
            u32 j = 0u;
            while (static_cast<s32>(j) < static_cast<s32>(n) &&
                   call(rt, ctx, 0x08B35FD8u, {self + j * 0xA4u, rec}) != 0u) {
                ++j;
            }
            if (static_cast<s32>(j) < static_cast<s32>(n)) {
                copy_tail(rt, self + j * 0xA4u, rec);
            } else {
                const u32 dst = self + n * 0xA4u;
                call(rt, ctx, 0x08B7C830u, {dst, rec, 0x80u});
                copy_tail(rt, dst, rec);
                put(rt, self + 0x1480u, n + 1u);
            }
        }
        ++i;
    } while (static_cast<s32>(i) < static_cast<s32>(word(rt, src + 0x1480u)));
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08915970 (178 insns): classifies the entry a1 (its bit in the table at 0x08CDD93C) for the stores at a3 (an int) and
// t0 (a float). Entries 1, 4, 6 and 7 tail-call the three helpers; the others pick a mode from a bit of the entry's
// byte +0xE9 and compute a distance-weighted float from the 0x08955300 result. The callees' v0 is the exit v0 when they
// run last.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08929104 (184 insns): the per-object reset of the sub-objects at +0x290..+0x1D30 and the tables after them; returns
// v0 = 3 (the original's last `addiu v0,zero,3`, not 2: the gate's v0 mismatch).
// Frame 0x40. Stack: 0 = a_290, 4 = a_4e0, 8 = a_900.

} // namespace
} // namespace new_batch_main_2095
} // namespace gt::game

#define NEW2095(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2095::name, gt::ReturnKind::kind)
NEW2095(0x089248A8u, func_089248A8, Word);
NEW2095(0x08AFF7E0u, func_08AFF7E0, Word);
NEW2095(0x08AB7988u, func_08AB7988, Word);
