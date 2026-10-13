// CarParameter: the description of one car that the Adhoc scripts pass around (MCarParameter in script):
// which car (code), colour, tuning, tires, and the specs the garage and the dealer show (power, torque,
// weight, PP). Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence for every name below. MCarParameter's methods are registered by 0x08B87108 and its attributes
// by 0x08B878E8 (initializer 0x08854EB4; names in .rodata). A script attribute or method has one handler
// (`MCarParameter::<name>` in config/symbol_addrs.txt) that calls the engine function this struct is
// read and written by, so the field takes the attribute's or method's name. Example: the handler of
// attribute `team_id` (0x088561B0) calls 0x088A5950 to write and 0x088A5944 to read, and both touch the
// signed byte at +0x2D of the data. Fields without such evidence keep `unk_XX` names.
//
// Object model, from the engine functions' own accesses: the script object's first word points to the
// engine handle (0x08854678), the handle's first word to CarParameterData (every accessor starts
// `lw v0, 0(a0)`), and the data holds three 384-byte blocks (the loops of 0x088A599C run over exactly
// three), one of which is "selected" by the signed byte at +0x08: every per-car accessor indexes the
// block with it (`block = data + 0x38 + index * 384`).
//
// GT keeps many numbers big-endian (be16/be32); the getters of the signed ones sign-extend, which
// src/game/car/car_parameter.cpp does where the original does.
// Type names: CarParameter comes from GT's script class MCarParameter. CarParameterData, CarBlock,
// CarParameterSpec and CarParameterHandle are ours: they name structural roles (the data, a per-car
// block, the spec it points to, the handle), not GT strings.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// One of the three car slots in CarParameterData (a block is 384 bytes: the stride of every access).
// Offsets here are relative to the block start (data + 0x38 + index * 384). The original addresses them
// from data + index * 384 with 56 more, e.g. quicktune_power at displacement 402 = 0x15A here.
struct CarBlock : guest::Layout {
    // The 64-bit car code (MCarParameter::getCarCode -> 0x088A5EE0 -> 0x08932558 reads the 64 bits at the
    // block start). MCarParameter::isVacant (0x088A6028) is true when both words are 0xFFFFFFFF.
    GT_FIELD(std::uint32_t, car_code_low, 0x00);
    GT_FIELD(std::uint32_t, car_code_high, 0x04);
    GT_FIELD(std::uint8_t, quicktune_maxspeed, 0x122);  // attribute quicktune_maxspeed: 0x088A6364 / 0x088A6344
    // attribute quicktune_tcs: stored as 11 - level, 0 when the level is 0 (0x088A6F7C / 0x088A63A0).
    GT_FIELD(std::uint8_t, quicktune_tcs, 0x14E);
    GT_FIELD(std::uint16_t, quicktune_weight, 0x158);   // attribute quicktune_weight: 0x088A6324 / 0x088A6304 (unsigned)
    GT_FIELD(std::int16_t, quicktune_power, 0x15A);     // attribute quicktune_power: 0x088A62E4 / 0x088A62C4 (signed)
    GT_FIELD(std::uint8_t, quicktune_chassis_performance, 0x160);  // 0x088A63F4 / 0x088A63D4
};

// The numbers the garage and dealer show for the car (the object CarParameterData::spec points to). The
// struct has no GT name; only the fields read by getPower, getTorque, getWeight, getPWR and getPP are
// named.
struct CarParameterSpec : guest::Layout {
    GT_FIELD(std::int16_t, weight, 0x1A70);     // MCarParameter::getWeight -> 0x088A5A90 (sign-extended half)
    GT_FIELD(float, power, 0x1B30);             // MCarParameter::getPower -> 0x088A5A48 (first output)
    GT_FIELD(float, torque, 0x1B34);            // MCarParameter::getTorque -> 0x088A5A6C (first output)
    GT_FIELD(std::uint16_t, unk_1B38, 0x1B38);  // getPower's second output
    GT_FIELD(std::uint16_t, unk_1B3A, 0x1B3A);  // getTorque's second output
    GT_FIELD(float, pp, 0x1CAC);                // MCarParameter::getPP / getGTPerformanceIndex -> 0x088A5AE4
};

struct CarParameterData : guest::Layout {
    // Selects the block (signed): the index every per-car accessor uses (0x088A5EE0 and others). No GT name
    // for it, so it keeps its offset.
    GT_FIELD(std::int8_t, unk_08, 0x08);
    GT_FIELD(std::int8_t, target, 0x2C);           // attribute target: 0x088A5938 / 0x088A5928 (non-zero test)
    GT_FIELD(std::int8_t, team_id, 0x2D);          // attribute team_id: 0x088A5950 / 0x088A5944
    GT_FIELD(std::int8_t, race_class_id, 0x2E);    // attribute race_class_id: 0x088A5968 / 0x088A595C
    GT_FIELD(guest::be32, quality_flag, 0x30);     // attribute quality_flag: 0x088A5974 / 0x088A746C
    // Three blocks (MCarParameter::setCarCode's loops 0x088A58A4 and 0x088A5820 visit blocks 1 and 2 from
    // block 0; 0x088A599C visits all three). They end at 0x4B8, where obtain_id starts.
    GT_ARRAY(CarBlock, blocks, 0x38, 384, 3);
    GT_FIELD(guest::be16, obtain_id, 0x4B8);       // attribute obtain_id: 0x088A5D3C / 0x088A75F8
    GT_FIELD(guest::be16, power_scratch, 0x4BA);   // MCarParameter::getPowerScratch -> 0x088A7608
    GT_FIELD(guest::be16, pp_scratch, 0x4BC);      // MCarParameter::getPPScratch / getPP1000Scratch -> 0x088A7618
    GT_FIELD(guest::be16, tuner_id, 0x4BE);        // MCarParameter::getTunerID -> 0x088A5804
    // Released by MCarParameter::endInfo (0x088A5A0C, unless unk_4CC is set) and cleared.
    GT_FIELD(guest::Pointer<CarParameterSpec>, spec, 0x4C8);
    GT_FIELD(std::uint8_t, unk_4CC, 0x4CC);        // endInfo does nothing when it is set
    // Read by MCarParameter::isTargetLocal (0x0885600C) through 0x088A5984: the car is local when it is 0.
    // No GT name for it.
    GT_FIELD(std::uint32_t, unk_4D0, 0x4D0);
};

// The engine object the script's MCarParameter wraps (0x08854678 returns it): a handle whose first word
// points to the CarParameterData.
struct CarParameterHandle : guest::Layout {
    GT_FIELD(guest::Pointer<CarParameterData>, data, 0x0);
};

} // namespace gt::game
