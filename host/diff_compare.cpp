#include "diff_compare.hpp"

#include <cstdio>

namespace gt {
namespace {

std::string hex32(std::uint32_t value) {
    char text[11];
    std::snprintf(text, sizeof text, "0x%08X", value);
    return text;
}

struct NamedRegister {
    unsigned index;
    const char *name;
};

// Return values and callee-saved registers: what a caller may rely on after the call returns.
constexpr NamedRegister kContractRegisters[] = {
    {2, "v0"},  {3, "v1"},  {16, "s0"}, {17, "s1"}, {18, "s2"}, {19, "s3"}, {20, "s4"},
    {21, "s5"}, {22, "s6"}, {23, "s7"}, {29, "sp"}, {30, "fp"}, {31, "ra"},
};

} // namespace

std::optional<std::string> first_difference(const EndState &original, const EndState &decompiled) {
    for (const auto &reg : kContractRegisters) {
        if (original.gpr[reg.index] != decompiled.gpr[reg.index])
            return std::string(reg.name) + ": original " + hex32(original.gpr[reg.index]) + ", decompiled " +
                   hex32(decompiled.gpr[reg.index]);
    }
    if (original.f0_bits != decompiled.f0_bits)
        return "f0: original " + hex32(original.f0_bits) + ", decompiled " + hex32(decompiled.f0_bits);
    if (original.ram.size() != decompiled.ram.size())
        return "RAM snapshot sizes differ";
    const auto ignored = [&](std::size_t offset) {
        const std::uint32_t address = original.ram_base + static_cast<std::uint32_t>(offset);
        return address >= original.ignore_begin && address < original.ignore_end;
    };
    for (std::size_t i = 0; i < original.ram.size(); ++i) {
        if (original.ram[i] == decompiled.ram[i] || ignored(i)) continue;
        std::size_t differing = 0;
        for (std::size_t j = i; j < original.ram.size(); ++j)
            differing += original.ram[j] != decompiled.ram[j] && !ignored(j);
        char bytes[64];
        std::snprintf(bytes, sizeof bytes, ": original %02X, decompiled %02X (%zu bytes differ)",
                      original.ram[i], decompiled.ram[i], differing);
        return "RAM at " + hex32(original.ram_base + static_cast<std::uint32_t>(i)) + bytes;
    }
    return std::nullopt;
}

} // namespace gt
