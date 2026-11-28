// Main-session batch 1577: 30 functions of one shape, drafted from tools.disasm (no old draft existed for these addresses).
// Shape (all 30): record = *a1; if record == 0 return 0. found = probe(a0, a1); if func_08A76F20(record, found) == 0 return 0;
// otherwise func_08A5BAD8(a0, *a1) is called (result dropped) and the function returns 1. The asm ends with `j epilogue` whose
// delay slot sets v0 = 1, so it is a plain return here. The probe differs per address (first jal target).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1577 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The shared body. `probe` is the first callee; it gets the incoming a0 and a1 unchanged.
void probe_then_act(Runtime &rt, AllegrexContext &ctx, u32 probe) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 record = word(rt, arg);
    u32 result = 0u;
    if (record != 0u) {
        const u32 found = call(rt, ctx, probe, {self, arg});
        if (call(rt, ctx, 0x08A76F20u, {record, found}) != 0u) {
            call(rt, ctx, 0x08A5BAD8u, {self, word(rt, arg)});
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}

void func_08A3DAA0(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A3D6D8u); }
void func_08A3F028(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A3EB18u); }
void func_08A3F140(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A3EBD8u); }
void func_08A3F2B4(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A3ED6Cu); }
void func_08A40D78(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A40B20u); }
void func_08A40F10(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A40AE8u); }
void func_08A44734(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A4441Cu); }
void func_08A52900(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A52628u); }
void func_08A5A5AC(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A5A490u); }
void func_08A61AA0(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A615DCu); }
void func_08A68E3C(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A68CE0u); }
void func_08A6ADD0(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A6ACE4u); }
void func_08A6C490(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A6BF40u); }
void func_08A703B8(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A701C4u); }
void func_08A81174(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A80EDCu); }
void func_08A85A50(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A85454u); }
void func_08A87D58(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A87690u); }
void func_08A88FC4(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A88C40u); }
void func_08A8C318(Runtime &rt, AllegrexContext &ctx) { probe_then_act(rt, ctx, 0x08A8BF90u); }

} // namespace
} // namespace new_batch_main_1577
} // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1577::name, gt::ReturnKind::kind)
NEW3(0x08A3DAA0u, func_08A3DAA0, Word);
NEW3(0x08A3F028u, func_08A3F028, Word);
NEW3(0x08A3F140u, func_08A3F140, Word);
NEW3(0x08A3F2B4u, func_08A3F2B4, Word);
NEW3(0x08A40D78u, func_08A40D78, Word);
NEW3(0x08A40F10u, func_08A40F10, Word);
NEW3(0x08A44734u, func_08A44734, Word);
NEW3(0x08A52900u, func_08A52900, Word);
NEW3(0x08A5A5ACu, func_08A5A5AC, Word);
NEW3(0x08A61AA0u, func_08A61AA0, Word);
NEW3(0x08A68E3Cu, func_08A68E3C, Word);
NEW3(0x08A6ADD0u, func_08A6ADD0, Word);
NEW3(0x08A6C490u, func_08A6C490, Word);
NEW3(0x08A703B8u, func_08A703B8, Word);
NEW3(0x08A81174u, func_08A81174, Word);
NEW3(0x08A85A50u, func_08A85A50, Word);
NEW3(0x08A87D58u, func_08A87D58, Word);
NEW3(0x08A88FC4u, func_08A88FC4, Word);
NEW3(0x08A8C318u, func_08A8C318, Word);
