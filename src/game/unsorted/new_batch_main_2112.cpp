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
void func_088BF3BC(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFpr saved_fpr(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 view = ctx.gpr[5];
    const u32 pair = ctx.gpr[6];
    const u32 behind = self + 4u;              // s2
    const u32 record = word(rt, self + 0x24u); // s0: the delay slot of the first beqz runs on both paths
    ctx.gpr[16] = record;
    ctx.gpr[17] = self;
    ctx.gpr[18] = behind;
    ctx.gpr[19] = view;
    ctx.gpr[20] = pair;
    u32 axis = word(rt, self + 0x14u);
    ctx.gpr[7] = axis;
    float total = 0.0f; // f20

    // The axis record's status byte, bit 0: the two-sample pass.
    if ((rt.memory().load8(axis + 9u) & 1u) != 0u) {
        (void)call_keep(rt, ctx, 0x088BF1FCu,
                        {self, rt.memory().load8(record + 0x10u), rt.memory().load8(record + 0x11u), view,
                         frame.at(8u)},
                        {0.0f});
        const float weight = float_at(rt, record + 0x20u);
        if (weight != 0.0f) {
            (void)call_keep(rt, ctx, 0x088BF1FCu,
                            {self, rt.memory().load8(record + 0x15u), rt.memory().load8(record + 0x11u), view,
                             frame.at(0x10u)},
                            {0.0f});
            const float from_x = float_at(rt, frame.at(8u));
            const float from_y = float_at(rt, frame.at(0xCu));
            const float to_x = float_at(rt, frame.at(0x10u));
            const float to_y = float_at(rt, frame.at(0x14u));
            put_float(rt, frame.at(8u), from_x + (to_x - from_x) * weight);
            put_float(rt, frame.at(0xCu), from_y + (to_y - from_y) * weight);
        }
        // The holder at *(behind + 4) meters the sampled pair: a0 = sp, a1 = sp + 4, f12 = *(holder + 0x704).
        const u32 holder = word(rt, behind + 4u);
        (void)call_keep(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)}, {float_at(rt, holder + 0x704u)});
        const float span_x = float_at(rt, frame.at(8u)) * float_at(rt, frame.at(4u));
        const float span_y = float_at(rt, frame.at(0xCu)) * float_at(rt, frame.at(0u));
        axis = word(rt, self + 0x14u);
        const u32 style = rt.memory().load8(record + 0x10u);
        const float rate = float_at(rt, axis + (((style - 3u) & 0xFFu) < 2u ? 0x84u : 0x7Cu));
        total += rate * (span_x + span_y);
    }

    // Bit 1, unless the style byte says 4: the band erasure of the +0x58 / +0x5C distances. `2.0 - below` is kept
    // only when below is under 2.0; the +0x5C term is subtracted only when above is under 2.0 (bc1fl).
    if ((rt.memory().load8(axis + 9u) & 2u) != 0u && rt.memory().load8(record + 0x10u) != 4u) {
        const float below = float_at(rt, record + 0x58u);
        float adjust = 0.0f;
        if (below < 2.0f) adjust = (2.0f - below) + adjust;
        const float above = float_at(rt, record + 0x5Cu);
        if (above < 2.0f) adjust = adjust - (2.0f - above);
        total += adjust;
    }

    const u32 kind = call(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
    axis = word(rt, self + 0x14u);      // reloaded in the bnez delay slot, on both paths
    u32 shares = word(rt, behind + 4u); // a2

    // Bit 2 and a live +0x20, unless the record already has a +0x12 mode or a +0x96 flag: the four-entry scan.
    if ((kind - 2u) >= 2u) {
        bool scan = false;
        if ((rt.memory().load8(axis + 9u) & 4u) != 0u && word(rt, self + 0x20u) != 0u) {
            scan = rt.memory().load8(record + 0x12u) == 0u || signed_byte(rt, record + 0x96u) == 0;
        }
        if (scan) {
            u32 even = 0u, odd = 0u;
            for (u32 i = 0u; i < 4u; ++i) {
                if (rt.memory().load8(shares + i * 0x118u + 0x15Du) != 0u) {
                    if ((i & 1u) == 0u) {
                        even = 1u;
                    } else {
                        odd = 1u;
                    }
                }
            }
            const u32 difference = even - odd;
            if (difference != 0u) total += static_cast<float>(static_cast<s32>(difference)) * 0.5f;
        }
    }

    // The band: 0.75 unless the direction opposes the +0x12 mode.
    if (!(0.0f <= total)) {
        if (rt.memory().load8(record + 0x12u) == 2u) total *= 0.75f;
    } else if (!(total <= 0.0f)) {
        if (rt.memory().load8(record + 0x12u) == 1u) total *= 0.75f;
    }

    const u32 band = word(rt, self + 0x2Cu);
    const u32 style = rt.memory().load8(record + 0x10u);
    float low = float_at(rt, band + 0x20u);
    float high = float_at(rt, band + 0x24u);
    if (style == 2u) {
        low *= 0.5f;
    } else if (style == 1u) {
        high *= 0.5f;
    }
    const float limit = -low;
    // The target: the negated low bound when the direction is below it, otherwise min(high, direction).
    const float target = (total < limit) ? limit : ((high < total) ? high : total);
    const float step = float_at(rt, axis + 0x88u) * float_at(rt, shares + 0x4F4u);
    float current = float_at(rt, record + 0x54u);
    if (current < target) {
        current += step;
        if (target < current) current = target;
    } else if (!(target < current)) {
        // At the target already (or an incomparable NaN): back off by one step.
        current -= step;
        if (!(current < target)) current = target;
    }
    put_float(rt, record + 0x54u, current);

    // The pair is driven by the clamped +0x54 value along the holder's +0x28 / +0x2C rates.
    const u32 rate_x = word(rt, behind + 4u);
    const float x = float_at(rt, pair + 0u);
    const float y = float_at(rt, pair + 4u);
    put_float(rt, pair + 0u, x + current * float_at(rt, rate_x + 0x28u));
    const u32 rate_y = word(rt, behind + 4u);
    put_float(rt, pair + 4u, y + current * float_at(rt, rate_y + 0x2Cu));
    ctx.gpr[2] = rate_y;
}

// 0x08AF623C (a0 = the object, a1 = the destination block array, a2 = the source record array, a3 = its count).
// The block table at +0x9C is zeroed-per-block in the destination for each source record's id (first pass), then
// the tail pass packs every block whose bit stayed clear into the destination in order; v0 ends as `limit`.

} // namespace
} // namespace new_batch_main_2112
} // namespace gt::game

#define NEW2112(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2112::name, gt::ReturnKind::kind)
NEW2112(0x088BF3BCu, func_088BF3BC, Word);
