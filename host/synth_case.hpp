// Synthetic-case inputs (docs/superpowers/specs/2026-10-25-synthetic-cases-design.md): what a harness case of a function
// no route reaches starts from — the argument registers and an arena of guest memory — generated deterministically.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace gt {

struct SynthLayout {
    std::uint32_t arena_base{};
    std::uint32_t arena_bytes{};
    std::vector<std::uint32_t> leaf_functions;  // trivial real functions a virtual call may land on
    std::vector<std::uint32_t> live_objects;    // the game's objects (first word a vtable), for pointer inputs
};

struct SynthInputs {
    std::array<std::uint32_t, 8> gpr{};   // a0-a3, t0-t3
    std::array<std::uint32_t, 8> fpr{};   // f12-f19 (bits)
    std::vector<std::uint32_t> arena;     // arena_bytes / 4 words, written at arena_base
};

SynthInputs make_synth_case(std::uint32_t function, unsigned index, std::uint64_t seed, const SynthLayout &layout);

} // namespace gt
