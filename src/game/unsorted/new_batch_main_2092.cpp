// New batch main-2092 (main-session batch, draft only, unverified). Structured C from tools.disasm.
// Each function: every argument register the callee reads is passed, delay-slot stores run before the call, v0 is the last call's result.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2092 {
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

// 0x08A2B6F4 (a0 = out object): a lookup 0x08B32138 that misses returns 0 at once; otherwise a few destructor-shaped calls, and v0 is the
// last call's result.

// 0x08A9C998 (a1 = out pointer slot, a2 = source): builds a local record, looks up a name and, when the answer is a record, calls its
// vtable slot 0x11C; v0 is the last call's result.

// 0x08C7A274 (a0 = destination record, a1 = source record, a2/a3 = flags): copies the links of the source into the destination, then
// calls the two rebuild helpers and returns the last call's result.

// 0x08A8EA78 (a0 = object being built): stores the vtable word, rebuilds the member lists and returns the last call's result (0x08A780B0).

} // namespace
} // namespace new_batch_main_2092
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2092::name, gt::ReturnKind::kind)
