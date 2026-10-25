// Differential harness: runs decompiled functions (host/decomp_registry.hpp) against their
// recompiled originals inside gt_native. Design: docs/superpowers/specs/2026-10-10-phase3-diff-harness-design.md
#pragma once

#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

namespace gt {

enum class DecompMode { Off, On, Diff };

// Parses "off" / "on" / "diff"; returns false for anything else.
bool parse_decomp_mode(std::string_view text, DecompMode &mode);

// An import stub as the harness sees it. In an isolated run of an original:
// - replay_safe: its HLE keeps no state outside guest RAM (snapshotted and restored) and the kernel
//   replay state (host/kernel_replay_state.hpp), and doesn't switch threads when uncontended (LwMutex
//   lock/unlock, interrupts, data cache, the system time, thread priorities, memory blocks). It runs in
//   both runs of a case. A thread switch still makes the case not comparable.
// - recordable: it never switches threads; it runs once, in the original's run, and its effect
//   (registers, guest RAM, kernel replay state) is recorded and applied in the decompiled run, which must
//   make the same calls with the same arguments (`arg_mask`) (host/import_replay.hpp).
// - neither: the run stops before it and the case is not comparable.
struct ImportStub {
    std::uint32_t address{};
    std::string name;
    bool replay_safe{};
    bool recordable{};
    std::uint8_t arg_mask{};  // argument registers it reads: bit i = a0-a3, t0-t3
};
void set_imports(std::vector<ImportStub> stubs);

// Called for every import call (from the runtime import observer).
void note_import() noexcept;

// Replaces each registered decompiled function per `mode`. Call after the generated code and the
// HLE profile are registered. `cases_per_function` bounds the diff work per function.
void install_decompiled(psprecomp::Runtime &runtime, DecompMode mode, unsigned cases_per_function);

// Synthetic cases (docs/superpowers/specs/2026-10-25-synthetic-cases-design.md). GT_SYNTH=<cases per function> in diff
// mode turns live sampling off; run_synthetic_cases then runs that many generated cases of every installed function on
// copies of `thread` (a live thread's context), each restoring the RAM it wrote.
bool synthetic_mode();
void run_synthetic_cases(psprecomp::Runtime &runtime, const psprecomp::AllegrexContext &thread, std::uint64_t seed);

// Prints the per-function report; returns 2 if any decompiled function differed from its original.
int decomp_report();

} // namespace gt
