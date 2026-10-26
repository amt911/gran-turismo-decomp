// Unit tests for the synthetic-case input generator (host/synth_case.cpp). Plain executable, run by CTest.
#include "synth_case.hpp"

#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool in_arena(const gt::SynthLayout &layout, std::uint32_t address) {
    return address >= layout.arena_base && address < layout.arena_base + layout.arena_bytes;
}
}  // namespace

int main() {
    gt::SynthLayout layout;
    layout.arena_base = 0x09E00000u;
    layout.arena_bytes = 0x10000u;
    layout.leaf_functions = {0x08804100u, 0x08804200u, 0x08804300u};

    const gt::SynthInputs a = gt::make_synth_case(0x08900000u, 3u, 7u, layout);
    const gt::SynthInputs b = gt::make_synth_case(0x08900000u, 3u, 7u, layout);
    check(a.gpr == b.gpr && a.fpr == b.fpr && a.arena == b.arena, "same function, index and seed: same inputs");
    const gt::SynthInputs c = gt::make_synth_case(0x08900000u, 4u, 7u, layout);
    check(c.gpr != a.gpr || c.arena != a.arena, "another case index: other inputs");
    check(a.arena.size() == layout.arena_bytes / 4u, "the arena fills its bytes");

    unsigned pointer_words = 0u, leaf_words = 0u, pointer_registers = 0u;
    for (unsigned index = 0u; index < 64u; ++index) {
        const gt::SynthInputs inputs = gt::make_synth_case(0x08912340u, index, 1u, layout);
        for (const std::uint32_t value : inputs.gpr)
            if (in_arena(layout, value)) {
                ++pointer_registers;
                check(value % 4u == 0u, "an arena pointer register is word-aligned");
            }
        for (std::size_t i = 0; i < inputs.arena.size(); ++i) {
            const std::uint32_t word = inputs.arena[i];
            if (in_arena(layout, word)) {
                ++pointer_words;
                check(word % 4u == 0u, "an arena pointer word is word-aligned");
            }
            if (word == 0x08804100u || word == 0x08804200u || word == 0x08804300u) ++leaf_words;
            if (word >= 0x08804000u && word < 0x08D00000u && word != 0x08804100u && word != 0x08804200u &&
                word != 0x08804300u && word % 4u == 0u && word > 0x0880FFFFu)
                check(false, "a code-range word that is not one of the given functions");
        }
    }
    check(pointer_registers > 64u, "argument registers are often arena pointers");
    check(pointer_words > 1000u, "arena words are often arena pointers");
    check(leaf_words > 0u, "the table blocks hold the given functions");

    gt::SynthLayout no_leaves = layout;
    no_leaves.leaf_functions.clear();
    const gt::SynthInputs d = gt::make_synth_case(0x08900000u, 0u, 7u, no_leaves);
    check(d.arena.size() == layout.arena_bytes / 4u, "no functions given: still an arena");
    {  // live objects: some pointer inputs are the game's own objects, exactly as given, and none without any
        gt::SynthLayout live = layout;
        live.live_objects = {0x09100000u, 0x09100040u, 0x09200010u};
        unsigned registers = 0u, words = 0u;
        for (unsigned index = 0u; index < 64u; ++index) {
            const gt::SynthInputs inputs = gt::make_synth_case(0x08912340u, index, 1u, live);
            for (const std::uint32_t value : inputs.gpr)
                if (value == 0x09100000u || value == 0x09100040u || value == 0x09200010u) ++registers;
            for (const std::uint32_t word : inputs.arena)
                if (word == 0x09100000u || word == 0x09100040u || word == 0x09200010u) ++words;
            const gt::SynthInputs again = gt::make_synth_case(0x08912340u, index, 1u, live);
            check(inputs.gpr == again.gpr && inputs.arena == again.arena, "live objects: still deterministic");
        }
        check(registers > 32u, "argument registers are often live objects");
        check(words > 100u, "arena words are sometimes live objects");
        unsigned without = 0u;
        for (unsigned index = 0u; index < 64u; ++index)
            for (const std::uint32_t value : gt::make_synth_case(0x08912340u, index, 1u, layout).gpr)
                if (value == 0x09100000u || value == 0x09100040u || value == 0x09200010u) ++without;
        check(without == 0u, "no live objects given: none used");
    }
    return failures == 0 ? 0 : 1;
}
