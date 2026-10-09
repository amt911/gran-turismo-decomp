#include "diff_harness.hpp"

#include "decomp_registry.hpp"
#include "diff_compare.hpp"
#include "psprecomp/common.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gt {

std::vector<DecompiledFunction> &decompiled_functions() {
    static std::vector<DecompiledFunction> functions;
    return functions;
}

namespace {

constexpr std::uint32_t kRamBase = 0x08000000u;
constexpr std::uint32_t kRamSize = 32u * 1024u * 1024u;
// Return address planted for isolated runs: never a registered function, so the run ends when the
// original returns to it.
constexpr std::uint32_t kSentinelReturn = 0x0BADC0DCu;
constexpr std::uint64_t kMaxSteps = 1'000'000u;
// Dead stack below the caller's sp that an original may scribble on (spilled registers).
constexpr std::uint32_t kDeadStackWindow = 64u * 1024u;

struct Entry {
    DecompiledFunction info;
    psprecomp::Runtime::RecompiledFunction original{};
    unsigned compared{};
    unsigned equal{};
    unsigned mismatched{};
    unsigned skipped{};
    unsigned informative{};   // compared cases where the original actually changed memory
    std::uint64_t calls{};    // all calls seen, to spread the compared cases over the run
    std::string first_mismatch;
};

// Compared calls: the first few, then one in kSampleStride, so the cases are not all the same
// boot-time call (e.g. memset of already-zero memory, which can't reveal a missing write).
constexpr std::uint64_t kFirstCases = 4u;
constexpr std::uint64_t kSampleStride = 64u;

DecompMode g_mode = DecompMode::Off;
unsigned g_cases = 16u;
std::unordered_map<std::uint32_t, Entry> g_entries;
std::unordered_set<std::uint32_t> g_import_stubs;
std::unordered_set<std::uint32_t> g_replay_safe_imports;
std::uint64_t g_imports = 0u;

std::vector<std::uint8_t> snapshot_ram(const psprecomp::Runtime &rt) {
    std::vector<std::uint8_t> ram(kRamSize);
    rt.memory().copy_out(kRamBase, ram);
    return ram;
}

void restore_ram(psprecomp::Runtime &rt, const std::vector<std::uint8_t> &ram) {
    rt.memory().copy_in(kRamBase, ram);
}

EndState end_state(const psprecomp::AllegrexContext &ctx, std::vector<std::uint8_t> ram, std::uint32_t entry_sp) {
    EndState state;
    for (unsigned r = 0; r < 32u; ++r) state.gpr[r] = ctx.gpr[r];
    state.f0_bits = ctx.fpr_bits(0);
    state.ram = std::move(ram);
    state.ram_base = kRamBase;
    state.ignore_begin = entry_sp - kDeadStackWindow;
    state.ignore_end = entry_sp;
    return state;
}

// Runs the recompiled original from `ctx` until it returns to kSentinelReturn. False when it would
// enter an import, exceeds the step bound, or stops the runtime: then the case is not comparable.
bool run_original_isolated(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Entry &entry) {
    ctx.gpr[31] = kSentinelReturn;
    ctx.pc = entry.info.address;
    const std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    for (std::uint64_t step = 0; ctx.pc != kSentinelReturn; ++step) {
        const bool replay_safe = g_replay_safe_imports.contains(ctx.pc);
        if (step >= kMaxSteps || (g_import_stubs.contains(ctx.pc) && !replay_safe) || rt.stopped()) return false;
        const auto decompiled = g_entries.find(ctx.pc);  // nested calls run originals, not the harness
        const auto fn = decompiled != g_entries.end() ? decompiled->second.original : rt.function_at(ctx.pc);
        if (fn == nullptr) return false;
        const std::uint64_t imports_before = g_imports;
        fn(rt, ctx);
        ctx.gpr[0] = 0u;
        if (g_imports != imports_before && !replay_safe) return false;
        if (!psprecomp::runtime_thread_switch_generation_matches(generation)) return false;
    }
    return true;
}

void run_decompiled(Entry &entry, psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    entry.info.fn(rt, ctx);
    ctx.gpr[0] = 0u;
    ctx.pc = ctx.gpr[31];
}

void trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = g_entries.find(ctx.pc);
    if (it == g_entries.end()) {
        rt.unsupported(ctx.pc, 0u, "decomp trampoline reached for an unregistered address");
        return;
    }
    Entry &entry = it->second;
    if (g_mode == DecompMode::On) {
        run_decompiled(entry, rt, ctx);
        return;
    }
    const std::uint64_t call = entry.calls++;
    const bool sampled = call < kFirstCases || call % kSampleStride == 0u;
    if (!sampled || entry.compared + entry.skipped >= g_cases) {  // not sampled / budget spent: run as recompiled
        entry.original(rt, ctx);
        return;
    }

    const psprecomp::AllegrexContext entry_ctx = ctx;
    const auto entry_ram = snapshot_ram(rt);

    psprecomp::AllegrexContext original_ctx = entry_ctx;
    if (!run_original_isolated(rt, original_ctx, entry)) {
        ++entry.skipped;
        restore_ram(rt, entry_ram);
        entry.original(rt, ctx);  // no HLE ran yet: replay the call normally
        return;
    }
    original_ctx.gpr[31] = entry_ctx.gpr[31];
    EndState original = end_state(original_ctx, snapshot_ram(rt), entry_ctx.gpr[29]);

    restore_ram(rt, entry_ram);
    psprecomp::AllegrexContext decompiled_ctx = entry_ctx;
    run_decompiled(entry, rt, decompiled_ctx);
    const EndState decompiled = end_state(decompiled_ctx, snapshot_ram(rt), entry_ctx.gpr[29]);

    ++entry.compared;
    for (std::size_t i = 0; i < entry_ram.size(); ++i) {
        const std::uint32_t address = kRamBase + static_cast<std::uint32_t>(i);
        if (entry_ram[i] != original.ram[i] && (address < original.ignore_begin || address >= original.ignore_end)) {
            ++entry.informative;
            break;
        }
    }
    if (const auto difference = first_difference(original, decompiled, entry.info.returns)) {
        ++entry.mismatched;
        if (entry.first_mismatch.empty()) {
            entry.first_mismatch = *difference + " (a0=" + psprecomp::hex32(entry_ctx.gpr[4]) +
                                   " a1=" + psprecomp::hex32(entry_ctx.gpr[5]) +
                                   " a2=" + psprecomp::hex32(entry_ctx.gpr[6]) + ")";
        }
    } else {
        ++entry.equal;
    }

    // The game continues with the original's result, so diff mode never changes the run.
    restore_ram(rt, original.ram);
    ctx = original_ctx;
    ctx.pc = entry_ctx.gpr[31];
}

} // namespace

bool parse_decomp_mode(std::string_view text, DecompMode &mode) {
    if (text == "off") mode = DecompMode::Off;
    else if (text == "on") mode = DecompMode::On;
    else if (text == "diff") mode = DecompMode::Diff;
    else return false;
    return true;
}

void set_import_stubs(std::unordered_set<std::uint32_t> stubs) { g_import_stubs = std::move(stubs); }
void set_replay_safe_imports(std::unordered_set<std::uint32_t> stubs) { g_replay_safe_imports = std::move(stubs); }

void note_import() noexcept { ++g_imports; }

void install_decompiled(psprecomp::Runtime &runtime, DecompMode mode, unsigned cases_per_function) {
    g_mode = mode;
    g_cases = cases_per_function;
    if (mode == DecompMode::Off) return;
    for (const auto &function : decompiled_functions()) {
        const auto original = runtime.function_at(function.address);
        if (original == nullptr) {
            std::cerr << "[decomp] " << function.name << ": no recompiled function at "
                      << psprecomp::hex32(function.address) << ", not installed\n";
            continue;
        }
        g_entries.emplace(function.address, Entry{function, original});
        runtime.register_function(function.address, &trampoline, std::string("decomp:") + function.name);
    }
    std::cerr << "[decomp] mode=" << (mode == DecompMode::On ? "on" : "diff") << " functions=" << g_entries.size()
              << " cases/function=" << cases_per_function << "\n";
}

int decomp_report() {
    if (g_mode == DecompMode::Off) return 0;
    int status = 0;
    for (const auto &[address, entry] : g_entries) {
        std::cerr << "[decomp] " << entry.info.name << " @" << psprecomp::hex32(address);
        if (g_mode == DecompMode::On) {
            std::cerr << ": replaced\n";
            continue;
        }
        std::cerr << ": compared " << entry.compared << ", equal " << entry.equal << ", mismatched "
                  << entry.mismatched << ", not comparable " << entry.skipped << " (calls " << entry.calls
                  << ", cases that changed memory " << entry.informative << ")\n";
        if (!entry.first_mismatch.empty()) std::cerr << "[decomp]   first mismatch: " << entry.first_mismatch << "\n";
        if (entry.mismatched != 0u) status = 2;
        if (entry.compared == 0u) std::cerr << "[decomp]   warning: never compared in this run\n";
    }
    return status;
}

} // namespace gt
