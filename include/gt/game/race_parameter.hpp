// RaceParameter: the settings of one race that the Adhoc scripts fill in before it starts (MRaceParameter
// in script) — race type, laps or time limit, entries, penalties, launch, boost, delayed start.
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence for every name below. GT registers MRaceParameter's script attributes by name, each with one
// handler that both reads and writes it (registration at 0x08847D64.., names at 0x08C9CC0C..,
// register function 0x08B878E8). Each handler converts the script value and calls one engine setter
// (write) or getter (read) on the engine object; those engine functions read or write one field of the
// struct below, so the field takes the attribute's name. For example, the handler of `race_type`
// (0x08848EB8) calls 0x088D5A24 to write and 0x088D62FC to read, and both touch the big-endian half
// at +0x04. Methods (register function 0x08B87108) name the rest: MRaceParameter::setLaunchSpeed
// (0x088471E4) -> 0x088D4E3C -> launch_speed[]. Fields without such evidence keep `unk_XX` names.
//
// Most numbers are big-endian (be16/be32): the struct is what MRaceParameter::pack sends to the other
// players online. Getters of the signed ones sign-extend; the code in src/game/race/race_parameter.cpp
// does it explicitly where the original does.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// Per-side boost settings (MRaceParameter::setBoostParams(a0, a1, b0, b1, c0, c1) writes value 0, 1, 2
// of side 0 and side 1 through 0x088D4E68, 0x088D4EE4, 0x088D4F48).
struct BoostParams : guest::Layout {
    GT_ARRAY(std::uint8_t, value, 0x0, 1, 3);
};

struct RaceParameter : guest::Layout {
    GT_FIELD(guest::be16, race_type, 0x04);          // attribute race_type: 0x088D5A24 / 0x088D62FC
    GT_FIELD(guest::be16, start_type, 0x06);         // attribute start_type: 0x088D5A14 / 0x088D6358
    GT_FIELD(guest::be16, complete_type, 0x08);      // attribute complete_type: 0x088D5A04 / 0x088D63D4
    GT_FIELD(guest::be16, finish_type, 0x0A);        // attribute finish_type: 0x088D59F4 / 0x088D6204
    GT_FIELD(guest::be16, race_limit_laps, 0x0C);    // attribute race_limit_laps: 0x088D5AB8 / 0x088D5D98
    GT_FIELD(guest::be16, race_limit_minute, 0x0E);  // attribute race_limit_minute: 0x088D5AA8 / 0x088D5D88
    GT_FIELD(guest::be32, time_to_start, 0x10);      // attribute time_to_start: 0x088D5958 / 0x088D5E70
    GT_FIELD(guest::be32, time_to_finish, 0x14);     // attribute time_to_finish: 0x088D5948 / 0x088D5938
    GT_FIELD(guest::be16, over_entry_max, 0x18);     // attribute over_entry_max: 0x088D5A8C / 0x088D5DF8
    GT_FIELD(guest::be16, unk_1A, 0x1A);             // caps racers_max's getter (0x088D5DA8)
    GT_FIELD(guest::be32, class_id, 0x1C);           // attribute class_id: 0x088D5968 / 0x088D5D44
    GT_FIELD(std::uint8_t, course_layout_no, 0x24);  // attribute course_layout_no: 0x088D4C78 / 0x088D4C6C
    GT_FIELD(std::int8_t, lucky_slot_id, 0x25);      // attribute lucky_slot_id: 0x088D4C90 / 0x088D4C84
    GT_FIELD(guest::be16, packet_timeout_interval, 0x30);  // attributes packet_timeout_*: one setter
    GT_FIELD(guest::be16, packet_timeout_latency, 0x32);   // 0x088D5AC8 (interval, latency, lag) and one
    GT_FIELD(guest::be16, packet_timeout_lag, 0x34);       // getter 0x088D5D54 for the three
    GT_FIELD(guest::be16, racers_max, 0x36);         // attribute racers_max: 0x088D5A70 / 0x088D5DA8
    GT_FIELD(std::int8_t, penalty_level, 0x48);      // attribute penalty_level: 0x088D4CC4 / 0x088D4CB8
    GT_FIELD(std::int8_t, auto_start_with_session, 0x49);       // 0x088D4C0C / 0x088D4BFC
    GT_FIELD(std::int8_t, auto_end_session_with_finish, 0x4A);  // 0x088D4C28 / 0x088D4C18
    GT_FIELD(std::int8_t, immediate_finish, 0x4B);   // attribute immediate_finish: 0x088D4C44 / 0x088D4C34
    GT_FIELD(std::int8_t, online_on, 0x4C);          // attribute online_on: 0x088D4CAC / 0x088D4C9C
    GT_FIELD(std::int8_t, endless, 0x4D);            // attribute endless: 0x088D4C60 / 0x088D4C50
    GT_FIELD(std::int8_t, with_ghost, 0x4F);         // attribute with_ghost: 0x088D4DAC / 0x088D4D9C
    GT_FIELD(std::int8_t, grid_sort_type, 0x50);     // attribute grid_sort_type: 0x088D4CD0 / 0x088D4CDC
    GT_FIELD(std::int8_t, accumulation, 0x51);       // attribute accumulation: 0x088D4CF8 / 0x088D4CE8
    GT_FIELD(std::int8_t, disable_collision, 0x55);  // attribute disable_collision: 0x088D4DE4 / 0x088D4DD4
    GT_FIELD(std::int8_t, penalty_condition, 0x56);  // attribute penalty_condition: 0x088D4DFC / 0x088D4DF0
    GT_FIELD(std::int8_t, academy_event, 0x57);      // attribute academy_event: 0x088D500C / 0x088D4FFC
    // MRaceParameter::getDelayStartSecond(i) (0x0884BDA8) -> 0x088D4D14: one per entry, up to 8 (the
    // next field is at +0x60).
    GT_ARRAY(std::int8_t, delay_start_second, 0x58, 1, 8);
    GT_FIELD(std::uint8_t, use_launch_data, 0x60);   // MRaceParameter::useLaunchData -> 0x088D4E18
    // MRaceParameter::setLaunchSpeed(i, v) (0x088471E4) -> 0x088D4E3C; up to 5 (the next field is at +0x66).
    GT_ARRAY(std::uint8_t, launch_speed, 0x61, 1, 5);
    // MRaceParameter::setLaunchPosition(i, v) (0x088472C0) -> 0x088D59A0; up to 4 (boost starts at +0x6E).
    GT_ARRAY(guest::be16, launch_position, 0x66, 2, 4);
    // MRaceParameter::setBoostParams: side 0 at +0x6E, side 1 at +0x72 (see BoostParams).
    GT_ARRAY(BoostParams, boost_params, 0x6E, 4, 2);
    GT_FIELD(std::int8_t, rolling_player_grid, 0x71);  // attribute rolling_player_grid: 0x088D4FB4 / 0x088D4FA8
    // MRaceParameter::setEventV(i, v) (0x088477A0) -> 0x088D5988; count not established.
    GT_ARRAY(guest::be16, event_v, 0x7A, 2, 1);
};

// The engine object the script's MRaceParameter wraps (0x08846E50 returns it): a handle whose first word
// points to the RaceParameter.
struct RaceParameterHandle : guest::Layout {
    GT_FIELD(guest::Pointer<RaceParameter>, data, 0x0);
};

} // namespace gt::game
