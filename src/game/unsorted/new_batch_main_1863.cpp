// New batch 1863: functions drafted before that mismatched the original in the gate (see gt-ops/drafts/fix_info.txt).
// Structured C from tools.disasm; registration at the bottom (GT_DECOMP).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_1863 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// 0x08B2DE6C: 0x08B2E05C(a0) (a thunk to 0x08C7AA78); the callee's v0 and v1 go to the globals 0x08D68D68 and
// 0x08D68D6C, and a0 ends as the 0x00090000 base the original's `lui` left there. The old draft read ctx.gpr[3]
// after the call, but call_guest discards everything but v0, so the v1 store wrote the caller's stale register
// (the gate's 0x08D68D6C: original 00, decompiled 80); the GuestResult overload carries v1 back.
void func_08B2DE6C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const GuestResult result = call_guest(rt, ctx, 0x08B2E05Cu, {ctx.gpr[4]}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[4] = 0x00090000u;
    put(rt, 0x08D68D68u, result.v0);
    put(rt, 0x08D68D6Cu, result.v1);
}

} // namespace
} // namespace new_batch_1863
} // namespace gt::game

#define NEW1863(address, name, kind) GT_DECOMP(address, gt::game::new_batch_1863::name, gt::ReturnKind::kind)
NEW1863(0x08B2DE6Cu, func_08B2DE6C, Word);
