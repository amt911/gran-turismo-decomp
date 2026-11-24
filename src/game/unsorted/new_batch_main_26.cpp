// New batch (main-session batch 26): functions the extra seeds made emittable, written as structured C from `tools.disasm`.
// Draft only — not yet through the differential gate. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_26 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same for a callee that takes its floats in $f12-$f15 and answers in $f0.
u32 call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// s0-s7 are callee-saved: the original keeps values in them and restores the caller's on every exit.
struct SavedCallee {
    AllegrexContext &ctx;
    std::array<u32, 8> saved;
    explicit SavedCallee(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedCallee() {
        for (unsigned i = 0; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
    SavedCallee(const SavedCallee &) = delete;
    SavedCallee &operator=(const SavedCallee &) = delete;
};

u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u16 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The compiler's byte copies (a run of lwl/lwr pairs is one unaligned word copy).
void copy_bytes(Runtime &rt, u32 to, u32 from, u32 bytes) {
    for (u32 i = 0; i < bytes; i += 4u) put(rt, to + i, word(rt, from + i));
}

// 0x088E5454 (a0 = object holder, a1 = destination state or null, a2 = a byte): copies the state the object's +0x114
// block holds into a1, under the +0x548 / +0x54C line it checks first.
void func_088E5454(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 dest = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    if (dest == 0u) return;
    const u32 owner = word(rt, self);
    const u32 line_a = word(rt, owner + 0x548u);
    const u32 line_b = word(rt, owner + 0x54Cu);
    // The tail every path ends with: the +0x14 word of the state goes to the object's +0x51C, and +0x526 says whether
    // the object's +0x11C word exists. v0 is the flag.
    const auto finish = [&](u32 object) {
        ctx.gpr[2] = word(rt, dest + 0x14u);
        put(rt, object + 0x51Cu, ctx.gpr[2]);
        const u32 current = word(rt, self);
        ctx.gpr[2] = word(rt, current + 0x11Cu) != 0u ? 1u : 0u;
        put_byte(rt, current + 0x526u, ctx.gpr[2]);
    };
    if (line_a != 0xFFFFFFFFu && line_a == line_b) {
        const u16 moved = half_at(rt, dest + 2u);
        put_byte(rt, dest, line_a);
        rt.memory().store16(dest + 0x10u, moved);
        put_byte(rt, dest + 1u, mode);
        call(rt, ctx, 0x088EF2C4u, {word(rt, word(rt, self) + 0x114u)});
        put_byte(rt, word(rt, self) + 0x514u, 1u);
        finish(word(rt, self));
        return;
    }
    if (line_a == 0xFFFFFFFFu || line_b == 0xFFFFFFFFu) {
        ctx.gpr[5] = 0xFFFFFFFFu;
        finish(owner);
        return;
    }
    ctx.gpr[5] = 0xFFFFFFFFu;
    call(rt, ctx, 0x08B344E0u, {word(rt, owner + 0x114u) + 4u});
    const u32 holder = word(rt, word(rt, self) + 0x114u);
    const u32 state = byte_at(rt, holder + 0x280u) != 0u ? holder + 0x160u : holder + 0x40u;
    put(rt, dest + 0x38u, word(rt, state + 0x38u));
    put(rt, dest + 0x3Cu, word(rt, state + 0x3Cu));
    if ((word(rt, state + 0x40u) | word(rt, state + 0x44u)) != 0u) {
        put(rt, dest + 0x40u, word(rt, state + 0x40u));
        put(rt, dest + 0x44u, word(rt, state + 0x44u));
    }
    put(rt, dest + 0x48u, word(rt, state + 0x48u));
    put(rt, dest + 0x4Cu, word(rt, state + 0x4Cu));
    put(rt, dest + 0x20u, word(rt, state + 0x20u));
    put(rt, dest + 0x24u, word(rt, state + 0x24u));
    put(rt, dest + 0x28u, word(rt, state + 0x28u));
    put(rt, dest + 0x2Cu, word(rt, state + 0x2Cu));
    put(rt, dest + 0x14u, word(rt, state + 0x14u));
    put_byte(rt, dest + 0x00u, byte_at(rt, state + 0x00u));
    put_byte(rt, dest + 0x01u, byte_at(rt, state + 0x01u));
    put_byte(rt, dest + 0x0Eu, byte_at(rt, state + 0x0Eu));
    put_byte(rt, dest + 0x18u, byte_at(rt, state + 0x18u));
    put_byte(rt, dest + 0x19u, byte_at(rt, state + 0x19u));
    put_byte(rt, dest + 0x1Au, byte_at(rt, state + 0x1Au));
    put_byte(rt, dest + 0x1Bu, byte_at(rt, state + 0x1Bu));
    put_half(rt, dest + 0x0Au, half_at(rt, state + 0x0Au));
    put_half(rt, dest + 0x0Cu, half_at(rt, state + 0x0Cu));
    put_half(rt, dest + 0x10u, half_at(rt, state + 0x10u));
    if (byte_at(rt, state + 0x13u) != 0u) put_byte(rt, dest + 0x13u, 1u);
    // The +0x30/+0x34 pair is copied only when the count is there (or, with a zero count, its pointer); with a zero
    // count and a zero pointer the copy loop starts from whatever the last pointer load left, as the original does.
    const u32 count = word(rt, state + 0x34u);
    u32 from;
    if (static_cast<s32>(count) > 0) {
        put(rt, dest + 0x30u, word(rt, state + 0x30u));
        put(rt, dest + 0x34u, count);
        from = state + 0x5Cu;
    } else if (static_cast<s32>(count) < 0) {
        from = state + 0x5Cu;
    } else {
        const u32 pointer_field = word(rt, state + 0x38u);
        if (pointer_field != 0u) {
            put(rt, dest + 0x30u, pointer_field);
            put(rt, dest + 0x34u, 0u);
            from = state + 0x5Cu;
        } else {
            from = word(rt, state + 0x2Cu);
        }
    }
    for (u32 to = dest + 0x5Cu; from != state + 0x11Cu; from += 0x10u, to += 0x10u) copy_bytes(rt, to, from, 0x10u);
    put(rt, dest + 0x50u, word(rt, state + 0x50u));
    put(rt, dest + 0x54u, word(rt, state + 0x54u));
    put(rt, dest + 0x58u, word(rt, state + 0x58u));
    call(rt, ctx, 0x08B3457Cu, {word(rt, word(rt, self) + 0x114u) + 4u});
    const u32 object = word(rt, self);
    const u32 slot = word(rt, object + 0x510u);
    if (slot == 0xFFFFFFFFu) {
        put_byte(rt, object + 0x514u, 0u);
    } else {
        const u32 same = (slot ^ byte_at(rt, dest + 0x1Bu)) < 1u ? 1u : 0u;
        put_byte(rt, object + 0x514u, same);
    }
    finish(word(rt, self));
}

// 0x08A511E0 (a0 = object, a1 = a handle/name argument): fills the object from a 0xA8-byte descriptor the file helpers
// read into its frame, allocates the two strips and the two line tables, then starts the worker thread at 0x08A5176C.

// 0x08A7A0D8 (a0 = map holder whose tree header sits at +0x10, a1 = key pointer, a2 = value holder pointer): the map's
// insert: an existing key gets the value assigned (or, when the value is null, the node erased and freed); a missing key
// gets a node built from the key and inserted at the lower bound, creating the tree header when the map is empty.

// 0x08876850 (a0 = owner object whose slot table sits at +0x82C, a1 = text): splits the text at its last ':' and '%', and
// when the head is the marker string at 0x08CA0010 builds the two label buffers and fills the current slot's entry with them.
// v0 = 1 on the filled path, 0 when the slot table is full, the entry is flagged, or the head is another string.
void func_08876850(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x230u);
    const u32 state = self + 0x82Cu;
    const u32 index = word(rt, state);
    if (index == 5u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 object = word(rt, state + index * 8u + 4u);
    if ((half_at(rt, object + 0x26u) & 1u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 head = frame.at(0x00u);
    const u32 tail = frame.at(0x20u);
    const u32 middle = frame.at(0x40u);
    put_byte(rt, head, 0u);
    put_byte(rt, tail, 0u);
    u32 middle_len = 0u;
    u32 after_colon = 0u;
    u32 length = 0u;
    s32 colon = -1;
    s32 percent = -1;
    u32 start = 0u;
    std::int8_t c = static_cast<std::int8_t>(byte_at(rt, text));
    if (c != 0) {
        colon = -1;
        percent = -1;
        length = 0u;
        for (;;) {
            if (c == 0x3A) {
                colon = static_cast<s32>(length);
            } else if (c == 0x25) {
                percent = static_cast<s32>(length);
            }
            length += 1u;
            c = static_cast<std::int8_t>(byte_at(rt, text + length));
            if (c == 0) break;
        }
        if (colon >= 0) {
            call(rt, ctx, 0x08B79A88u, {head, text, static_cast<u32>(colon)});
            after_colon = static_cast<u32>(colon) + 1u;
            put_byte(rt, head + static_cast<u32>(colon), 0u);
            start = after_colon;
        }
        middle_len = length - start;
        if (percent >= 0) {
            const u32 tail_len = length - static_cast<u32>(percent) - 1u;
            call(rt, ctx, 0x08B79A88u, {tail, text + static_cast<u32>(percent) + 1u, tail_len});
            put_byte(rt, tail + tail_len, 0u);
            middle_len = static_cast<u32>(percent) - start;
        }
    }
    call(rt, ctx, 0x08B79A88u, {middle, text + after_colon, middle_len});
    put_byte(rt, middle + middle_len, 0u);
    u32 second = 0u;
    u32 label = middle;
    if (call(rt, ctx, 0x08B35FD8u, {head, 0x08CA0010u}) == 0u) {
        const u32 first_buffer = frame.at(0x100u);
        call(rt, ctx, 0x08B37640u, {first_buffer, 0x08CA0018u, middle});
        label = first_buffer;
        if (call(rt, ctx, 0x0886AF64u, {word(rt, self)}) != 0u) {
            const u32 second_buffer = frame.at(0x180u);
            call(rt, ctx, 0x08B37640u, {second_buffer, 0x08CA002Cu, middle});
            second = second_buffer;
        }
    }
    const u32 slot_index = word(rt, state);
    const u32 entry = state + slot_index * 8u;
    const u32 target = word(rt, entry + 4u);
    put(rt, target + 0x24u, word(rt, target + 0x24u) | 0x10000u);
    put(rt, target + 0x08u, call(rt, ctx, 0x0886B0A4u, {word(rt, self)}));
    put(rt, target + 0x04u, call(rt, ctx, 0x0886B0C8u, {word(rt, self), 0u}));
    if (second != 0u) {
        put_byte(rt, target + 0x24u, 2u);
        const u32 second_len = call(rt, ctx, 0x08B35F60u, {second});
        call(rt, ctx, 0x08B945C4u, {target + 0x0Cu, second, second_len});
        const u32 label_len = call(rt, ctx, 0x08B35F60u, {label});
        call(rt, ctx, 0x08B945C4u, {target + 0x10u, label, label_len});
    } else {
        put_byte(rt, target + 0x24u, 1u);
        const u32 label_len = call(rt, ctx, 0x08B35F60u, {label});
        call(rt, ctx, 0x08B945C4u, {target + 0x0Cu, label, label_len});
    }
    const u32 current = word(rt, entry + 4u);
    put(rt, current + 0x34u, word(rt, self + 4u));
    const u32 text_len = call(rt, ctx, 0x08B35F60u, {label});
    call(rt, ctx, 0x08B945C4u, {current + 0x30u, label, text_len});
    const u32 slash = call(rt, ctx, 0x08B99550u, {current + 0x30u, 0x2Fu, 0xFFFFFFFFu});
    if (slash != 0xFFFFFFFFu) call(rt, ctx, 0x08B99E48u, {current + 0x30u, slash + 1u, 0u});
    call(rt, ctx, 0x08B9A258u, {entry + 4u});
    call(rt, ctx, 0x0886BC38u, {word(rt, self)});
    call(rt, ctx, 0x0886BC14u, {word(rt, self)});
    ctx.gpr[2] = 1u;
}

// 0x088C58F0 (a0 = object, a1 = a float output, a2 = a float output, a3 = a third float output, t0/t1/t2 = word outputs,
// the two words at the caller's stack top, $f12-$f15 = four floats): asks the object's virtual at +8 whether the point is
// inside, then resolves the four corner offsets through 0x088C5834 and writes the chosen distances out. v0 = 1 when the
// point was hit, 0 otherwise.

// 0x08902C00 and 0x08905000 (one body, two addresses; a0 = a float pair output, a1/a2/a3/t0 = three 0x28-byte geometry
// structs): picks the edge pair 0x08905594 reports, wraps the distance at +0x24 of the two structs into +0x48 of a local
// record, then measures the gap: a cross product, a scale by that record's first float, a second cross product, a length,
// and out[0] = sqrt(length) * (record[3] - 1 on the edge-pair path, record[3] on the other); out[1] = the wrapped value or
// -1 when the wrap's check fails. v0 = the output pointer.
void measure_gap(Runtime &rt, AllegrexContext &ctx, u32 out, u32 arg_a1, u32 arg_a2, u32 arg_a3, u32 arg_t0) {
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0xB0u);
    const u32 record = frame.at(0x48u);
    const u32 other_vec = frame.at(0x54u);
    const u32 measure_vec = frame.at(0x60u);
    u32 selector = call(rt, ctx, 0x08905594u, {record, arg_a2, arg_a3, arg_t0, arg_a3 + 0x0Cu, arg_t0 + 0x0Cu});
    if (selector == 0u) {
        if (call(rt, ctx, 0x08905594u, {record, arg_a2, arg_a3 + 0x0Cu, arg_t0 + 0x0Cu, arg_a3 + 0x18u, arg_t0 + 0x18u}) == 0u) {
            put_float(rt, out, 0.0f);
            put_float(rt, out + 4u, -1.0f);
            ctx.gpr[2] = out;
            return;
        }
    }
    float wrapped = -1.0f;
    const float reach = float_at(rt, record);
    if (std::fabs(reach) < std::bit_cast<float>(0x7F7FFFFFu)) {
        const float limit = float_at(rt, arg_a3 + 0x24u);
        const float scale = float_at(rt, arg_a1 + 0x14u);
        float value = float_at(rt, arg_t0 + 0x24u);
        if (value < limit) value += scale;
        value -= limit;
        wrapped = limit + value * reach;
        if (scale < wrapped) wrapped -= scale;
    }
    const auto measure = [&](u32 first, u32 second, u32 third) {
        copy_bytes(rt, second, first, 0x0Cu);
        call_floats(rt, ctx, 0x08B4A5E8u, {second}, {reach});
        copy_bytes(rt, third, other_vec, 0x0Cu);
        call(rt, ctx, 0x08B4AA34u, {third, third, second});
        copy_bytes(rt, measure_vec, third, 0x0Cu);
        return call_float(rt, ctx, 0x08B4A720u, {measure_vec}, {});
    };
    float result;
    if (selector != 0u) {
        const u32 first = frame.at(0x24u);
        put_float(rt, frame.at(0x2Cu), float_at(rt, arg_t0 + 0x14u) - float_at(rt, arg_t0 + 0x08u));
        put_float(rt, frame.at(0x5Cu), float_at(rt, arg_a3 + 0x14u) - float_at(rt, arg_a3 + 0x08u));
        put_float(rt, other_vec, float_at(rt, arg_a3 + 0x0Cu) - float_at(rt, arg_a3 + 0x00u));
        put_float(rt, frame.at(0x24u), float_at(rt, arg_t0 + 0x0Cu) - float_at(rt, arg_t0 + 0x00u));
        put(rt, frame.at(0x58u), 0u);
        put(rt, frame.at(0x28u), 0u);
        call(rt, ctx, 0x08B4AA68u, {first, first, other_vec});
        result = std::sqrt(measure(frame.at(0x24u), frame.at(0x30u), frame.at(0x3Cu))) *
                 (float_at(rt, frame.at(0x4Cu)) - 1.0f);
    } else {
        const u32 first = frame.at(0x00u);
        const float from = float_at(rt, arg_a3 + 0x0Cu);
        put_float(rt, frame.at(0x5Cu), float_at(rt, arg_a3 + 0x20u) - float_at(rt, arg_a3 + 0x14u));
        put_float(rt, frame.at(0x08u), float_at(rt, arg_t0 + 0x20u) - float_at(rt, arg_t0 + 0x14u));
        put_float(rt, other_vec, float_at(rt, arg_a3 + 0x18u) - from);
        put_float(rt, frame.at(0x00u), float_at(rt, arg_t0 + 0x18u) - float_at(rt, arg_t0 + 0x0Cu));
        put(rt, frame.at(0x58u), 0u);
        put(rt, frame.at(0x04u), 0u);
        call(rt, ctx, 0x08B4AA68u, {first, first, other_vec});
        result = std::sqrt(measure(frame.at(0x00u), frame.at(0x0Cu), frame.at(0x18u))) * float_at(rt, frame.at(0x4Cu));
    }
    put_float(rt, out, result);
    put_float(rt, out + 4u, wrapped);
    ctx.gpr[2] = out;
}

void func_08902C00(Runtime &rt, AllegrexContext &ctx) {
    measure_gap(rt, ctx, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]);
}

void func_08905000(Runtime &rt, AllegrexContext &ctx) {
    measure_gap(rt, ctx, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]);
}

// 0x08B7E014 (a0 = an errno word to set, a1 = text, a2 = an end pointer or null, a3 = base): the 64-bit text-to-number
// parser: it skips the class-8 (space) characters, takes a sign, resolves base 0 / 16 from a "0x" prefix, accumulates the
// digits the class table at 0x08C96BD9 accepts, and clamps on overflow (errno = 0x22, the extreme pair). v0:v1 = the value.

// 0x08C78E68 (a0 = a stream buffer object, a1 = a container the lookup index comes from): the buffer's underflow: looks the
// index up twice in the container, then either takes the object's virtual at +0x14 to grow, or sets the read area up through
// the virtuals at +0x18 / +0x10, or resets the area when no stream object is left. The element goes to +0x90.

// The intrusive holder the container at +0x44AC keeps: a retain counter at +4 and a shared counter at +8; the virtuals at +8
// and +0xC of the object run when each counter falls to zero.
void assign_holder(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    if (value == word(rt, slot)) return;
    if (value != 0u) put(rt, value + 4u, word(rt, value + 4u) + 1u);
    const u32 old = word(rt, slot);
    if (old != 0u) {
        const u32 count = word(rt, old + 4u);
        put(rt, old + 4u, count - 1u);
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, old) + 0x08u), {old});
            const u32 shared = word(rt, old + 8u);
            put(rt, old + 8u, shared - 1u);
            if (shared == 1u) call_indirect(rt, ctx, word(rt, word(rt, old) + 0x0Cu), {old});
        }
    }
    put(rt, slot, value);
}

void release_holder(Runtime &rt, AllegrexContext &ctx, u32 value) {
    if (value == 0u) return;
    const u32 count = word(rt, value + 4u);
    put(rt, value + 4u, count - 1u);
    if (count != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, value) + 0x08u), {value});
    const u32 shared = word(rt, value + 8u);
    put(rt, value + 8u, shared - 1u);
    if (shared == 1u) call_indirect(rt, ctx, word(rt, word(rt, value) + 0x0Cu), {value});
}

// 0x0886473C (a0 = the owner object): the destruction of the sub-container at +0x4490 / +0x44A0: its parts are moved into a
// stack temporary, the recorded values at +0x44AC / +0x44B4 and the two holders at +0x44B0 / +0x44B8 take what the temporary
// carries, and the temporary is released.

// 0x08A79DD4 (a0 = map holder with its tree at +0x10, a1 = the value holder, a2 = the key, a3 = a pointer to the object the
// key maps to): an existing key copies the node's object into the holder — through the object's virtual at +0x60 when its
// +0x28 check answers, else by a plain holder assignment — and resets the holder to empty otherwise; a missing key inserts
// the value the object supplies at +0xCC / +0x60 once the holder is empty. Only an empty holder (word 0) reaches the insert.

// 0x08AED688 (a0 = owner, a1 = a byte, a2 = a mode, a3 = a byte, t0/t1/t2/t3 = words, the word at the caller's stack top):
// takes a free entry (byte +0x38 clear) out of the global table at 0x08D41AD0 (+0x478 array, +0x47C count; stride 0x60B0),
// initialises it from the arguments and the mode's flag word (0x1000/0x4000, or 0x1001/0x2000 for the modes 0x69, 0xC0 and
// 0x84), and reports the entry's +4 word. An unknown mode reports through the assert at 0x08AD83B8 and answers -1.
void func_08AED688(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 arg_a1 = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 arg_a3 = ctx.gpr[7];
    const u32 arg_t0 = ctx.gpr[8];
    const u32 arg_t1 = ctx.gpr[9];
    const u32 arg_t2 = ctx.gpr[10];
    const u32 arg_t3 = ctx.gpr[11];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 stack_arg = word(rt, frame.at(0x40u));
    const auto fail = [&](u32 line, u32 message) {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) {
            call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, line, message, mode});
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    u32 flags;
    u32 extra;
    if (mode == 0x69u || mode == 0xC0u || mode == 0x84u) {
        flags = 0x1001u;
        extra = 0x2000u;
    } else if (mode == 0x20u || mode == 0x30u || mode == 0x40u || mode == 0x60u || mode == 0x80u || mode == 0xA0u ||
               mode == 0x100u || mode == 0x140u || mode == 0x160u || mode == 0x1001u) {
        flags = 0x1000u;
        extra = 0x4000u;
    } else {
        fail(0x968u, 0x08C92304u);
        return;
    }
    put(rt, frame.at(0x00u), flags);
    put(rt, frame.at(0x04u), extra);
    const u32 count = word(rt, 0x08D41AD0u + 0x47Cu);
    if (static_cast<s32>(count) <= 0) {
        fail(0x975u, 0x08C92320u);
        return;
    }
    u32 entry = word(rt, 0x08D41AD0u + 0x478u);
    u32 found = 0u;
    u32 index = 0u;
    for (;;) {
        const u32 occupied = byte_at(rt, entry + 0x38u);
        index += 1u;
        found = entry;
        entry += 0x60B0u;
        if (occupied == 0u) break;
        if (index >= count) {
            fail(0x975u, 0x08C92320u);
            return;
        }
    }
    put(rt, found + 0x08u, owner);
    call(rt, ctx, 0x08AE0BACu, {owner, 0u, 0u, 0u, 0u, found + 0x0Cu});
    put(rt, found + 0x20u, arg_t0);
    put(rt, found + 0x24u, arg_t1);
    put(rt, found + 0x28u, arg_t2);
    put_byte(rt, found + 0x38u, 0u);
    put_half(rt, found + 0x30u, word(rt, frame.at(0x00u)));
    put_half(rt, found + 0x32u, mode);
    put_byte(rt, found + 0x35u, arg_a3);
    put_byte(rt, found + 0x36u, arg_a1);
    put_byte(rt, found + 0x3Eu, arg_t3);
    put_half(rt, found + 0x42u, 0x1000u);
    put(rt, found + 0x2Cu, 0xFFFFFFFFu);
    put_byte(rt, found + 0x34u, 0xFFu);
    put_byte(rt, found + 0x37u, 0u);
    put_byte(rt, found + 0x39u, 0u);
    put_byte(rt, found + 0x3Au, 0u);
    put_byte(rt, found + 0x3Bu, 0u);
    put_byte(rt, found + 0x3Cu, 0u);
    put_byte(rt, found + 0x3Du, 0u);
    put_half(rt, found + 0x40u, 0x1000u);
    if (static_cast<s32>(stack_arg) < 0) {
        put(rt, found + 0x2084u, (found + 0x83u) & ~0x3Fu);
        put(rt, found + 0x2088u, 0x2000u);
        put(rt, found + 0x208Cu, 0xFFFFFFFFu);
    } else {
        call(rt, ctx, 0x08ADA4D8u, {stack_arg});
        put(rt, found + 0x208Cu, stack_arg);
        call(rt, ctx, 0x08ADA644u, {stack_arg, 0x40u, 0x40u});
        put(rt, found + 0x2084u, call(rt, ctx, 0x08ADA6E8u, {stack_arg}));
        put(rt, found + 0x2088u, call(rt, ctx, 0x08ADA718u, {stack_arg}));
    }
    put(rt, found + 0x209Cu, 0u);
    put(rt, found + 0x2098u, word(rt, frame.at(0x04u)));
    put(rt, found + 0x60A8u, 0u);
    put(rt, found + 0x60ACu, 0u);
    put_half(rt, found + 0x60A4u, 0x1000u);
    put_byte(rt, found + 0x38u, 1u);
    put(rt, found + 0x2090u, 0u);
    put(rt, found + 0x2094u, 0u);
    put_half(rt, found + 0x60A6u, 0u);
    call(rt, ctx, 0x08C7AA08u, {word(rt, found + 0x18u), 1u});
    ctx.gpr[2] = word(rt, found + 0x04u);
}

// 0x08B518F4 (a0 = a command record: a format text at +0x20, lengths at +0x1C / +0x14, a kind at +0x18, a context at +0x10
// and the two callbacks at +4 / +8): tokenises the command text with the library's tokenizer (0x08B52238 / 0x08B51F38), which
// tracks the quotes and the '=' separator, builds the argument table out of the tokens on the stack, and hands the table to
// the callback the kind selects (1: the +4 callback with the table, 2: the +8 callback, 3: both). v0 = 0 for no kind.

// 0x0893D6A4 (a0 = a curve block with the piecewise breakpoints at +0x18..+0x34, a1 = a table index, f12 = x): maps x through
// the block's curve. The byte at 0x08CDE655 selects the plain piecewise form; otherwise the shape is eased through the curve
// at 0x08B49D7C and blended with the weights in the tables at 0x08CF2A28..0x08CF2A40 when the byte at 0x08CF2A48 is set.
// v0 = f0 = the mapped value.

// 0x08977B14 (a0 = an object with its mode at +0x178, a1 = a second object, a2 / a3 = the two float outputs): the steering
// factor: the two axle angles from 0x08977290 / 0x089771F1, the magnitude's cubed falloff against the weight at 0x08CDE764,
// the per-mode adjustment (0: a decay of the value at 0x08CF4078, 1: the maximum at 0x08CF4074 and the flag byte at
// 0x08CDE7B8, 2: dropped when the axes did not move, 3: kept unless bit 0x400), then the clamped, normalised pair.
void func_08977B14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const u32 out_a = ctx.gpr[6];
    const u32 out_b = ctx.gpr[7];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const auto measure = [&](u32 target, u32 which) { return call_float(rt, ctx, target, {self, other, which}, {}); };
    const float previous_a = measure(0x08977290u, 2u);
    const float previous_b = measure(0x08977290u, 3u);
    const float axle_a = measure(0x089771F4u, 2u);
    const float axle_b = measure(0x089771F4u, 3u);
    const u32 flags = call(rt, ctx, 0x08977270u, {self, other});
    const float magnitude = std::sqrt(psp_mul(axle_a, axle_a) + psp_mul(axle_b, axle_b));
    const float weight = psp_mul(float_at(rt, 0x08CDE764u), 5.0f) / 640.0f;
    float factor = psp_mul(psp_mul(magnitude, psp_mul(magnitude, magnitude)), weight - 1.0f) + 1.0f;
    const float low = -0.2890625f;
    if (low < axle_a && axle_a < 0.2890625f && low < axle_b && axle_b < 0.2890625f) {
        const u32 mode = word(rt, self + 0x178u);
        const float moved_a = std::fabs(axle_a - previous_a);
        const float moved_b = std::fabs(axle_b - previous_b);
        if (mode == 0u) {
            if (0.015625f <= moved_a || 0.015625f <= moved_b) {
                put_float(rt, 0x08CF4078u, 1.0f);
            } else {
                put_float(rt, 0x08CF4078u, float_at(rt, 0x08CF4078u) - 0.1f);
            }
            const float decay = float_at(rt, 0x08CF4078u);
            if (decay < 0.0f) {
                put_float(rt, 0x08CF4078u, 0.0f);
                factor = psp_mul(factor, 0.0f);
            } else {
                factor = psp_mul(factor, decay);
            }
        } else if (mode == 1u) {
            if (0.015625f <= moved_a || 0.015625f <= moved_b) {
                const float top = float_at(rt, 0x08CF4074u);
                if (magnitude < top) {
                    put_byte(rt, 0x08CDE7B8u, 1u);
                } else if (top < magnitude) {
                    put_byte(rt, 0x08CDE7B8u, 0u);
                }
                put_float(rt, 0x08CF4074u, magnitude);
            }
            if (byte_at(rt, 0x08CDE7B8u) != 0u) factor = 0.0f;
        } else if (mode == 2u) {
            if (moved_a < 0.015625f && moved_b < 0.015625f) factor = 0.0f;
        } else if (mode == 3u) {
            if ((flags & 0x400u) == 0u) factor = 0.0f;
        }
    }
    if (factor < 0.0f) factor = 0.0f;
    if (weight < factor) factor = weight;
    float x = axle_a;
    float y = axle_b;
    if (0.0f < magnitude) {
        y = axle_b / magnitude;
        x = axle_a / magnitude;
    }
    put_float(rt, out_a, psp_mul(x, factor));
    put_float(rt, out_b, psp_mul(y, factor));
}

} // namespace
} // namespace new_batch_main_26
} // namespace gt::game

#define NEW26(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_26::name, gt::ReturnKind::kind)
NEW26(0x088E5454u, func_088E5454, Word);
NEW26(0x08876850u, func_08876850, Word);
NEW26(0x08902C00u, func_08902C00, Word);
NEW26(0x08905000u, func_08905000, Word);
NEW26(0x08AED688u, func_08AED688, Word);
NEW26(0x08977B14u, func_08977B14, Void);
