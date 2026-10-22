// Hot batch 128: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_128 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

} // namespace


// func_088E1C04(race, step): one frame of a race, under the lock race+0xF0 (released by the tail call). The 64-bit
// clock func_088D9C70(race) (now) is stored at race+0xB8; the frame's length in ms * 3 (or step[0] * 3000, or 50) goes
// to func_088D1F68(race+0x618, ...). With a session (+0x640) not yet started (+0x5B9), func_088E5454 / func_088E1808 run
// and, when func_088DDA18 opens one (+0x5B0), func_088E49EC answers +0x5BB and the deadline at (+0x130)+0x30 moves to
// now + 1000 when that is earlier; the frame ends there. Otherwise func_088DF220 / func_088E1404 / func_088DEF1C run,
// and func_088DE874 says whether the race steps: a replay-like mode (func_088D9CCC) only updates its viewers
// (+0x630, +0x5C8 slot 0x30, +0x14C); the race itself runs step+0xC physics substeps (default 1) of step[0] / n
// seconds (default 1/60), each giving the world +0x148 (slots 0x14 and 0x18), the camera +0x14C (slot 0x24, with
// func_088DC850) and the sound +0x5D0 (func_08B046B8 under the lock +0x5E8) its time, then each car (stride 0x2F8 from
// +0xC0, count func_088D9F80) is updated by func_088E8EB0 and, when active, by its driver (func_088E7FA4) and the
// world's record (slot 0x78; copied for frame 900), then the viewers, func_088E7698 and the race state
// (func_088D62FC, func_088E9B90 / func_088EA900, the restart flag +0x24). Last, the frame counter +0x38 against the
// limit min(+0x3C, +0x40) (warning 60 frames before through +0x134 slot 0xB0, time-out slot 0xAC with +0x45), the
// replay length +0x48 / +0x4C (slot 0xB4 with +0x50, func_088F9350), func_088DFF0C(race, -, now, substeps) and
// func_088DE348 for a session.
void func_088E1C04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t race = ctx.gpr[4];
    const std::uint32_t step = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto vcall = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
        return call_set(rt, ctx, word(rt, word(rt, object) + slot), arguments);
    };
    if (word(rt, race + 0x154u) != 0u) (void)call_set(rt, ctx, 0x088F4BE0u, {word(rt, race + 0x154u)});
    const GuestResult clock = call_setf(rt, ctx, 0x088D9C70u, {race}, {});
    const std::uint32_t now_lo = clock.v0, now_hi = clock.v1;
    std::uint32_t substeps = 1u;
    std::uint32_t delta = 0x32u;
    if (step != 0u) {
        delta = ctx.fpu_float_to_word(psp_mul(f(step), std::bit_cast<float>(0x453B8000u)), 1u);
        substeps = word(rt, step + 0xCu);
    }
    if (word(rt, race + 0x640u) != 0u) delta = (now_lo - word(rt, race + 0xB8u)) * 3u;
    const std::uint32_t lock = race + 0xF0u;
    put(rt, race + 0xB8u, now_lo);
    put(rt, race + 0xBCu, now_hi);
    (void)call_set(rt, ctx, kLock, {lock});
    const auto session = [&] { return word(rt, race + 0x640u); };
    bool stepped = false;  // the race ran its physics substeps this frame (sp+8)
    bool tail = false;
    if (session() != 0u && byte_u(rt, race + 0x5B9u) == 0u) {
        (void)call_set(rt, ctx, 0x088E5454u, {session(), word(rt, race + 0x130u), word(rt, race + 0x30u)});
        (void)call_set(rt, ctx, 0x088E1808u, {race});
        if (byte_u(rt, race + 0x5BBu) == 0u && byte_u(rt, race + 0x5B5u) != 0u) {
            const std::uint32_t opened = call_set(rt, ctx, 0x088DDA18u, {race});
            put(rt, race + 0x5B0u, opened);
            if (opened != 0u) {
                put8(rt, race + 0x5BCu, 0u);
                put8(rt, race + 0x5BBu, call_set(rt, ctx, 0x088E49ECu, {session(), opened}));
                const std::uint32_t deadline = word(rt, race + 0x130u);
                const std::int64_t due = static_cast<std::int64_t>((static_cast<std::uint64_t>(word(rt, deadline + 0x34u)) << 32) | word(rt, deadline + 0x30u));
                if (due > 0) {
                    const std::uint64_t soon = ((static_cast<std::uint64_t>(now_hi) << 32) | now_lo) + 1000u;
                    if (static_cast<std::int64_t>(soon) < due) {
                        put(rt, deadline + 0x30u, static_cast<std::uint32_t>(soon));
                        put(rt, deadline + 0x34u, static_cast<std::uint32_t>(soon >> 32));
                    }
                }
                tail = true;
            }
        }
    }
    if (!tail) {
        (void)call_set(rt, ctx, 0x088DF220u, {race});
        (void)call_set(rt, ctx, 0x088E1404u, {race});
        (void)call_set(rt, ctx, 0x088DEF1Cu, {race});
        tail = call_set(rt, ctx, 0x088DE874u, {race}) == 0u;
    }
    if (!tail) {
        if (byte_u(rt, race + 0x22u) != 0u || (word(rt, race + 0x30u) == 2u && byte_u(rt, race + 0x1Fu) != 0u))
            (void)call_set(rt, ctx, 0x088D776Cu, {race, 0u});
        const std::uint32_t viewer = word(rt, race + 0x630u);
        if (viewer != 0u && call_set(rt, ctx, 0x088D9CC4u, {race}) == 0u) (void)call_set(rt, ctx, 0x088E7BECu, {viewer});
        if (call_set(rt, ctx, 0x088D9CCCu, {race}) != 0u) {
            std::uint32_t watching = 0u;
            if (word(rt, race + 0x630u) != 0u) watching = call_set(rt, ctx, 0x088E77B4u, {word(rt, race + 0x630u)});
            bool camera = true;
            if (call_set(rt, ctx, 0x088D9CC4u, {race}) != 0u) {
                (void)call_set(rt, ctx, 0x088D9DD0u, {race, step});
            } else if (watching != 0u && word(rt, race + 0x5C8u) != 0u) {
                if (call_set(rt, ctx, 0x08917918u, {word(rt, race + 0x5C8u), 1u}) == 7u) {
                    camera = false;
                } else if (call_set(rt, ctx, 0x08917918u, {word(rt, race + 0x5C8u), 1u}) == 6u) {
                    camera = false;
                } else {
                    (void)vcall(word(rt, race + 0x5C8u), 0x30u, {word(rt, race + 0x5C8u), race, step});
                }
            }
            if (camera && word(rt, race + 0x14Cu) != 0u) (void)call_set(rt, ctx, 0x088F9350u, {word(rt, race + 0x14Cu)});
            tail = true;
        }
    }
    if (!tail) {
        (void)call_set(rt, ctx, 0x088E1310u, {race});
        (void)call_set(rt, ctx, 0x088D1F68u, {word(rt, race + 0x618u), delta});
        bool substep = true;
        if (session() != 0u) substep = call_set(rt, ctx, 0x088DA0BCu, {race}) != 0u;
        if (substep) {
            float seconds = std::bit_cast<float>(0x3C888889u);
            if (step != 0u) seconds = f(step) / static_cast<float>(signed_of(substeps));
            for (std::int32_t k = 0; k < signed_of(substeps); ++k) {
                const std::int32_t ms = signed_of(ctx.fpu_float_to_word(psp_mul(psp_mul(seconds, static_cast<float>(k)), 1000.0f), 1u));
                const std::uint64_t at = ((static_cast<std::uint64_t>(now_hi) << 32) | now_lo) + static_cast<std::uint64_t>(static_cast<std::int64_t>(ms));
                const std::uint32_t at_lo = static_cast<std::uint32_t>(at), at_hi = static_cast<std::uint32_t>(at >> 32);
                const std::uint32_t world = word(rt, race + 0x148u);
                const std::uint32_t world_table = word(rt, world);
                (void)call_set(rt, ctx, word(rt, world_table + 0x14u), {world, world_table, at_lo, at_hi});
                const std::uint32_t camera = word(rt, race + 0x14Cu);
                ctx.gpr[8] = 0x088DC850u;
                ctx.gpr[9] = race;
                if (camera != 0u) (void)vcall(camera, 0x24u, {camera, ctx.gpr[5], at_lo, at_hi, 0x088DC850u, race});
                const std::uint32_t world2 = word(rt, race + 0x148u);
                (void)call_setf(rt, ctx, word(rt, word(rt, world2) + 0x18u), {world2, ctx.gpr[5], at_lo, at_hi}, {seconds});
                if (word(rt, race + 0x5D0u) != 0u) {
                    (void)call_set(rt, ctx, kLock, {race + 0x5E8u});
                    (void)call_setf(rt, ctx, 0x08B046B8u, {word(rt, race + 0x5D0u)}, {seconds});
                    (void)call_set(rt, ctx, kUnlock, {race + 0x5E8u});
                }
            }
            stepped = true;
        }
        std::uint32_t record = 0u;  // sp+0, +0xD0 per car
        const std::uint32_t context = call_set(rt, ctx, 0x088FE518u, {word(rt, race + 0x14u)});
        for (std::uint32_t i = 0u; signed_of(i) < signed_of(call_set(rt, ctx, 0x088D9F80u, {race})); ++i, record += 0xD0u) {
            const std::uint32_t car = word(rt, race + 0xC0u) + i * 0x2F8u;
            (void)call_set(rt, ctx, 0x088E8EB0u, {car, word(rt, race + 0x618u), context, race, i});
            if (word(rt, car + 0xCu) == 0u || word(rt, car + 4u) != 1u || call_set(rt, ctx, 0x088E85E8u, {car}) == 0u) continue;
            if (word(rt, car + 0x1Cu) == 0u) {
                const std::uint32_t driver = word(rt, car + 0xCu);
                if (driver != 0u) {
                    const std::uint32_t a = vcall(driver, 0x38u, {driver});
                    const std::uint32_t b = vcall(driver, 0x3Cu, {driver});
                    (void)call_set(rt, ctx, 0x088E7FA4u, {car, a, b});
                }
            }
            if (session() == 0u) continue;
            const std::uint32_t world = word(rt, race + 0x148u);
            const std::uint32_t entry = word(rt, race + 0x644u) + record;
            if (vcall(world, 0x78u, {world, i, entry}) == 0u || word(rt, race + 0x38u) != 0x384u) continue;
            for (std::uint32_t k = 0u; k != 0x18u; k += 4u) put(rt, entry + 0x18u + k, word(rt, entry + k));
            put(rt, entry + 0x30u, call_set(rt, ctx, 0x088DAAF4u, {race}));
        }
        if (word(rt, race + 0x14Cu) != 0u) (void)vcall(word(rt, race + 0x14Cu), 0x28u, {word(rt, race + 0x14Cu), step});
        if (word(rt, race + 0x5CCu) != 0u) (void)vcall(word(rt, race + 0x5CCu), 0x24u, {word(rt, race + 0x5CCu)});
        if (word(rt, race + 0x628u) != 0u) (void)call_set(rt, ctx, 0x088D00B0u, {word(rt, race + 0x628u), word(rt, race + 0x618u)});
        if (word(rt, race + 0x630u) != 0u) (void)call_set(rt, ctx, 0x088E77ACu, {word(rt, race + 0x630u), word(rt, race + 0x618u)});
        if (word(rt, race + 0x5C8u) != 0u && stepped) {
            if (call_set(rt, ctx, 0x088D9CCCu, {race}) == 0u || byte_u(rt, race + 0x44u) != 0u)
                (void)vcall(word(rt, race + 0x5C8u), 0x30u, {word(rt, race + 0x5C8u), race, step});
        }
        if (word(rt, race + 0x594u) != 0u) (void)call_set(rt, ctx, 0x088E7698u, {word(rt, race + 0x594u), race});
        if (call_set(rt, ctx, 0x088DA0BCu, {race}) != 0u) {
            const std::uint32_t state = race + 4u;
            if (call_set(rt, ctx, 0x088D62FCu, {state}) != 0u && call_set(rt, ctx, 0x088D62FCu, {state}) != 4u &&
                word(rt, race + 0x30u) == 2u) {
                (void)call_set(rt, ctx, 0x088EA900u, {race});
            } else {
                (void)call_set(rt, ctx, 0x088E9B90u, {race, ctx.gpr[5], now_lo, now_hi});
            }
        }
        if (byte_u(rt, race + 0x24u) != 0u) {
            (void)call_set(rt, ctx, 0x088D78D8u, {race, 0u});
            put8(rt, race + 0x24u, 0u);
        }
    }
    // the frame counter and the replay length
    if (call_set(rt, ctx, 0x088DA0BCu, {race}) != 0u &&
        (call_set(rt, ctx, 0x088D9CCCu, {race}) == 0u || byte_u(rt, race + 0x44u) != 0u)) {
        const std::uint32_t frames = word(rt, race + 0x38u) + 1u;
        put(rt, race + 0x38u, frames);
        std::uint32_t limit = word(rt, race + 0x3Cu);
        if (limit == 0u) {
            limit = word(rt, race + 0x40u);
        } else if (word(rt, race + 0x40u) != 0u && word(rt, race + 0x40u) < limit) {
            limit = word(rt, race + 0x40u);
        }
        if (byte_u(rt, race + 0x45u) == 0u && limit != 0u) {
            const std::uint32_t warning = limit - 0x3Cu;
            if (limit < frames) {
                if (word(rt, race + 0x134u) != 0u) (void)vcall(word(rt, race + 0x134u), 0xACu, {word(rt, race + 0x134u)});
                put8(rt, race + 0x45u, 1u);
            } else if (frames == warning && word(rt, race + 0x134u) != 0u) {
                (void)vcall(word(rt, race + 0x134u), 0xB0u, {word(rt, race + 0x134u)});
            }
        }
        if (word(rt, race + 0x4Cu) != 0u) {
            if (word(rt, race + 0x14Cu) != 0u) put(rt, race + 0x48u, call_set(rt, ctx, 0x088F8E10u, {word(rt, race + 0x14Cu)}));
            if (word(rt, race + 0x4Cu) < word(rt, race + 0x48u) && byte_u(rt, race + 0x50u) == 0u) {
                (void)vcall(word(rt, race + 0x134u), 0xB4u, {word(rt, race + 0x134u)});
                put8(rt, race + 0x50u, 1u);
                if (word(rt, race + 0x14Cu) != 0u) (void)call_set(rt, ctx, 0x088F9350u, {word(rt, race + 0x14Cu)});
            }
        }
    }
    (void)call_set(rt, ctx, 0x088DFF0Cu, {race, ctx.gpr[5], now_lo, now_hi, substeps});
    if (byte_u(rt, race + 0x5BCu) == 0u && session() != 0u) (void)call_set(rt, ctx, 0x088DE348u, {race});
    tail_call(ctx, kUnlock, {lock});
}

} // namespace hot_batch_128
} // namespace gt::game

#define HOT128(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_128::name, gt::ReturnKind::kind)
HOT128(0x088E1C04u, func_088E1C04, Word);
