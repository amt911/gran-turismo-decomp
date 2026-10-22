// Hot batch 121: high-hit functions the 11-route gate reaches (div, 64-bit division, a libm wrapper, a buffered read,
// a bit / value interpreter, the voice mixer), written by hand from `tools.disasm`; role words describe access shapes,
// none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <optional>

namespace gt::game {
namespace hot_batch_121 {
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
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

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



// func_08B67720(f12 = x, f13 = y) -> f0: the libm power wrapper with SVID error handling around r = func_08B6B354(x,
// y). Unless the mode word 0x08C96698 is -1 or y is NaN (func_08B6ECB0), these cases fill an exception record {type,
// name 0x08CB5E74, arg1 = (double) x, arg2 = (double) y, retval, err} and return (float) retval (func_08B71EC8):
// NaN^0 (domain, retval x, or 1.0 in mode 2), 0^0 (domain, 0 in mode 0, else 1.0), 0^negative (domain, 0 or -HUGE_VAL),
// a NaN result of finite arguments (domain, 0 or 0/0), an infinite result of finite arguments (overflow, +-HUGE by the
// sign of x^y) and a zero result of finite non-zero arguments (underflow, 0). Mode 2 sets errno (func_08B77D40) itself;
// otherwise matherr (func_08B6E524) decides, errno being EDOM 0x21 / ERANGE 0x22 when it does not handle it.
void func_08B67720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    GuestFrame frame(ctx, 0x60u);
    const float result = call_setf(rt, ctx, 0x08B6B354u, {}, {x, y}).f0;
    const std::uint32_t mode = word(rt, 0x08C96698u);
    const auto keep = [&] { ctx.fpr[0] = result; };
    const auto is_nan = [&](float value) { return call_setf(rt, ctx, 0x08B6ECB0u, {}, {value}).v0 != 0u; };
    const auto is_finite = [&](float value) { return call_setf(rt, ctx, 0x08B6EC94u, {}, {value}).v0 != 0u; };
    const std::uint32_t record = frame.at(0u);
    const auto start = [&](std::uint32_t type) {
        put(rt, record, type);
        put(rt, record + 4u, 0x08CB5E74u);
        put(rt, record + 0x20u, 0u);
        const std::uint32_t x_low = call_setf(rt, ctx, 0x08B70F28u, {}, {x}).v0;
        const std::uint32_t x_high = ctx.gpr[3];
        put(rt, record + 8u, x_low);
        put(rt, record + 0xCu, x_high);
        const std::uint32_t y_low = call_setf(rt, ctx, 0x08B70F28u, {}, {y}).v0;
        put(rt, record + 0x10u, y_low);
        put(rt, record + 0x14u, ctx.gpr[3]);
    };
    const auto retval = [&](std::uint32_t low, std::uint32_t high) {
        put(rt, record + 0x18u, low);
        put(rt, record + 0x1Cu, high);
    };
    const auto set_errno = [&](std::uint32_t value) { put(rt, call_set(rt, ctx, 0x08B77D40u, {}), value); };
    // the end every case shares: matherr (unless `direct`) or errno, the record's own err, then (float) retval
    const auto finish = [&](bool use_matherr, std::uint32_t error) {
        if (!use_matherr) {
            if (error != 0u) set_errno(error);
        } else if (call_set(rt, ctx, 0x08B6E524u, {record}) == 0u) {
            set_errno(error);
        }
        const std::uint32_t err = word(rt, record + 0x20u);
        if (err != 0u) set_errno(err);
        call_set(rt, ctx, 0x08B71EC8u, {word(rt, record + 0x18u), word(rt, record + 0x1Cu)});
    };
    const auto huge_retval = [&](bool negative) {
        retval(word(rt, 0x08C96690u), word(rt, 0x08C96694u) ^ (negative ? 0x80000000u : 0u));
    };
    if (mode == 0xFFFFFFFFu || is_nan(y)) return keep();
    if (is_nan(x)) {
        if (!(y == 0.0f)) return keep();
        start(1u);
        retval(word(rt, record + 8u), word(rt, record + 0xCu));
        if (mode == 2u) {
            retval(word(rt, 0x08CB6618u), word(rt, 0x08CB661Cu));
            return finish(false, 0u);
        }
        return finish(true, 0x21u);
    }
    if (x == 0.0f) {
        if (y == 0.0f) {
            start(1u);
            retval(0u, 0u);
            if (mode == 0u) return finish(true, 0x21u);
            retval(word(rt, 0x08CB6618u), word(rt, 0x08CB661Cu));
            return finish(false, 0u);
        }
        if (!is_finite(y) || !(y < 0.0f)) return keep();
        start(1u);
        if (mode == 0u) {
            retval(0u, 0u);
            return finish(true, 0x21u);
        }
        huge_retval(true);
        return finish(mode != 2u, 0x21u);
    }
    if (!is_finite(result) && is_finite(x) && is_finite(y)) {
        if (is_nan(result)) {
            start(1u);
            if (mode == 0u) {
                retval(0u, 0u);
                return finish(true, 0x21u);
            }
            const std::uint32_t low = call_set(rt, ctx, 0x08B71720u, {0u, 0u, 0u, 0u});
            retval(low, ctx.gpr[3]);
            return finish(mode != 2u, 0x21u);
        }
        start(3u);
        if (mode == 0u) retval(word(rt, 0x08CB6620u), word(rt, 0x08CB6624u));
        else huge_retval(false);
        const float half = psp_mul(y, float_at(rt, 0x08CB62C4u));
        if (x < 0.0f) {
            const std::uint32_t low = call_setf(rt, ctx, 0x08B70F28u, {}, {half}).v0;
            const std::uint32_t high = ctx.gpr[3];
            const std::uint32_t rounded_low = call_set(rt, ctx, 0x08B6E52Cu, {low, high});
            const std::uint32_t rounded_high = ctx.gpr[3];
            if (call_set(rt, ctx, 0x08B71A08u, {low, high, rounded_low, rounded_high}) != 0u) {
                if (mode == 0u) retval(word(rt, 0x08CB6628u), word(rt, 0x08CB662Cu));
                else huge_retval(true);
            }
        }
        return finish(mode != 2u, 0x22u);
    }
    if (!(result == 0.0f) || !is_finite(x) || !is_finite(y)) return keep();
    start(4u);
    retval(0u, 0u);
    finish(mode != 2u, 0x22u);
}


// func_08B702B4(a0:a1 = n, a2:a3 = d) -> v0:v1: unsigned 64-bit division, libgcc's udivmoddi4 shape (normalising
// through the leading-zero table at 0x08C966C8, two 16-bit quotient digits per word). A zero divisor takes its
// `d0 = 1 / d0` step, which the Allegrex divu answers with 0xFFFFFFFF, so n is then divided by 0xFFFFFFFF.
void func_08B702B4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint64_t n = (std::uint64_t{ctx.gpr[5]} << 32) | ctx.gpr[4];
    const std::uint64_t d = (std::uint64_t{ctx.gpr[7]} << 32) | ctx.gpr[6];
    const std::uint64_t q = n / (d != 0u ? d : 0xFFFFFFFFu);
    ctx.gpr[2] = static_cast<std::uint32_t>(q);
    ctx.gpr[3] = static_cast<std::uint32_t>(q >> 32);
}



// func_08B77CD8(a0 = numerator, a1 = denominator) -> v0 = quotient, v1 = remainder: C's div() in registers. The
// Allegrex div truncates and gives the remainder the numerator's sign; the old ANSI fix-up after it (a remainder of
// the wrong sign moves one step) never fires then, but is kept. Dividing by zero gives the divider's answer: quotient
// -1 (numerator >= 0) or 1, remainder the numerator; 0x80000000 / -1 gives 0x80000000 rem 0.
void func_08B77CD8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::int32_t numerator = signed_of(ctx.gpr[4]);
    const std::int32_t denominator = signed_of(ctx.gpr[5]);
    std::int32_t quotient = allegrex_div(numerator, denominator);
    // the divider's remainder: the numerator itself for a zero divisor, 0 for 0x80000000 / -1
    std::int32_t remainder = denominator == 0 ? numerator : denominator == -1 ? 0 : numerator % denominator;
    if (numerator >= 0 && remainder < 0) {
        ++quotient;
        remainder -= denominator;
    } else if (numerator < 0 && remainder > 0) {
        --quotient;
        remainder += denominator;
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(quotient);
    ctx.gpr[3] = static_cast<std::uint32_t>(remainder);
}


// func_08B14A18(owner, file, request) -> bytes read, 0 or -1: one read of a buffered stream. request = {buffer +0,
// length +4}; the stream state file+0x10 holds {handle +0, base +4, offset +8, left +0xC, generation +0x10}. The
// length is capped to what is left; when the generation func_08B14724 reports differs from the stream's,
// func_08B129F4 resyncs it. With a positive generation the 64 KiB pages the read touches (base>>16 .. (base +
// length-1)>>16) are checked one by one with func_08B12D50; if all are ready func_08B33E50 reads them and a full read
// in an unchanged generation is kept. Otherwise (and with generation <= 0) func_08B1297C runs first and the read is
// retried once. A negative result stores func_08B1830C's error code at file+4 and returns -1; a positive one is copied
// (func_08B13DC8 from base + offset, then the import 0x08C7AAF8 over the buffer) and advances offset / left.
void func_08B14A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t file = ctx.gpr[5];
    const std::uint32_t request = ctx.gpr[6];
    const std::uint32_t stream = word(rt, file + 0x10u);
    const std::uint32_t buffer = word(rt, request);
    std::uint32_t length = word(rt, request + 4u);
    if (word(rt, stream + 0xCu) < length) length = word(rt, stream + 0xCu);
    std::uint32_t generation = call_set(rt, ctx, 0x08B14724u, {owner, file, request});
    if (generation != word(rt, stream + 0x10u)) {
        call_set(rt, ctx, 0x08B129F4u, {owner, file, generation});
        generation = word(rt, stream + 0x10u);
    }
    const auto read = [&] { return call_set(rt, ctx, 0x08B33E50u, {word(rt, stream), buffer, length}); };
    const auto direct_read = [&]() -> std::optional<std::uint32_t> {
        if (signed_of(generation) <= 0) return std::nullopt;
        const std::uint32_t base = word(rt, stream + 4u);
        const std::uint32_t last_page = ((length != 0u ? length - 1u : 0u) + base) >> 16;
        for (std::uint32_t page = base >> 16; page <= last_page; ++page) {
            if (call_set(rt, ctx, 0x08B12D50u, {owner, page}) == 0u) return std::nullopt;
        }
        const std::uint32_t got = read();
        if (got != length || call_set(rt, ctx, 0x08B14724u, {owner, buffer, length}) != generation) return std::nullopt;
        return got;
    };
    std::uint32_t got;
    if (const auto direct = direct_read()) {
        got = *direct;
    } else {
        call_set(rt, ctx, 0x08B1297Cu, {owner, file});
        got = read();
    }
    if (signed_of(got) < 0) {
        put(rt, file + 4u, call_set(rt, ctx, 0x08B1830Cu, {got}));
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (got != 0u) {
        call_set(rt, ctx, 0x08B13DC8u, {word(rt, stream + 4u) + word(rt, stream + 8u), buffer, got});
        call_set(rt, ctx, 0x08C7AAF8u, {buffer, got});
        put(rt, stream + 0xCu, word(rt, stream + 0xCu) - got);
        put(rt, stream + 8u, got + word(rt, stream + 8u));
    }
    ctx.gpr[2] = got;
}


// func_08B22E94(out, object, params) -> out: builds a derived state from object in two programs, then hands it to
// func_08B215B8(out, object+2, values, bits, func_08B218A4(object)). Values are 16-bit lists {+2 count, +4 values};
// bit arrays are {+2 tag, +4 bit count, +5 bits}. The flag func_08C19BEC(0x08D68C74) picks bit 5 / 6 for targets 0xFE
// and 0xFF.
// Program params+0 (when present) fills a local value list (sp+0x24, cleared by func_08B36018) with words
// {op:4, source:8, target:8, channel:8}: the input (object's value `source`, or for op 4 the local `target`) is
// sign-extended at the channel's bit (params+8, 4-byte {bit, key table}) and mapped through the channel's key table
// (params+0xC, 8-byte {x0, x1, y0, y1} rows up to an all-zero row; the last row whose [x0, x1] holds the input wins:
// identity, constant, or linear with a rounding half span); ops 1/4 store, 2 add, 3 subtract the result, all of them
// extend the local count. Without it the object's own list is used.
// Program params+4 (when present) rewrites a local copy (sp+0) of the bit array func_08B21894 returns, with words
// {op:5, source:8, target:8, extra:8}: op 3 copies `extra` input bits; ops 1/2 clear/set the target bit; ops 4-9
// combine it (and, or, xor, and-not, or-not, xnor; the jump table at 0x08C93780) with an input bit, 10-15 with a local
// bit, 0x10-0x15 / 0x16-0x1B with "object / local value `source` lies within the range params+0x10[extra] (default
// 0x80..0xFF)". Every op extends the local bit count.
void func_08B22E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    const std::uint32_t params = ctx.gpr[6];
    GuestFrame frame(ctx, 0xE0u);
    const std::uint32_t bits = frame.at(0u);
    const std::uint32_t locals = frame.at(0x24u);
    const std::uint32_t values = object + 4u;
    const auto half_s = [&](std::uint32_t address) { return signed_of(sext16(half_u(rt, address))); };
    const auto value_of = [&](std::uint32_t list, std::uint32_t index) { return half_s(list + 4u + index * 2u); };
    const bool flag = call_set(rt, ctx, 0x08C19BECu, {0x08D68C74u}) != 0u;
    std::uint32_t used_values = values;
    if (std::uint32_t command = word(rt, params); command != 0u) {
        call_set(rt, ctx, 0x08B36018u, {locals, 0u, 0x82u});
        const std::uint32_t channels = word(rt, params + 8u);
        const std::uint32_t keys = word(rt, params + 0xCu);
        for (std::uint32_t op = word(rt, command); (op & 0xFu) != 0u; command += 4u, op = word(rt, command)) {
            const std::uint32_t kind = op & 0xFu;
            const std::uint32_t source = (op >> 4) & 0xFFu;
            const std::uint32_t target = (op >> 12) & 0xFFu;
            const std::uint32_t channel = channels + ((op >> 20) & 0xFFu) * 4u;
            std::int32_t input = 0;
            if (kind == 4u) {
                input = value_of(locals, target);
            } else if (source < half_u(rt, values + 2u)) {
                input = value_of(values, source);
            }
            const std::int32_t sign_bit = half_s(channel);
            if (static_cast<std::uint32_t>(sign_bit) < 16u) {
                const std::int32_t mask = 1 << sign_bit;
                if ((input & mask) != 0) input |= -mask;
            }
            std::int32_t output = 0;
            for (std::uint32_t row = keys + static_cast<std::uint32_t>(half_s(channel + 2u)) * 8u;; row += 8u) {
                const std::int32_t x0 = half_s(row);
                const std::int32_t x1 = half_s(row + 2u);
                const std::int32_t y0 = half_s(row + 4u);
                const std::int32_t y1 = half_s(row + 6u);
                if ((x0 | x1 | y0 | y1) == 0) break;
                if (input < x0 || x1 < input) continue;
                if (x0 == y0 && x1 == y1) {
                    output = input;
                } else if (y0 == y1) {
                    output = y1;
                } else {
                    const std::int32_t span = x1 - x0;
                    const std::uint32_t scaled = static_cast<std::uint32_t>(span >> 1) +
                                                 static_cast<std::uint32_t>(y1 - y0) * static_cast<std::uint32_t>(input - x0);
                    output = y0 + allegrex_div(signed_of(scaled), span);
                }
            }
            const std::uint32_t slot = locals + 4u + target * 2u;
            if (kind == 1u || kind == 4u) {
                put16(rt, slot, static_cast<std::uint32_t>(output));
            } else if (kind == 2u) {
                put16(rt, slot, half_u(rt, slot) + static_cast<std::uint32_t>(output));
            } else if (kind == 3u) {
                put16(rt, slot, half_u(rt, slot) - static_cast<std::uint32_t>(output));
            }
            if (target >= half_u(rt, locals + 2u)) put16(rt, locals + 2u, target + 1u);
        }
        used_values = locals;
    }
    std::uint32_t program = word(rt, params + 4u);
    const std::uint32_t input_bits = call_set(rt, ctx, 0x08B21894u, {object});
    const std::uint32_t ranges = word(rt, params + 0x10u);
    std::uint32_t used_bits = input_bits;
    if (program != 0u) {
        for (std::uint32_t offset = 0u; offset < 0x24u; offset += 4u) put(rt, bits + offset, 0u);
        put16(rt, bits + 2u, half_u(rt, input_bits + 2u));
        for (std::uint32_t op = word(rt, program); (op & 0x1Fu) != 0u; program += 4u, op = word(rt, program)) {
            const std::uint32_t kind = op & 0x1Fu;
            const std::uint32_t source = (op >> 8) & 0xFFu;
            std::uint32_t target = (op >> 16) & 0xFFu;
            const std::uint32_t extra = op >> 24;
            if (kind == 3u) {
                const std::uint32_t input_count = byte_u(rt, input_bits + 4u);
                for (std::uint32_t step = 0u; step < extra; ++step) {
                    const std::uint32_t from = source + step;
                    const std::uint32_t to = target + step;
                    const std::uint32_t bit = from < input_count ? (byte_u(rt, input_bits + 5u + (from >> 3)) >> (from & 7u)) & 1u : 0u;
                    const std::uint32_t at = bits + 5u + (to >> 3);
                    put8(rt, at, (byte_u(rt, at) & ~(1u << (to & 7u))) | (bit << (to & 7u)));
                    if (to >= byte_u(rt, bits + 4u)) put8(rt, bits + 4u, to + 1u);
                }
                continue;
            }
            std::uint32_t at = bits + 5u + (target >> 3);
            std::uint32_t bit = target & 7u;
            if (target == 0xFEu || target == 0xFFu) {
                target = (target == 0xFEu) == flag ? 6u : 5u;
                at = bits + 5u;
                bit = target;
            }
            const std::uint32_t count = byte_u(rt, bits + 4u);
            std::uint32_t operation;
            std::uint32_t operand = 0u;
            if (kind == 1u) {
                operation = 4u;
            } else if (kind == 2u) {
                operation = 5u;
                operand = 1u;
            } else if (kind < 0x10u) {
                const std::uint32_t array = kind < 0xAu ? input_bits : bits;
                operation = kind < 0xAu ? kind : kind - 6u;
                // the shifted byte, unmasked: only its bit 0 survives the operation
                if (source < byte_u(rt, array + 4u)) operand = byte_u(rt, array + 5u + (source >> 3)) >> (source & 7u);
            } else {
                const std::uint32_t list = kind < 0x16u ? values : locals;
                operation = kind < 0x16u ? kind - 0xCu : kind - 0x12u;
                if (source < half_u(rt, list + 2u)) {
                    const std::int32_t value = value_of(list, source);
                    const std::int32_t low = ranges != 0u ? half_s(ranges + extra * 4u) : 0x80;
                    const std::int32_t high = ranges != 0u ? half_s(ranges + extra * 4u + 2u) : 0xFF;
                    operand = low <= value && value <= high ? 1u : 0u;
                }
            }
            const std::uint32_t old = byte_u(rt, at);
            std::uint32_t current = old >> bit;
            switch (operation) {
            case 4u: current &= operand; break;
            case 5u: current |= operand; break;
            case 6u: current ^= operand; break;
            case 7u: current &= ~operand; break;
            case 8u: current |= ~operand; break;
            case 9u: current ^= ~operand; break;
            default: break;
            }
            const std::uint32_t mask = 1u << bit;
            put8(rt, at, (current & 1u) != 0u ? old | mask : old & ~mask);
            if (target >= count) put8(rt, bits + 4u, target + 1u);
        }
        used_bits = bits;
    }
    const std::uint32_t tag = half_u(rt, object + 2u);
    const std::uint32_t extra = call_set(rt, ctx, 0x08B218A4u, {object});
    call_set(rt, ctx, 0x08B215B8u, {out, tag, used_values, used_bits, extra});
    ctx.gpr[2] = out;
}


// func_08AEFFE0(out, frames, fresh) -> fresh left: mixes the voice list into `frames` stereo s16 frames at out. The
// list (0x08D42018: next +4; a voice whose +8 id is negative ends it) is walked between the import calls
// 0x08C7AA20(word 0x08D42008, 1, 0) and 0x08C7AA08(the same, 1); the ids seen are then passed as a bit mask to the
// import 0x08C7A9A0(word 0x08D4200C, mask). A voice {channels +0x10 (2 = stereo 4-byte frames, else 2-byte mono),
// draining +0x11, left / right volume +0x14 / +0x16 (0x1000 = unity, >> 12), ring buffer +0x1C, size +0x20, bytes
// queued +0x24, read offset +0x2C, frames played +0x30 (64-bit)} is skipped when it holds fewer than `frames` frames,
// unless it is draining (then what is left, rounded up). While `fresh` the voice overwrites the output and the rest of
// it is cleared (func_08B36018); later voices add with clamping. Channel counts other than 1 and 2 only consume.
void func_08AEFFE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::int32_t frames = signed_of(ctx.gpr[5]);
    std::uint32_t fresh = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    constexpr std::uint32_t kList = 0x08D42018u;
    const auto next_voice = [&](std::uint32_t voice) {
        const std::uint32_t next = word(rt, voice + 4u);
        return signed_of(word(rt, next + 8u)) < 0 ? 0u : next;
    };
    if (next_voice(kList) == 0u) {
        ctx.gpr[2] = fresh;
        return;
    }
    call_set(rt, ctx, 0x08C7AA20u, {word(rt, 0x08D42008u), 1u, 0u});
    std::uint32_t mask = 0u;
    const auto clamp = [](std::int32_t value) { return static_cast<std::uint32_t>(std::min(std::max(value, -0x8000), 0x7FFF)); };
    const auto sample = [&](std::uint32_t address) { return signed_of(sext16(half_u(rt, address))); };
    for (std::uint32_t voice = next_voice(kList); voice != 0u; voice = next_voice(voice)) {
        const std::int32_t channels = signed_of(sext8(byte_u(rt, voice + 0x10u)));
        const std::int32_t frame_bytes = channels != 2 ? 2 : 4;
        mask |= 1u << (word(rt, voice + 8u) & 31u);
        const std::int32_t queued = signed_of(word(rt, voice + 0x24u));
        std::int32_t wanted = frames;
        if (byte_u(rt, voice + 0x11u) != 0u) {
            wanted = std::min(frames, allegrex_div(signed_of(static_cast<std::uint32_t>(queued) + static_cast<std::uint32_t>(frame_bytes) - 1u), frame_bytes));
        } else if (allegrex_div(queued, frame_bytes) < frames) {
            continue;
        }
        const std::int32_t left = sample(voice + 0x14u);
        const std::int32_t right = sample(voice + 0x16u);
        const bool unity = left == 0x1000 && right == 0x1000;
        std::int32_t done = 0;
        while (done < wanted) {
            const std::int32_t size = signed_of(word(rt, voice + 0x20u));
            const std::int32_t offset = signed_of(word(rt, voice + 0x2Cu));
            const std::int32_t count = std::min(allegrex_div(signed_of(static_cast<std::uint32_t>(size) - static_cast<std::uint32_t>(offset)), frame_bytes), wanted - done);
            std::uint32_t from = word(rt, voice + 0x1Cu) + static_cast<std::uint32_t>(offset);
            std::uint32_t to = out + static_cast<std::uint32_t>(done) * 4u;
            for (std::int32_t i = 0; i < count && (channels == 1 || channels == 2); ++i, to += 4u) {
                std::int32_t in_left = sample(from);
                std::int32_t in_right = channels == 2 ? sample(from + 2u) : in_left;
                from += static_cast<std::uint32_t>(frame_bytes);
                if (!unity) {
                    in_left = (in_left * left) >> 12;
                    in_right = (in_right * right) >> 12;
                }
                if (fresh != 0u) {
                    put16(rt, to, static_cast<std::uint32_t>(in_left));
                    put16(rt, to + 2u, static_cast<std::uint32_t>(in_right));
                } else {
                    put16(rt, to, clamp(sample(to) + in_left));
                    put16(rt, to + 2u, clamp(sample(to + 2u) + in_right));
                }
            }
            const std::int32_t bytes = count * frame_bytes;
            put(rt, voice + 0x24u, word(rt, voice + 0x24u) - static_cast<std::uint32_t>(bytes));
            const std::uint64_t played = ((std::uint64_t{word(rt, voice + 0x34u)} << 32) | word(rt, voice + 0x30u)) +
                                         static_cast<std::uint64_t>(static_cast<std::int64_t>(count));
            put(rt, voice + 0x30u, static_cast<std::uint32_t>(played));
            put(rt, voice + 0x34u, static_cast<std::uint32_t>(played >> 32));
            const std::int32_t moved = signed_of(static_cast<std::uint32_t>(offset) + static_cast<std::uint32_t>(bytes));
            // the divider's remainder: the dividend itself for a zero size
            put(rt, voice + 0x2Cu, static_cast<std::uint32_t>(size == 0 ? moved : size == -1 ? 0 : moved % size));
            done += count;
        }
        if (fresh != 0u && done < frames) {
            call_set(rt, ctx, 0x08B36018u, {out + static_cast<std::uint32_t>(done) * 4u, 0u, static_cast<std::uint32_t>(frames - done) * 4u});
        }
        fresh = 0u;
    }
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, 0x08D42008u), 1u});
    call_set(rt, ctx, 0x08C7A9A0u, {word(rt, 0x08D4200Cu), mask});
    ctx.gpr[2] = fresh;
}

} // namespace hot_batch_121
} // namespace gt::game

#define HOT121(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_121::name, gt::ReturnKind::kind)
HOT121(0x08B67720u, func_08B67720, Float);
HOT121(0x08B702B4u, func_08B702B4, DoubleWord);
HOT121(0x08B77CD8u, func_08B77CD8, DoubleWord);
// HOT121(0x08B14A18u, func_08B14A18, Word);  // batch 121: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT121(0x08B22E94u, func_08B22E94, Word);
HOT121(0x08AEFFE0u, func_08AEFFE0, Word);
