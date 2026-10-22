// Hot batch 125: functions of 300-450 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_125 {
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

// The 0x4C-byte ordering state func_08A16708 sorts focus entries with (passed by value to the sort helpers): words +0
// and +0x34 / +0x38, floats +0x4..0xC and +0x14..0x30, bytes +0x10 / +0x11, a vector of 16-byte items at +0x3C
// (begin, end, capacity) and a word at +0x48. Its copy constructor writes the fields (not the padding after the
// bytes), then the vector (zeroed, func_08A5BC20 memory for the used bytes, items copied by func_08C15F9C with the
// allocator byte at `scratch`), then +0x48.
void copy_order_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t to, std::uint32_t from, std::uint32_t scratch) {
    put(rt, to, word(rt, from));
    for (std::uint32_t k = 4u; k != 0x10u; k += 4u) put(rt, to + k, word(rt, from + k));
    put8(rt, to + 0x10u, byte_u(rt, from + 0x10u));
    put8(rt, to + 0x11u, byte_u(rt, from + 0x11u));
    for (std::uint32_t k = 0x14u; k != 0x3Cu; k += 4u) put(rt, to + k, word(rt, from + k));
    for (std::uint32_t k = 0x3Cu; k != 0x48u; k += 4u) put(rt, to + k, 0u);
    const std::uint32_t first = word(rt, from + 0x3Cu), last = word(rt, from + 0x40u);
    const std::uint32_t bytes = (last - first) & ~0xFu;
    const std::uint32_t memory = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    put(rt, to + 0x44u, memory + bytes);
    put(rt, to + 0x3Cu, memory);
    put(rt, to + 0x40u, memory);
    put(rt, to + 0x40u, call_set(rt, ctx, 0x08C15F9Cu, {first, last, memory, scratch}));
    put(rt, to + 0x48u, word(rt, from + 0x48u));
}
// Its destructor: func_08BF5B28(begin, end, scratch) on the items, then func_08A5BC54(begin, capacity bytes, 4) unless
// the vector has no memory (v0 is then the capacity in items, as the asm leaves it).
void destroy_order_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t scratch) {
    (void)call_set(rt, ctx, 0x08BF5B28u, {word(rt, object + 0x3Cu), word(rt, object + 0x40u), scratch});
    const std::uint32_t memory = word(rt, object + 0x3Cu);
    const std::uint32_t items = static_cast<std::uint32_t>(signed_of(word(rt, object + 0x44u) - memory) >> 4);
    if (memory == 0u) {
        ctx.gpr[2] = items;
        return;
    }
    (void)call_set(rt, ctx, 0x08A5BC54u, {memory, items << 4, 4u});
}
// A 0x2C-byte focus entry is plain data (11 words); its assignment copies them all.
void copy_entry(psprecomp::Runtime &rt, std::uint32_t to, std::uint32_t from) {
    for (std::uint32_t k = 0u; k != 0x2Cu; k += 4u) put(rt, to + k, word(rt, from + k));
}

} // namespace


// func_089F3108(self, force): func_089E1C90(self, force & 0xFF), then for each of the eight name strings at
// self+0x22C..0x248 the value object at self+0x24C..0x268 is (re)built when `force` or the slot is still 0: a non-empty
// name goes through func_089A50A0(temp, func_089BD4DC(self), &name, 0, 0, 1), an empty one gives
// func_089E4C14(temp, D_08CF6ED8) (as func_089F37AC does); func_08A5BAD8(slot, *temp) stores it and func_08A5B9EC
// destroys the temporary (sp + 0x3C - 8k for a name, sp + 0x38 - 8k for the default).
// v0 = the last call's answer, or the last slot when it is kept.
void func_089F3108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t force = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    (void)call_set(rt, ctx, 0x089E1C90u, {self, force});
    for (std::uint32_t k = 0u; k != 8u; ++k) {
        const std::uint32_t name = self + 0x22Cu + 4u * k;
        const std::uint32_t slot = self + 0x24Cu + 4u * k;
        if (force == 0u && word(rt, slot) != 0u) {
            ctx.gpr[2] = word(rt, slot);
            continue;
        }
        std::uint32_t temp;
        if (word(rt, word(rt, name) - 0xCu) != 0u) {
            temp = frame.at(0x3Cu - 8u * k);
            const std::uint32_t owner = call_set(rt, ctx, 0x089BD4DCu, {self});
            (void)call_set(rt, ctx, 0x089A50A0u, {temp, owner, name, 0u, 0u, 1u});
        } else {
            temp = frame.at(0x38u - 8u * k);
            (void)call_set(rt, ctx, 0x089E4C14u, {temp, 0x08CF6ED8u});
        }
        (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
}

// func_089A3930(self, input, event): key handling of a focus view; func_0896ED94(self, input, event) answers first
// (non-zero = handled). The key code event+0x28 uses X11 keysym values (the diagonals 0xFEE4..0xFEE7 move by
// (+-0.7071, +-0.7071) in the order UpLeft, UpRight, DownLeft, DownRight): 0xFF0D and 0xFF1B take the target view
// (func_089A3130(self), else self) through func_08A11BF0 / func_08A0BE48 into a temporary and answer 2 when its slot
// 0x108 does, else 0. The others move the focus with func_089A3868(self, mode, f12 = dx, f13 = dy) (arrows and the
// keypad arrows by one unit, 0xFEFA by the event's (+0x34, +0x38), 0xFF09 mode 1, or 2 with event+0x2C bit 0x20, and
// 0xFE20 mode 2): pointer moves need self+0x138 bit 0x20, the others are refused while func_089AA910(input) is set
// and bit 0x80 is; a move that happened hands func_08A36C3C's object to func_08A34DA8(object, self+0x128). Answers 1
// once func_089A3868 ran, 0 for other keys.
void func_089A3930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t input = ctx.gpr[5];
    const std::uint32_t event = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t handled = call_set(rt, ctx, 0x0896ED94u, {self, input, event});
    if (handled != 0u) {
        ctx.gpr[2] = handled;
        return;
    }
    const auto answer = [&](std::uint32_t value) { ctx.gpr[2] = value; };
    const auto locked = [&] {
        return call_set(rt, ctx, 0x089AA910u, {input}) != 0u && (word(rt, self + 0x138u) & 0x80u) != 0u;
    };
    const auto move = [&](std::uint32_t mode, float dx, float dy, std::uint32_t temp_offset) {
        if (call_setf(rt, ctx, 0x089A3868u, {self, mode}, {dx, dy}).v0 == 0u) return answer(1u);
        const std::uint32_t temp = frame.at(temp_offset);
        (void)call_set(rt, ctx, 0x08A36C3Cu, {temp});
        const std::uint32_t object = word(rt, temp);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4D80u});
        (void)call_set(rt, ctx, 0x08A34DA8u, {object, word(rt, self + 0x128u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
        answer(1u);
    };
    const auto step = [&](float dx, float dy, std::uint32_t temp_offset) {
        if (locked()) return answer(0u);
        move(0u, dx, dy, temp_offset);
    };
    const auto pointer = [&](float dx, float dy, std::uint32_t temp_offset) {
        if ((word(rt, self + 0x138u) & 0x20u) == 0u) return answer(0u);
        move(0u, dx, dy, temp_offset);
    };
    const auto activate = [&](std::uint32_t build, std::uint32_t message) {
        const std::uint32_t target = call_set(rt, ctx, 0x089A3130u, {self});
        const std::uint32_t temp = frame.at(0x2Cu);
        (void)call_set(rt, ctx, build, {temp, input, target != 0u ? target : self});
        const std::uint32_t object = word(rt, temp);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
        const std::uint32_t result = call_set(rt, ctx, word(rt, word(rt, object) + 0x108u), {object}) != 0u ? 2u : 0u;
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
        answer(result);
    };
    const float diagonal = std::bit_cast<float>(0x3F3504F3u);
    switch (word(rt, event + 0x28u)) {
    case 0xFF0Du: return activate(0x08A11BF0u, 0x08CA4D34u);
    case 0xFF1Bu: return activate(0x08A0BE48u, 0x08CA4D44u);
    case 0xFF52u:
    case 0xFF97u: return step(0.0f, -1.0f, 0x28u);
    case 0xFF54u:
    case 0xFF99u: return step(0.0f, 1.0f, 0x24u);
    case 0xFF51u:
    case 0xFF96u: return step(-1.0f, 0.0f, 0x20u);
    case 0xFF53u:
    case 0xFF98u: return step(1.0f, 0.0f, 0x1Cu);
    case 0xFEFAu: return pointer(float_at(rt, event + 0x34u), float_at(rt, event + 0x38u), 0x18u);
    case 0xFEE4u: return pointer(-diagonal, -diagonal, 0x14u);
    case 0xFEE5u: return pointer(diagonal, -diagonal, 0x10u);
    case 0xFEE6u: return pointer(-diagonal, diagonal, 0xCu);
    case 0xFEE7u: return pointer(diagonal, diagonal, 8u);
    case 0xFF09u:
        if (locked()) return answer(0u);
        return move((word(rt, event + 0x2Cu) & 0x20u) != 0u ? 2u : 1u, 0.0f, 0.0f, 4u);
    case 0xFE20u:
        if (locked()) return answer(0u);
        return move(2u, 0.0f, 0.0f, 0u);
    default: return answer(0u);
    }
}

// func_08A3BE8C(out, script, count, holder): a script method that lists music tracks (literals "MMusic", "HMap" and
// the keys below). Nothing when count <= 0. The held object's slot 0xC gives a string (sp+0x78), func_08A3A9D0 the
// MMusic object whose +0x14 is the track table; func_08ABCAA4(table, string) the number of tracks and func_08A98C1C an
// array of that size (sp+0x7C). Each track k becomes a map (func_08A813F4, sp+0x80) whose slot 0xA0 stores
// "index" = func_08A85B1C(k) and the strings "code" / "file" / "title" / "artist" / "genre" from func_08ABCB0C /
// func_08ABCB44 / func_08ABC8F4 / func_08ABCAD4 / func_08ABCB7C(table, string, k) ("" for a null answer), keys and
// strings made by func_08B85EB8 + func_08A6D2D4 and released in the asm's order; the map goes to array[k]
// (func_08A5BAD8). The array is copied to `out` the same way, then the temporaries are destroyed.
void func_08A3BE8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t script = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0xD0u);
    if (signed_of(count) <= 0) return;
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kRelease = 0x08B8542Cu;
    const auto checked = [&](std::uint32_t object, std::uint32_t message) {
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    const std::uint32_t held = checked(word(rt, holder), 0x08CAD518u);
    (void)call_set(rt, ctx, word(rt, word(rt, held) + 0xCu), {frame.at(0x78u), held});
    const std::uint32_t name = word(rt, frame.at(0x78u));
    (void)call_set(rt, ctx, 0x08A3A9D0u, {frame.at(0x74u), script});
    const std::uint32_t table = word(rt, checked(word(rt, frame.at(0x74u)), 0x08CAD544u) + 0x14u);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x74u)});
    const std::uint32_t tracks = call_set(rt, ctx, 0x08ABCAA4u, {table, name});
    const std::uint32_t array = frame.at(0x7Cu);
    (void)call_set(rt, ctx, 0x08A98C1Cu, {array, tracks});
    const std::uint32_t map = frame.at(0x80u);
    // map[key] = value through the map's slot 0xA0: the key's string and value object at sp+at, sp+at+4, its
    // allocator byte at sp+scratch; the fields' strings sit at sp+at-8 / sp+at-4 and release with sp+scratch-0xB / -0xC
    const auto set = [&](std::uint32_t key, std::uint32_t at, std::uint32_t scratch) {
        const std::uint32_t object = checked(word(rt, map), 0x08CAD674u);
        const std::uint32_t store = word(rt, word(rt, object) + 0xA0u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(at), key, frame.at(scratch)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {frame.at(at + 4u), frame.at(at)});
        return std::pair{object, store};
    };
    for (std::uint32_t k = 0u; k != tracks; ++k) {
        (void)call_set(rt, ctx, 0x08A813F4u, {map});
        {
            const auto [object, store] = set(0x08CAD67Cu, 0x6Cu, 0x16u);
            (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x68u), k});
            (void)call_set(rt, ctx, store, {object, frame.at(0x70u), frame.at(0x68u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x68u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x70u)});
            release_string(rt, ctx, frame.at(0x6Cu), kEmpty, kRelease, frame.at(0xBu));
        }
        struct Field {
            std::uint32_t getter, key, at, scratch;
        };
        static constexpr Field kFields[] = {{0x08ABCB0Cu, 0x08CAD684u, 0x60u, 0x15u},
                                            {0x08ABCB44u, 0x08CAD68Cu, 0x50u, 0x13u},
                                            {0x08ABC8F4u, 0x08CAD694u, 0x40u, 0x11u},
                                            {0x08ABCAD4u, 0x08CAD69Cu, 0x30u, 0xFu},
                                            {0x08ABCB7Cu, 0x08CAD6A4u, 0x20u, 0xDu}};
        for (const Field &field : kFields) {
            const std::uint32_t text = call_set(rt, ctx, field.getter, {table, name, k});
            const auto [object, store] = set(field.key, field.at, field.scratch);
            const std::uint32_t value = field.at - 8u;
            (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(value), text != 0u ? text : 0x08C8E7CCu, frame.at(field.scratch - 1u)});
            (void)call_set(rt, ctx, 0x08A6D2D4u, {frame.at(value + 4u), frame.at(value)});
            (void)call_set(rt, ctx, store, {object, frame.at(field.at + 4u), frame.at(value + 4u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(value + 4u)});
            release_string(rt, ctx, frame.at(value), kEmpty, kRelease, frame.at(field.scratch - 0xBu));
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(field.at + 4u)});
            release_string(rt, ctx, frame.at(field.at), kEmpty, kRelease, frame.at(field.scratch - 0xCu));
        }
        const std::uint32_t slot = word(rt, word(rt, array) + 0x14u) + k * 4u;
        if (slot != map) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, map)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {map});
    }
    if (out != array) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, array)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {array});
    release_string(rt, ctx, frame.at(0x78u), kEmpty, kRelease, frame.at(0u));
}

// func_08A16708(state, entries, view): moves the focus of `view` along its focusable entries (a vector of 0x2C-byte
// records, the first word an id). state+0xC = view+0x124, state+0x10 / +0x11 = bits 3 / 4 of view+0x138, state+0x34 =
// func_089A318C(view) (the focused id), state+0x38 = the first of func_089A3130(view), func_08965CE0(...)... with bit
// 0x02000000 in +0xE0 (unchanged when there is none). func_08C15E8C(entries, begin, end) and
// func_08969B98(view, &x, &y, 0, 0) / func_08A15F08(state, entries, view, 0, x, y) prepare them; empty -> 0. The
// entries are sorted with the state as comparator, copied by value three times (copy_order_state):
// func_08C18650(begin, end, 2 * floor(log2(count)), &copy) then func_08C16F58(begin, end, &copy) (an introsort). The
// new id is the first entry's (mode state+0 = 0), the one after the focused id (1; the first when it is last or
// missing), the one before it (2; the last when it is first or missing) or 0; answers 0 when it equals the focused id,
// else func_089A301C(view, id) and 1.
void func_08A16708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const std::uint32_t entries = ctx.gpr[5];
    const std::uint32_t view = ctx.gpr[6];
    GuestFrame frame(ctx, 0x130u);
    put(rt, state + 0xCu, word(rt, view + 0x124u));
    put8(rt, state + 0x10u, (word(rt, view + 0x138u) >> 3) & 1u);
    put8(rt, state + 0x11u, (word(rt, view + 0x138u) >> 4) & 1u);
    const std::uint32_t focused = call_set(rt, ctx, 0x089A318Cu, {view});
    put(rt, state + 0x34u, focused);
    for (std::uint32_t node = call_set(rt, ctx, 0x089A3130u, {view}); node != 0u; node = call_set(rt, ctx, 0x08965CE0u, {node})) {
        if ((word(rt, node + 0xE0u) & 0x02000000u) != 0u) {
            put(rt, state + 0x38u, node);
            break;
        }
    }
    (void)call_set(rt, ctx, 0x08C15E8Cu, {entries, word(rt, entries), word(rt, entries + 4u)});
    (void)call_set(rt, ctx, 0x08969B98u, {view, frame.at(8u), frame.at(0xCu), 0u, 0u});
    (void)call_setf(rt, ctx, 0x08A15F08u, {state, entries, view, 0u}, {float_at(rt, frame.at(8u)), float_at(rt, frame.at(0xCu))});
    const std::uint32_t begin = word(rt, entries);
    const std::uint32_t end = word(rt, entries + 4u);
    if (begin == end) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t comparator = frame.at(0xA8u);
    copy_order_state(rt, ctx, comparator, state, frame.at(5u));
    std::int32_t count = static_cast<std::int32_t>(((end - begin) >> 2) * 0xBA2E8BA3u);  // (end - begin) / 0x2C
    std::uint32_t depth = 0u;
    if (count != 1) {
        std::uint32_t steps = 0u;
        do {
            count >>= 1;
            ++steps;
        } while (count != 1);
        depth = steps * 2u;
    }
    copy_order_state(rt, ctx, frame.at(0x5Cu), comparator, frame.at(4u));
    (void)call_set(rt, ctx, 0x08C18650u, {begin, end, depth, frame.at(0x5Cu)});
    destroy_order_state(rt, ctx, frame.at(0x5Cu), frame.at(3u));
    copy_order_state(rt, ctx, frame.at(0x10u), comparator, frame.at(2u));
    (void)call_set(rt, ctx, 0x08C16F58u, {begin, end, frame.at(0x10u)});
    destroy_order_state(rt, ctx, frame.at(0x10u), frame.at(1u));
    destroy_order_state(rt, ctx, comparator, frame.at(0u));
    const std::uint32_t mode = word(rt, state);
    const std::uint32_t key = word(rt, state + 0x34u);
    const std::uint32_t first = word(rt, entries);
    const std::uint32_t last = word(rt, entries + 4u);
    std::uint32_t chosen = 0u;
    if (mode == 0u) {
        chosen = word(rt, first);
    } else if (mode == 1u) {
        std::uint32_t next = last;
        for (std::uint32_t entry = first; entry != last; entry += 0x2Cu) {
            if (word(rt, entry) == key) {
                next = entry + 0x2Cu;
                break;
            }
        }
        chosen = word(rt, next == last ? first : next);
    } else if (mode == 2u) {
        std::uint32_t found = first;
        for (std::uint32_t entry = last; entry != first;) {
            entry -= 0x2Cu;
            if (word(rt, entry) == key) {
                found = entry == first ? last : entry;
                break;
            }
        }
        chosen = word(rt, (found == first ? last : found) - 0x2Cu);
    }
    if (chosen == key) {
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_set(rt, ctx, 0x089A301Cu, {view, chosen});
    ctx.gpr[2] = 1u;
}

// func_08C16F58(first, last, order): libstdc++'s __final_insertion_sort over the 0x2C-byte focus entries with the
// ordering state (copy_order_state) as comparator: more than 16 entries -> func_08C16B9C(first, first + 16, &copy),
// then func_08C1683C(i, &value, &copy) (__unguarded_linear_insert) for each later entry, the value a copy of *i at
// sp+8 and a fresh comparator copy each time; otherwise func_08C16B9C(first, last, &copy).
void func_08C16F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    const std::uint32_t order = ctx.gpr[6];
    GuestFrame frame(ctx, 0x1A0u);
    const std::int32_t count = static_cast<std::int32_t>(((last - first) >> 2) * 0xBA2E8BA3u);  // (last - first) / 0x2C
    if (count < 0x11) {
        copy_order_state(rt, ctx, frame.at(0x80u), order, frame.at(1u));
        (void)call_set(rt, ctx, 0x08C16B9Cu, {first, last, frame.at(0x80u)});
        destroy_order_state(rt, ctx, frame.at(0x80u), frame.at(0u));
        return;
    }
    const std::uint32_t middle = first + 0x2C0u;
    copy_order_state(rt, ctx, frame.at(0x118u), order, frame.at(7u));
    (void)call_set(rt, ctx, 0x08C16B9Cu, {first, middle, frame.at(0x118u)});
    destroy_order_state(rt, ctx, frame.at(0x118u), frame.at(6u));
    const std::uint32_t kept = frame.at(0xCCu);
    copy_order_state(rt, ctx, kept, order, frame.at(5u));
    for (std::uint32_t entry = middle; entry != last; entry += 0x2Cu) {
        for (std::uint32_t k = 0u; k != 0x2Cu; k += 4u) put(rt, frame.at(8u + k), word(rt, entry + k));
        copy_order_state(rt, ctx, frame.at(0x34u), kept, frame.at(4u));
        (void)call_set(rt, ctx, 0x08C1683Cu, {entry, frame.at(8u), frame.at(0x34u)});
        destroy_order_state(rt, ctx, frame.at(0x34u), frame.at(3u));
    }
    destroy_order_state(rt, ctx, kept, frame.at(2u));
}

// func_08C18650(first, last, depth, order): libstdc++'s __introsort_loop over the 0x2C-byte focus entries with the
// ordering state as comparator (func_08A15D54(&copy, a, b) = a before b). While more than 16 entries remain: with no
// depth left func_08C17E04(first, last, last, &copy) (partial_sort) ends it; otherwise the median of first, middle and
// last - 1 is copied to sp+8 as the pivot, func_08C1667C(first, last, &pivot, &copy) partitions, this function sorts
// (cut, last) with depth - 1 and `last` becomes the cut. Each use takes a fresh comparator copy (copy_order_state).
// v0 = 1 when the range is short enough, else the partial sort's last call.
void func_08C18650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    std::uint32_t last = ctx.gpr[5];
    std::uint32_t depth = ctx.gpr[6];
    const std::uint32_t order = ctx.gpr[7];
    GuestFrame frame(ctx, 0x1C0u);
    const auto entries = [&](std::uint32_t end) { return static_cast<std::int32_t>(((end - first) >> 2) * 0xBA2E8BA3u); };
    std::int32_t count = entries(last);
    while (count >= 0x11) {
        if (depth == 0u) {
            copy_order_state(rt, ctx, frame.at(0x118u), order, frame.at(7u));
            (void)call_set(rt, ctx, 0x08C17E04u, {first, last, last, frame.at(0x118u)});
            destroy_order_state(rt, ctx, frame.at(0x118u), frame.at(6u));
            return;
        }
        --depth;
        const std::uint32_t middle = first + static_cast<std::uint32_t>(count / 2) * 0x2Cu;
        const std::uint32_t back = last - 0x2Cu;
        const std::uint32_t compare = frame.at(0xCCu);
        copy_order_state(rt, ctx, compare, order, frame.at(5u));
        const auto before = [&](std::uint32_t a, std::uint32_t b) { return call_set(rt, ctx, 0x08A15D54u, {compare, a, b}) != 0u; };
        std::uint32_t median;
        if (before(first, middle)) {
            median = before(middle, back) ? middle : (before(first, back) ? back : first);
        } else {
            median = before(first, back) ? first : (before(middle, back) ? back : middle);
        }
        for (std::uint32_t k = 0u; k != 0x2Cu; k += 4u) put(rt, frame.at(8u + k), word(rt, median + k));
        copy_order_state(rt, ctx, frame.at(0x80u), order, frame.at(4u));
        const std::uint32_t cut = call_set(rt, ctx, 0x08C1667Cu, {first, last, frame.at(8u), frame.at(0x80u)});
        destroy_order_state(rt, ctx, frame.at(0x80u), frame.at(3u));
        destroy_order_state(rt, ctx, compare, frame.at(2u));
        copy_order_state(rt, ctx, frame.at(0x34u), order, frame.at(1u));
        (void)call_set(rt, ctx, 0x08C18650u, {cut, last, depth, frame.at(0x34u)});
        destroy_order_state(rt, ctx, frame.at(0x34u), frame.at(0u));
        last = cut;
        count = entries(last);
    }
    ctx.gpr[2] = 1u;
}

// func_08BF5B28(begin, end): the destructor loop of a vector of trivially destructible 16-byte items: steps a0 by 16
// until it reaches `end` (and once more), nothing else.
void func_08BF5B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    ctx.gpr[4] = ctx.gpr[5] + 0x10u;
}

// func_08C1667C(first, last, pivot, order): libstdc++'s __unguarded_partition of the focus entries:
// func_08A15D54(order, a, b) is "a before b"; entries are swapped through a copy at sp+0. Answers the cut.
void func_08C1667C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t first = ctx.gpr[4];
    std::uint32_t last = ctx.gpr[5];
    const std::uint32_t pivot = ctx.gpr[6];
    const std::uint32_t order = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    for (;;) {
        while (call_set(rt, ctx, 0x08A15D54u, {order, first, pivot}) != 0u) first += 0x2Cu;
        last -= 0x2Cu;
        while (call_set(rt, ctx, 0x08A15D54u, {order, pivot, last}) != 0u) last -= 0x2Cu;
        if (!(first < last)) break;
        copy_entry(rt, frame.at(0u), first);
        copy_entry(rt, first, last);
        copy_entry(rt, last, frame.at(0u));
        first += 0x2Cu;
    }
    ctx.gpr[2] = first;
}

// func_08C1683C(last, value, order): libstdc++'s __unguarded_linear_insert: entries before `last` move up while
// func_08A15D54(order, value, previous) holds, then *last = *value. v0 = value+0x28, the last word copied.
void func_08C1683C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t last = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    const std::uint32_t order = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t previous = last - 0x2Cu; call_set(rt, ctx, 0x08A15D54u, {order, value, previous}) != 0u; previous -= 0x2Cu) {
        copy_entry(rt, last, previous);
        last = previous;
    }
    copy_entry(rt, last, value);
    ctx.gpr[2] = word(rt, value + 0x28u);
}

// func_08C16B9C(first, last, order): libstdc++'s __insertion_sort of the focus entries: each entry (copied to sp+0x30)
// that goes before the first one (func_08A15D54(order, &value, first)) shifts [first, i) up by one entry and becomes
// the first; any other goes through func_08C1683C(i, &copy at sp+4, &order copy at sp+0x5C) (copy_order_state).
// v0: order+0x3C for a single entry, value+0x28 after a move to the front, else the destructor's.
void func_08C16B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    const std::uint32_t order = ctx.gpr[6];
    GuestFrame frame(ctx, 0xF0u);
    if (first == last) return;
    ctx.gpr[2] = order + 0x3Cu;  // the delay slot of the second check
    const std::uint32_t value = frame.at(0x30u);
    for (std::uint32_t entry = first + 0x2Cu; entry != last; entry += 0x2Cu) {
        copy_entry(rt, value, entry);
        if (call_set(rt, ctx, 0x08A15D54u, {order, value, first}) != 0u) {
            for (std::uint32_t to = entry; to != first; to -= 0x2Cu) copy_entry(rt, to, to - 0x2Cu);
            copy_entry(rt, first, value);
            ctx.gpr[2] = word(rt, value + 0x28u);
            continue;
        }
        copy_entry(rt, frame.at(4u), value);
        copy_order_state(rt, ctx, frame.at(0x5Cu), order, frame.at(1u));
        (void)call_set(rt, ctx, 0x08C1683Cu, {entry, frame.at(4u), frame.at(0x5Cu)});
        destroy_order_state(rt, ctx, frame.at(0x5Cu), frame.at(0u));
    }
}

} // namespace hot_batch_125
} // namespace gt::game

#define HOT125(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_125::name, gt::ReturnKind::kind)
HOT125(0x089F3108u, func_089F3108, Word);
HOT125(0x089A3930u, func_089A3930, Word);
HOT125(0x08A3BE8Cu, func_08A3BE8C, Word);
HOT125(0x08A16708u, func_08A16708, Word);
HOT125(0x08C16F58u, func_08C16F58, Word);
HOT125(0x08C18650u, func_08C18650, Word);
HOT125(0x08BF5B28u, func_08BF5B28, Word);
HOT125(0x08C1667Cu, func_08C1667C, Word);
HOT125(0x08C1683Cu, func_08C1683C, Word);
HOT125(0x08C16B9Cu, func_08C16B9C, Word);
