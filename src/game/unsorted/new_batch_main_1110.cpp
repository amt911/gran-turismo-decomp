// Main-session batch 1110: thin wrappers (structured C from tools.disasm). Each one saves what the prologue saves, calls with the
// argument registers the callee reads and answers the v0 the asm leaves on every path; the two functions whose tail target is
// outside .text (0x08AC735C -> 0x08C7AA38, 0x08B2DE6C -> 0x08C7AA78) are left out.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1110 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// s0 is the object across the call; the epilogue restores it.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved() { ctx.gpr[16] = s0; }
};

// The output slot is pre-set with a1 and read back afterwards, written or not by the callee (the `sw $a1, 0x0($sp)` at the top).
void lookup_into_slot(Runtime &rt, AllegrexContext &ctx, u32 callee, bool forwards_two) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    if (forwards_two) {
        call(rt, ctx, callee, {frame.at(0u), ctx.gpr[4], ctx.gpr[6], ctx.gpr[7]});
    } else {
        call(rt, ctx, callee, {frame.at(0u), ctx.gpr[4]});
    }
    ctx.gpr[2] = word(rt, frame.at(0u));
}

// func(a0, a1): the object is returned in v0 after the call.
void call_and_return_self(Runtime &rt, AllegrexContext &ctx, u32 callee, u32 argument1) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, callee, {self, argument1});
    ctx.gpr[2] = self;
}

// func(a0, a1, a2, a3): a2 / a3 are the caller's, passed on untouched.
void call_and_return_self4(Runtime &rt, AllegrexContext &ctx, u32 callee) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, callee, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = self;
}

// func(a0, a1) with the word at +offset of the object cleared afterwards; v0 stays the callee's.
void release_and_clear(Runtime &rt, AllegrexContext &ctx, u32 callee, u32 offset) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, callee, {self, ctx.gpr[5]});
    put(rt, self + offset, 0u);
}

void func_08AA0668(Runtime &rt, AllegrexContext &ctx) { lookup_into_slot(rt, ctx, 0x08AA0528u, false); }
void func_08AA0A18(Runtime &rt, AllegrexContext &ctx) { lookup_into_slot(rt, ctx, 0x08AA0984u, true); }

// func_08AA5DE4(a0, a1) / func_08AA5E24(a0, a1, a2, a3): the bytes go into the object through its vtable; v0 = the object.
void func_08AA6084(Runtime &rt, AllegrexContext &ctx) { call_and_return_self(rt, ctx, 0x08AA5DE4u, ctx.gpr[5]); }

// Refcounted assignment: the callee stores into *a0 from the object *a1, then +4 is cleared; v0 is the callee's.
void func_08AAB5B4(Runtime &rt, AllegrexContext &ctx) { release_and_clear(rt, ctx, 0x08B88F44u, 4u); }
void func_08AAB738(Runtime &rt, AllegrexContext &ctx) { release_and_clear(rt, ctx, 0x08B88F44u, 4u); }
void func_08B11A70(Runtime &rt, AllegrexContext &ctx) { release_and_clear(rt, ctx, 0x08B0CE24u, 0x3Cu); }

// String assign from a char*: v0 = the object.
void func_08AAB654(Runtime &rt, AllegrexContext &ctx) { call_and_return_self4(rt, ctx, 0x08BEED1Cu); }
// The source pointer is a member: a1 = a1 + 0x18 / + 0x14.
void func_08A707F4(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08B88F44u, ctx.gpr[5] + 0x18u);
}
void func_08A89B4C(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08B88F44u, ctx.gpr[5] + 0x14u);
}
// a3 is the member block at a1 + 0x40.
void func_08A9D2AC(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A9D0D4u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[5] + 0x40u});
    ctx.gpr[2] = self;
}
// The callee's a0 is the word at +0x20 of the object (the load sits in the call's delay slot).
void func_08A9E300(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08BEED1Cu, {word(rt, self + 0x20u), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = self;
}
void func_08A9E3FC(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B89054u, {word(rt, self + 0x20u), ctx.gpr[5]});
    ctx.gpr[2] = self;
}

// Byte / half-word stores: a1 is masked before the call, v0 = the object.
void func_08AA5F7C(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08AA5D90u, ctx.gpr[5] & 0xFFu);
}
void func_08AA5FD4(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08AA5D90u, ctx.gpr[5] & 0xFFu);
}
void func_08AA6000(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08AA5D90u, ctx.gpr[5] & 0xFFu);
}
void func_08AA6058(Runtime &rt, AllegrexContext &ctx) {
    call_and_return_self(rt, ctx, 0x08AA5DB4u, ctx.gpr[5] & 0xFFFFu);
}

// 0x08ACC980 / 0x08ACC9A8: the first call takes a2 as well, the tail call gets the object.

// 0x08B4E758: the callee copies 0x40 bytes from a1 into a0, then the copy is normalised in place.
void func_08B4E758(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08B4E734u, {self, ctx.gpr[5]});
    }
    tail_call(ctx, 0x08B4DFF4u, {self});
}

// 0x08B1007C: a2 / a3 become the two-word block on the stack, a3 is t0 & 0xFF.
void func_08B1007C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[6]);
    put(rt, frame.at(4u), ctx.gpr[7]);
    call(rt, ctx, 0x08B10048u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[8] & 0xFFu});
}

// 0x08B1BA7C: a zeroed two-word block goes to the callee as a0; v0 is the callee's.
void func_08B1BA7C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    call(rt, ctx, 0x08B1BA54u, {frame.at(0u), ctx.gpr[4]});
}

// 0x08B577A8: a1's four words are packed by func_08B57F14, then written into the graphics library's state block (+0x5C / +0x244).
void func_08B577A8(Runtime &rt, AllegrexContext &ctx) {
    u32 packed;
    {
        GuestFrame frame(ctx, 0x10u);
        packed = call(rt, ctx, 0x08B57F14u, {ctx.gpr[5]});
    }
    tail_call(ctx, 0x08B57714u, {kGuState, packed});
}

// 0x08AC097C: a zeroed four-word block is the third argument; a0 is unused down the chain, a1 is the record.
void func_08AC097C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    for (u32 offset = 0u; offset < 0x10u; offset += 4u) put(rt, frame.at(offset), 0u);
    call(rt, ctx, 0x08AC0880u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
}

} // namespace

#define NEW1110(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1110::name, gt::ReturnKind::kind)
NEW1110(0x08A707F4u, func_08A707F4, Word);
NEW1110(0x08A89B4Cu, func_08A89B4C, Word);
NEW1110(0x08A9D2ACu, func_08A9D2AC, Word);
NEW1110(0x08A9E300u, func_08A9E300, Word);
NEW1110(0x08A9E3FCu, func_08A9E3FC, Word);
NEW1110(0x08AA0668u, func_08AA0668, Word);
NEW1110(0x08AA0A18u, func_08AA0A18, Word);
NEW1110(0x08AA5F7Cu, func_08AA5F7C, Word);
NEW1110(0x08AA5FD4u, func_08AA5FD4, Word);
NEW1110(0x08AA6000u, func_08AA6000, Word);
NEW1110(0x08AA6058u, func_08AA6058, Word);
NEW1110(0x08AA6084u, func_08AA6084, Word);
NEW1110(0x08AAB5B4u, func_08AAB5B4, Word);
NEW1110(0x08AAB654u, func_08AAB654, Word);
NEW1110(0x08AAB738u, func_08AAB738, Word);
NEW1110(0x08AC097Cu, func_08AC097C, Word);
NEW1110(0x08B1007Cu, func_08B1007C, Word);
NEW1110(0x08B11A70u, func_08B11A70, Word);
NEW1110(0x08B1BA7Cu, func_08B1BA7C, Word);
NEW1110(0x08B4E758u, func_08B4E758, Word);
NEW1110(0x08B577A8u, func_08B577A8, Word);

} // namespace new_batch_main_1110
} // namespace gt::game
