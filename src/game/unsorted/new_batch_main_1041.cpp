// New batch main 1041: functions that mismatched the gate in an earlier draft or were skipped twice.
// Not drafted: 0x088877DC. Its exits all leave $v0 holding an incidental value — a refcount decrement's
//     remainder or a data load's word (`lw v0` / `slt v0` between the calls) — so no structured body can
//     state $v0 as the asm leaves it; it needs a Void return contract decided first.
// 0x08A7A614 registers one long run of name/function pairs on a single object: straight-line calls from
//     tools.disasm, including the pairs of words copied out of .data and handed to the registerer by address.
// 0x08941D74 fills three 0x268-byte blocks from three 0xC0-byte source lanes (0x18 entries each), with the
//     PSP's single-precision divides and multiplies in the asm's order.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1041 {
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
// A call that returns a float in $f0.
float call_returns_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
// A call that takes its float argument in $f12 and returns one in $f0.
float call_taking_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        float f12) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {f12});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `lbu` then `cvt.s.w`: an unsigned byte widened as a signed word, then to single precision.
float byte_as_float(Runtime &rt, u32 address) { return static_cast<float>(static_cast<s32>(byte_at(rt, address))); }

// 0x08A7A614 (a0 = the object being filled): a0 first gets the name table at 0x08CF6FD4, then the member at
// +0x38 is released, then ~100 registrations follow, each (a0, name string, function) — some through a pair of
// words copied from .data to the stack and passed by address — and the frame's tail call ends the function.
void func_08A7A614(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xC0u);
    const u32 self = ctx.gpr[4];
    // 0x08B87108 (a0, name, function).
    const auto register_three = [&](u32 string, u32 function) {
        call(rt, ctx, 0x08B87108u, {self, string, function});
    };
    // 0x08B8D51C (a0, name, function, 0).
    const auto register_four = [&](u32 string, u32 function) {
        call(rt, ctx, 0x08B8D51Cu, {self, string, function, 0u});
    };
    // A pair of words read from `address`, copied to `slot` and passed by address (a3 keeps the first word).
    const auto register_pair = [&](u32 target, u32 string, u32 address, u32 slot) {
        put(rt, frame.at(slot), word(rt, address));
        put(rt, frame.at(slot + 4u), word(rt, address + 4u));
        call(rt, ctx, target, {self, string, frame.at(slot), word(rt, address)});
    };

    call(rt, ctx, 0x08A7BCC8u, {self, 0x08CF6FD4u});
    const u32 member = word(rt, self + 0x38u);
    if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
    put(rt, self + 0x38u, 0u);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A78964u});

    register_four(0x08CB0F40u, 0x08A789D8u);
    register_four(0x08CB0F48u, 0x08A78A4Cu);
    register_four(0x08CB0F54u, 0x08A78B20u);
    register_four(0x08CB0F60u, 0x08A78B84u);
    register_four(0x08CB0F6Cu, 0x08A78BE8u);

    register_three(0x08CB0F78u, 0x08A78C6Cu);
    register_three(0x08CB0F88u, 0x08A78D20u);
    register_three(0x08CB0F94u, 0x08A76B48u);
    register_three(0x08CB0F9Cu, 0x08A7758Cu);
    register_three(0x08CB0FB0u, 0x08A7788Cu);

    register_pair(0x08C27114u, 0x08CB0FC4u, 0x08C8F15Cu, 0xA8u);

    register_three(0x08CB0FDCu, 0x08A78D84u);
    register_three(0x08CB0FE4u, 0x08A78DF8u);
    register_three(0x08CB0FECu, 0x08A78E6Cu);
    register_three(0x08CB0FF4u, 0x08A78EE0u);
    register_three(0x08CB0FFCu, 0x08A78F54u);
    register_three(0x08CB1004u, 0x08A78FCCu);
    register_three(0x08CB100Cu, 0x08A79040u);
    register_three(0x08CB1018u, 0x08A790B4u);
    register_three(0x08CB1020u, 0x08A79128u);
    register_three(0x08CB1028u, 0x08A791A0u);
    register_three(0x08CB1030u, 0x08A79214u);
    register_three(0x08CB103Cu, 0x08A7928Cu);
    register_three(0x08CB1048u, 0x08A79354u);

    register_pair(0x08C27114u, 0x08CB1054u, 0x08C8F154u, 0xA0u);
    register_pair(0x08C27920u, 0x08CB1068u, 0x08C8F14Cu, 0x98u);
    register_pair(0x08C265FCu, 0x08CB1078u, 0x08C8F144u, 0x90u);
    register_pair(0x08C27C58u, 0x08CB1088u, 0x08C8F13Cu, 0x88u);
    register_pair(0x08C27ABCu, 0x08CB1098u, 0x08C8F134u, 0x80u);
    register_pair(0x08C27114u, 0x08CB10A8u, 0x08C8F12Cu, 0x78u);
    register_pair(0x08C27920u, 0x08CB10B8u, 0x08C8F124u, 0x70u);
    register_pair(0x08C27784u, 0x08CB10C4u, 0x08C8F11Cu, 0x68u);
    register_pair(0x08C275E8u, 0x08CB10D4u, 0x08C8F114u, 0x60u);
    register_pair(0x08C2744Cu, 0x08CB10E4u, 0x08C8F10Cu, 0x58u);
    register_pair(0x08C272B0u, 0x08CB10F4u, 0x08C8F104u, 0x50u);
    register_pair(0x08C27114u, 0x08CB1104u, 0x08C8F0FCu, 0x48u);

    register_three(0x08CB111Cu, 0x08A79420u);

    register_pair(0x08C27114u, 0x08CB1128u, 0x08C8F0F4u, 0x40u);
    register_four(0x08CB1140u, 0x08A794ACu);
    register_pair(0x08C27114u, 0x08CB114Cu, 0x08C8F0ECu, 0x38u);
    register_pair(0x08C27114u, 0x08CB1164u, 0x08C8F0E4u, 0x30u);
    register_three(0x08CB117Cu, 0x08A7951Cu);
    register_pair(0x08C27114u, 0x08CB1184u, 0x08C8F0DCu, 0x28u);
    register_three(0x08CB119Cu, 0x08A7972Cu);
    register_three(0x08CB11B0u, 0x08A797A0u);
    register_three(0x08CB11BCu, 0x08A77940u);
    register_pair(0x08C27114u, 0x08CB11C8u, 0x08C8F0D4u, 0x20u);

    call(rt, ctx, 0x08BEEBB4u, {self, 0x08CB11E4u, 0u, 0x08A77BE4u});

    register_pair(0x08C26F78u, 0x08CB11ECu, 0x08C8F0CCu, 0x18u);
    register_pair(0x08C26DDCu, 0x08CB11FCu, 0x08C8F0C4u, 0x10u);
    register_pair(0x08C26F78u, 0x08CB120Cu, 0x08C8F0BCu, 0x08u);
    register_pair(0x08C26DDCu, 0x08CB1218u, 0x08C8F0B4u, 0x00u);

    register_three(0x08CB1224u, 0x08A79980u);
    call(rt, ctx, 0x08C261D8u, {self, 0x08CF6FC0u, 0x08A79A0Cu});
    call(rt, ctx, 0x08C261D8u, {self, 0x08CF6F9Cu, 0x08A79A5Cu});
    tail_call(ctx, 0x08C261D8u, {self, 0x08CF6FC4u, 0x08A79AACu});
}

// 0x08941D74 (a0 = array base, a1 = 0xC0-byte source lanes, a2 = the lane count written so far): for three
// blocks of 0x268 bytes at a0 + 0x4A8 + (a2*3 + i)*0x268, fill the 0x18-entry runs from the source's bytes,
// scale them, and finish each group with the 0x08946734/0x089468C0 helpers. Returns 3.
void func_08941D74(Runtime &rt, AllegrexContext &ctx) {
    constexpr float kScaled = std::bit_cast<float>(0x3C23D70Au);      // 0.01
    constexpr float kNegFull = std::bit_cast<float>(0xC37F0000u);     // -255.0
    constexpr float kFull = std::bit_cast<float>(0x437F0000u);        // 255.0
    constexpr float kLevels = std::bit_cast<float>(0x43480000u);      // 200.0
    constexpr float kHundred = std::bit_cast<float>(0x42C80000u);     // 100.0
    constexpr float kNinety = std::bit_cast<float>(0x42B40000u);      // 90.0
    constexpr float kSeventyTwo = std::bit_cast<float>(0x42480000u);  // 72.0
    constexpr float kDegrees = std::bit_cast<float>(0x3C8EFA35u);     // pi/180
    constexpr float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);      // pi/2
    constexpr float kQuarterPi = std::bit_cast<float>(0x3F490FDBu);   // pi/4
    constexpr float kInverseLevels = std::bit_cast<float>(0x3E8E38E4u);  // 1/3.6
    constexpr float kInverseRate = std::bit_cast<float>(0x415E38E4u);    // 1000/72
    constexpr float kInverseOther = std::bit_cast<float>(0x41B1C71Du);   // 200/9
    constexpr float kGravityStep = std::bit_cast<float>(0x411CCCCDu);    // 9.8
    constexpr float kTwenty = std::bit_cast<float>(0x41A00000u);      // 20.0
    constexpr float kOne = 1.0f;
    constexpr u32 kSettings = 0x08CDE508u;
    constexpr u32 kLaneFractions = 0x08CDE613u;

    GuestFrame frame(ctx, 0x40u);
    u32 source = ctx.gpr[5];
    u32 block = ctx.gpr[4] + ctx.gpr[6] * 3u * 0x268u + 0x4A8u;

    for (u32 index = 0u; index != 3u; ++index) {
        // The header bytes and the counters of the block and its source lane.
        put_byte(rt, block + 0x250u, byte_at(rt, source + 3u));
        put(rt, block + 0x254u, 0u);
        put_byte(rt, block + 0x251u, byte_at(rt, source + 4u));
        put(rt, block + 0x258u, 0u);
        put(rt, block + 0x25Cu, 0u);
        put(rt, block + 0x260u, 0u);
        put(rt, block + 0x264u, 0u);
        const u32 header = byte_at(rt, source + 0x48u);
        const u32 count_first = byte_at(rt, source + 9u);
        put_byte(rt, source + 0x20u, 0u);
        put_byte(rt, source + 0x40u, header);
        put_byte(rt, source + 0x30u, 0u);
        put_byte(rt, source + 0x28u, 0u);
        put_byte(rt, source + 0x38u, 0u);
        const u32 count_second = byte_at(rt, source + 0xAu);
        put(rt, block + 0x164u, 0u);

        // The first run walks its slots downwards (the asm's `addiu v0, v0, -4`) and keeps the largest of the
        // +0x30 fractions, with the +0x00 one that produced it.
        if (static_cast<s32>(count_first) > 0) {
            float largest = 0.0f;
            u32 slot = block + 0x100u + count_first * 4u;
            u32 lane = source;
            for (u32 entry = 0u; entry != count_first; ++entry) {
                const float fraction = byte_as_float(rt, lane + 0x20u) / kNegFull;
                put_float(rt, slot, fraction);
                const float level = byte_as_float(rt, lane + 0x30u) / kLevels;
                put_float(rt, slot + 0x30u, level);
                if (largest < level) {
                    largest = level;
                    put_float(rt, block + 0x164u, fraction);
                }
                put_float(rt, slot + 0x78u, byte_as_float(rt, lane + 0x40u) / kLevels);
                slot -= 4u;
                lane += 1u;
            }
        }
        const float largest_first = float_at(rt, block + 0x164u);
        if (largest_first != 0.0f) {
            const float scaled = psp_mul(largest_first, kInverseRate);
            const float inverse = kOne / scaled;
            put_float(rt, block + 0x254u, inverse);
            if (byte_at(rt, block + 0x250u) == 0u) {
                put_float(rt, block + 0x254u, kOne / psp_mul(largest_first, kInverseOther));
            }
        }

        // The second run walks the same slots upwards and overwrites the fields the first one wrote.
        put_float(rt, block + 0x16Cu, kOne);
        put(rt, block + 0x168u, 0u);
        if (static_cast<s32>(count_second) > 0) {
            float largest = 0.0f;
            u32 slot = block + 0x100u + count_first * 4u;
            u32 lane = source;
            for (u32 entry = 0u; entry != count_second; ++entry) {
                const float fraction = byte_as_float(rt, lane + 0x28u) / kFull;
                put_float(rt, slot, fraction);
                const float level = byte_as_float(rt, lane + 0x38u) / kLevels;
                put_float(rt, slot + 0x30u, level);
                if (largest < level) {
                    largest = level;
                    put_float(rt, block + 0x168u, fraction);
                    put_float(rt, block + 0x16Cu, level);
                }
                put_float(rt, slot + 0x78u, byte_as_float(rt, lane + 0x48u) / kLevels);
                slot += 4u;
                lane += 1u;
            }
        }
        const float largest_second = float_at(rt, block + 0x168u);
        if (largest_second != 0.0f) {
            const float inverse = kOne / psp_mul(largest_second, kInverseRate);
            put_float(rt, block + 0x258u, inverse);
            if (byte_at(rt, block + 0x250u) == 0u) {
                put_float(rt, block + 0x258u, kOne / psp_mul(largest_second, kInverseOther));
            }
        }

        const u32 entries = count_first + count_second - 1u;
        call(rt, ctx, 0x08946734u, {block + 0xF8u, entries, block + 0x104u, block + 0x134u, 0u, 0u});
        call(rt, ctx, 0x08946734u, {block + 0x170u, entries, block + 0x104u, block + 0x17Cu, 0u, 0u});

        // The +0x94/+0x9C run, scaled by the global setting (72.0 when unset).
        const u32 setting = byte_at(rt, kSettings + 0x167u);
        float scale = kSeventyTwo;
        const u32 count_third = byte_at(rt, source + 0xFu);
        if (setting != 0u) scale = static_cast<float>(static_cast<s32>(setting));
        const float run_scale = psp_mul(scale, kInverseLevels);
        if (static_cast<s32>(count_third) > 0) {
            u32 slot = block;
            u32 lane = source;
            for (u32 entry = 0u; entry != count_third; ++entry) {
                put_float(rt, slot + 0x1C4u, psp_mul(run_scale, byte_as_float(rt, lane + 0x94u)) / kFull);
                put_float(rt, slot + 0x1E4u, byte_as_float(rt, lane + 0x9Cu) / kLevels);
                slot += 4u;
                lane += 1u;
            }
        }
        call(rt, ctx, 0x08946734u, {block + 0x1ACu, count_third, block + 0x1C4u, block + 0x1E4u, 0u, 0u});

        // The +0x10/+0x18 run, its scale in degrees clamped to pi/2.
        float heading = kNinety;
        const u32 angle = byte_at(rt, source + 2u);
        put_byte(rt, source + 0x10u, 0u);
        put_byte(rt, source + 0x18u, 0u);
        if (angle != 0u) heading = static_cast<float>(static_cast<s32>(angle));
        const float radians = psp_mul(heading, kDegrees);
        const float clamped = (kHalfPi < radians) ? kHalfPi : radians;
        const u32 count_fourth = byte_at(rt, source + 8u);
        if (static_cast<s32>(count_fourth) > 0) {
            u32 slot = block;
            u32 lane = source;
            for (u32 entry = 0u; entry != count_fourth; ++entry) {
                put_float(rt, slot + 0x78u, psp_mul(clamped, byte_as_float(rt, lane + 0x10u)) / kFull);
                put_float(rt, slot + 0x98u, byte_as_float(rt, lane + 0x18u) / kLevels);
                slot += 4u;
                lane += 1u;
            }
        }
        call(rt, ctx, 0x08946734u, {block + 0x6Cu, count_fourth, block + 0x78u, block + 0x98u, 0u, 0u});

        // The +0x50/+0x56 run; the first entry with both bytes clear also records 1/scale and the +0xC4 word.
        put_byte(rt, block + 0x253u, 0u);
        const u32 setting_low = byte_at(rt, kSettings + 0x167u);
        float low_scale = kSeventyTwo;
        put_byte(rt, source + 0x50u, 0u);
        put_byte(rt, source + 0x56u, 0u);
        const u32 count_fifth = byte_at(rt, source + 0xCu);
        if (setting_low != 0u) low_scale = static_cast<float>(static_cast<s32>(setting_low));
        const float low_run = psp_mul(low_scale, kInverseLevels);
        if (static_cast<s32>(count_fifth) > 0) {
            u32 slot = block;
            u32 lane = source;
            u32 seen = 0u;
            u32 armed = 1u;
            for (u32 entry = 0u; entry != count_fifth; ++entry) {
                put_float(rt, slot + 0xC8u, psp_mul(low_run, byte_as_float(rt, lane + 0x50u)) / kFull);
                put_float(rt, slot + 0xE0u, byte_as_float(rt, lane + 0x56u) / kLevels);
                if (armed != 0u) {
                    if (seen > 0u) {
                        if (byte_at(rt, lane + 0x55u) == 0u && byte_at(rt, lane + 0x56u) == 0u) {
                            put_float(rt, block + 0x264u, float_at(rt, slot + 0xC4u));
                            put_float(rt, block + 0x260u, kOne / low_run);
                            armed = 0u;
                        }
                    } else {
                        armed = 1u;
                    }
                }
                ++seen;
                slot += 4u;
                lane += 1u;
            }
        }
        call(rt, ctx, 0x08946734u, {block + 0xBCu, count_fifth, block + 0xC8u, block + 0xE0u, 0u, 0u});

        // A float answer clamped to pi/4, and (from it) the turn rate.
        const float answer = call_returns_float(rt, ctx, 0x08949C24u, {block});
        put_float(rt, block + 0xB8u, answer);
        if (kQuarterPi < answer) put_float(rt, block + 0xB8u, kQuarterPi);
        const float clamped_answer = float_at(rt, block + 0xB8u);
        if (0.0f < clamped_answer) {
            const float rate = kHalfPi / psp_mul(clamped_answer, kInverseRate);
            put_float(rt, block + 0x25Cu, rate);
            if (byte_at(rt, block + 0x251u) == 0u) {
                const float other = call_taking_float(rt, ctx, 0x08B49CC8u, {block}, clamped_answer);
                put_float(rt, block + 0x25Cu, kOne / psp_mul(other, kInverseOther));
            }
        }

        // The per-block fractions from the global byte table, then 24 scaled lanes.
        const u32 fraction = byte_at(rt, kLaneFractions + index);
        u32 limit = byte_at(rt, source);
        float limit_value = kHundred;
        const float fraction_scaled = psp_mul(static_cast<float>(static_cast<s32>(fraction)), kScaled);
        if (limit != 0u) limit_value = static_cast<float>(static_cast<s32>(limit));
        const float limit_scaled = psp_mul(limit_value, kScaled);
        {
            u32 slot = block;
            u32 lane = source;
            for (u32 entry = 0u; entry != 0x18u; ++entry) {
                const u32 factor = byte_at(rt, lane + 0x64u);
                if (factor == 0u) {
                    put_float(rt, slot + 8u, limit_scaled);
                } else {
                    put_float(rt, slot + 8u, psp_mul(limit_scaled, psp_mul(static_cast<float>(static_cast<s32>(factor)), kScaled)));
                }
                slot += 4u;
                lane += 1u;
            }
        }
        const float ratio = fraction_scaled / limit_scaled;
        put_float(rt, block + 4u, psp_mul(byte_as_float(rt, source + 1u), kScaled));
        put_float(rt, block + 0x68u, ratio);
        if (kOne < ratio) put_float(rt, block + 0x68u, kOne);

        // The +0x5C/+0x60 run, the +0x5C byte zeroed first.
        const u32 count_sixth = byte_at(rt, source + 0xBu);
        put_byte(rt, source + 0x5Cu, 0u);
        if (static_cast<s32>(count_sixth) > 0) {
            u32 slot = block;
            u32 lane = source;
            for (u32 entry = 0u; entry != count_sixth; ++entry) {
                put_float(rt, slot + 0x230u, psp_mul(psp_mul(byte_as_float(rt, lane + 0x5Cu), kTwenty), kGravityStep));
                put_float(rt, slot + 0x240u, byte_as_float(rt, lane + 0x60u) / kLevels);
                slot += 4u;
                lane += 1u;
            }
        }
        call(rt, ctx, 0x08946734u, {block + 0x224u, count_sixth, block + 0x230u, block + 0x240u, 0u, 0u});

        const float measured = call_taking_float(rt, ctx, 0x089468C0u, {block + 0x170u}, 0.0f);
        const float weighted = psp_mul(measured, kGravityStep);
        put_float(rt, block, weighted);
        if (weighted == 0.0f) put_float(rt, block, kGravityStep);

        block += 0x268u;
        source += 0xC0u;
    }
    ctx.gpr[2] = 3u;
}

}  // namespace
}  // namespace new_batch_main_1041
}  // namespace gt::game

#define NEW1041(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1041::name, gt::ReturnKind::kind)
NEW1041(0x08A7A614u, func_08A7A614, Word);
NEW1041(0x08941D74u, func_08941D74, Word);
