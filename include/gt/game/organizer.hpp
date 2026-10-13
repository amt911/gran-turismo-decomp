// Organizer: the race/session organizer — it runs a session from entry to finish: the entries and their
// driver and car parameters, the camera, the race display, the replay buffers, the pause state (MOrganizer
// in script).
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MOrganizer has 161 methods (registrar 0x08B87108) and 12 attributes (registrar 0x08B878E8;
// window_max, race_display_window_num, racers_num, sec_to_finish_all_complete, is_host, driver_name_disp,
// driver_debug, crowd_debug, driving_line, pauseHandleAvailable, pauseAvailable, pauseRace), registered by
// the initializer at 0x0882C2A8. The script object's engine object is a handle (0x088294D0 gives it) whose
// first word points to the organizer data, a block of more than 17,000 bytes whose members are objects of
// their own: this file only names the offsets the engine functions of src/game/race/organizer.cpp reach. A
// member takes the name of the method whose engine function reads or writes it; members without one are
// unk_XX. Struct names are ours, not GT's.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// Members that are objects with functions of their own (0x08917xxx, 0x0889xxxx, 0x08865Fxx): only their
// address is used here.
struct OrganizerCamera : guest::Layout {};
struct OrganizerRaceDisplay : guest::Layout {};
struct OrganizerReplayBuffer : guest::Layout {};
struct OrganizerGameCondition : guest::Layout {};
struct OrganizerPause : guest::Layout {};
struct OrganizerUnknownBlock : guest::Layout {};

struct OrganizerData : guest::Layout {
    GT_FIELD(std::uint32_t, unk_8C, 0x8C);                   // 0x0885E498 passes it when its argument is 0
    GT_FIELD(std::uint8_t, rendering_race, 0x9C);            // MOrganizer::enableRenderingRace / disableRenderingRace: 1 / 0
    GT_FIELD(std::uint32_t, unk_5AC, 0x5AC);                 // an object (0x088Dxxxx functions) most queries go through
    GT_FIELD(std::uint32_t, unk_5B4, 0x5B4);                 // the entries' list (0x088F8A14 / 0x088F8CC0)
    GT_FIELD(OrganizerCamera, camera, 0x5CC);                // the camera methods' object (0x08917xxx)
    GT_FIELD(OrganizerRaceDisplay, race_display, 0x600);     // MOrganizer::fetchRaceDisplay and getRaceDisplay* object
    GT_FIELD(std::uint32_t, unk_3F50, 0x3F50);               // MRaceOperator::setOrganizer stores the operator here
    GT_FIELD(OrganizerPause, unk_3F58, 0x3F58);              // the pause attributes' object (0x08865F44..0x08865F7C)
    GT_FIELD(OrganizerReplayBuffer, replay_buffer, 0x3F80);  // MOrganizer::getReplayBuffer / getSaveGhostBuffer
    GT_FIELD(OrganizerGameCondition, game_condition, 0x3F88);  // MOrganizer::getGameCondition / setGameCondition
    GT_FIELD(std::uint32_t, unk_3F98, 0x3F98);               // the sound target: enable/disableSoundingRace pass it to 0x08AB8858
    GT_FIELD(std::uint32_t, unk_3F9C, 0x3F9C);
    GT_FIELD(OrganizerUnknownBlock, unk_45A4, 0x45A4);       // 0x0885FC4C (enterCourse) hands its address to 0x088F27F8
};

// The engine object the script's MOrganizer wraps (0x088294D0 returns it): a handle whose first word points to
// the OrganizerData.
struct OrganizerHandle : guest::Layout {
    GT_FIELD(guest::Pointer<OrganizerData>, data, 0x0);
};

} // namespace gt::game
