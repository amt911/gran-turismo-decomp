// Hot batch 63: more functions with branches or loops that the four pad routes reach (call probe as in batches 57-62). As in
// batches 60-62, each function's comment lists the branches the compared cases of the fast gate do not take (found with
// `tools.decomp_gate coverage`); those were compared against the asm by hand, reading the branch TARGET (a value set in a
// delay slot is the result only if the target does not overwrite it). Their classes aren't known yet, so they keep their
// addresses as names. Written from the asm; the generated code was only a reading aid.
//
// Families in this batch (same body, other constants): the range-copy / fill loops over func_08A6972C and func_08A76ABC, the
// "call the virtual destructor of every element" loops (strides 40 / 48), the clamp pair func_08C4045C / func_08C3FEB8 and the
// singleton accessors of an empty ordered container (a function-local static with a guard byte, table below).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <unordered_map>

namespace gt::game {
namespace hot_batch_63 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kConstructHolder = 0x08A6972Cu; // (dest, source): constructs *dest from the element source
constexpr std::uint32_t kAssignHolder = 0x08A76ABCu;    // (dest, source): the same for the holder type of func_08B856F4
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;   // gt::core::holder_destroy
constexpr std::uint32_t kNil = 0x08A76AA0u;             // (holder *out): out = the global nil (src/game/adhoc/array.cpp)
constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu;    // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u;    // (guard)
constexpr std::uint32_t kAtExit = 0x08B77B1Cu;          // (function): registers a destructor for exit
constexpr std::uint32_t kFileState = 0x08D74100u;       // the state block of the sce calls func_08B83C2C & co. pass in a0
constexpr std::uint32_t kFileStateReady = 0x08D740F8u;  // nonzero once that block is set up

// copy_range: for each word-sized slot of [first, last): `if (dest) callee(dest, element)`, the element and the destination advancing by 4;
// returns the destination after the last slot.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty range (returns the destination), a null destination (the call is skipped but the
// destination still advances: the delay slot of the loop's branch), and a range of more than one slot (the loop-back).
std::uint32_t copy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (source != last) {
        do {
            if (destination != 0u) (void)call_guest(rt, ctx, callee, {destination, source});
            source += 4u;
            destination += 4u;
        } while (last != source);
    }
    return destination;
}

// fill_count: `count` times `if (p) callee(p, value)`, p advancing by 4 from `dest`; v0 is the last callee's result (untouched when none ran).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null p (the call is skipped, p still advances in the delay
// slot of the loop branch).
void fill_count(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    const std::uint32_t count = ctx.gpr[5], value = ctx.gpr[6];
    std::uint32_t p = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (p != 0u) ctx.gpr[2] = call_guest(rt, ctx, callee, {p, value});
        p += 4u;
    } while (count != done);
}

// destroy_range: the virtual call vtable[0](element) on each `stride`-byte element of [first, last); v0 is the last call's result.
void destroy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t stride) {
    std::uint32_t element = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    if (element == last) return;
    GuestFrame frame(ctx, 0x10u);
    do {
        const std::uint32_t current = element;
        const std::uint32_t function = word(rt, word(rt, current));
        element += stride;
        ctx.gpr[2] = call_guest(rt, ctx, function, {current});
    } while (last != element);
}

// walk_nodes_virtual (all its branches are taken by the routes): for each node from *a0 to *a1 (a node's word 0 is the next node, word 4 the object), vtable[offset](object, x); v0 = x.
void walk_nodes_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t x = ctx.gpr[6];
    std::uint32_t node = word(rt, ctx.gpr[4]);
    const std::uint32_t end = word(rt, ctx.gpr[5]);
    {
        GuestFrame frame(ctx, 0x10u);
        if (node != end) {
            do {
                const std::uint32_t object = word(rt, node + 4u);
                (void)call_guest(rt, ctx, word(rt, word(rt, object) + offset), {object, x});
                node = word(rt, node);
            } while (node != end);
        }
    }
    ctx.gpr[2] = x;
}

// clamp_step: the two-bound step shared by func_08C4045C and func_08C3FEB8. `limits` holds the bounds (+8 lower, +12 upper) and the
// words +0 / +4 added to the value; the result replaces *p, and is v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the word +4 nonzero while +12 is 0 (loads the lower bound +8 in
// the delay slot of the bne, as when +12 is set), and a lower bound at or above the upper one (r2 = value < lower in the branch's delay slot, then
// word +4 when it holds, else word +0).
void clamp_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t limits = ctx.gpr[4], p = ctx.gpr[5];
    const std::uint32_t x = word(rt, p);
    std::uint32_t result = 0u;
    if (x != 0u) {
        const std::uint32_t upper = word(rt, limits + 12u);
        std::uint32_t lower = 0u;
        bool bounded = true;
        if (upper != 0u) {
            lower = word(rt, limits + 8u);
        } else if (word(rt, limits + 4u) != 0u) {
            lower = word(rt, limits + 8u);
        } else {
            bounded = false;
            result = word(rt, limits) + x;
        }
        if (bounded) {
            std::uint32_t base;
            if (lower < upper) base = x < upper ? word(rt, limits) : word(rt, limits + 4u);
            else base = x < lower ? word(rt, limits + 4u) : word(rt, limits);
            result = base + x;
        }
    }
    put(rt, p, result);
    ctx.gpr[2] = result;
}

// The static of an accessor: an empty ordered container (words +4 / +8 cleared, +12 / +16 pointing at its header +4, +20 = 0) built the first
// time, under the guard byte, with its destructor registered for exit. Returns the container's address.
struct EmptyContainerStatic {
    std::uint32_t guard;
    std::uint32_t object;
    std::uint32_t destructor;
};

const std::unordered_map<std::uint32_t, EmptyContainerStatic> &container_table() {
    static const std::unordered_map<std::uint32_t, EmptyContainerStatic> table = {
        {0x08BF0630u, {0x08D76B08u, 0x08D76B14u, 0x0898AA7Cu}}, {0x08C0919Cu, {0x08D76E70u, 0x08D76E7Cu, 0x089E2ED4u}},
        {0x08C12FF8u, {0x08D77068u, 0x08D77074u, 0x08A08EE8u}}, {0x08C0FAF0u, {0x08D76FF8u, 0x08D77004u, 0x08A027B8u}},
        {0x08C14764u, {0x08D77090u, 0x08D7709Cu, 0x08A0E040u}}, {0x08BF4BE8u, {0x08D76C50u, 0x08D76C78u, 0x089A1564u}},
        {0x08BEF7E8u, {0x08D76AE0u, 0x08D76AECu, 0x089842C4u}}, {0x08BF4B68u, {0x08D76C48u, 0x08D76C60u, 0x089A1574u}},
        {0x08C0EEA8u, {0x08D76FA8u, 0x08D76FB4u, 0x089FEC08u}}, {0x08C0B478u, {0x08D76EE8u, 0x08D76EF4u, 0x089F4630u}},
    };
    return table;
}

} // namespace

// func_08C250A0(first, last, dest) -> dest after the copy: dest[i] (when not null) is built from first[i] by func_08A6972C(dest, element)
// (branches the routes do not take: see copy_range).
void func_08C250A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = copy_range(rt, ctx, kConstructHolder); }

// func_08B856F4(first, last, dest) -> dest after the copy, as func_08C250A0 with func_08A76ABC (branches the routes do not take: see copy_range).
void func_08B856F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = copy_range(rt, ctx, kAssignHolder); }

// func_08C251D0(dest, count, value): `count` times func_08A6972C(p, value), p from dest advancing by 4, a null p skipped.
void func_08C251D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_count(rt, ctx, kConstructHolder); }

// func_08B85AF4(dest, count, value): the same with func_08A76ABC.
void func_08B85AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_count(rt, ctx, kAssignHolder); }

// func_08BE6028(dest, count, source): `count` times (a non-null dest) func_0896418C(dest, source) and then dest[+16] = source[+16], dest advancing by 32;
// v0 = the word read from source + 16 last.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count 0 (nothing changes), a null dest (skipped, dest still advances).
void func_08BE6028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t p = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (p != 0u) {
            (void)call_guest(rt, ctx, 0x0896418Cu, {p, source});
            ctx.gpr[2] = word(rt, source + 16u);
            put(rt, p + 16u, ctx.gpr[2]);
        }
        p += 32u;
    } while (count != done);
}

// func_08C0D7F8(first, last, dest) -> dest after the copy: for each 32-byte element, func_0896418C(dest, element) and dest[+16] = element[+16] (a null dest skipped).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null dest (skipped; the source and the destination still advance).
void func_08C0D7F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (source != last) {
        do {
            const std::uint32_t current = source;
            source += 32u;
            if (destination != 0u) {
                (void)call_guest(rt, ctx, 0x0896418Cu, {destination, current});
                ctx.gpr[2] = word(rt, current + 16u);
                put(rt, destination + 16u, ctx.gpr[2]);
            }
            destination += 32u;
        } while (source != last);
    }
    ctx.gpr[2] = destination;
}

// func_08C2AED8(first, last, dest) -> dest after the copy: for each 12-byte element, func_08A6972C(dest, element), dest[+4] = element[+4],
// func_08A76ABC(dest + 8, element + 8) (a null dest skipped).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null dest (skipped; both pointers still advance).
void func_08C2AED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (source != last) {
        do {
            if (destination != 0u) {
                (void)call_guest(rt, ctx, kConstructHolder, {destination, source});
                ctx.gpr[2] = word(rt, source + 4u);
                put(rt, destination + 4u, ctx.gpr[2]);
                ctx.gpr[2] = call_guest(rt, ctx, kAssignHolder, {destination + 8u, source + 8u});
            }
            source += 12u;
            destination += 12u;
        } while (source != last);
    }
    ctx.gpr[2] = destination;
}

// func_08C3EACC(end*, begin*, x) -> x: while *begin != *end, the object stored 8 bytes below *end gets its vtable[0](object, x) and *end moves down by 8.
void func_08C3EACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t end_slot = ctx.gpr[4], begin_slot = ctx.gpr[5], x = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        std::uint32_t current = word(rt, end_slot);
        if (current != word(rt, begin_slot)) {
            do {
                const std::uint32_t object = word(rt, current - 8u);
                (void)call_guest(rt, ctx, word(rt, word(rt, object)), {object, x});
                current = word(rt, end_slot) - 8u;
                put(rt, end_slot, current);
            } while (word(rt, begin_slot) != current);
        }
    }
    ctx.gpr[2] = x;
}

// func_08C3F42C(begin*, end*, x) -> x: the nodes from *begin to *end, vtable[12](object, x) on each node's object (word +4).
void func_08C3F42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { walk_nodes_virtual(rt, ctx, 12u); }

// func_08C3F490(begin*, end*, x) -> x: the same with vtable[0].
void func_08C3F490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { walk_nodes_virtual(rt, ctx, 0u); }

// func_088A964C(this): v0 = 0, or vtable[32] of the child this[2] called on it (a1 passed on) when it is set.
void func_088A964C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 8u);
    ctx.gpr[2] = 0u;
    if (child == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, child) + 32u), {child, ctx.gpr[5]});
}

// func_08BDADF8(this): v0 = 0, or vtable[8] of the child this[5] called on it (a1 passed on) when it is set.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null child (v0 = 0).
void func_08BDADF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 20u);
    ctx.gpr[2] = 0u;
    if (child == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, child) + 8u), {child, ctx.gpr[5]});
}

// func_089BB1E8(this): the flag word +96 loses bit 8 and gains bit 9; then, when func_089BAF28(this) answers nonzero, func_089BAF40(this, this[22] + 4)
// (tail call); else v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_089BAF28 answering 0 (v0 = 0, no tail call).
void func_089BB1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self + 96u, (word(rt, self + 96u) & ~0x100u) | 0x200u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089BAF28u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    tail_call(ctx, 0x089BAF40u, {self, word(rt, self + 88u) + 4u});
}

// func_08B5F100(node) -> the next node in order of a red-black tree (parent +4, left +8, right +12; the header's right is the leftmost):
// the leftmost node of the right subtree, else up while the node is its parent's right child (the node itself for the end).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a node that is not its parent's right child and has
// no parent (answers the node itself).
void func_08B5F100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    const std::uint32_t right = word(rt, node + 12u);
    if (right != 0u) {
        std::uint32_t leftmost = right;
        for (std::uint32_t left = word(rt, leftmost + 8u); left != 0u; left = word(rt, leftmost + 8u)) leftmost = left;
        ctx.gpr[2] = leftmost;
        return;
    }
    std::uint32_t parent = word(rt, node + 4u);
    if (word(rt, parent + 12u) != node) {
        // The node is not its parent's right child: the answer is the parent, or the node itself when it has none.
        ctx.gpr[2] = parent == 0u ? node : parent;
        return;
    }
    std::uint32_t child = parent;
    parent = word(rt, child + 4u);
    while (word(rt, parent + 12u) == child) {
        child = parent;
        parent = word(rt, child + 4u);
    }
    // The header case: the climb ends on a node whose right child is the parent itself.
    ctx.gpr[2] = word(rt, child + 12u) == parent ? child : parent;
}

// func_08B590D0(): func_08C7AC48(0); then the object in the graphics state's word +52 (when set) is passed to func_08C7AC20(object, 0), and the word is
// cleared. v0 = that call's result, or 0.
void func_08B590D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08C7AC48u, {0u});
    const std::uint32_t object = word(rt, kGuState + 52u);
    ctx.gpr[2] = object;
    if (object != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08C7AC20u, {object, 0u});
    put(rt, kGuState + 52u, 0u);
}

// func_08B5908C(a, b): the graphics state takes +84 = +92 = a, +88 = b, +96 = a + b, +720 = 0; when the word +100 is nonzero, a memory barrier and
// func_08C7AAD8(a, b) (tail call); else v0 = the state's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the word +100 zero (returns; v0 stays the state's address, which
// the function leaves in r2).
void func_08B5908C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    const std::uint32_t enabled = word(rt, kGuState + 100u);
    put(rt, kGuState + 96u, a + b);
    put(rt, kGuState + 84u, a);
    put(rt, kGuState + 88u, b);
    put(rt, kGuState + 92u, a);
    put(rt, kGuState + 720u, 0u);
    ctx.gpr[2] = kGuState;
    if (enabled == 0u) return;
    rt.memory().memory_barrier();
    tail_call(ctx, 0x08C7AAD8u, {a, b});
}

// func_08B58E28(a, b, c, d): the graphics state remembers (+128 = a, +132 = b, halves +148 = c, +150 = d, +152 = (c + 63) & ~63); then
// func_08C7AC08(a, (c + 63) & ~63, b, changed) (tail call), changed being 0 only when the old +132 was b and the old half +152 the new aligned value.
void func_08B58E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7];
    const std::uint32_t aligned = (c + 63u) & ~63u;
    const bool same = word(rt, kGuState + 132u) == b && rt.memory().load16(kGuState + 152u) == aligned;
    rt.memory().store16(kGuState + 148u, static_cast<std::uint16_t>(c));
    rt.memory().store16(kGuState + 150u, static_cast<std::uint16_t>(d));
    rt.memory().store16(kGuState + 152u, static_cast<std::uint16_t>(aligned));
    put(rt, kGuState + 128u, a);
    put(rt, kGuState + 132u, b);
    tail_call(ctx, 0x08C7AC08u, {a, aligned, b, same ? 0u : 1u});
}

// func_08B83C84(a, b, c): 0x80420100 while the file block's ready word (0x08D740F8) is 0; 0x80420005 for a == 0 or a not 64-byte aligned;
// else func_08C7B058(block, a, b, c).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the ready word 0 and a null a (both return
// the constants).
void func_08B83C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    if (word(rt, kFileStateReady) == 0u) {
        ctx.gpr[2] = 0x80420100u;
        return;
    }
    ctx.gpr[2] = 0x80420005u;
    if (a == 0u || (a & 63u) != 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B058u, {kFileState, a, b, c});
}

// func_08B83C2C(a): as func_08B83C84 with one argument: func_08C7B0A0(block, a).
void func_08B83C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    if (word(rt, kFileStateReady) == 0u) {
        ctx.gpr[2] = 0x80420100u;
        return;
    }
    ctx.gpr[2] = 0x80420005u;
    if (a == 0u || (a & 63u) != 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B0A0u, {kFileState, a});
}

// func_08B8421C(a, b, c): 0x80420100 while the ready word is 0, else func_08C7B0C0(block, a, b, c).
void func_08B8421C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    if (word(rt, kFileStateReady) == 0u) {
        ctx.gpr[2] = 0x80420100u;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B0C0u, {kFileState, a, b, c});
}

// func_08C21B34(this, key): holder = func_08A6159C(key, 2); object = holder[1] (func_08A5BB7C(0x08CB0284) first when null); vtable[49](object, holder);
// then func_08A61E3C(key, holder) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder[1] (the func_08A5BB7C call, and the
// call goes on with the null object).
void func_08C21B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t holder;
    {
        GuestFrame frame(ctx, 0x10u);
        holder = call_guest(rt, ctx, 0x08A6159Cu, {key, 2u});
        const std::uint32_t object = word(rt, holder + 4u);
        if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 196u), {object, holder});
    }
    tail_call(ctx, 0x08A61E3Cu, {key, holder});
}

// func_08C1FF70(this, key): holder = func_08A6159C(key, 2); object = the word at func_08A61594(key) (func_08A5BB7C(0x08CB0284) first when null);
// then vtable[40](object, holder, holder + 4) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null object (the func_08A5BB7C call, and the
// tail call goes on with the null object).
void func_08C1FF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t holder, object;
    {
        GuestFrame frame(ctx, 0x10u);
        holder = call_guest(rt, ctx, 0x08A6159Cu, {key, 2u});
        object = word(rt, call_guest(rt, ctx, 0x08A61594u, {key}));
        if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    }
    tail_call(ctx, word(rt, word(rt, object) + 160u), {object, holder, holder + 4u});
}

// func_08B87774(this, a, b): a local holder (func_08A76AA0) is passed to this->vtable[6](holder, a, 1, b), then destroyed; v0 = the destroy's result.
void func_08B87774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, kNil, {frame.at(0u)});
    (void)call_guest(rt, ctx, word(rt, self + 24u), {frame.at(0u), a, 1u, b});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
}

// func_08C4045C(limits, p) / func_08C3FEB8(limits, p): *p = 0 for a zero *p; else *p = *p + (limits word +0 or +4, chosen by comparing *p with the bounds
// +8 and +12); v0 = the new *p. Unsigned compares. Branches the routes do not take: see clamp_step.
void func_08C4045C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C3FEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }

// func_08B522BC(p, a, b, c, d) -> *p after the update: 0 for a zero *p; else *p += one of a / c picked by the bounds b and d (a for the plain case
// d == 0 && c == 0; unsigned compares).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): *p == 0 (answers 0); b >= d with d set; *p >= d; and d == 0 with
// c set (the slt / movz chains at 0x08B522E0-0x08B522F8, delay slots included).
void func_08B522BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7], d = ctx.gpr[8];
    const std::uint32_t value = word(rt, p);
    std::uint32_t result = 0u;
    if (value != 0u) {
        if (d != 0u) {
            if (b < d) result = (value < d ? a : c) + value;
            else result = (value < b ? c : a) + value;
        } else if (c == 0u) {
            result = value + a;
        } else {
            result = (value < b ? c : a) + value;
        }
    }
    put(rt, p, result);
    ctx.gpr[2] = result;
}

// func_08BE5C0C & co. (first, last): vtable[0](element) on each element of [first, last), the elements `stride` bytes apart (40 / 48).
void func_08BE5C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 40u); }
void func_08BE593C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 40u); }
void func_08BE5BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 48u); }

// get_empty_container(): the accessor of a function-local static container. While its guard byte is 0 and func_08B6441C(guard) lets this caller build it,
// the container is made empty (+16 = +12 = its header at +4; +4, +8, +20 cleared), func_08B64494(guard) releases the guard and func_08B77B1C(destructor)
// registers the destructor for exit. v0 = the container's address. The three constants of each instance are in container_table().
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (another caller is
// building it: only the address is returned).
void get_empty_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const EmptyContainerStatic &s = container_table().at(ctx.pc);
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(s.guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {s.guard}) != 0u) {
            put(rt, s.object + 16u, s.object + 4u);
            put(rt, s.object + 20u, 0u);
            put(rt, s.object + 4u, 0u);
            put(rt, s.object + 8u, 0u);
            put(rt, s.object + 12u, s.object + 4u);
            (void)call_guest(rt, ctx, kGuardRelease, {s.guard});
            (void)call_guest(rt, ctx, kAtExit, {s.destructor});
        }
    }
    ctx.gpr[2] = s.object;
}

} // namespace hot_batch_63
} // namespace gt::game

#define HOT63(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_63::name, gt::ReturnKind::kind)
// Written but not registered: func_088A964C is compared 0 (no route calls it: calls 0); func_08B590D0, func_08B58E28, func_08B83C84, func_08B83C2C and
// func_08B8421C are all "not comparable" (every case reaches a Sony import stub, func_08C7AC48 / 08C7AC08 / 08C7B058 / 08C7B0A0 / 08C7B0C0, that the
// harness can neither replay nor record).
HOT63(0x08C250A0u, func_08C250A0, Word);
HOT63(0x08B856F4u, func_08B856F4, Word);
HOT63(0x08C251D0u, func_08C251D0, Word);
HOT63(0x08B85AF4u, func_08B85AF4, Word);
HOT63(0x08BE6028u, func_08BE6028, Word);
HOT63(0x08C0D7F8u, func_08C0D7F8, Word);
HOT63(0x08C2AED8u, func_08C2AED8, Word);
HOT63(0x08C3EACCu, func_08C3EACC, Word);
HOT63(0x08C3F42Cu, func_08C3F42C, Word);
HOT63(0x08C3F490u, func_08C3F490, Word);
// HOT63(0x088A964Cu, func_088A964C, Word);
HOT63(0x08BDADF8u, func_08BDADF8, Word);
HOT63(0x089BB1E8u, func_089BB1E8, Word);
HOT63(0x08B5F100u, func_08B5F100, Word);
// HOT63(0x08B590D0u, func_08B590D0, Word);
HOT63(0x08B5908Cu, func_08B5908C, Word);
// HOT63(0x08B58E28u, func_08B58E28, Word);
// HOT63(0x08B83C84u, func_08B83C84, Word);
// HOT63(0x08B83C2Cu, func_08B83C2C, Word);
// HOT63(0x08B8421Cu, func_08B8421C, Word);
HOT63(0x08C21B34u, func_08C21B34, Word);
HOT63(0x08C1FF70u, func_08C1FF70, Word);
HOT63(0x08B87774u, func_08B87774, Word);
HOT63(0x08C4045Cu, func_08C4045C, Word);
HOT63(0x08C3FEB8u, func_08C3FEB8, Word);
HOT63(0x08B522BCu, func_08B522BC, Word);
HOT63(0x08BE5C0Cu, func_08BE5C0C, Word);
HOT63(0x08BE593Cu, func_08BE593C, Word);
HOT63(0x08BE5BC0u, func_08BE5BC0, Word);
#define HOT63_CONTAINER(address) HOT63(address, get_empty_container, Word)
HOT63_CONTAINER(0x08BF0630u);
HOT63_CONTAINER(0x08C0919Cu);
HOT63_CONTAINER(0x08C12FF8u);
HOT63_CONTAINER(0x08C0FAF0u);
HOT63_CONTAINER(0x08C14764u);
HOT63_CONTAINER(0x08BF4BE8u);
HOT63_CONTAINER(0x08BEF7E8u);
HOT63_CONTAINER(0x08BF4B68u);
HOT63_CONTAINER(0x08C0EEA8u);
HOT63_CONTAINER(0x08C0B478u);
#undef HOT63_CONTAINER
#undef HOT63
