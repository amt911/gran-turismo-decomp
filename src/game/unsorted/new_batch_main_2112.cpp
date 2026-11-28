// New batch main-session 2112: functions a previous verdict drafted and found mismatching (088BF3BC, 08AF623C),
// re-written from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary). Both earlier
// drafts are reproduced from new_batch_main_1878 / new_batch_main_1596 with the difference the asm shows fixed:
//   0x088BF3BC  the +0x58 / +0x5C "band" branch was inverted: the original keeps `adjust` unchanged when the
//               +0x5C distance is NOT below 2.0 and subtracts (2.0 - above) only when it IS below
//               (bc1fl at 088BF52C branches on the false condition, its delay adds the *unmodified* adjust).
//   0x08AF623C  transcribed as the asm has it (the verdict's mismatch kept no detail); the +0x9C block table and
//               the +0x98 block-count halfword drive a first pass that marks referenced blocks and a tail pass
//               that packs the unmarked ones. 0x0890D050 (a 1224-instruction constructor) is skipped as too hard.
// Delay slots are read as "before the call / branch". v0 / f0 are left as the original leaves them on every exit.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2112 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 as the callee left them: a caller that reads them sees the callee's answer.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// The argument registers are set like the original sets them before the `jal` (a0-a3, t0-t3 in order).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments, {}).v0;
}
inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// s0-s7, fp and ra are callee-saved: the body sets them where the asm does and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// f20-f31 are callee-saved as well.
class SavedFpr {
public:
    explicit SavedFpr(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFpr() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFpr(const SavedFpr &) = delete;
    SavedFpr &operator=(const SavedFpr &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x088BF3BC (a0 = the car record, a1 = the view, a2 = the pair the result drives). *(a0 + 0x24) is the axis
// record, *(a0 + 0x14) the axis state object, *(a0 + 8) the motion holder and *(a0 + 0x2C) the band.

// 0x08AF623C (a0 = the object, a1 = the destination block array, a2 = the source record array, a3 = its count).
// The block table at +0x9C is zeroed-per-block in the destination for each source record's id (first pass), then
// the tail pass packs every block whose bit stayed clear into the destination in order; v0 ends as `limit`.

} // namespace
} // namespace new_batch_main_2112
} // namespace gt::game

#define NEW2112(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2112::name, gt::ReturnKind::kind)
