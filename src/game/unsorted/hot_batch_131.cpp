// Hot batch 131: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_131 {
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

// A script class's method table: func_08B86FB0(*holder, name, method) for each pair, *holder read again each time (an
// empty one first reported through func_08A5BB7C(report)); with `tail` the last registration is the function's tail jump.
inline void register_methods(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder,
                             std::uint32_t report, const std::uint32_t (*methods)[2], std::size_t count, bool tail) {
    for (std::size_t index = 0u; index < count; ++index) {
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {report});
        if (tail && index + 1u == count) return tail_call(ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
        (void)call_set(rt, ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
    }
}

} // namespace

// func_088E9B90(race, -, time_lo, time_hi): the race's per-frame notices. Nothing without a world (+0x148). The cars are
// visited in the order of the halfword keys at race->+0x138 (count func_088DA160(race); car = race->+0xC0 + key * 0x2F8);
// a car with any of the bytes +0x55, +0x57, +0x56 set is skipped by every pass. Each pass collects keys into the
// halfwords at race->+0x13C and hands them to the listener race->+0x134 (when there is one and the pass collected any)
// through a virtual slot with (listener, race + 4, keys, count): +0x8B set -> slot 0x60; the flag byte at race->+0x618
// set, +0x1C clear, +0x88 set and +0x89 clear -> slot 0x64; +0x1C clear and +0x8A set -> slot 0x68; +0x64 set, +0x5C
// positive and +0x1FC >= -1 -> slot 0x5C, and with a session (+0x640) in a mode with race->+0x27 set each of those
// without +0x1C gets a record from func_088E4C08 (filled from the car, its speed via its +0xC slot 0x1C times 3.6,
// sealed by func_088E4BC8). The state pass (+0x54 set) checks lap and finish rules against func_088D6358 /
// func_088DB2E4 / func_088D4FA8, the two modes A = func_08B702B4(func_088DAAF4(race), 60000) and B =
// func_088D63D4(race + 4), func_088D5D98 / func_088D5D88, marks finishers with func_088E8518(car, 1) and reports through
// slots 0x44, 0xBC, 0x74 (with func_088E77D0 for +0x630) and the world's slot 0x48; then slots 0x50 (and records again
// for a session, with +5 set), 0x44, the car states +0x84 1 / 3 / 5-6 (func_088E8A44 with 3 / 4 / 7, slots 0x9C / 0xA0 /
// 0xA4), the mode's closing report (slots 0xA8 / 0x6C) and last the cars whose func_088E855C(car, -, time) reaches
// func_088D5D34(race + 4) * 60000 (func_088E8518(car, 0); a tail jump to slot 0x6C with (.., 0, 1)).
void func_088E9B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t race = ctx.gpr[4];
    const std::uint32_t time_lo = ctx.gpr[6];
    const std::uint32_t time_hi = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    if (word(rt, race + 0x148u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t rules = race + 4u;
    const auto key_at = [&](std::uint32_t index) { return half_u(rt, word(rt, race + 0x138u) + index * 2u); };
    const auto car_of = [&](std::uint32_t key) { return word(rt, race + 0xC0u) + key * 0x2F8u; };
    const auto keys = [&]() { return word(rt, race + 0x13Cu); };
    const auto listener = [&]() { return word(rt, race + 0x134u); };
    const auto skipped = [&](std::uint32_t car) {
        return byte_u(rt, car + 0x55u) != 0u || byte_u(rt, car + 0x57u) != 0u || byte_u(rt, car + 0x56u) != 0u;
    };
    const auto notify = [&](std::uint32_t slot, std::uint32_t collected) {
        const std::uint32_t object = listener();
        if (object != 0u && collected != 0u) (void)call_virtual_at(rt, ctx, object, slot, {object, rules, keys(), collected});
    };
    // A record per collected car for the session +0x640 (`started`: the byte +5).
    const auto record_cars = [&](std::uint32_t collected, bool started) {
        for (std::uint32_t index = 0u; index != collected; ++index) {
            const std::uint32_t key = half_u(rt, keys() + index * 2u);
            const std::uint32_t car = car_of(key);
            if (word(rt, car + 0x1Cu) != 0u) continue;
            const std::uint32_t state = car + 0x30u;
            const std::uint32_t record = call_set(rt, ctx, 0x088E4C08u, {word(rt, race + 0x640u), key});
            const std::uint32_t slot = word(rt, state + 0x30u);
            put16(rt, record, key);
            put16(rt, record + 2u, word(rt, state + 0x10u));
            put8(rt, record + 4u, slot);
            put8(rt, record + 5u, started ? 1u : 0u);
            put(rt, record + slot * 4u + 0x10u, call_set(rt, ctx, 0x088D09A8u, {state, slot}));
            const std::uint32_t body = word(rt, word(rt, race + 0xC0u) + key * 0x2F8u + 0xCu);
            (void)call_virtual_at(rt, ctx, body, 0x1Cu, {body});
            const std::uint32_t speed = ctx.fpu_float_to_word(psp_mul(ctx.fpr[0], std::bit_cast<float>(0x40666666u)), 1u);
            if (started) {
                put16(rt, record + 0xCu, 0u);
                put16(rt, record + 0xEu, 0u);
            }
            put16(rt, record + slot * 2u + 0x50u, speed);
            if (started) {
                put(rt, record + 8u, word(rt, state + 0x78u));
                put(rt, record + 0x70u, word(rt, state + 0x70u));
            } else {
                put(rt, record + 0x70u, word(rt, state + 0x140u));
            }
            put(rt, record + 0x78u, word(rt, race + 0x80u));
            put(rt, record + 0x7Cu, word(rt, race + 0x84u));
            (void)call_set(rt, ctx, 0x088E4BC8u, {word(rt, race + 0x640u), key});
        }
    };

    if (call_set(rt, ctx, 0x088D4D9Cu, {rules}) == 0u) (void)call_set(rt, ctx, 0x088E9A6Cu, {race});
    const std::uint32_t count = call_set(rt, ctx, 0x088DA160u, {race});
    const bool any = signed_of(count) > 0;
    (void)call_set(rt, ctx, 0x088E9614u, {race});
    const GuestResult clock = call_keep(rt, ctx, 0x088DAAF4u, {race}, {});
    ctx.gpr[4] = clock.v0;
    ctx.gpr[5] = clock.v1;
    const std::uint32_t mode_a = call_set(rt, ctx, 0x08B702B4u, {clock.v0, clock.v1, 0xEA60u, 0u});
    const std::uint32_t mode_b = call_set(rt, ctx, 0x088D63D4u, {rules});

    // passes 1-2
    std::uint32_t collected = 0u;
    for (std::uint32_t index = 0u; any && index != count; ++index) {
        const std::uint32_t key = key_at(index);
        const std::uint32_t car = car_of(key);
        if (skipped(car) || byte_u(rt, car + 0x8Bu) == 0u) continue;
        put16(rt, keys() + collected * 2u, key);
        ++collected;
    }
    notify(0x60u, collected);
    collected = 0u;
    for (std::uint32_t index = 0u; any && index != count; ++index) {
        const std::uint32_t key = key_at(index);
        const std::uint32_t car = car_of(key);
        if (skipped(car) || byte_u(rt, word(rt, race + 0x618u)) == 0u) continue;
        if (word(rt, car + 0x1Cu) != 0u || byte_u(rt, car + 0x88u) == 0u || byte_u(rt, car + 0x89u) != 0u) continue;
        put16(rt, keys() + collected * 2u, key);
        ++collected;
    }
    notify(0x64u, collected);
    // passes 3-4 (only with cars)
    if (any) {
        collected = 0u;
        for (std::uint32_t index = 0u; index != count; ++index) {
            const std::uint32_t key = key_at(index);
            const std::uint32_t car = car_of(key);
            if (skipped(car) || word(rt, car + 0x1Cu) != 0u || byte_u(rt, car + 0x8Au) == 0u) continue;
            put16(rt, keys() + collected * 2u, key);
            ++collected;
        }
        notify(0x68u, collected);
        collected = 0u;
        for (std::uint32_t index = 0u; index != count; ++index) {
            const std::uint32_t key = key_at(index);
            const std::uint32_t car = car_of(key);
            if (skipped(car) || byte_u(rt, car + 0x64u) == 0u || signed_of(word(rt, car + 0x5Cu)) <= 0) continue;
            if (signed_of(word(rt, car + 0x1FCu)) < -1) continue;
            put16(rt, keys() + collected * 2u, key);
            ++collected;
        }
        if (collected != 0u) {
            notify(0x5Cu, collected);
            if (word(rt, race + 0x640u) != 0u && byte_u(rt, race + 0x27u) != 0u) record_cars(collected, false);
        }
    }

    // the state pass
    const std::uint32_t laps_a = call_set(rt, ctx, 0x088D5D98u, {rules});
    const std::uint32_t laps_b = call_set(rt, ctx, 0x088D5D88u, {rules});
    std::uint32_t states = 0u;        // sp+0x0
    std::uint32_t passed = 0u;        // sp+0x10
    std::uint32_t first_passed = ~0u; // sp+0x14
    std::uint32_t first_done = ~0u;   // sp+0x20
    std::uint32_t done = 0u;          // sp+0x24
    for (std::uint32_t index = 0u; any && index != count; ++index) {
        const std::uint32_t key = key_at(index);
        const std::uint32_t car = car_of(key);
        if (skipped(car)) continue;
        const std::uint32_t state = car + 0x30u;
        std::uint32_t next = states;
        if (byte_u(rt, state + 0x24u) != 0u) {
            put16(rt, keys() + states * 2u, key);
            next = states + 1u;
            if ((call_set(rt, ctx, 0x088D6358u, {rules}) == 1u || call_set(rt, ctx, 0x088D6358u, {rules}) == 6u) &&
                word(rt, state + 0x14u) == 1u) {
                if (signed_of(call_set(rt, ctx, 0x088DB2E4u, {race})) < 0 || signed_of(call_set(rt, ctx, 0x088D4FA8u, {rules})) < 0) {
                    if (first_passed == ~0u) first_passed = states;
                    ++passed;
                } else if (call_set(rt, ctx, 0x088DB2E4u, {race}) == key && listener() != 0u) {
                    const std::uint32_t object = listener();
                    (void)call_virtual_at(rt, ctx, object, 0x44u, {object, rules, word(rt, race + 0x138u), count});
                }
            }
            bool finished = false;
            if (mode_b == 1u) {
                finished = laps_b != 0u && signed_of(mode_a) >= signed_of(laps_b);
            } else if (mode_b == 0u && laps_a != 0u) {
                finished = word(rt, state + 0x14u) == laps_a + 1u;
            }
            if (finished) {
                if (first_done == ~0u) first_done = states;
                ++done;
                (void)call_set(rt, ctx, 0x088E8518u, {car, 1u, done});
            }
            if (listener() != 0u && call_set(rt, ctx, 0x088D6310u, {rules}) == 1u) {
                if (call_set(rt, ctx, 0x088D4D9Cu, {rules}) == 0u) {
                    const std::uint32_t object = listener();
                    (void)call_virtual_at(rt, ctx, object, 0xBCu, {object, key, key});
                } else {
                    const std::uint32_t target = word(rt, car + 0x1FCu);
                    if (signed_of(target) >= 0) {
                        const std::uint32_t object = listener();
                        (void)call_virtual_at(rt, ctx, object, 0xBCu, {object, key, target});
                        const std::uint32_t world = word(rt, race + 0x148u);
                        (void)call_virtual_at(rt, ctx, world, 0x48u, {world, target});
                    } else if (signed_of(target) < -1) {
                        const std::uint32_t object = listener();
                        (void)call_virtual_at(rt, ctx, object, 0xBCu, {object, key, 0xFFFFFFFFu});
                    }
                }
            }
        }
        if (call_set(rt, ctx, 0x088DAA84u, {race}) != 0u && listener() != 0u &&
            word(rt, state + 0x44u) != word(rt, state + 0x40u)) {
            const std::uint32_t viewer = word(rt, race + 0x630u);
            const std::uint32_t behind = word(rt, car + 0x70u);
            const std::uint32_t ahead = word(rt, car + 0x74u);
            if (viewer != 0u && signed_of(behind) < signed_of(ahead))
                (void)call_set(rt, ctx, 0x088E77D0u, {viewer, key, key_at(ahead), ahead, behind});
            const std::uint32_t reporter = listener();
            (void)call_virtual_at(rt, ctx, reporter, 0x74u, {reporter, rules, key, word(rt, car + 0x74u), word(rt, car + 0x70u)});
        }
        states = next;
    }
    if (listener() != 0u) {
        if (states != 0u) {
            notify(0x50u, states);
            if (word(rt, race + 0x640u) != 0u && byte_u(rt, race + 0x27u) != 0u) record_cars(states, true);
        }
        if (passed != 0u) {
            const std::uint32_t object = listener();
            (void)call_virtual_at(rt, ctx, object, 0x44u, {object, rules, keys() + first_passed * 2u, passed});
        }
        // the car states
        for (std::uint32_t index = 0u; any && index != count; ++index) {
            const std::uint32_t key = key_at(index);
            const std::uint32_t car = car_of(key);
            const std::uint32_t status = word(rt, car + 0x84u);
            std::uint32_t slot = 0u;
            if (status == 1u) {
                (void)call_set(rt, ctx, 0x088E8A44u, {car, 3u});
                slot = 0x9Cu;
            } else if (status == 3u) {
                (void)call_set(rt, ctx, 0x088E8A44u, {car, 4u});
                slot = 0xA0u;
            } else if (status - 5u < 2u && byte_u(rt, car + 0x57u) == 0u) {
                (void)call_set(rt, ctx, 0x088E8A44u, {car, 7u});
                slot = 0xA4u;
            }
            if (slot == 0u) continue;
            const std::uint32_t object = listener();
            (void)call_virtual_at(rt, ctx, object, slot, {object, key});
        }
    }
    // the mode's closing report
    if (mode_b == 0u) {
        if (done != 0u) {
            const std::uint32_t object = listener();
            (void)call_virtual_at(rt, ctx, object, 0xA8u, {object, rules, keys() + first_done * 2u, done, 0u, 1u});
        }
    } else if (mode_b == 1u) {
        if (laps_b != 0u && listener() != 0u && signed_of(mode_a) >= signed_of(laps_b)) {
            std::uint32_t first = done != 0u ? first_done : 0u;
            if (call_set(rt, ctx, 0x088D4C34u, {rules}) != 0u) {
                done = 0u;
                for (std::uint32_t index = 0u; any && index != count; ++index) {
                    const std::uint32_t key = key_at(index);
                    const std::uint32_t car = car_of(key);
                    const std::uint32_t at = done * 2u;
                    if (byte_u(rt, car + 0x55u) != 0u) continue;
                    put16(rt, keys() + at, key);
                    ++done;
                    (void)call_set(rt, ctx, 0x088E8518u, {car, 0u, key, at});
                }
                first = 0u;
            }
            if (done != 0u) {
                const std::uint32_t object = listener();
                (void)call_virtual_at(rt, ctx, object, 0x6Cu, {object, rules, keys() + first * 2u, done, 0u, 1u});
            }
        }
    } else if (mode_b == 3u) {
        const std::uint32_t object = listener();
        if (object != 0u)
            (void)call_virtual_at(rt, ctx, object, 0xA8u, {object, rules, word(rt, race + 0x138u), count, 0u, 1u});
    }
    // the cars past the time limit
    const std::uint32_t limit_minutes = call_set(rt, ctx, 0x088D5D34u, {rules});
    if (signed_of(limit_minutes) < 0) return;
    const std::int64_t limit = static_cast<std::int64_t>(signed_of(limit_minutes)) * 60000;
    ctx.gpr[2] = 0xEA60u;
    ctx.gpr[5] = 0u;
    collected = 0u;
    for (std::uint32_t index = 0u; any && index != count; ++index) {
        const std::uint32_t key = key_at(index);
        const std::uint32_t car = car_of(key);
        if (byte_u(rt, car + 0x55u) != 0u) {
            ctx.gpr[2] = byte_u(rt, car + 0x55u);
            continue;
        }
        const GuestResult used = call_keep(rt, ctx, 0x088E855Cu, {car, ctx.gpr[5], time_lo, time_hi}, {});
        const std::uint64_t taken = (static_cast<std::uint64_t>(used.v1) << 32) | used.v0;
        const std::int64_t left = static_cast<std::int64_t>(static_cast<std::uint64_t>(limit) - taken);
        if (static_cast<std::int32_t>(static_cast<std::uint64_t>(left) >> 32) <= 0) ctx.gpr[5] = 0u;
        if (left > 0) continue;
        put16(rt, keys() + collected * 2u, key);
        ++collected;
        (void)call_set(rt, ctx, 0x088E8518u, {car, 0u, static_cast<std::uint32_t>(left),
                                              static_cast<std::uint32_t>(static_cast<std::uint64_t>(left) >> 32)});
    }
    const std::uint32_t object = listener();
    if (object == 0u || collected == 0u) return;
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + 0x6Cu);
    tail_call(ctx, ctx.gpr[25], {object, rules, keys(), collected, 0u, 1u});
}

// func_08B00770(body, world, context, all) -> 1 when a contact was made: the contact pass of body (flags +0xE2 bit 0x2000,
// else 0) against the shapes of world->+0x38 (a vector of pointers; from 1 when flag bit 0 or 0x200 is set, else from 0).
// With `all` clear any group, else body->+0xD0 (none when negative -> 0). Each shape whose slot 0x10(shape, body + 0x20,
// group; f12 = (body->+0xD8)->+0x20) answers goes into a bit mask; for those func_08B01804(world, context, -(first + 1) -
// i) opens a contact set, its +0 bit 0 cleared and func_08B01154(set, world) run. Then for each of the
// (body->+0xD8)->+0x7E parts: func_08B0066C / func_08B00764 fill two tables of 16-byte points (frame +0x120, +0x200);
// per shape and point, the shape's slot 0xC(shape, out, a, b, group) finds a hit (>= 0), func_08BAC270(b, body + 0x20)
// a 64-bit key copied by func_08BAC428, and func_08B01D1C (11 arguments, the rest on the stack) a contact whose normal
// (+0x30, negated func_08C46364), midpoint (+0x24, +0x28: the part's +0x68 / +0x64 plus the hit, halved) and depth (+0x2C)
// are written; the set is marked (+0 bit 0). The set's existing contacts (a chain at world->+0x60, 0x54 bytes each,
// linked through +0x52) of this part whose point was not hit this time are tested again the same way and refreshed
// (+0x51 = 1). Last, func_08B012D8(set, world) for every marked set.
void func_08B00770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t body = ctx.gpr[4];
    const std::uint32_t world = ctx.gpr[5];
    const std::uint32_t context = ctx.gpr[6];
    const std::uint32_t all = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x380u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const std::uint32_t flags = half_u(rt, body + 0xE2u);
    ctx.gpr[2] = 0u;
    if ((flags & 0x2000u) == 0u) return;
    const std::uint32_t shapes = world + 0x38u;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, shapes + 4u) - word(rt, shapes)) >> 2);
    const std::uint32_t first = ((flags & 1u) != 0u || (flags & 0x200u) != 0u) ? 1u : 0u;
    std::uint32_t group = 0xFFFFFFFFu;
    if (all != 0u) {
        group = word(rt, body + 0xD0u);
        if (signed_of(group) < 0) return;
    }
    if (!(signed_of(first) < signed_of(count))) return;
    const std::uint32_t center = body + 0x20u;
    const std::uint32_t sets = sp(0xA0u);  // a contact set per shape
    std::uint32_t hits = 0u;
    for (std::uint32_t index = first; index != count; ++index) {
        const std::uint32_t shape = word(rt, word(rt, shapes) + index * 4u);
        const std::uint32_t method = word(rt, word(rt, shape) + 0x10u);
        if (call_setf(rt, ctx, method, {shape, center, group}, {float_at(rt, word(rt, body + 0xD8u) + 0x20u)}).v0 != 0u)
            hits |= 1u << index;
    }
    if (hits == 0u) return;
    const std::uint32_t base_key = 0u - first - 1u;
    for (std::uint32_t index = first; index != count; ++index) {
        if ((hits & (1u << index)) == 0u) continue;
        const std::uint32_t set = call_set(rt, ctx, 0x08B01804u, {world, context, base_key - (index - first)});
        put(rt, sets + index * 4u, set);
        if (set == 0u) continue;
        put16(rt, set, half_u(rt, set) & ~1u);
        (void)call_set(rt, ctx, 0x08B01154u, {set, world});
    }
    std::uint32_t made = 0u;
    const std::uint32_t parts = half_u(rt, word(rt, body + 0xD8u) + 0x7Eu);
    for (std::uint32_t part = 0u; signed_of(parts) > 0 && part != parts; ++part) {
        (void)call_set(rt, ctx, 0x08B0066Cu, {body, sp(0x120u), part, body + 0x10u, body + 0x2Cu});
        const std::uint32_t points = call_set(rt, ctx, 0x08B00764u, {body, sp(0x200u), part});
        std::uint32_t key = base_key;
        for (std::uint32_t index = first; index != count; ++index, --key) {
            if ((hits & (1u << index)) == 0u) continue;
            const std::uint32_t slot = sets + index * 4u;
            const std::uint32_t shape = word(rt, word(rt, shapes) + index * 4u);
            std::uint32_t chain = 0xFFFFFFFFu;
            if (word(rt, slot) != 0u) chain = sext16(half_u(rt, word(rt, slot) + 4u));
            std::uint32_t touched = 0u;
            const auto vcall_hit = [&](std::uint32_t out, std::uint32_t a, std::uint32_t b) {
                const std::uint32_t method = word(rt, word(rt, shape) + 0xCu);
                return signed_of(call_set(rt, ctx, method, {shape, out, a, b, group}));
            };
            for (std::uint32_t point = 0u; signed_of(points) > 0 && point != points; ++point) {
                const std::uint32_t a = call_set(rt, ctx, 0x08C45D94u, {sp(0x120u) + point * 16u});
                const std::uint32_t b = call_set(rt, ctx, 0x08C45D94u, {sp(0x200u) + point * 16u});
                if (vcall_hit(sp(0x60u), a, b) < 0) continue;
                const GuestResult hash = call_keep(rt, ctx, 0x08BAC270u, {b, center}, {});
                put(rt, sp(0x2E4u), hash.v1);
                put(rt, sp(0x2E0u), hash.v0);
                put(rt, sp(0x20u), hash.v0);
                (void)call_set(rt, ctx, 0x08BAC428u, {sp(0x34u), hash.v0, hash.v1});
                put(rt, sp(0x28u), word(rt, sp(0x60u)));
                put(rt, sp(0x2Cu), word(rt, sp(0x64u)));
                put(rt, sp(0x30u), word(rt, sp(0x68u)));
                put(rt, sp(0x24u), hash.v1);
                put(rt, sp(0xCu), sp(0x28u));
                put(rt, sp(0x14u), sp(0x60u));
                put(rt, sp(0x0u), 0u);
                put(rt, sp(0x4u), 0u);
                put(rt, sp(0x8u), b);
                const std::uint32_t contact = call_set(rt, ctx, 0x08B01D1Cu, {world, slot, 0u, context, part, point, sp(0x34u), key});
                if (contact == 0u) {
                    ctx.gpr[2] = made;
                    return;
                }
                const std::uint32_t normal = call_set(rt, ctx, 0x08C45D9Cu, {b});
                const float depth = call_setf(rt, ctx, 0x08C46364u, {sp(0x60u), normal}, {}).f0;
                touched |= 1u << point;
                put_float(rt, contact + 0x30u, -depth);
                made = 1u;
                const std::uint32_t shape_data = word(rt, body + 0xD8u);
                const float half = 0.5f;
                put_float(rt, contact + 0x24u, psp_mul(float_at(rt, shape_data + 0x68u) + float_at(rt, sp(0x74u)), half));
                put_float(rt, contact + 0x28u, psp_mul(float_at(rt, shape_data + 0x64u) + float_at(rt, sp(0x70u)), half));
                put_float(rt, contact + 0x2Cu, psp_mul(float_at(rt, word(rt, body + 0xD8u) + 0x6Cu), float_at(rt, sp(0x78u))));
                const std::uint32_t set = word(rt, slot);
                put16(rt, set, half_u(rt, set) | 1u);
            }
            if (signed_of(chain) < 0) continue;
            const std::uint32_t list = world + 0x60u;
            std::uint32_t link = word(rt, list) + chain * 0x54u;
            const std::uint32_t head = word(rt, sp(0x40u));
            for (const std::uint32_t at : {0x60u, 0x50u}) put(rt, sp(at), head);
            for (const std::uint32_t at : {0x44u, 0x54u, 0x64u}) put(rt, sp(at), list);
            for (const std::uint32_t at : {0x48u, 0x58u, 0x68u}) put(rt, sp(at), link);
            for (const std::uint32_t at : {0x4Cu, 0x5Cu, 0x6Cu}) put(rt, sp(at), 0u);
            while (link != 0u) {
                const std::uint32_t point = byte_u(rt, link + 0x4Du);
                if (sext8(byte_u(rt, link + 0x4Cu)) == part && (touched & (1u << point)) == 0u) {
                    const std::uint32_t original = call_set(rt, ctx, 0x08C45DA4u, {sp(0x200u) + point * 16u});
                    const std::uint32_t moved = call_set(rt, ctx, 0x08C467D4u, {original});
                    if (vcall_hit(sp(0x80u), center, moved) >= 0) {
                        put8(rt, link + 0x51u, 1u);
                        const float depth = call_setf(rt, ctx, 0x08C46364u, {sp(0x80u), original}, {}).f0;
                        made = 1u;
                        put_float(rt, link + 0x30u, -depth);
                        const GuestResult hash = call_keep(rt, ctx, 0x08BAC270u, {moved, center}, {});
                        put(rt, sp(0x2E0u), hash.v0);
                        put(rt, sp(0x2E4u), hash.v1);
                        put(rt, sp(0x18u), hash.v0);
                        put(rt, sp(0x1Cu), hash.v1);
                        (void)call_set(rt, ctx, 0x08BAC428u, {link + 0xCu, hash.v0, hash.v1});
                        put(rt, sp(0x10u), sp(0x80u));
                        put(rt, link + 0u, word(rt, sp(0x80u)));
                        put(rt, link + 4u, word(rt, sp(0x84u)));
                        put(rt, link + 8u, word(rt, sp(0x88u)));
                        const std::uint32_t set = word(rt, slot);
                        put16(rt, set, half_u(rt, set) | 1u);
                    }
                }
                const std::uint32_t next = sext16(half_u(rt, link + 0x52u));
                if (signed_of(next) < 0) break;
                link = word(rt, list) + next * 0x54u;
            }
        }
    }
    for (std::uint32_t index = first; index != count; ++index) {
        if ((hits & (1u << index)) == 0u) continue;
        const std::uint32_t set = word(rt, sets + index * 4u);
        if (set == 0u || (half_u(rt, set) & 1u) == 0u) continue;
        (void)call_set(rt, ctx, 0x08B012D8u, {set, world});
    }
    ctx.gpr[2] = made;
}

// func_08C17E04(first, middle, last, compare): a partial sort's heap selection over 0x2C-byte elements (libstdc++
// std::__heap_select's shape): the heap [first, middle) is built with func_08C174D4(first, hole, length, &value, &compare)
// from the last parent down; each element of [middle, last) that func_08A15D54(compare, element, first) puts before the
// top replaces it (the top moved into its place) and sifts down from 0; last func_08C178B0(first, middle, &compare)
// sorts the heap. `compare` is passed by value each time: a 0x4C-byte object whose vector of 16-byte items at +0x3C
// is deep-copied (func_08A5BC20(bytes, 4) and func_08C15F9C(begin, end, copy, &tag)) and destroyed (func_08BF5B28,
// func_08A5BC54(begin, bytes, 4)) around every call. Elements are divided by 0x2C through the inverse 0xBA2E8BA3.
void func_08C17E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t middle = ctx.gpr[5];
    const std::uint32_t last = ctx.gpr[6];
    const std::uint32_t compare = ctx.gpr[7];
    GuestFrame frame(ctx, 0x250u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    // A by-value copy of the comparator at `to` (its vector deep-copied, `tag` the allocator's frame byte).
    const auto copy_compare = [&](std::uint32_t to, std::uint32_t from, std::uint32_t tag) {
        for (const std::uint32_t at : {0x0u, 0x4u, 0x8u, 0xCu}) put(rt, to + at, word(rt, from + at));
        put8(rt, to + 0x10u, byte_u(rt, from + 0x10u));
        put8(rt, to + 0x11u, byte_u(rt, from + 0x11u));
        for (std::uint32_t at = 0x14u; at <= 0x38u; at += 4u) put(rt, to + at, word(rt, from + at));
        const std::uint32_t begin = word(rt, from + 0x3Cu);
        const std::uint32_t bytes = (word(rt, from + 0x40u) - begin) & ~15u;
        put(rt, to + 0x3Cu, 0u);
        put(rt, to + 0x40u, 0u);
        put(rt, to + 0x44u, 0u);
        const std::uint32_t storage = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        put(rt, to + 0x44u, storage + bytes);
        put(rt, to + 0x3Cu, storage);
        put(rt, to + 0x40u, storage);
        put(rt, to + 0x40u, call_set(rt, ctx, 0x08C15F9Cu, {begin, word(rt, from + 0x40u), storage, tag}));
        put(rt, to + 0x48u, word(rt, from + 0x48u));
    };
    const auto destroy_compare = [&](std::uint32_t object, std::uint32_t tag) {
        (void)call_set(rt, ctx, 0x08BF5B28u, {word(rt, object + 0x3Cu), word(rt, object + 0x40u), tag});
        const std::uint32_t begin = word(rt, object + 0x3Cu);
        if (begin == 0u) return;
        const std::uint32_t bytes = static_cast<std::uint32_t>(signed_of(word(rt, object + 0x44u) - begin) >> 4) << 4;
        (void)call_set(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    };
    const auto copy_element = [&](std::uint32_t to, std::uint32_t from) {
        for (std::uint32_t at = 0u; at < 0x2Cu; at += 4u) put(rt, to + at, word(rt, from + at));
    };

    const std::uint32_t heap = sp(0x194u);
    copy_compare(heap, compare, sp(9u));
    const std::uint32_t length = static_cast<std::uint32_t>(signed_of(middle - first) >> 2) * 0xBA2E8BA3u;
    if (signed_of(length) >= 2) {
        const std::uint32_t top = static_cast<std::uint32_t>(signed_of(length - 2u) / 2);
        for (std::uint32_t step = 0u;; ++step) {
            const std::uint32_t parent = top - step;
            copy_element(sp(0x38u), first + parent * 0x2Cu);
            copy_compare(sp(0xB0u), heap, sp(8u));
            (void)call_set(rt, ctx, 0x08C174D4u, {first, parent, length, sp(0x38u), sp(0xB0u)});
            destroy_compare(sp(0xB0u), sp(7u));
            if (step == top) break;
        }
    }
    destroy_compare(heap, sp(6u));
    for (std::uint32_t element = middle; element < last; element += 0x2Cu) {
        if (call_set(rt, ctx, 0x08A15D54u, {compare, element, first}) == 0u) continue;
        std::uint32_t value[11];
        for (std::uint32_t index = 0u; index < 11u; ++index) value[index] = word(rt, element + index * 4u);
        copy_compare(sp(0x148u), compare, sp(5u));
        copy_element(element, first);
        copy_compare(sp(0x64u), sp(0x148u), sp(4u));
        for (std::uint32_t index = 0u; index < 11u; ++index) put(rt, sp(0xCu) + index * 4u, value[index]);
        (void)call_set(rt, ctx, 0x08C174D4u, {first, 0u, length, sp(0xCu), sp(0x64u)});
        destroy_compare(sp(0x64u), sp(3u));
        destroy_compare(sp(0x148u), sp(2u));
    }
    copy_compare(sp(0xFCu), compare, sp(1u));
    (void)call_set(rt, ctx, 0x08C178B0u, {first, middle, sp(0xFCu)});
    destroy_compare(sp(0xFCu), sp(0u));
}

// func_08BC65F4(types, &stream, value, type) -> 1 when read: reads one value of `type` from the byte stream *stream
// (little-endian, advancing it) into `value` (a {data, object} pair whose +0 is the stored value's type token).
// Types 0x10000 + n are the records of `types` (0x14 bytes each from types->+8; fields of 0x10 bytes from the record's
// +4 to +8: +0 key, +4 type, 2 for an array of +8 items, +0xC a fixed count or 0 for a 32-bit count in the stream that
// sizes the array through func_08B19390): each field is opened with func_08B19590(slot, value, key, index), read
// recursively and closed (func_08B18A98). Below 16 a jump table at 0x08CC52E8: 0 nothing (1), 2 nothing (0); 1 an
// object of the class number read (classes 0x10000 + n from the same records, below 16 func_08BBC044), made by
// func_08B19030 and filled recursively (0); 3 a string of a 32-bit length (func_08BBD380, bytes copied with
// func_08BAFF30 un-sharing); 4-6 bytes, 7 a signed byte (func_08B19E84), 8/9 16-bit, 10-12 32-bit, 13/14 64-bit and 15 a
// 64-bit integer turned into a float by func_08B71EC8: each typed value is stored through value->+0's slot 0x20 when
// the stored type is the one its type function (func_08BC4A2C ... func_08B9DDB0) returns.
void func_08BC65F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t types = ctx.gpr[4];
    const std::uint32_t stream = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    const std::uint32_t type = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto read = [&](std::uint32_t bytes) {
        const std::uint32_t at = word(rt, stream);
        std::uint64_t result = 0u;
        for (std::uint32_t index = 0u; index < bytes; ++index) result |= static_cast<std::uint64_t>(byte_u(rt, at + index)) << (8u * index);
        put(rt, stream, at + bytes);
        return result;
    };
    const auto record_count = [&]() {
        return static_cast<std::uint32_t>(signed_of(word(rt, types + 0xCu) - word(rt, types + 8u)) >> 2) * 0xCCCCCCCDu;
    };
    const auto recurse = [&](std::uint32_t slot, std::uint32_t inner) {
        return call_set(rt, ctx, 0x08BC65F4u, {types, stream, slot, inner});
    };
    // A typed scalar: stored through value's slot 0x20 when its type token is the one `type_of` returns.
    const auto store = [&](std::uint32_t type_of, std::uint32_t at) {
        const std::uint32_t token = word(rt, value);
        if (token != call_set(rt, ctx, type_of, {})) return 1u;
        const std::uint32_t object = word(rt, value);
        (void)call_virtual_at(rt, ctx, object, 0x20u, {object, word(rt, value + 4u), at});
        return 1u;
    };
    const auto finish = [&](std::uint32_t result) { ctx.gpr[2] = result; };

    if (signed_of(type) > 0xFFFF) {
        const std::uint32_t index = type - 0x10000u;
        if (signed_of(index) < 0 || !(signed_of(index) < signed_of(record_count()))) return finish(0u);
        const std::uint32_t fields = word(rt, types + 8u) + index * 0x14u + 4u;
        const std::uint32_t slot = sp(0x3Cu);
        for (std::uint32_t field = word(rt, fields); field != word(rt, fields + 4u); field += 0x10u) {
            if (word(rt, field + 4u) != 2u) {
                (void)call_set(rt, ctx, 0x08B19590u, {slot, value, word(rt, field), 0u});
                (void)recurse(slot, word(rt, field + 4u));
                (void)call_set(rt, ctx, 0x08B18A98u, {slot});
                continue;
            }
            std::uint32_t items = word(rt, field + 0xCu);
            if (items == 0u) {
                items = static_cast<std::uint32_t>(read(4u));
                if (word(rt, value) != 0u) (void)call_set(rt, ctx, 0x08B19390u, {value, word(rt, field), items});
            }
            for (std::uint32_t item = 0u; signed_of(items) > 0 && item != items; ++item) {
                (void)call_set(rt, ctx, 0x08B19590u, {slot, value, word(rt, field), item});
                (void)recurse(slot, word(rt, field + 8u));
                (void)call_set(rt, ctx, 0x08B18A98u, {slot});
            }
        }
        return finish(1u);
    }
    if (type >= 16u) return finish(0u);
    switch (type) {
    case 0u: return finish(1u);
    case 2u: return finish(0u);
    case 1u: {
        const std::uint32_t kind = static_cast<std::uint32_t>(read(4u));
        if (kind == 0u) return finish(0u);
        std::uint32_t found = 0u;
        if (signed_of(kind) > 0xFFFF) {
            const std::uint32_t index = kind - 0x10000u;
            if (signed_of(index) >= 0 && signed_of(index) < signed_of(record_count())) {
                const std::uint32_t record = word(rt, types + 8u) + index * 0x14u;
                if (record != 0u) found = word(rt, record);
            }
        } else if (kind < 16u) {
            found = call_set(rt, ctx, 0x08BBC044u, {kind});
        }
        const std::uint32_t made = call_set(rt, ctx, 0x08B19030u, {found});
        const std::uint32_t token = word(rt, value);
        std::uint32_t holder = 0u;
        if (token == call_set(rt, ctx, 0x08BC4EB4u, {})) holder = word(rt, value + 4u);
        if (made != 0u && holder != 0u) {
            (void)call_virtual_at(rt, ctx, made, 0x8u, {sp(0x30u), made});
            (void)call_set(rt, ctx, 0x08B18C3Cu, {sp(0x30u), holder});
            (void)call_set(rt, ctx, 0x08B18A98u, {sp(0x30u)});
            (void)call_virtual_at(rt, ctx, made, 0xCu, {sp(0x3Cu), made, holder});
        } else {
            put(rt, sp(0x3Cu), 0u);
            put(rt, sp(0x40u), 0u);
            put8(rt, sp(0x44u), 0u);
        }
        (void)recurse(sp(0x3Cu), kind);
        (void)call_set(rt, ctx, 0x08B18A98u, {sp(0x3Cu)});
        return finish(0u);
    }
    case 3u: {
        const std::uint32_t text = sp(0x3Cu);
        const std::uint32_t length = static_cast<std::uint32_t>(read(4u));
        (void)call_set(rt, ctx, 0x08BBD380u, {text, length, 0u, sp(2u)});
        for (std::uint32_t index = 0u; signed_of(length) > 0 && index != length; ++index) {
            if (signed_of(word(rt, word(rt, text) - 4u)) >= 0) (void)call_set(rt, ctx, 0x08BAFF30u, {text});
            const std::uint32_t data = word(rt, text);
            put8(rt, data + index, static_cast<std::uint32_t>(read(1u)));
        }
        const std::uint32_t token = word(rt, value);
        if (token == call_set(rt, ctx, 0x08BC4D2Cu, {})) {
            const std::uint32_t object = word(rt, value);
            (void)call_virtual_at(rt, ctx, object, 0x20u, {object, word(rt, value + 4u), text});
        } else {
            const std::uint32_t wide = sp(0x1Cu);
            (void)call_set(rt, ctx, 0x08C74A68u, {wide, word(rt, text), sp(6u)});
            const std::uint32_t again = word(rt, value);
            if (again == call_set(rt, ctx, 0x08BC4BA4u, {})) {
                const std::uint32_t object = word(rt, value);
                (void)call_virtual_at(rt, ctx, object, 0x20u, {object, word(rt, value + 4u), wide});
            }
            release_string(rt, ctx, wide, 0x08D77498u, 0x08C74884u, sp(1u));
        }
        release_string(rt, ctx, text, 0x08D75298u, 0x08B93554u, sp(0u));
        return finish(1u);
    }
    case 4u: put8(rt, sp(5u), static_cast<std::uint32_t>(read(1u))); return finish(store(0x08BC4A2Cu, sp(5u)));
    case 5u: put8(rt, sp(4u), static_cast<std::uint32_t>(read(1u))); return finish(store(0x08BC48B4u, sp(4u)));
    case 6u: put8(rt, sp(3u), static_cast<std::uint32_t>(read(1u))); return finish(store(0x08BC473Cu, sp(3u)));
    case 7u: {
        const std::uint32_t byte = static_cast<std::uint32_t>(read(1u));
        (void)call_set(rt, ctx, 0x08B19E84u, {value, sext8(byte)});
        return finish(1u);
    }
    case 8u: put16(rt, sp(0xAu), static_cast<std::uint32_t>(read(2u))); return finish(store(0x08BC45C4u, sp(0xAu)));
    case 9u: put16(rt, sp(8u), static_cast<std::uint32_t>(read(2u))); return finish(store(0x08BC444Cu, sp(8u)));
    case 10u: put(rt, sp(0x18u), static_cast<std::uint32_t>(read(4u))); return finish(store(0x08BC42D4u, sp(0x18u)));
    case 11u: put(rt, sp(0x14u), static_cast<std::uint32_t>(read(4u))); return finish(store(0x08B9E8D4u, sp(0x14u)));
    case 12u: put(rt, sp(0x10u), static_cast<std::uint32_t>(read(4u))); return finish(store(0x08B9DDB0u, sp(0x10u)));
    case 13u:
    case 14u: {
        const std::uint64_t wide = read(8u);
        const std::uint32_t at = type == 13u ? sp(0x28u) : sp(0x20u);
        put(rt, at, static_cast<std::uint32_t>(wide));
        put(rt, at + 4u, static_cast<std::uint32_t>(wide >> 32));
        return finish(store(type == 13u ? 0x08BC415Cu : 0x08BC3FE4u, at));
    }
    default: {  // 15
        const std::uint64_t wide = read(8u);
        const float converted = call_setf(rt, ctx, 0x08B71EC8u, {static_cast<std::uint32_t>(wide), static_cast<std::uint32_t>(wide >> 32)}, {}).f0;
        const std::uint32_t token = word(rt, value);
        const std::uint32_t expected = call_set(rt, ctx, 0x08B9DDB0u, {});
        put_float(rt, sp(0xCu), converted);
        if (token != expected) return finish(1u);
        const std::uint32_t object = word(rt, value);
        (void)call_virtual_at(rt, ctx, object, 0x20u, {object, word(rt, value + 4u), sp(0xCu)});
        return finish(1u);
    }
    }
}

// func_0892E51C(source, out): fills the 0x690-byte `out` (cleared by func_08B36018; +0x10A = 1000, +0x10C = 100, +0x113 =
// 0) from the sections of `source`, whose 64-bit offsets sit at source+0x18..+0x104: each is loaded into a frame buffer
// by func_08936584(func_089363FC(), -, offset, buffer) and parsed by its own func_0892xxxx(source, out, buffer); the
// sections marked optional are skipped when their offset is -1 (both words). Then func_0892D700, the section +0xF8
// (load only) and +0xF0 (func_0892D71C), func_089361A8 / func_0892E424, the copies +0x24 -> +0x25 / +0x26 (>> 1), the
// header (offset +0) read into the frame base: its bytes +0x17 / +0x18 go to out+1 / +4; the halfwords out+0x138 past
// the count +0x155 and the bytes out+0xC0 (halfwords out+0xD8) past the count +0xBF are reset; then the fields
// +0xB / +8 / +0xA (100 when 0) / +0xC (1 for func_08935C68, 2 for func_08932570) / +0x108 / +0x15A / +0x1C4 (11 - n) /
// +0x1AC..+0x1AE from source+0x158.. .
void func_0892E51C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    const std::uint32_t buffer = frame.at(0x20u);
    (void)call_set(rt, ctx, 0x08B36018u, {out, 0u, 0x690u});
    put16(rt, out + 0x10Au, 1000u);
    put16(rt, out + 0x10Cu, 100u);
    put8(rt, out + 0x113u, 0u);
    const auto load = [&](std::uint32_t offset, std::uint32_t into) {
        const std::uint32_t handle = call_set(rt, ctx, 0x089363FCu, {});
        (void)call_set(rt, ctx, 0x08936584u, {handle, ctx.gpr[5], word(rt, source + offset), word(rt, source + offset + 4u), into});
    };
    const auto absent = [&](std::uint32_t offset) {
        return word(rt, source + offset) == 0xFFFFFFFFu && word(rt, source + offset + 4u) == 0xFFFFFFFFu;
    };
    struct Section {
        std::uint32_t offset;
        std::uint32_t parser;
        bool optional;
    };
    static constexpr Section kSections[] = {
        {0x18u, 0x0892C9F0u, false}, {0x20u, 0x0892DBB4u, false}, {0x68u, 0x0892CA0Cu, false}, {0x28u, 0x0892DC20u, false},
        {0x30u, 0x0892CB48u, false}, {0x38u, 0x0892CD18u, false}, {0x40u, 0x0892CD74u, false}, {0x48u, 0x0892D898u, false},
        {0x50u, 0x0892CDD4u, false}, {0x58u, 0x0892E2A4u, false}, {0x60u, 0x0892E12Cu, false}, {0x70u, 0x0892CE18u, true},
        {0x78u, 0x0892CE74u, false}, {0x80u, 0x0892CF40u, true}, {0x88u, 0x0892CF9Cu, true}, {0x90u, 0x0892D108u, true},
        {0x98u, 0x0892D164u, true}, {0xA0u, 0x0892D1C0u, true}, {0xA8u, 0x0892D32Cu, false}, {0xB0u, 0x0892D3E0u, true},
        {0xB8u, 0x0892D47Cu, true}, {0xC0u, 0x0892D520u, true}, {0xC8u, 0x0892D5ECu, true}, {0xD0u, 0x0892D648u, true},
        {0x100u, 0x0892D6A4u, true}, {0xD8u, 0x0892D794u, false}, {0xE0u, 0x0892D730u, false},
    };
    for (const Section &section : kSections) {
        if (section.optional && absent(section.offset)) continue;
        load(section.offset, buffer);
        (void)call_set(rt, ctx, section.parser, {source, out, buffer});
    }
    (void)call_set(rt, ctx, 0x0892D700u, {source, out});
    if (!absent(0xF8u)) load(0xF8u, buffer);
    if (!absent(0xF0u)) {
        load(0xF0u, buffer);
        (void)call_set(rt, ctx, 0x0892D71Cu, {source, out, buffer});
    }
    (void)call_set(rt, ctx, 0x089361A8u, {source, buffer});
    (void)call_set(rt, ctx, 0x0892E424u, {source, out, buffer});
    const std::uint32_t flags = byte_u(rt, out + 0x24u);
    put8(rt, out + 0x25u, flags);
    put8(rt, out + 0x26u, (flags >> 1) & 0x7Fu);
    put8(rt, out + 0x122u, call_set(rt, ctx, 0x08932C5Cu, {source}));
    load(0u, frame.at(0u));
    put8(rt, out + 1u, byte_u(rt, frame.at(0x17u)));
    put8(rt, out + 4u, byte_u(rt, frame.at(0x18u)));
    for (std::uint32_t index = byte_u(rt, out + 0x155u) + 1u; signed_of(index) < 12; ++index) put16(rt, out + 0x138u + index * 2u, 0xFFFFu);
    for (std::uint32_t index = byte_u(rt, out + 0xBFu); signed_of(index) < 16; ++index) {
        put8(rt, out + 0xC0u + index, 0xFFu);
        put16(rt, out + 0xD8u + index * 2u, 0u);
    }
    put8(rt, out + 0xBu, half_u(rt, source + 0x15Au));
    put8(rt, out + 8u, half_u(rt, source + 0x158u));
    put8(rt, out + 0xCu, 0u);
    const std::uint32_t level = byte_u(rt, source + 0x160u);
    put8(rt, out + 0xAu, level == 0u ? 100u : level);
    if (call_set(rt, ctx, 0x08935C68u, {source}) != 0u) put8(rt, out + 0xCu, 1u);
    if (call_set(rt, ctx, 0x08932570u, {source}) != 0u) put8(rt, out + 0xCu, 2u);
    if (half_u(rt, source + 0x156u) != 0u) put16(rt, out + 0x108u, half_u(rt, source + 0x156u));
    put8(rt, out + 0x15Au, call_set(rt, ctx, 0x08932CE0u, {source}) != 0u ? 1u : 0u);
    const std::uint32_t rank = byte_u(rt, source + 0x14Eu);
    put8(rt, out + 0x1C4u, rank != 0u ? 11u - rank : 0u);
    put8(rt, out + 0x1ACu, byte_u(rt, source + 0x161u));
    put8(rt, out + 0x1ADu, byte_u(rt, source + 0x162u));
    put8(rt, out + 0x1AEu, byte_u(rt, source + 0x163u));
    ctx.gpr[2] = byte_u(rt, source + 0x163u);
}

// func_08ACDEF0(player, path, mode) -> 1 when opened (also when the file does not open), else func_08ACDD14(player)'s
// result after closing it: opens a PSMF movie. func_08B0CE58(frame) and func_08ACDD14(player) first; the file
// player+0x40 is opened by func_08B0C9AC (+0x160 = 1) and its 0x800-byte header read (func_08B0C650 with a {buffer,
// size} pair) and checked by the Sony stubs func_08C7AF88..; the decoder's memory (func_08C7ADC8(0x400) bytes at
// +0x310 / +0x314, func_08C7AD40 on +0x1C0 with the callback 0x08ACCDB4) and the ring buffer (+0x308 / +0x30C /
// +0x244 -> +0x240, func_08C7ADD0 with 0x200 packets) are set up; the stream offset +0x514 and the header sizes must
// agree (frame +0 = +8, +4 = +0xC); the header is copied into a buffer of func_08B35DA0(size) bytes at +0x31C (the rest
// read when it is over 0x800) and parsed at +0x2C8 (sizes to +0x320 / +0x324). With one video stream (type 0) and at
// most one audio stream (types 1 / 2) of the counted ones (+0x330), each stream gets the Sony stream handle, its 0x40-byte
// slot at +0x248 + 0x40 n and its record: the video's at +0x3E4 (buffer func_08B2B4D8(0x40, +0x518 * 0x22000)), the
// audio's at +0x4CC (+0x4EC = not type 1, gain +0x4F8 = 1.0); +0x161 marks a video that answers func_08C7AFC0, +0x162 =
// 1 and the counts go to +0x328 / +0x32C.
void func_08ACDEF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t player = ctx.gpr[4];
    const std::uint32_t path = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    GuestFrame frame(ctx, 0x8E0u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto fail = [&]() { (void)call_set(rt, ctx, 0x08ACDD14u, {player}); };
    (void)call_set(rt, ctx, 0x08B0CE58u, {sp(0x50u)});
    (void)call_set(rt, ctx, 0x08ACDD14u, {player});
    const std::uint32_t file = player + 0x40u;
    if (signed_of(call_set(rt, ctx, 0x08B0C9ACu, {file, path, mode})) < 0) {
        ctx.gpr[2] = 1u;
        return;
    }
    put8(rt, player + 0x160u, 1u);
    const std::uint32_t header = sp(0x90u);
    put(rt, sp(0x30u), header);
    put(rt, sp(0x34u), 0x800u);
    if (call_set(rt, ctx, 0x08B0C650u, {file, sp(0x30u)}) != 0x800u) return fail();
    if (call_set(rt, ctx, 0x08C7AF88u, {header}) != 0u) return fail();
    const std::uint32_t work = call_set(rt, ctx, 0x08C7ADC8u, {0x400u});
    put(rt, player + 0x310u, work);
    put(rt, player + 0x314u, call_set(rt, ctx, 0x08B2B5E0u, {work}));
    if (call_set(rt, ctx, 0x08C7AD40u, {player + 0x1C0u, 0x400u, word(rt, player + 0x314u), word(rt, player + 0x310u),
                                        0x08ACCDB4u, player}) != 0u)
        return fail();
    const std::uint32_t ring_size = call_set(rt, ctx, 0x08C7ADB8u, {0u});
    put(rt, player + 0x308u, ring_size);
    const std::uint32_t ring = call_set(rt, ctx, 0x08B35840u, {0x40u, ring_size});
    put(rt, player + 0x30Cu, ring);
    put(rt, player + 0x244u, player + 0x240u);
    if (call_set(rt, ctx, 0x08C7ADD0u, {player + 0x240u, ring, word(rt, player + 0x308u), player + 0x1C0u, 0x200u, 0u, 0u}) != 0u)
        return fail();
    const std::uint32_t offset = word(rt, player + 0x514u);
    if (signed_of(offset) < 0) return fail();
    put(rt, sp(0x40u), 0xFFFFFFFFu);
    put(rt, sp(0x44u), offset);
    if (call_set(rt, ctx, 0x08C7AD90u, {word(rt, player + 0x244u), sp(0x40u)}) != 0u) return fail();
    if (call_set(rt, ctx, 0x08C7AD38u, {word(rt, player + 0x244u), header, sp(0u)}) != 0u) return fail();
    if (call_set(rt, ctx, 0x08C7AD68u, {header, sp(4u)}) != 0u) return fail();
    if (call_set(rt, ctx, 0x08C7AF98u, {header, sp(8u)}) != 0u) return fail();
    if (call_set(rt, ctx, 0x08C7AFB8u, {header, sp(0xCu)}) != 0u) return fail();
    if (word(rt, sp(0u)) != word(rt, sp(8u)) || word(rt, sp(4u)) != word(rt, sp(0xCu))) return fail();
    put(rt, player + 0x15Cu, word(rt, sp(4u)));
    put(rt, player + 0x158u, word(rt, sp(0u)));
    put(rt, player + 0x318u, word(rt, sp(0u)));
    const std::uint32_t copy = call_set(rt, ctx, 0x08B35DA0u, {word(rt, sp(0u))});
    put(rt, player + 0x31Cu, copy);
    for (std::uint32_t at = 0u; at < 0x800u; ++at) put8(rt, copy + at, byte_u(rt, header + at));
    const std::uint32_t size = word(rt, sp(8u));
    if (size >= 0x801u) {
        put(rt, sp(0x28u), word(rt, player + 0x31Cu) + 0x800u);
        put(rt, sp(0x2Cu), size - 0x800u);
        (void)call_set(rt, ctx, 0x08B0C650u, {file, sp(0x28u)});
    }
    const std::uint32_t psmf = player + 0x2C8u;
    if (call_set(rt, ctx, 0x08C7AFD0u, {psmf, word(rt, player + 0x31Cu)}) != 0u) return fail();
    (void)call_set(rt, ctx, 0x08C7AFE0u, {psmf});
    if (call_set(rt, ctx, 0x08C7AFA8u, {psmf, sp(0x10u)}) != 0u) return fail();
    if (call_set(rt, ctx, 0x08C7AFC8u, {psmf, sp(0x14u)}) != 0u) return fail();
    put(rt, player + 0x324u, word(rt, sp(0x14u)));
    put(rt, player + 0x320u, word(rt, sp(0x10u)));
    const std::uint32_t videos = call_set(rt, ctx, 0x08C7AFA0u, {psmf, 0u});
    (void)call_set(rt, ctx, 0x08C7AFA0u, {psmf, 1u});
    (void)call_set(rt, ctx, 0x08C7AFA0u, {psmf, 2u});
    const std::uint32_t audios = call_set(rt, ctx, 0x08C7AFA0u, {psmf, 0xFu});
    const std::uint32_t streams = videos + audios;
    if (signed_of(videos) <= 0 || signed_of(audios) < 0 || signed_of(videos) >= 2 || signed_of(audios) >= 2) return fail();
    std::uint32_t video_count = 0u;
    std::uint32_t audio_count = 0u;
    put(rt, player + 0x330u, streams);
    put8(rt, player + 0x161u, 0u);
    std::uint32_t slot = player + 0x248u;
    std::uint32_t video = player;
    std::uint32_t audio = player;
    for (std::uint32_t stream = 0u; signed_of(streams) > 0 && stream != streams; ++stream, slot += 0x40u) {
        if (call_set(rt, ctx, 0x08C7AF90u, {psmf, stream}) != 0u) return fail();
        if (call_set(rt, ctx, 0x08C7AFD8u, {psmf, sp(0x20u), sp(0x24u)}) != 0u) return fail();
        const std::uint32_t kind = word(rt, sp(0x20u));
        if (kind == 0u) {
            if (signed_of(video_count) > 0) continue;
            if (call_set(rt, ctx, 0x08C7AF90u, {psmf, stream}) == 0u && call_set(rt, ctx, 0x08C7AFC0u, {psmf}) == 0u)
                put8(rt, player + 0x161u, 1u);
            if (call_set(rt, ctx, 0x08C7AF80u, {psmf, sp(0x38u)}) != 0u) return fail();
            const std::uint32_t handle = call_set(rt, ctx, 0x08C7AD48u, {word(rt, player + 0x244u), 0u, word(rt, sp(0x24u))});
            if (handle == 0u) return fail();
            const std::uint32_t entry = call_set(rt, ctx, 0x08C7AD98u, {word(rt, player + 0x244u)});
            if (call_set(rt, ctx, 0x08C7AD30u, {word(rt, player + 0x244u), entry, slot}) != 0u) return fail();
            ++video_count;
            const std::uint32_t frames = word(rt, player + 0x518u);
            put(rt, video + 0x410u, call_set(rt, ctx, 0x08B2B4D8u, {0x40u, (frames << 13) + (frames << 17)}));
            put(rt, video + 0x3F4u, handle);
            put(rt, video + 0x3ECu, entry);
            put(rt, video + 0x3E4u, player);
            put(rt, video + 0x3F0u, slot);
            put(rt, video + 0x3F8u, stream);
            put(rt, video + 0x3E8u, word(rt, player + 0x244u));
            put(rt, video + 0x3FCu, word(rt, sp(0x38u)));
            put8(rt, video + 0x41Du, 0u);
            put(rt, video + 0x400u, word(rt, sp(0x3Cu)));
            put(rt, video + 0x408u, word(rt, sp(0x10u)));
            put(rt, video + 0x404u, 0x200u);
            put8(rt, video + 0x414u, 1u);
            put(rt, video + 0x418u, 0u);
            put8(rt, video + 0x41Cu, 0u);
            put(rt, video + 0x40Cu, 0u);
            put8(rt, video + 0x415u, 0u);
            put8(rt, video + 0x416u, 0u);
            video += 0xECu;
            continue;
        }
        if (signed_of(kind) < 0 || signed_of(kind) >= 3 || signed_of(audio_count) > 0) continue;
        const std::uint32_t handle = call_set(rt, ctx, 0x08C7AD48u, {word(rt, player + 0x244u), kind, word(rt, sp(0x24u))});
        if (handle == 0u) return fail();
        const std::uint32_t ring_handle = word(rt, player + 0x244u);
        (void)call_set(rt, ctx, kind == 1u ? 0x08C7ADE0u : 0x08C7ADB0u, {ring_handle, sp(0x18u), sp(0x1Cu)});
        const std::uint32_t memory = call_set(rt, ctx, 0x08B35840u, {0x40u, word(rt, sp(0x18u))});
        if (memory == 0u) return fail();
        if (call_set(rt, ctx, 0x08C7AD30u, {word(rt, player + 0x244u), memory, slot}) != 0u) return fail();
        put(rt, audio + 0x4DCu, handle);
        ++audio_count;
        put(rt, audio + 0x4ECu, kind != 1u ? 1u : 0u);
        put(rt, audio + 0x4D4u, memory);
        put(rt, audio + 0x4CCu, player);
        put(rt, audio + 0x4D8u, slot);
        put(rt, audio + 0x4E0u, stream);
        put(rt, audio + 0x4D0u, word(rt, player + 0x244u));
        put8(rt, audio + 0x4F5u, 0u);
        put8(rt, audio + 0x4F4u, 0u);
        put(rt, audio + 0x4E4u, word(rt, sp(0x10u)));
        put8(rt, audio + 0x50Eu, 0u);
        put8(rt, audio + 0x4E8u, 1u);
        put8(rt, audio + 0x50Du, 0u);
        put(rt, audio + 0x4F0u, 0u);
        put_float(rt, audio + 0x4F8u, 1.0f);
        put(rt, audio + 0x4FCu, 0u);
        put(rt, audio + 0x508u, 0u);
        put8(rt, audio + 0x50Cu, 0u);
        put(rt, audio + 0x500u, word(rt, sp(0x10u)));
        put8(rt, audio + 0x4E9u, 0u);
        put8(rt, audio + 0x4EAu, 0u);
        audio += 0xF0u;
    }
    put8(rt, player + 0x162u, 1u);
    put(rt, player + 0x328u, video_count);
    put(rt, player + 0x32Cu, audio_count);
    ctx.gpr[2] = 1u;
}

// func_08A4A3A0(holder): registers 30 script methods on the class object *holder: func_08B86FB0(*holder, name, method)
// for each (name, method) pair below (names are strings at 0x08CAEDB0..), the last as a tail jump; an empty holder
// first reports through func_08A5BB7C(0x08CAEDA8) each time.
void func_08A4A3A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kMethods[][2] = {
        {0x08CAEDB0u, 0x08A4A350u}, {0x08CAEDBCu, 0x08A4A960u}, {0x08CAEDC4u, 0x08A4AAA4u},
        {0x08CAEDDCu, 0x08A4ABE4u}, {0x08CAEDECu, 0x08A4AE9Cu}, {0x08CAEDF4u, 0x08A4AFB4u},
        {0x08CAEE00u, 0x08A4B318u}, {0x08CAEE0Cu, 0x08A4B4A8u}, {0x08CAEE18u, 0x08A4B638u},
        {0x08CAEE24u, 0x08A4B700u}, {0x08CAEE30u, 0x08A4B830u}, {0x08CAEE40u, 0x08A4B940u},
        {0x08CAEE50u, 0x08A4BA50u}, {0x08CAEE58u, 0x08A4BBC0u}, {0x08CAEE60u, 0x08A4BCF0u},
        {0x08CAEE70u, 0x08A4C1E0u}, {0x08CAEE80u, 0x08A4C2ACu}, {0x08CAEE90u, 0x08A4C40Cu},
        {0x08CAEEA0u, 0x08A4C538u}, {0x08CAEEACu, 0x08A4C580u}, {0x08CAEEB4u, 0x08A4A2D4u},
        {0x08CAEEBCu, 0x08A4C76Cu}, {0x08CAEED8u, 0x08A4A2DCu}, {0x08CAEEECu, 0x08A4C7B8u},
        {0x08CAEEFCu, 0x08A4C800u}, {0x08CAEF10u, 0x08A4A2E4u}, {0x08CAEF20u, 0x08A4C848u},
        {0x08CAEF30u, 0x08A4C88Cu}, {0x08CAEF48u, 0x08A4A2ECu}, {0x08CAEF54u, 0x08A4C8D8u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    register_methods(rt, ctx, holder, 0x08CAEDA8u, kMethods, std::size(kMethods), true);
}

// func_08A278B0(holder) -> holder: makes a script class and registers its methods: the name symbol
// func_08A69B3C(frame, 0x08CAB578), a 0x38-byte object (func_08A5BC20(0x38, 4)) built by func_08A7E914(object, name),
// handed to func_08A76AB4(holder, &object), the symbol released (func_08A69D68); then 32 methods through func_08B86FB0
// (report 0x08CAB58C for an empty holder).
void func_08A278B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kMethods[][2] = {
        {0x08CAB594u, 0x08A29408u}, {0x08CAB5B0u, 0x08A29338u}, {0x08CAB5CCu, 0x08A29268u},
        {0x08CAB5E8u, 0x08A29190u}, {0x08CAB604u, 0x08A290B8u}, {0x08CAB620u, 0x08A29010u},
        {0x08CAB638u, 0x08A28F68u}, {0x08CAB654u, 0x08A28EC0u}, {0x08CAB66Cu, 0x08A28E18u},
        {0x08CAB688u, 0x08A28D8Cu}, {0x08CAB6A4u, 0x08A28CE4u}, {0x08CAB6B8u, 0x08A28C3Cu},
        {0x08CAB6C4u, 0x08A28B94u}, {0x08CAB6D8u, 0x08A28AECu}, {0x08CAB6F0u, 0x08A28A44u},
        {0x08CAB700u, 0x08A2899Cu}, {0x08CAB710u, 0x08A28884u}, {0x08CAB720u, 0x08A27F20u},
        {0x08CAB734u, 0x08A287DCu}, {0x08CAB744u, 0x08A28734u}, {0x08CAB75Cu, 0x08A2868Cu},
        {0x08CAB770u, 0x08A285E4u}, {0x08CAB784u, 0x08A2853Cu}, {0x08CAB798u, 0x08A28494u},
        {0x08CAB7B0u, 0x08A283ECu}, {0x08CAB7C0u, 0x08A28344u}, {0x08CAB7D0u, 0x08A2829Cu},
        {0x08CAB7E0u, 0x08A27F68u}, {0x08CAB7F0u, 0x08A27FB0u}, {0x08CAB808u, 0x08A281F4u},
        {0x08CAB824u, 0x08A280D4u}, {0x08CAB838u, 0x08A2811Cu},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAB578u});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {object, frame.at(0u)});
    put(rt, frame.at(4u), object);
    (void)call_set(rt, ctx, 0x08A76AB4u, {holder, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    register_methods(rt, ctx, holder, 0x08CAB58Cu, kMethods, std::size(kMethods), false);
    ctx.gpr[2] = holder;
}

// func_0884F808(object): registers 61 script members on `object`, in this order: plain methods through
// func_08B87108(object, name, method) and properties through func_08B878E8(object, name, getter, setter) with the same
// function for both (the last as a tail jump).
void func_0884F808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    struct Member {
        std::uint32_t name;
        std::uint32_t function;
        bool property;
    };
    static constexpr Member kMembers[] = {
        {0x08C9D3CCu, 0x0884F760u, false}, {0x08C9D3DCu, 0x0884F7B4u, false},
        {0x08C9D3E8u, 0x0884FE7Cu, false}, {0x08C9D404u, 0x0884FF84u, false},
        {0x08C9D420u, 0x08850080u, false}, {0x08C9D43Cu, 0x0885017Cu, false},
        {0x08C9D450u, 0x08850264u, false}, {0x08C9D46Cu, 0x08850370u, false},
        {0x08C9D480u, 0x0885046Cu, false}, {0x08C9D494u, 0x08850574u, true},
        {0x08C9D49Cu, 0x08850684u, false}, {0x08C9D4ACu, 0x08850750u, false},
        {0x08C9D4BCu, 0x08850854u, false}, {0x08C9D4CCu, 0x08850958u, false},
        {0x08C9D4E4u, 0x08850B84u, false}, {0x08C9D4F8u, 0x08850C74u, false},
        {0x08C9D508u, 0x088511E0u, false}, {0x08C9D524u, 0x088512FCu, false},
        {0x08C9D53Cu, 0x08851390u, false}, {0x08C9D548u, 0x08851420u, false},
        {0x08C9D55Cu, 0x08851670u, false}, {0x08C9D56Cu, 0x08851760u, true},
        {0x08C9D580u, 0x08851864u, true}, {0x08C9D594u, 0x08851968u, true},
        {0x08C9D5A4u, 0x08851A6Cu, true}, {0x08C9D5B4u, 0x08851B70u, true},
        {0x08C9D5C4u, 0x08851C74u, true}, {0x08C9D5D4u, 0x08851D78u, true},
        {0x08C9D5E4u, 0x08851E7Cu, true}, {0x08C9D5F4u, 0x08851F80u, true},
        {0x08C9D604u, 0x08852084u, true}, {0x08C9D614u, 0x08852188u, true},
        {0x08C9D624u, 0x0885228Cu, true}, {0x08C9D634u, 0x08852390u, true},
        {0x08C9D644u, 0x08852494u, true}, {0x08C9D654u, 0x08852598u, true},
        {0x08C9D660u, 0x0885269Cu, true}, {0x08C9D668u, 0x088527A0u, true},
        {0x08C9D674u, 0x088528A4u, true}, {0x08C9D680u, 0x088529A8u, true},
        {0x08C9D68Cu, 0x08852AACu, true}, {0x08C9D698u, 0x08852BB0u, true},
        {0x08C9D6A0u, 0x08852CB4u, true}, {0x08C9D6A8u, 0x08852DB8u, true},
        {0x08C9D6B0u, 0x08852EBCu, true}, {0x08C9D6BCu, 0x08852FC0u, true},
        {0x08C9D6C4u, 0x088530C4u, true}, {0x08C9D6CCu, 0x088531C8u, true},
        {0x08C9D6D4u, 0x088532CCu, true}, {0x08C9D6DCu, 0x088533D0u, true},
        {0x08C9D6E4u, 0x088534D4u, true}, {0x08C9D6ECu, 0x088535D8u, true},
        {0x08C9D6F4u, 0x088536DCu, true}, {0x08C9D6FCu, 0x088537E0u, true},
        {0x08C9D704u, 0x088538E4u, true}, {0x08C9D710u, 0x088539E8u, true},
        {0x08C9D71Cu, 0x08853AECu, true}, {0x08C9D72Cu, 0x08853BF0u, true},
        {0x08C9D738u, 0x08853CF4u, true}, {0x08C9D750u, 0x08853DF8u, true},
        {0x08C9D768u, 0x08853EFCu, true},
    };
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::size_t count = std::size(kMembers);
    for (std::size_t index = 0u; index < count; ++index) {
        const Member &member = kMembers[index];
        if (index + 1u == count)
            return tail_call(ctx, 0x08B878E8u, {object, member.name, member.function, member.function});
        if (member.property)
            (void)call_set(rt, ctx, 0x08B878E8u, {object, member.name, member.function, member.function});
        else
            (void)call_set(rt, ctx, 0x08B87108u, {object, member.name, member.function});
    }
}

// func_08A5BE8C(initialize, priority): a translation unit's static constructor / destructor (only for priority 0xFFFF;
// otherwise v0 = 0xFFFF and nothing happens). initialize == 1: func_08AAD5F0(0x08CFF060), func_08AAD9C4(0x08CF705C),
// the 94 global symbols below made by func_08A69B3C(global, name), an empty tree at 0x08CFF120 (header 0x08CFF124
// linked to itself) and the destructor 0x08A5BD90 of 0x08CFF110 registered by a tail jump to func_08B1B858(object,
// destructor, 0). initialize == 0: func_08C1EC60(tree, its root +8), the symbols released by func_08A69D68 in reverse,
// func_08AAD860(0x08CF705C) and a tail jump to func_08AAD510(0x08CFF060).
void func_08A5BE8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kSymbols[][2] = {
        {0x08CF7054u, 0x08CAFC54u}, {0x08CF7050u, 0x08CAFC5Cu}, {0x08CF704Cu, 0x08CAFC60u},
        {0x08CF7048u, 0x08CAFC6Cu}, {0x08CF7044u, 0x08CAFC74u}, {0x08CF7040u, 0x08CAFC7Cu},
        {0x08CF703Cu, 0x08CAFC84u}, {0x08CF7038u, 0x08CAFC8Cu}, {0x08CF7034u, 0x08CAFC94u},
        {0x08CF7030u, 0x08CAFCA0u}, {0x08CF702Cu, 0x08CAFCB0u}, {0x08CF7028u, 0x08CAFCB8u},
        {0x08CF7024u, 0x08CAFCBCu}, {0x08CF7020u, 0x08CAFCC4u}, {0x08CF701Cu, 0x08CAFCCCu},
        {0x08CF7018u, 0x08CAFCD4u}, {0x08CF7014u, 0x08CAFCDCu}, {0x08CF7010u, 0x08CAFCE4u},
        {0x08CF700Cu, 0x08CAFCE8u}, {0x08CF7008u, 0x08CAFCF0u}, {0x08CF7004u, 0x08CAFCFCu},
        {0x08CF7000u, 0x08CAFD00u}, {0x08CF6FFCu, 0x08CAFD08u}, {0x08CF6FF8u, 0x08CAFD0Cu},
        {0x08CF6FF4u, 0x08CAFD14u}, {0x08CF6FF0u, 0x08CAFD24u}, {0x08CF6FECu, 0x08CAFD30u},
        {0x08CF6FE8u, 0x08CAFD38u}, {0x08CF6FE4u, 0x08CAFD40u}, {0x08CF6FE0u, 0x08CAFD48u},
        {0x08CF6FDCu, 0x08CAFD4Cu}, {0x08CF6FD8u, 0x08CAFD50u}, {0x08CF6FD4u, 0x08CAFD5Cu},
        {0x08CF6FD0u, 0x08CAFD64u}, {0x08CF6FCCu, 0x08CAFD6Cu}, {0x08CF6FC8u, 0x08CAFD74u},
        {0x08CF6FC4u, 0x08CAFD7Cu}, {0x08CF6FC0u, 0x08CAFD88u}, {0x08CF6FBCu, 0x08CAFD90u},
        {0x08CF6FB8u, 0x08CAFD98u}, {0x08CF6FB4u, 0x08CAFDA0u}, {0x08CF6FB0u, 0x08CAFDACu},
        {0x08CF6FACu, 0x08CAFDB4u}, {0x08CF6FA8u, 0x08CAFDC0u}, {0x08CF6FA4u, 0x08CAFDC8u},
        {0x08CF6FA0u, 0x08CAFDD0u}, {0x08CF6F9Cu, 0x08CAFDD8u}, {0x08CF6F98u, 0x08CAFDE0u},
        {0x08CF6F94u, 0x08CAFDE8u}, {0x08CF6F90u, 0x08CAFDF0u}, {0x08CF6F8Cu, 0x08CAFE00u},
        {0x08CF6F88u, 0x08CAFE10u}, {0x08CF6F84u, 0x08CAFE18u}, {0x08CF6F80u, 0x08CAFE28u},
        {0x08CF6F7Cu, 0x08CAFE38u}, {0x08CF6F78u, 0x08CAFE44u}, {0x08CF6F74u, 0x08CAFE4Cu},
        {0x08CF6F70u, 0x08CAFE58u}, {0x08CF6F6Cu, 0x08CAFE64u}, {0x08CF6F68u, 0x08CAFE6Cu},
        {0x08CF6F64u, 0x08CAFE78u}, {0x08CF6F60u, 0x08CAFE7Cu}, {0x08CF6F5Cu, 0x08CAFE84u},
        {0x08CF6F58u, 0x08CAFE90u}, {0x08CF6F54u, 0x08CAFE98u}, {0x08CF6F50u, 0x08CAFEA0u},
        {0x08CF6F4Cu, 0x08CAFEA4u}, {0x08CF6F48u, 0x08CAFEACu}, {0x08CF6F44u, 0x08CAFEB4u},
        {0x08CF6F40u, 0x08CAFEBCu}, {0x08CF6F3Cu, 0x08CAFEC4u}, {0x08CF6F38u, 0x08CAFECCu},
        {0x08CF6F34u, 0x08CAFED4u}, {0x08CF6F30u, 0x08CAFEE0u}, {0x08CF6F2Cu, 0x08CAFEF0u},
        {0x08CF6F28u, 0x08CAFEFCu}, {0x08CF6F24u, 0x08CAFF0Cu}, {0x08CF6F20u, 0x08CAFF18u},
        {0x08CF6F1Cu, 0x08CAFF24u}, {0x08CF6F18u, 0x08CAFF30u}, {0x08CF6F14u, 0x08CAFF3Cu},
        {0x08CF6F10u, 0x08CAFF44u}, {0x08CF6F0Cu, 0x08CAFF54u}, {0x08CF6F08u, 0x08CAFF64u},
        {0x08CF6F04u, 0x08CAFF70u}, {0x08CF6F00u, 0x08CAFF80u}, {0x08CF6EFCu, 0x08CAFF88u},
        {0x08CF6EF8u, 0x08CAFF8Cu}, {0x08CF6EF4u, 0x08CAFF94u}, {0x08CF6EF0u, 0x08CAFF9Cu},
        {0x08CF6EECu, 0x08CAFFA8u}, {0x08CF6EE8u, 0x08CAFFACu}, {0x08CF6EE4u, 0x08CAFFB4u},
        {0x08CF6EE0u, 0x08CAFFBCu},
    };
    const std::uint32_t initialize = ctx.gpr[4];
    const std::uint32_t priority = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    constexpr std::uint32_t kTree = 0x08CFF120u;
    ctx.gpr[2] = 0xFFFFu;
    if (priority != 0xFFFFu || (initialize != 1u && initialize != 0u)) return;
    if (initialize == 1u) {
        (void)call_set(rt, ctx, 0x08AAD5F0u, {0x08CFF060u});
        (void)call_set(rt, ctx, 0x08AAD9C4u, {0x08CF705Cu});
        for (const auto &symbol : kSymbols) (void)call_set(rt, ctx, 0x08A69B3Cu, {symbol[0], symbol[1]});
        put(rt, kTree + 0x10u, kTree + 4u);
        put(rt, kTree + 0x14u, 0u);
        put(rt, kTree + 4u, 0u);
        put(rt, kTree + 8u, 0u);
        put(rt, kTree + 0xCu, kTree + 4u);
        ctx.gpr[2] = kTree;
        ctx.gpr[3] = kTree + 4u;
        return tail_call(ctx, 0x08B1B858u, {0x08CFF110u, 0x08A5BD90u, 0u});
    }
    (void)call_set(rt, ctx, 0x08C1EC60u, {kTree, word(rt, kTree + 8u)});
    for (std::size_t index = std::size(kSymbols); index-- > 0u;) (void)call_set(rt, ctx, 0x08A69D68u, {kSymbols[index][0]});
    (void)call_set(rt, ctx, 0x08AAD860u, {0x08CF705Cu});
    tail_call(ctx, 0x08AAD510u, {0x08CFF060u});
}

} // namespace hot_batch_131
} // namespace gt::game

#define HOT131(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_131::name, gt::ReturnKind::kind)
HOT131(0x088E9B90u, func_088E9B90, Word);
// HOT131(0x08B00770u, func_08B00770, Word);  // batch 131: compared 0 in the fast gate (no route reaches it)
// HOT131(0x08C17E04u, func_08C17E04, Word);  // batch 131: compared 0 in the fast gate (no route reaches it)
HOT131(0x08BC65F4u, func_08BC65F4, Word);  // compared in the fast gate (uncapped routes), 0 in the full gate
HOT131(0x0892E51Cu, func_0892E51C, Word);  // compared in the fast gate (uncapped routes), 0 in the full gate
// HOT131(0x08ACDEF0u, func_08ACDEF0, Word);  // batch 131: compared 0 in the fast and the full gate (a movie open the routes do not compare)
HOT131(0x08A4A3A0u, func_08A4A3A0, Word);  // compared in the fast gate (uncapped routes), 0 in the full gate
HOT131(0x08A278B0u, func_08A278B0, Word);  // compared in the fast gate (uncapped routes), 0 in the full gate
HOT131(0x0884F808u, func_0884F808, Word);
HOT131(0x08A5BE8Cu, func_08A5BE8C, Word);
