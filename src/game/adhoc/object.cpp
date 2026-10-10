// Object: the root class of Adhoc values (initializer 0x276614 registers isInstanceOf, getDeepCopy,
// toBool..toDouble, toString, defined, ...; config/symbol_addrs.txt). Its methods are natives
// method(result holder, self holder, argc, argument).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::object {

// self.toInt(): a new integer with the value's integer conversion.
void toInt(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), static_cast<std::uint32_t>(to_int(rt, ctx, self)));
}

// self.toFloat(): a new float with the value's float conversion.
void toFloat(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    return_float(rt, ctx, result, frame.at(0u), to_float(rt, ctx, self));
}

// self.defined(name): with exactly one argument, a new boolean telling whether `name` is defined on
// the value; otherwise the result is left alone.
void defined(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t name = ctx.gpr[7];
    if (argc != 1) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t is_defined = call_guest(rt, ctx, virtual_function(rt, self, kDefinedSlot), {self, name});
    return_bool(rt, ctx, result, frame.at(0u), is_defined);
}

} // namespace gt::adhoc::object

GT_DECOMP(0x08A78EE0u, gt::adhoc::object::toInt, gt::ReturnKind::Void);
GT_DECOMP(0x08A791A0u, gt::adhoc::object::toFloat, gt::ReturnKind::Void);
GT_DECOMP(0x08A79980u, gt::adhoc::object::defined, gt::ReturnKind::Void);
