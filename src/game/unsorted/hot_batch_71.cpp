// Hot batch 71: finishes more members of the shape families of batches 59 and 66 (call probe over every function of up to 70 instructions
// not yet decompiled, run on the five routes with --decomp off, as in batches 57-70; the reached ones grouped by instruction shape with
// `tools.shapes`). The instruction shape hides the immediates and the call targets, so every member was compared with its family's
// template word by word: the words that differ are only the relocated constants (the lui / addiu / lw of a static address) and the jal / j
// target, never a stack offset, a field offset or a register. The constants and call targets were then read from each member's own words
// (the generated code only gave the relocation; it was not copied) and the jal / j targets checked against the raw word (target * 4 + the
// load base 0x08804000). The members are straight-line code (no branch): there is no untaken path to check against the asm.
// Rows generated with tools.family_members (--cpp) and re-read with --check against the words. Their classes aren't known yet, so they keep their addresses as names.
//
// Families (the implementation is the one of the earlier batch, with this member's constants):
//  - "pair call A" (18 instructions, batch 66): copies the pair at a static address and the pair a2 points to to the stack and calls a worker;
//  - "pair call B" (23 instructions, batch 66): the same with a3's pair as well;
//  - "init handle" (16 instructions, batch 66): clears the first nine bytes of this and calls an initialiser;
//  - "node initialiser" (7 instructions, batch 59): stores three words into this and tail calls a per-type routine.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_71 {
namespace {

using namespace gt::game::hot;

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

// func_XXXXXXXX (pair call A: the pair at STATIC and the pair a2 points to are copied to the stack and CALLEE gets pointers to both.)
void func_08BEA080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8B884u, 0x08BE9F78u); }
void func_08BEE500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8BDE8u, 0x08BEE3F8u); }
void func_08BF2294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C258u, 0x08BF218Cu); }
void func_08BFCE04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C7E8u, 0x08BFCCFCu); }
void func_08BFCFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C7F0u, 0x08BFCE98u); }
void func_08BFE2E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C9A8u, 0x08BFE1E0u); }
void func_08C034DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8CDD8u, 0x08C033D4u); }
void func_08C06A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8D37Cu, 0x08C06970u); }
void func_08C08714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8D540u, 0x08C0860Cu); }
void func_08C0A010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8D6C0u, 0x08C09F08u); }
void func_08C0FF04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DC18u, 0x08C0FDFCu); }
void func_08C100A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DC20u, 0x08C0FF98u); }
void func_08C1E370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8EA08u, 0x08C1E268u); }
void func_08C265FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F054u, 0x08C264F4u); }
void func_08C272B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F07Cu, 0x08C271A8u); }
void func_08C2744C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F084u, 0x08C27344u); }
void func_08C275E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F08Cu, 0x08C274E0u); }
void func_08C27784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F094u, 0x08C2767Cu); }
void func_08C27920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F09Cu, 0x08C27818u); }
void func_08C27ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F0A4u, 0x08C279B4u); }

// func_XXXXXXXX (pair call B: the same with a3's pair as well.)
void func_08B8C7DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C880ACu, 0x08B8C6BCu); }
void func_08B8C958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C880B4u, 0x08B8C838u); }
void func_08BEA4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B89Cu, 0x08BEA3C0u); }
void func_08BF0064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C118u, 0x08BEFF44u); }
void func_08BF01E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C120u, 0x08BF00C0u); }
void func_08BF1CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C238u, 0x08BF1B9Cu); }
void func_08BF2130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C250u, 0x08BF2010u); }
void func_08BF3FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C43Cu, 0x08BF3EA4u); }
void func_08BF44D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C4CCu, 0x08BF43B0u); }
void func_08BF5F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C610u, 0x08BF5E6Cu); }
void func_08BF6108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C618u, 0x08BF5FE8u); }
void func_08BFB1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C7A0u, 0x08BFB0C8u); }
void func_08BFB364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C7A8u, 0x08BFB244u); }
void func_08BFB4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C7B0u, 0x08BFB3C0u); }
void func_08BFD704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C940u, 0x08BFD5E4u); }

// func_XXXXXXXX (init handle: clears the first nine bytes of a0 and calls the initialiser CALLEE.)
void func_08B9DA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DA24u); }
void func_08B9E59C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9E548u); }
void func_08B9E7C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9E774u); }
void func_08B9E9D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9E984u); }
void func_08BA286C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA28AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA28EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA2974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }

// func_XXXXXXXX (node initialiser: see node_init_then (the tail target).)
void func_08BEC8CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BEC8A8u); }
void func_08BEC904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BEC8E8u); }
void func_08BEEA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BEEA3Cu); }
void func_08BF0A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF0A48u); }
void func_08BF3358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF3334u); }
void func_08BF46AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF4688u); }
void func_08BF46E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF46C8u); }

} // namespace hot_batch_71
} // namespace gt::game

#define HOT71(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_71::name, gt::ReturnKind::kind)
HOT71(0x08BEA080u, func_08BEA080, Word);
HOT71(0x08BEE500u, func_08BEE500, Word);
HOT71(0x08BF2294u, func_08BF2294, Word);
HOT71(0x08BFCE04u, func_08BFCE04, Word);
HOT71(0x08BFCFA0u, func_08BFCFA0, Word);
HOT71(0x08BFE2E8u, func_08BFE2E8, Word);
HOT71(0x08C034DCu, func_08C034DC, Word);
HOT71(0x08C06A78u, func_08C06A78, Word);
HOT71(0x08C08714u, func_08C08714, Word);
HOT71(0x08C0A010u, func_08C0A010, Word);
HOT71(0x08C0FF04u, func_08C0FF04, Word);
HOT71(0x08C100A0u, func_08C100A0, Word);
HOT71(0x08C1E370u, func_08C1E370, Word);
HOT71(0x08C265FCu, func_08C265FC, Word);
HOT71(0x08C272B0u, func_08C272B0, Word);
HOT71(0x08C2744Cu, func_08C2744C, Word);
HOT71(0x08C275E8u, func_08C275E8, Word);
HOT71(0x08C27784u, func_08C27784, Word);
HOT71(0x08C27920u, func_08C27920, Word);
HOT71(0x08C27ABCu, func_08C27ABC, Word);
HOT71(0x08B8C7DCu, func_08B8C7DC, Word);
HOT71(0x08B8C958u, func_08B8C958, Word);
HOT71(0x08BEA4E0u, func_08BEA4E0, Word);
HOT71(0x08BF0064u, func_08BF0064, Word);
HOT71(0x08BF01E0u, func_08BF01E0, Word);
HOT71(0x08BF1CBCu, func_08BF1CBC, Word);
HOT71(0x08BF2130u, func_08BF2130, Word);
HOT71(0x08BF3FC4u, func_08BF3FC4, Word);
HOT71(0x08BF44D0u, func_08BF44D0, Word);
HOT71(0x08BF5F8Cu, func_08BF5F8C, Word);
HOT71(0x08BF6108u, func_08BF6108, Word);
HOT71(0x08BFB1E8u, func_08BFB1E8, Word);
HOT71(0x08BFB364u, func_08BFB364, Word);
HOT71(0x08BFB4E0u, func_08BFB4E0, Word);
HOT71(0x08BFD704u, func_08BFD704, Word);
HOT71(0x08B9DA78u, func_08B9DA78, Word);
HOT71(0x08B9E59Cu, func_08B9E59C, Word);
HOT71(0x08B9E7C8u, func_08B9E7C8, Word);
HOT71(0x08B9E9D8u, func_08B9E9D8, Word);
HOT71(0x08BA286Cu, func_08BA286C, Word);
HOT71(0x08BA28ACu, func_08BA28AC, Word);
HOT71(0x08BA28ECu, func_08BA28EC, Word);
HOT71(0x08BA2974u, func_08BA2974, Word);
HOT71(0x08BEC8CCu, func_08BEC8CC, Word);
HOT71(0x08BEC904u, func_08BEC904, Word);
HOT71(0x08BEEA60u, func_08BEEA60, Word);
HOT71(0x08BF0A6Cu, func_08BF0A6C, Word);
HOT71(0x08BF3358u, func_08BF3358, Word);
HOT71(0x08BF46ACu, func_08BF46AC, Word);
HOT71(0x08BF46E4u, func_08BF46E4, Word);
