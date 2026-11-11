// Unit tests for the synthetic-case input generator (host/synth_case.cpp). Plain executable, run by CTest.
#include "synth_case.hpp"

#include <cstdio>
#include <string>
#include <map>
#include <utility>

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

    // Class-matched a0 (spec 2026-11-10-recorded-arguments-design.md, option B).
    {
        // Memory: a vtable of three functions at 0x08C80000 (run), a lone function word at 0x08C80100 (no run), then a
        // second run of two at 0x08C80200.
        const std::map<std::uint32_t, std::uint32_t> memory = {
            {0x08C80000u, 0x08810000u}, {0x08C80004u, 0x08810010u}, {0x08C80008u, 0x08810020u},
            {0x08C8000Cu, 5u},          {0x08C80100u, 0x08810030u}, {0x08C80104u, 0u},
            {0x08C80200u, 0x08810040u}, {0x08C80204u, 0x08810050u}};
        const auto load = [&](std::uint32_t a) { const auto it = memory.find(a); return it == memory.end() ? 0u : it->second; };
        const auto is_function = [](std::uint32_t w) { return w >= 0x08810000u && w < 0x08820000u; };
        const std::vector<gt::VtableRun> runs = gt::find_vtable_runs(0x08C80000u, 0x08C80300u, load, is_function);
        check(runs.size() == 2u, "two runs of >= 2 consecutive function words (a lone word is no run)");
        check(runs.size() == 2u && runs[0].start == 0x08C80000u && runs[0].functions.size() == 3u, "first run: start and members");
        check(runs.size() == 2u && runs[1].start == 0x08C80200u && runs[1].functions.size() == 2u, "second run: start and members");
        check(gt::find_vtable_runs(0x08C80000u, 0x08C80008u, load, is_function).size() == 1u, "a run cut by the end of the range still counts");

        // Objects: 0x09100000 has vptr = run 0 start, 0x09100010 has vptr inside run 1, 0x09100020 has an unknown vptr.
        const std::vector<std::pair<std::uint32_t, std::uint32_t>> objects = {
            {0x09100000u, 0x08C80000u}, {0x09100010u, 0x08C80204u}, {0x09100020u, 0x08C80900u}, {0x09100030u, 0x08C80000u}};
        const auto instances = gt::class_instances(runs, objects);
        check(instances.count(0x08810010u) == 1u && instances.at(0x08810010u).size() == 2u, "every member of a run gets that class's instances");
        check(instances.count(0x08810050u) == 1u && instances.at(0x08810050u) == std::vector<std::uint32_t>{0x09100010u},
              "a vptr inside the run (not at its start) matches");
        check(instances.count(0x08810030u) == 0u, "a function outside any run has no class instances");

        gt::SynthLayout classed = layout;
        classed.class_instances = instances;
        unsigned hits = 0u;
        for (unsigned index = 0u; index < 16u; ++index) {
            const gt::SynthInputs inputs = gt::make_synth_case(0x08810010u, index, 3u, classed);
            check(inputs.gpr[0] == 0x09100000u || inputs.gpr[0] == 0x09100030u, "a0 of a class function is one of its class's objects");
            const gt::SynthInputs again = gt::make_synth_case(0x08810010u, index, 3u, classed);
            check(inputs.gpr == again.gpr && inputs.arena == again.arena, "class-matched: deterministic");
            const gt::SynthInputs plain = gt::make_synth_case(0x08810010u, index, 3u, layout);
            check(plain.arena == inputs.arena && plain.fpr == inputs.fpr, "only a0 changes");
            for (unsigned r = 1u; r < 8u; ++r) check(plain.gpr[r] == inputs.gpr[r], "a1-a3 and t0-t3 unchanged");
            if (inputs.gpr[0] == 0x09100030u) ++hits;
        }
        check(hits > 0u && hits < 16u, "the instance varies with the case index");
        const gt::SynthInputs other = gt::make_synth_case(0x08810030u, 1u, 3u, classed);
        check(other.gpr == gt::make_synth_case(0x08810030u, 1u, 3u, layout).gpr, "a function with no instance keeps today's inputs");
    }

    // Recipes (spec 2026-11-11-coverage-guided-synthetic-inputs-design.md): a case plus mutations, as text.
    {
        gt::SynthLayout rl = layout;
        rl.live_objects = {0x09100000u, 0x09100040u};
        rl.class_instances[0x08810010u] = {0x09100000u, 0x09100030u};
        gt::Recipe recipe;
        recipe.index = 12u;
        recipe.seed = 3u;
        recipe.mutations = {{gt::MutationKind::Reg, 1u, {gt::ValueKind::Imm, 0xFFFFFFFFu}},
                            {gt::MutationKind::Reg, 2u, {gt::ValueKind::Arena, 0x40u}},
                            {gt::MutationKind::Word, 0x80u, {gt::ValueKind::Live, 1u}},
                            {gt::MutationKind::Word, 0x84u, {gt::ValueKind::Class, 1u}},
                            {gt::MutationKind::Block, 3u, {gt::ValueKind::Imm, 77u}}};
        const std::string text = gt::format_recipe(0x08810010u, recipe);
        check(text == "08810010 12 3 reg:1=imm:0xFFFFFFFF reg:2=arena:0x40 word:0x80=live:1 word:0x84=class:1 block:3=77", "format_recipe text");
        std::uint32_t function = 0u;
        gt::Recipe parsed;
        check(gt::parse_recipe(text, function, parsed) && function == 0x08810010u && gt::format_recipe(function, parsed) == text,
              "parse_recipe round trips");
        check(!gt::parse_recipe("08810010 12", function, parsed), "too few fields is rejected");
        check(!gt::parse_recipe("08810010 12 3 reg:9=imm:1", function, parsed), "register index 8 and up is rejected");
        check(!gt::parse_recipe("08810010 12 3 word:0x3=imm:1", function, parsed), "an unaligned word offset is rejected");
        check(!gt::parse_recipe("08810010 12 3 bogus:1=imm:1", function, parsed), "unknown mutation kind is rejected");
        check(!gt::parse_recipe("zz 12 3", function, parsed), "bad address is rejected");

        const gt::SynthInputs base = gt::make_synth_case(0x08810010u, 12u, 3u, rl);
        const gt::SynthInputs applied = gt::make_recipe_case(0x08810010u, recipe, rl);
        check(applied.gpr[1] == 0xFFFFFFFFu, "reg:imm sets the register");
        check(applied.gpr[2] == rl.arena_base + 0x40u, "reg:arena is arena-relative");
        check(applied.arena[0x80u / 4u] == 0x09100040u, "word:live picks live_objects[k % n]");
        check(applied.arena[0x84u / 4u] == 0x09100030u, "word:class picks the class instance");
        check(applied.gpr[0] == base.gpr[0], "a0 (the class instance) is as in make_synth_case");
        unsigned changed = 0u;
        for (std::size_t i = 0; i < base.arena.size(); ++i) if (base.arena[i] != applied.arena[i]) ++changed;
        check(changed >= 2u && changed <= 258u, "only the poked words and block 3 change");
        for (std::uint32_t w = 0; w < 32u; ++w) check(base.arena[w] == applied.arena[w], "words before the first mutation are untouched");
        const gt::SynthInputs again = gt::make_recipe_case(0x08810010u, recipe, rl);
        check(again.gpr == applied.gpr && again.arena == applied.arena, "a recipe replays to the same inputs");
        gt::Recipe plain;
        plain.index = 12u;
        plain.seed = 3u;
        const gt::SynthInputs same = gt::make_recipe_case(0x08810010u, plain, rl);
        check(same.gpr == base.gpr && same.arena == base.arena, "a recipe without mutations is make_synth_case");

        // mutate: deterministic for the same generator state, adds exactly one mutation, stays in range
        gt::MutationRandom a{99u}, b{99u};
        const gt::Recipe m1 = gt::mutate_recipe(0x08810010u, plain, a, rl);
        const gt::Recipe m2 = gt::mutate_recipe(0x08810010u, plain, b, rl);
        check(gt::format_recipe(0x08810010u, m1) == gt::format_recipe(0x08810010u, m2), "mutate is deterministic");
        check(m1.mutations.size() == 1u, "mutate adds one mutation");
        {   // hot offsets: words behind the registers that are arena pointers, one more level down
            gt::SynthInputs fake;
            fake.arena.assign(rl.arena_bytes / 4u, 0u);
            fake.gpr = {rl.arena_base + 0x100u, 5u, 0u, 0u, 0u, 0u, 0u, 0u};
            fake.arena[0x100u / 4u] = rl.arena_base + 0x2000u;  // a pointer inside the first range
            const std::vector<std::uint32_t> hot = gt::hot_offsets(fake, rl);
            const auto has = [&](std::uint32_t off) { for (const std::uint32_t h : hot) if (h == off) return true; return false; };
            check(has(0x100u) && has(0x1FCu) && !has(0x200u), "hot: 0x100 bytes at a pointer register");
            check(has(0x2000u) && has(0x20FCu) && !has(0x2100u), "hot: one more level through an arena pointer");
            check(!has(0u), "hot: nothing for registers that are not arena pointers");
            gt::MutationRandom h{7u};
            unsigned inside = 0u, words = 0u;
            for (unsigned i = 0u; i < 400u; ++i) {
                const gt::Recipe r = gt::mutate_recipe(0x08810010u, plain, h, rl, hot);
                if (r.mutations[0].kind != gt::MutationKind::Word) continue;
                ++words;
                if (has(r.mutations[0].where)) ++inside;
            }
            check(words > 100u && inside * 10u > words * 6u, "most word mutations land in the hot set");
        }
        gt::MutationRandom c{5u};
        gt::Recipe cur = plain;
        for (unsigned i = 0u; i < 300u; ++i) {
            cur = gt::mutate_recipe(0x08810010u, cur, c, rl);
            if (cur.mutations.size() > 6u) cur.mutations.erase(cur.mutations.begin());
            for (const gt::Mutation &m : cur.mutations) {
                if (m.kind == gt::MutationKind::Reg) check(m.where < 8u, "mutate: register index in range");
                if (m.kind == gt::MutationKind::Word) check(m.where % 4u == 0u && m.where < rl.arena_bytes, "mutate: word offset aligned and in the arena");
                if (m.kind == gt::MutationKind::Block) check(m.where < rl.arena_bytes / 1024u, "mutate: block index in range");
                if (m.value.kind == gt::ValueKind::Arena) check(m.value.number % 4u == 0u && m.value.number < rl.arena_bytes, "mutate: arena value aligned and in the arena");
            }
            const gt::SynthInputs ok = gt::make_recipe_case(0x08810010u, cur, rl);
            check(ok.arena.size() == rl.arena_bytes / 4u, "mutated case keeps the arena size");
        }
    }
    return failures == 0 ? 0 : 1;
}
