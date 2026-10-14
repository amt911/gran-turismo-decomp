// Hot batch 69: finishes more members of the shape families of batches 59 and 66 (call probe over every function of up to 70 instructions
// not yet decompiled, run on the five routes with --decomp off, as in batches 57-68; the reached ones grouped by instruction shape with
// `tools.shapes`). The instruction shape hides the immediates and the call targets, so every member was compared with its family's
// template word by word: the words that differ are only the relocated constants (the lui / addiu / lw of a static address) and the jal / j
// target, never a stack offset, a field offset or a register. The constants and call targets were then read from each member's own words
// (the generated code only gave the relocation; it was not copied) and the jal / j targets checked against the raw word (target * 4 + the
// load base 0x08804000). The members are straight-line code (no branch): there is no untaken path to check against the asm.
// Their classes aren't known yet, so they keep their addresses as names.
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
namespace hot_batch_69 {
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
void func_08BEAD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8B8C4u, 0x08BEAC80u); }
void func_08BECD5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8BD5Cu, 0x08BECC54u); }
void func_08BED2FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8BD64u, 0x08BED1F4u); }
void func_08BF3104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C268u, 0x08BF2FFCu); }
void func_08BF32A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C278u, 0x08BF3198u); }
void func_08BF4B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C528u, 0x08BF4A18u); }
void func_08BF508C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C530u, 0x08BF4F84u); }
void func_08BFB644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C7B8u, 0x08BFB53Cu); }
void func_08BFD13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8C7F8u, 0x08BFD034u); }
void func_08C0C758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DAB0u, 0x08C0C650u); }
void func_08C0C9FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DAB8u, 0x08C0C8F4u); }
void func_08C10D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DD6Cu, 0x08C10C90u); }
void func_08C11138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8DD74u, 0x08C11030u); }
void func_08C1C854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8E85Cu, 0x08C1C790u); }
void func_08C26DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F064u, 0x08C26CD4u); }
void func_08C26F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F06Cu, 0x08C26E70u); }
void func_08C2DB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F984u, 0x08C2DA9Cu); }

// func_XXXXXXXX (pair call B: the same with a3's pair as well.)
void func_08BEE39C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8BDE0u, 0x08BEE27Cu); }
void func_08BF1FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C248u, 0x08BF1E94u); }
void func_08BF81FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C6DCu, 0x08BF80DCu); }
void func_08BFEF50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CA88u, 0x08BFEE30u); }
void func_08C00028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CB7Cu, 0x08BFFF08u); }
void func_08C02EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CDB8u, 0x08C02DC0u); }
void func_08C03728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CDF8u, 0x08C03608u); }
void func_08C04A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D1E0u, 0x08C04964u); }
void func_08C06BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D384u, 0x08C06AC0u); }
void func_08C06D5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D38Cu, 0x08C06C3Cu); }
void func_08C082B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D528u, 0x08C08198u); }
void func_08C09C00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D6A8u, 0x08C09AE0u); }
void func_08C09D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D6B0u, 0x08C09C5Cu); }
void func_08C0B958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D9F4u, 0x08C0B838u); }

// func_XXXXXXXX (init handle: clears the first nine bytes of a0 and calls the initialiser CALLEE.)
void func_08BA2A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BC5698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB992Cu); }
void func_08BC56F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }
void func_08BC5734(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }
void func_08BC5774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }
void func_08BC57B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }
void func_08BC57F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }
void func_08BC5834(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08BB9718u); }

// func_XXXXXXXX (node initialiser: see node_init_then (the tail target).)
void func_08BEFD08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BEFCDCu); }
void func_08BF3490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF346Cu); }
void func_08BFE6E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BFE6BCu); }
void func_08C045A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0457Cu); }
void func_08C0F5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C0F5BCu); }
void func_08C14850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C1482Cu); }
void func_08C01DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08C01D9Cu); }
void func_08BF0984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { node_init_then(rt, ctx, 0x08BF0960u); }

} // namespace hot_batch_69
} // namespace gt::game

#define HOT69(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_69::name, gt::ReturnKind::kind)
HOT69(0x08BEAD88u, func_08BEAD88, Word);
HOT69(0x08BECD5Cu, func_08BECD5C, Word);
HOT69(0x08BED2FCu, func_08BED2FC, Word);
HOT69(0x08BF3104u, func_08BF3104, Word);
HOT69(0x08BF32A0u, func_08BF32A0, Word);
HOT69(0x08BF4B20u, func_08BF4B20, Word);
HOT69(0x08BF508Cu, func_08BF508C, Word);
HOT69(0x08BFB644u, func_08BFB644, Word);
HOT69(0x08BFD13Cu, func_08BFD13C, Word);
HOT69(0x08C0C758u, func_08C0C758, Word);
HOT69(0x08C0C9FCu, func_08C0C9FC, Word);
HOT69(0x08C10D98u, func_08C10D98, Word);
HOT69(0x08C11138u, func_08C11138, Word);
HOT69(0x08C1C854u, func_08C1C854, Word);
HOT69(0x08C26DDCu, func_08C26DDC, Word);
HOT69(0x08C26F78u, func_08C26F78, Word);
HOT69(0x08C2DB60u, func_08C2DB60, Word);
HOT69(0x08BEE39Cu, func_08BEE39C, Word);
HOT69(0x08BF1FB4u, func_08BF1FB4, Word);
HOT69(0x08BF81FCu, func_08BF81FC, Word);
HOT69(0x08BFEF50u, func_08BFEF50, Word);
HOT69(0x08C00028u, func_08C00028, Word);
HOT69(0x08C02EE0u, func_08C02EE0, Word);
HOT69(0x08C03728u, func_08C03728, Word);
HOT69(0x08C04A84u, func_08C04A84, Word);
HOT69(0x08C06BE0u, func_08C06BE0, Word);
HOT69(0x08C06D5Cu, func_08C06D5C, Word);
HOT69(0x08C082B8u, func_08C082B8, Word);
HOT69(0x08C09C00u, func_08C09C00, Word);
HOT69(0x08C09D7Cu, func_08C09D7C, Word);
HOT69(0x08C0B958u, func_08C0B958, Word);
HOT69(0x08BA2A74u, func_08BA2A74, Word);
HOT69(0x08BC5698u, func_08BC5698, Word);
HOT69(0x08BC56F4u, func_08BC56F4, Word);
HOT69(0x08BC5734u, func_08BC5734, Word);
HOT69(0x08BC5774u, func_08BC5774, Word);
HOT69(0x08BC57B4u, func_08BC57B4, Word);
HOT69(0x08BC57F4u, func_08BC57F4, Word);
HOT69(0x08BC5834u, func_08BC5834, Word);
HOT69(0x08BEFD08u, func_08BEFD08, Word);
HOT69(0x08BF3490u, func_08BF3490, Word);
HOT69(0x08BFE6E0u, func_08BFE6E0, Word);
HOT69(0x08C045A0u, func_08C045A0, Word);
HOT69(0x08C0F5E0u, func_08C0F5E0, Word);
HOT69(0x08C14850u, func_08C14850, Word);
HOT69(0x08C01DC0u, func_08C01DC0, Word);
HOT69(0x08BF0984u, func_08BF0984, Word);
