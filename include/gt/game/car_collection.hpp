// CarCollection: the player's garage — which cars they own, in which colours, the recently rode and newly
// gained lists, and the favourites (MCarCollection in script).
// Design: docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md.
//
// Evidence. MCarCollection has 34 methods (registrar 0x08B87108, initializer 0x0881C3E8; the 5 attributes
// `money`, `days`, `give_car_num`, `recv_car_num` and `trade_car_num` registered after them belong to the
// diff-info object getDiffInfo returns, not to the collection). The script object holds the engine object at
// +0x14 (0x0881B068 loads it). The names below are the methods whose engine function reads or writes the
// field; fields without such a method stay unk_XX. Struct names are ours, not GT's.
#pragma once

#include "gt/game/car_parameter.hpp"
#include "gt/guest.hpp"

#include <cstdint>

namespace gt::game {

// One owned car type: 34 bytes (the stride of 0x0881A840 and every access). The 32 bytes from +2 are
// indexed by a colour-like index (0x0881A8C0 reads record[i + 2]): the low nibble is how many the player
// has (0x0881A8C0, summed by 0x0881A8F0), the bits of the high nibble are flags (0x0881A9CC, used by
// MCarCollection::getFlg).
struct CarCollectionRecord : guest::Layout {
    GT_ARRAY(std::uint8_t, unk_02, 0x02, 1, 32);
};

struct CarCollectionData : guest::Layout {
    GT_FIELD(std::uint32_t, unk_00, 0x00);             // the number of records (0x0881A7AC allocates 34 * it)
    GT_FIELD(std::int32_t, my_car_type_count, 0x04);   // MCarCollection::getMyCarTypeCount
    GT_FIELD(std::int32_t, my_all_car_count, 0x08);    // MCarCollection::getMyAllCarCount
    GT_FIELD(guest::Pointer<CarCollectionRecord>, unk_30, 0x30);  // the records, indexed by car (0x0881A840)
    // Two lists of 100 four-byte entries (a 16-bit car index, a byte at +2): the recently rode cars
    // (getRecentlyRodeCars -> 0x0881ACB0 indexes the second) and the newly gained ones; 0x0881AA94 searches
    // them and 0x0881AB64 removes from them.
    GT_FIELD(std::uint32_t, unk_34, 0x34);             // first entry of the first list (+0x34 .. +0x1C4)
    GT_FIELD(std::uint32_t, unk_1C4, 0x1C4);           // first entry of the second list (+0x1C4 .. +0x354)
    // The favourites: 30 slots of a CarParameter handle each (0x0881AD9C reads 0x088A5EE0 of each, 0x0881AF28
    // returns 30 for MCarCollection::getFavoriteSize).
    GT_ARRAY(CarParameterHandle, favorites, 0x354, 8, 30);
};

// The engine object the script's MCarCollection wraps (0x0881B068 returns it): the script object's +0x14.
struct CarCollectionScript : guest::Layout {
    GT_FIELD(guest::Pointer<CarCollectionData>, engine, 0x14);
};

} // namespace gt::game
