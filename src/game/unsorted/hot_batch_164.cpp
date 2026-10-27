// Hot batch 164: small shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every
// member of their family (rows from tools.family_rows), verified by synthetic cases; written by hand from
// `tools.disasm`; role words describe access shapes, none is a game name. Each family's own template is a named
// function elsewhere, so a pending member is the template here.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_164 {
namespace {

using namespace gt::game::hot;

// two_constants_tail(this, ...) with the member's target and constants: target(this, first, second) (tail call).
void two_constants_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t first,
                        std::uint32_t second) {
    tail_call(ctx, target, {ctx.gpr[4], first, second});
}

// deref_virtual_tail(this, ...) with the member's slot: o = *this; o's virtual at that slot (o, ...) as a tail call
// (v0 = o's vtable, t9 = the target).
void deref_virtual_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + slot);
    tail_call(ctx, ctx.gpr[25], {object});
}

// global_first_tail(x, ...) with the member's global and target: target(global, x) (tail call).
void global_first_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t global, std::uint32_t target) {
    tail_call(ctx, target, {global, ctx.gpr[4]});
}

// table_entry(this, i) with the member's offsets: the word i of the array at this->[table] + base (a1 = its address
// less base).
void table_entry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t table, std::uint32_t base) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + table);
    ctx.gpr[5] = (ctx.gpr[5] << 2) + ctx.gpr[2];
    ctx.gpr[2] = word(rt, ctx.gpr[5] + base);
}

// deref_store_two(this, a, b) with the member's offsets: (*this)->[first] = a, (*this)->[second] = b (v0 / v1 = *this).
void deref_store_two(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    put(rt, ctx.gpr[2] + first, ctx.gpr[5]);
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    put(rt, ctx.gpr[3] + second, ctx.gpr[6]);
}

// store_two_late(this, _, a, b) with the member's offsets: this->[first] = a, this->[second] = b.
void store_two_late(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    put(rt, ctx.gpr[4] + first, ctx.gpr[6]);
    put(rt, ctx.gpr[4] + second, ctx.gpr[7]);
}

// byte_arg_tail(this, x, ...) with the member's target: target(this, x & 0xFF, ...) (tail call).
void byte_arg_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    tail_call(ctx, target, {ctx.gpr[4], ctx.gpr[5] & 0xFFu});
}

// adjust_tail(this, ...) with the member's adjustment and target: target(this + adjustment, ...) (tail call).
void adjust_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t adjustment, std::uint32_t target) {
    tail_call(ctx, target, {ctx.gpr[4] + adjustment});
}

// relative_tail(this, x, y, z, w) with the member's target: target(this, x - *this, y - x, z, w - z) (tail call; v0 =
// *this).
void relative_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    ctx.gpr[6] = ctx.gpr[6] - ctx.gpr[5];
    ctx.gpr[8] = ctx.gpr[8] - ctx.gpr[7];
    ctx.gpr[5] = ctx.gpr[5] - ctx.gpr[2];
    tail_call(ctx, target, {});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 164 rows below).
void func_0888A86C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B36018u, 0u, 0xA8u); }
void func_0885E764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x24u); }
void func_0892C880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08CF2960u, 0x08B20968u); }
void func_0886AF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry(rt, ctx, 4u, 0x10u); }
void func_088E4B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_two(rt, ctx, 0x4F4u, 0x4F8u); }
void func_08A82C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_two_late(rt, ctx, 0x18u, 0x1Cu); }
void func_08C31314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_arg_tail(rt, ctx, 0x08AA199Cu); }
void func_08C55484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_tail(rt, ctx, 0xFFFFFFF8u, 0x08C55490u); }
void func_08C76228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relative_tail(rt, ctx, 0x08C75FF8u); }

// Batch 164 rows (tools.family_rows).
void func_088B5D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B36018u, 0u, 0x18Cu); }
void func_08985DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B7DF7Cu, 0u, 0xAu); }
void func_08985DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B7E4CCu, 0u, 0xAu); }
void func_08985DBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B7DFC8u, 0u, 0xAu); }
void func_08985DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B7E518u, 0u, 0xAu); }
void func_08B21710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B36018u, 0u, 0x82u); }
void func_08B217A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B36018u, 0u, 0x24u); }
void func_08B47AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_constants_tail(rt, ctx, 0x08B36018u, 0u, 0x530u); }
void func_0886006C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x20u); }
void func_088E46DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x80u); }
void func_088E4704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x88u); }
void func_088E4878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x70u); }
void func_088E4B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_virtual_tail(rt, ctx, 0x60u); }
void func_0892C964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08CF2950u, 0x08B20968u); }
void func_08B0A988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08D4734Cu, 0x08B0A910u); }
void func_08B0AB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08D4734Cu, 0x08B0AA7Cu); }
void func_08B18A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08D688B0u, 0x08B20968u); }
void func_08B18A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_first_tail(rt, ctx, 0x08D688A8u, 0x08B20968u); }
void func_0886B010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry(rt, ctx, 4u, 0x48u); }
void func_0886B038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry(rt, ctx, 4u, 0x50u); }
void func_0888EBE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry(rt, ctx, 0u, 0x1A0u); }
void func_088E4718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_two(rt, ctx, 0x4E0u, 0x4E4u); }
void func_088E4DD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_two(rt, ctx, 0x1F0u, 0x1F4u); }
void func_088E4DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_two(rt, ctx, 0x1F8u, 0x1FCu); }
void func_08A5D8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_two_late(rt, ctx, 0x18u, 0x1Cu); }
void func_08A8A9BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_two_late(rt, ctx, 0x18u, 0x1Cu); }
void func_08B1AFF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_two_late(rt, ctx, 0x30u, 0x34u); }
void func_08C312F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_arg_tail(rt, ctx, 0x08AA18F0u); }
void func_08C55410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_tail(rt, ctx, 0xFFFFFFF8u, 0x08C5541Cu); }
void func_08C78738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_tail(rt, ctx, 0xFFFFFFF8u, 0x08C78744u); }
void func_08C7A724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_tail(rt, ctx, 0xFFFFFFF8u, 0x08C7A730u); }
void func_08C76214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relative_tail(rt, ctx, 0x08C75FF8u); }
void func_08C7623C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relative_tail(rt, ctx, 0x08C75FF8u); }
void func_08C76250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relative_tail(rt, ctx, 0x08C75FF8u); }

} // namespace hot_batch_164
} // namespace gt::game

#define HOT164(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_164::name, gt::ReturnKind::kind)
HOT164(0x0888A86Cu, func_0888A86C, Word);
HOT164(0x0885E764u, func_0885E764, Word);
HOT164(0x0892C880u, func_0892C880, Word);
HOT164(0x0886AF70u, func_0886AF70, Word);
HOT164(0x088E4B34u, func_088E4B34, Word);
HOT164(0x08A82C24u, func_08A82C24, Word);
HOT164(0x08C31314u, func_08C31314, Word);
HOT164(0x08C55484u, func_08C55484, Word);
// HOT164(0x08C76228u, func_08C76228, Word);  // no comparable synthetic case yet
HOT164(0x088B5D24u, func_088B5D24, Word);
HOT164(0x08985DA4u, func_08985DA4, Word);
HOT164(0x08985DB0u, func_08985DB0, Word);
HOT164(0x08985DBCu, func_08985DBC, Word);
HOT164(0x08985DC8u, func_08985DC8, Word);
HOT164(0x08B21710u, func_08B21710, Word);
HOT164(0x08B217A0u, func_08B217A0, Word);
HOT164(0x08B47AB8u, func_08B47AB8, Word);
// HOT164(0x0886006Cu, func_0886006C, Word);  // no comparable synthetic case yet
// HOT164(0x088E46DCu, func_088E46DC, Word);  // no comparable synthetic case yet
// HOT164(0x088E4704u, func_088E4704, Word);  // no comparable synthetic case yet
HOT164(0x088E4878u, func_088E4878, Word);
HOT164(0x088E4B6Cu, func_088E4B6C, Word);
HOT164(0x0892C964u, func_0892C964, Word);
HOT164(0x08B0A988u, func_08B0A988, Word);
HOT164(0x08B0AB0Cu, func_08B0AB0C, Word);
HOT164(0x08B18A08u, func_08B18A08, Word);
HOT164(0x08B18A18u, func_08B18A18, Word);
HOT164(0x0886B010u, func_0886B010, Word);
HOT164(0x0886B038u, func_0886B038, Word);
HOT164(0x0888EBE0u, func_0888EBE0, Word);
HOT164(0x088E4718u, func_088E4718, Word);
HOT164(0x088E4DD4u, func_088E4DD4, Word);
HOT164(0x088E4DE8u, func_088E4DE8, Word);
HOT164(0x08A5D8D4u, func_08A5D8D4, Word);
HOT164(0x08A8A9BCu, func_08A8A9BC, Word);
HOT164(0x08B1AFF4u, func_08B1AFF4, Word);
HOT164(0x08C312F4u, func_08C312F4, Word);
HOT164(0x08C55410u, func_08C55410, Word);
// HOT164(0x08C78738u, func_08C78738, Word);  // no comparable synthetic case yet
// HOT164(0x08C7A724u, func_08C7A724, Word);  // no comparable synthetic case yet
// HOT164(0x08C76214u, func_08C76214, Word);  // no comparable synthetic case yet
// HOT164(0x08C7623Cu, func_08C7623C, Word);  // no comparable synthetic case yet
// HOT164(0x08C76250u, func_08C76250, Word);  // no comparable synthetic case yet
