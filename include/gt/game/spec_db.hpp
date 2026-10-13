// SpecDB: GT's read-only database of cars, courses and races (MSpecDB in script) — what the dealer, the
// garage and the course lists show: car price, year, category, course conditions, maker and tuner tables.
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MSpecDB is a class of static natives (registrar 0x08B86FB0; initializer 0x088364D8, the
// natives at 0x0883A3D4..0x08841500). A native takes a car code or a course code, which is a 64-bit key:
// the low word is the id and the high word the table number (MSpecDB::extractID 0x08840D00 returns the low
// word, extractTableNumber 0x08840C7C the high one; CarCode32ToCarCode64 0x0883DA48 appends table 0 and
// CourseCode32ToCourseCode64 0x0883E924 table 34). The key is looked up in the table's database object
// (0x08936584 -> 0x08936544 -> 0x08938CB4), which fills a record. The two records below are what the
// readers of the natives take their value from. Names that are not GT's (the struct names) are ours;
// bytes without a native that reads them are unk_XX.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// The record a car code resolves to (32 bytes: 0x08930D20 copies eight words). Each field is the one the
// engine function of that MSpecDB native reads.
struct CarSpecRecord : guest::Layout {
    GT_FIELD(std::uint32_t, price, 0x08);           // MSpecDB::getCarPrice -> 0x08930E08
    GT_FIELD(std::int16_t, year, 0x0C);             // MSpecDB::getCarYear -> 0x08930E28 (sign-extended)
    GT_FIELD(std::uint8_t, country, 0x12);          // MSpecDB::getCarCountry -> 0x08930E48
    GT_FIELD(std::uint8_t, unk_13, 0x13);           // read with unk_14 by 0x08930EBC
    GT_FIELD(std::uint8_t, unk_14, 0x14);
    GT_FIELD(std::uint8_t, category, 0x15);         // MSpecDB::getCarCategory -> 0x08930D98
    GT_FIELD(std::uint8_t, unk_16, 0x16);           // MSpecDB::canRun's test of the car (0x0892C810)
};

// The record a course code resolves to (28 bytes: 0x089305CC copies seven words).
struct CourseRecord : guest::Layout {
    GT_FIELD(std::uint8_t, condition, 0x14);        // MSpecDB::getCourseCondition -> 0x0893069C; isDirt: 2 or 3
    GT_FIELD(std::uint8_t, max_entry, 0x15);        // MSpecDB::getCourseMaxEntry -> 0x0893067C
};

} // namespace gt::game
