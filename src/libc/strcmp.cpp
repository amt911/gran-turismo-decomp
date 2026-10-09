// strcmp from the C library statically linked into GT's executable.
//
// Runtime 0x08B35FD8 (ELF vaddr 0x331FD8). Name evidence: behaviour — compares the NUL-terminated
// strings in a0 and a1 two bytes per iteration and returns, in v0, the difference of the first pair
// of (unsigned) bytes that differ, or of the terminating NULs; 130 callers.

#include "decomp_registry.hpp"

namespace gt::libc {

void strcmp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t left = ctx.gpr[4];
    const std::uint32_t right = ctx.gpr[5];
    for (std::uint32_t i = 0;; ++i) {
        const std::uint32_t a = rt.memory().load8(left + i);
        const std::uint32_t b = rt.memory().load8(right + i);
        if (a != b || a == 0u) {
            ctx.gpr[2] = a - b;
            return;
        }
    }
}

} // namespace gt::libc

GT_DECOMP(0x08B35FD8u, gt::libc::strcmp, gt::ReturnKind::Word);
