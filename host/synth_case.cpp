// Synthetic-case input generator: see synth_case.hpp.
//
// The arena is cut in 1 KiB blocks; with trivial functions given, every eighth block is a "table" (a vtable stand-in)
// holding only those functions, and some arena pointers aim at a table's start, so an object whose first word is such a
// pointer makes a virtual call land on real code. The other words are arena pointers, small integers, zero, -1, common
// floats and random words kept out of the code range (a random word there would be a call target the case never meant).
#include "synth_case.hpp"

#include <algorithm>
#include <set>

namespace gt {
namespace {

constexpr std::uint32_t kBlockWords = 256u;
constexpr std::uint32_t kTableEvery = 8u;
constexpr std::uint32_t kCodeBegin = 0x08804000u, kCodeEnd = 0x08D00000u;
constexpr std::uint32_t kFloats[] = {0x00000000u, 0x3F800000u, 0xBF800000u, 0x3F000000u,
                                     0x40000000u, 0x42C80000u, 0x3A83126Fu, 0x447A0000u};  // 0, ±1, 0.5, 2, 100, 0.001, 1000

struct Random {
    std::uint64_t state;
    std::uint32_t next() {  // splitmix64
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return static_cast<std::uint32_t>((z ^ (z >> 31)) >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

std::uint32_t outside_code(std::uint32_t word) {
    return word >= kCodeBegin && word < kCodeEnd ? word ^ 0x40000000u : word;
}

struct Arena {
    const SynthLayout &layout;
    std::uint32_t words() const { return layout.arena_bytes / 4u; }
    bool has_tables() const { return !layout.leaf_functions.empty() && words() >= kBlockWords * kTableEvery; }
    bool is_table(std::uint32_t word_index) const { return has_tables() && (word_index / kBlockWords) % kTableEvery == kTableEvery - 1u; }
    // `live_share` percent of the picks are one of the game's objects when there are any (no draw is spent otherwise,
    // so the inputs without live objects stay what they were).
    std::uint32_t pointer(Random &random, std::uint32_t live_share = 0u) const {
        if (!layout.live_objects.empty() && live_share != 0u && random.below(100u) < live_share)
            return layout.live_objects[random.below(static_cast<std::uint32_t>(layout.live_objects.size()))];
        if (has_tables() && random.below(100u) < 15u) {
            const std::uint32_t tables = words() / (kBlockWords * kTableEvery);
            const std::uint32_t table = random.below(tables);
            return layout.arena_base + 4u * ((table * kTableEvery + kTableEvery - 1u) * kBlockWords);
        }
        return layout.arena_base + 4u * random.below(words());
    }
};

} // namespace

std::vector<VtableRun> find_vtable_runs(std::uint32_t begin, std::uint32_t end,
                                        const std::function<std::uint32_t(std::uint32_t)> &load32,
                                        const std::function<bool(std::uint32_t)> &is_function) {
    std::vector<VtableRun> runs;
    VtableRun current;
    const auto flush = [&] {
        if (current.functions.size() >= 2u) runs.push_back(current);
        current = VtableRun{};
    };
    for (std::uint32_t address = begin; address < end; address += 4u) {
        const std::uint32_t word = load32(address);
        if (is_function(word)) {
            if (current.functions.empty()) current.start = address;
            current.functions.push_back(word);
        } else {
            flush();
        }
    }
    flush();
    return runs;
}

std::map<std::uint32_t, std::vector<std::uint32_t>> class_instances(
    const std::vector<VtableRun> &runs, const std::vector<std::pair<std::uint32_t, std::uint32_t>> &objects) {
    std::map<std::uint32_t, std::set<std::uint32_t>> found;
    for (const VtableRun &run : runs) {
        const std::uint32_t last = run.start + 4u * static_cast<std::uint32_t>(run.functions.size());
        for (const auto &[object, vptr] : objects)
            if (vptr >= run.start && vptr < last)
                for (const std::uint32_t function : run.functions) found[function].insert(object);
    }
    std::map<std::uint32_t, std::vector<std::uint32_t>> result;
    for (const auto &[function, set] : found) result[function].assign(set.begin(), set.end());
    return result;
}

std::vector<ScenePoke> scene_pokes(std::uint32_t function, unsigned index) {
    if ((index & 1u) == 0u) return {};
    switch (function) {
    case 0x08945F6Cu: return {{0x08CF2F08u, 1u}};  // the flag func_0894CCB8 reads: set, so the body's mask-4 branch is taken
    default: return {};
    }
}

SynthInputs make_synth_case(std::uint32_t function, unsigned index, std::uint64_t seed, const SynthLayout &layout) {
    Random random{seed * 0x100000001B3ull ^ (static_cast<std::uint64_t>(function) << 20) ^ index};
    const Arena arena{layout};
    SynthInputs inputs;
    inputs.arena.resize(arena.words());
    for (std::uint32_t i = 0; i < arena.words(); ++i) {
        if (arena.is_table(i)) {
            inputs.arena[i] = layout.leaf_functions[random.below(static_cast<std::uint32_t>(layout.leaf_functions.size()))];
            continue;
        }
        const std::uint32_t pick = random.below(100u);
        inputs.arena[i] = pick < 40u ? arena.pointer(random, 20u)
                          : pick < 65u ? random.below(17u)
                          : pick < 75u ? 0u
                          : pick < 80u ? 0xFFFFFFFFu
                          : pick < 90u ? kFloats[random.below(8u)]
                                       : outside_code(random.next() << 16 | random.next());
    }
    for (std::uint32_t &reg : inputs.gpr) {
        const std::uint32_t pick = random.below(100u);
        reg = pick < 50u ? arena.pointer(random, 50u)
              : pick < 70u ? random.below(17u)
              : pick < 80u ? 0u
              : pick < 85u ? 0xFFFFFFFFu
                           : outside_code(random.next() << 16 | random.next());
    }
    for (std::uint32_t &reg : inputs.fpr) reg = kFloats[random.below(8u)];
    // Edge floats the generator never makes (NaNs of both signs, -0, denormals, infinities), for the functions whose float
    // results depend on them: the odd cases of those functions replace f12/f13 by a pair drawn from the table without
    // touching the random stream (so every other function's inputs stay as they were). func_08B4A430 (atan2): a NaN
    // y or x went through a sign flip in the decompiled run only (FINDINGS: the 0x08945F6C flag-1 mismatch).
    if ((index & 1u) != 0u && (function == 0x08B4A430u || function == 0x08B499B8u)) {
        constexpr std::uint32_t kEdges[] = {0xFFC00000u, 0x7FC00000u, 0x80000000u, 0x00000001u, 0x00800000u, 0x7F800000u, 0xFF800000u,
                                            0x894A612Au, 0x3F800000u, 0xBF800000u, 0x00000000u};
        const unsigned n = static_cast<unsigned>(sizeof kEdges / sizeof kEdges[0]);
        const unsigned pair = index / 2u;
        inputs.fpr[0] = kEdges[pair % n];
        inputs.fpr[1] = kEdges[(pair / n + pair) % n];
    }
    // The class's own object as `this`: only a0 changes and no draw is spent, so every other input stays as it was.
    if (const auto it = layout.class_instances.find(function); it != layout.class_instances.end() && !it->second.empty())
        inputs.gpr[0] = it->second[(seed + static_cast<std::uint64_t>(index) * 7u) % it->second.size()];
    return inputs;
}

} // namespace gt
