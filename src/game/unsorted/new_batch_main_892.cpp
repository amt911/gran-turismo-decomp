// Main session batch 892: derived constructors of one form. Every one of them (but 0x089ED504) is the same 16-instruction
// body: run the base constructor with (this, 0x08CF6ED8), then tail-call the real initialiser with (this, the original a1),
// so v0 is the tail call's. They were unreachable with useful inputs before, which is why they come with shapes: a0 must be
// a writable object (the base constructor stores into it) and the object is rebuilt, never read.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_892 {
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

// s0 / s1 saved and restored as the prologue does (both are the only s-registers these bodies touch).
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

// The shared shape: s0 = a1, s1 = a0, call base(this, 0x08CF6ED8), then a0 = this, a1 = the saved argument, frame popped
// (the original's delay slot) and a tail call. v0 = the tail call's.
void construct_then_forward(Runtime &rt, AllegrexContext &ctx, u32 base, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = self;
        call(rt, ctx, base, {self, 0x08CF6ED8u});
        ctx.gpr[4] = self;
        ctx.gpr[5] = argument;
    }
    tail_call(ctx, tail, {self, argument});
}

// 0x089ED504 (a0 = object): base(this, 0) first, then the two vtable words (+0x38 and +0) are stored; v0 = 0x08CC89B0.
// Its prologue saves s0 only (s1 is never touched), unlike the forwarding ones.
void func_089ED504(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 saved_s0 = ctx.gpr[16];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x089FE458u, {self, 0u});
    }
    ctx.gpr[16] = saved_s0;
    put(rt, self + 0x38u, 0x08CC89B0u);
    put(rt, self, 0x08CC8758u);
    ctx.gpr[3] = 0x08CC8758u;
    ctx.gpr[2] = 0x08CC89B0u;
}

void func_089ED758(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x089FE8D8u, 0x089ED64Cu); }
void func_089EE180(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x089E119Cu, 0x089EE028u); }
void func_089F6BD4(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A0FD20u, 0x089F6AE8u); }
void func_089FC630(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A0D868u, 0x089FC524u); }
void func_089FE238(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x089FF120u, 0x089FE18Cu); }
void func_089FF0C8(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08965854u, 0x089FEFDCu); }
void func_08A01378(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08969190u, 0x08A0128Cu); }
void func_08A037EC(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A013D0u, 0x08A036E0u); }
void func_08A03E98(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A76AB4u, 0x08A03DACu); }
void func_08A05144(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x089D8A60u, 0x08A05058u); }
void func_08A0C2F8(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A0CB9Cu, 0x08A0C1ECu); }
void func_08A0C754(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A0CB9Cu, 0x08A0C648u); }
void func_08A0CB44(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08965854u, 0x08A0CA58u); }
void func_08A0D810(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A013D0u, 0x08A0D724u); }
void func_08A122A0(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A76AB4u, 0x08A120A4u); }
void func_08A14738(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A2A218u, 0x08A14564u); }
void func_08A1D54C(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A76AB4u, 0x08A1D460u); }
void func_08A20D08(Runtime &rt, AllegrexContext &ctx) { construct_then_forward(rt, ctx, 0x08A76AB4u, 0x08A20C1Cu); }

} // namespace
} // namespace new_batch_main_892
} // namespace gt::game

#define NEW892(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_892::name, gt::ReturnKind::kind)
NEW892(0x089ED504u, func_089ED504, Word);
NEW892(0x089ED758u, func_089ED758, Word);
NEW892(0x089EE180u, func_089EE180, Word);
NEW892(0x089F6BD4u, func_089F6BD4, Word);
NEW892(0x089FC630u, func_089FC630, Word);
NEW892(0x089FE238u, func_089FE238, Word);
NEW892(0x089FF0C8u, func_089FF0C8, Word);
NEW892(0x08A01378u, func_08A01378, Word);
NEW892(0x08A037ECu, func_08A037EC, Word);
NEW892(0x08A03E98u, func_08A03E98, Word);
NEW892(0x08A05144u, func_08A05144, Word);
NEW892(0x08A0C2F8u, func_08A0C2F8, Word);
NEW892(0x08A0C754u, func_08A0C754, Word);
NEW892(0x08A0CB44u, func_08A0CB44, Word);
NEW892(0x08A0D810u, func_08A0D810, Word);
NEW892(0x08A122A0u, func_08A122A0, Word);
NEW892(0x08A14738u, func_08A14738, Word);
NEW892(0x08A1D54Cu, func_08A1D54C, Word);
NEW892(0x08A20D08u, func_08A20D08, Word);
