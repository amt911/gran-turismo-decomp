// strlen from the C library statically linked into GT's executable.
//
// Runtime 0x08B35F60 (ELF vaddr 0x331F60). Name evidence: behaviour — a0 = string, returns in v0
// the number of bytes before the first NUL; 146 callers.
//
// The original reads whole aligned words (bytes before an unaligned start are forced non-zero) and
// tests each byte; it writes nothing, so a byte loop gives the same result.

#include "decomp_registry.hpp"

namespace gt::libc {

void strlen(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t string = ctx.gpr[4];
    std::uint32_t length = 0;
    while (rt.memory().load8(string + length) != 0u) ++length;
    ctx.gpr[2] = length;
}

} // namespace gt::libc

GT_DECOMP(0x08B35F60u, gt::libc::strlen, gt::ReturnKind::Word);
