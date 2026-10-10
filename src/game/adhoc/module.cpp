// Module: the Adhoc module system (initializer 0x27AF54: "Module"; load, defineModule, defineClass,
// import, removeStatic, clearStatic, ...).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::module {
namespace {

constexpr std::uint32_t kCurrentModule = 0x08A7C3A4u;   // (holder *out): the calling script's module
constexpr std::uint32_t kClearStatics = 0x08A7D24Cu;    // (module *): drops its static members
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

} // namespace

// Module.clearStatic(): clears the static members of the calling script's module.
void clearStatic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t module = frame.at(0u);
    (void)call_guest(rt, ctx, kCurrentModule, {module});
    (void)call_guest(rt, ctx, kClearStatics, {rt.memory().load32(module)});  // NULL is not checked
    (void)call_guest(rt, ctx, kHolderDestroy, {module});
}

} // namespace gt::adhoc::module

GT_DECOMP(0x08A7D3CCu, gt::adhoc::module::clearStatic, gt::ReturnKind::Void);
