#include "diff_harness.hpp"

#include "fiber.hpp"
#include "kernel_isolation.hpp"
#include "kernel_replay_state.hpp"

#include "decomp_registry.hpp"
#include "diff_compare.hpp"
#include "psprecomp/common.hpp"

#include <chrono>
#include <exception>
#include <optional>
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
constexpr auto kMaxIsolatedTime = std::chrono::milliseconds(250);
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
    std::uint64_t next_sample{};
    std::string first_mismatch;
    std::string first_skip;   // why the first not-comparable case was dropped
};

// Why the last run_to_sentinel gave up (for the report).
std::string g_last_stop;

// Compared calls: the first few, then geometrically further apart (each ~1.6x the previous call
// index, plus a small jitter), so a function called 100,000 times is still compared late in the run
// and the samples don't lock onto one call site of a periodic loop. A fixed 1-in-64 stride spent the
// whole budget in the first ~2,000 calls (boot) and kept hitting the same trivial site
// (docs/FINDINGS.md).
constexpr std::uint64_t kFirstCases = 4u;

std::uint64_t next_sample_after(std::uint64_t call) {
    const std::uint64_t jitter = ((call + 1u) * 0x9E3779B97F4A7C15ull) >> 61u;  // 0..7
    return call + 1u + call * 3u / 5u + jitter;
}

DecompMode g_mode = DecompMode::Off;
unsigned g_cases = 32u;
std::unordered_map<std::uint32_t, Entry> g_entries;
std::unordered_set<std::uint32_t> g_import_stubs;
std::unordered_set<std::uint32_t> g_replay_safe_imports;
std::uint64_t g_imports = 0u;
// True while a case runs (original or decompiled): nested calls to decompiled functions then run
// their originals, so a case never opens another case inside itself and both runs see the same
// callees (each callee is compared on its own).
bool g_in_case = false;

// Both runs of a case are isolated from the kernel (host/kernel_isolation.hpp) for their whole
// length — the decompiled run too: its call_guest loops dispatch guest code just the same.
struct InCase {
    InCase() {
        g_in_case = true;
        set_kernel_isolated(true);
    }
    ~InCase() {
        g_in_case = false;
        set_kernel_isolated(false);
    }
};

// RAM snapshots reuse a few 32 MiB buffers: allocating (and faulting in) a fresh one per snapshot
// cost more than the copy.
std::vector<std::vector<std::uint8_t>> g_spare_buffers;

std::vector<std::uint8_t> snapshot_ram(const psprecomp::Runtime &rt) {
    std::vector<std::uint8_t> ram;
    if (!g_spare_buffers.empty()) {
        ram = std::move(g_spare_buffers.back());
        g_spare_buffers.pop_back();
    }
    ram.resize(kRamSize);
    rt.memory().copy_out(kRamBase, ram);
    return ram;
}

void recycle(std::vector<std::uint8_t> &&ram) {
    if (g_spare_buffers.size() < 4u) g_spare_buffers.push_back(std::move(ram));
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

// Runs guest code from ctx.pc until it returns to kSentinelReturn. False when it would enter an
// import that isn't replay-safe, switches threads, exceeds the step bound, or stops the runtime:
// then the case is not comparable.
bool run_to_sentinel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    // Also bounded in time: an original that spins waiting for another thread (which never runs in
    // isolation) would otherwise burn its whole step budget per case and stall the game.
    const auto deadline = std::chrono::steady_clock::now() + kMaxIsolatedTime;
    for (std::uint64_t step = 0; ctx.pc != kSentinelReturn; ++step) {
        const bool replay_safe = g_replay_safe_imports.contains(ctx.pc);
        if (g_import_stubs.contains(ctx.pc) && !replay_safe) {
            g_last_stop = "import stub " + psprecomp::hex32(ctx.pc);
            return false;
        }
        if (step >= kMaxSteps || rt.stopped()) {
            g_last_stop = rt.stopped() ? "runtime stopped" : "step bound";
            return false;
        }
        if ((step & 0x3FFu) == 0x3FFu && std::chrono::steady_clock::now() > deadline) {
            g_last_stop = "time bound";
            return false;
        }
        const auto decompiled = g_entries.find(ctx.pc);  // nested calls run originals, not the harness
        const auto fn = decompiled != g_entries.end() ? decompiled->second.original : rt.function_at(ctx.pc);
        if (fn == nullptr) {
            g_last_stop = "no function at " + psprecomp::hex32(ctx.pc);
            return false;
        }
        const std::uint64_t imports_before = g_imports;
        fn(rt, ctx);
        ctx.gpr[0] = 0u;
        if (g_imports != imports_before && !replay_safe) {
            g_last_stop = "import inside " + psprecomp::hex32(ctx.pc);
            return false;
        }
        if (kernel_isolation_broken()) {
            g_last_stop = "kernel isolation (LwMutex contention)";
            return false;
        }
        if (!psprecomp::runtime_thread_switch_generation_matches(generation)) {
            g_last_stop = "thread switch";
            return false;
        }
    }
    return true;
}

bool run_original_isolated(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Entry &entry) {
    ctx.gpr[31] = kSentinelReturn;
    ctx.pc = entry.info.address;
    return run_to_sentinel(rt, ctx);
}

// Runs the decompiled function. Returns to ra unless it ended with gt::tail_call, in which case
// ctx.pc is the tail target and the guest code there returns to ra by itself.
void run_decompiled(Entry &entry, psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.pc = entry.info.address;
    entry.info.fn(rt, ctx);
    ctx.gpr[0] = 0u;
    if (ctx.pc == entry.info.address) ctx.pc = ctx.gpr[31];
}

void trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = g_entries.find(ctx.pc);
    if (it == g_entries.end()) {
        rt.unsupported(ctx.pc, 0u, "decomp trampoline reached for an unregistered address");
        return;
    }
    Entry &entry = it->second;
    if (g_mode == DecompMode::On) {
        fiber::run(rt, ctx, entry.info.fn, entry.info.address);  // may block: see host/fiber.hpp
        return;
    }
    if (g_in_case) {
        entry.original(rt, ctx);
        return;
    }
    const std::uint64_t call = entry.calls++;
    // GT_DECOMP_DENSE=0xADDR compares every call of that one function (up to the case budget), to
    // reach branches the spread-out samples miss.
    static const std::uint32_t dense = [] {
        const char *text = std::getenv("GT_DECOMP_DENSE");
        return text != nullptr ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0)) : 0u;
    }();
    const bool sampled = call < kFirstCases || call == entry.next_sample || entry.info.address == dense;
    if (sampled && call >= kFirstCases) entry.next_sample = next_sample_after(call);
    if (call + 1u == kFirstCases) entry.next_sample = next_sample_after(call);
    if (!sampled || entry.compared + entry.skipped >= g_cases) {  // not sampled / budget spent: run as recompiled
        entry.original(rt, ctx);
        return;
    }

    wait_for_ge_idle();  // a GE list still running would write RAM during the case
    const psprecomp::AllegrexContext entry_ctx = ctx;
    auto entry_ram = snapshot_ram(rt);
    const KernelReplayState allocator_before = save_kernel_replay_state();

    psprecomp::AllegrexContext original_ctx = entry_ctx;
    bool original_ran;
    {
        InCase in_case;
        original_ran = run_original_isolated(rt, original_ctx, entry);
    }
    if (!original_ran) {
        ++entry.skipped;
        if (entry.first_skip.empty()) entry.first_skip = "original: " + g_last_stop;
        restore_ram(rt, entry_ram);
        restore_kernel_replay_state(allocator_before);
        recycle(std::move(entry_ram));
        entry.original(rt, ctx);  // no HLE ran yet: replay the call normally
        return;
    }
    original_ctx.gpr[31] = entry_ctx.gpr[31];
    EndState original = end_state(original_ctx, snapshot_ram(rt), entry_ctx.gpr[29]);
    const KernelReplayState allocator_original = save_kernel_replay_state();

    restore_ram(rt, entry_ram);
    restore_kernel_replay_state(allocator_before);
    psprecomp::AllegrexContext decompiled_ctx = entry_ctx;
    decompiled_ctx.gpr[31] = kSentinelReturn;
    bool decompiled_ran = false;
    std::string decompiled_fault;
    try {
        InCase in_case;
        run_decompiled(entry, rt, decompiled_ctx);
        if (kernel_isolation_broken()) g_last_stop = "kernel isolation (LwMutex contention)";
        decompiled_ran = !kernel_isolation_broken() &&
                         (decompiled_ctx.pc == kSentinelReturn || run_to_sentinel(rt, decompiled_ctx));
    } catch (const std::exception &error) {  // e.g. a guest memory access the original never made
        decompiled_fault = error.what();
        decompiled_ran = true;
    }
    if (!decompiled_ran) {
        ++entry.skipped;
        // the decompiled run (or its tail target) left what the harness can replay: keep the original's
        if (entry.first_skip.empty()) entry.first_skip = "decompiled: " + g_last_stop;
        restore_ram(rt, original.ram);
        restore_kernel_replay_state(allocator_original);
        recycle(std::move(entry_ram));
        recycle(std::move(original.ram));
        ctx = original_ctx;
        ctx.pc = entry_ctx.gpr[31];
        return;
    }
    decompiled_ctx.gpr[31] = entry_ctx.gpr[31];
    EndState decompiled = end_state(decompiled_ctx, snapshot_ram(rt), entry_ctx.gpr[29]);

    ++entry.compared;
    const std::size_t skip_begin = original.ignore_begin - kRamBase;
    const std::size_t skip_end = original.ignore_end - kRamBase;
    if (first_byte_difference(entry_ram, original.ram, skip_begin, skip_end)) ++entry.informative;
    auto difference = decompiled_fault.empty()
                          ? first_difference(original, decompiled, entry.info.returns)
                          : std::optional<std::string>("decompiled run faulted: " + decompiled_fault);
    if (!difference && save_kernel_replay_state() != allocator_original)
        difference = "kernel state differs (memory blocks or thread priorities)";
    if (difference) {
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
    restore_kernel_replay_state(allocator_original);
    recycle(std::move(entry_ram));
    recycle(std::move(original.ram));
    recycle(std::move(decompiled.ram));
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
    if (g_mode == DecompMode::On)
        std::cerr << "[decomp] fibers: " << fiber::created() << " created, " << fiber::suspensions()
                  << " suspensions (a decompiled call waited for its PSP thread)\n";
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
        if (!entry.first_skip.empty()) std::cerr << "[decomp]   first not comparable: " << entry.first_skip << "\n";
        if (entry.mismatched != 0u) status = 2;
        if (entry.compared == 0u) std::cerr << "[decomp]   warning: never compared in this run\n";
    }
    return status;
}

} // namespace gt
