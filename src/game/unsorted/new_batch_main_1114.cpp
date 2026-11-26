// New batch main-1114 (draft, unverified). Structured C from tools.disasm: constructors that call a base constructor and
// then store a vtable, pointer-out wrappers (store a half/byte of the callee's result), stack-copy callers and tail calls.
// Synthetic input shapes: gt-ops/shapes/shapes_main1114.txt. Before registering: the four audits and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1114 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Constructor shape: base(this, kind, a1) first, then *this = vtable and v0 = vtable.
void base_then_vtable(Runtime &rt, AllegrexContext &ctx, u32 base, u32 kind, u32 vtable) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    call(rt, ctx, base, {self, kind, arg});
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// 0x08AA646C / 0x08AA64A4: v0 = a0; the half-word result of callee(a0, a1) is stored at a1.
void call_store_half(Runtime &rt, AllegrexContext &ctx, u32 callee) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 result = call(rt, ctx, callee, {self, out});
    rt.memory().store16(out, static_cast<std::uint16_t>(result));
    ctx.gpr[2] = self;
}

// 0x08AA6500 / 0x08AA6538 / 0x08AA6570: v0 = a0; the byte result of callee(a0, a1) is stored at a1.
void call_store_byte(Runtime &rt, AllegrexContext &ctx, u32 callee) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 result = call(rt, ctx, callee, {self, out});
    rt.memory().store8(out, static_cast<std::uint8_t>(result));
    ctx.gpr[2] = self;
}

// The call-then-tail-call shape: callee(a0, a1) first, then tail_call(a0, a1) after the frame is popped.
void call_then_tail(Runtime &rt, AllegrexContext &ctx, u32 callee, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, callee, {self, arg});
    }
    tail_call(ctx, tail, {self, arg});
}

} // namespace
} // namespace new_batch_main_1114
} // namespace gt::game

#define NEW1114(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1114::name, gt::ReturnKind::kind)

namespace gt::game::new_batch_main_1114 {
namespace {

// 0x08A64914 / 0x08A64BC0: base constructor 0x08A644FC with kind 0x11 / 0x02.
void func_08A64914(Runtime &rt, AllegrexContext &ctx) { base_then_vtable(rt, ctx, 0x08A644FCu, 0x11u, 0x08CD2B38u); }
void func_08A64BC0(Runtime &rt, AllegrexContext &ctx) { base_then_vtable(rt, ctx, 0x08A644FCu, 0x02u, 0x08CD28E0u); }

void func_08AA64A4(Runtime &rt, AllegrexContext &ctx) { call_store_half(rt, ctx, 0x08AA643Cu); }
void func_08AA6500(Runtime &rt, AllegrexContext &ctx) { call_store_byte(rt, ctx, 0x08AA64DCu); }
void func_08AA6570(Runtime &rt, AllegrexContext &ctx) { call_store_byte(rt, ctx, 0x08AA64DCu); }

// 0x08AC09DC: two stack out-words (sp+0.., sp+8..) are zeroed and passed as a1 / a2; v0 is the callee's.
void func_08AC09DC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    for (u32 offset = 0; offset <= 0x14u; offset += 4u) put(rt, sp + offset, 0u);
    call(rt, ctx, 0x08AC0880u, {ctx.gpr[4], sp, sp + 8u});
}

// 0x08AC0BB8 / 0x08AC12AC: t0 is a byte; the 8-byte value at a3 is copied to the frame, a3 = frame, a4 = t0 (byte).
void func_08AC0BB8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[8] & 0xFFu;
    const u32 src = ctx.gpr[7];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp, lo);
    put(rt, sp + 4u, hi);
    put(rt, sp + 8u, 0u);
    put(rt, sp + 0xCu, 0u);
    call(rt, ctx, 0x08AC0A14u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], sp, flags});
}

void func_08AC12AC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[8] & 0xFFu;
    const u32 src = ctx.gpr[7];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp, lo);
    put(rt, sp + 4u, hi);
    put(rt, sp + 8u, 0u);
    put(rt, sp + 0xCu, 0u);
    call(rt, ctx, 0x08AC1060u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], sp, flags});
}

// 0x08AC0D00: a3 is a byte; the 8-byte value at a2 is copied to the frame, a2 = frame.
void func_08AC0D00(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[7] & 0xFFu;
    const u32 src = ctx.gpr[6];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp, lo);
    put(rt, sp + 4u, hi);
    put(rt, sp + 8u, 0u);
    put(rt, sp + 0xCu, 0u);
    call(rt, ctx, 0x08AC0AACu, {ctx.gpr[4], ctx.gpr[5], sp, flags});
}

// 0x08AC10C4: a3 is a byte, passed as a5 (t1); a3 = frame + 8, a4 = frame.
void func_08AC10C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[7] & 0xFFu;
    const u32 sp = ctx.gpr[29];
    for (u32 offset = 0; offset < 0x10u; offset += 4u) put(rt, sp + offset, 0u);
    call(rt, ctx, 0x08AC1028u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], sp + 8u, sp, flags});
}

// 0x08AC1164: t1 is a byte (a5); the 8-byte value at t0 (a4) is copied to the frame, a4 = frame.

// 0x08AC132C: a2 is a byte, passed as a4 (t0); a3 = frame, a2 = frame + 8.
void func_08AC132C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[6] & 0xFFu;
    const u32 sp = ctx.gpr[29];
    for (u32 offset = 0; offset < 0x10u; offset += 4u) put(rt, sp + offset, 0u);
    call(rt, ctx, 0x08AC11E4u, {ctx.gpr[4], ctx.gpr[5], sp + 8u, sp, flags});
}

// 0x08AC91D0: callee(a0) first; then the object's links and flags are cleared. v0 stays the callee's result.
void func_08AC91D0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AD55ECu, {self});
    put(rt, self + 0x2Cu, self);
    put(rt, self + 0x30u, 0u);
    rt.memory().store8(self + 0x34u, 0u);
    rt.memory().store8(self + 0x35u, 0u);
    put(rt, self + 0x28u, 0u);
}

// 0x08AD43F8: base 0x08AD023C(this), vtable at this, the singleton slot at 0x08D404DC holds this; v0 = vtable.
void func_08AD43F8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AD023Cu, {self});
    put(rt, self, 0x08CD6E20u);
    put(rt, 0x08D404DCu, self);
    ctx.gpr[2] = 0x08CD6E20u;
}

// 0x08AFBCD8: two calls on this + 0x10 (the second gets a0 as the first left it), then the flag byte at +0x50 = 1; v0 = 1.
void func_08AFBCD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B567F4u, {self + 0x10u});
    call(rt, ctx, 0x08B56510u, {ctx.gpr[4]});
    rt.memory().store8(self + 0x50u, 1u);
    ctx.gpr[2] = 1u;
}

void func_08B0A094(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08B09264u, 0x08B09298u); }
void func_08B0A104(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08B09264u, 0x08B0957Cu); }

// 0x08B0FB90 / 0x08B0FDEC: the 8-byte value at a2 is copied to the frame, a2 = frame, a3 is a byte.
// 0x08B0FB90 puts the pair at +8 (the high word at +0xC); 0x08B0FDEC puts it at +0 (the high word at +4).
void func_08B0FB90(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[7] & 0xFFu;
    const u32 src = ctx.gpr[6];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 0xCu, hi);
    put(rt, sp + 8u, lo);
    put(rt, sp, 0u);
    put(rt, sp + 4u, 0u);
    call(rt, ctx, 0x08B0F8F0u, {ctx.gpr[4], ctx.gpr[5], sp, flags});
}

void func_08B0FDEC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[7] & 0xFFu;
    const u32 src = ctx.gpr[6];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp, lo);
    put(rt, sp + 4u, hi);
    put(rt, sp + 8u, 0u);
    put(rt, sp + 0xCu, 0u);
    call(rt, ctx, 0x08B0F8F0u, {ctx.gpr[4], ctx.gpr[5], sp, flags});
}

// 0x08B0FBC8: t0 is a byte (a4); the 8-byte value at a3 is copied to +8 / +0xC, a3 = frame.

// 0x08B0FE24: t0 is a byte (a4); the 8-byte value at a3 is copied to +0 / +4, a3 = frame.
void func_08B0FE24(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 flags = ctx.gpr[8] & 0xFFu;
    const u32 src = ctx.gpr[7];
    const u32 lo = word(rt, src);
    const u32 hi = word(rt, src + 4u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp, lo);
    put(rt, sp + 4u, hi);
    put(rt, sp + 8u, 0u);
    put(rt, sp + 0xCu, 0u);
    call(rt, ctx, 0x08B0F840u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], sp, flags});
}

// 0x08B1C2EC: base 0x08B1C2B4(this); the two words at +4 and +8 become -1; v0 = -1.
void func_08B1C2EC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B1C2B4u, {self});
    put(rt, self + 8u, 0xFFFFFFFFu);
    put(rt, self + 4u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08B22C3C: base 0x08B219B0(this); vtable at this, zero at +4 and +8; v0 = vtable.
void func_08B22C3C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B219B0u, {self});
    put(rt, self, 0x08CD95C0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 4u, 0u);
    ctx.gpr[2] = 0x08CD95C0u;
}

// 0x08B26A9C: base 0x08B26A80(this); +0x24 = this + 0x2A; vtable at this; v0 = vtable.
void func_08B26A9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B26A80u, {self});
    put(rt, self + 0x24u, self + 0x2Au);
    put(rt, self, 0x08CD9648u);
    ctx.gpr[2] = 0x08CD9648u;
}

// 0x08A71A2C: base 0x08A76ACC(this); vtable at this, +0x14 = 2, +0x18 = 0; v0 = vtable.
void func_08A71A2C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCD618u);
    put(rt, self + 0x14u, 2u);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = 0x08CCD618u;
}

// 0x08ABCEF0: base 0x08ABD2E4(this); vtable at this, byte +0x84 = 0, the global at 0x08CFFAAC = this.
// v0 is the lui base of that global store: the address minus the store offset.

// 0x08ADDB6C: result of 0x08AEB82C(0x08D40644 as left by 0x08AD8C5C(0x08D40644, 0)) after the call to 0x08AEB808.

// 0x08AE2580: the same as 0x08ADDB6C with the object at 0x08D40688.

} // namespace
} // namespace gt::game::new_batch_main_1114

NEW1114(0x08A64914u, func_08A64914, Word);
NEW1114(0x08A64BC0u, func_08A64BC0, Word);
NEW1114(0x08AA64A4u, func_08AA64A4, Word);
NEW1114(0x08AA6500u, func_08AA6500, Word);
NEW1114(0x08AA6570u, func_08AA6570, Word);
NEW1114(0x08AC09DCu, func_08AC09DC, Word);
NEW1114(0x08AC0BB8u, func_08AC0BB8, Word);
NEW1114(0x08AC0D00u, func_08AC0D00, Word);
NEW1114(0x08AC10C4u, func_08AC10C4, Word);
NEW1114(0x08AC12ACu, func_08AC12AC, Word);
NEW1114(0x08AC132Cu, func_08AC132C, Word);
NEW1114(0x08AC91D0u, func_08AC91D0, Word);
NEW1114(0x08AD43F8u, func_08AD43F8, Word);
NEW1114(0x08AFBCD8u, func_08AFBCD8, Word);
NEW1114(0x08B0A094u, func_08B0A094, Word);
NEW1114(0x08B0A104u, func_08B0A104, Word);
NEW1114(0x08B0FB90u, func_08B0FB90, Word);
NEW1114(0x08B0FDECu, func_08B0FDEC, Word);
NEW1114(0x08B0FE24u, func_08B0FE24, Word);
NEW1114(0x08B1C2ECu, func_08B1C2EC, Word);
NEW1114(0x08B22C3Cu, func_08B22C3C, Word);
NEW1114(0x08B26A9Cu, func_08B26A9C, Word);
NEW1114(0x08A71A2Cu, func_08A71A2C, Word);
