// New batch main 998: functions the queue left for a second pass (skipped as too hard, or a draft that mismatched).
// Draft only: the differential gate decides which of them land. Written as structured C from `tools.disasm`, keeping the
// original's exit `v0` on every path, the call order, the delay slots' stores and every argument register a callee reads.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_998 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that also leaves `v1` / `a1` for the caller to read.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    return result;
}
// A call with float arguments ($f12-$f15), result in both `v0` and `$f0`.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// A callee-saved register a body keeps a value in, put back on every exit (the gate compares them).
struct Saved {
    AllegrexContext &ctx;
    unsigned reg;
    u32 value;
    Saved(AllegrexContext &c, unsigned r) : ctx(c), reg(r), value(c.gpr[r]) {}
    ~Saved() { ctx.gpr[reg] = value; }
};

// A truncating float -> word -> float round trip as `trunc.w.s` / `cvt.s.w` do it: out of range saturates and a NaN
// answers 0x7FFFFFFF.
s32 trunc_word(float value) {
    if (std::isnan(value) || value >= 2147483648.0f) return 0x7FFFFFFF;
    if (value < -2147483648.0f) return -2147483647 - 1;
    return static_cast<s32>(value);
}

// The PSP's `ll`/`sc` refcount decrement: one word, minus one, always stored.
u32 refcount_decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
void refcount_increment(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }

// A `lw v0, slot(vtable); jalr v0` virtual call (v0/v1/f0 are the callee's afterwards).
void virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
}

// Release of one intrusive reference: -1 at +4; on the last one the destructor at +8 runs, then -1 at +8; on that
// last one the deallocator at +0xC runs. The vtable word is re-read before each virtual call.
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (refcount_decrement(rt, object + 4u) != 1u) return;
    virtual_call(rt, ctx, object, 8u);
    if (refcount_decrement(rt, object + 8u) != 1u) return;
    virtual_call(rt, ctx, object, 0xCu);
}

// ---- 0x08863340: `j 0x0886309C` with `lw a0, 0(a0)` in its delay slot: a two-instruction forwarder. The rest of the
// bytes the recompiler lists under this address belong to the function at 0x08863348 and are never entered here. ----

// ---- 0x08889628: copy-assign a container of intrusive pointers (this = a0, source = a1) ----
// 0x08889244 prepares this from the source; the source's +0x10 element count picks the sub-container at this+0x40, whose
// element address is 12*count+0x10 past this. One element is appended or a range assigned, the temporary released, and
// every element copied: each new pointer is incremented (twice on the assignment path, as the original does), stored, and
// the old one released. v0 is the last call's result.

// ---- 0x08B30284: start a level/session from the descriptor at a1 (self = a0), holding the mutex at self+0x8FC. ----
// 0x08B301C4 validates the pending state, 0x08B30000 checks the requested size, 0x08B2F44C drops the previous one, then
// one of three table pointers goes with the descriptor's fields to 0x08C7AEA8. A negative answer ends it. The rest
// allocates a 1000-byte-per-entry block (self+0x12B0/0x12B4 hold the 64-bit end), creates and locks the second mutex,
// loads the table, and on success copies the flags into self and eventually calls 0x08B2F690. v0 = 1 on success.

// ---- 0x08C5355C: solve the 2x2 system (4 floats at a0, row-major) against the 2 floats at a2, writing the answer to
// the 2 words at a1. Partial pivoting: the larger |column entry| is swapped into place (the matrix, the pair on the stack
// and the right-hand side move together); a zero pivot gives v0 = 0, a solved system v0 = 1. ----
void func_08C5355C(Runtime &rt, AllegrexContext &ctx) {
    const u32 matrix = ctx.gpr[4];
    const u32 answer = ctx.gpr[5];
    const u32 rhs = ctx.gpr[6];
    /* The body below keeps the asm's s0-s7 names for its locals, so the register guards get their own names. */
    Saved keep0(ctx, 16), keep1(ctx, 17), keep2(ctx, 18), keep3(ctx, 19);
    Saved keep4(ctx, 20), keep5(ctx, 21), keep6(ctx, 22), keep7(ctx, 23), keep_fp(ctx, 30);
    GuestFrame frame(ctx, 0x70u);
    const u32 m = frame.at(8u);   // the four matrix floats (s4 never moves off this)
    const u32 v = frame.at(0u);   // the two right-hand-side floats
    put(rt, frame.at(0x24u), answer);
    put_float(rt, m + 0x0u, float_at(rt, matrix + 0x0u));
    put_float(rt, m + 0x4u, float_at(rt, matrix + 0x4u));
    put_float(rt, m + 0x8u, float_at(rt, matrix + 0x8u));
    put_float(rt, m + 0xCu, float_at(rt, matrix + 0xCu));
    put_float(rt, v + 0x0u, float_at(rt, rhs + 0x0u));
    put_float(rt, v + 0x4u, float_at(rt, rhs + 0x4u));
    u32 s2 = 0u;
    float pivot_abs = 0.0f;
    for (;;) {
        const u32 s3 = s2 + 1u;
        const u32 s5 = m + 12u * s2;
        const u32 s6 = v + 4u * s2;
        const u32 fpv = 4u + 4u * s2;
        put(rt, frame.at(0x20u), s2 << 3);
        const bool single = s3 < 2u;
        pivot_abs = std::fabs(float_at(rt, s5));
        if (single) {
            u32 a0 = m + (s3 << 2) + 8u;
            u32 a1 = frame.at(0u) + (s3 << 2);
            u32 a2 = fpv;
            const u32 t3 = m + (s3 << 2) + 4u;
            const u32 t1 = m + (s2 << 3);
            const u32 t0 = s2 << 2;
            u32 count = s3;
            do {
                const float candidate = std::fabs(float_at(rt, t1 + a2));
                const u32 a3 = m + t0;
                count += 1u;
                a2 += 4u;
                if (pivot_abs < candidate) {
                    const float old_pair = float_at(rt, a3);
                    const float new_pair = float_at(rt, a0 - 8u);
                    pivot_abs = candidate;
                    put_float(rt, a3, new_pair);
                    put_float(rt, a0 - 8u, old_pair);
                    const float old_high = float_at(rt, a0);
                    const float new_high = float_at(rt, t3);
                    put_float(rt, t3, old_high);
                    put_float(rt, a0, new_high);
                    const float old_rhs = float_at(rt, a1);
                    const float new_rhs = float_at(rt, s6);
                    put_float(rt, s6, old_rhs);
                    put_float(rt, a1, new_rhs);
                }
                a0 += 4u;
                a1 += 4u;
            } while (count != 2u);
        }
        if (pivot_abs == 0.0f) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (single) {
            const float pivot = float_at(rt, s5);
            const u32 row = s2 << 3;
            const u32 column = s2 << 2;
            u32 distance = fpv;
            for (u32 index = s3; index != 2u; ++index, distance += 4u) {
                const float factor = -float_at(rt, m + row + distance) / pivot;
                call_float(rt, ctx, 0x08B4D1BCu, {m, index, s2}, {factor});
                const float scale = float_at(rt, v + column);
                const u32 slot = v + (index << 2);
                put_float(rt, slot, float_at(rt, slot) + psp_mul(factor, scale));
            }
        }
        s2 = s3;
        if (s3 != 2u) continue;
        break;
    }
    {
        const u32 out = word(rt, frame.at(0x24u));
        u32 t1 = frame.at(0xCu);
        u32 t2 = frame.at(0x1Cu);
        for (u32 row = 1u, column = 2u;;) {
            const u32 offset = row << 2;
            float sum = float_at(rt, v + offset);
            const u32 slot = out + offset;
            put_float(rt, slot, sum);
            if (column < 2u) {
                u32 index = column;
                u32 source = (column << 2) + out;
                u32 factors = t2;
                do {
                    sum -= psp_mul(float_at(rt, source), float_at(rt, factors));
                    index += 1u;
                    source += 4u;
                    factors += 8u;
                    put_float(rt, slot, sum);
                } while (index != 2u);
            }
            const float solved = sum / float_at(rt, t1 + (row << 3));
            row -= 1u;
            column -= 1u;
            t1 -= 4u;
            t2 -= 0xCu;
            put_float(rt, slot, solved);
            if (row == 0xFFFFFFFFu) break;
        }
    }
    ctx.gpr[2] = 1u;
}

// ---- 0x088ACD24: for each of the eight 0x44-byte entries at self+0x60 whose word at +0x24 has bit 1 set, build two
// vectors (entry+0x14/+0x18 and a 68-byte-stride table entry), test the angle between them (outside [0,1] clears the
// entry's bit 1), and otherwise offset self+8 and entry+8 by a half-scale cross product, plus a second correction on
// self+0x3C / entry+0x3C when the cross of two stored vectors points back. v0 = 8 on exit. ----
void func_088ACD24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved s0(ctx, 16), s1(ctx, 17), s2(ctx, 18), s3(ctx, 19);
    Saved s4(ctx, 20), s5(ctx, 21), s6(ctx, 22), s7(ctx, 23), fp(ctx, 30);
    GuestFrame frame(ctx, 0x70u);
    const u32 pair = self + 0x3Cu;
    const u32 first = frame.at(0u);
    const u32 forward = frame.at(8u);
    const u32 side = frame.at(0x10u);
    const u32 stored = frame.at(0x18u);
    const u32 offset = frame.at(0x20u);
    u32 entry = self + 0x60u;
    for (u32 index = 0u; index != 8u; ++index) {
        if ((word(rt, entry + 0x24u) & 2u) == 0u) {
            entry += 0x44u;
            continue;
        }
        u32 row = word(rt, entry + 0x44u);
        const u32 table = word(rt, entry + 0x40u);
        put_float(rt, forward + 0u, float_at(rt, entry + 0x14u));
        put_float(rt, forward + 4u, float_at(rt, entry + 0x18u));
        const u32 target = table + 0x74u + 68u * row;
        call(rt, ctx, 0x08B4AC30u, {forward, forward, target});
        row = (row + 1u) & 7u;
        const u32 sample = table + 0x70u + 68u * row;
        put_float(rt, side + 0u, float_at(rt, sample + 4u));
        put_float(rt, side + 4u, float_at(rt, sample + 8u));
        call(rt, ctx, 0x08B4AC30u, {side, side, target});
        const float length = call_float(rt, ctx, 0x08B4ABCCu, {side, side}, {});
        const float inverse = 1.0f / length;
        const float cosine = psp_mul(inverse, call_float(rt, ctx, 0x08B4ABCCu, {forward, side}, {}));
        if (cosine < 0.0f || 1.0f < cosine) {
            put(rt, entry + 0x24u, word(rt, entry + 0x24u) & ~2u);
            entry += 0x44u;
            continue;
        }
        if (0.0f < call_float(rt, ctx, 0x08B4ABECu, {forward, side}, {})) {
            put(rt, entry + 0x24u, word(rt, entry + 0x24u) & ~2u);
        } else {
            put_float(rt, first + 0u, float_at(rt, side + 0u));
            put_float(rt, first + 4u, float_at(rt, side + 4u));
            call_float(rt, ctx, 0x08B4A560u, {first}, {cosine});
            put_float(rt, offset + 0u, float_at(rt, first + 0u));
            put_float(rt, offset + 4u, float_at(rt, first + 4u));
            call(rt, ctx, 0x08B4AC30u, {offset, offset, forward});
            call_float(rt, ctx, 0x08B4A560u, {offset}, {0.5f});
            call(rt, ctx, 0x08B4AC0Cu, {self + 8u, self + 8u, offset});
            call(rt, ctx, 0x08B4AC30u, {table + 8u, table + 8u, offset});
        }
        put_float(rt, stored + 0u, float_at(rt, pair + 0u));
        put_float(rt, stored + 4u, float_at(rt, pair + 4u));
        call(rt, ctx, 0x08B4AC30u, {stored, stored, table + 0x3Cu});
        if (call_float(rt, ctx, 0x08B4ABECu, {stored, side}, {}) < 0.0f) {
            const float scale = psp_mul(inverse, call_float(rt, ctx, 0x08B4ABCCu, {stored, side}, {}));
            call_float(rt, ctx, 0x08B4A560u, {side}, {scale});
            call(rt, ctx, 0x08B4AC30u, {side, side, stored});
            call_float(rt, ctx, 0x08B4A560u, {side}, {0.5f});
            call(rt, ctx, 0x08B4AC0Cu, {pair, pair, side});
            call(rt, ctx, 0x08B4AC30u, {table + 0x3Cu, table + 0x3Cu, side});
        }
        entry += 0x44u;
    }
    ctx.gpr[2] = 8u;
}

// ---- 0x088EFD54: per-index record check with a remainder test (a0 = self, a1 = mode, a3 = index) ----
// The index must be inside the byte table at self+0xD4 (count self+0xD8) with a non-zero entry. 0x088DF13C answers two
// records whose +0x14 / +0x18 are compared, 0x088DD0E4 the lookup; a non-zero answer returns it. The flag at +0x24 must
// be 1, self+4 non-null and the mode one of 3..5, and the byte at table+index must be -1. Then a remainder of the
// virtual float at object[self+8]'s slot 0x14 against -(10 * byte at +0x19 of what [self+0xC]'s slot 0x5C answers)
// decides between the two 0x088F0F24 calls (the second a tail call, or the only one when the record sort says so).

// ---- 0x0881E0AC: gather the entries of one string-keyed index into a temporary vector (a0 = the caller's holder,
// a3 = the key). A temporary at sp+8 (0x0881BD98) answers an object passed to 0x0881B068; the two containers at sp+0xC
// (0x08A98674) and sp+0x10 (0x08A98A70) hold the search and its results. The outer loop walks the vector at sp+0xC one
// element at a time; each element's virtual slot 0x50 answers a pair, 0x08930458 counts the matches and 0x0881A840
// filters them. A match with a non-zero low nibble at +2 is passed to 0x0884CD28 and appended to the vector at
// second+0x14 (or, before any match, to the same vector from the packed value at sp); the vector's elements are
// released one by one. The two temporaries are released, and the holder is assigned the result unless it already is it.

//// END FUNCTIONS
} // namespace
} // namespace new_batch_main_998
} // namespace gt::game

#define NEW998(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_998::name, gt::ReturnKind::kind)
NEW998(0x08C5355Cu, func_08C5355C, Word);
NEW998(0x088ACD24u, func_088ACD24, Word);
//// END REGISTRATIONS
