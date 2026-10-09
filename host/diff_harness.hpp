// Differential harness: runs decompiled functions (host/decomp_registry.hpp) against their
// recompiled originals inside gt_native. Design: docs/superpowers/specs/2026-10-10-phase3-diff-harness-design.md
#pragma once

#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_set>

namespace gt {

enum class DecompMode { Off, On, Diff };

// Parses "off" / "on" / "diff"; returns false for anything else.
bool parse_decomp_mode(std::string_view text, DecompMode &mode);

// Import stub addresses: an isolated run of an original stops before entering one (HLE side
// effects can't be replayed twice), and the case is reported as not comparable.
void set_import_stubs(std::unordered_set<std::uint32_t> stubs);

// Import stubs an isolated original may still call: their HLE keeps no state outside guest RAM
// (which the harness snapshots and restores) and doesn't switch threads when uncontended — LwMutex
// lock/unlock, interrupt suspend/resume, data-cache maintenance. A thread switch still makes the
// case not comparable.
void set_replay_safe_imports(std::unordered_set<std::uint32_t> stubs);

// Called for every import call (from the runtime import observer).
void note_import() noexcept;

// Replaces each registered decompiled function per `mode`. Call after the generated code and the
// HLE profile are registered. `cases_per_function` bounds the diff work per function.
void install_decompiled(psprecomp::Runtime &runtime, DecompMode mode, unsigned cases_per_function);

// Prints the per-function report; returns 2 if any decompiled function differed from its original.
int decomp_report();

} // namespace gt
