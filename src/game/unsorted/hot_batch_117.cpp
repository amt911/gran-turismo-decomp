// Hot batch 117: functions of 240-300 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_117 {
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


// The signed divide by 10 the compiler writes as a multiply by 0x66666667 (high word), a shift of 2 and the sign.
inline std::int32_t divide10(std::int32_t value) {
    const std::int32_t high = static_cast<std::int32_t>((static_cast<std::int64_t>(value) * 0x66666667LL) >> 32);
    return (high >> 2) - (value >> 31);
}

// The bit just before the end iterator {word pointer, bit offset} of a bit vector (the compiler's expansion of the
// iterator step back by one: a word pointer one word lower when the offset was 0).
inline bool bit_before_end(psprecomp::Runtime &rt, std::uint32_t end_ptr, std::uint32_t end_offset) {
    const std::int32_t previous = signed_of(end_offset) - 1;
    const std::uint32_t sign = static_cast<std::uint32_t>(previous >> 31) >> 27;
    std::int32_t words = previous < 0 ? signed_of(end_offset) + 0x1E : previous;
    words >>= 5;
    const std::int32_t offset = signed_of(((static_cast<std::uint32_t>(previous) + sign) & 0x1Fu) - sign);
    std::uint32_t address = (static_cast<std::uint32_t>(words) << 2) + end_ptr;
    std::uint32_t bit = static_cast<std::uint32_t>(offset);
    if (offset < 0) {
        bit += 0x20u;
        address -= 4u;
    }
    return ((1u << (bit & 31u)) & word(rt, address)) != 0u;
}

inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A float constant from its bit pattern (the `lui / ori / mtc1` triple).
inline float bits_to_float(std::uint32_t bits) { return std::bit_cast<float>(bits); }

// Copies `bytes` (a multiple of 16) from `source` to `destination`, 16 bytes at a time: four words loaded, then four stored.
inline void copy_words16(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t bytes) {
    for (std::uint32_t offset = 0u; offset != bytes; offset += 16u) {
        const std::uint32_t a = word(rt, source + offset);
        const std::uint32_t b = word(rt, source + offset + 4u);
        const std::uint32_t c = word(rt, source + offset + 8u);
        const std::uint32_t d = word(rt, source + offset + 12u);
        put(rt, destination + offset, a);
        put(rt, destination + offset + 4u, b);
        put(rt, destination + offset + 8u, c);
        put(rt, destination + offset + 12u, d);
    }
}

// A word read at any alignment (the original's lwl/lwr pair), little-endian.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24);
}
// A word stored at any alignment (the original's swl/swr pair), little-endian.
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (index * 8u));
}

// The pool's free step (the allocator of func_08BB6964, state 0x08D75F5C): block `bp` (size in units at +4) is put back
// into the address-ordered ring starting at the head (+0x14), merged with the next / previous block when they touch, the
// in-use counter (+0x20) lowered and the head moved to the block it was inserted after. Leaves the last v0 of the code.
inline void pool_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t bp) {
    constexpr std::uint32_t state = 0x08D75F5Cu;
    std::uint32_t at = word(rt, state + 0x14u);
    for (;;) {
        const bool below = at < bp;
        const std::uint32_t next = word(rt, at);
        if (below && bp < next) break;
        if (at < next) {
            at = next;
            continue;
        }
        if (below || bp < next) break;
        at = next;
    }
    put(rt, state + 0x20u, word(rt, state + 0x20u) - word(rt, bp + 4u));
    const std::uint32_t size = word(rt, bp + 4u);
    const std::uint32_t after = word(rt, at);
    std::uint32_t v0 = bp + (size << 3);
    if (v0 != after) {
        put(rt, bp, after);
    } else {
        v0 = size + word(rt, after + 4u);
        put(rt, bp + 4u, v0);
        put(rt, bp, word(rt, after));
    }
    const std::uint32_t at_size = word(rt, at + 4u);
    v0 = (at_size << 3) + at;
    if (bp != v0) {
        put(rt, at, bp);
    } else {
        v0 = at_size + word(rt, bp + 4u);
        put(rt, at + 4u, v0);
        put(rt, at, word(rt, bp));
    }
    put(rt, state + 0x14u, at);
    ctx.gpr[2] = v0;
}

// The pool's first-use set-up behind the guard byte 0x08D75F50 (as in func_08BB6964).
inline void pool_setup(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t state = 0x08D75F5Cu;
    constexpr std::uint32_t guard = 0x08D75F50u;
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    put(rt, state + 0x18u, 0x1001u);
    put(rt, state, 0u);
    put(rt, state + 4u, 0u);
    put(rt, state + 8u, 0u);
    put(rt, state + 0x14u, 0x08D75F68u);
    put(rt, state + 0x1Cu, 0u);
    put(rt, state + 0x20u, 0u);
    put(rt, state + 0x24u, 0u);
    put(rt, state + 0xCu, 0x08D75F68u);
    put(rt, state + 0x10u, 0u);
    call_set(rt, ctx, 0x08B64494u, {guard});
    call_set(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
}

// Puts `item` (when non-null) on the free chain of the size-class record that func_08BB1F48()'s table gives for `key`
// (looked up, or made with func_08BB1D0C(+0x24, +0x28, &key, 0x08BB1B40) when missing or of another class): the record's
// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}
// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}

// func_088103DC(owner, target, value, limit, flag): nothing when target == 0. Splits value into two digits (tens, ones: value
// > 0 gives 9 / 9 from 100 up; value <= 0 gives 0 / 1 when flag is set, else value / 10 and value % 10) and clamps limit to
// 99. func_089DDC94(target, owner, ...) gives the current mode; the wanted one is 4 when limit < 1, else 10. When different:
// target->+0xE8 = 6, func_089DE054 and func_089DE1BC(target, mode), then func_0880E024(owner + 0x224, target, k, c) is
// called with a running float sum in f12 (k = 0, 2, 4, 6, 8 for limit > 0, else 20.0 and then +20.0). Then
// func_089DB078(target, slot, record floats) draws the records of table owner->+0x224 (record = base + ((digit - (int(f[+8])
// - 1)) << 4) + 0x300, four floats): tens at slot 0 (or the constant record 0x08CE29F0), ones at slot 2, and for limit >= 10
// the digits of limit at slots 6 and 8 (the last a tail call), for 1..9 the limit's record at 6 and the constant at 8.
void func_088103DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    const std::uint32_t limit_in = ctx.gpr[7];
    const std::uint32_t flag = ctx.gpr[8] & 0xFFu;
    ctx.gpr[8] = flag;
    ctx.gpr[5] = owner;
    GuestFrame frame(ctx, 0x30u);
    if (target == 0u) return;
    std::int32_t tens;
    std::int32_t ones;
    if (signed_of(value) > 0) {
        tens = 9;
        ones = 9;
        if (signed_of(value) < 100) {
            tens = divide10(signed_of(value));
            ones = signed_of(value) - tens * 10;
        }
    } else if (flag != 0u) {
        tens = 0;
        ones = 1;
    } else {
        tens = divide10(signed_of(value));
        ones = signed_of(value) - tens * 10;
    }
    const std::int32_t limit = std::min(signed_of(limit_in), 99);
    const std::uint32_t mode = limit < 1 ? 4u : 10u;
    const std::uint32_t table = owner + 0x224u;
    const std::uint32_t current = call_set(rt, ctx, 0x089DDC94u, {target, owner, value, limit_in, flag});
    ctx.gpr[2] = 6u;
    if (mode != current) {
        put(rt, target + 0xE8u, 6u);
        call_set(rt, ctx, 0x089DE054u, {target, mode});
        call_set(rt, ctx, 0x089DE1BCu, {target, mode});
        const float zero = 0.0f;
        if (limit > 0) {
            float result = call_setf(rt, ctx, 0x0880E024u, {table, target, 0u, 0x30u}, {zero, zero}).f0;
            float sum = result + zero;
            result = call_setf(rt, ctx, 0x0880E024u, {table, target, 2u, 0x30u}, {sum, zero}).f0;
            sum = sum + result;
            result = call_setf(rt, ctx, 0x0880E024u, {table, target, 4u, 0x2Fu}, {sum, zero}).f0;
            sum = sum + result;
            result = call_setf(rt, ctx, 0x0880E024u, {table, target, 6u, 0x30u}, {sum, zero}).f0;
            const float total = sum + result;
            call_setf(rt, ctx, 0x0880E024u, {table, target, 8u, 0x30u}, {total, zero});
        } else {
            const float twenty = std::bit_cast<float>(0x41A00000u);
            const float result = call_setf(rt, ctx, 0x0880E024u, {table, target, 0u, 0x30u}, {twenty, zero}).f0;
            call_setf(rt, ctx, 0x0880E024u, {table, target, 2u, 0x30u}, {result + twenty, zero});
        }
    }
    // The pointer `base + ((digit - (int(base->+8) - 1)) << 4)`: the record is 0x300 further.
    const auto digit_base = [&](std::int32_t digit, std::uint32_t &index) {
        const std::uint32_t base = word(rt, table);
        index = ctx.fpu_float_to_word(float_at(rt, base + 8u), 1u);
        return base + (static_cast<std::uint32_t>(digit - static_cast<std::int32_t>(index - 1u)) << 4);
    };
    const auto draw = [&](std::uint32_t slot, std::uint32_t pointer, std::uint32_t record) {
        const float f15 = float_at(rt, record + 0xCu);
        const float f12 = float_at(rt, record);
        const float f13 = float_at(rt, record + 4u);
        const float f14 = float_at(rt, record + 8u);
        call_setf(rt, ctx, 0x089DB078u, {target, slot, pointer}, {f12, f13, f14, f15});
    };
    std::uint32_t index = 0u;
    std::uint32_t record = 0x08CE29F0u;
    if (tens != 0) record = digit_base(tens, index) + 0x300u;
    {
        const float f15 = float_at(rt, record + 0xCu);
        const float f12 = float_at(rt, record);
        const float f13 = float_at(rt, record + 4u);
        const float f14 = float_at(rt, record + 8u);
        call_setf(rt, ctx, 0x089DB078u, {target, 0u}, {f12, f13, f14, f15});
    }
    std::uint32_t pointer = digit_base(ones, index);
    draw(2u, pointer, pointer + 0x300u);
    if (limit >= 10) {
        const std::int32_t high = divide10(limit);
        pointer = digit_base(high, index);
        draw(6u, pointer, pointer + 0x300u);
        const std::int32_t low = limit - high * 10;
        pointer = digit_base(low, index);
        record = pointer + 0x300u;
        const float f15 = float_at(rt, record + 0xCu);
        const float f12 = float_at(rt, record);
        const float f13 = float_at(rt, record + 4u);
        const float f14 = float_at(rt, record + 8u);
        ctx.gpr[2] = record;
        ctx.fpr[12] = f12;
        ctx.fpr[13] = f13;
        ctx.fpr[14] = f14;
        ctx.fpr[15] = f15;
        tail_call(ctx, 0x089DB078u, {target, 8u, index});
        return;
    }
    if (limit <= 0) {
        ctx.gpr[2] = 1u;
        return;
    }
    pointer = digit_base(limit, index);
    draw(6u, pointer, pointer + 0x300u);
    record = 0x08CE29F0u;
    ctx.gpr[2] = record;
    ctx.fpr[12] = float_at(rt, record);
    ctx.fpr[13] = float_at(rt, record + 4u);
    ctx.fpr[14] = float_at(rt, record + 8u);
    ctx.fpr[15] = float_at(rt, record + 0xCu);
    tail_call(ctx, 0x089DB078u, {target, 8u});
}

// func_08825C00(out, key, count, args): nothing for count <= 0. Two frame strings are built from literals
// (func_08B85EB8, 0x08C87FC0 and 0x08C9A180). kind = how many leading args are strings: if func_08A76F20(args[0],
// func_08A6BF34()) is 0 kind = 0; else args[0]'s virtual slot 0xC fills a string that goes to the first frame string, and
// kind = 1 when count < 2 or args[1] is not of that type, else args[1]'s string goes to the second one and kind = 2.
// func_088248E4(temp, key) gives a record whose +0x14 word is the owner. With at least 3 args left after the strings,
// func_088547C4 / func_08854678 turn args[kind] into a word (frame +0x34), args[kind + 1]'s virtual slot 0x30 gives
// another (frame +0x30) and each of args[kind + 2 ..] (two slots, frame +0x28 / +0x2C, while below count and non-null)
// is converted by func_0884D660 / func_0884D3D8. func_0885CBCC(owner, string 1, string 2, +0x30, +0x34, frame +0x28)
// builds the result that func_08A85B1C turns into the value assigned to out; both frame strings are released.
void func_08825C00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x38u), out);
    if (signed_of(count) <= 0) return;
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kMessage = 0x08C99B24u;
    const std::uint32_t first_string = frame.at(0x20u);
    const std::uint32_t second_string = frame.at(0x24u);
    call_set(rt, ctx, 0x08B85EB8u, {first_string, 0x08C87FC0u, frame.at(5u)});
    call_set(rt, ctx, 0x08B85EB8u, {second_string, 0x08C9A180u, frame.at(4u)});
    std::uint32_t kind;
    {
        const std::uint32_t object = checked(rt, ctx, word(rt, args), kMessage);
        const std::uint32_t type = call_set(rt, ctx, 0x08A6BF34u, {});
        if (call_set(rt, ctx, 0x08A76F20u, {object, type}) == 0u) {
            kind = 0u;
        } else {
            const std::uint32_t holder = checked(rt, ctx, word(rt, args), kMessage);
            call_virtual_v1(rt, ctx, holder, 0xCu, {frame.at(0x1Cu), holder});
            call_set(rt, ctx, 0x08B855E0u, {first_string, frame.at(0x1Cu)});
            release_string(rt, ctx, frame.at(0x1Cu), kEmpty, 0x08B8542Cu, frame.at(3u));
            kind = 1u;
            if (!(signed_of(count) < 2)) {
                const std::uint32_t second = checked(rt, ctx, word(rt, args + 4u), kMessage);
                const std::uint32_t second_type = call_set(rt, ctx, 0x08A6BF34u, {});
                if (call_set(rt, ctx, 0x08A76F20u, {second, second_type}) != 0u) {
                    const std::uint32_t source = checked(rt, ctx, word(rt, args + 4u), kMessage);
                    call_virtual_v1(rt, ctx, source, 0xCu, {frame.at(0x18u), source});
                    call_set(rt, ctx, 0x08B855E0u, {second_string, frame.at(0x18u)});
                    release_string(rt, ctx, frame.at(0x18u), kEmpty, 0x08B8542Cu, frame.at(2u));
                    kind = 2u;
                }
            }
        }
    }
    call_set(rt, ctx, 0x088248E4u, {frame.at(0x14u), key});
    const std::uint32_t record = checked(rt, ctx, word(rt, frame.at(0x14u)), 0x08C99B4Cu);
    const std::uint32_t owner = word(rt, record + 0x14u);
    call_set(rt, ctx, kDestroy, {frame.at(0x14u)});
    put(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0x34u), 0u);
    std::uint32_t third = 0u;
    if (!(signed_of(count - kind) < 3)) {
        call_set(rt, ctx, 0x088547C4u, {frame.at(0x10u), args + (kind << 2)});
        const std::uint32_t first = checked(rt, ctx, word(rt, frame.at(0x10u)), 0x08C9A184u);
        const std::uint32_t converted = call_set(rt, ctx, 0x08854678u, {first});
        put(rt, frame.at(0x34u), converted);
        call_set(rt, ctx, kDestroy, {frame.at(0x10u)});
        const std::uint32_t next = checked(rt, ctx, word(rt, args + ((kind + 1u) << 2)), kMessage);
        put(rt, frame.at(0x30u), call_virtual_at(rt, ctx, next, 0x30u, {next}));
        std::uint32_t slot = frame.at(0x28u);
        std::uint32_t index = kind + 2u;
        std::uint32_t element = args + (index << 2);
        do {
            const bool in_range = signed_of(index) < signed_of(count);
            ++index;
            if (in_range && word(rt, element) != 0u) {
                call_set(rt, ctx, 0x0884D660u, {frame.at(0xCu), element});
                const std::uint32_t source = checked(rt, ctx, word(rt, frame.at(0xCu)), 0x08C9A194u);
                put(rt, slot, call_set(rt, ctx, 0x0884D3D8u, {source}));
                call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
            }
            slot += 4u;
            element += 4u;
        } while (slot != frame.at(0x30u));
        third = word(rt, frame.at(0x30u));
    }
    const std::uint32_t built = call_set(rt, ctx, 0x0885CBCCu, {owner, word(rt, first_string), word(rt, second_string), third,
                                                                  word(rt, frame.at(0x34u)), frame.at(0x28u)});
    const std::uint32_t result = frame.at(8u);
    call_set(rt, ctx, 0x08A85B1Cu, {result, built});
    keep_temporary(rt, ctx, word(rt, frame.at(0x38u)), result);
    release_string(rt, ctx, second_string, kEmpty, 0x08B8542Cu, frame.at(1u));
    release_string(rt, ctx, first_string, kEmpty, 0x08B8542Cu, frame.at(0u));
}

// func_08834670(registry): builds the table of six entries and hands it to the registry. A 0x5C-byte object is built from
// the string at frame +0x28 (literal 0x08C9B490; func_08A54F3C(object, string, 8)) and wrapped (func_08A52AB0) in the
// handle at frame +0x58. Then, for each of five names (literals 0x08C9B49C / 0x08C9B4B0 / 0x08C9B4BC / 0x08C9B4CC /
// 0x08C9B4E8, with the handle's word checked against 0x08C9B1D4), a 0x38-byte object is built (func_08A5BC20; from the
// literal 0x08C9B4A8 or 0x08C9B4E0 by func_08A52660; first word vtable 0x08CB7840 for the first three, 0x08CB7F98 for the
// last two), wrapped, and entered with func_08A535A4(handle word, name, wrapper, -1, index) for the indices 1, 2, 3, 4, 6.
// The temporaries are destroyed (func_08A5B9EC / func_08A69D68) after each. At the end func_08A7CE50(registry->+0's
// object, handle) runs and the handle's word is stored at 0x08CE2B90.
void func_08834670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t registry = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    constexpr std::uint32_t kStringFrom = 0x08A69B3Cu;
    constexpr std::uint32_t kStringDestroy = 0x08A69D68u;
    constexpr std::uint32_t kMessage = 0x08C9B1D4u;
    const std::uint32_t handle = frame.at(0x58u);
    call_set(rt, ctx, kStringFrom, {frame.at(0x28u), 0x08C9B490u});
    const std::uint32_t big = call_set(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
    call_set(rt, ctx, 0x08A54F3Cu, {big, frame.at(0x28u), 8u});
    put(rt, frame.at(0x2Cu), big);
    call_set(rt, ctx, 0x08A52AB0u, {handle, frame.at(0x2Cu)});
    call_set(rt, ctx, kStringDestroy, {frame.at(0x28u)});
    // One entry: the name string `name`, a 0x38-byte object from the string `temp` and wrapped at `wrapper`.
    const auto entry = [&](std::uint32_t name, std::uint32_t name_literal, std::uint32_t temp, std::uint32_t temp_literal,
                           std::uint32_t vtable, std::uint32_t wrapper, std::uint32_t slot, std::uint32_t index) {
        const std::uint32_t holder = checked(rt, ctx, word(rt, handle), kMessage);
        call_set(rt, ctx, kStringFrom, {name, name_literal});
        const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
        call_set(rt, ctx, kStringFrom, {temp, temp_literal});
        call_set(rt, ctx, 0x08A52660u, {object, temp});
        call_set(rt, ctx, kStringDestroy, {temp});
        put(rt, object, vtable);
        put(rt, slot, object);
        call_set(rt, ctx, 0x08A52AB0u, {wrapper, slot});
        call_set(rt, ctx, 0x08A535A4u, {holder, name, wrapper, 0xFFFFFFFFu, index});
        call_set(rt, ctx, kDestroy, {wrapper});
        call_set(rt, ctx, kStringDestroy, {name});
    };
    entry(frame.at(0x54u), 0x08C9B49Cu, frame.at(0x20u), 0x08C9B4A8u, 0x08CB7840u, frame.at(0x50u), frame.at(0x24u), 1u);
    entry(frame.at(0x4Cu), 0x08C9B4B0u, frame.at(0x18u), 0x08C9B4A8u, 0x08CB7840u, frame.at(0x48u), frame.at(0x1Cu), 2u);
    entry(frame.at(0x44u), 0x08C9B4BCu, frame.at(0x10u), 0x08C9B4A8u, 0x08CB7840u, frame.at(0x40u), frame.at(0x14u), 3u);
    entry(frame.at(0x3Cu), 0x08C9B4CCu, frame.at(8u), 0x08C9B4E0u, 0x08CB7F98u, frame.at(0x38u), frame.at(0xCu), 4u);
    entry(frame.at(0x34u), 0x08C9B4E8u, frame.at(0u), 0x08C9B4E0u, 0x08CB7F98u, frame.at(0x30u), frame.at(4u), 6u);
    const std::uint32_t owner = checked(rt, ctx, word(rt, registry), 0x08C9B4F8u);
    call_set(rt, ctx, 0x08A7CE50u, {owner, handle});
    put(rt, 0x08CE2B90u, word(rt, handle));
    call_set(rt, ctx, kDestroy, {handle});
}

// func_08838990(target, key, mode, holder): nothing when mode < 5. Five objects held at holder +0..+0x10 (each checked
// against 0x08C9BA2C) give: selector = obj0's virtual slot 0x3C; two strings in the frame (obj1 and obj3 slot 0xC, +0xC and
// +0x10); obj2's slot 0x3C and obj4's slot 0x3C results; func_088378F0(frame temp, key) builds a value object that
// func_08837498(temp->+0, 0) turns into a list owner whose slot 0x10(owner, obj2 result) gives a table of 0x10-byte entries
// {halfword +0, vector of 0x1C-byte records +4}. For each entry whose halfword equals the selector, each record
// {name +0, bytes vector +4, bytes vector +0x10}: when func_08B35FD8(func_088374A8(name), frame string) is not 0 nothing
// happens; else the bytes vector at +4 (or the one at +0x10 when func_08B35FD8(string, 0x08C9BB38) was not 0) is searched
// for a 2-byte element whose second byte equals obj4's result, and if missing a 2-byte element {halfword read from
// 0x08CDC614 / 0x08CDC610, that byte} is appended (inline, or func_08B8D1C8 / func_08B8D028 when full). Finally
// func_08A85B1C(temp2, any appended) builds the value assigned to target, and both frame strings are released.
void func_08838990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (signed_of(mode) < 5) return;
    const std::uint32_t object0 = checked(rt, ctx, word(rt, holder), 0x08C9BA2Cu);
    const std::uint32_t selector = call_virtual_at(rt, ctx, object0, 0x3Cu, {object0});
    const std::uint32_t object1 = checked(rt, ctx, word(rt, holder + 4u), 0x08C9BA2Cu);
    call_virtual_at(rt, ctx, object1, 0xCu, {frame.at(0xCu), object1});
    const std::uint32_t object2 = checked(rt, ctx, word(rt, holder + 8u), 0x08C9BA2Cu);
    const std::uint32_t second = call_virtual_at(rt, ctx, object2, 0x3Cu, {object2});
    const std::uint32_t object3 = checked(rt, ctx, word(rt, holder + 0xCu), 0x08C9BA2Cu);
    call_virtual_v1(rt, ctx, object3, 0xCu, {frame.at(0x10u), object3});
    const std::uint32_t flag = call_set(rt, ctx, 0x08B35FD8u, {word(rt, frame.at(0x10u)), 0x08C9BB38u});
    const std::uint32_t object4 = checked(rt, ctx, word(rt, holder + 0x10u), 0x08C9BA2Cu);
    const std::uint32_t wanted = call_virtual_at(rt, ctx, object4, 0x3Cu, {object4});
    call_set(rt, ctx, 0x088378F0u, {frame.at(8u), key});
    const std::uint32_t built = checked(rt, ctx, word(rt, frame.at(8u)), 0x08C9BAB4u);
    const std::uint32_t owner = call_set(rt, ctx, 0x08837498u, {built, 0u});
    call_set(rt, ctx, kDestroy, {frame.at(8u)});
    const std::uint32_t found_list = call_virtual_at(rt, ctx, owner, 0x10u, {owner, second});
    const std::uint32_t table = word(rt, found_list + 4u);
    const std::uint32_t entries = word(rt, table);
    std::uint32_t entries_end = word(rt, table + 4u);
    ctx.gpr[5] = 0u;
    std::uint32_t appended = 0u;
    // The search of a byte vector [begin, end) for a 2-byte element whose second byte is `wanted`.
    const auto contains = [&](std::uint32_t begin, std::uint32_t end) {
        for (std::uint32_t at = begin; at != end; at += 2u) {
            if (byte_u(rt, at + 1u) == wanted) return true;
        }
        return false;
    };
    // Appends {halfword at `source`, wanted} to the byte vector `list` (begin +0, end +4, capacity +8 of the vector at list+4).
    const auto append = [&](std::uint32_t list, std::uint32_t source, std::uint32_t grow) {
        put16(rt, frame.at(2u), half_u(rt, source));
        put8(rt, frame.at(3u), wanted);
        ctx.gpr[5] = word(rt, list + 4u);
        if (ctx.gpr[5] != word(rt, list + 8u)) {
            std::uint32_t at = ctx.gpr[5];
            if (at != 0u) {
                put8(rt, at, byte_u(rt, frame.at(2u)));
                put8(rt, at + 1u, byte_u(rt, frame.at(3u)));
                at = word(rt, list + 4u);
            }
            put(rt, list + 4u, at + 2u);
        } else {
            call_set(rt, ctx, grow, {list, ctx.gpr[5], frame.at(2u)});
        }
        appended = 1u;
    };
    if (entries != entries_end) {
        std::uint32_t entry = entries;
        do {
            if (half_u(rt, entry) == selector) {
                std::uint32_t record = word(rt, entry + 4u);
                if (word(rt, entry + 8u) != record) {
                    const std::uint32_t list = entry + 4u;
                    do {
                        const std::uint32_t name = call_set(rt, ctx, 0x088374A8u, {word(rt, record)});
                        const std::uint32_t equal = call_set(rt, ctx, 0x08B35FD8u, {name, word(rt, frame.at(0xCu))});
                        if (equal == 0u) {
                            if (flag != 0u) {
                                const std::uint32_t bytes = word(rt, record + 0x10u);
                                if (!contains(bytes, word(rt, record + 0x14u))) {
                                    append(record + 0x10u, 0x08CDC610u, 0x08B8D028u);
                                }
                            } else {
                                const std::uint32_t bytes = word(rt, record + 4u);
                                if (!contains(bytes, word(rt, record + 8u))) {
                                    append(record + 4u, 0x08CDC614u, 0x08B8D1C8u);
                                }
                            }
                        }
                        record += 0x1Cu;
                    } while (word(rt, list + 4u) != record);
                    entries_end = word(rt, table + 4u);
                }
            }
            entry += 0x10u;
        } while (entry != entries_end);
    }
    const std::uint32_t result = frame.at(4u);
    call_set(rt, ctx, 0x08A85B1Cu, {result, appended});
    keep_temporary(rt, ctx, target, result);
    release_string(rt, ctx, frame.at(0x10u), 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
    release_string(rt, ctx, frame.at(0xCu), 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// func_088541F0(registry): seven registrations of the same shape. Each builds a string from a literal (func_08A69B3C) at a
// frame slot, a 0x5C-byte object (func_08A5BC20(0x5C, 4); func_08A54F3C(object, string, size)) wrapped by
// func_08A52AB0 into the handle at frame +0x38, destroys the string (func_08A69D68), names the handle
// (func_08854000(handle, literal)), enters it with func_08A7CE50(registry->+0's object, handle) and keeps the handle's word
// in a global (0x08CE2D08 down to 0x08CE2CF0 in steps of 4) before destroying the handle (func_08A5B9EC). The sizes are
// 0x38, 0x1B0, 0x60, 0x10, 0x38, 0x70 and 0x18.
void func_088541F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t registry = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t handle = frame.at(0x38u);
    const auto registration = [&](std::uint32_t name, std::uint32_t literal, std::uint32_t size, std::uint32_t name_literal,
                                  std::uint32_t global) {
        call_set(rt, ctx, 0x08A69B3Cu, {frame.at(name), literal});
        const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
        call_set(rt, ctx, 0x08A54F3Cu, {object, frame.at(name), size});
        put(rt, frame.at(name + 4u), object);
        call_set(rt, ctx, 0x08A52AB0u, {handle, frame.at(name + 4u)});
        call_set(rt, ctx, 0x08A69D68u, {frame.at(name)});
        call_set(rt, ctx, 0x08854000u, {handle, name_literal});
        const std::uint32_t owner = checked(rt, ctx, word(rt, registry), 0x08C9E0C8u);
        call_set(rt, ctx, 0x08A7CE50u, {owner, handle});
        put(rt, global, word(rt, handle));
        call_set(rt, ctx, kDestroy, {handle});
    };
    registration(0x30u, 0x08C9E0B0u, 0x38u, 0x08C88ED4u, 0x08CE2D08u);
    registration(0x28u, 0x08C9E0D0u, 0x1B0u, 0x08C889B8u, 0x08CE2D04u);
    registration(0x20u, 0x08C9E0E8u, 0x60u, 0x08C8888Cu, 0x08CE2D00u);
    registration(0x18u, 0x08C9E100u, 0x10u, 0x08C88850u, 0x08CE2CFCu);
    registration(0x10u, 0x08C9E114u, 0x38u, 0x08C8879Cu, 0x08CE2CF8u);
    registration(0x08u, 0x08C9E12Cu, 0x70u, 0x08C88640u, 0x08CE2CF4u);
    registration(0x00u, 0x08C9E13Cu, 0x18u, 0x08C885F8u, 0x08CE2CF0u);
}

// func_088F3BC4(self, mode, group): nothing when self->+0x24 is 0 (v0 = 0) or self->+0x28 < 0 (v0 = it). Sets +0x1B8 = 0.5,
// +0x1BC = 100.0, +0x1B4 = 0, reads the matrix func_08B567F4(frame +0) and walks the chain of 64-byte entries at self->+4
// from the halfword self->+8 (the next index is the signed 13-bit field of each entry's first word; only entries whose 3-bit
// field at bit 13 equals `group`, or all when group < 0). Each entry's vector (func_08B5A75C(cursor, entry + 0x20)) is projected
// with the matrix and kept in a 16-byte record {x, y, z, index} at frame +0x8C when -z lies within self +0x30 / +0x2C and the
// scaled box (entry float +8 * 1.2071) stays inside [-1, 1] on both axes. Unless self->+0x3C == 0x48 func_088F360C(self,
// records, 0, count - 1) runs. Then a graphics state set-up (the 0x08B5xxxx calls), a draw of the entry chain at +0x24 / +0x28
// (func_08AF6CE0) and, per mode, func_088F3140 / func_088F381C over the records (mode 0 by a distance of 3.0 in the entry's
// float +8, mode 1 for all), closed by the state restore calls.
void func_088F3BC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[5];
    const std::uint32_t group = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10D0u);
    ctx.gpr[2] = word(rt, self + 0x24u);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = word(rt, self + 0x28u);
    if (signed_of(ctx.gpr[2]) < 0) return;
    const std::uint32_t records = frame.at(0x8Cu);
    put_float(rt, self + 0x1B8u, bits_to_float(0x3F000000u));
    put_float(rt, self + 0x1BCu, bits_to_float(0x42C80000u));
    std::uint32_t cursor = records;
    put(rt, self + 0x1B4u, 0u);
    call_set(rt, ctx, 0x08B56A00u, {1u});
    call_set(rt, ctx, 0x08B567F4u, {frame.at(0u)});
    call_set(rt, ctx, 0x08B56A00u, {0u});
    std::uint32_t index = sext16(half_u(rt, self + 8u));
    const float near = float_at(rt, self + 0x30u);
    const float far = float_at(rt, self + 0x2Cu);
    if (!(signed_of(index) < 0)) {
        std::uint32_t entries = word(rt, self + 4u);
        for (;;) {
            const std::uint32_t offset = index << 6;
            bool process = signed_of(group) < 0;
            if (!process) process = group == ((word(rt, entries + offset) >> 13) & 7u);
            if (process) {
                call_set(rt, ctx, 0x08B5A75Cu, {cursor, entries + offset + 0x20u});
                entries = word(rt, self + 4u);
                const float f9 = float_at(rt, cursor + 8u);
                const float negated = -f9;
                if (near < negated && negated < far) {
                    const float f10 = bits_to_float(0xBF800000u);
                    const float f6 = f10 / f9;
                    float f3 = float_at(rt, cursor);
                    float f4 = float_at(rt, cursor + 4u);
                    const float f7 = float_at(rt, frame.at(0x14u));
                    float f2 = float_at(rt, frame.at(4u));
                    float f1 = psp_mul(f4, f7);
                    f2 = psp_mul(f3, f2);
                    float f5 = float_at(rt, entries + offset + 8u);
                    const float f8 = float_at(rt, frame.at(0u));
                    f5 = psp_mul(f5, bits_to_float(0x3F9A827Au));
                    float f0 = float_at(rt, frame.at(0x10u));
                    f2 = f2 + f1;
                    f1 = float_at(rt, frame.at(0x24u));
                    f4 = psp_mul(f4, f0);
                    f3 = psp_mul(f3, f8);
                    f1 = psp_mul(f9, f1);
                    f0 = float_at(rt, frame.at(0x20u));
                    f3 = f3 + f4;
                    f0 = psp_mul(f9, f0);
                    f2 = f2 + f1;
                    f3 = f3 + f0;
                    f5 = psp_mul(f6, f5);
                    f2 = psp_mul(f6, f2);
                    f3 = psp_mul(f6, f3);
                    f1 = psp_mul(f8, f5);
                    if (f1 < bits_to_float(0x401A827Au)) {
                        const float f6b = psp_mul(f7, f5);
                        const float one = bits_to_float(0x3F800000u);
                        if (f3 - f1 < one && f10 < f3 + f1 && f2 - f6b < one && f10 < f2 + f6b) {
                            put(rt, cursor + 0xCu, index);
                            cursor += 0x10u;
                        }
                    }
                }
            }
            index = static_cast<std::uint32_t>(signed_of(word(rt, entries + offset) << 19) >> 19);
            if (signed_of(index) < 0) break;
        }
    }
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(cursor - records) >> 4);
    if (word(rt, self + 0x3Cu) != 0x48u) call_set(rt, ctx, 0x088F360Cu, {self, records, 0u, count - 1u});
    call_set(rt, ctx, 0x08B5711Cu, {0x481u});
    call_set(rt, ctx, 0x08B58874u, {0x6Fu});
    call_setf(rt, ctx, 0x08B5877Cu, {6u}, {0.0f});
    call_set(rt, ctx, 0x08B54FB8u, {0x64u});
    call_set(rt, ctx, 0x08B54FB8u, {0x78u});
    call_set(rt, ctx, 0x08B58874u, {0x8002u});
    call_set(rt, ctx, 0x08B58874u, {0x70u});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Cu});
    call_set(rt, ctx, 0x08B58A74u, {0u});
    call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {0u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {2u, 0x8000u});
    call_set(rt, ctx, 0x08B55AACu, {1u, 0u});
    call_set(rt, ctx, 0x08B55AACu, {5u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {6u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {4u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {3u, 1u});
    call_set(rt, ctx, 0x08B55B90u, {});
    const std::uint32_t chain = word(rt, self + 0x24u);
    if (chain != 0u && !(signed_of(word(rt, self + 0x28u)) < 0)) {
        call_set(rt, ctx, 0x08B58874u, {2u});
        const std::uint32_t position = word(rt, self + 0x28u);
        const std::uint32_t chain_again = word(rt, self + 0x24u);
        const std::uint32_t base = word(rt, chain_again + 0x18u);
        call_set(rt, ctx, 0x08AF6CE0u, {base + position * 152u, 0u, base, chain_again});
    } else {
        call_set(rt, ctx, 0x08B54FB8u, {2u});
    }
    if (word(rt, self + 0x3Cu) == 0x48u) {
        call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 1u});
    } else {
        call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 0x303u});
    }
    call_set(rt, ctx, 0x08B569A0u, {});
    put(rt, self + 0x1B0u, frame.at(0x40u));
    call_set(rt, ctx, 0x08B563C0u, {});
    call_set(rt, ctx, 0x08B56434u, {});
    call_set(rt, ctx, 0x08B567F4u, {word(rt, self + 0x1B0u)});
    call_set(rt, ctx, 0x08B56510u, {});
    if (mode == 0u) {
        if (signed_of(count) > 0) {
            std::uint32_t first = records;
            std::uint32_t second = records;
            std::uint32_t pointer = frame.at(0x98u);
            const float three = bits_to_float(0x40400000u);
            for (std::uint32_t done = 0u; done != count; ++done, first += 0x10u, second += 0x10u, pointer += 0x10u) {
                const std::uint32_t entry = (word(rt, pointer) << 6) + word(rt, self + 4u);
                if (three < float_at(rt, entry + 8u)) {
                    call_set(rt, ctx, 0x088F3140u, {self, first});
                } else {
                    call_set(rt, ctx, 0x088F381Cu, {self, second});
                }
            }
        }
    } else if (mode == 1u) {
        if (signed_of(count) > 0) {
            std::uint32_t at = records;
            for (std::uint32_t done = 0u; done != count; ++done, at += 0x10u) call_set(rt, ctx, 0x088F3140u, {self, at});
        }
    }
    call_set(rt, ctx, 0x08B56940u, {});
    call_set(rt, ctx, 0x08B54FB8u, {0x8002u});
    call_set(rt, ctx, 0x08B58A74u, {1u});
    call_set(rt, ctx, 0x08B58874u, {0x6Du});
}

// func_0890F1C0(out, owner, key) -> 0 or 1: func_08B0EC60(frame +0x60) fills a 0x3C-byte record that is copied to frame
// +0x18; a non-zero first word returns 0. func_089624EC(key) gives a pair {object +0, shared-pointer control block +4}
// (an empty one leaves both frame words +0x10 / +0x14 zero). Otherwise, under the lock at object +0x6C (func_08B34888 /
// func_08B34B24), func_08B2CB18(object + 8, record word +0x10, 0x80) gives a handle h; a 0x18-byte control block {vtable
// 0x08CC2F90, counts 1 / 1, h, object, shared block} is allocated (func_08B62344) and the shared block's use count goes up
// (net +1 after the temporaries are released) and func_08B8E380(frame +0x14, h, h) links it; frame +0x10 = h. out is
// assigned {frame +0x10, frame +0x14} like a shared pointer. When out's pointer is non-zero a closure {0x0890EF6C, out,
// pointer, record word +0x10} is registered with func_08AC0C38(owner, closure pointer, closure, 1) and func_08AC0B50() runs.
void func_0890F1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    GuestFrame frame(ctx, 0xD0u);
    call_set(rt, ctx, 0x08B0EC60u, {frame.at(0x60u)});
    copy_words16(rt, frame.at(0x18u), frame.at(0x60u), 0x30u);
    const std::uint32_t tail_second = word(rt, frame.at(0x98u));
    const std::uint32_t tail_first = word(rt, frame.at(0x90u));
    const std::uint32_t tail_third = word(rt, frame.at(0x94u));
    put(rt, frame.at(0x48u) + 8u, tail_second);
    put(rt, frame.at(0x48u), tail_first);
    put(rt, frame.at(0x48u) + 4u, tail_third);
    const std::uint32_t handle_word = word(rt, frame.at(0x28u));
    if (word(rt, frame.at(0x18u)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t pair = call_set(rt, ctx, 0x089624ECu, {key});
    const std::uint32_t shared = word(rt, pair);
    std::uint32_t result_pointer;
    if (shared == 0u) {
        put(rt, frame.at(0x10u), 0u);
        put(rt, frame.at(0x14u), 0u);
        result_pointer = 0u;
    } else {
        const std::uint32_t lock = shared + 0x6Cu;
        call_set(rt, ctx, 0x08B34888u, {lock});
        const std::uint32_t handle = call_set(rt, ctx, 0x08B2CB18u, {shared + 8u, handle_word, 0x80u});
        call_set(rt, ctx, 0x08B34B24u, {lock});
        const std::uint32_t block = word(rt, pair + 4u);
        const std::uint32_t object = word(rt, pair);
        if (block != 0u) count_up(rt, ctx, block + 4u);
        put(rt, frame.at(0x10u), handle);
        if (block != 0u) {
            count_up(rt, ctx, block + 4u);
            count_up(rt, ctx, block + 4u);
        }
        const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, control + 4u, 1u);
        put(rt, control, 0x08CC2F90u);
        put(rt, control + 8u, 1u);
        put(rt, control + 0xCu, handle);
        put(rt, control + 0x10u, object);
        put(rt, control + 0x14u, block);
        if (block != 0u) count_up(rt, ctx, block + 4u);
        put(rt, frame.at(0x14u), control);
        if (block != 0u) release_shared(rt, ctx, block);
        if (word(rt, frame.at(0x14u)) == 0u) {
            const std::uint32_t inner_lock = object + 0x6Cu;
            call_set(rt, ctx, 0x08B34888u, {inner_lock});
            call_set(rt, ctx, 0x08B2CE7Cu, {object + 8u, handle});
            const std::uint32_t count = call_set(rt, ctx, 0x08B2D1A8u, {object + 8u});
            if (count == word(rt, object + 0x68u)) call_set(rt, ctx, 0x08B34CA0u, {inner_lock});
            call_set(rt, ctx, 0x08B34B24u, {inner_lock});
        }
        if (block != 0u) release_shared(rt, ctx, block);
        call_set(rt, ctx, 0x08B8E380u, {frame.at(0x14u), handle, handle});
        if (block != 0u) release_shared(rt, ctx, block);
        result_pointer = word(rt, frame.at(0x10u));
    }
    ctx.gpr[2] = result_pointer;
    std::uint32_t block = word(rt, frame.at(0x14u));
    const std::uint32_t old_block = word(rt, out + 4u);
    put(rt, out, ctx.gpr[2]);
    if (block != old_block) {
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t previous = word(rt, out + 4u);
        if (previous != 0u) release_shared(rt, ctx, previous);
        put(rt, out + 4u, block);
        block = word(rt, frame.at(0x14u));
    }
    if (block != 0u) release_shared(rt, ctx, block);
    ctx.gpr[2] = word(rt, out);
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(8u), ctx.gpr[2]);
    put(rt, frame.at(0u), 0x0890EF6Cu);
    put(rt, frame.at(0xCu), handle_word);
    put(rt, frame.at(4u), out);
    call_set(rt, ctx, 0x08AC0C38u, {owner, frame.at(8u), frame.at(0u), 1u});
    call_set(rt, ctx, 0x08AC0B50u, {});
    ctx.gpr[2] = 1u;
}

// func_08921F70(self, key, unused, mode): self->+0x240 = key, bytes +0x24C / +0x24D = 0, self->+0x244 = func_08920064(self,
// key, mode). Without that object, +0x248 = -1. With it: the 16 bytes of its virtual slot 0x38 result go to +0x50 and those of
// slot 0x3C to +0x60, +0x80 = +0x1F8; func_088D7F54(key, mode) non-zero runs func_08921DAC(self, +0x1FC); a changed mode
// (+0x248) runs func_089262EC over the words +0x250 / +0x25C / +0x260 / +0x254 and is stored. The word +0x1FC selects
// (0x23-entry table, 7 by default) the value stored at +0x284. Then func_08B4E648(self + 0x10); the 0x1D0-byte block at
// self + 0x10 is copied to 13 frame slots, each followed by func_0892632C (when the slot index equals +0x284) or the virtual
// slot 8 of the word at +0x250 + 4 * index; the slot of index +0x284 is copied back. For +0x284 == 1 and byte +0x24C clear,
// +0x80 = 0x4251B9A7; for 4, 35.0. For +0x284 in 3..5 the object func_089260EC(self) answering virtual slot 8 with 8 or 0x10
// non-zero restores another frame block over self + 0x10 and sets byte +0x24E; byte +0xC0 = 1 when that flag changed.
// Last func_08B4A774(self + 0x1E4, self + 0x10, frame {0, 0, 0, 1.0}) and func_0891FF58(self).
void func_08921F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[7];
    GuestFrame frame(ctx, 0x17D0u);
    put(rt, self + 0x240u, key);
    put8(rt, self + 0x24Cu, 0u);
    put8(rt, self + 0x24Du, 0u);
    const std::uint32_t object = call_set(rt, ctx, 0x08920064u, {self, key, mode});
    put(rt, self + 0x244u, object);
    const std::uint32_t area = self + 0x10u;
    if (object == 0u) {
        put(rt, self + 0x248u, 0xFFFFFFFFu);
    } else {
        const std::uint32_t first = call_virtual_at(rt, ctx, object, 0x38u, {object});
        for (std::uint32_t index = 0u; index < 4u; ++index) put(rt, self + 0x50u + index * 4u, word(rt, first + index * 4u));
        const std::uint32_t holder = word(rt, self + 0x244u);
        const std::uint32_t second = call_virtual_at(rt, ctx, holder, 0x3Cu, {holder});
        for (std::uint32_t index = 0u; index < 4u; ++index) put(rt, self + 0x60u + index * 4u, word(rt, second + index * 4u));
        put(rt, self + 0x80u, word(rt, self + 0x1F8u));
        if (call_set(rt, ctx, 0x088D7F54u, {key, mode}) != 0u) call_set(rt, ctx, 0x08921DACu, {self, word(rt, self + 0x1FCu)});
        if (mode != word(rt, self + 0x248u)) {
            call_set(rt, ctx, 0x089262ECu, {word(rt, self + 0x250u)});
            call_set(rt, ctx, 0x089262ECu, {word(rt, self + 0x25Cu)});
            call_set(rt, ctx, 0x089262ECu, {word(rt, self + 0x260u)});
            call_set(rt, ctx, 0x089262ECu, {word(rt, self + 0x254u)});
            put(rt, self + 0x248u, mode);
        }
        // The value stored at +0x284 for each selector word (7 for anything out of the table).
        static const std::uint8_t kSelected[35] = {0, 3, 7, 7, 7, 5, 4, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                                   7, 7, 7, 7, 7, 7, 7, 7, 1, 7, 2, 2, 9, 9, 0xC, 8, 0xB};
        const std::uint32_t selector = word(rt, self + 0x1FCu);
        put(rt, self + 0x284u, selector < 0x23u ? kSelected[selector] : 7u);
        call_set(rt, ctx, 0x08B4E648u, {area});
        std::uint32_t current = word(rt, self + 0x284u);
        for (std::uint32_t index = 0u; index < 13u; ++index) {
            const std::uint32_t slot = frame.at(0x10u) + index * 0x1D0u;
            copy_words16(rt, slot, area, 0x1D0u);
            if (current == index) {
                call_set(rt, ctx, 0x0892632Cu, {word(rt, self + 0x250u + index * 4u), slot});
                current = word(rt, self + 0x284u);
            } else if (index != 0xCu) {
                const std::uint32_t entry = word(rt, self + 0x250u + index * 4u);
                call_virtual_at(rt, ctx, entry, 8u, {entry, slot});
                current = word(rt, self + 0x284u);
            }
        }
        copy_words16(rt, area, frame.at(0x10u) + current * 0x1D0u, 0x1D0u);
        current = word(rt, self + 0x284u);
        if (current == 1u && byte_u(rt, self + 0x24Cu) == 0u) put_float(rt, self + 0x80u, bits_to_float(0x4251B9A7u));
        if (current == 4u) put_float(rt, self + 0x80u, bits_to_float(0x420C0000u));
        const std::uint32_t previous = byte_u(rt, self + 0x24Eu);
        put8(rt, self + 0x24Eu, 0u);
        std::uint32_t flag = 0u;
        if (current - 3u < 3u) {
            std::uint32_t holder_object = call_set(rt, ctx, 0x089260ECu, {self});
            bool accepted = call_virtual_at(rt, ctx, holder_object, 8u, {holder_object, 8u}) != 0u;
            if (!accepted) {
                holder_object = call_set(rt, ctx, 0x089260ECu, {self});
                accepted = call_virtual_at(rt, ctx, holder_object, 8u, {holder_object, 0x10u}) != 0u;
                if (!accepted) flag = byte_u(rt, self + 0x24Eu);
            }
            if (accepted) {
                copy_words16(rt, area, frame.at(0x1230u), 0x1D0u);
                flag = 1u;
                put8(rt, self + 0x24Eu, 1u);
            }
        }
        if ((flag & 0xFFu) != previous) put8(rt, self + 0xC0u, 1u);
    }
    put_float(rt, frame.at(0xCu), bits_to_float(0x3F800000u));
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    call_set(rt, ctx, 0x08B4A774u, {self + 0x1E4u, area, frame.at(0u)});
    call_set(rt, ctx, 0x0891FF58u, {self});
}

// func_0894106C(self, a1, w0, w1, mode, f12 = x, f13 = y, f14 = z, f15 = angle) -> 0: the object's frame setup. With
// s = self->+8 and b = self->+0xC, func_089545BC() runs; for mode 2 / 1 the offset f20 = b->+0 / -b->+0x70 is rotated by
// func_08B498AC(angle) into x and y. The vector {x, z, -y} with the words w0 / w1 is passed to the virtual slot 8 of the
// object at (self->+0)->+0x14 (when set); s +0x10 / +0x14 / +0x18 / +0x690 / +0x694 and the out cells +0x6FC.. are set and
// func_0893C3E4 / func_08954870 run. For the four records func_08955300(self, i): a position from the vector's
// transform (func_0893C780), the record's bytes +0xE9..+0xED / floats +0xBC / +0xD0 from the transform's fields
// (func_08954618 / func_08954670 / func_08954688); counts go to s +0x69D / +0x69E and bit 4 of s +0xC is cleared. A second pass
// sets each record's floats +0x54 / +0x58 / +0xCC / +0x10 / +0x14 / +0x18 / +0x1C / +0xC from b via func_0894E9B4 /
// func_0894E714 / func_0893D0F4 / func_0893D064. Then func_08945F6C(self, 1), the angle func_08B4A430 added to s +0x6FC,
// func_0893C3E4 again, s +0x18, func_08954870 and the copies into s +0x764.. / +0x770; unless the byte s +3 is 4,
// func_0895A004(*self, byte s +0).
void func_0894106C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first_word = ctx.gpr[6];
    const std::uint32_t second_word = ctx.gpr[7];
    const std::uint32_t mode = ctx.gpr[8];
    float f21 = ctx.fpr[12];
    float f22 = ctx.fpr[13];
    const float f23 = ctx.fpr[14];
    const float f24 = ctx.fpr[15];
    GuestFrame frame(ctx, 0xB0u);
    const std::uint32_t entry = word(rt, self);
    const std::uint32_t s2 = word(rt, self + 8u);
    const std::uint32_t s3 = word(rt, self + 0xCu);
    call_setf(rt, ctx, 0x089545BCu, {frame.at(8u), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]}, {f21, f22, f23, f24});
    float f20 = 0.0f;
    float negated = -f22;
    if (mode == 2u || mode == 1u) {
        f20 = mode == 2u ? float_at(rt, s3) : -float_at(rt, s3 + 0x70u);
        call_setf(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)}, {f24});
        float f1 = float_at(rt, frame.at(0u));
        float f0 = float_at(rt, frame.at(4u));
        f1 = psp_mul(f20, f1);
        f0 = psp_mul(f20, f0);
        f21 = f21 + f1;
        f22 = f22 - f0;
        negated = -f22;
    }
    put_float(rt, frame.at(0xCu), f23);
    put_float(rt, frame.at(0x10u), negated);
    put(rt, frame.at(0x38u), first_word);
    put(rt, frame.at(0x3Cu), second_word);
    put_float(rt, frame.at(8u), f21);
    const std::uint32_t listener = word(rt, entry + 0x14u);
    if (listener != 0u) call_virtual_at(rt, ctx, listener, 8u, {listener, frame.at(8u)});
    const std::uint32_t low = word(rt, frame.at(0x38u));
    const std::uint32_t high = word(rt, frame.at(0x3Cu));
    f20 = float_at(rt, frame.at(0x14u));
    put_float(rt, s2 + 0x10u, f21);
    std::uint32_t kept_a = 0u;
    std::uint32_t kept_b = 0u;
    put(rt, frame.at(0x40u), s2 + 0x6FCu);
    put(rt, frame.at(0x54u), s2 + 0x1Cu);
    put(rt, s2 + 0x690u, low);
    put(rt, s2 + 0x694u, high);
    put(rt, frame.at(0x50u), s2 + 0x28u);
    put(rt, frame.at(0x4Cu), s2 + 0x34u);
    put_float(rt, s2 + 0x14u, f22);
    put(rt, frame.at(0x48u), s2 + 0x700u);
    put(rt, frame.at(0x44u), s2 + 0x704u);
    float f0 = float_at(rt, s3 + 0x5Cu);
    put_float(rt, s2 + 0x704u, f24);
    f0 = f20 + f0;
    put(rt, s2 + 0x6FCu, 0u);
    put_float(rt, s2 + 0x18u, f0);
    put(rt, s2 + 0x700u, 0u);
    call_set(rt, ctx, 0x0893C3E4u, {word(rt, frame.at(0x54u)), word(rt, frame.at(0x50u)), word(rt, frame.at(0x4Cu)),
                                    word(rt, frame.at(0x40u)), word(rt, frame.at(0x48u)), word(rt, frame.at(0x44u))});
    call_set(rt, ctx, 0x08954870u, {self});
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        const std::uint32_t transform = call_set(rt, ctx, 0x0893C780u, {self, index});
        float f3 = float_at(rt, record + 4u);
        float f1 = float_at(rt, s2 + 0x14u);
        float f2 = float_at(rt, s2 + 0x18u);
        const float f4 = float_at(rt, record + 8u);
        f1 = f1 + f3;
        f0 = float_at(rt, s2 + 0x10u);
        f3 = float_at(rt, record);
        f2 = f2 + f4;
        f1 = -f1;
        f0 = f0 + f3;
        put_float(rt, transform + 4u, f2);
        put_float(rt, transform, f0);
        put_float(rt, transform + 8u, f1);
        put(rt, transform + 0x30u, word(rt, s2 + 0x690u));
        put(rt, transform + 0x34u, word(rt, s2 + 0x694u));
        const std::uint32_t object = word(rt, entry + 0x14u);
        if (object != 0u) call_virtual_at(rt, ctx, object, 8u, {object, transform});
        put_float(rt, record + 0xD0u, bits_to_float(0x3F800000u));
        if (byte_u(rt, transform + 0x2Cu) != 0u) {
            f0 = float_at(rt, record + 8u);
            put8(rt, record + 0xEDu, 0u);
            f0 = f20 + f0;
            put8(rt, record + 0xE9u, 0u);
            put_float(rt, record + 0xBCu, f0);
            put8(rt, record + 0xEAu, 0u);
            put8(rt, record + 0xEBu, 0u);
            put8(rt, record + 0xECu, 0u);
        } else {
            f0 = float_at(rt, transform + 0xCu);
            put_float(rt, record + 0xBCu, f0);
            const std::uint32_t kind = call_set(rt, ctx, 0x08954618u, {transform});
            put8(rt, record + 0xE9u, kind);
            put8(rt, record + 0xEAu, byte_u(rt, transform + 0x2Eu));
            put8(rt, record + 0xEBu, byte_u(rt, transform + 0x2Fu));
            const std::uint32_t bit = byte_u(rt, transform + 0x2Fu) & 1u;
            kept_b += bit;
            const std::uint32_t flagged = call_set(rt, ctx, 0x08954670u, {transform});
            if (flagged == 0u) {
                put8(rt, record + 0xEDu, 0u);
            } else {
                ++kept_a;
                put8(rt, record + 0xEDu, 1u);
            }
            const std::uint32_t reached = call_set(rt, ctx, 0x08954688u, {transform, byte_u(rt, record + 0xE9u)});
            put8(rt, record + 0xECu, reached != 0u ? 0u : 1u);
        }
    }
    const std::uint32_t flags = byte_u(rt, s2 + 0xCu);
    put8(rt, s2 + 0x69Du, kept_a);
    put8(rt, s2 + 0xCu, flags & ~0x10u);
    put8(rt, s2 + 0x69Eu, kept_b);
    const float half = bits_to_float(0x3F000000u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        const std::uint32_t pair = index >> 1;
        const std::uint32_t inner = s3 + pair * 116u;
        float f1 = float_at(rt, s3 + pair * 4u + 0x13D0u);
        f1 = psp_mul(f1, half);
        f0 = float_at(rt, inner + 0x3A4u);
        put8(rt, record + 0x10Cu, 0u);
        f1 = f1 + f0;
        put_float(rt, record + 0x58u, f1);
        put_float(rt, record + 0x54u, f1);
        f0 = float_at(rt, inner + 0x370u);
        put_float(rt, record + 0xCCu, f0);
        f0 = call_setf(rt, ctx, 0x0894E9B4u, {s3, index}, {0.0f, 0.0f}).f0;
        put_float(rt, record + 0x10u, f0);
        put(rt, record + 0x14u, 0u);
        put(rt, record + 0x18u, 0u);
        const std::uint32_t shape = call_set(rt, ctx, 0x0894E714u, {});
        if (shape == 2u) {
            f0 = float_at(rt, record + 0x10u);
            put_float(rt, record + 0x18u, 0.0f);
            put_float(rt, record + 0x14u, f0);
        }
        f0 = call_setf(rt, ctx, 0x0893D0F4u, {s3, index}, {0.0f, 0.0f}).f0;
        put_float(rt, record + 0x1Cu, f0);
        f0 = call_setf(rt, ctx, 0x0893D064u, {s3, index}, {0.0f, 0.0f}).f0;
        put_float(rt, record + 0xCu, f0);
    }
    call_set(rt, ctx, 0x08945F6Cu, {self, 1u});
    f20 = float_at(rt, s2 + 0x708u);
    f0 = float_at(rt, s2 + 0x70Cu);
    put_float(rt, s2 + 0x6FCu, f20);
    put_float(rt, s2 + 0x700u, f0);
    const float f1 = float_at(rt, s3 + 0x60u);
    float f12 = float_at(rt, s3 + 0x64u);
    const float f13 = float_at(rt, s3 + 0x18u);
    f12 = f1 - f12;
    f0 = call_setf(rt, ctx, 0x08B4A430u, {}, {f12, f13}).f0;
    f20 = f20 + f0;
    put_float(rt, s2 + 0x6FCu, f20);
    call_set(rt, ctx, 0x0893C3E4u, {word(rt, frame.at(0x54u)), word(rt, frame.at(0x50u)), word(rt, frame.at(0x4Cu)),
                                    word(rt, frame.at(0x40u)), word(rt, frame.at(0x48u)), word(rt, frame.at(0x44u))});
    float g1 = float_at(rt, s3 + 0x5Cu);
    const float g2 = float_at(rt, s2 + 0x3Cu);
    f0 = float_at(rt, s2 + 0x48u);
    g1 = g1 / g2;
    f0 = f0 + g1;
    put_float(rt, s2 + 0x18u, f0);
    call_set(rt, ctx, 0x08954870u, {self});
    const std::uint32_t v0 = word(rt, s2 + 0x690u);
    const std::uint32_t v1 = word(rt, s2 + 0x694u);
    const std::uint32_t c0 = word(rt, s2 + 0x40u);
    const std::uint32_t c1 = word(rt, s2 + 0x44u);
    const std::uint32_t c2 = word(rt, s2 + 0x48u);
    const std::uint32_t kind = sext8(byte_u(rt, s2 + 3u));
    put(rt, s2 + 0x770u, v0);
    put(rt, s2 + 0x774u, v1);
    put(rt, s2 + 0x764u, c0);
    put(rt, s2 + 0x768u, c1);
    put(rt, s2 + 0x76Cu, c2);
    if (kind != 4u) call_set(rt, ctx, 0x0895A004u, {word(rt, self), byte_u(rt, s2)});
    ctx.gpr[2] = 0u;
    (void)kept_a;
}

// func_08946130(self): float bookkeeping over the state block s = self->+8 (and the block self->+0xC). A 3-term dot product
// of s +0x6CC.. with s +0x4C.. / +0x58.. gives (x, y) stored at s +0x6C4 / +0x6C8; r = func_08B49D7C(x*x + y*y) is stored
// at +0x6DC and, when r < 0.2778, +0x6DC = 0 and +0x6D8 = s +0x704; otherwise +0x6D8 = func_08B4A430(-s +0x6CC, s +0x6D0).
// The byte s +0x74E counts down (above 1), or while it is 1 and s +0x6F8 lies within +-pi/2 it is cleared when the angle
// func_0893C60C(s + 0x704, s + 0x6D8) (folded to pi/2) is below 0.1745 or s +0x6DC is 0. Then for the four records
// func_08955300(self, i): +0x2C / +0x30 are set from s +0x6C8 / +0x6C4 and the block's floats times s +0x6F8; func_0894A5BC;
// s +0x714 = s +0x6C4, the phase s2 +0x724 advances by s2 +0x714 * func_0895549C() modulo 8e7; s +0x714 = |s +0x714|;
// func_08945DB0. With byte +0x270 of the second block 3, s2 +0x53C non-zero and s2 +0x6C4 within +-0.2778 nothing more is
// done (v0 = the frame). Otherwise the four records without byte +0x53 get +0x34 = cross product with func_089554A8(i)
// and mark frame words by i / 2, and two floats +0x5FC.. of s2 are set from the pair sums, halved, times a table entry.
void func_08946130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t state = word(rt, self + 8u);
    float f3 = 0.0f;
    float f4 = 0.0f;
    for (std::uint32_t index = 0u; index < 3u; ++index) {
        float f0 = float_at(rt, state + 0x6CCu + index * 4u);
        float f1 = float_at(rt, state + 0x4Cu + index * 4u);
        const float f2 = float_at(rt, state + 0x58u + index * 4u);
        f1 = psp_mul(f0, f1);
        f0 = psp_mul(f0, f2);
        f3 = f3 + f1;
        f4 = f4 + f0;
    }
    float f0 = psp_mul(f3, f3);
    float f12 = psp_mul(f4, f4);
    put_float(rt, state + 0x6C4u, f3);
    f12 = f0 + f12;
    put_float(rt, state + 0x6C8u, f4);
    f0 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f12}).f0;
    put_float(rt, state + 0x6DCu, f0);
    if (f0 < bits_to_float(0x3E8E38E4u)) {
        f0 = float_at(rt, state + 0x704u);
        put(rt, state + 0x6DCu, 0u);
        put_float(rt, state + 0x6D8u, f0);
    } else {
        f12 = float_at(rt, state + 0x6CCu);
        const float f13 = float_at(rt, state + 0x6D0u);
        f0 = call_setf(rt, ctx, 0x08B4A430u, {}, {-f12, f13}).f0;
        put_float(rt, state + 0x6D8u, f0);
    }
    const std::uint32_t timer = byte_u(rt, state + 0x74Eu);
    if (timer >= 2u) {
        put8(rt, state + 0x74Eu, timer - 1u);
    } else if (timer != 0u) {
        float f1 = float_at(rt, state + 0x6F8u);
        const float half_pi = bits_to_float(0x3FC90FDBu);
        if (bits_to_float(0xBFC90FDBu) < f1 && f1 < half_pi) {
            f0 = float_at(rt, state + 0x6DCu);
            if (f0 == 0.0f) {
                put8(rt, state + 0x74Eu, 0u);
            } else {
                f0 = call_setf(rt, ctx, 0x0893C60Cu, {state + 0x704u, state + 0x6D8u}, {}).f0;
                f1 = f0;
                if (f0 < 0.0f) f1 = -f0;
                if (half_pi < f1) {
                    f1 = bits_to_float(0x40490FDBu) - f1;
                    if (f1 < 0.0f) f1 = -f1;
                }
                if (f1 < bits_to_float(0x3E32B8C2u)) put8(rt, state + 0x74Eu, 0u);
            }
        }
    }
    const std::uint32_t block = word(rt, self + 8u);
    std::uint32_t source = word(rt, self + 0xCu);
    const float scale = float_at(rt, block + 0x6F8u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        float a = float_at(rt, source + 0x40u);
        float b = float_at(rt, block + 0x6C8u);
        a = psp_mul(a, scale);
        b = b - a;
        put_float(rt, record + 0x2Cu, b);
        a = float_at(rt, source + 0x30u);
        b = float_at(rt, block + 0x6C4u);
        source += 4u;
        a = psp_mul(scale, a);
        a = a + b;
        put_float(rt, record + 0x30u, a);
    }
    call_set(rt, ctx, 0x0894A5BCu, {self});
    f0 = float_at(rt, state + 0x6C4u);
    put_float(rt, state + 0x714u, f0);
    const std::uint32_t second_block = word(rt, self + 8u);
    const float phase_step = float_at(rt, second_block + 0x714u);
    if (phase_step < 0.0f) {
        f0 = float_at(rt, state + 0x714u);
    } else {
        f0 = call_setf(rt, ctx, 0x0895549Cu, {self}, {}).f0;
        f0 = psp_mul(phase_step, f0);
        const float phase = float_at(rt, second_block + 0x724u);
        const float period = bits_to_float(0x4C969680u);
        f0 = f0 + phase;
        put_float(rt, second_block + 0x724u, f0);
        if (period <= f0) {
            f0 = f0 - period;
            put_float(rt, second_block + 0x724u, f0);
        }
        f0 = float_at(rt, state + 0x714u);
    }
    const float zero = 0.0f;
    if (f0 < zero) put_float(rt, state + 0x714u, -f0);
    call_set(rt, ctx, 0x08945DB0u, {self});
    const std::uint32_t pair = word(rt, self + 0xCu);
    const std::uint32_t kind = byte_u(rt, pair + 0x270u);
    const std::uint32_t second = word(rt, self + 8u);
    if (kind == 3u) {
        ctx.gpr[2] = frame.at(0u);
        const float held = float_at(rt, second + 0x53Cu);
        if (!(held == zero)) {
            const float lean = float_at(rt, second + 0x6C4u);
            if (bits_to_float(0xBE8E38E4u) < lean && lean < bits_to_float(0x3E8E38E4u)) return;
        }
    }
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        if (byte_u(rt, record + 0x53u) == 0u) {
            const std::uint32_t vector = call_set(rt, ctx, 0x089554A8u, {self, index});
            float f1 = float_at(rt, vector);
            float f0b = float_at(rt, vector + 4u);
            const float f2 = float_at(rt, record + 0x30u);
            const float f3b = float_at(rt, record + 0x2Cu);
            f0b = psp_mul(f0b, f2);
            f1 = psp_mul(f1, f3b);
            f0b = f0b - f1;
            put_float(rt, record + 0x34u, f0b);
            put(rt, frame.at((index >> 1) << 2), 1u);
        }
    }
    const std::uint32_t table = pair + 0x270u;
    std::uint32_t entry = pair + 0x320u;
    std::uint32_t target = second;
    const float half = bits_to_float(0x3F000000u);
    for (std::uint32_t index = 0u; index < 2u; ++index, target += 4u) {
        std::uint32_t a1 = index << 1;
        std::uint32_t s0 = a1 ^ 1u;
        if ((s0 ^ 0x7Fu) == 0u) s0 = a1;
        const std::uint32_t flag = word(rt, frame.at(index * 4u));
        ctx.gpr[2] = flag;
        if (flag == 0u) {
            ctx.gpr[2] = byte_u(rt, table + index + 0x14u);
            if (ctx.gpr[2] != 0u) {
                entry += 4u;
                continue;
            }
        }
        const std::uint32_t first = call_set(rt, ctx, 0x08955300u, {self, a1});
        float f20 = float_at(rt, first + 0x34u);
        const std::uint32_t other = call_set(rt, ctx, 0x08955300u, {self, s0});
        float f0b = float_at(rt, other + 0x34u);
        ctx.gpr[2] = byte_u(rt, table + 4u);
        float f1 = float_at(rt, entry);
        f20 = f20 + f0b;
        f0b = psp_mul(f20, half);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[2] = byte_u(rt, table + 2u);
            ctx.gpr[2] = (ctx.gpr[2] << 2) + table;
            f1 = float_at(rt, ctx.gpr[2] + 0xB0u);
        }
        f0b = psp_mul(f1, f0b);
        put_float(rt, target + 0x5FCu, f0b);
        entry += 4u;
    }
}

// func_08948D04(self, input) -> float: a float formula over self's fields. t = input->+0 + func_08947A30(self + 0x270)
// (func_08949A08() runs first); a = ((self->+0x1400 - 0.4) * 0.4 + 0.4), b = (self->+0x1404 + +0x1408) * 0.5 - a,
// c = (self->+0x1350 + +0x1354) * 0.5, scaled by (d^2 / 36000 + 0.85, at least 1) when d = halfword +0x13E0 - 1050 is
// negative, and by (+0x141C / +0x13CC + 1) when byte +0x140F is 7. Two records p / q come from func_0894268C(self, 0 / 2, 0);
// func_08946C58(self, 0 / 2, f12 = x / y) and func_089467AC(p / q + 0x224, f12) give factors; a square-root-style
// func_08B49D7C is applied four times and the result is a product / quotient of those with sqrt(self->+0x68),
// sqrt(input->+4) and constants, three square roots, / 2.5 and * 0.8.
void func_08948D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t input = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    float f20 = float_at(rt, input);
    call_set(rt, ctx, 0x08949A08u, {});
    call_set(rt, ctx, 0x08947A30u, {self + 0x270u});
    const float f25 = f20 + ctx.fpr[0];
    const float point_four = bits_to_float(0x3ECCCCCDu);
    const float half = bits_to_float(0x3F000000u);
    const float one = bits_to_float(0x3F800000u);
    float f2 = float_at(rt, self + 0x1400u);
    float f1 = float_at(rt, self + 0x1408u);
    f2 = f2 - point_four;
    float f3 = float_at(rt, self + 0x1404u);
    const std::uint32_t lifted = sext16(half_u(rt, self + 0x13E0u)) - 0x41Au;
    f3 = f3 + f1;
    f2 = psp_mul(f2, point_four);
    f3 = psp_mul(f3, half);
    f2 = f2 + point_four;
    const float f7 = static_cast<float>(signed_of(lifted));
    float f5 = float_at(rt, self + 0x1354u);
    float f4 = float_at(rt, self + 0x1350u);
    f3 = f3 - f2;
    f4 = f4 + f5;
    float f28 = psp_mul(f4, half);
    const float f30 = f3 + one;
    if (f7 < 0.0f) {
        float f0 = psp_mul(f7, f7);
        f0 = f0 / bits_to_float(0x480D0400u);
        f0 = f0 + bits_to_float(0x3F59999Au);
        if (f0 < one) f0 = one;
        f28 = psp_mul(f28, f0);
    }
    if (byte_u(rt, self + 0x140Fu) == 7u) {
        float f0 = float_at(rt, self + 0x141Cu);
        f1 = float_at(rt, self + 0x13CCu);
        f0 = f0 / f1;
        f0 = f0 + one;
        f28 = psp_mul(f28, f0);
    }
    call_set(rt, ctx, 0x0894268Cu, {self, 0u, 0u});
    const std::uint32_t first = ctx.gpr[2];
    const std::uint32_t second = call_set(rt, ctx, 0x0894268Cu, {self, 2u, 0u});
    float f0 = float_at(rt, self + 0x1404u);
    const float scale = bits_to_float(0x451C4002u);
    float f22 = float_at(rt, self + 0x13D0u);
    f0 = psp_mul(f0, scale);
    f1 = float_at(rt, self + 0x1408u);
    float f21 = float_at(rt, self + 0x13D4u);
    f1 = psp_mul(f1, scale);
    f22 = f22 + f0;
    f3 = float_at(rt, first + 8u);
    f4 = float_at(rt, second + 8u);
    f21 = f21 + f1;
    f22 = psp_mul(f22, half);
    f1 = float_at(rt, self + 0x3A4u);
    f2 = float_at(rt, self + 0x24u);
    f0 = float_at(rt, self + 0x20u);
    f22 = f22 + f1;
    f2 = psp_mul(f3, f2);
    f0 = psp_mul(f4, f0);
    f21 = psp_mul(f21, half);
    f1 = float_at(rt, self + 0x418u);
    f2 = f2 + f0;
    f21 = f21 + f1;
    f0 = float_at(rt, self + 0x1350u);
    f1 = float_at(rt, self + 0x1354u);
    const float f26 = psp_mul(f3, f0);
    const float factor = bits_to_float(0x3F308D3Du);
    const float f27 = psp_mul(f4, f1);
    const float f29 = psp_mul(f2, factor);
    f20 = call_setf(rt, ctx, 0x08946C58u, {self, 0u}, {f22}).f0;
    float f23 = call_setf(rt, ctx, 0x08946C58u, {self, 2u}, {f21}).f0;
    f20 = call_setf(rt, ctx, 0x089467ACu, {first + 0x224u}, {f20}).f0;
    f0 = call_setf(rt, ctx, 0x089467ACu, {second + 0x224u}, {f23}).f0;
    f0 = psp_mul(f27, f0);
    f20 = psp_mul(f26, f20);
    const std::uint32_t mode = byte_u(rt, self + 0x270u);
    f21 = psp_mul(f21, f0);
    f22 = psp_mul(f22, f20);
    f21 = f21 + f21;
    f22 = f22 + f22;
    const float c = bits_to_float(0x40FAE148u);
    float g1;
    float g3;
    if (mode == 0u) {
        g1 = psp_mul(f25, c);
        g3 = 0.0f;
    } else if (mode == 1u) {
        g3 = psp_mul(f25, c);
        g1 = 0.0f;
    } else {
        g1 = psp_mul(f25, half);
        g1 = psp_mul(g1, c);
        g3 = g1;
    }
    g3 = psp_mul(g3, g3);
    f2 = psp_mul(f22, f22);
    const float zero = 0.0f;
    g1 = psp_mul(g1, g1);
    f0 = psp_mul(f21, f21);
    float f12 = f2 - g3;
    f23 = f0 - g1;
    if (f12 < zero) f12 = zero;
    if (f23 < zero) f23 = zero;
    f20 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f12}).f0;
    f0 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f23}).f0;
    f2 = f20 / f22;
    const float tenth = bits_to_float(0x3DCCCCCDu);
    f1 = tenth;
    f0 = f0 / f21;
    if (f2 < f1) f2 = f1;
    if (f0 < f1) f0 = f1;
    f1 = psp_mul(f0, f27);
    f20 = psp_mul(f2, f26);
    f20 = f20 + f1;
    f20 = psp_mul(f20, half);
    f0 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {bits_to_float(0x3F866666u)}).f0;
    f12 = psp_mul(f0, bits_to_float(0x3F8D1EB7u));
    f0 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f12}).f0;
    f0 = call_setf(rt, ctx, 0x08B49D7Cu, {}, {f0}).f0;
    f2 = one;
    f0 = f0 - f2;
    f3 = float_at(rt, self + 0x68u);
    f4 = float_at(rt, input + 4u);
    f1 = bits_to_float(0x41E80000u);
    f0 = psp_mul(f0, f1);
    const float f5b = f29 - f2;
    f4 = std::sqrt(f4);
    f1 = std::sqrt(f3);
    f0 = psp_mul(f0, f5b);
    f0 = f0 + f2;
    f20 = psp_mul(f20, f0);
    f0 = bits_to_float(0x40200000u);
    f20 = psp_mul(f20, f20);
    f20 = psp_mul(f25, f20);
    f20 = psp_mul(f25, f20);
    f20 = psp_mul(f20, f4);
    f3 = psp_mul(f3, f1);
    f20 = f20 / f3;
    f20 = psp_mul(f30, f20);
    f20 = psp_mul(f28, f20);
    f20 = std::sqrt(f20);
    f20 = std::sqrt(f20);
    f20 = std::sqrt(f20);
    f20 = f20 / f0;
    f0 = bits_to_float(0x3F4CCCCDu);
    ctx.fpr[0] = psp_mul(f20, f0);
}

// func_089525EC(self, mode, flag_a, flag_b): decisions over the state block s = self->+8 and the block self->+0xC (b).
// Byte b +0x281 == 4: s +0x530 = 0 (when byte s +0xD) and s +0x5EC = 1.0, done. Otherwise func_0893EB40(b + 0x270) picks
// the branch. Non-zero: s +0x5EC is set from byte +0xD / +0x5F9 and the floats +0x538 / +0x6C4 (1.0 or 0); when it is
// not 0 the float s +0x530 is replaced by func_089401AC(s, s +0x530) if the signed lean s +0x6C4 reaches -0.2778 and, with
// flag_a, byte s +0x510 = 1 when the lean is below that. Zero: s +0x5EC is chosen by mode 3 (1 - s +0x538, clearing bytes
// +0x510 / +0x511), flag_b, func_0895BBC4 / func_08950770 (b + 0x270) and the speed func_08B49D7C(|s +0x6C4, +0x6C8|); then
// when func_08950770 holds, s +0x530 rises to a ratio built from s +0x580 / +0x57C, the halfwords b +0x23E and s +0x58E, and
// with flag_a and byte +0x5F8 clear byte +0x510 = 1.
void func_089525EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[5];
    const std::uint32_t flag_a = ctx.gpr[6] & 0xFFu;
    const std::uint32_t flag_b = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x40u);
    const float tenth = bits_to_float(0x3DCCCCCDu);
    const float one = bits_to_float(0x3F800000u);
    const float limit = bits_to_float(0x3E8E38E4u);
    const std::uint32_t b = word(rt, self + 0xCu);
    const std::uint32_t kind = byte_u(rt, b + 0x281u);
    const std::uint32_t s1 = word(rt, self + 8u);
    if (kind == 4u) {
        ctx.gpr[2] = byte_u(rt, s1 + 0xDu);
        if (ctx.gpr[2] != 0u) put(rt, s1 + 0x530u, 0u);
        put_float(rt, s1 + 0x5ECu, one);
        return;
    }
    const std::uint32_t table = b + 0x270u;
    const std::uint32_t ready = call_set(rt, ctx, 0x0893EB40u, {table});
    if (ready != 0u) {
        ctx.gpr[2] = byte_u(rt, s1 + 0xDu);
        if (ctx.gpr[2] != 0u) {
            put(rt, s1 + 0x5ECu, 0u);
        } else {
            ctx.gpr[2] = byte_u(rt, s1 + 0x5F9u);
            if (ctx.gpr[2] == 0u) {
                const float reading = float_at(rt, s1 + 0x538u);
                put(rt, s1 + 0x5ECu, 0u);
                if (reading < tenth) put_float(rt, s1 + 0x5ECu, one);
            } else {
                const float reading = float_at(rt, s1 + 0x538u);
                put_float(rt, s1 + 0x5ECu, one);
                if (tenth < reading) {
                    const float lean = float_at(rt, s1 + 0x6C4u);
                    if (lean < limit && -limit < lean) put(rt, s1 + 0x5ECu, 0u);
                }
            }
        }
        if (float_at(rt, s1 + 0x5ECu) == 0.0f) return;
        ctx.gpr[2] = byte_u(rt, s1 + 0x5F8u);
        float f13 = float_at(rt, s1 + 0x6C4u);
        if (ctx.gpr[2] == 0u) f13 = -f13;
        const float f20 = -limit;
        if (f20 <= f13) {
            const float f12 = float_at(rt, s1 + 0x530u);
            const float adjusted = call_setf(rt, ctx, 0x089401ACu, {s1}, {f12, f13}).f0;
            put_float(rt, s1 + 0x530u, adjusted);
        }
        if (flag_a == 0u) return;
        ctx.gpr[2] = 1u;
        if (float_at(rt, s1 + 0x6C4u) < f20) put8(rt, s1 + 0x510u, 1u);
        return;
    }
    const std::uint32_t s4 = word(rt, self + 8u);
    const std::uint32_t b2 = word(rt, self + 0xCu);
    // The final part: reached from every branch of the zero case.
    const auto finale = [&] {
        if (call_set(rt, ctx, 0x08950770u, {table}) == 0u) return;
        float f3;
        if (float_at(rt, s1 + 0x53Cu) < tenth) {
            f3 = float_at(rt, s1 + 0x580u);
        } else {
            const float x = float_at(rt, s1 + 0x6C4u);
            const float y = float_at(rt, s1 + 0x6C8u);
            const float speed = call_setf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(x, x) + psp_mul(y, y)}).f0;
            if (!(limit <= speed)) return;
            f3 = float_at(rt, s1 + 0x580u);
        }
        float f2 = float_at(rt, s1 + 0x57Cu);
        const std::uint32_t reference = half_u(rt, b + 0x23Eu);
        ctx.gpr[2] = sext16(half_u(rt, s1 + 0x58Eu));
        f2 = f2 + f3;
        float f0 = static_cast<float>(signed_of(reference));
        const float f5 = static_cast<float>(signed_of(ctx.gpr[2]));
        f3 = f3 / f2;
        f0 = f0 + bits_to_float(0x43FA0000u);
        f0 = f0 - f5;
        f0 = psp_mul(f0, bits_to_float(0x3A03126Fu));
        float f1 = f0 + f3;
        if (f1 < 0.0f) {
            f1 = 0.0f;
        } else if (one < f1) {
            f1 = one;
        }
        if (float_at(rt, s1 + 0x530u) < f1) put_float(rt, s1 + 0x530u, f1);
        if (flag_a == 0u) return;
        ctx.gpr[2] = byte_u(rt, s1 + 0x5F8u);
        if (ctx.gpr[2] != 0u) return;
        ctx.gpr[2] = 1u;
        put8(rt, s1 + 0x510u, 1u);
    };
    ctx.gpr[2] = byte_u(rt, s4 + 0xDu);
    if (ctx.gpr[2] != 0u) {
        put(rt, s4 + 0x5ECu, 0u);
        return finale();
    }
    ctx.gpr[2] = 3u;
    if (mode == 3u) {
        const float reading = float_at(rt, s4 + 0x538u);
        put(rt, s4 + 0x538u, 0u);
        const float f1 = one - reading;
        put_float(rt, s4 + 0x5ECu, f1);
        if (!(f1 == 0.0f)) {
            put8(rt, s4 + 0x510u, 0u);
            put8(rt, s4 + 0x511u, 0u);
        }
        return finale();
    }
    if (flag_b != 0u) {
        put(rt, s4 + 0x5ECu, 0u);
        return finale();
    }
    const std::uint32_t slot = byte_u(rt, b2 + 0x272u);
    const std::uint32_t inner = b2 + 0x270u;
    if (slot == 1u) {
        if (call_set(rt, ctx, 0x0895BBC4u, {inner}) == 0u) {
            if (!(float_at(rt, s4 + 0x53Cu) <= bits_to_float(0x3F4CCCCDu))) {
                put(rt, s4 + 0x5ECu, 0u);
                return finale();
            }
        }
    }
    const std::uint32_t verdict = call_set(rt, ctx, 0x08950770u, {inner});
    if (verdict != 0u) {
        if (!(float_at(rt, s4 + 0x53Cu) <= tenth)) {
            const float x = float_at(rt, s4 + 0x6C4u);
            const float y = float_at(rt, s4 + 0x6C8u);
            const float speed = call_setf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(x, x) + psp_mul(y, y)}).f0;
            if (!(limit <= speed)) {
                put(rt, s4 + 0x5ECu, 0u);
                return finale();
            }
        }
    }
    ctx.gpr[2] = byte_u(rt, s4 + 0x5F9u);
    if (ctx.gpr[2] != 0u) {
        put_float(rt, s4 + 0x5ECu, one);
        const std::uint32_t maximum = half_u(rt, b2 + 0x23Eu);
        const std::uint32_t current = sext16(half_u(rt, s4 + 0x58Eu));
        if (signed_of(current) < signed_of(maximum)) put(rt, s4 + 0x5ECu, 0u);
        return finale();
    }
    put(rt, s4 + 0x5ECu, 0u);
    const std::uint32_t second_slot = byte_u(rt, b2 + 0x272u);
    float f1 = float_at(rt, s4 + 0x538u);
    if (second_slot == 1u) {
        const float held = float_at(rt, s4 + 0x53Cu);
        if (f1 < held) f1 = held;
    }
    if (f1 < bits_to_float(0x3E800000u)) put_float(rt, s4 + 0x5ECu, one);
    finale();
}

// func_08957478(self, unused, mode): initialises defaults in a settings record. mode 0xFFFFFFFF counts as 0. Byte +0 = 2;
// +0x11C / +0x112 take the constants at 0x08CDE69C / 0x08CDE69D when zero; +0x29 (when zero and byte +1 is not 4) = 0x1E / 0x32 /
// 0x64 by byte +0x128 (1 / 2 / other). func_089499F8(self) non-zero runs func_08949A18(self). By byte +1: 8 sets byte +0xA8 = 5;
// 7 sets +0x129 = 8 and +0x12B = 0x19; 3 clears +0x11C / +0x112; 2 sets +0x15A = 1, clears them and, for mode 2 / 3, +0xA8 = 3 and
// adds the constants at 0x08CDE710 / 0x08CDE711 / 0x08CF3651 / 0x08CF3652 to the fields +0x66 / +0x7E / +0x9A / +0x9C / +0x92..+0x95
// and sets +0x68 / +0x80 from 0x08CDE70F / 0x08CDE70E. Halfwords +0x12 / +0x14 default to half of +0x10. Two 6-byte tables
// (0x08C8B494, 0x08C8B48C) are copied to +0x19C / +0x1A2 (byte +0x182 = 6) and +0x190 / +0x196 (byte +0x181 = 6) when
// those are unset. Many byte defaults follow (+0x1AA = 0x64, +0x1AB = 0x3C, +0x1FF.. = 1, +0x1F6.. = 2) and for the six 0xC0-byte
// blocks at +0x210: bytes +0x6E..+0x72 default 0x64, +0x73 0x46, +2 0x2D, +0xC 6 (with the 6-byte tables at 0x08CDE1AC / 0x08CDE1A4
// copied to +0x50 / +0x56), and +0x64..+0x83 default to the bytes at self +0x274...
void func_08957478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t mode = ctx.gpr[6];
    if (~mode == 0u) mode = 0u;
    GuestFrame frame(ctx, 0x20u);
    put8(rt, self, 2u);
    if (byte_u(rt, self + 0x11Cu) == 0u) put8(rt, self + 0x11Cu, byte_u(rt, 0x08CDE69Cu));
    if (byte_u(rt, self + 0x112u) == 0u) put8(rt, self + 0x112u, byte_u(rt, 0x08CDE69Du));
    if (byte_u(rt, self + 0x29u) == 0u && byte_u(rt, self + 1u) != 4u) {
        const std::uint32_t style = byte_u(rt, self + 0x128u);
        put8(rt, self + 0x29u, style == 1u ? 0x1Eu : style == 2u ? 0x32u : 0x64u);
    }
    if (call_set(rt, ctx, 0x089499F8u, {self}) != 0u) call_set(rt, ctx, 0x08949A18u, {self});
    const std::uint32_t kind = byte_u(rt, self + 1u);
    if (kind == 8u) put8(rt, self + 0xA8u, 5u);
    if (kind == 7u) {
        put8(rt, self + 0x129u, 8u);
        put8(rt, self + 0x12Bu, 0x19u);
    }
    if (kind == 3u) {
        put8(rt, self + 0x11Cu, 0u);
        put8(rt, self + 0x112u, 0u);
    }
    if (kind == 2u) {
        put8(rt, self + 0x15Au, 1u);
        put8(rt, self + 0x11Cu, 0u);
        put8(rt, self + 0x112u, 0u);
        if (mode - 2u < 2u) {
            put8(rt, self + 0xA8u, 3u);
            put16(rt, self + 0x66u, half_u(rt, self + 0x66u) + byte_u(rt, 0x08CDE710u));
            put16(rt, self + 0x7Eu, half_u(rt, self + 0x7Eu) + byte_u(rt, 0x08CDE710u));
            put8(rt, self + 0x92u, byte_u(rt, self + 0x92u) + byte_u(rt, 0x08CF3652u));
            put8(rt, self + 0x93u, byte_u(rt, self + 0x93u) + byte_u(rt, 0x08CF3652u));
            put16(rt, self + 0x9Au, half_u(rt, self + 0x9Au) + byte_u(rt, 0x08CDE711u));
            put16(rt, self + 0x9Cu, half_u(rt, self + 0x9Cu) + byte_u(rt, 0x08CDE711u));
            put8(rt, self + 0x94u, byte_u(rt, self + 0x94u) + byte_u(rt, 0x08CF3651u));
            put8(rt, self + 0x95u, byte_u(rt, self + 0x95u) + byte_u(rt, 0x08CF3651u));
            put16(rt, self + 0x68u, byte_u(rt, 0x08CDE70Fu));
            put16(rt, self + 0x80u, byte_u(rt, 0x08CDE70Eu));
        }
    }
    if (sext16(half_u(rt, self + 0x12u)) == 0u) {
        const std::uint32_t width = sext16(half_u(rt, self + 0x10u));
        put16(rt, self + 0x12u, static_cast<std::uint32_t>(signed_of((width >> 31) + width) >> 1));
    }
    if (sext16(half_u(rt, self + 0x14u)) == 0u) {
        const std::uint32_t width = sext16(half_u(rt, self + 0x10u));
        put16(rt, self + 0x14u, static_cast<std::uint32_t>(signed_of((width >> 31) + width) >> 1));
    }
    const std::uint32_t head_word = word(rt, 0x08C8B494u);
    const std::uint32_t head_half = half_u(rt, 0x08C8B498u);
    const std::uint32_t tail_half = half_u(rt, 0x08C8B490u);
    const std::uint32_t tail_word = word(rt, 0x08C8B48Cu);
    const std::uint32_t second = frame.at(6u);
    put_unaligned(rt, second, tail_word);
    put(rt, frame.at(0u), head_word);
    put16(rt, frame.at(4u), head_half);
    put16(rt, frame.at(0xAu), tail_half);
    // Copies the two 6-byte tables from the frame to the two consecutive slots at `first` and `first + 6`.
    const auto copy_tables = [&](std::uint32_t first, std::uint32_t marker, std::uint32_t marker_value) {
        const std::uint32_t head = word_unaligned(rt, frame.at(0u));
        const std::uint32_t tail = word_unaligned(rt, second);
        const std::uint32_t head_a = byte_u(rt, frame.at(4u));
        const std::uint32_t head_b = byte_u(rt, frame.at(5u));
        const std::uint32_t tail_a = byte_u(rt, frame.at(0xAu));
        const std::uint32_t tail_b = byte_u(rt, frame.at(0xBu));
        put_unaligned(rt, first, head);
        put_unaligned(rt, first + 6u, tail);
        put8(rt, first + 5u, head_b);
        put8(rt, first + 6u + 5u, tail_b);
        put8(rt, marker, marker_value);
        put8(rt, first + 4u, head_a);
        put8(rt, first + 6u + 4u, tail_a);
    };
    if (byte_u(rt, self + 0x182u) == 0u) copy_tables(self + 0x19Cu, self + 0x182u, 6u);
    if (byte_u(rt, self + 0x195u) == 0u) copy_tables(self + 0x190u, self + 0x181u, 6u);
    if (byte_u(rt, self + 0x1AAu) == 0u) put8(rt, self + 0x1AAu, 0x64u);
    if (byte_u(rt, self + 0x1ABu) == 0u) put8(rt, self + 0x1ABu, 0x3Cu);
    put8(rt, self + 0x1FFu, 1u);
    put8(rt, self + 0x207u, 2u);
    put8(rt, self + 0x1E0u, 1u);
    put8(rt, self + 0x1F2u, 1u);
    put8(rt, self + 0x1F3u, 1u);
    put8(rt, self + 0x1F4u, 1u);
    put8(rt, self + 0x1F5u, 1u);
    put8(rt, self + 0x1FDu, 1u);
    put8(rt, self + 0x1FEu, 1u);
    put8(rt, self + 0x1F6u, 2u);
    put8(rt, self + 0x204u, 2u);
    put8(rt, self + 0x205u, 2u);
    put8(rt, self + 0x206u, 2u);
    std::uint32_t block = self + 0x210u;
    for (std::uint32_t index = 0u; index < 6u; ++index, block += 0xC0u) {
        for (std::uint32_t offset = 0u; offset < 5u; ++offset) {
            if (byte_u(rt, block + 0x6Eu + offset) == 0u) put8(rt, block + 0x6Eu + offset, 0x64u);
        }
        if (byte_u(rt, block + 0x73u) == 0u) put8(rt, block + 0x73u, 0x46u);
    }
    const std::uint32_t first_block = self + 0x210u;
    if (byte_u(rt, first_block + 0x78u) == 0u) put8(rt, first_block + 0x78u, byte_u(rt, first_block + 0x64u));
    if (byte_u(rt, first_block + 0x79u) == 0u) put8(rt, first_block + 0x79u, byte_u(rt, first_block + 0x65u));
    if (byte_u(rt, first_block + 0x7Au) == 0u) put8(rt, first_block + 0x7Au, byte_u(rt, first_block + 0x6Du));
    block = first_block;
    for (std::uint32_t index = 0u; index < 6u; ++index, block += 0xC0u) {
        if (byte_u(rt, block + 2u) == 0u) put8(rt, block + 2u, 0x2Du);
        if (byte_u(rt, block + 0xCu) == 0u) {
            put8(rt, block + 0xCu, 6u);
            for (std::uint32_t offset = 0u; offset < 6u; ++offset) {
                put8(rt, block + 0x50u + offset, byte_u(rt, 0x08CDE1ACu + offset));
                put8(rt, block + 0x56u + offset, byte_u(rt, 0x08CDE1A4u + offset));
            }
        }
        for (std::uint32_t offset = 0u; offset < 0x20u; ++offset) {
            if (byte_u(rt, block + 0x64u + offset) == 0u) put8(rt, block + 0x64u + offset, byte_u(rt, self + 0x274u + offset));
        }
    }
    ctx.gpr[2] = byte_u(rt, block - 0xC0u + 0x64u + 0x1Fu);
}

// func_08963418(self, source): builds two {pointer, shared-pointer control block} pairs at self +0/+4 and +8/+0xC from the
// two words source->+0 / source->+4 (both pairs start as 0). A non-null source->+0: func_08B2B4D8(0x80, it) is wrapped by
// func_08BE43E4 (deleter func_08B35254) into a frame pair, a 0xA0-byte object is allocated (func_08B62344) and built
// (func_08BE45C4), a 0x10-byte control block {vtable 0x08CC6928, counts 1 / 1, object} is allocated and func_08B8E380 links
// it; the frame pair's block is released, self->+0 takes the object and self->+4 the control block (use count up, old block
// released). A non-null source->+4 does the same with func_08B59AC8 / func_08BE43E4 (0x08B59ADC) / func_08BE449C into
// self +8 / +0xC. The temporaries' blocks are released at the end.
void func_08963418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t first = word(rt, source);
    const std::uint32_t second = word(rt, source + 4u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0xCu, 0u);
    // The shared-pointer assignment of `block` to the slot at `slot`: nothing when it is the same block.
    const auto assign_block = [&](std::uint32_t slot, std::uint32_t block) {
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t previous = word(rt, slot);
        if (previous != 0u) release_shared(rt, ctx, previous);
        put(rt, slot, block);
    };
    if (first != 0u) {
        const std::uint32_t wrapped = call_set(rt, ctx, 0x08B2B4D8u, {0x80u, first});
        call_set(rt, ctx, 0x08BE43E4u, {frame.at(0u), wrapped, 0x08B35254u});
        const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0xA0u});
        call_set(rt, ctx, 0x08BE45C4u, {object, frame.at(0u), first, 0u});
        put(rt, frame.at(0x18u), object);
        const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x10u});
        put(rt, control + 4u, 1u);
        put(rt, control + 0xCu, object);
        put(rt, control, 0x08CC6928u);
        put(rt, control + 8u, 1u);
        put(rt, frame.at(0x1Cu), control);
        call_set(rt, ctx, 0x08B8E380u, {frame.at(0x1Cu), object, object, 1u});
        const std::uint32_t temporary = word(rt, frame.at(4u));
        if (temporary != 0u) release_shared(rt, ctx, temporary);
        ctx.gpr[2] = word(rt, frame.at(0x18u));
        std::uint32_t block = word(rt, frame.at(0x1Cu));
        const std::uint32_t old_block = word(rt, self + 4u);
        put(rt, self, ctx.gpr[2]);
        if (block != old_block) {
            assign_block(self + 4u, block);
            block = word(rt, frame.at(0x1Cu));
        }
        if (block != 0u) release_shared(rt, ctx, block);
    }
    if (second == 0u) return;
    const std::uint32_t length = call_set(rt, ctx, 0x08B59AC8u, {second});
    call_set(rt, ctx, 0x08BE43E4u, {frame.at(8u), length, 0x08B59ADCu});
    call_set(rt, ctx, 0x08BE449Cu, {frame.at(0x10u), frame.at(8u), second, 0u});
    ctx.gpr[2] = word(rt, frame.at(0x10u));
    const std::uint32_t old_block = word(rt, self + 0xCu);
    std::uint32_t block = word(rt, frame.at(0x14u));
    put(rt, self + 8u, ctx.gpr[2]);
    if (block != old_block) {
        assign_block(self + 0xCu, block);
        block = word(rt, frame.at(0x14u));
    }
    if (block != 0u) release_shared(rt, ctx, block);
    block = word(rt, frame.at(0xCu));
    if (block != 0u) release_shared(rt, ctx, block);
}

// func_0897BC08(self): push_back(true) on the bit vector at self + 0x74 {begin word pointer +0, begin offset +4, end word
// pointer +8 (self + 0x7C), end offset +0xC (self + 0x80), capacity word pointer +0x10}, after func_0897BB18(self,
// func_089D5B30()). With room the bit at the end iterator is set and the iterator advances; full, a block of 2 x size + 31
// bits (63 when empty, rounded to words; func_08A5BC20(bytes, 4)) takes the bits copied one by one, the new bit and the
// tail, the old block is freed (func_08A5BC54) and the members updated. Then, while the last bit of the vector is set:
// func_0897B6E4(), func_0897B800(self) and, by byte +0x88, func_08979C60 / func_08977318 and func_0897BB68(self), then
// func_08B1A844(). When it is clear the end iterator steps back, func_0897BB18 runs again and, for an empty vector,
// func_08978574(self) decides on the tail jump to func_0897851C(self).
void func_0897BC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t vec = self + 0x74u;
    GuestFrame frame(ctx, 0x20u);
    const auto notify = [&] {
        const std::uint32_t value = call_set(rt, ctx, 0x089D5B30u, {});
        call_set(rt, ctx, 0x0897BB18u, {self, value});
    };
    notify();
    const std::uint32_t old_end = word(rt, vec + 8u);
    if (old_end != word(rt, vec + 0x10u)) {
        const std::uint32_t offset = word(rt, self + 0x80u);
        const std::uint32_t pointer = word(rt, self + 0x7Cu);
        put(rt, self + 0x80u, offset + 1u);
        if (offset + 1u == 0x20u) {
            put(rt, self + 0x80u, 0u);
            put(rt, self + 0x7Cu, pointer + 4u);
        }
        const std::uint32_t current = word(rt, pointer);
        put(rt, pointer, (1u << (offset & 31u)) | current);
    } else {
        const std::uint32_t end_pointer = word(rt, self + 0x7Cu);
        const std::uint32_t begin_pointer = word(rt, self + 0x74u);
        const std::uint32_t end_offset = word(rt, self + 0x80u);
        const std::uint32_t begin_offset = word(rt, vec + 4u);
        const std::uint32_t total = end_offset + ((end_pointer - begin_pointer) << 3);
        const std::uint32_t old_offset = word(rt, vec + 0xCu);
        const std::uint32_t wanted = total == begin_offset ? 0x3Fu : ((total - begin_offset) << 1) + 0x1Fu;
        const std::uint32_t bytes = (wanted >> 5) << 2;
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        const std::uint32_t begin = word(rt, vec);
        const std::uint32_t source_offset0 = word(rt, vec + 4u);
        const std::uint32_t count = old_offset + ((old_end - begin) << 3) - source_offset0;
        std::uint32_t source = begin;
        std::uint32_t source_offset = source_offset0;
        std::uint32_t destination = block;
        std::uint32_t destination_offset = 0u;
        std::uint32_t current = word(rt, block);
        std::uint32_t next_offset = 1u;
        if (signed_of(count) > 0) {
            for (std::uint32_t done = 0u; done != count;) {
                const std::uint32_t source_word = word(rt, source);
                const bool set = ((1u << (source_offset & 31u)) & source_word) != 0u;
                const std::uint32_t mask = 1u << (destination_offset & 31u);
                current = set ? (mask | current) : (current & ~mask);
                put(rt, destination, current);
                ++source_offset;
                ++done;
                std::uint32_t step = destination_offset + 1u;
                next_offset = destination_offset + 2u;
                if (source_offset == 0x20u) {
                    source += 4u;
                    source_offset = 0u;
                }
                if (step == 0x20u) {
                    destination += 4u;
                    current = word(rt, destination);
                    step = 0u;
                    next_offset = 1u;
                }
                destination_offset = step;
            }
        }
        std::uint32_t new_pointer = destination;
        if (next_offset == 0x20u) {
            new_pointer = destination + 4u;
            next_offset = 0u;
        }
        put(rt, destination, (1u << (destination_offset & 31u)) | current);
        std::uint32_t tail_offset = next_offset;
        const std::uint32_t tail = word(rt, vec + 0xCu) + ((word(rt, vec + 8u) - old_end) << 3) - old_offset;
        std::uint32_t tail_pointer = new_pointer;
        if (signed_of(tail) > 0) {
            std::uint32_t read = old_end;
            std::uint32_t read_offset = old_offset;
            for (std::uint32_t done = 0u; done != tail;) {
                const std::uint32_t read_word = word(rt, read);
                const bool set = ((1u << (read_offset & 31u)) & read_word) != 0u;
                const std::uint32_t mask = 1u << (tail_offset & 31u);
                const std::uint32_t target = word(rt, tail_pointer);
                put(rt, tail_pointer, set ? (mask | target) : (target & ~mask));
                ++read_offset;
                ++done;
                ++tail_offset;
                if (read_offset == 0x20u) {
                    read += 4u;
                    read_offset = 0u;
                }
                if (tail_offset == 0x20u) {
                    tail_pointer += 4u;
                    tail_offset = 0u;
                }
            }
        }
        const std::uint32_t old_block = word(rt, vec);
        put(rt, vec + 0xCu, tail_offset);
        put(rt, vec + 8u, tail_pointer);
        if (old_block != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_block, (word(rt, vec + 0x10u) - old_block) & ~3u, 4u});
        put(rt, vec + 0x10u, block + bytes);
        put(rt, vec, block);
        put(rt, vec + 4u, 0u);
    }
    std::uint32_t end_pointer;
    for (;;) {
        const std::uint32_t end_offset = word(rt, vec + 0xCu);
        end_pointer = word(rt, vec + 8u);
        if (!bit_before_end(rt, end_pointer, end_offset)) break;
        call_set(rt, ctx, 0x0897B6E4u, {});
        call_set(rt, ctx, 0x0897B800u, {self});
        std::uint32_t flag = byte_u(rt, self + 0x88u);
        if (flag == 0u) {
            call_set(rt, ctx, 0x08979C60u, {self});
            call_set(rt, ctx, 0x08977318u, {self});
            flag = byte_u(rt, self + 0x88u);
        }
        if (flag != 0u) call_set(rt, ctx, 0x0897BB68u, {self});
        call_set(rt, ctx, 0x08B1A844u, {});
    }
    const std::uint32_t steps = word(rt, vec + 0xCu) - 1u;
    put(rt, vec + 0xCu, steps);
    if (steps == 0xFFFFFFFFu) {
        put(rt, vec + 0xCu, 0x1Fu);
        put(rt, vec + 8u, end_pointer - 4u);
    }
    notify();
    const std::uint32_t end_now = word(rt, vec + 8u);
    ctx.gpr[2] = word(rt, vec);
    if (ctx.gpr[2] != end_now) return;
    if (word(rt, vec + 4u) != word(rt, vec + 0xCu)) return;
    const std::uint32_t verdict = call_set(rt, ctx, 0x08978574u, {self});
    if (signed_of(verdict) <= 0) return;
    tail_call(ctx, 0x0897851Cu, {self});
}

// func_089AE690(out, unused, count, args): out is assigned a value object built from the arguments. count == 2:
// func_089A354C(frame +0x40, args) fills a string; with args->+4 == 0 the value is the literal 0x08C8C74C (func_08B85EB8 +
// func_08A6D2D4), else func_08A6C410(frame +0x3C, args + 4) fills a second string, func_089BD608(frame +0x40's object) and
// func_089B4754(that, +0x3C's object, func_08A6BF80(...)) look something up and, when found, its text becomes the value;
// otherwise out takes the +0x3C value. count >= 3 (else v0 = 1): func_089BB3A8 / func_089BD608 give a key, the object at
// args->+4 fills frame +0x3C (virtual slot 0xC) and the one at args->+8 (when set) frame +0x40; func_089B3688(key, +0x3C,
// +0x40 strings) finds the entry whose text becomes the value (func_08B85EB8 + func_08A6D2D4) or, without one, out takes
// *(args + 8). The frame strings are released at the end (empty rep 0x08D74FD0, release func_08B8542C).
void func_089AE690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kRelease = 0x08B8542Cu;
    // A value object from the C string `text` through the frame string at `string` and the value at `value`, assigned to out.
    const auto text_value = [&](std::uint32_t text, std::uint32_t string, std::uint32_t value, std::uint32_t tag, std::uint32_t release_tag) {
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(string), text, frame.at(tag)});
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(value), frame.at(string)});
        keep_temporary(rt, ctx, out, frame.at(value));
        release_string(rt, ctx, frame.at(string), kEmpty, kRelease, frame.at(release_tag));
    };
    if (count == 2u) {
        call_set(rt, ctx, 0x089A354Cu, {frame.at(0x40u), args});
        if (word(rt, args + 4u) == 0u) {
            text_value(0x08C8C74Cu, 0x34u, 0x38u, 0xDu, 7u);
        } else {
            call_set(rt, ctx, 0x08A6C410u, {frame.at(0x3Cu), args + 4u});
            const std::uint32_t first = checked(rt, ctx, word(rt, frame.at(0x40u)), 0x08CA5170u);
            const std::uint32_t key = call_set(rt, ctx, 0x089BD608u, {first});
            const std::uint32_t second_raw = word(rt, frame.at(0x3Cu));
            const std::uint32_t first_object = word(rt, frame.at(0x40u));
            const std::uint32_t second = checked(rt, ctx, second_raw, 0x08CA517Cu);
            const std::uint32_t second_value = call_set(rt, ctx, 0x08A6BF80u, {second});
            const std::uint32_t found = call_set(rt, ctx, 0x089B4754u, {key, first_object, second_value});
            if (found != 0u) {
                text_value(found, 0x2Cu, 0x30u, 0xCu, 6u);
            } else {
                keep_temporary(rt, ctx, out, frame.at(0x3Cu));
                call_set(rt, ctx, kDestroy, {frame.at(0x40u)});
                return;
            }
            call_set(rt, ctx, kDestroy, {frame.at(0x3Cu)});
        }
        call_set(rt, ctx, kDestroy, {frame.at(0x40u)});
        return;
    }
    if (signed_of(count) < 3) {
        ctx.gpr[2] = 1u;
        return;
    }
    call_set(rt, ctx, 0x089BB3A8u, {frame.at(0x28u), args});
    const std::uint32_t holder = checked(rt, ctx, word(rt, frame.at(0x28u)), 0x08CA5184u);
    const std::uint32_t key = call_set(rt, ctx, 0x089BD608u, {holder});
    call_set(rt, ctx, kDestroy, {frame.at(0x28u)});
    const std::uint32_t first = checked(rt, ctx, word(rt, args + 4u), 0x08CA50BCu);
    call_virtual_at(rt, ctx, first, 0xCu, {frame.at(0x3Cu), first});
    const std::uint32_t second = word(rt, args + 8u);
    if (second == 0u) {
        text_value(0x08C8C74Cu, 0x20u, 0x24u, 0xBu, 5u);
    } else {
        call_virtual_v1(rt, ctx, second, 0xCu, {frame.at(0x40u), second});
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x1Cu), word(rt, frame.at(0x3Cu)), frame.at(0xAu)});
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x18u), word(rt, frame.at(0x40u)), frame.at(9u)});
        const std::uint32_t entry = call_set(rt, ctx, 0x089B3688u, {key, frame.at(0x1Cu), frame.at(0x18u)});
        release_string(rt, ctx, frame.at(0x18u), kEmpty, kRelease, frame.at(4u));
        release_string(rt, ctx, frame.at(0x1Cu), kEmpty, kRelease, frame.at(3u));
        if (entry != 0u) {
            text_value(entry, 0x10u, 0x14u, 8u, 2u);
        } else if (args + 8u != out) {
            call_set(rt, ctx, kAssign, {out, word(rt, args + 8u)});
        }
        release_string(rt, ctx, frame.at(0x40u), kEmpty, kRelease, frame.at(1u));
    }
    release_string(rt, ctx, frame.at(0x3Cu), kEmpty, kRelease, frame.at(0u));
}

// func_089F25E0(self, object): after func_08A07B70(), byte object->+0x118 = 1 and the object's virtual slot 0x110(object,
// (self->+0x1A0 >> 6) & 3) runs; when func_08A06EC8(self) == 1 the slot 0x108 of the object gets (object,
// func_08A06E94(self, 0)). func_089E0570(frame +0, self) fills a frame value; nine words are gathered at frame +0x24
// (self +0x25C / +0x24C / +0x260 / +0x254, frame word +0, self +0x258 / +0x264 / +0x250 / +0x268). f20 = func_089A8B68(object),
// f21 = func_089A8B80(object). With only the first word set and its object's virtual slot 0x28 answering non-zero, that
// object (halfwords +0x12 / +0x14 as divisors of self +0x26C / +0x270) and the constant records at 0x08CDEB10 / 0x08CDEB30
// feed three calls of func_089F2028(self, object, record, frame +4, frame +0x48 / +0x4C / +0x50). Otherwise a table of nine
// 16-byte rectangles (frame +0x48) is built from f20 / f21 and the self +0x26C / +0x270 floats (zero where no neighbour word
// is set) and func_089F1AEC(self, object, word, rectangle) runs for each non-zero gathered word. Then the object's virtual
// slot 0x114, func_089A8C24(object), func_08A07CE0(self, object) and the destruction func_08A5B9EC(frame +0).
void func_089F25E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x110u);
    call_set(rt, ctx, 0x08A07B70u, {});
    put8(rt, object + 0x118u, 1u);
    call_virtual_at(rt, ctx, object, 0x110u, {object, (word(rt, self + 0x1A0u) >> 6) & 3u});
    if (call_set(rt, ctx, 0x08A06EC8u, {self}) == 1u) {
        const std::uint32_t handler = word(rt, word(rt, object) + 0x108u);
        const std::uint32_t argument = call_set(rt, ctx, 0x08A06E94u, {self, 0u});
        call_set(rt, ctx, handler, {object, argument});
    }
    call_set(rt, ctx, 0x089E0570u, {frame.at(0u), self});
    const std::uint32_t gathered = frame.at(0x24u);
    put(rt, frame.at(0x24u), word(rt, self + 0x25Cu));
    put(rt, frame.at(0x30u), word(rt, self + 0x254u));
    put(rt, frame.at(0x38u), word(rt, self + 0x258u));
    put(rt, frame.at(0x40u), word(rt, self + 0x250u));
    put(rt, frame.at(0x44u), word(rt, self + 0x268u));
    put(rt, frame.at(0x28u), word(rt, self + 0x24Cu));
    put(rt, frame.at(0x2Cu), word(rt, self + 0x260u));
    put(rt, frame.at(0x34u), word(rt, frame.at(0u)));
    put(rt, frame.at(0x3Cu), word(rt, self + 0x264u));
    const float f20 = call_setf(rt, ctx, 0x089A8B68u, {object}, {}).f0;
    const float f21 = call_setf(rt, ctx, 0x089A8B80u, {object}, {}).f0;
    std::uint32_t mask = 0u;
    for (std::uint32_t index = 0u; index < 9u; ++index) {
        if (word(rt, gathered + index * 4u) != 0u) mask |= 1u << index;
    }
    const std::uint32_t first = word(rt, frame.at(0x24u));
    const float x = float_at(rt, self + 0x26Cu);
    const float y = float_at(rt, self + 0x270u);
    bool use_record = false;
    if (mask == 1u && first != 0u) {
        use_record = call_virtual_at(rt, ctx, first, 0x28u, {first}) != 0u;
    }
    if (use_record) {
        put8(rt, object + 0x118u, 1u);
        put(rt, object + 0x11Cu, first);
        const float one = bits_to_float(0x3F800000u);
        const float width = static_cast<float>(signed_of(sext16(half_u(rt, first + 0x12u))));
        const float height = static_cast<float>(signed_of(sext16(half_u(rt, first + 0x14u))));
        const float f7 = f21 - y;
        const float ratio_x = x / width;
        const float f1 = f20 - x;
        const float ratio_y = y / height;
        const std::uint32_t t1 = word(rt, 0x08CDEB30u);
        const std::uint32_t t2 = word(rt, 0x08CDEB40u);
        const std::uint32_t t3 = word(rt, 0x08CDEB4Cu);
        const std::uint32_t t4 = word(rt, 0x08CDEB10u);
        const std::uint32_t t5 = word(rt, 0x08CDEB2Cu);
        const std::uint32_t t6 = word(rt, 0x08CDEB20u);
        const std::uint32_t record = frame.at(4u);
        put(rt, frame.at(4u), t1);
        put(rt, frame.at(0x14u), t2);
        put(rt, frame.at(0x20u), t3);
        put_float(rt, frame.at(0xCu), f1);
        put(rt, frame.at(0x48u), t4);
        put_float(rt, frame.at(0x1Cu), one - ratio_x);
        put(rt, frame.at(0x58u), t6);
        put_float(rt, frame.at(0x60u), one - ratio_y);
        put(rt, frame.at(0x64u), t5);
        put_float(rt, frame.at(0x50u), f7);
        put_float(rt, frame.at(8u), x);
        put_float(rt, frame.at(0x18u), ratio_x);
        put_float(rt, frame.at(0x4Cu), y);
        put_float(rt, frame.at(0x5Cu), ratio_y);
        put_float(rt, frame.at(0x10u), f20);
        put_float(rt, frame.at(0x54u), f21);
        call_set(rt, ctx, 0x089F2028u, {self, object, first, record, frame.at(0x48u)});
        call_set(rt, ctx, 0x089F2028u, {self, object, first, record, frame.at(0x4Cu)});
        call_set(rt, ctx, 0x089F2028u, {self, object, first, record, frame.at(0x50u)});
    } else {
        const std::uint32_t e1 = word(rt, frame.at(0x28u));
        const std::uint32_t e2 = word(rt, frame.at(0x2Cu));
        const std::uint32_t e3 = word(rt, frame.at(0x30u));
        const std::uint32_t e5 = word(rt, frame.at(0x38u));
        const std::uint32_t e6 = word(rt, frame.at(0x3Cu));
        const std::uint32_t e7 = word(rt, frame.at(0x40u));
        const std::uint32_t e8 = word(rt, frame.at(0x44u));
        const float f3 = (first == 0u && e3 == 0u && e6 == 0u) ? 0.0f : x;
        const bool reach = first != 0u || e1 != 0u || e2 != 0u;
        const float f2 = reach ? y : 0.0f;
        const bool skip = reach && e2 != 0u;
        const float f0 = (skip || e5 != 0u || e8 != 0u) ? x : 0.0f;
        const float f1 = (e6 != 0u || e7 != 0u || e8 != 0u) ? y : 0.0f;
        const float left = f20 - f0;
        const float bottom = f21 - f1;
        const float zero = 0.0f;
        const float values[] = {zero, zero, f3, f2, f3, zero, left, f2, left, zero, f20, f2, zero, f2, f3, bottom,
                                f3, f2, left, bottom, left, f2, f20, bottom, zero, bottom, f3, f21, f3, bottom, left, f21,
                                left, bottom, f20, f21};
        // Nine rectangles of four floats starting at frame +0x48.
        for (std::uint32_t index = 0u; index < 9u; ++index) {
            for (std::uint32_t member = 0u; member < 4u; ++member) {
                put_float(rt, frame.at(0x48u) + index * 0x10u + member * 4u, values[index * 4u + member]);
            }
        }
        std::uint32_t entry = frame.at(0x48u);
        for (std::uint32_t index = 0u; index < 9u; ++index, entry += 0x10u) {
            const std::uint32_t item = word(rt, gathered + index * 4u);
            if (item != 0u) call_set(rt, ctx, 0x089F1AECu, {self, object, item, entry});
        }
    }
    call_virtual_at(rt, ctx, object, 0x114u, {object});
    call_set(rt, ctx, 0x089A8C24u, {object});
    call_set(rt, ctx, 0x08A07CE0u, {self, object});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}

// func_08A13004(self): under the lock at self + 0x10 (func_08B34888; the unlock func_08B34B24 is the tail call). Nothing
// when byte +0x1F4 is 0, or when byte +0x782 is set and byte +0x1F7 is 0. Two five-word records sit at +0x1CC and +0x1E0
// (the second's first word is the one the comparisons use), a third at +0x78..+0x88 (+0x8C..+0x9C is its previous copy).
// With byte +0x1F7 set and func_08ACCC20(self + 0x240) == 0 the pair +0x1C0 / +0x1C4 is cleared. When both are zero:
// +0x84 / +0x98 = 0, func_08A128EC / func_08A128FC reset the records, func_08ACCFF8(self + 0x240, 0, rec->+0x1E0, 0) finds
// an entry (none: done) that func_08A12B60 and the copy +0x78..+0x88 take (byte +0x1F8 cleared unless byte +0x18D, byte
// +0x781 = old +0x1F7, +0x1F7 = 0). Otherwise the records are saved in the frame, and with byte +0x18D set
// func_08ACD284(self + 0x240, +0x784) minus +0x80 decides: below -6006 the saved records are written back, from 3004 up
// the records are re-synchronised (func_08A129EC / func_08A128FC), in between nothing. The previous copy +0x8C..+0x9C is
// taken, func_08ACCFF8 looks up again and refreshes +0x78..+0x88, and while +0x84 < +0x1E0 the saved records come back
// (byte +0x1FB clear) or the synchronising loop runs. The end sets +0x1C0 = +0x84, +0x1C4 = 0, byte +0x1F5 = 1 and,
// when byte +0x1FA is 0 and func_08ACCE04(self + 0x240) is not 0, the bytes +0x780 / +0x1FB / +0x1FA = 1.
void func_08A13004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 0x10u;
    GuestFrame frame(ctx, 0x50u);
    call_set(rt, ctx, 0x08B34888u, {lock});
    std::uint32_t guard = byte_u(rt, self + 0x782u);
    const std::uint32_t pending = byte_u(rt, self + 0x1F7u);
    const std::uint32_t active = byte_u(rt, self + 0x1F4u);
    ctx.gpr[2] = active;
    const auto unlock = [&] { tail_call(ctx, 0x08B34B24u, {lock}); };
    if (active == 0u) return unlock();
    if (pending != 0u) guard = 0u;
    if (guard != 0u) return unlock();
    const std::uint32_t table = self + 0x240u;
    const std::uint32_t first = self + 0x1CCu;
    const std::uint32_t second = self + 0x1E0u;
    if (pending != 0u) {
        if (call_set(rt, ctx, 0x08ACCC20u, {table}) == 0u) {
            put(rt, self + 0x1C0u, 0u);
            put(rt, self + 0x1C4u, 0u);
        }
    }
    // The end: +0x1C0 = +0x84, +0x1C4 = 0, byte +0x1F5 = 1, and the flags when the table accepts.
    const auto finish = [&] {
        ctx.gpr[2] = word(rt, self + 0x84u);
        const std::uint32_t done = byte_u(rt, self + 0x1FAu);
        put(rt, self + 0x1C0u, ctx.gpr[2]);
        put(rt, self + 0x1C4u, 0u);
        put8(rt, self + 0x1F5u, 1u);
        if (done != 0u) return unlock();
        if (call_set(rt, ctx, 0x08ACCE04u, {table}) == 0u) return unlock();
        put8(rt, self + 0x780u, 1u);
        put8(rt, self + 0x1FBu, 1u);
        put8(rt, self + 0x1FAu, 1u);
        unlock();
    };
    if ((word(rt, self + 0x1C0u) | word(rt, self + 0x1C4u)) == 0u) {
        put(rt, self + 0x84u, 0u);
        put(rt, self + 0x98u, 0u);
        call_set(rt, ctx, 0x08A128ECu, {first});
        call_set(rt, ctx, 0x08A128ECu, {second});
        call_set(rt, ctx, 0x08A128FCu, {second});
        const std::uint32_t entry = call_set(rt, ctx, 0x08ACCFF8u, {table, 0u, word(rt, self + 0x1E0u), 0u});
        if (entry == 0u) return unlock();
        call_set(rt, ctx, 0x08A12B60u, {self});
        const std::uint32_t w4 = word(rt, entry + 0x10u);
        const std::uint32_t w0 = word(rt, entry);
        const std::uint32_t w1 = word(rt, entry + 4u);
        const std::uint32_t w2 = word(rt, entry + 8u);
        const std::uint32_t w3 = word(rt, entry + 0xCu);
        const std::uint32_t kept = byte_u(rt, self + 0x18Du);
        put(rt, self + 0x78u, w0);
        put(rt, self + 0x7Cu, w1);
        put(rt, self + 0x80u, w2);
        put(rt, self + 0x84u, w3);
        put(rt, self + 0x88u, w4);
        if (kept == 0u) put8(rt, self + 0x1F8u, 0u);
        put8(rt, self + 0x781u, pending);
        put8(rt, self + 0x1F7u, 0u);
        return finish();
    }
    // The saved records.
    constexpr std::uint32_t kSaved[10] = {0x1CCu, 0x1D0u, 0x1D4u, 0x1D8u, 0x1DCu, 0x1E0u, 0x1E4u, 0x1E8u, 0x1ECu, 0x1F0u};
    for (std::uint32_t index = 0u; index < 10u; ++index) put(rt, frame.at(index * 4u), word(rt, self + kSaved[index]));
    // Writes the saved records back.
    const auto restore = [&] {
        for (std::uint32_t index = 0u; index < 10u; ++index) put(rt, self + kSaved[index], word(rt, frame.at(index * 4u)));
    };
    // The synchronising loop over the two records.
    const auto synchronise = [&] {
        while (call_set(rt, ctx, 0x08A129ECu, {first, second}) == 0u) call_set(rt, ctx, 0x08A128FCu, {first});
        call_set(rt, ctx, 0x08A128FCu, {second});
    };
    call_set(rt, ctx, 0x08A128FCu, {first});
    if (call_set(rt, ctx, 0x08A129ECu, {first, second}) != 0u) call_set(rt, ctx, 0x08A128FCu, {second});
    if (byte_u(rt, self + 0x18Du) != 0u) {
        const std::uint32_t reading = call_set(rt, ctx, 0x08ACD284u, {table, word(rt, self + 0x784u)});
        const std::int32_t difference = signed_of(reading - word(rt, self + 0x80u));
        if (difference < -0x1776) {
            restore();
        } else if (difference >= 0xBBC) {
            synchronise();
        }
    }
    const std::uint32_t copy_a = word(rt, self + 0x78u);
    const std::uint32_t copy_b = word(rt, self + 0x7Cu);
    const std::uint32_t copy_c = word(rt, self + 0x84u);
    const std::uint32_t copy_d = word(rt, self + 0x88u);
    const std::uint32_t key = word(rt, self + 0x1E0u);
    const std::uint32_t copy_e = word(rt, self + 0x80u);
    put(rt, self + 0x8Cu, copy_a);
    put(rt, self + 0x90u, copy_b);
    put(rt, self + 0x94u, copy_e);
    put(rt, self + 0x98u, copy_c);
    put(rt, self + 0x9Cu, copy_d);
    const std::uint32_t entry = call_set(rt, ctx, 0x08ACCFF8u, {table, 0u, key, 0u});
    if (entry != 0u) {
        const std::uint32_t w2 = word(rt, entry + 0x10u);
        const std::uint32_t w1 = word(rt, entry + 4u);
        const std::uint32_t w3 = word(rt, entry + 8u);
        const std::uint32_t w0 = word(rt, entry);
        const std::uint32_t w4 = word(rt, entry + 0xCu);
        put(rt, self + 0x7Cu, w1);
        put(rt, self + 0x78u, w0);
        put(rt, self + 0x80u, w3);
        put(rt, self + 0x84u, w4);
        put(rt, self + 0x88u, w2);
    }
    const std::uint32_t limit = word(rt, self + 0x84u);
    const std::uint32_t current = word(rt, self + 0x1E0u);
    if (limit < current) {
        bool again;
        if (byte_u(rt, self + 0x1FBu) == 0u) {
            restore();
            again = word(rt, frame.at(0x14u)) < limit;
        } else {
            again = current < limit;
        }
        if (again) {
            do {
                synchronise();
            } while (word(rt, self + 0x1E0u) < word(rt, self + 0x84u));
        }
    }
    call_set(rt, ctx, 0x08B2DBE0u, {});
    finish();
}

// func_08A517B4(out, matrix, index) -> out: three rotation angles taken from a 4x4 float matrix. The 24-byte table entry
// `index` at 0x08C8E910 + 24 * index holds three output slots (bytes +0..+2) and seven signed matrix-element selectors
// {sign '-' = 0x2D, row, column} at +3, +6, ..+0x15. The first selector (clamped to [-1, 1], through the soft-float helpers
// func_08B70F28 float->double, func_08B666E8, func_08B71EC8 double->float, func_08B662C4) gives angle f23 and a sine-like
// factor f2; when |f2| is above 1e-6 the angles f22 (func_08B66FBC of two scaled elements) and f20 (the same chain over the
// next selectors, mirrored through pi / reduced by 2 pi with func_08B712B0 / func_08B71A90 when out of range) are computed,
// else f22 is func_08B66FBC of two other selectors and f20 = 0. out = {0, 0, 0, 1.0} with f20 / f23 / f22 stored at the
// slots out[b0] / out[b1] / out[b2].
void func_08A517B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t matrix = ctx.gpr[5];
    const std::uint32_t index = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t entry = (index << 5) - (index << 3) + 0x08C8E910u;
    // The matrix element a selector names, negated when its sign byte is '-'.
    const auto element = [&](std::uint32_t selector) {
        const std::uint32_t sign = sext8(byte_u(rt, selector));
        const std::uint32_t row = byte_u(rt, selector + 1u);
        const std::uint32_t column = byte_u(rt, selector + 2u);
        const float value = float_at(rt, matrix + (((row << 2) + column) << 2));
        return sign == 0x2Du ? -value : value;
    };
    // v0:v1 = the double of `value` clamped to [-1, 1] (exactly 1.0 from 1.0 up, without a conversion).
    const auto clamped_double = [&](float value) {
        if (value < -1.0f) {
            call_setf(rt, ctx, 0x08B70F28u, {}, {-1.0f});
        } else if (value < 1.0f) {
            call_setf(rt, ctx, 0x08B70F28u, {}, {value});
        } else {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0x3FF00000u;
        }
    };
    // f = the float of func_08B666E8(v0:v1).
    const auto angle_of = [&]() {
        call_set(rt, ctx, 0x08B666E8u, {ctx.gpr[2], ctx.gpr[3]});
        return call_setf(rt, ctx, 0x08B71EC8u, {ctx.gpr[2], ctx.gpr[3]}, {}).f0;
    };
    clamped_double(element(entry + 3u));
    const float f23 = angle_of();
    float f2 = call_setf(rt, ctx, 0x08B662C4u, {}, {f23}).f0;
    if (f2 < -1.0f) {
        f2 = -1.0f;
    } else if (!(f2 < 1.0f)) {
        f2 = 1.0f;
    }
    float f20;
    float f22;
    if (bits_to_float(0x358637BDu) < std::fabs(f2)) {
        f20 = 1.0f / f2;
        const float first = element(entry + 6u);
        const float f12 = psp_mul(first, f20);
        const float second = element(entry + 9u);
        f22 = call_setf(rt, ctx, 0x08B66FBCu, {}, {f12, psp_mul(second, f20)}).f0;
        const float third = element(entry + 0xCu);
        const float scaled = psp_mul(third, f20);
        const float fourth = element(entry + 0xFu);
        const float f21 = psp_mul(fourth, f20);
        clamped_double(scaled);
        f20 = angle_of();
        if (f21 < 0.0f) {
            call_setf(rt, ctx, 0x08B70F28u, {}, {f20});
            call_set(rt, ctx, 0x08B712B0u, {0x54442D18u, 0x400921FBu, ctx.gpr[2], ctx.gpr[3]});
            f20 = call_setf(rt, ctx, 0x08B71EC8u, {ctx.gpr[2], ctx.gpr[3]}, {}).f0;
        }
        call_setf(rt, ctx, 0x08B70F28u, {}, {f20});
        const std::uint32_t low = ctx.gpr[2];
        const std::uint32_t high = ctx.gpr[3];
        const std::uint32_t order = call_set(rt, ctx, 0x08B71A90u, {low, high, 0x54442D18u, 0x400921FBu});
        if (signed_of(order) > 0) {
            call_set(rt, ctx, 0x08B712B0u, {low, high, 0x54442D18u, 0x401921FBu});
            f20 = call_setf(rt, ctx, 0x08B71EC8u, {ctx.gpr[2], ctx.gpr[3]}, {}).f0;
        }
    } else {
        const float f12 = element(entry + 0x12u);
        const float f13 = element(entry + 0x15u);
        f22 = call_setf(rt, ctx, 0x08B66FBCu, {}, {f12, f13}).f0;
        f20 = 0.0f;
    }
    put_float(rt, out + 0xCu, bits_to_float(0x3F800000u));
    ctx.gpr[2] = out;
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    put_float(rt, out + (byte_u(rt, entry) << 2), f20);
    put_float(rt, out + (byte_u(rt, entry + 1u) << 2), f23);
    put_float(rt, out + (byte_u(rt, entry + 2u) << 2), f22);
}

// func_08B90024(self, other) -> self: the libstdc++ vector::operator= for 16-byte elements {word +0, shared-pointer control
// block +4, word +8, shared-pointer control block +0xC}. Nothing for self == other. When other's size exceeds self's capacity
// a block of that size (func_08B35DA0) takes copies of other's elements (func_08B8FFA8), the old range is destroyed
// (func_08B8FE44) and freed (func_08B35254) and begin / capacity are replaced. Otherwise the elements are assigned in place
// (words, then each block with its use count up and the old one released): over other's whole size, destroying the surplus
// (func_08B8F680) when self was larger; over self's size, with func_08B8FFA8 copy-constructing the rest at the end, when it
// was smaller. The end becomes begin + other's size; returns self.
void func_08B90024(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t self_begin = word(rt, self);
    const std::uint32_t other_end = word(rt, other + 4u);
    const std::uint32_t other_begin = word(rt, other);
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(other_end - other_begin) >> 4);
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - self_begin) >> 4);
    // Assigns the control block `block` to the slot (use count up, old block released), nothing when it is the same one.
    const auto assign_block = [&](std::uint32_t slot, std::uint32_t old_block, std::uint32_t block) {
        if (block == old_block) return;
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t previous = word(rt, slot);
        if (previous != 0u) release_shared(rt, ctx, previous);
        put(rt, slot, block);
    };
    // Assigns one element in place: word, block, word, block.
    const auto assign_element = [&](std::uint32_t destination, std::uint32_t source) {
        const std::uint32_t first = word(rt, source);
        const std::uint32_t first_old = word(rt, destination + 4u);
        put(rt, destination, first);
        assign_block(destination + 4u, first_old, word(rt, source + 4u));
        const std::uint32_t second = word(rt, source + 8u);
        const std::uint32_t second_old = word(rt, destination + 0xCu);
        put(rt, destination + 8u, second);
        assign_block(destination + 0xCu, second_old, word(rt, source + 0xCu));
    };
    std::uint32_t bytes;
    if (capacity < count) {
        std::uint32_t block_bytes = 0u;
        std::uint32_t block = 0u;
        if (count != 0u) {
            block_bytes = count << 4;
            block = call_set(rt, ctx, 0x08B35DA0u, {block_bytes});
        }
        call_set(rt, ctx, 0x08B8FFA8u, {other_begin, other_end, block, frame.at(0u)});
        call_set(rt, ctx, 0x08B8FE44u, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        ctx.gpr[2] = block + block_bytes;
        if (word(rt, self) != 0u) {
            call_set(rt, ctx, 0x08B35254u, {word(rt, self)});
            ctx.gpr[2] = block + block_bytes;
        }
        put(rt, self + 8u, block + block_bytes);
        put(rt, self, block);
        bytes = block_bytes;
    } else {
        const std::uint32_t self_end = word(rt, self + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(self_end - self_begin) >> 4);
        bytes = count << 4;
        if (!(size < count)) {
            std::uint32_t destination = self_begin;
            if (signed_of(count) > 0) {
                std::uint32_t source = other_begin;
                for (std::uint32_t index = 0u; index != count; ++index) {
                    assign_element(destination, source);
                    source += 0x10u;
                    destination += 0x10u;
                }
            }
            call_set(rt, ctx, 0x08B8F680u, {destination, word(rt, self + 4u), frame.at(2u)});
        } else {
            std::uint32_t destination = self_begin;
            std::uint32_t source = other_begin;
            std::uint32_t copy_begin = other_begin;
            std::uint32_t copy_end = other_end;
            if (signed_of(size) > 0) {
                for (std::uint32_t index = 0u; index != size; ++index) {
                    assign_element(destination, source);
                    source += 0x10u;
                    destination += 0x10u;
                }
                copy_begin = word(rt, other);
                copy_end = word(rt, other + 4u);
            }
            const std::uint32_t old_bytes = (word(rt, self + 4u) - word(rt, self)) & ~0xFu;
            call_set(rt, ctx, 0x08B8FFA8u, {copy_begin + old_bytes, copy_end, word(rt, self + 4u), frame.at(1u)});
        }
    }
    put(rt, self + 4u, word(rt, self) + bytes);
    ctx.gpr[2] = self;
}

// func_08BB3B64(vector, position, value): the libstdc++ vector<word>::_M_insert_aux with the pool allocator inlined (the
// allocator of func_08BB6964: state 0x08D75F5C, first-use set-up behind the guard byte 0x08D75F50). With room: the last word
// is copied one past the end, the end moves and [position, end - 1) is moved up one word (func_08B79BAC) before
// *position = *value. Full: size words double (a word at the maximum 0x3FFFFFFF, func_08B5ADDC(0x08CA0A48); -4 bytes on
// overflow); (bytes + 7) / 8 + 1 units are taken from the pool's ring (split or unlinked; a pass without fit asks
// func_08B2B4D8(0x10, units * 8) for more, records it with func_08BB13D4 and merges it into the ring), the prefix is copied
// (func_08BB1CDC), the new word stored, the suffix copied, the old range destroyed (func_08BB1B8C) and its block returned to
// the ring; begin / end / capacity are replaced.
void func_08BB3B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    constexpr std::uint32_t state = 0x08D75F5Cu;
    constexpr std::uint32_t guard = 0x08D75F50u;
    std::uint32_t end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) {
            put(rt, end, word(rt, end - 4u));
            end = word(rt, vector + 4u);
        }
        put(rt, vector + 4u, end + 4u);
        const std::uint32_t bytes = (end - 4u - position) & ~3u;
        const std::uint32_t copy = word(rt, value);
        call_set(rt, ctx, 0x08B79BACu, {end - bytes, position, bytes});
        put(rt, position, copy);
        return;
    }
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - word(rt, vector)) >> 2);
    if (size == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0A48u});
    std::uint32_t bytes;
    if (size == 0u) {
        bytes = 4u;
    } else {
        bytes = (size << 1) < size ? 0xFFFFFFFCu : (size << 1) << 2;
    }
    if (sext8(byte_u(rt, guard)) == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {word(rt, 0x08CB605Cu)}) != 0u) {
            put(rt, state + 0x18u, 0x1001u);
            put(rt, state + 0xCu, 0x08D75F68u);
            put(rt, state + 0x10u, 0u);
            put(rt, state + 4u, 0u);
            put(rt, state + 8u, 0u);
            put(rt, state + 0x14u, 0x08D75F68u);
            put(rt, state + 0x1Cu, 0u);
            put(rt, state + 0x20u, 0u);
            put(rt, state + 0x24u, 0u);
            put(rt, state, 0u);
            call_set(rt, ctx, 0x08B64494u, {word(rt, 0x08CB605Cu)});
            call_set(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
        }
    }
    const std::uint32_t need = ((bytes + 7u) >> 3) + 1u;
    std::uint32_t previous = word(rt, state + 0x14u);
    std::uint32_t block = word(rt, previous);
    std::uint32_t found_size = word(rt, block + 4u);
    std::uint32_t pointer = 0u;
    for (;;) {
        if (!(found_size < need)) {
            std::uint32_t chosen = block;
            if (need == found_size) {
                put(rt, previous, word(rt, block));
            } else {
                const std::uint32_t rest = found_size - need;
                put(rt, block + 4u, rest);
                chosen = block + (rest << 3);
                put(rt, chosen + 4u, need);
            }
            const std::uint32_t used = need + word(rt, state + 0x20u);
            const std::uint32_t peak = word(rt, state + 0x24u);
            put(rt, state + 0x14u, previous);
            put(rt, state + 0x20u, used);
            if (peak < used) put(rt, state + 0x24u, used);
            pointer = chosen + 8u;
            break;
        }
        if (word(rt, state + 0x14u) != block) {
            previous = block;
            block = word(rt, previous);
            found_size = word(rt, block + 4u);
            continue;
        }
        std::uint32_t units = word(rt, state + 0x18u);
        if (!(need < units)) units = need;
        const std::uint32_t memory = call_set(rt, ctx, 0x08B2B4D8u, {0x10u, units << 3});
        put(rt, frame.at(4u), memory);
        if (memory == 0u) break;
        const std::uint32_t growth = word(rt, state + 0x18u);
        std::uint32_t vector_end = word(rt, state + 4u);
        const std::uint32_t vector_capacity = word(rt, state + 8u);
        put(rt, state + 0x18u, growth + (growth >> 1));
        if (vector_end == vector_capacity) {
            call_set(rt, ctx, 0x08BB13D4u, {state, vector_end, frame.at(4u)});
        } else {
            if (vector_end != 0u) {
                put(rt, vector_end, memory);
                vector_end = word(rt, state + 4u);
            }
            put(rt, state + 4u, vector_end + 4u);
        }
        const std::uint32_t fresh = word(rt, frame.at(4u));
        put(rt, state + 0x1Cu, units + word(rt, state + 0x1Cu));
        put(rt, fresh + 4u, units);
        const std::uint32_t total = units + word(rt, state + 0x20u);
        put(rt, state + 0x20u, total);
        if (fresh + 8u != 0u) {
            std::uint32_t at = word(rt, state + 0x14u);
            for (;;) {
                const bool below = at < fresh;
                const std::uint32_t next = word(rt, at);
                if (below && fresh < next) break;
                if (at < next) {
                    at = next;
                    continue;
                }
                if (below || fresh < next) break;
                at = next;
            }
            put(rt, state + 0x20u, total - word(rt, fresh + 4u));
            const std::uint32_t fresh_size = word(rt, fresh + 4u);
            const std::uint32_t after = word(rt, at);
            if (fresh + (fresh_size << 3) != after) {
                put(rt, fresh, after);
            } else {
                const std::uint32_t after_size = word(rt, after + 4u);
                const std::uint32_t after_next = word(rt, after);
                put(rt, fresh + 4u, fresh_size + after_size);
                put(rt, fresh, after_next);
            }
            const std::uint32_t at_size = word(rt, at + 4u);
            if (fresh != at + (at_size << 3)) {
                put(rt, at, fresh);
            } else {
                const std::uint32_t merged = word(rt, fresh + 4u);
                const std::uint32_t merged_next = word(rt, fresh);
                put(rt, at + 4u, at_size + merged);
                put(rt, at, merged_next);
            }
            put(rt, state + 0x14u, at);
        }
        previous = word(rt, state + 0x14u);
        if (previous == 0u) break;
        block = word(rt, previous);
        found_size = word(rt, block + 4u);
    }
    const std::uint32_t begin_now = word(rt, vector);
    const std::uint32_t slot = call_set(rt, ctx, 0x08BB1CDCu, {begin_now, position, pointer, frame.at(2u)});
    std::uint32_t after_slot = slot;
    if (slot != 0u) put(rt, slot, word(rt, value));
    after_slot += 4u;
    const std::uint32_t new_end = call_set(rt, ctx, 0x08BB1CDCu, {position, word(rt, vector + 4u), after_slot, frame.at(1u)});
    call_set(rt, ctx, 0x08BB1B8Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector);
    if (old_begin != 0u) {
        pool_setup(rt, ctx);
        pool_release(rt, ctx, old_begin - 8u);
    }
    ctx.gpr[2] = bytes + pointer;
    put(rt, vector + 8u, bytes + pointer);
    put(rt, vector + 4u, new_end);
    put(rt, vector, pointer);
}

// func_08BC09EC(vector, position, value): the libstdc++ vector::_M_insert_aux for 8-byte elements {word +0, shared-pointer
// control block +4}. With room: the last element is copied one past the end (its block's use count up), the end moves up,
// the value is copied to a frame element (frame +0x10, its block's use count up), the elements in [position, end - 1) are
// assigned upwards from the back (word, then block: new block's count up, old one released) and the value goes to
// *position the same way; the frame element's block is released last. Full: twice the size (1 at least, maximum 0x1FFFFFFF,
// func_08B5ADDC(0x08CA0DDC) at that size; func_08B35DA0 allocates), func_08BC099C copies the prefix, the new element is
// copied (count up) and func_08BC099C copies the suffix; the old range is destroyed (func_08BC08E4) and freed
// (func_08B35254) and begin / end / capacity updated.
void func_08BC09EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t end = word(rt, vector + 4u);
    if (end == word(rt, vector + 8u)) {
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - begin) >> 3);
        if (count == 0x1FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0DDCu});
        std::uint32_t slots;
        std::uint32_t block = 0u;
        std::uint32_t bytes = 0u;
        bool allocate = true;
        if (count == 0u) {
            slots = 1u;
        } else {
            slots = count << 1;
            if (slots < count) {
                slots = 0x1FFFFFFFu;
            } else if (slots == 0u) {
                allocate = false;
            }
        }
        if (allocate) {
            bytes = slots << 3;
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        }
        ctx.gpr[4] = word(rt, vector);
        const std::uint32_t front = call_set(rt, ctx, 0x08BC099Cu, {word(rt, vector), position, block, frame.at(2u)});
        if (front != 0u) {
            put(rt, front, word(rt, value));
            const std::uint32_t copied = word(rt, value + 4u);
            put(rt, front + 4u, copied);
            if (copied != 0u) {
                ctx.gpr[6] = copied + 4u;
                count_up(rt, ctx, copied + 4u);
            }
        }
        const std::uint32_t last = call_set(rt, ctx, 0x08BC099Cu, {position, word(rt, vector + 4u), front + 8u, frame.at(1u)});
        call_set(rt, ctx, 0x08BC08E4u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
        ctx.gpr[2] = block + bytes;
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, last);
        put(rt, vector, block);
        return;
    }
    if (end != 0u) {
        const std::uint32_t first = word(rt, end - 8u);
        const std::uint32_t shared = word(rt, end - 4u);
        put(rt, end, first);
        put(rt, end + 4u, shared);
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
    }
    put(rt, vector + 4u, word(rt, vector + 4u) + 8u);
    const std::uint32_t block = word(rt, value + 4u);
    const std::uint32_t pointer = word(rt, value);
    put(rt, frame.at(0x10u), pointer);
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const std::uint32_t top = word(rt, vector + 4u);
    std::uint32_t source = top - 0x10u;
    const std::int32_t pairs = signed_of(source - position) >> 3;
    if (pairs > 0) {
        std::uint32_t destination = top - 8u;
        for (std::int32_t index = 0; index != pairs; ++index) {
            source -= 8u;
            destination -= 8u;
            const std::uint32_t moved = word(rt, source);
            const std::uint32_t old_block = word(rt, destination + 4u);
            put(rt, destination, moved);
            const std::uint32_t new_block = word(rt, source + 4u);
            if (new_block == old_block) continue;
            if (new_block != 0u) count_up(rt, ctx, new_block + 4u);
            const std::uint32_t previous = word(rt, destination + 4u);
            if (previous != 0u) release_shared(rt, ctx, previous);
            put(rt, destination + 4u, new_block);
        }
    }
    const std::uint32_t saved = word(rt, frame.at(0x10u));
    const std::uint32_t current = word(rt, position + 4u);
    ctx.gpr[2] = current;
    put(rt, position, saved);
    if (block != current) {
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t previous = word(rt, position + 4u);
        if (previous != 0u) release_shared(rt, ctx, previous);
        put(rt, position + 4u, block);
    }
    if (block != 0u) release_shared(rt, ctx, block);
}

// func_08BDF434(vector, position, count, value): the libstdc++ vector::insert(position, count, value) for 0x1C-byte
// elements {word +0, halfword vector +4..+0xC, halfword vector +0x10..+0x18}. Nothing for count 0. The value is first copied
// to a frame element (frame +0x10; each halfword vector gets a block from func_08B35DA0 and func_08BDD8D8 / func_08BDD910
// copy into it). With room: when more than `count` elements follow the position the last `count` move up (func_08BDE70C),
// the middle ones are assigned backwards (word, then func_08BDDFDC and func_08BDEC3C on the two vectors) and func_08BDEDEC
// fills; otherwise func_08BDE5B0 fills the overflow, func_08BDE44C moves the tail and func_08BDEDEC fills the rest. Without
// room a block of size + max(size, count) elements (func_08B5ADDC(0x08CA1ABC) beyond 0x09249249) takes the prefix
// (func_08BDE18C), the new elements (func_08BDE2F0) and the suffix (func_08BDE18C); the old range is destroyed
// (func_08BDD7D8), freed (func_08B35254) and begin / end / capacity updated. The frame element's vectors are destroyed
// (func_08BDD7C8 / func_08BDD7B8) and freed at the end of the room paths.
void func_08BDF434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    constexpr std::uint32_t kInverse = 0xB6DB6DB7u;
    const std::uint32_t end_in = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end_in) >> 2) * kInverse;
    if (!(room < count)) {
        put(rt, frame.at(0x14u), 0u);
        put(rt, frame.at(0x10u), word(rt, value));
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        const std::uint32_t half_count = static_cast<std::uint32_t>(signed_of(word(rt, value + 8u) - word(rt, value + 4u)) >> 1);
        std::uint32_t bytes = 0u;
        std::uint32_t block = 0u;
        if (half_count != 0u) {
            bytes = half_count << 1;
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        }
        put(rt, frame.at(0x14u), block);
        put(rt, frame.at(0x18u), block);
        put(rt, frame.at(0x1Cu), block + bytes);
        const std::uint32_t copied = call_set(rt, ctx, 0x08BDD8D8u, {word(rt, value + 4u), word(rt, value + 8u), block, frame.at(3u)});
        put(rt, frame.at(0x20u), 0u);
        put(rt, frame.at(0x18u), copied);
        put(rt, frame.at(0x24u), 0u);
        put(rt, frame.at(0x28u), 0u);
        const std::uint32_t second_count = static_cast<std::uint32_t>(signed_of(word(rt, value + 0x14u) - word(rt, value + 0x10u)) >> 1);
        std::uint32_t second_bytes = 0u;
        std::uint32_t second_block = 0u;
        if (second_count != 0u) {
            second_bytes = second_count << 1;
            second_block = call_set(rt, ctx, 0x08B35DA0u, {second_bytes});
        }
        put(rt, frame.at(0x28u), second_block + second_bytes);
        put(rt, frame.at(0x20u), second_block);
        put(rt, frame.at(0x24u), second_block);
        const std::uint32_t second_copied =
            call_set(rt, ctx, 0x08BDD910u, {word(rt, value + 0x10u), word(rt, value + 0x14u), second_block, frame.at(2u)});
        const std::uint32_t end = word(rt, vector + 4u);
        const std::uint32_t after_raw = static_cast<std::uint32_t>(signed_of(end - position) >> 2);
        put(rt, frame.at(0x24u), second_copied);
        const std::uint32_t after = after_raw * kInverse;
        const std::uint32_t temp = frame.at(0x10u);
        if (count < after) {
            call_set(rt, ctx, 0x08BDE70Cu, {end - count * 0x1Cu, end, end, frame.at(0xAu)});
            const std::uint32_t moved_bytes = count * 0x1Cu;
            const std::uint32_t source_end = end - moved_bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 2) * kInverse;
            put(rt, vector + 4u, word(rt, vector + 4u) + moved_bytes);
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end - 0x1Cu;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    const std::uint32_t moved = word(rt, source);
                    destination -= 0x1Cu;
                    put(rt, destination, moved);
                    call_set(rt, ctx, 0x08BDDFDCu, {destination + 4u, source + 4u});
                    call_set(rt, ctx, 0x08BDEC3Cu, {destination + 0x10u, source + 0x10u});
                    source -= 0x1Cu;
                }
            }
            call_set(rt, ctx, 0x08BDEDECu, {position, position + moved_bytes, temp});
        } else {
            call_set(rt, ctx, 0x08BDE5B0u, {end, count - after, temp, frame.at(9u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 0x1Cu;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BDE44Cu, {position, end, new_end, frame.at(8u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after_raw << 2));
            call_set(rt, ctx, 0x08BDEDECu, {position, end, temp});
        }
        call_set(rt, ctx, 0x08BDD7C8u, {word(rt, frame.at(0x20u)), word(rt, frame.at(0x24u)), frame.at(1u)});
        if (word(rt, frame.at(0x20u)) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x20u))});
        call_set(rt, ctx, 0x08BDD7B8u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(0u)});
        if (word(rt, frame.at(0x14u)) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x14u))});
        return;
    }
    const std::uint32_t begin_in = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end_in - begin_in) >> 2) * kInverse;
    put(rt, frame.at(0xCu), size);
    if (0x09249249u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA1ABCu});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t total = size + larger;
    std::uint32_t bytes = 0xFFFFFFFCu;
    std::uint32_t block = 0u;
    std::uint32_t begin = begin_in;
    if (!(total < size)) {
        bytes = 0u;
        if (total != 0u) bytes = total * 0x1Cu;
    }
    if (total < size || total != 0u) {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        begin = word(rt, vector);
    }
    const std::uint32_t front = call_set(rt, ctx, 0x08BDE18Cu, {begin, position, block, frame.at(7u)});
    call_set(rt, ctx, 0x08BDE2F0u, {front, word(rt, frame.at(0x30u)), value, frame.at(6u)});
    const std::uint32_t back =
        call_set(rt, ctx, 0x08BDE18Cu, {position, word(rt, vector + 4u), front + word(rt, frame.at(0x30u)) * 0x1Cu, frame.at(5u)});
    call_set(rt, ctx, 0x08BDD7D8u, {word(rt, vector), word(rt, vector + 4u), frame.at(4u)});
    if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}

// func_08BE476C(out, pair, arg, word) -> out: pair is {object +0, shared-pointer control block +4}. A non-empty pair: under the
// lock at object +0x6C (func_08B34888 / func_08B34B24) func_08B2CB18(object + 8, arg, word) gives a handle h; a 0x18-byte
// control block {vtable 0x08CC2F90, counts 1 / 1, h, object, shared block} is allocated (func_08B62344), the shared block's
// use count rises by 4 and falls by 3 (releases) and func_08B8E380(frame +0xC, h, h) links it; frame +8 = h. An empty pair
// zeroes frame +8 / +0xC. Then func_08BE43B0(frame +0, frame +8) fills a frame pair, a 0xA0-byte object (func_08B62344;
// func_08BE45C4(object, frame +0, arg, 0)) and a 0x10-byte control block {vtable 0x08CC6928, counts 1 / 1, object} are
// allocated and stored at out +0 / +4 (func_08B8E380(out + 4, object, object)); both frame pairs' blocks (+4 and +0xC) are
// released. Returns out.
void func_08BE476C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t pair = ctx.gpr[5];
    const std::uint32_t argument = ctx.gpr[6];
    const std::uint32_t extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t shared = word(rt, pair);
    if (shared == 0u) {
        put(rt, frame.at(8u), 0u);
        put(rt, frame.at(0xCu), 0u);
    } else {
        const std::uint32_t lock = shared + 0x6Cu;
        call_set(rt, ctx, 0x08B34888u, {lock});
        const std::uint32_t handle = call_set(rt, ctx, 0x08B2CB18u, {shared + 8u, argument, extra});
        call_set(rt, ctx, 0x08B34B24u, {lock});
        const std::uint32_t block = word(rt, pair + 4u);
        const std::uint32_t object = word(rt, pair);
        if (block != 0u) count_up(rt, ctx, block + 4u);
        put(rt, frame.at(8u), handle);
        if (block != 0u) {
            count_up(rt, ctx, block + 4u);
            count_up(rt, ctx, block + 4u);
        }
        const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, control + 4u, 1u);
        put(rt, control, 0x08CC2F90u);
        put(rt, control + 8u, 1u);
        put(rt, control + 0xCu, handle);
        put(rt, control + 0x10u, object);
        put(rt, control + 0x14u, block);
        if (block != 0u) count_up(rt, ctx, block + 4u);
        put(rt, frame.at(0xCu), control);
        if (block != 0u) release_shared(rt, ctx, block);
        if (word(rt, frame.at(0xCu)) == 0u) {
            const std::uint32_t inner_lock = object + 0x6Cu;
            call_set(rt, ctx, 0x08B34888u, {inner_lock});
            call_set(rt, ctx, 0x08B2CE7Cu, {object + 8u, handle});
            const std::uint32_t count = call_set(rt, ctx, 0x08B2D1A8u, {object + 8u});
            if (count == word(rt, object + 0x68u)) call_set(rt, ctx, 0x08B34CA0u, {inner_lock});
            call_set(rt, ctx, 0x08B34B24u, {inner_lock});
        }
        if (block != 0u) release_shared(rt, ctx, block);
        call_set(rt, ctx, 0x08B8E380u, {frame.at(0xCu), handle, handle});
        if (block != 0u) release_shared(rt, ctx, block);
    }
    call_set(rt, ctx, 0x08BE43B0u, {frame.at(0u), frame.at(8u)});
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0xA0u});
    call_set(rt, ctx, 0x08BE45C4u, {object, frame.at(0u), argument, 0u});
    put(rt, out, object);
    const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, control + 4u, 1u);
    put(rt, control, 0x08CC6928u);
    put(rt, control + 8u, 1u);
    put(rt, control + 0xCu, object);
    put(rt, out + 4u, control);
    if (control == 0u && object != 0u) {
        call_set(rt, ctx, 0x08BE427Cu, {object + 0x98u, object});
        call_set(rt, ctx, 0x08B28BE0u, {object + 0x6Cu});
        call_set(rt, ctx, 0x08B2C3BCu, {object + 8u});
        const std::uint32_t block = word(rt, object + 4u);
        if (block != 0u) release_shared(rt, ctx, block);
        call_set(rt, ctx, 0x08B64654u, {object});
    }
    call_set(rt, ctx, 0x08B8E380u, {out + 4u, object, object});
    const std::uint32_t first = word(rt, frame.at(4u));
    if (first != 0u) release_shared(rt, ctx, first);
    const std::uint32_t second = word(rt, frame.at(0xCu));
    if (second != 0u) release_shared(rt, ctx, second);
    ctx.gpr[2] = out;
}

// func_08BE7DCC(vector, position, count, value): the libstdc++ vector::insert(position, count, value) for 0x60-byte
// elements {constant word +0, words +4 / +8, five floats +0xC..+0x1C, a copyable sub-object at +0x20 (copied by
// func_0896418C / assigned by func_089641C4, then its word at +0x30), another at +0x40 (word at +0x50)}. Nothing for count 0.
// The value is first copied to a frame element (frame +0x10, constant 0x08CCFF88); with room (capacity - end >= count) the
// last `count` elements are moved up (func_08BE7CD8) and the middle ones assigned backwards, func_08BE7730 fills; with
// fewer than count after the position func_08BE78E8 fills the overflow, func_08BE79E4 moves the tail and func_08BE7730
// fills the rest. Without room a block of size + max(size, count) elements (func_08B5ADDC(0x08CA2338) beyond 0x02AAAAAA;
// func_08A5BC20 allocates) takes the prefix (func_08BE7BDC), the new elements (func_08BE7AE0) and the suffix
// (func_08BE7BDC); the old range is destroyed (func_08BE5C58), freed (func_08A5BC54) and begin / end / capacity updated.
// The frame element ends with func_089D6684(frame +0x10).
void func_08BE7DCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0xB0u);
    put(rt, frame.at(0x70u), count);
    if (count == 0u) return;
    constexpr std::uint32_t kInverse = 0xAAAAAAABu;
    constexpr std::uint32_t kConstant = 0x08CCFF88u;
    const std::uint32_t temp = frame.at(0x10u);
    const std::uint32_t end_in = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end_in) >> 5) * kInverse;
    if (!(room < count)) {
        const std::uint32_t word4 = word(rt, value + 4u);
        const std::uint32_t word8 = word(rt, value + 8u);
        std::uint32_t floats[5];
        for (std::uint32_t index = 0u; index < 5u; ++index) floats[index] = word(rt, value + 0xCu + index * 4u);
        const std::uint32_t first = value + 0x20u;
        const std::uint32_t second = value + 0x40u;
        put(rt, frame.at(0x14u), word4);
        put(rt, frame.at(0x18u), word8);
        for (std::uint32_t index = 0u; index < 5u; ++index) put(rt, frame.at(0x1Cu) + index * 4u, floats[index]);
        put(rt, temp, kConstant);
        call_set(rt, ctx, 0x0896418Cu, {frame.at(0x30u), first, word8});
        put(rt, frame.at(0x40u), word(rt, first + 0x10u));
        call_set(rt, ctx, 0x0896418Cu, {frame.at(0x50u), second, word8});
        const std::uint32_t end = word(rt, vector + 4u);
        put(rt, frame.at(0x60u), word(rt, second + 0x10u));
        const std::uint32_t after_raw = static_cast<std::uint32_t>(signed_of(end - position) >> 5);
        const std::uint32_t after = after_raw * kInverse;
        std::uint32_t destination = end;
        if (count < after) {
            const std::uint32_t bytes = count * 0x60u;
            call_set(rt, ctx, 0x08BE7CD8u, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 5) * kInverse;
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            std::uint32_t source = source_end;
            if (signed_of(middle) > 0) {
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    const std::uint32_t from = source - 0x60u;
                    const std::uint32_t word4_in = word(rt, from + 4u);
                    const std::uint32_t word8_in = word(rt, from + 8u);
                    std::uint32_t moved[5];
                    for (std::uint32_t k = 0u; k < 5u; ++k) moved[k] = word(rt, from + 0xCu + k * 4u);
                    destination -= 0x60u;
                    const std::uint32_t sub_to = destination + 0x20u;
                    const std::uint32_t sub_from = from + 0x20u;
                    put(rt, destination + 4u, word4_in);
                    source = from;
                    put(rt, destination + 8u, word8_in);
                    for (std::uint32_t k = 0u; k < 5u; ++k) put(rt, destination + 0xCu + k * 4u, moved[k]);
                    if (sub_to != sub_from) {
                        call_set(rt, ctx, 0x089641C4u, {sub_to, sub_from});
                        put(rt, sub_to + 0x10u, word(rt, sub_from + 0x10u));
                    }
                    const std::uint32_t sub2_from = from + 0x40u;
                    const std::uint32_t sub2_to = destination + 0x40u;
                    if (sub2_to != sub2_from) {
                        call_set(rt, ctx, 0x089641C4u, {sub2_to, sub2_from});
                        put(rt, sub2_to + 0x10u, word(rt, sub2_from + 0x10u));
                    }
                }
            }
            call_set(rt, ctx, 0x08BE7730u, {position, position + count * 0x60u, temp});
        } else {
            call_set(rt, ctx, 0x08BE78E8u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 0x60u;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BE79E4u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after_raw << 5));
            call_set(rt, ctx, 0x08BE7730u, {position, end, temp});
        }
        put(rt, temp, kConstant);
        call_set(rt, ctx, 0x089D6684u, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end_in - begin) >> 5) * kInverse;
    put(rt, frame.at(8u), size);
    if (0x02AAAAAAu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA2338u});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t total = size + larger;
    std::uint32_t bytes = 0xFFFFFFC0u;
    if (!(total < size)) bytes = total * 0x60u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t front = call_set(rt, ctx, 0x08BE7BDCu, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BE7AE0u, {front, word(rt, frame.at(0x70u)), value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BE7BDCu, {position, word(rt, vector + 4u), front + word(rt, frame.at(0x70u)) * 0x60u, frame.at(1u)});
    call_set(rt, ctx, 0x08BE5C58u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector);
    const std::uint32_t old_size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_begin) >> 5);
    if (old_begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_begin, old_size << 5, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}

} // namespace hot_batch_117
} // namespace gt::game

#define HOT117(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_117::name, gt::ReturnKind::kind)

HOT117(0x088103DCu, func_088103DC, Word);
HOT117(0x08825C00u, func_08825C00, Word);
HOT117(0x08834670u, func_08834670, Word);
HOT117(0x08838990u, func_08838990, Word);
HOT117(0x088541F0u, func_088541F0, Word);
HOT117(0x088F3BC4u, func_088F3BC4, Word);
HOT117(0x0890F1C0u, func_0890F1C0, Word);
HOT117(0x08921F70u, func_08921F70, Word);
HOT117(0x0894106Cu, func_0894106C, Word);
HOT117(0x08946130u, func_08946130, Word);
HOT117(0x08948D04u, func_08948D04, Float);
HOT117(0x089525ECu, func_089525EC, Word);
HOT117(0x08957478u, func_08957478, Word);
HOT117(0x08963418u, func_08963418, Word);
HOT117(0x0897BC08u, func_0897BC08, Word);
HOT117(0x089AE690u, func_089AE690, Word);
HOT117(0x089F25E0u, func_089F25E0, Word);
HOT117(0x08A13004u, func_08A13004, Word);
HOT117(0x08A517B4u, func_08A517B4, Word);
HOT117(0x08B90024u, func_08B90024, Word);
HOT117(0x08BB3B64u, func_08BB3B64, Word);
HOT117(0x08BC09ECu, func_08BC09EC, Word);
HOT117(0x08BDF434u, func_08BDF434, Word);
HOT117(0x08BE476Cu, func_08BE476C, Word);
HOT117(0x08BE7DCCu, func_08BE7DCC, Word);
