// MManager: the scripts' registry of native classes (MManager.registerClass(class)).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::mmanager {
namespace {

constexpr std::uint32_t kManager = 0x089C5850u;        // (holder *out, self): the manager
constexpr std::uint32_t kArgumentAsClass = 0x08A92548u; // (holder *out, argument): argument as a class
constexpr std::uint32_t kRegister = 0x089C7CD4u;       // (manager *, class holder *)
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

} // namespace

// MManager.registerClass(class): with exactly one argument, registers it.
void registerClass(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argument = ctx.gpr[7];
    if (argc != 1u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t manager = frame.at(0x0u);
    const std::uint32_t cls = frame.at(0x4u);
    (void)call_guest(rt, ctx, kManager, {manager, self});
    (void)call_guest(rt, ctx, kArgumentAsClass, {cls, argument});
    (void)call_guest(rt, ctx, kRegister, {rt.memory().load32(manager), cls});  // NULL is not checked
    (void)call_guest(rt, ctx, kHolderDestroy, {cls});
    (void)call_guest(rt, ctx, kHolderDestroy, {manager});
}

} // namespace gt::adhoc::mmanager

GT_DECOMP(0x089C7EC0u, gt::adhoc::mmanager::registerClass, gt::ReturnKind::Void);
