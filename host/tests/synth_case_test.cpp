// Unit tests for the synthetic-case input generator (host/synth_case.cpp). Plain executable, run by CTest.
#include "synth_case.hpp"
#include "synth_report.hpp"

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

    // v1 stability (spec 2026-11-19-synthetic-pointer-graphs-design.md): the inputs of 200 cases of 20 addresses hash to the value
    // recorded with the pre-change host/synth_case.* (commit 8ef9ae7; those files are identical in origin/main bdb8a56). Changing it changes every existing comparison and corpus recipe.
    {
        gt::SynthLayout stable = layout;
        stable.live_objects = {0x09D00010u, 0x09D00200u};
        stable.class_instances[0x08900040u] = {0x09D00010u};
        std::uint64_t h = 1469598103934665603ull;
        const auto mix = [&](std::uint32_t v) { h ^= v; h *= 1099511628211ull; };
        for (unsigned f = 0; f < 20u; ++f)
            for (unsigned i = 0; i < 200u; ++i) {
                const gt::SynthInputs in = gt::make_synth_case(0x08900000u + f * 0x20u, i, 1u, stable);
                for (const std::uint32_t v : in.gpr) mix(v);
                for (const std::uint32_t v : in.fpr) mix(v);
                for (const std::uint32_t v : in.arena) mix(v);
            }
        // Recipes, mutations, scene pokes and hot offsets (the corpus replay path) over stored recipes.
        const char *recipes[] = {"08900000 3 1", "08900020 5 1 reg:0=arena:0x40 word:0x10=imm:0x7", "08900040 9 1 block:2=77 reg:1=live:1",
                                 "08900060 12 1 reg:0=class:0 word:0x20=arena:0x100"};
        for (const char *text : recipes) {
            std::uint32_t function = 0u;
            gt::Recipe recipe;
            if (!gt::parse_recipe(text, function, recipe)) { check(false, "stored recipe parses"); continue; }
            const gt::SynthInputs in = gt::make_recipe_case(function, recipe, stable);
            for (const std::uint32_t v : in.gpr) mix(v);
            for (const std::uint32_t v : in.arena) mix(v);
            for (const std::uint32_t o : gt::hot_offsets(in, stable)) mix(o);
            gt::MutationRandom random{0x1234u + function};
            for (unsigned k = 0; k < 6u; ++k) {
                const gt::Recipe child = gt::mutate_recipe(function, recipe, random, stable, gt::hot_offsets(in, stable));
                const std::string line = gt::format_recipe(function, child);
                for (const char c : line) mix(static_cast<unsigned char>(c));
                const gt::SynthInputs again = gt::make_recipe_case(function, child, stable);
                for (const std::uint32_t v : again.arena) mix(v);
            }
        }
        for (const std::uint32_t function : {0x08945F6Cu, 0x08900000u})
            for (unsigned i = 0; i < 4u; ++i)
                for (const gt::ScenePoke &poke : gt::scene_pokes(function, i)) { mix(poke.address); mix(poke.value); }
        check(h == 0xB981C03D8D73F9D2ull, "v1 inputs, recipes, mutations, pokes and hot offsets are bit-for-bit what they were before v2");
    }

    // v2: pointer closure. Every word is zero, a small int, a float, a leaf function (table), or a 16-byte-aligned pointer into the arena.
    {
        const std::uint32_t floats[] = {0x00000000u, 0x3F800000u, 0xBF800000u, 0x3F000000u, 0x40000000u, 0x42C80000u, 0x3A83126Fu, 0x447A0000u};
        unsigned bad = 0u, pointers = 0u, words = 0u;
        for (unsigned index = 0u; index < 16u; ++index) {
            const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, index, 1u, layout, nullptr);
            check(in.arena.size() == layout.arena_bytes / 4u, "v2 arena has the same size");
            check(in_arena(layout, in.gpr[0]), "v2 a0 is an arena pointer");
            for (const std::uint32_t w : in.arena) {
                ++words;
                bool known = w <= 16u;
                for (const std::uint32_t f : floats) known = known || w == f;
                for (const std::uint32_t l : layout.leaf_functions) known = known || w == l;
                if (in_arena(layout, w)) { known = (w - layout.arena_base) % 16u == 0u; ++pointers; }
                if (!known) ++bad;
            }
        }
        check(bad == 0u, "v2 words are only pointers into the arena, small ints, floats or leaf functions");
        check(pointers * 100u > words * 60u, "v2 draws mostly pointers");
        const gt::SynthInputs a2 = gt::make_synth_case_v2(0x08912340u, 3u, 1u, layout, nullptr);
        const gt::SynthInputs b2 = gt::make_synth_case_v2(0x08912340u, 3u, 1u, layout, nullptr);
        check(a2.gpr == b2.gpr && a2.arena == b2.arena, "v2 is deterministic");
        const gt::SynthInputs v1 = gt::make_synth_case(0x08912340u, 3u, 1u, layout);
        check(v1.arena != a2.arena, "v2 draws from its own stream");
    }

    // global holders (spec 2026-11-19-synthetic-pointer-graphs-design.md, addendum 2026-11-20): a shape line may set
    // words in .data/.bss to point at a fresh arena slot, so a function that first reads a null global compares.
    {
        constexpr std::uint32_t kDataBegin = 0x08CD8500u, kDataEnd = 0x08DF74ACu;  // the RW segment at load base 0x08804000
        std::string error;
        const auto shapes = gt::parse_shapes("08912340 global:0x08CF2F08=struct:16 field:+4=ptr\n", &error,
                                             kDataBegin, kDataEnd);
        check(error.empty() && shapes.size() == 1u, "a global shape line parses");
        const gt::Shape &shape = shapes.at(0x08912340u);
        check(!shape.has_reg, "a global-only line sets no register");
        check(shape.globals.size() == 1u && shape.globals[0].address == 0x08CF2F08u && shape.globals[0].size == 16u,
              "the global word and its slot size are kept");
        check(shape.globals[0].fields.size() == 1u && shape.globals[0].fields[0].offset == 4u &&
                  shape.globals[0].fields[0].kind == gt::ShapeFieldKind::Pointer,
              "the field after a global belongs to its slot");
        error.clear();
        check(gt::parse_shapes("08912340 global:147795720=struct:8\n", &error, kDataBegin, kDataEnd).size() == 1u && error.empty(),
              "a decimal global address parses");

        error.clear();
        const auto two = gt::parse_shapes(
            "08912340 global:0x08CF2F08=struct:8 field:+0=int:0..4 global:0x08CF2F0C=struct:12 field:+8=ptr\n",
            &error, kDataBegin, kDataEnd);
        check(error.empty() && two.size() == 1u && two.at(0x08912340u).globals.size() == 2u, "two globals on one line parse");
        check(two.at(0x08912340u).globals[0].fields.size() == 1u && two.at(0x08912340u).globals[1].fields.size() == 1u &&
                  two.at(0x08912340u).globals[1].fields[0].offset == 8u,
              "each global keeps the fields that follow it");
        error.clear();
        const auto both = gt::parse_shapes("08912340 reg:a1=struct:16 field:+0=ptr global:0x08CF2F08=struct:16\n",
                                           &error, kDataBegin, kDataEnd);
        check(error.empty() && both.size() == 1u && both.at(0x08912340u).has_reg && both.at(0x08912340u).globals.size() == 1u,
              "a line may set a register and a global");

        const auto rejects = [&](const char *text, const char *why) {
            std::string e;
            gt::parse_shapes(text, &e, kDataBegin, kDataEnd);
            check(!e.empty(), why);
        };
        rejects("08912340 global:0x08CF2F08=struct:8 global:0x08CF2F0C=struct:8 global:0x08CF2F10=struct:8 "
                "global:0x08CF2F14=struct:8 global:0x08CF2F18=struct:8\n", "a fifth global on a line is rejected");
        rejects("08912340 global:0x08CF2F08\n", "a global without a struct size is rejected");
        rejects("08912340 global:0x09E00000=struct:16\n", "a global address outside .data/.bss is rejected (the arena)");
        rejects("08912340 global:0x08CD84FC=struct:16\n", "a global address below .data is rejected");
        rejects("08912340 global:0x08DF74AC=struct:16\n", "a global address at the end of .bss is rejected");
        rejects("08912340 global:0x08CF2F0A=struct:16\n", "an unaligned global address is rejected");
        rejects("08912340 global:0x08CF2F08=struct:0\n", "a zero-size global slot is rejected");
        rejects("08912340 global:0x08CF2F08=struct:2048\n", "a global slot larger than a block is rejected");
        rejects("08912340 global:0x08CF2F08=struct:8 field:+8=ptr\n", "a field outside its global slot is rejected");
        rejects("08912340 reg:a0=struct:64 field:+0xFFFFFFFC=ptr\n", "a field offset that would wrap is rejected");
        rejects("08912340 global:0x08CF2F08=struct:8 global:0x08CF2F08=struct:8\n", "a duplicate global address is rejected");
        rejects("08912340 field:+0=ptr\n", "a field before any reg/global is rejected");
    }

    // literal words (spec 2026-11-19-synthetic-pointer-graphs-design.md, addendum 2026-11-20): a field may be a fixed
    // 32-bit word inside the ELF image — a holder object's real vtable — so a virtual call through the holder runs real
    // code instead of faulting on the pointer-closed arena.
    {
        constexpr std::uint32_t kDataBegin = 0x08CD8500u, kDataEnd = 0x08DF74ACu;  // the RW segment at load base 0x08804000
        constexpr std::uint32_t kVtable = 0x08C80000u;                             // a vtable word in .rodata
        std::string error;
        const auto shapes = gt::parse_shapes(
            "08912340 reg:a1=struct:16 field:+0=word:0x08C80000 field:+12=word:0\n"
            "08912341 global:0x08CF2F08=struct:16 field:+4=word:0x08C80000\n",
            &error, kDataBegin, kDataEnd);
        check(error.empty() && shapes.size() == 2u, "a word: field parses in a reg slot and in a global slot");
        if (shapes.size() == 2u) {
            const gt::Shape &shaped = shapes.at(0x08912340u);
            check(shaped.fields.size() == 2u && shaped.fields[0].kind == gt::ShapeFieldKind::Word &&
                      shaped.fields[0].offset == 0u && shaped.fields[0].value == kVtable,
                  "a word: field keeps its literal");
            check(shaped.fields.size() == 2u && shaped.fields[1].kind == gt::ShapeFieldKind::Word && shaped.fields[1].value == 0u,
                  "zero is a valid literal word");
            check(shapes.at(0x08912341u).globals[0].fields.size() == 1u &&
                      shapes.at(0x08912341u).globals[0].fields[0].kind == gt::ShapeFieldKind::Word,
                  "a word: field after a global belongs to its slot");
        }
        const auto rejects = [&](const char *text, const char *why) {
            std::string e;
            gt::parse_shapes(text, &e, kDataBegin, kDataEnd);
            check(!e.empty(), why);
        };
        rejects("08912340 reg:a0=struct:16 field:+0=word:0x09E00000\n", "a literal word in the arena is rejected");
        rejects("08912340 reg:a0=struct:16 field:+0=word:0x08DF74AC\n", "a literal word at the end of the image is rejected");
        rejects("08912340 reg:a0=struct:16 field:+0=word:0x08803FFC\n", "a literal word below the image is rejected");
        rejects("08912340 reg:a0=struct:16 field:+0=word:0xFFFFFFFF\n", "a literal word outside the image is rejected");
        rejects("08912340 reg:a0=struct:16 field:+0=word:0x108C80000\n", "a literal word wider than 32 bits is rejected");
        rejects("08912340 reg:a0=struct:16 field:+0=word:\n", "a word: field without a value is rejected");
        rejects("08912340 reg:a0=struct:8 field:+8=word:0x08C80000\n", "a literal word outside its slot is rejected");

        // The literal is the same in every v2 case of that function and costs no draw: the case is the shape's case
        // without the word fields, but for the literal words (a ptr/int field would move, so compare with only word fields).
        const auto worded = gt::parse_shapes("08912340 reg:a1=struct:32 field:+0=word:0x08C80000 field:+8=word:0\n", &error);
        const auto bare = gt::parse_shapes("08912340 reg:a1=struct:32\n", &error);
        check(error.empty() && worded.size() == 1u && bare.size() == 1u, "a word-only shape and a bare shape parse");
        for (unsigned index = 0u; index < 16u; ++index) {
            const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, index, 1u, layout, &worded);
            const gt::SynthInputs plain = gt::make_synth_case_v2(0x08912340u, index, 1u, layout, &bare);
            check(in.gpr == plain.gpr && in.fpr == plain.fpr, "a literal word moves no register");
            const std::uint32_t at = (in.gpr[1] - layout.arena_base) / 4u;
            bool others_same = true;
            for (std::size_t i = 0; i < in.arena.size(); ++i)
                if (i != at && i != at + 2u && in.arena[i] != plain.arena[i]) others_same = false;
            check(others_same, "a word: field changes no other word");
            check(in.arena[at] == kVtable && in.arena[at + 2u] == 0u, "every v2 case holds the same literal at its offset");
        }
        const auto holders = gt::parse_shapes("08912340 global:0x08CF2F08=struct:16 field:+4=word:0x08C80000\n", &error,
                                              kDataBegin, kDataEnd);
        const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, 0u, 1u, layout, &holders);
        check(in.pokes.size() == 1u, "the globally shaped case pokes its word");
        const std::uint32_t at = in.pokes.empty() ? 0u : (in.pokes[0].value - layout.arena_base) / 4u;
        check(!in.pokes.empty() && in_arena(layout, in.pokes[0].value) && in.arena[at + 1u] == kVtable,
              "the global slot holds the literal word too");
    }

    // shapes file
    {
        std::string error;
        const auto shapes = gt::parse_shapes("# comment\n08912340 reg:a1=struct:64 field:+0x10=ptr field:+0x14=int:0..8\n", &error);
        check(error.empty() && shapes.size() == 1u, "a shapes line parses");
        const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, 0u, 1u, layout, &shapes);
        const std::uint32_t base = in.gpr[1];
        check(in_arena(layout, base), "the shaped register points into the arena");
        const std::uint32_t at = (base - layout.arena_base) / 4u;
        check(in_arena(layout, in.arena[at + 4u]), "the ptr field holds an arena pointer");
        check(in.arena[at + 5u] <= 8u, "the int field is within its range");
        std::string bad_error;
        gt::parse_shapes("08912340 reg:q9=struct:64\n", &bad_error);
        check(!bad_error.empty(), "a malformed shapes line is reported");
    }

    // shapes: hardening
    {
        const auto rejects = [&](const char *text, const char *why) { std::string e; gt::parse_shapes(text, &e); check(!e.empty(), why); };
        rejects("08912340 reg:a0=struct:64 field:+0=int:0..4294967295\n", "a full-range int field is rejected (no zero span)");
        rejects("08912340 reg:a0=struct:64 field:+0=int:5..2\n", "an empty int range is rejected");
        rejects("08912340 reg:a0=struct:64\n08912340 reg:a1=struct:64\n", "a duplicate address is rejected");
        rejects("08912340 reg:a0=struct:64 reg:a1=struct:64\n", "a second reg: word is rejected");
        rejects("1089123400 reg:a0=struct:64\n", "an address above 32 bits is rejected");
        rejects("08912340 reg:a0=struct:64 field:+0x1zz=ptr\n", "garbage in a number is rejected");
        rejects("08912340 reg:a0=struct:0\n", "a zero-size struct is rejected");
        rejects("08912340 reg:a0=struct:4096\n", "a struct larger than a block is rejected");
        rejects("08912340 reg:a0=struct:010 field:+0=ptr\n08912341 reg:a0=struct:abc\n", "a non-numeric size is rejected");
        std::string ok_error;
        const auto ok = gt::parse_shapes("08912340 reg:a0=struct:010 field:+4=int:0x10..0x20\n", &ok_error);
        check(ok_error.empty() && ok.at(0x08912340u).size == 10u && ok.at(0x08912340u).fields[0].high == 0x20u, "decimal sizes (010 is ten, not octal) and 0x ranges parse");
        // a shape struct never overlaps a table block
        gt::SynthLayout big = layout;
        const auto shapes2 = gt::parse_shapes("08912340 reg:a2=struct:1024 field:+0=ptr\n", &ok_error);
        for (unsigned index = 0u; index < 64u; ++index) {
            const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, index, 1u, big, &shapes2);
            const std::uint32_t first = (in.gpr[2] - big.arena_base) / 1024u;
            const std::uint32_t last = (in.gpr[2] - big.arena_base + 1023u) / 1024u;
            check(first == last && first % 8u != 7u, "a shape struct sits inside one block that is not a table");
        }
    }
    // global holders in a v2 case: each global word is poked to a fresh, pointer-closed arena slot; nothing else moves.
    {
        constexpr std::uint32_t kDataBegin = 0x08CD8500u, kDataEnd = 0x08DF74ACu;
        std::string error;
        const auto shapes = gt::parse_shapes(
            "08912340 reg:a1=struct:16 field:+4=ptr "
            "global:0x08CF2F08=struct:32 field:+0=ptr field:+8=int:0..8 "
            "global:0x08CF2F0C=struct:16 field:+4=ptr\n",
            &error, kDataBegin, kDataEnd);
        check(error.empty(), "the reg + globals shape parses");
        const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, 0u, 1u, layout, &shapes);
        check(in.pokes.size() == 2u, "a v2 case carries one poke per global");
        check(in.pokes[0].address == 0x08CF2F08u && in.pokes[1].address == 0x08CF2F0Cu, "the pokes name the global words");
        const std::uint32_t first = in.pokes[0].value, second = in.pokes[1].value;
        check(in_arena(layout, first) && in_arena(layout, second), "each global points into the arena");
        check((first - layout.arena_base) % 16u == 0u && (second - layout.arena_base) % 16u == 0u, "a global slot is 16-byte aligned");
        check(first != second, "each global gets its own slot");
        check(in.gpr[1] != first && in.gpr[1] != second, "the shaped register has a slot of its own");
        const std::uint32_t at = (first - layout.arena_base) / 4u;
        check(in_arena(layout, in.arena[at]) && (in.arena[at] - layout.arena_base) % 16u == 0u,
              "a ptr field of a global slot is a 16-byte-aligned arena pointer");
        check(in.arena[at + 2u] <= 8u, "an int field of a global slot is within its range");
        const std::uint32_t other = (second - layout.arena_base) / 4u;
        check(in_arena(layout, in.arena[other + 1u]), "the second global's ptr field is an arena pointer too");
        // the slots and the register's struct do not overlap
        const auto overlaps = [](std::uint32_t a, std::uint32_t na, std::uint32_t b, std::uint32_t nb) {
            return a < b + nb && b < a + na;
        };
        check(!overlaps(first, 32u, second, 16u), "global slots do not overlap");
        check(!overlaps(first, 32u, in.gpr[1], 16u) && !overlaps(second, 16u, in.gpr[1], 16u), "a global slot does not overlap the register's struct");
        const gt::SynthInputs again = gt::make_synth_case_v2(0x08912340u, 0u, 1u, layout, &shapes);
        check(again.pokes.size() == 2u && again.pokes[0].value == first && again.pokes[1].value == second &&
                  again.gpr == in.gpr && again.arena == in.arena,
              "a global-shaped v2 case is deterministic");
        // a case of a global-shaped function whose line has no reg: still pokes, a0 keeps the plain v2 slot
        const auto only_global = gt::parse_shapes("08912340 global:0x08CF2F08=struct:16\n", &error, kDataBegin, kDataEnd);
        const gt::SynthInputs plain = gt::make_synth_case_v2(0x08912340u, 0u, 1u, layout, &only_global);
        check(plain.pokes.size() == 1u && plain.pokes[0].address == 0x08CF2F08u, "a global-only shape pokes its word");
        check(in_arena(layout, plain.gpr[0]), "a global-only shape leaves the plain v2 a0");
        const gt::SynthInputs v1 = gt::make_synth_case(0x08912340u, 0u, 1u, layout);
        check(v1.pokes.empty(), "a v1 case pokes nothing");
    }

    // a shape without globals keeps the v2 inputs it made before global holders (hash from the pre-change host/synth_case.cpp)
    {
        std::string error;
        const auto shapes = gt::parse_shapes("08912340 reg:a1=struct:64 field:+0x10=ptr field:+0x14=int:0..8\n", &error);
        std::uint64_t h = 1469598103934665603ull;
        const auto mix = [&](std::uint32_t v) { h ^= v; h *= 1099511628211ull; };
        for (unsigned index = 0u; index < 16u; ++index) {
            const gt::SynthInputs in = gt::make_synth_case_v2(0x08912340u, index, 1u, layout, &shapes);
            check(in.pokes.empty(), "a shape without globals pokes nothing");
            for (const std::uint32_t v : in.gpr) mix(v);
            for (const std::uint32_t v : in.fpr) mix(v);
            for (const std::uint32_t v : in.arena) mix(v);
        }
        check(h == 0x703DEAFA0EC10D89ull, "a shape without globals draws the v2 inputs it drew before global holders");
    }
    {
        unsigned n = 99u;
        check(gt::parse_case_count("8", 64u, n) && n == 8u, "a case count parses");
        check(gt::parse_case_count("1000", 64u, n) && n == 64u, "a case count is clamped");
        check(!gt::parse_case_count("abc", 64u, n) && !gt::parse_case_count("", 64u, n) && !gt::parse_case_count("-1", 64u, n) && !gt::parse_case_count("8x", 64u, n),
              "a non-numeric case count is rejected");
    }
    // child report: a v2 kill keeps the v1 results
    {
        const std::string v1 = "8 8 0 0 3\nfirst\nskip\n";
        const gt::ChildReport whole = gt::parse_child_report(v1 + "[v1-end]\n[v2] 5 5 0 3 1\n", false, true);
        check(whole.ok && !whole.v2_lost && whole.compared == 13u && whole.equal == 13u && whole.v2_compared == 5u && whole.informative == 4u, "v1 + v2 reports add up");
        const gt::ChildReport hung = gt::parse_child_report(v1 + "[v1-end]\n", true, true);
        check(hung.ok && hung.v2_lost && hung.compared == 8u && hung.equal == 8u && hung.v2_compared == 0u, "killed during v2: the v1 results are kept");
        const gt::ChildReport early = gt::parse_child_report("8 8 0 0", true, true);
        check(!early.ok, "killed during v1: nothing kept, as before");
        const gt::ChildReport off = gt::parse_child_report(v1, false, false);
        check(off.ok && !off.v2_lost && off.compared == 8u, "with v2 off the report is the old format");
        const gt::ChildReport off_killed = gt::parse_child_report(v1 + "[v1-end]\n", true, false);
        check(!off_killed.ok, "with v2 off a kill loses everything, as before");
        const gt::ChildReport mism = gt::parse_child_report(v1 + "[v1-end]\n[v2] 2 1 1 0 0\n[v2-mismatch] s0 differs\n08912340 1 2 3\n", false, true);
        check(mism.mismatched == 1u && mism.first_mismatch == "first" && mism.other_lines.size() == 1u, "v2 mismatch lines are carried");
    }
    return failures == 0 ? 0 : 1;
}
