// New batch main-1116: 29 small object wrappers, structured C from tools.disasm (draft, unverified).
// Shapes: call-then-tail wrappers (a0 = object, a1 = value), store-after-call initialisers (vtable at +0, a field at +0x14),
// and two-call getters that return this + offset. func_08A876C8 (float argument) is not drafted.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1880 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// The name constant the wrappers pass as a1 to their first call (the asm's lui/addiu pair, relocated).
constexpr u32 kName = 0x08CF6ED8u;

// s0 and s1 are saved on entry and restored on every exit, as the asm does.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    u32 s1;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]) {}
    ~Saved() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
    }
};

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Shape A: pre(a0, kName); then tail call tail(a0, a1). The first call's result is dropped.
void call_named_then_tail(Runtime &rt, AllegrexContext &ctx, u32 pre, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = self;
        call(rt, ctx, pre, {self, kName});
    }
    tail_call(ctx, tail, {self, argument});
}

enum class Field { Word, Byte };

// Shape B: pre(a0, a1); then the field at +0x14 gets a1 (word, or its low byte); vtable at +0; v0 = vtable.
void call_then_store_vtable(Runtime &rt, AllegrexContext &ctx, u32 pre, u32 vtable, Field field) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = self;
        call(rt, ctx, pre, {self, argument});
    }
    if (field == Field::Word) {
        put(rt, self + 0x14u, argument);
    } else {
        rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(argument));
    }
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// Shape C: two calls on the object at this + 0x4C, then v0 = this + offset.
void two_calls_then_offset(Runtime &rt, AllegrexContext &ctx, u32 first, u32 second, u32 offset) {
    const u32 self = ctx.gpr[4];
    const u32 inner = self + 0x4Cu;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = inner;
        ctx.gpr[17] = self;
        call(rt, ctx, first, {inner});
        call(rt, ctx, second, {inner});
    }
    ctx.gpr[2] = self + offset;
}

// Shape A wrappers: pre = 0x08A5D190 or 0x08A76AB4.

// Shape B initialisers: pre = 0x08A76ACC.

// Only a0 is passed to 0x08A5D5A0; the vtable goes at +0 and v0 = vtable.

// Shape C getters: this + 0x4C is passed to both calls, v0 = this + 0x34 or this + 0x3C.

} // namespace
} // namespace new_batch_main_1880
} // namespace gt::game

#define NEW1116(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1880::name, gt::ReturnKind::kind)
