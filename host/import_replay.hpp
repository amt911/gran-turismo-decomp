// Import record & replay for the diff harness (docs/superpowers/specs/2026-10-13-import-record-replay-design.md).
// In a case, the original's run executes each recordable import for real and records its effect; the
// decompiled run must make the same calls in the same order, and gets the recorded effect applied
// instead of a second execution. Pure: no Runtime, unit-tested in host/tests/import_replay_test.cpp.
#pragma once

#include "kernel_replay_state.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace gt {

// A run of consecutive guest RAM bytes an import wrote.
struct RamRun {
    std::uint32_t address{};
    std::vector<std::uint8_t> bytes;
};

using RegisterChanges = std::vector<std::pair<unsigned, std::uint32_t>>;  // index -> new value

struct ImportRecord {
    std::uint32_t stub{};
    std::uint8_t arg_mask{};              // argument registers the import reads: bit i = a0-a3, t0-t3
    std::array<std::uint32_t, 8> args{};  // as the original passed them
    RegisterChanges gprs;                 // what the HLE changed
    RegisterChanges fprs;                 // (raw float bits)
    std::vector<RamRun> ram;
    KernelReplayState kernel_before;
    KernelReplayState kernel_after;
};

// The bytes that differ between two images of guest RAM starting at `base`, compared 4 KiB at a time.
std::vector<RamRun> ram_changes(std::span<const std::uint8_t> before, std::span<const std::uint8_t> after,
                                std::uint32_t base);
// Writes the runs into an image of guest RAM starting at `base` (runs outside it are ignored).
void apply_ram(std::span<std::uint8_t> ram, std::uint32_t base, const std::vector<RamRun> &runs);

RegisterChanges changed_registers(std::span<const std::uint32_t, 32> before, std::span<const std::uint32_t, 32> after);
void apply_registers(std::span<std::uint32_t, 32> registers, const RegisterChanges &changes);

// Applies to `current` what the import changed between `before` and `after`: a field or a map entry
// (uid -> value) it changed takes its result; everything else keeps `current`'s value, so the final
// comparison still sees the decompiled run's own kernel changes. False (a conflict) when something the
// import changed holds a third value in `current` — neither before nor after: replaying would hide it.
bool apply_kernel_delta(KernelReplayState &current, const KernelReplayState &before, const KernelReplayState &after);

class ImportLog {
public:
    struct Match {
        const ImportRecord *record = nullptr;  // null: `error` says why the call doesn't match
        std::string error;
    };

    void clear() noexcept;
    void append(ImportRecord record);
    std::size_t size() const noexcept { return records_.size(); }

    // The next record, if this call (stub and the arguments in its mask) is the one it recorded.
    Match next(std::uint32_t stub, const std::array<std::uint32_t, 8> &args);
    // A description of the first record the replay never reached, if any.
    std::optional<std::string> leftover() const;

private:
    std::vector<ImportRecord> records_;
    std::size_t next_{};
};

} // namespace gt
