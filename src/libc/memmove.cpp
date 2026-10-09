// memmove from the C library statically linked into GT's executable.
//
// Runtime 0x08B79BAC (ELF vaddr 0x375BAC), 76 callers. Name evidence: behaviour — copies a2 bytes from
// a1 to a0 backwards when the destination starts inside the source (a1 < a0 < a1 + a2), forwards
// otherwise; it follows memcpy in the library.
//
// Quirk (reproduced, caught by the differential harness): the backward path returns a0 + a2, not
// a0 — the original keeps the destination END in its return register ($10) and never rewinds it.
// The forward path returns a0.

#include "decomp_registry.hpp"

namespace gt::libc {

void memmove(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto &memory = rt.memory();
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    if (source < dest && dest < source + count) {
        for (std::uint32_t i = count; i-- > 0u;) memory.store8(dest + i, memory.load8(source + i));
        ctx.gpr[2] = dest + count;
        return;
    }
    for (std::uint32_t i = 0; i < count; ++i) memory.store8(dest + i, memory.load8(source + i));
    ctx.gpr[2] = dest;
}

} // namespace gt::libc

GT_DECOMP(0x08B79BACu, gt::libc::memmove, gt::ReturnKind::Word);
