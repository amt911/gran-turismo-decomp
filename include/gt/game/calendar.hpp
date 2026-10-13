// Calendar: the game's day counter and event list (MCalendar in script) — the day progression of the
// GT mode, and the events (license tests, courses, garage visits) scheduled on it.
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MCalendar registers one attribute, `today` (handler 0x0881A208, registrar 0x08B878E8), and
// twelve methods (initializer 0x08818A04; instance methods through 0x08B87108, the static ones through
// 0x08B86FB0). The script object holds a pointer to the engine object at +0x14 (0x08817A54 loads it); the
// engine object, below, is what the natives read and write. Names that are not GT's (the struct names,
// the fields marked unk_XX) are ours or unknown.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// One scheduled event. 148 bytes (the stride of every access: 0x08817B10, 0x08817DD0, 0x08818168). Built
// by MCalendar::putRunCourseEvent (0x08818210), putRunLicenseEvent (0x08818264) and putGarageEvent
// (0x088183D4); the layout of its fields isn't decoded yet. The byte at +4 is an index into a 4-entry table
// (0x08817934 looks it up, index < 4); what it selects isn't established.
struct CalendarEvent : guest::Layout {
    GT_FIELD(std::uint8_t, unk_04, 0x04);
};

// The engine object of a Calendar.
struct CalendarData : guest::Layout {
    // The current day, as the day number of 0x08B1B2F4 (year, month, day). attribute `today` decodes it
    // into a date (0x0881799C -> 0x08B1B420); MCalendar::getGameDay returns it minus the first day
    // (0x08817924: 2009-10-01); MCalendar::incDate adds one (0x08817A0C).
    GT_FIELD(std::uint32_t, today, 0x00);
    // The events form a ring of 100 slots from +0x14 (capacity 100: 0x08818168 makes room (0x08B89DAC(deque, 1)) when the
    // count is 100). Indexes: 0x08817DD0 reads slot (i + unk_0C) % 100 for i < event_size; 0x08818168
    // writes at unk_08 and advances it.
    GT_FIELD(std::uint32_t, unk_08, 0x08);
    GT_FIELD(std::uint32_t, unk_0C, 0x0C);
    GT_FIELD(std::int32_t, event_size, 0x10);  // MCalendar::getEventSize
    GT_ARRAY(CalendarEvent, events, 0x14, 148, 100);
};

} // namespace gt::game
