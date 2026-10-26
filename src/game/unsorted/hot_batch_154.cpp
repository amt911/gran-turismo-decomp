// Hot batch 154: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_154 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A pointer to member function as this compiler lays it out (hot_batch_65): {function or vtable offset,
// adjustment * 2 | virtual flag}. The function it names for the object at `base` + adjustment.
std::uint32_t member_target(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t function, std::uint32_t tagged) {
    return (tagged & 1u) != 0u ? word(rt, word(rt, object) + function) : function;
}

// ptmf_chain(this, object, flag) with the member's two pointer-to-member offsets: r = (object->*first)(object, ...);
// then (r->*second)(r, flag & 0xFF) (tail call).
void ptmf_chain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t tagged = word(rt, self + first + 4u);
    const std::uint32_t function = word(rt, self + first);
    const std::uint32_t object = ctx.gpr[5] + static_cast<std::uint32_t>(static_cast<std::int32_t>(tagged) >> 1);
    const std::uint32_t target = member_target(rt, object, function, tagged);
    ctx.gpr[3] = tagged;
    ctx.gpr[2] = target;
    ctx.gpr[4] = object;
    ctx.gpr[6] = function;
    const GuestResult result = call_indirect(rt, ctx, target, {object, ctx.gpr[5], function}, {});
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    const std::uint32_t second_tagged = word(rt, self + second + 4u);
    const std::uint32_t second_function = word(rt, self + second);
    const std::uint32_t adjustment = static_cast<std::uint32_t>(static_cast<std::int32_t>(second_tagged) >> 1);
    const std::uint32_t receiver = result.v0 + adjustment;
    const std::uint32_t next = member_target(rt, receiver, second_function, second_tagged);
    if ((second_tagged & 1u) != 0u) {
        ctx.gpr[3] = second_function;
        ctx.gpr[2] = word(rt, receiver) + second_function;
    } else {
        ctx.gpr[3] = adjustment;
        ctx.gpr[2] = 0u;
    }
    ctx.gpr[25] = next;
    tail_call(ctx, next, {receiver, flag});
}

// guarded_init() with the member's guard, instance, factory and table: once (the guard byte clear and
// func_08B6441C(&guard) granting it): o = instance(), o->+8 = table, o->+0x2C = factory(), o->+0x30 = 0,
// func_08B18DFC(instance()), then func_08B64494(&guard) (tail call).
void guarded_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t instance,
                  std::uint32_t factory, std::uint32_t table) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int8_t>(rt.memory().load8(guard)));
    ctx.gpr[2] = guard & 0xFFFF0000u;
    ctx.gpr[4] = guard;
    if (ctx.gpr[3] != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}).v0 == 0u) return;
    const std::uint32_t object = call_set(rt, ctx, instance, {}).v0;
    const std::uint32_t made = call_set(rt, ctx, factory, {}).v0;
    ctx.gpr[3] = table;
    put(rt, object + 8u, table);
    put(rt, object + 0x2Cu, made);
    put(rt, object + 0x30u, 0u);
    const std::uint32_t again = call_set(rt, ctx, instance, {}).v0;
    call_set(rt, ctx, 0x08B18DFCu, {again});
    tail_call(ctx, 0x08B64494u, {guard});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 154 rows below).
void func_08BEB47C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08B9D41C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D75700u, 0x08B9D2BCu, 0x08B9ADA8u, 0x08CA0110u); }

// Batch 154 rows (tools.family_rows).
void func_08B8CC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BEE1E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BF2BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BF40F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BF67AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BFC02C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BFDBD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08BFF4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C004AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C03FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C05D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C07AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C0A5A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C0BD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C15454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C1D100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08C1E4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain(rt, ctx, 0x14u, 0x24u); }
void func_08B9D1F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D75688u, 0x08B9D090u, 0x08B9BDE0u, 0x08CA0104u); }
void func_08B9D648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D75698u, 0x08B9D4E8u, 0x08B9BED0u, 0x08CA0118u); }
void func_08B9D88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D756A0u, 0x08B9D714u, 0x08B9BED0u, 0x08CA0124u); }
void func_08B9DAB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D756A8u, 0x08B9D958u, 0x08B9BED0u, 0x08CA012Cu); }
void func_08B9DCE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D756B0u, 0x08B9DB84u, 0x08B9BED0u, 0x08CA0130u); }
void func_08B9E328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D75690u, 0x08B9E208u, 0x08B9BED0u, 0x08CA0134u); }
void func_08B9E5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D756B8u, 0x08B9E47Cu, 0x08B9BED0u, 0x08CA015Cu); }
void func_08B9E808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D756C0u, 0x08B9E6A8u, 0x08B9BED0u, 0x08CA0164u); }
void func_08BB97B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D76040u, 0x08BB964Cu, 0x08BB9194u, 0x08CA0B94u); }
void func_08BB99C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D76038u, 0x08BB9860u, 0x08BB9194u, 0x08CA0BA4u); }
void func_08BC5250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D76290u, 0x08BC50ACu, 0x08BBD42Cu, 0x08CA0DF4u); }
void func_08BC5AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D762A0u, 0x08BC593Cu, 0x08BBD42Cu, 0x08CA0E50u); }
void func_08BC5F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D762A8u, 0x08BC5DA8u, 0x08BBD42Cu, 0x08CA0E70u); }
void func_08BC61BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_init(rt, ctx, 0x08D76298u, 0x08BC6018u, 0x08BBD42Cu, 0x08CA0E80u); }

} // namespace hot_batch_154
} // namespace gt::game

#define HOT154(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_154::name, gt::ReturnKind::kind)
HOT154(0x08BEB47Cu, func_08BEB47C, Word);
HOT154(0x08B9D41Cu, func_08B9D41C, Word);
HOT154(0x08B8CC60u, func_08B8CC60, Word);
HOT154(0x08BEE1E4u, func_08BEE1E4, Word);
HOT154(0x08BF2BC0u, func_08BF2BC0, Word);
HOT154(0x08BF40F0u, func_08BF40F0, Word);
HOT154(0x08BF67ACu, func_08BF67AC, Word);
HOT154(0x08BFC02Cu, func_08BFC02C, Word);
HOT154(0x08BFDBD4u, func_08BFDBD4, Word);
HOT154(0x08BFF4E0u, func_08BFF4E0, Word);
HOT154(0x08C004ACu, func_08C004AC, Word);
HOT154(0x08C03FE4u, func_08C03FE4, Word);
HOT154(0x08C05D14u, func_08C05D14, Word);
HOT154(0x08C07AB8u, func_08C07AB8, Word);
HOT154(0x08C0A5A8u, func_08C0A5A8, Word);
HOT154(0x08C0BD68u, func_08C0BD68, Word);
HOT154(0x08C15454u, func_08C15454, Word);
HOT154(0x08C1D100u, func_08C1D100, Word);
HOT154(0x08C1E4FCu, func_08C1E4FC, Word);
HOT154(0x08B9D1F0u, func_08B9D1F0, Word);
HOT154(0x08B9D648u, func_08B9D648, Word);
HOT154(0x08B9D88Cu, func_08B9D88C, Word);
HOT154(0x08B9DAB8u, func_08B9DAB8, Word);
HOT154(0x08B9DCE4u, func_08B9DCE4, Word);
HOT154(0x08B9E328u, func_08B9E328, Word);
HOT154(0x08B9E5DCu, func_08B9E5DC, Word);
HOT154(0x08B9E808u, func_08B9E808, Word);
HOT154(0x08BB97B0u, func_08BB97B0, Word);
HOT154(0x08BB99C4u, func_08BB99C4, Word);
HOT154(0x08BC5250u, func_08BC5250, Word);
HOT154(0x08BC5AE0u, func_08BC5AE0, Word);
HOT154(0x08BC5F4Cu, func_08BC5F4C, Word);
HOT154(0x08BC61BCu, func_08BC61BC, Word);
