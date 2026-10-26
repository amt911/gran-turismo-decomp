// Hot batch 78: shape families that no earlier batch has a registered template for, found with `tools.family_members --list --any-template` over a fresh
// call probe (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off; batch 77 excluded with --exclude-ref).
// For each family one member was read in full by hand against the asm (rabbitizer disassembly with the relocations applied, data addresses taken from
// the relocation's own segment; the generated code was not used), and every other member was compared with it word by word by the tool: the words that
// differ are only the relocated constants, a displacement and the jal / j targets, never a register or a stack offset (no `!!` line). The constants of
// each member come from its own relocated words and `--check` re-reads every row of this file against them. A template that is itself reached is
// registered too. Their classes aren't known yet, so they keep their addresses as names.
//
// Families (the implementation is the one of the helper, with this member's constants):
//  - "wrap a" (10 instructions): a copy of an argument is kept in the frame and a sub-object's address plus an offset goes to a callee (4);
//  - "float setter" (2 instructions) (4); "copy three floats" (7) (3); "call, then tail call" (11) (3); "pair call" (15) (3);
//  - "guarded static, three zero words" (25 instructions) (3); "shared box" (25): a 16-byte reference-count block built around a value (3);
//  - "member-function-pointer call, then a member-function-pointer tail call" (37 instructions) (4) and its variant with an extra argument (38) (3);
//  - "new holder" (37 instructions): a new 24-byte object stored in a holder and handed to a registrar (3);
//  - "erase a string-keyed subtree" (42 instructions, recursive through its own address) (4);
//  - "holder pair" (41 instructions) (3); "release destructor" (43 instructions): two atomic counter decrements with virtual calls (3);
//  - "vector reserve" (63 instructions) (4); "tree find" (66 instructions): the exact-match lookup of a string key in a red-black tree (4).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep the
// caller's value, which call_guest copies from ctx. Where a function ends with a call whose registers its caller reads, v0, v1 and f0 are the
// callee's (the GuestResult overload).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_78 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAllocate = 0x08A5BC20u;     // (size, alignment): a new block
constexpr std::uint32_t kFree = 0x08A5BC54u;         // (block, size, alignment)

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

inline std::int32_t sra1(std::uint32_t value) { return static_cast<std::int32_t>(value) >> 1; }

// Wrap a (10 instructions): frame[0] = a1 (stored first), then callee(this[1] + offset, &frame[0]) with a2 / a3 as the caller has them. The result is the
// callee's.
// Branches: none, the function is straight-line.
void wrap_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    take(ctx, call_guest(rt, ctx, callee, {word(rt, self + 4u) + offset, frame.at(0u)}, {}));
}

// Float setter (2 instructions): this[offset] = f12 (the bits of the float). Leaves v0 alone.
// Branches: none, the function is straight-line.
void float_setter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    put(rt, ctx.gpr[4] + offset, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// Copy three floats (7 instructions): this[first / second / third] = source[0 / 4 / 8] (lwc1 / swc1 keep the bits); f1 = the last word, f0 = the middle one.
// Branches: none, the function is straight-line.
void copy_three(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second, std::uint32_t third) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    put(rt, self + first, word(rt, source));
    const std::uint32_t middle = word(rt, source + 4u);
    put(rt, self + second, middle);
    const std::uint32_t last = word(rt, source + 8u);
    put(rt, self + third, last);
    ctx.fpr[0] = std::bit_cast<float>(middle);
    ctx.fpr[1] = std::bit_cast<float>(last);
}

// Call, then tail call (11 instructions): r = first(a0, a1); then second(r, a1) as a tail call (the original keeps a1 in s0 across the call).
// Branches: none, the function is straight-line.
void pair_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t argument = ctx.gpr[5];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, first, {ctx.gpr[4], argument}, {});
    }
    take(ctx, result);
    tail_call(ctx, second, {result.v0, argument});
}

// Pair call (15 instructions): the two words at `pair` and +4 are copied to frame[0] / frame[4]; then callee(a0, a1, &frame[0], a2, a3) (a2 / a3 move up one
// register, t0 = a3). The result is the callee's.
// Branches: none, the function is straight-line.
void pair_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pair, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t second = word(rt, pair + 4u);
    const std::uint32_t first = word(rt, pair);
    put(rt, frame.at(0u), first);
    put(rt, frame.at(4u), second);
    take(ctx, call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[6], ctx.gpr[7]}, {}));
}

// Guarded static, three zero words (25 instructions): `static T x;` of three words, T zero-initialised. If the guard byte (signed) is 0 and
// func_08B6441C(guard) lets this caller build it: x[2] = x[1] = x[0] = 0 and the tail of the guard protocol func_08B64494(guard) (a normal call here: the
// original goes on to load the object's address). v0 = the object's address on every path. `store_disp` is the displacement of the original's
// `sw zero, disp(hi)` for the first word: the same address as `object`.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C answering 0 (the `beqz` lands on the epilogue's
// `addiu v0, hi, disp`, v0 = the object, the same instruction the already-built case reaches; its delay slot only sets a0 = guard). The guard byte already set
// and the construction both occur in the compared cases.
void guarded_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object, std::uint32_t /*store_disp*/) {
    GuestFrame frame(ctx, 0x10u);
    if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        put(rt, object + 8u, 0u);
        put(rt, object + 4u, 0u);
        put(rt, object, 0u);
        (void)call_guest(rt, ctx, 0x08B64494u, {guard});
    }
    ctx.gpr[2] = object;
}

// Shared box (25 instructions): this[0] = a1; box = func_08B62344(16); box[1] = 1, box[0] = vtable, box[2] = 1, box[3] = a1; this[1] = box; then the
// tail call func_08B8E380(this + 4, a1, a1, 1). v0 = box when the tail call starts.
// Branches: none, the function is straight-line.
void shared_box(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4], value = ctx.gpr[5];
    std::uint32_t box;
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, value);
        box = call_guest(rt, ctx, 0x08B62344u, {16u});
        put(rt, box + 4u, 1u);
        put(rt, box, vtable);
        put(rt, box + 8u, 1u);
        put(rt, box + 0xCu, value);
        put(rt, self + 4u, box);
    }
    ctx.gpr[2] = box;
    tail_call(ctx, 0x08B8E380u, {self + 4u, value, value, 1u});
}

// Member-function-pointer call, then a member-function-pointer tail call (37 instructions). this[0x14] / this[0x18] is the first pointer to member function
// {function, adjust} (adjust bit 0: virtual, so `function` is a vtable offset; adjust >> 1 is added to the object), this[0x1C] / this[0x20] the second.
// a2 = the address of the object pointer. The first call is callee(object + adjust >> 1, adjust, object, function, callee) and returns a pointer p; the tail call
// is target(p + adjust2 >> 1, p) with t9 = target. The registers v0 / v1 are left as the original leaves them for the tail callee.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual forms (bit 0 of the first adjust: the `bnez` lands on
// `lw v0, 0(a0)` and the callee is *(*receiver + function); bit 0 of the second: the `beqz` is not taken, v1 = this[0x1C] is loaded before the vtable word and the
// target is *(*a0 + v1), then the `j` into the shared epilogue); the routes only take the plain pointers.
void ptmf_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t target, receiver_next, result, v0, v1;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t adjust = word(rt, self + 0x18u);
        const std::uint32_t function = word(rt, self + 0x14u);
        const std::uint32_t object = word(rt, ctx.gpr[6]);
        const std::uint32_t receiver = object + static_cast<std::uint32_t>(sra1(adjust));
        std::uint32_t callee = function;
        if ((adjust & 1u) != 0u) callee = word(rt, word(rt, receiver) + function);
        result = call_guest(rt, ctx, callee, {receiver, adjust, object, function, callee});
        const std::uint32_t second = word(rt, self + 0x20u);
        const std::uint32_t shift = static_cast<std::uint32_t>(sra1(second));
        receiver_next = result + shift;
        if ((second & 1u) != 0u) {
            v1 = word(rt, self + 0x1Cu);
            v0 = word(rt, receiver_next) + v1;
            target = word(rt, v0);
        } else {
            target = word(rt, self + 0x1Cu);
            v0 = 0u;
            v1 = shift;
        }
    }
    ctx.gpr[2] = v0;
    ctx.gpr[3] = v1;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {receiver_next, result});
}

// The same with an extra argument (38 instructions): a1 = the object itself (not the address of a pointer), a2 = an extra value handed to the tail call as its
// second argument; the pointers are this[0x14] / [0x18] and this[0x24] / [0x28]. The first call is callee(object + adjust >> 1, object, function), the tail
// call target(p + adjust2 >> 1, extra).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual forms of both pointers, as in ptmf_tail.
void ptmf_tail_arg(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5], extra = ctx.gpr[6];
    std::uint32_t target, receiver_next, v0, v1;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t adjust = word(rt, self + 0x18u);
        const std::uint32_t receiver = object + static_cast<std::uint32_t>(sra1(adjust));
        const std::uint32_t function = word(rt, self + 0x14u);
        std::uint32_t callee = function;
        if ((adjust & 1u) != 0u) callee = word(rt, word(rt, receiver) + function);
        const std::uint32_t result = call_guest(rt, ctx, callee, {receiver, object, function});
        const std::uint32_t second = word(rt, self + 0x28u);
        const std::uint32_t shift = static_cast<std::uint32_t>(sra1(second));
        receiver_next = result + shift;
        if ((second & 1u) != 0u) {
            v1 = word(rt, self + 0x24u);
            v0 = word(rt, receiver_next) + v1;
            target = word(rt, v0);
        } else {
            target = word(rt, self + 0x24u);
            v0 = 0u;
            v1 = shift;
        }
    }
    ctx.gpr[2] = v0;
    ctx.gpr[3] = v1;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {receiver_next, extra});
}

// New holder (37 instructions): o = new 24-byte block (func_08A5BC20(24, 4)), func_08A86BDC(o, a1) constructs it, o[0] = vtable, o[5] = a2; a holder
// slot frame[0] = o; func_08A5D414(&frame[4], &frame[0]) fills the holder frame[4], func_08A7C5C8(a0, &frame[4]) hands it over and func_08A5B9EC(&frame[4])
// releases it. The result is the release's.
// Branches: none, the function is straight-line.
void new_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t target = ctx.gpr[4], source = ctx.gpr[5], extra = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {24u, 4u});
    (void)call_guest(rt, ctx, 0x08A86BDCu, {object, source});
    put(rt, object, vtable);
    put(rt, object + 0x14u, extra);
    put(rt, frame.at(0u), object);
    (void)call_guest(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A7C5C8u, {target, frame.at(4u)});
    take(ctx, call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)}, {}));
}

// Erase a string-keyed subtree (42 instructions, recursive through its own address): a0 = the owner, a1 = a node (null: nothing, v0 untouched). For each
// node, from the first: erase(owner, node[3]); the string pointer (node + string_offset) and the left child node[2] are read; unless the string is the
// shared empty one (0x08D74FD0 + 0xC), func_08B60804(string - 4, -1) drops its count and, when the answer is not positive, func_08B8542C(string - 0xC, &frame)
// frees it; then the node is freed with func_08A5BC54(node, 0x18, 4) and the walk goes on with the left child. v0 = the last free's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty string (the `beq` lands on the free of the node
// with a0 = node set there; its delay slot only sets a0 = string - 4). The compared cases take both outcomes of the count test.
void erase_str(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_address, std::uint32_t string_offset) {
    const std::uint32_t owner = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    do {
        (void)call_guest(rt, ctx, self_address, {owner, word(rt, node + 0xCu)});
        const std::uint32_t string = word(rt, node + string_offset);
        const std::uint32_t left = word(rt, node + 8u);
        const std::uint32_t rep = string - 0xCu;
        if (rep != 0x08D74FD0u) {
            const std::uint32_t count = call_guest(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu});
            if (static_cast<std::int32_t>(count) <= 0) (void)call_guest(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
        }
        ctx.gpr[2] = call_guest(rt, ctx, kFree, {node, 0x18u, 4u});
        node = left;
    } while (node != 0u);
}

// Holder pair (41 instructions): four temporaries on the frame (0, 4, 8, 0xC): func_08A69B3C(&t8) builds one, `second`(&t0, a2) fills t0, `third`(&t4, &t0)
// fills t4, func_08A744F0(&tC, &t8, &t4) combines them into tC, func_08A7C5C8(a0, &tC) hands tC over; tC, t4 and t0 are released (func_08A5B9EC) and
// func_08A69D68(&t8) disposes the first. The result is that last call's.
// Branches: none, the function is straight-line.
void holder_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t second, std::uint32_t third) {
    const std::uint32_t target = ctx.gpr[4], argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    (void)call_guest(rt, ctx, second, {frame.at(0u), argument});
    (void)call_guest(rt, ctx, third, {frame.at(4u), frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A744F0u, {frame.at(0xCu), frame.at(8u), frame.at(4u)});
    (void)call_guest(rt, ctx, 0x08A7C5C8u, {target, frame.at(0xCu)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0xCu)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    take(ctx, call_guest(rt, ctx, 0x08A69D68u, {frame.at(8u)}, {}));
}

// Release destructor (43 instructions): s = this[5]; this[0] = vtable (stored right after that load); if s != 0 its counter at +4 is decremented (ll / sc,
// one thread: a plain load and store) and, when it was 1, s's virtual slot +8 runs on s; then the counter at +8 is decremented the same way and, when it was
// 1, the virtual slot +0xC runs. this[0] = 0x08CBF488 (the base's vtable) and the tail call func_08B64654(this), with v0 = that vtable.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null s (the `beqz` lands on the final store: its delay slot is the
// first store of this[0]), and everything behind a first counter of 1: the compared cases all see a counter that is not 1 (the `bne` lands on the final store, its
// delay slot only loads the high half of the vtable address). For a counter of 1 the virtual slot +8 runs with a0 = s (the delay slot of the `jalr`), then the second
// counter is decremented the same way and, when it was 1, slot +0xC runs on s; the stores of both counters are the ll / sc loops read above, one thread.
void release_dtor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t shared = word(rt, self + 0x14u);
        put(rt, self, vtable);
        if (shared != 0u) {
            const std::uint32_t before = word(rt, shared + 4u);
            put(rt, shared + 4u, before - 1u);
            if (before == 1u) {
                (void)call_guest(rt, ctx, word(rt, word(rt, shared) + 8u), {shared});
                const std::uint32_t weak = word(rt, shared + 8u);
                put(rt, shared + 8u, weak - 1u);
                if (weak == 1u) (void)call_guest(rt, ctx, word(rt, word(rt, shared) + 0xCu), {shared});
            }
        }
        put(rt, self, 0x08CBF488u);
    }
    ctx.gpr[2] = 0x08CBF488u;
    tail_call(ctx, 0x08B64654u, {self});
}

// Vector reserve (63 instructions) of a vector of words: a0 = the vector {begin, end, end_of_storage}, a1 = n. A count above 0x3FFFFFFF calls
// func_08B5ADDC(message) (the length error, which does not return). When the capacity (in words) is below n: a new block of n words (func_08A5BC20(n * 4, 4)),
// copy(begin, end, block, &frame) moves the elements, destroy(this[0], this[1], &frame + 1) runs on the old range, the old block is freed with
// func_08A5BC54(old, capacity * 4, 4) when it exists, and the vector becomes {block, block + count * 4, block + n * 4}. v0 = the new end, or 0 when the
// capacity was enough.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length error (a0 = the message in the delay slot of the jal;
// the callee does not return), and the whole growth: every compared case already has the capacity (the `beqz` lands on the epilogue with v0 = the sltu result 0).
// The growth body was read word by word: the block, the copy (a3 = sp), the destroy (a2 = sp + 1) and the free (skipped by a `beqz` on a null old block, whose delay
// slot only computes the capacity in words) follow the original's order, and the three stores are end of storage, end, begin.
void vector_reserve(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t copy, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4], n = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (n > 0x3FFFFFFFu) {
        (void)call_guest(rt, ctx, 0x08B5ADDCu, {message});
        return;
    }
    const std::uint32_t storage_end = word(rt, self + 8u);
    const std::uint32_t begin = word(rt, self);
    const std::uint32_t capacity = static_cast<std::uint32_t>(static_cast<std::int32_t>(storage_end - begin) >> 2);
    if (!(capacity < n)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t end = word(rt, self + 4u);
    const std::uint32_t bytes = n << 2;
    const std::uint32_t count = static_cast<std::uint32_t>(static_cast<std::int32_t>(end - begin) >> 2);
    const std::uint32_t block = call_guest(rt, ctx, kAllocate, {bytes, 4u});
    (void)call_guest(rt, ctx, copy, {begin, end, block, frame.at(0u)});
    (void)call_guest(rt, ctx, destroy, {word(rt, self), word(rt, self + 4u), frame.at(1u)});
    const std::uint32_t old = word(rt, self);
    const std::uint32_t old_words = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, self + 8u) - old) >> 2);
    if (old != 0u) (void)call_guest(rt, ctx, kFree, {old, old_words << 2, 4u});
    const std::uint32_t new_end = block + (count << 2);
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, new_end);
    put(rt, self, block);
    ctx.gpr[2] = new_end;
    ctx.gpr[3] = block + bytes;
}

// Vector reserve through malloc (61 instructions) of a vector whose elements are 1 << shift bytes (words, or pairs of words): the same protocol as
// vector_reserve, but the block comes from func_08B35DA0(n << shift) (no alignment argument) and the old one is freed with func_08B35254(old) when it is not null;
// the length limit is 0x3FFFFFFF >> (shift - 2). copy(begin, end, block, &frame) and destroy(this[0], this[1], &frame + 1) are the member's. v0 = the new end,
// or 0 when the capacity was enough.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length error (the callee does not return) and the whole growth
// (every compared case already has the capacity: the `beqz` lands on the epilogue with v0 = 0). The growth body was read word by word as in vector_reserve; a null old
// block skips the free (the `beqz`, its delay slot only computes the new end's offset). n = 0 cannot reach the allocation: the capacity is never below it.
void reserve_malloc(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t copy, std::uint32_t destroy, unsigned shift) {
    const std::uint32_t self = ctx.gpr[4], n = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (n > (0x3FFFFFFFu >> (shift - 2u))) {
        (void)call_guest(rt, ctx, 0x08B5ADDCu, {message});
        return;
    }
    const std::uint32_t storage_end = word(rt, self + 8u);
    const std::uint32_t begin = word(rt, self);
    const std::uint32_t capacity = static_cast<std::uint32_t>(static_cast<std::int32_t>(storage_end - begin) >> shift);
    if (!(capacity < n)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t end = word(rt, self + 4u);
    const std::uint32_t count = static_cast<std::uint32_t>(static_cast<std::int32_t>(end - begin) >> shift);
    std::uint32_t block = 0u, bytes = 0u;
    if (n != 0u) {
        bytes = n << shift;
        block = call_guest(rt, ctx, 0x08B35DA0u, {bytes});
    }
    (void)call_guest(rt, ctx, copy, {begin, end, block, frame.at(0u)});
    (void)call_guest(rt, ctx, destroy, {word(rt, self), word(rt, self + 4u), frame.at(1u)});
    const std::uint32_t old = word(rt, self);
    if (old != 0u) (void)call_guest(rt, ctx, 0x08B35254u, {old});
    const std::uint32_t new_end = block + (count << shift);
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, new_end);
    put(rt, self, block);
    ctx.gpr[2] = new_end;
    ctx.gpr[3] = block + bytes;
}

void reserve_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t copy, std::uint32_t destroy) {
    reserve_malloc(rt, ctx, message, copy, destroy, 2u);
}
void reserve_pairs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t copy, std::uint32_t destroy) {
    reserve_malloc(rt, ctx, message, copy, destroy, 3u);
}

// Tree find (66 instructions): a0 = the tree (header at +4, root at +8), a1 = the address of the key's character pointer (the string's length is the word
// before its characters). Each node holds its key pointer at +0x10, left child +8, right child +0xC. The walk of tree_lower_bound (batch 77) finds the
// candidate; then, when it is not the header, the key is compared with the candidate's the other way round (func_08B36170(key, candidate key, min(lengths)),
// the length difference when it is 0) and the candidate is the answer when the order is not negative, else the header. v0 = the node or the header.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty tree (the `beqz` on the root lands on the comparison of the
// candidate with the header, equal: v0 = the header). Both directions of the walk, a candidate and the final comparison occur in the compared cases.
void tree_find(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4], key_slot = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t header = tree + 4u;
    std::uint32_t candidate = header;
    std::uint32_t node = word(rt, tree + 8u);
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
            if (static_cast<std::int32_t>(order) < 0) {
                node = word(rt, node + 0xCu);
            } else {
                const std::uint32_t left = word(rt, node + 8u);
                candidate = node;
                node = left;
            }
        } while (node != 0u);
    }
    std::uint32_t result = header;
    if (candidate != header) {
        const std::uint32_t key = word(rt, key_slot);
        const std::uint32_t key_length = word(rt, key - 12u);
        const std::uint32_t candidate_key = word(rt, candidate + 0x10u);
        const std::uint32_t candidate_length = word(rt, candidate_key - 12u);
        const std::uint32_t shorter = candidate_length < key_length ? candidate_length : key_length;
        std::uint32_t order = key_length - candidate_length;
        const std::uint32_t compared = call_guest(rt, ctx, 0x08B36170u, {key, candidate_key, shorter});
        if (compared != 0u) order = compared;
        if (static_cast<std::int32_t>(order) >= 0) result = candidate;
    }
    ctx.gpr[2] = result;
}

} // namespace

// func_XXXXXXXX (wrap_a): see wrap_a.
void func_0887CA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_a(rt, ctx, 0x08B9B6E0u, 60u); }
void func_0887CAA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_a(rt, ctx, 0x08B9B708u, 92u); }
void func_0887CAC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_a(rt, ctx, 0x08B9B73Cu, 108u); }
void func_0887CAF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_a(rt, ctx, 0x08B9B770u, 76u); }

// func_XXXXXXXX (float_setter): see float_setter.
void func_08AF4458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter(rt, ctx, 0x00000004u); }
void func_08AF4460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter(rt, ctx, 0x00000010u); }
void func_08AF4468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter(rt, ctx, 0x00000008u); }
void func_08AF4470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter(rt, ctx, 0x0000000Cu); }

// func_XXXXXXXX (copy_three): see copy_three.
void func_08AF43B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_three(rt, ctx, 0x00000014u, 0x00000018u, 0x0000001Cu); }
void func_08AF43D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_three(rt, ctx, 0x00000020u, 0x00000024u, 0x00000028u); }
void func_08AF43EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_three(rt, ctx, 0x0000002Cu, 0x00000030u, 0x00000034u); }

// func_XXXXXXXX (pair_tail): see pair_tail.
void func_08BB8280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_tail(rt, ctx, 0x088C7F64u, 0x088C85C0u); }
void func_08BB8328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_tail(rt, ctx, 0x088CA548u, 0x088CA28Cu); }
void func_08BB83E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_tail(rt, ctx, 0x088CBFD4u, 0x088CC7B0u); }

// func_XXXXXXXX (pair_call): see pair_call.
void func_08B878E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call(rt, ctx, 0x08C87D10u, 0x08B8782Cu); }
void func_08B8D51C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call(rt, ctx, 0x08C88200u, 0x08B8D470u); }
void func_08BEEBB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call(rt, ctx, 0x08C8BF28u, 0x08BEEB08u); }

// func_XXXXXXXX (guarded_zero): see guarded_zero.
void func_08B9C1F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757C0u, 0x08D7583Cu, 0xFFFF933Cu); }
void func_08BB9D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76050u, 0x08D76084u, 0xFFFF9B84u); }
void func_08BB9FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76048u, 0x08D76078u, 0xFFFF9B78u); }

// func_XXXXXXXX (shared_box): see shared_box.
void func_08BDB33C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_box(rt, ctx, 0x08CC5798u); }
void func_08BDB3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_box(rt, ctx, 0x08CC5780u); }
void func_08BDB484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_box(rt, ctx, 0x08CC5738u); }

// func_XXXXXXXX (ptmf_tail): see ptmf_tail.
void func_08BFBCB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail(rt, ctx); }
void func_08C08078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail(rt, ctx); }
void func_08C0A058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail(rt, ctx); }
void func_08C148D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail(rt, ctx); }

// func_XXXXXXXX (ptmf_tail_arg): see ptmf_tail_arg.
void func_08BEBD58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
void func_08BEBA90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BEFD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF2F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF34C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF3B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF5B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF7B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BF86E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BFC460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BFD460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BFE724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C03A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C04648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C0606C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C064C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C08BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C08ED4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C097CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C0EF94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C0F638(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C132E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C1572C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C15A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C29B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C2C928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C36BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08BEC040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }
void func_08C120E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_tail_arg(rt, ctx); }

// func_XXXXXXXX (new_holder): see new_holder.
void func_08B8850C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder(rt, ctx, 0x08CD4658u); }
void func_08BFD550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder(rt, ctx, 0x08CCEDA8u); }
void func_08C261D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder(rt, ctx, 0x08CD4080u); }

// func_XXXXXXXX (erase_str): see erase_str.
void func_08C00FEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_str(rt, ctx, 0x08C00FECu, 0x00000014u); }
void func_08C0DD50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_str(rt, ctx, 0x08C0DD50u, 0x00000010u); }
void func_08C3295C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_str(rt, ctx, 0x08C3295Cu, 0x00000010u); }
void func_08C32A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_str(rt, ctx, 0x08C32A04u, 0x00000010u); }

// func_XXXXXXXX (holder_pair): see holder_pair.
void func_08B86E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_pair(rt, ctx, 0x08A85834u, 0x08B86E58u); }
void func_08B8D810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_pair(rt, ctx, 0x08A5DBE8u, 0x08B8D7D0u); }
void func_08C3062C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_pair(rt, ctx, 0x08A6C358u, 0x08C305ECu); }

// func_XXXXXXXX (release_dtor): see release_dtor.
void func_08BB0408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_dtor(rt, ctx, 0x08CC2FA8u); }
void func_08BB055C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_dtor(rt, ctx, 0x08CC2F90u); }
void func_08BC71E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_dtor(rt, ctx, 0x08CC52B8u); }

// func_XXXXXXXX (vector_reserve): see vector_reserve.
void func_08BFA998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CA5130u, 0x08BF91ACu, 0x08BF919Cu); }
void func_08BFB8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CA5130u, 0x08BF8DBCu, 0x08BF8B48u); }
void func_08C22DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CB02C0u, 0x08C1FBD4u, 0x08C1FBC4u); }
void func_08C2BFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CB1E10u, 0x08C2BF34u, 0x08C2BF24u); }

// func_XXXXXXXX (tree_find): see tree_find.
void func_08C00B08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_find(rt, ctx); }
void func_08C1BD3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_find(rt, ctx); }
void func_08C2FA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_find(rt, ctx); }
void func_08C31E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_find(rt, ctx); }

// func_XXXXXXXX (vector reserve through malloc, words): see reserve_malloc.
void func_08B8EA8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_words(rt, ctx, 0x08C9F59Cu, 0x08B8E87Cu, 0x08B8E4D0u); }
void func_08C4A2C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_words(rt, ctx, 0x08CB4AECu, 0x08C499FCu, 0x08C499ECu); }
void func_08C4A3B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_words(rt, ctx, 0x08CB4AECu, 0x08C49904u, 0x08BBB838u); }

// func_XXXXXXXX (vector reserve through malloc, pairs of words): see reserve_malloc.
void func_08BA81B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_pairs(rt, ctx, 0x08CA0708u, 0x08BA8164u, 0x08B8E5D8u); }
void func_08BA9878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_pairs(rt, ctx, 0x08CA0730u, 0x08BA9338u, 0x08B8E594u); }
void func_08C4A8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reserve_pairs(rt, ctx, 0x08CB4AECu, 0x08C49EC8u, 0x08C49EB8u); }

// Batch 157: more members of the guarded_zero family (tools.family_rows), verified by synthetic cases.
void func_08B9C130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757B8u, 0x08D7586Cu, 0xFFFF936Cu); }
void func_08B9C194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757B8u, 0x08D7586Cu, 0xFFFF936Cu); }
void func_08B9C25C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757C0u, 0x08D7583Cu, 0xFFFF933Cu); }
void func_08B9C2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75768u, 0x08D75830u, 0xFFFF9330u); }
void func_08B9C324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75768u, 0x08D75830u, 0xFFFF9330u); }
void func_08B9C388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75770u, 0x08D75824u, 0xFFFF9324u); }
void func_08B9C3EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75770u, 0x08D75824u, 0xFFFF9324u); }
void func_08B9C450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75778u, 0x08D75818u, 0xFFFF9318u); }
void func_08B9C4B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75778u, 0x08D75818u, 0xFFFF9318u); }
void func_08B9C518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75780u, 0x08D7580Cu, 0xFFFF930Cu); }
void func_08B9C57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75780u, 0x08D7580Cu, 0xFFFF930Cu); }
void func_08B9C5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757C8u, 0x08D757E8u, 0xFFFF92E8u); }
void func_08B9C644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D757C8u, 0x08D757E8u, 0xFFFF92E8u); }
void func_08B9C6A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75790u, 0x08D757F4u, 0xFFFF92F4u); }
void func_08B9C70C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75790u, 0x08D757F4u, 0xFFFF92F4u); }
void func_08B9C770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75798u, 0x08D757DCu, 0xFFFF92DCu); }
void func_08B9C8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75798u, 0x08D757DCu, 0xFFFF92DCu); }
void func_08B9C938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75708u, 0x08D75800u, 0xFFFF9300u); }
void func_08B9C99C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75708u, 0x08D75800u, 0xFFFF9300u); }
void func_08B9CA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75788u, 0x08D75848u, 0xFFFF9348u); }
void func_08B9CA64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75788u, 0x08D75848u, 0xFFFF9348u); }
void func_08B9CAC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75758u, 0x08D75860u, 0xFFFF9360u); }
void func_08B9CB2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75758u, 0x08D75860u, 0xFFFF9360u); }
void func_08B9CB90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75760u, 0x08D75854u, 0xFFFF9354u); }
void func_08B9CBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75760u, 0x08D75854u, 0xFFFF9354u); }
void func_08BB9DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76050u, 0x08D76084u, 0xFFFF9B84u); }
void func_08BBA010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76048u, 0x08D76078u, 0xFFFF9B78u); }
void func_08BBDA38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76380u, 0x08D76394u, 0xFFFF9E94u); }
void func_08BBDA9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76358u, 0x08D763D0u, 0xFFFF9ED0u); }
void func_08BBDB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76358u, 0x08D763D0u, 0xFFFF9ED0u); }
void func_08BBDB64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76348u, 0x08D763E8u, 0xFFFF9EE8u); }
void func_08BBDBC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76348u, 0x08D763E8u, 0xFFFF9EE8u); }
void func_08BBDC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76338u, 0x08D76400u, 0xFFFF9F00u); }
void func_08BBDC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76338u, 0x08D76400u, 0xFFFF9F00u); }
void func_08BBDCF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76350u, 0x08D763DCu, 0xFFFF9EDCu); }
void func_08BBDD58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76350u, 0x08D763DCu, 0xFFFF9EDCu); }
void func_08BBDDBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762B8u, 0x08D76424u, 0xFFFF9F24u); }
void func_08BBDE20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762B8u, 0x08D76424u, 0xFFFF9F24u); }
void func_08BBDE84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76370u, 0x08D763ACu, 0xFFFF9EACu); }
void func_08BBDEE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76370u, 0x08D763ACu, 0xFFFF9EACu); }
void func_08BBDF4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76360u, 0x08D763C4u, 0xFFFF9EC4u); }
void func_08BBDFB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76360u, 0x08D763C4u, 0xFFFF9EC4u); }
void func_08BBE014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76368u, 0x08D763B8u, 0xFFFF9EB8u); }
void func_08BBE078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76368u, 0x08D763B8u, 0xFFFF9EB8u); }
void func_08BBE0DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76378u, 0x08D763A0u, 0xFFFF9EA0u); }
void func_08BBE140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76378u, 0x08D763A0u, 0xFFFF9EA0u); }
void func_08BBE1A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762D8u, 0x08D76388u, 0xFFFF9E88u); }
void func_08BBE208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762D8u, 0x08D76388u, 0xFFFF9E88u); }
void func_08BBE26C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762D0u, 0x08D7640Cu, 0xFFFF9F0Cu); }
void func_08BBE3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76340u, 0x08D763F4u, 0xFFFF9EF4u); }
void func_08BBE434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D76340u, 0x08D763F4u, 0xFFFF9EF4u); }
void func_08BBE498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762B0u, 0x08D76430u, 0xFFFF9F30u); }
void func_08BBE4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762B0u, 0x08D76430u, 0xFFFF9F30u); }
void func_08BBE560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762C8u, 0x08D76418u, 0xFFFF9F18u); }
void func_08BBE5C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762C8u, 0x08D76418u, 0xFFFF9F18u); }
void func_08BBE628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762C0u, 0x08D7643Cu, 0xFFFF9F3Cu); }
void func_08BBE68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D762C0u, 0x08D7643Cu, 0xFFFF9F3Cu); }
void func_08C4CF18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D75290u, 0x08D75878u, 0xFFFF9378u); }
void func_08C4CF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D77378u, 0x08D77380u, 0xFFFFAE80u); }
void func_08C4CFE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_zero(rt, ctx, 0x08D77378u, 0x08D77380u, 0xFFFFAE80u); }

} // namespace hot_batch_78
} // namespace gt::game

#define HOT78(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_78::name, gt::ReturnKind::kind)
HOT78(0x0887CA78u, func_0887CA78, Word);
HOT78(0x0887CAA0u, func_0887CAA0, Word);
HOT78(0x0887CAC8u, func_0887CAC8, Word);
HOT78(0x0887CAF0u, func_0887CAF0, Word);
HOT78(0x08AF4458u, func_08AF4458, Word);
HOT78(0x08AF4460u, func_08AF4460, Word);
HOT78(0x08AF4468u, func_08AF4468, Word);
HOT78(0x08AF4470u, func_08AF4470, Word);
HOT78(0x08AF43B4u, func_08AF43B4, Word);
HOT78(0x08AF43D0u, func_08AF43D0, Word);
HOT78(0x08AF43ECu, func_08AF43EC, Word);
HOT78(0x08BB8280u, func_08BB8280, Word);
HOT78(0x08BB8328u, func_08BB8328, Word);
HOT78(0x08BB83E8u, func_08BB83E8, Word);
HOT78(0x08B878E8u, func_08B878E8, Word);
HOT78(0x08B8D51Cu, func_08B8D51C, Word);
HOT78(0x08BEEBB4u, func_08BEEBB4, Word);
HOT78(0x08B9C1F8u, func_08B9C1F8, Word);
HOT78(0x08BB9D84u, func_08BB9D84, Word);
HOT78(0x08BB9FACu, func_08BB9FAC, Word);
HOT78(0x08BDB33Cu, func_08BDB33C, Word);
HOT78(0x08BDB3C0u, func_08BDB3C0, Word);
HOT78(0x08BDB484u, func_08BDB484, Word);
HOT78(0x08BFBCB8u, func_08BFBCB8, Word);
HOT78(0x08C08078u, func_08C08078, Word);
HOT78(0x08C0A058u, func_08C0A058, Word);
HOT78(0x08C148D0u, func_08C148D0, Word);
HOT78(0x08BEBD58u, func_08BEBD58, Word);
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
HOT78(0x08BEBA90u, func_08BEBA90, Word);
HOT78(0x08BEFD54u, func_08BEFD54, Word);
HOT78(0x08BF2F18u, func_08BF2F18, Word);
HOT78(0x08BF34C0u, func_08BF34C0, Word);
HOT78(0x08BF3B14u, func_08BF3B14, Word);
HOT78(0x08BF5B70u, func_08BF5B70, Word);
HOT78(0x08BF7B00u, func_08BF7B00, Word);
HOT78(0x08BF86E8u, func_08BF86E8, Word);
HOT78(0x08BFC460u, func_08BFC460, Word);
HOT78(0x08BFD460u, func_08BFD460, Word);
HOT78(0x08BFE724u, func_08BFE724, Word);
HOT78(0x08C03A90u, func_08C03A90, Word);
HOT78(0x08C04648u, func_08C04648, Word);
HOT78(0x08C0606Cu, func_08C0606C, Word);
// HOT78(0x08C064C8u, func_08C064C8, Word);  // no comparable synthetic case yet
HOT78(0x08C08BD4u, func_08C08BD4, Word);
HOT78(0x08C08ED4u, func_08C08ED4, Word);
HOT78(0x08C097CCu, func_08C097CC, Word);
HOT78(0x08C0EF94u, func_08C0EF94, Word);
HOT78(0x08C0F638u, func_08C0F638, Word);
HOT78(0x08C132E8u, func_08C132E8, Word);
HOT78(0x08C1572Cu, func_08C1572C, Word);
HOT78(0x08C15A84u, func_08C15A84, Word);
HOT78(0x08C29B8Cu, func_08C29B8C, Word);
HOT78(0x08C2C928u, func_08C2C928, Word);
HOT78(0x08C36BD4u, func_08C36BD4, Word);
HOT78(0x08BEC040u, func_08BEC040, Word);
HOT78(0x08C120E8u, func_08C120E8, Word);
HOT78(0x08B8850Cu, func_08B8850C, Word);
HOT78(0x08BFD550u, func_08BFD550, Word);
HOT78(0x08C261D8u, func_08C261D8, Word);
HOT78(0x08C00FECu, func_08C00FEC, Word);
HOT78(0x08C0DD50u, func_08C0DD50, Word);
HOT78(0x08C3295Cu, func_08C3295C, Word);
HOT78(0x08C32A04u, func_08C32A04, Word);
HOT78(0x08B86E98u, func_08B86E98, Word);
HOT78(0x08B8D810u, func_08B8D810, Word);
HOT78(0x08C3062Cu, func_08C3062C, Word);
HOT78(0x08BB0408u, func_08BB0408, Word);
HOT78(0x08BB055Cu, func_08BB055C, Word);
HOT78(0x08BC71E0u, func_08BC71E0, Word);
HOT78(0x08BFA998u, func_08BFA998, Word);
HOT78(0x08BFB8DCu, func_08BFB8DC, Word);
HOT78(0x08C22DDCu, func_08C22DDC, Word);
HOT78(0x08C2BFA0u, func_08C2BFA0, Word);
HOT78(0x08C00B08u, func_08C00B08, Word);
HOT78(0x08C1BD3Cu, func_08C1BD3C, Word);
HOT78(0x08C2FA50u, func_08C2FA50, Word);
HOT78(0x08C31E0Cu, func_08C31E0C, Word);
HOT78(0x08B8EA8Cu, func_08B8EA8C, Word);
HOT78(0x08C4A2C4u, func_08C4A2C4, Word);
HOT78(0x08C4A3B8u, func_08C4A3B8, Word);
HOT78(0x08BA81B4u, func_08BA81B4, Word);
HOT78(0x08BA9878u, func_08BA9878, Word);
HOT78(0x08C4A8F8u, func_08C4A8F8, Word);
HOT78(0x08B9C130u, func_08B9C130, Word);
HOT78(0x08B9C194u, func_08B9C194, Word);
HOT78(0x08B9C25Cu, func_08B9C25C, Word);
HOT78(0x08B9C2C0u, func_08B9C2C0, Word);
HOT78(0x08B9C324u, func_08B9C324, Word);
HOT78(0x08B9C388u, func_08B9C388, Word);
HOT78(0x08B9C3ECu, func_08B9C3EC, Word);
HOT78(0x08B9C450u, func_08B9C450, Word);
HOT78(0x08B9C4B4u, func_08B9C4B4, Word);
HOT78(0x08B9C518u, func_08B9C518, Word);
HOT78(0x08B9C57Cu, func_08B9C57C, Word);
HOT78(0x08B9C5E0u, func_08B9C5E0, Word);
HOT78(0x08B9C644u, func_08B9C644, Word);
HOT78(0x08B9C6A8u, func_08B9C6A8, Word);
HOT78(0x08B9C70Cu, func_08B9C70C, Word);
HOT78(0x08B9C770u, func_08B9C770, Word);
HOT78(0x08B9C8D4u, func_08B9C8D4, Word);
HOT78(0x08B9C938u, func_08B9C938, Word);
HOT78(0x08B9C99Cu, func_08B9C99C, Word);
HOT78(0x08B9CA00u, func_08B9CA00, Word);
HOT78(0x08B9CA64u, func_08B9CA64, Word);
HOT78(0x08B9CAC8u, func_08B9CAC8, Word);
HOT78(0x08B9CB2Cu, func_08B9CB2C, Word);
HOT78(0x08B9CB90u, func_08B9CB90, Word);
HOT78(0x08B9CBF4u, func_08B9CBF4, Word);
HOT78(0x08BB9DE8u, func_08BB9DE8, Word);
HOT78(0x08BBA010u, func_08BBA010, Word);
HOT78(0x08BBDA38u, func_08BBDA38, Word);
HOT78(0x08BBDA9Cu, func_08BBDA9C, Word);
HOT78(0x08BBDB00u, func_08BBDB00, Word);
HOT78(0x08BBDB64u, func_08BBDB64, Word);
HOT78(0x08BBDBC8u, func_08BBDBC8, Word);
HOT78(0x08BBDC2Cu, func_08BBDC2C, Word);
HOT78(0x08BBDC90u, func_08BBDC90, Word);
HOT78(0x08BBDCF4u, func_08BBDCF4, Word);
HOT78(0x08BBDD58u, func_08BBDD58, Word);
HOT78(0x08BBDDBCu, func_08BBDDBC, Word);
HOT78(0x08BBDE20u, func_08BBDE20, Word);
HOT78(0x08BBDE84u, func_08BBDE84, Word);
HOT78(0x08BBDEE8u, func_08BBDEE8, Word);
HOT78(0x08BBDF4Cu, func_08BBDF4C, Word);
HOT78(0x08BBDFB0u, func_08BBDFB0, Word);
HOT78(0x08BBE014u, func_08BBE014, Word);
HOT78(0x08BBE078u, func_08BBE078, Word);
HOT78(0x08BBE0DCu, func_08BBE0DC, Word);
HOT78(0x08BBE140u, func_08BBE140, Word);
HOT78(0x08BBE1A4u, func_08BBE1A4, Word);
HOT78(0x08BBE208u, func_08BBE208, Word);
HOT78(0x08BBE26Cu, func_08BBE26C, Word);
HOT78(0x08BBE3D0u, func_08BBE3D0, Word);
HOT78(0x08BBE434u, func_08BBE434, Word);
HOT78(0x08BBE498u, func_08BBE498, Word);
HOT78(0x08BBE4FCu, func_08BBE4FC, Word);
HOT78(0x08BBE560u, func_08BBE560, Word);
HOT78(0x08BBE5C4u, func_08BBE5C4, Word);
HOT78(0x08BBE628u, func_08BBE628, Word);
HOT78(0x08BBE68Cu, func_08BBE68C, Word);
HOT78(0x08C4CF18u, func_08C4CF18, Word);
HOT78(0x08C4CF7Cu, func_08C4CF7C, Word);
HOT78(0x08C4CFE0u, func_08C4CFE0, Word);
