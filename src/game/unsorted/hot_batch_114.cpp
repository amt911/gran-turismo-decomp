// Hot batch 114: functions of 141-185 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_114 {
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
// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// A word read at any alignment (the original's lwl/lwr pair), little-endian.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24);
}

// A word stored at any alignment (the original's swl/swr pair), little-endian.
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (index * 8u));
}
// Copies `bytes` (a multiple of 16) from `source` (any alignment) to the aligned `destination`, 16 bytes at a time: four
// words loaded, then four stored.
inline void copy_blocks(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t bytes) {
    for (std::uint32_t offset = 0u; offset != bytes; offset += 16u) {
        const std::uint32_t a = word_unaligned(rt, source + offset);
        const std::uint32_t b = word_unaligned(rt, source + offset + 4u);
        const std::uint32_t c = word_unaligned(rt, source + offset + 8u);
        const std::uint32_t d = word_unaligned(rt, source + offset + 12u);
        put(rt, destination + offset, a);
        put(rt, destination + offset + 4u, b);
        put(rt, destination + offset + 8u, c);
        put(rt, destination + offset + 12u, d);
    }
}
// wsbw: the bytes of a word reversed.
inline std::uint32_t bswap32(std::uint32_t value) { return __builtin_bswap32(value); }

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

} // namespace

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
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

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}


// func_089AECEC(a0, a1, mode, pair): mode 1 or 2 only (else v0 = 2 and nothing). A value object `result` in the frame
// (func_089C9C34; func_08A5BB7C(0x08CA518C) when its word is 0). Mode 1: a string from func_08B85EB8 into a frame slot,
// then the virtual slot 0xC of *pair (func_08A5BB7C(0x08CA50BC) for null) fills a second string; func_089C9024(result's
// word, first, second). Mode 2: slot 0xC of *pair and of pair->+4 fill two strings, same func_089C9024. The strings are
// released (empty rep 0x08D74FD0, free func_08B8542C), last the value object is destroyed.
void func_089AECEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t pair = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
    };
    if (mode == 1u) {
        const std::uint32_t result = frame.at(0x1Cu);
        call_set(rt, ctx, 0x089C9C34u, {result});
        const std::uint32_t result_word = word(rt, result);
        if (result_word == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA518Cu});
        const std::uint32_t first = frame.at(0x18u);
        call_set(rt, ctx, 0x08B85EB8u, {first, 0x08C8C74Cu, frame.at(4u)});
        const std::uint32_t object = word(rt, pair);
        checked(object);
        const std::uint32_t second = frame.at(0x14u);
        call_virtual_at(rt, ctx, object, 0xCu, {second, object});
        call_set(rt, ctx, 0x089C9024u, {result_word, first, second});
        release_string(rt, ctx, second, 0x08D74FD0u, 0x08B8542Cu, frame.at(3u));
        release_string(rt, ctx, first, 0x08D74FD0u, 0x08B8542Cu, frame.at(2u));
        call_set(rt, ctx, kDestroy, {result});
        return;
    }
    if (mode != 2u) {
        ctx.gpr[2] = 2u;
        return;
    }
    const std::uint32_t result = frame.at(0x10u);
    call_set(rt, ctx, 0x089C9C34u, {result});
    const std::uint32_t result_word = word(rt, result);
    if (result_word == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA518Cu});
    const std::uint32_t left = word(rt, pair);
    checked(left);
    const std::uint32_t first = frame.at(0xCu);
    call_virtual_at(rt, ctx, left, 0xCu, {first, left});
    const std::uint32_t right = word(rt, pair + 4u);
    checked(right);
    const std::uint32_t second = frame.at(8u);
    call_virtual_at(rt, ctx, right, 0xCu, {second, right});
    call_set(rt, ctx, 0x089C9024u, {result_word, first, second});
    release_string(rt, ctx, second, 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
    release_string(rt, ctx, first, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, kDestroy, {result});
}

// func_0880F7E0(self, source): the virtual slot 0x1A8 (with 0, then func_08A0F0CC) of the objects at self->+0x18C and
// +0x190, then func_089DB5CC(self->+0xD0, and +0xD4, self->+0x23C + 0xC0); the three objects at self->+0x180.. get slot
// 0x1A8(1) and func_08A0F0CC. self->+0x26C = source->+0x8C; n = trunc(source->+0xB0 x 10): func_0880DBCC(self, i) for
// i = 9..0 then func_0880DC94(self, i) for i = 0..n-1. With the mode word m = source->+0xAC: func_08ABC4C4(text,
// 0x7FFFFFFF; f12 = 0, f13 = 1, f14 = self->+0x2D4) with text 0x08C98778 (m = 0) or 0x08C98788, then the same call as a
// tail jump with text 1 -> 0x08C987AC, 2 -> 0x08C987A4, 3 -> 0x08C9879C, 4 -> 0x08C98790, else 0x08C987B4.
void func_0880F7E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    for (const std::uint32_t offset : {0x18Cu, 0x190u}) {
        const std::uint32_t object = word(rt, self + offset);
        call_virtual_at(rt, ctx, object, 0x1A8u, {object, 0u});
        call_set(rt, ctx, 0x08A0F0CCu, {word(rt, self + offset)});
    }
    const std::uint32_t first = word(rt, self + 0xD0u);
    const std::uint32_t second = word(rt, self + 0xD4u);
    call_set(rt, ctx, 0x089DB5CCu, {first, word(rt, self + 0x23Cu) + 0xC0u});
    call_set(rt, ctx, 0x089DB5CCu, {second, word(rt, self + 0x23Cu) + 0xC0u});
    for (std::uint32_t index = 0u; index != 3u; ++index) {
        const std::uint32_t object = word(rt, self + 0x180u + index * 4u);
        call_virtual_at(rt, ctx, object, 0x1A8u, {object, 1u});
        call_set(rt, ctx, 0x08A0F0CCu, {word(rt, self + 0x180u + index * 4u)});
    }
    put(rt, self + 0x26Cu, word(rt, source + 0x8Cu));
    const std::uint32_t count = ctx.fpu_float_to_word(psp_mul(float_at(rt, source + 0xB0u), 10.0f), 1u);
    for (std::int32_t index = 9; index != -1; --index) call_set(rt, ctx, 0x0880DBCCu, {self, static_cast<std::uint32_t>(index)});
    if (signed_of(count) > 0) {
        for (std::uint32_t index = 0u; index != count; ++index) call_set(rt, ctx, 0x0880DC94u, {self, index});
    }
    const std::uint32_t mode = word(rt, source + 0xACu);
    const float scale = float_at(rt, self + 0x2D4u);
    call_setf(rt, ctx, 0x08ABC4C4u, {mode == 0u ? 0x08C98778u : 0x08C98788u, 0x7FFFFFFFu}, {0.0f, 1.0f, scale});
    std::uint32_t text;
    switch (mode) {
    case 1u: text = 0x08C987ACu; break;
    case 2u: text = 0x08C987A4u; break;
    case 3u: text = 0x08C9879Cu; break;
    case 4u: text = 0x08C98790u; break;
    default: text = 0x08C987B4u; break;
    }
    ctx.fpr[12] = 0.0f;
    ctx.fpr[13] = 1.0f;
    ctx.fpr[14] = float_at(rt, self + 0x2D4u);
    tail_call(ctx, 0x08ABC4C4u, {text, 0x7FFFFFFFu});
}

// func_08B13B84(self, out, cursor): under the lock at self + 0x1B8 a table-driven scan (class bytes at 0x08C92BC0 indexed
// by the byte a node points to). node = func_08B137E0(self, 0, 0); then repeatedly the next piece after `cursor`
// (func_08B237A0, func_08B23778) is matched with func_08B130D4(self, node, cursor, length); a null node fails (0). A class
// with bit 0 set in the old node chains through func_08B1257C and func_08B137E0 with a running index; otherwise the
// matched node's class bits decide: with a non-zero byte at the piece end, bit 0 clear fails, else the scan continues
// on the next piece; with a zero byte the result record `out` is filled (out+0 = 0, kind in +4, +8, +0x10, +0x14,
// +0x18, +0x1C, +0x3C, from func_08B12268 on a frame word) and 1 is returned. The result is the unlock's caller value.
void func_08B13B84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    std::uint32_t cursor = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t lock = self + 0x1B8u;
    constexpr std::uint32_t kClass = 0x08C92BC0u;
    const auto class_of = [&](std::uint32_t node) { return byte_u(rt, kClass + byte_u(rt, node)); };
    const auto done = [&](std::uint32_t value) {
        call_set(rt, ctx, kUnlock, {lock});
        ctx.gpr[2] = value;
    };
    call_set(rt, ctx, kLock, {lock});
    std::uint32_t node = call_set(rt, ctx, 0x08B137E0u, {self, 0u, 0u});
    std::uint32_t step = 0u;
    for (;;) {
        const std::uint32_t piece = call_set(rt, ctx, 0x08B237A0u, {cursor});
        const std::uint32_t end = call_set(rt, ctx, 0x08B23778u, {piece});
        const std::uint32_t length = piece - cursor;
        std::uint32_t chain = step;
        bool next_piece = false;
        while (!next_piece) {
            const std::uint32_t old_class = class_of(node);
            node = call_set(rt, ctx, 0x08B130D4u, {self, node, cursor, length});
            if (node == 0u) return done(0u);
            if ((old_class & 1u) != 0u) {
                const std::uint32_t node_class = class_of(node);
                const std::uint32_t chained = call_set(rt, ctx, 0x08B1257Cu, {node});
                node = call_set(rt, ctx, 0x08B137E0u, {self, class_of(chained) | (node_class << 8), chain});
                ++chain;
                continue;
            }
            const std::uint32_t chained = call_set(rt, ctx, 0x08B1257Cu, {node});
            put(rt, frame.at(0u), chained);
            const std::uint32_t bits = class_of(node);
            if (sext8(byte_u(rt, end)) != 0u) {
                if ((bits & 1u) == 0u) return done(0u);
                node = call_set(rt, ctx, 0x08B137E0u, {self, class_of(chained) | ((bits >> 2) << 8), step});
                step += 4u;
                cursor = end;
                next_piece = true;
                continue;
            }
            put(rt, out, 0u);
            if ((bits & 1u) != 0u) {
                put(rt, out + 4u, 3u);
                return done(1u);
            }
            const std::uint32_t first = call_set(rt, ctx, 0x08B12268u, {frame.at(0u)});
            const std::uint32_t second = call_set(rt, ctx, 0x08B12268u, {frame.at(0u)});
            if (((bits >> 1) & 1u) != 0u) {
                put(rt, out + 4u, 2u);
                put(rt, out + 8u, 1u);
                const std::uint32_t third = call_set(rt, ctx, 0x08B12268u, {frame.at(0u)});
                put(rt, out + 0x10u, third);
                put(rt, out + 0x14u, 0u);
            } else {
                put(rt, out + 4u, 1u);
                put(rt, out + 0x10u, second);
                put(rt, out + 0x14u, 0u);
            }
            put(rt, out + 0x3Cu, first);
            put(rt, out + 0x18u, second);
            put(rt, out + 0x1Cu, 0u);
            return done(1u);
        }
    }
}

// func_0899A048(self, arg): when self->+0x124 (a holder) and func_089952D8(holder) (a record r) exist: func_089687D8 on
// self->+0x130 with r->+0x1D and on self->+0x134 with r->+0x1C (when set). With the mode m = holder->+0x120 two floats are
// read through func_0896A35C (into a frame word, from self->+0x128 or else from self->+0x12C and the holder; the exact
// operand slot depends on m == 1), then, when self->+0x12C is set, two values f20 + f1 x r->+0x14 and f1 x r->+0x18 are
// stored through func_0896A9C4 (frame word, operand slots by m). With the second object (self->+0x128 or +0x12C) set, bit 14
// of its word +0xE0 is set from `r->+0x18 < 1.0` when self->+0x120 == 1, else from `self->+0x120 == 0`. Ends with
// func_089FBC08(self, arg).
void func_0899A048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t slot = frame.at(0u);
    const auto finish = [&]() { call_set(rt, ctx, 0x089FBC08u, {self, arg}); };
    std::uint32_t holder = word(rt, self + 0x124u);
    if (holder == 0u) return finish();
    const std::uint32_t record = call_set(rt, ctx, 0x089952D8u, {holder});
    if (record == 0u) return finish();
    if (word(rt, self + 0x130u) != 0u) call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x130u), byte_u(rt, record + 0x1Du)});
    if (word(rt, self + 0x134u) != 0u) call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x134u), byte_u(rt, record + 0x1Cu)});
    holder = word(rt, self + 0x124u);
    std::uint32_t second = word(rt, self + 0x128u);
    const float high = float_at(rt, record + 0x18u);
    const float low = float_at(rt, record + 0x14u);
    const std::uint32_t mode = word(rt, holder + 0x120u);
    float base = 0.0f;
    float scale;
    if (second != 0u) {
        if (mode == 1u) call_set(rt, ctx, 0x0896A35Cu, {second, 0u, 0u, 0u, slot});
        else call_set(rt, ctx, 0x0896A35Cu, {second, 0u, 0u, slot, 0u});
        scale = float_at(rt, slot);
    } else {
        second = word(rt, self + 0x12Cu);
        if (mode == 1u) {
            call_set(rt, ctx, 0x0896A35Cu, {holder, 0u, slot, 0u, 0u});
            base = float_at(rt, slot);
            call_set(rt, ctx, 0x0896A35Cu, {word(rt, self + 0x124u), 0u, 0u, 0u, slot});
        } else {
            call_set(rt, ctx, 0x0896A35Cu, {holder, slot, 0u, 0u, 0u});
            base = float_at(rt, slot);
            call_set(rt, ctx, 0x0896A35Cu, {word(rt, self + 0x124u), 0u, 0u, slot, 0u});
        }
        scale = float_at(rt, slot);
    }
    const std::uint32_t lever = word(rt, self + 0x12Cu);
    if (lever != 0u) {
        const float first = base + psp_mul(scale, low);
        const float next = psp_mul(scale, high);
        if (mode == 0u) {
            put_float(rt, slot, first);
            call_set(rt, ctx, 0x0896A9C4u, {lever, slot, 0u, 0u, 0u});
            put_float(rt, slot, next);
            call_set(rt, ctx, 0x0896A9C4u, {word(rt, self + 0x12Cu), 0u, 0u, slot, 0u});
        } else if (mode == 1u) {
            put_float(rt, slot, first);
            call_set(rt, ctx, 0x0896A9C4u, {lever, 0u, slot, 0u, 0u});
            put_float(rt, slot, next);
            call_set(rt, ctx, 0x0896A9C4u, {word(rt, self + 0x12Cu), 0u, 0u, 0u, slot});
        }
    }
    if (second != 0u) {
        const std::uint32_t kind = word(rt, self + 0x120u);
        const std::uint32_t flags = word(rt, second + 0xE0u);
        const std::uint32_t bit = kind == 1u ? (high < 1.0f ? 1u : 0u) : (kind < 1u ? 1u : 0u);
        put(rt, second + 0xE0u, (flags & ~(1u << 14)) | (bit << 14));
    }
    finish();
}

// func_088DC974(self, mode): nothing for mode 0. With n = func_088D9F80(self) the halfword indices 0..n-1 go to a frame
// array, which a selection pass orders: an index i scores v + 1000 x v (v = func_088D4D14(self + 4, i) after
// func_088DAA64(self, i); the 1000 x v part only for v > 0), plus 1000000 when func_088DC90C(self, i) is 0 or 2000000 when it
// is 3. For each position the entry there is swapped with a later one whose score differs when "first < later" differs
// from (mode is not 2 or 4). Last, the inverse permutation (stored at the byte-swapped position) goes to a second frame array
// (frame +8) handed to func_088D4D48(self + 4, array).
void func_088DC974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    if (mode == 0u) return;
    const bool descending = mode != 2u && mode != 4u;
    const std::uint32_t table = frame.at(0u);
    const std::uint32_t inverse = frame.at(8u);
    const std::uint32_t side = self + 4u;
    const auto count = [&]() { return call_set(rt, ctx, 0x088D9F80u, {self}); };
    const auto score = [&](std::uint32_t index) {
        const std::uint32_t base = call_set(rt, ctx, 0x088DAA64u, {self, index});
        const std::uint32_t value = call_set(rt, ctx, 0x088D4D14u, {side, index});
        std::uint32_t result = base;
        if (signed_of(value) > 0) result += value * 1000u;
        const std::uint32_t kind = call_set(rt, ctx, 0x088DC90Cu, {self, index});
        if (kind == 0u) result += 1000000u;
        else if (kind == 3u) result += 2000000u;
        return result;
    };
    const std::uint32_t total = count();
    for (std::int32_t index = 0; index < signed_of(total); ++index) put16(rt, table + static_cast<std::uint32_t>(index) * 2u, static_cast<std::uint32_t>(index));
    std::uint32_t cursor = table;
    std::uint32_t first = 0u;
    while (signed_of(first) < signed_of(count() - 1u)) {
        std::uint32_t held = sext16(half_u(rt, cursor));
        std::uint32_t best = score(held);
        const std::uint32_t next = first + 1u;
        std::uint32_t walk = table + next * 2u;
        for (std::uint32_t other = next; signed_of(other) < signed_of(count()); ++other, walk += 2u) {
            const std::uint32_t candidate = sext16(half_u(rt, walk));
            const std::uint32_t value = score(candidate);
            if (best == value) continue;
            if ((descending ? 1u : 0u) == (signed_of(best) < signed_of(value) ? 1u : 0u)) continue;
            best = value;
            put16(rt, cursor, candidate);
            put16(rt, walk, held);
            held = candidate;
        }
        cursor += 2u;
        first = next;
    }
    std::uint32_t source = table;
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(count()); ++index, source += 2u) {
        const std::uint32_t swapped = ((index & 0x00FF00FFu) << 8) | ((index >> 8) & 0x00FF00FFu);
        put16(rt, inverse + (sext16(half_u(rt, source)) << 1), swapped);
    }
    call_set(rt, ctx, 0x088D4D48u, {side, inverse});
}

// func_0894D6D0(self, key, source): h = func_0894CFB8(self, key), then func_0894CF98(self, key) is overwritten with the
// 0x990 bytes at source (16 bytes at a time, source read at any alignment); h's bytes +0 and +2 are kept; h->+0x690 /
// +0x694 / +0x770 / +0x774 = -1. Four records (stride 0x38 from func_0894CF98(...) + 0x1E70, filled from stride 0x30 at
// +0x7C0 after func_0894CF40(self, key)): the three floats at +0..+8, +0x30 / +0x34 = -1 and the 9 words at +0xC..+0x2C
// copied. Then func_0894CFD8(self, key) = u is reset: words +0, +4, +0x14..+0x1C = 0, floats +8 = 1.0, words +0xC, +0x10 = 0
// and four 0x24-byte entries from +0x20 (zeros, 1.0 at +4 and +0x10, bytes +0x20 / +0x21 = 0).
void func_0894D6D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t head = call_set(rt, ctx, 0x0894CFB8u, {self, key});
    const std::uint32_t target = call_set(rt, ctx, 0x0894CF98u, {self, key});
    const std::uint32_t kept_2 = byte_u(rt, head + 2u);
    const std::uint32_t kept_0 = byte_u(rt, head);
    for (std::uint32_t offset = 0u; offset != 0x990u; offset += 0x10u) {
        const std::uint32_t a = word_unaligned(rt, source + offset);
        const std::uint32_t b = word_unaligned(rt, source + offset + 4u);
        const std::uint32_t c = word_unaligned(rt, source + offset + 8u);
        const std::uint32_t d = word_unaligned(rt, source + offset + 12u);
        put(rt, target + offset, a);
        put(rt, target + offset + 4u, b);
        put(rt, target + offset + 8u, c);
        put(rt, target + offset + 12u, d);
    }
    put(rt, head + 0x770u, 0xFFFFFFFFu);
    put(rt, head + 0x774u, 0xFFFFFFFFu);
    put8(rt, head + 2u, kept_2);
    put8(rt, head, kept_0);
    put(rt, head + 0x690u, 0xFFFFFFFFu);
    put(rt, head + 0x694u, 0xFFFFFFFFu);
    std::uint32_t destination = 0x1E70u;
    std::uint32_t from = 0x7C0u;
    for (std::uint32_t index = 0u; index != 4u; ++index, destination += 0x38u, from += 0x30u) {
        const std::uint32_t first_result = call_set(rt, ctx, 0x0894CF40u, {self, key});
        const std::uint32_t base = call_set(rt, ctx, 0x0894CF98u, {self, key});
        const std::uint32_t record = first_result + destination;
        const std::uint32_t entry = base + from;
        put(rt, record, word(rt, entry));
        put(rt, record + 4u, word(rt, entry + 4u));
        const std::uint32_t third = word(rt, entry + 8u);
        put(rt, record + 0x30u, 0xFFFFFFFFu);
        put(rt, record + 0x34u, 0xFFFFFFFFu);
        put(rt, record + 8u, third);
        for (std::uint32_t offset = 0xCu; offset != 0x2Cu; offset += 0x10u) {
            const std::uint32_t a = word(rt, entry + offset);
            const std::uint32_t b = word(rt, entry + offset + 4u);
            const std::uint32_t c = word(rt, entry + offset + 8u);
            const std::uint32_t d = word(rt, entry + offset + 12u);
            put(rt, record + offset, a);
            put(rt, record + offset + 4u, b);
            put(rt, record + offset + 8u, c);
            put(rt, record + offset + 12u, d);
        }
        put(rt, record + 0x2Cu, word(rt, entry + 0x2Cu));
    }
    const std::uint32_t reset = call_set(rt, ctx, 0x0894CFD8u, {self, key});
    for (std::uint32_t index = 0u; index != 2u; ++index) put(rt, reset + index * 4u, 0u);
    for (std::uint32_t index = 0u; index != 3u; ++index) put(rt, reset + 0x14u + index * 4u, 0u);
    put_float(rt, reset + 8u, 1.0f);
    put(rt, reset + 0xCu, 0u);
    put(rt, reset + 0x10u, 0u);
    for (std::uint32_t index = 0u; index != 4u; ++index) {
        const std::uint32_t entry = reset + 0x20u + index * 0x24u;
        put(rt, entry, 0u);
        put_float(rt, entry + 4u, 1.0f);
        put(rt, entry + 8u, 0u);
        put(rt, entry + 0xCu, 0u);
        put_float(rt, entry + 0x10u, 1.0f);
        put(rt, entry + 0x14u, 0u);
        put(rt, entry + 0x18u, 0u);
        put(rt, entry + 0x1Cu, 0u);
        put8(rt, entry + 0x20u, 0u);
        put8(rt, entry + 0x21u, 0u);
    }
}

// func_089C8C70(self, a, b): under the lock 0x08CF4E98 a lookup-or-insert in the tree at self + 0x48 (end node self + 0x4C)
// keyed by a string. The key is formatted (func_08B3767C into a 0x80-byte frame buffer from the template 0x08CA6664,
// *a, *b and func_08B49634(word 0x08D69524)) into a string (func_08B85EB8); func_08C00DB4(tree, key) finds it; only
// when absent the value object func_089F0AA4 / func_089F0AF4 is built, the key string made again, and the lower bound
// (func_08C00D14) compared with it (length-limited func_08B36170, then lengths); when not already there the entry is
// inserted (func_08C0195C with a pair built by func_08B88F44 / func_089F09EC) and the value object assigned to the
// entry's +0x14 (func_08A5BAD8) unless it already is it.
void func_089C8C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    GuestFrame frame(ctx, 0xE0u);
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kFree = 0x08B8542Cu;
    constexpr std::uint32_t kLockWord = 0x08CF4E98u;
    const std::uint32_t tree = self + 0x48u;
    const std::uint32_t end = self + 0x4Cu;
    const std::uint32_t buffer = frame.at(0x28u);
    const std::uint32_t key_slot = frame.at(0x10u);
    call_set(rt, ctx, kLock, {kLockWord});
    const std::uint32_t extra = call_set(rt, ctx, 0x08B49634u, {word(rt, 0x08D69524u)});
    const std::uint32_t first = word(rt, a);
    const std::uint32_t second = word(rt, b);
    call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08CA6664u, first, extra, second});
    call_set(rt, ctx, 0x08B85EB8u, {key_slot, buffer, frame.at(4u)});
    const std::uint32_t found = call_set(rt, ctx, 0x08C00DB4u, {tree, key_slot});
    release_string(rt, ctx, key_slot, kEmpty, kFree, frame.at(2u));
    if (end == found) {
        const std::uint32_t value = frame.at(0x14u);
        const std::uint32_t key = frame.at(0xCu);
        call_set(rt, ctx, 0x089F0AA4u, {value});
        const std::uint32_t value_word = word(rt, value);
        if (value_word == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA638Cu});
        call_set(rt, ctx, 0x089F0AF4u, {value_word, buffer});
        call_set(rt, ctx, 0x08B85EB8u, {key, buffer, frame.at(3u)});
        std::uint32_t hint = call_set(rt, ctx, 0x08C00D14u, {tree, key});
        std::uint32_t entry = hint;
        bool insert = true;
        if (end != hint) {
            const std::uint32_t key_length = word(rt, word(rt, key) - 0xCu);
            const std::uint32_t node_length = word(rt, word(rt, hint + 0x10u) - 0xCu);
            const std::uint32_t common = node_length < key_length ? node_length : key_length;
            std::int32_t order = signed_of(key_length - node_length);
            const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {word(rt, key), word(rt, hint + 0x10u), common});
            if (compared != 0u) order = signed_of(compared);
            insert = order < 0;
        }
        if (insert) {
            const std::uint32_t scratch = frame.at(8u);
            const std::uint32_t pair_key = frame.at(0x20u);
            const std::uint32_t pair = frame.at(0x24u);
            call_set(rt, ctx, 0x089F0AA4u, {scratch});
            call_set(rt, ctx, 0x08B88F44u, {pair_key, key});
            call_set(rt, ctx, 0x089F09ECu, {pair, scratch});
            entry = call_set(rt, ctx, 0x08C0195Cu, {tree, hint, pair_key});
            call_set(rt, ctx, kDestroy, {pair});
            release_string(rt, ctx, pair_key, kEmpty, kFree, frame.at(1u));
            call_set(rt, ctx, kDestroy, {scratch});
        }
        if (entry + 0x14u != value) call_set(rt, ctx, kAssign, {entry + 0x14u, word(rt, value)});
        release_string(rt, ctx, key, kEmpty, kFree, frame.at(0u));
        call_set(rt, ctx, kDestroy, {value});
    }
    call_set(rt, ctx, kUnlock, {kLockWord});
}

// func_08B0B8CC(self, source): func_08B0B630(self); a reader object in the frame (func_08B0CC04) is opened on `source`
// (func_08B0C9AC with a {1, self->+0xC, self->+0x10} record when the byte self->+8 is set, else func_08B0C99C). When it
// opened (frame word +0x54 = 0): the first 16 bytes (func_08B0C650) must read as 0x33627067 after a byte swap; then a
// second 16-byte header is read, its size field (byte-swapped) allocated (func_08B2B4D8(0x10, size)), the 32 header bytes
// stored in it (any alignment), the remainder read into the block (func_08B0C650), func_08B0B508 and, for each of
// the byte-swapped count of entries at block +0xC, a (word, size rounded up to 16) pair read (func_08B0C650);
// func_08B0B1F8(block, block); self->+4 = block, self->+0 = 0. Then func_08B0C778; and when self->+0 or self->+4 and the
// callback self->+0x14 are set, that callback runs as (self, i) for i below the entry count (self->+0 ? its +0xC :
// byte-swapped +0xC of self->+4) read again every time. Ends with func_08B0C808(reader).
void func_08B0B8CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x190u);
    const std::uint32_t reader = frame.at(0x50u);
    const auto finish = [&]() { call_set(rt, ctx, 0x08B0C808u, {reader}); };
    call_set(rt, ctx, 0x08B0B630u, {self});
    call_set(rt, ctx, 0x08B0CC04u, {reader});
    if (byte_u(rt, self + 8u) != 0u) {
        const std::uint32_t record = frame.at(0x20u);
        const std::uint32_t last = word(rt, self + 0x10u);
        put(rt, record, 1u);
        put(rt, record + 4u, word(rt, self + 0xCu));
        put(rt, record + 8u, last);
        call_set(rt, ctx, 0x08B0C9ACu, {reader, source, record, last});
    } else {
        call_set(rt, ctx, 0x08B0C99Cu, {reader, source, 0u, 0u});
    }
    if (word(rt, frame.at(0x54u)) != 0u) return finish();
    const std::uint32_t header = frame.at(0x2Cu);
    const std::uint32_t request = frame.at(0x18u);
    put(rt, request, header);
    put(rt, request + 4u, 0x10u);
    call_set(rt, ctx, 0x08B0C650u, {reader, request});
    if (bswap32(word(rt, header)) == 0x33627067u) {
        const std::uint32_t second = frame.at(0x3Cu);
        const std::uint32_t request2 = frame.at(0x10u);
        put(rt, request2, second);
        put(rt, request2 + 4u, 0x10u);
        call_set(rt, ctx, 0x08B0C650u, {reader, request2});
        std::uint32_t size = bswap32(word(rt, frame.at(0x44u)));
        const std::uint32_t block = call_set(rt, ctx, 0x08B2B4D8u, {0x10u, size});
        const std::uint32_t values[8] = {word(rt, frame.at(0x2Cu)), word(rt, frame.at(0x30u)), word(rt, frame.at(0x34u)), word(rt, frame.at(0x38u)),
                                         word(rt, frame.at(0x3Cu)), word(rt, frame.at(0x40u)), word(rt, frame.at(0x44u)), word(rt, frame.at(0x48u))};
        for (std::uint32_t index = 0u; index < 8u; ++index) put_unaligned(rt, block + index * 4u, values[index]);
        std::uint32_t entry = block + 0x20u;
        size -= 0x20u;
        const std::uint32_t request3 = frame.at(8u);
        put(rt, request3 + 4u, size);
        put(rt, request3, entry);
        call_set(rt, ctx, 0x08B0C650u, {reader, request3});
        call_set(rt, ctx, 0x08B0B508u, {block});
        const std::uint32_t pair = frame.at(0u);
        for (std::int32_t index = 0; index < signed_of(bswap32(word(rt, block + 0xCu))); ++index) {
            const std::uint32_t amount = bswap32(word(rt, entry + 8u));
            const std::uint32_t tag = bswap32(word(rt, entry + 4u));
            entry += 0x10u;
            put(rt, pair, tag);
            put(rt, pair + 4u, (amount + 0xFu) & ~0xFu);
            call_set(rt, ctx, 0x08B0C650u, {reader, pair});
        }
        call_set(rt, ctx, 0x08B0B1F8u, {block, block});
        put(rt, self + 4u, block);
        put(rt, self, 0u);
    }
    call_set(rt, ctx, 0x08B0C778u, {reader});
    const std::uint32_t holder = word(rt, self);
    if (holder == 0u && word(rt, self + 4u) == 0u) return finish();
    if (word(rt, self + 0x14u) == 0u) return finish();
    for (std::uint32_t index = 0u;; ++index) {
        const std::uint32_t current = word(rt, self);
        const std::uint32_t count = current != 0u ? word(rt, current + 0xCu) : bswap32(word(rt, word(rt, self + 4u) + 0xCu));
        if (!(signed_of(index) < signed_of(count))) break;
        const std::uint32_t callback = word(rt, self + 0x14u);
        ctx.gpr[2] = callback;
        call_set(rt, ctx, callback, {self, index});
    }
    finish();
}

// func_08A7F5FC(self, table, key): func_08A6972C(self, key); nothing more when *self is the value in word 0x08CF6FDC.
// Otherwise a value object in the frame (func_08A5D438) is filled by func_08A7D5FC(table, object, self); when its word is
// set the name string made from *self + 4 (func_08B88F44) loses its trailing digits (func_08B8D96C substring, assigned
// with func_08B855E0) and then, for counters 0, 1, 2..: func_08A69654(copy, name), func_08A6A23C(self, copy),
// func_08A69D68(copy), func_08A7D5FC again; while the object's word is set the name is rebuilt as prefix + the counter
// formatted by func_08B3767C (template 0x08CB14A4, 0x20-byte buffer; func_08B35F60 length, func_08BEED1C append) and
// assigned. Strings released (empty rep 0x08D74FD0, free func_08B8542C), the object destroyed; returns self.
void func_08A7F5FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t table = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kFree = 0x08B8542Cu;
    call_set(rt, ctx, 0x08A6972Cu, {self, key});
    if (word(rt, self) != word(rt, 0x08CF6FDCu)) {
        const std::uint32_t object = frame.at(0xCu);
        const std::uint32_t name = frame.at(0x10u);
        const std::uint32_t prefix = frame.at(0x14u);
        call_set(rt, ctx, 0x08A5D438u, {object});
        call_set(rt, ctx, 0x08A7D5FCu, {table, object, self});
        if (word(rt, object) != 0u) {
            call_set(rt, ctx, 0x08B88F44u, {name, word(rt, self) + 4u});
            const std::int32_t last = signed_of(word(rt, word(rt, name) - 0xCu)) - 1;
            std::int32_t cut = last;
            if (last >= 0) {
                std::int32_t scanned = 0;
                for (;;) {
                    const std::uint32_t data = word(rt, name);
                    ++scanned;
                    if (signed_of(word(rt, data - 4u)) >= 0) call_set(rt, ctx, 0x08B88FF8u, {name});
                    const std::uint32_t digit = sext8(byte_u(rt, word(rt, name) + static_cast<std::uint32_t>(cut))) - 0x30u;
                    if (!(digit < 10u)) break;
                    --cut;
                    if (last + 1 == scanned) break;
                }
            }
            call_set(rt, ctx, 0x08B8D96Cu, {prefix, name, 0u, static_cast<std::uint32_t>(cut + 1)});
            call_set(rt, ctx, 0x08B855E0u, {name, prefix});
            const std::uint32_t copy = frame.at(8u);
            const std::uint32_t built = frame.at(4u);
            const std::uint32_t buffer = frame.at(0x18u);
            for (std::uint32_t counter = 0u;; ++counter) {
                call_set(rt, ctx, 0x08A69654u, {copy, name});
                call_set(rt, ctx, 0x08A6A23Cu, {self, copy});
                call_set(rt, ctx, 0x08A69D68u, {copy});
                call_set(rt, ctx, 0x08A7D5FCu, {table, object, self});
                if (word(rt, object) == 0u) break;
                call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x20u, 0x08CB14A4u, counter});
                call_set(rt, ctx, 0x08B88F44u, {built, prefix});
                const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {buffer});
                call_set(rt, ctx, 0x08BEED1Cu, {built, buffer, length});
                call_set(rt, ctx, 0x08B855E0u, {name, built});
                release_string(rt, ctx, built, kEmpty, kFree, frame.at(2u));
            }
            release_string(rt, ctx, prefix, kEmpty, kFree, frame.at(1u));
            release_string(rt, ctx, name, kEmpty, kFree, frame.at(0u));
        }
        call_set(rt, ctx, kDestroy, {object});
    }
    ctx.gpr[2] = self;
}

// func_0891179C(): func_089106A4(0), func_08915714(); two value objects in the frame (func_08A9FA9C from 0x08CA1940,
// func_08A76AB4 from 0x08CF6ED8); when the first is set the second is assigned from func_08A7C3A4's result and, if that
// is set too, an array value is looked up (func_08A76AA0, func_08A69B3C(0x08CA1954), func_08A7BFD0) and, when found,
// four pairs of floats at 0x08CF1B10.. are filled from the virtual slot 0x54 of the two objects at its +0x14 entries
// (func_08A98674 element lookups, func_08A5BB7C(0x08CA192C) for null). Then 16 groups (stride 0x798) of 3 entries
// (stride 0x288) from 0x08CEA190: entry->+0x10/+0x14 = the 64-bit result of func_08B1CE38(entry) and the byte +0x281 =
// the 8 flag bytes at +0x69 + 0x4C k (k = 0..7) packed into bits 7 - k. Last, byte 0x08CF1BA1 = 1 and the value objects
// are destroyed.
void func_0891179C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t first = frame.at(8u);
    const std::uint32_t second = frame.at(0xCu);
    call_set(rt, ctx, 0x089106A4u, {0u});
    call_set(rt, ctx, 0x08915714u, {});
    call_set(rt, ctx, 0x08A9FA9Cu, {first, 0x08CA1940u});
    call_set(rt, ctx, 0x08A76AB4u, {second, 0x08CF6ED8u});
    if (word(rt, first) != 0u) {
        const std::uint32_t temporary = frame.at(4u);
        call_set(rt, ctx, 0x08A7C3A4u, {temporary, first});
        call_set(rt, ctx, kAssign, {second, word(rt, temporary)});
        const std::uint32_t found = word(rt, second);
        call_set(rt, ctx, kDestroy, {temporary});
        if (found != 0u) {
            const std::uint32_t list = frame.at(0x10u);
            call_set(rt, ctx, 0x08A76AA0u, {list});
            std::uint32_t owner = word(rt, second);
            if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA194Cu});
            call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CA1954u});
            call_set(rt, ctx, 0x08A7BFD0u, {owner, list, frame.at(0u)});
            call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
            if (word(rt, list) != 0u) {
                const std::uint32_t outer = frame.at(0x14u);
                const std::uint32_t inner = frame.at(0x18u);
                call_set(rt, ctx, 0x08A98674u, {outer, list});
                std::uint32_t output = 0x08CF1B14u;
                for (std::uint32_t offset = 0u; offset != 0x10u; offset += 4u, output += 8u) {
                    call_set(rt, ctx, 0x08A98674u, {inner, word(rt, word(rt, outer) + 0x14u) + offset});
                    const std::uint32_t pair = word(rt, word(rt, inner) + 0x14u);
                    std::uint32_t object = word(rt, pair);
                    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA192Cu});
                    call_virtual_at(rt, ctx, object, 0x54u, {object});
                    put_float(rt, output - 4u, ctx.fpr[0]);
                    object = word(rt, word(rt, word(rt, inner) + 0x14u) + 4u);
                    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA192Cu});
                    call_virtual_at(rt, ctx, object, 0x54u, {object});
                    put_float(rt, output, ctx.fpr[0]);
                    call_set(rt, ctx, kDestroy, {inner});
                }
                call_set(rt, ctx, kDestroy, {outer});
            }
            call_set(rt, ctx, kDestroy, {list});
        }
    }
    std::uint32_t group = 0x08CEA190u;
    for (std::uint32_t index = 0u; index != 0x10u; ++index, group += 0x798u) {
        std::uint32_t entry = group;
        for (std::uint32_t slot = 0u; slot != 3u; ++slot, entry += 0x288u) {
            call_set(rt, ctx, 0x08B1CE38u, {entry});
            put(rt, entry + 0x10u, ctx.gpr[2]);
            put(rt, entry + 0x14u, ctx.gpr[3]);
            std::uint32_t mask = 0u;
            for (std::uint32_t bit = 0u; bit != 8u; ++bit) {
                std::uint32_t value = 0u;
                if (byte_u(rt, entry + 0x69u + 0x4Cu * bit) != 0u) value = sext8(1u << (7u - bit));
                mask = (value | mask) & 0xFFu;
            }
            put8(rt, entry + 0x281u, mask);
        }
    }
    put8(rt, 0x08CF1BA1u, 1u);
    call_set(rt, ctx, kDestroy, {second});
    call_set(rt, ctx, kDestroy, {first});
}

// func_088D2098(dest, source): copies fields of a packed record (source read at any alignment): byte +0, words +1 / +5 to
// dest +4 / +8, then 0xC0 bytes from +9 to dest +0xC, 0x40 from +0xC9 to dest +0xCC, 0xC0 from +0x109 to dest +0x14C, three
// words from +0x1C9 to dest +0x20C, one from +0x1D5 to +0x218, three from +0x1D9 to +0x220, four from +0x1E5 to +0x304. v0 = source + 0x1F5.
void func_088D2098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    put8(rt, dest, byte_u(rt, source));
    put(rt, dest + 4u, word_unaligned(rt, source + 1u));
    put(rt, dest + 8u, word_unaligned(rt, source + 5u));
    copy_blocks(rt, dest + 0xCu, source + 9u, 0xC0u);
    copy_blocks(rt, dest + 0xCCu, source + 0xC9u, 0x40u);
    copy_blocks(rt, dest + 0x14Cu, source + 0x109u, 0xC0u);
    put(rt, dest + 0x20Cu, word_unaligned(rt, source + 0x1C9u));
    put(rt, dest + 0x210u, word_unaligned(rt, source + 0x1CDu));
    put(rt, dest + 0x214u, word_unaligned(rt, source + 0x1D1u));
    put(rt, dest + 0x218u, word_unaligned(rt, source + 0x1D5u));
    put(rt, dest + 0x220u, word_unaligned(rt, source + 0x1D9u));
    put(rt, dest + 0x224u, word_unaligned(rt, source + 0x1DDu));
    put(rt, dest + 0x228u, word_unaligned(rt, source + 0x1E1u));
    put(rt, dest + 0x304u, word_unaligned(rt, source + 0x1E5u));
    put(rt, dest + 0x308u, word_unaligned(rt, source + 0x1E9u));
    put(rt, dest + 0x30Cu, word_unaligned(rt, source + 0x1EDu));
    put(rt, dest + 0x310u, word_unaligned(rt, source + 0x1F1u));
    ctx.gpr[2] = source + 0x1F5u;
}

// func_08810010(self, object, value): nothing when object is 0. When func_089DDC94(object) is not 14: rec = self->+0x218
// (floats; entries of 16 bytes below it indexed by trunc(rec->+8)): object->+0xE8 = 6, func_089DE054(object, 14, index),
// func_089DE1BC(object, 14), then func_089DB078(object, 12; floats of entry +0x300..) and func_089DAEA0(object, 12; 0, 0,
// rec->+0 / 1.5 x entry +8, rec->+4 / 1.5 x entry +0x30C) and six more pairs (slots 10, 8 .. 0) of func_089DB078 (the four
// floats at 0x08CE29F0) and func_089DAEA0 with a running sum. Then (also for 14) the decimal digits of min(value,
// 9999999), least significant first, go to slots 0, 2 .. 12: func_089DB078(object, slot; the entry's floats from the digit's
// entry) - or the 0x08CE29F0 floats once the number ran out - and object->+0xE0 |= 0x4000.
void func_08810010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    if (object == 0u) return;
    constexpr std::uint32_t kBlank = 0x08CE29F0u;
    const std::uint32_t entries = self + 0x218u;
    const auto entry_of = [&](std::uint32_t record, std::uint32_t index) { return record - ((index - 1u) << 4); };
    const auto blank = [&](std::uint32_t slot) {
        call_setf(rt, ctx, 0x089DB078u, {object, slot},
                  {float_at(rt, kBlank), float_at(rt, kBlank + 4u), float_at(rt, kBlank + 8u), float_at(rt, kBlank + 0xCu)});
    };
    if (call_set(rt, ctx, 0x089DDC94u, {object}) != 14u) {
        std::uint32_t record = word(rt, entries);
        const float quarter = float_at(rt, record + 4u) / 1.5f;
        float width = float_at(rt, record) / 1.5f;
        float offset = 0.0f;
        std::uint32_t index = ctx.fpu_float_to_word(float_at(rt, record + 8u), 1u);
        const float unit = float_at(rt, entry_of(record, index) + 0x30Cu);
        put(rt, object + 0xE8u, 6u);
        const float height = psp_mul(quarter, unit);
        call_set(rt, ctx, 0x089DE054u, {object, 14u, index});
        call_set(rt, ctx, 0x089DE1BCu, {object, 14u});
        record = word(rt, entries);
        index = ctx.fpu_float_to_word(float_at(rt, record + 8u), 1u);
        const std::uint32_t base = entry_of(record, index) + 0x300u;
        call_setf(rt, ctx, 0x089DB078u, {object, 12u, ctx.gpr[6], index},
                  {float_at(rt, base), float_at(rt, base + 4u), 0.0f, 0.0f});
        width = psp_mul(width, float_at(rt, base + 8u));
        call_setf(rt, ctx, 0x089DAEA0u, {object, 12u}, {offset, 0.0f, width, height});
        offset += width;
        for (std::int32_t slot = 10;;) {
            blank(static_cast<std::uint32_t>(slot));
            call_setf(rt, ctx, 0x089DAEA0u, {object, static_cast<std::uint32_t>(slot)}, {offset, 0.0f, width, height});
            slot -= 2;
            if (slot == -2) break;
            offset += width;
        }
    }
    std::int32_t number = std::min(signed_of(value), 9999999);
    std::uint32_t slot = 0u;
    for (std::uint32_t digit_index = 0u; digit_index != 7u; ++digit_index, slot += 2u) {
        const std::int32_t sign = number >> 31;
        const std::int32_t quotient = static_cast<std::int32_t>((static_cast<std::int64_t>(number) * 0x66666667LL) >> 32 >> 2) - sign;
        if (digit_index == 0u || number > 0) {
            const std::uint32_t record = word(rt, entries);
            const std::uint32_t index = ctx.fpu_float_to_word(float_at(rt, record + 8u), 1u);
            const std::int32_t digit = number - quotient * 10;
            const std::uint32_t entry = record + (static_cast<std::uint32_t>(digit - (signed_of(index) - 1)) << 4) + 0x300u;
            call_setf(rt, ctx, 0x089DB078u, {object, slot},
                      {float_at(rt, entry), float_at(rt, entry + 4u), float_at(rt, entry + 8u), float_at(rt, entry + 0xCu)});
        } else {
            blank(slot);
        }
        number = quotient;
    }
    const std::uint32_t flags = word(rt, object + 0xE0u) | 0x4000u;
    put(rt, object + 0xE0u, flags);
    ctx.gpr[2] = flags;
}

// func_08B6DFD0(x = f12, y = f13, iy = a0): the libm float tangent kernel (fdlibm's __kernel_tan shape) over the
// coefficients at 0x08CB6520..0x08CB6564; returns in f0. |x| below 2^-28: x itself (iy = 1), 1 / |x| (x = 0 and
// iy = -1; func_08B663A4 is the fabs) or -1 / x. |x| above 0.6744: x = (pi/4 - x) + (pi/4lo - y) with y = 0 first (sign
// folded). The odd polynomial gives w = x + r; iy = 1 returns w, otherwise -1 / w with the low 12 bits of w and of the
// quotient cleared (the usual correction); for the large range the closed form sign x (iy - 2 (x - (w w / (w + iy) - r))).
void func_08B6DFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    float y = ctx.fpr[13];
    const std::uint32_t iy = ctx.gpr[4];
    const std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t ix = hx & 0x7FFFFFFFu;
    const auto constant = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto finish = [&](float result) { ctx.fpr[0] = result; };
    if (signed_of(ix) <= 0x317FFFFF) {
        if (ctx.fpu_float_to_word(x, 1u) == 0u) {
            if ((ix | (iy + 1u)) == 0u) {
                const float magnitude = call_setf(rt, ctx, 0x08B663A4u, {}, {x}).f0;
                return finish(constant(0x08CB6520u) / magnitude);
            }
            if (iy == 1u) return finish(x);
            return finish(constant(0x08CB6524u) / ctx.fpr[12]);
        }
    } else if (signed_of(ix) > 0x3F2CA13F) {
        if (signed_of(hx) < 0) {
            x = -x;
            y = -y;
        }
        const float high = constant(0x08CB6528u) - x;
        const float low = constant(0x08CB652Cu) - y;
        y = 0.0f;
        x = high + low;
    }
    const float z = psp_mul(x, x);
    const float w = psp_mul(z, z);
    float odd = psp_mul(w, constant(0x08CB6548u));
    float even = psp_mul(w, constant(0x08CB6530u));
    odd = odd + constant(0x08CB654Cu);
    even = even + constant(0x08CB6534u);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB6550u);
    even = even + constant(0x08CB6538u);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB6554u);
    even = even + constant(0x08CB653Cu);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB6558u);
    even = even + constant(0x08CB6540u);
    odd = psp_mul(w, odd);
    const float xz = psp_mul(x, z);
    float tail = psp_mul(w, even);
    odd = odd + constant(0x08CB655Cu);
    tail = tail + constant(0x08CB6544u);
    odd = psp_mul(z, odd);
    tail = tail + odd;
    const float last = psp_mul(xz, constant(0x08CB6560u));
    float inner = psp_mul(xz, tail);
    inner = y + inner;
    float rest = psp_mul(z, inner);
    rest = y + rest;
    rest = rest + last;
    const float sum = x + rest;
    if (signed_of(ix) > 0x3F2CA13F) {
        const float step = static_cast<float>(signed_of(iy));
        const float sign = static_cast<float>(1 - ((signed_of(hx) >> 30) & 2));
        float quotient = psp_mul(sum, sum) / (sum + step);
        quotient = quotient - rest;
        quotient = x - quotient;
        quotient = psp_mul(quotient, constant(0x08CB6564u));
        const float inside = step + quotient;
        return finish(psp_mul(sign, inside));
    }
    if (iy == 1u) return finish(sum);
    float inverse = constant(0x08CB6524u) / sum;
    const float head = std::bit_cast<float>(std::bit_cast<std::uint32_t>(sum) & ~0xFFFu);
    float correction = head - x;
    correction = rest - correction;
    const float cut = std::bit_cast<float>(std::bit_cast<std::uint32_t>(inverse) & ~0xFFFu);
    float product = psp_mul(head, cut);
    const float scaled = psp_mul(correction, cut);
    product = product + constant(0x08CB6520u);
    product = product + scaled;
    inverse = psp_mul(inverse, product);
    finish(cut + inverse);
}

// func_08B8BF44(first, last, depth, a3): the libstdc++ __introsort_loop over 24-byte elements {word, word, vector of
// halfwords at +8 (begin, end, capacity)}; elements compare by those vectors (func_08B8A72C(begin, end, begin, end)). A
// range of at most 16 elements returns 1. With depth 0, or after `depth` partitions, func_08B8BC08(first, last, last, ..)
// (the heap sort fallback) ends it. Else the median of the first, middle and last element is copied to a frame element
// (its vector duplicated: func_08A5BC20, func_08B8A654), func_08B8B4A4(first, last, &element, capacity end) partitions,
// the copy is destroyed (func_08B8A644) and freed (func_08A5BC54), the upper part is sorted recursively with depth - 1
// and the loop goes on with the lower one while it has more than 16 elements; v0 = that last count.
void func_08B8BF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    std::uint32_t last = ctx.gpr[5];
    const std::uint32_t limit = ctx.gpr[6];
    const std::uint32_t saved_s0 = ctx.gpr[16];
    GuestFrame frame(ctx, 0x60u);
    const auto count_between = [&](std::uint32_t high, std::uint32_t low) { return static_cast<std::uint32_t>(signed_of(high - low) >> 3) * 0xAAAAAAABu; };
    std::uint32_t count = count_between(last, first);
    if (signed_of(count) < 17) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (limit == 0u) {
        call_set(rt, ctx, 0x08B8BC08u, {first, last, last, saved_s0});
        return;
    }
    const auto less = [&](std::uint32_t left, std::uint32_t right) {
        return call_set(rt, ctx, 0x08B8A72Cu, {word(rt, left + 8u), word(rt, left + 0xCu), word(rt, right + 8u), word(rt, right + 0xCu)}) != 0u;
    };
    std::uint32_t depth = limit;
    std::uint32_t iterations = 0u;
    const std::uint32_t temp = frame.at(8u);
    const std::uint32_t vector = frame.at(0x10u);
    for (;;) {
        const std::uint32_t half = static_cast<std::uint32_t>(signed_of(count + (count >> 31)) >> 1);
        const std::uint32_t middle = first + half * 24u;
        const std::uint32_t tail = last - 0x18u;
        --depth;
        std::uint32_t pivot;
        if (less(first, middle)) {
            if (less(middle, tail)) pivot = middle;
            else if (less(first, tail)) pivot = tail;
            else pivot = first;
        } else {
            if (less(first, tail)) pivot = first;
            else if (less(middle, tail)) pivot = tail;
            else pivot = middle;
        }
        put(rt, temp + 4u, word(rt, pivot + 4u));
        put(rt, temp, word(rt, pivot));
        put(rt, vector + 4u, 0u);
        put(rt, vector, 0u);
        const std::uint32_t bytes = (word(rt, pivot + 0xCu) - word(rt, pivot + 8u)) & ~1u;
        put(rt, vector + 8u, 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        const std::uint32_t capacity = block + bytes;
        put(rt, vector, block);
        put(rt, vector + 4u, block);
        put(rt, vector + 8u, capacity);
        const std::uint32_t end = call_set(rt, ctx, 0x08B8A654u, {word(rt, pivot + 8u), word(rt, pivot + 0xCu), block, frame.at(1u)});
        put(rt, vector + 4u, end);
        const std::uint32_t split = call_set(rt, ctx, 0x08B8B4A4u, {first, last, temp, capacity});
        call_set(rt, ctx, 0x08B8A644u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - begin) >> 1) << 1;
        if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, size, 4u});
        call_set(rt, ctx, 0x08B8BF44u, {split, last, depth, split});
        count = count_between(split, first);
        last = split;
        ctx.gpr[2] = count;
        if (signed_of(count) < 17) return;
        ++iterations;
        if (limit == iterations) {
            call_set(rt, ctx, 0x08B8BC08u, {first, last, last, split});
            return;
        }
    }
}

// func_08C30F28(vector, position, count, value): the libstdc++ vector<string>::_M_fill_insert (elements are 4-byte string
// pointers; vector = {begin, end, capacity}). Nothing for count 0. With room: a frame copy of *value (func_08B88F44);
// when more elements follow the position than count, the last `count` are copied up (func_08C30DAC), the earlier ones
// moved backward by string assignment (func_08B855E0) and the gap filled with the copy (func_08C30ED8); otherwise the
// surplus copies of the value are made at the end (func_08C30E0C), the tail copied after them (func_08C30E74) and the
// old gap filled; the copy is released last. Full: the length is checked (func_08B5ADDC(0x08CB2F1C) beyond 0x3FFFFFFF
// elements), a block of size + max(size, count) elements allocated (func_08A5BC20), the front copied (func_08C30A6C), count
// copies of the value made (func_08C30D44), the rest copied (func_08C30A6C), the old range destroyed (func_08C2FBF8) and
// freed (func_08A5BC54), the vector updated; v0 = block + bytes.
void func_08C30F28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (count == 0u) return;
    if (static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - word(rt, vector + 4u)) >> 2) >= count) {
        const std::uint32_t copy = frame.at(8u);
        call_set(rt, ctx, 0x08B88F44u, {copy, value});
        const std::uint32_t end = word(rt, vector + 4u);
        std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 2);
        if (count < after) {
            call_set(rt, ctx, 0x08C30DACu, {end - count * 4u, end, end, frame.at(7u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + count * 4u;
            std::uint32_t source = end - count * 4u;
            after = static_cast<std::uint32_t>(signed_of(source - position) >> 2);
            put(rt, vector + 4u, new_end);
            if (signed_of(after) > 0) {
                std::uint32_t target = end - 4u;
                for (std::uint32_t moved = 1u;; ++moved) {
                    source -= 4u;
                    call_set(rt, ctx, 0x08B855E0u, {target, source});
                    if (moved == after) break;
                    target -= 4u;
                }
            }
            call_set(rt, ctx, 0x08C30ED8u, {position, position + count * 4u, copy});
        } else {
            call_set(rt, ctx, 0x08C30E0Cu, {end, count - after, copy, frame.at(6u)});
            const std::uint32_t moved_end = word(rt, vector + 4u) + (count - after) * 4u;
            put(rt, vector + 4u, moved_end);
            call_set(rt, ctx, 0x08C30E74u, {position, end, moved_end, frame.at(5u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + after * 4u);
            call_set(rt, ctx, 0x08C30ED8u, {position, end, copy});
        }
        release_string(rt, ctx, copy, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        return;
    }
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - word(rt, vector)) >> 2);
    if (0x3FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB2F1Cu});
    const std::uint32_t bigger = size < count ? count : size;
    const std::uint32_t grown = size + bigger;
    const std::uint32_t bytes = grown < size ? 0xFFFFFFFCu : grown << 2;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t middle = call_set(rt, ctx, 0x08C30A6Cu, {word(rt, vector), position, block, frame.at(4u)});
    call_set(rt, ctx, 0x08C30D44u, {middle, count, value, frame.at(3u)});
    const std::uint32_t finish = call_set(rt, ctx, 0x08C30A6Cu, {position, word(rt, vector + 4u), middle + count * 4u, frame.at(2u)});
    call_set(rt, ctx, 0x08C2FBF8u, {word(rt, vector), word(rt, vector + 4u), frame.at(1u)});
    const std::uint32_t begin = word(rt, vector);
    if (begin != 0u) {
        const std::uint32_t old_bytes = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - begin) >> 2) << 2;
        call_set(rt, ctx, 0x08A5BC54u, {begin, old_bytes, 4u});
    }
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);
}

// func_0892C4B8(owner, a, index, c): for each of the two keys 0x19 and 0x1A, func_0892F95C(a, key, c, c) gives a 64-bit
// value (skipped when it is -1 in both words); func_08936584(func_089363FC() [= word 0x08CF298C], .., value, frame
// record) fills a frame record whose two 24-byte halves (frame +0x50 / +0x68 for key 0x19, +0x90 / +0xA8 for 0x1A) are
// copied to frame +0x30 / +0x18; then 3 times (entries of 0xC0 bytes in the table owner->+4 (key 0x19) or owner->+8 (key
// 0x1A) at index x 0x240): two of the halves' 8-byte items go through func_08936584 into frame buffers and
// func_0892C0C8(entry, buffer1, buffer2) takes them.
void func_0892C4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t index = ctx.gpr[6];
    const std::uint32_t c = ctx.gpr[7];
    GuestFrame frame(ctx, 0x190u);
    const std::uint32_t half_a = frame.at(0x30u);
    const std::uint32_t half_b = frame.at(0x18u);
    const std::uint32_t buffer = frame.at(0xC8u);
    const std::uint32_t second_buffer = frame.at(0u);
    const auto copy_words = [&](std::uint32_t destination, std::uint32_t source) {
        for (std::uint32_t offset = 0u; offset != 24u; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
    };
    const auto pair_pass = [&](std::uint32_t entry, std::uint32_t first_half, std::uint32_t second_half) {
        for (std::uint32_t item = 0u; item != 0x18u; item += 8u, entry += 0xC0u) {
            call_set(rt, ctx, 0x089363FCu, {});
            call_set(rt, ctx, 0x08936584u, {ctx.gpr[2], ctx.gpr[5], word(rt, first_half + item), word(rt, first_half + item + 4u), buffer});
            call_set(rt, ctx, 0x089363FCu, {});
            call_set(rt, ctx, 0x08936584u, {ctx.gpr[2], ctx.gpr[5], word(rt, second_half + item), word(rt, second_half + item + 4u), second_buffer});
            call_set(rt, ctx, 0x0892C0C8u, {entry, buffer, second_buffer});
        }
    };
    const GuestResult first = call_setf(rt, ctx, 0x0892F95Cu, {a, 0x19u, c, c}, {});
    if (!(first.v0 == 0xFFFFFFFFu && first.v1 == 0xFFFFFFFFu)) {
        call_set(rt, ctx, 0x089363FCu, {});
        call_set(rt, ctx, 0x08936584u, {ctx.gpr[2], ctx.gpr[5], first.v0, first.v1, frame.at(0x48u)});
        copy_words(half_a, frame.at(0x50u));
        copy_words(half_b, frame.at(0x68u));
        pair_pass(word(rt, owner + 4u) + index * 0x240u, half_a, half_b);
    }
    const GuestResult second = call_setf(rt, ctx, 0x0892F95Cu, {a, 0x1Au, c, ctx.gpr[7]}, {});
    if (second.v0 == 0xFFFFFFFFu && second.v1 == 0xFFFFFFFFu) return;
    call_set(rt, ctx, 0x089363FCu, {});
    call_set(rt, ctx, 0x08936584u, {ctx.gpr[2], ctx.gpr[5], second.v0, second.v1, frame.at(0x88u)});
    copy_words(half_b, frame.at(0x90u));
    copy_words(half_a, frame.at(0xA8u));
    pair_pass(word(rt, owner + 8u) + index * 0x240u, half_b, half_a);
}

// func_08AF0508(index, kind, buffer, flags): a slot pool at 0x08D41AD0 (lock word +0x538, entry table pointer +0x540, entry
// count +0x544, a default entry pointer +0x55C; entries of 64 bytes). kind must be 1 or 2, else (when the log flag
// word 0x08CDFB3C has bit 0) func_08AD83B8 logs line 0x1094 and -1 is returned. Under the lock (func_08C7AA20 / 08C7AA08) the entry is
// index (or, for a negative index, the default entry when its +8 word is not negative); a used (+0xC bit 0), missing or
// out of range entry logs (lines 0x10AF / 0x10B8 / 0x10A2) and fails with -1; a zero buffer is allocated first
// (func_08AD84B4(flags, 1); line 0x10C2 on failure and func_08AD85C8 cleanup). The entry is then initialised (+0xC = 1, kind
// byte +0x10, 0x1000 halfwords +0x14 / +0x16, buffer +0x1C, flags &~3 at +0x20, zeros) and moved to the front of the
// list headed at 0x08D42018; its word +8 is returned.
void func_08AF0508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t index = ctx.gpr[4];
    const std::uint32_t kind = ctx.gpr[5];
    std::uint32_t buffer = ctx.gpr[6];
    std::uint32_t flags = ctx.gpr[7];
    constexpr std::uint32_t kPool = 0x08D41AD0u;
    constexpr std::uint32_t kHead = 0x08D42018u;
    constexpr std::uint32_t kLogFlag = 0x08CDFB3Cu;
    const auto log = [&](std::uint32_t line, std::uint32_t text, std::uint32_t detail) {
        if ((word(rt, kLogFlag) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, line, text, detail});
    };
    if (kind - 1u >= 2u) {
        if ((word(rt, kLogFlag) & 1u) == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        log(0x1094u, 0x08C92688u, kind);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const auto fail = [&](std::uint32_t pending) {
        call_set(rt, ctx, 0x08AD85C8u, {pending});
        call_set(rt, ctx, 0x08C7AA08u, {word(rt, kPool + 0x538u), 1u});
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    call_set(rt, ctx, 0x08C7AA20u, {word(rt, kPool + 0x538u), 1u, 0u});
    std::uint32_t entry;
    if (signed_of(index) < 0) {
        const std::uint32_t fallback = word(rt, kPool + 0x55Cu);
        entry = signed_of(word(rt, fallback + 8u)) < 0 ? 0u : fallback;
        if (entry == 0u) {
            log(0x10A2u, 0x08C92678u, ctx.gpr[8]);
            return fail(0u);
        }
    } else {
        if (!(signed_of(index) < signed_of(word(rt, kPool + 0x544u)))) {
            log(0x10B8u, 0x08C92658u, index);
            return fail(0u);
        }
        entry = word(rt, kPool + 0x540u) + (index << 6);
        if ((word(rt, entry + 0xCu) & 1u) != 0u) {
            log(0x10AFu, 0x08C92638u, index);
            return fail(0u);
        }
    }
    if (buffer == 0u) {
        const std::uint32_t allocated = call_set(rt, ctx, 0x08AD84B4u, {flags, 1u});
        if (allocated == 0u) {
            log(0x10C2u, 0x08C92618u, flags);
            return fail(0u);
        }
        buffer = allocated;
    }
    const std::uint32_t first = word(rt, entry);
    flags &= ~3u;
    put(rt, entry + 0x24u, 0u);
    put(rt, entry + 0x30u, 0u);
    put(rt, entry + 0x34u, 0u);
    put(rt, entry + 0x38u, 0u);
    put(rt, entry + 0xCu, 1u);
    put8(rt, entry + 0x10u, kind);
    put16(rt, entry + 0x16u, 0x1000u);
    put(rt, entry + 0x1Cu, buffer);
    put(rt, entry + 0x20u, flags);
    put16(rt, entry + 0x14u, 0x1000u);
    put(rt, entry + 0x28u, 0u);
    put(rt, entry + 0x2Cu, 0u);
    put8(rt, entry + 0x11u, 0u);
    put(rt, entry + 0x3Cu, 0u);
    if (first != entry) {
        put(rt, first + 4u, word(rt, entry + 4u));
        const std::uint32_t previous = word(rt, entry + 4u);
        put(rt, entry + 4u, entry);
        put(rt, previous, first);
        put(rt, entry, entry);
    }
    const std::uint32_t head = word(rt, kHead);
    put(rt, entry + 4u, kHead);
    put(rt, head + 4u, entry);
    put(rt, entry, head);
    put(rt, word(rt, entry + 4u), entry);
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, kHead - 0x10u), 1u});
    ctx.gpr[2] = word(rt, entry + 8u);
}

// func_08AF98F0(self, base): rebases the object at self: a 16-byte relocation record in the frame {self - old base, base -
// old base value (0 when base is 0), old base = self->+8, old base value = self->+0x6C} is built, self->+8 = self,
// self->+0x6C = base, and the record is applied to each field in turn: func_08C416DC / func_08C41B64 / func_08C418A4 /
// func_08C41A6C / func_08C41514 / func_08C41444 / func_08C413DC / func_08C41D00 / func_08C41BE4 / func_08C41C80 /
// func_08C4176C / func_08C411D4 / func_08C41144 / func_08C41074 / func_08C4100C / func_08C40FA4 / func_08C40EBC (record,
// &field, count halfword), func_08B5379C on each 32-byte entry of the array at +0xA8 and func_08B5378C on +0x94, and the
// sub-objects at +0x48 / +0x4C / +0x84 through func_08AF771C / func_08AFC9B4 / func_08AFD484. Then the default pointer
// +0x68 (self + 0xB0 when unset) is checked against the counts +0x20 / +0x1E; func_08AF7FF8(self) may run before
// func_08AF93DC(self, +0x68, 0, 0), and word 0 of *(+0x68) = self; v0 = self->+0x68.
void func_08AF98F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t base = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t record = frame.at(0u);
    const std::uint32_t old_base = word(rt, self + 8u);
    const std::uint32_t old_value = word(rt, self + 0x6Cu);
    put(rt, record, self - old_base);
    put(rt, record + 4u, base - old_value);
    put(rt, record + 8u, old_base);
    put(rt, record + 0xCu, old_value);
    if (base == 0u) {
        put(rt, record + 0xCu, 0u);
        put(rt, record + 4u, 0u);
    }
    put(rt, self + 8u, self);
    put(rt, self + 0x6Cu, base);
    // func(record, &field at self + offset, count halfword at self + count_offset)
    const auto field = [&](std::uint32_t function, std::uint32_t offset, std::uint32_t count_offset) {
        const std::uint32_t count = half_u(rt, self + count_offset);
        call_set(rt, ctx, function, {record, self + offset, count});
    };
    const auto plain = [&](std::uint32_t function, std::uint32_t offset) { call_set(rt, ctx, function, {record, self + offset}); };
    field(0x08C416DCu, 0x88u, 0x2Cu);
    field(0x08C41B64u, 0x90u, 0x8Eu);
    field(0x08C418A4u, 0x30u, 0x10u);
    field(0x08C41B64u, 0x34u, 0x12u);
    field(0x08C41A6Cu, 0x38u, 0x14u);
    field(0x08C41B64u, 0x3Cu, 0x16u);
    if (word(rt, self + 0xA8u) != 0u) {
        plain(0x08C4199Cu, 0xA8u);
        if (half_u(rt, self + 0x14u) != 0u) {
            std::uint32_t index = 0u;
            do {
                call_set(rt, ctx, 0x08B5379Cu, {word(rt, record), word(rt, self + 0xA8u) + (index << 5), word(rt, record + 4u), word(rt, record + 8u), word(rt, record + 0xCu)});
                ++index;
            } while (signed_of(index) < signed_of(half_u(rt, self + 0x14u)));
        }
    }
    field(0x08C41514u, 0x44u, 0x1Cu);
    plain(0x08C41444u, 0x48u);
    if (word(rt, self + 0x48u) != 0u) call_set(rt, ctx, 0x08AF771Cu, {word(rt, self + 0x48u), base});
    plain(0x08C413DCu, 0x4Cu);
    if (word(rt, self + 0x4Cu) != 0u) call_set(rt, ctx, 0x08AFC9B4u, {word(rt, self + 0x4Cu), base});
    field(0x08C41D00u, 0x50u, 0x1Au);
    field(0x08C41BE4u, 0x54u, 0x2Au);
    field(0x08C41C80u, 0x58u, 0x1Eu);
    field(0x08C41C80u, 0x5Cu, 0x22u);
    plain(0x08C4176Cu, 0x64u);
    plain(0x08C411D4u, 0x68u);
    field(0x08C41144u, 0x78u, 0x26u);
    plain(0x08C41074u, 0x84u);
    if (word(rt, self + 0x84u) != 0u) call_set(rt, ctx, 0x08AFD484u, {word(rt, self + 0x84u), record});
    if (half_u(rt, self + 0xCu) != 0u) {
        plain(0x08C4100Cu, 0x94u);
        const std::uint32_t pointer = word(rt, self + 0x94u);
        if (pointer != 0u) call_set(rt, ctx, 0x08B5378Cu, {word(rt, record), pointer, word(rt, record + 4u), word(rt, record + 8u), word(rt, record + 0xCu)});
    }
    plain(0x08C40FA4u, 0x9Cu);
    field(0x08C41B64u, 0xA0u, 0x9Au);
    field(0x08C40EBCu, 0xA4u, 0x28u);
    std::uint32_t current = word(rt, self + 0x68u);
    if (current == 0u) {
        put(rt, self + 0x68u, self + 0xB0u);
    } else {
        bool repair = false;
        if (half_u(rt, self + 0x20u) != 0u && word(rt, current + 0xCu) == 0u) repair = true;
        else if (half_u(rt, self + 0x1Eu) != 0u) repair = word(rt, current + 0x10u) == 0u || word(rt, current + 0x14u) == 0u;
        if (repair) call_set(rt, ctx, 0x08AF7FF8u, {self, current});
        current = word(rt, self + 0x68u);
        call_set(rt, ctx, 0x08AF93DCu, {self, current, 0u, 0u});
    }
    const std::uint32_t pointer = word(rt, self + 0x68u);
    put(rt, pointer, self);
    ctx.gpr[2] = pointer;
}

// func_0899EFFC(result, a1, count, arg): a pair of lookups through value objects of the frame (func_0899DD60 builds a
// holder; checked words: func_08A5BB7C(0x08CA496C / 0x08CA4A04 / 0x08CA485C) when null). For count > 0: func_08A98674(object,
// arg), then the virtual slot 0x84 of that object (a1, 0 and a1, 1) gives two floats through slot 0x54 which go to
// func_0899D0B0(holder, f) and func_0899D14C(holder, f); everything destroyed. Otherwise: func_08A98A70 fills the second
// object, and two floats (func_0899D0A8, func_0899D144 of the first) are built into value objects (func_08A87EEC) and
// appended to the vector at holder + 0x14 (construct at the end, func_08B8A03C when full); the second object goes to
// *result (func_08A5BAD8 unless it is result itself); both are destroyed.
void func_0899EFFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t arg = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (signed_of(count) > 0) {
        const std::uint32_t second = frame.at(0x14u);
        const std::uint32_t first = frame.at(0x10u);
        call_set(rt, ctx, 0x0899DD60u, {second});
        call_set(rt, ctx, 0x08A98674u, {first, arg});
        const auto load = [&](std::uint32_t &holder, std::uint32_t &object) {
            holder = word(rt, second);
            object = word(rt, first);
            if (holder == 0u) {
                call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
                object = word(rt, first);
            }
            if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4A04u});
        };
        const auto read_float = [&](std::uint32_t temporary, std::uint32_t object, std::uint32_t selector) {
            call_virtual_v1(rt, ctx, object, 0x84u, {temporary, object, selector});
            const std::uint32_t value = word(rt, temporary);
            if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
            call_virtual_at(rt, ctx, value, 0x54u, {value});
            return ctx.fpr[0];
        };
        std::uint32_t holder;
        std::uint32_t object;
        load(holder, object);
        const std::uint32_t low = frame.at(0xCu);
        float amount = read_float(low, object, 0u);
        call_setf(rt, ctx, 0x0899D0B0u, {holder}, {amount});
        call_set(rt, ctx, kDestroy, {low});
        load(holder, object);
        const std::uint32_t high = frame.at(8u);
        amount = read_float(high, object, 1u);
        call_setf(rt, ctx, 0x0899D14Cu, {holder}, {amount});
        call_set(rt, ctx, kDestroy, {high});
        call_set(rt, ctx, kDestroy, {first});
        call_set(rt, ctx, kDestroy, {second});
        return;
    }
    const std::uint32_t first = frame.at(0x10u);
    const std::uint32_t second = frame.at(0x14u);
    call_set(rt, ctx, 0x0899DD60u, {first});
    call_set(rt, ctx, 0x08A98A70u, {second});
    const auto load = [&](std::uint32_t &holder, std::uint32_t &object) {
        holder = word(rt, second);
        object = word(rt, first);
        if (holder == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4A04u});
            object = word(rt, first);
        }
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    };
    const auto append = [&](std::uint32_t holder, std::uint32_t temporary) {
        const std::uint32_t vector = holder + 0x14u;
        const std::uint32_t end = word(rt, vector + 4u);
        if (end == word(rt, vector + 8u)) {
            call_set(rt, ctx, 0x08B8A03Cu, {vector, end, temporary});
            return;
        }
        if (end != 0u) call_set(rt, ctx, 0x08A76ABCu, {end, temporary});
        put(rt, vector + 4u, word(rt, vector + 4u) + 4u);
    };
    std::uint32_t holder;
    std::uint32_t object;
    load(holder, object);
    const std::uint32_t temporary = frame.at(4u);
    call_setf(rt, ctx, 0x08A87EECu, {temporary}, {call_setf(rt, ctx, 0x0899D0A8u, {object}, {}).f0});
    append(holder, temporary);
    call_set(rt, ctx, kDestroy, {temporary});
    load(holder, object);
    const std::uint32_t temporary2 = frame.at(0u);
    call_setf(rt, ctx, 0x08A87EECu, {temporary2}, {call_setf(rt, ctx, 0x0899D144u, {object}, {}).f0});
    append(holder, temporary2);
    call_set(rt, ctx, kDestroy, {temporary2});
    if (result != second) call_set(rt, ctx, kAssign, {result, word(rt, second)});
    call_set(rt, ctx, kDestroy, {second});
    call_set(rt, ctx, kDestroy, {first});
}

// func_088638A8(holder, a1) -> a tail jump to func_08863630(holder->+0, a1). (The code after its two instructions, from
// 0x088638B0, is a separate function that no jump of this one reaches.)
void func_088638A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08863630u, {word(rt, ctx.gpr[4])});
}

// func_08BBF0D0(vector, position, count, value): the libstdc++ vector::_M_fill_insert for 16-byte elements {string, word,
// word, word} (vector = {begin, end, capacity}). Nothing for count 0. With room: a frame element made from *value (string
// copy func_08B9432C, the three words copied); when more elements follow the position than count, the last `count` are
// copied up (func_08BBEF50), the earlier ones moved backward (string assign func_08B957D8 + the three words) and the gap
// filled (func_08BBD954); otherwise the surplus copies are made at the end (func_08BBEFC8), the tail copied after them
// (func_08BBF048) and the old gap filled; the frame string is released (empty rep 0x08D75298, free func_08B93554). Full: length
// checked (func_08B5ADDC(0x08CA0DC4) beyond 0x0FFFFFFF elements), a block of size + max(size, count) elements allocated
// (func_08B35DA0), front copied (func_08BBE8D0), count copies made (func_08BBE850), the rest copied, the old range destroyed
// (func_08BBEBE4) and freed (func_08B35254), the vector updated; v0 = block + bytes.
void func_08BBF0D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (count == 0u) return;
    if (static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - word(rt, vector + 4u)) >> 4) >= count) {
        const std::uint32_t copy = frame.at(0xCu);
        call_set(rt, ctx, 0x08B9432Cu, {copy, value});
        const std::uint32_t end = word(rt, vector + 4u);
        std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 4);
        put(rt, frame.at(0x10u), word(rt, value + 4u));
        put(rt, frame.at(0x14u), word(rt, value + 8u));
        put(rt, frame.at(0x18u), word(rt, value + 0xCu));
        if (count < after) {
            call_set(rt, ctx, 0x08BBEF50u, {end - count * 16u, end, end, frame.at(7u)});
            std::uint32_t source = end - count * 16u;
            after = static_cast<std::uint32_t>(signed_of(source - position) >> 4);
            put(rt, vector + 4u, word(rt, vector + 4u) + count * 16u);
            if (signed_of(after) > 0) {
                std::uint32_t target = end;
                for (std::uint32_t moved = 0u; moved != after; ++moved) {
                    target -= 0x10u;
                    source -= 0x10u;
                    call_set(rt, ctx, 0x08B957D8u, {target, source});
                    const std::uint32_t w1 = word(rt, source + 4u);
                    const std::uint32_t w2 = word(rt, source + 8u);
                    const std::uint32_t w3 = word(rt, source + 0xCu);
                    put(rt, target + 4u, w1);
                    put(rt, target + 8u, w2);
                    put(rt, target + 0xCu, w3);
                }
            }
            call_set(rt, ctx, 0x08BBD954u, {position, position + count * 16u, copy});
        } else {
            call_set(rt, ctx, 0x08BBEFC8u, {end, count - after, copy, frame.at(6u)});
            const std::uint32_t moved_end = word(rt, vector + 4u) + (count - after) * 16u;
            put(rt, vector + 4u, moved_end);
            call_set(rt, ctx, 0x08BBF048u, {position, end, moved_end, frame.at(5u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + after * 16u);
            call_set(rt, ctx, 0x08BBD954u, {position, end, copy});
        }
        release_string(rt, ctx, copy, 0x08D75298u, 0x08B93554u, frame.at(0u));
        return;
    }
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - word(rt, vector)) >> 4);
    if (0x0FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0DC4u});
    const std::uint32_t bigger = size < count ? count : size;
    const std::uint32_t grown = size + bigger;
    const bool overflow = grown < size;
    const std::uint32_t bytes = overflow ? 0xFFFFFFF0u : grown << 4;
    std::uint32_t block = 0u;
    if (overflow || grown != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t middle = call_set(rt, ctx, 0x08BBE8D0u, {word(rt, vector), position, block, frame.at(4u)});
    call_set(rt, ctx, 0x08BBE850u, {middle, count, value, frame.at(3u)});
    const std::uint32_t finish = call_set(rt, ctx, 0x08BBE8D0u, {position, word(rt, vector + 4u), middle + count * 16u, frame.at(2u)});
    call_set(rt, ctx, 0x08BBEBE4u, {word(rt, vector), word(rt, vector + 4u), frame.at(1u)});
    const std::uint32_t begin = word(rt, vector);
    if (begin != 0u) call_set(rt, ctx, 0x08B35254u, {begin});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);
}

// func_08B65F0C(x = f12): the libm float arctangent (fdlibm's atanf shape), result in f0. ix = |x| bits: above 0x507FFFFF a
// NaN gives x + x, else +-(pi/2 hi + lo) (constants 0x08CB6264 / 0x08CB6268); below 2^-29 x itself (the 0x08CB626C "huge" test);
// from 0.4375 the argument is reduced by the id = 0..3 step (|x| from func_08B663A4 as the fabs): (2|x| - 1) / (2 + |x|),
// (|x| - 1) / (|x| + 1), (|x| - 1.5) / (1 + 1.5 |x|) or -1 / |x| (constants 0x08CB6270.. 0x08CB627C); then the polynomial
// of the 11 coefficients at 0x08CB6280..0x08CB62A8 in z = x x: without a reduction (id -1) x - x (s1 + s2), else
// atan_hi[id] - ((x (s1 + s2) - atan_lo[id]) - x) (tables 0x08C96034 / 0x08C96024), negated for a negative input.
void func_08B65F0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    const std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t ix = hx & 0x7FFFFFFFu;
    const auto constant = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto finish = [&](float result) { ctx.fpr[0] = result; };
    std::int32_t id;
    if (signed_of(ix) > 0x507FFFFF) {
        if (signed_of(ix) > 0x7F800000) return finish(x + x);
        if (signed_of(hx) <= 0) return finish((-constant(0x08CB6264u)) - constant(0x08CB6268u));
        return finish(constant(0x08CB6264u) + constant(0x08CB6268u));
    }
    if (signed_of(ix) > 0x3EDFFFFF) {
        const float magnitude = call_setf(rt, ctx, 0x08B663A4u, {}, {ctx.fpr[12]}).f0;
        if (signed_of(ix) > 0x3F97FFFF) {
            if (signed_of(ix) > 0x401BFFFF) {
                id = 3;
                x = constant(0x08CB627Cu) / magnitude;
            } else {
                id = 2;
                const float three_halves = constant(0x08CB6278u);
                const float scaled = psp_mul(magnitude, three_halves);
                const float numerator = magnitude - three_halves;
                x = numerator / (scaled + constant(0x08CB6270u));
            }
        } else if (signed_of(ix) > 0x3F2FFFFF) {
            id = 1;
            const float one = constant(0x08CB6270u);
            x = (magnitude - one) / (magnitude + one);
        } else {
            id = 0;
            const float doubled = magnitude + magnitude;
            const float numerator = doubled - constant(0x08CB6270u);
            x = numerator / (magnitude + constant(0x08CB6274u));
        }
    } else {
        id = -1;
        if (!(signed_of(ix) > 0x30FFFFFF)) {
            if (constant(0x08CB6270u) < x + constant(0x08CB626Cu)) return finish(x);
        }
    }
    const float z = psp_mul(x, x);
    const float w = psp_mul(z, z);
    float even = psp_mul(w, constant(0x08CB6280u));
    float odd = psp_mul(w, constant(0x08CB6298u));
    even = even + constant(0x08CB6284u);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB629Cu);
    even = even + constant(0x08CB6288u);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB62A0u);
    even = even + constant(0x08CB628Cu);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB62A4u);
    even = even + constant(0x08CB6290u);
    odd = psp_mul(w, odd);
    even = psp_mul(w, even);
    odd = odd + constant(0x08CB62A8u);
    even = even + constant(0x08CB6294u);
    odd = psp_mul(w, odd);
    even = psp_mul(z, even);
    if (id < 0) {
        const float sum = even + odd;
        return finish(x - psp_mul(x, sum));
    }
    float rest = even + odd;
    rest = psp_mul(x, rest);
    rest = rest - float_at(rt, 0x08C96024u + static_cast<std::uint32_t>(id) * 4u);
    rest = rest - x;
    const float result = float_at(rt, 0x08C96034u + static_cast<std::uint32_t>(id) * 4u) - rest;
    finish(signed_of(hx) >= 0 ? result : -result);
}

// func_08A20010(target): a string (func_08A69B3C(frame, 0x08CAA6BC)) names a new 0x38-byte object (func_08A5BC20(0x38, 4),
// initialised by func_08A7E914(object, string)) which is stored in a value object in the frame and assigned to *target
// (func_08A76AB4(target, &holder)); the string is destroyed (func_08A69D68). Then 13 members are registered on *target
// (func_08A5BB7C(0x08CAA6C8) when it is null): func_08B86FB0(*target, name, function) for the (name, guest function) pairs of
// the table below. Returns target.
void func_08A20010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t name = frame.at(0u);
    const std::uint32_t holder = frame.at(4u);
    call_set(rt, ctx, 0x08A69B3Cu, {name, 0x08CAA6BCu});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call_set(rt, ctx, 0x08A7E914u, {object, name});
    put(rt, holder, object);
    call_set(rt, ctx, 0x08A76AB4u, {target, holder});
    call_set(rt, ctx, 0x08A69D68u, {name});
    static constexpr std::uint32_t kMembers[13][2] = {
        {0x08CAA6D0u, 0x08A2041Cu}, {0x08CAA6D8u, 0x08A20414u}, {0x08CAA6E0u, 0x08A2040Cu}, {0x08CAA6E8u, 0x08A20404u},
        {0x08CAA6F0u, 0x08A203FCu}, {0x08CAA6F8u, 0x08A20360u}, {0x08CAA700u, 0x08A1FF94u}, {0x08CAA70Cu, 0x08A206E8u},
        {0x08CAA718u, 0x08A20424u}, {0x08CAA724u, 0x08A20514u}, {0x08CAA734u, 0x08A2055Cu}, {0x08CAA740u, 0x08A205ACu},
        {0x08CAA74Cu, 0x08A205FCu}};
    for (const auto &member : kMembers) {
        const std::uint32_t owner = word(rt, target);
        if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA6C8u});
        call_set(rt, ctx, 0x08B86FB0u, {owner, member[0], member[1]});
    }
    ctx.gpr[2] = target;
}

// func_0881BF5C(owner, mode, args, out64, out32): the object a = args[0] is classified by func_08A76F20(a, type) against the
// types func_08A5D70C() / func_08A82A5C() (slot 0x50 of a gives the 64-bit pair to *out64), func_08A85448() / func_08A5F374()
// (index = slot 0x3C of a; when it is in 0..owner->+0 the 0x100-byte frame buffer func_0893119C(index, buffer, 0x100)
// then func_08930170(buffer) gives the pair, else the pair is -1 and *out32 = -1 with mode 2) or func_08854634() (the pair
// from func_088A5EE0 of the object func_08854678(func_088547C4(args)) and *out32 = func_088A6164 of it, result 1). For the
// first two kinds the mode (< 2 gives *out32 = -1 and result 1, else *out32 = slot 0x3C of args[1] and result 2). The pair
// is then checked with func_08935B10 on a frame context (func_0893600C, destroyed by func_08932448): a failed check
// sets the pair to -1 and *out32 = 0. Returns the result (0 for an unknown type). A null object runs
// func_08A5BB7C(0x08C98EFC) first at every load.
void func_0881BF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    std::uint32_t mode = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[6];
    const std::uint32_t out64 = ctx.gpr[7];
    const std::uint32_t out32 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x2B0u);
    constexpr std::uint32_t kNull = 0x08C98EFCu;
    const auto reload = [&]() {
        const std::uint32_t object = word(rt, args);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
        return object;
    };
    const auto is_a = [&](std::uint32_t object, std::uint32_t type_getter) {
        const std::uint32_t type = call_set(rt, ctx, type_getter, {});
        return call_set(rt, ctx, 0x08A76F20u, {object, type}) != 0u;
    };
    const auto second_slot = [&]() {
        const std::uint32_t object = word(rt, args + 4u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
        return call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    };
    const auto finish = [&]() {
        const std::uint32_t context = frame.at(0x108u);
        call_set(rt, ctx, 0x0893600Cu, {context});
        const std::uint32_t low = word(rt, out64);
        const std::uint32_t high = word(rt, out64 + 4u);
        if (call_set(rt, ctx, 0x08935B10u, {context, ctx.gpr[5], low, high}) == 0u) {
            put(rt, out64, 0xFFFFFFFFu);
            put(rt, out64 + 4u, 0xFFFFFFFFu);
            put(rt, out32, 0u);
        }
        call_set(rt, ctx, 0x08932448u, {context});
        ctx.gpr[2] = mode;
    };
    const auto tail_pair = [&]() {
        if (signed_of(mode) < 2) {
            mode = 1u;
            put(rt, out32, 0xFFFFFFFFu);
        } else {
            const std::uint32_t value = second_slot();
            mode = 2u;
            put(rt, out32, value);
        }
    };
    std::uint32_t object = word(rt, args);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {kNull});
    bool direct = is_a(object, 0x08A5D70Cu);
    if (!direct) {
        object = reload();
        direct = is_a(object, 0x08A82A5Cu);
    }
    if (direct) {
        object = reload();
        call_virtual_at(rt, ctx, object, 0x50u, {object});
        put(rt, out64, ctx.gpr[2]);
        put(rt, out64 + 4u, ctx.gpr[3]);
        tail_pair();
        return finish();
    }
    object = reload();
    bool indexed = is_a(object, 0x08A85448u);
    if (!indexed) {
        object = reload();
        indexed = is_a(object, 0x08A5F374u);
    }
    if (indexed) {
        object = reload();
        const std::uint32_t index = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
        if (signed_of(index) < 0 || !(signed_of(index) < signed_of(word(rt, owner)))) {
            put(rt, out64, 0xFFFFFFFFu);
            put(rt, out64 + 4u, 0xFFFFFFFFu);
            mode = 2u;
            put(rt, out32, 0xFFFFFFFFu);
            return finish();
        }
        const std::uint32_t buffer = frame.at(4u);
        call_set(rt, ctx, 0x0893119Cu, {index, buffer, 0x100u});
        call_set(rt, ctx, 0x08930170u, {buffer});
        put(rt, out64, ctx.gpr[2]);
        put(rt, out64 + 4u, ctx.gpr[3]);
        tail_pair();
        return finish();
    }
    object = reload();
    mode = 0u;
    const std::uint32_t type = call_set(rt, ctx, 0x08854634u, {});
    if (call_set(rt, ctx, 0x08A76F20u, {object, type}) != 0u) {
        const std::uint32_t holder = frame.at(0u);
        call_set(rt, ctx, 0x088547C4u, {holder, args});
        const std::uint32_t inner = word(rt, holder);
        if (inner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98F58u});
        const std::uint32_t resolved = call_set(rt, ctx, 0x08854678u, {inner});
        call_set(rt, ctx, kDestroy, {holder});
        call_set(rt, ctx, 0x088A5EE0u, {resolved});
        put(rt, out64, ctx.gpr[2]);
        put(rt, out64 + 4u, ctx.gpr[3]);
        mode = 1u;
        const std::uint32_t extra = call_set(rt, ctx, 0x088A6164u, {resolved});
        put(rt, out32, extra);
    }
    finish();
}

// func_08A6A40C(result, source): *result = the C string at source->+4 as an identifier: a working string (frame +4, starts
// as the empty rep 0x08D74FD0) gets every character appended (reserve func_08B864EC(string, length + 1) when the capacity is
// too small or the string shared); letters, digits and '_' as they are, any other character (a backslash first for ') marks
// the name as needing quotes, as does a first character that is neither a letter nor '_'. Without the mark the result is
// func_08B88F44(result, copy) of the working string (assigned with func_08B855E0 to a second one); with it the second
// string is quote (0x08CB04CC) + func_08B89054(working) + quote (func_08BEED1C, 1 character each). Strings released (free
// func_08B8542C); returns result.
void func_08A6A40C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kFree = 0x08B8542Cu;
    const std::uint32_t working = frame.at(4u);
    const std::uint32_t second = frame.at(8u);
    std::uint32_t text = word(rt, source + 4u);
    put(rt, working, kEmpty + 0xCu);
    std::uint32_t character = sext8(byte_u(rt, text));
    if (character == 0u) {
        call_set(rt, ctx, 0x08B88F44u, {result, working});
        release_string(rt, ctx, working, kEmpty, kFree, frame.at(0u));
        ctx.gpr[2] = result;
        return;
    }
    const auto is_letter = [](std::uint32_t value) { return value - 0x41u < 26u || value - 0x61u < 26u; };
    bool quoted = false;
    if (!is_letter(character)) quoted = character != 0x5Fu;
    const auto append = [&](std::uint32_t value) {
        std::uint32_t data = word(rt, working);
        const std::uint32_t length = word(rt, data - 0xCu);
        const std::uint32_t wanted = length + 1u;
        std::uint32_t current = length;
        if (word(rt, data - 8u) < wanted || signed_of(word(rt, data - 4u)) > 0) {
            call_set(rt, ctx, 0x08B864ECu, {working, wanted});
            data = word(rt, working);
            current = word(rt, data - 0xCu);
        }
        put8(rt, data + current, value);
        data = word(rt, working);
        put(rt, data - 4u, 0u);
        put(rt, data - 0xCu, wanted);
        put8(rt, data + wanted, 0u);
    };
    for (;;) {
        if (is_letter(character) || character - 0x30u < 10u || character == 0x5Fu) {
            append(character);
        } else {
            if (character == 0x27u) append(0x5Cu);
            quoted = true;
            append(character);
        }
        ++text;
        character = sext8(byte_u(rt, text));
        if (character == 0u) break;
    }
    put(rt, second, kEmpty + 0xCu);
    if (quoted) {
        call_set(rt, ctx, 0x08BEED1Cu, {second, 0x08CB04CCu, 1u});
        call_set(rt, ctx, 0x08B89054u, {second, working});
        call_set(rt, ctx, 0x08BEED1Cu, {second, 0x08CB04CCu, 1u});
    } else {
        call_set(rt, ctx, 0x08B855E0u, {second, working});
    }
    call_set(rt, ctx, 0x08B88F44u, {result, second});
    release_string(rt, ctx, second, kEmpty, kFree, frame.at(1u));
    release_string(rt, ctx, working, kEmpty, kFree, frame.at(0u));
    ctx.gpr[2] = result;
}
} // namespace hot_batch_114
} // namespace gt::game

#define HOT114(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_114::name, gt::ReturnKind::kind)
HOT114(0x089AECECu, func_089AECEC, Word);
HOT114(0x0880F7E0u, func_0880F7E0, Word);
HOT114(0x08B13B84u, func_08B13B84, Word);
HOT114(0x0899A048u, func_0899A048, Word);
// HOT114(0x088DC974u, func_088DC974, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT114(0x0894D6D0u, func_0894D6D0, Word);
// HOT114(0x089C8C70u, func_089C8C70, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT114(0x08B0B8CCu, func_08B0B8CC, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT114(0x08A7F5FCu, func_08A7F5FC, Word);
HOT114(0x0891179Cu, func_0891179C, Word);
// HOT114(0x088D2098u, func_088D2098, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT114(0x08810010u, func_08810010, Word);
HOT114(0x08B6DFD0u, func_08B6DFD0, Float);
HOT114(0x08B8BF44u, func_08B8BF44, Word);
HOT114(0x08C30F28u, func_08C30F28, Word);
HOT114(0x0892C4B8u, func_0892C4B8, Word);
HOT114(0x08AF0508u, func_08AF0508, Word);
HOT114(0x08AF98F0u, func_08AF98F0, Word);
HOT114(0x0899EFFCu, func_0899EFFC, Word);
// HOT114(0x088638A8u, func_088638A8, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT114(0x08BBF0D0u, func_08BBF0D0, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT114(0x08B65F0Cu, func_08B65F0C, Float);
// HOT114(0x08A20010u, func_08A20010, Word);  // batch 114: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT114(0x0881BF5Cu, func_0881BF5C, Word);
HOT114(0x08A6A40Cu, func_08A6A40C, Word);
