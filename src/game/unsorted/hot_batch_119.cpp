// Hot batch 119: functions of 134-400 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"
#include "gt/vfpu.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_119 {
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


// func_089AD0E0(self, car): advances the history ring self+0xB0 (64-byte entries, count self+0xB4) by one entry whose
// +0x34 is the previous one's scaled by func_08967530(car); a non-positive result ends there. By func_08965BD4(car):
// kind 1 offsets the car's position (+0x70) by the previous entry's position scaled lane-wise ((x, y) x (z, w) + (x, y)),
// kind 2 adds the previous (x, y) to the car's position, kind 3 takes the position as is and an identity orientation;
// kinds 1 and 2 keep the previous orientation. The two position lanes go to func_089C3DA8(entry+0x10, ...). Then
// func_08B4EBE0(entry+0x10, func_0896B0A8(car)+0x14) when that is set, func_089ACEE8(self, car) when +0xFC is set and
// +0xF8 / +0xF9 are not, the counter self+0x13C (self->slot 0x124 first when it is 0) when the car's flags +0xE0 bit 13
// or +0xE4 bit 7 are set, and func_089A8D0C(self, car). The vector work is VFPU code, one operation a line.
void func_089AD0E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    using B = v::Binary;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t car = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t count = word(rt, self + 0xB4u) + 1u;
    const std::uint32_t entry = word(rt, self + 0xB0u) + (count << 6);
    put(rt, self + 0xB4u, count);
    const std::uint32_t previous = entry - 0x40u;
    const float base = float_at(rt, previous + 0x34u);
    call_set(rt, ctx, 0x08967530u, {car});
    const float weight = base * ctx.fpr[0];
    put_float(rt, entry + 0x34u, weight);
    if (!(0.0f < weight)) return;
    const std::uint32_t kind = call_set(rt, ctx, 0x08965BD4u, {car});
    const std::uint32_t lanes = frame.at(0u);
    const auto keep_orientation = [&] {
        v::load_quad(rt, ctx, 0x00u, previous + 0x10u);   // lv.q C000
        v::load_quad(rt, ctx, 0x01u, previous + 0x20u);   // lv.q C010
        v::load_scalar(rt, ctx, 0x02u, previous + 0x30u); // lv.s S020
        v::store_quad(rt, ctx, 0x00u, entry + 0x10u);     // sv.q C000
        v::store_quad(rt, ctx, 0x01u, entry + 0x20u);     // sv.q C010
        v::store_scalar(rt, ctx, 0x02u, entry + 0x30u);   // sv.s S020
    };
    if (kind == 2u || kind == 3u || kind == 1u) {
        float first = 0.0f;
        float second = 0.0f;
        if (kind == 1u) {
            v::load_quad(rt, ctx, 0x01u, car + 0x70u);    // lv.q C010
            v::load_quad(rt, ctx, 0x02u, previous);       // lv.q C020
            v::prefix(ctx, 1u, 0xEEu);                    // vpfxt Z, W, Z, W
            v::binary(ctx, B::Mul, 0x01u, 0x01u, 0x02u, 4u);  // vmul.q C010, C010, C020
            v::prefix(ctx, 1u, 0xC004u);                  // vpfxt X, Y, 0, 0
            v::binary(ctx, B::Add, 0x00u, 0x01u, 0x02u, 4u);  // vadd.q C000, C010, C020
            v::store_quad(rt, ctx, 0x00u, entry);         // sv.q C000
            v::store_scalar(rt, ctx, 0x01u, lanes);       // sv.s S010
            v::store_scalar(rt, ctx, 0x21u, lanes + 4u);  // sv.s S011
            keep_orientation();
            first = float_at(rt, lanes);
            second = float_at(rt, lanes + 4u);
        } else if (kind == 2u) {
            v::load_quad(rt, ctx, 0x01u, car + 0x70u);    // lv.q C010
            v::load_quad(rt, ctx, 0x02u, previous);       // lv.q C020
            v::prefix(ctx, 1u, 0xC004u);                  // vpfxt X, Y, 0, 0
            v::binary(ctx, B::Add, 0x00u, 0x01u, 0x02u, 4u);  // vadd.q C000, C010, C020
            v::store_quad(rt, ctx, 0x00u, entry);         // sv.q C000
            v::store_scalar(rt, ctx, 0x01u, lanes + 4u);  // sv.s S010
            v::store_scalar(rt, ctx, 0x21u, lanes);       // sv.s S011
            keep_orientation();
            first = float_at(rt, lanes + 4u);
            second = float_at(rt, lanes);
        } else {
            v::load_quad(rt, ctx, 0x00u, car + 0x70u);    // lv.q C000
            v::store_quad(rt, ctx, 0x00u, entry);         // sv.q C000
            v::store_scalar(rt, ctx, 0x00u, lanes);       // sv.s S000
            v::store_scalar(rt, ctx, 0x20u, lanes + 4u);  // sv.s S001
            v::unit(ctx, 0x00u, 4u);                      // vidt.q C000
            v::store_quad(rt, ctx, 0x00u, entry + 0x10u); // sv.q C000
            v::store_quad(rt, ctx, 0x00u, entry + 0x20u); // sv.q C000
            v::store_scalar(rt, ctx, 0x00u, entry + 0x30u);  // sv.s S000
            first = float_at(rt, lanes);
            second = float_at(rt, lanes + 4u);
        }
        call_setf(rt, ctx, 0x089C3DA8u, {entry + 0x10u}, {first, second});
    }
    const std::uint32_t shape = call_set(rt, ctx, 0x0896B0A8u, {car});
    if (shape != 0u) call_set(rt, ctx, 0x08B4EBE0u, {entry + 0x10u, shape + 0x14u});
    if (byte_u(rt, self + 0xFCu) != 0u && byte_u(rt, self + 0xF8u) == 0u && byte_u(rt, self + 0xF9u) == 0u)
        call_set(rt, ctx, 0x089ACEE8u, {self, car});
    if ((word(rt, car + 0xE0u) & 0x2000u) != 0u || (word(rt, car + 0xE4u) & 0x80u) != 0u) {
        if (word(rt, self + 0x13Cu) == 0u) call_virtual_at(rt, ctx, self, 0x124u, {self});
        put(rt, self + 0x13Cu, word(rt, self + 0x13Cu) + 1u);
    }
    call_set(rt, ctx, 0x089A8D0Cu, {self, car});
}


// func_0892331C(self, matrix): the body-tilt matrix of a car view. The owner (self+8)->+0x244 gives its velocity
// (slot 0x24 into a 4-float vector) and speed (slot 0x14); func_08926488(self+0x48, {0x08CF3668 x/y/z, 0x08CDDD38
// +0xC..+0x14}, 0) and the velocity turned by self+0xC (func_08B4A8D4) and scaled by 0x08CDDD68 (func_08B4A6D4) give
// two vectors whose lanes are compressed by func_08B67418(|v| / 9.81 + 1) and func_08B67418(|v| x 0.5 + 1), sign
// kept. Those drive the filtered angles self+0x1C / +0x48 / +0x74 / +0xA0 / +0xCC (func_0892690C), faded in below
// speed 5 and boosted up to x4 above 100; the matrix gets the two roll offsets, the yaw (func_08B4F47C) and pitch
// (func_08B4F2E4) and the offset back. Ends storing the slot-0x24 vector at self+0xC.
void func_0892331C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t matrix = ctx.gpr[5];
    GuestFrame frame(ctx, 0x130u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto copy4 = [&](std::uint32_t to, std::uint32_t from) {
        for (std::uint32_t i = 0u; i < 16u; i += 4u) put(rt, at(to + i), word(rt, at(from + i)));
    };
    const float gravity = std::bit_cast<float>(0x411CF5C3u);
    const std::uint32_t owner = word(rt, word(rt, self + 8u) + 0x244u);
    const std::uint32_t turn = self + 0xCu;
    put(rt, at(0xD0u), self + 0x48u);
    call_virtual_at(rt, ctx, owner, 0x24u, {at(0x30u), owner});
    call_virtual_at(rt, ctx, owner, 0x14u, {owner});
    const float speed = ctx.fpr[0];
    const std::uint32_t tuning = 0x08CF3668u;
    const std::uint32_t table = 0x08CDDD38u;
    put(rt, at(0x7Cu), word(rt, table + 0xCu));
    put(rt, at(0x80u), word(rt, table + 0x10u));
    put(rt, at(0x84u), word(rt, table + 0x14u));
    put(rt, at(0x78u), word(rt, tuning + 8u));
    const float lift = float_at(rt, tuning + 0x18u);
    const float pitch_gain = float_at(rt, tuning + 0x28u);
    put(rt, at(0x70u), word(rt, tuning));
    put(rt, at(0x74u), word(rt, tuning + 4u));
    call_set(rt, ctx, 0x08926488u, {word(rt, at(0xD0u)), at(0x70u), 0u});
    copy4(0x10u, 0x30u);
    call_set(rt, ctx, 0x08B4A8D4u, {at(0x10u), at(0x10u), turn});
    copy4(0x20u, 0x10u);
    call_setf(rt, ctx, 0x08B4A6D4u, {at(0x20u)}, {float_at(rt, 0x08CDDD68u)});
    copy4(0x50u, 0x20u);
    for (std::uint32_t i = 0u; i < 3u; ++i) {
        const float a = float_at(rt, at(0x30u + i * 4u));
        const float ratio = a / gravity;
        put_float(rt, at(0x40u + i * 4u), ratio);
        call_setf(rt, ctx, 0x08B67418u, {}, {std::fabs(ratio) + 1.0f});
        put_float(rt, at(0x40u + i * 4u), a < 0.0f ? -ctx.fpr[0] : ctx.fpr[0]);
        const float b = float_at(rt, at(0x50u + i * 4u));
        call_setf(rt, ctx, 0x08B67418u, {}, {psp_mul(std::fabs(b), 0.5f) + 1.0f});
        put_float(rt, at(0x60u + i * 4u), b < 0.0f ? -ctx.fpr[0] : ctx.fpr[0]);
    }
    float slope = float_at(rt, at(0x48u));
    slope = psp_mul(slope, std::bit_cast<float>(0.0f <= slope ? 0x3D0EFA35u : 0x3D567750u));
    const float pitch_scale = psp_mul(pitch_gain, std::bit_cast<float>(0x3C8EFA35u));
    const float forward = psp_mul(float_at(rt, at(0x40u)), pitch_scale);
    float pitch = call_setf(rt, ctx, 0x0892690Cu, {self + 0x1Cu}, {slope}).f0;
    float yaw = call_setf(rt, ctx, 0x0892690Cu, {word(rt, at(0xD0u))}, {forward}).f0;
    const float roll_input = psp_mul(float_at(rt, at(0x68u)), std::bit_cast<float>(0x3C0EFA35u));
    float side = psp_mul(float_at(rt, at(0x60u)), std::bit_cast<float>(0xBA83126Fu));
    float lean = psp_mul(float_at(rt, at(0x64u)), std::bit_cast<float>(0x3A83126Fu));
    if (100.0f < speed) {
        float boost = speed / 100.0f;
        if (boost < 0.0f) boost = 0.0f;
        else if (4.0f < boost) boost = 4.0f;
        lean = psp_mul(lean, boost);
    }
    pitch = pitch + call_setf(rt, ctx, 0x0892690Cu, {self + 0x74u}, {roll_input}).f0;
    if (speed < 5.0f) {
        float fade = speed / 5.0f;
        fade = psp_mul(fade, fade);
        lean = psp_mul(lean, fade);
        yaw = psp_mul(yaw, fade);
        pitch = psp_mul(pitch, fade);
        side = psp_mul(side, fade);
    }
    call_virtual_at(rt, ctx, owner, 0x24u, {at(0u), owner});
    put(rt, turn + 4u, word(rt, at(4u)));
    put(rt, turn + 8u, word(rt, at(8u)));
    put(rt, turn + 0xCu, word(rt, at(0xCu)));
    put(rt, turn, word(rt, at(0u)));
    const float side_angle = call_setf(rt, ctx, 0x0892690Cu, {self + 0xA0u}, {side}).f0;
    const float lean_angle = call_setf(rt, ctx, 0x0892690Cu, {self + 0xCCu}, {lean}).f0;
    const std::uint32_t local = at(0x90u);
    const auto identity_with = [&](std::uint32_t c0, std::uint32_t c4, std::uint32_t c8) {
        for (const std::uint32_t one : {0x90u, 0xA4u, 0xB8u, 0xCCu}) put_float(rt, at(one), 1.0f);
        for (const std::uint32_t zero : {0xA0u, 0xB0u, 0x94u, 0xB4u, 0x98u, 0xA8u, 0x9Cu, 0xACu, 0xBCu}) put(rt, at(zero), 0u);
        put(rt, at(0xC0u), c0);
        put(rt, at(0xC4u), c4);
        put(rt, at(0xC8u), c8);
        call_set(rt, ctx, 0x08B4E9D4u, {matrix, local});
    };
    identity_with(std::bit_cast<std::uint32_t>(side_angle), std::bit_cast<std::uint32_t>(lean_angle), 0u);
    identity_with(0u, std::bit_cast<std::uint32_t>(-lift), 0x3D75C28Fu);
    call_setf(rt, ctx, 0x08B4F47Cu, {matrix}, {yaw});
    call_setf(rt, ctx, 0x08B4F2E4u, {matrix}, {pitch});
    identity_with(0u, std::bit_cast<std::uint32_t>(lift), 0xBD75C28Fu);
}


// A static object built once behind its guard (func_08B6441C acquires, func_08B64494 releases).
template <typename Init>
inline void once(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, Init init) {
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    init();
    call_set(rt, ctx, 0x08B64494u, {guard});
}

// func_088E29DC(self, owner): constructor of a 0x784-byte session object owned by `owner` (self+0). Clears its state
// fields and counters (in the original's order), builds its members in place (func_088D65DC, func_08B28DCC,
// func_08AD04AC, func_08B8ED84, func_08B28D34, func_08B1A5B4) and allocates its parts: objects of 0x314 / 0x38 / 8 / 8
// / 4 / 8 / 0x15C / 0x80 bytes with their constructors, an array of four 0x2F8-byte entries (func_088E90F0 each, count
// cookie 4), zeroed buffers of 0x120 / 0x20 / 0x340 bytes, three 8-byte half-word quads and two slot tables whose
// entries start at -1. +0x614 points at the static object 0x08D75FF0 (vtable 0x08CC3748, built once). Last it opens
// the channel func_08B19F10(self+0x718, {0, 0x3000}, func_08B2E134(func_08B2E314(2, 0))) and registers
// func_088E3FB8() with the owner (func_088E537C).
void func_088E29DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const auto zero = [&](std::initializer_list<std::uint32_t> offsets) {
        for (const std::uint32_t offset : offsets) put(rt, self + offset, 0u);
    };
    const auto zero8 = [&](std::uint32_t from, std::uint32_t to) {
        for (std::uint32_t offset = from; offset <= to; ++offset) put8(rt, self + offset, 0u);
    };
    const auto made = [&](std::uint32_t bytes, std::uint32_t constructor) {
        const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {bytes});
        call_set(rt, ctx, constructor, {object});
        return object;
    };
    const auto new_array = [&](std::uint32_t bytes) { return call_set(rt, ctx, 0x08B6461Cu, {bytes}); };  // operator new[]
    put(rt, self, owner);
    call_set(rt, ctx, 0x088D65DCu, {self + 4u});
    call_set(rt, ctx, 0x088D65DCu, {self + 0xCu});
    put(rt, self + 0x30u, 3u);
    zero({0x80u, 0x88u, 0x84u, 0x8Cu});
    put(rt, self + 0x34u, 0xFFFFFFFFu);
    zero({0x90u, 0x94u, 0xA8u, 0xACu});
    put(rt, self + 0xA0u, 0x1Eu);
    zero({0xA4u, 0xB8u, 0xBCu});
    put8(rt, self + 0x25u, 1u);
    put8(rt, self + 0x27u, 1u);
    zero({0x14u, 0x18u});
    zero8(0x1Cu, 0x24u);
    put8(rt, self + 0x26u, 0u);
    put8(rt, self + 0x28u, 0u);
    zero({0x38u, 0x3Cu, 0x40u});
    put8(rt, self + 0x44u, 0u);
    put8(rt, self + 0x45u, 0u);
    zero({0x48u, 0x4Cu, 0x98u, 0xB0u, 0xC0u, 0xC4u});
    call_set(rt, ctx, 0x08B28DCCu, {self + 0xC8u});
    call_set(rt, ctx, 0x08B28DCCu, {self + 0xF0u});
    zero({0x118u, 0x11Cu});
    for (std::uint32_t offset = 0x124u; offset <= 0x154u; offset += 4u) put(rt, self + offset, 0u);
    call_set(rt, ctx, 0x08AD04ACu, {self + 0x158u});
    put(rt, self + 0x5E0u, 0x08CC4160u);
    put8(rt, self + 0x58Cu, 1u);
    put(rt, self + 0x5D8u, 0x08CC4140u);
    for (std::uint32_t offset = 0x590u; offset <= 0x5B0u; offset += 4u) put(rt, self + offset, 0u);
    zero8(0x5B4u, 0x5C1u);
    zero({0x5C4u, 0x5C8u, 0x5CCu, 0x5D0u, 0x5D4u, 0x5DCu, 0x5E4u});
    call_set(rt, ctx, 0x08B28DCCu, {self + 0x5E8u});
    const std::uint32_t link = made(0x314u, 0x088D29B0u);
    put(rt, self + 0x610u, link);
    constexpr std::uint32_t kShared = 0x08D75FF0u;
    once(rt, ctx, 0x08D75FE8u, [&] { put(rt, kShared, 0x08CC3748u); });
    put(rt, self + 0x614u, kShared);
    put(rt, self + 0x618u, word(rt, self + 0x610u));
    put(rt, self + 0x648u, 0x1770u);
    zero({0x61Cu, 0x620u});
    put8(rt, self + 0x624u, 0u);
    zero({0x628u, 0x62Cu, 0x630u, 0x634u, 0x63Cu, 0x640u, 0x644u, 0x64Cu, 0x650u, 0x654u, 0x65Cu, 0x664u, 0x660u});
    call_set(rt, ctx, 0x08B8ED84u, {self + 0x668u});
    call_set(rt, ctx, 0x08B8ED84u, {self + 0x678u});
    call_set(rt, ctx, 0x08B8ED84u, {self + 0x688u});
    zero({0x6A0u, 0x69Cu, 0x698u});
    call_set(rt, ctx, 0x08B8ED84u, {self + 0x6A4u});
    put(rt, self + 0x6C0u, 0xFFFFFFFFu);
    put(rt, self + 0x6C4u, 0xFFFFFFFFu);
    zero({0x6B4u, 0x6B8u, 0x6C8u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x6CCu});
    put8(rt, self + 0x6F8u, 1u);
    zero({0x704u, 0x70Cu, 0x714u, 0x700u, 0x708u, 0x710u});
    call_set(rt, ctx, 0x08B1A5B4u, {self + 0x718u});
    zero({0x77Cu, 0x780u});
    put(rt, self + 0x14u, made(0x38u, 0x088FEA94u));
    put(rt, self + 0x18u, made(8u, 0x088FDF8Cu));
    const std::uint32_t array = new_array(0xBE8u);
    const std::uint32_t entries = array + 8u;
    put(rt, array + 4u, 4u);
    for (std::uint32_t k = 0u; k < 4u; ++k) call_set(rt, ctx, 0x088E90F0u, {entries + k * 0x2F8u});
    put(rt, self + 0xC0u, entries);
    put(rt, self + 0x12Cu, new_array(0x10u));
    const std::uint32_t scratch = call_set(rt, ctx, 0x08B62344u, {0x120u});
    put(rt, self + 0x130u, scratch);
    call_set(rt, ctx, 0x08B36018u, {scratch, 0u, 0x120u});
    for (const std::uint32_t offset : {0x138u, 0x140u, 0x13Cu}) {
        const std::uint32_t quad = new_array(8u);
        put(rt, self + offset, quad);
        for (const std::uint32_t half : {6u, 0u, 2u, 4u}) put16(rt, quad + half, 0u);
    }
    const std::uint32_t flags = new_array(0x20u);
    put(rt, self + 0x598u, flags);
    call_set(rt, ctx, 0x08B36018u, {flags, 0u, 0x20u});
    put(rt, self + 0x154u, made(8u, 0x088F5670u));
    const std::uint32_t watcher = call_set(rt, ctx, 0x08B62344u, {4u});
    call_set(rt, ctx, 0x088D2BC0u, {watcher, self});
    put(rt, self + 0x61Cu, watcher);
    const std::uint32_t reader = call_set(rt, ctx, 0x08B62344u, {8u});
    call_set(rt, ctx, 0x088D2A68u, {reader, self, word(rt, self + 0x18u)});
    put(rt, self + 0x620u, reader);
    put(rt, self + 0x62Cu, made(0x15Cu, 0x088CEE6Cu));
    put(rt, self + 0x634u, made(0x80u, 0x088E7730u));
    const std::uint32_t big = new_array(0x340u);
    put(rt, self + 0x644u, big);
    call_set(rt, ctx, 0x08B36018u, {big, 0u, 0x340u});
    put(rt, self + 0x77Cu, new_array(0xE0u));
    put(rt, self + 0x780u, new_array(0x20u));
    for (std::uint32_t k = 0u; k < 4u; ++k) {
        put(rt, word(rt, self + 0x77Cu) + k * 0x38u + 0x20u, 0xFFFFFFFFu);
        put(rt, word(rt, self + 0x780u) + k * 8u, 0xFFFFFFFFu);
    }
    const std::uint32_t mode = call_set(rt, ctx, 0x08B2E314u, {2u, 0u});
    const std::uint32_t channel = call_set(rt, ctx, 0x08B2E134u, {mode});
    put(rt, frame.at(4u), 0x3000u);
    put(rt, frame.at(0u), 0u);
    call_set(rt, ctx, 0x08B19F10u, {self + 0x718u, frame.at(0u), channel});
    const std::uint32_t session = call_set(rt, ctx, 0x088E3FB8u, {});
    put(rt, self + 0x63Cu, session);
    call_set(rt, ctx, 0x088E537Cu, {session, owner});
}


// func_08B907F8(self): constructor of a 0x46A8-byte race-world object: the base part (func_08AD023C), its vtables
// 0x08CBF5E8 / 0x08CBF654 and arena constants (+0x68..+0x8C), then each member built in place in the original's order
// (two 0x1C38-byte car slots at +0x608, the lists at +0x4490..+0x4508, the vtabled sub-objects at +0x4524..+0x45D4)
// with the scalar fields between them cleared or set (1.0 scales at +0x3FAC..+0x3FC4, 52.0 at +0x4624, 1.7778 at
// +0x462C). The sub-objects at +0x4538 / +0x458C / +0x45A4 / +0x45B4 / +0x45C4 and the ones returned by
// func_088CBFD4 / func_088CA548 / func_088C7F64 / func_08916474 are registered with the scheduler at +0x4584
// (func_08889D4C). Ends clearing +0x4120 (0x140 bytes) and four slot tables, func_0885DD70(self), and sizing the two
// vectors at +0x4270 / +0x427C to 4 (func_08B8E998).
void func_08B907F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const auto set = [&](std::uint32_t offset, std::uint32_t value) { put(rt, self + offset, value); };
    const auto zero = [&](std::initializer_list<std::uint32_t> offsets) { for (const std::uint32_t o : offsets) set(o, 0u); };
    const auto zero8 = [&](std::initializer_list<std::uint32_t> offsets) { for (const std::uint32_t o : offsets) put8(rt, self + o, 0u); };
    const auto build = [&](std::uint32_t constructor, std::uint32_t offset) { call_set(rt, ctx, constructor, {self + offset}); };
    build(0x08AD023Cu, 0u);
    set(0u, 0x08CBF5E8u);
    set(0x64u, 0x08CBF654u);
    set(0x68u, 0x00996000u);
    set(0x6Cu, 0x00065800u);
    set(0x70u, 0x00010000u);
    set(0x74u, 0x80u);
    set(0x78u, 0x000F0000u);
    set(0x8Cu, 0x00020000u);
    zero({0x7Cu, 0x80u, 0x84u, 0x88u, 0x90u, 0x94u, 0x98u});
    zero8({0x9Cu});
    build(0x08B22A74u, 0xA0u);
    build(0x0892BE4Cu, 0xD8u);
    build(0x08B225C8u, 0x340u);
    zero({0x5A0u, 0x5A4u});
    zero8({0x5A8u, 0x5A9u, 0x5AAu});
    zero({0x5ACu, 0x5B0u, 0x5B8u});
    build(0x08896CB0u, 0x5BCu);
    build(0x089187BCu, 0x5CCu);
    zero({0x5F0u, 0x5F4u, 0x5F8u, 0x5FCu});
    build(0x08893A2Cu, 0x600u);
    for (std::uint32_t k = 0u; k < 2u; ++k) build(0x0888AC24u, 0x608u + k * 0x1C38u);
    zero({0x3E78u, 0x3F50u, 0x3F54u});
    build(0x08865E94u, 0x3F58u);
    build(0x08892A7Cu, 0x3F80u);
    build(0x08894444u, 0x3F88u);
    build(0x08893F7Cu, 0x3F90u);
    put8(rt, self + 0x3FB9u, 1u);
    set(0x3FC8u, 1u);
    for (const std::uint32_t o : {0x3FC4u, 0x3FACu, 0x3FB0u, 0x3FBCu, 0x3FC0u}) put_float(rt, self + o, 1.0f);
    zero({0x3F98u, 0x3F9Cu, 0x3FA0u, 0x3FA4u});
    zero8({0x3FA8u});
    zero({0x3FB4u});
    zero8({0x3FB8u, 0x3FCCu});
    zero({0x3FD0u});
    zero8({0x3FD8u, 0x3FD9u, 0x3FDAu, 0x3FDBu});
    call_set(rt, ctx, 0x08B04D98u, {self + 0x3FDCu, 0u, 0u, 0u});
    call_set(rt, ctx, 0x088FA158u, {self + 0x3FE4u, self + 0x3FDCu});
    for (std::uint32_t k = 0u; k < 4u; ++k) zero({0x3FECu + k * 8u, 0x3FF0u + k * 8u});
    zero({0x400Cu, 0x4010u});
    put8(rt, frame.at(0u), 0u);
    call_set(rt, ctx, 0x08B8E4E0u, {self + 0x4014u, self + 0x4018u, frame.at(0u)});
    zero8({0x4018u, 0x4019u});
    build(0x08866388u, 0x401Cu);
    zero({0x4274u, 0x4278u, 0x4270u, 0x427Cu, 0x4280u, 0x4284u});
    build(0x08B8ED84u, 0x4490u);
    zero({0x44A8u, 0x44A4u, 0x44A0u});
    build(0x08B8ED84u, 0x44ACu);
    build(0x08B8ED84u, 0x44BCu);
    build(0x08B8ED84u, 0x44CCu);
    build(0x08B907C4u, 0x44DCu);
    build(0x08B8ED84u, 0x4508u);
    build(0x0890F1B0u, 0x4518u);
    set(0x4524u, 0x08CD70F8u);
    zero({0x452Cu, 0x4530u, 0x4528u});
    call_set(rt, ctx, 0x08B8EA8Cu, {self + 0x4528u, 8u});
    set(0x4534u, 0x08CBF440u);
    call_set(rt, ctx, 0x08889A14u, {self + 0x4538u, self + 0x4534u});
    set(0x4554u, 0x08CBF4B8u);
    set(0x4558u, 0x08CBF4E8u);
    call_set(rt, ctx, 0x08888370u, {self + 0x455Cu, self + 0x4554u, self + 0x4558u, self + 0x4540u});
    build(0x0888A600u, 0x4584u);
    set(0x4594u, 0x08CC4250u);
    set(0x4598u, 0x08CC4264u);
    zero({0x4590u});
    set(0x458Cu, 0x08CC4200u);
    zero({0x459Cu, 0x45A0u});
    build(0x088F2AF8u, 0x45A4u);
    call_set(rt, ctx, 0x088F9970u, {self + 0x45B4u, self + 0x4598u});
    build(0x088F2728u, 0x45C4u);
    set(0x45D4u, 0x08CC3660u);
    zero({0x45D8u});
    call_set(rt, ctx, 0x089170E0u, {self + 0x45DCu, 1u});
    put8(rt, self + 0x4620u, 1u);
    put_float(rt, self + 0x4624u, 52.0f);
    set(0x462Cu, 0x3FE38E39u);
    set(0x4658u, 0x08CC6838u);
    zero8({0x45F8u, 0x45F9u, 0x45FAu});
    zero({0x460Cu, 0x4610u, 0x4614u});
    zero8({0x4618u});
    zero({0x461Cu});
    zero8({0x4628u});
    zero({0x4630u, 0x4634u, 0x4638u});
    zero8({0x463Cu});
    zero({0x4640u, 0x4644u, 0x4648u, 0x464Cu});
    zero8({0x4650u, 0x4651u, 0x4652u, 0x4653u, 0x4654u});
    zero({0x465Cu, 0x4660u, 0x4664u, 0x4668u});
    build(0x08B28DCCu, 0x4670u);
    set(0x4658u, 0x08CC6850u);
    zero({0x4698u, 0x469Cu, 0x46A4u});
    const std::uint32_t scheduler = self + 0x4584u;
    call_set(rt, ctx, 0x08889BB4u, {scheduler, self + 0x4594u});
    for (const std::uint32_t o : {0x458Cu, 0x4538u, 0x45B4u, 0x45A4u, 0x45C4u}) call_set(rt, ctx, 0x08889D4Cu, {scheduler, self + o});
    for (const std::uint32_t source : {0x088CBFD4u, 0x088CA548u, 0x088C7F64u, 0x08916474u}) {
        const std::uint32_t part = call_set(rt, ctx, source, {});
        call_set(rt, ctx, 0x08889D4Cu, {scheduler, part});
    }
    call_set(rt, ctx, 0x08B36018u, {self + 0x4120u, 0u, 0x140u});
    zero8({0x45FBu});
    for (std::uint32_t k = 0u; k < 4u; ++k) {
        set(0x4260u + k * 4u, 0u);
        put16(rt, self + 0x3E7Cu + k * 2u, 0u);
        put16(rt, self + 0x3E84u + k * 2u, 0xFFFFu);
        put8(rt, self + 0x3E8Cu + k, 0u);
    }
    call_set(rt, ctx, 0x0885DD70u, {self});
    call_set(rt, ctx, 0x08B8E998u, {self + 0x4270u, 4u});
    call_set(rt, ctx, 0x08B8E998u, {self + 0x427Cu, 4u});
}


// One member of a script module built by build_module: its name string, the type name its constructor takes
// (func_08A52660), its vtable and its slot in the module.
struct ModuleMember {
    std::uint32_t name, type_name, vtable, slot;
};

// The script-module builder func_08828D74 and func_08834A8C share: a 0x5C-byte module (func_08A54F3C(module, name,
// table_size)) whose members (0x38 bytes each) are registered with func_08A535A4(module, name, member, -1, slot), then
// handed to owner->+0 (func_08A7CE50) and kept at `keep`. Every temporary sits in the original's stack slot: the module's
// name at `name_slot` (the object pointer 4 bytes above), its reference at `ref_slot`, and member k's name / reference at
// ref_slot - 4 - 8k / ref_slot - 8 - 8k, its type name / object pointer at name_slot - 8 - 8k / name_slot - 4 - 8k.
void build_module(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t owner,
                  std::uint32_t module_name, std::uint32_t table_size, std::uint32_t name_slot, std::uint32_t ref_slot,
                  std::initializer_list<ModuleMember> members, std::uint32_t module_message, std::uint32_t owner_message,
                  std::uint32_t keep) {
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    const std::uint32_t module_ref = at(ref_slot);
    call_set(rt, ctx, 0x08A69B3Cu, {at(name_slot), module_name});
    const std::uint32_t module = call_set(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
    call_set(rt, ctx, 0x08A54F3Cu, {module, at(name_slot), table_size});
    put(rt, at(name_slot + 4u), module);
    call_set(rt, ctx, 0x08A52AB0u, {module_ref, at(name_slot + 4u)});
    call_set(rt, ctx, 0x08A69D68u, {at(name_slot)});
    std::uint32_t k = 0u;
    for (const ModuleMember &m : members) {
        const std::uint32_t name = at(ref_slot - 4u - 8u * k);
        const std::uint32_t member_ref = at(ref_slot - 8u - 8u * k);
        const std::uint32_t type_name = at(name_slot - 8u - 8u * k);
        const std::uint32_t member_slot = at(name_slot - 4u - 8u * k);
        const std::uint32_t target = word(rt, module_ref);
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {module_message});
        call_set(rt, ctx, 0x08A69B3Cu, {name, m.name});
        const std::uint32_t member = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
        call_set(rt, ctx, 0x08A69B3Cu, {type_name, m.type_name});
        call_set(rt, ctx, 0x08A52660u, {member, type_name});
        call_set(rt, ctx, 0x08A69D68u, {type_name});
        put(rt, member, m.vtable);
        put(rt, member_slot, member);
        call_set(rt, ctx, 0x08A52AB0u, {member_ref, member_slot});
        call_set(rt, ctx, 0x08A535A4u, {target, name, member_ref, 0xFFFFFFFFu, m.slot});
        call_set(rt, ctx, kDestroy, {member_ref});
        call_set(rt, ctx, 0x08A69D68u, {name});
        ++k;
    }
    const std::uint32_t holder = word(rt, owner);
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {owner_message});
    call_set(rt, ctx, 0x08A7CE50u, {holder, module_ref});
    put(rt, keep, word(rt, module_ref));
    call_set(rt, ctx, kDestroy, {module_ref});
}

// func_08828D74(owner): the script module named 0x08C9A214 (table 0x1C) with seven members, slots 0 to 0x18; kept at
// 0x08CE2B3C (build_module).
void func_08828D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0xB0u);
    build_module(rt, ctx, frame, owner, 0x08C9A214u, 0x1Cu, 0x38u, 0x78u,
                 {{0x08C9A224u, 0x08C9A230u, 0x08CB79B8u, 0u}, {0x08C9A238u, 0x08C9A244u, 0x08CB7B30u, 4u},
                  {0x08C9A24Cu, 0x08C9A244u, 0x08CB7B30u, 8u}, {0x08C9A258u, 0x08C9A244u, 0x08CB7B30u, 0xCu},
                  {0x08C9A268u, 0x08C9A244u, 0x08CB7B30u, 0x10u}, {0x08C9A274u, 0x08C9A244u, 0x08CB7B30u, 0x14u},
                  {0x08C9A280u, 0x08C9A244u, 0x08CB7B30u, 0x18u}},
                 0x08C9A1A8u, 0x08C9A28Cu, 0x08CE2B3Cu);
}

// func_08834A8C(owner): the script module named 0x08C9B500 (table 0x18) with eight members; kept at 0x08CE2B8C
// (build_module). The owner pointer is also kept at sp+0x90, as the original spills it.
void func_08834A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0xD0u);
    put(rt, frame.at(0x90u), owner);
    build_module(rt, ctx, frame, owner, 0x08C9B500u, 0x18u, 0x40u, 0x88u,
                 {{0x08C9B510u, 0x08C9B4A8u, 0x08CB7840u, 9u}, {0x08C9B51Cu, 0x08C9B520u, 0x08CB7E20u, 0u},
                  {0x08C9B528u, 0x08C9B530u, 0x08CB79B8u, 0xCu}, {0x08C9B538u, 0x08C9B4E0u, 0x08CB7F98u, 0x10u},
                  {0x08C9B53Cu, 0x08C9B4E0u, 0x08CB7F98u, 0x12u}, {0x08C9B540u, 0x08C9B4E0u, 0x08CB7F98u, 0x14u},
                  {0x08C9B550u, 0x08C9B4A8u, 0x08CB7840u, 0x16u}, {0x08C9B560u, 0x08C9B4A8u, 0x08CB7840u, 0x17u}},
                 0x08C9B1D4u, 0x08C9B4F8u, 0x08CE2B8Cu);
}


// func_089645C4(self): registers a script class: ten native bindings on self->+0 (func_08B86FB0(class, name, fn) for
// 0x08CA2358 / 0x08CA2368; func_08BE6098(class, name, fn, fn) and func_08BE6154(class, name, 0, 0x08964B48) for the
// others), then each class of the static table 0x08CF3908 as a member (func_08A7CC30); the table holds 75 classes
// (their getters called in order, built once behind the guard 0x08CF3900) and ends at its first zero. Last come the
// classes func_089C4100() and func_089C40F4() and the value func_08991490 makes, handed to the class with
// func_08A7CE50. A null class goes to func_08A5BB7C(0x08CA2350) first, each time it is read.
void func_089645C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const auto target = [&] {
        const std::uint32_t object = word(rt, self);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2350u});
        return object;
    };
    struct Binding {
        std::uint32_t kind, name, function;  // kind 0: func_08B86FB0, 1: func_08BE6098, 2: func_08BE6154
    };
    static constexpr Binding kBindings[] = {
        {0u, 0x08CA2358u, 0x0896428Cu}, {0u, 0x08CA2368u, 0x08964294u}, {1u, 0x08CA2378u, 0x0896429Cu},
        {1u, 0x08CA2380u, 0x0896432Cu}, {1u, 0x08CA238Cu, 0x089643BCu}, {2u, 0x08CA239Cu, 0x08964B48u},
        {2u, 0x08CA23ACu, 0x08964B48u}, {1u, 0x08CA23BCu, 0x0896444Cu}, {1u, 0x08CA23CCu, 0x089644E4u},
        {1u, 0x08CA23E0u, 0x0896457Cu},
    };
    for (const Binding &b : kBindings) {
        const std::uint32_t object = target();
        if (b.kind == 0u) call_set(rt, ctx, 0x08B86FB0u, {object, b.name, b.function});
        else if (b.kind == 1u) call_set(rt, ctx, 0x08BE6098u, {object, b.name, b.function, b.function});
        else call_set(rt, ctx, 0x08BE6154u, {object, b.name, 0u, b.function});
    }
    constexpr std::uint32_t kTable = 0x08CF3908u;
    once(rt, ctx, 0x08CF3900u, [&] {
        static constexpr std::uint32_t kGetters[75] = {
        0x08A1170Cu, 0x08A0F03Cu, 0x08A0E844u, 0x08A0CF80u, 0x08A0C918u, 0x08A0C4BCu, 0x08A0C060u, 0x08A0B9B8u, 
        0x08A06E14u, 0x08A04BD0u, 0x08A03A78u, 0x08A0355Cu, 0x08A003B4u, 0x089FF128u, 0x089FEDFCu, 0x089FDF80u, 
        0x089FD934u, 0x089FC8BCu, 0x089FB8D4u, 0x089F7860u, 0x089F60E4u, 0x089F5A90u, 0x089F543Cu, 0x089F16E8u, 
        0x089F0BF0u, 0x089ED9E4u, 0x089ED4C0u, 0x089ECF9Cu, 0x089E4D18u, 0x089DF9E4u, 0x089DA8B0u, 0x089D8374u, 
        0x089D5B30u, 0x089D5458u, 0x089D4FF8u, 0x089D4CA0u, 0x089CC898u, 0x089CA6C0u, 0x089C40A0u, 0x089C35F0u, 
        0x089C16E8u, 0x089BE7F0u, 0x089BA3DCu, 0x089B9E60u, 0x089B92A4u, 0x089B57E4u, 0x089B4FC4u, 0x089B47F4u, 
        0x089B3404u, 0x089A88B8u, 0x089A6920u, 0x089A2B50u, 0x089A24E4u, 0x089A06CCu, 0x0899D000u, 0x0899C210u, 
        0x0899B8ACu, 0x08999674u, 0x08995C6Cu, 0x08994488u, 0x089922DCu, 0x08991ACCu, 0x08A11E30u, 0x089915F0u, 
        0x0898D810u, 0x08988ED8u, 0x0897E284u, 0x0897D6C0u, 0x0897721Cu, 0x08976CD0u, 0x089767ACu, 0x089742A4u, 
        0x08965B18u, 0x08965418u, 0x08A141E8u
        };
        for (std::uint32_t i = 0u; i < 75u; ++i) put(rt, kTable + i * 4u, call_set(rt, ctx, kGetters[i], {}));
    });
    std::uint32_t object = word(rt, self);
    for (std::uint32_t entry = kTable; word(rt, entry) != 0u; entry += 4u) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2350u});
        call_set(rt, ctx, 0x08A7CC30u, {object, word(rt, entry)});
        object = word(rt, self);
    }
    for (const std::uint32_t getter : {0x089C4100u, 0x089C40F4u}) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2350u});
        call_set(rt, ctx, 0x08A7CC30u, {object, call_set(rt, ctx, getter, {})});
        object = word(rt, self);
    }
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2350u});
    call_set(rt, ctx, 0x08991490u, {frame.at(0u)});
    call_set(rt, ctx, 0x08A7CE50u, {object, frame.at(0u)});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}


// func_0891F6CC(self, car, info, index): one camera update. Adds the integer info+0xC to the float self+0x218, checks
// whether the car's name (func_088DA458) is one of the five at 0x08C8A620, and asks the camera mode self+0x230 for
// eight floats (mode 1: func_0891E93C, 2: func_0891E5BC(index), otherwise func_0891EB20 or func_0891EE68 by
// func_089273B4(self+0x1E0), both with the name match): eye (+0x1F8), target (+0x204), fov (+0x214), roll (+0x210).
// The view matrix self+0x10 is reset (func_08B4E648); for a valid index whose point func_088DCD90 gives, it becomes
// translate(point) x pitch/yaw/roll of the point's direction (slots 0x38 / 0x3C) times translate(eye) x the look angles
// (atan2 of the normalized eye-to-target vector, wrapped to (-pi, pi]); the field of view self+0x80 is clamped to
// [0.1, 80], and +0x1E4 / +0x50 get the matrix row +0x40, +0x60 the three angles. Otherwise +0x1E4..+0x1F0 are cleared.
// Returns self+0x1E4.
void func_0891F6CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t car = ctx.gpr[5];
    const std::uint32_t info = ctx.gpr[6];
    const std::uint32_t index = ctx.gpr[7];
    GuestFrame frame(ctx, 0x1C0u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    const float pi = std::bit_cast<float>(0x40490FDBu);
    const float two_pi = std::bit_cast<float>(0x40C90FDBu);
    put_float(rt, self + 0x218u, float_at(rt, self + 0x218u) + static_cast<float>(signed_of(word(rt, info + 0xCu))));
    for (std::uint32_t i = 0u; i < 5u; ++i) put(rt, at(0x2Cu + i * 4u), word(rt, 0x08C8A620u + i * 4u));
    std::uint32_t listed = 0u;
    for (std::uint32_t i = 0u; i < 5u; ++i) {
        const std::uint32_t name = call_set(rt, ctx, 0x088DA458u, {car});
        if (call_set(rt, ctx, 0x08B35FD8u, {name, word(rt, at(0x2Cu + i * 4u))}) == 0u) listed = 1u;
    }
    std::uint32_t pose;
    const std::uint32_t mode = word(rt, self + 0x230u);
    if (mode == 1u) {
        pose = at(0x60u);
        call_set(rt, ctx, 0x0891E93Cu, {pose, self, car});
    } else if (mode == 2u) {
        pose = at(0x40u);
        call_set(rt, ctx, 0x0891E5BCu, {pose, self, car, index});
    } else if (call_set(rt, ctx, 0x089273B4u, {word(rt, self + 0x1E0u)}) != 0u) {
        pose = at(0xA0u);
        call_set(rt, ctx, 0x0891EB20u, {pose, self, listed});
    } else {
        pose = at(0x80u);
        call_set(rt, ctx, 0x0891EE68u, {pose, self, listed});
    }
    const std::uint32_t eye = self + 0x1F8u;
    for (std::uint32_t i = 0u; i < 6u; ++i) put(rt, eye + i * 4u, word(rt, pose + i * 4u));
    put(rt, self + 0x210u, word(rt, pose + 0x1Cu));
    put(rt, self + 0x214u, word(rt, pose + 0x18u));
    const std::uint32_t view = self + 0x10u;
    const std::uint32_t object_matrix = at(0xC0u);
    const std::uint32_t look_matrix = at(0x100u);
    call_set(rt, ctx, 0x08B4E648u, {view});
    call_set(rt, ctx, 0x08B4E648u, {object_matrix});
    call_set(rt, ctx, 0x08B4E648u, {look_matrix});
    const std::uint32_t out = self + 0x1E4u;
    const auto clear = [&] {
        put(rt, out, 0u);
        put(rt, out + 0xCu, 0u);
        put(rt, out + 4u, 0u);
        put(rt, out + 8u, 0u);
        ctx.gpr[2] = out;
    };
    if (signed_of(index) < 0 || call_set(rt, ctx, 0x088DC90Cu, {car, index}) != 1u) {
        clear();
        return;
    }
    const std::uint32_t point = call_set(rt, ctx, 0x088DCD90u, {car, index});
    if (point == 0u) {
        clear();
        return;
    }
    const std::uint32_t position = call_virtual_at(rt, ctx, point, 0x38u, {point});
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, at(0xCu + i * 4u), word(rt, position + i * 4u));
    const std::uint32_t direction = call_virtual_at(rt, ctx, point, 0x3Cu, {point});
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, at(0x1Cu + i * 4u), word(rt, direction + i * 4u));
    const std::uint32_t translate = at(0x140u);
    const auto translation = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        put(rt, at(0x170u), x);
        put(rt, at(0x174u), y);
        put(rt, at(0x178u), z);
        for (const std::uint32_t one : {0x140u, 0x154u, 0x168u, 0x17Cu}) put_float(rt, at(one), 1.0f);
        for (const std::uint32_t zero : {0x150u, 0x160u, 0x144u, 0x164u, 0x148u, 0x158u, 0x14Cu, 0x15Cu, 0x16Cu}) put(rt, at(zero), 0u);
    };
    translation(word(rt, at(0xCu)), word(rt, at(0x10u)), word(rt, at(0x14u)));
    call_set(rt, ctx, 0x08B4E9D4u, {object_matrix, translate});
    call_setf(rt, ctx, 0x08B4F3B0u, {object_matrix}, {float_at(rt, at(0x20u))});
    call_setf(rt, ctx, 0x08B4F2E4u, {object_matrix}, {float_at(rt, at(0x1Cu))});
    call_setf(rt, ctx, 0x08B4F47Cu, {object_matrix}, {-float_at(rt, at(0x24u))});
    const std::uint32_t delta = at(0u);
    put(rt, delta + 4u, word(rt, self + 0x208u));
    put(rt, delta + 8u, word(rt, self + 0x20Cu));
    put(rt, delta, word(rt, self + 0x204u));
    call_set(rt, ctx, 0x08B4AA68u, {delta, delta, eye});
    call_set(rt, ctx, 0x08B4A720u, {delta});
    call_setf(rt, ctx, 0x08B4A5E8u, {delta}, {1.0f / std::sqrt(ctx.fpr[0])});
    put_float(rt, at(0x1Cu), call_setf(rt, ctx, 0x08B66E88u, {}, {float_at(rt, delta + 4u)}).f0);
    const float heading = call_setf(rt, ctx, 0x08B66FBCu, {}, {float_at(rt, delta), float_at(rt, delta + 8u)}).f0 - pi;
    put_float(rt, at(0x20u), heading);
    put_float(rt, at(0x24u), psp_mul(float_at(rt, self + 0x214u), std::bit_cast<float>(0x3C8EFA35u)));
    if (pi < heading) put_float(rt, at(0x20u), heading - two_pi);
    else if (heading < -pi) put_float(rt, at(0x20u), heading + two_pi);
    const float roll = float_at(rt, self + 0x210u);
    put_float(rt, self + 0x80u, roll);
    if (roll < std::bit_cast<float>(0x3DCCCCCDu)) put(rt, self + 0x80u, 0x3DCCCCCDu);
    if (80.0f < float_at(rt, self + 0x80u)) put_float(rt, self + 0x80u, 80.0f);
    translation(word(rt, eye), word(rt, eye + 4u), word(rt, eye + 8u));
    call_set(rt, ctx, 0x08B4E9D4u, {look_matrix, translate});
    call_setf(rt, ctx, 0x08B4F3B0u, {look_matrix}, {float_at(rt, at(0x20u))});
    call_setf(rt, ctx, 0x08B4F2E4u, {look_matrix}, {float_at(rt, at(0x1Cu))});
    call_setf(rt, ctx, 0x08B4F47Cu, {look_matrix}, {float_at(rt, at(0x24u))});
    call_set(rt, ctx, 0x08B4E9D4u, {view, object_matrix});
    call_set(rt, ctx, 0x08B4E9D4u, {view, look_matrix});
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, out + i * 4u, word(rt, view + 0x30u + i * 4u));
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, self + 0x50u + i * 4u, word(rt, out + i * 4u));
    put(rt, self + 0x60u, word(rt, at(0x1Cu)));
    put(rt, self + 0x64u, word(rt, at(0x20u)));
    put(rt, self + 0x68u, word(rt, at(0x24u)));
    put(rt, self + 0x6Cu, 0u);
    ctx.gpr[2] = out;
}

} // namespace hot_batch_119
} // namespace gt::game

#define HOT119(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_119::name, gt::ReturnKind::kind)
HOT119(0x089AD0E0u, func_089AD0E0, Word);
HOT119(0x0892331Cu, func_0892331C, Word);
HOT119(0x088E29DCu, func_088E29DC, Word);
HOT119(0x08B907F8u, func_08B907F8, Word);
HOT119(0x08828D74u, func_08828D74, Word);
HOT119(0x08834A8Cu, func_08834A8C, Word);
HOT119(0x089645C4u, func_089645C4, Word);
HOT119(0x0891F6CCu, func_0891F6CC, Word);
