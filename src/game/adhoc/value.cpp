#include "value.hpp"

namespace gt::adhoc {
namespace {

constexpr std::uint32_t kNewIntegerObject = 0x08A85B1Cu;  // (holder *out, int)
constexpr std::uint32_t kNewFloatObject = 0x08A87EECu;    // (float in $f12, holder *out)
constexpr std::uint32_t kNewBoolObject = 0x08A97A6Cu;     // (holder *out, bool)
constexpr std::uint32_t kNewLongObject = 0x08A5DEE8u;     // (holder *out, -, u64 in a2:a3)
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;       // src/core/refcount.cpp
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

// result = scratch (the holder assignment skips self-assignment), then scratch is destroyed.
void hand_back(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
               std::uint32_t scratch) {
    if (result != scratch) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(scratch)});
    (void)call_guest(rt, ctx, kHolderDestroy, {scratch});
}

} // namespace

std::uint32_t virtual_function(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t slot) {
    return rt.memory().load32(rt.memory().load32(object) + slot * 4u);
}

std::int32_t to_int(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    return static_cast<std::int32_t>(call_guest(rt, ctx, virtual_function(rt, object, kToIntSlot), {object}));
}

float to_float(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    return call_guest(rt, ctx, virtual_function(rt, object, kToFloatSlot), {object}, {}).f0;
}

void return_integer(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                    std::uint32_t scratch, std::uint32_t value) {
    (void)call_guest(rt, ctx, kNewIntegerObject, {scratch, value});
    hand_back(rt, ctx, result, scratch);
}

void return_float(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                  std::uint32_t scratch, float value) {
    (void)call_guest(rt, ctx, kNewFloatObject, {scratch}, {value});
    hand_back(rt, ctx, result, scratch);
}

void return_bool(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                 std::uint32_t scratch, std::uint32_t value) {
    (void)call_guest(rt, ctx, kNewBoolObject, {scratch, value});
    hand_back(rt, ctx, result, scratch);
}

void return_long(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                 std::uint32_t scratch, std::uint32_t low, std::uint32_t high) {
    (void)call_guest(rt, ctx, kNewLongObject, {scratch, ctx.gpr[5], low, high});
    hand_back(rt, ctx, result, scratch);
}

} // namespace gt::adhoc
