// One-instruction leaf functions (`jr $ra` + one instruction in its delay slot), one body per shape
// and a table with each instance's constant:
//   return_constant      `addiu $v0, $zero, K`     return K;
//   member_address       `addiu $v0, $a0, K`       return &this->member;
//   set_field            `sw $a1, K($a0)`          this->field = value;
//   byte_getter          `lbu $v0, K($a0)`         return this->byte_field;
//   float_getter         `lwc1 $f0, K($a0)`        return this->float_field;
//   clear_field          `sw $zero, K($a0)`        this->field = 0;
//   clear_byte           `sb $zero, K($a0)`        this->byte_field = 0;
//   return_argument      `move $v0, $a0` / `$a1`   return the first / second argument;
// Listed: the ones the routes call (the harness compared them equal); the others stay recompiled
// until a run reaches them.

#include "decomp_registry.hpp"

#include <bit>
#include <unordered_map>

namespace gt::core {
namespace {

std::unordered_map<std::uint32_t, std::int32_t> &leaf_table() {
    static std::unordered_map<std::uint32_t, std::int32_t> table;
    return table;
}

std::uint32_t leaf_constant(const psprecomp::AllegrexContext &ctx) {
    return static_cast<std::uint32_t>(leaf_table().at(ctx.pc));
}

} // namespace

struct LeafRegistrar {
    LeafRegistrar(std::uint32_t entry, std::int32_t constant) { leaf_table()[entry] = constant; }
};

void return_constant(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = leaf_constant(ctx); }

void member_address(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + leaf_constant(ctx); }

void set_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + leaf_constant(ctx), ctx.gpr[5]);
}

void byte_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + leaf_constant(ctx));
}

void float_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = std::bit_cast<float>(rt.memory().load32(ctx.gpr[4] + leaf_constant(ctx)));
}

void clear_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + leaf_constant(ctx), 0u);
}

void clear_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + leaf_constant(ctx), 0u);
}

void return_first_argument(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; }

void return_second_argument(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[5]; }

} // namespace gt::core

#define GT_DECOMP_LEAF_(fn, returns, entry, constant)                                                    \
    static const ::gt::core::LeafRegistrar GT_DECOMP_CAT(gt_leaf_, __LINE__){(entry), (constant)};       \
    GT_DECOMP(entry, gt::core::fn, returns)
#define GT_DECOMP_CONST(entry, k) GT_DECOMP_LEAF_(return_constant, gt::ReturnKind::Word, entry, k)
#define GT_DECOMP_MEMBER_ADDRESS(entry, k) GT_DECOMP_LEAF_(member_address, gt::ReturnKind::Word, entry, k)
#define GT_DECOMP_SET_FIELD(entry, k) GT_DECOMP_LEAF_(set_field, gt::ReturnKind::Void, entry, k)
#define GT_DECOMP_BYTE_GETTER(entry, k) GT_DECOMP_LEAF_(byte_getter, gt::ReturnKind::Word, entry, k)
#define GT_DECOMP_FLOAT_GETTER(entry, k) GT_DECOMP_LEAF_(float_getter, gt::ReturnKind::Float, entry, k)
#define GT_DECOMP_CLEAR_FIELD(entry, k) GT_DECOMP_LEAF_(clear_field, gt::ReturnKind::Void, entry, k)
#define GT_DECOMP_CLEAR_BYTE(entry, k) GT_DECOMP_LEAF_(clear_byte, gt::ReturnKind::Void, entry, k)
#define GT_DECOMP_RETURN_FIRST_ARG(entry) GT_DECOMP(entry, gt::core::return_first_argument, gt::ReturnKind::Word)
#define GT_DECOMP_RETURN_SECOND_ARG(entry) GT_DECOMP(entry, gt::core::return_second_argument, gt::ReturnKind::Word)

GT_DECOMP_CONST(0x088666E8u, 48);
GT_DECOMP_CONST(0x088975C8u, 192);
GT_DECOMP_CONST(0x088B1340u, 4);
GT_DECOMP_CONST(0x08AC329Cu, 280);
GT_DECOMP_CONST(0x08B48BE4u, 1);

GT_DECOMP_MEMBER_ADDRESS(0x089A8A68u, 160);
GT_DECOMP_MEMBER_ADDRESS(0x089CBF68u, 12);
GT_DECOMP_MEMBER_ADDRESS(0x089CBF70u, 16);
GT_DECOMP_MEMBER_ADDRESS(0x08A50F48u, 20);
GT_DECOMP_MEMBER_ADDRESS(0x08A5D444u, 12);
GT_DECOMP_MEMBER_ADDRESS(0x08A615C4u, 76);
GT_DECOMP_MEMBER_ADDRESS(0x08A6BF80u, 20);
GT_DECOMP_MEMBER_ADDRESS(0x08A7BCC0u, 24);
GT_DECOMP_MEMBER_ADDRESS(0x08B2A3B8u, 8);
GT_DECOMP_MEMBER_ADDRESS(0x08B2E134u, -63);

GT_DECOMP_SET_FIELD(0x0889867Cu, 60);
GT_DECOMP_SET_FIELD(0x08898684u, 64);
GT_DECOMP_SET_FIELD(0x088D2BC0u, 0);
GT_DECOMP_SET_FIELD(0x088DAF70u, 332);
GT_DECOMP_SET_FIELD(0x088DAF78u, 1492);
GT_DECOMP_SET_FIELD(0x088DAF80u, 1488);
GT_DECOMP_SET_FIELD(0x088DB5CCu, 1484);
GT_DECOMP_SET_FIELD(0x08925B68u, 12);
GT_DECOMP_SET_FIELD(0x08973B8Cu, 24);
GT_DECOMP_SET_FIELD(0x089A8990u, 100);
GT_DECOMP_SET_FIELD(0x089A8998u, 292);
GT_DECOMP_SET_FIELD(0x08A17B28u, 0);
GT_DECOMP_SET_FIELD(0x08A3A340u, 28);
GT_DECOMP_SET_FIELD(0x08A615D4u, 80);
GT_DECOMP_SET_FIELD(0x08AC26A4u, 0);
GT_DECOMP_SET_FIELD(0x08B5C14Cu, 0);

GT_DECOMP_BYTE_GETTER(0x088D9E58u, 30);
GT_DECOMP_BYTE_GETTER(0x088DAA84u, 28);
GT_DECOMP_BYTE_GETTER(0x0892B768u, 4);
GT_DECOMP_BYTE_GETTER(0x08988F1Cu, 490);
GT_DECOMP_BYTE_GETTER(0x089A8EA8u, 96);
GT_DECOMP_BYTE_GETTER(0x08B51F74u, 36);
GT_DECOMP_BYTE_GETTER(0x08C19DFCu, 16);

GT_DECOMP_FLOAT_GETTER(0x08967530u, 200);
GT_DECOMP_FLOAT_GETTER(0x0899D064u, 288);
GT_DECOMP_FLOAT_GETTER(0x0899D0A8u, 292);
GT_DECOMP_FLOAT_GETTER(0x0899D144u, 296);
GT_DECOMP_FLOAT_GETTER(0x089A8E8Cu, 272);

GT_DECOMP_CLEAR_FIELD(0x088A3F90u, 784);
GT_DECOMP_CLEAR_FIELD(0x0890B3D8u, 92);

GT_DECOMP_CLEAR_BYTE(0x08946698u, 0);

GT_DECOMP_RETURN_FIRST_ARG(0x089304C8u);
GT_DECOMP_RETURN_FIRST_ARG(0x08AAB540u);

GT_DECOMP_RETURN_SECOND_ARG(0x089304C0u);
