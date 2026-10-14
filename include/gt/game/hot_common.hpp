// Helpers the hot-function batches (src/game/unsorted/hot_*.cpp) share: guest word access and the PSP's float
// multiply, plus the addresses of the graphics library's state (.bss) that the display-list writers touch.
#pragma once

#include "psprecomp/runtime.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#ifdef GT_HOT_COVERAGE
#include <cstdio>
#include <map>
#include <string>
#endif

namespace gt::game::hot {

// The graphics library's state block: flag word +0x6C, display-list write pointer +0x5C.
inline constexpr std::uint32_t kGuState = 0x08D6CE00u;
inline constexpr std::uint32_t kGuFlags = kGuState + 0x6Cu;
inline constexpr std::uint32_t kGuListPointer = kGuState + 0x5Cu;
// Command blocks the library's helpers take in a0.
inline constexpr std::uint32_t kGuCommandBlock = 0x08D6D0E0u;
inline constexpr std::uint32_t kGuBlock2 = 0x08D6DE70u;

inline std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }
inline void put(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store32(address, value); }
inline float float_at(psprecomp::Runtime &rt, std::uint32_t address) { return std::bit_cast<float>(rt.memory().load32(address)); }
inline void put_float(psprecomp::Runtime &rt, std::uint32_t address, float value) {
    rt.memory().store32(address, std::bit_cast<std::uint32_t>(value));
}

// The FPU's multiply: inf * 0 is the PSP's default NaN (0x7FC00000), not the host's (0xFFC00000).
inline float psp_mul(float a, float b) {
    if ((std::isinf(a) && b == 0.0f) || (std::isinf(b) && a == 0.0f)) return std::bit_cast<float>(0x7FC00000u);
    return a * b;
}

// Branch coverage of the decompiled code for the gate's compared cases: define GT_HOT_COVERAGE in a source file and every
// GT_COVER("tag") counts there; the counts print to stderr at exit ("[cover] tag n"). A no-op otherwise.
#ifdef GT_HOT_COVERAGE
inline void cover(const char *tag) {
    static std::map<std::string, unsigned long> counts;
    ++counts[tag];
    static struct Printer {
        std::map<std::string, unsigned long> *map;
        ~Printer() { for (const auto &[k, v] : *map) std::fprintf(stderr, "[cover] %s %lu\n", k.c_str(), v); }
    } printer{&counts};
}
#define GT_COVER(tag) ::gt::game::hot::cover(tag)
#else
#define GT_COVER(tag) ((void)0)
#endif

} // namespace gt::game::hot
