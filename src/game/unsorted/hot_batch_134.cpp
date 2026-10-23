// Hot batch 134: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_134 {
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

// The shared-pointer counts of the 0x08Bxxxxx library: `ll/sc` steps on guest words (single-threaded here). A release
// drops the use count (+4); at zero the object's slot 8 runs, then the weak count (+8) drops and at zero its slot 0xC.
inline void count_up(psprecomp::Runtime &rt, std::uint32_t control) { put(rt, control + 4u, word(rt, control + 4u) + 1u); }
inline void release_control(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t control) {
    if (control == 0u) return;
    const std::uint32_t uses = word(rt, control + 4u);
    put(rt, control + 4u, uses - 1u);
    if (uses != 1u) return;
    (void)call_virtual_at(rt, ctx, control, 8u, {control});
    const std::uint32_t weak = word(rt, control + 8u);
    put(rt, control + 8u, weak - 1u);
    if (weak != 1u) return;
    (void)call_virtual_at(rt, ctx, control, 0xCu, {control});
}
// `slot`'s control word (the pointer pair's second word) takes `control`: counted up first, the old one released.
inline void assign_control(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t control) {
    if (control == word(rt, slot)) return;
    if (control != 0u) count_up(rt, control);
    release_control(rt, ctx, word(rt, slot));
    put(rt, slot, control);
}

} // namespace


// func_088DDB6C(race): ends a race. The counters at (race+0x130)+0x38..+0x44 are cleared, func_088D1F60(+0x618),
// func_088CF0E0(+0x628) and func_088E77C0(+0x630) run (when set); every car gets its byte in race->+0x598 set and, when
// active (+4 == 1), the world +0x148 hears about it (slot 0x40 with its index, whether +0x1C is 1 and
// func_088F8D58(+0x14C, index)). With a session (+0x640) the results are recorded through func_088E4360(session, key,
// 64-bit value): race keys 0x4F, 7, 0xD, 0x12, 0x14, 0x16, 0xC, 0x13 and the time 0x19; the first car
// func_088DC858 refuses (keys 0xE .. 0x10, 0x27 .. 0x2A from func_088E82E4); the record func_088DD984 picks out of the
// 0xD0-byte records at +0x644 (keys 0x1A .. 0x26 and 0x20; when the world's slot 0x7C accepts it also 0x2F = +0x34 *
// 1000, the two ratios 0x30 / 0x31 * 10000 when func_088E444C counts entries, and its block +0x50 as 0x33); 0x39 for
// +0x5B0; a cleared 0x400-byte frame block through func_088E4168 and func_088E6E14(session, 6) unless func_088D9D50
// agrees. Last func_088D776C(race, 1), func_088DC1B8(race, 3), the flags +0x1F / +0x23 cleared and +0x5C1 set, the
// listener +0x134's slot 0x34, func_088DC784(race, 1) and +0x22 set when +0x5BA is.
void func_088DDB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t race = ctx.gpr[4];
    GuestFrame frame(ctx, 0x440u);
    const auto session = [&]() { return word(rt, race + 0x640u); };
    const auto record = [&](std::uint32_t key, std::uint32_t value) {
        (void)call_set(rt, ctx, 0x088E4360u, {session(), key, value, static_cast<std::uint32_t>(signed_of(value) >> 31)});
    };
    const auto record_unsigned = [&](std::uint32_t key, std::uint32_t value) {
        (void)call_set(rt, ctx, 0x088E4360u, {session(), key, value, 0u});
    };
    const std::uint32_t counters = word(rt, race + 0x130u);
    put(rt, counters + 0x38u, 0u);
    put(rt, counters + 0x3Cu, 0u);
    put(rt, word(rt, race + 0x130u) + 0x40u, 0u);
    put(rt, word(rt, race + 0x130u) + 0x44u, 0u);
    (void)call_set(rt, ctx, 0x088D1F60u, {word(rt, race + 0x618u), 0u});
    if (word(rt, race + 0x628u) != 0u) (void)call_set(rt, ctx, 0x088CF0E0u, {word(rt, race + 0x628u)});
    if (word(rt, race + 0x630u) != 0u) (void)call_set(rt, ctx, 0x088E77C0u, {word(rt, race + 0x630u)});
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(call_set(rt, ctx, 0x088D9F80u, {race})); ++index) {
        ctx.gpr[5] = index;
        put8(rt, word(rt, race + 0x598u) + index * 8u, 1u);
        const std::uint32_t car = word(rt, race + 0xC0u) + index * 0x2F8u;
        if (word(rt, car + 4u) != 1u) continue;
        const std::uint32_t world = word(rt, race + 0x148u);
        const std::uint32_t player = word(rt, car + 0x1Cu) == 1u ? 1u : 0u;
        const std::uint32_t method = word(rt, word(rt, world) + 0x40u);
        const std::uint32_t view = call_set(rt, ctx, 0x088F8D58u, {word(rt, race + 0x14Cu), index});
        (void)call_set(rt, ctx, method, {world, index, player, view});
    }
    if (session() != 0u) {
        const std::uint32_t rules = race + 4u;
        (void)call_set(rt, ctx, 0x088E4360u, {session(), 0x4Fu, word(rt, word(rt, race + 0x130u) + 0x48u), word(rt, word(rt, race + 0x130u) + 0x4Cu)});
        record_unsigned(7u, byte_u(rt, word(rt, race + 0x130u) + 0xFu));
        record_unsigned(0xDu, call_set(rt, ctx, 0x088D5DF8u, {rules}));
        record_unsigned(0x12u, call_set(rt, ctx, 0x088D62FCu, {rules}));
        record_unsigned(0x14u, call_set(rt, ctx, 0x088D4DD4u, {rules}));
        record(0x16u, call_set(rt, ctx, 0x088DA160u, {race}));
        record(0xCu, call_set(rt, ctx, 0x088DA168u, {race}));
        record(0x13u, call_set(rt, ctx, 0x088DA170u, {race, 0u}));
        const GuestResult total = call_keep(rt, ctx, 0x088E43A8u, {session(), 0u}, {});
        const GuestResult used = call_keep(rt, ctx, 0x088E6D90u, {session(), 0xFFFFFFFFu}, {});
        const std::uint32_t low = total.v0 - used.v0;
        const std::uint32_t high = total.v1 - used.v1 - (total.v0 < low ? 1u : 0u);
        (void)call_set(rt, ctx, 0x088E4360u, {session(), 0x19u, low, high});
        const std::uint32_t cars = call_set(rt, ctx, 0x088D9F80u, {race});
        for (std::uint32_t index = 0u; signed_of(index) < signed_of(cars); ++index) {
            if (call_set(rt, ctx, 0x088DC858u, {race, index}) != 0u) continue;
            const std::uint32_t car = word(rt, race + 0xC0u) + index * 0x2F8u;
            record(0xEu, call_set(rt, ctx, 0x088DA1C8u, {race, index}));
            record(0xFu, call_set(rt, ctx, 0x088D636Cu, {rules, index}));
            record(0x10u, index);
            record(0x27u, call_set(rt, ctx, 0x088E82E4u, {car, 0xFFFFFFFFu}));
            record(0x28u, call_set(rt, ctx, 0x088E82E4u, {car, 0u}));
            record(0x29u, call_set(rt, ctx, 0x088E82E4u, {car, 1u}));
            record(0x2Au, call_set(rt, ctx, 0x088E82E4u, {car, 2u}));
            break;
        }
        const std::uint32_t chosen = call_set(rt, ctx, 0x088DD984u, {race, 0u});
        if (signed_of(chosen) >= 0) {
            const std::uint32_t entry = word(rt, race + 0x644u) + chosen * 0xD0u;
            static constexpr std::uint32_t kKeys[] = {0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u};
            for (std::uint32_t index = 0u; index < 12u; ++index) record(kKeys[index], word(rt, entry + index * 4u));
            record(0x20u, chosen);
            const std::uint32_t world = word(rt, race + 0x148u);
            if (call_virtual_at(rt, ctx, world, 0x7Cu, {world, chosen, entry}) != 0u) {
                record(0x2Fu, ctx.fpu_float_to_word(psp_mul(float_at(rt, entry + 0x34u), std::bit_cast<float>(0x447A0000u)), 1u));
                const std::uint32_t entries = call_set(rt, ctx, 0x088E444Cu, {session()});
                if (signed_of(entries) > 0) {
                    const std::int32_t laps = signed_of(word(rt, entry + 0x38u)) / signed_of(entries + 0xCu);
                    float first = 0.0f;
                    float second = 0.0f;
                    if (laps != 0) {
                        const float count = static_cast<float>(laps);
                        const std::int32_t extra = signed_of(word(rt, entry + 0x40u));
                        const float scale = std::bit_cast<float>(0x461C4000u);  // 10000
                        first = psp_mul(static_cast<float>(laps - signed_of(word(rt, entry + 0x3Cu)) + extra) / count, scale);
                        second = psp_mul(static_cast<float>(extra) / count, scale);
                    }
                    record(0x30u, ctx.fpu_float_to_word(first, 1u));
                    record(0x31u, ctx.fpu_float_to_word(second, 1u));
                }
                (void)call_set(rt, ctx, 0x088E436Cu, {session(), 0x33u, entry + 0x50u});
            }
        }
        if (word(rt, race + 0x5B0u) != 0u) record(0x39u, word(rt, race + 0x5B0u));
        (void)call_set(rt, ctx, 0x08B36018u, {frame.at(0u), 0u, 0x400u});
        (void)call_set(rt, ctx, 0x088E4168u, {session(), frame.at(0u), 0x400u});
        if (call_set(rt, ctx, 0x088D9D50u, {race}) == 0u) (void)call_set(rt, ctx, 0x088E6E14u, {session(), 6u});
    }
    (void)call_set(rt, ctx, 0x088D776Cu, {race, 1u});
    (void)call_set(rt, ctx, 0x088DC1B8u, {race, 3u});
    put8(rt, race + 0x1Fu, 0u);
    put8(rt, race + 0x23u, 0u);
    put8(rt, race + 0x5C1u, 1u);
    const std::uint32_t listener = word(rt, race + 0x134u);
    if (listener != 0u) (void)call_virtual_at(rt, ctx, listener, 0x34u, {listener});
    (void)call_set(rt, ctx, 0x088DC784u, {race, 1u});
    if (byte_u(rt, race + 0x5BAu) != 0u) put8(rt, race + 0x22u, 1u);
}

// func_08B15FC0(-, loader) -> bytes used, or a negative result: reads a PAC archive through the open file of `loader`
// (+8 the file, its +0xB8 the size; +0x20 set = busy (error 0xF); +0x28 the buffer, owned when +0x30 is set; +0x2C the size
// limit; +0x34 / +0x38 per-type entry handlers; +4 the error code). func_08B0C880 starts the read; the 0x40-byte header
// (func_08B0C650 with a {buffer, size} pair) holds the magic: "PACL" / "BCAP" are taken as they are, "PACB" / "LCAP" are
// byte-swapped (the words +4..+0x3C, then each 16-byte entry's two halfwords and three words). The table of entries
// (count at +0x3C) follows; an owned buffer is grown with func_08B35E3C to the size the handlers (or each entry's
// alignment + size) ask for. Each entry is then reached by func_08B158BC (skipping the gap to its file offset +8) and
// either given to its handler (`fn(context, &state, 0)`) or read at the next position aligned to its +4 (that position
// written to its +8). func_08B0C71C closes; errors are 3 (memory), 5 (reading), 2 (magic), 0x15 (a handler).
void func_08B15FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t loader = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto finish = [&](std::uint32_t result) { ctx.gpr[2] = result; };
    const auto fail = [&](std::uint32_t code) {
        (void)call_set(rt, ctx, 0x08B0C71Cu, {loader});
        put(rt, loader + 4u, code);
        finish(0xFFFFFFFFu);
    };
    const auto read = [&](std::uint32_t pair) { return call_set(rt, ctx, 0x08B0C650u, {loader, pair}); };
    // A byte-reversed word at `at` (any alignment).
    const auto reverse = [&](std::uint32_t at) {
        const std::uint32_t b0 = byte_u(rt, at), b1 = byte_u(rt, at + 1u), b2 = byte_u(rt, at + 2u), b3 = byte_u(rt, at + 3u);
        put8(rt, at, b3);
        put8(rt, at + 1u, b2);
        put8(rt, at + 2u, b1);
        put8(rt, at + 3u, b0);
    };
    const auto handler_of = [&](std::uint32_t entry) -> std::uint32_t {
        if (signed_of(word(rt, loader + 0x38u)) >= 0) return word(rt, loader + 0x34u) + half_u(rt, entry) * 8u;
        return word(rt, loader + 0x34u);
    };

    const std::uint32_t file_size = word(rt, word(rt, loader + 8u) + 0xB8u);
    std::uint32_t limit = word(rt, loader + 0x2Cu);
    std::uint32_t buffer = word(rt, loader + 0x28u);
    if (word(rt, loader + 0x20u) != 0u) {
        put(rt, loader + 4u, 0xFu);
        return finish(0xFFFFFFFFu);
    }
    if (file_size < limit) limit = file_size;
    const std::uint32_t started = call_set(rt, ctx, 0x08B0C880u, {loader, file_size});
    if (signed_of(started) < 0) return finish(started);
    std::uint32_t size = limit;
    if (buffer == 0u) {
        size = limit != 0u ? limit : 0x800u;
        buffer = call_set(rt, ctx, 0x08B15490u, {size, 1u});
        if (buffer == 0u) return fail(3u);
        put8(rt, loader + 0x30u, 1u);
    }
    put(rt, loader + 0x28u, buffer);
    if (size < 0x40u) return fail(3u);
    put(rt, sp(0x14u), 0x40u);
    put(rt, sp(0x10u), buffer);
    std::uint32_t result = read(sp(0x10u));
    if (signed_of(result) < 0) {
        (void)call_set(rt, ctx, 0x08B0C71Cu, {loader});
        put(rt, loader + 4u, 5u);
        return finish(result);
    }
    std::uint32_t native = 0u;
    const std::uint32_t magic = word(rt, buffer);
    if (magic == 0x4C434150u || magic == 0x50414342u) {
        native = 1u;
    } else if (magic == 0x42434150u || magic == 0x5041434Cu) {
        for (std::uint32_t at = 4u; at < 0x40u; at += 4u) reverse(buffer + at);
    } else {
        (void)call_set(rt, ctx, 0x08B0C71Cu, {loader});
        put(rt, loader + 4u, 2u);
        return finish(0xFFFFFFFFu);
    }
    const std::uint32_t table = word(rt, buffer + 0x3Cu) * 16u;
    const std::uint32_t start = table + 0x40u;
    std::uint32_t header = buffer;  // where the entry count is read (s5)
    if (size < start) {
        if (byte_u(rt, loader + 0x30u) == 0u) return fail(3u);
        buffer = call_set(rt, ctx, 0x08B35E3Cu, {buffer, start});
        header = buffer;
        if (buffer == 0u) return fail(3u);
        size = start;
    }
    put(rt, sp(8u), buffer + 0x40u);
    put(rt, sp(0xCu), table);
    result = read(sp(8u));
    if (signed_of(result) < 0) {
        (void)call_set(rt, ctx, 0x08B0C71Cu, {loader});
        put(rt, loader + 4u, 5u);
        return finish(result);
    }
    const auto count = [&]() { return word(rt, header + 0x3Cu); };
    if (native == 0u) {
        for (std::uint32_t index = 0u; index < count(); ++index) {
            const std::uint32_t entry = buffer + 0x40u + index * 16u;
            const std::uint32_t b0 = byte_u(rt, entry), b1 = byte_u(rt, entry + 1u), b2 = byte_u(rt, entry + 2u), b3 = byte_u(rt, entry + 3u);
            put8(rt, entry + 2u, b3);
            put8(rt, entry, b1);
            put8(rt, entry + 1u, b0);
            put8(rt, entry + 3u, b2);
            reverse(entry + 4u);
            reverse(entry + 8u);
            reverse(entry + 0xCu);
        }
    }
    // The handlers' state, at frame +0x18: loader, size, buffer, native, index, entry, cursor, room, file position.
    const std::uint32_t state = sp(0x18u);
    if (byte_u(rt, loader + 0x30u) != 0u) {
        put(rt, sp(0x30u), 0u);
        put(rt, sp(0x34u), 0u);
        put(rt, sp(0x1Cu), size);
        put(rt, sp(0x18u), loader);
        put(rt, sp(0x20u), buffer);
        put8(rt, sp(0x24u), native);
        put(rt, sp(0x38u), size);
        for (std::uint32_t index = 0u; index < count(); ++index) {
            const std::uint32_t entry = buffer + 0x40u + index * 16u;
            put(rt, sp(0x28u), index);
            put(rt, sp(0x2Cu), entry);
            const std::uint32_t handler = handler_of(entry);
            if (handler != 0u && word(rt, handler) != 0u) {
                if (signed_of(call_set(rt, ctx, word(rt, handler), {word(rt, handler + 4u), state, 1u})) < 0) return fail(0x15u);
                continue;
            }
            put(rt, sp(0x1Cu), word(rt, sp(0x1Cu)) + word(rt, entry + 4u) + word(rt, entry + 0xCu));
        }
        const std::uint32_t needed = word(rt, sp(0x1Cu));
        if (size != needed) {
            buffer = call_set(rt, ctx, 0x08B35E3Cu, {buffer, needed});
            header = buffer;
            size = needed;
            if (buffer == 0u) return fail(3u);
        }
    }
    put8(rt, sp(0x24u), native);
    put(rt, sp(0x34u), size - start);
    put(rt, sp(0x30u), buffer + start);
    put(rt, sp(0x1Cu), size);
    put(rt, sp(0x18u), loader);
    put(rt, sp(0x20u), buffer);
    put(rt, sp(0x38u), start);
    std::uint32_t position = start;
    for (std::uint32_t index = 0u; index < count(); ++index) {
        const std::uint32_t entry = buffer + 0x40u + index * 16u;
        const std::uint32_t gap = word(rt, entry + 8u) - position;
        const std::uint32_t bytes = word(rt, entry + 0xCu);
        if (signed_of(call_set(rt, ctx, 0x08B158BCu, {loader, gap})) < 0) return fail(5u);
        put(rt, sp(0x28u), index);
        position = gap + word(rt, sp(0x38u));
        put(rt, sp(0x38u), position);
        put(rt, sp(0x2Cu), entry);
        const std::uint32_t handler = handler_of(entry);
        if (handler != 0u && word(rt, handler) != 0u) {
            if (signed_of(call_set(rt, ctx, word(rt, handler), {word(rt, handler + 4u), state, 0u})) < 0) return fail(0x15u);
        } else {
            const std::uint32_t mask = word(rt, entry + 4u) - 1u;
            const std::uint32_t cursor = word(rt, sp(0x30u));
            const std::uint32_t at = (mask + cursor) & ~mask;
            put(rt, entry + 8u, at);
            const std::uint32_t room = word(rt, sp(0x34u)) - (at - cursor);
            put(rt, sp(0x30u), at);
            put(rt, sp(0x34u), room);
            if (room < bytes) return fail(3u);
            put(rt, sp(0u), at);
            put(rt, sp(4u), bytes);
            if (signed_of(read(sp(0u))) < 0) return fail(0x15u);
        }
        position = bytes + word(rt, sp(0x38u));
        put(rt, sp(0x38u), position);
    }
    const std::uint32_t used = word(rt, sp(0x30u)) - buffer;
    const std::uint32_t closed = call_set(rt, ctx, 0x08B0C71Cu, {loader});
    if (signed_of(closed) >= 0) return finish(used);
    finish(signed_of(used) >= 0 ? closed : used);
}

// func_0881FEF4(-, count, args): a script native of five arguments (fewer: nothing). The first argument's text (its
// slot 0xC, through func_08B88F44) names a file whose 0x3C-byte information func_08B10204 reads into the frame; an
// empty result (+0 clear) makes a fresh list (func_08A32A94 / func_08A32350 from its data and size, the data freed by
// func_08B35254, func_08A40C24), otherwise func_08A40C14(list, 0x08CF6ED8). The second and third arguments look the
// result up through the list's slot 0x70 (func_08A40CF8, func_08A41258); func_0881FAF8 is told the element count
// (func_08A40BF8). For each element func_08A419E0 gives its fields, the fourth and fifth arguments name two of them
// (slot 0x9C): the first's text becomes a 64-bit number (func_08930170), the second a float (its slot 0x54), and
// func_0881FB28(index, &{number, float, index}) keeps them. Every holder is released last (func_08A5B9EC); an empty
// one is reported first through func_08A5BB7C, as the original.
void func_0881FEF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    if (signed_of(count) < 5) return;
    // The object a holder slot keeps, reported through func_08A5BB7C(report) when empty.
    const auto held = [&](std::uint32_t slot, std::uint32_t report) {
        const std::uint32_t object = word(rt, slot);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {report});
        return object;
    };
    const auto destroy = [&](std::uint32_t slot) { (void)call_set(rt, ctx, kDestroy, {slot}); };
    const auto copy_arg = [&](std::uint32_t slot, std::uint32_t index) { (void)call_set(rt, ctx, 0x08A76ABCu, {slot, args + index * 4u}); };

    const std::uint32_t path = held(args, 0x08C99294u);
    (void)call_virtual_at(rt, ctx, path, 0xCu, {sp(0x18u), path});
    (void)call_set(rt, ctx, 0x08B88F44u, {sp(0x14u), sp(0x18u)});
    (void)call_set(rt, ctx, 0x08B10204u, {sp(0x90u), word(rt, sp(0x14u)), 1u});
    for (std::uint32_t at = 0u; at < 0x3Cu; at += 4u) put(rt, sp(0x48u) + at, word(rt, sp(0x90u) + at));
    if (word(rt, sp(0x48u)) != 0u) {
        (void)call_set(rt, ctx, 0x08A40C14u, {sp(0x1Cu), 0x08CF6ED8u});
    } else {
        (void)call_set(rt, ctx, 0x08A32A94u, {sp(0x38u)});
        const std::uint32_t data = word(rt, sp(0x68u));
        const std::uint32_t made = held(sp(0x38u), 0x08C9931Cu);
        (void)call_set(rt, ctx, 0x08A32350u, {made, data, word(rt, sp(0x58u)), 1u});
        (void)call_set(rt, ctx, 0x08B35254u, {data});
        const std::uint32_t list = held(sp(0x38u), 0x08C9931Cu);
        (void)call_set(rt, ctx, 0x08A40C24u, {sp(0x1Cu), list + 0x14u});
        destroy(sp(0x38u));
    }
    release_string(rt, ctx, sp(0x14u), 0x08D74FD0u, 0x08B8542Cu, sp(2u));
    copy_arg(sp(0x20u), 1u);
    std::uint32_t source = held(sp(0x1Cu), 0x08C99324u);
    (void)call_virtual_at(rt, ctx, source, 0x70u, {sp(0x10u), source, sp(0x20u)});
    (void)call_set(rt, ctx, 0x08A40CF8u, {sp(0x24u), sp(0x10u)});
    destroy(sp(0x10u));
    (void)call_set(rt, ctx, 0x08A41258u, {sp(0x28u), held(sp(0x24u), 0x08C99330u), 0u});
    copy_arg(sp(0x2Cu), 2u);
    source = held(sp(0x28u), 0x08C99324u);
    (void)call_virtual_at(rt, ctx, source, 0x70u, {sp(8u), source, sp(0x2Cu)});
    (void)call_set(rt, ctx, 0x08A40CF8u, {sp(0xCu), sp(8u)});
    (void)call_set(rt, ctx, kAssign, {sp(0x24u), word(rt, sp(0xCu))});
    destroy(sp(0xCu));
    destroy(sp(8u));
    const std::uint32_t elements = call_set(rt, ctx, 0x08A40BF8u, {held(sp(0x24u), 0x08C99330u)});
    (void)call_set(rt, ctx, 0x0881FAF8u, {elements});
    copy_arg(sp(0x30u), 3u);
    copy_arg(sp(0x34u), 4u);
    for (std::uint32_t index = 0u; signed_of(elements) > 0 && index != elements; ++index) {
        (void)call_set(rt, ctx, 0x08A41258u, {sp(4u), held(sp(0x24u), 0x08C99330u), index});
        (void)call_set(rt, ctx, kAssign, {sp(0x28u), word(rt, sp(4u))});
        destroy(sp(4u));
        (void)call_set(rt, ctx, 0x08A419E0u, {sp(0x38u), held(sp(0x28u), 0x08C99324u)});
        std::uint32_t fields = held(sp(0x38u), 0x08C99340u);
        (void)call_virtual_at(rt, ctx, fields, 0x9Cu, {sp(0x3Cu), fields, sp(0x30u)});
        const std::uint32_t first = held(sp(0x3Cu), 0x08C99294u);
        (void)call_virtual_at(rt, ctx, first, 0xCu, {sp(0x40u), first});
        const GuestResult number = call_keep(rt, ctx, 0x08930170u, {word(rt, sp(0x40u))}, {});
        fields = held(sp(0x38u), 0x08C99340u);
        (void)call_virtual_at(rt, ctx, fields, 0x9Cu, {sp(0x44u), fields, sp(0x34u)});
        const std::uint32_t second = held(sp(0x44u), 0x08C99294u);
        (void)call_virtual_at(rt, ctx, second, 0x54u, {second});
        put_float(rt, sp(0x50u), ctx.fpr[0]);
        put(rt, sp(0x48u), number.v0);
        put(rt, sp(0x4Cu), number.v1);
        put(rt, sp(0x54u), index);
        (void)call_set(rt, ctx, 0x0881FB28u, {index, sp(0x48u)});
        destroy(sp(0x44u));
        release_string(rt, ctx, sp(0x40u), 0x08D74FD0u, 0x08B8542Cu, sp(1u));
        destroy(sp(0x3Cu));
        destroy(sp(0x38u));
    }
    for (const std::uint32_t at : {0x34u, 0x30u, 0x2Cu, 0x28u, 0x24u, 0x20u, 0x1Cu}) destroy(sp(at));
    release_string(rt, ctx, sp(0x18u), 0x08D74FD0u, 0x08B8542Cu, sp(0u));
}

// func_08BD7974(request): a request's receiving step (state +4: 0 starts it with func_08AC0764 and becomes 0x6A; 0x6A
// waits until the transfer (+8)->+0xA8 is done). Its size (+0xB8) goes to +0x6C; func_08AC03E0 reads the 0xC4-byte header
// to +0x78, the body size (+0xE4, or the whole size) to +0x70 and the rest to +0x74. A body buffer of +0x70 bytes from
// the pool (func_08B2CB18 on the store func_089624EC(+0x44) gives, under its lock +0x6C) becomes a shared object (a
// 0x18-byte control block, vtable 0x08CC2F90, holding the buffer and the store; func_08BBD3B4 frees the buffer when
// that fails, func_08B8E380 links it) kept at +0x13C / +0x140; the rest gets one from func_08BD75B4(.., store
// func_089624F4(+0x44), size, 0x80) at +0x144 / +0x148. The header is copied to the body, both parts are read
// (func_08AC03E0), func_08AC0618 runs, and the request is handed on: func_0890AE00(+0x68, ...) when set (refusal ends
// here), then func_08909FC8(+0x40, &body, &rest). The frame's own shared pair from func_08BD7468 is released last.
void func_08BD7974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    (void)call_set(rt, ctx, 0x08BD7468u, {sp(0x10u), self});
    (void)call_virtual_at(rt, ctx, self, 0xCu, {self, 0u});
    const auto done = [&]() { release_control(rt, ctx, word(rt, sp(0x14u))); };
    const std::uint32_t state = word(rt, self + 4u);
    if (state == 0u) {
        put(rt, self + 4u, 0x6Au);
        (void)call_set(rt, ctx, 0x08AC0764u, {self, 0u, word(rt, self + 0x54u), 1u, word(rt, self + 0x64u), sp(0x10u)});
        return done();
    }
    if (state != 0x6Au) return done();
    const std::uint32_t transfer = word(rt, self + 8u) + 0xA8u;
    if (word(rt, transfer) != 0u) return done();
    put(rt, self + 0x6Cu, word(rt, transfer + 0x10u));
    const std::uint32_t header = self + 0x78u;
    if (call_set(rt, ctx, 0x08AC03E0u, {self, header, 0xC4u, 0u}) == 0u) return done();
    const std::uint32_t body_size = word(rt, self + 0xE4u);
    put(rt, self + 0x70u, body_size);
    if (body_size == 0u) put(rt, self + 0x70u, word(rt, self + 0x6Cu));
    put(rt, self + 0x74u, word(rt, self + 0x6Cu) - word(rt, self + 0x70u));
    const std::uint32_t stores = self + 0x44u;
    const std::uint32_t store = call_set(rt, ctx, 0x089624ECu, {stores});
    const std::uint32_t pool = word(rt, store);
    const std::uint32_t body = self + 0x13Cu;
    if (pool == 0u) {
        put(rt, sp(8u), 0u);
        put(rt, sp(0xCu), 0u);
    } else {
        (void)call_set(rt, ctx, 0x08B34888u, {pool + 0x6Cu});
        const std::uint32_t buffer = call_set(rt, ctx, 0x08B2CB18u, {pool + 8u, word(rt, self + 0x70u), 0x80u});
        (void)call_set(rt, ctx, 0x08B34B24u, {pool + 0x6Cu});
        const std::uint32_t owner_control = word(rt, store + 4u);
        const std::uint32_t owner = word(rt, store);
        if (owner_control != 0u) {  // a local copy and two for the control block's arguments
            count_up(rt, owner_control);
            count_up(rt, owner_control);
            count_up(rt, owner_control);
        }
        put(rt, sp(8u), buffer);
        const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, control + 4u, 1u);
        put(rt, control, 0x08CC2F90u);
        put(rt, control + 8u, 1u);
        put(rt, control + 0xCu, buffer);
        put(rt, control + 0x10u, owner);
        put(rt, control + 0x14u, owner_control);
        if (owner_control != 0u) count_up(rt, owner_control);
        put(rt, sp(0xCu), control);
        release_control(rt, ctx, owner_control);
        if (word(rt, sp(0xCu)) == 0u) (void)call_set(rt, ctx, 0x08BBD3B4u, {owner, buffer});
        release_control(rt, ctx, owner_control);
        (void)call_set(rt, ctx, 0x08B8E380u, {sp(0xCu), buffer, buffer});
        release_control(rt, ctx, owner_control);
    }
    // body = the new pair (a counted copy, assigned, then the copy and the original released)
    const std::uint32_t made = word(rt, sp(0xCu));
    if (made != 0u) count_up(rt, made);
    put(rt, body, word(rt, sp(8u)));
    assign_control(rt, ctx, body + 4u, made);
    release_control(rt, ctx, made);
    release_control(rt, ctx, word(rt, sp(0xCu)));
    if (word(rt, self + 0x74u) != 0u) {
        const std::uint32_t source = call_set(rt, ctx, 0x089624F4u, {stores});
        (void)call_set(rt, ctx, 0x08BD75B4u, {sp(0u), source, word(rt, self + 0x74u), 0x80u});
        put(rt, self + 0x144u, word(rt, sp(0u)));
        assign_control(rt, ctx, self + 0x148u, word(rt, sp(4u)));
        release_control(rt, ctx, word(rt, sp(4u)));
    }
    const std::uint32_t target = word(rt, self + 0x13Cu);
    for (std::uint32_t at = 0u; at < 0xC4u; at += 4u) put(rt, target + at, word(rt, header + at));
    if (call_set(rt, ctx, 0x08AC03E0u, {self, word(rt, self + 0x13Cu) + 0xC4u, word(rt, self + 0x70u) - 0xC4u, 0u}) == 0u) return done();
    if (word(rt, self + 0x74u) != 0u &&
        call_set(rt, ctx, 0x08AC03E0u, {self, word(rt, self + 0x144u), word(rt, self + 0x74u), 1u}) == 0u)
        return done();
    (void)call_set(rt, ctx, 0x08AC0618u, {self});
    const std::uint32_t handler = word(rt, self + 0x68u);
    if (handler != 0u && call_set(rt, ctx, 0x0890AE00u, {handler, word(rt, self + 0x13Cu), word(rt, self + 0x70u),
                                                          word(rt, self + 0x144u), word(rt, self + 0x74u)}) == 0u)
        return done();
    (void)call_set(rt, ctx, 0x08909FC8u, {word(rt, self + 0x40u), body, self + 0x144u});
    done();
}

} // namespace hot_batch_134
} // namespace gt::game

#define HOT134(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_134::name, gt::ReturnKind::kind)
HOT134(0x088DDB6Cu, func_088DDB6C, Void);
// HOT134(0x08B15FC0u, func_08B15FC0, Word);  // batch 134: compared 0 in the fast and the full gate
// HOT134(0x0881FEF4u, func_0881FEF4, Word);  // batch 134: compared 0 in the fast and the full gate
HOT134(0x08BD7974u, func_08BD7974, Void);
