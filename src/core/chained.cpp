// Two-step leaf functions (`lw` a pointer member, `jr $ra`, one access through it), one body per
// shape and a table with each instance's two offsets (`outer` in this, `inner` in the pointee):
//   ptr_member_address       return (char *)this->ptr + inner;
//   ptr_field_getter         return this->ptr->field;              (word)
//   ptr_byte_getter          return this->ptr->byte_field;         (unsigned)
//   ptr_signed_byte_getter   return this->ptr->signed_byte_field;
//   ptr_float_getter         return this->ptr->float_field;
//   ptr_set_field            this->ptr->field = value;             (word)
//   ptr_set_byte             this->ptr->byte_field = value;
//   ptr_set_float            this->ptr->float_field = value;       (value in $f12)
//   set_two_fields           this->a = second; this->b = first;    (`sw $a2, outer; sw $a1, inner`)
// Listed: the ones the routes call (the harness compared them equal); the others stay recompiled
// until a run reaches them.

#include "decomp_registry.hpp"

#include <bit>
#include <unordered_map>
#include <utility>

namespace gt::core {
namespace {

std::unordered_map<std::uint32_t, std::pair<std::int32_t, std::int32_t>> &chained_table() {
    static std::unordered_map<std::uint32_t, std::pair<std::int32_t, std::int32_t>> table;
    return table;
}

// this->ptr (the member at `outer`).
std::uint32_t pointer(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return rt.memory().load32(ctx.gpr[4] + static_cast<std::uint32_t>(chained_table().at(ctx.pc).first));
}

// The pointee's address of the inner member: this->ptr + inner.
std::uint32_t inner_address(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return pointer(rt, ctx) + static_cast<std::uint32_t>(chained_table().at(ctx.pc).second);
}

} // namespace

struct ChainedRegistrar {
    ChainedRegistrar(std::uint32_t entry, std::int32_t outer, std::int32_t inner) { chained_table()[entry] = {outer, inner}; }
};

void ptr_member_address(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = inner_address(rt, ctx); }
void ptr_field_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load32(inner_address(rt, ctx)); }
void ptr_byte_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load8(inner_address(rt, ctx)); }
void ptr_signed_byte_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(inner_address(rt, ctx)))));
}
void ptr_float_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = std::bit_cast<float>(rt.memory().load32(inner_address(rt, ctx)));
}
// The setters leave this->ptr in v0, as the originals do (they load it into $v0).
void ptr_set_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(inner_address(rt, ctx), ctx.gpr[5]);
    ctx.gpr[2] = pointer(rt, ctx);
}
void ptr_set_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(inner_address(rt, ctx), static_cast<std::uint8_t>(ctx.gpr[5]));
    ctx.gpr[2] = pointer(rt, ctx);
}
void ptr_set_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(inner_address(rt, ctx), std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = pointer(rt, ctx);
}
void set_two_fields(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto &[outer, inner] = chained_table().at(ctx.pc);
    rt.memory().store32(ctx.gpr[4] + static_cast<std::uint32_t>(outer), ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + static_cast<std::uint32_t>(inner), ctx.gpr[5]);
}

} // namespace gt::core

#define GT_DECOMP_CHAINED_(fn, returns, entry, outer, inner)                                              \
    static const ::gt::core::ChainedRegistrar GT_DECOMP_CAT(gt_chained_, __LINE__){(entry), (outer), (inner)}; \
    GT_DECOMP(entry, gt::core::fn, returns)
#define GT_DECOMP_PTR_MEMBER_ADDRESS(e, o, i) GT_DECOMP_CHAINED_(ptr_member_address, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_FIELD_GETTER(e, o, i) GT_DECOMP_CHAINED_(ptr_field_getter, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_BYTE_GETTER(e, o, i) GT_DECOMP_CHAINED_(ptr_byte_getter, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_SIGNED_BYTE_GETTER(e, o, i) GT_DECOMP_CHAINED_(ptr_signed_byte_getter, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_FLOAT_GETTER(e, o, i) GT_DECOMP_CHAINED_(ptr_float_getter, gt::ReturnKind::Float, e, o, i)
#define GT_DECOMP_PTR_SET_FIELD(e, o, i) GT_DECOMP_CHAINED_(ptr_set_field, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_SET_BYTE(e, o, i) GT_DECOMP_CHAINED_(ptr_set_byte, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_PTR_SET_FLOAT(e, o, i) GT_DECOMP_CHAINED_(ptr_set_float, gt::ReturnKind::Word, e, o, i)
#define GT_DECOMP_SET_TWO_FIELDS(e, o, i) GT_DECOMP_CHAINED_(set_two_fields, gt::ReturnKind::Void, e, o, i)

// 0x0885C380: gt::game::race_operator::get_memory_assign_standard (src/game/race/race_operator.cpp).
// 0x0885C38C: gt::game::race_operator::get_memory_assign_with_ghost.
GT_DECOMP_PTR_MEMBER_ADDRESS(0x0886B438u, 4, 136);
GT_DECOMP_PTR_MEMBER_ADDRESS(0x088723E8u, 92, 8);
GT_DECOMP_PTR_MEMBER_ADDRESS(0x088F4AA0u, 0, 2064);
GT_DECOMP_PTR_MEMBER_ADDRESS(0x0892BB10u, 608, 4);
GT_DECOMP_PTR_MEMBER_ADDRESS(0x08AF296Cu, 0, 64);
GT_DECOMP_PTR_MEMBER_ADDRESS(0x08B0980Cu, 0, 256);

GT_DECOMP_PTR_SET_BYTE(0x08896FD0u, 0, 141);
GT_DECOMP_PTR_SET_BYTE(0x08896FECu, 0, 142);
GT_DECOMP_PTR_SET_BYTE(0x08897008u, 0, 143);
GT_DECOMP_PTR_SET_BYTE(0x08897024u, 0, 148);
GT_DECOMP_PTR_SET_BYTE(0x08897054u, 0, 146);

GT_DECOMP_PTR_SET_FLOAT(0x08AB8738u, 0, 600);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8768u, 0, 608);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8784u, 0, 592);
GT_DECOMP_PTR_SET_FLOAT(0x08AB879Cu, 0, 596);
GT_DECOMP_PTR_SET_FLOAT(0x08AB87B4u, 0, 612);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8810u, 0, 652);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8828u, 0, 656);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8840u, 0, 664);
GT_DECOMP_PTR_SET_FLOAT(0x08AB8970u, 0, 580);

GT_DECOMP_PTR_SET_FIELD(0x0886AF24u, 4, 52);
GT_DECOMP_PTR_SET_FIELD(0x0886AF3Cu, 4, 68);
GT_DECOMP_PTR_SET_FIELD(0x0886AF48u, 4, 128);
GT_DECOMP_PTR_SET_FIELD(0x0886B074u, 4, 124);
GT_DECOMP_PTR_SET_FIELD(0x0886B098u, 4, 108);
GT_DECOMP_PTR_SET_FIELD(0x0887ACD0u, 4, 8);
GT_DECOMP_PTR_SET_FIELD(0x08AF11A0u, 4, 800);

GT_DECOMP_PTR_BYTE_GETTER(0x08ACB1BCu, 0, 140);

GT_DECOMP_PTR_FIELD_GETTER(0x08A189A8u, 96, 244);
GT_DECOMP_PTR_FIELD_GETTER(0x08A189B4u, 96, 240);
GT_DECOMP_PTR_FIELD_GETTER(0x08A7BDB0u, 24, 4);

GT_DECOMP_PTR_SIGNED_BYTE_GETTER(0x08897048u, 0, 146);

GT_DECOMP_SET_TWO_FIELDS(0x088A1408u, 40, 36);
GT_DECOMP_SET_TWO_FIELDS(0x088AF2A4u, 28, 24);
GT_DECOMP_SET_TWO_FIELDS(0x088D2A68u, 4, 0);
GT_DECOMP_SET_TWO_FIELDS(0x088DA770u, 292, 280);
GT_DECOMP_SET_TWO_FIELDS(0x088DA77Cu, 296, 284);
GT_DECOMP_SET_TWO_FIELDS(0x088E4114u, 4, 0);
GT_DECOMP_SET_TWO_FIELDS(0x08929EF8u, 4, 0);
GT_DECOMP_SET_TWO_FIELDS(0x08A128E0u, 16, 12);
GT_DECOMP_SET_TWO_FIELDS(0x08B2095Cu, 4, 0);
GT_DECOMP_SET_TWO_FIELDS(0x08B218D4u, 4, 0);
