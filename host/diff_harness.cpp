#include "diff_harness.hpp"

#include "fiber.hpp"
#include "import_replay.hpp"
#include "kernel_isolation.hpp"
#include "kernel_replay_state.hpp"

#include "decomp_registry.hpp"
#include "diff_compare.hpp"
#include "psprecomp/common.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <exception>
#include <optional>
#include <iostream>
#include <map>
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
// Return addresses planted for isolated runs, outside guest memory: the run ends when the original
// returns to its case's one. Each case takes one from a pool, because a case that continues live
// (below) keeps its address until the original really returns there — maybe never (a thread loop).
constexpr std::uint32_t kSentinelBase = 0x0BAD0000u;
constexpr std::uint32_t kSentinelSlots = 1024u;
std::vector<std::uint32_t> g_free_sentinels;
std::uint32_t g_sentinel = 0u;  // the running case's
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
std::uint64_t g_imports = 0u;

// --- Import gate (docs/superpowers/specs/2026-10-13-import-record-replay-design.md) ---------------
// Every import stub is wrapped in diff mode. Live: the HLE runs as usual. While an original runs
// isolated: recordable imports run and are recorded, anything else is noted (registers before the call)
// so an abort can resume just before it. While the decompiled run replays: recordable imports are
// matched against the record and its effect applied instead of running the HLE again.
enum class ImportPhase { Live, Recording, Replaying };
// Sets the gate's phase for one run of a case; back to Live however the run ends (exceptions too).
struct PhaseScope {
    explicit PhaseScope(ImportPhase phase);
    ~PhaseScope();
    PhaseScope(const PhaseScope &) = delete;
    PhaseScope &operator=(const PhaseScope &) = delete;
};
struct Gate {
    ImportStub info;
    psprecomp::Runtime::RecompiledFunction original{};
};
std::unordered_map<std::uint32_t, Gate> g_gates;
ImportPhase g_phase = ImportPhase::Live;
PhaseScope::PhaseScope(ImportPhase phase) { g_phase = phase; }
PhaseScope::~PhaseScope() { g_phase = ImportPhase::Live; }
ImportLog g_log;
constexpr std::size_t kMaxRecordsPerCase = 64u;  // each record costs two RAM snapshots
std::optional<psprecomp::AllegrexContext> g_before_import;  // the last non-recorded import's registers
std::map<std::string, std::uint64_t> g_skips_by_import;    // not-comparable cases per blocking import
std::uint64_t g_unsampled_no_sentinel = 0u;                 // sampled calls run unchecked: pool empty
const bool g_trace = std::getenv("GT_DECOMP_TRACE") != nullptr;  // logs records and live continuations

struct ReplayMismatch : std::runtime_error {
    using std::runtime_error::runtime_error;
};

const Gate *gate_at(std::uint32_t address) {
    const auto it = g_gates.find(address);
    return it != g_gates.end() ? &it->second : nullptr;
}
bool allowed_in_case(const Gate *gate) { return gate != nullptr && (gate->info.replay_safe || gate->info.recordable); }

// Continuing an aborted case live: the original returns to its case's sentinel, whose handler jumps to
// the call's real return address and frees the sentinel.
std::unordered_map<std::uint32_t, std::uint32_t> g_continuations;  // sentinel -> real return address
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

std::array<std::uint32_t, 32> fpr_bits(const psprecomp::AllegrexContext &ctx) {
    std::array<std::uint32_t, 32> bits{};
    for (unsigned r = 0u; r < 32u; ++r) bits[r] = ctx.fpr_bits(r);
    return bits;
}

std::array<std::uint32_t, 8> argument_registers(const psprecomp::AllegrexContext &ctx) {
    return {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]};
}

// The original's run executes a recordable import and records what it did.
void record_import(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Gate &gate) {
    if (g_log.size() >= kMaxRecordsPerCase) {  // too slow to record: give the case up, resume just before
        g_before_import = ctx;
        break_kernel_isolation("more than 64 recorded imports");
        return;
    }
    g_before_import = ctx;  // where to resume live if the kernel refuses the call in isolation
    ImportRecord record;
    record.stub = gate.info.address;
    record.arg_mask = gate.info.arg_mask;
    record.args = argument_registers(ctx);
    record.kernel_before = save_kernel_replay_state();
    const psprecomp::AllegrexContext before = ctx;
    auto ram_before = snapshot_ram(rt);
    const std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    gate.original(rt, ctx);
    if (kernel_isolation_broken()) {  // refused (it would have switched threads): nothing happened
        recycle(std::move(ram_before));
        return;
    }
    if (!psprecomp::runtime_thread_switch_generation_matches(generation)) {
        rt.stop("diff harness: recordable import " + gate.info.name + " switched PSP threads; it must not be "
                "recordable (host/main.cpp)");
        recycle(std::move(ram_before));
        return;
    }
    auto ram_after = snapshot_ram(rt);
    record.gprs = changed_registers(before.gpr, ctx.gpr);
    record.fprs = changed_registers(fpr_bits(before), fpr_bits(ctx));
    // The results always, even when the original's register already held the same value: the
    // decompiled run's stale value there may differ.
    const auto force = [](RegisterChanges &changes, unsigned index, std::uint32_t value) {
        if (std::none_of(changes.begin(), changes.end(), [&](const auto &c) { return c.first == index; }))
            changes.emplace_back(index, value);
    };
    force(record.gprs, 2u, ctx.gpr[2]);
    force(record.gprs, 3u, ctx.gpr[3]);
    force(record.fprs, 0u, ctx.fpr_bits(0));
    g_before_import.reset();  // it ran: an abort later resumes after it, not before
    record.ram = ram_changes(ram_before, ram_after, kRamBase);
    record.kernel_after = save_kernel_replay_state();
    recycle(std::move(ram_before));
    recycle(std::move(ram_after));
    if (g_trace)
        std::cerr << "[decomp-trace] recorded " << gate.info.name << " v0=" << psprecomp::hex32(ctx.gpr[2])
                  << " ram runs " << record.ram.size() << "\n";
    g_log.append(std::move(record));
}

// The decompiled run reaches a recordable import: it must be the original's next call; its recorded
// effect is applied instead of running the HLE a second time.
void replay_import(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Gate &gate) {
    const ImportLog::Match match = g_log.next(gate.info.address, argument_registers(ctx));
    if (match.record == nullptr) throw ReplayMismatch(gate.info.name + ": " + match.error);
    const ImportRecord &record = *match.record;
    apply_registers(ctx.gpr, record.gprs);
    auto fprs = fpr_bits(ctx);
    apply_registers(fprs, record.fprs);
    for (unsigned r = 0u; r < 32u; ++r) ctx.set_fpr_bits(r, fprs[r]);
    for (const RamRun &run : record.ram) rt.memory().copy_in(run.address, run.bytes);
    KernelReplayState kernel = save_kernel_replay_state();
    if (!apply_kernel_delta(kernel, record.kernel_before, record.kernel_after))
        throw ReplayMismatch(gate.info.name + ": kernel state the import changes differs from the original's");
    restore_kernel_replay_state(kernel);
    ctx.pc = ctx.gpr[31];  // the stub returns to its caller
}

void import_gate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Gate *gate = gate_at(ctx.pc);
    if (gate == nullptr) {
        rt.unsupported(ctx.pc, 0u, "import gate reached for an unknown stub");
        return;
    }
    switch (g_phase) {
    case ImportPhase::Live:
        gate->original(rt, ctx);
        return;
    case ImportPhase::Recording:
        if (gate->info.recordable) {
            record_import(rt, ctx, *gate);
            return;
        }
        g_before_import = ctx;
        if (!gate->info.replay_safe) {  // reached without the dispatch check: refuse, resume live before it
            break_kernel_isolation("an import reached outside the dispatch check");
            return;
        }
        gate->original(rt, ctx);
        if (!kernel_isolation_broken()) g_before_import.reset();
        return;
    case ImportPhase::Replaying:
        if (gate->info.recordable) {
            replay_import(rt, ctx, *gate);
            return;
        }
        if (!gate->info.replay_safe)
            throw ReplayMismatch("the decompiled run called " + gate->info.name + ", which the original's run didn't");
        gate->original(rt, ctx);
        return;
    }
}

// Where an aborted case's original returns once it continues live.
void sentinel_return(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto continuation = g_continuations.find(ctx.pc);
    if (continuation == g_continuations.end()) {
        rt.unsupported(ctx.pc, 0u, "diff harness sentinel reached without a pending continuation");
        return;
    }
    g_free_sentinels.push_back(continuation->first);
    ctx.pc = continuation->second;
    g_continuations.erase(continuation);
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

// Runs guest code from ctx.pc until it returns to the case's sentinel. False when it would enter an
// import that isn't replay-safe, switches threads, exceeds the step bound, or stops the runtime:
// then the case is not comparable.
bool run_to_sentinel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    g_last_stop.clear();
    const std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    // Also bounded in time: an original that spins waiting for another thread (which never runs in
    // isolation) would otherwise burn its whole step budget per case and stall the game.
    const auto deadline = std::chrono::steady_clock::now() + kMaxIsolatedTime;
    for (std::uint64_t step = 0; ctx.pc != g_sentinel; ++step) {
        const Gate *gate = gate_at(ctx.pc);
        const bool replay_safe = allowed_in_case(gate);
        if (gate != nullptr && !replay_safe) {
            g_last_stop = "import stub " + psprecomp::hex32(ctx.pc) + " (" + gate->info.name + ")";
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
            g_last_stop = std::string("kernel isolation (") + kernel_isolation_reason() + ")";
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
    ctx.gpr[31] = g_sentinel;
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
    if (sampled && entry.compared + entry.skipped < g_cases && g_free_sentinels.empty()) ++g_unsampled_no_sentinel;
    if (!sampled || entry.compared + entry.skipped >= g_cases || g_free_sentinels.empty()) {  // not sampled / budget spent: run as recompiled
        entry.original(rt, ctx);
        return;
    }

    wait_for_ge_idle();  // a GE list still running would write RAM during the case
    g_sentinel = g_free_sentinels.back();
    g_free_sentinels.pop_back();
    struct SentinelBack {  // back to the pool when the case ends, unless it continues live
        bool keep = false;
        ~SentinelBack() { if (!keep) g_free_sentinels.push_back(g_sentinel); }
    } sentinel_back;
    const psprecomp::AllegrexContext entry_ctx = ctx;
    auto entry_ram = snapshot_ram(rt);
    const KernelReplayState allocator_before = save_kernel_replay_state();

    psprecomp::AllegrexContext original_ctx = entry_ctx;
    bool original_ran;
    bool original_refused = false;  // the kernel refused an import (read before InCase clears the flag)
    g_log.clear();
    g_before_import.reset();
    {
        InCase in_case;
        PhaseScope recording(ImportPhase::Recording);
        original_ran = run_original_isolated(rt, original_ctx, entry);
        original_refused = kernel_isolation_broken();
    }
    if (!original_ran) {
        ++entry.skipped;
        if (entry.first_skip.empty()) entry.first_skip = "original: " + g_last_stop;
        if (g_last_stop.rfind("import stub", 0) == 0) ++g_skips_by_import[g_last_stop];
        if (g_log.size() != 0u && !rt.stopped()) {
            // A recorded import already ran for real: don't run the call again. Continue live from where
            // the isolated run stopped — or, when the kernel refused an import, from just before it.
            recycle(std::move(entry_ram));
            if (original_refused && !g_before_import) {
                rt.stop(std::string("diff harness: the kernel refused a call in ") + entry.info.name +
                        " outside an import gate; the case can't resume live");
                return;
            }
            ctx = original_refused ? *g_before_import : original_ctx;
            if (g_trace)
                std::cerr << "[decomp-trace] " << entry.info.name << " continues live at " << psprecomp::hex32(ctx.pc)
                          << " (" << g_last_stop << ", " << g_log.size() << " recorded, sentinel "
                          << psprecomp::hex32(g_sentinel) << " -> " << psprecomp::hex32(entry_ctx.gpr[31]) << ")\n";
            g_continuations[g_sentinel] = entry_ctx.gpr[31];
            sentinel_back.keep = true;
            return;
        }
        if (g_last_stop.rfind("import inside", 0) == 0) {  // an import ran outside the gate: can't re-run
            rt.stop(std::string("diff harness: ") + entry.info.name + ": " + g_last_stop +
                    " ran an import without its gate; the case can neither be re-run nor continued");
            return;
        }
        restore_ram(rt, entry_ram);
        restore_kernel_replay_state(allocator_before);
        recycle(std::move(entry_ram));
        entry.original(rt, ctx);  // no recordable import ran yet: replay the call normally
        return;
    }
    original_ctx.gpr[31] = entry_ctx.gpr[31];
    EndState original = end_state(original_ctx, snapshot_ram(rt), entry_ctx.gpr[29]);
    const KernelReplayState allocator_original = save_kernel_replay_state();

    restore_ram(rt, entry_ram);
    restore_kernel_replay_state(allocator_before);
    psprecomp::AllegrexContext decompiled_ctx = entry_ctx;
    decompiled_ctx.gpr[31] = g_sentinel;
    bool decompiled_ran = false;
    std::string decompiled_fault;
    std::string replay_error;
    try {
        InCase in_case;
        PhaseScope replaying(ImportPhase::Replaying);
        run_decompiled(entry, rt, decompiled_ctx);
        if (kernel_isolation_broken()) g_last_stop = std::string("kernel isolation (") + kernel_isolation_reason() + ")";
        decompiled_ran = !kernel_isolation_broken() &&
                         (decompiled_ctx.pc == g_sentinel || run_to_sentinel(rt, decompiled_ctx));
        if (decompiled_ran) {
            if (const auto leftover = g_log.leftover()) replay_error = *leftover;
        } else if (g_last_stop.rfind("import stub", 0) == 0) {
            // the original never reached that import (its run would have stopped there): a difference
            replay_error = "the decompiled run called " + g_last_stop.substr(std::string("import stub ").size()) +
                           ", which the original's run didn't";
            decompiled_ran = true;
        }
    } catch (const ReplayMismatch &error) {  // a different import call than the original's
        replay_error = error.what();
        decompiled_ran = true;
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
    auto difference = !replay_error.empty() ? std::optional<std::string>("import replay: " + replay_error)
                      : decompiled_fault.empty()
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

void set_imports(std::vector<ImportStub> stubs) {
    g_gates.clear();
    for (ImportStub &stub : stubs) {
        const std::uint32_t address = stub.address;
        g_gates[address] = Gate{std::move(stub), nullptr};
    }
}

void note_import() noexcept { ++g_imports; }

void install_decompiled(psprecomp::Runtime &runtime, DecompMode mode, unsigned cases_per_function) {
    g_mode = mode;
    g_cases = cases_per_function;
    if (mode == DecompMode::Off) return;
    if (mode == DecompMode::Diff) {  // the import gate and the continuation sentinel only exist in diff mode
        for (auto &[address, gate] : g_gates) {
            gate.original = runtime.function_at(address);
            if (gate.original != nullptr) runtime.register_function(address, &import_gate, "import-gate:" + gate.info.name);
        }
        for (std::uint32_t slot = kSentinelSlots; slot-- > 0u;) {
            const std::uint32_t sentinel = kSentinelBase + 4u * slot;
            runtime.register_function(sentinel, &sentinel_return, "decomp-diff-sentinel");
            g_free_sentinels.push_back(sentinel);
        }
    }
    // GT_DECOMP_ONLY=0xADDR,0xADDR... installs only those functions; the rest run recompiled. A batch's
    // gate and sabotage runs compare just the batch, so the game isn't slowed by ~25,000 cases of
    // functions already proven (docs/agents/decomp-workflow.md). The full gate still runs before a PR.
    std::optional<std::unordered_set<std::uint32_t>> only;
    if (const char *text = std::getenv("GT_DECOMP_ONLY"); text != nullptr && *text != '\0') {
        only = parse_address_list(text);
        if (!only) throw std::runtime_error(std::string("GT_DECOMP_ONLY: not a list of hex addresses: ") + text);
        for (const std::uint32_t address : *only) {
            const bool known = std::any_of(decompiled_functions().begin(), decompiled_functions().end(),
                                           [&](const DecompiledFunction &f) { return f.address == address; });
            if (!known) std::cerr << "[decomp] GT_DECOMP_ONLY: " << psprecomp::hex32(address) << " is not registered\n";
        }
    }
    for (const auto &function : decompiled_functions()) {
        if (only && !only->contains(function.address)) continue;
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
    if (g_unsampled_no_sentinel != 0u)
        std::cerr << "[decomp] warning: " << g_unsampled_no_sentinel
                  << " sampled calls ran unchecked: every sentinel return address was held by a live continuation\n";
    for (const auto &[stop, count] : g_skips_by_import)
        std::cerr << "[decomp] not comparable, stopped at " << stop << ": " << count << " cases\n";
    return status;
}

} // namespace gt
