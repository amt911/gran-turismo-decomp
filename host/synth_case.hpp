// Synthetic-case inputs (docs/superpowers/specs/2026-10-25-synthetic-cases-design.md): what a harness case of a function
// no route reaches starts from — the argument registers and an arena of guest memory — generated deterministically.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace gt {

struct SynthLayout {
    std::uint32_t arena_base{};
    std::uint32_t arena_bytes{};
    std::vector<std::uint32_t> leaf_functions;  // trivial real functions a virtual call may land on
    std::vector<std::uint32_t> live_objects;    // the game's objects (first word a vtable), for pointer inputs
    // function -> live objects of its class (a0 of that function is one of them; see class_instances).
    std::map<std::uint32_t, std::vector<std::uint32_t>> class_instances;
};

// Class-matched a0 (spec 2026-11-10-recorded-arguments-design.md, option B). A vtable run is >= 2 consecutive words in
// [begin, end) that are function addresses; an object belongs to the run when its first word (vptr) lies inside it.
struct VtableRun {
    std::uint32_t start{};
    std::vector<std::uint32_t> functions;
};
std::vector<VtableRun> find_vtable_runs(std::uint32_t begin, std::uint32_t end,
                                        const std::function<std::uint32_t(std::uint32_t)> &load32,
                                        const std::function<bool(std::uint32_t)> &is_function);
// {object, vptr} pairs -> for every function of a run, the objects whose vptr lies in that run (sorted, distinct).
std::map<std::uint32_t, std::vector<std::uint32_t>> class_instances(
    const std::vector<VtableRun> &runs, const std::vector<std::pair<std::uint32_t, std::uint32_t>> &objects);

struct SynthInputs {
    std::array<std::uint32_t, 8> gpr{};   // a0-a3, t0-t3
    std::array<std::uint32_t, 8> fpr{};   // f12-f19 (bits)
    std::vector<std::uint32_t> arena;     // arena_bytes / 4 words, written at arena_base
};

// Global words a function's odd-numbered cases set before they run (even ones keep the scene as it is): the paths a flag in
// game data selects, which no route and no generated input ever sets (func_08945F6C: the flag at 0x08CF2F08, FINDINGS).
struct ScenePoke {
    std::uint32_t address;
    std::uint32_t value;
};
std::vector<ScenePoke> scene_pokes(std::uint32_t function, unsigned index);

SynthInputs make_synth_case(std::uint32_t function, unsigned index, std::uint64_t seed, const SynthLayout &layout);

} // namespace gt
