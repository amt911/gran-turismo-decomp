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

// --- the cast template: holder_cast(holder *out, const holder *source) -> bool ---
// out = source when it holds an instance of the class (an empty source gives an empty out);
// otherwise out = what the class's fallback makes (nil for most classes, a conversion of the source
// for the few that convert), and the result says whether out holds something. 197 instances in the
// executable, two register allocations of the same code; a table gives each one's class getter,
// fallback and whether the fallback takes the source. Listed: the ones the race path calls.
namespace {

constexpr std::uint32_t kIsInstanceOf = 0x08A76F20u;  // (object *, class *) -> bool
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;   // (holder *, object *)
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

struct HolderCast {
    std::uint32_t class_of;    // () -> the class
    std::uint32_t fallback;    // (holder *out[, const holder *source])
    bool fallback_takes_source;
};

std::unordered_map<std::uint32_t, HolderCast> &cast_table() {
    static std::unordered_map<std::uint32_t, HolderCast> table;
    return table;
}

} // namespace

struct HolderCastRegistrar {
    HolderCastRegistrar(std::uint32_t entry, std::uint32_t class_of, std::uint32_t fallback, bool takes_source) {
        cast_table()[entry] = {class_of, fallback, takes_source};
    }
};

void holder_cast(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const HolderCast &instance = cast_table().at(ctx.pc);
    auto &memory = rt.memory();
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t object = memory.load32(source);
    if (object == 0u) {
        (void)call_guest(rt, ctx, kHolderReset, {out, 0u});
        ctx.gpr[2] = 1u;
        return;
    }
    const std::uint32_t cls = call_guest(rt, ctx, instance.class_of, {out, 0u});
    if (call_guest(rt, ctx, kIsInstanceOf, {object, cls}) != 0u) {
        (void)call_guest(rt, ctx, kHolderReset, {out, memory.load32(source)});
        ctx.gpr[2] = 1u;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t made = frame.at(0u);
    (void)call_guest(rt, ctx, instance.fallback, {made, instance.fallback_takes_source ? source : cls});
    std::uint32_t holds = 0u;
    if (const std::uint32_t value = memory.load32(made); value != 0u) {
        (void)call_guest(rt, ctx, kHolderReset, {out, value});
        holds = 1u;
    }
    (void)call_guest(rt, ctx, kHolderDestroy, {made});
    ctx.gpr[2] = holds;
}

} // namespace gt::adhoc

#define GT_DECOMP_CAST(entry, class_of, fallback, takes_source)                                                \
    static const ::gt::adhoc::HolderCastRegistrar GT_DECOMP_CAT(gt_cast_, __LINE__){(entry), (class_of), (fallback), \
                                                                                    (takes_source)};            \
    GT_DECOMP(entry, gt::adhoc::holder_cast, gt::ReturnKind::Word)

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

GT_DECOMP_CAST(0x0880881Cu, 0x08808524u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0881BCECu, 0x0881B060u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08824838u, 0x08824668u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x088296FCu, 0x0882930Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x088377E4u, 0x08837430u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08846F5Cu, 0x08846E48u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0884C7BCu, 0x0884C494u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0884D5B4u, 0x0884D3D0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08854718u, 0x08854670u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08965710u, 0x08965454u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08969198u, 0x08965B94u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08973E4Cu, 0x08973B54u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0897482Cu, 0x089742B0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08976E5Cu, 0x08976CDCu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08977F54u, 0x08977258u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0897E798u, 0x0897E290u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08989620u, 0x08988EE4u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08993928u, 0x08993240u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08994718u, 0x08994494u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089953B4u, 0x08994F9Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0899B9D8u, 0x0899B8B8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x0899DCB4u, 0x0899D00Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089A34A0u, 0x089A2B5Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089A6A88u, 0x089A692Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089AB2C4u, 0x089A88F4u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089B60F8u, 0x089B57F0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089BB2FCu, 0x089BA3E8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089BEDA0u, 0x089BE7FCu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089C57A4u, 0x089C40DCu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089CD580u, 0x089CC8D4u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089D5D48u, 0x089D5B6Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089E1058u, 0x089DF9F0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089E5AB0u, 0x089E4D54u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089F0D34u, 0x089F0C2Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089F5BC8u, 0x089F5ACCu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x089FDAD0u, 0x089FD970u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A0128Cu, 0x08A003C0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A0EAA8u, 0x08A0E850u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A0FC74u, 0x08A0F048u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A14564u, 0x08A141E8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A21FA4u, 0x08A21E34u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A353E0u, 0x08A34D40u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A3A924u, 0x08A3A2D8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A40C4Cu, 0x08A40B20u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A40DE4u, 0x08A40AE8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A5296Cu, 0x08A52628u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A532A0u, 0x08A53080u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A5524Cu, 0x08A5514Cu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A56B28u, 0x08A561A0u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A58A8Cu, 0x08A588D8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A5A638u, 0x08A5A490u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A6C360u, 0x08A6BF40u, 0x08AA2F1Cu, true);
GT_DECOMP_CAST(0x08A7C2F8u, 0x08A7BC88u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A81048u, 0x08A80EDCu, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A84F9Cu, 0x08A84C98u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A85890u, 0x08A85454u, 0x08AA2494u, true);
GT_DECOMP_CAST(0x08A92348u, 0x08A921A8u, 0x08A76AA0u, false);
GT_DECOMP_CAST(0x08A985C8u, 0x08A9833Cu, 0x08A76AA0u, false);
