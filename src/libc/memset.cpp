// memset from the C library statically linked into GT's executable.
//
// Runtime 0x08B36018 (ELF vaddr 0x332018). Name evidence: behaviour — a0 = destination, the low byte
// of a1 is replicated across a word, a2 = byte count, returns the destination in v0 — plus the
// classic unrolled word loop with a computed jump into its store run for the tail (see
// docs/FACTS.md).
//
// Behaviour (reproduced, including its quirk):
//   - count < 9: exactly `count` bytes are written.
//   - count >= 9: the value is replicated to a word; an unaligned head is stored with swr, the body
//     with word stores (64- and 16-byte blocks, then single words), and the tail with
//     `swl value, 0(dest + count)`. A correct memset would use dest + count - 1: with little-endian
//     swl semantics this stores from the start of that last word up to and INCLUDING dest + count,
//     so whenever dest + count is not word-aligned GT's memset writes one byte past the buffer.
//     The recompiled original does exactly that; the differential harness caught the difference.

#include "decomp_registry.hpp"

namespace gt::libc {

void memset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const auto value = static_cast<std::uint8_t>(ctx.gpr[5]);
    const std::uint32_t count = ctx.gpr[6];
    for (std::uint32_t i = 0; i < count; ++i) rt.memory().store8(dest + i, value);
    // Quirk of the original tail store (see above): one extra byte at dest + count.
    if (count >= 9u && ((dest + count) & 3u) != 0u) rt.memory().store8(dest + count, value);
    ctx.gpr[2] = dest;
}

} // namespace gt::libc

// Registered Word, not DoubleWord: contract_audit lists v1 because the caller at 0x08B2FCF0 starts with an lwl/lwr pair
// into v1, and the scan counts the lwl's merge as a read. The pair loads all four bytes, so v1 is never read (false positive).
GT_DECOMP(0x08B36018u, gt::libc::memset, gt::ReturnKind::Word);
