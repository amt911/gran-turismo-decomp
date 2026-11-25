// Main session 917 (draft-only batch): short thunks, virtual forwarders and relative adjusters, written by hand as structured
// C from `tools.disasm`. Every function is a leaf or ends in a `j` (a tail call): the arguments are built exactly where the
// asm builds them, so a register the asm leaves alone keeps the value the caller passed. v0 is set wherever the asm sets it
// (before the jump, or in the delay slot's place here), so a target that never writes v0 still hands the original's on.
// 0x08B35530 is `break 0` (a trap, no faithful structured-C form), so it is skipped, not registered.
// The shapes for the synthetic generator live in /home/andres/repos/gt-ops/shapes/shapes_main917.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_917 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// 0x08C75504 (a2 = holder): a2 = *a2, a3 = *(a2 - 0xC) (the delay slot); tail call 0x08C752BC(a0, a1, a2, a3).

// 0x08C78738: tail call 0x08C78744(a0 - 8) (the addiu is the first instruction, the nop the delay slot).

// 0x088A5CC0 (a0 = holder): v0 = *a0; the tail call 0x08944AAC starts at *(v0 + 0x4C8) + 0x690.

// 0x08A1C1E0: shifts the argument window left by one — tail call 0x08B2D61C(a1, a2, a3); a3 keeps its value.

// 0x08AD414C (a0 = object): v0 = 0x08CD6DA8; the word is stored at *a0 (the delay slot) and the tail call 0x08AD4430 takes
// the object.

// 0x08C76264 (a0 = holder): v0 = *a0, a2 -= a1, a1 -= v0; tail call 0x08C75FF8.

// 0x0885EA48 (a0 = holder): v0 = *a0; a2 = a3 = -1; tail call 0x088DA2F4(*(v0 + 0x5AC)).
void func_0885EA48(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    ctx.gpr[6] = 0xFFFFFFFFu;
    ctx.gpr[7] = 0xFFFFFFFFu;
    tail_call(ctx, 0x088DA2F4u, {word(rt, object + 0x5ACu)});
}

// 0x0885F43C (a0 = holder): the object is *a0; tail call 0x0885F430(object).

// 0x0886006C (a0 = holder): the object is *a0, its virtual at +0x20 is the tail call; v0 = the vtable, a0 = the object.

// 0x088E46DC: the same shape through slot +0x80.
void func_088E46DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + 0x80u);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object});
}

// 0x088E4704: the same shape through slot +0x88.
void func_088E4704(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + 0x88u);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object});
}

// 0x088E43A8 (a0 = holder, a1 = a byte): v1 = *(*a0), v0 = v1[0x18] = t9; a1 &= 0xFF, and the virtual is the tail call.
void func_088E43A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + 0x18u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object, ctx.gpr[5] & 0xFFu});
}

// 0x088E444C: the same through slot +0x20; a1 is not touched here, the virtual receives it unchanged.
void func_088E444C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + 0x20u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object});
}

// 0x0893389C: the global at 0x08CF2968 holds the table; v0 = table[0xC0], v1 = table[0xC4] (the delay slot).

// 0x089338B0: the same table, v0 = table[0xC8], v1 = table[0xCC] (the delay slot).

// 0x08B1C3E4 (a0, a1 = the two values): tail call 0x08B1C590(0x08D68A50, a1, a0, a1).

// 0x08B1C47C (a1 keeps its value): tail call 0x08B1C590(0x08D68A70, a1, -5, -1).

// 0x08C7506C (a0 = holder, a1/a2/t0 = values): v0 = *a0, a2 -= a1, t0 = (s8)t0, a1 -= v0; tail call 0x08C74FAC.

// 0x08C76214 / 0x08C76228 / 0x08C7623C / 0x08C76250 (a0 = holder): v0 = *a0, a2 -= a1, t0 -= a3, a1 -= v0;
// tail call 0x08C75FF8. Same body at four addresses.

void func_08C76228(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    ctx.gpr[6] = ctx.gpr[6] - ctx.gpr[5];
    ctx.gpr[8] = ctx.gpr[8] - ctx.gpr[7];
    ctx.gpr[5] = ctx.gpr[5] - object;
    tail_call(ctx, 0x08C75FF8u, {});
}


void func_08C76250(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    ctx.gpr[6] = ctx.gpr[6] - ctx.gpr[5];
    ctx.gpr[8] = ctx.gpr[8] - ctx.gpr[7];
    ctx.gpr[5] = ctx.gpr[5] - object;
    tail_call(ctx, 0x08C75FF8u, {});
}

// 0x08C78724 / 0x08C79814 / 0x08C798E8 / 0x08C79EC4 / 0x08C79F90 (a0 = object): v0 = *(*a0 - 0xC), a0 += v0; the tail call
// goes to the address just past this thunk (0x08C78744 / 0x08C79828 / 0x08C798FC / 0x08C79ED8 / 0x08C79FA4).





// 0x0892A58C (a0 = destination, a1 = source): v1 = *(u16*)a1 is stored at *a0, v0 = a0; tail call 0x0892A454(a0 + 4, *(a1 + 4)).

// 0x08ACC8C0 (a0 = holder, a1/a2 = values): a0 = *a0, a1 = 0, a2 = a1, a3 = a2, t0 = 0; tail call 0x08ACC704.

} // namespace
} // namespace new_batch_main_917
} // namespace gt::game

#define NEW917(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_917::name, gt::ReturnKind::kind)
NEW917(0x0885EA48u, func_0885EA48, Word);
NEW917(0x088E46DCu, func_088E46DC, Word);
NEW917(0x088E4704u, func_088E4704, Word);
NEW917(0x088E43A8u, func_088E43A8, Word);
NEW917(0x088E444Cu, func_088E444C, Word);
NEW917(0x08C76228u, func_08C76228, Word);
NEW917(0x08C76250u, func_08C76250, Word);
