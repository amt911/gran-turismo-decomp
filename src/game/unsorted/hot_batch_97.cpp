// Hot batch 97: reached functions of the same two probes batches 93-96 worked from, the 121-170 pool (`tools.decomp_gate probe b93p`) and the 171-230 pool (`probe b95p`): 141 candidates were still without a `func_X` in src/ after batch 96 (found by searching every probed address in src/ for a definition or a registration, the five that batch 96 left unregistered included); 34 are written, 27 registered (one of them nested) and 7 not. (main includes batch 96, 6,467 functions.)
//
// How the candidates were chosen: `tools.disasm` of the 141, then the ones with the most regular shape and the most calls on the routes first (layouts and digit windows, per-frame updates, float smoothers, constructors and one-time initialisers, a state machine with a jump table). Left (read, not written): the allocator-heavy and copy-on-write string code (func_08AACAF8, func_08A535A4, func_08A6A40C, func_08A7F5FC, func_08C23158), the 64-bit and soft-float routines (func_08B71720, func_08B71394, func_08ABE53C, func_0892C4B8), the `alloca` + smart-pointer code (func_08ADE5A4, func_08888D58, func_089101F0) and the VFPU function func_089AD0E0; the rest of the pool is longer than the time allowed.
//
// Registered (27): see the HOT97 lines at the end. Written but not registered (7, each commented HOT97 line says why): func_08AF0B78 and func_08AEB4CC wait on a semaphore (`sceKernelWaitSema`, never recordable: 89 of 89 and 88 of 99 compared cases are "not comparable"); func_08B04D98 builds an object whose tail callee fills new vector elements from uninitialised stack (11 of 11 cases mismatch, FINDINGS); func_08860890, func_08B3177C, func_08C496A4 and func_0889AD18 were compared (99 + 3000, 94 + 3000, 17 + 204 and 85 + 506 cases, the dense ones are runs of the race route with `GT_DECOMP_DENSE`) with 0 mismatches and not one case changed memory: they write only in conditions the scripted routes never reach (func_08C496A4's ranges always have fewer than 17 elements, so its partition and heap-sort paths never run).
//
// Gates (fast gate, five routes uncapped, with the coverage pass): every registered function compared at least once with 0 mismatches and, except the pure float / read-only ones, at least one compared case that changed memory (func_08904E08 only in its dense run: 8 of 327 calls); `tools.decomp_gate sabotage` pins 27 of 27 (func_08B610C0, reached only inside func_08B5BB0C's cases, as a nested registration); the full gate numbers are in the PR. Two bugs found by the first gates (func_0880E400 stored 6 where the asm stores 5 in w[0xE8], func_08915714 returned 0 where the loop bound `addiu v0, zero, 4` leaves 4) and one by an independent review of the paths no run takes (func_089ABBB0's four early exits left a stale v0). Host: `sceRtcGetCurrentClockLocalTime` (0xE7C27D1B) is now a recordable import, so func_08812CEC (which calls the stub itself) is comparable.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value, so a callee that reads a register this function never passes sees what an earlier call_set left; `call_setf` also sets f12-f15. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20-$f27 are locals. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the constants a test leaves in v0; the dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that callees read are.
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm by hand and by independent read-only reviews). The weakest proofs: func_08B8F080 (the guarded statics: only the first call's initialisation ran), func_0895E3F4 (several states of the menu cursor), func_0897D118 (the held-button counters), func_08C4D4EC (only some of the twelve type ids ran), func_08904E08 and func_089491A0 (rare float arms), func_08AF1524 (the camera and overlay arms).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_97 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]).
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }
// The HI of the MIPS signed `div` (what `mfhi` reads): the remainder; a zero divisor leaves the dividend (the recompiler's rule), INT_MIN / -1 leaves 0.
inline std::uint32_t mips_hi(std::uint32_t dividend, std::uint32_t divisor) {
    if (divisor == 0u) return dividend;
    if (dividend == 0x80000000u && divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<std::uint32_t>(signed_of(dividend) % signed_of(divisor));
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
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

// The four floats of one row of a table of 16-byte rows (a glyph or digit table): rows = trunc(table[8]), the row is table + ((index - (rows - 1)) << 4) and the floats sit at row + base + 0 / 4 / 8 / 0xC (f12 / f13 / f14 / f15 in that order).
inline std::array<float, 4> table_row(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t table, std::uint32_t index, std::uint32_t base) {
    const std::uint32_t rows = ctx.fpu_float_to_word(float_at(rt, table + 8u), 1u);
    const std::uint32_t row = table + ((index - (rows - 1u)) << 4u) + base;
    return {float_at(rt, row), float_at(rt, row + 4u), float_at(rt, row + 8u), float_at(rt, row + 0xCu)};
}
// The static all-zero-or-default record a digit window uses for a leading zero (0x08CE29F0): its floats sit at +0 / 4 / 8 / 0xC.
inline std::array<float, 4> record_floats(psprecomp::Runtime &rt, std::uint32_t row) {
    return {float_at(rt, row), float_at(rt, row + 4u), float_at(rt, row + 8u), float_at(rt, row + 0xCu)};
}

// func_0880E400(self; f12 = x): positions the five cells of a gauge on the window w = self[0x3C] (nothing when null). func_0896A35C(w, 0, 0, sp, sp + 4) fills the pair (sp[0], sp[4]); the cell table sp[8..0x2F] (five pairs) starts as (0.5f, 1.0f), (0.0f, 1.0f), (0.0f, 0.0f), (1.0f, 0.0f), (1.0f, 1.0f). When func_089DDC94(w) is not 5 the window is prepared: w[0xE8] = 5, func_089DE054(w, 5), func_089DE1BC(w, 5), then for cells 0 and 1 func_089DAF74(w, k; the cell pair) and func_089DAD9C(w, k; cell * (sp[0], sp[4])).
// x is clamped to 0.0f .. 2.0f (the ones that are not below 2.0f give 180.0f, else x * 90.0f); r = that * 0.017453292f (0x3C8EFA35). x < 0.5f: n = 3 and cell 2 = (sp[0x10], 1.0f - 1.41421356f * func_08B49CC8(r)); x < 1.5f: n = 4 and cell 3 = (sp[0x1C], 0.5f * (1.0f - 1.41421356f * func_08B499B8(r))); x < 2.0f: n = 5 and cell 4 = (sp[0x20], 1.0f - 1.41421356f * func_08B49CC8(r)); the other x leave n = 5 (the constant is set in the delay slot of the third test). Then for k = 2..4 the cell (k < n ? k : 0) goes through func_089DAF74(w, k; the cell pair) and func_089DAD9C(w, k; cell * (sp[0], sp[4])); v0 = (5 <u n) = 0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (window == 0u) return;`; `if (x < 0.0f) {`; `else if (x < 2.0f) {`; `else {`; `if (x < 0.5f) {`; `else if (x < 1.5f) {`; `if (x < 2.0f) {`.
void func_0880E400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t window = word(rt, self + 0x3Cu);
    if (window == 0u) return;
    float x = ctx.fpr[12];
    const std::uint32_t cells = frame.at(8u);
    (void)call_set(rt, ctx, 0x0896A35Cu, {window, 0u, 0u, frame.at(0u), frame.at(4u)});
    put_float(rt, frame.at(8u), 0.5f);
    put_float(rt, frame.at(0x2Cu), 1.0f);
    put_float(rt, frame.at(0xCu), 1.0f);
    put(rt, frame.at(0x10u), 0u);
    put_float(rt, frame.at(0x14u), 1.0f);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put_float(rt, frame.at(0x20u), 1.0f);
    put(rt, frame.at(0x24u), 0u);
    put_float(rt, frame.at(0x28u), 1.0f);
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 5u) {
        put(rt, window + 0xE8u, 5u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, 5u});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 5u});
        for (std::uint32_t k = 0u; k != 2u; ++k) {
            const std::uint32_t cell = frame.at(8u + 8u * k);
            (void)call_setf(rt, ctx, 0x089DAF74u, {window, k}, {float_at(rt, cell), float_at(rt, cell + 4u)});
            const float a = psp_mul(float_at(rt, cell), float_at(rt, frame.at(0u)));
            const float b = psp_mul(float_at(rt, cell + 4u), float_at(rt, frame.at(4u)));
            (void)call_setf(rt, ctx, 0x089DAD9Cu, {window, k}, {a, b});
        }
    }
    float radians;
    if (x < 0.0f) {
        x = 0.0f;
        radians = psp_mul(x, 90.0f);
    } else if (x < 2.0f) {
        radians = psp_mul(x, 90.0f);
    } else {
        x = 2.0f;
        radians = std::bit_cast<float>(0x43340000u);
    }
    radians = psp_mul(radians, std::bit_cast<float>(0x3C8EFA35u));
    ctx.fpr[12] = radians;
    std::uint32_t n;
    if (x < 0.5f) {
        n = 3u;
        const float s = call_keep(rt, ctx, 0x08B49CC8u, {}, {radians}).f0;
        put_float(rt, frame.at(0x1Cu), 1.0f - psp_mul(s, std::bit_cast<float>(0x3FB504F3u)));
        put(rt, frame.at(0x18u), word(rt, frame.at(0x10u)));
    } else if (x < 1.5f) {
        n = 4u;
        const float s = call_keep(rt, ctx, 0x08B499B8u, {}, {radians}).f0;
        const float t = psp_mul(1.0f - psp_mul(s, std::bit_cast<float>(0x3FB504F3u)), 0.5f);
        put(rt, frame.at(0x24u), word(rt, frame.at(0x1Cu)));
        put_float(rt, frame.at(0x20u), t);
    } else {
        n = 5u;
        if (x < 2.0f) {
            const float s = call_keep(rt, ctx, 0x08B49CC8u, {}, {radians}).f0;
            put_float(rt, frame.at(0x2Cu), 1.0f - psp_mul(s, std::bit_cast<float>(0x3FB504F3u)));
            put(rt, frame.at(0x28u), word(rt, frame.at(0x20u)));
        }
    }
    for (std::uint32_t k = 2u; k != 5u; ++k) {
        const std::uint32_t cell = cells + 8u * (k < n ? k : 0u);
        (void)call_setf(rt, ctx, 0x089DAF74u, {window, k}, {float_at(rt, cell), float_at(rt, cell + 4u)});
        const float a = psp_mul(float_at(rt, cell), float_at(rt, frame.at(0u)));
        const float b = psp_mul(float_at(rt, cell + 4u), float_at(rt, frame.at(4u)));
        (void)call_setf(rt, ctx, 0x089DAD9Cu, {window, k}, {a, b});
    }
    ctx.gpr[2] = 0u;
}

// func_08812CEC(self): the clock window w = self[0x40] (nothing when null), digits from func_08C7B018(sp) (a record: the halfwords sp[6] and sp[8]). When func_089DDC94(w) is not 0xE the window is prepared: w[0xE8] = 6, func_089DE054(w, 0xE), func_089DE1BC(w, 0xE) and the seven glyph widths func_0880E024(self + 0x220, w, i, c; f12 = the running x, f13 = 0.0f) for (i, c) = (0, 0x41), (2, 0x4D), (4, 0x30), (6, 0x30), (8, 0x3A), (0xA, 0x30), (0xC, 0x30): x += width + 1.0f except the pair (2: + 3.0f) and (6, 8: no padding); the last result is not summed.
// Then a = sp[6] (a >= 12 gives row base 0x50 and a - 12, else 0x41), b = sp[8], table = self[0x220]: func_089DB078(w, 0; the row (base) floats at +0), then the digits of a (a / 10 at index 4, a % 10 at index 6) and of b (b / 10 at index 0xA, b % 10 at index 0xC) as rows of the same table (floats at +0x300), each call func_089DB078(w, index; four floats); v0 = the last call's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (window == 0u) return;`; `if (signed_of(a) < 0xC) {`; `else {`.
void func_08812CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t window = word(rt, self + 0x40u);
    if (window == 0u) return;
    const std::uint32_t list = self + 0x220u;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 0xEu) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, 0xEu});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 0xEu});
        const auto glyph = [&](float at, std::uint32_t slot, std::uint32_t code) { return call_setf(rt, ctx, 0x0880E024u, {list, window, slot, code}, {at, 0.0f}).f0; };
        float x = glyph(0.0f, 0u, 0x41u) + 1.0f;
        x = x + 0.0f;
        x = x + (glyph(x, 2u, 0x4Du) + std::bit_cast<float>(0x40400000u));
        x = x + (glyph(x, 4u, 0x30u) + 1.0f);
        x = x + glyph(x, 6u, 0x30u);
        x = x + glyph(x, 8u, 0x3Au);
        x = x + (glyph(x, 0xAu, 0x30u) + 1.0f);
        (void)glyph(x, 0xCu, 0x30u);
    }
    (void)call_set(rt, ctx, 0x08C7B018u, {frame.at(0u)});
    std::uint32_t a = half_u(rt, frame.at(6u));
    const std::uint32_t b = half_u(rt, frame.at(8u));
    std::uint32_t first;
    if (signed_of(a) < 0xC) {
        first = 0x42u - 1u;
    } else {
        a -= 0xCu;
        first = 0x51u - 1u;
    }
    {
        const auto f = table_row(rt, ctx, word(rt, list), first, 0u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0u}, {f[0], f[1], f[2], f[3]});
    }
    const std::uint32_t a_tens = a / 10u;
    {
        const auto f = table_row(rt, ctx, word(rt, list), a_tens, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 4u}, {f[0], f[1], f[2], f[3]});
    }
    {
        const auto f = table_row(rt, ctx, word(rt, list), a - 10u * a_tens, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 6u}, {f[0], f[1], f[2], f[3]});
    }
    const std::uint32_t b_tens = b / 10u;
    {
        const auto f = table_row(rt, ctx, word(rt, list), b_tens, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0xAu}, {f[0], f[1], f[2], f[3]});
    }
    {
        const auto f = table_row(rt, ctx, word(rt, list), b - 10u * b_tens, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0xCu}, {f[0], f[1], f[2], f[3]});
    }
}

// func_08812720(self, n): the three digits of n on the window w = self[0x28] (nothing when null). When func_089DDC94(w) is not 0xC the window is prepared: w[0xE8] = 6, func_089DE054(w, 0xC), func_089DE1BC(w, 0xC), func_089DCCD0(w, 0xC) and six glyph cells func_0880E024(self + 0x210, w, 2 * i, 0x38; f12 = 0.0f / 23.0f (0x41B80000) / 46.0f (0x42380000) for i = 0..5, f13 = 1.0f), each followed by func_089DB204(w, 2 * i + 1, self[0x23C] + (i < 3 ? 0xA0 : 0x80)).
// Then, with table = self[0x210] and d(x) = the four floats of the row x of the table (floats at +0x300, see table_row): func_089DB078(w, 0xA; d(n % 10)); with m = n / 10: func_089DB078(w, 8; d(m % 10)), or the static record at 0x08CE29F0 (floats at +0) when m is 0; with h = m / 10: the tail call func_089DB078(w, 6; d(h % 10)) (the record when h is 0). The divisions are signed (the asm multiplies by 0x66666667); v0 = the callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (window == 0u) return;`; `if (h != 0u) {`.
void func_08812720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], n = ctx.gpr[5];
    const std::uint32_t window = word(rt, self + 0x28u);
    if (window == 0u) return;
    const std::uint32_t list = self + 0x210u;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 0xCu) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, 0xCu});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 0xCu});
        (void)call_set(rt, ctx, 0x089DCCD0u, {window, 0xCu});
        const float xs[3] = {0.0f, std::bit_cast<float>(0x41B80000u), std::bit_cast<float>(0x42380000u)};
        for (std::uint32_t i = 0u; i != 6u; ++i) {
            (void)call_setf(rt, ctx, 0x0880E024u, {list, window, 2u * i, 0x38u}, {xs[i % 3u], 1.0f});
            (void)call_set(rt, ctx, 0x089DB204u, {window, 2u * i + 1u, word(rt, self + 0x23Cu) + (i < 3u ? 0xA0u : 0x80u)});
        }
    }
    const std::uint32_t ones = n - 10u * static_cast<std::uint32_t>(signed_of(n) / 10);
    const std::uint32_t m = static_cast<std::uint32_t>(signed_of(n) / 10);
    {
        const auto f = table_row(rt, ctx, word(rt, list), ones, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0xAu}, {f[0], f[1], f[2], f[3]});
    }
    std::array<float, 4> f;
    if (m != 0u) {
        const std::uint32_t digit = m - 10u * static_cast<std::uint32_t>(signed_of(m) / 10);
        f = table_row(rt, ctx, word(rt, list), digit, 0x300u);
    } else {
        f = record_floats(rt, 0x08CE29F0u);
    }
    (void)call_setf(rt, ctx, 0x089DB078u, {window, 8u}, {f[0], f[1], f[2], f[3]});
    const std::uint32_t h = static_cast<std::uint32_t>(signed_of(m) / 10);
    if (h != 0u) {
        const std::uint32_t digit = h - 10u * static_cast<std::uint32_t>(signed_of(h) / 10);
        f = table_row(rt, ctx, word(rt, list), digit, 0x300u);
    } else {
        f = record_floats(rt, 0x08CE29F0u);
    }
    ctx.fpr[12] = f[0];
    ctx.fpr[13] = f[1];
    ctx.fpr[14] = f[2];
    ctx.fpr[15] = f[3];
    tail_call(ctx, 0x089DB078u, {window, 6u});
}

// func_08811E30(self, n): the elapsed-time display of the milliseconds n (a window found through self[0x178]). n >= self[0x290]: bit 14 of self[0x178][0xE0] is cleared (v0 = that word). Otherwise s = n / 1000 (v0 = the high word of n * 0x10624DD3 when s equals self[0x294], and nothing more happens); self[0x294] = s; the record sp[4] is built with func_08A69B3C(sp, 0x08C98878), func_08969160(sp + 4), func_08A7BFD0(self[0x178], sp + 4, sp), w = sp[4] (nothing more than func_08A5B9EC(sp + 4) and func_08A69D68(sp) when it is 0; v0 = that call's).
// Otherwise, when func_089DDC94(w) is not 0xA: w[0xE8] = 6, func_089DE054(w, 0xA), func_089DE1BC(w, 0xA) and the glyph widths func_0880E024(self + 0x224, w, i, c; f12 = the running x, f13 = 0.0f) for (i, c) = (6, 0x30), (4, 0x30), (8, 0x3A), (2, 0x30), (0, 0x30), x += width. Then with table = self[0x224] the digits of s as rows of the table (floats at +0x300, table_row): func_089DB078(w, 0; s % 10), then s1 = s / 10 gives index 2 (s1 % 6), s2 = s1 / 6 gives index 4 (s2 % 10) and the last, index 6, is s2 / 10 % 10 (the record at 0x08CE29F0 when it is 0); finally self[0x178][0xE0] |= 0x4000 (v0 = that word).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (!(n < word(rt, self + 0x290u))) {`; `const std::uint32_t high = static_cast<std::uint32_t>((static_cast<std::uint64_t>(n) * 0x10624DD3ull) >> 32u);`; `if (seconds == word(rt, self + 0x294u)) return;`; `const std::uint32_t overlay = word(rt, self + 0x178u);`; `if (window == 0u) return;`; `const std::uint32_t list = self + 0x224u;`; `if (call_set(rt, ctx, 0x089DDC94u, {window}) != 0xAu) {`; `const auto glyph = [&](float at, std::uint32_t slot, std::uint32_t code) { return call_setf(rt, ctx, 0x0880E024u, {list, window...`; `if (last != 0u) f = table_row(rt, ctx, word(rt, list), last, 0x300u);`; `else f = record_floats(rt, 0x08CE29F0u);`.
void func_08811E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], n = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    if (!(n < word(rt, self + 0x290u))) {
        const std::uint32_t overlay = word(rt, self + 0x178u);
        const std::uint32_t flags = word(rt, overlay + 0xE0u) & ~0x4000u;
        put(rt, overlay + 0xE0u, flags);
        ctx.gpr[2] = flags;
        return;
    }
    const std::uint32_t high = static_cast<std::uint32_t>((static_cast<std::uint64_t>(n) * 0x10624DD3ull) >> 32u);
    const std::uint32_t seconds = high >> 6u;
    ctx.gpr[2] = high;
    if (seconds == word(rt, self + 0x294u)) return;
    const std::uint32_t overlay = word(rt, self + 0x178u);
    put(rt, self + 0x294u, seconds);
    const std::uint32_t record = frame.at(4u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08C98878u});
    (void)call_set(rt, ctx, 0x08969160u, {record});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {overlay, record, frame.at(0u)});
    const std::uint32_t window = word(rt, record);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {record});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    if (window == 0u) return;
    const std::uint32_t list = self + 0x224u;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 0xAu) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, 0xAu});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 0xAu});
        const auto glyph = [&](float at, std::uint32_t slot, std::uint32_t code) { return call_setf(rt, ctx, 0x0880E024u, {list, window, slot, code}, {at, 0.0f}).f0; };
        float x = glyph(0.0f, 6u, 0x30u) + 0.0f;
        x = x + glyph(x, 4u, 0x30u);
        x = x + glyph(x, 8u, 0x3Au);
        x = x + glyph(x, 2u, 0x30u);
        (void)glyph(x, 0u, 0x30u);
    }
    const std::uint32_t s1 = seconds / 10u;
    {
        const auto f = table_row(rt, ctx, word(rt, list), seconds - 10u * s1, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0u}, {f[0], f[1], f[2], f[3]});
    }
    const std::uint32_t s2 = s1 / 6u;
    {
        const auto f = table_row(rt, ctx, word(rt, list), s1 - 6u * s2, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 2u}, {f[0], f[1], f[2], f[3]});
    }
    const std::uint32_t s3 = s2 / 10u;
    {
        const auto f = table_row(rt, ctx, word(rt, list), s2 - 10u * s3, 0x300u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 4u}, {f[0], f[1], f[2], f[3]});
    }
    const std::uint32_t last = s3 - 10u * (s3 / 10u);
    std::array<float, 4> f;
    if (last != 0u) f = table_row(rt, ctx, word(rt, list), last, 0x300u);
    else f = record_floats(rt, 0x08CE29F0u);
    (void)call_setf(rt, ctx, 0x089DB078u, {window, 6u}, {f[0], f[1], f[2], f[3]});
    const std::uint32_t flags = word(rt, word(rt, self + 0x178u) + 0xE0u) | 0x4000u;
    put(rt, word(rt, self + 0x178u) + 0xE0u, flags);
    ctx.gpr[2] = flags;
}

// func_08C496A4(first, last, depth, comp): the introsort loop over a range of halfword indices [first, last) ordered by a float key. comp points at two words (w0, w1) that are copied to sp[0] / sp[4] (the callees get sp as their comparator); the key of the index k (a signed halfword) is the float at (*w1) + 240 * k + 4 * w0 + 0x98. A range of fewer than 17 elements ((last - first) >> 1, signed) returns at once with v0 = last - first. Otherwise, with s3 = depth and i = 0, every round: i == depth gives the heap fallback func_08C49524(first, last, last, sp) (v0 = its result); else s3 -= 1 and the middle element mid = first + ((n + (n >>> 31)) & ~1) (n the element count) and the last one are compared with the first on their keys, the median of the three is the pivot (a signed halfword), p = func_08C491E4(first, last, pivot, sp) partitions, func_08C496A4(p, last, s3, sp) sorts the upper part, last = p, i += 1 and the loop goes on while the lower part has 17 or more elements (v0 = 1 at the exit).
// Branches the compared cases do not take are read in the notes at the end of the comment block of this batch.
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08C496A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], depth = ctx.gpr[6], comp = ctx.gpr[7];
    std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t object = word(rt, comp + 4u);
    const std::uint32_t field = word(rt, comp);
    put(rt, frame.at(4u), object);
    put(rt, frame.at(0u), field);
    std::uint32_t count = static_cast<std::uint32_t>(signed_of(last - first) >> 1);
    ctx.gpr[2] = last - first;
    if (signed_of(count) < 0x11) return;
    const std::uint32_t key_offset = field << 2u;
    std::uint32_t remaining = depth;
    for (std::uint32_t round = 0u;; ++round) {
        if (depth == round) {
            (void)call_set(rt, ctx, 0x08C49524u, {first, last, last, frame.at(0u)});
            return;
        }
        const std::uint32_t mid = first + ((count + (count >> 31u)) & ~1u);
        const std::uint32_t base = word(rt, object);
        const auto key = [&](std::uint32_t at) {
            const std::uint32_t index = sext16(half_u(rt, at));
            return float_at(rt, base + index * 240u + key_offset + 0x98u);
        };
        const float f2 = key(first);
        const float f1 = key(mid);
        --remaining;
        const std::uint32_t tail = last - 2u;
        std::uint32_t pick;
        if (f2 < f1) {
            const float f0 = key(tail);
            if (f1 < f0) pick = mid;
            else if (f2 < f0) pick = tail;
            else pick = first;
        } else {
            const float f0 = key(tail);
            if (f2 < f0) pick = first;
            else if (f1 < f0) pick = tail;
            else pick = mid;
        }
        const std::uint32_t pivot = sext16(half_u(rt, pick));
        const std::uint32_t split = call_set(rt, ctx, 0x08C491E4u, {first, last, pivot, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08C496A4u, {split, last, remaining, frame.at(0u)});
        count = static_cast<std::uint32_t>(signed_of(split - first) >> 1);
        last = split;
        const bool small = signed_of(count) < 0x11;
        ctx.gpr[2] = small ? 1u : 0u;
        if (small) return;
    }
}

// func_08AEB4CC(): the per-frame step of the 0x38-byte records at G[0x9C] (G = 0x08D41AD0, count G[0xA0], signed). sp[0] = sp[4] = sp[8] = 0 first, then sp[4] = (G[0x498] != 0 && !(G[0x4C0] < G[0x4B8], signed)) and sp[8] = (G[0x468] != 0); d = (G[0x4CC]:G[0x4C8]) - (G[0x4D4]:G[0x4D0]), a 64-bit difference.
// For each record r (kind k = r[0x18]): r[0x30] = r[0x28] and r[0x34] = r[0x2C]; when the word sp[4 * k] is not 0: the signed byte r[5] above 0 only counts down by one; otherwise k == 1 rebuilds the flags byte r[4] (flags = (r[4] & ~2) sign-extended | 0x20, with 2 added when the signed byte at G[0x4B0] + 0x6C * r[0x1C] is not 0; when r[4] has bit 0, 0x10 is added if neither bit 1 nor bit 2 is set) and stores the 64-bit sum (r[0x2C]:r[0x28]) + d into r[0x28] / r[0x2C]; k == 2 sets r[4] = (r[4] & 1) | (func_08AEDC78(r[0x1C]) & ~1) and (r[0x28], r[0x2C]) = (v0, v1) of func_08AEDCF4(r[0x1C]); any k but 1 (also 2) then adds one to the word r[0xC], and so does the k == 1 path. After the loop, when G[0x498] != 0 and sp[4] != 0: G[0x4B8] = func_08AEC61C(). v0 = 0, or the callee's.
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08AEB4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t G = 0x08D41AD0u;
    GuestFrame frame(ctx, 0x440u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    std::uint32_t reached = 0u;
    if (word(rt, G + 0x498u) != 0u) reached = signed_of(word(rt, G + 0x4C0u)) < signed_of(word(rt, G + 0x4B8u)) ? 0u : 1u;
    put(rt, frame.at(8u), 0u);
    const std::uint32_t lo_b = word(rt, G + 0x4D0u), hi_b = word(rt, G + 0x4D4u);
    const std::uint32_t lo_a = word(rt, G + 0x4C8u);
    std::uint32_t hi_a = word(rt, G + 0x4CCu);
    const std::uint32_t count = word(rt, G + 0xA0u);
    put(rt, frame.at(4u), reached);
    const std::uint32_t flag = word(rt, G + 0x468u) != 0u ? 1u : 0u;
    const std::uint32_t borrow = lo_a < lo_b ? 1u : 0u;
    hi_a = hi_a - hi_b - borrow;
    const std::uint32_t lo_d = lo_a - lo_b;
    put(rt, frame.at(8u), flag);
    if (signed_of(count) > 0) {
        std::uint32_t offset = 0u;
        for (std::uint32_t i = 0u;; ++i, offset += 0x38u) {
            const std::uint32_t r = word(rt, G + 0x9Cu) + offset;
            const std::uint32_t t0 = word(rt, r + 0x28u), t1 = word(rt, r + 0x2Cu);
            const std::uint32_t kind = word(rt, r + 0x18u);
            put(rt, r + 0x30u, t0);
            put(rt, r + 0x34u, t1);
            if (word(rt, frame.at(kind << 2u)) != 0u) {
                const std::uint32_t timer = byte_u(rt, r + 5u);
                if (signed_of(sext8(timer)) > 0) {
                    put8(rt, r + 5u, timer - 1u);
                } else {
                    if (kind == 1u) {
                        const std::uint32_t bits = byte_u(rt, r + 4u);
                        std::uint32_t flags = sext8(bits & ~2u);
                        const std::uint32_t entry = word(rt, G + 0x4B0u) + word(rt, r + 0x1Cu) * 0x6Cu;
                        const std::uint32_t entry_byte = sext8(byte_u(rt, entry));
                        flags |= 0x20u;
                        if (entry_byte != 0u) flags |= 2u;
                        if ((bits & 1u) != 0u && (flags & 6u) == 0u) flags |= 0x10u;
                        const std::uint32_t sum_lo = t0 + lo_d;
                        const std::uint32_t sum_hi = t1 + hi_a + (sum_lo < lo_d ? 1u : 0u);
                        put8(rt, r + 4u, flags);
                        put(rt, r + 0x28u, sum_lo);
                        put(rt, r + 0x2Cu, sum_hi);
                    } else if (kind == 2u) {
                        const std::uint32_t low_bit = byte_u(rt, r + 4u);  // the delay slot of the first jal
                        const std::uint32_t v = call_set(rt, ctx, 0x08AEDC78u, {word(rt, r + 0x1Cu)});
                        put8(rt, r + 4u, (low_bit & 1u) | (v & ~1u));
                        const GuestResult pair = call_guest(rt, ctx, 0x08AEDCF4u, {word(rt, r + 0x1Cu)}, {});
                        take(ctx, pair);
                        put(rt, r + 0x28u, pair.v0);
                        put(rt, r + 0x2Cu, pair.v1);
                    }
                    put(rt, r + 0xCu, word(rt, r + 0xCu) + 1u);
                }
            }
            if (!(signed_of(i + 1u) < signed_of(word(rt, G + 0xA0u)))) break;
        }
    }
    ctx.gpr[2] = word(rt, G + 0x498u);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = word(rt, frame.at(4u));
    if (ctx.gpr[2] == 0u) return;
    put(rt, G + 0x4B8u, call_set(rt, ctx, 0x08AEC61Cu, {}));
}

// func_08B8F080(obj): the lazy initialisation of a chain of function-local statics, then a tail call. Nothing (v0 = 0) when obj is null. s3 = the virtual call obj->vtable[2](obj). Every static is guarded by a byte guard that func_08B6441C(guard) claims (it answers 0 when the static needs no work) and func_08B64494(guard) releases. G1 = 0x08D75148 (the outer guard): when its byte is 0 and func_08B6441C(G1) answers non-zero, B = 0x08D75158 is handled the same way and inside it C = 0x08D74FF0 (initialises the record 0x08D7524C = {0, 0x08D75258, 0x08CBF5C0}) and D = 0x08D75138 (the record 0x08D75240 = {0, 0, 0}); [0x08D75270] = 0x08D7524C always runs when B was claimed, and [0x08D75274] = 0x08D75240 just before func_08B64494(B); then E = 0x08D75150 (the record 0x08D75264 = {0, 0x08D75270, 0x08CBF5C0}); [0x08D75288] = 0x08D75264 and func_08B64494(G1) close the outer guard. Then F = 0x08D75140 (the record 0x08D7527C = {0, 0x08D75288, 0x08CBF5C8}). Tail call func_08B8E6C0(obj, s3, 0x08D7527C, 0).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (obj == 0u) {`; `const auto guarded = [&](std::uint32_t guard) { return byte_u(rt, guard) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u...`; `const auto release = [&](std::uint32_t guard) { (void)call_set(rt, ctx, 0x08B64494u, {guard}); };`; `const auto record = [&](std::uint32_t at, std::uint32_t next, std::uint32_t table) {`; `if (byte_u(rt, kG1) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kG1}) != 0u) {`; `if (guarded(kB)) {`; `if (guarded(kC)) {`; `if (guarded(kD)) {`; `if (guarded(kE)) {`; `if (guarded(kF)) {`.
void func_08B8F080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    if (obj == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t target = slot_of(rt, obj, 8u);
    const std::uint32_t s3 = call_set(rt, ctx, target, {obj});
    const auto guarded = [&](std::uint32_t guard) { return byte_u(rt, guard) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u; };
    const auto release = [&](std::uint32_t guard) { (void)call_set(rt, ctx, 0x08B64494u, {guard}); };
    const auto record = [&](std::uint32_t at, std::uint32_t next, std::uint32_t table) {
        put(rt, at + 8u, table);
        put(rt, at + 4u, next);
        put(rt, at, 0u);
    };
    constexpr std::uint32_t kG1 = 0x08D75148u, kB = 0x08D75158u, kC = 0x08D74FF0u, kD = 0x08D75138u, kE = 0x08D75150u, kF = 0x08D75140u;
    if (byte_u(rt, kG1) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kG1}) != 0u) {
        if (guarded(kB)) {
            if (guarded(kC)) {
                record(0x08D7524Cu, 0x08D75258u, 0x08CBF5C0u);
                release(kC);
            }
            put(rt, 0x08D75270u, 0x08D7524Cu);
            if (guarded(kD)) {
                put(rt, 0x08D75240u + 8u, 0u);
                put(rt, 0x08D75240u + 4u, 0u);
                put(rt, 0x08D75240u, 0u);
                release(kD);
            }
            put(rt, 0x08D75270u + 4u, 0x08D75240u);
            release(kB);
        }
        if (guarded(kE)) {
            record(0x08D75264u, 0x08D75270u, 0x08CBF5C0u);
            release(kE);
        }
        put(rt, 0x08D75288u, 0x08D75264u);
        release(kG1);
    }
    if (guarded(kF)) {
        record(0x08D7527Cu, 0x08D75288u, 0x08CBF5C8u);
        release(kF);
    }
    tail_call(ctx, 0x08B8E6C0u, {obj, s3, 0x08D7527Cu, 0u});
}

// func_08B3177C(self): a state check on a record (self[0xC] < 0 returns that value; the rest tail-calls). self[8] != 0: self[8] = 0 and func_08B3027C(self). Otherwise func_08B344E0(self + 0x8FC); when self[5] == 0 and self[0x12A8] < 0 (signed) and func_08B2E05C() gives a 64-bit time (lo = v0, hi = v1) after the stamp (self[0x12B4]:self[0x12B0]) (compared signed on the high word, unsigned on the low), func_08B3457C(self + 0x8FC), func_08B2F70C(self, 8) and the tail call func_08B3027C(self). Else func_08B3457C(self + 0x8FC) and, when the byte self[0x9648] is 0, the tail call func_08B31600(self); otherwise the time func_08B2E05C() minus the stamp (self[0x9644]:self[0x9640]) as an absolute 64-bit value: above 10,000,000 gives func_08B2F70C(self, 8) and the tail call func_08B3027C(self), else the tail call func_08B31600(self).
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08B3177C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t state = word(rt, self + 0xCu);
    ctx.gpr[2] = state;
    if (signed_of(state) < 0) return;
    const std::uint32_t link = self + 0x8FCu;
    const std::uint32_t pending = byte_u(rt, self + 8u);
    ctx.gpr[2] = pending;
    if (pending != 0u) {
        put8(rt, self + 8u, 0u);
        tail_call(ctx, 0x08B3027Cu, {self});
        return;
    }
    (void)call_set(rt, ctx, 0x08B344E0u, {link});
    bool expired = false;
    ctx.gpr[2] = byte_u(rt, self + 5u);
    if (ctx.gpr[2] == 0u) {
        const std::uint32_t mark = word(rt, self + 0x12A8u);
        ctx.gpr[2] = mark;
        if (signed_of(mark) < 0) {
            const GuestResult now = call_guest(rt, ctx, 0x08B2E05Cu, {}, {});
            take(ctx, now);
            const std::uint32_t stamp_hi = word(rt, self + 0x12B4u);
            if (signed_of(stamp_hi) < signed_of(now.v1)) expired = true;
            else if (stamp_hi == now.v1) expired = word(rt, self + 0x12B0u) < now.v0;
        }
    }
    if (expired) {
        (void)call_set(rt, ctx, 0x08B3457Cu, {link});
        (void)call_set(rt, ctx, 0x08B2F70Cu, {self, 8u});
        tail_call(ctx, 0x08B3027Cu, {self});
        return;
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {link});
    ctx.gpr[2] = self + 0x10000u;
    ctx.gpr[3] = byte_u(rt, self + 0x9648u);
    ctx.gpr[4] = self;
    if (ctx.gpr[3] == 0u) {
        tail_call(ctx, 0x08B31600u, {self});
        return;
    }
    const GuestResult now = call_guest(rt, ctx, 0x08B2E05Cu, {}, {});
    take(ctx, now);
    const std::uint32_t lo = now.v0 - word(rt, self + 0x9640u);
    const std::uint32_t borrow = now.v0 < lo ? 1u : 0u;
    std::uint32_t hi = now.v1 - word(rt, self + 0x9644u) - borrow;
    const std::uint32_t sign = static_cast<std::uint32_t>(signed_of(hi) >> 31);
    std::uint32_t abs_lo = (lo ^ sign);
    hi = hi ^ sign;
    const std::uint32_t t2 = abs_lo - sign;
    const std::uint32_t carry = abs_lo < t2 ? 1u : 0u;
    const std::uint32_t t3 = hi - sign - carry;
    if (signed_of(t3) > 0 || (t3 == 0u && 0x00989680u < t2)) {
        (void)call_set(rt, ctx, 0x08B2F70Cu, {self, 8u});
        tail_call(ctx, 0x08B3027Cu, {self});
        return;
    }
    tail_call(ctx, 0x08B31600u, {self});
}

// func_0890E41C(self, mode, size): sets up a pass of four draw-state objects (the ids 0x65 .. 0x68 of func_08B57AB8 / func_08B57820 / func_08B57968, with the matrices func_08B567F4 writes to sp + 0x50 and sp + 0x90). func_08AF83AC(), func_08B56A00(1), func_08B567F4(sp + 0x50), then func_08AF2924(mode): non-zero gives func_08AF4B4C(sp + 0x50; f12 = self[4], f13 = self[0xC]) and func_08B568DC(sp + 0x50); zero gives f20 = self[4] * func_08B84A2C(0.30...f (0x3E9C88BB), self[8]) and func_08AF4B4C(sp + 0x50; f12 = f20 * func_08B84A2C(sp[0x64], self[8]), f13 = self[0xC]) and func_08B568DC. Then func_08B56A00(0), func_08AF7DF4(self[0x20], self[0x24], self[0x28]), func_08AF7E0C(1.0f, 1.0f, 1.0f) and func_0890E3A4(0; self[0x30], self[0x34], self[0x38]). The records at sp, sp + 0x10 and sp + 0x20 come from func_0890E370(sp + 16 * k, src; f12 = 0.5f (or the entry 0.5f * size[k] for k = 0, 1 and size[2] for k = 2 when size is not null)) with src = self + 0x4C, self + 0x5C (s0 + 0x20, s0 = self + 0x3C) and self + 0x6C (k = 2 only when size is not null; else s3 stays s0 + 0x30 itself). Eleven func_08B57AB8 / func_08B57820 / func_08B57968 calls bind them to the ids 0x65, 0x66 and 0x67 (0x67 also gets the four floats sp[0x98], sp[0xA8], sp[0xB8], 0.0f), 180.0f is the f12 of every func_08B57968; func_08B5371C(1, 0, 1, 2), func_08B58874(0x65), func_08B54FB8(0x66 .. 0x68), the vector (self[0x3DC], self[0x3E0], self[0x3E4], 1.0f) goes to func_08B57780(0, sp + 0x40), func_08B58874(0x64) and the tail of the function is func_08B52DDC(self[0x10] three times, 1.0f); v0 = its result.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (size != 0u) half = psp_mul(float_at(rt, size), half);`; `if (size != 0u) half = psp_mul(float_at(rt, size + 4u), half);`; `if (size != 0u) s3 = call_setf(rt, ctx, 0x0890E370u, {frame.at(0x20u), s3}, {float_at(rt, size + 8u)}).v0;`.
void func_0890E41C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], mode = ctx.gpr[5], size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    const std::uint32_t matrix = frame.at(0x50u);
    (void)call_set(rt, ctx, 0x08AF83ACu, {});
    (void)call_set(rt, ctx, 0x08B56A00u, {1u});
    (void)call_set(rt, ctx, 0x08B567F4u, {matrix});
    if (call_set(rt, ctx, 0x08AF2924u, {mode}) != 0u) {
        (void)call_setf(rt, ctx, 0x08AF4B4Cu, {matrix}, {float_at(rt, self + 4u), float_at(rt, self + 0xCu)});
    } else {
        float f20 = float_at(rt, self + 4u);
        const float scale = call_setf(rt, ctx, 0x08B84A2Cu, {}, {std::bit_cast<float>(0x3E9C88BBu), float_at(rt, self + 8u)}).f0;
        f20 = psp_mul(f20, scale);
        const float factor = call_setf(rt, ctx, 0x08B84A2Cu, {}, {float_at(rt, frame.at(0x64u)), float_at(rt, self + 8u)}).f0;
        (void)call_setf(rt, ctx, 0x08AF4B4Cu, {matrix}, {psp_mul(f20, factor), float_at(rt, self + 0xCu)});
    }
    (void)call_set(rt, ctx, 0x08B568DCu, {matrix});
    (void)call_set(rt, ctx, 0x08B56A00u, {0u});
    (void)call_setf(rt, ctx, 0x08AF7DF4u, {}, {float_at(rt, self + 0x20u), float_at(rt, self + 0x20u + 4u), float_at(rt, self + 0x20u + 8u)});
    const std::uint32_t s0 = self + 0x3Cu;
    (void)call_setf(rt, ctx, 0x08AF7E0Cu, {}, {1.0f, 1.0f, 1.0f});
    (void)call_setf(rt, ctx, 0x0890E3A4u, {0u}, {float_at(rt, self + 0x30u), float_at(rt, self + 0x34u), float_at(rt, self + 0x38u)});
    float half = 0.5f;
    if (size != 0u) half = psp_mul(float_at(rt, size), half);
    const std::uint32_t id_65_0 = call_setf(rt, ctx, 0x0890E370u, {frame.at(0u), self + 0x4Cu}, {half}).v0;
    half = 0.5f;
    if (size != 0u) half = psp_mul(float_at(rt, size + 4u), half);
    const std::uint32_t id_65_1 = call_setf(rt, ctx, 0x0890E370u, {frame.at(0x10u), s0 + 0x20u}, {half}).v0;
    std::uint32_t s3 = s0 + 0x30u;
    if (size != 0u) s3 = call_setf(rt, ctx, 0x0890E370u, {frame.at(0x20u), s3}, {float_at(rt, size + 8u)}).v0;
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x65u, 0u, id_65_0});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x65u, 1u, id_65_1});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x65u, 4u, s3});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x65u, 0xDu, s0});
    const float turn = std::bit_cast<float>(0x43340000u);
    (void)call_setf(rt, ctx, 0x08B57968u, {0x65u, 7u}, {turn});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x66u, 0u, id_65_0});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x66u, 1u, id_65_1});
    (void)call_set(rt, ctx, 0x08B57820u, {0x66u, 4u, 0u});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x66u, 0xDu, s0});
    (void)call_setf(rt, ctx, 0x08B57968u, {0x66u, 7u}, {turn});
    (void)call_set(rt, ctx, 0x08B56A00u, {0u});
    (void)call_set(rt, ctx, 0x08B567F4u, {frame.at(0x90u)});
    put(rt, frame.at(0x30u), word(rt, frame.at(0x98u)));
    put(rt, frame.at(0x38u), word(rt, frame.at(0xB8u)));
    put(rt, frame.at(0x34u), word(rt, frame.at(0xA8u)));
    put(rt, frame.at(0x3Cu), 0u);
    (void)call_set(rt, ctx, 0x08B57820u, {0x67u, 0u, 0u});
    (void)call_set(rt, ctx, 0x08B57820u, {0x67u, 1u, 0u});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x67u, 4u, s3});
    (void)call_set(rt, ctx, 0x08B57AB8u, {0x67u, 0xDu, frame.at(0x30u)});
    (void)call_setf(rt, ctx, 0x08B57968u, {0x67u, 7u}, {turn});
    (void)call_set(rt, ctx, 0x08B5371Cu, {1u, 0u, 1u, 2u});
    (void)call_set(rt, ctx, 0x08B58874u, {0x65u});
    (void)call_set(rt, ctx, 0x08B54FB8u, {0x66u});
    (void)call_set(rt, ctx, 0x08B54FB8u, {0x67u});
    (void)call_set(rt, ctx, 0x08B54FB8u, {0x68u});
    put(rt, frame.at(0x40u), word(rt, self + 0x3DCu));
    put(rt, frame.at(0x44u), word(rt, self + 0x3DCu + 4u));
    put(rt, frame.at(0x48u), word(rt, self + 0x3DCu + 8u));
    put_float(rt, frame.at(0x4Cu), 1.0f);
    (void)call_set(rt, ctx, 0x08B57780u, {0u, frame.at(0x40u)});
    (void)call_set(rt, ctx, 0x08B58874u, {0x64u});
    const float size_value = float_at(rt, self + 0x10u);
    (void)call_setf(rt, ctx, 0x08B52DDCu, {}, {size_value, size_value, size_value, 1.0f});
}

// func_089FB990(self): grows a size from the chain of children starting at self[0xF8] (nothing when func_08968DE4(self), called after func_08A00AB0(), answers 0). x = y = 0.0f; for each child c (c = func_08965BCC(c) until it answers 0) with func_08A00448(self, c) non-zero: func_0896B7AC(c, sp + 0x10, sp + 0x14) and func_08966B20(c, sp, sp + 4, sp + 8, sp + 0xC) fill two floats and four words; w = sp[0x10] + (float)(sp[0] + sp[4]) and h = sp[0x14] + (float)(sp[8] + sp[0xC]); by kind = func_089670A0(c): 1 and 2 add w to x and raise y to h when y < h; 0 and 3 raise x to w when x < w and add h to y; any other kind leaves both; then func_08966BF0(c; f12 = x, f13 = y). After the chain, when 0 < x or 0 < y, func_08A0D048(self, sp, sp + 4, sp + 8, sp + 0xC) adds (float)(sp[0] + sp[4]) to x and (float)(sp[8] + sp[0xC]) to y; finally func_08966CF0(self; f12 = x, f13 = y).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (child != 0u) {`; `if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {`; `if (kind == 1u || kind == 2u) {`; `if (f20 < h) f20 = h;`; `else if (kind == 0u || kind == 3u) {`; `if (0.0f < f21 || 0.0f < f20) {`.
void func_089FB990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    (void)call_set(rt, ctx, 0x08A00AB0u, {});
    if (call_set(rt, ctx, 0x08968DE4u, {self}) == 0u) return;
    float f20 = 0.0f, f21 = 0.0f;
    const auto whole = [&](std::uint32_t at) { return static_cast<float>(signed_of(word(rt, at) + word(rt, at + 4u))); };
    std::uint32_t child = word(rt, self + 0xF8u);
    if (child != 0u) {
        for (;;) {
            if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
                (void)call_set(rt, ctx, 0x0896B7ACu, {child, frame.at(0x10u), frame.at(0x14u)});
                (void)call_set(rt, ctx, 0x08966B20u, {child, frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)});
                const float f1 = float_at(rt, frame.at(0x14u));
                const float f3 = whole(frame.at(0u));
                const float f2 = whole(frame.at(8u));
                const float f0 = float_at(rt, frame.at(0x10u));
                const float h = f2 + f1;
                const float w = f3 + f0;
                put_float(rt, frame.at(0x14u), h);
                put_float(rt, frame.at(0x10u), w);
                const std::uint32_t kind = call_set(rt, ctx, 0x089670A0u, {child});
                if (kind == 1u || kind == 2u) {
                    f21 = f21 + w;
                    if (f20 < h) f20 = h;
                } else if (kind == 0u || kind == 3u) {
                    if (f21 < w) f21 = w;
                    f20 = f20 + h;
                }
                (void)call_setf(rt, ctx, 0x08966BF0u, {child}, {f21, f20});
            }
            child = call_set(rt, ctx, 0x08965BCCu, {child});
            if (child == 0u) break;
        }
        if (0.0f < f21 || 0.0f < f20) {
            (void)call_set(rt, ctx, 0x08A0D048u, {self, frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)});
            f21 = f21 + whole(frame.at(0u));
            f20 = f20 + whole(frame.at(8u));
        }
    }
    (void)call_setf(rt, ctx, 0x08966CF0u, {self}, {f21, f20});
}

// func_08974A14(self): nothing (v0 = 0) when the object o = self[0xB4] is null or func_089DA5C0(self, sp) answers 0. Otherwise the pair a = *func_089749A4(self) (two floats) scaled by 1.0f - sp[0] (func_08B4A560(sp + 0x2C; f12) on sp[0x2C] / sp[0x30]) and b = *func_08974718(self) scaled by sp[0] (func_08B4A560(sp + 0x24; f12)) are added into sp[0x34] / sp[0x38] with func_08B4AC0C(sp + 0x34, sp + 0x34, sp + 0x24). By self[0x7C] (v0 = 2 when it is none of 0, 1, 2): 0 calls the virtual slot 0x3C of o's table as slot(o, func_08A0F8C8(self), sp + 0x34); 1 gets the pair p = slot 0x38(sp + 0x14, o, func_08A0F8C8(self)), adds the sum with func_08B4AC0C(sp + 0x1C, sp + 0x1C, sp + 0x34) (p copied to sp[0x1C] / sp[0x20] first) and calls slot 0x3C(o, func_08A0F8C8(self), sp + 0x1C); 2 does the same with the buffers sp + 4 / sp + 0xC and func_08B4AC54 (a difference) instead of func_08B4AC0C.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (object == 0u) return;`; `if (call_set(rt, ctx, 0x089DA5C0u, {self, frame.at(0u)}) == 0u) return;`; `if (kind == 1u || kind == 2u) {`; `const std::uint32_t buffer = frame.at(plus ? 0x1Cu : 0xCu);`; `const std::uint32_t scratch = frame.at(plus ? 0x14u : 4u);`; `(void)call_set(rt, ctx, plus ? 0x08B4AC0Cu : 0x08B4AC54u, {buffer, buffer, sum});`; `if (kind != 0u) return;`.
void func_08974A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t object = word(rt, self + 0xB4u);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    if (call_set(rt, ctx, 0x089DA5C0u, {self, frame.at(0u)}) == 0u) return;
    const std::uint32_t sum = frame.at(0x34u);
    const std::uint32_t first = call_set(rt, ctx, 0x089749A4u, {self});
    put(rt, frame.at(0x2Cu), word(rt, first));
    const float rest = 1.0f - float_at(rt, frame.at(0u));
    put(rt, frame.at(0x30u), word(rt, first + 4u));
    (void)call_setf(rt, ctx, 0x08B4A560u, {frame.at(0x2Cu)}, {rest});
    const std::uint32_t second = call_set(rt, ctx, 0x08974718u, {self});
    put(rt, frame.at(0x24u), word(rt, second));
    const float share = float_at(rt, frame.at(0u));
    put(rt, frame.at(0x28u), word(rt, second + 4u));
    (void)call_setf(rt, ctx, 0x08B4A560u, {frame.at(0x24u)}, {share});
    put(rt, frame.at(0x34u), word(rt, frame.at(0x2Cu)));
    put(rt, frame.at(0x38u), word(rt, frame.at(0x30u)));
    (void)call_set(rt, ctx, 0x08B4AC0Cu, {sum, sum, frame.at(0x24u)});
    const std::uint32_t kind = word(rt, self + 0x7Cu);
    ctx.gpr[2] = 2u;
    if (kind == 1u || kind == 2u) {
        const bool plus = kind == 1u;
        const std::uint32_t buffer = frame.at(plus ? 0x1Cu : 0xCu);
        const std::uint32_t scratch = frame.at(plus ? 0x14u : 4u);
        const std::uint32_t current = word(rt, self + 0xB4u);
        const std::uint32_t setter = word(rt, word(rt, current) + 0x3Cu);
        const std::uint32_t slot = call_set(rt, ctx, 0x08A0F8C8u, {self});
        const std::uint32_t o = word(rt, self + 0xB4u);
        const std::uint32_t getter = word(rt, word(rt, o) + 0x38u);
        const std::uint32_t id = call_set(rt, ctx, 0x08A0F8C8u, {self});
        (void)call_set(rt, ctx, getter, {scratch, o, id});
        put(rt, buffer, word(rt, scratch));
        put(rt, buffer + 4u, word(rt, scratch + 4u));
        (void)call_set(rt, ctx, plus ? 0x08B4AC0Cu : 0x08B4AC54u, {buffer, buffer, sum});
        (void)call_set(rt, ctx, setter, {o, slot, buffer});
        return;
    }
    if (kind != 0u) return;
    const std::uint32_t o = word(rt, self + 0xB4u);
    const std::uint32_t setter = word(rt, word(rt, o) + 0x3Cu);
    const std::uint32_t slot = call_set(rt, ctx, 0x08A0F8C8u, {self});
    (void)call_set(rt, ctx, setter, {o, slot, sum});
}

// func_08969D2C(self, a, b, c, d): applies up to four optional float values to the state of self, by mode = func_08965BD4(self): 1 (the others return v0 = 1 and do nothing for any other mode): the object o = func_08965CE0(self) (nothing, v0 = 0, when null), func_08969B98(o, sp, sp + 4, sp + 0xC, sp + 8) gives (sp[0], sp[4], sp[0xC], sp[8]) and each present pointer p is turned into a fraction: a: (a[0] - sp[0]) / sp[0xC] (0.0f when sp[0xC] is not above 0.0f) for func_08965BE8(self; f12); b: (b[0] - sp[4]) / sp[8] for func_08965C14; c: c[0] / sp[0xC] (1.0f when not above 0.0f) for func_08965C40; d: d[0] / sp[8] (1.0f) for func_08965C90. Mode 2: sp[8] = sp[0xC] = 0 first and func_08969B98(o, sp + 8, sp + 0xC, 0, 0) when o is not null, then a[0] - sp[8] for func_08965BE8, b[0] - sp[0xC] for func_08965C14, c[0] for func_08965C40 and d[0] for func_08965C90 (the plain values). Mode 3: the plain values a[0], b[0], c[0], d[0] to the same four callees.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (o != 0u) (void)call_set(rt, ctx, 0x08969B98u, {o, frame.at(8u), frame.at(0xCu), 0u, 0u});`; `if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {float_at(rt, a) - float_at(rt, frame.at(8u))});`; `if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {float_at(rt, b) - float_at(rt, frame.at(0xCu))});`; `if (c != 0u) (void)call_setf(rt, ctx, 0x08965C40u, {self}, {float_at(rt, c)});`; `if (d != 0u) (void)call_setf(rt, ctx, 0x08965C90u, {self}, {float_at(rt, d)});`; `if (mode == 3u) {`; `if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {float_at(rt, a)});`; `if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {float_at(rt, b)});`; `if (mode != 1u) return;`; `if (o == 0u) return;`; `if (0.0f < span) f12 = minus ? (float_at(rt, p) - low) / span : float_at(rt, p) / span;`; `if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {fraction(a, float_at(rt, frame.at(0u)), float_at(rt, frame.at(0xCu)...`; `if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {fraction(b, float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u)),...`.
void func_08969D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7], d = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t mode = call_set(rt, ctx, 0x08965BD4u, {self});
    if (mode == 2u) {
        put(rt, frame.at(8u), 0u);
        put(rt, frame.at(0xCu), 0u);
        const std::uint32_t o = call_set(rt, ctx, 0x08965CE0u, {self});
        if (o != 0u) (void)call_set(rt, ctx, 0x08969B98u, {o, frame.at(8u), frame.at(0xCu), 0u, 0u});
        if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {float_at(rt, a) - float_at(rt, frame.at(8u))});
        if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {float_at(rt, b) - float_at(rt, frame.at(0xCu))});
        if (c != 0u) (void)call_setf(rt, ctx, 0x08965C40u, {self}, {float_at(rt, c)});
        if (d != 0u) (void)call_setf(rt, ctx, 0x08965C90u, {self}, {float_at(rt, d)});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode == 3u) {
        if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {float_at(rt, a)});
        if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {float_at(rt, b)});
        if (c != 0u) (void)call_setf(rt, ctx, 0x08965C40u, {self}, {float_at(rt, c)});
        if (d != 0u) (void)call_setf(rt, ctx, 0x08965C90u, {self}, {float_at(rt, d)});
        return;
    }
    if (mode != 1u) return;
    const std::uint32_t o = call_set(rt, ctx, 0x08965CE0u, {self});
    if (o == 0u) return;
    (void)call_set(rt, ctx, 0x08969B98u, {o, frame.at(0u), frame.at(4u), frame.at(0xCu), frame.at(8u)});
    const auto fraction = [&](std::uint32_t p, float low, float span, float otherwise, bool minus) {
        float f12 = otherwise;
        if (0.0f < span) f12 = minus ? (float_at(rt, p) - low) / span : float_at(rt, p) / span;
        return f12;
    };
    if (a != 0u) (void)call_setf(rt, ctx, 0x08965BE8u, {self}, {fraction(a, float_at(rt, frame.at(0u)), float_at(rt, frame.at(0xCu)), 0.0f, true)});
    if (b != 0u) (void)call_setf(rt, ctx, 0x08965C14u, {self}, {fraction(b, float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u)), 0.0f, true)});
    if (c != 0u) (void)call_setf(rt, ctx, 0x08965C40u, {self}, {fraction(c, 0.0f, float_at(rt, frame.at(0xCu)), 1.0f, false)});
    if (d != 0u) (void)call_setf(rt, ctx, 0x08965C90u, {self}, {fraction(d, 0.0f, float_at(rt, frame.at(8u)), 1.0f, false)});
}

// func_08860890(self): walks the four slots k = 0..3 of the records at self + 0x4120 (0x50 bytes each, r = the record), nothing (v0 = 0) when self[0x3F50] is null; v0 = 4 at the end. A slot with the byte r[0x18] != 0: a = func_088F8A14(self[0x5B4], k) and, unless a is -1 or equals r[0x24], func_08860664(self, k). A slot with r[0x18] == 0: the entry e = self[0x3F50][0] + 392 * (signed halfword r[0x1E]); when the byte r[0x1A] is not 0, two rounds j = 0, 1 over the words e[0x44 + 4 * j]: a word that is -1 or k, or func_088DC90C(self[0x5AC]) not 3, is skipped; otherwise func_08860740(self, k, 0) when r[0x24] == j, func_0885D9C8(e, j), func_088A79AC(sp, 0), func_08897B8C(sp + 0x40) and (sp + 0x70), func_08866510(r[0x44], r[0x48], sp + 8, sp, sp + 0x40), the byte sp[8 + j + 0x20] = 0xFF, func_088661B8(r[0x44], r[0x48], sp + 8, sp, signed byte sp[0x12], sp + 0x40), func_088978B4(sp + 0x70) and (sp + 0x40), func_088A7548(sp). Then the halfword h = self[0x3E84 + 2 * k] != -1 gives func_088D69D0(self[0x5AC], k, h, 1) and h = 0xFFFF; and the byte self[0x3E8C + k] != 0 gives func_088DA550(self[0x5AC], k, (e + 4 * r[0x24])[0x44], r[0x24]) and the byte = 0 (only for r[0x18] == 0 slots).
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08860890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0xD0u);
    const std::uint32_t table = word(rt, self + 0x3F50u);
    ctx.gpr[2] = table;
    if (table == 0u) return;
    for (std::uint32_t k = 0u; k != 4u; ++k) {
        const std::uint32_t r = self + 0x4120u + 0x50u * k;
        if (byte_u(rt, r + 0x18u) != 0u) {
            const std::uint32_t a = call_set(rt, ctx, 0x088F8A14u, {word(rt, self + 0x5B4u), k});
            if (a != 0xFFFFFFFFu && a != word(rt, r + 0x24u)) (void)call_set(rt, ctx, 0x08860664u, {self, k, a});
            continue;
        }
        const std::uint32_t e = word(rt, word(rt, self + 0x3F50u)) + sext16(half_u(rt, r + 0x1Eu)) * 392u;
        if (byte_u(rt, r + 0x1Au) != 0u) {
            for (std::uint32_t j = 0u; j != 2u; ++j) {
                const std::uint32_t cell = word(rt, e + 0x44u + 4u * j);
                if (cell == 0xFFFFFFFFu || cell == k) continue;
                if (call_set(rt, ctx, 0x088DC90Cu, {word(rt, self + 0x5ACu), cell}) != 3u) continue;
                if (word(rt, r + 0x24u) == j) (void)call_set(rt, ctx, 0x08860740u, {self, k, 0u});
                (void)call_set(rt, ctx, 0x0885D9C8u, {e, j});
                (void)call_set(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
                (void)call_set(rt, ctx, 0x08897B8Cu, {frame.at(0x40u)});
                (void)call_set(rt, ctx, 0x08897B8Cu, {frame.at(0xA0u - 0x30u)});
                (void)call_set(rt, ctx, 0x08866510u, {word(rt, r + 0x44u), word(rt, r + 0x48u), frame.at(8u), frame.at(0u), frame.at(0x40u)});
                const std::uint32_t t0 = sext8(byte_u(rt, frame.at(0x12u)));
                put8(rt, frame.at(8u) + j + 0x20u, 0xFFFFFFFFu);
                (void)call_set(rt, ctx, 0x088661B8u, {word(rt, r + 0x44u), word(rt, r + 0x48u), frame.at(8u), frame.at(0u), t0, frame.at(0x40u)});
                (void)call_set(rt, ctx, 0x088978B4u, {frame.at(0xA0u - 0x30u)});
                (void)call_set(rt, ctx, 0x088978B4u, {frame.at(0x40u)});
                (void)call_set(rt, ctx, 0x088A7548u, {frame.at(0u)});
            }
        }
        const std::uint32_t h = sext16(half_u(rt, self + 0x3E84u + 2u * k));
        if (h != 0xFFFFFFFFu) {
            (void)call_set(rt, ctx, 0x088D69D0u, {word(rt, self + 0x5ACu), k, h, 1u});
            put16(rt, self + 0x3E84u + 2u * k, 0xFFFFFFFFu);
        }
        if (byte_u(rt, self + 0x3E8Cu + k) != 0u) {
            const std::uint32_t index = word(rt, r + 0x24u);
            (void)call_set(rt, ctx, 0x088DA550u, {word(rt, self + 0x5ACu), k, word(rt, e + 4u * index + 0x44u), index});
            put8(rt, self + 0x3E8Cu + k, 0u);
        }
    }
    ctx.gpr[2] = 4u;
}

// func_08AF0B78(a, buffer, wanted, check): copies up to `wanted` units out of a ring buffer. h = func_08AEFD90(a, 0x08C926C4) and func_08AEFCF0(h) (v0 = -1 when either answers 0); the ring record r = h's result: r[0x11] != 0 and r[0x20] < wanted (signed) are errors (func_08AEFD48(r) and, when bit 0 of [0x08CDFB3C] is set, func_08AD83B8([0x08CDFB40], 0x08C91F50, 0x1178 / 0x1183, 0x08C926EC / 0x08C926CC, a or r[0x20], wanted)), v0 = -1. When `check` is not 0 and r[0x20] - r[0x24] < wanted: func_08C7AA10([0x08D41AD0 + 0x53C], 1 << r[8], 0x20, 0, 0) is repeated until enough is free. n = wanted when wanted < func_08AF0978(a), else the result of a second func_08AF0978(a). Then while n > 0: c = min(r[0x20] - r[0x28], n - done); when `buffer` is not null func_08B79A88(r[0x1C] + r[0x28], buffer + done, c) (a copy); func_08C7AA20([0x08D41AD0 + 0x538], 1, 0); r[0x24] += c and r[0x28] = (r[0x28] + c) % r[0x20] (a signed remainder); func_08C7AA08([0x08D41AD0 + 0x538]). func_08AEFD48(r); v0 = n (0 or less included).
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08AF0B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t G = 0x08D41AD0u;
    const std::uint32_t a = ctx.gpr[4], buffer = ctx.gpr[5], wanted = ctx.gpr[6], check = ctx.gpr[7];
    const std::uint32_t r = call_set(rt, ctx, 0x08AEFD90u, {a, 0x08C926C4u});
    if (r == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (call_set(rt, ctx, 0x08AEFCF0u, {r}) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const auto fail = [&](std::uint32_t line, std::uint32_t site, std::uint32_t extra, bool with_wanted) {
        (void)call_set(rt, ctx, 0x08AEFD48u, {r});
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) {
            const std::uint32_t log = word(rt, 0x08CDFB40u);
            if (with_wanted) (void)call_set(rt, ctx, 0x08AD83B8u, {log, 0x08C91F50u, line, site, extra, wanted});
            else (void)call_set(rt, ctx, 0x08AD83B8u, {log, 0x08C91F50u, line, site, extra});
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    if (static_cast<std::int8_t>(byte_u(rt, r + 0x11u)) != 0) {
        fail(0x1178u, 0x08C926ECu, a, false);
        return;
    }
    if (check != 0u) {
        const std::uint32_t total = word(rt, r + 0x20u);
        if (signed_of(total) < signed_of(wanted)) {
            fail(0x1183u, 0x08C926CCu, total, true);
            return;
        }
        if (signed_of(total - word(rt, r + 0x24u)) < signed_of(wanted)) {
            do {
                (void)call_set(rt, ctx, 0x08C7AA10u, {word(rt, G + 0x53Cu), 1u << (word(rt, r + 8u) & 31u), 0x20u, 0u, 0u});
            } while (signed_of(word(rt, r + 0x20u) - word(rt, r + 0x24u)) < signed_of(wanted));
        }
    }
    std::uint32_t n;
    if (signed_of(wanted) < signed_of(call_set(rt, ctx, 0x08AF0978u, {a}))) {
        n = wanted;
    } else {
        n = call_set(rt, ctx, 0x08AF0978u, {a});
    }
    std::uint32_t done = 0u;
    if (signed_of(n) > 0) {
        do {
            const std::uint32_t head = word(rt, r + 0x28u);
            const std::uint32_t room = word(rt, r + 0x20u) - head;
            const std::uint32_t left = n - done;
            const std::uint32_t chunk = signed_of(room) < signed_of(left) ? room : left;
            const std::uint32_t from = buffer + done;
            done += chunk;
            if (buffer != 0u) (void)call_set(rt, ctx, 0x08B79A88u, {word(rt, r + 0x1Cu) + head, from, chunk});
            (void)call_set(rt, ctx, 0x08C7AA20u, {word(rt, G + 0x538u), 1u, 0u});
            const std::uint32_t sum = word(rt, r + 0x28u) + chunk;
            const std::uint32_t modulus = word(rt, r + 0x20u);
            put(rt, r + 0x24u, word(rt, r + 0x24u) + chunk);
            put(rt, r + 0x28u, mips_hi(sum, modulus));
            (void)call_set(rt, ctx, 0x08C7AA08u, {word(rt, G + 0x538u), 1u});
        } while (signed_of(done) < signed_of(n));
    }
    (void)call_set(rt, ctx, 0x08AEFD48u, {r});
    ctx.gpr[2] = n;
}

// func_089ABBB0(self): marks the children c of the list self[0x12C .. 0x130) that lie inside the rectangle (x, y, w, h) = (r4, r1, r2, r3) with r1 = func_089A8B50(self), r2 = func_089A8B68(self), r3 = func_089A8B80(self), r4 = func_089A8B38(self) (stored as sp[0] = r4, sp[4] = r1, sp[8] = r2, sp[0xC] = r3). Each child (when not null) gives a box (x0, y0, w0, h0) = func_08969B98(c, sp + 0x10, sp + 0x14, sp + 0x18, sp + 0x1C); it is outside when x0 + w0 < r4, r4 + r2 < x0, y0 + h0 < r1 or r3 + r1 < y0 (bit 8 of c[0xE4] cleared) and inside otherwise (bit 8 set, remembered). When one child was inside: the virtual slot 0x118 of self (self, sp, 0); then every child whose bit 8 is set has its box read again and slot 0x118 (self, sp + 0x10, 1) runs.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (cursor == word(rt, list + 4u)) return;`; `if (cursor == end) return;`; `if (child != 0u) {`; `const bool outside = (x0 + float_at(rt, frame.at(0x18u)) < r4) || (right < x0) || (y0 + float_at(rt, frame.at(0x1Cu)) < r1) || ...`; `if (outside) {`; `while (cursor != end);`; `if (!any) return;`; `if (cursor == last) return;`; `if (child != 0u && (word(rt, child + 0xE4u) & 0x100u) != 0u) {`; `while (cursor != last);`. The v0 of the four early exits (list empty, range empty, nothing inside, a child that is null or has bit 8 clear in the second walk) was fixed after an independent review (it is the value the asm leaves, not the last callee's).
void func_089ABBB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t list = self + 0x12Cu;
    std::uint32_t cursor = word(rt, list);
    if (cursor == word(rt, list + 4u)) {
        ctx.gpr[2] = cursor;  // the asm has loaded v0 = list end (equal to the begin here) for the compare
        return;
    }
    const float r1 = call_setf(rt, ctx, 0x089A8B50u, {self}, {}).f0;
    const float r2 = call_setf(rt, ctx, 0x089A8B68u, {self}, {}).f0;
    const float r3 = call_setf(rt, ctx, 0x089A8B80u, {self}, {}).f0;
    const float r4 = call_setf(rt, ctx, 0x089A8B38u, {self}, {}).f0;
    const std::uint32_t end = word(rt, self + 0x130u);
    const float right = r4 + r2;
    const float bottom = r3 + r1;
    put_float(rt, frame.at(0u), r4);
    put_float(rt, frame.at(4u), r1);
    put_float(rt, frame.at(8u), r2);
    put_float(rt, frame.at(0xCu), r3);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    if (cursor == end) {
        ctx.gpr[2] = cursor;  // 089ABC30: v0 = s0 before the beq
        return;
    }
    bool any = false;
    do {
        const std::uint32_t child = word(rt, cursor);
        if (child != 0u) {
            (void)call_set(rt, ctx, 0x08969B98u, {child, frame.at(0x10u), frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu)});
            const float x0 = float_at(rt, frame.at(0x10u));
            const float y0 = float_at(rt, frame.at(0x14u));
            const bool outside = (x0 + float_at(rt, frame.at(0x18u)) < r4) || (right < x0) || (y0 + float_at(rt, frame.at(0x1Cu)) < r1) || (bottom < y0);
            const std::uint32_t flags = word(rt, child + 0xE4u);
            if (outside) {
                put(rt, child + 0xE4u, flags & ~0x100u);
            } else {
                any = true;
                put(rt, child + 0xE4u, flags | 0x100u);
            }
        }
        cursor += 4u;
    } while (cursor != end);
    if (!any) {
        ctx.gpr[2] = cursor;  // the delay slot of the loop branch: v0 = s0 = end
        return;
    }
    (void)call_set(rt, ctx, slot_of(rt, self, 0x118u), {self, frame.at(0u), 0u});
    cursor = word(rt, list);
    const std::uint32_t last = word(rt, self + 0x130u);
    if (cursor == last) return;
    do {
        const std::uint32_t child = word(rt, cursor);
        cursor += 4u;
        ctx.gpr[2] = child;  // lw v0, 0(a0)
        if (child != 0u) {
            ctx.gpr[2] = word(rt, child + 0xE4u) & 0x100u;  // lw v0, 0xE4(v0); andi v0, v0, 0x100
            if (ctx.gpr[2] != 0u) {
                (void)call_set(rt, ctx, 0x08969B98u, {child, frame.at(0x10u), frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu)});
                (void)call_set(rt, ctx, slot_of(rt, self, 0x118u), {self, frame.at(0x10u), 1u});
            }
        }
    } while (cursor != last);
}

// func_0896A540(self): a per-frame step of a node. func_089BB1E8(self); when func_08965BD4(self) is 0, func_08966040(self, 1) when bit 12 of self[0xE0] is set, else func_08966040(self, 2). When func_08968C84(self) is not 0 and self[0xD0] is 0, self[0xD0] = func_08968FE4(). func_0896A254(self, 0). With mode = func_08965BD4(self): mode 2, or mode 1 with func_08965CE0(self) not 0, runs func_0896A35C(self, 0, 0, sp, sp + 4) (the pair (sx, sy)) and, for o = self[0xD0] not null: if bit 0 of o[0x28] is set it is cleared and o[0] / o[8] are divided by sx (when 0 < sx) and o[4] / o[0xC] by sy (when 0 < sy); then the same with bit 1 and o[0x10] / o[0x18] by sx, o[0x14] / o[0x1C] by sy. Then, with f = self[0xE4], when bit 5 is set: it is cleared, and with g = self[0xE0]: g & 0xFF0 == 0 clears bit 14 of g; else when ((g >> 4) & 0xF) & ~((g >> 8) & 0xF) is not 0, self[0xE0] = (g | 0x4000) with bits 4-7 and 8-11 cleared. func_0896A4E0(self); and for c = self[0x90], then c[0x54], ...: the virtual slot 0x12C of c (c).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (word(rt, self + 0xD0u) == 0u) put(rt, self + 0xD0u, call_set(rt, ctx, 0x08968FE4u, {}));`; `if (!scale && mode == 1u) scale = call_set(rt, ctx, 0x08965CE0u, {self}) != 0u;`; `if (scale) {`; `const auto divide = [&](std::uint32_t offset, std::uint32_t by) {`; `if ((flags & 1u) != 0u) {`; `if (0.0f < float_at(rt, frame.at(0u))) {`; `if (0.0f < float_at(rt, frame.at(4u))) {`; `if ((flags & 2u) != 0u) {`; `if ((f & 0x20u) != 0u) {`; `if ((g & 0xFF0u) == 0u) {`; `else if ((((g >> 4u) & 0xFu) & ~((g >> 8u) & 0xFu)) != 0u) {`.
void func_0896A540(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x089BB1E8u, {self});
    if (call_set(rt, ctx, 0x08965BD4u, {self}) == 0u) {
        const std::uint32_t mode_arg = (word(rt, self + 0xE0u) & 0x1000u) != 0u ? 1u : 2u;
        (void)call_set(rt, ctx, 0x08966040u, {self, mode_arg});
    }
    if (call_set(rt, ctx, 0x08968C84u, {self}) != 0u) {
        if (word(rt, self + 0xD0u) == 0u) put(rt, self + 0xD0u, call_set(rt, ctx, 0x08968FE4u, {}));
    }
    (void)call_set(rt, ctx, 0x0896A254u, {self, 0u});
    const std::uint32_t mode = call_set(rt, ctx, 0x08965BD4u, {self});
    bool scale = mode == 2u;
    if (!scale && mode == 1u) scale = call_set(rt, ctx, 0x08965CE0u, {self}) != 0u;
    if (scale) {
        (void)call_set(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0u), frame.at(4u)});
        if (word(rt, self + 0xD0u) != 0u) {
            const auto divide = [&](std::uint32_t offset, std::uint32_t by) {
                const std::uint32_t o = word(rt, self + 0xD0u);
                put_float(rt, o + offset, float_at(rt, o + offset) / float_at(rt, frame.at(by)));
            };
            std::uint32_t flags = word(rt, word(rt, self + 0xD0u) + 0x28u);
            if ((flags & 1u) != 0u) {
                put(rt, word(rt, self + 0xD0u) + 0x28u, flags & ~1u);
                if (0.0f < float_at(rt, frame.at(0u))) {
                    divide(0u, 0u);
                    divide(8u, 0u);
                }
                if (0.0f < float_at(rt, frame.at(4u))) {
                    divide(4u, 4u);
                    divide(0xCu, 4u);
                }
                flags = word(rt, word(rt, self + 0xD0u) + 0x28u);
            }
            if ((flags & 2u) != 0u) {
                put(rt, word(rt, self + 0xD0u) + 0x28u, flags & ~2u);
                if (0.0f < float_at(rt, frame.at(0u))) {
                    divide(0x10u, 0u);
                    divide(0x18u, 0u);
                }
                if (0.0f < float_at(rt, frame.at(4u))) {
                    divide(0x14u, 4u);
                    divide(0x1Cu, 4u);
                }
            }
        }
    }
    std::uint32_t f = word(rt, self + 0xE4u);
    if ((f & 0x20u) != 0u) {
        f &= ~0x20u;
        std::uint32_t g = word(rt, self + 0xE0u);
        put(rt, self + 0xE4u, f);
        if ((g & 0xFF0u) == 0u) {
            put(rt, self + 0xE0u, g & ~0x4000u);
        } else if ((((g >> 4u) & 0xFu) & ~((g >> 8u) & 0xFu)) != 0u) {
            g = (g | 0x4000u) & ~0xF0u & ~0xF00u;
            put(rt, self + 0xE0u, g);
        }
    }
    (void)call_set(rt, ctx, 0x0896A4E0u, {self});
    std::uint32_t child = word(rt, self + 0x90u);
    while (child != 0u) {
        (void)call_set(rt, ctx, slot_of(rt, child, 0x12Cu), {child});
        child = word(rt, child + 0x54u);
    }
}

// func_08AE0244(x; f12): steers the phase s[0x38] of the object s = func_08ADDCD4(a0, 0x08C91158) (nothing, v0 = 0, when it answers 0) towards the phase of x. With wrap(v) = v - trunc(v) for v >= 0 and C1 - (-v - trunc(-v)) otherwise (C1 = [0x08CB6084], C2 = [0x08CB6088], C3 = [0x08CB608C], the constants of the phase circle): p = wrap(x) * C2; q = wrap(s[0x38]); d1 = |q - wrap(p + C2)| and d2 = |q - wrap(p)|, each folded as C1 - d when it is above C2; when d1 is below d2 the phase p moves on to p + C2. func_08AD8CB0(s + 0x3C, 5, trunc(2 * p * C3)) and s[0x38] = p.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (s == 0u) return;`; `const float a = (0.0f <= v) ? v : -v;`.
void func_08AE0244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t s = call_set(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C91158u});
    if (s == 0u) return;
    const auto whole = [&](float v) { return static_cast<float>(static_cast<std::int32_t>(ctx.fpu_float_to_word(v, 1u))); };
    const auto wrap = [&](float v) {
        if (v < 0.0f) {
            const float a = (0.0f <= v) ? v : -v;
            const float frac = a - whole(a);
            return float_at(rt, 0x08CB6084u) - frac;
        }
        return v - whole(v);
    };
    const auto fold = [&](float a, float b) {
        float d = a - b;
        d = (0.0f <= d) ? d : -d;
        if (float_at(rt, 0x08CB6088u) < d) d = float_at(rt, 0x08CB6084u) - d;
        return d;
    };
    float p = psp_mul(wrap(x), float_at(rt, 0x08CB6088u));
    const float shifted = p + float_at(rt, 0x08CB6088u);
    const float phase = float_at(rt, s + 0x38u);
    const float d1 = fold(wrap(phase), wrap(shifted));
    const float d2 = fold(wrap(phase), wrap(p));
    if (d1 < d2) p = p + float_at(rt, 0x08CB6088u);
    const float twice = psp_mul(p, float_at(rt, 0x08CB608Cu));
    const float doubled = twice + twice;
    (void)call_set(rt, ctx, 0x08AD8CB0u, {s + 0x3Cu, 5u, ctx.fpu_float_to_word(doubled, 1u)});
    put_float(rt, s + 0x38u, p);
}

// func_08928A80(self, a, b): refreshes a scene object. self[0x1FC4] = 0, func_08925D10(self + 0x1FD4), func_08927F9C(self, a), func_089277E8(self, a), func_089283AC(self, a); then for k = 0..11: c = func_0892778C(self, k); c is skipped when it is the object func_08927120(self) or when the virtual slot 0x34 of c answers other than 1 (the slot is only asked for objects that are not the one func_08927120 gave); otherwise func_08926AE0(func_0892778C(self, k), a, b, self[0x1FB0]). func_089282E0(self); self[0x1FC4] = (self[0x1FC5] | func_08927120(self)[0xC0]) != 0 and self[0x1FC5] = 0. By self[0x1FB8]: 0xA copies the four floats func_0891A9E0(sp + 0x1C, self + 0x1B20) gives (x, y, z, w order sp[0x1C], sp[0x20], sp[0x24], sp[0x28]) to self + 0x20E4 and the four of func_0891A8C4(sp + 0xC, self + 0x1B20) to self + 0x20F4; other values: i = func_089275A8(self) and, when i >= 0, the object o = func_088DCD90(a, i) (not null: the four floats at virtual slot 0x38 of o are copied to self + 0x20E4) and self + 0x20F4 = (0, 0, 0, 1.0f). func_08928880(self, self + 0x2058); when self[0x1FC4] is set self[0x215C] = func_08927128(self); (sx, sy, sz) = func_089279B4(self, sp + 0x2C); when self[0x2114] is 0 or the three floats differ from self[0x2118 .. 0x2120] they replace them and self[0x2114] = (byte) func_088DB56C(a, self + 0x2124, (sx, sy, sz)).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (mode == 0xAu) {`; `if (signed_of(index) >= 0) {`; `if (o != 0u) {`; `if (byte_u(rt, self + 0x2114u) != 0u && x == float_at(rt, self + 0x2118u) && y == float_at(rt, self + 0x2118u + 4u) && z == flo...`.
void func_08928A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    put8(rt, self + 0x1FC4u, 0u);
    (void)call_set(rt, ctx, 0x08925D10u, {self + 0x1FD4u});
    (void)call_set(rt, ctx, 0x08927F9Cu, {self, a});
    (void)call_set(rt, ctx, 0x089277E8u, {self, a});
    (void)call_set(rt, ctx, 0x089283ACu, {self, a});
    for (std::uint32_t k = 0u; k != 0xCu; ++k) {
        const std::uint32_t object = call_set(rt, ctx, 0x0892778Cu, {self, k});
        const std::uint32_t current = call_set(rt, ctx, 0x08927120u, {self});
        if (current != object) {
            if (call_set(rt, ctx, slot_of(rt, object, 0x34u), {object}) != 1u) continue;
        }
        const std::uint32_t target = call_set(rt, ctx, 0x0892778Cu, {self, k});
        (void)call_set(rt, ctx, 0x08926AE0u, {target, a, b, word(rt, self + 0x1FB0u)});
    }
    (void)call_set(rt, ctx, 0x089282E0u, {self});
    const std::uint32_t scene = call_set(rt, ctx, 0x08927120u, {self});
    const std::uint32_t lit = byte_u(rt, scene + 0xC0u);
    const std::uint32_t pending = byte_u(rt, self + 0x1FC5u);
    const std::uint32_t mode = word(rt, self + 0x1FB8u);
    put8(rt, self + 0x1FC5u, 0u);
    put8(rt, self + 0x1FC4u, (pending | lit) != 0u ? 1u : 0u);
    if (mode == 0xAu) {
        const std::uint32_t origin = self + 0x1B20u;
        (void)call_set(rt, ctx, 0x0891A9E0u, {frame.at(0x1Cu), origin});
        const std::uint32_t f2 = word(rt, frame.at(0x20u)), f3 = word(rt, frame.at(0x24u)), f1 = word(rt, frame.at(0x28u)), f0 = word(rt, frame.at(0x1Cu));
        put(rt, self + 0x20E4u, f0);
        put(rt, self + 0x20E4u + 0xCu, f1);
        put(rt, self + 0x20E4u + 4u, f2);
        put(rt, self + 0x20E4u + 8u, f3);
        (void)call_set(rt, ctx, 0x0891A8C4u, {frame.at(0xCu), origin});
        const std::uint32_t g2 = word(rt, frame.at(0x10u)), g3 = word(rt, frame.at(0x14u)), g1 = word(rt, frame.at(0x18u)), g0 = word(rt, frame.at(0xCu));
        put(rt, self + 0x20F4u, g0);
        put(rt, self + 0x20F4u + 0xCu, g1);
        put(rt, self + 0x20F4u + 4u, g2);
        put(rt, self + 0x20F4u + 8u, g3);
    } else {
        const std::uint32_t index = call_set(rt, ctx, 0x089275A8u, {self});
        if (signed_of(index) >= 0) {
            const std::uint32_t o = call_set(rt, ctx, 0x088DCD90u, {a, index});
            if (o != 0u) {
                const std::uint32_t floats = call_set(rt, ctx, slot_of(rt, o, 0x38u), {o});
                put(rt, self + 0x20E4u, word(rt, floats));
                put(rt, self + 0x20E4u + 4u, word(rt, floats + 4u));
                put(rt, self + 0x20E4u + 8u, word(rt, floats + 8u));
                put(rt, self + 0x20E4u + 0xCu, word(rt, floats + 0xCu));
            }
            put(rt, self + 0x20F4u, 0u);
            put_float(rt, self + 0x20F4u + 0xCu, 1.0f);
            put(rt, self + 0x20F4u + 4u, 0u);
            put(rt, self + 0x20F4u + 8u, 0u);
        }
    }
    (void)call_set(rt, ctx, 0x08928880u, {self, self + 0x2058u});
    if (byte_u(rt, self + 0x1FC4u) != 0u) put(rt, self + 0x215Cu, call_set(rt, ctx, 0x08927128u, {self}));
    (void)call_set(rt, ctx, 0x089279B4u, {self, frame.at(0x2Cu)});
    const float x = float_at(rt, frame.at(0x2Cu)), y = float_at(rt, frame.at(0x30u)), z = float_at(rt, frame.at(0x34u));
    put(rt, frame.at(0u), word(rt, frame.at(0x2Cu)));
    put(rt, frame.at(4u), word(rt, frame.at(0x30u)));
    put(rt, frame.at(8u), word(rt, frame.at(0x34u)));
    ctx.gpr[2] = self + 0x2118u;
    if (byte_u(rt, self + 0x2114u) != 0u && x == float_at(rt, self + 0x2118u) && y == float_at(rt, self + 0x2118u + 4u) && z == float_at(rt, self + 0x2118u + 8u)) return;
    put(rt, self + 0x2118u, word(rt, frame.at(0u)));
    put(rt, self + 0x2118u + 8u, word(rt, frame.at(8u)));
    put(rt, self + 0x2118u + 4u, word(rt, frame.at(4u)));
    const std::uint32_t found = call_set(rt, ctx, 0x088DB56Cu, {a, self + 0x2124u, frame.at(0u)});
    put8(rt, self + 0x2114u, found);
}

// func_08883B7C(self, other): re-aims the camera record c = self[0xC] (nothing, v0 = 0, when func_088819D0(c, self[8]) answers 0 or the byte self[4][0x20] is 0). func_08883AE0(c, func_0886A9A0(self[4], 5)); o = func_0886B080(self[8]) and the virtual slot 0x28 of o (o, c[0x16F0]) answering a non-zero n gives func_0889F3A8(c + 0x130, n, 1); func_08882558(c). A matrix m at sp + 0x60 is built from func_0886A9A0(self[4], 4): func_08B4E9E0(m, func_088703AC(r)) when r is not 0, func_08B4E648(m) when it is; func_08B4E9D4(m, c). With (x, y, z) = c[0x140 .. 0x148] func_08B4B114(c + 0x130, sp + 0xA0; f12 = x, f13 = y, f14 = z), func_08882E34(sp + 0xE0, sp + 0xA0, c + 0x164), func_08B4E9D4(sp + 0xE0, m), func_088822CC(sp + 0x30, sp + 0xE0) and the four floats at sp + 0x30 are copied to c + 0x164. sp[0x68] is clamped to -1.0f .. 1.0f; func_08B4F5E0(sp, m); the three floats c[0x140 ..] and sp[0x90 .. 0x9C] (sp[0x10 ..]) go through func_08B4BAF8(sp, sp + 0x20), func_08B4A890(sp + 0x20, sp + 0x20, sp + 0x10) and func_08B4B864(c + 0x130, sp), sp[0x20 .. 0x28] are written back to c[0x140 .. 0x148]. When c[0x4C] = e is not null: f20 = slot 0x1C of other (other) * func_0886B0F8(self[8]) goes to the record at sp + 0x30 (zero but sp[0x4C] = f20) and the virtual slot 0xC of e (e, sp + 0x30, c + 0x54) runs.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (ctx.gpr[2] == 0u) return;`; `if (n != 0u) (void)call_set(rt, ctx, 0x0889F3A8u, {word(rt, self + 0xCu) + 0x130u, n, 1u});`; `if (r != 0u) {`; `else {`; `if (f0 < -1.0f) {`; `if (1.0f < f0) put_float(rt, frame.at(0x68u), 1.0f);`; `if (e == 0u) return;`.
void func_08883B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x150u);
    if (call_set(rt, ctx, 0x088819D0u, {word(rt, self + 0xCu), word(rt, self + 8u)}) == 0u) return;
    ctx.gpr[2] = byte_u(rt, word(rt, self + 4u) + 0x20u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t t = call_set(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 5u});
    (void)call_set(rt, ctx, 0x08883AE0u, {word(rt, self + 0xCu), t});
    const std::uint32_t o = call_set(rt, ctx, 0x0886B080u, {word(rt, self + 8u)});
    const std::uint32_t slot = word(rt, word(rt, o) + 0x28u);
    const std::uint32_t n = call_set(rt, ctx, slot, {o, word(rt, word(rt, self + 0xCu) + 0x16F0u)});
    if (n != 0u) (void)call_set(rt, ctx, 0x0889F3A8u, {word(rt, self + 0xCu) + 0x130u, n, 1u});
    (void)call_set(rt, ctx, 0x08882558u, {word(rt, self + 0xCu)});
    const std::uint32_t matrix = frame.at(0x60u);
    const std::uint32_t r = call_set(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 4u});
    if (r != 0u) {
        const std::uint32_t converted = call_set(rt, ctx, 0x088703ACu, {r});
        (void)call_set(rt, ctx, 0x08B4E9E0u, {matrix, converted});
    } else {
        (void)call_set(rt, ctx, 0x08B4E648u, {matrix});
    }
    (void)call_set(rt, ctx, 0x08B4E9D4u, {matrix, word(rt, self + 0xCu)});
    std::uint32_t c = word(rt, self + 0xCu);
    (void)call_setf(rt, ctx, 0x08B4B114u, {c + 0x130u, frame.at(0xA0u)}, {float_at(rt, c + 0x140u), float_at(rt, c + 0x144u), float_at(rt, c + 0x148u)});
    (void)call_set(rt, ctx, 0x08882E34u, {frame.at(0xE0u), frame.at(0xA0u), word(rt, self + 0xCu) + 0x164u});
    (void)call_set(rt, ctx, 0x08B4E9D4u, {frame.at(0xE0u), matrix});
    (void)call_set(rt, ctx, 0x088822CCu, {frame.at(0x30u), frame.at(0xE0u)});
    c = word(rt, self + 0xCu);
    for (std::uint32_t i = 0u; i != 4u; ++i) put(rt, c + 0x164u + 4u * i, word(rt, frame.at(0x30u + 4u * i)));
    float f0 = float_at(rt, frame.at(0x68u));
    if (f0 < -1.0f) {
        f0 = -1.0f;
        put_float(rt, frame.at(0x68u), -1.0f);
    }
    if (1.0f < f0) put_float(rt, frame.at(0x68u), 1.0f);
    (void)call_set(rt, ctx, 0x08B4F5E0u, {frame.at(0u), matrix});
    c = word(rt, self + 0xCu);
    put(rt, frame.at(0x20u), word(rt, c + 0x140u));
    put(rt, frame.at(0x24u), word(rt, c + 0x144u));
    put(rt, frame.at(0x10u), word(rt, frame.at(0x90u)));
    put(rt, frame.at(0x14u), word(rt, frame.at(0x94u)));
    put(rt, frame.at(0x28u), word(rt, c + 0x148u));
    put(rt, frame.at(0x18u), word(rt, frame.at(0x98u)));
    put(rt, frame.at(0x1Cu), word(rt, frame.at(0x9Cu)));
    put(rt, frame.at(0x2Cu), 0u);
    (void)call_set(rt, ctx, 0x08B4BAF8u, {frame.at(0u), frame.at(0x20u)});
    (void)call_set(rt, ctx, 0x08B4A890u, {frame.at(0x20u), frame.at(0x20u), frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08B4B864u, {word(rt, self + 0xCu) + 0x130u, frame.at(0u)});
    put(rt, word(rt, self + 0xCu) + 0x140u, word(rt, frame.at(0x20u)));
    put(rt, word(rt, self + 0xCu) + 0x144u, word(rt, frame.at(0x24u)));
    put(rt, word(rt, self + 0xCu) + 0x148u, word(rt, frame.at(0x28u)));
    ctx.gpr[2] = word(rt, self + 0xCu);
    const std::uint32_t e = word(rt, ctx.gpr[2] + 0x4Cu);
    if (e == 0u) return;
    const GuestResult scale = gt::game::hot::call_indirect(rt, ctx, word(rt, word(rt, other) + 0x1Cu), {other}, {});
    take(ctx, scale);
    float f20 = scale.f0;
    f20 = psp_mul(f20, call_setf(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)}, {}).f0);
    c = word(rt, self + 0xCu);
    put(rt, frame.at(0x30u), 0u);
    put_float(rt, frame.at(0x4Cu), f20);
    put(rt, frame.at(0x34u), 0u);
    put(rt, frame.at(0x38u), 0u);
    put(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x40u), 0u);
    put(rt, frame.at(0x44u), 0u);
    put(rt, frame.at(0x48u), 0u);
    put(rt, frame.at(0x50u), 0u);
    put8(rt, frame.at(0x54u), 0u);
    put8(rt, frame.at(0x55u), 0u);
    const std::uint32_t target = word(rt, c + 0x4Cu);
    (void)call_set(rt, ctx, word(rt, word(rt, target) + 0xCu), {target, frame.at(0x30u), c + 0x54u});
}

// func_089BEF60(self, other): func_08B344E0(self + 0x2B0) ... func_08B3457C(self + 0x2B0). With t = self[0x264]: when t is null or the virtual slot 0x18 of t (t) answers 0 only func_089E0E68(self, other) runs. Otherwise slot 0x2C and slot 0x34 of the object self[0x264] run (each time re-read; a null one calls func_08A5BB7C(0x08CA5EA4) first and still reads its table from address 0), func_089BEA78(self, sp, sp + 4, sp + 8, sp + 0xC), the float of slot 0x38 (f20) and the float of slot 0x3C (f4); other[0x118] = 1 and the record sp[0x10 .. 0x2C] = (0, 0, f20, f4, x, y, x + w, y + h) with (x, y, w, h) = sp[0 .. 0xC]; slot 0x114 of other (other), func_089A932C(other, sp + 0x20, sp + 0x10), slot 0x114 again, func_089A8C24(other) and slot 0x30 of self[0x264]. (The two empty countdown loops the compiler left between the calls and the stores do nothing.)
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (!simple) simple = call_set(rt, ctx, slot_of(rt, t, 0x18u), {t}) == 0u;`; `if (simple) {`; `if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5EA4u});`.
void func_089BEF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t lock = self + 0x2B0u;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t t = word(rt, self + 0x264u);
    bool simple = t == 0u;
    if (!simple) simple = call_set(rt, ctx, slot_of(rt, t, 0x18u), {t}) == 0u;
    if (simple) {
        (void)call_set(rt, ctx, 0x089E0E68u, {self, other});
        (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
        return;
    }
    const auto current = [&]() {
        const std::uint32_t s0 = word(rt, self + 0x264u);
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5EA4u});
        return s0;
    };
    std::uint32_t s0 = current();
    (void)call_set(rt, ctx, slot_of(rt, s0, 0x2Cu), {s0});
    s0 = current();
    (void)call_set(rt, ctx, slot_of(rt, s0, 0x34u), {s0});
    (void)call_set(rt, ctx, 0x089BEA78u, {self, frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)});
    s0 = word(rt, self + 0x264u);
    const GuestResult first = [&]() {
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5EA4u});
        return call_guest(rt, ctx, slot_of(rt, s0, 0x38u), {s0}, {});
    }();
    take(ctx, first);
    const float f20 = first.f0;
    s0 = word(rt, self + 0x264u);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5EA4u});
    const GuestResult second = call_guest(rt, ctx, slot_of(rt, s0, 0x3Cu), {s0}, {});
    take(ctx, second);
    const float f4 = second.f0;
    const float f2 = float_at(rt, frame.at(0u)), f3 = float_at(rt, frame.at(4u));
    const float width = f2 + float_at(rt, frame.at(8u));
    const float height = f3 + float_at(rt, frame.at(0xCu));
    const std::uint32_t table = word(rt, other);
    put_float(rt, frame.at(0x1Cu), f4);
    put_float(rt, frame.at(0x28u), width);
    put_float(rt, frame.at(0x2Cu), height);
    put_float(rt, frame.at(0x18u), f20);
    put8(rt, other + 0x118u, 1u);
    put_float(rt, frame.at(0x20u), f2);
    put_float(rt, frame.at(0x24u), f3);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    (void)call_set(rt, ctx, word(rt, table + 0x114u), {other});
    (void)call_set(rt, ctx, 0x089A932Cu, {other, frame.at(0x20u), frame.at(0x10u)});
    (void)call_set(rt, ctx, slot_of(rt, other, 0x114u), {other});
    (void)call_set(rt, ctx, 0x089A8C24u, {other});
    s0 = current();
    (void)call_set(rt, ctx, slot_of(rt, s0, 0x30u), {s0});
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
}

// func_08B610C0(obj): fills the table of 47 code / data addresses of obj[8] (allocated and zeroed first when it is null: p = func_08B62344(0xC8), p[0] = 0x08CDB7E0, the words p[4 .. 0xC0] = 0, the byte p[0xC4] = 0, obj[8] = p). The entries (offset, value) of p are the ones in kEntries (read from the stores the asm makes, the values are the relocated addresses of the lui / addiu pairs). v0 = 0x08CB5A9C and v1 = 0x08CB5AA0 at the return.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (table == 0u) {`; `for (std::uint32_t offset = 4u; offset <= 0xC0u; offset += 4u) put(rt, table + offset, 0u);`.
void func_08B610C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kEntries[][2] = {
        {0x8, 0x08CB5990u},  {0xC, 0x08CB5990u},  {0x10, 0x08CB599Cu}, {0x14, 0x08CB599Cu}, {0x18, 0x08C95E00u}, {0x1C, 0x08C95E00u}, {0x20, 0x08CB59A8u},
        {0x24, 0x08CB59ACu}, {0x28, 0x08C95E00u}, {0x2C, 0x08CB59B0u}, {0x30, 0x08CB59B8u}, {0x34, 0x08CB59C0u}, {0x38, 0x08CB59C8u}, {0x3C, 0x08CB59D4u},
        {0x40, 0x08CB59E0u}, {0x44, 0x08CB59E8u}, {0x48, 0x08CB59F4u}, {0x4C, 0x08CB59F8u}, {0x50, 0x08CB59FCu}, {0x54, 0x08CB5A00u}, {0x58, 0x08CB5A04u},
        {0x5C, 0x08CB5A08u}, {0x60, 0x08CB5A0Cu}, {0x64, 0x08CB5A10u}, {0x68, 0x08CB5A18u}, {0x6C, 0x08CB5A24u}, {0x70, 0x08CB5A2Cu}, {0x74, 0x08CB5A34u},
        {0x78, 0x08CB5A38u}, {0x7C, 0x08CB5A40u}, {0x80, 0x08CB5A48u}, {0x84, 0x08CB5A50u}, {0x88, 0x08CB5A5Cu}, {0x8C, 0x08CB5A64u}, {0x90, 0x08CB5A70u},
        {0x94, 0x08CB5A7Cu}, {0x98, 0x08CB5A80u}, {0x9C, 0x08CB5A84u}, {0xA0, 0x08CB5A88u}, {0xA4, 0x08CB5A34u}, {0xA8, 0x08CB5A8Cu}, {0xAC, 0x08CB5A90u},
        {0xB0, 0x08CB5A94u}, {0xB4, 0x08CB5A98u}, {0xB8, 0x08CB5A9Cu}, {0xBC, 0x08CB5AA0u}, {0xC0, 0x08CB5AA4u},
    };
    const std::uint32_t obj = ctx.gpr[4];
    std::uint32_t table = word(rt, obj + 8u);
    if (table == 0u) {
        table = call_set(rt, ctx, 0x08B62344u, {0xC8u});
        put(rt, table, 0x08CDB7E0u);
        for (std::uint32_t offset = 4u; offset <= 0xC0u; offset += 4u) put(rt, table + offset, 0u);
        put(rt, obj + 8u, table);
        put8(rt, table + 0xC4u, 0u);
    }
    for (const auto &entry : kEntries) put(rt, table + entry[0], entry[1]);
    ctx.gpr[2] = 0x08CB5A9Cu;
    ctx.gpr[3] = 0x08CB5AA0u;
}

// func_088EE6C0(self): the constructor of a 0x558-byte object. self[0] = 0x08CC3970, self[0xA4] = self[0xA8] = 0; self[0xAC] = func_08B62344(1) after func_088EF418 initialises it; self[0xB0 .. 0xC0] = 0; func_08B28DCC(self + 0xC4) and (self + 0xEC); self[0x114] = func_08B62344(0x288) initialised by func_088EF210, self[0x118] = func_08B62344(0x30) initialised by func_088EEF20; the zeroed pairs at self + 0x128 and self + 0x178 (halfword -1 first), func_08B28DCC(self + 0x1C8); words self[0x1F0 .. 0x20C] = 0; four entries of 0x90 bytes at self + 0x218 (func_08B36018(e, 0, 0x80), halfword -1, bytes 0x88 / 0x89 and words 0x80 / 0x84 zero); the flag block self[0x4E0 .. 0x554] set to its defaults (-1 at 0x510, 0x52D .. 0x52F, 0x548, 0x54C; 1 at 0x524), self[0x544] = func_08B6461C(0x400), func_08B36018(self + 4, 0, 0xA0), func_08B36018(self + 0x218, 0, 0x240), self[0xA8] = func_08B0980C(func_08B0979C()), self[0x120] = func_08B6461C(8), self[0x4E8] = self[0x4F0] = 0, func_088ED494(self + 0x4C0) and four slots k = 0..3 of small tables (words self[0x468 + 8k], [0x46C + 8k], [0x488 + 8k], [0x48C + 8k] and [0x28 + 0x28k], halfword [0x458 + 2k], bytes [0x210 + k], [0x460 + k], [0x4A8 + k] = 0xFF, [0x4AC + k], [0x4B0 + k], [0x4B4 + k] = 0xFF, [0x4B8 + k]). v0 = self + 3.
// Branches the compared cases do not take: none (the coverage run of the fast gate ran every branch and region).
void func_088EE6C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CC3970u);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0xA8u, 0u);
    std::uint32_t s0 = call_set(rt, ctx, 0x08B62344u, {1u});
    (void)call_set(rt, ctx, 0x088EF418u, {s0});
    put(rt, self + 0xACu, s0);
    put(rt, self + 0xB0u, 0u);
    put(rt, self + 0xB4u, 0u);
    put(rt, self + 0xB8u, 0u);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC0u, 0u);
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0xC4u});
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0xECu});
    s0 = call_set(rt, ctx, 0x08B62344u, {0x288u});
    (void)call_set(rt, ctx, 0x088EF210u, {s0});
    put(rt, self + 0x114u, s0);
    s0 = call_set(rt, ctx, 0x08B62344u, {0x30u});
    (void)call_set(rt, ctx, 0x088EEF20u, {s0});
    put(rt, self + 0x118u, s0);
    put(rt, self + 0x11Cu, 0u);
    put(rt, self + 0x120u, 0u);
    put(rt, self + 0x124u, 0u);
    put16(rt, self + 0x178u, 0xFFFFFFFFu);
    put(rt, self + 0x128u + 8u, 0u);
    put(rt, self + 0x128u + 0xCu, 0u);
    put16(rt, self + 0x128u + 2u, 0u);
    put(rt, self + 0x128u + 4u, 0u);
    put(rt, self + 0x178u + 8u, 0u);
    put(rt, self + 0x178u + 0xCu, 0u);
    put16(rt, self + 0x128u, 0xFFFFFFFFu);
    put16(rt, self + 0x178u + 2u, 0u);
    put(rt, self + 0x178u + 4u, 0u);
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x1C8u});
    put(rt, self + 0x208u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x1F0u, 0u);
    put(rt, self + 0x1F4u, 0u);
    put(rt, self + 0x1F8u, 0u);
    put(rt, self + 0x1FCu, 0u);
    put(rt, self + 0x200u, 0u);
    const std::uint32_t list = self + 0x218u;
    std::uint32_t entry = list;
    for (std::uint32_t remaining = 4u; remaining != 0u; --remaining) {
        (void)call_set(rt, ctx, 0x08B36018u, {entry, 0u, 0x80u});
        put16(rt, entry, 0xFFFFFFFFu);
        put8(rt, entry + 0x88u, 0u);
        put8(rt, entry + 0x89u, 0u);
        put(rt, entry + 0x80u, 0u);
        put(rt, entry + 0x84u, 0u);
        entry += 0x90u;
    }
    put8(rt, self + 0x524u, 1u);
    put(rt, self + 0x504u, 0u);
    put(rt, self + 0x500u, 0u);
    put8(rt, self + 0x52Fu, 0xFFFFFFFFu);
    put(rt, self + 0x4E0u, 0u);
    put(rt, self + 0x4E4u, 0u);
    put(rt, self + 0x4ECu, 0u);
    put(rt, self + 0x4F4u, 0u);
    put(rt, self + 0x4F8u, 0u);
    put(rt, self + 0x510u, 0xFFFFFFFFu);
    put8(rt, self + 0x514u, 0u);
    put(rt, self + 0x518u, 0u);
    put(rt, self + 0x51Cu, 0u);
    for (const std::uint32_t at : {0x520u, 0x521u, 0x522u, 0x523u, 0x525u, 0x526u, 0x527u, 0x528u, 0x529u, 0x52Au, 0x52Bu, 0x52Cu}) put8(rt, self + at, 0u);
    put8(rt, self + 0x52Du, 0xFFFFFFFFu);
    put8(rt, self + 0x52Eu, 0xFFFFFFFFu);
    put(rt, self + 0x530u, 0u);
    put(rt, self + 0x534u, 0u);
    put(rt, self + 0x538u, 0u);
    put(rt, self + 0x53Cu, 0u);
    put(rt, self + 0x540u, 0u);
    const std::uint32_t buffer = call_set(rt, ctx, 0x08B6461Cu, {0x400u});
    put(rt, self + 0x544u, buffer);
    put(rt, self + 0x54Cu, 0xFFFFFFFFu);
    put(rt, self + 0x548u, 0xFFFFFFFFu);
    put(rt, self + 0x550u, 0u);
    put8(rt, self + 0x554u, 0u);
    (void)call_set(rt, ctx, 0x08B36018u, {self + 4u, 0u, 0xA0u});
    (void)call_set(rt, ctx, 0x08B36018u, {list, 0u, 0x240u});
    (void)call_set(rt, ctx, 0x08B0979Cu, {});
    const std::uint32_t handle = call_set(rt, ctx, 0x08B0980Cu, {ctx.gpr[2]});
    put(rt, self + 0xA8u, handle);
    const std::uint32_t small = call_set(rt, ctx, 0x08B6461Cu, {8u});
    put(rt, self + 0x120u, small);
    put(rt, self + 0x4E8u, 0u);
    put(rt, self + 0x4F0u, 0u);
    (void)call_set(rt, ctx, 0x088ED494u, {self + 0x4C0u});
    std::uint32_t column = self;
    std::uint32_t cell = self + 0x28u;
    std::uint32_t pair = self + 0x458u;
    for (std::uint32_t k = 0u; k != 4u; ++k) {
        const std::uint32_t at = k + self;
        put(rt, column + 0x468u, 0u);
        put(rt, column + 0x46Cu, 0u);
        put(rt, column + 0x488u, 0u);
        put(rt, column + 0x48Cu, 0u);
        column += 8u;
        put16(rt, pair, 0u);
        pair += 2u;
        put8(rt, at + 0x210u, 0u);
        put8(rt, at + 0x460u, 0u);
        put8(rt, at + 0x4B0u, 0u);
        put8(rt, at + 0x4B4u, 0xFFFFFFFFu);
        put8(rt, at + 0x4ACu, 0u);
        put8(rt, at + 0x4B8u, 0u);
        put(rt, cell, 0u);
        cell += 0x28u;
        put8(rt, at + 0x4A8u, 0xFFFFFFFFu);
        ctx.gpr[2] = at;
    }
}

// func_08928E24(self): the constructor of a 0x21A0-byte scene object. Eight sub-objects get their own constructors in the order func_08924DB8(self), func_0891E0F8(self + 0x290), func_0891A088(self + 0x4E0), func_0891A484(self + 0x900), func_0891C4D8(self + 0xB00), func_0891C614(self + 0xD00), func_0891A088(self + 0xF00), func_08926D60(self + 0x1320) (word [0x1320] = 0x08CC5A28 after it), func_08926D60(self + 0x1520) ([0x1520] = 0x08CC59D0), func_08926D60(self + 0x1720) ([0x1720] = 0x08CC6020), func_08918F30(self + 0x1920), func_0891C428(self + 0x1B20), func_0891E220(self + 0x1D30), func_08925AF4(self + 0x1FD4), func_08925A90(self + 0x2010) ([0x2010] = 0x08CC5990, [0x2158] = -1, [0x2154] = 0), then [0x215C] = func_08927128() (a counter), the records at 0x2160 / 0x2174 (two small tables: [0x2160] = 0x08CC6200, [0x2164] = 0x08CC6288, [0x2168] = self, [0x216C] = 0x08CC62A0, [0x2170] = self, [0x2174] = 0x08CC5928, [0x2178] = self + 0x2160, [0x217C] = self + 0x2164), the pointers to the sub-objects at 0x1F70 .. 0x1FA4 and every other field of the object zeroed or set to its default (flags 1 / 3 / 2, the floats 11.0f and 50.0f at 0x20D4 / 0x20D8). v0 = 3.
// Branches the compared cases do not take: none (the coverage run of the fast gate ran every branch and region).
void func_08928E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t s7 = self + 0xD00u, s6 = self + 0x1320u, s5 = self + 0x1520u, s4 = self + 0x1720u, s3 = self + 0x1920u, s2 = self + 0x1B20u, s1 = self + 0x1D30u, s0 = self + 0xF00u;
    put(rt, frame.at(0u), self + 0x290u);
    (void)call_set(rt, ctx, 0x08924DB8u, {self});
    put(rt, frame.at(4u), self + 0x4E0u);
    (void)call_set(rt, ctx, 0x0891E0F8u, {word(rt, frame.at(0u))});
    put(rt, frame.at(8u), self + 0x900u);
    (void)call_set(rt, ctx, 0x0891A088u, {word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x0891A484u, {word(rt, frame.at(8u))});
    (void)call_set(rt, ctx, 0x0891C4D8u, {self + 0xB00u});
    (void)call_set(rt, ctx, 0x0891C614u, {s7});
    (void)call_set(rt, ctx, 0x0891A088u, {s0});
    (void)call_set(rt, ctx, 0x08926D60u, {s6});
    put(rt, self + 0x1320u, 0x08CC5A28u);
    (void)call_set(rt, ctx, 0x08926D60u, {s5});
    put(rt, self + 0x1520u, 0x08CC59D0u);
    (void)call_set(rt, ctx, 0x08926D60u, {s4});
    put(rt, self + 0x1720u, 0x08CC6020u);
    (void)call_set(rt, ctx, 0x08918F30u, {s3});
    (void)call_set(rt, ctx, 0x0891C428u, {s2});
    (void)call_set(rt, ctx, 0x0891E220u, {s1});
    (void)call_set(rt, ctx, 0x08925AF4u, {self + 0x1FD4u});
    (void)call_set(rt, ctx, 0x08925A90u, {self + 0x2010u});
    put(rt, self + 0x2158u, 0xFFFFFFFFu);
    put(rt, self + 0x2010u, 0x08CC5990u);
    put(rt, self + 0x2154u, 0u);
    const std::uint32_t counter = call_set(rt, ctx, 0x08927128u, {});
    const std::uint32_t a1 = self + 0x2160u;
    put(rt, self + 0x215Cu, counter);
    put(rt, a1 + 4u, 0x08CC6288u);
    put(rt, self + 0x2160u, 0x08CC6200u);
    const std::uint32_t t1 = self + 0x2164u;
    const std::uint32_t a3 = self + 0x2174u;
    put(rt, a1 + 8u, self);
    put(rt, self + 0x216Cu, 0x08CC62A0u);
    put(rt, self + 0x2170u, self);
    put(rt, a3 + 8u, t1);
    put(rt, a3 + 4u, a1);
    put(rt, self + 0x2174u, 0x08CC5928u);
    put(rt, self + 0x1F74u, word(rt, frame.at(0u)));
    put(rt, self + 0x1F78u, word(rt, frame.at(8u)));
    put(rt, self + 0x1F84u, s7);
    put(rt, self + 0x1F7Cu, word(rt, frame.at(4u)));
    put(rt, self + 0x1F88u, s6);
    put(rt, self + 0x1F8Cu, s5);
    put(rt, self + 0x1F90u, s4);
    put(rt, self + 0x1FA0u, s0);
    put(rt, self + 0x1F94u, s3);
    put(rt, self + 0x1F98u, s2);
    put(rt, self + 0x1F9Cu, s1);
    put(rt, self + 0x1F70u, self);
    put(rt, self + 0x1F80u, s0);
    put(rt, self + 0x1FA4u, self);
    put(rt, self + 0x1FB4u, self);
    put(rt, self + 0x1FB0u, 0u);
    put(rt, self + 0x1FB8u, 0u);
    put8(rt, self + 0x1FBCu, 0u);
    put8(rt, self + 0x1FBDu, 0u);
    put8(rt, self + 0x1FBEu, 0u);
    put(rt, self + 0x1FC0u, 0u);
    put8(rt, self + 0x1FC4u, 0u);
    put(rt, self + 0x1FD0u, 2u);
    put8(rt, self + 0x1FC5u, 0u);
    put8(rt, self + 0x1FC6u, 0u);
    put8(rt, self + 0x1FC7u, 1u);
    put8(rt, self + 0x204Cu, 1u);
    put(rt, self + 0x1FC8u, 0u);
    put8(rt, self + 0x1FCCu, 0u);
    put8(rt, self + 0x1FCDu, 0u);
    put8(rt, self + 0x1FCEu, 1u);
    put8(rt, self + 0x1FCFu, 0u);
    put(rt, self + 0x2050u, 0u);
    put8(rt, self + 0x2054u, 0u);
    put(rt, self + 0x20B4u, 0u);
    const std::uint32_t t0 = self + 0x20B4u;
    put(rt, t0 + 0xCu, 0u);
    put(rt, t0 + 4u, 0u);
    put(rt, t0 + 8u, 0u);
    const std::uint32_t a0 = self + 0x2058u;
    put_float(rt, a0 + 0x7Cu, 11.0f);
    put_float(rt, a0 + 0x80u, 50.0f);
    put(rt, a0 + 0x84u, 3u);
    put(rt, a0 + 0x88u, 0u);
    put(rt, a0 + 0x6Cu, 0u);
    put(rt, a0 + 0x70u, 0u);
    put(rt, a0 + 0x74u, 0u);
    put(rt, a0 + 0x78u, 0u);
    put8(rt, self + 0x2104u, 0u);
    put8(rt, self + 0x2105u, 0u);
    put8(rt, self + 0x2106u, 0u);
    put8(rt, self + 0x2107u, 0u);
    put8(rt, self + 0x2114u, 0u);
    put(rt, self + 0x2118u, 0u);
    const std::uint32_t t2 = self + 0x2118u;
    put(rt, t2 + 8u, 0u);
    put(rt, t2 + 4u, 0u);
    put8(rt, self + 0x2180u, 0u);
    std::uint32_t t3 = self;
    for (std::uint32_t t4 = 0u; t4 != 2u; ++t4) {
        put(rt, t3 + 0x2184u, 0u);
        put(rt, t3 + 0x2188u, 0u);
        put(rt, t3 + 0x218Cu, 0u);
        put(rt, t3 + 0x2190u, 0u);
        t3 += 0x10u;
    }
    put(rt, self + 0x2108u, 0u);
    put(rt, self + 0x2110u, 0u);
    put(rt, self + 0x210Cu, 0u);
    ctx.gpr[2] = 3u;
}

// func_08915714(): rebuilds four pairs of lookup records from two byte tables. For i = 0..3: func_0893A814(0x08CDDA08 + 0x28 * i, 1) and func_0893A814(0x08CDD968 + 0x28 * i, 0); the byte at 0x08CDD988 + 0x28 * i is clamped to 0x32 .. 0xC8. Then for i = 0..3, with the table rows t1 = 0x08CDDA24 + 0x28 * i and t2 = 0x08CDD984 + 0x28 * i (three bytes each) and the 0x84-byte records r1 = 0x08CF1E40 + 0x84 * i and r2 = 0x08CF1C30 + 0x84 * i: memset(r1, 0, 0x84), func_0893A854(r1, 0x08CDDA08 + 0x28 * i, 0; f12 = 0x3CC6F2D5, f13 = t1[0] / 2550.0f), func_0893A8D8(r1, that, 0; f12 = 0x3C5BE3F9, f13 = t1[1] / 2550.0f, f14 = t1[2] / 2550.0f), func_0893A9CC(r1, that) and the same for r2 with t2 and 0x08CDD968 + 0x28 * i.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (value < 0x32u) put8(rt, at, 0x32u);`; `else if (!(value < 0xC9u)) put8(rt, at, 0xFFFFFFC8u);`.
void func_08915714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        (void)call_set(rt, ctx, 0x0893A814u, {0x08CDDA08u + 0x28u * i, 1u});
        (void)call_set(rt, ctx, 0x0893A814u, {0x08CDD968u + 0x28u * i, 0u});
        const std::uint32_t at = 0x08CDD988u + 0x28u * i;
        const std::uint32_t value = byte_u(rt, at);
        if (value < 0x32u) put8(rt, at, 0x32u);
        else if (!(value < 0xC9u)) put8(rt, at, 0xFFFFFFC8u);
    }
    const float divisor = std::bit_cast<float>(0x451F6000u);
    const auto fill = [&](std::uint32_t record, std::uint32_t row, std::uint32_t pair) {
        (void)call_set(rt, ctx, 0x08B36018u, {record, 0u, 0x84u});
        const float f13 = static_cast<float>(static_cast<std::int32_t>(byte_u(rt, row))) / divisor;
        (void)call_setf(rt, ctx, 0x0893A854u, {record, pair, 0u}, {std::bit_cast<float>(0x3CC6F2D5u), f13});
        const float g13 = static_cast<float>(static_cast<std::int32_t>(byte_u(rt, row + 1u))) / divisor;
        const float g14 = static_cast<float>(static_cast<std::int32_t>(byte_u(rt, row + 2u))) / divisor;
        (void)call_setf(rt, ctx, 0x0893A8D8u, {record, pair, 0u}, {std::bit_cast<float>(0x3C5BE3F9u), g13, g14});
        (void)call_set(rt, ctx, 0x0893A9CCu, {record, pair});
    };
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        fill(0x08CF1E40u + 0x84u * i, 0x08CDDA24u + 0x28u * i, 0x08CDDA08u + 0x28u * i);
        fill(0x08CF1C30u + 0x84u * i, 0x08CDD984u + 0x28u * i, 0x08CDD968u + 0x28u * i);
    }
    ctx.gpr[2] = 4u;
}

// func_08B04D98(out, size, a, b): builds a 0x120-byte object o = func_08B62344(0x120) and stores it in out[0]; the intrusive list heads (each a halfword -1 at +2 of a node and the node's own address or a neighbour's at +8) and counters are zeroed or linked by hand, then func_08C4A6B0(o, n), func_08C4A4AC(o + 0x2C, n), func_08C4A3B8(o + 0xA8, k), func_08C4A2C4(o + 0x38, 2), func_08C4A8F8(o + 0x44, 0x20), func_08C4A7CC(o + 0x60, 4k or 0x100) and func_08C4A594(o + 0x7C, n) with n = a (0x40 when a is 0) and k = b (0x40 when b is 0; then sp[0] = 0x100). The tuning floats o[0xB8] = -9.8f-like 0xC11CCCCD, o[0x11C] = 0.5f, o[0x10C] = 0x37D1B717, o[0x110] = 0x3A9FB50F, o[0x114] = o[0x118] = 0x3DCCCCCD and the count o[0x108] = 5 follow. out[4] = the address 0x08D77338 of a function-local static (guard 0x08D77330: when its byte is 0 and func_08B6441C(guard) answers non-zero the static is set to 0x08CD79B0 and func_08B64494(guard) closes it). The tail call func_08B039F8(out, size or 0x60, 0x10).
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_08B04D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], size = ctx.gpr[5], a = ctx.gpr[6], b = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t n = a != 0u ? a : 0x40u;
    put(rt, frame.at(8u), size);
    const std::uint32_t o = call_set(rt, ctx, 0x08B62344u, {0x120u});
    put(rt, frame.at(0u), 0x100u);
    put(rt, frame.at(4u), 0x40u);
    if (b != 0u) {
        put(rt, frame.at(4u), b);
        put(rt, frame.at(0u), b << 2u);
    }
    const std::uint32_t v1 = o + 0xCu, a2 = o + 0x1Cu, a3 = o + 0x50u, t0 = o + 0x6Cu, t1 = o + 0x88u, t2 = o + 0x98u;
    const std::uint32_t s0 = o + 0x7Cu, s2 = o + 0x44u, s4 = o + 0x60u, s1 = o + 0xA8u, s3 = o + 0x2Cu, s5 = o + 0x38u;
    put16(rt, o + 0xCu, 0xFFFFFFFFu);
    put16(rt, o + 0x1Cu, 0xFFFFFFFFu);
    put(rt, o, 0u);
    put(rt, o + 4u, 0u);
    put(rt, o + 8u, 0u);
    put16(rt, v1 + 2u, 0xFFFFFFFFu);
    put(rt, v1 + 8u, o);
    put(rt, v1 + 4u, 0u);
    put16(rt, a2 + 2u, 0xFFFFFFFFu);
    put(rt, a2 + 8u, o);
    put(rt, a2 + 4u, 0u);
    put(rt, s3 + 4u, 0u);
    put(rt, s3 + 8u, 0u);
    put(rt, o + 0x2Cu, 0u);
    put(rt, o + 0x38u, 0u);
    put(rt, s5 + 4u, 0u);
    put(rt, s5 + 8u, 0u);
    put(rt, s2 + 4u, 0u);
    put(rt, s2 + 8u, 0u);
    put(rt, o + 0x44u, 0u);
    put16(rt, o + 0x50u, 0xFFFFFFFFu);
    put16(rt, a3 + 2u, 0xFFFFFFFFu);
    put(rt, a3 + 8u, s2);
    put(rt, a3 + 4u, 0u);
    put(rt, s4 + 4u, 0u);
    put(rt, s4 + 8u, 0u);
    put(rt, o + 0x60u, 0u);
    put16(rt, o + 0x6Cu, 0xFFFFFFFFu);
    put16(rt, t0 + 2u, 0xFFFFFFFFu);
    put(rt, t0 + 8u, s4);
    put(rt, t0 + 4u, 0u);
    put(rt, o + 0x7Cu, 0u);
    put(rt, s0 + 4u, 0u);
    put(rt, s0 + 8u, 0u);
    put16(rt, t1 + 2u, 0xFFFFFFFFu);
    put(rt, t1 + 8u, s0);
    put16(rt, t2 + 2u, 0xFFFFFFFFu);
    put(rt, t1 + 4u, 0u);
    put(rt, t2 + 8u, s0);
    put16(rt, o + 0x88u, 0xFFFFFFFFu);
    put(rt, t2 + 4u, 0u);
    put16(rt, o + 0x98u, 0xFFFFFFFFu);
    put(rt, o + 0xA8u, 0u);
    put(rt, s1 + 4u, 0u);
    put(rt, s1 + 8u, 0u);
    (void)call_set(rt, ctx, 0x08C4A6B0u, {o, n});
    (void)call_set(rt, ctx, 0x08C4A4ACu, {s3, n});
    (void)call_set(rt, ctx, 0x08C4A3B8u, {s1, word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x08C4A2C4u, {s5, 2u});
    (void)call_set(rt, ctx, 0x08C4A8F8u, {s2, 0x20u});
    (void)call_set(rt, ctx, 0x08C4A7CCu, {s4, word(rt, frame.at(0u))});
    (void)call_set(rt, ctx, 0x08C4A594u, {s0, n});
    const std::uint32_t v0 = o + 0xB4u;
    put(rt, o + 0xB4u, 0u);
    const std::uint32_t c8 = o + 0xC8u, d4 = o + 0xD4u, e0 = o + 0xE0u, ec = o + 0xECu;
    put_float(rt, v0 + 4u, std::bit_cast<float>(0xC11CCCCDu));
    put(rt, v0 + 8u, 0u);
    put(rt, o + 0xC0u, 0u);
    put8(rt, o + 0xC4u, 0u);
    put(rt, o + 0xC8u, 0u);
    put(rt, c8 + 4u, 0u);
    put(rt, c8 + 8u, 0u);
    put(rt, d4 + 8u, 0u);
    put(rt, d4 + 4u, 0u);
    put(rt, o + 0xD4u, 0u);
    put(rt, o + 0xE0u, 0u);
    put(rt, e0 + 8u, 0u);
    put(rt, e0 + 4u, 0u);
    put(rt, ec + 8u, 0u);
    put(rt, o + 0xECu, 0u);
    put(rt, ec + 4u, 0u);
    put_float(rt, c8 + 0x54u, 0.5f);
    put(rt, c8 + 0x40u, 5u);
    put_float(rt, c8 + 0x44u, std::bit_cast<float>(0x37D1B717u));
    put_float(rt, c8 + 0x48u, std::bit_cast<float>(0x3A9FB50Fu));
    put_float(rt, c8 + 0x50u, std::bit_cast<float>(0x3DCCCCCDu));
    put(rt, c8 + 0x30u, 0u);
    put_float(rt, c8 + 0x4Cu, std::bit_cast<float>(0x3DCCCCCDu));
    put(rt, out, o);
    constexpr std::uint32_t kGuard = 0x08D77330u, kStatic = 0x08D77338u;
    if (byte_u(rt, kGuard) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
        put(rt, kStatic, 0x08CD79B0u);
        (void)call_set(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, out + 4u, kStatic);
    const std::uint32_t saved = word(rt, frame.at(8u));
    tail_call(ctx, 0x08B039F8u, {out, saved != 0u ? saved : 0x60u, 0x10u});
}

// func_08904E08(self, p, q; f0 result): the best fractional position of the point p over the polyline records of a group. q[4] (a signed key) <= 0 asks func_08904A04(self, sp, p) with sp[0x30] = 0 and sp[0x34] = -1 first (0.0f, with nothing changed, when it answers 0; its two words go to q[4] and q[0]). The group is the record self[0x7C] + 16 * q[4]: count = its halfword [2] and start = its halfword [4] (-1.0f when the range is empty). For every index k of the range the points a = self[0x60] + 40 * self[0x64][2 * k] and b = the next one (index + 1, or 0 when it equals the halfword self[0x3C]) are tried with func_08905594(sp, p, a, b, a + 0xC, b + 0xC) and, when that answers 0, func_08905594(sp, p, a + 0xC, b + 0xC, a + 0x18, b + 0x18); an answer gives sp[8] (a distance): when |sp[8]| is below the best so far (FLT_MAX at the start) the position is f = a[0x24] + (b[0x24] (+ self[0x14] when it is below a[0x24]) - a[0x24]) * sp[0] (wrapped by subtracting self[0x14] when above it) and the best becomes |sp[8]|. f0 = the last position (-1.0f when nothing was found).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (signed_of(key) <= 0) {`; `if (call_set(rt, ctx, 0x08904A04u, {self, frame.at(0u), p}) == 0u) {`; `key = word(rt, frame.at(0x34u));`; `if (!(signed_of(start) < signed_of(end))) {`; `const std::uint32_t b = (next == limit ? 0u : next * 40u) + base;`; `if (!(f3 < f21)) continue;`; `if (g < f2) g = g + f4;`; `if (f4 < f20) f20 = f20 - f4;`.
void func_08904E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[5], q = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    std::uint32_t key = word(rt, q + 4u);
    float f20;
    if (signed_of(key) <= 0) {
        put(rt, frame.at(0x34u), 0xFFFFFFFFu);
        f20 = 0.0f;
        put(rt, frame.at(0x30u), 0u);
        if (call_set(rt, ctx, 0x08904A04u, {self, frame.at(0u), p}) == 0u) {
            ctx.fpr[0] = f20;
            return;
        }
        key = word(rt, frame.at(0x34u));
        const std::uint32_t first = word(rt, frame.at(0x30u));
        put(rt, q + 4u, key);
        put(rt, q, first);
    }
    const std::uint32_t group = word(rt, self + 0x7Cu) + (key << 4u);
    f20 = -1.0f;
    const std::uint32_t head = sext16(half_u(rt, group + 2u));
    const std::uint32_t start = sext16(half_u(rt, group + 4u));
    const std::uint32_t end = start + head;
    if (!(signed_of(start) < signed_of(end))) {
        ctx.fpr[0] = f20;
        return;
    }
    const std::uint32_t count = end - start;
    std::uint32_t cursor = start << 1u;
    float f21 = std::bit_cast<float>(0x7F7FFFFFu);
    for (std::uint32_t k = 0u; k != count; ++k, cursor += 2u) {
        const std::uint32_t base = word(rt, self + 0x60u);
        const std::uint32_t limit = half_u(rt, self + 0x3Cu);
        const std::uint32_t index = sext16(half_u(rt, cursor + word(rt, self + 0x64u)));
        const std::uint32_t a = base + index * 40u;
        const std::uint32_t next = index + 1u;
        const std::uint32_t b = (next == limit ? 0u : next * 40u) + base;
        const std::uint32_t a_c = a + 0xCu, b_c = b + 0xCu;
        float f0;
        if (call_set(rt, ctx, 0x08905594u, {frame.at(0u), p, a, b, a_c, b_c}) == 0u) {
            f0 = float_at(rt, frame.at(8u));
            if (call_set(rt, ctx, 0x08905594u, {frame.at(0u), p, a_c, b_c, a + 0x18u, b + 0x18u}) == 0u) continue;
            f0 = float_at(rt, frame.at(8u));
        } else {
            f0 = float_at(rt, frame.at(8u));
        }
        const float f3 = std::fabs(f0);
        if (!(f3 < f21)) continue;
        const float f2 = float_at(rt, a + 0x24u);
        float g = float_at(rt, b + 0x24u);
        const float f4 = float_at(rt, self + 0x14u);
        if (g < f2) g = g + f4;
        g = g - f2;
        g = psp_mul(g, float_at(rt, frame.at(0u)));
        f20 = f2 + g;
        f21 = f3;
        if (f4 < f20) f20 = f20 - f4;
    }
    ctx.fpr[0] = f20;
}

// func_08B5BB0C(): one-time initialisation of five static record groups, a no-op (v0 = its answer) when func_08B60804([0x08CB6260], 1) is non-zero. Otherwise [0x08CE0E0C] = 1 and, for the records A = 0x08D72930, B = 0x08D72908, C = 0x08D728E0 (words +4 .. +0x18 zeroed, +0x20 = the word func_08B796C4() answers at +8 / +4 / +0xC, +0x24 = -1, vtable words 0x08CDBDF0 then 0x08CDBD28 around func_08B5E0EC(0x08D7294C / 0x08D72924 / 0x08D728FC)) and the buffers D = 0x08D72A74 (vtable 0x08CDBDA8, [0x08D72A70] = 0x08CDBD94), E = 0x08D72B04 (0x08CDB188), F = 0x08D72AFC (0x08CDB174), G = 0x08D729E8 (0x08CDBDA8, [0x08D729E4] = 0x08CDBD94) and H = 0x08D7295C (0x08CDBDA8, [0x08D72958] = 0x08CDBD94): func_08B65660(buffer), its seven zeroed words (+0x70 .. +0x84, the two at +0x74 / +0x75 bytes) and func_08C54A7C(buffer, its owner record). Last, [0x08D72B74] = 0x08D72A70, [0x08D729F4] = 0x2000 and the tail call func_08B6085C([0x08CB6260], 1).
// Branches the compared cases do not take: none (the coverage run of the fast gate ran every branch and region).
void func_08B5BB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLock = 0x08CB6260u;
    const std::uint32_t answer = call_set(rt, ctx, 0x08B60804u, {word(rt, kLock), 1u});
    if (answer != 0u) return;
    put8(rt, 0x08CE0E0Cu, 1u);
    const std::uint32_t s7 = 0x08D72930u, s0 = 0x08D72908u, s1 = 0x08D728E0u, fp = 0x08D72A74u;
    const std::uint32_t s4 = 0x08CDBDF0u, s2 = 0x08CDBD28u, ind = 0xFFFFFFFFu;
    const GuestResult g0 = call_guest(rt, ctx, 0x08B796C4u, {}, {});
    take(ctx, g0);
    const std::uint32_t s6 = word(rt, g0.v0 + 8u);
    for (std::uint32_t offset = 4u; offset <= 0x18u; offset += 4u) put(rt, s7 + offset, 0u);
    put(rt, s7, s4);
    (void)call_set(rt, ctx, 0x08B5E0ECu, {0x08D7294Cu});
    put(rt, s7 + 0x20u, s6);
    put(rt, s7 + 0x24u, ind);
    put(rt, s7, s2);
    const std::uint32_t s3 = word(rt, call_set(rt, ctx, 0x08B796C4u, {}) + 4u);
    for (std::uint32_t offset = 4u; offset <= 0x18u; offset += 4u) put(rt, s0 + offset, 0u);
    put(rt, s0, s4);
    (void)call_set(rt, ctx, 0x08B5E0ECu, {0x08D72924u});
    put(rt, s0 + 0x20u, s3);
    put(rt, s0 + 0x24u, ind);
    put(rt, s0, s2);
    const std::uint32_t last = word(rt, call_set(rt, ctx, 0x08B796C4u, {}) + 0xCu);
    for (std::uint32_t offset = 4u; offset <= 0x18u; offset += 4u) put(rt, s1 + offset, 0u);
    put(rt, s1, s4);
    (void)call_set(rt, ctx, 0x08B5E0ECu, {0x08D728FCu});
    put(rt, s1 + 0x24u, ind);
    put(rt, s1 + 0x20u, last);
    put(rt, s1, s2);
    (void)call_set(rt, ctx, 0x08B65660u, {fp});
    const auto clear = [&](std::uint32_t buffer) {
        put(rt, buffer + 0x70u, 0u);
        put8(rt, buffer + 0x74u, 0u);
        put8(rt, buffer + 0x75u, 0u);
        put(rt, buffer + 0x78u, 0u);
        put(rt, buffer + 0x7Cu, 0u);
        put(rt, buffer + 0x80u, 0u);
        put(rt, buffer + 0x84u, 0u);
    };
    put(rt, 0x08D72A70u, 0x08CDBD94u);
    clear(fp);
    put(rt, fp, 0x08CDBDA8u);
    (void)call_set(rt, ctx, 0x08C54A7Cu, {fp, s7});
    constexpr std::uint32_t kE = 0x08D72B04u;
    (void)call_set(rt, ctx, 0x08B65660u, {kE});
    put(rt, kE, 0x08CDB188u);
    put(rt, 0x08D72AFCu, 0x08CDB174u);
    put(rt, 0x08D72AFCu + 4u, 0u);
    clear(kE);
    (void)call_set(rt, ctx, 0x08C54A7Cu, {kE, s0});
    constexpr std::uint32_t kG = 0x08D729E8u;
    (void)call_set(rt, ctx, 0x08B65660u, {kG});
    put(rt, kG, 0x08CDBDA8u);
    put(rt, 0x08D729E4u, 0x08CDBD94u);
    clear(kG);
    (void)call_set(rt, ctx, 0x08C54A7Cu, {kG, s1});
    constexpr std::uint32_t kH = 0x08D7295Cu;
    (void)call_set(rt, ctx, 0x08B65660u, {kH});
    put(rt, 0x08D72958u, 0x08CDBD94u);
    clear(kH);
    put(rt, kH, 0x08CDBDA8u);
    (void)call_set(rt, ctx, 0x08C54A7Cu, {kH, s1});
    put(rt, 0x08D72AFCu + 0x78u, 0x08D72A70u);
    put(rt, 0x08D729E4u + 0x10u, 0x2000u);
    tail_call(ctx, 0x08B6085Cu, {word(rt, kLock), 1u});
}

// func_08C4D4EC(obj; f12 = x): stores the float x into the typed slot obj[4] (a pointer) according to the type id obj[0]; v0 = 1 when a store was made, 0 otherwise. Each id comes from a guest getter and is tested in this order (the pointer must not be null): func_08B9DDB0 float (the float x), func_08C4D18C (the 64-bit result of func_08B70F28(x) as two words), func_08BC473C byte (trunc(x)), func_08BC48B4 unsigned byte, func_08BC45C4 halfword (trunc(x)), func_08BC444C unsigned halfword, func_08BC42D4 word (trunc(x)), func_08B9E8D4 unsigned word, func_08BC415C (the 64-bit result of func_08B6F4D0(x)), func_08BC3FE4 (func_08B6F37C(x)), func_08B9CEBC byte (x != 0.0f), func_08BC4A2C byte (x != 0.0f). The unsigned ones add 0x80000000 to trunc(x - 2147483648.0f) when 2147483648.0f <= x, else take trunc(x) (saturating like the PSP's trunc.w.s).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `const auto truncated = [&](float v) { return ctx.fpu_float_to_word(v, 1u); };`; `const auto unsigned_of = [&](float v) {`; `if (std::bit_cast<float>(0x4F000000u) <= v) w = truncated(v - std::bit_cast<float>(0x4F000000u)) | 0x80000000u;`; `if (matches(0x08B9DDB0u)) {`; `if (p != 0u) {`; `if (matches(0x08C4D18Cu)) {`; `if (matches(0x08BC473Cu)) {`; `if (matches(0x08BC48B4u)) {`; `if (matches(0x08BC45C4u)) {`; `if (matches(0x08BC444Cu)) {`; `if (matches(0x08BC42D4u)) {`; `if (matches(0x08B9E8D4u)) {`; `if (matches(0x08BC415Cu)) {`; `if (matches(0x08BC3FE4u)) {`; `if (matches(0x08B9CEBCu)) {`; `put8(rt, p, x == 0.0f ? 0u : 1u);`; `if (matches(0x08BC4A2Cu)) {`; `ctx.gpr[2] = 0u;`.
void func_08C4D4EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    const float x = ctx.fpr[12];
    const auto truncated = [&](float v) { return ctx.fpu_float_to_word(v, 1u); };
    const auto unsigned_of = [&](float v) {
        std::uint32_t w = truncated(v);
        if (std::bit_cast<float>(0x4F000000u) <= v) w = truncated(v - std::bit_cast<float>(0x4F000000u)) | 0x80000000u;
        return w;
    };
    const auto matches = [&](std::uint32_t getter) {
        const std::uint32_t id = word(rt, obj);
        return call_set(rt, ctx, getter, {}) == id;
    };
    const auto done = [&]() { ctx.gpr[2] = 1u; };
    if (matches(0x08B9DDB0u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put_float(rt, p, x);
            done();
            return;
        }
    }
    if (matches(0x08C4D18Cu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            const GuestResult r = call_setf(rt, ctx, 0x08B70F28u, {}, {x});
            put(rt, p, r.v0);
            put(rt, p + 4u, r.v1);
            done();
            return;
        }
    }
    if (matches(0x08BC473Cu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put8(rt, p, truncated(x));
            done();
            return;
        }
    }
    if (matches(0x08BC48B4u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put8(rt, p, unsigned_of(x));
            done();
            return;
        }
    }
    if (matches(0x08BC45C4u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put16(rt, p, truncated(x));
            done();
            return;
        }
    }
    if (matches(0x08BC444Cu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put16(rt, p, unsigned_of(x));
            done();
            return;
        }
    }
    if (matches(0x08BC42D4u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put(rt, p, truncated(x));
            done();
            return;
        }
    }
    if (matches(0x08B9E8D4u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put(rt, p, unsigned_of(x));
            done();
            return;
        }
    }
    if (matches(0x08BC415Cu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            const GuestResult r = call_setf(rt, ctx, 0x08B6F4D0u, {}, {x});
            put(rt, p, r.v0);
            put(rt, p + 4u, r.v1);
            done();
            return;
        }
    }
    if (matches(0x08BC3FE4u)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            const GuestResult r = call_setf(rt, ctx, 0x08B6F37Cu, {}, {x});
            put(rt, p, r.v0);
            put(rt, p + 4u, r.v1);
            done();
            return;
        }
    }
    if (matches(0x08B9CEBCu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put8(rt, p, x == 0.0f ? 0u : 1u);
            done();
            return;
        }
    }
    if (matches(0x08BC4A2Cu)) {
        const std::uint32_t p = word(rt, obj + 4u);
        if (p != 0u) {
            put8(rt, p, x == 0.0f ? 0u : 1u);
            done();
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

// func_0889AD18(self, op): a switch over the command id `op` (0 .. 0x45) on t = self[4][0xB04] (v0 = 0 when t is null). t is an object with a vtable. The groups of ids (from the jump table at 0x08C89B10): 0: 1 when slot 0x64(t) > 0.1f (0x3DCCCCCD) else 0; 1-4: func_08B53844(slot 0xA4(t, op - 1)); 5: func_08B53844(slot 0x5C(t)); 6: func_08B53844(slot 0x9C(t, 0)); 7: v0 = self[4][0x74]; 8: func_08B55FB0(slot 0x60(t) * 0x42652EE0); 9: func_08B566BC(self[4] + 0x8A0); 11-14: func_0889AB88(t, op - 0xB, p) with p = self[4][0x58] ? that[0x10] : 0; 15-18 (k = op - 0xF, r = self[4] + 0xD4 * k + 0x70): func_08899D84(t, k), func_08B5606C(180.0f) when k is odd, r[8 + 0xD0] = 1, func_08B56A00(3), func_08B568DC(r + 0x94), func_08B566BC(r + 0x14), func_08B56A00(0); 20-35: func_08B566BC(self[4] + 0x4A0 + 64 * (op - 0x14)); 36-39 (e = self[4] + 0xD4 * op - 0x1D58): e[0xCC] = 1 when ((func_08B587EC() ^ 2) == 0) equals e[0xD2] (as a bool xor), else 2, and e[0xD1] = 1; 42-45: func_08B53844(slot 0x9C(t, op - 0x2A)); 52 / 53: func_08B53844(slot 0x68 / 0x6C(t)); 56 / 57: func_08B53844(slot 0x74(t, op - 0x38)); 67-69 (s = self[4] + 0xA4 * op - 0x21FC): nothing (0) when the byte s[0x28] is 0, else func_08B56128(s[0x4C]), func_08B5606C(s[0x50]), (nothing more, 0, when self[4][0x68] is null) func_08AF8590(self[4][0x68], s + 0x58, s[4]), func_08B569A0(), func_08B568DC(s + 0x58), s[0x54] = func_08AF8750(self[4][0x68][0x38] + 48 * s[0]) after which func_08B56940() runs, and v0 = (s[0x54] == 0). The other ids answer 0.
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.
void func_0889AD18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], op = ctx.gpr[5];
    const std::uint32_t a2 = word(rt, self + 4u);
    const std::uint32_t t = word(rt, a2 + 0xB04u);
    ctx.gpr[2] = 0u;
    if (t == 0u) return;
    const auto slot = [&](std::uint32_t offset) { return word(rt, word(rt, t) + offset); };
    const auto show = [&](float f0) { (void)call_setf(rt, ctx, 0x08B53844u, {}, {f0}); };
    const auto finish = [&](std::uint32_t value) { ctx.gpr[2] = value; };
    if (op >= 0x46u) {
        finish(0u);
        return;
    }
    switch (op) {
    case 0: {
        const GuestResult r = call_guest(rt, ctx, slot(0x64u), {t}, {});
        finish((std::bit_cast<float>(0x3DCCCCCDu) < r.f0) ? 1u : 0u);
        return;
    }
    case 1: case 2: case 3: case 4: {
        const GuestResult r = call_guest(rt, ctx, slot(0xA4u), {t, op - 1u}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 5: {
        const GuestResult r = call_guest(rt, ctx, slot(0x5Cu), {t}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 6: {
        const GuestResult r = call_guest(rt, ctx, slot(0x9Cu), {t, 0u}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 42: case 43: case 44: case 45: {
        const GuestResult r = call_guest(rt, ctx, slot(0x9Cu), {t, op - 0x2Au}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 7:
        finish(word(rt, a2 + 0x74u));
        return;
    case 8: {
        const GuestResult r = call_guest(rt, ctx, slot(0x60u), {t}, {});
        (void)call_setf(rt, ctx, 0x08B55FB0u, {}, {psp_mul(r.f0, std::bit_cast<float>(0x42652EE0u))});
        finish(0u);
        return;
    }
    case 9:
        (void)call_set(rt, ctx, 0x08B566BCu, {a2 + 0x8A0u});
        finish(0u);
        return;
    case 11: case 12: case 13: case 14: {
        const std::uint32_t o = word(rt, a2 + 0x58u);
        const std::uint32_t third = o == 0u ? 0u : word(rt, o + 0x10u);
        (void)call_set(rt, ctx, 0x0889AB88u, {t, op - 0xBu, third});
        finish(0u);
        return;
    }
    case 15: case 16: case 17: case 18: {
        const std::uint32_t k = op - 0xFu;
        const std::uint32_t r = a2 + k * 0xD4u + 0x70u;
        (void)call_set(rt, ctx, 0x08899D84u, {t, k});
        if ((k & 1u) != 0u) (void)call_setf(rt, ctx, 0x08B5606Cu, {}, {std::bit_cast<float>(0x43340000u)});
        put8(rt, r + 8u + 0xD0u, 1u);
        (void)call_set(rt, ctx, 0x08B56A00u, {3u});
        (void)call_set(rt, ctx, 0x08B568DCu, {r + 0x94u});
        (void)call_set(rt, ctx, 0x08B566BCu, {r + 0x14u});
        (void)call_set(rt, ctx, 0x08B56A00u, {0u});
        finish(0u);
        return;
    }
    case 36: case 37: case 38: case 39: {
        const std::uint32_t e = a2 + op * 0xD4u - 0x1D58u;
        const std::uint32_t lit = byte_u(rt, e + 0xD2u);
        const std::uint32_t r = call_set(rt, ctx, 0x08B587ECu, {});
        std::uint32_t flag = ((r ^ 2u) == 0u) ? 1u : 0u;
        flag ^= lit;
        put(rt, e + 0xCCu, flag == 0u ? 1u : 2u);
        put8(rt, e + 0xD1u, 1u);
        finish(0u);
        return;
    }
    case 52: case 53: {
        const GuestResult r = call_guest(rt, ctx, slot(op == 0x34u ? 0x68u : 0x6Cu), {t}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 56: case 57: {
        const GuestResult r = call_guest(rt, ctx, slot(0x74u), {t, op - 0x38u}, {});
        show(r.f0);
        finish(0u);
        return;
    }
    case 67: case 68: case 69: {
        const std::uint32_t s = a2 + op * 0xA4u - 0x21FCu;
        if (byte_u(rt, s + 0x28u) == 0u) {
            finish(0u);
            return;
        }
        (void)call_setf(rt, ctx, 0x08B56128u, {}, {float_at(rt, s + 0x4Cu)});
        (void)call_setf(rt, ctx, 0x08B5606Cu, {}, {float_at(rt, s + 0x50u)});
        const std::uint32_t group = word(rt, word(rt, self + 4u) + 0x68u);
        if (group == 0u) {
            finish(0u);
            return;
        }
        (void)call_set(rt, ctx, 0x08AF8590u, {group, s + 0x58u, word(rt, s + 4u)});
        (void)call_set(rt, ctx, 0x08B569A0u, {});
        (void)call_set(rt, ctx, 0x08B568DCu, {s + 0x58u});
        const std::uint32_t base = word(rt, word(rt, self + 4u) + 0x68u);
        const std::uint32_t entry = word(rt, base + 0x38u) + word(rt, s) * 48u;
        const std::uint32_t found = call_set(rt, ctx, 0x08AF8750u, {entry});
        put8(rt, s + 0x54u, found);
        (void)call_set(rt, ctx, 0x08B56940u, {});
        finish(byte_u(rt, s + 0x54u) != 0u ? 0u : 1u);
        return;
    }
    default:
        if (op - 0x14u < 0x10u) (void)call_set(rt, ctx, 0x08B566BCu, {a2 + 0x4A0u + ((op - 0x14u) << 6u)});
        finish(0u);
        return;
    }
}

// func_0895E3F4(obj): a menu-cursor step. r = obj[8] (a record), q = obj[0xC]; wait = 1 when the halfword r[0x50C] is not 3, else 0xC. When the byte r[0xE] is set: the tail call func_0895DEC8(obj, 0). Otherwise by the state byte r[4] (the jump table at 0x08C8B548): 0: func_0895DEC8(obj, 0); 2: func_0895DEC8(obj, 1); other than 0 .. 6: v0 = -1; 1, 3, 4, 5 and 6 answer the next item index (or -1) and may set the countdown r[0x512] = wait (r[0x511] is the signed direction, r[0x5F8] the current item, q[0x280] the item count; a nonzero countdown is decremented by one and the answer is -1; when it reaches 0 and the direction (state 1 and 5: the signed byte r[0x511], state 3, 4 and 6: the halfword r[0x510]) is not 0 it is set to 1): state 1 first answers 0 / -1 for a signed byte r[0x510] != 0 (0 when r[0x5F8] != 0) and asks func_0895DD80(obj, 0; f12 = r[0x6C4]) when r[0x5F8] is 0 and the float r[0x530] is above 0.0f (its answer, or 1 when it is 0).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `const std::uint32_t wait = (half_u(rt, r + 0x50Cu) ^ 3u) != 0u ? 1u : 0xCu;`; `if (byte_u(rt, r + 0xEu) != 0u) {`; `if (state >= 7u) {`; `const auto answer = [&](std::uint32_t value) { ctx.gpr[2] = value; };`; `const auto countdown = [&](bool rearm_when_zero) {`; `if (timer == 0u && rearm_when_zero) put8(rt, r + 0x512u, 1u);`; `if (state == 0u) {`; `if (state == 2u) {`; `if (state == 1u) {`; `if (sext8(byte_u(rt, r + 0x510u)) != 0u) {`; `answer(item != 0u ? 0u : 0xFFFFFFFFu);`; `if (item == 0u) {`; `if (0.0f < float_at(rt, r + 0x530u)) {`; `answer(now.v0 != 0u ? now.v0 : 1u);`; `if (byte_u(rt, r + 0x512u) != 0u) {`; `const std::uint32_t direction = sext8(byte_u(rt, r + 0x511u));`; `if (signed_of(direction) > 0 && item < byte_u(rt, q + 0x280u)) {`; `if (signed_of(direction) >= 0 || item < 2u) {`; `put8(rt, r + 0x512u, wait);`; `const std::uint32_t item = byte_u(rt, r + 0x5F8u);`; `const bool rearm = state == 5u ? sext8(byte_u(rt, r + 0x511u)) != 0u : half_u(rt, r + 0x510u) != 0u;`; `if (state != 5u && sext8(byte_u(rt, r + 0x510u)) != 0u) {`; `if (signed_of(direction) >= 0) {`; `if (state == 5u ? item == 0u : item < 2u) {`.
void func_0895E3F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    const std::uint32_t r = word(rt, obj + 8u);
    const std::uint32_t wait = (half_u(rt, r + 0x50Cu) ^ 3u) != 0u ? 1u : 0xCu;
    if (byte_u(rt, r + 0xEu) != 0u) {
        tail_call(ctx, 0x0895DEC8u, {obj, 0u});
        return;
    }
    const std::uint32_t state = byte_u(rt, r + 4u);
    if (state >= 7u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const auto answer = [&](std::uint32_t value) { ctx.gpr[2] = value; };
    // The countdown the states 1, 3-6 share: a non-zero r[0x512] is decremented (answer -1); when it reaches 0 and `rearm` holds it becomes 1.
    const auto countdown = [&](bool rearm_when_zero) {
        const std::uint32_t timer = (byte_u(rt, r + 0x512u) - 1u) & 0xFFu;
        put8(rt, r + 0x512u, timer);
        if (timer == 0u && rearm_when_zero) put8(rt, r + 0x512u, 1u);
        answer(0xFFFFFFFFu);
    };
    const std::uint32_t q = word(rt, obj + 0xCu);
    if (state == 0u) {
        tail_call(ctx, 0x0895DEC8u, {obj, 0u});
        return;
    }
    if (state == 2u) {
        tail_call(ctx, 0x0895DEC8u, {obj, 1u});
        return;
    }
    if (state == 1u) {
        const std::uint32_t item = byte_u(rt, r + 0x5F8u);
        if (sext8(byte_u(rt, r + 0x510u)) != 0u) {
            answer(item != 0u ? 0u : 0xFFFFFFFFu);
            return;
        }
        if (item == 0u) {
            if (0.0f < float_at(rt, r + 0x530u)) {
                const GuestResult now = call_setf(rt, ctx, 0x0895DD80u, {obj, 0u}, {float_at(rt, r + 0x6C4u)});
                answer(now.v0 != 0u ? now.v0 : 1u);
                return;
            }
        }
        if (byte_u(rt, r + 0x512u) != 0u) {
            countdown(sext8(byte_u(rt, r + 0x511u)) != 0u);
            return;
        }
        const std::uint32_t direction = sext8(byte_u(rt, r + 0x511u));
        if (signed_of(direction) > 0 && item < byte_u(rt, q + 0x280u)) {
            put8(rt, r + 0x512u, wait);
            answer(item + 1u);
            return;
        }
        if (signed_of(direction) >= 0 || item < 2u) {
            answer(0xFFFFFFFFu);
            return;
        }
        put8(rt, r + 0x512u, wait);
        answer(item - 1u);
        return;
    }
    const std::uint32_t item = byte_u(rt, r + 0x5F8u);
    if (byte_u(rt, r + 0x512u) != 0u) {
        const bool rearm = state == 5u ? sext8(byte_u(rt, r + 0x511u)) != 0u : half_u(rt, r + 0x510u) != 0u;
        countdown(rearm);
        return;
    }
    if (state != 5u && sext8(byte_u(rt, r + 0x510u)) != 0u) {
        if (item == 0u) {
            answer(0xFFFFFFFFu);
            return;
        }
        put8(rt, r + 0x512u, wait);
        answer(0u);
        return;
    }
    const std::uint32_t direction = sext8(byte_u(rt, r + 0x511u));
    if (signed_of(direction) > 0 && item < byte_u(rt, q + 0x280u)) {
        put8(rt, r + 0x512u, wait);
        answer(item + 1u);
        return;
    }
    if (signed_of(direction) >= 0) {
        answer(0xFFFFFFFFu);
        return;
    }
    if (state == 5u ? item == 0u : item < 2u) {
        answer(0xFFFFFFFFu);
        return;
    }
    put8(rt, r + 0x512u, wait);
    answer(item - 1u);
}

// func_089491A0(self; f12 = t; f0 result): per-frame smoothing of a gauge. s = self[8] (the state record), r = self[0xC] + 0x170, k = func_0895549C(self) (f22). When the float r[0xFC] equals 0.0f: s[0x584] = s[0x59C] = t and the byte s[0x736] = 0, then the last step with f12 = t. Otherwise m = func_0894C240(r, s.h[0x58E], s.h[0x58E], sp) and, with d = s.h[0x58E] * s[0x584] / m, the two halfwords s.h[0x588 + 2 * i] move by trunc(k * (d - s.h[0x588 + 2 * i]) * r[0xEC + 4 * i]) (clamped to 0 when negative; skipped when r[0xE4 + 4 * i] is 0.0f), for i = 0, 1. Then q = t * func_0894C240(r, s.h[0x588], s.h[0x58A], sp), g = max(s.h[0x58E] * 2^-12 * -0.25f + 0.5f, 0.0625f), s[0x5A0] = 0 and s[0x5A4] = (q + g * (1.0f - t) + s[0x5A4]) * 0.5f; by the byte s[0x57B]: 0 sets it to 1 and s[0x5A0] = max(s[0x584] - 0.8f, 0.0f) when t < 0.75f, 1 clears it when 0.9f < t. s[0x584] = q, f12 = 1.0f (q / m when q is below m), s.h[0x58C] = max(s.h[0x588] (unsigned), s.h[0x58A]). The last step: f3 = func_08949128(self; f12); when r[0xFC] is above 0.0f and both s[0x59C] - 0.75f and f3 are above 0.0f the byte s[0x736] = clamp(trunc(x * x * 128.0f)) (1 .. 0x7F, x = s[0x59C] - 0.75f, times f3 * 0x3ADEE95C unless f3 is 588.0f or more); with either not above 0.0f the byte is 1; s[0x5A8] = f3; f0 = f3.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (signed_of(h) < 0) put16(rt, a0 + 0x588u, 0u);`; `if (g < floor_f) g = floor_f;`; `if (f1 < 0.0f) f1 = 0.0f;`; `else if (mode == 1u) {`; `if (std::bit_cast<float>(0x3F666666u) < t) put8(rt, s0 + 0x57Bu, 0u);`; `if (signed_of(low) < signed_of(high)) put16(rt, s0 + 0x58Cu, high);`; `if (f1 <= 0.0f || f3 <= 0.0f) {`; `if (!(std::bit_cast<float>(0x44130000u) <= f3)) {`; `if (signed_of(a0) < 0x80) a1 = signed_of(a0) > 0 ? (a0 & 0xFFu) : 1u;`.
void func_089491A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t s0 = word(rt, self + 8u);
    const std::uint32_t s2 = word(rt, self + 0xCu) + 0x170u;
    const float f22 = call_setf(rt, ctx, 0x0895549Cu, {self}, {}).f0;
    float f12;
    float f23 = 0.0f, f3 = 0.0f;
    if (float_at(rt, s2 + 0xFCu) == 0.0f) {
        put_float(rt, s0 + 0x584u, t);
        put8(rt, s0 + 0x736u, 0u);
        put_float(rt, s0 + 0x59Cu, t);
        f12 = t;
    } else {
        const std::uint32_t a1 = sext16(half_u(rt, s0 + 0x58Eu));
        f23 = call_setf(rt, ctx, 0x0894C240u, {s2, a1, a1, frame.at(0u)}, {}).f0;
        const std::uint32_t v0 = sext16(half_u(rt, s0 + 0x58Eu));
        const float f2_in = float_at(rt, s0 + 0x584u);
        put(rt, s0 + 0x59Cu, word(rt, s0 + 0x584u));
        float f0 = static_cast<float>(signed_of(v0));
        f0 = psp_mul(f0, f2_in);
        const float f2 = f0 / f23;
        for (std::uint32_t i = 0u; i != 2u; ++i) {
            const std::uint32_t v1 = s2 + 4u * i;
            const std::uint32_t a0 = s0 + 2u * i;
            if (float_at(rt, v1 + 0xE4u) == 0.0f) continue;
            std::uint32_t h = sext16(half_u(rt, a0 + 0x588u));
            const float f1 = float_at(rt, v1 + 0xECu);
            float g = f2 - static_cast<float>(signed_of(h));
            g = psp_mul(g, f1);
            g = psp_mul(f22, g);
            const std::uint32_t moved = ctx.fpu_float_to_word(g, 1u);
            h = sext16(h + moved);
            put16(rt, a0 + 0x588u, h);
            if (signed_of(h) < 0) put16(rt, a0 + 0x588u, 0u);
        }
        const std::uint32_t b1 = sext16(half_u(rt, s0 + 0x588u));
        const std::uint32_t b2 = sext16(half_u(rt, s0 + 0x58Au));
        const float next = call_setf(rt, ctx, 0x0894C240u, {s2, b1, b2, frame.at(0u)}, {}).f0;
        const std::uint32_t v = sext16(half_u(rt, s0 + 0x58Eu));
        f3 = psp_mul(t, next);
        const float half_f = 0.5f, floor_f = std::bit_cast<float>(0x3D800000u);
        float g = static_cast<float>(signed_of(v));
        g = psp_mul(g, std::bit_cast<float>(0x39800000u));
        g = psp_mul(g, std::bit_cast<float>(0xBE800000u));
        g = g + half_f;
        if (g < floor_f) g = floor_f;
        float f0b = 1.0f - t;
        const float prev = float_at(rt, s0 + 0x5A4u);
        const std::uint32_t mode = byte_u(rt, s0 + 0x57Bu);
        put(rt, s0 + 0x5A0u, 0u);
        f0b = psp_mul(g, f0b);
        f0b = f3 + f0b;
        f0b = f0b + prev;
        f0b = psp_mul(f0b, half_f);
        put_float(rt, s0 + 0x5A4u, f0b);
        if (mode == 0u) {
            if (t < std::bit_cast<float>(0x3F400000u)) {
                float f1 = float_at(rt, s0 + 0x584u) - std::bit_cast<float>(0x3F4CCCCDu);
                if (f1 < 0.0f) f1 = 0.0f;
                put_float(rt, s0 + 0x5A0u, f1);
                put8(rt, s0 + 0x57Bu, 1u);
            }
        } else if (mode == 1u) {
            if (std::bit_cast<float>(0x3F666666u) < t) put8(rt, s0 + 0x57Bu, 0u);
        }
        put_float(rt, s0 + 0x584u, f3);
        f12 = 1.0f;
        if (!(f23 <= f3)) f12 = f3 / f23;
        const std::uint32_t low = half_u(rt, s0 + 0x588u);
        const std::uint32_t high = sext16(half_u(rt, s0 + 0x58Au));
        put16(rt, s0 + 0x58Cu, low);
        if (signed_of(low) < signed_of(high)) put16(rt, s0 + 0x58Cu, high);
    }
    f3 = call_setf(rt, ctx, 0x08949128u, {self}, {f12}).f0;
    if (!(0.0f < float_at(rt, s2 + 0xFCu))) {
        put_float(rt, s0 + 0x5A8u, f3);
        ctx.fpr[0] = f3;
        return;
    }
    float f1 = float_at(rt, s0 + 0x59Cu) - std::bit_cast<float>(0x3F400000u);
    if (f1 <= 0.0f || f3 <= 0.0f) {
        put8(rt, s0 + 0x736u, 1u);
    } else {
        float f0 = f1;
        if (!(std::bit_cast<float>(0x44130000u) <= f3)) {
            f1 = psp_mul(f3, f1);
            f0 = psp_mul(f1, std::bit_cast<float>(0x3ADEE95Cu));
        }
        f0 = psp_mul(f0, f0);
        f0 = psp_mul(f0, std::bit_cast<float>(0x43000000u));
        const std::uint32_t a0 = ctx.fpu_float_to_word(f0, 1u);
        std::uint32_t a1 = 0x7Fu;
        if (signed_of(a0) < 0x80) a1 = signed_of(a0) > 0 ? (a0 & 0xFFu) : 1u;
        put8(rt, s0 + 0x736u, a1);
    }
    put_float(rt, s0 + 0x5A8u, f3);
    ctx.fpr[0] = f3;
}

// func_0897D118(self): the per-frame handler of a held button. Nothing (v0 = the byte) when the byte self[0x1B4] is set. self[0x1B0] += 1; o = func_08977260(self) and k = func_089AAC8C(o); nothing more (v0 = o[0x6E], 0 or the byte) when the byte o[0x6E] or k is 0. f = o[0x70]; func_089AA95C(o, sp + 0x14, sp + 0x18). pressed (s5) = 0; bit 2 of f: s5 = func_0897CE9C(self, 0, o, k) != 0. Bit 0 (s1): when func_089A8FF4(o) answers 0, w = func_08977270(self, 0) and self[0x1B0] = 0 when (w & 0x30000) is set, s5 = func_0897C698(self, 0, o, k) != 0; else s5 = 1 when func_089A8FFC(o, 0) answers non-zero. func_089AA95C(o, sp + 0x1C, sp + 0x20) (the pair (x, y)); bit 4 of f with trunc(sp[0x14]) != trunc(x): n = func_089AA544(o) (else func_089AA5A0(o)) non-zero gives func_089C3C04(sp + 0x10, o, n; f12 = x, f13 = y), func_0897783C(self, sp + 0x10), func_08A5B9EC(sp + 0x10) and s5 = 1. With bit 0 set: w = func_08977270(self, 0); the counters self[0x164] / the state byte self[0x160] (0, 1, 2) / self[0x168] / self[0x16C] advance: s5 or no bit 0x400000 of w resets self[0x164] = 0 (and the state 0 / 1 -> 0), a set bit 0x400000 adds one to it; state 0 becomes 1 when self[0x164] is above self[0x168]; state 1 becomes 2 when self[0x164] - self[0x168] is above self[0x16C] and allocates an object (func_08A5BC20(0x40, 4) built by func_089D6378(obj, o, k, 0xFFD6, 0x10, 0; f12 = x, f13 = y), vtable 0x08CC9758, byte [0x3C] = 0, wrapped by func_089D5E8C and handed to func_0897783C(self, ..), func_08A5B9EC); state 2 with self[0x164] below self[0x168] returns it to 0 and does the same with a 0x3C-byte object (vtable 0x08CC9888). Finally self[0x1B0] = 0 when s5 and v0 = func_089B0950(o).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (pressed_byte != 0u) return;`; `if (active == 0u) return;`; `if ((flags & 4u) != 0u) {`; `s5 = call_set(rt, ctx, 0x0897CE9Cu, {self, 0u, s3, s4}) != 0u ? 1u : 0u;`; `if (s1 != 0u) {`; `if (call_set(rt, ctx, 0x089A8FF4u, {s3}) == 0u) {`; `if ((w & 0x30000u) != 0u) put(rt, self + 0x1B0u, 0u);`; `if (call_set(rt, ctx, 0x0897C698u, {self, 0u, s3, s4}) != 0u) s5 = 1u;`; `else if (call_set(rt, ctx, 0x089A8FFCu, {s3, 0u}) != 0u) {`; `if ((flags & 0x10u) != 0u) {`; `if (before != after) {`; `if (n == 0u) n = call_set(rt, ctx, 0x089AA5A0u, {s3});`; `if (n != 0u) {`; `const bool counting = s5 == 0u && (w & 0x400000u) != 0u;`; `if (counting) {`; `if (state < 2u) {`; `if (check_first && word(rt, self + 0x168u) < word(rt, self + 0x164u)) {`; `if (state == 1u && word(rt, self + 0x16Cu) < word(rt, self + 0x164u) - word(rt, self + 0x168u)) {`; `if (state == 2u && word(rt, self + 0x164u) < word(rt, self + 0x168u)) {`; `if (s5 != 0u) put(rt, self + 0x1B0u, 0u);`.
void func_0897D118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t pressed_byte = byte_u(rt, self + 0x1B4u);
    ctx.gpr[2] = pressed_byte;
    if (pressed_byte != 0u) return;
    put(rt, self + 0x1B0u, word(rt, self + 0x1B0u) + 1u);
    const std::uint32_t s3 = call_set(rt, ctx, 0x08977260u, {self});
    const std::uint32_t s4 = call_set(rt, ctx, 0x089AAC8Cu, {s3});
    const std::uint32_t active = byte_u(rt, s3 + 0x6Eu);
    ctx.gpr[2] = active;
    if (active == 0u) return;
    if (s4 == 0u) return;
    const std::uint32_t flags = word(rt, s3 + 0x70u);
    (void)call_set(rt, ctx, 0x089AA95Cu, {s3, frame.at(0x14u), frame.at(0x18u)});
    std::uint32_t s5 = 0u;
    if ((flags & 4u) != 0u) {
        s5 = call_set(rt, ctx, 0x0897CE9Cu, {self, 0u, s3, s4}) != 0u ? 1u : 0u;
    }
    const std::uint32_t s1 = flags & 1u;
    if (s1 != 0u) {
        if (call_set(rt, ctx, 0x089A8FF4u, {s3}) == 0u) {
            const std::uint32_t w = call_set(rt, ctx, 0x08977270u, {self, 0u});
            if ((w & 0x30000u) != 0u) put(rt, self + 0x1B0u, 0u);
            if (call_set(rt, ctx, 0x0897C698u, {self, 0u, s3, s4}) != 0u) s5 = 1u;
        } else if (call_set(rt, ctx, 0x089A8FFCu, {s3, 0u}) != 0u) {
            s5 = 1u;
        }
    }
    (void)call_set(rt, ctx, 0x089AA95Cu, {s3, frame.at(0x1Cu), frame.at(0x20u)});
    if ((flags & 0x10u) != 0u) {
        const std::uint32_t before = ctx.fpu_float_to_word(float_at(rt, frame.at(0x14u)), 1u);
        const std::uint32_t after = ctx.fpu_float_to_word(float_at(rt, frame.at(0x1Cu)), 1u);
        if (before != after) {
            std::uint32_t n = call_set(rt, ctx, 0x089AA544u, {s3});
            if (n == 0u) n = call_set(rt, ctx, 0x089AA5A0u, {s3});
            if (n != 0u) {
                const std::uint32_t temp = frame.at(0x10u);
                (void)call_setf(rt, ctx, 0x089C3C04u, {temp, s3, n}, {float_at(rt, frame.at(0x1Cu)), float_at(rt, frame.at(0x20u))});
                (void)call_set(rt, ctx, 0x0897783Cu, {self, temp});
                (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
                s5 = 1u;
            }
        }
    }
    if (s1 != 0u) {
        const std::uint32_t w = call_set(rt, ctx, 0x08977270u, {self, 0u});
        std::uint32_t state = byte_u(rt, self + 0x160u);
        const bool counting = s5 == 0u && (w & 0x400000u) != 0u;
        bool check_first = false;
        if (counting) {
            put(rt, self + 0x164u, word(rt, self + 0x164u) + 1u);
            check_first = state == 0u;
        } else {
            put(rt, self + 0x164u, 0u);
            if (state < 2u) {
                state = 0u;
                put8(rt, self + 0x160u, 0u);
                check_first = true;
            }
        }
        if (check_first && word(rt, self + 0x168u) < word(rt, self + 0x164u)) {
            state = 1u;
            put8(rt, self + 0x160u, 1u);
        }
        if (state == 1u && word(rt, self + 0x16Cu) < word(rt, self + 0x164u) - word(rt, self + 0x168u)) {
            const float f20 = float_at(rt, frame.at(0x1Cu)), f21 = float_at(rt, frame.at(0x20u));
            put8(rt, self + 0x160u, 2u);
            const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x40u, 4u});
            (void)call_setf(rt, ctx, 0x089D6378u, {object, s3, s4, 0xFFD6u, 0x10u, 0u}, {f20, f21});
            put(rt, object, 0x08CC9758u);
            put8(rt, object + 0x3Cu, 0u);
            put(rt, frame.at(4u), object);
            (void)call_set(rt, ctx, 0x089D5E8Cu, {frame.at(0xCu), frame.at(4u)});
            (void)call_set(rt, ctx, 0x0897783Cu, {self, frame.at(0xCu)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
            state = byte_u(rt, self + 0x160u);
        }
        if (state == 2u && word(rt, self + 0x164u) < word(rt, self + 0x168u)) {
            const float f20 = float_at(rt, frame.at(0x1Cu)), f21 = float_at(rt, frame.at(0x20u));
            put8(rt, self + 0x160u, 0u);
            const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x3Cu, 4u});
            (void)call_setf(rt, ctx, 0x089D6378u, {object, s3, s4, 0xFFD6u, 0x10u, 0u}, {f20, f21});
            put(rt, object, 0x08CC9888u);
            put(rt, frame.at(0u), object);
            (void)call_set(rt, ctx, 0x089D5E8Cu, {frame.at(8u), frame.at(0u)});
            (void)call_set(rt, ctx, 0x0897783Cu, {self, frame.at(8u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        }
    }
    if (s5 != 0u) put(rt, self + 0x1B0u, 0u);
    (void)call_set(rt, ctx, 0x089B0950u, {s3});
}

// func_08992320(self, flag): re-lays the children of a panel by the margins of its three anchors (self[0x124], self[0x128] and the neighbours) and finishes with func_08A01C30(self, flag). Nothing but that when func_089673F4(self) answers 0. Otherwise func_08A0D048(self, sp + 8, sp + 0xC, sp + 0x10, sp + 0x14) (four integers: left, right, top, bottom of the frame) and func_0896A35C(self, 0, 0, sp, sp + 4) (the pair (x, y)); f24 = x - (float)sp[0xC], f23 = y - (float)sp[0x14], f22 = (float)sp[8], f21 = (float)sp[0x10]. For the anchor a = self[0x124] (when func_08A00448(self, a) answers non-zero): the pair func_0896B7AC(a, sp + 0x18, sp + 0x1C) and func_08966B20(a, 0, 0, sp + 0x10, sp + 0x14) give w = (float)(sp[0x10] + sp[0x14]) + sp[0x1C] (stored back to sp[0x1C]), and by func_089670A0(a): 3 gives f25 = self[0x138] + w (f27 = 0.0f), other values f27 = self[0x138] + w (f25 = 0.0f); no anchor gives both 0.0f. The anchor b = self[0x128] does the same with func_08966B20(b, sp + 8, sp + 0xC, 0, 0) and func_089670A0(b) == 2: f20 = self[0x134] + (...) else f26 = self[0x134] + (...), with sp[0x18]. func_08A16EA4(sp + 0x20); then for every child c of the list self[0xF4], c = func_08968A38(c) while non-zero, that func_08A00448(self, c) accepts: the rectangle (f12, f13, f14, f15) is (f26 + f22, f24 - f20, f27 + f21, f23 - f25) for the child self[0x120], (f26 + f22, f24 - f20, f21, f23) for self[0x124], (f22, f24, f27 + f21, f23 - f25) for self[0x128] and (f22, f24, f21, f23) for any other, and func_08A17414(sp + 0x20, c; f12 .. f15) runs.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (anchor_a != 0u && call_set(rt, ctx, 0x08A00448u, {self, anchor_a}) != 0u) {`; `if (kind == 3u) {`; `else {`; `if (anchor_b != 0u && call_set(rt, ctx, 0x08A00448u, {self, anchor_b}) != 0u) {`; `if (kind != 2u) {`; `if (child != 0u) {`; `if (child == word(rt, self + 0x120u)) {`; `else if (child == word(rt, self + 0x124u)) {`; `else if (child == word(rt, self + 0x128u)) {`.
void func_08992320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    if (call_set(rt, ctx, 0x089673F4u, {self}) != 0u) {
        (void)call_set(rt, ctx, 0x08A0D048u, {self, frame.at(8u), frame.at(0xCu), frame.at(0x10u), frame.at(0x14u)});
        (void)call_set(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0u), frame.at(4u)});
        const auto whole = [&](std::uint32_t at) { return static_cast<float>(signed_of(word(rt, at))); };
        const float f2 = whole(frame.at(0xCu)), f3 = whole(frame.at(0x14u));
        const std::uint32_t anchor_a = word(rt, self + 0x124u);
        const float f24 = float_at(rt, frame.at(0u)) - f2;
        const float f23 = float_at(rt, frame.at(4u)) - f3;
        const float f22 = whole(frame.at(8u));
        const float f21 = whole(frame.at(0x10u));
        float f25 = 0.0f, f27 = 0.0f, f20 = 0.0f, f26 = 0.0f;
        if (anchor_a != 0u && call_set(rt, ctx, 0x08A00448u, {self, anchor_a}) != 0u) {
            (void)call_set(rt, ctx, 0x0896B7ACu, {word(rt, self + 0x124u), frame.at(0x18u), frame.at(0x1Cu)});
            (void)call_set(rt, ctx, 0x08966B20u, {word(rt, self + 0x124u), 0u, 0u, frame.at(0x10u), frame.at(0x14u)});
            const std::uint32_t sum = word(rt, frame.at(0x10u)) + word(rt, frame.at(0x14u));
            float w = static_cast<float>(signed_of(sum)) + float_at(rt, frame.at(0x1Cu));
            put_float(rt, frame.at(0x1Cu), w);
            const std::uint32_t kind = call_set(rt, ctx, 0x089670A0u, {word(rt, self + 0x124u)});
            const float f1 = float_at(rt, self + 0x138u);
            w = float_at(rt, frame.at(0x1Cu));
            if (kind == 3u) {
                f27 = 0.0f;
                f25 = f1 + w;
            } else {
                f25 = 0.0f;
                f27 = f1 + w;
            }
        }
        const std::uint32_t anchor_b = word(rt, self + 0x128u);
        if (anchor_b != 0u && call_set(rt, ctx, 0x08A00448u, {self, anchor_b}) != 0u) {
            (void)call_set(rt, ctx, 0x0896B7ACu, {word(rt, self + 0x128u), frame.at(0x18u), frame.at(0x1Cu)});
            (void)call_set(rt, ctx, 0x08966B20u, {word(rt, self + 0x128u), frame.at(8u), frame.at(0xCu), 0u, 0u});
            const std::uint32_t sum = word(rt, frame.at(8u)) + word(rt, frame.at(0xCu));
            float w = static_cast<float>(signed_of(sum)) + float_at(rt, frame.at(0x18u));
            put_float(rt, frame.at(0x18u), w);
            const std::uint32_t kind = call_set(rt, ctx, 0x089670A0u, {word(rt, self + 0x128u)});
            const float f1 = float_at(rt, self + 0x134u);
            w = float_at(rt, frame.at(0x18u));
            if (kind != 2u) {
                f20 = 0.0f;
                f26 = f1 + w;
            } else {
                f26 = 0.0f;
                f20 = f1 + w;
            }
        }
        (void)call_set(rt, ctx, 0x08A16EA4u, {frame.at(0x20u)});
        std::uint32_t child = word(rt, self + 0xF4u);
        if (child != 0u) {
            do {
                if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
                    float r12, r13, r14, r15;
                    if (child == word(rt, self + 0x120u)) {
                        r12 = f26 + f22;
                        r13 = f24 - f20;
                        r14 = f27 + f21;
                        r15 = f23 - f25;
                    } else if (child == word(rt, self + 0x124u)) {
                        r12 = f26 + f22;
                        r13 = f24 - f20;
                        r14 = f21;
                        r15 = f23;
                    } else if (child == word(rt, self + 0x128u)) {
                        r12 = f22;
                        r13 = f24;
                        r14 = f27 + f21;
                        r15 = f23 - f25;
                    } else {
                        r12 = f22;
                        r13 = f24;
                        r14 = f21;
                        r15 = f23;
                    }
                    (void)call_setf(rt, ctx, 0x08A17414u, {frame.at(0x20u), child}, {r12, r13, r14, r15});
                }
                child = call_set(rt, ctx, 0x08968A38u, {child});
            } while (child != 0u);
        }
    }
    (void)call_set(rt, ctx, 0x08A01C30u, {self, flag});
}

// func_08AF1524(self, view): draws the layers of a scene object with its state restored afterwards. fp = func_08AF26E4(view), saved1 = func_08AF293C(view), saved2 = func_08AF2954(view). Three tests of the view and the flags of d = self[4]: s3 = (func_08AF2834(view) != 0 && d[0x311] != 0), s2 = (func_08AF2840(view) != 0 && d[0x312] != 0), s5 = (func_08AF2840(view) != 0 && d[0x313] != 0). When one of the three bytes d[0x311 .. 0x313] is set: s3 gives func_08B58AD0(1, 1, 1, 1) and func_08B581F4(d[0x314], d[0x318], d[0x31C], 0.0f), s2 gives func_08B58A74(1) and func_08B581BC(1.0f); func_08AF2864(view) and func_08B58218((s3 ? 2 : 0) | (s2 ? 4 : 0) | (s5 ? 0x10 : 0)). func_08B56A00(1), func_08B569A0(), func_08B56A00(0), func_08B569A0(); the camera c = func_08AF1248(self) (func_08B56790(6, sp + 0x80, 0, 0) when it is not null); d[0x310] = 0 after func_08AF2828(view, self). Then for every layer i < d[0x298] (records of 20 bytes at d + 0x18) whose mask word r[8] shares a bit with d[0x2AC]: func_08AF2948(view, r[8]), func_08AF2960(view, r[0xC]); with a camera and r[0x11] set: func_08B56A00(1), the matrix of the virtual slot 8 of c into func_08B4E734(sp + 0xC0, sp + 0x40) and func_08B568DC(sp + 0xC0), func_08B56A00(0), slot 0xC of c into sp and func_08B4E734(sp + 0xC0, sp), func_08B568DC(sp + 0xC0); func_08AF2864(view); the overlay d[0x2B4] (when non-null and r[0x10] is set) gets slot 8(overlay, view); the layer object r[0] gets its slot 8 (r[0], view); func_08B5380C(). At the end func_08B56A00(1), func_08B56940(), func_08B56A00(0), func_08B568DC(sp + 0x80) with a camera, func_08B56940() and the view is restored: func_08AF2828(view, fp), func_08AF2948(view, saved1), func_08AF2960(view, saved2).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand and by an independent read-only review, branch targets and delay slots included): `if (call_set(rt, ctx, 0x08AF2834u, {view}) != 0u) s3 = byte_u(rt, word(rt, self + 4u) + 0x311u) != 0u ? 1u : 0u;`; `if (call_set(rt, ctx, 0x08AF2840u, {view}) != 0u) s2 = byte_u(rt, word(rt, self + 4u) + 0x312u) != 0u ? 1u : 0u;`; `if (call_set(rt, ctx, 0x08AF2840u, {view}) != 0u) s5 = byte_u(rt, word(rt, self + 4u) + 0x313u) != 0u ? 1u : 0u;`; `if (byte_u(rt, d0 + 0x311u) != 0u || byte_u(rt, d0 + 0x312u) != 0u || byte_u(rt, d0 + 0x313u) != 0u) {`; `if (s3 != 0u) {`; `if (s2 != 0u) {`; `const std::uint32_t mask = (s3 != 0u ? 2u : 0u) | (s2 != 0u ? 4u : 0u) | (s5 != 0u ? 0x10u : 0u);`; `if (camera != 0u) (void)call_set(rt, ctx, 0x08B56790u, {6u, frame.at(0x80u), 0u, 0u});`; `if (signed_of(layers) > 0) {`; `if (shared != 0u) {`; `if (camera != 0u && byte_u(rt, r + 0x11u) != 0u) {`; `if (overlay != 0u && byte_u(rt, r + 0x10u) != 0u) (void)call_set(rt, ctx, slot_of(rt, overlay, 8u), {overlay, view});`; `if (camera != 0u) (void)call_set(rt, ctx, 0x08B568DCu, {frame.at(0x80u)});`.
void func_08AF1524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], view = ctx.gpr[5];
    GuestFrame frame(ctx, 0x140u);
    const std::uint32_t fp = call_set(rt, ctx, 0x08AF26E4u, {view});
    const std::uint32_t saved1 = call_set(rt, ctx, 0x08AF293Cu, {view});
    put(rt, frame.at(0x100u), saved1);
    const std::uint32_t saved2 = call_set(rt, ctx, 0x08AF2954u, {view});
    put(rt, frame.at(0x104u), saved2);
    std::uint32_t s3 = 0u, s2 = 0u, s5 = 0u;
    if (call_set(rt, ctx, 0x08AF2834u, {view}) != 0u) s3 = byte_u(rt, word(rt, self + 4u) + 0x311u) != 0u ? 1u : 0u;
    if (call_set(rt, ctx, 0x08AF2840u, {view}) != 0u) s2 = byte_u(rt, word(rt, self + 4u) + 0x312u) != 0u ? 1u : 0u;
    if (call_set(rt, ctx, 0x08AF2840u, {view}) != 0u) s5 = byte_u(rt, word(rt, self + 4u) + 0x313u) != 0u ? 1u : 0u;
    const std::uint32_t d0 = word(rt, self + 4u);
    if (byte_u(rt, d0 + 0x311u) != 0u || byte_u(rt, d0 + 0x312u) != 0u || byte_u(rt, d0 + 0x313u) != 0u) {
        if (s3 != 0u) {
            (void)call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
            const std::uint32_t d = word(rt, self + 4u);
            (void)call_setf(rt, ctx, 0x08B581F4u, {}, {float_at(rt, d + 0x314u), float_at(rt, d + 0x318u), float_at(rt, d + 0x31Cu), 0.0f});
        }
        if (s2 != 0u) {
            (void)call_set(rt, ctx, 0x08B58A74u, {1u});
            (void)call_setf(rt, ctx, 0x08B581BCu, {}, {1.0f});
        }
        (void)call_set(rt, ctx, 0x08AF2864u, {view});
        const std::uint32_t mask = (s3 != 0u ? 2u : 0u) | (s2 != 0u ? 4u : 0u) | (s5 != 0u ? 0x10u : 0u);
        (void)call_set(rt, ctx, 0x08B58218u, {mask});
    }
    (void)call_set(rt, ctx, 0x08B56A00u, {1u});
    (void)call_set(rt, ctx, 0x08B569A0u, {});
    (void)call_set(rt, ctx, 0x08B56A00u, {0u});
    (void)call_set(rt, ctx, 0x08B569A0u, {});
    const std::uint32_t camera = call_set(rt, ctx, 0x08AF1248u, {self});
    if (camera != 0u) (void)call_set(rt, ctx, 0x08B56790u, {6u, frame.at(0x80u), 0u, 0u});
    const std::uint32_t d1 = word(rt, self + 4u);
    put8(rt, d1 + 0x310u, 0u);
    (void)call_set(rt, ctx, 0x08AF2828u, {view, self});
    const std::uint32_t d2 = word(rt, self + 4u);
    const std::uint32_t layers = word(rt, d2 + 0x298u);
    if (signed_of(layers) > 0) {
        std::uint32_t a0 = d2;
        for (std::uint32_t i = 0u; i != layers;) {
            const std::uint32_t r = a0 + i * 20u + 0x18u;
            const std::uint32_t shared = word(rt, a0 + 0x2ACu) & word(rt, r + 8u);
            const std::uint32_t mask_word = word(rt, r + 8u);
            ++i;
            if (shared != 0u) {
                (void)call_set(rt, ctx, 0x08AF2948u, {view, mask_word});
                (void)call_set(rt, ctx, 0x08AF2960u, {view, word(rt, r + 0xCu)});
                if (camera != 0u && byte_u(rt, r + 0x11u) != 0u) {
                    const std::uint32_t small = frame.at(0x40u), big = frame.at(0xC0u);
                    (void)call_set(rt, ctx, 0x08B56A00u, {1u});
                    (void)call_set(rt, ctx, slot_of(rt, camera, 8u), {small, camera});
                    (void)call_set(rt, ctx, 0x08B4E734u, {big, small});
                    (void)call_set(rt, ctx, 0x08B568DCu, {big});
                    (void)call_set(rt, ctx, 0x08B56A00u, {0u});
                    (void)call_set(rt, ctx, slot_of(rt, camera, 0xCu), {frame.at(0u), camera});
                    (void)call_set(rt, ctx, 0x08B4E734u, {big, frame.at(0u)});
                    (void)call_set(rt, ctx, 0x08B568DCu, {big});
                }
                (void)call_set(rt, ctx, 0x08AF2864u, {view});
                const std::uint32_t overlay = word(rt, word(rt, self + 4u) + 0x2B4u);
                if (overlay != 0u && byte_u(rt, r + 0x10u) != 0u) (void)call_set(rt, ctx, slot_of(rt, overlay, 8u), {overlay, view});
                const std::uint32_t object = word(rt, r);
                (void)call_set(rt, ctx, slot_of(rt, object, 8u), {object, view});
                (void)call_set(rt, ctx, 0x08B5380Cu, {});
            }
            a0 = word(rt, self + 4u);
        }
    }
    (void)call_set(rt, ctx, 0x08B56A00u, {1u});
    (void)call_set(rt, ctx, 0x08B56940u, {});
    (void)call_set(rt, ctx, 0x08B56A00u, {0u});
    if (camera != 0u) (void)call_set(rt, ctx, 0x08B568DCu, {frame.at(0x80u)});
    (void)call_set(rt, ctx, 0x08B56940u, {});
    (void)call_set(rt, ctx, 0x08AF2828u, {view, fp});
    (void)call_set(rt, ctx, 0x08AF2948u, {view, word(rt, frame.at(0x100u))});
    (void)call_set(rt, ctx, 0x08AF2960u, {view, word(rt, frame.at(0x104u))});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

}  // namespace
}  // namespace hot_batch_97
}  // namespace gt::game

#define HOT97(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_97::name, gt::ReturnKind::kind)
#define HOT97N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_97::name, gt::ReturnKind::kind)

// HOT97REGS
HOT97(0x0880E400u, func_0880E400, Word);
HOT97(0x08811E30u, func_08811E30, Word);
HOT97(0x08812720u, func_08812720, Word);
HOT97(0x08812CECu, func_08812CEC, Word);
// HOT97(0x08860890u, func_08860890, Word);  // HOT97: 3000 dense race cases (13,634 calls) and 99 fast-gate cases, 0 mismatches, none changed memory
HOT97(0x08883B7Cu, func_08883B7C, Word);
// HOT97(0x0889AD18u, func_0889AD18, Word);  // HOT97: 506 dense race cases (all its calls) and 85 fast-gate cases, 0 mismatches, none changed memory (the ids that write memory never occur)
HOT97(0x088EE6C0u, func_088EE6C0, Word);
HOT97(0x08904E08u, func_08904E08, Float);
HOT97(0x0890E41Cu, func_0890E41C, Word);
HOT97(0x08915714u, func_08915714, Word);
HOT97(0x08928A80u, func_08928A80, Word);
HOT97(0x08928E24u, func_08928E24, Word);
HOT97(0x089491A0u, func_089491A0, Float);
HOT97(0x0895E3F4u, func_0895E3F4, Word);
HOT97(0x08969D2Cu, func_08969D2C, Word);
HOT97(0x0896A540u, func_0896A540, Word);
HOT97(0x08974A14u, func_08974A14, Word);
HOT97(0x0897D118u, func_0897D118, Word);
HOT97(0x08992320u, func_08992320, Word);
HOT97(0x089ABBB0u, func_089ABBB0, Word);
HOT97(0x089BEF60u, func_089BEF60, Word);
HOT97(0x089FB990u, func_089FB990, Word);
HOT97(0x08AE0244u, func_08AE0244, Word);
HOT97(0x08AEB4CCu, func_08AEB4CC, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT97(0x08AF0B78u, func_08AF0B78, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT97(0x08AF1524u, func_08AF1524, Word);
// HOT97(0x08B04D98u, func_08B04D98, Word);  // HOT97: the tail callee fills new vector elements from uninitialised stack memory (a3 = its own sp + 0x20): 11 of 11 compared cases mismatch on the stack garbage the decompiled version does not reproduce
// HOT97(0x08B3177Cu, func_08B3177C, Word);  // HOT97: 3000 dense race cases (10,470 calls) and 94 fast-gate cases, 0 mismatches, none changed memory
HOT97(0x08B5BB0Cu, func_08B5BB0C, Word);
HOT97N(0x08B610C0u, func_08B610C0, Word);
HOT97(0x08B8F080u, func_08B8F080, Word);
// HOT97(0x08C496A4u, func_08C496A4, Word);  // HOT97: 204 dense race cases (all its calls) and 17 fast-gate cases, 0 mismatches, none changed memory: the ranges are always below 17 elements, so the partition path never runs
HOT97(0x08C4D4ECu, func_08C4D4EC, Word);
