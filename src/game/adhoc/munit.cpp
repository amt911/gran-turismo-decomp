// MUnit: the Adhoc module through which scripts read and set the display units (km/h or mph,
// PS/hp/kW, kgf·m/ft-lb/N·m, °C/°F). Names are the ones GT registers for its scripts
// (config/symbol_addrs.txt, Adhoc natives); each unit is an integer kept in a global.
//
// Setters: native(context, argc, argv) — with at least one argument, the unit becomes argv[0]'s
// integer value (virtual slot 15 of the Adhoc object). Getters: native(result holder) — the result
// becomes a new Adhoc integer object holding the unit.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::adhoc::munit {
namespace {

constexpr std::uint32_t kUnitOfVelocity = 0x08D6950Cu;
constexpr std::uint32_t kUnitOfPower = 0x08D69510u;
constexpr std::uint32_t kUnitOfTorque = 0x08D69514u;
constexpr std::uint32_t kUnitOfTemperature = 0x08D69520u;

constexpr std::uint32_t kObjectToIntSlot = 15u;            // Adhoc object: integer value
constexpr std::uint32_t kNewIntegerObject = 0x08A85B1Cu;   // (holder*, value): holder = new integer
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;        // src/core/refcount.cpp
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

void set_unit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t unit) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    if (argc <= 0) return;
    const std::uint32_t argument = rt.memory().load32(ctx.gpr[6]);  // argv[0]; NULL is not checked
    const std::uint32_t to_int = rt.memory().load32(rt.memory().load32(argument) + kObjectToIntSlot * 4u);
    rt.memory().store32(unit, call_guest(rt, ctx, to_int, {argument}));
}

void get_unit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t unit) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t value = frame.at(0u);  // a holder for the new integer object
    (void)call_guest(rt, ctx, kNewIntegerObject, {value, rt.memory().load32(unit)});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    (void)call_guest(rt, ctx, kHolderDestroy, {value});
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
