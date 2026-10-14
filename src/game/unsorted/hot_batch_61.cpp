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
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;  // gt::core::holder_destroy
constexpr std::uint32_t kNil = 0x08A76AA0u;            // (holder *out): out = the global nil (src/game/adhoc/array.cpp)

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

// func_08B4A82C(d3, a3, b3): d = a x b (the cross product).
void func_08B4A82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t d = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    const float a0 = float_at(rt, a), a1 = float_at(rt, a + 4u), a2 = float_at(rt, a + 8u);
    const float b0 = float_at(rt, b), b1 = float_at(rt, b + 4u), b2 = float_at(rt, b + 8u);
    put_float(rt, d, psp_mul(a1, b2) - psp_mul(a2, b1));
    put_float(rt, d + 4u, psp_mul(a2, b0) - psp_mul(a0, b2));
    put_float(rt, d + 8u, psp_mul(a0, b1) - psp_mul(a1, b0));
}

// func_08BEEF60(p, q): swaps the words *p and *q (two string data pointers) after resetting a negative count (word +8 of data - 12) to 0 in each; v0 = the old *p.
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

// func_08C1CD74(a, key, p) -> 0 when key is the end node func_08C1CD28(a) answers; else whether the signed word *p is not below the word at the node (1 / 0).
void func_08C1CD74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5], p = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_guest(rt, ctx, 0x08C1CD28u, {ctx.gpr[4]});
    if (key == node) { ctx.gpr[2] = 0u; return; }
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
#undef HOT61
