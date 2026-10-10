// The malloc front ends of the C library statically linked into GT (newlib-style: each function has
// a reentrant _r twin that ignores its struct _reent argument). All of them end in the allocator core
// at 0x08B35840 (memalign(alignment, size), src/libc/memalign.cpp).
//
// Runtime addresses: malloc 0x08B35DA0, _malloc_r 0x08B35DAC. Name evidence: memalign with the
// default alignment 4, and the _r twin's argument shift. calloc (0x08B35DDC) and realloc (0x08B35E3C)
// sit right after, by shape (malloc + memset 0; malloc + memcpy + free).

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::libc {
namespace {

constexpr std::uint32_t kMemalign = 0x08B35840u;
constexpr std::uint32_t kMalloc = 0x08B35DA0u;
constexpr std::uint32_t kFree = 0x08B35254u;    // src/libc/free.cpp
constexpr std::uint32_t kMemset = 0x08B36018u;  // src/libc/memset.cpp
constexpr std::uint32_t kMemcpy = 0x08B79A88u;  // src/libc/memcpy.cpp

} // namespace

// malloc(size): memalign with the default alignment of 4.
void malloc(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, kMemalign, {4u, ctx.gpr[4]});
}

void malloc_r(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, kMalloc, {ctx.gpr[5]});
}

// calloc(count, size): malloc(count * size) (the product wraps, unchecked) cleared to zero.
void calloc(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t bytes = ctx.gpr[4] * ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t block = call_guest(rt, ctx, kMalloc, {bytes});
    if (block != 0u) (void)call_guest(rt, ctx, kMemset, {block, 0u, bytes});
    ctx.gpr[2] = block;
}

// realloc(old, size): a new block of `size` bytes with `size` bytes copied from `old` — whatever the
// old block's size (a quirk kept as is) — and `old` freed, even when the new allocation failed.
void realloc(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = ctx.gpr[4];
    const std::uint32_t size = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t block = call_guest(rt, ctx, kMalloc, {size});
    if (block != 0u && old != 0u) (void)call_guest(rt, ctx, kMemcpy, {block, old, size});
    if (old != 0u) (void)call_guest(rt, ctx, kFree, {old});
    ctx.gpr[2] = block;
}

} // namespace gt::libc

GT_DECOMP(0x08B35DA0u, gt::libc::malloc, gt::ReturnKind::Word);
GT_DECOMP(0x08B35DACu, gt::libc::malloc_r, gt::ReturnKind::Word);
// calloc and realloc are never called on the three routes: written, registered once a route reaches them.
// GT_DECOMP(0x08B35DDCu, gt::libc::calloc, gt::ReturnKind::Word);
// GT_DECOMP(0x08B35E3Cu, gt::libc::realloc, gt::ReturnKind::Word);
