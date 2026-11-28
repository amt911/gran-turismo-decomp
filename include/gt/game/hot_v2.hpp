// Helpers of the structured rewrites of batch >= 91 (the functions generator v2 made comparable): the shared-holder release the
// original inlines (ll/sc use count at +4, weak count at +8, virtual +8 / +0xC) and a call with float arguments.
#pragma once

#include "gt/game/hot_common.hpp"

#include <initializer_list>

namespace gt::game::hot_v2 {

// ll / sc decrement of the word at `address` (nothing else runs, so the sc succeeds and v0 is 1 afterwards): true when it was 1.
inline bool decrement_reaches_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = hot::word(rt, address);
    hot::put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// Drops one use of a holder: the last use calls the virtual +8, the last weak count the virtual +0xC. With `tail` the last call is left to the
// caller (a `jr t9`): the function returns its target (0 when there is none).
inline std::uint32_t release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, bool tail) {
    if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return 0u;
    hot::call_indirect(rt, ctx, hot::word(rt, hot::word(rt, holder) + 8u), {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return 0u;
    const std::uint32_t target = hot::word(rt, hot::word(rt, holder) + 0xCu);
    if (tail) {
        ctx.gpr[4] = holder;
        return target;
    }
    ctx.gpr[4] = holder;
    hot::call_indirect(rt, ctx, target, {holder});
    return 0u;
}

} // namespace gt::game::hot_v2
