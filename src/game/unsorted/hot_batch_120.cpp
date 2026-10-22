// Hot batch 120: functions of 300-400 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_120 {
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


// A virtual call `object->slot(f12)` with one float argument (`lw v1, 0(object); lw v0, slot(v1); jalr v0`).
inline float call_virtual_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object,
                                std::uint32_t slot, float argument) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_setf(rt, ctx, ctx.gpr[2], {object}, {argument}).f0;
}

// func_0892381C(self, matrix): the driver-view sway matrix. The owner (self+0xC)->+0x244 gives its velocity (slot 0x38);
// three stages each turn it by self+0x10 / +0x20 / +0x30 (func_08B4A8D4) and scale it by 0x08CDDD68 (func_08B4A6D4),
// and the stages become the new +0x10..+0x3C (the third one zeroed while the counter self+0x1CC runs down). The last
// stage, rotated by the matrix's angles (+0x50..+0x58), gives three lanes clamped to [-0x08C8A638, 500] / 500 where
// their size passes 200 (flag), faded below 60 km/h of func_08B49D7C(length of stage two) x 3.6; a flag seen twice in a
// row clears them (byte self+0x1D0). Unless the view object (func_089260EC(self+0xC))->slot 8 accepts 2 or 4, the
// matrix gets offset / pitch / yaw / roll through the filters at +0x148 / +0x1A0 / +0x174 (mode self+8 == 1), or
// pitch / yaw through the filters at +0x40 / +0x98.
void func_0892381C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t matrix = ctx.gpr[5];
    GuestFrame frame(ctx, 0x150u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto copy = [&](std::uint32_t to, std::uint32_t from) {
        for (std::uint32_t i = 0u; i < 16u; i += 4u) put(rt, to + i, word(rt, from + i));
    };
    const float scale = float_at(rt, 0x08CDDD68u);
    const std::uint32_t owner = word(rt, word(rt, self + 0xCu) + 0x244u);
    copy(at(0x3Cu), call_virtual_at(rt, ctx, owner, 0x38u, {owner}));
    call_set(rt, ctx, 0x08B4A8D4u, {at(0x3Cu), at(0x3Cu), self + 0x10u});
    copy(at(0x4Cu), at(0x3Cu));
    call_setf(rt, ctx, 0x08B4A6D4u, {at(0x4Cu)}, {scale});
    copy(at(0x1Cu), at(0x4Cu));
    copy(at(0x5Cu), at(0x4Cu));
    call_set(rt, ctx, 0x08B4A8D4u, {at(0x1Cu), at(0x1Cu), self + 0x20u});
    copy(at(0x2Cu), at(0x1Cu));
    call_setf(rt, ctx, 0x08B4A6D4u, {at(0x2Cu)}, {scale});
    copy(at(0xCu), at(0x2Cu));
    std::uint32_t third[4];
    for (std::uint32_t i = 0u; i < 4u; ++i) third[i] = word(rt, at(0x2Cu + i * 4u));
    call_set(rt, ctx, 0x08B4A8D4u, {at(0xCu), at(0xCu), self + 0x30u});
    copy(at(0x6Cu), at(0xCu));
    const std::uint32_t countdown = word(rt, self + 0x1CCu);
    if (signed_of(countdown) > 0) {
        put(rt, self + 0x1CCu, countdown - 1u);
        for (std::uint32_t i = 0u; i < 16u; i += 4u) put(rt, at(0x6Cu + i), 0u);
    }
    copy(self + 0x10u, call_virtual_at(rt, ctx, owner, 0x38u, {owner}));
    copy(self + 0x20u, at(0x5Cu));
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, self + 0x30u + i * 4u, third[i]);
    const std::uint32_t turn = at(0x90u);
    call_set(rt, ctx, 0x08B4E648u, {turn});
    call_setf(rt, ctx, 0x08B4F47Cu, {turn}, {float_at(rt, matrix + 0x58u)});
    call_setf(rt, ctx, 0x08B4F2E4u, {turn}, {-float_at(rt, matrix + 0x50u)});
    call_setf(rt, ctx, 0x08B4F3B0u, {turn}, {-float_at(rt, matrix + 0x54u)});
    const std::uint32_t lanes = at(0x7Cu);
    call_set(rt, ctx, 0x08B4A774u, {lanes, turn, at(0x6Cu)});
    const float low = -float_at(rt, 0x08C8A638u);
    bool flagged = false;
    for (std::uint32_t i = 0u; i < 3u; ++i) {
        put(rt, at(i * 4u), 0u);
        float x = float_at(rt, lanes + i * 4u);
        if (!(200.0f < std::fabs(x))) continue;
        flagged = true;
        if (x < low) {
            x = low;
            put_float(rt, lanes + i * 4u, low);
        }
        if (500.0f < x) {
            x = 500.0f;
            put_float(rt, lanes + i * 4u, 500.0f);
        }
        const float ratio = x / 500.0f;
        put_float(rt, lanes + i * 4u, ratio);
        put_float(rt, at(i * 4u), ratio);
    }
    call_set(rt, ctx, 0x08B4A744u, {at(0x5Cu)});
    const float speed = psp_mul(call_setf(rt, ctx, 0x08B49D7Cu, {}, {ctx.fpr[0]}).f0, std::bit_cast<float>(0x40666666u));
    if (speed < 60.0f) {
        const float fade = speed / 60.0f;
        put_float(rt, at(8u), psp_mul(fade, float_at(rt, at(8u))));
        put_float(rt, at(0u), psp_mul(float_at(rt, at(0u)), fade));
        put_float(rt, at(4u), psp_mul(fade, float_at(rt, at(4u))));
    }
    if (!flagged) {
        put8(rt, self + 0x1D0u, 0u);
    } else if (byte_u(rt, self + 0x1D0u) != 1u) {
        put8(rt, self + 0x1D0u, 1u);
    } else {
        for (std::uint32_t i = 0u; i < 12u; i += 4u) put(rt, at(i), 0u);
        put8(rt, self + 0x1D0u, 0u);
    }
    const std::uint32_t view = call_set(rt, ctx, 0x089260ECu, {word(rt, self + 0xCu)});
    if (call_virtual_at(rt, ctx, view, 8u, {view, 2u}) != 0u) return;
    if (call_virtual_at(rt, ctx, view, 8u, {view, 4u}) != 0u) return;
    if (word(rt, self + 8u) != 1u) {
        const float lateral = float_at(rt, at(8u));
        const float pitch_in = psp_mul(lateral, std::bit_cast<float>(0xC0060A92u));
        float yaw_in = 0.0f;
        if (std::fabs(lateral) < 0.5f) yaw_in = psp_mul(float_at(rt, at(0u)), std::bit_cast<float>(0x40060A92u));
        call_setf(rt, ctx, 0x08B4F2E4u, {matrix}, {call_virtual_float(rt, ctx, self + 0x40u, 0x14u, pitch_in)});
        call_setf(rt, ctx, 0x08B4F47Cu, {matrix}, {call_virtual_float(rt, ctx, self + 0x98u, 0x14u, yaw_in)});
        return;
    }
    const float pitch_in = psp_mul(float_at(rt, at(8u)), std::bit_cast<float>(0xBF860A92u));
    const float side = float_at(rt, at(0u));
    const std::uint32_t local = at(0xD0u);
    const auto offset = [&](std::uint32_t y, std::uint32_t z) {
        put(rt, at(0x108u), z);
        put(rt, at(0x104u), y);
        for (const std::uint32_t one : {0xD0u, 0xE4u, 0xF8u, 0x10Cu}) put_float(rt, at(one), 1.0f);
        for (const std::uint32_t zero : {0xE0u, 0xF0u, 0x100u, 0xD4u, 0xF4u, 0xD8u, 0xE8u, 0xDCu, 0xECu, 0xFCu}) put(rt, at(zero), 0u);
        call_set(rt, ctx, 0x08B4E9D4u, {matrix, local});
    };
    offset(0xBE19999Au, 0x3D75C28Fu);
    call_setf(rt, ctx, 0x08B4F2E4u, {matrix}, {call_virtual_float(rt, ctx, self + 0x148u, 0x14u, pitch_in)});
    call_setf(rt, ctx, 0x08B4F47Cu, {matrix},
              {call_virtual_float(rt, ctx, self + 0x1A0u, 0x14u, psp_mul(side, std::bit_cast<float>(0x3FB2B8C2u)))});
    call_setf(rt, ctx, 0x08B4F3B0u, {matrix},
              {call_virtual_float(rt, ctx, self + 0x174u, 0x14u, psp_mul(side, std::bit_cast<float>(0x3F5F66F3u)))});
    offset(0x3E19999Au, 0xBD75C28Fu);
}


// func_08821C4C(count, args): builds a sorted table from a script list and hands it to a result. For each element of
// the list func_08A98674(args) the element's 64-bit key (slot 0x84 then 0x50) is printed into a 128-byte buffer
// (func_08930A9C) and parsed into half-words (func_08821A98); the table entry (0x18 bytes: the key and that half-word
// vector, copied twice by value through func_08B8A654 as the original does) is assigned in place (func_08B8AD88). The
// table is sorted (func_08B8BF44 with depth 2 x floor(log2 n), then func_08B8B33C), copied (func_08B8AF08) and given to
// func_08A983E8's result by func_08B8A8A8 with a flag: count 1, or args[1]->slot 0x3C. Each temporary vector is
// destroyed (func_08B8A644 / func_08B8A9E0) and freed (func_08A5BC54) where the original does it. Count <= 0 does nothing.
void func_08821C4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x140u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    put(rt, at(0x100u), count);
    put(rt, at(0x104u), args);
    if (signed_of(count) <= 0) return;
    const auto checked = [&](std::uint32_t object, std::uint32_t message) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    // a vector {begin, end, capacity} at `slot`: destroy its range, then free its storage (elements of 2 or 8 bytes)
    const auto drop = [&](std::uint32_t slot, std::uint32_t destroy, std::uint32_t tag, std::uint32_t shift) {
        call_set(rt, ctx, destroy, {word(rt, slot), word(rt, slot + 4u), tag});
        const std::uint32_t begin = word(rt, slot);
        if (begin != 0u)
            call_set(rt, ctx, 0x08A5BC54u, {begin, static_cast<std::uint32_t>(signed_of(word(rt, slot + 8u) - begin) >> shift) << shift, 4u});
    };
    // a copy of the vector at `from` into `to` (elements of `mask`-aligned size) through `copier`
    const auto copy_vector = [&](std::uint32_t to, std::uint32_t from_begin, std::uint32_t from_end, std::uint32_t mask,
                                 std::uint32_t copier, std::uint32_t tag) {
        const std::uint32_t bytes = (from_end - from_begin) & ~mask;
        put(rt, to, 0u);
        put(rt, to + 4u, 0u);
        put(rt, to + 8u, 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        put(rt, to + 8u, block + bytes);
        put(rt, to, block);
        put(rt, to + 4u, block);
        put(rt, to + 4u, call_set(rt, ctx, copier, {from_begin, from_end, block, tag}));
    };
    const std::uint32_t list_ref = at(0x14u);
    call_set(rt, ctx, 0x08A98674u, {list_ref, args});
    const std::uint32_t list = checked(word(rt, list_ref), 0x08C99660u);
    const std::uint32_t n = static_cast<std::uint32_t>(signed_of(word(rt, list + 0x18u) - word(rt, list + 0x14u)) >> 2);
    const std::uint32_t table = at(0x30u);
    const std::uint32_t table_bytes = n * 24u;
    put(rt, table, 0u);
    put(rt, table + 4u, 0u);
    put(rt, table + 8u, 0u);
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {table_bytes, 4u});
    put(rt, table + 8u, block + table_bytes);
    put(rt, at(0x48u), 0u);
    put(rt, at(0x4Cu), 0u);
    put(rt, table, block);
    put(rt, table + 4u, block);
    put(rt, at(0x50u), 0u);
    put(rt, at(0x54u), 0u);
    put(rt, at(0x58u), 0u);
    call_set(rt, ctx, 0x08B8AA60u, {block, n, at(0x48u), at(9u)});
    drop(at(0x50u), 0x08B8A644u, at(8u), 1u);
    put(rt, table + 4u, n * 24u + word(rt, table));
    const std::uint32_t text = at(0x78u);
    for (std::uint32_t i = 0u;; ++i) {
        const std::uint32_t begin = word(rt, table);
        const std::uint32_t entries = static_cast<std::uint32_t>(signed_of(word(rt, table + 4u) - begin) >> 3) * 0xAAAAAAABu;
        if (!(i < entries)) break;
        const std::uint32_t source = checked(word(rt, list_ref), 0x08C99660u);
        call_virtual_at(rt, ctx, source, 0x84u, {at(0x10u), source, i});
        const std::uint32_t element = checked(word(rt, at(0x10u)), 0x08C995ECu);
        const std::uint32_t key_low = call_virtual_at(rt, ctx, element, 0x50u, {element});
        const std::uint32_t key_high = ctx.gpr[3];
        call_set(rt, ctx, kDestroy, {at(0x10u)});
        put8(rt, text, byte_u(rt, 0x08C87F3Cu));
        call_set(rt, ctx, 0x08B36018u, {text + 1u, 0u, 0x7Fu});
        call_set(rt, ctx, 0x08930A9Cu, {key_low, key_high, text, 0x80u});
        const std::uint32_t parsed = at(0x3Cu);
        put(rt, parsed, 0u);
        put(rt, parsed + 4u, 0u);
        put(rt, parsed + 8u, 0u);
        call_set(rt, ctx, 0x08821A98u, {parsed, text});
        const std::uint32_t entry = i * 24u + word(rt, table);
        copy_vector(at(0x24u), word(rt, parsed), word(rt, parsed + 4u), 1u, 0x08B8A654u, at(7u));
        put(rt, at(0x60u), key_low);
        put(rt, at(0x64u), key_high);
        copy_vector(at(0x68u), word(rt, at(0x24u)), word(rt, at(0x28u)), 1u, 0x08B8A654u, at(6u));
        put(rt, entry + 4u, word(rt, at(0x64u)));
        put(rt, entry, word(rt, at(0x60u)));
        call_set(rt, ctx, 0x08B8AD88u, {entry + 8u, at(0x68u)});
        drop(at(0x68u), 0x08B8A644u, at(5u), 1u);
        drop(at(0x24u), 0x08B8A644u, at(4u), 1u);
        drop(parsed, 0x08B8A644u, at(3u), 1u);
    }
    const std::uint32_t first = word(rt, table);
    const std::uint32_t last = word(rt, table + 4u);
    if (last != first) {
        std::uint32_t entries = static_cast<std::uint32_t>(signed_of(last - first) >> 3) * 0xAAAAAAABu;
        std::uint32_t depth = 0u;
        if (entries != 1u) {
            do {
                entries = static_cast<std::uint32_t>(signed_of(entries) >> 1);
                ++depth;
            } while (entries != 1u);
            depth <<= 1;
        }
        call_set(rt, ctx, 0x08B8BF44u, {first, last, depth, last});
        call_set(rt, ctx, 0x08B8B33Cu, {first, last, last});
    }
    std::uint32_t flag = 1u;
    if (word(rt, at(0x100u)) != 1u) {
        const std::uint32_t option = checked(word(rt, word(rt, at(0x104u)) + 4u), 0x08C995ECu);
        flag = call_virtual_at(rt, ctx, option, 0x3Cu, {option}) != 0u ? 1u : 0u;
    }
    const std::uint32_t result = at(0xCu);
    call_set(rt, ctx, 0x08A983E8u, {result, list_ref});
    copy_vector(at(0x18u), word(rt, table), word(rt, table + 4u), 7u, 0x08B8AF08u, at(2u));
    call_set(rt, ctx, 0x08B8A8A8u, {result, at(0x18u), flag});
    drop(at(0x18u), 0x08B8A9E0u, at(1u), 3u);
    call_set(rt, ctx, kDestroy, {result});
    drop(table, 0x08B8A9E0u, at(0u), 3u);
    call_set(rt, ctx, kDestroy, {list_ref});
}


// The original's float-to-unsigned conversion: truncate, or for values from 2^31 on truncate value - 2^31 and set the
// top bit.
inline std::uint32_t float_to_unsigned(psprecomp::AllegrexContext &ctx, float value) {
    const float limit = std::bit_cast<float>(0x4F000000u);
    if (!(limit <= value)) return ctx.fpu_float_to_word(value, 1u);
    return ctx.fpu_float_to_word(value - limit, 1u) | 0x80000000u;
}
// The original's unsigned-to-float conversion (halve with the low bit kept, convert, double).
inline float unsigned_float(std::uint32_t value) {
    if (signed_of(value) >= 0) return static_cast<float>(signed_of(value));
    const float half = static_cast<float>(signed_of((value >> 1) | (value & 1u)));
    return half + half;
}

// func_088F4160(self, spec, layer): spawns one particle of the type 0x08C8A3A8[spec->+4] when its 0x38-byte record
// (self+0x38 x type) has a weight +0x4C of at least 0.01 and spec+0x2C is above 0.001; the type's counter self+0xC+4 x
// type wraps at record+0x74. The particle (64 bytes at self+4) comes from the free list self+0xA (13-bit links, pushed
// on the active list self+8) or from func_088F2D38(self). Its fields come from the random source self+0x198
// (func_08BB9130(0, 1)): a size class 0..7 (scale 0x08C8A388), colour (spec+0x18 blended with self+0x34, x self+0x1A0
// x record colour, packed with 255 caps), size, spin, life, rates, and the plane through spec+8..+0x28.
void func_088F4160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t spec = ctx.gpr[5];
    const std::uint32_t layer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t type = word(rt, 0x08C8A3A8u + word(rt, spec + 4u) * 4u);
    const std::uint32_t record = self + type * 0x38u;
    if (float_at(rt, record + 0x4Cu) < std::bit_cast<float>(0x3C23D70Au)) {
        ctx.gpr[2] = type * 0x38u;
        return;
    }
    const std::uint32_t counter = self + type * 4u + 0xCu;
    const std::uint32_t next = word(rt, counter) + 1u;
    put(rt, counter, next);
    const std::uint32_t wrap = word(rt, record + 0x74u);
    if (wrap < next) put(rt, counter, 0u);
    const float strength = float_at(rt, spec + 0x2Cu);
    if (strength <= std::bit_cast<float>(0x3A83126Fu)) {
        ctx.gpr[2] = wrap;
        return;
    }
    std::uint32_t slot;
    const std::uint32_t free_head = sext16(half_u(rt, self + 0xAu));
    if (signed_of(free_head) >= 0) {
        slot = free_head;
        const std::uint32_t link = word(rt, self + 4u) + (slot << 6);
        const std::uint32_t active = half_u(rt, self + 8u);
        put16(rt, self + 0xAu, static_cast<std::uint32_t>(signed_of(word(rt, link) << 19) >> 19));
        put(rt, link, (word(rt, link) & ~0x1FFFu) | (active & 0x1FFFu));
        put16(rt, self + 8u, slot);
    } else {
        slot = call_set(rt, ctx, 0x088F2D38u, {self});
    }
    const std::uint32_t particle = word(rt, self + 4u) + (slot << 6);
    const std::uint32_t random = self + 0x198u;
    const auto roll = [&] { return call_setf(rt, ctx, 0x08BB9130u, {random}, {0.0f, 1.0f}).f0; };
    std::uint32_t size_class = ctx.fpu_float_to_word(psp_mul(roll(), 8.0f), 1u);
    if (!(signed_of(size_class) < 8)) size_class = 7u;
    put8(rt, particle + 2u, size_class & 0xFFu);
    const float scale = psp_mul(strength, float_at(rt, 0x08C8A388u + size_class * 4u));
    if ((half_u(rt, spec + 2u) & 1u) != 0u) put8(rt, particle + 2u, (size_class & 0xFFu) | 0x20u);
    put8(rt, particle + 2u, ((slot & 3u) << 6) | byte_u(rt, particle + 2u));
    put8(rt, particle + 3u, type);
    const float base = float_at(rt, self + 0x34u);
    const float blend = psp_mul(base + psp_mul(float_at(rt, spec + 0x18u), 1.0f - base), float_at(rt, self + 0x1A0u));
    const auto channel = [&](std::uint32_t offset) {
        return float_to_unsigned(ctx, psp_mul(psp_mul(blend, float_at(rt, record + offset)), 255.0f));
    };
    const std::uint32_t red = channel(0x40u);
    const std::uint32_t green = channel(0x44u);
    const std::uint32_t blue = channel(0x48u);
    put(rt, particle + 4u, (blue < 0x100u ? blue << 16 : 0x00FF0000u) | (green < 0x100u ? green << 8 : 0xFF00u) |
                               (red < 0x100u ? red : 0xFFu));
    put_float(rt, particle + 0xCu, scale);
    put_float(rt, particle + 8u, psp_mul(scale, float_at(rt, record + 0x50u)));
    put_float(rt, particle + 0x18u,
              psp_mul(psp_mul(roll() - std::bit_cast<float>(0x3E4CCCCDu), std::bit_cast<float>(0x3E99999Au)) + 1.0f,
                      float_at(rt, record + 0x58u)));
    put_float(rt, particle + 0x14u, psp_mul(roll(), 360.0f));
    put_float(rt, particle + 0x10u, scale + psp_mul(1.0f - scale, float_at(rt, record + 0x70u)));
    float life = psp_mul(roll(), unsigned_float(word(rt, record + 0x74u)) + std::bit_cast<float>(0x3E4CCCCDu)) + 1.0f;
    if (byte_u(rt, self + 0x197u) != 0u) life = psp_mul(life, std::bit_cast<float>(0x40133333u));
    const std::uint32_t ticks = 255.0f <= life ? ctx.fpu_float_to_word(65280.0f, 1u)
                                               : ctx.fpu_float_to_word(psp_mul(life, 256.0f), 1u);
    put16(rt, particle + 0x1Cu, ticks);
    put8(rt, particle + 0x1Fu, ctx.fpu_float_to_word(psp_mul(roll(), 64.0f), 1u));
    const float wobble = psp_mul(psp_mul(roll() - 0.5f, std::bit_cast<float>(0x3E4CCCCDu)) + 1.0f, 128.0f);
    const std::uint32_t head = (word(rt, particle) & ~0xE000u) | ((layer & 7u) << 13);
    const std::uint32_t wobble_byte = ctx.fpu_float_to_word(wobble, 1u);
    put8(rt, particle + 0x1Eu, wobble_byte);
    const float x = float_at(rt, spec + 8u);
    put_float(rt, particle + 0x20u, x);
    put(rt, particle + 0x24u, word(rt, spec + 0xCu));
    const float z = float_at(rt, spec + 0x10u);
    put_float(rt, particle + 0x28u, z);
    const float nx = float_at(rt, spec + 0x1Cu);
    put_float(rt, particle + 0x30u, nx);
    const float ny = float_at(rt, spec + 0x20u);
    put_float(rt, particle + 0x34u, ny);
    const float nz = float_at(rt, spec + 0x24u);
    put_float(rt, particle + 0x38u, nz);
    const float d = float_at(rt, spec + 0x28u);
    put(rt, particle, head);
    put_float(rt, particle + 0x3Cu, d);
    put_float(rt, particle + 0x2Cu, -(psp_mul(nx, x) + psp_mul(nz, z) + d) / ny);
    ctx.gpr[2] = wobble_byte;
}


// A tween record (0x24 bytes): +4 exponent, +8 start, +0xC (start's second word), +0x10 target, +0x14 duration,
// +0x18 current, +0x1C elapsed, +0x20 flags (bit 0 restart, bit 1 running, bit 2 eased). Restart: start = value, the
// target / current / their second words follow it, bits 0-1 cleared.
inline void tween_restart(psprecomp::Runtime &rt, std::uint32_t tween, std::uint32_t value) {
    put(rt, tween + 8u, value);
    put(rt, tween + 0xCu, 0u);
    put(rt, tween + 0x20u, word(rt, tween + 0x20u) & ~3u);
    put(rt, tween + 0x10u, word(rt, tween + 8u));
    put(rt, tween + 0x14u, word(rt, tween + 0xCu));
    put(rt, tween + 0x18u, word(rt, tween + 8u));
    put(rt, tween + 0x1Cu, word(rt, tween + 0xCu));
}
// One tween step of `step` frames: past its duration it lands on the target (bit 0 moves to bit 1); otherwise current =
// start + (target - start) x func_08B67720(t, exponent) with t = elapsed / duration, eased by
// (func_08B66524((t - 0.5) x pi) + 1) x 0.5 when bit 2 is set; a pending restart (bit 0) holds the clock once.
inline void tween_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t tween, float step) {
    const float elapsed = float_at(rt, tween + 0x1Cu);
    const float duration = float_at(rt, tween + 0x14u);
    if (duration <= elapsed) {
        const std::uint32_t flags = word(rt, tween + 0x20u);
        put(rt, tween + 0x18u, word(rt, tween + 0x10u));
        put(rt, tween + 0x20u, ((flags & ~2u) | ((flags & 1u) << 1)) & ~1u);
        return;
    }
    const std::uint32_t flags = word(rt, tween + 0x20u);
    float t = elapsed / duration;
    if (((flags >> 2) & 1u) != 0u) {
        const float wave = call_setf(rt, ctx, 0x08B66524u, {}, {psp_mul(t - 0.5f, std::bit_cast<float>(0x40490FDBu))}).f0;
        t = psp_mul(wave + 1.0f, 0.5f);
    }
    const float start = float_at(rt, tween + 8u);
    const float shape = call_setf(rt, ctx, 0x08B67720u, {}, {t, float_at(rt, tween + 4u)}).f0;
    const std::uint32_t running = flags | 2u;
    put(rt, tween + 0x20u, running);
    put_float(rt, tween + 0x18u, start + psp_mul(shape, float_at(rt, tween + 0x10u) - start));
    if ((running & 1u) != 0u) put(rt, tween + 0x20u, running & ~1u);
    else put_float(rt, tween + 0x1Cu, step + elapsed);
}

// func_08871A7C(self, clock): one frame of a camera rig. Restarts the rig's first tween ((self+0xC)+4) when the
// controller self+8 reports state 4 (func_0886B444), refreshes its +4 from func_0890AF20 when that tween sits at
// 0x08D75378, restarts the second / third tweens (+0x28 / +0x4C) with func_0887C9D0 / func_0887C9E0 of the target
// func_0886AFA8(self+8)+4 when they are idle, and steps all three by clock->slot 0x1C x func_0886B0F8 x 60 frames. When
// the scene object func_0886F8D4 of func_0886A9A0(self+4, 5) exists and state 4 is off, it reads the object's lens
// values (slots 0xB4 / 0xB8 / 0xBC) into the target (func_0887CA78 .. func_0887C914, six of them by 0x08C8918C).
// Then the target gets the distance (+4 + +0x1C, three lanes), and the angles +0x28 + +0x40 and +0x4C + +0x64.
void func_08871A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t clock = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t scratch = frame.at(0u);
    const std::uint32_t rig_slot = self + 0xCu;
    const std::uint32_t owner = call_set(rt, ctx, 0x0886AFA8u, {word(rt, self + 8u)});
    const std::uint32_t target = owner + 4u;
    const std::uint32_t camera = call_set(rt, ctx, 0x0886B060u, {word(rt, self + 8u), 0u});
    if (call_set(rt, ctx, 0x0886B444u, {word(rt, self + 8u), 4u}) != 0u) tween_restart(rt, word(rt, rig_slot) + 4u, 0u);
    std::uint32_t first = word(rt, rig_slot) + 4u;
    if (!(float_at(rt, first + 0x1Cu) < float_at(rt, first + 0x14u)) &&
        float_at(rt, first + 0x18u) == float_at(rt, 0x08D75378u)) {
        call_set(rt, ctx, 0x0890AF20u, {camera, scratch});
        put(rt, word(rt, rig_slot) + 4u, word(rt, scratch));
    }
    std::uint32_t rig = word(rt, rig_slot);
    if (((word(rt, rig + 0x48u) >> 1) & 1u) == 0u) {
        call_set(rt, ctx, 0x0887C9D0u, {target});
        const float value = ctx.fpr[0];
        tween_restart(rt, rig + 0x28u, std::bit_cast<std::uint32_t>(value));
        rig = word(rt, rig_slot);
    }
    if (((word(rt, rig + 0x6Cu) >> 1) & 1u) == 0u) {
        call_set(rt, ctx, 0x0887C9E0u, {target});
        tween_restart(rt, rig + 0x4Cu, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
    }
    call_virtual_at(rt, ctx, clock, 0x1Cu, {clock});
    const float tick = ctx.fpr[0];
    call_set(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)});
    const float rate = ctx.fpr[0];
    rig = word(rt, rig_slot);
    const float step = psp_mul(psp_mul(tick, rate), 60.0f);
    for (const std::uint32_t offset : {4u, 0x28u, 0x4Cu}) tween_step(rt, ctx, rig + offset, step);
    std::uint32_t scene = call_set(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 5u});
    if (scene != 0u) {
        call_set(rt, ctx, 0x0886F8CCu, {scene});
        call_set(rt, ctx, 0x0886F8E4u, {scene});
        scene = call_set(rt, ctx, 0x0886F8D4u, {scene});
    }
    if (scene != 0u && call_set(rt, ctx, 0x0886B444u, {word(rt, self + 8u), 4u}) == 0u) {
        put(rt, frame.at(0xCu), 0u);
        call_virtual_at(rt, ctx, scene, 0xB4u, {scene, scratch});
        call_set(rt, ctx, 0x0887CA78u, {target, scratch});
        call_virtual_at(rt, ctx, scene, 0xB8u, {scene, 1u, scratch});
        call_set(rt, ctx, 0x0887CAA0u, {target, scratch});
        call_virtual_at(rt, ctx, scene, 0xB8u, {scene, 3u, scratch});
        call_set(rt, ctx, 0x0887CAC8u, {target, scratch});
        call_virtual_at(rt, ctx, scene, 0xB8u, {scene, 2u, scratch});
        call_set(rt, ctx, 0x0887C90Cu, {target, scratch});
        for (std::uint32_t i = 0u; i < 6u; ++i) {
            call_virtual_at(rt, ctx, scene, 0xB8u, {scene, word(rt, 0x08C8918Cu + i * 4u), scratch});
            call_set(rt, ctx, 0x0887C914u, {target, i, scratch});
        }
        call_virtual_at(rt, ctx, scene, 0xBCu, {scene});
        const float lens = ctx.fpr[0];
        if (0.0f <= lens) put_float(rt, word(rt, rig_slot) + 4u, lens);
    }
    rig = word(rt, rig_slot);
    const float distance = float_at(rt, rig + 4u) + float_at(rt, rig + 0x1Cu);
    put_float(rt, scratch, distance);
    put_float(rt, scratch + 8u, distance);
    put_float(rt, scratch + 4u, distance);
    call_set(rt, ctx, 0x0890AEF8u, {camera, scratch});
    call_set(rt, ctx, 0x0887C9F8u, {owner, scratch});
    rig = word(rt, rig_slot);
    call_setf(rt, ctx, 0x0887C998u, {target}, {float_at(rt, rig + 0x28u) + float_at(rt, rig + 0x40u)});
    rig = word(rt, rig_slot);
    call_setf(rt, ctx, 0x0887C9C8u, {target}, {float_at(rt, rig + 0x4Cu) + float_at(rt, rig + 0x64u)});
}


// func_0893D188(self): the four wheels' drive shares of a car (self+8 = car state). With func_089557BC(self) set it
// only copies wheel 0's position (+0x30 / +0x34) to state+0x690 unless that wheel is off (+0x2C). Otherwise it counts
// the wheels on the ground (func_08954670) and those with surface flag +0x2F bit 0, then per wheel: the surface type
// (func_08954618, steady when func_08954688 confirms it, +0xEC), the surface table self->+0xE4 by type (+0x1A8 grip,
// +0x208 frequency, +0x280 squared bumps, +0x268 scaled by the wheel's +0x4C) gives the bump force +0xC4 = grip x
// (cos(x f) + sin(z f) [- 2] ...). From state byte +1 >= 0x7D the slip +0xC0 also follows the wheel's speed and the
// sideways slip for types 3-4, the axle weights self+0xC +0x60 are summed, and the slip share goes to state+0x698.
// Ends storing the counts and wheel 0's position in the state; returns the grounded count.
void func_0893D188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t grounded_slot = frame.at(0u);
    const std::uint32_t state = word(rt, self + 8u);
    const bool driven = !(byte_u(rt, state + 1u) < 0x7Du);
    put(rt, state + 0x698u, 0u);
    if (call_set(rt, ctx, 0x089557BCu, {self}) != 0u) {
        const std::uint32_t wheel = call_set(rt, ctx, 0x0893C780u, {self, 0u});
        const std::uint32_t off = byte_u(rt, wheel + 0x2Cu);
        if (off != 0u) {
            ctx.gpr[2] = off;
            return;
        }
        put(rt, state + 0x690u, word(rt, wheel + 0x30u));
        put(rt, state + 0x694u, word(rt, wheel + 0x34u));
        ctx.gpr[2] = word(rt, wheel + 0x30u);
        return;
    }
    put(rt, grounded_slot, 0u);
    std::uint32_t flagged = 0u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t tyre = call_set(rt, ctx, 0x08955300u, {self, i});
        const std::uint32_t wheel = call_set(rt, ctx, 0x0893C780u, {self, i});
        put8(rt, tyre + 0xEDu, 0u);
        if (byte_u(rt, wheel + 0x2Cu) != 0u) continue;
        flagged += byte_u(rt, wheel + 0x2Fu) & 1u;
        if (call_set(rt, ctx, 0x08954670u, {wheel}) != 0u) {
            put(rt, grounded_slot, word(rt, grounded_slot) + 1u);
            put8(rt, tyre + 0xEDu, 1u);
        }
    }
    float weight_sum = 0.0f;
    float slip_sum = 0.0f;
    const std::uint32_t surfaces = word(rt, self) + 0xE4u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t tyre = call_set(rt, ctx, 0x08955300u, {self, i});
        const std::uint32_t wheel = call_set(rt, ctx, 0x0893C780u, {self, i});
        const float previous_slip = float_at(rt, tyre + 0xC4u);
        if (byte_u(rt, wheel + 0x2Cu) != 0u) {
            put(rt, tyre + 0xC4u, 0u);
            put8(rt, tyre + 0xECu, 0u);
        } else {
            const std::uint32_t old_type = byte_u(rt, tyre + 0xE9u);
            const float height = float_at(rt, wheel + 0xCu);
            const std::uint32_t type = call_set(rt, ctx, 0x08954618u, {wheel});
            put8(rt, tyre + 0xE9u, type);
            const bool steady = old_type == type && call_set(rt, ctx, 0x08954688u, {wheel, old_type}) != 0u;
            put8(rt, tyre + 0xECu, steady ? 1u : 0u);
            put_float(rt, tyre + 0xBCu, height);
            put8(rt, tyre + 0xEAu, byte_u(rt, wheel + 0x2Eu));
            const std::uint32_t current = byte_u(rt, tyre + 0xE9u);
            const std::uint32_t row = surfaces + current * 4u;
            put8(rt, tyre + 0xEBu, byte_u(rt, wheel + 0x2Fu));
            const float grip = float_at(rt, row + 0x1A8u);
            if (grip == 0.0f) {
                put_float(rt, tyre + 0xC4u, 0.0f);
            } else {
                const float frequency = float_at(rt, row + 0x208u);
                const float z = float_at(rt, wheel + 8u);
                const std::uint32_t squared = sext8(byte_u(rt, surfaces + current + 0x280u));
                const float a = call_setf(rt, ctx, 0x08B49CC8u, {}, {psp_mul(float_at(rt, wheel), frequency)}).f0;
                float bump = a + call_setf(rt, ctx, 0x08B499B8u, {}, {psp_mul(z, frequency)}).f0;
                if (squared == 1u) {
                    const bool negative = bump < 0.0f;
                    bump = psp_mul(bump, bump);
                    if (negative) bump = -bump;
                }
                bump = bump - 2.0f;
                if (byte_u(rt, tyre + 0xEDu) != 0u && word(rt, grounded_slot) != 4u) bump = psp_mul(bump, 0.5f);
                if (sext8(byte_u(rt, surfaces + byte_u(rt, tyre + 0xE9u) + 0x268u)) != 0u)
                    bump = psp_mul(bump, float_at(rt, tyre + 0x4Cu));
                put_float(rt, tyre + 0xC4u, psp_mul(bump, grip));
            }
        }
        if (!driven) continue;
        const float axle = float_at(rt, word(rt, self + 0xCu) + ((i >> 1) << 2) + 0x60u);
        weight_sum = weight_sum + axle;
        float limit = 0.0f;
        float slip = 0.0f;
        const std::uint32_t type = (byte_u(rt, tyre + 0xE9u) - 3u) & 0xFFu;
        if (!(float_at(rt, tyre + 0x54u) <= 0.0f) && type < 2u) {
            const float speed = float_at(rt, tyre + 0x40u);
            const float threshold = std::bit_cast<float>(0x4031C71Du);
            if (threshold < speed) {
                limit = psp_mul(speed - threshold, 0.5f) / std::bit_cast<float>(0x40B1C71Du);
                if (0.5f < limit) limit = 0.5f;
                const float current_slip = float_at(rt, tyre + 0xC0u);
                call_set(rt, ctx, 0x0895549Cu, {self});
                const float rate = ctx.fpr[0];
                slip = current_slip - psp_mul(limit, rate);
                put_float(rt, tyre + 0xC0u, slip);
                if (slip < -axle) put_float(rt, tyre + 0xC0u, -axle);
            } else {
                float sideways = float_at(rt, tyre + 0x34u);
                if (sideways < 0.0f) sideways = -sideways;
                float gain = psp_mul(sideways, std::bit_cast<float>(0x3DDD2F1Bu));
                if (std::bit_cast<float>(0x3E99999Au) < gain) gain = std::bit_cast<float>(0x3E99999Au);
                const float current_slip = float_at(rt, tyre + 0xC0u);
                call_set(rt, ctx, 0x0895549Cu, {self});
                const float rate = ctx.fpr[0];
                slip = current_slip + psp_mul(gain, rate);
                put_float(rt, tyre + 0xC0u, slip);
                if (0.0f < slip) put_float(rt, tyre + 0xC0u, 0.0f);
            }
            const float bump = float_at(rt, tyre + 0xC4u);
            slip = float_at(rt, tyre + 0xC0u);
            if (slip < bump) slip_sum = slip_sum + (bump - slip);
        } else {
            if (previous_slip < 0.0f) put_float(rt, tyre + 0xC0u, previous_slip);
            slip = float_at(rt, tyre + 0xC0u);
            if (!(slip < 0.0f)) continue;
            call_set(rt, ctx, 0x0895549Cu, {self});
                const float rate = ctx.fpr[0];
            slip = slip + psp_mul(rate, 0.5f);
            put_float(rt, tyre + 0xC0u, slip);
            if (0.0f < slip) {
                slip = 0.0f;
                put_float(rt, tyre + 0xC0u, 0.0f);
            }
        }
        if (slip < 0.0f && slip < float_at(rt, tyre + 0xC4u)) put_float(rt, tyre + 0xC4u, slip);
    }
    if (driven && 0.0f < slip_sum) {
        const float share = slip_sum / weight_sum;
        float stored;
        if (1.0f < share) stored = 1.0f;
        else if (share < 0.0f) stored = 0.0f;
        else stored = psp_mul(share, share);
        put_float(rt, state + 0x698u, stored);
    }
    const std::uint32_t wheel = call_set(rt, ctx, 0x0893C780u, {self, 0u});
    const std::uint32_t flags = byte_u(rt, state + 0xCu) & ~0x10u;
    const std::uint32_t x = word(rt, wheel + 0x30u);
    const std::uint32_t y = word(rt, wheel + 0x34u);
    const std::uint32_t grounded = word(rt, grounded_slot);
    put8(rt, state + 0xCu, flags);
    put8(rt, state + 0x69Du, grounded);
    put(rt, state + 0x690u, x);
    put(rt, state + 0x694u, y);
    put8(rt, state + 0x69Eu, flagged);
    ctx.gpr[2] = grounded;
}


// A 0xED-byte record copy (the original copies it in half-word or word chunks by alignment; the bytes are the same).
inline void copy_record(psprecomp::Runtime &rt, std::uint32_t to, std::uint32_t from) {
    for (std::uint32_t i = 0u; i < 0xEDu; ++i) put8(rt, to + i, byte_u(rt, from + i));
}

// func_08B20DB0(result, source): reads a 0xED-byte record from source+4 (created by func_08B20C48(source+0xC) when
// missing; its slot 8), runs it through func_08B22E94(..., source+8) into result, and opens the flag set
// func_08B21C64(result). By the record's kind (half-word at +2): kinds 2-7 and 16-71 not divisible by 7 after 16 copy
// the flags of func_08B20A78's record listed at 0x08CE0A34 (key, flag pairs to 0xFFFF; func_08B21410 / func_08B21758)
// and set 0xA when 0x17 and 0x18 are on, 0xC when 8 and 0x1A-0x1C are on; kinds 9 (8 + 1) and 23-29 ((kind - 16) / 7 ==
// 1) set 6 / 5 from flags 0x10 / 0x15 of func_08B3A480's record. Returns result.
void func_08B20DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x6C0u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    std::uint32_t reader = word(rt, source + 4u);
    if (reader == 0u) {
        reader = call_set(rt, ctx, 0x08B20C48u, {word(rt, source + 0xCu)});
        put(rt, source + 4u, reader);
    }
    call_virtual_at(rt, ctx, reader, 8u, {at(0x2D0u), reader});
    const std::uint32_t base = at(0xEEu);
    copy_record(rt, base, at(0x2D0u));
    call_set(rt, ctx, 0x08B22E94u, {at(0x3C0u), base, word(rt, source + 8u)});
    copy_record(rt, at(0u), at(0x3C0u));
    copy_record(rt, result, at(0u));
    const std::uint32_t kind = half_u(rt, at(0xF0u));
    const std::uint32_t flags = call_set(rt, ctx, 0x08B21C64u, {result});
    const bool listed = (kind - 1u < 7u) ? kind - 1u != 0u : (kind - 16u < 0x38u && (kind - 16u) % 7u != 0u);
    const auto flag_set = [&](std::uint32_t make, std::uint32_t record_at) {
        call_set(rt, ctx, make, {record_at, base});
        copy_record(rt, at(0x1DCu), record_at);
        return call_set(rt, ctx, 0x08B21C64u, {at(0x1DCu)});
    };
    const auto has = [&](std::uint32_t set, std::uint32_t flag) { return call_set(rt, ctx, 0x08B21410u, {set, flag}) != 0u; };
    if (listed) {
        const std::uint32_t other = flag_set(0x08B20A78u, at(0x4B0u));
        for (std::uint32_t entry = 0x08CE0A34u; half_u(rt, entry) != 0xFFFFu; entry += 4u) {
            const std::uint32_t key = half_u(rt, entry);
            call_set(rt, ctx, 0x08B21758u, {flags, key, call_set(rt, ctx, 0x08B21410u, {other, half_u(rt, entry + 2u)})});
        }
        if (has(other, 0x17u) && has(other, 0x18u)) call_set(rt, ctx, 0x08B21758u, {flags, 0xAu, 1u});
        if (has(other, 8u) && has(other, 0x1Au) && has(other, 0x1Bu) && has(other, 0x1Cu))
            call_set(rt, ctx, 0x08B21758u, {flags, 0xCu, 1u});
    }
    std::uint32_t level = 0u;
    if (kind - 8u < 8u) level = kind - 8u;
    else if (kind - 16u < 0x38u) level = (kind - 16u) / 7u;
    if (level != 0u) {
        const std::uint32_t other = flag_set(0x08B3A480u, at(0x5A0u));
        if (level == 1u) {
            if (has(other, 0x10u)) call_set(rt, ctx, 0x08B21758u, {flags, 6u, 1u});
            if (has(other, 0x15u)) call_set(rt, ctx, 0x08B21758u, {flags, 5u, 1u});
        }
    }
    ctx.gpr[2] = result;
}

} // namespace hot_batch_120
} // namespace gt::game

#define HOT120(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_120::name, gt::ReturnKind::kind)
HOT120(0x0892381Cu, func_0892381C, Word);
HOT120(0x08821C4Cu, func_08821C4C, Word);
HOT120(0x088F4160u, func_088F4160, Word);
HOT120(0x08871A7Cu, func_08871A7C, Word);
HOT120(0x0893D188u, func_0893D188, Word);
HOT120(0x08B20DB0u, func_08B20DB0, Word);
