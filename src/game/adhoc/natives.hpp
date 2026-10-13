// Shared shapes of GT's Adhoc natives: fetch `self` as an engine class into a guest-stack holder, take its
// engine object, drop the holder, and hand an integer or a boolean back. All of it is GT's own code,
// called through the guest (value.hpp has the value factories these build on).
#pragma once

#include "value.hpp"

namespace gt::adhoc {

inline constexpr std::uint32_t kHolderDestroyFunction = 0x08A5B9ECu;
inline constexpr std::uint32_t kHolderResetFunction = 0x08A5BAD8u;

inline void destroy(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroyFunction, {holder});
}

template <typename Use>
inline void with_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, Use use) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, self_as, {holder});
    use(rt.memory().load32(holder));  // NULL is not checked (the originals only report it)
    destroy(rt, ctx, holder);
}

// self as `self_as`, then its engine object (`core_of`); the holder is dropped before returning.
inline std::uint32_t self_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, std::uint32_t core_of,
                        std::uint32_t self_holder_arg = 0u, bool explicit_self = false) {
    std::uint32_t core = 0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0u);
    if (explicit_self) (void)call_guest(rt, ctx, self_as, {holder, self_holder_arg});
    else (void)call_guest(rt, ctx, self_as, {holder});
    core = call_guest(rt, ctx, core_of, {rt.memory().load32(holder)});
    destroy(rt, ctx, holder);
    return core;
}

// Argument `index` as an integer (its toInt()).
inline std::uint32_t int_argument(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t argv, std::uint32_t index) {
    return static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(argv + 4u * index)));
}

inline void return_int(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t value) {
    GuestFrame frame(ctx, 0x10u);
    return_integer(rt, ctx, result, frame.at(0u), value);
}

inline void return_boolean(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t value) {
    GuestFrame frame(ctx, 0x10u);
    return_bool(rt, ctx, result, frame.at(0u), value);
}

// *result = the holder `wrap(out, object)` fills.
inline void return_wrapped(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t wrap,
                    std::uint32_t object) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t out = frame.at(0u);
    (void)call_guest(rt, ctx, wrap, {out, object});
    if (result != out) (void)call_guest(rt, ctx, kHolderResetFunction, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

} // namespace gt::adhoc
