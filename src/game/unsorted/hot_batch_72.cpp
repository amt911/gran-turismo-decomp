// Hot batch 72: finishes more members of the shape families of batches 59 and 66 (call probe over every function of up to 70 instructions
// not yet decompiled, run on the five routes with --decomp off, as in batches 57-71 (probe logs of batch 70; batch 71's functions excluded); the reached ones grouped by instruction shape with
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
// (Left for a later batch: the rest of these two families, the init-handle and node-initialiser families.)
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_72 {
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

} // namespace

// func_XXXXXXXX (pair call A: the pair at STATIC and the pair a2 points to are copied to the stack and CALLEE gets pointers to both.)
void func_08C27C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F0ACu, 0x08C27B50u); }
void func_08C28510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F1A8u, 0x08C28418u); }
void func_08C2A53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F4E4u, 0x08C2A444u); }
void func_08C2D1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F7B4u, 0x08C2D124u); }
void func_08C2D384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F7BCu, 0x08C2D27Cu); }
void func_08C2D520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F7C4u, 0x08C2D418u); }
void func_08C2F4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8FA24u, 0x08C2F3B0u); }
void func_08C2F610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8FA2Cu, 0x08C2F54Cu); }
void func_08C2F7AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8FA34u, 0x08C2F6A4u); }
void func_08C2F948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8FA3Cu, 0x08C2F840u); }
void func_08C388FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9030Cu, 0x08C387F4u); }
void func_08C38A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90314u, 0x08C38990u); }
void func_08C38C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9031Cu, 0x08C38B2Cu); }
void func_08C38DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90324u, 0x08C38CC8u); }
void func_08C38F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9032Cu, 0x08C38E64u); }
void func_08C39108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90334u, 0x08C39000u); }
void func_08C392A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9033Cu, 0x08C3919Cu); }
void func_08C39440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90344u, 0x08C39338u); }
void func_08C395DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9034Cu, 0x08C394D4u); }
void func_08C39778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90354u, 0x08C39670u); }
void func_08C39914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C9035Cu, 0x08C3980Cu); }
void func_08C39AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90364u, 0x08C399A8u); }
void func_08C3AA30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C903F8u, 0x08C3A928u); }
void func_08C3ABCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90400u, 0x08C3AAC4u); }
void func_08C3AD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90408u, 0x08C3AC60u); }
void func_08C3AF04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90410u, 0x08C3ADFCu); }
void func_08C3B0A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90418u, 0x08C3AF98u); }
void func_08C3B23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90420u, 0x08C3B134u); }
void func_08C3B3D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90428u, 0x08C3B2D0u); }
void func_08C3B574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90430u, 0x08C3B46Cu); }
void func_08C3B710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90438u, 0x08C3B608u); }
void func_08C3B8AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90440u, 0x08C3B7A4u); }
void func_08C3BA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90448u, 0x08C3B940u); }

// func_XXXXXXXX (pair call B: the same with a3's pair as well.)
void func_08BFD880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C948u, 0x08BFD760u); }
void func_08BFF0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CA90u, 0x08BFEFACu); }
void func_08C001A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CB84u, 0x08C00084u); }
void func_08C04D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D1F0u, 0x08C04C5Cu); }
void func_08C04EF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D1F8u, 0x08C04DD8u); }
void func_08C05158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D200u, 0x08C05038u); }
void func_08C06ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D394u, 0x08C06DB8u); }
void func_08C085B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D538u, 0x08C08490u); }
void func_08C0B7DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D9ECu, 0x08C0B6BCu); }
void func_08C0F43C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DB8Cu, 0x08C0F31Cu); }
void func_08C0FA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DC10u, 0x08C0F928u); }
void func_08C12794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DE80u, 0x08C12674u); }
void func_08C12910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DE88u, 0x08C127F0u); }
void func_08C12A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DE90u, 0x08C1296Cu); }
void func_08C14C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8E158u, 0x08C14B28u); }
void func_08C14DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8E160u, 0x08C14CA4u); }
void func_08C14F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8E168u, 0x08C14E20u); }

} // namespace hot_batch_72
} // namespace gt::game

#define HOT72(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_72::name, gt::ReturnKind::kind)
HOT72(0x08C27C58u, func_08C27C58, Word);
HOT72(0x08C28510u, func_08C28510, Word);
HOT72(0x08C2A53Cu, func_08C2A53C, Word);
HOT72(0x08C2D1E8u, func_08C2D1E8, Word);
HOT72(0x08C2D384u, func_08C2D384, Word);
HOT72(0x08C2D520u, func_08C2D520, Word);
HOT72(0x08C2F4B8u, func_08C2F4B8, Word);
HOT72(0x08C2F610u, func_08C2F610, Word);
HOT72(0x08C2F7ACu, func_08C2F7AC, Word);
HOT72(0x08C2F948u, func_08C2F948, Word);
HOT72(0x08C388FCu, func_08C388FC, Word);
HOT72(0x08C38A98u, func_08C38A98, Word);
HOT72(0x08C38C34u, func_08C38C34, Word);
HOT72(0x08C38DD0u, func_08C38DD0, Word);
HOT72(0x08C38F6Cu, func_08C38F6C, Word);
HOT72(0x08C39108u, func_08C39108, Word);
HOT72(0x08C392A4u, func_08C392A4, Word);
HOT72(0x08C39440u, func_08C39440, Word);
HOT72(0x08C395DCu, func_08C395DC, Word);
HOT72(0x08C39778u, func_08C39778, Word);
HOT72(0x08C39914u, func_08C39914, Word);
HOT72(0x08C39AB0u, func_08C39AB0, Word);
HOT72(0x08C3AA30u, func_08C3AA30, Word);
HOT72(0x08C3ABCCu, func_08C3ABCC, Word);
HOT72(0x08C3AD68u, func_08C3AD68, Word);
HOT72(0x08C3AF04u, func_08C3AF04, Word);
HOT72(0x08C3B0A0u, func_08C3B0A0, Word);
HOT72(0x08C3B23Cu, func_08C3B23C, Word);
HOT72(0x08C3B3D8u, func_08C3B3D8, Word);
HOT72(0x08C3B574u, func_08C3B574, Word);
HOT72(0x08C3B710u, func_08C3B710, Word);
HOT72(0x08C3B8ACu, func_08C3B8AC, Word);
HOT72(0x08C3BA48u, func_08C3BA48, Word);
HOT72(0x08BFD880u, func_08BFD880, Word);
HOT72(0x08BFF0CCu, func_08BFF0CC, Word);
HOT72(0x08C001A4u, func_08C001A4, Word);
HOT72(0x08C04D7Cu, func_08C04D7C, Word);
HOT72(0x08C04EF8u, func_08C04EF8, Word);
HOT72(0x08C05158u, func_08C05158, Word);
HOT72(0x08C06ED8u, func_08C06ED8, Word);
HOT72(0x08C085B0u, func_08C085B0, Word);
HOT72(0x08C0B7DCu, func_08C0B7DC, Word);
HOT72(0x08C0F43Cu, func_08C0F43C, Word);
HOT72(0x08C0FA48u, func_08C0FA48, Word);
HOT72(0x08C12794u, func_08C12794, Word);
HOT72(0x08C12910u, func_08C12910, Word);
HOT72(0x08C12A8Cu, func_08C12A8C, Word);
HOT72(0x08C14C48u, func_08C14C48, Word);
HOT72(0x08C14DC4u, func_08C14DC4, Word);
HOT72(0x08C14F40u, func_08C14F40, Word);
