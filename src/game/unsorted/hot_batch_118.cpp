// Hot batch 118: functions of 240-300 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_118 {
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


// Copies `count` words from `source` to `destination`.
inline void copy_words(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t count) {
    for (std::uint32_t index = 0u; index < count; ++index) put(rt, destination + index * 4u, word(rt, source + index * 4u));
}

// One holder counted under the owner's lock: func_08B34888(owner), owner->+0x2C + 1, func_08B34B24(owner).
inline void lock_count_use(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t owner) {
    call_set(rt, ctx, 0x08B34888u, {owner});
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) + 1u);
    call_set(rt, ctx, 0x08B34B24u, {owner});
}

// What follows func_08B34888(owner) when one use is dropped: count 0 -> unlock, the weak holder (+0x34 / +0x38) told (virtual
// slot 0), then func_08B28BE0 and the free func_08B64654; otherwise the count steps down and, when it reaches 0, the
// weak state is taken out (virtual slots 8 and 0 run after the unlock, func_08B34C1C first when byte +0x30 is set).
inline void lock_drop_use(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t owner) {
    if (word(rt, owner + 0x2Cu) == 0u) {
        call_set(rt, ctx, 0x08B34B24u, {owner});
        const std::uint32_t weak = owner + 0x34u;
        const std::uint32_t holder = word(rt, weak + 4u);
        if (holder != 0u) {
            call_virtual_at(rt, ctx, holder, 0u, {holder, word(rt, owner + 0x34u)});
            put(rt, weak + 4u, 0u);
            put(rt, owner + 0x34u, 0u);
        }
        call_set(rt, ctx, 0x08B28BE0u, {owner});
        call_set(rt, ctx, 0x08B64654u, {owner});
        return;
    }
    std::uint32_t dropped_second = 0u;
    std::uint32_t dropped_first = 0u;
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) - 1u);
    if (word(rt, owner + 0x2Cu) == 0u) {
        const std::uint32_t weak_object = word(rt, owner + 0x34u);
        if (weak_object != 0u) {
            dropped_second = word(rt, owner + 0x38u);
            dropped_first = weak_object;
            put(rt, owner + 0x34u, 0u);
            put(rt, owner + 0x38u, 0u);
        }
        if (byte_u(rt, owner + 0x30u) != 0u) call_set(rt, ctx, 0x08B34C1Cu, {owner});
    }
    call_set(rt, ctx, 0x08B34B24u, {owner});
    if (dropped_first != 0u) call_virtual_at(rt, ctx, dropped_first, 8u, {dropped_first});
    if (dropped_second != 0u) call_virtual_at(rt, ctx, dropped_second, 0u, {dropped_second, dropped_first});
}

// Puts `item` (when non-null) on the free chain of the size-class record that func_08BB1F48()'s table gives for `key`
// (looked up, or made with func_08BB1D0C(+0x24, +0x28, &key, 0x08BB1B40) when missing or of another class): the record's
// +0x1C count goes down and the chain head (+0xC) takes the item. v0 ends as the original's: the new count with an item, else
// the lookup result (or `key` when the cached record matched).
inline void return_to_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key, std::uint32_t item, std::uint32_t key_slot) {
    const std::uint32_t context = call_set(rt, ctx, 0x08BB1F48u, {});
    put(rt, key_slot, key);
    std::uint32_t record = word(rt, context + 0x34u);
    if (record != 0u) ctx.gpr[2] = word(rt, record + 0x10u);
    if (record == 0u || word(rt, record + 0x10u) != key) {
        const std::uint32_t found = call_set(rt, ctx, 0x08BB1D0Cu, {word(rt, context + 0x24u), word(rt, context + 0x28u), key_slot, 0x08BB1B40u});
        record = word(rt, found);
        put(rt, context + 0x34u, record);
    }
    if (item != 0u) {
        const std::uint32_t head = word(rt, record + 0xCu);
        const std::uint32_t count = word(rt, record + 0x1Cu) - 1u;
        put(rt, record + 0x1Cu, count);
        put(rt, item, head);
        put(rt, record + 0xCu, item);
        ctx.gpr[2] = count;
    }
}

// func_08BACB64(self): nothing when self->+0xC (record) is 0. Otherwise: when record->+0x68 (a) and record->+0x20 (b) are
// set and b->+8 (c) is set, a->+0x48 is cleared if it equals c->+0x48. Then the words at +0xB74 / 0xB68 / 0xB58 / 0xB4C /
// 0xB40 / 0xB30 / 0xB20 get 0x08CC1730 each followed by func_08AF20E8 on that field; +0xB14 / +0xB0C get 0x08CC22B0 /
// 0x08CC2288 with func_08AF86DC. func_08BAC238(record->+0x8E0, record->+0x8E4, frame), func_08B35254(record->+0x8E0) when
// set, func_08909D98 on +0x60, the shared pointer at +0x5C released, func_08909D98 on +0x48 and +0x38, on +0x28, the
// shared pointers at +0x24, +0x1C, +0x14, +0xC and +4 released and func_08B64654(record) frees it.
void func_08BACB64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t record = word(rt, ctx.gpr[4] + 0xCu);
    if (record == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t a = word(rt, record + 0x68u);
    if (a != 0u) {
        const std::uint32_t b = word(rt, record + 0x20u);
        if (b != 0u) {
            const std::uint32_t c = word(rt, b + 8u);
            if (c != 0u && word(rt, a + 0x48u) == word(rt, c + 0x48u)) put(rt, a + 0x48u, 0u);
        }
    }
    const std::uint32_t kind = 0x08CC1730u;
    for (const std::uint32_t offset : {0xB74u, 0xB68u, 0xB58u, 0xB4Cu, 0xB40u, 0xB30u, 0xB20u}) {
        put(rt, record + offset, kind);
        call_set(rt, ctx, 0x08AF20E8u, {record + offset});
    }
    put(rt, record + 0xB14u, 0x08CC22B0u);
    call_set(rt, ctx, 0x08AF86DCu, {record + 0xB14u});
    put(rt, record + 0xB0Cu, 0x08CC2288u);
    call_set(rt, ctx, 0x08AF86DCu, {record + 0xB0Cu});
    call_set(rt, ctx, 0x08BAC238u, {word(rt, record + 0x8E0u), word(rt, record + 0x8E4u), frame.at(0u)});
    if (word(rt, record + 0x8E0u) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, record + 0x8E0u)});
    call_set(rt, ctx, 0x08909D98u, {record + 0x60u});
    const auto release = [&](std::uint32_t slot) {
        const std::uint32_t object = word(rt, slot);
        if (object != 0u) release_shared(rt, ctx, object);
    };
    release(record + 0x5Cu);
    for (std::uint32_t element = record + 0x58u; element != record + 0x38u;) {
        element -= 0x10u;
        call_set(rt, ctx, 0x08909D98u, {element});
    }
    call_set(rt, ctx, 0x08909D98u, {record + 0x28u});
    release(record + 0x24u);
    release(record + 0x1Cu);
    release(record + 0x14u);
    release(record + 0xCu);
    release(record + 4u);
    call_set(rt, ctx, 0x08B64654u, {record});
}

// func_08A1ED68(holder): registers 18 callbacks on the object holder->+0 (func_08A5BB7C(0x08CAA488) first when it is null):
// each is func_08B86FB0(object, name, handler) with name 0x08CAA490 ... 0x08CAA5DC and handler a code address; after the
// 11th, a value object is built in the frame (func_08A85B1C(frame, 0x481907FF)), func_08B86E98(object, 0x08CAA56C, frame)
// takes it and it is destroyed (func_08A5B9EC).
void func_08A1ED68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const auto object_of_holder = [&]() {
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA488u});
        return object;
    };
    const auto bind = [&](std::uint32_t name, std::uint32_t handler) {
        const std::uint32_t object = object_of_holder();
        call_set(rt, ctx, 0x08B86FB0u, {object, name, handler});
    };
    bind(0x08CAA490u, 0x08A1EBE4u);
    bind(0x08CAA4A4u, 0x08A1EBECu);
    bind(0x08CAA4B8u, 0x08A1EBF4u);
    bind(0x08CAA4C8u, 0x08A1EBFCu);
    bind(0x08CAA4E0u, 0x08A1EC04u);
    bind(0x08CAA4F8u, 0x08A1FE40u);
    bind(0x08CAA50Cu, 0x08A1FCECu);
    bind(0x08CAA520u, 0x08A1FBA0u);
    bind(0x08CAA534u, 0x08A1FA54u);
    bind(0x08CAA54Cu, 0x08A1F8FCu);
    bind(0x08CAA55Cu, 0x08A1F7C0u);
    {
        const std::uint32_t object = object_of_holder();
        call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), 0x481907FFu});
        call_set(rt, ctx, 0x08B86E98u, {object, 0x08CAA56Cu, frame.at(0u)});
        call_set(rt, ctx, kDestroy, {frame.at(0u)});
    }
    bind(0x08CAA57Cu, 0x08A1ED00u);
    bind(0x08CAA588u, 0x08A1F130u);
    bind(0x08CAA598u, 0x08A1EC0Cu);
    bind(0x08CAA5A8u, 0x08A1F5D4u);
    bind(0x08CAA5B8u, 0x08A1F520u);
    bind(0x08CAA5C8u, 0x08A1F174u);
    bind(0x08CAA5DCu, 0x08A1F328u);
}

// func_0895FA54(self, mode): converts a byte table at 0x08CDE508 into float tables of self. With n = the byte at +0x23,
// mode = 0 when it was -1: self[0xC + 4i] = (table[0x24 + i] * 2) x (1/7.0625...) and self[0x2C + 4i] = table[0x2C + i] x
// 0.001 (i < n); func_08946734(self, n, self + 0xC, self + 0x2C, 0, 0). Then +0x4C = byte[0x34] / 100 + 1 (1 when 0);
// +0x50.. / +0x58.. / +0x60.. = the byte pairs at 0xC / 0x12 / 0x18 x 0.005; +0x68 / +0x6C / +0x70 = bytes 0x11 / 0x17 /
// 0x1D x 0.005; +0x134 = byte 0x1E / 10 (1 when 0); +0x138 = (100 - byte 0x1F) / 100 (0 when 0); +0x13C / +0x140 =
// 1 / (10 x byte 0x20 / 0x21) (0 when 0); +0x144 = byte 0x22 / 100 (1 when 0). Last, 24 entries i (the index is 0xF for
// entry 5 when mode is 3): +0x74 + 4i = byte[0x38 + i] / 100 (1 when 0), +0xD4 + 4i = 1 / (10 x byte[0x50 + i]) (0 when 0),
// +0x148 + 4i = halfword[0x80 + 2k] x 0.01, +0x1A8 + 4i = byte[0x68 + k] x 0.001, +0x208 + 4i = 6.28318... / (byte[0xB0 + k]
// x 0.1), and the bytes +0x268 + i / +0x280 + i = bytes [0xC8 + k] / [0xE0 + k]. v0 ends as the last byte [0xC8 + k].
void func_0895FA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t mode = ctx.gpr[5];
    const std::uint32_t table = 0x08CDE508u;
    const std::uint32_t count = byte_u(rt, table + 0x23u);
    if (mode == 0xFFFFFFFFu) mode = 0u;
    const auto number = [&](std::uint32_t value) { return static_cast<float>(signed_of(value)); };
    if (signed_of(count) > 0) {
        const float scale_a = std::bit_cast<float>(0x3E8E38E4u);
        const float scale_b = std::bit_cast<float>(0x3A83126Fu);
        for (std::uint32_t index = 0u; index != count; ++index) {
            put_float(rt, self + 0xCu + index * 4u, psp_mul(number(byte_u(rt, table + 0x24u + index) << 1), scale_a));
            put_float(rt, self + 0x2Cu + index * 4u, psp_mul(number(byte_u(rt, table + 0x2Cu + index)), scale_b));
        }
    }
    call_set(rt, ctx, 0x08946734u, {self, count, self + 0xCu, self + 0x2Cu, 0u, 0u});
    const float one = 1.0f;
    const float hundred = std::bit_cast<float>(0x42C80000u);
    float value = one;
    std::uint32_t byte = byte_u(rt, table + 0x34u);
    if (byte != 0u) value = number(byte) / hundred + one;
    put_float(rt, self + 0x4Cu, value);
    const float half_percent = std::bit_cast<float>(0x3BA3D70Au);
    for (std::uint32_t index = 0u; index < 2u; ++index) {
        put_float(rt, self + 0x50u + index * 4u, psp_mul(number(byte_u(rt, table + 0xCu + index)), half_percent));
        put_float(rt, self + 0x58u + index * 4u, psp_mul(number(byte_u(rt, table + 0x12u + index)), half_percent));
        put_float(rt, self + 0x60u + index * 4u, psp_mul(number(byte_u(rt, table + 0x18u + index)), half_percent));
    }
    put_float(rt, self + 0x68u, psp_mul(number(byte_u(rt, table + 0x11u)), half_percent));
    put_float(rt, self + 0x6Cu, psp_mul(number(byte_u(rt, table + 0x17u)), half_percent));
    put_float(rt, self + 0x70u, psp_mul(number(byte_u(rt, table + 0x1Du)), half_percent));
    byte = byte_u(rt, table + 0x1Eu);
    put_float(rt, self + 0x134u, byte == 0u ? one : number(byte) / std::bit_cast<float>(0x41200000u));
    byte = byte_u(rt, table + 0x1Fu);
    put_float(rt, self + 0x138u, byte == 0u ? 0.0f : number(100u - byte) / hundred);
    byte = byte_u(rt, table + 0x20u);
    put_float(rt, self + 0x13Cu, byte == 0u ? 0.0f : one / number(byte * 10u));
    byte = byte_u(rt, table + 0x21u);
    put_float(rt, self + 0x140u, byte == 0u ? 0.0f : one / number(byte * 10u));
    byte = byte_u(rt, table + 0x22u);
    put_float(rt, self + 0x144u, byte == 0u ? one : number(byte) / hundred);
    const float centi = std::bit_cast<float>(0x3C23D70Au);
    const float milli = std::bit_cast<float>(0x3A83126Fu);
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float turn = std::bit_cast<float>(0x40C90FDBu);
    std::uint32_t last = ctx.gpr[2];
    for (std::uint32_t index = 0u; index != 24u; ++index) {
        const std::uint32_t k = (mode == 3u && index == 5u) ? 0xFu : index;
        byte = byte_u(rt, table + 0x38u + index);
        put_float(rt, self + 0x74u + index * 4u, byte == 0u ? one : number(byte) / hundred);
        byte = byte_u(rt, table + 0x50u + index);
        put_float(rt, self + 0xD4u + index * 4u, byte == 0u ? 0.0f : one / number(byte * 10u));
        const std::uint32_t half = half_u(rt, table + k * 2u + 0x80u);
        put_float(rt, self + 0x148u + index * 4u, psp_mul(number(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(half)))), centi));
        put_float(rt, self + 0x1A8u + index * 4u, psp_mul(number(byte_u(rt, table + k + 0x68u)), milli));
        byte = byte_u(rt, table + k + 0xB0u);
        put_float(rt, self + 0x208u + index * 4u, turn / psp_mul(number(byte), tenth));
        last = byte_u(rt, table + k + 0xC8u);
        put8(rt, self + 0x268u + index, last);
        put8(rt, self + 0x280u + index, byte_u(rt, table + k + 0xE0u));
    }
    ctx.gpr[2] = last;
}

// func_088AC1D4(self, other): walks 16 steps over a ring of 8 records (0x44 bytes each, at other + 0x64, index & 7): a record
// whose word +0x20 is set is skipped. With the 2D point p = self[0], self[2] and the segment end q taken from self's
// entry (+0x10, moved by the halfword at +0xC when negative), it tests the segment from the record's point (+0x18) to the
// neighbour's (+0x18 of index - 1, or + 1 when the step has bit 3 set) against p with cross products; a hit builds the
// nearest point on the segment into the frame (func_08B4AC30 on other + 0x1C) or, past the segment, a pushed-out vector
// (func_08B4A708 gives its length; func_08B4AC0C adds it to other + 0x1C), then func_088AB798(other, other + 0x18, 0); the
// last case also marks the record (+0x20 |= 1, +0x2C / +0x30 = the direction x 1/length, +0x34 = self). Ends with v0 = 0.
void func_088AC1D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    std::uint32_t entry = self + 0x10u;
    const std::int32_t shift = signed_of(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(half_u(rt, self + 0xCu)))));
    if (shift < 0) entry += static_cast<std::uint32_t>(shift) << 4;
    const std::uint32_t ring = other + 0x64u;
    const std::uint32_t result = other + 0x1Cu;
    const float f23 = float_at(rt, self);
    const float f22 = -float_at(rt, self + 8u);
    const float f25 = -float_at(rt, entry + 8u);
    const float f26 = float_at(rt, entry);
    const float f21 = f26 - f23;
    const float f20 = f25 - f22;
    const float one = 1.0f;
    const float f27 = psp_mul(f21, f21) + psp_mul(f20, f20);
    std::uint32_t step = 0u;
    do {
        const std::uint32_t record = ring + (step & 7u) * 0x44u;
        if (word(rt, record + 0x20u) != 0u) {
            ++step;
            continue;
        }
        const std::uint32_t next = step + 1u;
        std::uint32_t neighbour = (step & 8u) != 0u ? next : step - 1u;
        neighbour = ring + (neighbour & 7u) * 0x44u + 0x18u;
        float f6 = float_at(rt, record + 0x18u);
        float f7 = float_at(rt, record + 0x1Cu);
        float f4 = float_at(rt, neighbour);
        float f2 = float_at(rt, neighbour + 4u);
        const float f14 = f7 - f22;
        const float f13 = f6 - f23;
        const float f12 = f4 - f23;
        const float f11 = f2 - f22;
        const float f5a = psp_mul(f20, f13);
        float f0 = psp_mul(f21, f14);
        const float f3a = psp_mul(f20, f12);
        float f1 = psp_mul(f21, f11);
        f6 = f6 - f4;
        f7 = f7 - f2;
        f4 = f4 - f26;
        f2 = f2 - f25;
        f0 = f0 - f5a;
        f1 = f1 - f3a;
        const float f9 = psp_mul(f11, f6);
        const float f8 = psp_mul(f12, f7);
        float f5 = psp_mul(f6, f2);
        float f3 = psp_mul(f7, f4);
        const float f10 = 0.0f;
        if (f10 <= f0) {
            step = next;
            continue;
        }
        if (f1 <= f10) {
            step = next;
            continue;
        }
        f0 = f9 - f8;
        f1 = f5 - f3;
        f3 = 0.0f;
        if (f0 < f3) {
            step = next;
            continue;
        }
        if (f3 < f1) {
            step = next;
            continue;
        }
        f1 = psp_mul(f21, f13);
        f0 = psp_mul(f20, f14);
        float f24 = f1 + f0;
        const auto push_result = [&](float x, float y) {
            put_float(rt, frame.at(0xCu), y);
            step = next;
            put_float(rt, frame.at(8u), x);
            call_set(rt, ctx, 0x08B4AC30u, {result, result, frame.at(8u)});
            call_set(rt, ctx, 0x088AB798u, {other, other + 0x18u, 0u});
        };
        if (f24 < f3) {
            f1 = psp_mul(f6, f6);
            f0 = psp_mul(f7, f7);
            f2 = f1 + f0;
            if (f2 == f3) {
                f0 = psp_mul(f3, f7);
            } else {
                f0 = -f12;
                f1 = psp_mul(f11, f7);
                f0 = psp_mul(f6, f0);
                f0 = f0 - f1;
                f3 = f0 / f2;
                f0 = psp_mul(f3, f7);
            }
            f1 = psp_mul(f3, f6);
            f0 = f11 + f0;
            f1 = f12 + f1;
            push_result(f1, f0);
            continue;
        }
        f1 = psp_mul(f6, f6);
        if (f27 < f24) {
            f0 = psp_mul(f7, f7);
            f5 = f1 + f0;
            if (f5 == f3) {
                f0 = psp_mul(f3, f7);
            } else {
                f0 = -f4;
                f1 = psp_mul(f7, f2);
                f0 = psp_mul(f6, f0);
                f0 = f0 - f1;
                f3 = f0 / f5;
                f0 = psp_mul(f3, f7);
            }
            f1 = psp_mul(f3, f6);
            f0 = f2 + f0;
            f1 = f4 + f1;
            push_result(f1, f0);
            continue;
        }
        float f28 = one;
        if (f27 != f3) {
            const float root = std::sqrt(f27);
            f24 = f24 / f27;
            f28 = one / root;
        }
        f0 = psp_mul(f24, f21);
        f1 = psp_mul(f24, f20);
        f0 = f0 - f13;
        f1 = f1 - f14;
        put_float(rt, frame.at(0u), f0);
        put_float(rt, frame.at(4u), f1);
        call_setf(rt, ctx, 0x08B4A708u, {frame.at(0u)}, {});
        f0 = ctx.fpr[0];
        f1 = std::bit_cast<float>(0x3E800000u);
        if (f1 < f0) {
            if (signed_of(step) < 8) f0 = one - f24;
            else f0 = -f24;
            f1 = psp_mul(f20, f0);
            f0 = psp_mul(f21, f0);
            put_float(rt, frame.at(4u), f1);
            put_float(rt, frame.at(0u), f0);
            call_set(rt, ctx, 0x08B4AC0Cu, {result, result, frame.at(0u)});
            call_set(rt, ctx, 0x088AB798u, {other, other + 0x18u, 0u});
            step = next;
            continue;
        }
        call_set(rt, ctx, 0x08B4AC0Cu, {result, result, frame.at(0u)});
        call_set(rt, ctx, 0x088AB798u, {other, other + 0x18u, 0u});
        const std::uint32_t flags = word(rt, record + 0x20u);
        f0 = psp_mul(f20, f28);
        f1 = psp_mul(f21, f28);
        step = next;
        put_float(rt, record + 0x30u, f0);
        put(rt, record + 0x20u, flags | 1u);
        put_float(rt, record + 0x2Cu, f1);
        put(rt, record + 0x34u, self);
    } while (signed_of(step) < 16);
    ctx.gpr[2] = 0u;
}

// func_08A7858C(out, holder, mode, arg): kind = holder->+0->+8. mode 1: a table is asked of the object arg->+0 (virtual
// slot 0x24); the chain of tables (+0x38 links, each needing +4 set) is searched for a slot [kind * 4 + 0x3C] whose
// object has +4 set; that object's virtual slot 0x2C(object, out, 1, arg) runs and the result is 1 (0 if none). Other modes
// with arg->+0 and arg->+4 both set: when slot 0x2C of the first gives a count smaller than that of the second, two
// value objects are built in the frame (func_08A76AA0), the second object's virtual slot 0x68 fills one (assigned to the
// other), and the same table search runs on the first with the result as a3 (all value objects destroyed after);
// otherwise the table of arg->+0 is searched and slot 0x2C(object, out, mode, arg) runs. Without both set, kind 4 / 0xD
// builds a boolean value object (func_08A97A6C(temp, arg->+0 == arg->+4 / !=)) assigned to out (result 1). Result in v0.
void func_08A7858C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t arg = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t kind = word(rt, word(rt, holder) + 8u);
    const auto finish = [&](std::uint32_t result) { ctx.gpr[2] = result; };
    // The table chain: slot [kind * 4 + 0x3C] of a table with a set object (+4); else the next table (+0x38), which needs +4.
    const auto lookup = [&](std::uint32_t table) -> std::uint32_t {
        for (;;) {
            const std::uint32_t slot = word(rt, table + kind * 4u + 0x3Cu);
            if (slot != 0u && word(rt, slot + 4u) != 0u) return slot;
            table = word(rt, table + 0x38u);
            if (table == 0u || word(rt, table + 4u) == 0u) return 0u;
        }
    };
    const auto invoke = [&](std::uint32_t slot, std::uint32_t second, std::uint32_t third) {
        ctx.gpr[2] = word(rt, slot);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x2Cu);
        call_set(rt, ctx, ctx.gpr[3], {slot, out, second, third});
    };
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    };
    if (mode == 1u) {
        const std::uint32_t object = word(rt, arg);
        if (object == 0u) return finish(0u);
        const std::uint32_t table = call_virtual_at(rt, ctx, object, 0x24u, {object});
        if (table == 0u) return finish(0u);
        const std::uint32_t slot = lookup(table);
        if (slot == 0u) return finish(0u);
        invoke(slot, 1u, arg);
        return finish(1u);
    }
    const std::uint32_t first = word(rt, arg);
    if (first == 0u || word(rt, arg + 4u) == 0u) {
        if (kind == 4u || kind == 0xDu) {
            const std::uint32_t temporary = frame.at(kind == 4u ? 8u : 4u);
            const std::uint32_t other = word(rt, arg + 4u);
            const std::uint32_t equal = first ^ other;
            call_set(rt, ctx, 0x08A97A6Cu, {temporary, kind == 4u ? static_cast<std::uint32_t>(equal == 0u) : static_cast<std::uint32_t>(equal != 0u)});
            keep_temporary(rt, ctx, out, temporary);
            return finish(1u);
        }
        return finish(0u);
    }
    const std::uint32_t second_slot = arg + 4u;
    const std::uint32_t count_first = call_virtual_at(rt, ctx, first, 0x2Cu, {first});
    std::uint32_t object = word(rt, second_slot);
    checked(object);
    const std::uint32_t count_second = call_virtual_at(rt, ctx, object, 0x2Cu, {object});
    if (!(count_first < count_second)) {
        object = word(rt, arg);
        checked(object);
        const std::uint32_t table = call_virtual_at(rt, ctx, object, 0x24u, {object});
        if (table == 0u) return finish(0u);
        const std::uint32_t slot = lookup(table);
        if (slot == 0u) return finish(0u);
        invoke(slot, mode, arg);
        return finish(1u);
    }
    const std::uint32_t pair = frame.at(0xCu);
    call_set(rt, ctx, 0x08A76AA0u, {pair});
    call_set(rt, ctx, 0x08A76AA0u, {pair + 4u});
    object = word(rt, second_slot);
    checked(object);
    call_virtual_at(rt, ctx, object, 0x68u, {frame.at(0u), object, arg});
    keep_temporary(rt, ctx, pair, frame.at(0u));
    std::uint32_t result = 0u;
    object = word(rt, pair);
    if (object != 0u) {
        if (pair + 4u != second_slot) call_set(rt, ctx, kAssign, {pair + 4u, word(rt, second_slot)});
        object = word(rt, pair);
        checked(object);
        const std::uint32_t table = call_virtual_at(rt, ctx, object, 0x24u, {object});
        if (table != 0u) {
            const std::uint32_t slot = lookup(table);
            if (slot != 0u) {
                invoke(slot, mode, pair);
                result = 1u;
            }
        }
    }
    std::uint32_t element = pair + 8u;
    do {
        element -= 4u;
        call_set(rt, ctx, kDestroy, {element});
    } while (element != pair);
    finish(result);
}

// func_08889244(self, list, source): with source and list->+0 (a vector of 16-byte elements) both non-empty, count n = source
// virtual slot 8; the vector of 8-byte entries at self + 0xC (begin +0xC, end +0x10) is shrunk to n (func_08BA9498) or grown
// with a {0, 0} pair from the frame (func_08BA996C), then func_0888879C(frame). For each index i < n, with entry =
// begin + 8i: a = source slot 0x1C(i), b = source slot 0x18(i)'s slot 8 (-1 without b). With entry word 0 set and a
// null or rejected (slot 0x24) a, the entry is cleared and its shared pointer (+4) released; a kept entry whose a
// is set and b is a valid index of list->+0 asks self->+0x18's slot 8 for a new {object, shared pointer}, stored into the
// entry (the old shared pointer released, the new one counted), and the entry object's slot 0x1C(entry, list, a) runs.
// With source or the vector empty: func_08BA9498(self + 0xC, begin, end).
void func_08889244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[5];
    const std::uint32_t source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t entries = self + 0xCu;
    bool empty = true;
    if (source != 0u) {
        const std::uint32_t vector = word(rt, list);
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - word(rt, vector)) >> 4);
        empty = count == 0u;
    }
    if (empty) {
        const std::uint32_t last = word(rt, self + 0x10u);
        const std::uint32_t first = word(rt, entries);
        call_set(rt, ctx, 0x08BA9498u, {entries, first, last});
        return;
    }
    const std::uint32_t wanted = call_virtual_at(rt, ctx, source, 8u, {source});
    const std::uint32_t begin = word(rt, entries);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    ctx.gpr[7] = begin;
    if (wanted < static_cast<std::uint32_t>(signed_of(word(rt, entries + 4u) - begin) >> 3)) {
        const std::uint32_t last = word(rt, self + 0x10u);
        call_set(rt, ctx, 0x08BA9498u, {entries, begin + (wanted << 3), last});
    } else {
        const std::uint32_t last = word(rt, entries + 4u);
        const std::uint32_t first = word(rt, entries);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(last - first) >> 3);
        call_set(rt, ctx, 0x08BA996Cu, {entries, last, wanted - size, frame.at(0u)});
    }
    call_set(rt, ctx, 0x0888879Cu, {frame.at(0u)});
    if (signed_of(wanted) <= 0) return;
    std::uint32_t index = 0u;
    do {
        const std::uint32_t base = word(rt, entries);
        const std::uint32_t entry = base + index * 8u;
        std::uint32_t a = call_virtual_at(rt, ctx, source, 0x1Cu, {source, index, base});
        const std::uint32_t holder = call_virtual_at(rt, ctx, source, 0x18u, {source, index});
        std::int32_t b = -1;
        if (holder != 0u) b = signed_of(call_virtual_at(rt, ctx, holder, 8u, {holder}));
        enum class Next { Advance, Middle, Call } next = Next::Middle;
        std::uint32_t object = word(rt, entry);
        if (object != 0u) {
            bool cleared = false;
            if (a == 0u) {
                a = word(rt, entry + 4u);
                cleared = true;
            } else if (call_virtual_at(rt, ctx, a, 0x24u, {a}) != 0u) {
                a = word(rt, entry + 4u);
                cleared = true;
            } else {
                next = word(rt, entry) != 0u ? Next::Call : Next::Middle;
            }
            if (cleared) {
                put(rt, entry, 0u);
                put(rt, entry + 4u, 0u);
                if (a != 0u) release_shared(rt, ctx, a);
                next = Next::Advance;
            }
        }
        if (next == Next::Middle) {
            if (a == 0u || b < 0) {
                next = Next::Advance;
            } else {
                const std::uint32_t vector = word(rt, list);
                const std::uint32_t first = word(rt, vector);
                const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - first) >> 4);
                ctx.gpr[2] = static_cast<std::uint32_t>(b < signed_of(count));
                if (!(b < signed_of(count))) {
                    next = Next::Advance;
                } else {
                    const std::uint32_t maker = word(rt, self + 0x18u);
                    call_virtual_at(rt, ctx, maker, 8u, {frame.at(8u), maker, a, first + (static_cast<std::uint32_t>(b) << 4), vector + 0xCu});
                    const std::uint32_t made = word(rt, frame.at(8u));
                    const std::uint32_t old_block = word(rt, entry + 4u);
                    ctx.gpr[2] = old_block;
                    std::uint32_t block = word(rt, frame.at(0xCu));
                    put(rt, entry, made);
                    put(rt, frame.at(0x10u), entry + 4u);
                    if (block != old_block) {
                        if (block != 0u) count_up(rt, ctx, block + 4u);
                        ctx.gpr[2] = word(rt, frame.at(0x10u));
                        const std::uint32_t previous = word(rt, ctx.gpr[2]);
                        if (previous != 0u) release_shared(rt, ctx, previous);
                        put(rt, word(rt, frame.at(0x10u)), block);
                        block = word(rt, frame.at(0xCu));
                    }
                    if (block != 0u) release_shared(rt, ctx, block);
                    next = word(rt, entry) != 0u ? Next::Call : Next::Advance;
                }
            }
        }
        if (next == Next::Call && a != 0u) {
            const std::uint32_t target = word(rt, entry);
            call_virtual_at(rt, ctx, target, 0x1Cu, {target, list, a});
        }
        ++index;
    } while (wanted != index);
}

// func_08AA5944(self): func_08A69B3C(frame, 0x08CB2E2C); a 0x38-byte block (func_08A5BC20(0x38, 4)) is initialised
// (func_08A7E914(block, frame)), its pointer stored in the frame (+4) and handed to func_08A76AB4(self, frame + 4);
// func_08A69D68(frame). Then with object = self->+0 (func_08A5BB7C(0x08CB2E30) first when null) a series of registrations:
// func_08A7CC30(object, getter()) for the getters 08A88C04 / 08A6ACA8, func_08A7CE50(object, value) for values built in
// the frame by func_08AA4468 / func_08AB6AD0, func_08B86E98(object, name, value) with values func_08A85B1C(frame, 1 / 2 / 6 / 0 /
// 4 / 2 / 1) and names 0x08CB2E38 ... 0x08CB2E64 (each value destroyed after), again func_08A7CC30 for the getters 08AB2AA4 / 08A68CD4 /
// 08AB5E50 / 08AB5454, and last func_08BE6098(object, 0x08CB2E68, 0x08AA593C, 0x08AA593C). Returns self.
void func_08AA5944(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CB2E2Cu});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call_set(rt, ctx, 0x08A7E914u, {block, frame.at(0u)});
    put(rt, frame.at(4u), block);
    call_set(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    // The object at self->+0, func_08A5BB7C(0x08CB2E30) first when it is null (a0 holds the relocated `lui` meanwhile).
    const auto object_of_self = [&]() {
        const std::uint32_t object = word(rt, self);
        ctx.gpr[4] = 0x08CB0000u;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2E30u});
        return object;
    };
    const auto getter = [&](std::uint32_t address) {
        const std::uint32_t object = object_of_self();
        const std::uint32_t value = call_set(rt, ctx, address, {});
        call_set(rt, ctx, 0x08A7CC30u, {object, value});
    };
    const auto built = [&](std::uint32_t builder, std::uint32_t temporary) {
        const std::uint32_t object = object_of_self();
        call_set(rt, ctx, builder, {temporary});
        call_set(rt, ctx, 0x08A7CE50u, {object, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    };
    const auto named = [&](std::uint32_t temporary, std::uint32_t number, std::uint32_t name) {
        const std::uint32_t object = object_of_self();
        call_set(rt, ctx, 0x08A85B1Cu, {temporary, number});
        call_set(rt, ctx, 0x08B86E98u, {object, name, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    };
    getter(0x08A88C04u);
    getter(0x08A6ACA8u);
    built(0x08AA4468u, frame.at(0x28u));
    built(0x08AB6AD0u, frame.at(0x24u));
    named(frame.at(0x20u), 1u, 0x08CB2E38u);
    named(frame.at(0x1Cu), 2u, 0x08CB2E40u);
    named(frame.at(0x18u), 6u, 0x08CB2E48u);
    named(frame.at(0x14u), 0u, 0x08CB2E50u);
    named(frame.at(0x10u), 4u, 0x08CB2E58u);
    named(frame.at(0xCu), 2u, 0x08CB2E5Cu);
    named(frame.at(8u), 1u, 0x08CB2E64u);
    getter(0x08AB2AA4u);
    getter(0x08A68CD4u);
    getter(0x08AB5E50u);
    getter(0x08AB5454u);
    const std::uint32_t object = object_of_self();
    call_set(rt, ctx, 0x08BE6098u, {object, 0x08CB2E68u, 0x08AA593Cu, 0x08AA593Cu});
    ctx.gpr[2] = self;
}

// func_08AEC210(a, b): under the lock G+0x49C (func_08C7AA20(lock, 1, 0) ... func_08C7AA08(lock, 1, counter)) every one of the
// G+0x4B4 records of 0x6C bytes at G+0x4B0 (G = 0x08D41AD0) whose word +0x38 (flags) is set is serviced and the word cleared:
// bits 0x1800 give func_08B8421C(index, record->+0x64, +0x68) (0x80FF / 0x9FC3 when both are 0), 0x3C0 func_08B83F40(index,
// +0x50 .. +0x5C), 0x400 func_08B83FA4(index, +0x60), and bit 2 selects by the mode word +0x3C (0 .. 3) a sequence of
// func_08B83E60 / 08B83E24 / 08B83DE8 / 08B83FF4 / 08B840AC / 08B83F40 / 08B8421C calls on the index, keeping the signed
// bytes +0 and +1 of the record as state; bit 5 (0x20) sets byte +1 from the word +0x4C (func_08B83E60(index, value)). Then the
// counter G+0x4BC is stepped (G+0x4C0 = its old value), the lock released and a mask is obtained (func_08B83C2C(a) when
// b is set, else func_08B83C84(a, 0x1000, 0x1000); a negative result is logged with func_08AD83B8 when flag 0x08CDFB3C & 1 is
// set); func_08B84270() gives a mask of records whose byte +0 is cleared. Ends with v0 = 0 (or the record count when <= 0).
void func_08AEC210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t g = 0x08D41AD0u;
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0u), a);
    put(rt, frame.at(4u), b);
    call_set(rt, ctx, 0x08C7AA20u, {word(rt, g + 0x49Cu), 1u, 0u});
    const auto records = [&]() { return word(rt, g + 0x4B0u); };
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(word(rt, g + 0x4B4u)); ++index) {
        const std::uint32_t record = records() + index * 0x6Cu;
        std::uint32_t flags = word(rt, record + 0x38u);
        if (flags == 0u) continue;
        const std::uint32_t state = record + 0x38u;
        put(rt, record + 0x38u, 0u);
        if ((flags & 0x1800u) != 0u) {
            std::uint32_t low = word(rt, state + 0x2Cu);
            std::uint32_t high = word(rt, state + 0x30u);
            if ((low | high) == 0u) {
                low = 0x80FFu;
                high = 0x9FC3u;
            }
            call_set(rt, ctx, 0x08B8421Cu, {index, low, high});
        }
        if ((flags & 0x3C0u) != 0u) {
            const std::uint32_t p1 = word(rt, state + 0x18u);
            const std::uint32_t p2 = word(rt, state + 0x1Cu);
            const std::uint32_t p3 = word(rt, state + 0x20u);
            const std::uint32_t p4 = word(rt, state + 0x24u);
            call_set(rt, ctx, 0x08B83F40u, {index, p1, p2, p3, p4});
        }
        if ((flags & 0x400u) != 0u) call_set(rt, ctx, 0x08B83FA4u, {index, word(rt, state + 0x28u)});
        const auto signed_byte = [&](std::uint32_t address) { return sext8(byte_u(rt, address)); };
        const auto release_slot = [&]() {
            // func_08B83E60(index, 0) and the byte +1 cleared after it.
            call_set(rt, ctx, 0x08B83E60u, {index, 0u});
            put8(rt, record + 1u, 0u);
        };
        const auto refresh = [&]() {
            call_set(rt, ctx, 0x08B840ACu, {index, word(rt, state + 0x10u)});
            if (signed_of(call_set(rt, ctx, 0x08B83DE8u, {index})) < 0) {
                call_set(rt, ctx, 0x08B83E24u, {index});
                call_set(rt, ctx, 0x08B83DE8u, {index});
            }
        };
        const auto clear_both = [&]() {
            call_set(rt, ctx, 0x08B83F40u, {index, 0u, 0u, 0u, 0u});
            call_set(rt, ctx, 0x08B83E60u, {index, 0u});
            put8(rt, record + 1u, 0u);
            call_set(rt, ctx, 0x08B83E24u, {index});
            put8(rt, record, 0u);
        };
        if ((flags & 2u) != 0u) {
            const std::uint32_t mode = word(rt, state + 4u);
            if (mode == 1u) {
                if (signed_byte(record + 1u) != 0u) release_slot();
                if (signed_byte(record) != 0u) call_set(rt, ctx, 0x08B83E24u, {index});
                refresh();
                put8(rt, record, mode);
            } else if (signed_of(mode) < 2) {
                if (mode == 0u) {
                    if (signed_byte(record + 1u) != 0u) release_slot();
                    if (signed_byte(record) != 0u) call_set(rt, ctx, 0x08B83E24u, {index});
                    const std::uint32_t first = word(rt, state + 8u);
                    const std::uint32_t second = word(rt, state + 0xCu);
                    call_set(rt, ctx, 0x08B83FF4u, {index, first, second, 1u});
                    if (signed_of(call_set(rt, ctx, 0x08B83DE8u, {index})) < 0) {
                        call_set(rt, ctx, 0x08B83E24u, {index});
                        call_set(rt, ctx, 0x08B83DE8u, {index});
                    }
                    put8(rt, record, 1u);
                }
            } else if (mode == 2u) {
                flags &= ~0x20u;
                if (signed_byte(record + 1u) != 0u) clear_both();
                else call_set(rt, ctx, 0x08B83E24u, {index});
            } else if (mode == 3u) {
                flags &= ~0x20u;
                if (signed_byte(record + 1u) != 0u) {
                    clear_both();
                } else {
                    call_set(rt, ctx, 0x08B8421Cu, {index, 0x80FFu, 0x9FE6u});
                    call_set(rt, ctx, 0x08B83E24u, {index});
                    put8(rt, record, 0u);
                }
            }
        }
        if ((flags & 0x20u) != 0u) {
            const std::uint32_t value = word(rt, state + 0x14u);
            if (signed_byte(record + 1u) != value) {
                call_set(rt, ctx, 0x08B83E60u, {index, value});
                put8(rt, record + 1u, value);
            }
        }
    }
    const std::uint32_t counter = word(rt, g + 0x4BCu);
    put(rt, g + 0x4BCu, counter + 1u);
    put(rt, g + 0x4C0u, counter);
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, g + 0x49Cu), 1u, counter});
    std::uint32_t mask;
    if (b != 0u) {
        mask = call_set(rt, ctx, 0x08B83C2Cu, {a});
        ctx.gpr[8] = mask;
        if (signed_of(mask) < 0 && (word(rt, 0x08CDFB3Cu) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, 0x664u, 0x08C920DCu});
    } else {
        mask = call_set(rt, ctx, 0x08B83C84u, {a, 0x1000u, 0x1000u});
        ctx.gpr[8] = mask;
        if (signed_of(mask) < 0 && (word(rt, 0x08CDFB3Cu) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, 0x66Bu, 0x08C920F0u});
    }
    const std::uint32_t bits = call_set(rt, ctx, 0x08B84270u, {});
    const std::uint32_t count = word(rt, g + 0x4B4u);
    ctx.gpr[2] = count;
    if (signed_of(count) <= 0) return;
    std::uint32_t index = 0u;
    do {
        const bool selected = ((static_cast<std::uint32_t>(signed_of(bits) >> (index & 31u))) & 1u) != 0u;
        if (selected) put8(rt, word(rt, g + 0x4B0u) + index * 0x6Cu, 0u);
        ++index;
    } while (signed_of(index) < signed_of(word(rt, g + 0x4B4u)));
    ctx.gpr[2] = 0u;
}

// func_088FD0F0(self, a, b): a record is built (func_08B62344(0x548) + func_088FCA30(record, a, b, 1), vtable 0x08CC51A8) and
// shared (a 0x10-byte block, func_08B8E380(frame + 4, record, record, 1)); the pair is swapped with self->+0x10 / +0x14 (the old
// block released). The 64-bit pair at 0x08C8A4E0 goes through func_08BBC08C(frame + 8, pair.high, pair.low, self->+0x10) giving
// a member pointer (v0 = function or offset, v1 = adjustment). With p = self->+0x10->+4 (shared, counted twice): when
// p->+0x2C is 0 the member function is called on self->+0x10 + adj >> 1 (virtual when adj & 1: [[this] + v0]); otherwise a
// 0x14-byte node {vtable 0x08CC51D0, self->+0x10, shared, v0, v1} (func_08B62344) replaces the one at p->+0x34 (the old one's
// virtual slot 0 runs first) and p->+0x34 + 4 gets the static object 0x08D76498 (initialised under a guard,
// func_08B6441C / func_08B64494). Last func_08B34B24(p) and the two uses of the shared pointer are released.
void func_088FD0F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x548u});
    call_set(rt, ctx, 0x088FCA30u, {block, first, second, 1u});
    put(rt, block, 0x08CC51A8u);
    put(rt, frame.at(0u), block);
    const std::uint32_t counter = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, counter + 4u, 1u);
    put(rt, counter, 0x08CC51B8u);
    put(rt, counter + 8u, 1u);
    put(rt, counter + 0xCu, block);
    put(rt, frame.at(4u), counter);
    call_set(rt, ctx, 0x08B8E380u, {frame.at(4u), block, block, 1u});
    {
        const std::uint32_t old_object = word(rt, self + 0x10u);
        const std::uint32_t new_object = word(rt, frame.at(0u));
        const std::uint32_t old_block = word(rt, self + 0x14u);
        const std::uint32_t new_block = word(rt, frame.at(4u));
        put(rt, frame.at(0u), old_object);
        ctx.gpr[7] = new_object;
        put(rt, self + 0x14u, new_block);
        put(rt, self + 0x10u, new_object);
        put(rt, frame.at(4u), old_block);
        if (old_block != 0u) release_shared(rt, ctx, old_block);
        ctx.gpr[7] = word(rt, self + 0x10u);
    }
    const std::uint32_t pair_low = word(rt, 0x08C8A4E0u);
    const std::uint32_t pair_high = word(rt, 0x08C8A4E4u);
    put(rt, frame.at(0xCu), pair_high);
    put(rt, frame.at(8u), pair_low);
    const std::uint32_t object = ctx.gpr[7];
    const GuestResult member = call_setf(rt, ctx, 0x08BBC08Cu, {frame.at(8u), pair_high, pair_low, object}, {});
    const std::uint32_t shared = word(rt, self + 0x14u);
    put(rt, frame.at(0x10u), member.v0);
    put(rt, frame.at(0x14u), member.v1);
    put(rt, frame.at(0x20u), member.v0);
    put(rt, frame.at(0x24u), member.v1);
    const std::uint32_t target = word(rt, self + 0x10u);
    if (shared != 0u) count_up(rt, ctx, shared + 4u);
    const std::uint32_t owner = word(rt, object + 4u);
    const std::uint32_t adjust = word(rt, frame.at(0x14u));
    const std::uint32_t function = word(rt, frame.at(0x10u));
    if (shared != 0u) count_up(rt, ctx, shared + 4u);
    call_set(rt, ctx, 0x08B34888u, {owner});
    if (word(rt, owner + 0x2Cu) == 0u) {
        const std::uint32_t receiver = target + static_cast<std::uint32_t>(signed_of(adjust) >> 1);
        std::uint32_t function_address = function;
        if ((adjust & 1u) != 0u) function_address = word(rt, word(rt, receiver) + function);
        ctx.gpr[2] = function_address;
        call_set(rt, ctx, function_address, {receiver});
    } else {
        const std::uint32_t slot = owner + 0x34u;
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
        const std::uint32_t node = call_set(rt, ctx, 0x08B62344u, {0x14u});
        put(rt, node, 0x08CC51D0u);
        put(rt, node + 4u, target);
        put(rt, node + 8u, shared);
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
        put(rt, node + 0x10u, adjust);
        put(rt, node + 0xCu, function);
        if (word(rt, slot) != node) {
            const std::uint32_t previous = word(rt, slot + 4u);
            if (previous == 0u) {
                put(rt, slot, node);
            } else {
                ctx.gpr[3] = word(rt, previous);
                ctx.gpr[2] = word(rt, ctx.gpr[3]);
                call_set(rt, ctx, ctx.gpr[2], {previous});
                put(rt, slot + 4u, 0u);
                put(rt, slot, 0u);
                put(rt, slot, node);
            }
            std::uint32_t statics = 0x08D76498u;
            if (byte_u(rt, 0x08D761B8u) == 0u) {
                const std::uint32_t guard = 0x08D761B8u;
                if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                    put(rt, statics, 0x08CC51E8u);
                    call_set(rt, ctx, 0x08B64494u, {guard});
                }
            }
            put(rt, slot + 4u, statics);
        }
        if (shared != 0u) release_shared(rt, ctx, shared);
    }
    call_set(rt, ctx, 0x08B34B24u, {owner});
    if (shared != 0u) {
        release_shared(rt, ctx, shared);
        release_shared(rt, ctx, shared);
    }
}

// func_0888CBAC(self, query, ids, count): with the byte at self->+4->+0x618->+0 set and count > 0 (else returns that byte or 0),
// for each halfword id of ids[0..count): e = func_088DA788(world, id) and, when func_088DD0E4(world, id) and func_088DC858(world,
// id) are both 0, a record is set up from e (a key triple in the frame, func_088D26B4(table, frame, e->+0x30) into e->+0x38) and,
// with self->+0x34 and e->+0x14 > 0, the 168-byte entry of self->+0x34 + 0xD78 + 168 * id is filled (bytes +0x11 = 1, +0x12 = 0,
// +0x13 = e->+0x2C, halfword +0x14 = e->+0x14, words from func_088D09A8 and the float of the object func_088DCD28(world, id)'s
// virtual slot 0x1C x 3.6 truncated to a halfword) followed by func_0888AD94, func_088D0580. When func_088DCEAC(world, id) equals
// func_088D4FC0(query) and func_088DC858 is 0, a small frame record is built (func_088D62FC(query), func_0888CAFC,
// func_088DA1C8, func_088DF324 choose its words) and, depending on self->+0x1C and func_08917898 / func_088D4DB8, each of
// func_08917840(self->+8) items is offered to func_0888B24C (when func_08918438 accepts it).
void func_0888CBAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t query = ctx.gpr[5];
    std::uint32_t ids = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t table = word(rt, word(rt, self + 4u) + 0x618u);
    put(rt, frame.at(0x24u), table);
    const std::uint32_t enabled = byte_u(rt, table);
    ctx.gpr[2] = enabled;
    if (enabled == 0u) return;
    if (signed_of(count) <= 0) return;
    const auto world = [&]() { return word(rt, self + 4u); };
    std::uint32_t done = 0u;
    do {
        const std::uint32_t id = half_u(rt, ids);
        const std::uint32_t entity = call_set(rt, ctx, 0x088DA788u, {world(), id});
        const std::uint32_t found = call_set(rt, ctx, 0x088DD0E4u, {world(), id});
        bool filled = found == 0u;
        if (filled && call_set(rt, ctx, 0x088DC858u, {world(), id}) != 0u) filled = false;
        if (filled) {
            const std::uint32_t second = word(rt, entity + 0x140u);
            const std::uint32_t third = word(rt, entity + 0x14u);
            put(rt, frame.at(0u), id);
            put(rt, frame.at(4u), second);
            put(rt, frame.at(8u), third);
            const std::uint32_t key = call_set(rt, ctx, 0x088D26B4u, {word(rt, frame.at(0x24u)), frame.at(0u), word(rt, entity + 0x30u)});
            put(rt, entity + 0x38u, key);
            if (word(rt, self + 0x34u) != 0u && signed_of(word(rt, entity + 0x14u)) > 0) {
                const std::uint32_t object = call_set(rt, ctx, 0x088DCD28u, {world(), id});
                call_virtual_at(rt, ctx, object, 0x1Cu, {object});
                const float scale = ctx.fpr[0];
                const std::uint32_t entry = word(rt, self + 0x34u) + id * 168u + 0xD78u;
                const std::uint32_t level = word(rt, entity + 0x2Cu);
                put8(rt, entry + 0x13u, level);
                put8(rt, entry + 0x11u, 1u);
                const std::uint32_t row = entry + ((level - 1u) << 2);
                put8(rt, entry + 0x12u, 0u);
                put16(rt, entry + 0x14u, word(rt, entity + 0x14u));
                const std::uint32_t built = call_set(rt, ctx, 0x088D09A8u, {entity, level - 1u});
                put(rt, row + 0x28u, built);
                const float scaled = psp_mul(scale, std::bit_cast<float>(0x40666666u));
                const std::uint32_t truncated = ctx.fpu_float_to_word(scaled, 1u);
                put(rt, row + 0x68u, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(truncated))));
                call_set(rt, ctx, 0x0888AD94u, {word(rt, self + 0x34u), id, entry});
                const std::uint32_t target = call_set(rt, ctx, 0x088DA7F0u, {world(), id});
                call_setf(rt, ctx, 0x088D0580u, {target, word(rt, entity + 0x30u)}, {scale});
            }
        }
        const std::uint32_t current = call_set(rt, ctx, 0x088DCEACu, {world(), id});
        const std::uint32_t wanted = call_set(rt, ctx, 0x088D4FC0u, {query});
        bool process = current == wanted;
        if (process && call_set(rt, ctx, 0x088DC858u, {world(), id}) != 0u) process = false;
        if (process) {
            for (std::uint32_t offset = 0xCu; offset != 0x1Cu; offset += 4u) put(rt, frame.at(offset), 0u);
            const std::uint32_t other = call_set(rt, ctx, 0x088DA788u, {world(), id});
            if (signed_of(word(rt, other + 0x14u)) <= 0) process = false;
        }
        if (process) {
            const std::uint32_t kind_slot = word(rt, entity + 0x30u);
            const std::uint32_t reference = word(rt, entity + kind_slot * 4u + 0xC0u);
            put(rt, frame.at(0x20u), reference);
            const std::uint32_t selected = call_set(rt, ctx, 0x088D62FCu, {query});
            if (selected == 0u) {
                if (word(rt, entity + 0x38u) != 0u) {
                    call_set(rt, ctx, 0x0888CAFCu, {self, id, 0u, 0u});
                    put(rt, frame.at(0xCu), ctx.gpr[2]);
                    put(rt, frame.at(0x10u), ctx.gpr[3]);
                    if (!(signed_of(ctx.gpr[3]) <= 0)) {
                        put(rt, frame.at(0xCu), 0u);
                        put(rt, frame.at(0x10u), 0u);
                    }
                }
                const std::uint32_t state = call_set(rt, ctx, 0x088DA1C8u, {world(), id});
                if (state == 1u) {
                    call_set(rt, ctx, 0x0888CAFCu, {self, id, 0u, 1u});
                    put(rt, frame.at(0x14u), ctx.gpr[2]);
                    put(rt, frame.at(0x18u), ctx.gpr[3]);
                }
            } else {
                const std::uint32_t magic = call_set(rt, ctx, 0x088DF324u, {world(), id, 0u});
                if (magic != 0x481907FFu) {
                    const std::uint32_t kind = word(rt, entity + 0x30u);
                    const std::uint32_t base = word(rt, world() + 0x618u);
                    const std::uint32_t row = kind * 12u + base;
                    const std::uint32_t reference_value = word(rt, entity + kind * 4u + 0xC0u);
                    put(rt, frame.at(0xCu), reference_value - word(rt, row + 0x23Cu));
                }
            }
            std::uint32_t mode = word(rt, self + 0x1Cu);
            ctx.gpr[2] = mode;
            std::uint32_t flag = 0u;
            bool via_check = mode == 2u;
            if (!via_check) {
                const std::uint32_t result = call_set(rt, ctx, 0x08917898u, {word(rt, self + 8u)});
                mode = word(rt, self + 0x1Cu);
                flag = static_cast<std::uint32_t>(result == 0u);
                via_check = mode == 2u;
            }
            bool enter = flag != 0u;
            if (via_check) {
                if (call_set(rt, ctx, 0x088D4DB8u, {query}) == 1u) {
                    put(rt, frame.at(0xCu), 0u);
                    put(rt, frame.at(0x14u), 0u);
                    put(rt, frame.at(0x10u), 0u);
                    put(rt, frame.at(0x18u), 0u);
                    enter = true;
                } else {
                    enter = flag != 0u;
                }
            }
            if (enter) {
                std::uint32_t item = 0u;
                const auto items = [&]() {
                    const std::uint32_t total = call_set(rt, ctx, 0x08917840u, {word(rt, self + 8u)});
                    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(item) < signed_of(total));
                    return signed_of(item) < signed_of(total);
                };
                while (items()) {
                    const std::uint32_t accepted = call_set(rt, ctx, 0x08918438u, {word(rt, self + 8u), id, item});
                    if (accepted != 0u) call_set(rt, ctx, 0x0888B24Cu, {self, item, word(rt, frame.at(0x20u)), frame.at(0xCu)});
                    const std::uint32_t per_kind = word(rt, entity + 0x2Cu);
                    const std::uint32_t row = word(rt, world() + 0x618u) + per_kind * 12u - 0xCu;
                    const std::uint32_t reference_word = word(rt, row + 0xCu);
                    if (call_set(rt, ctx, 0x08918438u, {word(rt, self + 8u), reference_word, item}) != 0u) {
                        if (word(rt, frame.at(0x18u)) == 0u) call_set(rt, ctx, 0x0888B24Cu, {self, item, 0u, frame.at(0x14u)});
                    }
                    ++item;
                }
                ctx.gpr[2] = 0u;
            }
        }
        ++done;
        ids += 2u;
    } while (count != done);
}

// func_088C3118(unused, obj, out): float result f0 = f20 and *out = f21 - f20 (when out is set). With obj->+0x24 (path), +0x1C
// (a), +0x18 (b), obj->+4 (world), obj->+8 / +0xC and the parameter block at obj + 4 all set: f21 = block->+4->+0x6C4 and, when
// one of the path bytes +0xF / +0x6D / +0xA5 is set, f20 = world->+0x1360. Otherwise f20 = path->+0x18 and for kind byte
// +0x12 of 3 or 4 a walk over the path's stretches (func_088C50B8 / 088C5324 / 088C50C4 on a frame list, func_088B1A0C,
// func_088BD288, func_088BD23C, func_08B49D7C) lowers f20 to min(f20, f12) with f12 the position reached, f20 + ...
// for other kinds the stretch ends (func_088C52A0, func_088DF13C) interpolate f20 += ratio * (path->+0x1C - path->+0x18).
// Zero when any pointer is missing.
void func_088C3118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[5];
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t params = obj + 4u;
    const std::uint32_t path = word(rt, obj + 0x24u);
    const std::uint32_t list = word(rt, obj + 0x1Cu);
    const std::uint32_t second = word(rt, obj + 0x18u);
    float f20 = 0.0f;
    float f21 = 0.0f;
    bool usable = path != 0u && list != 0u && second != 0u;
    std::uint32_t world = 0u;
    std::uint32_t block = 0u;
    if (usable) {
        world = word(rt, obj + 4u);
        usable = world != 0u;
    }
    if (usable) {
        block = word(rt, params + 4u);
        usable = block != 0u;
    }
    if (usable) {
        ctx.gpr[2] = word(rt, params + 8u);
        usable = ctx.gpr[2] != 0u;
    }
    if (usable) {
        f21 = float_at(rt, block + 0x6C4u);
        std::uint32_t flag = byte_u(rt, path + 0xFu);
        ctx.gpr[2] = flag;
        bool plain = flag != 0u;
        if (!plain) {
            flag = byte_u(rt, path + 0x6Du);
            ctx.gpr[2] = flag;
            if (flag != 0u) {
                plain = true;
            } else {
                flag = byte_u(rt, path + 0xA5u);
                ctx.gpr[2] = flag;
                plain = flag != 0u;
            }
        }
        if (plain) {
            f20 = float_at(rt, world + 0x1360u);
        } else {
            const std::uint32_t kind = byte_u(rt, path + 0x12u);
            f20 = float_at(rt, path + 0x18u);
            const bool walk = ((kind - 3u) & 0xFFu) < 2u;
            ctx.gpr[2] = static_cast<std::uint32_t>(walk);
            const std::uint32_t scratch = frame.at(0u);
            if (walk) {
                call_set(rt, ctx, 0x088C50B8u, {scratch, list, byte_u(rt, path + 0x10u)});
                std::uint32_t selector = byte_u(rt, path + 0x11u);
                if ((word(rt, scratch) ^ selector) == 0u) selector = 0u;
                const std::uint32_t cursor = call_set(rt, ctx, 0x088C5324u, {scratch, selector});
                const std::uint32_t first = call_set(rt, ctx, 0x088C50C4u, {scratch, cursor});
                f20 = float_at(rt, first + 4u);
                f20 = f20 - float_at(rt, word(rt, params + 4u) + 0x10u);
                const std::uint32_t again = call_set(rt, ctx, 0x088C50C4u, {scratch, cursor});
                float f12 = float_at(rt, again + 8u);
                f20 = psp_mul(f20, f20);
                f12 = f12 - float_at(rt, word(rt, params + 4u) + 0x14u);
                f12 = psp_mul(f12, f12);
                float f23 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f20 + f12}).f0;
                const float f26 = float_at(rt, path + 0x1Cu);
                if (!(0.0f < f23)) {
                    f12 = f26;
                } else {
                    const float f27 = 0.0f;
                    float f24 = f26;
                    const float f25 = std::bit_cast<float>(0x4031C71Du);
                    for (;;) {
                        const std::uint32_t previous = call_set(rt, ctx, 0x088C50C4u, {scratch, cursor});
                        const float f22 = f24 + f25;
                        const std::uint32_t owner = word(rt, obj);
                        const std::uint32_t extra = word(rt, obj + 0x14u);
                        const std::uint32_t what = call_set(rt, ctx, 0x088B1A0Cu, {owner});
                        const std::uint32_t t0 = what == 2u ? 1u : (what == 3u ? 2u : 0u);
                        f20 = call_setf(rt, ctx, 0x088BD288u, {previous, params, second, extra, t0}, {f22, f24}).f0;
                        const std::uint32_t item = call_set(rt, ctx, 0x088C50C4u, {scratch, cursor});
                        const float f0 = call_setf(rt, ctx, 0x088BD23Cu, {item}, {f20}).f0;
                        if (f0 < f23) {
                            f23 = f23 - f0;
                            const bool more = f27 < f23;
                            f24 = f22;
                            if (more) continue;
                            f12 = f26;
                            break;
                        }
                        f12 = f22;
                        if (0.0f < f0) f12 = f24 + psp_mul(f23, f25) / f0;
                        break;
                    }
                }
                f20 = float_at(rt, path + 0x18u);
                if (!(f20 < f12)) f20 = f12;
            } else if (f20 < float_at(rt, path + 0x1Cu)) {
                call_set(rt, ctx, 0x088C50B8u, {scratch, list, byte_u(rt, path + 0x10u)});
                std::uint32_t s0 = word(rt, scratch);
                std::uint32_t selector = byte_u(rt, path + 0x11u);
                if ((s0 ^ selector) == 0u) selector = 0u;
                const std::uint32_t cursor = call_set(rt, ctx, 0x088C5324u, {scratch, selector});
                const std::uint32_t first = call_set(rt, ctx, 0x088C50C4u, {scratch, cursor});
                std::uint32_t previous_index = byte_u(rt, path + 0x11u);
                float f23 = float_at(rt, first + 0x14u);
                previous_index -= 1u;
                const bool negative = signed_of(previous_index) < 0;
                s0 += previous_index;
                const std::uint32_t wrapped = call_set(rt, ctx, 0x088C52A0u, {scratch, negative ? s0 : previous_index});
                const std::uint32_t last = call_set(rt, ctx, 0x088C50C4u, {scratch, wrapped});
                const float f22 = float_at(rt, last + 0x14u);
                if (f23 < f22) {
                    call_virtual_at(rt, ctx, list, 0x14u, {list});
                    f23 = f23 + ctx.fpr[0];
                }
                bool adjust = !(f23 <= f22);
                if (adjust) {
                    const std::uint32_t entry = call_set(rt, ctx, 0x088DF13Cu, {word(rt, params + 8u), word(rt, params + 0xCu)});
                    if (float_at(rt, entry) < f22) adjust = false;
                }
                if (adjust) {
                    const std::uint32_t entry = call_set(rt, ctx, 0x088DF13Cu, {word(rt, params + 8u), word(rt, params + 0xCu)});
                    const float span = f23 - f22;
                    const float one = 1.0f;
                    float ratio = float_at(rt, entry) - f22;
                    float f3 = std::bit_cast<float>(0x3E99999Au);
                    ratio = ratio / span;
                    float f2 = 0.0f;
                    bool scale = true;
                    if (!(one < ratio)) {
                        const float seven_tenths = std::bit_cast<float>(0x3F333333u);
                        if (ratio < seven_tenths) scale = false;
                        else f3 = ratio - seven_tenths;
                    }
                    if (scale) f2 = f3 / std::bit_cast<float>(0x3E99999Au);
                    const float f0 = float_at(rt, path + 0x1Cu) - float_at(rt, path + 0x18u);
                    f20 = f20 + psp_mul(f2, f0);
                }
            }
        }
    } else {
        f21 = f20;
    }
    if (out != 0u) put_float(rt, out, f21 - f20);
    ctx.fpr[0] = f20;
}

// func_089DD648(self, target): two one-shot flags are consumed atomically (compare 1, store 0): self->+0x1B8 set runs
// func_089AB194(target, self + 0x138, self + 0x13C) under the lock 0x08CF5428; self->+0x1BC set toggles, under the lock
// 0x08CF5400, bit 7 of the flags word +0x140 when bit 10 is set, bit 8 when bit 11 is set and bit 9 when bit 12 is set (the
// trigger bits cleared). Then with the buffer chosen by bit 7 (begin +0xEC / end +0xF0 of 8-byte items; n items, nothing for
// n < 2, v0 = 1) the target object's virtual slots 0x108 / 0x110 / 0x10C / 0x114 draw the items (s0 = the +0x138 range when
// its size matches n), with a scaled pass (func_089A8BF4 gives a float f, func_089A8C0C(f * f), func_089A8B20 + func_089C3DA8 with
// the pair at +0x1B0) when flag bit 4 is set, a range chosen by bit 9 (+0x11C / +0x120 / +0x150), and a second pass when bit 3
// is set; it ends as a tail jump to func_089A8C0C(target) with f12 = f.
void func_089DD648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t once_a = self + 0x1B8u;
    const std::uint32_t once_b = self + 0x1BCu;
    // The atomic "if (*word == 1) *word = 0": the old value is returned, v0 = 1 as after the successful `sc`.
    const auto take_flag = [&](std::uint32_t address) {
        const std::uint32_t old = word(rt, address);
        ctx.gpr[3] = address;
        ctx.gpr[4] = old;
        if (old == 1u) put(rt, address, 0u);
        ctx.gpr[2] = 1u;
        return old;
    };
    if (take_flag(once_a) == 1u) {
        call_set(rt, ctx, kLock, {0x08CF5428u});
        call_set(rt, ctx, 0x089AB194u, {target, self + 0x138u, self + 0x13Cu});
        call_set(rt, ctx, kUnlock, {0x08CF5428u});
    }
    const std::uint32_t second = take_flag(once_b);
    std::uint32_t flags;
    if (second == 1u) {
        call_set(rt, ctx, kLock, {0x08CF5400u});
        flags = word(rt, self + 0x140u);
        const auto toggle = [&](std::uint32_t value, std::uint32_t bit) {
            const std::uint32_t current = (value >> bit) & 1u;
            const std::uint32_t inserted = (second - current) & 1u;
            return (value & ~(1u << bit)) | (inserted << bit);
        };
        if ((flags & 0x400u) != 0u) {
            flags &= ~0x400u;
            flags = toggle(flags, 7u);
            put(rt, self + 0x140u, flags);
        }
        if ((flags & 0x800u) != 0u) {
            flags &= ~0x800u;
            flags = toggle(flags, 8u);
            put(rt, self + 0x140u, flags);
        }
        if ((flags & 0x1000u) != 0u) {
            flags &= ~0x1000u;
            put(rt, self + 0x140u, toggle(flags, 9u));
        }
        call_set(rt, ctx, kUnlock, {0x08CF5400u});
    }
    flags = word(rt, self + 0x140u);
    const std::uint32_t buffer = self + ((flags >> 7) & 1u) * 12u + 0xE0u;
    const std::uint32_t begin = word(rt, buffer + 0xCu);
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, buffer + 0x10u) - begin) >> 3);
    const std::uint32_t item_a = word(rt, self + 0xE8u);
    if (count < 2u) {
        ctx.gpr[2] = 1u;
        return;
    }
    std::uint32_t exact = 0u;
    if (word(rt, self + 0x138u) != 0u) {
        const std::uint32_t range = self + ((flags >> 8) & 1u) * 12u + 0x100u;
        const std::uint32_t first = word(rt, range + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, range + 8u) - first) >> 3);
        if (count == size) exact = first;
    }
    const float f22 = call_setf(rt, ctx, 0x089A8BF4u, {target}, {}).f0;
    flags = word(rt, self + 0x140u);
    if ((flags & 0x10u) != 0u) {
        call_setf(rt, ctx, 0x089A8C0Cu, {target}, {psp_mul(f22, f22)});
        call_virtual_at(rt, ctx, target, 0x108u, {target, self + 0x170u});
        if (exact != 0u) {
            put8(rt, target + 0x118u, 1u);
            put(rt, target + 0x11Cu, word(rt, self + 0x138u));
        }
        const float f20 = float_at(rt, self + 0x1B0u);
        const float f21 = float_at(rt, self + 0x1B4u);
        call_virtual_at(rt, ctx, target, 0x110u, {target, 0u});
        std::uint32_t holder = call_set(rt, ctx, 0x089A8B20u, {target});
        call_setf(rt, ctx, 0x089C3DA8u, {holder}, {f20, f21});
        call_virtual_at(rt, ctx, target, 0x10Cu, {target, item_a, count, begin, exact, 0u});
        holder = call_set(rt, ctx, 0x089A8B20u, {target});
        call_setf(rt, ctx, 0x089C3DA8u, {holder}, {-f20, -f21});
        call_virtual_at(rt, ctx, target, 0x114u, {target});
        if (exact != 0u) call_set(rt, ctx, 0x089A8C24u, {target});
        flags = word(rt, self + 0x140u);
    }
    const std::uint32_t range = self + ((flags >> 9) & 1u) * 12u + 0x110u;
    const std::uint32_t range_end = word(rt, range + 0x10u);
    const std::uint32_t range_begin = word(rt, range + 0xCu);
    std::uint32_t item_b = 0u;
    if (range_begin == range_end) {
        call_virtual_at(rt, ctx, target, 0x108u, {target, self + 0x150u});
    } else {
        const std::uint32_t items = static_cast<std::uint32_t>(signed_of(range_end - range_begin) >> 4);
        if (items == 1u) {
            call_virtual_at(rt, ctx, target, 0x108u, {target, range_begin});
        } else if ((count ^ items) == 0u) {
            item_b = range_begin;
        }
    }
    if (exact != 0u) {
        put8(rt, target + 0x118u, 1u);
        put8(rt, target + 0x150u, (word(rt, self + 0x140u) >> 5) & 1u);
        put8(rt, target + 0x151u, (word(rt, self + 0x140u) >> 6) & 1u);
        put(rt, target + 0x11Cu, word(rt, self + 0x138u));
    }
    call_virtual_at(rt, ctx, target, 0x110u, {target, word(rt, self + 0x144u)});
    call_virtual_at(rt, ctx, target, 0x10Cu, {target, item_a, count, begin, exact, item_b});
    if ((word(rt, self + 0x140u) & 8u) != 0u) {
        call_virtual_at(rt, ctx, target, 0x108u, {target, self + 0x190u});
        call_virtual_at(rt, ctx, target, 0x10Cu, {target, word(rt, 0x08C8CC1Cu), count, begin, 0u, 0u});
    }
    call_virtual_at(rt, ctx, target, 0x114u, {target});
    if (exact != 0u) call_set(rt, ctx, 0x089A8C24u, {target});
    ctx.fpr[12] = f22;
    tail_call(ctx, 0x089A8C0Cu, {target});
}

// func_088D00B0(self, flag): a state step of an object whose word +0x98 is 2 (else v0 = 2, nothing done): e = func_088DCD28(
// self->+0, self->+0x10) (v0 = 0 and nothing else when 0), mode m = func_088CFC48(self); state word +0x12C (1 <-> 2, 0 -> 1, when m
// is 1 and byte +0x95 is 0); func_088CFEB0(self, e); the phase word +0x9C (0: func_088CF0EC and +0x9C = 1 when m != 0) selects
// the update of +0xB8 / +0xD0 / +0xD4 from func_088CF108 / 168 / B14 / EA60 / F2CC (float f20), +0x130 counts down and +0x134
// counts up to 0x79; in state 2 the words +0xA8 / +0xB0 / +0xDC are refreshed (func_088CFA28, 088CF2CC, 088CF6D8, 088CEF98).
// Then +0xE4 eases toward func_088CEF98(self) by 1/10 (0.1 x difference), func_088CF328 when m is 1; when func_088CFCD4 says so
// the object at +8 gets virtual slot 0xA8(+8, +4, frame{halfword +0x10}, 1, 0, 1) (byte +0x94 = 0), the height record at
// +0xA0..+0xA5 is refreshed from func_088CEF68 / 088984C4 / 08897014 when *flag is set, byte +0x94 moves +0xA8 to +0xAC, and
// with byte +0x95 set the state becomes 3 (+0x130 = 0x78, +0x134 = 0, +0x9C = 1, +0xE4 = 0, func_088CF7DC, func_088CF0EC,
// +0xB0 = +0xAC).
void func_088D00B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[2] = 2u;
    if (word(rt, self + 0x98u) != 2u) return;
    const std::uint32_t e = call_set(rt, ctx, 0x088DCD28u, {word(rt, self), word(rt, self + 0x10u)});
    if (e == 0u) return;
    const std::uint32_t m = call_set(rt, ctx, 0x088CFC48u, {self});
    if (m == 1u && byte_u(rt, self + 0x95u) == 0u) {
        const std::uint32_t state = word(rt, self + 0x12Cu);
        if (state == 1u) put(rt, self + 0x12Cu, 2u);
        else if (state == 2u) put(rt, self + 0x12Cu, 1u);
        else if (state == 0u) put(rt, self + 0x12Cu, 1u);
    }
    call_set(rt, ctx, 0x088CFEB0u, {self, e});
    const std::uint32_t phase = word(rt, self + 0x9Cu);
    bool main_pass = true;
    if (phase == 0u) {
        if (m == 0u) {
            main_pass = false;
        } else {
            call_set(rt, ctx, 0x088CF0ECu, {self});
            const std::uint32_t count = word(rt, self + 0x9Cu);
            put(rt, self + 0xB0u, 0u);
            put(rt, self + 0x9Cu, count + 1u);
        }
    } else if (phase != 1u) {
        main_pass = false;
    }
    if (main_pass) {
        call_set(rt, ctx, 0x088CF108u, {self});
        std::int32_t level = 0;
        if (ctx.gpr[2] == 0u) level = 2;
        const std::int32_t second_level = signed_of(call_set(rt, ctx, 0x088CF168u, {self, e}));
        level = std::max(level, second_level);
        float f20;
        if (level == 0) {
            f20 = call_setf(rt, ctx, 0x088CFB14u, {self, e}, {}).f0;
            const std::uint32_t truncated = ctx.fpu_float_to_word(f20, 1u);
            const std::uint32_t current = word(rt, self + 0xB8u);
            put(rt, self + 0xB8u, call_set(rt, ctx, 0x088CF2CCu, {self, current, truncated}));
        } else {
            f20 = 0.0f;
            if (level == 2) {
                call_set(rt, ctx, 0x088CEA60u, {self});
                put(rt, self + 0xB8u, 0u);
                put(rt, self + 0xB4u, 0u);
                put_float(rt, self + 0xD4u, f20);
                put_float(rt, self + 0xD0u, f20);
            }
        }
        call_setf(rt, ctx, 0x088CF2E0u, {self}, {f20});
        std::uint32_t active = byte_u(rt, self + 0x95u);
        std::uint32_t amount;
        const auto count_down = [&]() { put(rt, self + 0x130u, word(rt, self + 0x130u) - 1u); };
        if (active == 0u) {
            amount = word(rt, self + 0xB8u);
            count_down();
        } else if (word(rt, self + 0x12Cu) == 1u || f20 <= 0.0f) {
            amount = word(rt, self + 0xB8u);
            count_down();
        } else {
            amount = word(rt, self + 0xB8u);
            if (signed_of(amount) <= 0) {
                count_down();
            } else if (signed_of(word(rt, self + 0x130u)) >= 0) {
                count_down();
            } else {
                put(rt, self + 0x12Cu, 1u);
            }
        }
        if (signed_of(amount) > 0 && f20 == 0.0f) {
            call_set(rt, ctx, 0x088CF9C4u, {self});
            call_set(rt, ctx, 0x088CEA60u, {self});
            active = byte_u(rt, self + 0x95u);
        }
        const std::uint32_t mode = word(rt, self + 0x12Cu);
        std::uint32_t marker = 0u;
        if (active == 0u) {
            put(rt, self + 0x134u, 0u);
        } else if (mode != 1u) {
            put(rt, self + 0x134u, 0u);
        } else {
            bool counting = signed_of(word(rt, self + 0xB8u)) > 0 || !(f20 == 0.0f);
            bool advance = true;
            if (counting) {
                if (m != mode) {
                    put(rt, self + 0x134u, 0u);
                    advance = false;
                }
            }
            if (advance) {
                const std::uint32_t ticks = word(rt, self + 0x134u) + 1u;
                put(rt, self + 0x134u, ticks);
                if (!(signed_of(ticks) < 0x79) || m == 1u) {
                    put(rt, self + 0x130u, 0x3Cu);
                    put(rt, self + 0x12Cu, 2u);
                    marker = 1u;
                    put(rt, self + 0x134u, 0u);
                }
            }
        }
        if (word(rt, self + 0x12Cu) == 2u && (m == 1u || marker != 0u)) {
            const std::uint32_t chosen = call_set(rt, ctx, 0x088CFA28u, {self});
            const std::uint32_t previous = word(rt, self + 0xA8u);
            put(rt, self + 0xA8u, call_set(rt, ctx, 0x088CF2CCu, {self, previous, chosen}));
            call_set(rt, ctx, 0x088CF6D8u, {self});
            put(rt, self + 0xB0u, call_set(rt, ctx, 0x088CFA28u, {self}));
            call_set(rt, ctx, 0x088CEF98u, {self});
            put_float(rt, self + 0xDCu, ctx.fpr[0]);
            call_set(rt, ctx, 0x088CEA60u, {self});
            if (byte_u(rt, self + 0x95u) != 0u) call_set(rt, ctx, 0x088CF0ECu, {self});
            else put(rt, self + 0x9Cu, 0u);
        }
    }
    float f20 = float_at(rt, self + 0xE4u);
    call_set(rt, ctx, 0x088CEF98u, {self});
    float difference = ctx.fpr[0] - f20;
    difference = psp_mul(difference, std::bit_cast<float>(0x3DCCCCCDu));
    f20 = f20 + difference;
    put_float(rt, self + 0xE4u, f20);
    if (m == 1u) call_set(rt, ctx, 0x088CF328u, {self});
    if (call_set(rt, ctx, 0x088CFCD4u, {self}) == 0u) return;
    const std::uint32_t id = word(rt, self + 0x10u);
    const std::uint32_t quiet = byte_u(rt, self + 0x94u);
    put16(rt, frame.at(0u), id);
    if (quiet == 0u) {
        const std::uint32_t owner = word(rt, self + 8u);
        call_virtual_at(rt, ctx, owner, 0xA8u, {owner, word(rt, self + 4u), frame.at(0u), 1u, 0u, 1u});
    }
    if (byte_u(rt, flag) != 0u) {
        const std::uint32_t measure = call_set(rt, ctx, 0x088CEF68u, {self});
        if (!(signed_of(measure) < signed_of(word(rt, self + 0xA0u)))) {
            const std::uint32_t root = word(rt, self);
            put(rt, self + 0xA0u, measure);
            const std::uint32_t table = word(rt, root + 0x14Cu);
            const std::uint32_t record = call_set(rt, ctx, 0x088984C4u, {table, word(rt, self + 0x10u), 0u});
            const std::uint32_t result = call_set(rt, ctx, 0x08897014u, {record + 0x20u});
            const std::uint32_t active = byte_u(rt, self + 0x95u);
            put8(rt, self + 0xA4u, result);
            put8(rt, self + 0xA5u, active);
        }
    }
    const std::uint32_t active = byte_u(rt, self + 0x95u);
    if (byte_u(rt, self + 0x94u) != 0u) {
        const std::uint32_t held = word(rt, self + 0xA8u);
        put(rt, self + 0xA8u, 0u);
        put(rt, self + 0xACu, held);
    }
    if ((active & 0xFFu) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, self + 0x12Cu, 3u);
    put(rt, self + 0x130u, 0x78u);
    put(rt, self + 0x134u, 0u);
    call_set(rt, ctx, 0x088CF7DCu, {self});
    put(rt, self + 0x9Cu, 1u);
    put(rt, self + 0xE4u, 0u);
    call_set(rt, ctx, 0x088CF0ECu, {self});
    const std::uint32_t carried = word(rt, self + 0xACu);
    ctx.gpr[2] = carried;
    put(rt, self + 0xB0u, carried);
}

// func_08867268(self): nothing when self->+0x5AC (owner) is 0. func_088DB4F8(); with byte +0x9C set, s4 = self + 0x45D4 and
// func_088CE914(s4) selects between (0) the cleanup of the block record K = 0x08CE2E0C (+0 pointer, +4 size): func_08B59ADC(K[0])
// and func_08B52D70(0, 0, 0x10) when K[1] is set (after func_0890B5F0(self->+0x98) when that is set), and (non-0) the work path, which
// first replaces the K block by a fresh func_08B59AC8(0x42000) one (func_08B59ADC of the old, func_08B52D70(block, 0x42000, 0x10)) unless K[1]
// is already 0x42000. The work path copies 52 bytes of func_08AF4DF4(frame + 0xF0, func_08AFAA04(), 0) after func_0890B4FC(self->+0x98) into frame
// + 0x44, takes a float (func_08ACF14C(word 0x08CFFAAC)), steps the words +0x400C / +0x4010, finds an object via the owner's
// +0x5C8's virtual slots 0xC / 8 / 0xC, builds a small record in the frame (vtables 0x08CC58A8 / 0x08CD7168 / 0x08CD7200, floats from
// 0x08CE2DF4 when byte +0x3FCC is set, func_08AFD0EC), runs func_08866B40, func_0890B92C, func_08B4E734 (x2), func_08867D0C(self, s4),
// func_088D9CCC / func_08B2DBE0 and destroys the frame objects (func_08AFD06C x4, func_08AF484C x3 with vtable 0x08CD7310).
void func_08867268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (word(rt, self + 0x5ACu) == 0u) return;
    GuestFrame frame(ctx, 0x170u);
    const std::uint32_t key = 0x08CE2E0Cu;
    std::uint32_t s4 = ctx.gpr[20];
    call_set(rt, ctx, 0x088DB4F8u, {word(rt, self + 0x5ACu)});
    bool work = true;
    if (byte_u(rt, self + 0x9Cu) != 0u) {
        s4 = self + 0x45D4u;
        work = call_set(rt, ctx, 0x088CE914u, {s4}) != 0u;
    }
    if (!work) {
        const std::uint32_t current = word(rt, self + 0x98u);
        if (current != 0u) call_set(rt, ctx, 0x0890B5F0u, {current});
        if (word(rt, key + 4u) == 0u) return;
        call_set(rt, ctx, 0x08B59ADCu, {word(rt, key)});
        put(rt, key + 4u, 0u);
        put(rt, key, 0u);
        call_set(rt, ctx, 0x08B52D70u, {0u, 0u, 0x10u});
        return;
    }
    if (word(rt, key + 4u) != 0x42000u) {
        call_set(rt, ctx, 0x08B59ADCu, {word(rt, key)});
        put(rt, key + 4u, 0u);
        put(rt, key, 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08B59AC8u, {0x42000u});
        put(rt, key + 4u, 0x42000u);
        put(rt, key, block);
        call_set(rt, ctx, 0x08B52D70u, {block, 0x42000u, 0x10u});
    }
    const std::uint32_t s5 = frame.at(0x44u);
    call_set(rt, ctx, 0x0890B4FCu, {word(rt, self + 0x98u)});
    const std::uint32_t seed = call_set(rt, ctx, 0x08AFAA04u, {});
    call_set(rt, ctx, 0x08AF4DF4u, {frame.at(0xF0u), seed, 0u});
    copy_words(rt, s5, frame.at(0xF0u), 13u);
    const float f23 = call_setf(rt, ctx, 0x08ACF14Cu, {word(rt, 0x08CFFAACu)}, {}).f0;
    const std::uint32_t counter = word(rt, self + 0x400Cu);
    const std::uint32_t extra = byte_u(rt, self + 0x3FCCu);
    if (signed_of(counter) <= 0) {
        put8(rt, frame.at(0u), 0u);
        call_set(rt, ctx, 0x08B8E4E0u, {self + 0x4014u + counter, self + 0x4015u, frame.at(0u)});
    }
    const std::int32_t stepped = signed_of(word(rt, self + 0x4010u)) + 1;
    put(rt, self + 0x4010u, static_cast<std::uint32_t>(std::min(stepped, 0)));
    put(rt, self + 0x400Cu, 1u);
    put(rt, word(rt, self + 0x98u) + 0xE0u, 0u);
    if (extra != 0u) {
        const std::uint32_t current = word(rt, self + 0x98u);
        call_setf(rt, ctx, 0x08AF26C0u, {word(rt, current)}, {float_at(rt, 0x08CE2E00u)});
    }
    const std::uint32_t owner = word(rt, word(rt, self + 0x5ACu) + 0x5C8u);
    ctx.gpr[3] = word(rt, owner + 0x1Cu);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    std::uint32_t found = call_set(rt, ctx, ctx.gpr[2], {owner + 0x1Cu, 0u, 0u});
    if (found == 0u) return;
    const std::uint32_t object = found;
    if (call_virtual_at(rt, ctx, object, 8u, {object}) == 0u) return;
    found = call_virtual_at(rt, ctx, object, 0xCu, {object, 0u});
    if (found == 0u) return;
    const std::uint32_t item = found;
    put(rt, frame.at(0xCu), 0x08CC58A8u);
    put(rt, frame.at(0x28u), 0x08CD7168u);
    put_float(rt, frame.at(0x3Cu), 52.0f);
    put(rt, frame.at(0x10u), item);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0x30u), 0u);
    put(rt, frame.at(0x34u), 0u);
    const float one = 1.0f;
    put_float(rt, frame.at(0x38u), one);
    put(rt, frame.at(0x40u), 0u);
    float f22;
    float f20;
    float f21;
    float f24;
    float f2;
    if (extra != 0u) {
        f2 = float_at(rt, 0x08CE2E04u);
        f24 = float_at(rt, 0x08CE2DF4u);
        f22 = float_at(rt, 0x08CE2DF8u);
        f21 = float_at(rt, 0x08CE2DFCu);
        f20 = float_at(rt, 0x08CE2E00u);
    } else {
        f22 = 0.0f;
        f20 = one;
        f21 = one;
        f24 = f22;
        f2 = one;
    }
    float f0 = psp_mul(f21, f23);
    f0 = f0 / f20;
    const std::uint32_t temp_a = frame.at(0x14u);
    put_float(rt, frame.at(0x24u), f2);
    std::uint32_t s2 = 0u;
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, temp_a, 0x08CD7200u);
    put_float(rt, frame.at(0x20u), f0);
    if (!(f23 == 0.0f)) {
        ctx.gpr[2] = word(rt, 0x08CD7200u + 0x2Cu);
        call_set(rt, ctx, ctx.gpr[2], {temp_a, 0u});
        s2 = temp_a;
    }
    ctx.fpr[16] = one;
    ctx.fpr[17] = one;
    const std::uint32_t third = word(rt, frame.at(0x48u));
    const std::uint32_t second = word(rt, frame.at(0x44u));
    call_setf(rt, ctx, 0x08AFD0ECu, {frame.at(0x78u), second, third, 0u, 0u}, {f24, f22, f21, f20});
    const std::uint32_t temp_b = frame.at(0xCu);
    put(rt, frame.at(4u), item);
    put(rt, frame.at(8u), temp_b);
    call_set(rt, ctx, 0x08866B40u, {self, frame.at(4u)});
    call_set(rt, ctx, 0x0890B92Cu, {word(rt, self + 0x98u), s5, s2, 0u});
    put(rt, word(rt, self + 0x98u) + 0x30u, 0u);
    call_set(rt, ctx, 0x08B4E734u, {self + 0x4290u, word(rt, self + 0x98u) + 0x60u});
    call_set(rt, ctx, 0x08B4E734u, {self + 0x4390u, word(rt, self + 0x98u) + 0xA0u});
    call_set(rt, ctx, 0x08867D0Cu, {self, s4});
    if (call_set(rt, ctx, 0x088D9CCCu, {word(rt, self + 0x5ACu)}) == 0u) call_set(rt, ctx, 0x08B2DBE0u, {});
    call_set(rt, ctx, 0x08AFD06Cu, {frame.at(0xD0u)});
    call_set(rt, ctx, 0x08AFD06Cu, {frame.at(0xB4u)});
    call_set(rt, ctx, 0x08AFD06Cu, {frame.at(0x98u)});
    call_set(rt, ctx, 0x08AFD06Cu, {frame.at(0x7Cu)});
    const std::uint32_t vtable = 0x08CD7310u;
    put(rt, temp_a, vtable);
    call_set(rt, ctx, 0x08AF484Cu, {temp_a});
    put(rt, frame.at(0x28u), vtable);
    call_set(rt, ctx, 0x08AF484Cu, {frame.at(0x28u)});
    put(rt, temp_b, vtable);
    call_set(rt, ctx, 0x08AF484Cu, {temp_b});
}

// func_088C3D5C(p, q, tag, quiet, out): a leaf over two 0x74-byte float records. With d = q - p at +0x28..+0x30, the axes of p at
// +0x34 / +0x40 and the pairs p/q +0x4C..+0x60 it computes the projections f8 / f7 of d, the clearances f6 / f5 (each with the
// opposite box edge) and the dot f1 of the +0x64 offsets, then f3 (f5 shifted by f1 when f6 > -0.1). When f3 < 0.3 and f6 > -0.1
// it keeps the smaller f6 at out->+0x58 / +0x5C (by the sign of f8) and writes the byte out->+0x4A / +0x4B = trunc(-100 f5) when
// f5 < 0 (0xFA beyond 250). Otherwise (f7 >= 0, f6 <= 0.2) it keeps the smaller f5 at out->+0x60 and sets out->+0x44 (and the
// byte +0x51 = tag unless -1) to a limit chosen from f5, f1, f8 (0.08 / 0.02 / 1.0 / 0.5 / 0.1 / 0.05 ...) when it is larger.
// v0 is the leftover of the last step (q + 0x64, the compare of the truncation, or -1).
void func_088C3D5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4];
    const std::uint32_t q = ctx.gpr[5];
    const std::uint32_t tag = ctx.gpr[6];
    const std::uint32_t quiet = ctx.gpr[7] & 0xFFu;
    const std::uint32_t out = ctx.gpr[8];
    float f0 = float_at(rt, p + 0x28u);
    float f5 = float_at(rt, q + 0x2Cu);
    float f1 = float_at(rt, p + 0x2Cu);
    float f3 = float_at(rt, q + 0x28u);
    f5 = f5 - f1;
    f3 = f3 - f0;
    float f4 = float_at(rt, q + 0x30u);
    f1 = float_at(rt, p + 0x30u);
    float f2 = float_at(rt, p + 0x34u);
    f0 = float_at(rt, p + 0x38u);
    f4 = f4 - f1;
    f2 = psp_mul(f3, f2);
    f0 = psp_mul(f5, f0);
    f1 = float_at(rt, p + 0x3Cu);
    const float f10 = float_at(rt, p + 0x40u);
    f2 = f2 + f0;
    f1 = psp_mul(f4, f1);
    const float f9 = float_at(rt, p + 0x44u);
    f3 = psp_mul(f3, f10);
    const float f11 = float_at(rt, p + 0x48u);
    const float f8 = f2 + f1;
    f5 = psp_mul(f5, f9);
    f4 = psp_mul(f4, f11);
    f3 = f3 + f5;
    const float f7 = f3 + f4;
    float f6;
    if (0.0f < f8) {
        f0 = float_at(rt, p + 0x4Cu) + float_at(rt, q + 0x58u);
        f6 = f8 - f0;
    } else {
        f0 = float_at(rt, p + 0x58u);
        f1 = float_at(rt, q + 0x4Cu);
        f2 = -f8;
        f0 = f0 + f1;
        f6 = f2 - f0;
    }
    if (0.0f < f7) {
        f0 = float_at(rt, p + 0x54u) + float_at(rt, q + 0x60u);
        f5 = f7 - f0;
    } else {
        f0 = float_at(rt, p + 0x60u);
        f1 = float_at(rt, q + 0x54u);
        f2 = -f7;
        f0 = f0 + f1;
        f5 = f2 - f0;
    }
    ctx.gpr[2] = q + 0x64u;
    f3 = float_at(rt, p + 0x64u);
    f4 = float_at(rt, p + 0x68u);
    f0 = float_at(rt, q + 0x64u);
    f2 = float_at(rt, q + 0x68u);
    f1 = float_at(rt, q + 0x6Cu);
    f0 = f0 - f3;
    f2 = f2 - f4;
    f3 = float_at(rt, p + 0x6Cu);
    f4 = std::bit_cast<float>(0xBDCCCCCDu);
    f0 = psp_mul(f10, f0);
    f2 = psp_mul(f9, f2);
    f1 = f1 - f3;
    const bool beyond = f4 < f6;
    f0 = f0 + f2;
    f1 = psp_mul(f11, f1);
    f1 = f0 + f1;
    if (beyond) {
        f0 = 0.0f;
        if (0.0f < f7) {
            if (f1 < f0) f3 = f5 + f1;
            else f3 = f5;
        } else {
            f0 = psp_mul(f1, f4);
            if (0.0f < f1) f3 = f5 + f0;
            else f3 = f5;
        }
    } else {
        f3 = f5;
    }
    if (f3 < std::bit_cast<float>(0x3E99999Au) && std::bit_cast<float>(0xBDCCCCCDu) < f6) {
        const std::uint32_t slot = 0.0f < f8 ? 0x58u : 0x5Cu;
        if (!(f6 < float_at(rt, out + slot))) return;
        put_float(rt, out + slot, f6);
        std::uint32_t value = 0u;
        if (f5 < 0.0f) {
            f0 = psp_mul(f5, std::bit_cast<float>(0xC2C80000u));
            value = 0xFAu;
            const std::uint32_t truncated = ctx.fpu_float_to_word(f0, 1u);
            const bool fits = truncated < 0xFBu;
            ctx.gpr[2] = static_cast<std::uint32_t>(fits);
            if (fits) value = truncated & 0xFFu;
        }
        put8(rt, out + (slot == 0x58u ? 0x4Au : 0x4Bu), value);
        return;
    }
    f0 = 0.0f;
    if (f7 < f0) return;
    if (std::bit_cast<float>(0x3E4CCCCDu) < f6) return;
    f0 = float_at(rt, out + 0x60u);
    if (f5 < f0) put_float(rt, out + 0x60u, f5);
    const float half = 0.5f;
    f4 = 0.0f;
    bool settle = false;
    if (f5 < half && f8 < half) {
        f3 = 1.0f;
        if (-0.5f < f8) settle = true;
    }
    if (!settle) {
        if (f4 < f1) {
            f1 = std::bit_cast<float>(0x3CA3D70Au);
            if (f5 < half) {
                f1 = std::bit_cast<float>(0x3DA3D70Au);
            } else if (!(f5 < 4.0f)) {
                f1 = f4;
            }
            f0 = float_at(rt, out + 0x44u);
            ctx.gpr[2] = 0xFFFFFFFFu;
            if (!(f0 < f1)) return;
            if (tag != 0xFFFFFFFFu) put8(rt, out + 0x51u, tag);
            put_float(rt, out + 0x44u, f1);
            return;
        }
        f2 = std::bit_cast<float>(0x3D4CCCCDu);
        if (f5 < f2) f5 = f2;
        f1 = -f1;
        f3 = f5 / f1;
        if (std::bit_cast<float>(0x40A00000u) < f3) {
            f3 = std::bit_cast<float>(0x3DCCCCCDu);
            if (!(f5 < half)) {
                f3 = f2;
                if (!(f5 < 4.0f)) f3 = f4;
            }
        } else {
            const float ratio = f3;
            f3 = 1.0f;
            if (!(ratio < 1.0f) && !(std::bit_cast<float>(0x40B1C71Du) < f1)) f3 = 0.5f;
        }
    }
    if (quiet != 0u) return;
    f0 = float_at(rt, out + 0x44u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (!(f0 < f3)) return;
    if (tag != 0xFFFFFFFFu) put8(rt, out + 0x51u, tag);
    put_float(rt, out + 0x44u, f3);
}

// func_08A189C0(self, other): copies the string self->+4 twice and compares it with two literals (func_08B85EB8 builds each
// literal, func_08B36170(a, b, min length) compares, ties broken by the length difference; the strings are released through
// the empty-rep check / func_08B60804 / func_08B8542C). When the first literal (0x08CA9C48) equals it: nothing more (v0 left
// by the releases). When the second (0x08CA9C4C) equals it too: a value string is built on other's stream (func_08987A74(other,
// frame) twice; on 0x102 func_08AA4748(frame + 0x34, other + 4), func_08BEED1C with 0x08CA9C58 / 0x08CA9C5C, func_08B89054,
// func_089C9CE4(frame, self->+0xC) and the flag 0x10 set in that object's word +0x60, func_0898682C(other, 1)). When they differ: func_08987A74
// (other, frame) and a keyed lookup func_089C9C34 / func_089C719C whose result goes to self->+0xC (func_08A5BAD8), then the
// object's virtual slot 0x128 and a loop of func_08987A74 until -1 (0x7D ends it) that offers each item to the object at
// +0x38 (slot 8) and calls func_0898682C(other, 0) when it refuses.
void func_08A189C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t source = self + 4u;
    const std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t release = 0x08B8542Cu;
    // The string compare of the text at `first_slot` with the one at `second_slot`: the lengths are spilled to the frame, then
    // func_08B36170(first, second, min length); a tie gives the difference.
    const auto compare = [&](std::uint32_t first_slot, std::uint32_t second_slot, bool first_pair) {
        const std::uint32_t first = word(rt, first_slot);
        const std::uint32_t second = word(rt, second_slot);
        const std::uint32_t first_length = word(rt, first - 0xCu);
        const std::uint32_t second_length = word(rt, second - 0xCu);
        const std::uint32_t lower = first_pair ? frame.at(0x28u) : frame.at(0x2Cu);
        const std::uint32_t upper = first_pair ? frame.at(0x2Cu) : frame.at(0x28u);
        put(rt, upper, first_length);
        put(rt, lower, second_length);
        const bool shorter = second_length < first_length;
        const std::uint32_t chosen = shorter ? lower : upper;
        const std::uint32_t result = call_set(rt, ctx, 0x08B36170u, {first, second, word(rt, chosen)});
        return result != 0u ? result : first_length - second_length;
    };
    call_set(rt, ctx, 0x08B88F44u, {frame.at(0x24u), source});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x10u), 0x08CA9C48u, frame.at(9u)});
    std::uint32_t difference = compare(frame.at(0x24u), frame.at(0x10u), true);
    release_string(rt, ctx, frame.at(0x10u), empty, release, frame.at(8u));
    release_string(rt, ctx, frame.at(0x24u), empty, release, frame.at(7u));
    if (difference == 0u) return;
    call_set(rt, ctx, 0x08B88F44u, {frame.at(0x20u), source});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0xCu), 0x08CA9C4Cu, frame.at(6u)});
    difference = compare(frame.at(0x20u), frame.at(0xCu), false);
    release_string(rt, ctx, frame.at(0xCu), empty, release, frame.at(5u));
    release_string(rt, ctx, frame.at(0x20u), empty, release, frame.at(4u));
    const std::uint32_t pair = frame.at(0x34u);
    const std::uint32_t text = frame.at(0x30u);
    if (difference == 0u) {
        put(rt, text, empty + 0xCu);
        call_set(rt, ctx, 0x08987A74u, {other, text});
        if (call_set(rt, ctx, 0x08987A74u, {other, text}) == 0x102u) {
            call_set(rt, ctx, 0x08AA4748u, {pair, other + 4u});
            call_set(rt, ctx, 0x08BEED1Cu, {pair, 0x08CA9C58u, 1u});
            call_set(rt, ctx, 0x08B89054u, {pair, text});
            call_set(rt, ctx, 0x08BEED1Cu, {pair, 0x08CA9C5Cu, 8u});
            if (call_set(rt, ctx, 0x089C9CE4u, {pair, word(rt, self + 0xCu)}) != 0u) {
                const std::uint32_t object = word(rt, word(rt, self + 0xCu));
                if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9C68u});
                put(rt, object + 0x60u, word(rt, object + 0x60u) | 0x10u);
            }
            call_set(rt, ctx, 0x0898682Cu, {other, 1u});
            release_string(rt, ctx, pair, empty, release, frame.at(3u));
        }
        release_string(rt, ctx, text, empty, release, frame.at(2u));
        return;
    }
    put(rt, pair, empty + 0xCu);
    call_set(rt, ctx, 0x08987A74u, {other, pair});
    const std::uint32_t target = word(rt, self + 0xCu);
    call_set(rt, ctx, 0x089C9C34u, {frame.at(0x18u)});
    std::uint32_t key = word(rt, frame.at(0x18u));
    if (key == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9C70u});
    call_set(rt, ctx, 0x08B88F44u, {frame.at(0x14u), source});
    call_set(rt, ctx, 0x089C719Cu, {frame.at(0x1Cu), key, frame.at(0x14u)});
    if (target != frame.at(0x1Cu)) call_set(rt, ctx, kAssign, {target, word(rt, frame.at(0x1Cu))});
    call_set(rt, ctx, kDestroy, {frame.at(0x1Cu)});
    release_string(rt, ctx, frame.at(0x14u), empty, release, frame.at(1u));
    call_set(rt, ctx, kDestroy, {frame.at(0x18u)});
    const std::uint32_t object = word(rt, word(rt, self + 0xCu));
    if (object != 0u) {
        call_virtual_at(rt, ctx, object, 0x128u, {object});
        for (;;) {
            const std::uint32_t code = call_set(rt, ctx, 0x08987A74u, {other, pair});
            if (code == 0xFFFFFFFFu) break;
            ctx.gpr[4] = 0x08CA9C68u;
            if (code == 0x7Du) break;
            std::uint32_t holder = word(rt, word(rt, self + 0xCu));
            if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {});
            ctx.gpr[3] = word(rt, holder + 0x38u);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 8u);
            if (call_set(rt, ctx, ctx.gpr[2], {holder + 0x38u, other, pair}) == 0u) call_set(rt, ctx, 0x0898682Cu, {other, 0u});
        }
    }
    release_string(rt, ctx, pair, empty, release, frame.at(0u));
}

// func_08BD25E4(result, a, b, c, shared_ptr): builds a 0xA4-byte object (func_08B62344 + func_08AC0640; vtable 0x08CC53A0, words
// set to 0 / a / 0x08BD0C84, two func_08B7BB6C calls with b and c) and its 0x14-byte control block (vtable 0x08CC53E0, counts 1,
// object, the owner s3 = *shared_ptr), stores {object, control} to result[0..1] under the owner's lock (func_08B34888 /
// func_08B34B24, use count +0x2C counted up for each of the four holders) and finishes with func_08BCF958(result + 4, object +
// 0x20, object). Each lock section drops one use: when the count reaches 0 the owner's weak state (+0x34 / +0x38) is
// released (virtual slots 8 and 0), func_08B34C1C if byte +0x30 is set. Returns result.
void func_08BD25E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    const std::uint32_t third = ctx.gpr[7];
    const std::uint32_t owner = word(rt, ctx.gpr[8]);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t pair = result + 4u;
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0xA4u});
    call_set(rt, ctx, 0x08AC0640u, {object});
    const std::uint32_t inner = object + 0x20u;
    put(rt, object + 0x20u, 0u);
    put(rt, inner + 4u, 0u);
    put(rt, object, 0x08CC53A0u);
    put(rt, object + 0x30u, 0x08BD0C84u);
    put(rt, object + 0x38u, first);
    for (const std::uint32_t offset : {0x28u, 0x2Cu, 0x34u, 0x80u, 0x84u, 0x88u, 0x8Cu, 0x94u, 0x98u, 0x9Cu, 0xA0u}) put(rt, object + offset, 0u);
    call_set(rt, ctx, 0x08B7BB6Cu, {object + 0x3Cu, second});
    call_set(rt, ctx, 0x08B7BB6Cu, {object + 0x5Cu, third});
    lock_count_use(rt, ctx, owner);
    put(rt, result, object);
    call_set(rt, ctx, 0x08B34888u, {owner});
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) + 1u);
    call_set(rt, ctx, 0x08B34B24u, {owner});
    lock_count_use(rt, ctx, owner);
    const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x14u});
    put(rt, control + 4u, 1u);
    put(rt, control + 8u, 1u);
    put(rt, control, 0x08CC53E0u);
    put(rt, control + 0xCu, object);
    put(rt, control + 0x10u, owner);
    call_set(rt, ctx, 0x08B34888u, {owner});
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) + 1u);
    call_set(rt, ctx, 0x08B34B24u, {owner});
    put(rt, pair, control);
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    if (word(rt, pair) == 0u) call_virtual_at(rt, ctx, object, 4u, {object});
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    call_set(rt, ctx, 0x08BCF958u, {pair, inner, object});
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    ctx.gpr[2] = result;
}

// func_08BC728C(result, holder, key, item, tag): with the owner O = *holder (result = {0, 0} when null) locked (func_08B34888(holder->+0x6C))
// the list at O + 0x98 (nodes: next +0, string text +8) is searched for a node whose string equals the key text *key
// (length, func_08B36170 over the shorter, then the lengths). Not found: result {0, 0}. Found with item = 0: func_08BBFE68(list,
// node), result {0, 0}. Otherwise func_08B2CB18(O + 8, item, tag) = r under O + 0x6C's lock; a 0x18-byte control record
// {vtable 0x08CC52B8, 1, 1, r, O, shared} is made (func_08B62344) and shared (func_08B8E380(frame + 0xC, r, r)); when that failed
// the entry is erased (func_08B2CE7C, func_08B2D1A8, func_08B34CA0, func_08BBEA18); node->+0xC = r, the shared pointer node->+0x10 is replaced
// and result = {r, shared} (use counts stepped along). Ends with func_08BBCFA0(holder) and v0 = result.
void func_08BC728C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    const std::uint32_t item = ctx.gpr[7];
    const std::uint32_t tag = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10u), holder);
    const std::uint32_t first = word(rt, holder);
    if (first == 0u) {
        put(rt, result, 0u);
        put(rt, result + 4u, 0u);
        ctx.gpr[2] = result;
        return;
    }
    call_set(rt, ctx, 0x08B34888u, {first + 0x6Cu});
    const std::uint32_t owner = word(rt, word(rt, frame.at(0x10u)));
    const std::uint32_t head = owner + 0x98u;
    std::uint32_t node = word(rt, head);
    bool found = false;
    if (node != head) {
        const std::uint32_t text = word(rt, key);
        for (;;) {
            const std::uint32_t node_length = word(rt, word(rt, node + 8u) - 0xCu);
            put(rt, frame.at(0u), node_length);
            const std::uint32_t text_length = word(rt, text - 0xCu);
            put(rt, frame.at(4u), text_length);
            const std::uint32_t chosen = text_length < node_length ? frame.at(4u) : frame.at(0u);
            const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {word(rt, node + 8u), text, word(rt, chosen)});
            if (order == 0u && node_length == text_length) {
                found = true;
                break;
            }
            node = word(rt, node);
            if (node == head) break;
        }
    }
    if (!found) node = head;
    ctx.gpr[4] = head;
    if (node == head) {
        put(rt, result, 0u);
        put(rt, result + 4u, 0u);
    } else if (item == 0u) {
        call_set(rt, ctx, 0x08BBFE68u, {head, node});
        put(rt, result, 0u);
        put(rt, result + 4u, 0u);
    } else {
        const std::uint32_t lock = owner + 0x6Cu;
        call_set(rt, ctx, 0x08B34888u, {lock});
        const std::uint32_t made = call_set(rt, ctx, 0x08B2CB18u, {owner + 8u, item, tag});
        call_set(rt, ctx, 0x08B34B24u, {lock});
        const std::uint32_t outer = word(rt, frame.at(0x10u));
        const std::uint32_t shared = word(rt, outer + 4u);
        const std::uint32_t object = word(rt, outer);
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
        put(rt, frame.at(8u), made);
        if (shared != 0u) {
            count_up(rt, ctx, shared + 4u);
            count_up(rt, ctx, shared + 4u);
        }
        const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, control + 4u, 1u);
        put(rt, control, 0x08CC52B8u);
        put(rt, control + 8u, 1u);
        put(rt, control + 0xCu, made);
        put(rt, control + 0x10u, object);
        put(rt, control + 0x14u, shared);
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
        put(rt, frame.at(0xCu), control);
        if (shared != 0u) release_shared(rt, ctx, shared);
        if (word(rt, frame.at(0xCu)) == 0u) {
            const std::uint32_t inner_lock = object + 0x6Cu;
            call_set(rt, ctx, 0x08B34888u, {inner_lock});
            const std::uint32_t table = object + 8u;
            call_set(rt, ctx, 0x08B2CE7Cu, {table, made});
            const std::uint32_t count = call_set(rt, ctx, 0x08B2D1A8u, {table});
            if (count == word(rt, object + 0x68u)) call_set(rt, ctx, 0x08B34CA0u, {inner_lock});
            call_set(rt, ctx, 0x08B34B24u, {inner_lock});
            call_set(rt, ctx, 0x08BBEA18u, {object + 0x98u, table});
        }
        ctx.gpr[5] = made;
        if (shared != 0u) release_shared(rt, ctx, shared);
        call_set(rt, ctx, 0x08B8E380u, {frame.at(0xCu), made, made});
        if (shared != 0u) release_shared(rt, ctx, shared);
        put(rt, node + 0xCu, word(rt, frame.at(8u)));
        std::uint32_t current = word(rt, frame.at(0xCu));
        const std::uint32_t slot = node + 0x10u;
        if (current != 0u) count_up(rt, ctx, current + 8u);
        const std::uint32_t previous = word(rt, slot);
        if (previous != 0u && count_down(rt, ctx, previous + 8u)) call_virtual_at(rt, ctx, previous, 0xCu, {previous});
        put(rt, slot, current);
        const std::uint32_t value = word(rt, frame.at(8u));
        current = word(rt, frame.at(0xCu));
        put(rt, result, value);
        put(rt, result + 4u, current);
        if (current != 0u) {
            count_up(rt, ctx, current + 4u);
            current = word(rt, frame.at(0xCu));
            if (current != 0u) release_shared(rt, ctx, current);
        }
    }
    call_set(rt, ctx, 0x08BBCFA0u, {word(rt, frame.at(0x10u))});
    ctx.gpr[2] = result;
}

// func_0895D7CC(self): two passes (index i = 0, 1) over the pairs of slots s1 = 2i / s3 = s1 ^ 1 (func_08955300(self, slot) gives
// the 0x9C-float records r1 / r2): f20 = r(s1)->+0x34 - r(s3)->+0x34 and the mode byte m = (self->+0xC + 0x270 + i)->+0x14. Mode 5
// moves both records' +0x9C by min(f20^2, rec->+0x1C) x rec->+0x24 in the sign direction of f20; modes 3 / 4 use the clamped
// push f2 from word +0x604 (and clear the flag byte +0x60C when the two +0x9C differ by more than 2 f2, else fall to the mode 1 path);
// mode 0 limits each record's +0x34 against its +0x9C x +0xD0 x f22 (f22 = func_0895549C(self)); mode 1 writes +0x5FC x +0xA8-table
// to both records' +0x34. Other modes (and the common tail) blend +0x34 as f21 = f22 x (r1->+0x9C - r2->+0x9C) x +0xD0 x 0.5 + f20 x 0.5
// (flag byte set for modes 3 / 4 when the product goes negative) and write +0x34 = tail +/- f21. Ends with v0 = 2.
void func_0895D7CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const float half = 0.5f;
    const std::uint32_t base = word(rt, self + 0xCu) + 0x270u;
    std::uint32_t flags = word(rt, self + 8u);
    std::uint32_t words = flags;
    const float f22 = call_setf(rt, ctx, 0x0895549Cu, {self}, {}).f0;
    std::uint32_t record = base;
    std::uint32_t scale_base = base;
    // The table pick shared by the mode 1 path and the tail: word +0xA8 of the base record, or of the entry its byte +2 selects when byte +4 is set.
    const auto table_value = [&](std::uint32_t slot_base) {
        float value = float_at(rt, scale_base + 0xA8u);
        if (byte_u(rt, slot_base + 4u) != 0u) value = float_at(rt, slot_base + byte_u(rt, slot_base + 2u) * 4u + 0xA8u);
        return value;
    };
    for (std::uint32_t index = 0u; index != 2u; ++index) {
        const std::uint32_t mode = byte_u(rt, record + 0x14u);
        const std::uint32_t s1 = index * 2u;
        std::uint32_t s3 = s1 ^ 1u;
        if ((s1 ^ 0x7Eu) == 0u) s3 = s1;
        const std::uint32_t first = call_set(rt, ctx, 0x08955300u, {self, s1});
        const std::uint32_t second = call_set(rt, ctx, 0x08955300u, {self, s3});
        const std::uint32_t third = call_set(rt, ctx, 0x08955300u, {self, s1});
        float f20 = float_at(rt, third + 0x34u);
        const std::uint32_t fourth = call_set(rt, ctx, 0x08955300u, {self, s3});
        f20 = f20 - float_at(rt, fourth + 0x34u);
        const std::uint32_t s0 = first;
        const std::uint32_t s5 = second;
        enum class Go { Common, Table, ZeroLoop, Done } go = Go::Common;
        bool reached_da24 = true;
        if (mode >= 3u && mode < 5u) {
            float f1 = float_at(rt, words + 0x604u);
            float f2;
            if (0.0f <= f1) {
                f2 = psp_mul(f1, float_at(rt, scale_base + 0x24u));
            } else {
                f1 = -f1;
                f2 = psp_mul(float_at(rt, scale_base + 0x2Cu), f1);
            }
            const float limit = float_at(rt, scale_base + 0x1Cu);
            if (f2 < limit) f2 = limit;
            if (byte_u(rt, flags + 0x60Cu) != 0u) {
                const float gap = float_at(rt, s0 + 0x9Cu) - float_at(rt, s5 + 0x9Cu);
                const float f3 = f2 + f2;
                if (f3 < gap || gap < -f3) {
                    put8(rt, flags + 0x60Cu, 0u);
                } else {
                    go = Go::Table;
                    reached_da24 = false;
                }
            }
            if (reached_da24) {
                if (0.0f < f20) {
                    put_float(rt, s0 + 0x9Cu, float_at(rt, s0 + 0x9Cu) - f2);
                    put_float(rt, s5 + 0x9Cu, f2 + float_at(rt, s5 + 0x9Cu));
                } else if (f20 < 0.0f) {
                    put_float(rt, s0 + 0x9Cu, f2 + float_at(rt, s0 + 0x9Cu));
                    put_float(rt, s5 + 0x9Cu, float_at(rt, s5 + 0x9Cu) - f2);
                }
            }
        } else if (mode == 5u) {
            float f1 = psp_mul(f20, f20);
            const float limit = float_at(rt, scale_base + 0x1Cu);
            if (limit < f1) f1 = limit;
            f1 = psp_mul(f1, float_at(rt, scale_base + 0x24u));
            if (0.0f < f20) {
                put_float(rt, s0 + 0x9Cu, float_at(rt, s0 + 0x9Cu) - f1);
                put_float(rt, s5 + 0x9Cu, f1 + float_at(rt, s5 + 0x9Cu));
            } else if (f20 < 0.0f) {
                put_float(rt, s0 + 0x9Cu, f1 + float_at(rt, s0 + 0x9Cu));
                put_float(rt, s5 + 0x9Cu, float_at(rt, s5 + 0x9Cu) - f1);
            }
            reached_da24 = false;
        }
        if (reached_da24) {
            if (mode == 0u) go = Go::ZeroLoop;
            else if (mode == 1u) go = Go::Table;
        }
        if (go == Go::ZeroLoop) {
            for (std::uint32_t pass = 0u; pass < 2u; ++pass) {
                const std::uint32_t target = call_set(rt, ctx, 0x08955300u, {self, pass == 0u ? s1 : s3});
                const std::uint32_t rec = pass == 0u ? first : second;
                const float f2 = float_at(rt, scale_base + 0xD0u);
                const float f4 = 0.0f;
                float f0 = float_at(rt, rec + 0x9Cu);
                const float f1 = float_at(rt, rec + 0x90u);
                const float f3 = float_at(rt, target + 0x34u);
                f0 = psp_mul(f0, f2);
                f0 = psp_mul(f22, f0);
                f0 = f0 + f3;
                if (!(f1 == f4)) {
                    if (f4 < f3) {
                        if (f0 < f4) f0 = f4;
                    } else if (f3 < f4) {
                        if (f4 < f0) f0 = f4;
                    } else {
                        f0 = f4;
                    }
                }
                put_float(rt, target + 0x34u, f0);
            }
            go = Go::Done;
        }
        if (go == Go::Table) {
            const float f20b = table_value(base);
            const std::uint32_t one = call_set(rt, ctx, 0x08955300u, {self, s1});
            const std::uint32_t two = call_set(rt, ctx, 0x08955300u, {self, s3});
            const float result = psp_mul(f20b, float_at(rt, words + 0x5FCu));
            put_float(rt, two + 0x34u, result);
            put_float(rt, one + 0x34u, result);
        } else if (go == Go::Common) {
            const float f3 = float_at(rt, s5 + 0x9Cu);
            float f0 = float_at(rt, s0 + 0x9Cu);
            const float f1 = float_at(rt, scale_base + 0xD0u);
            const float f2 = psp_mul(f20, half);
            f0 = f0 - f3;
            f0 = psp_mul(f0, f1);
            f0 = psp_mul(f0, half);
            f0 = psp_mul(f22, f0);
            float f21 = f0 + f2;
            const float f1b = psp_mul(f20, f21);
            if (f1b < 0.0f) {
                f21 = 0.0f;
                if (((mode - 3u) & 0xFFFFFFFFu) < 2u) put8(rt, flags + 0x60Cu, 1u);
            }
            const float f0b = table_value(base);
            float f20c = float_at(rt, words + 0x5FCu);
            f20c = psp_mul(f0b, f20c);
            const std::uint32_t one = call_set(rt, ctx, 0x08955300u, {self, s1});
            const float f0c = f21 + f20c;
            f20c = f20c - f21;
            put_float(rt, one + 0x34u, f0c);
            const std::uint32_t two = call_set(rt, ctx, 0x08955300u, {self, s3});
            put_float(rt, two + 0x34u, f20c);
        }
        ++record;
        words += 4u;
        flags += 1u;
        scale_base += 4u;
    }
    ctx.gpr[2] = 2u;
}

// func_0893BE1C(self, flag_out) -> float f22 (f0): with e = self->+8 and the four records r(i) = func_08955300(self, i): the first two
// that have +0x54 != 0 add +0x88 / +0x8C / +0x3C^2 into sums (n counted); the first sum scaled by (mean square + 0.1) and made
// positive, bounded sums give f24; for each record whose byte +0x10C is 0 func_0893A6BC(self, i) feeds a limited level into
// e->+0x744 (byte +0x74A = 0x12 when raised); func_0893AEF8(self); then a target level f22 (0 or e->+0x744 + 0.25), the per-record
// sums f20 (from +0xE0, +0x40 and the e->+0xD flag) and f21 (records 0..1 whose byte +0xE9 is 1 / 0xA / 0xB / 0x15) give the
// clamped blend f1; *flag_out = 1 when f21 is positive; e->+0x738 / +0x73C are stepped toward f1 by a spring (x1000, -50, 0.0167...).
void func_0893BE1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag_out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    float f20 = 0.0f;
    float f21 = 0.0f;
    float f22 = 0.0f;
    const float f23 = 0.0f;
    std::uint32_t used = 0u;
    const std::uint32_t entry = word(rt, self + 8u);
    for (std::uint32_t index = 0u; index != 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        if (index >= 2u) continue;
        if (float_at(rt, record + 0x54u) == f23) continue;
        float f0 = float_at(rt, record + 0x3Cu);
        const float f2 = float_at(rt, record + 0x8Cu);
        const float f1 = float_at(rt, record + 0x88u);
        f0 = psp_mul(f0, f0);
        f21 = f21 + f2;
        f20 = f20 + f1;
        ++used;
        f22 = f22 + f0;
    }
    const float zero = 0.0f;
    if (used != 0u) {
        float f0 = static_cast<float>(signed_of(used));
        f0 = f22 / f0;
        f0 = f0 + std::bit_cast<float>(0x3DCCCCCDu);
        f20 = psp_mul(f20, f0);
    }
    if (f20 < zero) f20 = -f20;
    float f1 = psp_mul(f20, std::bit_cast<float>(0x3A2A7EFAu));
    const float third = std::bit_cast<float>(0x3EAA7EFAu);
    if (third < f1) f1 = third;
    float f0 = psp_mul(f21, std::bit_cast<float>(0x3855FEBFu));
    f0 = psp_mul(f0, f0);
    if (third < f0) f0 = third;
    const float f24 = f0 + f1;
    f21 = zero;
    f20 = std::bit_cast<float>(0x3EB33333u);
    f22 = std::bit_cast<float>(0x3E800000u);
    for (std::uint32_t index = 0u; index != 4u; ) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        const std::uint32_t flagged = byte_u(rt, record + 0x10Cu);
        const std::uint32_t current = index;
        ++index;
        if (flagged != 0u) continue;
        f0 = call_setf(rt, ctx, 0x0893A6BCu, {self, current}, {}).f0;
        f1 = f0;
        const float limit = std::bit_cast<float>(0x3E99999Au);
        f0 = f0 - f20;
        if (f21 < f1) {
            f1 = psp_mul(f0, f22);
            if (limit < f1) f1 = limit;
        }
        if (float_at(rt, entry + 0x744u) < f1) {
            put_float(rt, entry + 0x744u, f1);
            put8(rt, entry + 0x74Au, 0x12u);
        }
    }
    call_set(rt, ctx, 0x0893AEF8u, {self});
    f1 = float_at(rt, entry + 0x744u);
    f22 = 0.0f;
    if (std::bit_cast<float>(0x3C23D70Au) < f1) f22 = f1 + std::bit_cast<float>(0x3E800000u);
    f21 = 0.0f;
    const float f23b = std::bit_cast<float>(0x3E4CCCCDu);
    f20 = f21;
    for (std::uint32_t index = 0u; index != 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        if (byte_u(rt, entry + 0xDu) != 0u) continue;
        f1 = float_at(rt, record + 0xE0u);
        bool early = index < 2u;
        if (0.0f < f1) {
            float f3 = 0.0f;
            if (byte_u(rt, record + 0x10Cu) != 0u) f3 = f1 / std::bit_cast<float>(0x46992000u);
            f0 = float_at(rt, record + 0x40u);
            f0 = psp_mul(f0, std::bit_cast<float>(0x3D9374BCu));
            const float f2 = std::bit_cast<float>(0x3ECCCCCDu);
            if (f2 < f0) {
                f0 = f0 - f23b;
                f0 = psp_mul(f0, f23b);
                f3 = f3 + f0;
            }
            f0 = 1.0f;
            if (f0 < f3) f3 = f0;
            f20 = f20 + f3;
        }
        if (!early) continue;
        const std::uint32_t kind = byte_u(rt, record + 0xE9u);
        if (kind != 1u && kind != 0xAu && kind != 0xBu && kind != 0x15u) continue;
        float f3 = float_at(rt, record + 0x54u);
        f0 = std::bit_cast<float>(0x4537C000u);
        float f2 = float_at(rt, entry + 0x6C4u);
        f3 = f3 / f0;
        f2 = psp_mul(f2, std::bit_cast<float>(0x3C800000u));
        f3 = psp_mul(f3, f2);
        if (f3 < 0.0f) f3 = -f3;
        f0 = std::bit_cast<float>(0x3E800000u);
        if (f0 < f3) f3 = f0;
        f21 = f21 + f3;
    }
    float f3 = psp_mul(f24, std::bit_cast<float>(0x3FC00000u));
    f0 = std::bit_cast<float>(0x3E99999Au);
    if (f0 < f3) f3 = f0;
    float f2 = psp_mul(f20, std::bit_cast<float>(0x40200000u));
    f0 = std::bit_cast<float>(0x3ECCCCCDu);
    if (f0 < f2) f2 = f0;
    f1 = f21 + f21;
    f0 = 0.5f;
    if (f0 < f1) f1 = f0;
    const std::uint32_t positive = 0.0f < f1 ? 1u : 0u;
    f1 = f2 + f1;
    put8(rt, flag_out, positive);
    if (f1 < f3) f1 = f3;
    if (f1 < f22) f1 = f22;
    const float f5 = float_at(rt, entry + 0x738u);
    float f4 = float_at(rt, entry + 0x73Cu);
    f0 = std::bit_cast<float>(0x447A0000u);
    f1 = f1 - f5;
    f2 = psp_mul(f4, std::bit_cast<float>(0xC2480000u));
    f3 = std::bit_cast<float>(0x3C88AB7Cu);
    f1 = psp_mul(f1, f0);
    f1 = f1 + f2;
    f1 = psp_mul(f1, f3);
    f4 = f4 + f1;
    f3 = psp_mul(f4, f3);
    put_float(rt, entry + 0x73Cu, f4);
    put_float(rt, entry + 0x738u, f5 + f3);
    ctx.gpr[2] = positive;
    ctx.fpr[0] = f22;
}

// func_08BC9A34(result, owner_arg, a, b, shared_ptr): the same construction as func_08BD25E4 with a 0xA4-byte object (vtable
// 0x08CC5200, +0x30 = 0x08BC7ED8, +0x38 = owner_arg) whose two 0x20-element members (+0x3C, +0x5C) are set up by func_08B3767C(member,
// 0x20, literal 0x08CA0F38 / 0x08CA0F40, func_089312DC(a, b)); control block vtable 0x08CC5240; the holders are counted under the
// owner's lock, result = {object, control} and the finish is func_08BBD79C(result + 4, object + 0x20, object). Returns result.
void func_08BC9A34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t owner_arg = ctx.gpr[5];
    const std::uint32_t a = ctx.gpr[6];
    const std::uint32_t b = ctx.gpr[7];
    const std::uint32_t owner = word(rt, ctx.gpr[8]);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t pair = result + 4u;
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0xA4u});
    call_set(rt, ctx, 0x08AC0640u, {object});
    const std::uint32_t inner = object + 0x20u;
    put(rt, object + 0x20u, 0u);
    put(rt, inner + 4u, 0u);
    put(rt, object, 0x08CC5200u);
    put(rt, object + 0x30u, 0x08BC7ED8u);
    put(rt, object + 0x38u, owner_arg);
    for (const std::uint32_t offset : {0x28u, 0x2Cu, 0x34u, 0x80u, 0x84u, 0x88u, 0x8Cu, 0x94u, 0x98u, 0x9Cu, 0xA0u}) put(rt, object + offset, 0u);
    const std::uint32_t first = call_set(rt, ctx, 0x089312DCu, {a, b});
    call_set(rt, ctx, 0x08B3767Cu, {object + 0x3Cu, 0x20u, 0x08CA0F38u, first});
    const std::uint32_t second = call_set(rt, ctx, 0x089312DCu, {a, b});
    call_set(rt, ctx, 0x08B3767Cu, {object + 0x5Cu, 0x20u, 0x08CA0F40u, second});
    lock_count_use(rt, ctx, owner);
    put(rt, result, object);
    call_set(rt, ctx, 0x08B34888u, {owner});
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) + 1u);
    call_set(rt, ctx, 0x08B34B24u, {owner});
    lock_count_use(rt, ctx, owner);
    const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x14u});
    put(rt, control + 4u, 1u);
    put(rt, control + 8u, 1u);
    put(rt, control, 0x08CC5240u);
    put(rt, control + 0xCu, object);
    put(rt, control + 0x10u, owner);
    call_set(rt, ctx, 0x08B34888u, {owner});
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) + 1u);
    call_set(rt, ctx, 0x08B34B24u, {owner});
    put(rt, pair, control);
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    if (word(rt, pair) == 0u) call_virtual_at(rt, ctx, object, 4u, {object});
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    call_set(rt, ctx, 0x08BBD79Cu, {pair, inner, object});
    call_set(rt, ctx, 0x08B34888u, {owner});
    lock_drop_use(rt, ctx, owner);
    ctx.gpr[2] = result;
}

// func_088C38AC(a, b, flags): a leaf comparing two float records. With d = b - a at +0x28 (squared length > 400 -> v0 = a + 0x34
// and out), the projections f1 of d on a's axes (+0x34 / +0x40), sums f8 of the +0x4C pair and the combined thresholds it derives
// 2-bit "seen" fields of the halfword flags->+0x48: a field at bit 2 / 4 / 6 / 8 / 10 / 12 / 14 (bit 0 as well) is raised to 2 when it
// is below 2, or to 1 when it is 0, depending on where f1 (the lateral offset) and f9 (the negated forward offset) fall against
// the 5.0 / 20.0 bands and the half / three-quarter widths. v0 is whatever the last step left (the new halfword, the extracted
// field, 0, or a + 0x34 / a + 0x40).
void func_088C38AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t flags = ctx.gpr[6] + 0x48u;
    float f2 = float_at(rt, b + 0x28u);
    float f3 = float_at(rt, b + 0x2Cu);
    float f0 = float_at(rt, a + 0x28u);
    float f1 = float_at(rt, a + 0x2Cu);
    float f4 = float_at(rt, b + 0x30u);
    float f5 = f2 - f0;
    float f6 = f3 - f1;
    f0 = float_at(rt, a + 0x30u);
    f1 = psp_mul(f5, f5);
    float f7 = f4 - f0;
    f3 = psp_mul(f6, f6);
    f2 = psp_mul(f7, f7);
    f1 = f1 + f3;
    f1 = f1 + f2;
    ctx.gpr[2] = a + 0x34u;
    if (std::bit_cast<float>(0x43C80000u) < f1) return;
    f1 = float_at(rt, a + 0x34u);
    f0 = float_at(rt, a + 0x38u);
    f2 = float_at(rt, a + 0x3Cu);
    f1 = psp_mul(f5, f1);
    f0 = psp_mul(f6, f0);
    f4 = float_at(rt, a + 0x4Cu);
    f3 = float_at(rt, b + 0x4Cu);
    f2 = psp_mul(f7, f2);
    f1 = f1 + f0;
    const float f8 = f4 + f3;
    f1 = f1 + f2;
    f2 = f8 + std::bit_cast<float>(0x40400000u);
    const float f9 = -f1;
    f0 = -f2;
    if (f9 < f0) return;
    ctx.gpr[2] = a + 0x40u;
    if (f2 < f9) return;
    f2 = float_at(rt, a + 0x40u);
    f0 = float_at(rt, a + 0x44u);
    f4 = float_at(rt, a + 0x54u);
    f2 = psp_mul(f5, f2);
    f0 = psp_mul(f6, f0);
    f3 = float_at(rt, b + 0x60u);
    f1 = float_at(rt, a + 0x48u);
    const float nine_tenths = std::bit_cast<float>(0x3F666666u);
    f2 = f2 + f0;
    f6 = f4 + f3;
    f1 = psp_mul(f7, f1);
    f0 = float_at(rt, a + 0x60u);
    f3 = float_at(rt, b + 0x54u);
    f4 = psp_mul(f6, nine_tenths);
    f1 = f2 + f1;
    f3 = f0 + f3;
    const float half = 0.5f;
    f2 = half;
    const bool forward = f4 < f1;
    f7 = psp_mul(f8, std::bit_cast<float>(0x3F400000u));
    f4 = psp_mul(f8, half);
    f0 = psp_mul(f3, nine_tenths);
    const float five = std::bit_cast<float>(0x40A00000u);
    const float twenty = std::bit_cast<float>(0x41A00000u);
    const auto load = [&]() { return half_u(rt, flags); };
    const auto field = [&](std::uint32_t value, unsigned position) { return (value >> position) & 3u; };
    const auto with_field = [&](std::uint32_t value, unsigned position, std::uint32_t replacement) {
        const std::uint32_t merged = (value & ~(3u << position)) | (replacement << position);
        put16(rt, flags, merged);
        ctx.gpr[2] = merged;
    };
    // The field at `position` of the loaded halfword is raised to 2 when below 2 (v0 = the compare, then the new value).
    const auto raise_to_two = [&](std::uint32_t value, unsigned position) {
        const std::uint32_t current = field(value, position);
        ctx.gpr[2] = static_cast<std::uint32_t>(current < 2u);
        if (current < 2u) with_field(value, position, 2u);
    };
    // ... or to 1 when it is 0 (v0 = the field while it is positive).
    const auto raise_to_one = [&](std::uint32_t value, unsigned position) {
        const std::uint32_t current = field(value, position);
        ctx.gpr[2] = current;
        if (current == 0u) with_field(value, position, 1u);
    };
    if (forward) {
        f0 = -f7;
        f1 = f1 - f6;
        if (f9 < f0) {
            if (f1 < five) {
                raise_to_two(load(), 2u);
            } else if (f1 < twenty) {
                raise_to_one(load(), 2u);
            }
            return;
        }
        bool band = false;
        if (f7 < f9) {
            band = true;
        } else {
            if (f1 < twenty) {
                const std::uint32_t value = load();
                const std::uint32_t current = value & 3u;
                ctx.gpr[2] = static_cast<std::uint32_t>(current < 2u);
                if (current < 2u) with_field(value, 0u, 2u);
            }
            f0 = -f4;
            if (f9 < f0) {
                if (f1 < five) raise_to_two(load(), 2u);
                else if (f1 < twenty) raise_to_one(load(), 2u);
            }
            if (f4 < f9) band = true;
        }
        if (band) {
            if (f1 < five) raise_to_two(load(), 4u);
            else if (f1 < twenty) raise_to_one(load(), 4u);
        }
        return;
    }
    f0 = -f0;
    if (f1 < f0) {
        f0 = -f8;
        if (f9 < f0) {
            raise_to_two(load(), 10u);
            return;
        }
        std::uint32_t value = load();
        if (f8 < f9) {
            raise_to_two(value, 12u);
            return;
        }
        const std::uint32_t top = field(value, 14u);
        ctx.gpr[2] = static_cast<std::uint32_t>(top < 2u);
        if (top < 2u) {
            const std::uint32_t merged = (value & 0x3FFFu) | 0xFFFF8000u;
            put16(rt, flags, merged);
            value = merged;
            ctx.gpr[2] = merged;
        }
        ctx.gpr[2] = field(value, 12u);
        if (0.0f < f9) {
            if (field(value, 12u) > 0u) return;
            with_field(value, 12u, 1u);
            return;
        }
        raise_to_one(value, 10u);
        return;
    }
    f3 = psp_mul(f3, f2);
    f2 = psp_mul(f6, f2);
    if (f9 < 0.0f) {
        std::uint32_t value = load();
        const std::uint32_t current = field(value, 6u);
        ctx.gpr[2] = static_cast<std::uint32_t>(current < 2u);
        if (current < 2u) with_field(value, 6u, 2u);
        if (f2 < f1) {
            raise_to_one(load(), 2u);
            return;
        }
        f0 = -f3;
        if (!(f1 < f0)) return;
        raise_to_one(load(), 10u);
        return;
    }
    std::uint32_t value = load();
    const std::uint32_t current = field(value, 8u);
    ctx.gpr[2] = static_cast<std::uint32_t>(current < 2u);
    if (current < 2u) with_field(value, 8u, 2u);
    if (f2 < f1) {
        raise_to_one(load(), 4u);
        return;
    }
    f0 = -f3;
    if (f1 < f0) raise_to_one(load(), 12u);
}

// func_08A7FD4C(self, out, seq): n = func_08A759BC(seq); a probe value of seq (func_08A759E0(frame, seq, 0), tag compared with
// the words at 0x08CF6F04 / 0x08CF6F18) picks how the result value object `out` is made: tag 1: func_08A9DD14 +
// func_08A9BC30 give a value func_08A7B544(frame, probe, v) assigned to out; tag 2: func_08A7C0E0 + func_08A7B544; else with fewer
// than 2 items seq the owner's virtual slot 0x10C(self, out, probe), or, walking the chain self, func_08A7C230(self), ... that
// slot 0x10C(object, out, probe) until out is set. If out is still empty a default value (func_08A5BC20(0x38, 4) block,
// func_08A7E914, func_08A76AB4, func_08A7B544) is assigned and func_08A7C5C8(self, out) runs. For n >= 2 each further index i (1 ..
// n - 1) is offered the same way (func_08A7C28C(iterator, frame + 0x60), slot 0x10C(object, out, probe(seq, i)); func_08A7C5C8
// fallback) until out is set; the frame values are destroyed (func_08A5B9EC / func_08A69D68).
void func_08A7FD4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t seq = ctx.gpr[6];
    GuestFrame frame(ctx, 0xB0u);
    constexpr std::uint32_t kProbe = 0x08A759E0u;
    constexpr std::uint32_t kProbeDestroy = 0x08A69D68u;
    const std::uint32_t count = call_set(rt, ctx, 0x08A759BCu, {seq});
    put(rt, frame.at(0x70u), count);
    call_set(rt, ctx, kProbe, {frame.at(0x58u), seq, 0u});
    std::uint32_t tag = word(rt, 0x08CF6F04u);
    std::uint32_t probed = word(rt, frame.at(0x58u));
    call_set(rt, ctx, kProbeDestroy, {frame.at(0x58u)});
    const auto value_or_null = [&](std::uint32_t address, std::uint32_t message) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    // The temporary value made by func_08A7B544 goes to out (assigned unless it is out), then it is destroyed.
    const auto finish_value = [&](std::uint32_t temporary) {
        if (out != temporary) call_set(rt, ctx, kAssign, {out, word(rt, temporary)});
        call_set(rt, ctx, kDestroy, {temporary});
    };
    if (probed == tag) {
        call_set(rt, ctx, kProbe, {frame.at(0x50u), seq, 0u});
        call_set(rt, ctx, 0x08A9DD14u, {frame.at(0x4Cu)});
        const std::uint32_t source = value_or_null(frame.at(0x4Cu), 0x08CB1348u);
        const std::uint32_t converted = call_set(rt, ctx, 0x08A9BC30u, {source});
        call_set(rt, ctx, 0x08A7B544u, {frame.at(0x54u), frame.at(0x50u), converted});
        finish_value(frame.at(0x54u));
        call_set(rt, ctx, kDestroy, {frame.at(0x4Cu)});
        call_set(rt, ctx, kProbeDestroy, {frame.at(0x50u)});
    } else {
        call_set(rt, ctx, kProbe, {frame.at(0x48u), seq, 0u});
        tag = word(rt, 0x08CF6F18u);
        probed = word(rt, frame.at(0x48u));
        call_set(rt, ctx, kProbeDestroy, {frame.at(0x48u)});
        if (probed == tag) {
            call_set(rt, ctx, kProbe, {frame.at(0x40u), seq, 0u});
            put(rt, frame.at(0x38u), self);
            call_set(rt, ctx, 0x08A7C0E0u, {frame.at(0x3Cu), frame.at(0x38u)});
            call_set(rt, ctx, 0x08A7B544u, {frame.at(0x44u), frame.at(0x40u), frame.at(0x3Cu)});
            finish_value(frame.at(0x44u));
            call_set(rt, ctx, kDestroy, {frame.at(0x3Cu)});
            call_set(rt, ctx, kProbeDestroy, {frame.at(0x40u)});
        } else {
            const std::uint32_t seq_size = static_cast<std::uint32_t>(signed_of(word(rt, seq + 4u) - word(rt, seq)) >> 2);
            if (seq_size < 2u) {
                const std::uint32_t slot = word(rt, word(rt, self) + 0x10Cu);
                call_set(rt, ctx, kProbe, {frame.at(0x34u), seq, 0u});
                call_set(rt, ctx, slot, {self, out, frame.at(0x34u)});
                call_set(rt, ctx, kProbeDestroy, {frame.at(0x34u)});
            } else if (self != 0u) {
                std::uint32_t object = self;
                for (;;) {
                    const std::uint32_t slot = word(rt, word(rt, object) + 0x10Cu);
                    call_set(rt, ctx, kProbe, {frame.at(0x30u), seq, 0u});
                    call_set(rt, ctx, slot, {object, out, frame.at(0x30u)});
                    call_set(rt, ctx, kProbeDestroy, {frame.at(0x30u)});
                    if (word(rt, out) != 0u) break;
                    object = call_set(rt, ctx, 0x08A7C230u, {object});
                    if (object == 0u) break;
                }
            }
        }
    }
    if (word(rt, out) == 0u) {
        call_set(rt, ctx, kProbe, {frame.at(0x28u), seq, 0u});
        call_set(rt, ctx, kProbe, {frame.at(0x20u), seq, 0u});
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
        call_set(rt, ctx, 0x08A7E914u, {block, frame.at(0x20u)});
        put(rt, frame.at(4u), block);
        call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x24u), frame.at(4u)});
        call_set(rt, ctx, 0x08A7B544u, {frame.at(0x2Cu), frame.at(0x28u), frame.at(0x24u)});
        finish_value(frame.at(0x2Cu));
        call_set(rt, ctx, kDestroy, {frame.at(0x24u)});
        call_set(rt, ctx, kProbeDestroy, {frame.at(0x20u)});
        call_set(rt, ctx, kProbeDestroy, {frame.at(0x28u)});
        call_set(rt, ctx, 0x08A7C5C8u, {self, out});
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(count) < 2);
    if (signed_of(count) < 2) return;
    call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x5Cu), 0x08CF6ED8u});
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x60u)});
    std::uint32_t index = 1u;
    do {
        const std::uint32_t object = value_or_null(out, 0x08CB1358u);
        const std::uint32_t slot = word(rt, word(rt, object) + 0x1Cu);
        call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x1Cu)});
        call_set(rt, ctx, slot, {object, frame.at(0x60u), frame.at(0x1Cu)});
        call_set(rt, ctx, kDestroy, {frame.at(0x1Cu)});
        if (call_set(rt, ctx, 0x08A7C28Cu, {frame.at(0x5Cu), frame.at(0x60u)}) != 0u) {
            const std::uint32_t target = value_or_null(frame.at(0x5Cu), 0x08CB1360u);
            const std::uint32_t target_slot = word(rt, word(rt, target) + 0x10Cu);
            call_set(rt, ctx, kProbe, {frame.at(0x18u), seq, index});
            call_set(rt, ctx, target_slot, {target, out, frame.at(0x18u)});
            call_set(rt, ctx, kProbeDestroy, {frame.at(0x18u)});
            if (word(rt, out) == 0u) {
                call_set(rt, ctx, kProbe, {frame.at(0x10u), seq, index});
                call_set(rt, ctx, kProbe, {frame.at(8u), seq, index});
                const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
                call_set(rt, ctx, 0x08A7E914u, {block, frame.at(8u)});
                put(rt, frame.at(0u), block);
                call_set(rt, ctx, 0x08A76AB4u, {frame.at(0xCu), frame.at(0u)});
                call_set(rt, ctx, 0x08A7B544u, {frame.at(0x14u), frame.at(0x10u), frame.at(0xCu)});
                finish_value(frame.at(0x14u));
                call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
                call_set(rt, ctx, kProbeDestroy, {frame.at(8u)});
                call_set(rt, ctx, kProbeDestroy, {frame.at(0x10u)});
                const std::uint32_t last = value_or_null(frame.at(0x5Cu), 0x08CB1360u);
                call_set(rt, ctx, 0x08A7C5C8u, {last, out});
            }
        }
        ++index;
    } while (count != index);
    call_set(rt, ctx, kDestroy, {frame.at(0x60u)});
    call_set(rt, ctx, kDestroy, {frame.at(0x5Cu)});
}

// func_08BB2FB4(self): the teardown of a container: func_08BB295C(self->+0xC, self + 4, 0x08BB1B08), the tree freed
// (func_08BB2CFC(self, self->+8)) and reset to the empty header (+0xC = +0x10 = self + 4, +8 = +0x14 = 0), func_08BB27BC(self->+0x18,
// self->+0x1C, 0x08BB1B08), func_08BB2B00(self + 0x18, self->+0x18, self->+0x1C); then the shared records at +0x28 (released
// when its count +0x10 is 0, then again after the field is cleared and its count dropped to 0), +0x24 and +0x2C are freed: the
// record's chain word (+0xC) goes back to the size class 4 pool, func_08BB1FF0(record) runs with +0xC cleared, and the record
// itself goes to the size class 0x14 pool. Last +0x30 = 0.
void func_08BB2FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t header = self + 4u;
    call_set(rt, ctx, 0x08BB295Cu, {word(rt, self + 0xCu), header, 0x08BB1B08u});
    call_set(rt, ctx, 0x08BB2CFCu, {self, word(rt, self + 8u)});
    put(rt, self + 0x10u, header);
    put(rt, self + 0xCu, header);
    put(rt, self + 8u, 0u);
    put(rt, self + 0x14u, 0u);
    call_set(rt, ctx, 0x08BB27BCu, {word(rt, self + 0x18u), word(rt, self + 0x1Cu), 0x08BB1B08u});
    call_set(rt, ctx, 0x08BB2B00u, {self + 0x18u, word(rt, self + 0x18u), word(rt, self + 0x1Cu)});
    const auto release_blocks = [&](std::uint32_t record) {
        return_to_class(rt, ctx, 4u, word(rt, record + 0xCu), frame.at(0u));
        put(rt, record + 0xCu, 0u);
        call_set(rt, ctx, 0x08BB1FF0u, {record});
        return_to_class(rt, ctx, 0x14u, record, frame.at(0u));
    };
    // Clears the field, drops one use of the record and frees it on the last.
    const auto drop_field = [&](std::uint32_t slot) {
        const std::uint32_t record = word(rt, slot);
        put(rt, slot, 0u);
        if (record == 0u) return;
        const std::uint32_t uses = word(rt, record + 0x10u) - 1u;
        ctx.gpr[2] = uses;
        put(rt, record + 0x10u, uses);
        if (uses == 0u) release_blocks(record);
    };
    std::uint32_t record = word(rt, self + 0x28u);
    if (record != 0u) {
        ctx.gpr[2] = word(rt, record + 0x10u);
        if (word(rt, record + 0x10u) == 0u) release_blocks(record);
    }
    drop_field(self + 0x28u);
    drop_field(self + 0x24u);
    drop_field(self + 0x2Cu);
    put(rt, self + 0x30u, 0u);
}

// func_089CB534(self): after func_08A00AB0(self), nothing when func_08968DE4(self) is 0. With n = self->+0x120 and rows s2 =
// (self->+0xF0 - 1) / n + 1 (the quotient is kept at frame +0x30) the four vectors at +0x144 (bytes, n), +0x12C (floats, n), +0x150
// (bytes, s2) and +0x138 (floats, s2) are resized (func_08C01FB8 / 08C01DFC for bytes, func_08BEF324 / 08BEF1A8 for floats; new
// items 0) and cleared. Over the chain self->+0xF4, func_08968A38(...), whose items func_08A00448(self, item) accepts, the
// item's extents (func_0896B7AC, func_08966B20) are added to the per-column maximum floats [+0x12C][s6] and per-row maxima
// [+0x138][fp] (and the flag bytes set when func_089670DC / func_08967124 say so) with s6 counting columns and fp rows. Then the
// counts +0x15C / +0x160 of flagged columns / rows and the sums f22 / f20 of the maxima are formed, adjusted by func_08A0D048's
// margins (+0x124 and +0x128 spacings) when positive, and func_08966CF0(self) is called with f12 = f22, f13 = f20.
void func_089CB534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    call_set(rt, ctx, 0x08A00AB0u, {self});
    if (call_set(rt, ctx, 0x08968DE4u, {self}) == 0u) return;
    const std::uint32_t bytes_a = self + 0x144u;
    const std::uint32_t floats_b = self + 0x12Cu;
    const std::uint32_t bytes_c = self + 0x150u;
    const std::uint32_t floats_d = self + 0x138u;
    const std::int32_t dividend = signed_of(word(rt, self + 0xF0u)) - 1;
    const std::int32_t divisor = signed_of(word(rt, self + 0x120u));
    std::int32_t quotient;
    if (divisor == 0) quotient = dividend < 0 ? 1 : -1;
    else if (dividend == INT32_MIN && divisor == -1) quotient = INT32_MIN;
    else quotient = dividend / divisor;
    put(rt, frame.at(0x30u), static_cast<std::uint32_t>(quotient));
    const std::uint32_t rows = static_cast<std::uint32_t>(quotient) + 1u;
    // vector<byte> resized to `wanted` items: grown with func_08C01FB8(vector, end, missing, &0), shrunk with func_08C01DFC(vector, begin + wanted, end).
    const auto resize_bytes = [&](std::uint32_t vector, std::uint32_t wanted, std::uint32_t fill) {
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t end = word(rt, vector + 4u);
        const std::uint32_t size = end - begin;
        if (wanted == size) return;
        put8(rt, fill, 0u);
        if (wanted < size) call_set(rt, ctx, 0x08C01DFCu, {vector, begin + wanted, end});
        else call_set(rt, ctx, 0x08C01FB8u, {vector, end, wanted - size, fill});
    };
    const auto resize_floats = [&](std::uint32_t vector, std::uint32_t wanted, std::uint32_t fill) {
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t end = word(rt, vector + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
        if (wanted == size) return;
        put(rt, fill, 0u);
        if (wanted < size) call_set(rt, ctx, 0x08BEF1A8u, {vector, begin + (wanted << 2), end});
        else call_set(rt, ctx, 0x08BEF324u, {vector, end, wanted - size, fill});
    };
    resize_bytes(bytes_a, word(rt, self + 0x120u), frame.at(1u));
    resize_floats(floats_b, word(rt, self + 0x120u), frame.at(8u));
    resize_bytes(bytes_c, rows, frame.at(0u));
    resize_floats(floats_d, rows, frame.at(4u));
    for (std::int32_t index = 0; index < signed_of(word(rt, self + 0x120u)); ++index) {
        put(rt, word(rt, floats_b) + static_cast<std::uint32_t>(index) * 4u, 0u);
        put8(rt, word(rt, bytes_a) + static_cast<std::uint32_t>(index), 0u);
    }
    for (std::uint32_t index = 0u; signed_of(rows) > 0 && index != rows; ++index) {
        put(rt, word(rt, floats_d) + index * 4u, 0u);
        put8(rt, word(rt, bytes_c) + index, 0u);
    }
    std::uint32_t item = word(rt, self + 0xF4u);
    if (item != 0u) {
        std::uint32_t column = 0u;
        std::uint32_t row = 0u;
        do {
            if (call_set(rt, ctx, 0x08A00448u, {self, item}) != 0u) {
                call_set(rt, ctx, 0x0896B7ACu, {item, frame.at(0xCu), frame.at(0x10u)});
                call_set(rt, ctx, 0x08966B20u, {item, frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu), frame.at(0x20u)});
                const std::uint32_t width = word(rt, frame.at(0x14u)) + word(rt, frame.at(0x18u));
                float f2 = static_cast<float>(signed_of(width));
                const std::uint32_t height = word(rt, frame.at(0x1Cu)) + word(rt, frame.at(0x20u));
                float f1 = static_cast<float>(signed_of(height));
                f2 = f2 + float_at(rt, frame.at(0xCu));
                float f3 = f1 + float_at(rt, frame.at(0x10u));
                put_float(rt, frame.at(0xCu), f2);
                put_float(rt, frame.at(0x10u), f3);
                const std::uint32_t column_slot = word(rt, floats_b) + column * 4u;
                if (float_at(rt, column_slot) < f2) {
                    put_float(rt, column_slot, f2);
                    f3 = float_at(rt, frame.at(0x10u));
                }
                const std::uint32_t row_slot = word(rt, floats_d) + row * 4u;
                if (float_at(rt, row_slot) < f3) put_float(rt, row_slot, f3);
                if (call_set(rt, ctx, 0x089670DCu, {item}) != 0u) put8(rt, word(rt, bytes_a) + column, 1u);
                if (call_set(rt, ctx, 0x08967124u, {item}) != 0u) put8(rt, word(rt, bytes_c) + row, 1u);
                ++column;
                if (column == word(rt, self + 0x120u)) {
                    ++row;
                    column = 0u;
                }
            }
            item = call_set(rt, ctx, 0x08968A38u, {item});
        } while (item != 0u);
    }
    put(rt, self + 0x15Cu, 0u);
    float f22 = 0.0f;
    const std::int32_t columns = signed_of(word(rt, self + 0x120u));
    if (columns > 0) {
        for (std::int32_t index = 0; index < columns; ++index) {
            const float value = float_at(rt, word(rt, floats_b) + static_cast<std::uint32_t>(index) * 4u);
            const std::uint32_t flagged = sext8(byte_u(rt, word(rt, bytes_a) + static_cast<std::uint32_t>(index)));
            f22 = f22 + value;
            if (flagged != 0u) put(rt, self + 0x15Cu, word(rt, self + 0x15Cu) + 1u);
        }
    }
    put(rt, self + 0x160u, 0u);
    float f20 = 0.0f;
    if (signed_of(rows) > 0) {
        for (std::uint32_t index = 0u; index != rows; ++index) {
            const float value = float_at(rt, word(rt, floats_d) + index * 4u);
            const std::uint32_t flagged = sext8(byte_u(rt, word(rt, bytes_c) + index));
            f20 = f20 + value;
            if (flagged != 0u) put(rt, self + 0x160u, word(rt, self + 0x160u) + 1u);
        }
    }
    const float zero = 0.0f;
    if (zero < f22 || zero < f20) {
        call_set(rt, ctx, 0x08A0D048u, {self, frame.at(0x14u), frame.at(0x18u), frame.at(0x1Cu), frame.at(0x20u)});
        if (zero < f22) {
            const std::uint32_t before = word(rt, frame.at(0x14u));
            const std::uint32_t count = word(rt, self + 0x120u) - 1u;
            const float spacing = float_at(rt, self + 0x124u);
            float f1 = static_cast<float>(signed_of(count));
            const std::uint32_t sum = before + word(rt, frame.at(0x18u));
            f1 = psp_mul(f1, spacing);
            const float f2 = static_cast<float>(signed_of(sum));
            f1 = f1 + f22;
            f22 = f1 + f2;
        }
        if (zero < f20) {
            const std::uint32_t before = word(rt, frame.at(0x1Cu));
            const std::uint32_t sum = before + word(rt, frame.at(0x20u));
            float f1 = static_cast<float>(quotient);
            f1 = psp_mul(f1, float_at(rt, self + 0x128u));
            f1 = f1 + f20;
            const float f2 = static_cast<float>(signed_of(sum));
            f20 = f1 + f2;
        }
    }
    call_setf(rt, ctx, 0x08966CF0u, {self}, {f22, f20});
}

} // namespace hot_batch_118
} // namespace gt::game

#define HOT118(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_118::name, gt::ReturnKind::kind)
HOT118(0x08BACB64u, func_08BACB64, Word);
HOT118(0x08A1ED68u, func_08A1ED68, Word);
HOT118(0x0895FA54u, func_0895FA54, Word);
HOT118(0x088AC1D4u, func_088AC1D4, Word);
HOT118(0x08A7858Cu, func_08A7858C, Word);
HOT118(0x08889244u, func_08889244, Word);
HOT118(0x08AA5944u, func_08AA5944, Word);
HOT118(0x08AEC210u, func_08AEC210, Word);
HOT118(0x088FD0F0u, func_088FD0F0, Word);
HOT118(0x0888CBACu, func_0888CBAC, Word);
HOT118(0x088C3118u, func_088C3118, Float);
HOT118(0x089DD648u, func_089DD648, Word);
HOT118(0x088D00B0u, func_088D00B0, Word);
// HOT118(0x08867268u, func_08867268, Word);  // batch 118: mismatched in the full gate (the 0x9C branch at 0x088672BC is read backwards: a non-zero func_088CE914 jumps to 0x08867654); to rewrite
HOT118(0x088C3D5Cu, func_088C3D5C, Word);
HOT118(0x08A189C0u, func_08A189C0, Word);
HOT118(0x08BD25E4u, func_08BD25E4, Word);
HOT118(0x08BC728Cu, func_08BC728C, Word);
HOT118(0x0895D7CCu, func_0895D7CC, Word);
HOT118(0x0893BE1Cu, func_0893BE1C, Float);
HOT118(0x08BC9A34u, func_08BC9A34, Word);
HOT118(0x088C38ACu, func_088C38AC, Word);
HOT118(0x08A7FD4Cu, func_08A7FD4C, Word);
HOT118(0x08BB2FB4u, func_08BB2FB4, Word);
HOT118(0x089CB534u, func_089CB534, Word);
