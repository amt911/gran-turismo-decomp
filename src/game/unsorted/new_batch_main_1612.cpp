// New batch main-1612 (draft, unverified): 31 small functions of the 0x08A8xxxx-0x08AAFxxx and 0x08A9Bxxx groups.
// Two shapes: (a) a wrapper that calls `first(this, 0x08CF6ED8)` and then tail-calls `tail(this, a1)`; (b) the
// constructors/setters that call one function and then store a vtable word (and a byte or float) at this.
// The B-family (0x08A9BBF0..0x08A9BD70) calls two functions on this + 0x4C and returns this + offset.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1612 {
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

// The second argument the wrappers pass to their first call (lui/addiu pair at the function start).
constexpr u32 kTableArgument = 0x08CF6ED8u;

// Saves s0 and s1 on entry and restores them on scope exit, as the asm does before the jump or the return.
struct SavedPair {
    AllegrexContext &ctx;
    u32 s0;
    u32 s1;
    explicit SavedPair(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]) {}
    ~SavedPair() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
    }
};

// `first(this, K)`, the result discarded; then the frame is popped and `tail(this, a1)` is jumped to. v0 is the tail's answer.
void probe_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedPair saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = self;
        call(rt, ctx, first, {self, kTableArgument});
    }
    tail_call(ctx, tail, {self, argument});
}

// a0 = this: s0 = this + 0x4C; two calls with s0; v0 = this + offset.
void member_calls_then_offset(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    {
        SavedPair saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 member = self + 0x4Cu;
        ctx.gpr[16] = member;
        ctx.gpr[17] = self;
        call(rt, ctx, 0x08B344E0u, {member});
        call(rt, ctx, 0x08B3457Cu, {member});
    }
    ctx.gpr[2] = self + offset;
}

void func_08A87D18(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A87C28u); }
void func_08A89AE0(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A899F4u); }
void func_08A8A5A0(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A5D190u, 0x08A8A3B8u); }
void func_08A8C2D8(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A8C1ECu); }
void func_08A90534(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A90488u); }
void func_08A90574(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A90488u); }
void func_08A95F3C(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A95E4Cu); }
void func_08A986B4(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A985C8u); }
void func_08A9B9A8(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A9B8FCu); }
void func_08A9B9E8(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08A9B8FCu); }
void func_08AABC8C(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A52AB0u, 0x08AABBE0u); }
void func_08AAC154(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A52AB0u, 0x08AAC0A8u); }
void func_08AAC67C(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A52AB0u, 0x08AAC570u); }
void func_08AAF634(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08AAF588u); }
void func_08AAF674(Runtime &rt, AllegrexContext &ctx) { probe_then_tail(rt, ctx, 0x08A76AB4u, 0x08AAF588u); }

// 0x08A95A6C: X(this, a1) (a1 raw at the call; s0 = sign-extended a1 in the delay slot); sb s0 at +0x14; vtable at +0;
// v0 is left as the vtable word, which the asm loaded into v0 for the store.
void func_08A95A6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedPair saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value)));
    ctx.gpr[17] = self;
    call(rt, ctx, 0x08A76ACCu, {self, value});
    rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(ctx.gpr[16]));
    put(rt, self, 0x08CD21A8u);
    ctx.gpr[2] = 0x08CD21A8u;
}

// 0x08A9737C: X(this, a1); sb (a1 & 0xFF) at +0x14; vtable at +0; v0 = the vtable word.
void func_08A9737C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedPair saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = value & 0xFFu;
    ctx.gpr[17] = self;
    call(rt, ctx, 0x08A76ACCu, {self, value});
    rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(ctx.gpr[16]));
    put(rt, self, 0x08CBCC00u);
    ctx.gpr[2] = 0x08CBCC00u;
}

// 0x08A876C8: constructor. f20 = f12 (the float argument); X(this) with f12 passed through; f20 stored at +0x14 then the
// vtable at +0; f20 is restored on exit and v0 is the vtable word.
void func_08A876C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    SavedPair saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[20] = value;
    ctx.gpr[16] = self;
    const auto result = call_guest(rt, ctx, 0x08A76ACCu, {self}, {value});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    put_float(rt, self + 0x14u, value);
    put(rt, self, 0x08CBBF20u);
    ctx.gpr[2] = 0x08CBBF20u;
    ctx.fpr[20] = saved_f20;
}

// 0x08AA2D10: a stack record at sp: X(sp); Y(this, sp); Z(sp); v0 = this.
void func_08AA2D10(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedPair saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 record = frame.at(0u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A6D2D4u, {record});
    call(rt, ctx, 0x08A76ABCu, {self, record});
    call(rt, ctx, 0x08A5B9ECu, {record});
    ctx.gpr[2] = self;
}

void func_08A9BBF0(Runtime &rt, AllegrexContext &ctx) { member_calls_then_offset(rt, ctx, 0x34u); }
void func_08A9BC70(Runtime &rt, AllegrexContext &ctx) { member_calls_then_offset(rt, ctx, 0x3Cu); }
void func_08A9BCF0(Runtime &rt, AllegrexContext &ctx) { member_calls_then_offset(rt, ctx, 0x40u); }
void func_08A9BD70(Runtime &rt, AllegrexContext &ctx) { member_calls_then_offset(rt, ctx, 0x44u); }

} // namespace
} // namespace new_batch_main_1612
} // namespace gt::game

#define NEW1612(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1612::name, gt::ReturnKind::kind)
NEW1612(0x08A876C8u, func_08A876C8, Word);
NEW1612(0x08A87D18u, func_08A87D18, Word);
NEW1612(0x08A89AE0u, func_08A89AE0, Word);
NEW1612(0x08A8A5A0u, func_08A8A5A0, Word);
NEW1612(0x08A8C2D8u, func_08A8C2D8, Word);
NEW1612(0x08A90534u, func_08A90534, Word);
NEW1612(0x08A90574u, func_08A90574, Word);
NEW1612(0x08A95A6Cu, func_08A95A6C, Word);
NEW1612(0x08A95F3Cu, func_08A95F3C, Word);
NEW1612(0x08A9737Cu, func_08A9737C, Word);
NEW1612(0x08A986B4u, func_08A986B4, Word);
NEW1612(0x08A9B9A8u, func_08A9B9A8, Word);
NEW1612(0x08A9B9E8u, func_08A9B9E8, Word);
NEW1612(0x08A9BBF0u, func_08A9BBF0, Word);
NEW1612(0x08A9BC70u, func_08A9BC70, Word);
NEW1612(0x08A9BCF0u, func_08A9BCF0, Word);
NEW1612(0x08A9BD70u, func_08A9BD70, Word);
NEW1612(0x08AA2D10u, func_08AA2D10, Word);
NEW1612(0x08AABC8Cu, func_08AABC8C, Word);
NEW1612(0x08AAC154u, func_08AAC154, Word);
NEW1612(0x08AAC67Cu, func_08AAC67C, Word);
NEW1612(0x08AAF634u, func_08AAF634, Word);
NEW1612(0x08AAF674u, func_08AAF674, Word);
