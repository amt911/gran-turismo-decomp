// One-instruction leaf functions (`jr $ra` + one instruction in its delay slot), one body per shape
// and a table with each instance's constant:
//   return_constant      `addiu $v0, $zero, K`     return K;
//   member_address       `addiu $v0, $a0, K`       return &this->member;
//   set_field            `sw $a1, K($a0)`          this->field = value;
//   byte_getter          `lbu $v0, K($a0)`         return this->byte_field;
//   float_getter         `lwc1 $f0, K($a0)`        return this->float_field;
//   clear_field          `sw $zero, K($a0)`        this->field = 0;
//   clear_byte           `sb $zero, K($a0)`        this->byte_field = 0;
//   set_byte             `sb $a1, K($a0)`          this->byte_field = value;
//   return_pair          `move $v0, $a0; move $v1, $a1` (a 16-byte frame, unused)  return {a0, a1};
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

std::uint32_t leaf_constant(psprecomp::AllegrexContext &ctx) {
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

// set_byte(this, value): this->byte_field = value's low byte.
void set_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + leaf_constant(ctx), static_cast<std::uint8_t>(ctx.gpr[5]));
}
// return_pair(a, b) -> the 64-bit pair (a, b) in v0 / v1.
void return_pair(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    ctx.gpr[3] = ctx.gpr[5];
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
#define GT_DECOMP_SET_BYTE(entry, k) GT_DECOMP_LEAF_(set_byte, gt::ReturnKind::Void, entry, k)
#define GT_DECOMP_RETURN_FIRST_ARG(entry) GT_DECOMP(entry, gt::core::return_first_argument, gt::ReturnKind::Word)
#define GT_DECOMP_RETURN_SECOND_ARG(entry) GT_DECOMP(entry, gt::core::return_second_argument, gt::ReturnKind::Word)
#define GT_DECOMP_RETURN_PAIR(entry) GT_DECOMP(entry, gt::core::return_pair, gt::ReturnKind::DoubleWord)

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
// Batch 143 (2026-10-26): the rest of this family, which no route reaches — verified by synthetic cases.
GT_DECOMP_RETURN_FIRST_ARG(0x08894B60u);
GT_DECOMP_RETURN_FIRST_ARG(0x08894B68u);
GT_DECOMP_RETURN_FIRST_ARG(0x089F9B24u);
GT_DECOMP_RETURN_FIRST_ARG(0x08A5B8B0u);
GT_DECOMP_RETURN_FIRST_ARG(0x08B09658u);
GT_DECOMP_RETURN_FIRST_ARG(0x08BAC1D8u);
GT_DECOMP_RETURN_FIRST_ARG(0x08BAC1E0u);
GT_DECOMP_RETURN_FIRST_ARG(0x08BDCC98u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C45D94u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C45D9Cu);
GT_DECOMP_RETURN_FIRST_ARG(0x08C45DA4u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C467D4u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C4AC68u);
GT_DECOMP_RETURN_FIRST_ARG(0x08AAB540u);

GT_DECOMP_RETURN_SECOND_ARG(0x089304C0u);
// Batch 106: reached by the 11-route gate, each shape matched instruction for instruction.
GT_DECOMP_BYTE_GETTER(0x088CEF48u, 149);
GT_DECOMP_SET_BYTE(0x088CEF50u, 149);
GT_DECOMP_SET_BYTE(0x088CEF58u, 148);
GT_DECOMP_CONST(0x088DA0B4u, 1);
GT_DECOMP_SET_FIELD(0x088DAD00u, 60);
GT_DECOMP_SET_BYTE(0x088DAD9Cu, 68);
GT_DECOMP_SET_FIELD(0x088DADACu, 64);
GT_DECOMP_FLOAT_GETTER(0x08A87824u, 20);
GT_DECOMP_RETURN_FIRST_ARG(0x08BAE28Cu);
GT_DECOMP_RETURN_FIRST_ARG(0x08BBBE8Cu);
GT_DECOMP_RETURN_FIRST_ARG(0x08C45D8Cu);
GT_DECOMP_RETURN_FIRST_ARG(0x08C46D64u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C4ACC0u);
GT_DECOMP_RETURN_FIRST_ARG(0x08C4ACC8u);
// Batch 107: reached by the 11-route gate.
GT_DECOMP_RETURN_PAIR(0x08BAE264u);
GT_DECOMP_RETURN_PAIR(0x08C4AC84u);
GT_DECOMP_RETURN_PAIR(0x08C498A0u);
GT_DECOMP_RETURN_PAIR(0x08C45F58u);
GT_DECOMP_RETURN_PAIR(0x08C45FB8u);
GT_DECOMP_RETURN_PAIR(0x08BDCCB8u);
GT_DECOMP_RETURN_PAIR(0x08C45F30u);
// Batch 137: same five instructions as 0x08C45F58.
GT_DECOMP_RETURN_PAIR(0x08C46034u);
GT_DECOMP_RETURN_PAIR(0x08C46048u);

// Batch 139 (2026-10-25): the rest of these families, which no route reaches — verified by synthetic cases
// (tools.decomp_gate synth); rows from tools.family_rows (each argument mapped from the template's own row). A commented row had no
// comparable synthetic case (every one faulted in the original), so nothing verifies it yet.
GT_DECOMP_SET_FIELD(0x088350E4u, 120);
GT_DECOMP_SET_FIELD(0x0885D478u, 40);
GT_DECOMP_SET_FIELD(0x0887BEF4u, 20);
GT_DECOMP_SET_FIELD(0x088E77A4u, 16);
GT_DECOMP_SET_FIELD(0x089EDA28u, 612);
GT_DECOMP_SET_FIELD(0x08A340CCu, 28);
GT_DECOMP_SET_FIELD(0x08A340D4u, 32);
GT_DECOMP_SET_FIELD(0x08A5F504u, 20);
GT_DECOMP_SET_FIELD(0x08A855D8u, 20);
GT_DECOMP_SET_FIELD(0x08AC2204u, 4);
GT_DECOMP_SET_FIELD(0x08AC220Cu, 24);
GT_DECOMP_SET_FIELD(0x08AC2214u, 28);
GT_DECOMP_SET_FIELD(0x08AC26D4u, 0);
GT_DECOMP_SET_FIELD(0x08B096A4u, 36);
GT_DECOMP_SET_FIELD(0x08B72A1Cu, 1232);

// Batch 151: more members of the 0x08967530 family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_FLOAT_GETTER(0x0891A698u, 12);
GT_DECOMP_FLOAT_GETTER(0x0891A864u, 28);
GT_DECOMP_FLOAT_GETTER(0x0891A874u, 16);
GT_DECOMP_FLOAT_GETTER(0x089299C8u, 0);
GT_DECOMP_FLOAT_GETTER(0x089299D0u, 4);
GT_DECOMP_FLOAT_GETTER(0x0896862Cu, 184);
GT_DECOMP_FLOAT_GETTER(0x08995CB0u, 32);
GT_DECOMP_FLOAT_GETTER(0x0899D0C8u, 328);
GT_DECOMP_FLOAT_GETTER(0x0899D100u, 332);
GT_DECOMP_FLOAT_GETTER(0x0899D164u, 308);
GT_DECOMP_FLOAT_GETTER(0x08A3A320u, 32);

// Batch 151: more members of the 0x088CEF48 family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_BYTE_GETTER(0x08865F44u, 24);
GT_DECOMP_BYTE_GETTER(0x08865F60u, 25);
GT_DECOMP_BYTE_GETTER(0x08866970u, 16332);
GT_DECOMP_BYTE_GETTER(0x0889D2A0u, 2);
GT_DECOMP_BYTE_GETTER(0x088EF910u, 200);
GT_DECOMP_BYTE_GETTER(0x08917890u, 26);
GT_DECOMP_BYTE_GETTER(0x08961474u, 20);
GT_DECOMP_BYTE_GETTER(0x08A3A318u, 36);
GT_DECOMP_BYTE_GETTER(0x08A3A328u, 44);
GT_DECOMP_BYTE_GETTER(0x08B54728u, 36);

// Batch 151: more members of the 0x08946698 family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_CLEAR_BYTE(0x0880DE40u, 8);
GT_DECOMP_CLEAR_BYTE(0x088910B8u, 107);
GT_DECOMP_CLEAR_BYTE(0x088F8938u, 10);
GT_DECOMP_CLEAR_BYTE(0x088FE2ECu, 9);
GT_DECOMP_CLEAR_BYTE(0x0890B3D0u, 228);

// Batch 158: more members of the return_constant family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_CONST(0x08817B08u, 14808);
GT_DECOMP_CONST(0x0886B2C4u, -1);
GT_DECOMP_CONST(0x0886F8C4u, -1);
GT_DECOMP_CONST(0x088A5740u, 432);
GT_DECOMP_CONST(0x088A6C68u, 388);
GT_DECOMP_CONST(0x088A6C70u, 1216);
GT_DECOMP_CONST(0x088D47E4u, 102);
GT_DECOMP_CONST(0x088D47ECu, 288);
GT_DECOMP_CONST(0x088D4808u, 96);
GT_DECOMP_CONST(0x088D699Cu, -1);
GT_DECOMP_CONST(0x088D7F94u, 1);
GT_DECOMP_CONST(0x088F0ED8u, 8);
GT_DECOMP_CONST(0x088F0FF8u, 64);
GT_DECOMP_CONST(0x08932798u, 1);
GT_DECOMP_CONST(0x08A517A4u, -1);
GT_DECOMP_CONST(0x08B089D4u, 428);
GT_DECOMP_CONST(0x08B20824u, 1);
GT_DECOMP_CONST(0x08B55100u, 480);
GT_DECOMP_CONST(0x08B55108u, 272);
GT_DECOMP_CONST(0x08B779E8u, 1);

// Batch 158: more members of the member_address family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_MEMBER_ADDRESS(0x0886A948u, 68);
GT_DECOMP_MEMBER_ADDRESS(0x08875524u, 2068);
GT_DECOMP_MEMBER_ADDRESS(0x0897E2C8u, 480);
GT_DECOMP_MEMBER_ADDRESS(0x08995DA8u, 384);
GT_DECOMP_MEMBER_ADDRESS(0x089DA8F4u, 400);
GT_DECOMP_MEMBER_ADDRESS(0x089DA8FCu, 336);
GT_DECOMP_MEMBER_ADDRESS(0x089DA904u, 368);
GT_DECOMP_MEMBER_ADDRESS(0x089DFA28u, 420);
GT_DECOMP_MEMBER_ADDRESS(0x089DFA30u, 424);
GT_DECOMP_MEMBER_ADDRESS(0x089DFA38u, 428);
GT_DECOMP_MEMBER_ADDRESS(0x089EDABCu, 568);
GT_DECOMP_MEMBER_ADDRESS(0x089EDAC4u, 572);
GT_DECOMP_MEMBER_ADDRESS(0x089F172Cu, 556);
GT_DECOMP_MEMBER_ADDRESS(0x089F1734u, 560);
GT_DECOMP_MEMBER_ADDRESS(0x089F173Cu, 564);
GT_DECOMP_MEMBER_ADDRESS(0x089F1744u, 568);
GT_DECOMP_MEMBER_ADDRESS(0x089F174Cu, 572);
GT_DECOMP_MEMBER_ADDRESS(0x089F1754u, 576);
GT_DECOMP_MEMBER_ADDRESS(0x089F175Cu, 580);
GT_DECOMP_MEMBER_ADDRESS(0x089F1764u, 584);
GT_DECOMP_MEMBER_ADDRESS(0x089FC900u, 100);
GT_DECOMP_MEMBER_ADDRESS(0x08A60BF4u, 20);
GT_DECOMP_MEMBER_ADDRESS(0x08B3F8A0u, 16);
GT_DECOMP_MEMBER_ADDRESS(0x08B3F8A8u, 16);
