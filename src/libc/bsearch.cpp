// bsearch from the C library statically linked into GT's executable.
//
// Runtime 0x08B77C14 (ELF vaddr 0x373C14). Name evidence: behaviour — bsearch(key, base, count, size,
// compare) with the comparator as the fifth (EABI t0) argument: halves [0, count) calling
// compare(key, base + size * mid) and returns the matching element or NULL. GT's busiest SDK function
// on the way to a race (1.6 million calls).

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::libc {

void bsearch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[4];
    const std::uint32_t base = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t size = ctx.gpr[7];
    const std::uint32_t compare = ctx.gpr[8];
    ctx.gpr[2] = 0u;
    if (count == 0u || size == 0u) return;
    std::uint32_t low = 0u;
    std::uint32_t high = count;
    while (low < high) {
        const std::uint32_t mid = (low + high) >> 1;  // unsigned, as the original's srl
        const std::uint32_t element = base + size * mid;
        const auto order = static_cast<std::int32_t>(call_guest(rt, ctx, compare, {key, element}));
        if (order < 0) {
            high = mid;
        } else if (order > 0) {
            low = mid + 1u;
        } else {
            ctx.gpr[2] = element;
            return;
        }
    }
}

} // namespace gt::libc

GT_DECOMP(0x08B77C14u, gt::libc::bsearch, gt::ReturnKind::Word);
