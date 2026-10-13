// RaceOperator: the object that runs the race session on the script side — which cars and drivers sit in
// which slots, the race parameter, the series of courses, the memory plan (MRaceOperator in script).
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MRaceOperator has 58 methods and 11 attributes (registrars 0x08B87108 and 0x08B878E8,
// initializer 0x0882506C). The script object holds the engine object at +0x14 (0x088248E4 gives it, through
// a holder). The engine object (RaceOperatorHandle) is what every engine function of 0x0885BB9C..0x0885D2A0
// takes in a0: its first word points to the data, +4 is the organizer (MRaceOperator::setOrganizer stores
// it, 0x0885BB9C) and +8 a lock (0x0885BD3C takes it). Field names are the attribute or method whose engine
// function reads or writes the field (config/symbol_addrs.txt lists each address); fields without such a
// name are unk_XX. Struct names are ours, not GT's.
#pragma once

#include "gt/game/car_parameter.hpp"
#include "gt/game/race_parameter.hpp"
#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// One participant: 392 bytes (the stride of every access: 0x0885C0E8, 0x0885C188, 0x0885BBF0 ...).
struct RaceOperatorSlot : guest::Layout {
    GT_FIELD(std::uint8_t, unk_16, 0x16);               // 0x0885D4CC: the slot counts for getTargetSlot when set ...
    GT_FIELD(std::uint8_t, unk_17, 0x17);               // ... and this one is clear and the car is not a target
    GT_FIELD(std::int32_t, account_point, 0x18);        // MRaceOperator::setAccountPoint / getAccountPoint
    // MRaceOperator::setAccountPersonalID / getAccountPersonalID: a 64-bit value.
    GT_FIELD(std::uint32_t, account_personal_id_low, 0x38);
    GT_FIELD(std::uint32_t, account_personal_id_high, 0x3C);
    // MRaceOperator::checkValidDriver(slot, i) reads the byte at +0x40 + i; the count of drivers isn't known.
    GT_ARRAY(std::uint8_t, unk_40, 0x40, 1, 0);
    GT_FIELD(CarParameterHandle, unk_50, 0x50);         // 0x0885D4CC calls CarParameter::target on it
};

// A block of the memory plan MRaceOperator::getMemoryAssign_* returns: 28 bytes whose fields are unknown.
struct RaceOperatorMemoryAssign : guest::Layout {
    GT_FIELD(std::uint32_t, unk_00, 0x00);
};

struct RaceOperatorData : guest::Layout {
    GT_FIELD(guest::Pointer<RaceOperatorSlot>, unk_00, 0x00);  // the slots (0x0885C0E8: base + 392 * index)
    GT_FIELD(std::int32_t, unk_04, 0x04);               // how many slots (0x0885BBF0 loops to it; clear zeroes it)
    GT_FIELD(std::uint32_t, unk_08, 0x08);              // cleared by MRaceOperator::clear
    GT_FIELD(std::int32_t, window_max, 0x0C);           // attribute window_max: 0x0885BBB0 / 0x0885BBBC (clear sets 1)
    GT_FIELD(std::int32_t, representative_slot_id, 0x10);  // attribute representative_slot_id: 0x0885C0DC (clear sets -1)
    GT_FIELD(std::uint8_t, expand_memory, 0x14);        // attribute expand_memory: 0x0885C368 / 0x0885C374
    GT_FIELD(std::uint8_t, unk_15, 0x15);               // MRaceOperator::setReplayMode clears it
    GT_FIELD(std::uint8_t, unk_16, 0x16);               // MRaceOperator::setReplayMode stores its argument
    GT_FIELD(std::uint8_t, unk_17, 0x17);               // MRaceOperator::isTheater -> 0x0885C35C reads it (replay mode sets it)
    GT_FIELD(std::uint8_t, with_ghost, 0x18);           // attribute with_ghost: 0x0885BC90 / 0x0885BC9C
    GT_FIELD(std::uint8_t, unk_19, 0x19);               // set when race_build_parameter was given; buildRace then passes +0x88
    GT_FIELD(std::uint32_t, personal_id_low, 0x28);     // attribute personal_id: 0x0885BBC8 / 0x0885BBD8 (clear sets -1, -1)
    GT_FIELD(std::uint32_t, personal_id_high, 0x2C);
    GT_FIELD(std::int32_t, max_series_race, 0x30);      // MRaceOperator::getMaxSeriesRace -> 0x0885BD30
    GT_FIELD(std::int32_t, current_series_race_number, 0x34);  // MRaceOperator::getCurrentSeriesRaceNumber -> 0x0885BD24
    // MRaceOperator::getSeriesCourseCode(i) -> 0x0885BCA8: the 64-bit course code at +0x38 + 8 * i (low, high).
    GT_ARRAY(std::uint32_t, series_course_code_low, 0x38, 8, 0);
    GT_ARRAY(std::uint32_t, series_course_code_high, 0x3C, 8, 0);
    GT_FIELD(std::uint32_t, unk_78, 0x78);              // MRaceOperator::setReplayMode and clear zero it
    GT_FIELD(guest::Pointer<RaceParameter>, race_parameter, 0x7C);  // attribute race_parameter: 0x0885BE8C
    GT_FIELD(std::uint32_t, game_condition, 0x80);      // attribute game_condition (a guest pointer): 0x0885BEA4
    GT_FIELD(std::uint32_t, gadget_condition, 0x84);    // attribute gadget_condition (a guest pointer): 0x0885BEC8
    GT_FIELD(std::uint32_t, race_build_parameter, 0x88);  // attribute race_build_parameter (a guest pointer): 0x0885BF50
    // MRaceOperator::getMemoryAssign_Standard (+0x90), _ExpandMemory (+0xAC), _WithoutReplay (+0xC8),
    // _ForReplay (+0xE4), _WithGhost (+0x100) and _ManyPlayers (+0x11C): six blocks of 28 bytes.
    GT_ARRAY(RaceOperatorMemoryAssign, memory_assign, 0x90, 28, 6);
};

struct RaceOperatorHandle : guest::Layout {
    GT_FIELD(guest::Pointer<RaceOperatorData>, data, 0x00);
    GT_FIELD(std::uint32_t, organizer, 0x04);           // MRaceOperator::setOrganizer
};

} // namespace gt::game
