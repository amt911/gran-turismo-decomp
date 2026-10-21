// RaceParameter: the engine side of MRaceParameter, the race settings the Adhoc scripts fill in before a
// race (layout and evidence: include/gt/game/race_parameter.hpp; design:
// docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md).
//
// Every function takes the RaceParameterHandle (the engine object the script wraps) in a0 and works on
// the RaceParameter its first word points to. Names are GT's: the script attribute or method whose
// handler calls the function (config/symbol_addrs.txt lists each address with that evidence).
//
// Registered functions replace the recompiled originals in the game; the ones listed as not registered at
// the end aren't verified yet and don't run (the originals do).
//
// Modding: the defaults a race starts with are what the scripts write through these setters; a mod that
// wants, say, every race endless or without collisions can force the value in the setter (or in the
// getter, for races the scripts don't configure). Keep the original behaviour for the differential gate
// and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/race_parameter.hpp"

#include <cstdint>

namespace gt::game::race_parameter {
namespace {

using guest::View;

// The RaceParameter of the handle in a0.
View<RaceParameter> data(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return *View<RaceParameterHandle>(rt.memory(), ctx.gpr[4]).data();
}

// A flag's getter answers whether the signed byte is positive (`0 < (s8)value` in the original).
std::uint32_t positive(std::int8_t value) { return value > 0 ? 1u : 0u; }
// Big-endian halves read back signed: the original swaps the word and shifts it right arithmetically.
std::uint32_t sign_extended(std::uint16_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
}
std::uint32_t sign_extended(std::int8_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)); }

// racers_max as the race uses it: one more with a ghost, capped by unk_1A (0x088D5DA8).
std::uint32_t effective_racers_max(const View<RaceParameter> &race) {
    const std::uint32_t racers = race.racers_max() + positive(race.with_ghost());
    const std::uint32_t cap = race.unk_1A();
    return cap < racers ? cap : racers;
}

} // namespace

// --- Script attributes: one setter (a1 = value) and one getter (-> v0) each -------------------------

void set_race_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).race_type() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void race_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).race_type()); }
void set_start_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).start_type() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void start_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).start_type()); }
void set_complete_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).complete_type() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void complete_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).complete_type());
}
void set_finish_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).finish_type() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void finish_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).finish_type()); }

void set_race_limit_laps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).race_limit_laps() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void race_limit_laps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).race_limit_laps(); }
void set_race_limit_minute(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).race_limit_minute() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void race_limit_minute(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).race_limit_minute(); }

// complete_laps is how the scripts set a lap race: the laps go to race_limit_laps and complete_type
// becomes 0 (by laps) — or 2 when the laps are 0. Reading it gives the laps only for complete_type 0.
void set_complete_laps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceParameter> race = data(rt, ctx);
    race.race_limit_laps() = static_cast<std::uint16_t>(ctx.gpr[5]);
    race.complete_type() = ctx.gpr[5] != 0u ? 0u : 2u;
}
void complete_laps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceParameter> race = data(rt, ctx);
    ctx.gpr[2] = race.complete_type() != 0u ? 0u : race.race_limit_laps();
}

void set_time_to_start(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data(rt, ctx).time_to_start() = ctx.gpr[5]; }
void time_to_start(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).time_to_start(); }
void set_time_to_finish(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data(rt, ctx).time_to_finish() = ctx.gpr[5]; }
void time_to_finish(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).time_to_finish(); }
void set_class_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data(rt, ctx).class_id() = ctx.gpr[5]; }
void class_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).class_id(); }

// racers_max and over_entry_max: at most 4 (5 or more is stored as 4).
void set_racers_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).racers_max() = static_cast<std::uint16_t>(ctx.gpr[5] < 5u ? ctx.gpr[5] : 4u);
}
void racers_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = effective_racers_max(data(rt, ctx)); }
void set_over_entry_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).over_entry_max() = static_cast<std::uint16_t>(ctx.gpr[5] < 5u ? ctx.gpr[5] : 4u);
}
// over_entry_max (one more with a ghost) when it reaches racers_max in an online race, else racers_max.
void over_entry_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceParameter> race = data(rt, ctx);
    const std::uint32_t over = race.over_entry_max() + positive(race.with_ghost());
    const std::uint32_t racers = effective_racers_max(race);
    ctx.gpr[2] = over < racers || race.online_on() <= 0 ? racers : over;
}

void set_course_layout_no(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).course_layout_no() = static_cast<std::uint8_t>(ctx.gpr[5]);
}
void course_layout_no(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).course_layout_no(); }
void set_lucky_slot_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).lucky_slot_id() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void lucky_slot_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).lucky_slot_id()); }
void set_penalty_level(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).penalty_level() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void penalty_level(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).penalty_level()); }
void set_grid_sort_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).grid_sort_type() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void grid_sort_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).grid_sort_type()); }
void set_penalty_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).penalty_condition() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void penalty_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).penalty_condition());
}
void set_rolling_player_grid(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).rolling_player_grid() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void rolling_player_grid(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).rolling_player_grid());
}

// Flags: set to a1's low byte; read as "positive".
#define GT_RACE_FLAG(name)                                                                                \
    void set_##name(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {                           \
        data(rt, ctx).name() = static_cast<std::int8_t>(ctx.gpr[5]);                                    \
    }                                                                                                     \
    void name(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data(rt, ctx).name()); }
GT_RACE_FLAG(auto_start_with_session)
GT_RACE_FLAG(auto_end_session_with_finish)
GT_RACE_FLAG(immediate_finish)
GT_RACE_FLAG(online_on)
GT_RACE_FLAG(endless)
GT_RACE_FLAG(with_ghost)
GT_RACE_FLAG(accumulation)
GT_RACE_FLAG(disable_collision)
GT_RACE_FLAG(academy_event)
#undef GT_RACE_FLAG

// packet_timeout_interval / _latency / _lag share one setter (a1, a2, a3) and one getter that writes the
// three into the halves a1, a2, a3 point to.
void set_packet_timeouts(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceParameter> race = data(rt, ctx);
    race.packet_timeout_interval() = static_cast<std::uint16_t>(ctx.gpr[5]);
    race.packet_timeout_latency() = static_cast<std::uint16_t>(ctx.gpr[6]);
    race.packet_timeout_lag() = static_cast<std::uint16_t>(ctx.gpr[7]);
}
void packet_timeouts(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceParameter> race = data(rt, ctx);
    rt.memory().store16(ctx.gpr[5], race.packet_timeout_interval());
    rt.memory().store16(ctx.gpr[6], race.packet_timeout_latency());
    rt.memory().store16(ctx.gpr[7], race.packet_timeout_lag());
}

// --- Methods' engine functions ----------------------------------------------------------------------

// MRaceParameter::useLaunchData(bool).
void set_use_launch_data(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).use_launch_data() = static_cast<std::uint8_t>(ctx.gpr[5]);
}
// MRaceParameter::setLaunchSpeed(i, v) / setLaunchPosition(i, v) / setEventV(i, v).
void set_launch_speed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).launch_speed(ctx.gpr[5]) = static_cast<std::uint8_t>(ctx.gpr[6]);
}
void set_launch_position(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).launch_position(ctx.gpr[5]) = static_cast<std::uint16_t>(ctx.gpr[6]);
}
void set_event_v(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).event_v(ctx.gpr[5]) = static_cast<std::uint16_t>(ctx.gpr[6]);
}
// MRaceParameter::getDelayStartSecond(i).
void delay_start_second(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).delay_start_second(ctx.gpr[5]));
}
// MRaceParameter::setBoostParams: value k of a side; side 1 is the second set, any other side the first.
template <unsigned Value>
void set_boost_param(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).boost_params(ctx.gpr[5] == 1u ? 1u : 0u).value(Value) = static_cast<std::uint8_t>(ctx.gpr[6]);
}

} // namespace gt::game::race_parameter

namespace race_parameter_registrations {
using gt::ReturnKind;
// Attributes (setter, getter) — the handler addresses in include/gt/game/race_parameter.hpp.
GT_DECOMP_NESTED(0x088D5A24u, gt::game::race_parameter::set_race_type, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D62FCu, gt::game::race_parameter::race_type, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5A14u, gt::game::race_parameter::set_start_type, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D6358u, gt::game::race_parameter::start_type, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5A04u, gt::game::race_parameter::set_complete_type, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D63D4u, gt::game::race_parameter::complete_type, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D59F4u, gt::game::race_parameter::set_finish_type, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D5AB8u, gt::game::race_parameter::set_race_limit_laps, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D5D98u, gt::game::race_parameter::race_limit_laps, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5D88u, gt::game::race_parameter::race_limit_minute, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5910u, gt::game::race_parameter::complete_laps, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5E70u, gt::game::race_parameter::time_to_start, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5A70u, gt::game::race_parameter::set_racers_max, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D5DA8u, gt::game::race_parameter::racers_max, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D5DF8u, gt::game::race_parameter::over_entry_max, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4C6Cu, gt::game::race_parameter::course_layout_no, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4C84u, gt::game::race_parameter::lucky_slot_id, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4CC4u, gt::game::race_parameter::set_penalty_level, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D4CB8u, gt::game::race_parameter::penalty_level, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4CDCu, gt::game::race_parameter::grid_sort_type, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4FA8u, gt::game::race_parameter::rolling_player_grid, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4BFCu, gt::game::race_parameter::auto_start_with_session, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4C9Cu, gt::game::race_parameter::online_on, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4C50u, gt::game::race_parameter::endless, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4D9Cu, gt::game::race_parameter::with_ghost, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4DD4u, gt::game::race_parameter::disable_collision, ReturnKind::Word);
// Methods.
GT_DECOMP_NESTED(0x088D4E18u, gt::game::race_parameter::set_use_launch_data, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D4E3Cu, gt::game::race_parameter::set_launch_speed, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D59A0u, gt::game::race_parameter::set_launch_position, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D5988u, gt::game::race_parameter::set_event_v, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D4D14u, gt::game::race_parameter::delay_start_second, ReturnKind::Word);
GT_DECOMP_NESTED(0x088D4E68u, gt::game::race_parameter::set_boost_param<0>, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D4EE4u, gt::game::race_parameter::set_boost_param<1>, ReturnKind::Void);
GT_DECOMP_NESTED(0x088D4F48u, gt::game::race_parameter::set_boost_param<2>, ReturnKind::Void);
// Not registered yet: the three pad routes never call these (compared 0, calls 0), so the gate can't
// check them; they run as the recompiled originals until a route reaches them. Mostly setters of
// attributes the scripts leave at their defaults in those states.
// GT_DECOMP(0x088D6204u, gt::game::race_parameter::finish_type, ReturnKind::Word);
// GT_DECOMP(0x088D5AA8u, gt::game::race_parameter::set_race_limit_minute, ReturnKind::Void);
// GT_DECOMP(0x088D5A34u, gt::game::race_parameter::set_complete_laps, ReturnKind::Void);
// GT_DECOMP(0x088D5958u, gt::game::race_parameter::set_time_to_start, ReturnKind::Void);
// GT_DECOMP(0x088D5948u, gt::game::race_parameter::set_time_to_finish, ReturnKind::Void);
// GT_DECOMP(0x088D5938u, gt::game::race_parameter::time_to_finish, ReturnKind::Word);
// GT_DECOMP(0x088D5968u, gt::game::race_parameter::set_class_id, ReturnKind::Void);
// GT_DECOMP(0x088D5D44u, gt::game::race_parameter::class_id, ReturnKind::Word);
// GT_DECOMP(0x088D5A8Cu, gt::game::race_parameter::set_over_entry_max, ReturnKind::Void);
// GT_DECOMP(0x088D4C78u, gt::game::race_parameter::set_course_layout_no, ReturnKind::Void);
// GT_DECOMP(0x088D4C90u, gt::game::race_parameter::set_lucky_slot_id, ReturnKind::Void);
// GT_DECOMP(0x088D4CD0u, gt::game::race_parameter::set_grid_sort_type, ReturnKind::Void);
// GT_DECOMP(0x088D4DFCu, gt::game::race_parameter::set_penalty_condition, ReturnKind::Void);
// GT_DECOMP(0x088D4DF0u, gt::game::race_parameter::penalty_condition, ReturnKind::Word);
// GT_DECOMP(0x088D4FB4u, gt::game::race_parameter::set_rolling_player_grid, ReturnKind::Void);
// GT_DECOMP(0x088D4C0Cu, gt::game::race_parameter::set_auto_start_with_session, ReturnKind::Void);
// GT_DECOMP(0x088D4C28u, gt::game::race_parameter::set_auto_end_session_with_finish, ReturnKind::Void);
// GT_DECOMP(0x088D4C18u, gt::game::race_parameter::auto_end_session_with_finish, ReturnKind::Word);
// GT_DECOMP(0x088D4C44u, gt::game::race_parameter::set_immediate_finish, ReturnKind::Void);
// GT_DECOMP(0x088D4C34u, gt::game::race_parameter::immediate_finish, ReturnKind::Word);
// GT_DECOMP(0x088D4CACu, gt::game::race_parameter::set_online_on, ReturnKind::Void);
// GT_DECOMP(0x088D4C60u, gt::game::race_parameter::set_endless, ReturnKind::Void);
// GT_DECOMP(0x088D4DACu, gt::game::race_parameter::set_with_ghost, ReturnKind::Void);
// GT_DECOMP(0x088D4CF8u, gt::game::race_parameter::set_accumulation, ReturnKind::Void);
// GT_DECOMP(0x088D4CE8u, gt::game::race_parameter::accumulation, ReturnKind::Word);
// GT_DECOMP(0x088D4DE4u, gt::game::race_parameter::set_disable_collision, ReturnKind::Void);
// GT_DECOMP(0x088D500Cu, gt::game::race_parameter::set_academy_event, ReturnKind::Void);
// GT_DECOMP(0x088D4FFCu, gt::game::race_parameter::academy_event, ReturnKind::Word);
// GT_DECOMP(0x088D5AC8u, gt::game::race_parameter::set_packet_timeouts, ReturnKind::Void);
// GT_DECOMP(0x088D5D54u, gt::game::race_parameter::packet_timeouts, ReturnKind::Void);
} // namespace race_parameter_registrations
