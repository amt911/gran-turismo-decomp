// MUnit: the Adhoc module through which scripts read and set the display units (km/h or mph,
// PS/hp/kW, kgf·m/ft-lb/N·m, °C/°F). Names are the ones GT registers for its scripts
// (config/symbol_addrs.txt, Adhoc natives); each unit is an integer kept in a global.
//
// Setters: native(context, argc, argv) — with at least one argument, the unit becomes argv[0]'s
// integer value. Getters: native(result holder) — the result becomes a new Adhoc integer.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::munit {
namespace {

constexpr std::uint32_t kUnitOfVelocity = 0x08D6950Cu;
constexpr std::uint32_t kUnitOfPower = 0x08D69510u;
constexpr std::uint32_t kUnitOfTorque = 0x08D69514u;
constexpr std::uint32_t kUnitOfTemperature = 0x08D69520u;

void set_unit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t unit) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    if (argc <= 0) return;
    const std::uint32_t argument = rt.memory().load32(ctx.gpr[6]);  // argv[0]; NULL is not checked
    rt.memory().store32(unit, static_cast<std::uint32_t>(to_int(rt, ctx, argument)));
}

void get_unit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t unit) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), rt.memory().load32(unit));
}

} // namespace

void SetUnitOfVelocity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_unit(rt, ctx, kUnitOfVelocity); }
void SetUnitOfPower(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_unit(rt, ctx, kUnitOfPower); }
void SetUnitOfTorque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_unit(rt, ctx, kUnitOfTorque); }
void SetUnitOfTemperature(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_unit(rt, ctx, kUnitOfTemperature); }
void GetCurrentUnitOfVelocity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_unit(rt, ctx, kUnitOfVelocity); }
void GetCurrentUnitOfPower(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_unit(rt, ctx, kUnitOfPower); }
void GetCurrentUnitOfTorque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_unit(rt, ctx, kUnitOfTorque); }
void GetCurrentUnitOfTemperature(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_unit(rt, ctx, kUnitOfTemperature); }

} // namespace gt::adhoc::munit

GT_DECOMP(0x08A252E8u, gt::adhoc::munit::SetUnitOfVelocity, gt::ReturnKind::Void);
GT_DECOMP(0x08A25338u, gt::adhoc::munit::SetUnitOfPower, gt::ReturnKind::Void);
GT_DECOMP(0x08A25388u, gt::adhoc::munit::SetUnitOfTorque, gt::ReturnKind::Void);
GT_DECOMP(0x08A253D8u, gt::adhoc::munit::SetUnitOfTemperature, gt::ReturnKind::Void);
GT_DECOMP(0x08A25B54u, gt::adhoc::munit::GetCurrentUnitOfVelocity, gt::ReturnKind::Void);
GT_DECOMP(0x08A25C34u, gt::adhoc::munit::GetCurrentUnitOfPower, gt::ReturnKind::Void);
GT_DECOMP(0x08A25DA4u, gt::adhoc::munit::GetCurrentUnitOfTorque, gt::ReturnKind::Void);
GT_DECOMP(0x08A25DECu, gt::adhoc::munit::GetCurrentUnitOfTemperature, gt::ReturnKind::Void);
