#include "diff_compare.hpp"

#include <algorithm>
#include <cstring>

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

// Callee-saved registers: what any caller may rely on after any call returns.
constexpr NamedRegister kCalleeSaved[] = {
    {16, "s0"}, {17, "s1"}, {18, "s2"}, {19, "s3"}, {20, "s4"}, {21, "s5"},
    {22, "s6"}, {23, "s7"}, {29, "sp"}, {30, "fp"}, {31, "ra"},
};
constexpr NamedRegister kV0{2, "v0"};
constexpr NamedRegister kV1{3, "v1"};

std::optional<std::string> register_difference(const EndState &original, const EndState &decompiled,
                                               const NamedRegister &reg) {
    if (original.gpr[reg.index] == decompiled.gpr[reg.index]) return std::nullopt;
    return std::string(reg.name) + ": original " + hex32(original.gpr[reg.index]) + ", decompiled " +
           hex32(decompiled.gpr[reg.index]);
}

} // namespace

std::optional<std::string> first_difference(const EndState &original, const EndState &decompiled,
                                            ReturnKind kind) {
    if (kind == ReturnKind::Word || kind == ReturnKind::DoubleWord) {
        if (auto d = register_difference(original, decompiled, kV0)) return d;
    }
    if (kind == ReturnKind::DoubleWord) {
        if (auto d = register_difference(original, decompiled, kV1)) return d;
    }
    for (const auto &reg : kCalleeSaved) {
        if (auto d = register_difference(original, decompiled, reg)) return d;
    }
    if (kind == ReturnKind::Float && original.f0_bits != decompiled.f0_bits)
        return "f0: original " + hex32(original.f0_bits) + ", decompiled " + hex32(decompiled.f0_bits);
    if (original.ram.size() != decompiled.ram.size())
        return "RAM snapshot sizes differ";
    const auto offset_of = [&](std::uint32_t address) -> std::size_t {
        if (address <= original.ram_base) return 0u;
        return std::min<std::size_t>(address - original.ram_base, original.ram.size());
    };
    const std::size_t skip_begin = offset_of(original.ignore_begin);
    const std::size_t skip_end = std::max(skip_begin, offset_of(original.ignore_end));
    const auto first = first_byte_difference(original.ram, decompiled.ram, skip_begin, skip_end);
    if (!first) return std::nullopt;
    std::size_t differing = 0;
    for (std::size_t j = *first; j < original.ram.size(); ++j)
        differing += original.ram[j] != decompiled.ram[j] && (j < skip_begin || j >= skip_end);
    char bytes[64];
    std::snprintf(bytes, sizeof bytes, ": original %02X, decompiled %02X (%zu bytes differ)",
                  original.ram[*first], decompiled.ram[*first], differing);
    return "RAM at " + hex32(original.ram_base + static_cast<std::uint32_t>(*first)) + bytes;
}

std::optional<std::size_t> first_byte_difference(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b,
                                                 std::size_t skip_begin, std::size_t skip_end) {
    constexpr std::size_t kBlock = 4096u;
    const std::size_t size = std::min(a.size(), b.size());
    const auto scan = [&](std::size_t begin, std::size_t end) -> std::optional<std::size_t> {
        for (std::size_t at = begin; at < end; at += kBlock) {
            const std::size_t length = std::min(kBlock, end - at);
            if (std::memcmp(a.data() + at, b.data() + at, length) == 0) continue;
            for (std::size_t i = at; i < at + length; ++i)
                if (a[i] != b[i]) return i;
        }
        return std::nullopt;
    };
    if (auto found = scan(0u, std::min(skip_begin, size))) return found;
    return scan(std::min(skip_end, size), size);
}

std::optional<std::unordered_set<std::uint32_t>> parse_address_list(std::string_view text) {
    std::unordered_set<std::uint32_t> addresses;
    const auto separator = [](char c) { return c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    std::size_t at = 0u;
    while (at < text.size()) {
        if (separator(text[at])) {
            ++at;
            continue;
        }
        std::size_t end = at;
        while (end < text.size() && !separator(text[end])) ++end;
        std::string_view token = text.substr(at, end - at);
        at = end;
        if (token.size() > 2u && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) token.remove_prefix(2u);
        if (token.empty() || token.size() > 8u) return std::nullopt;
        std::uint32_t value = 0u;
        for (const char c : token) {
            unsigned digit;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else return std::nullopt;
            value = (value << 4u) | digit;
        }
        addresses.insert(value);
    }
    return addresses;
}

} // namespace gt
