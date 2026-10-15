// Hot batch 77: fourteen registered shape families (sixteen written; two left out, see the end of the file) that no earlier batch has a registered template for, found with `tools.family_members --list --any-template` over the
// call probe of batches 70-76 (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off; batch 76 excluded
// with --exclude-ref). For each family one member was read in full by hand against the asm (rabbitizer disassembly with the relocations applied; the
// generated code was not used), and every other member was compared with it word by word by the tool: the words that differ are only the relocated
// constants, a displacement and the jal / j targets, never a register or a stack offset (no `!!` line). The constants of each member come from its own
// relocated words and `--check` re-reads every row of this file against them. A template that is itself reached is registered too (the tool lists it as a
// member when it is run from another member of the family). Their classes aren't known yet, so they keep their addresses as names.
//
// Families (the implementation is the one of the helper, with this member's constants):
//  - "holder apply" (58 instructions): a float taken from a holder's object (a virtual call) or a default, applied to a sub-object of the object another
//    function returns in a holder, the result put into the caller's holder (9);
//  - "guarded static object with a size" (40 instructions; batch 76's "no size" shape plus one store) (6);
//  - "counted assign" (35 instructions): a reference-counted pointer assignment with the atomic ll / sc increments and decrements (6);
//  - "member-function-pointer call, then a float tail call" (47 instructions) (6);
//  - "tree lower bound" (40 instructions): the lower bound of a string key in a red-black tree (6);
//  - "for each element, call a member function pointer" (45 instructions) (5);
//  - "init with a vtable, then tail call" (14 instructions) (4); "construct each element" (32 instructions) (4); "erase a subtree" (31 instructions,
//    recursive through its own address) (4);
//  - small ones: "copy four floats" (13 instructions) (3), "handle init" (14) (3), "construct with a base" (17) (3), "two-holder destructor" (19; the
//    shape of batch 59's func_08C1EB60) (2) and "record call" (19; the displacement of the record read back differs) (3).
//
// Left out (they stay recompiled): the "clamp" family (func_08C401C4 and its instances, 26 instructions) and "release a vector" (func_08C4B740 and its three
// instances): written here at first, but both are already written and unregistered in batches 65 / 68 / 73 / 74 for the reason that still holds — every
// compared case saw a zero offset / an empty vector, so none changed memory and the gate cannot see their stores.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep the
// caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_77 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAssignSmart = 0x08A5BAD8u;  // (dst, value): smart-pointer assignment
constexpr std::uint32_t kReportNull = 0x08A5BB7Cu;   // (message): the original's null-object assert

// Holder apply (58 instructions): func_08A22050(&h) fills the holder h with the engine object o; null-checked with func_08A5BB7C(message 0x08CAAA84), then h is released. items = a3 holds
// `count` (a2) object pointers; when count > 0 the first one (null-checked, message 0x08CAAA48) answers a virtual call (vtable slot 0x54, a float), and
// apply(o + 0x14, that float) is the result (the function leaves through the epilogue with v0 as apply returns it). Otherwise apply_empty(o + 0x14) gives
// the float, func_08A87EEC(&r, float) fills a second holder r, which is assigned to the caller's holder a0 (func_08A5BAD8, unless a0 is the frame's own
// slot) and released. v0 = the last callee's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object from func_08A22050 (the bnez falls into the report call, whose delay slot loads a0 = the message; the code goes on with a null object:
// apply targets 0x14), a null first item (the same with message 0x08CAAA48, then the vtable is read through the null pointer), and the whole count <= 0 path
// (the blez lands on the `jal apply_empty`, a0 = object + 0x14 set in its own delay slot): the float it returns goes to func_08A87EEC(&r, f12), r is assigned
// to a0 with func_08A5BAD8(a0, r) unless a0 is the frame slot itself (the `beq` lands on the release, a0 = &r from its delay slot), and r is released; v0 is
// the release's. The routes only compare count > 0.
void holder_apply(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t apply, std::uint32_t apply_empty) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], items = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x08A22050u, {frame.at(4u)});
    const std::uint32_t object = word(rt, frame.at(4u));
    if (object == 0u) (void)call_guest(rt, ctx, kReportNull, {0x08CAAA84u});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    const std::uint32_t target = object + 0x14u;
    if (static_cast<std::int32_t>(count) > 0) {
        const std::uint32_t first = word(rt, items);
        if (first == 0u) (void)call_guest(rt, ctx, kReportNull, {0x08CAAA48u});
        const std::uint32_t function = word(rt, word(rt, first) + 0x54u);
        const GuestResult value = call_guest(rt, ctx, function, {first}, {});
        const GuestResult result = call_guest(rt, ctx, apply, {target}, {value.f0});
        ctx.fpr[0] = result.f0;
        ctx.gpr[2] = result.v0;
        ctx.gpr[3] = result.v1;
        return;
    }
    const GuestResult value = call_guest(rt, ctx, apply_empty, {target}, {});
    (void)call_guest(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {value.f0});
    if (out != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(0u))});
    const GuestResult tail = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)}, {});
    ctx.fpr[0] = tail.f0;
    ctx.gpr[2] = tail.v0;
    ctx.gpr[3] = tail.v1;
}

// Guarded static object with a size (40 instructions): `static T x(source, 0); x.size = N;` inside a function. If the guard byte is 0 and
// func_08B6441C(guard) lets this caller build it, object != 0: constructor(object, source, 0), object[4] (+0x10) = size, object[0] = vtable (stored before
// the next call runs), registrar() (the accessor of another function-local static; it reads no argument); then the tail call func_08B64494(guard).
// v0 is hi(guard) when the guard byte is already set, 0 when the acquire fails, else whatever the release leaves.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object being already built (the guard byte set: the `lb` + `bnez` lands on the epilogue with v0 = hi(guard)), func_08B6441C answering 0 (the
// beqz lands on the epilogue with v0 = 0; its delay slot only sets s0) and the null object (the beqz at the call lands on `move a0, guard` and the same tail
// call, skipping the construction; its delay slot only sets a2 = 0; the objects are constants above the load base, so it cannot happen).
void guarded_static_sized(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object, std::uint32_t source,
                          std::uint32_t constructor, std::uint32_t size, std::uint32_t vtable, std::uint32_t registrar) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) != 0) {
            ctx.gpr[2] = (guard + 0x8000u) & 0xFFFF0000u;
            return;
        }
        if (call_guest(rt, ctx, 0x08B6441Cu, {guard}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (object != 0u) {
            (void)call_guest(rt, ctx, constructor, {object, source, 0u});
            put(rt, object + 0x10u, size);
            put(rt, object, vtable);
            (void)call_guest(rt, ctx, registrar, {});
        }
    }
    tail_call(ctx, 0x08B64494u, {guard});
}

// Counted assign (35 instructions): a0 = the holder to copy from (its word 0 is the shared counter block, or null), a1 = the destination (null: nothing
// happens, v0 untouched), a2 = the value stored in dest[0]. The block's counter (+8) is incremented (ll / sc, one thread: a plain load and store),
// v0 = 1; then dest[1]'s old block, if any, has its counter decremented, v0 = 1, and when it was 1 the block's virtual slot +0xC runs on it
// (a0 = block, a1 = 1, v0 = its result); dest[1] = the new block.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null destination (the beqz lands on the epilogue, v0 untouched; its delay slot only saves s0), a null source block (the beqz skips the increment, v0
// untouched; s1 = dest + 4 comes from its delay slot), and everything after the load of the old block: the routes only compare cases whose old block is null
// (the beqz lands on the final store). For a non-null old block: the counter is decremented by the ll / sc loop (it retries while the `sc` fails: never with one
// thread); a block whose counter was not 1 leaves with v0 = 1 and the store done in the delay slot of the likely branch (taken: the delay slot runs); a block
// whose counter was 1 (the bnel falls through and its delay slot is nullified) runs the virtual call vtable[3] with a0 = the block, a1 = 1 (the counter's old
// value, left by the `ll`), then the same store, v0 = the call's.
void counted_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4], dest = ctx.gpr[5], value = ctx.gpr[6];
    if (dest == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    put(rt, dest, value);
    const std::uint32_t incoming = word(rt, source);
    if (incoming != 0u) {
        put(rt, incoming + 8u, word(rt, incoming + 8u) + 1u);
        ctx.gpr[2] = 1u;
    }
    const std::uint32_t slot = dest + 4u;
    const std::uint32_t old = word(rt, slot);
    if (old != 0u) {
        const std::uint32_t before = word(rt, old + 8u);
        put(rt, old + 8u, before - 1u);
        ctx.gpr[2] = 1u;
        if (before == 1u) {
            const std::uint32_t function = word(rt, word(rt, old) + 0xCu);
            ctx.gpr[2] = call_guest(rt, ctx, function, {old, before, value, ctx.gpr[7]});
        }
    }
    put(rt, slot, incoming);
}

// Member-function-pointer call, then a float tail call (47 instructions). this[0x14] / this[0x18] is a pointer to member function {function, adjust}
// (adjust bit 0: virtual, so `function` is a vtable offset; adjust >> 1 is added to the object). a1 = the address of a pointer to the object. The call
// returns a pointer p; the second pointer {this[ptr_offset], this[adjust_offset]} (the same encoding) is resolved on p, and the tail call is
// target(p + adjust >> 1, f12 = func_08B8C5D4(a2)) with t9 = target. The registers a1-a3 of the first call are the object, function and adjust words.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual forms of the pointers (bit 0 of the first pointer's adjust: the `bnez` lands on `lw v0, 0(a0)` and the call goes through *(*receiver + function);
// bit 0 of the second: the `beqz` is not taken, v1 = this[ptr] is loaded before the vtable word and the target is *(*base + that), then the `j` into the shared
// tail; both reach the same `jal func_08B8C5D4`). The routes only take the plain pointers.
void ptmf_call_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t adjust_offset, std::uint32_t pointer_offset) {
    const std::uint32_t self = ctx.gpr[4], holder = ctx.gpr[5], argument = ctx.gpr[6];
    std::uint32_t target, base;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t adjust = word(rt, self + 0x18u);
        const std::uint32_t object = word(rt, holder);
        const std::uint32_t function = word(rt, self + 0x14u);
        const std::uint32_t receiver = object + static_cast<std::uint32_t>(static_cast<std::int32_t>(adjust) >> 1);
        std::uint32_t callee = function;
        if ((adjust & 1u) != 0u) callee = word(rt, word(rt, receiver) + function);
        const std::uint32_t result = call_guest(rt, ctx, callee, {receiver, object, function, adjust});
        const std::uint32_t second = word(rt, self + adjust_offset);
        base = result + static_cast<std::uint32_t>(static_cast<std::int32_t>(second) >> 1);
        if ((second & 1u) != 0u) {
            const std::uint32_t slot = word(rt, self + pointer_offset);
            target = word(rt, word(rt, base) + slot);
        } else {
            target = word(rt, self + pointer_offset);
        }
        const GuestResult scale = call_guest(rt, ctx, 0x08B8C5D4u, {argument}, {});
        ctx.fpr[12] = scale.f0;
        ctx.gpr[2] = scale.v0;
        ctx.gpr[3] = scale.v1;
    }
    ctx.gpr[25] = target;
    tail_call(ctx, target, {base});
}

// Tree lower bound (40 instructions): a0 = the tree (header at +4, root at +8), a1 = the address of the key's character pointer (the string's length is
// the word before its characters). Each node holds its key pointer at +0x10, left child +8, right child +0xC. With order = func_08B36170(node key, key,
// min(lengths)) (the length difference when it is 0): order < 0 goes right, else the node is the candidate and the walk goes left. v0 = the candidate
// (the header when no node qualifies or the tree is empty).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none the gate missed (every branch outcome was taken: the empty tree, both directions, ties on the length).
void tree_lower_bound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4], key_slot = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t node = word(rt, tree + 8u);
    std::uint32_t best = tree + 4u;
    if (node != 0u) {
        const std::uint32_t key = word(rt, key_slot);
        do {
            const std::uint32_t node_key = word(rt, node + 0x10u);
            const std::uint32_t node_length = word(rt, node_key - 12u);
            const std::uint32_t key_length = word(rt, key - 12u);
            const std::uint32_t shorter = key_length < node_length ? key_length : node_length;
            std::uint32_t order = node_length - key_length;
            const std::uint32_t compared = call_guest(rt, ctx, 0x08B36170u, {node_key, key, shorter});
            if (compared != 0u) order = compared;
            std::uint32_t next;
            if (static_cast<std::int32_t>(order) < 0) {
                next = word(rt, node + 0xCu);
            } else {
                next = word(rt, node + 8u);
                best = node;
            }
            node = next;
        } while (node != 0u);
    }
    ctx.gpr[2] = best;
}

// For each element, call a member function pointer (45 instructions): a0 = first, a1 = last (element size `stride`), a2 = the address of {function, adjust}
// (adjust bit 0: virtual, so `function` is a vtable offset; adjust >> 1 is added to the element). Calls it on every element (a0 = element + adjust >> 1);
// v0 / v1 = the pointer's two words.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty range (the `beq` lands on the epilogue with v0 = function, v1 = adjust, the latter loaded in its delay slot) and the virtual form (bit 0 of
// the adjust: the call goes through *(*object + function)). The routes only take plain pointers on non-empty ranges.
void for_each_ptmf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t stride) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5], pointer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t function = word(rt, pointer);
    const std::uint32_t adjust = word(rt, pointer + 4u);
    if (first != last) {
        const std::uint32_t shift = static_cast<std::uint32_t>(static_cast<std::int32_t>(adjust) >> 1);
        std::uint32_t element = first;
        do {
            const std::uint32_t object = element + shift;
            std::uint32_t callee = function;
            if ((adjust & 1u) != 0u) callee = word(rt, word(rt, object) + function);
            element += stride;
            (void)call_guest(rt, ctx, callee, {object});
        } while (last != element);
    }
    ctx.gpr[2] = function;
    ctx.gpr[3] = adjust;
}

// Init with a vtable, then tail call (14 instructions): func_08B1E2D4(this); this[0] = vtable, this[5] = 0; then func_08B1E1C0(this) (a tail call; v0 =
// the vtable, as the original leaves it).
// Branches: none, the function is straight-line.
void init_vtable_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B1E2D4u, {self});
        put(rt, self, vtable);
        put(rt, self + 0x14u, 0u);
    }
    ctx.gpr[2] = vtable;
    tail_call(ctx, 0x08B1E1C0u, {self});
}

// Construct each element (32 instructions): prepare(this, vec, count) runs first; then, when vec[0] is non-null, element(vec[0] + i * 8, this) for i = 0 ..
// count - 1 (vec[0] is read again before each call but the first). v0 = 0 when vec[0] is null, vec[0] when count is 0, else the last call's result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count = 0 with a non-null vec[0] (the `bnez` falls through; its delay slot only sets s0, v0 stays vec[0]); a null vec[0] and the loop both occur.
void construct_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t prepare, std::uint32_t element) {
    const std::uint32_t self = ctx.gpr[4], vec = ctx.gpr[5], count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, prepare, {self, vec, count});
    std::uint32_t begin = word(rt, vec);
    if (begin == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = begin;
    if (count == 0u) return;
    std::uint32_t index = 0u;
    do {
        const std::uint32_t address = begin + (index << 3);
        ++index;
        ctx.gpr[2] = call_guest(rt, ctx, element, {address, self});
        if (count != index) begin = word(rt, vec);
    } while (count != index);
}

// Erase a subtree (31 instructions, recursive through its own address): a0 = the owner, a1 = a node (null: nothing, v0 untouched). For each node, from
// the first: erase(owner, node[3]) (the right subtree), the left child node[2] is read, the holder at node + 0x14 is released (func_08A5B9EC),
// destroy(node + 0x10) runs, and the node is freed with func_08A5BC54(node, 0x18, 4); the walk goes on with the left child. v0 = the last free's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none the gate missed (a null node at entry, a leaf, a node with one or two children all occur).
void tree_erase(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_address, std::uint32_t destroy) {
    const std::uint32_t owner = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    do {
        (void)call_guest(rt, ctx, self_address, {owner, word(rt, node + 0xCu)});
        const std::uint32_t left = word(rt, node + 8u);
        (void)call_guest(rt, ctx, kReleaseLocal, {node + 0x14u});
        (void)call_guest(rt, ctx, destroy, {node + 0x10u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BC54u, {node, 0x18u, 4u});
        node = left;
    } while (node != 0u);
}

// Copy four floats (13 instructions): dest[i] = (*source)[i] for i = 0..3, the pointer *source read again before each word (the stores may alias it).
// v0 = the last pointer read, f0 = the last float. Plain word copies: lwc1 / swc1 keep the bits.
// Branches: none, the function is straight-line (the loop below is the original's unrolled sequence).
void copy_four_floats(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4], source = ctx.gpr[5];
    std::uint32_t block = 0u, value = 0u;
    for (std::uint32_t offset = 0u; offset < 16u; offset += 4u) {
        block = word(rt, source);
        value = word(rt, block + offset);
        put(rt, dest + offset, value);
    }
    ctx.gpr[2] = block;
    ctx.fpr[0] = std::bit_cast<float>(value);
}

// Handle init (14 instructions): this[0] = this[1] = 0, byte this[8] = 0 (in the delay slot of the call), then maker(this, a1, 0). v0 = this.
// Branches: none, the function is straight-line.
void handle_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    rt.memory().store8(self + 8u, 0u);
    (void)call_guest(rt, ctx, maker, {self, ctx.gpr[5], 0u});
    ctx.gpr[2] = self;
}

// Construct with a base (17 instructions): base(this, a1, 1), then this[6] = a2 and this[0] = the vtable. v0 = the vtable.
// Branches: none, the function is straight-line.
void construct_with_base(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4], extra = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, base, {self, ctx.gpr[5], 1u});
    put(rt, self + 0x18u, extra);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// Two-holder destructor (19 instructions; batch 59's func_08C298A4 / func_08C1EB60 shape): this[0] = vtable, the holder at +8 released
// (func_08A5B9EC), this[0] = 0x08CD1F30 (stored before the second call runs), the holder at +4 released, then func_08B64654(this) (a tail call, v0 =
// the second release's).
// Branches: none, the function is straight-line.
void two_holder_destructor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 8u});
        put(rt, self, 0x08CD1F30u);
        ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {self + 4u});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// Record call (19 instructions; batch 75's "record query" with the record read back as a word): object = func_089363FC(this) (the singleton), the record
// func_08936584(object, a1, this[0x30], this[0x34], &record) fills (a frame of 0x90 bytes), then func_08937D90(func_089363FC()[0x1EC], record[offset]).
// v0 = that result.
// Branches: none, the function is straight-line.
void record_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t registry = call_guest(rt, ctx, 0x089363FCu, {self});
    const std::uint32_t key_low = word(rt, self + 0x30u);
    const std::uint32_t key_high = word(rt, self + 0x34u);
    (void)call_guest(rt, ctx, 0x08936584u, {registry, ctx.gpr[5], key_low, key_high, frame.at(0u)});
    const std::uint32_t second = call_guest(rt, ctx, 0x089363FCu, {});
    const std::uint32_t handle = word(rt, second + 0x1ECu);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08937D90u, {handle, word(rt, frame.at(offset))});
}

} // namespace

// func_XXXXXXXX (holder apply): see holder_apply.
void func_08A224C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB8784u, 0x08AB8790u); }
void func_08A225B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB879Cu, 0x08AB87A8u); }
void func_08A22698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB8738u, 0x08AB8744u); }
void func_08A22780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB87D0u, 0x08AB87D8u); }
void func_08A22868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB87E0u, 0x08AB87ECu); }
void func_08A22950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB87F8u, 0x08AB8804u); }
void func_08A22A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB8810u, 0x08AB881Cu); }
void func_08A22B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB8828u, 0x08AB8834u); }
void func_08A22C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_apply(rt, ctx, 0x08AB8840u, 0x08AB884Cu); }

// func_XXXXXXXX (guarded static object with a size): see guarded_static_sized.
void func_08B9FC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D75678u, 0x08D75BBCu, 0x08CA0198u, 0x08B9FBECu, 3u, 0x08CC0CE0u, 0x08B9D41Cu); }
void func_08B9FE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D75458u, 0x08D75BD0u, 0x08CA019Cu, 0x08B9FE00u, 4u, 0x08CC0BD0u, 0x08B9D648u); }
void func_08BA1704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D75648u, 0x08D75C34u, 0x08CA01CCu, 0x08B9FFD0u, 4u, 0x08CC05A0u, 0x08B9DAB8u); }
void func_08BBA094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D76060u, 0x08D76100u, 0x08CA0BB8u, 0x08BB976Cu, 4u, 0x08CC3D30u, 0x08BB97B0u); }
void func_08BC5B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D76280u, 0x08D76788u, 0x08CA0E48u, 0x08BC5A9Cu, 4u, 0x08CC4918u, 0x08BC5AE0u); }
void func_08BC624C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_sized(rt, ctx, 0x08D76278u, 0x08D7681Cu, 0x08CA0E8Cu, 0x08BC6178u, 4u, 0x08CC4400u, 0x08BC61BCu); }

// func_XXXXXXXX (counted assign): see counted_assign.
void func_08BBD79C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }
void func_08BD368C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }
void func_08BD6600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }
void func_08BD668C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }
void func_08BE079C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }
void func_08BE0828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counted_assign(rt, ctx); }

// func_XXXXXXXX (member-function-pointer call, then a float tail call): see ptmf_call_float.
void func_08BEB08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000028u, 0x00000024u); }
void func_08BF2650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000028u, 0x00000024u); }
void func_08C03BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000028u, 0x00000024u); }
void func_08C05924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000028u, 0x00000024u); }
void func_08C0A1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000028u, 0x00000024u); }
void func_08C0BA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_call_float(rt, ctx, 0x00000020u, 0x0000001Cu); }

// func_XXXXXXXX (tree lower bound): see tree_lower_bound.
void func_08BF6164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }
void func_08BF6308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }
void func_08C00D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }
void func_08C2FB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }
void func_08C31D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }
void func_08C31F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_lower_bound(rt, ctx); }

// func_XXXXXXXX (for each element, call a member function pointer): see for_each_ptmf.
void func_08BB8784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_ptmf(rt, ctx, 84u); }
void func_08BB8838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_ptmf(rt, ctx, 8u); }
void func_08BB8D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_ptmf(rt, ctx, 8u); }
void func_08C3FF20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_ptmf(rt, ctx, 32u); }
void func_08C3FFD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_ptmf(rt, ctx, 12u); }

// func_XXXXXXXX (init with a vtable, then tail call): see init_vtable_tail.
void func_08BDAE2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_vtable_tail(rt, ctx, 0x08CC5690u); }
void func_08C19C20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_vtable_tail(rt, ctx, 0x08CD0E18u); }
void func_08C1A21C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_vtable_tail(rt, ctx, 0x08CD1D88u); }
void func_08C3CF04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_vtable_tail(rt, ctx, 0x08CD6918u); }

// func_XXXXXXXX (construct each element): see construct_range.
void func_08C406A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_range(rt, ctx, 0x08C405D4u, 0x08AF76E8u); }
void func_08C4081C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_range(rt, ctx, 0x08C40294u, 0x08AF770Cu); }
void func_08C41B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_range(rt, ctx, 0x08C40F3Cu, 0x08AF98B0u); }
void func_08C41C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_range(rt, ctx, 0x08C4123Cu, 0x08AF98D0u); }

// func_XXXXXXXX (erase a subtree): see tree_erase.
void func_08C00860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_erase(rt, ctx, 0x08C00860u, 0x08A69D68u); }
void func_08C1D728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_erase(rt, ctx, 0x08C1D728u, 0x08A69D68u); }
void func_08C25BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_erase(rt, ctx, 0x08C25BFCu, 0x08A69D68u); }
void func_08C28648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_erase(rt, ctx, 0x08C28648u, 0x08A5B9ECu); }

// func_XXXXXXXX (copy four floats): see copy_four_floats.
void func_08B9B708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_four_floats(rt, ctx); }
void func_08B9B73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_four_floats(rt, ctx); }
void func_08B9B770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_four_floats(rt, ctx); }

// func_XXXXXXXX (handle init): see handle_init.
void func_08B9CC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_init(rt, ctx, 0x08B9BD70u); }
void func_08BBD250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_init(rt, ctx, 0x08BBD0E4u); }
void func_08BD2B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_init(rt, ctx, 0x08BD2B14u); }

// func_XXXXXXXX (construct with a base): see construct_with_base.
void func_08B88BA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_base(rt, ctx, 0x08B88B64u, 0x08CBCD30u); }
void func_08C3C230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_base(rt, ctx, 0x08C3C1F4u, 0x08CD6090u); }
void func_08C3DF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_base(rt, ctx, 0x08C3DF3Cu, 0x08CD6D20u); }

// func_XXXXXXXX (two-holder destructor): see two_holder_destructor.
void func_08C1DD4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_holder_destructor(rt, ctx, 0x08CD1F18u); }
void func_08C2859C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_holder_destructor(rt, ctx, 0x08CD4168u); }

// func_XXXXXXXX (record call): see record_call.
void func_0893628C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_call(rt, ctx, 0x00000014u); }
void func_089362D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_call(rt, ctx, 0x00000018u); }
void func_08936324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_call(rt, ctx, 0x00000004u); }

} // namespace hot_batch_77
} // namespace gt::game

#define HOT77(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_77::name, gt::ReturnKind::kind)
HOT77(0x08A224C8u, func_08A224C8, Word);
HOT77(0x08A225B0u, func_08A225B0, Word);
HOT77(0x08A22698u, func_08A22698, Word);
HOT77(0x08A22780u, func_08A22780, Word);
HOT77(0x08A22868u, func_08A22868, Word);
HOT77(0x08A22950u, func_08A22950, Word);
HOT77(0x08A22A38u, func_08A22A38, Word);
HOT77(0x08A22B20u, func_08A22B20, Word);
HOT77(0x08A22C08u, func_08A22C08, Word);
HOT77(0x08B9FC30u, func_08B9FC30, Word);
HOT77(0x08B9FE44u, func_08B9FE44, Word);
HOT77(0x08BA1704u, func_08BA1704, Word);
HOT77(0x08BBA094u, func_08BBA094, Word);
HOT77(0x08BC5B70u, func_08BC5B70, Word);
HOT77(0x08BC624Cu, func_08BC624C, Word);
HOT77(0x08BBD79Cu, func_08BBD79C, Word);
HOT77(0x08BD368Cu, func_08BD368C, Word);
HOT77(0x08BD6600u, func_08BD6600, Word);
HOT77(0x08BD668Cu, func_08BD668C, Word);
HOT77(0x08BE079Cu, func_08BE079C, Word);
HOT77(0x08BE0828u, func_08BE0828, Word);
HOT77(0x08BEB08Cu, func_08BEB08C, Word);
HOT77(0x08BF2650u, func_08BF2650, Word);
HOT77(0x08C03BF4u, func_08C03BF4, Word);
HOT77(0x08C05924u, func_08C05924, Word);
HOT77(0x08C0A1B8u, func_08C0A1B8, Word);
HOT77(0x08C0BA00u, func_08C0BA00, Word);
HOT77(0x08BF6164u, func_08BF6164, Word);
HOT77(0x08BF6308u, func_08BF6308, Word);
HOT77(0x08C00D14u, func_08C00D14, Word);
HOT77(0x08C2FB58u, func_08C2FB58, Word);
HOT77(0x08C31D6Cu, func_08C31D6C, Word);
HOT77(0x08C31F14u, func_08C31F14, Word);
HOT77(0x08BB8784u, func_08BB8784, DoubleWord);
HOT77(0x08BB8838u, func_08BB8838, DoubleWord);
HOT77(0x08BB8D60u, func_08BB8D60, DoubleWord);
HOT77(0x08C3FF20u, func_08C3FF20, DoubleWord);
HOT77(0x08C3FFD4u, func_08C3FFD4, DoubleWord);
HOT77(0x08BDAE2Cu, func_08BDAE2C, Word);
HOT77(0x08C19C20u, func_08C19C20, Word);
HOT77(0x08C1A21Cu, func_08C1A21C, Word);
HOT77(0x08C3CF04u, func_08C3CF04, Word);
HOT77(0x08C406A4u, func_08C406A4, Word);
HOT77(0x08C4081Cu, func_08C4081C, Word);
HOT77(0x08C41B64u, func_08C41B64, Word);
HOT77(0x08C41C80u, func_08C41C80, Word);
HOT77(0x08C00860u, func_08C00860, Word);
HOT77(0x08C1D728u, func_08C1D728, Word);
HOT77(0x08C25BFCu, func_08C25BFC, Word);
HOT77(0x08C28648u, func_08C28648, Word);
HOT77(0x08B9B708u, func_08B9B708, Word);
HOT77(0x08B9B73Cu, func_08B9B73C, Word);
HOT77(0x08B9B770u, func_08B9B770, Word);
HOT77(0x08B9CC58u, func_08B9CC58, Word);
HOT77(0x08BBD250u, func_08BBD250, Word);
HOT77(0x08BD2B48u, func_08BD2B48, Word);
HOT77(0x08B88BA0u, func_08B88BA0, Word);
HOT77(0x08C3C230u, func_08C3C230, Word);
HOT77(0x08C3DF88u, func_08C3DF88, Word);
HOT77(0x08C1DD4Cu, func_08C1DD4C, Word);
HOT77(0x08C2859Cu, func_08C2859C, Word);
HOT77(0x0893628Cu, func_0893628C, Word);
HOT77(0x089362D8u, func_089362D8, Word);
HOT77(0x08936324u, func_08936324, Word);
