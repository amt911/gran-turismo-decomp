// Hot batch 115: functions of 185-239 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_115 {
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

// Copies `count` words from `source` to `destination` (a struct copy the original unrolls into 16-byte groups).
inline void copy_words(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t count) {
    for (std::uint32_t index = 0u; index < count; ++index) put(rt, destination + index * 4u, word(rt, source + index * 4u));
}

// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}
// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24);
}
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (index * 8u));
}

// Puts `item` (when non-null) on the free chain of the size-class record that func_08BB1F48()'s table gives for `key`
// (looked up, or made with func_08BB1D0C(+0x24, +0x28, &key, 0x08BB1B40) when missing or of another class): the record's
// +0x1C count goes down and the chain head (+0xC) takes the item; v0 = the new count and v1 = the old head then.
inline void return_to_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key, std::uint32_t item, std::uint32_t key_slot) {
    const std::uint32_t context = call_set(rt, ctx, 0x08BB1F48u, {});
    put(rt, key_slot, key);
    std::uint32_t record = word(rt, context + 0x34u);
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
        ctx.gpr[3] = head;
    }
    ctx.gpr[4] = record;
}

// func_08AD0C9C(self, list): under the lock at self + 4 (after self's virtual slot 0x6C), for up to 8 entries of `list` (stop
// at a null one, skip empty strings): func_08B0EC60 fills a 60-byte record in the frame; when its first word is 0 the size
// word at +0x10 rounded up to 128 goes to a frame size table and total += size + 128. With a total, memory is claimed
// (func_08B2E0B0, func_08B35418 limit, func_08B35DA0 for half the spare) and each sized entry gets func_08B15490(size, 1).
// A second pass zeroes self->+0x74[i] and, for sized entries, builds a record with func_08B10170, keeps its +0x20 word in
// self->+0x74[i] and, when func_08AF6CE8 accepts it, runs func_08AF771C(obj, 0) and func_08AF7B0C(obj, -1). The unlock is
// a tail jump to func_08B34B24.
void func_08AD0C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[5];
    GuestFrame frame(ctx, 0x140u);
    const std::uint32_t lock = self + 4u;
    put(rt, frame.at(0x100u), lock);
    call_virtual_at(rt, ctx, self, 0x6Cu, {self, list});
    call_set(rt, ctx, 0x08B34888u, {lock});
    for (std::uint32_t offset = 0u; offset != 0x20u; offset += 4u) {
        put(rt, frame.at(offset), 0u);
        put(rt, frame.at(0x20u + offset), 0u);
    }
    std::uint32_t total = 0u;
    for (std::uint32_t index = 0u; index < 8u; ++index) {
        const std::uint32_t entry = word(rt, list + index * 4u);
        if (entry == 0u) break;
        if (sext8(byte_u(rt, entry)) == 0u) continue;
        call_set(rt, ctx, 0x08B0EC60u, {frame.at(0x80u), entry});
        copy_words(rt, frame.at(0x40u), frame.at(0x80u), 15u);
        if (word(rt, frame.at(0x40u)) != 0u) continue;
        const std::uint32_t size = (word(rt, frame.at(0x50u)) + 0x7Fu) & ~0x7Fu;
        put(rt, frame.at(0x20u + index * 4u), size);
        total += size + 0x80u;
    }
    if (total != 0u) {
        const std::uint32_t saved = call_set(rt, ctx, 0x08B2E0B0u, {});
        const std::uint32_t limit = call_set(rt, ctx, 0x08B35418u, {});
        std::uint32_t block = 0u;
        if (total < limit && ((limit - total) >> 1) != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {(limit - total) >> 1});
        for (std::uint32_t index = 0u; index < 8u; ++index) {
            const std::uint32_t entry = word(rt, list + index * 4u);
            if (entry == 0u) break;
            if (sext8(byte_u(rt, entry)) == 0u) continue;
            const std::uint32_t size = word(rt, frame.at(0x20u + index * 4u));
            if (size == 0u) continue;
            put(rt, frame.at(index * 4u), call_set(rt, ctx, 0x08B15490u, {size, 1u}));
        }
        call_set(rt, ctx, 0x08B35254u, {block});
        call_set(rt, ctx, 0x08B2E0A4u, {saved});
    }
    for (std::uint32_t index = 0u; index < 8u; ++index) {
        const std::uint32_t entry = word(rt, list + index * 4u);
        const std::uint32_t slot = self + 0x74u + index * 4u;
        put(rt, slot, 0u);
        if (entry == 0u) break;
        if (sext8(byte_u(rt, entry)) == 0u) continue;
        const std::uint32_t size = word(rt, frame.at(0x20u + index * 4u));
        if (size == 0u) continue;
        call_set(rt, ctx, 0x08B10170u, {frame.at(0xC0u), entry, word(rt, frame.at(index * 4u)), size, 1u});
        copy_words(rt, frame.at(0x40u), frame.at(0xC0u), 15u);
        const std::uint32_t object = word(rt, frame.at(0x60u));
        put(rt, slot, object);
        if (call_set(rt, ctx, 0x08AF6CE8u, {object}) == 0u) continue;
        call_set(rt, ctx, 0x08AF771Cu, {object, 0u});
        call_set(rt, ctx, 0x08AF7B0Cu, {object, 0xFFFFFFFFu});
    }
    tail_call(ctx, 0x08B34B24u, {word(rt, frame.at(0x100u))});
}
// func_08C11318(self, other): the libstdc++ vector<96-byte element>::operator= (returns self; nothing when other is self).
// With n = other's count: when self's capacity is below n a block of n elements is made (func_08A5BC20(bytes, 4)),
// filled by func_08C10EE8, the old range destroyed (func_08BE5C58) and freed (func_08A5BC54); else the first min(n, size)
// elements are assigned one by one (words +4, +8, five more words +0xC..+0x1C, then the two 32-byte parts at +0x20 and
// +0x40 through func_089641C4 unless identical, each followed by its word +0x10) and the rest is constructed
// (func_08C10EE8) or the surplus destroyed (func_08BE59D4). self->+4 = self->+0 + n * 96.
void func_08C11318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10u), other);
    if (other != self) {
        const auto assign_part = [&](std::uint32_t target, std::uint32_t source) {
            if (target != source) {
                call_set(rt, ctx, 0x089641C4u, {target, source});
                put(rt, target + 0x10u, word(rt, source + 0x10u));
            }
        };
        const auto assign_element = [&](std::uint32_t target, std::uint32_t source) {
            put(rt, target + 4u, word(rt, source + 4u));
            put(rt, target + 8u, word(rt, source + 8u));
            for (std::uint32_t offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, target + offset, word(rt, source + offset));
            assign_part(target + 0x20u, source + 0x20u);
            assign_part(target + 0x40u, source + 0x40u);
        };
        std::uint32_t source = word(rt, other);
        const std::uint32_t source_end = word(rt, other + 4u);
        const std::uint32_t begin = word(rt, self);
        const std::uint32_t end = word(rt, self + 4u);
        const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - begin) >> 5) * 0xAAAAAAABu;
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(source_end - source) >> 5) * 0xAAAAAAABu;
        if (capacity < count) {
            const std::uint32_t bytes = static_cast<std::uint32_t>(signed_of(source_end - source) >> 5) << 5;
            const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
            call_set(rt, ctx, 0x08C10EE8u, {source, source_end, block, frame.at(0u)});
            call_set(rt, ctx, 0x08BE5C58u, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
            const std::uint32_t old = word(rt, self);
            if (old != 0u) call_set(rt, ctx, 0x08A5BC54u, {old, static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old) >> 5) << 5, 4u});
            put(rt, self + 8u, block + bytes);
            put(rt, self, block);
            put(rt, self + 4u, block + bytes);
        } else {
            const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 5) * 0xAAAAAAABu;
            std::uint32_t target = begin;
            if (!(size < count)) {
                for (std::uint32_t done = 0u; signed_of(count) > 0 && done != count; ++done) {
                    assign_element(target, source);
                    target += 0x60u;
                    source += 0x60u;
                }
                call_set(rt, ctx, 0x08BE59D4u, {target, end, frame.at(2u)});
            } else {
                put(rt, frame.at(0x14u), size);
                for (std::uint32_t done = 0u; signed_of(size) > 0 && done != size; ++done) {
                    assign_element(target, source);
                    target += 0x60u;
                    source += 0x60u;
                }
                call_set(rt, ctx, 0x08C10EE8u, {word(rt, other) + ((end - begin) & ~31u), word(rt, other + 4u), end, frame.at(1u)});
            }
            put(rt, self + 4u, word(rt, self) + (count << 7) - (count << 5));
        }
    }
    ctx.gpr[2] = self;
}
// func_089C06C0(self, target): nothing (returns the byte) when the byte self->+0x2AC is set. Under the lock at self + 0x2B0
// (func_08B344E0 / func_08B3457C): with o = self->+0x264 (virtual slot 0x20 first): when self->+0x2A8 >= 0 and o's slot
// 0x4C equals it, a string made by func_08B85EB8 from self + 0x268 goes through func_08A98C1C / func_0899C764(tmp,
// target, self, str, obj) / func_089A9324(target, tmp), the temporaries are destroyed and self->+0x2A8 = -1. Then if o's
// slot 0x48 is non-zero the same with the string 0x08CA5F04 (checking byte self->+0x242 afterwards); if that byte is clear
// and o's slot 0x44 is non-zero the same with the string 0x08CA5F14.
void func_089C06C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t flag = byte_u(rt, self + 0x2ACu);
    if (flag != 0u) {
        ctx.gpr[2] = flag;
        return;
    }
    const std::uint32_t lock = self + 0x2B0u;
    constexpr std::uint32_t kEmptyString = 0x08D74FD0u;
    constexpr std::uint32_t kStringFree = 0x08B8542Cu;
    const auto emit = [&](std::uint32_t text, std::uint32_t object, std::uint32_t temporary, std::uint32_t source, std::uint32_t construct_frame,
                          std::uint32_t release_frame) {
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(text), source, frame.at(construct_frame)});
        call_set(rt, ctx, 0x08A98C1Cu, {frame.at(object), 0u});
        call_set(rt, ctx, 0x0899C764u, {frame.at(temporary), target, self, frame.at(text), frame.at(object)});
        call_set(rt, ctx, 0x089A9324u, {target, frame.at(temporary)});
        call_set(rt, ctx, kDestroy, {frame.at(temporary)});
        call_set(rt, ctx, kDestroy, {frame.at(object)});
        release_string(rt, ctx, frame.at(text), kEmptyString, kStringFree, frame.at(release_frame));
    };
    const auto holder = [&]() { return checked(rt, ctx, word(rt, self + 0x264u), 0x08CA5EA4u); };
    call_set(rt, ctx, kLock, {lock});
    do {
        std::uint32_t object = word(rt, self + 0x264u);
        if (object == 0u) break;
        call_virtual_v1(rt, ctx, object, 0x20u, {object});
        if (signed_of(word(rt, self + 0x2A8u)) >= 0) {
            object = holder();
            if (call_virtual_at(rt, ctx, object, 0x4Cu, {object}) == word(rt, self + 0x2A8u)) {
                emit(0x24u, 0x20u, 0x28u, self + 0x268u, 5u, 2u);
                put(rt, self + 0x2A8u, 0xFFFFFFFFu);
                ctx.gpr[2] = 0xFFFFFFFFu;
            }
        }
        object = holder();
        if (call_virtual_at(rt, ctx, object, 0x48u, {object}) != 0u) emit(0x18u, 0x14u, 0x1Cu, 0x08CA5F04u, 4u, 1u);
        const std::uint32_t flag_byte = byte_u(rt, self + 0x242u);
        ctx.gpr[2] = flag_byte;
        if (flag_byte != 0u) break;
        object = holder();
        if (call_virtual_at(rt, ctx, object, 0x44u, {object}) == 0u) break;
        emit(0xCu, 8u, 0x10u, 0x08CA5F14u, 3u, 0u);
    } while (false);
    call_set(rt, ctx, kUnlock, {lock});
}
// func_08A43398(a0, key, count, args): nothing when count <= 0. With count 1: func_08A40E90(tmp1, key) gives a target
// (checked), args[0]'s virtual slot 0xC fills tmp2, func_08A40C14(tmp3, 0x08CF6ED8) a constant, func_08A430A8(target, tmp2,
// tmp3) takes them. With count >= 2: when func_08A76F20(args[1], func_08A40AD0()) holds, the same with tmp3 made from slot
// 0xC of args[1] by func_08A40E90 (via the address args + 4); otherwise both args[0] and args[1] fill a pair of temporaries
// for func_08A43288(target, tmp_a, tmp_b). Temporaries are destroyed or released (strings) on the way out.
void func_08A43398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (signed_of(count) <= 0) return;
    constexpr std::uint32_t kEmptyString = 0x08D74FD0u;
    constexpr std::uint32_t kStringFree = 0x08B8542Cu;
    const auto target_of = [&](std::uint32_t slot) {
        call_set(rt, ctx, 0x08A40E90u, {slot, key});
        return checked(rt, ctx, word(rt, slot), 0x08CADBFCu);
    };
    const auto fill_from_first = [&](std::uint32_t destination) {
        const std::uint32_t object = checked(rt, ctx, word(rt, args), 0x08CADB98u);
        call_virtual_v1(rt, ctx, object, 0xCu, {destination, object});
    };
    if (signed_of(count) < 2) {
        const std::uint32_t target = target_of(frame.at(0xCu));
        fill_from_first(frame.at(8u));
        call_set(rt, ctx, 0x08A40C14u, {frame.at(4u), 0x08CF6ED8u});
        call_set(rt, ctx, 0x08A430A8u, {target, frame.at(8u), frame.at(4u)});
        call_set(rt, ctx, kDestroy, {frame.at(4u)});
        release_string(rt, ctx, frame.at(8u), kEmptyString, kStringFree, frame.at(0u));
        call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
        return;
    }
    const std::uint32_t second = checked(rt, ctx, word(rt, args + 4u), 0x08CADB98u);
    const std::uint32_t selector = call_set(rt, ctx, 0x08A40AD0u, {});
    if (call_set(rt, ctx, 0x08A76F20u, {second, selector}) != 0u) {
        const std::uint32_t temporary = frame.at(0x24u);
        const std::uint32_t target = target_of(temporary);
        fill_from_first(frame.at(0x20u));
        call_set(rt, ctx, 0x08A40E90u, {frame.at(0x1Cu), args + 4u});
        call_set(rt, ctx, 0x08A430A8u, {target, frame.at(0x20u), frame.at(0x1Cu)});
        call_set(rt, ctx, kDestroy, {frame.at(0x1Cu)});
        release_string(rt, ctx, frame.at(0x20u), kEmptyString, kStringFree, frame.at(3u));
        call_set(rt, ctx, kDestroy, {temporary});
        return;
    }
    const std::uint32_t temporary = frame.at(0x18u);
    const std::uint32_t target = target_of(temporary);
    fill_from_first(frame.at(0x14u));
    const std::uint32_t other = checked(rt, ctx, word(rt, args + 4u), 0x08CADB98u);
    call_virtual_at(rt, ctx, other, 0xCu, {frame.at(0x10u), other});
    call_set(rt, ctx, 0x08A43288u, {target, frame.at(0x14u), frame.at(0x10u)});
    release_string(rt, ctx, frame.at(0x10u), kEmptyString, kStringFree, frame.at(2u));
    release_string(rt, ctx, frame.at(0x14u), kEmptyString, kStringFree, frame.at(1u));
    call_set(rt, ctx, kDestroy, {temporary});
}
// func_089FB5B4(self, target): with n = self->+8 > 0: a block of n 32-byte elements (func_08B35840(16, bytes)) is filled with
// a template {1.0, 1.0, 1.0, 1.0, -1} by func_08BE6028; for each element target's virtual slot 8 gets a frame record {0x08CCE190,
// empty string, -1, element} (its string released after, the record's vtable word set to 0x08CCD990); then func_089FA398 /
// func_089FB570(self->+0xC, .) / func_089F9B74 consume the range, which is destroyed (func_08BE5888) and freed
// (func_08B35254). With n <= 0: the strings func_08B88F44(self + 4) and func_08B85EB8(0x08CA86A0) are compared by length and
// func_08B36170 (a non-zero difference ends it); equal ones ask func_08987A74(target, empty string) twice and, when that
// answers 0x102, run func_089FB374 / func_089FB570 / func_089F9B74 on self->+0xC; then func_0898682C(target, 1).
void func_089FB5B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0xC0u);
    constexpr std::uint32_t kEmptyString = 0x08D74FD0u;
    constexpr std::uint32_t kStringFree = 0x08B8542Cu;
    put(rt, frame.at(0x80u), self);
    const std::uint32_t count = word(rt, self + 8u);
    if (signed_of(count) > 0) {
        const std::uint32_t bytes = count << 5;
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        put(rt, frame.at(0x20u), 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08B35840u, {0x10u, bytes});
        put(rt, frame.at(0x20u), block + bytes);
        put(rt, frame.at(0x70u), 0xFFFFFFFFu);
        put(rt, frame.at(0x18u), block);
        put(rt, frame.at(0x1Cu), block);
        for (const std::uint32_t offset : {0x60u, 0x6Cu, 0x68u, 0x64u}) put_float(rt, frame.at(offset), 1.0f);
        ctx.fpr[0] = 1.0f;
        call_set(rt, ctx, 0x08BE6028u, {block, count, frame.at(0x60u), frame.at(6u)});
        put(rt, frame.at(0x1Cu), bytes + word(rt, frame.at(0x18u)));
        for (std::uint32_t index = 0u; index != count;) {
            const std::uint32_t vtable = word(rt, target);
            put(rt, frame.at(0x50u), word(rt, frame.at(0x18u)) + (index << 5));
            put(rt, frame.at(0x4Cu), 0xFFFFFFFFu);
            put(rt, frame.at(0x44u), 0x08CCE190u);
            put(rt, frame.at(0x48u), kEmptyString + 0xCu);
            ++index;
            ctx.gpr[2] = word(rt, vtable + 8u);
            call_set(rt, ctx, ctx.gpr[2], {target, frame.at(0x44u), 0x08CCE190u, vtable});
            put(rt, frame.at(0x44u), 0x08CCD990u);
            release_string(rt, ctx, frame.at(0x48u), kEmptyString, kStringFree, frame.at(5u));
        }
        const std::uint32_t holder = word(rt, self + 0xCu);
        call_set(rt, ctx, 0x089FA398u, {frame.at(0x34u), frame.at(0x18u)});
        call_set(rt, ctx, 0x089FB570u, {holder, frame.at(0x34u)});
        call_set(rt, ctx, 0x089F9B74u, {frame.at(0x34u)});
        call_set(rt, ctx, 0x08BE5888u, {word(rt, frame.at(0x18u)), word(rt, frame.at(0x1Cu)), frame.at(4u)});
        const std::uint32_t old = word(rt, frame.at(0x18u));
        if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
        return;
    }
    call_set(rt, ctx, 0x08B88F44u, {frame.at(0xCu), self + 4u});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(8u), 0x08CA86A0u, frame.at(3u)});
    const std::uint32_t left = word(rt, frame.at(0xCu));
    const std::uint32_t right = word(rt, frame.at(8u));
    const std::uint32_t left_length = word(rt, left - 0xCu);
    const std::uint32_t right_length = word(rt, right - 0xCu);
    put(rt, frame.at(0x10u), left_length);
    put(rt, frame.at(0x14u), right_length);
    std::uint32_t difference = left_length - right_length;
    const std::uint32_t common = right_length < left_length ? right_length : left_length;
    const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {left, right, common, left_length});
    if (order != 0u) difference = order;
    release_string(rt, ctx, frame.at(8u), kEmptyString, kStringFree, frame.at(2u));
    release_string(rt, ctx, frame.at(0xCu), kEmptyString, kStringFree, frame.at(1u));
    if (difference != 0u) return;
    put(rt, frame.at(0x18u), kEmptyString + 0xCu);
    call_set(rt, ctx, 0x08987A74u, {target, frame.at(0x18u)});
    if (call_set(rt, ctx, 0x08987A74u, {target, frame.at(0x18u)}) == 0x102u) {
        const std::uint32_t holder = word(rt, self + 0xCu);
        call_set(rt, ctx, 0x089FB374u, {frame.at(0x24u), frame.at(0x18u)});
        call_set(rt, ctx, 0x089FB570u, {holder, frame.at(0x24u)});
        call_set(rt, ctx, 0x089F9B74u, {frame.at(0x24u)});
    }
    call_set(rt, ctx, 0x0898682Cu, {target, 1u});
    release_string(rt, ctx, frame.at(0x18u), kEmptyString, kStringFree, frame.at(0u));
}
// func_08B13388(self): func_08B33790(self + 0xAC); a frame request {self + 0x12C, 0x80, 0} goes through func_08B1262C /
// func_08B23A70(.., 0x08CB4BD4); then func_08C7A848(self + 0xAC, request) is polled (func_08B2E064 between tries) until it
// answers 0 and func_08B33C0C(.., 1, 0) returns >= 0 (self->+0x1B0 / +0x1B4 take frame +0x48 and that result). A 64-byte-aligned
// frame buffer is read by func_08B11F30; unless its first word is 0x71D319F3 or 0x515111D3 that is all (v0 = 0x515111D3).
// Otherwise its fields are normalised (func_08B1222C), its first 40 bytes copied to self->+0x208 and the buffers made
// (func_08B15490) and read in (func_08B11F30) per the format word, ending with func_08B11E3C(self).
void func_08B13388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0xF0u);
    const std::uint32_t object = self + 0xACu;
    const std::uint32_t request = frame.at(8u);
    call_set(rt, ctx, 0x08B33790u, {object});
    put8(rt, self + 0x12Cu, 0u);
    put(rt, request, self + 0x12Cu);
    put(rt, request + 4u, 0x80u);
    put(rt, request + 8u, 0u);
    const std::uint32_t handle = call_set(rt, ctx, 0x08B1262Cu, {request});
    call_set(rt, ctx, 0x08B23A70u, {handle, 0x08CB4BD4u});
    for (;;) {
        if (call_set(rt, ctx, 0x08C7A848u, {object, request}) == 0u) {
            put(rt, self + 0x1B0u, word(rt, frame.at(0x48u)));
            const std::uint32_t result = call_set(rt, ctx, 0x08B33C0Cu, {object, 1u, 0u});
            put(rt, self + 0x1B4u, result);
            if (signed_of(result) >= 0) break;
        }
        call_set(rt, ctx, 0x08B2E064u, {});
    }
    const std::uint32_t buffer = frame.at(0x9Fu) & ~0x3Fu;
    call_set(rt, ctx, 0x08B11F30u, {self, 0u, buffer, 0x40u});
    const std::uint32_t magic = word(rt, buffer);
    if (magic != 0x71D319F3u && magic != 0x515111D3u) {
        ctx.gpr[2] = 0x515111D3u;
        return;
    }
    const std::uint32_t fields[7][2] = {{4u, 4u}, {8u, 8u}, {0x10u, 4u}, {0x14u, 4u}, {0x18u, 4u}, {0x1Cu, 4u}, {0x20u, 4u}};
    for (const auto &field : fields) call_set(rt, ctx, 0x08B1222Cu, {buffer + field[0], buffer + field[0], field[1]});
    copy_words(rt, self + 0x208u, buffer, 10u);
    put(rt, self + 0x230u, 1u);
    put(rt, self + 0x234u, 0x800u);
    put(rt, self + 0x238u, 0xFFFFFFFFu);
    const std::uint32_t format = word(rt, self + 0x208u);
    put(rt, self + 0x23Cu, call_set(rt, ctx, 0x08B15490u, {0x800u, 1u}));
    const std::uint32_t total = word(rt, self + 0x230u) + word(rt, self + 0x218u);
    put(rt, self + 0x244u, total << 11);
    put(rt, self + 0x240u, total);
    std::uint32_t length;
    if (format != 0x515111D3u) {
        length = word(rt, self + 0x220u) * 2u + 2u;
        const std::uint32_t block = call_set(rt, ctx, 0x08B15490u, {length, 1u});
        put(rt, self + 0x248u, block);
        call_set(rt, ctx, 0x08B11F30u, {self, word(rt, self + 0x244u), block, length});
    } else {
        length = word(rt, self + 0x224u);
        const std::uint32_t block = call_set(rt, ctx, 0x08B15490u, {length, 1u});
        put(rt, self + 0x248u, block);
        call_set(rt, ctx, 0x08B11F30u, {self, word(rt, self + 0x244u), block, length});
        put(rt, self + 0x98u, 0x2800u);
        length = word(rt, self + 0x220u) * 4u + 4u;
    }
    const std::uint32_t data = word(rt, self + 0x248u);
    call_set(rt, ctx, 0x08B1222Cu, {data, data, length});
    const std::uint32_t pool = call_set(rt, ctx, 0x08B15490u, {0xC000u, 1u});
    put(rt, self + 0x24Cu, pool);
    put(rt, frame.at(0u), pool);
    put(rt, frame.at(4u), 0xC000u);
    call_set(rt, ctx, 0x08B2B918u, {self + 0x280u, frame.at(0u)});
    const std::uint32_t sum = word(rt, self + 0x240u) + word(rt, self + 0x21Cu);
    const std::uint32_t words = (word(rt, self + 0x228u) + 7u) >> 3;
    put(rt, self + 0x260u, 0xFFFFFFFFu);
    put(rt, self + 0x250u, sum);
    put(rt, self + 0x25Cu, 0xFFFFFFFFu);
    put(rt, self + 0x264u, words);
    put(rt, self + 0x254u, sum << 11);
    put(rt, self + 0x2D0u, 0u);
    put(rt, self + 0x4D4u, 0u);
    put8(rt, self + 0x258u, 0u);
    put(rt, self + 0x268u, call_set(rt, ctx, 0x08B2B5E0u, {words}));
    put(rt, self + 0x26Cu, call_set(rt, ctx, 0x08B2B5E0u, {word(rt, self + 0x264u)}));
    put(rt, self + 0x270u, call_set(rt, ctx, 0x08B15490u, {0x800u, 1u}));
    call_set(rt, ctx, 0x08B11E3Cu, {self});
}
// func_08B1554C(handle, destination, size): under the lock 0x08D47840 reads `size` bytes through func_08B0C650(handle,
// {address, length}) requests: a head of up to 16 - (destination & 15) bytes straight to the destination, then chunks of
// whole 16-byte units (at most 0x8000) into the buffer 0x08D47880 that are copied on to the destination (128 bytes at a
// time, then 16), and a final request for the remaining 1-15 bytes. The first negative answer is returned, else size.
void func_08B1554C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    const std::uint32_t destination = ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    constexpr std::uint32_t kReadLock = 0x08D47840u;
    constexpr std::uint32_t kBuffer = 0x08D47880u;
    put(rt, frame.at(0x58u), handle);
    put(rt, frame.at(0x5Cu), size);
    call_set(rt, ctx, kLock, {kReadLock});
    const auto transfer = [&]() -> std::uint32_t {
        std::uint32_t cursor = destination;
        put(rt, frame.at(0x20u), size);
        const std::uint32_t misalignment = destination & 0xFu;
        if (misalignment != 0u) {
            std::uint32_t head = 0x10u - misalignment;
            if (size < head) head = size;
            put(rt, frame.at(0x10u), destination);
            cursor = destination + head;
            put(rt, frame.at(0x20u), size - head);
            put(rt, frame.at(0x14u), head);
            const std::uint32_t answer = call_set(rt, ctx, 0x08B0C650u, {handle, frame.at(0x10u)});
            if (signed_of(answer) < 0) return answer;
        }
        if (word(rt, frame.at(0x20u)) >= 0x10u) {
            for (;;) {
                const std::uint32_t remaining = word(rt, frame.at(0x20u));
                std::uint32_t chunk = remaining & ~0xFu;
                if (chunk == 0u) break;
                if (chunk > 0x7FFFu) chunk = 0x8000u;
                put(rt, frame.at(8u), kBuffer);
                put(rt, frame.at(0x20u), remaining - chunk);
                put(rt, frame.at(0xCu), chunk);
                const std::uint32_t answer = call_set(rt, ctx, 0x08B0C650u, {handle, frame.at(8u)});
                if (signed_of(answer) < 0) return answer;
                const std::uint32_t blocks = chunk >> 7;
                const std::uint32_t units = chunk >> 4;
                put(rt, frame.at(0x50u), blocks);
                put(rt, frame.at(0x54u), units);
                std::uint32_t from = kBuffer;
                for (std::uint32_t block = 0u; block < blocks; ++block) {
                    copy_words(rt, cursor, from, 32u);
                    from += 0x80u;
                    cursor += 0x80u;
                }
                for (std::uint32_t unit = 0u; unit < (units & 7u); ++unit) {
                    copy_words(rt, cursor, from, 4u);
                    from += 0x10u;
                    cursor += 0x10u;
                }
                if (word(rt, frame.at(0x20u)) < 0x10u) break;
            }
        }
        const std::uint32_t rest = word(rt, frame.at(0x20u));
        if (rest == 0u) return size;
        put(rt, frame.at(0u), cursor);
        put(rt, frame.at(4u), rest);
        const std::uint32_t answer = call_set(rt, ctx, 0x08B0C650u, {handle, frame.at(0u)});
        if (signed_of(answer) < 0) return answer;
        return size;
    };
    const std::uint32_t result = transfer();
    call_set(rt, ctx, kUnlock, {kReadLock});
    ctx.gpr[2] = result;
}
// func_08AFE0C4(self, a, b): 0 unless the half self->+0xE2 has bit 0 and self->+0xD0 >= 0. With origin = self + 0xB0 and
// the vector (self->+0x20.., 1.0): returns 0 when self->+0xD8's float +0x20 is below func_08C46364(origin, vector). Else
// func_08BAC230(self) / func_08C45F58 give a record whose negated plane + w goes (func_08C4624C) into a frame vector; the
// distance func_08BBBEC4 adds to its fourth float. For each of self->+0xD8's half-+0x7E groups (0x200 bytes apart from
// +0x80), each 16-byte item up to the count in the group's word +0x1FC bits 8-11 whose func_08C46364 distance is
// negative gets an object from func_08B01D1C(a, &out, 0, b, group, item, record, -1) (the extra words on the stack):
// its floats +0x30 = -distance, +0x24 / +0x28 = half-sums of the owner's +0x64/+0x68 and self's +0xC0/+0xC4, +0x2C =
// owner's +0x6C x self->+0xC8. A zero object returns whether `out` is set; at the end `out` gets bit 0 on its half and
// func_08B012D8(out, a) runs. Returns (out != 0).
void func_08AFE0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first_argument = ctx.gpr[5];
    const std::uint32_t second_argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0xD0u);
    put(rt, frame.at(0x80u), first_argument);
    put(rt, frame.at(0x84u), second_argument);
    if ((half_u(rt, self + 0xE2u) & 1u) == 0u || signed_of(word(rt, self + 0xD0u)) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t bounds = self + 0x20u;
    const std::uint32_t origin = self + 0xB0u;
    put_float(rt, frame.at(0x40u), float_at(rt, bounds));
    put_float(rt, frame.at(0x44u), float_at(rt, bounds + 4u));
    put_float(rt, frame.at(0x48u), float_at(rt, bounds + 8u));
    put_float(rt, frame.at(0x4Cu), 1.0f);
    call_set(rt, ctx, 0x08C46364u, {origin, frame.at(0x40u)});
    if (float_at(rt, word(rt, self + 0xD8u) + 0x20u) < ctx.fpr[0]) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t source = call_set(rt, ctx, 0x08BAC230u, {self});
    put(rt, frame.at(0x18u), source);
    call_set(rt, ctx, 0x08C45F58u, {frame.at(0x18u), origin});
    const std::uint32_t record = ctx.gpr[2];
    const std::uint32_t extra = ctx.gpr[3];
    const std::uint32_t plane_vector = frame.at(0x50u);
    const std::uint32_t plane = frame.at(0x60u);
    put(rt, frame.at(0x74u), extra);
    put(rt, frame.at(0x30u), extra);
    put(rt, frame.at(0x70u), record);
    put(rt, frame.at(0x2Cu), record);
    const std::uint32_t normal = word(rt, record);
    put_float(rt, plane, -float_at(rt, normal));
    put_float(rt, plane + 4u, -float_at(rt, normal + 4u));
    put_float(rt, plane + 8u, -float_at(rt, normal + 8u));
    put_float(rt, plane + 0xCu, float_at(rt, normal + 0xCu));
    call_set(rt, ctx, 0x08C4624Cu, {plane_vector, plane, extra});
    put_float(rt, plane, float_at(rt, self + 0xB0u));
    put_float(rt, plane + 4u, float_at(rt, origin + 4u));
    put_float(rt, plane + 8u, float_at(rt, origin + 8u));
    float offset = float_at(rt, frame.at(0x5Cu));
    put(rt, frame.at(0x14u), origin);
    call_set(rt, ctx, 0x08BBBEC4u, {plane, bounds});
    offset = offset + ctx.fpr[0];
    std::uint32_t owner = word(rt, self + 0xD8u);
    put(rt, frame.at(0x1Cu), 0u);
    put_float(rt, frame.at(0x5Cu), offset);
    const std::uint32_t cursor = frame.at(0x20u);
    if (signed_of(half_u(rt, owner + 0x7Eu)) > 0) {
        std::uint32_t group_index = 0u;
        std::uint32_t group_offset = 0x80u;
        do {
            const std::uint32_t group = owner + group_offset;
            const std::uint32_t end = group + (((word(rt, group + 0x1FCu) >> 8) & 0xFu) << 4);
            put(rt, cursor, group);
            if (end != group) {
                std::uint32_t item_index = 0u;
                do {
                    const std::uint32_t item = call_set(rt, ctx, 0x08C45D84u, {cursor});
                    call_set(rt, ctx, 0x08C46364u, {item, plane_vector});
                    const float distance = ctx.fpr[0];
                    if (distance < 0.0f) {
                        const std::uint32_t entry = call_set(rt, ctx, 0x08C45D7Cu, {cursor});
                        const std::uint32_t pair = call_set(rt, ctx, 0x08BAE278u, {self, entry});
                        const std::uint32_t pair_extra = ctx.gpr[3];
                        put(rt, frame.at(0x74u), pair_extra);
                        put(rt, frame.at(0x28u), pair_extra);
                        put(rt, frame.at(0x70u), pair);
                        put(rt, frame.at(0x24u), pair);
                        call_set(rt, ctx, 0x08BAC284u, {plane, pair, pair_extra});
                        put_float(rt, frame.at(0x34u), float_at(rt, origin));
                        put_float(rt, frame.at(0x38u), float_at(rt, origin + 4u));
                        put_float(rt, frame.at(0x3Cu), float_at(rt, origin + 8u));
                        put(rt, frame.at(0xCu), frame.at(0x34u));
                        put(rt, frame.at(0x10u), origin);
                        put(rt, frame.at(0u), 0u);
                        put(rt, frame.at(4u), 0u);
                        put(rt, frame.at(8u), plane);
                        const std::uint32_t object = call_set(rt, ctx, 0x08B01D1Cu,
                                                              {first_argument, frame.at(0x1Cu), 0u, second_argument, group_index, item_index, plane, 0xFFFFFFFFu});
                        if (object == 0u) {
                            ctx.gpr[2] = word(rt, frame.at(0x1Cu)) != 0u ? 1u : 0u;
                            return;
                        }
                        put_float(rt, object + 0x30u, -distance);
                        const std::uint32_t body = word(rt, self + 0xD8u);
                        const float low = psp_mul(float_at(rt, body + 0x68u) + float_at(rt, self + 0xC4u), 0.5f);
                        const float high = psp_mul(float_at(rt, body + 0x64u) + float_at(rt, self + 0xC0u), 0.5f);
                        put_float(rt, object + 0x24u, low);
                        put_float(rt, object + 0x28u, high);
                        const std::uint32_t body_again = word(rt, self + 0xD8u);
                        put_float(rt, object + 0x2Cu, psp_mul(float_at(rt, body_again + 0x6Cu), float_at(rt, self + 0xC8u)));
                    }
                    ++item_index;
                    put(rt, cursor, word(rt, cursor) + 0x10u);
                } while (word(rt, cursor) != end);
            }
            owner = word(rt, self + 0xD8u);
            ++group_index;
            group_offset += 0x200u;
        } while (signed_of(group_index) < signed_of(half_u(rt, owner + 0x7Eu)));
    }
    const std::uint32_t out = word(rt, frame.at(0x1Cu));
    if (out != 0u) {
        put16(rt, out, half_u(rt, out) | 1u);
        call_set(rt, ctx, 0x08B012D8u, {word(rt, frame.at(0x1Cu)), first_argument});
    }
    ctx.gpr[2] = word(rt, frame.at(0x1Cu)) != 0u ? 1u : 0u;
}
// func_08998838(self): func_08A02218(self); the object self->+0x154 (when set) is released (func_08A5B930) and cleared with
// self->+0x130; func_08A76AA0(slot) / func_08A69B3C(name, 0x08CA4408) / func_08A7BFD0(self, slot, name) fill a frame value
// and self->+0x130 takes the one func_08A01338 makes from it, its flag word +0xE0 getting bit 0x02000000. The two vector
// triples at self->+0x158 and +0x164 are emptied (func_08BF0F4C on the range, func_08A5BC54 on the block) and self->+0x170 /
// +0x174 cleared; self->+0x12C bit 2 is cleared and bits 3-4 set to 2, self->+0x178 = 1.0, +0x17C = 0, and func_08A7BFD0(self,
// self + 0x180, name 0x08CA4410) runs. The chain self->+0x90, +0x54, ... is searched for an entry func_08A76F20 accepts whose
// name string (+0x78) equals the string 0x08CA4420; its holder goes through func_089D8A68 to self->+0x184. Last, the object
// self->+0x184 gets virtual slot 0x198, self->+0x188 = its float +0x9C and slot 0x1B8.
void func_08998838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    constexpr std::uint32_t kEmptyString = 0x08D74FD0u;
    constexpr std::uint32_t kStringFree = 0x08B8542Cu;
    call_set(rt, ctx, 0x08A02218u, {self});
    const std::uint32_t slot = frame.at(0x1Cu);
    const std::uint32_t name = frame.at(0x18u);
    if (word(rt, self + 0x154u) != 0u) call_set(rt, ctx, 0x08A5B930u, {word(rt, self + 0x154u)});
    put(rt, self + 0x154u, 0u);
    put(rt, self + 0x130u, 0u);
    call_set(rt, ctx, 0x08A76AA0u, {slot});
    call_set(rt, ctx, 0x08A69B3Cu, {name, 0x08CA4408u});
    call_set(rt, ctx, 0x08A7BFD0u, {self, slot, name});
    call_set(rt, ctx, 0x08A69D68u, {name});
    if (word(rt, slot) != 0u) {
        const std::uint32_t made = frame.at(0x14u);
        call_set(rt, ctx, 0x08A01338u, {made, slot});
        put(rt, self + 0x130u, word(rt, made));
        call_set(rt, ctx, kDestroy, {made});
    }
    const std::uint32_t flagged = word(rt, self + 0x130u);
    if (flagged != 0u) put(rt, flagged + 0xE0u, word(rt, flagged + 0xE0u) | 0x02000000u);
    const std::uint32_t triple = frame.at(0x28u);
    std::uint32_t source = self + 0x150u;
    for (std::uint32_t index = 0u; index < 2u; ++index) {
        const std::uint32_t begin = word(rt, source + 8u);
        const std::uint32_t end = word(rt, source + 0xCu);
        const std::uint32_t capacity = word(rt, source + 0x10u);
        put(rt, triple, begin);
        put(rt, triple + 4u, end);
        put(rt, source + 8u, 0u);
        put(rt, source + 0xCu, 0u);
        put(rt, source + 0x10u, 0u);
        source += 0xCu;
        put(rt, frame.at(0x30u), capacity);
        call_set(rt, ctx, 0x08BF0F4Cu, {begin, end, frame.at(2u)});
        const std::uint32_t block = word(rt, triple);
        const std::uint32_t bytes = static_cast<std::uint32_t>(signed_of(word(rt, triple + 8u) - block) >> 2) << 2;
        if (block != 0u) call_set(rt, ctx, 0x08A5BC54u, {block, bytes, 4u});
        put(rt, self + 0x170u + index * 4u, 0u);
    }
    const std::uint32_t mode = word(rt, self + 0x12Cu) & ~4u;
    put_float(rt, self + 0x178u, 1.0f);
    put(rt, self + 0x12Cu, (mode & ~0x18u) | 0x10u);
    put(rt, self + 0x17Cu, 0u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x10u), 0x08CA4410u});
    call_set(rt, ctx, 0x08A7BFD0u, {self, self + 0x180u, frame.at(0x10u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
    const std::uint32_t slot_target = self + 0x184u;
    std::uint32_t entry = word(rt, self + 0x90u);
    while (entry != 0u) {
        const std::uint32_t kind = call_set(rt, ctx, 0x089D8374u, {});
        if (call_set(rt, ctx, 0x08A76F20u, {entry, kind}) != 0u) {
            call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CA4420u, frame.at(1u)});
            const std::uint32_t text = word(rt, frame.at(4u));
            const std::uint32_t entry_length = word(rt, word(rt, entry + 0x78u) - 0xCu);
            const std::uint32_t text_length = word(rt, text - 0xCu);
            put(rt, frame.at(0x20u), entry_length);
            put(rt, frame.at(0x24u), text_length);
            std::uint32_t difference = entry_length - text_length;
            const std::uint32_t common = text_length < entry_length ? text_length : entry_length;
            const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {word(rt, entry + 0x78u), text, common});
            if (order != 0u) difference = order;
            release_string(rt, ctx, frame.at(4u), kEmptyString, kStringFree, frame.at(0u));
            if (difference == 0u) {
                put(rt, frame.at(8u), entry);
                call_set(rt, ctx, 0x089D8A68u, {frame.at(0xCu), frame.at(8u)});
                keep_temporary(rt, ctx, slot_target, frame.at(0xCu));
                break;
            }
        }
        entry = word(rt, entry + 0x54u);
    }
    const std::uint32_t object = word(rt, slot_target);
    if (object != 0u) {
        call_virtual_at(rt, ctx, object, 0x198u, {object});
        const std::uint32_t holder = word(rt, slot_target);
        checked(rt, ctx, holder, 0x08CA43F8u);
        const std::uint32_t again = holder == 0u ? word(rt, slot_target) : holder;
        put_float(rt, self + 0x188u, float_at(rt, holder + 0x9Cu));
        checked(rt, ctx, again, 0x08CA43F8u);
        call_virtual_at(rt, ctx, again, 0x1B8u, {again});
    }
    call_set(rt, ctx, kDestroy, {slot});
}
// func_08C4C0A0(vector, position, count, value): the libstdc++ vector<112-byte element>::_M_fill_insert (nothing for
// count 0). With room, the value is copied to a frame temporary; when count is below the elements after `position` the last
// `count` elements are constructed past the end (func_08C4AD04), the middle is copied backwards one element at a time and
// func_08C4B078 fills the gap; else the surplus copies of the value are constructed at the end (func_08C4AE48), the tail
// moved there (func_08C4B0D0) and the old range filled. Full: a block of size + max(size, count) elements (func_08B35DA0;
// func_08B5ADDC(0x08CB4AFC) beyond 0x2492492) receives the prefix (func_08C4B3C0), count copies (func_08C4B35C) and the
// tail, the old range is destroyed (func_08C498B4) and freed (func_08B35254) and begin / end / capacity updated.
void func_08C4C0A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0xC0u);
    put(rt, frame.at(0x80u), count);
    if (count == 0u) return;
    constexpr std::uint32_t kElement = 0x70u;
    constexpr std::uint32_t kInverseSeven = 0xB6DB6DB7u;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t spare = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 4) * kInverseSeven;
    if (!(spare < count)) {
        const std::uint32_t temporary = frame.at(0xCu);
        copy_words(rt, temporary, value, 28u);
        const std::uint32_t after_units = static_cast<std::uint32_t>(signed_of(end - position) >> 4);
        const std::uint32_t after = after_units * kInverseSeven;
        if (count < after) {
            call_set(rt, ctx, 0x08C4AD04u, {end - count * kElement, end, end, frame.at(6u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + count * kElement;
            const std::uint32_t source_end = end - count * kElement;
            const std::uint32_t moved = static_cast<std::uint32_t>(signed_of(source_end - position) >> 4) * kInverseSeven;
            put(rt, vector + 4u, new_end);
            if (signed_of(moved) > 0) {
                std::uint32_t from = source_end;
                std::uint32_t to = end;
                for (std::uint32_t index = 0u; index != moved; ++index) {
                    from -= kElement;
                    to -= kElement;
                    copy_words(rt, to, from, 28u);
                }
            }
            call_set(rt, ctx, 0x08C4B078u, {position, position + count * kElement, temporary});
        } else {
            call_set(rt, ctx, 0x08C4AE48u, {end, count - after, temporary, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * kElement;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08C4B0D0u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after_units << 4));
            call_set(rt, ctx, 0x08C4B078u, {position, end, temporary});
        }
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4) * kInverseSeven;
    put(rt, frame.at(8u), size);
    if (0x2492492u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4AFCu});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t length = size + grow;
    std::uint32_t bytes;
    std::uint32_t block = 0u;
    std::uint32_t front = begin;
    if (length < size) {
        bytes = 0xFFFFFFE0u;
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        front = word(rt, vector);
    } else if (length == 0u) {
        bytes = 0u;
    } else {
        bytes = (length << 7) - (length << 4);
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        front = word(rt, vector);
    }
    const std::uint32_t prefix_end = call_set(rt, ctx, 0x08C4B3C0u, {front, position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08C4B35Cu, {prefix_end, count, value, frame.at(2u)});
    const std::uint32_t new_end = call_set(rt, ctx, 0x08C4B3C0u, {position, word(rt, vector + 4u), (count << 7) - (count << 4) + prefix_end, frame.at(1u)});
    call_set(rt, ctx, 0x08C498B4u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old = word(rt, vector);
    ctx.gpr[2] = block + bytes;
    if (old != 0u) {
        call_set(rt, ctx, 0x08B35254u, {old});
        ctx.gpr[2] = block + bytes;
    }
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_end);
    put(rt, vector, block);
}
// func_08AEE144(owner, count): registers two tables with func_08AD8960(owner, name, 0x08D41FB0 / 0x08D41FB4, 16 / 0x4000), sets
// the state block at 0x08D41AD0 (+0x4E8 = 1, the handle words +0x4EC..+0x4FC / +0x514 / +0x518 = -1, +0x504 = count, the
// rest 0) and allocates func_08AD84B4(count << 5, 1) into +0x500. When that is non-zero it creates four kernel objects
// (func_08C7A998 x3 with counts count + 1 / count / 1, func_08C7AA30), then func_08C7AA18 (+0x4EC) and func_08C7A9D0 starts
// it: success sets +0x4DC = 1 and returns 1. Any failure logs (func_08AD83B8 when the flag word 0x08CDFB3C has bit 0) and
// unwinds: each handle >= 0 is deleted (func_08C7AAA0 / func_08C7A9C8 / func_08C7A9E0 x3) and set to -1, the block freed
// (func_08AD85C8) and +0x500 cleared; returns 0.
void func_08AEE144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    constexpr std::uint32_t kTables = 0x08D41FB0u;
    constexpr std::uint32_t kState = 0x08D41AD0u;
    constexpr std::uint32_t kLogFlag = 0x08CDFB3Cu;
    constexpr std::uint32_t kLogger = 0x08CDFB40u;
    call_set(rt, ctx, 0x08AD8960u, {owner, 0x08C92378u, kTables, 0x10u});
    call_set(rt, ctx, 0x08AD8960u, {owner, 0x08C9238Cu, kTables + 4u, 0x4000u});
    const std::uint32_t extra = kTables + 0x34u;
    put(rt, extra + 0x1Cu, 0u);
    put(rt, kState + 0x518u, 0xFFFFFFFFu);
    put(rt, kState + 0x4E8u, 1u);
    for (const std::uint32_t offset : {0x4ECu, 0x4F0u, 0x4F4u, 0x4F8u, 0x4FCu}) put(rt, kState + offset, 0xFFFFFFFFu);
    put(rt, kState + 0x500u, 0u);
    put(rt, kState + 0x504u, count);
    put(rt, kState + 0x508u, 0u);
    put(rt, kState + 0x50Cu, 0u);
    put(rt, kState + 0x510u, 0u);
    for (const std::uint32_t offset : {8u, 0xCu, 0x10u, 0x14u, 0x18u}) put(rt, extra + offset, 0u);
    put(rt, kState + 0x514u, 0xFFFFFFFFu);
    const std::uint32_t block = call_set(rt, ctx, 0x08AD84B4u, {count << 5, 1u});
    put(rt, kState + 0x500u, block);
    const auto log = [&](std::uint32_t line, std::uint32_t message) {
        if ((word(rt, kLogFlag) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, kLogger), 0x08C91F50u, line, message});
    };
    bool success = false;
    if (block == 0u) {
        if ((word(rt, kLogFlag) & 1u) != 0u)
            call_set(rt, ctx, 0x08AD83B8u, {word(rt, kLogger), 0x08C91F50u, 0xB2Cu, 0x08C923A0u, word(rt, kState + 0x504u)});
    } else {
        do {
            const std::uint32_t slots = word(rt, kState + 0x504u);
            std::uint32_t handle = call_set(rt, ctx, 0x08C7A998u, {0x08C923CCu, 0u, 0u, slots + 1u, 0u});
            put(rt, kState + 0x4F0u, handle);
            if (signed_of(handle) < 0) {
                log(0xB37u, 0x08C92490u);
                break;
            }
            const std::uint32_t limit = word(rt, kState + 0x504u);
            handle = call_set(rt, ctx, 0x08C7A998u, {0x08C923E4u, 0u, limit, limit, 0u});
            put(rt, kState + 0x4F4u, handle);
            if (signed_of(handle) < 0) {
                log(0xB37u, 0x08C92490u);
                break;
            }
            handle = call_set(rt, ctx, 0x08C7A998u, {0x08C923FCu, 0u, 1u, 1u, 0u});
            put(rt, kState + 0x4F8u, handle);
            if (signed_of(handle) < 0) {
                log(0xB37u, 0x08C92490u);
                break;
            }
            handle = call_set(rt, ctx, 0x08C7AA30u, {0x08C92414u, 0x200u, 0u, 0u});
            put(rt, kState + 0x4FCu, handle);
            if (signed_of(handle) < 0) {
                log(0xB37u, 0x08C92490u);
                break;
            }
            const std::uint32_t stack_size = word(rt, kState + 0x4E0u);
            const std::uint32_t priority = word(rt, kState + 0x4E4u);
            handle = call_set(rt, ctx, 0x08C7AA18u, {0x08C9242Cu, 0x08AEEB08u, stack_size, priority, 0u, 0u});
            put(rt, kState + 0x4ECu, handle);
            if (signed_of(handle) < 0) {
                log(0xB3Fu, 0x08C92468u);
                break;
            }
            if (signed_of(call_set(rt, ctx, 0x08C7A9D0u, {handle, 0u, 0u})) >= 0) {
                ctx.gpr[2] = 1u;
                put(rt, kState + 0x4DCu, 1u);
                success = true;
                break;
            }
            log(0xB45u, 0x08C92440u);
        } while (false);
    }
    if (success) return;
    const struct {
        std::uint32_t offset;
        std::uint32_t function;
    } handles[5] = {{0x4ECu, 0x08C7AAA0u}, {0x4FCu, 0x08C7A9C8u}, {0x4F8u, 0x08C7A9E0u}, {0x4F4u, 0x08C7A9E0u}, {0x4F0u, 0x08C7A9E0u}};
    for (const auto &entry : handles) {
        const std::uint32_t handle = word(rt, kState + entry.offset);
        if (signed_of(handle) >= 0) {
            call_set(rt, ctx, entry.function, {handle});
            put(rt, kState + entry.offset, 0xFFFFFFFFu);
        }
    }
    const std::uint32_t pool = word(rt, kState + 0x500u);
    if (pool != 0u) {
        call_set(rt, ctx, 0x08AD85C8u, {pool});
        put(rt, kState + 0x500u, 0u);
    }
    ctx.gpr[2] = 0u;
}
// func_08C0E874(vector, position, count, source): the libstdc++ vector<12-byte element>::_M_range_insert for a source range
// object {begin +0, end +4} (nothing for count 0). With room, the range is first copied (func_08B35840 block of
// (end - begin) & ~31 bytes, func_08C0D7F8) into a frame temporary vector; when count is below the elements after
// `position` the last `count` are constructed past the end (func_08C0E054), the middle assigned backwards (func_08C0E654)
// and the gap filled (func_08C0E814 from the temporary); else the surplus is constructed at the end (func_08C0E30C), the
// tail moved (func_08C0E258) and the old range filled. The temporary is destroyed (func_08BE5888) and freed. Full: a
// block of size + max(size, count) elements (func_08A5BC20; func_08B5ADDC(0x08CA8644) beyond 0x15555555) takes the
// prefix (func_08C0E0F8), the source (func_08C0E1AC) and the tail; the old range is destroyed (func_08C0DC90) and freed.
void func_08C0E874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    constexpr std::uint32_t kElement = 12u;
    constexpr std::uint32_t kInverseThree = 0xAAAAAAABu;
    const std::uint32_t temporary = frame.at(0x10u);
    const std::uint32_t spare = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - word(rt, vector + 4u)) >> 2) * kInverseThree;
    if (!(spare < count)) {
        const std::uint32_t bytes = (word(rt, source + 4u) - word(rt, source)) & ~0x1Fu;
        put(rt, temporary, 0u);
        put(rt, temporary + 4u, 0u);
        put(rt, temporary + 8u, 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08B35840u, {0x10u, bytes});
        put(rt, temporary + 8u, block + bytes);
        put(rt, temporary, block);
        put(rt, temporary + 4u, block);
        const std::uint32_t copy_end = call_set(rt, ctx, 0x08C0D7F8u, {word(rt, source), word(rt, source + 4u), block, frame.at(1u)});
        const std::uint32_t end = word(rt, vector + 4u);
        put(rt, temporary + 4u, copy_end);
        const std::uint32_t after_words = static_cast<std::uint32_t>(signed_of(end - position) >> 2);
        const std::uint32_t after = after_words * kInverseThree;
        if (word(rt, frame.at(0x20u)) < after) {
            const std::uint32_t n = word(rt, frame.at(0x20u));
            call_set(rt, ctx, 0x08C0E054u, {end - n * kElement, end, end, frame.at(8u)});
            const std::uint32_t reload = word(rt, frame.at(0x20u));
            const std::uint32_t shift = reload * kElement;
            const std::uint32_t middle_end = end - shift;
            const std::uint32_t moved = static_cast<std::uint32_t>(signed_of(middle_end - position) >> 2) * kInverseThree;
            put(rt, vector + 4u, word(rt, vector + 4u) + shift);
            if (signed_of(moved) > 0) {
                std::uint32_t to = end;
                std::uint32_t from = middle_end;
                for (std::uint32_t index = 0u; index != moved; ++index) {
                    to -= kElement;
                    from -= kElement;
                    if (to != from) call_set(rt, ctx, 0x08C0E654u, {to, from});
                }
            }
            const std::uint32_t fill = word(rt, frame.at(0x20u)) * kElement;
            call_set(rt, ctx, 0x08C0E814u, {position, position + fill, temporary});
        } else {
            const std::uint32_t n = word(rt, frame.at(0x20u));
            call_set(rt, ctx, 0x08C0E30Cu, {end, n - after, temporary, frame.at(7u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (n - after) * kElement;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08C0E258u, {position, end, new_end, frame.at(6u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after_words << 2));
            call_set(rt, ctx, 0x08C0E814u, {position, end, temporary});
        }
        call_set(rt, ctx, 0x08BE5888u, {word(rt, temporary), word(rt, temporary + 4u), frame.at(0u)});
        const std::uint32_t old = word(rt, temporary);
        if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - begin) >> 2) * kInverseThree;
    put(rt, frame.at(0xCu), size);
    if (0x15555555u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA8644u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t length = size + grow;
    const std::uint32_t bytes = length < size ? 0xFFFFFFFCu : (length << 4) - (length << 2);
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t prefix_end = call_set(rt, ctx, 0x08C0E0F8u, {word(rt, vector), position, block, frame.at(5u)});
    call_set(rt, ctx, 0x08C0E1ACu, {prefix_end, word(rt, frame.at(0x20u)), source, frame.at(4u)});
    const std::uint32_t tail_target = ((word(rt, frame.at(0x20u)) << 4) - (word(rt, frame.at(0x20u)) << 2)) + prefix_end;
    const std::uint32_t new_end = call_set(rt, ctx, 0x08C0E0F8u, {position, word(rt, vector + 4u), tail_target, frame.at(3u)});
    call_set(rt, ctx, 0x08C0DC90u, {word(rt, vector), word(rt, vector + 4u), frame.at(2u)});
    const std::uint32_t old = word(rt, vector);
    if (old != 0u) call_set(rt, ctx, 0x08A5BC54u, {old, static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old) >> 2) << 2, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_end);
    put(rt, vector, block);
}
// func_08BB1FF0(holder): the recursive release of a chain of use-counted nodes (word +0 the next node, +0xC a payload block,
// +0x10 the use count). A null node or one whose count stays above 0 ends it (v0 = that count). On reaching 0 the
// payload goes back to the size-class 4 pool, the payload word is cleared, and the same is done for the next node and the
// one after it; for the third, func_08BB1FF0 recurses on it first. The nodes themselves then go back to the size-class
// 0x14 pool (the third, the second, the first), each through func_08BB1F48's table (see return_to_class).
void func_08BB1FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t key_slot = frame.at(0u);
    const std::uint32_t node = word(rt, holder);
    if (node == 0u) return;
    const std::uint32_t uses = word(rt, node + 0x10u) - 1u;
    put(rt, node + 0x10u, uses);
    if (uses != 0u) {
        ctx.gpr[2] = uses;
        return;
    }
    return_to_class(rt, ctx, 4u, word(rt, node + 0xCu), key_slot);
    const std::uint32_t second = word(rt, node);
    put(rt, node + 0xCu, 0u);
    if (second != 0u) {
        const std::uint32_t second_uses = word(rt, second + 0x10u) - 1u;
        put(rt, second + 0x10u, second_uses);
        if (second_uses == 0u) {
            return_to_class(rt, ctx, 4u, word(rt, second + 0xCu), key_slot);
            const std::uint32_t third = word(rt, second);
            put(rt, second + 0xCu, 0u);
            if (third != 0u) {
                const std::uint32_t third_uses = word(rt, third + 0x10u) - 1u;
                put(rt, third + 0x10u, third_uses);
                if (third_uses == 0u) {
                    return_to_class(rt, ctx, 4u, word(rt, third + 0xCu), key_slot);
                    put(rt, third + 0xCu, 0u);
                    call_set(rt, ctx, 0x08BB1FF0u, {third});
                    return_to_class(rt, ctx, 0x14u, third, key_slot);
                }
            }
            return_to_class(rt, ctx, 0x14u, second, key_slot);
        }
    }
    return_to_class(rt, ctx, 0x14u, node, key_slot);
}
// func_08AD90AC(owner): one-time set-up of the state block at 0x08D405D0 (word +0 = ready). When it is already set the
// flag word 0x08CDFB3C (bit 0) logs through func_08AD83B8 and 0 returns. Else (bit 2 of the flag logs too) the words +4..+0x28
// are cleared, two resources func_08AEB850(0x08C908FC / 0x08C9090C) go to +0x24 / +0x28 and func_08AEB954() to +8
// (+0x20 = 0x2000, +0x14 = -1); two tables are registered (func_08AD8960(owner, name, +0xC / +0x1C, 0 / 32)), a block
// of (+0x1C << 5) + 32 bytes (func_08AD84B4) is split into two 16-byte records at +0x18, and seven set-up functions run on
// owner (the first to return 0 fails). Success sets +0 = 1, logs per flag bit 2 and returns 1. Any failure logs per bit 0,
// runs the teardown (func_08AEA0A4, six no-argument functions, func_08AEB8C8 on +0x28 and +0x24, func_08AD85C8 on +0x18)
// and returns 0.
void func_08AD90AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    constexpr std::uint32_t kState = 0x08D405D0u;
    constexpr std::uint32_t kLogFlag = 0x08CDFB3Cu;
    constexpr std::uint32_t kLogger = 0x08CDFB40u;
    const auto log = [&](std::uint32_t line, std::uint32_t message) {
        if ((word(rt, kLogFlag) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, kLogger), 0x08C9085Cu, line, message});
    };
    if (word(rt, kState) != 0u) {
        log(0x266u, 0x08C908BCu);
        ctx.gpr[2] = 0u;
        return;
    }
    if ((word(rt, kLogFlag) & 4u) != 0u) call_set(rt, ctx, 0x08AD8464u, {0x08C908D8u});
    put(rt, kState + 4u, 0u);
    for (std::uint32_t offset = 8u; offset <= 0x28u; offset += 4u) put(rt, kState + offset, 0u);
    put(rt, kState, 0u);
    bool failed_resources = true;
    const std::uint32_t first = call_set(rt, ctx, 0x08AEB850u, {0x08C908FCu});
    put(rt, kState + 0x24u, first);
    if (first != 0u) {
        const std::uint32_t second = call_set(rt, ctx, 0x08AEB850u, {0x08C9090Cu});
        put(rt, kState + 0x28u, second);
        failed_resources = second == 0u;
    }
    if (failed_resources) log(0x273u, 0x08C9091Cu);
    if (!failed_resources) {
        put(rt, kState + 8u, call_set(rt, ctx, 0x08AEB954u, {}));
        put(rt, kState + 0x20u, 0x2000u);
        put(rt, kState + 0x14u, 0xFFFFFFFFu);
        call_set(rt, ctx, 0x08AD8960u, {owner, 0x08C90934u, kState + 0xCu, 0u});
        call_set(rt, ctx, 0x08AD8960u, {owner, 0x08C90944u, kState + 0x1Cu, 0x20u});
        const std::uint32_t block = call_set(rt, ctx, 0x08AD84B4u, {(word(rt, kState + 0x1Cu) << 5) + 0x20u, 1u});
        if (block == 0u) {
            log(0x284u, 0x08C9098Cu);
        } else {
            put(rt, kState + 0x18u, block);
            std::uint32_t pointer = block + 0x20u;
            for (std::uint32_t index = 0u; index < 2u; ++index) {
                const std::uint32_t record = word(rt, kState + 0x18u) + (index << 4);
                put(rt, record, pointer);
                put(rt, record + 0xCu, 0u);
                pointer += word(rt, kState + 0x1Cu) << 4;
                put(rt, record + 4u, 0u);
                put(rt, record + 8u, 0u);
            }
            bool all_ready = true;
            for (const std::uint32_t function : {0x08AEAD38u, 0x08AD9F10u, 0x08ADB3E8u, 0x08AE6620u, 0x08AE2D38u, 0x08ADDE30u, 0x08AE9F98u}) {
                if (call_set(rt, ctx, function, {owner}) == 0u) {
                    all_ready = false;
                    break;
                }
            }
            if (all_ready) {
                call_set(rt, ctx, 0x08AD97D8u, {1u});
                std::uint32_t flag = word(rt, kLogFlag);
                put(rt, kState, 1u);
                if ((flag & 4u) != 0u) {
                    call_set(rt, ctx, 0x08AD8464u, {0x08C90978u, word(rt, kState + 0x10u)});
                    flag = word(rt, kLogFlag);
                }
                if ((flag & 4u) != 0u) call_set(rt, ctx, 0x08AD8464u, {0x08C90954u});
                ctx.gpr[2] = 1u;
                return;
            }
        }
    }
    call_set(rt, ctx, 0x08AEA0A4u, {});
    for (const std::uint32_t function : {0x08ADDF3Cu, 0x08AE2EB4u, 0x08AE687Cu, 0x08ADB4BCu, 0x08AD9FF0u, 0x08AEB3BCu}) call_set(rt, ctx, function, {});
    const std::uint32_t second_resource = word(rt, kState + 0x28u);
    if (second_resource != 0u) call_set(rt, ctx, 0x08AEB8C8u, {second_resource});
    const std::uint32_t first_resource = word(rt, kState + 0x24u);
    if (first_resource != 0u) call_set(rt, ctx, 0x08AEB8C8u, {first_resource});
    const std::uint32_t pool = word(rt, kState + 0x18u);
    if (pool != 0u) call_set(rt, ctx, 0x08AD85C8u, {pool});
    ctx.gpr[2] = 0u;
}
// func_08C227FC(a0, key): a value object (func_08A76ABC(tmp, func_08A6158C(key)), checked) is turned into a string by its virtual
// slot 0xC; every '\\' in that string becomes '/' (each access first making the shared string unique with func_08B88FF8
// when its count is >= 0). Then func_08A7C0F8(func_08A61624(key), string): when it answers non-zero that is all.
// Otherwise a message string is assembled: func_08A9DD14 / func_08A9BCB0 / func_08A7C0F0 / func_08A98A70 / func_08A7BFD0 build
// a list object, func_08BEED1C appends the literal 0x08CB0370 (19 bytes), the path string (func_08B89054), the literal
// 0x08CB0384 (26) and, when the list holds elements, 0x08CB03A0 (4) and for each element the literal 0x08CB03A8 (1)
// followed by that element's virtual slot 0xC string (appended with func_08B35F60 as its length). Strings and value
// objects are released/destroyed on the way out.
void func_08C227FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    constexpr std::uint32_t kEmptyString = 0x08D74FD0u;
    constexpr std::uint32_t kStringFree = 0x08B8542Cu;
    const std::uint32_t path = frame.at(0x10u);
    const std::uint32_t lookup = call_set(rt, ctx, 0x08A6158Cu, {key});
    call_set(rt, ctx, 0x08A76ABCu, {frame.at(0xCu), lookup});
    const std::uint32_t source = checked(rt, ctx, word(rt, frame.at(0xCu)), 0x08CB0284u);
    call_virtual_v1(rt, ctx, source, 0xCu, {path, source});
    if (word(rt, word(rt, path) - 0xCu) != 0u) {
        std::uint32_t index = 0u;
        for (;;) {
            std::uint32_t text = word(rt, path);
            if (!(signed_of(word(rt, text - 4u)) < 0)) {
                call_set(rt, ctx, 0x08B88FF8u, {path});
                text = word(rt, path);
            }
            std::uint32_t length;
            if (sext8(byte_u(rt, text + index)) == 0x5Cu) {
                if (!(signed_of(word(rt, text - 4u)) < 0)) {
                    call_set(rt, ctx, 0x08B88FF8u, {path});
                    text = word(rt, path);
                }
                put8(rt, text + index, 0x2Fu);
                length = word(rt, word(rt, path) - 0xCu);
            } else {
                length = word(rt, text - 0xCu);
            }
            ++index;
            if (!(index < length)) break;
        }
    }
    const std::uint32_t resolved = call_set(rt, ctx, 0x08A61624u, {key});
    if (call_set(rt, ctx, 0x08A7C0F8u, {resolved, path}) == 0u) {
        const std::uint32_t holder = frame.at(8u);
        call_set(rt, ctx, 0x08A9DD14u, {holder});
        const std::uint32_t holder_object = checked(rt, ctx, word(rt, holder), 0x08CB0368u);
        const std::uint32_t built = call_set(rt, ctx, 0x08A9BCB0u, {holder_object});
        call_set(rt, ctx, 0x08A7C0F0u, {frame.at(0x14u), built});
        call_set(rt, ctx, kDestroy, {holder});
        const std::uint32_t list_slot = frame.at(0x18u);
        call_set(rt, ctx, 0x08A98A70u, {list_slot});
        const std::uint32_t owner = checked(rt, ctx, word(rt, frame.at(0x14u)), 0x08CB02ACu);
        call_set(rt, ctx, 0x08A7BFD0u, {owner, list_slot, 0x08CF6FF0u});
        const std::uint32_t message = frame.at(0x1Cu);
        put(rt, message, kEmptyString + 0xCu);
        call_set(rt, ctx, 0x08BEED1Cu, {message, 0x08CB0370u, 0x13u});
        call_set(rt, ctx, 0x08B89054u, {message, path});
        call_set(rt, ctx, 0x08BEED1Cu, {message, 0x08CB0384u, 0x1Au});
        const std::uint32_t list = checked(rt, ctx, word(rt, list_slot), 0x08CB0294u);
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, list + 0x18u) - word(rt, list + 0x14u)) >> 2);
        if (signed_of(count) > 0) {
            call_set(rt, ctx, 0x08BEED1Cu, {message, 0x08CB03A0u, 4u});
            const std::uint32_t piece = frame.at(4u);
            for (std::uint32_t index = 0u; index != count;) {
                call_set(rt, ctx, 0x08BEED1Cu, {message, 0x08CB03A8u, 1u});
                const std::uint32_t element = checked(rt, ctx, word(rt, word(rt, word(rt, list_slot) + 0x14u) + index * 4u), 0x08CB0284u);
                ++index;
                call_virtual_at(rt, ctx, element, 0xCu, {piece, element});
                const std::uint32_t text = word(rt, piece);
                const std::uint32_t text_length = call_set(rt, ctx, 0x08B35F60u, {text});
                call_set(rt, ctx, 0x08BEED1Cu, {message, text, text_length});
                release_string(rt, ctx, piece, kEmptyString, kStringFree, frame.at(2u));
            }
        }
        release_string(rt, ctx, message, kEmptyString, kStringFree, frame.at(1u));
        call_set(rt, ctx, kDestroy, {list_slot});
        call_set(rt, ctx, kDestroy, {frame.at(0x14u)});
    }
    release_string(rt, ctx, path, kEmptyString, kStringFree, frame.at(0u));
    call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
}
// func_088175B8(self): the first call builds a table of 17 singletons at 0x08CE2A38 (guard byte 0x08CE2A30; each of
// func_0884D394, func_088586B4, ... func_08817918 in order, its result stored before the next call). Then every table entry up
// to the first zero follows func_08A7CC30(*self, entry) (the object *self checked each time), func_0883A4D8(tmp) /
// func_08A7CE50(*self, tmp) run once, a long list of 42 per-self set-up functions each take self, and two literal-based
// registrations go through func_08A85B1C(tmp, 4 / 2) and func_08B86E98(*self, 0x08C98C80 / 0x08C98C8C, tmp); last
// func_088213BC, func_0881FDEC and func_08820C98 on self.
void func_088175B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    constexpr std::uint32_t kGuard = 0x08CE2A30u;
    constexpr std::uint32_t kTable = 0x08CE2A38u;
    constexpr std::uint32_t kMessage = 0x08C98C78u;
    if (sext8(byte_u(rt, kGuard)) == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            static constexpr std::uint32_t kBuilders[17] = {0x0884D394u, 0x088586B4u, 0x08854634u, 0x0884C458u, 0x088292D0u, 0x0882462Cu, 0x088350A0u, 0x08846E0Cu, 0x08845754u, 0x088445F0u, 0x088433C0u, 0x08841738u, 0x08835934u, 0x088373F4u, 0x08838D64u, 0x0881A768u, 0x08817918u, };
            for (std::uint32_t index = 0u; index < 17u; ++index) put(rt, kTable + index * 4u, call_set(rt, ctx, kBuilders[index], {}));
            call_set(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    if (word(rt, kTable) != 0u) {
        std::uint32_t slot = kTable;
        for (;;) {
            const std::uint32_t object = checked(rt, ctx, word(rt, self), kMessage);
            call_set(rt, ctx, 0x08A7CC30u, {object, word(rt, slot)});
            const std::uint32_t next = word(rt, slot + 4u);
            slot += 4u;
            if (next == 0u) break;
        }
    }
    const std::uint32_t registry = checked(rt, ctx, word(rt, self), kMessage);
    call_set(rt, ctx, 0x0883A4D8u, {frame.at(8u)});
    call_set(rt, ctx, 0x08A7CE50u, {registry, frame.at(8u)});
    call_set(rt, ctx, kDestroy, {frame.at(8u)});
    static constexpr std::uint32_t kSetups[42] = {0x0882222Cu, 0x08822920u, 0x08823BBCu, 0x088541F0u, 0x08828D74u, 0x08834670u, 0x08834A8Cu, 0x0885B4CCu, 0x0885B5F8u, 0x0885B724u, 0x0885B850u, 0x0885B3A0u, 0x088592D0u, 0x088593FCu, 0x088591A4u, 0x0885A7E8u, 0x0885A914u, 0x0885AA40u, 0x0885AB6Cu, 0x0885AC98u, 0x0885ADC4u, 0x0885B01Cu, 0x0885B274u, 0x0885B148u, 0x0885A590u, 0x0885A464u, 0x0885A338u, 0x08859D5Cu, 0x08859E88u, 0x0885A20Cu, 0x08859FB4u, 0x0885A0E0u, 0x08859C30u, 0x088598ACu, 0x088599D8u, 0x08859B04u, 0x08859780u, 0x08859654u, 0x08859528u, 0x08859078u, 0x08858F4Cu, 0x08858E20u, };
    for (const std::uint32_t function : kSetups) call_set(rt, ctx, function, {self});
    const std::uint32_t first = checked(rt, ctx, word(rt, self), kMessage);
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(4u), 4u});
    call_set(rt, ctx, 0x08B86E98u, {first, 0x08C98C80u, frame.at(4u)});
    call_set(rt, ctx, kDestroy, {frame.at(4u)});
    const std::uint32_t second = checked(rt, ctx, word(rt, self), kMessage);
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), 2u});
    call_set(rt, ctx, 0x08B86E98u, {second, 0x08C98C8Cu, frame.at(0u)});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
    call_set(rt, ctx, 0x088213BCu, {self});
    call_set(rt, ctx, 0x0881FDECu, {self});
    call_set(rt, ctx, 0x08820C98u, {self});
}
// func_08AC9CC8(self, name, create): looks the name up (func_08AC99A4 over the pair func_08B1CE38(name) returns); a hit
// returns entry + 0x28. Without `create` (low byte) 0. Else a new entry (func_08AC9A68): for the name 0x08CB4948 an object
// per word of the zero-terminated list func_08C3CEDC(0x08D00120) is made (func_08B62344(20) + func_08AD5590, named by
// func_08B37640 formats, func_08AD5460 / func_08AD542C / func_08ADC988) into the vector at entry + 0x30, func_08AD36AC is told
// the size sum and func_08AC963C(entry, object->+0x10, text, size, index, running sum) records each; for any other name one
// object is made from the name (destroyed through its virtual slot 4 and the entry func_08AC9240 on failure), pushed on that
// vector and recorded. Success sets entry->+0x28 = self, entry->+0x2C = entry and returns entry + 0x28.
void func_08AC9CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t name = ctx.gpr[5];
    const std::uint32_t create = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x1040u);
    constexpr std::uint32_t kDefaultName = 0x08CB4948u;
    constexpr std::uint32_t kFormat = 0x08CB4950u;
    call_set(rt, ctx, 0x08B1CE38u, {name, name});
    const std::uint32_t pair_low = ctx.gpr[2];
    const std::uint32_t pair_high = ctx.gpr[3];
    const std::uint32_t found = call_set(rt, ctx, 0x08AC99A4u, {pair_low, pair_high});
    if (found != 0u) {
        ctx.gpr[2] = found + 0x28u;
        return;
    }
    if (create == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    std::uint32_t entry;
    const std::uint32_t text = frame.at(0x808u);
    if (call_set(rt, ctx, 0x08B35FD8u, {name, kDefaultName}) == 0u) {
        const std::uint32_t owner = call_set(rt, ctx, 0x08AD354Cu, {});
        const std::uint32_t label = call_set(rt, ctx, 0x08C1A0C4u, {0x08D00138u});
        call_set(rt, ctx, 0x08B37640u, {frame.at(8u), kFormat, owner, label});
        const std::uint32_t list = call_set(rt, ctx, 0x08C3CEDCu, {0x08D00120u});
        if (word(rt, list) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        std::uint32_t count = 0u;
        for (std::uint32_t cursor = list;; cursor += 4u) {
            ++count;
            if (word(rt, cursor + 4u) == 0u) break;
        }
        entry = call_set(rt, ctx, 0x08AC9A68u, {kDefaultName});
        const std::uint32_t objects = entry + 0x30u;
        put(rt, frame.at(4u), 0u);
        const std::uint32_t begin = word(rt, objects);
        const std::uint32_t end = word(rt, objects + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
        if (count < size) call_set(rt, ctx, 0x08C3D0C8u, {objects, begin + count * 4u, end});
        else call_set(rt, ctx, 0x08C3D298u, {objects, end, count - size, frame.at(4u)});
        std::uint32_t total = 0u;
        if (signed_of(count) > 0) {
            std::uint32_t cursor = list;
            for (std::uint32_t index = 0u; index != count; ++index) {
                const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x14u});
                call_set(rt, ctx, 0x08AD5590u, {block});
                put(rt, word(rt, objects) + index * 4u, block);
                call_set(rt, ctx, 0x08B37640u, {frame.at(0x408u), kFormat, frame.at(8u), word(rt, cursor)});
                cursor += 4u;
                if (call_set(rt, ctx, 0x08AD5460u, {block, frame.at(0x408u)}) != 0u) {
                    call_set(rt, ctx, 0x08AD542Cu, {block});
                    total += call_set(rt, ctx, 0x08ADC988u, {word(rt, block + 0x10u), 0u});
                }
            }
        }
        call_set(rt, ctx, 0x08AD36ACu, {entry, total});
        if (signed_of(count) > 0) {
            std::uint32_t running = 0u;
            for (std::uint32_t index = 0u; index != count; ++index) {
                const std::uint32_t object = word(rt, word(rt, objects) + index * 4u);
                const std::uint32_t resource = word(rt, object + 0x10u);
                const std::uint32_t resource_size = call_set(rt, ctx, 0x08ADC988u, {resource, text});
                call_set(rt, ctx, 0x08AC963Cu, {entry, resource, text, resource_size, index, running});
                running += resource_size;
            }
        }
    } else {
        entry = call_set(rt, ctx, 0x08AC9A68u, {name});
        const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x14u});
        call_set(rt, ctx, 0x08AD5590u, {block});
        if (call_set(rt, ctx, 0x08AD5460u, {block, name}) == 0u) {
            if (block != 0u) call_virtual_at(rt, ctx, block, 4u, {block});
            call_set(rt, ctx, 0x08AC9240u, {entry});
            ctx.gpr[2] = 0u;
            return;
        }
        call_set(rt, ctx, 0x08AD542Cu, {block});
        const std::uint32_t resource = word(rt, block + 0x10u);
        const std::uint32_t resource_size = call_set(rt, ctx, 0x08ADC988u, {resource, text});
        call_set(rt, ctx, 0x08AD36ACu, {entry, resource_size});
        put(rt, frame.at(0u), block);
        const std::uint32_t objects = entry + 0x30u;
        std::uint32_t end = word(rt, objects + 4u);
        if (end == word(rt, objects + 8u)) {
            call_set(rt, ctx, 0x08C3D12Cu, {objects, end, frame.at(0u)});
        } else {
            if (end != 0u) {
                put(rt, end, block);
                end = word(rt, objects + 4u);
            }
            put(rt, objects + 4u, end + 4u);
        }
        call_set(rt, ctx, 0x08AC963Cu, {entry, resource, text, resource_size, 0u, 0u});
    }
    if (entry == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, entry + 0x28u, self);
    put(rt, entry + 0x2Cu, entry);
    ctx.gpr[2] = entry + 0x28u;
}
// func_08A270AC(self): a name value (func_08A69B3C(tmp, 0x08CAB3D8)) and a 56-byte block (func_08A5BC20(0x38, 4)) go through
// func_08A7E914(block, tmp) and func_08A76AB4(self, &block) (the block pointer stored at frame +4 first); the name is destroyed.
// Then 16 handlers are registered on the checked object *self with func_08B86FB0(*self, name, handler): name literals
// 0x08CAB3F0.. paired with the code addresses 0x08A26E98.. in the table below. Returns self.
void func_08A270AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAB3D8u});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call_set(rt, ctx, 0x08A7E914u, {block, frame.at(0u)});
    put(rt, frame.at(4u), block);
    call_set(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    static constexpr std::uint32_t kHandlers[16][2] = {
        {0x08CAB3F0u, 0x08A26E98u}, {0x08CAB3F8u, 0x08A26EA0u}, {0x08CAB3FCu, 0x08A26EA8u}, {0x08CAB404u, 0x08A2741Cu},
        {0x08CAB410u, 0x08A27460u}, {0x08CAB420u, 0x08A27808u}, {0x08CAB42Cu, 0x08A274A8u}, {0x08CAB438u, 0x08A274F0u},
        {0x08CAB444u, 0x08A26EB0u}, {0x08CAB454u, 0x08A26F30u}, {0x08CAB460u, 0x08A27538u}, {0x08CAB470u, 0x08A26EC4u},
        {0x08CAB480u, 0x08A27610u}, {0x08CAB490u, 0x08A26E28u}, {0x08CAB4A0u, 0x08A276E8u}, {0x08CAB4B0u, 0x08A277C0u}};
    for (const auto &handler : kHandlers) {
        const std::uint32_t registry = checked(rt, ctx, word(rt, self), 0x08CAB3E8u);
        call_set(rt, ctx, 0x08B86FB0u, {registry, handler[0], handler[1]});
    }
    ctx.gpr[2] = self;
}
// func_0890B92C(self, a, object): a display-list set-up. func_08AF2C0C(*self, a, 0, a3), func_08AF2864(*self), func_08AFBEF4(), then a run of graphics
// state calls (func_08B58874 / func_08B54FB8 / func_08B57744 / func_08B57278 / func_08B58A74 / func_08B58AD0 / func_08B5877C /
// func_08B584D0 / func_08B587A8 / func_08B57750 / func_08B53844 / func_08B581BC with fixed constants) and func_08B581F4 with
// the three floats func_08AF1308(self->+0x20) fills. A frame record {0x08CD71A8, view, 0, scale y, scale x, ...} is built (the
// view is self->+0x44 when self->+0x5C is set; when func_08BDADF8(0x08CE9F74) holds and (float)(owner->+0xF0) x 1e-6 is
// below 0.0171 the flag byte 0x08CDD7F0 toggles and the scales are 0.5 / (float)(*a->+0x18 / +0x1C) or 0, the view
// becoming the record). `object`, when given and a view exists, gets virtual slot 0x30(view). func_08AF1254 binds the view
// to self->+0x20 and a second record to self->+0x10; the view's slots 8 and 0xC fill matrices copied by func_08B4E734 to self + 0x60 /
// + 0xA0; self->+0x34 goes to both through func_08AF1274; the rest is more state calls, slot 0 of *(self->+0x20) and
// func_08AF484C on both records with their vtable word reset to 0x08CD7310.
void func_0890B92C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t object = ctx.gpr[6];
    const std::uint32_t fourth = ctx.gpr[7];
    GuestFrame frame(ctx, 0xE0u);
    const std::uint32_t target_slot = self + 0x20u;
    std::uint32_t view = 0u;
    call_set(rt, ctx, 0x08AF2C0Cu, {word(rt, self), second, 0u, fourth});
    call_set(rt, ctx, 0x08AF2864u, {word(rt, self)});
    call_set(rt, ctx, 0x08AFBEF4u, {});
    call_set(rt, ctx, 0x08B58874u, {0x77u});
    call_set(rt, ctx, 0x08B58874u, {0x69u});
    call_set(rt, ctx, 0x08B54FB8u, {0x65u});
    call_set(rt, ctx, 0x08B54FB8u, {0x66u});
    call_set(rt, ctx, 0x08B54FB8u, {0x67u});
    call_set(rt, ctx, 0x08B54FB8u, {0x68u});
    call_set(rt, ctx, 0x08B58874u, {0x64u});
    call_set(rt, ctx, 0x08B57744u, {0u, 0xFFFFFFFFu});
    call_set(rt, ctx, 0x08B57278u, {1u, 2u});
    call_set(rt, ctx, 0x08B58874u, {0x78u});
    call_set(rt, ctx, 0x08B54FB8u, {0x8002u});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Fu});
    call_set(rt, ctx, 0x08B54FB8u, {2u});
    call_set(rt, ctx, 0x08B58874u, {0x70u});
    call_set(rt, ctx, 0x08B58A74u, {1u});
    call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
    call_setf(rt, ctx, 0x08B5877Cu, {6u}, {std::bit_cast<float>(0x3E008081u)});
    call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x306u, 0x307u});
    call_set(rt, ctx, 0x08B587A8u, {5u});
    call_set(rt, ctx, 0x08B57750u, {3u, 0u});
    call_setf(rt, ctx, 0x08B53844u, {}, {1.0f});
    call_setf(rt, ctx, 0x08B581BCu, {}, {1.0f});
    call_set(rt, ctx, 0x08AF1308u, {word(rt, target_slot), frame.at(0xCu)});
    call_setf(rt, ctx, 0x08B581F4u, {}, {float_at(rt, frame.at(0xCu)), float_at(rt, frame.at(0x10u)), float_at(rt, frame.at(0x14u)), 0.0f});
    if (word(rt, self + 0x5Cu) != 0u) view = word(rt, self + 0x44u);
    const std::uint32_t record = frame.at(0x18u);
    put(rt, record, 0x08CD71A8u);
    put(rt, record + 4u, view);
    for (std::uint32_t offset = 8u; offset <= 0x18u; offset += 4u) put(rt, record + offset, 0u);
    std::uint32_t owner = word(rt, 0x08CFFAACu);
    if (call_set(rt, ctx, 0x08BDADF8u, {0x08CE9F74u}) != 0u) {
        const float ratio = psp_mul(static_cast<float>(signed_of(word(rt, owner + 0xF0u))), std::bit_cast<float>(0x358637BDu));
        if (!(std::bit_cast<float>(0x3C8CA11Cu) <= ratio)) {
            const std::uint32_t toggled = (byte_u(rt, 0x08CDD7F0u) ^ 1u) != 0u ? 1u : 0u;
            put8(rt, 0x08CDD7F0u, toggled);
            float scale_x = 0.0f;
            float scale_y = 0.0f;
            if (toggled != 0u) {
                const std::uint32_t size = word(rt, second);
                const float half = 0.5f;
                const float height = static_cast<float>(signed_of(word(rt, size + 0x1Cu)));
                const float width = static_cast<float>(signed_of(word(rt, size + 0x18u)));
                scale_y = half / height;
                scale_x = half / width;
            }
            put_float(rt, record + 0xCu, scale_x);
            put_float(rt, record + 0x10u, scale_y);
            view = record;
        }
    }
    if (object != 0u && view != 0u) {
        call_virtual_at(rt, ctx, object, 0x30u, {object, view});
        view = object;
    }
    call_set(rt, ctx, 0x08AF1254u, {word(rt, target_slot), view});
    const std::uint32_t pass = frame.at(0u);
    put(rt, pass, 0x08CD7240u);
    put(rt, pass + 4u, view);
    put(rt, pass + 8u, 0u);
    call_set(rt, ctx, 0x08AF1254u, {word(rt, self + 0x10u), pass});
    if (view != 0u) {
        call_virtual_v1(rt, ctx, view, 8u, {frame.at(0x80u), view});
        call_set(rt, ctx, 0x08B4E734u, {self + 0x60u, frame.at(0x80u)});
        call_virtual_at(rt, ctx, view, 0xCu, {frame.at(0x40u), view});
        call_set(rt, ctx, 0x08B4E734u, {self + 0xA0u, frame.at(0x40u)});
    }
    const std::uint32_t shared = word(rt, self + 0x34u);
    call_set(rt, ctx, 0x08AF1274u, {word(rt, target_slot), shared});
    call_set(rt, ctx, 0x08AF1274u, {word(rt, self + 0x10u), shared});
    call_set(rt, ctx, 0x08B587F8u, {0u});
    call_set(rt, ctx, 0x08B587D0u, {2u});
    const std::uint32_t pipeline = word(rt, target_slot);
    call_virtual_at(rt, ctx, pipeline, 0u, {pipeline, word(rt, self)});
    call_set(rt, ctx, 0x08B5711Cu, {0x481u});
    call_set(rt, ctx, 0x08AFBF3Cu, {});
    call_set(rt, ctx, 0x08B54FB8u, {2u});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Cu});
    put(rt, pass, 0x08CD7310u);
    call_set(rt, ctx, 0x08AF484Cu, {pass});
    put(rt, record, 0x08CD7310u);
    call_set(rt, ctx, 0x08AF484Cu, {record});
}
// func_08A54B50(out, source, context): builds a value object into `out` (returned). With source->+0x58 set: an offset
// s1 = context->+0x1C (or checked context->+0x18's +0xC), the owner context->+0x14's virtual slot 0x120 gives a length
// and func_08A950D0(tmp, s1, length, 16) a record whose slots 0xDC / 0xD8 give two numbers; a 24-byte block
// (func_08A5BC20) is set up by func_08A56018, wrapped (func_08A5BAB8, func_08A52AA8) and func_08A57B54(res, wrap, wrap2,
// number) builds the result, taken by func_08A56974 and assigned to out (func_08A76ABC). With the byte clear: for every
// node of source's circular list (from source->+0x44 to the sentinel source + 0x3C, step func_08B5F16C) a 24-byte object
// (func_08A76ACC, vtable 0x08CB6B88, func_08A6972C copying node->+0x10) joins out's value list through func_08A57B54 /
// virtual slots 0xF0 / 0xA0. Temporaries are destroyed on the way out.
void func_08A54B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t context = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    if (byte_u(rt, source + 0x58u) != 0u) {
        std::uint32_t base = word(rt, context + 0x1Cu);
        if (base == 0u) base = word(rt, checked(rt, ctx, word(rt, context + 0x18u), 0x08CAF540u) + 0xCu);
        const std::uint32_t owner = checked(rt, ctx, word(rt, context + 0x14u), 0x08CAF548u);
        const std::uint32_t length = call_virtual_at(rt, ctx, owner, 0x120u, {owner});
        call_set(rt, ctx, 0x08A950D0u, {frame.at(0x20u), base, length, 0x10u});
        std::uint32_t record = checked(rt, ctx, word(rt, frame.at(0x20u)), 0x08CAF5F8u);
        const std::uint32_t first = call_virtual_at(rt, ctx, record, 0xDCu, {record, 0u});
        record = checked(rt, ctx, word(rt, frame.at(0x20u)), 0x08CAF5F8u);
        const std::uint32_t second = call_virtual_at(rt, ctx, record, 0xD8u, {record});
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
        call_set(rt, ctx, 0x08A56018u, {block, first, second, 0u, 0u});
        put(rt, frame.at(4u), block);
        call_set(rt, ctx, 0x08A5BAB8u, {frame.at(0x1Cu), frame.at(4u)});
        put(rt, frame.at(0x14u), source);
        call_set(rt, ctx, 0x08A52AA8u, {frame.at(0x18u), frame.at(0x14u)});
        call_set(rt, ctx, 0x08A57B54u, {frame.at(0x10u), frame.at(0x18u), frame.at(0x1Cu), first});
        const std::uint32_t built = checked(rt, ctx, word(rt, frame.at(0x10u)), 0x08CAF5E0u);
        call_set(rt, ctx, 0x08A56974u, {built});
        call_set(rt, ctx, kDestroy, {frame.at(0x10u)});
        call_set(rt, ctx, 0x08A76ABCu, {out, frame.at(0x20u)});
        call_set(rt, ctx, kDestroy, {frame.at(0x18u)});
        call_set(rt, ctx, kDestroy, {frame.at(0x1Cu)});
        call_set(rt, ctx, kDestroy, {frame.at(0x20u)});
        ctx.gpr[2] = out;
        return;
    }
    call_set(rt, ctx, 0x08A813F4u, {frame.at(0x18u)});
    std::uint32_t node = word(rt, source + 0x44u);
    const std::uint32_t sentinel = source + 0x3Cu;
    put(rt, frame.at(0x30u), sentinel);
    if (sentinel != node) {
        put(rt, frame.at(0x34u), context + 0x18u);
        do {
            const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
            call_set(rt, ctx, 0x08A76ACCu, {block});
            put(rt, block, 0x08CB6B88u);
            call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, node + 0x10u});
            put(rt, frame.at(0u), block);
            call_set(rt, ctx, 0x08A76AB4u, {frame.at(0xCu), frame.at(0u)});
            call_set(rt, ctx, 0x08A76ABCu, {frame.at(0x1Cu), frame.at(0xCu)});
            call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
            const std::uint32_t slot = node + 0x14u;
            const std::uint32_t delta = word(rt, checked(rt, ctx, word(rt, slot), 0x08CAF594u) + 0x10u);
            std::uint32_t total = word(rt, context + 0x1Cu);
            if (total == 0u) total = word(rt, checked(rt, ctx, word(rt, context + 0x18u), 0x08CAF540u) + 0xCu);
            total += delta;
            const std::uint32_t entry = checked(rt, ctx, word(rt, slot), 0x08CAF594u);
            call_set(rt, ctx, 0x08A57B54u, {frame.at(8u), entry + 0xCu, word(rt, frame.at(0x34u)), total});
            const std::uint32_t built = checked(rt, ctx, word(rt, frame.at(8u)), 0x08CAF5E0u);
            call_virtual_at(rt, ctx, built, 0xF0u, {frame.at(0x20u), built});
            call_set(rt, ctx, kDestroy, {frame.at(8u)});
            const std::uint32_t list = checked(rt, ctx, word(rt, frame.at(0x18u)), 0x08CAF604u);
            call_virtual_at(rt, ctx, list, 0xA0u, {list, frame.at(0x1Cu), frame.at(0x20u)});
            node = call_set(rt, ctx, 0x08B5F16Cu, {node});
            call_set(rt, ctx, kDestroy, {frame.at(0x20u)});
            call_set(rt, ctx, kDestroy, {frame.at(0x1Cu)});
        } while (word(rt, frame.at(0x30u)) != node);
    }
    call_set(rt, ctx, 0x08A76ABCu, {out, frame.at(0x18u)});
    call_set(rt, ctx, kDestroy, {frame.at(0x18u)});
    ctx.gpr[2] = out;
}
// func_08B71394(a_lo, a_hi, b_lo, b_hi): libgcc's soft-float double multiply (_fpmul_parts + pack): both doubles are
// unpacked by func_08B72314 into frame records {class, sign, exponent, fraction}. NaN, infinity and zero operands return
// the corresponding record (sign = signs differ; inf x 0 gives the NaN record 0x08C966B0) through the packer func_08B72030.
// Otherwise the 64 x 64-bit fraction product is built from four 32 x 32 multiplies, the exponents added (+4), the 128-bit
// result is shifted right while its high word exceeds 0x1FFFFFFF (a dropped 1 bit sets the top bit of the low half) or
// left until above 0x0FFFFFFF, the round-to-even fix-up on the low byte 0x80 applied and the result packed (v0:v1 = the double).
void func_08B71394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t a = frame.at(0x10u);
    const std::uint32_t b = frame.at(0x28u);
    const std::uint32_t result = frame.at(0x40u);
    put(rt, frame.at(0u), ctx.gpr[4]);
    put(rt, frame.at(4u), ctx.gpr[5]);
    put(rt, frame.at(8u), ctx.gpr[6]);
    put(rt, frame.at(0xCu), ctx.gpr[7]);
    call_set(rt, ctx, 0x08B72314u, {frame.at(0u), a});
    call_set(rt, ctx, 0x08B72314u, {frame.at(8u), b});
    const std::uint32_t class_a = word(rt, a);
    const std::uint32_t class_b = word(rt, b);
    // The returned record takes the sign "the signs differ".
    const auto sign_of_product = [&](std::uint32_t record) {
        put(rt, record + 4u, word(rt, a + 4u) != word(rt, b + 4u) ? 1u : 0u);
        call_set(rt, ctx, 0x08B72030u, {record});
    };
    if (class_a < 2u) return sign_of_product(a);
    if (class_b < 2u) return sign_of_product(b);
    if (class_a == 4u) {
        if (class_b == 2u) return void(call_set(rt, ctx, 0x08B72030u, {0x08C966B0u}));
        return sign_of_product(a);
    }
    if (class_b == 4u) {
        if (class_a == 2u) return void(call_set(rt, ctx, 0x08B72030u, {0x08C966B0u}));
        return sign_of_product(b);
    }
    if (class_a == 2u) return sign_of_product(a);
    if (class_b == 2u) return sign_of_product(b);
    const std::uint32_t a_low = word(rt, a + 0x10u);
    const std::uint32_t a_high = word(rt, a + 0x14u);
    const std::uint32_t b_low = word(rt, b + 0x10u);
    const std::uint32_t b_high = word(rt, b + 0x14u);
    const std::uint64_t cross_one = static_cast<std::uint64_t>(b_low) * a_high;
    const std::uint64_t cross_two = static_cast<std::uint64_t>(a_low) * b_high;
    const std::uint64_t top = static_cast<std::uint64_t>(b_high) * a_high;
    const std::uint64_t bottom = static_cast<std::uint64_t>(b_low) * a_low;
    const std::uint32_t one_low = static_cast<std::uint32_t>(cross_one);
    const std::uint32_t one_high = static_cast<std::uint32_t>(cross_one >> 32);
    const std::uint32_t two_low = static_cast<std::uint32_t>(cross_two);
    const std::uint32_t two_high = static_cast<std::uint32_t>(cross_two >> 32);
    const std::uint32_t top_low = static_cast<std::uint32_t>(top);
    const std::uint32_t top_high = static_cast<std::uint32_t>(top >> 32);
    const std::uint32_t bottom_low = static_cast<std::uint32_t>(bottom);
    const std::uint32_t bottom_high = static_cast<std::uint32_t>(bottom >> 32);
    const std::uint32_t middle_low = two_low + one_low;
    const std::uint32_t middle_sum = two_high + one_high;
    const std::uint32_t middle_high = (middle_low < two_low ? 1u : 0u) + middle_sum;
    // Carry out of the 64-bit middle sum (it is worth 2^32 in the high product).
    std::uint32_t carry_count_low = 0u;
    std::uint32_t carry_count_high = 0u;
    if (middle_high < two_high || (middle_high == two_high && middle_low < two_low)) carry_count_high = 1u;
    std::uint32_t low_low = bottom_low;
    std::uint32_t low_high = bottom_high + middle_low;
    if (low_high < bottom_high) {
        const std::uint32_t incremented = carry_count_low + 1u;
        carry_count_high = (incremented < carry_count_low ? 1u : 0u) + carry_count_high;
        carry_count_low = incremented;
    }
    const std::uint32_t partial = top_low + middle_high;
    std::uint32_t high_low = carry_count_low + partial;
    std::uint32_t high_high = carry_count_high + (top_high + (partial < top_low ? 1u : 0u));
    high_high = (high_low < carry_count_low ? 1u : 0u) + high_high;
    std::uint32_t exponent = word(rt, a + 8u) + word(rt, b + 8u) + 4u;
    put(rt, result + 4u, (word(rt, a + 4u) ^ word(rt, b + 4u)) != 0u ? 1u : 0u);
    put(rt, result + 8u, exponent);
    if (0x1FFFFFFFu < high_high) {
        bool again;
        do {
            const std::uint32_t dropped = high_low & 1u;
            const std::uint32_t shifted_low = (high_high << 31) | (high_low >> 1);
            const std::uint32_t shifted_high = high_high >> 1;
            again = 0x1FFFFFFFu < shifted_high;
            const std::uint32_t carried = low_high << 31;
            high_low = shifted_low;
            high_high = shifted_high;
            ++exponent;
            if (dropped != 0u) {
                low_low = carried | (low_low >> 1);
                low_high = 0x80000000u | (low_high >> 1);
            }
        } while (again);
        put(rt, result + 8u, exponent);
    }
    if (!(0x0FFFFFFFu < high_high)) {
        exponent = word(rt, result + 8u);
        bool again;
        do {
            const std::uint32_t from_high_low = high_low >> 31;
            const std::uint32_t from_low_low = low_low >> 31;
            const std::uint32_t new_high = (high_high << 1) | from_high_low;
            const std::uint32_t new_low_high = (low_high << 1) | from_low_low;
            std::uint32_t new_low = high_low << 1;
            const std::uint32_t new_low_low = low_low << 1;
            --exponent;
            if (signed_of(low_high) < 0) new_low |= 1u;
            high_low = new_low;
            high_high = new_high;
            again = !(0x0FFFFFFFu < high_high);
            low_low = new_low_low;
            low_high = new_low_high;
        } while (again);
        put(rt, result + 8u, exponent);
    }
    if ((high_low & 0xFFu) == 0x80u && (high_low & 0x100u) == 0u && (low_low | low_high) != 0u) {
        const std::uint32_t sum = high_low + 0x80u;
        high_high = (sum < high_low ? 1u : 0u) + high_high;
        high_low = sum & ~0xFFu;
    }
    put(rt, result + 0x10u, high_low);
    put(rt, result + 0x14u, high_high);
    put(rt, result, 3u);
    call_set(rt, ctx, 0x08B72030u, {result, 0u});
}
// func_088F469C(self, config): nothing when config is null; returns the byte when self->+0x194 is set. Else self->+0x2C /
// +0x30 default to 400.0 / 0.1 and the config object's virtual slot 0xC (name lookup into a frame holder; the holder is
// released through its slot 4) overrides them from the entries 0x08CA0C18 / 0x08CA0C20 (accessor func_08BA0D34) and +0x34
// from 0x08CA0C30 (func_08BB9C24). Then the six 56-byte records at self->+0x40: each looks up its name word of the table at
// 0x08C8A408 (func_08BB9E4C gives four floats, color w kept at least 0.01, else the default from func_08BBA094) and
// converts the 8-byte color row of the table at 0x08C8A320 (row 4 -> 6 when self->+0x195, row 5 -> 7 when self->+0x196) to
// floats (bytes x 0.01 / 0.1 / 0.01 / 1 / 0.01 / 0.01 / 0.01 and the last byte as a word). self->+0x194 = 1, +0x3C = 0x44,
// byte +0x38 = 0.
void func_088F469C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t config = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    if (config == 0u) return;
    const std::uint32_t done = byte_u(rt, self + 0x194u);
    if (done != 0u) {
        ctx.gpr[2] = done;
        return;
    }
    const float hundredth = std::bit_cast<float>(0x3C23D70Au);
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    put_float(rt, self + 0x2Cu, std::bit_cast<float>(0x43C80000u));
    put_float(rt, self + 0x30u, tenth);
    const std::uint32_t holder = frame.at(4u);
    const auto release_holder = [&]() {
        const std::uint32_t object = word(rt, holder);
        if (object != 0u) call_virtual_at(rt, ctx, object, 4u, {object});
    };
    const auto lookup = [&](std::uint32_t name) {
        put(rt, holder, 0u);
        return call_virtual_at(rt, ctx, config, 0xCu, {config, holder, name}) != 0u;
    };
    float value = 0.0f;
    for (const auto &entry : {std::pair<std::uint32_t, std::uint32_t>{0x08CA0C18u, 0x2Cu}, std::pair<std::uint32_t, std::uint32_t>{0x08CA0C20u, 0x30u}}) {
        bool found = false;
        if (lookup(entry.first)) {
            call_set(rt, ctx, 0x08BA0D34u, {holder});
            value = float_at(rt, ctx.gpr[2]);
            found = true;
        }
        release_holder();
        if (found) put_float(rt, self + entry.second, value);
    }
    call_set(rt, ctx, 0x08BBA1DCu, {frame.at(0u)});
    bool found = false;
    if (lookup(0x08CA0C30u)) {
        call_set(rt, ctx, 0x08BB9C24u, {holder});
        put_float(rt, frame.at(0u), float_at(rt, ctx.gpr[2]));
        found = true;
    }
    release_holder();
    if (found) put_float(rt, self + 0x34u, float_at(rt, frame.at(0u)));
    std::uint32_t name_slot = 0x08C8A408u;
    std::uint32_t record = self + 0x40u;
    const std::uint32_t values = frame.at(8u);
    for (std::uint32_t index = 0u; index != 6u; ++index, name_slot += 4u, record += 0x38u) {
        std::uint32_t row = index;
        if (byte_u(rt, self + 0x195u) != 0u && index == 4u) row = 6u;
        if (byte_u(rt, self + 0x196u) != 0u && index == 5u) row = 7u;
        const std::uint32_t colors = 0x08C8A320u + (row << 3);
        call_set(rt, ctx, 0x08BBA094u, {values});
        bool present = false;
        if (lookup(word(rt, name_slot))) {
            const std::uint32_t source = call_set(rt, ctx, 0x08BB9E4Cu, {holder});
            const std::uint32_t fourth = word(rt, source + 0xCu);
            put(rt, values, word(rt, source));
            put(rt, values + 4u, word(rt, source + 4u));
            put(rt, values + 8u, word(rt, source + 8u));
            put(rt, values + 0xCu, fourth);
            present = true;
        }
        release_holder();
        if (present) {
            const float alpha = float_at(rt, values + 0xCu);
            put_float(rt, record, float_at(rt, values));
            put_float(rt, record + 4u, float_at(rt, values + 4u));
            put_float(rt, record + 8u, float_at(rt, values + 8u));
            put_float(rt, record + 0xCu, hundredth <= alpha ? alpha : 1.0f);
        }
        put_float(rt, record + 0x10u, psp_mul(static_cast<float>(byte_u(rt, colors)), hundredth));
        put_float(rt, record + 0x14u, psp_mul(static_cast<float>(byte_u(rt, colors + 1u)), tenth));
        put_float(rt, record + 0x18u, psp_mul(static_cast<float>(byte_u(rt, colors + 2u)), hundredth));
        put_float(rt, record + 0x1Cu, static_cast<float>(byte_u(rt, colors + 3u)));
        put_float(rt, record + 0x20u, psp_mul(static_cast<float>(byte_u(rt, colors + 4u)), hundredth));
        put_float(rt, record + 0x2Cu, psp_mul(static_cast<float>(byte_u(rt, colors + 5u)), hundredth));
        put_float(rt, record + 0x30u, psp_mul(static_cast<float>(byte_u(rt, colors + 6u)), hundredth));
        put(rt, record + 0x34u, byte_u(rt, colors + 7u));
    }
    ctx.gpr[2] = 1u;
    put8(rt, self + 0x194u, 1u);
    put(rt, self + 0x3Cu, 0x44u);
    put8(rt, self + 0x38u, 0u);
}
// func_0887D990(self, scale): with A = self->+0x58, when A's half +0x26 has bit 0 and the slot object
// A->+(0x14 + (1 - A->+0x25) * 8) has bit 0 in its word +0x18: A->+0x24 loses bit 16 and, when that slot's object has bit 1 in
// +0x18, the slot pair (+4 object, +8 shared control block) is cleared and its block released; otherwise func_0887D3BC(self)
// flips the byte A->+0x25 and func_08AC1F40(tmp, object->+8, self, 0x50) builds a new handle pair: self->+0x50 takes the
// pointer, self->+0x54 the shared block (use count raised, old block released, the temporary's reference released), then
// slot 8(self->+0x50, 0) and func_0887D6FC(self). Then func_0886BAF8(self->+0x60) (after those paths); func_0886BAEC and func_0886B120
// on self->+0x60 end it when non-zero, func_0887D38C(self) when zero; with scale > 0 the object self->+0x50 gets
// slot 0x34 and slot 0x38(0.0) when its slot 0x1C holds, else slot 0x38(scale x 60.0).
void func_0887D990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t area = word(rt, self + 0x58u);
    bool through_release = false;
    if ((half_u(rt, area + 0x26u) & 1u) != 0u) {
        const std::uint32_t current = word(rt, area + (((1u - byte_u(rt, area + 0x25u)) << 3)) + 0x14u);
        if (current != 0u && (word(rt, current + 0x18u) & 1u) != 0u) {
            put(rt, area + 0x24u, word(rt, area + 0x24u) & ~0x10000u);
            const std::uint32_t area_again = word(rt, self + 0x58u);
            const std::uint32_t chosen = word(rt, area_again + (((1u - byte_u(rt, area_again + 0x25u)) << 3)) + 0x14u);
            through_release = true;
            if (chosen != 0u && ((word(rt, chosen + 0x18u) >> 1) & 1u) != 0u) {
                const std::uint32_t slot = area_again + 0x10u + ((1u - byte_u(rt, area_again + 0x25u)) << 3);
                const std::uint32_t block = word(rt, slot + 8u);
                put(rt, slot + 4u, 0u);
                put(rt, slot + 8u, 0u);
                if (block != 0u) release_shared(rt, ctx, block);
            } else {
                call_set(rt, ctx, 0x0887D3BCu, {self});
                const std::uint32_t swapped = word(rt, self + 0x58u);
                put8(rt, swapped + 0x25u, 1u - byte_u(rt, swapped + 0x25u));
                const std::uint32_t flipped = word(rt, self + 0x58u);
                const std::uint32_t entry = word(rt, flipped + (byte_u(rt, flipped + 0x25u) << 3) + 0x14u);
                const std::uint32_t source = entry != 0u ? word(rt, entry + 8u) : 0u;
                call_set(rt, ctx, 0x08AC1F40u, {frame.at(0u), source, self, 0x50u});
                const std::uint32_t pointer = word(rt, frame.at(0u));
                std::uint32_t fresh = word(rt, frame.at(4u));
                put(rt, self + 0x50u, pointer);
                if (fresh != word(rt, self + 0x54u)) {
                    if (fresh != 0u) count_up(rt, ctx, fresh + 4u);
                    const std::uint32_t old = word(rt, self + 0x54u);
                    if (old != 0u) release_shared(rt, ctx, old);
                    put(rt, self + 0x54u, fresh);
                    fresh = word(rt, frame.at(4u));
                }
                if (fresh != 0u) release_shared(rt, ctx, fresh);
                const std::uint32_t handle = word(rt, self + 0x50u);
                if (handle != 0u) call_virtual_at(rt, ctx, handle, 8u, {handle, 0u});
                call_set(rt, ctx, 0x0887D6FCu, {self});
            }
        }
    }
    if (through_release) call_set(rt, ctx, 0x0886BAF8u, {word(rt, self + 0x60u)});
    if (call_set(rt, ctx, 0x0886BAECu, {word(rt, self + 0x60u)}) != 0u) return;
    if (call_set(rt, ctx, 0x0886B120u, {word(rt, self + 0x60u)}) != 0u) return;
    if (call_set(rt, ctx, 0x0887D38Cu, {self}) == 0u) return;
    if (scale <= 0.0f) return;
    std::uint32_t object = word(rt, self + 0x50u);
    if (call_virtual_at(rt, ctx, object, 0x1Cu, {object}) != 0u) {
        object = word(rt, self + 0x50u);
        call_virtual_at(rt, ctx, object, 0x34u, {object});
        object = word(rt, self + 0x50u);
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x38u);
        call_setf(rt, ctx, ctx.gpr[2], {object}, {0.0f});
    } else {
        object = word(rt, self + 0x50u);
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x38u);
        call_setf(rt, ctx, ctx.gpr[2], {object}, {psp_mul(scale, std::bit_cast<float>(0x42700000u))});
    }
}
// func_08BBAA74(self, actor): returns 0 when self->+8 (the world object) is null. Else graphics set-up calls
// (func_08B56790, func_08B55140) and, with pos = func_08AF26F0(actor), dir = func_08AF2724(actor), the world's +0x68 value
// goes through func_08AF8904. For each of the n elements of self->+0xC (virtual slot 8 count, slot 0xC fills an index half at
// frame +0x1C and a point pointer at +0x24) the record world->+0xA4[world->+0x30[idx].half(+0x2C)] is tested: the squared
// distance from pos must not exceed (0.0001-scale)^2 x record->+0x20^2 and the dot with dir must not be below -2 x +0x20. A
// passing one builds a transform (func_08B4B114 with the point, a plane height from the plane at frame +0x28,
// func_08AF52E4 distance; when positive func_08B5894C(-0.02 x d), func_08B569A0, func_08AF5394, func_08B566BC x2, func_08B56364
// (-record +0x24 / +0x28 / +0x2C), func_08AF9E88(world, idx * 2 + 1, value, 0), func_08B56940, func_08B5894C(0)) and always
// func_08B569A0, func_08B566BC, func_08B56364 (negated record fields), func_08AF9E88(world, idx * 2, value, func_08AF2954(actor))
// and func_08B56940. Last func_08AF8434(world).
void func_08BBAA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t actor = ctx.gpr[5];
    GuestFrame frame(ctx, 0x160u);
    if (word(rt, self + 8u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08B56790u, {1u, frame.at(0x3Cu), 0u, 0u});
    call_set(rt, ctx, 0x08B55140u, {0x65u, 0xDu, frame.at(0xCu)});
    const float scale = float_at(rt, 0x08CE9ECCu);
    const float scale_squared = psp_mul(scale, scale);
    const std::uint32_t position = call_set(rt, ctx, 0x08AF26F0u, {actor});
    const std::uint32_t direction = call_set(rt, ctx, 0x08AF2724u, {actor});
    const std::uint32_t value = word(rt, word(rt, self + 8u) + 0x68u);
    call_set(rt, ctx, 0x08AF8904u, {word(rt, self + 8u), value});
    const std::uint32_t list = word(rt, self + 0xCu);
    const std::uint32_t count = call_virtual_at(rt, ctx, list, 8u, {list});
    if (signed_of(count) > 0) {
        const std::uint32_t y_slot = position + 4u;
        const std::uint32_t z_slot = position + 8u;
        for (std::uint32_t index = 0u; index != count; ++index) {
            const std::uint32_t elements = word(rt, self + 0xCu);
            call_virtual_at(rt, ctx, elements, 0xCu, {elements, index, frame.at(0x1Cu)});
            const std::uint32_t world = word(rt, self + 8u);
            const std::uint32_t element = sext16(half_u(rt, frame.at(0x1Cu)));
            const std::uint32_t point = word(rt, frame.at(0x24u));
            const std::uint32_t row = element * 96u + word(rt, world + 0x30u);
            const std::uint32_t record = word(rt, word(rt, world + 0xA4u) + (sext16(half_u(rt, row + 0x2Cu)) << 2));
            const float dx = float_at(rt, point) - float_at(rt, position);
            const float dy = float_at(rt, point + 4u) - float_at(rt, y_slot);
            const float dz = float_at(rt, point + 8u) - float_at(rt, z_slot);
            const float radius = float_at(rt, record + 0x20u);
            const float distance_squared = (psp_mul(dx, dx) + psp_mul(dy, dy)) + psp_mul(dz, dz);
            const float limit = psp_mul(scale_squared, psp_mul(radius, radius));
            if (limit < distance_squared) continue;
            const float along = (psp_mul(dx, float_at(rt, direction)) + psp_mul(dy, float_at(rt, direction + 4u))) + psp_mul(dz, float_at(rt, direction + 8u));
            if (along < psp_mul(radius, -2.0f)) continue;
            const std::uint32_t box = word(rt, frame.at(0x20u));
            const std::uint32_t pointer_x = record + 0x24u;
            const std::uint32_t pointer_y = record + 0x28u;
            const std::uint32_t pointer_z = record + 0x2Cu;
            put(rt, frame.at(0x2Cu), word(rt, box));
            put(rt, frame.at(0x30u), word(rt, box + 4u));
            put(rt, frame.at(0x34u), word(rt, box + 8u));
            put(rt, frame.at(0x38u), word(rt, box + 0xCu));
            put(rt, frame.at(0x110u), pointer_x);
            put(rt, frame.at(0x104u), pointer_y);
            put(rt, frame.at(0x100u), pointer_z);
            call_setf(rt, ctx, 0x08B4B114u, {frame.at(0x2Cu), frame.at(0x80u)}, {float_at(rt, point), float_at(rt, point + 4u), float_at(rt, point + 8u)});
            const std::uint32_t plane = word(rt, frame.at(0x28u));
            const float height = -((psp_mul(float_at(rt, plane), float_at(rt, point)) + psp_mul(float_at(rt, plane + 8u), float_at(rt, point + 8u))) +
                                   float_at(rt, plane + 0xCu)) /
                                 float_at(rt, plane + 4u);
            put_float(rt, frame.at(0u), float_at(rt, point));
            put_float(rt, frame.at(4u), height);
            put_float(rt, frame.at(8u), float_at(rt, point + 8u));
            call_set(rt, ctx, 0x08AF52E4u, {plane, frame.at(0u), frame.at(0x3Cu)});
            const float gap = ctx.fpr[0];
            if (0.0f < gap) {
                call_setf(rt, ctx, 0x08B5894Cu, {}, {psp_mul(gap, std::bit_cast<float>(0xBCA3D70Au))});
                call_set(rt, ctx, 0x08B569A0u, {});
                call_set(rt, ctx, 0x08AF5394u, {frame.at(0xC0u), frame.at(0xCu), word(rt, frame.at(0x28u))});
                call_set(rt, ctx, 0x08B566BCu, {frame.at(0xC0u)});
                call_set(rt, ctx, 0x08B566BCu, {frame.at(0x80u)});
                call_setf(rt, ctx, 0x08B56364u, {}, {-float_at(rt, record + 0x24u), -float_at(rt, record + 0x28u), -float_at(rt, record + 0x2Cu)});
                const std::uint32_t slot = sext16(half_u(rt, frame.at(0x1Cu)));
                call_set(rt, ctx, 0x08AF9E88u, {word(rt, self + 8u), (slot << 1) + 1u, value, 0u});
                call_set(rt, ctx, 0x08B56940u, {});
                call_setf(rt, ctx, 0x08B5894Cu, {}, {0.0f});
            }
            call_set(rt, ctx, 0x08B569A0u, {});
            call_set(rt, ctx, 0x08B566BCu, {frame.at(0x80u)});
            call_setf(rt, ctx, 0x08B56364u, {},
                      {-float_at(rt, word(rt, frame.at(0x110u))), -float_at(rt, word(rt, frame.at(0x104u))), -float_at(rt, word(rt, frame.at(0x100u)))});
            const std::uint32_t slot = sext16(half_u(rt, frame.at(0x1Cu)));
            const std::uint32_t current_world = word(rt, self + 8u);
            const std::uint32_t extra = call_set(rt, ctx, 0x08AF2954u, {actor});
            call_set(rt, ctx, 0x08AF9E88u, {current_world, slot << 1, value, extra});
            call_set(rt, ctx, 0x08B56940u, {});
        }
    }
    call_set(rt, ctx, 0x08AF8434u, {word(rt, self + 8u)});
}
// func_08C160CC(vector, position, value): the libstdc++ vector<44-byte element>::_M_insert_aux. With room: the last
// element is copied one past the end (unless the end is null), the end moves, `value` is copied to a frame temporary
// (frame +4..+0x2C), the elements between `position` and the old last are copied backwards one at a time and the temporary
// goes to `position` (v0 = the temporary's last word). Full: a block of twice the size in elements (one at least; 44 x count bytes,
// -4 on overflow; func_08B5ADDC(0x08CA9B68) at 0x05D1745D elements) from func_08A5BC20(bytes, 4) takes the prefix
// (func_08C15FE4), a copy of the value and the tail (func_08C15FE4); the old range is destroyed (func_08C15E7C) and freed
// (func_08A5BC54) and begin / end / capacity updated (v0 = block + bytes).
void func_08C160CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    constexpr std::uint32_t kElementWords = 11u;
    constexpr std::uint32_t kInverse = 0xBA2E8BA3u;
    const std::uint32_t end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) copy_words(rt, end, end - 0x2Cu, kElementWords);
        const std::uint32_t old_end = word(rt, vector + 4u);
        const std::uint32_t last = old_end - 0x2Cu;
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(last - position) >> 2) * kInverse;
        put(rt, vector + 4u, old_end + 0x2Cu);
        const std::uint32_t temporary = frame.at(4u);
        copy_words(rt, temporary, value, kElementWords);
        if (signed_of(count) > 0) {
            std::uint32_t destination = last;
            std::uint32_t source = old_end - 0x58u;
            for (std::uint32_t index = 0u; index != count; ++index) {
                copy_words(rt, destination, source, kElementWords);
                destination -= 0x2Cu;
                source -= 0x2Cu;
            }
        }
        copy_words(rt, position, temporary, kElementWords);
        ctx.gpr[2] = word(rt, frame.at(0x2Cu));
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2) * kInverse;
    if (size == 0x05D1745Du) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA9B68u});
    std::uint32_t grown = 1u;
    if (size != 0u) grown = size * 2u;
    std::uint32_t bytes = 0xFFFFFFFCu;
    if (size == 0u || !(grown < size)) bytes = grown * 44u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t prefix_end = call_set(rt, ctx, 0x08C15FE4u, {word(rt, vector), position, block, frame.at(2u)});
    if (prefix_end != 0u) copy_words(rt, prefix_end, value, kElementWords);
    const std::uint32_t new_end = call_set(rt, ctx, 0x08C15FE4u, {position, word(rt, vector + 4u), prefix_end + 0x2Cu, frame.at(1u)});
    call_set(rt, ctx, 0x08C15E7Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old = word(rt, vector);
    if (old != 0u) call_set(rt, ctx, 0x08A5BC54u, {old, static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old) >> 2) << 2, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_end);
    put(rt, vector, block);
}
} // namespace hot_batch_115
} // namespace gt::game

#define HOT115(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_115::name, gt::ReturnKind::kind)
// HOT115(0x08AD0C9Cu, func_08AD0C9C, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x08C11318u, func_08C11318, Word);
HOT115(0x089C06C0u, func_089C06C0, Word);
HOT115(0x08A43398u, func_08A43398, Word);
HOT115(0x089FB5B4u, func_089FB5B4, Word);
// HOT115(0x08B13388u, func_08B13388, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT115(0x08B1554Cu, func_08B1554C, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x08AFE0C4u, func_08AFE0C4, Word);
HOT115(0x08998838u, func_08998838, Word);
// HOT115(0x08C4C0A0u, func_08C4C0A0, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT115(0x08AEE144u, func_08AEE144, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT115(0x08C0E874u, func_08C0E874, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x08BB1FF0u, func_08BB1FF0, Word);
// HOT115(0x08AD90ACu, func_08AD90AC, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x08C227FCu, func_08C227FC, Word);
// HOT115(0x088175B8u, func_088175B8, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT115(0x08AC9CC8u, func_08AC9CC8, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT115(0x08A270ACu, func_08A270AC, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x0890B92Cu, func_0890B92C, Word);
// HOT115(0x08A54B50u, func_08A54B50, Word);  // batch 115: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT115(0x08B71394u, func_08B71394, DoubleWord);
HOT115(0x088F469Cu, func_088F469C, Word);
HOT115(0x0887D990u, func_0887D990, Word);
// HOT115(0x08BBAA74u, func_08BBAA74, Word);  // unregistered 2026-11-05: with the batch-329 set the synthetic gate (dealer,race)
// deterministically faults the decompiled run at 0x0002594C where the original returns (a0=0x09492F20 a1=0x09EDBAB8). The
// function reads frame +0x20 (lw 0x20($sp) at 08BBAC24, then 4 words from it) and never writes it: suspected stale-stack
// read (FINDINGS: "A callee can read a stale stack slot", func_0894D484). Route cases were equal (15 per route).
HOT115(0x08C160CCu, func_08C160CC, Word);
