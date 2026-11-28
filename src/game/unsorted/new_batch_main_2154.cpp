// New batch main-session 2154: one function previous verdicts drafted and found mismatching (08AF623C; the
// task listed it twice, 352 and 200 bytes, one function).
// What changed from the old drafts (commits 5793c5ba / e5120b8d):
//   0x08AF623C  the old drafts transcribed the loops faithfully but dropped the prologue's register saves.
//               The stack bitmap at sp+0x10 is only eight words (0x08C3FBF0 zeroes 0x20 bytes), while the
//               block count at +0x98 reaches past 256 in the failing route: marking a record id of 256..287
//               lands in the word at sp+0x30, which the prologue filled with the caller's s0 (ids 288..319 in
//               sp+0x34 = s1, 320..351 = s2, 352..383 = s3, 384..415 = s4, 416..447 = ra). The packing pass
//               reads those same words back as the "already referenced" mask, and the epilogue restores
//               s0-s4/ra from them, so the saved values are part of the function's observable behaviour.
//               The gate's diff (original 0x08CD1F48 at dst + 259*0x20, decompiled untouched, then a
//               contiguous run of records) is exactly the first spilled bit the old drafts got wrong.
// The object/table/record shapes are handed over in /home/andres/repos/gt-ops/shapes/shapes_main2154.txt.
// Delay slots are read as "before the call / branch"; v0 is the asm's on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2154 {
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

// 0x08AF623C (a0 = the object, a1 = the destination record array, a2 = the source record array, a3 = its count).
// The caller's s0-s4/ra live at sp+0x30..0x44 (the prologue's stores) because the bitmap at sp+0x10 is eight
// words wide and record ids of 256 and up are marked in those slots; the epilogue loads the same slots back.
void func_08AF623C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 dst = ctx.gpr[5];
    const u32 src = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 0x30u, ctx.gpr[16]);
    put(rt, sp + 0x34u, ctx.gpr[17]);
    put(rt, sp + 0x38u, ctx.gpr[18]);
    put(rt, sp + 0x3Cu, ctx.gpr[19]);
    put(rt, sp + 0x40u, ctx.gpr[20]);
    put(rt, sp + 0x44u, ctx.gpr[31]);
    const auto restore = [&]() {
        ctx.gpr[31] = word(rt, sp + 0x44u);
        ctx.gpr[20] = word(rt, sp + 0x40u);
        ctx.gpr[19] = word(rt, sp + 0x3Cu);
        ctx.gpr[18] = word(rt, sp + 0x38u);
        ctx.gpr[17] = word(rt, sp + 0x34u);
        ctx.gpr[16] = word(rt, sp + 0x30u);
    };
    if (word(rt, self + 0x9Cu) == 0u) {
        ctx.gpr[2] = 0u;
        restore();
        return;
    }
    call(rt, ctx, 0x08C3FBF0u, {sp + 0x10u, 0u});
    if (src != 0u) {
        const u32 end = src + count * 28u;
        u32 rec = src;
        while (rec != end) {
            const u32 id = word(rt, rec);
            const u32 mask_at = sp + 0x10u + ((static_cast<u32>(static_cast<s32>(id) >> 5)) << 2);
            const u32 table = word(rt, self + 0x9Cu);
            put(rt, mask_at, word(rt, mask_at) | (1u << (id & 31u)));
            call(rt, ctx, 0x08AF5CE4u, {dst + (id << 5), table + (id << 7), rec + 4u});
            rec += 0x1Cu;
        }
    }
    const u32 limit = static_cast<u32>(rt.memory().load16(self + 0x98u)) << 7;
    ctx.gpr[2] = limit;
    const u32 table = word(rt, self + 0x9Cu);
    const u32 blk_end = table + limit;
    if (table == blk_end) {
        restore();
        return;
    }
    u32 blk = table;
    u32 out = dst;
    u32 index = 0u;
    do {
        const u32 mask_word = word(rt, sp + 0x10u + ((index >> 5) << 2));
        const u32 clear = ((mask_word >> (index & 31u)) & 1u) ^ 1u;
        ctx.gpr[2] = clear;
        if (clear != 0u) {
            call(rt, ctx, 0x08AF56D8u,
                 {sp, blk + 0x40u, blk + 0x5Cu, static_cast<u32>(rt.memory().load8(blk + 0xCu))});
            put(rt, out, word(rt, sp));
            put(rt, out + 4u, word(rt, sp + 4u));
            put(rt, out + 8u, word(rt, sp + 8u));
            put(rt, out + 0xCu, word(rt, sp + 0xCu));
            put(rt, out + 0x10u, word(rt, blk + 0x50u));
            put(rt, out + 0x14u, word(rt, blk + 0x54u));
            put(rt, out + 0x18u, word(rt, blk + 0x58u));
            rt.memory().store8(out + 0x1Cu, 0u);
        }
        index += 1u;
        blk += 0x80u;
        out += 0x20u;
    } while (blk != blk_end);
    restore();
}

} // namespace
} // namespace new_batch_main_2154
} // namespace gt::game

#define NEW2154(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2154::name, gt::ReturnKind::kind)
NEW2154(0x08AF623Cu, func_08AF623C, Word);
