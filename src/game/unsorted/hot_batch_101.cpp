// Hot batch 101: reached, unregistered functions of the main checkout with batch 100 merged (probe `b101p`: `tools.family_members --write-probe-symbols FILE --max-insns 300`, 13,968 recompiled functions that no `src/` file registers, then `tools.decomp_gate probe b101p --symbols FILE`), filtered to those that store to memory, have no
// sce* import within two calls and are not written-but-unregistered elsewhere. Written from `tools.disasm` one function at a time. Role words in the comments describe access shapes; none is a game name.
//
// Written 60, registered 49 (every one compared in the five-route fast gate, 0 mismatches, sabotage pinned on all of them). Not registered (11, each has a commented HOT101 line with the reason): five whose compared cases are all "not comparable" (their callees reach sce* imports: func_08ABC9D0, func_08ABCC1C, func_08AC3308, func_08AD08A4, func_08AD0974),
// five that write memory but no compared case changed it (func_08870448, func_0891FEF0, func_0892C034, func_0885E788, func_08BD2D24) and func_08B93AE8, whose six compared cases all had an empty tree (the walk never ran).
//
// The pool this time: of the 13,968 unregistered recompiled functions the probe reached, the ones that store to memory (or are pure forwarders and getters), have no `sce*` import within a few calls (jal AND j targets past the end of .text) and are not written elsewhere are about 100; the hottest unwritten ones were tail-jump stubs and getters
// (func_08B4B864 134,690 probed calls, func_08B77BF4 97,882, func_08B4BAA8 86,154), which earlier batches had skipped as too small. A tail stub passes the argument registers it sets in its delay slot and leaves the rest alone.
//
// Left (read, not written): the copy-on-write string replace func_08B8634C, the soft-float 64-bit division func_08B71720 / func_08B71394, the VFPU-free but float-heavy physics func_08945F6C / func_089572C4, the allocator free-list tree func_08BE3C80 / func_08BE3DA8, the parsers func_08B51D60 / func_08B196B8 / func_08ADC4E0 / func_08ABE53C (long, many callees, few hits)
// and every function that is written but unregistered on main (func_08A9DD14 included: it switches off start-up code, FINDINGS).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value; `call_setf` also passes the float registers. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
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

namespace gt::game {
namespace hot_batch_101 {
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
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
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
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
inline std::uint32_t insert_bits(std::uint32_t target, std::uint32_t value, unsigned pos, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// ---- functions ----

// func_088693C4(object = a0, value = a1): the low byte of a1 goes to object[0x1C4]. v0 untouched.
void func_088693C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put8(rt, ctx.gpr[4] + 0x1C4u, ctx.gpr[5]); }

// func_08870448(object = a0, f12, f13, f14): object[0xC] points at a block; the three floats go to its +0x30 / +0x34 / +0x38 (the pointer is re-read before each store). v0 = the third read of object[0xC], v1 = the second.
void func_08870448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    std::uint32_t block = word(rt, object + 0xCu);
    put_float(rt, block + 0x30u, ctx.fpr[12]);
    block = word(rt, object + 0xCu);
    ctx.gpr[3] = block;
    put_float(rt, block + 0x34u, ctx.fpr[13]);
    block = word(rt, object + 0xCu);
    ctx.gpr[2] = block;
    put_float(rt, block + 0x38u, ctx.fpr[14]);
}

// func_0887BD88(value = a0): the halfword at 0x08CE2F36 becomes a0 & 0xFFFF; v0 = 0x08CE0000 (the lui of the address pair).
void func_0887BD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = lui_of(0x08CE2F36u);
    put16(rt, 0x08CE2F36u, ctx.gpr[4]);
}

// func_088E43C0(pointer = a0): a0 + 8 is stored at the word 0x08CE9C54; a0 ends as a0 + 8 and v0 = 0x08CF0000 (the lui of the address pair).
void func_088E43C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[4] += 8u;
    ctx.gpr[2] = lui_of(0x08CE9C54u);
    put(rt, 0x08CE9C54u, ctx.gpr[4]);
}

// func_0891FEF0(object = a0): func_08926C2C(object), then object[0x248] = -1; v0 = -1.
void func_0891FEF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x08926C2Cu, {object});
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, object + 0x248u, 0xFFFFFFFFu);
}

// func_08965DDC(object = a0, value = a1): the low four bits of object[0xE0] become the low four bits of ((a1 & 0xFF) | (object[0xE0] & 0xE)); v0 = the new word, v1 = old & 0xE, a1 = the or'ed value.
void func_08965DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t old = word(rt, object + 0xE0u);
    const std::uint32_t kept = old & 0xEu;
    const std::uint32_t merged = (ctx.gpr[5] & 0xFFu) | kept;
    const std::uint32_t result = insert_bits(old, merged, 0u, 4u);
    ctx.gpr[3] = kept;
    ctx.gpr[5] = merged;
    ctx.gpr[2] = result;
    put(rt, object + 0xE0u, result);
}

// func_0885E788(object = a0): object[0] points at a block whose byte +0x9C becomes 0. v0 = object[0].
void func_0885E788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = block;
    put8(rt, block + 0x9Cu, 0u);
}

// func_0885E778(object = a0): the same with 1: block[0x9C] = 1. v0 = 1, v1 = object[0].
void func_0885E778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = block;
    ctx.gpr[2] = 1u;
    put8(rt, block + 0x9Cu, 1u);
}

// func_08B54F24(object = a0, a1, a2): object[0] = a1, object[4] = a2, then the tail jump func_08B54E98(object, 0x200, a2).
void func_08B54F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    put(rt, object, ctx.gpr[5]);
    put(rt, object + 4u, ctx.gpr[6]);  // the delay slot of the j
    tail_call(ctx, 0x08B54E98u, {object, 0x200u, ctx.gpr[6]});
}

// func_08BD2D24(object = a0, delta = a1): when object[0] (the pointer v0) is zero returns (a0 = v0 + a1, v1 = the same, v0 = object[0]); otherwise object[0] = v0 + a1 and the tail jump func_08908F54(v0 + a1, a1, object).
void func_08BD2D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t pointer = word(rt, object);
    const std::uint32_t moved = pointer + ctx.gpr[5];
    ctx.gpr[2] = pointer;
    ctx.gpr[3] = moved;
    ctx.gpr[6] = object;
    ctx.gpr[4] = moved;  // the delay slot of the beqz: both paths
    if (pointer == 0u) return;
    put(rt, object, moved);  // the delay slot of the j
    tail_call(ctx, 0x08908F54u, {moved, ctx.gpr[5], object});
}

// func_08AC3308(object = a0): func_08AC31B4(object[0]); v0 is its answer.
void func_08AC3308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_set(rt, ctx, 0x08AC31B4u, {word(rt, ctx.gpr[4])});
}

// func_08AD08A4(): func_08B2F060(); then the object in the word 0x08CFFAAC (v0 = 0x08D00000, a0 = the object, v1 = its vtable) is handed to the virtual function in slot 0x20 of its vtable (t9) as a tail jump.
void func_08AD08A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_set(rt, ctx, 0x08B2F060u, {});
    const std::uint32_t object = word(rt, 0x08CFFAAC);
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + 0x20u);
    ctx.gpr[2] = lui_of(0x08CFFAACu);
    ctx.gpr[3] = table;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}

// func_0885E91C(object = a0): v0 = (func_088D7FF8(object[0][0x5AC]) != 0).
// Branches the routes do not take (checked against the asm by hand): the callee answering zero (v0 = 0).
void func_0885E91C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = block;
    const std::uint32_t answer = call_set(rt, ctx, 0x088D7FF8u, {word(rt, block + 0x5ACu)});
    ctx.gpr[2] = answer != 0u ? 1u : 0u;
}

// func_08A8798C(object = a0): func_08B70F28(f12 = object[0x14]); v0 / f0 are its answers.
void func_08A8798C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_setf(rt, ctx, 0x08B70F28u, {}, {float_at(rt, ctx.gpr[4] + 0x14u)});
}

// func_088D4824(dest = a0, source = a1): a0 and a1 each point at a block pointer; 0x116 bytes of the source block go to the destination block in four pieces: [0, 0x60) by words (16 bytes per turn: four loads, then four stores), [0x60, 0x82) (0x20 bytes of words or of halfwords, then one halfword at 0x80), [0x82, 0x112) in 8-byte halfword turns or 16-byte word turns
// (halfwords when either address + 0x60 / + 0x82 is not word aligned), and the last four bytes at 0x112 (two halfwords or one word). v0 = a0.
// Branches the routes do not take (checked against the asm by hand): the halfword arm of the first piece (taken when a base is not word aligned; both were aligned) and the word arm of the second piece (taken when both bases are 2 mod 4; the halfword arm of the second piece, whose address + 0x82 is never word aligned for aligned bases, always ran).
void func_088D4824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest_ref = ctx.gpr[4];
    const std::uint32_t source_ref = ctx.gpr[5];
    auto copy_words = [&](std::uint32_t from, std::uint32_t to, std::uint32_t count16) {
        for (std::uint32_t i = 0u; i < count16; ++i) {
            const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
            put(rt, to, w0);
            put(rt, to + 4u, w1);
            put(rt, to + 8u, w2);
            put(rt, to + 0xCu, w3);
            from += 0x10u;
            to += 0x10u;
        }
    };
    auto copy_halves = [&](std::uint32_t from, std::uint32_t to, std::uint32_t count8) {
        for (std::uint32_t i = 0u; i < count8; ++i) {
            const std::uint32_t h0 = half_u(rt, from), h1 = half_u(rt, from + 2u), h2 = half_u(rt, from + 4u), h3 = half_u(rt, from + 6u);
            put16(rt, to, h0);
            put16(rt, to + 2u, h1);
            put16(rt, to + 4u, h2);
            put16(rt, to + 6u, h3);
            from += 8u;
            to += 8u;
        }
    };
    std::uint32_t source = word(rt, source_ref);
    std::uint32_t target = word(rt, dest_ref);
    copy_words(source, target, 6u);
    source = word(rt, source_ref);
    target = word(rt, dest_ref);
    if (((source + 0x60u) | (target + 0x60u)) & 3u) {
        copy_halves(source + 0x60u, target + 0x60u, 4u);
    } else {
        copy_words(source + 0x60u, target + 0x60u, 2u);
    }
    put16(rt, target + 0x80u, half_u(rt, source + 0x80u));
    source = word(rt, source_ref);
    target = word(rt, dest_ref);
    if (((source + 0x82u) | (target + 0x82u)) & 3u) {
        copy_halves(source + 0x82u, target + 0x82u, 18u);
        const std::uint32_t h1 = half_u(rt, source + 0x114u);
        const std::uint32_t h0 = half_u(rt, source + 0x112u);
        put16(rt, target + 0x114u, h1);
        put16(rt, target + 0x112u, h0);
    } else {
        copy_words(source + 0x82u, target + 0x82u, 9u);
        put(rt, target + 0x112u, word(rt, source + 0x112u));
    }
    ctx.gpr[2] = dest_ref;
}

// func_0892C034(object = a0): the four owned blocks at object[4], [8], [0xC], [0x10] are released one after the other (func_08B61FC0(block) when the pointer is not zero, then the pointer is cleared), and object[0] (a byte) is cleared. v0 is the last read pointer, or the last release's answer.
void func_0892C034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    for (std::uint32_t offset = 4u; offset <= 0x10u; offset += 4u) {
        const std::uint32_t block = word(rt, object + offset);
        ctx.gpr[2] = block;
        ctx.gpr[4] = block;  // the delay slot of the beqz: both paths
        if (block != 0u) {
            (void)call_set(rt, ctx, 0x08B61FC0u, {block});
            put(rt, object + offset, 0u);
        }
    }
    put8(rt, object, 0u);
}

// func_0892BF7C(object = a0, count = a1 (signed byte)): object[0] = count; four blocks from func_08B6461C are stored at object[4] (count * 576 bytes asked), object[8] (the byte re-read, times 576), object[0xC] and object[0x10] (the byte itself asked); when the byte is above zero the 'count' bytes of the last two blocks are set to 0xFF. v0 = 0 after the loop, the last block (v0) when the byte is zero or negative.
// Branches the routes do not take (checked against the asm by hand): a count byte <= 0 (the early return, v0 = the last block); only the loop that fills the last two blocks with 0xFF ran.
void func_0892BF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t count = sext8(ctx.gpr[5]);
    ctx.gpr[5] = count;
    std::uint32_t triple = count + (count << 1);
    put8(rt, object, count);  // the delay slot of the first jal
    std::uint32_t answer = call_set(rt, ctx, 0x08B6461Cu, {(triple << 8) - (triple << 6)});
    std::uint32_t current = sext8(byte_u(rt, object));
    put(rt, object + 4u, answer);
    triple = current + (current << 1);
    answer = call_set(rt, ctx, 0x08B6461Cu, {(triple << 8) - (triple << 6)});
    current = sext8(byte_u(rt, object));
    put(rt, object + 8u, answer);  // the delay slot of the next jal
    answer = call_set(rt, ctx, 0x08B6461Cu, {current});
    current = sext8(byte_u(rt, object));
    put(rt, object + 0xCu, answer);
    answer = call_set(rt, ctx, 0x08B6461Cu, {current});
    ctx.gpr[3] = sext8(byte_u(rt, object));
    put(rt, object + 0x10u, answer);
    if (signed_of(ctx.gpr[3]) <= 0) return;
    std::uint32_t index = 0u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    do {
        put8(rt, word(rt, object + 0xCu) + index, 0xFFu);
        put8(rt, word(rt, object + 0x10u) + index, 0xFFu);
        ++index;
    } while (signed_of(index) < signed_of(sext8(byte_u(rt, object))));
    ctx.gpr[4] = index;
    ctx.gpr[2] = 0u;
}

// func_0888AB4C(object = a0, value = a1): twenty 0xA8-byte entries: byte [0x11 + 0xA8 * i] = 0 and word [0xD20 + 4 * i] = i; object[0xD70] = 0; then for i in 0..3 func_0888A9B8(object, i, 0x08C89A84, a3, 0, 0, 1) followed by the halfwords [0x1C2A + 2i] = 0, [0x1558 + 2i] = 0xFFFF and [0x1C20 + 2i] = 0; finally byte [0x1C35] = value & 0xFF, halfwords [0x1C28] and [0x1C32] = 0, byte [0x1C34] = 0.
// v0 is the last func_0888A9B8's answer.
void func_0888AB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5] & 0xFFu;
    for (std::uint32_t i = 0u; i < 0x14u; ++i) {
        put8(rt, object + 0x11u + 0xA8u * i, 0u);
        put(rt, object + 0xD20u + 4u * i, i);
    }
    put(rt, object + 0xD70u, 0u);
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        (void)call_set(rt, ctx, 0x0888A9B8u, {object, i, 0x08C89A84u, ctx.gpr[7], 0u, 0u, 1u});
        put16(rt, object + 0x1C2Au + 2u * i, 0u);
        put16(rt, object + 0x1558u + 2u * i, 0xFFFFu);
        put16(rt, object + 0x1C20u + 2u * i, 0u);
    }
    put8(rt, object + 0x1C35u, value);
    put16(rt, object + 0x1C28u, 0u);
    put16(rt, object + 0x1C32u, 0u);
    put8(rt, object + 0x1C34u, 0u);
}

// func_0896A4E0(object = a0): when func_08966174(object) is not zero: func_0896A35C(object, 0, 0, sp + 0, sp + 4) and func_08966E24(object, sp + 0, sp + 4) follow. v0 is the last call's (the first call's when it answered zero).
void func_0896A4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t found = call_set(rt, ctx, 0x08966174u, {object});
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = frame.at(0u);
    ctx.gpr[8] = frame.at(4u);  // the delay slot of the beqz: both paths
    if (found == 0u) return;
    (void)call_set(rt, ctx, 0x0896A35Cu, {object, 0u, 0u, frame.at(0u), frame.at(4u)});
    (void)call_set(rt, ctx, 0x08966E24u, {object, frame.at(0u), frame.at(4u)});
}

// func_089311EC(a0, a1, sink = a2, tail = a3): func_08930A28(a0, a1, sp + 0); when it answers zero v0 = 0. Otherwise a0 = func_089363FC()[0x1F0] and the word at sp + 0 go to func_08937D90, whose answer is handed on as func_08B7C830(sink, answer, tail); v0 is that call's.
// Branches the routes do not take (checked against the asm by hand): func_08930A28 answering zero (v0 = 0).
void func_089311EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[6];
    const std::uint32_t tail = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t found = call_set(rt, ctx, 0x08930A28u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
    if (found == 0u) return;
    const std::uint32_t owner = call_set(rt, ctx, 0x089363FCu, {});
    const std::uint32_t answer = call_set(rt, ctx, 0x08937D90u, {word(rt, owner + 0x1F0u), word(rt, frame.at(0u))});
    (void)call_set(rt, ctx, 0x08B7C830u, {sink, answer, tail});
}

// func_08930458(a0, a1): a 0x190-byte stack object is built by func_0893600C(sp); when func_08935B10(sp, a1, a0, a1') answers non-zero, v0 = func_0893273C(sp); either way func_08932448(sp) tears it down and v0 is the saved answer (0 when func_08935B10 answered zero).
// Branches the routes do not take (checked against the asm by hand): func_08935B10 answering zero (the saved answer 0).
void func_08930458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x190u);
    (void)call_set(rt, ctx, 0x0893600Cu, {frame.at(0u)});
    const std::uint32_t found = call_set(rt, ctx, 0x08935B10u, {frame.at(0u), ctx.gpr[5], first, second});
    std::uint32_t result = 0u;
    if (found != 0u) result = call_set(rt, ctx, 0x0893273Cu, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08932448u, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// func_08935B88(a0 = target, a1 = key): the pair (v0, v1) of func_089364AC(func_089363FC(), key, 0) is looked at; when it is not (-1, -1) func_08935B10(target, key, v0, v1) answers. Otherwise the same lookup with 0x26 as the third argument; when that pair is not (-1, -1) func_08935B10(target, key, v0, v1) answers, else v0 = 0. v0 is the answer.
// Branches the routes do not take (checked against the asm by hand): the first lookup answering (-1, -1): the whole second half (the lookup with 0x26, its call and the v0 = 0 return) never ran.
void func_08935B88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t owner = call_set(rt, ctx, 0x089363FCu, {});
    ctx.gpr[5] = key;
    std::uint32_t low = call_set(rt, ctx, 0x089364ACu, {owner, key, 0u});
    std::uint32_t high = ctx.gpr[3];
    if (low == 0xFFFFFFFFu && high == 0xFFFFFFFFu) {
        owner = call_set(rt, ctx, 0x089363FCu, {});
        ctx.gpr[5] = key;
        low = call_set(rt, ctx, 0x089364ACu, {owner, key, 0x26u});
        high = ctx.gpr[3];
        if (low == 0xFFFFFFFFu && high == 0xFFFFFFFFu) {
            ctx.gpr[4] = 0u;
            ctx.gpr[2] = 0u;
            return;
        }
    }
    ctx.gpr[4] = target;
    (void)call_set(rt, ctx, 0x08935B10u, {target, key, low, high});
    ctx.gpr[4] = ctx.gpr[2];
}

// func_08930290(a0, a1): (s0, s1) = (v0, v1) of func_08930244(); the tail jump func_0893665C(func_089363FC(), a1, s0, s1).
void func_08930290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = call_set(rt, ctx, 0x08930244u, {ctx.gpr[4], ctx.gpr[5]});  // the callee reads the caller's a0 / a1
    const std::uint32_t high = ctx.gpr[3];
    const std::uint32_t owner = call_set(rt, ctx, 0x089363FCu, {});
    ctx.gpr[2] = owner;
    tail_call(ctx, 0x0893665Cu, {owner, ctx.gpr[5], low, high});
}

// func_08897470(dest_ref = a0, source_ref = a1): both point at a block pointer. 0xB8 bytes of the source block go to the destination block: [0, 0x80) in 16-byte turns (four loads, then four stores), [0x80, 0x8C) (loads of +8, +0, +4 then stores), then the words +0x8C..+0x98, +0x9C, +0xA0..+0xB4 (all
// loaded first, stored with +0xB4 first and +0xB0 in the delay slot of the call). Then func_08896E38(dest_ref + 8, source_ref + 8). v0 = dest_ref.
void func_08897470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest_ref = ctx.gpr[4];
    const std::uint32_t source_ref = ctx.gpr[5];
    const std::uint32_t source = word(rt, source_ref);
    const std::uint32_t dest = word(rt, dest_ref);
    for (std::uint32_t offset = 0u; offset < 0x80u; offset += 0x10u) {
        const std::uint32_t w0 = word(rt, source + offset), w1 = word(rt, source + offset + 4u), w2 = word(rt, source + offset + 8u), w3 = word(rt, source + offset + 0xCu);
        put(rt, dest + offset, w0);
        put(rt, dest + offset + 4u, w1);
        put(rt, dest + offset + 8u, w2);
        put(rt, dest + offset + 0xCu, w3);
    }
    {
        const std::uint32_t w88 = word(rt, source + 0x88u), w80 = word(rt, source + 0x80u), w84 = word(rt, source + 0x84u);
        put(rt, dest + 0x88u, w88);
        put(rt, dest + 0x80u, w80);
        put(rt, dest + 0x84u, w84);
    }
    std::uint32_t w[0xB8u / 4u] = {};
    for (std::uint32_t offset = 0x8Cu; offset <= 0xB4u; offset += 4u) w[offset / 4u] = word(rt, source + offset);
    put(rt, dest + 0xB4u, w[0xB4u / 4u]);
    for (std::uint32_t offset = 0x8Cu; offset <= 0xB0u; offset += 4u) put(rt, dest + offset, w[offset / 4u]);  // 0xB0 is the delay slot of the jal
    (void)call_set(rt, ctx, 0x08896E38u, {dest_ref + 8u, source_ref + 8u});
    ctx.gpr[2] = dest_ref;
}

// func_088D176C(source = a0, dest = a1): 0x1CC bytes at source go to dest (16-byte turns of four loads and four stores up to 0x1C0, then three words; with swl/swr stores, i.e. byte-exact unaligned ones, when either address is not word aligned). v0 = dest + func_088D06A4() (a constant 0x1D0).
// Branches the routes do not take (checked against the asm by hand): the unaligned arm (the swl/swr stores); source and destination were always word aligned.
void func_088D176C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t dest = ctx.gpr[5];
    const bool unaligned = ((source | dest) & 3u) != 0u;
    auto store = [&](std::uint32_t address, std::uint32_t value) {
        if (unaligned) {
            for (unsigned i = 0u; i < 4u; ++i) rt.memory().store8(address + i, static_cast<std::uint8_t>(value >> (8u * i)));
        } else {
            put(rt, address, value);
        }
    };
    for (std::uint32_t offset = 0u; offset < 0x1C0u; offset += 0x10u) {
        const std::uint32_t w0 = word(rt, source + offset), w1 = word(rt, source + offset + 4u), w2 = word(rt, source + offset + 8u), w3 = word(rt, source + offset + 0xCu);
        store(dest + offset, w0);
        store(dest + offset + 4u, w1);
        store(dest + offset + 8u, w2);
        store(dest + offset + 0xCu, w3);
    }
    const std::uint32_t t0 = word(rt, source + 0x1C0u), t1 = word(rt, source + 0x1C4u), t2 = word(rt, source + 0x1C8u);
    if (unaligned) {
        store(dest + 0x1C0u, t0);
        store(dest + 0x1C4u, t1);
        store(dest + 0x1C8u, t2);
    } else {
        put(rt, dest + 0x1C8u, t2);
        put(rt, dest + 0x1C0u, t0);
        put(rt, dest + 0x1C4u, t1);
    }
    const std::uint32_t size = call_set(rt, ctx, 0x088D06A4u, {});
    ctx.gpr[2] = dest + size;
}

// func_08909CB4(header = a0): rebases the pointers of a block that was built at another address. delta = header - header[4]; header[4] = header; header[0x14] += delta (the table of count = (h[0x10] as u16) * (h[0x12] as u16) entries, signed count above zero);
// every non-zero table entry gets + delta and is the head of a zero-terminated chain of words that each get + delta too; then header[0x34] += delta and header[0x30] entries of 16 bytes each starting there have their first word += delta. v0 = the last value stored.
// Branches the routes do not take (checked against the asm by hand): a product <= 0 (no table), a zero table entry, an empty chain and a header[0x30] of zero; every compared case had a table with non-empty chains.
void func_08909CB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t header = ctx.gpr[4];
    std::uint32_t t1 = half_u(rt, header + 0x10u);
    std::uint32_t t0 = half_u(rt, header + 0x12u);
    const std::uint32_t old_base = word(rt, header + 4u);
    std::uint32_t table = word(rt, header + 0x14u);
    put(rt, header + 4u, header);
    const std::uint32_t delta = header - old_base;
    table = delta + table;
    const std::uint32_t product = t1 * t0;
    put(rt, header + 0x14u, table);  // the delay slot of the bgtz
    std::uint32_t v0;
    if (signed_of(product) > 0) {
        std::uint32_t index = 0u;
        for (;;) {
            const std::uint32_t entry = table + (index << 2);
            v0 = word(rt, entry);
            if (v0 != 0u) {
                std::uint32_t chain = delta + v0;
                put(rt, entry, chain);
                v0 = word(rt, chain);
                if (v0 != 0u) {
                    do {
                        put(rt, chain, delta + v0);
                        chain += 4u;
                        v0 = word(rt, chain);
                    } while (v0 != 0u);
                }
                t1 = half_u(rt, header + 0x10u);
                t0 = half_u(rt, header + 0x12u);
            }
            ++index;
            if (!(signed_of(index) < signed_of(t0 * t1))) break;
            table = word(rt, header + 0x14u);  // the delay slot of the bnel
        }
    }
    v0 = word(rt, header + 0x34u);
    const std::uint32_t entries = word(rt, header + 0x30u);
    v0 = delta + v0;
    put(rt, header + 0x34u, v0);
    if (entries == 0u) {
        ctx.gpr[2] = v0;
        return;
    }
    std::uint32_t cursor = v0;
    for (std::uint32_t i = 1u;; ++i) {
        v0 = delta + word(rt, cursor);
        put(rt, cursor, v0);
        cursor += 0x10u;
        if (entries == i) break;
    }
    ctx.gpr[2] = v0;
}

// func_0892EFB4(object = a0): clears object[0] .. [0x34] (fourteen words), memsets (func_08B36018) [0x38, +0x1B0) and [0x1E8, +0x60) to zero, clears the words [0x248], [0x24C], [0x250], [0x254], [0x258], [0x25C] .. [0x28C], memsets [0x290, +0x70), clears [0x300], [0x304] .. [0x314]. v0 is the last memset's answer.
void func_0892EFB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    for (std::uint32_t offset = 0u; offset <= 0x34u; offset += 4u) put(rt, object + offset, 0u);
    (void)call_set(rt, ctx, 0x08B36018u, {object + 0x38u, 0u, 0x1B0u});
    (void)call_set(rt, ctx, 0x08B36018u, {object + 0x1E8u, 0u, 0x60u});
    put(rt, object + 0x258u, 0u);
    put(rt, object + 0x248u, 0u);
    for (std::uint32_t offset = 0x25Cu; offset <= 0x28Cu; offset += 4u) put(rt, object + offset, 0u);
    put(rt, object + 0x254u, 0u);
    put(rt, object + 0x24Cu, 0u);
    put(rt, object + 0x250u, 0u);
    (void)call_set(rt, ctx, 0x08B36018u, {object + 0x290u, 0u, 0x70u});
    put(rt, object + 0x300u, 0u);
    put(rt, object + 0x314u, 0u);
    put(rt, object + 0x304u, 0u);
    put(rt, object + 0x308u, 0u);
    put(rt, object + 0x30Cu, 0u);
    put(rt, object + 0x310u, 0u);
}

// func_08BC264C(object = a0, a1, a2): object[0] = 0x08CC44E8 (a vtable), object[8] = a1, object[0xC] = a2, object[4] = 0; then the tail jump func_08B18DC4(func_08BBD034(), object, a2). v0 is func_08BBD034's answer (the vtable address when it sets none).
void func_08BC264C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    ctx.gpr[2] = 0x08CC44E8u;
    put(rt, object + 8u, ctx.gpr[5]);
    put(rt, object, 0x08CC44E8u);
    put(rt, object + 0xCu, ctx.gpr[6]);
    put(rt, object + 4u, 0u);  // the delay slot of the jal
    const std::uint32_t answer = call_set(rt, ctx, 0x08BBD034u, {});
    tail_call(ctx, 0x08B18DC4u, {answer, object, ctx.gpr[6]});
}

// func_08B5F444(z = a0, header = a1): the erase-and-rebalance of the red-black tree whose header holds root / leftmost / rightmost at +4 / +8 / +0xC and whose nodes hold colour / parent / left / right at +0 / +4 / +8 / +0xC (red = 0, black = 1) - the library routine that unlinks node z.
// y = z when z has at most one child (x = that child), else y = the leftmost node of z's right subtree (x = y's right child). When y != z, y takes z's place (links, colour swapped with z) and y = z afterwards; when y == z, x takes its place under z's parent and the header's leftmost / rightmost are fixed (z's parent when it has no
// other child on that side, else the minimum / maximum below x). When the removed colour is black the fix-up loop runs (x is the node that is one black short, x_parent its parent): sibling red -> recolour and rotate (func_08B5F1F8 = rotate left, func_08B5F258 = rotate right, a0 = node, a1 = &header[4]);
// both nephews black -> sibling red and climb; else the far-nephew arm with at most one extra rotation; then x is painted black. v0 = the removed node (z).
// Branches the routes do not take (checked against the asm by hand): the weakest proof of the batch (84 compared cases in the batch gates, 3,000 dense cases of the race route all equal with memory changed, but the coverage run lists most arms as never taken): z with a single left child, a successor deeper than z's right child (the y_parent / x_parent arm), z being the root, the leftmost / rightmost fix-ups, and in the fix-up loop the red-sibling arm, the inner rotation and most of the mirror (right-child) arm. Each was checked against the asm by hand (the layout is libstdc++'s erase-and-rebalance).
void func_08B5F444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t z = ctx.gpr[4];
    const std::uint32_t header = ctx.gpr[5];
    const std::uint32_t root_ref = header + 4u;
    const std::uint32_t leftmost_ref = header + 8u;
    const std::uint32_t rightmost_ref = header + 0xCu;
    auto parent_of = [&](std::uint32_t node) { return word(rt, node + 4u); };
    auto left_of = [&](std::uint32_t node) { return word(rt, node + 8u); };
    auto right_of = [&](std::uint32_t node) { return word(rt, node + 0xCu); };
    auto rotate_left = [&](std::uint32_t node) { (void)call_set(rt, ctx, 0x08B5F1F8u, {node, root_ref}); };
    auto rotate_right = [&](std::uint32_t node) { (void)call_set(rt, ctx, 0x08B5F258u, {node, root_ref}); };
    std::uint32_t y = z;
    std::uint32_t x;
    std::uint32_t x_parent;
    const std::uint32_t z_left = left_of(z);
    if (z_left == 0u) {
        x = right_of(z);
    } else if (right_of(z) == 0u) {
        x = z_left;
    } else {
        y = right_of(z);
        while (left_of(y) != 0u) y = left_of(y);
        x = right_of(y);
    }
    if (y != z) {
        const std::uint32_t z_right = right_of(z);
        put(rt, z_left + 4u, y);
        put(rt, y + 8u, z_left);
        if (y != z_right) {
            x_parent = parent_of(y);
            if (x != 0u) put(rt, x + 4u, x_parent);
            const std::uint32_t y_parent = parent_of(y);
            put(rt, y + 0xCu, z_right);
            const std::uint32_t z_right_again = right_of(z);
            put(rt, y_parent + 8u, x);
            put(rt, z_right_again + 4u, y);
        } else {
            x_parent = y;
        }
        const std::uint32_t z_parent = parent_of(z);
        if (word(rt, root_ref) == z) {
            put(rt, root_ref, y);
        } else if (left_of(z_parent) == z) {
            put(rt, z_parent + 8u, y);
        } else {
            put(rt, z_parent + 0xCu, y);
        }
        const std::uint32_t y_colour = word(rt, y);
        const std::uint32_t z_colour = word(rt, z);
        put(rt, y + 4u, z_parent);
        put(rt, y, z_colour);
        put(rt, z, y_colour);
        y = z;
    } else {
        x_parent = parent_of(y);
        if (x != 0u) put(rt, x + 4u, x_parent);
        if (word(rt, root_ref) == z) {
            put(rt, root_ref, x);
        } else {
            const std::uint32_t z_parent = parent_of(z);
            if (left_of(z_parent) == z) put(rt, z_parent + 8u, x); else put(rt, z_parent + 0xCu, x);
        }
        if (word(rt, leftmost_ref) == z) {
            if (right_of(z) == 0u) {
                put(rt, leftmost_ref, parent_of(z));
            } else {
                std::uint32_t node = x;
                while (left_of(node) != 0u) node = left_of(node);
                put(rt, leftmost_ref, node);
            }
        }
        if (word(rt, rightmost_ref) == z) {
            if (left_of(z) == 0u) {
                put(rt, rightmost_ref, parent_of(z));
            } else {
                std::uint32_t node = x;
                while (right_of(node) != 0u) node = right_of(node);
                put(rt, rightmost_ref, node);
            }
        }
    }
    ctx.gpr[2] = y;
    if (word(rt, y) == 0u) return;  // the removed node was red
    for (;;) {
        if (word(rt, root_ref) == x) break;
        if (x != 0u && word(rt, x) != 1u) break;
        if (x == left_of(x_parent)) {
            std::uint32_t w = right_of(x_parent);
            if (word(rt, w) == 0u) {
                put(rt, w, 1u);
                put(rt, x_parent, 0u);
                rotate_left(x_parent);
                w = right_of(x_parent);
            }
            const std::uint32_t wl = left_of(w);
            const std::uint32_t wr = right_of(w);
            if ((wl == 0u || word(rt, wl) == 1u) && (wr == 0u || word(rt, wr) == 1u)) {
                put(rt, w, 0u);
                x = x_parent;
                x_parent = parent_of(x_parent);
                continue;
            }
            if (wr == 0u || word(rt, wr) == 1u) {
                put(rt, wl, 1u);
                put(rt, w, 0u);
                rotate_right(w);
                w = right_of(x_parent);
            }
            const std::uint32_t colour = word(rt, x_parent);
            const std::uint32_t far_nephew = right_of(w);
            put(rt, w, colour);
            put(rt, x_parent, 1u);
            if (far_nephew != 0u) put(rt, far_nephew, 1u);
            rotate_left(x_parent);
            break;
        }
        std::uint32_t w = left_of(x_parent);
        if (word(rt, w) == 0u) {
            put(rt, w, 1u);
            put(rt, x_parent, 0u);
            rotate_right(x_parent);
            w = left_of(x_parent);
        }
        const std::uint32_t wr = right_of(w);
        const std::uint32_t wl = left_of(w);
        if ((wr == 0u || word(rt, wr) == 1u) && (wl == 0u || word(rt, wl) == 1u)) {
            put(rt, w, 0u);
            x = x_parent;
            x_parent = parent_of(x_parent);
            continue;
        }
        if (wl == 0u || word(rt, wl) == 1u) {
            put(rt, wr, 1u);
            put(rt, w, 0u);
            rotate_left(w);
            w = left_of(x_parent);
        }
        const std::uint32_t colour = word(rt, x_parent);
        const std::uint32_t far_nephew = left_of(w);
        put(rt, w, colour);
        put(rt, x_parent, 1u);
        if (far_nephew != 0u) put(rt, far_nephew, 1u);
        rotate_right(x_parent);
        break;
    }
    if (x != 0u) put(rt, x, 1u);
    ctx.gpr[2] = y;
}

// func_08B4B864(a0, a1): the tail jump func_08B4B7B4(a0, a1, a0) (a2 = a0 in the delay slot); the float registers pass through.
void func_08B4B864(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = ctx.gpr[4];
    tail_call(ctx, 0x08B4B7B4u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]});
}

// func_08B77BF4(a0): the tail jump func_08B7DC1C(a0, 0) (a1 = 0 in the delay slot).
void func_08B77BF4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = 0u;
    tail_call(ctx, 0x08B7DC1Cu, {ctx.gpr[4], 0u});
}

// func_08B4BAA8(a0, a1): the tail jump func_08B4BA68(a0, a1, a1) (a2 = a1 in the delay slot).
void func_08B4BAA8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = ctx.gpr[5];
    tail_call(ctx, 0x08B4BA68u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]});
}

// func_08C31304(a0, a1): the tail jump func_08AA195C(a0, sign-extended low byte of a1).
void func_08C31304(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = sext8(ctx.gpr[5]);
    tail_call(ctx, 0x08AA195Cu, {ctx.gpr[4], ctx.gpr[5]});
}

// func_08C3131C(a0, a1): the tail jump func_08AA19DC(a0, sign-extended low halfword of a1).
void func_08C3131C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = sext16(ctx.gpr[5]);
    tail_call(ctx, 0x08AA19DCu, {ctx.gpr[4], ctx.gpr[5]});
}

// func_08B52814(a0, a1, a2): v0 = a2.
void func_08B52814(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[6]; }

// func_0881AF28(): v0 = 0x1E.
void func_0881AF28(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x1Eu; }

// func_08BBC08C(pair = a0): v0 = pair[0], v1 = pair[1].
void func_08BBC08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 4u);
}

// func_08AD0974(): the tail jump func_08B2E98C() with the registers untouched.
void func_08AD0974(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B2E98Cu, {}); }

// func_0892C8B8(a0): the tail jump func_08B20968(0x08CF2958, a0) (a1 = a0 in the first instruction).
void func_0892C8B8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argument = ctx.gpr[4];
    ctx.gpr[5] = argument;
    ctx.gpr[4] = 0x08CF2958u;
    tail_call(ctx, 0x08B20968u, {0x08CF2958u, argument});
}

// func_08860058(object = a0): a0 = object[0]; the tail jump through slot 0x1C of its vtable (t9 = [[a0]+0x1C], v0 = the vtable).
void func_08860058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = word(rt, ctx.gpr[4]);
    const std::uint32_t table = word(rt, target);
    const std::uint32_t function = word(rt, table + 0x1Cu);
    ctx.gpr[4] = target;
    ctx.gpr[2] = table;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {target});
}

// func_0885E794(object = a0) / func_0885E7B8(object = a0): v1 = object[0], v0 = v1[0x3F98] (a0 = v0), a1 = the flag (1 / 0, set first): when v0 is zero the function returns (v0 = 0), otherwise the tail jump func_08AB8858(v0, flag).
inline void forward_flag_to_child(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = block;
    const std::uint32_t child = word(rt, block + 0x3F98u);
    ctx.gpr[2] = child;
    ctx.gpr[4] = child;
    if (child == 0u) return;
    tail_call(ctx, 0x08AB8858u, {child, ctx.gpr[5]});
}
// Branches the routes do not take (checked against the asm by hand): object[0][0x3F98] == 0 (the return with v0 = 0) - the shared helper forward_flag_to_child, also func_0885E7B8.
void func_0885E794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = 1u;
    forward_flag_to_child(rt, ctx);
}
void func_0885E7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = 0u;
    forward_flag_to_child(rt, ctx);
}

// func_0885E9EC(object = a0) / func_0885E9F8(object = a0): v0 = object[0]; the tail jump func_088DAD14 / func_088DAD54(v0[0x5AC]).
void func_0885E9EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = block;
    tail_call(ctx, 0x088DAD14u, {word(rt, block + 0x5ACu)});
}
void func_0885E9F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = block;
    tail_call(ctx, 0x088DAD54u, {word(rt, block + 0x5ACu)});
}

// func_08ABCC1C(object = a0): a1 = a0; with a0 and then object[0] not zero the tail jump through slot 0x28 of [object[0]]'s vtable with a0 = object[0], a1 = object (t9 = the slot, v0 = the vtable); a null object or a null object[0] returns (v0 untouched for a null a0, 0 for a null object[0]).
void func_08ABCC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    ctx.gpr[5] = object;
    if (object == 0u) return;
    const std::uint32_t inner = word(rt, object);
    ctx.gpr[2] = inner;
    ctx.gpr[4] = inner;
    if (inner == 0u) return;
    const std::uint32_t table = word(rt, inner);
    const std::uint32_t function = word(rt, table + 0x28u);
    ctx.gpr[2] = table;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {inner, object});
}

// func_08ABC9D0(object = a0, a1, a2, a3): v1 = a1, t1 = a2, t0 = a3 & 0xFF; a null object returns 0. Otherwise inner = object[0] (a0 = inner, a1 = object, a2 = v1, a3 = t1); a null inner returns 0, else the tail jump through slot 0x30 of inner's vtable (t9, v0 = the vtable).
void func_08ABC9D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[7] & 0xFFu;
    ctx.gpr[3] = second;
    ctx.gpr[9] = third;
    ctx.gpr[8] = flag;
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t inner = word(rt, object);
    ctx.gpr[5] = object;
    ctx.gpr[6] = second;
    ctx.gpr[4] = inner;
    ctx.gpr[7] = third;  // the delay slot of the beqz: both paths
    if (inner == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t table = word(rt, inner);
    const std::uint32_t function = word(rt, table + 0x30u);
    ctx.gpr[2] = table;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {inner, object, second, third, flag});
}

// func_08B6E268(a0, a1): the pair (a0, a1 with bit 31 cleared): v0 = a0, v1 = a3 = a1 & 0x7FFFFFFF.
void func_08B6E268(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[7] = ctx.gpr[5] & 0x7FFFFFFFu;
    ctx.gpr[2] = ctx.gpr[4];
    ctx.gpr[3] = ctx.gpr[7];
}

// func_08B6E4FC(a0, a1): a0' = ((a0 | -a0) >> 31) | (a1 & 0x7FFFFFFF) (logical shift); v0 = (0x7FF00000 - a0') >> 31 (logical). a0 / a1 end as a0' / a1 & 0x7FFFFFFF.
void func_08B6E4FC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[4];
    const std::uint32_t folded = (-low | low) >> 31;
    const std::uint32_t high = ctx.gpr[5] & 0x7FFFFFFFu;
    const std::uint32_t combined = folded | high;
    ctx.gpr[2] = (0x7FF00000u - combined) >> 31;
    ctx.gpr[4] = combined;
    ctx.gpr[5] = high;
}

// func_08B93AE8(tree = a0, key_ref = a1): the lower-bound walk of a tree whose header (a0 + 4) holds the root at +4 (a0[8]) and whose nodes hold the key at +0x10 and the children at +0xC / +8: the node is the candidate (a0) whenever its key is not below *a1 (go left), else go right.
// v0 = the last candidate (the header a0 + 4 when none).
// Branches the routes do not take (checked against the asm by hand): the whole walk: the root pointer was zero in every compared case (6), so only the empty-tree return ran; unregistered for that reason.
void func_08B93AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t node = word(rt, ctx.gpr[4] + 8u);
    std::uint32_t candidate = ctx.gpr[4] + 4u;
    ctx.gpr[4] = candidate;
    if (node != 0u) {
        const std::uint32_t key = word(rt, ctx.gpr[5]);
        ctx.gpr[5] = key;
        for (;;) {
            std::uint32_t next;
            if (signed_of(word(rt, node + 0x10u)) < signed_of(key)) {
                next = word(rt, node + 0xCu);
            } else {
                next = word(rt, node + 8u);
                candidate = node;
            }
            ctx.gpr[4] = candidate;
            ctx.gpr[2] = next;
            node = next;
            if (next == 0u) break;
        }
    }
    ctx.gpr[2] = candidate;
}

// func_08B79998(s = a0, c = a1 & 0xFF, n = a2): the pointer to the first byte equal to c among the n bytes at s, or 0 (the original scans a word at a time when s is aligned and n >= 4; a word holding the byte is then scanned bytewise, so the result is the first match either way).
void func_08B79998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t start = ctx.gpr[4];
    const std::uint32_t wanted = ctx.gpr[5] & 0xFFu;
    const std::uint32_t count = ctx.gpr[6];
    ctx.gpr[5] = wanted;
    for (std::uint32_t i = 0u; i < count; ++i) {
        if (byte_u(rt, start + i) == wanted) {
            ctx.gpr[2] = start + i;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

// func_08B7C598(s1 = a0, s2 = a1, n = a2): a bounded case-insensitive compare of two byte strings through the table at 0x08C96BD9 (bit 0 = upper case; the first pass indexes it with the SIGN-extended byte, the last with the zero-extended one): at most n bytes; stops at the first pair whose lowered
// (signed) values differ, at the byte n - 1, or when the byte of s1 is zero or the byte of s2 is zero; v0 = lowered(last byte of s1) - lowered(last byte of s2) (zero-extended). n == 0 returns v0 = 0.
// Branches the routes do not take (checked against the asm by hand): n == 0, the exit at the last of n bytes, the exits on a zero byte of either string, and the upper-case adjustment (+0x20): every compared case ended at the first pair whose lowered bytes differ and with no upper-case letter in the (sign-extended) first pass.
void func_08B7C598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kTable = 0x08C96BD9u;
    std::uint32_t first = ctx.gpr[4];
    std::uint32_t second = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    if (count == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    auto lower = [&](std::uint32_t c) { return (byte_u(rt, kTable + c) & 1u) != 0u ? c + 0x20u : c; };
    std::uint32_t c1 = 0u;
    std::uint32_t c2 = 0u;
    for (std::uint32_t index = 0u;; ++index) {
        c1 = byte_u(rt, first++);
        c2 = byte_u(rt, second++);
        const std::uint32_t s1 = sext8(c1);
        const std::uint32_t s2 = sext8(c2);
        if (lower(s1) != lower(s2)) break;
        if (index == count - 1u) break;
        if (s1 == 0u) break;
        if (s2 == 0u) break;
    }
    ctx.gpr[2] = lower(c1) - lower(c2);
}

// The halfword search of func_08C46E38 / func_08BB890C (the original unrolls by four and finishes with the 3/2/1-element tail): the pointer to the first halfword in [first, first + 2 * ((last - first) >> 1)) whose sign-extended value equals the word at *value_ref, or last; an odd byte distance leaves the odd byte unread.
inline void find_halfword(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    const std::int32_t halves = signed_of(last - first) >> 1;
    ctx.gpr[2] = last;
    if (halves > 0) {
        const std::uint32_t value = word(rt, ctx.gpr[6]);
        for (std::int32_t i = 0; i < halves; ++i) {
            const std::uint32_t address = first + 2u * static_cast<std::uint32_t>(i);
            if (sext16(half_u(rt, address)) == value) {
                ctx.gpr[2] = address;
                return;
            }
        }
    }
}
// func_08C46E38(first = a0, last = a1, value_ref = a2): find_halfword.
void func_08C46E38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_halfword(rt, ctx); }

// func_08B2BF20(owner = a0, node = a1): inserts `node` into the singly linked list whose head is owner[0x38] and whose nodes hold their next pointer at +4, ordered by address: the head when node < head (or the list is empty), else after the last element that is not above node. node[4] = the following element; the previous one's next (or the head) = node.
// Then the neighbours are merged by func_08B2BEE0(owner, previous, node) (the head case treats node as that answer) and, when its answer is not zero, the tail jump func_08B2BEE0(owner, answer, following); v0 = 0 when the answer is zero.
// Branches the routes do not take (checked against the asm by hand): walking on past a node that is not above the new one (following = ahead): the insertion always happened at the first comparison.
void func_08B2BF20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    ctx.gpr[6] = node;
    std::uint32_t following = word(rt, owner + 0x38u);
    std::uint32_t previous = 0u;
    if (following != 0u && !(node < following)) {
        std::uint32_t ahead = word(rt, following + 4u);
        for (;;) {
            if (ahead == 0u) {
                previous = following;
                following = 0u;
                break;
            }
            if (node < ahead) {
                previous = following;
                following = ahead;
                break;
            }
            following = ahead;
            ahead = word(rt, ahead + 4u);
        }
    }
    put(rt, node + 4u, following);  // the delay slot of the bnez on `previous`
    std::uint32_t answer;
    if (previous == 0u) {
        answer = node;
        put(rt, owner + 0x38u, node);  // the delay slot of the j
        ctx.gpr[2] = answer;
    } else {
        put(rt, previous + 4u, node);
        ctx.gpr[5] = previous;
        answer = call_set(rt, ctx, 0x08B2BEE0u, {owner, previous, node});
    }
    if (answer == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08B2BEE0u, {owner, answer, following});
}

// func_0891C35C(object = a0): object[0] = 0x08CC5CA0 (a vtable), object[4] = 0, three embedded members at +0x2C / +0x58 / +0x84 are constructed with func_089263EC(member) and get the vtable word 0x08CC6178 stored (+0x2C and +0x58 in the delay slots of the calls, +0x84 after the third), then func_08926488(member, name, 1) with the names 0x08CDDBB8 / 0x08CDDBD0 / 0x08CDDBB8; finally the tail jump func_0891AA34(object).
void func_0891C35C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t member_a = object + 0x2Cu;
    const std::uint32_t member_b = object + 0x58u;
    const std::uint32_t member_c = object + 0x84u;
    put(rt, object, 0x08CC5CA0u);
    put(rt, object + 4u, 0u);  // the delay slot of the first jal
    (void)call_set(rt, ctx, 0x089263ECu, {member_a});
    put(rt, member_a, 0x08CC6178u);
    (void)call_set(rt, ctx, 0x089263ECu, {member_b});
    put(rt, member_b, 0x08CC6178u);
    (void)call_set(rt, ctx, 0x089263ECu, {member_c});
    put(rt, member_c, 0x08CC6178u);
    (void)call_set(rt, ctx, 0x08926488u, {member_a, 0x08CDDBB8u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {member_b, 0x08CDDBD0u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {member_c, 0x08CDDBB8u, 1u});
    tail_call(ctx, 0x0891AA34u, {object});
}

// func_08AD337C(table = a0, name = a1): table[0] points at count = table[4] entries of 0x20 bytes whose name byte is at +0x10. When func_08AD3310(table, name) answers non-zero v0 = 0. Otherwise the first entry with an empty name byte gets the name
// (func_08B7C830(entry + 0x10, name, 0xF), byte +0x1F = 0) and v0 = the entry; no free entry (or a count <= 0) gives 0.
// Branches the routes do not take (checked against the asm by hand): func_08AD3310 answering non-zero, a count <= 0 and a table without a free entry.
void func_08AD337C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = ctx.gpr[4];
    const std::uint32_t name = ctx.gpr[5];
    const std::uint32_t known = call_set(rt, ctx, 0x08AD3310u, {table, name});
    ctx.gpr[2] = 0u;  // the delay slot of the bnez: both paths
    if (known != 0u) return;
    const std::int32_t count = signed_of(word(rt, table + 4u));
    if (count <= 0) return;
    const std::uint32_t base = word(rt, table);
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t offset = 0x20u * static_cast<std::uint32_t>(i);
        if (sext8(byte_u(rt, base + offset + 0x10u)) != 0u) continue;
        (void)call_set(rt, ctx, 0x08B7C830u, {base + offset + 0x10u, name, 0xFu});
        put8(rt, word(rt, table) + offset + 0x1Fu, 0u);
        ctx.gpr[2] = offset + word(rt, table);
        return;
    }
}

// func_08C46D74(object = a0, ...): a variadic-style wrapper that spills a2..t3 to its own (dead) frame and returns func_08C46D6C(object) (the sign-extended halfword at object + 0xEE).
void func_08C46D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)call_set(rt, ctx, 0x08C46D6Cu, {ctx.gpr[4]}); }

// func_08B8E380(): only spills a1..t3 to its own frame (a variadic stub); nothing else changes.
void func_08B8E380(psprecomp::Runtime &, psprecomp::AllegrexContext &) {}

// func_08BB890C(first = a0, last = a1, value_ref = a2): the same search as func_08C46E38 (a second instance of the template).
void func_08BB890C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_halfword(rt, ctx); }

// func_08903958(object = a0): delta = object - object[4]; when it is zero returns (v0 = the old object[4]). Otherwise object[4] = object and each non-zero pointer field at +0x58 .. +0x84 (step 4), +0x8C, +0x90, +0x9C gets + delta;
// then func_088FB578(object[0x58] + 8 * i, delta) for i < halfword[0x38], func_088FB560(object[0x7C] + 16 * i, delta) for i < halfword[0x4A], func_088FB970(object[0x9C] + 32 * i, delta) for i < halfword[0x54]. v0 = 0.
// Branches the routes do not take (checked against the asm by hand): delta == 0 (the early return) and zero-valued pointer fields; every compared case rebased all fifteen fields.
void func_08903958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t old_base = word(rt, object + 4u);
    ctx.gpr[2] = old_base;
    const std::uint32_t delta = object - old_base;
    if (delta == 0u) return;
    put(rt, object + 4u, object);
    static constexpr std::uint32_t kFields[] = {0x58u, 0x5Cu, 0x60u, 0x64u, 0x68u, 0x6Cu, 0x70u, 0x74u, 0x78u, 0x7Cu, 0x80u, 0x84u, 0x8Cu, 0x90u, 0x9Cu};
    for (const std::uint32_t field : kFields) {
        const std::uint32_t value = word(rt, object + field);
        if (value != 0u) put(rt, object + field, delta + value);
    }
    for (std::uint32_t i = 0u; i < half_u(rt, object + 0x38u); ++i) (void)call_set(rt, ctx, 0x088FB578u, {word(rt, object + 0x58u) + (i << 3), delta});
    for (std::uint32_t i = 0u; i < half_u(rt, object + 0x4Au); ++i) (void)call_set(rt, ctx, 0x088FB560u, {word(rt, object + 0x7Cu) + (i << 4), delta});
    for (std::uint32_t i = 0u; i < half_u(rt, object + 0x54u); ++i) (void)call_set(rt, ctx, 0x088FB970u, {word(rt, object + 0x9Cu) + (i << 5), delta});
    ctx.gpr[2] = 0u;
}

// func_0894DB6C(a0, a1, dest = a2): four turns i = 0..3: s = func_0894CF40(a0, a1) + 0x1E70 + 0x38 * i, d = func_0894CF98(a0, a1) + 0x7C0 + 0x30 * i; the three words at s go to d, the 0x20 bytes at s + 0xC to d + 0xC (two turns of four words) and the word at s + 0x2C to d + 0x2C.
// Then 0x990 bytes from func_0894CF98(a0, a1) go to dest in 16-byte turns (four loads, then four stores; swl/swr stores, i.e. byte-exact unaligned ones, when source or dest is not word aligned). v0 = the first word of the last 16-byte block read.
// Branches the routes do not take (checked against the asm by hand): the unaligned arm of the final copy (swl/swr stores).
void func_0894DB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t dest = ctx.gpr[6];
    std::uint32_t step_a = 0x7C0u;
    std::uint32_t step_b = 0x1E70u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t source = call_set(rt, ctx, 0x0894CF40u, {owner, key}) + step_b;
        const std::uint32_t target = call_set(rt, ctx, 0x0894CF98u, {owner, key}) + step_a;
        put(rt, target, word(rt, source));
        put(rt, target + 4u, word(rt, source + 4u));
        put(rt, target + 8u, word(rt, source + 8u));
        for (std::uint32_t offset = 0xCu; offset < 0x2Cu; offset += 0x10u) {
            const std::uint32_t w0 = word(rt, source + offset), w1 = word(rt, source + offset + 4u), w2 = word(rt, source + offset + 8u), w3 = word(rt, source + offset + 0xCu);
            put(rt, target + offset, w0);
            put(rt, target + offset + 4u, w1);
            put(rt, target + offset + 8u, w2);
            put(rt, target + offset + 0xCu, w3);
        }
        put(rt, target + 0x2Cu, word(rt, source + 0x2Cu));
        step_b += 0x38u;
        step_a += 0x30u;
    }
    const std::uint32_t source = call_set(rt, ctx, 0x0894CF98u, {owner, key});
    const bool unaligned = ((source | dest) & 3u) != 0u;
    std::uint32_t last_first_word = 0u;
    for (std::uint32_t offset = 0u; offset < 0x990u; offset += 0x10u) {
        const std::uint32_t w0 = word(rt, source + offset), w1 = word(rt, source + offset + 4u), w2 = word(rt, source + offset + 8u), w3 = word(rt, source + offset + 0xCu);
        last_first_word = w0;
        const std::uint32_t words[4] = {w0, w1, w2, w3};
        for (unsigned k = 0u; k < 4u; ++k) {
            if (unaligned) {
                for (unsigned b = 0u; b < 4u; ++b) rt.memory().store8(dest + offset + 4u * k + b, static_cast<std::uint8_t>(words[k] >> (8u * b)));
            } else {
                put(rt, dest + offset + 4u * k, words[k]);
            }
        }
    }
    ctx.gpr[2] = last_first_word;
}

// ---- end of functions ----

}  // namespace
}  // namespace hot_batch_101
}  // namespace gt::game

#define HOT101(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_101::name, gt::ReturnKind::kind)
#define HOT101N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_101::name, gt::ReturnKind::kind)

// HOT101REGS
HOT101(0x088693C4u, func_088693C4, Word);
// HOT101(0x08870448u, func_08870448, Word);  // HOT101: writes memory but no compared case changed it (6 cases: the floats stored were already there)
HOT101(0x0887BD88u, func_0887BD88, Word);
HOT101(0x088E43C0u, func_088E43C0, Word);
// HOT101(0x0891FEF0u, func_0891FEF0, Word);  // HOT101: writes memory but no compared case changed it (5 cases: the word was already -1)
HOT101(0x08965DDCu, func_08965DDC, Word);
// HOT101(0x0885E788u, func_0885E788, Word);  // HOT101: writes memory but no compared case changed it (4 cases: the byte was already zero)
HOT101(0x0885E778u, func_0885E778, Word);
HOT101(0x08B54F24u, func_08B54F24, Word);
// HOT101(0x08BD2D24u, func_08BD2D24, Word);  // HOT101: writes memory but no compared case changed it (1 case)
// HOT101(0x08AC3308u, func_08AC3308, Word);  // HOT101: compared 0 in every run: every case is not comparable (the callee reaches sce* imports that block or switch threads)
// HOT101(0x08AD08A4u, func_08AD08A4, Word);  // HOT101: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
HOT101(0x0885E91Cu, func_0885E91C, Word);
HOT101(0x08A8798Cu, func_08A8798C, Word);
HOT101(0x088D4824u, func_088D4824, Word);
// HOT101(0x0892C034u, func_0892C034, Word);  // HOT101: writes memory but no compared case changed it (8 cases: every block pointer was already zero)
HOT101(0x0892BF7Cu, func_0892BF7C, Word);
HOT101(0x0888AB4Cu, func_0888AB4C, Word);
HOT101(0x0896A4E0u, func_0896A4E0, Word);
HOT101(0x089311ECu, func_089311EC, Word);
HOT101(0x08930458u, func_08930458, Word);
HOT101(0x08935B88u, func_08935B88, Word);
HOT101(0x08930290u, func_08930290, Word);
HOT101(0x08897470u, func_08897470, Word);
HOT101(0x088D176Cu, func_088D176C, Word);
HOT101(0x08909CB4u, func_08909CB4, Word);
HOT101(0x0892EFB4u, func_0892EFB4, Word);
HOT101(0x08BC264Cu, func_08BC264C, Word);
HOT101(0x08B5F444u, func_08B5F444, Word);
HOT101(0x08B4B864u, func_08B4B864, Word);
HOT101(0x08B77BF4u, func_08B77BF4, DoubleWord);
HOT101(0x08B4BAA8u, func_08B4BAA8, Word);
HOT101(0x08C31304u, func_08C31304, Word);
HOT101(0x08C3131Cu, func_08C3131C, Word);
HOT101(0x08B52814u, func_08B52814, Word);
HOT101(0x0881AF28u, func_0881AF28, Word);
HOT101(0x08BBC08Cu, func_08BBC08C, DoubleWord);
// HOT101(0x08AD0974u, func_08AD0974, Word);  // HOT101: compared 0 in every run: every case is not comparable (the tail callee reaches sce* imports that block or switch threads)
HOT101(0x0892C8B8u, func_0892C8B8, Word);
HOT101(0x08860058u, func_08860058, Word);
HOT101(0x0885E794u, func_0885E794, Word);
HOT101(0x0885E7B8u, func_0885E7B8, Word);
HOT101(0x0885E9ECu, func_0885E9EC, Word);
HOT101(0x0885E9F8u, func_0885E9F8, Word);
// HOT101(0x08ABCC1Cu, func_08ABCC1C, Word);  // HOT101: compared 0 in every run: every case is not comparable (the virtual callee reaches sce* imports that block or switch threads)
// HOT101(0x08ABC9D0u, func_08ABC9D0, Word);  // HOT101: compared 0 in every run: every case is not comparable (the virtual callee reaches sce* imports that block or switch threads)
HOT101(0x08B6E268u, func_08B6E268, DoubleWord);
HOT101(0x08B6E4FCu, func_08B6E4FC, Word);
// HOT101(0x08B93AE8u, func_08B93AE8, Word);  // HOT101: compared 6 times but the tree was empty in every case (the root pointer was zero): the walk never ran, so the compared cases prove nothing about it
HOT101(0x08B79998u, func_08B79998, Word);
HOT101(0x08B7C598u, func_08B7C598, Word);
HOT101(0x08C46E38u, func_08C46E38, Word);
HOT101(0x08B2BF20u, func_08B2BF20, Word);
HOT101(0x0891C35Cu, func_0891C35C, Word);
HOT101(0x08AD337Cu, func_08AD337C, Word);
HOT101(0x08C46D74u, func_08C46D74, Word);
HOT101(0x08B8E380u, func_08B8E380, Word);
HOT101(0x08BB890Cu, func_08BB890C, Word);
HOT101(0x08903958u, func_08903958, Word);
HOT101(0x0894DB6Cu, func_0894DB6C, Word);
