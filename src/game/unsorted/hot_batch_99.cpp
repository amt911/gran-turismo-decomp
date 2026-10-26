// Hot batch 99: 39 functions written, 36 registered. Pool: a new probe (`b99p`, `tools.decomp_gate probe`) of all 14,157 recompiled functions of at most 420 instructions that no `src/` file defines or registers; 668 are reached by the five routes (463 of 20 or more instructions, the 2-instruction ones are import stubs). Earlier batches filtered
// candidates by "the address does not appear in src/", which also hides functions that are only CALLED by decompiled code (func_08B58874 with 9.7 million calls in the probe and func_08B52578 are in this batch): the right filter is `void func_X(` / a registration. The graphics-library state writers, matrix and polynomial float code, tree / vector container code, path and string helpers and
// per-frame call trees below come from that pool. (main includes batch 98, 6,527 functions.)
//
// Left (read, not written): copy-on-write string code (func_08B8634C, func_08B8A4C0, func_08AACAF8, func_08A32F9C, func_08BF738C), 64-bit / soft-float code (func_08B6EF68, func_08B66E88, func_08B6728C), the VFPU functions (func_08B53D2C, func_08B54D28, ...), the ones that reach sce* imports or take a lock (func_08AEFFE0, func_08AEC210, func_08AF0508, func_08B14A18, func_08B2BFDC, func_08B2BF20), atomic reference counts (func_0887D990, func_08889244), setjmp-style code (func_08B7B34C), long iterators (func_08B00E90, func_08BDBB44, func_08BD4724, func_08A92588) and func_08B20DB0 (a 0x6C0-byte frame), func_08B020EC (reads an uninitialised stack word).
//
// Not registered (3, each has a commented HOT99 line): func_08AE1D6C (the routes only reach its total <= 0 exit: the loop body never ran in 84 compared cases), func_08B57968 (the 109 compared cases all take one arm that writes values already there; four arms never ran) and func_08978080 (compared 0: its tail callee reaches the sceDisplay import in every case).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f19): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value; `call_setf` also passes the float registers. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
// Branches the compared cases do not take are listed in each function's comment; they were read against the asm by hand.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gt::game {
namespace hot_batch_99 {
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
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset, std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_set(rt, ctx, function, arguments);
}
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

// func_08C20B4C(self = a0, owner = a1): s6 = self + 8 (a three-word header: begin self[8], end self[0xC], key self[0x14]); n = (end - begin) >> 2 compared unsigned with 2. Both arms look up a node in owner[0x3C] (func_08A72768 / func_08A72780 with the key s7 = self[0x14]); when the node's first word has a virtual slot 0xF8 that answers non-zero the result is already there
// and only the shared tail runs. n < 2: temporaries at sp+0x18..0x24 are built (func_08A759E0 / func_08A76AA0 / func_08A84910) and s4 = func_08A729A4(owner[0x3C], key, sp+0x24); otherwise the slow arm walks a chain of owner[0x34][0x30] entries (virtual slots 0x18 / 0x1C / 0x10C, func_08A759BC counts them) and ends in func_08A72BD8(owner[0x3C], key, sp+4).
// The tail (0x08C20EEC): func_08A76AA0(sp+0x28); if s4[0] != 0 its virtual slot 0xC0 is called (s4[0], sp+0x28); then func_08A61584(owner, sp+0x28) and func_08A5B9EC(sp+0x28); v0 is that last call's.
// Null checks (func_08A5BB7C with the strings at 0x08CB02A4 / 0x08CB0284 / 0x08CB028C / 0x08CB02AC) are kept in order, the reads after them are made after the call as the asm does.
// Branches the compared cases do not take (checked against the asm by hand): see the coverage run in the PR.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (a0 != 0u) (void)call_virtual(rt, ctx, a0, 0xC0u, {a0, temp28});`; `if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {`; `s4 = word(rt, owner + 0x3Cu);  // the delay slot of the jal below`; `if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});`; `if (s2 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});`; `if (a0 != 0u && call_virtual(rt, ctx, a0, 0x18u, {a0}) != 0u) more = true;`; `if (more) {`; `if (!(signed_of(n) < 2)) {`; `if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB028Cu});`; `if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02ACu});`; `if (s1 == 0u) break;`; `if (word(rt, frame.at(0x40u)) == index) break;`.
void func_08C20B4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    const std::uint32_t s6 = self + 8u;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, s6 + 4u) - word(rt, self + 8u)) >> 2);
    const std::uint32_t key = word(rt, self + 0x14u);
    const std::uint32_t temp28 = frame.at(0x28u);
    const auto tail = [&](std::uint32_t s4) {
        (void)call_set(rt, ctx, 0x08A76AA0u, {temp28});
        const std::uint32_t a0 = word(rt, s4);
        if (a0 != 0u) (void)call_virtual(rt, ctx, a0, 0xC0u, {a0, temp28});
        (void)call_set(rt, ctx, 0x08A61584u, {owner, temp28});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp28});
    };
    if (count < 2u) {
        std::uint32_t s4 = call_set(rt, ctx, 0x08A72768u, {word(rt, owner + 0x3Cu), key});
        const std::uint32_t a0 = word(rt, s4);
        if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {
            tail(s4);
            return;
        }
        s4 = word(rt, owner + 0x3Cu);  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0x20u), s6, 0u});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x1Cu)});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x18u)});
        (void)call_set(rt, ctx, 0x08A84910u, {frame.at(0x24u), frame.at(0x20u), frame.at(0x1Cu), frame.at(0x18u)});
        s4 = call_set(rt, ctx, 0x08A729A4u, {s4, key, frame.at(0x24u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x20u)});
        tail(s4);
        return;
    }
    const std::uint32_t s4 = call_set(rt, ctx, 0x08A72780u, {word(rt, owner + 0x3Cu), key});
    {
        const std::uint32_t a0 = word(rt, s4);
        if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {
            tail(s4);
            return;
        }
    }
    (void)call_set(rt, ctx, 0x08A5D438u, {frame.at(0x2Cu)});
    const std::uint32_t s0 = word(rt, word(rt, owner + 0x34u) + 0x30u);
    std::uint32_t s1 = word(rt, owner + 0x3Cu);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});
    const std::uint32_t found = call_set(rt, ctx, 0x08A72768u, {s1, word(rt, s0 + 0xCu)});
    (void)call_set(rt, ctx, 0x08A76ABCu, {frame.at(0x30u), found});
    const std::uint32_t fp = frame.at(0x34u);
    if (word(rt, frame.at(0x30u)) != 0u) {
        (void)call_set(rt, ctx, 0x08A5D438u, {fp});
        const std::uint32_t s2 = word(rt, frame.at(0x30u));
        if (s2 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
        const std::uint32_t first_slot = word(rt, word(rt, s2) + 0x18u);
        (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0x14u), s6, 0u});
        (void)call_set(rt, ctx, first_slot, {s2, fp, frame.at(0x14u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x14u)});
        bool more = false;
        {
            const std::uint32_t a0 = word(rt, fp);
            if (a0 != 0u && call_virtual(rt, ctx, a0, 0x18u, {a0}) != 0u) more = true;
        }
        if (more) {
            const std::uint32_t n = call_set(rt, ctx, 0x08A759BCu, {s6});
            put(rt, frame.at(0x40u), n);
            (void)call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x38u), 0x08CF6ED8u});
            (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x3Cu)});
            s1 = word(rt, fp);
            if (!(signed_of(n) < 2)) {
                std::uint32_t index = 1u;
                for (;;) {
                    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB028Cu});
                    const std::uint32_t slot_1c = word(rt, word(rt, s1) + 0x1Cu);
                    (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x10u)});
                    (void)call_set(rt, ctx, slot_1c, {s1, frame.at(0x3Cu), frame.at(0x10u)});
                    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
                    (void)call_set(rt, ctx, 0x08A7C28Cu, {frame.at(0x38u), frame.at(0x3Cu)});
                    s1 = word(rt, frame.at(0x38u));
                    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02ACu});
                    const std::uint32_t slot_10c = word(rt, word(rt, s1) + 0x10Cu);
                    (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0xCu), s6, index});
                    (void)call_set(rt, ctx, slot_10c, {s1, fp, frame.at(0xCu)});
                    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0xCu)});
                    s1 = word(rt, fp);
                    ++index;  // the delay slot of the beqz: both paths
                    if (s1 == 0u) break;
                    if (word(rt, frame.at(0x40u)) == index) break;
                }
            }
            (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x2Cu), s1});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x3Cu)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x38u)});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {fp});
    }
    ctx.gpr[2] = word(rt, frame.at(0x2Cu));
    if (word(rt, frame.at(0x2Cu)) == 0u) {
        const std::uint32_t head = call_set(rt, ctx, 0x08A61624u, {owner});
        (void)call_set(rt, ctx, 0x08A7DA34u, {frame.at(8u), head, frame.at(0x2Cu), s6});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    }
    s1 = word(rt, owner + 0x3Cu);
    (void)call_set(rt, ctx, 0x08A75A14u, {frame.at(0u), s6});
    (void)call_set(rt, ctx, 0x08A7B0E8u, {frame.at(4u), frame.at(0u), frame.at(0x30u), frame.at(0x2Cu)});
    const std::uint32_t last = call_set(rt, ctx, 0x08A72BD8u, {s1, key, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x30u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x2Cu)});
    tail(last);
}

// func_08C23E60(self = a0, owner = a1): the same walk as func_08C20B4C compiled for another template instance (no temporary at sp+0x28, so the frame slots below it shift by 4: sp+0x28 / 0x2C / 0x30 (fp) / 0x34 / 0x38 / 0x40 (count) instead of 0x2C / 0x30 / 0x34 / 0x38 / 0x3C / 0x40) and a different tail:
// s4 is kept, a0 = owner, and the last call is func_08A61584(owner, s4) (no virtual slot 0xC0, no func_08A76AA0 temporary); v0 is that call's.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {`; `if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});`; `if (word(rt, frame.at(0x2Cu)) != 0u) {`; `if (s2 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});`; `if (a0 != 0u && call_virtual(rt, ctx, a0, 0x18u, {a0}) != 0u) more = true;`; `if (more) {`; `if (!(signed_of(n) < 2)) {`; `if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB028Cu});`; `if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02ACu});`; `if (s1 == 0u) break;`; `if (word(rt, frame.at(0x40u)) == index) break;`; `if (word(rt, frame.at(0x28u)) == 0u) {`.
void func_08C23E60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    const std::uint32_t s6 = self + 8u;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, s6 + 4u) - word(rt, self + 8u)) >> 2);
    const std::uint32_t key = word(rt, self + 0x14u);
    const auto tail = [&](std::uint32_t s4) { (void)call_set(rt, ctx, 0x08A61584u, {owner, s4}); };
    if (count < 2u) {
        std::uint32_t s4 = call_set(rt, ctx, 0x08A72768u, {word(rt, owner + 0x3Cu), key});
        const std::uint32_t a0 = word(rt, s4);
        if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {
            tail(s4);
            return;
        }
        s4 = word(rt, owner + 0x3Cu);  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0x20u), s6, 0u});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x1Cu)});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x18u)});
        (void)call_set(rt, ctx, 0x08A84910u, {frame.at(0x24u), frame.at(0x20u), frame.at(0x1Cu), frame.at(0x18u)});
        s4 = call_set(rt, ctx, 0x08A729A4u, {s4, key, frame.at(0x24u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x20u)});
        tail(s4);
        return;
    }
    const std::uint32_t s4 = call_set(rt, ctx, 0x08A72780u, {word(rt, owner + 0x3Cu), key});
    {
        const std::uint32_t a0 = word(rt, s4);
        if (a0 != 0u && call_virtual(rt, ctx, a0, 0xF8u, {a0}) != 0u) {
            tail(s4);
            return;
        }
    }
    (void)call_set(rt, ctx, 0x08A5D438u, {frame.at(0x28u)});
    const std::uint32_t s0 = word(rt, word(rt, owner + 0x34u) + 0x30u);
    std::uint32_t s1 = word(rt, owner + 0x3Cu);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});
    const std::uint32_t found = call_set(rt, ctx, 0x08A72768u, {s1, word(rt, s0 + 0xCu)});
    (void)call_set(rt, ctx, 0x08A76ABCu, {frame.at(0x2Cu), found});
    const std::uint32_t fp = frame.at(0x30u);
    if (word(rt, frame.at(0x2Cu)) != 0u) {
        (void)call_set(rt, ctx, 0x08A5D438u, {fp});
        const std::uint32_t s2 = word(rt, frame.at(0x2Cu));
        if (s2 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
        const std::uint32_t first_slot = word(rt, word(rt, s2) + 0x18u);
        (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0x14u), s6, 0u});
        (void)call_set(rt, ctx, first_slot, {s2, fp, frame.at(0x14u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x14u)});
        bool more = false;
        {
            const std::uint32_t a0 = word(rt, fp);
            if (a0 != 0u && call_virtual(rt, ctx, a0, 0x18u, {a0}) != 0u) more = true;
        }
        if (more) {
            const std::uint32_t n = call_set(rt, ctx, 0x08A759BCu, {s6});
            put(rt, frame.at(0x40u), n);
            (void)call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x34u), 0x08CF6ED8u});
            (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x38u)});
            s1 = word(rt, fp);
            if (!(signed_of(n) < 2)) {
                std::uint32_t index = 1u;
                for (;;) {
                    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB028Cu});
                    const std::uint32_t slot_1c = word(rt, word(rt, s1) + 0x1Cu);
                    (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x10u)});
                    (void)call_set(rt, ctx, slot_1c, {s1, frame.at(0x38u), frame.at(0x10u)});
                    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
                    (void)call_set(rt, ctx, 0x08A7C28Cu, {frame.at(0x34u), frame.at(0x38u)});
                    s1 = word(rt, frame.at(0x34u));
                    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02ACu});
                    const std::uint32_t slot_10c = word(rt, word(rt, s1) + 0x10Cu);
                    (void)call_set(rt, ctx, 0x08A759E0u, {frame.at(0xCu), s6, index});
                    (void)call_set(rt, ctx, slot_10c, {s1, fp, frame.at(0xCu)});
                    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0xCu)});
                    s1 = word(rt, fp);
                    ++index;  // the delay slot of the beqz: both paths
                    if (s1 == 0u) break;
                    if (word(rt, frame.at(0x40u)) == index) break;
                }
            }
            (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x28u), s1});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x38u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x34u)});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {fp});
    }
    ctx.gpr[2] = word(rt, frame.at(0x28u));
    if (word(rt, frame.at(0x28u)) == 0u) {
        const std::uint32_t head = call_set(rt, ctx, 0x08A61624u, {owner});
        (void)call_set(rt, ctx, 0x08A7DA34u, {frame.at(8u), head, frame.at(0x28u), s6});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    }
    s1 = word(rt, owner + 0x3Cu);
    (void)call_set(rt, ctx, 0x08A75A14u, {frame.at(0u), s6});
    (void)call_set(rt, ctx, 0x08A7B0E8u, {frame.at(4u), frame.at(0u), frame.at(0x2Cu), frame.at(0x28u)});
    const std::uint32_t last = call_set(rt, ctx, 0x08A72BD8u, {s1, key, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x2Cu)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x28u)});
    tail(last);
}

// func_08B2E314(index = a0, x = a1): clamps x plus a per-index offset into a per-index range (the original stores the three candidates in a stack array and picks with movz; signed compares): index 0 answers 0x10; 1, 4 and every index >= 7: x + 0x40 into [0x30, 0x4F]; 2: x + 0x60 into [0x50, 0x6F]; 3: x + 0x18 into [0x11, 0x1F]; 5: x + 0x28 into [0x20, 0x2F]; 6: x + 0x19 into [0x11, 0x1F].
// The jump table is at 0x08C93A58 (7 entries). Only v0 is the result; a0 / a1 / v1 hold the intermediate values as the original leaves them (the callers do not read them).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (index < 7u) {`; `case 2u: offset = 0x60; low = 0x50; high = 0x6F; break;`; `default: break;`; `ctx.gpr[2] = static_cast<std::uint32_t>(raised < high + 1 ? raised : high);`.
void func_08B2E314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const std::uint32_t index = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    std::int32_t offset = 0x40;
    std::int32_t low = 0x30;
    std::int32_t high = 0x4F;
    if (index < 7u) {
        switch (index) {
        case 0u:
            ctx.gpr[2] = 0x10u;
            return;
        case 2u: offset = 0x60; low = 0x50; high = 0x6F; break;
        case 3u: offset = 0x18; low = 0x11; high = 0x1F; break;
        case 5u: offset = 0x28; low = 0x20; high = 0x2F; break;
        case 6u: offset = 0x19; low = 0x11; high = 0x1F; break;
        default: break;
        }
    }
    const std::int32_t shifted = signed_of(x) + offset;
    const std::int32_t raised = shifted < low ? low : shifted;
    ctx.gpr[2] = static_cast<std::uint32_t>(raised < high + 1 ? raised : high);
}

// func_08B5596C(index = a0, value = a1, block = a2): stores `value` (or a part of it) into one field of the block by index and answers a mask of the changed group: v0 = a0 = 0 for an index >= 0x16 and for 7 / 12 (the jump table at 0x08C95604 sends both to the empty case), then
// 0/1/2: block[0x25] / [0x26] / [0x24] = byte (mask 0x200); 3/4: [0x18] / [0x19] (0x40); 5/6: [0x1C] / [0x1D] (0x80); 8/9/13: [0xA] / [8] / [9] (4); 10/11: [0x20] / [0x22] (0x100); 14: bits 6..2 of the word at +0x14 = value & 0x1F (0x20); 15: [0x15] = value (0x20); 16: [0x16] = value >> 4 arithmetic (0x20);
// 17/18: [0] / [1] (1); 19/20: [4] / [5] (2); 21: [0x28] = value, [0x29] = value >> 8, [0x2A] = value >> 16 (0x400).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (index < 0x16u) {`; `case 10u: put8(rt, block + 0x20u, value); mask = 0x100u; break;`; `case 11u: put8(rt, block + 0x22u, value); mask = 0x100u; break;`; `case 17u: put8(rt, block, value); mask = 1u; break;`; `case 18u: put8(rt, block + 1u, value); mask = 1u; break;`; `case 19u: put8(rt, block + 4u, value); mask = 2u; break;`; `case 20u: put8(rt, block + 5u, value); mask = 2u; break;`; `case 21u:`; `default: break;  // 7 and 12`.
void func_08B5596C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t index = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    const std::uint32_t block = ctx.gpr[6];
    std::uint32_t mask = 0u;
    if (index < 0x16u) {
        ctx.gpr[3] = 0x08C95604u;  // v1 = the jump table's base (case 21 replaces it with value >> 8)
        switch (index) {
        case 0u: put8(rt, block + 0x25u, value); mask = 0x200u; break;
        case 1u: put8(rt, block + 0x26u, value); mask = 0x200u; break;
        case 2u: put8(rt, block + 0x24u, value); mask = 0x200u; break;
        case 3u: put8(rt, block + 0x18u, value); mask = 0x40u; break;
        case 4u: put8(rt, block + 0x19u, value); mask = 0x40u; break;
        case 5u: put8(rt, block + 0x1Cu, value); mask = 0x80u; break;
        case 6u: put8(rt, block + 0x1Du, value); mask = 0x80u; break;
        case 8u: put8(rt, block + 0xAu, value); mask = 4u; break;
        case 9u: put8(rt, block + 8u, value); mask = 4u; break;
        case 10u: put8(rt, block + 0x20u, value); mask = 0x100u; break;
        case 11u: put8(rt, block + 0x22u, value); mask = 0x100u; break;
        case 13u: put8(rt, block + 9u, value); mask = 4u; break;
        case 14u: {
            const std::uint32_t old = word(rt, block + 0x14u);
            ctx.gpr[2] = (old & ~0x7Cu) | ((value & 0x1Fu) << 2);
            put(rt, block + 0x14u, ctx.gpr[2]);
            mask = 0x20u;
            break;
        }
        case 15u: put8(rt, block + 0x15u, value); mask = 0x20u; break;
        case 16u: put8(rt, block + 0x16u, static_cast<std::uint32_t>(signed_of(value) >> 4)); mask = 0x20u; break;
        case 17u: put8(rt, block, value); mask = 1u; break;
        case 18u: put8(rt, block + 1u, value); mask = 1u; break;
        case 19u: put8(rt, block + 4u, value); mask = 2u; break;
        case 20u: put8(rt, block + 5u, value); mask = 2u; break;
        case 21u:
            put8(rt, block + 0x2Au, static_cast<std::uint32_t>(signed_of(value) >> 16));
            ctx.gpr[3] = static_cast<std::uint32_t>(signed_of(value) >> 8);
            put8(rt, block + 0x29u, static_cast<std::uint32_t>(signed_of(value) >> 8));
            put8(rt, block + 0x28u, value);
            mask = 0x400u;
            break;
        default: break;  // 7 and 12
        }
    }
    ctx.gpr[4] = mask;
    ctx.gpr[2] = mask;
}

// func_08B7C830(dst = a0, src = a1, count = a2): strncpy. Bytes are copied up to and including the first zero, then the rest of the `count` bytes are zeroed; v0 = dst. The original copies four bytes per iteration (a count >= 4) and clears the tail with aligned word / halfword / byte stores; every path writes exactly the `count` bytes starting at dst
// (checked against the zero-fill counts of the entry points 0x08B7C8FC / 904 / 90C / 914 and the tail loop at 0x08B7C8B4: remaining = bytes after the zero byte). It reads at most `count` source bytes (four at a time inside the 4-byte blocks, all within the count).
void func_08B7C830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    bool ended = false;
    for (std::uint32_t i = 0u; i != count; ++i) {
        std::uint32_t c = 0u;
        if (!ended) {
            c = byte_u(rt, src + i);
            if (c == 0u) ended = true;
        }
        put8(rt, dst + i, c);
    }
    ctx.gpr[2] = dst;
}

// func_088B29FC(self = a0): a forwarder: tail jump to func_088B2994(self[4]) (the load is in the delay slot).
void func_088B29FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088B2994u, {word(rt, ctx.gpr[4] + 4u)});
}

// func_08C3E4E8(first = a0, middle = a1, last = a2): the heap-building and replacement step of a partial sort over 8-byte records (a signed high word at +4, an unsigned low word at +0). With s5 = (middle - first) bytes >= 0x10 a heap is built over the first s5 / 8 records by calling func_08C3E380(first, i, count, a3, rec.lo, rec.hi, &rec) from the middle node
// (s2 = (count - 2) / 2) down to 0; then every record from `middle` below `last` that orders before the root (high word less, or equal with a smaller low word) is swapped with the root (root = record, func_08C3E380(first, 0, count, a3, old.lo, old.hi, &record)); it ends in a tail jump func_08C3E450(first, middle, s0).
// s0 is not initialised when the first loop is skipped or when middle >= last: the tail then passes the caller's s0 (or the first loop's last value), as the original does (a callee-saved register read before it is written).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (signed_of(root_hi) < signed_of(a3)) {`; `} else if (a3 != root_hi) {`; `if (replace) {`.
void func_08C3E4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t middle = ctx.gpr[5];
    const std::uint32_t last = ctx.gpr[6];
    const std::uint32_t bytes = middle - first;
    std::uint32_t s0 = ctx.gpr[16];
    if (!(signed_of(bytes) < 0x10)) {
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(bytes) >> 3);
        const std::uint32_t minus2 = count - 2u;
        const std::uint32_t s2 = static_cast<std::uint32_t>(signed_of((minus2 >> 31) + minus2) >> 1);
        s0 = first + (s2 << 3);
        std::uint32_t s1 = 0u;
        for (;;) {
            const std::uint32_t lo = word(rt, s0);
            const std::uint32_t hi = word(rt, s0 + 4u);
            (void)call_set(rt, ctx, 0x08C3E380u, {first, s2 - s1, count, ctx.gpr[7], lo, hi, s0});
            s0 -= 8u;
            if (s2 == s1) break;
            ++s1;
        }
    }
    if (middle < last) {
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(bytes) >> 3);
        s0 = middle;
        do {
            const std::uint32_t a3 = word(rt, s0 + 4u);
            const std::uint32_t root_hi = word(rt, first + 4u);
            bool replace;
            if (signed_of(root_hi) < signed_of(a3)) {
                replace = true;
            } else if (a3 != root_hi) {
                replace = false;
            } else {
                replace = word(rt, first) < word(rt, s0);
            }
            if (replace) {
                const std::uint32_t root_lo = word(rt, first);
                const std::uint32_t lo = word(rt, s0);
                const std::uint32_t hi = word(rt, s0 + 4u);
                put(rt, s0, root_lo);
                put(rt, s0 + 4u, root_hi);
                (void)call_set(rt, ctx, 0x08C3E380u, {first, 0u, count, a3, lo, hi, s0});
            }
            s0 += 8u;
        } while (s0 < last);
    }
    tail_call(ctx, 0x08C3E450u, {first, middle, s0});
}

// func_08B58874(mode = a0): one bit of the graphics library's dirty set per mode, handed to func_08B58814(state, 1 << n) as a tail jump (a0 = 0x08D6CE00, a1 = 1 << n; every other argument register stays the caller's). n = mode - 0x64 for modes 0x64..0x6A and 0x6C..0x75 (v0 = 1 for the first range,
// 0 for 0x6C..0x6D, 1 for 0x6F..0x75), 7 for mode 2, 10 for mode 0x8002 (v0 = 0x8002). Modes 0x77 and 0x78 set a byte (state[0x148] / state[0x240] = 1) and a flag in the word at state + 0x6C (|= 8 / |= 0x200) and answer v0 = state. Every other mode (0, 1, 3..0x63, 0x6B, 0x6E, 0x76, 0x79..) returns with v0 = 0 (0x8002 for the modes above 0x78 that are not 0x8002).
// v1 is the shift (mode - 0x64) or the new flag word as the original leaves it; the tail callee sets a2 / a3 itself before it reads them.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `const auto leave = [&](std::uint32_t v0, std::uint32_t v1) {`; `if (mode < 2u) {`; `if (shift < 7u) {`; `if (mode - 0x6Fu < 7u) {`; `if (mode == 0x78u) {`; `if (mode == 0x8002u) {`.
void func_08B58874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = ctx.gpr[4];
    const auto bit = [&](std::uint32_t shift, std::uint32_t v0) {
        ctx.gpr[2] = v0;
        ctx.gpr[3] = shift;
        tail_call(ctx, 0x08B58814u, {kGuState, 1u << (shift & 31u)});
    };
    const auto leave = [&](std::uint32_t v0, std::uint32_t v1) {
        ctx.gpr[2] = v0;
        ctx.gpr[3] = v1;
    };
    const auto flag = [&](std::uint32_t byte_offset, std::uint32_t flag_bits) {
        const std::uint32_t flags = word(rt, kGuState + 0x6Cu);
        put8(rt, kGuState + byte_offset, 1u);
        put(rt, kGuState + 0x6Cu, flags | flag_bits);
        ctx.gpr[2] = kGuState;
        ctx.gpr[3] = flags | flag_bits;
        ctx.gpr[4] = 1u;
    };
    if (mode < 0x6Eu) {
        if (mode >= 0x6Cu) {
            bit(mode - 0x64u, 0u);
            return;
        }
        if (mode == 2u) {
            bit(7u, 0u);
            return;
        }
        const std::uint32_t shift = mode - 0x64u;
        if (mode < 2u) {
            leave(0u, shift);
            return;
        }
        if (shift < 7u) {
            bit(shift, 1u);
            return;
        }
        ctx.gpr[5] = 1u;  // the delay slot of the beqz at 0x08B588A0 (modes 3..0x63 and 0x6B)
        leave(0u, shift);
        return;
    }
    if (mode == 0x77u) {
        flag(0x148u, 8u);
        return;
    }
    if (mode < 0x78u) {
        const std::uint32_t shift = mode - 0x64u;
        if (mode - 0x6Fu < 7u) {
            bit(shift, 1u);
            return;
        }
        leave(0u, shift);
        return;
    }
    if (mode == 0x78u) {
        flag(0x240u, 0x200u);
        return;
    }
    if (mode == 0x8002u) {
        bit(10u, 0x8002u);
        return;
    }
    leave(0x8002u, 10u);
}

// func_08B52578(block = a0): the graphics library's colour-lookup upload. The 11 words at block + 0x18 are copied to 0x08D6D048 (block + 0x18 is also read below as t2: [0xA] a row count, [0xC] a type). The display-list pointer is state[0x5C] (state = 0x08D6CE00):
// block[0] != 0: two command words (0x10000000 | (block[0] >> 24) << 16, 0x0A000000 | block[0] & 0xFFFFFF), state[0x27C] = 0x800, the pointer += 8. Otherwise four words 0x48000000 / 0x49000000 / 0x4A000000 / 0x4B000000 | (block[8] / [0xC] / [0x10] / [0x14] >> 8) & 0xFFFFFF, then for each row i = 0, 1, ... while
// block[0x22] >= i + 1 (a do-while that runs at least once) three words from the row at block + 0x40 + 8 i: 0xA0000000 + (i << 24) | row[8] & 0xFFFFFF; 0xA8000000 + (i << 24) with bits 23..16 = row[0xB] and bits 10..0 = half row[0xC]; 0xB8000000 + (i << 24) with bits 15..8 = row[0xF] and bits 7..0 = row[0xE];
// and for a type (block[0x24] - 4) & 0xFF < 4 two more: 0xB0000000 | block[0x44] & 0xFFFFFF and 0xB1000000 | row byte block[0x47] << 16. state[0x27C] = 0xFFF and the pointer are stored. Both arms end with state[0x278] = 0xFFF and state[0x6C] |= 0x80; v0 = state, v1 = the new flag word.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (first != 0u) {`; `} else {`; `if (signed_of(byte_u(rt, t2 + 0xAu)) < signed_of(row_index)) break;`; `if (((byte_u(rt, t2 + 0xCu) - 4u) & 0xFFu) < 4u) {`.
void func_08B52578(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = ctx.gpr[4];
    const std::uint32_t t2 = block + 0x18u;
    for (std::uint32_t i = 0u; i != 11u; ++i) put(rt, 0x08D6D048u + 4u * i, word(rt, t2 + 4u * i));
    const std::uint32_t first = word(rt, block);
    if (first != 0u) {
        const std::uint32_t list = word(rt, kGuState + 0x5Cu);
        put(rt, list, 0x10000000u | ((first >> 24) << 16));
        put(rt, list + 4u, 0x0A000000u | (word(rt, block) & 0xFFFFFFu));
        put(rt, kGuState + 0x27Cu, 0x800u);
        put(rt, kGuState + 0x5Cu, list + 8u);
    } else {
        const std::uint32_t list = word(rt, kGuState + 0x5Cu);
        put(rt, list, 0x48000000u | ((word(rt, block + 8u) >> 8) & 0xFFFFFFu));
        put(rt, list + 4u, 0x49000000u | ((word(rt, block + 0xCu) >> 8) & 0xFFFFFFu));
        put(rt, list + 8u, 0x4A000000u | ((word(rt, block + 0x10u) >> 8) & 0xFFFFFFu));
        put(rt, list + 0xCu, 0x4B000000u | ((word(rt, block + 0x14u) >> 8) & 0xFFFFFFu));
        std::uint32_t out = list + 0x10u;
        std::uint32_t row = block + 0x40u;
        std::uint32_t t4 = 0xA0000000u;
        std::uint32_t t5 = 0xA8000000u;
        std::uint32_t t6 = 0xB8000000u;
        std::uint32_t row_index = 0u;
        for (;;) {
            put(rt, out, (t4 & 0xFF000000u) | (word(rt, row + 8u) & 0xFFFFFFu));
            const std::uint32_t row_byte = byte_u(rt, row + 0xBu);
            const std::uint32_t row_half = half_u(rt, row + 0xCu);
            ++row_index;
            std::uint32_t second = (t5 & ~0xFF0000u) | (row_byte << 16);
            second = (second & ~0x7FFu) | (row_half & 0x7FFu);
            put(rt, out + 4u, second);
            const std::uint32_t third_base = t6;  // the original copies t6 to a0 before it advances the three counters
            t4 += 0x01000000u;
            t5 += 0x01000000u;
            t6 += 0x01000000u;
            const std::uint32_t hi_byte = byte_u(rt, row + 0xFu);
            const std::uint32_t lo_byte = byte_u(rt, row + 0xEu);
            put(rt, out + 8u, (third_base & 0xFFFF0000u) | (hi_byte << 8) | lo_byte);
            row += 8u;
            out += 0xCu;
            if (signed_of(byte_u(rt, t2 + 0xAu)) < signed_of(row_index)) break;
        }
        if (((byte_u(rt, t2 + 0xCu) - 4u) & 0xFFu) < 4u) {
            put(rt, out, 0xB0000000u | (word(rt, block + 0x44u) & 0xFFFFFFu));
            put(rt, out + 4u, 0xB1000000u | (byte_u(rt, block + 0x47u) << 16));
            out += 8u;
        }
        put(rt, kGuState + 0x27Cu, 0xFFFu);
        put(rt, kGuState + 0x5Cu, out);
    }
    const std::uint32_t flags = word(rt, kGuState + 0x6Cu);
    put(rt, kGuState + 0x278u, 0xFFFu);
    put(rt, kGuState + 0x6Cu, flags | 0x80u);
    ctx.gpr[2] = kGuState;
    ctx.gpr[3] = flags | 0x80u;
    ctx.gpr[4] = 0xFFFu;
}

// func_08B57968(x = a0, kind = a1, f12): kind - 6 selects a case of the jump table at 0x08C95728 (5 entries; any other kind returns with v0 = 0). With s1 = x - 0x65:
// 6: tail jump func_08B577F8(state, 0x87, s1) (state = 0x08D6CE00); 7: when f12 == the float at 0x08CB61C4 the byte state[s1 * 0x28 + 0x1C1] = 0, otherwise it is 1 and func_08B58178(f12) (a VFPU sine/cosine helper) gives f0; then func_08B577F8(state, 0x8B, s1) with f12 = f0; both then state[0x6C] |= 0x400 and v0 = that word, v1 = state;
// 8 / 9 / 10: tail jump func_08B57938(state, 0x7B / 0x7C / 0x7D, s1). In the tail-jump cases v0 is the table slot's address as the original leaves it.
void func_08B57968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t index = ctx.gpr[5] - 6u;
    const std::uint32_t s1 = ctx.gpr[4] - 0x65u;
    ctx.gpr[5] = index;
    if (!(index < 5u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = 0x08C95728u + (index << 2);
    ctx.gpr[3] = 0x08C95728u;
    switch (index) {
    case 0u:
        tail_call(ctx, 0x08B577F8u, {kGuState, 0x87u, s1});
        return;
    case 1u: {
        const std::uint32_t entry = kGuState + s1 * 0x28u + 0x1C1u;
        ctx.fpr[0] = float_at(rt, 0x08CB61C4u);
        if (ctx.fpr[12] == ctx.fpr[0]) {
            put8(rt, entry, 0u);
        } else {
            put8(rt, entry, 1u);
            (void)call_keep(rt, ctx, 0x08B58178u, {}, {ctx.fpr[12]});
            ctx.fpr[12] = ctx.fpr[0];
            (void)call_setf(rt, ctx, 0x08B577F8u, {kGuState, 0x8Bu, s1}, {ctx.fpr[12]});
        }
        const std::uint32_t flags = word(rt, kGuState + 0x6Cu) | 0x400u;
        put(rt, kGuState + 0x6Cu, flags);
        ctx.gpr[2] = flags;
        ctx.gpr[3] = kGuState;
        return;
    }
    case 2u:
        tail_call(ctx, 0x08B57938u, {kGuState, 0x7Bu, s1});
        return;
    case 3u:
        tail_call(ctx, 0x08B57938u, {kGuState, 0x7Cu, s1});
        return;
    default:
        tail_call(ctx, 0x08B57938u, {kGuState, 0x7Du, s1});
        return;
    }
}

// func_08AE1D6C(a0 = owner, key = a1, limit = a2, table = a3, count = t0 (fp), pos = t1 (s4), total = t2 (s6), flags = t3): walks `total` positions in chunks and answers the new position (v0 = s4). When key | limit is 0 both become -1. While s7 < total (do-while on s6 > 0, else v0 = t1):
// s2 = total - s7, cut to limit - pos when limit >= 0 (signed min); s3 = pos + s2. When the table (12-byte rows at table + 4 + 12 i, word keys) is non-null, s2 > 0 and count > 0, every row i whose key is not below pos and is below s3 calls func_08AE0CD8(owner, i) (do-while over count rows).
// Then, when limit >= 0 and s3 >= limit: pos = key and flags (when non-null) |= 0x100 (pos = key even for a null flags pointer: the delay slot of the beqz loads it); otherwise pos = s3. s7 += s2.
void func_08AE1D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    std::uint32_t key = ctx.gpr[5];
    std::uint32_t limit = ctx.gpr[6];
    const std::uint32_t table = ctx.gpr[7];
    const std::uint32_t count = ctx.gpr[8];
    std::uint32_t pos = ctx.gpr[9];
    const std::uint32_t total = ctx.gpr[10];
    const std::uint32_t flags = ctx.gpr[11];
    if ((key | limit) == 0u) {
        key = 0xFFFFFFFFu;
        limit = 0xFFFFFFFFu;
    }
    if (!(signed_of(total) <= 0)) {
        const bool limited = signed_of(limit) >= 0;
        std::uint32_t done = 0u;
        do {
            std::uint32_t step = total - done;
            if (limited) {
                const std::uint32_t room = limit - pos;
                if (signed_of(room) < signed_of(step)) step = room;
            }
            const std::uint32_t end = pos + step;
            if (table != 0u && signed_of(step) > 0 && signed_of(count) > 0) {
                std::uint32_t i = 0u;
                do {
                    const std::uint32_t row_key = word(rt, table + 4u + 0xCu * i);
                    if (!(signed_of(row_key) < signed_of(pos)) && signed_of(row_key) < signed_of(end)) (void)call_set(rt, ctx, 0x08AE0CD8u, {owner, i});
                    ++i;
                } while (signed_of(i) < signed_of(count));
            }
            if (limited && !(signed_of(end) < signed_of(limit))) {
                pos = key;
                if (flags != 0u) put(rt, flags, word(rt, flags) | 0x100u);
            } else {
                pos = end;
            }
            done += step;
        } while (signed_of(done) < signed_of(total));
    }
    ctx.gpr[2] = pos;
}

// func_08B58218(mask = a0): pushes a six-word command block on the graphics display list (state = 0x08D6CE00, list pointer state[0x5C], the block pointer state[0x60] moves down 0x18). func_08B59118(state, 0x3C) is called first. t7 = 0 and t8 = 1; mask & 2: t7 = state[0xA4] with its top byte cleared and t8 = 0x101; mask & 0x10: t8 |= 0x200, t7 |= state[0xDC] << 24;
// mask & 4: t8 |= 0x400 and s0 = trunc(state[0xC4] * state[0xC8]) & 0xFFFF (when the product is >= the float at 0x08CB61D4 the constant is subtracted first and bit 31 is set in the integer: the unsigned conversion), otherwise s0 = 0.
// The block at a0 = state[0x60] - 0x18 gets halfwords 0 at +4 / +6 and s0 at +8, and the block at a2 = state[0x60] - 0xC halfwords state[0xB0] / state[0xB2] / s0 at +4 / +6 / +8; t7 is stored at a0[0] and a2[0]. The list gets 0xD3000000 | t8 & 0xFFFFFF, 0x10000000 | (a0 >> 24) << 16, 0x01000000 | a0 & 0xFFFFFF, 0x1280011C, 0x04060002, 0xD3000000 and the pointer moves on 0x18; v0 = the old list pointer.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (limit <= product) {`.
void func_08B58218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mask = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x08B59118u, {kGuState, 0x3Cu});
    std::uint32_t t7 = 0u;
    std::uint32_t t8 = 1u;
    if ((mask & 2u) != 0u) {
        t7 = word(rt, kGuState + 0xA4u) & 0x00FFFFFFu;
        t8 = 0x101u;
    }
    if ((mask & 0x10u) != 0u) {
        t8 |= 0x200u;
        t7 |= word(rt, kGuState + 0xDCu) << 24;
    }
    std::uint32_t s0 = 0u;
    if ((mask & 4u) != 0u) {
        float product = psp_mul(float_at(rt, kGuState + 0xC4u), float_at(rt, kGuState + 0xC8u));
        const float limit = float_at(rt, 0x08CB61D4u);
        t8 |= 0x400u;
        std::uint32_t converted;
        if (limit <= product) {
            product = product - limit;
            converted = ctx.fpu_float_to_word_ct<1u>(product) | 0x80000000u;
        } else {
            converted = ctx.fpu_float_to_word_ct<1u>(product);
        }
        s0 = converted & 0xFFFFu;
    }
    const std::uint32_t third = word(rt, kGuState + 0x60u);
    const std::uint32_t half_b0 = half_u(rt, kGuState + 0xB0u);
    const std::uint32_t half_b2 = half_u(rt, kGuState + 0xB2u);
    const std::uint32_t a0 = third - 0x18u;
    const std::uint32_t a2 = third - 0xCu;
    put(rt, kGuState + 0x60u, a0);
    put16(rt, a0 + 4u, 0u);
    put16(rt, a0 + 6u, 0u);
    put16(rt, a0 + 8u, s0);
    put(rt, third - 0xCu, t7);
    put16(rt, a2 + 4u, half_b0);
    put16(rt, a2 + 6u, half_b2);
    put16(rt, a2 + 8u, s0);
    put(rt, third - 0x18u, t7);
    const std::uint32_t list = word(rt, kGuState + 0x5Cu);
    put(rt, list, 0xD3000000u | (t8 & 0xFFFFFFu));
    put(rt, list + 4u, 0x10000000u | (((a0 >> 24) & 0xFFu) << 16));
    put(rt, list + 8u, 0x01000000u | (a0 & 0xFFFFFFu));
    put(rt, list + 0xCu, 0x1280011Cu);
    put(rt, list + 0x10u, 0x04060002u);
    put(rt, list + 0x14u, 0xD3000000u);
    put(rt, kGuState + 0x5Cu, list + 0x18u);
    ctx.gpr[2] = list;
}

// func_08C238F0(self = a0, out = a1): builds a 0x34-byte object from self[8] (func_08A5BC20(0x34, 4) allocates, func_08A90D6C(object, self[8]) fills it, func_08A90470(sp+0x18, sp+0xC) wraps it: the smart pointer's target is s4). A null self[8] or null target reports through func_08A5BB7C(0x08CB02F0) and carries on.
// Two lists of 12-byte records are then read from the target (begin / end words at +0x24 and +0x18; count = ((end - begin) >> 2) * 0xAAAAAAAB, the multiplicative inverse of 3): for each list a buffer is made with func_08A6159C(out, count) and, for record i (pointer begin + 12 i + 8), when the buffer slot s0 is not that very pointer func_08A5BAD8(record + 8, *s0) is called; s0 += 4 either way.
// Then func_08A61624(out) gives s1 and func_08A90BF4(target, s1) is called, two names are built with func_08A6972C (sp+0x10 from the string at 0x08CF7008, sp from sp+0x10), func_08A74F00(sp+4, sp, sp+0x18) combines them, a 0x18-byte object (func_08A5BC20(0x18, 4), func_08A87040(object, sp+4)) is wrapped (func_08A76AB4(sp+0x14, &object)),
// func_08A61584(out, sp+0x14) stores it, and the temporaries are released with func_08A5B9EC / func_08A69D68. v0 is the last release's.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {kNull});`; `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {kNull});`; `if (s0 != record) (void)call_set(rt, ctx, 0x08A5BAD8u, {record, word(rt, s0)});`.
void func_08C238F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t kNull = 0x08CB02F0u;
    const std::uint32_t source = word(rt, self + 8u);
    if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x34u, 4u});
    (void)call_set(rt, ctx, 0x08A90D6Cu, {object, source});
    put(rt, frame.at(0xCu), object);
    (void)call_set(rt, ctx, 0x08A90470u, {frame.at(0x18u), frame.at(0xCu)});
    const auto copy_list = [&](std::uint32_t offset) {
        const std::uint32_t target = word(rt, frame.at(0x18u));
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
        const std::uint32_t list = target + offset;
        const std::uint32_t first_count = static_cast<std::uint32_t>(signed_of(word(rt, list + 4u) - word(rt, list)) >> 2) * 0xAAAAAAABu;
        std::uint32_t s0 = call_set(rt, ctx, 0x08A6159Cu, {out, first_count});
        std::uint32_t row = 0u;
        std::uint32_t index = 0u;
        for (;;) {
            const std::uint32_t begin = word(rt, list);
            const std::uint32_t end = word(rt, list + 4u);
            const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - begin) >> 2) * 0xAAAAAAABu;
            const std::uint32_t record = row + begin + 8u;
            const bool more = index < count;
            row += 0xCu;
            ++index;
            if (!more) break;
            if (s0 != record) (void)call_set(rt, ctx, 0x08A5BAD8u, {record, word(rt, s0)});
            s0 += 4u;
        }
    };
    copy_list(0x24u);
    copy_list(0x18u);
    const std::uint32_t s1 = call_set(rt, ctx, 0x08A61624u, {out});
    const std::uint32_t target = word(rt, frame.at(0x18u));
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
    (void)call_set(rt, ctx, 0x08A90BF4u, {target, s1});
    (void)call_set(rt, ctx, 0x08A6972Cu, {frame.at(0x10u), 0x08CF7008u});
    (void)call_set(rt, ctx, 0x08A6972Cu, {frame.at(0u), frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08A74F00u, {frame.at(4u), frame.at(0u), frame.at(0x18u)});
    const std::uint32_t second = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_set(rt, ctx, 0x08A87040u, {second, frame.at(4u)});
    put(rt, frame.at(8u), second);
    (void)call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x14u), frame.at(8u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A61584u, {out, frame.at(0x14u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
}

// func_089B684C(self = a0, s3 = a1): after func_08A02218(self, a1), creates up to three child objects the first time each is needed and stores them in self[0x120] / [0x124] / [0x128]; then, when self[0x60] & 4, binds three named slots, and ends in the virtual call self->vtable[0x250](self, a1); v0 is that call's.
// Each child is made like this: a temporary at sp+0x18 / 0x1C (func_08A76AA0), a name from the string at 0x08CA5978 / 0x08CA5980 / 0x08CA5988 (func_08A69B3C into sp+0x14 / 0x10 / 0x0C), a lookup func_08A7BFD0(owner, temporary, name) and, when the lookup found something, func_08969244 (the first and third) / func_08976F68 (the second) wraps it into the other temporary,
// the wrapped pointer is stored in the slot, and func_0896A9C4(pointer, 0, sp+0x20 (cleared), 0, 0) runs (the first also func_08966818(self[0x120], 0), the second func_0896A824(self[0x124], 1)). The second child is looked up through self[0x120], the third likewise. A block is skipped when its slot is already set; the second and third blocks are also skipped when self[0x120] is still null (the beql at 0x089B6918 / 0x089B69C8 jumps to the flag test).
// The flag arm (self[0x60] & 4) looks up the three names at 0x08CA5990 / 0x08CA599C / 0x08CA59AC into self + 0x12C / 0x130 / 0x134 with func_08A7BFD0(self, slot, name).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (word(rt, found_slot) != 0u) {`; `if (word(rt, self + 0x120u) == 0u) {`; `if (word(rt, self + 0x120u) == 0u) to_flags = true;`; `if (!to_flags && word(rt, self + 0x124u) == 0u) {`; `if (!to_flags && word(rt, self + 0x128u) == 0u) {`; `if ((word(rt, self + 0x60u) & 4u) != 0u) {`.
void func_089B684C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t s3 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    (void)call_set(rt, ctx, 0x08A02218u, {self, s3});
    const auto make_child = [&](std::uint32_t owner, std::uint32_t name, std::uint32_t name_slot, std::uint32_t found_slot, std::uint32_t wrapped_slot, std::uint32_t wrap, std::uint32_t slot, bool after_one, bool after_two) {
        (void)call_set(rt, ctx, 0x08A76AA0u, {found_slot});
        (void)call_set(rt, ctx, 0x08A69B3Cu, {name_slot, name});
        (void)call_set(rt, ctx, 0x08A7BFD0u, {owner, found_slot, name_slot});
        (void)call_set(rt, ctx, 0x08A69D68u, {name_slot});
        if (word(rt, found_slot) != 0u) {
            (void)call_set(rt, ctx, wrap, {wrapped_slot, found_slot});
            const std::uint32_t child = word(rt, wrapped_slot);
            put(rt, self + slot, child);
            put(rt, frame.at(0x20u), 0u);
            (void)call_set(rt, ctx, 0x0896A9C4u, {child, 0u, frame.at(0x20u), 0u, 0u});
            if (after_one) (void)call_set(rt, ctx, 0x08966818u, {word(rt, self + 0x120u), 0u});
            if (after_two) (void)call_set(rt, ctx, 0x0896A824u, {word(rt, self + 0x124u), 1u});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {wrapped_slot});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {found_slot});
    };
    bool to_flags = false;
    if (word(rt, self + 0x120u) == 0u) {
        make_child(self, 0x08CA5978u, frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu), 0x08969244u, 0x120u, true, false);
        if (word(rt, self + 0x120u) == 0u) to_flags = true;
    }
    if (!to_flags && word(rt, self + 0x124u) == 0u) {
        make_child(word(rt, self + 0x120u), 0x08CA5980u, frame.at(0x10u), frame.at(0x1Cu), frame.at(0x18u), 0x08976F68u, 0x124u, false, true);
        if (word(rt, self + 0x120u) == 0u) to_flags = true;
    }
    if (!to_flags && word(rt, self + 0x128u) == 0u) {
        make_child(word(rt, self + 0x120u), 0x08CA5988u, frame.at(0xCu), frame.at(0x18u), frame.at(0x1Cu), 0x08969244u, 0x128u, false, false);
    }
    if ((word(rt, self + 0x60u) & 4u) != 0u) {
        (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(8u), 0x08CA5990u});
        (void)call_set(rt, ctx, 0x08A7BFD0u, {self, self + 0x12Cu, frame.at(8u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(8u)});
        (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(4u), 0x08CA599Cu});
        (void)call_set(rt, ctx, 0x08A7BFD0u, {self, self + 0x130u, frame.at(4u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(4u)});
        (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CA59ACu});
        (void)call_set(rt, ctx, 0x08A7BFD0u, {self, self + 0x134u, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    }
    (void)call_virtual(rt, ctx, self, 0x250u, {self, s3});
}

// Shared by func_08C1C1A8 / func_08BE9D90 (the same unique-key insert of a red-black tree whose keys are counted strings: a string's length is the word at data - 0xC, a node's key pointer is at node + 0x10, left / right children at node + 8 / 0xC, the tree's header at tree + 4, its leftmost node at tree[0xC]).
// The descent compares the key with each node: n = min(lengths), r = func_08B36170(key, node key, n) (a memcmp), the order is r when non-zero, else the length difference; a negative order goes left. The last node visited is s4; when the key does not belong before s4 the predecessor candidate is s4 itself,
// otherwise s4 is the leftmost node (insert at once) or its predecessor func_08B5F174(s4) is compared again (order of predecessor against key, memcmp(predecessor key, key, n)); an order >= 0 means the key exists (flag 0, result = that node), a negative one inserts with `insert`(tree, 0, s4, key) (flag 1, result = the new node).
// v0 = the node, v1 = the word at sp + 0xC whose low byte is the flag (the upper bytes are stale stack, not compared).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (compared != 0u) order = compared;`; `if (insert_now) s2 = call_set(rt, ctx, insert, {tree, 0u, s4, key});`; `const std::uint32_t flag = insert_now ? 1u : 0u;`.
void rb_find_insert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t insert) {
    const std::uint32_t tree = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    std::uint32_t s2 = word(rt, tree + 8u);
    std::uint32_t s4 = tree + 4u;
    bool predecessor_check = false;
    if (s2 != 0u) {
        for (;;) {
            const std::uint32_t text = word(rt, key);
            const std::uint32_t key_length = word(rt, text - 0xCu);
            const std::uint32_t node_text = word(rt, s2 + 0x10u);
            const std::uint32_t node_length = word(rt, node_text - 0xCu);
            std::uint32_t order = key_length - node_length;
            const std::uint32_t shorter = node_length < key_length ? node_length : key_length;
            const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {text, node_text, shorter});
            if (compared != 0u) order = compared;
            const std::uint32_t next = (order >> 31) == 0u ? word(rt, s2 + 0xCu) : word(rt, s2 + 8u);
            const bool goes_left = (order >> 31) != 0u;
            if (next != 0u) {
                s2 = next;
                continue;
            }
            s4 = s2;
            if (!goes_left) predecessor_check = true;
            break;
        }
    }
    bool insert_now = false;
    if (!predecessor_check) {
        if (s4 == word(rt, tree + 0xCu)) {
            insert_now = true;
        } else {
            s2 = call_set(rt, ctx, 0x08B5F174u, {s4});
        }
    }
    if (!insert_now) {
        const std::uint32_t node_text = word(rt, s2 + 0x10u);
        const std::uint32_t text = word(rt, key);
        const std::uint32_t node_length = word(rt, node_text - 0xCu);
        const std::uint32_t key_length = word(rt, text - 0xCu);
        const std::uint32_t shorter = key_length < node_length ? key_length : node_length;
        std::uint32_t order = node_length - key_length;
        const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {node_text, text, shorter});
        if (compared != 0u) order = compared;
        insert_now = signed_of(order) < 0;
    }
    if (insert_now) s2 = call_set(rt, ctx, insert, {tree, 0u, s4, key});
    const std::uint32_t flag = insert_now ? 1u : 0u;
    put8(rt, frame.at(0xCu), flag);
    ctx.gpr[2] = s2;
    ctx.gpr[3] = word(rt, frame.at(0xCu));
}

// func_08C1C1A8(tree = a0, key = a1): unique-key insert of a counted string key (rb_find_insert with the node constructor func_08C1C0A4).
void func_08C1C1A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C1C0A4u); }

// func_08BE9D90(tree = a0, key = a1): the same for another instance (node constructor func_08BE9C90).
void func_08BE9D90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08BE9C90u); }

// func_08A7D24C(self = a0): clears a list under a lock. v0 = [0x08CF6FDC]; when self[0x18] already equals it nothing is done (and v0 is that word). Otherwise func_08B344E0(0x08CFF450) takes the lock, a temporary at sp is made from self + 0x18 (func_08A6972C), self + 0x18 is set to [0x08CF6FDC] (func_08A6A23C(self + 0x18, 0x08CF6FDC)) and every node of the list at self + 0x20 (sentinel self + 0x24, first node at s4[0xC],
// next node func_08B5F100(node), node payload at node[0x14], a null payload reports through func_08A5BB7C(0x08CB1358)) gets func_08AAD700(0x08CF705C, self, func_08A5D444(payload)) and func_08A5D460(payload, self). The list is then emptied: func_08C00860(self + 0x20, self[0x28]), self[0x30] = self[0x2C] = sentinel, self[0x34] = 0, self[0x28] = 0; func_08A6A23C(self + 0x18, sp), func_08A69D68(sp) and func_08B3457C(0x08CFF450) end it.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (payload == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1358u});`.
void func_08A7D24C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t s6 = self + 0x18u;
    const std::uint32_t reference = word(rt, 0x08CF6FDCu);
    if (word(rt, self + 0x18u) == reference) {
        ctx.gpr[2] = reference;
        return;
    }
    (void)call_set(rt, ctx, 0x08B344E0u, {0x08CFF450u});
    (void)call_set(rt, ctx, 0x08A6972Cu, {frame.at(0u), s6});
    const std::uint32_t s4 = self + 0x20u;
    (void)call_set(rt, ctx, 0x08A6A23Cu, {s6, 0x08CF6FDCu});
    const std::uint32_t s3 = self + 0x24u;
    std::uint32_t node = word(rt, s4 + 0xCu);
    if (node != s3) {
        do {
            std::uint32_t payload = word(rt, node + 0x14u);
            if (payload == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1358u});
            const std::uint32_t text = call_set(rt, ctx, 0x08A5D444u, {payload});
            (void)call_set(rt, ctx, 0x08AAD700u, {0x08CF705Cu, self, text});
            payload = word(rt, node + 0x14u);
            if (payload == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1358u});
            (void)call_set(rt, ctx, 0x08A5D460u, {payload, self});
            node = call_set(rt, ctx, 0x08B5F100u, {node});
        } while (s3 != node);
    }
    const std::uint32_t root = word(rt, s4 + 8u);
    (void)call_set(rt, ctx, 0x08C00860u, {s4, root});
    put(rt, s4 + 0x10u, s3);
    put(rt, s4 + 0x14u, 0u);
    put(rt, s4 + 0xCu, s3);
    put(rt, s4 + 8u, 0u);
    (void)call_set(rt, ctx, 0x08A6A23Cu, {s6, frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08B3457Cu, {0x08CFF450u});
}

// func_08AAACF4(self = a0, ref = a1): ref[0] == 0: tail jump func_08AAAB6C(self, [0x08CDF804]). Otherwise o = ref[0]; r = o->vtable[0xEC](o); s3 = func_08AA9AC8(self, r); a null ref[0] reported (func_08A5BB7C(0x08CB30B8)) after that, r2 = ref[0]->vtable[0xEC](ref[0]); func_08AAAB6C(self, r2); and a tail jump through s3->vtable[8] with (s3, self, ref).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (first == 0u) {`; `if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});`.
void func_08AAACF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t ref = ctx.gpr[5];
    const std::uint32_t first = word(rt, ref);
    if (first == 0u) {
        ctx.gpr[2] = 0u;  // lui v0, 0 in the branch's target
        tail_call(ctx, 0x08AAAB6Cu, {self, word(rt, 0x08CDF804u)});
        return;
    }
    const std::uint32_t r = call_virtual(rt, ctx, first, 0xECu, {first});
    const std::uint32_t s3 = call_set(rt, ctx, 0x08AA9AC8u, {self, r});
    const std::uint32_t second = word(rt, ref);
    if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    const std::uint32_t r2 = call_virtual(rt, ctx, second, 0xECu, {second});
    (void)call_set(rt, ctx, 0x08AAAB6Cu, {self, r2});
    const std::uint32_t target = slot_of(rt, s3, 8u);
    ctx.gpr[2] = word(rt, s3);  // v0 = the vtable of s3, t9 = the slot's function
    ctx.gpr[25] = target;
    tail_call(ctx, target, {s3, self, ref});
}

// func_08B7C6BC(a = a0, b = a1, count = a2): strncmp. The first position where the bytes differ gives the difference of the unsigned bytes (a - b); two equal zero bytes, or `count` equal bytes, give 0. (The original compares a word at a time when both pointers are aligned and falls back to this byte loop at the first word that differs;
// a word that holds a zero byte and is equal in both returns 0 as well.) Reads stay inside `count`.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (x == 0u) break;`.
void func_08B7C6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    std::uint32_t result = 0u;
    for (std::uint32_t i = 0u; i != count; ++i) {
        const std::uint32_t x = byte_u(rt, a + i);
        const std::uint32_t y = byte_u(rt, b + i);
        if (x != y) {
            result = x - y;
            break;
        }
        if (x == 0u) break;
    }
    ctx.gpr[2] = result;
}

// func_08B7BB6C(dst = a0, src = a1): strcpy; the bytes up to and including the first zero are copied (the original copies whole words while none holds a zero byte, then bytes) and v0 = dst.
void func_08B7BB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    std::uint32_t i = 0u;
    for (;;) {
        const std::uint32_t c = byte_u(rt, src + i);
        put8(rt, dst + i, c);
        ++i;
        if (c == 0u) break;
    }
    ctx.gpr[2] = dst;
}

// func_08B23928(buffer = a0, path = a1): appends the components of `path` to the buffer object {+0 data, +4 end, +8 position} (remaining = [4] - [8]): a null data pointer or no room answers v0 = buffer. Slashes in `path` are collapsed: func_08B23778 skips them and func_08B237A0 finds the end of a component. s4 starts as "path begins with '/'";
// each component of length v1 is preceded by a '/' when s4, then at most s3 (the remaining room - 1) bytes are copied with func_08B7C830 (strncpy). The loop ends when the path is used up or no room is left; the data gets its zero byte and the function ends in the tail jump func_08B1CB80(buffer).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (s0 == 0u || capacity == 0u) {`; `std::uint32_t s4 = (sext8(byte_u(rt, path)) ^ 0x2Fu) == 0u ? 1u : 0u;`; `if (sext8(byte_u(rt, s2)) == 0u) {`; `if (signed_of(s3) > 0) {`; `if (length == 0u) break;`; `if (fits) {`; `if (!(signed_of(s3) > 0)) break;`.
void func_08B23928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    const std::uint32_t path = ctx.gpr[5];
    const std::uint32_t capacity = word(rt, buffer + 4u) - word(rt, buffer + 8u);
    std::uint32_t s0 = word(rt, buffer);
    if (s0 == 0u || capacity == 0u) {
        ctx.gpr[2] = buffer;
        return;
    }
    std::uint32_t s4 = (sext8(byte_u(rt, path)) ^ 0x2Fu) == 0u ? 1u : 0u;
    std::uint32_t s3 = capacity - 1u;
    std::uint32_t s2 = call_set(rt, ctx, 0x08B23778u, {path});
    if (sext8(byte_u(rt, s2)) == 0u) {
        put8(rt, s0, 0u);
        tail_call(ctx, 0x08B1CB80u, {buffer});
        return;
    }
    if (signed_of(s3) > 0) {
        for (;;) {
            const std::uint32_t s5 = call_set(rt, ctx, 0x08B237A0u, {s2});
            const std::uint32_t length = s5 - s2;
            if (length == 0u) break;
            bool fits = signed_of(length) < signed_of(s3);
            if (s4 != 0u) {
                put8(rt, s0, 0x2Fu);
                --s3;
                ++s0;
                fits = signed_of(length) < signed_of(s3);
            }
            std::uint32_t count;
            if (fits) {
                count = length;
                s3 -= length;
            } else {
                count = s3;
                s3 = 0u;
            }
            (void)call_set(rt, ctx, 0x08B7C830u, {s0, s2, count, count});
            s0 += count;
            s2 = call_set(rt, ctx, 0x08B23778u, {s5});
            s4 = 1u;
            if (sext8(byte_u(rt, s2)) == 0u) break;
            if (!(signed_of(s3) > 0)) break;
        }
    }
    put8(rt, s0, 0u);
    tail_call(ctx, 0x08B1CB80u, {buffer});
}

// func_08B6D564(f12 = x, f13 = tail; f0 result): a cosine-style polynomial in x * x from the constants at 0x08CB64C0..0x08CB64E4 (0x08CB64C0 is the value returned for a tiny x). a = |x| as integer bits. When a <= 0x31FFFFFF and trunc(x) == 0 the answer is the float at 0x08CB64C0. Otherwise p = ((((C4 * x^2 + C8) * x^2 - CC) * x^2 + D0) * x^2 - D4) * x^2 + D8, r = x^2 * p (the original multiplies in this order):
// a <= 0x3E999999: C0 - (DC * x^2 - (x^2 * r - x * tail)); a above that: the bits a + 0xFF000000 are read as a float h (an exponent step down by two); a > 0x3F480000: E4 - ((DC * x^2 - E0) - (x^2 * r - x * tail)); otherwise (C0 - h) - ((DC * x^2 - h) - (x^2 * r - x * tail)). Products use the PSP multiply.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (!above_tiny && ctx.fpu_float_to_word_ct<1u>(x) == 0u) {`; `if (above_high) {`.
void func_08B6D564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float tail = ctx.fpr[13];
    std::uint32_t a0 = std::bit_cast<std::uint32_t>(x) & 0x7FFFFFFFu;
    const bool above_tiny = signed_of(a0) > 0x31FFFFFF;
    const bool above_mid = signed_of(a0) > 0x3E999999;
    const bool above_high = signed_of(a0) > 0x3F480000;
    if (!above_tiny && ctx.fpu_float_to_word_ct<1u>(x) == 0u) {
        ctx.fpr[0] = float_at(rt, 0x08CB64C0u);
        return;
    }
    const float f3 = psp_mul(x, x);
    float f0 = psp_mul(f3, float_at(rt, 0x08CB64C4u));
    f0 = f0 + float_at(rt, 0x08CB64C8u);
    f0 = psp_mul(f3, f0);
    f0 = f0 - float_at(rt, 0x08CB64CCu);
    f0 = psp_mul(f3, f0);
    f0 = f0 + float_at(rt, 0x08CB64D0u);
    f0 = psp_mul(f3, f0);
    f0 = f0 - float_at(rt, 0x08CB64D4u);
    f0 = psp_mul(f3, f0);
    f0 = f0 + float_at(rt, 0x08CB64D8u);
    float f1 = psp_mul(f3, f0);
    if (!above_mid) {
        f0 = psp_mul(x, tail);
        f1 = psp_mul(f3, f1);
        f1 = f1 - f0;
        f0 = float_at(rt, 0x08CB64DCu);
        f0 = psp_mul(f3, f0);
        f0 = f0 - f1;
        ctx.fpr[0] = float_at(rt, 0x08CB64C0u) - f0;
        return;
    }
    a0 += 0xFF000000u;
    float f4 = std::bit_cast<float>(a0);
    if (above_high) {
        f4 = float_at(rt, 0x08CB64E0u);
        f0 = float_at(rt, 0x08CB64DCu);
        f1 = psp_mul(f3, f1);
        const float f2 = psp_mul(x, tail);
        f0 = psp_mul(f3, f0);
        const float f5 = float_at(rt, 0x08CB64E4u);
        f1 = f1 - f2;
        f0 = f0 - f4;
        f0 = f0 - f1;
        ctx.fpr[0] = f5 - f0;
        return;
    }
    f0 = float_at(rt, 0x08CB64C0u);
    f1 = psp_mul(f3, f1);
    const float f5 = f0 - f4;
    f0 = float_at(rt, 0x08CB64DCu);
    const float f2 = psp_mul(x, tail);
    f0 = psp_mul(f3, f0);
    f1 = f1 - f2;
    f0 = f0 - f4;
    f0 = f0 - f1;
    ctx.fpr[0] = f5 - f0;
}

// func_089A932C(self = a0, p = a1, q = a2): builds two arrays of 2-float points on the stack and hands them to self->vtable[0x10C](self, [0x08C8CC14], count, array1, array2, 0). With w = p[8] - p[0], n = trunc(w * 0.015625f) and n - 1 < 7 (unsigned; otherwise the call is self->vtable[0x10C](self, [0x08C8CC00], 2, p, q, 0) with a3 = the original a1),
// m = n + 1 and the steps dx = w / m, dy = (q[8] - q[0]) / m, the loop writes m + 1 rows: array1 (sp + 16 i) { x, p[0xC] } { x, p[4] } and array2 (sp + 0x90 + 16 i) { y, q[0xC] } { y, q[4] } with x = p[0] + i * dx and y = q[0] + i * dy (accumulated by repeated addition), count = 2 * (m + 1). v0 is the call's.
// (The original spins in two 18-iteration delay loops first; nothing depends on them.)
void func_089A932C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t p = ctx.gpr[5];
    const std::uint32_t q = ctx.gpr[6];
    GuestFrame frame(ctx, 0x130u);
    float f3 = float_at(rt, p);
    const float width = float_at(rt, p + 8u) - f3;
    const float scaled = psp_mul(width, std::bit_cast<float>(0x3C800000u));
    const std::uint32_t n = ctx.fpu_float_to_word_ct<1u>(scaled);
    const std::uint32_t m = n + 1u;
    if (!((n - 1u) < 7u)) {
        ctx.gpr[2] = word(rt, self);  // v0 = the vtable, the slot's function sits in t2
        (void)call_set(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, word(rt, 0x08C8CC00u), 2u, p, q, 0u});
        return;
    }
    const float divisor = static_cast<float>(signed_of(m));
    float f2 = float_at(rt, q);
    float f0 = float_at(rt, q + 8u) - f2;
    const float f7 = float_at(rt, p + 4u);
    const float f6 = float_at(rt, p + 0xCu);
    const float f5 = width / divisor;
    f0 = f0 / divisor;
    const float f4 = float_at(rt, q + 0xCu);
    const float f1 = float_at(rt, q + 4u);
    std::uint32_t pairs = 0u;
    std::uint32_t t1 = 8u;
    std::uint32_t t2 = 0u;
    const std::uint32_t second = frame.at(0x90u);
    for (;;) {
        const std::uint32_t row = pairs << 3;
        ++t2;
        put_float(rt, frame.at(row), f3);
        pairs += 2u;
        put_float(rt, frame.at(row + 4u), f6);
        put_float(rt, second + row, f2);
        put_float(rt, second + row + 4u, f4);
        put_float(rt, frame.at(t1), f3);
        f3 = f3 + f5;
        put_float(rt, frame.at(t1 + 4u), f7);
        put_float(rt, second + t1, f2);
        f2 = f2 + f0;
        t1 += 0x10u;
        put_float(rt, second + t1 - 0x10u + 4u, f1);
        if (signed_of(m) < signed_of(t2)) break;
    }
    ctx.gpr[2] = word(rt, self);
    (void)call_set(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, word(rt, 0x08C8CC14u), pairs, frame.at(0u), second, 0u});
}

// func_089DA5C0(self = a0, out = a1): answers 1 and stores a fraction through `out`, or 0 (nothing stored). Not applicable (v0 = 0) when self[0xA4] >= self->vtable[0x1C8](self) (signed), when self[0x90] + 1/120 (0x3C088889) is below func_089D83F4(self)'s float, or when self[0x9C] - func_089D8410(self) is below self[0x90] - 1/120.
// Otherwise s0 = self[0xA4], f20 = self[0x94], f22 = self[0x98], f23 = func_089DA558(self, s0) and the offset f20 = (self[0x90] - f20) - func_089DA468(self, s0) when func_089DA370(self, s0) is 0, else (f22 - self[0x90]) - func_089DA4E0(self, s0). *out = 0.0f when f20 < 0 or f23 <= 0; 1.0f when f23 < f20; else
// (the byte self[0x8C] != 0 first turns s0 into self->vtable[0x1C8](self) - s0 - 1) r = self->vtable[0x1D0](self, s0) (the slot is read before the 0x1C8 call) and *out = func_089D7294(r; f12 = f20 / f23)'s float result.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (!(signed_of(s0) < signed_of(limit))) {`; `if (f20 < ctx.fpr[0]) {`; `if (f1 < f2) {`; `if (f23 < f20) {`; `if (!(0.0f < f23)) {`.
void func_089DA5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    std::uint32_t s0 = word(rt, self + 0xA4u);
    const std::uint32_t limit = call_virtual(rt, ctx, self, 0x1C8u, {self});
    if (!(signed_of(s0) < signed_of(limit))) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float f21 = float_at(rt, self + 0x90u);
    const float step = std::bit_cast<float>(0x3C088889u);
    float f20 = f21 + step;
    (void)call_keep(rt, ctx, 0x089D83F4u, {self});
    if (f20 < ctx.fpr[0]) {
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_keep(rt, ctx, 0x089D8410u, {self});
    {
        const float f1 = float_at(rt, self + 0x9Cu) - ctx.fpr[0];
        const float f2 = f21 - step;
        if (f1 < f2) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    s0 = word(rt, self + 0xA4u);
    f20 = float_at(rt, self + 0x94u);
    const float f22 = float_at(rt, self + 0x98u);
    (void)call_keep(rt, ctx, 0x089DA558u, {self, s0});
    const float f23 = ctx.fpr[0];
    const std::uint32_t flag = call_set(rt, ctx, 0x089DA370u, {self, s0});
    if (flag == 0u) {
        (void)call_keep(rt, ctx, 0x089DA468u, {self, s0});
        f20 = (f21 - f20) - ctx.fpr[0];
    } else {
        (void)call_keep(rt, ctx, 0x089DA4E0u, {self, s0});
        f20 = (f22 - f21) - ctx.fpr[0];
    }
    ctx.gpr[2] = 1u;
    if (f20 < 0.0f) {
        put_float(rt, out, 0.0f);
        return;
    }
    if (f23 < f20) {
        put_float(rt, out, 1.0f);
        return;
    }
    if (!(0.0f < f23)) {
        put_float(rt, out, 0.0f);
        return;
    }
    const std::uint32_t slot_1d0 = word(rt, word(rt, self) + 0x1D0u);
    if (byte_u(rt, self + 0x8Cu) != 0u) {
        const std::uint32_t count = call_virtual(rt, ctx, self, 0x1C8u, {self});
        s0 = (count - s0) - 1u;
    }
    const std::uint32_t r = call_set(rt, ctx, slot_1d0, {self, s0});
    (void)call_setf(rt, ctx, 0x089D7294u, {r}, {f20 / f23});
    ctx.gpr[2] = 1u;
    put_float(rt, out, ctx.fpr[0]);
}

// The polynomial p(z) = ((((((z * C0 + C1) * z + C2) * z + C3) * z + C4) * z + C5) * z + C6) * z + 1 of func_08B49D98 (C0..C6 = 0x3D0F2DE7, 0x3BEC93BC, 0x3CCA7312, 0x3CF68352, 0x3D36EF92, 0x3D999946, 0x3E2AAAAB), products with the PSP multiply.
float arcsine_poly(float z) {
    float p = psp_mul(z, std::bit_cast<float>(0x3D0F2DE7u));
    p = p + std::bit_cast<float>(0x3BEC93BCu);
    p = psp_mul(z, p);
    p = p + std::bit_cast<float>(0x3CCA7312u);
    p = psp_mul(z, p);
    p = p + std::bit_cast<float>(0x3CF68352u);
    p = psp_mul(z, p);
    p = p + std::bit_cast<float>(0x3D36EF92u);
    p = psp_mul(z, p);
    p = p + std::bit_cast<float>(0x3D999946u);
    p = psp_mul(z, p);
    p = p + std::bit_cast<float>(0x3E2AAAABu);
    p = psp_mul(z, p);
    return p + 1.0f;
}
// func_08B49D98(f12 = x; f0 result): arcsine. |x| <= 0.5: x * p(x * x) (returned at once, no sign step). Otherwise h = (1 - |x|) * 0.5, s = sqrt(h), r = (s * p(s * s)) * -2 + 0x3F490FDB + 0x3F490FDB (pi/4 twice); for |x| > 1 r is 0x3FC90FDB (pi/2) instead; r is negated when x < 0.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (std::fabs(x) <= one) {`.
void func_08B49D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const float x = ctx.fpr[12];
    const float half = 0.5f;
    const float one = 1.0f;
    float f2 = std::bit_cast<float>(0x3FC90FDBu);
    if (std::fabs(x) <= half) {
        const float z = psp_mul(x, x);
        ctx.fpr[0] = psp_mul(x, arcsine_poly(z));
        return;
    }
    const float quarter_turn = std::bit_cast<float>(0x3F490FDBu);
    const float h = psp_mul(one - std::fabs(x), half);
    const float s = std::sqrt(h);
    if (std::fabs(x) <= one) {
        const float z = psp_mul(s, s);
        f2 = psp_mul(s, arcsine_poly(z));
        f2 = psp_mul(f2, std::bit_cast<float>(0xC0000000u));
        f2 = f2 + quarter_turn;
        f2 = f2 + quarter_turn;
    }
    if (x < 0.0f) f2 = -f2;
    ctx.fpr[0] = f2;
}

// func_08C468E4(out = a0, m = a1, in = a2): out = in * m for a 3x3 float matrix (rows at m + 0 / 0xC / 0x18): out.x = ((in.x * m[0] + 0) + in.y * m[3]) + in.z * m[6], out.y likewise with m[1] / m[4] / m[7], out.z with m[2] / m[5] / m[8] (the "+ 0.0f" is real: it turns -0 into +0).
// out == in: all three inputs are read first and the results stored x, z (via the stack), y at the end; otherwise each component is stored before the next one's inputs are read, as the original does.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (out == in) {`.
void func_08C468E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t m = ctx.gpr[5];
    const std::uint32_t in = ctx.gpr[6];
    const float zero = 0.0f;
    if (out == in) {
        const float x = float_at(rt, out);
        const float m2 = float_at(rt, m + 8u);
        const float m0 = float_at(rt, m);
        const float m1 = float_at(rt, m + 4u);
        const float y = float_at(rt, out + 4u);
        float f1 = psp_mul(x, m0);
        float f2 = psp_mul(x, m1);
        float f4 = psp_mul(x, m2);
        const float m3 = float_at(rt, m + 0xCu);
        const float m4 = float_at(rt, m + 0x10u);
        const float m5 = float_at(rt, m + 0x14u);
        f4 = f4 + zero;
        f1 = f1 + zero;
        const float f6 = psp_mul(y, m5);
        const float f8 = psp_mul(y, m3);
        f2 = f2 + zero;
        const float f5 = psp_mul(y, m4);
        const float m8 = float_at(rt, m + 0x20u);
        const float z = float_at(rt, out + 8u);
        const float m6 = float_at(rt, m + 0x18u);
        const float m7 = float_at(rt, m + 0x1Cu);
        f4 = f4 + f6;
        const float f7a = psp_mul(z, m8);
        const float f3 = psp_mul(z, m6);
        f1 = f1 + f8;
        f2 = f2 + f5;
        const float f0 = psp_mul(z, m7);
        const float f7 = f4 + f7a;
        f1 = f1 + f3;
        f2 = f2 + f0;
        put_float(rt, out, f1);
        put_float(rt, out + 8u, f7);
        put_float(rt, out + 4u, f2);
        return;
    }
    {
        const float m0 = float_at(rt, m);
        float f1 = float_at(rt, in);
        const float m3 = float_at(rt, m + 0xCu);
        float f2 = float_at(rt, in + 4u);
        f1 = psp_mul(f1, m0);
        float f0 = float_at(rt, in + 8u);
        f2 = psp_mul(f2, m3);
        const float m6 = float_at(rt, m + 0x18u);
        f1 = f1 + zero;
        f0 = psp_mul(f0, m6);
        f1 = f1 + f2;
        f1 = f1 + f0;
        put_float(rt, out, f1);
    }
    {
        const float m1 = float_at(rt, m + 4u);
        float f0 = float_at(rt, in);
        const float m4 = float_at(rt, m + 0x10u);
        float f2 = float_at(rt, in + 4u);
        f0 = psp_mul(f0, m1);
        float f1 = float_at(rt, in + 8u);
        f2 = psp_mul(f2, m4);
        const float m7 = float_at(rt, m + 0x1Cu);
        f0 = f0 + zero;
        f1 = psp_mul(f1, m7);
        f0 = f0 + f2;
        f0 = f0 + f1;
        put_float(rt, out + 4u, f0);
    }
    {
        const float m2 = float_at(rt, m + 8u);
        float f1 = float_at(rt, in);
        float f2 = float_at(rt, in + 4u);
        const float m5 = float_at(rt, m + 0x14u);
        f1 = psp_mul(f1, m2);
        float f0 = float_at(rt, in + 8u);
        f2 = psp_mul(f2, m5);
        const float m8 = float_at(rt, m + 0x20u);
        f1 = f1 + zero;
        f0 = psp_mul(f0, m8);
        f1 = f1 + f2;
        f0 = f1 + f0;
        put_float(rt, out + 8u, f0);
    }
}

// func_08BEF574(this = a0, other = a1): assignment of a vector of 4-byte elements (begin / end / capacity-end words at +0 / +4 / +8); v0 = this. n = other's element count s5. this == other: nothing. Capacity < n: a new block func_08A5BC20(n * 4, 4), func_08BEF08C(other.begin, other.end, block, sp) copies, func_08BEEA84(begin, end, sp+3) destroys the old range,
// the old block is released with func_08A5BC54(begin, capacity bytes, 4) when it exists, begin = block, capacity-end = block + n * 4, end = begin + n * 4. Size >= n: func_08B79BAC(begin, other.begin, n * 4) (a memmove), func_08BEF140(begin + n * 4, end, sp+2) destroys the tail, end = begin + n * 4.
// Size < n: func_08B79BAC(begin, other.begin, size * 4), then func_08BEF08C(other.begin + ((end - begin) & ~3), other.end, end, sp+1) copies the rest; end = begin + n * 4.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (other == self) {`; `if (old != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old, bytes, 4u});`; `if (!(size < s5)) {`.
void func_08BEF574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t s2 = word(rt, self);
    const std::uint32_t s3 = word(rt, other);
    const std::uint32_t s6 = word(rt, other + 4u);
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - s2) >> 2);
    const std::uint32_t s4 = s6 - s3;
    const std::uint32_t s5 = static_cast<std::uint32_t>(signed_of(s4) >> 2);
    const std::uint32_t s7 = s5 << 2;
    std::uint32_t a0;
    if (capacity < s5) {
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {s7, 4u});
        (void)call_set(rt, ctx, 0x08BEF08Cu, {s3, s6, block, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08BEEA84u, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const std::uint32_t old = word(rt, self);
        const std::uint32_t bytes = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old) >> 2) << 2;
        if (old != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old, bytes, 4u});
        put(rt, self + 8u, block + s7);
        put(rt, self, block);
        a0 = s7;
    } else {
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, self + 4u) - s2) >> 2);
        if (!(size < s5)) {
            (void)call_set(rt, ctx, 0x08B79BACu, {s2, s3, s4});
            (void)call_set(rt, ctx, 0x08BEF140u, {s2 + s4, word(rt, self + 4u), frame.at(2u)});
            a0 = s5 << 2;
        } else {
            (void)call_set(rt, ctx, 0x08B79BACu, {s2, s3, size << 2});
            const std::uint32_t a2 = word(rt, self + 4u);
            const std::uint32_t used = (a2 - word(rt, self)) & ~3u;
            (void)call_set(rt, ctx, 0x08BEF08Cu, {word(rt, other) + used, word(rt, other + 4u), a2, frame.at(1u)});
            a0 = s5 << 2;
        }
    }
    put(rt, self + 4u, a0 + word(rt, self));
    ctx.gpr[2] = self;
}

// func_08B509EC(m = a0; f12..f14 = eye, f15..f17 = target, f18 / f19 = the first two components of an up vector, the third is the float at the caller's sp + 0): a look-at matrix. d = target - eye, f = d / |d| (m[8] = -f.x, m[0x18] = -f.y, m[0x28] = -f.z);
// the side vector s = (up.z * f.y - up.y * f.z, up.x * f.z - up.z * f.x, up.y * f.x - up.x * f.y) (the original's order of products), normalised (m[0] = s.x, m[0x10] = s.y, m[0x20] = s.z), and the third axis f x s (m[4], m[0x14], m[0x24]); m[0xC], m[0x1C], m[0x2C], m[0x30..0x38] = 0 and m[0x3C] = 1.
// A temporary matrix at sp (identity with the translation -eye at 0x30..0x38) is handed to func_08B4E9D4(m, sp) at the end (v0 is its).
void func_08B509EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t m = ctx.gpr[4];
    const float eye_x = ctx.fpr[12];
    const float eye_y = ctx.fpr[13];
    const float eye_z = ctx.fpr[14];
    float f15 = ctx.fpr[15];
    float f16 = ctx.fpr[16];
    float f17 = ctx.fpr[17];
    float f18 = ctx.fpr[18];
    float f19 = ctx.fpr[19];
    float f4 = float_at(rt, ctx.gpr[29]);
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t t = frame.at(0u);
    const float one = 1.0f;
    f15 = f15 - eye_x;
    f16 = f16 - eye_y;
    f17 = f17 - eye_z;
    float f0 = psp_mul(f15, f15);
    float f2 = psp_mul(f16, f16);
    float f3 = psp_mul(f17, f17);
    f0 = f0 + f2;
    const float neg_x = -eye_x;
    const float neg_y = -eye_y;
    f0 = f0 + f3;
    put_float(rt, m + 0x3Cu, one);
    const float neg_z = -eye_z;
    f0 = std::sqrt(f0);
    put(rt, m + 0x30u, 0u);
    put(rt, m + 0x34u, 0u);
    put(rt, m + 0x38u, 0u);
    put(rt, m + 0xCu, 0u);
    put(rt, m + 0x1Cu, 0u);
    put(rt, m + 0x2Cu, 0u);
    put_float(rt, t + 0x3Cu, one);
    put_float(rt, t, one);
    put_float(rt, t + 0x14u, one);
    f0 = one / f0;
    put_float(rt, t + 0x28u, one);
    put_float(rt, t + 0x30u, neg_x);
    put_float(rt, t + 0x34u, neg_y);
    put_float(rt, t + 0x38u, neg_z);
    put(rt, t + 0x10u, 0u);
    put(rt, t + 0x20u, 0u);
    put(rt, t + 4u, 0u);
    put(rt, t + 0x24u, 0u);
    put(rt, t + 8u, 0u);
    f15 = psp_mul(f15, f0);
    f16 = psp_mul(f16, f0);
    f17 = psp_mul(f17, f0);
    put(rt, t + 0x18u, 0u);
    f3 = psp_mul(f4, f15);
    f4 = psp_mul(f4, f16);
    f0 = psp_mul(f19, f17);
    f2 = psp_mul(f18, f17);
    f19 = psp_mul(f19, f15);
    f18 = psp_mul(f18, f16);
    f4 = f4 - f0;
    f2 = f2 - f3;
    const float f8 = -f15;
    f19 = f19 - f18;
    f0 = psp_mul(f4, f4);
    f3 = psp_mul(f2, f2);
    const float f6 = -f16;
    const float f5 = psp_mul(f19, f19);
    const float f7 = -f17;
    f0 = f0 + f3;
    put_float(rt, m + 8u, f8);
    f0 = f0 + f5;
    put_float(rt, m + 0x18u, f6);
    f0 = std::sqrt(f0);
    put_float(rt, m + 0x28u, f7);
    put(rt, t + 0xCu, 0u);
    put(rt, t + 0x1Cu, 0u);
    put(rt, t + 0x2Cu, 0u);
    float f1 = one / f0;
    f19 = psp_mul(f19, f1);
    f2 = psp_mul(f2, f1);
    f4 = psp_mul(f4, f1);
    f3 = psp_mul(f15, f19);
    f0 = psp_mul(f17, f2);
    f1 = psp_mul(f16, f4);
    f15 = psp_mul(f15, f2);
    f16 = psp_mul(f16, f19);
    f17 = psp_mul(f17, f4);
    put_float(rt, m, f4);
    f1 = f1 - f15;
    f0 = f0 - f16;
    f3 = f3 - f17;
    put_float(rt, m + 0x10u, f2);
    put_float(rt, m + 0x24u, f1);
    put_float(rt, m + 4u, f0);
    put_float(rt, m + 0x14u, f3);
    put_float(rt, m + 0x20u, f19);
    (void)call_set(rt, ctx, 0x08B4E9D4u, {m, t});
}

// func_08B52B70(object = a0, size = a1, data = a2, kind = t0, flags = t3, ninth = the word at the caller's sp + 0): uploads a texture level for the graphics library object a0 (its library state sits at a0 + 0x20 = s2).
// s0 = (size + 0x3F) & ~0x3F; func_08B52870(object, size, data, kind, flags, ninth) answering 0 gives v0 = 0 and nothing else. Otherwise func_08B54808(s2), then func_08B5236C(s2, object[0], kind != 0xB ? kind : 0, 0, size, data, 0, s4 * s0) with s4 = 4 when kind == 3, else 2, and func_08B52560(s2, n, 1) for n = 3, 4, 5, 6; v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (ok == 0u) {`.
void func_08B52B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t size = ctx.gpr[5];
    const std::uint32_t data = ctx.gpr[6];
    const std::uint32_t kind = ctx.gpr[8];
    const std::uint32_t flags = ctx.gpr[11];
    const std::uint32_t ninth = word(rt, ctx.gpr[29]);
    const std::uint32_t s2 = object + 0x20u;
    const std::uint32_t s0 = (size + 0x3Fu) & ~0x3Fu;
    const std::uint32_t ok = call_set(rt, ctx, 0x08B52870u, {object, size, data, kind, flags, ninth});
    const std::uint32_t s4 = (kind ^ 3u) != 0u ? 2u : 4u;
    if (ok == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    (void)call_set(rt, ctx, 0x08B54808u, {s2});
    const std::uint32_t product = s4 * s0;
    const std::uint32_t a2 = (kind ^ 0xBu) != 0u ? kind : 0u;
    (void)call_set(rt, ctx, 0x08B5236Cu, {s2, word(rt, object), a2, 0u, size, data, 0u, product});
    (void)call_set(rt, ctx, 0x08B52560u, {s2, 3u, 1u});
    (void)call_set(rt, ctx, 0x08B52560u, {s2, 4u, 1u});
    (void)call_set(rt, ctx, 0x08B52560u, {s2, 5u, 1u});
    (void)call_set(rt, ctx, 0x08B52560u, {s2, 6u, 1u});
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// func_08B23FA0(path = a0, prefix = a1): v0 = 1 when the components of `prefix` are the leading components of `path`, else 0. Both must be non-null and path must start with '/'. func_08B35F60 gives the lengths: a path of length 1 matches at once, a prefix shorter than the path does not.
// Then func_08B23778 skips slashes and func_08B237A0 finds the end of a component; each pair of components must have the same length and an equal func_08B7C6BC (strncmp). A path with nothing left after skipping the slashes answers 1.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (s0 == 0u || s1 == 0u || sext8(byte_u(rt, s0)) != 0x2Fu) {`; `if (prefix_length < s2) {`; `if (sext8(byte_u(rt, s0)) == 0u) {`; `if (length0 != length1) {`; `if (call_set(rt, ctx, 0x08B7C6BCu, {s0, s1, length0}) != 0u) {`; `} while (sext8(byte_u(rt, s0)) != 0u);`.
void func_08B23FA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t s0 = ctx.gpr[4];
    std::uint32_t s1 = ctx.gpr[5];
    if (s0 == 0u || s1 == 0u || sext8(byte_u(rt, s0)) != 0x2Fu) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t s2 = call_set(rt, ctx, 0x08B35F60u, {s0});
    const std::uint32_t prefix_length = call_set(rt, ctx, 0x08B35F60u, {s1});
    if (s2 == 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (prefix_length < s2) {
        ctx.gpr[2] = 0u;
        return;
    }
    s0 = call_set(rt, ctx, 0x08B23778u, {s0});
    s1 = call_set(rt, ctx, 0x08B23778u, {s1});
    if (sext8(byte_u(rt, s0)) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    do {
        const std::uint32_t end0 = call_set(rt, ctx, 0x08B237A0u, {s0});
        const std::uint32_t end1 = call_set(rt, ctx, 0x08B237A0u, {s1});
        const std::uint32_t length0 = end0 - s0;
        const std::uint32_t length1 = end1 - s1;
        if (length0 != length1) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (call_set(rt, ctx, 0x08B7C6BCu, {s0, s1, length0}) != 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        s0 = call_set(rt, ctx, 0x08B23778u, {end0});
        s1 = call_set(rt, ctx, 0x08B23778u, {end1});
    } while (sext8(byte_u(rt, s0)) != 0u);
    ctx.gpr[2] = 1u;
}

// func_08AE69F4(flag = a0, value = a1): takes the next free 0x98-byte slot of the pool at 0x08D4098C (the slot is pool[4] when its word +8 is not negative; none: v0 = -1, reported through func_08AD83B8([0x08CDFB44], 0x08C91908, 0x260, 0x08C91980) when bit 1 of [0x08CDFB3C] is set), initialises two embedded list heads at slot + 0x18 / 0x28
// (next/prev self, word +8 = -1, +0xC = 0), slot[0x10] = value, slot[0xC] = 1, slot[0x14] = flag; flag == 1 copies the 0x60 bytes at 0x08D409D4 to slot + 0x38. The slot is unlinked from its current list when it is linked (slot[0] != slot) and put at the head of the list whose sentinel is at 0x08D4097C; v0 = slot[8].
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `const std::uint32_t slot = signed_of(word(rt, candidate + 8u)) < 0 ? 0u : candidate;`; `if (slot == 0u) {`; `if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u) {`; `if (flag == 1u) {`; `} while (from != kPool + 0xA8u);`; `if (previous != slot) {`.
void func_08AE69F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flag = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    constexpr std::uint32_t kPool = 0x08D4098Cu;
    constexpr std::uint32_t kSentinel = 0x08D4097Cu;
    const std::uint32_t candidate = word(rt, kPool + 4u);
    const std::uint32_t slot = signed_of(word(rt, candidate + 8u)) < 0 ? 0u : candidate;
    if (slot == 0u) {
        if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u) {
            (void)call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB44u), 0x08C91908u, 0x260u, 0x08C91980u});
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t head_a = slot + 0x18u;
    const std::uint32_t head_b = slot + 0x28u;
    put(rt, head_a + 0xCu, 0u);
    put(rt, head_a + 4u, head_a);
    put(rt, head_b + 0xCu, 0u);
    put(rt, head_a + 8u, 0xFFFFFFFFu);
    put(rt, head_b + 4u, head_b);
    put(rt, head_b + 8u, 0xFFFFFFFFu);
    put(rt, slot + 0x18u, head_a);
    put(rt, slot + 0x10u, value);
    put(rt, slot + 0x28u, head_b);
    put(rt, slot + 0xCu, 1u);
    put(rt, slot + 0x14u, flag);
    if (flag == 1u) {
        std::uint32_t from = kPool + 0x48u;
        std::uint32_t to = slot + 0x38u;
        do {
            const std::uint32_t w0 = word(rt, from);
            const std::uint32_t w1 = word(rt, from + 4u);
            const std::uint32_t w2 = word(rt, from + 8u);
            const std::uint32_t w3 = word(rt, from + 0xCu);
            put(rt, to, w0);
            from += 0x10u;
            to += 0x10u;
            put(rt, to - 0xCu, w1);
            put(rt, to - 8u, w2);
            put(rt, to - 4u, w3);
        } while (from != kPool + 0xA8u);
    }
    const std::uint32_t previous = word(rt, slot);
    if (previous != slot) {
        const std::uint32_t next = word(rt, slot + 4u);
        put(rt, previous + 4u, next);
        const std::uint32_t again = word(rt, slot + 4u);
        put(rt, slot + 4u, slot);
        put(rt, again, previous);
        put(rt, slot, slot);
    }
    const std::uint32_t first = word(rt, kSentinel);
    put(rt, slot + 4u, kSentinel);
    put(rt, first + 4u, slot);
    const std::uint32_t second = word(rt, slot + 4u);
    put(rt, slot, first);
    put(rt, second, slot);
    ctx.gpr[2] = word(rt, slot + 8u);
}

// func_0896A254(node = a0, recurse = a1 & 0xFF): when node[0xE0] & 0x1000 is set and func_08966174(node) answers 0, func_0896A09C(node, 0) runs, otherwise func_0896A178(node, 0). A non-zero `recurse` then asks node->vtable[0x19C](node) and, when it answers non-zero, walks the chain node[0xF4], next = func_08968A38(child):
// each child gets func_0896A254(child, 1). v0 is the last call's.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (recurse == 0u) return;`; `if (call_virtual(rt, ctx, node, 0x19Cu, {node}) == 0u) return;`; `if (child == 0u) return;`; `} while (child != 0u);`.
void func_0896A254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    const std::uint32_t recurse = ctx.gpr[5] & 0xFFu;
    bool use_09c = false;
    if ((word(rt, node + 0xE0u) & 0x1000u) != 0u && call_set(rt, ctx, 0x08966174u, {node}) == 0u) use_09c = true;
    if (use_09c) {
        (void)call_set(rt, ctx, 0x0896A09Cu, {node, 0u});
    } else {
        (void)call_set(rt, ctx, 0x0896A178u, {node, 0u});
    }
    if (recurse == 0u) return;
    if (call_virtual(rt, ctx, node, 0x19Cu, {node}) == 0u) return;
    std::uint32_t child = word(rt, node + 0xF4u);
    if (child == 0u) return;
    do {
        (void)call_set(rt, ctx, 0x0896A254u, {child, 1u});
        child = call_set(rt, ctx, 0x08968A38u, {child});
    } while (child != 0u);
}

// Shared by func_08C12E58 / func_08C0E654 (assignment of a vector whose elements are 16 / 32 bytes: begin / end / capacity-end words at +0 / +4 / +8; v0 = this): the same plan as func_08BEF574. Capacity < n: a new block func_08B35840(0x10, n bytes), `uninit_copy`(other.begin, other.end, block, sp) copies, `destroy_range`(begin, end, sp+3) destroys the old range,
// the old block is released with func_08B35254(begin) when it exists, begin = block, capacity-end = block + n bytes. Size >= n: each of the n elements is assigned (`element_copy`: for the 32-byte kind a pair is copied with func_089641C4(dst, src) and the word at +0x10, nothing when dst == src; for the 16-byte kind func_089FB570(dst, src)), then `destroy_tail`(begin + n elements, end, sp+2).
// Size < n: the first `size` elements are assigned and `uninit_copy`(other.begin + ((end - begin) & ~(element - 1)), other.end, end, sp+1) copies the rest. end = begin + n bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (other == self) {`; `if (wide_elements) {`; `if (src != dst) {`; `if (!(size < s4)) {`; `if (signed_of(s4) > 0) {`; `if (signed_of(size) > 0) {`; `for (std::uint32_t i = 0u; i != size; ++i) {`.
template <unsigned Shift, std::uint32_t UninitCopy, std::uint32_t DestroyRange, std::uint32_t DestroyTail, bool WideElements>
void vector_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr unsigned shift = Shift;
    constexpr std::uint32_t uninit_copy = UninitCopy;
    constexpr std::uint32_t destroy_range = DestroyRange;
    constexpr std::uint32_t destroy_tail = DestroyTail;
    constexpr bool wide_elements = WideElements;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t step = 1u << shift;
    std::uint32_t s0 = word(rt, other);
    std::uint32_t s2 = word(rt, other + 4u);
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - word(rt, self)) >> shift);
    std::uint32_t s4 = static_cast<std::uint32_t>(signed_of(s2 - s0) >> shift);
    const auto assign = [&](std::uint32_t dst, std::uint32_t src) {
        if (wide_elements) {
            if (src != dst) {
                (void)call_set(rt, ctx, 0x089641C4u, {dst, src});
                put(rt, dst + 0x10u, word(rt, src + 0x10u));
            }
        } else {
            (void)call_set(rt, ctx, 0x089FB570u, {dst, src});
        }
    };
    std::uint32_t bytes;
    if (capacity < s4) {
        s4 <<= shift;
        const std::uint32_t block = call_set(rt, ctx, 0x08B35840u, {0x10u, s4});
        (void)call_set(rt, ctx, uninit_copy, {s0, s2, block, frame.at(0u)});
        (void)call_set(rt, ctx, destroy_range, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const std::uint32_t old = word(rt, self);
        if (old != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old});
        put(rt, self + 8u, block + s4);
        put(rt, self, block);
        bytes = s4;
    } else {
        std::uint32_t a2 = word(rt, self + 4u);
        const std::uint32_t a0 = word(rt, self);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(a2 - a0) >> shift);
        std::uint32_t s1 = a0;
        if (!(size < s4)) {
            if (signed_of(s4) > 0) {
                for (std::uint32_t i = 0u; i != s4; ++i) {
                    assign(s1, s0);
                    s0 += step;
                    s1 += step;
                }
                a2 = word(rt, self + 4u);
            }
            (void)call_set(rt, ctx, destroy_tail, {s1, a2, frame.at(2u)});
        } else {
            if (signed_of(size) > 0) {
                for (std::uint32_t i = 0u; i != size; ++i) {
                    assign(s1, s0);
                    s0 += step;
                    s1 += step;
                }
                s0 = word(rt, other);
                s2 = word(rt, other + 4u);
                a2 = word(rt, self + 4u);
            }
            const std::uint32_t used = (a2 - a0) & ~(step - 1u);
            (void)call_set(rt, ctx, uninit_copy, {s0 + used, s2, a2, frame.at(1u)});
        }
        bytes = s4 << shift;
    }
    put(rt, self + 4u, word(rt, self) + bytes);
    ctx.gpr[2] = self;
}

// func_08C12E58(this = a0, other = a1): vector assignment of 16-byte elements (vector_assign).
void func_08C12E58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<4u, 0x08C121C8u, 0x08C12060u, 0x08C12284u, false>(rt, ctx);
}

// func_08C0E654(this = a0, other = a1): vector assignment of 32-byte elements (vector_assign).
void func_08C0E654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<5u, 0x08C0D7F8u, 0x08BE5888u, 0x08C0D870u, true>(rt, ctx);
}

// func_08C1BF48(tree = a0, key = a1): unique-key insert of a counted string key (rb_find_insert with the node constructor func_08C1BE44).
void func_08C1BF48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C1BE44u); }

// func_08978080(a0, count = a1, holder = a2): with count > 0 the object s0 = holder[0] (null reports through func_08A5BB7C(0x08CA2F84)) is asked for its float with the virtual slot 0x54 and a0 = trunc(f0 * 60.0f); otherwise a0 = 1. It ends in the tail jump func_089C4558(a0, a1, a2, a3) (only a0 changes).
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `void func_08978080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {`; `if (!(signed_of(count) <= 0)) {`; `if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});`.
void func_08978080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t holder = ctx.gpr[6];
    std::uint32_t a0 = 1u;
    if (!(signed_of(count) <= 0)) {
        const std::uint32_t s0 = word(rt, holder);
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
        ctx.gpr[3] = word(rt, s0);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x54u);
        const GuestResult r = call_keep(rt, ctx, ctx.gpr[2], {s0}, {});
        a0 = ctx.fpu_float_to_word_ct<1u>(psp_mul(r.f0, std::bit_cast<float>(0x42700000u)));
    }
    tail_call(ctx, 0x089C4558u, {a0});
}

// func_08AA1D7C(stream = a0, value = a2 (low word), a3 (high word)): writes the 64-bit value big-endian: byte i (0..7) = bits 56 - 8 i .. 63 - 8 i of a3:a2 (the original builds each with a funnel shift (a2 >> n) | ((a3 << 1) << (31 - n)), or a3 >> n for n >= 32), into 8 bytes at sp, then stream->vtable[0x10C](stream, sp, 8); v0 is that call's.
void func_08AA1D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stream = ctx.gpr[4];
    const std::uint32_t low = ctx.gpr[6];
    const std::uint32_t high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t at = 0u;
    for (std::int32_t shift = 0x38; shift != -8; shift -= 8) {
        const std::uint32_t n = static_cast<std::uint32_t>(shift) & 31u;
        std::uint32_t part = (low >> n) | ((high << 1) << (~static_cast<std::uint32_t>(shift) & 31u));
        if ((static_cast<std::uint32_t>(shift) & 0x20u) != 0u) part = high >> n;
        put8(rt, frame.at(at), part);
        ++at;
    }
    (void)call_virtual(rt, ctx, stream, 0x10Cu, {stream, frame.at(0u), 8u});
}

// func_0886AAA4(self = a0, item = a1): item null: nothing. Otherwise item->vtable[0x10](item) < 8 (unsigned) is required (else nothing more); then func_0886AA0C(self, item->vtable[0x10](item)) (a second call of the same slot), self[0x24 + 4 * item->vtable[0x10](item)] = item (a third call),
// func_0887E1C8(item, self) and three calls item->vtable[0x14](item, i, self[0x10 + 4 i]) for i = 0, 1, 2. v0 is the last call's.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `if (item == 0u) return;`; `if (!(kind < 8u)) {`.
void func_0886AAA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t item = ctx.gpr[5];
    if (item == 0u) return;
    const std::uint32_t kind = call_virtual(rt, ctx, item, 0x10u, {item});
    if (!(kind < 8u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = kind;
    const std::uint32_t again = call_virtual(rt, ctx, item, 0x10u, {item});
    (void)call_set(rt, ctx, 0x0886AA0Cu, {self, again});
    const std::uint32_t third = call_virtual(rt, ctx, item, 0x10u, {item});
    put(rt, self + (third << 2) + 0x24u, item);
    (void)call_set(rt, ctx, 0x0887E1C8u, {item, self});
    std::uint32_t at = self;
    for (std::uint32_t i = 0u; i != 3u; ++i) {
        const std::uint32_t table = word(rt, item);
        const std::uint32_t slot = word(rt, table + 0x14u);
        const std::uint32_t argument = word(rt, at + 0x10u);
        ctx.gpr[2] = table;  // v0 = the vtable, v1 = the slot's function (jalr v1)
        ctx.gpr[3] = slot;
        (void)call_set(rt, ctx, slot, {item, i, argument});
        at += 4u;
    }
}

// func_08C053E8(binding = a0, a1, holder = a2, a3, arg = t0; f12 result of nothing): two pointer-to-member-function pairs (function word / adjustment word: binding[0x14] / [0x18] and [0x1C] / [0x20]; an odd adjustment means "virtual": the function word is an offset into the vtable of the adjusted object). s3 = func_08AA2A30(arg); f20 = func_08AA2898(arg + 4)'s float;
// r = the first member called on holder[0] + (adj1 >> 1) (v0 of the call); then a tail jump to the second member with a0 = r + (adj2 >> 1), a1 = s3, f12 = f20.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand and by an independent read-only review): `const std::uint32_t target1 = (adjust1 & 1u) != 0u ? word(rt, word(rt, this1) + function1) : function1;`; `const std::uint32_t target2 = (adjust2 & 1u) != 0u ? word(rt, word(rt, this2) + function2) : function2;`; `ctx.gpr[2] = (adjust2 & 1u) != 0u ? word(rt, this2) + function2 : 0u;  // v0 = the address of the vtable entry (virtual) or the masked adjustment (0)`.
void func_08C053E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t binding = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[6];
    const std::uint32_t arg = ctx.gpr[8];
    (void)call_set(rt, ctx, 0x08AA2A30u, {arg});
    const std::uint32_t s3 = ctx.gpr[2];
    (void)call_keep(rt, ctx, 0x08AA2898u, {arg + 4u}, {});
    const float f20 = ctx.fpr[0];
    const std::uint32_t adjust1 = word(rt, binding + 0x18u);
    const std::uint32_t function1 = word(rt, binding + 0x14u);
    const std::uint32_t object = word(rt, holder);
    const std::uint32_t this1 = object + static_cast<std::uint32_t>(signed_of(adjust1) >> 1);
    const std::uint32_t target1 = (adjust1 & 1u) != 0u ? word(rt, word(rt, this1) + function1) : function1;
    // The member is entered with v1 = adjust1 & 1 (andi at 0x08C05434) and, on the virtual arm, v0 = the address of the
    // vtable entry (0x08C05454-58); on the other arm v0 is still func_08AA2898's. A member that never writes v0 returns them.
    ctx.gpr[3] = adjust1 & 1u;
    if ((adjust1 & 1u) != 0u) ctx.gpr[2] = word(rt, this1) + function1;
    const std::uint32_t result = call_set(rt, ctx, target1, {this1, adjust1, function1, target1});
    const std::uint32_t adjust2 = word(rt, binding + 0x20u);
    const std::uint32_t function2 = word(rt, binding + 0x1Cu);
    const std::uint32_t this2 = result + static_cast<std::uint32_t>(signed_of(adjust2) >> 1);
    const std::uint32_t target2 = (adjust2 & 1u) != 0u ? word(rt, word(rt, this2) + function2) : function2;
    ctx.fpr[12] = f20;
    ctx.gpr[2] = (adjust2 & 1u) != 0u ? word(rt, this2) + function2 : 0u;  // v0 = the address of the vtable entry (virtual) or the masked adjustment (0)
    ctx.gpr[3] = (adjust2 & 1u) != 0u ? function2 : static_cast<std::uint32_t>(signed_of(adjust2) >> 1);
    ctx.gpr[25] = target2;
    tail_call(ctx, target2, {this2, s3});
}

// @@END

}  // namespace
// Batch 160: more members of the 0x08BE9D90 family (tools.family_rows), verified by synthetic cases.
void func_08B948FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08B947FCu); }
void func_08BAF940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08BAF828u); }
void func_08BF6F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08BF6E48u); }
void func_08BF754C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08BF7448u); }
void func_08C01800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C016FCu); }
void func_08C2FD7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C2FC7Cu); }
void func_08C334A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C333A0u); }
void func_08C33AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C339E8u); }
void func_08C4EB54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rb_find_insert(rt, ctx, 0x08C4EA54u); }
}  // namespace hot_batch_99
}  // namespace gt::game

#define HOT99(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_99::name, gt::ReturnKind::kind)
#define HOT99N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_99::name, gt::ReturnKind::kind)

// HOT99REGS
HOT99(0x08C20B4Cu, func_08C20B4C, Word);
HOT99(0x08C23E60u, func_08C23E60, Word);
HOT99(0x08B2E314u, func_08B2E314, Word);
HOT99(0x08B5596Cu, func_08B5596C, Word);
HOT99(0x08B7C830u, func_08B7C830, Word);
HOT99(0x088B29FCu, func_088B29FC, Word);
HOT99(0x08C3E4E8u, func_08C3E4E8, Word);
HOT99(0x08B58874u, func_08B58874, Word);
HOT99(0x08B52578u, func_08B52578, Word);
// HOT99(0x08B57968u, func_08B57968, Word);  // HOT99: 109 compared cases all take the f12 == constant arm of kind 7 (idempotent writes, none changed memory); the other four arms never ran
// HOT99(0x08AE1D6Cu, func_08AE1D6C, Word);  // HOT99: the routes only reach the total <= 0 exit (the whole loop body never ran in 84 compared cases, none changed memory)
HOT99(0x08B58218u, func_08B58218, Word);
HOT99(0x08C238F0u, func_08C238F0, Word);
HOT99(0x089B684Cu, func_089B684C, Word);
HOT99(0x08C1C1A8u, func_08C1C1A8, DoubleWord);
HOT99(0x08BE9D90u, func_08BE9D90, DoubleWord);
HOT99(0x08A7D24Cu, func_08A7D24C, Word);
HOT99(0x08AAACF4u, func_08AAACF4, Word);
HOT99(0x08B7C6BCu, func_08B7C6BC, Word);
HOT99(0x08B7BB6Cu, func_08B7BB6C, Word);
HOT99(0x08B23928u, func_08B23928, Word);
HOT99(0x08B6D564u, func_08B6D564, Float);
HOT99(0x089A932Cu, func_089A932C, Word);
HOT99(0x089DA5C0u, func_089DA5C0, Word);
HOT99(0x08B49D98u, func_08B49D98, Float);
HOT99(0x08C468E4u, func_08C468E4, Word);
HOT99(0x08BEF574u, func_08BEF574, Word);
HOT99(0x08B509ECu, func_08B509EC, Word);
HOT99(0x08B52B70u, func_08B52B70, Word);
HOT99(0x08B23FA0u, func_08B23FA0, Word);
HOT99(0x08AE69F4u, func_08AE69F4, Word);
HOT99(0x0896A254u, func_0896A254, Word);
HOT99(0x08C12E58u, func_08C12E58, Word);
HOT99(0x08C0E654u, func_08C0E654, Word);
HOT99(0x08C1BF48u, func_08C1BF48, DoubleWord);
// HOT99(0x08978080u, func_08978080, Word);  // HOT99: compared 0 in every run (all 25 cases not comparable: the tail callee func_089C4558 reaches the sceDisplay import)
HOT99(0x08AA1D7Cu, func_08AA1D7C, Word);
HOT99(0x0886AAA4u, func_0886AAA4, Word);
HOT99(0x08C053E8u, func_08C053E8, Word);
// HOT99(0x08B948FCu, func_08B948FC, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08BAF940u, func_08BAF940, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08BF6F58u, func_08BF6F58, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08BF754Cu, func_08BF754C, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08C01800u, func_08C01800, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08C2FD7Cu, func_08C2FD7C, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08C334A0u, func_08C334A0, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08C33AE8u, func_08C33AE8, DoubleWord);  // no comparable synthetic case yet
// HOT99(0x08C4EB54u, func_08C4EB54, DoubleWord);  // no comparable synthetic case yet
