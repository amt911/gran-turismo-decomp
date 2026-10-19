// Hot batch 100: reached, unregistered functions of the batch-99 pool, probed again on main with batch 99 merged (`b100p`: `tools.family_members --write-probe-symbols FILE --max-insns 420`, 14,121 recompiled functions that no `src/` file defines or registers, then `tools.decomp_gate probe b100p --symbols FILE`; 638 of 20 or more instructions are reached by the five routes).
// Written from `tools.disasm` one function at a time. Role words in the comments describe access shapes; none is a game name.
//
// Written 39 (plus the setjmp func_08B7B2E4), registered 11: func_08A4D57C, func_08B5F2B8, func_088AA074, func_088FA224, func_08A9DD14, func_08B19F10, func_08B23A70, func_08C34270, func_08C401C4, func_08C40FA4, func_08C413DC. The others have a commented HOT100 line with the reason: ten reach sce* imports in every compared case (compared 0), eighteen were compared but only on
// their empty paths (count zero, empty range, null destination), so no compared case changed memory (a dense run of the race route for the hottest of them changed nothing either); func_08B7B2E4 is a setjmp, which the harness cannot compare (its saved return address is the case's sentinel).
//
// Left (read, not written): code that takes atomic reference counts with ll/sc (func_08BA9498, func_088B0520), the copy-on-write string append func_08BEED1C, a hinted tree insert (func_08BFEBAC), the 64-bit-time lock code (func_08B33E50, func_08B1C590), the functions whose callees reach sce* imports (func_08B83C84 and the other func_08B83xxx / func_08B84xxx stubs, func_08AEFC14, func_08AEFC74), VFPU code (func_08B53D2C, func_089A9064, func_08B498AC, ...) and the long iterators func_08AFFCF4 / func_088F05BC.
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
namespace hot_batch_100 {
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

// func_08A4D57C(dest = a0, text = a1): scans a zero-terminated string for '*' items. dest[0x20] / dest[0x24] start as 1.0f; each '*' is followed by a number (func_08A4CD7C(p, &n, p, 100) parses it into a stack word, default 100) and optionally ',' and a second number
// (default: the first); the two numbers times 0.01f (cvt.s.w then mul.s) become dest[0x20] and dest[0x24]; the scan goes on after the last number. Afterwards the text up to the first '*' (copied by func_08B79A88 into the stack buffer sp+8 and terminated there) goes to
// func_08A4CDD8(dest, buffer); text with no '*' goes to it whole; a prefix of 0x3F or more bytes is dropped and v0 = 0. a1 == 0 returns at once with v0 untouched.
// Branches the routes do not take (from `tools.decomp_gate coverage`, checked against the asm by hand): a1 == 0; EVERY '*' path (the loop only ever saw text without '*': `c != '*'` was never false, so the number parsing, the ',' pair, the 0.01f scale stores, the prefix copy through func_08B79A88 and the 0x3F-byte limit never ran) -
// weakest proof of the batch. The only compared path is: dest[0x20] = dest[0x24] = 1.0f, scan to the terminator, func_08A4CDD8(dest, text).
void func_08A4D57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    if (text == 0u) return;
    GuestFrame frame(ctx, 0x80u);
    const float scale = std::bit_cast<float>(0x3C23D70Au);
    put_float(rt, dest + 0x20u, 1.0f);
    put_float(rt, dest + 0x24u, 1.0f);
    std::uint32_t first_star = 0u;  // s0
    std::uint32_t p = text;         // v1
    for (;;) {
        const std::uint32_t c = sext8(byte_u(rt, p));
        const std::uint32_t next = p + 1u;  // a2
        if (c == 0u) break;
        if (c != 0x2Au) {
            p = next;
            continue;
        }
        put(rt, frame.at(0u), 100u);
        if (first_star == 0u) first_star = p;  // movz in the jal's delay slot
        std::uint32_t position = call_set(rt, ctx, 0x08A4CD7Cu, {next, frame.at(0u), next, 100u});
        std::uint32_t a3 = word(rt, frame.at(0u));
        put(rt, frame.at(4u), a3);
        if (sext8(byte_u(rt, position)) == 0x2Cu) {
            position = call_set(rt, ctx, 0x08A4CD7Cu, {position + 1u, frame.at(4u), position, a3});
            a3 = word(rt, frame.at(0u));
        }
        const float x = psp_mul(static_cast<float>(signed_of(a3)), scale);
        const float y = psp_mul(static_cast<float>(signed_of(word(rt, frame.at(4u)))), scale);
        put_float(rt, dest + 0x20u, x);
        put_float(rt, dest + 0x24u, y);
        p = position;
    }
    if (first_star == 0u) {
        (void)call_set(rt, ctx, 0x08A4CDD8u, {dest, text});
        return;
    }
    const std::uint32_t length = first_star - text;
    if (!(signed_of(length) < 0x3F)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t buffer = frame.at(8u);
    (void)call_set(rt, ctx, 0x08B79A88u, {buffer, text, length});
    put8(rt, buffer + length, 0u);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A4CDD8u, {dest, buffer});
}

// func_08B5F2B8(side = a0 & 0xFF, node = a1, parent = a2, header = a3): the insert-and-rebalance of a red-black tree whose header holds root / leftmost / rightmost at +4 / +8 / +0xC and whose nodes hold colour / parent / left / right at +0 / +4 / +8 / +0xC
// (colour 0 = red). The new node is linked under `parent` (right child when side == 0, else left; the header's leftmost / rightmost / root are updated), then the red-parent fix-ups run with the two rotations func_08B5F1F8 / func_08B5F258 (a0 = grandparent, a1 = &header[4]) until
// the node is the root or its parent is black. The root ends black; v0 = 1 on every path.
// Branches the routes do not take: none (all 13 branch outcomes of the coverage run were taken).
void func_08B5F2B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t side = ctx.gpr[4] & 0xFFu;
    const std::uint32_t node = ctx.gpr[5];
    const std::uint32_t parent = ctx.gpr[6];
    const std::uint32_t header = ctx.gpr[7];
    const std::uint32_t root_ref = header + 4u;
    put(rt, node + 4u, parent);
    put(rt, node + 8u, 0u);
    put(rt, node + 0xCu, 0u);
    put(rt, node, 0u);
    std::uint32_t root;  // a0
    if (side == 0u) {
        put(rt, parent + 0xCu, node);
        root = word(rt, header + 4u);  // the delay slot of the bne
        if (word(rt, header + 0xCu) == parent) put(rt, header + 0xCu, node);
    } else {
        put(rt, parent + 8u, node);  // the delay slot of the beq: both paths
        if (header == parent) {
            root = node;
            put(rt, parent + 0xCu, node);
            put(rt, parent + 4u, node);
        } else {
            const std::uint32_t leftmost = word(rt, header + 8u);
            root = word(rt, header + 4u);  // the delay slot of the beq below
            if (parent == leftmost) put(rt, header + 8u, node);
        }
    }
    std::uint32_t current = node;  // s2
    while (current != root) {
        std::uint32_t s0 = word(rt, current + 4u);
        if (word(rt, s0) != 0u) break;
        const std::uint32_t s1 = word(rt, s0 + 4u);
        const bool left = (s0 == word(rt, s1 + 8u));
        const std::uint32_t uncle = word(rt, s1 + (left ? 0xCu : 8u));
        if (uncle != 0u && word(rt, uncle) == 0u) {
            put(rt, s0, 1u);
            put(rt, uncle, 1u);
            put(rt, s1, 0u);
            current = s1;
            continue;
        }
        const std::uint32_t first_rotation = left ? 0x08B5F1F8u : 0x08B5F258u;
        const std::uint32_t second_rotation = left ? 0x08B5F258u : 0x08B5F1F8u;
        if (current == word(rt, s0 + (left ? 0xCu : 8u))) {
            (void)call_set(rt, ctx, first_rotation, {s0, root_ref});
            current = s0;
            s0 = word(rt, s0 + 4u);
        }
        put(rt, s0, 1u);
        put(rt, s1, 0u);  // the delay slot of the rotation's jal
        (void)call_set(rt, ctx, second_rotation, {s1, root_ref});
        root = word(rt, root_ref);
    }
    put(rt, root, 1u);
    ctx.gpr[2] = 1u;
}

// func_088AA074(self = a0, out = a1): when self[8] (an object) is zero, v0 = 0 and nothing else happens. Otherwise three float getters of the same shape (func_08955CC8 / func_08955DD0 / func_08955D4C, a0 = obj[0xC], a1 = self[4], f12 = obj[0x34]) give
// s = f21, c = f20 and f14; func_08B4C804(&q, -s, c, f14) builds a quaternion on the stack, func_08B4B864(&q, self[8] + 0x190) combines it, func_08B4A744(&q) returns its squared length, and func_08B4A6A0(&q, 1.0f / sqrt(len)) scales it; the four floats
// go to out[4], out[0], out[0xC], out[8] (in that order). v0 is the last call's.
// Float argument registers of the callees were checked by hand: the three getters read f12, func_08B4C804 f12 / f13 / f14, func_08B4A6A0 f12.
// Branches the routes do not take (checked against the asm by hand): self[8] == 0 (the early return with v0 = 0).
void func_088AA074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t obj = word(rt, self + 8u);
    ctx.gpr[2] = obj;
    if (obj == 0u) return;
    const std::uint32_t q = frame.at(0u);
    const float f21 = call_setf(rt, ctx, 0x08955CC8u, {word(rt, obj + 0xCu), word(rt, self + 4u)}, {float_at(rt, obj + 0x34u)}).f0;
    obj = word(rt, self + 8u);
    const float f20 = call_setf(rt, ctx, 0x08955DD0u, {word(rt, obj + 0xCu), word(rt, self + 4u)}, {float_at(rt, obj + 0x34u)}).f0;
    obj = word(rt, self + 8u);
    const float f14 = call_setf(rt, ctx, 0x08955D4Cu, {word(rt, obj + 0xCu), word(rt, self + 4u)}, {float_at(rt, obj + 0x34u)}).f0;
    (void)call_setf(rt, ctx, 0x08B4C804u, {q}, {-f21, f20, f14});
    (void)call_set(rt, ctx, 0x08B4B864u, {q, word(rt, self + 8u) + 0x190u});
    const float length = call_setf(rt, ctx, 0x08B4A744u, {q}, {}).f0;
    const float scale = 1.0f / std::sqrt(length);
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {q}, {scale});
    const std::uint32_t w1 = word(rt, q + 4u);
    const std::uint32_t w2 = word(rt, q + 8u);
    const std::uint32_t w3 = word(rt, q + 0xCu);
    const std::uint32_t w0 = word(rt, q);
    put(rt, out + 4u, w1);
    put(rt, out, w0);
    put(rt, out + 0xCu, w3);
    put(rt, out + 8u, w2);
}

// func_088FA224(dest = a0, src = a1): packs seven answers of src's virtual slots into the bit fields of dest[0]: slot 0x8 -> bit 0, 0x14 -> bit 1, 0x18 -> bit 2, 0x1C -> bit 3, 0x20 -> bit 4 (one bit each), slot 0xC -> bits 5-12, slot 0x10 -> bits 13-20 (eight bits each).
// dest[0] is re-read after every call (the other fields stay). The first call is the `lw v1, 0(a1); lw v0, 8(v1); jalr v0` shape (v1 = vtable), the others load the vtable into a1 and the function into v0; v0 is the last answer.
// Branches the routes do not take: none (the coverage run took every outcome).
void func_088FA224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    std::uint32_t answer = call_virtual(rt, ctx, src, 0x8u, {src});
    std::uint32_t packed = insert_bits(word(rt, dest), answer, 0u, 1u);
    put(rt, dest, packed);
    struct Field {
        std::uint32_t slot;
        unsigned pos;
        unsigned size;
    };
    static constexpr std::array<Field, 6> kFields{{{0x14u, 1u, 1u}, {0x18u, 2u, 1u}, {0x1Cu, 3u, 1u}, {0x20u, 4u, 1u}, {0xCu, 5u, 8u}, {0x10u, 13u, 8u}}};
    for (const Field &field : kFields) {
        const std::uint32_t table = word(rt, src);
        const std::uint32_t function = word(rt, table + field.slot);
        ctx.gpr[5] = table;
        ctx.gpr[3] = packed;
        ctx.gpr[2] = function;
        answer = call_set(rt, ctx, function, {src});
        packed = insert_bits(word(rt, dest), answer, field.pos, field.size);
        put(rt, dest, packed);
    }
    ctx.gpr[3] = packed;
}

// func_08AF2D20(a0 = holder): p = func_08AF296C(a0) (holder[0] + 0x40) points at three words; they are moved into a stack triple (the first into sp+4, the others swapped through sp+8 / sp+0xC, which start as zero), p's words become 0, the stack pair is handed to func_08C3F8BC(sp[4], sp[8], sp) and sp[4] (the old first
// word) is released with func_08B35254 when it is not zero. v0 = that call's, or sp[4] (zero) when there was none.
void func_08AF2D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t p = call_set(rt, ctx, 0x08AF296Cu, {a0});  // the callee reads a0 (the caller's, untouched)
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(4u), word(rt, p));
    put(rt, p, 0u);
    const std::uint32_t old8 = word(rt, frame.at(8u));
    put(rt, frame.at(8u), word(rt, p + 4u));
    put(rt, p + 4u, old8);
    const std::uint32_t oldc = word(rt, frame.at(0xCu));
    put(rt, frame.at(0xCu), word(rt, p + 8u));
    put(rt, p + 8u, oldc);
    ctx.gpr[6] = frame.at(0u);
    (void)call_set(rt, ctx, 0x08C3F8BCu, {word(rt, frame.at(4u)), word(rt, frame.at(8u)), frame.at(0u)});
    const std::uint32_t first = word(rt, frame.at(4u));
    ctx.gpr[2] = first;
    ctx.gpr[4] = first;
    if (first != 0u) (void)call_set(rt, ctx, 0x08B35254u, {first});
}

// func_08A9DD14(object = a0): the singleton getter func_08A9DCB0(0) is called three times (twice more when the byte at +0x7C of the second answer is zero): its first answer is stored at sp[0] and func_08A5BAB8(object, &sp[0]) runs; if the second answer's byte +0x7C is
// zero, func_08A9C340(func_08A9DCB0(0)) follows. v0 is the last call's (or the second answer, v1 = the byte).
// Branches the routes do not take: none (the coverage run took every outcome).
void func_08A9DD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = call_set(rt, ctx, 0x08A9DCB0u, {0u});
    put(rt, frame.at(0u), first);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A5BAB8u, {object, frame.at(0u)});
    const std::uint32_t second = call_set(rt, ctx, 0x08A9DCB0u, {0u});
    const std::uint32_t flag = byte_u(rt, second + 0x7Cu);
    ctx.gpr[3] = flag;
    if (flag == 0u) {
        const std::uint32_t third = call_set(rt, ctx, 0x08A9DCB0u, {0u});
        (void)call_set(rt, ctx, 0x08A9C340u, {third});
    }
}

// func_08BE6D44(dest = a0, count = a1, src = a2): fills `count` 0x28-byte elements at dest (null dest is skipped but still advances) with the same element each time: [0] = the vtable 0x08CCE6E0, [4..0x24] = src[4..0x24] (src is not advanced); v0 = src[4], v1 = src[8] of the
// last copy, v0 = 0x08CD0000 (the lui in the first branch's delay slot) when nothing was copied. count == 0 returns at once.
void func_08BE6D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t src = ctx.gpr[6];
    ctx.gpr[2] = 0x08CD0000u;
    if (count == 0u) return;
    for (std::uint32_t done = 1u;; ++done) {
        if (dest != 0u) {
            const std::uint32_t v0 = word(rt, src + 4u);
            const std::uint32_t v1 = word(rt, src + 8u);
            put(rt, dest + 4u, v0);
            put(rt, dest + 8u, v1);
            for (std::uint32_t offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, dest + offset, word(rt, src + offset));
            put(rt, dest, 0x08CCE6E0u);
            put(rt, dest + 0x20u, word(rt, src + 0x20u));
            put(rt, dest + 0x24u, word(rt, src + 0x24u));
            ctx.gpr[2] = v0;
            ctx.gpr[3] = v1;
        }
        dest += 0x28u;
        if (count == done) break;
    }
}

// func_08BE6674(dest = a0, count = a1, src = a2): the same fill for 0x30-byte elements: [0] = the vtable 0x08CCDE00, [4..0x2C] = src[4..0x2C]; v0 = src[4], v1 = src[8] of the last copy, v0 = 0x08CD0000 when nothing was copied.
void func_08BE6674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t src = ctx.gpr[6];
    ctx.gpr[2] = 0x08CD0000u;
    if (count == 0u) return;
    for (std::uint32_t done = 1u;; ++done) {
        if (dest != 0u) {
            const std::uint32_t v0 = word(rt, src + 4u);
            const std::uint32_t v1 = word(rt, src + 8u);
            put(rt, dest + 4u, v0);
            put(rt, dest + 8u, v1);
            for (std::uint32_t offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, dest + offset, word(rt, src + offset));
            put(rt, dest, 0x08CCDE00u);
            for (std::uint32_t offset = 0x20u; offset <= 0x2Cu; offset += 4u) put(rt, dest + offset, word(rt, src + offset));
            ctx.gpr[2] = v0;
            ctx.gpr[3] = v1;
        }
        dest += 0x30u;
        if (count == done) break;
    }
}

// func_08BE834C(dest = a0, count = a1, src = a2): fills `count` 0x40-byte elements at dest (null dest skipped) with the same element: [4..0x1C] = src[4..0x1C], [0] = the vtable 0x08CCFC90 (stored in the delay slot of the first call), then func_089FA280(dest + 0x20, src + 0x20) and
// func_089FA280(dest + 0x30, src + 0x30) (each a 0x10-byte copy). v0 = the last call's; count == 0 returns at once with v0 untouched, otherwise v0 starts as 0x08CD0000.
void func_08BE834C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t src = ctx.gpr[6];
    if (count == 0u) return;
    ctx.gpr[2] = 0x08CD0000u;
    for (std::uint32_t done = 1u;; ++done) {
        if (dest != 0u) {
            const std::uint32_t v0 = word(rt, src + 4u);
            const std::uint32_t v1 = word(rt, src + 8u);
            put(rt, dest + 4u, v0);
            put(rt, dest + 8u, v1);
            for (std::uint32_t offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, dest + offset, word(rt, src + offset));
            ctx.gpr[2] = v0;
            ctx.gpr[3] = v1;
            put(rt, dest, 0x08CCFC90u);
            (void)call_set(rt, ctx, 0x089FA280u, {dest + 0x20u, src + 0x20u});
            (void)call_set(rt, ctx, 0x089FA280u, {dest + 0x30u, src + 0x30u});
        }
        dest += 0x40u;
        if (count == done) break;
    }
}

// func_08C2B76C(first = a0, last = a1, dest = a2): copy-constructs 12-byte elements [first, last) at dest (a null dest is skipped but advances): func_08A6972C(dest, src), dest[4] = src[4], func_08A76ABC(dest + 8, src + 8). v0 = the end of dest.
void func_08C2B76C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t src = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    std::uint32_t dest = ctx.gpr[6];
    if (src != last) {
        for (;;) {
            const std::uint32_t next = src + 0xCu;
            if (dest != 0u) {
                (void)call_set(rt, ctx, 0x08A6972Cu, {dest, src});
                put(rt, dest + 4u, word(rt, src + 4u));  // the delay slot of the second jal
                (void)call_set(rt, ctx, 0x08A76ABCu, {dest + 8u, src + 8u});
            }
            src = next;
            dest += 0xCu;
            if (last == next) break;
        }
    }
    ctx.gpr[2] = dest;
}

// func_08C401C4 / func_08C40FA4 / func_08C413DC share one body: the compared cases of the three (88 + a dense run of func_08C401C4: 367 cases, 8 changed memory) take the request == 0 answer and a few of the others; the coverage run lists the compare arms (c < b; request < b / c) as not taken in the five routes (checked against the asm by hand: they are the sltu pairs at 0x08C401F4-0x08C40220).
// Shared by func_08C401C4 / func_08C40FA4 / func_08C413DC (the same code three times; a0 = a block of four words and a size, a1 = &request): request = a1[0]; zero answers 0. Otherwise with b = a0[0xC] and c = a0[8]: when b == 0 and a0[4] == 0 the answer is a0[0] + request; else
// the answer is (c < b ? (request < b ? a0[0] : a0[4]) : (request < c ? a0[4] : a0[0])) + request (unsigned compares). a1[0] receives the answer, v0 = it.
inline void select_base_plus_request(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = ctx.gpr[4];
    const std::uint32_t request_ref = ctx.gpr[5];
    const std::uint32_t request = word(rt, request_ref);
    std::uint32_t answer = 0u;
    if (request != 0u) {
        const std::uint32_t b = word(rt, block + 0xCu);
        if (b == 0u && word(rt, block + 4u) == 0u) {
            answer = word(rt, block) + request;
        } else {
            const std::uint32_t c = word(rt, block + 8u);
            std::uint32_t base;
            if (c < b) {
                base = request < b ? word(rt, block) : word(rt, block + 4u);
            } else {
                base = request < c ? word(rt, block + 4u) : word(rt, block);
            }
            answer = base + request;
        }
    }
    ctx.gpr[2] = answer;
    put(rt, request_ref, answer);
}
void func_08C401C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { select_base_plus_request(rt, ctx); }
void func_08C40FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { select_base_plus_request(rt, ctx); }
void func_08C413DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { select_base_plus_request(rt, ctx); }

// func_08B7B2E4(buf = a0): saves the callee-saved registers like setjmp: s0-s7 at +0..0x1C, sp +0x20, fp +0x24, ra +0x28, f20-f31 at +0x2C..0x58 (raw float bits); v0 = 0.
void func_08B7B2E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buf = ctx.gpr[4];
    for (unsigned i = 0u; i < 8u; ++i) put(rt, buf + 4u * i, ctx.gpr[16u + i]);
    put(rt, buf + 0x20u, ctx.gpr[29]);
    put(rt, buf + 0x24u, ctx.gpr[30]);
    put(rt, buf + 0x28u, ctx.gpr[31]);
    for (unsigned i = 20u; i < 32u; ++i) put(rt, buf + 0x2Cu + 4u * (i - 20u), std::bit_cast<std::uint32_t>(ctx.fpr[i]));
    ctx.gpr[2] = 0u;
}

// Copies the 0x3C bytes at `from` to `to` (three 16-byte chunks and a 12-byte tail in the original; loads of a chunk come before its stores).
inline void copy_3c(psprecomp::Runtime &rt, std::uint32_t to, std::uint32_t from) {
    for (std::uint32_t chunk = 0u; chunk < 0x30u; chunk += 0x10u) {
        const std::uint32_t w0 = word(rt, from + chunk);
        const std::uint32_t w1 = word(rt, from + chunk + 4u);
        const std::uint32_t w2 = word(rt, from + chunk + 8u);
        const std::uint32_t w3 = word(rt, from + chunk + 0xCu);
        put(rt, to + chunk, w0);
        put(rt, to + chunk + 4u, w1);
        put(rt, to + chunk + 8u, w2);
        put(rt, to + chunk + 0xCu, w3);
    }
    const std::uint32_t t0 = word(rt, from + 0x38u);
    const std::uint32_t t1 = word(rt, from + 0x30u);
    const std::uint32_t t2 = word(rt, from + 0x34u);
    put(rt, to + 0x38u, t0);
    put(rt, to + 0x30u, t1);
    put(rt, to + 0x34u, t2);
}

// func_08B100D0(dest = a0, a1, a2, flag = a3 & 0xFF): a four-word stack header (zeros) and the object func_08B0F4E0(sp + 0x10, a1, a2, sp, flag) builds in the stack (0x3C bytes at sp + 0x10) are copied to dest; v0 = dest. a1 / a2 are handed on untouched.
void func_08B100D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    const std::uint32_t a2 = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t built = frame.at(0x10u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    (void)call_set(rt, ctx, 0x08B0F4E0u, {built, a1, a2, frame.at(0u), flag});
    copy_3c(rt, dest, built);
    ctx.gpr[2] = dest;
}

// func_08B10170(dest = a0, a1, a2, a3, flag = t0 & 0xFF): stores a2 / a3 at sp[0] / sp[4] and calls func_08B100D0(sp + 0x10, a1, sp, flag); the 0x3C-byte object it builds is copied to dest; v0 = dest.
void func_08B10170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t built = frame.at(0x10u);
    put(rt, frame.at(0u), ctx.gpr[6]);
    put(rt, frame.at(4u), ctx.gpr[7]);
    (void)call_set(rt, ctx, 0x08B100D0u, {built, a1, frame.at(0u), ctx.gpr[8] & 0xFFu});
    copy_3c(rt, dest, built);
    ctx.gpr[2] = dest;
}

// func_08B10204(dest = a0, a1, flag = a2 & 0xFF): the same with a zeroed pair at sp[0] / sp[4]: func_08B100D0(sp + 0x10, a1, sp, flag), the 0x3C-byte result copied to dest; v0 = dest.
void func_08B10204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t built = frame.at(0x10u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    (void)call_set(rt, ctx, 0x08B100D0u, {built, a1, frame.at(0u), flag});
    copy_3c(rt, dest, built);
    ctx.gpr[2] = dest;
}

// func_089C4C20(a0, a1, out = a2): out[0] = 0; func_08B10204(sp + 0x40, a1, 1) builds a 0x3C-byte object (copied to sp[0..0x3B]); when its first word is zero out[0] receives its word +0x10 and v0 = its word +0x20, otherwise v0 = 0.
void func_089C4C20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a1 = ctx.gpr[5];
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t built = frame.at(0x40u);
    put(rt, out, 0u);
    (void)call_set(rt, ctx, 0x08B10204u, {built, a1, 1u});
    copy_3c(rt, frame.at(0u), built);
    std::uint32_t result = 0u;
    if (word(rt, frame.at(0u)) == 0u) {
        const std::uint32_t second = word(rt, frame.at(0x10u));
        result = word(rt, frame.at(0x20u));
        put(rt, out, second);
    }
    ctx.gpr[2] = result;
}

// func_08AC03E0(obj = a0, a1, a2, flag = a3 & 0xFF): obj[0x1C] = func_08B1554C(obj[8][0x54]) when flag is set, otherwise func_08B0C650(obj[8][0x54], &sp[0]) with sp[0] = a1, sp[4] = a2; v0 = 1 when obj[0x1C] is not negative (nor + srl 31), else 0.
void func_08AC03E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    const std::uint32_t a2 = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[3] = a1;
    if (flag != 0u) {
        const std::uint32_t target = word(rt, word(rt, obj + 8u) + 0x54u);
        put(rt, obj + 0x1Cu, call_set(rt, ctx, 0x08B1554Cu, {target, a1, a2}));
    } else {
        const std::uint32_t target = word(rt, word(rt, obj + 8u) + 0x54u);
        put(rt, frame.at(0u), a1);
        put(rt, frame.at(4u), a2);  // the delay slot of the jal
        put(rt, obj + 0x1Cu, call_set(rt, ctx, 0x08B0C650u, {target, frame.at(0u)}));
    }
    ctx.gpr[2] = (~word(rt, obj + 0x1Cu)) >> 31;
}

// func_08872E18(self = a0, obj = a1): clears bit 0 of self[0x5C][0x100]; t = obj's virtual slot 0x1C (float) * func_0886B0F8(self[8]) (float); func_088728C0(self[0x5C], self + 0xC); func_08872054(self[0x5C], t * 60.0f);
// when func_0886B444(self[8], 3) is zero: s2 = func_0886A9A0(self[4], 5) and, when it is not zero, func_0886F8CC(s2), func_0886F8E4(s2), func_08872AC8(self[0x5C], self + 0xC, func_0886F8D4(s2)) and bit 0 of self[0x5C][0x100] is set again.
// Ends with a tail call func_0890AEBC(self[0x5C] + 8) with f12 = t. v0 / v1 at the tail call are the last call's (or the OR-ed flag word and self[0x5C] on the full path).
void func_08872E18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    (void)frame;
    const std::uint32_t chain = self + 0xCu;  // s3
    const std::uint32_t owner = word(rt, self + 0x5Cu);
    put(rt, owner + 0x100u, insert_bits(word(rt, owner + 0x100u), 0u, 0u, 1u));
    float t = call_virtual_full(rt, ctx, obj, 0x1Cu, {obj}).f0;
    t = psp_mul(t, call_setf(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)}, {}).f0);
    (void)call_set(rt, ctx, 0x088728C0u, {word(rt, self + 0x5Cu), chain});
    const float scaled = psp_mul(t, std::bit_cast<float>(0x42700000u));
    (void)call_setf(rt, ctx, 0x08872054u, {word(rt, self + 0x5Cu)}, {scaled});
    const std::uint32_t ready = call_set(rt, ctx, 0x0886B444u, {word(rt, self + 8u), 3u});
    if (ready == 0u) {
        const std::uint32_t s2 = call_set(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 5u});
        if (s2 != 0u) {
            (void)call_set(rt, ctx, 0x0886F8CCu, {s2});
            (void)call_set(rt, ctx, 0x0886F8E4u, {s2});
            const std::uint32_t target = word(rt, self + 0x5Cu);
            const std::uint32_t third = call_set(rt, ctx, 0x0886F8D4u, {s2});
            (void)call_set(rt, ctx, 0x08872AC8u, {target, chain, third});
            const std::uint32_t block = word(rt, self + 0x5Cu);
            const std::uint32_t flags = word(rt, block + 0x100u) | 1u;
            put(rt, block + 0x100u, flags);
            ctx.gpr[2] = flags;
            ctx.gpr[3] = block;
        }
    }
    ctx.fpr[12] = t;
    tail_call(ctx, 0x0890AEBCu, {word(rt, self + 0x5Cu) + 8u});
}

// func_08C15E8C(vector = a0, dest = a1, first = a2): moves the 0x2C-byte elements [first, vector[4]) down to dest (n = ((vector[4] - first) >> 2) * 0xBA2E8BA3, i.e. the element count; a dest equal to the source is not copied word by word), then
// func_08C15E7C(end of the moved range, vector[4], sp) runs and vector[4] shrinks by (first - dest) & ~3. v0 = dest.
void func_08C15E8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t dest = ctx.gpr[5];
    const std::uint32_t first = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t distance = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - first) >> 2);
    const std::uint32_t n = distance * 0xBA2E8BA3u;
    std::uint32_t out = dest;
    if (signed_of(n) > 0) {
        std::uint32_t src = first;
        for (std::uint32_t i = 0u; i != n; ++i) {
            const std::uint32_t w0 = word(rt, src);
            const std::uint32_t w1 = word(rt, src + 4u);
            put(rt, out, w0);
            put(rt, out + 4u, w1);
            if (out + 8u != src + 8u) {
                const std::uint32_t f0 = word(rt, src + 0x14u);
                const std::uint32_t f1 = word(rt, src + 8u);
                const std::uint32_t f2 = word(rt, src + 0xCu);
                const std::uint32_t f3 = word(rt, src + 0x10u);
                put(rt, out + 0x14u, f0);
                put(rt, out + 8u, f1);
                put(rt, out + 0xCu, f2);
                put(rt, out + 0x10u, f3);
            }
            if (out + 0x18u != src + 0x18u) {
                const std::uint32_t f0 = word(rt, src + 0x24u);
                const std::uint32_t f1 = word(rt, src + 0x18u);
                const std::uint32_t f2 = word(rt, src + 0x1Cu);
                const std::uint32_t f3 = word(rt, src + 0x20u);
                put(rt, out + 0x24u, f0);
                put(rt, out + 0x18u, f1);
                put(rt, out + 0x1Cu, f2);
                put(rt, out + 0x20u, f3);
            }
            const std::uint32_t last = word(rt, src + 0x28u);
            src += 0x2Cu;
            put(rt, out + 0x28u, last);
            out += 0x2Cu;
        }
    }
    const std::uint32_t old_end = word(rt, vector + 4u);
    (void)call_set(rt, ctx, 0x08C15E7Cu, {out, old_end, frame.at(0u)});
    const std::uint32_t shrink = (first - dest) & ~3u;
    put(rt, vector + 4u, word(rt, vector + 4u) - shrink);
    ctx.gpr[2] = dest;
}

// func_08B0EC60(out = a0, key = a1): func_08B0CE58(out, key), func_08B0E0CC(sp) and node = func_08B0EA48(sp, key); a node that exists gets node[0x7C] = 4, func_08B0DE3C(node), then its 0x3C bytes at node + 0xA8 are copied to out and func_08B0EA58(node, node[0xDC]) runs; func_08B0DDF0(sp) always; v0 = out.
void func_08B0EC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x110u);
    (void)call_set(rt, ctx, 0x08B0CE58u, {out, key});
    (void)call_set(rt, ctx, 0x08B0E0CCu, {frame.at(0u)});
    const std::uint32_t node = call_set(rt, ctx, 0x08B0EA48u, {frame.at(0u), key});
    if (node != 0u) {
        put(rt, node + 0x7Cu, 4u);
        (void)call_set(rt, ctx, 0x08B0DE3Cu, {node});
        for (std::uint32_t chunk = 0u; chunk < 0x30u; chunk += 0x10u) {
            const std::uint32_t w0 = word(rt, node + 0xA8u + chunk);
            const std::uint32_t w1 = word(rt, node + 0xA8u + chunk + 4u);
            const std::uint32_t w2 = word(rt, node + 0xA8u + chunk + 8u);
            const std::uint32_t w3 = word(rt, node + 0xA8u + chunk + 0xCu);
            put(rt, out + chunk, w0);
            put(rt, out + chunk + 4u, w1);
            put(rt, out + chunk + 8u, w2);
            put(rt, out + chunk + 0xCu, w3);
        }
        const std::uint32_t t0 = word(rt, node + 0xE0u);
        const std::uint32_t t1 = word(rt, node + 0xD8u);
        const std::uint32_t t2 = word(rt, node + 0xDCu);
        put(rt, out + 0x38u, t0);
        put(rt, out + 0x30u, t1);
        put(rt, out + 0x34u, t2);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08B0EA58u, {node, t2});
    }
    (void)call_set(rt, ctx, 0x08B0DDF0u, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// func_08BEE82C(list = a0, key = a1): walks the circular doubly linked list whose sentinel is `list` (next at +0): for each node whose payload at +8 satisfies func_089FD6E0(key, node[8]) the node is unlinked (func_08B5C094(node)), its payload destroyed (func_08A5B9EC(node + 8)) and the node freed
// (func_08A5BC54(node, 0xC, 4)); the next pointer is read before the test. v0 = the sentinel when the loop ends, or list[0] (= list) for an empty list.
void func_08BEE82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t node = word(rt, list);
    ctx.gpr[2] = node;
    if (node == list) return;
    for (;;) {
        const std::uint32_t payload = word(rt, node + 8u);
        const std::uint32_t next = word(rt, node);  // the delay slot of the jal below
        if (call_set(rt, ctx, 0x089FD6E0u, {key, payload}) != 0u) {
            (void)call_set(rt, ctx, 0x08B5C094u, {node});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {node + 8u});
            (void)call_set(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        }
        node = next;
        ctx.gpr[2] = next;
        if (list == next) break;
    }
}

// func_089C5BB8(a0, a1): v = func_08A7BDBC(a1) stored at sp[4]; func_08A7C0E0(sp + 8, &sp[4]); func_08A9DD14(sp) fills sp[0] (an object, reported by func_08A5BB7C when null), func_08A9D890(sp[0], a0, sp + 8), then three releases: func_08A5B9EC(sp), func_089C4EFC(a1), func_08A5B9EC(sp + 8).
void func_089C5BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t pair = frame.at(8u);
    const std::uint32_t cell = frame.at(4u);
    const std::uint32_t value = call_set(rt, ctx, 0x08A7BDBCu, {a1});
    put(rt, cell, value);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A7C0E0u, {pair, cell});
    (void)call_set(rt, ctx, 0x08A9DD14u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6398u});
    (void)call_set(rt, ctx, 0x08A9D890u, {object, a0, pair});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x089C4EFCu, {a1});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pair});
}

// func_089B17B8(obj = a0): resets an object with six calls on it: func_089B17AC(obj), then (obj, 0) for func_089B0044, func_089AB008, func_089B05F8, func_089B08E4, func_089AFF3C and, as the tail call, func_089B0284.
void func_089B17B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x089B17ACu, {obj});
    (void)call_set(rt, ctx, 0x089B0044u, {obj, 0u});
    (void)call_set(rt, ctx, 0x089AB008u, {obj, 0u});
    (void)call_set(rt, ctx, 0x089B05F8u, {obj, 0u});
    (void)call_set(rt, ctx, 0x089B08E4u, {obj, 0u});
    (void)call_set(rt, ctx, 0x089AFF3Cu, {obj, 0u});
    tail_call(ctx, 0x089B0284u, {obj, 0u});
}

// func_08C4B3C0(first = a0, last = a1, dest = a2): copies the 0x70-byte elements [first, last) to dest (a null dest is skipped but advances; the element is copied as 16-byte chunks); v0 = the end of dest.
void func_08C4B3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t src = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    std::uint32_t dest = ctx.gpr[6];
    if (src != last) {
        for (;;) {
            if (dest != 0u) {
                for (std::uint32_t chunk = 0u; chunk < 0x70u; chunk += 0x10u) {
                    const std::uint32_t w0 = word(rt, src + chunk);
                    const std::uint32_t w1 = word(rt, src + chunk + 4u);
                    const std::uint32_t w2 = word(rt, src + chunk + 8u);
                    const std::uint32_t w3 = word(rt, src + chunk + 0xCu);
                    put(rt, dest + chunk, w0);
                    put(rt, dest + chunk + 4u, w1);
                    put(rt, dest + chunk + 8u, w2);
                    put(rt, dest + chunk + 0xCu, w3);
                }
            }
            src += 0x70u;
            const bool more = (last != src);
            dest += 0x70u;
            if (!more) break;
        }
    }
    ctx.gpr[2] = dest;
}

// func_08C4B568(first = a0, last = a1, dest = a2): the same copy for 0x18-byte elements (six words, all loaded before they are stored); v0 = the end of dest.
void func_08C4B568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t src = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    std::uint32_t dest = ctx.gpr[6];
    if (src != last) {
        for (;;) {
            const std::uint32_t next = src + 0x18u;
            if (dest != 0u) {
                const std::uint32_t w5 = word(rt, src + 0x14u);
                const std::uint32_t w0 = word(rt, src);
                const std::uint32_t w1 = word(rt, src + 4u);
                const std::uint32_t w2 = word(rt, src + 8u);
                const std::uint32_t w3 = word(rt, src + 0xCu);
                const std::uint32_t w4 = word(rt, src + 0x10u);
                put(rt, dest, w0);
                put(rt, dest + 4u, w1);
                put(rt, dest + 8u, w2);
                put(rt, dest + 0xCu, w3);
                put(rt, dest + 0x10u, w4);
                put(rt, dest + 0x14u, w5);
            }
            src = next;
            dest += 0x18u;
            if (last == next) break;
        }
    }
    ctx.gpr[2] = dest;
}

// Shared by func_08C4B740 / func_08C4B7A0 / func_08C4B800 / func_08C4B860 (the same code four times, only the callee differs; obj = a0): takes the three words of obj (begin / end / capacity at +0 / +4 / +8), zeroes them, keeps them in a stack triple (sp+4 / sp+8 / sp+0xC),
// calls Callee(begin, end, sp) and releases sp[4] with func_08B35254 when it is not zero; v0 = that call's, or sp[4].
template <std::uint32_t Callee>
void take_range_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t end = word(rt, obj + 4u);
    const std::uint32_t begin = word(rt, obj);
    const std::uint32_t capacity = word(rt, obj + 8u);
    put(rt, obj + 8u, 0u);
    put(rt, obj, 0u);
    put(rt, obj + 4u, 0u);
    put(rt, frame.at(0xCu), capacity);
    put(rt, frame.at(4u), begin);
    put(rt, frame.at(8u), end);  // the delay slot of the jal
    ctx.gpr[2] = obj;
    (void)call_set(rt, ctx, Callee, {begin, end, frame.at(0u)});
    const std::uint32_t held = word(rt, frame.at(4u));
    ctx.gpr[2] = held;
    if (held != 0u) (void)call_set(rt, ctx, 0x08B35254u, {held});
}
void func_08C4B740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_range_and_release<0x08C498B4u>(rt, ctx); }
void func_08C4B7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_range_and_release<0x08C498C4u>(rt, ctx); }
void func_08C4B800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_range_and_release<0x08C498D4u>(rt, ctx); }
void func_08C4B860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_range_and_release<0x08C499B0u>(rt, ctx); }

// func_08C1CE44(a = a0, b = a1): swaps the contents of two red-black tree headers (word +8 = root, +0xC = leftmost, +0x10 = rightmost, +0x14 = count; the roots' parent words at +4 point back at the header + 4): an empty side takes the other's root / leftmost / rightmost and the other becomes empty
// (leftmost / rightmost point at its own header + 4), two non-empty sides swap the words and repair both roots' parents; the counts are always swapped. v0 = the old count of b.
void func_08C1CE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t a_root = word(rt, a + 8u);
    if (a_root == 0u) {
        const std::uint32_t b_root = word(rt, b + 8u);
        if (b_root != 0u) {
            put(rt, a + 8u, b_root);
            put(rt, a + 0xCu, word(rt, b + 0xCu));
            put(rt, a + 0x10u, word(rt, b + 0x10u));
            put(rt, b_root + 4u, a + 4u);
            put(rt, b + 0x10u, b + 4u);
            put(rt, b + 8u, 0u);
            put(rt, b + 0xCu, b + 4u);
        }
    } else {
        const std::uint32_t b_root = word(rt, b + 8u);
        if (b_root == 0u) {
            put(rt, b + 8u, a_root);
            put(rt, b + 0xCu, word(rt, a + 0xCu));
            put(rt, b + 0x10u, word(rt, a + 0x10u));
            put(rt, a_root + 4u, b + 4u);
            put(rt, a + 0x10u, a + 4u);
            put(rt, a + 8u, 0u);
            put(rt, a + 0xCu, a + 4u);
        } else {
            put(rt, a + 8u, b_root);
            put(rt, b + 8u, a_root);
            const std::uint32_t a_left = word(rt, a + 0xCu);
            const std::uint32_t b_left = word(rt, b + 0xCu);
            put(rt, a + 0xCu, b_left);
            put(rt, b + 0xCu, a_left);
            const std::uint32_t b_right = word(rt, b + 0x10u);
            const std::uint32_t a_right = word(rt, a + 0x10u);
            put(rt, a + 0x10u, b_right);
            put(rt, b + 0x10u, a_right);
            put(rt, word(rt, a + 8u) + 4u, a + 4u);
            put(rt, word(rt, b + 8u) + 4u, b + 4u);
        }
    }
    const std::uint32_t a_count = word(rt, a + 0x14u);
    const std::uint32_t b_count = word(rt, b + 0x14u);
    put(rt, a + 0x14u, b_count);
    ctx.gpr[2] = b_count;
    ctx.gpr[3] = a_count;
    put(rt, b + 0x14u, a_count);
}

// func_08C34270(a0, sink = a1, holder = a2): holder[0] is an object (reported by func_08A5BB7C when null) whose virtual slot 0x7C answers a count n; func_08C31A00(sink, n) and then, for index 0..n-1, a temporary at sp is filled by slot 0x84 (sp, holder[0], index; v0 = vtable, v1 = function), handed to func_08C34248(sink, sp)
// and released with func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand): a null holder[0] (both func_08A5BB7C reports) and the empty answer count == 0.
void func_08C34270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[5];
    const std::uint32_t holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t object = word(rt, holder);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    const std::uint32_t count = call_virtual(rt, ctx, object, 0x7Cu, {object});
    (void)call_set(rt, ctx, 0x08C31A00u, {sink, count});
    if (count == 0u) return;
    std::uint32_t index = 0u;
    do {
        object = word(rt, holder);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        const std::uint32_t table = word(rt, object);
        const std::uint32_t function = word(rt, table + 0x84u);
        ctx.gpr[2] = table;
        ctx.gpr[3] = function;
        const std::uint32_t current = index++;  // the delay slot of the jalr
        (void)call_set(rt, ctx, function, {frame.at(0u), object, current});
        (void)call_set(rt, ctx, 0x08C34248u, {sink, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    } while (count != index);
}

// func_08B23A70(obj = a0, text = a1): appends '/'-separated components of `text` to the string described by obj (data pointer at +0, capacity-ish word +4, length word +8, as read below): nothing happens when the data pointer plus length is zero or +4 equals the length. Otherwise room = obj[4] - obj[8] - 1 bytes
// are available: func_08B35F60 gives the length of what is there, then for each component (func_08B237A0 finds its end, func_08B23778 skips to the next one) a '/' and as many bytes as fit are copied with func_08B7C830 (strncpy); the string is closed with a zero byte and obj goes to func_08B1CB80 as a tail call. v0 = obj on the early exits.
// Branches the routes do not take (checked against the asm by hand): data pointer + length == 0 and capacity == length (the two early returns with v0 = obj), a zero-length component (the break before the '/'), a component that does not fit (count = remaining, remaining = 0) and the two loop exits (next component empty, no room left).
void func_08B23A70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::uint32_t used = word(rt, obj + 8u);
    const std::uint32_t data = word(rt, obj);
    const std::uint32_t capacity = word(rt, obj + 4u);
    std::uint32_t end = data + used;  // s0
    if (end == 0u) {
        ctx.gpr[2] = obj;
        return;
    }
    const std::uint32_t room = capacity - used;  // v0 of the delay slot
    if (room == 0u) {
        ctx.gpr[2] = obj;
        return;
    }
    std::int32_t remaining = signed_of(room - 1u);  // s3: the delay slot of the jal reads v0 = room
    std::uint32_t s1 = call_set(rt, ctx, 0x08B23778u, {text});
    const std::uint32_t existing = call_set(rt, ctx, 0x08B35F60u, {end});
    end += existing;
    if (sext8(byte_u(rt, s1)) != 0u && remaining > 0) {
        for (;;) {
            const std::uint32_t s5 = call_set(rt, ctx, 0x08B237A0u, {s1});
            const std::int32_t after = remaining - 1;  // v1
            const std::int32_t length = signed_of(s5 - s1);
            if (length == 0) {
                ctx.gpr[2] = 0u;
                break;
            }
            const bool fits = length < after;  // a2 = slt v0, v1
            const std::uint32_t s4 = end + 1u;
            put8(rt, end, 0x2Fu);
            std::uint32_t count = static_cast<std::uint32_t>(after);
            remaining = 0;
            if (fits) {
                count = static_cast<std::uint32_t>(length);
                remaining = after - length;
            }
            (void)call_set(rt, ctx, 0x08B7C830u, {s4, s1, count});
            s1 = call_set(rt, ctx, 0x08B23778u, {s5});
            end = s4 + count;
            ctx.gpr[2] = sext8(byte_u(rt, s1));
            if (ctx.gpr[2] == 0u) break;
            if (!(remaining > 0)) break;
        }
    }
    put8(rt, end, 0u);
    tail_call(ctx, 0x08B1CB80u, {obj});
}

// Shared by func_08C07E24 / func_08C070FC (assignment of a vector whose elements are 8 / 16 bytes: begin / end / capacity-end words at +0 / +4 / +8; v0 = this; the same plan as the batch-99 assignments): this == other does nothing. Capacity < n: a new block func_08B35840(0x10, n bytes),
// `uninit_copy`(other.begin, other.end, block, sp) copies, `destroy_range`(begin, end, sp+3) destroys the old range, the old block is released with func_08B35254(begin) when it exists, begin = block, capacity-end = block + n bytes. Size >= n: each of the n elements is assigned
// (8-byte kind: two raw words; 16-byte kind: func_089641C4(dst, src)) and `destroy_tail`(end of the assigned range, end, sp+2) destroys the rest. Size < n: the first `size` elements are assigned and `uninit_copy`(other.begin + ((end - begin) & ~(element - 1)), other.end, end, sp+1) copies the rest.
// end = begin + n bytes.
template <unsigned Shift, std::uint32_t UninitCopy, std::uint32_t DestroyRange, std::uint32_t DestroyTail, bool CallAssign>
void vector_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t step = 1u << Shift;
    std::uint32_t s0 = word(rt, other);
    std::uint32_t s2 = word(rt, other + 4u);
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - word(rt, self)) >> Shift);
    std::uint32_t count = static_cast<std::uint32_t>(signed_of(s2 - s0) >> Shift);
    const auto assign = [&](std::uint32_t dst, std::uint32_t src) {
        if (CallAssign) {
            (void)call_set(rt, ctx, 0x089641C4u, {dst, src});
        } else {
            put(rt, dst, word(rt, src));
            put(rt, dst + 4u, word(rt, src + 4u));
        }
    };
    std::uint32_t bytes;
    if (capacity < count) {
        count <<= Shift;
        const std::uint32_t block = call_set(rt, ctx, 0x08B35840u, {0x10u, count});
        (void)call_set(rt, ctx, UninitCopy, {s0, s2, block, frame.at(0u)});
        (void)call_set(rt, ctx, DestroyRange, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const std::uint32_t old = word(rt, self);
        if (old != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old});
        put(rt, self + 8u, block + count);
        put(rt, self, block);
        bytes = count;
    } else {
        std::uint32_t a2 = word(rt, self + 4u);
        const std::uint32_t a0 = word(rt, self);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(a2 - a0) >> Shift);
        std::uint32_t s1 = a0;
        if (!(size < count)) {
            if (signed_of(count) > 0) {
                for (std::uint32_t i = 0u; i != count; ++i) {
                    assign(s1, s0);
                    s0 += step;
                    s1 += step;
                }
                a2 = word(rt, self + 4u);
            }
            (void)call_set(rt, ctx, DestroyTail, {s1, a2, frame.at(2u)});
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
            (void)call_set(rt, ctx, UninitCopy, {s0 + used, s2, a2, frame.at(1u)});
        }
        bytes = count << Shift;
    }
    put(rt, self + 4u, word(rt, self) + bytes);
    ctx.gpr[2] = self;
}
void func_08C07E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<3u, 0x08C066F4u, 0x08BF0BF0u, 0x08BF1070u, false>(rt, ctx);
}
void func_08C070FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<4u, 0x08C0672Cu, 0x08C06684u, 0x08C06560u, true>(rt, ctx);
}

// func_08B19F10(obj = a0): when the byte obj[0x60] is zero it is set to 1 and obj[0x5C] = func_08B1B92C(sp, a1, a2, 0) where sp[0] = 0x08B1A284 (a function address) and sp[4] = obj; otherwise v0 = that byte.
// Branches the routes do not take (checked against the asm by hand): the byte at obj[0x60] is never already set in a compared case (the early return with v0 = that byte); 5 of the 10 calls are not comparable (func_08B1B92C reaches imports).
void func_08B19F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[7] = 0u;
    const std::uint32_t flag = byte_u(rt, obj + 0x60u);
    ctx.gpr[2] = flag;
    ctx.gpr[4] = frame.at(0u);
    if (flag != 0u) return;
    ctx.gpr[3] = 1u;
    put8(rt, obj + 0x60u, 1u);
    put(rt, frame.at(0u), 0x08B1A284u);
    put(rt, frame.at(4u), obj);  // the delay slot of the jal
    put(rt, obj + 0x5Cu, call_set(rt, ctx, 0x08B1B92Cu, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], 0u}));
}

// func_08A12E38(owner = a0): block = func_08A5BC20(0x7C0, 4); func_08A12C54(block); sp[0] = block; func_089C15F4(owner, &sp[0]). v0 is the last call's.
void func_08A12E38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x7C0u, 4u});
    (void)call_set(rt, ctx, 0x08A12C54u, {block});
    put(rt, frame.at(0u), block);  // the delay slot of the last jal
    (void)call_set(rt, ctx, 0x089C15F4u, {owner, frame.at(0u)});
}

// func_08ACEB40(base = a0, index = a1, value = a2): record = base + index * 0xF0 (the byte at record + 0x4F5 marks it used; v0 = that byte when it is set). Otherwise the byte becomes 1, record[0x4C8] = value, func_08B24238(value), then func_08ACEA1C(base, base + record_offset + 0x420) and
// a tail call to func_08ACEA8C with the same two arguments.
void func_08ACEB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    const std::uint32_t offset = (index << 8) - (index << 4);
    const std::uint32_t record = offset + base;
    const std::uint32_t used = byte_u(rt, record + 0x4F5u);
    ctx.gpr[4] = value;
    if (used != 0u) {
        ctx.gpr[2] = used;
        return;
    }
    const std::uint32_t second = base + (offset + 0x420u);
    put8(rt, record + 0x4F5u, 1u);
    put(rt, record + 0x4C8u, value);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B24238u, {value});
    (void)call_set(rt, ctx, 0x08ACEA1Cu, {base, second});
    tail_call(ctx, 0x08ACEA8Cu, {base, second});
}

// func_08BDD7D8(first = a0, last = a1): for each 0x1C-byte element [first, last), with s0 = element + 4: func_08BDD7C8(s0[0xC], s0[0x10], sp+1), func_08B35254 on s0[0xC] when it is not zero, func_08BDD7B8(s0[0], s0[4], sp), func_08B35254 on s0[0] when it is not zero.
// Empty range: nothing (v0 untouched).
void func_08BDD7D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (first == last) return;
    std::uint32_t cursor = first;  // s1
    std::uint32_t s0 = first + 4u;
    do {
        cursor += 0x1Cu;  // the delay slot of the first jal
        (void)call_set(rt, ctx, 0x08BDD7C8u, {word(rt, s0 + 0xCu), word(rt, s0 + 0x10u), frame.at(1u)});
        const std::uint32_t second = word(rt, s0 + 0xCu);
        ctx.gpr[2] = second;
        if (second != 0u) (void)call_set(rt, ctx, 0x08B35254u, {second});
        (void)call_set(rt, ctx, 0x08BDD7B8u, {word(rt, s0), word(rt, s0 + 4u), frame.at(0u)});
        const std::uint32_t head = word(rt, s0);
        ctx.gpr[2] = head;
        s0 += 0x1Cu;  // the delay slot of the beqz
        if (head != 0u) (void)call_set(rt, ctx, 0x08B35254u, {head});
    } while (last != cursor);
}

}  // namespace
}  // namespace hot_batch_100
}  // namespace gt::game

#define HOT100(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_100::name, gt::ReturnKind::kind)
#define HOT100N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_100::name, gt::ReturnKind::kind)

// HOT100REGS
HOT100(0x08A4D57Cu, func_08A4D57C, Word);
HOT100(0x08B5F2B8u, func_08B5F2B8, Word);
HOT100(0x088AA074u, func_088AA074, Word);
HOT100(0x088FA224u, func_088FA224, Word);
// HOT100(0x08AF2D20u, func_08AF2D20, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08A9DD14u, func_08A9DD14, Word);  // HOT100: NOT registered — installing it stops ~330 start-up functions from running with a green gate (FINDINGS "A decompiled function can switch off code the gate never sees"; batch 83 unregistered it, review of #237 found this re-registration by the drop of compared functions 5,201 → 4,915)
// HOT100(0x08BE6D44u, func_08BE6D44, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08BE6674u, func_08BE6674, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08BE834Cu, func_08BE834C, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C2B76Cu, func_08C2B76C, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
HOT100(0x08C401C4u, func_08C401C4, Word);
HOT100(0x08C40FA4u, func_08C40FA4, Word);
HOT100(0x08C413DCu, func_08C413DC, Word);
// HOT100(0x08B7B2E4u, func_08B7B2E4, Word);  // HOT100: a setjmp: it stores the harness's sentinel return address in the buffer and the later longjmp jumps to it outside the case ("diff harness sentinel reached without a pending continuation", every route stops)
// HOT100(0x08B100D0u, func_08B100D0, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08B10170u, func_08B10170, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08B10204u, func_08B10204, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x089C4C20u, func_089C4C20, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08AC03E0u, func_08AC03E0, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08872E18u, func_08872E18, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C15E8Cu, func_08C15E8C, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08B0EC60u, func_08B0EC60, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08BEE82Cu, func_08BEE82C, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x089C5BB8u, func_089C5BB8, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x089B17B8u, func_089B17B8, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08C4B3C0u, func_08C4B3C0, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C4B568u, func_08C4B568, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C4B740u, func_08C4B740, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C4B7A0u, func_08C4B7A0, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C4B800u, func_08C4B800, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C4B860u, func_08C4B860, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C1CE44u, func_08C1CE44, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
HOT100(0x08C34270u, func_08C34270, Word);
HOT100(0x08B23A70u, func_08B23A70, Word);
// HOT100(0x08C07E24u, func_08C07E24, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
// HOT100(0x08C070FCu, func_08C070FC, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
HOT100(0x08B19F10u, func_08B19F10, Word);
// HOT100(0x08A12E38u, func_08A12E38, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08ACEB40u, func_08ACEB40, Word);  // HOT100: compared 0 in every run: every case is not comparable (the callees reach sce* imports that block or switch threads)
// HOT100(0x08BDD7D8u, func_08BDD7D8, Word);  // HOT100: compared cases never changed memory and never ran the body (empty range / count zero / null destination paths only; a dense run of the race route changed nothing either)
