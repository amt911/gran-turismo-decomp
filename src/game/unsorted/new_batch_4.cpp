// New batch 4: constructor- and destructor-shaped functions the extra seeds made emittable, written as structured C from `tools.disasm` and
// verified by synthetic cases. Each one stores a vtable word at *this, calls one or two base/member functions with `this + offset`, and either
// returns or ends with a tail call (the frame is popped first). The callee-saved register s0 holds `this` for the calls and is put back.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_4 {
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

// s0 is callee-saved: the original keeps `this` in it and restores the caller's value.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};

// vtable store, one call with this + first_offset, then a tail call with this + tail_offset (v0 = the call's result).
void destroy_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 call_target, u32 first_offset, u32 tail_target, u32 tail_offset) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, vtable);
        call(rt, ctx, call_target, {self + first_offset});
    }
    tail_call(ctx, tail_target, {self + tail_offset});
}

// vtable store, a call with this + offset, a second vtable store: v0 = the second vtable (a constructor of a derived class).
void construct_in_two_steps(Runtime &rt, AllegrexContext &ctx, u32 first_vtable, u32 call_target, u32 offset, u32 second_vtable) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, first_vtable);
    call(rt, ctx, call_target, {self + offset});
    put(rt, self, second_vtable);
    ctx.gpr[2] = second_vtable;
}

// vtable store, two calls with this + offsets, then a tail call with this.
void destroy_two_members_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 first_target, u32 first_offset, u32 second_target,
                                   u32 second_offset, u32 tail_target) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, vtable);
        call(rt, ctx, first_target, {self + first_offset});
        call(rt, ctx, second_target, {self + second_offset});
    }
    tail_call(ctx, tail_target, {self});
}

void func_08A22354(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CBE7F8u, 0x08AB86A8u, 0x14u, 0x08A780B0u, 0x0u); }
void func_08A371A0(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD1C30u, 0x08B48804u, 0x14u, 0x08A780B0u, 0x0u); }
void func_08A39DAC(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CCC730u, 0x08C1B4A0u, 0x14u, 0x08A780B0u, 0x0u); }
void func_08A3D118(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD1A10u, 0x08B28BE0u, 0x14u, 0x08A780B0u, 0x0u); }
void func_08A6471C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD2BB0u, 0x08A5B9ECu, 0xCu, 0x08A64554u, 0x0u); }
void func_08A649C8(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD2A20u, 0x08A5B9ECu, 0xCu, 0x08A64554u, 0x0u); }
void func_08A745F0(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD3A18u, 0x08A5B9ECu, 0x14u, 0x08A72654u, 0x0u); }
void func_08A74DB4(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD3A58u, 0x08A5B9ECu, 0x14u, 0x08A8036Cu, 0x0u); }
void func_08A75264(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD3AA0u, 0x08A5B9ECu, 0x14u, 0x08A86D24u, 0x0u); }
void func_08A7B6B8(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD40C8u, 0x08A5B9ECu, 0x14u, 0x08A5D638u, 0x0u); }
void func_08A91ED0(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD43D8u, 0x08A5B9ECu, 0x14u, 0x08A5D638u, 0x0u); }
void func_08ABA4CC(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD6018u, 0x08B28BE0u, 0x14u, 0x08ABB100u, 0x0u); }
void func_08B159BC(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD9088u, 0x08B15944u, 0xF8u, 0x08B0DDDCu, 0x0u); }
void func_08B22A3C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD9588u, 0x08B28DCCu, 0x4u, 0x08C4FB94u, 0x2Cu); }
void func_08B2C318(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CDA710u, 0x08B2C2C8u, 0x58u, 0x08B2C2E0u, 0x0u); }
void func_08B32F2C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CDA840u, 0x08B34DA0u, 0x4u, 0x08B219C0u, 0x0u); }
void func_08A64ED0(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2908u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A65034(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2930u, 0x08A69D68u, 0x8u, 0x08CD3638u); }
void func_08A65198(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2958u, 0x08A69D68u, 0x8u, 0x08CD3638u); }
void func_08A65F50(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2BD8u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A669DC(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2DB8u, 0x08A69D68u, 0x8u, 0x08CD3638u); }
void func_08A66B40(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2DE0u, 0x08A69D68u, 0x8u, 0x08CD3638u); }
void func_08A66CA4(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2E08u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A66E48(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2E30u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A66FAC(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2E58u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A67150(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2E80u, 0x08A75A88u, 0x8u, 0x08CD3638u); }
void func_08A674C4(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2ED0u, 0x08A5B9ECu, 0x8u, 0x08CD3638u); }
void func_08A67628(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD2EF8u, 0x08A5B9ECu, 0x8u, 0x08CD3638u); }
void func_08A67CC4(Runtime &rt, AllegrexContext &ctx) { construct_in_two_steps(rt, ctx, 0x08CD3010u, 0x08A69D68u, 0x8u, 0x08CD3638u); }
void func_08A57FD4(Runtime &rt, AllegrexContext &ctx) { destroy_two_members_then_tail(rt, ctx, 0x08CB7730u, 0x08A5B9ECu, 0x18u, 0x08A5B9ECu, 0x14u, 0x08A780B0u); }
void func_08A7B288(Runtime &rt, AllegrexContext &ctx) { destroy_two_members_then_tail(rt, ctx, 0x08CD2758u, 0x08A5B9ECu, 0x1Cu, 0x08A5B9ECu, 0x18u, 0x08A5D354u); }
void func_08A80CB8(Runtime &rt, AllegrexContext &ctx) { destroy_two_members_then_tail(rt, ctx, 0x08CCFA70u, 0x08A5B9ECu, 0x18u, 0x08A5B9ECu, 0x14u, 0x08A780B0u); }
void func_08A84A58(Runtime &rt, AllegrexContext &ctx) { destroy_two_members_then_tail(rt, ctx, 0x08CD2648u, 0x08A5B9ECu, 0x1Cu, 0x08A5B9ECu, 0x18u, 0x08A5D354u); }

} // namespace
} // namespace new_batch_4
} // namespace gt::game

#define NEW4(address, name, kind) GT_DECOMP(address, gt::game::new_batch_4::name, gt::ReturnKind::kind)
NEW4(0x08A22354u, func_08A22354, Word);
NEW4(0x08A371A0u, func_08A371A0, Word);
NEW4(0x08A39DACu, func_08A39DAC, Word);
NEW4(0x08A3D118u, func_08A3D118, Word);
NEW4(0x08A6471Cu, func_08A6471C, Word);
NEW4(0x08A649C8u, func_08A649C8, Word);
NEW4(0x08A745F0u, func_08A745F0, Word);
NEW4(0x08A74DB4u, func_08A74DB4, Word);
NEW4(0x08A75264u, func_08A75264, Word);
NEW4(0x08A7B6B8u, func_08A7B6B8, Word);
NEW4(0x08A91ED0u, func_08A91ED0, Word);
NEW4(0x08ABA4CCu, func_08ABA4CC, Word);
NEW4(0x08B159BCu, func_08B159BC, Word);
NEW4(0x08B22A3Cu, func_08B22A3C, Word);
NEW4(0x08B2C318u, func_08B2C318, Word);
NEW4(0x08B32F2Cu, func_08B32F2C, Word);
NEW4(0x08A64ED0u, func_08A64ED0, Word);
NEW4(0x08A65034u, func_08A65034, Word);
NEW4(0x08A65198u, func_08A65198, Word);
NEW4(0x08A65F50u, func_08A65F50, Word);
NEW4(0x08A669DCu, func_08A669DC, Word);
NEW4(0x08A66B40u, func_08A66B40, Word);
NEW4(0x08A66CA4u, func_08A66CA4, Word);
NEW4(0x08A66E48u, func_08A66E48, Word);
NEW4(0x08A66FACu, func_08A66FAC, Word);
NEW4(0x08A67150u, func_08A67150, Word);
NEW4(0x08A674C4u, func_08A674C4, Word);
NEW4(0x08A67628u, func_08A67628, Word);
NEW4(0x08A67CC4u, func_08A67CC4, Word);
NEW4(0x08A57FD4u, func_08A57FD4, Word);
NEW4(0x08A7B288u, func_08A7B288, Word);
NEW4(0x08A80CB8u, func_08A80CB8, Word);
NEW4(0x08A84A58u, func_08A84A58, Word);
