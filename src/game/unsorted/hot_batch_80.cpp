// Hot batch 80: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet
// decompiled, run on the five routes with --decomp off; batch 79 excluded with --exclude-ref) in two groups. Before reading any member its address was searched
// for in src/ and in the branch of batch 79: the ones that earlier batches wrote and left unregistered (the "clamp", "release a vector" and organizer families, ...) are
// not rewritten here.
//
// Group 1, shape families (`tools.family_members --list --limit 0 --any-template --min-reached 1`): one member of each family was read in full by hand against the
// asm (rabbitizer disassembly with the relocations applied, data addresses taken from the relocation's own segment; the generated code was not used), and every
// other member was compared with it word by word by the tool: the words that differ are only the relocated constants, a displacement and the jal / j targets, never a
// register or a stack offset (no `!!` line; the one member that did differ by a register, func_08B8705C, got its own wrapper). The constants of each member come from its
// own relocated words and `--check` re-reads every row of this file against them. Their classes aren't known yet, so they keep their addresses as names.
//  - "string assign" (52 instructions): the reference-counted string's copy assignment, func_08B855E0 with 984,732 calls on the routes (2);
//  - "vector erase of pointers" (50): func_08B85798 with 14,545 calls (2); "new holder from a temporary" (43) (2 + a sibling that moves a3 instead of a2);
//  - "erase a string-keyed subtree with a holder" (44, recursive) (2); "forward to a member's virtual slot" (11) (2);
//  - small ones: "pair call" (14), "set a1, tail call" (3), "fields to arguments, tail call" (3), "three float stores" (7), "one float store" (3),
//    "call, then tail call with a constant" (11), "vtable, call, tail call at an offset" (13), "call, vtable, tail call" (13), "vtable, destructor, free" (15).
//
// Group 2, singles: reached functions with no sibling of the same shape, ordered by calls on the five routes (up to 40 instructions); each was read by hand, word by
// word, with its branch targets and delay slots.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep the
// caller's value, which call_guest copies from ctx. Every call goes through `call_keep`, which leaves the callee's v0, v1 and f0 in the registers like the original
// (a callee that sets no v0 hands the previous one on: func_089FE57C and func_088B46A8 mismatched on that in the first gate run).
//
// Written but not registered (the commented HOT80 lines above say why): func_08870448, func_08BD2D24, func_08BF7980, func_08C2B76C — no compared case changed memory, so the
// gate cannot see their stores. Left out entirely: see the ROADMAP entry of this batch.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_80 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAllocate = 0x08A5BC20u;     // (size, alignment): a new block
constexpr std::uint32_t kFree = 0x08A5BC54u;         // (block, size, alignment)
constexpr std::uint32_t kReportNull = 0x08A5BB7Cu;   // (message): the original's null-object assert
constexpr std::uint32_t kNullMessage = 0x08CB0284u;  // the message the null-object asserts of the holder code pass

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

inline std::int32_t sra1(std::uint32_t value) { return static_cast<std::int32_t>(value) >> 1; }

// The high half the loader writes into a `lui` of a relocated address (the paired low half is signed).
inline std::uint32_t hi16(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Group 1: families
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// Pair call, 14 instructions (batch 78's 15-instruction shape without the move of a3 into t0): the two words at `pair` and +4 are copied to frame[0] / frame[4];
// then callee(a0, a1, &frame[0], a2) with a3 = the caller's a2, and the two words also stay in t0 / t1 (the callee's 5th and 6th arguments). The result is the
// callee's.
// Branches: none, the function is straight-line.
void pair_call_e(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pair, std::uint32_t callee) {
    const std::uint32_t second = word(rt, pair + 4u);
    const std::uint32_t first = word(rt, pair);
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), first);
    put(rt, frame.at(4u), second);
    take(ctx, call_keep(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[6], first, second}, {}));
}

// Set a1 to a constant and tail call (3 instructions): tail_call(target)(a0, constant). v0 is untouched.
// Branches: none, the function is straight-line.
void const_tail(psprecomp::AllegrexContext &ctx, std::uint32_t constant, std::uint32_t target) {
    tail_call(ctx, target, {ctx.gpr[4], constant});
}

// Fields to arguments, tail call (3 instructions): a1 = *(this + offset_a1), a0 = *(this + offset_a0) (loaded in the delay slot, after a1), then the jump.
// Branches: none, the function is straight-line.
void fields_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset_a1, std::uint32_t target, std::uint32_t offset_a0) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = word(rt, self + offset_a1);
    const std::uint32_t second = word(rt, self + offset_a0);
    tail_call(ctx, target, {second, first});
}

// Three float stores through a sub-object (7 instructions): p = this[3] is reloaded before every store; (p + 0x30) = f12, (p + 0x34) = f13, (p + 0x38) = f14
// (the bits of the float arguments). v0 = the third reload, v1 = the second.
// Branches: none, the function is straight-line.
void float_setter3(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = word(rt, self + 0xCu);
    put(rt, first + 0x30u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    const std::uint32_t second = word(rt, self + 0xCu);
    put(rt, second + 0x34u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    const std::uint32_t third = word(rt, self + 0xCu);
    put(rt, third + 0x38u, std::bit_cast<std::uint32_t>(ctx.fpr[14]));
    ctx.gpr[2] = third;
    ctx.gpr[3] = second;
}

// Float store through a sub-object (3 instructions): p = this[3]; p[offset] = f12 (the bits of the float argument). v0 = p.
// Branches: none, the function is straight-line.
void float_setter1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t pointer = word(rt, ctx.gpr[4] + 0xCu);
    put(rt, pointer + offset, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = pointer;
}

// Call, then tail call with the result (12 instructions): r = first(a0, ...) (no argument set here); then second(r.v0) as a tail call. v0 / v1 / f0 are
// first's.
// Branches: none, the function is straight-line.
void func_08A101D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, 0x08C149ACu, {}, {});
    }
    take(ctx, result);
    tail_call(ctx, 0x08A10020u, {result.v0});
}

// Add an offset to a field and tail call (9 instructions): v = this[0]; if v == 0 nothing happens (v0 = 0, v1 = a1: the delay slot still computes v + a1);
// otherwise this[0] = v + a1 (stored in the delay slot of the jump) and tail_call(target)(v + a1, a1, this) with v0 = v.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the non-null field (the `beqz` falls into the delay slot of the jump, which stores
// this[0] = v + a1; v0 = v, v1 = v + a1, a2 = this and the jump to 0x08908F54 with a0 = v + a1 from the first delay slot). The compared cases all see a null field.
void func_08BD2D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], offset = ctx.gpr[5];
    const std::uint32_t old = word(rt, self);
    const std::uint32_t moved = old + offset;
    if (old == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = moved;
        return;
    }
    put(rt, self, moved);
    ctx.gpr[2] = old;
    ctx.gpr[3] = moved;
    tail_call(ctx, 0x08908F54u, {moved, offset, self});
}

// Call, then tail call with a constant (11 instructions): callee(this); then tail_call(target)(this, constant). v0 / v1 / f0 are the callee's.
// Branches: none, the function is straight-line.
void call_then_tail_const(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t constant, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, callee, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, target, {self, constant});
}

// Set the vtable, call, tail call at an offset (13 instructions): this[0] = vtable; callee(this); then tail_call(target)(this + offset). v0 / v1 / f0 are the
// callee's.
// Branches: none, the function is straight-line.
void vtable_call_tail_offset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t callee, std::uint32_t offset,
                             std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, vtable);
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, callee, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, target, {self + offset});
}

// Call, then set the vtable, then tail call (13 instructions): callee(this); this[0] = vtable (the high half in v0 is built after the call); tail_call(target)(this).
// v0 = vtable (the address the `addiu` leaves in v0), v1 = the callee's.
// Branches: none, the function is straight-line.
void call_vtable_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t vtable, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, callee, {self}, {});
        put(rt, self, vtable);
    }
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    tail_call(ctx, target, {self});
}

// Vtable store, base destructor, free (15 instructions): this[0] = vtable; callee(this); then tail_call(free)(this, size, 4) (the block's size and alignment).
// v0 / v1 / f0 are the callee's.
// Branches: none, the function is straight-line.
void vtable_dtor_free(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t callee, std::uint32_t size) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, vtable);
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, callee, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, kFree, {self, size, 4u});
}

// Destroy an object with two holders (20 instructions): this[0] = vtable; the holders at this + 0x18 and this + 0x14 are released (func_08A5B9EC), then
// func_08A780B0(this) and the free tail call (this, 0x1C, 4). v0 / v1 / f0 are func_08A780B0's.
// Branches: none, the function is straight-line.
void func_08B87724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CB8BD8u);
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, kReleaseLocal, {self + 0x18u});
        (void)call_keep(rt, ctx, kReleaseLocal, {self + 0x14u});
        result = call_keep(rt, ctx, 0x08A780B0u, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, kFree, {self, 0x1Cu, 4u});
}

// Destroy an object with an embedded sub-object (18 instructions): this[0] = vtable; func_08B48804(this + 0x14); func_08A780B0(this); free (this, 0x1C, 4).
// v0 / v1 / f0 are func_08A780B0's.
// Branches: none, the function is straight-line.
void func_08A37210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CD1C30u);
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B48804u, {self + 0x14u});
        result = call_keep(rt, ctx, 0x08A780B0u, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, kFree, {self, 0x1Cu, 4u});
}

// Destroy an object with a listener (28 instructions): this[0] = vtable; when this[6] (the listener) is set its virtual slot 0 runs with (listener,
// this[5]) and both fields are cleared (this[6] first); then func_08A780B0(this) and the free tail call (this, 0x1C, 4). v0 / v1 / f0 are func_08A780B0's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null listener (the `beqz` lands on the call of func_08A780B0, skipping the
// virtual call and both stores; its delay slot only sets a0). The compared cases all have a listener.
void func_08A1C55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CD0B38u);
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t listener = word(rt, self + 0x18u);
        if (listener != 0u) {
            const std::uint32_t function = word(rt, word(rt, listener));
            (void)call_keep(rt, ctx, function, {listener, word(rt, self + 0x14u)});
            put(rt, self + 0x18u, 0u);
            put(rt, self + 0x14u, 0u);
        }
        result = call_keep(rt, ctx, 0x08A780B0u, {self}, {});
    }
    take(ctx, result);
    tail_call(ctx, kFree, {self, 0x1Cu, 4u});
}

// Fill a holder and hand it over (17 instructions): func_08A83160(&holder, a1, this[2], this[3]) fills a temporary holder, func_08A61584(a1, &holder) takes it,
// and func_08A5B9EC(&holder) releases it. v0 / v1 / f0 are the release's.
// Branches: none, the function is straight-line.
void func_08C20F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t third = word(rt, self + 8u);
    const std::uint32_t fourth = word(rt, self + 0xCu);
    (void)call_keep(rt, ctx, 0x08A83160u, {frame.at(0u), target, third, fourth});
    (void)call_keep(rt, ctx, 0x08A61584u, {target, frame.at(0u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)}, {}));
}

// Member-function-pointer call (19 instructions): this[1] / this[2] is a pointer to member function {function, adjust} (adjust bit 0: virtual, so `function`
// is a vtable offset; adjust >> 1 is added to the object); the callee runs as callee(a1 + adjust >> 1, a2, a2, this). The result is the callee's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual form (bit 0 of the adjust: v1 = this[1], v0 = *receiver + v1, the callee is
// *v0 after the jump to 0x08C024F8, whose delay slot loads it); the routes only take the plain pointer (the `beqz` lands on `lw v0, 4(a3)`).
void func_08C024C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5], extra = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t adjust = word(rt, self + 8u);
    const std::uint32_t receiver = object + static_cast<std::uint32_t>(sra1(adjust));
    std::uint32_t callee;
    if ((adjust & 1u) != 0u) {
        const std::uint32_t function = word(rt, self + 4u);
        callee = word(rt, word(rt, receiver) + function);
    } else {
        callee = word(rt, self + 4u);
    }
    take(ctx, call_keep(rt, ctx, callee, {receiver, extra, extra, self}, {}));
}

// Guarded static object, constructor with a size of 12 (35 instructions; batch 76's "no size" shape with the constructor's third argument 0xC):
// `static T x(source, 0xC);` inside a function. If the guard byte is 0 and func_08B6441C(guard) lets this caller build it, object != 0: constructor(object,
// source, 0xC), object[0] = vtable (stored before the next call runs), registrar() (the accessor of another function-local static; it reads no argument);
// then the tail call func_08B64494(guard).
// v0 is hi(guard) when the guard byte is already set, 0 when the acquire fails, else whatever the release leaves.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C answering 0 (the beqz lands on the epilogue with v0 = 0; its delay slot
// only sets v1 = the object's address).
void func_08B9F960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08D754A8u, object = 0x08D75B8Cu;
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) != 0) {
            ctx.gpr[2] = hi16(guard);
            return;
        }
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) == 0u) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = object;
            return;
        }
        (void)call_keep(rt, ctx, 0x08B9F91Cu, {object, 0x08CA016Cu, 0xCu});
        put(rt, object, 0x08CC0E60u);
        (void)call_keep(rt, ctx, 0x08B9D1F0u, {});
    }
    tail_call(ctx, 0x08B64494u, {guard});
}

// Release a shared pointer (35 instructions): s = this[0]; a null s leaves v0 alone. The counter at s + 4 is decremented (ll / sc, one thread: a plain load and
// store), v0 = 1; when it was 1 the virtual slot +8 of s runs on s, then the counter at s + 8 is decremented the same way (v0 = 1) and, when it was 1, the
// virtual slot +0xC runs on s as a tail call (v0 = the vtable, v1 = s + 8).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null s (the `beqz` lands on the epilogue with v0 untouched; its delay slot only sets
// v1 = 4) and a weak counter of 1 (the final tail call: v0 = the vtable word loaded, v1 = s + 8, t9 = slot 0xC, a0 = s, then the jump with the stack released in its delay slot).
void func_08BAEC80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t shared = word(rt, self);
    if (shared == 0u) {
        ctx.gpr[3] = 4u;
        return;
    }
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t strong = word(rt, shared + 4u);
        put(rt, shared + 4u, strong - 1u);
        ctx.gpr[2] = 1u;
        ctx.gpr[3] = shared + 4u;
        if (strong != 1u) return;
        (void)call_keep(rt, ctx, word(rt, word(rt, shared) + 8u), {shared});
        const std::uint32_t weak = word(rt, shared + 8u);
        put(rt, shared + 8u, weak - 1u);
        ctx.gpr[2] = 1u;
        ctx.gpr[3] = shared + 8u;
        if (weak != 1u) return;
    }
    const std::uint32_t vtable = word(rt, shared);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = shared + 8u;
    tail_call(ctx, word(rt, vtable + 0xCu), {shared});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Group 2: singles
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_089FE57C(this, flag): when func_089673F4(this) answers non-zero, a temporary holder (the frame) is filled from this + 0x120 (func_08A17C48) and
// from this (func_08A180CC), then every element of the chain that starts at this[0x3D] (the next element = func_08968A38(element)) for which
// func_08A00448(this, element) answers non-zero is added to the holder (func_08A18494(&holder, element)). In every case func_08A01C30(this, flag) ends the
// function, and its v0 / v1 / f0 are the result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every iteration of the chain (the compared cases all have an empty chain: the first
// `beqz` lands on the final call). The loop was read word by word: func_08A00448(this, element) answering 0 skips func_08A18494 (the `beqz` lands on the call for the next element, whose delay
// slot already sets a1), and the `bnez` on the next element jumps back to the top of the body (a0 = this).
void func_089FE57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (call_keep(rt, ctx, 0x089673F4u, {self}) != 0u) {
        (void)call_keep(rt, ctx, 0x08A17C48u, {frame.at(0u), self + 0x120u});
        (void)call_keep(rt, ctx, 0x08A180CCu, {frame.at(0u), self});
        std::uint32_t element = word(rt, self + 0xF4u);
        while (element != 0u) {
            if (call_keep(rt, ctx, 0x08A00448u, {self, element}) != 0u) (void)call_keep(rt, ctx, 0x08A18494u, {frame.at(0u), element});
            element = call_keep(rt, ctx, 0x08968A38u, {element});
        }
    }
    take(ctx, call_keep(rt, ctx, 0x08A01C30u, {self, flag}, {}));
}

// func_089294F8(this): the answer of func_089279E0(this[2]) is a small index; the jump table at 0x08C8A890 (read from the ELF) maps 0..5 to 0..5, anything
// else answers 0. v0 = the answer, v1 = the index.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an index of 6 or more (the `beqz` lands on the epilogue with v0 = 0 from its delay slot).
void func_089294F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, 0x089279E0u, {word(rt, ctx.gpr[4] + 8u)}, {});
    }
    ctx.gpr[3] = result.v0;
    ctx.gpr[2] = result.v0 < 6u ? result.v0 : 0u;
}

// func_0887CA00(this, a1): s = func_08AF293C(a1) & 0x180; func_08AF2924(a1); then the tail call func_0887C8F0(this + 4, a1, s != 0, 0). v0 / v1 / f0 are
// func_08AF2924's.
// Branches: none, the function is straight-line.
void func_0887CA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestResult result;
    std::uint32_t flags;
    {
        GuestFrame frame(ctx, 0x10u);
        flags = call_keep(rt, ctx, 0x08AF293Cu, {other}) & 0x180u;
        result = call_keep(rt, ctx, 0x08AF2924u, {other}, {});
    }
    take(ctx, result);
    tail_call(ctx, 0x0887C8F0u, {self + 4u, other, flags != 0u ? 1u : 0u, 0u});
}

// func_08C467DC(this, other): when this == other the result matrix is built in a temporary (func_08C464BC(&temp, other), nine words) and copied over this
// (four words at a time, then the last one); otherwise func_08C464BC(this, other) writes it directly. v0 / v1 are the callee's when the arguments differ;
// when they are the same v0 = the last word copied and v1 = the sixth word of the temporary.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this == other, the whole temporary-and-copy path (the `bne` falls through into the call with a0 = sp
// from its delay slot; the loop copies four words per round for two rounds, then the ninth word). The routes only call it with two different matrices.
void func_08C467DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    if (self != other) {
        take(ctx, call_keep(rt, ctx, 0x08C464BCu, {self, other}, {}));
        return;
    }
    (void)call_keep(rt, ctx, 0x08C464BCu, {frame.at(0u), other});
    std::uint32_t source = frame.at(0u), destination = self, v1 = 0u;
    do {
        const std::uint32_t w0 = word(rt, source), w1 = word(rt, source + 4u), w2 = word(rt, source + 8u), w3 = word(rt, source + 0xCu);
        put(rt, destination, w0);
        put(rt, destination + 4u, w1);
        put(rt, destination + 8u, w2);
        put(rt, destination + 0xCu, w3);
        v1 = w1;
        source += 0x10u;
        destination += 0x10u;
    } while (source != frame.at(0x20u));
    const std::uint32_t last = word(rt, source);
    put(rt, destination, last);
    ctx.gpr[2] = last;
    ctx.gpr[3] = v1;
}

// func_088CDCFC(this, other): func_088CDE3C(this, other), then three values read from `other` through its vtable: this[0x44] = (byte) slot 0x24, this[0x48] =
// slot 0x28 (a float), this[0x4C] = slot 0x2C, this[0x50] = slot 0x30. v0 / v1 / f0 are the last call's.
// Branches: none, the function is straight-line.
void func_088CDCFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x088CDE3Cu, {self, other});
        std::uint32_t slot = word(rt, word(rt, other) + 0x24u);
        result = call_keep(rt, ctx, slot, {other}, {});
        rt.memory().store8(self + 0x44u, static_cast<std::uint8_t>(result.v0));
        slot = word(rt, word(rt, other) + 0x28u);
        result = call_keep(rt, ctx, slot, {other}, {});
        put(rt, self + 0x48u, std::bit_cast<std::uint32_t>(result.f0));
        slot = word(rt, word(rt, other) + 0x2Cu);
        result = call_keep(rt, ctx, slot, {other}, {});
        put(rt, self + 0x4Cu, std::bit_cast<std::uint32_t>(result.f0));
        slot = word(rt, word(rt, other) + 0x30u);
        result = call_keep(rt, ctx, slot, {other}, {});
        put(rt, self + 0x50u, std::bit_cast<std::uint32_t>(result.f0));
    }
    take(ctx, result);
}

// func_08A1AAB8(this): o = func_08A1AA6C(); when o != 0 and func_08B1A844() == 0: f = (float)(int)(*(0x08CFFAAC))[0xF0] * 1.0e-6; unless the byte at
// 0x08CFFB70 is set, o's virtual slot 0x11C runs with f; then slot 0x120 runs with f as a tail call. When o == 0 the function leaves with v0 = 0, and when
// func_08B1A844 answered non-zero v0 = hi(0x08CFFAAC) (the delay slot of that branch is the `lui`).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object (the `beqz` lands on the epilogue, v0 = 0), a busy answer from func_08B1A844
// (the `bnez` lands on the epilogue with v0 = hi(0x08CFFAAC) from its delay slot, the `lui`) and the flag byte set (the `bnez` skips the first virtual call, its delay slot already
// computed the float, and lands on `lw v0, 0(s0)` for the tail call).
void func_08A1AAB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kTimeBase = 0x08CFFAACu, kFlag = 0x08CFFB70u;
    std::uint32_t object;
    {
        GuestFrame frame(ctx, 0x20u);
        object = call_keep(rt, ctx, 0x08A1AA6Cu, {});
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t busy = call_keep(rt, ctx, 0x08B1A844u, {});
        if (busy != 0u) {
            ctx.gpr[2] = hi16(kTimeBase);
            return;
        }
        const std::uint32_t block = word(rt, kTimeBase);
        const std::uint32_t skip = rt.memory().load8(kFlag);
        const float ticks = static_cast<float>(static_cast<std::int32_t>(word(rt, block + 0xF0u)));
        const float seconds = psp_mul(ticks, std::bit_cast<float>(0x358637BDu));
        if (skip == 0u) (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0x11Cu), {object}, {seconds});
        ctx.fpr[12] = seconds;
    }
    const std::uint32_t vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    tail_call(ctx, word(rt, vtable + 0x120u), {object});
}

// func_0892BCC0(this): func_08B22694(this); s = this[0x98]; its first word is an object that, when set, has its virtual slot 2 run as (object, 1, this[0x18],
// s + 4); then func_0892B7E8(this[0x98] + 0x78, s + 4, s + 4) and the tail call func_0892B410(this[0x98] + 0x90, (s + 4)[0xE]). A null object leaves v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object (the `beqz` lands on the epilogue with v0 = 0; its delay slot only sets a0).
void func_0892BCC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t tail_first, tail_second;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B22694u, {self});
        const std::uint32_t state = word(rt, self + 0x260u);
        const std::uint32_t block = state + 4u;
        const std::uint32_t object = word(rt, state);
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t slot = word(rt, word(rt, object) + 8u);
        (void)call_keep(rt, ctx, slot, {object, 1u, word(rt, self + 0x60u), block});
        (void)call_keep(rt, ctx, 0x0892B7E8u, {word(rt, self + 0x260u) + 0x78u, block, block});
        tail_first = word(rt, self + 0x260u) + 0x90u;
        tail_second = word(rt, block + 0x38u);
    }
    tail_call(ctx, 0x0892B410u, {tail_first, tail_second});
}

// func_0888A058(this): builds a request on the frame from this[0]'s two words and the pair at 0x08C89A6C; func_08BAA488(&out, a1, a2, a3, t0 = &request)
// fills a 20-byte result at frame + 0x40, whose five words are copied to frame + 0x18, and func_08BAA850(&out2, &request, &zero_word, &copy) ends the function.
// v0 / v1 / f0 are the last call's.
// Branches: none, the function is straight-line.
void func_0888A058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kPair = 0x08C89A6Cu;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t head = word(rt, self);
    const std::uint32_t pair_second = word(rt, kPair + 4u);
    const std::uint32_t head_first = word(rt, head);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x10u), head_first);
    const std::uint32_t head_second = word(rt, head + 4u);
    const std::uint32_t pair_first = word(rt, kPair);
    put(rt, frame.at(4u), pair_second);
    put(rt, frame.at(0xCu), head_second);
    put(rt, frame.at(0u), pair_first);
    put(rt, frame.at(8u), 0u);
    (void)call_keep(rt, ctx, 0x08BAA488u, {frame.at(0x40u), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], frame.at(0u)});
    const std::uint32_t r0 = word(rt, frame.at(0x40u)), r1 = word(rt, frame.at(0x44u));
    put(rt, frame.at(0x18u), r0);
    const std::uint32_t r2 = word(rt, frame.at(0x48u));
    put(rt, frame.at(0x1Cu), r1);
    put(rt, frame.at(0x20u), r2);
    const std::uint32_t r3 = word(rt, frame.at(0x4Cu)), r4 = word(rt, frame.at(0x50u));
    put(rt, frame.at(0x24u), r3);
    put(rt, frame.at(0x28u), r4);
    take(ctx, call_keep(rt, ctx, 0x08BAA850u, {frame.at(0x60u), frame.at(0x10u), frame.at(8u), frame.at(0x18u)}, {}));
}

// func_08C4677C(this, q): n = sqrt(func_08C46364(q)) (a float); the frame holds 1 / n and func_08C46744(this, q, &frame) ends the function. v0 / v1 / f0 are
// that call's.
// Branches: none, the function is straight-line.
void func_08C4677C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const GuestResult length = call_keep(rt, ctx, 0x08C46364u, {other}, {});
    const float root = std::sqrt(length.f0);
    put_float(rt, frame.at(0u), 1.0f / root);
    take(ctx, call_keep(rt, ctx, 0x08C46744u, {self, other, frame.at(0u)}, {}));
}

// func_08C2176C(this, handle): the object behind func_08A6158C(handle) (the address of a pointer to an object; null: the original's assert func_08A5BB7C(message), which
// does not return) answers its virtual slot 0x30; a non-zero answer is the result, a zero one runs the tail call func_08A61DD4(handle, this[2]).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null object (the `bnez` falls into the assert call; the code goes on with a null
// object, the vtable is read through address 0).
void func_08C2176C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], handle = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t slot_address = call_keep(rt, ctx, 0x08A6158Cu, {handle});
        const std::uint32_t object = word(rt, slot_address);
        if (object == 0u) (void)call_keep(rt, ctx, kReportNull, {kNullMessage});
        const std::uint32_t function = word(rt, word(rt, object) + 0x30u);
        if (call_keep(rt, ctx, function, {object}) != 0u) return;
    }
    tail_call(ctx, 0x08A61DD4u, {handle, word(rt, self + 8u)});
}

// func_08C209F0(this, handle): s = func_08A6159C(handle, this[2] + 1) is the address of a pointer to an object; the frame holds {handle} at +0 and a holder at
// +4 (func_08A61BC0(&holder, &frame[0])); the object's slot 0xD0 runs as (object, &holder, this[2], s + 4) and the holder is released. v0 / v1 / f0 are the
// release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null object (the assert func_08A5BB7C, then the code goes
// on with a null object).
void func_08C209F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], handle = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t slot_address = call_keep(rt, ctx, 0x08A6159Cu, {handle, word(rt, self + 8u) + 1u});
    put(rt, frame.at(0u), handle);
    (void)call_keep(rt, ctx, 0x08A61BC0u, {frame.at(4u), frame.at(0u)});
    const std::uint32_t object = word(rt, slot_address);
    if (object == 0u) (void)call_keep(rt, ctx, kReportNull, {kNullMessage});
    const std::uint32_t function = word(rt, word(rt, object) + 0xD0u);
    (void)call_keep(rt, ctx, function, {object, frame.at(4u), word(rt, self + 8u), slot_address + 4u});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(4u)}, {}));
}

// func_08C21548(this, handle): the object behind func_08A6158C(handle) answers its slot 0x30; a non-zero answer is the result. Otherwise func_08A61DD4(handle,
// this[2]) runs, a temporary holder is filled (func_08A97A6C(&holder, 0)), handed to func_08A61584(handle, &holder) and released. v0 / v1 / f0 are the
// release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null object (the assert func_08A5BB7C, then the code goes
// on with a null object).
void func_08C21548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], handle = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t slot_address = call_keep(rt, ctx, 0x08A6158Cu, {handle});
    const std::uint32_t object = word(rt, slot_address);
    if (object == 0u) (void)call_keep(rt, ctx, kReportNull, {kNullMessage});
    const GuestResult answer = call_keep(rt, ctx, word(rt, word(rt, object) + 0x30u), {object}, {});
    if (answer.v0 != 0u) {
        take(ctx, answer);
        return;
    }
    (void)call_keep(rt, ctx, 0x08A61DD4u, {handle, word(rt, self + 8u)});
    (void)call_keep(rt, ctx, 0x08A97A6Cu, {frame.at(0u), 0u});
    (void)call_keep(rt, ctx, 0x08A61584u, {handle, frame.at(0u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)}, {}));
}

// func_08C21AC0(this, out): func_08A98A70(&holder, out) fills a holder on the frame; its object (null: the assert func_08A5BB7C with another message) is
// handed to func_08C219C4(object + 0x14, this[2]); then func_08A61584(out, &holder) and the release of the holder. v0 / v1 / f0 are the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null object (the assert, then the code goes on with a
// null object).
void func_08C21AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08A98A70u, {frame.at(0u), out});
    const std::uint32_t object = word(rt, frame.at(0u));
    if (object == 0u) (void)call_keep(rt, ctx, kReportNull, {0x08CB0294u});
    (void)call_keep(rt, ctx, 0x08C219C4u, {object + 0x14u, word(rt, self + 8u)});
    (void)call_keep(rt, ctx, 0x08A61584u, {out, frame.at(0u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)}, {}));
}

// func_08C21C08(this, handle): p = func_08A6158C(handle); then the tail call func_08A61E3C(handle, p). v0 / v1 / f0 are func_08A6158C's.
// Branches: none, the function is straight-line.
void func_08C21C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[5];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_keep(rt, ctx, 0x08A6158Cu, {handle}, {});
    }
    take(ctx, result);
    tail_call(ctx, 0x08A61E3Cu, {handle, result.v0});
}

inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// func_088B476C(this, other): s1 = this + 4, s0 = other + 4. Unless the byte (this[3]->[3]) says kind 4, func_088C4754(s1, s0) runs and then
// func_088B822C(s1[0x28], s1[0x2C], s0[0x2C]). Then, unless the same byte of s0 is 4 (v0 = 4, v1 = that pointer), func_088C4754(s0, s1) runs and the tail call
// func_088B822C(s0[0x28], s0[0x2C], s1[0x2C]) ends the function.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the first byte being 4 (the `beq` lands on the load of s0[2], skipping the first
// two calls; its delay slot already computed s0) and the second byte being 4 (the `beq` lands on the epilogue with v0 = 4 from the `lb` and v1 = s0[2]).
void func_088B476C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4] + 4u, second = ctx.gpr[5] + 4u;
    std::uint32_t tail_a0, tail_a1, tail_a2;
    {
        GuestFrame frame(ctx, 0x10u);
        if (byte_at(rt, word(rt, first + 8u) + 3u) != 4) {
            (void)call_keep(rt, ctx, 0x088C4754u, {first, second});
            const std::uint32_t a2 = word(rt, second + 0x2Cu);
            const std::uint32_t a0 = word(rt, first + 0x28u);
            const std::uint32_t a1 = word(rt, first + 0x2Cu);
            (void)call_keep(rt, ctx, 0x088B822Cu, {a0, a1, a2});
        }
        const std::uint32_t pointer = word(rt, second + 8u);
        if (byte_at(rt, pointer + 3u) == 4) {
            ctx.gpr[2] = 4u;
            ctx.gpr[3] = pointer;
            return;
        }
        (void)call_keep(rt, ctx, 0x088C4754u, {second, first});
        tail_a1 = word(rt, second + 0x2Cu);
        tail_a2 = word(rt, first + 0x2Cu);
        tail_a0 = word(rt, second + 0x28u);
    }
    tail_call(ctx, 0x088B822Cu, {tail_a0, tail_a1, tail_a2});
}

// func_088B46A8(this): s1 = this + 8, s2 = this + 4; func_088B5534(s2[0x2C], s1); unless the byte (s1[1]->[3]) says kind 4 (v0 = s1[1], v1 = 4): o =
// func_088DF13C(s1[2], s1[3]) is the address of a float; func_088B5670(s2[0x2C], s2[0x1C], f12 = *o), func_088C37C0(s2), and the tail call func_088B5D94(s2[0x28]).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the byte being 4 (the `beq` lands on the epilogue with v0 = s1[1], v1 = 4; its delay
// slot only restores ra).
void func_088B46A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[4] + 4u, first = ctx.gpr[4] + 8u;
    std::uint32_t tail_argument;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x088B5534u, {word(rt, second + 0x2Cu), first});
        const std::uint32_t pointer = word(rt, first + 4u);
        if (byte_at(rt, pointer + 3u) == 4) {
            ctx.gpr[2] = pointer;
            ctx.gpr[3] = 4u;
            return;
        }
        const std::uint32_t a1 = word(rt, first + 0xCu), a0 = word(rt, first + 8u);
        const std::uint32_t holder = word(rt, second + 0x2Cu);
        const std::uint32_t scale = call_keep(rt, ctx, 0x088DF13Cu, {a0, a1});
        (void)call_keep(rt, ctx, 0x088B5670u, {holder, word(rt, second + 0x1Cu)}, {float_at(rt, scale)});
        (void)call_keep(rt, ctx, 0x088C37C0u, {second});
        tail_argument = word(rt, second + 0x28u);
    }
    tail_call(ctx, 0x088B5D94u, {tail_argument});
}

// func_088C3088(this, other): unless the byte (other[2]->[3]) says kind 4 (v0 = other[2], v1 = 4): func_088C2E18(other); when other[9]->[2] > 0 also
// func_088C2E20(other); then func_088C22C8(other) when the byte other[9]->[0x94] is 0, else func_088C066C(other); the tail call func_088BCA94(other) ends it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the byte being 4 (the `beq` lands on the epilogue with v0 = other[2], v1 = 4; its
// delay slot only restores ra).
void func_088C3088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t pointer = word(rt, other + 8u);
        if (byte_at(rt, pointer + 3u) == 4) {
            ctx.gpr[2] = pointer;
            ctx.gpr[3] = 4u;
            return;
        }
        (void)call_keep(rt, ctx, 0x088C2E18u, {other});
        if (static_cast<std::int32_t>(word(rt, word(rt, other + 0x24u) + 8u)) > 0) (void)call_keep(rt, ctx, 0x088C2E20u, {other});
        if (rt.memory().load8(word(rt, other + 0x24u) + 0x94u) == 0u) {
            (void)call_keep(rt, ctx, 0x088C22C8u, {other});
        } else {
            (void)call_keep(rt, ctx, 0x088C066Cu, {other});
        }
    }
    tail_call(ctx, 0x088BCA94u, {other});
}

// func_08BD3210(this, destination, from): erase the elements [from, this[1]) of a vector of 8-byte elements by moving them down to `destination`
// (the elements at `from` and above go to `destination` and up, two words each, in order), then func_08BD319C(end, this[1], &frame) and this[1] -= (from -
// destination) & ~7. v0 = destination.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a positive count (the `blez` falls into the copy loop, whose rounds load both words
// before storing them and advance both pointers by 8); the compared cases all have an empty tail, so the call gets the destination as its first argument.
void func_08BD3210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5], from = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::int32_t count = static_cast<std::int32_t>(word(rt, self + 4u) - from) >> 3;
    std::uint32_t source = from, target = destination;
    if (count > 0) {
        for (std::int32_t i = 0; i < count; ++i) {
            const std::uint32_t low = word(rt, source), high = word(rt, source + 4u);
            put(rt, target, low);
            put(rt, target + 4u, high);
            source += 8u;
            target += 8u;
        }
    }
    (void)call_keep(rt, ctx, 0x08BD319Cu, {target, word(rt, self + 4u), frame.at(0u)});
    const std::uint32_t end = word(rt, self + 4u) - ((from - destination) & ~7u);
    put(rt, self + 4u, end);
    ctx.gpr[2] = destination;
    ctx.gpr[3] = end;
}

// func_08C3E9B0(first, last): the final insertion sort of a range of 8-byte elements. Fewer than 17 elements: the tail call func_08C3E8DC(first, last) (v0 = 1).
// Otherwise func_08C3E8DC(first, first + 0x80) sorts the first sixteen, and every later element is copied to the frame and inserted with
// func_08C3E7E0(&element, &frame). v0 / v1 / f0 are the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): seventeen elements or more (the `bnez` falls through: the first sixteen are sorted
// with func_08C3E8DC(first, first + 0x80), then each later element is stored on the frame and inserted with func_08C3E7E0(&element, &frame); the loop's `bne` compares the advanced
// cursor, a0 is the old one). The routes only call it with short ranges, which take the tail call.
void func_08C3E9B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (static_cast<std::int32_t>(last - first) >> 3 < 0x11) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08C3E8DCu, {first, last});
        return;
    }
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t cursor = first + 0x80u;
    GuestResult result = call_keep(rt, ctx, 0x08C3E8DCu, {first, cursor}, {});
    if (last != cursor) {
        do {
            const std::uint32_t high = word(rt, cursor + 4u), low = word(rt, cursor);
            put(rt, frame.at(0u), low);
            put(rt, frame.at(4u), high);
            const std::uint32_t element = cursor;
            cursor += 8u;
            result = call_keep(rt, ctx, 0x08C3E7E0u, {element, frame.at(0u)}, {});
        } while (last != cursor);
    }
    take(ctx, result);
}

// func_0893665C(this, a1, a2, a3): i = func_0893656C(this, ...); k = func_08936578(this, a1, a2, a3); h = this[i + 0x4F]; no handler (0) answers -1,
// otherwise the tail call func_08938770(h, k, a2, a3).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no handler (the `beqz` lands on the epilogue; v0 = -1 comes from the instruction before the
// branch, v1 = 0).
void func_0893665C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    std::uint32_t handler, key;
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t index = call_keep(rt, ctx, 0x0893656Cu, {self, ctx.gpr[5], a2, a3});
        key = call_keep(rt, ctx, 0x08936578u, {self, ctx.gpr[5], a2, a3});
        handler = word(rt, self + (index << 2) + 0x13Cu);
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = handler;
        if (handler == 0u) return;
    }
    tail_call(ctx, 0x08938770u, {handler, key, a2, a3});
}

// func_0886BA50(this, other): this[1]->[0x68] = 1 (a byte); s = other's slot 0x24 (called on other); p = this[1]; when p[0xB] (0x2C) and s are both set:
// o = func_0887C900(p + 0x3C) is the address of three floats, func_08AF12EC(s[1], o[0], o[1], o[2]) (floats in f12-f14), then the tail call
// func_08AF12D0(s[1], 1, 1, 1). Otherwise v0 = p[0xB] (the loaded word) and nothing else runs.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero flag word (the `beqz` lands on the epilogue with v0 = 0) or a zero s (the
// `beqz` lands there with v0 = the flag word); the compared cases take neither.
void func_0886BA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t owner = word(rt, self + 4u);
        rt.memory().store8(owner + 0x68u, 1u);
        const std::uint32_t slot = word(rt, word(rt, other) + 0x24u);
        const std::uint32_t s = call_keep(rt, ctx, slot, {other});
        const std::uint32_t p = word(rt, self + 4u);
        const std::uint32_t flag = word(rt, p + 0x2Cu);
        if (flag == 0u || s == 0u) {
            ctx.gpr[2] = flag;
            return;
        }
        const std::uint32_t vector = call_keep(rt, ctx, 0x0887C900u, {p + 0x3Cu});
        const float z = float_at(rt, vector + 8u);
        const std::uint32_t handle = word(rt, s + 4u);
        const float x = float_at(rt, vector);
        const float y = float_at(rt, vector + 4u);
        (void)call_keep(rt, ctx, 0x08AF12ECu, {handle}, {x, y, z});
        ctx.fpr[12] = x;
        ctx.fpr[13] = y;
        ctx.fpr[14] = z;
        target = word(rt, s + 4u);
    }
    tail_call(ctx, 0x08AF12D0u, {target, 1u, 1u, 1u});
}

// func_08B00220(this): the words 0, 1, 2, 3 are copied to 4, 5, 6, 7 and the words 8, 9, 10 to 11, 12, 13; this[0xE2] (a halfword) |= 0x2000;
// func_08AFED50(this, this, this + 0x20, w1, t0 = w2, t1 = w3, t2 = w8, t3 = w9) runs, then func_08AFEA88(this) and the tail call func_08AFFCF4(this).
// Branches: none, the function is straight-line.
void func_08B00220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flags = rt.memory().load16(self + 0xE2u);
    const std::uint32_t w0 = word(rt, self), w1 = word(rt, self + 4u), w2 = word(rt, self + 8u), w3 = word(rt, self + 0xCu);
    const std::uint32_t w8 = word(rt, self + 0x20u), w9 = word(rt, self + 0x24u), w10 = word(rt, self + 0x28u);
    put(rt, self + 0x10u, w0);
    put(rt, self + 0x14u, w1);
    put(rt, self + 0x18u, w2);
    put(rt, self + 0x1Cu, w3);
    put(rt, self + 0x2Cu, w8);
    put(rt, self + 0x30u, w9);
    put(rt, self + 0x34u, w10);
    rt.memory().store16(self + 0xE2u, static_cast<std::uint16_t>(flags | 0x2000u));
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08AFED50u, {self, self, self + 0x20u, w1, w2, w3, w8, w9});
        take(ctx, call_keep(rt, ctx, 0x08AFEA88u, {self}, {}));
    }
    tail_call(ctx, 0x08AFFCF4u, {self});
}

// func_08C2B76C(first, last, out): copy-constructs the 12-byte elements [first, last) to `out` (a null out skips the construction but still advances): for
// each, func_08A6972C(out, element), out[1] = element[1], func_08A76ABC(out + 8, element + 8). v0 = the end of the output.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-empty range (the `beq` falls through into the loop; each round bumps s1 and
// stores the second word in the delay slot of the second call; a null `out` skips both calls through the `beqz`, whose delay slot already bumped s1; the `bne` jumps back after the
// delay slot that advances `out`). The compared cases all have an empty range, so v0 = `out`.
void func_08C2B76C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t element = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (element != last) {
        std::uint32_t next = element;
        do {
            next += 0xCu;
            if (out != 0u) {
                (void)call_keep(rt, ctx, 0x08A6972Cu, {out, element});
                put(rt, out + 4u, word(rt, element + 4u));
                (void)call_keep(rt, ctx, 0x08A76ABCu, {out + 8u, element + 8u});
            }
            element = next;
            out += 0xCu;
        } while (last != next);
    }
    ctx.gpr[2] = out;
}

// func_08B4E9E0(dst, src): a null src does nothing; otherwise the sixteen words of src are copied to dst (all loaded first, stored in the order of the
// original: word 0, 15, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11).
// Branches: none that matter, both outcomes of the null test are taken or are a plain return.
void func_08B4E9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t destination = ctx.gpr[4], source = ctx.gpr[5];
    if (source == 0u) return;
    std::uint32_t w[16];
    for (std::uint32_t i : {0u, 4u, 8u, 12u, 1u, 5u, 9u, 13u, 2u, 6u, 10u, 14u, 3u, 7u, 11u, 15u}) w[i] = word(rt, source + i * 4u);
    for (std::uint32_t i : {0u, 15u, 4u, 8u, 12u, 1u, 5u, 9u, 13u, 2u, 6u, 10u, 14u, 3u, 7u, 11u}) put(rt, destination + i * 4u, w[i]);
}

// func_08C1FE90(this, handle): s = func_08A6158C(handle); o = *func_08A6158C(handle) (null: the assert func_08A5BB7C); the frame gets a holder from o's slot
// 0x70 (frame, o, s), which is handed to func_08A61584(handle, &holder) and released. v0 / v1 / f0 are the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null object (the assert, then the code goes on with a
// null object).
void func_08C1FE90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = call_keep(rt, ctx, 0x08A6158Cu, {handle});
    const std::uint32_t second = call_keep(rt, ctx, 0x08A6158Cu, {handle});
    const std::uint32_t object = word(rt, second);
    if (object == 0u) (void)call_keep(rt, ctx, kReportNull, {kNullMessage});
    const std::uint32_t function = word(rt, word(rt, object) + 0x70u);
    (void)call_keep(rt, ctx, function, {frame.at(0u), object, first});
    (void)call_keep(rt, ctx, 0x08A61584u, {handle, frame.at(0u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)}, {}));
}

// func_08B87C10(this): this[0] = vtable; the string at this[0x11] is released (a representation other than the shared empty one 0x08D74FD0 loses a reference,
// func_08B60804(string - 4, -1), and is freed with func_08B8542C(rep, &frame) when the count is not positive); func_08B87B60(this + 0x2C, ...[2]),
// func_08B87AB0(this + 0x14, ...[2]) destroy two members, func_08A780B0(this) and the free (this, 0x48, 4). v0 / v1 / f0 are the free's.
// Branches: both outcomes of every test occur in the compared cases.
void func_08B87C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t string = word(rt, self + 0x44u);
    const std::uint32_t rep = string - 0xCu;
    put(rt, self, 0x08CB88A8u);
    if (rep != 0x08D74FD0u) {
        const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu});
        if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
    }
    (void)call_keep(rt, ctx, 0x08B87B60u, {self + 0x2Cu, word(rt, self + 0x2Cu + 8u)});
    (void)call_keep(rt, ctx, 0x08B87AB0u, {self + 0x14u, word(rt, self + 0x14u + 8u)});
    (void)call_keep(rt, ctx, 0x08A780B0u, {self});
    take(ctx, call_keep(rt, ctx, kFree, {self, 0x48u, 4u}, {}));
}

// func_08BB1D0C(begin, end, key, compare): the upper bound of *key in the sorted words [begin, end): while the length is positive, half = length / 2 and
// compare(begin[half], *key) decides (non-zero: begin = &begin[half] + 4 and the length drops by half + 1, else the length is half). v0 = begin.
// Branches: both outcomes of every test occur in the compared cases.
void func_08BB1D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t begin = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[6], compare = ctx.gpr[7];
    std::int32_t length = static_cast<std::int32_t>(ctx.gpr[5] - begin) >> 2;
    GuestFrame frame(ctx, 0x20u);
    while (length > 0) {
        const std::int32_t half = length >> 1;
        const std::uint32_t middle = begin + (static_cast<std::uint32_t>(half) << 2);
        const std::uint32_t a1 = word(rt, key);
        const std::uint32_t answer = call_keep(rt, ctx, compare, {word(rt, middle), a1});
        if (answer != 0u) {
            begin = middle + 4u;
            length = length - half - 1;
        } else {
            length = half;
        }
    }
    ctx.gpr[2] = begin;
}

// func_08BDB970(first, last): a range of 8-byte elements shorter than 17 elements is the tail call func_08BDB8A0(first, last) (v0 = 1); otherwise
// func_08BDB8A0(first, first + 0x80) sorts the first sixteen and every later element is inserted with func_08BDB52C(&element, element[0], element[1]).
// v0 / v1 / f0 are the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): seventeen elements or more (the `bnez` falls through; the loop's `bnel` reloads a2
// in its likely delay slot, a3 at the top). The routes only call it with short ranges, which take the tail call.
void func_08BDB970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (static_cast<std::int32_t>(last - first) < 0x88) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08BDB8A0u, {first, last});
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t cursor = first + 0x80u;
    GuestResult result = call_keep(rt, ctx, 0x08BDB8A0u, {first, cursor}, {});
    if (last != cursor) {
        do {
            const std::uint32_t element = cursor;
            const std::uint32_t low = word(rt, element), high = word(rt, element + 4u);
            cursor += 8u;
            result = call_keep(rt, ctx, 0x08BDB52Cu, {element, ctx.gpr[5], low, high}, {});
        } while (last != cursor);
    }
    take(ctx, result);
}

// func_08AA195C(this, byte): the byte is stored on the frame and this's virtual slot 0x10C runs as (this, &byte, 1). v0 / v1 / f0 are the call's.
// Branches: none, the function is straight-line.
void func_08AA195C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store8(frame.at(0u), static_cast<std::uint8_t>(ctx.gpr[5]));
    take(ctx, call_keep(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, frame.at(0u), 1u}, {}));
}

// func_08920018(this): a table indexed by this[0x7F] (0x1FC) answers 0..3; indexes of 0x22 and above answer 3. The jump table at 0x08C8A6D0 (read from
// the ELF) selects among four constant returns.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an index of 0x22 or more (the `beqz` lands on `jr ra` with v0 = 3 from its delay slot).
void func_08920018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint8_t kAnswers[34] = {0, 2, 3, 3, 3, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 2};
    const std::uint32_t index = word(rt, ctx.gpr[4] + 0x1FCu);
    ctx.gpr[3] = index;
    ctx.gpr[2] = index < 0x22u ? kAnswers[index] : 3u;
}

// func_08B54730(table, index, a2, a3): the two bytes at table + index * 8 + 0x4E / 0x4F get 32 - clz(a2 - 1) and 32 - clz(a3 - 1) (the bit lengths of the
// last values). v0 = the first byte.
// Branches: none, the function is straight-line.
void func_08B54730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = ctx.gpr[4] + (ctx.gpr[5] << 3);
    const std::uint32_t low = 32u - static_cast<std::uint32_t>(std::countl_zero(ctx.gpr[6] - 1u));
    const std::uint32_t high = 32u - static_cast<std::uint32_t>(std::countl_zero(ctx.gpr[7] - 1u));
    rt.memory().store8(entry + 0x4Fu, static_cast<std::uint8_t>(high));
    rt.memory().store8(entry + 0x4Eu, static_cast<std::uint8_t>(low));
    ctx.gpr[2] = low;
}

// func_08B56434(): func_08B563E0(0x08D6D0E0) and then the graphics library's flag word (+0x6C of its state) gets bit 1 set. v0 = the new flags, v1 = the state's
// address.
// Branches: none, the function is straight-line.
void func_08B56434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B563E0u, {kGuCommandBlock});
    }
    const std::uint32_t flags = word(rt, kGuFlags) | 2u;
    ctx.gpr[3] = kGuState;
    ctx.gpr[2] = flags;
    put(rt, kGuFlags, flags);
}

// func_08C460E4(out, q): the 3x3 rotation matrix of the quaternion q = {x, y, z, w} (a1[0..3]), rows written to out in the order of the original. Plain float
// adds, subtracts and multiplies in the order of the original.
// Branches: none, the function is straight-line.
void func_08C460E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], q = ctx.gpr[5];
    const float z = float_at(rt, q + 8u), y = float_at(rt, q + 4u), x = float_at(rt, q), w = float_at(rt, q + 0xCu);
    float f7 = z + z, f6 = y + y, f1 = x + x;
    const float f12 = psp_mul(z, f7), f10 = psp_mul(y, f6), f5 = psp_mul(x, f1);
    const float f11 = psp_mul(w, f1);
    f7 = psp_mul(w, f7);
    float f2 = psp_mul(y, f1);
    const float f4 = psp_mul(w, f6);
    f1 = psp_mul(z, f1);
    float f8 = 1.0f - f5;
    float f3 = psp_mul(z, f6);
    float f9 = 1.0f - f12;
    float f0 = 1.0f - f10;
    f6 = f2 + f7;
    const float f13 = f1 - f4;
    const float f14 = f11 + f3;
    f8 = f8 - f10;
    f0 = f0 - f12;
    f9 = f9 - f5;
    f2 = f2 - f7;
    f1 = f1 + f4;
    f3 = f3 - f11;
    put_float(rt, out + 0x20u, f8);
    put_float(rt, out, f0);
    put_float(rt, out + 0xCu, f2);
    put_float(rt, out + 0x18u, f1);
    put_float(rt, out + 4u, f6);
    put_float(rt, out + 0x10u, f9);
    put_float(rt, out + 0x1Cu, f3);
    put_float(rt, out + 8u, f13);
    put_float(rt, out + 0x14u, f14);
}

// func_08955140(this, index, t): a = (the float at (func_089551B0(this) + index * 4 + 0x98)), b = (the float at (func_08955300(this, index) + 0xAC)); the result
// is (b + (a - b) * t)^2, in f0.
// Branches: none, the function is straight-line.
void func_08955140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    const float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t table = call_keep(rt, ctx, 0x089551B0u, {self});
    const float a = float_at(rt, table + (index << 2) + 0x98u);
    const std::uint32_t entry = call_keep(rt, ctx, 0x08955300u, {self, index});
    const float b = float_at(rt, entry + 0xACu);
    const float blend = b + psp_mul(a - b, t);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = psp_mul(blend, blend);
}

// func_08955A3C(this, index, t): u = func_089559C4(this) + t; a = the float at (func_089551B0(this) + index * 4 + 0xF8); e = func_08955300(this, index);
// b = the float at (e + 0xF8); the result is ((u * a + b * (1 - u)) * 0.5) + 0.5, in f0.
// Branches: none, the function is straight-line.
void func_08955A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    const float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const float u = call_keep(rt, ctx, 0x089559C4u, {self}, {}).f0 + t;
    const std::uint32_t table = call_keep(rt, ctx, 0x089551B0u, {self});
    const float a = float_at(rt, table + (index << 2) + 0xF8u);
    const std::uint32_t entry = call_keep(rt, ctx, 0x08955300u, {self, index});
    const float rest = 1.0f - u;
    const float b = float_at(rt, entry + 0xF8u);
    const float blend = psp_mul(u, a) + psp_mul(b, rest);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = psp_mul(blend, 0.5f) + 0.5f;
}

// func_089D9DA4(this, flag): when the byte this[0x8C] is already flag (low 8 bits), v0 = that byte. Otherwise a = func_089D83F4(this), b = func_089D8410(this)
// (floats), this[0x8C] = flag and the tail call func_089D9B88(this) with f12 = b + ((this[0x27] - (a + b)) - (this[0x24] - a)).
// Branches: both outcomes of every test occur in the compared cases.
void func_089D9DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    float argument;
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t current = rt.memory().load8(self + 0x8Cu);
        if (current == flag) {
            ctx.gpr[2] = current;
            return;
        }
        const float a = call_keep(rt, ctx, 0x089D83F4u, {self}, {}).f0;
        const float b = call_keep(rt, ctx, 0x089D8410u, {self}, {}).f0;
        float tail = float_at(rt, self + 0x9Cu);
        const float sum = a + b;
        float other = float_at(rt, self + 0x90u);
        rt.memory().store8(self + 0x8Cu, static_cast<std::uint8_t>(flag));
        other = other - a;
        tail = tail - sum;
        tail = tail - other;
        argument = b + tail;
    }
    ctx.fpr[12] = argument;
    tail_call(ctx, 0x089D9B88u, {self});
}

// String assignment (52 instructions; basic_string::operator=(const basic_string &) of the 1-byte-character string with its reference-counted
// representation at data - 0xC: {length, capacity, refcount}): this = a0 (one word, the data pointer), a1 = the source (the address of its data pointer).
// The same representation: nothing happens (v0 = this). Otherwise the source's share is taken: when its refcount (rep + 8, signed) is not negative the data
// is shared (the count is raised with func_08B6085C(data - 4, 1) unless it is the empty representation `empty`), else `clone`(rep, &frame, 0) makes a copy;
// the old representation of this loses a reference (func_08B60804(data - 4, -1)) unless it is the empty one and is freed with `release`(rep, &frame) when
// the count answers not-positive; this[0] = the new data. v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the source being the shared empty representation (the `beql` is taken: its likely delay
// slot sets s3 = the source data and the code goes straight to the release of the old one, with v1 still the old data pointer).
void string_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t empty, std::uint32_t clone, std::uint32_t release) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t current = word(rt, self);
    const std::uint32_t source = word(rt, ctx.gpr[5]);
    const std::uint32_t source_rep = source - 0xCu;
    if (current - 0xCu == source_rep) {
        ctx.gpr[2] = self;
        return;
    }
    std::uint32_t next;
    if (static_cast<std::int32_t>(word(rt, source_rep + 8u)) < 0) {
        next = call_keep(rt, ctx, clone, {source_rep, frame.at(0u), 0u});
        current = word(rt, self);
    } else {
        next = source;
        if (source_rep != empty) {
            (void)call_keep(rt, ctx, 0x08B6085Cu, {source - 4u, 1u});
            current = word(rt, self);
        }
    }
    const std::uint32_t old_rep = current - 0xCu;
    if (old_rep != empty) {
        const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {current - 4u, 0xFFFFFFFFu});
        if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, release, {old_rep, frame.at(0u)});
    }
    put(rt, self, next);
    ctx.gpr[2] = self;
}

// Vector erase of reference-counted pointers (50 instructions; vector<holder>::erase(first, last)): a0 = the vector {begin, end, ...}, a1 = the destination
// (first), a2 = the first element kept. The elements from a2 to the end are assigned one by one to the destination and up (func_08A5BAD8(dst, *src), skipped when
// the two addresses are the same), then `destroy`(dst_end, end, &frame) destroys what is left, and end -= (a2 - a1) & ~3. v0 = the destination.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a positive count (the `blez` falls into the loop; the `beq` skips the assignment when
// source and destination are the same, its delay slot sets a0; the `bne` repeats until the counter reaches the count). The compared cases have nothing to move.
void vector_erase_pointers(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5], from = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::int32_t count = static_cast<std::int32_t>(word(rt, self + 4u) - from) >> 2;
    std::uint32_t source = from, target = destination;
    for (std::int32_t i = 0; i < count; ++i) {
        if (source != target) (void)call_keep(rt, ctx, 0x08A5BAD8u, {target, word(rt, source)});
        source += 4u;
        target += 4u;
    }
    (void)call_keep(rt, ctx, destroy, {target, word(rt, self + 4u), frame.at(0u)});
    const std::uint32_t end = word(rt, self + 4u) - ((from - destination) & ~3u);
    put(rt, self + 4u, end);
    ctx.gpr[2] = destination;
    ctx.gpr[3] = end;
}

// New holder from a temporary (43 instructions; batch 78's "new holder" with a temporary built first): t = a temporary holder on the frame (func_08A69B3C(&t)),
// o = new 24-byte block (func_08A5BC20(24, 4)), `ctor`(o, &t) constructs it, o[0] = vtable, o[5] = the extra argument (a2 in func_08B86FB0's shape, a3 in func_08B8705C's), the temporary is disposed (func_08A69D68(&t)); a holder slot
// frame[0] = o; func_08A5D414(&frame[4], &frame[0]) fills the holder frame[4], func_08A7C5C8(a0, &frame[4]) hands it over and func_08A5B9EC(&frame[4]) releases it.
// The result is the release's.
// Branches: none, the function is straight-line.
void new_holder_temp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t ctor, std::uint32_t vtable, std::uint32_t extra) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    const std::uint32_t object = call_keep(rt, ctx, kAllocate, {24u, 4u});
    (void)call_keep(rt, ctx, ctor, {object, frame.at(8u)});
    put(rt, object, vtable);
    put(rt, object + 0x14u, extra);
    (void)call_keep(rt, ctx, 0x08A69D68u, {frame.at(8u)});
    put(rt, frame.at(0u), object);
    (void)call_keep(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    (void)call_keep(rt, ctx, 0x08A7C5C8u, {target, frame.at(4u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(4u)}, {}));
}

void new_holder_temp_a2(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t ctor, std::uint32_t vtable) {
    new_holder_temp(rt, ctx, ctor, vtable, ctx.gpr[6]);
}
void new_holder_temp_a3(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t ctor, std::uint32_t vtable) {
    new_holder_temp(rt, ctx, ctor, vtable, ctx.gpr[7]);
}

// Erase a string-keyed subtree with a holder (44 instructions, recursive through its own address; batch 78's "erase_str" plus a holder in each node): a0 = the owner,
// a1 = a node (null: nothing, v0 untouched). For each node, from the first: erase(owner, node[3]); the holder at node + holder_offset is released
// (func_08A5B9EC), the left child node[2] is read before that call; unless the string at node[4] is the shared empty one (0x08D74FD0 + 0xC),
// func_08B60804(string - 4, -1) drops its count and, when the answer is not positive, func_08B8542C(string - 0xC, &frame) frees it; then the node is freed with
// func_08A5BC54(node, node_size, 4) and the walk goes on with the left child. v0 = the last free's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty string (the `beq` lands on the free of the node, a0 = string - 4 from
// its delay slot) and a count above zero after the decrement (the `bgtz` lands on the free of the node).
void erase_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_address, std::uint32_t holder_offset, std::uint32_t node_size) {
    const std::uint32_t owner = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    do {
        (void)call_keep(rt, ctx, self_address, {owner, word(rt, node + 0xCu)});
        const std::uint32_t left = word(rt, node + 8u);
        (void)call_keep(rt, ctx, kReleaseLocal, {node + holder_offset});
        const std::uint32_t string = word(rt, node + 0x10u);
        const std::uint32_t rep = string - 0xCu;
        if (rep != 0x08D74FD0u) {
            const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu});
            if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
        }
        ctx.gpr[2] = call_keep(rt, ctx, kFree, {node, node_size, 4u});
        node = left;
    } while (node != 0u);
}

// Forward to a member's virtual slot (11 instructions): p = this[member_offset / 4]; no p leaves v0 = 0; otherwise the tail call to the slot at (*(p + vtable_offset)
// + slot) on p, with v0 / v1 as the loads leave them (the slot's address / the vtable).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null member (the `beqz` lands on `jr ra` with v0 = 0, v1 = 0; its delay slot only sets a0).
void member_virtual_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t member_offset, std::uint32_t vtable_offset, std::uint32_t slot) {
    const std::uint32_t member = word(rt, ctx.gpr[4] + member_offset);
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = member;
    if (member == 0u) return;
    const std::uint32_t vtable = word(rt, member + vtable_offset);
    const std::uint32_t target = word(rt, vtable + slot);
    ctx.gpr[2] = target;
    ctx.gpr[3] = vtable;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {member});
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Registered wrappers of the families
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_XXXXXXXX (pair_call_e): see pair_call_e (pair, callee).
void func_08B87108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_e(rt, ctx, 0x08C87D04u, 0x08B8705Cu); }

// func_XXXXXXXX (const_tail): see const_tail (constant, target).
void func_08BF8B58(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { const_tail(ctx, 0x08C95EF4u, 0x08B64604u); }

// func_XXXXXXXX (fields_tail): see fields_tail (offset of a1, target, offset of a0).
void func_08BAED0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_tail(rt, ctx, 0x0000000Cu, 0x0896264Cu, 0x00000010u); }

// func_XXXXXXXX (float_setter3): see float_setter3.
void func_08870448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter3(rt, ctx); }

// func_XXXXXXXX (float_setter1): see float_setter1 (offset).
void func_08881A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_setter1(rt, ctx, 0x000016E8u); }

// func_XXXXXXXX (call_then_tail_const): see call_then_tail_const (callee, constant, target).
void func_08B3FA98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_then_tail_const(rt, ctx, 0x08B3F838u, 0x00000002u, 0x08B3F7A8u); }

// func_XXXXXXXX (vtable_call_tail_offset): see vtable_call_tail_offset (vtable, callee, offset, target).
void func_08B2C3BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vtable_call_tail_offset(rt, ctx, 0x08CDA710u, 0x08B2C2E0u, 0x00000058u, 0x08B2C2D8u); }

// func_XXXXXXXX (call_vtable_tail): see call_vtable_tail (callee, vtable, target).
void func_08884AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_vtable_tail(rt, ctx, 0x0887E1E0u, 0x08CC16C0u, 0x08884720u); }

// func_XXXXXXXX (vtable_dtor_free): see vtable_dtor_free (vtable, callee, size).
void func_08C2D80C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vtable_dtor_free(rt, ctx, 0x08CD4580u, 0x08A8036Cu, 0x00000018u); }

// func_XXXXXXXX (string_assign): see string_assign (empty representation, clone, release).
void func_08B855E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_assign(rt, ctx, 0x08D74FD0u, 0x08B854DCu, 0x08B8542Cu); }
void func_08B957D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_assign(rt, ctx, 0x08D75298u, 0x08B94288u, 0x08B93554u); }

// func_XXXXXXXX (vector_erase_pointers): see vector_erase_pointers (destroy).
void func_08B85798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_erase_pointers(rt, ctx, 0x08B85754u); }
void func_08C028F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_erase_pointers(rt, ctx, 0x08C026B8u); }

// func_XXXXXXXX (new_holder_temp_a2): see new_holder_temp (constructor, vtable); the extra argument is a2.
void func_08B86FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder_temp_a2(rt, ctx, 0x08A86BDCu, 0x08CD4658u); }
void func_08C307DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder_temp_a2(rt, ctx, 0x08A725A0u, 0x08CD4FD0u); }

// func_08B8705C (new_holder_temp_a3): the same body with the extra argument taken from a3, a shape of its own (the word that moves the argument differs).
void func_08B8705C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder_temp_a3(rt, ctx, 0x08A80214u, 0x08CD45C8u); }

// func_XXXXXXXX (erase_release): see erase_release (its own address, holder offset, node size).
void func_08BF7980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_release(rt, ctx, 0x08BF7980u, 20u, 24u); }
void func_08C012FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_release(rt, ctx, 0x08C012FCu, 24u, 28u); }

// func_XXXXXXXX (member_virtual_tail): see member_virtual_tail (member offset, vtable offset, slot).
void func_08C1A0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_virtual_tail(rt, ctx, 0x00000014u, 0x00000000u, 0x00000008u); }
void func_08C1A1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_virtual_tail(rt, ctx, 0x00000014u, 0x00000000u, 0x00000008u); }

}  // namespace
}  // namespace hot_batch_80
}  // namespace gt::game

#define HOT80(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_80::name, gt::ReturnKind::kind)
// Group 1
HOT80(0x08B87108u, func_08B87108, Word);
HOT80(0x08BF8B58u, func_08BF8B58, Word);
HOT80(0x08BAED0Cu, func_08BAED0C, Word);
// Not registered (every compared case stored the value already there (no case changed memory), so the gate cannot see its stores): HOT80(0x08870448u, func_08870448, Word);
HOT80(0x08881A5Cu, func_08881A5C, Word);
HOT80(0x08A101D8u, func_08A101D8, Word);
// Not registered (the only compared case saw a null field (nothing stored), so the gate cannot see its store or the tail call): HOT80(0x08BD2D24u, func_08BD2D24, Word);
HOT80(0x08B3FA98u, func_08B3FA98, Word);
HOT80(0x08B2C3BCu, func_08B2C3BC, Word);
HOT80(0x08884AC8u, func_08884AC8, Word);
HOT80(0x08C20F54u, func_08C20F54, Word);
HOT80(0x08C2D80Cu, func_08C2D80C, Word);
HOT80(0x08C024C0u, func_08C024C0, Word);
HOT80(0x08B87724u, func_08B87724, Word);
HOT80(0x08A37210u, func_08A37210, Word);
HOT80(0x08A1C55Cu, func_08A1C55C, Word);
HOT80(0x08B9F960u, func_08B9F960, Word);
HOT80(0x08BAEC80u, func_08BAEC80, Word);
HOT80(0x08B855E0u, func_08B855E0, Word);
HOT80(0x08B957D8u, func_08B957D8, Word);
HOT80(0x08B85798u, func_08B85798, Word);
HOT80(0x08C028F0u, func_08C028F0, Word);
HOT80(0x08B86FB0u, func_08B86FB0, Word);
HOT80(0x08B8705Cu, func_08B8705C, Word);
HOT80(0x08C307DCu, func_08C307DC, Word);
// Not registered (its compared cases changed no memory (the family's body is proven through func_08C012FC, but this member's own constants were not seen by a case that changed memory)): HOT80(0x08BF7980u, func_08BF7980, Word);
HOT80(0x08C012FCu, func_08C012FC, Word);
HOT80(0x08C1A0C4u, func_08C1A0C4, Word);
HOT80(0x08C1A1B8u, func_08C1A1B8, Word);
// Group 2
HOT80(0x089FE57Cu, func_089FE57C, Word);
HOT80(0x089294F8u, func_089294F8, Word);
HOT80(0x0887CA00u, func_0887CA00, Word);
HOT80(0x08C467DCu, func_08C467DC, Word);
HOT80(0x088CDCFCu, func_088CDCFC, Word);
HOT80(0x08A1AAB8u, func_08A1AAB8, Word);
HOT80(0x0892BCC0u, func_0892BCC0, Word);
HOT80(0x0888A058u, func_0888A058, Word);
HOT80(0x08C4677Cu, func_08C4677C, Word);
HOT80(0x08C2176Cu, func_08C2176C, Word);
HOT80(0x08C209F0u, func_08C209F0, Word);
HOT80(0x08C21548u, func_08C21548, Word);
HOT80(0x08C21AC0u, func_08C21AC0, Word);
HOT80(0x08C21C08u, func_08C21C08, Word);
HOT80(0x088B476Cu, func_088B476C, Word);
HOT80(0x088B46A8u, func_088B46A8, Word);
HOT80(0x088C3088u, func_088C3088, Word);
HOT80(0x08BD3210u, func_08BD3210, Word);
HOT80(0x08C3E9B0u, func_08C3E9B0, Word);
HOT80(0x0893665Cu, func_0893665C, Word);
HOT80(0x0886BA50u, func_0886BA50, Word);
HOT80(0x08B00220u, func_08B00220, Word);
// Not registered (the compared cases all had an empty range, so the gate cannot see its stores): HOT80(0x08C2B76Cu, func_08C2B76C, Word);
HOT80(0x08B4E9E0u, func_08B4E9E0, Word);
HOT80(0x08C1FE90u, func_08C1FE90, Word);
HOT80(0x08B87C10u, func_08B87C10, Word);
HOT80(0x08BB1D0Cu, func_08BB1D0C, Word);
HOT80(0x08BDB970u, func_08BDB970, Word);
HOT80(0x08AA195Cu, func_08AA195C, Word);
HOT80(0x08920018u, func_08920018, Word);
HOT80(0x08B54730u, func_08B54730, Word);
HOT80(0x08B56434u, func_08B56434, Word);
HOT80(0x08C460E4u, func_08C460E4, Word);
HOT80(0x08955140u, func_08955140, Float);
HOT80(0x08955A3Cu, func_08955A3C, Float);
HOT80(0x089D9DA4u, func_089D9DA4, Word);
