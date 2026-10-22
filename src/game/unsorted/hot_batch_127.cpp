// Hot batch 127: functions of 350-420 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_127 {
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


// func_089EB580(self, input, event): key handling of a list view; func_0896ED94(self, input, event) answers first.
// The key event+0x28 (X11 keysym values) moves the cursor under the lock D_08CF575C: up func_089E7E4C (0xFF52,
// 0xFF97, 0xFEE4, 0xFEE5), down func_089E7D1C (0xFF54, 0xFF99, 0xFEE6, 0xFEE7) by the vertical step, left
// func_089E80A4 (0xFF51, 0xFF96) and right func_089E7F74 (0xFF53, 0xFF98) by the horizontal one. A step is
// self+0x12C, or self+0x130 once the key has been held (event+0x3C set) and the position func_089E4EA0 (horizontal) /
// func_089E4EC8 (vertical) moved 10 or more from the one stored at self+0x1AC / +0x1B0 (stored when the key is first
// pressed). A move calls func_089EB244(self, input), sets self+0x1D8 and plays the sound named at self+0x1E0
// (func_08A36C3C's object, func_08A34DA8) and answers 1; no move stores both positions again and answers 0 — 1
// anyway for a held key of either axis.
void func_089EB580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t input = ctx.gpr[5];
    const std::uint32_t event = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t handled = call_set(rt, ctx, 0x0896ED94u, {self, input, event});
    if (handled != 0u) {
        ctx.gpr[2] = handled;
        return;
    }
    const std::uint32_t key = word(rt, event + 0x28u);
    const bool held = byte_u(rt, event + 0x3Cu) != 0u;
    const bool diagonal = key - 0xFEE4u < 4u;
    const bool up = key == 0xFF52u || key == 0xFF97u || key == 0xFEE4u || key == 0xFEE5u;
    const bool down = key == 0xFF54u || key == 0xFF99u || key == 0xFEE6u || key == 0xFEE7u;
    const bool left = key == 0xFF51u || key == 0xFF96u;
    const bool right = key == 0xFF53u || key == 0xFF98u;
    const bool horizontal = diagonal || left || right;
    const bool vertical = up || down;
    // the step of one axis: +0x12C, or +0x130 for a held key that already moved 10 or more from `stored`
    const auto step = [&](bool axis, std::uint32_t stored, std::uint32_t position) {
        if (!axis) return word(rt, self + 0x12Cu);
        if (!held) put(rt, self + stored, call_set(rt, ctx, position, {self}));
        const std::int32_t start = signed_of(word(rt, self + stored));
        if (start < 0) return word(rt, self + 0x12Cu);
        const std::int32_t moved = signed_of(call_set(rt, ctx, position, {self})) - start;
        const std::int32_t distance = std::max(moved, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(moved)));
        return word(rt, self + (distance < 10 ? 0x12Cu : 0x130u));
    };
    const std::uint32_t across = step(horizontal, 0x1ACu, 0x089E4EA0u);
    const std::uint32_t along = step(vertical, 0x1B0u, 0x089E4EC8u);
    (void)call_set(rt, ctx, kLock, {0x08CF575Cu});
    std::uint32_t moved = 0u;
    if (up) moved = call_set(rt, ctx, 0x089E7E4Cu, {self, along});
    else if (down) moved = call_set(rt, ctx, 0x089E7D1Cu, {self, along});
    else if (left) moved = call_set(rt, ctx, 0x089E80A4u, {self, across});
    else if (right) moved = call_set(rt, ctx, 0x089E7F74u, {self, across});
    (void)call_set(rt, ctx, kUnlock, {0x08CF575Cu});
    std::uint32_t result;
    if (moved != 0u) {
        (void)call_set(rt, ctx, 0x089EB244u, {self, input});
        put(rt, self + 0x1D8u, 1u);
        if (word(rt, word(rt, self + 0x1E0u) - 0xCu) != 0u) {
            (void)call_set(rt, ctx, 0x08A36C3Cu, {frame.at(0u)});
            const std::uint32_t sound = word(rt, frame.at(0u));
            if (sound == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7C9Cu});
            (void)call_set(rt, ctx, 0x08A34DA8u, {sound, word(rt, self + 0x1E0u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
        result = 1u;
    } else {
        if (horizontal) put(rt, self + 0x1ACu, call_set(rt, ctx, 0x089E4EA0u, {self}));
        if (vertical) put(rt, self + 0x1B0u, call_set(rt, ctx, 0x089E4EC8u, {self}));
        result = 0u;
    }
    if ((horizontal || vertical) && held) result = 1u;
    ctx.gpr[2] = result;
}

// func_08A25498(self): the script module "MUnit" (D_08CAAFCC) registers its 28 methods, as func_08A2982C does: a string
// from D_08CAAFCC (func_08A69B3C(sp, ..)), a 0x38-byte block (func_08A5BC20(0x38, 4)) built by func_08A7E914(block,
// sp) and stored at sp[4], func_08A76AB4(self, sp + 4) takes it and func_08A69D68(sp) releases the string; then each
// (name, handler) pair, from "GetCourseLengthString" to "GetUnitKeywordOfCourseHeight", goes to
// func_08B86FB0(self[0], name, handler) (a zero self[0] first calls func_08A5BB7C("HModule")). v0 = self.
void func_08A25498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAAFCCu});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {block, frame.at(0u)});
    put(rt, frame.at(4u), block);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    struct Pair {
        std::uint32_t name, handler;
    };
    static constexpr Pair kPairs[28] = {
        {0x08CAAFDCu, 0x08A26940u}, {0x08CAAFF4u, 0x08A26744u}, {0x08CAB00Cu, 0x08A265F8u}, {0x08CAB020u, 0x08A25428u},
        {0x08CAB038u, 0x08A25A48u}, {0x08CAB050u, 0x08A25A9Cu}, {0x08CAB068u, 0x08A26548u}, {0x08CAB07Cu, 0x08A25B54u},
        {0x08CAB098u, 0x08A252E8u}, {0x08CAB0ACu, 0x08A25B9Cu}, {0x08CAB0C0u, 0x08A26498u}, {0x08CAB0D0u, 0x08A25C34u},
        {0x08CAB0E8u, 0x08A25338u}, {0x08CAB0F8u, 0x08A25C7Cu}, {0x08CAB10Cu, 0x08A25D14u}, {0x08CAB124u, 0x08A25DA4u},
        {0x08CAB13Cu, 0x08A25388u}, {0x08CAB14Cu, 0x08A263E8u}, {0x08CAB15Cu, 0x08A26340u}, {0x08CAB16Cu, 0x08A26290u},
        {0x08CAB17Cu, 0x08A261E0u}, {0x08CAB194u, 0x08A25DECu}, {0x08CAB1B0u, 0x08A253D8u}, {0x08CAB1C8u, 0x08A26124u},
        {0x08CAB1E4u, 0x08A26068u}, {0x08CAB1FCu, 0x08A25FACu}, {0x08CAB214u, 0x08A25EF0u}, {0x08CAB234u, 0x08A25E34u}};
    for (const Pair &pair : kPairs) {
        const std::uint32_t target = word(rt, self);
        ctx.gpr[5] = 0x004A0000u;  // the delay slot of the bnez
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFD4u});
        (void)call_set(rt, ctx, 0x08B86FB0u, {target, pair.name, pair.handler});
    }
    ctx.gpr[2] = self;
}

// func_088467D4(out, owner, set, value): a script property of the game condition (literal "MGameCondition"): a record of
// a kind and three integers, func_08894100 (read) / func_0889410C (write) of func_08845798 on func_088458D0's
// object. Reading (`set` 0) builds [kind, [v1, v2, v3]] (func_08A98C1C arrays of func_08A85B1C integers; the inner
// array only for kinds 1-4) into `out`. Writing takes the array `value` (func_08A98674): the kind from its element 0
// and, for kinds 1-4 and a non-empty element 1, up to three integers from element 1 through slot 0x3C of each.
void func_088467D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t set = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    const auto checked = [&](std::uint32_t object, std::uint32_t message) {
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    const auto condition = [&](std::uint32_t holder) {
        const std::uint32_t object = checked(word(rt, holder), 0x08C9CA90u);
        const std::uint32_t result = call_set(rt, ctx, 0x08845798u, {object});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return result;
    };
    // array element `slot` = an integer value of `number`, built in the temporary at `temp`
    const auto store_int = [&](std::uint32_t slot, std::uint32_t number, std::uint32_t temp) {
        (void)call_set(rt, ctx, 0x08A85B1Cu, {temp, number});
        if (slot != temp) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
    };
    if (set == 0u) {
        (void)call_set(rt, ctx, 0x088458D0u, {frame.at(0x38u), owner});
        const std::uint32_t source = condition(frame.at(0x38u));
        const std::uint32_t outer = frame.at(0x40u);
        const std::uint32_t record = call_set(rt, ctx, 0x08894100u, {source});
        (void)call_set(rt, ctx, 0x08A98C1Cu, {outer, 2u});
        store_int(word(rt, word(rt, outer) + 0x14u), word(rt, record), frame.at(0x34u));
        const std::uint32_t kind = word(rt, record);
        if (kind - 1u < 4u) {
            static constexpr std::uint32_t kTemps[4][3] = {{0x30u, 0x2Cu, 0x28u}, {0x24u, 0x20u, 0x1Cu}, {0x18u, 0x14u, 0x10u}, {0xCu, 8u, 4u}};
            const std::uint32_t inner = frame.at(0x3Cu);
            (void)call_set(rt, ctx, 0x08A98C1Cu, {inner, 3u});
            for (std::uint32_t k = 0u; k != 3u; ++k) {
                const std::uint32_t slot = word(rt, word(rt, inner) + 0x14u) + 4u * k;
                store_int(slot, word(rt, record + 4u + 4u * k), frame.at(kTemps[kind - 1u][k]));
            }
            const std::uint32_t slot = word(rt, word(rt, outer) + 0x14u) + 4u;
            if (slot != inner) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, inner)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {inner});
        }
        if (out != outer) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, outer)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {outer});
        return;
    }
    const std::uint32_t array = frame.at(0x3Cu);
    (void)call_set(rt, ctx, 0x08A98674u, {array, value});
    (void)call_set(rt, ctx, 0x088458D0u, {frame.at(0u), owner});
    const std::uint32_t source = condition(frame.at(0u));
    const std::uint32_t record = call_set(rt, ctx, 0x0889410Cu, {source});
    const auto integer = [&](std::uint32_t element) {
        const std::uint32_t object = checked(element, 0x08C9CA54u);
        return call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    };
    const std::uint32_t kind = integer(word(rt, word(rt, word(rt, array) + 0x14u)));
    put(rt, record, kind);
    const std::uint32_t data = word(rt, word(rt, array) + 0x14u);
    if (word(rt, data + 4u) != 0u && kind - 1u < 4u) {
        const std::uint32_t inner = frame.at(0x40u);
        (void)call_set(rt, ctx, 0x08A98674u, {inner, data + 4u});
        for (std::uint32_t k = 0u; k != 3u; ++k) {
            const std::uint32_t holder = checked(word(rt, inner), 0x08C9CB60u);
            const std::uint32_t items = static_cast<std::uint32_t>(signed_of(word(rt, holder + 0x18u) - word(rt, holder + 0x14u)) >> 2);
            if (items <= k) continue;
            put(rt, record + 4u + 4u * k, integer(word(rt, word(rt, word(rt, inner) + 0x14u) + 4u * k)));
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {inner});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {array});
}

} // namespace hot_batch_127
} // namespace gt::game

#define HOT127(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_127::name, gt::ReturnKind::kind)
HOT127(0x089EB580u, func_089EB580, Word);
HOT127(0x08A25498u, func_08A25498, Word);
HOT127(0x088467D4u, func_088467D4, Word);
