// New batch main-890: the "select then assign" wrappers of the 0x08CF6ED8 family (a0 = object, a1 = handle record: the
// first call receives a1 = 0x08CF6ED8 and the tail call the caller's a1), two vtable constructors, one three-argument
// constructor and one float step. Structured C from tools.disasm; every body is 16 instructions, frame 0x10.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_890 {
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

// The two forms of the prologue: s0 only, or s0 and s1.
struct Saved0 {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved0(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved0() { ctx.gpr[16] = s0; }
};
struct Saved1 {
    AllegrexContext &ctx;
    u32 s0, s1;
    explicit Saved1(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]) {}
    ~Saved1() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
    }
};

// lw s0,0(a1)-style wrappers: s0 = a1 and s1 = a0 (in the jal's delay slot), a1 = 0x08CF6ED8 for the first call (a0, a2,
// a3 unchanged), then a0 = s1 / a1 = s0 and the work continues in a tail call to `tail` with the frame popped.
void select_then_tail(Runtime &rt, AllegrexContext &ctx, u32 select, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 handle = ctx.gpr[5];
    {
        Saved1 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = handle;
        ctx.gpr[17] = self;
        call(rt, ctx, select, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, tail, {self, handle});
}

// The constructors of the 0x089767F0 shape: the base's constructor gets (a0, 1), then the vtable at +0x38 and the one at
// +0 receive their tables; v0 holds the +0x38 table, which is the last value the original writes to it.
void vtable_ctor(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable_at_0x38, u32 vtable_at_0) {
    const u32 self = ctx.gpr[4];
    {
        Saved0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, base, {self, 1u});
        ctx.gpr[2] = vtable_at_0x38;
        ctx.gpr[3] = vtable_at_0;
        put(rt, self + 0x38u, vtable_at_0x38);
        put(rt, self + 0x0u, vtable_at_0);
    }
}

// 0x08854804 (a0 = object, a1 = handle record): the base-class selection with the 0x08CF6ED8 key, then the assignment.
void func_08854804(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A76AB4u, 0x08854718u); }

// 0x088588AC: same shape as 0x08854804, another assign tail.

// 0x0890F838 (a0, a1 = object, a2 = a3, a3): first call gets a2 = a3 (its copy sits before the jal), then a1 = 0,
// then f12 = 50.0f and the tail call with a0 = a1.

// 0x08918F70 (a0 = object): a1 = word(+0x1FC), f12 = float(+0x40C) into the call, then +0x40C += 1.0f; f0 is the new
// value, v0 stays the callee's.
void func_08918F70(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        Saved0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.fpr[12] = float_at(rt, self + 0x40Cu);
        call(rt, ctx, 0x08918C84u, {self, word(rt, self + 0x1FCu)});
        const float step = float_at(rt, self + 0x40Cu) + 1.0f;
        put_float(rt, self + 0x40Cu, step);
        ctx.fpr[0] = step;
    }
}

// 0x089203D8 (a0 = object, a1 passed on, a2 = value): base constructor, then +8 = a2 and the vtable at +0; v0 = that
// vtable (the original's last write to v0), v1 stays the callee's.
void func_089203D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    {
        Saved1 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = value;
        ctx.gpr[17] = self;
        call(rt, ctx, 0x08925FE4u, {self, second});
        ctx.gpr[2] = 0x08CC5FA0u;
        put(rt, self + 0x8u, value);
        put(rt, self + 0x0u, 0x08CC5FA0u);
    }
}

// 0x089657FC: the 0x08CF6ED8 selection, then the assign tail.
void func_089657FC(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089BA130u, 0x08965710u); }

// 0x08969284: the same shape, its own selection helper.
void func_08969284(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089BAC44u, 0x08969198u); }

// 0x08973FC4: the same shape, another assign tail.
void func_08973FC4(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A76AB4u, 0x08973E4Cu); }

// 0x08974918: the same shape, its own selection helper.

// 0x089767F0 (a0 = object): base constructor with (a0, 1), then +0x38 and +0 get their tables; v0 = the +0x38 one.
void func_089767F0(Runtime &rt, AllegrexContext &ctx) { vtable_ctor(rt, ctx, 0x08993DECu, 0x08CCCF68u, 0x08CCCD10u); }

// 0x08976A44: the 0x08CF6ED8 selection, then the assign tail.
void func_08976A44(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08993BF4u, 0x08976938u); }

// 0x08976D14 (a0 = object): the 0x089767F0 shape with its own base and tables.
void func_08976D14(Runtime &rt, AllegrexContext &ctx) { vtable_ctor(rt, ctx, 0x089FE458u, 0x08CCABF0u, 0x08CCA998u); }

// 0x08978040: the 0x08CF6ED8 selection, then the assign tail.
void func_08978040(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A76AB4u, 0x08977F54u); }

// 0x0897DBDC: the same shape, another assign tail.

// 0x0897EA2C: its own selection helper, its own assign tail.
void func_0897EA2C(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08969190u, 0x0897E798u); }

// 0x089898DC: the same selection helper, another assign tail.

// 0x0898E1B0: its own selection helper and assign tail.
void func_0898E1B0(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x0899537Cu, 0x0898E0C4u); }

// 0x089919A0: the 0x08CF6ED8 selection, then the assign tail.
void func_089919A0(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A76AB4u, 0x089918F4u); }

// 0x089919E0: byte-identical body to 0x089919A0.
void func_089919E0(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A76AB4u, 0x089918F4u); }

// 0x08991DAC: its own selection helper and assign tail.
void func_08991DAC(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089FFC94u, 0x08991CA0u); }

// 0x08992860: its own selection helper and assign tail.

// 0x08995594: the shared selection helper 0x089948F4, its own assign tail.

// 0x089965B0: the same selection helper, another assign tail.
void func_089965B0(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089948F4u, 0x089964C4u); }

// 0x08999B14: its own selection helper and assign tail.
void func_08999B14(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089FC5D0u, 0x08999A28u); }

// 0x0899AF4C: the shared selection helper, the shared assign tail.
void func_0899AF4C(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089948F4u, 0x0899AEA0u); }

// 0x0899AF8C: byte-identical body to 0x0899AF4C.

// 0x0899BAC4: its own selection helper and assign tail.

// 0x0899DF28: its own selection helper and assign tail.
void func_0899DF28(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x089A274Cu, 0x0899DCB4u); }

// 0x089A2818: its own selection helper, the assign tail of 0x089A2858.
void func_089A2818(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A0D868u, 0x089A276Cu); }

// 0x089A2858: byte-identical body to 0x089A2818.
void func_089A2858(Runtime &rt, AllegrexContext &ctx) { select_then_tail(rt, ctx, 0x08A0D868u, 0x089A276Cu); }

#undef NEW890
#define NEW890(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_890::name, gt::ReturnKind::kind)
NEW890(0x08854804u, func_08854804, Word);
NEW890(0x08918F70u, func_08918F70, Float);
NEW890(0x089203D8u, func_089203D8, Word);
NEW890(0x089657FCu, func_089657FC, Word);
NEW890(0x08969284u, func_08969284, Word);
NEW890(0x08973FC4u, func_08973FC4, Word);
NEW890(0x089767F0u, func_089767F0, Word);
NEW890(0x08976A44u, func_08976A44, Word);
NEW890(0x08976D14u, func_08976D14, Word);
NEW890(0x08978040u, func_08978040, Word);
NEW890(0x0897EA2Cu, func_0897EA2C, Word);
NEW890(0x0898E1B0u, func_0898E1B0, Word);
NEW890(0x089919A0u, func_089919A0, Word);
NEW890(0x089919E0u, func_089919E0, Word);
NEW890(0x08991DACu, func_08991DAC, Word);
NEW890(0x089965B0u, func_089965B0, Word);
NEW890(0x08999B14u, func_08999B14, Word);
NEW890(0x0899AF4Cu, func_0899AF4C, Word);
NEW890(0x0899DF28u, func_0899DF28, Word);
NEW890(0x089A2818u, func_089A2818, Word);
NEW890(0x089A2858u, func_089A2858, Word);

} // namespace
} // namespace new_batch_main_890
} // namespace gt::game
