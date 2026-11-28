// Main-session batch 1611: 30 thunk-shaped functions and 2 object initialisers. No old draft was named for these, and fix_info.txt has
// no gate detail for them, so they are drafted from the asm alone (tools.disasm).
// Thunk (28): the prologue saves s0/s1/ra; the first call gets a0 = this (unchanged) and a1 = 0x08CF6ED8, the tail call goes to the
// target with a0 = this and a1 = the original a1. v0 is whatever the tail target returns.
// Initialiser (2): 0x08A76ACC(this, a1), then word +0x14 = a1 and the vtable word at +0 (0x08CBEA58 or 0x08CB8578); v0 = vtable.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1611 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The thunk: the first call's result is dropped; the frame is popped before the tail call, as the asm does.
void call_then_tail(Runtime &rt, AllegrexContext &ctx, u32 helper, u32 tail) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, helper, {object, 0x08CF6ED8u});
    }
    tail_call(ctx, tail, {object, argument});
}

// The initialiser: the store of the argument at +0x14 comes before the vtable store at +0, as in the asm; v0 = the vtable.
void init_object(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 object = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A76ACCu, {object, value});
    put(rt, object + 0x14u, value);
    put(rt, object, vtable);
    ctx.gpr[2] = vtable;
}

// 0x0899AF8C: call 0x089948F4(this, 0x08CF6ED8); tail 0x0899AEA0(this, a1).
// 0x0899BAC4: call 0x08973D98; tail 0x0899B9D8.
// 0x089F0E20: call 0x089FDAB0; tail 0x089F0D34.
// 0x089F5748: call 0x089FF120; tail 0x089F5574.
void func_089F5748(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x089FF120u, 0x089F5574u); }
// 0x089F5D9C: call 0x089FF120; tail 0x089F5BC8.
// 0x089F7E9C: call 0x089D8A60; tail 0x089F7DB0.
// 0x089FDC4C: call 0x08A76AB4; tail 0x089FDAD0.
// 0x089FE938: call 0x08A0D868; tail 0x089FE788.
// 0x089FF088: call 0x08965854; tail 0x089FEFDC.
// 0x089FFC3C: call 0x08A0FD20; tail 0x089FF4A0.
// 0x08A08268: call 0x08969190; tail 0x08A0817C.
// 0x08A0BD2C: call 0x08965854; tail 0x08A0BB58.
// 0x08A1B5B0: call 0x08A76AB4; tail 0x08A1B4C4.
// 0x08A58B78: call 0x08A52AB0; tail 0x08A58A8C.
// 0x08A59318: call 0x08A52AB0; tail 0x08A5926C.
// 0x08A59828: call 0x08A52AB0; tail 0x08A5973C.
// 0x08A5A174: call 0x08A52AB0; tail 0x08A5A0C8.
// 0x08A5A724: call 0x08A52AB0; tail 0x08A5A638.
// 0x08A5F980: call 0x08A76AB4; tail 0x08A5F800.
// 0x08A70378: call 0x08A76AB4; tail 0x08A7028C.
// 0x08A7B168: call 0x08A5D190; tail 0x08A7AF84.
void func_08A7B168(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A5D190u, 0x08A7AF84u); }
// 0x08A7C3E4: call 0x08A76AB4; tail 0x08A7C2F8.
void func_08A7C3E4(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A7C2F8u); }
// 0x08A81134: call 0x08A76AB4; tail 0x08A81048.
// 0x08A83044: call 0x08A76AB4; tail 0x08A82EC0.
void func_08A83044(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A82EC0u); }
// 0x08A846F0: call 0x08A5D190; tail 0x08A84644.
void func_08A846F0(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A5D190u, 0x08A84644u); }
// 0x08A84730: call 0x08A5D190; tail 0x08A84644.
// 0x08A84CD0: object initialiser, vtable 0x08CBEA58.
void func_08A84CD0(Runtime &rt, AllegrexContext &ctx) { init_object(rt, ctx, 0x08CBEA58u); }
// 0x08A85088: call 0x08A76AB4; tail 0x08A84F9C.
void func_08A85088(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A84F9Cu); }
// 0x08A8548C: object initialiser, vtable 0x08CB8578.
void func_08A8548C(Runtime &rt, AllegrexContext &ctx) { init_object(rt, ctx, 0x08CB8578u); }
// 0x08A85A10: call 0x08A76AB4; tail 0x08A85890.
void func_08A85A10(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A85890u); }

} // namespace
} // namespace new_batch_main_1611
} // namespace gt::game

#define NEW1611(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1611::name, gt::ReturnKind::kind)
NEW1611(0x089F5748u, func_089F5748, Word);
NEW1611(0x08A7B168u, func_08A7B168, Word);
NEW1611(0x08A7C3E4u, func_08A7C3E4, Word);
NEW1611(0x08A83044u, func_08A83044, Word);
NEW1611(0x08A846F0u, func_08A846F0, Word);
NEW1611(0x08A84CD0u, func_08A84CD0, Word);
NEW1611(0x08A85088u, func_08A85088, Word);
NEW1611(0x08A8548Cu, func_08A8548C, Word);
NEW1611(0x08A85A10u, func_08A85A10, Word);
