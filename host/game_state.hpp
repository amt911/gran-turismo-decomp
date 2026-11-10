// Named game state for the mod SDK (API v3, docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md): the researched guest
// addresses live here, next to their evidence, so mods never hard-code them. When the emulated memory goes away only this file's
// insides change.
#pragma once

#include "psprecomp/guest_memory.hpp"

#include <cstdint>

namespace gt::game_state {

// The live balance (docs/FACTS.md "Money and cars"): the word at 0x08F6AC00, the low word of a script value whose high word
// (+4) is 0 and whose +8 word is 0x00020100 in a new game or 0x01030000 once the game has saved or loaded; the dealer reads and
// writes it and the game's own save stores it. The game's cap is 999,999,999.
constexpr std::uint32_t kMoneyCap = 999999999u;

// The balance, or false when the cell does not check out (no game loaded, another heap layout).
bool money_get(const psprecomp::GuestMemory &memory, std::uint32_t &value);

// Writes `value` when the cell checks out and `value` <= kMoneyCap; false otherwise (nothing written).
bool money_set(psprecomp::GuestMemory &memory, std::uint32_t value);

} // namespace gt::game_state
