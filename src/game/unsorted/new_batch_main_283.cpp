// ---- from main-session batch 229 (draft 3b9a182b09d3b5c7ff2bd81e972214c1019955e7) ----
// Main-session batch 229: hand-written structured C from `tools.disasm`.
// Draft only — nothing here is gate-verified yet; the input shapes live in /tmp/claude-1000/shapes_main229.txt.
// 0x0886D5A0's body comes from the main-64 draft (new_batch_main_64.cpp, `destroy_with_subobject`); fixed here: the
// sub-object word is read before the four 0x0886B600() calls (as the asm loads it), and the no-sub-object path keeps
// the last call's v0 instead of 0 (the original returns on its own there).
//
// Delay slots: `beql`/`bnel` (and every `bc1tl`) NULLIFY their delay slot when the branch is not taken, so the
// instruction there belongs to the taken path only; a plain `beq`/`bne`/`beqz`/`bnez`/`bltz`/`bgtz`/`blez` always
// executes it. 0x08B5FB4C's error flags (ios eofbit 2 / failbit 4) and 0x08ACD920's `s4 += 0x140` both ride on that
// distinction. A `jal`'s delay slot runs before the callee, and v0 of a call that sets nothing is what the caller
// passed to the previous one (the calls below go through `call_keep`, which stores v0 / v1 / f0 like the original).
//
// Left out on purpose (both statically linked SDK code, AGENTS.md § Target — game code first):
//   0x089B14F0  libstdc++ internals: an iterator-range vector copy followed by a map/tree insert built on
//               operator new (0x08B62344) and the tree rebalancing helpers 0x08BF9104 / 0x08BF99BC.
//   0x08C67BD8  the printf core: `alloca` on the argument list (ins/`subu sp, sp, v0`), `va_arg` walks and the
//               format dispatch through 0x08C5B850 / 0x08C59E34 / 0x08C5F858.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_229 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 / v1 / f0 hold what the callee left, as after the original's `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments).v0;
}

[[nodiscard]] u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
[[nodiscard]] s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
[[nodiscard]] s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

inline float fbits(u32 bits) { return std::bit_cast<float>(bits); }
inline u32 bits_of(float value) { return std::bit_cast<u32>(value); }
// The Allegrex pair lock at 0x08B344E0 / unlock at 0x08B3457C several of these wrap their body in.
constexpr u32 kLock = 0x08B344E0u;
constexpr u32 kUnlock = 0x08B3457Cu;

// `lw v0, slot(vtable); jalr v0` on `object`: v0 = the target at the callee's entry, then the callee's v0.
u32 virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 function = word(rt, word(rt, object) + slot);
    return call_indirect(rt, ctx, function, arguments);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0886D5A0 (a0 = this): the destructor of an object whose sub-object at +4 owns a 0x1F8-block and three shared
// pointer-style control blocks. Stores the vtable, calls 0x0886BF44(this, 0) and four times 0x0886B600(), and when the
// sub-object exists releases the three members at +0x1C..+0x24 through their virtual +0x2C, clears the three small
// lists at +0x1F4/+0x1DC/+0x1C4 (by their +8 word), then releases the blocks at +0x13C, +0x108, +0x100 and +0xF4,
// runs the member destructors at +0x88 / +0x60 / +0x3C and finally the base at +0 (vtable 0x08CBF758) through
// 0x08B1FD74, and hands the sub-object to 0x08B64654 as a tail call. v0 is that tail call's result, or — with no
// sub-object — the last 0x0886B600() result (the function then returns on its own).
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    // +4 counts the users (the last one runs virtual +8), +8 the weak users (the last one runs virtual +0xC). The
    // original decrements with an ll/sc pair; a single store leaves the same word.
    const u32 users = word(rt, block + 4u);
    put(rt, block + 4u, users - 1u);
    if (users != 1u) return;
    (void)virtual_call(rt, ctx, block, 8u, {block});
    const u32 weak = word(rt, block + 8u);
    put(rt, block + 8u, weak - 1u);
    if (weak != 1u) return;
    (void)virtual_call(rt, ctx, block, 0xCu, {block});
}

// v0 = the sub-object, or 0 when there is none (then v0 keeps the last 0x0886B600() result).
u32 destroy_subobject(Runtime &rt, AllegrexContext &ctx, u32 self) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CBF748u);
    (void)call_keep(rt, ctx, 0x0886BF44u, {self, 0u});
    const u32 inner = word(rt, self + 4u);
    for (u32 round = 0u; round != 4u; ++round) (void)call_keep(rt, ctx, 0x0886B600u);
    if (inner == 0u) return 0u;
    for (u32 slot = 0u; slot != 3u; ++slot) {
        const u32 member = word(rt, inner + 0x1Cu + 4u * slot);
        if (member == 0u) continue;
        (void)virtual_call(rt, ctx, member, 0x2Cu, {member});
        put(rt, inner + 0x1Cu + 4u * slot, 0u);
    }
    const u32 part = inner + 0xF8u;
    (void)call_keep(rt, ctx, 0x08B95120u, {inner + 0x1F4u, word(rt, inner + 0x1F4u + 8u)});
    (void)call_keep(rt, ctx, 0x08B93D3Cu, {inner + 0x1DCu, word(rt, inner + 0x1DCu + 8u)});
    (void)call_keep(rt, ctx, 0x08B95080u, {inner + 0x1C4u, word(rt, inner + 0x1C4u + 8u)});
    const u32 first_block = word(rt, inner + 0x13Cu);
    put(rt, part, 0x08CC6850u);
    if (first_block != 0u) release_shared(rt, ctx, first_block);
    put(rt, part, 0x08CC6838u);
    (void)call_keep(rt, ctx, 0x08B28DC4u, {part + 0x18u});
    const u32 second_block = word(rt, part + 0x10u);
    if (second_block != 0u) release_shared(rt, ctx, second_block);
    const u32 third_block = word(rt, part + 8u);
    if (third_block != 0u) release_shared(rt, ctx, third_block);
    const u32 fourth_block = word(rt, inner + 0xF4u);
    if (fourth_block != 0u) release_shared(rt, ctx, fourth_block);
    (void)call_keep(rt, ctx, 0x0887D4ECu, {inner + 0x88u});
    (void)call_keep(rt, ctx, 0x0890AFC0u, {inner + 0x60u});
    put(rt, inner + 0x38u, 0x08CBFF08u);
    (void)call_keep(rt, ctx, 0x0887C8E8u, {inner + 0x3Cu});
    put(rt, inner + 0x38u, 0x08CD7118u);
    put(rt, inner, 0x08CBF758u);
    (void)call_keep(rt, ctx, 0x08B1FD74u, {inner});
    return inner;
}


// ---------------------------------------------------------------------------------------------------------------
// 0x08B6AF7C (x in $f12): the natural logarithm of a float — fdlibm's `__ieee754_logf` kernel, read word by word
// from the asm: +-0 answers -two25 / 0 (+-inf), a negative x answers (x - x) / 0 (the 0/0 NaN), anything with the
// exponent field above 0x7F7FFFFF answers x + x (inf or a NaN), a subnormal is scaled up by 2^25 (k -= 25).
// The mantissa then picks x or x/2 around 2^23 + 0x4AFB20 and the exponent k becomes dk; f near 0 (the mantissa
// within 15 of 2^23) takes the R = f*f*(0.5 - f/3) form, everything else s = f/(2 + f) with the Lg1..Lg7 polynomial
// at 0x08CB63C4..0x08CB63F4. The 0xFFCF5C30 / 0x35C288 pair built from the mantissa picks the hfsq form or the
// R form of the correction. Result in $f0 (bits compared), as after each `div.s` / mul.s (psp_mul).
void func_08B6AF7C(Runtime &rt, AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const u32 bits = bits_of(x);
    if ((bits << 1) == 0u) {  // `ins v0, zero, 31, 1` of the raw word: +-0 answers -two25 / 0.0f
        ctx.fpr[0] = float_at(rt, 0x08CB63B8u) / 0.0f;
        return;
    }
    const s32 raw = static_cast<s32>(bits);
    if (raw < 0) {  // negative (and not -0): (x - x) / 0.0f
        ctx.fpr[0] = (x - x) / 0.0f;
        return;
    }
    if (static_cast<u32>(raw) > 0x7F7FFFFFu) {  // inf or NaN: x + x
        ctx.fpr[0] = x + x;
        return;
    }
    float value = x;
    u32 value_bits = static_cast<u32>(raw);
    s32 scale = 0;
    if (value_bits <= 0x007FFFFFu) {  // subnormal: x *= two25, k -= 25
        scale = -25;
        value = psp_mul(x, float_at(rt, 0x08CB63BCu));
        value_bits = bits_of(value);
    }
    const u32 mantissa = value_bits & 0x007FFFFFu;
    // `i = (ix + (0x95f64 << 3)) & 0x800000; k += i >> 23; x = ix | (i ^ 0x3f800000)`.
    const u32 carry = (mantissa + 0x4AFB20u) & 0x00800000u;
    const u32 exponent_bits = carry != 0u ? (0x00800000u ^ 0x3F800000u) : 0x3F800000u;
    const float reduced = fbits(mantissa | exponent_bits);
    const s32 k = (static_cast<s32>(value_bits) >> 23) + scale - 0x7F + static_cast<s32>(carry >> 23);
    // `(0x007fffff & (0x8000 + ix)) < 0xc000` compiled as `((mantissa + 0xF) & 0x7fffff) < 0x10`.
    const bool near_one = ((mantissa + 0xFu) & 0x007FFFFFu) < 0x10u;

    const float f = reduced - float_at(rt, 0x08CB63C0u);  // f12 = x - 1
    if (near_one) {
        if (f == 0.0f) {
            if (k == 0) {
                ctx.fpr[0] = 0.0f;
                return;
            }
            const float dk = static_cast<float>(k);
            const float high = psp_mul(dk, float_at(rt, 0x08CB63C4u));  // ln2_hi
            const float low = psp_mul(dk, float_at(rt, 0x08CB63C8u));   // ln2_lo
            ctx.fpr[0] = high + low;
            return;
        }
        const float third = psp_mul(f, float_at(rt, 0x08CB63CCu));  // f * (-1/3)
        const float square = psp_mul(f, f);
        const float poly = third + float_at(rt, 0x08CB63D0u);       // + 0.5
        const float r = psp_mul(square, poly);                      // R = f*f*(0.5 - f/3)
        if (k == 0) {
            ctx.fpr[0] = f - r;
            return;
        }
        const float dk = static_cast<float>(k);
        const float low = psp_mul(dk, float_at(rt, 0x08CB63D4u));   // dk * (-ln2_lo)
        const float high = psp_mul(dk, float_at(rt, 0x08CB63C4u));  // dk * ln2_hi
        ctx.fpr[0] = high - ((r + low) - f);
        return;
    }

    const float s = f / (float_at(rt, 0x08CB63D8u) + f);  // f6 = f / (2 + f)
    const float dk = static_cast<float>(k);               // f7
    const float z = psp_mul(s, s);                        // f4
    const float w = psp_mul(z, z);                        // f3
    // t2 = z*(Lg1 + w*(Lg3 + w*(Lg5 + w*Lg7))); t1 = w*(Lg2 + w*(Lg4 + w*Lg6)).
    float chain = psp_mul(w, float_at(rt, 0x08CB63E8u));  // w * Lg7
    chain = chain + float_at(rt, 0x08CB63ECu);            // + Lg5
    chain = psp_mul(w, chain);
    chain = chain + float_at(rt, 0x08CB63F0u);            // + Lg3
    chain = psp_mul(w, chain);
    chain = chain + float_at(rt, 0x08CB63F4u);            // + Lg1
    const float t2 = psp_mul(z, chain);
    float other = psp_mul(w, float_at(rt, 0x08CB63DCu));  // w * Lg6
    other = other + float_at(rt, 0x08CB63E0u);            // + Lg4
    other = psp_mul(w, other);
    other = other + float_at(rt, 0x08CB63E4u);            // + Lg2
    const float t1 = psp_mul(w, other);
    const float r_sum = t1 + t2;  // f1 = R

    const u32 selector = (mantissa + 0xFFCF5C30u) | (0x35C288u - mantissa);
    if (static_cast<s32>(selector) > 0) {
        const float hfsq = psp_mul(f, psp_mul(f, float_at(rt, 0x08CB63D0u)));  // f5 = f * (f * 0.5)
        if (k == 0) {
            ctx.fpr[0] = f - (hfsq - psp_mul(s, r_sum + hfsq));
            return;
        }
        float inner = r_sum + hfsq;
        const float low = psp_mul(dk, float_at(rt, 0x08CB63C8u));   // dk * ln2_lo
        inner = psp_mul(s, inner);
        inner = inner + low;
        inner = hfsq - inner;
        const float high = psp_mul(dk, float_at(rt, 0x08CB63C4u));  // dk * ln2_hi
        inner = inner - f;
        ctx.fpr[0] = high - inner;
        return;
    }
    if (k != 0) {
        float inner = f - r_sum;
        const float low = psp_mul(dk, float_at(rt, 0x08CB63D4u));   // dk * (-ln2_lo)
        inner = psp_mul(s, inner);
        inner = inner + low;
        inner = inner - f;
        const float high = psp_mul(dk, float_at(rt, 0x08CB63C4u));  // dk * ln2_hi
        ctx.fpr[0] = high - inner;
        return;
    }
    ctx.fpr[0] = f - psp_mul(s, f - r_sum);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x088E1808 (a0 = race object): the per-frame pass over the race's cars that fills one 0x38-byte slot per car in the
// table at word(race + 0x77C) + 0x38*index, for the first car whose slot is still empty (its +0x20 word is -1).
// Skipped entirely when 0x088E4AA8(word(race+0x640)) answers 0 (no session); with the completed count
// (word(+0x660) - word(+0x65C)) >> 4 not below 0x088DAA8C(race), the flag byte +0x5B5 is raised (through
// 0x088DC784(race, 1) when it was clear) and 0x088D62FC(race + 4) runs. Per car: the session must exist, 0x088D9E58
// (race) must be non-zero, the car's +0xC word non-zero and its +4 word 1, and 0x088E85E8(car) must be non-zero;
// 0x088E4860(session, index) gives a 64-bit stamp that must be in (0, now = 0x088D9C70's v0:v1) to run the
// 0x088E47B4 / 0x088F8A84 / 0x088E7F14 / 0x088DD714 / 0x088E4804 group. Then a car whose +0x1C word is not 1 is
// skipped; 0x088E8394(car, frame) gates the rest, and the slot is filled when the car's +4 word is 0 and the word
// 0x088E8394 wrote equals its +0x1C word (otherwise 0x088D6ECC(race, index) runs for a +4 word of 1). The record
// keeps the car, word(race+0x118) and word(race+0x124) and the car's +0x1C, +0x24, +0x28, +0x2C words, its +0x19
// byte and the index (-1 in +0x10, +0x20, +0x30 and +0x34); v0 = that byte. Without a record, v0 = the last
// 0x088D9F80(race) count.
void func_088E1808(Runtime &rt, AllegrexContext &ctx) {
    const u32 race = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    // Every call below that the asm does not give an a1 sees the a1 the previous callee left (a caller-saved
    // register walk), so it is threaded through explicitly. `tools.arg_reads` reports no callee here reading a
    // register this file does not pass, so the few remaining ones (0x088E7F14) cannot see the difference.
    u32 a1 = ctx.gpr[5];
    if (call_keep(rt, ctx, 0x088E4AA8u, {word(rt, race + 0x640u)}).v0 == 0u) return;
    const u32 completed = static_cast<u32>(static_cast<s32>(word(rt, race + 0x660u) - word(rt, race + 0x65Cu)) >> 4);
    const GuestResult limit = call_keep(rt, ctx, 0x088DAA8Cu, {race});
    a1 = limit.a1;
    if (completed < limit.v0) return;  // v0 = the limit
    if (byte_u(rt, race + 0x5B5u) == 0u) {
        a1 = call_keep(rt, ctx, 0x088DC784u, {race, 1u}).a1;
    }
    put8(rt, race + 0x5B5u, 1u);
    a1 = call_keep(rt, ctx, 0x088D62FCu, {race + 4u}).a1;
    const GuestResult started = call_keep(rt, ctx, 0x088D9E58u, {race});
    a1 = started.a1;
    if (started.v0 != 0u) {
        // 0x088D5ED0(race + 4, a1, word(session + 0x20), word(session + 0x24), 0): the leftover a1 again.
        const u32 session = word(rt, race + 0x130u);
        const GuestResult event = call_keep(rt, ctx, 0x088D5ED0u, {race + 4u, a1, word(rt, session + 0x20u),
                                                                    word(rt, session + 0x24u), 0u});
        a1 = event.a1;
    }
    const GuestResult now = call_keep(rt, ctx, 0x088D9C70u, {race});
    a1 = now.a1;
    const u32 now_lo = now.v0;  // s6
    const u32 now_hi = now.v1;  // s7
    for (u32 index = 0u;; ++index) {
        const u32 count = call_keep(rt, ctx, 0x088D9F80u, {race, a1}).v0;
        if (!(index < count)) {
            ctx.gpr[2] = count;  // the loop leaves the last count in v0
            return;
        }
        const u32 car = word(rt, race + 0xC0u) + 0x2F8u * index;
        if (word(rt, race + 0x640u) != 0u) {
            bool eligible = false;
            const GuestResult gate = call_keep(rt, ctx, 0x088D9E58u, {race});
            a1 = gate.a1;
            if (gate.v0 != 0u && word(rt, car + 0xCu) != 0u && word(rt, car + 4u) == 1u) {
                const GuestResult passes = call_keep(rt, ctx, 0x088E85E8u, {car});
                a1 = passes.a1;
                eligible = passes.v0 != 0u;
            }
            if (eligible) {
                const u32 session = word(rt, race + 0x640u);
                const GuestResult stamp = call_keep(rt, ctx, 0x088E4860u, {session, index});
                a1 = stamp.a1;
                const u32 low = stamp.v0;
                const u32 high = stamp.v1;
                bool in_window = false;
                if (static_cast<s32>(high) > 0 || (high == 0u && low != 0u)) {
                    in_window = static_cast<s32>(high) < static_cast<s32>(now_hi) || (high == now_hi && low < now_lo);
                }
                if (in_window) {
                    const u32 slot = call(rt, ctx, 0x088E47B4u, {word(rt, race + 0x640u), index});
                    // `nor v1, zero, v0; movn a2, v0, v1`: a2 = v0 unless v0 is 0xFFFFFFFF.
                    const u32 mapped = slot == 0xFFFFFFFFu ? 0u : slot;
                    (void)call_keep(rt, ctx, 0x088F8A84u, {word(rt, race + 0x14Cu), index, mapped});
                    const u32 produced = call_keep(rt, ctx, 0x088E7F14u, {car}).v0;
                    (void)call_keep(rt, ctx, 0x088DD714u, {race, index, produced});
                    a1 = call_keep(rt, ctx, 0x088E4804u, {word(rt, race + 0x640u), index}).a1;
                }
            }
        }
        const u32 state = word(rt, car + 0x1Cu);  // s0
        if (state != 1u) continue;
        const GuestResult probe = call_keep(rt, ctx, 0x088E8394u, {car, frame.at(0u)});
        a1 = probe.a1;
        if (probe.v0 == 0u) continue;
        if (word(rt, car + 4u) != 0u) {
            // A +4 word of 1 with the probe's first word clear records nothing.
            if (word(rt, frame.at(0u)) == 0u) {
                a1 = call_keep(rt, ctx, 0x088D6ECCu, {race, index}).a1;
            }
            continue;
        }
        if (word(rt, frame.at(0u)) != state) continue;
        const u32 entry = word(rt, race + 0x77Cu) + 0x38u * index;
        const u32 empty = word(rt, entry + 0x20u);
        if (empty != 0xFFFFFFFFu) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        put(rt, entry + 0x1Cu, car);
        put(rt, entry + 0x30u, 0xFFFFFFFFu);
        put(rt, entry + 0x34u, 0xFFFFFFFFu);
        put(rt, entry + 0x24u, word(rt, race + 0x118u));
        put(rt, entry + 0x28u, race);
        put8(rt, entry + 0x2Cu, 0u);
        put(rt, entry, word(rt, car + 0x1Cu));
        put(rt, entry + 4u, word(rt, race + 0x124u));
        put(rt, entry + 8u, word(rt, car + 0x24u));
        put(rt, entry + 0xCu, word(rt, car + 0x28u));
        put(rt, entry + 0x10u, empty);
        put(rt, entry + 0x14u, word(rt, car + 0x2Cu));
        const u32 mark = byte_u(rt, car + 0x19u);
        put8(rt, entry + 0x18u, mark);
        put(rt, entry + 0x20u, index);
        ctx.gpr[2] = mark;
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08ACD920 (a0 = owner, a1 = state block): a submission/poll loop. Under the lock at state+0x80: byte +0xD4 clear
// (else return with the unlock's result) and word +0xCC picks the path — 0 runs the 0x08C7ADD8 dispatch and its
// 0x08ACCD54 / 0x08B2440C / 0x08B24580 follow-up, 1 runs up to 16 attempts of the 0x08C7AD88 dispatch with a
// 0x140-byte record per attempt at sp+0xC (0x08B79A88 copies word(word(state+0xB8)+0x10) / +0x14 into it, 0x1388 ms
// apart), anything else sleeps 20 s. The two words at sp+0x200C / sp+0x2010 (0x2000 / 0x1400 and the +0xC4 counter)
// and sp+0 are written exactly as the asm writes them. A null dispatch answer retries (LOOP), 0x80618001 with the
// owner's byte +0x304 set sleeps 10 s and raises byte state+0xC9, without it 0x08ACD86C(owner, state) runs, and any
// other answer clears byte state+0xD4 and returns.

// ---------------------------------------------------------------------------------------------------------------
// 0x088BD288 (a0 = holder, a1 = params, a2 = geometry pair at +0x44/+0x48, a3 = density record at +4/+0x1A8,
// t0 = extra word, f12 = a, f13 = b): a speed factor.
// a <= b leaves $f0 untouched (the function's only early answer). Otherwise i = (s8)byte(holder+0x12) picks an entry of
// 0x0894268C(word(params), 0, i) (a table of floats at +8, 4 bytes each) and of its second answer (+0x16C per entry);
// word(params)+0x13CC * 0.5 / word(params)+0x328 feeds 0x089468C0(answer + 0x224) whose result scales it, then
// * word(params)+0x137C * word(params)+0x1350. With word(0x088DF13C(word(params+8), word(params+0xC)) + 0x54) zero the
// first two of the four 0x118-byte entries at word(params+4) (their +0xCC floats) average that value. A positive
// float(record+4) scales it (unless float(object+0x1360) < 55.5555 or the half at +0x13E0 is below 500), a positive
// float(record+0x1A8) applies 0x088BB4B8(i, record, extra) as (1 - x) + x*result with a 0.1 floor, and the final
// geometry term uses float(record+0x44)/+0x48 (the +0x48 one dropped when byte(holder+0x28) is 1), the object's
// +0x13AC and 0x08B49CC8(float(holder+0x1C)). Result in $f0; f20-f24 are locals here, never written back.
void func_088BD288(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 params = ctx.gpr[5];
    const u32 geometry = ctx.gpr[6];         // a2: the +0x44 / +0x48 pair
    const u32 density_record = ctx.gpr[7];   // a3: the +4 / +0x1A8 pair and 0x088BB4B8's a1
    const u32 extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const float a = ctx.fpr[12];  // f23
    const float b = ctx.fpr[13];  // f24
    if (a <= b) return;
    const s32 index = signed_byte(rt, holder + 0x12u);
    const u32 table = call(rt, ctx, 0x0894268Cu, {word(rt, params), 0u, static_cast<u32>(index)});
    const u32 object = word(rt, params);
    const float scaled = psp_mul(float_at(rt, object + 0x13CCu), 0.5f) / float_at(rt, object + 0x328u);  // f21
    float total = float_at(rt, table + 4u * static_cast<u32>(index) + 8u);  // f20
    const u32 owner = call(rt, ctx, 0x0894268Cu, {object, 0u, static_cast<u32>(index)});
    (void)call_keep(rt, ctx, 0x089468C0u, {owner + 0x224u}, {scaled});
    total = psp_mul(total, ctx.fpr[0]);
    const u32 entry = call(rt, ctx, 0x0894268Cu, {word(rt, params), 0u, static_cast<u32>(index)});
    total = psp_mul(total, float_at(rt, entry + 0x16Cu));  // +0x16C of the third lookup
    const u32 table_owner = word(rt, params);
    total = psp_mul(total, float_at(rt, table_owner + 0x137Cu));
    total = psp_mul(total, float_at(rt, table_owner + 0x1350u));
    const u32 object2 = call(rt, ctx, 0x088DF13Cu, {word(rt, params + 8u), word(rt, params + 0xCu)});
    float factor = total;  // f22
    if (word(rt, object2 + 0x54u) == 0u) {
        const u32 elements = word(rt, params + 4u);
        float sum = 0.0f;
        u32 counted = 0u;
        for (u32 attempt = 0u; attempt < 4u; ++attempt) {
            // Only the first two of the four 0x118-byte entries add (the beql skips the rest).
            if (attempt < 2u) {
                sum = sum + float_at(rt, elements + 0x118u * attempt + 0xCCu);
                ++counted;
            }
        }
        if (counted != 0u) factor = psp_mul(total, sum) / static_cast<float>(counted);
    }
    const float density = float_at(rt, density_record + 4u);        // f2
    float over = float_at(rt, density_record + 0x1A8u);             // f21
    if (!(density <= 0.0f)) {
        const float threshold = float_at(rt, object + 0x1360u);
        if (!(threshold < fbits(0x425E38E4u)) && signed_half(rt, object + 0x13E0u) < 0x1F4) {
            factor = psp_mul(factor, density);
        }
    }
    if (0.0f < over) {
        const float one = 1.0f;
        const float rest = one - over;
        const GuestResult push =
            call_keep(rt, ctx, 0x088BB4B8u, {static_cast<u32>(index), density_record, extra}, {a});
        float combined = rest + psp_mul(over, push.f0);
        if (combined < fbits(0x3DCCCCCDu)) combined = fbits(0x3DCCCCCDu);
        factor = psp_mul(factor, combined);
    }
    const float one = 1.0f;
    float f2 = float_at(rt, geometry + 0x48u);
    const float g1 = float_at(rt, geometry + 0x44u);
    const float g0 = psp_mul(b, g1);
    const float g2 = psp_mul(a, g1);
    const float h0 = psp_mul(b, g0);
    const float h1 = psp_mul(a, g2);
    const float i0 = h0 + one;
    const float i1 = h1 + one;
    if (byte_u(rt, holder + 0x28u) == 1u) f2 = 0.0f;
    const float sum1 = f2 + i0;
    const float sum2 = f2 + i1;
    float speed = psp_mul(factor, fbits(0x411CCCCDu));
    float ceiling = float_at(rt, object + 0x13ACu);  // f21 is reassigned to the object's ceiling here
    speed = psp_mul(speed, sum2 + sum1);
    speed = psp_mul(speed, 0.5f);
    speed = speed + speed;
    const float divisor = one / speed;
    if (ceiling < divisor) ceiling = divisor;
    const float wrap = call_keep(rt, ctx, 0x08B49CC8u, {}, {float_at(rt, holder + 0x1Cu)}).f0;
    ctx.fpr[0] = psp_mul(psp_mul(ceiling, one - wrap), psp_mul(a + b, a - b)) + 0.0f;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B5FB4C (a0 = stream, a1 = buffer, a2 = size, a3 = delimiter as a signed char): istream::getline. The sentry at
// 0x08C55FDC(&sentry, stream, 1) decides; without it the buffer is only terminated and the state flags are merged.
// With it the streambuf at word(stream + word(word(stream) - 0xC) + 0x78) is drained one char at a time (0x24 is the
// underflow slot, 0x28 the one-char advance) up to size-1 chars, copying runs of 2+ with 0x08B79998 (find the
// delimiter) and 0x08B79A88 (copy), until the delimiter (consumed, cleared flags), end of file (eofbit 2) or a full
// buffer without a delimiter (failbit 4). The count at stream+4 rises per stored char; 0 leaves failbit set too, and
// 0x08C54CFC(stream + word(word(stream)-0xC), word(...+0x14) | flags) merges the flags. v0 = the stream.

} // namespace
} // namespace new_batch_main_229
} // namespace gt::game

#define NEW_MAIN229(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_229::name, gt::ReturnKind::kind)
NEW_MAIN229(0x088BD288u, func_088BD288, Float);
NEW_MAIN229(0x088E1808u, func_088E1808, Word);
NEW_MAIN229(0x08B6AF7Cu, func_08B6AF7C, Float);

// ---- from main-session batch 230 (draft 553fa8869d17a2565ec6b01e66593bb392729266) ----
// Main-session batch 230 — DRAFT ONLY, unverified (the gate is run by the main session).
//
// Written from `tools.disasm`; 0x08B04D98 starts from the body in hot_batch_97.cpp (written there, never registered) and was
// re-read against the asm word by word. Registers: arguments come in a0-a3 / t0-t1, results leave in v0 ("void" below means
// the original leaves no meaningful v0), every delay slot of a call is executed BEFORE the call, the delay slot of a
// `beql`/`bnel`/`bc1tl` only when the branch is taken. Float ops keep the original's order (psp_mul instead of `*` so the
// compiler cannot contract them into an fma and inf*0 gives the PSP's NaN).
//
// Left out of this batch (read, not written — unsure enough that a draft would guess):
//  - 0x08B6A844 and 0x08B69EBC: hand-written IEEE assembly from the linked libm (bit-field `ins` juggling, NaN/inf arms,
//    float arguments in f12/f13 and a double in a0/a1). A C transcription would have to reproduce NaN sign and rounding
//    choices the asm makes instruction by instruction; guessing them is worse than leaving the function recompiled.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace new_batch_main_230 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose result lands in v0 (the asm's v0 after a `jal` that the code then reads).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call with float arguments; v0 / v1 / f0 end where the callee left them.
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The compiler's virtual call: v1 = the vtable (object[0]), the callee = vtable[slot].
GuestResult vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    return call_indirect(rt, ctx, word(rt, table + slot), arguments, {});
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------------------
// 0x0892AE14 (a0 = flag word, a1 = table of two words, a2 = parameter block, a3 = command, t0 = amount, t1 = flag byte):
// the jump-table dispatcher of the parameter block's commands. Each command either returns at once or ends in a tail call;
// the block is written at +2 / +7 / +0x14 (bytes), +0x2C (float) and +0x34 (word). Commands 9, 10, 14 and 15 share the
// gate `movz v1, v0, t1`: `amount != 0` when the caller's flag byte is set, else `!(amount < 0x4000)`.
// v0: the original leaves the dispatch's own scratch value (the table address plus the command's index), never read back —
// registered Void.
void func_0892AE14(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);  // the frame itself is never addressed (the tail callees get the caller's stack)
    (void)frame;
    const u32 self = ctx.gpr[4];
    const u32 table = ctx.gpr[5];
    const u32 parameters = ctx.gpr[6];
    const s32 command = static_cast<s32>(ctx.gpr[7]);
    const u32 amount = ctx.gpr[8];
    const u32 flags = ctx.gpr[9] & 0xFFu;
    const auto passes = [&] { return flags == 0u ? !(static_cast<s32>(amount) < 0x4000) : amount != 0u; };
    if (static_cast<u32>(command) >= 0x14u) return;
    switch (command) {
    case 0:
        break;
    case 1:
    case 2:
        tail_call(ctx, 0x0892A980u, {parameters, amount, word(rt, table)});
        return;
    case 3:
        tail_call(ctx, 0x0892A980u, {parameters, 0u - amount, word(rt, table)});
        return;
    case 4:
        tail_call(ctx, 0x0892A9E4u, {parameters, amount, word(rt, table + 4u)});
        return;
    case 5:
        tail_call(ctx, 0x0892AA38u, {parameters, amount});
        return;
    case 6:
        tail_call(ctx, 0x0892AA78u, {parameters, amount, (self >> 1) & 1u});
        return;
    case 7:
        tail_call(ctx, 0x0892AACCu, {parameters, amount});
        return;
    case 8:
        tail_call(ctx, 0x0892AB0Cu, {parameters, amount});
        return;
    case 9:
        if (!passes()) return;
        put8(rt, parameters + 2u, byte_at(rt, parameters + 2u) + 1u);
        break;
    case 10:
        if (!passes()) return;
        put8(rt, parameters + 2u, byte_at(rt, parameters + 2u) - 1u);
        break;
    case 11:
        if ((self & 4u) == 0u) return;
        call(rt, ctx, 0x0892AB4Cu, {parameters, amount});
        if ((self & 8u) != 0u) return;
        put8(rt, parameters + 0x14u, byte_at(rt, parameters + 0x14u) | 4u);
        break;
    case 12:
        if ((self & 4u) == 0u) return;
        tail_call(ctx, 0x0892AB60u, {parameters, amount});
        return;
    case 13:
        if ((self & 4u) == 0u) return;
        tail_call(ctx, 0x0892AB6Cu, {parameters, amount});
        return;
    case 14:
        if (!passes()) return;
        put8(rt, parameters + 7u, byte_at(rt, parameters + 7u) | 2u);
        break;
    case 15:
        if (!passes()) return;
        put8(rt, parameters + 7u, byte_at(rt, parameters + 7u) | 4u);
        break;
    case 16: {
        const float step = static_cast<float>(static_cast<s32>(amount - 0x4000u)) * std::bit_cast<float>(0x38800000u);
        if (std::fabs(float_at(rt, parameters + 0x2Cu)) < std::fabs(step)) put_float(rt, parameters + 0x2Cu, step);
        return;
    }
    case 17: {
        const float step = static_cast<float>(static_cast<s32>(0u - amount)) / std::bit_cast<float>(0x46FFFE00u);
        if (std::fabs(float_at(rt, parameters + 0x2Cu)) < std::fabs(step)) put_float(rt, parameters + 0x2Cu, step);
        return;
    }
    case 18: {
        const float step = static_cast<float>(static_cast<s32>(amount)) / std::bit_cast<float>(0x46FFFE00u);
        if (std::fabs(float_at(rt, parameters + 0x2Cu)) < std::fabs(step)) put_float(rt, parameters + 0x2Cu, step);
        return;
    }
    case 19:
        put(rt, parameters + 0x34u, amount);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08B04D98(out, size, a, b): builds the 0x120-byte object o = 0x08B62344(0x120) and stores it in out[0]. The intrusive list
// heads (a halfword -1 at +2 of a node, the node's own address or a neighbour's at +8) and the counters are linked by hand,
// then the seven members are initialised: 0x08C4A6B0(o, n), 0x08C4A4AC(o + 0x2C, n), 0x08C4A3B8(o + 0xA8, k),
// 0x08C4A2C4(o + 0x38, 2), 0x08C4A8F8(o + 0x44, 0x20), 0x08C4A7CC(o + 0x60, 4k or 0x100) and 0x08C4A594(o + 0x7C, n), with
// n = a (0x40 when a is 0) and k = b (0x40 when b is 0, and then sp[0] = 0x100 instead of 4k). The tuning floats follow, then
// out[4] = the address of a function-local static (guard 0x08D77330: when its byte is 0 and 0x08B6441C(guard) answers non-zero
// the static is set to 0x08CD79B0 and 0x08B64494(guard) closes it). The work goes on in the tail call 0x08B039F8(out, size or
// 0x60, 0x10), so v0 is that callee's.

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08959704 (a0 = emitter object, a1 = state block, a2 = source block, a3 = count): copies the source's 0x34 bytes into the
// state, then integrates it for min(count, 300 * table byte) steps in chunks of (steps + 25) / 50. Per chunk: the two velocity
// words at +8 / +0xC take a step along +0x14 / +0x18 scaled by the chunk's rate, the position at +0x10 advances by
// rate * (+0x1C), and unless the chunk index is past `segment` (6 * table byte, or the chunk count) the block is rotated by
// its own heading through two 0x08B498AC evaluations (a sine/cosine pair written to the frame) and its +0x1C spins by +0x28.
// A non-empty global counter at 0x08CF3550 fades the scale 0x08CF354C per chunk. The state's 0x08CDE1F4-table bytes are the
// particle budget (+2) and the segment count (+6). v0: scratch (the last table read), not read back — registered Void.
void func_08959704(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 object = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const s32 count = static_cast<s32>(ctx.gpr[7]);
    const float total_span = psp_mul(static_cast<float>(count), std::bit_cast<float>(0x39AEC33Fu));
    for (u32 offset = 0u; offset != 0x30u; offset += 0x10u) {
        const u32 w0 = word(rt, source + offset);
        const u32 w1 = word(rt, source + offset + 4u);
        const u32 w2 = word(rt, source + offset + 8u);
        const u32 w3 = word(rt, source + offset + 0xCu);
        put(rt, state + offset, w0);
        put(rt, state + offset + 4u, w1);
        put(rt, state + offset + 8u, w2);
        put(rt, state + offset + 0xCu, w3);
    }
    put(rt, state + 0x30u, word(rt, source + 0x30u));
    if (count <= 0) return;
    // The sine/cosine pair of the source's heading (+0x10): this call writes it to (sp+4, sp).
    callf(rt, ctx, 0x08B498ACu, {frame.at(4u), frame.at(0u)}, {float_at(rt, source + 0x10u)});
    const s32 budget = static_cast<s32>(byte_at(rt, 0x08CDE1F6u)) * 300;
    s32 steps = count;
    if (budget > 0) steps = std::min(count, budget);
    const float side = float_at(rt, source + 0x14);
    const float forward = float_at(rt, source + 0x18);
    const float cross = float_at(rt, frame.at(0u));
    const float along = float_at(rt, frame.at(4u));
    const float heading = psp_mul(forward, cross) - psp_mul(along, side);
    s32 chunk = (steps + 25) / 50;
    float chunk_float = static_cast<float>(chunk);
    if (chunk <= 0) {
        chunk = 1;
        chunk_float = 1.0f;
    }
    const s32 segments = byte_at(rt, 0x08CDE1FAu) != 0u ? static_cast<s32>(byte_at(rt, 0x08CDE1FAu)) * 6 : chunk;
    const float rate = psp_mul(total_span / chunk_float, float_at(rt, object + 0x4F0u));
    if (chunk <= 0) return;
    float spin = float_at(rt, state + 0x1Cu);
    float scale = 1.0f;
    s32 index = 0;
    do {
        const u32 counter = word(rt, 0x08CF3550u);
        float step_scale = rate;
        if (counter != 0u) {
            const bool ahead = index < static_cast<s32>(counter);
            step_scale = psp_mul(scale, rate);
            if (ahead) scale = scale - float_at(rt, 0x08CF354Cu);
        }
        for (u32 k = 0u; k != 2u; ++k) {
            const u32 at = state + 4u * k;
            const float velocity = float_at(rt, at + 0x14u);
            const float current = float_at(rt, at + 8u);
            put_float(rt, at + 8u, current + psp_mul(rate, velocity));
        }
        const float position = float_at(rt, state + 0x10u) + psp_mul(rate, spin);
        put_float(rt, state + 0x10u, position);
        if (!(segments < index)) {
            const float speed_x = float_at(rt, state + 0x20u);
            const float speed_y = float_at(rt, state + 0x24u);
            callf(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)}, {position});
            const float s = float_at(rt, frame.at(0u));
            const float c = float_at(rt, frame.at(4u));
            const float back = -s;
            float vx = psp_mul(rate, speed_x);
            float vy = psp_mul(step_scale, speed_y);
            float y = float_at(rt, state + 0x14u);
            const float t1 = psp_mul(vy, s);
            const float t0 = psp_mul(vx, c);
            vy = psp_mul(vy, c);
            vx = psp_mul(vx, back);
            float z = float_at(rt, state + 0x18u);
            const float sum0 = t0 + t1;
            const float zero = 0.0f;
            const float sum_vx = vx + vy;
            z = z + sum0;
            y = y + sum_vx;
            const float t2 = psp_mul(z, c);
            put_float(rt, state + 0x18u, z);
            float q0 = psp_mul(back, y);
            q0 = q0 + t2;
            const float turn = psp_mul(heading, q0);
            const bool negative = turn < zero;
            put_float(rt, state + 0x14u, y);
            if (negative) {
                float m0 = psp_mul(y, c);
                const float m4 = psp_mul(z, s);
                float m1 = psp_mul(c, zero);
                float m2 = psp_mul(back, zero);
                m0 = m0 + m4;
                const float m3 = psp_mul(m0, s);
                const float m5 = psp_mul(c, m0);
                m1 = m1 + m3;
                m2 = m2 + m5;
                put_float(rt, state + 0x18u, m1);
                put_float(rt, state + 0x14u, m2);
            }
            const float spin_add = psp_mul(rate, float_at(rt, state + 0x28u));
            const float current_spin = float_at(rt, state + 0x1Cu);
            const float updated = current_spin + spin_add;
            spin = updated;
            put_float(rt, state + 0x1Cu, updated);
        }
        ++index;
    } while (index != chunk);
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x088F1588 (a0 = result block, a1 = holder {word a, word b, word c}, a2 = key): the camera-style follower. Looks the key up
// twice on *(a1) (0x088DCD90 for the object, 0x088DF13C for the float at +0), copies that float to +0 and counts the object's
// elements that answer non-zero through vtable slots +0x8C (count) and +0x90 (per index). `alive` is set when every element
// answered and 0x088EF924(*(a1 + 0x18)) answers 0; the state byte at +0x3A moves 1 -> 2 / -1, 2 -> -1, -1 -> 1 / 0 and any
// other value to 1 when alive. Then +4 = the object's slot +0x3C's word +4 through 0x08B499B8, the heading is turned into
// +8 / +0xC (0x08B498AC writes the pair into the frame), the slot's result fills +0x10 / +0x14 / +0x18 (the last one the
// length 0x08B49D7C computes from the first two), 0x088A4630 runs, and the holder's second object's +0x14 / +0x18 slots give
// +0x20 and the +0x1C counter stepping. The early path (no object) hands over to 0x088F0FBC. v0: the original leaves the last
// call's result, no caller reads it — registered Void.
void func_088F1588(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 result = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    const u32 object = call(rt, ctx, 0x088DCD90u, {word(rt, holder), key});
    if (object == 0u) {
        call(rt, ctx, 0x088F0FBCu, {result});
        return;
    }
    const u32 value = call(rt, ctx, 0x088DF13Cu, {word(rt, holder), key});
    put_float(rt, result, float_at(rt, value));
    const GuestResult count = vcall(rt, ctx, object, 0x8Cu, {object});
    const s32 elements = static_cast<s32>(count.v0);
    u32 answered = 0u;
    if (elements > 0) {
        s32 index = 0;
        do {
            const GuestResult answer = vcall(rt, ctx, object, 0x90u, {object, static_cast<u32>(index)});
            ++index;
            answered += answer.v0 != 0u ? 1u : 0u;
        } while (static_cast<u32>(elements) != static_cast<u32>(index));
    }
    bool alive = false;
    if (elements > 0 && static_cast<s32>(answered) == elements) alive = call(rt, ctx, 0x088EF924u, {word(rt, holder + 0x18u)}) == 0u;
    const s32 state = signed_byte_at(rt, result + 0x3Au);
    if (state == 1) {
        put8(rt, result + 0x3Au, alive ? 2u : 0xFFFFFFFFu);
    } else if (state == 2) {
        if (!alive) put8(rt, result + 0x3Au, 0xFFFFFFFFu);
    } else if (state == -1) {
        put8(rt, result + 0x3Au, alive ? 1u : 0u);
    } else if (alive) {
        put8(rt, result + 0x3Au, 1u);
    }
    const GuestResult first = vcall(rt, ctx, object, 0x3Cu, {object});
    const float heading = float_at(rt, first.v0);
    const GuestResult second = vcall(rt, ctx, object, 0x3Cu, {object});
    const float angle = float_at(rt, second.v0 + 4u);
    put_float(rt, result + 4u, angle);  // the `swc1` sits in the delay slot of the 0x08B499B8 call
    const float turn = callf(rt, ctx, 0x08B499B8u, {}, {heading});
    const float side = -turn;
    const float cosine = float_at(rt, result + 4u);
    const float sine = float_at(rt, result + 8u);
    callf(rt, ctx, 0x08B498ACu, {result + 8u, result + 0xCu}, {cosine});
    const float scaled_cos = psp_mul(turn, cosine);
    const float scaled_sin = psp_mul(sine, side);
    put_float(rt, result + 0xCu, scaled_cos);
    put_float(rt, result + 8u, scaled_sin);
    call(rt, ctx, 0x088A444Cu, {frame.at(0u), object});
    const float up = -float_at(rt, frame.at(8u));
    const float x = float_at(rt, frame.at(0u));
    const float z = float_at(rt, frame.at(4u));
    const float length = psp_mul(x, x) + psp_mul(up, up) + psp_mul(z, z);
    put_float(rt, result + 0x10u, x);
    put_float(rt, result + 0x14u, up);
    callf(rt, ctx, 0x08B49D7Cu, {}, {length});
    put_float(rt, result + 0x18u, ctx.fpr[0]);
    call(rt, ctx, 0x088A4630u, {object});
    const u32 watcher = word(rt, holder + 4u);
    const GuestResult asked = vcall(rt, ctx, watcher, 0x14u, {watcher});
    const float asked_float = asked.f0;
    const float position = float_at(rt, result);  // loaded in the delay slot of the 0x18 query, before it runs
    const GuestResult mode = vcall(rt, ctx, watcher, 0x18u, {watcher, 6u});
    if (mode.v0 != 0u) {
        callf(rt, ctx, 0x088C50B8u, {frame.at(0u), watcher, 6u}, {});
        const u32 slot = callf(rt, ctx, 0x088C50D4u, {frame.at(0u), static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(half_at(rt, result + 0x1Cu))))},
                               {position, asked_float});
        const u32 narrowed = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(slot)));
        put16(rt, result + 0x1Cu, narrowed);
        u32 index = narrowed - 1u;
        if (static_cast<s32>(index) < 0) index = word(rt, frame.at(0u)) - 1u;
        callf(rt, ctx, 0x088F1000u, {index, watcher, 6u}, {position});
        put_float(rt, result + 0x20u, ctx.fpr[0]);
    }
    const u32 fade = byte_at(rt, result + 0x36u);
    if (fade != 0u) {
        const u32 next = (fade - 1u) & 0xFFu;
        put8(rt, result + 0x36u, next);
        if (next == 0u) {
            put16(rt, result + 0x34u, 0xFFFFFFFFu);
            put16(rt, result + 0x32u, 0u);
            put16(rt, result + 0x30u, 0u);
        }
    }
    const u32 blink = byte_at(rt, result + 0x37u);
    if (blink != 0u) {
        const u32 next = (blink - 1u) & 0xFFu;
        put8(rt, result + 0x37u, next);
        if (next == 0u) {
            put16(rt, result + 0x38u, 0u);
        } else {
            put16(rt, result + 0x38u, half_at(rt, result + 0x38u) + 1u);
        }
    }
    const u32 countdown = byte_at(rt, result + 0x3Cu);
    if (countdown != 0u) put8(rt, result + 0x3Cu, countdown - 1u);
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08A2EEDC (a0 = destination, a1 = argument of the work object, a2 = count, a3 = key source): builds the work object at
// sp+0x88 (0x08B09F98), fills the descriptor at sp+0x58 twice (the same fill: halfword +0x5A = 0, bytes +0x5D = 1, +0x5E = 1,
// +0x5F = 0, words +0x60 = +0x68 = +0x74 = 0x4000, +0x64 = 0x100000, +0x6C = +0x70 = 0xC000, +0x78 = +0x7C = 0, +0x80 = -1,
// +0x84 = 0) with 0x08B09264 in between, and runs the key source through 0x08A6BA34 into the string at sp+4. Five keys of the
// table 0x08CAC5B4 / 0x08CAC780 / 0x08CAC788 / 0x08CAC794 are looked up (0x08A6BE64 finds, 0x08A6BBE8 loads the element, a
// null element reports 0x08CAC378), and the element's virtual slot 0x30 / 0x3C answer fills the descriptor's +0x84, +0x5E,
// +0x80 and +0x78. Then 0x08B0A52C(*(0x08CF6648), sp+8) and 0x08A97A6C(sp, answer) make a holder, which is assigned to the
// destination unless it is the frame itself; both temporaries and the work object are released. v0: the last release's,
// never read back — registered Void.
void func_08A2EEDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xF0u);
    const u32 destination = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 key_source = ctx.gpr[7];
    if (count <= 0) return;
    put(rt, frame.at(0xB0u), destination);
    const u32 work = frame.at(0x88u);
    call(rt, ctx, 0x08B09F98u, {work, argument});
    const u32 text = frame.at(4u);
    const auto fill = [&] {
        put16(rt, frame.at(0x5Au), 0u);
        put8(rt, frame.at(0x5Du), 1u);
        put8(rt, frame.at(0x5Eu), 1u);
        put8(rt, frame.at(0x5Fu), 0u);
        put(rt, frame.at(0x60u), 0x4000u);
        put(rt, frame.at(0x64u), 0x100000u);
        put(rt, frame.at(0x68u), 0x4000u);
        put(rt, frame.at(0x6Cu), 0xC000u);
        put(rt, frame.at(0x70u), 0xC000u);
        put(rt, frame.at(0x74u), 0x4000u);
        put(rt, frame.at(0x78u), 0u);
        put(rt, frame.at(0x7Cu), 0u);
        put(rt, frame.at(0x80u), 0xFFFFFFFFu);
        put(rt, frame.at(0x84u), 0u);
    };
    fill();
    call(rt, ctx, 0x08B09264u, {work, 0u});
    fill();
    call(rt, ctx, 0x08B09264u, {work, 0u});
    call(rt, ctx, 0x08A6BA34u, {text, key_source});
    // Each lookup: element = *(0x08A6BE64(text, key)); its vtable is read even when the element is null (the delay slot of the
    // `bne`), and the null path reports and reads it again.
    const auto lookup = [&](u32 key) {
        const u32 slot = call(rt, ctx, 0x08A6BE64u, {text, key});
        const u32 element = word(rt, slot);
        u32 vtable = word(rt, element);
        if (element == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
            vtable = word(rt, element);
        }
        return std::pair<u32, u32>{element, vtable};
    };
    {
        const auto [element, vtable] = lookup(0x08CAC5B4u);
        const GuestResult answer = call_indirect(rt, ctx, word(rt, vtable + 0x30u), {element}, {});
        put(rt, frame.at(0x84u), answer.v0);
    }
    {
        const auto [element, vtable] = lookup(0x08CAC780u);
        const GuestResult answer = call_indirect(rt, ctx, word(rt, vtable + 0x30u), {element}, {});
        put8(rt, frame.at(0x5Eu), answer.v0);
    }
    {
        const auto [element, vtable] = lookup(0x08CAC788u);
        const GuestResult answer = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {element}, {});
        put(rt, frame.at(0x80u), answer.v0);
    }
    {
        const auto [element, vtable] = lookup(0x08CAC794u);
        const GuestResult answer = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {element}, {});
        put(rt, frame.at(0x78u), answer.v0);
    }
    const u32 answer = call(rt, ctx, 0x08B0A52Cu, {word(rt, 0x08CF6648u), frame.at(8u), 0u});
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), answer});
    const u32 target = word(rt, frame.at(0xB0u));
    if (target != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {target, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {text});
    call(rt, ctx, 0x08B09290u, {work});
}
} // namespace
} // namespace new_batch_main_230
} // namespace gt::game

#define NEW230(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_230::name, gt::ReturnKind::kind)
NEW230(0x0892AE14u, func_0892AE14, Void);
NEW230(0x08959704u, func_08959704, Void);
NEW230(0x088F1588u, func_088F1588, Void);
NEW230(0x08A2EEDCu, func_08A2EEDC, Void);

// ---- from main-session batch 231 (draft e633e2c01a95eacf1be328357174018ad04d7da8) ----
// New batch (main-session batch 231): functions the extra seeds made emittable, written as structured C from
// `tools.disasm`. Draft only — not through the differential gate yet. No names: none has evidence.
// Not drafted here (seen, left alone): 0x08882030 (its pivot loop walks 18 words from sp while only the first seven
// are written, so it reads its own saved registers and the caller's frame — the frame has to be modelled word for word
// before the gate can compare it) and 0x08B30A6C (it hands 0x08B308FC and 0x08B30660 an $a1 that only the preceding
// 0x08B2F774 call's leftover can supply, and both callees read $a1 before writing it).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_231 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A call whose answer is a two-word struct in $v0/$v1 (`lw a1, 0(v1)` and friends read the second word).
struct Pair {
    u32 v0;
    u32 v1;
};
Pair call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return {result.v0, result.v1};
}

// The same for a callee that answers in $f0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }

// s0-s7, fp and f20-f31 are callee-saved: the original keeps values in them and gives the caller's back on every exit.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) gpr_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12u; ++i) fpr_[i] = ctx.fpr[20u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12u; ++i) ctx_.fpr[20u + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

void swap_floats(Runtime &rt, u32 first, u32 second) {
    const float value = float_at(rt, first);
    put_float(rt, first, float_at(rt, second));
    put_float(rt, second, value);
}

// 0x088C58F0 (a0 = object, a1/a2/a3 = three float outputs, t0/t1/t2 = three word outputs, $f12-$f15 = x/y/z/w, plus
// the two words at the caller's stack top): asks the object's virtual at +8 whether the point is inside, then resolves
// the four corner offsets through 0x088C5834 and writes the chosen distances out. v0 = 1 when the point was hit, else 0.

// 0x08A511E0 (a0 = object, a1 = a name/handle argument): fills the object from a 0xA8-byte descriptor the file helpers
// read into its frame, allocates the two strips and the two line tables, then starts the worker thread at 0x08A5176C.
// Every failure path logs 0x08CF6D34 and returns.

// 0x08A7A0D8 (a0 = map holder whose tree header sits at +0x10, a1 = key pointer, a2 = value holder pointer): the map's
// insert: an existing key gets the value assigned (a null value erases the node and frees it); when the value's object
// answers its +0x28 virtual the value goes through its +0x64 instead; a missing key gets a node built from the key and
// inserted at the lower bound, creating the tree header when the map is empty. When the map empties, the header is
// destroyed and the tree pointer cleared.

// 0x08B036F8 (a0 = object with a candidate count in bits 25-29 of +0x1FC and a byte-pair array at +0x1C0, a1 = a target
// object, a2/a3 = two position sources, t0 = a float distance output, t1 = a vector output, t2 = a float output): walks
// the object's byte pairs, measures each candidate's distance and keeps the nearest one. v0 = the chosen pair's index,
// or -1 when no candidate was closer than zero.
void func_08B036F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x100u);
    const u32 object = ctx.gpr[4];
    const u32 target_object = ctx.gpr[5];
    const u32 source_a = ctx.gpr[6];
    const u32 source_b = ctx.gpr[7];
    const u32 distance_out = ctx.gpr[8];
    const u32 vector_out = ctx.gpr[9];
    const u32 other_out = ctx.gpr[10];
    SavedRegisters saved(ctx);
    put(rt, frame.at(0xA4u), target_object);
    put(rt, frame.at(0xA8u), distance_out);
    put(rt, frame.at(0xACu), vector_out);
    put(rt, frame.at(0xB0u), other_out);
    const u32 pairs = object + 0x1C0u;
    const auto candidates = [&] { return (word(rt, object + 0x1FCu) >> 25) & 0x1Fu; };
    float best = std::bit_cast<float>(0x7F7FFFFFu);  // f20
    float best_other = 0.0f;                          // f21
    put(rt, frame.at(0xA0u), 0xFFFFFFFFu);            // the chosen index
    if (candidates() != 0u) {
        const u32 first_vec = frame.at(0x48u);  // s6
        const u32 scratch = frame.at(0x78u);    // s3
        const u32 measured = frame.at(0x6Cu);   // s4
        u32 entry = pairs;
        u32 index = 0u;
        for (;;) {
            const u32 point_a = call(rt, ctx, 0x08C45D8Cu, {object + (byte_at(rt, entry) << 4)});
            const u32 point_b = call(rt, ctx, 0x08C45D8Cu, {object + (byte_at(rt, entry + 1u) << 4)});
            const Pair sums = call_pair(rt, ctx, 0x08BAC270u, {point_b, point_a});
            put(rt, frame.at(0x34u), sums.v0);
            put(rt, frame.at(0x38u), sums.v1);
            put(rt, frame.at(0x90u), sums.v0);
            put(rt, frame.at(0x94u), sums.v1);
            call(rt, ctx, 0x08BAC428u, {first_vec, sums.v0, sums.v1});
            call(rt, ctx, 0x08B03448u, {point_a, first_vec, source_a, source_b, frame.at(0x04u), frame.at(0x08u)});
            const Pair part_a = call_pair(rt, ctx, 0x08C45EA8u, {first_vec}, {float_at(rt, frame.at(0x04u))});
            put(rt, frame.at(0x24u), part_a.v0);
            put(rt, frame.at(0x28u), part_a.v1);
            put(rt, frame.at(0x90u), part_a.v0);
            put(rt, frame.at(0x94u), part_a.v1);
            const Pair shape_a = call_pair(rt, ctx, 0x08C498A0u, {point_a, frame.at(0x24u)});
            put(rt, frame.at(0x30u), shape_a.v1);
            put(rt, frame.at(0x2Cu), shape_a.v0);
            put(rt, frame.at(0x90u), shape_a.v0);
            put(rt, frame.at(0x94u), shape_a.v1);
            call(rt, ctx, 0x08BBBECCu, {scratch, word(rt, shape_a.v1), shape_a.v1 + 4u});
            call(rt, ctx, 0x08BAE2FCu, {frame.at(0x54u), shape_a.v0, scratch});
            const Pair part_b = call_pair(rt, ctx, 0x08C45EA8u, {source_b}, {float_at(rt, frame.at(0x08u))});
            put(rt, frame.at(0x14u), part_b.v0);
            put(rt, frame.at(0x18u), part_b.v1);
            put(rt, frame.at(0x90u), part_b.v0);
            put(rt, frame.at(0x94u), part_b.v1);
            const Pair shape_b = call_pair(rt, ctx, 0x08C498A0u, {source_a, frame.at(0x14u)});
            put(rt, frame.at(0x20u), shape_b.v1);
            put(rt, frame.at(0x1Cu), shape_b.v0);
            put(rt, frame.at(0x90u), shape_b.v0);
            put(rt, frame.at(0x94u), shape_b.v1);
            call(rt, ctx, 0x08BBBECCu, {scratch, word(rt, shape_b.v1), shape_b.v1 + 4u});
            call(rt, ctx, 0x08BAE2FCu, {frame.at(0x60u), shape_b.v0, scratch});
            const Pair joined = call_pair(rt, ctx, 0x08BAC270u, {frame.at(0x54u), frame.at(0x60u)});
            put(rt, frame.at(0x0Cu), joined.v0);
            put(rt, frame.at(0x10u), joined.v1);
            put(rt, frame.at(0x90u), joined.v0);
            put(rt, frame.at(0x94u), joined.v1);
            call(rt, ctx, 0x08BAC428u, {measured, joined.v0, joined.v1});
            const float near = call_float(rt, ctx, 0x08BBBEC4u, {target_object, measured}, {});
            if (!(near <= 0.0f)) {
                const float candidate = call_float(rt, ctx, 0x08BBBEC4u, {measured, measured}, {});
                if (candidate < best) {
                    put(rt, frame.at(0x3Cu), word(rt, frame.at(0x6Cu)));
                    put(rt, frame.at(0x40u), word(rt, frame.at(0x70u)));
                    put(rt, frame.at(0x44u), word(rt, frame.at(0x74u)));
                    best = candidate;
                    best_other = float_at(rt, frame.at(0x08u));
                    put(rt, frame.at(0xA0u), index);
                }
            }
            index += 1u;
            entry += 2u;
            if (index >= candidates()) break;
        }
    }
    const float root = std::sqrt(best);  // the original takes it in the first output test's delay slot
    if (distance_out != 0u) put_float(rt, distance_out, root);
    if (vector_out != 0u) {
        const u32 built = call(rt, ctx, 0x08BBBE8Cu, {frame.at(0x3Cu)});
        put(rt, frame.at(0x00u), built);
        call(rt, ctx, 0x08BBBEF8u, {vector_out, built});
    }
    if (other_out != 0u) put_float(rt, other_out, best_other);
    ctx.gpr[2] = word(rt, frame.at(0xA0u));
}

// 0x08C53264 (a0 = a 3x3 matrix in nine words, a1 = the three-float answer, a2 = the three-float right-hand side):
// solves the system on a frame copy — pivots by swapping the matrix's columns and the vector's entries (the pivot is the
// largest magnitude found in the step's column), eliminates below it with 0x08B4D650, then substitutes back into the
// answer. v0 = 1 when it solved, 0 when a pivot was exactly zero.
} // namespace
} // namespace new_batch_main_231
} // namespace gt::game

#define NEW231(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_231::name, gt::ReturnKind::kind)
NEW231(0x08B036F8u, func_08B036F8, Word);

// ---- from main-session batch 232 (draft c21da963d682a7c35da4143d85520d02949a2cd0) ----
// New batch (main-session batch 232): functions written by hand from `tools/disasm` as structured C.
// Draft only — not through the differential gate yet. No names: none has evidence.
//
// Two of the functions (0x0886473C, 0x08A79DD4) start from the drafts of new_batch_main_26.cpp and are
// re-read against the asm here; the rest are new. All seven are refcounted-container or table-walk
// shapes: the intrusive holder helper (retain word +4, shared count +8, virtuals +8 / +0xC) recurs.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_232 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them: several shapes here read the callee's v1
// (the high word of a 64-bit result) after the call.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments ($f12 upwards) and the callee's $f0 as the result.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_floats(rt, ctx, target, arguments, floats).f0;
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
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The intrusive holder the containers here keep: a retain counter at +4 and a shared counter at +8; the
// virtuals at +8 and +0xC of the object run when each counter falls to zero (0886473C, 0889E6E8).
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
void assign_holder(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    if (value == word(rt, slot)) return;
    if (value != 0u) put(rt, value + 4u, word(rt, value + 4u) + 1u);
    release_holder(rt, ctx, word(rt, slot));
    put(rt, slot, value);
}

// 0x0886473C (a0 = the owner object): destroys the range container at +0x4490 / +0x44A0. The three words
// at +0x44A0 are moved into a stack temporary and cleared, the range is destroyed (0x08B8FE44) and its
// first holder released, 0x08B90024 clears what is left; the temporary's words +0x1C / +0x20 / +0x24 /
// +0x28 go to +0x44AC / +0x44B0 (holder) / +0x44B4 / +0x44B8 (holder). Then the two temporary holders
// are released, the range is destroyed a second time and the sub-object at +0x4490 is destroyed.

// 0x08A79DD4 (a0 = map holder with its tree at +0x10, a1 = the value holder, a2 = the key, a3 = a pointer
// to the object the key maps to): an existing key copies the node's object into the holder — through the
// object's virtual at +0x60 when its +0x28 check answers, else by a plain holder assignment — and resets
// the holder to empty otherwise; a missing key inserts the value the object supplies at +0xCC / +0x60 once
// the holder is empty. Only an empty holder (word 0) reaches the insert. v0 = word(holder) until then.

// 0x0889E6E8 and 0x0889E9F4 (a0 = object): the two destructor stages — the body is byte-identical, only
// an internal jump target differs. Stores the stage vtable +0 at the object, destroys the pair at +0x30 /
// +0x34, the sub-object at +0x28, then the record at +0x24: with its word +0x2C non-zero it only counts
// down (destroying the pair +0x34 / +0x38 and calling 0x08B34C1C when that count reaches zero), otherwise
// it destroys the pair +0x34 / +0x38, 0x08B28BE0 and 0x08B64654 run; then the four holders at +0x20, +0x18,
// +0x10 and +8 are released and the base vtable 0x08CC24F0 is stored. v0 = 0x08CC24F0.
void destroy_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC2420u);
    const u32 holder = word(rt, self + 0x34u);
    if (holder != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, holder)), {holder, word(rt, self + 0x30u)});
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x30u, 0u);
    }
    call(rt, ctx, 0x0889A4F0u, {self + 0x28u});
    const u32 record = word(rt, self + 0x24u);
    call(rt, ctx, 0x08B34888u, {record});
    if (word(rt, record + 0x2Cu) != 0u) {
        put(rt, record + 0x2Cu, word(rt, record + 0x2Cu) - 1u);
        u32 first = 0u;
        u32 second = 0u;
        if (word(rt, record + 0x2Cu) == 0u) {
            const u32 pair = word(rt, record + 0x34u);
            if (pair != 0u) {
                second = word(rt, record + 0x38u);
                first = pair;
                put(rt, record + 0x34u, 0u);
                put(rt, record + 0x38u, 0u);
            }
            if (byte_at(rt, record + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {record});
        }
        call(rt, ctx, 0x08B34B24u, {record});
        if (first != 0u) call_indirect(rt, ctx, word(rt, word(rt, first) + 8u), {first});
        if (second != 0u) call_indirect(rt, ctx, word(rt, word(rt, second)), {second, first});
    } else {
        call(rt, ctx, 0x08B34B24u, {record});
        const u32 pair = word(rt, record + 0x38u);
        if (pair != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, pair)), {pair, word(rt, record + 0x34u)});
            put(rt, record + 0x38u, 0u);
            put(rt, record + 0x34u, 0u);
        }
        call(rt, ctx, 0x08B28BE0u, {record});
        call(rt, ctx, 0x08B64654u, {record});
    }
    release_holder(rt, ctx, word(rt, self + 0x20u));
    release_holder(rt, ctx, word(rt, self + 0x18u));
    release_holder(rt, ctx, word(rt, self + 0x10u));
    release_holder(rt, ctx, word(rt, self + 0x08u));
    put(rt, self, 0x08CC24F0u);
    ctx.gpr[2] = 0x08CC24F0u;
}

// 0x08A2FB84 (a0 = the output holder, a1 = unused, a2 = the argument count, a3 = a four-pointer record):
// with fewer than two arguments it answers 1. The record's first object's slot 0x50 gives the pair
// (v0, v1) the manager call takes as a2 / a3, its +8 object's slot 0x3C gives t2 (with three arguments)
// and its +0xC object's slot 0x30 gives t3 (with four, else 1). The record's +4 object is looked up twice
// (0x08A443E0, then 0x08A6BF34) and 0x08A76F20 decides between them:
//   - hit on the first: 0x08A449B8 fills a temporary from the record at +4; the manager 0x08B30660 runs
//     (t1 = its word +0x18, t0 = its word +0x14, two zero stack arguments), its result goes through
//     0x08A97A6C into a second temporary that is assigned to the output holder, and both temporaries are
//     released;
//   - hit on the second: the object's virtual at +0xC fills the temporary, the manager runs (t0 = the
//     temporary's word, t1 = word(t0 - 0xC)), the result is assigned as above, and the temporary's pointer
//     is released through the intrusive header at -0xC / -4 (0x08B60804, then 0x08B8542C at its frame+0x10);
//   - neither: 0x08A97A6C's empty value is assigned and released.
void func_08A2FB84(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 record = ctx.gpr[7];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[2] = static_cast<s32>(count) < 2 ? 1u : 0u;
    if (static_cast<s32>(count) < 2) return;
    const auto require = [&](u32 object) {
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
    };
    u32 object = word(rt, record);
    require(object);
    const GuestResult first = call_keep(rt, ctx, word(rt, word(rt, object) + 0x50u), {object});
    const u32 arg_a2 = first.v0;
    const u32 arg_a3 = first.v1;
    u32 arg_t2 = 0u;
    if (static_cast<s32>(count) >= 3) {
        const u32 object_8 = word(rt, record + 8u);
        require(object_8);
        arg_t2 = call_indirect(rt, ctx, word(rt, word(rt, object_8) + 0x3Cu), {object_8});
    }
    u32 arg_t3 = 1u;
    if (static_cast<s32>(count) >= 4) {
        const u32 object_c = word(rt, record + 0xCu);
        require(object_c);
        arg_t3 = call_indirect(rt, ctx, word(rt, word(rt, object_c) + 0x30u), {object_c});
    }
    const u32 lookup = word(rt, record + 4u);
    require(lookup);
    const auto manager = [&](u32 arg_a0, u32 arg_t0, u32 arg_t1) {
        put(rt, frame.at(0x00u), 0u);
        put(rt, frame.at(0x04u), 0u);
        // a1 (the argument beside a0) is never read by 0x08B30660: it zeroes it at entry.
        return call(rt, ctx, 0x08B30660u, {arg_a0, 0u, arg_a2, arg_a3, arg_t0, arg_t1, arg_t2, arg_t3});
    };
    if (call(rt, ctx, 0x08A76F20u, {lookup, call(rt, ctx, 0x08A443E0u)}) != 0u) {
        const u32 temporary = frame.at(0x20u);
        call(rt, ctx, 0x08A449B8u, {temporary, record + 4u});
        const u32 arg_a0 = call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
        u32 item = word(rt, temporary);
        require(item);
        item = word(rt, temporary);
        const u32 link = word(rt, item + 0x14u);
        require(item);
        const u32 made = manager(arg_a0, link, word(rt, item + 0x18u));
        const u32 result = frame.at(0x1Cu);
        call(rt, ctx, 0x08A97A6Cu, {result, made});
        if (self != result) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, result)});
        call(rt, ctx, 0x08A5B9ECu, {result});
        call(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    const u32 lookup_again = word(rt, record + 4u);
    require(lookup_again);
    if (call(rt, ctx, 0x08A76F20u, {lookup_again, call(rt, ctx, 0x08A6BF34u)}) != 0u) {
        const u32 object_4 = word(rt, record + 4u);
        require(object_4);
        const u32 temporary = frame.at(0x20u);
        call_indirect(rt, ctx, word(rt, word(rt, object_4) + 0x0Cu), {temporary, object_4});
        const u32 arg_a0 = call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
        const u32 item = word(rt, temporary);
        const u32 made = manager(arg_a0, item, word(rt, item - 0x0Cu));
        const u32 result = frame.at(0x18u);
        call(rt, ctx, 0x08A97A6Cu, {result, made});
        if (self != result) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, result)});
        call(rt, ctx, 0x08A5B9ECu, {result});
        const u32 holder = word(rt, temporary);
        const u32 base = holder - 0x0Cu;
        if (base != 0x08D74FD0u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu})) <= 0) {
                call(rt, ctx, 0x08B8542Cu, {base, frame.at(0x10u)});
            }
        }
        return;
    }
    const u32 empty = frame.at(0x14u);
    call(rt, ctx, 0x08A97A6Cu, {empty, 0u});
    if (self != empty) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, empty)});
    call(rt, ctx, 0x08A5B9ECu, {empty});
}

// 0x08B43B3C (a0 = the owner object, a2 / a3 = the low / high word of a 64-bit key): walks the bucket
// table at +0x454 (+0x400: one flag byte per bucket, +0x480: the bucket count) for the first bucket whose
// flag byte is set, then the singly linked list hanging off that bucket (next at +8, item at +0). An item
// counts when 0x08B3E6E8 accepts it, its word +0x10 is set and the distance between the key and the item's
// 64-bit pair at +0x20 / +0x24 (absolute value, unsigned) is at least that word; the object's virtual at
// +0x90 must then answer the item's signed half at +0x16. The item stores the key at +0x20 / +0x24 and, when
// 0x08B3E6BC accepts it, 0x08B3E65C runs, the key goes to +0x18 / +0x1C and a request record is built at sp
// (word +0x14 = 0x4E29, +0x8 / +0xC = the first call's v0 / v1, then 0x08B47AD4 with word(item+0xC)+0x18 and
// 0x08B47B40); 0x08B79A88 fills the record's +0x18 from the item's +0x58. With the item's bytes +0x14 and
// +0x15 both clear the object's list at +0x8DC is walked (0x08B42618 per entry whose half differs from the
// object's virtual +0x90 and whose byte +5 is set); otherwise 0x08B42A90 takes the request with the half at
// sp+0. Both end with 0x08B47ACC(sp).

// 0x088AECC8 (a0 = the vehicle state): one steering/attitude step, all floats. The frame builds a point
// pair at sp (0x08B67C24 with the float at +0x14 gives sp+0 / sp+4), a zero point at sp+0x28 from the
// object selected by the index at +0x3DC (records of 0x28 bytes from +0x280, the object at +0x28 of the
// record holds two floats pitted against the object's +0x8 / +0xC), 0x08B4AD90 builds the pair at sp+0x18
// from sp+0x48 and the record's +0x14; the heading pair at sp+0x20 is the +0x3C / +0x40 pair plus that
// pair scaled by the float at +0x48. 0x088AD680 and 0x088ACA78 integrate (sp+0x10 / sp+0x20 / sp+0x30), the
// cross product of sp+0x28 with sp+0x10 over +0x430 updates the float at +0x48, the merged pair at sp+0x8
// is scaled by 1/+0x42C and added into the +0x3C pair, and the heading at +0x44 grows by (sp+0x38)/+0x42C.
// The +0x44 clamp only fires for an unordered / negative value; when the heading is positive and the +0x3C
// pair is longer than 5.0 the heading is negated. With the word at +0x3FC set, +0x400..+0x418 are filled
// (0x089552E8's +0xC float times 0.3 into +0x404) and +0x43F is set. v0 is not a result: $f0 = sqrt of the
// squared length of sp+0x10.
void func_088AECC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x90u);
    const float one = 1.0f;
    call_floats(rt, ctx, 0x08B67C24u, {frame.at(0x00u), frame.at(0x04u)}, {float_at(rt, self + 0x14u)});
    const float sine = float_at(rt, frame.at(0x00u));
    const float cosine = float_at(rt, frame.at(0x04u));
    const u32 block = self + 0x280u + word(rt, self + 0x3DCu) * 0x28u;
    put_float(rt, frame.at(0x50u), -sine);
    put_float(rt, frame.at(0x4Cu), sine);
    put_float(rt, frame.at(0x54u), cosine);
    put_float(rt, frame.at(0x48u), cosine);
    const u32 link = word(rt, block + 0x28u);
    put_float(rt, frame.at(0x28u), float_at(rt, link) - float_at(rt, self + 0x08u));
    put_float(rt, frame.at(0x2Cu), -float_at(rt, link + 0x08u) - float_at(rt, self + 0x0Cu));
    call(rt, ctx, 0x08B4AD90u, {frame.at(0x18u), frame.at(0x48u), block + 0x14u});
    const float speed = float_at(rt, self + 0x48u);
    const float along = psp_mul(speed, float_at(rt, frame.at(0x28u)));
    const float across = psp_mul(speed, float_at(rt, frame.at(0x2Cu)));
    put_float(rt, frame.at(0x24u), float_at(rt, self + 0x40u) + along);
    put_float(rt, frame.at(0x20u), float_at(rt, self + 0x3Cu) - across);
    call(rt, ctx, 0x088AD680u, {self, frame.at(0x10u), frame.at(0x20u), frame.at(0x18u), frame.at(0x28u)});
    call(rt, ctx, 0x088ACA78u, {self, frame.at(0x10u), frame.at(0x20u), frame.at(0x30u)});
    float travelled = float_at(rt, self + 0x48u);
    const float cross = call_float(rt, ctx, 0x08B4ABECu, {frame.at(0x28u), frame.at(0x10u)}, {});
    const float inverse_side = one / float_at(rt, self + 0x430u);
    const float inverse_turn = one / float_at(rt, self + 0x42Cu);
    const float merged_x = float_at(rt, frame.at(0x14u)) + float_at(rt, frame.at(0x34u));
    const float merged_y = float_at(rt, frame.at(0x10u)) + float_at(rt, frame.at(0x30u));
    put_float(rt, frame.at(0x0Cu), merged_x);
    put_float(rt, frame.at(0x08u), merged_y);
    put_float(rt, frame.at(0x10u), merged_y);
    put_float(rt, frame.at(0x14u), merged_x);
    travelled = travelled + psp_mul(cross, inverse_side);
    put_float(rt, self + 0x48u, travelled);
    call_floats(rt, ctx, 0x08B4A560u, {frame.at(0x08u)}, {inverse_turn});
    call(rt, ctx, 0x08B4AC0Cu, {self + 0x3Cu, self + 0x3Cu, frame.at(0x08u)});
    const float turn_rate = one / float_at(rt, self + 0x42Cu);
    const float heading = float_at(rt, self + 0x44u) + psp_mul(float_at(rt, frame.at(0x38u)), turn_rate);
    put_float(rt, self + 0x44u, heading);
    float clamped = heading;
    if (0.0f <= heading) {
        if (heading < 0.0f) {
            clamped = 0.0f;
            put_float(rt, self + 0x44u, 0.0f);
        }
    } else if (0.0f < heading) {
        clamped = 0.0f;
        put_float(rt, self + 0x44u, 0.0f);
    }
    if (0.0f < clamped) {
        if (5.0f < std::sqrt(call_float(rt, ctx, 0x08B4A708u, {self + 0x3Cu}, {}))) {
            put_float(rt, self + 0x44u, -float_at(rt, self + 0x44u));
        }
    }
    if (word(rt, self + 0x3FCu) != 0u) {
        put_float(rt, self + 0x40Cu, -float_at(rt, frame.at(0x18u)));
        put_float(rt, self + 0x414u, float_at(rt, frame.at(0x1Cu)));
        put(rt, self + 0x410u, 0u);
        const float squared = call_float(rt, ctx, 0x08B4ABCCu, {frame.at(0x10u), frame.at(0x10u)}, {});
        put_float(rt, self + 0x418u, squared);
        put_float(rt, self + 0x400u, float_at(rt, frame.at(0x28u)) + float_at(rt, self + 0x1Cu));
        float drift = float_at(rt, self + 0x24u);
        put_float(rt, frame.at(0x44u), -float_at(rt, frame.at(0x2Cu)));
        put_float(rt, frame.at(0x3Cu), float_at(rt, frame.at(0x28u)));
        put_float(rt, frame.at(0x40u), 0.0f);
        const float rate = call_float(rt, ctx, 0x089552E8u, {word(rt, self + 0x3FCu)}, {});
        drift = drift + psp_mul(rate, 0.3f);
        put_float(rt, self + 0x404u, drift);
        put_float(rt, self + 0x408u, float_at(rt, frame.at(0x44u)) - float_at(rt, self + 0x20u));
        put_byte(rt, self + 0x43Fu, 1u);
    }
    ctx.fpr[0] = std::sqrt(call_float(rt, ctx, 0x08B4A708u, {frame.at(0x10u)}, {}));
}

} // namespace
} // namespace new_batch_main_232
} // namespace gt::game

#define NEW232(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_232::name, gt::ReturnKind::kind)
NEW232(0x08A2FB84u, func_08A2FB84, Word);
NEW232(0x088AECC8u, func_088AECC8, Float);

// ---- from main-session batch 233 (draft 1704b4361165b21509f23b6e47f349b7676fd91a) ----
// New batch main-233 (main session draft, unverified): functions the gate could not reach with useful inputs.
// Written by hand from `tools.disasm` as structured C; every call is a guest call, and v0 is what the asm leaves in it
// on every exit. Skipped, with the reason:
//   0x08B5F7EC, 0x08B5FE68: streambuf scan loops (an underflow at three places in the middle of the loop, the state
//     machine entered from four different points) — not written as structured C without the risk of a silent branch
//     difference the gate would only find once a route reaches them.
//   0x08BB404C: allocator plus red-black-tree insert (bnel chains, rotations) — unclear.
//   0x08A8CE2C: STL container assign/splice with iterator proxies and shared-null sentinels — unclear.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_233 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u16 half(Runtime &rt, u32 address) { return rt.memory().load16(address); }

// 0x08B37B34 (a0 = counter): v0 = the word at +0x10, which is then bumped.
u32 next_index(Runtime &rt, AllegrexContext &ctx, u32 counter) {
    const u32 value = word(rt, counter + 0x10u);
    put(rt, counter + 0x10u, value + 1u);
    return value;
}

// The temporary 0x08B380C4 stores: operator new(0xC), vtable 0x08CD93B0 at +0, a refcount of 1 at +4 and two halfword
// tags at +8 / +0xA.
u32 make_object(Runtime &rt, AllegrexContext &ctx, u16 kind, u16 tag) {
    const u32 object = call(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, object, 0x08CD93B0u);
    put(rt, object + 4u, 0u);
    rt.memory().store16(object + 8u, kind);
    rt.memory().store16(object + 0xAu, tag);
    put(rt, object + 4u, word(rt, object + 4u) + 1u);
    return object;
}

// The release of one such temporary: the count at +4 drops and the destructor in vtable slot +4 runs when it reaches 0.
// v0 afterwards is the caller's value for a null object, the new count, or the destructor's result — as the asm leaves
// it (case 4 returns it).
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 count = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, count);
    ctx.gpr[2] = count;
    if (count != 0u) return;
    const u32 destructor = word(rt, word(rt, object) + 4u);
    ctx.gpr[2] = call_indirect(rt, ctx, destructor, {object});
}

// 0x08B38160 (a0 = walker, a1 = node, a2 = value, a3 = extra): dispatches on the halfword at +8 of the object the
// node's +4 points to (a jump table of six cases, everything else returns 0) and walks the node's +0x14 / +0x1C
// children through 0x08B380C4, which stores an indexed entry built from the temporary pairs at sp+0 / sp+8.
void func_08B38160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    const u32 node = word(rt, ctx.gpr[5] + 4u);
    switch (half(rt, node + 8u)) {
    case 0:  // 0x08B381C8
        ctx.gpr[2] = call(rt, ctx, 0x08B380C4u, {self, value, extra, node + 0xCu});
        return;
    case 1: {  // 0x08B38400: the counter once, then both children with it
        const u32 index = next_index(rt, ctx, self);
        call(rt, ctx, 0x08B38160u, {self, node + 0x14u, value, index});
        ctx.gpr[2] = call(rt, ctx, 0x08B38160u, {self, node + 0x1Cu, index, extra});
        return;
    }
    case 2:  // 0x08B38204: both children with the arguments unchanged
        call(rt, ctx, 0x08B38160u, {self, node + 0x14u, value, extra});
        ctx.gpr[2] = call(rt, ctx, 0x08B38160u, {self, node + 0x1Cu, value, extra});
        return;
    case 3: {  // 0x08B38230: two counters, an entry each, and the +0x14 child between them
        const u32 first = next_index(rt, ctx, self);
        const u32 second = next_index(rt, ctx, self);
        call(rt, ctx, 0x08B380C4u, {self, value, first, 0x08D694D4u});
        call(rt, ctx, 0x08B38160u, {self, node + 0x14u, first, second});
        call(rt, ctx, 0x08B380C4u, {self, second, first, 0x08D694D4u});
        ctx.gpr[2] = call(rt, ctx, 0x08B380C4u, {self, first, extra, 0x08D694D4u});
        return;
    }
    case 4: {  // 0x08B382A8: two temporaries (kinds 2 and 3, the node's +0xA tag) released after their entries
        const u16 tag = half(rt, node + 0xAu);
        const u32 first = next_index(rt, ctx, self);
        const u32 second = next_index(rt, ctx, self);
        put(rt, frame.at(8u), 0x08CDA8E8u);
        put(rt, frame.at(0xCu), make_object(rt, ctx, 2u, tag));
        call(rt, ctx, 0x08B380C4u, {self, value, first, frame.at(8u)});
        release_object(rt, ctx, word(rt, frame.at(0xCu)));
        call(rt, ctx, 0x08B38160u, {self, node + 0x14u, first, second});
        put(rt, frame.at(0u), 0x08CDA8E8u);
        put(rt, frame.at(4u), make_object(rt, ctx, 3u, tag));
        ctx.gpr[2] = call(rt, ctx, 0x08B380C4u, {self, second, extra, frame.at(0u)});
        release_object(rt, ctx, word(rt, frame.at(4u)));
        return;
    }
    case 5:  // 0x08B381E4
        ctx.gpr[2] = call(rt, ctx, 0x08B380C4u, {self, value, extra, 0x08D694D4u});
        return;
    default:  // the sltiu test's zero
        ctx.gpr[2] = 0u;
        return;
    }
}

// 0x08B36018 (a0 = destination, a1 = byte, a2 = count): the library's fill.
void fill_bytes(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 byte, u32 count) {
    call(rt, ctx, 0x08B36018u, {destination, byte, count});
}
// 0x08B7C830 (a0 = destination, a1 = source, a2 = count): copies up to `count` bytes, stopping after a zero byte.
void copy_string(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source, u32 count) {
    call(rt, ctx, 0x08B7C830u, {destination, source, count});
}

// 0x08A1D07C (a0 = object, a1 = mode, a2 = source words, a3 = record, t0 = value): constructs the base at a0+8, copies
// six blocks out of the source struct and fills the record words into the object. Mode 8 registers the two stack
// temporaries (sp+0x14 / sp+0x44 / sp+0x28) in the object at a0+0x5D0..+0x5D8 instead of copying into it and then
// measures the run, storing the size at a0+0x608; every other mode just runs the iteration.

// The COW-string release of the 0x08D74FD0 empty-string family: the handle's block is at -0xC and its refcount at -4;
// when the count reaches zero the block is freed (the asm's 0x08B60804 returns the count, 0x08B8542C frees).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 handle, u32 free_argument) {
    if (handle - 0xCu == 0x08D74FD0u) return;
    const u32 count = call(rt, ctx, 0x08B60804u, {handle - 4u, 0xFFFFFFFFu});
    if (static_cast<std::int32_t>(count) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {handle - 0xCu, free_argument});
}

// 0x08A1E0BC (a0 = object, a1 = output, a2 = source, a3 = mode, t0 = list): builds a formatted string out of the list's
// two entries and, in mode 3, out of the 20-byte records of the collection at list+8, then hands it to 0x08A1D398.
// The short path (mode < 2) passes no record array and no output struct at all.

} // namespace
} // namespace new_batch_main_233
} // namespace gt::game

#define NEW233(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_233::name, gt::ReturnKind::kind)
NEW233(0x08B38160u, func_08B38160, Word);

// ---- from main-session batch 234 (draft e8344e3bdb7bb316fe3a12c6e0b7259a6d78ecea) ----
// New batch 234 (main session): functions the recompiler emits that no src/ file defines or registers. Written by hand
// from `tools.disasm` (and its `[LO]` annotations, which carry the address the ELF relocation builds) as structured C.
// This is a draft batch, not yet verified by the differential gate.
//
// The three shapes here:
//  - 0x08C4DF20: a type-token chain that converts one property of a descriptor into a 64-bit value.
//  - 0x08B5B78C: the module initializer that installs five objects and their tables (straight-line stores + calls).
//  - 0x08B6A844: the two-argument libm routine behind func_08B66FBC (the SVID wrapper): pi/2 and pi constants,
//    quadrant switches on the two sign bits, and calls to the already decompiled fabsf (0x08B663A4) / atanf (0x08B65F0C).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_234 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call whose 64-bit (v0/v1) result the original stores; the argument registers are set like `call`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same for a callee that reads float arguments ($f12, ...).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// The callee-saved registers a function body overwrites: the original restores them from its frame before it
// returns, so the state the caller sees must be the entry one.
struct Saved {
    AllegrexContext &ctx;
    u32 saved[7];  // s0-s6
    explicit Saved(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 7u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~Saved() {
        for (unsigned i = 0; i < 7u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// --- 0x08C4DF20: the descriptor's type token decides the conversion -------------------------------------------------
//
// The descriptor (a0) is { word +0 type token, word +4 payload }. Each engine type has a token accessor (the
// 0x08B9DDB0 family: a lazily built static type object); the first token that matches the descriptor's word selects a
// conversion of the payload into the 64-bit value stored at a1 +0/+4, and the function answers 1. A payload word of 0
// skips its case; a descriptor that matches no token answers 0 and writes nothing.
void func_08C4DF20(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(ctx);
    const u32 descriptor = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    ctx.gpr[17] = descriptor;
    ctx.gpr[18] = out;
    // Every path that converts ends with a0 = 1 and `v0 = a0`; the no-match path ends with v0 = a0 = 0.
    const auto store_pair = [&](const GuestResult &value) {
        put(rt, out, value.v0);
        put(rt, out + 4u, value.v1);
    };
    const auto matched_pair = [&](const GuestResult &value) {
        store_pair(value);
        ctx.gpr[4] = 1u;
        ctx.gpr[2] = 1u;
    };
    // The `lb/lh/lw s0, 0(payload)` + convert shape: a negative source is biased (the add is 0x08B71328 with the
    // source as one 64-bit operand and the bias as the other), a non-negative one is stored as converted.
    const auto convert_signed = [&](s32 source, u32 bias) {
        const GuestResult converted = call_keep(rt, ctx, 0x08B71CE0u, {static_cast<u32>(source)});
        if (source < 0) {
            matched_pair(call_keep(rt, ctx, 0x08B71328u, {converted.v0, converted.v1, 0u, bias}));
        } else {
            matched_pair(converted);
        }
    };
    const auto convert_plain = [&](s32 source) { matched_pair(call_keep(rt, ctx, 0x08B71CE0u, {static_cast<u32>(source)})); };

    // The token is reloaded into s0 before every probe (it sits in each call's delay slot).
    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08B9DDB0u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            // A float payload: f12 = *(float*)payload, double result.
            ctx.fpr[12] = float_at(rt, payload);
            matched_pair(call_keepf(rt, ctx, 0x08B70F28u, {}, {ctx.fpr[12]}));
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08C4D18Cu)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            // The payload is the value itself: its two words, high word first.
            const u32 high = word(rt, payload + 4u);
            const u32 low = word(rt, payload);
            put(rt, out + 4u, high);
            put(rt, out, low);
            ctx.gpr[4] = 1u;
            ctx.gpr[2] = 1u;
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC473Cu)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_plain(signed_byte(rt, payload));
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC48B4u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_signed(signed_byte(rt, payload), 0x40700000u);
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC45C4u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_plain(signed_half(rt, payload));
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC444Cu)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_signed(signed_half(rt, payload), 0x40F00000u);
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC42D4u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_plain(static_cast<s32>(word(rt, payload)));
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08B9E8D4u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_signed(static_cast<s32>(word(rt, payload)), 0x41F00000u);
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC415Cu)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            // The payload's two words go straight to the 64-bit conversion.
            const u32 high = word(rt, payload + 4u);
            const u32 low = word(rt, payload);
            matched_pair(call_keep(rt, ctx, 0x08B6EE80u, {low, high}));
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08BC3FE4u)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            const u32 low = word(rt, payload);
            const u32 high = word(rt, payload + 4u);
            // func_08B6EE4C compares the 64-bit value with (0, 0): 0 less, 1 equal, 2 greater.
            if (static_cast<s32>(call_keep(rt, ctx, 0x08B6EE4Cu, {low, high, 0u, 0u}).v0) > 0) {
                matched_pair(call_keep(rt, ctx, 0x08B6EE80u, {low, high}));
            } else {
                // A quarter of the value (shifted right by one, the low bit folded into the top of the low word).
                const u32 halves_low = (low >> 1) | (high << 31);
                const u32 halves_high = high >> 1;
                const u32 folded = (low & 1u) | halves_low;
                const GuestResult halved = call_keep(rt, ctx, 0x08B6EE80u, {folded, halves_high});
                matched_pair(call_keep(rt, ctx, 0x08B71328u, {halved.v0, halved.v1, halved.v0, halved.v1}));
            }
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] == call(rt, ctx, 0x08B9CEBCu)) {
        const u32 payload = word(rt, descriptor + 4u);
        if (payload != 0u) {
            convert_signed(signed_byte(rt, payload), 0x40700000u);
            return;
        }
    }

    ctx.gpr[16] = word(rt, descriptor);
    if (ctx.gpr[16] != call(rt, ctx, 0x08BC4A2Cu)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 payload = word(rt, descriptor + 4u);
    if (payload == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    convert_plain(signed_byte(rt, payload));
}

// --- 0x08B5B78C: the module initializer ----------------------------------------------------------------------------
//
// The guard call counts one; when it was already done the function returns its non-zero answer. Otherwise it installs
// five objects in the runtime's global block: each gets its constructor called, a vtable word, six zero words, two
// members read out of the runtime's tables (0x08B796C4 answers them), and a state block cleared at +0x70. Four of them
// get their view table (0x08C54A7C) and their table word (0x08D72A70, 0x08D72B04, 0x08D729E4). Ends in a tail call of
// the same guard routine that started it (0x08B6085C -> 0x08B60804).
void func_08B5B78C(Runtime &rt, AllegrexContext &ctx) {
    const u32 module = word(rt, 0x08CB625Cu);
    {
        GuestFrame frame(ctx, 0x30u);
        if (call(rt, ctx, 0x08B60804u, {module, 1u}) != 0u) return;

        rt.memory().store8(0x08CE0E0Cu, 1u);  // the "initializing" flag (the first table read's delay slot)
        const u32 table_a = call(rt, ctx, 0x08B796C4u);
        const u32 member_a = word(rt, table_a + 8u);

        // The three objects whose constructor is 0x08B5E0EC and that get two member words at +0x20/+0x24.
        const u32 member_object_a = 0x08D72930u;  // constructor argument 0x08D7294C
        const u32 member_object_b = 0x08D72908u;  // constructor argument 0x08D72924
        const u32 member_object_c = 0x08D728E0u;  // constructor argument 0x08D728FC
        // The four objects whose constructor is 0x08B65660 and that get a view table (0x08C54A7C) and a zero block.
        const u32 view_object_a = 0x08D72A74u;
        const u32 view_object_b = 0x08D72B04u;
        const u32 view_object_c = 0x08D729E8u;
        const u32 view_object_d = 0x08D7295Cu;

        // 0x08D72930: six zero words, vtable 0x08CDBDF0 while the constructor runs, then the members and 0x08CDBD28.
        for (u32 at = 4u; at <= 0x18u; at += 4u) put(rt, member_object_a + at, 0u);
        put(rt, member_object_a, 0x08CDBDF0u);
        call(rt, ctx, 0x08B5E0ECu, {0x08D7294Cu});
        put(rt, member_object_a + 0x20u, member_a);
        put(rt, member_object_a + 0x24u, 0xFFFFFFFFu);
        put(rt, member_object_a, 0x08CDBD28u);
        const u32 table_b = call(rt, ctx, 0x08B796C4u);
        const u32 member_b = word(rt, table_b + 4u);

        // 0x08D72908, the same install with 0x08D72924 as its constructor argument.
        for (u32 at = 4u; at <= 0x18u; at += 4u) put(rt, member_object_b + at, 0u);
        put(rt, member_object_b, 0x08CDBDF0u);
        call(rt, ctx, 0x08B5E0ECu, {0x08D72924u});
        put(rt, member_object_b + 0x20u, member_b);
        put(rt, member_object_b + 0x24u, 0xFFFFFFFFu);
        put(rt, member_object_b, 0x08CDBD28u);
        const u32 table_c = call(rt, ctx, 0x08B796C4u);
        const u32 member_c = word(rt, table_c + 0xCu);

        // 0x08D728E0, constructor argument 0x08D728FC: -1 is stored before the member word here.
        for (u32 at = 4u; at <= 0x18u; at += 4u) put(rt, member_object_c + at, 0u);
        put(rt, member_object_c, 0x08CDBDF0u);
        call(rt, ctx, 0x08B5E0ECu, {0x08D728FCu});
        put(rt, member_object_c + 0x24u, 0xFFFFFFFFu);
        put(rt, member_object_c + 0x20u, member_c);
        put(rt, member_object_c, 0x08CDBD28u);

        // 0x08D72A74: constructor 0x08B65660, vtable 0x08CDBDA8, view table paired with the 0x08D72930 object, and the
        // separate table word at 0x08D72A70 (0x08CDBD94, the same value 0x08D729E4 and 0x08D72958 hold).
        call(rt, ctx, 0x08B65660u, {view_object_a});
        put(rt, 0x08D72A70u, 0x08CDBD94u);
        put(rt, view_object_a + 0x70u, 0u);
        rt.memory().store8(view_object_a + 0x74u, 0u);
        rt.memory().store8(view_object_a + 0x75u, 0u);
        put(rt, view_object_a + 0x78u, 0u);
        put(rt, view_object_a + 0x7Cu, 0u);
        put(rt, view_object_a + 0x80u, 0u);
        put(rt, view_object_a + 0x84u, 0u);
        put(rt, view_object_a, 0x08CDBDA8u);
        call(rt, ctx, 0x08C54A7Cu, {view_object_a, member_object_a});

        // 0x08D72B04 (vtable 0x08CDB188, view table paired with 0x08D72908) and its neighbour 0x08D72AFC (0x08CDB174,
        // a zero word at +4). The constructor runs before the vtable word here, as the original's order has it.
        call(rt, ctx, 0x08B65660u, {view_object_b});
        put(rt, view_object_b, 0x08CDB188u);
        put(rt, 0x08D72AFCu, 0x08CDB174u);
        put(rt, 0x08D72AFCu + 4u, 0u);
        put(rt, view_object_b + 0x70u, 0u);
        rt.memory().store8(view_object_b + 0x74u, 0u);
        rt.memory().store8(view_object_b + 0x75u, 0u);
        put(rt, view_object_b + 0x78u, 0u);
        put(rt, view_object_b + 0x7Cu, 0u);
        put(rt, view_object_b + 0x80u, 0u);
        put(rt, view_object_b + 0x84u, 0u);
        call(rt, ctx, 0x08C54A7Cu, {view_object_b, member_object_b});

        // 0x08D729E8 (vtable 0x08CDBDA8, view table paired with 0x08D728E0) and its neighbour 0x08D729E4.
        call(rt, ctx, 0x08B65660u, {view_object_c});
        put(rt, view_object_c, 0x08CDBDA8u);
        put(rt, 0x08D729E4u, 0x08CDBD94u);
        put(rt, view_object_c + 0x70u, 0u);
        rt.memory().store8(view_object_c + 0x74u, 0u);
        rt.memory().store8(view_object_c + 0x75u, 0u);
        put(rt, view_object_c + 0x78u, 0u);
        put(rt, view_object_c + 0x7Cu, 0u);
        put(rt, view_object_c + 0x80u, 0u);
        put(rt, view_object_c + 0x84u, 0u);
        call(rt, ctx, 0x08C54A7Cu, {view_object_c, member_object_c});

        // 0x08D7295C (vtable 0x08CDBDA8 in the view-table call's delay slot, view table paired with 0x08D728E0) and
        // its neighbour 0x08D72958.
        call(rt, ctx, 0x08B65660u, {view_object_d});
        put(rt, 0x08D72958u, 0x08CDBD94u);
        put(rt, view_object_d + 0x70u, 0u);
        rt.memory().store8(view_object_d + 0x74u, 0u);
        rt.memory().store8(view_object_d + 0x75u, 0u);
        put(rt, view_object_d + 0x78u, 0u);
        put(rt, view_object_d + 0x7Cu, 0u);
        put(rt, view_object_d + 0x80u, 0u);
        put(rt, view_object_d + 0x84u, 0u);
        put(rt, view_object_d, 0x08CDBDA8u);
        call(rt, ctx, 0x08C54A7Cu, {view_object_d, member_object_c});

        // The last two state words: 0x08D72AFC's +0x78 gets the 0x08D72A70 table word, and the 0x08D729E8 object's
        // +0xC gets 0x2000 (0x08D729E4's +0x10).
        put(rt, 0x08D72AFCu + 0x78u, 0x08D72A70u);
        put(rt, 0x08D729E4u + 0x10u, 0x2000u);
    }
    // The tail call hands the freshly read module word and the two table words on, as the original's a0-a3 do.
    tail_call(ctx, 0x08B6085Cu, {word(rt, 0x08CB625Cu), 1u, 0x08D72AFCu, 0x08D729E4u});
}

// --- 0x08B30A6C: the default message handler ----------------------------------------------------------------
//
// Called by the dispatcher 0x08B3141C as func_08B30A6C(self, selector, first, second, third). Under the object's lock
// (0x08B344E0 / 0x08B3457C on self + 0x8FC) it builds the message block the callback at self + 0x14 receives:
//   +0  the id from 0x08B2F774(self, selector)     +4  the third argument's first word
//   +8  the first argument's first word (if any)   +0xC/+0xD  the first argument's bytes +4/+5
//   +0x10 the second argument (id 11: second - 0xC)  +0x14 the third argument (id 11: third + 0xC)
// The id 11 path is the "short message" special case: it compares the 64-bit pair at third +4/+8 with the object's
// cached pair at +0xA0/+0xA4 and, when it is neither that nor (-1, -1), hands it to 0x08B30660; a (-1, -1) pair goes to
// 0x08B308FC. The message word at +4 then selects the follow-up: 3 builds a request block at sp+0x38 (with the matrix
// helper 0x08B79A88) and calls 0x08B2F950, 4 calls 0x08B2FAD0, 5 copies the pair into self + 0x9638/+0x963C, 6-11 only
// call the callback, and 12/13 clear a matching 8-byte entry of the 16-entry table at self + 0x95B8 (no callback).
// Every path ends by releasing the lock; the callback runs with a0 = the message block and a1 = self + 0x18.
void func_08B30A6C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x200u);
    Saved saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 selector = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    const u32 third = ctx.gpr[8];
    const u32 lock = self + 0x8FCu;
    const u32 message = frame.at(0x20u);
    const u32 request = frame.at(0x38u);
    ctx.gpr[22] = lock;
    ctx.gpr[19] = self;
    ctx.gpr[21] = selector;
    ctx.gpr[20] = second;
    ctx.gpr[17] = third;
    ctx.gpr[16] = first;  // s0 is set in the lock call's delay slot
    call(rt, ctx, 0x08B344E0u, {lock});
    const u32 callback = word(rt, self + 0x14u);
    if (callback == 0u) {
        call(rt, ctx, 0x08B3457Cu, {lock});
        return;
    }
    // The callback is the handler, called with the message block and the word at self + 0x18.
    const auto invoke_handler = [&](u32 handler) {
        call_keep(rt, ctx, handler, {message, word(rt, self + 0x18u)});
        call(rt, ctx, 0x08B3457Cu, {lock});
    };
    const auto release = [&] { call(rt, ctx, 0x08B3457Cu, {lock}); };

    // The six-word message block, then the header word and bytes (only when a header is passed).
    for (u32 at = 0u; at <= 0x14u; at += 4u) put(rt, message + at, 0u);
    if (first != 0u) {
        put(rt, message + 8u, word(rt, first));
        rt.memory().store8(message + 0xCu, rt.memory().load8(first + 4u));
        rt.memory().store8(message + 0xCu + 1u, rt.memory().load8(first + 5u));
    }
    put(rt, message, call(rt, ctx, 0x08B2F774u, {self, selector}));
    put(rt, message + 0x10u, second);
    put(rt, message + 0x14u, third);
    const u32 id = word(rt, message);
    if (id != 11u) {
        invoke_handler(callback);
        return;
    }
    if (selector == 0u) {  // the request has no payload of its own: self + 0x18 goes as the handler's second word
        invoke_handler(callback);
        return;
    }
    if (static_cast<s32>(second) <= 0) {
        invoke_handler(callback);
        return;
    }
    // The payload pair at third +4/+8, and the message's own copy of the record pointers.
    const u32 low = word(rt, third + 4u);
    const u32 high = word(rt, third + 8u);
    put(rt, message + 0x14u, third + 0xCu);
    put(rt, message + 0x10u, second - 0xCu);
    const u32 command = word(rt, message + 4u);  // the message keeps its own copy of the command word
    if ((low | high) != 0u) {
        if (rt.memory().load8(self + 5u) != 0u) {
            const bool cached = word(rt, self + 0xA0u) == low && word(rt, self + 0xA4u) == high;
            if (!cached) {
                if (low == 0xFFFFFFFFu && high == 0xFFFFFFFFu) {
                    call(rt, ctx, 0x08B308FCu, {self, third + 0xCu, second - 0xCu, third + 0xCu, 1u, 1u});
                } else {
                    // A new pair: 0x08B30660 takes it with the record pointers and two stack words.
                    put(rt, frame.at(0u), 1u);
                    put(rt, frame.at(4u), 0u);
                    call(rt, ctx, 0x08B30660u,
                         {self, third + 0xCu, low, high, third + 0xCu, second - 0xCu, word(rt, message + 4u), 1u});
                }
            }
        }
    }
    switch (command - 3u) {
    case 0u: {  // 3: the request block {record word, record +4, message +8, message +0xC}
        const u32 record = word(rt, message + 0x14u);
        put(rt, request, word(rt, record));
        put(rt, request + 4u, word(rt, record + 4u));
        call(rt, ctx, 0x08B79A88u, {frame.at(0xE0u), record + 0x10u, word(rt, record + 4u)});
        put(rt, request + 8u, word(rt, message + 8u));
        put(rt, request + 0xCu, word(rt, message + 0xCu));
        call(rt, ctx, 0x08B2F950u, {self, request});
        invoke_handler(word(rt, self + 0x14u));
        return;
    }
    case 1u:  // 4
        call(rt, ctx, 0x08B2FAD0u, {self, word(rt, message + 0x14u), word(rt, message + 0x10u)});
        invoke_handler(word(rt, self + 0x14u));
        return;
    case 2u:  // 5: the pair lands in the object's cache when its byte +5 is clear
        if (rt.memory().load8(self + 5u) == 0u) {
            const u32 pair = word(rt, message + 0x14u);
            put(rt, self + 0x10000u - 0x69C8u, word(rt, pair));
            put(rt, self + 0x10000u - 0x69C4u, word(rt, pair + 4u));
        }
        invoke_handler(word(rt, self + 0x14u));
        return;
    case 9u:
    case 10u: {  // 12, 13: drop the matching 8-byte entry of the 16-entry table at self + 0x95B8
        const u32 key_low = word(rt, message + 8u);
        const u32 key_high = word(rt, message + 0xCu);
        for (u32 index = 0u; index != 16u; ++index) {
            const u32 entry = self + 0x95B8u + index * 8u;
            if (word(rt, entry) != key_low) continue;
            if (word(rt, entry + 4u) == key_high) {
                put(rt, entry, 0u);
                put(rt, entry + 4u, 0u);
                break;
            }
        }
        release();
        return;
    }
    default:  // 6..11 and everything outside 3..13
        invoke_handler(word(rt, self + 0x14u));
        return;
    }
}

// --- 0x08B6A844: atan2f's core ------------------------------------------------------------------------------------
//
// Arguments: f12 = y (first), f13 = x (second). The function answers f0. It is the routine behind func_08B66FBC, whose
// domain check is "x = y = 0", and its constants are the .rodata pi/2, pi, -pi/2, 3pi/4 and pi/4 (0x08CB6358 onwards);
// the slow path calls fabsf (0x08B663A4, already decompiled) and atanf (0x08B65F0C, already decompiled). The sign pair
// `m` is (y < 0) | ((x < 0) << 1), built exactly as the asm's `sra x_bits, 30` + `andi 2` do it. Every path returns
// through the 0x10 frame; f0 is the last value each path loads or computes, as the asm leaves it.
void func_08B6A844(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(ctx);
    const float y = ctx.fpr[12];
    const float x = ctx.fpr[13];
    const u32 y_bits = std::bit_cast<u32>(y);
    const u32 x_bits = std::bit_cast<u32>(x);
    const u32 ay = y_bits & ~0x80000000u;
    const u32 ax = x_bits & ~0x80000000u;
    const u32 half_pi = 0x08CB6364u;      // +pi/2
    const u32 negative_half_pi = 0x08CB6360u;
    const u32 pi = 0x08CB6358u;
    const u32 negative_pi = 0x08CB635Cu;
    const u32 pi_lo = 0x08CB6380u;        // the low part of pi: 1.5099579e-07
    const auto y_negative = static_cast<s32>(y_bits) < 0;

    ctx.fpr[0] = y;
    if (static_cast<s32>(ax) > 0x7F800000) {  // x is NaN
        ctx.fpr[0] = y + x;
        return;
    }
    if (static_cast<s32>(ay) > 0x7F800000) {  // y is NaN
        ctx.fpr[0] = y + x;
        return;
    }
    if (x_bits == 0x3F800000u) {  // x == 1.0: atan2(y, 1) = atan(y)
        tail_call(ctx, 0x08B65F0Cu, {});
        return;
    }
    const u32 m = (y_bits >> 31) | ((static_cast<u32>(static_cast<s32>(x_bits) >> 30)) & 2u);
    ctx.gpr[16] = m;  // s0 holds the sign pair for the rest of the body, as the original's sra/andi delay slot leaves it

    if (ay == 0u) {  // y == +-0
        if (m == 2u) ctx.fpr[0] = float_at(rt, pi);
        else if (m < 3u) ctx.fpr[0] = y;
        else ctx.fpr[0] = float_at(rt, negative_pi);
        return;
    }
    if (ax == 0u) {  // x == +-0
        ctx.fpr[0] = y_negative ? float_at(rt, negative_half_pi) : float_at(rt, half_pi);
        return;
    }
    if (ax == 0x7F800000u) {  // x == +-inf
        if (ay == ax) {       // y == +-inf too: the 45-degree answers
            u32 constant = 0x08CB6370u;  // +pi/4
            if (m == 1u) constant = 0x08CB6374u;      // -pi/4
            else if (m == 2u) constant = 0x08CB6368u;  // +3pi/4
            else if (m == 3u) constant = 0x08CB636Cu;  // -3pi/4
            ctx.fpr[0] = float_at(rt, constant);
        } else {
            u32 constant = 0u;  // m == 0 answers +0.0
            if (m == 1u) constant = 0x08CB6378u;      // -0.0
            else if (m == 2u) constant = pi;
            else if (m == 3u) constant = negative_pi;
            ctx.fpr[0] = m == 0u ? 0.0f : float_at(rt, constant);
        }
        return;
    }
    if (ay == 0x7F800000u) {  // y == +-inf
        ctx.fpr[0] = y_negative ? float_at(rt, negative_half_pi) : float_at(rt, half_pi);
        return;
    }

    // k is the exponent distance between the two magnitudes: |y|/|x| > 2^60 answers +-pi/2 directly.
    const s32 k = (static_cast<s32>(ay) - static_cast<s32>(ax)) >> 23;
    float z;
    if (k >= 0x3D) {
        z = float_at(rt, 0x08CB637Cu);  // +pi/2
    } else if (static_cast<s32>(x_bits) < 0 && k < -0x3C) {
        z = 0.0f;  // |y/x| < 2^-60 with a negative x
    } else {
        // z = atanf(fabsf(y / x)): the division sits in the first call's delay slot, its result in the second's.
        ctx.fpr[12] = ctx.fpr[0] / ctx.fpr[13];
        ctx.fpr[0] = call_keepf(rt, ctx, 0x08B663A4u, {}, {ctx.fpr[12]}).f0;
        ctx.fpr[12] = ctx.fpr[0];
        z = call_keepf(rt, ctx, 0x08B65F0Cu, {}, {ctx.fpr[12]}).f0;
    }
    switch (m) {
    case 0u: ctx.fpr[0] = z; break;
    case 1u: ctx.fpr[0] = std::bit_cast<float>(std::bit_cast<u32>(z) ^ 0x80000000u); break;
    case 2u: ctx.fpr[0] = float_at(rt, pi) - (z - float_at(rt, pi_lo)); break;
    default: ctx.fpr[0] = (z - float_at(rt, pi_lo)) - float_at(rt, pi); break;
    }
}
} // namespace
} // namespace new_batch_main_234
} // namespace gt::game

#define NEW234(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_234::name, gt::ReturnKind::kind)
NEW234(0x08B30A6Cu, func_08B30A6C, Word);
NEW234(0x08B5B78Cu, func_08B5B78C, Word);
NEW234(0x08B6A844u, func_08B6A844, Float);
NEW234(0x08C4DF20u, func_08C4DF20, Word);

