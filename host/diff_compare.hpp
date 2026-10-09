// Comparison used by the differential harness: does a decompiled function leave the same observable
// state as its recompiled original? Pure code, no runtime dependency, unit-tested in host/tests/.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gt {

// What a call leaves behind, as seen by its caller under the PSP (MIPS EABI) calling convention.
struct EndState {
    std::array<std::uint32_t, 32> gpr{};
    std::uint32_t f0_bits{};
    std::vector<std::uint8_t> ram;  // the whole snapshotted RAM after the call
    std::uint32_t ram_base{0x08000000u};
    // Guest range [ignore_begin, ignore_end) excluded from the RAM comparison: the dead stack below
    // the caller's sp, where the original spills registers and a native version writes nothing.
    std::uint32_t ignore_begin{0u};
    std::uint32_t ignore_end{0u};
};

// nullopt when equal. Compared: return values (v0, v1, f0), callee-saved registers (s0-s7, sp, fp,
// ra) and every RAM byte. Caller-saved temporaries are not part of a function's contract.
[[nodiscard]] std::optional<std::string> first_difference(const EndState &original, const EndState &decompiled);

} // namespace gt
