// New batch main 1082 (draft): one function, 1111 instructions, written by hand from `tools/disasm` as
// structured C. Not verified by the differential gate yet.
//
// 0x0895A2F8 (a0 = the object, a1 = a time in its own unit; v0 = as the registers leave it) advances one
// keyframe of the camera/pose track of the object `a0`: it reads the current record (0x08959180(a0->0,
// byte state[0])) and the record 0x3C bytes further on, samples the pose track into two 0x34-byte records on
// its frame with 0x08959704, blends them by the distance between the two records (the gains at 0x08CF3570 /
// 0x08CF3588 and the table 0x08CDE1F4 the state machine reads), and integrates the two spring lanes at
// state +0x6CC..+0x6D4 before handing the pose to 0x0894106C.
//
// Quirks kept as the asm has them:
//   * the two small state bytes compared at 0x0895A3E0 / 0x0895A4A8 / 0x0895A6CC use the "too far" rule of
//     `jump_too_far` (a jump of 3 or more forward, or a single step back), and the byte is stored only on
//     the path that recomputes the pose (0x0895A710);
//   * the pose record at frame +0x28 is only filled when 0x08959704 runs, and the tail reads its halves and
//     bytes (frame +0x54..+0x5A) on every path: on the paths that skip the call those words are whatever the
//     stack held, as in the original (the harness ignores the dead stack, so this only shows through the
//     values the function stores);
//   * `v0` is compared (ReturnKind::Word): the three exits end with a zero, with byte state[0xD], and with the
//     leftover frame address the last store loop leaves in v0 (0x0895B3A4).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1082 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A call that leaves v0 / v1 / f0 as the callee left them (the tail reads f0 and v0 of several calls).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments ($f12-$f15), which is how every float-taking callee here is called.
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_keepf(rt, ctx, target, arguments, floats).f0;
}

u32 load_u8(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 load_u16(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void store_u8(Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
void store_u16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
s32 sext8(u32 value) { return static_cast<s32>(static_cast<std::int8_t>(value)); }
s32 sext16(u32 value) { return static_cast<s32>(static_cast<std::int16_t>(value)); }
// `trunc.w.s` as the baseline does it: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_word(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
constexpr float fbits(u32 bits) { return std::bit_cast<float>(bits); }

// The two small state bytes: a jump of three or more forward, or a single step back, is "too far" (this is
// the sltu/slti pair at 0x0895A3F0 and 0x0895A4BC read as one predicate).
bool jump_too_far(u32 current, u32 reference) {
    const s32 a = static_cast<s32>(current & 0xFFu);
    const s32 b = static_cast<s32>(reference & 0xFFu);
    if (a == b) return false;
    if (b > a) return (b - a) >= 3;
    return (a - b) == 1;
}

// Where the prologue stores a callee-saved register and gives it back on return (same shape as the other
// batch files): reg 0-31 is a general register, 32-63 is floating (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots)
        : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

// The 0x34-byte record 0x08959704 fills: an index, four bytes, nine floats and two halves plus two small
// fields. The first sample sits at frame + 0x28, the second at frame + 0x5C (so their fields never overlap).
constexpr u32 kPoseA = 0x28u;
constexpr u32 kPoseB = 0x5Cu;

void func_0895A2F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 now = ctx.gpr[5];

    GuestFrame frame(ctx, 0xF0u);
    SavedRegisters saved(rt, ctx, frame,
                         {{20u, 0xB4u}, {19u, 0xB0u}, {17u, 0xA4u}, {16u, 0xA0u}, {31u, 0xC4u},
                          {30u, 0xC0u}, {23u, 0xBCu}, {22u, 0xB8u}, {18u, 0xA8u}, {52u, 0xE0u},
                          {51u, 0xDCu}, {50u, 0xD8u}, {49u, 0xD4u}, {48u, 0xD0u}});

    constexpr float kTwoPi = fbits(0x40C90FDBu);   // 6.2831855
    constexpr float kPi = fbits(0x40490FDBu);      // 3.1415927
    constexpr float kHalfPi3 = fbits(0x40060A92u); // 2.0943952
    constexpr float kStep = fbits(0x3CD6AE37u);      // 0.0261799
    constexpr float kDegree = fbits(0x3C0EFA35u);    // 0.0087266
    constexpr float kPiOver30 = fbits(0x3DD67750u);  // 0.1047198
    constexpr float kRate = fbits(0x3C88AB7Cu);    // 1/60
    constexpr float kGravity = fbits(0x411CCCCDu); // 9.8
    constexpr u32 kGain1 = 0x08CF3570u;            // the spring gains, written by 0x089594D8
    constexpr u32 kGain2 = 0x08CF3588u;
    constexpr u32 kTable = 0x08CDE1F4u;            // the byte block the same function fills

    // 0x0895A340: the first record and the two fields the state machine compares.
    u32 state = word(rt, object + 8u);
    u32 record = call(rt, ctx, 0x08959180u, {word(rt, object), load_u8(rt, state)});
    const u32 record4 = record + 4u;
    const u32 record3c = record + 0x3Cu;

    if (word(rt, record + 0xA0u) == 2u && word(rt, record + 0xA4u) != 0u) {
        const s32 elapsed = static_cast<s32>(now - word(rt, record + 4u));
        put(rt, record + 0xCCu, static_cast<u32>(elapsed > 0 ? elapsed : 0));
    }
    const u32 stop = word(rt, record + 0x9Cu);
    if (stop != 0xFFFFFFFFu) {
        if (static_cast<s32>(stop) < static_cast<s32>(word(rt, record + 0x3Cu))) {
            put(rt, record + 0x9Cu, 0xFFFFFFFFu);
        }
    }
    if (word(rt, record + 4u) == 0xFFFFFFFFu) {
        if (load_u8(rt, record + 0x74u) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        call(rt, ctx, 0x0895A0D8u, {object, record4, now});
        store_u8(rt, record + 0x74u, 0u);
    }

    // 0x0895A3E0: the record's byte +0x76 against the next record's byte +7; a jump the wrong way drops the
    // pose and zeroes the two lanes and the spring value (0x0895A424).
    {
        const u32 current = load_u8(rt, record + 0x76u);
        const u32 reference = load_u8(rt, record4 + 7u);
        if (current != reference && jump_too_far(current, reference)) {
            const u32 enabled = load_u8(rt, state + 0xDu);
            if (enabled == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
            put(rt, state + 0x6F8u, 0u);
            put(rt, state + 0x6CCu, 0u);
            put(rt, state + 0x6D0u, 0u);
            ctx.gpr[2] = enabled;
            return;
        }
    }

    // 0x0895A440: sample the pose track at the first record and clamp the distance to the second record.
    s32 range;
    {
        call(rt, ctx, 0x08959704u, {state, frame.at(kPoseA), record4, now - word(rt, record4)});
        const u32 lane_count = load_u8(rt, kTable + 2u);
        const u32 kind = load_u8(rt, kTable + 4u);
        const s32 elapsed = static_cast<s32>(now - word(rt, record + 0x38u));
        const s32 cap = 300 * static_cast<s32>(lane_count);
        range = elapsed < 0 ? 0 : elapsed;
        if (cap < range) range = cap;

        if (kind != 0u) {
            const u32 started = word(rt, record + 0x70u);
            bool blend = started != 0xFFFFFFFFu;
            if (blend) {
                const u32 current = load_u8(rt, record + 0x76u);
                const u32 reference = load_u8(rt, record3c + 7u);
                if (jump_too_far(current, reference)) {
                    blend = false;
                } else {
                    const s32 window = static_cast<s32>(word(rt, record + 0x38u)) -
                                       static_cast<s32>(started);
                    const s32 span = 50 * static_cast<s32>(load_u8(rt, kTable + 5u));
                    if (span < window) {
                        blend = false;
                    } else {
                        const u32 index = word(rt, record + 0x9Cu);
                        const bool short_hop = span < range;
                        if (index == 0xFFFFFFFFu) {
                            blend = !short_hop;
                        } else {
                            blend = static_cast<s32>(index) < static_cast<s32>(word(rt, record + 0x3Cu)) &&
                                    !short_hop;
                        }
                    }
                }
            }
            if (blend) {
                // 0x0895A544: sample the second record's pose and blend the two by the hop (0.5 or 1.0
                // added for the two marked kinds, as 0x0895A5AC does).
                call(rt, ctx, 0x08959704u, {state, frame.at(kPoseB), record3c, now - word(rt, record3c)});
                float ratio = static_cast<float>(range) / static_cast<float>(50 * static_cast<s32>(load_u8(rt, kTable + 5u)));
                put_float(rt, frame.at(8u), ratio);
                const u32 blend_kind = load_u8(rt, kTable + 4u);
                if (blend_kind == 2u) ratio = ratio + 0.5f;
                else if (blend_kind == 3u) ratio = ratio + 1.0f;
                put_float(rt, frame.at(8u), ratio);
                const float inverse = 1.0f - ratio;
                put_float(rt, frame.at(4u), inverse);
                for (u32 i = 0; i < 2u; ++i) {
                    const u32 to = frame.at(kPoseA + 8u) + 4u * i;
                    const u32 from = frame.at(kPoseB + 8u) + 4u * i;
                    put_float(rt, to, float_at(rt, to) * ratio + float_at(rt, from) * inverse);
                }
                put_float(rt, frame.at(kPoseA + 0x18u),
                          float_at(rt, frame.at(kPoseA + 0x18u)) * ratio +
                              float_at(rt, frame.at(kPoseB + 0x18u)) * inverse);
                put_float(rt, frame.at(kPoseA + 0x14u),
                          float_at(rt, frame.at(kPoseA + 0x14u)) * ratio +
                              float_at(rt, frame.at(kPoseB + 0x14u)) * inverse);
                const GuestResult turned =
                    call_keep(rt, ctx, 0x0893C6A8u,
                              {frame.at(8u), frame.at(kPoseA + 0x10u), frame.at(4u), frame.at(kPoseB + 0x10u)});
                put_float(rt, frame.at(kPoseA + 0x10u), turned.f0);
                put_float(rt, frame.at(kPoseA + 0x1Cu),
                          float_at(rt, frame.at(kPoseA + 0x1Cu)) * float_at(rt, frame.at(8u)) +
                              float_at(rt, frame.at(kPoseB + 0x1Cu)) * float_at(rt, frame.at(4u)));
            }
        }
    }

    // 0x0895A678: the record the state now points at.
    state = word(rt, object + 8u);
    record = call(rt, ctx, 0x08959180u, {word(rt, object), load_u8(rt, state)});
    const u32 fresh = load_u8(rt, record + 0x75u);
    const u32 index = word(rt, record + 0x9Cu);
    store_u8(rt, record + 0x75u, 1u);
    u32 flag_9c = fresh < 1u ? 1u : 0u;
    u32 flag_98 = 0u;
    if (index != 0xFFFFFFFFu) {
        // 0x0895A6B0: the flag stays 1 when the index is NOT below the sample's index (beqz skips the clear at 0x0895A6C4).
        flag_98 = 1u;
        if (static_cast<s32>(index) < static_cast<s32>(word(rt, frame.at(kPoseA)))) flag_98 = 0u;
    }
    const u32 next_state = load_u8(rt, frame.at(kPoseA + 7u));

    // 0x0895A6CC: the byte advanced normally, so the pose is recomputed here (0x0895B3C0); a jump the wrong
    // way or no change leaves the pose as it is.
    bool pose_valid = false;
    {
        const u32 current = load_u8(rt, record + 0x76u);
        if (current != next_state && !jump_too_far(current, next_state)) {
            store_u8(rt, record + 0x76u, next_state);
            const float step = float_at(rt, state + 0x18u);
            const u32 first = word(rt, state + 0x690u);
            const u32 second = word(rt, state + 0x694u);
            call_keepf(rt, ctx, 0x0894106Cu, {object, next_state, first, second, 0u},
                       {float_at(rt, frame.at(kPoseA + 8u)), float_at(rt, frame.at(kPoseA + 0xCu)), step,
                        float_at(rt, frame.at(kPoseA + 0x10u))});
            const u32 entry = call(rt, ctx, 0x0893C780u, {object, 0u});
            const float angle = float_at(rt, frame.at(kPoseA + 8u));
            if (load_u8(rt, entry + 0x2Cu) != 0u) {
                // a1 is what 0x0893C780 (a1 = 0 in, a1 << 3 out) left: 0.
                call_keepf(rt, ctx, 0x0894106Cu, {object, 0u, first, second, 0u},
                           {angle, float_at(rt, frame.at(kPoseA + 0xCu)), 100.0f + step,
                            float_at(rt, frame.at(kPoseA + 0x10u))});
            }
            // 0x0895A744: the pose lanes and their spring derivative.
            const float spring = float_at(rt, state + 0x6Cu);
            put_float(rt, state + 0x6CCu, float_at(rt, frame.at(kPoseA + 0x14u)));
            put_float(rt, state + 0x6D0u, float_at(rt, frame.at(kPoseA + 0x18u)));
            const float pull = float_at(rt, state + 0x64u) * float_at(rt, state + 0x6CCu) +
                               float_at(rt, state + 0x68u) * float_at(rt, state + 0x6D0u);
            const float value = spring * pull;
            if (0.5f <= spring) put_float(rt, state + 0x6D4u, -value / (spring * spring));
            // 0x0895A7A4: the new spring value is the blended float at frame +0x44.
            flag_9c = 1u;
            pose_valid = true;
            put_float(rt, state + 0x6F8u, float_at(rt, frame.at(kPoseA + 0x1Cu)));
            call(rt, ctx, 0x089567C0u, {object});
            call(rt, ctx, 0x08956B20u, {object});
        }
    }

    // 0x0895A7D8: the input command bytes of the first sample.
    const u32 mode = (load_u8(rt, frame.at(kPoseA + 4u)) >> 5) & 3u;
    store_u8(rt, state + 0x5F9u, mode);
    if (mode == 1u) put_float(rt, state + 0x5ECu, 1.0f);
    else put(rt, state + 0x5ECu, 0u);
    call(rt, ctx, 0x0895DCACu, {object, load_u8(rt, frame.at(kPoseA + 4u)) & 0xFu});
    {
        const float share = static_cast<float>(load_u8(rt, frame.at(kPoseA + 5u)) >> 4) / 15.0f;
        put_float(rt, state + 0x534u, share);
        put_float(rt, state + 0x530u, share);
        put_float(rt, state + 0x538u,
                  static_cast<float>(load_u8(rt, frame.at(kPoseA + 5u)) & 0xFu) / 15.0f);
        const float limit = static_cast<float>(sext8(load_u8(rt, frame.at(kPoseA + 6u)))) * kDegree;
        float angle = float_at(rt, state + 0x520u);
        if (angle < limit) {
            const float shifted = angle + kStep;
            angle = limit < shifted ? limit : shifted;
        } else if (limit < angle) {
            const float shifted = angle - kStep;
            angle = shifted < limit ? limit : shifted;
        }
        put_float(rt, state + 0x520u, angle);
        put_float(rt, state + 0x518u, call_float(rt, ctx, 0x08952550u, {}, {angle}));
    }
    call(rt, ctx, 0x0894E9C0u, {object});

    // 0x0895A8EC: four lanes accumulated from the two sample's halves.
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    const u32 car = word(rt, object + 0xCu);
    float f20 = 0.0f;
    {
        const float squared = float_at(rt, state + 0x6C4u);
        f20 = squared * squared;
        for (u32 i = 0; i < 4u; ++i) {
            const u32 lane = call(rt, ctx, 0x08955300u, {object, i});
            const u32 column = load_u8(rt, lane + 0xE9u);
            const u32 entry = call(rt, ctx, 0x0894268Cu, {car, i, column});
            const u32 offset = (i >> 1) << 2;
            const u32 base = offset + car;
            const float rate = float_at(rt, base + 0x1404u);
            const float target = float_at(rt, base + 0x13D8u);
            const float weight = float_at(rt, entry);
            const u32 slot = frame.at(0x10u) + offset;
            put_float(rt, slot, float_at(rt, slot) + weight * (target + f20 * rate));
        }
    }
    // 0x0895A95C: the two pose floats into the working pair.
    for (u32 i = 0; i < 2u; ++i) {
        put_float(rt, frame.at(0x18u) + 4u * i, float_at(rt, frame.at(kPoseA + 8u) + 4u * i));
    }

    // 0x0895A984: with no fresh pose the pair is turned by the pose angle and the gains; otherwise it keeps
    // the sampled values. f22 / f21 are the pair, f20 / f23 / f24 the temporaries the saves hold.
    float f22 = 0.0f;
    float f21 = 0.0f;
    float f23 = 0.0f;
    float f24 = 0.0f;
    if (!pose_valid) {
        call_keepf(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)},
                   {float_at(rt, frame.at(kPoseA + 0x10u))});
        const float sine = float_at(rt, frame.at(0xCu));
        const float cosine = float_at(rt, frame.at(8u));
        const float ahead = float_at(rt, frame.at(kPoseA + 0x14u));
        const float neg_sine = -sine;
        const float side = float_at(rt, frame.at(kPoseA + 0x18u));
        float turned = ahead * cosine;
        const float gain_a = float_at(rt, kGain2 + 0xCu);
        const float cross = neg_sine * side;
        const float gain_b = float_at(rt, kGain1 + 0xCu);
        turned = turned + cross;
        put_float(rt, frame.at(0xCu), neg_sine);
        turned = turned * gain_a;
        f22 = float_at(rt, frame.at(0x18u));
        f21 = float_at(rt, frame.at(0x1Cu));
        turned = turned / gain_b;
        const u32 lanes = load_u8(rt, kTable + 0xEu);
        const float side_step = neg_sine * turned;
        const float ahead_step = cosine * turned;
        f21 = f21 + side_step;
        f22 = f22 + ahead_step;
        put_float(rt, frame.at(0x1Cu), f21);
        if (lanes != 0u && !(static_cast<s32>(300u * lanes) < range)) {
            const float first_part = cosine * side;
            const float second_part = sine * ahead;
            const float gain = (second_part + first_part) * float_at(rt, kGain2 + 0x10u) /
                               float_at(rt, kGain1 + 0x10u);
            f21 = f21 + cosine * gain;
            f22 = f22 + sine * gain;
            put_float(rt, frame.at(0x1Cu), f21);
            put_float(rt, frame.at(0x18u), f22);
        } else {
            put_float(rt, frame.at(0x18u), f22);
        }
    } else {
        f22 = float_at(rt, frame.at(0x18u));
        f21 = float_at(rt, frame.at(0x1Cu));
    }

    // 0x0895AA68: the drift/offset correction of the pair.
    float f4 = float_at(rt, frame.at(kPoseA + 0x10u));
    put_float(rt, frame.at(4u), f4);
    if (!pose_valid) {
        const u32 lanes = load_u8(rt, kTable + 0xFu);
        if (lanes != 0u && !(static_cast<s32>(300u * lanes) < range)) {
            f4 = f4 + float_at(rt, kGain2 + 0x14u) * float_at(rt, frame.at(kPoseA + 0x1Cu)) /
                          float_at(rt, kGain1 + 0x14u);
            put_float(rt, frame.at(4u), f4);
        }
    }
    {
        const float height = float_at(rt, state + 0x10u);
        const float lateral = float_at(rt, state + 0x14u);
        const float dx = f22 - height;
        const float dy = f21 - lateral;
        const float near_distance = call_float(rt, ctx, 0x08B49D7Cu, {}, {dx * dx + dy * dy});
        const u32 pair = record + 4u;
        const float ex = f21 - float_at(rt, pair + 0xCu);
        const float ey = f22 - float_at(rt, pair + 8u);
        const float far_distance = call_float(rt, ctx, 0x08B49D7Cu, {}, {ey * ey + ex * ex});

        // 0x0895AB10: close to the pair (or a live key hop) the pose comes from the state directly. The
        // register a1 the two 0x0894106C calls read is 8 on the path that skipped the turn block (it holds
        // the copy loop's bound there) and the gain table otherwise, as in the asm.
        const bool direct = range != 0 || near_distance <= 8.0f || near_distance <= far_distance;
        const u32 selector = pose_valid ? 8u : kGain1;
        if (!direct) {
            const float step = float_at(rt, state + 0x18u);
            const u32 first = word(rt, state + 0x690u);
            const u32 second = word(rt, state + 0x694u);
            put(rt, frame.at(0x90u), first);
            put(rt, frame.at(0x94u), second);
            call_keepf(rt, ctx, 0x0894106Cu, {object, selector, first, second, 0u},
                       {float_at(rt, pair + 8u), float_at(rt, pair + 0xCu), step,
                        float_at(rt, pair + 0x10u)});
            const u32 entry = call(rt, ctx, 0x0893C780u, {object, 0u});
            if (load_u8(rt, entry + 0x2Cu) != 0u) {
                call_keepf(rt, ctx, 0x0894106Cu, {object, 0u, first, second, 0u},
                           {float_at(rt, pair + 8u), float_at(rt, pair + 0xCu), 100.0f + step,
                            float_at(rt, pair + 0x10u)});
            }
            // 0x0895ABC0: the same lane/spring update as 0x0895A744, with the pair's own float +0x1C.
            const float spring = float_at(rt, state + 0x6Cu);
            put_float(rt, state + 0x6CCu, float_at(rt, frame.at(kPoseA + 0x14u)));
            put_float(rt, state + 0x6D0u, float_at(rt, frame.at(kPoseA + 0x18u)));
            const float pull = float_at(rt, state + 0x64u) * float_at(rt, state + 0x6CCu) +
                               float_at(rt, state + 0x68u) * float_at(rt, state + 0x6D0u);
            const float value = spring * pull;
            put_float(rt, state + 0x6F8u, float_at(rt, pair + 0x1Cu));
            if (0.5f <= spring) put_float(rt, state + 0x6D4u, -value / (spring * spring));
            call(rt, ctx, 0x089567C0u, {object});
            call(rt, ctx, 0x08956B20u, {object});
        }

        // 0x0895AC38: the correction angle, and the pair's offset in the state.
        if (static_cast<s32>(range) < static_cast<s32>(300u * load_u8(rt, kTable + 0xDu))) {
            pose_valid = false;
        } else {
            pose_valid = false;
            const u32 holder = word(rt, word(rt, object) + 0x10u);
            if (holder == 0u || load_u8(rt, state + 0xDu) != 0u) {
                // 0x0895AD54: no correction, the state's own angle is the offset.
                pose_valid = true;
                put_float(rt, frame.at(4u), float_at(rt, state + 0x704u));
            } else {
                put_float(rt, frame.at(kPoseB), float_at(rt, state + 0x10u));
                put_float(rt, frame.at(kPoseB + 4u), float_at(rt, state + 0x14u));
                put_float(rt, frame.at(kPoseB + 8u), float_at(rt, state + 0x18u) + 1.0f);
                pose_valid = true;
                const u32 method = word(rt, word(rt, holder) + 0x14u);
                const u32 answered = call_indirect(rt, ctx, method,
                                                   {holder, load_u8(rt, state), frame.at(kPoseB),
                                                    frame.at(0x20u), frame.at(0xCu)});
                if (answered != 0u) {
                    const float direction = call_float(rt, ctx, 0x08B4A430u, {},
                                                       {-float_at(rt, frame.at(0x20u)),
                                                        float_at(rt, frame.at(0x24u))});
                    put_float(rt, frame.at(8u), direction);
                    float difference =
                        call_float(rt, ctx, 0x0893C60Cu, {frame.at(8u), state + 0x704u}, {});
                    if (difference < 0.0f) difference = -difference;
                    if (kHalfPi3 < difference) {
                        put_float(rt, frame.at(0u), float_at(rt, frame.at(8u)) + kPi);
                        const float wrapped = call_float(rt, ctx, 0x0893C54Cu, {frame.at(0u)}, {});
                        put_float(rt, frame.at(8u), wrapped);
                    }
                    pose_valid = true;
                    put_float(rt, frame.at(4u), float_at(rt, frame.at(8u)));
                }
            }
        }
    }

    // 0x0895AD5C / 0x0895AEBC: with the first sample in use the pose is copied through; otherwise the pair
    // drives the state's own velocity lanes.
    if (flag_9c != 0) {
        put_float(rt, state + 0x6F8u, float_at(rt, frame.at(kPoseA + 0x1Cu)));
        put_float(rt, state + 0x6CCu, float_at(rt, frame.at(kPoseA + 0x14u)));
        put_float(rt, state + 0x6D0u, float_at(rt, frame.at(kPoseA + 0x18u)));
    } else {
        const float angle = call_float(rt, ctx, 0x0893C60Cu, {frame.at(4u), state + 0x704u}, {});
        const float gain = float_at(rt, kGain1 + 0x14u);
        const float spring_now = float_at(rt, state + 0x6F8u);
        float drift = angle * gain - float_at(rt, kGain2 + 0x14u) * spring_now;
        const float lateral = float_at(rt, car + 0x10u);
        if (load_u8(rt, kTable + 0x23u) != 0u) {
            const float limit = float_at(rt, frame.at(0x10u)) * lateral * float_at(rt, car + 0x14Cu) *
                                float_at(rt, 0x08CF356Cu);
            if (limit < drift) drift = limit;
            else if (drift < -limit) drift = -limit;
        }
        const float wheel = float_at(rt, car + 0x140u);
        const float mass = float_at(rt, car + 0x68u);
        f24 = drift * wheel / (lateral * mass);
        if (flag_98 != 0u) f24 = 0.0f;

        const float spring_step = spring_now + drift * kRate;
        put_float(rt, state + 0x6F8u, spring_step);
        call_keepf(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)},
                   {float_at(rt, state + 0x704u)});
        const float lane_x = float_at(rt, state + 0x6CCu);
        const float lane_y = float_at(rt, state + 0x6D0u);
        const float sine = float_at(rt, frame.at(0xCu));
        const float cosine = float_at(rt, frame.at(8u));
        float dx = f22 - float_at(rt, state + 0x10u);
        float dy = f21 - float_at(rt, state + 0x14u);
        float forward = dx * sine;
        float sideways = dy * cosine;
        const float neg_cosine = -cosine;
        const float dy_cos = dy * sine;
        f20 = forward + sideways;
        dx = dx * neg_cosine;
        const float lane_a = sine * lane_x;
        float lane_b = cosine * lane_y;
        const float twice = f20 + f20;
        f21 = dx + dy_cos;
        const float neg_lane = neg_cosine * lane_x;
        const float lane_c = sine * lane_y;
        const float dist = call_float(rt, ctx, 0x08B49D7Cu, {}, {twice * twice + f21 * f21});
        f22 = lane_a + lane_b;
        f23 = neg_lane + lane_c;

        // 0x0895AEA8: a hysteresis flag on the pair's distance.
        if (load_u8(rt, record + 0x77u) == 0u) {
            if (5.0f < dist) store_u8(rt, record + 0x77u, 1u);
        } else if (dist < 1.0f) {
            store_u8(rt, record + 0x77u, 0u);
        }

        // 0x0895AF10: clamp the pair's x to the lane limit.
        const float clamp = float_at(rt, 0x08CF3568u);
        if (clamp < f20) f20 = clamp;
        else if (f20 < -clamp) f20 = -clamp;

        // 0x0895AF40: the pair's lateral term, zeroed when the pose was already valid or flag_98 is set.
        const float gain_lat = float_at(rt, kGain2 + 0x10u);
        float lateral_term = f22 * gain_lat;
        float lateral_other = f20 * float_at(rt, kGain1 + 0x10u);
        float term;
        if (!pose_valid) term = lateral_other - lateral_term;
        else term = f22 * (-gain_lat);
        const float spring_term = f24 * float_at(rt, 0x08CF3564u);
        float f6 = flag_98 != 0u ? 0.0f : term - spring_term;
        {
            const float share = (float_at(rt, frame.at(0x10u)) + float_at(rt, frame.at(0x14u))) /
                                float_at(rt, car + 0x68u);
            if (load_u8(rt, kTable + 0x22u) != 0u) {
                const float limit = share * float_at(rt, 0x08CF3560u);
                if (limit < f6) f6 = limit;
                else if (f6 < -limit) f6 = -limit;
            }
            // 0x0895AFD8: the heading correction, clamped to the lane's own limit.
            const float reach = float_at(rt, 0x08CF355Cu);
            const float turn = f6 * float_at(rt, state + 0x60u);
            float f5 = f22 + f6 * kRate;
            float heading;
            f6 = turn + 0.0f; // the add sits in the delay slot of the branch (0x0895B004): every path
            if (reach < f21) {
                heading = reach;
            } else if (f21 < -reach) {
                heading = -reach;
            } else {
                heading = f21;
            }
            // 0x0895B01C: the next blend of the pair into the lanes.
            float f3 = float_at(rt, kGain1 + 0xCu) * heading - f23 * float_at(rt, kGain2 + 0xCu);
            if (pose_valid) f3 = 0.0f;
            if (flag_98 != 0u) f3 = 0.0f;
            const float side = f3 * f23;
            if (0.0f < side) {
                if (load_u8(rt, kTable + 0x20u) != 0u) {
                    const float limit = share * float_at(rt, 0x08CF3558u);
                    if (limit < f3) f3 = limit;
                    else if (f3 < -limit) f3 = -limit;
                }
            } else if (load_u8(rt, kTable + 0x21u) != 0u) {
                const float limit = share * float_at(rt, 0x08CF3554u);
                if (limit < f3) f3 = limit;
                else if (f3 < -limit) f3 = -limit;
            }
            // 0x0895B0C8: integrate the two lanes and the spring derivative.
            const float ahead = f23 + f3 * kRate;
            const float lane_out = f5 * float_at(rt, frame.at(0xCu)) - float_at(rt, frame.at(8u)) * ahead;
            put_float(rt, state + 0x6CCu, lane_out);
            f6 = f6 + f3 * float_at(rt, state + 0x54u);
            put_float(rt, state + 0x6D0u,
                      ahead * float_at(rt, frame.at(0xCu)) + f5 * float_at(rt, frame.at(8u)));
            if (f6 <= 0.0f) {
                put_float(rt, state + 0x6D4u, float_at(rt, state + 0x6D4u) + f6 * kRate);
            }
            put_float(rt, state + 0x6C4u, ahead);
            put_float(rt, state + 0x6C8u, f5);
        }
    }

    // 0x0895B144: the height block of the second sample goes to the state, then the wheels.
    float height = float_at(rt, frame.at(kPoseA + 0x20u));
    if (pose_valid && load_u8(rt, state + 0xDu) != 0u) {
        put(rt, state + 0x6CCu, 0u);
        put(rt, state + 0x6D0u, 0u);
        put(rt, state + 0x6F8u, 0u);
    }
    const u32 wheels = car + 0x270u;
    put_float(rt, state + 0x6B4u, height);
    put_float(rt, state + 0x6B8u, float_at(rt, frame.at(kPoseA + 0x24u)));
    put_float(rt, state + 0x6A4u, height * float_at(rt, car + 0x68u));
    put_float(rt, state + 0x6E0u, float_at(rt, frame.at(kPoseA + 0x28u)));
    const u32 shown = load_u16(rt, frame.at(kPoseA + 0x30u));
    if (call(rt, ctx, 0x0895BBC4u, {wheels}) != 0u || call(rt, ctx, 0x0893EB40u, {wheels}) != 0u ||
        load_u8(rt, state + 0x5F9u) != 1u) {
        // 0x0895B1C0: the gear comes from the block's own speed.
        store_u16(rt, state + 0x58Eu, shown);
        call(rt, ctx, 0x08949DE0u, {object, 1u});
        const float value = float_at(rt, state + 0x594u) * kPiOver30;
        put_float(rt, state + 0x590u, value);
        store_u16(rt, state + 0x58Eu, static_cast<u32>(trunc_word(value * 60.0f / kTwoPi)));
    } else {
        // 0x0895B214: the gear comes from the lane's own table.
        const u32 slot = wheels + 4u * load_u8(rt, state + 0x5F8u);
        float value = float_at(rt, state + 0x6C4u) * float_at(rt, slot + 0x80u);
        put_float(rt, state + 0x590u, value);
        const float limit = static_cast<float>(load_u16(rt, car + 0x23Eu)) * kPiOver30;
        if (value < limit) {
            value = limit;
            put_float(rt, state + 0x590u, value);
        }
        store_u16(rt, state + 0x58Eu, static_cast<u32>(trunc_word(value * 60.0f / kTwoPi)));
        call(rt, ctx, 0x08949DE0u, {object, 0u});
    }

    // 0x0895B294: the pitch, the two vertical speeds and the four wheel records.
    call_keepf(rt, ctx, 0x089491A0u, {object}, {float_at(rt, state + 0x534u)});
    const s32 dip = sext8(load_u8(rt, frame.at(0x5Au)));
    float vertical = float_at(rt, state + 0x5ACu);
    float bob = float_at(rt, state + 0x5B0u);
    bob = bob + (((static_cast<float>(dip) * 0.015625f - vertical) * 200.0f - bob * 20.0f) * kRate);
    put_float(rt, state + 0x5B0u, bob);
    vertical = vertical + bob * kRate;
    put_float(rt, state + 0x5ACu, vertical);
    put_float(rt, frame.at(0x20u),
              static_cast<float>(sext16(load_u16(rt, frame.at(kPoseA + 0x2Cu)))) * kGravity);
    put_float(rt, frame.at(0x24u),
              static_cast<float>(sext16(load_u16(rt, frame.at(kPoseA + 0x2Eu)))) * kGravity);
    const float wheel_speed =
        float_at(rt, car + 0x68u) * float_at(rt, frame.at(kPoseA + 0x24u)) * 0.5f * 0.5f;
    for (u32 i = 0; i < 4u; ++i) {
        const u32 wheel = call(rt, ctx, 0x08955300u, {object, i});
        const u32 slot = frame.at(0x20u) + ((i >> 1) << 2);
        put(rt, wheel + 0x8Cu, 0u);
        put_float(rt, wheel + 0x80u, wheel_speed);
        put_float(rt, wheel + 0x88u, float_at(rt, slot) * 0.5f);
        if (pose_valid) store_u8(rt, wheel + 0x53u, 0u);
    }
    // 0x0895B3A4: the last loop iteration leaves the slot address in v0.
    ctx.gpr[2] = frame.at(4u);
}

} // namespace
} // namespace new_batch_main_1082
} // namespace gt::game

#define MAIN1082(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1082::name, gt::ReturnKind::kind)
MAIN1082(0x0895A2F8u, func_0895A2F8, Word);
