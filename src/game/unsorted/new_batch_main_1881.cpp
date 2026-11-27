// Main-session batch 1117 (draft only, unverified). Structured C from tools.disasm; v0 as the asm on every exit path.
// Skipped (unsure): 0x08AC0DF4 (fifth argument in t0), 0x08ADDBA8 (a0 left by the first callee feeds the second),
// 0x08B03408 (float vector argument).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1881 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The functions below save s0 and/or s1 in their prologue; this restores both on every exit.
struct Saved2 {
    AllegrexContext &ctx;
    u32 s0;
    u32 s1;
    explicit Saved2(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]) {}
    ~Saved2() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
    }
};

// The name constant the asm loads into a1 (lui/addiu pair, 0x08CF6ED8).
constexpr u32 kName = 0x08CF6ED8u;

// Wrappers: the first call gets (a0, name) and its result is dropped; then a tail call with the original a0 and a1.
void name_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first_call, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved2 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, first_call, {self, kName});
    }
    tail_call(ctx, tail, {self, argument});
}

// Stores the vtable at *this after the first call, which gets this; the tail is not used here.
void call_then_vtable(Runtime &rt, AllegrexContext &ctx, u32 callee, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved2 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, callee, {self});
        put(rt, self + 0x14u, word(rt, argument + 0x14u));
        put(rt, self, vtable);
    }
    ctx.gpr[2] = vtable;
}

}  // namespace





// 0x08AAFBB8 (a0 = object, a1 = a halfword): calls 0x08A76ACC(this), stores the halfword at +0x14, vtable 0x08CD3838 at +0; v0 = vtable.

// 0x08AB133C (a0 = object, a1 = a byte): calls 0x08A76ACC(this), stores the byte at +0x14, vtable 0x08CD3728 at +0; v0 = vtable.

// 0x08AC854C (a0 = object, a1 = a word): calls 0x08AB9BF4(this), stores a1 at +0x18, vtable 0x08CD67A0 at +0; v0 = vtable.

// 0x08AC08FC (a0 = caller object, a1 = a two-word struct): builds {0, 0, a1[0], a1[1], 0, 0} on the stack and calls 0x08AC0880(a0, sp, sp+8).

// 0x08AC093C (a0 = caller object, a1 = a two-word struct): builds {0, 0, 0, 0, a1[0], a1[1]} on the stack and calls 0x08AC0880(a0, sp, sp+8).

// 0x08AC0D80 (a0 = caller object, a1 = a byte): zeroes a 24-byte block on the stack and calls 0x08AC0AAC(a0, sp, sp+8, byte).

// 0x08B09618 (a0 = object, a1 = argument): calls 0x08B09264(this), then tail-calls 0x08B0957C(this, a1).

// 0x08B0E89C (a0 = object): calls 0x08B0E008(this), vtable 0x08CD7C68 at +0, calls 0x08C4C82C(this+0xF8), zeroes +0x108 and +0x104.

// 0x08B1F928 (a0 = object): calls 0x08B344E0(this+0xC) and 0x08B1FDA4(this), then tail-calls 0x08B3457C(this+0xC).

// 0x08B2BD78 (a0 = object, a1 = argument): calls 0x08B28DCC(this), zeroes +0x2C and +0x30, then tail-calls 0x08B2B918(this, a1).

// 0x08B372D4 (a0 = object, a1 = a word): calls 0x08B36290(this), stores a1 at +0x20, vtable 0x08CDA898 at +0; v0 = vtable.

// 0x08C72C44 (a0 = object, a1 = pointer to a block): calls 0x08B5C450(this, a1+0x1C); v0 = this.
void func_08C72C44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved2 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B5C450u, {self, argument + 0x1Cu});
    }
    ctx.gpr[2] = self;
}

// 0x08A5F438 (a0 = object, a1 = source object): calls 0x08A782D8(this), copies the word at source+0x14 to +0x14, vtable 0x08CBDFD0 at +0; v0 = vtable.

// 0x08A5F47C: the same code as 0x08A5F438 at another address.

// 0x08A6D24C (a0 = object, a1 = argument): calls 0x08A76ACC(this), vtable 0x08CB7328 at +0, then tail-calls 0x08B88F44(this+0x14, a1).

}  // namespace new_batch_main_1881
}  // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1881::name, gt::ReturnKind::kind)
NEWM(0x08C72C44u, func_08C72C44, Word);
