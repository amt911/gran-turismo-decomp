// Main-session batch 2271: one function re-drafted from the asm after its batch-2084 gate mismatch (fix_info.txt:
// v0: original 0x08D70000, decompiled 0x08D7C500). The old draft hardcoded the address pair's pre-relocation value;
// the recompiler normalizes the `lui`/`addiu` pair, so the lone `lui` that builds the guard's address leaves
// high_half(0x08D75128) = 0x08D70000 in v0 (FINDINGS 2026-10-31). Shape: a0 is a 0x20-byte object (writes at +0,
// +0x14, +0x18); its globals 0x08D75128 / 0x08D75130 are not expressible as shapes. No name: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2271 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A lone `lui` holds the high half of the relocated address, `(address + 0x8000) & 0xFFFF0000`, not the address minus
// the immediate the disassembly shows (FINDINGS 2026-10-31: the base's 0x4000 hides in the pair).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// 0x0881CAEC (a0 = this): stores the vtable 0x08CBE450 at +0; allocates a 0x448-byte record (0x08B62344), clears its
// word +0x30, zeroes 30 eight-byte entries from +0x354 (0x088A79AC) and its words 0..0x2C, and stores it at +0x14.
// The one-time table entry 0x08D75130 — built once behind the guard byte 0x08D75128 — goes at +0x18. v0 is the guard's
// last call, or the `lui` half of the guard's address pair (0x08D70000) when the byte was already set.
void func_0881CAEC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 saved_ra = ctx.gpr[31];
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 saved_s2 = ctx.gpr[18];
    const u32 saved_s3 = ctx.gpr[19];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBE450u);
    const u32 node = call(rt, ctx, 0x08B62344u, {0x448u});
    put(rt, node + 0x30u, 0u);
    u32 slot = node + 0x354u;
    for (u32 count = 30u; count != 0u; --count) {
        call(rt, ctx, 0x088A79ACu, {slot, 0u});
        slot += 8u;
    }
    for (u32 offset = 0u; offset <= 0x2Cu; offset += 4u) put(rt, node + offset, 0u);
    put(rt, self + 0x14u, node);
    constexpr u32 kGuard = 0x08D75128u;
    constexpr u32 kEntry = 0x08D75130u;
    const bool build = rt.memory().load8(kGuard) == 0u && call(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u;
    if (build) {
        put(rt, kEntry, 0x08CBE560u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    } else {
        // The address pair's `lui` half stays in v0 on this path.
        ctx.gpr[2] = high_half(kGuard);
    }
    put(rt, self + 0x18u, kEntry);
    ctx.gpr[31] = saved_ra;
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
    ctx.gpr[18] = saved_s2;
    ctx.gpr[19] = saved_s3;
}

} // namespace
} // namespace new_batch_main_2271
} // namespace gt::game

#define NEW2271(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2271::name, gt::ReturnKind::kind)
NEW2271(0x0881CAECu, func_0881CAEC, Word);
