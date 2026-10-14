// Hot batch 70: two function-local-static families found with `tools.family_members` (PR #178) over the call probe of the five routes
// (`tools.decomp_gate probe`: every function of up to 70 instructions not yet decompiled, run with --decomp off; the reached ones grouped by
// instruction shape). Neither family is a straight-line one: the guarded constructions branch on the C++ guard byte, so each function's
// comment lists the branches the compared cases of the fast gate do not take, checked against the asm by hand reading the branch TARGET.
//
// Families:
//  - "guarded static object" (35 instructions, new, 37 members): `static T x(source, size);` inside a function: if the guard byte is 0 and
//    func_08B6441C(guard) lets this caller build it, the object is constructed (constructor(this, source, size)), its first word is set to the
//    class's vtable-like constant, the accessor of the static it depends on is called, and func_08B64494(guard) releases the guard (a tail call);
//  - "empty container accessor" (32 instructions, batch 68 / PR #176): the same guard around an empty container; 12 more members. The
//    implementation is the one of batch 68, kept as this file's own copy so this PR does not depend on that one.
//
// Every member's constants and call targets were read from its own relocated words (`tools.family_members`, which resolves each `lui`/`addiu`
// pair against the ELF relocations' segment, and checks every jal / j target against the raw word), after the tool had reported that no other
// word of the member differs from the family's template. The first member (func_08B9FA50) was read by hand in full to write the template.
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_70 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu; // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u; // (guard)
constexpr std::uint32_t kAtExit = 0x08B77B1Cu;       // (function): registers a destructor for exit

// Guarded static object. Original order:
//   v0 = hi(guard) (the relocated `lui`; also the result of the early exit); if the byte at guard is not 0: return;
//   if func_08B6441C(guard) == 0: return 0 (another caller is building it);
//   if object != 0 (the compiler's `this` check of the placement new; the address is a constant, so it always holds):
//     constructor(object, source, size); object[0] = vtable (the store sits in the delay slot of the next call, so it is done before it runs);
//     registrar() (the accessor of another function-local static; it loads its own guard first and reads no argument);
//   tail call func_08B64494(guard).
// v0 is hi(guard) when the guard byte is already set, 0 when the acquire fails, else whatever the release leaves.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS; the 37 members have the same words as func_08B9FA50
// apart from the relocated constants and call targets): func_08B6441C answering 0 (another caller is building it): the beqz lands on the epilogue
// (lw ra / s1 / s0, jr ra) with v0 = 0, the delay slot only sets v1; the null `object` (beqz v1): lands on `move a0, s0` and the same tail call
// to the release, skipping the construction (the delay slot a2 = size is dead). Every member's object is a constant address above the load base,
// so that one cannot happen; it is kept because the original tests it.
void guarded_static_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                           std::uint32_t source, std::uint32_t size, std::uint32_t constructor, std::uint32_t vtable,
                           std::uint32_t registrar) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) != 0) {
            ctx.gpr[2] = (guard + 0x8000u) & 0xFFFF0000u;
            return;
        }
        if (call_guest(rt, ctx, kGuardAcquire, {guard}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (object != 0u) {
            (void)call_guest(rt, ctx, constructor, {object, source, size});
            put(rt, object, vtable);
            (void)call_guest(rt, ctx, registrar, {});
        }
    }
    tail_call(ctx, kGuardRelease, {guard});
}

// The accessor of a function-local static container: while its guard byte is 0 and func_08B6441C(guard) lets this caller build it, the container is
// made empty (+16 = +12 = its header at +4; +4, +8, +20 cleared), func_08B64494(guard) releases the guard and func_08B77B1C(destructor) registers
// the destructor for exit. v0 = the container's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (another caller is
// building it: the beqz lands on the epilogue, and v0 = the object is set there; the delay slot a0 = guard is dead).
void get_empty_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                         std::uint32_t destructor) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 16u, object + 4u);
            put(rt, object + 20u, 0u);
            put(rt, object + 4u, 0u);
            put(rt, object + 8u, 0u);
            put(rt, object + 12u, object + 4u);
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
            (void)call_guest(rt, ctx, kAtExit, {destructor});
        }
    }
    ctx.gpr[2] = object;
}

} // namespace

// func_XXXXXXXX (guarded static object): see guarded_static_object (guard, object, source, size, constructor, vtable, registrar).
void func_08B9FA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75448u, 0x08D75B9Cu, 0x08CA0184u, 1004u, 0x08B9FA0Cu, 0x08CC0EF8u, 0x08B9E3B8u); }
void func_08BA1498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D754E8u, 0x08D75C04u, 0x08CA01B8u, 24u, 0x08BA1454u, 0x08CC0700u, 0x08B9DAB8u); }
void func_08BA1588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D754F0u, 0x08D75C14u, 0x08CA01C0u, 20u, 0x08BA1544u, 0x08CC06B0u, 0x08B9DAB8u); }
void func_08BA1678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D754F8u, 0x08D75C24u, 0x08CA01C4u, 16u, 0x08BA1634u, 0x08CC0660u, 0x08B9DAB8u); }
void func_08BA1964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75450u, 0x08D75C48u, 0x08CA01D4u, 988u, 0x08BA1920u, 0x08CC0C90u, 0x08B9E328u); }
void func_08BA1AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75490u, 0x08D75C58u, 0x08CA01E4u, 32u, 0x08BA1A74u, 0x08CC0750u, 0x08B9E328u); }
void func_08BA1BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75498u, 0x08D75C68u, 0x08CA01E8u, 4u, 0x08BA1B64u, 0x08CC0550u, 0x08B9E328u); }
void func_08BA1E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75508u, 0x08D75C88u, 0x08CA01F4u, 24u, 0x08BA1DE0u, 0x08CC0500u, 0x08B9DCE4u); }
void func_08BA1F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75510u, 0x08D75C98u, 0x08CA0208u, 20u, 0x08BA1ED0u, 0x08CC04B0u, 0x08B9DCE4u); }
void func_08BA2004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75518u, 0x08D75CA8u, 0x08CA0214u, 16u, 0x08BA1FC0u, 0x08CC0460u, 0x08B9DCE4u); }
void func_08BA20F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75520u, 0x08D75CB8u, 0x08CA0224u, 12u, 0x08BA20B0u, 0x08CC0410u, 0x08B9DCE4u); }
void func_08BA21E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75528u, 0x08D75CC8u, 0x08CA01C0u, 8u, 0x08BA21A0u, 0x08CC03C0u, 0x08B9DCE4u); }
void func_08BA22D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D75530u, 0x08D75CD8u, 0x08CA0230u, 4u, 0x08BA2290u, 0x08CC0370u, 0x08B9DCE4u); }
void func_08BC53B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D761E0u, 0x08D76720u, 0x08CA0E04u, 4u, 0x08BC536Cu, 0x08CC4D68u, 0x08BC5250u); }
void func_08BC5480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D761D8u, 0x08D76730u, 0x08CA0E08u, 8u, 0x08BC543Cu, 0x08CC4DB8u, 0x08BC5250u); }
void func_08BC5550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D761D0u, 0x08D76740u, 0x08CA0E10u, 12u, 0x08BC550Cu, 0x08CC4E08u, 0x08BC5250u); }
void func_08BC5C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76200u, 0x08D7679Cu, 0x08CA0E58u, 16u, 0x08BC5C10u, 0x08CC49D8u, 0x08BC5AE0u); }
void func_08BCB55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76210u, 0x08D76834u, 0x08CA0FB4u, 36u, 0x08BCB518u, 0x08CC4878u, 0x08BC5F4Cu); }
void func_08BCB64C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76218u, 0x08D76844u, 0x08CA0FD0u, 32u, 0x08BCB608u, 0x08CC4828u, 0x08BC5F4Cu); }
void func_08BCB73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76220u, 0x08D76854u, 0x08CA0FE4u, 28u, 0x08BCB6F8u, 0x08CC47D8u, 0x08BC5F4Cu); }
void func_08BCB82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76228u, 0x08D76864u, 0x08CA1004u, 24u, 0x08BCB7E8u, 0x08CC4788u, 0x08BC5F4Cu); }
void func_08BCB91C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76230u, 0x08D76874u, 0x08CA101Cu, 20u, 0x08BCB8D8u, 0x08CC4738u, 0x08BC5F4Cu); }
void func_08BCBA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76238u, 0x08D76884u, 0x08CA103Cu, 16u, 0x08BCB9C8u, 0x08CC46E8u, 0x08BC5F4Cu); }
void func_08BCBAFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76240u, 0x08D76894u, 0x08CA1054u, 12u, 0x08BCBAB8u, 0x08CC4698u, 0x08BC5F4Cu); }
void func_08BCBBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76248u, 0x08D768A4u, 0x08CA106Cu, 8u, 0x08BCBBA8u, 0x08CC4648u, 0x08BC5F4Cu); }
void func_08BCBCDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76250u, 0x08D768B4u, 0x08CA107Cu, 4u, 0x08BCBC98u, 0x08CC45F8u, 0x08BC5F4Cu); }
void func_08BCC0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D762E0u, 0x08D768D4u, 0x08CA1138u, 1180u, 0x08BCC088u, 0x08CC4E58u, 0x08BC6550u); }
void func_08BCC1BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D762E8u, 0x08D768E4u, 0x08CA1140u, 1176u, 0x08BCC178u, 0x08CC4C58u, 0x08BC6550u); }
void func_08BCC2AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D762F0u, 0x08D768F4u, 0x08CA114Cu, 1160u, 0x08BCC268u, 0x08CC4C08u, 0x08BC6550u); }
void func_08BCC39C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D762F8u, 0x08D76904u, 0x08CA1158u, 1144u, 0x08BCC358u, 0x08CC4BB8u, 0x08BC6550u); }
void func_08BCC48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76300u, 0x08D76914u, 0x08CA1164u, 1128u, 0x08BCC448u, 0x08CC4B68u, 0x08BC6550u); }
void func_08BCC57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76308u, 0x08D76924u, 0x08CA1174u, 1112u, 0x08BCC538u, 0x08CC4B18u, 0x08BC6550u); }
void func_08BCC66C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76310u, 0x08D76934u, 0x08CA1180u, 1096u, 0x08BCC628u, 0x08CC4AC8u, 0x08BC6550u); }
void func_08BCC75C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76318u, 0x08D76944u, 0x08CA118Cu, 1080u, 0x08BCC718u, 0x08CC4A78u, 0x08BC6550u); }
void func_08BCC84C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76320u, 0x08D76954u, 0x08CA1108u, 1060u, 0x08BCC808u, 0x08CC4A28u, 0x08BC6550u); }
void func_08BCC93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76328u, 0x08D76964u, 0x08CA119Cu, 1020u, 0x08BCC8F8u, 0x08CC48C8u, 0x08BC6550u); }
void func_08BCC9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_object(rt, ctx, 0x08D76330u, 0x08D76974u, 0x08CA11ACu, 1004u, 0x08BC264Cu, 0x08CC44C0u, 0x08BC6550u); }

// func_XXXXXXXX (empty container accessor): see get_empty_container (guard, object, destructor).
void func_08BF08A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76B30u, 0x08D76B3Cu, 0x0898FF78u); }
void func_08BF3384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76BD0u, 0x08D76BDCu, 0x0899A784u); }
void func_08BFF624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76D30u, 0x08D76D3Cu, 0x089BCC48u); }
void func_08BFFCE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76D58u, 0x08D76D64u, 0x089C062Cu); }
void func_08C00754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76D80u, 0x08D76D8Cu, 0x089C2C98u); }
void func_08C044BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76DF8u, 0x08D76E04u, 0x089D7D04u); }
void func_08C04F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76E20u, 0x08D76E2Cu, 0x089D8EA8u); }
void func_08C0B2D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76EC0u, 0x08D76ECCu, 0x089EF1E4u); }
void func_08C0C010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76F10u, 0x08D76F1Cu, 0x089F6EE0u); }
void func_08C0ECFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76F80u, 0x08D76F8Cu, 0x089FD3E4u); }
void func_08C10E60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D77028u, 0x08D77050u, 0x08A05BE4u); }
void func_08C149AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D770B8u, 0x08D770C4u, 0x08A10144u); }
void func_08C18ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D770E0u, 0x08D770ECu, 0x08A19ED4u); }

} // namespace hot_batch_70
} // namespace gt::game

#define HOT70(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_70::name, gt::ReturnKind::kind)
HOT70(0x08B9FA50u, func_08B9FA50, Word);
HOT70(0x08BA1498u, func_08BA1498, Word);
HOT70(0x08BA1588u, func_08BA1588, Word);
HOT70(0x08BA1678u, func_08BA1678, Word);
HOT70(0x08BA1964u, func_08BA1964, Word);
HOT70(0x08BA1AB8u, func_08BA1AB8, Word);
HOT70(0x08BA1BA8u, func_08BA1BA8, Word);
HOT70(0x08BA1E24u, func_08BA1E24, Word);
HOT70(0x08BA1F14u, func_08BA1F14, Word);
HOT70(0x08BA2004u, func_08BA2004, Word);
HOT70(0x08BA20F4u, func_08BA20F4, Word);
HOT70(0x08BA21E4u, func_08BA21E4, Word);
HOT70(0x08BA22D4u, func_08BA22D4, Word);
HOT70(0x08BC53B0u, func_08BC53B0, Word);
HOT70(0x08BC5480u, func_08BC5480, Word);
HOT70(0x08BC5550u, func_08BC5550, Word);
HOT70(0x08BC5C54u, func_08BC5C54, Word);
HOT70(0x08BCB55Cu, func_08BCB55C, Word);
HOT70(0x08BCB64Cu, func_08BCB64C, Word);
HOT70(0x08BCB73Cu, func_08BCB73C, Word);
HOT70(0x08BCB82Cu, func_08BCB82C, Word);
HOT70(0x08BCB91Cu, func_08BCB91C, Word);
HOT70(0x08BCBA0Cu, func_08BCBA0C, Word);
HOT70(0x08BCBAFCu, func_08BCBAFC, Word);
HOT70(0x08BCBBECu, func_08BCBBEC, Word);
HOT70(0x08BCBCDCu, func_08BCBCDC, Word);
HOT70(0x08BCC0CCu, func_08BCC0CC, Word);
HOT70(0x08BCC1BCu, func_08BCC1BC, Word);
HOT70(0x08BCC2ACu, func_08BCC2AC, Word);
HOT70(0x08BCC39Cu, func_08BCC39C, Word);
HOT70(0x08BCC48Cu, func_08BCC48C, Word);
HOT70(0x08BCC57Cu, func_08BCC57C, Word);
HOT70(0x08BCC66Cu, func_08BCC66C, Word);
HOT70(0x08BCC75Cu, func_08BCC75C, Word);
HOT70(0x08BCC84Cu, func_08BCC84C, Word);
HOT70(0x08BCC93Cu, func_08BCC93C, Word);
HOT70(0x08BCC9C8u, func_08BCC9C8, Word);
HOT70(0x08BF08A8u, func_08BF08A8, Word);
HOT70(0x08BF3384u, func_08BF3384, Word);
HOT70(0x08BFF624u, func_08BFF624, Word);
HOT70(0x08BFFCE4u, func_08BFFCE4, Word);
HOT70(0x08C00754u, func_08C00754, Word);
HOT70(0x08C044BCu, func_08C044BC, Word);
HOT70(0x08C04F64u, func_08C04F64, Word);
HOT70(0x08C0B2D8u, func_08C0B2D8, Word);
HOT70(0x08C0C010u, func_08C0C010, Word);
HOT70(0x08C0ECFCu, func_08C0ECFC, Word);
HOT70(0x08C10E60u, func_08C10E60, Word);
HOT70(0x08C149ACu, func_08C149AC, Word);
HOT70(0x08C18ECCu, func_08C18ECC, Word);
