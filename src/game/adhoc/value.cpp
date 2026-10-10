#include "value.hpp"

namespace gt::adhoc {
namespace {

constexpr std::uint32_t kNewIntegerObject = 0x08A85B1Cu;  // (holder *out, int)
constexpr std::uint32_t kNewFloatObject = 0x08A87EECu;    // (holder *out, float in $f12)
constexpr std::uint32_t kNewBoolObject = 0x08A97A6Cu;     // (holder *out, bool)
constexpr std::uint32_t kNewLongObject = 0x08A5DEE8u;     // (holder *out, -, u64 in a2:a3)
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;       // src/core/refcount.cpp
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kEmptyStringRep = 0x08D74FD0u;    // libstdc++'s shared empty std::string rep
constexpr std::uint32_t kExchangeAndAdd = 0x08B60804u;    // __gnu_cxx::__exchange_and_add(int *, int) -> old value
constexpr std::uint32_t kStringRepDestroy = 0x08B8542Cu;  // std::string::_Rep::_M_destroy(rep *, const allocator &)
constexpr std::uint32_t kStringRepBytes = 0xCu;
constexpr std::uint32_t kStringFromCString = 0x08B85EB8u;  // std::string(out, const char *, const allocator &)
constexpr std::uint32_t kNewStringObject = 0x08A6D2D4u;    // (holder *out, const std::string *): an Adhoc string           // length, capacity, reference count, then the characters

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

void to_string(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t out) {
    (void)call_guest(rt, ctx, virtual_function(rt, object, kToStringSlot), {out, object});
}

void string_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t chars) {
    const std::uint32_t rep = chars - kStringRepBytes;
    if (rep == kEmptyStringRep) return;
    if (static_cast<std::int32_t>(call_guest(rt, ctx, kExchangeAndAdd, {chars - 4u, 0xFFFFFFFFu})) > 0) return;
    GuestFrame frame(ctx, 0x10u);  // the allocator argument: an empty object on the stack
    (void)call_guest(rt, ctx, kStringRepDestroy, {rep, frame.at(0u)});
}

void return_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t string) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0x0u);
    (void)call_guest(rt, ctx, kNewStringObject, {holder, string});
    hand_back(rt, ctx, result, holder);
}

void return_c_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t text) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t string = frame.at(0x0u);
    (void)call_guest(rt, ctx, kStringFromCString, {string, text, frame.at(0x8u)});
    return_string(rt, ctx, result, string);
    string_release(rt, ctx, rt.memory().load32(string));
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
