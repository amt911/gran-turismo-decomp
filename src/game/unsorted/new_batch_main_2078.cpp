// New batch (main session 2078): functions that mismatched the original or were skipped twice before, drafted again from
// tools.disasm. Nineteen of them are the same wrapper shape: func_INIT(this, 0x08CF6ED8) (the global nil holder), then a
// tail call func_TAIL(this, a1) with the frame popped and s0/s1 restored, as the original does. The rest are small
// hand-written bodies: a float4 build, a list walk over a global (.bss), two stack-record builders, a virtual release, a
// constructor that adjusts through a negative vtable offset, and three initialisers. Draft-only: not gate-verified yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2078 {
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

// What the loader leaves in a register whose `lui` was relocated to the high half of `address` (the pair it patches into
// `lui` / `lw`: the high 16 bits, with the carry of a low half at or above 0x8000 folded in).
u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// Callee-saved registers the originals set (and restore from their frame): restore them on the way out, since the
// harness compares s0-s7, so the caller's values must come back.
struct Saved {
    AllegrexContext &ctx;
    u32 s0, s1, s2;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]), s2(c.gpr[18]) {}
    ~Saved() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
    }
};

// func_XXXXXXXX(this, a1): func_INIT(this, 0x08CF6ED8), frame popped, then func_TAIL(this, a1) as a tail call.
void init_then_tail(Runtime &rt, AllegrexContext &ctx, u32 init, u32 tail) {
    Saved saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self;
        ctx.gpr[16] = argument;
        (void)call(rt, ctx, init, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, tail, {self, argument});
}


// 0x08B03408(dst, a1, source): copies a float3 into the stack's first three slots, appends 1.0f and calls func_08B03170.
void func_08B03408(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[6];
    const u32 x = word(rt, source);
    const u32 y = word(rt, source + 4u);
    const u32 z = word(rt, source + 8u);
    {
        GuestFrame frame(ctx, 0x20u);
        put(rt, frame.at(0x0u), x);
        put(rt, frame.at(0x4u), y);
        put(rt, frame.at(0x8u), z);
        put(rt, frame.at(0xCu), 0x3F800000u);
        (void)call(rt, ctx, 0x08B03170u, {ctx.gpr[4], ctx.gpr[5], frame.at(0x0u)});
    }
}

// 0x08B468BC(this): func_08B46880(this); func_08C7AED0(0x08B46868, this) (signed) is stored at +0x4898 and answers 1 when
// it is not negative, 0 otherwise.

// 0x08ACC9D0(this): func_08ACC6B4(this); when the object at +4 exists, its vtable's first slot runs with
// (object, this[0]), then both words are cleared.

// 0x08AC1264(a0, a1, value, flags): copies the two words at `value` to the frame's +8 block, zeroes two 8-byte records
// beside it and calls func_08AC1060 with (a0, a1, record = sp, copy = sp+8) and t0 = flags (byte).

// 0x08AC12E4(a0, a1, a2, value, flags): the same record build, sourced from a3 and with t0 = sp+8 and t1 = flags.

// 0x08B20C00(): walks the list at the global 0x08D74F44 (the relocated lui leaves v0 = 0x08D70000 when it is empty);
// each node's +4 is the object whose virtual at +8 runs with the caller's a0, as the asm leaves it.

// 0x08C55C24(this, source, a2): stores source[0] at +0 and source[1] at this + source[0][-0xC] (the second base), clears
// +4, and calls func_08C54A7C with (this + same offset, a2, source[0]).

// 0x089E8848(this, a1, a2): v0 = func_089BD668(this, a1, a2); then func_089E8408(this, v0, a1, a2) as a tail call.

// 0x08A35848(this): vtable 0x08CCE588 at +0, func_08A35114(this), then func_08A5B9EC on the members at +0x1C, +0x18 and
// +0x14, then func_08A780B0(this) as a tail call.

// 0x08A67074(this): vtable 0x08CD2E80 at +0, the byte 0x26 at +4 widened back to a word, func_08A75D04(this + 8), and +0x14
// cleared.

// 0x08A5D888(this, other): func_08A781D4(this, other) and the two words at +0x18 / +0x1C copied over, unless this is
// other; v0 = this either way.
void func_08A5D888(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = other;
        if (self != other) {
            (void)call(rt, ctx, 0x08A781D4u, {self, other});
            put(rt, self + 0x18u, word(rt, other + 0x18u));
            put(rt, self + 0x1Cu, word(rt, other + 0x1Cu));
        }
    }
    ctx.gpr[2] = self;
}

} // namespace
} // namespace new_batch_main_2078
} // namespace gt::game

#define NEW2078(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2078::name, gt::ReturnKind::kind)
NEW2078(0x08B03408u, func_08B03408, Word);
NEW2078(0x08A5D888u, func_08A5D888, Word);
