// MSystemBGM: the scripts' interface to the background music system.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::msystembgm {
namespace {

constexpr std::uint32_t kBgmContextSize = 0x08AC329Cu;  // () -> int
constexpr std::uint32_t kBgmPlayOrStop = 0x08A202F0u;   // (result, argc, argv, play): shared by play/stop

} // namespace

// MSystemBGM.getContextSize() -> integer.
void getContextSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t size = call_guest(rt, ctx, kBgmContextSize, {});
    return_integer(rt, ctx, result, frame.at(0u), size);
}

// MSystemBGM.stop(...): the play/stop body with play = 0 (play() jumps there with 1).
void stop(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kBgmPlayOrStop, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], 0u});
}

} // namespace gt::adhoc::msystembgm

GT_DECOMP(0x08A20514u, gt::adhoc::msystembgm::getContextSize, gt::ReturnKind::Void);
GT_DECOMP(0x08A20414u, gt::adhoc::msystembgm::stop, gt::ReturnKind::Void);
