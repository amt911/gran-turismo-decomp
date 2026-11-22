// New batch, main session 92 (DRAFT, unverified): the functions of the list that reduce to arithmetic or a fixed fill and
// could be written as structured C from `tools.disasm` with certainty. The long libc-shaped bodies of the list (stdio seek/flush,
// printf core, float conversions) are not written here. No names: none has evidence beyond the shapes noted at each function.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace new_batch_main_92 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// libgcc's 64-bit remainder core. A zero divisor takes the core's `d0 = 1 / d0` step, which the Allegrex divu answers with
// 0xFFFFFFFF (FINDINGS: div by zero does not trap), so the dividend is reduced modulo 0xFFFFFFFF.
u64 remainder_of(u64 dividend, u64 divisor) { return dividend % (divisor != 0u ? divisor : 0xFFFFFFFFu); }

// func_08B6FBA4(a0:a1 = n, a2:a3 = d) -> v0:v1: signed 64-bit remainder (__moddi3): the magnitudes are reduced, the result takes
// the sign of the dividend (s4 = -1 when the dividend is negative; the divisor's sign is dropped).
void func_08B6FBA4(Runtime &, AllegrexContext &ctx) {
    const u64 n = (u64{ctx.gpr[5]} << 32) | ctx.gpr[4];
    const u64 d = (u64{ctx.gpr[7]} << 32) | ctx.gpr[6];
    const bool negative = static_cast<std::int32_t>(ctx.gpr[5]) < 0;
    const u64 n_magnitude = negative ? 0u - n : n;
    const u64 d_magnitude = static_cast<std::int32_t>(ctx.gpr[7]) < 0 ? 0u - d : d;
    u64 r = remainder_of(n_magnitude, d_magnitude);
    if (negative) r = 0u - r;
    ctx.gpr[2] = static_cast<u32>(r);
    ctx.gpr[3] = static_cast<u32>(r >> 32);
}

// func_08B708C4(a0:a1 = n, a2:a3 = d) -> v0:v1: unsigned 64-bit remainder (__umoddi3).
void func_08B708C4(Runtime &, AllegrexContext &ctx) {
    const u64 n = (u64{ctx.gpr[5]} << 32) | ctx.gpr[4];
    const u64 d = (u64{ctx.gpr[7]} << 32) | ctx.gpr[6];
    const u64 r = remainder_of(n, d);
    ctx.gpr[2] = static_cast<u32>(r);
    ctx.gpr[3] = static_cast<u32>(r >> 32);
}

// func_08B72A58(): a table of 306 bytes at 0x08D72BB4 .. 0x08D72CE5 is filled with 4. Afterwards v0 = 4 and v1 = a0 = 0x90000
// (the `lui` of the address halves).
void func_08B72A58(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08D72BB4u;
    constexpr u32 kEntries = 306u;
    for (u32 index = 0u; index < kEntries; ++index) rt.memory().store8(kTable + index, 4u);
    ctx.gpr[2] = 4u;
    ctx.gpr[3] = 0x90000u;
    ctx.gpr[4] = 0x90000u;
}

} // namespace
} // namespace new_batch_main_92
} // namespace gt::game

#define NEW_MAIN92(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_92::name, gt::ReturnKind::kind)
NEW_MAIN92(0x08B6FBA4u, func_08B6FBA4, DoubleWord);
NEW_MAIN92(0x08B708C4u, func_08B708C4, DoubleWord);
NEW_MAIN92(0x08B72A58u, func_08B72A58, Word);
