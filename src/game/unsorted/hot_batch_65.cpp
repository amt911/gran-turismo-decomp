// Hot batch 65: more small functions with branches or loops that the five pad routes reach (call probe as in batches 57-64, now with the
// options route). As in batches 60-64, each function's comment lists the branches the compared cases of the fast gate do not take
// (found with `tools.decomp_gate coverage`); those were compared against the asm by hand, reading the branch TARGET (a value set in a
// delay slot is the result only if the target does not overwrite it). Their classes aren't known yet, so they keep their addresses as
// names. Written from the asm; the generated code was only a reading aid (it gives the relocated constants).
//
// Families: pointer-to-member-function call thunks, "call a function on every element of a range" loops (strides 8 / 0x30 / 0x40 / 0x60),
// element copy loops that skip a null destination, the "move the three words out of a smart pointer, call the erase function, free the
// node" quartet, the destructors that release a holder through its vtable, and the "release the holder, then operator delete" tails.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3): the ones it leaves alone keep the
// caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_65 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kDelete = 0x08B64654u;  // (object): operator delete
constexpr std::uint32_t kNodeFree = 0x08B35254u; // (node): frees / releases one node

// vtable[0](holder, value): the release call every holder-owning object makes on its holder.
std::uint32_t release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, std::uint32_t value) {
    return call_guest(rt, ctx, word(rt, word(rt, holder)), {holder, value});
}

// (ptmf, this, argument): a pointer to member function {+4 function or vtable offset, +8 this adjustment * 2 | virtual flag}.
// Calls it with a0 = this + adjustment, a1 = argument (a2 / a3 as the caller had them); v0 = its result.
void call_member_pointer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ptmf = ctx.gpr[4], self = ctx.gpr[5], argument = ctx.gpr[6];
    const std::uint32_t tagged = word(rt, ptmf + 8u);
    const std::uint32_t target_this = self + static_cast<std::uint32_t>(static_cast<std::int32_t>(tagged) >> 1);
    std::uint32_t function;
    if ((tagged & 1u) != 0u) {
        const std::uint32_t offset = word(rt, ptmf + 4u);
        function = word(rt, word(rt, target_this) + offset);
    } else {
        function = word(rt, ptmf + 4u);
    }
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, function, {target_this, argument});
}

// callee(p) on every 8-byte slot of [first, last); v0 = the last result (untouched for an empty range).
void call_each_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    std::uint32_t p = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (p == last) return;
    do {
        const std::uint32_t current = p;
        p += 8u;
        ctx.gpr[2] = call_guest(rt, ctx, callee, {current});
    } while (p != last);
}

// The virtual call vtable[0](element) on each `stride`-byte element of [first, last); v0 = the last call's result.
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

// callee(p) for `count` slots of `stride` bytes from `first`, then a tail call of `then` with a0 = this.
void call_each_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first_offset, std::uint32_t stride,
                    std::uint32_t count, std::uint32_t callee, std::uint32_t then) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t p = self + first_offset;
    {
        GuestFrame frame(ctx, 0x10u);
        for (std::uint32_t i = 0; i < count; ++i) {
            ctx.gpr[2] = call_guest(rt, ctx, callee, {p});
            p += stride;
        }
    }
    tail_call(ctx, then, {self});
}

// (box, a1): takes the three words {+0, +4, +8} out of *box (clearing them), builds {+4: first, +8: second, +0xC: third} in a frame and calls
// callee(first, second, &frame); then, when the frame's word at +4 is not null, kNodeFree(that word). v0 = that word (0: no call) or the call's result.
void drain_box(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    const std::uint32_t box = ctx.gpr[4];
    const std::uint32_t first = word(rt, box), second = word(rt, box + 4u), third = word(rt, box + 8u);
    put(rt, box + 8u, 0u);
    put(rt, box, 0u);
    put(rt, box + 4u, 0u);
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0xCu), third);
    put(rt, frame.at(4u), first);
    put(rt, frame.at(8u), second);
    (void)call_guest(rt, ctx, callee, {first, second, frame.at(0u)});
    const std::uint32_t result = word(rt, frame.at(4u));
    ctx.gpr[2] = result;
    if (result != 0u) ctx.gpr[2] = call_guest(rt, ctx, kNodeFree, {result});
}

// The destructor of an object that owns a holder: this[0] = first vtable; when the holder this[value + 4] is set, vtable[0](holder, this[value])
// and both words cleared; this[0] = final vtable (= v0).
void release_and_reset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t value_offset, std::uint32_t first_table,
                       std::uint32_t final_table) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, first_table);
    const std::uint32_t holder = word(rt, self + value_offset + 4u);
    if (holder != 0u) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)release_holder(rt, ctx, holder, word(rt, self + value_offset));
        }
        put(rt, self + value_offset, 0u);
        put(rt, self + value_offset + 4u, 0u);
    }
    put(rt, self, final_table);
    ctx.gpr[2] = final_table;
}

// Erases the subtree under `node` (right subtrees by recursion on the child at +0xC, then along the chain at +8), freeing each node.
void erase_tree(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_address) {
    const std::uint32_t tree = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    do {
        (void)call_guest(rt, ctx, self_address, {tree, word(rt, node + 0xCu)});
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[2] = call_guest(rt, ctx, kNodeFree, {node});
        node = next;
    } while (node != 0u);
}

// clamp_step (also in batch 63): `limits` holds the bounds (+8 lower, +12 upper) and the words +0 / +4 added to the value; *p becomes 0 for a zero
// *p, else *p + (word +0 or +4 chosen by comparing *p, unsigned, with the bounds); the result is v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): *p != 0 with the upper bound +12 equal to 0 (the word +4 nonzero: the lower
// bound +8 is loaded in the delay slot of the bne and the compares go on as with +12 set; the word +4 zero: *p = word +0 + *p, v0 the same, the lw / addu sitting in the
// j's delay slot) and a lower bound at or above the upper one (sltu of *p with the lower bound, then word +4 when it holds, else word +0), as for func_08C4045C of batch 63.
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

// `count` times `if (p) callee(p, value)`, p advancing by 4 from a0; v0 is the last callee's result (untouched when none ran).
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

} // namespace

// func_08BF8BFC / func_08BF8C48 / func_08C09730(ptmf, this, argument): calls the pointer to member function (see call_member_pointer).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the virtual side of the flag (bit 0 of ptmf[2]) in all three: the function is
// then *(vtable of this + adjustment + ptmf[1]), the load sitting in the delay slot of the j that joins the two sides; the delay slot of the jalr moves a2 into a1 on both,
// and the delay slot of the beqz computes this + adjustment on both.
void func_08BF8BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_pointer(rt, ctx); }
void func_08BF8C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_pointer(rt, ctx); }
void func_08C09730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_pointer(rt, ctx); }

// func_08B8E594 / func_08B8E5D8(first, last): func_0888879C / func_088875CC(p) on every 8-byte slot of [first, last).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): every range the routes pass is empty (returns at once, v0 untouched): the loop (callee(p) with a0 = the slot, the
// slot advancing by 8 in the jal's delay slot, back while it differs from last) is checked against the asm only.
void func_08B8E594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_each_slot(rt, ctx, 0x0888879Cu); }
void func_08B8E5D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_each_slot(rt, ctx, 0x088875CCu); }

// func_08C15F9C(first, last, dest) -> dest after the copy: each 16-byte element (four words, all read before any is written) of [first, last)
// goes to dest (a null dest only advancing). An empty range returns dest.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty range (v0 = a2 from the delay slot of the first beq), a null dest (the copy is skipped, dest still advances in the delay slot of the loop branch).
void func_08C15F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    ctx.gpr[2] = destination;
    if (source == last) return;
    do {
        const std::uint32_t current = source;
        source += 16u;
        if (destination != 0u) {
            const std::uint32_t w3 = word(rt, current + 12u), w0 = word(rt, current), w1 = word(rt, current + 4u), w2 = word(rt, current + 8u);
            put(rt, destination, w0);
            put(rt, destination + 4u, w1);
            put(rt, destination + 8u, w2);
            put(rt, destination + 12u, w3);
        }
        destination += 16u;
    } while (source != last);
    ctx.gpr[2] = destination;
}

// func_08BE58F0 / func_08BE5988 / func_08BE59D4(first, last): the virtual call vtable[0](element) on each element of [first, last), strides 0x30 / 0x40 / 0x60.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): every range the routes pass is empty (returns at once, v0 untouched): the loop (vtable[0](element) with the vtable loaded in
// the branch-likely's delay slot, the element advancing by the stride, back while it differs from last) is checked against the asm only.
void func_08BE58F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x30u); }
void func_08BE5988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x40u); }
void func_08BE59D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x60u); }

// func_08B3EB80(this): func_08B3E73C(p) for 32 slots of 0x18 bytes from this, then a tail call of func_08B3EAE4(this).
void func_08B3EB80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_each_then(rt, ctx, 0u, 0x18u, 32u, 0x08B3E73Cu, 0x08B3EAE4u); }
// func_08C3DECC(this): func_08AD2FE8(p) for 128 slots of 0x7C bytes from this + 0xC, then a tail call of func_08C3DAA0(this).
void func_08C3DECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_each_then(rt, ctx, 0xCu, 0x7Cu, 128u, 0x08AD2FE8u, 0x08C3DAA0u); }

// func_08C4B50C(dest, count, source): `count` times (a non-null dest) copies the six words at source to dest (all read first), dest advancing by 0x18.
// v0 = the first word read last (untouched when nothing was copied).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count 0 (returns at once), a null dest (skipped, the counter and dest still advance in the delay slots; the routes always pass a set dest).
void func_08C4B50C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t p = ctx.gpr[4];
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (p != 0u) {
            std::uint32_t w[6];
            for (unsigned i = 0; i < 6u; ++i) w[i] = word(rt, source + 4u * i);
            ctx.gpr[2] = w[0];
            for (unsigned i = 0; i < 6u; ++i) put(rt, p + 4u * i, w[i]);
        }
        p += 0x18u;
    } while (count != done);
}

// func_08C4B568(first, last, dest) -> dest after the copy: each 0x18-byte element of [first, last) goes to dest (read all six words first), a null dest only advancing.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty range (v0 = dest from the delay slot of jr), a null dest (the copy is skipped, dest still advances in the delay slot of the loop branch).
void func_08C4B568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            const std::uint32_t current = source;
            source += 0x18u;
            if (destination != 0u) {
                std::uint32_t w[6];
                w[5] = word(rt, current + 0x14u);
                for (unsigned i = 0; i < 5u; ++i) w[i] = word(rt, current + 4u * i);
                for (unsigned i = 0; i < 6u; ++i) put(rt, destination + 4u * i, w[i]);
            }
            destination += 0x18u;
        } while (source != last);
    }
    ctx.gpr[2] = destination;
}

// func_08C4B35C(dest, count, source): `count` times (a non-null dest) copies the 0x70 bytes at source to dest in 16-byte blocks (four words read, then four
// written), dest advancing by 0x70. v0 = the first word of the last block read (untouched when nothing was copied).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count 0 (returns at once), a null dest (the beqzl takes its delay slot: the counter advances, the copy is skipped; the routes always pass a set dest).
void func_08C4B35C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t p = ctx.gpr[4];
    if (count == 0u) return;
    for (std::uint32_t done = 1; ; ++done) {
        if (p != 0u) {
            for (std::uint32_t block = 0; block < 0x70u; block += 16u) {
                const std::uint32_t w0 = word(rt, source + block), w1 = word(rt, source + block + 4u), w2 = word(rt, source + block + 8u),
                                    w3 = word(rt, source + block + 12u);
                ctx.gpr[2] = w0;
                put(rt, p + block, w0);
                put(rt, p + block + 4u, w1);
                put(rt, p + block + 8u, w2);
                put(rt, p + block + 12u, w3);
            }
        }
        p += 0x70u;
        if (done == count) break;
    }
}

// func_08C4B740 / func_08C4B7A0 / func_08C4B800 / func_08C4B860(box): see drain_box, callees func_08C498B4 / func_08C498C4 / func_08C498D4 / func_08C499B0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the frame's word +4 left null by the callee (v0 = 0, no free).
void func_08C4B740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { drain_box(rt, ctx, 0x08C498B4u); }
void func_08C4B7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { drain_box(rt, ctx, 0x08C498C4u); }
void func_08C4B800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { drain_box(rt, ctx, 0x08C498D4u); }
void func_08C4B860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { drain_box(rt, ctx, 0x08C499B0u); }

// func_0886FCA4 / func_0887172C (value at this + 0xC, holder at this + 0x10) and func_0887AD7C / func_0887C644 (value at this + 4, holder at this + 8): see release_and_reset.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder (no call, the words stay; the final table store and v0 are the same); a set holder is taken.
void func_0886FCA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_reset(rt, ctx, 0xCu, 0x08CBF868u, 0x08CBF428u); }
void func_0887172C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_reset(rt, ctx, 0xCu, 0x08CBF9A0u, 0x08CBF428u); }
void func_0887AD7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_reset(rt, ctx, 4u, 0x08CBFDA8u, 0x08CBFDE8u); }
void func_0887C644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_reset(rt, ctx, 4u, 0x08CBFE88u, 0x08CBFEB0u); }

// func_08B93D3C(tree, node) / func_08C1CC2C(tree, node): see erase_tree (each calls itself).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null node (returns at once, v0 untouched).
void func_08B93D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_tree(rt, ctx, 0x08B93D3Cu); }
void func_08C1CC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_tree(rt, ctx, 0x08C1CC2Cu); }

// func_08AB93B4(this): v0 = the byte this[0xC]; when set, kNodeFree(this[4]) and this[8] = this[4] = this[0xC] = 0 (the call's result is v0); this[0xD] = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the byte set (the routes only see 0: v0 = 0, only this[0xD] cleared). The set side (kNodeFree(this[4]), then
// sw this[8] = 0, sw this[4] = 0, sb this[0xC] = 0) is checked against the asm only.
void func_08AB93B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = rt.memory().load8(self + 0xCu);
    ctx.gpr[2] = flag;
    if (flag != 0u) {
        {
            GuestFrame frame(ctx, 0x10u);
            ctx.gpr[2] = call_guest(rt, ctx, kNodeFree, {word(rt, self + 4u)});
        }
        put(rt, self + 8u, 0u);
        put(rt, self + 4u, 0u);
        rt.memory().store8(self + 0xCu, 0u);
    }
    rt.memory().store8(self + 0xDu, 0u);
}

// func_0890AFC0(this): when the holder this[1] is set, vtable[0](holder, this[0]) and both words cleared. v0 = the call's result (untouched without a holder).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder (the beqz skips the call and both clears; v0 untouched).
void func_0890AFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = word(rt, self + 4u);
    if (holder == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = release_holder(rt, ctx, holder, word(rt, self));
    }
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// func_08BDAF18(a0, this): nothing when this is null; else (for a set holder this[1]) vtable[0](holder, this[0]) and both words cleared, then operator delete(this) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null this (returns, v0 untouched), a null holder (the call and both clears are skipped, then operator delete(this)); the routes always pass a set holder.
void func_08BDAF18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    if (self == 0u) return;
    const std::uint32_t holder = word(rt, self + 4u);
    if (holder != 0u) {
        {
            GuestFrame frame(ctx, 0x10u);
            ctx.gpr[2] = release_holder(rt, ctx, holder, word(rt, self));
        }
        put(rt, self + 4u, 0u);
        put(rt, self, 0u);
    }
    tail_call(ctx, kDelete, {self});
}

// func_08AD0700(this): func_08B1A3F4(this + 0x368); then, for a set holder this[0x364 / 4], vtable[0](holder, this[0x360 / 4]) and both words cleared; tail call of func_08B0C808(this + 0x248).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a set holder (the routes see only a null one: the virtual call vtable[0](holder, this[0x360 / 4]) and the two clears are checked against the asm only).
void func_08AD0700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B1A3F4u, {self + 0x368u});
        const std::uint32_t holder = word(rt, self + 0x364u);
        if (holder != 0u) {
            ctx.gpr[2] = release_holder(rt, ctx, holder, word(rt, self + 0x360u));
            put(rt, self + 0x364u, 0u);
            put(rt, self + 0x360u, 0u);
        }
    }
    tail_call(ctx, 0x08B0C808u, {self + 0x248u});
}

// func_088824F8(this, vec): the object o = this[3]. A non-null vec sets bit 4 of o[0x16E0 / 4] and copies its four floats to o + 0xDA8 / 0xE60 / 0xF18 / 0xFD0
// (o re-read before every store, as the original does); a null vec clears that bit. v0 = o (last re-read) or the new flag word.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null vec (the flag cleared by ins in the branch's target; v0 = the new flag word).
void func_088824F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], vec = ctx.gpr[5];
    if (vec == 0u) {
        const std::uint32_t object = word(rt, self + 12u);
        const std::uint32_t flags = word(rt, object + 0x16E0u) & ~0x10u;
        ctx.gpr[2] = flags;
        ctx.gpr[3] = object;
        put(rt, object + 0x16E0u, flags);
        return;
    }
    std::uint32_t object = word(rt, self + 12u);
    put(rt, object + 0x16E0u, word(rt, object + 0x16E0u) | 0x10u);
    std::uint32_t value = word(rt, vec);
    object = word(rt, self + 12u);
    put(rt, object + 0xDA8u, value);
    object = word(rt, self + 12u);
    value = word(rt, vec + 4u);
    put(rt, object + 0xE60u, value);
    value = word(rt, vec + 8u);
    object = word(rt, self + 12u);
    put(rt, object + 0xF18u, value);
    object = word(rt, self + 12u);
    value = word(rt, vec + 12u);
    put(rt, object + 0xFD0u, value);
    ctx.gpr[2] = object;
}

// func_08B5C230() -> the word at 0x08D6E118, after func_08B5C218() has run when it was 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the word nonzero (returns it at once; the routes always find it 0 and take the init call, so the early return is checked against the asm only).
void func_08B5C230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLazyWord = 0x08D6E118u;
    ctx.gpr[2] = word(rt, kLazyWord);
    if (ctx.gpr[2] != 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B5C218u, {});
    }
    ctx.gpr[2] = word(rt, kLazyWord);
}

// func_08C2016C(a0, a1): r = func_08A61594(a1); a non-null object *r is the target of a tail call of its vtable method at offset 0xC0 (a0 = *r, a1 = r).
// Without an object v0 = r.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): *r null (v0 = r, returns; the routes always find an object).
void func_08C2016C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A61594u, {ctx.gpr[5]});
    }
    ctx.gpr[2] = result;
    const std::uint32_t object = word(rt, result);
    ctx.gpr[3] = object;
    if (object == 0u) return;
    const std::uint32_t table = word(rt, object);
    const std::uint32_t method = word(rt, table + 0xC0u);
    ctx.gpr[2] = table;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {object, result});
}

// func_0895FE28(this): func_0894419C(this); a non-null this also func_0895FE5C(this); then a tail call of func_0894F438(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null this (the second call is skipped; a0 = this still goes to the tail call from the delay slot; the routes always pass a set this).
void func_0895FE28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x0894419Cu, {self});
        if (self != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x0895FE5Cu, {self});
    }
    tail_call(ctx, 0x0894F438u, {self});
}

// func_08B516F4(this): this[0] = 0x08CDAC58; the child this[0xC20 / 4], if set, gets its vtable method at offset 4 called (a0 = child); then operator delete(this) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null child (no call; the routes always have a child).
void func_08B516F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t child = word(rt, self + 0xC20u);
    put(rt, self, 0x08CDAC58u);
    if (child != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, child) + 4u), {child});
    }
    tail_call(ctx, kDelete, {self});
}

// func_08BDADAC(a0, this): nothing when this is null; else this[2] = 0x08CC57B0, func_08AF20E8(this + 8), then operator delete(this) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null this (returns, v0 untouched).
void func_08BDADAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    if (self == 0u) return;
    put(rt, self + 8u, 0x08CC57B0u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF20E8u, {self + 8u});
    }
    tail_call(ctx, kDelete, {self});
}

// func_0894CEB8(this): this[0] = 0x08CC6818 (also when this[1] is null); a set this[1] calls func_0894CE2C(this); then operator delete(this) (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[1] set (the routes see only null: the call func_0894CE2C(this) is checked against the asm only; the store of this[0] sits in the delay slot of the beqz so it happens either way).
void func_0894CEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t member = word(rt, self + 4u);
    put(rt, self, 0x08CC6818u);
    if (member != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x0894CE2Cu, {self});
    }
    tail_call(ctx, kDelete, {self});
}

// func_0886F1B4(this): object = this[0x70 / 4]; list = object[0x130 / 4]; when list[-3] (the word at list - 0xC) is not 0, func_0886B9A4(this[7], 0, list);
// then a tail call of func_0886F0E0(object, 0) (a0 = this[0x70 / 4] re-read after the call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): list[-3] != 0 (the routes only see 0: the call func_0886B9A4(this[7], 0, list) and the object re-read before the tail call are checked against the asm only).
void func_0886F1B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x70u);
    const std::uint32_t list = word(rt, object + 0x130u);
    const std::uint32_t count = word(rt, list - 0xCu);
    ctx.gpr[2] = count;
    if (count != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x0886B9A4u, {word(rt, self + 0x1Cu), 0u, list});
    }
    tail_call(ctx, 0x0886F0E0u, {word(rt, self + 0x70u), 0u});
}

// func_08C40FA4 / func_08C4100C / func_08C41074 / func_08C411D4 / func_08C413DC / func_08C41444 / func_08C4199C(limits, p): seven instances of the
// clamp step (the same instruction sequence as func_08C4045C of batch 63, compared op by op with the branch targets masked): see clamp_step.
void func_08C40FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C4100C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C411D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C413DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C4199C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }

// func_08C1D3F0(dest, count, value): `count` times func_08A590CC(p, value) for a non-null p, p from dest advancing by 4 (as func_08C251D0 of batch 63).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count 0 (returns at once), a null p (the call is skipped, p still advances in the delay slot of the loop's bne; the routes always pass a set p).
void func_08C1D3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_count(rt, ctx, 0x08A590CCu); }

} // namespace hot_batch_65
} // namespace gt::game

#define HOT65(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_65::name, gt::ReturnKind::kind)
HOT65(0x08BF8BFCu, func_08BF8BFC, Word);
HOT65(0x08BF8C48u, func_08BF8C48, Word);
HOT65(0x08C09730u, func_08C09730, Word);
HOT65(0x08B8E594u, func_08B8E594, Word);
HOT65(0x08B8E5D8u, func_08B8E5D8, Word);
HOT65(0x08BE58F0u, func_08BE58F0, Word);
HOT65(0x08BE5988u, func_08BE5988, Word);
HOT65(0x08BE59D4u, func_08BE59D4, Word);
HOT65(0x08B3EB80u, func_08B3EB80, Word);
HOT65(0x08C3DECCu, func_08C3DECC, Word);
HOT65(0x08C4B50Cu, func_08C4B50C, Word);
HOT65(0x08C4B35Cu, func_08C4B35C, Word);
HOT65(0x0886FCA4u, func_0886FCA4, Word);
HOT65(0x0887172Cu, func_0887172C, Word);
HOT65(0x0887AD7Cu, func_0887AD7C, Word);
HOT65(0x0887C644u, func_0887C644, Word);
HOT65(0x08B93D3Cu, func_08B93D3C, Word);
HOT65(0x08C1CC2Cu, func_08C1CC2C, Word);
HOT65(0x08AB93B4u, func_08AB93B4, Word);
HOT65(0x0890AFC0u, func_0890AFC0, Word);
HOT65(0x08BDAF18u, func_08BDAF18, Word);
HOT65(0x08AD0700u, func_08AD0700, Word);
HOT65(0x08B5C230u, func_08B5C230, Word);
HOT65(0x08C2016Cu, func_08C2016C, Word);
HOT65(0x0895FE28u, func_0895FE28, Word);
HOT65(0x08B516F4u, func_08B516F4, Word);
// Written but not registered: no route reaches it (compared 0 in all five routes of the fast gate).
// HOT65(0x08BDADACu, func_08BDADAC, Word);
HOT65(0x0894CEB8u, func_0894CEB8, Word);
HOT65(0x0886F1B4u, func_0886F1B4, Word);
HOT65(0x08C4100Cu, func_08C4100C, Word);
HOT65(0x08C41074u, func_08C41074, Word);
HOT65(0x08C411D4u, func_08C411D4, Word);
HOT65(0x08C41444u, func_08C41444, Word);
HOT65(0x08C4199Cu, func_08C4199C, Word);
HOT65(0x08C1D3F0u, func_08C1D3F0, Word);
// Written but not registered: they write memory and no compared case of the five routes changed any (the box / vector / elements the routes
// pass leave the stores no visible effect: empty ranges, zero boxes, or values equal to what was there), so the gate cannot see their stores;
// func_08BDADAC is reached by no route (compared 0).
// HOT65(0x08C15F9Cu, func_08C15F9C, Word);
// HOT65(0x08C4B568u, func_08C4B568, Word);
// HOT65(0x08C4B740u, func_08C4B740, Word);
// HOT65(0x08C4B7A0u, func_08C4B7A0, Word);
// HOT65(0x08C4B800u, func_08C4B800, Word);
// HOT65(0x08C4B860u, func_08C4B860, Word);
// HOT65(0x088824F8u, func_088824F8, Word);
// func_08C40FA4 and func_08C413DC (clamp instances) only ever saw a zero *p, so their store of 0 over 0 changed nothing:
// HOT65(0x08C40FA4u, func_08C40FA4, Word);
// HOT65(0x08C413DCu, func_08C413DC, Word);
