// Synthetic-case input generator: see synth_case.hpp.
//
// The arena is cut in 1 KiB blocks; with trivial functions given, every eighth block is a "table" (a vtable stand-in)
// holding only those functions, and some arena pointers aim at a table's start, so an object whose first word is such a
// pointer makes a virtual call land on real code. The other words are arena pointers, small integers, zero, -1, common
// floats and random words kept out of the code range (a random word there would be a call target the case never meant).
#include "synth_case.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace gt {
namespace {

constexpr std::uint32_t kBlockWords = 256u;
constexpr std::uint32_t kTableEvery = 8u;
constexpr std::uint32_t kCodeBegin = 0x08804000u, kCodeEnd = 0x08D00000u;
// The ELF image at load base 0x08804000: .text through the end of .bss. A `word:` field holds a real address in it (a
// vtable pointer in .rodata/.data), or zero; anything else points outside the guest's memory.
constexpr std::uint32_t kImageBegin = 0x08804000u, kImageEnd = 0x08DF74ACu;
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

// One arena word as the generator draws it (also what a mutated block is refilled with).
std::uint32_t draw_word(const Arena &arena, Random &random) {
    const std::uint32_t pick = random.below(100u);
    return pick < 40u ? arena.pointer(random, 20u)
           : pick < 65u ? random.below(17u)
           : pick < 75u ? 0u
           : pick < 80u ? 0xFFFFFFFFu
           : pick < 90u ? kFloats[random.below(8u)]
                        : outside_code(random.next() << 16 | random.next());
}

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
        inputs.arena[i] = draw_word(arena, random);
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

namespace gt {

std::uint32_t MutationRandom::next() {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return static_cast<std::uint32_t>((z ^ (z >> 31)) >> 16);
}

namespace {

constexpr std::uint32_t kBlockBytes = kBlockWords * 4u;

std::string format_value(const MutationValue &value) {
    char buffer[40];
    switch (value.kind) {
    case ValueKind::Imm: std::snprintf(buffer, sizeof buffer, "imm:0x%X", value.number); break;
    case ValueKind::Arena: std::snprintf(buffer, sizeof buffer, "arena:0x%X", value.number); break;
    case ValueKind::Live: std::snprintf(buffer, sizeof buffer, "live:%u", value.number); break;
    case ValueKind::Class: std::snprintf(buffer, sizeof buffer, "class:%u", value.number); break;
    }
    return buffer;
}

bool parse_number(const std::string &text, std::uint32_t &out) {
    if (text.empty()) return false;
    char *end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 0);
    if (*end != '\0' || value > 0xFFFFFFFFull) return false;
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool parse_value(const std::string &text, MutationValue &value) {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos) return false;
    const std::string kind = text.substr(0, colon);
    if (kind == "imm") value.kind = ValueKind::Imm;
    else if (kind == "arena") value.kind = ValueKind::Arena;
    else if (kind == "live") value.kind = ValueKind::Live;
    else if (kind == "class") value.kind = ValueKind::Class;
    else return false;
    if (!parse_number(text.substr(colon + 1), value.number)) return false;
    return value.kind != ValueKind::Arena || value.number % 4u == 0u;
}

std::uint32_t resolve(const MutationValue &value, std::uint32_t function, const SynthLayout &layout) {
    switch (value.kind) {
    case ValueKind::Imm: return value.number;
    case ValueKind::Arena: return layout.arena_base + (value.number % layout.arena_bytes & ~3u);
    case ValueKind::Live:
        return layout.live_objects.empty() ? 0u : layout.live_objects[value.number % layout.live_objects.size()];
    case ValueKind::Class: {
        const auto it = layout.class_instances.find(function);
        return it == layout.class_instances.end() || it->second.empty() ? 0u : it->second[value.number % it->second.size()];
    }
    }
    return 0u;
}

} // namespace

std::string format_recipe(std::uint32_t function, const Recipe &recipe) {
    char head[64];
    std::snprintf(head, sizeof head, "%08X %u %llu", function, recipe.index, static_cast<unsigned long long>(recipe.seed));
    std::string text = head;
    for (const Mutation &m : recipe.mutations) {
        char where[32];
        switch (m.kind) {
        case MutationKind::Reg: std::snprintf(where, sizeof where, " reg:%u=", m.where); text += where + format_value(m.value); break;
        case MutationKind::Word: std::snprintf(where, sizeof where, " word:0x%X=", m.where); text += where + format_value(m.value); break;
        case MutationKind::Block: std::snprintf(where, sizeof where, " block:%u=%u", m.where, m.value.number); text += where; break;
        }
    }
    return text;
}

bool parse_recipe(const std::string &line, std::uint32_t &function, Recipe &recipe) {
    std::vector<std::string> fields;
    std::size_t at = 0u;
    while (at < line.size()) {
        const std::size_t space = line.find(' ', at);
        const std::size_t end = space == std::string::npos ? line.size() : space;
        if (end > at) fields.push_back(line.substr(at, end - at));
        at = end + 1u;
    }
    if (fields.size() < 3u) return false;
    char *end = nullptr;
    const unsigned long address = std::strtoul(fields[0].c_str(), &end, 16);
    if (fields[0].size() != 8u || *end != '\0') return false;
    std::uint32_t index = 0u, seed = 0u;
    if (!parse_number(fields[1], index) || !parse_number(fields[2], seed)) return false;
    Recipe result;
    result.index = index;
    result.seed = seed;
    for (std::size_t i = 3u; i < fields.size(); ++i) {
        const std::size_t colon = fields[i].find(':'), equals = fields[i].find('=');
        if (colon == std::string::npos || equals == std::string::npos || equals < colon) return false;
        const std::string kind = fields[i].substr(0, colon);
        Mutation m;
        if (!parse_number(fields[i].substr(colon + 1u, equals - colon - 1u), m.where)) return false;
        const std::string rhs = fields[i].substr(equals + 1u);
        if (kind == "reg") {
            m.kind = MutationKind::Reg;
            if (m.where >= 8u || !parse_value(rhs, m.value)) return false;
        } else if (kind == "word") {
            m.kind = MutationKind::Word;
            if (m.where % 4u != 0u || !parse_value(rhs, m.value)) return false;
        } else if (kind == "block") {
            m.kind = MutationKind::Block;
            m.value.kind = ValueKind::Imm;
            if (!parse_number(rhs, m.value.number)) return false;
        } else {
            return false;
        }
        result.mutations.push_back(m);
    }
    function = static_cast<std::uint32_t>(address);
    recipe = result;
    return true;
}

SynthInputs make_recipe_case(std::uint32_t function, const Recipe &recipe, const SynthLayout &layout) {
    SynthInputs inputs = make_synth_case(function, recipe.index, recipe.seed, layout);
    const Arena arena{layout};
    for (const Mutation &m : recipe.mutations) {
        switch (m.kind) {
        case MutationKind::Reg:
            if (m.where < inputs.gpr.size()) inputs.gpr[m.where] = resolve(m.value, function, layout);
            break;
        case MutationKind::Word:
            if (m.where / 4u < inputs.arena.size()) inputs.arena[m.where / 4u] = resolve(m.value, function, layout);
            break;
        case MutationKind::Block: {
            Random random{static_cast<std::uint64_t>(m.value.number) * 0x2545F4914F6CDD1Dull ^ m.where};
            const std::uint32_t first = m.where * kBlockWords;
            for (std::uint32_t i = first; i < first + kBlockWords && i < inputs.arena.size(); ++i)
                if (!arena.is_table(i)) inputs.arena[i] = draw_word(arena, random);
            break;
        }
        }
    }
    return inputs;
}

std::vector<std::uint32_t> hot_offsets(const SynthInputs &inputs, const SynthLayout &layout) {
    std::vector<std::uint32_t> out;
    std::set<std::uint32_t> seen;
    const auto in_arena = [&](std::uint32_t address) {
        return address >= layout.arena_base && address < layout.arena_base + layout.arena_bytes && (address & 3u) == 0u;
    };
    const auto add_range = [&](std::uint32_t address, std::vector<std::uint32_t> &next) {
        const std::uint32_t first = address - layout.arena_base;
        for (std::uint32_t at = first; at < first + 0x100u && at < layout.arena_bytes; at += 4u) {
            if (!seen.insert(at).second) continue;
            out.push_back(at);
            if (in_arena(inputs.arena[at / 4u])) next.push_back(inputs.arena[at / 4u]);
        }
    };
    std::vector<std::uint32_t> level;
    for (const std::uint32_t reg : inputs.gpr)
        if (in_arena(reg)) add_range(reg, level);
    std::vector<std::uint32_t> deeper;
    for (const std::uint32_t address : level) add_range(address, deeper);
    return out;
}

Recipe mutate_recipe(std::uint32_t function, const Recipe &parent, MutationRandom &random, const SynthLayout &layout,
                     const std::vector<std::uint32_t> &hot) {
    Recipe child = parent;
    const std::uint32_t words = layout.arena_bytes / 4u, blocks = layout.arena_bytes / kBlockBytes;
    const auto value = [&]() {
        MutationValue v;
        const std::uint32_t pick = random.below(100u);
        if (pick < 35u) {
            static constexpr std::uint32_t kImms[] = {0u, 1u, 2u, 3u, 4u, 8u, 16u, 0xFFFFFFFFu, 0x7FFFFFFFu, 0x80000000u, 0x3F800000u, 0xBF800000u,
                                                      0x7FC00000u, 0xFFC00000u, 0x00000001u, 0x00800000u};
            v.kind = ValueKind::Imm;
            v.number = pick < 20u ? kImms[random.below(16u)] : pick < 28u ? 1u << random.below(32u) : random.below(256u);
        } else if (pick < 70u) {
            v.kind = ValueKind::Arena;
            v.number = random.below(words) * 4u;
        } else if (pick < 85u || layout.class_instances.find(function) == layout.class_instances.end()) {
            v.kind = ValueKind::Live;
            v.number = random.below(1u << 16);
        } else {
            v.kind = ValueKind::Class;
            v.number = random.below(1u << 16);
        }
        return v;
    };
    Mutation m;
    const std::uint32_t which = random.below(100u);
    if (which < 35u) {
        m.kind = MutationKind::Reg;
        m.where = random.below(8u);
        m.value = value();
    } else if (which < 85u || blocks == 0u) {
        m.kind = MutationKind::Word;
        m.where = !hot.empty() && random.below(10u) < 8u ? hot[random.below(static_cast<std::uint32_t>(hot.size()))] : random.below(words) * 4u;
        m.value = value();
    } else {
        m.kind = MutationKind::Block;
        m.where = random.below(blocks);
        m.value = {ValueKind::Imm, random.next()};
    }
    child.mutations.push_back(m);
    return child;
}

} // namespace gt

// ---- generator v2 ----

namespace gt {
namespace {

constexpr std::uint64_t kV2Stream = 0x56324B0000000000ull;

} // namespace

namespace {

// Decimal digits or 0x-prefixed hex only (no octal), the whole text, at most 32 bits.
bool parse_u32(const std::string &text, bool default_hex, std::uint32_t &out) {
    if (text.empty()) return false;
    bool hex = default_hex;
    std::size_t start = 0u;
    if (text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { hex = true; start = 2u; }
    if (start >= text.size()) return false;
    unsigned long long value = 0u;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (hex && c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
        else if (hex && c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
        else return false;
        value = value * (hex ? 16u : 10u) + digit;
        if (value > 0xFFFFFFFFull) return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

} // namespace

bool parse_case_count(const char *text, unsigned maximum, unsigned &out) {
    std::uint32_t value = 0u;
    if (text == nullptr || !parse_u32(text, false, value)) return false;
    out = value > maximum ? maximum : value;
    return true;
}

std::map<std::uint32_t, Shape> parse_shapes(const std::string &text, std::string *error,
                                            std::uint32_t globals_begin, std::uint32_t globals_end) {
    std::map<std::uint32_t, Shape> shapes;
    std::size_t begin = 0u;
    unsigned line_number = 0u;
    const auto fail = [&](const char *what) {
        if (error != nullptr && error->empty()) *error = "shapes line " + std::to_string(line_number) + ": " + what;
    };
    constexpr int kNoSlot = -2;  // the slot a `field:` belongs to: kNoSlot, -1 (the register), or an index in shape.globals
    while (begin <= text.size()) {
        std::size_t end = text.find('\n', begin);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(begin, end - begin);
        begin = end + 1u;
        ++line_number;
        if (const std::size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        std::vector<std::string> words;
        for (std::size_t i = 0; i < line.size();) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
            std::size_t j = i;
            while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') ++j;
            if (j > i) words.push_back(line.substr(i, j - i));
            i = j;
        }
        if (words.empty()) continue;
        std::uint32_t address = 0u;
        if (!parse_u32(words[0], true, address)) { fail("bad address"); continue; }
        Shape shape;
        int current = kNoSlot;
        bool ok = true;
        const auto slot_size = [&] { return current < 0 ? shape.size : shape.globals[static_cast<std::size_t>(current)].size; };
        for (std::size_t w = 1; w < words.size() && ok; ++w) {
            const std::string &word = words[w];
            if (word.rfind("reg:", 0) == 0) {
                if (shape.has_reg) { fail("more than one reg"); ok = false; break; }
                const std::size_t eq = word.find('=');
                const std::string name = word.substr(4, eq == std::string::npos ? 0 : eq - 4);
                const std::string rest = eq == std::string::npos ? "" : word.substr(eq + 1);
                static const char *const names[8] = {"a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"};
                int found = -1;
                for (int k = 0; k < 8; ++k) if (name == names[k]) found = k;
                if (found < 0 || rest.rfind("struct:", 0) != 0) { fail("bad reg"); ok = false; break; }
                shape.reg = static_cast<unsigned>(found);
                if (!parse_u32(rest.substr(7), false, shape.size) || shape.size == 0u || shape.size > kBlockWords * 4u) {
                    fail("bad size (1..1024)"); ok = false; break;
                }
                shape.has_reg = true;
                current = -1;
            } else if (word.rfind("global:", 0) == 0) {
                if (shape.globals.size() >= 4u) { fail("more than 4 globals"); ok = false; break; }
                const std::size_t eq = word.find('=');
                const std::string rest = eq == std::string::npos ? "" : word.substr(eq + 1);
                GlobalShape global;
                if (eq == std::string::npos || !parse_u32(word.substr(7, eq - 7), false, global.address) ||
                    rest.rfind("struct:", 0) != 0) { fail("bad global"); ok = false; break; }
                if (global.address % 4u != 0u) { fail("unaligned global"); ok = false; break; }
                if (globals_begin >= globals_end || global.address < globals_begin ||
                    static_cast<std::uint64_t>(global.address) + 4u > globals_end) {
                    fail("global outside .data/.bss"); ok = false; break;
                }
                if (!parse_u32(rest.substr(7), false, global.size) || global.size == 0u || global.size > kBlockWords * 4u) {
                    fail("bad size (1..1024)"); ok = false; break;
                }
                for (const GlobalShape &seen : shape.globals)
                    if (seen.address == global.address) { fail("duplicate global"); ok = false; break; }
                if (!ok) break;
                shape.globals.push_back(global);
                current = static_cast<int>(shape.globals.size()) - 1;
            } else if (word.rfind("field:+", 0) == 0) {
                if (current == kNoSlot) { fail("field before a reg or global"); ok = false; break; }
                const std::size_t eq = word.find('=');
                if (eq == std::string::npos) { fail("bad field"); ok = false; break; }
                ShapeField field;
                if (!parse_u32(word.substr(7, eq - 7), false, field.offset)) { fail("bad field offset"); ok = false; break; }
                const std::string kind = word.substr(eq + 1);
                if (kind == "ptr") {
                    field.kind = ShapeFieldKind::Pointer;
                } else if (kind.rfind("int:", 0) == 0) {
                    field.kind = ShapeFieldKind::Integer;
                    const std::size_t dots = kind.find("..");
                    if (dots == std::string::npos || !parse_u32(kind.substr(4, dots - 4), false, field.low) ||
                        !parse_u32(kind.substr(dots + 2), false, field.high)) { fail("bad int range"); ok = false; break; }
                    if (field.high < field.low) { fail("empty int range"); ok = false; break; }
                    if (field.high - field.low > 0xFFFFu) { fail("int range wider than 65535"); ok = false; break; }
                } else if (kind.rfind("word:", 0) == 0) {
                    field.kind = ShapeFieldKind::Word;
                    if (!parse_u32(kind.substr(5), false, field.value)) { fail("bad word value"); ok = false; break; }
                    if (field.value != 0u && (field.value < kImageBegin || field.value >= kImageEnd)) {
                        fail("word outside the ELF image"); ok = false; break;
                    }
                } else { fail("bad field kind"); ok = false; break; }
                if (field.offset % 4u != 0u) { fail("unaligned field"); ok = false; break; }
                if (static_cast<std::uint64_t>(field.offset) + 4u > slot_size()) { fail("field outside the struct"); ok = false; break; }
                if (current < 0) shape.fields.push_back(field);
                else shape.globals[static_cast<std::size_t>(current)].fields.push_back(field);
            } else { fail("unknown word"); ok = false; }
        }
        if (!ok) continue;
        if (!shape.has_reg && shape.globals.empty()) { fail("no reg"); continue; }
        if (!shapes.emplace(address, shape).second) fail("duplicate address");
    }
    return shapes;
}

SynthInputs make_synth_case_v2(std::uint32_t function, unsigned index, std::uint64_t seed, const SynthLayout &layout,
                               const std::map<std::uint32_t, Shape> *shapes) {
    Random random{(seed * 0x100000001B3ull ^ (static_cast<std::uint64_t>(function) << 20) ^ index) ^ kV2Stream};
    const Arena arena{layout};
    const std::uint32_t slots = arena.words() / 4u;  // 16-byte slots
    const auto slot_pointer = [&]() {
        for (;;) {  // a slot that is not inside a table block
            const std::uint32_t slot = random.below(slots);
            if (!arena.is_table(slot * 4u)) return layout.arena_base + slot * 16u;
        }
    };
    const auto table_pointer = [&]() {
        const std::uint32_t tables = arena.words() / (kBlockWords * kTableEvery);
        return layout.arena_base + 4u * ((random.below(tables) * kTableEvery + kTableEvery - 1u) * kBlockWords);
    };
    SynthInputs inputs;
    inputs.arena.resize(arena.words());
    for (std::uint32_t i = 0; i < arena.words(); ++i) {
        if (arena.is_table(i)) {
            inputs.arena[i] = layout.leaf_functions[random.below(static_cast<std::uint32_t>(layout.leaf_functions.size()))];
            continue;
        }
        const std::uint32_t pick = random.below(100u);
        if (pick < 70u) inputs.arena[i] = slot_pointer();
        else if (pick < 80u) inputs.arena[i] = arena.has_tables() ? table_pointer() : slot_pointer();
        else if (pick < 90u) inputs.arena[i] = 0u;
        else if (pick < 95u) inputs.arena[i] = random.below(17u);
        else inputs.arena[i] = kFloats[random.below(8u)];
    }
    for (std::uint32_t &reg : inputs.gpr) {
        const std::uint32_t pick = random.below(100u);
        reg = pick < 60u ? slot_pointer() : pick < 80u ? random.below(17u) : pick < 90u ? 0u : slot_pointer();
    }
    inputs.gpr[0] = slot_pointer();
    for (std::uint32_t &reg : inputs.fpr) reg = kFloats[random.below(8u)];
    if (const auto it = layout.class_instances.find(function); it != layout.class_instances.end() && !it->second.empty())
        inputs.gpr[0] = it->second[(seed + static_cast<std::uint64_t>(index) * 7u) % it->second.size()];
    if (shapes != nullptr) {
        if (const auto it = shapes->find(function); it != shapes->end()) {
            const Shape &shape = it->second;
            // Slots used by this case's register struct and global holders: each is fresh, so no two overlap.
            std::vector<std::pair<std::uint32_t, std::uint32_t>> used;
            const auto fresh_slot = [&](std::uint32_t size) {
                for (;;) {  // inside one block that is not a table (a struct must not overlap a vtable block)
                    const std::uint32_t base = slot_pointer();
                    const std::uint32_t first = (base - layout.arena_base) / (kBlockWords * 4u);
                    const std::uint32_t last = (base - layout.arena_base + size - 1u) / (kBlockWords * 4u);
                    if (first != last || arena.is_table(first * kBlockWords)) continue;
                    bool overlaps = false;
                    for (const auto &[other, length] : used)
                        if (base < other + length && other < base + size) overlaps = true;
                    if (overlaps) continue;
                    used.emplace_back(base, size);
                    return base;
                }
            };
            const auto fill = [&](std::uint32_t base, const std::vector<ShapeField> &fields) {
                const std::uint32_t at = (base - layout.arena_base) / 4u;
                for (const ShapeField &field : fields) {
                    const std::uint32_t word = at + field.offset / 4u;
                    switch (field.kind) {
                    case ShapeFieldKind::Pointer: inputs.arena[word] = slot_pointer(); break;
                    case ShapeFieldKind::Integer:
                        inputs.arena[word] = field.low + random.below(field.high - field.low + 1u);  // span <= 65536
                        break;
                    case ShapeFieldKind::Word: inputs.arena[word] = field.value; break;  // a real vtable, the same in every case
                    }
                }
            };
            if (shape.has_reg) {
                const std::uint32_t base = fresh_slot(shape.size);
                inputs.gpr[shape.reg] = base;
                fill(base, shape.fields);
            }
            for (const GlobalShape &global : shape.globals) {  // global holders (addendum 2026-11-20): poked before both runs
                const std::uint32_t base = fresh_slot(global.size);
                inputs.pokes.push_back({global.address, base});
                fill(base, global.fields);
            }
        }
    }
    return inputs;
}

} // namespace gt
