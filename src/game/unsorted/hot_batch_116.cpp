// Hot batch 116: functions of 185-239 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_116 {
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

// A count of 24-byte elements from a byte distance: the compiler's `sra 3` and multiply by the inverse of 3 (0xAAAAAAAB).
inline std::uint32_t elements24(std::uint32_t distance) { return static_cast<std::uint32_t>(signed_of(distance) >> 3) * 0xAAAAAAABu; }

// func_088464EC(target, key, flag, value): with flag == 0, func_088458D0(temp) gives an object (checked against 0x08C9CA90)
// whose func_08845798 / func_08894170 result `bits` is a bit set; func_08A98C1C(list, 2) and func_08A98C1C(list2, sum of
// four table[byte] values of bits->+0, & 0xFF) build two value objects; for every bit k < 15 set in `bits`, func_08A85B1C(temp, k)
// builds a value that goes to the next slot of list2's table (+0x14); list2's first slot gets list2's +0xC, and `target`
// gets list's value. With flag != 0, func_08A98674(list2, value) and the object of func_088458D0(key) (func_0889417C gives a
// bit set) are used the other way round: each entry of the list's table (+0x14), through its virtual slot 0x3C, sets a bit
// (when 0 <= index < 15) of the bit set. Value objects are destroyed after.
void func_088464EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (flag == 0u) {
        const std::uint32_t holder = frame.at(8u);
        call_set(rt, ctx, 0x088458D0u, {holder});
        std::uint32_t object = word(rt, holder);
        checked(rt, ctx, object, 0x08C9CA90u);
        object = call_set(rt, ctx, 0x08845798u, {object});
        call_set(rt, ctx, kDestroy, {holder});
        const std::uint32_t bits = call_set(rt, ctx, 0x08894170u, {object});
        const std::uint32_t first = frame.at(0x10u);
        call_set(rt, ctx, 0x08A98C1Cu, {first, 2u});
        const std::uint32_t packed = word(rt, bits);
        const std::uint32_t sum = byte_u(rt, 0x08CBEC60u + (packed >> 24)) + byte_u(rt, 0x08CBEC60u + ((packed >> 16) & 0xFFu)) +
                                  byte_u(rt, 0x08CBEC60u + ((packed >> 8) & 0xFFu)) + byte_u(rt, 0x08CBEC60u + (packed & 0xFFu));
        const std::uint32_t second = frame.at(0xCu);
        call_set(rt, ctx, 0x08A98C1Cu, {second, sum & 0xFFu});
        std::uint32_t used = 0u;
        for (std::int32_t bit = 0; bit < 15; ++bit) {
            const std::uint32_t mask = word(rt, bits + (static_cast<std::uint32_t>(bit) >> 5) * 4u);
            if (((mask >> (bit & 31)) & 1u) == 0u) continue;
            const std::uint32_t temp = frame.at(4u);
            const std::uint32_t slot = word(rt, word(rt, second) + 0x14u) + used * 4u;
            ++used;
            call_set(rt, ctx, 0x08A85B1Cu, {temp, static_cast<std::uint32_t>(bit)});
            ctx.gpr[5] = word(rt, temp);
            if (slot != temp) call_set(rt, ctx, kAssign, {slot, word(rt, temp)});
            call_set(rt, ctx, kDestroy, {temp});
        }
        const std::uint32_t head = word(rt, word(rt, first) + 0x14u);
        if (head != second) call_set(rt, ctx, kAssign, {head, word(rt, second)});
        if (target != first) call_set(rt, ctx, kAssign, {target, word(rt, first)});
        call_set(rt, ctx, kDestroy, {second});
        call_set(rt, ctx, kDestroy, {first});
        return;
    }
    const std::uint32_t list = frame.at(0xCu);
    call_set(rt, ctx, 0x08A98674u, {list, value});
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, 0x088458D0u, {holder, key});
    std::uint32_t object = word(rt, holder);
    checked(rt, ctx, object, 0x08C9CA90u);
    object = call_set(rt, ctx, 0x08845798u, {object});
    call_set(rt, ctx, kDestroy, {holder});
    const std::uint32_t bits = call_set(rt, ctx, 0x0889417Cu, {object});
    const std::uint32_t table = word(rt, word(rt, list) + 0x14u);
    if (word(rt, table) != 0u) {
        const std::uint32_t second = frame.at(0x10u);
        call_set(rt, ctx, 0x08A98674u, {second, table});
        std::int32_t index = 0;
        for (;;) {
            const std::uint32_t holder2 = word(rt, second);
            checked(rt, ctx, holder2, 0x08C9CB60u);
            const std::int32_t count = signed_of(word(rt, holder2 + 0x18u) - word(rt, holder2 + 0x14u)) >> 2;
            const std::uint32_t offset = static_cast<std::uint32_t>(index) * 4u;
            const std::int32_t current = index++;
            ctx.gpr[4] = 0x08C9CA54u;
            ctx.gpr[5] = offset;
            if (!(current < count)) break;
            const std::uint32_t entry = word(rt, word(rt, word(rt, second) + 0x14u) + offset);
            checked(rt, ctx, entry, 0x08C9CA54u);
            const std::uint32_t bit = call_virtual_at(rt, ctx, entry, 0x3Cu, {entry});
            if (signed_of(bit) < 0) continue;
            if (!(signed_of(bit) < 15)) continue;
            const std::uint32_t address = bits + (static_cast<std::uint32_t>(signed_of(bit) >> 5) << 2);
            put(rt, address, word(rt, address) | (1u << (bit & 31)));
        }
        call_set(rt, ctx, kDestroy, {second});
    }
    call_set(rt, ctx, kDestroy, {list});
}
//REG HOT116(0x088464ECu, func_088464EC, Word);
// func_08861CB0(self, source, a, b, c, d): returns 1 at once when the flag word at self->+0 + 0x3FD4 was already set,
// otherwise sets it (ll/sc) and runs: s3 = the record at self->+0's +0x5AC (func_088D65DC(frame) first); `item` =
// (a != 0) func_08890478(func_088915C8(func_0885EE70(self)), frame) with item = frame, else `source` or, when that is zero,
// s3 + 0xC; unless d != 0, func_088D776C(s3, 1) and func_08B1C3E4(-1, -1); func_0889365C(self->+0 + 0x600); the bytes
// +0x3FD8 / +0x3FD9 / +0x3FDA = a / b / c. The block self->+0 + 0x5CC takes func_08918298 and func_08917AB0 (c set) or
// func_08918298(block, a, b, 0) (unless +0x5A9 is clear and +0x5AA set), then func_08918170(block, func_088D6358(item),
// func_088D4C9C(item)). The word +0x1C of the record at +0x5B8 and the byte s3 + 0x624 are set from func_088D4C9C(item); then
// func_088D78EC(s3, item) decides the return value (1 unless +0x5A9 is clear and +0x5AA set), a long run of renderer-state calls
// follows and the byte +0x5A9 is cleared when that call returned non-zero; func_088D5CF0(frame) ends it.
void func_08861CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t a = ctx.gpr[6] & 0xFFu;
    const std::uint32_t b = ctx.gpr[7] & 0xFFu;
    const std::uint32_t c = ctx.gpr[8] & 0xFFu;
    const std::uint32_t d = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t flag = word(rt, self) + 0x3FD4u;
    const std::uint32_t previous = word(rt, flag);
    ctx.gpr[4] = 1u;
    ctx.gpr[3] = flag;
    ctx.gpr[5] = previous;
    ctx.gpr[2] = 1u;
    if (previous == 0u) put(rt, flag, 1u);
    std::uint32_t result = 1u;
    if (previous != 0u) {
        ctx.gpr[2] = result;
        return;
    }
    const std::uint32_t s3 = word(rt, word(rt, self) + 0x5ACu);
    call_set(rt, ctx, 0x088D65DCu, {frame.at(0u)});
    std::uint32_t item;
    ctx.gpr[2] = s3 + 0xCu;
    if (a != 0u) {
        const std::uint32_t chain = call_set(rt, ctx, 0x0885EE70u, {self});
        const std::uint32_t second = call_set(rt, ctx, 0x088915C8u, {chain});
        call_set(rt, ctx, 0x08890478u, {second, frame.at(0u)});
        item = frame.at(0u);
    } else {
        item = source != 0u ? source : s3 + 0xCu;
    }
    if (d == 0u) {
        call_set(rt, ctx, 0x088D776Cu, {s3, 1u});
        call_set(rt, ctx, 0x08B1C3E4u, {0xFFFFFFFFu, 0xFFFFFFFFu});
    }
    call_set(rt, ctx, 0x0889365Cu, {word(rt, self) + 0x600u});
    put8(rt, word(rt, self) + 0x3FD8u, a);
    put8(rt, word(rt, self) + 0x3FD9u, b);
    put8(rt, word(rt, self) + 0x3FDAu, c);
    std::uint32_t base;
    if (c != 0u) {
        call_set(rt, ctx, 0x08918298u, {word(rt, self) + 0x5CCu, 0u, 1u, 1u});
        call_set(rt, ctx, 0x08917AB0u, {word(rt, self) + 0x5CCu});
        base = word(rt, self);
    } else {
        base = word(rt, self);
        if (byte_u(rt, base + 0x5A9u) != 0u || byte_u(rt, base + 0x5AAu) == 0u) {
            call_set(rt, ctx, 0x08918298u, {base + 0x5CCu, a, b, 0u});
            base = word(rt, self);
        }
    }
    const std::uint32_t scale = call_set(rt, ctx, 0x088D6358u, {item});
    const std::uint32_t block = base + 0x5CCu;
    const std::uint32_t extent = call_set(rt, ctx, 0x088D4C9Cu, {item});
    call_set(rt, ctx, 0x08918170u, {block, scale, extent});
    if (call_set(rt, ctx, 0x088D4C9Cu, {item}) != 0u) {
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x1Cu, 0u);
    } else {
        const std::uint32_t state = word(rt, self);
        std::uint32_t value = 2u;
        if (a == 0u) value = c < 1u ? 1u : 0u;
        put(rt, word(rt, state + 0x5B8u) + 0x1Cu, value);
    }
    const std::uint32_t flagged = call_set(rt, ctx, 0x088D4C9Cu, {item});
    put8(rt, s3 + 0x624u, flagged);
    ctx.gpr[6] = (c != 0u || a != 0u || b != 0u) ? 1u : 0u;
    const std::uint32_t state = word(rt, self);
    std::uint32_t changed = 1u;
    result = 1u;
    if (byte_u(rt, state + 0x5A9u) != 0u || byte_u(rt, state + 0x5AAu) == 0u) {
        const std::uint32_t outcome = call_set(rt, ctx, 0x088D78ECu, {s3, item});
        result = outcome;
        changed = outcome;
    }
    call_set(rt, ctx, 0x088D776Cu, {s3, 0u});
    call_set(rt, ctx, 0x088D7744u, {s3, 0u});
    call_set(rt, ctx, 0x088D6910u, {s3, 0u, 0u});
    const std::uint32_t hook = word(rt, word(rt, self) + 0x3F9C);
    if (hook != 0u) call_set(rt, ctx, 0x0890F878u, {hook});
    // The jal delay slots copy the previous call's v0 into s1 / s0 / a0 / a1, so each call's arguments are the results of the calls before it.
    const std::uint32_t lead = call_set(rt, ctx, 0x088CBFD4u, {});
    const std::uint32_t first = call_set(rt, ctx, 0x0885EDBCu, {self});
    const std::uint32_t second = call_set(rt, ctx, 0x0885EDD4u, {self});
    call_set(rt, ctx, 0x088CB958u, {lead, first, second});
    const std::uint32_t lead2 = call_set(rt, ctx, 0x088CA548u, {});
    const std::uint32_t third = call_set(rt, ctx, 0x088DA4B0u, {s3});
    const std::uint32_t fourth = call_set(rt, ctx, 0x0885EDBCu, {self});
    call_set(rt, ctx, 0x088CA2B0u, {lead2, third, fourth});
    const std::uint32_t renderer = call_set(rt, ctx, 0x08916474u, {});
    ctx.gpr[5] = word(rt, s3 + 0x61Cu);
    call_set(rt, ctx, 0x08916188u, {renderer, word(rt, s3 + 0x61Cu)});
    const std::uint32_t target = call_set(rt, ctx, 0x088C7F64u, {});
    call_set(rt, ctx, 0x088C95C4u, {target, word(rt, s3 + 0x61Cu), item});
    if (changed != 0u) put8(rt, word(rt, self) + 0x5A9u, 0u);
    call_set(rt, ctx, 0x088D5CF0u, {frame.at(0u)});
    ctx.gpr[2] = result;
}
//REG HOT116(0x08861CB0u, func_08861CB0, Word);
// func_0886D88C(self): a destructor: self->+0 = 0x08CBF748, func_0886BF44(self, 0), func_0886B600() four times; with
// o = self->+4 (nothing more when 0): the three objects at o->+0x1C / +0x20 / +0x24 are each destroyed through their virtual
// slot 0x2C and cleared; func_08B95120 / func_08B93D3C / func_08B95080 on the three sub-blocks at o + 0x1F4 / +0x1DC / +0x1C4
// (second argument their +8); the shared pointer at o->+0x13C is released; o->+0xF8 gets two vtable words in turn
// (0x08CC6850, 0x08CC6838), func_08B28DC4(o + 0x110) and the shared pointers at o->+0x108 / +0x100 / +0xF4 are released;
// func_0887D4EC(o + 0x88), func_0890AFC0(o + 0x60), +0x38 = 0x08CBFF08, func_0887C8E8(o + 0x3C), +0x38 = 0x08CD7118,
// o->+0 = 0x08CBF758, func_08B1FD74(o), and a tail jump to the free func_08B64654(o).
void func_0886D88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[5] = 0u;
    put(rt, self, 0x08CBF748u);
    call_set(rt, ctx, 0x0886BF44u, {self, 0u});
    for (int index = 0; index < 4; ++index) call_set(rt, ctx, 0x0886B600u, {});
    const std::uint32_t o = word(rt, self + 4u);
    if (o == 0u) return;
    for (std::uint32_t index = 0u; index < 3u; ++index) {
        const std::uint32_t entry = word(rt, o + index * 4u + 0x1Cu);
        if (entry == 0u) continue;
        call_virtual_v1(rt, ctx, entry, 0x2Cu, {entry});
        put(rt, o + index * 4u + 0x1Cu, 0u);
    }
    const std::uint32_t block_a = o + 0x1F4u;
    const std::uint32_t inner = o + 0xF8u;
    call_set(rt, ctx, 0x08B95120u, {block_a, word(rt, block_a + 8u)});
    const std::uint32_t block_b = o + 0x1DCu;
    call_set(rt, ctx, 0x08B93D3Cu, {block_b, word(rt, block_b + 8u)});
    const std::uint32_t block_c = o + 0x1C4u;
    call_set(rt, ctx, 0x08B95080u, {block_c, word(rt, block_c + 8u)});
    const std::uint32_t shared = word(rt, o + 0x13Cu);
    put(rt, o + 0xF8u, 0x08CC6850u);
    if (shared != 0u) release_shared(rt, ctx, shared);
    put(rt, inner, 0x08CC6838u);
    call_set(rt, ctx, 0x08B28DC4u, {inner + 0x18u});
    const std::uint32_t first = word(rt, inner + 0x10u);
    if (first != 0u) release_shared(rt, ctx, first);
    const std::uint32_t second = word(rt, inner + 8u);
    if (second != 0u) release_shared(rt, ctx, second);
    const std::uint32_t third = word(rt, o + 0xF4u);
    if (third != 0u) release_shared(rt, ctx, third);
    call_set(rt, ctx, 0x0887D4ECu, {o + 0x88u});
    call_set(rt, ctx, 0x0890AFC0u, {o + 0x60u});
    put(rt, o + 0x38u, 0x08CBFF08u);
    call_set(rt, ctx, 0x0887C8E8u, {o + 0x3Cu});
    put(rt, o + 0x38u, 0x08CD7118u);
    put(rt, o, 0x08CBF758u);
    call_set(rt, ctx, 0x08B1FD74u, {o});
    ctx.gpr[4] = o;
    tail_call(ctx, 0x08B64654u, {o});
}
//REG HOT116(0x0886D88Cu, func_0886D88C, Word);
// func_08883E74(self, owner, name, flag): returns 0 at once when self->+0x16F4 is set. Otherwise a frame string object is built from
// `name` (func_08B7C830(frame, name, 8), a terminating byte at frame +8); unless func_08B7B8C4(frame, 0x08CA0634) is non-zero,
// func_08B7BB6C(frame, name + 8), r = func_08B77BFC(frame) and name = func_0886C638(owner, r). With a non-empty name the
// object h = func_0886B080(owner) gives index = h's virtual slot 0x18(h, name) (negative: return 0); item = slot 0x28(h, index) (zero:
// slot 0x24(h, index), return 0). self->+0x16F0 = index; slot 0x2C(h, index, 0); two slot 0x20 calls (a2 = 0 and 1) give the
// arguments of a new 0x48-byte object (func_0889F1DC(new, item, first, second, flag & 0xFF, 1)) that a 0x10-byte control block
// (vtable 0x08CBF458, counts 1 / 1) wraps in a shared pointer stored at self + 0x4C / +0x50 (the old one released);
// func_0889E258(self->+0x4C, frame +8) fills a pair whose block is released; self->+0x16F4 = 1; slot 0x30 of h is then called with
// (h, index, func_0886C31C(owner, r)); when self->+0x16F4 is still set func_0886BC14(owner) runs and bit 4 of self->+0x16EC is
// set. The return value is self->+0x16F4.
void func_08883E74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    std::uint32_t name = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x50u);
    if (word(rt, self + 0x16F4u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t text = frame.at(0x10u);
    call_set(rt, ctx, 0x08B7C830u, {text, name, 8u});
    put8(rt, frame.at(0x18u), 0u);
    std::uint32_t extra = 0u;
    if (call_set(rt, ctx, 0x08B7B8C4u, {text, 0x08CA0634u}) == 0u) {
        call_set(rt, ctx, 0x08B7BB6Cu, {text, name + 8u});
        const std::uint32_t width = call_set(rt, ctx, 0x08B77BFCu, {text});
        extra = width;
        name = call_set(rt, ctx, 0x0886C638u, {owner, width});
    }
    if (name == 0u || sext8(byte_u(rt, name)) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t handler = call_set(rt, ctx, 0x0886B080u, {owner});
    const std::uint32_t index = call_virtual_at(rt, ctx, handler, 0x18u, {handler, name});
    if (signed_of(index) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t item = call_virtual_at(rt, ctx, handler, 0x28u, {handler, index});
    if (item == 0u) {
        call_virtual_at(rt, ctx, handler, 0x24u, {handler, index});
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, self + 0x16F0u, index);
    call_virtual_at(rt, ctx, handler, 0x2Cu, {handler, index, 0u});
    const std::uint32_t first = call_virtual_at(rt, ctx, handler, 0x20u, {handler, word(rt, self + 0x16F0u), 0u});
    const std::uint32_t second = call_virtual_at(rt, ctx, handler, 0x20u, {handler, word(rt, self + 0x16F0u), 1u});
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0x48u});
    call_set(rt, ctx, 0x0889F1DCu, {object, item, first, second, flag, 1u});
    put(rt, frame.at(0u), object);
    const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, control, 0x08CBF458u);
    put(rt, control + 4u, 1u);
    put(rt, control + 0xCu, object);
    put(rt, control + 8u, 1u);
    put(rt, frame.at(4u), control);
    call_set(rt, ctx, 0x08B8E380u, {frame.at(4u), object, object});
    const std::uint32_t old_pointer = word(rt, self + 0x4Cu);
    const std::uint32_t new_pointer = word(rt, frame.at(0u));
    const std::uint32_t old_control = word(rt, self + 0x50u);
    const std::uint32_t new_control = word(rt, frame.at(4u));
    put(rt, frame.at(0u), old_pointer);
    put(rt, self + 0x50u, new_control);
    put(rt, self + 0x4Cu, new_pointer);
    put(rt, frame.at(4u), old_control);
    if (old_control != 0u) release_shared(rt, ctx, old_control);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call_set(rt, ctx, 0x0889E258u, {word(rt, self + 0x4Cu), frame.at(8u)});
    const std::uint32_t pair_control = word(rt, frame.at(0xCu));
    if (pair_control != 0u) release_shared(rt, ctx, pair_control);
    put(rt, self + 0x16F4u, 1u);
    const std::uint32_t table = word(rt, handler);
    const std::uint32_t current = word(rt, self + 0x16F0u);
    const std::uint32_t slot = word(rt, table + 0x30u);
    ctx.gpr[2] = table;
    const std::uint32_t argument = call_set(rt, ctx, 0x0886C31Cu, {owner, extra});
    call_set(rt, ctx, slot, {handler, current, argument});
    const std::uint32_t state = word(rt, self + 0x16F4u);
    if (state == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x0886BC14u, {owner});
    put(rt, self + 0x16ECu, word(rt, self + 0x16ECu) | 4u);
    ctx.gpr[2] = word(rt, self + 0x16F4u);
}
//REG HOT116(0x08883E74u, func_08883E74, Word);
// func_08888D58(self, out, query, source): count = self's virtual slot 0x2C; nothing when it is <= 0. With index = source's
// slot 0x64 and the query object's slot 0x1C(query, index) giving an object whose slot 0x50 fills a frame result (flag byte
// frame +0), the value frame +0x20 is source's slot 0x60 (0 when that is 1 and the flag is set) or 4 when index is outside
// 0 <= index < count. A frame array of `count` 20-byte entries {4 floats} is filled by self's slot 0x30(result, self, i)
// (frame result: object +0, shared control block +4; the object's slot 0x2C gives the floats at result +8, zeros without an
// object; the control block released), handed to func_089101F0(out->+0, array, count), then for each i: slot 0x30 again, and
// with query's slot 0x1C(query, i) = item (non-zero) and its slot 0x18(query, i) = source2, a3 = (source2's slots 0xC and
// 0x10 both non-zero), the result object's slot 0x18(object, out, item, a3, i == index, array byte +0x11, frame +0x20)
// is called; the result's control block is released again.
void func_08888D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t query = ctx.gpr[6];
    const std::uint32_t source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t fp = frame.at(0u);
    put(rt, fp + 0x24u, out);
    const std::uint32_t count = call_virtual_at(rt, ctx, self, 0x2Cu, {self});
    if (signed_of(count) <= 0) return;
    const std::uint32_t index = call_virtual_at(rt, ctx, source, 0x64u, {source});
    if (signed_of(index) < 0 || !(signed_of(index) < signed_of(count))) {
        put(rt, fp + 0x20u, 4u);
    } else {
        std::uint32_t flag = 0u;
        const std::uint32_t item = call_virtual_at(rt, ctx, query, 0x1Cu, {query, index});
        if (item != 0u) {
            call_virtual_at(rt, ctx, item, 0x50u, {item, fp});
            flag = byte_u(rt, fp);
        }
        const std::uint32_t value = call_virtual_at(rt, ctx, source, 0x60u, {source});
        put(rt, fp + 0x20u, value);
        if (flag != 0u) {
            std::uint32_t adjusted = value;
            if ((value ^ 1u) == 0u) adjusted = 0u;
            put(rt, fp + 0x20u, adjusted);
        }
    }
    GuestFrame array_frame(ctx, (count * 20u + 0x22u) & ~0xFu);
    const std::uint32_t array = array_frame.at(0u);
    std::uint32_t entry = array;
    for (std::uint32_t i = 0u; i != count; ++i, entry += 0x14u) {
        call_virtual_at(rt, ctx, self, 0x30u, {fp, self, i});
        const std::uint32_t object = word(rt, fp);
        std::uint32_t floats[4] = {0u, 0u, 0u, 0u};
        if (object != 0u) {
            call_virtual_at(rt, ctx, object, 0x2Cu, {object, fp + 8u});
            floats[0] = word(rt, fp + 8u);
            floats[1] = word(rt, fp + 0xCu);
            floats[2] = word(rt, fp + 0x10u);
            floats[3] = word(rt, fp + 0x14u);
        }
        const std::uint32_t control = word(rt, fp + 4u);
        put(rt, entry, floats[0]);
        put(rt, entry + 4u, floats[1]);
        put(rt, entry + 8u, floats[2]);
        put(rt, entry + 0xCu, floats[3]);
        if (control != 0u) release_shared(rt, ctx, control);
    }
    call_set(rt, ctx, 0x089101F0u, {word(rt, word(rt, fp + 0x24u)), array, count});
    entry = array;
    for (std::uint32_t i = 0u; i != count; ++i, entry += 0x14u) {
        call_virtual_at(rt, ctx, self, 0x30u, {fp, self, i});
        const std::uint32_t object = word(rt, fp);
        ctx.gpr[2] = object;
        if (object != 0u) {
            const std::uint32_t item = call_virtual_at(rt, ctx, query, 0x1Cu, {query, i});
            if (item != 0u) {
                const std::uint32_t second = call_virtual_at(rt, ctx, query, 0x18u, {query, i});
                std::uint32_t a3 = 0u;
                if (call_virtual_at(rt, ctx, second, 0xCu, {second}) != 0u) a3 = call_virtual_at(rt, ctx, second, 0x10u, {second}) != 0u ? 1u : 0u;
                const std::uint32_t result_object = word(rt, fp);
                const std::uint32_t tail = sext8(byte_u(rt, entry + 0x11u));
                const std::uint32_t destination = word(rt, fp + 0x24u);
                const std::uint32_t value = word(rt, fp + 0x20u);
                const std::uint32_t same = (i ^ index) < 1u ? 1u : 0u;
                call_virtual_v1(rt, ctx, result_object, 0x18u, {result_object, destination, item, a3, same, tail, value});
            }
        }
        const std::uint32_t control = word(rt, fp + 4u);
        if (control != 0u) release_shared(rt, ctx, control);
    }
}
//REG HOT116(0x08888D58u, func_08888D58, Word);
// func_0888C750(self, item, ids, count, flag_a, flag_b): returns count when func_088D4C50(self->+4 + 4) is zero and nothing is
// listed (count <= 0), otherwise the result of the last func_088936A4. With func_088D4C50 zero, each of the count halfwords in ids
// goes through func_088DA588(self->+4, id): a non-zero object gets virtual slot 0x1C(obj, 1) then slot 0x18(obj, 1, 0x50) (flag_b
// set) or slots 0x1C and 0x14 (obj, 1). Then for each id (until one reports): func_088DA788 / func_088DA1C8(self->+4, id) give
// a record and an entry; func_088D6204(item) classifies: 0 (entry must pass func_088DB2C8; a code is derived from the block
// func_0889410C(self->+0xC): kinds 1 / 2 / 4 compare its words +4 / +8 / +0xC against a limit and give 0x23 / 0x24 / 0x25), 1 (the entry of
// func_088DA1C8 unless flag_a) or other (code = entry) -> where a code is reported func_08917AB0(self->+8) and
// func_088D6FD4(self->+4, func_088D5938(item), mode, code) run and the loop ends after the record; the log record {0xD, id, code,
// flag_a byte, func_088DB2C8 byte} goes to func_088936A4(self->+0x18, record).
void func_0888C750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t item = ctx.gpr[5];
    const std::uint32_t ids = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    const std::uint32_t flag_a = ctx.gpr[8] & 0xFFu;
    const std::uint32_t flag_b = ctx.gpr[9] & 0xFFu;
    ctx.gpr[8] = flag_a;
    GuestFrame frame(ctx, 0xE0u);
    put(rt, frame.at(0xA0u), item);
    put(rt, frame.at(0xA4u), count);
    put(rt, frame.at(0xA8u), flag_a);
    const std::uint32_t pending = call_set(rt, ctx, 0x088D4C50u, {word(rt, self + 4u) + 4u});
    ctx.gpr[2] = count;
    if (pending == 0u) {
        if (!(signed_of(count) > 0)) return;
        std::uint32_t cursor = ids;
        std::uint32_t done = 0u;
        for (;;) {
            const std::uint32_t object = call_set(rt, ctx, 0x088DA588u, {word(rt, self + 4u), half_u(rt, cursor)});
            if (object != 0u) {
                if (flag_b != 0u) {
                    call_virtual_v1(rt, ctx, object, 0x1Cu, {object, 1u});
                    call_virtual_at(rt, ctx, object, 0x18u, {object, 1u, 0x50u});
                } else {
                    call_virtual_at(rt, ctx, object, 0x1Cu, {object, 1u});
                    call_virtual_at(rt, ctx, object, 0x14u, {object, 1u});
                }
            }
            ctx.gpr[2] = count;
            ++done;
            cursor += 2u;
            if (count == done) break;
        }
    } else if (signed_of(count) <= 0) {
        return;
    }
    std::uint32_t cursor = ids;
    bool reported = false;
    std::uint32_t logged = 0u;
    for (;;) {
        const std::uint32_t id = half_u(rt, cursor);
        const std::uint32_t record = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        const std::uint32_t entry = call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), id});
        const std::uint32_t kind = call_set(rt, ctx, 0x088D6204u, {word(rt, frame.at(0xA0u))});
        std::uint32_t code = entry;
        const auto report = [&](std::uint32_t mode, std::uint32_t reported_code) {
            call_set(rt, ctx, 0x08917AB0u, {word(rt, self + 8u)});
            reported = true;
            const std::uint32_t owner = word(rt, self + 4u);
            const std::uint32_t source = call_set(rt, ctx, 0x088D5938u, {word(rt, frame.at(0xA0u))});
            call_set(rt, ctx, 0x088D6FD4u, {owner, source, mode, reported_code});
        };
        if (kind == 0u) {
            if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id}) != 0u) {
                code = 0xFFFFFFFFu;
                if (word(rt, frame.at(0xA8u)) != 0u) {
                    report(0u, code);
                } else {
                    code = call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), id});
                    const std::uint32_t block = call_set(rt, ctx, 0x0889410Cu, {word(rt, self + 0xCu)});
                    const std::uint32_t shape = word(rt, block);
                    if (shape == 1u) {
                        const std::uint32_t limit = word(rt, record + 0x74u);
                        if (!(word(rt, block + 4u) < limit)) code = 0x23u;
                        else if (!(word(rt, block + 8u) < limit)) code = 0x24u;
                        else if (!(word(rt, block + 0xCu) < limit)) code = 0x25u;
                    } else if (shape == 2u) {
                        const std::uint32_t limit = call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), id}) + 1u;
                        if (!(signed_of(word(rt, block + 4u)) < signed_of(limit))) code = 0x23u;
                        else if (!(signed_of(word(rt, block + 8u)) < signed_of(limit))) code = 0x24u;
                        else if (!(signed_of(word(rt, block + 0xCu)) < signed_of(limit))) code = 0x25u;
                    } else if (shape == 4u) {
                        const std::uint32_t source = word(rt, word(rt, self + 4u) + 0x628u);
                        std::uint32_t limit = 0u;
                        if (source != 0u) limit = call_set(rt, ctx, 0x088CEF68u, {source});
                        if (!(signed_of(limit) < signed_of(word(rt, block + 4u)))) code = 0x23u;
                        else if (!(signed_of(limit) < signed_of(word(rt, block + 8u)))) code = 0x24u;
                        else if (!(signed_of(limit) < signed_of(word(rt, block + 0xCu)))) code = 0x25u;
                    }
                    report(1u, code);
                }
            }
        } else if (kind == 1u) {
            code = 0xFFFFFFFFu;
            std::uint32_t mode = 0u;
            if (word(rt, frame.at(0xA8u)) == 0u) {
                code = call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), id});
                mode = 1u;
            }
            report(mode, code);
        }
        put(rt, frame.at(0u), 0xDu);
        ++logged;
        put(rt, frame.at(0xCu), code);
        put(rt, frame.at(8u), id);
        put8(rt, frame.at(0x10u), word(rt, frame.at(0xA8u)));
        const std::uint32_t flag = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id});
        put8(rt, frame.at(0x11u), flag);
        call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
        if (word(rt, frame.at(0xA4u)) == logged) return;
        if (reported) return;
        cursor += 2u;
    }
}
//REG HOT116(0x0888C750u, func_0888C750, Word);
// func_08892C28(self, count_a, count_b): returns count_a. With na = the halfword at self->+4 and nb = the halfword at +6, each
// 32-byte slot is run through func_08ABE178(0x481907FF, slot, 0x20): slots i < na at +0x988 and +0xA08, then the slots of the
// halfword index lists at +0x560 (nb entries, slots at +0xA08) and +0x558 (na entries, slots at +0x988), each relative to the first
// entry's word (+0x28 or +0x18 of the index): a word other than 0x481907FF has the first one subtracted. Then, for each i <
// count_a: the word at self + 0x18 + 4i goes to func_08ABE090(word, self + 0x20i + 0x788, 0x20) (when it is 0x481907FF
// the byte there is cleared instead) and the words +0x28 / +0x38 / +0x48 to the slots +0x808 / +0x888 / +0x908; and for each
// j < count_b the words at self + 0x58 + 0x40i + 4j (+0, +0x200, +0x100) go to the slots self + 0x200i + 0x20j + 0x1288 / +0x1A88 / +0xA88.
void func_08892C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count_a = ctx.gpr[5];
    const std::uint32_t count_b = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    constexpr std::uint32_t kNone = 0x481907FFu;
    put(rt, frame.at(4u), count_a);
    put(rt, frame.at(8u), count_b);
    const auto fill = [&](std::uint32_t pattern, std::uint32_t slot) { call_set(rt, ctx, 0x08ABE178u, {pattern, slot, 0x20u}); };
    const auto store = [&](std::uint32_t source, std::uint32_t slot) { call_set(rt, ctx, 0x08ABE090u, {source, slot, 0x20u}); };
    const auto adjusted = [&](std::uint32_t entry, std::uint32_t base) { return entry != kNone ? entry - base : entry; };
    if (signed_of(sext16(half_u(rt, self + 4u))) > 0) {
        for (std::int32_t index = 0; index < signed_of(sext16(half_u(rt, self + 4u))); ++index) {
            fill(kNone, self + 0x988u + static_cast<std::uint32_t>(index) * 0x20u);
            fill(kNone, self + 0xA08u + static_cast<std::uint32_t>(index) * 0x20u);
        }
    }
    if (signed_of(sext16(half_u(rt, self + 6u))) > 0) {
        const std::uint32_t head = sext16(half_u(rt, self + 0x560u));
        const std::uint32_t base = word(rt, self + (head << 2) + 0x28u);
        fill(kNone, self + (head << 5) + 0xA08u);
        for (std::int32_t index = 1; index < signed_of(sext16(half_u(rt, self + 6u))); ++index) {
            const std::uint32_t entry = sext16(half_u(rt, self + 0x562u + static_cast<std::uint32_t>(index - 1) * 2u));
            const std::uint32_t value = word(rt, self + (entry << 2) + 0x28u);
            fill(adjusted(value, base), self + (entry << 5) + 0xA08u);
        }
    }
    if (signed_of(sext16(half_u(rt, self + 4u))) > 0) {
        const std::uint32_t head = sext16(half_u(rt, self + 0x558u));
        const std::uint32_t base = word(rt, self + (head << 2) + 0x18u);
        fill(kNone, self + (head << 5) + 0x988u);
        for (std::int32_t index = 1; index < signed_of(sext16(half_u(rt, self + 4u))); ++index) {
            const std::uint32_t entry = sext16(half_u(rt, self + 0x55Au + static_cast<std::uint32_t>(index - 1) * 2u));
            const std::uint32_t value = word(rt, self + (entry << 2) + 0x18u);
            fill(adjusted(value, base), self + (entry << 5) + 0x988u);
        }
    }
    if (signed_of(count_a) > 0) {
        for (std::uint32_t index = 0u; index != count_a; ++index) {
            const std::uint32_t row = self + index * 4u;
            const std::uint32_t slot = index << 5;
            const std::uint32_t first = word(rt, row + 0x18u);
            if (first == kNone) put8(rt, self + index * 0x20u + 0x788u, 0u);
            else store(first, self + slot + 0x788u);
            store(word(rt, row + 0x28u), self + slot + 0x808u);
            store(word(rt, row + 0x38u), self + slot + 0x888u);
            store(word(rt, row + 0x48u), self + slot + 0x908u);
            if (signed_of(count_b) > 0) {
                const std::uint32_t inner = self + 0x50u + index * 0x40u + 8u;
                for (std::uint32_t j = 0u; j != count_b; ++j) {
                    store(word(rt, inner + j * 4u), self + index * 0x200u + 0x1288u + j * 0x20u);
                    store(word(rt, inner + j * 4u + 0x200u), self + index * 0x200u + 0x1A88u + j * 0x20u);
                    store(word(rt, inner + j * 4u + 0x100u), self + index * 0x200u + 0xA88u + j * 0x20u);
                }
            }
        }
    }
    ctx.gpr[2] = count_a;
}
//REG HOT116(0x08892C28u, func_08892C28, Word);
// func_0896E9F4(self, other): the copy-assignment self = other after func_089BD1E0(self). Nothing more when other is 0. Copies
// the 16 bytes at +0x70 (unless same), runs func_089B8BA0(self + 0x84), then for every node of other's chain (+0x90, next at +0x54)
// func_08A77444(frame, node), func_08A0FD80(frame +4, frame) and func_089B8C6C(self + 0x84, frame word +4, self->+0x90),
// destroying both frame objects; the ten floats at +0xA4..+0xC8; func_08B855E0(self + 0xCC, other + 0xCC); the bit fields of
// the word at +0xE0 one at a time (bits 0-14, 16, 17, 18-20, 21-22, 23, 24, 25) and of +0xE4 (bits 0-1 copied, bit 2 set, bits
// 3-4 cleared); when bit 17 is set and other has a block at +0xD0 it is copied to self's (created by func_08968FE4 when
// missing; func_08A5BAD8 for the value object at +0x24); the floats +0xD4..+0xDC; func_0896890C(self); finally the 0x32 bytes at
// the +0x80 blocks when both exist.
void func_0896E9F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, 0x089BD1E0u, {});
    if (other == 0u) return;
    const auto insert = [](std::uint32_t destination, std::uint32_t source, unsigned position, unsigned size) {
        const std::uint32_t mask = ((1u << size) - 1u) << position;
        return (destination & ~mask) | ((source << position) & mask);
    };
    const auto extract = [](std::uint32_t source, unsigned position, unsigned size) { return (source >> position) & ((1u << size) - 1u); };
    const std::uint32_t inner = self + 0x70u;
    const std::uint32_t other_inner = other + 0x70u;
    if (inner != other_inner) {
        const std::uint32_t a = word(rt, other_inner + 0xCu);
        const std::uint32_t b = word(rt, other + 0x70u);
        const std::uint32_t c = word(rt, other_inner + 4u);
        const std::uint32_t d = word(rt, other_inner + 8u);
        put(rt, inner + 0xCu, a);
        put(rt, self + 0x70u, b);
        put(rt, inner + 4u, c);
        put(rt, inner + 8u, d);
    }
    const std::uint32_t list = self + 0x84u;
    call_set(rt, ctx, 0x089B8BA0u, {list});
    std::uint32_t node = word(rt, other + 0x90u);
    const std::uint32_t extra = word(rt, list + 0xCu);
    while (node != 0u) {
        call_set(rt, ctx, 0x08A77444u, {frame.at(0u), node});
        call_set(rt, ctx, 0x08A0FD80u, {frame.at(4u), frame.at(0u)});
        call_set(rt, ctx, kDestroy, {frame.at(0u)});
        call_set(rt, ctx, 0x089B8C6Cu, {list, word(rt, frame.at(4u)), extra});
        call_set(rt, ctx, kDestroy, {frame.at(4u)});
        node = word(rt, node + 0x54u);
        ctx.gpr[2] = node;
    }
    for (std::uint32_t offset = 0xA4u; offset <= 0xC8u; offset += 4u) put(rt, self + offset, word(rt, other + offset));
    call_set(rt, ctx, 0x08B855E0u, {self + 0xCCu, other + 0xCCu});
    std::uint32_t flags = insert(word(rt, self + 0xE0u), word(rt, other + 0xE0u), 0u, 4u);
    put(rt, self + 0xE0u, flags);
    static constexpr unsigned kFields[][2] = {{4u, 4u}, {8u, 4u}, {12u, 1u}, {13u, 1u}, {14u, 1u}};
    for (const auto &field : kFields) {
        flags = insert(flags, extract(word(rt, other + 0xE0u), field[0], field[1]), field[0], field[1]);
        put(rt, self + 0xE0u, flags);
    }
    flags = insert(flags, half_u(rt, other + 0xE2u), 16u, 1u);
    put(rt, self + 0xE0u, flags);
    flags = insert(flags, extract(word(rt, other + 0xE0u), 17u, 1u), 17u, 1u);
    put(rt, self + 0xE0u, flags);
    if ((flags & 0x20000u) != 0u) {
        const std::uint32_t source = word(rt, other + 0xD0u);
        if (source != 0u) {
            std::uint32_t target = word(rt, self + 0xD0u);
            if (target == 0u) {
                target = call_set(rt, ctx, 0x08968FE4u, {});
                put(rt, self + 0xD0u, target);
            }
            const std::uint32_t current = word(rt, other + 0xD0u);
            if (target != current) {
                const std::uint32_t a = word(rt, current);
                const std::uint32_t b = word(rt, current + 4u);
                const std::uint32_t c = word(rt, current + 8u);
                const std::uint32_t d = word(rt, current + 0xCu);
                put(rt, target, a);
                put(rt, target + 4u, b);
                put(rt, target + 8u, c);
                put(rt, target + 0xCu, d);
            }
            if (target + 0x10u != current + 0x10u) {
                const std::uint32_t a = word(rt, current + 0x1Cu);
                const std::uint32_t b = word(rt, current + 0x10u);
                const std::uint32_t c = word(rt, current + 0x14u);
                const std::uint32_t d = word(rt, current + 0x18u);
                put(rt, target + 0x1Cu, a);
                put(rt, target + 0x10u, b);
                put(rt, target + 0x14u, c);
                put(rt, target + 0x18u, d);
            }
            put(rt, target + 0x20u, word(rt, current + 0x20u));
            if (target + 0x24u != current + 0x24u) call_set(rt, ctx, kAssign, {target + 0x24u, word(rt, current + 0x24u)});
            std::uint32_t tail = insert(word(rt, target + 0x28u), word(rt, current + 0x28u), 0u, 1u);
            put(rt, target + 0x28u, tail);
            tail = insert(tail, extract(word(rt, current + 0x28u), 1u, 1u), 1u, 1u);
            put(rt, target + 0x28u, tail);
            flags = word(rt, self + 0xE0u);
        }
    }
    const std::uint32_t d4 = word(rt, other + 0xD4u);
    const std::uint32_t d8 = word(rt, other + 0xD8u);
    const std::uint32_t dc = word(rt, other + 0xDCu);
    put(rt, self + 0xD4u, d4);
    put(rt, self + 0xD8u, d8);
    put(rt, self + 0xDCu, dc);
    ctx.gpr[4] = self;
    std::uint32_t second = word(rt, self + 0xE4u);
    static constexpr unsigned kMoreFields[][2] = {{18u, 1u}, {19u, 1u}, {20u, 1u}};
    for (const auto &field : kMoreFields) {
        flags = insert(flags, extract(word(rt, other + 0xE0u), field[0], field[1]), field[0], field[1]);
        put(rt, self + 0xE0u, flags);
    }
    second = insert(second, word(rt, other + 0xE4u), 0u, 1u);
    put(rt, self + 0xE4u, second);
    second = insert(second, extract(word(rt, other + 0xE4u), 1u, 1u), 1u, 1u);
    second |= 4u;
    second = insert(second, 0u, 3u, 2u);
    put(rt, self + 0xE4u, second);
    flags = insert(flags, extract(word(rt, other + 0xE0u), 21u, 2u), 21u, 2u);
    put(rt, self + 0xE0u, flags);
    flags = insert(flags, extract(word(rt, other + 0xE0u), 23u, 1u), 23u, 1u);
    put(rt, self + 0xE0u, flags);
    flags = insert(flags, byte_u(rt, other + 0xE3u), 24u, 1u);
    put(rt, self + 0xE0u, flags);
    flags = insert(flags, extract(word(rt, other + 0xE0u), 25u, 1u), 25u, 1u);
    put(rt, self + 0xE0u, flags);
    call_set(rt, ctx, 0x0896890Cu, {self});
    const std::uint32_t target_block = word(rt, self + 0x80u);
    if (target_block == 0u) return;
    const std::uint32_t source_block = word(rt, other + 0x80u);
    ctx.gpr[2] = source_block;
    if (source_block == 0u) return;
    for (std::uint32_t offset = 0u; offset < 0x30u; offset += 4u) put(rt, target_block + offset, word(rt, source_block + offset));
    const std::uint32_t last = half_u(rt, source_block + 0x30u);
    put16(rt, target_block + 0x30u, last);
    ctx.gpr[2] = last;
}
//REG HOT116(0x0896E9F4u, func_0896E9F4, Word);
// func_08A4AFB4(target, count, holder): nothing when count <= 0. With object = holder->+0 (checked against 0x08CAED38): its
// virtual slot 0xC(frame +0x1C, object) fills a string, func_08B0CE58(frame +0x70) and func_08B10204(frame +0xB0, string, 1)
// build a 60-byte record that is copied through frame +0x30 to frame +0x70. A value object at frame +0x20 starts as
// func_08A76AAC(.., 0x08CF6ED8) (or func_08A44D9C(temp, record +0x20, record +0x10, 1) when the record's first word is zero),
// func_08A6BA44 prepares the map object at frame +0x24 and func_08A6BBE8(map, key) looks up the six keys 0x08CAEFD8,
// 0x08CAEFE0, 0x08CAEFE8, 0x08CAEFF4 (each assigned a func_08A85B1C value of the record words +0 / +4 / +8 / +0x18),
// 0x08CAF014 (assigned the value object) and 0x08CAF00C (assigned func_08A85B1C(.., 0)); the map is copied to `target`
// (func_08A76ABC) and the temporaries destroyed; the string released (func_08B60804 / func_08B8542C unless the empty rep).
void func_08A4AFB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::int32_t count = signed_of(ctx.gpr[5]);
    const std::uint32_t holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x110u);
    if (count <= 0) return;
    const std::uint32_t object = checked(rt, ctx, word(rt, holder), 0x08CAED38u);
    call_virtual_v1(rt, ctx, object, 0xCu, {frame.at(0x1Cu), object});
    const std::uint32_t string = word(rt, frame.at(0x1Cu));
    const std::uint32_t record = frame.at(0x70u);
    const std::uint32_t built = frame.at(0xB0u);
    call_set(rt, ctx, 0x08B0CE58u, {record});
    call_set(rt, ctx, 0x08B10204u, {built, string, 1u});
    const std::uint32_t staging = frame.at(0x30u);
    for (std::uint32_t offset = 0u; offset < 0x30u; offset += 4u) put(rt, staging + offset, word(rt, built + offset));
    for (std::uint32_t offset = 0x30u; offset < 0x3Cu; offset += 4u) put(rt, staging + offset, word(rt, built + offset));
    for (std::uint32_t offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, record + offset, word(rt, staging + offset));
    const std::uint32_t value = frame.at(0x20u);
    ctx.gpr[6] = word(rt, frame.at(0x64u));
    ctx.gpr[7] = frame.at(0x60u);
    call_set(rt, ctx, 0x08A76AACu, {value, 0x08CF6ED8u});
    const std::uint32_t map = frame.at(0x24u);
    if (word(rt, record) == 0u) {
        const std::uint32_t temp = frame.at(0x28u);
        call_set(rt, ctx, 0x08A44D9Cu, {temp, word(rt, frame.at(0x90u)), word(rt, frame.at(0x80u)), 1u});
        keep_temporary(rt, ctx, value, temp);
    }
    call_set(rt, ctx, 0x08A6BA44u, {map});
    // The key's slot, then the value built in `temp` from the frame word at `source_offset` (0: from zero).
    const auto assign_value = [&](std::uint32_t key, std::uint32_t temp, std::uint32_t source_offset) {
        const std::uint32_t slot = call_set(rt, ctx, 0x08A6BBE8u, {map, key});
        call_set(rt, ctx, 0x08A85B1Cu, {temp, source_offset == 0u ? 0u : word(rt, frame.at(source_offset))});
        keep_temporary(rt, ctx, slot, temp);
    };
    assign_value(0x08CAEFD8u, frame.at(0x18u), 0x70u);
    assign_value(0x08CAEFE0u, frame.at(0x14u), 0x74u);
    assign_value(0x08CAEFE8u, frame.at(0x10u), 0x78u);
    assign_value(0x08CAEFF4u, frame.at(0xCu), 0x88u);
    const std::uint32_t found = call_set(rt, ctx, 0x08A6BBE8u, {map, 0x08CAF014u});
    if (found != value) call_set(rt, ctx, kAssign, {found, word(rt, value)});
    assign_value(0x08CAF00Cu, frame.at(8u), 0u);
    const std::uint32_t copy = frame.at(4u);
    call_set(rt, ctx, 0x08A76ABCu, {copy, map});
    keep_temporary(rt, ctx, target, copy);
    call_set(rt, ctx, kDestroy, {map});
    call_set(rt, ctx, kDestroy, {value});
    ctx.gpr[2] = 0x08D74FD0u;
    release_string(rt, ctx, frame.at(0x1Cu), 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}
//REG HOT116(0x08A4AFB4u, func_08A4AFB4, Word);
// func_08AA3F44(target, source): a path normaliser over the libstdc++ strings: `source` (a holder of a string pointer) is trimmed
// of leading and trailing white space (form feed, space, line feed, carriage return, tab, vertical tab), copied
// (func_08C309B4 range constructor into frame +4) with every '\' replaced by '/' (func_08C30980 with the two characters
// in frame +3 / +2), given a trailing '/' when it is longer than one character and lacks it (func_08B864EC reserves; the
// representation's count and length words are written by hand), then assigned to the string at target + 4 (func_08B86674(string,
// length, 0), func_08B79BAC copies the characters). The copy-on-write strings are made unique with func_08B88FF8 where
// their count word is not negative; the temporary is released (func_08B60804 / func_08B8542C unless the empty rep 0x08D74FD0).
void func_08AA3F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const auto is_space = [](std::uint32_t c) { return c == 0xCu || c == 0x20u || c == 0xAu || c == 0xDu || c == 9u || c == 0xBu; };
    const std::uint32_t temp = frame.at(4u);
    const std::uint32_t begin = word(rt, holder);
    const std::uint32_t length = word(rt, begin - 0xCu);
    const std::uint32_t end = begin + length;
    std::uint32_t start = begin;
    if (is_space(sext8(byte_u(rt, begin)))) {
        if (begin != end) {
            std::uint32_t scan = begin + 1u;
            for (;;) {
                start = scan;
                if (!is_space(sext8(byte_u(rt, scan)))) break;
                if (scan == end) break;
                ++scan;
            }
        }
    }
    std::uint32_t stop = end;
    if (end != start) {
        const std::uint32_t total = end - start;
        std::uint32_t seen = 0u;
        for (;;) {
            const std::uint32_t last = sext8(byte_u(rt, stop - 1u));
            ++seen;
            const std::uint32_t before = stop - 1u;
            if (!is_space(last)) break;
            if (total == seen) break;
            stop = before;
        }
    }
    call_set(rt, ctx, 0x08C309B4u, {temp, start, stop, frame.at(1u)});
    std::uint32_t text = word(rt, temp);
    std::uint32_t kept = text;
    if (signed_of(word(rt, text - 4u)) >= 0) {
        call_set(rt, ctx, 0x08B88FF8u, {temp});
        text = word(rt, temp);
        kept = text;
        if (signed_of(word(rt, text - 4u)) >= 0) {
            call_set(rt, ctx, 0x08B88FF8u, {temp});
            text = word(rt, temp);
        }
    }
    put8(rt, frame.at(3u), 0x5Cu);
    put8(rt, frame.at(2u), 0x2Fu);
    call_set(rt, ctx, 0x08C30980u, {kept, text + word(rt, text - 0xCu), frame.at(3u), frame.at(2u)});
    text = word(rt, temp);
    std::uint32_t size = word(rt, text - 0xCu);
    if (!(signed_of(size) < 2)) {
        std::uint32_t last = size - 1u;
        if (signed_of(word(rt, text - 0xCu + 8u)) >= 0) {
            call_set(rt, ctx, 0x08B88FF8u, {temp});
            text = word(rt, temp);
        }
        if (sext8(byte_u(rt, text + last)) != 0x2Fu) {
            size = word(rt, text - 0xCu);
            const std::uint32_t grown = size + 1u;
            bool reserve = word(rt, text - 0xCu + 4u) < grown;
            if (!reserve && signed_of(word(rt, text - 0xCu + 8u)) > 0) reserve = true;
            if (reserve) {
                call_set(rt, ctx, 0x08B864ECu, {temp, grown});
                text = word(rt, temp);
            }
            put8(rt, text + word(rt, text - 0xCu), 0x2Fu);
            const std::uint32_t current = word(rt, temp);
            put(rt, current - 4u, 0u);
            put(rt, current - 0xCu, grown);
            put8(rt, current + grown, 0u);
            text = word(rt, temp);
        }
    }
    const std::uint32_t destination = target + 4u;
    call_set(rt, ctx, 0x08B86674u, {destination, word(rt, text - 0xCu), 0u});
    text = word(rt, temp);
    std::uint32_t source_begin = text;
    if (signed_of(word(rt, text - 4u)) >= 0) {
        call_set(rt, ctx, 0x08B88FF8u, {temp});
        text = word(rt, temp);
        source_begin = text;
        if (signed_of(word(rt, text - 4u)) >= 0) {
            call_set(rt, ctx, 0x08B88FF8u, {temp});
            text = word(rt, temp);
        }
    }
    std::uint32_t destination_text = word(rt, destination);
    const std::uint32_t source_end = text + word(rt, text - 0xCu);
    if (signed_of(word(rt, destination_text - 4u)) >= 0) {
        call_set(rt, ctx, 0x08B88FF8u, {destination});
        destination_text = word(rt, destination);
    }
    call_set(rt, ctx, 0x08B79BACu, {destination_text, source_begin, source_end - source_begin});
    ctx.gpr[2] = 0x08D74FD0u;
    release_string(rt, ctx, temp, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}
//REG HOT116(0x08AA3F44u, func_08AA3F44, Word);
// func_08AAADC8(self, key): func_08AAA2C0(); with owner = self->+4 (every use checked against 0x08CB30B0): func_08A71B00(owner,
// word 0x08CDF808), `mode` = owner's virtual slot 0x118, func_08C312E4(owner, 0), func_08C34248(self, func_08A76ABC(temp, key)),
// `old` = slot 0x118 again. The vector at self + 0x38 (begin +0x38, end +0x3C) is resized to the count at self->+0x34 (the
// shared-string result held in frame +4 is released); the tree nodes of self + 0x20 are walked (func_08B5F100) and each
// handed to func_08B855E0(begin + node->+0x14 * 4, node + 0x10); func_08A71B00(owner, count); for each of the count
// entries func_08B88F44(frame +8, begin + 4i) and func_08C312EC(owner, it) (the string released); then slot 0x114(owner, mode),
// func_08C312E4(owner, old) and func_08AA97D4(self).
void func_08AAADC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    constexpr std::uint32_t kMessage = 0x08CB30B0u;
    constexpr std::uint32_t kEmptyRep = 0x08D74FD0u;
    call_set(rt, ctx, 0x08AAA2C0u, {});
    std::uint32_t owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    call_set(rt, ctx, 0x08A71B00u, {owner, word(rt, 0x08CDF808u)});
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    const std::uint32_t mode = call_virtual_at(rt, ctx, owner, 0x118u, {owner});
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    call_set(rt, ctx, 0x08C312E4u, {owner, 0u});
    const std::uint32_t temp = frame.at(0xCu);
    call_set(rt, ctx, 0x08A76ABCu, {temp, key});
    call_set(rt, ctx, 0x08C34248u, {self, temp});
    call_set(rt, ctx, kDestroy, {temp});
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    const std::uint32_t old = call_virtual_at(rt, ctx, owner, 0x118u, {owner});
    const std::uint32_t tree = self + 0x20u;
    const std::uint32_t vector = self + 0x38u;
    const std::uint32_t begin = word(rt, self + 0x38u);
    const std::uint32_t count = word(rt, tree + 0x14u);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - begin) >> 2);
    const std::uint32_t result = frame.at(4u);
    put(rt, result, kEmptyRep + 0xCu);
    if (count < size) {
        call_set(rt, ctx, 0x08C311E0u, {vector, begin + count * 4u, word(rt, self + 0x3Cu)});
    } else {
        call_set(rt, ctx, 0x08C30F28u, {vector, word(rt, self + 0x3Cu), count - size, result});
    }
    release_string(rt, ctx, result, kEmptyRep, 0x08B8542Cu, frame.at(1u));
    const std::uint32_t head = self + 0x24u;
    std::uint32_t node = word(rt, tree + 0xCu);
    if (node != head) {
        for (;;) {
            call_set(rt, ctx, 0x08B855E0u, {word(rt, self + 0x38u) + (word(rt, node + 0x14u) << 2), node + 0x10u});
            node = call_set(rt, ctx, 0x08B5F100u, {node});
            if (node == head) break;
        }
    }
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    call_set(rt, ctx, 0x08A71B00u, {owner, count});
    if (count != 0u) {
        const std::uint32_t element = frame.at(8u);
        for (std::uint32_t index = 0u; index != count; ++index) {
            owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
            call_set(rt, ctx, 0x08B88F44u, {element, word(rt, self + 0x38u) + index * 4u});
            call_set(rt, ctx, 0x08C312ECu, {owner, element});
            release_string(rt, ctx, element, kEmptyRep, 0x08B8542Cu, frame.at(0u));
        }
    }
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    call_virtual_at(rt, ctx, owner, 0x114u, {owner, mode});
    owner = checked(rt, ctx, word(rt, self + 4u), kMessage);
    call_set(rt, ctx, 0x08C312E4u, {owner, old});
    call_set(rt, ctx, 0x08AA97D4u, {self});
}
//REG HOT116(0x08AAADC8u, func_08AAADC8, Word);
// func_08AB398C(target, key, holder): builds a string in frame +0x1C and assigns it to `target` (func_08A6D2D4(target, string)). A
// frame string from the literal 0x08CB3CF4 (func_08B85EB8) and func_08A6F4D8(frame +0x18, key, string) give an object whose
// table (+0x14, begin +0x14, end +0x18, 4-byte entries; each use checked against 0x08CB3C64 / 0x08CB3CF8 / 0x08CB3D04)
// supplies the first entry's virtual slot 0xC(string, entry) fill of the result string. For index i = 1 .. count - 1: entry
// i is copied (func_08A6C410, frame +0x20); a second frame string from the literal 0x08CB3D00 with
// func_08A6F4D8(frame +0x24, entry, string) gives another table; the first entry's virtual slot 0x3C picks a position j in the
// holder's own table, whose entry's slot 0xC fills a frame string that func_08B89054 appends to the result; and when that second
// table has at least 2 entries its second entry's slot 0xC fills one more string that is appended. Every string is
// released (func_08B60804 / func_08B8542C unless the empty rep 0x08D74FD0), the value objects destroyed. Returns `target`.
void func_08AB398C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    constexpr std::uint32_t kEmptyRep = 0x08D74FD0u;
    constexpr std::uint32_t kRelease = 0x08B8542Cu;
    constexpr std::uint32_t kNullFirst = 0x08CB3C64u;
    constexpr std::uint32_t kNullHolder = 0x08CB3D04u;
    put(rt, frame.at(0x30u), target);
    const std::uint32_t first_string = frame.at(0x14u);
    call_set(rt, ctx, 0x08B85EB8u, {first_string, 0x08CB3CF4u, frame.at(6u)});
    call_set(rt, ctx, 0x08A6F4D8u, {frame.at(0x18u), key, first_string});
    release_string(rt, ctx, first_string, kEmptyRep, kRelease, frame.at(4u));
    const std::uint32_t result = frame.at(0x1Cu);
    const std::uint32_t entry_copy = frame.at(0x20u);
    const std::uint32_t second_object = frame.at(0x24u);
    std::uint32_t object = word(rt, frame.at(0x18u));
    std::uint32_t first = checked(rt, ctx, word(rt, word(rt, object + 0x14u)), kNullFirst);
    call_virtual_v1(rt, ctx, first, 0xCu, {result, first});
    for (std::int32_t index = 1;; ++index) {
        std::uint32_t table = checked(rt, ctx, word(rt, frame.at(0x18u)), kNullHolder);
        const std::int32_t count = signed_of(word(rt, table + 0x18u) - word(rt, table + 0x14u)) >> 2;
        if (!(index < count)) break;
        object = word(rt, frame.at(0x18u));
        call_set(rt, ctx, 0x08A6C410u, {entry_copy, word(rt, object + 0x14u) + static_cast<std::uint32_t>(index) * 4u});
        std::uint32_t entry = checked(rt, ctx, word(rt, entry_copy), 0x08CB3CF8u);
        const std::uint32_t literal = frame.at(0x10u);
        call_set(rt, ctx, 0x08B85EB8u, {literal, 0x08CB3D00u, frame.at(5u)});
        call_set(rt, ctx, 0x08A6F4D8u, {second_object, entry, literal});
        release_string(rt, ctx, literal, kEmptyRep, kRelease, frame.at(3u));
        object = word(rt, second_object);
        entry = checked(rt, ctx, word(rt, word(rt, object + 0x14u)), kNullFirst);
        const std::int32_t position = signed_of(call_virtual_at(rt, ctx, entry, 0x3Cu, {entry}));
        table = checked(rt, ctx, word(rt, holder), kNullHolder);
        const std::int32_t holder_count = signed_of(word(rt, table + 0x18u) - word(rt, table + 0x14u)) >> 2;
        if (position < holder_count) {
            const std::uint32_t other = word(rt, holder);
            entry = checked(rt, ctx, word(rt, word(rt, other + 0x14u) + static_cast<std::uint32_t>(position) * 4u), kNullFirst);
            const std::uint32_t piece = frame.at(0xCu);
            call_virtual_at(rt, ctx, entry, 0xCu, {piece, entry});
            call_set(rt, ctx, 0x08B89054u, {result, piece});
            release_string(rt, ctx, piece, kEmptyRep, kRelease, frame.at(2u));
        }
        table = checked(rt, ctx, word(rt, second_object), kNullHolder);
        const std::uint32_t second_count = (word(rt, table + 0x18u) - word(rt, table + 0x14u)) >> 2;
        if (!(second_count < 2u)) {
            object = word(rt, second_object);
            entry = checked(rt, ctx, word(rt, word(rt, object + 0x14u) + 4u), kNullFirst);
            const std::uint32_t piece = frame.at(8u);
            call_virtual_at(rt, ctx, entry, 0xCu, {piece, entry});
            call_set(rt, ctx, 0x08B89054u, {result, piece});
            release_string(rt, ctx, piece, kEmptyRep, kRelease, frame.at(1u));
        }
        call_set(rt, ctx, kDestroy, {second_object});
        call_set(rt, ctx, kDestroy, {entry_copy});
    }
    call_set(rt, ctx, 0x08A6D2D4u, {word(rt, frame.at(0x30u)), result});
    release_string(rt, ctx, result, kEmptyRep, kRelease, frame.at(0u));
    call_set(rt, ctx, kDestroy, {frame.at(0x18u)});
    ctx.gpr[2] = word(rt, frame.at(0x30u));
}
//REG HOT116(0x08AB398Cu, func_08AB398C, Word);
// func_08ADE5A4(dt) (float in f12): under func_08AEB808 / func_08AEB82C (lock, unlock), the entry list (a frame array of
// word(0x08D4069C) pointers, filled by func_08ADE268, `count` entries). Pass 1: an entry whose float +0x34 is > 0 and
// (byte +0xE9 > 0 or flag byte +0x8F bit 2 clear) advances its float +0x30 by dt (cleared with +0x34 when it reached it),
// func_08ADDF88(entry, 0) after the halfword +0x2A = +0x2E + trunc(ratio * (half +0x2C - half +0x2E)). Pass 2: every entry
// func_08AD8DB4(entry + 0x70, entry + 0x56), the state bytes +0x8C / +0x8D move to +0x8E / +0x8F, float +0xF8 to +0xFC,
// +0xF8, +0x56 and +0x8C cleared. Pass 3: entries whose +0x8F bit 0 is set (state = +0x8E) run the callbacks of the table
// at +0x90: slot 0 (state bit 0), func_08ADE0E4(entry, bit 4 of flags) (state bit 1; ends the entry), slot 0x18(entry, 0)
// (bit 3), slot 0x10(entry, bit 2 of flags) (bit 2); then, when the half +0xEA is >= 0, func_08ADE52C(entry; f12 = dt) and on
// success slot 0x14(entry, byte +0xE8) and a locked clear of bits 0x42 of the word +0xC (when byte +0xE8 is set); last slot
// 8(entry; f12 = dt) when set.
void func_08ADE5A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float dt = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    call_set(rt, ctx, 0x08AEB808u, {});
    const std::uint32_t capacity = word(rt, 0x08D4069Cu);
    GuestFrame list(ctx, (capacity * 4u + 0xFu) & ~0xFu);
    const std::uint32_t array = list.at(0u);
    const std::int32_t count = signed_of(call_set(rt, ctx, 0x08ADE268u, {array}));
    call_set(rt, ctx, 0x08AEB82Cu, {});
    if (count > 0) {
        std::uint32_t remaining = static_cast<std::uint32_t>(count);
        std::uint32_t cursor = array;
        do {
            const std::uint32_t entry = word(rt, cursor);
            --remaining;
            cursor += 4u;
            if (!(float_at(rt, entry + 0x34u) <= 0.0f)) {
                const bool skip = signed_of(sext8(byte_u(rt, entry + 0xE9u))) <= 0 && (byte_u(rt, entry + 0x8Fu) & 4u) != 0u;
                if (!skip) {
                    call_set(rt, ctx, 0x08AEB808u, {});
                    const float step = float_at(rt, entry + 0x30u);
                    const float limit = float_at(rt, entry + 0x34u);
                    const float moved = step + dt;
                    put_float(rt, entry + 0x30u, moved);
                    float ratio = moved / limit;
                    if (limit <= moved) {
                        put(rt, entry + 0x30u, 0u);
                        put(rt, entry + 0x34u, 0u);
                        ratio = float_at(rt, 0x08CB6074u);
                    }
                    const std::uint32_t low = half_u(rt, entry + 0x2Eu);
                    const std::uint32_t span = sext16(half_u(rt, entry + 0x2Cu)) - sext16(low);
                    const float scaled = psp_mul(ratio, static_cast<float>(signed_of(span)));
                    const std::uint32_t moved_by = ctx.fpu_float_to_word(scaled, 1u);
                    put16(rt, entry + 0x2Au, low + moved_by);
                    call_set(rt, ctx, 0x08ADDF88u, {entry, 0u});
                    call_set(rt, ctx, 0x08AEB82Cu, {});
                }
            }
        } while (remaining != 0u);
    }
    call_set(rt, ctx, 0x08AEB808u, {});
    if (count > 0) {
        std::uint32_t remaining = static_cast<std::uint32_t>(count);
        std::uint32_t cursor = array;
        do {
            const std::uint32_t entry = word(rt, cursor);
            --remaining;
            cursor += 4u;
            call_set(rt, ctx, 0x08AD8DB4u, {entry + 0x70u, entry + 0x56u});
            const std::uint32_t old_state = byte_u(rt, entry + 0x8Cu);
            const std::uint32_t old_flags = byte_u(rt, entry + 0x8Du);
            const std::uint32_t saved = word(rt, entry + 0xF8u);
            put8(rt, entry + 0x8Eu, old_state);
            put8(rt, entry + 0x8Fu, old_flags);
            put(rt, entry + 0xFCu, saved);
            put(rt, entry + 0xF8u, 0u);
            put16(rt, entry + 0x56u, 0u);
            put8(rt, entry + 0x8Cu, 0u);
        } while (remaining != 0u);
    }
    call_set(rt, ctx, 0x08AEB82Cu, {});
    if (count <= 0) return;
    std::uint32_t remaining = static_cast<std::uint32_t>(count);
    std::uint32_t cursor = array;
    for (;;) {
        const std::uint32_t entry = word(rt, cursor);
        const std::uint32_t flags = sext8(byte_u(rt, entry + 0x8Fu));
        const std::uint32_t state = sext8(byte_u(rt, entry + 0x8Eu));
        ctx.gpr[2] = flags & 1u;
        if ((flags & 1u) != 0u) {
            ctx.gpr[2] = state & 1u;
            if ((state & 1u) != 0u) {
                const std::uint32_t callback = word(rt, word(rt, entry + 0x90u));
                ctx.gpr[2] = callback;
                if (callback != 0u) call_set(rt, ctx, callback, {entry});
            }
            ctx.gpr[2] = state & 2u;
            if ((state & 2u) != 0u) {
                call_set(rt, ctx, 0x08ADE0E4u, {entry, (flags >> 4) & 1u});
            } else {
                ctx.gpr[2] = state & 8u;
                if ((state & 8u) != 0u) {
                    const std::uint32_t callback = word(rt, word(rt, entry + 0x90u) + 0x18u);
                    ctx.gpr[2] = callback;
                    ctx.gpr[4] = entry;
                    if (callback != 0u) call_set(rt, ctx, callback, {entry, 0u});
                }
                ctx.gpr[2] = state & 4u;
                if ((state & 4u) != 0u) {
                    const std::uint32_t table = word(rt, entry + 0x90u);
                    const std::uint32_t callback = word(rt, table + 0x10u);
                    ctx.gpr[2] = table;
                    ctx.gpr[3] = callback;
                    ctx.gpr[5] = (flags >> 2) & 1u;
                    if (callback != 0u) call_set(rt, ctx, callback, {entry, (flags >> 2) & 1u});
                }
                const std::uint32_t slot = sext16(half_u(rt, entry + 0xEAu));
                ctx.gpr[2] = slot;
                ctx.gpr[4] = entry;
                if (signed_of(slot) >= 0) {
                    const std::uint32_t ok = call_setf(rt, ctx, 0x08ADE52Cu, {entry}, {dt}).v0;
                    ctx.gpr[2] = word(rt, entry + 0x90u);
                    if (ok != 0u) {
                        const std::uint32_t callback = word(rt, word(rt, entry + 0x90u) + 0x14u);
                        ctx.gpr[2] = callback;
                        if (callback != 0u) call_set(rt, ctx, callback, {entry, sext8(byte_u(rt, entry + 0xE8u))});
                        call_set(rt, ctx, 0x08AEB808u, {});
                        const std::uint32_t marked = sext8(byte_u(rt, entry + 0xE8u));
                        ctx.gpr[2] = marked;
                        if (marked != 0u) {
                            const std::uint32_t cleared = word(rt, entry + 0xCu) & 0xFFFFFFBDu;
                            ctx.gpr[2] = cleared;
                            put(rt, entry + 0xCu, cleared);
                        }
                        call_set(rt, ctx, 0x08AEB82Cu, {});
                    }
                }
                ctx.gpr[2] = word(rt, entry + 0x90u);
                const std::uint32_t last = word(rt, ctx.gpr[2] + 8u);
                ctx.gpr[2] = last;
                if (last != 0u) call_setf(rt, ctx, last, {entry}, {dt});
            }
        }
        if (--remaining == 0u) break;
        cursor += 4u;
    }
}
//REG HOT116(0x08ADE5A4u, func_08ADE5A4, Word);
// func_08B2CB18(heap, size, alignment): a first-fit allocation from size-class free lists. size 0 counts as 1, alignment below 0x10 as
// 0x10; the rounded size is fp and the request s2 = fp + 0x10. Under the lock heap + 0x58 (func_08B34D40 / func_08B34D80),
// class = func_08B2C4A4(heap, s2); the 16 list heads at heap + 0x10 (4 bytes each, nodes linked through +0xC, size = node->+4 -
// node) are searched from `class` upwards for a node that holds s2 plus the padding that aligns node + 0x10: first only nodes
// on one side of the limit word heap->+0x50 (below it when s2 >= heap->+0x54, above it when smaller; none when +0x54 is -1),
// then every node. The node found is unlinked (func_08B2C720), cut (func_08B2CA08, then func_08B2C42C / func_08B2C504 when
// at least fp + 0x30 remain) when s2 < heap->+0x54, or placed by func_08B2C62C otherwise; its words +8 / +0xC are set to -1. The
// result is the node + 0x10, or 0 when nothing fits.
void func_08B2CB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t heap = ctx.gpr[4];
    std::uint32_t size = ctx.gpr[5];
    std::uint32_t alignment = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t mask = alignment - 1u;
    if (alignment < 0x10u) {
        alignment = 0x10u;
        mask = 0xFu;
    }
    if (size == 0u) size = 1u;
    const std::uint32_t inverse = ~mask;
    const std::uint32_t rounded = (size + mask) & inverse;
    const std::uint32_t request = rounded + 0x10u;
    const std::uint32_t lock = heap + 0x58u;
    call_set(rt, ctx, 0x08B2C4A4u, {heap, request});
    const std::uint32_t first_class = ctx.gpr[2];
    call_set(rt, ctx, 0x08B34D40u, {lock});
    const std::int32_t start = signed_of(first_class);
    const std::uint32_t limit = word(rt, heap + 0x54u);
    std::uint32_t found = 0u;
    const auto head = [&](std::int32_t index) { return word(rt, heap + 0x10u + static_cast<std::uint32_t>(index) * 4u); };
    const auto fits = [&](std::uint32_t node) {
        const std::uint32_t payload = node + 0x10u;
        const std::uint32_t padding = ((payload + mask) & inverse) - payload;
        return !((word(rt, node + 4u) - node) < request + padding);
    };
    // Pass 1: only the nodes on one side of the limit word at heap + 0x50.
    const auto search_side = [&](bool skip_below_boundary) {
        for (std::int32_t index = start; index < 16; ++index) {
            std::uint32_t node = head(index);
            if (node != 0u) {
                const std::uint32_t boundary = word(rt, heap + 0x50u);
                while (node != 0u) {
                    const bool skip = skip_below_boundary ? (node < boundary) : (boundary < node);
                    if (!skip && fits(node)) {
                        found = node;
                        break;
                    }
                    node = word(rt, node + 0xCu);
                }
            }
            if (found != 0u) break;
        }
    };
    // Pass 2: every node.
    const auto search_all = [&]() {
        for (std::int32_t index = start; index < 16; ++index) {
            std::uint32_t node = head(index);
            while (node != 0u) {
                if (fits(node)) {
                    found = node;
                    break;
                }
                node = word(rt, node + 0xCu);
            }
            if (found != 0u) break;
        }
    };
    if (start < 16) {
        if (limit == 0xFFFFFFFFu) {
            search_all();
        } else {
            search_side(!(request < limit));
            if (found == 0u) search_all();
        }
    }
    if (found != 0u) {
        call_set(rt, ctx, 0x08B2C720u, {heap, found});
        std::uint32_t marker = 0xFFFFFFFFu;
        if (request < word(rt, heap + 0x54u)) {
            found = call_set(rt, ctx, 0x08B2CA08u, {heap, found, alignment});
            if (!((word(rt, found + 4u) - found) < rounded + 0x30u)) {
                const std::uint32_t split = call_set(rt, ctx, 0x08B2C42Cu, {heap, found, request});
                call_set(rt, ctx, 0x08B2C504u, {heap, split});
            }
        } else {
            found = call_set(rt, ctx, 0x08B2C62Cu, {heap, found, request, alignment});
        }
        ctx.gpr[2] = marker;
        put(rt, found + 0xCu, marker);
        put(rt, found + 8u, marker);
    }
    call_set(rt, ctx, 0x08B34D80u, {lock});
    ctx.gpr[2] = found != 0u ? found + 0x10u : 0u;
}
//REG HOT116(0x08B2CB18u, func_08B2CB18, Word);
// func_08B6E920(x) (float in f12, result in f0 = f12): the fdlibm expm1f over the constant table at 0x08CB6568..0x08CB65B8.
// Magnitudes above 27 ln 2 (0x4195B843): NaN gives x + x, +inf gives x, -inf and x below the table limit give the constant
// at 0x08CB6568, x above 0x42B17217 the constant at 0x08CB656C. Below 0x32FFFFFF the result is x. Otherwise the argument is reduced
// by k * ln 2 (k = -1 / 1 for |x| within 1.5 ln 2, else trunc(x * 1/ln 2 +- 0.5)) to r with the correction f7, a polynomial in
// r * r / 2 (constants at +0x2594..+0x25B0) gives the series and the result is assembled for k = 0, -1, 1 and the
// exponent-adjusted cases (k < 23, k < 0x39 or larger) by adding k << 23 to the float's bits.
void func_08B6E920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t magnitude = bits & 0x7FFFFFFFu;
    const std::uint32_t sign = bits & 0x80000000u;
    const auto constant = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto finish = [&](float result) {
        ctx.fpr[12] = result;
        ctx.fpr[0] = result;
    };
    float r = x;
    float correction = 0.0f;
    std::int32_t k = 0;
    bool main_range = false;
    if (0x4195B843u < magnitude) {
        if (0x7F800000u < magnitude) return finish(x + x);
        if (magnitude == 0x7F800000u) {
            if (sign == 0u) return finish(x);
            return finish(constant(0x08CB6568u));
        }
        if (sign != 0u) {
            const float shifted = x + constant(0x08CB6570u);
            if (shifted < 0.0f) return finish(constant(0x08CB6568u));
        } else if (0x42B17217u < magnitude) {
            return finish(constant(0x08CB656Cu));
        }
        main_range = true;
    } else if (0x3EB17218u < magnitude) {
        if (0x3F851591u < magnitude) {
            main_range = true;
        } else if (sign == 0u) {
            const float shifted = x - constant(0x08CB6574u);
            k = 1;
            r = shifted - constant(0x08CB6578u);
            correction = (shifted - r) - constant(0x08CB6578u);
        } else {
            const float shifted = x + constant(0x08CB6574u);
            k = -1;
            r = shifted - constant(0x08CB657Cu);
            correction = (shifted - r) - constant(0x08CB657Cu);
        }
    } else {
        if (!(0x32FFFFFFu < magnitude)) {
            float tiny = x + constant(0x08CB6590u);
            tiny = tiny - tiny;
            return finish(x - tiny);
        }
        k = 0;
    }
    if (main_range) {
        float scaled = psp_mul(x, constant(0x08CB6580u));
        const float half = sign == 0u ? constant(0x08CB6584u) : constant(0x08CB6588u);
        scaled = half + scaled;
        const std::uint32_t truncated = ctx.fpu_float_to_word(scaled, 1u);
        k = signed_of(truncated);
        const float steps = static_cast<float>(k);
        const float high = psp_mul(steps, constant(0x08CB658Cu));
        const float low = psp_mul(steps, constant(0x08CB6578u));
        const float shifted = x + high;
        r = shifted - low;
        correction = (shifted - r) - low;
    }
    const float half_constant = constant(0x08CB6584u);
    const float scaled_r = psp_mul(r, half_constant);
    const float square = psp_mul(r, scaled_r);
    float poly = psp_mul(square, constant(0x08CB6594u));
    poly = poly + constant(0x08CB6598u);
    poly = psp_mul(square, poly);
    poly = poly - constant(0x08CB659Cu);
    poly = psp_mul(square, poly);
    poly = poly + constant(0x08CB65A0u);
    poly = psp_mul(square, poly);
    poly = poly - constant(0x08CB65A4u);
    poly = psp_mul(square, poly);
    const float one = constant(0x08CB65A8u);
    const float tail = constant(0x08CB65ACu);
    poly = poly + one;
    const float product = psp_mul(scaled_r, poly);
    const float numerator_term = tail - product;
    const float denominator_scale = psp_mul(r, numerator_term);
    poly = poly - numerator_term;
    const float denominator = constant(0x08CB65B0u) - denominator_scale;
    poly = poly / denominator;
    float series = psp_mul(square, poly);
    if (k == 0) {
        series = psp_mul(r, series);
        series = series - square;
        return finish(r - series);
    }
    series = series - correction;
    series = psp_mul(r, series);
    series = series - correction;
    const float shifted = series - square;
    if (k == -1) {
        float result = r - shifted;
        result = psp_mul(result, half_constant);
        return finish(result - half_constant);
    }
    if (k == 1) {
        if (r < constant(0x08CB65B4u)) {
            const float sum = r + half_constant;
            return finish(psp_mul(shifted - sum, constant(0x08CB65B8u)));
        }
        float result = r - shifted;
        result = result + result;
        return finish(result + one);
    }
    if (static_cast<std::uint32_t>(k + 1) < 0x3Au) {
        if (k < 0x17) {
            const std::uint32_t pattern = static_cast<std::uint32_t>(signed_of(0x01000000u) >> (static_cast<std::uint32_t>(k) & 31u));
            const float bias = std::bit_cast<float>(0x3F800000u - pattern);
            const float residue = shifted - r;
            float value = bias - residue;
            const std::uint32_t adjusted = std::bit_cast<std::uint32_t>(value) + (static_cast<std::uint32_t>(k) << 23);
            return finish(std::bit_cast<float>(adjusted));
        }
        const float scale = std::bit_cast<float>((0x7Fu - static_cast<std::uint32_t>(k)) << 23);
        float value = shifted + scale;
        value = r - value;
        value = value + one;
        const std::uint32_t adjusted = std::bit_cast<std::uint32_t>(value) + (static_cast<std::uint32_t>(k) << 23);
        return finish(std::bit_cast<float>(adjusted));
    }
    const float residue = shifted - r;
    const float value = one - residue;
    const std::uint32_t adjusted = std::bit_cast<std::uint32_t>(value) + (static_cast<std::uint32_t>(k) << 23);
    return finish(std::bit_cast<float>(adjusted) - one);
}
//REG HOT116(0x08B6E920u, func_08B6E920, Float);
// func_08BA996C(vector, position, count, value): the libstdc++ vector<8-byte element>::insert(position, count, value) for an element
// {word +0, shared-pointer control block +4}. Nothing when count == 0. With room: the value is copied to a frame element
// (frame +0xC, its control block's use count incremented); the elements after the position move up by count (func_08BA9338
// for the tail, then word and shared-pointer assignments backwards: the new block's use count goes up, the old one is released)
// or, when fewer follow, func_08BA91F8 / func_08BA9248; func_08BA9388 fills; the frame element is destroyed (func_0888879C).
// Full: size + max(size, count) elements (maximum 0x1FFFFFFF, func_08B5ADDC(0x08CA0740) beyond; func_08B35DA0 allocates),
// func_08BA92E8 / func_08BA9298 / func_08BA92E8 build the new range, func_08B8E594 destroys the old one, func_08B35254 frees it
// and begin / end / capacity are updated.
void func_08BA996C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 3);
    if (!(room < count)) {
        const std::uint32_t temp = frame.at(0xCu);
        const std::uint32_t block = word(rt, value + 4u);
        put(rt, temp + 4u, block);
        put(rt, temp, word(rt, value));
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
        if (count < after) {
            call_set(rt, ctx, 0x08BA9338u, {end - (count << 3), end, end, frame.at(6u)});
            const std::uint32_t source_end = end - (count << 3);
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 3);
            put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    source -= 8u;
                    destination -= 8u;
                    const std::uint32_t moved = word(rt, source);
                    const std::uint32_t old_block = word(rt, destination + 4u);
                    put(rt, destination, moved);
                    const std::uint32_t new_block = word(rt, source + 4u);
                    const std::uint32_t slot = destination + 4u;
                    if (new_block == old_block) continue;
                    if (new_block != 0u) count_up(rt, ctx, new_block + 4u);
                    const std::uint32_t previous = word(rt, slot);
                    if (previous != 0u) release_shared(rt, ctx, previous);
                    put(rt, slot, new_block);
                }
            }
            call_set(rt, ctx, 0x08BA9388u, {position, position + (count << 3), temp});
        } else {
            call_set(rt, ctx, 0x08BA91F8u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 3);
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BA9248u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
            call_set(rt, ctx, 0x08BA9388u, {position, end, temp});
        }
        call_set(rt, ctx, 0x0888879Cu, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 3);
    put(rt, frame.at(8u), size);
    if (0x1FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0740u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFF8u;
    std::uint32_t block = 0u;
    if (!(total < size)) bytes = total << 3;
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08BA92E8u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BA9298u, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BA92E8u, {position, word(rt, vector + 4u), front + (count << 3), frame.at(1u)});
    call_set(rt, ctx, 0x08B8E594u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BA996Cu, func_08BA996C, Word);
// func_08BBFAAC(vector, position, count, value): the libstdc++ vector<20-byte element>::insert(position, count, value) for an element
// {string pointer +0, inner vector of 16-byte records at +4 (begin, end, capacity), word +0x10}. Nothing when count == 0. With
// room: the value is copied to a frame element (frame +0x10: string by func_08B9432C, inner vector duplicated through func_08B35DA0
// / func_08BBEC68, word at +0x20); the elements after the position move up by count (func_08BBF7E8 for the tail, then string
// (func_08B957D8) and inner-vector (func_08BBECF0) assignments plus the word, backwards) or, when fewer follow, func_08BBF8CC /
// func_08BBF9B8; func_08BBEED0 fills; the frame inner vector is destroyed (func_08BBEBE4) and freed (func_08B35254) and the frame string
// released (func_08B60804 / func_08B93554 unless the empty rep 0x08D75298). Full: size + max(size, count) elements (maximum
// 0x0CCCCCCC, func_08B5ADDC(0x08CA0DC4) beyond; func_08B35DA0 allocates), func_08BBF640 / func_08BBF554 / func_08BBF640 build the
// new range, func_08BBF734 destroys the old one, func_08B35254 frees it and begin / end / capacity are updated.
void func_08BBFAAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 2) * 0xCCCCCCCDu;
    if (!(room < count)) {
        const std::uint32_t temp = frame.at(0x10u);
        const std::uint32_t inner = value + 4u;
        call_set(rt, ctx, 0x08B9432Cu, {temp, value});
        const std::uint32_t inner_end = word(rt, inner + 4u);
        const std::uint32_t inner_begin = word(rt, value + 4u);
        std::uint32_t bytes = 0u;
        std::uint32_t block = 0u;
        put(rt, frame.at(0x14u), 0u);
        const std::uint32_t records = static_cast<std::uint32_t>(signed_of(inner_end - inner_begin) >> 4);
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        if (records != 0u) {
            bytes = records << 4;
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        }
        put(rt, frame.at(0x14u), block);
        put(rt, frame.at(0x18u), block);
        put(rt, frame.at(0x1Cu), block + bytes);
        const std::uint32_t copied = call_set(rt, ctx, 0x08BBEC68u, {word(rt, inner), word(rt, inner + 4u), block, frame.at(2u)});
        const std::uint32_t current_end = word(rt, vector + 4u);
        const std::uint32_t before = static_cast<std::uint32_t>(signed_of(current_end - position) >> 2);
        const std::uint32_t after = before * 0xCCCCCCCDu;
        put(rt, frame.at(0x20u), word(rt, value + 0x10u));
        put(rt, frame.at(0x18u), copied);
        if (count < after) {
            const std::uint32_t shift = count * 20u;
            call_set(rt, ctx, 0x08BBF7E8u, {current_end - shift, current_end, current_end, frame.at(9u)});
            const std::uint32_t source_end = current_end - shift;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 2) * 0xCCCCCCCDu;
            put(rt, vector + 4u, word(rt, vector + 4u) + shift);
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = current_end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    destination -= 0x14u;
                    source -= 0x14u;
                    call_set(rt, ctx, 0x08B957D8u, {destination, source});
                    call_set(rt, ctx, 0x08BBECF0u, {destination + 4u, source + 4u});
                    const std::uint32_t tail = word(rt, source + 0x10u);
                    put(rt, destination + 0x10u, tail);
                }
            }
            call_set(rt, ctx, 0x08BBEED0u, {position, position + shift, temp});
        } else {
            call_set(rt, ctx, 0x08BBF8CCu, {current_end, count - after, temp, frame.at(8u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 20u;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BBF9B8u, {position, current_end, new_end, frame.at(7u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (before << 2));
            call_set(rt, ctx, 0x08BBEED0u, {position, current_end, temp});
        }
        call_set(rt, ctx, 0x08BBEBE4u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(1u)});
        if (word(rt, frame.at(0x14u)) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x14u))});
        ctx.gpr[2] = 0x08D75298u;
        release_string(rt, ctx, frame.at(0x10u), 0x08D75298u, 0x08B93554u, frame.at(0u));
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2) * 0xCCCCCCCDu;
    put(rt, frame.at(0xCu), size);
    if (0x0CCCCCCCu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0DC4u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFF0u;
    std::uint32_t block = 0u;
    if (!(total < size)) bytes = total * 20u;
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08BBF640u, {word(rt, vector), position, block, frame.at(6u)});
    call_set(rt, ctx, 0x08BBF554u, {front, count, value, frame.at(5u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BBF640u, {position, word(rt, vector + 4u), front + count * 20u, frame.at(4u)});
    call_set(rt, ctx, 0x08BBF734u, {word(rt, vector), word(rt, vector + 4u), frame.at(3u)});
    const std::uint32_t old_block = word(rt, vector);
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BBFAACu, func_08BBFAAC, Word);
// func_08BC17C8(result, holder, key): a locked lookup of the string `key` in the circular list at holder->+0's +0x98
// (nodes: next +0, string data pointer +8 compared by length then func_08B36170 over the shorter length, shared pointer
// {object +0xC, control block +0x10}, flag byte +0x14). result = {found byte, object, control block}. A holder of 0
// gives {1, 0, 0}. A match whose flag byte is set gives {0, 0, 0}; otherwise the shared pointer is copied (use count
// incremented only when it was not zero, then again and released as the copy dies). No match: func_08BBE958 inserts a node
// built from an empty string in the frame (the frame's temporary node list is torn down node by node through
// func_08BBD1B8 and the virtual slot 4, its shared string released), the node's string (+8) gets func_08B957D8(+8, key),
// its flag byte is set and result = {1, 0, 0}. func_08B34888(holder->+0 + 0x6C) locks first, func_08BBCFA0(holder) unlocks.
void func_08BC17C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    const std::uint32_t owner = word(rt, holder);
    if (owner == 0u) {
        put8(rt, result, 1u);
        put(rt, result + 4u, 0u);
        put(rt, result + 8u, 0u);
        ctx.gpr[2] = result;
        return;
    }
    call_set(rt, ctx, 0x08B34888u, {owner + 0x6Cu});
    const std::uint32_t table = word(rt, holder);
    const std::uint32_t sentinel = table + 0x98u;
    std::uint32_t node = sentinel;
    const std::uint32_t first = word(rt, table + 0x98u);
    if (first != sentinel) {
        const std::uint32_t text = word(rt, key);
        node = first;
        for (;;) {
            const std::uint32_t node_text = word(rt, node + 8u);
            const std::uint32_t node_length = word(rt, node_text - 0xCu);
            put(rt, frame.at(4u), node_length);
            const std::uint32_t key_length = word(rt, text - 0xCu);
            put(rt, frame.at(8u), key_length);
            const std::uint32_t shorter = key_length < node_length ? frame.at(8u) : frame.at(4u);
            const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {word(rt, node + 8u), text, word(rt, shorter)});
            if (order != 0u || node_length != key_length) node = word(rt, node);
            else break;
            if (sentinel == node) break;
        }
    }
    if (sentinel == node) {
        // No match: a node is inserted.
        constexpr std::uint32_t kEmptyRep = 0x08D75298u;
        const std::uint32_t pending = frame.at(0x1Cu);
        put(rt, pending, kEmptyRep + 0xCu);
        put(rt, pending + 4u, 0u);
        put(rt, pending + 8u, 0u);
        put8(rt, pending + 0xCu, 0u);
        put(rt, pending + 0x10u, 0u);
        put(rt, pending + 0x14u, 0u);
        put(rt, pending + 0x18u, 0u);
        const std::uint32_t inserted = call_set(rt, ctx, 0x08BBE958u, {sentinel, sentinel, pending});
        std::uint32_t entry = word(rt, frame.at(0x2Cu));
        ctx.gpr[2] = 0u;
        while (entry != 0u) {
            const std::uint32_t next = word(rt, entry + 4u);
            put(rt, frame.at(0xCu), entry);
            put(rt, frame.at(0x10u), ctx.gpr[2]);
            ctx.gpr[4] = frame.at(0x2Cu);
            ctx.gpr[5] = frame.at(0xCu);
            call_keep(rt, ctx, 0x08BBD1B8u, {frame.at(0x2Cu), frame.at(0xCu)}, {});
            put(rt, frame.at(0x14u), ctx.gpr[2]);
            put(rt, frame.at(0x18u), ctx.gpr[3]);
            put(rt, frame.at(0x40u), ctx.gpr[2]);
            put(rt, frame.at(0x44u), ctx.gpr[3]);
            call_virtual_v1(rt, ctx, entry, 4u, {entry});
            ctx.gpr[2] = entry;
            entry = next;
        }
        ctx.gpr[4] = frame.at(0x2Cu);
        ctx.gpr[5] = frame.at(0xCu);
        const std::uint32_t control = word(rt, frame.at(0x24u));
        if (control != 0u) {
            if (count_down(rt, ctx, control + 8u)) call_virtual_at(rt, ctx, control, 0xCu, {control});
        }
        release_string(rt, ctx, frame.at(0x1Cu), kEmptyRep, 0x08B93554u, frame.at(0u));
        const std::uint32_t name = inserted + 8u;
        call_set(rt, ctx, 0x08B957D8u, {name, key});
        put8(rt, name + 0xCu, 1u);
        put8(rt, result, 1u);
        put(rt, result + 4u, 0u);
        put(rt, result + 8u, 0u);
    } else if (byte_u(rt, node + 0x14u) != 0u) {
        put8(rt, result, 0u);
        put(rt, result + 4u, 0u);
        put(rt, result + 8u, 0u);
    } else {
        const std::uint32_t control = word(rt, node + 0x10u);
        std::uint32_t shared = 0u;
        std::uint32_t object = 0u;
        if (control != 0u && word(rt, control + 4u) != 0u) {
            shared = control;
            const std::uint32_t uses = word(rt, control + 4u);
            if (uses != 0u) put(rt, control + 4u, uses + 1u);
            object = word(rt, node + 0xCu);
        }
        put(rt, result + 4u, object);
        put8(rt, result, 0u);
        put(rt, result + 8u, shared);
        if (shared != 0u) {
            count_up(rt, ctx, shared + 4u);
            release_shared(rt, ctx, shared);
        }
    }
    call_set(rt, ctx, 0x08BBCFA0u, {holder});
    ctx.gpr[2] = result;
}
//REG HOT116(0x08BC17C8u, func_08BC17C8, Word);
// func_08BDC338(vector, position, count, value): the libstdc++ vector<8-byte element>::insert(position, count, value) for an element
// {word +0, shared-pointer control block +4}. Nothing when count == 0. With room: the value is copied to a frame element
// (frame +0xC, its control block's use count incremented); the elements after the position move up by count (func_08BDBFE0
// for the tail, then word and shared-pointer assignments backwards: the new block's use count goes up, the old one is released)
// or, when fewer follow, func_08BDC030 / func_08BDC080; func_08BDC228 fills; the frame element's shared pointer is released.
// Full: size + max(size, count) elements (maximum 0x1FFFFFFF, func_08B5ADDC(0x08CA1A44) beyond; func_08B35DA0 allocates),
// func_08BDC1D8 / func_08BDC188 / func_08BDC1D8 build the new range, func_08B8ECAC destroys the old one, func_08B35254 frees it
// and begin / end / capacity are updated.
void func_08BDC338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 3);
    if (!(room < count)) {
        const std::uint32_t temp = frame.at(0xCu);
        const std::uint32_t block = word(rt, value + 4u);
        put(rt, temp + 4u, block);
        put(rt, temp, word(rt, value));
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
        if (count < after) {
            call_set(rt, ctx, 0x08BDBFE0u, {end - (count << 3), end, end, frame.at(6u)});
            const std::uint32_t source_end = end - (count << 3);
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 3);
            put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    source -= 8u;
                    destination -= 8u;
                    const std::uint32_t moved = word(rt, source);
                    const std::uint32_t old_block = word(rt, destination + 4u);
                    put(rt, destination, moved);
                    const std::uint32_t new_block = word(rt, source + 4u);
                    const std::uint32_t slot = destination + 4u;
                    if (new_block == old_block) continue;
                    if (new_block != 0u) count_up(rt, ctx, new_block + 4u);
                    const std::uint32_t previous = word(rt, slot);
                    if (previous != 0u) release_shared(rt, ctx, previous);
                    put(rt, slot, new_block);
                }
            }
            call_set(rt, ctx, 0x08BDC228u, {position, position + (count << 3), temp});
        } else {
            call_set(rt, ctx, 0x08BDC030u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 3);
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BDC080u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
            call_set(rt, ctx, 0x08BDC228u, {position, end, temp});
        }
        const std::uint32_t temp_block = word(rt, temp + 4u);
        if (temp_block != 0u) release_shared(rt, ctx, temp_block);
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 3);
    put(rt, frame.at(8u), size);
    if (0x1FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA1A44u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFF8u;
    std::uint32_t block = 0u;
    if (!(total < size)) bytes = total << 3;
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08BDC1D8u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BDC188u, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BDC1D8u, {position, word(rt, vector + 4u), front + (count << 3), frame.at(1u)});
    call_set(rt, ctx, 0x08B8ECACu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BDC338u, func_08BDC338, Word);
// func_08BDFB84(vector, position, count, value): the libstdc++ vector<16-byte element>::insert(position, count, value) for an
// element {halfword +0, inner vector of 28-byte records at +4 (begin, end, capacity)}. Nothing when count == 0. The value is
// first copied to a frame element (halfword at frame +0x10, inner vector at +0x14 duplicated by func_08B35DA0 /
// func_08BDEAD8). With room: the elements after the position move up by count (func_08BDEF64 for the tail, then halfword copy
// + func_08BDF848 inner-vector assignment backwards) or, when fewer follow, func_08BDF33C / func_08BDF244; func_08BDFB18 fills;
// the frame copy is destroyed (func_08BDD7D8) and freed (func_08B35254). Full: size + max(size, count) elements (maximum
// 0x0FFFFFFF, func_08B5ADDC(0x08CA1ABC) beyond; func_08B35DA0 allocates), func_08BDF054 / func_08BDF14C / func_08BDF054 build the
// new range, func_08BDD878 destroys the old one, func_08B35254 frees it and begin / end / capacity are updated.
void func_08BDFB84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    const std::uint32_t free_elements = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - word(rt, vector + 4u)) >> 4);
    if (!(free_elements < count)) {
        const std::uint32_t inner = value + 4u;
        const std::uint32_t inner_begin = word(rt, value + 4u);
        const std::uint32_t inner_end = word(rt, inner + 4u);
        const std::uint32_t words = static_cast<std::uint32_t>(signed_of(inner_end - inner_begin) >> 2);
        const std::uint32_t records = words * 0xB6DB6DB7u;
        put16(rt, frame.at(0x10u), half_u(rt, value));
        put(rt, frame.at(0x14u), 0u);
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        std::uint32_t bytes = 0u;
        std::uint32_t block = 0u;
        if (records != 0u) {
            bytes = words << 2;
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        }
        put(rt, frame.at(0x1Cu), block + bytes);
        put(rt, frame.at(0x14u), block);
        put(rt, frame.at(0x18u), block);
        const std::uint32_t copied = call_set(rt, ctx, 0x08BDEAD8u, {word(rt, inner), word(rt, inner + 4u), block, frame.at(1u)});
        put(rt, frame.at(0x18u), copied);
        const std::uint32_t end = word(rt, vector + 4u);
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 4);
        const std::uint32_t element = frame.at(0x10u);
        if (count < after) {
            call_set(rt, ctx, 0x08BDEF64u, {end - (count << 4), end, end, frame.at(8u)});
            const std::uint32_t shift = count << 4;
            const std::uint32_t middle_end = end - shift;
            put(rt, vector + 4u, word(rt, vector + 4u) + shift);
            const std::int32_t middle = signed_of(middle_end - position) >> 4;
            std::uint32_t source = middle_end - 0x10u;
            std::uint32_t destination = end - 0x10u;
            for (std::int32_t index = 0; index < middle; ++index) {
                put16(rt, destination, half_u(rt, source));
                call_set(rt, ctx, 0x08BDF848u, {destination + 4u, source + 4u});
                source -= 0x10u;
                destination -= 0x10u;
            }
            call_set(rt, ctx, 0x08BDFB18u, {position, position + (count << 4), element});
        } else {
            call_set(rt, ctx, 0x08BDF33Cu, {end, count - after, element, frame.at(7u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 4);
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BDF244u, {position, end, new_end, frame.at(6u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after << 4));
            call_set(rt, ctx, 0x08BDFB18u, {position, end, element});
        }
        call_set(rt, ctx, 0x08BDD7D8u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(0u)});
        if (word(rt, frame.at(0x14u)) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x14u))});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - begin) >> 4);
    put(rt, frame.at(0xCu), size);
    if (0x0FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA1ABCu});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t block = 0u;
    std::uint32_t bytes = 0xFFFFFFF0u;
    if (!(total < size)) {
        bytes = 0u;
        if (total != 0u) bytes = total << 4;
    }
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08BDF054u, {word(rt, vector), position, block, frame.at(5u)});
    call_set(rt, ctx, 0x08BDF14Cu, {front, count, value, frame.at(4u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BDF054u, {position, word(rt, vector + 4u), front + (count << 4), frame.at(3u)});
    call_set(rt, ctx, 0x08BDD878u, {word(rt, vector), word(rt, vector + 4u), frame.at(2u)});
    const std::uint32_t old_block = word(rt, vector);
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BDFB84u, func_08BDFB84, Word);
// func_08BE6708(vector, position, count, value): the libstdc++ vector<48-byte element>::insert(position, count, value) for an element
// {vtable word +0 (0x08CCDE00), words +4 / +8, floats +0xC..+0x2C}. Nothing when count == 0. With room: the value is copied to a frame
// element (frame +0xC, vtable word + 11 words); the elements after the position move up by count (func_08BE642C for the tail,
// then the 11 words backwards) or, when fewer follow, func_08BE6674 / func_08BE65E0; func_08BE5F04 fills; the frame element's vtable
// word is rewritten and it is destroyed (func_089D6684). Full: size + max(size, count) elements (maximum 0x05555555,
// func_08B5ADDC(0x08CA2338) beyond; func_08B35840(0x10, bytes) allocates), func_08BE64BC / func_08BE654C / func_08BE64BC build the
// new range, func_08BE5BC0 destroys the old one, func_08B35254 frees it and begin / end / capacity are updated.
void func_08BE6708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    constexpr std::uint32_t kVtable = 0x08CCDE00u;
    put(rt, frame.at(0x40u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 4) * 0xAAAAAAABu;
    if (!(room < count)) {
        const std::uint32_t before = static_cast<std::uint32_t>(signed_of(end - position) >> 4);
        const std::uint32_t after = before * 0xAAAAAAABu;
        const std::uint32_t temp = frame.at(0xCu);
        for (std::uint32_t offset = 4u; offset <= 0x2Cu; offset += 4u) put(rt, temp + offset, word(rt, value + offset));
        put(rt, temp, kVtable);
        if (count < after) {
            const std::uint32_t bytes = count * 48u;
            call_set(rt, ctx, 0x08BE642Cu, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 4) * 0xAAAAAAABu;
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    source -= 0x30u;
                    destination -= 0x30u;
                    std::uint32_t words[11];
                    for (std::uint32_t k = 0u; k < 11u; ++k) words[k] = word(rt, source + 4u + k * 4u);
                    for (std::uint32_t k = 0u; k < 11u; ++k) put(rt, destination + 4u + k * 4u, words[k]);
                }
            }
            call_set(rt, ctx, 0x08BE5F04u, {position, position + bytes, temp});
        } else {
            call_set(rt, ctx, 0x08BE6674u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 48u;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BE65E0u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (before << 4));
            call_set(rt, ctx, 0x08BE5F04u, {position, end, temp});
        }
        put(rt, temp, kVtable);
        call_set(rt, ctx, 0x089D6684u, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4) * 0xAAAAAAABu;
    put(rt, frame.at(8u), size);
    if (0x05555555u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA2338u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFF0u;
    if (!(total < size)) bytes = total * 48u;
    const std::uint32_t block = call_set(rt, ctx, 0x08B35840u, {0x10u, bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08BE64BCu, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BE654Cu, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BE64BCu, {position, word(rt, vector + 4u), front + count * 48u, frame.at(1u)});
    call_set(rt, ctx, 0x08BE5BC0u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BE6708u, func_08BE6708, Word);
// func_08BE6DB8(vector, position, count, value): the libstdc++ vector<40-byte element>::insert(position, count, value) for an element
// {vtable word +0 (0x08CCE6E0), words +4 / +8, five floats +0xC..+0x1C, words +0x20 / +0x24}. Nothing when count == 0. With room: the
// value is copied to a frame element (frame +0xC); the elements after the position move up by count (func_08BE6B5C for the
// tail, then word / float copies backwards, without the vtable word) or, when fewer follow, func_08BE6D44 / func_08BE6CC4;
// func_08BE5EA0 fills; the frame element's vtable word is rewritten and it is destroyed (func_089D6684). Full: size + max(size,
// count) elements (maximum 0x06666666, func_08B5ADDC(0x08CA2338) beyond; func_08A5BC20 allocates), func_08BE6BD4 / func_08BE6C50 /
// func_08BE6BD4 build the new range, func_08BE5C0C destroys the old one, func_08A5BC54 frees it and begin / end / capacity are
// updated.
void func_08BE6DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    constexpr std::uint32_t kVtable = 0x08CCE6E0u;
    put(rt, frame.at(0x40u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 3) * 0xCCCCCCCDu;
    if (!(room < count)) {
        const std::uint32_t before = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
        const std::uint32_t after = before * 0xCCCCCCCDu;
        const std::uint32_t temp = frame.at(0xCu);
        put(rt, temp + 4u, word(rt, value + 4u));
        put(rt, temp + 8u, word(rt, value + 8u));
        for (std::uint32_t offset = 0xCu; offset <= 0x24u; offset += 4u) put(rt, temp + offset, word(rt, value + offset));
        put(rt, temp, kVtable);
        if (count < after) {
            const std::uint32_t bytes = count * 40u;
            call_set(rt, ctx, 0x08BE6B5Cu, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 3) * 0xCCCCCCCDu;
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    source -= 0x28u;
                    destination -= 0x28u;
                    std::uint32_t words[9];
                    for (std::uint32_t k = 0u; k < 9u; ++k) words[k] = word(rt, source + 4u + k * 4u);
                    for (std::uint32_t k = 0u; k < 9u; ++k) put(rt, destination + 4u + k * 4u, words[k]);
                }
            }
            call_set(rt, ctx, 0x08BE5EA0u, {position, position + bytes, temp});
        } else {
            call_set(rt, ctx, 0x08BE6D44u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 40u;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BE6CC4u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (before << 3));
            call_set(rt, ctx, 0x08BE5EA0u, {position, end, temp});
        }
        put(rt, temp, kVtable);
        call_set(rt, ctx, 0x089D6684u, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 3) * 0xCCCCCCCDu;
    put(rt, frame.at(8u), size);
    if (0x06666666u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA2338u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFF0u;
    if (!(total < size)) bytes = total * 40u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t front = call_set(rt, ctx, 0x08BE6BD4u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BE6C50u, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BE6BD4u, {position, word(rt, vector + 4u), front + count * 40u, frame.at(1u)});
    call_set(rt, ctx, 0x08BE5C0Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    const std::uint32_t old_words = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_block) >> 3);
    if (old_block != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_block, old_words << 3, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BE6DB8u, func_08BE6DB8, Word);
// func_08BE8700(vector, position, count, value): the libstdc++ vector<64-byte element>::insert(position, count, value) for an
// element {vtable word +0 (0x08CCFC90), words +4 / +8, five floats +0xC..+0x1C, value objects +0x20 and +0x30}. Nothing when
// count == 0. With room: the value is copied to a frame element (frame +0xC; objects by func_089FA280 at +0x2C / +0x3C); the
// elements after the position move up by count (func_08BE864C for the tail, then words, floats and func_089FB570 object
// assignments backwards) or, when fewer follow, func_08BE834C / func_08BE8408; func_08BE7828 fills; the frame element is
// destroyed (func_089F9B74 twice, func_089D6684). Full: size + max(size, count) elements (maximum 0x03FFFFFF,
// func_08B5ADDC(0x08CA2338) beyond; func_08A5BC20 allocates), func_08BE8588 / func_08BE84CC / func_08BE8588 build the new range,
// func_08BE5B74 destroys the old one, func_08A5BC54 frees it and begin / end / capacity are updated.
void func_08BE8700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x90u);
    constexpr std::uint32_t kVtable = 0x08CCFC90u;
    put(rt, frame.at(0x50u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 6);
    if (!(room < count)) {
        const std::uint32_t temp = frame.at(0xCu);
        const std::uint32_t first_object = frame.at(0x2Cu);
        const std::uint32_t second_object = frame.at(0x3Cu);
        put(rt, temp + 4u, word(rt, value + 4u));
        put(rt, temp, kVtable);
        put(rt, temp + 8u, word(rt, value + 8u));
        for (std::uint32_t offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, temp + offset, word(rt, value + offset));
        call_set(rt, ctx, 0x089FA280u, {first_object, value + 0x20u});
        call_set(rt, ctx, 0x089FA280u, {second_object, value + 0x30u});
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 6);
        if (count < after) {
            const std::uint32_t bytes = count << 6;
            call_set(rt, ctx, 0x08BE864Cu, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 6);
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            if (signed_of(middle) > 0) {
                std::uint32_t source = source_end;
                std::uint32_t destination = end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    source -= 0x40u;
                    destination -= 0x40u;
                    const std::uint32_t a = word(rt, source + 4u);
                    const std::uint32_t b = word(rt, source + 8u);
                    std::uint32_t floats[5];
                    for (std::uint32_t k = 0u; k < 5u; ++k) floats[k] = word(rt, source + 0xCu + k * 4u);
                    put(rt, destination + 4u, a);
                    put(rt, destination + 8u, b);
                    for (std::uint32_t k = 0u; k < 5u; ++k) put(rt, destination + 0xCu + k * 4u, floats[k]);
                    call_set(rt, ctx, 0x089FB570u, {destination + 0x20u, source + 0x20u});
                    call_set(rt, ctx, 0x089FB570u, {destination + 0x30u, source + 0x30u});
                }
            }
            call_set(rt, ctx, 0x08BE7828u, {position, position + bytes, temp});
        } else {
            call_set(rt, ctx, 0x08BE834Cu, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 6);
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08BE8408u, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after << 6));
            call_set(rt, ctx, 0x08BE7828u, {position, end, temp});
        }
        put(rt, temp, kVtable);
        call_set(rt, ctx, 0x089F9B74u, {second_object});
        call_set(rt, ctx, 0x089F9B74u, {first_object});
        call_set(rt, ctx, 0x089D6684u, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 6);
    put(rt, frame.at(8u), size);
    if (0x03FFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA2338u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFC0u;
    if (!(total < size)) bytes = total << 6;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t front = call_set(rt, ctx, 0x08BE8588u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08BE84CCu, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08BE8588u, {position, word(rt, vector + 4u), front + (count << 6), frame.at(1u)});
    call_set(rt, ctx, 0x08BE5B74u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    const std::uint32_t old_elements = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_block) >> 6);
    if (old_block != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_block, old_elements << 6, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08BE8700u, func_08BE8700, Word);
// func_08C2C56C(vector, position, count, value): the libstdc++ vector<12-byte element>::insert(position, count, value) for an
// element {value object +0, word +4, value object +8}. Nothing when count == 0. With room: the value is copied to a frame
// element (func_08A6972C at frame +0xC, word +0x10, func_08A76ABC at frame +0x14); the elements after the position move up by
// count (func_08C2C46C for the tail, then func_08A6A23C + word + func_08A5BAD8 assignments backwards) or, when fewer follow,
// func_08C2C2F0 / func_08C2C36C; func_08C2C4E0 fills; both frame objects are destroyed (func_08A5B9EC, func_08A69D68). Full:
// size + max(size, count) elements (maximum 0x15555555, func_08B5ADDC(0x08CB1E84) beyond; func_08A5BC20 allocates),
// func_08C2B76C / func_08C2C3F0 / func_08C2B76C build the new range, func_08C2AE8C destroys the old one, func_08A5BC54 frees it
// and begin / end / capacity are updated.
void func_08C2C56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 2) * 0xAAAAAAABu;
    if (!(room < count)) {
        const std::uint32_t temp = frame.at(0xCu);
        const std::uint32_t temp_object = frame.at(0x14u);
        call_set(rt, ctx, 0x08A6972Cu, {temp, value});
        put(rt, frame.at(0x10u), word(rt, value + 4u));
        call_set(rt, ctx, 0x08A76ABCu, {temp_object, value + 8u});
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 2) * 0xAAAAAAABu;
        if (count < after) {
            const std::uint32_t bytes = count * 12u;
            call_set(rt, ctx, 0x08C2C46Cu, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = static_cast<std::uint32_t>(signed_of(source_end - position) >> 2) * 0xAAAAAAABu;
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            if (signed_of(middle) > 0) {
                std::uint32_t destination = end;
                std::uint32_t source = source_end;
                for (std::uint32_t index = 0u; index != middle; ++index) {
                    destination -= 0xCu;
                    source -= 0xCu;
                    call_set(rt, ctx, 0x08A6A23Cu, {destination, source});
                    put(rt, destination + 4u, word(rt, source + 4u));
                    if (destination + 8u != source + 8u) call_set(rt, ctx, kAssign, {destination + 8u, word(rt, source + 8u)});
                }
            }
            call_set(rt, ctx, 0x08C2C4E0u, {position, position + bytes, temp});
        } else {
            const std::uint32_t before = static_cast<std::uint32_t>(signed_of(end - position) >> 2);
            call_set(rt, ctx, 0x08C2C2F0u, {end, count - after, temp, frame.at(5u)});
            const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 12u;
            put(rt, vector + 4u, new_end);
            call_set(rt, ctx, 0x08C2C36Cu, {position, end, new_end, frame.at(4u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (before << 2));
            call_set(rt, ctx, 0x08C2C4E0u, {position, end, temp});
        }
        call_set(rt, ctx, kDestroy, {temp_object});
        call_set(rt, ctx, 0x08A69D68u, {temp});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2) * 0xAAAAAAABu;
    put(rt, frame.at(8u), size);
    if (0x15555555u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB1E84u});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t bytes = 0xFFFFFFFCu;
    if (!(total < size)) bytes = total * 12u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t front = call_set(rt, ctx, 0x08C2B76Cu, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08C2C3F0u, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08C2B76Cu, {position, word(rt, vector + 4u), front + count * 12u, frame.at(1u)});
    call_set(rt, ctx, 0x08C2AE8Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    const std::uint32_t old_words = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_block) >> 2);
    if (old_block != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_block, old_words << 2, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08C2C56Cu, func_08C2C56C, Word);
// func_08C4BD80(vector, position, count, value): the libstdc++ vector<24-byte element>::insert(position, count, value). Nothing
// when count == 0. The value (6 words) is copied to frame +0xC first. With room (capacity +8 - end +4, in elements): the elements
// after the position are moved up by count (func_08C4AD94 for the tail copy, a backward word-by-word copy for the middle,
// func_08C4AF5C fills) or, when fewer than count follow, func_08C4AEAC / func_08C4AFB4 / func_08C4AF5C. Full: a block of size
// + max(size, count) elements (func_08B5ADDC(0x08CB4AFC) beyond the maximum 0x0AAAAAAA; func_08B35DA0 allocates) receives
// the front (func_08C4B568), the new elements (func_08C4B50C) and the back (func_08C4B568); the old range is destroyed
// (func_08C498C4), the old block freed (func_08B35254) and begin / end / capacity updated.
void func_08C4BD80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = elements24(word(rt, vector + 8u) - end);
    if (!(room < count)) {
        const std::uint32_t before = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
        const std::uint32_t after = before * 0xAAAAAAABu;
        const std::uint32_t copy = frame.at(0xCu);
        for (std::uint32_t index = 0u; index < 6u; ++index) put(rt, copy + index * 4u, word(rt, value + index * 4u));
        if (count < after) {
            const std::uint32_t bytes = count * 24u;
            call_set(rt, ctx, 0x08C4AD94u, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = elements24(source_end - position);
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            std::uint32_t source = source_end;
            std::uint32_t destination = end;
            for (std::uint32_t index = 0u; signed_of(middle) > 0 && index != middle; ++index) {
                source -= 24u;
                destination -= 24u;
                std::uint32_t words[6];
                for (std::uint32_t k = 0u; k < 6u; ++k) words[k] = word(rt, source + k * 4u);
                for (std::uint32_t k = 0u; k < 6u; ++k) put(rt, destination + k * 4u, words[k]);
            }
            call_set(rt, ctx, 0x08C4AF5Cu, {position, position + count * 24u, copy});
            return;
        }
        call_set(rt, ctx, 0x08C4AEACu, {end, count - after, copy, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 24u;
        put(rt, vector + 4u, new_end);
        call_set(rt, ctx, 0x08C4AFB4u, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (before << 3));
        call_set(rt, ctx, 0x08C4AF5Cu, {position, end, copy});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = elements24(end - begin);
    put(rt, frame.at(8u), size);
    if (0x0AAAAAAAu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4AFCu});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t block = 0u;
    std::uint32_t bytes = 0xFFFFFFF0u;
    if (!(total < size)) {
        bytes = 0u;
        if (total != 0u) bytes = total * 24u;
    }
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08C4B568u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08C4B50Cu, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08C4B568u, {position, word(rt, vector + 4u), front + count * 24u, frame.at(1u)});
    call_set(rt, ctx, 0x08C498C4u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    ctx.gpr[2] = block + bytes;
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}
//REG HOT116(0x08C4BD80u, func_08C4BD80, Word);
} // namespace hot_batch_116
} // namespace gt::game

#define HOT116(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_116::name, gt::ReturnKind::kind)
HOT116(0x088464ECu, func_088464EC, Word);
HOT116(0x08861CB0u, func_08861CB0, Word);
HOT116(0x0886D88Cu, func_0886D88C, Word);
HOT116(0x08883E74u, func_08883E74, Word);
HOT116(0x08888D58u, func_08888D58, Word);
HOT116(0x0888C750u, func_0888C750, Word);
HOT116(0x08892C28u, func_08892C28, Word);
HOT116(0x0896E9F4u, func_0896E9F4, Word);
HOT116(0x08A4AFB4u, func_08A4AFB4, Word);
HOT116(0x08AA3F44u, func_08AA3F44, Word);
HOT116(0x08AAADC8u, func_08AAADC8, Word);
HOT116(0x08AB398Cu, func_08AB398C, Word);
HOT116(0x08ADE5A4u, func_08ADE5A4, Word);
HOT116(0x08B2CB18u, func_08B2CB18, Word);
HOT116(0x08B6E920u, func_08B6E920, Float);
// HOT116(0x08BA996Cu, func_08BA996C, Word);  // batch 116: mismatched in the full gate (v0 on the in-place insert path: original 0, decompiled a pointer); to fix
HOT116(0x08BBFAACu, func_08BBFAAC, Word);
HOT116(0x08BC17C8u, func_08BC17C8, Word);
HOT116(0x08BDC338u, func_08BDC338, Word);
HOT116(0x08BDFB84u, func_08BDFB84, Word);
HOT116(0x08BE6708u, func_08BE6708, Word);
HOT116(0x08BE6DB8u, func_08BE6DB8, Word);
HOT116(0x08BE8700u, func_08BE8700, Word);
HOT116(0x08C2C56Cu, func_08C2C56C, Word);
HOT116(0x08C4BD80u, func_08C4BD80, Word);
