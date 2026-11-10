#include "game_state.hpp"

namespace gt::game_state {

namespace {
constexpr std::uint32_t kMoneyCell = 0x08F6AC00u;  // docs/FACTS.md "Money and cars" (probe runs 2026-11-08..10)

bool money_cell_ok(const psprecomp::GuestMemory &memory) {
    if (!memory.contains(kMoneyCell, 12u)) return false;
    const std::uint32_t high = memory.load32(kMoneyCell + 4u), kind = memory.load32(kMoneyCell + 8u);
    return high == 0u && (kind == 0x00020100u || kind == 0x01030000u) && memory.load32(kMoneyCell) <= kMoneyCap;
}
} // namespace

bool money_get(const psprecomp::GuestMemory &memory, std::uint32_t &value) {
    if (!money_cell_ok(memory)) return false;
    value = memory.load32(kMoneyCell);
    return true;
}

bool money_set(psprecomp::GuestMemory &memory, std::uint32_t value) {
    if (value > kMoneyCap || !money_cell_ok(memory)) return false;
    memory.store32(kMoneyCell, value);
    return true;
}

} // namespace gt::game_state
