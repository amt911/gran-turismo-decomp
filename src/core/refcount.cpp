// Reference counting of the engine's shared objects (pdistd-style intrusive counts).
//
// Runtime 0x08A5BA08 (ELF vaddr 0x257A08), 224 callers. Behaviour: atomically increments the 32-bit
// count at a0 + 8 (an ll/sc loop on the PSP; a single core makes it a plain increment). Its partner
// at vaddr 0x257930 decrements the same count and, when it was 1, calls the object's virtual
// destructor (vtable slot 1) — evidence that +8 is the reference count.

#include "decomp_registry.hpp"

namespace gt::core {

void add_ref(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[4] + 8u;
    rt.memory().store32(count, rt.memory().load32(count) + 1u);
}

} // namespace gt::core

GT_DECOMP(0x08A5BA08u, gt::core::add_ref, gt::ReturnKind::Void);
