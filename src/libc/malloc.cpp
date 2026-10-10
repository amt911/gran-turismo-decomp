// The malloc front ends of the C library statically linked into GT (newlib-style: each function has
// a reentrant _r twin that ignores its struct _reent argument). All of them end in the allocator core
// at 0x08B35840 (memalign(alignment, size), src/libc/memalign.cpp).
//
// Runtime addresses: malloc 0x08B35DA0, _malloc_r 0x08B35DAC. Name evidence: memalign with the
// default alignment 4, and the _r twin's argument shift. calloc (0x08B35DDC), realloc (0x08B35E3C)
// and their _r twins sit right after; GT never calls them in the states reached so far, so they stay
// recompiled until the harness can compare them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::libc {
namespace {

constexpr std::uint32_t kMemalign = 0x08B35840u;
constexpr std::uint32_t kMalloc = 0x08B35DA0u;

} // namespace

// malloc(size): memalign with the default alignment of 4.
void malloc(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, kMemalign, {4u, ctx.gpr[4]});
}

void malloc_r(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, kMalloc, {ctx.gpr[5]});
}

} // namespace gt::libc

GT_DECOMP(0x08B35DA0u, gt::libc::malloc, gt::ReturnKind::Word);
GT_DECOMP(0x08B35DACu, gt::libc::malloc_r, gt::ReturnKind::Word);
