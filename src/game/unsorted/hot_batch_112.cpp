// Hot batch 112: functions of 81-140 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_112 {
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
    if (rep == empty) {
        ctx.gpr[2] = empty;  // the original compares the rep with v0 = the empty rep, which a Word caller returns (synthetic cases)
        return;
    }
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// func_088FEFC0(source, target): the shared-pointer copy assignment target = source as the compiler expands it through a
// temporary copy: the source's use count (+4 of its control block) goes up for the copy and again for the target unless
// the target already shares that block (then the target keeps it); the target's old block is released (use count;
// virtual slot 8, then weak count +8 and slot 0xC on the last); last, the temporary is released, its final weak release
// a tail jump to the slot 0xC.
void func_088FEFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t block = word(rt, source + 4u);
    const std::uint32_t pointer = word(rt, source);
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const std::uint32_t old = word(rt, target + 4u);
    put(rt, target, pointer);
    if (block != old) {
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t previous = word(rt, target + 4u);
        if (previous != 0u) release_shared(rt, ctx, previous);
        put(rt, target + 4u, block);
    }
    if (block == 0u) return;
    if (!count_down(rt, ctx, block + 4u)) return;
    call_virtual_at(rt, ctx, block, 8u, {block});
    if (!count_down(rt, ctx, block + 8u)) return;
    ctx.gpr[2] = word(rt, block);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + 0xCu);
    tail_call(ctx, ctx.gpr[25], {block});
}

// func_08C22ED8(vector, position, value): the libstdc++ vector<word>::_M_insert_aux: with room, the last word is copied one
// past the end, the end moves and [position, end - 1) is moved up one word (func_08B79BAC) before *position = *value.
// Full: a new block of twice the size (at least 1 word; func_08B5ADDC(0x08CB026C) at the maximum) from
// func_08A5BC20(bytes, 4); the halves are copied around the new word (func_08C1FC10), the old range destroyed
// (func_08C1FC00) and freed (func_08A5BC54), and begin / end / capacity updated.
void func_08C22ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
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
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - word(rt, vector)) >> 2);
    if (count == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB026Cu});
    std::uint32_t bytes;
    if (count == 0u) bytes = 4u;
    else bytes = count * 2u < count ? 0xFFFFFFFCu : (count * 2u) << 2;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t middle = call_set(rt, ctx, 0x08C1FC10u, {word(rt, vector), position, block, frame.at(2u)});
    if (middle != 0u) put(rt, middle, word(rt, value));
    const std::uint32_t finish = call_set(rt, ctx, 0x08C1FC10u, {position, word(rt, vector + 4u), middle + 4u, frame.at(1u)});
    call_set(rt, ctx, 0x08C1FC00u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t begin = word(rt, vector);
    if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - begin) >> 2) << 2, 4u});
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);    ctx.gpr[2] = block + bytes;
}

// func_08877B68(self): a tokenizer over a string at frame +0x14 (func_08AB85F0) gives a name (func_08AB856C) and then float
// tokens (func_08B77BF4 to double, func_08B71EC8 to float) collected in a frame vector<float> (inline, or func_08B9A5C4
// when full). When the tokenizer counted at least 2 tokens, func_0887CCA8(func_0886AFA8(self->+0) + 4, count, floats,
// name) takes them. The vector is destroyed (func_08B9A574) and freed (func_08B35254), and the string released.
void func_08877B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t tokens = frame.at(0x14u);
    const std::uint32_t floats = frame.at(8u);
    const std::uint32_t scratch = frame.at(4u);
    call_set(rt, ctx, 0x08AB85F0u, {tokens});
    const std::uint32_t name = call_set(rt, ctx, 0x08AB856Cu, {tokens});
    put(rt, floats, 0u);
    put(rt, floats + 4u, 0u);
    put(rt, floats + 8u, 0u);
    for (;;) {
        const std::uint32_t token = call_set(rt, ctx, 0x08AB856Cu, {tokens});
        if (token == 0u || sext8(byte_u(rt, token)) == 0u) break;
        const std::uint32_t low = call_set(rt, ctx, 0x08B77BF4u, {token});
        call_set(rt, ctx, 0x08B71EC8u, {low, ctx.gpr[3]});
        put_float(rt, scratch, ctx.fpr[0]);
        const std::uint32_t end = word(rt, floats + 4u);
        if (end == word(rt, floats + 8u)) {
            call_set(rt, ctx, 0x08B9A5C4u, {floats, end, scratch});
            continue;
        }
        std::uint32_t next = 4u;
        if (end != 0u) {
            put_float(rt, end, ctx.fpr[0]);
            next = word(rt, floats + 4u) + 4u;
        }
        put(rt, floats + 4u, next);
    }
    if (!(signed_of(word(rt, frame.at(0x1Cu))) < 2)) {
        const std::uint32_t owner = call_set(rt, ctx, 0x0886AFA8u, {word(rt, self)});
        const std::uint32_t begin = word(rt, floats);
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, floats + 4u) - begin) >> 2);
        call_set(rt, ctx, 0x0887CCA8u, {owner + 4u, count, begin, name});
    }
    call_set(rt, ctx, 0x08B9A574u, {word(rt, floats), word(rt, floats + 4u), frame.at(1u)});
    if (word(rt, floats) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, floats)});
    release_string(rt, ctx, tokens, 0x08D75298u, 0x08B93554u, frame.at(0u));
}

// func_0894419C(): fills constant tables: the six bytes at 0x08CF2AE0 / 0x08CF2AE8 / 0x08CF2AF0 = 1 / 10 / 0; after
// func_0894F158() and func_08944304(), for k = 0..10 with a = 50 + 10k and b = 44 + 7k: 0x08CF2B8C[k] = (a - 40) x 0.01,
// 0x08CF2BBC[k] = a x 0.01 and 0x08CF2B2C[k] = 0x08CF2B5C[k] = b x 0.01 (floats); and the 12 words at 0x08CF2AF8: 0, then
// (i - 1) / 10 for i = 1..11.
void func_0894419C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t index = 0u; index < 6u; ++index) {
        put8(rt, 0x08CF2AF0u + index, 0u);
        put8(rt, 0x08CF2AE8u + index, 10u);
        put8(rt, 0x08CF2AE0u + index, 1u);
    }
    call_set(rt, ctx, 0x0894F158u, {});
    call_set(rt, ctx, 0x08944304u, {});
    const float hundredth = std::bit_cast<float>(0x3C23D70Au);
    std::int32_t b = 44;
    std::uint32_t slot = 0u;
    for (std::int32_t a = 50; a != 160; a += 10, b += 7, slot += 4u) {
        const float low = psp_mul(static_cast<float>(a - 40), hundredth);
        const float high = psp_mul(static_cast<float>(a), hundredth);
        const float width = psp_mul(static_cast<float>(b), hundredth);
        put_float(rt, 0x08CF2B8Cu + slot, low);
        put_float(rt, 0x08CF2BBCu + slot, high);
        put_float(rt, 0x08CF2B2Cu + slot, width);
        put_float(rt, 0x08CF2B5Cu + slot, width);
    }
    put(rt, 0x08CF2AF8u, 0u);
    for (std::int32_t index = 1; index < 12; ++index) put_float(rt, 0x08CF2AF8u + static_cast<std::uint32_t>(index) * 4u, static_cast<float>(index - 1) / 10.0f);
    ctx.gpr[2] = 0u;
}

// func_08B8B33C(first, last): the libstdc++ __final_insertion_sort over 24-byte elements {word, word, vector of halfwords}:
// up to 16 elements, func_08B8B0BC(first, last) (insertion sort); otherwise that over the first 16 and then, for each later
// element, an unguarded linear insert (func_08B8AFE8) of a frame copy whose vector is duplicated (func_08A5BC20,
// func_08B8A654) and destroyed after (func_08B8A644, func_08A5BC54).
void func_08B8B33C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(last - first) >> 3) * 0xAAAAAAABu;
    if (signed_of(count) < 17) {
        call_set(rt, ctx, 0x08B8B0BCu, {first, last});
        return;
    }
    const std::uint32_t threshold = first + 0x180u;
    call_set(rt, ctx, 0x08B8B0BCu, {first, threshold});
    const std::uint32_t copy = frame.at(8u);
    const std::uint32_t vector = frame.at(0x10u);
    for (std::uint32_t element = threshold; element != last; element += 0x18u) {
        put(rt, copy + 4u, word(rt, element + 4u));
        put(rt, copy, word(rt, element));
        const std::uint32_t bytes = (word(rt, element + 0xCu) - word(rt, element + 8u)) & ~1u;
        put(rt, vector, 0u);
        put(rt, vector + 4u, 0u);
        put(rt, vector + 8u, 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        const std::uint32_t capacity = block + bytes;
        put(rt, vector, block);
        put(rt, vector + 4u, block);
        put(rt, vector + 8u, capacity);
        const std::uint32_t end = call_set(rt, ctx, 0x08B8A654u, {word(rt, element + 8u), word(rt, element + 0xCu), block, frame.at(1u)});
        put(rt, vector + 4u, end);
        call_set(rt, ctx, 0x08B8AFE8u, {element, copy, capacity});
        call_set(rt, ctx, 0x08B8A644u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t size = (static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - begin) >> 1)) << 1;
        ctx.gpr[5] = size;
        ctx.gpr[6] = 4u;
        if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, size, 4u});
    }
}

// func_0882F04C(target, key, count, args): nothing when count < 0. With i = args->+0's virtual slot 0x3C and r the record
// func_088294D0(w) of the value object func_088297A8(temp, key) fills (destroyed after; func_08A5BB7C(0x08C9A2F8) for a
// zero word), o = func_088DCD28(func_0885EDF8(r), i): f = -1.0 when o is 0, else o's virtual slot 0x28(o, count >= 2 ?
// args->+4's slot 0x3C : -1) (func_08A5BB7C(0x08C9A2B4) first for a null argument object); func_08A87EEC(temp; f12 = f)
// builds a value object that goes to target.
void func_0882F04C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t args = ctx.gpr[7];
    if (signed_of(count) < 0) return;
    GuestFrame frame(ctx, 0x30u);
    const auto slot = [&](std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        return call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    };
    const std::uint32_t index = slot(args);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x088297A8u, {source, key});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t record = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t owner = call_set(rt, ctx, 0x0885EDF8u, {record});
    const std::uint32_t object = call_set(rt, ctx, 0x088DCD28u, {owner, index});
    float result = -1.0f;
    if (object != 0u) {
        const std::uint32_t argument = signed_of(count) < 2 ? 0xFFFFFFFFu : slot(args + 4u);
        call_virtual_at(rt, ctx, object, 0x28u, {object, argument});
        result = ctx.fpr[0];
    }
    const std::uint32_t temporary = frame.at(0u);
    call_setf(rt, ctx, 0x08A87EECu, {temporary}, {result});
    keep_temporary(rt, ctx, target, temporary);
}

// func_088A6DE4(holder) -> a tail jump to func_088A6C80(holder->+0).
void func_088A6DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088A6C80u, {word(rt, ctx.gpr[4])});
}

// func_08C3D12C(vector, position, value): the libstdc++ vector<word>::_M_insert_aux with operator new: with room as in
// func_08C22ED8; full, a block of twice the size (1 word at least, capped at 0x3FFFFFFF words, func_08B5A9C8 beyond;
// func_08B5ADDC(0x08CB4918) at the maximum length) from func_08B62344; both halves are moved around the new word
// (func_08B79BAC), the old block freed (func_08B64654) and begin / end / capacity updated.
void func_08C3D12C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
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
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - word(rt, vector)) >> 2);
    if (count == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4918u});
    std::uint32_t words = 1u;
    if (count != 0u) words = count * 2u < count ? 0x3FFFFFFFu : count * 2u;
    if (0x3FFFFFFFu < words) call_set(rt, ctx, 0x08B5A9C8u, {});
    const std::uint32_t bytes = words << 2;
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {bytes});
    const std::uint32_t before = position - word(rt, vector);
    call_set(rt, ctx, 0x08B79BACu, {block, word(rt, vector), before});
    std::uint32_t middle = block + before;
    if (middle != 0u) put(rt, middle, word(rt, value));
    middle += 4u;
    const std::uint32_t after = word(rt, vector + 4u) - position;
    call_set(rt, ctx, 0x08B79BACu, {middle, position, after});
    if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B64654u, {word(rt, vector)});
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, middle + after);
    put(rt, vector, block);    ctx.gpr[2] = block + bytes;
}

// func_0885C3C8(self): under the lock at self + 8 resets the state block o = self->+0: +4 = +8 = 0, +0xC = 1, the bytes
// +0x14..+0x19 cleared, +0x10, +0x28 and +0x2C = -1; func_088D5AFC(o->+0x7C), func_0889406C(o->+0x80),
// func_08893D08(o->+0x84), func_088D43E4(o->+0x88); the holder o->+0x78 (when set) is released (its object's virtual
// slot 0 with the holder's +0, then the free func_08B64654) and cleared; func_0885D5E0 on the four 0x188-byte records at
// o->+0; o->+0x1C and +0x20 = -1. The unlock is a tail jump.
void func_0885C3C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 8u;
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, kLock, {lock});
    std::uint32_t state = word(rt, self);
    put(rt, state + 4u, 0u);
    put(rt, state + 8u, 0u);
    put(rt, state + 0xCu, 1u);
    for (const std::uint32_t offset : {0x14u, 0x16u, 0x15u, 0x17u, 0x18u}) put8(rt, state + offset, 0u);
    put(rt, state + 0x10u, 0xFFFFFFFFu);
    put8(rt, state + 0x19u, 0u);
    put(rt, state + 0x2Cu, 0xFFFFFFFFu);
    put(rt, state + 0x28u, 0xFFFFFFFFu);
    call_set(rt, ctx, 0x088D5AFCu, {word(rt, word(rt, self) + 0x7Cu)});
    call_set(rt, ctx, 0x0889406Cu, {word(rt, word(rt, self) + 0x80u)});
    call_set(rt, ctx, 0x08893D08u, {word(rt, word(rt, self) + 0x84u)});
    call_set(rt, ctx, 0x088D43E4u, {word(rt, word(rt, self) + 0x88u)});
    state = word(rt, self);
    const std::uint32_t holder = word(rt, state + 0x78u);
    if (holder != 0u) {
        const std::uint32_t object = word(rt, holder + 4u);
        if (object != 0u) {
            call_virtual_at(rt, ctx, object, 0u, {object, word(rt, holder)});
            put(rt, holder + 4u, 0u);
            put(rt, holder, 0u);
        }
        call_set(rt, ctx, 0x08B64654u, {holder});
        state = word(rt, self);
    }
    put(rt, state + 0x78u, 0u);
    for (std::uint32_t offset = 0u; offset != 0x620u; offset += 0x188u) call_set(rt, ctx, 0x0885D5E0u, {word(rt, word(rt, self)) + offset});
    for (std::uint32_t index = 0u; index < 2u; ++index) put(rt, word(rt, self) + 0x1Cu + index * 4u, 0xFFFFFFFFu);
    tail_call(ctx, kUnlock, {lock});
}

// func_088D57A0(holder, src) -> src + func_088D4800(): copies the 0x82-byte record src to holder->+0 (word by word, either
// side may be unaligned), then func_088EF51C(holder->+0 + 0x82).
void func_088D57A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t dst = word(rt, holder);
    for (std::uint32_t offset = 0u; offset < 0x82u; ++offset) put8(rt, dst + offset, byte_u(rt, src + offset));
    call_set(rt, ctx, 0x088EF51Cu, {word(rt, holder) + 0x82u});
    ctx.gpr[2] = src + call_set(rt, ctx, 0x088D4800u, {});
}

// func_0897F1D8(self, text): under the lock at 0x08CF4100 the string self->+0x1E4 + 8 is replaced by text (cleared through
// a swap with an empty temporary, func_08BEEF60, when text is empty; otherwise assigned by func_08B8A4C0(.., text,
// strlen)), comparing old and new (func_08B36170 over the shorter length, then the length difference); func_0897E2E0(self)
// runs after the unlock when they differ. The old copy (func_08B88F44) is released.
void func_0897F1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLockWord = 0x08CF4100u;
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t old = frame.at(8u);
    call_set(rt, ctx, kLock, {kLockWord});
    call_set(rt, ctx, 0x08B88F44u, {old, word(rt, self + 0x1E4u) + 8u});
    const std::uint32_t text = word(rt, holder);
    if (word(rt, text - 0xCu) == 0u) {
        const std::uint32_t empty = frame.at(4u);
        put(rt, empty, kEmpty + 0xCu);
        call_set(rt, ctx, 0x08BEEF60u, {empty, word(rt, self + 0x1E4u) + 8u});
        release_string(rt, ctx, empty, kEmpty, 0x08B8542Cu, frame.at(1u));
    } else {
        const std::uint32_t target = word(rt, self + 0x1E4u) + 8u;
        const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {text});
        call_set(rt, ctx, 0x08B8A4C0u, {target, text, length});
    }
    const std::uint32_t current = word(rt, word(rt, self + 0x1E4u) + 8u);
    const std::uint32_t previous = word(rt, old);
    const std::uint32_t new_length = word(rt, current - 0xCu);
    const std::uint32_t old_length = word(rt, previous - 0xCu);
    put(rt, frame.at(0xCu), new_length);
    put(rt, frame.at(0x10u), old_length);
    const std::uint32_t shorter = old_length < new_length ? old_length : new_length;
    const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {current, previous, shorter});
    const std::uint32_t differs = compared != 0u ? compared : new_length - old_length;
    release_string(rt, ctx, old, kEmpty, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, kUnlock, {kLockWord});
    if (differs != 0u) call_set(rt, ctx, 0x0897E2E0u, {self});
}

// func_08AC9B54(owner, description) -> the entry (+0x28) of the item func_08AC99A4(id) finds for the 64-bit id
// description->+0 (sign-extended); otherwise a new item func_08AC99F4(id) gets a 0x14-byte loader (func_08B62344,
// func_08AD5590) that func_08AD5524 initialises from description (on failure the loader is deleted through its virtual slot
// 4, func_08AC9240(item) and the result is 0): func_08AD542C(loader), r = func_08ADC988(loader->+0x10, &tmp),
// func_08AD36AC(item, r), the loader appended to the item's vector +0x30 (func_08C3D12C when full),
// func_08AC963C(item, loader->+0x10, &tmp, r, 0, 0), and the item's +0x28 / +0x2C = owner / item.
void func_08AC9B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t description = ctx.gpr[5];
    GuestFrame frame(ctx, 0x830u);
    const std::uint32_t id = word(rt, description);
    std::uint32_t item = call_set(rt, ctx, 0x08AC99A4u, {id, static_cast<std::uint32_t>(signed_of(id) >> 31)});
    if (item != 0u) {
        ctx.gpr[2] = item + 0x28u;
        return;
    }
    const std::uint32_t again = word(rt, description);
    item = call_set(rt, ctx, 0x08AC99F4u, {again, static_cast<std::uint32_t>(signed_of(again) >> 31)});
    const std::uint32_t loader = call_set(rt, ctx, 0x08B62344u, {0x14u});
    call_set(rt, ctx, 0x08AD5590u, {loader});
    if (call_set(rt, ctx, 0x08AD5524u, {loader, description}) == 0u) {
        if (loader != 0u) call_virtual_at(rt, ctx, loader, 4u, {loader});
        call_set(rt, ctx, 0x08AC9240u, {item});
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08AD542Cu, {loader});
    const std::uint32_t source = word(rt, loader + 0x10u);
    const std::uint32_t scratch = frame.at(4u);
    const std::uint32_t result = call_set(rt, ctx, 0x08ADC988u, {source, scratch});
    call_set(rt, ctx, 0x08AD36ACu, {item, result});
    put(rt, frame.at(0u), loader);
    const std::uint32_t vector = item + 0x30u;
    const std::uint32_t end = word(rt, vector + 4u);
    if (end == word(rt, vector + 8u)) {
        call_set(rt, ctx, 0x08C3D12Cu, {vector, end, frame.at(0u)});
    } else {
        std::uint32_t next = 4u;
        if (end != 0u) {
            put(rt, end, loader);
            next = word(rt, vector + 4u) + 4u;
        }
        put(rt, vector + 4u, next);
    }
    call_set(rt, ctx, 0x08AC963Cu, {item, source, scratch, result, 0u, 0u});
    if (item == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, item + 0x28u, owner);
    put(rt, item + 0x2Cu, item);
    ctx.gpr[2] = item + 0x28u;
}

// func_08AEC030(count, name) -> 1 when the subsystem with its state at 0x08D41F64.. starts, else 0 (after a cleanup):
// func_08B83CE8(name) must not fail (logs 0x553 otherwise); func_08B8443C(1, 0), func_08B844D0(0, 0); count records of
// 0x6C bytes from func_08AD84B4(.., 1) go to 0x08D41F80 (count at 0x08D41F84); the Sony stub func_08C7A998(0x08C92084, 0,
// 1, 1, 0) gives the handle at 0x08D41F6C (logs 0x569 when negative); 0x08D41F64 = 1. The logs go through
// func_08AD83B8(the word at 0x08CDFB40, 0x08C91F50, line, message) when bit 0 of 0x08CDFB3C is set; the cleanup deletes a
// valid handle (func_08C7A9E0), frees the records (func_08AD85C8) and runs func_08B84698().
void func_08AEC030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kHandle = 0x08D41F6Cu;
    constexpr std::uint32_t kRecords = 0x08D41F80u;
    const std::uint32_t count = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const auto log = [&](std::uint32_t line, std::uint32_t message) {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, line, message});
    };
    const auto cleanup = [&] {
        if (signed_of(word(rt, kHandle)) >= 0) call_set(rt, ctx, 0x08C7A9E0u, {word(rt, kHandle)});
        if (word(rt, kRecords) != 0u) call_set(rt, ctx, 0x08AD85C8u, {word(rt, kRecords)});
        call_set(rt, ctx, 0x08B84698u, {});
        ctx.gpr[2] = 0u;
    };
    put(rt, kHandle, 0xFFFFFFFFu);
    const std::uint32_t opened = call_set(rt, ctx, 0x08B83CE8u, {ctx.gpr[5]});
    if (signed_of(opened) < 0) {
        log(0x553u, 0x08C920BCu);
        return cleanup();
    }
    ctx.gpr[8] = opened;
    call_set(rt, ctx, 0x08B8443Cu, {1u, 0u});
    call_set(rt, ctx, 0x08B844D0u, {0u, 0u});
    put(rt, 0x08D41F84u, count);
    const std::uint32_t records = call_set(rt, ctx, 0x08AD84B4u, {count * 0x6Cu, 1u});
    put(rt, kRecords, records);
    if (records == 0u) return cleanup();
    const std::uint32_t handle = call_set(rt, ctx, 0x08C7A998u, {0x08C92084u, 0u, 1u, 1u, 0u});
    put(rt, kHandle, handle);
    if (signed_of(handle) < 0) {
        log(0x569u, 0x08C9209Cu);
        return cleanup();
    }
    put(rt, 0x08D41F64u, 1u);
    ctx.gpr[2] = 1u;
}

// func_08C4B5C8(out, v, m) (a leaf): out = the 3x3 row-major matrix m times the vector v, each row summed as
// ((v0 x m0 + 0) + v1 x m1) + v2 x m2; when out is m itself the three rows are computed before any is stored, otherwise
// row by row (re-reading v after each store, as the asm does).
void func_08C4B5C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t v = ctx.gpr[5];
    const std::uint32_t m = ctx.gpr[6];
    const auto row = [&](std::uint32_t index) {
        const std::uint32_t base = m + index * 0xCu;
        float sum = psp_mul(float_at(rt, v), float_at(rt, base)) + 0.0f;
        sum = sum + psp_mul(float_at(rt, v + 4u), float_at(rt, base + 4u));
        return sum + psp_mul(float_at(rt, v + 8u), float_at(rt, base + 8u));
    };
    if (out == m) {
        const float first = row(0u);
        const float second = row(1u);
        const float third = row(2u);
        put_float(rt, out, first);
        put_float(rt, out + 8u, third);
        put_float(rt, out + 4u, second);
        return;
    }
    for (std::uint32_t index = 0u; index < 3u; ++index) put_float(rt, out + index * 4u, row(index));
}

// func_08AF5164(self, flag): with the byte self->+0x426 set and flag clear, the two views at self + 0x1A8 and + 0x27C get
// their virtual slot 0x14. With it clear and flag set, both views are set up for a 480 x 272 screen (func_08AFAD8C(view,
// 0x1E0, 0x110, 3, 0, 0)), and for each of (view 0x1A8, self) and (view 0x27C, self + 0xD4): the view's slot 0x18, the
// object's slot 0x3C (object, 0, 0) and func_08AF5040(f12..f19 = 0, 0, 480, 272, 0, 0, 1, 1). Then, under the lock
// 0x08D470C4, self->+0x427 = flag (the unlock is a tail jump).
void func_08AF5164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLockWord = 0x08D470C4u;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    const std::uint32_t first = self + 0x1A8u;
    const std::uint32_t second = self + 0x27Cu;
    GuestFrame frame(ctx, 0x20u);
    const auto viewport = [&] {
        ctx.fpr[16] = 0.0f;
        ctx.fpr[17] = 0.0f;
        ctx.fpr[18] = 1.0f;
        ctx.fpr[19] = 1.0f;
        call_setf(rt, ctx, 0x08AF5040u, {}, {0.0f, 0.0f, 480.0f, 272.0f});
    };
    if (byte_u(rt, self + 0x426u) != 0u) {
        if (flag == 0u) {
            call_virtual_at(rt, ctx, first, 0x14u, {first});
            call_virtual_at(rt, ctx, second, 0x14u, {second});
        }
    } else if (flag != 0u) {
        call_set(rt, ctx, 0x08AFAD8Cu, {first, 0x1E0u, 0x110u, 3u, 0u, 0u});
        call_set(rt, ctx, 0x08AFAD8Cu, {second, 0x1E0u, 0x110u, 3u, 0u, 0u});
        call_virtual_at(rt, ctx, first, 0x18u, {first});
        call_virtual_at(rt, ctx, self, 0x3Cu, {self, 0u, 0u});
        viewport();
        call_virtual_at(rt, ctx, second, 0x18u, {second});
        call_virtual_at(rt, ctx, self + 0xD4u, 0x3Cu, {self + 0xD4u, 0u, 0u});
        viewport();
    }
    call_set(rt, ctx, kLock, {kLockWord});
    put8(rt, self + 0x427u, flag);
    tail_call(ctx, kUnlock, {kLockWord});
}

// func_088D6FD4(self, delta, flag, value) -> 0 or 1: unless func_088DAA84(self, delta, flag, value) holds, a set time
// (64-bit at self->+0x130 + 0x38) is cleared with +0x40 and func_088DC1B8(self, 2, 0, 0) runs (0 when none was set).
// Otherwise t = the 64-bit func_088D9C70(self) + delta; when func_088DAADC agrees and t has reached the limit at
// self->+0x130 + 0x40, nothing more happens. Else the byte self->+0x5B9 may be set to !flag (when self->+0x640 is set,
// func_088E5B9C of it agrees and the byte +0x13 of the timer block is clear), the timer's +0x40 becomes t (or 0 when
// +0x5B9 is set and +0x13 clear), self->+0x2C = value; the byte self->+0x26 = flag in both kept paths.
void func_088D6FD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t delta = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (call_set(rt, ctx, 0x088DAA84u, {}) == 0u) {
        const std::uint32_t timer = word(rt, self + 0x130u);
        if ((word(rt, timer + 0x38u) | word(rt, timer + 0x3Cu)) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        put(rt, timer + 0x38u, 0u);
        put(rt, timer + 0x3Cu, 0u);
        const std::uint32_t block = word(rt, self + 0x130u);
        put(rt, block + 0x40u, 0u);
        put(rt, block + 0x44u, 0u);
        call_set(rt, ctx, 0x088DC1B8u, {self, 2u, 0u, 0u});
        put8(rt, self + 0x26u, flag);
        ctx.gpr[2] = 1u;
        return;
    }
    const std::uint32_t now = call_set(rt, ctx, 0x088D9C70u, {self});
    const std::uint32_t low = now + delta;
    const std::uint32_t carry = low < now ? 1u : 0u;
    const std::uint32_t high = ctx.gpr[3] + carry;
    if (call_set(rt, ctx, 0x088DAADCu, {self, 0u, carry}) != 0u) {
        const std::uint32_t block = word(rt, self + 0x130u);
        const std::uint32_t limit_high = word(rt, block + 0x44u);
        const bool before = high < limit_high || (limit_high == high && low < word(rt, block + 0x40u));
        if (!before) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
    const std::uint32_t watcher = word(rt, self + 0x640u);
    std::uint32_t block = word(rt, self + 0x130u);
    if (watcher != 0u) {
        const std::uint32_t agreed = call_set(rt, ctx, 0x088E5B9Cu, {watcher});
        block = word(rt, self + 0x130u);
        if (agreed != 0u && byte_u(rt, block + 0x13u) == 0u) put8(rt, self + 0x5B9u, flag ^ 1u);
    }
    if (byte_u(rt, self + 0x5B9u) != 0u && byte_u(rt, block + 0x13u) == 0u) {
        put(rt, block + 0x40u, 0u);
        put(rt, block + 0x44u, 0u);
    } else {
        put(rt, block + 0x40u, low);
        put(rt, block + 0x44u, high);
    }
    put(rt, self + 0x2Cu, value);
    put8(rt, self + 0x26u, flag);
    ctx.gpr[2] = 1u;
}

// func_08AAC96C(self, text) -> 0 for an empty text; 1 when it equals the string 0x08CB3374 (compared through a frame
// copy, func_08B36170 over the shorter length, then the lengths); otherwise, under the lock at self + 0x84, whether the
// map at self + 0x68 holds the key {copy of text, func_08B29E88(0, text, length)} (func_08C34FE8 != end at self + 0x6C).
void func_08AAC96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::uint32_t lock = self + 0x84u;
    if (word(rt, word(rt, text) - 0xCu) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t name = frame.at(4u);
    call_set(rt, ctx, 0x08B85EB8u, {name, 0x08CB3374u, frame.at(2u)});
    const std::uint32_t mine = word(rt, text);
    const std::uint32_t theirs = word(rt, name);
    const std::uint32_t mine_length = word(rt, mine - 0xCu);
    const std::uint32_t their_length = word(rt, theirs - 0xCu);
    put(rt, frame.at(8u), mine_length);
    put(rt, frame.at(0xCu), their_length);
    const std::uint32_t shorter = their_length < mine_length ? their_length : mine_length;
    const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {mine, theirs, shorter});
    const std::uint32_t differs = compared != 0u ? compared : mine_length - their_length;
    release_string(rt, ctx, name, kEmpty, 0x08B8542Cu, frame.at(1u));
    ctx.gpr[4] = lock;
    if (differs == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    call_set(rt, ctx, kLock, {lock});
    const std::uint32_t key = frame.at(0x10u);
    call_set(rt, ctx, 0x08B88F44u, {key, text});
    const std::uint32_t data = word(rt, text);
    put(rt, key + 4u, call_set(rt, ctx, 0x08B29E88u, {0u, data, word(rt, data - 0xCu)}));
    const std::uint32_t found = call_set(rt, ctx, 0x08C34FE8u, {self + 0x68u, key});
    release_string(rt, ctx, key, kEmpty, 0x08B8542Cu, frame.at(0u));
    const std::uint32_t result = found != self + 0x6Cu ? 1u : 0u;
    call_set(rt, ctx, kUnlock, {lock});
    ctx.gpr[2] = result;
}

// func_08B6728C(f12 = x, f13 = y) -> f0: the libm two-argument wrapper (domain error when y = 0) with SVID error handling:
// r = func_08B6AD94(x, y); unless the mode word at 0x08C96698 is -1, y or x is NaN (func_08B6ECB0) or y is not 0, a record
// {type 1, name 0x08CB5E5C, arg1 = (double) x, arg2 = (double) y, err 0} gets retval = x in mode 0, else 0.0 / 0.0
// (func_08B71720); in mode 2, or when func_08B6E524 (matherr) does not handle it, errno = 0x21 (EDOM); a nonzero err goes
// to errno; the result is (float) retval.
void func_08B6728C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    GuestFrame frame(ctx, 0x50u);
    call_setf(rt, ctx, 0x08B6AD94u, {}, {x, y});
    const float result = ctx.fpr[0];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    const auto keep = [&] { ctx.fpr[0] = result; };
    if (mode == 0xFFFFFFFFu) return keep();
    if (call_setf(rt, ctx, 0x08B6ECB0u, {}, {y}).v0 != 0u) return keep();
    if (call_setf(rt, ctx, 0x08B6ECB0u, {}, {x}).v0 != 0u) return keep();
    if (!(y == 0.0f)) return keep();
    const std::uint32_t record = frame.at(0u);
    put(rt, record, 1u);
    put(rt, record + 4u, 0x08CB5E5Cu);
    put(rt, record + 0x20u, 0u);
    const std::uint32_t x_low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
    const std::uint32_t x_high = ctx.gpr[3];
    put(rt, record + 8u, x_low);
    put(rt, record + 0xCu, x_high);
    const std::uint32_t y_low = call_setf(rt, ctx, 0x08B70F28u, {}, {y}).v0;
    put(rt, record + 0x10u, y_low);
    put(rt, record + 0x14u, ctx.gpr[3]);
    bool raise = false;
    if (mode == 0u) {
        put(rt, record + 0x18u, x_low);
        put(rt, record + 0x1Cu, x_high);
    } else {
        const std::uint32_t nan_low = call_set(rt, ctx, 0x08B71720u, {0u, 0u, 0u, 0u});
        put(rt, record + 0x18u, nan_low);
        put(rt, record + 0x1Cu, ctx.gpr[3]);
        raise = mode == 2u;
    }
    if (raise || call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), 0x21u);
    if (word(rt, record + 0x20u) != 0u) {
        const std::uint32_t error = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, error, word(rt, record + 0x20u));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
}

// func_088A6F74(holder) -> a tail jump to func_088A6DEC(holder->+0).
void func_088A6F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088A6DECu, {word(rt, ctx.gpr[4])});
}

// func_08B67418(f12 = x) -> f0: the libm logarithm wrapper with SVID error handling: r = func_08B6AF7C(x); unless the mode
// word at 0x08C96698 is -1, x is NaN (func_08B6ECB0) or x > 0, a record {name 0x08CB5E64, arg1 = arg2 = (double) x, err 0,
// retval = the double at 0x08CB6608 in mode 0, else minus the double at 0x08C96690} gets type 2 (x = 0; in mode 2 errno
// = 0x22) or 1 (x < 0); otherwise func_08B6E524 (matherr) decides and errno = 0x21 when it does not handle it (also for
// x < 0 in mode 2). A nonzero err goes to errno; the result is (float) retval.
void func_08B67418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    call_setf(rt, ctx, 0x08B6AF7Cu, {}, {x});
    const float result = ctx.fpr[0];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    const auto keep = [&] { ctx.fpr[0] = result; };
    if (mode == 0xFFFFFFFFu) return keep();
    if (call_setf(rt, ctx, 0x08B6ECB0u, {}, {x}).v0 != 0u) return keep();
    if (0.0f < x) return keep();
    const std::uint32_t record = frame.at(0u);
    put(rt, record + 4u, 0x08CB5E64u);
    put(rt, record + 0x20u, 0u);
    const std::uint32_t low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
    const std::uint32_t high = ctx.gpr[3];
    put(rt, record + 8u, low);
    put(rt, record + 0xCu, high);
    put(rt, record + 0x10u, low);
    put(rt, record + 0x14u, high);
    if (mode == 0u) {
        put(rt, record + 0x18u, word(rt, 0x08CB6608u));
        put(rt, record + 0x1Cu, word(rt, 0x08CB660Cu));
    } else {
        put(rt, record + 0x18u, word(rt, 0x08C96690u));
        put(rt, record + 0x1Cu, word(rt, 0x08C96694u) ^ 0x80000000u);
    }
    std::uint32_t error = 0u;
    if (x == 0.0f) {
        put(rt, record, 2u);
        if (mode == 2u) error = 0x22u;
        else if (call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) error = 0x21u;
    } else {
        put(rt, record, 1u);
        if (mode == 2u || call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) error = 0x21u;
    }
    if (error != 0u) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), error);
    if (word(rt, record + 0x20u) != 0u) {
        const std::uint32_t slot = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, word(rt, record + 0x20u));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
}

// func_08B196B8(out, node, path, fallback) -> out: walks the path from the node (a 9-byte handle {word, word, byte}): for
// each segment up to the separator func_08B18B58(segment) finds, an index in brackets after it ("[...]", parsed by
// func_08B18B98 up to the ']' strchr finds) or else the fallback (for the last segment without a separator) selects
// r = func_08B1916C(node, segment, separator), and node = func_08B19538(&tmp, node, r, index) (func_08B18B18 assigns,
// func_08B18A98 ends tmp). A ".", after a closing bracket, is skipped. A missing ']' or child gives the null handle;
// otherwise out = a copy of the final node (func_08B18A58).
void func_08B196B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    std::uint32_t segment = ctx.gpr[6];
    const std::uint32_t fallback = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    const auto null_handle = [&] {
        put(rt, out, 0u);
        put(rt, out + 4u, 0u);
        put8(rt, out + 8u, 0u);
        ctx.gpr[2] = out;
    };
    if (segment != 0u) {
        while (sext8(byte_u(rt, segment)) != 0u) {
            const std::uint32_t separator = call_set(rt, ctx, 0x08B18B58u, {segment});
            std::uint32_t close = 0u;
            std::uint32_t index = 0u;
            if (separator != 0u && sext8(byte_u(rt, separator)) == 0x5Bu) {
                close = call_set(rt, ctx, 0x08B7BA50u, {separator + 1u, 0x5Du});
                if (close == 0u) return null_handle();
                index = call_set(rt, ctx, 0x08B18B98u, {separator + 1u, close});
            }
            const std::uint32_t child = call_set(rt, ctx, 0x08B1916Cu, {node, segment, separator});
            const std::uint32_t chosen = (close | separator) != 0u ? index : fallback;
            if (child == 0u) return null_handle();
            call_set(rt, ctx, 0x08B19538u, {temporary, node, child, chosen});
            call_set(rt, ctx, 0x08B18B18u, {node, temporary});
            call_set(rt, ctx, 0x08B18A98u, {temporary});
            if (separator == 0u) break;
            if (close == 0u) {
                segment = separator + 1u;
            } else {
                segment = sext8(byte_u(rt, close + 1u)) == 0x2Eu ? close + 2u : close + 1u;
            }
        }
    }
    call_set(rt, ctx, 0x08B18A58u, {out, node});
    ctx.gpr[2] = out;
}

// func_08B67C70(f12 = x) -> f0: the libm wrapper for a function that overflows to plus or minus infinity with the sign of
// x (sinh-like), with SVID error handling: r = func_08B6BE8C(x); unless the mode word at 0x08C96698 is -1, r is finite or
// x is not (func_08B6EC94), an overflow record {type 3, name 0x08CB5E7C, arg1 = arg2 = (double) x, err 0} gets
// retval = (x > 0 ? +1 : -1) x the double at 0x08C96690 (modes other than 0) or the doubles at 0x08CB6630 / 0x08CB6638
// (mode 0); in mode 2, or when func_08B6E524 (matherr) does not handle it, errno = 0x22 (ERANGE); a nonzero err goes to
// errno; the result is (float) retval.
void func_08B67C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    call_setf(rt, ctx, 0x08B6BE8Cu, {}, {x});
    const float result = ctx.fpr[0];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    const auto keep = [&] { ctx.fpr[0] = result; };
    if (mode == 0xFFFFFFFFu) return keep();
    if (call_setf(rt, ctx, 0x08B6EC94u, {}, {result}).v0 != 0u) return keep();
    if (call_setf(rt, ctx, 0x08B6EC94u, {}, {x}).v0 == 0u) return keep();
    const std::uint32_t record = frame.at(0u);
    put(rt, record, 3u);
    put(rt, record + 4u, 0x08CB5E7Cu);
    put(rt, record + 0x20u, 0u);
    const std::uint32_t low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
    const std::uint32_t high = ctx.gpr[3];
    put(rt, record + 8u, low);
    put(rt, record + 0xCu, high);
    put(rt, record + 0x10u, low);
    put(rt, record + 0x14u, high);
    const bool positive = 0.0f < x;
    bool handled = false;
    if (mode != 0u) {
        put(rt, record + 0x18u, word(rt, 0x08C96690u));
        put(rt, record + 0x1Cu, positive ? word(rt, 0x08C96694u) : word(rt, 0x08C96694u) ^ 0x80000000u);
        if (mode != 2u) handled = call_set(rt, ctx, 0x08B6E524u, {record}) != 0u;
    } else {
        put(rt, record + 0x18u, word(rt, positive ? 0x08CB6630u : 0x08CB6638u));
        put(rt, record + 0x1Cu, word(rt, positive ? 0x08CB6634u : 0x08CB663Cu));
        handled = call_set(rt, ctx, 0x08B6E524u, {record}) != 0u;
    }
    if (!handled) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), 0x22u);
    if (word(rt, record + 0x20u) != 0u) {
        const std::uint32_t slot = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, word(rt, record + 0x20u));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
}

// func_089ABDCC(self, args): a script object's update: a frame value object (func_089A6A7C) takes self->+0x68 under the
// lock 0x08CF4ACC (self->+0x6C (byte) = func_089A7120(it) when set, and self->+0x68 is reset to a null object made by
// func_089A7644); after func_08B1C47C(), the object (when set) gets func_089A81C4(object, self), then
// func_089A6C20(object, args->+0 ? func_089A70D4(args->+0) : 0) (func_08A5BB7C(0x08CA50AC) for null objects) and
// func_089A71C8(object, self); under the lock it goes back to self->+0x68, and the frame object is destroyed.
void func_089ABDCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kLockWord = 0x08CF4ACCu;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t args = ctx.gpr[5];
    const std::uint32_t slot = self + 0x68u;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t held = frame.at(4u);
    const std::uint32_t empty = frame.at(0u);
    call_set(rt, ctx, 0x089A6A7Cu, {held});
    call_set(rt, ctx, kLock, {kLockWord});
    if (held != slot) call_set(rt, ctx, kAssign, {held, word(rt, slot)});
    if (word(rt, held) != 0u) put8(rt, self + 0x6Cu, call_set(rt, ctx, 0x089A7120u, {word(rt, held)}));
    call_set(rt, ctx, 0x089A7644u, {empty, 0u});
    keep_temporary(rt, ctx, slot, empty);
    call_set(rt, ctx, kUnlock, {kLockWord});
    call_set(rt, ctx, 0x08B1C47Cu, {});
    std::uint32_t object = word(rt, held);
    if (object != 0u) {
        call_set(rt, ctx, 0x089A81C4u, {object, self});
        object = word(rt, held);
    }
    std::uint32_t argument = word(rt, args);
    if (argument != 0u) {
        if (object == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50ACu});
            argument = word(rt, args);
        }
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50ACu});
        call_set(rt, ctx, 0x089A6C20u, {object, call_set(rt, ctx, 0x089A70D4u, {argument})});
    } else {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50ACu});
        call_set(rt, ctx, 0x089A6C20u, {object, 0u});
    }
    if (word(rt, held) != 0u) call_set(rt, ctx, 0x089A71C8u, {word(rt, held), self});
    call_set(rt, ctx, kLock, {kLockWord});
    if (slot != held) call_set(rt, ctx, kAssign, {slot, word(rt, held)});
    call_set(rt, ctx, kUnlock, {kLockWord});
    call_set(rt, ctx, kDestroy, {held});
}

// func_089EF70C(target, a1, count, args): a script property of the record r, the word of the value object func_089EE140
// fills (func_08A5BB7C(0x08CA7EB4) when 0; destroyed after): with count > 0, r->+0x22C = func_08B20A54(0x08CF5874, the
// string args->+0's virtual slot 0xC fills, -1) (func_08A5BB7C(0x08CA7E0C) first for a null object; the string is
// released after); otherwise func_08A6D2D4(temp, &string of func_08B20968(0x08CF5874, r->+0x22C)) builds a value object
// that goes to target.
void func_089EF70C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (signed_of(ctx.gpr[6]) > 0) {
        const std::uint32_t object = word(rt, args);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7E0Cu});
        const std::uint32_t text = frame.at(0x14u);
        const std::uint32_t source = frame.at(0x10u);
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
        call_set(rt, ctx, ctx.gpr[2], {text, object});
        call_set(rt, ctx, 0x089EE140u, {source, second});
        const std::uint32_t record = word(rt, source);
        if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
        put(rt, record + 0x22Cu, call_set(rt, ctx, 0x08B20A54u, {0x08CF5874u, word(rt, text), 0xFFFFFFFFu}));
        call_set(rt, ctx, kDestroy, {source});
        release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
        return;
    }
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x089EE140u, {source});
    const std::uint32_t record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7EB4u});
    const std::uint32_t name = call_set(rt, ctx, 0x08B20968u, {0x08CF5874u, word(rt, record + 0x22Cu)});
    const std::uint32_t text = frame.at(8u);
    const std::uint32_t temporary = frame.at(0xCu);
    call_set(rt, ctx, 0x08B85EB8u, {text, name, frame.at(2u)});
    call_set(rt, ctx, 0x08A6D2D4u, {temporary, text});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, kDestroy, {source});
}

// func_08B8634C(self, position, removed, inserted): the libstdc++ copy-on-write string _M_mutate: the gap at position
// changes from `removed` to `inserted` bytes. When the new length exceeds the capacity or the rep is shared, a new rep
// (func_08B8543C(length, capacity, frame)) gets the head and the tail copied (one byte inline, else func_08B79A88), the old
// rep is released and replaces it; otherwise the tail moves in place (one byte inline, else func_08B79BAC). The rep's
// count becomes 0, its length the new one, and a terminator is written.
void func_08B8634C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t removed = ctx.gpr[6];
    const std::uint32_t inserted = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t data = word(rt, self);
    const std::uint32_t length = word(rt, data - 0xCu);
    const std::uint32_t capacity = word(rt, data - 8u);
    const std::uint32_t total = length + inserted - removed;
    const std::uint32_t tail = length - position - removed;
    const auto copy = [&](std::uint32_t to, std::uint32_t from, std::uint32_t bytes, std::uint32_t mover) {
        if (bytes == 1u) put8(rt, to, byte_u(rt, from));
        else call_set(rt, ctx, mover, {to, from, bytes});
    };
    if (capacity < total || signed_of(word(rt, data - 4u)) > 0) {
        const std::uint32_t rep = call_set(rt, ctx, 0x08B8543Cu, {total, capacity, frame.at(0u)});
        const std::uint32_t fresh = rep + 0xCu;
        if (position != 0u) copy(fresh, word(rt, self), position, 0x08B79A88u);
        if (tail != 0u) copy(fresh + position + inserted, word(rt, self) + position + removed, tail, 0x08B79A88u);
        release_string(rt, ctx, self, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        put(rt, self, fresh);
        data = fresh;
    } else if (tail != 0u && inserted != removed) {
        copy(data + position + inserted, data + position + removed, tail, 0x08B79BACu);
        data = word(rt, self);
    }
    put(rt, data - 4u, 0u);
    put(rt, data - 0xCu, total);
    put8(rt, data + total, 0u);
    ctx.gpr[2] = data + total;
}

// func_08A180CC(self, a1): fits a layout: func_08A17EBC(self, a1, &w0, &w1, &h0, &h1) gives two integer widths and two
// heights, func_08A18088(self, a1, &width, &height) the available floats. When w0 + w1 (as a float) is positive and above
// width, both widths are scaled by width / (w0 + w1) (truncated); the heights likewise with height. self->+8 = w0,
// +0xC = width - w1, +0x10 = h0, +0x14 = height - h1 (floats); func_08A17C80(self, a1, &x, &y), and self->+4 =
// (width - x) / self->+0->+0xC when that count is positive, else 0.
void func_08A180CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, 0x08A17EBCu, {self, second, frame.at(8u), frame.at(0xCu), frame.at(0x10u), frame.at(0x14u)});
    call_set(rt, ctx, 0x08A18088u, {self, second, frame.at(0u), frame.at(4u)});
    const auto fit = [&](std::uint32_t first_slot, std::uint32_t second_slot, float limit, std::uint32_t &first, std::uint32_t &other) {
        first = word(rt, first_slot);
        other = word(rt, second_slot);
        const float total = static_cast<float>(signed_of(first + other));
        if (!(0.0f < total) || !(limit < total)) return;
        const float ratio = limit / total;
        other = ctx.fpu_float_to_word(psp_mul(ratio, static_cast<float>(signed_of(other))), 1u);
        first = ctx.fpu_float_to_word(psp_mul(ratio, static_cast<float>(signed_of(first))), 1u);
        put(rt, second_slot, other);
        put(rt, first_slot, first);
    };
    const float width = float_at(rt, frame.at(0u));
    std::uint32_t w0;
    std::uint32_t w1;
    fit(frame.at(8u), frame.at(0xCu), width, w0, w1);
    const float height = float_at(rt, frame.at(4u));
    std::uint32_t h0;
    std::uint32_t h1;
    fit(frame.at(0x10u), frame.at(0x14u), height, h0, h1);
    put_float(rt, self + 8u, static_cast<float>(signed_of(w0)));
    put_float(rt, self + 0xCu, width - static_cast<float>(signed_of(w1)));
    put_float(rt, self + 0x10u, static_cast<float>(signed_of(h0)));
    put_float(rt, self + 0x14u, height - static_cast<float>(signed_of(h1)));
    call_set(rt, ctx, 0x08A17C80u, {self, second, frame.at(0x18u), frame.at(0x1Cu)});
    const std::uint32_t count = word(rt, word(rt, self) + 0xCu);
    float step = 0.0f;
    if (signed_of(count) > 0) step = (float_at(rt, frame.at(0u)) - float_at(rt, frame.at(0x18u))) / static_cast<float>(signed_of(count));
    put_float(rt, self + 4u, step);    ctx.gpr[2] = count;
}

// func_08B6EF68(low, high) -> f0: libgcc's 64-bit integer to float through doubles: (double) high x 2^32 (the double at
// 0x08CB6908) plus (double) low (as unsigned: + 2^32 when negative) with the soft-float helpers func_08B71CE0 (int to
// double), func_08B71394 (multiply), func_08B71328 (add) and func_08B71EC8 (to float). Values beyond 2^53 first get the
// low 11 bits folded into a sticky bit (0x800) so the double rounds like the float would.
void func_08B6EF68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t low = ctx.gpr[4];
    const std::uint32_t high = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t biased_low = low - 1u;
    const std::uint32_t biased_high = high + 0x1FFFFFu + (biased_low < low ? 1u : 0u);
    const bool large = 0x3FFFFFu < biased_high || (biased_high == 0x3FFFFFu && !(biased_low < 0xFFFFFFFFu));
    if (large && (low & 0x7FFu) != 0u) low = (low & ~0x7FFu) | 0x800u;
    const std::uint32_t two32_low = word(rt, 0x08CB6908u);
    const std::uint32_t two32_high = word(rt, 0x08CB690Cu);
    const std::uint32_t h_low = call_set(rt, ctx, 0x08B71CE0u, {high});
    const std::uint32_t scaled_low = call_set(rt, ctx, 0x08B71394u, {h_low, ctx.gpr[3], two32_low, two32_high});
    const std::uint32_t scaled_high = ctx.gpr[3];
    std::uint32_t l_low = call_set(rt, ctx, 0x08B71CE0u, {low});
    std::uint32_t l_high = ctx.gpr[3];
    if (signed_of(low) < 0) {
        l_low = call_set(rt, ctx, 0x08B71328u, {l_low, l_high, two32_low, two32_high});
        l_high = ctx.gpr[3];
    }
    const std::uint32_t sum_low = call_set(rt, ctx, 0x08B71328u, {scaled_low, scaled_high, l_low, l_high});
    call_set(rt, ctx, 0x08B71EC8u, {sum_low, ctx.gpr[3]});
}

// func_0880714C(target, key, count, args): nothing when count <= 0 or the index i = func_08930204(args->+0's virtual slot
// 0x50, 64-bit) is negative or above 89 (func_08A5BB7C(0x08C97DA8) for a null object). Otherwise the object o, the word
// of the value object func_08A57854(temp, key) fills (func_08A5BB7C(0x08C97DD8) when 0), gets its virtual slot 0x9C with
// (result, o, &wrapper), the wrapper (func_08A76AB4) holding a new 0x18-byte block (func_08A5BC20; func_08A76ACC, vtable
// 0x08CB6B88, func_08A6972C(block + 0x14, &v)) built around the value func_08A69B3C(&v, 0x08C97E54) (ended by
// func_08A69D68); the result's word gets its virtual slot 0x84 (temp, it, i), a value object that goes to target. The
// frame value objects are destroyed.
void func_0880714C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[7];
    if (signed_of(ctx.gpr[6]) <= 0) return;
    GuestFrame frame(ctx, 0x50u);
    std::uint32_t object = word(rt, args);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DA8u});
    const std::uint32_t low = call_virtual_at(rt, ctx, object, 0x50u, {object});
    const std::uint32_t index = call_set(rt, ctx, 0x08930204u, {low, ctx.gpr[3]});
    if (signed_of(index) < 0 || !(signed_of(index) < 0x5A)) return;
    const std::uint32_t value = frame.at(0u);
    const std::uint32_t holder = frame.at(4u);
    const std::uint32_t wrapper = frame.at(8u);
    const std::uint32_t source = frame.at(0xCu);
    const std::uint32_t result = frame.at(0x10u);
    const std::uint32_t temporary = frame.at(0x14u);
    call_set(rt, ctx, 0x08A57854u, {source, key});
    const std::uint32_t owner = word(rt, source);
    if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DD8u});
    const std::uint32_t method = word(rt, word(rt, owner) + 0x9Cu);
    call_set(rt, ctx, 0x08A69B3Cu, {value, 0x08C97E54u});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_set(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, 0x08CB6B88u);
    call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, value});
    put(rt, holder, block);
    call_set(rt, ctx, 0x08A76AB4u, {wrapper, holder});
    call_set(rt, ctx, 0x08A69D68u, {value});
    call_set(rt, ctx, method, {result, owner, wrapper});
    object = word(rt, result);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DA8u});
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x84u);
    call_set(rt, ctx, ctx.gpr[3], {temporary, object, index});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {result});
    call_set(rt, ctx, kDestroy, {wrapper});
    call_set(rt, ctx, kDestroy, {source});
}

// The shared body of func_0883C988 / func_0883CCC8 (target, count, args): nothing when count <= 0. A frame record
// (func_0893600C; ended by func_08932448) is filled by func_08935B10(record, .., func_0883B848(args) as 64 bits); its name
// `getter`(record) becomes the text: the 4 bytes at 0x08C9C45C when the name is 0x08C9C460, otherwise the name formatted
// by func_08B3767C into a 0x80-byte buffer with 0x08C9C478, or with 0x08C9C470 and func_08B48CFC(0x08D6950C) when count is
// 1 or args->+4's virtual slot 0x3C is nonzero (func_08A5BB7C(0x08C9BD48) for a null object); func_08A6D2D4(temp,
// &string) of it builds a value object that goes to target.
inline void record_name(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t getter) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t args = ctx.gpr[6];
    if (signed_of(count) <= 0) return;
    GuestFrame frame(ctx, 0x230u);
    const std::uint32_t record = frame.at(0x90u);
    const std::uint32_t buffer = frame.at(0xCu);
    call_set(rt, ctx, 0x0893600Cu, {record});
    const std::uint32_t low = call_set(rt, ctx, 0x0883B848u, {args});
    call_set(rt, ctx, 0x08935B10u, {record, ctx.gpr[5], low, ctx.gpr[3]});
    const std::uint32_t name = call_set(rt, ctx, getter, {record});
    if (call_set(rt, ctx, 0x08B35FD8u, {name, 0x08C9C460u}) == 0u) {
        for (std::uint32_t offset = 0u; offset < 4u; ++offset) put8(rt, buffer + offset, byte_u(rt, 0x08C9C45Cu + offset));
    } else {
        bool numbered = count == 1u;
        if (!numbered) {
            const std::uint32_t object = word(rt, args + 4u);
            if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
            numbered = call_virtual_at(rt, ctx, object, 0x3Cu, {object}) != 0u;
        }
        if (numbered) {
            const std::uint32_t number = call_set(rt, ctx, 0x08B48CFCu, {0x08D6950Cu});
            call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C470u, name, number});
        } else {
            call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C478u, name});
        }
    }
    const std::uint32_t text = frame.at(4u);
    const std::uint32_t temporary = frame.at(8u);
    call_set(rt, ctx, 0x08B85EB8u, {text, buffer, frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {temporary, text});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
    call_set(rt, ctx, 0x08932448u, {record});
}

// func_0883C988(target, count, args): record_name with func_0893628C.
void func_0883C988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_name(rt, ctx, 0x0893628Cu); }

// func_0883CCC8(target, count, args): record_name with func_089362D8.
void func_0883CCC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_name(rt, ctx, 0x089362D8u); }

// func_08BBBA44(vector, position, value): the libstdc++ vector::_M_insert_aux for 4-byte elements of two halfwords: with
// room, the last element is copied one past the end, [position, end - 1) moves up one element (halfword pairs, from the
// back) and *position = *value (unaligned); full, a block of twice the size (one element at least; func_08B5ADDC
// (0x08CA0C70) at the maximum) from func_08B35DA0 gets the halves around the new element (func_08BBB848), the old range is
// destroyed (func_08BBB6E8) and freed (func_08B35254), and begin / end / capacity updated.
void func_08BBBA44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) {
            const std::uint32_t high = half_u(rt, end - 4u + 2u);
            const std::uint32_t low = half_u(rt, end - 4u);
            put16(rt, end + 2u, high);
            put16(rt, end, low);
            end = word(rt, vector + 4u);
        }
        put(rt, vector + 4u, end + 4u);
        const std::uint32_t copy = byte_u(rt, value) | byte_u(rt, value + 1u) << 8 | byte_u(rt, value + 2u) << 16 | byte_u(rt, value + 3u) << 24;
        put(rt, frame.at(4u), copy);
        const std::int32_t count = signed_of((end - 4u) - position) >> 2;
        std::uint32_t to = end - 4u;
        std::uint32_t from = end - 4u;
        for (std::int32_t index = 0; index < count; ++index) {
            from -= 4u;
            const std::uint32_t low = half_u(rt, from);
            const std::uint32_t high = half_u(rt, from + 2u);
            put16(rt, to, low);
            put16(rt, to + 2u, high);
            if (index + 1 != count) to -= 4u;
        }
        for (std::uint32_t offset = 0u; offset < 4u; ++offset) put8(rt, position + offset, copy >> (offset * 8u));
        ctx.gpr[2] = copy;
        return;
    }
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - word(rt, vector)) >> 2);
    if (count == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0C70u});
    std::uint32_t bytes;
    std::uint32_t block = 0u;
    bool allocate = true;
    if (count == 0u) {
        bytes = 4u;
    } else if (count * 2u < count) {
        bytes = 0xFFFFFFFCu;
    } else {
        bytes = (count * 2u) << 2;
        if (count * 2u == 0u) allocate = false;
    }
    if (allocate) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t middle = call_set(rt, ctx, 0x08BBB848u, {word(rt, vector), position, block, frame.at(2u)});
    if (middle != 0u) {
        put16(rt, middle + 2u, half_u(rt, value + 2u));
        put16(rt, middle, half_u(rt, value));
    }
    const std::uint32_t finish = call_set(rt, ctx, 0x08BBB848u, {position, word(rt, vector + 4u), middle + 4u, frame.at(1u)});
    call_set(rt, ctx, 0x08BBB6E8u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);    ctx.gpr[2] = block + bytes;
}

// func_0884085C(target, count, args): nothing when count < 2. With s the string args->+0's virtual slot 0xC fills and
// o = args->+4 (func_08A5BB7C(0x08C9BD48) for a null object): when o is of the kind func_08A5D70C() or func_08A82A5C() names
// (func_08A76F20), v = func_0892C810(s, .., o's virtual slot 0x50 as 64 bits); otherwise func_08937A60(s, o's slot 0x3C).
// func_08A97A6C(temp, v) builds a value object that goes to target; the string is released last.
void func_0884085C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t args = ctx.gpr[6];
    if (signed_of(ctx.gpr[5]) < 2) return;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t text = frame.at(8u);
    const auto fetch = [&](std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
        return object;
    };
    std::uint32_t object = fetch(args);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {text, object});
    object = fetch(args + 4u);
    bool wide = call_set(rt, ctx, 0x08A76F20u, {object, call_set(rt, ctx, 0x08A5D70Cu, {})}) != 0u;
    if (!wide) {
        object = fetch(args + 4u);
        wide = call_set(rt, ctx, 0x08A76F20u, {object, call_set(rt, ctx, 0x08A82A5Cu, {})}) != 0u;
    }
    object = fetch(args + 4u);
    const std::uint32_t characters = word(rt, text);
    std::uint32_t value;
    if (wide) {
        const std::uint32_t low = call_virtual_at(rt, ctx, object, 0x50u, {object});
        value = call_set(rt, ctx, 0x0892C810u, {characters, ctx.gpr[5], low, ctx.gpr[3]});
    } else {
        value = call_set(rt, ctx, 0x08937A60u, {characters, call_virtual_at(rt, ctx, object, 0x3Cu, {object})});
    }
    const std::uint32_t temporary = frame.at(4u);
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, value});
    keep_temporary(rt, ctx, target, temporary);
    release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
}

// func_08978950(self): when the bit vector at self + 0x74 ({begin word, begin bit, end word, end bit, storage end}) has
// room for fewer than 32 bits, its bits are copied one by one into a new 4-byte block (func_08A5BC20(4, 4)), the old
// storage freed (func_08A5BC54) and the vector repointed. Then self->+0x180 = self's virtual slot 0x110,
// func_08A5BA28(it), func_089A94B8(it, &{0, 0, the floats at 0x08CDE764 and 0x08CDE760}), its virtual slot 0x18C, and the
// byte self->+0x1B4 is cleared.
void func_08978950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t bits = self + 0x74u;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t room = ((word(rt, bits + 0x10u) - word(rt, bits)) << 3) - word(rt, bits + 4u);
    if (room < 0x20u) {
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {4u, 4u});
        std::uint32_t source = word(rt, self + 0x74u);
        std::uint32_t source_bit = word(rt, bits + 4u);
        const std::uint32_t total = word(rt, bits + 0xCu) + ((word(rt, bits + 8u) - source) << 3) - source_bit;
        std::uint32_t target = block;
        std::uint32_t target_bit = 0u;
        for (std::uint32_t done = 0u; signed_of(total) > 0 && done != total;) {
            const std::uint32_t mask = 1u << (target_bit & 31u);
            if (((1u << (source_bit & 31u)) & word(rt, source)) != 0u) put(rt, target, mask | word(rt, target));
            else put(rt, target, word(rt, target) & ~mask);
            ++source_bit;
            ++done;
            ++target_bit;
            if (source_bit == 0x20u) {
                source += 4u;
                source_bit = 0u;
            }
            if (target_bit == 0x20u) {
                target += 4u;
                target_bit = 0u;
            }
        }
        const std::uint32_t old = word(rt, bits);
        put(rt, bits + 0xCu, target_bit);
        put(rt, bits + 8u, target);
        if (old != 0u) call_set(rt, ctx, 0x08A5BC54u, {old, (word(rt, bits + 0x10u) - old) & ~3u, 4u});
        put(rt, bits + 0x10u, block + 4u);
        put(rt, bits + 4u, 0u);
        put(rt, bits, block);
    }
    const float first = float_at(rt, 0x08CDE764u);
    const float second = float_at(rt, 0x08CDE760u);
    const std::uint32_t body = call_virtual_at(rt, ctx, self, 0x110u, {self});
    put(rt, self + 0x180u, body);
    call_set(rt, ctx, 0x08A5BA28u, {body});
    const std::uint32_t settings = frame.at(0u);
    put_float(rt, settings + 8u, first);
    put_float(rt, settings + 0xCu, second);
    put(rt, settings, 0u);
    put(rt, settings + 4u, 0u);
    call_set(rt, ctx, 0x089A94B8u, {word(rt, self + 0x180u), settings});
    const std::uint32_t object = word(rt, self + 0x180u);
    call_virtual_at(rt, ctx, object, 0x18Cu, {object});
    put8(rt, self + 0x1B4u, 0u);
}

// func_08ABEAEC(self) -> an object from the first of 16 pools at self + 0x1C ({capacity, used, objects, free list}, 16
// bytes each) with room, under the lock at self + 0x11C: a pool's first use allocates capacity x 32-byte objects with a
// count word (func_08B6461C; func_08AD408C each) and a list of their addresses (func_08B6461C); the next list entry is
// taken. The object then gets func_08AD401C(object, self, pool index); 0 when every pool is full.
void func_08ABEAEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 0x11Cu;
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, kLock, {lock});
    std::uint32_t chosen = 0x10u;
    std::uint32_t object = 0u;
    for (std::uint32_t index = 0u; index < 0x10u; ++index) {
        const std::uint32_t pool = self + 0x1Cu + (index << 4);
        const std::uint32_t used = word(rt, pool + 4u);
        const std::uint32_t capacity = word(rt, pool);
        if (!(used < capacity)) continue;
        chosen = index;
        if (word(rt, pool + 0xCu) == 0u) {
            const std::uint32_t block = call_set(rt, ctx, 0x08B6461Cu, {(capacity << 5) + 4u});
            put(rt, block, capacity);
            const std::uint32_t objects = block + 4u;
            for (std::uint32_t slot = 0u; slot != capacity; ++slot) call_set(rt, ctx, 0x08AD408Cu, {objects + slot * 0x20u});
            put(rt, pool + 8u, objects);
            const std::uint32_t list = call_set(rt, ctx, 0x08B6461Cu, {word(rt, pool) << 2});
            put(rt, pool + 0xCu, list);
            put(rt, pool + 4u, 0u);
            if (list != 0u && word(rt, pool + 8u) != 0u) {
                for (std::uint32_t slot = 0u; slot < word(rt, pool); ++slot) {
                    put(rt, word(rt, pool + 0xCu) + slot * 4u, word(rt, pool + 8u) + (slot << 5));
                }
            }
        }
        const std::uint32_t taken = word(rt, pool + 4u);
        object = word(rt, (taken << 2) + word(rt, pool + 0xCu));
        put(rt, pool + 4u, taken + 1u);
        break;
    }
    call_set(rt, ctx, kUnlock, {lock});
    if (object != 0u) call_set(rt, ctx, 0x08AD401Cu, {object, self, chosen & 0xFFu});
    ctx.gpr[2] = object;
}

// func_08AFF62C(self, a1, f12 = dt): one integration step, nothing when the scale self->+0xD8->+0x60 is 0: the 64-bit
// values (vector handles) of func_08C45E40(self + 0x80; scale), func_08C45FB8(&that, a1) and func_08C45DC0(&that; dt) build
// a step through func_08C46718 / func_08BAE2FC into frame vectors that func_08C46508 adds to self + 0x38; then
// func_08C45E40(self + 0x8C; dt) through func_08C46718 is added to self + 0x44, and func_08C468E4(self + 0x50,
// func_08C45E2C(self + 0x5C, self + 0x44)); the accumulators self + 0x80..+0x88 and + 0x8C..+0x94 are cleared.
void func_08AFF62C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const float step = ctx.fpr[12];
    const std::uint32_t force = self + 0x80u;
    const std::uint32_t torque = self + 0x8Cu;
    const float scale = float_at(rt, word(rt, self + 0xD8u) + 0x60u);
    ctx.gpr[4] = force;
    ctx.fpr[12] = scale;
    if (scale == 0.0f) return;
    GuestFrame frame(ctx, 0xA0u);
    const auto keep_pair = [&](std::uint32_t slot, std::uint32_t low, std::uint32_t high) {
        put(rt, frame.at(slot), low);
        put(rt, frame.at(slot + 4u), high);
        put(rt, frame.at(0x50u), low);
        put(rt, frame.at(0x54u), high);
    };
    std::uint32_t low = call_setf(rt, ctx, 0x08C45E40u, {force}, {scale}).v0;
    keep_pair(0x10u, low, ctx.gpr[3]);
    low = call_set(rt, ctx, 0x08C45FB8u, {frame.at(0x10u), second});
    keep_pair(0x18u, low, ctx.gpr[3]);
    low = call_setf(rt, ctx, 0x08C45DC0u, {frame.at(0x18u)}, {step}).v0;
    const std::uint32_t handle = ctx.gpr[3];
    put(rt, frame.at(0x20u), low);
    put(rt, frame.at(0x50u), low);
    put(rt, frame.at(0x54u), handle);
    put(rt, frame.at(0x24u), handle);
    const std::uint32_t first = word(rt, handle);
    call_set(rt, ctx, 0x08C46718u, {frame.at(0x40u), first, word(rt, first + 4u)});
    call_set(rt, ctx, 0x08BAE2FCu, {frame.at(0x34u), frame.at(0x40u), word(rt, handle + 4u)});
    call_set(rt, ctx, 0x08C46718u, {frame.at(0x28u), frame.at(0x20u), frame.at(0x34u)});
    call_set(rt, ctx, 0x08C46508u, {self + 0x38u, frame.at(0x28u)});
    low = call_setf(rt, ctx, 0x08C45E40u, {torque}, {step}).v0;
    const std::uint32_t high = ctx.gpr[3];
    put(rt, frame.at(8u), low);
    put(rt, frame.at(0x50u), low);
    put(rt, frame.at(0x54u), high);
    put(rt, frame.at(0xCu), high);
    call_set(rt, ctx, 0x08C46718u, {frame.at(0x40u), frame.at(8u), high});
    call_set(rt, ctx, 0x08C46508u, {self + 0x44u, frame.at(0x40u)});
    low = call_set(rt, ctx, 0x08C45E2Cu, {self + 0x5Cu, self + 0x44u});
    keep_pair(0u, low, ctx.gpr[3]);
    call_set(rt, ctx, 0x08C468E4u, {self + 0x50u, low, ctx.gpr[3]});
    for (const std::uint32_t offset : {0x80u, 0x88u, 0x84u, 0x8Cu, 0x94u, 0x90u}) put_float(rt, self + offset, 0.0f);
}

// func_088F0064(self, stream): reads a record from the stream cursor (kept in the frame): the bytes self->+0xC8..+0xCA
// (func_08BB8B40, chained) with +0xB7 = the second, +0xCB (func_08BB8B98) and a count n (func_08BB8B6C); then n elements
// of 0x54 bytes into self->+0xCC's first array (func_088F1278; past its size into a frame scratch from func_088F1310),
// n of 8 bytes into its second (func_088F0FAC; scratch from func_088F0EF4), and n bytes into self->+0xD4 (func_08BB8B98,
// each kept one followed by func_088EFC00(self, i)).
void func_088F0064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const std::uint32_t cursor = frame.at(4u);
    const std::uint32_t count = frame.at(8u);
    const std::uint32_t scratch = frame.at(0xCu);
    put(rt, cursor, ctx.gpr[5]);
    call_set(rt, ctx, 0x08BB8B40u, {cursor, self + 0xC8u});
    const std::uint32_t chained = call_set(rt, ctx, 0x08BB8B40u, {cursor, self + 0xC9u});
    call_set(rt, ctx, 0x08BB8B40u, {chained, self + 0xCAu});
    put8(rt, self + 0xB7u, byte_u(rt, self + 0xC9u));
    call_set(rt, ctx, 0x08BB8B98u, {cursor, self + 0xCBu});
    call_set(rt, ctx, 0x08BB8B6Cu, {cursor, count});
    if (word(rt, count) == 0u) return;
    for (std::uint32_t index = 0u; index < word(rt, count); ++index) {
        const std::uint32_t arrays = word(rt, self + 0xCCu);
        if (index < word(rt, arrays + 4u)) {
            call_set(rt, ctx, 0x088F1278u, {index * 0x54u + word(rt, arrays), cursor});
        } else {
            call_set(rt, ctx, 0x088F1310u, {scratch});
            call_set(rt, ctx, 0x088F1278u, {scratch, cursor});
        }
    }
    if (word(rt, count) == 0u) return;
    for (std::uint32_t index = 0u; index < word(rt, count); ++index) {
        const std::uint32_t arrays = word(rt, self + 0xCCu);
        if (index < word(rt, arrays + 0xCu)) {
            call_set(rt, ctx, 0x088F0FACu, {word(rt, arrays + 8u) + (index << 3), cursor});
        } else {
            call_set(rt, ctx, 0x088F0EF4u, {scratch});
            call_set(rt, ctx, 0x088F0FACu, {scratch, cursor});
        }
    }
    if (word(rt, count) == 0u) return;
    for (std::uint32_t index = 0u; index < word(rt, count); ++index) {
        if (index < word(rt, self + 0xD4u + 4u)) {
            call_set(rt, ctx, 0x08BB8B98u, {cursor, word(rt, self + 0xD4u) + index});
            call_set(rt, ctx, 0x088EFC00u, {self, index});
        } else {
            call_set(rt, ctx, 0x08BB8B98u, {cursor, frame.at(0u)});
        }
    }
}

// func_08849B8C(target, a1, count, holder): a script property of the object func_08846E50(w), w the word of the value object
// func_08847008 fills (func_08A5BB7C(0x08C9CBBC) when 0; destroyed after). With count > 0 and o = holder->+0
// (func_08A5BB7C(0x08C9CB80) for a null object): when func_08A76F20(o, func_08A6BF34()) says o is of that kind,
// func_088D5FA4(object, the string o's virtual slot 0xC fills; released after); otherwise func_088D5ED0(object, .., o's
// slot 0x50 as 64 bits, 0). With count <= 0, func_08A5DEE8(temp, .., func_088D5E80(object) as 64 bits) builds a value
// object that goes to target.
void func_08849B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[7];
    const bool set = signed_of(ctx.gpr[6]) > 0;
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t source = frame.at(set ? 0xCu : 8u);
    call_set(rt, ctx, 0x08847008u, {source});
    const std::uint32_t value = word(rt, source);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
    const std::uint32_t object = call_set(rt, ctx, 0x08846E50u, {value});
    call_set(rt, ctx, kDestroy, {source});
    if (!set) {
        const std::uint32_t low = call_set(rt, ctx, 0x088D5E80u, {object});
        const std::uint32_t temporary = frame.at(4u);
        call_set(rt, ctx, 0x08A5DEE8u, {temporary, ctx.gpr[5], low, ctx.gpr[3]});
        keep_temporary(rt, ctx, target, temporary);
        return;
    }
    std::uint32_t argument = word(rt, holder);
    if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
    const std::uint32_t kind = call_set(rt, ctx, 0x08A6BF34u, {});
    const bool text = call_set(rt, ctx, 0x08A76F20u, {argument, kind}) != 0u;
    argument = word(rt, holder);
    if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
    if (text) {
        const std::uint32_t string = frame.at(0x10u);
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
        call_set(rt, ctx, ctx.gpr[2], {string, argument});
        call_set(rt, ctx, 0x088D5FA4u, {object, word(rt, string)});
        release_string(rt, ctx, string, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        return;
    }
    const std::uint32_t low = call_virtual_at(rt, ctx, argument, 0x50u, {argument});
    call_set(rt, ctx, 0x088D5ED0u, {object, ctx.gpr[5], low, ctx.gpr[3], 0u});
}

// func_08B8A03C(vector, position, value): the libstdc++ vector::_M_insert_aux for 4-byte value objects (copied by
// func_08A76ABC, assigned by func_08A5BAD8, destroyed by func_08A5B9EC): with room, the last element is copy-constructed one
// past the end, the others move up by assignment (from the back) and *position = a frame copy of value; full, a block of
// twice the size (one element at least; func_08B5ADDC(0x08C98DA4) at the maximum) from func_08A5BC20 gets the halves
// around a copy of value (func_08B859C4), the old range is destroyed (func_08B85754) and freed (func_08A5BC54).
void func_08B8A03C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) {
            call_set(rt, ctx, 0x08A76ABCu, {end, end - 4u});
            end = word(rt, vector + 4u);
        }
        put(rt, vector + 4u, end + 4u);
        const std::uint32_t copy = frame.at(4u);
        call_set(rt, ctx, 0x08A76ABCu, {copy, value});
        const std::uint32_t top = word(rt, vector + 4u);
        std::uint32_t from = top - 8u;
        const std::int32_t count = signed_of(from - position) >> 2;
        std::uint32_t to = top - 4u;
        for (std::int32_t index = 0; index < count; ++index) {
            to -= 4u;
            from -= 4u;
            if (to != from) call_set(rt, ctx, kAssign, {to, word(rt, from)});
        }
        if (position != copy) call_set(rt, ctx, kAssign, {position, word(rt, copy)});
        call_set(rt, ctx, kDestroy, {copy});
        return;
    }
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - word(rt, vector)) >> 2);
    if (count == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08C98DA4u});
    std::uint32_t bytes;
    if (count == 0u) bytes = 4u;
    else bytes = count * 2u < count ? 0xFFFFFFFCu : (count * 2u) << 2;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t middle = call_set(rt, ctx, 0x08B859C4u, {word(rt, vector), position, block, frame.at(2u)});
    if (middle != 0u) call_set(rt, ctx, 0x08A76ABCu, {middle, value});
    const std::uint32_t finish = call_set(rt, ctx, 0x08B859C4u, {position, word(rt, vector + 4u), middle + 4u, frame.at(1u)});
    call_set(rt, ctx, 0x08B85754u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t begin = word(rt, vector);
    if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - begin) >> 2) << 2, 4u});
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);    ctx.gpr[2] = block + bytes;
}

// func_08945F6C(self): reads the four parts func_08955300(self, i) (float +0xBC each, kept at sp+0..0xC); the body
// self+8 copies +0x10/+0x14 to +0x40/+0x44. Unless func_0894CCB8() and func_089557BC(self) are both non-zero, with
// a..d the four floats and s = limits(self+0xC)+0x20: x = a + (c - a)s, y = b + s(d - b), w = |+0x18 x func_08B499B8(body+0x6FC)|,
// h = |+0x1C x func_08B499B8(body+0x700)|; body+0x708 = func_08B4A430(((a+b) - (c+d)) x 0.5, w), +0x70C = func_08B4A430(x - y, h),
// +0x48 = (x + y) x 0.5. Otherwise body+0x48 = float +0xC of func_0893C780(self, 0). Ends in func_0893C3E4 over the body's fields.
void func_08945F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t body = word(rt, self + 8u);
    const std::uint32_t limits = word(rt, self + 0xCu);
    float part[4];
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t entry = call_set(rt, ctx, 0x08955300u, {self, i});
        part[i] = float_at(rt, entry + 0xBCu);
        put_float(rt, frame.at(i * 4u), part[i]);
    }
    put_float(rt, body + 0x40u, float_at(rt, body + 0x10u));
    put_float(rt, body + 0x44u, float_at(rt, body + 0x14u));
    if (call_set(rt, ctx, 0x0894CCB8u, {}) != 0u && call_set(rt, ctx, 0x089557BCu, {self}) != 0u) {
        const std::uint32_t point = call_set(rt, ctx, 0x0893C780u, {self, 0u});
        put_float(rt, body + 0x48u, float_at(rt, point + 0xCu));
    } else {
        const float scale = float_at(rt, limits + 0x20u);
        const float across = (part[2] - part[0]) * scale;
        const float down = scale * (part[3] - part[1]);
        const float first_sum = part[0] + part[1];
        const float second_sum = part[2] + part[3];
        const float x = part[0] + across;
        const float y = part[1] + down;
        const float width_scale = float_at(rt, limits + 0x18u);
        const float height_scale = float_at(rt, limits + 0x1Cu);
        float width = width_scale * call_setf(rt, ctx, 0x08B499B8u, {}, {float_at(rt, body + 0x6FCu)}).f0;
        float height = height_scale * call_setf(rt, ctx, 0x08B499B8u, {}, {float_at(rt, body + 0x700u)}).f0;
        if (width < 0.0f) width = -width;
        if (height < 0.0f) height = -height;
        put_float(rt, body + 0x708u, call_setf(rt, ctx, 0x08B4A430u, {}, {(first_sum - second_sum) * 0.5f, width}).f0);
        put_float(rt, body + 0x70Cu, call_setf(rt, ctx, 0x08B4A430u, {}, {x - y, height}).f0);
        put_float(rt, body + 0x48u, (x + y) * 0.5f);
    }
    tail_call(ctx, 0x0893C3E4u, {body + 0x4Cu, body + 0x58u, body + 0x64u, body + 0x708u, body + 0x70Cu, body + 0x704u});
}

// func_08A7EB58(_, first, mode, pair): mode 2 builds values from pair and first (func_08A7C3A4), takes the value
// (pair+4)->slot 0x5C and calls func_08A7E99C(first value's object, pair value, it, 0x08CF6FDC); mode 3 takes the
// slot-0x5C values of pair+4 and pair+8 and passes both. Null objects go to func_08A5BB7C with the message
// 0x08CB1360 / 0x08CB1350 first. Any other mode returns 3.
void func_08A7EB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t pair = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const auto object_at = [&](std::uint32_t address) {
        const std::uint32_t object = word(rt, address);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1350u});
        return object;
    };
    if (mode == 2u) {
        const std::uint32_t paired = frame.at(0x14u);
        const std::uint32_t held = frame.at(0x10u);
        const std::uint32_t value = frame.at(0xCu);
        call_set(rt, ctx, 0x08A7C3A4u, {paired, pair});
        call_set(rt, ctx, 0x08A7C3A4u, {held, first});
        const std::uint32_t target = word(rt, held);
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
        const std::uint32_t source = object_at(pair + 4u);
        call_virtual_at(rt, ctx, source, 0x5Cu, {value, source});
        call_set(rt, ctx, 0x08A7E99Cu, {target, paired, value, 0x08CF6FDCu});
        call_set(rt, ctx, 0x08A69D68u, {value});
        call_set(rt, ctx, kDestroy, {held});
        call_set(rt, ctx, kDestroy, {paired});
        return;
    }
    if (mode != 3u) {
        ctx.gpr[2] = 3u;
        return;
    }
    const std::uint32_t paired = frame.at(0x14u);
    const std::uint32_t held = frame.at(8u);
    const std::uint32_t value = frame.at(4u);
    const std::uint32_t other = frame.at(0u);
    call_set(rt, ctx, 0x08A7C3A4u, {paired, pair});
    call_set(rt, ctx, 0x08A7C3A4u, {held, first});
    const std::uint32_t target = word(rt, held);
    if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
    const std::uint32_t source = object_at(pair + 4u);
    call_virtual_at(rt, ctx, source, 0x5Cu, {value, source});
    const std::uint32_t second = object_at(pair + 8u);
    call_virtual_at(rt, ctx, second, 0x5Cu, {other, second});
    call_set(rt, ctx, 0x08A7E99Cu, {target, paired, value, other});
    call_set(rt, ctx, 0x08A69D68u, {other});
    call_set(rt, ctx, 0x08A69D68u, {value});
    call_set(rt, ctx, kDestroy, {held});
    call_set(rt, ctx, kDestroy, {paired});
}

// func_08AB7754(): maps the system parameter 8 (func_08B2ED74(8, &value)) to a code: 0 -> 0, 1 -> 1 when
// func_08B35FD8(func_08ABD974(), 0x08CB42D0) is non-zero else 2, 2..11 -> value + 1, anything else 0 (jump table
// 0x08C904B4). If func_08ABD5EC(list, 14) lists codes and the code is not among them, the first listed one replaces it.
// Stores it with func_08B49858(0x08D69524, code), reads parameter 9 and returns func_08C7AC60(index, parameter 9),
// index = 0 / 1 / code - 1 for code 0 / 1 / 2..12 and 0 above (jump table 0x08C904E4).
void func_08AB7754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    call_set(rt, ctx, 0x08B2ED74u, {8u, frame.at(0u)});
    const std::uint32_t current = call_set(rt, ctx, 0x08ABD974u, {});
    const std::uint32_t differs = call_set(rt, ctx, 0x08B35FD8u, {current, 0x08CB42D0u});
    const std::uint32_t value = word(rt, frame.at(0u));
    std::uint32_t code = 0u;
    if (value == 1u) {
        code = differs != 0u ? 1u : 2u;
    } else if (value >= 2u && value < 12u) {
        code = value + 1u;
    }
    const std::uint32_t list = frame.at(8u);
    const std::uint32_t count = call_set(rt, ctx, 0x08ABD5ECu, {list, 0xEu});
    if (signed_of(count) > 0) {
        bool listed = false;
        for (std::uint32_t i = 0u; i < count && !listed; ++i) listed = word(rt, list + i * 4u) == code;
        if (!listed) code = word(rt, list);
    }
    call_set(rt, ctx, 0x08B49858u, {0x08D69524u, code});
    call_set(rt, ctx, 0x08B2ED74u, {9u, frame.at(4u)});
    std::uint32_t index = 0u;
    if (code == 1u) {
        index = 1u;
    } else if (code >= 2u && code < 13u) {
        index = code - 1u;
    }
    call_set(rt, ctx, 0x08C7AC60u, {index, word(rt, frame.at(4u))});
}

// func_088FB728(box, near, far, point, direction): slab test of a ray against a box (min at box+0, max at box+0xC) axis
// by axis. On the first axis where the point is outside the slab: moving away returns 0, otherwise func_088FB274
// fills near / far for that axis with the entry and exit planes. If the point is inside every slab, the first axis with a
// non-zero direction copies the point to near and fills far with the plane the ray heads to. Then returns
// func_088FB3D8(box, near, far, axis + 1); a ray with no direction returns 0.
void func_088FB728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t box = ctx.gpr[4];
    const std::uint32_t near = ctx.gpr[5];
    const std::uint32_t far = ctx.gpr[6];
    const std::uint32_t point = ctx.gpr[7];
    const std::uint32_t direction = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const auto plane = [&](std::uint32_t target, std::uint32_t axis, float at) {
        call_setf(rt, ctx, 0x088FB274u, {target, point, direction, axis}, {at});
    };
    for (std::uint32_t axis = 0u; axis < 3u; ++axis) {
        const float at = float_at(rt, point + axis * 4u);
        const float low = float_at(rt, box + axis * 4u);
        const float high = float_at(rt, box + 0xCu + axis * 4u);
        const float heading = float_at(rt, direction + axis * 4u);
        if (at < low) {
            if (heading <= 0.0f) break;
            plane(near, axis, low);
            plane(far, axis, float_at(rt, box + 0xCu + axis * 4u));
        } else if (high < at) {
            if (0.0f <= heading) break;
            plane(near, axis, high);
            plane(far, axis, float_at(rt, box + axis * 4u));
        } else {
            if (heading == 0.0f) continue;
            call_set(rt, ctx, 0x08B79BACu, {near, point, 0xCu});
            plane(far, axis, float_at(rt, direction + axis * 4u) < 0.0f ? float_at(rt, box + axis * 4u) : float_at(rt, box + 0xCu + axis * 4u));
        }
        call_set(rt, ctx, 0x088FB3D8u, {box, near, far, axis + 1u});
        return;
    }
    ctx.gpr[2] = 0u;
}

// func_088BDA50(self, a, b, c, time): wraps time into [0, period] with period = self->slot 0x14 (non-positive period
// returns 0). With the keys of func_088C50B8(keys, self, 6) the first key whose +0x14 passes time and the one before it
// (func_088C52A0 / func_088C5324, wrapping around the count) are each measured with func_088BD8F8(key, a, c, b, float
// (c)+0x1360); returns the smaller.
void func_088BDA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    const std::uint32_t holder = ctx.gpr[7];
    float time = ctx.fpr[12];
    GuestFrame frame(ctx, 0x40u);
    call_virtual_at(rt, ctx, self, 0x14u, {self});
    const float period = ctx.fpr[0];
    if (period <= 0.0f) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    if (period < time) {
        do time -= period;
        while (period < time);
    } else if (time < 0.0f) {
        do time += period;
        while (time < 0.0f);
    }
    const std::uint32_t keys = frame.at(0u);
    call_set(rt, ctx, 0x088C50B8u, {keys, self, 6u});
    const std::int32_t count = signed_of(word(rt, keys));
    std::int32_t found = count;
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t key = call_set(rt, ctx, 0x088C50C4u, {keys, static_cast<std::uint32_t>(i)});
        if (time < float_at(rt, key + 0x14u)) {
            found = i;
            break;
        }
    }
    std::int32_t before = found + count - 1;
    while (before >= count) before -= count;
    const std::uint32_t low = call_set(rt, ctx, 0x088C52A0u, {keys, static_cast<std::uint32_t>(before)});
    std::uint32_t next = low + 1u;
    if (!(signed_of(next) < count)) next -= static_cast<std::uint32_t>(count);
    const std::uint32_t high = call_set(rt, ctx, 0x088C5324u, {keys, next});
    const float scale = float_at(rt, word(rt, holder) + 0x1360u);
    const std::uint32_t low_key = call_set(rt, ctx, 0x088C50C4u, {keys, low});
    const float near = call_setf(rt, ctx, 0x088BD8F8u, {low_key, first, holder, second}, {scale}).f0;
    const std::uint32_t high_key = call_set(rt, ctx, 0x088C50C4u, {keys, high});
    const float far = call_setf(rt, ctx, 0x088BD8F8u, {high_key, first, holder, second}, {scale}).f0;
    ctx.fpr[0] = near < far ? near : far;
}

// func_088F5F08(self): one draw pass: builds a 0x150-byte state on the stack (func_088F6FC8), sets the render
// state (func_08B54FB8 / 08B5877C / 08B58A74 / 08B58AD0 / 08B58874 / 08B584D0 / 08B55AAC ... calls with the
// original's constants), runs func_088F7490(node, state) over the list self+4 (next at +0x108), and when self+0x18 is set
// and self+0x1C >= 0 calls func_08AF6CE0(entries + index x 0x98, 0, entries, self+0x18) with entries = (self+0x18)+0x18
// and func_088F74DC over the list; then restores the state.
void func_088F5F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x160u);
    const std::uint32_t state = frame.at(0u);
    call_set(rt, ctx, 0x088F6FC8u, {state});
    call_set(rt, ctx, 0x08B5711Cu, {0x481u});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Du});
    call_set(rt, ctx, 0x08B54FB8u, {0x64u});
    call_set(rt, ctx, 0x08B54FB8u, {0x78u});
    call_setf(rt, ctx, 0x08B5877Cu, {6u}, {0.05f});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Cu});
    call_set(rt, ctx, 0x08B58A74u, {0u});
    call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
    call_set(rt, ctx, 0x08B54FB8u, {0x6Au});
    call_set(rt, ctx, 0x08B58874u, {0x8002u});
    call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {0u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {2u, 0x8000u});
    call_set(rt, ctx, 0x08B55AACu, {1u, 0u});
    call_set(rt, ctx, 0x08B55AACu, {5u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {6u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {4u, 1u});
    call_set(rt, ctx, 0x08B55AACu, {3u, 1u});
    call_set(rt, ctx, 0x08B55B90u, {});
    call_set(rt, ctx, 0x08B56A00u, {0u});
    call_set(rt, ctx, 0x08B569A0u, {});
    call_set(rt, ctx, 0x08B56510u, {});
    call_set(rt, ctx, 0x08B54FB8u, {2u});
    for (std::uint32_t node = word(rt, self + 4u); node != 0u; node = word(rt, node + 0x108u)) call_set(rt, ctx, 0x088F7490u, {node, state});
    if (word(rt, self + 0x18u) != 0u && signed_of(word(rt, self + 0x1Cu)) >= 0) {
        call_set(rt, ctx, 0x08B58874u, {2u});
        const std::uint32_t index = word(rt, self + 0x1Cu);
        const std::uint32_t owner = word(rt, self + 0x18u);
        const std::uint32_t entries = word(rt, owner + 0x18u);
        call_set(rt, ctx, 0x08AF6CE0u, {entries + index * 0x98u, 0u, entries, owner});
        for (std::uint32_t node = word(rt, self + 4u); node != 0u; node = word(rt, node + 0x108u)) call_set(rt, ctx, 0x088F74DCu, {node, state});
    }
    call_set(rt, ctx, 0x08B56940u, {});
    call_set(rt, ctx, 0x08B54FB8u, {0x8002u});
    call_set(rt, ctx, 0x08B58A74u, {1u});
    call_set(rt, ctx, 0x08B58874u, {0x6Du});
}

// func_089DEA00(self): after func_0896D6A4(self), clears bits 10..12 of self+0x140 and self+0x1B8 / +0x1BC, then for two
// slots k empties three buffers {begin, end, capacity}: self+0xEC+12k and self+0x104+12k through func_08BF0BF0 and
// self+0x11C+12k through func_08C06684 (moved to the stack, zeroed in self, destroyed, freed with func_08B35254). Then
// self+0x138 and self+0x13C each take a fresh value from func_089E4C14(&value, 0x08CF6ED8).
void func_089DEA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    call_set(rt, ctx, 0x0896D6A4u, {self});
    const std::uint32_t bits = word(rt, self + 0x140u);
    put(rt, self + 0x1B8u, 0u);
    put(rt, self + 0x140u, bits & ~0x1C00u);
    put(rt, self + 0x1BCu, 0u);
    const auto empty = [&](std::uint32_t buffer, std::uint32_t moved, std::uint32_t destroy, std::uint32_t scratch) {
        const std::uint32_t begin = word(rt, buffer);
        const std::uint32_t end = word(rt, buffer + 4u);
        const std::uint32_t capacity = word(rt, buffer + 8u);
        put(rt, moved, begin);
        put(rt, moved + 4u, end);
        put(rt, buffer, 0u);
        put(rt, buffer + 4u, 0u);
        put(rt, buffer + 8u, 0u);
        put(rt, moved + 8u, capacity);
        call_set(rt, ctx, destroy, {word(rt, moved), word(rt, moved + 4u), scratch, capacity});
        const std::uint32_t storage = word(rt, moved);
        if (storage != 0u) call_set(rt, ctx, 0x08B35254u, {storage});
    };
    for (std::uint32_t k = 0u; k < 2u; ++k) {
        empty(self + 0xECu + k * 0xCu, frame.at(0x24u), 0x08BF0BF0u, frame.at(2u));
        empty(self + 0x104u + k * 0xCu, frame.at(0x18u), 0x08BF0BF0u, frame.at(1u));
        empty(self + 0x11Cu + k * 0xCu, frame.at(0xCu), 0x08C06684u, frame.at(0u));
    }
    for (const std::uint32_t offset : {0x138u, 0x13Cu}) {
        const std::uint32_t fresh = frame.at(offset == 0x138u ? 8u : 4u);
        call_set(rt, ctx, 0x089E4C14u, {fresh, 0x08CF6ED8u});
        call_set(rt, ctx, kAssign, {self + offset, word(rt, fresh)});
        call_set(rt, ctx, kDestroy, {fresh});
    }
}

// func_08A9D93C(self, key, target): looks key up (func_08A9D8AC(&found, self, name) over a fresh name); a hit is handed
// to func_08A7E99C(target object, &found, 0x08CF6FA0, 0x08CF6FDC) and returns 1. Otherwise creates a 0x38-byte entry
// (func_08A5BC20 / func_08A7E914), and if func_08A9D890(self, key, &found) accepts it, registers it (func_08A9BF4C),
// collects its members into a stack tree (func_08A7D1F8) and passes each node's +0x10 / +0x14 to target->slot 0x110;
// returns 1 then, 0 when not accepted. Null objects go to func_08A5BB7C(0x08CB2570) first.
void func_08A9D93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t target = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t name = frame.at(8u);
    const std::uint32_t found = frame.at(0xCu);
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
        return object;
    };
    call_set(rt, ctx, 0x08A69654u, {name});
    call_set(rt, ctx, 0x08A9D8ACu, {found, self, name});
    std::uint32_t result = 1u;
    if (word(rt, found) != 0u) {
        const std::uint32_t object = checked(word(rt, target));
        call_set(rt, ctx, 0x08A7E99Cu, {object, found, 0x08CF6FA0u, 0x08CF6FDCu});
    } else {
        const std::uint32_t entry = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
        call_set(rt, ctx, 0x08A7E914u, {entry, name});
        put(rt, frame.at(0u), entry);
        call_set(rt, ctx, 0x08A76AB4u, {frame.at(4u), frame.at(0u)});
        call_set(rt, ctx, kAssign, {found, word(rt, frame.at(4u))});
        call_set(rt, ctx, kDestroy, {frame.at(4u)});
        result = 0u;
        if (call_set(rt, ctx, 0x08A9D890u, {self, key, found}) != 0u) {
            call_set(rt, ctx, 0x08A9BF4Cu, {self, found});
            const std::uint32_t owner = word(rt, found);
            const std::uint32_t tree = frame.at(0x10u);
            const std::uint32_t header = frame.at(0x14u);
            put(rt, frame.at(0x24u), 0u);
            put(rt, header, 0u);
            put(rt, frame.at(0x18u), 0u);
            put(rt, frame.at(0x1Cu), header);
            put(rt, frame.at(0x20u), header);
            call_set(rt, ctx, 0x08A7D1F8u, {checked(owner), tree});
            for (std::uint32_t node = word(rt, frame.at(0x1Cu)); node != header; node = call_set(rt, ctx, 0x08B5F100u, {node})) {
                const std::uint32_t object = checked(word(rt, target));
                call_virtual_at(rt, ctx, object, 0x110u, {object, node + 0x10u, node + 0x14u});
            }
            call_set(rt, ctx, 0x08C00860u, {tree, word(rt, frame.at(0x18u))});
            result = 1u;
        }
    }
    call_set(rt, ctx, kDestroy, {found});
    call_set(rt, ctx, 0x08A69D68u, {name});
    ctx.gpr[2] = result;
}

// func_08A8220C(self, key): map lookup with refresh. The key value made by func_08A818A0 is looked up in the map at
// self+0x14 (func_08C287AC). A hit is checked first: (self+0x14)'s object +0x2C gets slot 0xCC(fresh, 2, {key, entry
// value}) and the fresh value's slot 0x30 decides; zero keeps the hit. A miss, or a non-zero answer, inserts {key, empty}
// at the found place (func_08C28D70). Returns the node's value address (+0x14). Null objects go to func_08A5BB7C(0x08CB15A0).
void func_08A8220C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t map = self + 0x14u;
    const std::uint32_t made = frame.at(4u);
    put(rt, frame.at(0x20u), map);
    call_set(rt, ctx, 0x08A818A0u, {made, self, key});
    const std::uint32_t place = call_set(rt, ctx, 0x08C287ACu, {map, made});
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
        return object;
    };
    std::uint32_t node = place;
    bool insert = place == self + 0x18u;
    if (!insert) {
        const std::uint32_t owner = word(rt, self + 0x14u);
        const std::uint32_t fresh = frame.at(8u);
        const std::uint32_t pair = frame.at(0x14u);
        call_set(rt, ctx, 0x08A76AA0u, {fresh});
        call_set(rt, ctx, 0x08A76AA0u, {pair});
        call_set(rt, ctx, 0x08A76AA0u, {pair + 4u});
        call_set(rt, ctx, kAssign, {pair, word(rt, made)});
        call_set(rt, ctx, kAssign, {pair + 4u, word(rt, place + 0x10u)});
        const std::uint32_t checker = checked(word(rt, owner + 0x2Cu));
        call_virtual_at(rt, ctx, checker, 0xCCu, {checker, fresh, 2u, pair});
        const std::uint32_t answer_object = checked(word(rt, fresh));
        const std::uint32_t answer = call_virtual_at(rt, ctx, answer_object, 0x30u, {answer_object});
        call_set(rt, ctx, kDestroy, {pair + 4u});
        call_set(rt, ctx, kDestroy, {pair});
        call_set(rt, ctx, kDestroy, {fresh});
        insert = answer != 0u;
    }
    if (insert) {
        const std::uint32_t empty = frame.at(0u);
        const std::uint32_t entry = frame.at(0xCu);
        call_set(rt, ctx, 0x08A76AA0u, {empty});
        call_set(rt, ctx, 0x08A76ABCu, {entry, made});
        call_set(rt, ctx, 0x08A76ABCu, {entry + 4u, empty});
        node = call_set(rt, ctx, 0x08C28D70u, {word(rt, frame.at(0x20u)), place, entry});
        call_set(rt, ctx, kDestroy, {entry + 4u});
        call_set(rt, ctx, kDestroy, {entry});
        call_set(rt, ctx, kDestroy, {empty});
    }
    call_set(rt, ctx, kDestroy, {made});
    ctx.gpr[2] = node + 0x14u;
}

// func_08B05DC4(self, count, other): clears the four vectors (func_08C4B740 / 08C4B7A0 / 08C4B800 / 08C4B860), stores
// other at self+0x30 and resizes self+0 (0x70-byte elements) and self+0x18 (4-byte, zero fill) to count + 6, self+0xC
// (0x18-byte) and self+0x24 (2-byte, zero fill) to other + 2: shrinking erases the tail, growing inserts copies of a
// stack value (the two larger kinds copy whatever the stack slot holds, as the original).
void func_08B05DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t other = ctx.gpr[6];
    GuestFrame frame(ctx, 0xB0u);
    call_set(rt, ctx, 0x08C4B740u, {self});
    call_set(rt, ctx, 0x08C4B7A0u, {self + 0xCu});
    call_set(rt, ctx, 0x08C4B800u, {self + 0x18u});
    call_set(rt, ctx, 0x08C4B860u, {self + 0x24u});
    const auto resize = [&](std::uint32_t vector, std::uint32_t wanted, std::uint32_t size, std::uint32_t element,
                            std::uint32_t erase, std::uint32_t insert, std::uint32_t value) {
        const std::uint32_t begin = word(rt, vector);
        const std::uint32_t end = word(rt, vector + 4u);
        if (wanted < size) {
            call_set(rt, ctx, erase, {vector, begin + wanted * element, end});
        } else {
            call_set(rt, ctx, insert, {vector, end, wanted - size, value});
        }
    };
    const std::uint32_t big = count + 6u;
    const std::uint32_t small = other + 2u;
    const auto span = [&](std::uint32_t vector) { return signed_of(word(rt, vector + 4u) - word(rt, vector)); };
    put(rt, self + 0x30u, other);
    resize(self, big, static_cast<std::uint32_t>(span(self) >> 4) * 0xB6DB6DB7u, 0x70u, 0x08C4B284u, 0x08C4C0A0u, frame.at(0x20u));
    resize(self + 0xCu, small, static_cast<std::uint32_t>(span(self + 0xCu) >> 3) * 0xAAAAAAABu, 0x18u, 0x08C4B43Cu, 0x08C4BD80u, frame.at(8u));
    put16(rt, frame.at(2u), 0u);
    put16(rt, frame.at(4u), 0u);
    resize(self + 0x18u, big, static_cast<std::uint32_t>(span(self + 0x18u) >> 2), 4u, 0x08C4B174u, 0x08C4BB10u, frame.at(2u));
    put16(rt, frame.at(0u), 0u);
    resize(self + 0x24u, small, static_cast<std::uint32_t>(span(self + 0x24u) >> 1), 2u, 0x08C49930u, 0x08C4B8C0u, frame.at(0u));
}

// func_08BD4724(self, target): applies the entry self+0xC (negative = every entry) of self+4's table to target. Mode
// self+0x10 other than 1 hands it to func_08AFA0F0 / func_08AFA170 with the code func_08BD2DC4(target, (s8)self+0x14).
// Mode 1 works on target's block func_08AF296C(target): for each selected 0x30-byte entry of the table that
// func_089063A8(table, code, index) accepts, the weight func_08906304(func_08BD2E5C(..., target, 0), entry+4) goes to
// func_08AF10C4(block, func_08BD2EB4(self, &index) or self); then func_08AF1124(block) and
// func_08AF1068(block, target, !func_08AF29C4(target)).
void func_08BD4724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t code = call_set(rt, ctx, 0x08BD2DC4u, {target, sext8(byte_u(rt, self + 0x14u))});
    if (word(rt, self + 0x10u) != 1u) {
        const std::uint32_t index = word(rt, self + 0xCu);
        const std::uint32_t holder = word(rt, self + 4u);
        if (signed_of(index) < 0) {
            call_set(rt, ctx, 0x08AFA0F0u, {word(rt, holder), holder, code});
        } else {
            call_set(rt, ctx, 0x08AFA170u, {word(rt, holder), index, holder, code});
        }
        return;
    }
    const std::uint32_t block = call_set(rt, ctx, 0x08AF296Cu, {target});
    call_set(rt, ctx, 0x08BD3210u, {block, word(rt, block), word(rt, block + 4u)});
    put(rt, frame.at(0x20u), call_set(rt, ctx, 0x08BD2E5Cu, {frame.at(4u), target, 0u}));
    const std::uint32_t table = word(rt, word(rt, self + 4u));
    const std::uint32_t only = word(rt, self + 0xCu);
    if (signed_of(only) < 0) {
        std::uint32_t entry = word(rt, table + 0x30u);
        const std::uint32_t end = entry + half_u(rt, table + 0x10u) * 0x30u;
        for (std::uint32_t index = 0u; entry != end; entry += 0x30u, ++index) {
            if (call_set(rt, ctx, 0x089063A8u, {table, code, index}) == 0u) continue;
            put(rt, frame.at(0u), index);
            const std::uint32_t slot = call_set(rt, ctx, 0x08BD2EB4u, {self, frame.at(0u)});
            call_set(rt, ctx, 0x08906304u, {word(rt, frame.at(0x20u)), entry + 4u});
            const float weight = ctx.fpr[0];
            call_setf(rt, ctx, 0x08AF10C4u, {block, slot}, {weight});
        }
    } else if (call_set(rt, ctx, 0x089063A8u, {table, code, only}) != 0u) {
        call_set(rt, ctx, 0x08906304u, {word(rt, frame.at(0x20u)), word(rt, table + 0x30u) + only * 0x30u + 4u});
        call_setf(rt, ctx, 0x08AF10C4u, {block, self}, {ctx.fpr[0]});
    }
    call_set(rt, ctx, 0x08AF1124u, {block});
    const std::uint32_t still = call_set(rt, ctx, 0x08AF29C4u, {target});
    call_set(rt, ctx, 0x08AF1068u, {block, target, (still ^ 1u) & 0xFFu});
}

// The original's unsigned-to-float conversion: halves a value with the top bit set (keeping the low bit) and doubles
// the converted result.
inline float unsigned_to_float(std::uint32_t value) {
    if (signed_of(value) >= 0) return static_cast<float>(signed_of(value));
    const float half = static_cast<float>(signed_of((value >> 1) | (value & 1u)));
    return half + half;
}

// func_0880DE48(self, source): reads a 0x38-byte record with func_08B10170(record, source, info, 0x38, 1) and copies its
// 0x3C-byte status to the stack; a non-zero status is returned. Otherwise resets self+0x98 twice (func_0896A35C) and
// builds the 2D transform at self+0x248 from the record: angle (+0x24) in degrees, scales (+0x18, +0x20), offset
// (+0x1C) and the unsigned size (+0x10, +0x14) halved; returns self+0x248.
void func_0880DE48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x130u);
    const std::uint32_t read = frame.at(0xC0u);
    call_set(rt, ctx, 0x08B10170u, {read, ctx.gpr[5], frame.at(0x50u), 0x38u, 1u});
    for (std::uint32_t offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, frame.at(0x10u + offset), word(rt, read + offset));
    const std::uint32_t status = word(rt, frame.at(0x10u));
    if (status != 0u) {
        ctx.gpr[2] = status;
        return;
    }
    const std::uint32_t flag = frame.at(8u);
    call_set(rt, ctx, 0x0896A35Cu, {word(rt, self + 0x98u), 0u, 0u, flag, 0u});
    call_set(rt, ctx, 0x0896A35Cu, {word(rt, self + 0x98u), 0u, 0u, 0u, flag});
    const float width = unsigned_to_float(word(rt, frame.at(0x60u)));
    const float height = unsigned_to_float(word(rt, frame.at(0x64u)));
    const float angle = float_at(rt, frame.at(0x74u)) * std::bit_cast<float>(0x3C8EFA35u);
    const float x_scale = float_at(rt, frame.at(0x68u));
    const float offset = float_at(rt, frame.at(0x6Cu));
    const float y_scale = float_at(rt, frame.at(0x70u));
    call_setf(rt, ctx, 0x08B67C24u, {frame.at(0u), frame.at(4u)}, {angle});
    const float sine = float_at(rt, frame.at(0u));
    const float cosine = float_at(rt, frame.at(4u));
    const float half_height = height * 0.5f;
    const float row_cos = y_scale * cosine;
    const float row_sin = y_scale * sine;
    const float half_width = width * 0.5f;
    const float shift = offset * row_cos;
    const float dot = x_scale * row_sin + shift;
    const float cross = x_scale * row_cos - offset * row_sin;
    const std::uint32_t transform = self + 0x248u;
    put_float(rt, transform, row_cos);
    put_float(rt, transform + 0xCu, -row_sin);
    put_float(rt, transform + 0x1Cu, half_height - dot);
    put_float(rt, transform + 0x18u, half_width - cross);
    put(rt, transform + 0x20u, 0u);
    put_float(rt, transform + 4u, row_sin);
    put_float(rt, transform + 0x10u, row_cos);
    put(rt, transform + 8u, 0u);
    put(rt, transform + 0x14u, 0u);
    ctx.gpr[2] = transform;
}

// func_08ACDD14(self): tears down the two channel lists: func_08ACD55C(self, i) for i < self+0x328 and func_08ACDC90(self, i)
// for i < self+0x32C, then per 0xEC-byte slot of the first kind releases +0x410 (func_08B35254) and the handles +0x3EC /
// +0x3F4 on the device self+0x244 (after func_08C7AD50), and per 0xF0-byte slot of the second +0x4D4 and +0x4DC. Then
// stops self+0x40 if +0x160 is set, zeroes the counters, closes the device if +0x162 is set and frees +0x31C / +0x30C / +0x314.
void func_08ACDD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const auto count = [&](std::uint32_t offset) { return signed_of(word(rt, self + offset)); };
    for (std::int32_t i = 0; i < count(0x328u); ++i) call_set(rt, ctx, 0x08ACD55Cu, {self, static_cast<std::uint32_t>(i)});
    for (std::int32_t i = 0; i < count(0x32Cu); ++i) call_set(rt, ctx, 0x08ACDC90u, {self, static_cast<std::uint32_t>(i)});
    if (count(0x328u) > 0) {
        std::uint32_t slot = self;
        std::int32_t k = 0;
        do {
            call_set(rt, ctx, 0x08C7AD50u, {word(rt, self + 0x244u)});
            ++k;
            call_set(rt, ctx, 0x08B35254u, {word(rt, slot + 0x410u)});
            const std::uint32_t first = word(rt, slot + 0x3ECu);
            if (first != 0u) call_set(rt, ctx, 0x08C7ADC0u, {word(rt, self + 0x244u), first});
            const std::uint32_t second = word(rt, slot + 0x3F4u);
            put(rt, slot + 0x3ECu, 0u);
            if (second != 0u) call_set(rt, ctx, 0x08C7AD58u, {word(rt, self + 0x244u), second});
            put(rt, slot + 0x3F4u, 0u);
            slot += 0xECu;
        } while (k < count(0x328u));
    }
    if (count(0x32Cu) > 0) {
        std::uint32_t slot = self;
        std::int32_t k = 0;
        do {
            const std::uint32_t buffer = word(rt, slot + 0x4D4u);
            ++k;
            if (buffer != 0u) {
                call_set(rt, ctx, 0x08B35254u, {buffer});
                put(rt, slot + 0x4D4u, 0u);
            }
            const std::uint32_t handle = word(rt, slot + 0x4DCu);
            if (handle != 0u) call_set(rt, ctx, 0x08C7AD58u, {word(rt, self + 0x244u), handle});
            put(rt, slot + 0x4DCu, 0u);
            slot += 0xF0u;
        } while (k < count(0x32Cu));
    }
    if (byte_u(rt, self + 0x160u) != 0u) {
        call_set(rt, ctx, 0x08B0C778u, {self + 0x40u});
        put8(rt, self + 0x160u, 0u);
    }
    const std::uint32_t open = byte_u(rt, self + 0x162u);
    ctx.gpr[2] = open;
    put(rt, self + 0x15Cu, 0u);
    put(rt, self + 0x158u, 0u);
    put(rt, self + 0x330u, 0u);
    put(rt, self + 0x328u, 0u);
    put(rt, self + 0x32Cu, 0u);
    if (open != 0u) {
        call_set(rt, ctx, 0x08C7AD60u, {word(rt, self + 0x244u)});
        call_set(rt, ctx, 0x08C7AD28u, {self + 0x1C0u});
        put8(rt, self + 0x162u, 0u);
    }
    for (const std::uint32_t offset : {0x31Cu, 0x30Cu, 0x314u}) {
        const std::uint32_t buffer = word(rt, self + offset);
        if (buffer == 0u) continue;
        call_set(rt, ctx, 0x08B35254u, {buffer});
        put(rt, self + offset, 0u);
    }
}

// func_08832C48(result, source, count, values): copies the 0x26D0-byte block func_08893768 of the object found
// through func_088297A8 / func_088294D0 / func_0885EE58 into a fresh zeroed allocation; with count > 0 and a non-zero
// values[0]->slot 0x3C, func_08892C28(copy, values[1]->slot 0x3C, values[2]->slot 0x3C) adjusts it. Then
// result = func_08A52AB8(..., func_088221BC's object, copy, -1, 0x08829348, 0). Null objects go to func_08A5BB7C first.
void func_08832C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t values = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const auto checked = [&](std::uint32_t object, std::uint32_t message) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    const std::uint32_t found = frame.at(4u);
    call_set(rt, ctx, 0x088297A8u, {found, source});
    const std::uint32_t owner = call_set(rt, ctx, 0x088294D0u, {checked(word(rt, found), 0x08C9A2F8u)});
    call_set(rt, ctx, kDestroy, {found});
    const std::uint32_t model = call_set(rt, ctx, 0x0885EE58u, {owner});
    const std::uint32_t copy = call_set(rt, ctx, 0x08B62344u, {0x26D0u});
    call_set(rt, ctx, 0x08B36018u, {copy, 0u, 0x26D0u});
    const std::uint32_t block = call_set(rt, ctx, 0x08893768u, {model});
    for (std::uint32_t offset = 0u; offset < 0x26D0u; offset += 4u) put(rt, copy + offset, word(rt, block + offset));
    const auto value_of = [&](std::uint32_t index) {
        const std::uint32_t object = checked(word(rt, values + index * 4u), 0x08C9A2B4u);
        return call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    };
    if (signed_of(count) > 0 && value_of(0u) != 0u) {
        const std::uint32_t first = value_of(1u);
        const std::uint32_t second = value_of(2u);
        call_set(rt, ctx, 0x08892C28u, {copy, first, second});
    }
    const std::uint32_t made = frame.at(8u);
    call_set(rt, ctx, 0x088221BCu, {made});
    const std::uint32_t maker = checked(word(rt, made), 0x08C9B1D4u);
    call_set(rt, ctx, 0x08A52AB8u, {frame.at(0u), maker, copy, 0xFFFFFFFFu, 0x08829348u, 0u});
    call_set(rt, ctx, kAssign, {result, word(rt, frame.at(0u))});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
    call_set(rt, ctx, kDestroy, {made});
}

// func_088D7B04(self, index, argument, flag): starts the 0x2F8-byte slot index of self+0xC0 (index in range of
// func_088D9F80(self)). A slot without +0x1C needs func_088DA4C8(self, 1); when func_088DAA8C(self) is below the count it
// also needs a positive +0x5C or a set +0x58 in the pair at self+0x58 + 8 x index, else it is prepared with
// func_088E7EBC(slot, index) (under the lock self+0xC8 for a slot with +0x1C), followed by func_088CB3A4(func_088CBFD4())
// for one without. Then func_088E84B0(slot, 0); a slot whose +4 is 1 stops there. func_088E8A84(slot, argument) decides
// whether it runs (func_088E89A4 below the count, func_088D1FBC(self+0x618, index, flag)), between
// func_088DE24C(self, 1, 0) and func_088D9BBC(self, 0, 0). Returns 1 when it ran.
void func_088D7B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t argument = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = 0u;
    const auto below = [&]() {
        const std::int32_t count = signed_of(call_set(rt, ctx, 0x088D9F80u, {self}));
        return signed_of(call_set(rt, ctx, 0x088DAA8Cu, {self})) < count;
    };
    if (signed_of(index) < 0) return;
    if (!(signed_of(index) < signed_of(call_set(rt, ctx, 0x088D9F80u, {self})))) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t slot = word(rt, self + 0xC0u) + index * 0x2F8u;
    if (word(rt, slot + 0x1Cu) == 0u) {
        if (call_set(rt, ctx, 0x088DA4C8u, {self, 1u}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (below()) {
            const std::uint32_t pair = self + index * 8u;
            const std::int32_t state = signed_of(word(rt, pair + 0x5Cu));
            if (state <= 0 && (state != 0 || word(rt, pair + 0x58u) == 0u)) {
                ctx.gpr[2] = 0u;
                return;
            }
        } else {
            call_set(rt, ctx, 0x088E7EBCu, {slot, index});
        }
        call_set(rt, ctx, 0x088CB3A4u, {call_set(rt, ctx, 0x088CBFD4u, {})});
    } else if (!below()) {
        call_set(rt, ctx, kLock, {self + 0xC8u});
        call_set(rt, ctx, 0x088E7EBCu, {slot, index});
        call_set(rt, ctx, kUnlock, {self + 0xC8u});
    }
    call_set(rt, ctx, 0x088E84B0u, {slot, 0u});
    if (word(rt, slot + 4u) == 1u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x088DE24Cu, {self, 1u, 0u});
    std::uint32_t ran = 0u;
    if (call_set(rt, ctx, 0x088E8A84u, {slot, argument}) != 0u) {
        if (below()) call_set(rt, ctx, 0x088E89A4u, {slot});
        call_set(rt, ctx, 0x088D1FBCu, {word(rt, self + 0x618u), index, flag});
        ran = 1u;
    }
    call_set(rt, ctx, 0x088D9BBCu, {self, 0u, 0u});
    ctx.gpr[2] = ran;
}

// func_08820794(result, level, values): from level 3 on, takes the range between the table rows func_0881FC44(values[0]
// ->slot 0x54) (the next row while its +0xC + 1 is below 0x08CE2AD0) and func_0881FC44(values[1]->slot 0x54), and fills
// a list of values[2]->slot 0x3C entries (func_08A98C1C): each picks a random row of the range (func_08B1ECFC over its
// size), a random amount below func_08930290(row) and formats both with func_0884CC18. result takes the list.
void func_08820794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t values = ctx.gpr[6];
    if (signed_of(ctx.gpr[5]) < 3) return;
    GuestFrame frame(ctx, 0x50u);
    const auto value_at = [&](std::uint32_t index) {
        const std::uint32_t object = word(rt, values + index * 4u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
        return object;
    };
    const std::uint32_t low_object = value_at(0u);
    call_virtual_at(rt, ctx, low_object, 0x54u, {low_object});
    const float low_value = ctx.fpr[0];
    const std::uint32_t high_object = value_at(1u);
    call_virtual_at(rt, ctx, high_object, 0x54u, {high_object});
    const float high_value = ctx.fpr[0];
    std::uint32_t low = call_setf(rt, ctx, 0x0881FC44u, {}, {low_value}).v0;
    if (signed_of(word(rt, low + 0xCu) + 1u) < signed_of(word(rt, 0x08CE2AD0u))) low += 0x10u;
    const std::uint32_t high = call_setf(rt, ctx, 0x0881FC44u, {}, {high_value}).v0;
    if (low == 0u || high == 0u) return;
    const std::uint32_t count_object = value_at(2u);
    const std::uint32_t count = call_virtual_at(rt, ctx, count_object, 0x3Cu, {count_object});
    const std::uint32_t list = frame.at(4u);
    call_set(rt, ctx, 0x08A98C1Cu, {list, count});
    const float span = static_cast<float>(signed_of(word(rt, high + 0xCu) - word(rt, low + 0xCu) + 1u));
    const std::uint32_t text = frame.at(0u);
    for (std::uint32_t i = 0u; signed_of(count) > 0 && i != count; ++i) {
        call_setf(rt, ctx, 0x08B1ECFCu, {}, {span});
        const std::uint32_t pick = ctx.fpu_float_to_word(ctx.fpr[0], 1u);
        const std::uint32_t row = (pick + word(rt, low + 0xCu)) * 16u + word(rt, 0x08CE2ACCu);
        const std::uint32_t limit = call_set(rt, ctx, 0x08930290u, {word(rt, row), word(rt, row + 4u)});
        call_setf(rt, ctx, 0x08B1ECFCu, {}, {static_cast<float>(signed_of(limit))});
        const std::uint32_t amount = ctx.fpu_float_to_word(ctx.fpr[0], 1u);
        const std::uint32_t first = word(rt, row);
        const std::uint32_t second = word(rt, row + 4u);
        const std::uint32_t entry = word(rt, word(rt, list) + 0x14u) + i * 4u;
        call_set(rt, ctx, 0x0884CC18u, {text, ctx.gpr[5], first, second, amount});
        call_set(rt, ctx, kAssign, {entry, word(rt, text)});
        call_set(rt, ctx, kDestroy, {text});
    }
    call_set(rt, ctx, kAssign, {result, word(rt, list)});
    call_set(rt, ctx, kDestroy, {list});
}

// func_0888D3D0(self, owner, ids, count): for each 16-bit id, the item func_088DA788(self+4, id) (x / y at +0 / +4) is
// tested against the map func_088DB5C4(self+4) by state func_0889414C(self+0xC): state 1 with func_088DB2C8(self+4, id)
// set, a hit (func_088FB144(map, func_088FB0C8(map, func_088D6218(owner, 1)), y, x) > 0) clears the first byte of
// (self+4)+0x618 and calls self->slot 0x70(owner, &id, 1, 0, 0); state 2 tests with func_088D6218(owner, 0) and on a hit
// runs func_0888C228(self, item, item+0x184, flag) then func_0888D088(self, owner, id, item+0x184, 1) for a flagged id or
// self->slot 0x6C(owner, &id, 1, 0, 1).
void func_0888D3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t ids = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x14u), owner);
    const std::uint32_t state = call_set(rt, ctx, 0x0889414Cu, {word(rt, self + 0xCu)});
    put(rt, frame.at(0x10u), call_set(rt, ctx, 0x088DB5C4u, {word(rt, self + 4u)}));
    const auto hit = [&](std::uint32_t mode, float y, float x) {
        const std::uint32_t level = call_set(rt, ctx, 0x088D6218u, {owner, mode});
        const float reach = call_setf(rt, ctx, 0x088FB0C8u, {word(rt, frame.at(0x10u))}, {static_cast<float>(signed_of(level))}).f0;
        return signed_of(call_setf(rt, ctx, 0x088FB144u, {word(rt, frame.at(0x10u))}, {reach, y, x}).v0) > 0;
    };
    for (std::uint32_t i = 0u; signed_of(count) > 0 && i != count; ++i) {
        const std::uint32_t id = half_u(rt, ids + i * 2u);
        const std::uint32_t item = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        const float x = float_at(rt, item);
        const float y = float_at(rt, item + 4u);
        if (word(rt, state) == 1u) {
            if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id}) != 0u && hit(1u, y, x)) {
                put16(rt, frame.at(0u), id);
                put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
                call_virtual_at(rt, ctx, self, 0x70u, {self, owner, frame.at(0u), 1u, 0u, 0u});
            }
        }
        ctx.gpr[2] = 2u;
        if (word(rt, state) != 2u) continue;
        if (!hit(0u, y, x)) continue;
        put16(rt, frame.at(0u), id);
        const std::uint32_t target = word(rt, item + 0x184u);
        const std::uint32_t flagged = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id});
        call_set(rt, ctx, 0x0888C228u, {self, item, target, flagged});
        if (flagged != 0u) {
            call_set(rt, ctx, 0x0888D088u, {self, owner, id, target, 1u});
        } else {
            call_virtual_at(rt, ctx, self, 0x6Cu, {self, owner, frame.at(0u), 1u, 0u, 1u});
        }
    }
}

// func_089C9024(self, key, value): under the lock 0x08CF4E98, formats "key / func_08B49634(0x08D69524) / value" with the
// pattern 0x08CA6664 and walks the string-keyed tree self+0x48 for the first node whose key contains it
// (func_08C008DC(key, text, -1, length) != -1). That node's value object gets func_089F0AFC, and the node is unlinked
// (func_08B5F444), its value and key released, freed (func_08A5BC54(node, 0x18, 4)) and the count self+0x5C lowered.
void func_089C9024(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0xB0u);
    const std::uint32_t tree = self + 0x48u;
    const std::uint32_t header = self + 0x4Cu;
    const std::uint32_t text = frame.at(8u);
    const std::uint32_t name = frame.at(4u);
    call_set(rt, ctx, kLock, {0x08CF4E98u});
    const std::uint32_t stamp = call_set(rt, ctx, 0x08B49634u, {word(rt, 0x08D69524u)});
    call_set(rt, ctx, 0x08B3767Cu, {text, 0x80u, 0x08CA6664u, word(rt, key), stamp, word(rt, value)});
    for (std::uint32_t node = word(rt, tree + 0xCu); node != header;) {
        call_set(rt, ctx, 0x08B88F44u, {name, node + 0x10u});
        const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {text});
        if (call_set(rt, ctx, 0x08C008DCu, {name, text, 0xFFFFFFFFu, length}) == 0xFFFFFFFFu) {
            node = call_set(rt, ctx, 0x08B5F100u, {node});
            release_string(rt, ctx, name, 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
            continue;
        }
        release_string(rt, ctx, name, 0x08D74FD0u, 0x08B8542Cu, frame.at(1u));
        call_set(rt, ctx, 0x089F09ECu, {name, node + 0x14u});
        const std::uint32_t object = word(rt, name);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA638Cu});
        call_set(rt, ctx, 0x089F0AFCu, {object});
        const std::uint32_t removed = call_set(rt, ctx, 0x08B5F444u, {node, header});
        call_set(rt, ctx, kDestroy, {removed + 0x14u});
        release_string(rt, ctx, removed + 0x10u, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
        call_set(rt, ctx, 0x08A5BC54u, {removed, 0x18u, 4u});
        put(rt, tree + 0x14u, word(rt, tree + 0x14u) - 1u);
        call_set(rt, ctx, kDestroy, {name});
        break;
    }
    call_set(rt, ctx, kUnlock, {0x08CF4E98u});
}

// func_0888C534(self, car, time (64-bit in a2/a3)): every slot func_088DA588(self+4, i) the car belongs to
// (func_088D2B10((self+4)+0x620, func_088D6358(car))) gets its slot 0x20 with time - func_088D9C70(self+4). When
// func_088D2AEC passes and func_088D5E70(car) is at least 3000, func_0889369C(self+0x18, time / 1000, now / 1000) with
// both 64-bit values converted as unsigned (func_08B6EE4C / func_08B6EF68). Ends with func_088936A4(self+0x18, {6, time}).
void func_0888C534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t car = ctx.gpr[5];
    const std::uint32_t time_low = ctx.gpr[6];
    const std::uint32_t time_high = ctx.gpr[7];
    GuestFrame frame(ctx, 0xE0u);
    const std::uint32_t count = call_set(rt, ctx, 0x088D9F80u, {word(rt, self + 4u)});
    for (std::uint32_t i = 0u; signed_of(count) > 0 && i != count; ++i) {
        const std::uint32_t slot = call_set(rt, ctx, 0x088DA588u, {word(rt, self + 4u), i});
        if (slot == 0u) continue;
        const std::uint32_t group = word(rt, word(rt, self + 4u) + 0x620u);
        if (call_set(rt, ctx, 0x088D2B10u, {group, call_set(rt, ctx, 0x088D6358u, {car})}) == 0u) continue;
        const std::uint32_t handler = word(rt, word(rt, slot) + 0x20u);
        const std::uint32_t now_low = call_set(rt, ctx, 0x088D9C70u, {word(rt, self + 4u)});
        const std::uint32_t now_high = ctx.gpr[3];
        const std::uint32_t low = time_low - now_low;
        call_set(rt, ctx, handler, {slot, ctx.gpr[5], low, time_high - now_high - (time_low < low ? 1u : 0u)});
    }
    const std::uint32_t group = word(rt, word(rt, self + 4u) + 0x620u);
    if (call_set(rt, ctx, 0x088D2AECu, {group, call_set(rt, ctx, 0x088D6358u, {car})}) != 0u &&
        !(call_set(rt, ctx, 0x088D5E70u, {car}) < 3000u)) {
        const std::uint32_t now_low = call_set(rt, ctx, 0x088D9C70u, {word(rt, self + 4u)});
        const std::uint32_t now_high = ctx.gpr[3];
        const std::uint32_t target = word(rt, self + 0x18u);
        const auto to_float = [&](std::uint32_t low, std::uint32_t high) {
            if (signed_of(call_set(rt, ctx, 0x08B6EE4Cu, {low, high, 0u, 0u})) > 0) {
                call_set(rt, ctx, 0x08B6EF68u, {low, high});
                return ctx.fpr[0];
            }
            call_set(rt, ctx, 0x08B6EF68u, {(low & 1u) | (high << 31) | (low >> 1), high >> 1});
            return ctx.fpr[0] + ctx.fpr[0];
        };
        const float elapsed = to_float(time_low, time_high) / 1000.0f;
        const float current = to_float(now_low, now_high) / 1000.0f;
        call_setf(rt, ctx, 0x0889369Cu, {target}, {elapsed, current});
    }
    put(rt, frame.at(8u), time_low);
    put(rt, frame.at(0u), 6u);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
}

// func_08ABE53C(value (64-bit in a0/a1), text, size): writes value in decimal with thousands separators into text
// (func_08B7C830 copies at most size): non-positive values copy the string 0x08CB4684; when func_08ABDC28() equals
// 0x08CB4688 the value is scaled by 100 first. Separators by language 0x08D69524: 3 a space, below 3 or from 9 a comma,
// otherwise a dot.
void func_08ABE53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t low = ctx.gpr[4];
    std::uint32_t high = ctx.gpr[5];
    const std::uint32_t text = ctx.gpr[6];
    const std::uint32_t size = ctx.gpr[7];
    GuestFrame frame(ctx, 0xB0u);
    if (signed_of(high) < 0 || (high == 0u && low == 0u)) {
        call_set(rt, ctx, 0x08B7C830u, {text, 0x08CB4684u, size});
        return;
    }
    const std::uint32_t unit = call_set(rt, ctx, 0x08ABDC28u, {});
    const std::uint32_t differs = call_set(rt, ctx, 0x08B35FD8u, {unit, 0x08CB4688u});
    put8(rt, frame.at(0x3Fu), 0u);
    if (differs == 0u) {
        const std::uint64_t scaled = ((static_cast<std::uint64_t>(high) << 32) | low) * 100u;
        low = static_cast<std::uint32_t>(scaled);
        high = static_cast<std::uint32_t>(scaled >> 32);
    }
    std::uint32_t digits = frame.at(0x3Eu);
    std::uint32_t left = 0x3Fu;
    for (;;) {
        --left;
        if (left == 0xFFFFFFFFu) {
            put8(rt, frame.at(0u), 0x3Fu);
            digits = frame.at(0u);
            break;
        }
        const std::uint32_t at = digits;
        const std::uint32_t quotient_low = call_set(rt, ctx, 0x08B6F518u, {low, high, 10u, 0u});
        const std::uint32_t quotient_high = ctx.gpr[3];
        put8(rt, at, quotient_low * 0xFFFFFFF6u + low + 0x30u);
        low = quotient_low;
        high = quotient_high;
        digits = at;
        if ((quotient_low | quotient_high) == 0u) break;
        digits = at - 1u;
    }
    const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {digits});
    put8(rt, frame.at(0x7Fu), 0u);
    const std::int32_t language = signed_of(word(rt, 0x08D69524u));
    std::uint32_t start = 0x3Fu;
    if (signed_of(length) > 0) {
        std::uint32_t source = digits + length - 1u;
        std::uint32_t out = 0x3Eu;
        std::int32_t written = 0;
        bool more = true;
        while (more) {
            ++written;
            put8(rt, frame.at(0x40u + out), byte_u(rt, source));
            more = written < signed_of(length);
            if (written % 3 != 0 || !more) {
                out -= 1u;
            } else {
                const std::uint32_t mark = language == 3 ? 0x20u : (language < 3 || !(language < 9)) ? 0x2Cu : 0x2Eu;
                put8(rt, frame.at(0x40u + out - 1u), mark);
                out -= 2u;
            }
            --source;
        }
        start = out + 1u;
    }
    call_set(rt, ctx, 0x08B7C830u, {text, frame.at(0x40u + start), size});
}

} // namespace hot_batch_112
} // namespace gt::game

#define HOT112(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_112::name, gt::ReturnKind::kind)
// HOT112(0x088FEFC0u, func_088FEFC0, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08C22ED8u, func_08C22ED8, Word);
HOT112(0x08877B68u, func_08877B68, Word);
// HOT112(0x0894419Cu, func_0894419C, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08B8B33Cu, func_08B8B33C, Word);
HOT112(0x0882F04Cu, func_0882F04C, Word);
HOT112(0x088A6DE4u, func_088A6DE4, Word);
// HOT112(0x08C3D12Cu, func_08C3D12C, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x0885C3C8u, func_0885C3C8, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x088D57A0u, func_088D57A0, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x0897F1D8u, func_0897F1D8, Word);
// HOT112(0x08AC9B54u, func_08AC9B54, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08AEC030u, func_08AEC030, Word);
// HOT112(0x08C4B5C8u, func_08C4B5C8, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x08AF5164u, func_08AF5164, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x088D6FD4u, func_088D6FD4, Word);
// HOT112(0x08AAC96Cu, func_08AAC96C, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08B6728Cu, func_08B6728C, Float);
HOT112(0x088A6F74u, func_088A6F74, Word);
HOT112(0x08B67418u, func_08B67418, Float);
// HOT112(0x08B196B8u, func_08B196B8, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x08B67C70u, func_08B67C70, Float);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x089ABDCCu, func_089ABDCC, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x089EF70Cu, func_089EF70C, Word);
HOT112(0x08B8634Cu, func_08B8634C, Word);
HOT112(0x08A180CCu, func_08A180CC, Word);
HOT112(0x08B6EF68u, func_08B6EF68, Float);
HOT112(0x0880714Cu, func_0880714C, Word);
HOT112(0x0883C988u, func_0883C988, Word);
HOT112(0x0883CCC8u, func_0883CCC8, Word);
HOT112(0x08BBBA44u, func_08BBBA44, Word);
HOT112(0x0884085Cu, func_0884085C, Word);
// HOT112(0x08978950u, func_08978950, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x08ABEAECu, func_08ABEAEC, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x08AFF62Cu, func_08AFF62C, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x088F0064u, func_088F0064, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08849B8Cu, func_08849B8C, Word);
HOT112(0x08B8A03Cu, func_08B8A03C, Word);
HOT112(0x08945F6Cu, func_08945F6C, Word);
HOT112(0x08A7EB58u, func_08A7EB58, Word);
// HOT112(0x08AB7754u, func_08AB7754, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x088FB728u, func_088FB728, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT112(0x088BDA50u, func_088BDA50, Float);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x088F5F08u, func_088F5F08, Word);
HOT112(0x089DEA00u, func_089DEA00, Word);
HOT112(0x08A9D93Cu, func_08A9D93C, Word);
HOT112(0x08A8220Cu, func_08A8220C, Word);
// HOT112(0x08B05DC4u, func_08B05DC4, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08BD4724u, func_08BD4724, Word);
// HOT112(0x0880DE48u, func_0880DE48, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x08ACDD14u, func_08ACDD14, Word);
HOT112(0x08832C48u, func_08832C48, Word);
HOT112(0x088D7B04u, func_088D7B04, Word);
HOT112(0x08820794u, func_08820794, Word);
// HOT112(0x0888D3D0u, func_0888D3D0, Word);  // batch 112: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT112(0x089C9024u, func_089C9024, Word);
HOT112(0x0888C534u, func_0888C534, Word);
HOT112(0x08ABE53Cu, func_08ABE53C, Word);
