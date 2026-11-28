// Main batch 2320 (draft-only, unverified): structured C read from tools.disasm.
// 0x0882D758 is the byte-identical twin of 0x0882D838 (0x0882D838 - 0x0882D758 = 0xE0, same 56 instructions and the same
// relocations), which batch 266 decompiled and verified as construct_with_member (src/game/unsorted/hot_batch_266.cpp).
// This draft follows that verified body: the callees see this function's saved registers as the asm sets them (s2 = a0,
// s1 = 0x08CF0000, then s0 = the member and, at the guard, s0 = a0 + 0x18, s1 = the guard and s3 = the record), and one of
// them reads s2 as the object, which the earlier drafts left at the caller's value (the gate's heap writes of `self`).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2320 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }

// 0x0882D758 (a0 = self): the base constructor 0x08A2A154(self), then the vtable 0x08CBBBC0 at +0; a 0x10-byte block from
// 0x08B62344(0x10) is built by 0x08865D1C(block) with 0x08A21F8C(block + 8, 0x08CF6ED8) and 0x08835AA8(block + 0xC,
// 0x08CF6ED8) — the asm re-loads a1 with that same constant before each of the two — and stored at +0x18. The one-time global
// 0x08D75160 (record 0x08D75168 with vtable 0x08CBE908) is initialized when its signed byte is zero and 0x08B6441C(0x08D75160)
// answers non-zero; +0x1C = 0x08D75168, then 0x08860050(block) answers into +0x14 and is v0.
void func_0882D758(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    // The callees of the original see these as the asm sets them (s2 = a0, s1 = the 0x08CF high half; then s0 = the member
    // and, at the guard, s0 = a0 + 0x18, s1 = the guard and s3 = the record): one of them reads s2 as the object, so they are
    // set the same way and restored at the end.
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 saved_s2 = ctx.gpr[18];
    const u32 saved_s3 = ctx.gpr[19];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;
    ctx.gpr[17] = 0x08CF0000u;
    call(rt, ctx, 0x08A2A154u, {self});
    ctx.gpr[2] = 0x08CBBBC0u;
    put(rt, self, 0x08CBBBC0u);
    const u32 member = call(rt, ctx, 0x08B62344u, {0x10u});
    ctx.gpr[16] = member;
    call(rt, ctx, 0x08865D1Cu, {member});
    call(rt, ctx, 0x08A21F8Cu, {member + 8u, 0x08CF6ED8u});
    call(rt, ctx, 0x08835AA8u, {member + 0xCu, 0x08CF6ED8u});
    put(rt, self + 0x18u, member);
    constexpr u32 kGuard = 0x08D75160u;
    constexpr u32 kRecord = 0x08D75168u;
    ctx.gpr[17] = kGuard;
    ctx.gpr[19] = kRecord;
    ctx.gpr[16] = self + 0x18u;
    if (signed_byte(rt, kGuard) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            ctx.gpr[2] = 0x08CBE908u;
            put(rt, kRecord, 0x08CBE908u);
            call(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    ctx.gpr[4] = word(rt, self + 0x18u);
    put(rt, self + 0x18u + 4u, kRecord); // the asm's sw s3, 0x4(s0) with s0 = self + 0x18
    const u32 handle = call(rt, ctx, 0x08860050u, {word(rt, self + 0x18u)});
    put(rt, self + 0x14u, handle);
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
    ctx.gpr[18] = saved_s2;
    ctx.gpr[19] = saved_s3;
    ctx.gpr[2] = handle;
}

} // namespace
} // namespace new_batch_main_2320
} // namespace gt::game

#define NEW2320(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2320::name, gt::ReturnKind::kind)
NEW2320(0x0882D758u, func_0882D758, Word);
