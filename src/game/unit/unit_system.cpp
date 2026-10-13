// UnitSystem: the engine side and the natives of MUnit, the display units (layout and evidence:
// include/gt/game/unit_system.hpp; names from GT's registration, config/symbol_addrs.txt).
//
// Engine functions take the address of the UnitSystem block (0x08D6950C) in a0. The `get_unit_of_*` ones
// answer the C string a unit prints as; the `get_*_for_display` ones scale a value the game keeps in its
// internal units (m/s, PS, kgfm, m) to the chosen unit; set_unit_system picks a whole set of units at once
// and set_unit_system_by_language picks the set for a language. Registered functions replace the recompiled
// originals; the ones commented out at the end were not compared in any fast or full run of the PR (the
// routes never print that unit), so the gate can't verify them.
//
// Modding: this is where a mod adds a unit (a new text and factor) or changes which units a language gets
// (the table behind set_unit_system_by_language). Keep the original behaviour for the differential gate and
// make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/unit_system.hpp"
#include "guest_call.hpp"
#include "../adhoc/natives.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game::unit_system {
namespace {

using guest::View;
using namespace gt::adhoc;

constexpr std::uint32_t kStrcmp = 0x08B35FD8u;                 // gt::libc::strcmp
constexpr std::uint32_t kLanguageToSystem = 0x08C95494u;       // 14 pairs (language, unit system), then (-1, 0)
constexpr std::uint32_t kLanguageNames = 0x08D69528u;          // 14 pairs (name string, code), filled at startup
constexpr std::uint32_t kLanguageCount = 14u;
constexpr std::uint32_t kDefaultUnitSystem = 1u;               // what 0x08B48BE4 (a constant 1) answers

// The texts the units print as (.rodata).
constexpr std::uint32_t kTextMph = 0x08CB5660u;
constexpr std::uint32_t kTextUnknownVelocity = 0x08CB5664u;    // "VVV"
constexpr std::uint32_t kTextKmh = 0x08CB5668u;
constexpr std::uint32_t kTextPowerFallback = 0x08CB5674u;      // "PPP"
constexpr std::uint32_t kPowerTexts[7] = {
    0x08CB5678u,  // "PS"
    0x08CB567Cu,  // "HP"
    0x08CB5680u,  // "BHP"
    0x08CB5684u,  // "ch"
    0x08CB568Cu,  // "kW"
    0x08CB5688u,  // "CV"
    0x08CB5670u,  // "PF"
};
constexpr std::uint32_t kTextNm = 0x08CB5690u;
constexpr std::uint32_t kTextUnknownTorque = 0x08CB5694u;      // "TTT"
constexpr std::uint32_t kTextKgfm = 0x08CB5698u;
constexpr std::uint32_t kTextFtLb = 0x08CB56A0u;
constexpr std::uint32_t kTextUPerMin = 0x08CB56A8u;            // "U/min"
constexpr std::uint32_t kTextRpm = 0x08CB56B0u;
constexpr std::uint32_t kTextTrPerMin = 0x08CB56B4u;
constexpr std::uint32_t kTextGPerMin = 0x08CB56BCu;
constexpr std::uint32_t kTextDegF = 0x08CB56E8u;               // "deg.F"
constexpr std::uint32_t kTextK = 0x08CB56F0u;                  // "K"
constexpr std::uint32_t kTextDegC = 0x08CB56F4u;               // "deg.C"

// Conversion factors, as the PSP's float constants (the bits are what the originals load).
constexpr float kKmhPerMetrePerSecond = std::bit_cast<float>(0x40666666u);  // 3.6
constexpr float kMphPerKmh = std::bit_cast<float>(0x3F1F136Au);             // 0.62139
constexpr float kHorsepowerPerPs = std::bit_cast<float>(0x3F7C76F3u);       // 0.98619 (HP, BHP)
constexpr float kKilowattPerPs = std::bit_cast<float>(0x3F3C49BAu);         // 0.7355
constexpr float kFtLbPerKgfm = std::bit_cast<float>(0x40E773EBu);           // 7.2329
constexpr float kNmPerKgfm = std::bit_cast<float>(0x411CCCCDu);             // 9.8
constexpr float kFeetPerMetre = std::bit_cast<float>(0x4051F8A1u);          // 3.2808
constexpr float kKmPerMile = std::bit_cast<float>(0x3FCDF3B6u);             // 1.609
constexpr float kHalf = 0.5f;

// The FPU's multiply: inf * 0 is the PSP's default NaN (0x7FC00000), not the host's (0xFFC00000).
float psp_mul(float a, float b) {
    if ((std::isinf(a) && b == 0.0f) || (std::isinf(b) && a == 0.0f)) return std::bit_cast<float>(0x7FC00000u);
    return a * b;
}

View<UnitSystem> units_of(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return View<UnitSystem>(rt.memory(), ctx.gpr[4]);
}

// The result of a display function: the value stays in $f12 and is returned in $f0.
void return_value(psprecomp::AllegrexContext &ctx, float value) {
    ctx.fpr[12] = value;
    ctx.fpr[0] = value;
}

} // namespace

// --- Texts -----------------------------------------------------------------------------------------

// MUnit::GetUnitOfVelocity (0x08A26548) and GetVelocityString (0x08A265F8).
void get_unit_of_velocity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).velocity();
    ctx.gpr[2] = unit == 0u ? kTextKmh : unit == 1u ? kTextMph : kTextUnknownVelocity;
}

// MUnit::GetUnitOfPower (0x08A26498).
void get_unit_of_power(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).power();
    ctx.gpr[2] = unit < 7u ? kPowerTexts[unit] : kTextPowerFallback;
}

// MUnit::GetUnitOfTorque (0x08A263E8).
void get_unit_of_torque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).torque();
    ctx.gpr[2] = unit == 1u ? kTextFtLb : unit == 2u ? kTextNm : unit == 0u ? kTextKgfm : kTextUnknownTorque;
}

// MUnit::GetUnitOfRpm (0x08A26290).
void get_unit_of_rpm(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    switch (units_of(rt, ctx).rpm()) {
    case 0u: ctx.gpr[2] = kTextRpm; break;
    case 1u: ctx.gpr[2] = kTextTrPerMin; break;
    case 2u: ctx.gpr[2] = kTextGPerMin; break;
    case 3u: ctx.gpr[2] = kTextUPerMin; break;
    default: ctx.gpr[2] = kTextUnknownTorque; break;  // the shared "TTT"
    }
}

// MUnit::GetUnitOfTemperature (0x08A261E0).
void get_unit_of_temperature(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).temperature();
    ctx.gpr[2] = unit == 1u ? kTextDegC : unit == 2u ? kTextDegF : unit == 0u ? kTextK : kTextUnknownTorque;
}

// --- Values for display: (units, $f12 = the value in internal units) -> $f0 -----------------------------

// MUnit::GetVelocityForDisplay (0x08A25A9C) and GetVelocityString: m/s to km/h or mph.
void get_velocity_for_display(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).velocity();
    float value = ctx.fpr[12];
    if (unit == 0u) {
        value = psp_mul(value, kKmhPerMetrePerSecond);
    } else if (unit == 1u) {
        const float kmh = psp_mul(value, kKmhPerMetrePerSecond);
        value = psp_mul(kmh, kMphPerKmh);
    }
    return_value(ctx, value);
}

// MUnit::GetPowerForDisplay (0x08A25B9C): PS to HP/BHP (units 1, 2) or kW (unit 4); the others are metric
// horsepower and stay as they are.
void get_power_for_display(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto unit = static_cast<std::int32_t>(units_of(rt, ctx).power());
    float value = ctx.fpr[12];
    if (unit > 0) {
        if (unit < 3) value = psp_mul(value, kHorsepowerPerPs);
        else if (unit == 4) value = psp_mul(value, kKilowattPerPs);
    }
    return_value(ctx, value);
}

// MUnit::GetTorqueForDisplay (0x08A25C7C) and GetTorqueForDisplayF (0x08A25D14): kgfm to ft-lb or Nm.
void get_torque_for_display(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t unit = units_of(rt, ctx).torque();
    float value = ctx.fpr[12];
    if (unit == 1u) value = psp_mul(value, kFtLbPerKgfm);
    else if (unit == 2u) value = psp_mul(value, kNmPerKgfm);
    return_value(ctx, value);
}

// MUnit::GetCourseHeightString (0x08A26744): metres to feet when the velocity unit is mph.
void get_course_height_for_display(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float value = ctx.fpr[12];
    if (units_of(rt, ctx).velocity() == 1u) value = psp_mul(value, kFeetPerMetre);
    return_value(ctx, value);
}

// MUnit::GetCourseLengthString (0x08A26940): (units, length, from) -> the length in the shown unit, rounded
// (+0.5, truncated). `from` is the unit the length is given in (0 = km, 1 = mile): the shown unit is km when
// the velocity unit is 0, miles otherwise; a length already in the shown unit is returned as it is.
void get_course_length_for_display(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t length = ctx.gpr[5];
    const std::uint32_t from = ctx.gpr[6];
    const bool shown_in_km = units_of(rt, ctx).velocity() == 0u;
    const auto as_float = static_cast<float>(static_cast<std::int32_t>(length));
    float scaled;
    if (shown_in_km) {
        if (from == 0u) { ctx.gpr[2] = length; return; }
        scaled = psp_mul(as_float, kKmPerMile);
    } else {
        if (from == 1u) { ctx.gpr[2] = length; return; }
        scaled = psp_mul(as_float, kMphPerKmh);
    }
    scaled = scaled + kHalf;
    ctx.gpr[5] = ctx.fpu_float_to_word(scaled, 1u);  // trunc.w.s
    ctx.gpr[2] = ctx.gpr[5];
}

// --- Choosing units --------------------------------------------------------------------------------

// set_unit_system(units, system): the set of units for a system number 0..13 (others behave as 0). The
// table is the stores of the original's jump table, 14 rows for systems 0..13 (others behave as 0), one per case: velocity, power, torque, rpm, unk_10,
// temperature. Cases 1, 8, 9 and 10 share a row.
void set_unit_system(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint8_t kRows[14][6] = {
        {0, 4, 2, 0, 0, 0},  // 0, and everything >= 14
        {0, 0, 0, 0, 0, 1},  // 1
        {1, 1, 1, 0, 1, 2},  // 2
        {1, 2, 0, 0, 3, 1},  // 3
        {0, 3, 2, 1, 3, 1},  // 4
        {0, 0, 2, 3, 4, 1},  // 5
        {0, 5, 2, 2, 3, 1},  // 6
        {0, 5, 2, 0, 3, 1},  // 7
        {0, 0, 0, 0, 0, 1},  // 8
        {0, 0, 0, 0, 0, 1},  // 9
        {0, 0, 0, 0, 0, 1},  // 10
        {0, 5, 0, 0, 3, 1},  // 11
        {0, 2, 2, 0, 4, 1},  // 12
        {0, 0, 2, 0, 4, 1},  // 13
    };
    const std::uint32_t system = ctx.gpr[5];
    const std::uint8_t *row = kRows[system < 14u ? system : 0u];
    auto units = units_of(rt, ctx);
    units.velocity() = row[0];
    units.power() = row[1];
    units.torque() = row[2];
    units.rpm() = row[3];
    units.unk_10() = row[4];
    units.temperature() = row[5];
}

// MUnit::SetUnitSystemByLanguage (0x08A25428): (units, language) -> set_unit_system with the system the
// language maps to in the table at 0x08C95494 (a language not in it gets system 1). Ends as a tail call.
void set_unit_system_by_language(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t units = ctx.gpr[4];
    const std::uint32_t language = ctx.gpr[5];
    std::uint32_t system = kDefaultUnitSystem;
    for (std::uint32_t i = 0u; i < kLanguageCount; ++i) {
        const std::uint32_t key = rt.memory().load32(kLanguageToSystem + 8u * i);
        if (key == 0xFFFFFFFFu) break;
        if (key == language) {
            system = rt.memory().load32(kLanguageToSystem + 8u * i + 4u);
            break;
        }
    }
    tail_call(ctx, 0x08B49238u, {units, system});
}

// MUnit::SetUnitSystemByLanguage (0x08A25428): (name) -> the index of the language whose name is `name`
// (strcmp against the 14 names), or -1.
void get_language_index(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t name = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0u; i < kLanguageCount; ++i) {
        if (call_guest(rt, ctx, kStrcmp, {rt.memory().load32(kLanguageNames + 8u * i), name}) == 0u) {
            ctx.gpr[2] = i;
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08B49688: MUnit::IsCalendarTypeDayFirst (0x08A25A48) calls it and tests the result for 1 (the name of its role is an inference from that one use). 0 for system numbers 0, 10, 11, 12
// and 13, 2 for numbers >= 14, 1 otherwise.
void func_08B49688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const std::uint32_t system = ctx.gpr[4];
    if (system >= 14u) ctx.gpr[2] = 2u;
    else if (system == 13u) ctx.gpr[2] = 0u;
    else ctx.gpr[2] = ((1u << system) & 0x1C01u) != 0u ? 0u : 1u;
}

// --- The natives -----------------------------------------------------------------------------------
//
// Setters: native(context, argc, argv) — with at least one argument, the unit becomes argv[0]'s integer
// value. Getters: native(result holder) — the result becomes a new Adhoc integer.

namespace {

View<UnitSystem> global_units(psprecomp::Runtime &rt) { return View<UnitSystem>(rt.memory(), kUnitSystemAddress); }

std::uint32_t first_argument(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(ctx.gpr[6])));  // argv[0]; NULL is not checked
}

void return_unit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t value) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), value);
}

bool has_argument(const psprecomp::AllegrexContext &ctx) { return static_cast<std::int32_t>(ctx.gpr[5]) > 0; }

} // namespace

void MUnit_SetUnitOfVelocity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (has_argument(ctx)) global_units(rt).velocity() = first_argument(rt, ctx);
}
void MUnit_SetUnitOfPower(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (has_argument(ctx)) global_units(rt).power() = first_argument(rt, ctx);
}
void MUnit_SetUnitOfTorque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (has_argument(ctx)) global_units(rt).torque() = first_argument(rt, ctx);
}
void MUnit_SetUnitOfTemperature(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (has_argument(ctx)) global_units(rt).temperature() = first_argument(rt, ctx);
}
void MUnit_GetCurrentUnitOfVelocity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_unit(rt, ctx, global_units(rt).velocity()); }
void MUnit_GetCurrentUnitOfPower(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_unit(rt, ctx, global_units(rt).power()); }
void MUnit_GetCurrentUnitOfTorque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_unit(rt, ctx, global_units(rt).torque()); }
void MUnit_GetCurrentUnitOfTemperature(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_unit(rt, ctx, global_units(rt).temperature()); }

} // namespace gt::game::unit_system

namespace unit_system_registrations {
using gt::ReturnKind;
#define UNIT_SYSTEM(address, name, kind) GT_DECOMP_NESTED(address, gt::game::unit_system::name, ReturnKind::kind)

UNIT_SYSTEM(0x08A252E8u, MUnit_SetUnitOfVelocity, Void);
UNIT_SYSTEM(0x08A25338u, MUnit_SetUnitOfPower, Void);
UNIT_SYSTEM(0x08A25388u, MUnit_SetUnitOfTorque, Void);
UNIT_SYSTEM(0x08A253D8u, MUnit_SetUnitOfTemperature, Void);
UNIT_SYSTEM(0x08A25B54u, MUnit_GetCurrentUnitOfVelocity, Void);
UNIT_SYSTEM(0x08A25C34u, MUnit_GetCurrentUnitOfPower, Void);
UNIT_SYSTEM(0x08A25DA4u, MUnit_GetCurrentUnitOfTorque, Void);
UNIT_SYSTEM(0x08A25DECu, MUnit_GetCurrentUnitOfTemperature, Void);

UNIT_SYSTEM(0x08B48C30u, get_unit_of_power, Word);
UNIT_SYSTEM(0x08B48F04u, get_power_for_display, Float);
UNIT_SYSTEM(0x08B48F54u, get_torque_for_display, Float);
UNIT_SYSTEM(0x08B491A8u, get_course_height_for_display, Float);
UNIT_SYSTEM(0x08B491D0u, get_course_length_for_display, Word);
// Verified for system 1 only: the routes never select another, so rows 0 and 2..13, the default and the >= 14 branch were
// never run by the gate (rows were extracted from the original's jump-table stores, not compared).
UNIT_SYSTEM(0x08B49238u, set_unit_system, Void);
// Verified only for the languages the routes pass (the lookup in the table at 0x08C95494 was compared for those entries
// alone, not for the other languages nor for a language missing from it).
UNIT_SYSTEM(0x08B49414u, set_unit_system_by_language, Void);
// Verified only for the names the routes pass (a match among the 14 names and the -1 result for no match were
// compared only as far as the routes reach them).
UNIT_SYSTEM(0x08B497A0u, get_language_index, Word);
// Not registered: compared 0 in every fast and full run of the PR (no pad route prints that
// unit or asks the calendar type), so nothing could verify them; they run as the recompiled originals.
// UNIT_SYSTEM(0x08B48BFCu, get_unit_of_velocity, Word);
// UNIT_SYSTEM(0x08B48CB8u, get_unit_of_torque, Word);
// UNIT_SYSTEM(0x08B48CFCu, get_unit_of_rpm, Word);
// UNIT_SYSTEM(0x08B48DCCu, get_unit_of_temperature, Word);
// UNIT_SYSTEM(0x08B48EACu, get_velocity_for_display, Float);
// UNIT_SYSTEM(0x08B49688u, func_08B49688, Word);
#undef UNIT_SYSTEM
} // namespace unit_system_registrations
