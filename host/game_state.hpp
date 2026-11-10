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

// ---- Cars of the running race (docs/FACTS.md "Car body state", "Cheats: the world step"). `world` is the a0 of the world
// step (0x0894DD9C): +4 the car slots (0x2100 bytes each, the car at +0x2010), +8 the car count, +0x10 the controller whose
// +0xC list maps a car index to its driver entry. A car handle is its body state K = word(car + 8). ----

enum class CarRole { None, Human, Ai };

struct Car {
    std::uint32_t handle = 0;  // K, opaque to mods
    std::uint32_t index = 0;   // the car's slot index
    CarRole role = CarRole::None;
};

constexpr std::uint32_t kMaxCars = 32;

// The cars that have a driver entry and a body, in slot order. A human is live (not a replay) and reads a pad. Returns how
// many were written to `out` (at most kMaxCars).
std::uint32_t race_cars(const psprecomp::GuestMemory &memory, std::uint32_t world, Car *out);

// The live human car's handle when there is exactly one, else 0.
std::uint32_t player_car(const psprecomp::GuestMemory &memory, std::uint32_t world);

// The body fields the physics step integrates, plus the pedals (offsets from K, FACTS "Car body state").
struct CarState {
    float position[3];
    float basis[9];     // forward, right, up rows (left-handed)
    float speed;        // m/s
    float velocity[3];
    float rates[5];     // angular rates (+0x6E4, +0x6E8, +0x6F0 pitch?, +0x6F4 roll, +0x6F8 yaw?: FACTS)
    float euler[3];
    float throttle;     // +0x530 accelerator pedal 0..1
    float brake;        // +0x538
};

bool car_state(const psprecomp::GuestMemory &memory, std::uint32_t handle, CarState &out);
bool car_set_state(psprecomp::GuestMemory &memory, std::uint32_t handle, const CarState &state);

} // namespace gt::game_state
