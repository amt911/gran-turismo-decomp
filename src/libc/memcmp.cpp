// memcmp from the C library statically linked into GT's executable.
//
// Runtime 0x08B36170 (ELF vaddr 0x332170). Name evidence: behaviour — compares a2 bytes of a0 and
// a1 (unrolled four per iteration, with no test for NUL, so not strncmp) and returns, in v0, the
// difference of the first pair of (unsigned) bytes that differ, or 0; 194 callers.

#include "decomp_registry.hpp"

namespace gt::libc {

void memcmp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t left = ctx.gpr[4];
    const std::uint32_t right = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t a = rt.memory().load8(left + i);
        const std::uint32_t b = rt.memory().load8(right + i);
        if (a != b) {
            ctx.gpr[2] = a - b;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

} // namespace gt::libc

GT_DECOMP(0x08B36170u, gt::libc::memcmp, gt::ReturnKind::Word);
