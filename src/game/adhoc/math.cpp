// Math: the Adhoc math module (initializer 0x29F0EC: "Math", "PI"; abs, sin, ..., clamp, lerp).

#include "decomp_registry.hpp"
#include "value.hpp"

#include <bit>
#include <cmath>

namespace gt::adhoc::math {

// Math.clamp(x, lo, hi) -> float. Three arguments are assumed (argc isn't checked). With ordered
// operands this is min(max(x, lo), hi); the two `<` tests are kept as written because NaNs fall
// through them differently than std::clamp would.
void clamp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t argv = ctx.gpr[6];
    float x = to_float(rt, ctx, rt.memory().load32(argv));
    const float lo = to_float(rt, ctx, rt.memory().load32(argv + 4u));
    float clamped = to_float(rt, ctx, rt.memory().load32(argv + 8u));  // hi
    if (x < lo) x = lo;
    if (x < clamped) clamped = x;
    GuestFrame frame(ctx, 0x30u);
    return_float(rt, ctx, result, frame.at(0u), clamped);
}

// Math.sqrt(x) / Math.abs(x) -> float, from argv[0]'s float value (sqrt.s / abs.s; abs clears the
// sign bit, NaNs included).
void sqrt(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const float x = to_float(rt, ctx, rt.memory().load32(ctx.gpr[6]));
    GuestFrame frame(ctx, 0x20u);
    return_float(rt, ctx, result, frame.at(0u), std::sqrt(x));
}

void abs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const float x = to_float(rt, ctx, rt.memory().load32(ctx.gpr[6]));
    GuestFrame frame(ctx, 0x20u);
    return_float(rt, ctx, result, frame.at(0u), std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) & 0x7FFFFFFFu));
}

} // namespace gt::adhoc::math

GT_DECOMP(0x08AA39FCu, gt::adhoc::math::clamp, gt::ReturnKind::Void);
GT_DECOMP(0x08AA3BE8u, gt::adhoc::math::sqrt, gt::ReturnKind::Void);
GT_DECOMP(0x08AA34F0u, gt::adhoc::math::abs, gt::ReturnKind::Void);
