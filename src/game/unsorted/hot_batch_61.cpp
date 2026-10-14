// Hot batch 61: more functions with branches or loops that the four pad routes reach (call probe as in batches 57-60). As in
// batch 60, each function's comment lists the branches the compared cases of the fast gate do not take (found with
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
namespace hot_batch_61 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kAssignHolder = 0x08A61584u;  // (dest *holder, source *holder)
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu; // gt::core::holder_destroy
constexpr std::uint32_t kFree = 0x08A5BC54u;          // (object, size, alignment)
constexpr std::uint32_t kNil = 0x08A76AA0u;           // (holder *out): out = the global nil (src/game/adhoc/array.cpp)

} // namespace

// func_08C20318(this, dest): dest = func_08A7858C(nil, this + 8, 2, func_08A6159C(dest, 2)) built in a local holder, then the local one destroyed.
void func_08C20318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t key = call_guest(rt, ctx, 0x08A6159Cu, {destination, 2u});
    (void)call_guest(rt, ctx, kNil, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A7858Cu, {frame.at(0u), self + 8u, 2u, key});
    (void)call_guest(rt, ctx, kAssignHolder, {destination, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
}

// func_089098B8(this, x): func_08AF128C(child, x) for the children this[1] and (when set) this[2]; for this[3], when set, as the tail call.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[2] set (the second call); this[3] null (returns
// 0).
void func_089098B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF128Cu, {word(rt, self + 4u), x});
        const std::uint32_t second = word(rt, self + 8u);
        ctx.gpr[2] = second;
        if (second != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08AF128Cu, {second, x});
    }
    const std::uint32_t third = word(rt, self + 12u);
    ctx.gpr[2] = third;
    if (third != 0u) tail_call(ctx, 0x08AF128Cu, {third, x});
}

// func_08B4A82C(d3, a3, b3): d = a x b (the cross product). Each component re-reads a and b after the previous one
// is stored, as the original does: an in-place call (d == a or d == b) sees the components already written.
void func_08B4A82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t d = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    auto ax = [&] { return float_at(rt, a); };
    auto ay = [&] { return float_at(rt, a + 4u); };
    auto az = [&] { return float_at(rt, a + 8u); };
    auto bx = [&] { return float_at(rt, b); };
    auto by = [&] { return float_at(rt, b + 4u); };
    auto bz = [&] { return float_at(rt, b + 8u); };
    put_float(rt, d, psp_mul(ay(), bz()) - psp_mul(az(), by()));
    put_float(rt, d + 4u, psp_mul(az(), bx()) - psp_mul(ax(), bz()));
    put_float(rt, d + 8u, psp_mul(ax(), by()) - psp_mul(ay(), bx()));
}

// func_08BEEF60(p, q): swaps the words *p and *q (two string data pointers) after resetting a negative count (word +8 of data - 12) to 0 in each; v0
// = the old *p. Branches the routes do not take (checked against the asm by hand, reading the branch targets): a negative count in *p or in *q (reset
// to 0).
void func_08BEEF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
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

// func_08BF5C1C(a, b): returns 0x40000 (left in v0) unless func_08A76F20(b, func_08965B18(a)) answers nonzero and b's word +0xE0 has bit 18 set;
// then func_089A2FCC(a's word +4, b) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08A76F20 answering nonzero with bit 18 set (the
// tail call).
void func_08BF5C1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    std::uint32_t answer;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t value = call_guest(rt, ctx, 0x08965B18u, {a});
        answer = call_guest(rt, ctx, 0x08A76F20u, {b, value});
    }
    ctx.gpr[2] = 0x40000u;
    if (answer == 0u || (word(rt, b + 224u) & 0x40000u) == 0u) return;
    tail_call(ctx, 0x089A2FCCu, {word(rt, a + 4u), b});
}

// func_08C2BF34(first, last, dest) -> dest after the copy: *dest++ = *first++ for each word of [first, last), a null dest only advancing.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty range (the whole copy loop, a null dest
// included).
void func_08C2BF34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
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

// func_08C1CD74(a, key, p) -> 0 when key is the end node func_08C1CD28(a) answers; else whether the signed word *p is not below the word at the node
// (1 / 0). Branches the routes do not take (checked against the asm by hand, reading the branch targets): key == the end node (answers 0).
void func_08C1CD74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5], p = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_guest(rt, ctx, 0x08C1CD28u, {ctx.gpr[4]});
    if (key == node) {
        ctx.gpr[2] = 0u;
        return;
    }
    const bool below = static_cast<std::int32_t>(word(rt, p)) < static_cast<std::int32_t>(word(rt, node));
    ctx.gpr[2] = below ? 0u : 1u;
}

// func_08C12060(first, last): func_089F9B74(p) for each 16-byte element p of [first, last).
void func_08C12060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (first == last) return;
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t p = first;
    do {
        ctx.gpr[2] = call_guest(rt, ctx, 0x089F9B74u, {p});
        p += 16u;
    } while (last != p);
}

// func_08B55588(kind, value): flushes the graphics state when its flags are nonzero (func_08B59118(state, flags)), then appends
// 0x04000000 | (kind & 7) << 16 | value & 0xFFFF to the display list; v0 = the list pointer before.
void func_08B55588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kind = ctx.gpr[4], value = ctx.gpr[5];
    const std::uint32_t flags = word(rt, kGuFlags);
    if (flags != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B59118u, {kGuState, flags});
    }
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, list, 0x04000000u | ((kind & 7u) << 16) | (value & 0xFFFFu));
    put(rt, kGuListPointer, list + 4u);
    ctx.gpr[2] = list;
}

// func_08903504(this, x) -> x wrapped by the period (the float at this + 20): x - period * trunc(x / period) when x is above the period,
// x - period * trunc(x / period - 1) when x is negative, else x.
void func_08903504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float period = float_at(rt, ctx.gpr[4] + 20u);
    float x = ctx.fpr[12];
    if (period < x) {
        const float quotient = x / period;
        const auto whole = static_cast<float>(static_cast<std::int32_t>(ctx.fpu_float_to_word(quotient, 1u)));
        x = x - psp_mul(period, whole);
    } else if (x < 0.0f) {
        const float quotient = x / period - 1.0f;
        const auto whole = static_cast<float>(static_cast<std::int32_t>(ctx.fpu_float_to_word(quotient, 1u)));
        x = x - psp_mul(period, whole);
    }
    ctx.fpr[12] = x;
    ctx.fpr[0] = x;
}

// func_08B88FF8(p): marks the string *p unshareable: unless it is the empty string, a positive count (word -4 of the data) first has the string
// copied by func_08B8634C(p, 0, 0, 0), then the count is set to -1. v0 = -1, or 0x08D74FD0 (the empty representation's address) for the empty string.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the empty string, a positive count (the copy call).
void func_08B88FF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4];
    std::uint32_t data = word(rt, p);
    ctx.gpr[2] = 0x08D74FD0u;
    if (data - 12u == 0x08D74FD0u) return;
    if (static_cast<std::int32_t>(word(rt, data - 4u)) > 0) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B8634Cu, {p, 0u, 0u, 0u});
        data = word(rt, p);
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, data - 4u, 0xFFFFFFFFu);
}

// func_08C0ED84(list, pos, x): a 12-byte node (func_08A5BC20(12, 4)) whose holder at +8 is set by func_089FDAC8(node + 8, x) (skipped when
// node + 8 is 0), linked before `pos` (func_08B5C078(node, pos)); v0 = the node.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): node + 8 == 0 (the holder set is skipped).
void func_08C0ED84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pos = ctx.gpr[5], x = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_guest(rt, ctx, 0x08A5BC20u, {12u, 4u});
    if (node + 8u != 0u) (void)call_guest(rt, ctx, 0x089FDAC8u, {node + 8u, x});
    (void)call_guest(rt, ctx, 0x08B5C078u, {node, pos});
    ctx.gpr[2] = node;
}

// func_08B17308(this): when the byte +0x19DD is set, the words +0x19F0 and +0x19EC are cleared, func_08B28C74(this + 0x1984) and
// func_08B28C44(this + 0x1900) run and the byte is cleared; v0 = the last call's result, or 0 when the byte was clear.
void func_08B17308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 6621u);
    if (ctx.gpr[2] == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 6640u, 0u);
    put(rt, self + 6636u, 0u);
    (void)call_guest(rt, ctx, 0x08B28C74u, {self + 6532u});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B28C44u, {self + 6400u});
    rt.memory().store8(self + 6621u, 0u);
}

// func_08B8CF24(this, dest): a local holder is made by func_08A76ABC(local, the word at this[5] + 4); dest takes it (gt::core::holder_reset)
// unless dest is the local itself; the local holder is destroyed.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): dest == the local holder (the reset is skipped).
void func_08B8CF24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A76ABCu, {frame.at(0u), word(rt, self + 20u) + 4u});
    if (destination != frame.at(0u)) (void)call_guest(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
}

// func_08B93CD8(out, s, n): as func_08B85EB8 with func_08B93C20: out = func_08B93C20(s, end, n, s), end = s + func_08B35F60(s, -1) or -1 for a null
// s. Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null string (end = -1).
void func_08B93CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], s = ctx.gpr[5], n = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t end = 0xFFFFFFFFu;
    if (s != 0u) end = s + call_guest(rt, ctx, 0x08B35F60u, {s, 0xFFFFFFFFu});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B93C20u, {s, end, n, s});
    put(rt, out, ctx.gpr[2]);
}

// func_08AD3798(this, x) -> -1, or func_08B1D1BC(this[7], r) once r = func_08B1D080(this[7], x, func_08B1D1F4(x) as a 64-bit value) is not negative.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a negative func_08B1D080 result (answers -1).
void func_08AD3798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    const std::uint32_t target = word(rt, self + 28u);
    GuestFrame frame(ctx, 0x10u);
    const GuestResult pair = call_guest(rt, ctx, 0x08B1D1F4u, {x}, {});
    const std::uint32_t r = call_guest(rt, ctx, 0x08B1D080u, {target, x, pair.v0, pair.v1});
    if (static_cast<std::int32_t>(r) < 0) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B1D1BCu, {word(rt, self + 28u), r});
}

// func_08B64604(p): func_08B35254(p) (the free) unless p is null (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null pointer (returns).
void func_08B64604(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == 0u) return;
    tail_call(ctx, 0x08B35254u, {});
}

// func_08BBCFA0(this): func_08B34B24(this[0] + 108) (tail call) when this[0] is set; v0 = this[0] meanwhile.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[0] == 0 (returns 0).
void func_08BBCFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    tail_call(ctx, 0x08B34B24u, {object + 108u});
}

// func_08BDB2C0(to, from): to = from (two words); the second is a counted object whose count (+4) is incremented; v0 = that word, or 1 after the
// increment. Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null counted object (nothing
// incremented, v0 = 0).
void func_08BDB2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t to = ctx.gpr[4], from = ctx.gpr[5];
    put(rt, to, word(rt, from));
    const std::uint32_t counted = word(rt, from + 4u);
    put(rt, to + 4u, counted);
    ctx.gpr[2] = counted;
    if (counted == 0u) return;
    put(rt, counted + 4u, word(rt, counted + 4u) + 1u);
    ctx.gpr[2] = 1u;
}

// func_08BE7474(this): vtable 0x08CC9628, the weak release of this[9] when set (func_08A5B930), func_08965A3C(this), the free of 60 bytes.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[9] == 0 (no weak release).
void func_08BE7474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t held = word(rt, self + 36u);
    put(rt, self, 0x08CC9628u);
    {
        GuestFrame frame(ctx, 0x10u);
        if (held != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {held});
        (void)call_guest(rt, ctx, 0x08965A3Cu, {self});
    }
    tail_call(ctx, kFree, {self, 60u, 4u});
}

// func_08C1D3AC(first, last): func_08A5B9EC(p) for each word p of [first, last).
void func_08C1D3AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (first == last) return;
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t p = first;
    do {
        ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {p});
        p += 4u;
    } while (last != p);
}

// func_08BEDD64(list): the nodes {next, prev, holder at +8} (12 bytes) other than the head are destroyed one by one (func_08A5B9EC(node + 8), then
// freed).
void func_08BEDD64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4];
    std::uint32_t node = word(rt, head);
    if (node == head) return;
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t next = word(rt, node);
    for (;;) {
        (void)call_guest(rt, ctx, kHolderDestroy, {node + 8u});
        ctx.gpr[2] = call_guest(rt, ctx, kFree, {node, 12u, 4u});
        node = next;
        if (head == next) break;
        next = word(rt, node);
    }
}

// func_08C02614 / func_08B876E0 (first, last): func_08A5B9EC(p) for each word p of [first, last), as func_08C1D3AC.
void func_08C02614(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { func_08C1D3AC(rt, ctx); }
void func_08B876E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { func_08C1D3AC(rt, ctx); }

// func_08BF5ADC(head): frees (func_08A5BC54(node, 12, 4)) each node {next, ...} of the circular list that starts at head[0], up to the head.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty list (the free loop).
void func_08BF5ADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4];
    std::uint32_t node = word(rt, head);
    if (node == head) return;
    GuestFrame frame(ctx, 0x10u);
    for (;;) {
        const std::uint32_t next = word(rt, node);
        ctx.gpr[2] = call_guest(rt, ctx, kFree, {node, 12u, 4u});
        node = next;
        if (head == next) break;
    }
}

// func_08B5C2B0(p) -> *p - 1, after *p = func_08B60804(0x08D6E110, 1) + 1 when *p is 0 (an id taken from a counter on first use).
void func_08B5C2B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4];
    std::uint32_t id = word(rt, p);
    if (id == 0u) {
        GuestFrame frame(ctx, 0x10u);
        id = call_guest(rt, ctx, 0x08B60804u, {0x08D6E110u, 1u}) + 1u;
        put(rt, p, id);
    }
    ctx.gpr[2] = id - 1u;
}

// func_08C30980(first, last, from*, to*): every byte of [first, last) equal to the signed byte *from becomes *to; v0 = the last byte read for the
// comparison (*from) or, after a replacement, *to (unchanged for an empty range).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a byte equal to *from (the replacement).
void func_08C30980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t last = ctx.gpr[5], from = ctx.gpr[6], to = ctx.gpr[7];
    for (std::uint32_t p = ctx.gpr[4]; p != last; ++p) {
        const auto current = static_cast<std::int8_t>(rt.memory().load8(p));
        const auto wanted = static_cast<std::int8_t>(rt.memory().load8(from));
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(wanted));
        if (current == wanted) {
            ctx.gpr[2] = rt.memory().load8(to);
            rt.memory().store8(p, static_cast<std::uint8_t>(ctx.gpr[2]));
        }
    }
}

} // namespace hot_batch_61
} // namespace gt::game

#define HOT61(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_61::name, gt::ReturnKind::kind)
HOT61(0x08C20318u, func_08C20318, Word);
HOT61(0x089098B8u, func_089098B8, Word);
HOT61(0x08B4A82Cu, func_08B4A82C, Void);
HOT61(0x08BEEF60u, func_08BEEF60, Word);
HOT61(0x08BF5C1Cu, func_08BF5C1C, Word);
HOT61(0x08C2BF34u, func_08C2BF34, Word);
HOT61(0x08C1CD74u, func_08C1CD74, Word);
HOT61(0x08C12060u, func_08C12060, Word);
HOT61(0x08B55588u, func_08B55588, Word);
HOT61(0x08903504u, func_08903504, Float);
HOT61(0x08B88FF8u, func_08B88FF8, Word);
HOT61(0x08C0ED84u, func_08C0ED84, Word);
HOT61(0x08B8CF24u, func_08B8CF24, Word);
HOT61(0x08B93CD8u, func_08B93CD8, Word);
HOT61(0x08AD3798u, func_08AD3798, Word);
HOT61(0x08B64604u, func_08B64604, Word);
HOT61(0x08BBCFA0u, func_08BBCFA0, Word);
HOT61(0x08BDB2C0u, func_08BDB2C0, Word);
HOT61(0x08BE7474u, func_08BE7474, Word);
HOT61(0x08C1D3ACu, func_08C1D3AC, Word);
HOT61(0x08BEDD64u, func_08BEDD64, Word);
HOT61(0x08C02614u, func_08C02614, Word);
HOT61(0x08B876E0u, func_08B876E0, Word);
HOT61(0x08BF5ADCu, func_08BF5ADC, Word);
HOT61(0x08B5C2B0u, func_08B5C2B0, Word);
HOT61(0x08C30980u, func_08C30980, Word);
// func_08B17308 is written but not registered: compared 0 in every fast run (no route calls it from outside its generated unit).
// HOT61(0x08B17308u, func_08B17308, Word);
#undef HOT61
