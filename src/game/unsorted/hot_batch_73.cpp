// Hot batch 73: finishes the shape families of batches 59, 66 and 63/68 (call probe over every function of up to 70 instructions not yet
// decompiled, run on the five routes with --decomp off, as in batches 57-72 (probe logs of batch 70; the functions of batch 72's branch
// excluded); the reached ones grouped by instruction shape with `tools.shapes`). The instruction shape hides the immediates and the call targets,
// so every member was compared with its family's template word by word (`tools.family_members` prints no `!!` line for any of them): the words
// that differ are only the relocated constants (the lui / addiu / lw of a static address) and the jal / j target, never a stack offset, a field
// offset or a register. The constants and call targets were read from each member's own words (the generated code only gave the relocation;
// it was not copied) and the jal / j targets checked against the raw word (target * 4 + the load base 0x08804000).
// Rows generated with tools.family_members (--cpp) and re-read with --check against the words. Their classes aren't known yet, so they keep
// their addresses as names.
//
// Families (the implementation is the one of the earlier batch, with this member's constants):
//  - "pair call B" (23 instructions, batch 66): copies the static pair, a2's pair and a3's pair to the stack and calls a worker;
//  - "init handle" (16 instructions, batch 66): clears the first nine bytes of this and calls an initialiser;
//  - "node initialiser" (7 instructions, batch 59): stores three words into this and tail calls a per-type routine;
//  - "clamp step" (26 instructions, batches 63/68): the two-bound step, no constants of its own (every member has the same words).
// Written but not registered (the gate saw no case change memory): four clamp-step members, listed above the registrations.
// Left for a later batch: func_08C401C4 (clamp step; same reason, batch 68), the pair call A members that batch 72 takes, six empty-container accessors whose words differ from batch 68's template (two
// more statics and a displacement: a variant, not the same shape), and the larger families.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_73 {
namespace {

using namespace gt::game::hot;

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

// Init handle: this[0] = this[1] = 0, byte this[8] = 0 (that one before the call), then initialiser(this, a2 + a1[3], 0);
// a3 and t0-t3 reach the initialiser as they came. v0 = this.
void init_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t initialiser) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t offset = word(rt, ctx.gpr[5] + 12u);
    put(rt, self, 0u);
    const std::uint32_t where = ctx.gpr[6] + offset;
    put(rt, self + 4u, 0u);
    rt.memory().store8(self + 8u, 0u);
    (void)call_guest(rt, ctx, initialiser, {self, where, 0u});
    ctx.gpr[2] = self;
}

// clamp_step: the two-bound step shared with batches 63 and 65. `limits` holds the bounds (+8 lower, +12 upper) and the words +0 / +4 added to
// the value; the result replaces *p, and is v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets; the members have the same words as func_08C3FEB8 /
// func_08C4045C, whose paths were read in batch 63): the word +4 nonzero while +12 is 0 (the bne loads the lower bound +8 in its delay slot, as when +12 is
// set), a value at or above the upper bound with a lower bound below it (word +4 is added), and a lower bound at or above the upper one (r2 = value < lower
// in the delay slot, then word +4 when it holds, else word +0). The routes only take upper != 0 or +4 == 0 with a value below the upper bound.
// Batch 73's coverage run agrees: upper == 0 (both inner branches and the unbounded sum), lower >= upper and x >= upper never ran.
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

} // namespace

// func_XXXXXXXX (pair call B: the same with a3's pair as well.)
void func_08C1DCAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8E9B0u, 0x08C1DB8Cu); }
void func_08C1EAC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8EA54u, 0x08C1E9A0u); }
void func_08C2A004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8F344u, 0x08C29EE4u); }
void func_08C2CB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8F7ACu, 0x08C2CA48u); }
void func_08C36E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8FFCCu, 0x08C36CE8u); }
void func_08C36F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8FFD4u, 0x08C36E64u); }

// func_XXXXXXXX (init handle: clears the first nine bytes of a0 and calls the initialiser CALLEE.)
void func_08BA29B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA29F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA2A34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA2AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BB95AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BC51CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BC5178u); }
void func_08BC5A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BC5A08u); }
void func_08BC5EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BC5E74u); }
void func_08BC6138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BC60E4u); }
void func_08BCDF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCDFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCDFEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE0B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE0F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE1B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE2B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BCE2F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }

// func_XXXXXXXX (node initialiser: see node_init_then (the tail target).)
void func_08BFD208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BFD1E4u); }
void func_08BFDF00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BFDEDCu); }
void func_08BFE5C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BFE5A4u); }
void func_08C0C330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0C30Cu); }
void func_08C0C368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0C34Cu); }
void func_08C0ECD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0ECACu); }
void func_08C0EE7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0EE58u); }
void func_08C0EF64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0EF40u); }
void func_08C10998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C10974u); }
void func_08C109D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C109B4u); }

// func_XXXXXXXX (clamp step): see clamp_step; the same words as func_08C3FEB8 once the relocated internal jumps are masked.
void func_08C4063C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C40FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C413DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C417D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }

} // namespace hot_batch_73
} // namespace gt::game

#define HOT73(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_73::name, gt::ReturnKind::kind)
HOT73(0x08C4063Cu, func_08C4063C, Word);
// Written but not registered: func_08C40FA4, func_08C413DC, func_08C41934 and func_08C41A04 write memory and none of their compared cases changed any
// (17, 17, 2 and 1 cases, every one saw a zero *p, so the store of 0 over 0 changed nothing), as with func_08C401C4 in batch 68.
// HOT73(0x08C40FA4u, func_08C40FA4, Word);
// HOT73(0x08C413DCu, func_08C413DC, Word);
HOT73(0x08C417D4u, func_08C417D4, Word);
// HOT73(0x08C41934u, func_08C41934, Word);
// HOT73(0x08C41A04u, func_08C41A04, Word);
HOT73(0x08C41AFCu, func_08C41AFC, Word);
HOT73(0x08C1DCACu, func_08C1DCAC, Word);
HOT73(0x08C1EAC0u, func_08C1EAC0, Word);
HOT73(0x08C2A004u, func_08C2A004, Word);
HOT73(0x08C2CB68u, func_08C2CB68, Word);
HOT73(0x08C36E08u, func_08C36E08, Word);
HOT73(0x08C36F84u, func_08C36F84, Word);
HOT73(0x08BFD208u, func_08BFD208, Word);
HOT73(0x08BFDF00u, func_08BFDF00, Word);
HOT73(0x08BFE5C8u, func_08BFE5C8, Word);
HOT73(0x08C0C330u, func_08C0C330, Word);
HOT73(0x08C0C368u, func_08C0C368, Word);
HOT73(0x08C0ECD0u, func_08C0ECD0, Word);
HOT73(0x08C0EE7Cu, func_08C0EE7C, Word);
HOT73(0x08C0EF64u, func_08C0EF64, Word);
HOT73(0x08C10998u, func_08C10998, Word);
HOT73(0x08C109D0u, func_08C109D0, Word);
HOT73(0x08BA29B4u, func_08BA29B4, Word);
HOT73(0x08BA29F4u, func_08BA29F4, Word);
HOT73(0x08BA2A34u, func_08BA2A34, Word);
HOT73(0x08BA2AB4u, func_08BA2AB4, Word);
HOT73(0x08BB95ACu, func_08BB95AC, Word);
HOT73(0x08BC51CCu, func_08BC51CC, Word);
HOT73(0x08BC5A5Cu, func_08BC5A5C, Word);
HOT73(0x08BC5EC8u, func_08BC5EC8, Word);
HOT73(0x08BC6138u, func_08BC6138, Word);
HOT73(0x08BCDF6Cu, func_08BCDF6C, Word);
HOT73(0x08BCDFACu, func_08BCDFAC, Word);
HOT73(0x08BCDFECu, func_08BCDFEC, Word);
HOT73(0x08BCE02Cu, func_08BCE02C, Word);
HOT73(0x08BCE0B4u, func_08BCE0B4, Word);
HOT73(0x08BCE0F4u, func_08BCE0F4, Word);
HOT73(0x08BCE134u, func_08BCE134, Word);
HOT73(0x08BCE174u, func_08BCE174, Word);
HOT73(0x08BCE1B4u, func_08BCE1B4, Word);
HOT73(0x08BCE1F4u, func_08BCE1F4, Word);
HOT73(0x08BCE234u, func_08BCE234, Word);
HOT73(0x08BCE274u, func_08BCE274, Word);
HOT73(0x08BCE2B4u, func_08BCE2B4, Word);
HOT73(0x08BCE2F4u, func_08BCE2F4, Word);
