// New batch main-session 1014: functions earlier passes could not match, hand-written from `tools.disasm` as structured
// C (named locals, `G[n]` only where the original passes an argument).
//
//   0x088BF3BC  0x088BF3BC(self, view, pair): the steering/band pass of the car record. *(self + 0x24) holds the axis
//               record (bytes +0x10, +0x12, +0x15, +0x96, floats +0x20, +0x54, +0x58, +0x5C), *(self + 0x14) the axis
//               state object (byte +9 gates the three passes, floats +0x7C / +0x84 / +0x88), *(self + 8) the motion
//               holder (floats +0x28, +0x2C, +0x4F4). Two 0x088BF1FC samples of `view` land on the frame pair at sp+8,
//               the second one weighted by the record's +0x20; 0x08B498AC meters that pair against the holder's +0x704;
//               finally the +0x54 value is stepped toward the band and the `pair` floats are driven by it.
//   0x088F6FC8  (self, source): sizes the GU command block from the four words at 0x08C8A420, copies the 4x2 matrix
//               rows at +0x00..+0x34 into +0xC0..+0xDC, then derives the cell ranges from the +0x28 / +0x14 spacing
//               (a spacing of zero gives the infinite range), the counts from *(source + 4) / *(source + 8) and the
//               angle from a 0x08B65F0C / 0x08B70F28 / 0x08B71720 / 0x08B71EC8 atan2 chain.
//   0x089419D8  (self, source, strength, kind, extra, f12): builds the two 4-float rows at +0x18 / +0x28 of `self`
//               from the byte fields of `source` (+1, +2, +4..+0xB, +0xD) and its signed halves (+0x10..+0x16), with
//               the geometry scaled by 0.01 * (strength & 0xFF). `kind` 1 / 3 and 1 / 2 select the sampling paths.
//
// Delay slots are read as "before the call / branch"; a branch-likely's delay slot only when the branch is taken (the
// repo convention, hot_batch_100's header). v0 is left as the original leaves it on every exit path. Unverified drafts:
// the gate decides which of them keep their registration.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1014 {
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
inline u32 call(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
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

// The float constants the original keeps in .rodata, by their encoded value (0x3C23D70A = 0.01f, 0x411CCCCD = 9.8f).
inline constexpr float kHundredth = std::bit_cast<float>(0x3C23D70Au);
inline constexpr float kNinePointEight = std::bit_cast<float>(0x411CCCCDu);
// The table the source-pointer object's vtable routes through (0x08C8A420, four words).
inline constexpr u32 kGuBlockWords = 0x08C8A420u;
// The byte at +0x14D of this singleton selects the frame-row layout in 0x089419D8.
inline constexpr u32 kRowLayout = 0x08CDE508u;

// 0x088BF3BC (a0 = the car record, a1 = the view, a2 = the pair the result drives).

// 0x088F6FC8 (a0 = the object, a1 = the source record).

// 0x089419D8 (a0 = the record written, a1 = the source, a2 = the strength byte, a3 = the kind, t0 = extra, f12 = scale).

} // namespace
} // namespace new_batch_main_1014
} // namespace gt::game

#define MAIN1014(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1014::name, gt::ReturnKind::kind)

