// host/game_state.hpp: the named accessors of the mod SDK against a guest memory image built here.
#include "game_state.hpp"

#include <cstdio>
#include <memory>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
constexpr std::uint32_t kCell = 0x08F6AC00u;
} // namespace

int main() {
    using namespace gt::game_state;
    auto owned = std::make_unique<psprecomp::GuestMemory>();
    psprecomp::GuestMemory &m = *owned;
    std::uint32_t value = 7u;
    check(!money_get(m, value) && value == 7u, "money: an empty cell is not the balance");
    m.store32(kCell, 100000u);
    m.store32(kCell + 4u, 0u);
    m.store32(kCell + 8u, 0x00020100u);
    check(money_get(m, value) && value == 100000u, "money: new-game cell read");
    m.store32(kCell + 8u, 0x01030000u);
    check(money_get(m, value) && value == 100000u, "money: saved/loaded cell read");
    check(money_set(m, 2100000u) && m.load32(kCell) == 2100000u, "money: set writes the cell");
    check(!money_set(m, kMoneyCap + 1u) && m.load32(kCell) == 2100000u, "money: above the cap refused, nothing written");
    m.store32(kCell + 4u, 1u);
    check(!money_get(m, value) && !money_set(m, 5u) && m.load32(kCell) == 2100000u, "money: a high word set is not the balance");
    m.store32(kCell + 4u, 0u);
    m.store32(kCell + 8u, 0x00020101u);
    check(!money_set(m, 5u), "money: another +8 word is not the balance");
    if (failures == 0) std::puts("game_state: all checks passed");
    return failures == 0 ? 0 : 1;
}
