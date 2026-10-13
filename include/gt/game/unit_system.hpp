// UnitSystem: the display units of the game — speed in km/h or mph, power in PS/HP/kW, torque in
// kgfm/ft-lb/Nm, engine speed, temperature (MUnit in script).
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MUnit has 28 methods (registrar 0x08B87108) and no attributes. Its natives keep the units in six
// consecutive words of .bss at 0x08D6950C: SetUnitOfVelocity / SetUnitOfPower / SetUnitOfTorque /
// SetUnitOfTemperature (0x08A252E8, 0x08A25338, 0x08A25388, 0x08A253D8) store their argument at +0x00, +0x04,
// +0x08 and +0x14, and the GetCurrentUnitOf* getters return those words. Every engine function takes the address
// of this block in a0 (the natives load it with lui 0x08D7 / addiu -27380). A field takes its name from the native
// that sets it or from the native whose engine function reads it (config/symbol_addrs.txt lists each address);
// the field without such a name is unk_10. Struct and namespace names are ours, not GT's.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// Where GT keeps the one UnitSystem (.bss).
inline constexpr std::uint32_t kUnitSystemAddress = 0x08D6950Cu;

struct UnitSystem : guest::Layout {
    // 0 = km/h, 1 = mph (anything else prints "VVV"): UnitSystem::get_unit_of_velocity (0x08B48BFC), read by
    // MUnit::GetUnitOfVelocity (0x08A26548); GetVelocityForDisplay scales by it (0x08B48EAC).
    GT_FIELD(std::uint32_t, velocity, 0x00);
    // Index into the power names PS, HP, BHP, ch, kW, CV, PF (0x08B48C30, read by MUnit::GetUnitOfPower 0x08A26498);
    // GetPowerForDisplay scales by it (0x08B48F04).
    GT_FIELD(std::uint32_t, power, 0x04);
    // 0 = kgfm, 1 = ft-lb, 2 = Nm (0x08B48CB8, read by MUnit::GetUnitOfTorque 0x08A263E8); GetTorqueForDisplay
    // scales by it (0x08B48F54).
    GT_FIELD(std::uint32_t, torque, 0x08);
    // 0 = rpm, 1 = tr/min, 2 = G/min, 3 = U/min (0x08B48CFC, read by MUnit::GetUnitOfRpm 0x08A26290).
    GT_FIELD(std::uint32_t, rpm, 0x0C);
    // Values 0..4, set by set_unit_system and read by 0x08B48D68 and 0x08B48FA0 (their strings and factors are
    // fuel-economy-like, 'l/100 km' and 'km/L'); no MUnit native calls them, so the role is not established.
    GT_FIELD(std::uint32_t, unk_10, 0x10);
    // 0 = K, 1 = deg.C, 2 = deg.F (0x08B48DCC, read by MUnit::GetUnitOfTemperature 0x08A261E0).
    GT_FIELD(std::uint32_t, temperature, 0x14);
};

} // namespace gt::game
