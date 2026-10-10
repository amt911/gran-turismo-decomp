// MRandom: the Adhoc module scripts draw random numbers from (name: GT's own script registration,
// config/symbol_addrs.txt).
//
// MRandom::GetValue(result, argc, argv) with argc >= 2: argv[0], argv[1] are integers lo, hi.
//   - two arguments: lo + random_float(hi - lo), truncated;
//   - three or more: random_float_between(lo, hi, &state), truncated, where state is argv[2]'s
//     integer value (a seed the script supplies; the callee may advance it, the copy is discarded).
// The result is a new Adhoc integer object. Fewer than two arguments: the result is left alone.
// Integer -> float and float -> integer conversions are the FPU's (cvt.s.w, trunc.w.s).
// Harness: 10/10 cases to the menu, all two-argument draws; the seeded path has no case yet.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::mrandom {
namespace {

constexpr std::uint32_t kRandomFloat = 0x08B1ECFCu;          // (float span) -> [0, span)
constexpr std::uint32_t kRandomFloatBetween = 0x08BB9130u;   // (float lo, float hi, int *state)
constexpr std::uint32_t kToIntegerObject = 0x08A85940u;      // (holder *out, argument holder *)
constexpr std::uint32_t kIntegerValue = 0x08A855E8u;         // (integer object *) -> value
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

std::int32_t integer_argument(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder,
                              std::uint32_t argument) {
    (void)call_guest(rt, ctx, kToIntegerObject, {holder, argument});
    const auto value = static_cast<std::int32_t>(call_guest(rt, ctx, kIntegerValue, {rt.memory().load32(holder)}));
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
    return value;
}

} // namespace

void GetValue(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 1) return;
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t holder = frame.at(0x10u);
    const std::uint32_t state = frame.at(0x20u);
    const std::int32_t lo = integer_argument(rt, ctx, holder, argv);
    const std::int32_t hi = integer_argument(rt, ctx, holder, argv + 4u);
    float drawn;
    if (argc == 2) {
        const float span = static_cast<float>(hi - lo);
        drawn = call_guest(rt, ctx, kRandomFloat, {}, {span}).f0 + static_cast<float>(lo);
    } else {
        rt.memory().store32(state, static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(argv + 8u))));
        drawn = call_guest(rt, ctx, kRandomFloatBetween, {state}, {static_cast<float>(lo), static_cast<float>(hi)}).f0;
    }
    return_integer(rt, ctx, result, holder, ctx.fpu_float_to_word(drawn, 1u));  // trunc.w.s
}

} // namespace gt::adhoc::mrandom

GT_DECOMP(0x08A38170u, gt::adhoc::mrandom::GetValue, gt::ReturnKind::Void);
