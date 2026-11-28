// New batch main 1112: 30 small functions drafted from tools.disasm (unverified: the gate runs later).
// Shapes for the synthetic generator: gt-ops/shapes/shapes_main1112.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1112 {
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

// 0x08AF4CBC: vtable slot +8 of *a0 with (sp, a0), then func_08AF4930(sp); v0 = the second call.
void func_08AF4CBC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 target = word(rt, word(rt, self) + 8u);
    call_indirect(rt, ctx, target, {frame.at(0u), self});
    call(rt, ctx, 0x08AF4930u, {frame.at(0u), self});
}

// 0x08AFB7E4: base constructor, then vtable 0x08CD7848 at +0; v0 = the vtable.
void func_08AFB7E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AFB4E8u, {self});
    put(rt, self, 0x08CD7848u);
    ctx.gpr[2] = 0x08CD7848u;
}

// 0x08B14BBC: base constructor, then vtable 0x08CD8ED0 at +0; v0 = the vtable.
void func_08B14BBC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B14CF8u, {self});
    put(rt, self, 0x08CD8ED0u);
    ctx.gpr[2] = 0x08CD8ED0u;
}

// 0x08B1E5E8: v0 = func_08B1E390(func_08B1E528(a0), a1 & 0xFF).
void func_08B1E5E8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 byte = ctx.gpr[5] & 0xFFu;
    const u32 inner = call(rt, ctx, 0x08B1E528u, {ctx.gpr[4]});
    ctx.gpr[2] = call(rt, ctx, 0x08B1E390u, {inner, byte});
}

// 0x08B208F0: stores a1 at +0, func_08B208C8(a1) at +4; v0 = the call.
void func_08B208F0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    put(rt, self, value);
    ctx.gpr[2] = call(rt, ctx, 0x08B208C8u, {value, value});
    put(rt, self + 4u, ctx.gpr[2]);
}

// 0x08B28CA4: base constructor, clears +0x28/+0x29, tail func_08B34B1C(this); v0 = the constructor's.
void func_08B28CA4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B34880u, {self});
    rt.memory().store8(self + 0x28u, 0u);
    rt.memory().store8(self + 0x29u, 0u);
    tail_call(ctx, 0x08B34B1Cu, {self});
}

// 0x08B2AFB0: base constructor, then vtable 0x08CDA690 at +0; v0 = the vtable.
void func_08B2AFB0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B2B3F8u, {self});
    put(rt, self, 0x08CDA690u);
    ctx.gpr[2] = 0x08CDA690u;
}

// 0x08B2BCA0: func_08B344E0(this), func_08B2BC5C(this), tail func_08B3457C(this); v0 = the second call.

// 0x08B2E2E4: s0 = *a0, v0 = func_08B2E2C0(a1), tail func_08C7AA50(s0, v0).
void func_08B2E2E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 inner = word(rt, ctx.gpr[4]);
    const u32 result = call(rt, ctx, 0x08B2E2C0u, {ctx.gpr[5]});
    tail_call(ctx, 0x08C7AA50u, {inner, result});
}

// 0x08A5BBE0: v0 = ((*(*(0x08CFFAAC) + 0x10) ^ func_08B2E750(a0)) == 0).
void func_08A5BBE0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 value = word(rt, word(rt, 0x08CFFAACu) + 0x10u);
    const u32 result = call(rt, ctx, 0x08B2E750u, {ctx.gpr[4]});
    ctx.gpr[2] = (value ^ result) == 0u ? 1u : 0u;
}

// 0x08A75A54: func_08B88F44(a0, *(*(a1 + 4) - 4) + 4); v0 = a0.
void func_08A75A54(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = word(rt, word(rt, ctx.gpr[5] + 4u) - 4u) + 4u;
    call(rt, ctx, 0x08B88F44u, {self, argument});
    ctx.gpr[2] = self;
}

// 0x08AA0950: f12 goes to sp+0, func_08AA08B8(sp, a0, a1, a2), f0 = the slot afterwards.
void func_08AA0950(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put_float(rt, frame.at(0u), ctx.fpr[12]);
    call(rt, ctx, 0x08AA08B8u, {frame.at(0u), ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]});
    ctx.fpr[0] = float_at(rt, frame.at(0u));
}

// 0x08AB2AE8: base constructor, vtable 0x08CD5010 at +0, zero at +0x24; v0 = the vtable.
void func_08AB2AE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A717ECu, {self});
    put(rt, self, 0x08CD5010u);
    put(rt, self + 0x24u, 0u);
    ctx.gpr[2] = 0x08CD5010u;
}

// 0x08AB5498: base constructor, vtable 0x08CD5240 at +0, zero at +0x1C; v0 = the vtable.
void func_08AB5498(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A70B1Cu, {self});
    put(rt, self, 0x08CD5240u);
    put(rt, self + 0x1Cu, 0u);
    ctx.gpr[2] = 0x08CD5240u;
}

// 0x08AB5E94: base constructor, vtable 0x08CD5128 at +0, zero at +0x24; v0 = the vtable.
void func_08AB5E94(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A717ECu, {self});
    put(rt, self, 0x08CD5128u);
    put(rt, self + 0x24u, 0u);
    ctx.gpr[2] = 0x08CD5128u;
}

// 0x08AC09A8: copies the pair at a2 into the stack block and calls func_08AC0880(a0, a1, sp, a3).
void func_08AC09A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 source = ctx.gpr[6];
    put(rt, frame.at(0x4u), word(rt, source + 4u));
    put(rt, frame.at(0x0u), word(rt, source));
    put(rt, frame.at(0x8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call(rt, ctx, 0x08AC0880u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[7]});
}

// 0x08AC0B84: a3 is a byte moved to t0, the stack block goes in a3; call func_08AC0A14.
void func_08AC0B84(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 byte = ctx.gpr[7] & 0xFFu;
    put(rt, frame.at(0x0u), 0u);
    put(rt, frame.at(0x4u), 0u);
    put(rt, frame.at(0x8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call(rt, ctx, 0x08AC0A14u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], frame.at(0u), byte});
}

// 0x08AC0DC0: a2 is a byte moved to a3, the stack block goes in a2; call func_08AC0AAC.
void func_08AC0DC0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 byte = ctx.gpr[6] & 0xFFu;
    put(rt, frame.at(0x0u), 0u);
    put(rt, frame.at(0x4u), 0u);
    put(rt, frame.at(0x8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call(rt, ctx, 0x08AC0AACu, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), byte});
}

// 0x08AC10FC: t0 is a byte moved to t1, the stack block goes in t0; call func_08AC0F08.
void func_08AC10FC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 byte = ctx.gpr[8] & 0xFFu;
    put(rt, frame.at(0x0u), 0u);
    put(rt, frame.at(0x4u), 0u);
    put(rt, frame.at(0x8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call(rt, ctx, 0x08AC0F08u,
         {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], frame.at(0u), byte});
}

// 0x08AC1130: a3 is a byte moved to t0, the stack block goes in a3; call func_08AC1060.

// 0x08AC84E4: base constructor, vtable 0x08CD67A0 at +0, zero at +0x18; v0 = the vtable.
void func_08AC84E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AB9BF4u, {self});
    put(rt, self, 0x08CD67A0u);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = 0x08CD67A0u;
}

// 0x08AC8518: same body as 0x08AC84E4.
void func_08AC8518(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AB9BF4u, {self});
    put(rt, self, 0x08CD67A0u);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = 0x08CD67A0u;
}

// 0x08AD07EC: func_08B0CC04(this + 0x248), clears +0x360/+0x364, tail func_08B1A5B4(this + 0x368).
void func_08AD07EC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B0CC04u, {self + 0x248u});
    put(rt, self + 0x360u, 0u);
    put(rt, self + 0x364u, 0u);
    tail_call(ctx, 0x08B1A5B4u, {self + 0x368u});
}

// 0x08AFB64C: base constructor, vtable 0x08CD78A0 at +0, zero at +0xD4; v0 = the vtable.
void func_08AFB64C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AFB4E8u, {self});
    put(rt, self, 0x08CD78A0u);
    put(rt, self + 0xD4u, 0u);
    ctx.gpr[2] = 0x08CD78A0u;
}

// 0x08AFB680: same body as 0x08AFB64C.
void func_08AFB680(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AFB4E8u, {self});
    put(rt, self, 0x08CD78A0u);
    put(rt, self + 0xD4u, 0u);
    ctx.gpr[2] = 0x08CD78A0u;
}

// 0x08B0CC38: base constructor, vtable 0x08CD7B80 at +0, tail func_08B0E0CC(this + 0x20).
void func_08B0CC38(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B0CBCCu, {self});
    put(rt, self, 0x08CD7B80u);
    ctx.gpr[2] = 0x08CD7B80u;
    tail_call(ctx, 0x08B0E0CCu, {self + 0x20u});
}

// 0x08B15F8C: base constructor, vtable 0x08CD9088 at +0, tail func_08B15C30(this + 0xF8).
void func_08B15F8C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B0E008u, {self});
    put(rt, self, 0x08CD9088u);
    ctx.gpr[2] = 0x08CD9088u;
    tail_call(ctx, 0x08B15C30u, {self + 0xF8u});
}

// 0x08B17C10: base constructor, vtable 0x08CD91C8 at +0, tail func_08C4C82C(this + 0xA0).
void func_08B17C10(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B11830u, {self});
    put(rt, self, 0x08CD91C8u);
    ctx.gpr[2] = 0x08CD91C8u;
    tail_call(ctx, 0x08C4C82Cu, {self + 0xA0u});
}

// 0x08B1A580: func_08B28D34(this), func_08B1F478(this + 0x2C), clears +0x5C and the byte at +0x60.
void func_08B1A580(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B28D34u, {self});
    call(rt, ctx, 0x08B1F478u, {self + 0x2Cu});
    rt.memory().store8(self + 0x60u, 0u);
    put(rt, self + 0x5Cu, 0u);
}

// 0x08B1F844: base constructor, vtable 0x08CD9488 at +0, tail func_08B28DCC(this + 0xC).
void func_08B1F844(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B1FD44u, {self});
    put(rt, self, 0x08CD9488u);
    ctx.gpr[2] = 0x08CD9488u;
    tail_call(ctx, 0x08B28DCCu, {self + 0xCu});
}

} // namespace

#define NEW1112(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1112::name, gt::ReturnKind::kind)
NEW1112(0x08AF4CBCu, func_08AF4CBC, Word);
NEW1112(0x08AFB7E4u, func_08AFB7E4, Word);
NEW1112(0x08B14BBCu, func_08B14BBC, Word);
NEW1112(0x08B1E5E8u, func_08B1E5E8, Word);
NEW1112(0x08B208F0u, func_08B208F0, Word);
NEW1112(0x08B28CA4u, func_08B28CA4, Word);
NEW1112(0x08B2AFB0u, func_08B2AFB0, Word);
NEW1112(0x08B2E2E4u, func_08B2E2E4, Word);
NEW1112(0x08A5BBE0u, func_08A5BBE0, Word);
NEW1112(0x08A75A54u, func_08A75A54, Word);
NEW1112(0x08AA0950u, func_08AA0950, Float);
NEW1112(0x08AB2AE8u, func_08AB2AE8, Word);
NEW1112(0x08AB5498u, func_08AB5498, Word);
NEW1112(0x08AB5E94u, func_08AB5E94, Word);
NEW1112(0x08AC09A8u, func_08AC09A8, Word);
NEW1112(0x08AC0B84u, func_08AC0B84, Word);
NEW1112(0x08AC0DC0u, func_08AC0DC0, Word);
NEW1112(0x08AC10FCu, func_08AC10FC, Word);
NEW1112(0x08AC84E4u, func_08AC84E4, Word);
NEW1112(0x08AC8518u, func_08AC8518, Word);
NEW1112(0x08AD07ECu, func_08AD07EC, Word);
NEW1112(0x08AFB64Cu, func_08AFB64C, Word);
NEW1112(0x08AFB680u, func_08AFB680, Word);
NEW1112(0x08B0CC38u, func_08B0CC38, Word);
NEW1112(0x08B15F8Cu, func_08B15F8C, Word);
NEW1112(0x08B17C10u, func_08B17C10, Word);
NEW1112(0x08B1A580u, func_08B1A580, Word);
NEW1112(0x08B1F844u, func_08B1F844, Word);

} // namespace new_batch_main_1112
} // namespace gt::game
