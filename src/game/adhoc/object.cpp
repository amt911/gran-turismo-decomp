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

// self.isInstanceOf(class): whether self is an instance of the class argv[0] (argc is not checked).
void isInstanceOf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kArgumentAsClass = 0x08A92548u;  // (holder *out, argument)
    constexpr std::uint32_t kIsInstanceOf = 0x08A76F20u;     // (object *, class *) -> bool
    constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t cls = frame.at(0x4u);
    (void)call_guest(rt, ctx, kArgumentAsClass, {cls, argv});
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    const std::uint32_t is = call_guest(rt, ctx, kIsInstanceOf, {self, rt.memory().load32(cls)});
    return_bool(rt, ctx, result, frame.at(0x0u), is);
    (void)call_guest(rt, ctx, kHolderDestroy, {cls});
}

constexpr std::uint32_t kToSymbolSlot = 23u;             // symbol toSymbol(this) (into a 16-byte symbol)
constexpr std::uint32_t kSymbolDestroy = 0x08A69D68u;     // (symbol *)
constexpr std::uint32_t kSymbolBytes = 0x10u;

// self.getObjectAttribute(name): *result = self's attribute `name` (argv[0] as a symbol; argc is not
// checked).
void getObjectAttribute(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kGetAttribute = 0x08A774B0u;  // (object *, holder *result, symbol *)
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    const std::uint32_t name = rt.memory().load32(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t symbol = frame.at(0u);
    (void)call_guest(rt, ctx, virtual_function(rt, name, kToSymbolSlot), {symbol, name});
    (void)call_guest(rt, ctx, kGetAttribute, {self, result, symbol});
    (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
}

// self.setObjectAttribute(name, value), with exactly two arguments.
void setObjectAttribute(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kHolderCopySelf = 0x08A76ABCu;  // (holder *out, const holder *self)
    constexpr std::uint32_t kSetAttribute = 0x08A7761Cu;    // (object *, symbol *, holder *value)
    constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
    const std::uint32_t argv = ctx.gpr[7];
    if (ctx.gpr[6] != 2u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t symbol = frame.at(0x0u);
    const std::uint32_t holder = frame.at(kSymbolBytes);
    (void)call_guest(rt, ctx, kHolderCopySelf, {holder});  // a1 = the self holder, as passed in
    const std::uint32_t self = rt.memory().load32(holder);
    const std::uint32_t name = rt.memory().load32(argv);
    (void)call_guest(rt, ctx, virtual_function(rt, name, kToSymbolSlot), {symbol, name});
    (void)call_guest(rt, ctx, kSetAttribute, {self, symbol, argv + 4u});
    (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// self.toBool(): a new boolean with the value's truth.
void toBool(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t truth = call_guest(rt, ctx, virtual_function(rt, self, kToBoolSlot), {self});
    return_bool(rt, ctx, result, frame.at(0u), truth);
}

// self.toULong(): a new 64-bit integer.
void toULong(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    const GuestResult value = call_guest(rt, ctx, virtual_function(rt, self, kToULongSlot), {self}, {});
    return_long(rt, ctx, result, frame.at(0u), value.v0, value.v1);
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
GT_DECOMP(0x08A78D84u, gt::adhoc::object::toBool, gt::ReturnKind::Void);
GT_DECOMP(0x08A79128u, gt::adhoc::object::toULong, gt::ReturnKind::Void);
GT_DECOMP(0x08A78C6Cu, gt::adhoc::object::isInstanceOf, gt::ReturnKind::Void);
GT_DECOMP(0x08A7758Cu, gt::adhoc::object::getObjectAttribute, gt::ReturnKind::Void);
GT_DECOMP(0x08A7788Cu, gt::adhoc::object::setObjectAttribute, gt::ReturnKind::Void);
