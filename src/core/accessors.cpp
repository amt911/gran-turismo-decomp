// One-line member accessors the compiler emits many times over, one body per shape and a table
// per shape with each instance's constant (its field offset, its target):
//   field_getter   `jr $ra; lw $v0, off($a0)`            return this->field;
//   return_zero    `jr $ra; move $v0, $zero`              return 0;
//   member_forward `j target; lw $a0, off($a0)`           return target(this->member, ...);
//   deref_getter   `lw $v1, 0($a0); jr $ra; lw $v0, off($v1)`  return (*this)->field;
//   field_plus     `lw $v0, off($a0); jr $ra; addiu $v0, $v0, k`  return this->field + k;
//   deref_float    `lw $v0, off($a0); jr $ra; lwc1 $f0, k($v0)`   return this->ptr->float_field;
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

struct Accessor {
    std::int32_t offset;
    std::uint32_t target;
};

std::unordered_map<std::uint32_t, Accessor> &accessor_table() {
    static std::unordered_map<std::uint32_t, Accessor> table;
    return table;
}

const Accessor &accessor(psprecomp::AllegrexContext &ctx) { return accessor_table().at(ctx.pc); }

} // namespace

struct AccessorRegistrar {
    AccessorRegistrar(std::uint32_t entry, std::int32_t offset, std::uint32_t target) {
        accessor_table()[entry] = {offset, target};
    }
};

// field_getter(this) -> this->field.
void field_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load32(ctx.gpr[4] + static_cast<std::uint32_t>(accessor(ctx).offset));
}

// return_zero() -> 0.
void return_zero(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0u; }

// member_forward(this, ...): continue in the target with this->member in place of this.
void member_forward(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Accessor &a = accessor(ctx);
    const std::uint32_t member = rt.memory().load32(ctx.gpr[4] + static_cast<std::uint32_t>(a.offset));
    tail_call(ctx, a.target, {member});
}

// deref_getter(this) -> (*this)->field.
void deref_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t inner = rt.memory().load32(ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load32(inner + static_cast<std::uint32_t>(accessor(ctx).offset));
}

// field_plus(this) -> this->field + k (the target slot holds k).
void field_plus(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Accessor &a = accessor(ctx);
    ctx.gpr[2] = rt.memory().load32(ctx.gpr[4] + static_cast<std::uint32_t>(a.offset)) + a.target;
}
// deref_float(this) -> this->ptr->float_field (v0 keeps the pointer; the target slot holds the float's offset).
void deref_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Accessor &a = accessor(ctx);
    ctx.gpr[2] = rt.memory().load32(ctx.gpr[4] + static_cast<std::uint32_t>(a.offset));
    ctx.set_fpr_bits(0u, rt.memory().load32(ctx.gpr[2] + a.target));
}
} // namespace gt::core

#define GT_DECOMP_ACCESSOR_(kind, fn, returns, entry, offset, target)                                    \
    static const ::gt::core::AccessorRegistrar GT_DECOMP_CAT(gt_accessor_, __LINE__){(entry), (offset), (target)}; \
    GT_DECOMP(entry, gt::core::fn, returns)
#define GT_DECOMP_FIELD(entry, offset) GT_DECOMP_ACCESSOR_(field, field_getter, gt::ReturnKind::Word, entry, offset, 0u)
#define GT_DECOMP_ZERO(entry) GT_DECOMP(entry, gt::core::return_zero, gt::ReturnKind::Word)
#define GT_DECOMP_FORWARD(entry, offset, target) \
    GT_DECOMP_ACCESSOR_(forward, member_forward, gt::ReturnKind::Word, entry, offset, target)
#define GT_DECOMP_DEREF(entry, offset) GT_DECOMP_ACCESSOR_(deref, deref_getter, gt::ReturnKind::Word, entry, offset, 0u)
#define GT_DECOMP_FIELD_PLUS(entry, offset, k) GT_DECOMP_ACCESSOR_(plus, field_plus, gt::ReturnKind::Word, entry, offset, k)
#define GT_DECOMP_DEREF_FLOAT(entry, offset, k) \
    GT_DECOMP_ACCESSOR_(dflt, deref_float, gt::ReturnKind::Float, entry, offset, k)

GT_DECOMP_FIELD(0x08817A54u, 20);
GT_DECOMP_FIELD(0x08817DC8u, 20);
GT_DECOMP_FIELD(0x0881B068u, 20);
GT_DECOMP_FIELD(0x0881BBF4u, 20);
GT_DECOMP_FIELD(0x088294D0u, 24);
GT_DECOMP_FIELD(0x08844634u, 20);
GT_DECOMP_FIELD(0x08845798u, 20);
GT_DECOMP_FIELD(0x08846E50u, 20);
GT_DECOMP_FIELD(0x0884D3D8u, 20);
GT_DECOMP_FIELD(0x08854678u, 20);
GT_DECOMP_FIELD(0x088546F0u, 20);
GT_DECOMP_FIELD(0x08860050u, 0);
GT_DECOMP_FIELD(0x088703ACu, 12);
GT_DECOMP_FIELD(0x0888F548u, 0);
GT_DECOMP_FIELD(0x088940ECu, 0);
GT_DECOMP_FIELD(0x08896FB8u, 0);
GT_DECOMP_FIELD(0x088D9F80u, 1436);
GT_DECOMP_FIELD(0x088DA160u, 1444);
GT_DECOMP_FIELD(0x088DA168u, 1440);
GT_DECOMP_FIELD(0x088E7F14u, 696);
GT_DECOMP_FIELD(0x08917840u, 16);
GT_DECOMP_FIELD(0x08917938u, 20);
GT_DECOMP_FIELD(0x08927120u, 8100);
GT_DECOMP_FIELD(0x089275A8u, 8112);
GT_DECOMP_FIELD(0x089277B4u, 8100);
GT_DECOMP_FIELD(0x0892B7E0u, 16);
GT_DECOMP_FIELD(0x0894D048u, 220);
GT_DECOMP_FIELD(0x08958D40u, 36);
GT_DECOMP_FIELD(0x08965BCCu, 80);
GT_DECOMP_FIELD(0x08965BE0u, 76);
GT_DECOMP_FIELD(0x08965CE0u, 76);
GT_DECOMP_FIELD(0x08968A38u, 84);
GT_DECOMP_FIELD(0x08968E68u, 84);
GT_DECOMP_FIELD(0x08977260u, 384);
GT_DECOMP_FIELD(0x089A8988u, 100);
GT_DECOMP_FIELD(0x089A91CCu, 292);
GT_DECOMP_FIELD(0x089C4FD4u, 56);
GT_DECOMP_FIELD(0x089CCA54u, 408);
GT_DECOMP_FIELD(0x08A197F0u, 12);
GT_DECOMP_FIELD(0x08A1B3CCu, 20);
GT_DECOMP_FIELD(0x08A1CB14u, 20);
GT_DECOMP_FIELD(0x08A61624u, 48);
GT_DECOMP_FIELD(0x08A6BFFCu, 20);
GT_DECOMP_FIELD(0x08A855E8u, 20);
GT_DECOMP_FIELD(0x08AB8594u, 4);
GT_DECOMP_FIELD(0x08ABF868u, 4);
GT_DECOMP_FIELD(0x08BAE240u, 0);
GT_DECOMP_FIELD(0x08BAE248u, 0);
GT_DECOMP_FIELD(0x08C45D84u, 0);

GT_DECOMP_ZERO(0x08866960u);
GT_DECOMP_ZERO(0x088B1330u);
GT_DECOMP_ZERO(0x088D6910u);
GT_DECOMP_ZERO(0x088D9CC4u);
GT_DECOMP_ZERO(0x0890AF40u);
GT_DECOMP_ZERO(0x0893E0F4u);
GT_DECOMP_ZERO(0x08946708u);
GT_DECOMP_ZERO(0x08955894u);
GT_DECOMP_ZERO(0x089A8FF4u);
GT_DECOMP_ZERO(0x08ACCB98u);
GT_DECOMP_ZERO(0x08B3544Cu);
GT_DECOMP_ZERO(0x08B6E524u);

GT_DECOMP_FORWARD(0x0885E314u, 0, 0x088669B4u);
GT_DECOMP_FORWARD(0x0885E41Cu, 0, 0x0885DDD8u);
GT_DECOMP_FORWARD(0x0886A280u, 4, 0x0886A13Cu);
GT_DECOMP_FORWARD(0x0886BD48u, 4, 0x08B1FBECu);
GT_DECOMP_FORWARD(0x08875088u, 0, 0x08875F1Cu);
GT_DECOMP_FORWARD(0x0887AC3Cu, 4, 0x0887AB44u);
GT_DECOMP_FORWARD(0x0887ACB0u, 4, 0x0887AA74u);
GT_DECOMP_FORWARD(0x0887B1ECu, 4, 0x0887B0DCu);
GT_DECOMP_FORWARD(0x0887C5A4u, 4, 0x0887C3ACu);
GT_DECOMP_FORWARD(0x0887C8E8u, 0, 0x08B35254u);
GT_DECOMP_FORWARD(0x0889364Cu, 0, 0x08893160u);
GT_DECOMP_FORWARD(0x0889365Cu, 0, 0x088931A0u);
GT_DECOMP_FORWARD(0x08893664u, 0, 0x08893230u);
GT_DECOMP_FORWARD(0x0889366Cu, 0, 0x08893244u);
GT_DECOMP_FORWARD(0x08893684u, 0, 0x08893260u);
GT_DECOMP_FORWARD(0x0889368Cu, 0, 0x08893280u);
GT_DECOMP_FORWARD(0x08893694u, 0, 0x088932A0u);
GT_DECOMP_FORWARD(0x0889369Cu, 0, 0x0889347Cu);
GT_DECOMP_FORWARD(0x0889387Cu, 0, 0x088937E0u);
GT_DECOMP_FORWARD(0x088A6AB4u, 0, 0x088A693Cu);
GT_DECOMP_FORWARD(0x088A719Cu, 0, 0x088A70CCu);
GT_DECOMP_FORWARD(0x088AAAE0u, 0, 0x088AA774u);
GT_DECOMP_FORWARD(0x088AB024u, 0, 0x088AAF00u);
GT_DECOMP_FORWARD(0x088DA4B0u, 20, 0x088FE2FCu);
GT_DECOMP_FORWARD(0x088DB5C4u, 20, 0x088FE518u);
GT_DECOMP_FORWARD(0x088FE388u, 0, 0x088FAED4u);
GT_DECOMP_FORWARD(0x089061D0u, 72, 0x08906160u);
GT_DECOMP_FORWARD(0x08909998u, 0, 0x08AF2654u);
GT_DECOMP_FORWARD(0x08910C7Cu, 0, 0x08910B8Cu);
GT_DECOMP_FORWARD(0x08911780u, 0, 0x089116D0u);
GT_DECOMP_FORWARD(0x08911D80u, 0, 0x08911B80u);
GT_DECOMP_FORWARD(0x0891570Cu, 0, 0x08914268u);
GT_DECOMP_FORWARD(0x089260ECu, 480, 0x089273D8u);
GT_DECOMP_FORWARD(0x089A930Cu, 20, 0x0897BB18u);
GT_DECOMP_FORWARD(0x089A9314u, 20, 0x0897C0E4u);
GT_DECOMP_FORWARD(0x089A931Cu, 20, 0x0897876Cu);
GT_DECOMP_FORWARD(0x089A9324u, 20, 0x0897783Cu);
GT_DECOMP_FORWARD(0x08A4D2F0u, 120, 0x08A508E0u);
GT_DECOMP_FORWARD(0x08A61584u, 60, 0x08A728A0u);
GT_DECOMP_FORWARD(0x08A6158Cu, 60, 0x08A72708u);
GT_DECOMP_FORWARD(0x08A61594u, 60, 0x08A72724u);
GT_DECOMP_FORWARD(0x08A615B4u, 60, 0x08A728FCu);
GT_DECOMP_FORWARD(0x08A615BCu, 60, 0x08A729F8u);
GT_DECOMP_FORWARD(0x08AB86B8u, 0, 0x08AD2B50u);
GT_DECOMP_FORWARD(0x08AB87D0u, 0, 0x08AD1D20u);
GT_DECOMP_FORWARD(0x08AB89BCu, 0, 0x08AD1660u);
GT_DECOMP_FORWARD(0x08AB89C4u, 0, 0x08AD1990u);
GT_DECOMP_FORWARD(0x08AB89CCu, 0, 0x08AD1740u);
GT_DECOMP_FORWARD(0x08AB89F4u, 0, 0x08AD1A3Cu);
GT_DECOMP_FORWARD(0x08AB89FCu, 0, 0x08AD1528u);
GT_DECOMP_FORWARD(0x08ACB4F8u, 0, 0x08ACB484u);
GT_DECOMP_FORWARD(0x08ACB5B4u, 0, 0x08ACB548u);
GT_DECOMP_FORWARD(0x08AF1DC8u, 4, 0x08AF1C40u);
GT_DECOMP_FORWARD(0x08B03A04u, 0, 0x08B025D4u);
GT_DECOMP_FORWARD(0x08B03A1Cu, 0, 0x08B0162Cu);
GT_DECOMP_FORWARD(0x08B046B8u, 0, 0x08B044C0u);
GT_DECOMP_FORWARD(0x08B09804u, 0, 0x08B09320u);

// 0x0885BBBC: gt::game::race_operator::window_max (src/game/race/race_operator.cpp).
// 0x0885BE8C: gt::game::race_operator::race_parameter.
GT_DECOMP_DEREF(0x0885C2B4u, 4);
GT_DECOMP_DEREF(0x0885EDBCu, 1520);
GT_DECOMP_DEREF(0x0885EDD4u, 1524);
GT_DECOMP_DEREF(0x0885EDF8u, 1452);
GT_DECOMP_DEREF(0x08893D14u, 4);
GT_DECOMP_DEREF(0x088A5984u, 1232);
GT_DECOMP_DEREF(0x088FE398u, 60);
GT_DECOMP_DEREF(0x08AF26E4u, 16);
GT_DECOMP_DEREF(0x08AF27B4u, 16);
GT_DECOMP_DEREF(0x08AF2834u, 0);
GT_DECOMP_DEREF(0x08AF2840u, 4);
GT_DECOMP_DEREF(0x08AF2924u, 52);
GT_DECOMP_DEREF(0x08AF293Cu, 56);
GT_DECOMP_DEREF(0x08AF2954u, 60);
// Batch 106: reached by the 11-route gate, each shape matched instruction for instruction.
GT_DECOMP_FIELD_PLUS(0x0888EB9Cu, 0, static_cast<std::uint32_t>(8));
GT_DECOMP_FIELD_PLUS(0x088915C8u, 0, static_cast<std::uint32_t>(8));
GT_DECOMP_FORWARD(0x088915F0u, 0, 0x0889117Cu);
GT_DECOMP_FORWARD(0x08891600u, 0, 0x0889118Cu);
GT_DECOMP_FORWARD(0x08892138u, 0, 0x08892114u);
GT_DECOMP_FIELD_PLUS(0x088A429Cu, 484, static_cast<std::uint32_t>(1036));
GT_DECOMP_DEREF_FLOAT(0x088A42A8u, 484, static_cast<std::uint32_t>(1048));
GT_DECOMP_FIELD(0x088CEF60u, 188);
GT_DECOMP_FIELD(0x088CEF68u, 168);
GT_DECOMP_FIELD(0x088CEF70u, 172);
GT_DECOMP_FIELD(0x088CEF78u, 160);
GT_DECOMP_FIELD(0x088CEF80u, 224);
GT_DECOMP_FIELD(0x088CF6D0u, 296);
GT_DECOMP_FIELD(0x088CFA8Cu, 292);
GT_DECOMP_FIELD(0x088CFA94u, 300);
GT_DECOMP_FIELD_PLUS(0x08B09A04u, 0, static_cast<std::uint32_t>(288));

// Batch 138 (2026-10-25): the rest of these families, which no route reaches — verified by synthetic cases
// (tools.decomp_gate synth); rows generated from each member's own words by tools.family_members. A commented row
// had no comparable synthetic case (every one faulted in the original), so nothing verifies it yet.
GT_DECOMP_ZERO(0x0880B638u);
GT_DECOMP_ZERO(0x088669D4u);
GT_DECOMP_ZERO(0x0886F834u);
GT_DECOMP_ZERO(0x0886F874u);
GT_DECOMP_ZERO(0x0886F8D4u);
GT_DECOMP_ZERO(0x0886F8DCu);
GT_DECOMP_ZERO(0x0887E270u);
GT_DECOMP_ZERO(0x088D47D4u);
GT_DECOMP_ZERO(0x088ECAC8u);
GT_DECOMP_ZERO(0x08907D64u);
GT_DECOMP_ZERO(0x0890AF90u);
GT_DECOMP_ZERO(0x0895935Cu);
GT_DECOMP_ZERO(0x089A88FCu);
GT_DECOMP_ZERO(0x089A8FFCu);
GT_DECOMP_ZERO(0x089C4BA8u);
GT_DECOMP_ZERO(0x08A11F28u);
GT_DECOMP_ZERO(0x08A76DA8u);
GT_DECOMP_ZERO(0x08A76DB0u);
GT_DECOMP_ZERO(0x08A76DB8u);
GT_DECOMP_ZERO(0x08A76DC0u);
GT_DECOMP_ZERO(0x08A76DD4u);
GT_DECOMP_ZERO(0x08A76DDCu);
GT_DECOMP_ZERO(0x08A76DE4u);
GT_DECOMP_ZERO(0x08ACCC20u);
GT_DECOMP_ZERO(0x08AFC8B8u);
GT_DECOMP_ZERO(0x08AFC8C0u);
GT_DECOMP_ZERO(0x08AFC8C8u);
GT_DECOMP_ZERO(0x08B09438u);
GT_DECOMP_ZERO(0x08B09528u);
GT_DECOMP_ZERO(0x08B2F704u);
GT_DECOMP_ZERO(0x08B52D50u);
GT_DECOMP_ZERO(0x08B61BD0u);
GT_DECOMP_FORWARD(0x0885BBE8u, 0x00000004u, 0x0885E414u);
// GT_DECOMP_FORWARD(0x0885C0D4u, 0x00000004u, 0x08864A74u);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x0885C2C0u, 0x00000004u, 0x0885E570u);
GT_DECOMP_FORWARD(0x0885E414u, 0x00000000u, 0x0885DD70u);
GT_DECOMP_FORWARD(0x0885F0B8u, 0x00000000u, 0x0885E2F8u);
// GT_DECOMP_FORWARD(0x0885F428u, 0x00000000u, 0x0885F3FCu);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x08860AD0u, 0x00000000u, 0x08860740u);
// GT_DECOMP_FORWARD(0x0886B2E0u, 0x00000004u, 0x08B1FDB0u);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x088852B4u, 0x0000000Cu, 0x08884EB4u);
GT_DECOMP_FORWARD(0x088852BCu, 0x0000000Cu, 0x08884E54u);
GT_DECOMP_FORWARD(0x0888598Cu, 0x0000000Cu, 0x088858B0u);
// GT_DECOMP_FORWARD(0x08891560u, 0x00000000u, 0x088910E4u);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x088915E8u, 0x00000000u, 0x08891140u);
GT_DECOMP_FORWARD(0x088916C0u, 0x00000000u, 0x08891650u);
GT_DECOMP_FORWARD(0x088936DCu, 0x00000000u, 0x0889362Cu);
GT_DECOMP_FORWARD(0x088A5E54u, 0x00000000u, 0x088A5D50u);
GT_DECOMP_FORWARD(0x088D2C28u, 0x00000000u, 0x088DA460u);
GT_DECOMP_FORWARD(0x088DA7E8u, 0x00000618u, 0x088D1E14u);
GT_DECOMP_FORWARD(0x088E49C8u, 0x00000000u, 0x088ECB0Cu);
GT_DECOMP_FORWARD(0x088E49D0u, 0x00000000u, 0x088ECB14u);
GT_DECOMP_FORWARD(0x088E49E4u, 0x00000000u, 0x088ECAD0u);
GT_DECOMP_FORWARD(0x08917E54u, 0x00000008u, 0x089272F4u);
GT_DECOMP_FORWARD(0x08A23A1Cu, 0x00000014u, 0x08A237ECu);
GT_DECOMP_FORWARD(0x08A23A2Cu, 0x00000014u, 0x08A238A0u);
GT_DECOMP_FORWARD(0x08A23A34u, 0x00000014u, 0x08A23838u);
GT_DECOMP_FORWARD(0x08A23BA0u, 0x00000014u, 0x08A23ABCu);
GT_DECOMP_FORWARD(0x08A25054u, 0x00000014u, 0x08A24E54u);
GT_DECOMP_FORWARD(0x08AB87D8u, 0x00000000u, 0x08AD1D2Cu);
GT_DECOMP_FORWARD(0x08AC337Cu, 0x00000000u, 0x08AC3280u);
GT_DECOMP_FORWARD(0x08ACADA0u, 0x00000000u, 0x08ACAD3Cu);
GT_DECOMP_FORWARD(0x08ACAFCCu, 0x00000000u, 0x08ACA4F4u);
GT_DECOMP_FORWARD(0x08ACAFD4u, 0x00000000u, 0x08ACA53Cu);
GT_DECOMP_FORWARD(0x08ACAFDCu, 0x00000000u, 0x08ACAC58u);
GT_DECOMP_FORWARD(0x08ACAFE4u, 0x00000000u, 0x08ACA584u);
GT_DECOMP_FORWARD(0x08ACAFECu, 0x00000000u, 0x08ACA5D4u);
GT_DECOMP_FORWARD(0x08ACAFF4u, 0x00000000u, 0x08ACA624u);
GT_DECOMP_FORWARD(0x08ACAFFCu, 0x00000000u, 0x08ACAEB0u);
// GT_DECOMP_FORWARD(0x08ACC0ECu, 0x00000000u, 0x08ACBC10u);  // no comparable synthetic case yet
// GT_DECOMP_FORWARD(0x08ACC6B4u, 0x00000000u, 0x08ACC640u);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x08B03A40u, 0x00000000u, 0x08B0157Cu);
GT_DECOMP_FORWARD(0x08B09974u, 0x00000000u, 0x08B09354u);
// GT_DECOMP_FORWARD(0x08B19FECu, 0x00000000u, 0x08B19FB0u);  // no comparable synthetic case yet
// GT_DECOMP_FORWARD(0x08B1A0B4u, 0x00000000u, 0x08B1A088u);  // no comparable synthetic case yet
GT_DECOMP_FORWARD(0x08B61664u, 0x00000000u, 0x08B7831Cu);
GT_DECOMP_FORWARD(0x08B61974u, 0x00000000u, 0x08B7807Cu);
GT_DECOMP_FORWARD(0x08C6F55Cu, 0x00000020u, 0x08B7807Cu);
