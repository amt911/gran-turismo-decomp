// Hot batch 111: functions of 61-100 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_111 {
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

// func_0885BFE0(self) -> 0: picks one of five parameter blocks of o = self->+0 by its flag bytes (+0x18 chooses +0x100 over
// +0x90; with +0x14 set, +0x17 picks +0xE4, else +0x15 picks +0xC8 over +0xAC) and plays it on self->+4: with +0x17 set
// through func_08865A04(self->+4, b->+4, +8, +0xC, +0x10, +0x18); otherwise through func_08865B98(self->+4, n, b->+4, +8,
// +0xC, +0x10, +0x18, id) with n = func_088D5DA8(o->+0x7C), capped by b->+0 when that is set, and id = b->+0x14 unless
// +0x15 is set (then -1).
void func_0885BFE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self);
    std::uint32_t block = byte_u(rt, owner + 0x18u) != 0u ? owner + 0x100u : owner + 0x90u;
    const std::uint32_t looped = byte_u(rt, owner + 0x17u);
    const std::uint32_t shared = byte_u(rt, owner + 0x15u);
    if (byte_u(rt, owner + 0x14u) != 0u) {
        if (looped != 0u) block = owner + 0xE4u;
        else block = shared == 0u ? owner + 0xACu : owner + 0xC8u;
    }
    const std::uint32_t id = shared != 0u ? 0xFFFFFFFFu : word(rt, block + 0x14u);
    GuestFrame frame(ctx, 0x10u);
    if (looped != 0u) {
        call_set(rt, ctx, 0x08865A04u, {word(rt, self + 4u), word(rt, block + 4u), word(rt, block + 8u), word(rt, block + 0xCu),
                                         word(rt, block + 0x10u), word(rt, block + 0x18u)});
    } else {
        const std::uint32_t count = call_set(rt, ctx, 0x088D5DA8u, {word(rt, owner + 0x7Cu)});
        const std::uint32_t cap = word(rt, block);
        const std::uint32_t used = cap != 0u ? static_cast<std::uint32_t>(std::min(signed_of(count), signed_of(cap))) : count;
        call_set(rt, ctx, 0x08865B98u, {word(rt, self + 4u), used, word(rt, block + 4u), word(rt, block + 8u), word(rt, block + 0xCu),
                                         word(rt, block + 0x10u), word(rt, block + 0x18u), id});
    }
    ctx.gpr[2] = 0u;
}

// func_0888D8B4(self, a1, keys, count): for each halfword key whose entry func_088DA788(self->+4, key) has the byte +0x26
// clear while func_0889417C(self->+0xC, key)->+0 has bit 2 and func_088DB2C8(self->+4, key) finds it: the byte at
// self->+4->+0x618 is cleared and self's virtual slot 0x70 runs with (self, a1, &key, 1, 0, 0).
void func_0888D8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t keys = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    if (signed_of(count) <= 0) return;
    GuestFrame frame(ctx, 0x30u);
    for (std::uint32_t index = 0u; index != count; ++index) {
        const std::uint32_t key = half_u(rt, keys + index * 2u);
        const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), key});
        const std::uint32_t flags = call_set(rt, ctx, 0x0889417Cu, {word(rt, self + 0xCu)});
        ctx.gpr[2] = byte_u(rt, entry + 0x26u);  // a skipped element leaves v0 = the value its test read
        if (ctx.gpr[2] != 0u) continue;
        ctx.gpr[2] = word(rt, flags) & 2u;
        if (ctx.gpr[2] == 0u) continue;
        const std::uint32_t found = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), key});
        ctx.gpr[4] = self;
        ctx.gpr[5] = second;
        ctx.gpr[6] = frame.at(0u);
        ctx.gpr[7] = 1u;
        ctx.gpr[8] = 0u;
        ctx.gpr[9] = 0u;
        if (found == 0u) continue;
        put16(rt, frame.at(0u), key);
        put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
        call_virtual_at(rt, ctx, self, 0x70u, {self, second, frame.at(0u), 1u, 0u, 0u});
    }
}

// func_0892F95C(self, key, value) -> (0, 0x1F) when key is 0x1F; otherwise the 64-bit entry e = func_0892F878(self, i) of
// the first i < func_0892F868(self) whose rank func_0893656C(func_089363FC(), a1, e) equals key and whose
// func_0892F890(self, i) equals value; (-1, -1) when none, or as soon as a rank is above key.
void func_0892F95C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    if (key == 0x1Fu) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0x1Fu;
        return;
    }
    GuestFrame frame(ctx, 0x30u);
    const std::int32_t count = signed_of(call_set(rt, ctx, 0x0892F868u, {}));
    for (std::int32_t index = 0; index < count; ++index) {
        const std::uint32_t low = call_set(rt, ctx, 0x0892F878u, {self, static_cast<std::uint32_t>(index)});
        const std::uint32_t high = ctx.gpr[3];
        const std::uint32_t table = call_set(rt, ctx, 0x089363FCu, {});
        const std::uint32_t rank = call_set(rt, ctx, 0x0893656Cu, {table, ctx.gpr[5], low, high});
        if (key == rank && call_set(rt, ctx, 0x0892F890u, {self, static_cast<std::uint32_t>(index)}) == value) {
            ctx.gpr[2] = low;
            ctx.gpr[3] = high;
            return;
        }
        if (key < rank) break;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}

// func_089CF6D4(a0, a1, mode, args): nothing unless mode is 3 (v0 = 3). Three value objects in the frame: o (+8) from
// func_089CD62C, p (+0xC) = func_089AB244(&args->+4), q (+0x10) = func_08969188(&args->+8); then func_089CF330(o's word,
// args->+0's virtual slot 0x3C, p's word, &q) (func_08A5BB7C(0x08CA69B8 / 0x08CA697C) when o's word / that object is 0);
// q, p and o are destroyed.
void func_089CF6D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t first = frame.at(8u);
    const std::uint32_t second = frame.at(0xCu);
    const std::uint32_t third = frame.at(0x10u);
    ctx.gpr[2] = 3u;
    ctx.gpr[4] = first;
    if (ctx.gpr[6] != 3u) return;
    call_set(rt, ctx, 0x089CD62Cu, {first});
    put(rt, frame.at(4u), word(rt, args + 4u));
    call_set(rt, ctx, 0x089AB244u, {second, frame.at(4u)});
    put(rt, frame.at(0u), word(rt, args + 8u));
    call_set(rt, ctx, 0x08969188u, {third, frame.at(0u)});
    const std::uint32_t value = word(rt, first);
    ctx.gpr[4] = 0x08CA69B8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
    const std::uint32_t object = word(rt, args);
    ctx.gpr[4] = 0x08CA697Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
    const std::uint32_t slot = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    call_set(rt, ctx, 0x089CF330u, {value, slot, word(rt, second), third});
    call_set(rt, ctx, kDestroy, {third});
    call_set(rt, ctx, kDestroy, {second});
    call_set(rt, ctx, kDestroy, {first});
}

// func_089D6284(target, a1, mode, holder): a record r is the word of a value object func_089D5DF4 fills
// (func_08A5BB7C(0x08CA6E2C) when 0). Mode 0: func_08A85B1C(temp, r->+0x2C) builds a value object that goes to target.
// Mode 1: r->+0x2C = holder->+0's virtual slot 0x3C (func_08A5BB7C(0x08CA6DE0) first when that object is 0). Other modes:
// nothing (v0 = 1).
void func_089D6284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        const std::uint32_t source = frame.at(4u);
        const std::uint32_t temporary = frame.at(8u);
        call_set(rt, ctx, 0x089D5DF4u, {source});
        const std::uint32_t record = word(rt, source);
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
        call_set(rt, ctx, 0x08A85B1Cu, {temporary, word(rt, record + 0x2Cu)});
        keep_temporary(rt, ctx, target, temporary);
        call_set(rt, ctx, kDestroy, {source});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const std::uint32_t source = frame.at(0u);
    call_set(rt, ctx, 0x089D5DF4u, {source});
    const std::uint32_t record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6DE0u});
    put(rt, record + 0x2Cu, call_virtual_at(rt, ctx, object, 0x3Cu, {object}));
    call_set(rt, ctx, kDestroy, {source});
}

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

// func_08C3DDB8(pool, node): unlinks node from the pool's doubly linked list of 0x7C-byte nodes at pool + 0xC (halfword
// links: prev +4, next +6; head pool->+0, tail pool->+2), pushes it on the free list (+8 = the old pool->+4, pool->+4 =
// its index, found as (node - (pool + 0xC)) / 4 times the inverse of 31, 0xBDEF7BDF) and counts it out of pool->+8; a tail
// jump to func_08C3DAA0(pool) when that count reaches 0. func_08C3DD14(pool, node) runs first.
void func_08C3DDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08C3DD14u, {pool, node});
    const std::uint32_t prev = half_u(rt, node + 4u);
    if (signed_of(sext16(prev)) >= 0) put16(rt, pool + sext16(prev) * 0x7Cu + 0x12u, half_u(rt, node + 6u));
    const std::uint32_t next = half_u(rt, node + 6u);
    if (signed_of(sext16(next)) >= 0) put16(rt, pool + sext16(next) * 0x7Cu + 0x10u, prev);
    const std::uint32_t index = static_cast<std::uint32_t>(signed_of(node - (pool + 0xCu)) >> 2) * 0xBDEF7BDFu;
    if (index == sext16(half_u(rt, pool))) put16(rt, pool, next);
    if (index == sext16(half_u(rt, pool + 2u))) put16(rt, pool + 2u, half_u(rt, node + 4u));
    put16(rt, node + 6u, 0xFFFFu);
    put16(rt, node + 4u, 0xFFFFu);
    put16(rt, node + 8u, half_u(rt, pool + 4u));
    put16(rt, pool + 4u, index);
    const std::uint32_t count = sext16(half_u(rt, pool + 8u) - 1u);
    put16(rt, pool + 8u, count);
    ctx.gpr[2] = count;
    if (count == 0u) tail_call(ctx, 0x08C3DAA0u, {pool});
}

// func_088AAE08(self, a1, a2): a name is formatted by func_088A0DD4(10, 0, func_088A6088(a2), buffer, 0x20) into the frame;
// func_08BAC9EC(o + 0x4C, o, buffer, 0x08CA097C, a1, 0x08BAC228, 0x10, &result) with o = self->+0 fills a shared pointer
// (+0 / +4 in the frame, cleared first), which is then released.
void func_088AAE08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t buffer = frame.at(8u);
    const std::uint32_t id = call_set(rt, ctx, 0x088A6088u, {third});
    call_set(rt, ctx, 0x088A0DD4u, {0xAu, 0u, id, buffer, 0x20u});
    const std::uint32_t owner = word(rt, self);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    call_set(rt, ctx, 0x08BAC9ECu, {owner + 0x4Cu, owner, buffer, 0x08CA097Cu, second, 0x08BAC228u, 0x10u, frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(4u));
    if (object != 0u) release_shared(rt, ctx, object);
}

// func_088D3DA0(self, holder, key, kind) -> self: func_088D2F9C(self, holder, key, kind); with s = holder->+0 + 4 (skipped
// unless func_088D4E08(s) agrees, for kinds other than 10, 13 and 14): self->+0x18 = func_088D61E0(s, key) (a float),
// func_088D390C(p, self, self + 0xC, 2 for kind 13, 1 for 14, else 6; f12 = func_088D2BC8(p, that), f13 = 0) with
// p = holder->+0->+0x61C, self->+0x1C = func_088D4E34(s, key) and self->+0x20 (byte) = 1.
void func_088D3DA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    const std::uint32_t kind = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x088D2F9Cu, {});
    const std::uint32_t base = word(rt, holder);
    const std::uint32_t player = word(rt, base + 0x61Cu);
    const std::uint32_t source = base + 4u;
    ctx.gpr[2] = self;
    if (kind != 0xAu && kind != 0xDu && kind != 0xEu && call_set(rt, ctx, 0x088D4E08u, {source}) == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    call_set(rt, ctx, 0x088D61E0u, {source, key});
    const float value = ctx.fpr[0];
    put_float(rt, self + 0x18u, value);
    call_setf(rt, ctx, 0x088D2BC8u, {player}, {value});
    const std::uint32_t mode = kind == 0xDu ? 2u : (kind != 0xEu ? 6u : 1u);
    call_setf(rt, ctx, 0x088D390Cu, {player, self, self + 0xCu, mode}, {ctx.fpr[0], 0.0f});
    const std::uint32_t result = call_set(rt, ctx, 0x088D4E34u, {source, key});
    put8(rt, self + 0x20u, 1u);
    put(rt, self + 0x1Cu, result);
    ctx.gpr[2] = self;
}

// func_08875138(holder): deletes holder->+0 (b) when set: the shared pointers of the 5-slot array at b + 0x830 (8 bytes
// each, object at +4) are released from the last down, then func_0887BEB0(b + 0x814), func_08AD0574(b + 0x3E0),
// func_08AD0700(b + 0x10) and a tail jump to func_08B64654(b) (the free).
void func_08875138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]);
    if (block == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = block + 0x830u;
    if (first != 0u) {
        for (std::uint32_t slot = block + 0x858u; slot != first;) {
            slot -= 8u;
            const std::uint32_t object = word(rt, slot + 4u);
            if (object != 0u) release_shared(rt, ctx, object);
        }
    }
    call_set(rt, ctx, 0x0887BEB0u, {block + 0x814u});
    call_set(rt, ctx, 0x08AD0574u, {block + 0x3E0u});
    call_set(rt, ctx, 0x08AD0700u, {block + 0x10u});
    tail_call(ctx, 0x08B64654u, {block});
}

// func_088DE24C(self, wait, value) -> 1 with the lock self + 0xC8 held, or 0. While self->+0x640 is set the caller must
// pass func_088E412C(wait) (0 otherwise), and func_088E4160() runs when it cleared meanwhile. The lock is taken
// (func_08B344E0) when wait, else tried (func_08B3453C; on failure func_088E4160 runs again when needed and the result
// is 0). Taken with +0x640 set again after a clear: released, and retried when wait (0 otherwise). On success the byte
// +0x5B4 records whether +0x640 was set, and +0x640 = value when value is nonzero.
void func_088DE24C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t wait = ctx.gpr[5] & 0xFFu;
    const std::uint32_t value = ctx.gpr[6];
    const std::uint32_t lock = self + 0xC8u;
    GuestFrame frame(ctx, 0x20u);
    for (;;) {
        bool busy = false;
        if (word(rt, self + 0x640u) != 0u) {
            if (call_set(rt, ctx, 0x088E412Cu, {wait}) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
            if (word(rt, self + 0x640u) != 0u) busy = true;
            else call_set(rt, ctx, 0x088E4160u, {});
        }
        if (wait != 0u) {
            call_set(rt, ctx, kLock, {lock});
        } else if (call_set(rt, ctx, 0x08B3453Cu, {lock}) == 0u) {
            if (busy) {
                call_set(rt, ctx, 0x088E4160u, {});
                ctx.gpr[2] = 0u;
            }
            return;
        }
        if (!busy && word(rt, self + 0x640u) != 0u) {
            call_set(rt, ctx, kUnlock, {lock});
            ctx.gpr[2] = 0u;
            if (wait != 0u) continue;
            return;
        }
        put8(rt, self + 0x5B4u, busy ? 1u : 0u);
        if (value != 0u) put(rt, self + 0x640u, value);
        ctx.gpr[2] = 1u;
        return;
    }
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

// func_08A78490(result, object) -> result: a string built at frame +4 (func_08AAB5DC) from the text 0x08CB0EC8, the name
// func_08A7BDB0(object's virtual slot 0x24), 0x08CB0ECC, object formatted by func_08AAB604 and 0x08CB0ED0 (appends by
// func_08AABA8C) is copied into result by func_08B88F44(result, func_08AAB540(&string)), then released.
void func_08A78490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t text = frame.at(4u);
    call_set(rt, ctx, 0x08AAB5DCu, {text});
    const std::uint32_t built = call_set(rt, ctx, 0x08AABA8Cu, {text, 0x08CB0EC8u});
    const std::uint32_t kind = call_virtual_at(rt, ctx, object, 0x24u, {object});
    const std::uint32_t name = call_set(rt, ctx, 0x08A7BDB0u, {kind});
    std::uint32_t chain = call_set(rt, ctx, 0x08AABA8Cu, {built, name});
    chain = call_set(rt, ctx, 0x08AABA8Cu, {chain, 0x08CB0ECCu});
    chain = call_set(rt, ctx, 0x08AAB604u, {chain, object});
    call_set(rt, ctx, 0x08AABA8Cu, {chain, 0x08CB0ED0u});
    const std::uint32_t characters = call_set(rt, ctx, 0x08AAB540u, {text});
    call_set(rt, ctx, 0x08B88F44u, {result, characters});
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    ctx.gpr[2] = result;
}

// func_08AFEC54(self, a1, a2, f12 = t): nothing when s = self->+0xD8->+0x60 (a float) is 0. Otherwise the 64-bit
// func_08C45E40(a1; f12 = s) goes through func_08C46718(&v, &it, high) into a vector v at frame +0x10, which is added by
// func_08C46508 to self + 0x38; a2 is added to self + 0x44; func_08C468E4(self + 0x50, func_08C45E2C(self + 0x5C,
// self + 0x44)) (64-bit); and self->+0xD4 += func_08C460C4(&v) x t.
void func_08AFEC54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    const float factor = ctx.fpr[12];
    const float scale = float_at(rt, word(rt, self + 0xD8u) + 0x60u);
    ctx.gpr[4] = first;
    ctx.fpr[12] = scale;
    if (scale == 0.0f) return;
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t vector = frame.at(0x10u);
    const std::uint32_t low = call_setf(rt, ctx, 0x08C45E40u, {first}, {scale}).v0;
    const std::uint32_t high = ctx.gpr[3];
    put(rt, frame.at(8u), low);
    put(rt, frame.at(0x20u), low);
    put(rt, frame.at(0x24u), high);
    put(rt, frame.at(0xCu), high);
    call_set(rt, ctx, 0x08C46718u, {vector, frame.at(8u), high});
    call_set(rt, ctx, 0x08C46508u, {self + 0x38u, vector});
    call_set(rt, ctx, 0x08C46508u, {self + 0x44u, second});
    const std::uint32_t low2 = call_set(rt, ctx, 0x08C45E2Cu, {self + 0x5Cu, self + 0x44u});
    const std::uint32_t high2 = ctx.gpr[3];
    put(rt, frame.at(0x20u), low2);
    put(rt, frame.at(0x24u), high2);
    put(rt, frame.at(0u), low2);
    put(rt, frame.at(4u), high2);
    call_set(rt, ctx, 0x08C468E4u, {self + 0x50u, low2, high2});
    const float before = float_at(rt, self + 0xD4u);
    call_set(rt, ctx, 0x08C460C4u, {vector});
    put_float(rt, self + 0xD4u, before + psp_mul(ctx.fpr[0], factor));
}

// func_08811D30(a0, a1, count, pair): nothing when count < 2. Otherwise o = the word of a value object func_0880F0C0
// fills (func_08A5BB7C(0x08C986F4) when 0) gives r = o->+0xE8; the virtual slot 0x3C of pair->+0 and pair->+4
// (func_08A5BB7C(0x08C986DC) when an object is 0) give x and y; func_08811684(r, r->+0x54, x, t, t, 0) when x and
// func_08811684(r, r->+0x58, y, t + 0x40, t + 0x60, 1) when y, with t = r->+0x23C.
void func_08811D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pair = ctx.gpr[7];
    if (signed_of(ctx.gpr[6]) < 2) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t source = frame.at(0u);
    call_set(rt, ctx, 0x0880F0C0u, {source});
    const std::uint32_t owner = word(rt, source);
    if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C986F4u});
    std::uint32_t object = word(rt, pair);
    const std::uint32_t record = word(rt, owner + 0xE8u);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C986DCu});
    const std::uint32_t first = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    object = word(rt, pair + 4u);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C986DCu});
    const std::uint32_t second = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    if (first != 0u) {
        const std::uint32_t table = word(rt, record + 0x23Cu);
        call_set(rt, ctx, 0x08811684u, {record, word(rt, record + 0x54u), first, table, table, 0u});
    }
    if (second != 0u) {
        const std::uint32_t table = word(rt, record + 0x23Cu);
        call_set(rt, ctx, 0x08811684u, {record, word(rt, record + 0x58u), second, table + 0x40u, table + 0x60u, 1u});
    }
    call_set(rt, ctx, kDestroy, {source});
}

// func_08A32F9C(self): pops the value object stack at self + 0x408 (end pointer at +4, 4-byte elements). When
// func_08A3233C(self) gives a name, the record (word of the value object func_08A40C24 copies from the top; func_08A5BB7C
// (0x08CACC60) when 0) gets it as a string at +0x44 (func_08B85EB8 builds it in the frame, func_08B855E0 assigns, then it
// is released). Then self's bytes +0 and +8 and word +4 are cleared and the top element is destroyed.
void func_08A32F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t stack = self + 0x408u;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t name = call_set(rt, ctx, 0x08A3233Cu, {self});
    if (name != 0u) {
        const std::uint32_t copy = frame.at(8u);
        const std::uint32_t text = frame.at(4u);
        call_set(rt, ctx, 0x08A40C24u, {copy, word(rt, stack + 4u) - 4u});
        const std::uint32_t record = word(rt, copy);
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACC60u});
        call_set(rt, ctx, 0x08B85EB8u, {text, name, frame.at(1u)});
        call_set(rt, ctx, 0x08B855E0u, {record + 0x44u, text});
        release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        call_set(rt, ctx, kDestroy, {copy});
    }
    put8(rt, self, 0u);
    put8(rt, self + 8u, 0u);
    put(rt, self + 4u, 0u);
    const std::uint32_t top = word(rt, stack + 4u) - 4u;
    put(rt, stack + 4u, top);
    call_set(rt, ctx, kDestroy, {top});
}

// A function-local static of three zero words at `object` behind the one-time guard `guard` (func_08B6441C acquires,
// func_08B64494 releases).
inline void static_zero3(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object) {
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    put(rt, object + 4u, 0u);
    put(rt, object + 8u, 0u);
    put(rt, object, 0u);
    call_set(rt, ctx, 0x08B64494u, {guard});
}

// func_08BA0D34(holder) -> holder->+0 + 4 when the type tag of holder->+0 (its virtual slot 8, or the static 0x08D75878
// when it is null) is the static tag 0x08D7583C; otherwise 0 (also for a null holder).
void func_08BA0D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    if (holder == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = word(rt, holder);
    std::uint32_t tag;
    if (object != 0u) {
        tag = call_virtual_at(rt, ctx, object, 8u, {object});
    } else {
        static_zero3(rt, ctx, 0x08D75290u, 0x08D75878u);
        tag = 0x08D75878u;
    }
    static_zero3(rt, ctx, 0x08D757C0u, 0x08D7583Cu);
    ctx.gpr[2] = tag == 0x08D7583Cu ? word(rt, holder) + 4u : 0u;
}

// The index of a 0xF0-byte node in the pool self->+4->+0 as a signed halfword: (node - base) / 16 times the inverse of
// 15 (0xEEEEEEEF), -1 for a null node.
inline std::uint32_t node_index(psprecomp::Runtime &rt, std::uint32_t self, std::uint32_t node) {
    if (node == 0u) return 0xFFFFFFFFu;
    const std::uint32_t units = static_cast<std::uint32_t>(signed_of(node - word(rt, word(rt, self + 4u))) >> 4);
    return sext16(units * 0xEEEEEEEFu);
}

// func_08C48EC4(self, source, target): the 16-byte iterator source is copied into the frame. Without a node in target
// (+0xC), func_08C48CDC(self, &copy, target, source->+0xC). Otherwise, unless target->+8 is source->+8: func_08C46DAC(
// self->+0xC, index of source->+8, self) when self->+0xC is set, then a tail jump to func_08C46DAC(target->+0xC, index of
// self->+8, self, self).
void func_08C48EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t target = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t copy = frame.at(0u);
    const std::uint32_t last = word(rt, source + 0xCu);
    const std::uint32_t node = word(rt, source + 8u);
    put(rt, copy, word(rt, source));
    put(rt, copy + 4u, word(rt, source + 4u));
    put(rt, copy + 0xCu, last);
    put(rt, copy + 8u, node);
    if (word(rt, target + 0xCu) == 0u) {
        call_set(rt, ctx, 0x08C48CDCu, {self, copy, target, last});
        return;
    }
    if (word(rt, target + 8u) == node) return;
    const std::uint32_t current = word(rt, self + 0xCu);
    if (current != 0u) call_set(rt, ctx, 0x08C46DACu, {current, node_index(rt, self, node), self});
    tail_call(ctx, 0x08C46DACu, {word(rt, target + 0xCu), node_index(rt, self, word(rt, self + 8u)), self, self});
}

// func_08841AD0(target, a1, count, holder): nothing when count <= 0. Otherwise r = func_0884177C(w) for the word w of the
// value object func_088418B4 fills (func_08A5BB7C(0x08C9C584) when 0), which is destroyed; with o = holder->+0
// (func_08A5BB7C(0x08C9C520) when 0), func_08A85B1C(temp, func_0888F374(r, o's slot 0xDC (o, 0), o's slot 0xD8)) builds
// a value object that goes to target.
void func_08841AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    if (signed_of(ctx.gpr[6]) <= 0) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x088418B4u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C584u});
    const std::uint32_t found = call_set(rt, ctx, 0x0884177Cu, {value});
    call_set(rt, ctx, kDestroy, {source});
    std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C520u});
    const std::uint32_t first = call_virtual_at(rt, ctx, object, 0xDCu, {object, 0u});
    object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C520u});
    const std::uint32_t second = call_virtual_at(rt, ctx, object, 0xD8u, {object});
    const std::uint32_t result = call_set(rt, ctx, 0x0888F374u, {found, first, second});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, result});
    keep_temporary(rt, ctx, target, temporary);
}

// func_0888EA68(self): when the byte self->+0x1D4 is set, frees what it owns: the 4 objects at +0x1A0 (func_088A7548,
// then the free func_08B64654), the 4 x 2 objects at +0x1B0 (rows of 8 bytes; func_088978B4, then the free), and
// self->+0x184 through func_08B61FC0, each slot cleared; then the byte is cleared.
void func_0888EA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (byte_u(rt, self + 0x1D4u) == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t object = word(rt, self + 0x1A0u + index * 4u);
        if (object == 0u) continue;
        call_set(rt, ctx, 0x088A7548u, {object});
        call_set(rt, ctx, 0x08B64654u, {object});
        put(rt, self + 0x1A0u + index * 4u, 0u);
    }
    for (std::uint32_t row = 0u; row < 4u; ++row) {
        for (std::uint32_t column = 0u; column < 2u; ++column) {
            const std::uint32_t slot = self + 0x1B0u + row * 8u + column * 4u;
            const std::uint32_t object = word(rt, slot);
            if (object == 0u) continue;
            call_set(rt, ctx, 0x088978B4u, {object});
            call_set(rt, ctx, 0x08B64654u, {object});
            put(rt, slot, 0u);
        }
    }
    const std::uint32_t block = word(rt, self + 0x184u);
    if (block != 0u) {
        call_set(rt, ctx, 0x08B61FC0u, {block});
        put(rt, self + 0x184u, 0u);
    }
    put8(rt, self + 0x1D4u, 0u);
}

// func_08AF8AB0(self): when func_08AF7FEC(self) agrees: func_08AF75A0(self->+0x48) and func_08AFCA04(self->+0x4C) when set;
// func_08AF8A50 on each 0x30-byte element of self->+0x38 and func_08B537BC on each 0x20-byte element of self->+0xA8 (when
// set; the count is the halfword self->+0x14, re-read each step); then bit 15 of the halfword self->+0xE is cleared.
void func_08AF8AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (call_set(rt, ctx, 0x08AF7FECu, {self}) == 0u) return;
    if (word(rt, self + 0x48u) != 0u) call_set(rt, ctx, 0x08AF75A0u, {word(rt, self + 0x48u)});
    if (word(rt, self + 0x4Cu) != 0u) call_set(rt, ctx, 0x08AFCA04u, {word(rt, self + 0x4Cu)});
    if (word(rt, self + 0x38u) != 0u) {
        std::uint32_t offset = 0u;
        for (std::uint32_t index = 0u; signed_of(index) < signed_of(half_u(rt, self + 0x14u)); ++index, offset += 0x30u) {
            call_set(rt, ctx, 0x08AF8A50u, {word(rt, self + 0x38u) + offset});
        }
    }
    if (word(rt, self + 0xA8u) != 0u) {
        for (std::uint32_t index = 0u; signed_of(index) < signed_of(half_u(rt, self + 0x14u)); ++index) {
            call_set(rt, ctx, 0x08B537BCu, {word(rt, self + 0xA8u) + (index << 5)});
        }
    }
    put16(rt, self + 0xEu, half_u(rt, self + 0xEu) & 0x7FFFu);
}

// func_0883B950(holder) -> a 64-bit value of the object o = holder->+0 (func_08A5BB7C(0x08C9BD48) when it is 0): when
// func_08A76F20(o, func_08A6BF34()) says o is of that kind, func_08931014 of the string o's virtual slot 0xC fills (the
// string is released after); otherwise o's virtual slot 0x50.
void func_0883B950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t object = word(rt, holder);
    GuestFrame frame(ctx, 0x20u);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    const std::uint32_t kind = call_set(rt, ctx, 0x08A6BF34u, {});
    const std::uint32_t matches = call_set(rt, ctx, 0x08A76F20u, {object, kind});
    const std::uint32_t current = word(rt, holder);
    if (current == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    if (matches != 0u) {
        const std::uint32_t text = frame.at(4u);
        ctx.gpr[3] = word(rt, current);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
        call_set(rt, ctx, ctx.gpr[2], {text, current});
        const std::uint32_t low = call_set(rt, ctx, 0x08931014u, {word(rt, text)});
        const std::uint32_t high = ctx.gpr[3];
        release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        ctx.gpr[2] = low;
        ctx.gpr[3] = high;
        return;
    }
    call_virtual_at(rt, ctx, current, 0x50u, {current});
}

// func_08B1C958(reader, count) -> (v0, v1): the next count bits of the bit reader {+4: byte pointer, +8: current byte,
// +0xC: bits left in it}, most significant first, as a 64-bit value; the reader is written back. A leaf.
void func_08B1C958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t reader = ctx.gpr[4];
    std::uint32_t wanted = ctx.gpr[5];
    std::uint32_t current = byte_u(rt, reader + 8u);
    std::uint32_t pointer = word(rt, reader + 4u);
    std::uint32_t left = word(rt, reader + 0xCu);
    std::uint64_t value = 0u;
    while (wanted != 0u) {
        std::uint32_t taken = wanted;
        if (left == 0u) {
            current = byte_u(rt, pointer);
            left = 8u;
            ++pointer;
        }
        if (signed_of(left) < signed_of(wanted)) {
            taken = left;
            wanted -= left;
            left = 0u;
        } else {
            left -= wanted;
            wanted = 0u;
        }
        value = (value << taken) | (current >> (8u - taken));
        current = (current << taken) & 0xFFu;
    }
    put(rt, reader + 0xCu, left);
    put(rt, reader + 4u, pointer);
    put8(rt, reader + 8u, current);
    ctx.gpr[2] = static_cast<std::uint32_t>(value);
    ctx.gpr[3] = static_cast<std::uint32_t>(value >> 32);
}

// func_08B33790(out): a one-time network bring-up (the call count at 0x08D6920C; only the first call works): until it
// settles, it polls the Sony stubs func_08C7B108 / func_08C7B128 / func_08C7B140 / func_08C7B100 / func_08C7A848 /
// func_08C7B120 in the asm's order (the flag 0x08D69210 marks the one-time func_08C7B140(1, 0x08CB54E0)); when the state
// word at frame +4 has bit 0x10 and out is set, func_08C7A848(out, frame + 8) fills it.
void func_08B33790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kCount = 0x08D6920Cu;
    constexpr std::uint32_t kStarted = 0x08D69210u;
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t count = word(rt, kCount) + 1u;
    put(rt, kCount, count);
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[4] = 1u;
    if (count != 1u) return;
    GuestFrame frame(ctx, 0x80u);
    for (;;) {
        if (call_set(rt, ctx, 0x08C7B108u, {}) == 0u && call_set(rt, ctx, 0x08C7B128u, {2u}) != 0u) continue;
        if (byte_u(rt, kStarted) == 0u) {
            if (call_set(rt, ctx, 0x08C7B140u, {1u, 0x08CB54E0u}) != 0u) continue;
            put8(rt, kStarted, 1u);
        }
        if (call_set(rt, ctx, 0x08C7B128u, {0x20u}) != 0u) continue;
        put(rt, frame.at(0u), 8u);
        if (call_set(rt, ctx, 0x08C7B100u, {frame.at(0u)}) != 0u) continue;
        if ((word(rt, frame.at(4u)) & 0x10u) != 0u) {
            if (out == 0u) return;
            if (call_set(rt, ctx, 0x08C7A848u, {out, frame.at(8u)}) == 0u) {
                if (call_set(rt, ctx, 0x08C7B120u, {}) != 0u) continue;
                return;
            }
        }
        call_set(rt, ctx, 0x08C7B128u, {4u});
    }
}

// func_08861B20(self, key, a2, a3, t0) -> r: func_08894098(o + 0x3F88, a2), func_08893C40(o + 0x3F90, a3) and
// func_088618E8(o, func_088D6310(key)) with o = self->+0; r = func_088DFB68(o->+0x5AC, key, t0, 0, 1). When r is set the
// byte o->+0x5A9 = 1, the callback block o->+0x5B8 gets {+0x20: 0x08861FD0, +0x24: 0x0885F550, +0x28: 0x08860C2C,
// +0x2C: 0x08860B3C, +0x30: self} and func_0885E7DC(self) runs.
void func_08861B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t fourth = ctx.gpr[7];
    const std::uint32_t fifth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08894098u, {word(rt, self) + 0x3F88u, third});
    call_set(rt, ctx, 0x08893C40u, {word(rt, self) + 0x3F90u, fourth});
    const std::uint32_t owner = word(rt, self);
    const std::uint32_t mapped = call_set(rt, ctx, 0x088D6310u, {key});
    call_set(rt, ctx, 0x088618E8u, {owner, mapped});
    const std::uint32_t result = call_set(rt, ctx, 0x088DFB68u, {word(rt, word(rt, self) + 0x5ACu), key, fifth, 0u, 1u});
    if (result != 0u) {
        put8(rt, word(rt, self) + 0x5A9u, 1u);
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x30u, self);
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x2Cu, 0x08860B3Cu);
        const std::uint32_t block = word(rt, word(rt, self) + 0x5B8u);
        put(rt, block + 0x24u, 0x0885F550u);
        put(rt, block + 0x20u, 0x08861FD0u);
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x28u, 0x08860C2Cu);
        call_set(rt, ctx, 0x0885E7DCu, {self});
    }
    ctx.gpr[2] = result;
}

// func_08957B30(self): with c = self->+0xC and s = self->+8, by the mode byte c->+0x140F: 2 -> s->+0x610 =
// func_08957AD4(self; f12 = s->+0x6C4); 1 -> when c->+0x1414 <= s->+0x6C4, s->+0x60E (byte) = 1 and s->+0x610 = 1.0;
// 6 -> the same with 0x3E1D89D9 (about 0.154); 8 -> s->+0x60E = 2 when c->+0x1414 <= s->+0x6C4, else 1 with s->+0x610 = 1.0
// when c->+0x1410 <= s->+0x6C4. Other modes: nothing.
void func_08957B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t config = word(rt, ctx.gpr[4] + 0xCu);
    const std::uint32_t state = word(rt, ctx.gpr[4] + 8u);
    const std::uint32_t mode = byte_u(rt, config + 0x140Fu);
    // read in the modes that use it, as the original (synthetic cases: a bad state faulted here only)
    const auto level = [&] { return float_at(rt, state + 0x6C4u); };
    GuestFrame frame(ctx, 0x10u);
    // v0 is left by the mode tests: 1 for modes 0, 1 and 6, 8 for the others (mode 8 then sets 1 unless it stores 2).
    ctx.gpr[2] = mode < 2u || mode == 6u ? 1u : 8u;
    switch (mode) {
    case 2u:
        call_setf(rt, ctx, 0x08957AD4u, {}, {level()});
        put_float(rt, state + 0x610u, ctx.fpr[0]);
        break;
    case 1u:
    case 6u:
        if (float_at(rt, config + 0x1414u) <= level()) {
            put8(rt, state + 0x60Eu, 1u);
            put(rt, state + 0x610u, mode == 1u ? 0x3F800000u : 0x3E1D89D9u);
        }
        break;
    case 8u:
        if (float_at(rt, config + 0x1414u) <= level()) {
            put8(rt, state + 0x60Eu, 2u);
            break;
        }
        ctx.gpr[2] = 1u;
        if (float_at(rt, config + 0x1410u) <= level()) {
            put8(rt, state + 0x60Eu, 1u);
            put_float(rt, state + 0x610u, 1.0f);
        }
        break;
    default: break;
    }
}

// func_08BEED1C(self, text, count) -> self: the libstdc++ copy-on-write string append(text, count). Over the maximum size
// func_08B5ADDC(0x08CA3454) reports the length error; when the capacity is short or the rep is shared the rep is
// reallocated by func_08B864EC(self, new length) (text keeps its offset when it points into the old buffer); the count
// bytes are copied (one inline, otherwise func_08B79A88), and the rep's count and length are updated and the string ends
// with the terminator byte at 0x08CB7430.
void func_08BEED1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t text = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    ctx.gpr[2] = self;
    if (count == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t data = word(rt, self);
    const std::uint32_t length = word(rt, data - 0xCu);
    std::uint32_t capacity;
    if (0x3FFFFFFCu - length < count) capacity = call_set(rt, ctx, 0x08B5ADDCu, {0x08CA3454u});
    else capacity = word(rt, data - 8u);
    const std::uint32_t total = count + length;
    if (capacity < total || signed_of(word(rt, data - 4u)) > 0) {
        if (text < data || data + length < text) {
            call_set(rt, ctx, 0x08B864ECu, {self, total});
            data = word(rt, self);
        } else {
            const std::uint32_t offset = text - data;
            call_set(rt, ctx, 0x08B864ECu, {self, total});
            data = word(rt, self);
            text = data + offset;
        }
    }
    const std::uint32_t end = data + word(rt, data - 0xCu);
    if (count == 1u) put8(rt, end, byte_u(rt, text));
    else call_set(rt, ctx, 0x08B79A88u, {end, text, count});
    const std::uint32_t current = word(rt, self);
    put(rt, current - 4u, 0u);
    put(rt, current - 0xCu, total);
    put8(rt, current + total, byte_u(rt, 0x08CB7430u));
    ctx.gpr[2] = self;
}

// A C++ pointer to member function as this compiler lays it out: the function (or vtable offset) and the adjustment << 1,
// bit 0 set when the function is virtual. Returns the function to call for `object`, and adjusts `object`.
inline std::uint32_t member_function(psprecomp::Runtime &rt, std::uint32_t &object, std::uint32_t function, std::uint32_t adjustment) {
    object += static_cast<std::uint32_t>(signed_of(adjustment) >> 1);
    if ((adjustment & 1u) != 0u) return word(rt, word(rt, object) + function);
    return function;
}

// The shared body of func_08BF2D6C / func_08C158D8 (self, objects, value): p = the member function (self->+0x14,
// self->+0x18) called on objects->+0; then the member function (self->+0x24, self->+0x28) is called on p with a string
// built from value by func_08BE9070 in the frame, which is released after.
inline void forward_as_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[6];
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t first = object;
    const std::uint32_t getter = member_function(rt, first, word(rt, self + 0x14u), word(rt, self + 0x18u));
    ctx.gpr[2] = getter;
    const std::uint32_t part = call_set(rt, ctx, getter, {first, object, word(rt, self + 0x14u), word(rt, self + 0x18u)});
    std::uint32_t second = part;
    const std::uint32_t setter = member_function(rt, second, word(rt, self + 0x24u), word(rt, self + 0x28u));
    const std::uint32_t text = frame.at(4u);
    call_set(rt, ctx, 0x08BE9070u, {text, value});
    call_set(rt, ctx, setter, {second, text});
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// func_08BF2D6C(self, objects, value): forward_as_string.
void func_08BF2D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_as_string(rt, ctx); }

// func_08C158D8(self, objects, value): forward_as_string (a second copy of the same code).
void func_08C158D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_as_string(rt, ctx); }

// The shared body of a script property of the object func_08846E50(w) finds, w being the word of the value object
// func_08847008 fills (func_08A5BB7C(0x08C9CBBC) when 0; destroyed after). With count > 0 the setter gets holder->+0's
// virtual slot 0x3C (func_08A5BB7C(0x08C9CB80) when that object is 0), as 0/1 when `as_flag`; otherwise
// func_08A85B1C(temp, getter(object)) builds a value object that goes to target.
inline void script_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t setter, bool as_flag, std::uint32_t getter) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    const bool set = signed_of(ctx.gpr[6]) > 0;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t source = frame.at(set ? 8u : 4u);
    call_set(rt, ctx, 0x08847008u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
    const std::uint32_t object = call_set(rt, ctx, 0x08846E50u, {value});
    call_set(rt, ctx, kDestroy, {source});
    if (set) {
        const std::uint32_t argument = word(rt, holder);
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
        const std::uint32_t slot = call_virtual_at(rt, ctx, argument, 0x3Cu, {argument});
        call_set(rt, ctx, setter, {object, as_flag ? (slot != 0u ? 1u : 0u) : slot});
        return;
    }
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, call_set(rt, ctx, getter, {object})});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08849D4C(target, a1, count, holder): script_property with func_088D4C78 / func_088D4C6C.
void func_08849D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { script_property(rt, ctx, 0x088D4C78u, false, 0x088D4C6Cu); }

// func_0884A29C(target, a1, count, holder): script_property with func_088D4CD0 / func_088D4CDC.
void func_0884A29C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { script_property(rt, ctx, 0x088D4CD0u, false, 0x088D4CDCu); }

// func_0884A3AC(target, a1, count, holder): script_property with func_088D4CAC (a 0/1 flag) / func_088D4C9C.
void func_0884A3AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { script_property(rt, ctx, 0x088D4CACu, true, 0x088D4C9Cu); }

// func_0892063C(self): with K = the static float 0x08CF289C (set once to 0x3F860A92, about pi / 3, behind the guard
// 0x08CF2888) and o = func_089260EC(self->+8): a tail jump to func_089202A0(o; f12 = o's virtual slot 0x10, f13 = -K,
// f14 = -K when o's virtual slot 8 answers for 0x10, else K when it answers for 8, else 0; f15 = o's virtual slot 0x14).
void func_0892063C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kGuard = 0x08CF2888u;
    constexpr std::uint32_t kAngle = 0x08CF289Cu;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    if (sext8(byte_u(rt, kGuard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
        put(rt, kAngle, 0x3F860A92u);
        call_set(rt, ctx, 0x08B64494u, {kGuard});
    }
    const std::uint32_t object = call_set(rt, ctx, 0x089260ECu, {word(rt, self + 8u)});
    float turn = 0.0f;
    if (call_virtual_at(rt, ctx, object, 8u, {object, 8u}) != 0u) turn = float_at(rt, kAngle);
    if (call_virtual_at(rt, ctx, object, 8u, {object, 0x10u}) != 0u) turn = -float_at(rt, kAngle);
    const float angle = float_at(rt, kAngle);
    call_virtual_at(rt, ctx, object, 0x10u, {object});
    const float first = ctx.fpr[0];
    call_virtual_at(rt, ctx, object, 0x14u, {object});
    ctx.fpr[15] = ctx.fpr[0];
    ctx.fpr[12] = first;
    ctx.fpr[13] = -angle;
    ctx.fpr[14] = turn;
    tail_call(ctx, 0x089202A0u, {object});
}

// func_088451C0(target, a1, count, holder): nothing when count <= 0. Otherwise r = func_08844634(w) for the word w of the
// value object func_088447E8 fills (func_08A5BB7C(0x08C9C948) when 0; destroyed after); holder->+0's virtual slot 0xC
// (func_08A5BB7C(0x08C9C908) first when that object is 0) fills a string, and func_08A85B1C(temp, func_08893B0C(r, the
// string's characters)) builds a value object that goes to target; the string is released last.
void func_088451C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    if (signed_of(ctx.gpr[6]) <= 0) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(4u);
    const std::uint32_t source = frame.at(8u);
    const std::uint32_t text = frame.at(0xCu);
    call_set(rt, ctx, 0x088447E8u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C948u});
    const std::uint32_t found = call_set(rt, ctx, 0x08844634u, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C908u});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {text, object});
    const std::uint32_t index = call_set(rt, ctx, 0x08893B0Cu, {found, word(rt, text)});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, index});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// func_0888BC00(self): by self->+0x1C: 2 -> func_08891560(self->+0x14), 1 -> func_08891EF8(self->+0x14, 0); then
// func_089178AC(self->+8). Unless func_088D4C50(self->+4 + 4) says so, each object func_088DA588(self->+4, i) gives for
// i < func_088D9F80(self->+4) gets its virtual slots 0x1C (o, 1) and 0x18 (o, 1, 50). Then func_088936A4(self->+0x18,
// message {5}) and the callback self->+0x24 (when set) with self->+0x30.
void func_0888BC00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0xC0u);
    const std::uint32_t mode = word(rt, self + 0x1Cu);
    if (mode == 2u) call_set(rt, ctx, 0x08891560u, {word(rt, self + 0x14u)});
    else if (mode == 1u) call_set(rt, ctx, 0x08891EF8u, {word(rt, self + 0x14u), 0u});
    call_set(rt, ctx, 0x089178ACu, {word(rt, self + 8u)});
    if (call_set(rt, ctx, 0x088D4C50u, {word(rt, self + 4u) + 4u}) == 0u) {
        const std::int32_t count = signed_of(call_set(rt, ctx, 0x088D9F80u, {word(rt, self + 4u)}));
        for (std::int32_t index = 0; index < count; ++index) {
            const std::uint32_t object = call_set(rt, ctx, 0x088DA588u, {word(rt, self + 4u), static_cast<std::uint32_t>(index)});
            ctx.gpr[5] = 1u;
            if (object == 0u) continue;
            ctx.gpr[2] = word(rt, object);
            ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x1Cu);
            call_set(rt, ctx, ctx.gpr[3], {object, 1u});
            const std::uint32_t table = word(rt, object);
            ctx.gpr[2] = word(rt, table + 0x18u);
            call_set(rt, ctx, ctx.gpr[2], {object, 1u, 0x32u});
        }
    }
    put(rt, frame.at(0u), 5u);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
    const std::uint32_t callback = word(rt, self + 0x24u);
    if (callback != 0u) call_set(rt, ctx, callback, {word(rt, self + 0x30u)});
}

// func_088D06AC(dst, src, flag) -> src + func_088D06A4(...): copies the 0x1CC-byte record word by word (src may be
// unaligned); when flag & 0xFF is 1 the copy is reset: +0x78 = 0x481907FF (no time), +0x10 = 0, +0x14 = +0x18 = 1.
// func_088D06A4 is entered with the registers the copy leaves (a0 = src->+0x1C4, a1 = src->+0x1BC, a2 = the flag,
// a3 = src + 0x1C0).
void func_088D06AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t offset = 0u; offset < 0x1CCu; offset += 4u) {
        const std::uint32_t value = byte_u(rt, src + offset) | byte_u(rt, src + offset + 1u) << 8 | byte_u(rt, src + offset + 2u) << 16 |
                                    byte_u(rt, src + offset + 3u) << 24;
        put(rt, dst + offset, value);
    }
    ctx.gpr[8] = dst + 0x1C0u;
    ctx.gpr[9] = src + 0x1C0u;
    ctx.gpr[10] = dst;
    if (flag == 1u) {
        put(rt, dst + 0x78u, 0x481907FFu);
        put(rt, dst + 0x18u, flag);
        put(rt, dst + 0x10u, 0u);
        put(rt, dst + 0x14u, flag);
    }
    const std::uint32_t size = call_set(rt, ctx, 0x088D06A4u, {word(rt, dst + 0x1C4u), word(rt, dst + 0x1BCu), flag, src + 0x1C0u});
    ctx.gpr[2] = src + size;
}

// func_08832FC4(target): e = func_08893768(func_0885EE58(func_088294D0(w))) for the word w of the value object
// func_088297A8 fills (func_08A5BB7C(0x08C9A2F8) when 0; destroyed after); an array value object of n = e->+4 (signed
// halfword) elements is made by func_08A98C1C in the frame, element i set from func_08A85B1C(temp, the signed halfword
// e->+(0x558 + 2i)); the array goes to target.
void func_08832FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    const std::uint32_t array = frame.at(8u);
    call_set(rt, ctx, 0x088297A8u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t found = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t owner = call_set(rt, ctx, 0x0885EE58u, {found});
    const std::uint32_t entry = call_set(rt, ctx, 0x08893768u, {owner});
    const std::uint32_t count = sext16(half_u(rt, entry + 4u));
    call_set(rt, ctx, 0x08A98C1Cu, {array, count});
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(count); ++index) {
        const std::uint32_t element = word(rt, word(rt, array) + 0x14u) + index * 4u;
        call_set(rt, ctx, 0x08A85B1Cu, {temporary, sext16(half_u(rt, entry + 0x558u + index * 2u))});
        keep_temporary(rt, ctx, element, temporary);
    }
    keep_temporary(rt, ctx, target, array);
}

// func_088697E8(self): a destructor body: func_08869D70(self + 0x1B0), func_0887B1EC(self + 0x74), func_0886A2EC(self + 0x1B0);
// the member at +0x1A0 gets the vtable 0x08CD70F8, func_08B8E4D0(+0x1A4, +0x1A8, frame), its buffer +0x1A4 freed by
// func_08B35254, then the vtable 0x08CD7118; func_0890B62C(self + 0x90); the holder at +0x80 / +0x84 released through the
// object's virtual slot 0; func_0887AD7C(self + 0x74), func_08B28BE0(self + 0x48), func_08B9355C(self + 0x30, self->+0x38);
// the weak count (+8) of self->+0x2C steps down (virtual slot 0xC on the last); func_08B28DC4(self).
void func_088697E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08869D70u, {self + 0x1B0u});
    call_set(rt, ctx, 0x0887B1ECu, {self + 0x74u});
    call_set(rt, ctx, 0x0886A2ECu, {self + 0x1B0u});
    const std::uint32_t buffer = word(rt, self + 0x1A4u);
    const std::uint32_t size = word(rt, self + 0x1A8u);
    put(rt, self + 0x1A0u, 0x08CD70F8u);
    call_set(rt, ctx, 0x08B8E4D0u, {buffer, size, frame.at(0u)});
    if (word(rt, self + 0x1A4u) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, self + 0x1A4u)});
    put(rt, self + 0x1A0u, 0x08CD7118u);
    call_set(rt, ctx, 0x0890B62Cu, {self + 0x90u});
    const std::uint32_t object = word(rt, self + 0x84u);
    if (object != 0u) {
        call_virtual_at(rt, ctx, object, 0u, {object, word(rt, self + 0x80u)});
        put(rt, self + 0x84u, 0u);
        put(rt, self + 0x80u, 0u);
    }
    call_set(rt, ctx, 0x0887AD7Cu, {self + 0x74u});
    call_set(rt, ctx, 0x08B28BE0u, {self + 0x48u});
    call_set(rt, ctx, 0x08B9355Cu, {self + 0x30u, word(rt, self + 0x38u)});
    const std::uint32_t shared = word(rt, self + 0x2Cu);
    if (shared != 0u) {
        const std::uint32_t old = word(rt, shared + 8u);
        put(rt, shared + 8u, old - 1u);
        ctx.gpr[3] = shared + 8u;
        ctx.gpr[5] = old;
        ctx.gpr[2] = 1u;
        if (old == 1u) {
            ctx.gpr[3] = word(rt, shared);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
            call_set(rt, ctx, ctx.gpr[2], {shared});
        }
    }
    call_set(rt, ctx, 0x08B28DC4u, {self});
}

// The MIPS `wsbh`: the bytes of each halfword swapped.
inline std::uint32_t swap_halves(std::uint32_t value) { return ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu); }

// func_088A6FB4(self): with o = self->+0: the 64-bit e = func_08932558(o + 0x38 + 0x180 x o->+8 (signed byte));
// func_088A61A4(self); func_088A5A48(self, &x, &y) fills two frame words. o->+0x4BA = trunc(x x 10) and o->+0x4BC =
// trunc(func_088A5AE4(self, that) x 10000) (byte-swapped halfwords; func_088A5A0C(self, that) after); o->+0x4BE = the
// byte-swapped x word when func_08930EBC(e, &y, &x) succeeds, else 0xFFFF.
void func_088A6FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(4u);
    const std::uint32_t second = frame.at(0u);
    const std::uint32_t owner = word(rt, self);
    const std::uint32_t slot = sext8(byte_u(rt, owner + 8u));
    const std::uint32_t low = call_set(rt, ctx, 0x08932558u, {(slot << 9) - (slot << 7) + owner + 0x38u});
    const std::uint32_t high = ctx.gpr[3];
    call_set(rt, ctx, 0x088A61A4u, {self});
    call_set(rt, ctx, 0x088A5A48u, {self, first, second});
    // f0 keeps x x 10 across the call: the asm scales whatever f0 holds when func_088A5AE4 returns.
    ctx.fpr[0] = psp_mul(float_at(rt, first), 10.0f);
    const std::uint32_t tenths = ctx.fpu_float_to_word(ctx.fpr[0], 1u);
    ctx.fpr[1] = std::bit_cast<float>(tenths);
    put16(rt, word(rt, self) + 0x4BAu, swap_halves(tenths & 0xFFFFu));
    call_set(rt, ctx, 0x088A5AE4u, {self, tenths});
    ctx.fpr[0] = psp_mul(ctx.fpr[0], 10000.0f);
    const std::uint32_t scaled = ctx.fpu_float_to_word(ctx.fpr[0], 1u);
    ctx.fpr[1] = std::bit_cast<float>(scaled);
    put16(rt, word(rt, self) + 0x4BCu, swap_halves(scaled & 0xFFFFu));
    call_set(rt, ctx, 0x088A5A0Cu, {self, scaled});
    if (call_set(rt, ctx, 0x08930EBCu, {low, high, second, first}) != 0u) put16(rt, word(rt, self) + 0x4BEu, swap_halves(word(rt, first)));
    else put16(rt, word(rt, self) + 0x4BEu, 0xFFFFu);
}

// func_08A28884(target): a name chosen by func_08ABD974() (asked again for each test, compared by func_08B35FD8):
// 0x08CAB8BC for 0x08CAB898 or 0x08CAB89C, 0x08CAB8C4 for 0x08CAB8A0, otherwise func_08ABDE0C(); a string built from it
// (func_08B85EB8) gives func_08A6D2D4(temp, &string), a value object that goes to target; the string is released last.
void func_08A28884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t text = frame.at(4u);
    const std::uint32_t temporary = frame.at(8u);
    const auto is = [&](std::uint32_t name) {
        const std::uint32_t current = call_set(rt, ctx, 0x08ABD974u, {});
        return call_set(rt, ctx, 0x08B35FD8u, {current, name}) == 0u;
    };
    std::uint32_t name;
    if (is(0x08CAB898u) || is(0x08CAB89Cu)) name = 0x08CAB8BCu;
    else if (is(0x08CAB8A0u)) name = 0x08CAB8C4u;
    else name = call_set(rt, ctx, 0x08ABDE0Cu, {});
    call_set(rt, ctx, 0x08B85EB8u, {text, name, frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {temporary, text});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// func_08ACD2EC(self, flag): over the self->+0x32C slots of 0xF0 bytes, each under its lock at +0x4A0: the byte +0x50C is
// set to 1 when flag & 0xFF, otherwise cleared, and then each slot's event at +0x448 is signalled by func_08B28C74.
void func_08ACD2EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    if (signed_of(word(rt, self + 0x32Cu)) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(word(rt, self + 0x32Cu)); ++index) {
        const std::uint32_t lock = self + 0x4A0u + index * 0xF0u;
        call_set(rt, ctx, kLock, {lock});
        put8(rt, self + 0x50Cu + index * 0xF0u, flag != 0u ? 1u : 0u);
        call_set(rt, ctx, kUnlock, {lock});
    }
    if (flag != 0u) return;
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(word(rt, self + 0x32Cu)); ++index) {
        call_set(rt, ctx, 0x08B28C74u, {self + 0x448u + index * 0xF0u});
    }
}

// func_0882AA38(a0, key, count, args): nothing when count < 2. With w the word of the value object func_088297A8(temp, key)
// fills (func_08A5BB7C(0x08C9A2F8) when 0; destroyed after): func_0885F838(func_088294D0(w), slot(args->+0),
// slot(args->+4), count >= 3 ? slot(args->+8) : 0), slot being an object's virtual slot 0x3C (func_08A5BB7C(0x08C9A2B4)
// first when the object is 0; the third is asked first).
void func_0882AA38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    if (signed_of(count) < 2) {
        ctx.gpr[2] = 1u;  // the original's v0 there: its `slti v0, a2, 2` (synthetic cases)
        return;
    }
    GuestFrame frame(ctx, 0x30u);
    const auto slot = [&](std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        return call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    };
    const std::uint32_t third = signed_of(count) < 3 ? 0u : slot(args + 8u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x088297A8u, {temporary, key});
    const std::uint32_t value = word(rt, temporary);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t found = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {temporary});
    const std::uint32_t first = slot(args);
    const std::uint32_t second = slot(args + 4u);
    call_set(rt, ctx, 0x0885F838u, {found, first, second, third});
}

// func_0885F550(self) -> a tail jump to func_088D4D24(o->+0x3F50->+0x7C), o = self->+0: first func_0885F46C(self) when the
// byte o->+0x3FDB is set; o->+0x5B8->+0x34 and o->+0x3FD4 are cleared; when the byte d->+0x26 of d = o->+0x5AC is set, each
// of the 8 records of 24 bytes at o + 0x3E90 has +9 (byte) = -1 and, by its signed key byte +8: none (-1) -> +0xA counts up;
// else +0x12 counts up, +9 = func_088DB118(d, key) (signed byte) and +0x10 counts up when that is 0.
void func_0885F550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    if (byte_u(rt, word(rt, self) + 0x3FDBu) != 0u) call_set(rt, ctx, 0x0885F46Cu, {self});
    put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x34u, 0u);
    put(rt, word(rt, self) + 0x3FD4u, 0u);
    const std::uint32_t device = word(rt, word(rt, self) + 0x5ACu);
    if (byte_u(rt, device + 0x26u) == 0u) return;
    for (std::uint32_t index = 0u; index < 8u; ++index) {
        const std::uint32_t record = word(rt, self) + 0x3E90u + index * 24u;
        const std::uint32_t key = sext8(byte_u(rt, record + 8u));
        put8(rt, record + 9u, 0xFFu);
        if (key == 0xFFFFFFFFu) {
            put16(rt, record + 0xAu, half_u(rt, record + 0xAu) + 1u);
            continue;
        }
        put16(rt, record + 0x12u, half_u(rt, record + 0x12u) + 1u);
        const std::uint32_t slot = sext8(call_set(rt, ctx, 0x088DB118u, {device, key}));
        put8(rt, record + 9u, slot);
        if (slot == 0u) put16(rt, record + 0x10u, half_u(rt, record + 0x10u) + 1u);
    }
    tail_call(ctx, 0x088D4D24u, {word(rt, word(rt, word(rt, self) + 0x3F50u) + 0x7Cu)});
}

// func_0888EC10(holder): holder->+0 = a new 0x1D8-byte block (func_08B62344) set up by func_088D65DC(+0x188),
// func_08894444(+0x190) and func_08893F7C(+0x198), with +0 = 0, +8..+0x17F zeroed (func_08B36018), +0x180, +0x184, the 4
// words at +0x1A0 and the 4 x 2 at +0x1B0 cleared, +0x1D0 = -1 and the byte +0x1D4 = 0; holder->+4 = the static object
// 0x08D75E90 (its vtable 0x08CC1D18 set once behind the guard 0x08D75E88).
void func_0888EC10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kGuard = 0x08D75E88u;
    constexpr std::uint32_t kStatic = 0x08D75E90u;
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x1D8u});
    call_set(rt, ctx, 0x088D65DCu, {block + 0x188u});
    call_set(rt, ctx, 0x08894444u, {block + 0x190u});
    call_set(rt, ctx, 0x08893F7Cu, {block + 0x198u});
    put(rt, block, 0u);
    call_set(rt, ctx, 0x08B36018u, {block + 8u, 0u, 0x178u});
    put(rt, block + 0x180u, 0u);
    put(rt, block + 0x184u, 0u);
    for (std::uint32_t index = 0u; index < 4u; ++index) put(rt, block + 0x1A0u + index * 4u, 0u);
    for (std::uint32_t row = 0u; row < 4u; ++row) {
        for (std::uint32_t column = 0u; column < 2u; ++column) put(rt, block + 0x1B0u + row * 8u + column * 4u, 0u);
    }
    put(rt, block + 0x1D0u, 0xFFFFFFFFu);
    put8(rt, block + 0x1D4u, 0u);
    put(rt, holder, block);
    if (sext8(byte_u(rt, kGuard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
        put(rt, kStatic, 0x08CC1D18u);
        call_set(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, holder + 4u, kStatic);
}

// func_08B8A4C0(self, text, count) -> self: the libstdc++ copy-on-write string assign(text, count). Over the maximum size
// func_08B5ADDC(0x08C99400) reports the length error. When text lies outside the buffer or the rep is shared, a tail
// jump to func_08B8A43C(self, 0, length, text, count) (the general replace); otherwise in place: count bytes moved from
// text (one inline; func_08B79A88 when they cannot overlap, func_08B79BAC when they can; nothing when text is the start),
// then the rep's count and length are set and the terminator byte at 0x08CB7430 written.
void func_08B8A4C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (0x3FFFFFFCu < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08C99400u});
    const std::uint32_t data = word(rt, self);
    const std::uint32_t length = word(rt, data - 0xCu);
    if (text < data || data + length < text || signed_of(word(rt, data - 4u)) > 0) {
        return tail_call(ctx, 0x08B8A43Cu, {self, 0u, word(rt, data - 0xCu), text, count});
    }
    const std::uint32_t offset = text - data;
    if (!(offset < count)) {
        if (count == 1u) put8(rt, data, byte_u(rt, text));
        else call_set(rt, ctx, 0x08B79A88u, {data, text, count});
    } else if (offset != 0u) {
        if (count == 1u) put8(rt, data, byte_u(rt, text));
        else call_set(rt, ctx, 0x08B79BACu, {data, text, count});
    }
    const std::uint32_t current = word(rt, self);
    put(rt, current - 4u, 0u);
    put(rt, current - 0xCu, count);
    put8(rt, current + count, byte_u(rt, 0x08CB7430u));
    ctx.gpr[2] = self;
}

// func_08849848(target, a1, count, holder): the script property of the object func_08846E50(w) finds (w as in
// script_property): with count > 0, func_088D5A70(object, holder->+0's virtual slot 0x3C) and then
// func_088D5A8C(object, func_088D5DA8(object)); otherwise func_08A85B1C(temp, func_088D5DF8(object)) goes to target.
void func_08849848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    const bool set = signed_of(ctx.gpr[6]) > 0;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t source = frame.at(set ? 8u : 4u);
    call_set(rt, ctx, 0x08847008u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
    const std::uint32_t object = call_set(rt, ctx, 0x08846E50u, {value});
    call_set(rt, ctx, kDestroy, {source});
    if (set) {
        const std::uint32_t argument = word(rt, holder);
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
        call_set(rt, ctx, 0x088D5A70u, {object, call_virtual_at(rt, ctx, argument, 0x3Cu, {argument})});
        call_set(rt, ctx, 0x088D5A8Cu, {object, call_set(rt, ctx, 0x088D5DA8u, {object})});
        return;
    }
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, call_set(rt, ctx, 0x088D5DF8u, {object})});
    keep_temporary(rt, ctx, target, temporary);
}

// func_089EE2E4(self): func_089E0288(self); then for each of the two strings self->+0x238 / +0x23C that is not empty,
// unless the object at +0x240 / +0x244 is set and its virtual slot 0x28 agrees, that object is rebuilt:
// func_089A50A0(temp, func_089BD4DC(self), &string, 0, 0, 1) makes a value object that goes there.
void func_089EE2E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x089E0288u, {self});
    for (std::uint32_t part = 0u; part < 2u; ++part) {
        const std::uint32_t text = self + 0x238u + part * 4u;
        const std::uint32_t slot = self + 0x240u + part * 4u;
        ctx.gpr[2] = word(rt, word(rt, text) - 0xCu);
        if (ctx.gpr[2] == 0u) continue;
        const std::uint32_t object = word(rt, slot);
        if (object != 0u && call_virtual_at(rt, ctx, object, 0x28u, {object}) != 0u) continue;
        const std::uint32_t temporary = frame.at(part == 0u ? 4u : 0u);
        const std::uint32_t owner = call_set(rt, ctx, 0x089BD4DCu, {self});
        call_set(rt, ctx, 0x089A50A0u, {temporary, owner, text, 0u, 0u, 1u});
        keep_temporary(rt, ctx, slot, temporary);
    }
}

// func_08B2EEB8(state): by state->+0: 1 -> when state->+8 is set, a path is built in a 0x100-byte frame buffer
// (func_08B23928 with 0x08CB5488, func_08B23A70 with state->+8, func_08B1CD8C with 0x08C93A74) and copied down
// (func_08B0EB48); the Sony stub func_08C7A900(path, 0, 0) is retried (func_08B2E064 between tries) until it gives a
// positive handle, kept at state->+0x10 and passed to func_08C7A8E8(handle, 0, 0, 0, 0); the byte state->+4 = 1.
// 2 -> r = func_08C7B168(state->+0xC); the byte state->+4 = 1 when r >= 0 (0x80111102 and 0x80020139 leave it).
void func_08B2EEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const std::uint32_t mode = word(rt, state);
    if (mode != 1u && mode != 2u) return;
    GuestFrame frame(ctx, 0x220u);
    if (mode == 2u) {
        const std::uint32_t result = call_set(rt, ctx, 0x08C7B168u, {word(rt, state + 0xCu)});
        if (result == 0x80111102u || result == 0x80020139u) return;
        if (signed_of(result) >= 0) put8(rt, state + 4u, 1u);
        return;
    }
    if (word(rt, state + 8u) == 0u) return;
    const std::uint32_t builder = frame.at(0x100u);
    put(rt, builder + 4u, 0x100u);
    put(rt, builder, frame.at(0x10Cu));
    put(rt, builder + 8u, 0u);
    put8(rt, frame.at(0x10Cu), 0u);
    call_set(rt, ctx, 0x08B23928u, {builder, 0x08CB5488u});
    call_set(rt, ctx, 0x08B23A70u, {builder, word(rt, state + 8u)});
    call_set(rt, ctx, 0x08B1CD8Cu, {builder, 0x08C93A74u});
    call_set(rt, ctx, 0x08B0EB48u, {word(rt, builder), frame.at(0u), 0x100u});
    std::uint32_t handle;
    while (signed_of(handle = call_set(rt, ctx, 0x08C7A900u, {frame.at(0u), 0u, 0u})) <= 0) call_set(rt, ctx, 0x08B2E064u, {handle});
    put(rt, state + 0x10u, handle);
    call_set(rt, ctx, 0x08C7A8E8u, {handle, 0u, 0u, 0u, 0u});
    put8(rt, state + 4u, 1u);
}

// func_0883074C(target, a1, count, args): nothing when count <= 0. With r = func_088294D0(w) for the word w of the value
// object func_088297A8 fills (func_08A5BB7C(0x08C9A2F8) when 0; destroyed after) and i = args->+0's virtual slot 0x3C
// (nothing when negative): e = func_0885EC28(r, i); when e is set func_08857EE4(temp, e, count >= 2 ? args->+4's virtual
// slot 0x30 : 0) builds a value object that goes to target (func_08A5BB7C(0x08C9A2B4) first for a null argument object).
void func_0883074C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    if (signed_of(count) <= 0) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x088297A8u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t found = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {source});
    std::uint32_t object = word(rt, args);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const std::uint32_t index = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    if (signed_of(index) < 0) return;
    std::uint32_t extra = 0u;
    if (!(signed_of(count) < 2)) {
        object = word(rt, args + 4u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        extra = call_virtual_at(rt, ctx, object, 0x30u, {object});
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x0885EC28u, {found, index});
    if (entry == 0u) return;
    call_set(rt, ctx, 0x08857EE4u, {temporary, entry, extra});
    keep_temporary(rt, ctx, target, temporary);
}

// func_0880F6B0(self, other): sets bit 0x4000 of self->+0x15C->+0xE0; the objects +0x18C, +0x190 and the three at +0x180
// get their virtual slot 0x1A8 (o, 1) and func_08A0F0CC(o); func_089DB5CC(self->+0xD0 / +0xD4, self->+0x23C); bit 0x4000
// of +0xE0 is cleared on the 10 objects at +0xDC; self->+0x26C = other->+0x8C; then a tail jump to
// func_08ABC4C4(0x08C9876C, 0x7FFFFFFF; f12 = 0, f13 = 1.0, f14 = self->+0x2D4).
void func_0880F6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t flags = word(rt, self + 0x15Cu);
    put(rt, flags + 0xE0u, word(rt, flags + 0xE0u) | 0x4000u);
    const auto refresh = [&](std::uint32_t slot) {
        const std::uint32_t object = word(rt, slot);
        call_virtual_at(rt, ctx, object, 0x1A8u, {object, 1u});
        call_set(rt, ctx, 0x08A0F0CCu, {word(rt, slot)});
    };
    refresh(self + 0x18Cu);
    refresh(self + 0x190u);
    const std::uint32_t second = word(rt, self + 0xD4u);
    call_set(rt, ctx, 0x089DB5CCu, {word(rt, self + 0xD0u), word(rt, self + 0x23Cu)});
    call_set(rt, ctx, 0x089DB5CCu, {second, word(rt, self + 0x23Cu)});
    for (std::uint32_t index = 0u; index < 10u; ++index) {
        const std::uint32_t object = word(rt, self + 0xDCu + index * 4u);
        put(rt, object + 0xE0u, word(rt, object + 0xE0u) & ~0x4000u);
    }
    for (std::uint32_t index = 0u; index < 3u; ++index) refresh(self + 0x180u + index * 4u);
    const float scale = float_at(rt, self + 0x2D4u);
    put(rt, self + 0x26Cu, word(rt, other + 0x8Cu));
    ctx.fpr[12] = 0.0f;
    ctx.fpr[13] = 1.0f;
    ctx.fpr[14] = scale;
    tail_call(ctx, 0x08ABC4C4u, {0x08C9876Cu, 0x7FFFFFFFu});
}

// func_08AD31E0(self): resets a state block: func_08AD16C0(self), func_08AD1098(self + 0x1D0); +0x260 = 50.0, the floats
// +0x250..+0x25C, +0x264 and +0x268 = 1.0, the byte +0x27A = 1 and +0x26C..+0x279 cleared; func_08AD0FAC(self + 0x27C);
// +0x29C = a new 0xA0-byte block (func_08B62344) set up by func_08AD6AB8(block, 0x80, 0x18); +0x2A0 = the static object
// 0x08D77268 (vtable 0x08CD6CF0 set once behind the guard 0x08D77260); func_08C3DECC(self + 0x2A4),
// func_08B28DCC(self + 0x40B0); +0x240 = 1.0.
void func_08AD31E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kGuard = 0x08D77260u;
    constexpr std::uint32_t kStatic = 0x08D77268u;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08AD16C0u, {self});
    call_set(rt, ctx, 0x08AD1098u, {self + 0x1D0u});
    put_float(rt, self + 0x260u, 50.0f);
    for (const std::uint32_t offset : {0x268u, 0x250u, 0x254u, 0x258u, 0x25Cu, 0x264u}) put_float(rt, self + offset, 1.0f);
    put8(rt, self + 0x27Au, 1u);
    put(rt, self + 0x26Cu, 0u);
    for (std::uint32_t offset = 0x270u; offset <= 0x276u; offset += 2u) put16(rt, self + offset, 0u);
    put8(rt, self + 0x278u, 0u);
    put8(rt, self + 0x279u, 0u);
    call_set(rt, ctx, 0x08AD0FACu, {self + 0x27Cu});
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0xA0u});
    call_set(rt, ctx, 0x08AD6AB8u, {block, 0x80u, 0x18u});
    put(rt, self + 0x29Cu, block);
    if (sext8(byte_u(rt, kGuard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
        put(rt, kStatic, 0x08CD6CF0u);
        call_set(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, self + 0x2A0u, kStatic);
    call_set(rt, ctx, 0x08C3DECCu, {self + 0x2A4u});
    call_set(rt, ctx, 0x08B28DCCu, {self + 0x40B0u});
    put_float(rt, self + 0x240u, 1.0f);
}

// func_08B1A284(self): func_08B2E924(self), then a tail jump to func_08B1A17C(self).
void func_08B1A284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B2E924u, {self});
    tail_call(ctx, 0x08B1A17Cu, {self});
}

// func_0881BAC0(self, a1, low, high, value) -> 0 when one of the 30 entries of 8 bytes at self + 0x354 already holds the
// 64-bit key (low, high) (func_088A5EE0) with func_088A6164 equal to value; otherwise 1 after the first empty entry (key
// -1) -- or, when there is none, the last one after func_0881ACC0(self, 1, 1) -- gets a frame record built by
// func_088A79AC / func_088A58A4(rec, 0, low, high) / func_088A599C(rec, value), assigned by func_088A6C18 and destroyed
// by func_088A7548.
void func_0881BAC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t low = ctx.gpr[6];
    const std::uint32_t high = ctx.gpr[7];
    const std::uint32_t value = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10u), value);
    std::uint32_t slot = 29u;
    bool found = false;
    for (std::uint32_t index = 0u; index < 30u; ++index) {
        const std::uint32_t entry = self + 0x354u + index * 8u;
        const std::uint32_t key_low = call_set(rt, ctx, 0x088A5EE0u, {entry});
        const std::uint32_t key_high = ctx.gpr[3];
        const std::uint32_t current = call_set(rt, ctx, 0x088A6164u, {entry});
        if (key_low == 0xFFFFFFFFu && key_high == 0xFFFFFFFFu) {
            slot = index;
            found = true;
            break;
        }
        if (key_low == low && key_high == high && word(rt, frame.at(0x10u)) == current) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (!found) call_set(rt, ctx, 0x0881ACC0u, {self, 1u, 1u});
    const std::uint32_t record = frame.at(0u);
    call_set(rt, ctx, 0x088A79ACu, {record, 0u});
    call_set(rt, ctx, 0x088A58A4u, {record, ctx.gpr[5], low, high});
    call_set(rt, ctx, 0x088A599Cu, {record, word(rt, frame.at(0x10u))});
    call_set(rt, ctx, 0x088A6C18u, {self + 0x354u + slot * 8u, record});
    call_set(rt, ctx, 0x088A7548u, {record});
    ctx.gpr[2] = 1u;
}

// func_088EA900(self): for i < func_088D9F80(self), the record r of key k = the i-th halfword of self->+0x138 (0x2F8-byte
// records at self->+0xC0) is collected when r->+0xC is set, r->+4 is 1, func_088E85E8(r) agrees and the bytes r->+0x55 and
// r->+0x56 are clear: k goes to the next halfword of self->+0x13C and func_088E8518(r, 0) runs. When any were collected,
// a tail jump to self->+0x134's virtual slot 0x6C with (object, self + 4, self->+0x13C, count, 1, 1).
void func_088EA900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t collected = 0u;
    for (std::uint32_t index = 0u; signed_of(index) < signed_of(call_set(rt, ctx, 0x088D9F80u, {self})); ++index) {
        const std::uint32_t key = half_u(rt, word(rt, self + 0x138u) + index * 2u);
        const std::uint32_t record = word(rt, self + 0xC0u) + key * 0x2F8u;
        if (word(rt, record + 0xCu) == 0u || word(rt, record + 4u) != 1u) continue;
        if (call_set(rt, ctx, 0x088E85E8u, {record}) == 0u) continue;
        if (byte_u(rt, record + 0x55u) != 0u || byte_u(rt, record + 0x56u) != 0u) continue;
        put16(rt, word(rt, self + 0x13Cu) + collected * 2u, key);
        ++collected;
        call_set(rt, ctx, 0x088E8518u, {record, 0u});
    }
    ctx.gpr[2] = 0u;  // the loop test that ended it
    if (collected == 0u) return;
    const std::uint32_t object = word(rt, self + 0x134u);
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + 0x6Cu);
    tail_call(ctx, ctx.gpr[25], {object, self + 4u, word(rt, self + 0x13Cu), collected, 1u, 1u});
}

// func_08ABE178(milliseconds, buffer, size): the signed form of func_08ABE090: formats through func_08B3767C(buffer, size,
// pattern, ...): "s, ms" with 0x08CB4670 while the minutes are not positive, "m, s, ms" with 0x08CB4658 while the hours
// are not positive, otherwise "h, m, s, ms" with 0x08CB4644; 0x481907FF copies 0x08CB4668 through func_08B7C830. The
// divisions are the compiler's signed multiply-high forms of / 1000 and / 60. A leaf ending in a tail jump.
void func_08ABE178(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t total = ctx.gpr[4];
    const std::uint32_t buffer = ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[6];
    if (total == 0x481907FFu) return tail_call(ctx, 0x08B7C830u, {buffer, 0x08CB4668u, size});
    const auto high = [](std::uint32_t a, std::uint32_t b) {
        return static_cast<std::uint32_t>((static_cast<std::int64_t>(signed_of(a)) * signed_of(b)) >> 32);
    };
    const auto sign = [](std::uint32_t value) { return static_cast<std::uint32_t>(signed_of(value) >> 31); };
    const std::uint32_t seconds = static_cast<std::uint32_t>(signed_of(high(total, 0x10624DD3u)) >> 6) - sign(total);
    const std::uint32_t minutes = static_cast<std::uint32_t>(signed_of(high(seconds, 0x88888889u) + seconds) >> 5) - sign(seconds);
    const std::uint32_t hours = static_cast<std::uint32_t>(signed_of(high(minutes, 0x88888889u) + minutes) >> 5) - sign(minutes);
    const std::uint32_t millis = total - seconds * 1000u;
    const std::uint32_t second = seconds - minutes * 60u;
    const std::uint32_t minute = minutes - hours * 60u;
    if (signed_of(minute) <= 0) return tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB4670u, second, millis, seconds, minutes * 60u});
    if (signed_of(hours) <= 0) return tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB4658u, minute, second, millis, millis});
    tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB4644u, hours, minute, second, millis});
}

// func_08AEFE28(a0) -> 1 when a subsystem with its state at 0x08D42008.. comes up, else 0: func_08AD8960(a0, 0x08C925D4,
// &count, 2) reads the count at 0x08D42014; a block of count x 64 bytes from func_08AD84B4(.., 1) goes to 0x08D42010
// (0 -> fail); two empty list heads at 0x08D42018 and 0x08D42028 are set up and func_08AD8C18(&list, block, count, 0x40,
// -1) fills the second; the Sony stubs func_08C7A998(0x08C925E4, 0, 1, 1, 0) and func_08C7AA30(0x08C925F0, 0x200, 0, 0)
// give handles kept at 0x08D42008 / 0x08D4200C. A negative handle logs through func_08AD83B8(the word at 0x08CDFB40,
// 0x08C91F50, 0xF3B, 0x08C925FC) when bit 0 of the word at 0x08CDFB3C is set; every failure runs func_08AEFF5C.
void func_08AEFE28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kCount = 0x08D42014u;
    constexpr std::uint32_t kFree = 0x08D42018u;
    constexpr std::uint32_t kUsed = 0x08D42028u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08AD8960u, {ctx.gpr[4], 0x08C925D4u, kCount, 2u});
    const std::uint32_t block = call_set(rt, ctx, 0x08AD84B4u, {word(rt, kCount) << 6, 1u});
    put(rt, 0x08D42010u, block);
    const auto fail = [&] {
        call_set(rt, ctx, 0x08AEFF5Cu, {});
        ctx.gpr[2] = 0u;
    };
    if (block == 0u) {
        ctx.gpr[4] = kUsed;
        return fail();
    }
    const std::uint32_t count = word(rt, kCount);
    put(rt, kUsed, kUsed);
    put(rt, kUsed + 8u, 0xFFFFFFFFu);
    put(rt, kFree + 0xCu, 0u);
    put(rt, kFree + 4u, kFree);
    put(rt, kFree + 8u, 0xFFFFFFFFu);
    put(rt, kUsed + 4u, kUsed);
    put(rt, kUsed + 0xCu, 0u);
    put(rt, kFree, kFree);
    call_set(rt, ctx, 0x08AD8C18u, {kUsed, block, count, 0x40u, 0xFFFFFFFFu});
    const auto log_failure = [&] {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, 0xF3Bu, 0x08C925FCu});
        fail();
    };
    const std::uint32_t first = call_set(rt, ctx, 0x08C7A998u, {0x08C925E4u, 0u, 1u, 1u, 0u});
    put(rt, 0x08D42008u, first);
    if (signed_of(first) < 0) return log_failure();
    const std::uint32_t second = call_set(rt, ctx, 0x08C7AA30u, {0x08C925F0u, 0x200u, 0u, 0u});
    put(rt, 0x08D4200Cu, second);
    if (signed_of(second) < 0) return log_failure();
    ctx.gpr[2] = 1u;
}

// func_08B66E88(f12 = x) -> f0: the libm wrapper with SVID error handling: r = func_08B6A500(x); unless the mode word at
// 0x08C96698 is -1 or func_08B6ECB0(x) (not finite) holds, an x above the bound 0x08CB62B8 (by func_08B663A4) raises an
// overflow exception record {type 1, name 0x08CB5E44, arg1 = arg2 = (double) x, retval 0, err 0} (func_08B70F28); in mode
// 2, or when func_08B6E524(&record) (matherr) does not handle it, errno (func_08B77D40) = 0x21 (ERANGE); a nonzero
// record err is stored to errno too; the result is (float) retval (func_08B71EC8).
void func_08B66E88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    call_setf(rt, ctx, 0x08B6A500u, {}, {x});
    const float result = ctx.fpr[0];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    if (mode == 0xFFFFFFFFu || call_setf(rt, ctx, 0x08B6ECB0u, {}, {x}).v0 != 0u) {
        ctx.fpr[0] = result;
        return;
    }
    call_setf(rt, ctx, 0x08B663A4u, {}, {x});
    if (!(float_at(rt, 0x08CB62B8u) < ctx.fpr[0])) {
        ctx.fpr[0] = result;
        return;
    }
    const std::uint32_t record = frame.at(0u);
    put(rt, record, 1u);
    put(rt, record + 4u, 0x08CB5E44u);
    put(rt, record + 0x20u, 0u);
    const std::uint32_t low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
    const std::uint32_t high = ctx.gpr[3];
    put(rt, record + 8u, low);
    put(rt, record + 0x10u, low);
    put(rt, record + 0xCu, high);
    put(rt, record + 0x14u, high);
    put(rt, record + 0x18u, 0u);
    put(rt, record + 0x1Cu, 0u);
    if (mode == 2u || call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), 0x21u);
    if (word(rt, record + 0x20u) != 0u) {
        const std::uint32_t error = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, error, word(rt, record + 0x20u));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
}

// The shared body of func_089EFF1C / func_089F00BC (target, a1, count, holder), a script property of the record r, the
// word of the value object func_089EE140 fills (func_08A5BB7C(0x08CA7EB4) when 0): with count > 0 `setter`(r, &string),
// the string filled by holder->+0's virtual slot 0xC (func_08A5BB7C(0x08CA7E0C) first when that object is 0) and released
// after; otherwise func_08A6D2D4(temp, `getter`(r)) builds a value object that goes to target.
inline void record_string_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t getter, std::uint32_t setter) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (signed_of(ctx.gpr[6]) <= 0) {
        const std::uint32_t source = frame.at(0xCu);
        const std::uint32_t temporary = frame.at(0x10u);
        call_set(rt, ctx, 0x089EE140u, {source});
        const std::uint32_t record = word(rt, source);
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
        call_set(rt, ctx, 0x08A6D2D4u, {temporary, call_set(rt, ctx, getter, {record})});
        keep_temporary(rt, ctx, target, temporary);
        call_set(rt, ctx, kDestroy, {source});
        return;
    }
    const std::uint32_t source = frame.at(8u);
    const std::uint32_t text = frame.at(4u);
    call_set(rt, ctx, 0x089EE140u, {source});
    const std::uint32_t record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7E0Cu});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {text, object});
    call_set(rt, ctx, setter, {record, text});
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, kDestroy, {source});
}

// func_089EFF1C(target, a1, count, holder): record_string_property with func_089EDABC / func_089EFEB4.
void func_089EFF1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_string_property(rt, ctx, 0x089EDABCu, 0x089EFEB4u); }

// func_089F00BC(target, a1, count, holder): record_string_property with func_089EDAC4 / func_089F0054.
void func_089F00BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_string_property(rt, ctx, 0x089EDAC4u, 0x089F0054u); }

// func_0881F8A8(target, key, count, args): with x = args->+0's and y = args->+4's virtual slot 0x30 (0 when count is below 1
// / 2; func_08A5BB7C(0x08C98EFC) first for a null object) and r = func_0881B068(w) for the word w of the value object
// func_0881BD98(temp, key) fills (func_08A5BB7C(0x08C98F48) when 0; destroyed after): func_08A85B1C(temp,
// func_0881F5CC(r, 0, x, y)) builds a value object that goes to target.
void func_0881F8A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const auto slot = [&](std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
        return call_virtual_at(rt, ctx, object, 0x30u, {object});
    };
    const std::uint32_t first = signed_of(count) > 0 ? slot(args) : 0u;
    const std::uint32_t second = signed_of(count) < 2 ? 0u : slot(args + 4u);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x0881BD98u, {source, key});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const std::uint32_t found = call_set(rt, ctx, 0x0881B068u, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t result = call_set(rt, ctx, 0x0881F5CCu, {found, 0u, first, second});
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, result});
    keep_temporary(rt, ctx, target, temporary);
}

// func_0897876C(self, key): under the lock at self + 0x20, func_08BEDE74(self + 0x18, key, &list) collects nodes into a
// frame list (a circular head at frame +0); then for each node, while the last bit of the bit vector at self + 0x74
// (begin word / bit at +0x74 / +0x78, end at +0x7C / +0x80) is set and the vector is not empty, the node's object (+8;
// func_08A5BB7C(0x08CA2F7C) when 0) gets its virtual slot 0x108. func_08BEDD64(&list) frees the list.
void func_0897876C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t list = frame.at(0u);
    put(rt, list, list);
    put(rt, list + 4u, list);
    call_set(rt, ctx, kLock, {self + 0x20u});
    call_set(rt, ctx, 0x08BEDE74u, {self + 0x18u, key, list});
    call_set(rt, ctx, kUnlock, {self + 0x20u});
    for (std::uint32_t node = word(rt, list); node != list; node = word(rt, node)) {
        const std::uint32_t end_word = word(rt, self + 0x7Cu);
        const std::uint32_t end_bit = word(rt, self + 0x80u);
        if (word(rt, self + 0x74u) == end_word && word(rt, self + 0x78u) == end_bit) continue;
        const std::int32_t last = signed_of(end_bit) - 1;
        std::uint32_t address = end_word + static_cast<std::uint32_t>(((last < 0 ? last + 31 : last) >> 5) * 4);
        std::int32_t bit = last % 32;
        if (bit < 0) {
            bit += 32;
            address -= 4u;
        }
        if (((word(rt, address) >> bit) & 1u) == 0u) continue;
        const std::uint32_t object = word(rt, node + 8u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2F7Cu});
        call_virtual_at(rt, ctx, object, 0x108u, {object});
    }
    call_set(rt, ctx, 0x08BEDD64u, {list});
}

// func_08A7E99C(result, holder, type, key): when the word type->+0 is the table type 0x08CF6FA0, holder->+0
// (func_08A5BB7C(0x08CB1360) when 0) fills a frame tree map through func_08A7D1F8, and every (key +0x10, value +0x14) node
// in order (func_08B5F100 steps) goes to func_08A7E360(result, &key, &value); func_08C00860 frees the tree. Otherwise
// holder->+0's virtual slot 0x10C(object, &value, type) fills a value object, and when it is set func_08A7E360(result,
// type when key->+0 is the type 0x08CF6FDC else key, &value); the value object is destroyed.
void func_08A7E99C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t type = ctx.gpr[6];
    const std::uint32_t key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t value = frame.at(0u);
    if (word(rt, type) == word(rt, 0x08CF6FA0u)) {
        const std::uint32_t object = word(rt, holder);
        const std::uint32_t header = frame.at(4u);
        put(rt, frame.at(0x14u), 0u);
        put(rt, header, 0u);
        put(rt, header + 4u, 0u);
        put(rt, header + 8u, header);
        put(rt, header + 0xCu, header);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
        call_set(rt, ctx, 0x08A7D1F8u, {object, value});
        for (std::uint32_t node = word(rt, header + 8u); node != header;) {
            call_set(rt, ctx, 0x08A7E360u, {result, node + 0x10u, node + 0x14u});
            node = call_set(rt, ctx, 0x08B5F100u, {node});
        }
        call_set(rt, ctx, 0x08C00860u, {value, word(rt, header + 4u)});
        return;
    }
    call_set(rt, ctx, 0x08A5D438u, {value});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
    call_virtual_at(rt, ctx, object, 0x10Cu, {object, value, type});
    if (word(rt, value) != 0u) {
        const std::uint32_t which = word(rt, key) == word(rt, 0x08CF6FDCu) ? type : key;
        call_set(rt, ctx, 0x08A7E360u, {result, which, value});
    }
    call_set(rt, ctx, kDestroy, {value});
}

// func_08AB8430(tokenizer, keep_quotes) -> the next token of the tokenizer {+4: cursor, +8: token count, +0xC: the byte
// that ended the token}, or 0 at the end: blanks are skipped (func_08AB836C). A token opening with " or ' runs to the
// matching quote (func_08B7BA50 = strchr from just after the opening quote); without keep_quotes the quotes are cut off
// and the closing one becomes the terminator; with keep_quotes the quotes stay and a quote right after the closing one
// searches again from the token start. Any other token is scanned by func_08AB83CC and its end byte is saved and replaced
// by a terminator. The token count goes up.
void func_08AB8430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tokenizer = ctx.gpr[4];
    const std::uint32_t keep = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    put8(rt, tokenizer + 0xCu, 0u);
    call_set(rt, ctx, 0x08AB836Cu, {tokenizer});
    const std::uint32_t cursor = word(rt, tokenizer + 4u);
    ctx.gpr[2] = 0u;
    if (cursor == 0u) return;
    std::uint32_t quote = sext8(byte_u(rt, cursor));
    if (quote == 0u) return;
    std::uint32_t start = cursor;
    bool scan_word = true;
    if (quote == 0x22u || quote == 0x27u) {
        scan_word = false;
        if (keep == 0u) start = cursor + 1u;
        std::uint32_t from = cursor + 1u;
        for (;;) {
            const std::uint32_t close = call_set(rt, ctx, 0x08B7BA50u, {from, quote});
            put(rt, tokenizer + 4u, close);
            if (keep == 0u) {
                if (close != 0u) {
                    put8(rt, close, 0u);
                    put(rt, tokenizer + 4u, close + 1u);
                }
                break;
            }
            if (close == 0u || sext8(byte_u(rt, close)) == 0u) break;
            put(rt, tokenizer + 4u, close + 1u);
            const std::uint32_t next = sext8(byte_u(rt, close + 1u));
            if (next == 0u) break;
            if (next == 0x27u || next == 0x22u) {
                quote = next;
                from = start + 1u;
                continue;
            }
            scan_word = true;
            break;
        }
    }
    if (scan_word) {
        call_set(rt, ctx, 0x08AB83CCu, {tokenizer});
        const std::uint32_t end = word(rt, tokenizer + 4u);
        put8(rt, tokenizer + 0xCu, byte_u(rt, end));
        if (sext8(byte_u(rt, end)) != 0u) {
            put8(rt, end, 0u);
            put(rt, tokenizer + 4u, end + 1u);
        }
    }
    put(rt, tokenizer + 8u, word(rt, tokenizer + 8u) + 1u);
    ctx.gpr[2] = start;
}

// func_08AF8EB4(self, frame_index): when self->+0x48 is set it is moved to frame min(index, func_08AF6E90(it) - 1) by
// func_08AF6DCC; then for each of the halfword self->+0x14 parts: target = the 0x20-byte element of self->+0xA8 (or,
// without that array, self->+0x38's 0x30-byte element ->+0x2C->+0x30), and func_08B537AC(target, self->+0x94,
// func_08AFD45C(self->+0x84, 0, min(index, self->+0x84->+6 - 1)), 0x1C, self->+0x48->+0x18 when its halfword +0x14 is set
// else 0, 0x98).
void func_08AF8EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), ctx.gpr[5]);
    if (word(rt, self + 0x48u) != 0u) {
        const std::uint32_t last = call_set(rt, ctx, 0x08AF6E90u, {word(rt, self + 0x48u)}) - 1u;
        put(rt, frame.at(4u), last);
        const std::uint32_t index = word(rt, frame.at(0x10u));
        call_set(rt, ctx, 0x08AF6DCCu, {word(rt, self + 0x48u), signed_of(index) < signed_of(last) ? index : last});
    }
    const std::uint32_t limit = half_u(rt, word(rt, self + 0x84u) + 6u) - 1u;
    put(rt, frame.at(0u), limit);
    const std::uint32_t index = word(rt, frame.at(0x10u));
    const std::uint32_t chosen = signed_of(index) < signed_of(limit) ? index : limit;
    // v0 is that comparison when no part runs (the loop's own `slt` leaves 0 when it ends); v1 the slot min() read.
    ctx.gpr[2] = signed_of(index) < signed_of(limit) ? 1u : 0u;
    ctx.gpr[3] = ctx.gpr[2] != 0u ? frame.at(0x10u) : frame.at(0u);
    for (std::uint32_t part = 0u; signed_of(part) < signed_of(half_u(rt, self + 0x14u)); ++part) {
        const std::uint32_t array = word(rt, self + 0xA8u);
        std::uint32_t target = array + (part << 5);
        if (array == 0u) target = word(rt, word(rt, word(rt, self + 0x38u) + part * 0x30u + 0x2Cu) + 0x30u);
        const std::uint32_t palette = word(rt, self + 0x94u);
        const std::uint32_t value = call_set(rt, ctx, 0x08AFD45Cu, {word(rt, self + 0x84u), 0u, chosen});
        std::uint32_t extra = 0u;
        const std::uint32_t animation = word(rt, self + 0x48u);
        if (animation != 0u && half_u(rt, animation + 0x14u) != 0u) extra = word(rt, animation + 0x18u);
        call_set(rt, ctx, 0x08B537ACu, {target, palette, value, 0x1Cu, extra, 0x98u});
        ctx.gpr[2] = 0u;
    }
}

// func_0888C0E4(self, key, value): when func_0889417C(self->+0xC, key)->+0 has bit 0x20, the entry
// func_088DA788(self->+4, key) has the bytes +0x25 / +0x26 / +0x27 clear, the byte at self->+4->+0x618 is set and
// func_088DB2C8(self->+4, key) finds it: i = func_088E0BE4(self->+4, value); when i is one of func_088DAF90(self->+4, s)
// for the names 0x08CA0770 / 0x08CA0778 / 0x08CA0780, that byte is cleared and self's virtual slot 0x70 runs with
// (self, self->+4 + 4, &key, 1, 0, 0).
void func_0888C0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t flags = call_set(rt, ctx, 0x0889417Cu, {word(rt, self + 0xCu)});
    const std::uint32_t pending = word(rt, word(rt, self + 4u) + 0x618u);
    if ((word(rt, flags) & 0x20u) == 0u) return;  // v0 stays the flags pointer; later early returns leave the value read
    const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), key});
    for (const std::uint32_t offset : {0x25u, 0x27u, 0x26u}) {
        ctx.gpr[2] = byte_u(rt, entry + offset);
        if (ctx.gpr[2] != 0u) return;
    }
    ctx.gpr[2] = byte_u(rt, pending);
    if (ctx.gpr[2] == 0u) return;
    if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), key}) == 0u) return;
    const std::uint32_t index = call_set(rt, ctx, 0x088E0BE4u, {word(rt, self + 4u), value});
    bool match = false;
    for (const std::uint32_t name : {0x08CA0770u, 0x08CA0778u, 0x08CA0780u}) {
        if (call_set(rt, ctx, 0x088DAF90u, {word(rt, self + 4u), name}) == index) {
            match = true;
            break;
        }
    }
    if (!match) return;
    put16(rt, frame.at(0u), key);
    put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
    ctx.gpr[2] = word(rt, self);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x70u);
    call_set(rt, ctx, ctx.gpr[3], {self, word(rt, self + 4u) + 4u, frame.at(0u), 1u, 0u, 0u});
}

// func_088D32E0(out, holder, a2, f12) -> out: a position, zero unless o = holder->+0->+0x14C's virtual slot 0x2C gives a
// model m, m's slot 0x1C gives a node n and func_088DB5C4(holder->+0) agrees. Then n's slot 8 (n, m, &query) answers a
// query {1, 0, a2, 0, f12, 0} in the frame with a point p (at query + 0x28): out = (p.x, p.y + 5, -p.z), which
// func_088FAB84(func_088DB5C4(holder->+0), &tmp, out) converts into the frame copy that becomes out.
void func_088D32E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0xC0u);
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    ctx.gpr[2] = out;
    const std::uint32_t owner = word(rt, word(rt, holder) + 0x14Cu);
    const std::uint32_t model = call_virtual_at(rt, ctx, owner, 0x2Cu, {owner});
    if (model == 0u) {
        ctx.gpr[2] = out;
        return;
    }
    const std::uint32_t node = call_virtual_at(rt, ctx, model, 0x1Cu, {model});
    if (node == 0u || call_set(rt, ctx, 0x088DB5C4u, {word(rt, holder)}) == 0u) {
        ctx.gpr[2] = out;
        return;
    }
    const std::uint32_t query = frame.at(0x38u);
    put(rt, query, 1u);
    put(rt, query + 8u, third);
    put_float(rt, query + 0x10u, value);
    put(rt, query + 4u, 0u);
    put(rt, query + 0xCu, 0u);
    put(rt, query + 0x14u, 0u);
    call_virtual_at(rt, ctx, node, 8u, {node, model, query});
    const float y = float_at(rt, frame.at(0x68u)) + 5.0f;
    put_float(rt, out + 4u, y);
    put(rt, out, word(rt, frame.at(0x60u)));
    put_float(rt, out + 8u, -float_at(rt, frame.at(0x64u)));
    put(rt, frame.at(0x34u), 0xFFFFFFFFu);
    put(rt, frame.at(0x30u), 0u);
    const std::uint32_t context = call_set(rt, ctx, 0x088DB5C4u, {word(rt, holder)});
    call_set(rt, ctx, 0x088FAB84u, {context, frame.at(0u), out});
    const std::uint32_t z = word(rt, frame.at(8u));
    const std::uint32_t x = word(rt, frame.at(0u));
    const std::uint32_t y2 = word(rt, frame.at(4u));
    put(rt, out + 8u, z);
    put(rt, out + 4u, y2);
    put(rt, out, x);
    ctx.gpr[2] = out;
}

// func_089AEF20(a0, a1, mode, args): nothing unless mode is 2 (v0 = 2). With r the word of the value object func_089C9C34
// fills (func_08A5BB7C(0x08CA518C) when 0): func_089C8C70(r, &a, &b), a and b the strings args->+0's and args->+4's virtual
// slot 0xC fill (func_08A5BB7C(0x08CA50BC) first for a null object); b, a and the value object are released after.
void func_089AEF20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t args = ctx.gpr[7];
    ctx.gpr[2] = 2u;
    if (ctx.gpr[6] != 2u) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t source = frame.at(0xCu);
    const std::uint32_t first = frame.at(8u);
    const std::uint32_t second = frame.at(4u);
    call_set(rt, ctx, 0x089C9C34u, {source});
    const std::uint32_t record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA518Cu});
    const auto fill = [&](std::uint32_t text, std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
        call_set(rt, ctx, ctx.gpr[2], {text, object});
    };
    fill(first, args);
    fill(second, args + 4u);
    call_set(rt, ctx, 0x089C8C70u, {record, first, second});
    release_string(rt, ctx, second, 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
    release_string(rt, ctx, first, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, kDestroy, {source});
}

// func_08B66FBC(f12 = x, f13 = y) -> f0: the libm two-argument wrapper with SVID error handling: r = func_08B6A844(x, y);
// unless the mode word at 0x08C96698 is -1 or y or x is NaN (func_08B6ECB0), x = y = 0 raises a domain error record
// {type 1, name 0x08CB5E4C, arg1 = (double) x, arg2 = (double) y, retval 0, err 0}: in mode 2, or when func_08B6E524
// (matherr) does not handle it, errno (func_08B77D40) = 0x21 (EDOM); a nonzero record err goes to errno too; the result
// is (float) retval.
void func_08B66FBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    GuestFrame frame(ctx, 0x50u);
    call_setf(rt, ctx, 0x08B6A844u, {}, {x, y});
    const float result = ctx.fpr[0];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    const auto keep = [&] { ctx.fpr[0] = result; };
    if (mode == 0xFFFFFFFFu) return keep();
    if (call_setf(rt, ctx, 0x08B6ECB0u, {}, {y}).v0 != 0u) return keep();
    if (call_setf(rt, ctx, 0x08B6ECB0u, {}, {x}).v0 != 0u) return keep();
    if (!(y == 0.0f) || !(x == 0.0f)) return keep();
    const std::uint32_t record = frame.at(0u);
    const std::uint32_t x_low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
    put(rt, record + 8u, x_low);
    put(rt, record + 0xCu, ctx.gpr[3]);
    const std::uint32_t y_low = call_setf(rt, ctx, 0x08B70F28u, {}, {y}).v0;
    put(rt, record + 0x10u, y_low);
    put(rt, record + 0x14u, ctx.gpr[3]);
    put(rt, record, 1u);
    put(rt, record + 4u, 0x08CB5E4Cu);
    put(rt, record + 0x20u, 0u);
    put(rt, record + 0x18u, 0u);
    put(rt, record + 0x1Cu, 0u);
    if (mode == 2u || call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), 0x21u);
    if (word(rt, record + 0x20u) != 0u) {
        const std::uint32_t error = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, error, word(rt, record + 0x20u));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
}

// func_08937918(self, a1, a2, a3, out) -> a buffer with a file's data, or 0: unless func_0893656C(self, a1, a2, a3) is set,
// v = func_08936578(self, .., a2, a3) and i = func_08939920(self + 0x1F4, v) (-1 -> 0) name the entry
// e = func_08938EF0(self + 0x1F4, i); the file self + 0xC + the suffix 0x08CA2220 (func_08B7BB6C / func_08B7B9AC into a
// frame path) is opened (func_08B0C99C on a frame file object from func_08B0CC04); when that works, it seeks to e->+4
// (func_08B0C5C0), e->+8 bytes go into a buffer from func_08937AF4 (func_08B0C650), the file is closed (func_08B0C778) and
// out = the size. func_08B0C808 ends the file object.
void func_08937918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t fourth = ctx.gpr[7];
    const std::uint32_t out = ctx.gpr[8];
    const std::uint32_t table = self + 0x1F4u;
    GuestFrame frame(ctx, 0x240u);
    const std::uint32_t skip = call_set(rt, ctx, 0x0893656Cu, {});
    const std::uint32_t name = call_set(rt, ctx, 0x08936578u, {self, ctx.gpr[5], third, fourth});
    ctx.gpr[2] = 0u;
    if (skip != 0u) return;
    const std::uint32_t index = call_set(rt, ctx, 0x08939920u, {table, name});
    if (index == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x08938EF0u, {table, index});
    const std::uint32_t file = frame.at(0x108u);
    const std::uint32_t path = frame.at(8u);
    call_set(rt, ctx, 0x08B0CC04u, {file});
    call_set(rt, ctx, 0x08B7BB6Cu, {path, self + 0xCu});
    call_set(rt, ctx, 0x08B7B9ACu, {path, 0x08CA2220u});
    std::uint32_t buffer = 0u;
    if (signed_of(call_set(rt, ctx, 0x08B0C99Cu, {file, path, 0u, 0u})) >= 0) {
        call_set(rt, ctx, 0x08B0C5C0u, {file, 0u, word(rt, entry + 4u), 0u});
        buffer = call_set(rt, ctx, 0x08937AF4u, {word(rt, entry + 8u)});
        put(rt, frame.at(4u), word(rt, entry + 8u));
        put(rt, frame.at(0u), buffer);
        call_set(rt, ctx, 0x08B0C650u, {file, frame.at(0u)});
        call_set(rt, ctx, 0x08B0C778u, {file});
        put(rt, out, word(rt, entry + 8u));
    } else {
        ctx.gpr[5] = 0u;
    }
    call_set(rt, ctx, 0x08B0C808u, {file});
    ctx.gpr[2] = buffer;
}

// func_08AF07C8(id, wait): stops the channel func_08AEFD90(id, 0) finds: its +0x38 = 1 inside func_08AEB808 /
// func_08AEB82C; waits (func_08C7A988(10000)) while +0x3C is positive; with wait, while +0x24 is positive the Sony stub
// func_08C7AA10(the handle at 0x08D4200C, 1 << +8, 0x20, 0, 0) waits on it. Then under the semaphore 0x08D42008
// (func_08C7AA20 / func_08C7AA08) its buffer +0x18 is freed (func_08AD85C8), the fields +0xC..+0x2C are cleared and the
// node moves from its list to the free list at 0x08D42028.
void func_08AF07C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kFree = 0x08D42028u;
    const std::uint32_t wait = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t channel = call_set(rt, ctx, 0x08AEFD90u, {ctx.gpr[4], 0u});
    if (channel == 0u) return;
    call_set(rt, ctx, 0x08AEB808u, {});
    put(rt, channel + 0x38u, 1u);
    call_set(rt, ctx, 0x08AEB82Cu, {});
    while (signed_of(word(rt, channel + 0x3Cu)) > 0) call_set(rt, ctx, 0x08C7A988u, {10000u});
    if (wait != 0u && signed_of(word(rt, channel + 0x24u)) > 0) {
        do {
            call_set(rt, ctx, 0x08C7AA10u, {word(rt, 0x08D4200Cu), 1u << (word(rt, channel + 8u) & 31u), 0x20u, 0u, 0u});
        } while (signed_of(word(rt, channel + 0x24u)) > 0);
    }
    call_set(rt, ctx, 0x08C7AA20u, {word(rt, 0x08D42008u), 1u, 0u});
    call_set(rt, ctx, 0x08AD85C8u, {word(rt, channel + 0x18u)});
    const std::uint32_t next = word(rt, channel);
    put8(rt, channel + 0x10u, 0u);
    put16(rt, channel + 0x14u, 0u);
    put16(rt, channel + 0x16u, 0u);
    for (std::uint32_t offset = 0x18u; offset <= 0x2Cu; offset += 4u) put(rt, channel + offset, 0u);
    put(rt, channel + 0xCu, 0u);
    if (next != channel) {
        put(rt, next + 4u, word(rt, channel + 4u));
        const std::uint32_t prev = word(rt, channel + 4u);
        put(rt, channel + 4u, channel);
        put(rt, prev, next);
        put(rt, channel, channel);
    }
    const std::uint32_t head = word(rt, kFree);
    put(rt, channel + 4u, kFree);
    put(rt, head + 4u, channel);
    const std::uint32_t back = word(rt, channel + 4u);
    put(rt, channel, head);
    put(rt, back, channel);
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, 0x08D42008u), 1u});
}

// func_08B82420() -> 0, or 0x80440001 when already started (the flag 0x08D72E4C) or a kernel object cannot be made: the
// pointers 0x08D740E4 / 0x08D740E0 are set to 0x08D72E54 / 0x08D72EC4, the 0x200-byte areas at 0x08D73CD4 and 0x08D73ED4
// cleared, and the Sony stub func_08C7ABB0(-1, size, 0) makes one object of 0x1C0 (kept at 0x08D740D8) and three of 0x100
// (0x08D740EC..); on a failure the ones made are released by func_08C7ABB8 (the asm passes their indices).
void func_08B82420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kStarted = 0x08D72E4Cu;
    constexpr std::uint32_t kFailed = 0x80440001u;
    ctx.gpr[2] = kFailed;
    if (word(rt, kStarted) != 0u) return;
    GuestFrame frame(ctx, 0x20u);
    put(rt, 0x08D740E4u, 0x08D72E54u);
    put(rt, 0x08D740E0u, 0x08D72EC4u);
    for (std::uint32_t offset = 0u; offset < 0x200u; ++offset) put8(rt, 0x08D73CD4u + offset, 0u);
    for (std::uint32_t offset = 0u; offset < 0x200u; ++offset) put8(rt, 0x08D73ED4u + offset, 0u);
    const std::uint32_t main = call_set(rt, ctx, 0x08C7ABB0u, {0xFFFFFFFFu, 0x1C0u, 0u});
    put(rt, 0x08D740D8u, main);
    if (signed_of(main) < 0) {
        ctx.gpr[2] = kFailed;
        return;
    }
    for (std::int32_t index = 0; index < 3; ++index) {
        const std::uint32_t handle = call_set(rt, ctx, 0x08C7ABB0u, {0xFFFFFFFFu, 0x100u, 0u});
        put(rt, 0x08D740ECu + static_cast<std::uint32_t>(index) * 4u, handle);
        if (signed_of(handle) < 0) {
            for (std::int32_t made = index - 1; made >= 0; --made) call_set(rt, ctx, 0x08C7ABB8u, {static_cast<std::uint32_t>(made)});
            call_set(rt, ctx, 0x08C7ABB8u, {word(rt, 0x08D740D8u)});
            ctx.gpr[2] = kFailed;
            return;
        }
    }
    put(rt, kStarted, 1u);
    ctx.gpr[2] = 0u;
}

// func_0888E62C(self, key, f12): like func_0888C0E4 for the flag bits 0x40 or 4 of func_0889417C(self->+0xC, key)->+0, with
// no name test: with bit 4 set, the object func_088DCD28(self->+4, key)'s virtual slot 0x10 (a float d) must also give
// f12 / d >= 5.0 before self's virtual slot 0x70 runs.
void func_0888E62C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t flags = call_set(rt, ctx, 0x0889417Cu, {word(rt, self + 0xCu)});
    const std::uint32_t pending = word(rt, word(rt, self + 4u) + 0x618u);
    ctx.gpr[2] = word(rt, flags) & 4u;  // every early return leaves v0 = the value its test read
    if ((word(rt, flags) & 0x44u) == 0u) return;
    const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), key});
    for (const std::uint32_t offset : {0x25u, 0x27u, 0x26u}) {
        ctx.gpr[2] = byte_u(rt, entry + offset);
        if (ctx.gpr[2] != 0u) return;
    }
    ctx.gpr[2] = byte_u(rt, pending);
    if (ctx.gpr[2] == 0u) return;
    if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), key}) == 0u) return;
    const std::uint32_t object = call_set(rt, ctx, 0x088DCD28u, {word(rt, self + 4u), key});
    call_virtual_at(rt, ctx, object, 0x10u, {object});
    if ((word(rt, flags) & 4u) != 0u && !(5.0f <= value / ctx.fpr[0])) {
        ctx.gpr[2] = 4u;
        return;
    }
    put16(rt, frame.at(0u), key);
    put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
    ctx.gpr[2] = word(rt, self);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x70u);
    call_set(rt, ctx, ctx.gpr[3], {self, word(rt, self + 4u) + 4u, frame.at(0u), 1u, 0u, 0u});
}

// The size in bytes of a bit vector of `bits` bits: whole 32-bit words.
inline std::uint32_t bit_bytes(std::uint32_t bits) { return ((bits + 0x1Fu) >> 5) << 2; }

// func_088B4178(self, stream): func_088B4080(self, 1); a count n (func_08BB1668) of bytes follows (func_08BB1694 each):
// those that fit (index < self->+4) go to self->+0, and +0x10 (halfword) tracks the length up to the last non-negative
// byte. Then, when n is nonzero, an n-bit vector read into a temporary block (func_08B6461C, func_088BA534) is copied over
// the bit vector at self + 8 (the smaller of both sizes; func_08B79A88) and freed (func_08B61FC0). Last, two bytes go to
// self + 0x12 (func_08BB1694) and self + 0x13 (func_08BB16C0).
void func_088B4178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t byte_slot = frame.at(0u);
    const std::uint32_t count_slot = frame.at(4u);
    const std::uint32_t vector = frame.at(8u);
    call_set(rt, ctx, 0x088B4080u, {self, 1u});
    call_set(rt, ctx, 0x08BB1668u, {stream, count_slot});
    if (word(rt, count_slot) != 0u) {
        for (std::uint32_t index = 0u; index < word(rt, count_slot); ++index) {
            call_set(rt, ctx, 0x08BB1694u, {stream, byte_slot});
            if (index < word(rt, self + 4u)) {
                put8(rt, word(rt, self) + index, byte_u(rt, byte_slot));
                if (signed_of(sext8(byte_u(rt, byte_slot))) >= 0) put16(rt, self + 0x10u, index + 1u);
            }
        }
        const std::uint32_t bits = word(rt, count_slot);
        if (bits != 0u) {
            put(rt, vector + 4u, bits);
            put(rt, vector, 0u);
            const std::uint32_t block = call_set(rt, ctx, 0x08B6461Cu, {bit_bytes(bits)});
            put(rt, vector, block);
            call_set(rt, ctx, 0x088BA534u, {stream, block, bit_bytes(word(rt, vector + 4u))});
            const std::uint32_t mine = bit_bytes(word(rt, self + 0xCu));
            const std::uint32_t theirs = bit_bytes(word(rt, vector + 4u));
            call_set(rt, ctx, 0x08B79A88u, {word(rt, self + 8u), word(rt, vector), mine < theirs ? mine : theirs});
            if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B61FC0u, {word(rt, vector)});
            put(rt, vector, 0u);
            put(rt, vector + 4u, 0u);
        }
    }
    const std::uint32_t next = call_set(rt, ctx, 0x08BB1694u, {stream, self + 0x12u});
    call_set(rt, ctx, 0x08BB16C0u, {next, self + 0x13u});
}

// func_08BBEA18(list): sweeps the circular list at list (next at +0): every node whose byte +0x14 is clear and whose shared
// object +0x10 is null or has no uses (+4) is unlinked (func_08B5C094) and freed: its chain at +0x18 (next at +4) is
// walked, each element handed to func_08BBD1B8(node + 0x18, &(element, previous)) and deleted through its virtual slot
// 4; the shared object's weak count (+8) steps down (slot 0xC on the last); the string at +8 is released (empty rep
// 0x08D75298, func_08B93554); then func_08B35254(node).
void func_08BBEA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    for (std::uint32_t node = word(rt, list); node != list;) {
        const std::uint32_t next = word(rt, node);
        const std::uint32_t shared = word(rt, node + 0x10u);
        if (byte_u(rt, node + 0x14u) != 0u || (shared != 0u && word(rt, shared + 4u) != 0u)) {
            node = next;
            continue;
        }
        call_set(rt, ctx, 0x08B5C094u, {node});
        std::uint32_t previous = 0u;
        for (std::uint32_t element = word(rt, node + 0x18u); element != 0u;) {
            const std::uint32_t following = word(rt, element + 4u);
            put(rt, frame.at(4u), element);
            put(rt, frame.at(8u), previous);
            const std::uint32_t low = call_set(rt, ctx, 0x08BBD1B8u, {node + 0x18u, frame.at(4u)});
            const std::uint32_t high = ctx.gpr[3];
            put(rt, frame.at(0xCu), low);
            put(rt, frame.at(0x10u), high);
            put(rt, frame.at(0x20u), low);
            put(rt, frame.at(0x24u), high);
            call_virtual_at(rt, ctx, element, 4u, {element});
            previous = element;
            element = following;
        }
        const std::uint32_t object = word(rt, node + 0x10u);
        if (object != 0u) {
            const std::uint32_t old = word(rt, object + 8u);
            put(rt, object + 8u, old - 1u);
            ctx.gpr[3] = object + 8u;
            ctx.gpr[5] = old;
            ctx.gpr[2] = 1u;
            if (old == 1u) call_virtual_at(rt, ctx, object, 0xCu, {object});
        }
        release_string(rt, ctx, node + 8u, 0x08D75298u, 0x08B93554u, frame.at(0u));
        call_set(rt, ctx, 0x08B35254u, {node});
        node = next;
    }
}

// func_088F9CE4(self, a1, key, a3, t0, t1, t2, t3) -> an id, or 0: id = func_08B03BD0(o, a1, a3, t0 or the default name
// 0x08C8A444, t1, t2, t3 & 0xFF, 0; stack: 0, 0x1000, 1) with o = self->+0->+0x1C (negative -> 0); its entry
// e = func_08B03BB4(o, id) gets func_08AFFECC(e) when t0, t1 and t2 are all 0 and bit 4 of the halfword +0xE2 when t1 and
// t2 are; then the word 0x80000000 | (key & 0x7FFF) << 16 | (id & 0xFFFF) is appended to the vector self->+0 + 4 (inline
// when there is room, else func_08BBBA44).
void func_088F9CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[6];
    const std::uint32_t name = ctx.gpr[8];
    const std::uint32_t sixth = ctx.gpr[9];
    const std::uint32_t seventh = ctx.gpr[10];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(4u), 0x1000u);
    put(rt, frame.at(8u), 1u);
    put(rt, frame.at(0u), 0u);
    const std::uint32_t id = call_set(rt, ctx, 0x08B03BD0u, {word(rt, word(rt, self) + 0x1Cu), ctx.gpr[5], ctx.gpr[7],
                                                             name != 0u ? name : 0x08C8A444u, sixth, seventh, ctx.gpr[11] & 0xFFu, 0u});
    if (signed_of(id) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x08B03BB4u, {word(rt, word(rt, self) + 0x1Cu), id});
    if ((name | sixth | seventh) == 0u) call_set(rt, ctx, 0x08AFFECCu, {entry});
    if ((sixth | seventh) == 0u) put16(rt, entry + 0xE2u, half_u(rt, entry + 0xE2u) | 4u);
    const std::uint32_t packed = (id & 0xFFFFu) | ((key & 0x7FFFu) << 16) | 0x80000000u;
    put(rt, frame.at(0x10u), packed);
    const std::uint32_t vector = word(rt, self) + 4u;
    const std::uint32_t end = word(rt, vector + 4u);
    if (end == word(rt, vector + 8u)) {
        call_set(rt, ctx, 0x08BBBA44u, {vector, end, frame.at(0x10u)});
    } else {
        std::uint32_t next = 4u;
        if (end != 0u) {
            for (std::uint32_t offset = 0u; offset < 4u; ++offset) put8(rt, end + offset, packed >> (offset * 8u));
            next = word(rt, vector + 4u) + 4u;
        }
        put(rt, vector + 4u, next);
    }
    ctx.gpr[2] = id;
}

// func_08A1FE40(target, count, holder): nothing when count <= 0. A time in milliseconds -- holder->+0's virtual slot 0x40
// when func_08A76F20(o, func_08A82A5C()) says the object is of that kind, else its slot 0x3C (func_08A5BB7C(0x08CAA480)
// for a null object) -- is formatted by func_08ABE090 into a 0x80-byte frame buffer; func_08A6D2D4(temp, &string) of that
// text builds a value object that goes to target; the string is released last.
void func_08A1FE40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[6];
    if (signed_of(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0xA0u);
    const std::uint32_t first = word(rt, holder);
    if (first == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    const std::uint32_t kind = call_set(rt, ctx, 0x08A82A5Cu, {});
    const std::uint32_t matches = call_set(rt, ctx, 0x08A76F20u, {first, kind});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    const std::uint32_t time = call_virtual_at(rt, ctx, object, matches != 0u ? 0x40u : 0x3Cu, {object});
    const std::uint32_t buffer = frame.at(0xCu);
    call_set(rt, ctx, 0x08ABE090u, {time, buffer, 0x80u});
    const std::uint32_t text = frame.at(4u);
    const std::uint32_t temporary = frame.at(8u);
    call_set(rt, ctx, 0x08B85EB8u, {text, buffer, frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {temporary, text});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// A one-time static: when the guard byte is clear and func_08B6441C acquires it, `init` runs and func_08B64494 releases it.
template <typename Init>
inline void once(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, Init init) {
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    init();
    call_set(rt, ctx, 0x08B64494u, {guard});
}

// func_08B93734(object) -> 0 for a null object, otherwise a tail jump to func_08B93430(object, its virtual slot 8 (the
// dynamic type), the static type record 0x08D752B8, 0) (a dynamic cast); the type records 0x08D752A8 and 0x08D752B8
// ({0, name, vtable 0x08CBF670}, the second also pointing at the first from 0x08D752C4) are built once behind their
// guards 0x08D75068 / 0x08D750F0 / 0x08D75060.
void func_08B93734(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t type = call_virtual_at(rt, ctx, object, 8u, {object});
    once(rt, ctx, 0x08D75068u, [&] {
        once(rt, ctx, 0x08D750F0u, [&] {
            put(rt, 0x08D752A8u + 8u, 0x08CBF670u);
            put(rt, 0x08D752A8u + 4u, 0x08D752B4u);
            put(rt, 0x08D752A8u, 0u);
        });
        put(rt, 0x08D752C4u, 0x08D752A8u);
    });
    once(rt, ctx, 0x08D75060u, [&] {
        put(rt, 0x08D752B8u + 8u, 0x08CBF670u);
        put(rt, 0x08D752B8u + 4u, 0x08D752C4u);
        put(rt, 0x08D752B8u, 0u);
    });
    tail_call(ctx, 0x08B93430u, {object, type, 0x08D752B8u, 0u});
}

// func_08A01A24(a0, a1, mode, args): mode 2 or 3 calls the record's virtual slot 0x23C, the record being the word of the
// value object func_08A01338 fills (func_08A5BB7C(0x08CA8B14) when 0): mode 2 with (record, 0, x, y) from
// func_08969244(&args[0]) and func_08969244(&args[1]); mode 3 with (record, func_089AB370(&args[0]), func_08969244(&args[1]),
// func_08969244(&args[2])). All value objects are destroyed after (v0 = 3 for other modes).
void func_08A01A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const auto call_slot = [&](std::uint32_t record, std::initializer_list<std::uint32_t> arguments) {
        ctx.gpr[2] = word(rt, record);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x23Cu);
        call_set(rt, ctx, ctx.gpr[3], arguments);
    };
    if (mode == 2u) {
        call_set(rt, ctx, 0x08A01338u, {frame.at(8u)});
        call_set(rt, ctx, 0x08969244u, {frame.at(4u), args});
        call_set(rt, ctx, 0x08969244u, {frame.at(0u), args + 4u});
        const std::uint32_t record = word(rt, frame.at(8u));
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8B14u});
        call_slot(record, {record, 0u, word(rt, frame.at(4u)), word(rt, frame.at(0u))});
        call_set(rt, ctx, kDestroy, {frame.at(0u)});
        call_set(rt, ctx, kDestroy, {frame.at(4u)});
        call_set(rt, ctx, kDestroy, {frame.at(8u)});
        return;
    }
    ctx.gpr[2] = 3u;
    ctx.gpr[4] = frame.at(0u);
    if (mode != 3u) return;
    call_set(rt, ctx, 0x08A01338u, {frame.at(0u)});
    call_set(rt, ctx, 0x089AB370u, {frame.at(4u), args});
    call_set(rt, ctx, 0x08969244u, {frame.at(8u), args + 4u});
    call_set(rt, ctx, 0x08969244u, {frame.at(0xCu), args + 8u});
    const std::uint32_t record = word(rt, frame.at(0u));
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8B14u});
    call_slot(record, {record, word(rt, frame.at(4u)), word(rt, frame.at(8u)), word(rt, frame.at(0xCu))});
    call_set(rt, ctx, kDestroy, {frame.at(0xCu)});
    call_set(rt, ctx, kDestroy, {frame.at(8u)});
    call_set(rt, ctx, kDestroy, {frame.at(4u)});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}

// func_088FA42C(self, point, key, f12 = radius) -> 1 when the point (x +0, z +8) lies within radius of one of the segments
// that func_088FAD1C(&range, self->+4, &key, point) selects: halfword node indices [begin, end) into 16-byte nodes
// {x, z at +8, link at +0xC}, each segment running from a node to node + 1, or back by the negative link. The test is the
// squared distance to the start when the projection is negative, to the end when it overruns, else the perpendicular one.
void func_088FA42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t point = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    const float radius2 = psp_mul(ctx.fpr[12], ctx.fpr[12]);
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(4u), key);
    const float pz = float_at(rt, point + 8u);
    const float px = float_at(rt, point);
    put(rt, frame.at(0u), key);
    const std::uint32_t range = frame.at(0x20u);
    call_set(rt, ctx, 0x088FAD1Cu, {range, word(rt, self + 4u), frame.at(0u), point});
    const std::uint32_t nodes = word(rt, range);
    std::uint32_t cursor = word(rt, range + 4u);
    const std::uint32_t end = word(rt, range + 8u);
    put(rt, frame.at(8u), nodes);
    put(rt, frame.at(0xCu), cursor);
    put(rt, frame.at(0x10u), end);
    ctx.gpr[2] = 0u;
    while (cursor < end && nodes != 0u) {
        const std::uint32_t node = nodes + (sext16(half_u(rt, cursor)) << 4);
        cursor += 2u;
        const std::uint32_t link = sext16(half_u(rt, node + 0xCu));
        std::uint32_t other = node + 0x10u;
        if (signed_of(link) < 0) other += link << 4;
        const float nz = float_at(rt, node + 8u);
        const float nx = float_at(rt, node);
        const float oz = float_at(rt, other + 8u);
        const float ox = float_at(rt, other);
        const float dz = pz - nz;
        const float dx = px - nx;
        const float sz = oz - nz;
        const float sx = ox - nx;
        const float near2 = psp_mul(dx, dx) + psp_mul(dz, dz);
        const float length2 = psp_mul(sx, sx) + psp_mul(sz, sz);
        const float dot = psp_mul(sx, dx) + psp_mul(sz, dz);
        bool inside;
        if (dot < 0.0f) {
            inside = near2 < radius2;
        } else {
            const float ex = px - ox;
            const float ez = pz - oz;
            float distance2 = psp_mul(ex, ex) + psp_mul(ez, ez);
            const float start2 = psp_mul(dx, dx) + psp_mul(dz, dz);
            if (!(length2 < dot)) distance2 = start2 - psp_mul(dot, dot) / length2;
            inside = distance2 < radius2;
        }
        if (inside) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

} // namespace hot_batch_111
} // namespace gt::game

#define HOT111(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_111::name, gt::ReturnKind::kind)
// HOT111(0x0885BFE0u, func_0885BFE0, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x0888D8B4u, func_0888D8B4, Word);
HOT111(0x0892F95Cu, func_0892F95C, DoubleWord);
HOT111(0x089CF6D4u, func_089CF6D4, Word);
HOT111(0x089D6284u, func_089D6284, Word);
HOT111(0x08C3DDB8u, func_08C3DDB8, Word);
// HOT111(0x088AAE08u, func_088AAE08, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x088D3DA0u, func_088D3DA0, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x08875138u, func_08875138, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x088DE24Cu, func_088DE24C, Word);
HOT111(0x08A78490u, func_08A78490, Word);
HOT111(0x08AFEC54u, func_08AFEC54, Word);
HOT111(0x08811D30u, func_08811D30, Word);
// HOT111(0x08A32F9Cu, func_08A32F9C, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08BA0D34u, func_08BA0D34, Word);
// HOT111(0x08C48EC4u, func_08C48EC4, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08841AD0u, func_08841AD0, Word);
// HOT111(0x0888EA68u, func_0888EA68, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x08AF8AB0u, func_08AF8AB0, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x0883B950u, func_0883B950, DoubleWord);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08B1C958u, func_08B1C958, DoubleWord);
// HOT111(0x08B33790u, func_08B33790, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x08861B20u, func_08861B20, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08957B30u, func_08957B30, Word);
HOT111(0x08BEED1Cu, func_08BEED1C, Word);
HOT111(0x08BF2D6Cu, func_08BF2D6C, Word);
HOT111(0x08C158D8u, func_08C158D8, Word);
HOT111(0x08849D4Cu, func_08849D4C, Word);
HOT111(0x0884A29Cu, func_0884A29C, Word);
HOT111(0x0884A3ACu, func_0884A3AC, Word);
HOT111(0x0892063Cu, func_0892063C, Word);
HOT111(0x088451C0u, func_088451C0, Word);
HOT111(0x0888BC00u, func_0888BC00, Word);
HOT111(0x088D06ACu, func_088D06AC, Word);
HOT111(0x08832FC4u, func_08832FC4, Word);
HOT111(0x088697E8u, func_088697E8, Word);
// HOT111(0x088A6FB4u, func_088A6FB4, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08A28884u, func_08A28884, Word);
// HOT111(0x08ACD2ECu, func_08ACD2EC, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x0882AA38u, func_0882AA38, Word);
// HOT111(0x0885F550u, func_0885F550, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x0888EC10u, func_0888EC10, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08B8A4C0u, func_08B8A4C0, Word);
HOT111(0x08849848u, func_08849848, Word);
HOT111(0x089EE2E4u, func_089EE2E4, Word);
// HOT111(0x08B2EEB8u, func_08B2EEB8, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x0883074Cu, func_0883074C, Word);
HOT111(0x0880F6B0u, func_0880F6B0, Word);
// HOT111(0x08AD31E0u, func_08AD31E0, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x08B1A284u, func_08B1A284, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x0881BAC0u, func_0881BAC0, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x088EA900u, func_088EA900, Word);
HOT111(0x08ABE178u, func_08ABE178, Word);
HOT111(0x08AEFE28u, func_08AEFE28, Word);
HOT111(0x08B66E88u, func_08B66E88, Float);
HOT111(0x089EFF1Cu, func_089EFF1C, Word);
HOT111(0x089F00BCu, func_089F00BC, Word);
HOT111(0x0881F8A8u, func_0881F8A8, Word);
HOT111(0x0897876Cu, func_0897876C, Word);
HOT111(0x08A7E99Cu, func_08A7E99C, Word);
HOT111(0x08AB8430u, func_08AB8430, Word);
HOT111(0x08AF8EB4u, func_08AF8EB4, Word);
HOT111(0x0888C0E4u, func_0888C0E4, Word);
HOT111(0x088D32E0u, func_088D32E0, Word);
// HOT111(0x089AEF20u, func_089AEF20, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08B66FBCu, func_08B66FBC, Float);
// HOT111(0x08937918u, func_08937918, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08AF07C8u, func_08AF07C8, Word);
// HOT111(0x08B82420u, func_08B82420, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x0888E62Cu, func_0888E62C, Word);
// HOT111(0x088B4178u, func_088B4178, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT111(0x08BBEA18u, func_08BBEA18, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x088F9CE4u, func_088F9CE4, Word);
HOT111(0x08A1FE40u, func_08A1FE40, Word);
// HOT111(0x08B93734u, func_08B93734, Word);  // batch 111: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT111(0x08A01A24u, func_08A01A24, Word);
HOT111(0x088FA42Cu, func_088FA42C, Word);
