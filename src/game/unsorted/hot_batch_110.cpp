// Hot batch 110: functions of 41-60 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_110 {
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

// func_08C008DC(text, needle, position, length) -> the last index <= position (and <= size - length) where the length
// bytes of needle sit in the shared string text->+0 (size at -0xC), compared by func_08B36170; -1 when none or when the
// string is shorter than length.
void func_08C008DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t data = word(rt, ctx.gpr[4]);
    const std::uint32_t needle = ctx.gpr[5];
    const std::uint32_t position = ctx.gpr[6];
    const std::uint32_t length = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), position);
    const std::uint32_t size = word(rt, data - 0xCu);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (size < length) return;
    const std::uint32_t last = size - length;
    put(rt, frame.at(0u), last);
    std::uint32_t index = position < last ? position : last;
    for (;;) {
        put(rt, frame.at(0x10u), index);
        if (call_set(rt, ctx, 0x08B36170u, {data + index, needle, length}) == 0u) {
            ctx.gpr[2] = index;
            return;
        }
        if (--index == 0xFFFFFFFFu) break;
    }
    put(rt, frame.at(0x10u), index);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08ABC388(name, argument) -> walks the list at 0x08D7718C (next at +0, object at +4): for each object whose virtual
// slot 8 (a name) func_08B35FD8 calls equal to name, its virtual slot 0x28 runs with argument, and a nonzero answer ends
// the walk and is the result; otherwise the last answer (0 when none ran).
void func_08ABC388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t name = ctx.gpr[4];
    const std::uint32_t argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t result = 0u;
    for (std::uint32_t node = word(rt, 0x08D7718Cu); node != 0u; node = word(rt, node)) {
        const std::uint32_t object = word(rt, node + 4u);
        const std::uint32_t own = call_virtual_at(rt, ctx, object, 8u, {object});
        const std::uint32_t differs = call_set(rt, ctx, 0x08B35FD8u, {own, name});
        ctx.gpr[4] = object;
        ctx.gpr[5] = argument;
        if (differs != 0u) continue;
        result = call_virtual_at(rt, ctx, object, 0x28u, {object, argument});
        if (result != 0u) break;
    }
    ctx.gpr[2] = result;
}

// func_0882B460(a0, a1, count, holder): nothing when count <= 0. Otherwise func_088297A8 fills a value object in the frame
// (func_08A5BB7C(0x08C9A2F8) when its word is 0), r = func_0885EDF8(func_088294D0(word)) after the temporary is destroyed,
// and func_088DAD9C(r, holder->+0's virtual slot 0x3C != 0) (func_08A5BB7C(0x08C9A2B4) first when that object is 0).
void func_0882B460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = temporary;
    if (signed_of(ctx.gpr[6]) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {temporary});
    const std::uint32_t value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t found = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {temporary});
    const std::uint32_t result = call_set(rt, ctx, 0x0885EDF8u, {found});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08C9A2B4u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const std::uint32_t flag = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    call_set(rt, ctx, 0x088DAD9Cu, {result, flag != 0u ? 1u : 0u});
}

// func_088CF00C(self) -> a grade 0-4 of the mean of the self->+0x124 floats from self->+0xEC, divided by 0.8: below 0.25,
// 0.5, 0.75 and 1.0 give 0-3, anything else (NaN included) 4. A leaf.
void func_088CF00C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count = word(rt, self + 0x124u);
    float sum = 0.0f;
    for (std::uint32_t index = 0u; signed_of(count) > 0 && index != count; ++index) sum = sum + float_at(rt, self + 0xECu + index * 4u);
    const float mean = sum / static_cast<float>(signed_of(count));
    const float scaled = mean / std::bit_cast<float>(0x3F4CCCCDu);
    std::uint32_t grade = 4u;
    if (scaled < 0.25f) grade = 0u;
    else if (scaled < 0.5f) grade = 1u;
    else if (scaled < 0.75f) grade = 2u;
    else if (scaled < 1.0f) grade = 3u;
    ctx.gpr[2] = grade;
}

// func_088E1ADC(self, index) -> flag bits of the index-th 0x2F8-byte record r of self->+0xC0: 1 unless r->+0xC and the byte
// r->+0x10 are both set; 2 when r->+0xC is 0, r->+4 is not 1, or func_088E85E8(r) is 0 (asked only otherwise); 4 when a
// second func_088E85E8(r) is 0; 8 when the signed r->+0x1FC is below -1.
void func_088E1ADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t record = word(rt, ctx.gpr[4] + 0xC0u) + ctx.gpr[5] * 0x2F8u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t linked = word(rt, record + 0xCu);
    std::uint32_t flags = linked != 0u && byte_u(rt, record + 0x10u) != 0u ? 0u : 1u;
    if (linked == 0u || word(rt, record + 4u) != 1u || call_set(rt, ctx, 0x088E85E8u, {record}) == 0u) flags |= 2u;
    if (call_set(rt, ctx, 0x088E85E8u, {record}) == 0u) flags |= 4u;
    if (signed_of(word(rt, record + 0x1FCu)) < -1) flags |= 8u;
    ctx.gpr[2] = flags;
}

// func_08C46CB0(out, list, it) -> out (a leaf): func_08C46854's iterator step for 0x54-byte nodes linked at +0x52: the
// list's count (+4) goes down; n = it->+8->+0x52; with a node c = it->+0xC, c->+0x52 = n and when n is -1 the list's +2
// becomes c's index ((c - list->+8->+0) / 4, divided exactly by 21 as the low word of the multiply by 0x3CF3CF3D); without
// one the list's +0 = n, and its +2 = n when n is -1. out = {+4: it->+4, +8: node n (0 when n < 0), +0xC: c}.
void func_08C46CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[5];
    const std::uint32_t it = ctx.gpr[6];
    const std::uint32_t next = sext16(half_u(rt, word(rt, it + 8u) + 0x52u));
    const std::uint32_t current = word(rt, it + 0xCu);
    put(rt, list + 4u, word(rt, list + 4u) - 1u);
    if (current != 0u) {
        put16(rt, current + 0x52u, next);
        if (next == 0xFFFFFFFFu) {
            const std::uint32_t quarters = static_cast<std::uint32_t>(signed_of(current - word(rt, word(rt, list + 8u))) >> 2);
            put16(rt, list + 2u, quarters * 0x3CF3CF3Du);
        }
    } else {
        put16(rt, list, next);
        if (next == 0xFFFFFFFFu) put16(rt, list + 2u, next);
    }
    const std::uint32_t node = signed_of(next) < 0 ? 0u : word(rt, word(rt, list + 8u)) + next * 0x54u;
    const std::uint32_t link = word(rt, it + 4u);
    put(rt, out + 0xCu, current);
    put(rt, out + 4u, link);
    put(rt, out + 8u, node);
    ctx.gpr[2] = out;
}

// func_08829B94(a0, a1, count, key): nothing when count <= 0. Otherwise r = func_088294D0(w) for the word w of the value
// object func_088297A8 fills at frame +4 (func_08A5BB7C(0x08C9A2F8) when it is 0), which is destroyed; func_088458D0(temp,
// key) fills a second one at +0 whose word u (func_08A5BB7C(0x08C9A31C) when 0) gives func_0885EE1C(r, func_08845798(u));
// then that one is destroyed.
void func_08829B94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t second = frame.at(0u);
    const std::uint32_t first = frame.at(4u);
    ctx.gpr[4] = first;
    if (signed_of(ctx.gpr[6]) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {first});
    const std::uint32_t value = word(rt, first);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t found = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {first});
    call_set(rt, ctx, 0x088458D0u, {second, key});
    const std::uint32_t other = word(rt, second);
    ctx.gpr[4] = 0x08C9A31Cu;
    if (other == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A31Cu});
    const std::uint32_t part = call_set(rt, ctx, 0x08845798u, {other});
    call_set(rt, ctx, 0x0885EE1Cu, {found, part});
    call_set(rt, ctx, kDestroy, {second});
}

// func_0889868C(self, a1, f12) -> 0 unless self->+0x40 (an object) and self->+0x34 are set; otherwise a tail jump to that
// object's virtual slot 0x34 (read before the second call) with (object, func_0894D934(self->+0x34),
// func_0894CF58(self->+0x34, a1)) and f12 passed on.
void func_0889868C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const float value = ctx.fpr[12];
    if (word(rt, self + 0x40u) == 0u || word(rt, self + 0x34u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = call_set(rt, ctx, 0x0894D934u, {word(rt, self + 0x34u)});
    const std::uint32_t object = word(rt, self + 0x40u);
    const std::uint32_t function = word(rt, word(rt, object) + 0x34u);
    const std::uint32_t last = call_set(rt, ctx, 0x0894CF58u, {word(rt, self + 0x34u), second});
    ctx.fpr[12] = value;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object, first, last});
}

// func_089AFF8C(a0, a1, mode, holder): nothing unless mode is 1 (v0 = 1). func_089AB370 fills a value object at frame +0
// whose word w (func_08A5BB7C(0x08CA50C4) when 0) gets func_089AFF3C(w, x): x = 0 when holder->+0 is 0, otherwise the word
// of the value object func_08969244(temp, holder) fills at +4 (destroyed after). The first object is destroyed last.
void func_089AFF8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = frame.at(0u);
    const std::uint32_t second = frame.at(4u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = first;
    if (ctx.gpr[6] != 1u) return;
    call_set(rt, ctx, 0x089AB370u, {first});
    const bool empty = word(rt, holder) == 0u;
    if (!empty) call_set(rt, ctx, 0x08969244u, {second, holder});
    const std::uint32_t value = word(rt, first);
    ctx.gpr[4] = 0x08CA50C4u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call_set(rt, ctx, 0x089AFF3Cu, {value, empty ? 0u : word(rt, second)});
    if (!empty) call_set(rt, ctx, kDestroy, {second});
    call_set(rt, ctx, kDestroy, {first});
}

// func_08AC7EDC(self): a worker loop: func_08B2E0B0(), then func_08B28C44(self->+4) waits; it ends when the byte self->+0xC
// is clear. Each round takes (function +0x10, argument +0x14, value +0x18) under the lock self->+8, runs
// func_08B2E0EC(value) and function(argument) when set, then under the lock clears the byte +0xD and signals
// func_08B28BE8(lock).
void func_08AC7EDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    for (;;) {
        call_set(rt, ctx, 0x08B2E0B0u, {});
        call_set(rt, ctx, 0x08B28C44u, {word(rt, self + 4u)});
        if (byte_u(rt, self + 0xCu) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        std::uint32_t lock = word(rt, self + 8u);
        call_set(rt, ctx, 0x08B34888u, {lock});
        const std::uint32_t value = word(rt, self + 0x18u);
        const std::uint32_t argument = word(rt, self + 0x14u);
        const std::uint32_t function = word(rt, self + 0x10u);
        call_set(rt, ctx, 0x08B34B24u, {lock});
        call_set(rt, ctx, 0x08B2E0ECu, {value});
        ctx.gpr[4] = argument;
        if (function != 0u) call_set(rt, ctx, function, {argument});
        lock = word(rt, self + 8u);
        call_set(rt, ctx, 0x08B34888u, {lock});
        put8(rt, self + 0xDu, 0u);
        call_set(rt, ctx, 0x08B28BE8u, {lock});
        call_set(rt, ctx, 0x08B34B24u, {lock});
    }
}

// func_088B1550(object, record): nothing when object is 0. Otherwise object's virtual slot 0x30 runs twice, with
// (object, record->+0, &a) and (object, record->+4, &b), a and b frame descriptors {0x08CC3060, 4, byte}: a's byte is
// record->+0x14 (| 0x10 when record->+0xC is 0), b's is record->+0x15. Both get the vtable 0x08CC3030 afterwards.
void func_088B1550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kDescriptor = 0x08CC3060u;
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t record = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t first = frame.at(0u);
    const std::uint32_t second = frame.at(0xCu);
    ctx.gpr[6] = first;
    if (object == 0u) {
        ctx.gpr[2] = 0x08CC0000u;  // v0 keeps the `lui` of 0x08CC3030 (a caller-visible leftover; synthetic cases)
        return;
    }
    std::uint32_t mode = byte_u(rt, record + 0x14u);
    if (word(rt, record + 0xCu) == 0u) mode |= 0x10u;
    const std::uint32_t table = word(rt, object);
    put8(rt, first + 8u, mode);
    put(rt, first + 4u, 4u);
    put(rt, first, kDescriptor);
    ctx.gpr[2] = word(rt, table + 0x30u);
    call_set(rt, ctx, ctx.gpr[2], {object, word(rt, record), first, table});
    const std::uint32_t other = byte_u(rt, record + 0x15u);
    const std::uint32_t table2 = word(rt, object);
    put(rt, second + 4u, 4u);
    put(rt, second, kDescriptor);
    put8(rt, second + 8u, other);
    ctx.gpr[2] = word(rt, table2 + 0x30u);
    call_set(rt, ctx, ctx.gpr[2], {object, word(rt, record + 4u), second});
    put(rt, first, 0x08CC3030u);
    put(rt, second, 0x08CC3030u);
}

// func_08894CC8(self, value): writes value (a signed halfword) to the bit writer at self + 0x28 with func_08B1CA60(writer,
// a1, 64-bit bits, count): the magnitude m is split into a size class k (k = 0 for m < 2; otherwise the smallest k with
// m - (2 + 8 + ... + 2^(2k-1)) < 2^(2k+1)), written as k in 3 bits, the sign in 1 bit, then the remainder in 2k + 1 bits.
void func_08894CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t magnitude = sext16(ctx.gpr[5]);
    std::uint32_t negative = 0u;
    if (signed_of(magnitude) < 0) {
        magnitude = sext16(0u - magnitude);
        negative = 1u;
    }
    std::uint32_t width = 1u;
    std::uint32_t size_class = 0u;
    if (!(signed_of(magnitude) < 2)) {
        std::uint32_t step = 2u;
        for (;;) {
            width += 2u;
            magnitude = sext16(magnitude - step);
            step = 1u << width;
            if (signed_of(magnitude) < signed_of(step)) break;
        }
        ctx.gpr[5] = step;
        size_class = static_cast<std::uint32_t>(signed_of(width) >> 1);
    }
    const std::uint32_t writer = ctx.gpr[4] + 0x28u;
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08B1CA60u, {writer, ctx.gpr[5], size_class, static_cast<std::uint32_t>(signed_of(size_class) >> 31), 3u});
    call_set(rt, ctx, 0x08B1CA60u, {writer, ctx.gpr[5], negative, 0u, 1u});
    tail_call(ctx, 0x08B1CA60u, {writer, ctx.gpr[5], magnitude, static_cast<std::uint32_t>(signed_of(magnitude) >> 31), width});
}

// func_089613A8(list, key, flag): in the circular list at list (next at +0, entries {key +8, flag byte +0xC}), the entry
// with key gets its flag OR-ed with flag (stored as 0/1); when there is none a 0x10-byte node from func_08B35DA0 gets
// {key, flag & 0xFF} and func_08B5C078(node, list) links it.
void func_089613A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    for (std::uint32_t node = word(rt, list); node != list; node = word(rt, node)) {
        if (word(rt, node + 8u) != key) continue;
        const std::uint32_t entry = node + 8u;
        const std::uint32_t merged = (byte_u(rt, entry + 4u) | flag) != 0u ? 1u : 0u;
        put8(rt, entry + 4u, merged);
        ctx.gpr[2] = merged;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_set(rt, ctx, 0x08B35DA0u, {0x10u});
    if (node + 8u != 0u) {
        put8(rt, node + 0xCu, flag);
        put(rt, node + 8u, key);
    }
    tail_call(ctx, 0x08B5C078u, {node, list});
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
    if (rep == empty) {
        ctx.gpr[2] = empty;  // the original compares the rep with v0 = the empty rep, which a Word caller returns (synthetic cases)
        return;
    }
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// func_08877354(self): a string built at frame +4 by func_08AB85F0 goes through func_08AB856C, func_08B77BF4 (64 bits) and
// func_08B71EC8 to a float; when the count at frame +0xC is positive and e = func_088757B0(self, 0x08CA0074) is set,
// func_08AF4458(e + 0xC, f12 = that float). The string is released last (empty rep 0x08D75298, func_08B93554).
void func_08877354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t text = frame.at(4u);
    call_set(rt, ctx, 0x08AB85F0u, {text});
    const std::uint32_t value = call_set(rt, ctx, 0x08AB856Cu, {text});
    const std::uint32_t low = call_set(rt, ctx, 0x08B77BF4u, {value});
    call_set(rt, ctx, 0x08B71EC8u, {low, ctx.gpr[3]});
    const float number = ctx.fpr[0];
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0x08CA0074u;
    if (signed_of(word(rt, frame.at(0xCu))) > 0) {
        const std::uint32_t entry = call_set(rt, ctx, 0x088757B0u, {self, 0x08CA0074u});
        ctx.gpr[4] = entry + 0xCu;
        ctx.fpr[12] = number;
        if (entry != 0u) call_setf(rt, ctx, 0x08AF4458u, {entry + 0xCu}, {number});
    }
    release_string(rt, ctx, text, 0x08D75298u, 0x08B93554u, frame.at(0u));
}

// func_0888A878(dst, src) -> dst (a leaf): copies a record field by field: 16 bytes at +0 (unaligned), the bytes +0x10,
// +0x11, +0x12, +0x13, the halfword +0x14, the words +0x18 to +0x24, and the two 16-word arrays at +0x28 and +0x68.
void func_0888A878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    for (std::uint32_t offset = 0u; offset < 0x10u; ++offset) put8(rt, dst + offset, byte_u(rt, src + offset));
    put8(rt, dst + 0x10u, byte_u(rt, src + 0x10u));
    put8(rt, dst + 0x11u, byte_u(rt, src + 0x11u));
    put(rt, dst + 0x20u, word(rt, src + 0x20u));
    put(rt, dst + 0x24u, word(rt, src + 0x24u));
    put16(rt, dst + 0x14u, half_u(rt, src + 0x14u));
    put8(rt, dst + 0x13u, byte_u(rt, src + 0x13u));
    put8(rt, dst + 0x12u, byte_u(rt, src + 0x12u));
    put(rt, dst + 0x18u, word(rt, src + 0x18u));
    put(rt, dst + 0x1Cu, word(rt, src + 0x1Cu));
    for (std::uint32_t index = 0u; index < 16u; ++index) {
        const std::uint32_t first = word(rt, src + 0x28u + index * 4u);
        const std::uint32_t second = word(rt, src + 0x68u + index * 4u);
        put(rt, dst + 0x28u + index * 4u, first);
        put(rt, dst + 0x68u + index * 4u, second);
    }
    ctx.gpr[2] = dst;
}

// func_08894264(self, src) -> src + func_08894020(...): the 9 words at src are copied to self->+0 first (word by word, the
// source may be unaligned); func_08894020 is entered with the registers the copy leaves (a0 = src->+0x18, a1 = src->+0x1C,
// a2 = src + 0x20, a3 = the destination + 0x20).
void func_08894264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t src = ctx.gpr[5];
    const std::uint32_t dst = word(rt, ctx.gpr[4]);
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t offset = 0u; offset < 0x24u; offset += 4u) {
        const std::uint32_t value = byte_u(rt, src + offset) | byte_u(rt, src + offset + 1u) << 8 | byte_u(rt, src + offset + 2u) << 16 |
                                    byte_u(rt, src + offset + 3u) << 24;
        put(rt, dst + offset, value);
    }
    ctx.gpr[8] = src + 0x20u;
    const std::uint32_t added = call_set(rt, ctx, 0x08894020u, {word(rt, dst + 0x18u), word(rt, dst + 0x1Cu), src + 0x20u, dst + 0x20u});
    ctx.gpr[2] = src + added;
}

// func_088DD714(self, index, value): nothing when self->+0x5B0 is set, func_088DC90C(self, index, value) is not 1, or the
// byte self->+0x22 is set. Otherwise, when func_088E85E8 accepts the index-th 0x2F8-byte record of self->+0xC0,
// self->+0x148's virtual slot 0x38 runs with index; then a tail jump to func_088DD144(self, index, value, 1, 1).
void func_088DD714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    ctx.gpr[2] = word(rt, self + 0x5B0u);
    if (ctx.gpr[2] != 0u) return;
    GuestFrame frame(ctx, 0x10u);
    if (call_set(rt, ctx, 0x088DC90Cu, {}) != 1u) return;
    ctx.gpr[2] = byte_u(rt, self + 0x22u);
    if (ctx.gpr[2] != 0u) return;
    if (call_set(rt, ctx, 0x088E85E8u, {word(rt, self + 0xC0u) + index * 0x2F8u}) != 0u) {
        const std::uint32_t object = word(rt, self + 0x148u);
        call_virtual_at(rt, ctx, object, 0x38u, {object, index});
    }
    tail_call(ctx, 0x088DD144u, {self, index, value, 1u, 1u});
}

// func_088F90CC(self): for each of 4 slots whose bit in the mask word at self + 0x2C is clear, the two objects at
// self + 8 x slot + 4 and + 8 (when set) get their virtual slot 0x40.
void func_088F90CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t slot = 0u; slot < 4u; ++slot) {
        ctx.gpr[2] = self + 0x2Cu + ((slot >> 5) << 2);
        if (((word(rt, ctx.gpr[2]) >> (slot & 31u)) & 1u) != 0u) continue;
        for (std::uint32_t part = 0u; part < 2u; ++part) {
            const std::uint32_t object = word(rt, self + slot * 8u + 4u + part * 4u);
            ctx.gpr[2] = object;
            if (object != 0u) call_virtual_at(rt, ctx, object, 0x40u, {object});
        }
    }
}

// func_089D1118(target, a1, mode, holder): nothing unless mode is 1 (v0 = 1). func_089CD62C fills a value object at frame
// +4 whose word w (func_08A5BB7C(0x08CA69B8) when 0) and holder->+0's virtual slot 0x3C (func_08A5BB7C(0x08CA697C) when
// that object is 0) give func_08A97A6C(temp, func_089CFC00(w, slot)), a value object that goes to target; the first is
// destroyed last.
void func_089D1118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = source;
    if (ctx.gpr[6] != 1u) return;
    call_set(rt, ctx, 0x089CD62Cu, {source});
    const std::uint32_t value = word(rt, source);
    ctx.gpr[4] = 0x08CA69B8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08CA697Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
    const std::uint32_t slot = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    const std::uint32_t result = call_set(rt, ctx, 0x089CFC00u, {value, slot});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, result});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {source});
}

// func_08A69D84(result, a1, holder) -> result: o = holder->+0 (func_08A5BB7C(0x08CB044C) first when 0) fills a frame value
// at +4 through its virtual slot 0x5C (frame, o); a 0x18-byte block from func_08A5BC20(0x18, 4) is constructed by
// func_08A76ACC, gets the vtable 0x08CB6B88 and func_08A6972C(block + 0x14, &value); func_08A76AB4(temp, &block) wraps it
// in a value object at +8 handed to func_08A76ABC(result, temp); then the temporary is destroyed and func_08A69D68 ends the
// frame value.
void func_08A69D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[6]);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t value = frame.at(4u);
    const std::uint32_t temporary = frame.at(8u);
    ctx.gpr[4] = 0x08CB044Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB044Cu});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x5Cu);
    call_set(rt, ctx, ctx.gpr[2], {value, object});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_set(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, 0x08CB6B88u);
    call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, value});
    put(rt, frame.at(0u), block);
    call_set(rt, ctx, 0x08A76AB4u, {temporary, frame.at(0u)});
    call_set(rt, ctx, 0x08A76ABCu, {result, temporary});
    call_set(rt, ctx, kDestroy, {temporary});
    call_set(rt, ctx, 0x08A69D68u, {value});
    ctx.gpr[2] = result;
}

// A C++ pointer to member function as this compiler lays it out: the function (or vtable offset) and the adjustment << 1,
// bit 0 set when the function is virtual. Returns the function to call for `object`, and sets `self` to the adjusted
// object.
inline std::uint32_t member_function(psprecomp::Runtime &rt, std::uint32_t &object, std::uint32_t function, std::uint32_t adjustment) {
    object += static_cast<std::uint32_t>(signed_of(adjustment) >> 1);
    if ((adjustment & 1u) != 0u) return word(rt, word(rt, object) + function);
    return function;
}

// func_08BF1184(self, a1, objects, a3, holder): v = holder->+0's virtual slot 0xFC (func_08A5BB7C(0x08CA42E0) first when
// that object is 0); p = the member function (self->+0x14, self->+0x18) called on objects->+0; then a tail jump to the
// member function (self->+0x1C, self->+0x20) on p with v.
void func_08BF1184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t objects = ctx.gpr[6];
    const std::uint32_t object = word(rt, ctx.gpr[8]);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[4] = 0x08CA42E0u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA42E0u});
    const std::uint32_t value = call_virtual_at(rt, ctx, object, 0xFCu, {object});
    std::uint32_t first = word(rt, objects);
    const std::uint32_t first_function = member_function(rt, first, word(rt, self + 0x14u), word(rt, self + 0x18u));
    const std::uint32_t part = call_set(rt, ctx, first_function, {first, word(rt, self + 0x14u), word(rt, objects), first_function});
    std::uint32_t second = part;
    const std::uint32_t second_function = member_function(rt, second, word(rt, self + 0x1Cu), word(rt, self + 0x20u));
    ctx.gpr[25] = second_function;
    tail_call(ctx, second_function, {second, value});
}

// func_088F5D20(self, a, b, c; f12, f13): when v = func_088F5C8C(self, a, b, c) is set, func_088F6DFC(v, &a', &b', &c';
// f12, f13) with frame copies a' = {a[0..2], 1.0}, b' = b[0..2] and c' = c[0..3].
void func_088F5D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    const std::uint32_t c = ctx.gpr[7];
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t found = call_set(rt, ctx, 0x088F5C8Cu, {});
    const std::uint32_t a_copy = frame.at(0xCu);
    const std::uint32_t b_copy = frame.at(0u);
    const std::uint32_t c_copy = frame.at(0x1Cu);
    ctx.gpr[4] = found;
    ctx.gpr[5] = a_copy;
    ctx.gpr[6] = b_copy;
    ctx.gpr[7] = c_copy;
    ctx.fpr[12] = first;
    ctx.fpr[13] = second;
    if (found == 0u) return;
    for (std::uint32_t index = 0u; index < 3u; ++index) put(rt, a_copy + index * 4u, word(rt, a + index * 4u));
    put_float(rt, a_copy + 0xCu, 1.0f);
    for (std::uint32_t index = 0u; index < 3u; ++index) put(rt, b_copy + index * 4u, word(rt, b + index * 4u));
    for (std::uint32_t index = 0u; index < 4u; ++index) put(rt, c_copy + index * 4u, word(rt, c + index * 4u));
    call_setf(rt, ctx, 0x088F6DFCu, {found, a_copy, b_copy, c_copy}, {first, second});
}

// func_089547A0(self): for i = 0..3, e = func_08955300(self, i) gets e->+8 = +-(p->+0x30 x q->+(0x28 + 4(i/2))) (negated for
// even i), then +(p->+0x24 x q->+(0x10 + 4(i/2))) for i < 2 or minus it for i >= 2, with p = self->+8 and q = self->+0xC.
void func_089547A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = word(rt, self + 8u);
    const std::uint32_t second = word(rt, self + 0xCu);
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t entry = call_set(rt, ctx, 0x08955300u, {self, index});
        const std::uint32_t pair = (index >> 1) << 2;
        float value = psp_mul(float_at(rt, first + 0x30u), float_at(rt, second + pair + 0x28u));
        if ((index & 1u) == 0u) value = -value;
        put_float(rt, entry + 8u, value);
        const float term = psp_mul(float_at(rt, first + 0x24u), float_at(rt, second + pair + 0x10u));
        if (index < 2u) put_float(rt, entry + 8u, term + float_at(rt, entry + 8u));
        else put_float(rt, entry + 8u, float_at(rt, entry + 8u) - term);
        ctx.gpr[2] = second + pair;  // the asm leaves v0 = q + 4(i / 2), the address of the second product
    }
}

// func_08ACB3B4(self): under the lock at self + 0x60, polls s = func_08C7B1A8(). When the byte self->+0x8C is set and s is
// not 1-4: self->+0x90 = func_08C7B190(self->+0x94) (self->+0x8D = 1 when that is 0) and +0x8C is cleared. Otherwise
// s = 2 -> func_08C7B1D8(1); s = 3 -> func_08C7B1C0(); s = 0 with the byte +0x8D set -> it is cleared and func_08B34CA0
// signals the lock. The unlock (func_08B34B24) is a tail jump.
void func_08ACB3B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 0x60u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B34888u, {lock});
    const std::uint32_t state = call_set(rt, ctx, 0x08C7B1A8u, {});
    if (byte_u(rt, self + 0x8Cu) != 0u && !(state - 1u < 4u)) {
        const std::uint32_t handle = call_set(rt, ctx, 0x08C7B190u, {word(rt, self + 0x94u)});
        put(rt, self + 0x90u, handle);
        if (handle == 0u) put8(rt, self + 0x8Du, 1u);
        put8(rt, self + 0x8Cu, 0u);
    } else if (state == 2u) {
        call_set(rt, ctx, 0x08C7B1D8u, {1u});
    } else if (state == 3u) {
        call_set(rt, ctx, 0x08C7B1C0u, {});
    } else if (state == 0u && byte_u(rt, self + 0x8Du) != 0u) {
        put8(rt, self + 0x8Du, 0u);
        call_set(rt, ctx, 0x08B34CA0u, {lock});
    }
    tail_call(ctx, 0x08B34B24u, {lock});
}

// func_08ACEF04(self): a destructor body: vtable 0x08CD6B28; func_08ACDD14(self); func_08C7AD80(); func_08B2EE2C on
// self + 0x2C, + 0x18 and + 4; the two event groups at self + 0x420 (func_08B28DC4 on +0x4A0, func_08B28BE0 on +0x474 and
// +0x448, func_08B28DC4 on +0x420) and self + 0x334 (func_08B28DC4 on +0x3B8, func_08B28BE0 on +0x38C, +0x360 and +0x334);
// func_08B28DC4(self + 0x164); then a tail jump to func_08B0C808(self + 0x40).
void func_08ACEF04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = self + 0x420u;
    const std::uint32_t second = self + 0x334u;
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CD6B28u);
    call_set(rt, ctx, 0x08ACDD14u, {self});
    call_set(rt, ctx, 0x08C7AD80u, {});
    call_set(rt, ctx, 0x08B2EE2Cu, {self + 0x2Cu});
    call_set(rt, ctx, 0x08B2EE2Cu, {self + 0x18u});
    call_set(rt, ctx, 0x08B2EE2Cu, {self + 4u});
    if (first != 0u && first != self + 0x510u) {
        call_set(rt, ctx, 0x08B28DC4u, {self + 0x4A0u});
        call_set(rt, ctx, 0x08B28BE0u, {self + 0x474u});
        call_set(rt, ctx, 0x08B28BE0u, {self + 0x448u});
        call_set(rt, ctx, 0x08B28DC4u, {first});
    }
    if (second != 0u && second != first) {
        call_set(rt, ctx, 0x08B28DC4u, {self + 0x3B8u});
        call_set(rt, ctx, 0x08B28BE0u, {self + 0x38Cu});
        call_set(rt, ctx, 0x08B28BE0u, {self + 0x360u});
        call_set(rt, ctx, 0x08B28BE0u, {second});
    }
    call_set(rt, ctx, 0x08B28DC4u, {self + 0x164u});
    tail_call(ctx, 0x08B0C808u, {self + 0x40u});
}

// func_08AD3E70(): counts its calls at 0x08D404D0 and acts on the first only: a name is formatted by func_08B37640 with the
// pattern 0x08CB4A3C and func_08AD354C(), func_08B10204(record, name, 1) fills a 0x3C-byte record that is copied down the
// frame; its word +0x20 is kept at 0x08D404CC and goes to func_08AD3D7C.
void func_08AD3E70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kCount = 0x08D404D0u;
    const std::uint32_t count = word(rt, kCount) + 1u;
    put(rt, kCount, count);
    ctx.gpr[2] = 0x08D40000u;
    ctx.gpr[4] = 1u;
    if (count != 1u) return;
    GuestFrame frame(ctx, 0x490u);
    const std::uint32_t value = call_set(rt, ctx, 0x08AD354Cu, {1u});
    const std::uint32_t name = frame.at(0x40u);
    const std::uint32_t record = frame.at(0x440u);
    call_set(rt, ctx, 0x08B37640u, {name, 0x08CB4A3Cu, value});
    call_set(rt, ctx, 0x08B10204u, {record, name, 1u});
    for (std::uint32_t offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, frame.at(offset), word(rt, record + offset));
    const std::uint32_t handle = word(rt, frame.at(0x20u));
    put(rt, 0x08D404CCu, handle);
    call_set(rt, ctx, 0x08AD3D7Cu, {handle});
}

// func_088E0984(self, key, a2, a3, t0, t1) -> under the lock at self + 0x5E8: 0 unless e = func_088FE5B4(self->+0x14, key),
// self->+0x5D0 and self->+0x5D4 are all set; then func_088F9CE4(self->+0x5D4, e, key, a2, a3, t0, t1, 1).
void func_088E0984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t fourth = ctx.gpr[7];
    const std::uint32_t fifth = ctx.gpr[8];
    const std::uint32_t sixth = ctx.gpr[9];
    const std::uint32_t lock = self + 0x5E8u;
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, kLock, {lock});
    std::uint32_t result = 0u;
    const std::uint32_t entry = call_set(rt, ctx, 0x088FE5B4u, {word(rt, self + 0x14u), key});
    if (entry != 0u && word(rt, self + 0x5D0u) != 0u && word(rt, self + 0x5D4u) != 0u) {
        result = call_set(rt, ctx, 0x088F9CE4u, {word(rt, self + 0x5D4u), entry, key, third, fourth, fifth, sixth, 1u});
    }
    call_set(rt, ctx, kUnlock, {lock});
    ctx.gpr[2] = result;
}

// func_08B0E6B0(self): under the lock at self + 4: self->+0xEC, +0x104 and +0x108 are cleared. With no list at
// self->+0xFC, the byte self->+0x68 is taken (and cleared) and self's virtual slot 0x24 runs; otherwise each node of the
// list (next at +8) has its object (+0)'s virtual slot 8 run and self->+0x104 counts them. After the unlock, a tail jump
// to func_08B0DEB0(self) when the taken byte was set.
void func_08B0E6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 4u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B34888u, {lock});
    const std::uint32_t list = word(rt, self + 0xFCu);
    put(rt, self + 0xECu, 0u);
    put(rt, self + 0x104u, 0u);
    put(rt, self + 0x108u, 0u);
    std::uint32_t pending = 0u;
    if (list == 0u) {
        pending = byte_u(rt, self + 0x68u);
        put8(rt, self + 0x68u, 0u);
        call_virtual_at(rt, ctx, self, 0x24u, {self});
    } else {
        for (std::uint32_t node = list; node != 0u; node = word(rt, node + 8u)) {
            const std::uint32_t object = word(rt, node);
            ctx.gpr[2] = word(rt, object);
            ctx.gpr[3] = word(rt, ctx.gpr[2] + 8u);
            call_set(rt, ctx, ctx.gpr[3], {object});
            put(rt, self + 0x104u, word(rt, self + 0x104u) + 1u);
        }
    }
    call_set(rt, ctx, 0x08B34B24u, {lock});
    if (pending != 0u) tail_call(ctx, 0x08B0DEB0u, {self});
}

// func_08B327AC(handle, mode) -> switches the mode kept at 0x08D69168: 1 at once when it is already mode. Otherwise
// func_08B324CC(handle, mode); mode 0 -> func_08B2DBF0(the word at 0x08CE0C80) and 1, the mode left unchanged; modes 1 and
// 2 first need func_08B322CC(handle) / func_08B3265C(handle) to succeed (0 otherwise); then the word at 0x08CE0C80 becomes
// max(it, func_08B2DC70()), func_08B2DBF0(0xDE) runs, the mode is stored and the result is 1.
void func_08B327AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kMode = 0x08D69168u;
    constexpr std::uint32_t kLevel = 0x08CE0C80u;
    const std::uint32_t handle = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[5];
    ctx.gpr[2] = 1u;
    if (word(rt, kMode) == mode) return;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B324CCu, {});
    if (mode == 0u) {
        call_set(rt, ctx, 0x08B2DBF0u, {word(rt, kLevel)});
        ctx.gpr[2] = 1u;
        return;
    }
    if ((mode == 1u && call_set(rt, ctx, 0x08B322CCu, {handle}) == 0u) || (mode == 2u && call_set(rt, ctx, 0x08B3265Cu, {handle}) == 0u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t level = call_set(rt, ctx, 0x08B2DC70u, {});
    put(rt, kLevel, static_cast<std::uint32_t>(std::max(signed_of(level), signed_of(word(rt, kLevel)))));
    call_set(rt, ctx, 0x08B2DBF0u, {0xDEu});
    put(rt, kMode, mode);
    ctx.gpr[2] = 1u;
}

// func_0887871C(self): like func_08877354 with a second func_08AB856C value v: when the count at frame +0xC is positive,
// e = func_088756F0(self, 0x08CA0088, v) and r = func_0886A9A0(e, 2) lead to func_08881A5C(r, f12 = the float) when both
// are set. The string is released last.
void func_0887871C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t text = frame.at(4u);
    call_set(rt, ctx, 0x08AB85F0u, {text});
    const std::uint32_t value = call_set(rt, ctx, 0x08AB856Cu, {text});
    const std::uint32_t low = call_set(rt, ctx, 0x08B77BF4u, {value});
    call_set(rt, ctx, 0x08B71EC8u, {low, ctx.gpr[3]});
    const float number = ctx.fpr[0];
    const std::uint32_t again = call_set(rt, ctx, 0x08AB856Cu, {text});
    ctx.gpr[6] = again;
    if (signed_of(word(rt, frame.at(0xCu))) > 0) {
        const std::uint32_t entry = call_set(rt, ctx, 0x088756F0u, {self, 0x08CA0088u, again});
        if (entry != 0u) {
            const std::uint32_t part = call_set(rt, ctx, 0x0886A9A0u, {entry, 2u});
            ctx.fpr[12] = number;
            if (part != 0u) call_setf(rt, ctx, 0x08881A5Cu, {part}, {number});
        }
    }
    release_string(rt, ctx, text, 0x08D75298u, 0x08B93554u, frame.at(0u));
}

// func_088A61A4(self): once per owner o = self->+0 (unless the byte o->+0x4CC is set): a zeroed 0x1CB0-byte block from
// func_08B62344 becomes o->+0x4C8 and is set up: func_0892E51C(o + 0x38 + 0x180 x o->+8 (signed byte), block),
// func_08944100(block + 0x690, block, o + 0xC, -1), func_08951304(block, block + 0x1C30, block + 0x1B30, o + 0xC), and
// block->+0x1CAC = func_08948D04(block + 0x690, block + 0x1B30) (a float).
void func_088A61A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self);
    ctx.gpr[2] = byte_u(rt, owner + 0x4CCu);
    if (ctx.gpr[2] != 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x1CB0u});
    call_set(rt, ctx, 0x08B36018u, {block, 0u, 0x1CB0u});
    put(rt, owner + 0x4C8u, block);
    const std::uint32_t current = word(rt, self);
    const std::uint32_t slot = sext8(byte_u(rt, current + 8u));
    const std::uint32_t data = word(rt, current + 0x4C8u);
    call_set(rt, ctx, 0x0892E51Cu, {(slot << 9) - (slot << 7) + current + 0x38u, data});
    call_set(rt, ctx, 0x08944100u, {data + 0x690u, data, word(rt, self) + 0xCu, 0xFFFFFFFFu});
    call_set(rt, ctx, 0x08951304u, {data, data + 0x1C30u, data + 0x1B30u, word(rt, self) + 0xCu});
    call_set(rt, ctx, 0x08948D04u, {data + 0x690u, data + 0x1B30u});
    put_float(rt, data + 0x1CACu, ctx.fpr[0]);
}

// func_08A2811C(target): a 64-bit value chosen by the name func_08ABD974() answers (asked again for each test, compared
// by func_08B35FD8): 0x004694A0 for 0x08CAB898, 0x004694B7 for 0x08CAB89C, 0x004694A5 for 0x08CAB8A0, otherwise
// func_08B702B4(func_08ABD724(), 0x3B) (64-bit); func_08A5DEE8(temp, value) builds a value object that goes to target.
void func_08A2811C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    constexpr std::uint32_t kNames[3] = {0x08CAB898u, 0x08CAB89Cu, 0x08CAB8A0u};
    constexpr std::uint32_t kValues[3] = {0x004694A0u, 0x004694B7u, 0x004694A5u};
    std::uint32_t low = 0u;
    std::uint32_t high = 0u;
    bool found = false;
    for (std::uint32_t index = 0u; index < 3u && !found; ++index) {
        const std::uint32_t name = call_set(rt, ctx, 0x08ABD974u, {});
        ctx.gpr[5] = kNames[index];
        if (call_set(rt, ctx, 0x08B35FD8u, {name, kNames[index]}) == 0u) {
            low = kValues[index];
            found = true;
        }
    }
    if (!found) {
        const std::uint32_t seconds = call_set(rt, ctx, 0x08ABD724u, {});
        low = call_set(rt, ctx, 0x08B702B4u, {seconds, ctx.gpr[3], 0x3Bu, 0u});
        high = ctx.gpr[3];
    }
    call_set(rt, ctx, 0x08A5DEE8u, {temporary, ctx.gpr[5], low, high});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08C4624C(out, q, v) (a leaf): out->xyz = v->xyz rotated by the quaternion q (x, y, z, w), written as 2 x the products
// with the (0.5 - ...) matrix terms in the asm's order; out->+0xC = v->+0xC.
void func_08C4624C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t q = ctx.gpr[5];
    const std::uint32_t v = ctx.gpr[6];
    const float z = float_at(rt, q + 8u);
    const float y = float_at(rt, q + 4u);
    const float x = float_at(rt, q);
    const float w = float_at(rt, q + 0xCu);
    const float half = 0.5f;
    const float zz = psp_mul(z, z), yy = psp_mul(y, y), xx = psp_mul(x, x);
    const float zw = psp_mul(z, w), xw = psp_mul(x, w), xy = psp_mul(x, y), yw = psp_mul(y, w), xz = psp_mul(x, z), yz = psp_mul(y, z);
    const float r11 = (half - zz) - xx;
    const float r22 = (half - yy) - zz;
    const float xy_plus = xy + zw;
    const float xz_minus = xz - yw;
    const float xw_plus = xw + yz;
    const float r00 = (half - xx) - yy;
    const float xy_minus = xy - zw;
    const float vx = float_at(rt, v);
    const float vy = float_at(rt, v + 4u);
    const float vz = float_at(rt, v + 8u);
    float third = psp_mul(vx, xz_minus);
    const float third_y = psp_mul(vy, xw_plus);
    float first = psp_mul(vx, r22);
    const float first_y = psp_mul(vy, xy_minus);
    float second = psp_mul(vx, xy_plus);
    const float second_y = psp_mul(vy, r11);
    const float xz_plus = xz + yw;
    const float yz_minus = yz - xw;
    first = first + first_y;
    second = second + second_y;
    const float third_z = psp_mul(vz, r00);
    const float first_z = psp_mul(vz, xz_plus);
    third = third + third_y;
    const float second_z = psp_mul(vz, yz_minus);
    first = first + first_z;
    third = third + third_z;
    second = second + second_z;
    put_float(rt, out, first + first);
    put_float(rt, out + 8u, third + third);
    put_float(rt, out + 4u, second + second);
    put(rt, out + 0xCu, word(rt, v + 0xCu));
}

// func_088692E8(self): sets up the two parts at self + 0x74 (p) and self + 0x1B0 (q): func_0887BD88(0x1000),
// func_08869254(self), func_08869D40(q), func_08869F64(q, 2, 1), func_0887ACB0(p, 1, 0, 0), func_0887ACD0(p, self + 0x80),
// then q's setters func_08869D04 (+0x90), D10 (+0x28), D1C (+0x1A0), CCC (self->+0x1BC), CE8 (self->+0x1C0), D28 (p) and
// E60 (0x08C9F630); a tail jump to func_08869D58(q).
void func_088692E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = self + 0x74u;
    const std::uint32_t second = self + 0x1B0u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x0887BD88u, {0x1000u});
    call_set(rt, ctx, 0x08869254u, {self});
    call_set(rt, ctx, 0x08869D40u, {second});
    call_set(rt, ctx, 0x08869F64u, {second, 2u, 1u});
    call_set(rt, ctx, 0x0887ACB0u, {first, 1u, 0u, 0u});
    call_set(rt, ctx, 0x0887ACD0u, {first, self + 0x80u});
    call_set(rt, ctx, 0x08869D04u, {second, self + 0x90u});
    call_set(rt, ctx, 0x08869D10u, {second, self + 0x28u});
    call_set(rt, ctx, 0x08869D1Cu, {second, self + 0x1A0u});
    call_set(rt, ctx, 0x08869CCCu, {second, word(rt, self + 0x1BCu)});
    call_set(rt, ctx, 0x08869CE8u, {second, word(rt, self + 0x1C0u)});
    call_set(rt, ctx, 0x08869D28u, {second, first});
    call_set(rt, ctx, 0x08869E60u, {second, 0x08C9F630u});
    tail_call(ctx, 0x08869D58u, {second});
}

// func_0885F46C(self) -> 1 at once (after func_08893664(o + 0x600)) when the byte o->+0x5AA is set, o = self->+0;
// otherwise r = func_088D7158(d, 1) with d = o->+0x5AC; func_08893664(o + 0x600), func_08B1C47C(), func_08889CF0(o + 0x4584);
// up to 50 waits of 10000 us (func_08B2E2B8) while func_088DA0BC(d) is busy; func_08917AB0(o + 0x5CC); then waits of
// 100000 us while it stays busy; the result is r.
void func_0885F46C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self);
    const std::uint32_t device = word(rt, owner + 0x5ACu);
    GuestFrame frame(ctx, 0x20u);
    if (byte_u(rt, owner + 0x5AAu) != 0u) {
        call_set(rt, ctx, 0x08893664u, {owner + 0x600u});
        ctx.gpr[2] = 1u;
        return;
    }
    const std::uint32_t result = call_set(rt, ctx, 0x088D7158u, {device, 1u});
    call_set(rt, ctx, 0x08893664u, {word(rt, self) + 0x600u});
    call_set(rt, ctx, 0x08B1C47Cu, {});
    call_set(rt, ctx, 0x08889CF0u, {word(rt, self) + 0x4584u});
    for (std::uint32_t tries = 50u; tries != 0u;) {
        const std::uint32_t busy = call_set(rt, ctx, 0x088DA0BCu, {device});
        --tries;
        ctx.gpr[4] = 10000u;
        if (busy == 0u) break;
        call_set(rt, ctx, 0x08B2E2B8u, {10000u});
    }
    call_set(rt, ctx, 0x08917AB0u, {word(rt, self) + 0x5CCu});
    while (call_set(rt, ctx, 0x088DA0BCu, {device}) != 0u) call_set(rt, ctx, 0x08B2E2B8u, {100000u});
    ctx.gpr[4] = 100000u;
    ctx.gpr[2] = result;
}

// func_0888D2E8(self, a1, keys, count, t0, t1): for each of the count halfword keys: with the key also at frame +0, when
// func_088DB2C8(self->+4, key) finds it, func_0888D088(self, a1, key, func_088DA788(self->+4, key, &key, 1, t0 & 0xFF,
// t1 & 0xFF)->+0x70, 1, t1 & 0xFF); otherwise self's virtual slot 0x6C runs with (self, a1, &key, 1, t0 & 0xFF, t1 & 0xFF).
void func_0888D2E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t keys = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    const std::uint32_t low = ctx.gpr[8] & 0xFFu;
    const std::uint32_t high = ctx.gpr[9] & 0xFFu;
    if (signed_of(count) <= 0) return;
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t slot = frame.at(0u);
    for (std::uint32_t index = 0u; index != count; ++index) {
        const std::uint32_t key = half_u(rt, keys + index * 2u);
        put16(rt, slot, key);
        if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), key}) != 0u) {
            const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), key, slot, 1u, low, high});
            call_set(rt, ctx, 0x0888D088u, {self, second, key, word(rt, entry + 0x70u), 1u, high});
        } else {
            call_virtual_at(rt, ctx, self, 0x6Cu, {self, second, slot, 1u, low, high});
        }
    }
}

inline std::uint32_t unaligned_word(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | byte_u(rt, address + 1u) << 8 | byte_u(rt, address + 2u) << 16 | byte_u(rt, address + 3u) << 24;
}

// The shared body of func_088BA534 / func_088F19DC (cursor, dst, size): reads size bytes at the byte cursor cursor->+0 into
// dst (1, 2, 4 and 8 bytes inline and unaligned, any other size through func_08B79A88(dst, cursor, size)) and advances it.
inline void read_at_cursor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t cursor = ctx.gpr[4];
    const std::uint32_t dst = ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t source = word(rt, cursor);
    switch (size) {
    case 1u: put8(rt, dst, byte_u(rt, source)); break;
    case 2u: put16(rt, dst, byte_u(rt, source) | byte_u(rt, source + 1u) << 8); break;
    case 4u: put(rt, dst, unaligned_word(rt, source)); break;
    case 8u: {
        const std::uint32_t low = unaligned_word(rt, source);
        const std::uint32_t high = unaligned_word(rt, source + 4u);
        put(rt, dst, low);
        put(rt, dst + 4u, high);
        break;
    }
    default: call_set(rt, ctx, 0x08B79A88u, {dst, source, size}); break;
    }
    ctx.gpr[2] = word(rt, cursor) + size;
    put(rt, cursor, ctx.gpr[2]);
}

// func_088BA534(cursor, dst, size): read_at_cursor.
void func_088BA534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_at_cursor(rt, ctx); }

// func_088F19DC(cursor, dst, size): read_at_cursor (a second copy of the same code).
void func_088F19DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_at_cursor(rt, ctx); }

// func_088CFDC8(self): resets a state: +0x98 = 2, +0x9C = 0; func_088CED68, func_088CEA34 and func_088CEA60 on self; the
// words +0x140 to +0x154 cleared; +0x158 = func_088DA788(self->+0, self->+0x10)->+0xC; +0x124 = func_088FDA60(t) when
// t = func_088FE2FC(self->+0xC) is set, then 1 when the byte +0x95 is set; func_088CF328(self). When
// e = func_088984C4(self->+0->+0x14C, self->+0x10, 0) is set, +0xA0 is cleared if the byte +0xA4 differs from
// func_08897014(e + 0x20) or the byte +0xA5 differs from +0x95.
void func_088CFDC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 0x98u, 2u);
    put(rt, self + 0x9Cu, 0u);
    call_set(rt, ctx, 0x088CED68u, {self});
    call_set(rt, ctx, 0x088CEA34u, {self});
    call_set(rt, ctx, 0x088CEA60u, {self});
    const std::uint32_t key = word(rt, self + 0x10u);
    const std::uint32_t owner = word(rt, self);
    for (std::uint32_t offset = 0x140u; offset <= 0x154u; offset += 4u) put(rt, self + offset, 0u);
    const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {owner, key});
    put(rt, self + 0x158u, word(rt, entry + 0xCu));
    const std::uint32_t table = call_set(rt, ctx, 0x088FE2FCu, {word(rt, self + 0xCu)});
    if (table != 0u) put(rt, self + 0x124u, call_set(rt, ctx, 0x088FDA60u, {table}));
    if (byte_u(rt, self + 0x95u) != 0u) put(rt, self + 0x124u, 1u);
    call_set(rt, ctx, 0x088CF328u, {self});
    const std::uint32_t found = call_set(rt, ctx, 0x088984C4u, {word(rt, word(rt, self) + 0x14Cu), word(rt, self + 0x10u), 0u});
    if (found == 0u) return;
    const std::uint32_t expected = byte_u(rt, self + 0xA4u);
    if (expected != call_set(rt, ctx, 0x08897014u, {found + 0x20u})) put(rt, self + 0xA0u, 0u);
    if (byte_u(rt, self + 0xA5u) != byte_u(rt, self + 0x95u)) put(rt, self + 0xA0u, 0u);
}

// func_08ABE090(milliseconds, buffer, size): formats a time with the formatter func_08B3767C(buffer, size, pattern, ...):
// "h, m, s, ms" with the pattern 0x08CB4644 when the hours are nonzero, otherwise "m, s, ms" with 0x08CB4658; the value
// 0x481907FF (no time) copies 0x08CB4638 through func_08B7C830 instead. The divisions are the compiler's multiply-high
// forms of / 1000 and / 60. A leaf ending in a tail jump.
void func_08ABE090(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t total = ctx.gpr[4];
    const std::uint32_t buffer = ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[6];
    if (total == 0x481907FFu) return tail_call(ctx, 0x08B7C830u, {buffer, 0x08CB4638u, size});
    const auto high = [](std::uint32_t a, std::uint32_t b) { return static_cast<std::uint32_t>((static_cast<std::uint64_t>(a) * b) >> 32); };
    const std::uint32_t seconds = high(total, 0x10624DD3u) >> 6;
    const std::uint32_t minutes = high(seconds, 0x88888889u) >> 5;
    const std::uint32_t hours = high(minutes, 0x88888889u) >> 5;
    const std::uint32_t millis = total - seconds * 1000u;
    const std::uint32_t second = seconds - minutes * 60u;
    const std::uint32_t minute = minutes - hours * 60u;
    if (signed_of(hours) > 0) return tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB4644u, hours, minute, second, millis});
    tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB4658u, minute, second, millis, millis});
}

// func_0896B4D4(self, size): when self->+0xD0 (o) is set, func_0896A35C(self, 0, 0, &w, &h) gives two floats; o->+0x10 =
// size->+0 / w and o->+0x18 = size->+8 / w when w > 0, otherwise 0 and 1; the same with h for +0x14 / +0x1C from size->+4
// and size->+0xC.
void func_0896B4D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t size = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = frame.at(0u);
    ctx.gpr[8] = frame.at(4u);
    if (word(rt, self + 0xD0u) == 0u) return;
    call_set(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0u), frame.at(4u)});
    for (std::uint32_t axis = 0u; axis < 2u; ++axis) {
        const float extent = float_at(rt, frame.at(axis * 4u));
        if (0.0f < extent) {
            put_float(rt, word(rt, self + 0xD0u) + 0x10u + axis * 4u, float_at(rt, size + axis * 4u) / extent);
            put_float(rt, word(rt, self + 0xD0u) + 0x18u + axis * 4u, float_at(rt, size + 8u + axis * 4u) / float_at(rt, frame.at(axis * 4u)));
        } else {
            put_float(rt, word(rt, self + 0xD0u) + 0x10u + axis * 4u, 0.0f);
            put_float(rt, word(rt, self + 0xD0u) + 0x18u + axis * 4u, 1.0f);
        }
        ctx.gpr[2] = word(rt, self + 0xD0u);  // v0 keeps the last load of self->+0xD0
    }
}

// func_089EE95C(target, a1, count, holder): a value object made by func_089EE140 holds a record r (its word; func_08A5BB7C
// (0x08CA7EB4) when 0). With count > 0, r->+0x258 = holder->+0's virtual slot 0x54 (a float; func_08A5BB7C(0x08CA7E0C)
// first when that object is 0); otherwise func_08A85B1C(temp, trunc(r->+0x258)) builds a value object that goes to target.
void func_089EE95C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (signed_of(ctx.gpr[6]) > 0) {
        const std::uint32_t source = frame.at(0u);
        call_set(rt, ctx, 0x089EE140u, {source});
        const std::uint32_t record = word(rt, source);
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7E0Cu});
        call_virtual_at(rt, ctx, object, 0x54u, {object});
        put_float(rt, record + 0x258u, ctx.fpr[0]);
        call_set(rt, ctx, kDestroy, {source});
        return;
    }
    const std::uint32_t source = frame.at(4u);
    const std::uint32_t temporary = frame.at(8u);
    call_set(rt, ctx, 0x089EE140u, {source});
    const std::uint32_t record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, ctx.fpu_float_to_word(float_at(rt, record + 0x258u), 1u)});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {source});
}

// func_08B1F2B8(self) -> takes the first node of a lock-free queue (head part at self + 0x10, tail at self, the sentinel at
// self + 0x20), retrying until the compare-and-swap steps succeed: n = func_08B1F67C(self + 0x10, &tag),
// next = func_08B1F4DC(n); 0 when next is 0; the head moves with func_08B1F5DC(self + 0x10, n, next, tag, tag + 1), the
// tail is swung the same way when it still points at n; the sentinel is put back (func_08B1F1C4) and the loop goes on when
// n was it, otherwise n is the result.
void func_08B1F2B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t head = self + 0x10u;
    const std::uint32_t sentinel = self + 0x20u;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t tag = frame.at(0u);
    for (;;) {
        const std::uint32_t node = call_set(rt, ctx, 0x08B1F67Cu, {head, tag});
        const std::uint32_t next = call_set(rt, ctx, 0x08B1F4DCu, {node});
        if ((call_set(rt, ctx, 0x08B1F4F4u, {head, node, word(rt, tag)}) & 0xFFu) == 0u) continue;
        if (next == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if ((call_set(rt, ctx, 0x08B1F5DCu, {head, node, next, word(rt, tag), word(rt, tag) + 1u}) & 0xFFu) == 0u) continue;
        if (call_set(rt, ctx, 0x08B1F67Cu, {self, tag}) == node) {
            call_set(rt, ctx, 0x08B1F5DCu, {self, node, next, word(rt, tag), word(rt, tag) + 1u});
        }
        if (node != sentinel) {
            ctx.gpr[2] = node;
            return;
        }
        call_set(rt, ctx, 0x08B1F1C4u, {self, node});
    }
}

} // namespace hot_batch_110
} // namespace gt::game

#define HOT110(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_110::name, gt::ReturnKind::kind)
HOT110(0x08C008DCu, func_08C008DC, Word);
// HOT110(0x08ABC388u, func_08ABC388, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x0882B460u, func_0882B460, Word);
HOT110(0x088CF00Cu, func_088CF00C, Word);
HOT110(0x088E1ADCu, func_088E1ADC, Word);
HOT110(0x08C46CB0u, func_08C46CB0, Word);
HOT110(0x08829B94u, func_08829B94, Word);
HOT110(0x0889868Cu, func_0889868C, Word);
HOT110(0x089AFF8Cu, func_089AFF8C, Word);
// HOT110(0x08AC7EDCu, func_08AC7EDC, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x088B1550u, func_088B1550, Word);
HOT110(0x08894CC8u, func_08894CC8, Word);
HOT110(0x089613A8u, func_089613A8, Word);
// HOT110(0x08877354u, func_08877354, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x0888A878u, func_0888A878, Word);
HOT110(0x08894264u, func_08894264, Word);
HOT110(0x088DD714u, func_088DD714, Word);
HOT110(0x088F90CCu, func_088F90CC, Word);
HOT110(0x089D1118u, func_089D1118, Word);
HOT110(0x08A69D84u, func_08A69D84, Word);
HOT110(0x08BF1184u, func_08BF1184, Word);
HOT110(0x088F5D20u, func_088F5D20, Word);
HOT110(0x089547A0u, func_089547A0, Word);
// HOT110(0x08ACB3B4u, func_08ACB3B4, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT110(0x08ACEF04u, func_08ACEF04, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT110(0x08AD3E70u, func_08AD3E70, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x088E0984u, func_088E0984, Word);
// HOT110(0x08B0E6B0u, func_08B0E6B0, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT110(0x08B327ACu, func_08B327AC, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x0887871Cu, func_0887871C, Word);
// HOT110(0x088A61A4u, func_088A61A4, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x08A2811Cu, func_08A2811C, Word);
HOT110(0x08C4624Cu, func_08C4624C, Word);
// HOT110(0x088692E8u, func_088692E8, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT110(0x0885F46Cu, func_0885F46C, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x0888D2E8u, func_0888D2E8, Word);
// HOT110(0x088BA534u, func_088BA534, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT110(0x088F19DCu, func_088F19DC, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT110(0x088CFDC8u, func_088CFDC8, Word);
HOT110(0x08ABE090u, func_08ABE090, Word);
HOT110(0x0896B4D4u, func_0896B4D4, Word);
HOT110(0x089EE95Cu, func_089EE95C, Word);
// HOT110(0x08B1F2B8u, func_08B1F2B8, Word);  // batch 110: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
