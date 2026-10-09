// memset from the C library statically linked into GT's executable.
//
// Runtime 0x08B36018 (ELF vaddr 0x332018). Name evidence: behaviour — a0 = destination, the low byte
// of a1 is replicated across a word, a2 = byte count, returns the destination in v0 — plus the
// classic unrolled word loop with a computed jump into its store run for the tail (see
// docs/FACTS.md).
//
// Differential status (2026-10-10): v0 equal on every compared case, but no compared case changed
// memory yet — every boot-time call before the run stops fills already-zero memory — so the write
// path is NOT proven. A sabotaged v0 is caught (16/16 mismatches); a sabotaged last byte is not,
// for that reason. Re-check once the run gets further.
//
// The original fills 16 bytes per iteration with aligned word stores; the observable result (every
// byte in [dest, dest + count) set to the value, nothing else touched) is what is reproduced here.

#include "decomp_registry.hpp"

namespace gt::libc {

void memset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const auto value = static_cast<std::uint8_t>(ctx.gpr[5]);
    const std::uint32_t count = ctx.gpr[6];
    for (std::uint32_t i = 0; i < count; ++i) rt.memory().store8(dest + i, value);
    ctx.gpr[2] = dest;
}

} // namespace gt::libc

GT_DECOMP(0x08B36018u, gt::libc::memset, gt::ReturnKind::Word);
