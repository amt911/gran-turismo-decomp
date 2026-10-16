// Hot batch 96: reached functions of the two probes batches 93-95 worked from, the 121-170 pool (`tools.decomp_gate probe b93p`, 80 reached functions still without a `func_X` in src/) and the 171-230 pool (`probe b95p`, 98 reached and not yet written): 178 candidates, 37 written (32 registered, 5 not). (main includes batch 95, 6,435 functions.) Before any function was read its address was searched for in src/ (main and the open PR branches: none is open; the hits are callers only).
//
// How the functions were found: `tools.shapes --skeleton` over the 178 candidates finds no exact family (every cluster is one function); the near pairs (0.80-0.89 similar) are fill-insert variants whose element-move loops differ (func_08BAC714 / func_08BBF0D0 / func_08C30F28, func_08BE6708 / func_08BE6DB8, func_08C0E874 / func_08C2C56C / func_08C4C0A0 / func_08C4BD80) and were left. So the functions were read one by one with `tools.disasm`, `tools.sp_loads` and `tools.arg_reads`, the straight-line float code and the shallow call chains first.
// What was written (the role words describe the shape of the accesses; none is a name from the game): float leaves (func_088CB9B0 a smoothing step over a ring of 36 records, func_08954898 a byte-table to float-record converter, func_08B49F68 a tangent: range reduction and two polynomials, func_08C46A5C a 4x4 matrix times a 4-vector, func_08B4FC34 the spherical interpolation of two quaternions), curves and followers (func_089109A0 a piecewise table curve, func_08926EA0 an angle follower, func_089F6748 a scrolling value, func_089AA708 three tracked values), layouts (func_08A17104 / func_08A18494 a rectangle layout from callee-filled integers and flags, func_08A4D79C a text block, func_0880E1E0 / func_08812A8C / func_08813044 marker, key and digit windows) and per-frame updates (func_088705D4, func_08956B20, func_088AA230, func_08AD2B50, func_08AD6EBC, func_089AAD34, func_088AB884, func_08905300, func_089230A4, func_0896A9C4, func_089ACCA0, func_0894CA04, func_0894F1C8, func_08A07E28, func_0889E01C, func_0893B494, func_08956930).
//
// Registered (32): see the HOT96 lines at the end. Written but not registered (5, each commented HOT96 line says why): func_088616A8, func_0889B810, func_0893CBCC, func_0895B8C4 and func_0895C140 were compared (98, 87, 83, 83 and 83 cases in the fast gate, 3000 each in a dense race run of 11,810-16,430 calls) with 0 mismatches and not one case changed memory: they update state only in conditions the scripted routes never reach, so the paths that write are unproven. Left (read, not written): the libm-like functions with tables in .rodata (func_08B65F0C, func_08B6DFD0, func_08B6E920), the VFPU function func_089AD0E0, the 64-bit software division func_08B71720, the allocator and string-path code with tangled likely branches (func_08B2CB18, func_08AA3F44, func_089101F0 with its alloca and sort) and the rest of the pool.
//
// Gates (fast gate, five routes uncapped, with the coverage pass): every registered function compared at least once with 0 mismatches and, except the two pure float functions func_08B49F68 and func_089109A0, at least one compared case that changed memory; `tools.decomp_gate sabotage` pins 32 of 32 in the first round; the full gate (every function, real time, 6,467 registered) compares 5,168 with 0 mismatches (5,132 on main; 31 of the 32 new functions are compared in it, func_08954898 only in the uncapped fast gate); `tools.contract_audit` finds nothing new (the two registrations of batch 90 whose callers read v1); `tools.family_members --check` reads no row; `tools.literal_check` finds 0 literals missing in 37 functions with 3 `--allow` entries
// (0x51EB851F of func_0894CA04 is a `lui` / `ori` pair on two registers; 0x08CF4A2C of func_089AAD34 and func_089AA708 is the lock the asm builds with `lui s0, 2` + `addiu a0, s0, -0x7AD4`, the tool resolves only the other lock's -0x7A34); `tools.sp_loads` finds only loads the C++ models; `tools.arg_reads` (now following `j` forwarders, 3 new tests) lists three calls (and one line that quotes a call in a comment) that pass fewer arguments than their callee reads, all harmless (func_08B58874 is called with 0x6D, a mode that reads only a0; func_089AAC8C and func_08A07B70 read the a1 the context still holds from the entry, the same value the asm has).
// Findings of the first gates: func_089230A4 and func_0895C140 mismatched on v0 only (the constants a test leaves in v0, FINDINGS), and func_08AD6EBC mismatched on all 98 cases because its call to the forwarder func_08AD6A00 (`j func_08C3E4E8`, a three-argument partial sort) passed two arguments (FINDINGS; `tools.arg_reads` follows forwarders now). A review of the paths no run takes (three read-only reviewers over the asm) found three more: func_0889E01C's early return (v0 is the `andi` of the test, 0), func_0894CA04's `slt` (a signed compare of the halfword with the signed start) and func_089F6748 (the value of the two middle virtual calls is re-read from memory after each call).
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm by hand). The weakest proofs: func_08956930 (the state machine of the flag byte: only the first call's arm ran), func_0896A9C4 (only mode 2 ran: modes 1 and 3 are read from the asm), func_089AAD34 (the whole tolerance block and the lock block), func_0889E01C, func_088705D4 (the block call and both angle wraps), func_089109A0 (only the table lookups of the way 0), func_08AD6EBC (the o[0x1E] / o[0x21] arms), func_088AB884 (the stores of the best index) and func_08B4FC34 (only the generic arm ran: t == 0, t == 1, d < 0, 1 < d and s == 0 are read from the asm).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value, so a callee that reads a register this function never passes sees what an earlier call_set left; `call_setf` also sets f12-f15. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20-$f24 are locals. The delay slot of a `jal` / `jalr` runs BEFORE the call,
// a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths; every "the delay slot of ..." comment below names the instruction right after the branch it says. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the constants a test leaves in v0 (an `andi` / `slti` before a branch or a loop bound); the dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that callees read are.
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
namespace hot_batch_96 {
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

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_0880E1E0(self, q): positions the markers of a list on the window w = self[0x9C]. wanted = func_08893684(self[4], 0) (kept at sp[0x10]); n = the signed halfword q[4]; when func_089DDC94(w) is not 2 * n the window is prepared: w[0xE8] = 6, func_089DE054(w, 2 * n), func_089DE1BC(w, 2 * n). For k = 0..n-1 (entries j = the signed halfword q[0x558 + 2 * (n - 1 - k)]): (x, y) = the floats q[0x2288 + 8 * j], q[0x228C + 8 * j] go through func_08B4AD64(sp + 4, self + 0x248, sp + 4);
// f20 = 1.0f when the byte q[j] is 1, else 0.0f; when j equals wanted: bit 14 of self[0xA0][0xE0] is set when f20 was above 0.0f (cleared otherwise), f20 becomes 0.0f and func_0896A9C4(self[0xA0], sp, 0, 0, 0) runs with sp[0] = x' - 5.0f and then func_0896A9C4(self[0xA0], 0, sp, 0, 0) with sp[0] = y' - 5.0f (x', y' = the transformed pair); then func_089DB078(w, 2 * k; f12 = f13 = 0.0f, f14 = f15 = f20) and func_089DAEA0(w, 2 * k; f12 = x' - 3.0f, f13 = y' - 3.0f, f14 = f15 = 6.0f (0x40C00000)). At the end w[0xE0] |= 0x4000 (v0 = that value).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count - 1u) >= 0) {`; `const float mark = (sext8(byte_u(rt, q + j)) ^ 1u) == 0u ? 1.0f : 0.0f;`; `put(rt, overlay + 0xE0u, (word(rt, overlay + 0xE0u) & ~0x4000u) | (lit ? 0x4000u : 0u));`
void func_0880E1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], q = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t window = word(rt, self + 0x9Cu);  // the delay slot of the first jal
    const std::uint32_t wanted = call_set(rt, ctx, 0x08893684u, {word(rt, self + 4u), 0u});
    put(rt, frame.at(0x10u), wanted);
    const std::uint32_t count = sext16(half_u(rt, q + 4u));
    const std::uint32_t doubled = count << 1u;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != doubled) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, doubled});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, doubled});
    }
    if (signed_of(count - 1u) >= 0) {
        std::uint32_t entry = q + ((count - 1u) << 1u) + 0x558u;
        const std::uint32_t pair = frame.at(4u);
        for (std::uint32_t k = 0u; k != count;) {
            const std::uint32_t j = sext16(half_u(rt, entry));
            const std::uint32_t marker = q + (j << 3u);
            const float mark = (sext8(byte_u(rt, q + j)) ^ 1u) == 0u ? 1.0f : 0.0f;
            float f20 = mark;
            put(rt, pair, word(rt, marker + 0x2288u));
            const std::uint32_t slot = k << 1u;
            entry -= 2u;
            put(rt, frame.at(8u), word(rt, marker + 0x228Cu));  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B4AD64u, {pair, self + 0x248u, pair});
            ++k;
            if (word(rt, frame.at(0x10u)) == j) {
                const bool lit = 0.0f < f20;
                f20 = 0.0f;
                const std::uint32_t overlay = word(rt, self + 0xA0u);
                put(rt, overlay + 0xE0u, (word(rt, overlay + 0xE0u) & ~0x4000u) | (lit ? 0x4000u : 0u));
                put_float(rt, frame.at(0u), float_at(rt, pair) - 5.0f);
                (void)call_set(rt, ctx, 0x0896A9C4u, {word(rt, self + 0xA0u), frame.at(0u), 0u, 0u, 0u});
                put_float(rt, frame.at(0u), float_at(rt, frame.at(8u)) - 5.0f);
                (void)call_set(rt, ctx, 0x0896A9C4u, {word(rt, self + 0xA0u), 0u, frame.at(0u), 0u, 0u});
            }
            (void)call_setf(rt, ctx, 0x089DB078u, {window, slot}, {0.0f, 0.0f, f20, f20});
            (void)call_setf(rt, ctx, 0x089DAEA0u, {window, slot}, {float_at(rt, frame.at(4u)) - 3.0f, float_at(rt, frame.at(8u)) - 3.0f, 6.0f, 6.0f});
        }
    }
    const std::uint32_t flags = word(rt, window + 0xE0u) | 0x4000u;
    put(rt, window + 0xE0u, flags);
    ctx.gpr[2] = flags;
}

// func_08812A8C(self, key, flag, index, dim): lays out one key of an on-screen keyboard on the window w = self[0x48] (nothing when null). When func_089DDC94(w) answers other than 4 the window is prepared: w[0xE8] = 6, func_089DCCD0(w, 4), func_089DE054(w, 4), func_089DE1BC(w, 4), func_0880E024(self + 0x21C, w, 0, 0x30; f12 = 7.0f, f13 = 8.0f), func_089DB204(w, 1, self[0x23C] + 0x40) and the same with (2, 0x30; f12 = 31.0f (0x41F80000), f13 = 8.0f) and func_089DB204(w, 3, self[0x23C] + 0x20).
// Then the colour c = a copy of the record self[0x23C] + 0x40 (func_0896418C(sp, ..)) whose alpha sp[0xC] is 0.0f when index < 0, ((float)((self[0x10] << 4) & 0xFF) / 255.0f) when dim != 0, else the record's own; unless index < 0, func_089DB078(w, 0; f12..f15 = the four floats of the entry (index - (trunc(table[8]) - 1)) of the table at table + 0x300, table = self[0x21C], 16 bytes each); func_089DB204(w, 1, sp).
// The key code selects an entry: 0x4E gives 0xB, 0x52 gives 0xC, 0x44 gives 0xA, any other code - 0x30. The colour becomes a copy of self[0x23C] + 0x20 (func_089641C4(sp, ..)) with alpha 1.0f (0.5f when flag == 0) and func_089DB078(w, 2; the entry's floats), func_089DB204(w, 3, sp) end: v0 = the last call's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (window == 0u) return;`; `if (signed_of(index) < 0) {`; `if (dim != 0u) {`; `if (key == 0x4Eu) selected = 0xBu;`; `else if (key == 0x44u) selected = 0xAu;`; `put_float(rt, colour + 0xCu, flag != 0u ? 1.0f : 0.5f);`
void func_08812A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5], flag = ctx.gpr[6], index = ctx.gpr[7], dim = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t window = word(rt, self + 0x48u);
    if (window == 0u) return;
    const std::uint32_t list = self + 0x21Cu;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 4u) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DCCD0u, {window, 4u});
        (void)call_set(rt, ctx, 0x089DE054u, {window, 4u});
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 4u});
        (void)call_setf(rt, ctx, 0x0880E024u, {list, window, 0u, 0x30u}, {7.0f, 8.0f});
        (void)call_set(rt, ctx, 0x089DB204u, {window, 1u, word(rt, self + 0x23Cu) + 0x40u});
        (void)call_setf(rt, ctx, 0x0880E024u, {list, window, 2u, 0x30u}, {std::bit_cast<float>(0x41F80000u), 8.0f});
        (void)call_set(rt, ctx, 0x089DB204u, {window, 3u, word(rt, self + 0x23Cu) + 0x20u});
    }
    const std::uint32_t colour = frame.at(0u);
    (void)call_set(rt, ctx, 0x0896418Cu, {colour, word(rt, self + 0x23Cu) + 0x40u});
    const auto entry_floats = [&](std::uint32_t table, std::uint32_t entry_index) {
        const std::int32_t rows = static_cast<std::int32_t>(ctx.fpu_float_to_word(float_at(rt, table + 8u), 1u));
        const std::uint32_t row = table + (((entry_index - static_cast<std::uint32_t>(rows - 1)) << 4u));
        const std::uint32_t floats = row + 0x300u;
        return std::array<float, 4>{float_at(rt, floats), float_at(rt, floats + 4u), float_at(rt, floats + 8u), float_at(rt, floats + 0xCu)};
    };
    if (signed_of(index) < 0) {
        put(rt, colour + 0xCu, 0u);
    } else {
        if (dim != 0u) {
            const std::uint32_t scaled = (word(rt, self + 0x10u) << 4u) & 0xFFu;
            put_float(rt, colour + 0xCu, static_cast<float>(static_cast<std::int32_t>(scaled)) / std::bit_cast<float>(0x437F0000u));
        }
        const auto f = entry_floats(word(rt, self + 0x21Cu), index);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0u}, {f[0], f[1], f[2], f[3]});
    }
    (void)call_set(rt, ctx, 0x089DB204u, {window, 1u, colour});
    std::uint32_t selected;
    if (key == 0x4Eu) selected = 0xBu;
    else if (key == 0x52u) selected = 0xCu;
    else if (key == 0x44u) selected = 0xAu;
    else selected = key - 0x30u;
    (void)call_set(rt, ctx, 0x089641C4u, {colour, word(rt, self + 0x23Cu) + 0x20u});
    put_float(rt, colour + 0xCu, flag != 0u ? 1.0f : 0.5f);
    const auto f = entry_floats(word(rt, list), selected);
    (void)call_setf(rt, ctx, 0x089DB078u, {window, 2u}, {f[0], f[1], f[2], f[3]});
    (void)call_set(rt, ctx, 0x089DB204u, {window, 3u, colour});
}

// func_08813044(self, q): draws a number as eight digits on the window w = self[0xA8] (nothing when null). When func_089DDC94(w) answers other than 0x10 the window is prepared: w[0xE8] = 6, func_089DE054(w, 0x10), func_089DE1BC(w, 0x10) and, for s = 0xE, 0xC, ... 0: f20 += func_0880E024(self + 0x220, w, s, 0x30; f12 = f20, f13 = 0.0f) + 1.0f (the sum is not used afterwards).
// n = the entry self[0x280 + 4 * i] for the first i in 0..3 whose halfword q[0x558 + 2 * i] equals func_08893684(self[4], 0) (0 when none). When n equals self[0x27C] nothing is drawn; otherwise table = self[0x220] and row(d) = table + ((d - (trunc(table[8]) - 1)) << 4) with the four floats at row + 0x300: func_089DB078(w, 0, row(n % 10) floats) draws the lowest digit, then seven times (index 2, 4, .. 0xE) n = n / 10 and func_089DB078(w, index, the floats of row(n % 10), or of the static record D_08CE29F0 (0x08CE29F0) when n is 0).
// self[0x27C] = n at its last value (the quotient left after the loop, as the asm stores s0).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (window == 0u) return;`; `for (std::uint32_t i = 0u; i != 4u; ++i) {`; `if (n != word(rt, self + 0x27Cu)) {`
void func_08813044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], q = ctx.gpr[5];
    const std::uint32_t window = word(rt, self + 0xA8u);
    if (window == 0u) return;
    const std::uint32_t list = self + 0x220u;
    if (call_set(rt, ctx, 0x089DDC94u, {window}) != 0x10u) {
        put(rt, window + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {window, 0x10u});
        float sum = 0.0f;
        (void)call_set(rt, ctx, 0x089DE1BCu, {window, 0x10u});
        for (std::uint32_t s = 0xEu; s != 0xFFFFFFFEu; s -= 2u) {
            const float ret = call_setf(rt, ctx, 0x0880E024u, {list, window, s, 0x30u}, {sum, 0.0f}).f0;
            sum = sum + (ret + 1.0f);
        }
    }
    const std::uint32_t wanted = call_set(rt, ctx, 0x08893684u, {word(rt, self + 4u), 0u});
    std::uint32_t n = 0u;
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        if (sext16(half_u(rt, q + 0x558u + 2u * i)) == wanted) {
            n = word(rt, self + 0x280u + 4u * i);
            break;
        }
    }
    ctx.gpr[2] = word(rt, self + 0x27Cu);  // the lw v0 of the compare: the v0 the equal case leaves
    if (n != word(rt, self + 0x27Cu)) {
        const auto row_of = [&](std::uint32_t table, std::uint32_t digit) {
            const std::int32_t rows = static_cast<std::int32_t>(ctx.fpu_float_to_word(float_at(rt, table + 8u), 1u));
            return table + ((digit - static_cast<std::uint32_t>(rows - 1)) << 4u) + 0x300u;
        };
        const std::uint32_t table = word(rt, list);
        const std::uint32_t first = row_of(table, n % 10u);
        (void)call_setf(rt, ctx, 0x089DB078u, {window, 0u}, {float_at(rt, first), float_at(rt, first + 4u), float_at(rt, first + 8u), float_at(rt, first + 0xCu)});
        for (std::uint32_t index = 2u; index != 0x10u; index += 2u) {
            n = n / 10u;
            std::uint32_t record;
            if (n == 0u) record = 0x08CE29F0u;
            else record = row_of(word(rt, list), n % 10u);
            (void)call_setf(rt, ctx, 0x089DB078u, {window, index, record}, {float_at(rt, record), float_at(rt, record + 4u), float_at(rt, record + 8u), float_at(rt, record + 0xCu)});
        }
    }
    put(rt, self + 0x27Cu, n);
}

// func_088616A8(self): nothing happens when func_088DAA84(self[0x5AC]) answers zero. R = self + 0x608 + 7224 * self[0x3E78] (a record of 0x1C38 bytes); u = func_088D9C50(self[0x5AC], 0, R); a non-zero u runs func_08861538(self, R, u). Nothing more happens when the byte R[0x1C34] is zero. c = the signed halfword func_088DA160(self[0x5AC]) is stored at R[0x1C32] and when positive R[0x1C2A + 2 * k] = func_088DA170(self[0x5AC], k) for k below the (re-read) R[0x1C32].
// R[0x1C28] = 0, then for the n = func_088D9F80(self[0x5AC]) slots j (entry j of 0xA8 bytes from R): when func_088DC90C(self[0x5AC], j) is below 2 and the byte at R + 0xA8 * j + 0x102A is set, j is appended as a halfword at R[0x1C20 + 2 * R[0x1C28]] and R[0x1C28] += 1. Then the list of R[0x1C28] halfwords at R + 0x1C20 is selection-sorted by the 64-bit key (high word at +0x24, low at +0x20) of the record R + 0x1018 + 168 * index: for i, with the pivot the entry i, every later entry whose key is below the pivot's (a0 = 1) is swapped in when a0 differs from the byte R[0x1C35] (an equal key never swaps).
// Not registered: 3000 dense race cases compared equal (0 mismatches) and none of them changed memory, so the paths that write are proven only by reading them against the asm by hand (the gate cannot see them).
[[maybe_unused]] void func_088616A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (call_set(rt, ctx, 0x088DAA84u, {word(rt, self + 0x5ACu)}) == 0u) return;
    const std::uint32_t index = word(rt, self + 0x3E78u);
    const std::uint32_t record = self + (index * 56u + (index * 56u << 7u)) + 0x608u;
    const std::uint32_t found = call_set(rt, ctx, 0x088D9C50u, {word(rt, self + 0x5ACu), 0u, record});
    if (found != 0u) (void)call_set(rt, ctx, 0x08861538u, {self, record, found});
    if (byte_u(rt, record + 0x1C34u) == 0u) return;
    const std::int32_t count = signed_of(sext16(call_set(rt, ctx, 0x088DA160u, {word(rt, self + 0x5ACu)})));
    put16(rt, record + 0x1C32u, static_cast<std::uint32_t>(count));
    if (count > 0) {
        std::uint32_t k = 0u;
        do {
            const std::uint32_t value = call_set(rt, ctx, 0x088DA170u, {word(rt, self + 0x5ACu), k});
            put16(rt, record + 0x1C2Au + 2u * k, value);
            ++k;
        } while (signed_of(k) < signed_of(sext16(half_u(rt, record + 0x1C32u))));
    }
    put16(rt, record + 0x1C28u, 0u);
    const std::int32_t slots = signed_of(call_set(rt, ctx, 0x088D9F80u, {word(rt, self + 0x5ACu)}));
    if (slots > 0) {
        for (std::int32_t j = 0; j != slots; ++j) {
            const std::uint32_t kind = call_set(rt, ctx, 0x088DC90Cu, {word(rt, self + 0x5ACu), static_cast<std::uint32_t>(j)});
            if (kind < 2u && byte_u(rt, record + 0xA8u * static_cast<std::uint32_t>(j) + 0x102Au) != 0u) {
                const std::uint32_t used = sext16(half_u(rt, record + 0x1C28u));
                put16(rt, record + 0x1C20u + 2u * used, static_cast<std::uint32_t>(j));
                put16(rt, record + 0x1C28u, used + 1u);
            }
        }
    }
    const std::int32_t n = signed_of(sext16(half_u(rt, record + 0x1C28u)));
    const auto key_record = [&](std::uint32_t entry) { return record + 0x1018u + 168u * entry; };
    for (std::int32_t i = 0; i < n - 1; ++i) {
        std::uint32_t pivot = sext16(half_u(rt, record + 0x1C20u + 2u * static_cast<std::uint32_t>(i)));
        std::uint32_t pivot_record = key_record(pivot);
        const std::uint32_t descending = byte_u(rt, record + 0x1C35u);
        for (std::int32_t j = i + 1; j < n; ++j) {
            const std::uint32_t slot = record + 0x1C20u + 2u * static_cast<std::uint32_t>(j);
            const std::uint32_t other = sext16(half_u(rt, slot));
            const std::uint32_t other_record = key_record(other);
            const std::uint32_t high1 = word(rt, pivot_record + 0x24u), low1 = word(rt, pivot_record + 0x20u);
            const std::uint32_t high2 = word(rt, other_record + 0x24u), low2 = word(rt, other_record + 0x20u);
            if (low1 == low2 && high1 == high2) continue;
            std::uint32_t below = 0u;
            if (high2 < high1) below = 1u;
            else if (high2 == high1 && low2 < low1) below = 1u;
            if (descending == below) continue;
            put16(rt, slot, pivot);
            pivot_record = other_record;
            pivot = other;
            put16(rt, record + 0x1C20u + 2u * static_cast<std::uint32_t>(i), other);
        }
    }
}

// func_088705D4(self, obj): a per-frame update of the record r = self[0xC] (s3) with the owner o = self[8] (s6). Three entries i = 0..2 of 0xC bytes at r + 0x98 (byte flag at +0, float at +4, word at +8 at r + 0xA0 + 0xC * i): for each one whose flag is set, f = the float; func_0886CBE8(o, sp, entry word) fills sp[0xC + i] (floats at sp + 0x30 + 4 * i) and, when it answers non-zero, f += sp[0xC + i]; r[0xC + i] = f (floats at r + 0x30 + 4 * i) and the flag is cleared.
// When the word at r[0x88] (a2) has a non-zero word at a2 - 0xC: func_0886CBE8(o, sp + 0x40, a2) fills a block and, when it answers non-zero, r[0x30..0x3C] = the four floats at sp + 0x70..0x7C and func_08B4E9D4(r, r + 0x40) runs when the byte r[0x8C] is non-zero, else func_08B4E9CC(r, sp + 0x40, r + 0x40). Then the virtual slot 0x1C of obj runs (obj, answers f0 = a) and func_0886B0F8(o) answers b; p = a * b.
// With r2 = self[0xC] again: when the float r2[0x94] (c) is not 0.0f, r2[0x90] += p * c, wrapped by 360.0f (0x43B4) into (-180.0f (0xC334), 180.0f (0x4334)]; then when r2[0x90] is not 0.0f the four words r2[0x30..0x3C] are saved, func_08B4F174(r2, r2[0x90] * 0x3C8EFA35) runs and the words are written back. v0 = self + 0xC (the register the asm keeps) unless that last call ran (its v0).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, entry + 0x98u) == 0u) continue;`; `float f = float_at(rt, entry + 0x9Cu);`; `if (found != 0u) f = f + float_at(rt, buffer + 4u * (0xCu + i));`; `if (word(rt, pointer - 0xCu) != 0u) {`; `if (call_set(rt, ctx, 0x0886CBE8u, {owner, block, pointer}) != 0u) {`; `if (byte_u(rt, r + 0x8Cu) != 0u) {`; `if (angle <= low) {`; `do { wrapped = wrapped + turn; } while (wrapped <= low);`; `if (high < angle) {`; `do { wrapped = wrapped - turn; } while (high < wrapped);`
void func_088705D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    const std::uint32_t r = word(rt, self + 0xCu), owner = word(rt, self + 8u);
    const std::uint32_t buffer = frame.at(0u);
    for (std::uint32_t i = 0u; i != 3u; ++i) {
        ctx.gpr[4] = owner;  // a0 / a1 are set before the flag test, whatever it answers
        ctx.gpr[5] = buffer;
        const std::uint32_t entry = r + 0xCu * i;
        if (byte_u(rt, entry + 0x98u) == 0u) continue;
        float f = float_at(rt, entry + 0x9Cu);
        const std::uint32_t found = call_set(rt, ctx, 0x0886CBE8u, {owner, buffer, word(rt, r + 0xA0u + 0xCu * i)});
        if (found != 0u) f = f + float_at(rt, buffer + 4u * (0xCu + i));
        put_float(rt, r + 4u * (0xCu + i), f);
        put8(rt, entry + 0x98u, 0u);
    }
    const std::uint32_t pointer = word(rt, r + 0x88u);
    ctx.gpr[6] = pointer;  // a2 of the asm at the next test
    if (word(rt, pointer - 0xCu) != 0u) {
        const std::uint32_t block = frame.at(0x40u);
        if (call_set(rt, ctx, 0x0886CBE8u, {owner, block, pointer}) != 0u) {
            if (byte_u(rt, r + 0x8Cu) != 0u) {
                put(rt, r + 0x30u, word(rt, frame.at(0x70u)));
                put(rt, r + 0x34u, word(rt, frame.at(0x74u)));
                put(rt, r + 0x38u, word(rt, frame.at(0x78u)));
                put(rt, r + 0x3Cu, word(rt, frame.at(0x7Cu)));
                (void)call_set(rt, ctx, 0x08B4E9D4u, {r, r + 0x40u});
            } else {
                ctx.gpr[4] = r;
                (void)call_set(rt, ctx, 0x08B4E9CCu, {r, block, r + 0x40u});
            }
        }
    }
    const std::uint32_t vtable = word(rt, obj);
    const float a = call_keep(rt, ctx, word(rt, vtable + 0x1Cu), {obj}, {}).f0;
    const float b = call_keep(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)}, {}).f0;
    const float product = psp_mul(a, b);
    ctx.gpr[2] = self + 0xCu;
    const std::uint32_t r2 = word(rt, self + 0xCu);
    const float c = float_at(rt, r2 + 0x94u);
    float angle = float_at(rt, r2 + 0x90u);
    if (!(c == 0.0f)) {
        angle = angle + psp_mul(product, c);
        put_float(rt, r2 + 0x90u, angle);
        const float low = std::bit_cast<float>(0xC334'0000u), high = std::bit_cast<float>(0x4334'0000u), turn = std::bit_cast<float>(0x43B4'0000u);
        if (angle <= low) {
            float wrapped = angle;
            do { wrapped = wrapped + turn; } while (wrapped <= low);
            angle = wrapped;
            put_float(rt, r2 + 0x90u, angle);
        }
        if (high < angle) {
            float wrapped = angle;
            do { wrapped = wrapped - turn; } while (high < wrapped);
            angle = wrapped;
            put_float(rt, r2 + 0x90u, angle);
        }
    }
    if (angle == 0.0f) return;
    const std::uint32_t keep0 = word(rt, r2 + 0x30u), keep1 = word(rt, r2 + 0x34u), keep2 = word(rt, r2 + 0x38u), keep3 = word(rt, r2 + 0x3Cu);
    const float radians = psp_mul(float_at(rt, r2 + 0x90u), std::bit_cast<float>(0x3C8EFA35u));
    ctx.fpr[12] = radians;
    (void)call_keep(rt, ctx, 0x08B4F174u, {r2}, {radians});
    put(rt, r2 + 0x30u, keep0);
    put(rt, r2 + 0x34u, keep1);
    put(rt, r2 + 0x38u, keep2);
    put(rt, r2 + 0x3Cu, keep3);
}

// func_0889B810(self, flag): a per-frame pass over the list o = self[0x58][0x10] (s6; nothing when self[0x58], o or self[0xB88] is null, v0 = the null / self[0x58] loaded last). Four entries of 0x10 bytes at sp + 0xC are initialised {0, 1.0f, 0, byte 0} and the virtual slot 0xA8 of self[0xB04] runs for i = 0..3 with (object, entry i, i); when no entry's first float is above 0.0f nothing more happens (v0 = the last call's).
// Otherwise a stack object {D_08CC2260 (0x08CC2260), self[0xB88][0x68], 0} is linked (its owner's word +4 = sp) and, for each of the o[0x38] elements e (24 bytes at o[0x3C] + 0x18 * k) whose entry k & 3 has a first float f21 above 0.0f: func_08B569A0(), func_08B56364(e[0], e[4], e[8]), a = func_08B66D54(e[0x10]) * 180.0f / 3.14159274f - 90.0f, b = func_08B66FBC(e[0xC], e[0x14]), func_08B5606C(b * 180.0f / 3.14159274f);
// with flag == 0 func_08B56128(a), func_08B55FB0(entry[8]) and s = D_08CE2FE0's float at +8, otherwise s = the float at D_08CE2FE0 + 0xC (0x08CE2FE0); then func_08B562E0(f12 = f13 = f14 = s * entry[4]), sp[8] = owner[0x1C] = f21, func_08B54FB8(0x6D), func_08AFA170(self[0xB88], self[0xB8C + 4 * flag], 0, 0), func_08B58874(0x6D) and func_08B56940(). At the end the owner's word +4 is cleared and func_08AF86DC(sp) runs with sp[0] = D_08CC2260. v0 = its result.
// Not registered: 3000 dense race cases compared equal (0 mismatches) and none of them changed memory, so the paths that write are proven only by reading them against the asm by hand (the gate cannot see them).
[[maybe_unused]] void func_0889B810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[5] = flag;
    ctx.gpr[2] = word(rt, self + 0x58u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t list = word(rt, ctx.gpr[2] + 0x10u);
    if (list == 0u) return;
    ctx.gpr[2] = word(rt, self + 0xB88u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t entries = frame.at(0xCu);
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t entry = entries + 0x10u * i;
        put(rt, entry, 0u);
        put_float(rt, entry + 4u, 1.0f);
        put(rt, entry + 8u, 0u);
        put8(rt, entry + 0xCu, 0u);
    }
    bool any = false;
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t object = word(rt, self + 0xB04u);
        (void)call_set(rt, ctx, slot_of(rt, object, 0xA8u), {object, entries + 0x10u * i, i});
        if (0.0f < float_at(rt, entries + 0x10u * i)) any = true;
    }
    if (!any) return;
    const std::uint32_t owner = word(rt, word(rt, self + 0xB88u) + 0x68u);
    const std::uint32_t stack_object = frame.at(0u);
    put(rt, stack_object, 0x08CC2260u);
    put(rt, stack_object + 4u, owner);
    put(rt, stack_object + 8u, 0u);
    put(rt, owner + 4u, stack_object);
    std::uint32_t count = word(rt, list + 0x38u);
    if (signed_of(count) > 0) {
        const float half_turn = std::bit_cast<float>(0x43340000u), pi = std::bit_cast<float>(0x40490FDBu), ninety = std::bit_cast<float>(0x42B40000u);
        std::uint32_t offset = 0u;
        for (std::uint32_t k = 0u; signed_of(k) < signed_of(count);) {
            const std::uint32_t entry = entries + ((k & 3u) << 4u);
            const float f21 = float_at(rt, entry);
            if (f21 <= 0.0f) {
                ++k;
                offset += 0x18u;
                continue;
            }
            const std::uint32_t element = word(rt, list + 0x3Cu) + offset;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B569A0u, {});
            (void)call_keep(rt, ctx, 0x08B56364u, {}, {float_at(rt, element), float_at(rt, element + 4u), float_at(rt, element + 8u)});
            const float first = call_keep(rt, ctx, 0x08B66D54u, {}, {float_at(rt, element + 0x10u)}).f0;
            const float angle = (psp_mul(first, half_turn) / pi) - ninety;  // f20
            const float second = call_keep(rt, ctx, 0x08B66FBCu, {}, {float_at(rt, element + 0xCu), float_at(rt, element + 0x14u)}).f0;
            (void)call_keep(rt, ctx, 0x08B5606Cu, {}, {psp_mul(second, half_turn) / pi});
            float scale;
            if (flag == 0u) {
                (void)call_keep(rt, ctx, 0x08B56128u, {}, {angle});
                (void)call_keep(rt, ctx, 0x08B55FB0u, {}, {float_at(rt, entry + 8u)});
                scale = float_at(rt, 0x08CE2FE0u + 8u);
            } else {
                scale = float_at(rt, 0x08CE2FE0u + 0xCu);
            }
            const float size = psp_mul(scale, float_at(rt, entry + 4u));
            (void)call_keep(rt, ctx, 0x08B562E0u, {}, {size, size, size});
            put_float(rt, stack_object + 8u, f21);
            put_float(rt, owner + 0x1Cu, f21);
            (void)call_set(rt, ctx, 0x08B54FB8u, {0x6Du});
            (void)call_set(rt, ctx, 0x08AFA170u, {word(rt, self + 0xB88u), word(rt, self + flag * 4u + 0xB8Cu), 0u, 0u});
            (void)call_set(rt, ctx, 0x08B58874u, {0x6Du});
            ++k;
            (void)call_set(rt, ctx, 0x08B56940u, {});
            count = word(rt, list + 0x38u);
            offset += 0x18u;
        }
    }
    put(rt, owner + 4u, 0u);
    put(rt, stack_object, 0x08CC2260u);
    (void)call_set(rt, ctx, 0x08AF86DCu, {stack_object});
}

// func_0889E01C(self, a1, b, c, flag): nothing happens when the virtual slot 0x20 of self answers zero. fp = the slot 0x60 of c, s4 = its slot 0x5C and s3 = the slot 0x30 of b; with s3 non-zero and the bit 0x800 of s4 clear nothing more happens. s5 = 1, or s4 & 1 when flag & 0xFF is non-zero. Unless bit 0x400 of s4 is set and flag non-zero: the slot 0x10 of c (c, sp + 0x18) fills the vector sp[0x18..0x20];
// {f5, f6, f4} = {2.5f, 0x3D4CCCCD, 1.0f} when fp == 1 and (s3 != 0 or the slot 0x34 of b answers zero), else all 0x3E4CCCCD; f2 = 0.0f, or 0x3DCCCCCD when flag is zero; sp[0xC..0x14] = the vector, sp = {max(f2, f4), max(f2, f6), max(f2, f5)} and r = func_0889DEE4(b, sp + 0xC, sp) (the maxima keep the asm's compares: f4 < f2 gives f2, else f4); s5 = (r ^ 1) & s5.
// a3 = 0, or (fp == 1 and the slot 0xFC of b answers non-zero) = 1, the second test only when flag is non-zero. func_0889A968(self + 0x28, a1, b, a3, flag & 0xFF, (s4 >> 4) & 1, s5) ends: v0 = its result.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, slot_of(rt, self, 0x20u), {self}) == 0u) return;`; `if (s3 != 0u && (s4 & 0x800u) == 0u) {`; `const bool direct = (s4 & 0x400u) != 0u && flag != 0u;`; `if (!direct) {`; `if (fp == 1u) bright = s3 != 0u || call_set(rt, ctx, slot_of(rt, b, 0x34u), {b}) == 0u;`; `if (bright) {`; `const float x = f4 < f2 ? f2 : f4;`; `const float y = f6 < f2 ? f2 : f6;`; `if (!(f5 < f2)) f2 = f5;`; `if (check_c && fp == 1u) a3 = call_set(rt, ctx, slot_of(rt, b, 0xFCu), {b}) != 0u ? 1u : 0u;`
void func_0889E01C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], second = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7], flag = ctx.gpr[8] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    if (call_set(rt, ctx, slot_of(rt, self, 0x20u), {self}) == 0u) return;
    const std::uint32_t fp = call_set(rt, ctx, slot_of(rt, c, 0x60u), {c});
    const std::uint32_t s4 = call_set(rt, ctx, slot_of(rt, c, 0x5Cu), {c});
    const std::uint32_t s3 = call_set(rt, ctx, slot_of(rt, b, 0x30u), {b});
    if (s3 != 0u && (s4 & 0x800u) == 0u) {
        ctx.gpr[2] = s4 & 0x800u;  // the andi v0 of the test: zero on this path
        return;
    }
    std::uint32_t s5 = 1u;
    if (flag != 0u) s5 = s4 & 1u;
    std::uint32_t a3 = 0u;
    const bool direct = (s4 & 0x400u) != 0u && flag != 0u;
    bool check_c = direct;
    if (!direct) {
        (void)call_set(rt, ctx, slot_of(rt, c, 0x10u), {c, frame.at(0x18u)});
        bool bright = false;
        if (fp == 1u) bright = s3 != 0u || call_set(rt, ctx, slot_of(rt, b, 0x34u), {b}) == 0u;
        float f5, f6, f4;
        if (bright) {
            f5 = std::bit_cast<float>(0x40200000u);
            f6 = std::bit_cast<float>(0x3D4CCCCDu);
            f4 = 1.0f;
        } else {
            f5 = f6 = f4 = std::bit_cast<float>(0x3E4CCCCDu);
        }
        float f2 = 0.0f;
        if (flag == 0u) f2 = std::bit_cast<float>(0x3DCCCCCDu);
        put(rt, frame.at(0xCu), word(rt, frame.at(0x18u)));
        put(rt, frame.at(0x14u), word(rt, frame.at(0x20u)));
        put(rt, frame.at(0x10u), word(rt, frame.at(0x1Cu)));
        const float x = f4 < f2 ? f2 : f4;
        const float y = f6 < f2 ? f2 : f6;
        if (!(f5 < f2)) f2 = f5;
        put_float(rt, frame.at(0u), x);
        put_float(rt, frame.at(4u), y);
        put_float(rt, frame.at(8u), f2);
        const std::uint32_t result = call_set(rt, ctx, 0x0889DEE4u, {b, frame.at(0xCu), frame.at(0u)});
        s5 = (result ^ 1u) & s5;
        check_c = flag != 0u;
    }
    if (check_c && fp == 1u) a3 = call_set(rt, ctx, slot_of(rt, b, 0xFCu), {b}) != 0u ? 1u : 0u;
    (void)call_set(rt, ctx, 0x0889A968u, {self + 0x28u, second, b, a3, flag, (s4 >> 4u) & 1u, s5});
}

// func_088AA230(self, f12 = dt): when self[0xC] is null only self[0x38] = 0 happens. Otherwise the virtual slot 0x10 of self fills the vector sp[4..0xC], slot 0x14 (self, self + 0x190) runs, func_08B4B114(self + 0x190, self + 0x150; f12..f14 = the vector) and func_0893C320(self[0xC], self + 0x1A0, + 0x1A4, + 0x1A8, + 0x1AC). With the byte self[0x1B0] == 0 or the byte self[0x38] != 0 func_0894CA04(self[0xC][0xC] + 0x170, sp + 0x10, 0) fills a record that is copied to self[0x1B4] / [0x1B8] (floats), self[0x1BC] / [0x1BE] (halfwords) and self[0x1B0] = 1.
// level = func_0895711C(self[0xC], sp) with sp[0] = 0 (f0): v1 = (0.1f < level), the byte self[0x9C] = v1 and s4 = v1 && the old self[0x9C] was 0 && !(level < func_08B1EFF8(func_08B1ED60(self + 0xA0))). For the four particles k at self + 0x30 + 0x18 * k: an active one (byte +0x20) integrates c = p[0xC] + p[0x1C] * dt, byte +0x18 = (not (0.0f < old p[0xC]) and 0.0f < c), p[0xC] = c, and when 1.0f < c is deactivated (byte +0x20 = 0, word +0xC = 0); an inactive one with s4 set is activated (byte +0x20 = 1) from four draws r = func_08B1EFF8(func_08B1ED60(self + 0xA0)):
// p[0x1C] = 1.0f / (r1 * 0.1f + 0.1f), p[0xC] = -p[0x1C] * (r2 * 0x3CA3D70A + 0x3C23D70A), p[0x14] = r3 * 360.0f, p[0x10] = r4 * 0x3ECCCCCD + 0x3F4CCCCD. Finally self[0x38] = 0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, self + 0xCu) == 0u) {`; `if (byte_u(rt, self + 0x1B0u) == 0u || byte_u(rt, self + 0x38u) != 0u) {`; `if (std::bit_cast<float>(0x3DCCCCCDu) < level) {`; `if (spawn) {`; `if (byte_u(rt, p + 0x20u) != 0u) {`; `if (0.0f < old || c <= 0.0f) active = 0u;`; `if (1.0f < c) {`; `} else if (spawn) {`
void func_088AA230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float dt = ctx.fpr[12];
    GuestFrame frame(ctx, 0x140u);
    if (word(rt, self + 0xCu) == 0u) {
        put8(rt, self + 0x38u, 0u);
        return;
    }
    const std::uint32_t block = self + 0x190u;
    (void)call_set(rt, ctx, slot_of(rt, self, 0x10u), {self, frame.at(4u)});
    (void)call_set(rt, ctx, slot_of(rt, self, 0x14u), {self, block});
    (void)call_setf(rt, ctx, 0x08B4B114u, {block, self + 0x150u}, {float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u)), float_at(rt, frame.at(0xCu))});
    (void)call_set(rt, ctx, 0x0893C320u, {word(rt, self + 0xCu), self + 0x1A0u, self + 0x1A4u, self + 0x1A8u, self + 0x1ACu});
    if (byte_u(rt, self + 0x1B0u) == 0u || byte_u(rt, self + 0x38u) != 0u) {
        const std::uint32_t owner = word(rt, self + 0xCu);
        (void)call_set(rt, ctx, 0x0894CA04u, {word(rt, owner + 0xCu) + 0x170u, frame.at(0x10u), 0u});
        const std::uint32_t half_a = half_u(rt, frame.at(0x18u)), half_b = half_u(rt, frame.at(0x1Au));
        put16(rt, self + 0x1BCu, half_a);
        put(rt, self + 0x1B4u, word(rt, frame.at(0x10u)));
        put(rt, self + 0x1B8u, word(rt, frame.at(0x14u)));
        put16(rt, self + 0x1BEu, half_b);
        put8(rt, self + 0x1B0u, 1u);
    }
    put(rt, frame.at(0u), 0u);  // the delay slot of the jal
    const float level = call_setf(rt, ctx, 0x0895711Cu, {word(rt, self + 0xCu), frame.at(0u)}, {}).f0;
    const std::uint32_t generator = self + 0xA0u;
    const auto draw = [&]() {
        const std::uint32_t state = call_set(rt, ctx, 0x08B1ED60u, {generator});
        return call_setf(rt, ctx, 0x08B1EFF8u, {state}, {}).f0;
    };
    std::uint32_t entered;
    bool spawn;
    if (std::bit_cast<float>(0x3DCCCCCDu) < level) {
        entered = 1u;
        spawn = byte_u(rt, self + 0x9Cu) == 0u;
    } else {
        entered = 0u;
        spawn = false;
    }
    put8(rt, self + 0x9Cu, entered);
    if (spawn) {
        const std::uint32_t state = call_set(rt, ctx, 0x08B1ED60u, {generator});
        const float other = call_setf(rt, ctx, 0x08B1EFF8u, {state}, {}).f0;
        spawn = !(level < other);
    }
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    for (std::uint32_t k = 0u; k != 4u; ++k) {
        const std::uint32_t p = self + 0x30u + 0x18u * k;
        if (byte_u(rt, p + 0x20u) != 0u) {
            const float old = float_at(rt, p + 0xCu);
            const float c = old + psp_mul(float_at(rt, p + 0x1Cu), dt);
            std::uint32_t active = 1u;
            if (0.0f < old || c <= 0.0f) active = 0u;
            put8(rt, p + 0x18u, active);
            put_float(rt, p + 0xCu, c);
            if (1.0f < c) {
                put8(rt, p + 0x20u, 0u);
                put(rt, p + 0xCu, 0u);
            }
        } else if (spawn) {
            put8(rt, p + 0x20u, 1u);
            const float r1 = draw();
            put_float(rt, p + 0x1Cu, 1.0f / (psp_mul(r1, tenth) + tenth));
            const float r2 = draw();
            const float speed = -float_at(rt, p + 0x1Cu);
            put_float(rt, p + 0xCu, psp_mul(psp_mul(r2, std::bit_cast<float>(0x3CA3D70Au)) + std::bit_cast<float>(0x3C23D70Au), speed));
            const float r3 = draw();
            put_float(rt, p + 0x14u, psp_mul(r3, std::bit_cast<float>(0x43B40000u)));
            const float r4 = draw();
            put_float(rt, p + 0x10u, psp_mul(r4, std::bit_cast<float>(0x3ECCCCCDu)) + std::bit_cast<float>(0x3F4CCCCDu));
        }
    }
    put8(rt, self + 0x38u, 0u);
}

// func_088AB884(self, a1, a2, a3): eight entries of 0x44 bytes at self + 0x64 (two points {x, y} at +0x10 and +0x18) and eight of 0x28 bytes at self + 0x284 are reset (entry byte +0x24 = 0, float +0x28 = 2.0f; second list byte +0x1D = 0, float +0x20 = 2.0f) while the bounding box of the sixteen points is taken, starting from the point {self[0x74], self[0x78]}. The block sp = {min x, -max y, max x, -min y, a2, a3, self[0x10], a1} is given to
// func_088AB5D0(sp, 0x088ABD7C (the callback), self), which may set the entries' bytes and floats. Then the best (smallest) float is searched: for i = 0..7 an entry whose byte +0x24 is set and whose word +0x20 is zero offers its float +0x28, a second-list entry whose byte +0x1D is set offers its float +0x20 (as index i + 8); the first offer is taken when it is below 2.0f, later ones when they are strictly below the best.
// No offer: v0 = 0. Otherwise self[0x3C8] = self[4] + best * (self[0x18] - self[4]) and v0 = 1 with: an index j below 8 sets self[0x3E8] = j, self[0x43A] = 1, self[0x3EC] = self[0x98 + 0x44 * j], self[0x3E0] / self[0x3E4] = the floats at self + 0x90 + 0x44 * j and + 4, and the byte self[0x43C] = 1 (j < 4) or self[0x43D] = 1; an index j - 8 = k below 3 or in 4..6 sets self[0x43B] = 1, self[0x3DC] = k, self[0x43C] = 1 (k < 3) and self[0x43D] = 1 (k - 4 unsigned below 3).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, entry + 0x24u) != 0u && word(rt, entry + 0x20u) == 0u) {`; `if (offer < best) {`; `if (byte_u(rt, other + 0x1Du) != 0u) {`; `if (index < 0) {`; `if (index < 8) {`; `if (index < 4) put8(rt, self + 0x43Cu, 1u);`; `if (signed_of(k) < 3) put8(rt, self + 0x43Cu, 1u);`; `if (word(rt, self + 0x3DCu) - 4u < 3u) put8(rt, self + 0x43Du, 1u);`
void func_088AB884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t entries = self + 0x64u, others = self + 0x284u;
    float min_x = float_at(rt, self + 0x74u), max_x = min_x;
    float min_y = float_at(rt, self + 0x78u), max_y = min_y;
    const float two = 2.0f;
    for (std::uint32_t i = 0u; i != 8u; ++i) {
        const std::uint32_t entry = entries + 0x44u * i, other = others + 0x28u * i;
        for (std::uint32_t point = entry + 0x10u; point <= entry + 0x18u; point += 8u) {
            const float x = float_at(rt, point), y = float_at(rt, point + 4u);
            if (x < min_x) min_x = x;
            if (max_x < x) max_x = x;
            if (y < min_y) min_y = y;
            if (max_y < y) max_y = y;
        }
        put8(rt, entry + 0x24u, 0u);
        put_float(rt, entry + 0x28u, two);
        put8(rt, other + 0x1Du, 0u);
        put_float(rt, other + 0x20u, two);
    }
    put_float(rt, frame.at(0x18u), float_at(rt, self + 0x10u));
    put_float(rt, frame.at(0u), min_x);
    put_float(rt, frame.at(4u), -max_y);
    put_float(rt, frame.at(8u), max_x);
    put_float(rt, frame.at(0xCu), -min_y);
    put(rt, frame.at(0x10u), a2);
    put(rt, frame.at(0x14u), a3);
    put(rt, frame.at(0x1Cu), a1);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088AB5D0u, {frame.at(0u), 0x088ABD7Cu, self});
    float best = two;
    std::int32_t index = -1;
    for (std::uint32_t i = 0u; i != 8u; ++i) {
        const std::uint32_t entry = entries + 0x44u * i, other = others + 0x28u * i;
        if (byte_u(rt, entry + 0x24u) != 0u && word(rt, entry + 0x20u) == 0u) {
            const float offer = float_at(rt, entry + 0x28u);
            if (offer < best) {
                best = offer;
                index = static_cast<std::int32_t>(i);
            }
        }
        if (byte_u(rt, other + 0x1Du) != 0u) {
            const float offer = float_at(rt, other + 0x20u);
            if (offer < best) {
                best = offer;
                index = static_cast<std::int32_t>(i + 8u);
            }
        }
    }
    if (index < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float from = float_at(rt, self + 4u);
    put_float(rt, self + 0x3C8u, from + psp_mul(best, float_at(rt, self + 0x18u) - from));
    ctx.gpr[2] = 1u;
    if (index < 8) {
        const std::uint32_t base = static_cast<std::uint32_t>(index) * 0x44u;
        put(rt, self + 0x3E8u, static_cast<std::uint32_t>(index));
        put8(rt, self + 0x43Au, 1u);
        put(rt, self + 0x3ECu, word(rt, self + base + 0x98u));
        put(rt, self + 0x3E0u, word(rt, self + base + 0x80u + 0x10u));
        put(rt, self + 0x3E4u, word(rt, self + base + 0x80u + 0x14u));
        if (index < 4) put8(rt, self + 0x43Cu, 1u);
        else put8(rt, self + 0x43Du, 1u);
        return;
    }
    const std::uint32_t k = static_cast<std::uint32_t>(index) - 8u;
    put8(rt, self + 0x43Bu, 1u);
    put(rt, self + 0x3DCu, k);
    if (signed_of(k) < 3) put8(rt, self + 0x43Cu, 1u);
    if (word(rt, self + 0x3DCu) - 4u < 3u) put8(rt, self + 0x43Du, 1u);
}

// func_088CB9B0(self, n): a ring of 0x24 records of 0x24 bytes at self + 0x640 whose head index is self[0xB50]. i = (self[0xB50] + n) wrapped below 0x24 (one subtraction of 0x24, signed compare), j = i + 1 wrapped the same way; low = float self[0xB58], high = float self[0xB54]. A record's float x = r[0x18] is "inside" when low < x < high. The weights r_i[0x1C] and r_j[0x1C] move 2 % (0x3CA3D70A) of the way
// to 0.0 (inside) or 1.0 (outside) — r_i[0x1C] += ((inside_i ? 0 : 1) - r_i[0x1C]) * 0.02f, the same for r_j — and are clamped to [0, 1]. Then r_i[0x20] = 0.5f for n < 0x1F, ((1 - (n - 0x1E) / 5) * 0.5f) * 0.5f above and ((n / 0.0f) * 0.5f) * 0.5f for a negative n; when i is not a multiple of self[0x600] (mfhi of the signed div; a zero divisor leaves hi = i) r_i[0x20] = (1 - r_i[0x1C]) * that value.
// v0 = self[0x600], v1 = i % self[0x600] (the div's HI).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (!(j < 0x24)) j = i - 0x23;`; `const bool inside_i = low < xi && xi < high;`; `const bool inside_j = low < xj && xj < high;`; `wi = wi + psp_mul((inside_i ? 0.0f : 1.0f) - wi, rate);`; `wj = wj + psp_mul((inside_j ? 0.0f : 1.0f) - wj, rate);`; `if (wi < 0.0f) wi = 0.0f;`; `if (1.0f < wi) wi = 1.0f;`; `if (wj < 0.0f) wj = 0.0f;`; `if (1.0f < wj) wj = 1.0f;`; `if (n < 0) {`
void func_088CB9B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::int32_t n = signed_of(ctx.gpr[5]);
    std::int32_t i = signed_of(word(rt, self + 0xB50u) + ctx.gpr[5]);
    if (!(i < 0x24)) i -= 0x24;
    std::int32_t j = i + 1;
    if (!(j < 0x24)) j = i - 0x23;
    const auto record = [&](std::int32_t index) { return self + 0x640u + 36u * static_cast<std::uint32_t>(index); };
    const std::uint32_t ri = record(i), rj = record(j);
    const std::uint32_t window = self + 0xB50u;  // t0 of the asm
    const float low = float_at(rt, window + 8u), high = float_at(rt, window + 4u);
    const float xi = float_at(rt, ri + 0x18u), xj = float_at(rt, rj + 0x18u);
    const bool inside_i = low < xi && xi < high;
    const bool inside_j = low < xj && xj < high;
    const float rate = std::bit_cast<float>(0x3CA3D70Au);
    float wi = float_at(rt, ri + 0x1Cu);
    wi = wi + psp_mul((inside_i ? 0.0f : 1.0f) - wi, rate);
    float wj = float_at(rt, rj + 0x1Cu);
    wj = wj + psp_mul((inside_j ? 0.0f : 1.0f) - wj, rate);
    if (wi < 0.0f) wi = 0.0f;
    if (1.0f < wi) wi = 1.0f;
    if (wj < 0.0f) wj = 0.0f;
    if (1.0f < wj) wj = 1.0f;
    put_float(rt, ri + 0x1Cu, wi);
    put_float(rt, rj + 0x1Cu, wj);
    float scale;
    if (n < 0) {
        scale = psp_mul(psp_mul(static_cast<float>(n) / 0.0f, 0.5f), 0.5f);
    } else if (n < 0x1F) {
        scale = 0.5f;
    } else {
        scale = psp_mul(psp_mul(1.0f - static_cast<float>(n - 0x1E) / 5.0f, 0.5f), 0.5f);
    }
    put_float(rt, ri + 0x20u, scale);
    const std::uint32_t divisor = word(rt, self + 0x600u);
    const std::uint32_t remainder = mips_hi(static_cast<std::uint32_t>(i), divisor);
    if (remainder != 0u) put_float(rt, ri + 0x20u, psp_mul(1.0f - wi, scale));
    ctx.gpr[2] = divisor;
    ctx.gpr[3] = remainder;
}

// func_08905300(f12 = t, out, obj, arr): out = {x, y} floats, v0 = out. When t > 0 and not above obj[0x14] the search starts from t (f12 = t); otherwise (t <= 0 or obj[0x14] < t) a probe is made first: sp[0x14..0x1C] = arr[0..2] with sp[0x18] += 0.5f, sp[0x50] = 0, sp[0x54] = -1, func_08904A04(obj, sp + 0x20, sp + 0x14), sp[4] = sp[0x50], sp[8] = sp[0x54] and w = func_08904E08(obj, sp + 0x14, sp + 4) (f0, also stored at sp[0xC]); w < 0.0f gives out = {0.0f, -1.0f} and ends,
// otherwise t = w when 0.0f < w, else 0.0f. Then n = halfword obj[0x3C] (zero: out = {0, -1.0f}) and s = func_0890345C(obj) (f12 = t); for i = 0, 1, ...: s = s - i (i even) or s + i (i odd), k = (s < 0 ? s + n : s) mod n, func_08905000(sp + 0xC, obj, arr, obj[0x60] + 40 * k, obj[0x60] + 40 * (k + 1) (0 added instead when k + 1 == n)) fills sp[0xC] and sp[0x10]; when 0.0f <= sp[0x10] the answer is out = {sp[0xC], sp[0x10]} and the loop ends; after n rounds (n re-read from obj[0x3C] every round) out = {0, -1.0f}.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (t <= 0.0f || float_at(rt, obj + 0x14u) < t) {`; `for (std::uint32_t offset = 0u; offset != 0xCu; offset += 4u) put(rt, frame.at(0x14u) + offset, word(r...`; `if (w < 0.0f) {`; `t = (0.0f < w) ? float_at(rt, frame.at(0xCu)) : float_at(rt, frame.at(0u));`; `if (n != 0u) {`; `start = (round & 1u) == 0u ? start - round : start + round;`; `const std::uint32_t wrapped = signed_of(start) < 0 ? start + n : start;`; `const std::uint32_t next = base + ((n == k + 1u) ? 0u : 40u * (k + 1u));`; `if (0.0f <= found) {`; `if (!(signed_of(round) < signed_of(n))) break;`
void func_08905300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], obj = ctx.gpr[5], arr = ctx.gpr[6];
    float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x90u);
    ctx.gpr[2] = out;
    if (t <= 0.0f || float_at(rt, obj + 0x14u) < t) {
        put(rt, frame.at(0x54u), 0xFFFFFFFFu);
        put(rt, frame.at(0x50u), 0u);
        for (std::uint32_t offset = 0u; offset != 0xCu; offset += 4u) put(rt, frame.at(0x14u) + offset, word(rt, arr + offset));
        put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x18u)) + 0.5f);
        (void)call_set(rt, ctx, 0x08904A04u, {obj, frame.at(0x20u), frame.at(0x14u)});
        put(rt, frame.at(4u), word(rt, frame.at(0x50u)));
        put(rt, frame.at(8u), word(rt, frame.at(0x54u)));
        const float w = call_setf(rt, ctx, 0x08904E08u, {obj, frame.at(0x14u), frame.at(4u)}, {}).f0;
        put_float(rt, frame.at(0xCu), w);
        if (w < 0.0f) {
            put_float(rt, out, 0.0f);
            put_float(rt, out + 4u, -1.0f);
            ctx.gpr[2] = out;
            return;
        }
        put_float(rt, frame.at(0u), 0.0f);
        t = (0.0f < w) ? float_at(rt, frame.at(0xCu)) : float_at(rt, frame.at(0u));
        ctx.gpr[2] = out;
    }
    std::uint32_t start = call_setf(rt, ctx, 0x0890345Cu, {obj}, {t}).v0;
    std::uint32_t n = half_u(rt, obj + 0x3Cu);
    if (n != 0u) {
        for (std::uint32_t round = 0u;;) {
            start = (round & 1u) == 0u ? start - round : start + round;
            const std::uint32_t wrapped = signed_of(start) < 0 ? start + n : start;
            const std::uint32_t k = mips_hi(wrapped, n);
            const std::uint32_t base = word(rt, obj + 0x60u);
            ++round;
            const std::uint32_t current = base + 40u * k;
            const std::uint32_t next = base + ((n == k + 1u) ? 0u : 40u * (k + 1u));
            (void)call_set(rt, ctx, 0x08905000u, {frame.at(0xCu), obj, arr, current, next});
            const float found = float_at(rt, frame.at(0x10u));
            if (0.0f <= found) {
                put_float(rt, out + 4u, found);
                put_float(rt, out, float_at(rt, frame.at(0xCu)));
                ctx.gpr[2] = out;
                return;
            }
            n = half_u(rt, obj + 0x3Cu);
            if (!(signed_of(round) < signed_of(n))) break;
        }
    }
    put_float(rt, out + 4u, -1.0f);
    put(rt, out, 0u);
    ctx.gpr[2] = out;
}

// func_089109A0(self, f12 = t) -> f0: a piecewise curve. self[0x40] points to a table of 16-byte records {key, value, a, b} (floats at +0, +4, +8, +0xC); self[0x44] (signed byte) is the number of segments n, self[0x46] (signed byte) the way a segment is evaluated and self[0x47] (signed byte) what happens below the first key. No table: f0 = 0.0f.
// t below the first key: self[0x47] == 1 gives t * (value0 / max(key0, 0x00800000)) (the smallest normal float, the key itself when it is not below it), == 2 evaluates record 0 with the way self[0x46], anything else 0.0f. t not below it: n <= 0 gives the float at table + 16 * n + 4; otherwise the first k in 0..n-1 with t < key[k + 1] evaluates record k with the way self[0x46] and, when none, the value of record n (table + 16 * n + 4).
// The way, with b = r[0xC] and a = r[8]: 0 gives b + t * a, 1 gives b + a * (func_08B847A8(t) * 0x3E9A209B), 2 ends with the tail call func_08B84954(0x40135D8E * (b + t * a)), 3 ends with the tail call func_08B84954(0x40135D8E * (b + a * (func_08B847A8(t) * 0x3E9A209B))), any other 0.0f.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (table == 0u) {`; `if (mode == 1) {`; `} else if (mode < 2) {`; `ctx.fpr[0] = mode == 0 ? b + psp_mul(t, a) : 0.0f;`; `} else if (mode == 2) {`; `} else if (mode == 3) {`; `if (below == 1) {`; `if (!(first_key < tiny)) divisor = first_key;`; `} else if (below == 2) {`; `if (count <= 0) {`
void func_089109A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float t = ctx.fpr[12];
    const std::uint32_t table = word(rt, self + 0x40u);
    if (table == 0u) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    const float scale = std::bit_cast<float>(0x3E9A209Bu), exponent = std::bit_cast<float>(0x40135D8Eu);
    const auto way = [&](std::uint32_t record) {
        const std::int32_t mode = signed_of(sext8(byte_u(rt, self + 0x46u)));
        const float b = float_at(rt, record + 0xCu), a = float_at(rt, record + 8u);
        const auto logarithmic = [&]() {
            const float r = call_keep(rt, ctx, 0x08B847A8u, {}, {t}).f0;
            return b + psp_mul(a, psp_mul(r, scale));
        };
        if (mode == 1) {
            ctx.fpr[0] = logarithmic();
        } else if (mode < 2) {
            ctx.fpr[0] = mode == 0 ? b + psp_mul(t, a) : 0.0f;
        } else if (mode == 2) {
            ctx.fpr[12] = psp_mul(exponent, b + psp_mul(t, a));
            tail_call(ctx, 0x08B84954u, {});
        } else if (mode == 3) {
            const float inner = logarithmic();
            ctx.fpr[12] = psp_mul(inner, exponent);
            tail_call(ctx, 0x08B84954u, {});
        } else {
            ctx.fpr[0] = 0.0f;
        }
    };
    const float first_key = float_at(rt, table);
    if (t < first_key) {
        const std::int32_t below = signed_of(sext8(byte_u(rt, self + 0x47u)));
        if (below == 1) {
            const float tiny = std::bit_cast<float>(0x00800000u);
            float divisor = tiny;
            if (!(first_key < tiny)) divisor = first_key;
            ctx.fpr[0] = psp_mul(t, float_at(rt, table + 4u) / divisor);
        } else if (below == 2) {
            way(table);
        } else {
            ctx.fpr[0] = 0.0f;
        }
        return;
    }
    const std::int32_t count = signed_of(sext8(byte_u(rt, self + 0x44u)));
    if (count <= 0) {
        ctx.fpr[0] = float_at(rt, table + (static_cast<std::uint32_t>(count) << 4u) + 4u);
        return;
    }
    for (std::int32_t k = 0; k != count; ++k) {
        if (t < float_at(rt, table + 16u * static_cast<std::uint32_t>(k + 1))) {
            way(table + 16u * static_cast<std::uint32_t>(k));
            return;
        }
    }
    ctx.fpr[0] = float_at(rt, table + (static_cast<std::uint32_t>(count) << 4u) + 4u);
}

// func_089230A4(self, obj): builds the view of a camera. flags = self[0xC]; the position p = self[0x10] (words at +0, +4, +8: the sp[0], sp[4], sp[8] it is copied to when flags & 2 / flags & 1). An identity 4x4 matrix at sp + 0x20 gets the translation {sp[0], sp[4], sp[8], 1.0f} in its last row (sp + 0x50..0x5C) and is handed to func_08B4E9D4(obj + 0x140, matrix); when self[8][0x1FC] == 1 the matrix is rebuilt with the translation {0, 0x3E851EB8, 0x40266666, 1.0f} and handed again.
// The floats sp[0xC], sp[0x10], sp[0x14] = p[0xC], p[0x10], p[0x14] (sp[0xC] = 0xBD23D70A when self[8][0x1FC] == 1; negated when flags & 0x10 and the byte self[8][0x24C] is set). flags & 4 runs func_08B4F3B0(obj + 0x140, f12 = sp[0x10]), func_08B4F2E4(.., sp[0xC]) and func_08B4F47C(.., sp[0x14]); flags & 8 func_08B4F2E4(.., sp[0xC]) (flags re-read after each group); flags & 0x20 sets obj[0x70] = 2 * func_08B65F0C(f12 = 1.0f / p[0x18]) * 0x42652EE1. func_08B4E9D4(obj, obj + 0x140) ends: the result is its v0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if ((flags & 1u) != 0u) {`; `if (word(rt, scene + 0x1FCu) == 1u) {`; `if (word(rt, scene + 0x1FCu) == 1u) put(rt, frame.at(0xCu), 0xBD23D70Au);`; `if ((flags & 0x10u) != 0u && byte_u(rt, scene + 0x24Cu) != 0u) put_float(rt, frame.at(0xCu), -float_at...`
void func_089230A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5], view = obj + 0x140u;
    GuestFrame frame(ctx, 0x90u);
    std::uint32_t flags = word(rt, self + 0xCu);
    for (std::uint32_t offset = 0u; offset != 0x18u; offset += 4u) put(rt, frame.at(offset), 0u);
    if ((flags & 1u) != 0u) {
        const std::uint32_t position = word(rt, self + 0x10u);
        put(rt, frame.at(4u), word(rt, position + 4u));
        put(rt, frame.at(8u), word(rt, position + 8u));
    }
    if ((flags & 2u) != 0u) put(rt, frame.at(0u), word(rt, word(rt, self + 0x10u)));
    const std::uint32_t matrix = frame.at(0x20u);
    const auto build = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        put_float(rt, frame.at(0x20u), 1.0f);
        put(rt, frame.at(0x30u), 0u);
        put(rt, frame.at(0x40u), 0u);
        put(rt, frame.at(0x24u), 0u);
        put_float(rt, frame.at(0x34u), 1.0f);
        put(rt, frame.at(0x44u), 0u);
        put(rt, frame.at(0x28u), 0u);
        put(rt, frame.at(0x38u), 0u);
        put_float(rt, frame.at(0x48u), 1.0f);
        put(rt, frame.at(0x2Cu), 0u);
        put(rt, frame.at(0x3Cu), 0u);
        put(rt, frame.at(0x4Cu), 0u);
        put(rt, frame.at(0x50u), x);
        put(rt, frame.at(0x54u), y);
        put(rt, frame.at(0x58u), z);
        put_float(rt, frame.at(0x5Cu), 1.0f);
    };
    build(word(rt, frame.at(0u)), word(rt, frame.at(4u)), word(rt, frame.at(8u)));
    (void)call_set(rt, ctx, 0x08B4E9D4u, {view, matrix});
    std::uint32_t scene = word(rt, self + 8u);
    if (word(rt, scene + 0x1FCu) == 1u) {
        build(0u, 0x3E851EB8u, 0x40266666u);
        (void)call_set(rt, ctx, 0x08B4E9D4u, {view, matrix});
        scene = word(rt, self + 8u);
    }
    const std::uint32_t position = word(rt, self + 0x10u);
    put(rt, frame.at(0xCu), word(rt, position + 0xCu));
    put(rt, frame.at(0x14u), word(rt, position + 0x14u));
    put(rt, frame.at(0x10u), word(rt, position + 0x10u));
    if (word(rt, scene + 0x1FCu) == 1u) put(rt, frame.at(0xCu), 0xBD23D70Au);
    flags = word(rt, self + 0xCu);
    if ((flags & 0x10u) != 0u && byte_u(rt, scene + 0x24Cu) != 0u) put_float(rt, frame.at(0xCu), -float_at(rt, frame.at(0xCu)));
    if ((flags & 4u) != 0u) {
        (void)call_setf(rt, ctx, 0x08B4F3B0u, {view}, {float_at(rt, frame.at(0x10u))});
        (void)call_setf(rt, ctx, 0x08B4F2E4u, {view}, {float_at(rt, frame.at(0xCu))});
        (void)call_setf(rt, ctx, 0x08B4F47Cu, {view}, {float_at(rt, frame.at(0x14u))});
        flags = word(rt, self + 0xCu);
    }
    if ((flags & 8u) != 0u) {
        (void)call_setf(rt, ctx, 0x08B4F2E4u, {view}, {float_at(rt, frame.at(0xCu))});
        flags = word(rt, self + 0xCu);
    }
    if ((flags & 0x20u) != 0u) {
        const float focal = 1.0f / float_at(rt, word(rt, self + 0x10u) + 0x18u);
        const float half_angle = call_setf(rt, ctx, 0x08B65F0Cu, {}, {focal}).f0;
        put_float(rt, obj + 0x70u, psp_mul(half_angle + half_angle, std::bit_cast<float>(0x42652EE1u)));
    } else {
        ctx.gpr[2] = 0u;  // the andi v0, v1, 0x20 of the asm: the last call sees v0 = 0
    }
    (void)call_set(rt, ctx, 0x08B4E9D4u, {obj, view});
}

// func_08926EA0(self, f12 = angle, f13 = blend) -> f0: a follower of an angle. When the byte at D_08CF28AC + 0 (0x08CF28AC) is non-zero nothing happens and f0 = self[4]. Otherwise k = 1.0f, or with blend <= 1.0f k = func_08926D84(self + 0x2C, f12 = blend) * 0x3F666666 + 0x3F19999A; self[0x10] = the old self[8] and self[8] = angle; delta = angle - old self[8] wrapped once into [-pi, pi] (pi = 0x40490FDB, 2 pi = 0x40C90FDB) and
// d = angle - self[4] wrapped twice the same way (each wrap is one subtraction or addition of 2 pi when the value is beyond +-pi); g = (delta * 5.0f + d) * clamp(blend, 0, 1) (blend < 0 gives 0 * (delta * 5.0f + d)); func_08926564(self, f12 = k * g) runs; then self[4] += self[0xC] * D_08CDDD68 (the float at 0x08CDDD68) wrapped once into [-pi, pi] and stored; f0 = self[4].
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, 0x08CF28ACu) != 0u) {`; `if (blend <= 1.0f) {`; `if (pi < value) return value - two_pi;`; `if (value < -pi) return value + two_pi;`; `if (blend < 0.0f) {`; `if (!(1.0f < blend)) g = blend;`; `if (pi < heading) {`; `} else if (heading < -pi) {`
void func_08926EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float angle = ctx.fpr[12], blend = ctx.fpr[13];
    if (byte_u(rt, 0x08CF28ACu) != 0u) {
        ctx.fpr[0] = float_at(rt, self + 4u);
        return;
    }
    const float pi = std::bit_cast<float>(0x40490FDBu), two_pi = std::bit_cast<float>(0x40C90FDBu);
    float k = 1.0f;
    if (blend <= 1.0f) {
        ctx.fpr[12] = blend;
        const float shaped = call_keep(rt, ctx, 0x08926D84u, {self + 0x2Cu}, {blend}).f0;
        k = psp_mul(shaped, std::bit_cast<float>(0x3F666666u)) + std::bit_cast<float>(0x3F19999Au);
    }
    const float old_angle = float_at(rt, self + 8u);
    put_float(rt, self + 8u, angle);
    put_float(rt, self + 0x10u, old_angle);
    const auto wrap = [&](float value) {
        if (pi < value) return value - two_pi;
        if (value < -pi) return value + two_pi;
        return value;
    };
    const float scaled = psp_mul(wrap(angle - old_angle), 5.0f);
    const float d = wrap(wrap(angle - float_at(rt, self + 4u)));  // the second wrap changes nothing when the first did not
    {
        const float sum = scaled + d;
        float g;
        if (blend < 0.0f) {
            g = psp_mul(0.0f, sum);
        } else {
            g = 1.0f;
            if (!(1.0f < blend)) g = blend;
            g = psp_mul(g, sum);
        }
        ctx.fpr[12] = psp_mul(k, g);
        (void)call_keep(rt, ctx, 0x08926564u, {self}, {ctx.fpr[12]});
        float heading = float_at(rt, self + 4u) + psp_mul(float_at(rt, self + 0xCu), float_at(rt, 0x08CDDD68u));
        put_float(rt, self + 4u, heading);
        if (pi < heading) {
            heading -= two_pi;
            put_float(rt, self + 4u, heading);
        } else if (heading < -pi) {
            heading += two_pi;
            put_float(rt, self + 4u, heading);
        }
        ctx.fpr[0] = heading;
    }
}

// func_0893B494(self): updates three bytes and a float of the record r = self[8] (s0) from the record q = self[0xC] (s1 = q + 0x270): limit = (float)halfword q[0x23E] * 0x3DD67750; func_0893EB40(q + 0x270) (the "halved" test h, called three times) answers non-zero and limit > r[0x590]: r[0x734] = r[0x735] = 0 and nothing more. Otherwise x = min(r[0x584], 1.0f), f22 = r[0x580] / (r[0x580] + r[0x57C]), f21 = x - f22,
// f20 = (|f21| + f22) halved when the byte r[0x5F9] != 1 and halved again when h answers non-zero; r[0x734] = (min(trunc(f20 * 127.0f * D_08CF2990[0x68]), 0x7F) + r[0x734]) / 2 (the PSP's srl/sra division by 2: toward zero); then with c = max(f21, 0) (0.0f when f21 < 0) f20 = (1 - (1 - c / (1 - f22))^16) * 0x3F19999A + 0x3ECCCCCD (halved when h answers non-zero),
// r[0x735] = (min(trunc(f20 * 127.0f * D_08CF2990[0x6C]), 0x7F when it is >= 0x80) + r[0x735]) / 2, r[0x736] = the low byte of trunc((float)r[0x736] * D_08CF2990[0x70]) when that is below 0x80, else 0x7F, and r[0x5A0] *= D_08CF2990[0x74] (D_08CF2990 = 0x08CF2990). v0 = h's answer on the early exit, otherwise (the last trunc < 0x80).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u && !(limit <= float_at(rt, r + 0x590u))) {`; `if (ctx.gpr[2] != 0u) f20 = psp_mul(f20, 0.5f);`; `const std::uint32_t limited = scaled < 0x80 ? static_cast<std::uint32_t>(scaled) : 0x7Fu;`; `put8(rt, r + 0x736u, third < 0x80 ? (static_cast<std::uint32_t>(third) & 0xFFu) : 0x7Fu);`; `ctx.gpr[2] = third < 0x80 ? 1u : 0u;`
void func_0893B494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t q = word(rt, self + 0xCu), r = word(rt, self + 8u);
    const std::uint32_t halved_object = q + 0x270u;
    const float limit = psp_mul(static_cast<float>(static_cast<std::int32_t>(half_u(rt, q + 0x23Eu))), std::bit_cast<float>(0x3DD67750u));
    const std::uint32_t first = call_set(rt, ctx, 0x0893EB40u, {halved_object});
    float x;
    if (first != 0u && !(limit <= float_at(rt, r + 0x590u))) {
        put8(rt, r + 0x734u, 0u);
        put8(rt, r + 0x735u, 0u);
        return;
    }
    x = float_at(rt, r + 0x584u);
    if (1.0f < x) x = 1.0f;
    const float weight = float_at(rt, r + 0x580u);
    const float f22 = weight / (weight + float_at(rt, r + 0x57Cu));
    const float f21 = x - f22;
    float magnitude = f21;
    if (f21 < 0.0f) magnitude = -f21;
    float f20 = magnitude + f22;
    if (byte_u(rt, r + 0x5F9u) != 1u) f20 = psp_mul(f20, 0.5f);
    ctx.gpr[2] = call_set(rt, ctx, 0x0893EB40u, {halved_object});
    if (ctx.gpr[2] != 0u) f20 = psp_mul(f20, 0.5f);
    const std::uint32_t table = 0x08CF2990u;
    const float full = std::bit_cast<float>(0x42FE0000u);
    const auto half_sum = [](std::uint32_t value, std::uint32_t old) {
        const std::uint32_t sum = value + old;
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(sum + (sum >> 31u)) >> 1);
    };
    {
        const std::uint32_t old = byte_u(rt, r + 0x734u);
        const std::int32_t scaled = static_cast<std::int32_t>(ctx.fpu_float_to_word(psp_mul(psp_mul(f20, full), float_at(rt, table + 0x68u)), 1u));
        put8(rt, r + 0x734u, half_sum(static_cast<std::uint32_t>(std::min(scaled, 0x7F)), old));
    }
    float clamped = 0.0f;
    if (!(f21 < 0.0f)) clamped = f21;
    float curve = 1.0f - (clamped / (1.0f - f22));
    curve = psp_mul(curve, curve);
    curve = psp_mul(curve, curve);
    curve = psp_mul(curve, curve);
    curve = psp_mul(curve, curve);
    f20 = psp_mul(1.0f - curve, std::bit_cast<float>(0x3F19999Au)) + std::bit_cast<float>(0x3ECCCCCDu);
    ctx.gpr[2] = call_set(rt, ctx, 0x0893EB40u, {halved_object});
    if (ctx.gpr[2] != 0u) f20 = psp_mul(f20, 0.5f);
    {
        const std::uint32_t old = byte_u(rt, r + 0x735u);
        const std::uint32_t old_third = byte_u(rt, r + 0x736u);
        const float old_float = float_at(rt, r + 0x5A0u);
        const std::int32_t scaled = static_cast<std::int32_t>(ctx.fpu_float_to_word(psp_mul(psp_mul(f20, full), float_at(rt, table + 0x6Cu)), 1u));
        const std::uint32_t limited = scaled < 0x80 ? static_cast<std::uint32_t>(scaled) : 0x7Fu;
        put8(rt, r + 0x735u, half_sum(limited, old));
        const std::int32_t third = static_cast<std::int32_t>(ctx.fpu_float_to_word(psp_mul(static_cast<float>(static_cast<std::int32_t>(old_third)), float_at(rt, table + 0x70u)), 1u));
        put8(rt, r + 0x736u, third < 0x80 ? (static_cast<std::uint32_t>(third) & 0xFFu) : 0x7Fu);
        put_float(rt, r + 0x5A0u, psp_mul(old_float, float_at(rt, table + 0x74u)));
        ctx.gpr[2] = third < 0x80 ? 1u : 0u;
    }
}

// func_0893CBCC(self): r = self[8] (s1), q = self[0xC] (s4); nothing happens when the byte r[0x69C] is zero or func_089557BC() answers non-zero. Otherwise func_089547A0(self), func_0893CA3C(self; f12 = r[0x6A8]) and r[0x69C] = func_0893C870(self, r[0x69C] << 4) & 0xFF; when its low nibble is non-zero: for i = 0..3 d_i = q[0x364 + 116 * (i >> 1)] - func_08955300(self, i)[0xC8] (0 when negative) fill sp[0..3];
// with a = d0, b = d1, c = d2, e = d3: x = a + (c - a) * q[0x20], y = b + (e - b) * q[0x20], u = (a + b) * 0.5f, w = (c + e) * 0.5f. The sums g (starts 0.0f) and r[0x700] / r[0x6FC] take func_08B4A430(x - y; f13 = q[0x1C]) when x differs from y (x > y: g = y * 0.5f, x < y: g = x * 0.5f, else g = x * 0.5f) and the same with u, w (f13 = q[0x18]), g += the smaller half of u / w; r[0x18] += g (skipped when u and w are both 0.0f).
// Then func_0893C3E4(r + 0x1C, r + 0x28, r + 0x34, r + 0x6FC, r + 0x700, r + 0x704) runs, p = min(r[0x54] * sum(r[0x6CC + 4 * j] * r[0x4C + 4 * j]) + r[0x60] * sum(r[0x6CC + 4 * j] * r[0x58 + 4 * j]), 0.0f) (j = 0..2) and r[0x6D4] = p when it is above the old value.
// Not registered: 3000 dense race cases compared equal (0 mismatches) and none of them changed memory, so the paths that write are proven only by reading them against the asm by hand (the gate cannot see them).
[[maybe_unused]] void func_0893CBCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t r = word(rt, self + 8u);
    if (byte_u(rt, r + 0x69Cu) == 0u) return;
    const std::uint32_t q = word(rt, self + 0xCu);
    if (call_set(rt, ctx, 0x089557BCu, {self}) != 0u) return;
    (void)call_set(rt, ctx, 0x089547A0u, {self});
    (void)call_setf(rt, ctx, 0x0893CA3Cu, {self}, {float_at(rt, r + 0x6A8u)});
    const std::uint32_t nibbles = call_set(rt, ctx, 0x0893C870u, {self, byte_u(rt, r + 0x69Cu) << 4u}) & 0xFFu;
    put8(rt, r + 0x69Cu, nibbles);
    if ((nibbles & 0xFu) != 0u) {
        const auto sp = [&](std::uint32_t index) { return float_at(rt, frame.at(4u * index)); };
        float g = 0.0f;
        for (std::uint32_t i = 0u; i != 4u; ++i) {
            const std::uint32_t row = q + 116u * (i >> 1u);
            const float base = float_at(rt, row + 0x364u);
            const std::uint32_t entry = call_set(rt, ctx, 0x08955300u, {self, i});
            const float d = base - float_at(rt, entry + 0xC8u);
            if (d < 0.0f) put(rt, frame.at(4u * i), 0u);
            else put_float(rt, frame.at(4u * i), d);
        }
        const float a = sp(0u), b = sp(1u), c = sp(2u), e = sp(3u);
        const float rate = float_at(rt, q + 0x20u);
        const float half = std::bit_cast<float>(0x3F000000u);
        const float u = psp_mul(a + b, half), w = psp_mul(c + e, half);
        const float x = a + psp_mul(c - a, rate);
        const float y = b + psp_mul(rate, e - b);
        const float zero = 0.0f;
        bool skip_first = false;
        if (x == zero && y == zero) {
            g = 0.0f;
            skip_first = true;
        }
        if (!skip_first) {
            if (y < x) {
                const float r1 = call_setf(rt, ctx, 0x08B4A430u, {}, {x - y, float_at(rt, q + 0x1Cu)}).f0;
                put_float(rt, r + 0x700u, r1 + float_at(rt, r + 0x700u));
                g = psp_mul(y, half);
            } else if (x < y) {
                const float r1 = call_setf(rt, ctx, 0x08B4A430u, {}, {x - y, float_at(rt, q + 0x1Cu)}).f0;
                put_float(rt, r + 0x700u, r1 + float_at(rt, r + 0x700u));
                g = psp_mul(x, half);
            } else {
                g = psp_mul(x, half);
            }
        }
        float adjust = 0.0f;
        bool apply = true;
        if (u == zero && w == zero) apply = false;
        if (apply) {
            if (w < u) {
                const float r2 = call_setf(rt, ctx, 0x08B4A430u, {}, {u - w, float_at(rt, q + 0x18u)}).f0;
                put_float(rt, r + 0x6FCu, r2 + float_at(rt, r + 0x6FCu));
                adjust = psp_mul(w, half);
            } else if (u < w) {
                const float r2 = call_setf(rt, ctx, 0x08B4A430u, {}, {u - w, float_at(rt, q + 0x18u)}).f0;
                put_float(rt, r + 0x6FCu, r2 + float_at(rt, r + 0x6FCu));
                adjust = psp_mul(u, half);
            } else {
                adjust = psp_mul(u, half);
            }
            g = g + adjust;
        }
        put_float(rt, r + 0x18u, g + float_at(rt, r + 0x18u));
    }
    (void)call_set(rt, ctx, 0x0893C3E4u, {r + 0x1Cu, r + 0x28u, r + 0x34u, r + 0x6FCu, r + 0x700u, r + 0x704u});
    float sum_a = 0.0f, sum_b = 0.0f;
    for (std::uint32_t j = 0u; j != 3u; ++j) {
        const float f0 = float_at(rt, r + 0x6CCu + 4u * j);
        const float f1 = psp_mul(f0, float_at(rt, r + 0x4Cu + 4u * j));
        const float f2 = psp_mul(f0, float_at(rt, r + 0x58u + 4u * j));
        sum_a = sum_a + f1;
        sum_b = sum_b + f2;
    }
    float p = psp_mul(float_at(rt, r + 0x54u), sum_a) + psp_mul(sum_b, float_at(rt, r + 0x60u));
    if (0.0f < p) p = 0.0f;
    if (float_at(rt, r + 0x6D4u) < p) put_float(rt, r + 0x6D4u, p);
}

// func_0894CA04(obj, out, a2): fills the record out. n = the signed halfword obj[0]; for the entries i < n: v = trunc(obj[4 * i + 0xC] * 60.0f / 6.28318548f) (0x42700000, 0x40C90FDB) rounded to the hundreds, ((v + 50) / 100) * 100 (the PSP's multiply-high division); it is stored as the next halfword out[0x10 + 2 * m] when m is 0 or the previous stored halfword is below it (m = the number stored so far); out[0xE] = m.
// last = the signed halfword out[0xE + 2 * m ... ] (the last stored, out[0xE] itself when none), top = max(obj[0xCC] (halfword), last), s0 = the signed halfword out[0x10], out[0xC] = obj[0xCC]. When top < s0: out[0] = out[4] = 0.0f and out[8] = out[0xA] = 0. Otherwise for s = s0, s0 + 100, .. while s <= top: f21 = (float)s * 0x3DD67750, g = func_0894BF60(obj, a2; f12 = f21), h = func_0894BFF4(obj; f12 = g, f13 = f21) / 9.8f (0x411CCCCD), w = ((float)s * h) / 0x44330CCD;
// when s equals the next stored halfword (out + 0x10 + 2 * j) out[0x40 + 4 * j] = w and out[0xA0 + 4 * j] = h (j counts the matches); when out[0xC] (unsigned) is not below s: the best h is kept (h > best: out[4] = h, out[0xA] = s) and the best w (w > best: out[0] = w, out[8] = s).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (n > 0) {`; `if (kept <= 0u || signed_of(sext16(half_u(rt, out + 0xEu + 2u * kept))) < signed_of(rounded)) {`; `if (top < start) {`; `if (static_cast<std::int32_t>(half_u(rt, out + 0xCu)) < start) continue;`
void func_0894CA04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4], out = ctx.gpr[5], arg = ctx.gpr[6];
    const std::int32_t n = signed_of(sext16(half_u(rt, obj)));
    std::uint32_t kept = 0u;
    if (n > 0) {
        const float sixty = std::bit_cast<float>(0x42700000u), two_pi = std::bit_cast<float>(0x40C90FDBu);
        for (std::int32_t i = 0; i < n; ++i) {
            const float angle = psp_mul(float_at(rt, obj + 0xCu + 4u * static_cast<std::uint32_t>(i)), sixty) / two_pi;
            const std::uint32_t truncated = ctx.fpu_float_to_word(angle, 1u);
            const std::uint32_t biased = truncated + 0x32u;
            const std::int64_t product = static_cast<std::int64_t>(signed_of(biased)) * static_cast<std::int64_t>(0x51EB851F);
            const std::int32_t high = static_cast<std::int32_t>(product >> 32) >> 5;
            const std::int32_t quotient = high - (signed_of(biased) >> 31);
            const std::uint32_t rounded = static_cast<std::uint32_t>(quotient) * 100u;
            if (kept <= 0u || signed_of(sext16(half_u(rt, out + 0xEu + 2u * kept))) < signed_of(rounded)) {
                put16(rt, out + 0x10u + 2u * kept, rounded);
                ++kept;
            }
        }
    }
    put16(rt, out + 0xEu, kept);
    const std::uint32_t last = sext16(half_u(rt, out + (kept - 1u) * 2u + 0x10u));
    const std::uint32_t limit = half_u(rt, obj + 0xCCu);
    std::int32_t start = signed_of(sext16(half_u(rt, out + 0x10u)));
    const std::int32_t top = std::max(signed_of(limit), signed_of(last));
    put16(rt, out + 0xCu, limit);
    if (top < start) {
        put_float(rt, out + 4u, 0.0f);
        put_float(rt, out, 0.0f);
        put16(rt, out + 8u, 0u);
        put16(rt, out + 0xAu, 0u);
        return;
    }
    float best_h = 0.0f, best_w = 0.0f;
    std::uint32_t best_h_at = 0u, best_w_at = 0u;
    std::uint32_t write = out;
    std::uint32_t match = out + 0x10u;
    const float scale = std::bit_cast<float>(0x3DD67750u), gravity = std::bit_cast<float>(0x411CCCCDu), divisor = std::bit_cast<float>(0x44330CCDu);
    for (; !(top < start); start += 100) {
        const float f20 = static_cast<float>(start);
        const float f21 = psp_mul(f20, scale);
        const float g = call_setf(rt, ctx, 0x0894BF60u, {obj, arg}, {f21}).f0;
        const float h = call_setf(rt, ctx, 0x0894BFF4u, {obj}, {g, f21}).f0 / gravity;
        const float w = psp_mul(f20, h) / divisor;
        if (signed_of(sext16(half_u(rt, match))) == start) {
            put_float(rt, write + 0x40u, w);
            match += 2u;
            put_float(rt, write + 0xA0u, h);
            write += 4u;
        }
        if (static_cast<std::int32_t>(half_u(rt, out + 0xCu)) < start) continue;  // slt: a signed compare of the zero-extended halfword
        if (best_h < h) {
            best_h = h;
            best_h_at = static_cast<std::uint32_t>(start);
        }
        if (best_w < w) {
            best_w = w;
            best_w_at = static_cast<std::uint32_t>(start);
        }
    }
    put16(rt, out + 0xAu, best_h_at);
    put_float(rt, out, best_w);
    put16(rt, out + 8u, best_w_at);
    put_float(rt, out + 4u, best_h);
}

// func_0894F1C8(out, src, flag): converts the byte record src into the float record out. With u(k) = (float)src[k] and the scales 0.01f (0x3C23D70A) / 0.1f (0x3DCCCCCD): out[0] = u(8) * 0.01f, out[4] = u(9) * 0.01f, out[8] = u(0xA) * 0.01f, the bytes out[0xC..0xF] = bits 0..3 of src[5]; out[0x10] = u(0xC) * 0.01f, out[0x14] = u(0xD) * 0.01f, out[0x18] = u(0xE) * 0.01f, the bytes out[0x1C..0x1F] = bits 4..7 of src[5]; out[0x24] = u(0) * 0.1f, out[0x28] = u(1) * 0.01f, out[0x20] = u(2) * 0.1f, out[0x2C] = u(3) * 0.01f.
// The bytes out[0x30] / out[0x32] = (the low halfword of the word out[0xC] / out[0x1C] is non-zero and out[0] / out[0x10] != 0.0f), out[0x31] / out[0x33] = (the high halfword of that word is non-zero and out[8] / out[0x18] != 0.0f). Then, when func_08945628() answers non-zero and (flag & 0xFF) == 0: out[0x24] = func_0894F130() when src[0] != 0, else the integer 0, and out[0x28] = u(1) * 0x3E8E38E4.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if ((flags & 0xFFFFu) != 0u && !(first == 0.0f)) low = 1u;`; `if ((flags >> 16u) != 0u && !(float_at(rt, out + second_offset) == 0.0f)) high = 1u;`; `if (ready == 0u || flag != 0u) return;`; `if (first_byte != 0u) {`
void func_0894F1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], src = ctx.gpr[5], flag = ctx.gpr[6] & 0xFFu;
    const auto u = [&](std::uint32_t offset) { return static_cast<float>(static_cast<std::int32_t>(byte_u(rt, src + offset))); };
    const float hundredth = std::bit_cast<float>(0x3C23D70Au), tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const std::uint32_t bits = byte_u(rt, src + 5u);
    const float f3 = psp_mul(u(8u), hundredth);
    put_float(rt, out, f3);
    put_float(rt, out + 4u, psp_mul(u(9u), hundredth));
    put8(rt, out + 0xCu, bits & 1u);
    put8(rt, out + 0xDu, (bits >> 1u) & 1u);
    put8(rt, out + 0xEu, (bits >> 2u) & 1u);
    put8(rt, out + 0xFu, (bits >> 3u) & 1u);
    put_float(rt, out + 8u, psp_mul(u(0xAu), hundredth));
    const auto summaries = [&](std::uint32_t flags_offset, std::uint32_t second_offset, std::uint32_t low_flag, std::uint32_t high_flag, float first) {
        const std::uint32_t flags = word(rt, out + flags_offset);
        std::uint32_t low = 0u;
        if ((flags & 0xFFFFu) != 0u && !(first == 0.0f)) low = 1u;
        put8(rt, out + low_flag, low);
        std::uint32_t high = 0u;
        if ((flags >> 16u) != 0u && !(float_at(rt, out + second_offset) == 0.0f)) high = 1u;
        put8(rt, out + high_flag, high);
    };
    summaries(0xCu, 8u, 0x30u, 0x31u, f3);
    const float f3b = psp_mul(u(0xCu), hundredth);
    put_float(rt, out + 0x10u, f3b);
    put_float(rt, out + 0x14u, psp_mul(u(0xDu), hundredth));
    put8(rt, out + 0x1Cu, (bits >> 4u) & 1u);
    put8(rt, out + 0x1Du, (bits >> 5u) & 1u);
    put8(rt, out + 0x1Eu, (bits >> 6u) & 1u);
    put8(rt, out + 0x1Fu, bits >> 7u);
    put_float(rt, out + 0x18u, psp_mul(u(0xEu), hundredth));
    summaries(0x1Cu, 0x18u, 0x32u, 0x33u, f3b);
    put_float(rt, out + 0x24u, psp_mul(u(0u), tenth));
    put_float(rt, out + 0x28u, psp_mul(u(1u), hundredth));
    put_float(rt, out + 0x20u, psp_mul(u(2u), tenth));
    put_float(rt, out + 0x2Cu, psp_mul(u(3u), hundredth));  // the delay slot of the jal
    const std::uint32_t ready = call_set(rt, ctx, 0x08945628u, {});
    if (ready == 0u || flag != 0u) return;
    const std::uint32_t first_byte = byte_u(rt, src);
    if (first_byte != 0u) {
        ctx.gpr[4] = first_byte;  // the callee converts its a0 (the byte just loaded)
        const float value = call_keep(rt, ctx, 0x0894F130u, {first_byte}, {}).f0;
        put_float(rt, out + 0x24u, value);
    } else {
        put(rt, out + 0x24u, 0u);
    }
    put_float(rt, out + 0x28u, psp_mul(u(1u), std::bit_cast<float>(0x3E8E38E4u)));
}

// func_08954898(): a leaf that converts the six 8-byte records at D_08CDE160 (0x08CDE160, bytes b0..b7) into the six 0x1C-byte float records at D_08CF334C (0x08CF334C): +0 = 10 * b0 * 0x3E8E38E4, +4 = 10 * b1 * 0x3E8E38E4, +8 = 5 * b2 * 0x3A83126F, +0xC = (signed b3) * 0x3C8EFA35, +0x10 = b5 / 100.0f, +0x14 = b6 * 0x3A83126F, +0x18 = b7 * 0x3A83126F; then from the bytes at D_08CDE160 + 0x30..0x32 the floats +0xA8 = b30 * 0x3A83126F, +0xAC = b31 * 0x3C8EFA35, +0xB0 = b32 * 0x3A83126F, +0xB8 = record0[8], and
// +0xB4 = 0 (the float 0.0 bits) when d = record2[8] - record0[8] is 0.0f, else 1.0f / d; +0xBC = the word at +0xC, +0xC0 = the word at +0x7C, +0xC4 = the word at +0x44, +0xC8 = the word at +0x60 (copies of words the loop stored); then for i = 0..2 (p = 0x08CF340C + 4 * i): d = p[0] - p[-4]; p[0x18] = 0 (the integer 0) when d == 0.0f else 1.0f / (d * 3.0f); p[0xC] = (float)i / 3.0f; the last store is +0xD8 = 1.0f (it overwrites i = 0's p[0x18]). v0 = 0x08CF334C.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (d0 == 0.0f) put_float(rt, out + 0xB4u, 0.0f);`; `if (d == 0.0f) put(rt, p + 0x18u, 0u);`
void func_08954898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = 0x08CDE160u, out = 0x08CF334Cu;
    const float k_ten = std::bit_cast<float>(0x3E8E38E4u), k_milli = std::bit_cast<float>(0x3A83126Fu), k_angle = std::bit_cast<float>(0x3C8EFA35u);
    for (std::uint32_t i = 0u; i != 6u; ++i) {
        const std::uint32_t in = table + 8u * i, o = out + 0x1Cu * i;
        const auto u8 = [&](std::uint32_t k) { return static_cast<float>(static_cast<std::int32_t>(byte_u(rt, in + k))); };
        const float f6 = u8(5u) / 100.0f;
        const float f1 = static_cast<float>(static_cast<std::int32_t>(sext8(byte_u(rt, in + 3u))));
        put_float(rt, o + 0u, psp_mul(static_cast<float>(static_cast<std::int32_t>(10u * byte_u(rt, in + 0u))), k_ten));
        put_float(rt, o + 4u, psp_mul(static_cast<float>(static_cast<std::int32_t>(10u * byte_u(rt, in + 1u))), k_ten));
        put_float(rt, o + 8u, psp_mul(static_cast<float>(static_cast<std::int32_t>(5u * byte_u(rt, in + 2u))), k_milli));
        put_float(rt, o + 0xCu, psp_mul(f1, k_angle));
        put_float(rt, o + 0x10u, f6);
        put_float(rt, o + 0x14u, psp_mul(u8(6u), k_milli));
        put_float(rt, o + 0x18u, psp_mul(u8(7u), k_milli));
    }
    const auto byte_float = [&](std::uint32_t k) { return static_cast<float>(static_cast<std::int32_t>(byte_u(rt, table + k))); };
    const float f6 = float_at(rt, out + 8u);
    const float d0 = float_at(rt, out + 0x40u) - f6;
    put_float(rt, out + 0xACu, psp_mul(byte_float(0x31u), k_angle));
    put_float(rt, out + 0xA8u, psp_mul(byte_float(0x30u), k_milli));
    put_float(rt, out + 0xB0u, psp_mul(byte_float(0x32u), k_milli));
    put_float(rt, out + 0xB8u, f6);
    if (d0 == 0.0f) put_float(rt, out + 0xB4u, 0.0f);
    else put_float(rt, out + 0xB4u, 1.0f / d0);
    const std::uint32_t w_0c = word(rt, out + 0xCu), w_7c = word(rt, out + 0x7Cu), w_44 = word(rt, out + 0x44u), w_60 = word(rt, out + 0x60u);
    put(rt, out + 0xC8u, w_60);
    put(rt, out + 0xBCu, w_0c);
    put(rt, out + 0xC0u, w_7c);
    put(rt, out + 0xC4u, w_44);
    std::uint32_t p = 0x08CF340Cu;
    for (std::uint32_t i = 0u; i != 3u; ++i, p += 4u) {
        const float d = float_at(rt, p) - float_at(rt, p - 4u);
        const float scaled = psp_mul(d, 3.0f);
        if (d == 0.0f) put(rt, p + 0x18u, 0u);
        else put_float(rt, p + 0x18u, 1.0f / scaled);
        put_float(rt, p + 0xCu, static_cast<float>(static_cast<std::int32_t>(i)) / 3.0f);
    }
    put_float(rt, out + 0xD8u, 1.0f);
    ctx.gpr[2] = out;
}

// func_08956930(self): when func_0894CCB8(self) answers zero the three calls of the "else" ending run: func_0895469C(self), func_08960EF0(self) and the tail call func_0893C968(self). Otherwise v = self[8] and f21 = v[0x3C] when v[0x6C] < 0.5f, else (v[0x34] * v[0x64] + v[0x38] * v[0x68]) + v[0x6C] * v[0x3C]; the state byte b = v[0x74B] (bit 2, bit 4):
// without bit 2: f21 < 0.1f (0x3DCCCCCD) or (f21 < 0.15f (0x3E19999A) and the sum of func_08955300(self, i)[0x54] over i = 0..3 equal to 0.0f) sets bits 2 and 4 (b | 6), runs func_089557CC(self, 0x78) and reloads b. With bit 2: bit 4 set and f21 > 0.5f clears bit 4 (the "else" ending), bit 4 set otherwise the tail call func_0893E0A4(self); bit 4 clear and f21 < 0.15f sets it (the tail call), else the "else" ending.
// Without bit 2 (after the above) the tail call func_0893E0A4(self) runs when bit 4 is set, else the "else" ending. v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, 0x0894CCB8u, {self}) == 0u) {`; `const std::uint32_t v = word(rt, self + 8u), flags_address = v + 0x74Bu;`; `if (f5 < 0.5f) {`; `if ((b & 2u) == 0u) {`; `if (f21 < std::bit_cast<float>(0x3DCCCCCDu)) {`; `} else if (f21 < std::bit_cast<float>(0x3E19999Au)) {`; `for (std::uint32_t i = 0u; i != 4u; ++i) {`; `if (sum == 0.0f) set = true;`; `if (set) {`; `if ((b & 2u) != 0u) {`; `if ((b & 4u) != 0u) {`; `if (std::bit_cast<float>(0x3F000000u) < f21) {`; `if (finish) {`
void func_08956930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (call_set(rt, ctx, 0x0894CCB8u, {self}) == 0u) {
        (void)call_set(rt, ctx, 0x0895469Cu, {self});
        (void)call_set(rt, ctx, 0x08960EF0u, {self});
        ctx.gpr[4] = self;
        tail_call(ctx, 0x0893C968u, {self});
        return;
    }
    const std::uint32_t v = word(rt, self + 8u), flags_address = v + 0x74Bu;
    const float f5 = float_at(rt, v + 0x6Cu);
    float f21;
    if (f5 < 0.5f) {
        f21 = float_at(rt, v + 0x3Cu);
    } else {
        const float a = psp_mul(float_at(rt, v + 0x34u), float_at(rt, v + 0x64u));
        const float b2 = psp_mul(float_at(rt, v + 0x38u), float_at(rt, v + 0x68u));
        f21 = (a + b2) + psp_mul(f5, float_at(rt, v + 0x3Cu));
    }
    std::uint32_t b = byte_u(rt, flags_address);
    if ((b & 2u) == 0u) {
        bool set = false;
        if (f21 < std::bit_cast<float>(0x3DCCCCCDu)) {
            set = true;
        } else if (f21 < std::bit_cast<float>(0x3E19999Au)) {
            float sum = 0.0f;
            for (std::uint32_t i = 0u; i != 4u; ++i) {
                const std::uint32_t entry = call_set(rt, ctx, 0x08955300u, {self, i});
                sum = sum + float_at(rt, entry + 0x54u);
            }
            b = byte_u(rt, flags_address);
            if (sum == 0.0f) set = true;
        }
        if (set) {
            put8(rt, flags_address, b | 6u);
            (void)call_set(rt, ctx, 0x089557CCu, {self, 0x78u});
            b = byte_u(rt, flags_address);
        }
    }
    bool finish;
    if ((b & 2u) != 0u) {
        if ((b & 4u) != 0u) {
            if (std::bit_cast<float>(0x3F000000u) < f21) {
                b ^= 4u;
                put8(rt, flags_address, b);
                finish = (b & 4u) != 0u;
            } else {
                finish = true;
            }
        } else if (f21 < std::bit_cast<float>(0x3E19999Au)) {
            b |= 4u;
            put8(rt, flags_address, b);
            finish = true;
        } else {
            finish = (b & 4u) != 0u;
        }
    } else {
        finish = (b & 4u) != 0u;
    }
    ctx.gpr[4] = self;
    if (finish) {
        tail_call(ctx, 0x0893E0A4u, {self});
        return;
    }
    (void)call_set(rt, ctx, 0x0895469Cu, {self});
    (void)call_set(rt, ctx, 0x08960EF0u, {self});
    ctx.gpr[4] = self;
    tail_call(ctx, 0x0893C968u, {self});
}

// func_08956B20(self): r = self[8] (s4), q = self[0xC] (s6), t = func_089551B0(self) (the record s1; the delay slot of the jal that follows copies the first call's v0), k = func_0895549C(self) (f0, f20). The angle r[0x704] is wrapped once into [-pi, pi] (pi = 0x40490FDB, 2 pi = 0x40C90FDB) and t[0x3C] gets the same 2 pi; when func_0894CCB8() answers non-zero the two angles r[0x6FC], r[0x700] are wrapped the same way together with t[0x34], t[0x38].
// For each of the four entries i = 0..3: o = func_08955300(self, i); a = (o[0x34] * q[0x320 + 4 * (i >> 1)]) * k, or 0.0f when the byte r[0xF] is set; a += o[0x20] is wrapped (up to twice, by 2 pi, adding the same 2 pi to t[0x88 + 4 * i]) and stored in o[0x20]. Then t[0x1C], t[0x20..0x30] = t[0], t[4..0x14] and t[0x108] = t[0x18] (words), func_08956540(self) runs and the tail call func_089503E0(self) ends.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (pi <= angle) {`; `} else if (angle <= -pi) {`; `if (call_set(rt, ctx, 0x0894CCB8u, {}) != 0u) {`; `for (std::uint32_t i = 0u; i != 2u; ++i) {`; `if (pi <= value) {`; `} else if (value <= -pi) {`; `for (std::uint32_t round = 0u; round != 2u; ++round) {`; `} else if (a <= -pi) {`
void func_08956B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t r = word(rt, self + 8u);
    const std::uint32_t t = call_set(rt, ctx, 0x089551B0u, {self});
    const std::uint32_t q = word(rt, self + 0xCu);  // the delay slot of the jal
    const float k = call_keep(rt, ctx, 0x0895549Cu, {self}, {}).f0;
    const float pi = std::bit_cast<float>(0x40490FDBu), two_pi = std::bit_cast<float>(0x40C90FDBu);
    float angle = float_at(rt, r + 0x704u);
    if (pi <= angle) {
        angle = angle - two_pi;
        put_float(rt, r + 0x704u, angle);
        put_float(rt, t + 0x3Cu, float_at(rt, t + 0x3Cu) - two_pi);
    } else if (angle <= -pi) {
        angle = angle + two_pi;
        put_float(rt, r + 0x704u, angle);
        put_float(rt, t + 0x3Cu, float_at(rt, t + 0x3Cu) + two_pi);
    }
    if (call_set(rt, ctx, 0x0894CCB8u, {}) != 0u) {
        for (std::uint32_t i = 0u; i != 2u; ++i) {
            const float value = float_at(rt, r + 0x6FCu + 4u * i);
            if (pi <= value) {
                put_float(rt, r + 0x6FCu + 4u * i, value - two_pi);
                put_float(rt, t + 0x34u + 4u * i, float_at(rt, t + 0x34u + 4u * i) - two_pi);
            } else if (value <= -pi) {
                put_float(rt, r + 0x6FCu + 4u * i, value + two_pi);
                put_float(rt, t + 0x34u + 4u * i, float_at(rt, t + 0x34u + 4u * i) + two_pi);
            }
        }
    }
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t entry = call_set(rt, ctx, 0x08955300u, {self, i});
        const float weight = float_at(rt, q + ((i >> 1u) << 2u) + 0x320u);
        const float scaled = psp_mul(float_at(rt, entry + 0x34u), weight);
        float a = psp_mul(k, scaled);
        if (byte_u(rt, r + 0xFu) != 0u) a = 0.0f;
        a = a + float_at(rt, entry + 0x20u);
        for (std::uint32_t round = 0u; round != 2u; ++round) {
            if (pi <= a) {
                a = a - two_pi;
                put_float(rt, t + 4u * i + 0x88u, float_at(rt, t + 4u * i + 0x88u) - two_pi);
            } else if (a <= -pi) {
                a = a + two_pi;
                put_float(rt, t + 4u * i + 0x88u, float_at(rt, t + 4u * i + 0x88u) + two_pi);
            } else {
                break;
            }
        }
        put_float(rt, entry + 0x20u, a);
    }
    const std::uint32_t w1 = word(rt, t), w2 = word(rt, t + 4u), w3 = word(rt, t + 8u), w4 = word(rt, t + 0xCu), w5 = word(rt, t + 0x10u), w6 = word(rt, t + 0x14u), w0 = word(rt, t + 0x18u);
    put(rt, t + 0x1Cu, w1);
    put(rt, t + 0x108u, w0);
    put(rt, t + 0x20u, w2);
    put(rt, t + 0x24u, w3);
    put(rt, t + 0x28u, w4);
    put(rt, t + 0x2Cu, w5);
    put(rt, t + 0x30u, w6);
    (void)call_set(rt, ctx, 0x08956540u, {self});
    ctx.gpr[4] = self;
    tail_call(ctx, 0x089503E0u, {self});
}

// func_0895B8C4(self, f12 = x) -> v0: 0 when func_0895B59C(self) answers zero, otherwise 1 after two weights are stored. r = self[8] (s0), q = self[0xC] (s2), w = r + 0x778 (s3, w[4] = w[8] = 0 first), T = D_08CF35A0 (0x08CF35A0). func_08B498AC(sp + 4, sp, f12 = r[0x51C]) fills the pair {sp[0], sp[4]}. With T[0x2C] != 0: t = x when T[0x30] != 0, else r[0x6F8];
// u = (r[0x6C4] * sp[0] - (r[0x6C8] - t * q[0x10]) * sp[4]) - r[0x6C4], d = (r[0x5FC] * q[0x318] - r[0x600] * q[0x31C]) - u (q[0x318], q[0x31C] are the floats at q + 0x270 + 0xA8 and + 0xAC); w[4] = 1.0f when T[0x38] <= d, 0.0f when d <= -T[0x34], else (d + T[0x34]) / (T[0x38] + T[0x34]).
// With T[0x3C] != 0: s = x when T[0x40] != 0, else r[0x6F8]; e = (s + s) * q[0x2C], a = func_08955300(self, 2)[0x34], b = func_08955300(self, 3)[0x34] - a; a negative e is negated together with b; d2 = b - e: w[8] = 1.0f when T[0x48] <= d2, 0.0f when d2 <= -T[0x44], else (d2 + T[0x44]) / (T[0x48] + T[0x44]). v0 = 1 on every path that gets past the first call.
// Not registered: 3000 dense race cases compared equal (0 mismatches) and none of them changed memory, so the paths that write are proven only by reading them against the asm by hand (the gate cannot see them).
[[maybe_unused]] void func_0895B8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x40u);
    if (call_set(rt, ctx, 0x0895B59Cu, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[3] = 0u;
    const std::uint32_t r = word(rt, self + 8u);
    const std::uint32_t w = r + 0x778u;
    put(rt, w + 4u, 0u);
    put(rt, w + 8u, 0u);
    const std::uint32_t q = word(rt, self + 0xCu);  // the delay slot of the jal
    (void)call_setf(rt, ctx, 0x08B498ACu, {frame.at(4u), frame.at(0u)}, {float_at(rt, r + 0x51Cu)});
    const std::uint32_t table = 0x08CF35A0u;
    const auto weight = [&](float value, float high, float band) {
        if (high <= value) return 1.0f;
        if (value <= -band) return 0.0f;
        return (value + band) / (high + band);
    };
    if (word(rt, table + 0x2Cu) != 0u) {
        const float t = word(rt, table + 0x30u) != 0u ? x : float_at(rt, r + 0x6F8u);
        const float rate = float_at(rt, q + 0x10u);
        const float p = float_at(rt, r + 0x6C8u) - psp_mul(t, rate);
        const float c = float_at(rt, r + 0x6C4u);
        const float u = (psp_mul(c, float_at(rt, frame.at(0u))) - psp_mul(p, float_at(rt, frame.at(4u)))) - c;
        const std::uint32_t block = q + 0x270u;
        const float left = psp_mul(float_at(rt, r + 0x5FCu), float_at(rt, block + 0xA8u));
        const float right = psp_mul(float_at(rt, r + 0x600u), float_at(rt, block + 0xACu));
        const float d = (left - right) - u;
        const float high = float_at(rt, table + 0x38u), band = float_at(rt, table + 0x34u);
        put_float(rt, w + 4u, weight(d, high, band));
    }
    ctx.gpr[3] = 1u;
    if (word(rt, table + 0x3Cu) != 0u) {
        const float s = word(rt, table + 0x40u) != 0u ? x : float_at(rt, r + 0x6F8u);
        float e = psp_mul(s + s, float_at(rt, q + 0x2Cu));
        const std::uint32_t first = call_set(rt, ctx, 0x08955300u, {self, 2u});
        const float a = float_at(rt, first + 0x34u);
        const std::uint32_t second = call_set(rt, ctx, 0x08955300u, {self, 3u});
        float b = float_at(rt, second + 0x34u) - a;
        if (e < 0.0f) {
            e = -e;
            b = -b;
        }
        const float d2 = b - e;
        put_float(rt, w + 8u, weight(d2, float_at(rt, table + 0x48u), float_at(rt, table + 0x44u)));
    }
    ctx.gpr[2] = 1u;
}

// func_0895C140(self): q = self[0xC], r = self[8] (s6), P = q + 0x270 (a block of per-side data: bytes at P + 0x14 + i, floats at P + 0x1C + 4 * i). When the byte P[0] == 7 func_08948324() runs first (a0 = self, a1 = 7). For the sides i = 0, 1 (a = 2 * i, b = a ^ 1) whose byte P[0x14 + i] - 6 is 0 or 1 and while r[0x6C4] is not below 0x4031C71D: g = sp[0] is func_0895B5B0(self, sp)'s answer when it answers non-zero, otherwise (r[0x6C4] not above 0x42480001)
// g = min(|r[0x520]| * 0x40747645, 1.0f); p = P[0x1C + 4 * i] * g (nothing when it is 0.0f); A = func_08955300(self, a), B = func_08955300(self, b), m = (A[0x34] + B[0x34]) * 0.5f, raised by m / 8 (0x3E000000) when r[0x520] > 0 and lowered by m / 8 when r[0x520] < 0; B[0x34] < m gives B[0x9C] += p * 0x3EE00000 and A[0x9C] -= p * 0x3F100000, m < B[0x34] gives B[0x9C] -= p * 0x3F100000 and A[0x9C] += p * 0x3EE00000.
// Not registered: 3000 dense race cases compared equal (0 mismatches) and none of them changed memory, so the paths that write are proven only by reading them against the asm by hand (the gate cannot see them).
[[maybe_unused]] void func_0895C140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t q = word(rt, self + 0xCu), r = word(rt, self + 8u);
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t block = q + 0x270u;
    if (byte_u(rt, block) == 7u) (void)call_set(rt, ctx, 0x08948324u, {self, 7u});
    const std::uint32_t slot = frame.at(0u);
    for (std::uint32_t i = 0u; i != 2u; ++i) {
        const std::uint32_t a = 2u * i, b = a ^ 1u;
        if (byte_u(rt, block + i + 0x14u) - 6u > 1u) continue;
        put(rt, slot, 0u);
        if (float_at(rt, r + 0x6C4u) < std::bit_cast<float>(0x4031C71Du)) continue;
        float g;
        if (call_set(rt, ctx, 0x0895B5B0u, {self, slot}) != 0u) {
            g = float_at(rt, slot);
        } else {
            if (std::bit_cast<float>(0x42480001u) < float_at(rt, r + 0x6C4u)) continue;
            g = float_at(rt, r + 0x520u);
            put_float(rt, slot, g);
            if (g < 0.0f) {
                g = -g;
                put_float(rt, slot, g);
            }
            g = psp_mul(float_at(rt, slot), std::bit_cast<float>(0x40747645u));
            put_float(rt, slot, g);
            if (1.0f < g) {
                g = 1.0f;
                put_float(rt, slot, g);
            }
        }
        const float p = psp_mul(float_at(rt, block + 0x1Cu + 4u * i), g);
        if (p == 0.0f) continue;
        const std::uint32_t first = call_set(rt, ctx, 0x08955300u, {self, a});
        const std::uint32_t second = call_set(rt, ctx, 0x08955300u, {self, b});
        const float f4 = float_at(rt, second + 0x34u);
        const float side = float_at(rt, r + 0x520u);
        float m = psp_mul(float_at(rt, first + 0x34u) + f4, 0.5f);
        const float eighth = psp_mul(m, std::bit_cast<float>(0x3E000000u));
        if (0.0f < side) m = m + eighth;
        else if (side < 0.0f) m = m - eighth;
        const float big = psp_mul(p, std::bit_cast<float>(0x3F100000u)), small = psp_mul(p, std::bit_cast<float>(0x3EE00000u));
        if (f4 < m) {
            put_float(rt, second + 0x9Cu, small + float_at(rt, second + 0x9Cu));
            put_float(rt, first + 0x9Cu, float_at(rt, first + 0x9Cu) - big);
        } else if (m < f4) {
            put_float(rt, second + 0x9Cu, float_at(rt, second + 0x9Cu) - big);
            put_float(rt, first + 0x9Cu, small + float_at(rt, first + 0x9Cu));
        }
    }
    ctx.gpr[2] = 2u;  // the addiu v0, zero, 2 of the loop test
}

// func_0896A9C4(obj, p0, p1, p2, p3): four optional float pointers set four properties of obj, by the mode m = func_08965BD4(obj) (nothing for a mode other than 1, 2 or 3). Mode 2 hands the floats themselves: func_08965BE8(obj; f12 = *p0), func_08965C14(obj; *p1), func_08965C40(obj; *p2), func_08965C90(obj; *p3), each for a non-null pointer. Mode 1 (when func_08965CE0(obj) answers a record r) asks func_0896A35C(r, 0, 0, sp + 4, sp) for the pair {sp[0], sp[4]} and hands the ratios
// *p0 / sp[4] (0.0f when sp[4] is not positive) to func_08965BE8, *p1 / sp[0] (0.0f otherwise) to func_08965C14, *p2 / sp[4] (1.0f otherwise) to func_08965C40 and *p3 / sp[0] (1.0f otherwise) to func_08965C90. Mode 3 zeroes sp[0], sp[4], lets func_08969B98(r, sp, sp + 4, 0, 0) fill them when r exists and hands *p0 + sp[0] to func_08965BE8, *p1 + sp[4] to func_08965C14, *p2 to func_08965C40 and *p3 to func_08965C90.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `} else if (mode == 3u) {`; `if (record != 0u) (void)call_set(rt, ctx, 0x08969B98u, {record, frame.at(0u), frame.at(4u), 0u, 0u});`; `if (p0 != 0u) deliver(0x08965BE8u, float_at(rt, p0) + float_at(rt, frame.at(0u)));`; `if (p1 != 0u) deliver(0x08965C14u, float_at(rt, p1) + float_at(rt, frame.at(4u)));`; `if (p2 != 0u) deliver(0x08965C40u, float_at(rt, p2));`; `if (p3 != 0u) deliver(0x08965C90u, float_at(rt, p3));`; `} else if (mode == 1u) {`; `if (record == 0u) return;`; `deliver(0x08965BE8u, 0.0f < divisor ? float_at(rt, p0) / divisor : 0.0f);`; `deliver(0x08965C14u, 0.0f < divisor ? float_at(rt, p1) / divisor : 0.0f);`; `if (p2 != 0u) {`; `deliver(0x08965C40u, 0.0f < divisor ? float_at(rt, p2) / divisor : 1.0f);`; `if (p3 != 0u) {`; `deliver(0x08965C90u, 0.0f < divisor ? float_at(rt, p3) / divisor : 1.0f);`
void func_0896A9C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4], p0 = ctx.gpr[5], p1 = ctx.gpr[6], p2 = ctx.gpr[7], p3 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t mode = call_set(rt, ctx, 0x08965BD4u, {obj});
    const auto deliver = [&](std::uint32_t target, float value) { (void)call_setf(rt, ctx, target, {obj}, {value}); };
    if (mode == 2u) {
        if (p0 != 0u) deliver(0x08965BE8u, float_at(rt, p0));
        if (p1 != 0u) deliver(0x08965C14u, float_at(rt, p1));
        if (p2 != 0u) deliver(0x08965C40u, float_at(rt, p2));
        if (p3 != 0u) deliver(0x08965C90u, float_at(rt, p3));
    } else if (mode == 3u) {
        put(rt, frame.at(0u), 0u);
        put(rt, frame.at(4u), 0u);
        const std::uint32_t record = call_set(rt, ctx, 0x08965CE0u, {obj});
        if (record != 0u) (void)call_set(rt, ctx, 0x08969B98u, {record, frame.at(0u), frame.at(4u), 0u, 0u});
        if (p0 != 0u) deliver(0x08965BE8u, float_at(rt, p0) + float_at(rt, frame.at(0u)));
        if (p1 != 0u) deliver(0x08965C14u, float_at(rt, p1) + float_at(rt, frame.at(4u)));
        if (p2 != 0u) deliver(0x08965C40u, float_at(rt, p2));
        if (p3 != 0u) deliver(0x08965C90u, float_at(rt, p3));
    } else if (mode == 1u) {
        const std::uint32_t record = call_set(rt, ctx, 0x08965CE0u, {obj});
        if (record == 0u) return;
        (void)call_set(rt, ctx, 0x0896A35Cu, {record, 0u, 0u, frame.at(4u), frame.at(0u)});
        if (p0 != 0u) {
            const float divisor = float_at(rt, frame.at(4u));
            deliver(0x08965BE8u, 0.0f < divisor ? float_at(rt, p0) / divisor : 0.0f);
        }
        if (p1 != 0u) {
            const float divisor = float_at(rt, frame.at(0u));
            deliver(0x08965C14u, 0.0f < divisor ? float_at(rt, p1) / divisor : 0.0f);
        }
        if (p2 != 0u) {
            const float divisor = float_at(rt, frame.at(4u));
            deliver(0x08965C40u, 0.0f < divisor ? float_at(rt, p2) / divisor : 1.0f);
        }
        if (p3 != 0u) {
            const float divisor = float_at(rt, frame.at(0u));
            deliver(0x08965C90u, 0.0f < divisor ? float_at(rt, p3) / divisor : 1.0f);
        }
    }
}

// func_089AA708(self, f12 = dt): func_089A8920(self) runs; nothing more happens (v0 = the last value the asm loaded) when self[0xC0] is zero, the bytes self[0xFC] or self[0xFD] are zero, self[0x6D] is set or self[0xFA] is zero. Otherwise under the lock D_08CF4A2C (0x08CF4A2C, func_08B344E0, released by the tail call func_08B3457C): d1 = |self[0xE4] - self[0x7C]|, d2 = |self[0xE8] - self[0x88]|, step = dt * 60.0f * 0x3E666667.
// d1 < 1.0f snaps self[0x7C] = self[0xE4] (self[0x74] = self[0x78] = 0), otherwise self[0x7C] = func_089A94EC(self[0xE4], self[0x7C], 8.0f, step) (and the same two zeros); d2 likewise with self[0x88] / self[0xE8] (self[0x80] = self[0x84] = 0). With both below 1.0f: e = |self[0xEC] - self[0x94]| < 1.0f snaps self[0x94] = self[0xEC] (self[0x90] = self[0x8C] = 0, self[0xFA] = 0), otherwise func_08B29034(self + 0x8C; f12 = self[0xEC], f13 = step) runs; then self[0xFB] = 1.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (ctx.gpr[2] == 0u) return;`; `if (ctx.gpr[2] != 0u) return;`; `if (d1 < one) {`; `if (d2 < one) {`; `if (d1 < one && d2 < one) {`; `if (std::fabs(target_z - float_at(rt, third + 8u)) < one) {`
void func_089AA708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float dt = ctx.fpr[12];
    (void)call_set(rt, ctx, 0x089A8920u, {self});
    ctx.gpr[2] = word(rt, self + 0xC0u);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = byte_u(rt, self + 0xFCu);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = byte_u(rt, self + 0xFDu);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = byte_u(rt, self + 0x6Du);
    if (ctx.gpr[2] != 0u) return;
    ctx.gpr[2] = byte_u(rt, self + 0xFAu);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t lock = 0x08CF4A2Cu;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t first = self + 0x74u, second = self + 0x80u;
    const float target_x = float_at(rt, self + 0xE4u);
    const float one = 1.0f;
    const float d1 = std::fabs(target_x - float_at(rt, first + 8u));
    const float step = psp_mul(psp_mul(dt, 60.0f), std::bit_cast<float>(0x3E666667u));
    const float target_y = float_at(rt, self + 0xE8u);
    const float d2 = std::fabs(target_y - float_at(rt, second + 8u));
    if (d1 < one) {
        put_float(rt, first + 8u, target_x);
        put(rt, self + 0x74u, 0u);
        put(rt, first + 4u, 0u);
    } else {
        const float moved = call_setf(rt, ctx, 0x089A94ECu, {}, {target_x, float_at(rt, first + 8u), 8.0f, step}).f0;
        put_float(rt, first + 8u, moved);
        put(rt, self + 0x74u, 0u);
        put(rt, first + 4u, 0u);
    }
    if (d2 < one) {
        put(rt, second, 0u);
        put_float(rt, second + 8u, float_at(rt, self + 0xE8u));
        put(rt, second + 4u, 0u);
    } else {
        const float moved = call_setf(rt, ctx, 0x089A94ECu, {}, {float_at(rt, self + 0xE8u), float_at(rt, second + 8u), 8.0f, step}).f0;
        put_float(rt, second + 8u, moved);
        put(rt, second, 0u);
        put(rt, second + 4u, 0u);
    }
    if (d1 < one && d2 < one) {
        const std::uint32_t third = self + 0x8Cu;
        const float target_z = float_at(rt, self + 0xECu);
        if (std::fabs(target_z - float_at(rt, third + 8u)) < one) {
            put_float(rt, third + 8u, target_z);
            put(rt, third + 4u, 0u);
            put(rt, self + 0x8Cu, 0u);
            put8(rt, self + 0xFAu, 0u);
        } else {
            (void)call_setf(rt, ctx, 0x08B29034u, {third}, {target_z, step});
        }
        ctx.gpr[2] = 1u;
        put8(rt, self + 0xFBu, 1u);
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_089AAD34(self, obj): nothing happens when obj is null or func_089BD4DC(obj) differs from func_089AAC8C(self). changed = false; when the bit 0x1000000 of obj[0xE0] is set and func_08965CE0(obj) answers non-zero: the six words sp[8..0x18) are zeroed, func_08969B98(obj, sp + 8, sp + 0xC, sp + 0x10, sp + 0x14) fills the floats, sp[0x10] += sp[8], sp[0x14] += sp[0xC], the lock D_08CF4ACC (0x08CF4ACC) is taken (func_08B344E0)
// and the four floats self[0xC8..0xD4] take sp[8], sp[0xC], sp[0x10], sp[0x14] when any of them differs from its old value by more than 0.1f (changed = true); self[0xD8], self[0xDC], self[0xE0] take obj[0xD4], obj[0xD8], obj[0xDC] when different (changed = true); the lock is released (func_08B3457C). Then, when the bytes self[0xF8] and self[0xF9] are both zero or changed is false nothing more happens.
// Otherwise the other lock (0x08CF4A2C) is taken, p = (self[0xC8] + 1.0f, self[0xCC] + 1.0f) + ((self[0xD0] - 1.0f) - that, (self[0xD4] - 1.0f) - that) * (self[0xD8], self[0xDC]) goes to func_089A8A78(self, sp, sp + 4); self[0xE4] = sp[0] when self[0xF8] is set, self[0xE8] = sp[4] when self[0xF9] is set, self[0xEC] = self[0xE0], self[0xFA] = 1 and the lock is released.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (obj == 0u) return;`; `if (first != call_set(rt, ctx, 0x089AAC8Cu, {self})) return;`; `if ((word(rt, obj + 0xE0u) & 0x01000000u) != 0u && call_set(rt, ctx, 0x08965CE0u, {obj}) != 0u) {`; `const bool differs = tolerance < std::fabs(float_at(rt, box) - x) || tolerance < std::fabs(float_at(rt...`; `tolerance < std::fabs(float_at(rt, box + 8u) - float_at(rt, frame.at(0x10u))) || tolerance < std::fabs...`; `if (!(float_at(rt, self + 0xD8u) == float_at(rt, obj + 0xD4u))) {`; `if (!(float_at(rt, self + 0xDCu) == float_at(rt, obj + 0xD8u))) {`; `if (!(float_at(rt, self + 0xE0u) == float_at(rt, obj + 0xDCu))) {`; `if (byte_u(rt, self + 0xF8u) == 0u && byte_u(rt, self + 0xF9u) == 0u) return;`; `if (byte_u(rt, self + 0xF8u) != 0u) put(rt, self + 0xE4u, word(rt, frame.at(0u)));`; `if (byte_u(rt, self + 0xF9u) != 0u) put(rt, self + 0xE8u, word(rt, frame.at(4u)));`
void func_089AAD34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    if (obj == 0u) return;
    const std::uint32_t first = call_set(rt, ctx, 0x089BD4DCu, {obj});
    if (first != call_set(rt, ctx, 0x089AAC8Cu, {self})) return;
    bool changed = false;
    if ((word(rt, obj + 0xE0u) & 0x01000000u) != 0u && call_set(rt, ctx, 0x08965CE0u, {obj}) != 0u) {
        for (std::uint32_t offset = 8u; offset <= 0x14u; offset += 4u) put(rt, frame.at(offset), 0u);
        (void)call_set(rt, ctx, 0x08969B98u, {obj, frame.at(8u), frame.at(0xCu), frame.at(0x10u), frame.at(0x14u)});
        const float f2 = float_at(rt, frame.at(0x10u)) + float_at(rt, frame.at(8u));
        const float f1 = float_at(rt, frame.at(0x14u)) + float_at(rt, frame.at(0xCu));
        put_float(rt, frame.at(0x10u), f2);
        put_float(rt, frame.at(0x14u), f1);
        (void)call_set(rt, ctx, 0x08B344E0u, {0x08CF4ACCu});
        const float tolerance = std::bit_cast<float>(0x3DCCCCCDu);
        const float x = float_at(rt, frame.at(8u));
        const std::uint32_t box = self + 0xC8u;
        const bool differs = tolerance < std::fabs(float_at(rt, box) - x) || tolerance < std::fabs(float_at(rt, box + 4u) - float_at(rt, frame.at(0xCu))) ||
                             tolerance < std::fabs(float_at(rt, box + 8u) - float_at(rt, frame.at(0x10u))) || tolerance < std::fabs(float_at(rt, box + 0xCu) - float_at(rt, frame.at(0x14u)));
        if (differs) {
            changed = true;
            put_float(rt, box, x);
            put(rt, box + 0xCu, word(rt, frame.at(0x14u)));
            put(rt, box + 4u, word(rt, frame.at(0xCu)));
            put(rt, box + 8u, word(rt, frame.at(0x10u)));
        }
        if (!(float_at(rt, self + 0xD8u) == float_at(rt, obj + 0xD4u))) {
            put(rt, self + 0xD8u, word(rt, obj + 0xD4u));
            changed = true;
        }
        if (!(float_at(rt, self + 0xDCu) == float_at(rt, obj + 0xD8u))) {
            put(rt, self + 0xDCu, word(rt, obj + 0xD8u));
            changed = true;
        }
        if (!(float_at(rt, self + 0xE0u) == float_at(rt, obj + 0xDCu))) {
            put(rt, self + 0xE0u, word(rt, obj + 0xDCu));
            changed = true;
        }
        (void)call_set(rt, ctx, 0x08B3457Cu, {0x08CF4ACCu});
    }
    if (byte_u(rt, self + 0xF8u) == 0u && byte_u(rt, self + 0xF9u) == 0u) return;
    if (!changed) return;
    const std::uint32_t lock = 0x08CF4A2Cu;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const float x0 = float_at(rt, self + 0xC8u) + 1.0f, y0 = float_at(rt, self + 0xCCu) + 1.0f;
    const float x1 = float_at(rt, self + 0xD0u) - 1.0f, y1 = float_at(rt, self + 0xD4u) - 1.0f;
    const float wx = float_at(rt, self + 0xD8u), wy = float_at(rt, self + 0xDCu);
    put_float(rt, frame.at(0u), x0 + psp_mul(x1 - x0, wx));
    put_float(rt, frame.at(4u), y0 + psp_mul(y1 - y0, wy));  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089A8A78u, {self, frame.at(0u), frame.at(4u)});
    if (byte_u(rt, self + 0xF8u) != 0u) put(rt, self + 0xE4u, word(rt, frame.at(0u)));
    if (byte_u(rt, self + 0xF9u) != 0u) put(rt, self + 0xE8u, word(rt, frame.at(4u)));
    const std::uint32_t kept = word(rt, self + 0xE0u);
    put8(rt, self + 0xFAu, 1u);
    put(rt, self + 0xECu, kept);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
}

// func_089ACCA0(a, b, c, d, e; f12 = x, f13 = y): emits a grid of vertices through func_089AC2A8(a, sp, d[0] + 16 * k, e; f12 = x, f13 = y). b and c are float vectors {begin, end} of nb = (b[4] - b[0]) >> 2 and nc = (c[4] - c[0]) >> 2 entries; the vertex record at sp is {sp[0], sp[4], sp[8], sp[0xC]}. For each row r = 0..nc-1 and then the closing row r = nc: sp[0xC] = y * c[0][r] (y for the closing row) and sp[4] = the previous row's sp[0xC] (0 for the first row), sp[0] = 0;
// for each column j = 0..nb-1: sp[8] = x * b[0][j], the vertex k = r * (nb + 1) + j is emitted and sp[0] = sp[8]; the closing vertex k = r * (nb + 1) + nb is emitted with sp[8] = x. d[0] and b[0] are re-read at every use. v0 = the last call's.
// Branches the compared cases do not take: none (every branch and region of the fast gate's coverage run ran).
void func_089ACCA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7], e = ctx.gpr[8];
    const float x = ctx.fpr[12], y = ctx.fpr[13];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t record = frame.at(0u);
    const std::int32_t nb = signed_of(word(rt, b + 4u) - word(rt, b)) >> 2;
    const std::int32_t nc = signed_of(word(rt, c + 4u) - word(rt, c)) >> 2;
    put(rt, frame.at(0x10u), static_cast<std::uint32_t>(nb + 1));
    put(rt, frame.at(0x14u), static_cast<std::uint32_t>(nc));
    put(rt, frame.at(0x18u), c);
    put(rt, record, 0u);
    put(rt, record + 4u, 0u);
    put(rt, record + 8u, 0u);
    put(rt, record + 0xCu, 0u);
    const auto emit = [&](std::uint32_t k) {
        (void)call_setf(rt, ctx, 0x089AC2A8u, {a, record, word(rt, d) + (k << 4u), e}, {x, y});
    };
    const auto row = [&](std::uint32_t r) {
        const std::uint32_t base = r * static_cast<std::uint32_t>(nb + 1);
        put(rt, record, 0u);
        std::uint32_t j = 0u;
        for (; signed_of(j) < nb; ++j) {
            put_float(rt, record + 8u, psp_mul(x, float_at(rt, word(rt, b) + (j << 2u))));
            emit(base + j);
            put(rt, record, word(rt, record + 8u));
        }
        put_float(rt, record + 8u, x);
        emit(base + j);
    };
    std::uint32_t r = 0u;
    if (nc > 0) {
        for (; signed_of(r) < nc; ++r) {
            const std::uint32_t row_values = word(rt, word(rt, frame.at(0x18u)));
            put_float(rt, record + 0xCu, psp_mul(y, float_at(rt, row_values + (r << 2u))));
            row(r);
            put(rt, record + 4u, word(rt, record + 0xCu));
        }
    }
    put_float(rt, record + 0xCu, y);
    row(r);
}

// func_089F6748(self, a1) -> v0: a scrolling value. step = self[0x88] * func_089A8E8C(a1) (f20); lo / hi = self[0x80], self[0x84]; with the byte self[0x98] set the limit is f3 = self[0x84] + |self[0x94]| and the step is f5 = +step when self[0x80] < self[0x84], else f3 = self[0x84] - |self[0x94]| and f5 = -step; with it clear f3 = self[0x80] -+ |self[0x90]| (- when self[0x80] < self[0x84], f5 = -step) and f5 = +step otherwise.
// Unless the flag word self[0x60] has bit 0x80000, self[0x7C] += f5. When the value has passed the limit (f5 < 0: self[0x7C] < f3; else f3 < self[0x7C]) self[0x7C] = f3, s1 = 1 and, when the byte self[0x99] is set, self[0x98] ^= 1 (it was a0) and s1 = 0; otherwise s1 = 0. Without bit 0x40000 nothing more happens and v0 = 1.
// With it, the clamp range [min(lo, hi), max(lo, hi)] is used on the value (the clamp keeps the asm's two compares: not above the minimum gives the minimum, then not below the maximum gives the maximum): when self[0x8C] is 0.0f or self[0x9C] is null func_08967538(self[0x78]; f12 = clamp(self[0x7C])) runs; otherwise the virtual slot 0x1B8 of o = self[0x9C] runs with (o, 0; clamp(v)), (o, 1; clamp(v +- |self[0x8C]|)), (o, 2; the same, both re-read from memory), (o, 3; clamp(self[0x7C] re-read)) where v = self[0x7C] and the sign is + when f5 < 0. v0 = s1.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (f4 < f2) {`; `} else if (f4 < f2) {`; `if (byte_u(rt, self + 0x99u) != 0u) {`; `const float lo = f4 < f2 ? f4 : f2, hi = f4 < f2 ? f2 : f4;`; `if (!(lo < value)) value = lo;`; `if (spread == 0.0f || word(rt, self + 0x9Cu) == 0u) {`; `const auto shifted = [&]() { return plus ? float_at(rt, self + 0x7Cu) + std::fabs(float_at(rt, self + ...`
void func_089F6748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    float step = float_at(rt, self + 0x88u);  // the delay slot of the jal
    step = psp_mul(step, call_setf(rt, ctx, 0x089A8E8Cu, {argument}, {}).f0);
    const std::uint32_t byte98 = byte_u(rt, self + 0x98u);
    const float f2 = float_at(rt, self + 0x84u), f4 = float_at(rt, self + 0x80u);
    float f5, f3;
    if (byte98 != 0u) {
        if (f4 < f2) {
            f5 = step;
            f3 = f2 + std::fabs(float_at(rt, self + 0x94u));
        } else {
            f5 = -step;
            f3 = f2 - std::fabs(float_at(rt, self + 0x94u));
        }
    } else if (f4 < f2) {
        f5 = -step;
        f3 = f4 - std::fabs(float_at(rt, self + 0x90u));
    } else {
        f5 = step;
        f3 = f4 + std::fabs(float_at(rt, self + 0x90u));
    }
    const std::uint32_t flags = word(rt, self + 0x60u);
    float f1 = float_at(rt, self + 0x7Cu);
    if ((flags & 0x00080000u) == 0u) {
        f1 = f5 + f1;
        put_float(rt, self + 0x7Cu, f1);
    }
    std::uint32_t s1 = 0u;
    const bool passed = f5 < 0.0f ? (f1 < f3) : (f3 < f1);
    if (passed) {
        put_float(rt, self + 0x7Cu, f3);
        s1 = 1u;
        if (byte_u(rt, self + 0x99u) != 0u) {
            put8(rt, self + 0x98u, byte98 ^ 1u);
            s1 = 0u;
        }
    }
    if ((flags & 0x00040000u) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const float lo = f4 < f2 ? f4 : f2, hi = f4 < f2 ? f2 : f4;
    const auto clamp = [&](float value) {
        if (!(lo < value)) value = lo;
        if (!(value < hi)) value = hi;
        return value;
    };
    const float spread = float_at(rt, self + 0x8Cu);
    if (spread == 0.0f || word(rt, self + 0x9Cu) == 0u) {
        (void)call_setf(rt, ctx, 0x08967538u, {word(rt, self + 0x78u)}, {clamp(float_at(rt, self + 0x7Cu))});
    } else {
        const auto invoke = [&](std::uint32_t index, float value) {
            const std::uint32_t object = word(rt, self + 0x9Cu);
            const std::uint32_t target = word(rt, word(rt, object) + 0x1B8u);
            (void)call_setf(rt, ctx, target, {object, index}, {clamp(value)});
        };
        const bool plus = f5 < 0.0f;
        invoke(0u, float_at(rt, self + 0x7Cu));
        const auto shifted = [&]() { return plus ? float_at(rt, self + 0x7Cu) + std::fabs(float_at(rt, self + 0x8Cu)) : float_at(rt, self + 0x7Cu) - std::fabs(float_at(rt, self + 0x8Cu)); };
        invoke(1u, shifted());  // re-read after each call, like the asm
        invoke(2u, shifted());
        invoke(3u, float_at(rt, self + 0x7Cu));
    }
    ctx.gpr[2] = s1;
}

// func_08A07E28(self, obj): func_08A07B70(self), func_089A8C24(obj), the virtual slot 0x110 of obj (obj, (self[0x1A0] >> 6) & 3), x = func_089A8B68(obj) (f20) and y = func_089A8B80(obj) (f21). With bit 4 of the flags self[0x1A0] and func_089A8BF4(obj) not below 1.0f the virtual slot 0x150 of obj runs. With bit 0x20: func_089ACCA0(obj, self + 0xE8, self + 0xF4, self + 0x100, 0; f12 = x, f13 = y).
// Otherwise m = func_08A06EC8(self): m == 1 runs the slot 0x108 of obj with (obj, func_08A06E94(self, 0)) and then func_089A9654(obj, 0, 0; x, y, self[0x11C]) when bit 0x20 is set, or |self[0x11C]| is not below 0x34000000, or |func_089A8B20(obj)[4]| / [0xC] is not below it (with the self[0x11C] re-read), otherwise func_08A073E0(self, obj); any other m runs func_08A06EC8(self) again and, when that answers 4, copies func_08A06E94(self, k) into the four records sp + 0x10 * k (func_0896418C) and runs func_089A9654(obj, 0, sp; x, y, self[0x11C]).
// At the end (self[0x1A0] re-read): bit 4 with func_089A8BF4(obj) not below 1.0f runs the slot 0x154 of obj, the slot 0x114 runs and func_08A07CE0(self, obj) ends: v0 = its result.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `bool draw = bit20 || epsilon <= std::fabs(f14);`; `if (!draw) {`; `if (epsilon <= std::fabs(float_at(rt, entry + 4u)) || epsilon <= std::fabs(float_at(rt, entry + 0xCu))) {`; `if (draw) (void)call_setf(rt, ctx, 0x089A9654u, {obj, 0u, 0u}, {x, y, f14_draw});`; `if (call_set(rt, ctx, 0x08A06EC8u, {self}) == 4u) {`
void func_08A07E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    (void)call_set(rt, ctx, 0x08A07B70u, {self});
    (void)call_set(rt, ctx, 0x089A8C24u, {obj});
    const std::uint32_t slot110 = slot_of(rt, obj, 0x110u);
    const std::uint32_t mode = word(rt, self + 0x1A0u);
    (void)call_set(rt, ctx, slot110, {obj, (mode >> 6u) & 3u});
    const float x = call_setf(rt, ctx, 0x089A8B68u, {obj}, {}).f0;
    const float y = call_setf(rt, ctx, 0x089A8B80u, {obj}, {}).f0;
    std::uint32_t flags = word(rt, self + 0x1A0u);
    const float one = 1.0f;
    if ((flags & 4u) != 0u) {
        if (!(call_setf(rt, ctx, 0x089A8BF4u, {obj}, {}).f0 < one)) (void)call_set(rt, ctx, slot_of(rt, obj, 0x150u), {obj});
        flags = word(rt, self + 0x1A0u);
    }
    std::uint32_t after;
    if ((flags & 0x20u) != 0u) {
        (void)call_setf(rt, ctx, 0x089ACCA0u, {obj, self + 0xE8u, self + 0xF4u, self + 0x100u, 0u}, {x, y});
        after = word(rt, self + 0x1A0u);
    } else if (call_set(rt, ctx, 0x08A06EC8u, {self}) == 1u) {
        const std::uint32_t slot108 = slot_of(rt, obj, 0x108u);  // the delay slot of the next jal
        const std::uint32_t record = call_set(rt, ctx, 0x08A06E94u, {self, 0u});
        (void)call_set(rt, ctx, slot108, {obj, record});
        const bool bit20 = (word(rt, self + 0x1A0u) & 0x20u) != 0u;
        const float f14 = float_at(rt, self + 0x11Cu);
        const float epsilon = std::bit_cast<float>(0x34000000u);
        bool draw = bit20 || epsilon <= std::fabs(f14);
        float f14_draw = f14;
        if (!draw) {
            const std::uint32_t entry = call_set(rt, ctx, 0x089A8B20u, {obj});
            if (epsilon <= std::fabs(float_at(rt, entry + 4u)) || epsilon <= std::fabs(float_at(rt, entry + 0xCu))) {
                draw = true;
                f14_draw = float_at(rt, self + 0x11Cu);
            } else {
                (void)call_set(rt, ctx, 0x08A073E0u, {self, obj});
            }
        }
        if (draw) (void)call_setf(rt, ctx, 0x089A9654u, {obj, 0u, 0u}, {x, y, f14_draw});
        after = word(rt, self + 0x1A0u);
    } else {
        if (call_set(rt, ctx, 0x08A06EC8u, {self}) == 4u) {
            for (std::uint32_t k = 0u; k != 4u; ++k) {
                const std::uint32_t record = call_set(rt, ctx, 0x08A06E94u, {self, k});
                (void)call_set(rt, ctx, 0x0896418Cu, {frame.at(0x10u * k), record});
            }
            (void)call_setf(rt, ctx, 0x089A9654u, {obj, 0u, frame.at(0u)}, {x, y, float_at(rt, self + 0x11Cu)});
        }
        after = word(rt, self + 0x1A0u);
    }
    if ((after & 4u) != 0u) {
        if (!(call_setf(rt, ctx, 0x089A8BF4u, {obj}, {}).f0 < one)) (void)call_set(rt, ctx, slot_of(rt, obj, 0x154u), {obj});
    }
    (void)call_set(rt, ctx, slot_of(rt, obj, 0x114u), {obj});
    (void)call_set(rt, ctx, 0x08A07CE0u, {self, obj});
}

// func_08A17104(self, obj): lays out a rectangle. Six callees fill the frame: func_0896A35C(obj, sp + 4, sp + 8, 0, 0) and func_0896B7AC(obj, sp + 0xC, sp + 0x10) the float pairs {sp[4], sp[8]} and {sp[0xC], sp[0x10]}, func_08966B20(obj, sp + 0x14, sp + 0x18, sp + 0x1C, sp + 0x20) the integers i14, i18, i1C, i20, func_08967380(obj, sp + 0x24, sp + 0x28) the floats sp[0x24] and sp[0x28],
// func_0896726C(obj, sp + 0, sp + 1) and func_089672C8(obj, sp + 2, sp + 3) the flag bytes b0..b3, and func_089670A0(obj) runs. With self floats s0..s3 = self[0], self[4], self[8], self[0xC]: ex = (s1 - s0) - ((float)i14 + sp[0xC] + (float)i18) and ey = (s3 - s2) - ((float)i1C + sp[0x10] + (float)i20) (both kept in callee-saved f20 / f21).
// The x pair: b0 and b2 give f12 = s0 + (float)i14, f14 = (s1 - s0) - (float)(i14 + i18); b0 alone gives f12 = (s0 + ex * sp[0x24]) + (float)i14, f14 = sp[0xC]; without b0 and with 0 < ex: f12 = sp[4] raised to s0 + (float)i14 or, when it was not below it, lowered to (s1 - (float)i18) - sp[0xC] when that is below it; f14 = sp[0xC]; otherwise f12 = s0 + (float)i14, f14 = sp[0xC].
// The y pair is the same with b1 / b3, s2 / s3, i1C / i20, ey, sp[0x28], sp[8] and sp[0x10] (f13 and f15). func_08A17014(self, obj) is then called with f12..f15 = x, y, w, h: the result is its v0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (b2 != 0u) {`; `if (position < floor_x) {`; `if (ceiling_x < position) position = ceiling_x;`; `if (b3 != 0u) {`; `if (position < floor_y) {`; `if (ceiling_y < position) position = ceiling_y;`
void func_08A17104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    (void)call_set(rt, ctx, 0x0896A35Cu, {obj, frame.at(4u), frame.at(8u), 0u, 0u});
    (void)call_set(rt, ctx, 0x0896B7ACu, {obj, frame.at(0xCu), frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08966B20u, {obj, frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu), frame.at(0x20u)});
    const auto integer = [&](std::uint32_t offset) { return signed_of(word(rt, frame.at(offset))); };
    const auto as_float = [](std::int32_t value) { return static_cast<float>(value); };
    const std::int32_t i14 = integer(0x14u), i18 = integer(0x18u), i1c = integer(0x1Cu), i20 = integer(0x20u);
    const float s0 = float_at(rt, self), s1 = float_at(rt, self + 4u), s2 = float_at(rt, self + 8u), s3 = float_at(rt, self + 0xCu);
    const float ex = (s1 - s0) - ((as_float(i14) + float_at(rt, frame.at(0xCu))) + as_float(i18));
    const float ey = (s3 - s2) - ((as_float(i1c) + float_at(rt, frame.at(0x10u))) + as_float(i20));
    (void)call_set(rt, ctx, 0x08967380u, {obj, frame.at(0x24u), frame.at(0x28u)});
    (void)call_set(rt, ctx, 0x0896726Cu, {obj, frame.at(0u), frame.at(1u)});
    (void)call_set(rt, ctx, 0x089672C8u, {obj, frame.at(2u), frame.at(3u)});
    (void)call_set(rt, ctx, 0x089670A0u, {obj});
    const std::uint32_t b0 = byte_u(rt, frame.at(0u)), b1 = byte_u(rt, frame.at(1u)), b2 = byte_u(rt, frame.at(2u)), b3 = byte_u(rt, frame.at(3u));
    float x, y, w, h;
    // x and w (f12, f14)
    if (b0 != 0u) {
        if (b2 != 0u) {
            const float lo = float_at(rt, self), hi = float_at(rt, self + 4u);
            const std::int32_t total = signed_of(word(rt, frame.at(0x14u)) + word(rt, frame.at(0x18u)));
            w = (hi - lo) - as_float(total);
            x = lo + as_float(integer(0x14u));
        } else {
            const float step = psp_mul(ex, float_at(rt, frame.at(0x24u)));
            x = (float_at(rt, self) + step) + as_float(integer(0x14u));
            w = float_at(rt, frame.at(0xCu));
        }
    } else if (0.0f < ex) {
        float position = float_at(rt, frame.at(4u));
        const float floor_x = float_at(rt, self) + as_float(integer(0x14u));
        w = float_at(rt, frame.at(0xCu));
        if (position < floor_x) {
            position = floor_x;
        } else {
            const float ceiling_x = (float_at(rt, self + 4u) - as_float(integer(0x18u))) - w;
            if (ceiling_x < position) position = ceiling_x;
        }
        x = position;
    } else {
        x = float_at(rt, self) + as_float(integer(0x14u));
        w = float_at(rt, frame.at(0xCu));
    }
    // y and h (f13, f15)
    if (b1 != 0u) {
        if (b3 != 0u) {
            const float lo = float_at(rt, self + 8u), hi = float_at(rt, self + 0xCu);
            const std::int32_t total = signed_of(word(rt, frame.at(0x1Cu)) + word(rt, frame.at(0x20u)));
            h = (hi - lo) - as_float(total);
            y = lo + as_float(integer(0x1Cu));
        } else {
            const float step = psp_mul(ey, float_at(rt, frame.at(0x28u)));
            y = (float_at(rt, self + 8u) + step) + as_float(integer(0x1Cu));
            h = float_at(rt, frame.at(0x10u));
        }
    } else if (0.0f < ey) {
        float position = float_at(rt, frame.at(8u));
        const float floor_y = float_at(rt, self + 8u) + as_float(integer(0x1Cu));
        h = float_at(rt, frame.at(0x10u));
        if (position < floor_y) {
            position = floor_y;
        } else {
            const float ceiling_y = (float_at(rt, self + 0xCu) - as_float(integer(0x20u))) - h;
            if (ceiling_y < position) position = ceiling_y;
        }
        y = position;
    } else {
        y = float_at(rt, self + 8u) + as_float(integer(0x1Cu));
        h = float_at(rt, frame.at(0x10u));
    }
    ctx.fpr[12] = x;
    ctx.fpr[13] = y;
    ctx.fpr[14] = w;
    ctx.fpr[15] = h;
    ctx.gpr[4] = self;
    ctx.gpr[5] = obj;
    (void)call_keep(rt, ctx, 0x08A17014u, {self, obj}, {x, y, w, h});
}

// func_08A18494(self, obj): the sibling of func_08A17104 for an object whose layout sits in self[0] (a child). Six callees fill the frame like that function's: func_08A1804C(self, obj, sp + 4, sp + 8) and func_08A17CAC(self, obj, sp + 0xC, sp + 0x10) the float pairs, func_08A17CD8(self, obj, sp + 0x14, sp + 0x18, sp + 0x1C, sp + 0x20) the integers i14, i18, i1C, i20, func_08A17D24(self, obj, sp + 0x24, sp + 0x28) the floats sp[0x24] and sp[0x28],
// func_08A17D50(self, obj, sp + 0, sp + 1) and func_08A17E90(self, obj, sp + 2, sp + 3) the flag bytes b0..b3. f21 = (float)(i1C + i20) + sp[0x10] and f20 = (float)(i14 + i18) + sp[0xC]; when the byte child[4] (child = self[0]) is non-zero f20 = child[0x10] (a float) and sp[0xC] = f20 - (float)(i14 + i18); with b0, f20 += self[4]. func_08A17B38(self, obj) == 2 gives sp[4] = (self[0xC] - f20) + (float)i14 and self[0xC] -= f20 + (float)(int)child2[8] (child2 = self[0] again), anything else sp[4] = self[8] + (float)i14 and self[8] += f20 + (float)(int)child2[8].
// The x pair (f12 = sp[4], f14 = sp[0xC]): with b0 and b2 sp[0xC] = self[4] + sp[0xC]; with b0 alone sp[4] = self[4] * sp[0x24] + sp[4]. The y pair (f13 = sp[8], f15 = sp[0x10]): with b1, g = (self[0x14] - self[0x10]) - f21 and b3 gives sp[0x10] = g + sp[0x10], sp[8] = self[0x10] + (float)i1C, b1 alone sp[8] = (self[0x10] + (float)i1C) + g * sp[0x28]; without b1 and with 0 < (self[0x14] - self[0x10]) - f21 sp[8] is raised to self[0x10] + (float)i1C or, when it was not below it, lowered to (self[0x14] - (float)i20) - sp[0x10]
// when that is below it; otherwise sp[8] = self[0x10] + (float)i1C. func_08A1845C(self, obj) runs with f12..f15 = sp[4], sp[8], sp[0xC], sp[0x10]: the result is its v0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, child + 4u) != 0u) {`; `if (slot(8u) < floor_y) {`; `if (ceiling_y < slot(8u)) put_float(rt, frame.at(8u), ceiling_y);`
void func_08A18494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    (void)call_set(rt, ctx, 0x08A1804Cu, {self, obj, frame.at(4u), frame.at(8u)});
    (void)call_set(rt, ctx, 0x08A17CACu, {self, obj, frame.at(0xCu), frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08A17CD8u, {self, obj, frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu), frame.at(0x20u)});
    (void)call_set(rt, ctx, 0x08A17D24u, {self, obj, frame.at(0x24u), frame.at(0x28u)});
    (void)call_set(rt, ctx, 0x08A17D50u, {self, obj, frame.at(0u), frame.at(1u)});
    (void)call_set(rt, ctx, 0x08A17E90u, {self, obj, frame.at(2u), frame.at(3u)});
    const auto integer = [&](std::uint32_t offset) { return signed_of(word(rt, frame.at(offset))); };
    const auto as_float = [](std::int32_t value) { return static_cast<float>(value); };
    const auto slot = [&](std::uint32_t offset) { return float_at(rt, frame.at(offset)); };
    const std::int32_t i14 = integer(0x14u), i1c = integer(0x1Cu);
    const std::uint32_t child = word(rt, self);
    const float width = as_float(signed_of(word(rt, frame.at(0x14u)) + word(rt, frame.at(0x18u))));
    const float height = as_float(signed_of(word(rt, frame.at(0x1Cu)) + word(rt, frame.at(0x20u))));
    const float f21 = height + slot(0x10u);
    float f20 = width + slot(0xCu);
    if (byte_u(rt, child + 4u) != 0u) {
        f20 = float_at(rt, child + 0x10u);
        put_float(rt, frame.at(0xCu), f20 - width);
    }
    if (byte_u(rt, frame.at(0u)) != 0u) f20 = f20 + float_at(rt, self + 4u);
    const std::uint32_t kind = call_set(rt, ctx, 0x08A17B38u, {self, obj});
    const std::uint32_t child2 = word(rt, self);
    if (kind == 2u) {
        const float edge_far = float_at(rt, self + 0xCu);
        put_float(rt, frame.at(4u), (edge_far - f20) + as_float(i14));
        put_float(rt, self + 0xCu, edge_far - (f20 + as_float(signed_of(word(rt, child2 + 8u)))));
    } else {
        const float edge_near = float_at(rt, self + 8u);
        put_float(rt, frame.at(4u), edge_near + as_float(i14));
        put_float(rt, self + 8u, edge_near + (f20 + as_float(signed_of(word(rt, child2 + 8u)))));
    }
    if (byte_u(rt, frame.at(0u)) != 0u) {
        if (byte_u(rt, frame.at(2u)) != 0u) {
            put_float(rt, frame.at(0xCu), float_at(rt, self + 4u) + slot(0xCu));
        } else {
            put_float(rt, frame.at(4u), psp_mul(float_at(rt, self + 4u), slot(0x24u)) + slot(4u));
        }
    }
    if (byte_u(rt, frame.at(1u)) != 0u) {
        const float top = float_at(rt, self + 0x10u);
        const float g = (float_at(rt, self + 0x14u) - top) - f21;
        if (byte_u(rt, frame.at(3u)) != 0u) {
            put_float(rt, frame.at(0x10u), g + slot(0x10u));
            put_float(rt, frame.at(8u), top + as_float(i1c));
        } else {
            put_float(rt, frame.at(8u), (top + as_float(i1c)) + psp_mul(g, slot(0x28u)));
        }
    } else {
        const float bottom = float_at(rt, self + 0x14u), top = float_at(rt, self + 0x10u);
        if (0.0f < (bottom - top) - f21) {
            const float floor_y = top + as_float(i1c);
            if (slot(8u) < floor_y) {
                put_float(rt, frame.at(8u), floor_y);
            } else {
                const float ceiling_y = (bottom - as_float(integer(0x20u))) - slot(0x10u);
                if (ceiling_y < slot(8u)) put_float(rt, frame.at(8u), ceiling_y);
            }
        } else {
            put_float(rt, frame.at(8u), top + as_float(i1c));
        }
    }
    const float x = slot(4u), y = slot(8u), w = slot(0xCu), h = slot(0x10u);
    ctx.fpr[12] = x;
    ctx.fpr[13] = y;
    ctx.fpr[14] = w;
    ctx.fpr[15] = h;
    (void)call_keep(rt, ctx, 0x08A1845Cu, {self, obj}, {x, y, w, h});
}

// func_08A4D79C(self, text, a2, a3, a4): lays out a text block. s5 = self[0x3C] (restored at the end when a4 & 0xFF is 0), self[0x34] = self[0x38] = 0; with a4 & 0xFF == 0, func_08A4CF6C(self) and func_08A50AB8(self, self[0x3C]) run first. With bit 0x100 of the flags self[0x58]: lines = 1 + the '\n' count of text (a string; flags & 0x200 with a null text gives 0), f3 = (float)lines (1.0f without bit 0x200, 0.0f for a null text);
// f12 = (self[0x68] + 14.0f (0x41600000)) * self[0x24], self[0x38] = f12 * f3, self[0x2C] = self[0x24] * 11.5f (0x41380000) moved by the alignment bits 2..3 of the byte self[0x58] (1: + (self[0x54] - self[0x4C] - self[0x38]) * 0.5f, 2: + (self[0x54] - self[0x4C] - self[0x38])); then, by the bits 0..1: none: self[0x28] = 0, func_08A4CFCC(self, text, a2, a3 & 0xFF, a4 & 0xFF) and self[0x34] = self[0x28]; else with bit 0x200 func_08A4D0E8(self, text, a2, lines, a3 & 0xFF, a4 & 0xFF; f12) and without it func_08A4CFEC(self, text, a2, a3 & 0xFF, a4 & 0xFF; f12).
// Without bit 0x100: o = self[0x2C], func_08A4CFCC(...), func_08A4D764(self) when self[0x28] > 0.0f, self[0x34] = self[0x50] - self[0x48] and self[0x38] = self[0x2C] - o.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (text != 0u) {`; `if ((low & 0xCu) == 0u) {`; `} else if (mode == 2u) {`
void func_08A4D79C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], text = ctx.gpr[5], a2 = ctx.gpr[6], s4 = ctx.gpr[7] & 0xFFu, s2 = ctx.gpr[8] & 0xFFu;
    const std::uint32_t saved = word(rt, self + 0x3Cu);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    if (s2 == 0u) {
        (void)call_set(rt, ctx, 0x08A4CF6Cu, {self});
        (void)call_set(rt, ctx, 0x08A50AB8u, {self, word(rt, self + 0x3Cu)});
    }
    const std::uint32_t flags = word(rt, self + 0x58u);
    if ((flags & 0x100u) != 0u) {
        float f3 = 1.0f;
        std::uint32_t lines = 1u;
        if ((flags & 0x200u) != 0u) {
            lines = 0u;
            f3 = 0.0f;
            if (text != 0u) {
                lines = 1u;
                for (std::uint32_t p = text;;) {
                    const std::uint32_t c = sext8(byte_u(rt, p));
                    ++p;
                    if (c == 0u) break;
                    if (c == 10u) ++lines;
                }
                f3 = static_cast<float>(static_cast<std::int32_t>(lines));
            }
        }
        const float f1 = float_at(rt, self + 0x68u) + std::bit_cast<float>(0x41600000u);
        const float f2 = float_at(rt, self + 0x24u);
        const std::uint32_t low = byte_u(rt, self + 0x58u);
        const float f4 = psp_mul(f2, std::bit_cast<float>(0x41380000u));
        const float f12 = psp_mul(f1, f2);
        const float height = psp_mul(f12, f3);
        put_float(rt, self + 0x38u, height);
        if ((low & 0xCu) == 0u) {
            put_float(rt, self + 0x2Cu, f4);
        } else {
            float f0 = float_at(rt, self + 0x54u) - float_at(rt, self + 0x4Cu);
            const std::uint32_t mode = (low >> 2u) & 3u;
            if (mode == 1u) {
                f0 = f0 - height;
                f0 = psp_mul(f0, std::bit_cast<float>(0x3F000000u));
                put_float(rt, self + 0x2Cu, f4 + f0);
            } else if (mode == 2u) {
                f0 = f0 - height;
                put_float(rt, self + 0x2Cu, f4 + f0);
            }
        }
        if ((low & 3u) == 0u) {
            put(rt, self + 0x28u, 0u);
            (void)call_set(rt, ctx, 0x08A4CFCCu, {self, text, a2, s4, s2});
            put(rt, self + 0x34u, word(rt, self + 0x28u));
        } else if ((flags & 0x200u) != 0u) {
            (void)call_setf(rt, ctx, 0x08A4D0E8u, {self, text, a2, lines, s4, s2}, {f12});
        } else {
            (void)call_setf(rt, ctx, 0x08A4CFECu, {self, text, a2, s4, s2}, {f12});
        }
    } else {
        const float old = float_at(rt, self + 0x2Cu);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A4CFCCu, {self, text, a2, s4, s2});
        if (0.0f < float_at(rt, self + 0x28u)) (void)call_set(rt, ctx, 0x08A4D764u, {self});
        const float top = float_at(rt, self + 0x50u) - float_at(rt, self + 0x48u);
        const float span = float_at(rt, self + 0x2Cu) - old;
        put_float(rt, self + 0x34u, top);
        put_float(rt, self + 0x38u, span);
    }
    if (s2 == 0u) {
        (void)call_set(rt, ctx, 0x08A4CF7Cu, {self});
        put(rt, self + 0x3Cu, saved);
    }
}

// func_08AD2B50(self, f12 = dt): under the lock self + 0x40B0 (func_08B344E0, released by the tail call func_08B3457C). With the byte self[0x279] set it is cleared and the chain of 124-byte records starting at index (signed halfword) self[0x2A4 + 6] (record k at self + 0x2A4 + 0xC + 124 * k, the next index is the signed halfword at +0xE, the chain ends at a negative one) runs the virtual slot 0x58 of every record.
// Then p = self + 0x1D0: func_08AD1040(p), s5 = the byte p[0x79] (cleared before func_08AD2204(self)), func_08AD2438(self, 0; f12 = dt, f13 = (self[0x278] ? 1.0f : 0.0f) * p[0x70]); the bit set s4 = func_08AD6EBC(self[0x29C]) and func_08AD5EC8(self[0x29C]); for every index s1 >= 0 of s4 (func_08B2A868(s4, 0), then (s4, s1 + 1)): q = func_08C3DCC4(self[0x29C] + 8, s1), func_08AD28A8(self, q) and, when the byte q[0x21] is zero, the virtual slot 0x10 of q answers a float (f0) added squared to f20.
// self[0x26C] = sqrt(f20); with s5 non-zero func_08AD11C4(self + 0x27C); self[0x27C] = self[0x26C]; func_08AD1374(self + 0x27C; f12 = dt); func_08AD665C(self[0x29C]; f12 = self[0x280] * self[0x264]), func_08AD5ED0(self[0x29C]), func_08AD2108(self) and the unlock.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (index >= 0) {`; `while (record != 0u) {`; `if (index < 0) break;`; `if (byte_u(rt, q + 0x21u) == 0u) {`
void func_08AD2B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float dt = ctx.fpr[12];
    const std::uint32_t lock = self + 0x40B0u;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t base = self + 0x2A4u, current_record = self + 0x1D0u;
    if (byte_u(rt, self + 0x279u) != 0u) {
        put8(rt, self + 0x279u, 0u);
        std::int32_t index = signed_of(sext16(half_u(rt, base + 6u)));
        if (index >= 0) {
            std::uint32_t record = base + ((static_cast<std::uint32_t>(index) << 7u) - (static_cast<std::uint32_t>(index) << 2u)) + 0xCu;
            while (record != 0u) {
                (void)call_set(rt, ctx, slot_of(rt, record, 0x58u), {record});
                index = signed_of(sext16(half_u(rt, record + 0xEu)));
                record = base + ((static_cast<std::uint32_t>(index) << 7u) - (static_cast<std::uint32_t>(index) << 2u)) + 0xCu;
                if (index < 0) break;
            }
        }
    }
    const std::uint32_t p = current_record;
    (void)call_set(rt, ctx, 0x08AD1040u, {p});
    const std::uint32_t s5 = byte_u(rt, p + 0x79u);
    put8(rt, p + 0x79u, 0u);
    (void)call_set(rt, ctx, 0x08AD2204u, {self});
    const float scale = byte_u(rt, self + 0x278u) != 0u ? 1.0f : 0.0f;
    (void)call_setf(rt, ctx, 0x08AD2438u, {self, 0u}, {dt, psp_mul(scale, float_at(rt, p + 0x70u))});
    float sum = 0.0f;
    const std::uint32_t set = call_set(rt, ctx, 0x08AD6EBCu, {word(rt, self + 0x29Cu)});
    (void)call_set(rt, ctx, 0x08AD5EC8u, {word(rt, self + 0x29Cu)});
    std::int32_t s1 = signed_of(call_set(rt, ctx, 0x08B2A868u, {set, 0u}));
    while (s1 >= 0) {
        const std::uint32_t q = call_set(rt, ctx, 0x08C3DCC4u, {word(rt, self + 0x29Cu) + 8u, static_cast<std::uint32_t>(s1)});
        (void)call_set(rt, ctx, 0x08AD28A8u, {self, q});
        if (byte_u(rt, q + 0x21u) == 0u) {
            const float r = call_setf(rt, ctx, slot_of(rt, q, 0x10u), {q}, {}).f0;
            sum = sum + psp_mul(r, r);
        }
        s1 = signed_of(call_set(rt, ctx, 0x08B2A868u, {set, static_cast<std::uint32_t>(s1) + 1u}));
    }
    const float root = std::sqrt(sum);
    put_float(rt, self + 0x26Cu, root);
    const std::uint32_t velocity = self + 0x27Cu;
    if (s5 != 0u) (void)call_set(rt, ctx, 0x08AD11C4u, {velocity});
    put(rt, velocity, word(rt, self + 0x26Cu));
    (void)call_setf(rt, ctx, 0x08AD1374u, {velocity}, {dt});
    const float f0 = float_at(rt, velocity + 4u);
    (void)call_setf(rt, ctx, 0x08AD665Cu, {word(rt, self + 0x29Cu)}, {psp_mul(f0, float_at(rt, self + 0x264u))});
    (void)call_set(rt, ctx, 0x08AD5ED0u, {word(rt, self + 0x29Cu)});
    (void)call_set(rt, ctx, 0x08AD2108u, {self});
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_08AD6EBC(self) -> v0 = self + 0x60: builds bit sets. A set is a pair {.., data pointer at +4} whose bit i is the word (i >> 5) of the data at bit mask 1 << (~i & 31) (the most significant bit first); self + 0x24 is the set walked by func_08B2A868(set, from) (the next index at or after `from`, negative when none). func_08B2AB4C(self + 0x40, self + 0x24) runs, then the sets self + 0x80 and self + 0x70 are cleared (func_08B2A8E0). cursor = self[0x34] (the output array of 8-byte entries {index, float}).
// For every index i of self + 0x24: o = func_08C3DCC4(self + 8, i); the byte o[0x1E] sets bit i of self + 0x80; with o[0x1F]: w = o[8]; with the byte o[0x21] set: o[0x1E] clears bit i of the set self + 0x50, else the bit is set when w == 0.0f or the byte o[0x20] is set; func_08B2A908(self + 0x50, i) non-zero gives w = 0, else w = FLT_MAX (0x7F7FFFFF) when the byte o[0x1D] is set and w + w otherwise, and bit i of self + 0x70 is set; with o[0x21] clear, w = o[8] * 0x3F35C28F when the byte o[0x1D] is zero.
// A positive w pushes {i, w} at cursor (cursor += 8). Then end = min(cursor, self[0x34] + 8 * halfword self[6]) (unsigned), func_08AD6A00(self[0x34], end, cursor) (a forwarder to func_08C3E4E8(first, middle, last)), the set self + 0x60 is cleared and, with budget = the byte self[6], the entries from the re-read self[0x34] below end set their bit in self + 0x60 while budget >= the byte func_08C3DCC4(self + 8, index)[0x22] (budget -= it, kept to 8 bits). func_08B2ACDC(self + 0x70, self + 0x60) and func_08B2ABE4(self + 0x50, self + 0x70) end.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, o + 0x1Eu) != 0u) set_bit(sorted, index);`; `if (byte_u(rt, o + 0x21u) != 0u) {`; `if (byte_u(rt, o + 0x1Eu) != 0u) {`; `} else if (original == 0.0f || byte_u(rt, o + 0x20u) != 0u) {`; `if (call_set(rt, ctx, 0x08B2A908u, {keep, index}) != 0u) {`; `if (byte_u(rt, o + 0x1Du) == 0u) w = original + original;`; `if (0.0f < w) {`; `} while (entry < end);`
void func_08AD6EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t walked = self + 0x24u, keep = self + 0x50u, found = self + 0x70u, chosen = self + 0x60u, sorted = self + 0x80u, objects = self + 8u;
    const auto bit_address = [&](std::uint32_t set, std::uint32_t index) { return word(rt, set + 4u) + ((index >> 5u) << 2u); };
    const auto bit_mask = [](std::uint32_t index) { return 1u << ((~index) & 31u); };
    const auto set_bit = [&](std::uint32_t set, std::uint32_t index) {
        const std::uint32_t address = bit_address(set, index);
        put(rt, address, word(rt, address) | bit_mask(index));
    };
    (void)call_set(rt, ctx, 0x08B2AB4Cu, {self + 0x40u, walked});
    (void)call_set(rt, ctx, 0x08B2A8E0u, {sorted});
    (void)call_set(rt, ctx, 0x08B2A8E0u, {found});
    const std::uint32_t begin = word(rt, self + 0x34u);
    std::uint32_t cursor = begin;
    std::int32_t s1 = signed_of(call_set(rt, ctx, 0x08B2A868u, {walked, 0u}));
    while (s1 >= 0) {
        const std::uint32_t index = static_cast<std::uint32_t>(s1);
        const std::uint32_t o = call_set(rt, ctx, 0x08C3DCC4u, {objects, index});
        if (byte_u(rt, o + 0x1Eu) != 0u) set_bit(sorted, index);
        if (byte_u(rt, o + 0x1Fu) != 0u) {
            const float original = float_at(rt, o + 8u);
            float w = original;
            if (byte_u(rt, o + 0x21u) != 0u) {
                if (byte_u(rt, o + 0x1Eu) != 0u) {
                    const std::uint32_t address = bit_address(keep, index);
                    put(rt, address, word(rt, address) & ~bit_mask(index));
                } else if (original == 0.0f || byte_u(rt, o + 0x20u) != 0u) {
                    set_bit(keep, index);
                }
                if (call_set(rt, ctx, 0x08B2A908u, {keep, index}) != 0u) {
                    w = 0.0f;
                } else {
                    w = std::bit_cast<float>(0x7F7FFFFFu);
                    if (byte_u(rt, o + 0x1Du) == 0u) w = original + original;
                    set_bit(found, index);
                }
            } else if (byte_u(rt, o + 0x1Du) == 0u) {
                w = psp_mul(original, std::bit_cast<float>(0x3F35C28Fu));
            }
            if (0.0f < w) {
                put(rt, cursor, index);
                put(rt, cursor + 4u, std::bit_cast<std::uint32_t>(w) | (index >> 31u));
                cursor += 8u;
            }
        }
        s1 = signed_of(call_set(rt, ctx, 0x08B2A868u, {walked, index + 1u}));
    }
    const std::uint32_t limit = begin + (half_u(rt, self + 6u) << 3u);
    const std::uint32_t end = cursor < limit ? cursor : limit;
    (void)call_set(rt, ctx, 0x08AD6A00u, {begin, end, cursor});  // the forwarder jumps to func_08C3E4E8(first, middle, last): a2 is the cursor the asm still holds
    (void)call_set(rt, ctx, 0x08B2A8E0u, {chosen});
    std::uint32_t budget = byte_u(rt, self + 6u);
    std::uint32_t entry = word(rt, self + 0x34u);
    if (entry < end) {
        do {
            const std::uint32_t index = word(rt, entry);
            const std::uint32_t o = call_set(rt, ctx, 0x08C3DCC4u, {objects, index});
            const std::uint32_t cost = byte_u(rt, o + 0x22u);
            const bool short_of_budget = budget < cost;
            budget = (budget - cost) & 0xFFu;
            if (short_of_budget) break;
            set_bit(chosen, index);
            entry += 8u;
        } while (entry < end);
    }
    (void)call_set(rt, ctx, 0x08B2ACDCu, {found, chosen});
    (void)call_set(rt, ctx, 0x08B2ABE4u, {keep, found});
    ctx.gpr[2] = chosen;
}

// func_08B49F68(f12 = x) -> f0: the tangent of x. x is first brought into [-pi/2, pi/2] (the reduction is done with trunc.w.s of (x + pi/2) * (1/pi), or of (pi/2 - x) * (1/pi) for x <= 0, times -pi (0xC0490FDB), then x - pi * n); then, with c1 = 0xBEE3AF08, c2 = 0xBDE41177, c3 = 0x3C82DAA2, c4 = 0x3A8CEC36 and the three arms of the asm:
// x > pi/4 (0x3F490FDB): u = pi/2 - x, f0 = ((u2 * c1 + 1) + u2 * u2 * c3) / ((u + u3 * c2) + u2 * u3 * c4); x < -pi/4: the same with u = -pi/2 - x (0xBFC90FDB); otherwise f0 = (x + (x2 * x3 * c4 + x3 * c2)) / ((x4 * c3 + x2 * c1) + 1) with x2 = x * x, x3 = x * x2, x4 = x2 * x2. The asm keeps the operand order of every add (float adds do not associate).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `const auto series = [&](float u) {`; `if (quarter_pi < x) {`; `} else if (x < -quarter_pi) {`
void func_08B49F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    float x = ctx.fpr[12];
    const float half_pi = std::bit_cast<float>(0x3FC90FDBu), minus_pi = std::bit_cast<float>(0xC0490FDBu), inv_pi = std::bit_cast<float>(0x3EA2F983u);
    const float quarter_pi = std::bit_cast<float>(0x3F490FDBu);
    const float c1 = std::bit_cast<float>(0xBEE3AF08u), c2 = std::bit_cast<float>(0xBDE41177u), c3 = std::bit_cast<float>(0x3C82DAA2u), c4 = std::bit_cast<float>(0x3A8CEC36u);
    const float negated = -x;
    const float plus = x + half_pi;
    const float minus = negated + half_pi;
    const auto whole = [&](float value) { return static_cast<float>(static_cast<std::int32_t>(ctx.fpu_float_to_word(value, 1u))); };
    if (0.0f < x) {
        x = x + psp_mul(whole(psp_mul(plus, inv_pi)), minus_pi);
    } else {
        x = -(negated + psp_mul(whole(psp_mul(minus, inv_pi)), minus_pi));
    }
    const auto series = [&](float u) {
        const float u2 = psp_mul(u, u), u3 = psp_mul(u, u2), u4 = psp_mul(u2, u2), u5 = psp_mul(u2, u3);
        const float numerator = (psp_mul(u2, c1) + 1.0f) + psp_mul(u4, c3);
        const float denominator = (u + psp_mul(u3, c2)) + psp_mul(u5, c4);
        return numerator / denominator;
    };
    if (quarter_pi < x) {
        ctx.fpr[0] = series(half_pi - x);
    } else if (x < -quarter_pi) {
        ctx.fpr[0] = series(std::bit_cast<float>(0xBFC90FDBu) - x);
    } else {
        const float x2 = psp_mul(x, x), x3 = psp_mul(x, x2), x4 = psp_mul(x2, x2), x5 = psp_mul(x2, x3);
        const float denominator = (psp_mul(x4, c3) + psp_mul(x2, c1)) + 1.0f;
        const float numerator = x + (psp_mul(x5, c4) + psp_mul(x3, c2));
        ctx.fpr[0] = numerator / denominator;
    }
}

// func_08B4FC34(out, q0, q1, f12 = t): the spherical interpolation of two 4-float quaternions. t == 0.0f copies q0 and t == 1.0f copies q1 into out. d = func_08B4A7EC(q0, q1) (the dot product, f0); d < 0.0f negates q1 into sp + 0x30 and calls itself with it (func_08B4FC34(out, q0, sp + 0x30; f12 = t)); 1.0f < d calls func_08B4FBBC(out, q0, q1; f12 = t) (the linear blend).
// Otherwise a = func_08B49F3C(f12 = d) (f24) and s = func_08B49CC8(f12 = a); s == 0.0f copies q0. Else k = 1.0f / s, w0 = func_08B49CC8(a * (1.0f - t)), w1 = func_08B49CC8(t * a); the copies of q0 (sp + 0x10) and q1 (sp) are scaled by func_08B4A6A0(copy; f12 = k * w0) and (f12 = k * w1), func_08B4A890(sp + 0x20, sp + 0x20, sp) adds them (the sum starts as a copy of the scaled q0) and out = the sum.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (t == 0.0f) {`; `if (t == 1.0f) {`; `if (d < 0.0f) {`; `if (1.0f < d) {`; `if (s == 0.0f) {`
void func_08B4FC34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], q0 = ctx.gpr[5], q1 = ctx.gpr[6];
    const float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x70u);
    const auto copy4 = [&](std::uint32_t destination, std::uint32_t source) {
        put(rt, destination, word(rt, source));
        put(rt, destination + 4u, word(rt, source + 4u));
        put(rt, destination + 8u, word(rt, source + 8u));
        put(rt, destination + 0xCu, word(rt, source + 0xCu));
    };
    if (t == 0.0f) {
        copy4(out, q0);
        return;
    }
    if (t == 1.0f) {
        copy4(out, q1);
        return;
    }
    const float d = call_setf(rt, ctx, 0x08B4A7ECu, {q0, q1}, {}).f0;
    if (d < 0.0f) {
        const std::uint32_t negated = frame.at(0x30u);
        put_float(rt, negated, -float_at(rt, q1));
        put_float(rt, negated + 4u, -float_at(rt, q1 + 4u));
        put_float(rt, negated + 8u, -float_at(rt, q1 + 8u));
        put_float(rt, negated + 0xCu, -float_at(rt, q1 + 0xCu));
        (void)call_setf(rt, ctx, 0x08B4FC34u, {out, q0, negated}, {t});
        return;
    }
    if (1.0f < d) {
        (void)call_setf(rt, ctx, 0x08B4FBBCu, {out, q0, q1}, {t});
        return;
    }
    const float a = call_setf(rt, ctx, 0x08B49F3Cu, {}, {d}).f0;
    const float s = call_setf(rt, ctx, 0x08B49CC8u, {}, {a}).f0;
    if (s == 0.0f) {
        copy4(out, q0);
        return;
    }
    const float k = 1.0f / s;
    const float w0 = call_setf(rt, ctx, 0x08B49CC8u, {}, {psp_mul(a, 1.0f - t)}).f0;
    const float w1 = call_setf(rt, ctx, 0x08B49CC8u, {}, {psp_mul(t, a)}).f0;
    const std::uint32_t first = frame.at(0x10u), second = frame.at(0u), sum = frame.at(0x20u);
    put(rt, first, word(rt, q0));
    put(rt, first + 4u, word(rt, q0 + 4u));
    put(rt, first + 8u, word(rt, q0 + 8u));
    put(rt, first + 0xCu, word(rt, q0 + 0xCu));
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {first}, {psp_mul(k, w0)});
    put(rt, second + 8u, word(rt, q1 + 8u));
    put(rt, second + 0xCu, word(rt, q1 + 0xCu));
    put(rt, second, word(rt, q1));
    put(rt, second + 4u, word(rt, q1 + 4u));
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {second}, {psp_mul(k, w1)});
    copy4(sum, first);
    (void)call_setf(rt, ctx, 0x08B4A890u, {sum, sum, second}, {});
    put(rt, out + 4u, word(rt, sum + 4u));
    put(rt, out, word(rt, sum));
    put(rt, out + 0xCu, word(rt, sum + 0xCu));
    put(rt, out + 8u, word(rt, sum + 8u));
}

// func_08C46A5C(out, m, v): the product of the 4-float vector v with the 4x4 float matrix m, out[k] = (((v[0] * m[k] + 0.0f) + v[1] * m[4 + k]) + v[2] * m[8 + k]) + v[3] * m[12 + k] (m indexed in floats; the `+ 0.0f` turns -0.0 into +0.0). When out == v (a0 == a2) all four sums are computed before any store (the stores are out[0], out[1], out[3], out[2] at the end); otherwise each out[k] is stored right after its sum, in order, so that out == m also behaves as the original.
// In the in-place arm v0 = the bits of out[0], v1 = the bits of out[1], a0 = the bits of out[3], a1 = the bits of out[2] stay behind (the asm reloads them from its spill slots); the other arm leaves them alone. The spills to the stack are not reproduced.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (out == v) {`
void func_08C46A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], m = ctx.gpr[5], v = ctx.gpr[6];
    ctx.gpr[8] = out;  // t0 and a3 as the asm's first branch delay slots leave them
    ctx.gpr[7] = m;
    const auto sum = [&](std::uint32_t k, float b0, float b1, float b2, float b3) {
        const float t0 = psp_mul(b0, float_at(rt, m + 4u * k));
        const float t1 = psp_mul(b1, float_at(rt, m + 0x10u + 4u * k));
        const float t2 = psp_mul(b2, float_at(rt, m + 0x20u + 4u * k));
        const float t3 = psp_mul(b3, float_at(rt, m + 0x30u + 4u * k));
        return (((t0 + 0.0f) + t1) + t2) + t3;
    };
    if (out == v) {
        const float b0 = float_at(rt, out), b1 = float_at(rt, out + 4u), b2 = float_at(rt, out + 8u), b3 = float_at(rt, out + 0xCu);
        const float r0 = sum(0u, b0, b1, b2, b3), r1 = sum(1u, b0, b1, b2, b3), r2 = sum(2u, b0, b1, b2, b3), r3 = sum(3u, b0, b1, b2, b3);
        put_float(rt, out, r0);
        put_float(rt, out + 4u, r1);
        put_float(rt, out + 0xCu, r3);
        put_float(rt, out + 8u, r2);
        ctx.gpr[2] = std::bit_cast<std::uint32_t>(r0);
        ctx.gpr[3] = std::bit_cast<std::uint32_t>(r1);
        ctx.gpr[4] = std::bit_cast<std::uint32_t>(r3);
        ctx.gpr[5] = std::bit_cast<std::uint32_t>(r2);
        return;
    }
    for (std::uint32_t k = 0u; k != 4u; ++k) {
        const float r = sum(k, float_at(rt, v), float_at(rt, v + 4u), float_at(rt, v + 8u), float_at(rt, v + 0xCu));
        put_float(rt, out + 4u * k, r);
    }
}

}  // namespace
}  // namespace hot_batch_96
}  // namespace gt::game

#define HOT96(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_96::name, gt::ReturnKind::kind)
#define HOT96N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_96::name, gt::ReturnKind::kind)
HOT96(0x0880E1E0u, func_0880E1E0, Word);
HOT96(0x08812A8Cu, func_08812A8C, Word);
HOT96(0x08813044u, func_08813044, Word);
// HOT96(0x088616A8u, func_088616A8, Void);  // HOT96: 3000 dense race cases (16,430 calls) and 98 fast-gate cases, none changed memory
HOT96(0x088705D4u, func_088705D4, Word);
// HOT96(0x0889B810u, func_0889B810, Word);  // HOT96: 3000 dense race cases (11,810 calls) and 87 fast-gate cases, none changed memory
HOT96(0x0889E01Cu, func_0889E01C, Word);
HOT96(0x088AA230u, func_088AA230, Void);
HOT96(0x088AB884u, func_088AB884, Word);
HOT96(0x088CB9B0u, func_088CB9B0, Word);
HOT96(0x08905300u, func_08905300, Word);
HOT96(0x089109A0u, func_089109A0, Float);
HOT96(0x089230A4u, func_089230A4, Word);
HOT96(0x08926EA0u, func_08926EA0, Float);
HOT96(0x0893B494u, func_0893B494, Word);
// HOT96(0x0893CBCCu, func_0893CBCC, Void);  // HOT96: 3000 dense race cases (12,088 calls) and 83 fast-gate cases, none changed memory
HOT96(0x0894CA04u, func_0894CA04, Void);
HOT96(0x0894F1C8u, func_0894F1C8, Void);
HOT96(0x08954898u, func_08954898, Word);
HOT96(0x08956930u, func_08956930, Word);
HOT96(0x08956B20u, func_08956B20, Word);
// HOT96(0x0895B8C4u, func_0895B8C4, Word);  // HOT96: 3000 dense race cases (12,092 calls) and 83 fast-gate cases, none changed memory
// HOT96(0x0895C140u, func_0895C140, Word);  // HOT96: 3000 dense race cases (12,088 calls) and 83 fast-gate cases, none changed memory
HOT96(0x0896A9C4u, func_0896A9C4, Void);
HOT96(0x089AA708u, func_089AA708, Word);
HOT96(0x089AAD34u, func_089AAD34, Void);
HOT96(0x089ACCA0u, func_089ACCA0, Void);
HOT96(0x089F6748u, func_089F6748, Word);
HOT96(0x08A07E28u, func_08A07E28, Word);
HOT96(0x08A17104u, func_08A17104, Word);
HOT96(0x08A18494u, func_08A18494, Word);
HOT96(0x08A4D79Cu, func_08A4D79C, Void);
HOT96(0x08AD2B50u, func_08AD2B50, Void);
HOT96(0x08AD6EBCu, func_08AD6EBC, Word);
HOT96(0x08B49F68u, func_08B49F68, Float);
HOT96(0x08B4FC34u, func_08B4FC34, Void);
HOT96(0x08C46A5Cu, func_08C46A5C, Void);
