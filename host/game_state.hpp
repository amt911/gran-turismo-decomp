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

enum class CarRole { None, Human, Ai, ReplayHuman };  // ReplayHuman: a human car in its replay (entry +0xA8 set)

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

// ---- v5: impact (docs/FACTS.md "Collision code for dent contact points: NOT FOUND": no contact point exists in the state, so the
// velocity change of one step stands in for it). ----

// The previous step's sample of every car (keyed by body handle): car_impact compares with it, so call it once per world step.
struct ImpactTracker {
    struct Sample {
        std::uint32_t handle = 0;
        float velocity[3] = {0, 0, 0};
        float speed = 0, yaw_rate = 0;
        bool valid = false;
    };
    Sample samples[kMaxCars];
};

struct Impact {
    float dv_body[3];       // velocity change of the step in the body frame (forward, right, up rows)
    float dv_magnitude;     // |dv|, m/s; 0 on the first sample of a car
    float speed_before;     // K+0x6C4 of the previous sample
    float yaw_rate_jump;    // |K+0x6F8 - previous|
    bool off_track;         // any wheel record's surface word (+0x2C) is 0x300
    float contact_dir[3];   // -dv normalised, in the body frame (where on the body the hit landed); 0 when dv is 0
    float box_half_length, box_half_width, box_floor, box_roof;  // the rollover body box (FACTS "Rollover body box": a heuristic)
};

// Fills `out` for the car whose body is `handle` in `world`; false when the body is no slot of the world.
bool car_impact(const psprecomp::GuestMemory &memory, std::uint32_t world, std::uint32_t handle, ImpactTracker &tracker, Impact &out);

// ---- v5: the link from a car to its model (docs/FACTS.md "Car to model link", measured 2026-11-14 by probes on the rollover race route). ----

struct CarDrawLink {
    std::uint32_t car = 0;    // the body handle K (what race_cars / player_car return)
    std::uint32_t model = 0;  // the MDL3 container of the car's parts
};

constexpr std::uint32_t kCarDrawFunction = 0x0889BAB0u;  // the per-car view update; a0 = the view's inner object
constexpr std::uint32_t kModelMagic = 0x4D444C33u;       // "3LDM" at +0 of a model container

// From the a0 of the car draw function: P = word(a0 + 4) is the car's view; word(P + 0xB04) its driver entry, whose +0xC is the car object (K =
// word(car + 8)); the model is word(word(P + 0x20) + 8). False when a link is null or the model's magic is not "3LDM".
bool car_draw_link(const psprecomp::GuestMemory &memory, std::uint32_t a0, CarDrawLink &out);

// The parts of a model: `count` = word(model + 0x14) entries of 32 bytes from word(model + 0xA8), each starting with a blob pointer. Writes the
// blobs that parse as meshes (host/mesh_blob.hpp) to `out`; returns how many (at most `capacity`).
std::uint32_t model_blobs(const psprecomp::GuestMemory &memory, std::uint32_t model, std::uint32_t *out, std::uint32_t capacity);

// Every vertex run of every part of a model (host/mesh_blob.hpp parse_runs: blobs with several VADRs included, which model_blobs refuses): fills
// `out` with (blob, run index within the blob, vertex count); returns how many (at most `capacity`).
struct RunRef {
    std::uint32_t blob;
    std::uint32_t run;
    std::uint32_t count;
};
std::uint32_t model_runs(const psprecomp::GuestMemory &memory, std::uint32_t model, RunRef *out, std::uint32_t capacity);

// The matrix the display list being built has uploaded last: the list cursor is word(kListState + 0x5C) (kListState = 0x08D6CE00, the state the leaf
// 0x08B550A8 appends BASE + CALL with); looking back from it for WORLDN (0x3A) followed by 12 WORLDD (0x3B), each carrying a float's top 24 bits.
// True and `out` filled (12 floats in upload order) when found within `max_bytes` of list.
constexpr std::uint32_t kListState = 0x08D6CE00u;
bool list_world_matrix(const psprecomp::GuestMemory &memory, float out[12], std::uint32_t max_bytes = 0x10000u);

// ---- v7: pose writing, dimensions and wheel records (docs/superpowers/specs/2026-11-18-rollover-physics-design.md; FACTS "Rollover physics research (2026-11-18)"). ----

// The Euler convention the body is rebuilt from (0x0893C3E4, read from the asm 2026-11-18): angles (pitch = K+0x6FC, roll = K+0x700, yaw = K+0x704),
// s = sin, c = cos, indices 1 pitch, 2 roll, 3 yaw:
//   forward row = (-s3 c1, c3 c1, s1)
//   right row   = (c3 c2 - s1 s2 s3, s3 c2 + s1 s2 c3, -c1 s2)
//   up row      = (c3 s2 + s1 c2 s3, s3 s2 - s1 c2 c3, c1 c2)
// (yaw 1.571 gives forward (-1, 0, 0): the `race` start's velocity direction, FACTS "Car body state").
void euler_to_rows(const float euler[3], float rows[9]);
// The inverse: pitch = asin(forward.z) in [-pi/2, pi/2], roll = atan2(-right.z, up.z), yaw = atan2(-forward.x, forward.y). At the pole (|forward.z| ~ 1) yaw and
// roll are one degree of freedom: roll = 0 and yaw takes it all, so the rows rebuilt from the result are the input's within float noise.
void rows_to_euler(const float rows[9], float euler[3]);

// A world's car slot: the car object (word(car + 8) is the body handle), or 0 when `handle` is no slot of `world`.
std::uint32_t car_object(const psprecomp::GuestMemory &memory, std::uint32_t world, std::uint32_t handle);

struct CarDimensions {
    float wheelbase, track, front_axle, rear_axle, radius;
};
bool car_dimensions(const psprecomp::GuestMemory &memory, std::uint32_t world, std::uint32_t handle, CarDimensions &out);

struct WheelContact {
    float x, y, ground_height;
    std::int32_t surface, valid;
    float load;
};
bool car_wheel_contacts(const psprecomp::GuestMemory &memory, std::uint32_t world, std::uint32_t handle, WheelContact out[4]);

// Writes position, basis rows, the Euler angles derived from them, velocity, speed (|velocity|) and zero in all five rate fields. False (nothing written)
// for a bad handle, a non-finite value or rows that are not an orthonormal basis (each row unit within 1e-2, mutually perpendicular within 1e-2).
bool car_set_pose(psprecomp::GuestMemory &memory, std::uint32_t handle, const float position[3], const float basis[9], const float velocity[3],
                  const float angular_velocity[3]);

} // namespace gt::game_state
