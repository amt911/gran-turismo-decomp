// Hot batch 145: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_145 {
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

// pair_virtual(this, pair *a, pair *b) with the member's vtable slot: a and b copied to the stack (a at +0x10, b at
// +8), then this's virtual at that slot runs with (this, &a copy, &b copy); t2-t5 and v1 hold the words as the
// original leaves them.
void pair_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t table = word(rt, self);
    const std::uint32_t b0 = word(rt, ctx.gpr[6]), a1 = word(rt, ctx.gpr[5] + 4u), a0 = word(rt, ctx.gpr[5]);
    const std::uint32_t b1 = word(rt, ctx.gpr[6] + 4u);
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x14u), a1);
    put(rt, frame.at(0x10u), a0);
    put(rt, frame.at(0xCu), b1);
    put(rt, frame.at(8u), b0);
    ctx.gpr[11] = table;
    ctx.gpr[13] = b0;
    ctx.gpr[3] = a1;
    ctx.gpr[10] = a0;
    ctx.gpr[12] = b1;
    call_set(rt, ctx, word(rt, table + slot), {self, frame.at(0x10u), frame.at(8u)});
}

// destroy_and_delete(this) with the member's vtable, member destructor and its offset, and the next destructor:
// *this = vtable; member(this + offset); next(this); then func_08B64654(this) (tail call: the delete).
void destroy_and_delete(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t member,
                        std::uint32_t offset, std::uint32_t next) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = vtable;
    put(rt, self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        call_set(rt, ctx, member, {self + offset});
        call_set(rt, ctx, next, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// new_handle(out) with the member's size, cleared field and maker: p = func_08B62344(size) with p->field = 0; out = {0,
// 0, byte 0}; maker(out, p, 0); byte +8 of out = 1. Returns out.
void new_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t field,
                std::uint32_t maker) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {size}).v0;
    put(rt, object + field, 0u);
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    rt.memory().store8(out + 8u, 0u);
    call_set(rt, ctx, maker, {out, object, 0u});
    rt.memory().store8(out + 8u, 1u);
    ctx.gpr[2] = out;
}

// facet(this) with the member's key: index = func_08B5C2B0(key); the table *this has (+4 entries, +8 count): entry
// `index` when it is in range and set, else func_08B5A9FC (it does not return: the original has no code after it).
void facet(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t index = call_set(rt, ctx, 0x08B5C2B0u, {key}).v0;
    ctx.gpr[4] = index;
    const std::uint32_t table = word(rt, self);
    const std::uint32_t entries = word(rt, table + 4u);
    ctx.gpr[3] = entries;
    if (index < word(rt, table + 8u)) {
        const std::uint32_t entry = word(rt, entries + index * 4u);
        ctx.gpr[2] = entry;
        if (entry != 0u) return;
    }
    call_set(rt, ctx, 0x08B5A9FCu, {index});
}

} // namespace

// Templates of their families (the members are the batch 145 rows below).
void func_08C59774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 8u); }
void func_08A1A9D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD0698u, 0x08B28DC4u, 104u, 0x08ACFFC4u); }
void func_08B9D000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08B9CF6Cu); }
void func_08C5AB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77478u); }

void func_08C597CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 12u); }
void func_08C59824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 16u); }
void func_08C5987C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 20u); }
void func_08C598D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 24u); }
void func_08C5992C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 28u); }
void func_08C59984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 32u); }
void func_08C599DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 36u); }
void func_08C59A34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 40u); }
void func_08C59A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 44u); }
void func_08C59AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 48u); }
void func_08C5A378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 12u); }
void func_08C5A3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 16u); }
void func_08C5A428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 20u); }
void func_08C5A480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 24u); }
void func_08C5A4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_virtual(rt, ctx, 28u); }
void func_08ABA53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD6018u, 0x08B28BE0u, 20u, 0x08ABB100u); }
void func_08B0DCE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD7C08u, 0x08B0C24Cu, 248u, 0x08B0DDDCu); }
void func_08B15A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD9088u, 0x08B15944u, 248u, 0x08B0DDDCu); }
void func_08B18048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD9270u, 0x08B28BE0u, 6188u, 0x08B24DA4u); }
void func_08B1F8E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD9488u, 0x08B28DC4u, 12u, 0x08B1FD74u); }
void func_08B248BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD9610u, 0x08B28DC4u, 28u, 0x08B24A0Cu); }
void func_08B32F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CDA840u, 0x08B34DA0u, 4u, 0x08B219C0u); }
void func_08BF97DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CCED68u, 0x08A5B9ECu, 28u, 0x089FD82Cu); }
void func_08C1B50C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD1E60u, 0x08B28DC4u, 272u, 0x08C1B490u); }
void func_08C3D680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD6970u, 0x08B28BE0u, 96u, 0x08AC0194u); }
void func_08C3D818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD6A18u, 0x08B28BE0u, 168u, 0x08AC0194u); }
void func_08C3D988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_and_delete(rt, ctx, 0x08CD6AA0u, 0x08B28BE0u, 96u, 0x08AC0194u); }
void func_08B9DEF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08B9DE60u); }
void func_08B9EA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08B9E984u); }
void func_08BC40E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 16u, 8u, 0x08BC4094u); }
void func_08BC4260(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 16u, 8u, 0x08BC420Cu); }
void func_08BC43D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC4384u); }
void func_08BC4550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC44FCu); }
void func_08BC46C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC4674u); }
void func_08BC4840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC47ECu); }
void func_08BC49B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC4964u); }
void func_08BC4B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 8u, 4u, 0x08BC4ADCu); }
void func_08C4D290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_handle(rt, ctx, 16u, 8u, 0x08C4D23Cu); }
void func_08C5ABC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77494u); }
void func_08C5AC20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77490u); }
void func_08C5AC7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77484u); }
void func_08C5ACD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D721C8u); }
void func_08C5AD34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77468u); }
void func_08C5AD90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D7747Cu); }
void func_08C5ADEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77480u); }
void func_08C5AE48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77488u); }
void func_08C5AEA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D7748Cu); }
void func_08C5AF00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77474u); }
void func_08C5AF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D77470u); }
void func_08C5AFB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { facet(rt, ctx, 0x08D7746Cu); }

} // namespace hot_batch_145
} // namespace gt::game

#define HOT145(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_145::name, gt::ReturnKind::kind)
// HOT145(0x08C59774u, func_08C59774, Word);  // no comparable synthetic case yet
HOT145(0x08A1A9D8u, func_08A1A9D8, Word);
HOT145(0x08B9D000u, func_08B9D000, Word);
HOT145(0x08C5AB68u, func_08C5AB68, Word);
// HOT145(0x08C597CCu, func_08C597CC, Word);  // no comparable synthetic case yet
// HOT145(0x08C59824u, func_08C59824, Word);  // no comparable synthetic case yet
// HOT145(0x08C5987Cu, func_08C5987C, Word);  // no comparable synthetic case yet
HOT145(0x08C598D4u, func_08C598D4, Word);
// HOT145(0x08C5992Cu, func_08C5992C, Word);  // no comparable synthetic case yet
// HOT145(0x08C59984u, func_08C59984, Word);  // no comparable synthetic case yet
// HOT145(0x08C599DCu, func_08C599DC, Word);  // no comparable synthetic case yet
// HOT145(0x08C59A34u, func_08C59A34, Word);  // no comparable synthetic case yet
// HOT145(0x08C59A8Cu, func_08C59A8C, Word);  // no comparable synthetic case yet
// HOT145(0x08C59AE4u, func_08C59AE4, Word);  // no comparable synthetic case yet
// HOT145(0x08C5A378u, func_08C5A378, Word);  // no comparable synthetic case yet
// HOT145(0x08C5A3D0u, func_08C5A3D0, Word);  // no comparable synthetic case yet
HOT145(0x08C5A428u, func_08C5A428, Word);
HOT145(0x08C5A480u, func_08C5A480, Word);
HOT145(0x08C5A4D8u, func_08C5A4D8, Word);
HOT145(0x08ABA53Cu, func_08ABA53C, Word);
// HOT145(0x08B0DCE0u, func_08B0DCE0, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT145(0x08B15A2Cu, func_08B15A2C, Word);
HOT145(0x08B18048u, func_08B18048, Word);
// HOT145(0x08B1F8E8u, func_08B1F8E8, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT145(0x08B248BCu, func_08B248BC, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT145(0x08B32F9Cu, func_08B32F9C, Word);
// HOT145(0x08BF97DCu, func_08BF97DC, Word);  // no comparable synthetic case yet
HOT145(0x08C1B50Cu, func_08C1B50C, Word);
HOT145(0x08C3D680u, func_08C3D680, Word);
// HOT145(0x08C3D818u, func_08C3D818, Word);  // no comparable synthetic case yet
// HOT145(0x08C3D988u, func_08C3D988, Word);  // no comparable synthetic case yet
HOT145(0x08B9DEF4u, func_08B9DEF4, Word);
HOT145(0x08B9EA18u, func_08B9EA18, Word);
HOT145(0x08BC40E8u, func_08BC40E8, Word);
HOT145(0x08BC4260u, func_08BC4260, Word);
HOT145(0x08BC43D8u, func_08BC43D8, Word);
HOT145(0x08BC4550u, func_08BC4550, Word);
HOT145(0x08BC46C8u, func_08BC46C8, Word);
HOT145(0x08BC4840u, func_08BC4840, Word);
HOT145(0x08BC49B8u, func_08BC49B8, Word);
HOT145(0x08BC4B30u, func_08BC4B30, Word);
HOT145(0x08C4D290u, func_08C4D290, Word);
HOT145(0x08C5ABC4u, func_08C5ABC4, Word);
HOT145(0x08C5AC20u, func_08C5AC20, Word);
// HOT145(0x08C5AC7Cu, func_08C5AC7C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT145(0x08C5ACD8u, func_08C5ACD8, Word);  // no comparable synthetic case yet
// HOT145(0x08C5AD34u, func_08C5AD34, Word);  // no comparable synthetic case yet
// HOT145(0x08C5AD90u, func_08C5AD90, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT145(0x08C5ADECu, func_08C5ADEC, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT145(0x08C5AE48u, func_08C5AE48, Word);  // no comparable synthetic case yet
HOT145(0x08C5AEA4u, func_08C5AEA4, Word);
// HOT145(0x08C5AF00u, func_08C5AF00, Word);  // no comparable synthetic case yet
HOT145(0x08C5AF5Cu, func_08C5AF5C, Word);
HOT145(0x08C5AFB8u, func_08C5AFB8, Word);
