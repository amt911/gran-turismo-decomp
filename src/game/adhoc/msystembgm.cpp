// MSystemBGM: the scripts' interface to the background music system.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::msystembgm {
namespace {

constexpr std::uint32_t kBgmContextSize = 0x08AC329Cu;  // () -> int

} // namespace

// MSystemBGM.getContextSize() -> integer.
void getContextSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t size = call_guest(rt, ctx, kBgmContextSize, {});
    return_integer(rt, ctx, result, frame.at(0u), size);
}

} // namespace gt::adhoc::msystembgm

GT_DECOMP(0x08A20514u, gt::adhoc::msystembgm::getContextSize, gt::ReturnKind::Void);
