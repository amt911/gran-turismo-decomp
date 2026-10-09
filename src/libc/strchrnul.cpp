// strchrnul-style search from the C library statically linked into GT's executable.
//
// Runtime 0x08B35ED0 (ELF vaddr 0x331ED0). Name evidence: behaviour — returns, in v0, the address
// of the first byte of the string in a0 equal to the low byte of a1, or of the terminating NUL if
// there is none (never NULL, unlike strchr).
//
// Quirk (reproduced): the search reads whole aligned words; the bytes before an unaligned start are
// replaced by (c ^ 0xFF) so they never match. For c == 0xFF that replacement is 0, which looks like
// the terminator: with an unaligned string the original returns the aligned word's first address,
// before the string.

#include "decomp_registry.hpp"

namespace gt::libc {

void strchrnul(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t string = ctx.gpr[4];
    const auto wanted = static_cast<std::uint8_t>(ctx.gpr[5]);
    if (wanted == 0xFFu && (string & 3u) != 0u) {
        ctx.gpr[2] = string & ~3u;
        return;
    }
    std::uint32_t at = string;
    for (;; ++at) {
        const std::uint8_t byte = rt.memory().load8(at);
        if (byte == 0u || byte == wanted) break;
    }
    ctx.gpr[2] = at;
}

} // namespace gt::libc

GT_DECOMP(0x08B35ED0u, gt::libc::strchrnul, gt::ReturnKind::Word);
