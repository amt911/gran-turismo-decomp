// Adhoc's boxed value types: integer, float, boolean and 64-bit integer objects, and the factories
// every native uses to return one (src/game/adhoc/value.cpp calls them). Each object is an Adhoc
// object (vtable, strong and weak counts — src/core/refcount.cpp) with the value after it.
//
// Name evidence: the decompiled natives that return integers, floats, booleans and 64-bit codes
// through these factories (MUnit getters, Math.sqrt, MSpecDB.isDirt, MSpecDB.getCarCode...), and the
// stored width (word, float register, byte, doubleword).

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <bit>

namespace gt::adhoc::values {
namespace {

constexpr std::uint32_t kAllocate = 0x08A5BC20u;          // (size, align) -> object memory
constexpr std::uint32_t kObjectConstruct = 0x08A76ACCu;   // the base Adhoc object's constructor (object *)
constexpr std::uint32_t kHolderCopy = 0x08A76AB4u;        // thunk to holder_copy(holder *to, const holder *from)

constexpr std::uint32_t kIntegerVtable = 0x08CB8578u;
constexpr std::uint32_t kFloatVtable = 0x08CBBF20u;
constexpr std::uint32_t kBoolVtable = 0x08CBCC00u;
constexpr std::uint32_t kLongVtable = 0x08CB8688u;

constexpr std::uint32_t kIntegerConstruct = 0x08A854CCu;
constexpr std::uint32_t kFloatConstruct = 0x08A87708u;
constexpr std::uint32_t kBoolConstruct = 0x08A973BCu;
constexpr std::uint32_t kLongConstruct = 0x08A5D7A0u;

constexpr std::uint32_t kValue = 0x14u;      // integer, float, boolean (byte)
constexpr std::uint32_t kLongValue = 0x18u;  // 64-bit integer (low word; high word at +0x1C)

// *out = a holder of the freshly constructed `object` (the original passes it through a stack slot).
void hand_out(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t out, std::uint32_t object) {
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store32(frame.at(0u), object);
    (void)call_guest(rt, ctx, kHolderCopy, {out, frame.at(0u)});
}

} // namespace

// Constructors: the base object, then the type's vtable and value.
void construct_integer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    (void)call_guest(rt, ctx, kObjectConstruct, {object});
    rt.memory().store32(object + kValue, value);
    rt.memory().store32(object, kIntegerVtable);
}

void construct_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const auto value = std::bit_cast<std::uint32_t>(ctx.fpr[12]);  // $f12
    (void)call_guest(rt, ctx, kObjectConstruct, {object});
    rt.memory().store32(object + kValue, value);
    rt.memory().store32(object, kFloatVtable);
}

void construct_bool(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const auto value = static_cast<std::uint8_t>(ctx.gpr[5]);
    (void)call_guest(rt, ctx, kObjectConstruct, {object});
    rt.memory().store8(object + kValue, value);
    rt.memory().store32(object, kBoolVtable);
}

void construct_long(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t low = ctx.gpr[6];  // EABI: the 64-bit value in the a2:a3 pair
    const std::uint32_t high = ctx.gpr[7];
    (void)call_guest(rt, ctx, kObjectConstruct, {object});
    rt.memory().store32(object + kLongValue, low);
    rt.memory().store32(object + kLongValue + 4u, high);
    rt.memory().store32(object, kLongVtable);
}

// Factories: *out = a new object of the type holding the value.
void new_integer_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x18u, 4u});
    (void)call_guest(rt, ctx, kIntegerConstruct, {object, value});
    hand_out(rt, ctx, out, object);
}

void new_float_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const float value = ctx.fpr[12];
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x18u, 4u});
    (void)call_guest(rt, ctx, kFloatConstruct, {object}, {value});
    hand_out(rt, ctx, out, object);
}

void new_bool_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5] & 0xFFu;
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x18u, 4u});
    (void)call_guest(rt, ctx, kBoolConstruct, {object, value});
    hand_out(rt, ctx, out, object);
}

void new_long_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t low = ctx.gpr[6];
    const std::uint32_t high = ctx.gpr[7];
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x20u, 4u});
    (void)call_guest(rt, ctx, kLongConstruct, {object, 0u, low, high});
    hand_out(rt, ctx, out, object);
}

} // namespace gt::adhoc::values

GT_DECOMP(0x08A854CCu, gt::adhoc::values::construct_integer, gt::ReturnKind::Void);
GT_DECOMP(0x08A87708u, gt::adhoc::values::construct_float, gt::ReturnKind::Void);
GT_DECOMP(0x08A973BCu, gt::adhoc::values::construct_bool, gt::ReturnKind::Void);
GT_DECOMP(0x08A5D7A0u, gt::adhoc::values::construct_long, gt::ReturnKind::Void);
GT_DECOMP(0x08A85B1Cu, gt::adhoc::values::new_integer_object, gt::ReturnKind::Void);
GT_DECOMP(0x08A87EECu, gt::adhoc::values::new_float_object, gt::ReturnKind::Void);
GT_DECOMP(0x08A97A6Cu, gt::adhoc::values::new_bool_object, gt::ReturnKind::Void);
GT_DECOMP(0x08A5DEE8u, gt::adhoc::values::new_long_object, gt::ReturnKind::Void);
