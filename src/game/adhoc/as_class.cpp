// The "as class" template instances: GT's C++ casts an Adhoc value holder to one engine class with a
// small function per class (every native calls one or two). Each instance does the same
// two things — reset the output holder to the shared empty holder, then jump to that class's checked
// assignment `cast(out, source)` — and differs only in which holder-copy instance it calls and which
// cast it jumps to. One function body here serves them all; the table gives each instance's two
// addresses (found by matching the instances' identical code in the executable). The executable has
// 394 such instances; only those the race path calls (and the harness compared equal) are listed —
// the others stay recompiled until a run reaches them.
//
// Name evidence: the natives that call them right before using the object as that class
// (e.g. 0x088547C4 before MCarParameter methods: src/game/adhoc/game_natives.cpp).

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::adhoc {
namespace {

constexpr std::uint32_t kEmptyHolder = 0x08CF6ED8u;  // a holder that holds nothing

struct AsClass {
    std::uint32_t copy;  // holder_copy instance (holder *to, const holder *from)
    std::uint32_t cast;  // (holder *out, const holder *source): out = source if it is that class
};

std::unordered_map<std::uint32_t, AsClass> &as_class_table() {
    static std::unordered_map<std::uint32_t, AsClass> table;
    return table;
}

} // namespace

struct AsClassRegistrar {
    AsClassRegistrar(std::uint32_t entry, std::uint32_t copy, std::uint32_t cast) { as_class_table()[entry] = {copy, cast}; }
};

// as_class(holder *out, const holder *source): out = empty, then the class's cast fills it in.
// The instance is told apart by its entry address (ctx.pc when it starts).
void as_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const AsClass &instance = as_class_table().at(ctx.pc);
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    (void)call_guest(rt, ctx, instance.copy, {out, kEmptyHolder});
    tail_call(ctx, instance.cast, {out, source});
}

} // namespace gt::adhoc

#define GT_DECOMP_AS_CLASS(entry, copy, cast)                                                                  \
    static const ::gt::adhoc::AsClassRegistrar GT_DECOMP_CAT(gt_as_class_, __LINE__){(entry), (copy), (cast)}; \
    GT_DECOMP(entry, gt::adhoc::as_class, gt::ReturnKind::Void)

GT_DECOMP_AS_CLASS(0x088088C8u, 0x08A76AB4u, 0x0880881Cu);
GT_DECOMP_AS_CLASS(0x08809DBCu, 0x08A013D0u, 0x08809D10u);
GT_DECOMP_AS_CLASS(0x0880F0C0u, 0x08969190u, 0x0880EFB4u);
GT_DECOMP_AS_CLASS(0x08817FCCu, 0x08A76AB4u, 0x08817F20u);
GT_DECOMP_AS_CLASS(0x0881BD98u, 0x08A76AB4u, 0x0881BCECu);
GT_DECOMP_AS_CLASS(0x088248E4u, 0x08A76AB4u, 0x08824838u);
GT_DECOMP_AS_CLASS(0x088297A8u, 0x08A2A218u, 0x088296FCu);
GT_DECOMP_AS_CLASS(0x08835BD8u, 0x08A76AB4u, 0x08835B2Cu);
GT_DECOMP_AS_CLASS(0x088378F0u, 0x08A76AB4u, 0x088377E4u);
GT_DECOMP_AS_CLASS(0x088447E8u, 0x08A76AB4u, 0x0884473Cu);
GT_DECOMP_AS_CLASS(0x088458D0u, 0x08A76AB4u, 0x08845824u);
GT_DECOMP_AS_CLASS(0x08847008u, 0x08A76AB4u, 0x08846F5Cu);
GT_DECOMP_AS_CLASS(0x0884C868u, 0x08A76AB4u, 0x0884C7BCu);
GT_DECOMP_AS_CLASS(0x0884D660u, 0x08A76AB4u, 0x0884D5B4u);
GT_DECOMP_AS_CLASS(0x088547C4u, 0x08A76AB4u, 0x08854718u);
GT_DECOMP_AS_CLASS(0x089657BCu, 0x089BA130u, 0x08965710u);
GT_DECOMP_AS_CLASS(0x08969244u, 0x089BAC44u, 0x08969198u);
GT_DECOMP_AS_CLASS(0x08973EF8u, 0x08A76AB4u, 0x08973E4Cu);
GT_DECOMP_AS_CLASS(0x089748D8u, 0x089D8A60u, 0x0897482Cu);
GT_DECOMP_AS_CLASS(0x08976F68u, 0x089FE8D8u, 0x08976E5Cu);
GT_DECOMP_AS_CLASS(0x08978000u, 0x08A76AB4u, 0x08977F54u);
GT_DECOMP_AS_CLASS(0x0897E844u, 0x08969190u, 0x0897E798u);
GT_DECOMP_AS_CLASS(0x089896CCu, 0x08969190u, 0x08989620u);
GT_DECOMP_AS_CLASS(0x08993C54u, 0x08A0D868u, 0x08993928u);
GT_DECOMP_AS_CLASS(0x08994954u, 0x08A013D0u, 0x08994718u);
GT_DECOMP_AS_CLASS(0x08995460u, 0x089948F4u, 0x089953B4u);
GT_DECOMP_AS_CLASS(0x0899BA84u, 0x08973D98u, 0x0899B9D8u);
GT_DECOMP_AS_CLASS(0x0899C4B4u, 0x089BA130u, 0x0899C33Cu);
GT_DECOMP_AS_CLASS(0x0899DD60u, 0x089A274Cu, 0x0899DCB4u);
GT_DECOMP_AS_CLASS(0x089A0D48u, 0x089D8A60u, 0x089A0C3Cu);
GT_DECOMP_AS_CLASS(0x089A354Cu, 0x089A274Cu, 0x089A34A0u);
GT_DECOMP_AS_CLASS(0x089A6B34u, 0x08A76AB4u, 0x089A6A88u);
GT_DECOMP_AS_CLASS(0x089AB370u, 0x08A76AB4u, 0x089AB2C4u);
GT_DECOMP_AS_CLASS(0x089B3840u, 0x08A013D0u, 0x089B3744u);
GT_DECOMP_AS_CLASS(0x089B6204u, 0x089A274Cu, 0x089B60F8u);
GT_DECOMP_AS_CLASS(0x089BB3A8u, 0x08A7C0D8u, 0x089BB2FCu);
GT_DECOMP_AS_CLASS(0x089BEE4Cu, 0x089E119Cu, 0x089BEDA0u);
GT_DECOMP_AS_CLASS(0x089C5850u, 0x08A76AB4u, 0x089C57A4u);
GT_DECOMP_AS_CLASS(0x089CD62Cu, 0x0899537Cu, 0x089CD580u);
GT_DECOMP_AS_CLASS(0x089D5DF4u, 0x08965854u, 0x089D5D48u);
GT_DECOMP_AS_CLASS(0x089E1104u, 0x08A0815Cu, 0x089E1058u);
GT_DECOMP_AS_CLASS(0x089E5E50u, 0x0899AE08u, 0x089E5AB0u);
GT_DECOMP_AS_CLASS(0x089F0DE0u, 0x089FDAB0u, 0x089F0D34u);
GT_DECOMP_AS_CLASS(0x089F5C74u, 0x089FF120u, 0x089F5BC8u);
GT_DECOMP_AS_CLASS(0x089F6B94u, 0x08A0FD20u, 0x089F6AE8u);
GT_DECOMP_AS_CLASS(0x089FDB7Cu, 0x08A76AB4u, 0x089FDAD0u);
GT_DECOMP_AS_CLASS(0x08A01338u, 0x08969190u, 0x08A0128Cu);
GT_DECOMP_AS_CLASS(0x08A0EBB4u, 0x08A76AB4u, 0x08A0EAA8u);
GT_DECOMP_AS_CLASS(0x08A0FD80u, 0x089BAC44u, 0x08A0FC74u);
GT_DECOMP_AS_CLASS(0x08A14610u, 0x08A2A218u, 0x08A14564u);
GT_DECOMP_AS_CLASS(0x08A1B570u, 0x08A76AB4u, 0x08A1B4C4u);
GT_DECOMP_AS_CLASS(0x08A1D50Cu, 0x08A76AB4u, 0x08A1D460u);
GT_DECOMP_AS_CLASS(0x08A20CC8u, 0x08A76AB4u, 0x08A20C1Cu);
GT_DECOMP_AS_CLASS(0x08A22050u, 0x08A76AB4u, 0x08A21FA4u);
GT_DECOMP_AS_CLASS(0x08A32598u, 0x08A76AB4u, 0x08A324ECu);
GT_DECOMP_AS_CLASS(0x08A354ECu, 0x08A76AB4u, 0x08A353E0u);
GT_DECOMP_AS_CLASS(0x08A3A9D0u, 0x08A76AB4u, 0x08A3A924u);
GT_DECOMP_AS_CLASS(0x08A40CF8u, 0x08A76AB4u, 0x08A40C4Cu);
GT_DECOMP_AS_CLASS(0x08A40E90u, 0x08A76AB4u, 0x08A40DE4u);
GT_DECOMP_AS_CLASS(0x08A52A18u, 0x08A7C0D8u, 0x08A5296Cu);
GT_DECOMP_AS_CLASS(0x08A5334Cu, 0x08A52AB0u, 0x08A532A0u);
GT_DECOMP_AS_CLASS(0x08A552F8u, 0x08A52AB0u, 0x08A5524Cu);
GT_DECOMP_AS_CLASS(0x08A57854u, 0x08A76AB4u, 0x08A56B28u);
GT_DECOMP_AS_CLASS(0x08A58B38u, 0x08A52AB0u, 0x08A58A8Cu);
GT_DECOMP_AS_CLASS(0x08A5A6E4u, 0x08A52AB0u, 0x08A5A638u);
GT_DECOMP_AS_CLASS(0x08A5D2D4u, 0x08A76AB4u, 0x08A5D228u);
GT_DECOMP_AS_CLASS(0x08A6C410u, 0x08A76AB4u, 0x08A6C360u);
GT_DECOMP_AS_CLASS(0x08A7C3A4u, 0x08A76AB4u, 0x08A7C2F8u);
GT_DECOMP_AS_CLASS(0x08A810F4u, 0x08A76AB4u, 0x08A81048u);
GT_DECOMP_AS_CLASS(0x08A85048u, 0x08A76AB4u, 0x08A84F9Cu);
GT_DECOMP_AS_CLASS(0x08A85940u, 0x08A76AB4u, 0x08A85890u);
GT_DECOMP_AS_CLASS(0x08A92548u, 0x08A7C0D8u, 0x08A92348u);
GT_DECOMP_AS_CLASS(0x08A94DA0u, 0x08A76AB4u, 0x08A94CF4u);
GT_DECOMP_AS_CLASS(0x08A98674u, 0x08A76AB4u, 0x08A985C8u);
