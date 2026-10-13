// CarDriverParameter: how one car is driven — human or AI, the driver aids, the AI level (MCarDriverParameter
// in script).
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MCarDriverParameter has 10 methods (registrar 0x08B87108) and 14 attributes (registrar
// 0x08B878E8), registered by the initializer at 0x0884E9D4: type, user_number, display_name, region, manual,
// assist_asm, assist_tcs, assist_4was, active_brake_level, physics_pro, steering_assist_type,
// active_steering, academy_flag and special_driver_model_identity. The script object holds a handle (0x0884D660
// and 0x0884D3D8 give it); the handle's first word points to the 184-byte data (0x08897B8C allocates 184
// bytes). A field takes the attribute's or method's name when its engine function reads or writes it
// (config/symbol_addrs.txt lists each address); fields without such a name are unk_XX. The two
// big-endian words are the ones MCarDriverParameter's pack sends online. Struct names are ours, not GT's.
#pragma once

#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

struct CarDriverParameterData : guest::Layout {
    // The 140 bytes before +0x8C are cleared by setDefault (0x0889793C) and not otherwise named here.
    GT_FIELD(std::int8_t, manual, 0x8D);                // attribute manual: 0x08896FD0 / 0x08896FC0 (a flag: read as > 0)
    GT_FIELD(std::int8_t, assist_tcs, 0x8E);            // attribute assist_tcs: 0x08896FEC / 0x08896FDC (flag)
    GT_FIELD(std::int8_t, assist_asm, 0x8F);            // attribute assist_asm: 0x08897008 / 0x08896FF8 (flag)
    GT_FIELD(std::int8_t, steering_assist_type, 0x90);  // attribute steering_assist_type: 0x0889703C / 0x08897030
    GT_FIELD(std::int8_t, active_steering, 0x92);       // attribute active_steering: 0x08897054 / 0x08897048
    GT_FIELD(std::int8_t, active_brake_level, 0x93);    // attribute active_brake_level: 0x08897088 / 0x0889707C
    GT_FIELD(std::int8_t, physics_pro, 0x94);           // attribute physics_pro: 0x08897024 / 0x08897014 (flag)
    GT_FIELD(std::int8_t, academy_flag, 0x95);          // attribute academy_flag: 0x088970C8 / 0x088970B8 (flag)
    GT_FIELD(std::int8_t, assist_4was, 0x97);           // attribute assist_4was: 0x08897070 / 0x08897060 (flag)
    // MCarDriverParameter::setRTAUnadjustable ORs its argument into this byte (0x088970A0);
    // getRTAUnadjustable reads it (0x08897094).
    GT_FIELD(std::uint8_t, rta_unadjustable, 0x99);
    GT_FIELD(guest::be32, type, 0x9C);                  // attribute type: 0x08897574 / 0x088977D4; isHuman: type == 0
    GT_FIELD(guest::be32, user_number, 0xA0);           // setPlayer stores it (0x08897554); getControllerPort reads it
    GT_FIELD(std::uint8_t, unk_A8, 0xA8);               // MCarDriverParameter::setAISkill: 0x0889722C stores its second argument, clamped to 10..200
    GT_FIELD(std::uint8_t, unk_AB, 0xAB);               // ... and its first
    GT_FIELD(std::uint8_t, unk_AD, 0xAD);               // 0x088970EC stores (v clamped to 0..100) + 1
    GT_FIELD(std::uint8_t, ai_level, 0xAF);             // MCarDriverParameter::setAILevel: 0x08897290 stores (v clamped to 0..100) + 1
    // attribute special_driver_model_identity (0x08897130 / 0x08897118): a value below 7 goes to +0xB0, a larger
    // one to +0xB1, and the other byte is cleared; the getter answers +0xB1 when it is positive, else +0xB0.
    GT_FIELD(std::int8_t, unk_B0, 0xB0);
    GT_FIELD(std::int8_t, special_driver_model_identity, 0xB1);
};

// The engine object the script's MCarDriverParameter wraps (0x0884D3D8 returns it): a handle whose first
// word points to the CarDriverParameterData.
struct CarDriverParameterHandle : guest::Layout {
    GT_FIELD(guest::Pointer<CarDriverParameterData>, data, 0x0);
};

} // namespace gt::game
