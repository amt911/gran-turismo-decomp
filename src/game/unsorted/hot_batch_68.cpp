// Hot batch 68: finishes the shape families of batches 59, 63, 65 and 66 (call probe over every function of up to 70 instructions not yet
// decompiled, run on the five routes with --decomp off, as in batches 57-67; the reached ones grouped by instruction shape with
// `tools.shapes`). The instruction shape hides the immediates and the call targets, so every member was compared with its family's
// template word by word: the words that differ are only the relocated constants (lui / addiu / lb / lw of a static address, the
// `li` of a constant) and the jal / j target, never a stack offset, a field offset or a register. The constants and call targets were
// then read from each member's own relocated words (the generated code only gave the relocation; it was not copied) and the jal / j
// targets checked against the raw word (target * 4 + the load base 0x08804000). As in batches 60-67, each function's comment lists the
// branches the compared cases of the fast gate do not take; those were compared against the asm by hand, reading the branch TARGET (a
// value set in a delay slot is the result only if the target does not overwrite it). Their classes aren't known yet, so they keep
// their addresses as names.
//
// Families (the implementation is the one of the earlier batch, with this member's constants):
//  - "pair call A" (18 instructions, batch 66): copies the pair at a static address and the pair a2 points to to the stack and calls a worker;
//  - "pair call B" (23 instructions, batch 66): the same with a3's pair as well;
//  - "node initialiser" (7 instructions, batch 59): stores three words into this and tail calls a per-type routine;
//  - "clamp step" (batch 63): the two-bound step, identical words apart from the relocated internal jumps;
//  - "call with a constant" (11 instructions, batch 66): worker(this, a1, constant), v0 = this; the worker differs per group of members;
//  - "empty container accessor" (32 instructions, batch 63): a function-local static container built under the C++ guard.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <unordered_map>

namespace gt::game {
namespace hot_batch_68 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu; // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u; // (guard)
constexpr std::uint32_t kAtExit = 0x08B77B1Cu;       // (function): registers a destructor for exit

// Pair call A: copies the pair at `pairs` (static) to frame+0 and the pair a2 points to to frame+8, then calls `callee`
// (a0, a1, &frame[0], &frame[8], t0 = a2[0], t1 = a2[1], t2 = pairs[1], t3 as it was); v0 = its result.
void pair_call_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pairs, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t arg_hi = word(rt, source + 4u), arg_lo = word(rt, source);
    const std::uint32_t static_hi = word(rt, pairs + 4u), static_lo = word(rt, pairs);
    put(rt, frame.at(12u), arg_hi);
    put(rt, frame.at(8u), arg_lo);
    put(rt, frame.at(0u), static_lo);
    put(rt, frame.at(4u), static_hi);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), arg_lo, arg_hi, static_hi, ctx.gpr[11]});
}

// Pair call B: frame+0 = the static pair, frame+8 = a3's pair, frame+16 = a2's pair; calls `callee`
// (a0, a1, &frame[0], &frame[16], t0 = &frame[8], t1 = a2[0], t2 = a2[1], t3 = pairs[0]); v0 = its result.
void pair_call_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pairs, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t second = ctx.gpr[6], third = ctx.gpr[7];
    const std::uint32_t third_lo = word(rt, third);
    const std::uint32_t second_hi = word(rt, second + 4u);
    const std::uint32_t second_lo = word(rt, second);
    const std::uint32_t static_hi = word(rt, pairs + 4u);
    const std::uint32_t static_lo = word(rt, pairs);
    const std::uint32_t third_hi = word(rt, third + 4u);
    put(rt, frame.at(0x14u), second_hi);
    put(rt, frame.at(0x10u), second_lo);
    put(rt, frame.at(0x0Cu), third_hi);
    put(rt, frame.at(8u), third_lo);
    put(rt, frame.at(0u), static_lo);
    put(rt, frame.at(4u), static_hi);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(0x10u), frame.at(8u), second_lo, second_hi, static_lo});
}

// worker(this, a1, constant); v0 = this.
void call_with_constant(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t worker, std::uint32_t constant) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    (void)call_guest(rt, ctx, worker, {self, ctx.gpr[5], constant});
    ctx.gpr[2] = self;
}

// clamp_step: the two-bound step shared with batches 63 and 65. `limits` holds the bounds (+8 lower, +12 upper) and the words +0 / +4 added to
// the value; the result replaces *p, and is v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets; the members have the same words as func_08C3FEB8 /
// func_08C4045C, whose paths were read in batch 63): the word +4 nonzero while +12 is 0 (the bne loads the lower bound +8 in its delay slot, as when +12 is
// set), a value at or above the upper bound with a lower bound below it (word +4 is added), and a lower bound at or above the upper one (r2 = value < lower
// in the delay slot, then word +4 when it holds, else word +0). The routes only take upper != 0 or +4 == 0 with a value below the upper bound.
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

// Node initialisers: this[0] = a1, this[1] = *a2, this[2] = a2[1], this[3] = 0, then a per-type routine (tail call).
void node_init_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4], pair = ctx.gpr[6];
    const std::uint32_t first = word(rt, pair), second = word(rt, pair + 4u);
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 8u, second);
    put(rt, self + 4u, first);
    put(rt, self + 12u, 0u);
    tail_call(ctx, target, {});
}

// The accessor of a function-local static container: while its guard byte is 0 and func_08B6441C(guard) lets this caller build it, the container is
// made empty (+16 = +12 = its header at +4; +4, +8, +20 cleared), func_08B64494(guard) releases the guard and func_08B77B1C(destructor) registers
// the destructor for exit. v0 = the container's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (another caller is
// building it: the beqz lands on the epilogue, and v0 = the object is set there; the delay slot a0 = guard is dead).
void get_empty_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                         std::uint32_t destructor) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 16u, object + 4u);
            put(rt, object + 20u, 0u);
            put(rt, object + 4u, 0u);
            put(rt, object + 8u, 0u);
            put(rt, object + 12u, object + 4u);
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
            (void)call_guest(rt, ctx, kAtExit, {destructor});
        }
    }
    ctx.gpr[2] = object;
}

} // namespace

// func_XXXXXXXX (pair call A): the pair at STATIC and the pair a2 points to are copied to the stack and CALLEE gets pointers to both.
void func_08C2A7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F510u, 0x08C2A6F4u); }
void func_08C2A9E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F640u, 0x08C2A924u); }
void func_08C249B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8EE00u, 0x08C248ECu); }
void func_08C2441C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8ED8Cu, 0x08C24358u); }
void func_08BFCC68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C7E0u, 0x08BFCB60u); }
void func_08C0491C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8D1D8u, 0x08C04814u); }
void func_08C05354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8D208u, 0x08C0524Cu); }

// func_XXXXXXXX (pair call B): the same with a3's pair as well.
void func_08BEA364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B894u, 0x08BEA244u); }
void func_08BFB06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C798u, 0x08BFAF4Cu); }
void func_08C04C00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D1E8u, 0x08C04AE0u); }
void func_08BEA65C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B8A4u, 0x08BEA53Cu); }
void func_08BEAB8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B8BCu, 0x08BEAA6Cu); }
void func_08BF8080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C6D4u, 0x08BF7F60u); }

// func_XXXXXXXX (node initialiser): see node_init_then (the tail target).
void func_08BF0BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF0BB0u); }
void func_08BF40C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF409Cu); }
void func_08BFFDE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BFFDC0u); }
void func_08C044A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C04484u); }
void func_08C0AAC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0AAA0u); }
void func_08C0B584(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0B560u); }

// func_XXXXXXXX (clamp step): see clamp_step; the same words as func_08C3FEB8 once the relocated internal jumps are masked.
void func_08C401C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C4176C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C4183C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C45D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }

// func_XXXXXXXX (call with a constant): worker(this, a1, CONSTANT); v0 = this.
void func_08BB186C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088BA424u, 168u); }
void func_08BB8ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088F18CCu, 1u); }
void func_08BB8AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088F18CCu, 4u); }
void func_08BB8CFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088F18CCu, 8u); }
void func_08BB8E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088F18CCu, 64u); }
void func_08BB8B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 0x088F18CCu, 1u); }

// func_XXXXXXXX (empty container accessor): see get_empty_container (guard, object, destructor).
void func_08BE9C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76A28u, 0x08D76A34u, 0x0896BA28u); }
void func_08BECDA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76A50u, 0x08D76A68u, 0x089751ACu); }
void func_08BECE24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76A58u, 0x08D76A80u, 0x089751BCu); }
void func_08BEEFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76AB8u, 0x08D76AC4u, 0x0897FC38u); }
void func_08BF09B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76B58u, 0x08D76B64u, 0x089941E0u); }
void func_08BF19D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76BA8u, 0x08D76BB4u, 0x089978B8u); }
void func_08BF4318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76C20u, 0x08D76C2Cu, 0x0899E198u); }
void func_08BF6DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76C90u, 0x08D76C9Cu, 0x089A4C08u); }
void func_08BFD278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76CB8u, 0x08D76CC4u, 0x089B3A08u); }
void func_08BFE51C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76CE0u, 0x08D76CECu, 0x089B72E4u); }
void func_08C021D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76DA8u, 0x08D76DB4u, 0x089CB0B0u); }
void func_08C03524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76DD0u, 0x08D76DDCu, 0x089D0348u); }
void func_08C07064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76E48u, 0x08D76E54u, 0x089DB73Cu); }
void func_08C098CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76E98u, 0x08D76EA4u, 0x089E6748u); }
void func_08C0C7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76F38u, 0x08D76F50u, 0x089F89C4u); }
void func_08C0C820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76F40u, 0x08D76F68u, 0x089F89D4u); }

} // namespace hot_batch_68
} // namespace gt::game

#define HOT68(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_68::name, gt::ReturnKind::kind)
HOT68(0x08C2A7B8u, func_08C2A7B8, Word);
HOT68(0x08C2A9E8u, func_08C2A9E8, Word);
HOT68(0x08C249B0u, func_08C249B0, Word);
HOT68(0x08C2441Cu, func_08C2441C, Word);
HOT68(0x08BFCC68u, func_08BFCC68, Word);
HOT68(0x08C0491Cu, func_08C0491C, Word);
HOT68(0x08C05354u, func_08C05354, Word);
HOT68(0x08BEA364u, func_08BEA364, Word);
HOT68(0x08BFB06Cu, func_08BFB06C, Word);
HOT68(0x08C04C00u, func_08C04C00, Word);
HOT68(0x08BEA65Cu, func_08BEA65C, Word);
HOT68(0x08BEAB8Cu, func_08BEAB8C, Word);
HOT68(0x08BF8080u, func_08BF8080, Word);
HOT68(0x08BF0BD4u, func_08BF0BD4, Word);
HOT68(0x08BF40C0u, func_08BF40C0, Word);
HOT68(0x08BFFDE4u, func_08BFFDE4, Word);
HOT68(0x08C044A0u, func_08C044A0, Word);
HOT68(0x08C0AAC4u, func_08C0AAC4, Word);
HOT68(0x08C0B584u, func_08C0B584, Word);
// Written but not registered: func_08C401C4 writes memory and none of its 49 compared cases changed any (every case saw a zero *p, so the store of 0 over 0
// changed nothing).
// HOT68(0x08C401C4u, func_08C401C4, Word);
HOT68(0x08C4176Cu, func_08C4176C, Word);
HOT68(0x08C4183Cu, func_08C4183C, Word);
HOT68(0x08C45D0Cu, func_08C45D0C, Word);
HOT68(0x08BB186Cu, func_08BB186C, Word);
HOT68(0x08BB8ABCu, func_08BB8ABC, Word);
HOT68(0x08BB8AE8u, func_08BB8AE8, Word);
HOT68(0x08BB8CFCu, func_08BB8CFC, Word);
HOT68(0x08BB8E14u, func_08BB8E14, Word);
HOT68(0x08BB8B14u, func_08BB8B14, Word);
HOT68(0x08BE9C08u, func_08BE9C08, Word);
HOT68(0x08BECDA4u, func_08BECDA4, Word);
HOT68(0x08BECE24u, func_08BECE24, Word);
HOT68(0x08BEEFA0u, func_08BEEFA0, Word);
HOT68(0x08BF09B0u, func_08BF09B0, Word);
HOT68(0x08BF19D4u, func_08BF19D4, Word);
HOT68(0x08BF4318u, func_08BF4318, Word);
HOT68(0x08BF6DC0u, func_08BF6DC0, Word);
HOT68(0x08BFD278u, func_08BFD278, Word);
HOT68(0x08BFE51Cu, func_08BFE51C, Word);
HOT68(0x08C021D0u, func_08C021D0, Word);
HOT68(0x08C03524u, func_08C03524, Word);
HOT68(0x08C07064u, func_08C07064, Word);
HOT68(0x08C098CCu, func_08C098CC, Word);
HOT68(0x08C0C7A0u, func_08C0C7A0, Word);
HOT68(0x08C0C820u, func_08C0C820, Word);
