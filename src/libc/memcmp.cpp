// memcmp from the C library statically linked into GT's executable.
//
// Runtime 0x08B36170 (ELF vaddr 0x332170). Name evidence: behaviour — compares a2 bytes of a0 and
// a1 (unrolled four per iteration, with no test for NUL, so not strncmp) and returns, in v0, the
// difference of the first pair of (unsigned) bytes that differ, or 0; 194 callers.

#include "decomp_registry.hpp"

namespace gt::libc {

// The count is signed: a2 - 5 < 0 (a count of 1..4, or one with the top bit set) goes straight to the tail, which
// compares at most four bytes; otherwise four bytes per iteration while the remaining count stays >= 0. A "negative"
// count of 0xFDFFB1D2 therefore compares 4 bytes, not 4 billion (synthetic case, PR #352). Each byte pair is loaded
// before the previous pair is tested, as the asm's delay slots do.
void memcmp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto byte = [&](std::uint32_t address) { return static_cast<std::uint32_t>(rt.memory().load8(address)); };
    auto negative = [](std::uint32_t n) { return static_cast<std::int32_t>(n) < 0; };
    std::uint32_t left = ctx.gpr[4];
    std::uint32_t right = ctx.gpr[5];
    std::uint32_t n = ctx.gpr[6];
    std::uint32_t &v0 = ctx.gpr[2];
    if (n == 0u) {
        v0 = 0u;
        return;
    }
    n -= 5u;
    std::uint32_t t0 = byte(left), t1 = 0u, t2 = 0u, t3 = 0u;
    if (!negative(n)) {
        for (;;) {
            t1 = byte(right);
            if (t2 != t3) { v0 = t2 - t3; return; }
            t2 = byte(left + 1u);
            n -= 4u;
            t3 = byte(right + 1u);
            if (t0 != t1) { v0 = t0 - t1; return; }
            t0 = byte(left + 2u);
            left += 4u;
            t1 = byte(right + 2u);
            if (t2 != t3) { v0 = t2 - t3; return; }
            t2 = byte(left - 1u);
            right += 4u;
            t3 = byte(right - 1u);
            if (t0 != t1) { v0 = t0 - t1; return; }
            t0 = byte(left);
            if (negative(n)) break;
        }
        if (t2 != t3) { v0 = t2 - t3; return; }
    }
    n += 4u;
    t1 = byte(right);
    if (n == 0u) { v0 = t0 - t1; return; }
    t2 = byte(left + 1u);
    n -= 1u;
    t3 = byte(right + 1u);
    if (t0 != t1) { v0 = t0 - t1; return; }
    if (n == 0u) { v0 = t2 - t3; return; }
    n -= 1u;
    t0 = byte(left + 2u);
    if (t2 != t3) { v0 = t2 - t3; return; }
    t1 = byte(right + 2u);
    if (n == 0u) { v0 = t0 - t1; return; }
    t2 = byte(left + 3u);
    if (t0 != t1) { v0 = t0 - t1; return; }
    t3 = byte(right + 3u);
    v0 = t2 - t3;
}

} // namespace gt::libc

GT_DECOMP(0x08B36170u, gt::libc::memcmp, gt::ReturnKind::Word);
