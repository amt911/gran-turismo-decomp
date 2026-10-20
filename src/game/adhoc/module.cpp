// Module: the Adhoc module system (initializer 0x27AF54: "Module"; load, defineModule, defineClass,
// import, removeStatic, clearStatic, ...).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::module {
namespace {

constexpr std::uint32_t kCurrentModule = 0x08A7C3A4u;   // (holder *out): the calling script's module
constexpr std::uint32_t kClearStatics = 0x08A7D24Cu;    // (module *): drops its static members
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kRemoveStatic = 0x08A7DE18u;    // (module *, symbol *)
constexpr std::uint32_t kDefineModule = 0x08A7CE50u;    // (module *, module holder *)
constexpr std::uint32_t kDefineNamedModule = 0x08A7CBA8u;  // (module *, symbol *, module holder *)
constexpr std::uint32_t kDefineAttribute = 0x08C203C8u; // (module *, symbol *, value holder *)
constexpr std::uint32_t kNilValue = 0x08A76AA0u;        // (holder *out): nil
constexpr std::uint32_t kHolderCopy = 0x08A76ABCu;      // (holder *out, const holder *)
constexpr std::uint32_t kToSymbolSlot = 23u;            // symbol toSymbol(this) (16 bytes)
constexpr std::uint32_t kSymbolDestroy = 0x08A69D68u;
constexpr std::uint32_t kSymbolBytes = 0x10u;

void destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}
void to_symbol(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t out) {
    (void)call_guest(rt, ctx, virtual_function(rt, object, kToSymbolSlot), {out, object});
}

} // namespace

// Module.clearStatic(): clears the static members of the calling script's module.
void clearStatic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t module = frame.at(0u);
    (void)call_guest(rt, ctx, kCurrentModule, {module});
    (void)call_guest(rt, ctx, kClearStatics, {rt.memory().load32(module)});  // NULL is not checked
    (void)call_guest(rt, ctx, kHolderDestroy, {module});
}

// Module.removeStatic(name), with exactly one argument.
void removeStatic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t module = frame.at(0x0u);
    const std::uint32_t symbol = frame.at(0x10u);
    (void)call_guest(rt, ctx, kCurrentModule, {module});
    if (ctx.gpr[6] == 1u) {
        to_symbol(rt, ctx, rt.memory().load32(argv), symbol);
        (void)call_guest(rt, ctx, kRemoveStatic, {rt.memory().load32(module), symbol});
        (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
    }
    destroy(rt, ctx, module);
}

// Module.defineModule(module) / defineModule(name, module).
void defineModule(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t module = frame.at(0x0u);
    const std::uint32_t other = frame.at(0x4u);
    const std::uint32_t symbol = frame.at(0x10u);
    (void)call_guest(rt, ctx, kCurrentModule, {module});
    if (argc == 1u) {
        (void)call_guest(rt, ctx, kCurrentModule, {other, argv});  // the argument as a module
        (void)call_guest(rt, ctx, kDefineModule, {rt.memory().load32(module), other});
        destroy(rt, ctx, other);
    } else if (argc == 2u) {
        to_symbol(rt, ctx, rt.memory().load32(argv), symbol);
        (void)call_guest(rt, ctx, kCurrentModule, {other, argv + 4u});
        (void)call_guest(rt, ctx, kDefineNamedModule, {rt.memory().load32(module), symbol, other});
        destroy(rt, ctx, other);
        (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
    }
    destroy(rt, ctx, module);
}

// Module.defineAttribute(name[, value = nil]).
void defineAttribute(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t module = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    const std::uint32_t symbol = frame.at(0x10u);
    (void)call_guest(rt, ctx, kCurrentModule, {module});
    if (argc == 1u || argc == 2u) {
        to_symbol(rt, ctx, rt.memory().load32(argv), symbol);
        if (argc == 1u) (void)call_guest(rt, ctx, kNilValue, {value});
        else (void)call_guest(rt, ctx, kHolderCopy, {value, argv + 4u});
        (void)call_guest(rt, ctx, kDefineAttribute, {rt.memory().load32(module), symbol, value});
        destroy(rt, ctx, value);
        (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
    }
    destroy(rt, ctx, module);
}

} // namespace gt::adhoc::module

GT_DECOMP(0x08A7D3CCu, gt::adhoc::module::clearStatic, gt::ReturnKind::Void);
GT_DECOMP(0x08A7E2B0u, gt::adhoc::module::removeStatic, gt::ReturnKind::Void);
GT_DECOMP(0x08A7CEC8u, gt::adhoc::module::defineModule, gt::ReturnKind::Void);
GT_DECOMP(0x08A7CFE8u, gt::adhoc::module::defineAttribute, gt::ReturnKind::Void);
