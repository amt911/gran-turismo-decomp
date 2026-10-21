// Comparison used by the differential harness: does a decompiled function leave the same observable
// state as its recompiled original? Pure code, no runtime dependency, unit-tested in host/tests/.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gt {

// What a call leaves behind, as seen by its caller under the PSP (MIPS EABI) calling convention.
struct EndState {
    std::array<std::uint32_t, 32> gpr{};
    std::uint32_t f0_bits{};
    // The VFPU's 128 lanes (bit patterns) and its control registers: no calling convention covers them, and GT's
    // graphics library leaves matrices there for the next call, so a call's VFPU state is part of what it leaves.
    std::array<std::uint32_t, 128> vfpu_bits{};
    std::array<std::uint32_t, 16> vfpu_ctrl{};
    std::vector<std::uint8_t> ram;  // the whole snapshotted RAM after the call
    std::uint32_t ram_base{0x08000000u};
    // Guest range [ignore_begin, ignore_end) excluded from the RAM comparison: the dead stack below
    // the caller's sp, where the original spills registers and a native version writes nothing.
    std::uint32_t ignore_begin{0u};
    std::uint32_t ignore_end{0u};
};

// Which return registers are part of a function's contract. GCC uses v1 and f0 as scratch, so they
// only count when the function really returns through them.
enum class ReturnKind { Void, Word, DoubleWord, Float };

// nullopt when equal. Compared: the return registers `kind` names, callee-saved registers (s0-s7,
// sp, fp, ra), the VFPU registers and every RAM byte outside the ignored range. Caller-saved temporaries never count.
[[nodiscard]] std::optional<std::string> first_difference(const EndState &original, const EndState &decompiled,
                                                          ReturnKind kind = ReturnKind::Word);

// Offset of the first byte where `a` and `b` differ outside the offsets [skip_begin, skip_end);
// nullopt when they're equal there. Block-wise memcmp: the harness compares 32 MiB per case.
[[nodiscard]] std::optional<std::size_t> first_byte_difference(std::span<const std::uint8_t> a,
                                                               std::span<const std::uint8_t> b,
                                                               std::size_t skip_begin, std::size_t skip_end);

// One RAM page either run of a case wrote: its guest address and the bytes each run left there (page_size each).
struct PageImages {
    std::uint32_t address;
    const std::uint8_t *original;
    const std::uint8_t *decompiled;
};

// first_difference's RAM comparison over only the pages either run wrote, sorted by address — the rest of RAM
// is the case's entry state in both runs. Same message, same ignored range [ignore_begin, ignore_end).
[[nodiscard]] std::optional<std::string> first_page_difference(std::span<const PageImages> pages, std::size_t page_size,
                                                               std::uint32_t ignore_begin, std::uint32_t ignore_end);

// Hex addresses (with or without 0x, any case) separated by commas or whitespace, as GT_DECOMP_ONLY
// takes them. nullopt when any token isn't a 32-bit hex number, so a typo can't silently select nothing.
[[nodiscard]] std::optional<std::unordered_set<std::uint32_t>> parse_address_list(std::string_view text);

// The first address registered twice in (address, name) pairs, as a message naming both registrations; a
// second registration would otherwise be ignored silently (the harness keeps the first), so a decompiled
// function in a subsystem file could be shadowed by an older copy and never run.
[[nodiscard]] std::optional<std::string> duplicate_registration(
    const std::vector<std::pair<std::uint32_t, std::string>> &registrations);

} // namespace gt
