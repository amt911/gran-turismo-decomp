// Batch main-887 (draft-only, unverified): the epilogue-shaped functions the earlier seeds could not run with useful
// inputs. Every one of them saves s0/ra into a 0x10 frame, does one or two things and then returns or tail-calls; the
// vtable word the asm's lui/addiu pair leaves in v0 is stored at +0 of the object a0 points at, and the first call is
// the base destructor (or the field getter) that runs before it. Inputs: /home/andres/repos/gt-ops/shapes/shapes_main887.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_887 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The shape of most of this batch: the first call takes this (a0), then the class's vtable is stored at +0 and v0 keeps
// it, because the asm loads it into v0 for that store. v0 is therefore the vtable on this path, not the call's answer.
void store_vtable_after_call(Runtime &rt, AllegrexContext &ctx, u32 first_call, u32 vtable) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, first_call, {self});
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// The same, then the frame is popped and the work goes on in a tail call of `tail` with this + `tail_offset`; v0 still
// holds the vtable as the guest code at `tail` starts.
void store_vtable_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first_call, u32 vtable, u32 tail, u32 tail_offset) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, first_call, {self});
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
    tail_call(ctx, tail, {self + tail_offset});
}

// 0x08909C84 (a0 = object): v0 = (0x08909B60(a0) - word at +0x34) >> 4, logical — the number of 16-byte entries between two pointers.
void func_08909C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); const u32 self = ctx.gpr[4]; ctx.gpr[2] = (call(rt, ctx, 0x08909B60u, {self}) - word(rt, self + 0x34u)) >> 4; }
// 0x08916A80 (a0 = object): 0x0890B2F4(a0 + 4), then the byte at +0x14 and the word at +0x10 are cleared; v0 = the call's answer.
void func_08916A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); const u32 self = ctx.gpr[4]; ctx.gpr[2] = call(rt, ctx, 0x0890B2F4u, {self + 4u}); rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(0u)); put(rt, self + 0x10u, 0u); }
// 0x0891A454: 0x08926D60(a0) then the vtable 0x08CC5B98 at +0.
void func_0891A454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08926D60u, 0x08CC5B98u); }
// 0x08922AD4: 0x0892025C(a0) then the vtable 0x08CC5F80 at +0.
void func_08922AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x0892025Cu, 0x08CC5F80u); }
// 0x089299D8: 0x08929DD0(a0) then the vtable 0x08CC62D0 at +0.
void func_089299D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08929DD0u, 0x08CC62D0u); }
// 0x089302D8 (a0 = object): v0 = 0x089302D0(a0) (the word at +8) rounded toward zero to a multiple of 4 — the asm's
// addiu 3 / slti 0 / movn pair, then an arithmetic shift; v1 keeps the biased value.
void func_089302D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); const s32 value = static_cast<s32>(call(rt, ctx, 0x089302D0u, {ctx.gpr[4]})); const s32 biased = value < 0 ? static_cast<s32>(static_cast<u32>(value) + 3u) : value; ctx.gpr[3] = static_cast<u32>(biased); ctx.gpr[2] = static_cast<u32>(biased >> 2); }
// 0x08931910 (a0 = object): a0 = 0x089363FC(a0), then a tail call of 0x089365F8 with the words at +8 and +0xC as a2 / a3.
void func_08931910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = call(rt, ctx, 0x089363FCu, {self});
    tail_call(ctx, 0x089365F8u, {first, ctx.gpr[5], word(rt, self + 8u), word(rt, self + 0xCu)});
}
// 0x08931940: the same with the words at +0x10 and +0x14.
void func_08931940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = call(rt, ctx, 0x089363FCu, {self});
    tail_call(ctx, 0x089365F8u, {first, ctx.gpr[5], word(rt, self + 0x10u), word(rt, self + 0x14u)});
}
// 0x08932528: the same with the words at +0 and +4.
void func_08932528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = call(rt, ctx, 0x089363FCu, {self});
    tail_call(ctx, 0x089365F8u, {first, ctx.gpr[5], word(rt, self), word(rt, self + 4u)});
}
// 0x08936718 (a0 = object): v0 = 1 when 0x08B35FD8(0x089366FC(a0), 0x08CA1F44) is 0, else 0 (sltiu against 1).
void func_08936718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 first = call(rt, ctx, 0x089366FCu, {ctx.gpr[4]});
    const u32 answer = call(rt, ctx, 0x08B35FD8u, {first, 0x08CA1F44u});
    ctx.gpr[2] = answer < 1u ? 1u : 0u;
}
// 0x08951914 (a0 = object): the halfword at +0 gets 1 through 0x089518F4, then a tail call of 0x08951904 sets the one at +2.
void func_08951914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); call(rt, ctx, 0x089518F4u, {ctx.gpr[4], 1u}); tail_call(ctx, 0x08951904u, {ctx.gpr[4], 1u}); }
// 0x0895F0F8 (a0 = object): f0 = 0x089468C0(a0 + 0x24) * the float at +0x10; v0 stays the call's answer.
void func_0895F0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const GuestResult result = call_guest(rt, ctx, 0x089468C0u, {self + 0x24u}, {});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = psp_mul(result.f0, float_at(rt, self + 0x10u));
}
// 0x089C1644: 0x08A5B828(a0) then the vtable 0x08CCEFF8 at +0.
void func_089C1644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A5B828u, 0x08CCEFF8u); }
// 0x089F58E8: 0x08965984(a0) then the vtable 0x08CC8158 at +0.
void func_089F58E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08965984u, 0x08CC8158u); }
// 0x089F5F3C: 0x08965984(a0) then the vtable 0x08CC8028 at +0 (the sibling of 0x089F58E8).
void func_089F5F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08965984u, 0x08CC8028u); }
// 0x08A31D00: 0x08A76ACC(a0) then the vtable 0x08CD15D0 at +0.
void func_08A31D00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A76ACCu, 0x08CD15D0u); }
// 0x08A43CD4: 0x08A76ACC(a0) then the vtable 0x08CD0E60 at +0.
void func_08A43CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A76ACCu, 0x08CD0E60u); }
// 0x08A43D04: the same body as 0x08A43CD4 (a second constructor of the same class).
void func_08A43D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A76ACCu, 0x08CD0E60u); }
// 0x08A52690: 0x08A7E88C(a0) then the vtable 0x08CB7440 at +0.
void func_08A52690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A7E88Cu, 0x08CB7440u); }
// 0x08A55D40: 0x08A725A0(a0) then the vtable 0x08CD20C0 at +0.
void func_08A55D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_after_call(rt, ctx, 0x08A725A0u, 0x08CD20C0u); }
// 0x088359AC: 0x08A76ACC(a0), the vtable 0x08CBDCD8 at +0, then a tail call of 0x0890FA6C(a0 + 0x18).
void func_088359AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08A76ACCu, 0x08CBDCD8u, 0x0890FA6Cu, 0x18u); }
// 0x08884A94: 0x0887E1E0(a0), the vtable 0x08CC16C0 at +0, then a tail call of 0x08884720(a0).
void func_08884A94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x0887E1E0u, 0x08CC16C0u, 0x08884720u, 0u); }
// 0x0889464C: 0x08898EE8(a0), the vtable 0x08CC1E08 at +0, and the word at +0x54 cleared; v0 keeps the vtable.
void func_0889464C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u); const u32 self = ctx.gpr[4]; call(rt, ctx, 0x08898EE8u, {self}); put(rt, self, 0x08CC1E08u); put(rt, self + 0x54u, 0u); ctx.gpr[2] = 0x08CC1E08u; }
// 0x088E84E4 (a0 = object, a1 kept): 0x088D0918(a0 + 0x30), then a tail call of 0x088E84B8(a0, a1, -1, -1).
void func_088E84E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = call(rt, ctx, 0x088D0918u, {self + 0x30u});
    tail_call(ctx, 0x088E84B8u, {self, ctx.gpr[5], 0xFFFFFFFFu, 0xFFFFFFFFu});
}
// 0x0890AE68 (a0 = a word stored in the opposite byte order): v0 = 1 when its byteswap equals 0x08B29E88(0), else 0.
void func_0890AE68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 stored = word(rt, ctx.gpr[4]);
    const u32 answer = call(rt, ctx, 0x08B29E88u, {0u});
    ctx.gpr[2] = (__builtin_bswap32(stored) ^ answer) < 1u ? 1u : 0u;
}
// 0x08918EFC: 0x08926D60(a0), the vtable 0x08CC5B40 at +0, then a tail call of 0x08918EF4(a0) (clears the word at +0x1F8).
void func_08918EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08926D60u, 0x08CC5B40u, 0x08918EF4u, 0u); }
// 0x08A21EA0: 0x08A76ACC(a0), the vtable 0x08CBE7F8 at +0, then a tail call of 0x08AB8644(a0 + 0x14).
void func_08A21EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08A76ACCu, 0x08CBE7F8u, 0x08AB8644u, 0x14u); }
// 0x08A36D70: 0x08A76ACC(a0), the vtable 0x08CD1C30 at +0, then a tail call of 0x08B487AC(a0 + 0x14).
void func_08A36D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08A76ACCu, 0x08CD1C30u, 0x08B487ACu, 0x14u); }
// 0x08A39928: 0x08A76ACC(a0), the vtable 0x08CCC730 at +0, then a tail call of 0x08C1B4D8(a0 + 0x14).
void func_08A39928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08A76ACCu, 0x08CCC730u, 0x08C1B4D8u, 0x14u); }
// 0x08A3CBB0: 0x08A76ACC(a0), the vtable 0x08CD1A10 at +0, then a tail call of 0x08B28D34(a0 + 0x14).
void func_08A3CBB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_then_tail(rt, ctx, 0x08A76ACCu, 0x08CD1A10u, 0x08B28D34u, 0x14u); }

} // namespace
} // namespace new_batch_main_887
} // namespace gt::game

#define MAIN887(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_887::name, gt::ReturnKind::kind)
MAIN887(0x08909C84u, func_08909C84, Word);
MAIN887(0x08916A80u, func_08916A80, Word);
MAIN887(0x0891A454u, func_0891A454, Word);
MAIN887(0x08922AD4u, func_08922AD4, Word);
MAIN887(0x089299D8u, func_089299D8, Word);
MAIN887(0x089302D8u, func_089302D8, Word);
MAIN887(0x08931910u, func_08931910, Word);
MAIN887(0x08931940u, func_08931940, Word);
MAIN887(0x08932528u, func_08932528, Word);
MAIN887(0x08936718u, func_08936718, Word);
MAIN887(0x08951914u, func_08951914, Word);
MAIN887(0x0895F0F8u, func_0895F0F8, Float);
MAIN887(0x089C1644u, func_089C1644, Word);
MAIN887(0x089F58E8u, func_089F58E8, Word);
MAIN887(0x089F5F3Cu, func_089F5F3C, Word);
MAIN887(0x08A31D00u, func_08A31D00, Word);
MAIN887(0x08A43CD4u, func_08A43CD4, Word);
MAIN887(0x08A43D04u, func_08A43D04, Word);
MAIN887(0x08A52690u, func_08A52690, Word);
MAIN887(0x08A55D40u, func_08A55D40, Word);
MAIN887(0x088359ACu, func_088359AC, Word);
MAIN887(0x08884A94u, func_08884A94, Word);
MAIN887(0x0889464Cu, func_0889464C, Word);
MAIN887(0x088E84E4u, func_088E84E4, Word);
MAIN887(0x0890AE68u, func_0890AE68, Word);
MAIN887(0x08918EFCu, func_08918EFC, Word);
MAIN887(0x08A21EA0u, func_08A21EA0, Word);
MAIN887(0x08A36D70u, func_08A36D70, Word);
MAIN887(0x08A39928u, func_08A39928, Word);
MAIN887(0x08A3CBB0u, func_08A3CBB0, Word);
