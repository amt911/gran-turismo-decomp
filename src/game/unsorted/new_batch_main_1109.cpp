// New batch main-1109 (draft, unverified): 29 small functions (28-80 bytes) written by hand from `tools.disasm`
// (relocations applied, every branch read as the target it prints) as structured C, in the style of new_batch_3.cpp.
// The gate had no useful inputs for these, so the input shapes that make each case runnable live in
// gt-ops/shapes/shapes_main1109.txt.
// Delay slots are read as "before the branch / call"; a callee-saved register the body writes is set where the asm sets
// it (a callee may read it) and put back before the return or the tail call; `v0` ends as the asm leaves it on every exit
// path. A call passes every argument register the asm sets before the `jal`, in EABI order; a register the asm leaves
// alone keeps the caller's value (call_guest starts from the caller's context). The frame of each body is the original's
// `addiu sp, sp, -N`, on the signature line so `tools.frame_audit` reads it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1109 {
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
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// --- 28 bytes: thunks that only forward an argument register ---

// 0x08AD96BC: 0x08AD95EC(1) — the delay slot sets a0; the other argument registers are the caller's.
void func_08AD96BC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AD95ECu, {1u});
}

// 0x08AE0814: 0x08AE0830(caller's a0-a2, 0) — the delay slot clears a3.
void func_08AE0814(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[7] = 0u;
    call(rt, ctx, 0x08AE0830u);
}

// 0x08AE1D50: 0x08AE1D20 with the caller's argument registers untouched.
void func_08AE1D50(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AE1D20u);
}

// 0x08AEF93C: 0x08AE9534: a `lw v0, 0x4540($v1)` getter of the word at 0x08D40A40.
void func_08AEF93C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AE9534u);
}

// 0x08AEF9B0: 0x08AEF958 with the caller's argument registers untouched.
void func_08AEF9B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AEF958u);
}

// 0x08AEFA40: 0x08AEF9CC with the caller's argument registers untouched.
void func_08AEFA40(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AEF9CCu);
}

// 0x08B19E30: 0x08C4D2E4(caller's a0, a1 & 0xFF) — the delay slot masks the second argument to a byte.
void func_08B19E30(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4D2E4u, {ctx.gpr[4], ctx.gpr[5] & 0xFFu});
}

// 0x08B19E68: 0x08C4D7D4 with all four argument registers the caller passed.
void func_08B19E68(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4D7D4u);
}

// 0x08B19EA0: 0x08C4DC48(caller's a0, a1).
void func_08B19EA0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4DC48u);
}

// 0x08B19EBC: 0x08C4DF20(caller's a0, a1).
void func_08B19EBC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4DF20u);
}

// 0x08B19ED8: 0x08C4E244(caller's a0, a1).
void func_08B19ED8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4E244u);
}

// 0x08B19EF4: 0x08C4E448(caller's a0, a1).
void func_08B19EF4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C4E448u);
}

// 0x08B1A06C: 0x08B1A02C(*(u32 *)a0) — the delay slot loads the object word out of a0.

// 0x08B3498C: 0x08B34890(caller's a0, 1) — the delay slot sets a1.
void func_08B3498C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B34890u, {ctx.gpr[4], 1u});
}

// 0x08B5095C: 0x08C53264(caller's a0-a2).
void func_08B5095C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C53264u);
}

// 0x08B50978: 0x08C5355C(caller's a0-a3).
void func_08B50978(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C5355Cu);
}

// 0x08B77AAC: 0x08C7AB10 with the caller's argument registers untouched.

// --- 32 bytes ---

// 0x08A5CC88: 0x08A5CB28(caller's a0) answers a slot; v0 = the word it holds.
void func_08A5CC88(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 slot = call(rt, ctx, 0x08A5CB28u);
    ctx.gpr[2] = word(rt, slot);
}

// 0x08AB5F18: 0x08C39AF8(caller's a0) and v0 = (v0 != 0) (`sltu v0, zero, v0`).
void func_08AB5F18(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08C39AF8u);
    ctx.gpr[2] = answer != 0u ? 1u : 0u;
}

// 0x08B096F8: 0x08B36170(caller's a0, a1, 0x28) — the delay slot sets a2 — and v0 = (v0 < 1) (unsigned, `sltiu v0, v0, 1`).
void func_08B096F8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B36170u, {ctx.gpr[4], ctx.gpr[5], 0x28u});
    ctx.gpr[2] = answer < 1u ? 1u : 0u;
}

// 0x08B09718: 0x08B35FD8(caller's a0, a1) and v0 = (v0 < 1) (unsigned, `sltiu v0, v0, 1`).
void func_08B09718(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B35FD8u);
    ctx.gpr[2] = answer < 1u ? 1u : 0u;
}

// 0x08B1E2B4: 0x08B1E048(caller's a0) and v0 = (v0 != 0) (`sltu v0, zero, v0`).
void func_08B1E2B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B1E048u);
    ctx.gpr[2] = answer != 0u ? 1u : 0u;
}

// 0x08B797D0: 0x08B796C4() answers the pointer the tail call 0x08B79788 goes on with in a0; the delay slot of the `j`
// restores sp, so the frame is gone before the tail call.
void func_08B797D0(Runtime &rt, AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = call(rt, ctx, 0x08B796C4u);
    }
    tail_call(ctx, 0x08B79788u, {object});
}

// --- 36 bytes ---

// 0x08ADA4B4: 0x08ADA094(caller's a0, 0, caller's a1, 0) — a2 receives a1 first, then a1 is cleared in the delay slot.
void func_08ADA4B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    call(rt, ctx, 0x08ADA094u, {self, 0u, second, 0u});
}

// 0x08AFC460: 0x08AFC360(a local 0x50-byte block at sp, caller's a0, caller's a1); a2 receives a1 and a1 receives a0
// before the delay slot hands sp over as a0.
void func_08AFC460(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    call(rt, ctx, 0x08AFC360u, {frame.at(0u), first, second});
}

// 0x08B21CE0: 0x08B21C64(caller's a0) answers a base whose halfword at +0 is its length: v0 = base + length (unsigned
// halfword, 32-bit wrap), v1 = the halfword.
void func_08B21CE0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 base = call(rt, ctx, 0x08B21C64u);
    const u32 length = rt.memory().load16(base);
    ctx.gpr[3] = length;
    ctx.gpr[2] = base + length;
}

// 0x08B28BBC (a0 = this, a1 / a2 = two words of it, a3 = a byte): stores a3 at +0x57, a1 at +0x68 and a2 at +0x6C (the
// last one in the `jal` delay slot, so before the call) and 0x08B28768(this) finishes with the caller's a0-a3.
void func_08B28BBC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put_byte(rt, self + 0x57u, ctx.gpr[7]);
    put(rt, self + 0x68u, ctx.gpr[5]);
    put(rt, self + 0x6Cu, ctx.gpr[6]);
    call(rt, ctx, 0x08B28768u);
}

// --- 40 bytes ---

// 0x08A5BD0C: the varargs register-save area of a frame 0x20: a1-a3 and t0-t3 go to sp+4 .. sp+0x1C; v0 is the caller's.
void func_08A5BD0C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0x04u), ctx.gpr[5]);
    put(rt, frame.at(0x08u), ctx.gpr[6]);
    put(rt, frame.at(0x0Cu), ctx.gpr[7]);
    put(rt, frame.at(0x10u), ctx.gpr[8]);
    put(rt, frame.at(0x14u), ctx.gpr[9]);
    put(rt, frame.at(0x18u), ctx.gpr[10]);
    put(rt, frame.at(0x1Cu), ctx.gpr[11]);
}

// 0x08A5CC60 (a0 = a word): 0x08A5CB28(caller's a0) answers a slot and a0 is stored there; s0 carries a0 across the call
// and comes back as the caller's. v0 stays the call's answer.
void func_08A5CC60(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 saved_s0 = ctx.gpr[16];
    ctx.gpr[16] = self;
    const u32 slot = call(rt, ctx, 0x08A5CB28u);
    ctx.gpr[16] = saved_s0;
    put(rt, slot, self);
}

// 0x08AA03C0 (a0 = an object, f12 = a value): f12 goes to a local float at sp, 0x08AA027C(&local, object, f12) runs and
// f0 is the float the callee left in that local.
void func_08AA03C0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    put_float(rt, frame.at(0u), value);
    const GuestResult result = call_guest(rt, ctx, 0x08AA027Cu, {frame.at(0u), self}, {value});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = float_at(rt, frame.at(0u));
}

} // namespace
} // namespace new_batch_main_1109
} // namespace gt::game

#define NEW1109(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1109::name, gt::ReturnKind::kind)
NEW1109(0x08AD96BCu, func_08AD96BC, Word);
NEW1109(0x08AE0814u, func_08AE0814, Word);
NEW1109(0x08AE1D50u, func_08AE1D50, Word);
NEW1109(0x08AEF93Cu, func_08AEF93C, Word);
NEW1109(0x08AEF9B0u, func_08AEF9B0, Word);
NEW1109(0x08AEFA40u, func_08AEFA40, Word);
NEW1109(0x08B19E30u, func_08B19E30, Word);
NEW1109(0x08B19E68u, func_08B19E68, Word);
NEW1109(0x08B19EA0u, func_08B19EA0, Word);
NEW1109(0x08B19EBCu, func_08B19EBC, Word);
NEW1109(0x08B19ED8u, func_08B19ED8, Word);
NEW1109(0x08B19EF4u, func_08B19EF4, Word);
NEW1109(0x08B3498Cu, func_08B3498C, Word);
NEW1109(0x08B5095Cu, func_08B5095C, Word);
NEW1109(0x08B50978u, func_08B50978, Word);
NEW1109(0x08A5CC88u, func_08A5CC88, Word);
NEW1109(0x08AB5F18u, func_08AB5F18, Word);
NEW1109(0x08B096F8u, func_08B096F8, Word);
NEW1109(0x08B09718u, func_08B09718, Word);
NEW1109(0x08B1E2B4u, func_08B1E2B4, Word);
NEW1109(0x08B797D0u, func_08B797D0, Word);
NEW1109(0x08ADA4B4u, func_08ADA4B4, Word);
NEW1109(0x08AFC460u, func_08AFC460, Word);
NEW1109(0x08B21CE0u, func_08B21CE0, Word);
NEW1109(0x08B28BBCu, func_08B28BBC, Word);
NEW1109(0x08A5BD0Cu, func_08A5BD0C, Word);
NEW1109(0x08A5CC60u, func_08A5CC60, Word);
NEW1109(0x08AA03C0u, func_08AA03C0, Float);
