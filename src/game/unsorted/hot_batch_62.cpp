// Hot batch 62: more functions with branches or loops that the four pad routes reach (call probe as in batches 57-61). As in
// batches 60-61, each function's comment lists the branches the compared cases of the fast gate do not take (found with
// `tools.decomp_gate coverage`); those were compared against the asm by hand, reading the branch TARGET (a value set in a
// delay slot is the result only if the target does not overwrite it). Their classes aren't known yet, so they keep their
// addresses as names. Written from the asm; the generated code was only a reading aid.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_62 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kStreamFlag = 0x08D740F8u;   // word: nonzero once the object below is set up
constexpr std::uint32_t kStreamObject = 0x08D74100u; // the object whose methods 0x08C7B050 / 0x08C7B068 / 0x08C7B0A8 run

std::int32_t half_at(psprecomp::Runtime &rt, std::uint32_t address) {
    return static_cast<std::int16_t>(rt.memory().load16(address));
}

void put_half(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}

// *dest++ = *first++ for each 2-byte element of [first, last) (a null dest only advancing); v0 = dest after.
void copy_pairs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            if (destination != 0u) {
                rt.memory().store8(destination, static_cast<std::uint8_t>(rt.memory().load8(source)));
                rt.memory().store8(destination + 1u, static_cast<std::uint8_t>(rt.memory().load8(source + 1u)));
            }
            source += 2u;
            destination += 2u;
        } while (last != source);
    }
    ctx.gpr[2] = destination;
}

// The method at byte `slot` of the vtable of the object at `object`, tail-called with a0 = `argument`; v0 holds the vtable meanwhile.
void forward_to_method(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                       std::uint32_t argument) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t method = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {argument});
}

} // namespace

// func_0887D33C(this): the method at vtable offset 32 of the object this[20], tail call with a0 = that object; nothing when it is null.
// (v0 is the method's address meanwhile.)
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-null child (the tail call).
void func_0887D33C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 80u);
    if (child == 0u) return;
    const std::uint32_t method = word(rt, word(rt, child) + 32u);
    ctx.gpr[2] = method;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {child});
}

// func_0887D364(this): as func_0887D33C with the method at vtable offset 36.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-null child (the tail call).
void func_0887D364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 80u);
    if (child == 0u) return;
    const std::uint32_t method = word(rt, word(rt, child) + 36u);
    ctx.gpr[2] = method;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {child});
}

// func_08B8E360(object): the method at vtable offset 4 of the object (tail call, a0 unchanged) unless it is null.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null object (returns with v0 untouched).
void func_08B8E360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    if (object == 0u) return;
    forward_to_method(rt, ctx, object, 4u, object);
}

// func_08BD5ED8 / func_08BD5EFC / func_08BE0668 / func_08BE068C(this): the method at vtable offset 4 of the object this[3] (tail call, a0 = that
// object); returns 0 when this[3] is null.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[3] == 0 (returns 0).
void forward_member_12(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 12u);
    ctx.gpr[2] = child;
    if (child == 0u) return;
    forward_to_method(rt, ctx, child, 4u, child);
}
void func_08BD5ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx); }
void func_08BD5EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx); }
void func_08BE0668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx); }
void func_08BE068C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx); }

// func_08B5371C(enable, r, g, b): two 3-word records at 0x08CE0DA8 and 0x08CE0DB8: with `enable` (low byte) nonzero they take the shifted
// components (c + 24) << 24 = r', g', b' as {r', g', b'} and {r' + 1, g' + 1, b' + 1}; without it both start with 0x0B000000 (the other words
// stay). v0 = b' + 1, or 0x08CE0000 when `enable` is 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): enable == 0 (the 0x0B000000 stores, v0 = 0x08CE0000).
void func_08B5371C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLow = 0x08CE0DA8u, kHigh = 0x08CE0DB8u;
    const std::uint32_t r = (ctx.gpr[5] + 24u) << 24, g = (ctx.gpr[6] + 24u) << 24, b = (ctx.gpr[7] + 24u) << 24;
    if ((ctx.gpr[4] & 255u) == 0u) {
        ctx.gpr[2] = 0x08CE0000u;
        put(rt, kLow, 0x0B000000u);
        put(rt, kHigh, 0x0B000000u);
        return;
    }
    ctx.gpr[2] = b + 1u;
    put(rt, kHigh + 8u, b + 1u);
    put(rt, kLow + 8u, b);
    put(rt, kHigh + 4u, g + 1u);
    put(rt, kLow, r + 1u);
    put(rt, kLow + 4u, g);
    put(rt, kHigh, r);
}

// func_08B571F4(values): appends four display-list words (commands 226..229) to the graphics library's list: command << 24 | the four values at
// values + 16 i + {12, 8, 4, 0}, each clamped to -8..7 and kept to 4 bits, in nibbles 3..0. The list pointer advances by 16; v0 = the library state.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a value below -8 or above 7 (the clamps).
void func_08B571F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t values = ctx.gpr[4];
    std::uint32_t list = word(rt, kGuListPointer);
    const auto nibble = [&](std::uint32_t at) {
        std::int32_t v = static_cast<std::int32_t>(word(rt, at));
        if (v < -8) v = -8;
        if (v > 7) v = 7;
        return static_cast<std::uint32_t>(v) & 0xFu;
    };
    for (std::uint32_t command = 226u; command != 230u; ++command) {
        const std::uint32_t packed = (command << 24) | (nibble(values + 12u) << 12) | (nibble(values + 8u) << 8) |
                                     (nibble(values + 4u) << 4) | nibble(values);
        put(rt, list, packed);
        values += 16u;
        list += 4u;
    }
    ctx.gpr[2] = kGuState;
    put(rt, kGuListPointer, list);
}

// func_08B7CA4C(hay, needle) -> the first place of hay where the string needle occurs (strstr): hay itself for an empty needle, 0 for none.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty needle, and a needle of two or more characters (the comparison loop after the first character; its mismatch resumes the scan).
void func_08B7CA4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t hay = ctx.gpr[4], needle = ctx.gpr[5];
    const auto at = [&](std::uint32_t p) { return static_cast<std::int8_t>(rt.memory().load8(p)); };
    const std::int8_t first = at(needle);
    if (first == 0) {
        ctx.gpr[2] = hay;
        return;
    }
    ctx.gpr[2] = 0u;
    for (std::uint32_t p = hay; rt.memory().load8(p) != 0u; ++p) {
        if (at(p) != first) continue;
        for (std::uint32_t k = 1u;; ++k) {
            const std::int8_t wanted = at(needle + k);
            if (wanted == 0) {
                ctx.gpr[2] = p;
                return;
            }
            if (at(p + k) != wanted) break;
        }
    }
}

// func_08B8CFA8 / func_08B8CFF0(first, last, dest) -> dest after the copy: 2 bytes per element of [first, last) (a null dest only advancing).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty range (the copy loop, null dest included).
void func_08B8CFA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_pairs(rt, ctx); }
void func_08B8CFF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_pairs(rt, ctx); }

// func_08BB1B5C(first, last, dest) -> dest after: *dest++ = *first++ for each word of [first, last), a null dest only advancing.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty range (the copy loop, null dest included).
void func_08BB1B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            if (destination != 0u) put(rt, destination, word(rt, source));
            source += 4u;
            destination += 4u;
        } while (last != source);
    }
    ctx.gpr[2] = destination;
}

// func_08B5F174(node) -> the node before `node` in a red-black tree (std::_Rb_tree_decrement; nodes {color, parent, left, right}, red = 0): the
// right child of the header when node is the red header, else the rightmost node of the left subtree, else the first ancestor reached from a right
// side.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the red header (the right child answers).
void func_08B5F174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    if (word(rt, node) == 0u && word(rt, word(rt, node + 4u) + 4u) == node) {
        ctx.gpr[2] = word(rt, node + 12u);
        return;
    }
    const std::uint32_t left = word(rt, node + 8u);
    if (left != 0u) {
        std::uint32_t rightmost = left;
        for (std::uint32_t next = word(rt, rightmost + 12u); next != 0u; next = word(rt, rightmost + 12u)) rightmost = next;
        ctx.gpr[2] = rightmost;
        return;
    }
    std::uint32_t parent = word(rt, node + 4u);
    if (node == word(rt, parent + 8u)) {
        std::uint32_t child = parent;
        for (;;) {
            parent = word(rt, child + 4u);
            if (child != word(rt, parent + 8u)) break;
            child = parent;
        }
    }
    ctx.gpr[2] = parent;
}

// func_08BB2620(first, last, dest) -> dest after: *dest++ = *first++ for each pointer of [first, last) (a null dest only advancing); each non-null
// pointer's count (word +16) is incremented.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty range (the copy loop, null dest and null elements included).
void func_08BB2620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            const std::uint32_t object = word(rt, source);
            source += 4u;
            if (destination != 0u) {
                put(rt, destination, object);
                if (object != 0u) put(rt, object + 16u, word(rt, object + 16u) + 1u);
            }
            destination += 4u;
        } while (source != last);
    }
    ctx.gpr[2] = destination;
}

// func_08BB8760(first, last, value*): every halfword of [first, last) takes the low 16 bits of the word *value.
void func_08BB8760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t last = ctx.gpr[5];
    const std::uint32_t value = word(rt, ctx.gpr[6]);
    for (std::uint32_t p = ctx.gpr[4]; p != last; p += 2u) put_half(rt, p, value);
}

// func_08BE43B0(dest, src): dest = src (two words); the second is a counted object whose count (+4) is incremented; v0 = that word, or 1 after the
// increment.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null counted object (nothing incremented, v0 = 0).
void func_08BE43B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t to = ctx.gpr[4], from = ctx.gpr[5];
    put(rt, to, word(rt, from));
    const std::uint32_t counted = word(rt, from + 4u);
    put(rt, to + 4u, counted);
    ctx.gpr[2] = counted;
    if (counted == 0u) return;
    put(rt, counted + 4u, word(rt, counted + 4u) + 1u);
    ctx.gpr[2] = 1u;
}

// func_08BF9240(dest, count, src): count times, the two words at src are copied to dest (a null dest skipped), dest advancing 8 each time; v0 = the
// first of those words when a copy happened.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count == 0 (returns with v0 untouched), and a null dest (skipped, v0 untouched).
void func_08BF9240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t destination = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (destination != 0u) {
            const std::uint32_t first = word(rt, source), second = word(rt, source + 4u);
            ctx.gpr[2] = first;
            put(rt, destination, first);
            put(rt, destination + 4u, second);
        }
        destination += 8u;
    } while (count != done);
}

// func_08C01F00(first, last) -> (last - first) + 1 for a non-empty range (the original counts up to it one step at a time); untouched v0 for an
// empty range.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty range (v0 = last - first + 1).
void func_08C01F00(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == ctx.gpr[5]) return;
    ctx.gpr[2] = ctx.gpr[5] - ctx.gpr[4] + 1u;
}

// func_08B958A8(p, q): swaps the words *p and *q (two string data pointers) after resetting a negative count (word +8 of data - 12) to 0 in each; v0
// = the old *p (the same as func_08BEEF60).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a negative count in *p or in *q (reset to 0).
void func_08B958A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4], q = ctx.gpr[5];
    const std::uint32_t first_rep = word(rt, p) - 12u;
    if (static_cast<std::int32_t>(word(rt, first_rep + 8u)) < 0) put(rt, first_rep + 8u, 0u);
    std::uint32_t second = word(rt, q);
    const std::uint32_t second_rep = second - 12u;
    if (static_cast<std::int32_t>(word(rt, second_rep + 8u)) < 0) {
        put(rt, second_rep + 8u, 0u);
        second = word(rt, q);
    }
    const std::uint32_t first = word(rt, p);
    put(rt, p, second);
    put(rt, q, first);
    ctx.gpr[2] = first;
}

// func_08B65660(this): vtable 0x08CDC1B8, the words 1..8 cleared, the eight pairs at +36 cleared, this[25] = 8, this[26] = this + 36; then
// func_08B5E0EC(this + 108) (tail call; v0 = 8 meanwhile).
void func_08B65660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CDC1B8u);
    for (std::uint32_t offset = 4u; offset <= 32u; offset += 4u) put(rt, self + offset, 0u);
    for (std::uint32_t offset = 36u; offset != 100u; offset += 4u) put(rt, self + offset, 0u);
    put(rt, self + 100u, 8u);
    put(rt, self + 104u, self + 36u);
    ctx.gpr[2] = 8u;
    tail_call(ctx, 0x08B5E0ECu, {self + 108u});
}

// func_08C3DBEC(pool, node) -> returns a node of the pool of 124-byte elements (starting at pool + 12) to its free list, unless its halfword +10 is
// already set: that halfword becomes 1, the node remembers the free list's head (halfword +14, the head at pool + 6), the old head's halfword +24 (when
// the head is not negative) and the head itself take the node's index. v0 = the old head's address, or node - (pool + 12) when it was already set.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the halfword +10 already set (returns node - (pool + 12)).
void func_08C3DBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4], node = ctx.gpr[5];
    const std::uint32_t difference = node - (pool + 12u);
    ctx.gpr[2] = difference;
    if (half_at(rt, node + 10u) != 0) return;
    // index = difference / 124 (exact): (difference >> 2) times the inverse of 31 modulo 2^32.
    const std::uint32_t index = static_cast<std::uint32_t>(static_cast<std::int32_t>(difference) >> 2) * 0xBDEF7BDFu;
    const std::int32_t head = half_at(rt, pool + 6u);
    put_half(rt, node + 10u, 1u);
    const std::uint32_t head_element = static_cast<std::uint32_t>(head) * 124u + pool;
    ctx.gpr[2] = head_element;
    put_half(rt, node + 14u, static_cast<std::uint32_t>(head));
    if (head >= 0) put_half(rt, head_element + 24u, index);
    put_half(rt, pool + 6u, index);
}

// func_08C3DD14(pool, node) -> takes a node of the pool (124-byte elements at pool + 12, as func_08C3DBEC) out of its doubly linked list of 16-bit
// indices (halfwords +12 next, +14 previous; -1 ends): the halfword +10 is cleared, +12 and +14 become -1, the neighbours (or the head at pool + 6)
// are linked to each other. v0 = the next node's address, else the previous one's, else 0 (also 0 when +10 was already clear).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the halfword +10 clear (returns 0), a negative previous index, and both neighbours negative (answers 0).
void func_08C3DD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4], node = ctx.gpr[5];
    if (half_at(rt, node + 10u) == 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::int32_t next = half_at(rt, node + 12u), previous = half_at(rt, node + 14u);
    put_half(rt, node + 10u, 0u);
    put_half(rt, node + 12u, 0xFFFFu);
    put_half(rt, node + 14u, 0xFFFFu);
    if (next >= 0) put_half(rt, pool + static_cast<std::uint32_t>(next) * 124u + 26u, static_cast<std::uint32_t>(previous));
    else put_half(rt, pool + 6u, static_cast<std::uint32_t>(previous));
    if (previous >= 0) put_half(rt, pool + static_cast<std::uint32_t>(previous) * 124u + 24u, static_cast<std::uint32_t>(next));
    if (next >= 0) ctx.gpr[2] = pool + static_cast<std::uint32_t>(next) * 124u + 12u;
    else if (previous >= 0) ctx.gpr[2] = pool + static_cast<std::uint32_t>(previous) * 124u + 12u;
    else ctx.gpr[2] = 0u;
}

// func_08C47DFC(out, state, args) -> out, after: state[1] (count) decremented; v = the halfword at args[2] + 4 is recorded (as state's halfword +0
// and, when v is -1, +2 when args[3] is null; else in the halfword args[3] + 4 and, when v is -1, the index (args[3] - *state[2]) / 6 in state's
// halfword +2); out = {+4: args[1], +8: *state[2] + 6 v (0 when v is negative), +12: args[3]}.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): args[3] set (the halfword args[3] + 4 and, for v == -1, the index (args[3] - *state[2]) / 6).
void func_08C47DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], state = ctx.gpr[5], args = ctx.gpr[6];
    const std::uint32_t source = word(rt, args + 8u);
    const std::uint32_t count = word(rt, state + 4u);
    const std::uint32_t end = word(rt, args + 12u);
    const std::int32_t value = half_at(rt, source + 4u);
    put(rt, state + 4u, count - 1u);
    if (end == 0u) {
        put_half(rt, state, static_cast<std::uint32_t>(value));
        if (value == -1) put_half(rt, state + 2u, static_cast<std::uint32_t>(value));
    } else {
        put_half(rt, end + 4u, static_cast<std::uint32_t>(value));
        if (value == -1) {
            const std::uint32_t begin = word(rt, word(rt, state + 8u));
            const std::int32_t halves = static_cast<std::int32_t>(end - begin) >> 1;
            put_half(rt, state + 2u, static_cast<std::uint32_t>(halves) * 0xAAAAAAABu);
        }
    }
    std::uint32_t element = 0u;
    if (value >= 0) element = word(rt, word(rt, state + 8u)) + static_cast<std::uint32_t>(value) * 6u;
    put(rt, out + 12u, end);
    put(rt, out + 4u, word(rt, args + 4u));
    put(rt, out + 8u, element);
    ctx.gpr[2] = out;
}

// func_08B55140(n, kind, out): when out is set and kind is 13, out = {the three floats at record n + 12.., the float 0 or (when the byte +36 of the
// record is set) the constant at 0x08CB61A0}, where the record is n * 40 bytes into the table at the graphics state - 3640. v0 = the state when out is
// null, 13 for another kind, else the byte (or 0x08CB0000 when it is set).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null out (v0 = the state), another kind (v0 = 13), a set byte at +48 (the constant at 0x08CB61A0).
void func_08B55140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t record = kGuState - 3640u + ctx.gpr[4] * 40u;
    const std::uint32_t out = ctx.gpr[6];
    ctx.gpr[2] = kGuState;
    if (out == 0u) return;
    ctx.gpr[2] = 13u;
    if (ctx.gpr[5] != 13u) return;
    put(rt, out, word(rt, record + 12u));
    put(rt, out + 4u, word(rt, record + 16u));
    put(rt, out + 8u, word(rt, record + 20u));
    ctx.gpr[2] = rt.memory().load8(record + 48u);
    std::uint32_t last = 0u;
    if (ctx.gpr[2] != 0u) {
        ctx.gpr[2] = 0x08CB0000u;
        last = word(rt, 0x08CB61A0u);
    }
    put(rt, out + 12u, last);
}

// func_08B57820(a, kind, c): kind 1 -> code 144, 0 -> 143, 4 -> 145 (another kind returns 4); then func_08B577D0(kGuState, code, a - 101, c)
// (tail call; v0 = 1 for kind 0 or 1, 4 for kind 4).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a kind other than 0, 1 and 4 (returns 4).
void func_08B57820(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], kind = ctx.gpr[5], c = ctx.gpr[6];
    std::uint32_t code;
    if (kind == 1u) {
        ctx.gpr[2] = 1u;
        code = 144u;
    } else if (kind == 0u) {
        ctx.gpr[2] = 1u;
        code = 143u;
    } else {
        ctx.gpr[2] = 4u;
        if (kind != 4u) return;
        code = 145u;
    }
    tail_call(ctx, 0x08B577D0u, {kGuState, code, a - 101u, c});
}

// func_08B4A9D0(d3, a3, b3): d = a x b (the cross product); the loads and stores interleave as in the original (d may overlap a or b).
void func_08B4A9D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t d = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    {
        const float b2 = float_at(rt, b + 8u), b1 = float_at(rt, b + 4u), a1 = float_at(rt, a + 4u), a2 = float_at(rt, a + 8u);
        put_float(rt, d, psp_mul(a1, b2) - psp_mul(a2, b1));
    }
    {
        const float b0 = float_at(rt, b), b2 = float_at(rt, b + 8u), a2 = float_at(rt, a + 8u), a0 = float_at(rt, a);
        put_float(rt, d + 4u, psp_mul(a2, b0) - psp_mul(a0, b2));
    }
    {
        const float a1 = float_at(rt, a + 4u), b0 = float_at(rt, b), a0 = float_at(rt, a), b1 = float_at(rt, b + 4u);
        put_float(rt, d + 8u, psp_mul(a0, b1) - psp_mul(a1, b0));
    }
}

// func_08B84270() -> 0 when the word at 0x08D740F8 is 0, else func_08C7B068(0x08D74100).
void func_08B84270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0u;
    if (word(rt, kStreamFlag) == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B068u, {kStreamObject});
}

// func_08B83FA4(a, b) -> 0x80420100 when the word at 0x08D740F8 is 0; 0x80420012 when b is 0; else func_08C7B0A8(0x08D74100, a, b).
void func_08B83FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    ctx.gpr[2] = 0x80420100u;
    if (word(rt, kStreamFlag) == 0u) return;
    ctx.gpr[2] = 0x80420012u;
    if (b == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B0A8u, {kStreamObject, a, b});
}

// func_08B83F40(a, b, c, d, e) -> 0x80420100 when the word at 0x08D740F8 is 0, else func_08C7B050(0x08D74100, a, b, c, d, e).
void func_08B83F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7], e = ctx.gpr[8];
    ctx.gpr[2] = 0x80420100u;
    if (word(rt, kStreamFlag) == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B050u, {kStreamObject, a, b, c, d, e});
}

// func_08BD2F8C(out, first, last, state) -> out: func_08AF9E88(*state[0], halfword p, state[0], state[1]) for each halfword p of [first, last)
// (state re-read for each); then out = {state[0], state[1]}.
void func_08BD2F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], last = ctx.gpr[6], state = ctx.gpr[7];
    std::uint32_t p = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        while (p != last) {
            const std::uint32_t first_word = word(rt, state);
            const std::uint32_t element = rt.memory().load16(p);
            const std::uint32_t second_word = word(rt, state + 4u);
            const std::uint32_t target = word(rt, first_word);
            p += 2u;
            ctx.gpr[2] = call_guest(rt, ctx, 0x08AF9E88u, {target, element, first_word, second_word});
        }
    }
    put(rt, out + 4u, word(rt, state + 4u));
    put(rt, out, word(rt, state));
    ctx.gpr[2] = out;
}

} // namespace hot_batch_62
} // namespace gt::game

#define HOT62(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_62::name, gt::ReturnKind::kind)
HOT62(0x0887D33Cu, func_0887D33C, Word);
HOT62(0x0887D364u, func_0887D364, Word);
HOT62(0x08B8E360u, func_08B8E360, Word);
HOT62(0x08BD5ED8u, func_08BD5ED8, Word);
HOT62(0x08BD5EFCu, func_08BD5EFC, Word);
HOT62(0x08BE0668u, func_08BE0668, Word);
HOT62(0x08BE068Cu, func_08BE068C, Word);
HOT62(0x08B5371Cu, func_08B5371C, Word);
HOT62(0x08B571F4u, func_08B571F4, Word);
HOT62(0x08B7CA4Cu, func_08B7CA4C, Word);
HOT62(0x08B8CFA8u, func_08B8CFA8, Word);
HOT62(0x08B8CFF0u, func_08B8CFF0, Word);
HOT62(0x08BB1B5Cu, func_08BB1B5C, Word);
HOT62(0x08B5F174u, func_08B5F174, Word);
HOT62(0x08BB2620u, func_08BB2620, Word);
HOT62(0x08BB8760u, func_08BB8760, Void);
HOT62(0x08BE43B0u, func_08BE43B0, Word);
HOT62(0x08BF9240u, func_08BF9240, Word);
HOT62(0x08C01F00u, func_08C01F00, Word);
HOT62(0x08B958A8u, func_08B958A8, Word);
HOT62(0x08B65660u, func_08B65660, Word);
HOT62(0x08C3DBECu, func_08C3DBEC, Word);
HOT62(0x08C3DD14u, func_08C3DD14, Word);
HOT62(0x08C47DFCu, func_08C47DFC, Word);
HOT62(0x08B55140u, func_08B55140, Word);
HOT62(0x08B57820u, func_08B57820, Word);
HOT62(0x08B4A9D0u, func_08B4A9D0, Void);
HOT62(0x08BD2F8Cu, func_08BD2F8C, Word);
// Written but not registered: every case reaches a sceSasCore import (0x08C7B050 / 0x08C7B068 / 0x08C7B0A8), which the harness cannot record
// ("not comparable" in all routes, compared 0), so the gate cannot verify them.
// HOT62(0x08B84270u, func_08B84270, Word);
HOT62(0x08B83FA4u, func_08B83FA4, Word);
HOT62(0x08B83F40u, func_08B83F40, Word);
