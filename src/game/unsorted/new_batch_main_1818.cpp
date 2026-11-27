// Main-session batch 1818 (draft, UNVERIFIED): functions written by hand from `tools.disasm` as structured C, started
// from batch 1792 (commit 71ecfd10) and repaired against the asm. Delay slots are read as "before the branch / call",
// `v0` ends as the asm leaves it on every exit path, and every callee-saved register the original keeps a value in
// stays untouched: the work lives in C++ locals, never in ctx.gpr[16..23], so nothing has to be saved or restored.
// No names: none has evidence beyond what its comment says.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1818 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A `jal`: afterwards v0 holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// 0x0885F22C (a0 = holder, f12 = a value): the word at +0 of a0 is the object; f12 is stored at its +0x4634 (in the
// `bc1t` delay slot, so always) and f20 is 0 when f12 is negative, 4.0 (0x4080) when it is above 1.0, and f12 * 4.0
// otherwise (`bc1fl` is a branch-if-false *likely*: its delay slot runs only when it branches, i.e. when
// !(1.0 < f12)). 0x0885F0B8(holder) then gets f20 + 1.0 at +0x18. v0 = that call's result; f20 is the caller's again.
void func_0885F22C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 holder = ctx.gpr[4];
    const float value = ctx.fpr[12];
    const u32 object = word(rt, holder);
    put_float(rt, object + 0x4634u, value);
    float scaled = 0.0f;
    if (!(value < 0.0f)) {
        if (1.0f < value) {
            scaled = 4.0f;
        } else {
            scaled = psp_mul(value, 4.0f);
        }
    }
    const u32 result = call(rt, ctx, 0x0885F0B8u, {holder});
    ctx.fpr[0] = scaled + 1.0f;
    put_float(rt, result + 0x18u, ctx.fpr[0]);
    ctx.gpr[2] = result;
}

} // namespace

#define NEW1818(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1818::name, gt::ReturnKind::kind)
NEW1818(0x0885F22Cu, func_0885F22C, Word);

} // namespace new_batch_main_1818
} // namespace gt::game
