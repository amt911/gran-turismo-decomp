// Hot batch 137: the last functions the 16 routes reach that were still recompiled (besides the SDK's longjmp, the
// module entry and a thread entry), written by hand from `tools.disasm`; role words describe access shapes, none is a
// game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_137 {

using namespace gt::game::hot;

// Copies the four floats at a1 to a0 with the middle two swapped, through a stack copy func_08B4D05C(&tmp, a1) makes
// (a1 stays in its register); the floats are moved as bits, and f0-f3 keep tmp +0 / +0xC / +8 / +4 as the original
// leaves them.
void func_08B4D080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t tmp = frame.at(0u);
    ctx.gpr[4] = tmp;
    call_guest(rt, ctx, 0x08B4D05Cu, {tmp, ctx.gpr[5]});
    const std::uint32_t x = word(rt, tmp), y = word(rt, tmp + 4u), z = word(rt, tmp + 8u), w = word(rt, tmp + 0xCu);
    put(rt, out + 8u, z);
    put(rt, out, x);
    put(rt, out + 0xCu, w);
    put(rt, out + 4u, y);
    ctx.fpr[0] = std::bit_cast<float>(x);
    ctx.fpr[1] = std::bit_cast<float>(w);
    ctx.fpr[2] = std::bit_cast<float>(z);
    ctx.fpr[3] = std::bit_cast<float>(y);
}

// func_089310A0(0, a0, a1, a2) (a tail jump; its result is returned).
void func_0893119C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    ctx.gpr[2] = call_guest(rt, ctx, 0x089310A0u, {0u, a0, a1, a2});
}

} // namespace hot_batch_137
} // namespace gt::game

#define HOT137(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_137::name, gt::ReturnKind::kind)
HOT137(0x08B4D080u, func_08B4D080, Void);
HOT137(0x0893119Cu, func_0893119C, Word);
