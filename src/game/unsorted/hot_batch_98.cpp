// Hot batch 98: 35 functions written, 33 registered. Two pools: the 11 first-read reached functions of the 121-300 instruction bands left by batches 93-97 (the b93p / b95p probes and a new 231-300 probe, `b98p`: the draft's func_089C5FE0 ... func_089F7F64 and a family of three vector-resize functions, func_08BED9A0 / func_08C0D538 / func_08C11D08) and the reached functions of 22-120 instructions nobody had registered (a new probe, `b98s`, of all 7,866 unregistered functions of 20-120 instructions found 43 the routes call; the float-only ones among them were written, the VFPU ones and the ones that reach `sce*` imports were left). (main includes batch 97, 6,494 functions.)
//
// Left (read, not written): copy-on-write string code (func_089A50A0, func_089E1C90, func_089C06C0, func_08A43398, func_08BC17C8, func_08998838), the vector fill-insert / atomic-pointer code (func_08BA996C, func_0890673C, func_08BDFB84), 64-bit and soft-float wrappers (func_08B6728C, func_08B66E88, func_08C3EEE0 an introsort), the VFPU functions of the graphics library, the ones that reach sce* imports (func_08ACB3B4, the func_08B8443C family) and the long state machines (func_089525EC, func_0895D7CC, func_08C160CC).
//
// Not registered (2, each has a commented HOT98 line): func_0888E81C (reached once per run, 0 or 1 compared case, none changed memory) and func_0893B824 (two calls on the routes, compared 0 in every run).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f19): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value; `call_setf` also passes the float registers. A callee that reads a float argument register (func_08949B04 reads f12) must be given it: `tools.arg_reads` checks only the integer ones (FINDINGS). The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20-$f27 are locals. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the constants a test leaves in v0; the dead stack is not reproduced, the frames that callees read are.
// Branches the compared cases do not take are listed in each function's comment; they were read against the asm by hand and by three independent read-only reviews (which found func_08AE78E8's n[0x40] store, func_088DFB68's read order before a slot call and func_08881A68's table read order).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gt::game {
namespace hot_batch_98 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]).
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }
// The HI of the MIPS signed `div` (what `mfhi` reads): the remainder; a zero divisor leaves the dividend (the recompiler's rule), INT_MIN / -1 leaves 0.
inline std::uint32_t mips_hi(std::uint32_t dividend, std::uint32_t divisor) {
    if (divisor == 0u) return dividend;
    if (dividend == 0x80000000u && divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<std::uint32_t>(signed_of(dividend) % signed_of(divisor));
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
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


// func_089C5FE0(self, s2 = a1, s4 = a2, s6 = a3, s3 = t0, s5 = t1): a1 < 0 returns at once (v0 is the caller's). Three arms, each ending in the same helper calls:
// (1) s3 != 0 && self[0x118] != 0 && (int8)self[0x152] == 1: a 16-byte record at 0x08C8CC54 is copied to sp[0..0xC], the colour/state helpers (0x08B58874, 0x08B57744, 0x08B57750, 0x08B57820 x3, 0x08B57AB8, 0x08B5730C x2, 0x08B572C8) are driven with s0 = func_08964254(self + 0x160; f12 = func_089A8BF4(self)), then func_089C5148(self, s2, s4, s6, s3) and four 0x08B54FB8 / 0x08B57750 resets.
// (2) s3 != 0, self[0x118] != 0, self[0x152] != 1: with (0 < self[0x170] || 0 < self[0x174]) a 0x40-halfword table is built (object at sp, func_08AFAA04 .. 08AFB0F8) and every halfword set to 0xFFFF, else the plain draw; (3) s3 == 0 or self[0x118] == 0: the plain draw (func_08B59838, func_089A8B20, func_08B53978(s2, s0, s4, s6, s3, s5; f12 = func_089A8BF4(self))).
void func_089C5FE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t s2 = ctx.gpr[5];
    const std::uint32_t s4 = ctx.gpr[6];
    const std::uint32_t s6 = ctx.gpr[7];
    const std::uint32_t s3 = ctx.gpr[8];
    const std::uint32_t s5 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x100u);
    if (signed_of(s2) < 0) return;
    const auto plain = [&]() {
        (void)call_set(rt, ctx, 0x08B59838u, {});
        (void)call_set(rt, ctx, 0x089A8B20u, {self});
        const std::uint32_t s0 = ctx.gpr[2];
        (void)call_set(rt, ctx, 0x089A8BF4u, {self});
        ctx.fpr[12] = ctx.fpr[0];
        (void)call_setf(rt, ctx, 0x08B53978u, {s2, s0, s4, s6, s3, s5}, {ctx.fpr[12]});
    };
    if (s3 == 0u || byte_u(rt, self + 0x118u) == 0u) {
        (void)call_set(rt, ctx, 0x08B54FB8u, {2u});
        plain();
        return;
    }
    if (sext8(byte_u(rt, self + 0x152u)) == 1u) {
        const std::uint32_t record = 0x08C8CC54u;
        put(rt, frame.at(0xCu), word(rt, record + 0xCu));
        put(rt, frame.at(8u), word(rt, record + 8u));
        put(rt, frame.at(4u), word(rt, record + 4u));
        put(rt, frame.at(0u), word(rt, record));
        ctx.gpr[5] = word(rt, record);
        ctx.gpr[6] = word(rt, record + 4u);
        ctx.gpr[7] = word(rt, record + 0xCu);
        (void)call_set(rt, ctx, 0x089A8BF4u, {self});
        ctx.fpr[12] = ctx.fpr[0];
        (void)call_setf(rt, ctx, 0x08964254u, {self + 0x160u}, {ctx.fpr[12]});
        const std::uint32_t s0 = ctx.gpr[2];
        (void)call_set(rt, ctx, 0x08B58874u, {0x64u});
        (void)call_set(rt, ctx, 0x08B57744u, {0u, 0xFF000000u});
        (void)call_set(rt, ctx, 0x08B57750u, {3u, 1u});
        (void)call_set(rt, ctx, 0x08B58874u, {0x65u});
        (void)call_set(rt, ctx, 0x08B57820u, {0x65u, 0u, 0xFF000000u});
        (void)call_set(rt, ctx, 0x08B57820u, {0x65u, 1u, 0xFF000000u});
        (void)call_set(rt, ctx, 0x08B57820u, {0x65u, 4u, 0xFFFFFFFFu});
        (void)call_set(rt, ctx, 0x08B57AB8u, {0x65u, 0xDu, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08B5730Cu, {1u, 1u, s0 & 0xFF000000u});
        (void)call_set(rt, ctx, 0x08B5730Cu, {1u, 4u, s0});
        (void)call_setf(rt, ctx, 0x08B572C8u, {1u, 0xBu}, {0.0f});
        (void)call_set(rt, ctx, 0x08B58874u, {2u});
        (void)call_set(rt, ctx, 0x089A8C40u, {self, 0u});
        (void)call_set(rt, ctx, 0x089C5148u, {self, s2, s4, s6, s3});
        (void)call_set(rt, ctx, 0x08B54FB8u, {2u});
        (void)call_set(rt, ctx, 0x08B57750u, {3u, 0u});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x65u});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x64u});
        return;
    }
    (void)call_set(rt, ctx, 0x08B58874u, {2u});
    (void)call_set(rt, ctx, 0x089A8C40u, {self, 0u});
    if (0.0f < float_at(rt, self + 0x170u) || 0.0f < float_at(rt, self + 0x174u)) {
        (void)call_set(rt, ctx, 0x08AFAA04u, {});
        const std::uint32_t s0 = ctx.gpr[2];
        (void)call_set(rt, ctx, 0x08AFB4B4u, {frame.at(0u)});
        (void)call_set(rt, ctx, 0x08AFAD8Cu, {frame.at(0u), 0x40u, 1u, 2u, 0u, 0u});
        (void)call_set(rt, ctx, 0x08AFA5C0u, {frame.at(0u)});
        (void)call_set(rt, ctx, 0x08AFAA78u, {frame.at(0u), 0u});
        const std::uint32_t entry = ctx.gpr[2];
        put_float(rt, entry + 0xCu, 1.0f);
        put_float(rt, entry + 8u, 1.0f);
        (void)call_set(rt, ctx, 0x08B52578u, {entry});
        const std::uint32_t table = call_set(rt, ctx, slot_of(rt, frame.at(0u), 0x48u), {frame.at(0u), 0u, 0u, 0u, 0u});
        for (std::uint32_t i = 0u; i != 0x40u; ++i) put16(rt, table + 2u * i, 0xFFFFFFFFu);
        ctx.gpr[2] = table + 2u * 0x40u;
        ctx.gpr[3] = 0x40u;
        ctx.gpr[4] = 0x40u;
        ctx.gpr[5] = 0xFFFFFFFFu;
        (void)call_keep(rt, ctx, 0x08B55B90u, {});
        (void)call_set(rt, ctx, slot_of(rt, s0, 0x18u), {s0});
        (void)call_set(rt, ctx, 0x08B57DE8u, {0xFFFFFFFFu});
        (void)call_set(rt, ctx, 0x089C4810u, {self, word(rt, self + 0x184u)});
        (void)call_set(rt, ctx, 0x08B55AACu, {2u, 0x8000u});
        (void)call_setf(rt, ctx, 0x089C5394u, {}, {0.0f, 0.0f, 100.0f, 100.0f, 0.0f, 0.0f, 64.0f, 1.0f});
        (void)call_set(rt, ctx, 0x08AFB0F8u, {frame.at(0u)});
        (void)call_set(rt, ctx, 0x08B54FB8u, {2u});
        return;
    }
    plain();
    (void)call_set(rt, ctx, 0x08B54FB8u, {2u});
}

// func_08AE3DB4(self, a1 = default): evaluates a value node. t = self->fn_2C(self) (a function pointer at self + 0x2C); kind = t & 0xF0, arg = t & 0xF. kind >= 0xA0: v0 = a1. Otherwise self->fn_34(self) is called (result unused) and
// 0x00: func_08AE3D30(self, arg); 0x10: its negation; 0x30: func_08ADEACC(func_08AE3D30(self, arg)); 0x40: func_08AE3CD8(self, func_08AE3D30(self, arg)); 0x20: three nested evaluations (lo, hi, step) and a pick lo + rnd % (...) (func_08AD9E2C is the random generator);
// 0x50: n = func_08AE3D30(self, arg) values (n <= 0: a1) with n weights |value| (all weights 0: every weight 1), a weighted pick by rnd % total; any other kind: 0 (and when the bit-0 debug flag at 0x08CDFB3C is set, func_08AD83B8 reports it).
void func_08AE3DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t deflt = ctx.gpr[5];
    (void)call_keep(rt, ctx, word(rt, self + 0x2Cu), {self});
    const std::uint32_t t = ctx.gpr[2];
    const std::uint32_t kind = t & 0xF0u;
    const std::uint32_t arg = t & 0xFu;
    if (signed_of(kind) >= 0xA0) {
        ctx.gpr[2] = deflt;
        return;
    }
    (void)call_set(rt, ctx, word(rt, self + 0x34u), {self});
    std::uint32_t result = 0u;
    switch (kind) {
    case 0x00u:
    case 0x10u:
        result = call_set(rt, ctx, 0x08AE3D30u, {self, arg});
        if (kind == 0x10u) result = 0u - result;
        break;
    case 0x30u:
        (void)call_set(rt, ctx, 0x08AE3D30u, {self, arg});
        result = call_set(rt, ctx, 0x08ADEACCu, {ctx.gpr[2]});
        break;
    case 0x40u: {
        const std::uint32_t value = call_set(rt, ctx, 0x08AE3D30u, {self, arg});
        result = call_set(rt, ctx, 0x08AE3CD8u, {self, value});
        break;
    }
    case 0x50u: {
        const std::uint32_t n = call_set(rt, ctx, 0x08AE3D30u, {self, arg});
        if (signed_of(n) <= 0) {
            ctx.gpr[2] = deflt;
            return;
        }
        std::vector<std::uint32_t> values(n);
        std::vector<std::uint32_t> weights(n);
        std::uint32_t total = 0u;
        for (std::uint32_t i = 0u; i < n; ++i) {
            values[i] = call_set(rt, ctx, 0x08AE3DB4u, {self, 0u});
            const std::uint32_t v = call_set(rt, ctx, 0x08AE3DB4u, {self, 0u});
            const std::uint32_t negated = 0u - v;
            weights[i] = signed_of(v) > signed_of(negated) ? v : negated;
            total += weights[i];
        }
        if (signed_of(total) <= 0) {
            for (std::uint32_t i = 0u; i < n; ++i) weights[i] = 1u;
            total = n;
        }
        const std::uint32_t r = call_set(rt, ctx, 0x08AD9E2Cu, {});
        const std::uint32_t pick = mips_hi(r, total);
        std::uint32_t base = 0u;
        for (std::uint32_t i = 0u; i < n; ++i) {
            const std::uint32_t weight = weights[i];
            if (signed_of(weight) > 0) {
                if (!(signed_of(pick) < signed_of(base))) {
                    if (signed_of(pick) < signed_of(base + weight)) {
                        ctx.gpr[2] = values[i];
                        return;
                    }
                }
                base += weight;
            }
        }
        break;
    }
    case 0x20u: {
        std::uint32_t lo = call_set(rt, ctx, 0x08AE3DB4u, {self, 0u});
        std::uint32_t hi = call_set(rt, ctx, 0x08AE3DB4u, {self, 0u});
        const std::uint32_t step = call_set(rt, ctx, 0x08AE3DB4u, {self, 0u});
        if (signed_of(hi) < signed_of(lo)) std::swap(lo, hi);
        result = lo;
        if (lo == hi) break;
        const std::uint32_t r = call_set(rt, ctx, 0x08AD9E2Cu, {});
        if (signed_of(step) < 2) {
            result = lo + mips_hi(r, hi - lo + 1u);
        } else {
            const std::uint32_t slots = static_cast<std::uint32_t>(signed_of(hi - lo) / signed_of(step)) + 1u;
            result = lo + mips_hi(r, slots) * step;
        }
        break;
    }
    default:
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) {
            (void)call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C9145Cu, 0x45Du, 0x08C91520u, t});
        }
        break;
    }
    ctx.gpr[2] = result;
}

// The low word of a MIPS `mult` shifted right 12 arithmetically (`mflo; sra 12`): fixed-point 20.12 scaling of two signed values.
inline std::uint32_t mul_sra12(std::uint32_t a, std::uint32_t b) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(a * b) >> 12); }

// func_08AE78E8(self = a0, p = a1): p = {+4 key, +8 list, +0xC id, +0x10 level, +0x14 scale}. func_08AE753C(self, p[4]); func_08ADC1CC(list) == 0 returns 0. func_08ADBF4C(list, id, sp) returns the first record r and writes the record count to sp[0]; the records are walked by r += r[8].
// A record matches when (int8)r[0x18] <= level <= (int8)r[0x19]; for each match a node n = func_08AE7618(self, p[4], list, a3, r[0xC], r[0xD], r[0xE], r[0xF]) (a3 = (r[0] & 4) ? r[0x1C] - 1000 : r[0x34]) is made (null: stop), the first node is remembered in fp, n's (int8)n[0x36] halfwords at n[0x38..] are passed to func_08AEF874(h, 7, r[0x28], r[0x2C]), and n[0x40..0x53] are filled from r: bit 0 of r[0] copies the scaled r[0x20] (n[0x40] = scale * r[0x20] >> 12, n[0x42] = r[0x22], n[0x44], n[0x46]) else n[0x40] = scale (the low halfword; the beqz delay slot leaves it in v0) and n[0x48] / n[0x4A] are scaled by r[0x20] / r[0x22]; bit 2 copies r[0x30..0x33] to n[0x50..0x53] directly, else n[0x4C] is scaled by r[0x1E] (and by func_08AD986C(level * 128 - r[0x1C] * 128 + (int8)r[0x1D]) unless bit 1); n[0x24] = r.
// At the end (or when a node could not be made) the chain from fp is searched for a negative halfword (func_08AE75C0(self, fp)); func_08ADC230(list) is the last call and its v0 the result.
// Branches the compared cases do not take (checked against the asm by hand): see the PR.
void func_08AE78E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t p = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t key = word(rt, p + 4u);
    const std::uint32_t list = word(rt, p + 8u);
    const std::uint32_t scale = word(rt, p + 0x14u);
    const std::uint32_t id = word(rt, p + 0xCu);
    const std::uint32_t level = word(rt, p + 0x10u);
    (void)call_set(rt, ctx, 0x08AE753Cu, {self, key, 0u});
    if (call_set(rt, ctx, 0x08ADC1CCu, {list}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    std::uint32_t r = call_set(rt, ctx, 0x08ADBF4Cu, {list, id, frame.at(0u)});
    std::uint32_t fp = 0u;
    const auto finish = [&]() { (void)call_set(rt, ctx, 0x08ADC230u, {list}); };
    // 0x7A34: the remembered node's chain has a negative halfword, or a node could not be made.
    const auto report = [&]() {
        if (fp != 0u) (void)call_set(rt, ctx, 0x08AE75C0u, {self, fp});
        finish();
    };
    if (r != 0u && signed_of(word(rt, frame.at(0u))) > 0) {
        const std::uint32_t scaled_level = level << 7u;
        std::uint32_t index = 0u;
        for (;;) {
            const std::uint32_t count = word(rt, frame.at(0u));
            const bool below = signed_of(level) < signed_of(sext8(byte_u(rt, r + 0x18u)));
            const bool above = !below && signed_of(sext8(byte_u(rt, r + 0x19u))) < signed_of(level);
            if (!below && !above) {
                const std::uint32_t bits = word(rt, r);
                const std::uint32_t flag4 = bits & 4u;
                const std::uint32_t a3 = flag4 != 0u ? byte_u(rt, r + 0x1Cu) - 0x3E8u : word(rt, r + 0x34u);
                const std::uint32_t node = call_set(rt, ctx, 0x08AE7618u,
                                                    {self, key, list, a3, sext8(byte_u(rt, r + 0xCu)), byte_u(rt, r + 0xDu), sext8(byte_u(rt, r + 0xEu)),
                                                     sext8(byte_u(rt, r + 0xFu))});
                if (node == 0u) {
                    report();
                    return;
                }
                if (fp == 0u) fp = node;
                const std::uint32_t entries = sext8(byte_u(rt, node + 0x36u));
                if (signed_of(entries) > 0) {
                    std::uint32_t at = node + 0x38u;
                    std::uint32_t i = 0u;
                    do {
                        (void)call_set(rt, ctx, 0x08AEF874u, {sext16(half_u(rt, at)), 7u, word(rt, r + 0x28u), word(rt, r + 0x2Cu)});
                        ++i;
                        at += 2u;
                    } while (signed_of(i) < signed_of(sext8(byte_u(rt, node + 0x36u))));
                }
                if ((bits & 1u) != 0u) {
                    put16(rt, node + 0x40u, mul_sra12(scale, sext16(half_u(rt, r + 0x20u))));
                    put16(rt, node + 0x42u, half_u(rt, r + 0x22u));
                } else {
                    const std::uint32_t y = sext16(half_u(rt, node + 0x4Au));
                    put16(rt, node + 0x40u, scale);  // v0 = the scale: the delay slot of the beqz loads it on both paths
                    put16(rt, node + 0x48u, mul_sra12(sext16(half_u(rt, node + 0x48u)), sext16(half_u(rt, r + 0x20u))));
                    put16(rt, node + 0x4Au, mul_sra12(y, sext16(half_u(rt, r + 0x22u))));
                }
                put16(rt, node + 0x44u, half_u(rt, r + 0x24u));
                put16(rt, node + 0x46u, half_u(rt, r + 0x26u));
                std::uint32_t b50;
                if (flag4 != 0u) {
                    b50 = byte_u(rt, r + 0x30u);
                } else {
                    std::uint32_t s0 = mul_sra12(sext16(half_u(rt, node + 0x4Cu)), sext16(half_u(rt, r + 0x1Eu)));
                    if ((bits & 2u) == 0u) {
                        const std::uint32_t arg = scaled_level - (byte_u(rt, r + 0x1Cu) << 7u) + sext8(byte_u(rt, r + 0x1Du));
                        const std::uint32_t factor = call_set(rt, ctx, 0x08AD986Cu, {arg});
                        s0 = mul_sra12(s0, factor);
                    }
                    put16(rt, node + 0x4Cu, s0);
                    b50 = byte_u(rt, r + 0x30u);
                }
                put8(rt, node + 0x50u, b50);
                put8(rt, node + 0x51u, byte_u(rt, r + 0x31u));
                put8(rt, node + 0x52u, byte_u(rt, r + 0x32u));
                put(rt, node + 0x24u, r);
                put8(rt, node + 0x53u, byte_u(rt, r + 0x33u));
                (void)count;
            }
            const std::uint32_t step = word(rt, r + 8u);
            ++index;
            const bool more = signed_of(index) < signed_of(word(rt, frame.at(0u)));
            r += step;
            if (!more) break;
        }
    }
    if (fp != 0u) {
        std::uint32_t chain = fp;
        for (;;) {
            const std::uint32_t entries = sext8(byte_u(rt, chain + 0x36u));
            if (signed_of(entries) > 0) {
                std::uint32_t at = chain + 0x38u;
                std::uint32_t i = 0u;
                for (;;) {
                    const std::uint32_t h = sext16(half_u(rt, at));
                    ++i;
                    if (signed_of(h) < 0) {
                        report();
                        return;
                    }
                    at += 2u;
                    if (!(signed_of(i) < signed_of(entries))) break;
                }
            }
            const std::uint32_t parent = word(rt, chain + 4u);
            chain = signed_of(word(rt, parent + 8u)) < 0 ? 0u : parent;
            if (chain == 0u) break;
        }
    }
    finish();
}

// func_088E8A84(self, a1 = owner): a1 != 0 attaches: while self[0x11] is set, func_08B2E2B8(2000) is called up to 0x1388 times; then if self[4] != 0 and self[0x2C4] != 0 the answer is 0 (busy); otherwise self[0x2C4] = 1, self[0xC] = a1. a1 == 0 detaches: self[0x2C4] = 0, self[0xC] = 0, the old owner's slot 0xD0 is called, self[0x10] = 0, func_088E7EBC(self, -1), self[0x12] = 0, func_088E8A44(self, 9), self[0x1FC] = -1.
// Both then reset the state block (self[0x13..0x22], [0x2A0..0x2D8], the three words at 0x2A4, func_088D1A90(self + 0x30)) and, with a record r = self[0x2E4] != 0 and self[0x1C] == 0, clear r[3], r[0x36], r[1], set r[0x3D] = -1, r[0x3E] = 0 and give the slot s = func_088E7EA0(self) the value (a1 != 0) plus a counter r[0x3F]; r[0x14] = (a1 != 0). v0 = 1.
void func_088E8A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    std::uint32_t attached;
    if (owner != 0u) {
        if (byte_u(rt, self + 0x11u) != 0u) {
            std::uint32_t tries = 1u;
            do {
                (void)call_set(rt, ctx, 0x08B2E2B8u, {0x7D0u});
                ++tries;
            } while (byte_u(rt, self + 0x11u) != 0u && tries != 0x1389u);
        }
        if (word(rt, self + 4u) != 0u && word(rt, self + 0x2C4u) != 0u) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            return;
        }
        attached = 1u;
        put(rt, self + 0x2C4u, 1u);
        put(rt, self + 0xCu, owner);
    } else {
        const std::uint32_t old = word(rt, self + 0xCu);
        put(rt, self + 0x2C4u, 0u);
        put(rt, self + 0xCu, 0u);
        if (old != 0u) (void)call_set(rt, ctx, slot_of(rt, old, 0xD0u), {old});
        put8(rt, self + 0x10u, 0u);
        (void)call_set(rt, ctx, 0x088E7EBCu, {self, 0xFFFFFFFFu});
        put8(rt, self + 0x12u, 0u);
        (void)call_set(rt, ctx, 0x088E8A44u, {self, 9u});
        put(rt, self + 0x1FCu, 0xFFFFFFFFu);
        attached = 0u;
    }
    put8(rt, self + 0x15u, 1u);
    put(rt, self + 0x2D8u, 0xFFFFFFFFu);
    put(rt, self + 0x2DCu, 0xFFFFFFFFu);
    put8(rt, self + 0x13u, 0u);
    put8(rt, self + 0x14u, 0u);
    put8(rt, self + 0x2B7u, 0u);
    put8(rt, self + 0x22u, 0u);
    put8(rt, self + 0x2A2u, 0u);
    put16(rt, self + 0x20u, 0xFFFFFFFFu);
    (void)call_set(rt, ctx, 0x088D1A90u, {self + 0x30u});
    put(rt, self + 0x2BCu, 0xFFFFFFFFu);
    put8(rt, self + 0x2A0u, 0xFFFFFFFFu);
    put8(rt, self + 0x18u, 0u);
    put(rt, self + 0x2C0u, 0u);
    put8(rt, self + 0x2A1u, 0u);
    for (std::uint32_t i = 0u; i != 3u; ++i) put(rt, self + 0x2A4u + 4u * i, 0u);
    ctx.gpr[3] = 1u;
    ctx.gpr[4] = 3u;
    std::uint32_t flag = 1u;
    const std::uint32_t record = word(rt, self + 0x2E4u);
    if (record != 0u) {
        (void)call_set(rt, ctx, 0x088E7EA0u, {self});
        const std::uint32_t slot = ctx.gpr[2];
        if (word(rt, self + 0x1Cu) == 0u) {
            put8(rt, record + 3u, 0u);
            put8(rt, word(rt, self + 0x2E4u) + 0x36u, 0u);
            put8(rt, word(rt, self + 0x2E4u) + 1u, 0u);
            put8(rt, word(rt, self + 0x2E4u) + 0x3Du, 0xFFFFFFFFu);
            put8(rt, word(rt, self + 0x2E4u) + 0x3Eu, 0u);
            if (word(rt, self + 0xCu) == 0u) {
                put(rt, slot, attached);
            } else {
                const std::uint32_t counter = word(rt, self + 0x2E4u) + 0x3Fu;
                put8(rt, counter, byte_u(rt, counter) + 1u);
                put8(rt, slot + 4u, byte_u(rt, word(rt, self + 0x2E4u) + 0x3Fu));
                put(rt, slot, attached);
            }
            put(rt, word(rt, self + 0x2E4u) + 0x14u, attached);
        }
        flag = 1u;
    }
    put8(rt, self + 0x2B4u, flag);
    put(rt, self + 0x2D0u, 0u);
    put(rt, self + 0x2D4u, 0u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 1u;
}

// func_08B13DC8(position = a0, buffer = a1, length = a2): XORs `length` bytes of the buffer with the 256-byte keystream table at 0x08C92BC0, starting at table index position & 0xFF and wrapping at 256. The original works in three stages (a head up to the next 64-byte table boundary, whole 64-byte blocks as word or lwr/lwl xors, a byte tail) and issues `cache 0x1E` hints (a no-op in the host); the effect on memory is the same as the byte loop below.
// A zero length is not handled by the original (its do-while loops run on), so it is not reproduced. No callee-saved register or return value is set (v0 / v1 are whatever the last stage leaves).
void func_08B13DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t position = ctx.gpr[4];
    const std::uint32_t buffer = ctx.gpr[5];
    const std::uint32_t length = ctx.gpr[6];
    constexpr std::uint32_t kTable = 0x08C92BC0u;
    for (std::uint32_t i = 0u; i != length; ++i) {
        put8(rt, buffer + i, byte_u(rt, buffer + i) ^ byte_u(rt, kTable + ((position + i) & 0xFFu)));
    }
}

// func_0894A15C(self): for i = 0..3: A = func_08955300(self, i), B = func_0894268C(self[0xC], i, A[0xE9]), C = func_089554A8(self, i) (two floats); d = C[4] * A[0x30] - C[0] * A[0x2C] (f22), k = A[0x34] (f21), A[0x51] = 0.
// B[0x250] != 0: A[0x51] / A[0x52] get the signs (+1 / -1) of the comparisons of d with k; A[0x38] = 1.0f and A[0x3C] = clamp(+-(d - k) * self[0xC][0x1348], -1, 1). B[0x250] == 0: with |k| below 1 degree (0.017453292f = 0x3C8E38E4): f = func_08949B04(0) and A[0x52] = sign(k vs d), f20 = 1 or 0 and A[0x51] = -1; otherwise ratios (d - k) * func_08949B04(0 or 1) / k or d divided into f20; A[0x38] = the last func_08949B04 result f1; f20 is clamped to [-1, 1], A[0x3C] = 0 when A[0x51] == 0, else f20 scaled by (d / (s7[0x100 or 0x101] * 0.27777779f)) when |d| is below that bound (s7 = 0x08CDE508). v0 = 4.
// Branches the compared cases do not take (checked against the asm by hand): see the PR.
void func_0894A15C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    constexpr std::uint32_t kS7 = 0x08CDE508u;
    const float f25 = std::bit_cast<float>(0x3C8E38E4u);
    const float f24 = std::bit_cast<float>(0xBC8E38E4u);
    const float f23 = 0.0f;
    const std::uint32_t game = word(rt, self + 0xCu);
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t a = call_set(rt, ctx, 0x08955300u, {self, i});
        const std::uint32_t b = call_set(rt, ctx, 0x0894268Cu, {game, i, byte_u(rt, a + 0xE9u)});
        const std::uint32_t c = call_set(rt, ctx, 0x089554A8u, {self, i});
        float f1 = float_at(rt, c);
        float f0 = float_at(rt, c + 4u);
        const float f12 = float_at(rt, a + 0x30u);
        const float f2c = float_at(rt, a + 0x2Cu);
        put8(rt, a + 0x51u, 0u);
        f0 = psp_mul(f0, f12);
        f1 = psp_mul(f1, f2c);
        const float f21 = float_at(rt, a + 0x34u);
        const float f22 = f0 - f1;
        if (byte_u(rt, b + 0x250u) != 0u) {
            float f2 = f22 - f21;
            if (f23 <= f21) {
                if (f22 <= f21) {
                    put8(rt, a + 0x52u, 1u);
                    put8(rt, a + 0x51u, 1u);
                } else {
                    put8(rt, a + 0x52u, 0xFFFFFFFFu);
                    put8(rt, a + 0x51u, 0xFFFFFFFFu);
                }
            } else {
                f2 = -f2;
                if (f21 <= f22) {
                    put8(rt, a + 0x52u, 0xFFFFFFFFu);
                    put8(rt, a + 0x51u, 1u);
                } else {
                    put8(rt, a + 0x52u, 1u);
                    put8(rt, a + 0x51u, 0xFFFFFFFFu);
                }
            }
            f0 = float_at(rt, game + 0x1348u);
            f1 = 1.0f;
            f0 = psp_mul(f2, f0);
            put_float(rt, a + 0x38u, f1);
            if (f1 < f0) {
                f0 = f1;
                put_float(rt, a + 0x3Cu, f0);
            } else {
                f1 = -1.0f;
                if (!(f0 < f1)) {
                    put_float(rt, a + 0x3Cu, f0);
                } else {
                    f0 = f1;
                    put_float(rt, a + 0x3Cu, f0);
                }
            }
            continue;
        }
        float f20;
        const auto ratio = [&]() {
            float t = f22 - f21;
            t = psp_mul(f1, t);
            f20 = t / f22;
        };
        if (f21 <= f24 || f25 <= f21) {
            if (f23 < f21) {
                f20 = f22 - f21;
                if (f22 <= f21) {
                    (void)call_setf(rt, ctx, 0x08949B04u, {1u}, {f12});
                    f1 = ctx.fpr[0];
                    f20 = psp_mul(f20, f1);
                    put8(rt, a + 0x52u, 1u);
                    f20 = f20 / f21;
                    put8(rt, a + 0x51u, 1u);
                } else {
                    (void)call_setf(rt, ctx, 0x08949B04u, {0u}, {f12});
                    f1 = ctx.fpr[0];
                    if (f25 <= f22) {
                        ratio();
                    } else {
                        f20 = 0.0f;
                        if (f22 <= f24) ratio();
                    }
                    put8(rt, a + 0x52u, 0xFFFFFFFFu);
                    put8(rt, a + 0x51u, 0xFFFFFFFFu);
                }
            } else {
                f20 = f22 - f21;
                if (f21 <= f22) {
                    (void)call_setf(rt, ctx, 0x08949B04u, {1u}, {f12});
                    f1 = ctx.fpr[0];
                    f20 = psp_mul(f20, f1);
                    put8(rt, a + 0x52u, 0xFFFFFFFFu);
                    f20 = f20 / f21;
                    put8(rt, a + 0x51u, 1u);
                } else {
                    (void)call_setf(rt, ctx, 0x08949B04u, {0u}, {f12});
                    f1 = ctx.fpr[0];
                    if (f25 <= f22) {
                        ratio();
                    } else {
                        f20 = 0.0f;
                        if (f22 <= f24) ratio();
                    }
                    put8(rt, a + 0x52u, 1u);
                    put8(rt, a + 0x51u, 0xFFFFFFFFu);
                }
            }
        } else {
            (void)call_setf(rt, ctx, 0x08949B04u, {0u}, {f12});
            f1 = ctx.fpr[0];
            if (f21 < f22) {
                f20 = 1.0f;
                put8(rt, a + 0x52u, 0xFFFFFFFFu);
            } else if (f22 < f21) {
                f20 = 1.0f;
                put8(rt, a + 0x52u, 1u);
            } else {
                f20 = 0.0f;
                put8(rt, a + 0x52u, 0u);
            }
            put8(rt, a + 0x51u, 0xFFFFFFFFu);
        }
        // 0x0894A428: the common tail.
        put_float(rt, a + 0x38u, f1);
        if (1.0f < f20) {
            f20 = 1.0f;
        } else {
            const float low = -1.0f;
            if (f20 < low) f20 = low;
        }
        const std::uint32_t sign = sext8(byte_u(rt, a + 0x51u));
        if (sign == 0u) {
            f20 = 0.0f;
            put_float(rt, a + 0x3Cu, f20);
            continue;
        }
        float bound_d = f22;
        std::uint32_t scale;
        if (signed_of(sign) >= 0) {
            bound_d = f21;
            scale = byte_u(rt, kS7 + 0x101u);
        } else {
            scale = byte_u(rt, kS7 + 0x100u);
        }
        if (scale == 0u) {
            put_float(rt, a + 0x3Cu, f20);
            continue;
        }
        const float converted = static_cast<float>(signed_of(scale));
        const bool negative = bound_d < f23;
        float limit = psp_mul(converted, std::bit_cast<float>(0x3E8E38E4u));
        if (negative) bound_d = -bound_d;
        if (!(bound_d < limit)) {
            put_float(rt, a + 0x3Cu, f20);
        } else {
            limit = bound_d / limit;
            f20 = psp_mul(f20, limit);
            put_float(rt, a + 0x3Cu, f20);
        }
    }
    ctx.gpr[2] = 4u;
}

// func_0880E6EC(self; f12 = x, f13 = a, f14 = b): positions the five cells of a window w = self[0x24] (nothing when null; v0 is the caller's then). When func_089DDC94(w) is not 0xA the window is prepared: w[0xE8] = 6, func_089DCCD0(w, 0xA), func_089DE054(w, 0xA), func_089DE1BC(w, 0xA), then for k = 0, 2, 4, 6, 8 func_089DAEA0(w, k; 2.0f / 17.0f, 0, 21.0f / 22.0f, 25.0f), func_089DB078(w, k; 0, 0, 1.0f, 1.0f) and func_089DB204(w, k + 1, self[0x23C] + 0xA0 / 0xA0 / 0x40 / 0x40 / 0x80).
// Then with x' = 1.0f - x: func_089DAF14(w, 0 / 4; 2.0f, 0 / x' * 25.0f, 21.0f, x' * 25.0f / 25.0f) and func_089DB0EC(w, 0 / 4; ...), and with p = (a < b) ? 1.0f - b : 1.0f - a, q = 1.0f - b (b' = the new b) the cells 2, 6, 8 the same way; the last func_089DB0EC(w, 8; 0, b', 1.0f, 1.0f) is a tail call.
void func_0880E6EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    float f20 = ctx.fpr[12];
    const float f26 = ctx.fpr[13];
    float f25 = ctx.fpr[14];
    const std::uint32_t w = word(rt, self + 0x24u);
    if (w == 0u) return;
    if (call_set(rt, ctx, 0x089DDC94u, {w}) != 0xAu) {
        put(rt, w + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DCCD0u, {w, 0xAu});
        (void)call_set(rt, ctx, 0x089DE054u, {w, 0xAu});
        (void)call_set(rt, ctx, 0x089DE1BCu, {w, 0xAu});
        for (std::uint32_t k = 0u; k != 10u; k += 2u) {
            const bool narrow = k == 0u || k == 4u;
            (void)call_setf(rt, ctx, 0x089DAEA0u, {w, k}, {narrow ? 2.0f : 17.0f, 0.0f, narrow ? 21.0f : 22.0f, 25.0f});
            (void)call_setf(rt, ctx, 0x089DB078u, {w, k}, {0.0f, 0.0f, 1.0f, 1.0f});
            (void)call_set(rt, ctx, 0x089DB204u, {w, k + 1u, word(rt, self + 0x23Cu) + (k < 4u ? 0xA0u : (k < 8u ? 0x40u : 0x80u))});
        }
    }
    const float f24 = 1.0f;
    f20 = f24 - f20;
    const float f23 = 25.0f;
    const float f21 = psp_mul(f20, f23);
    const float f22 = 0.0f;
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 0u}, {2.0f, 0.0f, 21.0f, f21});
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 0u}, {f22, f22, f24, f20});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 4u}, {2.0f, f21 + f22, 21.0f, f23});
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 4u}, {f22, f20, f24, f24});
    float f24b = f24;
    if (f26 < f25) {
        f25 = f24 - f25;
        f24b = f25;
    } else {
        f25 = f24 - f25;
        f24b = f24 - f26;
    }
    float p = psp_mul(f24b, f23);
    const float z = 0.0f;
    float q = psp_mul(f25, f23);
    p = p + z;
    q = q + z;
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 2u}, {17.0f, z, 39.0f, p});
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 2u}, {z, z, 1.0f, f24b});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 6u}, {17.0f, p, 39.0f, q});
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 6u}, {z, f24b, 1.0f, f25});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 8u}, {17.0f, q, 39.0f, f23});
    ctx.fpr[12] = z;
    ctx.fpr[13] = f25;
    ctx.fpr[14] = 1.0f;
    ctx.fpr[15] = 1.0f;
    tail_call(ctx, 0x089DB0ECu, {w, 8u});
}

// func_0880EB34(self; f12 = x, f13 = y, f14 = d): positions the five cells of a window w = self[0x38] (nothing when null; v0 is the caller's then). f25 = x / d, f27 = y / d. When func_089DDC94(w) is not 0xA the window is prepared: w[0xE8] = 6, func_089DE054(w, 0xA), func_089DE1BC(w, 0xA), func_089DCCD0(w, 0xA), w[0x140] |= 0x60, then for k = 0, 2, 4, 6, 8 func_089DAEA0(w, k; the row of the first table below), func_089DB078(w, k; the second table) and func_089DB204(w, k + 1, self[0x23C] + 0xA0 / 0x40 / 0x80 / 0x40 / 0x80).
// Then with m = (y / d < x / d) ? y / d : x / d the cells 0 .. 6 get func_089DAF14 / func_089DB0EC pairs built from x / d * 80, m * 80, m * 2.5, x / d * 2.5, y / d * 80 and 1.0f / 2.5f / 8.0f / 3.0f / 81.0f / 82.0f; the last func_089DB0EC(w, 6; y / d * 2.5, 0, 2.5, 1.0) is a tail call.
void func_0880EB34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    const float d = ctx.fpr[14];
    const std::uint32_t w = word(rt, self + 0x38u);
    if (w == 0u) return;
    const float f27 = y / d;
    float f25 = x / d;
    if (call_set(rt, ctx, 0x089DDC94u, {w}) != 0xAu) {
        put(rt, w + 0xE8u, 6u);
        (void)call_set(rt, ctx, 0x089DE054u, {w, 0xAu});
        (void)call_set(rt, ctx, 0x089DE1BCu, {w, 0xAu});
        (void)call_set(rt, ctx, 0x089DCCD0u, {w, 0xAu});
        put(rt, w + 0x140u, word(rt, w + 0x140u) | 0x60u);
        struct Cell {
            std::uint32_t k;
            std::array<float, 4> first;
            std::array<float, 4> second;
            std::uint32_t offset;
        };
        static constexpr Cell kCells[5] = {
            {0u, {1.0f, 4.0f, 80.0f, 4.0f}, {0.0f, 0.0f, 2.5f, 1.0f}, 0xA0u},
            {2u, {1.0f, 4.0f, 0.0f, 4.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0x40u},
            {4u, {1.0f, 4.0f, 0.0f, 4.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0x80u},
            {6u, {2.0f, 2.0f, 80.0f, 1.0f}, {0.0f, 0.0f, 2.5f, 1.0f}, 0x40u},
            {8u, {1.0f, 9.0f, 80.0f, 1.0f}, {0.0f, 0.0f, 2.5f, 1.0f}, 0x80u},
        };
        for (const Cell &cell : kCells) {
            (void)call_setf(rt, ctx, 0x089DAEA0u, {w, cell.k}, {cell.first[0], cell.first[1], cell.first[2], cell.first[3]});
            (void)call_setf(rt, ctx, 0x089DB078u, {w, cell.k}, {cell.second[0], cell.second[1], cell.second[2], cell.second[3]});
            (void)call_set(rt, ctx, 0x089DB204u, {w, cell.k + 1u, word(rt, self + 0x23Cu) + cell.offset});
        }
    }
    const float f20a = f27 < f25 ? f27 : f25;
    const float f23 = 80.0f;
    float f21 = psp_mul(f25, f23);
    const float f22 = 1.0f;
    const float f24 = 2.5f;
    const float f26 = psp_mul(f20a, f24);
    f21 = f21 + f22;
    f25 = psp_mul(f25, f24);
    float f20 = psp_mul(f20a, f23);
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 0u}, {f21, 4.0f, 81.0f, 8.0f});
    f20 = f20 + f22;
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 0u}, {f25, 0.0f, f24, f22});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 2u}, {f20, 4.0f, f21, 8.0f});
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 2u}, {f26, 0.0f, f25, f22});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 4u}, {f22, 4.0f, f20, 8.0f});
    const float f23b = psp_mul(f27, f23);
    (void)call_setf(rt, ctx, 0x089DB0ECu, {w, 4u}, {0.0f, 0.0f, f26, f22});
    (void)call_setf(rt, ctx, 0x089DAF14u, {w, 6u}, {f23b + 2.0f, 2.0f, 82.0f, 3.0f});
    ctx.fpr[12] = psp_mul(f27, f24);
    ctx.fpr[13] = 0.0f;
    ctx.fpr[14] = f24;
    ctx.fpr[15] = f22;
    tail_call(ctx, 0x089DB0ECu, {w, 6u});
}

// func_08923E0C(obj = a0, src = a1): a camera orientation pass. The three floats at 0x08CF3668 + 0x58 / 0x60 / 0x54 are copied to 0x08CF2874 / 0x08CF2864 + 0x08CF2860 / 0x08CF2870. (src[0x50], src[0x54]) go to sp[0x60..]; sp[0x68] / sp[0x6C] are read without ever being written by this function (stack garbage the original reads too: the harness gives both runs the same RAM, and the decompiled frame is at the same address).
// When obj[0x2C] == 1 the pair and sp[0x68..0x6C] become obj[0xC..0x18] and obj[0x2C] is cleared. Then func_08B4A8D4(sp + 0x50, sp + 0x50, obj + 0xC) and the two angles sp[0x50] / sp[0x54] are wrapped into [-pi, pi] (3.1415927f = 0x40490FDB, 2 pi = 0x40C90FDB), func_08B4A918(sp + 0x30, ..., 0x08CF2870), the vector obj[0x1C..0x28] is copied to sp + 0x20 and func_08B4A918 / func_08B4A8D4 / func_08B4A6A0(sp + 0x10; f12 = D_08CDDD68) / func_08B4A890 / func_08B4A6A0(sp; ...) / func_08B4A890(obj + 0xC, ...) update both vectors; obj[0xC] and obj[0x10] are wrapped again; the matrix at src + 0xC0 is built from sp + 0x80 (identity-like: 1.0f at +0, +0x14, +0x28, the position src[0x40..0x48] at sp + 0xB0) with func_08B4E9D4(src + 0xC0, sp + 0x80), func_08B4F3B0(src + 0xC0; f12 = obj[0x10]), func_08B4F2E4(src + 0xC0; f12 = obj[0xC]).
void func_08923E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t obj = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    GuestFrame frame(ctx, 0xF0u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    const float kPi = std::bit_cast<float>(0x40490FDBu);
    const float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
    const float kMinusPi = std::bit_cast<float>(0xC0490FDBu);
    constexpr std::uint32_t kBase = 0x08CF3668u;
    put_float(rt, 0x08CF2874u, float_at(rt, kBase + 0x58u));
    const float k60 = float_at(rt, kBase + 0x60u);
    const float k54 = float_at(rt, kBase + 0x54u);
    put_float(rt, 0x08CF2864u, k60);
    put_float(rt, 0x08CF2870u, k54);
    put_float(rt, 0x08CF2860u, k60);
    float f0 = float_at(rt, src + 0x50u);
    float f1 = float_at(rt, src + 0x54u);
    const std::uint32_t flag = byte_u(rt, obj + 0x2Cu);
    put_float(rt, at(0x60u), f0);
    put_float(rt, at(0x64u), f1);
    const std::uint32_t s4 = obj + 0xCu;
    float f3 = float_at(rt, at(0x68u));
    float f2 = float_at(rt, at(0x6Cu));
    if (flag == 1u) {
        put_float(rt, obj + 0xCu, f0);
        put_float(rt, s4 + 4u, f1);
        put_float(rt, s4 + 8u, f3);
        put_float(rt, s4 + 0xCu, f2);
        put8(rt, obj + 0x2Cu, 0u);
    }
    f0 = float_at(rt, at(0x60u));
    f1 = float_at(rt, at(0x64u));
    put_float(rt, at(0x50u), f0);
    put_float(rt, at(0x54u), f1);
    put_float(rt, at(0x5Cu), f2);
    put_float(rt, at(0x58u), f3);
    (void)call_set(rt, ctx, 0x08B4A8D4u, {at(0x50u), at(0x50u), s4});
    f2 = float_at(rt, at(0x50u));
    f1 = float_at(rt, at(0x54u));
    put_float(rt, at(0x74u), f1);
    f0 = float_at(rt, at(0x58u));
    f1 = float_at(rt, at(0x5Cu));
    put_float(rt, at(0x70u), f2);
    put_float(rt, at(0x78u), f0);
    put_float(rt, at(0x7Cu), f1);
    if (kPi < f2) {
        put_float(rt, at(0x70u), f2 - kTwoPi);
    } else if (f2 < kMinusPi) {
        put_float(rt, at(0x70u), f2 + kTwoPi);
    }
    f1 = float_at(rt, at(0x74u));
    if (kPi < f1) {
        f1 = f1 - kTwoPi;
        put_float(rt, at(0x74u), f1);
    } else if (f1 < kMinusPi) {
        f1 = f1 + kTwoPi;
        put_float(rt, at(0x74u), f1);
    }
    f0 = float_at(rt, at(0x70u));
    put_float(rt, at(0x34u), f1);
    f1 = float_at(rt, at(0x7Cu));
    put_float(rt, at(0x30u), f0);
    const std::uint32_t s0 = obj + 0x1Cu;
    f0 = float_at(rt, at(0x78u));
    put_float(rt, at(0x3Cu), f1);
    put_float(rt, at(0x38u), f0);
    (void)call_set(rt, ctx, 0x08B4A918u, {at(0x30u), at(0x30u), 0x08CF2870u});
    f2 = float_at(rt, s0 + 8u);
    f3 = float_at(rt, s0 + 0xCu);
    f0 = float_at(rt, obj + 0x1Cu);
    f1 = float_at(rt, s0 + 4u);
    put_float(rt, at(0x28u), f2);
    put_float(rt, at(0x2Cu), f3);
    put_float(rt, at(0x20u), f0);
    put_float(rt, at(0x24u), f1);
    (void)call_set(rt, ctx, 0x08B4A918u, {at(0x20u), at(0x20u), 0x08CF2860u});
    f0 = float_at(rt, at(0x30u));
    f1 = float_at(rt, at(0x34u));
    put_float(rt, at(0x40u), f0);
    f0 = float_at(rt, at(0x38u));
    put_float(rt, at(0x44u), f1);
    f1 = float_at(rt, at(0x3Cu));
    put_float(rt, at(0x48u), f0);
    put_float(rt, at(0x4Cu), f1);
    (void)call_set(rt, ctx, 0x08B4A8D4u, {at(0x40u), at(0x40u), at(0x20u)});
    f2 = float_at(rt, at(0x40u));
    f3 = float_at(rt, at(0x44u));
    f0 = float_at(rt, at(0x48u));
    f1 = float_at(rt, at(0x4Cu));
    const float scale = float_at(rt, 0x08CDDD68u);
    put_float(rt, at(0x10u), f2);
    put_float(rt, at(0x14u), f3);
    put_float(rt, at(0x18u), f0);
    put_float(rt, at(0x1Cu), f1);
    put_float(rt, at(0x70u), f2);
    put_float(rt, at(0x74u), f3);
    put_float(rt, at(0x78u), f0);
    put_float(rt, at(0x7Cu), f1);
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {at(0x10u)}, {scale});
    (void)call_set(rt, ctx, 0x08B4A890u, {s0, s0, at(0x10u)});
    f0 = float_at(rt, s0 + 4u);
    f1 = float_at(rt, s0 + 8u);
    f2 = float_at(rt, obj + 0x1Cu);
    f3 = float_at(rt, s0 + 0xCu);
    const float scale2 = float_at(rt, 0x08CDDD68u);
    put_float(rt, at(4u), f0);
    put_float(rt, at(8u), f1);
    put_float(rt, at(0u), f2);
    put_float(rt, at(0xCu), f3);
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {at(0u)}, {scale2});
    (void)call_set(rt, ctx, 0x08B4A890u, {s4, s4, at(0u)});
    f1 = float_at(rt, s4);
    if (kPi < f1) {
        put_float(rt, s4, f1 - kTwoPi);
    } else if (f1 < kMinusPi) {
        put_float(rt, s4, f1 + kTwoPi);
    }
    f1 = float_at(rt, s4 + 4u);
    const std::uint32_t s1 = s4 + 4u;
    if (kPi < f1) {
        put_float(rt, s4 + 4u, f1 - kTwoPi);
    } else if (f1 < kMinusPi) {
        put_float(rt, s4 + 4u, f1 + kTwoPi);
    }
    const std::uint32_t v0 = src + 0x40u;
    f2 = float_at(rt, src + 0x40u);
    f3 = float_at(rt, v0 + 8u);
    f1 = float_at(rt, v0 + 4u);
    const std::uint32_t matrix = src + 0xC0u;
    put_float(rt, at(0xB0u), f2);
    put_float(rt, at(0xB4u), f1);
    put_float(rt, at(0xB8u), f3);
    put_float(rt, at(0xBCu), 1.0f);
    put_float(rt, at(0x80u), 1.0f);
    put_float(rt, at(0x94u), 1.0f);
    put_float(rt, at(0xA8u), 1.0f);
    put(rt, at(0x90u), 0u);
    put(rt, at(0xA0u), 0u);
    put(rt, at(0x84u), 0u);
    put(rt, at(0xA4u), 0u);
    put(rt, at(0x88u), 0u);
    put(rt, at(0x98u), 0u);
    put(rt, at(0x8Cu), 0u);
    put(rt, at(0x9Cu), 0u);
    put(rt, at(0xACu), 0u);
    (void)call_set(rt, ctx, 0x08B4E9D4u, {matrix, at(0x80u)});
    (void)call_setf(rt, ctx, 0x08B4F3B0u, {matrix}, {float_at(rt, s1)});
    (void)call_setf(rt, ctx, 0x08B4F2E4u, {matrix}, {float_at(rt, s4)});
}

// func_08992AA4(self): the bounding size of a container. func_08A00AB0() (with the a0 / a1 the context holds from the entry) and func_08968DE4(self) == 0 end it (v0 = 0). Otherwise the children are walked from self[0xF4] with func_08968A38(child) as the next link: a child other than self[0x120] / [0x124] / [0x128] that answers func_08A00448(self, child) gives (x, y) = func_0896B7AC(child, sp + 4, sp + 8) plus the integer sums of func_08966B20(child, sp + 0xC .. sp + 0x18) (sp[0xC] + sp[0x10], sp[0x14] + sp[0x18]) and func_0896726C(child, sp, sp + 1); f23 / f22 keep the maxima of x / y.
// The three special children (self[0x120], [0x124], [0x128]; the same calls into sp + 0x1C / 0x24 / 0x2C) are then added the same way; (f12, f13) = (max of the x's, max of the y's) + (self[0x134] + the third x, self[0x138] + the second y); the results are floored by f23 / f22 (f20, f21); when either is above zero func_08A0D048(self, sp + 0xC .. sp + 0x18) and func_08966CF0(self; f12 = f20 + the sum of sp[0xC] + sp[0x10] as a float, f13 = f21 + the sum of sp[0x14] + sp[0x18]).
// v0 is the last value the original leaves (a callee's v0, or the sp[0x18] it loads after func_08966B20).
void func_08992AA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    (void)call_set(rt, ctx, 0x08A00AB0u, {});
    if (call_set(rt, ctx, 0x08968DE4u, {self}) == 0u) return;
    float f22 = 0.0f;
    float f23 = f22;
    const auto bounds = [&](std::uint32_t child) {
        // 08966B20 writes four ints; the float sums are (sp[0xC] + sp[0x10], sp[0x14] + sp[0x18]) as the original adds them.
        (void)call_set(rt, ctx, 0x08966B20u, {child, at(0xCu), at(0x10u), at(0x14u), at(0x18u)});
        const std::uint32_t a = word(rt, at(0x10u));
        const std::uint32_t b = word(rt, at(0xCu));
        const std::uint32_t c = word(rt, at(0x18u));
        const std::uint32_t d = word(rt, at(0x14u));
        ctx.gpr[2] = c;
        return std::pair<float, float>(static_cast<float>(signed_of(b + a)), static_cast<float>(signed_of(d + c)));
    };
    std::uint32_t s0 = word(rt, self + 0xF4u);
    while (s0 != 0u) {
        if (s0 != word(rt, self + 0x120u) && s0 != word(rt, self + 0x124u) && s0 != word(rt, self + 0x128u)) {
            if (call_set(rt, ctx, 0x08A00448u, {self, s0}) != 0u) {
                (void)call_set(rt, ctx, 0x0896B7ACu, {s0, at(4u), at(8u)});
                const auto sums = bounds(s0);
                const float f1 = float_at(rt, at(8u));
                const float f0 = float_at(rt, at(4u));
                const float f2 = sums.second + f1;
                const float f3 = sums.first + f0;
                put_float(rt, at(8u), f2);
                put_float(rt, at(4u), f3);
                (void)call_set(rt, ctx, 0x0896726Cu, {s0, at(0u), at(1u)});
                const float x = float_at(rt, at(4u));
                const float y = float_at(rt, at(8u));
                if (f23 < x) f23 = x;
                if (f22 < y) f22 = y;
            }
        }
        s0 = call_set(rt, ctx, 0x08968A38u, {s0});
    }
    put(rt, at(0x1Cu), 0u);
    put(rt, at(0x20u), 0u);
    std::uint32_t child = word(rt, self + 0x120u);
    if (child != 0u) {
        if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
            (void)call_set(rt, ctx, 0x0896B7ACu, {word(rt, self + 0x120u), at(0x1Cu), at(0x20u)});
            const auto sums = bounds(word(rt, self + 0x120u));
            const float f1 = float_at(rt, at(0x1Cu));
            const float f3 = sums.first + f1;
            const float f0 = float_at(rt, at(0x20u));
            put_float(rt, at(0x1Cu), f3);
            const float f2 = sums.second + f0;
            put_float(rt, at(0x20u), f2);
        } else {
            ctx.gpr[2] = 0u;
        }
    }
    child = word(rt, self + 0x124u);
    put(rt, at(0x24u), 0u);
    put(rt, at(0x28u), 0u);
    if (child != 0u) {
        if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
            (void)call_set(rt, ctx, 0x0896B7ACu, {word(rt, self + 0x124u), at(0x24u), at(0x28u)});
            const auto sums = bounds(word(rt, self + 0x124u));
            const float f1 = float_at(rt, at(0x24u));
            const float f3 = sums.first + f1;
            const float f0 = float_at(rt, at(0x28u));
            put_float(rt, at(0x24u), f3);
            const float f2 = sums.second + f0;
            put_float(rt, at(0x28u), f2);
        } else {
            ctx.gpr[2] = 0u;
        }
    }
    float f3 = 0.0f;
    float f4 = f3;
    put_float(rt, at(0x2Cu), f3);
    put_float(rt, at(0x30u), f3);
    child = word(rt, self + 0x128u);
    if (child != 0u) {
        if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
            (void)call_set(rt, ctx, 0x0896B7ACu, {word(rt, self + 0x128u), at(0x2Cu), at(0x30u)});
            const auto sums = bounds(word(rt, self + 0x128u));
            f3 = float_at(rt, at(0x2Cu));
            f4 = float_at(rt, at(0x30u));
            f3 = sums.first + f3;
            put_float(rt, at(0x2Cu), f3);
            f4 = sums.second + f4;
            put_float(rt, at(0x30u), f4);
        } else {
            ctx.gpr[2] = 0u;
            f3 = float_at(rt, at(0x2Cu));
            f4 = float_at(rt, at(0x30u));
        }
    }
    float f12 = float_at(rt, at(0x24u));
    float f0 = float_at(rt, at(0x1Cu));
    if (!(f0 < f12)) f12 = f0;
    f0 = float_at(rt, at(0x20u));
    float f13 = f4;
    put_float(rt, at(4u), f12);
    if (!(f0 < f4)) f13 = f0;
    f0 = float_at(rt, self + 0x134u);
    float f2 = float_at(rt, self + 0x138u);
    const float f1 = float_at(rt, at(0x28u));
    f0 = f0 + f3;
    f2 = f2 + f1;
    float f20 = f12 + f0;
    float f21 = f13 + f2;
    put_float(rt, at(4u), f20);
    put_float(rt, at(8u), f21);
    if (!(f23 < f20)) f20 = f23;
    if (!(f22 < f21)) f21 = f22;
    f0 = 0.0f;
    if (!(f0 < f20) && !(f0 < f21)) return;
    (void)call_set(rt, ctx, 0x08A0D048u, {self, at(0xCu), at(0x10u), at(0x14u), at(0x18u)});
    const std::uint32_t v10 = word(rt, at(0x10u));
    const std::uint32_t v0c = word(rt, at(0xCu));
    const std::uint32_t v18 = word(rt, at(0x18u));
    const std::uint32_t v14 = word(rt, at(0x14u));
    ctx.gpr[4] = self;
    const float g12 = f20 + static_cast<float>(signed_of(v0c + v10));
    const float g13 = f21 + static_cast<float>(signed_of(v14 + v18));
    ctx.gpr[2] = v18;
    (void)call_setf(rt, ctx, 0x08966CF0u, {self}, {g12, g13});
}

// func_089F7F64(self): blends the keyframes of an animation. self[0xB4] == 0 returns 0; func_089DA5C0(self, sp) == 0 (the blend factor t is the float at sp[0]) returns 0. The object at sp + 4 is filled by the slot 0x38 of self[0xB4]'s vtable (slot(sp + 4, self[0xB4], func_08A0F8C8(self))); s3 = func_089F7EFC(self), s5 = func_089F7CB4(self).
// With func_089FA8A0(s3) == 1 and func_089FA8A0(s5) == 1 (a single track): func_089FB168(sp + 4, 1), (1 - t) * P + t * Q for the four floats of func_089F9B54(s3, 0) = P and func_089F9B54(s5, 0) = Q go to sp[0x20..0x2C], sp[0x30] = -1 and by the mode self[0x7C] (== 1: add into / 2: multiply into / 0: copy to) func_089F9B34(sp + 4, 0). Otherwise (func_089FA8A0(sp + 4) == 4 skips the func_089FB168(sp + 4, 4)) the same for each of the four tracks i = 0..3 with (1 - t) * P_i + Q_i (Q is not scaled by t there, as the original does).
// The vtable slot 0x3C of self[0xB4] is then called with (self[0xB4], func_08A0F8C8(self), sp + 4) and func_089F9B74(sp + 4) ends it (v0 = its result).
void func_089F7F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    const auto at = [&](std::uint32_t offset) { return frame.at(offset); };
    ctx.gpr[2] = word(rt, self + 0xB4u);
    if (ctx.gpr[2] == 0u) return;
    if (call_set(rt, ctx, 0x089DA5C0u, {self, at(0u)}) == 0u) return;
    std::uint32_t s0 = word(rt, self + 0xB4u);
    const std::uint32_t s4 = at(4u);
    {
        const std::uint32_t slot = word(rt, word(rt, s0) + 0x38u);
        const std::uint32_t arg = call_set(rt, ctx, 0x08A0F8C8u, {self});
        (void)call_set(rt, ctx, slot, {s4, s0, arg});
    }
    const std::uint32_t s3 = call_set(rt, ctx, 0x089F7EFCu, {self});
    const std::uint32_t s5 = call_set(rt, ctx, 0x089F7CB4u, {self});
    const std::uint32_t tracks = call_set(rt, ctx, 0x089FA8A0u, {s3});
    // R op= sp[0x20..0x2C]: add (mode 1) or multiply (mode 2) the four floats of R.
    const auto combine = [&](std::uint32_t r, bool add) {
        for (std::uint32_t k = 0u; k != 0x10u; k += 4u) {
            const float a = float_at(rt, r + k);
            const float b = float_at(rt, at(0x20u + k));
            put_float(rt, r + k, add ? a + b : psp_mul(a, b));
        }
    };
    bool single = false;
    std::uint32_t s1 = 0u;
    if (tracks == 1u) {
        s1 = call_set(rt, ctx, 0x089FA8A0u, {s5});
        single = (s1 == tracks);
    }
    if (single) {
        (void)call_set(rt, ctx, 0x089FB168u, {s4, 1u});
        const std::uint32_t p = call_set(rt, ctx, 0x089F9B54u, {s3, 0u});
        float f20 = 1.0f - float_at(rt, at(0u));
        float f23 = float_at(rt, p + 0xCu);
        float f21 = float_at(rt, p);
        const float f0p = float_at(rt, p + 8u);
        float f22 = float_at(rt, p + 4u);
        f23 = psp_mul(f20, f23);
        f21 = psp_mul(f20, f21);
        f22 = psp_mul(f20, f22);
        const std::uint32_t q = call_set(rt, ctx, 0x089F9B54u, {s5, 0u});
        f20 = psp_mul(f20, f0p);
        const float f1 = float_at(rt, q + 0xCu);
        const float f2 = float_at(rt, q);
        const float f3 = float_at(rt, q + 4u);
        const float f4 = float_at(rt, q + 8u);
        const float f0 = float_at(rt, at(0u));
        const std::uint32_t mode = word(rt, self + 0x7Cu);
        f23 = f23 + psp_mul(f0, f1);
        f21 = f21 + psp_mul(f0, f2);
        f22 = f22 + psp_mul(f0, f3);
        f20 = f20 + psp_mul(f0, f4);
        put_float(rt, at(0x2Cu), f23);
        put_float(rt, at(0x20u), f21);
        put_float(rt, at(0x24u), f22);
        put_float(rt, at(0x28u), f20);
        put(rt, at(0x30u), 0xFFFFFFFFu);
        if (mode == s1) {
            combine(call_set(rt, ctx, 0x089F9B34u, {s4, 0u}), true);
        } else if (mode == 2u) {
            combine(call_set(rt, ctx, 0x089F9B34u, {s4, 0u}), false);
        } else if (mode == 0u) {
            const std::uint32_t r = call_set(rt, ctx, 0x089F9B34u, {s4, 0u});
            if (r != at(0x20u)) {
                (void)call_set(rt, ctx, 0x089641C4u, {r, at(0x20u)});
                put(rt, r + 0x10u, word(rt, at(0x30u)));
            }
        }
    } else {
        std::uint32_t probe = call_set(rt, ctx, 0x089FA8A0u, {s4});
        if (probe != 4u) (void)call_set(rt, ctx, 0x089FB168u, {s4, 4u});
        const std::uint32_t s6 = at(0x20u);
        for (s1 = 0u; s1 != 4u; ++s1) {
            const std::uint32_t p = call_set(rt, ctx, 0x089F9B54u, {s3, s1});
            float f20 = 1.0f - float_at(rt, at(0u));
            float f23 = float_at(rt, p + 0xCu);
            float f21 = float_at(rt, p);
            const float f0p = float_at(rt, p + 8u);
            float f22 = float_at(rt, p + 4u);
            f23 = psp_mul(f20, f23);
            f21 = psp_mul(f20, f21);
            f22 = psp_mul(f20, f22);
            const std::uint32_t q = call_set(rt, ctx, 0x089F9B54u, {s5, s1});
            f20 = psp_mul(f20, f0p);
            const float f3 = float_at(rt, q + 0xCu);
            const float f0 = float_at(rt, q);
            const float f1 = float_at(rt, q + 4u);
            const float f2 = float_at(rt, q + 8u);
            f21 = f21 + f0;
            f22 = f22 + f1;
            f20 = f20 + f2;
            f23 = f23 + f3;
            const std::uint32_t mode = word(rt, self + 0x7Cu);
            put_float(rt, at(0x20u), f21);
            put_float(rt, at(0x24u), f22);
            put_float(rt, at(0x28u), f20);
            put_float(rt, at(0x2Cu), f23);
            put(rt, at(0x30u), 0xFFFFFFFFu);
            if (mode == 1u) {
                combine(call_set(rt, ctx, 0x089F9B34u, {s4, s1}), true);
            } else if (mode == 2u) {
                combine(call_set(rt, ctx, 0x089F9B34u, {s4, s1}), false);
            } else if (mode == 0u) {
                const std::uint32_t r = call_set(rt, ctx, 0x089F9B34u, {s4, s1});
                if (r != s6) {
                    (void)call_set(rt, ctx, 0x089641C4u, {r, s6});
                    put(rt, r + 0x10u, word(rt, at(0x30u)));
                }
            }
        }
    }
    s0 = word(rt, self + 0xB4u);
    {
        const std::uint32_t slot = word(rt, word(rt, s0) + 0x3Cu);
        const std::uint32_t arg = call_set(rt, ctx, 0x08A0F8C8u, {self});
        (void)call_set(rt, ctx, slot, {s0, arg, s4});
    }
    (void)call_set(rt, ctx, 0x089F9B74u, {s4});
}

// func_0888E81C(self, a1 = first, a2 = second): n = func_0889417C(self[0xC]) (a record whose first word is a flag set), w = self[4], b = w[0x618]. Neither flag 0x10 nor flag 8 of n[0] set: v0 = 0 and nothing else. Otherwise sp[4] = (func_088DB2C8(w, first) != 0 ? first : -1) and sp[8] = (func_088DB2C8(w, second) != 0 ? second : -1);
// for the two ids s1 = sp[4], sp[8] (negative: skipped): r = func_088DA788(w, s1) must have bytes r[0x25], r[0x27], r[0x26] clear, b[0] != 0; then n[0] & 8 clear counts the id without recording it; otherwise r2 = func_088DA788(w, index != 0 ? first : second) gives the 16 bytes at r2 + 0x1BC copied to sp[0xC..0x1B],
// func_08B4A8D4(sp + 0xC, sp + 0xC, r + 0x1BC) is run and the id is recorded (a halfword at sp + 2 * count, count + 1) when sqrt(func_08B4A744(sp + 0xC)) * 216.0f > 30.0f and sqrt(func_08B4A744(r + 0x1BC)) > sqrt(func_08B4A744(r2 + 0x1BC)). A count <= 0 ends with v0 = 2 (the loop bound left in v0); else b[0] = 0 and the slot 0x70 of self's vtable runs with (self, w + 4, sp, count, 0, 0): its v0 is the result.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_0888E81C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t node = call_set(rt, ctx, 0x0889417Cu, {word(rt, self + 0xCu)});
    const std::uint32_t flags = word(rt, node);
    const std::uint32_t world = word(rt, self + 4u);
    const std::uint32_t gate = word(rt, world + 0x618u);
    if ((flags & 0x10u) == 0u && (flags & 8u) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = flags;
        return;
    }
    put(rt, frame.at(8u), 0xFFFFFFFFu);
    put(rt, frame.at(4u), 0xFFFFFFFFu);
    const std::uint32_t found_first = call_set(rt, ctx, 0x088DB2C8u, {world, first});
    put(rt, frame.at(4u), found_first != 0u ? first : 0xFFFFFFFFu);
    const std::uint32_t found_second = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), second});
    put(rt, frame.at(8u), found_second != 0u ? second : 0xFFFFFFFFu);
    std::uint32_t count = 0u;
    for (std::uint32_t index = 0u; index != 2u; ++index) {
        const std::uint32_t id = word(rt, frame.at(4u + 4u * index));
        if (signed_of(id) < 0) continue;
        const std::uint32_t record = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        if (byte_u(rt, record + 0x25u) != 0u || byte_u(rt, record + 0x27u) != 0u || byte_u(rt, record + 0x26u) != 0u) continue;
        if (byte_u(rt, gate) == 0u) continue;
        if ((word(rt, node) & 8u) == 0u) {
            ++count;
            continue;
        }
        const std::uint32_t outer = word(rt, self + 4u);
        put(rt, frame.at(0x20u), record + 0x1BCu);
        const std::uint32_t other = call_set(rt, ctx, 0x088DA788u, {outer, index != 0u ? first : second, record + 0x1BCu});
        const std::uint32_t vector = other + 0x1BCu;
        put(rt, frame.at(0x24u), vector);
        put(rt, frame.at(0xCu), word(rt, vector));
        put(rt, frame.at(0x10u), word(rt, vector + 4u));
        put(rt, frame.at(0x14u), word(rt, vector + 8u));
        put(rt, frame.at(0x18u), word(rt, vector + 0xCu));
        (void)call_set(rt, ctx, 0x08B4A8D4u, {frame.at(0xCu), frame.at(0xCu), word(rt, frame.at(0x20u))});
        float length = call_keep(rt, ctx, 0x08B4A744u, {frame.at(0xCu)}, {}).f0;
        length = psp_mul(std::sqrt(length), std::bit_cast<float>(0x43580000u));
        if (length <= std::bit_cast<float>(0x41F00000u)) continue;
        const float a = call_keep(rt, ctx, 0x08B4A744u, {word(rt, frame.at(0x20u))}, {}).f0;
        const float b = call_keep(rt, ctx, 0x08B4A744u, {word(rt, frame.at(0x24u))}, {}).f0;
        if (std::sqrt(a) <= std::sqrt(b)) continue;
        put16(rt, frame.at(0u) + 2u * count, id);
        ++count;
    }
    if (signed_of(count) <= 0) {
        ctx.gpr[2] = 2u;
        return;
    }
    put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
    const std::uint32_t slot = word(rt, word(rt, self) + 0x70u);
    ctx.gpr[2] = call_set(rt, ctx, slot, {self, word(rt, self + 4u) + 4u, frame.at(0u), count, 0u, 0u});
}

// func_088D78EC(self, a1 = s4, a2 = mode): a one-time start. func_088D9E58(self) == 0 returns 0; self[0x30] != 3 or self[0x22] != 0 returns 1 (v1 = 1 is set in the delay slot of the first test on both paths). Then, with c = self[0x640]: c != 0 and func_088E66C0(c) != 0 clears s4; c == 0 and s4 != 0 with func_088D4C9C(s4) != 0 returns 0.
// For i = 0..3 (n = func_088DB2C8(self, i)) and j = 0..1: the object o = func_088F8CC0(self[0x14C], i, j), when not null, gets its virtual slot 0x5C with (o, flag) where flag = (mode == 1 && n != 0). Then (s4 != 0: func_088D4988(self + 4, s4), func_088D4CDC(self + 4) fed to func_088DC974(self, .), func_088D5028(self + 4, self[0x130] + 0x5C); s4 == 0 and self[0x640] != 0: func_088D5108(self + 4, self[0x130] + 0x5C)),
// func_088D5ED0(self + 4, ., self[0x6C0], self[0x6C4], 0) (the 64-bit pair in a2/a3, its a1 is only an output), the slot 0x28 of self[0x134] with (obj, self + 4) when that is not null, func_088D7744(self, 0), func_088D776C(self, 1), func_088D78D8(self, 1), self[0x22] = 1 and v0 = 1.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088D78EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t s4 = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    if (call_set(rt, ctx, 0x088D9E58u, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    if (word(rt, self + 0x30u) != 3u || byte_u(rt, self + 0x22u) != 0u) {
        ctx.gpr[2] = 1u;
        ctx.gpr[3] = 1u;
        return;
    }
    const std::uint32_t counter = word(rt, self + 0x640u);
    if (counter != 0u) {
        if (call_set(rt, ctx, 0x088E66C0u, {counter}) != 0u) s4 = 0u;
    } else if (s4 != 0u) {
        if (call_set(rt, ctx, 0x088D4C9Cu, {s4}) != 0u) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            return;
        }
    }
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t known = call_set(rt, ctx, 0x088DB2C8u, {self, i});
        for (std::uint32_t j = 0u; j != 2u; ++j) {
            const std::uint32_t object = call_set(rt, ctx, 0x088F8CC0u, {word(rt, self + 0x14Cu), i, j});
            if (object == 0u) continue;
            const std::uint32_t flag = (mode == 1u && known != 0u) ? 1u : 0u;
            (void)call_set(rt, ctx, slot_of(rt, object, 0x5Cu), {object, flag});
        }
    }
    const std::uint32_t inner = self + 4u;
    std::uint32_t a2;
    if (s4 != 0u) {
        (void)call_set(rt, ctx, 0x088D4988u, {inner, s4});
        const std::uint32_t value = call_set(rt, ctx, 0x088D4CDCu, {inner});
        (void)call_set(rt, ctx, 0x088DC974u, {self, value});
        (void)call_set(rt, ctx, 0x088D5028u, {inner, word(rt, self + 0x130u) + 0x5Cu});
    } else if (word(rt, self + 0x640u) != 0u) {
        (void)call_set(rt, ctx, 0x088D5108u, {inner, word(rt, self + 0x130u) + 0x5Cu});
    }
    a2 = word(rt, self + 0x6C0u);
    (void)call_set(rt, ctx, 0x088D5ED0u, {inner, 0u, a2, word(rt, self + 0x6C4u), 0u});
    const std::uint32_t handler = word(rt, self + 0x134u);
    if (handler != 0u) {
        (void)call_set(rt, ctx, slot_of(rt, handler, 0x28u), {handler, inner});
    }
    (void)call_set(rt, ctx, 0x088D7744u, {self, 0u});
    (void)call_set(rt, ctx, 0x088D776Cu, {self, 1u});
    (void)call_set(rt, ctx, 0x088D78D8u, {self, 1u});
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 1u;
    put8(rt, self + 0x22u, 1u);
}

// func_088DFB68(self, a1, a2 = key, a3, t0 = flag): inner = self + 4. func_088D4824(inner), func_088DF790(self), func_088D5028(inner, self[0x130] + 0x5C); ok = func_088D4C9C(inner). When ok: func_088DC1F0(self) == 0 returns 0; func_088E4EF8(self[0x640], inner, key) == 0 runs the slot 0x70 of self[0x148] (its answer feeds func_088E49D0(self[0x640], .)), clears self[0x640] and returns 0.
// With a handler h = self[0x148] != 0 (n = self[0x640] != 0): n != 0 first takes a count (key == 0: -1; else key[0x7C], -1 unless positive), runs the slot 0x10 of h and gives its answer to func_088E43D0(self[0x640], ., count); then the slot 0xC of h (h, n) and the slot 0x80 of h (h, self[0x640] != 0). self[0x5B5] = (ok == 0). func_088DC784(self, 1), func_088D776C(self, 1), func_088FD974(func_088D4C6C(inner)).
// func_088D62FC(inner) == 2: self[0x628] = self[0x62C], func_088CEF14(that, self, inner, self[0x134], self[0x14]), func_088CEF38(that, 0), func_088CEF58(that, func_088D4C50(inner)); == 4 (asked again): self[0x630] = self[0x634], func_088E7790(that, self, inner, self[0x134], self[0x14]), func_088E77A4(that, 0). v0 = func_088DC32C(self, flag, 1).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088DFB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t arg1 = ctx.gpr[5];
    std::uint32_t key = ctx.gpr[6];
    const std::uint32_t flag = ctx.gpr[8] & 0xFFu;
    const std::uint32_t inner = self + 4u;
    (void)call_set(rt, ctx, 0x088D4824u, {inner, arg1});
    (void)call_set(rt, ctx, 0x088DF790u, {self});
    (void)call_set(rt, ctx, 0x088D5028u, {inner, word(rt, self + 0x130u) + 0x5Cu});
    const std::uint32_t ok = call_set(rt, ctx, 0x088D4C9Cu, {inner});
    if (ok != 0u) {
        if (call_set(rt, ctx, 0x088DC1F0u, {self}) == 0u) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            return;
        }
        if (call_set(rt, ctx, 0x088E4EF8u, {word(rt, self + 0x640u), inner, key}) == 0u) {
            const std::uint32_t handler = word(rt, self + 0x148u);
            const std::uint32_t named = word(rt, self + 0x640u);  // the delay slot of the jalr loads it before the call
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, handler, 0x70u), {handler});
            (void)call_set(rt, ctx, 0x088E49D0u, {named, answer});
            put(rt, self + 0x640u, 0u);
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            return;
        }
    }
    const std::uint32_t handler = word(rt, self + 0x148u);
    if (handler != 0u) {
        const std::uint32_t named = word(rt, self + 0x640u) != 0u ? 1u : 0u;
        if (named != 0u) {
            if (key == 0u) {
                key = 0xFFFFFFFFu;
            } else {
                key = word(rt, key + 0x7Cu);
                if (signed_of(key) <= 0) key = 0xFFFFFFFFu;
            }
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, handler, 0x10u), {handler});
            (void)call_set(rt, ctx, 0x088E43D0u, {word(rt, self + 0x640u), answer, key});
        }
        const std::uint32_t again = word(rt, self + 0x148u);
        (void)call_set(rt, ctx, slot_of(rt, again, 0xCu), {again, named});
        const std::uint32_t last = word(rt, self + 0x148u);
        const std::uint32_t current = word(rt, self + 0x640u);
        (void)call_set(rt, ctx, slot_of(rt, last, 0x80u), {last, current != 0u ? 1u : 0u});
    }
    put8(rt, self + 0x5B5u, ok == 0u ? 1u : 0u);
    (void)call_set(rt, ctx, 0x088DC784u, {self, 1u});
    (void)call_set(rt, ctx, 0x088D776Cu, {self, 1u});
    const std::uint32_t four = call_set(rt, ctx, 0x088D4C6Cu, {inner});
    (void)call_set(rt, ctx, 0x088FD974u, {four});
    if (call_set(rt, ctx, 0x088D62FCu, {inner}) == 2u) {
        const std::uint32_t object = word(rt, self + 0x62Cu);
        put(rt, self + 0x628u, object);
        (void)call_set(rt, ctx, 0x088CEF14u, {object, self, inner, word(rt, self + 0x134u), word(rt, self + 0x14u)});
        (void)call_set(rt, ctx, 0x088CEF38u, {word(rt, self + 0x628u), 0u});
        const std::uint32_t value = call_set(rt, ctx, 0x088D4C50u, {inner});
        (void)call_set(rt, ctx, 0x088CEF58u, {word(rt, self + 0x628u), value});
    }
    if (call_set(rt, ctx, 0x088D62FCu, {inner}) == 4u) {
        const std::uint32_t object = word(rt, self + 0x634u);
        put(rt, self + 0x630u, object);
        (void)call_set(rt, ctx, 0x088E7790u, {object, self, inner, word(rt, self + 0x134u), word(rt, self + 0x14u)});
        (void)call_set(rt, ctx, 0x088E77A4u, {word(rt, self + 0x630u), 0u});
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x088DC32Cu, {self, flag, 1u});
}

// ---- the "resize a vector, then hand each element to an object" family (func_08BED9A0, func_08C0D538, func_08C11D08): self[8] = n, self[0xC] = a vector of elements of 0x30 / 0x40 / 0x60 bytes ----
// The descriptor {vtable, string data pointer (the shared empty string's data 0x08D74FDC), -1, element pointer} at `desc`.
inline void descriptor_init(psprecomp::Runtime &rt, std::uint32_t desc, std::uint32_t vtable, std::uint32_t element) {
    put(rt, desc, vtable);
    put(rt, desc + 4u, 0x08D74FDCu);
    put(rt, desc + 8u, 0xFFFFFFFFu);
    put(rt, desc + 0xCu, element);
}
// The end of a descriptor: its vtable is reset to 0x08CCD990 and its string released: unless the representation (data - 0xC) is the shared empty one, func_08B60804(data - 4, -1) and, when that answers
// zero or less, func_08B8542C(rep, a1). v0 is the empty-case constant, or the last call's.
inline void descriptor_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t desc, std::uint32_t empty_v0, std::uint32_t frame_argument) {
    const std::uint32_t data = word(rt, desc + 4u);
    put(rt, desc, 0x08CCD990u);
    const std::uint32_t rep = data - 0xCu;
    if (rep == 0x08D74FD0u) {
        ctx.gpr[2] = empty_v0;
        return;
    }
    const std::uint32_t count = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(count) > 0) return;
    (void)call_set(rt, ctx, 0x08B8542Cu, {rep, frame_argument});
}
// The element count of a vector {begin, end}: (end - begin) >> shift (arithmetic) times 0xAAAAAAAB, the inverse of 3 (elements of 0x30 or 0x60 bytes), or just the shifted difference (0x40 bytes).
inline std::uint32_t element_count(std::uint32_t begin, std::uint32_t end, unsigned shift, bool times_inverse) {
    const std::uint32_t scaled = static_cast<std::uint32_t>(signed_of(end - begin) >> shift);
    return times_inverse ? scaled * 0xAAAAAAABu : scaled;
}

// func_08BED9A0(self, target): n = self[8], v = self[0xC] (elements of 0x30 bytes). n > 0: a default element t (func_089D76A8(t), t[0] = 0x08CCDE00, four zero words at t + 0x20) resizes v to n (n below the count: func_08BE5A78(v, begin + 48 n, end) erases the tail; else func_08BE6708(v, end, n - count, t) appends copies), t[0] = 0x08CCDE00 and func_089D6684(t);
// then for every i below n the descriptor {0x08CCDE38, the empty string data, -1, v[0] + 48 i} goes to the slot 8 of target's vtable (target, descriptor) and is released. n <= 0: v is made one element long (count below 2: func_08BE6708(v, end, 1 - count, t); else func_08BE5A78(v, begin + 0x30, end)) and func_08975314(descriptor with v[0], target, v[0]) runs once, then the descriptor is released.
// v0 is what the last release leaves: 0x08D74FD0 (loop) / 0x08CCD990 (single) for the empty string, else the last call's.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08BED9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    const std::uint32_t n = word(rt, self + 8u);
    if (signed_of(n) > 0) {
        const std::uint32_t vec = word(rt, self + 0xCu);
        const std::uint32_t temp = frame.at(0x44u);
        (void)call_set(rt, ctx, 0x089D76A8u, {temp});
        put(rt, temp, 0x08CCDE00u);
        put(rt, frame.at(0x64u), 0u);
        put(rt, frame.at(0x68u), 0u);
        put(rt, frame.at(0x6Cu), 0u);
        put(rt, frame.at(0x70u), 0u);
        const std::uint32_t end = word(rt, vec + 4u);
        const std::uint32_t begin = word(rt, vec);
        const std::uint32_t count = element_count(begin, end, 4u, true);
        if (n < count) {
            (void)call_set(rt, ctx, 0x08BE5A78u, {vec, begin + n * 0x30u, end, begin});
        } else {
            (void)call_set(rt, ctx, 0x08BE6708u, {vec, end, n - count, temp});
        }
        put(rt, temp, 0x08CCDE00u);
        (void)call_set(rt, ctx, 0x089D6684u, {temp});
        const std::uint32_t desc = frame.at(4u);
        for (std::uint32_t i = 0u; i != n; ++i) {
            const std::uint32_t vtable = word(rt, target);
            descriptor_init(rt, desc, 0x08CCDE38u, word(rt, word(rt, self + 0xCu)) + 0x30u * i);
            (void)call_set(rt, ctx, word(rt, vtable + 8u), {target, desc, vtable});
            descriptor_release(rt, ctx, desc, 0x08D74FD0u, frame.at(1u));
        }
        return;
    }
    const std::uint32_t vec = word(rt, self + 0xCu);
    const std::uint32_t temp = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x089D76A8u, {temp});
    put(rt, temp, 0x08CCDE00u);
    put(rt, frame.at(0x34u), 0u);
    put(rt, frame.at(0x38u), 0u);
    put(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x40u), 0u);
    const std::uint32_t end = word(rt, vec + 4u);
    const std::uint32_t begin = word(rt, vec);
    const std::uint32_t count = element_count(begin, end, 4u, true);
    if (count >= 2u) {
        (void)call_set(rt, ctx, 0x08BE5A78u, {vec, begin + 0x30u, end});
    } else {
        (void)call_set(rt, ctx, 0x08BE6708u, {vec, end, 1u - count, temp});
    }
    put(rt, temp, 0x08CCDE00u);
    (void)call_set(rt, ctx, 0x089D6684u, {temp});
    const std::uint32_t desc = frame.at(4u);
    const std::uint32_t first = word(rt, word(rt, self + 0xCu));
    descriptor_init(rt, desc, 0x08CCDE38u, first);
    (void)call_set(rt, ctx, 0x08975314u, {desc, target, first});
    descriptor_release(rt, ctx, desc, 0x08CCD990u, frame.at(0u));
}

// func_08C0D538(self, target): the same with elements of 0x40 bytes (count = (end - begin) >> 6): the default element t has the vtable 0x08CCFC90 and two sub-objects at t + 0x20 / t + 0x30 (func_089FA09C each; destroyed after the resize by func_089F9B74 in reverse order),
// the erase is func_08BE5CB8(v, begin + 64 n, end), the append func_08BE8700(v, end, n - count, t), the descriptor vtable 0x08CCFCE0, the element pointer v[0] + 64 i, and for n <= 0 the single call is func_089F8B2C(descriptor, target, v[0]) after v is made one element long (func_08BE8700(v, end, 1 - count, t) when it had fewer than 2, else func_08BE5CB8(v, begin + 0x40, end)).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08C0D538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    const std::uint32_t n = word(rt, self + 8u);
    if (signed_of(n) > 0) {
        const std::uint32_t vec = word(rt, self + 0xCu);
        const std::uint32_t temp = frame.at(0x54u);
        (void)call_set(rt, ctx, 0x089D76A8u, {temp});
        put(rt, temp, 0x08CCFC90u);
        (void)call_set(rt, ctx, 0x089FA09Cu, {frame.at(0x74u)});
        (void)call_set(rt, ctx, 0x089FA09Cu, {frame.at(0x84u)});
        const std::uint32_t end = word(rt, vec + 4u);
        const std::uint32_t begin = word(rt, vec);
        const std::uint32_t count = element_count(begin, end, 6u, false);
        if (n < count) {
            (void)call_set(rt, ctx, 0x08BE5CB8u, {vec, begin + (n << 6u), end});
        } else {
            (void)call_set(rt, ctx, 0x08BE8700u, {vec, end, n - count, temp});
        }
        put(rt, temp, 0x08CCFC90u);
        (void)call_set(rt, ctx, 0x089F9B74u, {frame.at(0x84u)});
        (void)call_set(rt, ctx, 0x089F9B74u, {frame.at(0x74u)});
        (void)call_set(rt, ctx, 0x089D6684u, {temp});
        const std::uint32_t desc = frame.at(4u);
        for (std::uint32_t i = 0u; i != n; ++i) {
            const std::uint32_t vtable = word(rt, target);
            descriptor_init(rt, desc, 0x08CCFCE0u, word(rt, word(rt, self + 0xCu)) + (i << 6u));
            (void)call_set(rt, ctx, word(rt, vtable + 8u), {target, desc, vtable});
            descriptor_release(rt, ctx, desc, 0x08D74FD0u, frame.at(1u));
        }
        return;
    }
    const std::uint32_t vec = word(rt, self + 0xCu);
    const std::uint32_t temp = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x089D76A8u, {temp});
    put(rt, temp, 0x08CCFC90u);
    (void)call_set(rt, ctx, 0x089FA09Cu, {frame.at(0x34u)});
    (void)call_set(rt, ctx, 0x089FA09Cu, {frame.at(0x44u)});
    const std::uint32_t end = word(rt, vec + 4u);
    const std::uint32_t begin = word(rt, vec);
    const std::uint32_t count = element_count(begin, end, 6u, false);
    if (count >= 2u) {
        (void)call_set(rt, ctx, 0x08BE5CB8u, {vec, begin + 0x40u, end});
    } else {
        (void)call_set(rt, ctx, 0x08BE8700u, {vec, end, 1u - count, temp});
    }
    put(rt, temp, 0x08CCFC90u);
    (void)call_set(rt, ctx, 0x089F9B74u, {frame.at(0x44u)});
    (void)call_set(rt, ctx, 0x089F9B74u, {frame.at(0x34u)});
    (void)call_set(rt, ctx, 0x089D6684u, {temp});
    const std::uint32_t desc = frame.at(4u);
    const std::uint32_t first = word(rt, word(rt, self + 0xCu));
    descriptor_init(rt, desc, 0x08CCFCE0u, first);
    (void)call_set(rt, ctx, 0x089F8B2Cu, {desc, target, first});
    descriptor_release(rt, ctx, desc, 0x08CCD990u, frame.at(0u));
}

// func_08C11D08(self, target): the same with elements of 0x60 bytes (count = ((end - begin) >> 5) * 0xAAAAAAAB): the default element t has the vtable 0x08CCFF88, t[0x30] = t[0x50] = -1 and 1.0f at t + 0x20 / 0x24 / 0x28 / 0x2C / 0x40 / 0x44 / 0x48 / 0x4C, the erase is func_08BE6244(v, begin + 96 n, end), the append func_08BE7DCC(v, end, n - count, t),
// the descriptor vtable 0x08CCFFC0 with element v[0] + 96 i, and for n <= 0 the single call is func_08A05D3C(descriptor, target, v[0]).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08C11D08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x110u);
    const auto fill_default = [&](std::uint32_t temp) {
        (void)call_set(rt, ctx, 0x089D76A8u, {temp});
        put_float(rt, temp + 0x40u, 1.0f);
        put(rt, temp, 0x08CCFF88u);
        put(rt, temp + 0x50u, 0xFFFFFFFFu);
        put(rt, temp + 0x30u, 0xFFFFFFFFu);
        put_float(rt, temp + 0x2Cu, 1.0f);
        put_float(rt, temp + 0x28u, 1.0f);
        put_float(rt, temp + 0x24u, 1.0f);
        put_float(rt, temp + 0x20u, 1.0f);
        put_float(rt, temp + 0x4Cu, 1.0f);
        put_float(rt, temp + 0x48u, 1.0f);
        put_float(rt, temp + 0x44u, 1.0f);
    };
    const std::uint32_t n = word(rt, self + 8u);
    if (signed_of(n) > 0) {
        const std::uint32_t vec = word(rt, self + 0xCu);
        const std::uint32_t temp = frame.at(0x80u);
        fill_default(temp);
        const std::uint32_t end = word(rt, vec + 4u);
        const std::uint32_t begin = word(rt, vec);
        const std::uint32_t count = element_count(begin, end, 5u, true);
        if (n < count) {
            (void)call_set(rt, ctx, 0x08BE6244u, {vec, begin + n * 0x60u, end, begin});
        } else {
            (void)call_set(rt, ctx, 0x08BE7DCCu, {vec, end, n - count, temp});
        }
        put(rt, temp, 0x08CCFF88u);
        (void)call_set(rt, ctx, 0x089D6684u, {temp});
        const std::uint32_t desc = frame.at(4u);
        for (std::uint32_t i = 0u; i != n; ++i) {
            const std::uint32_t vtable = word(rt, target);
            descriptor_init(rt, desc, 0x08CCFFC0u, word(rt, word(rt, self + 0xCu)) + 0x60u * i);
            (void)call_set(rt, ctx, word(rt, vtable + 8u), {target, desc, vtable});
            descriptor_release(rt, ctx, desc, 0x08D74FD0u, frame.at(1u));
        }
        return;
    }
    const std::uint32_t vec = word(rt, self + 0xCu);
    const std::uint32_t temp = frame.at(0x20u);
    fill_default(temp);
    const std::uint32_t end = word(rt, vec + 4u);
    const std::uint32_t begin = word(rt, vec);
    const std::uint32_t count = element_count(begin, end, 5u, true);
    if (count >= 2u) {
        (void)call_set(rt, ctx, 0x08BE6244u, {vec, begin + 0x60u, end});
    } else {
        (void)call_set(rt, ctx, 0x08BE7DCCu, {vec, end, 1u - count, temp});
    }
    put(rt, temp, 0x08CCFF88u);
    (void)call_set(rt, ctx, 0x089D6684u, {temp});
    const std::uint32_t desc = frame.at(4u);
    const std::uint32_t first = word(rt, word(rt, self + 0xCu));
    descriptor_init(rt, desc, 0x08CCFFC0u, first);
    (void)call_set(rt, ctx, 0x08A05D3Cu, {desc, target, first});
    descriptor_release(rt, ctx, desc, 0x08CCD990u, frame.at(0u));
}

// func_088A1D84(self, a1 = index, a2, a3, t0 = b0, t1 = b1, t2 = source, t3): o = self[8] + (int16)(self[0x10] + 2 * index) * 0x430; sp[0] = a2, sp[4] = t3; the slot 0x7C of o's vtable (o) answering 0 ends the call (v0 = 0). Otherwise e = self[0x4C] + h * 0x690 (h the halfword), mode m = (b0 ? 4 : 0) and, when `source` is not null,
// m = 2 if b0 is clear and the slot 0x60 of source answers non-zero, k = func_088970D4(source + 0x20). func_0894D2E0(self[0x54], index, m, b1, e, k, self[0x20][index]), func_0894D484(self[0x54], index, a2, a3, 0), c = func_0894CFF8(self[0x54], index), the four floats of func_0895523C / 08955248 / 08955254 / 08955260 and the fifth of func_0895526C (c) go with 0.2f (0x3E4CCCCD)
// to func_088AF2C8(self[0x50], index; f12..f17), the slot 0x10 of self[0x50] (index), the slot 0xDC of o (a2, a3), the slot 0x30 of o (0), r = func_088AF28C(self[0x50], index): o[0x1E4] = r, r[0x3FC] = c, r[0x3F8] = b1 (index when b1 == 1), func_0894D5A0(self[0x54], index), func_088AF2E0(self[0x50], index), func_088A386C(o, t3, index), func_088A3980(o, index, self + 4) and the tail call func_088A3718(o).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088A1D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t arg2 = ctx.gpr[6];
    const std::uint32_t arg3 = ctx.gpr[7];
    const std::uint32_t b0 = ctx.gpr[8] & 0xFFu;
    const std::uint32_t b1 = ctx.gpr[9] & 0xFFu;
    const std::uint32_t source = ctx.gpr[10];
    const std::uint32_t arg_t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t halfword = sext16(half_u(rt, word(rt, self + 0x10u) + (index << 1u)));
    const std::uint32_t object = word(rt, self + 8u) + halfword * 0x430u;
    put(rt, frame.at(0u), arg2);
    put(rt, frame.at(4u), arg_t3);
    if (call_set(rt, ctx, slot_of(rt, object, 0x7Cu), {object}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = word(rt, self + 0x4Cu) + halfword * 0x690u;
    std::uint32_t mode;
    std::uint32_t extra = 0u;
    if (b0 != 0u) {
        mode = 4u;
        if (source != 0u) extra = call_set(rt, ctx, 0x088970D4u, {source + 0x20u});
    } else if (source == 0u) {
        mode = 0u;
    } else {
        mode = call_set(rt, ctx, slot_of(rt, source, 0x60u), {source}) != 0u ? 2u : 0u;
        extra = call_set(rt, ctx, 0x088970D4u, {source + 0x20u});
    }
    const std::uint32_t grid = word(rt, self + 0x54u);
    (void)call_set(rt, ctx, 0x0894D2E0u, {grid, index, mode, b1, entry, extra, word(rt, word(rt, self + 0x20u) + (index << 2u))});
    (void)call_set(rt, ctx, 0x0894D484u, {word(rt, self + 0x54u), index, word(rt, frame.at(0u)), arg3, 0u});
    const std::uint32_t block = call_set(rt, ctx, 0x0894CFF8u, {word(rt, self + 0x54u), index});
    const std::uint32_t layer = word(rt, self + 0x50u);
    const float f23 = call_keep(rt, ctx, 0x0895523Cu, {block}, {}).f0;
    const float f22 = call_keep(rt, ctx, 0x08955248u, {block}, {}).f0;
    const float f21 = call_keep(rt, ctx, 0x08955254u, {block}, {}).f0;
    const float f20 = call_keep(rt, ctx, 0x08955260u, {block}, {}).f0;
    const float f17 = call_keep(rt, ctx, 0x0895526Cu, {block}, {}).f0;
    (void)call_setf(rt, ctx, 0x088AF2C8u, {layer, index}, {f23, f22, f21, std::bit_cast<float>(0x3E4CCCCDu), f20, f17});
    const std::uint32_t layer2 = word(rt, self + 0x50u);
    (void)call_set(rt, ctx, slot_of(rt, layer2, 0x10u), {layer2, index});
    (void)call_set(rt, ctx, slot_of(rt, object, 0xDCu), {object, word(rt, frame.at(0u)), arg3});
    (void)call_set(rt, ctx, slot_of(rt, object, 0x30u), {object, 0u});
    const std::uint32_t record = call_set(rt, ctx, 0x088AF28Cu, {word(rt, self + 0x50u), index});
    put(rt, object + 0x1E4u, record);
    put(rt, record + 0x3FCu, block);
    put(rt, record + 0x3F8u, b1);
    if (b1 == 1u) put(rt, record + 0x3F8u, index);
    (void)call_set(rt, ctx, 0x0894D5A0u, {word(rt, self + 0x54u), index});
    (void)call_set(rt, ctx, 0x088AF2E0u, {word(rt, self + 0x50u), index});
    (void)call_set(rt, ctx, 0x088A386Cu, {object, word(rt, frame.at(4u)), index});
    (void)call_set(rt, ctx, 0x088A3980u, {object, index, self + 4u});
    tail_call(ctx, 0x088A3718u, {object, index, self + 4u});
}

// The release of a counted block (the asm's ll / sc pairs, one thread: a plain load and store; v0 = 1 and a0 = the old count after each): the strong count at c[4] is decremented and, when it was 1, the virtual slot 8 of c runs; then the weak count at c[8] is decremented and, when that was 1, the slot 0xC.
inline void release_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t counted) {
    std::uint32_t old = word(rt, counted + 4u);
    put(rt, counted + 4u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    (void)call_set(rt, ctx, slot_of(rt, counted, 8u), {counted});
    old = word(rt, counted + 8u);
    put(rt, counted + 8u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    (void)call_set(rt, ctx, slot_of(rt, counted, 0xCu), {counted});
}

// func_0889ED00(self): a destructor. self[0] = 0x08CC2420; the object p = self[0x34], when not null, gets the slot 0 of its vtable (p, self[0x30]) and self[0x34] = self[0x30] = 0; func_0889A4F0(self + 0x28); m = self[0x24] (a mutex-like record): func_08B34888(m); m[0x2C] == 0: func_08B34B24(m), the object q = m[0x38] (when not null) gets the slot 0 (q, m[0x34]) and m[0x34] = m[0x38] = 0,
// func_08B28BE0(m), func_08B64654(m); else m[0x2C] is decremented and, when it reaches 0, a pending pair (m[0x34], m[0x38]) is taken out (both cleared) and func_08B34C1C(m) runs when the byte m[0x30] is set; func_08B34B24(m); the taken pair's slot 8 / slot 0 (when not null) run. Then the counted blocks self[0x20], self[0x18], self[0x10] and self[8] are released (release_counted), self[0] = 0x08CC24F0 and
// the tail call func_08B64654(self).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_0889ED00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CC2420u);
    const std::uint32_t listener = word(rt, self + 0x34u);
    if (listener != 0u) {
        (void)call_set(rt, ctx, slot_of(rt, listener, 0u), {listener, word(rt, self + 0x30u)});
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x30u, 0u);
    }
    (void)call_set(rt, ctx, 0x0889A4F0u, {self + 0x28u});
    const std::uint32_t record = word(rt, self + 0x24u);
    (void)call_set(rt, ctx, 0x08B34888u, {record});
    if (word(rt, record + 0x2Cu) == 0u) {
        (void)call_set(rt, ctx, 0x08B34B24u, {record});
        const std::uint32_t pending = word(rt, record + 0x38u);
        if (pending != 0u) {
            (void)call_set(rt, ctx, slot_of(rt, pending, 0u), {pending, word(rt, record + 0x34u)});
            put(rt, record + 0x38u, 0u);
            put(rt, record + 0x34u, 0u);
        }
        (void)call_set(rt, ctx, 0x08B28BE0u, {record});
        (void)call_set(rt, ctx, 0x08B64654u, {record});
    } else {
        const std::uint32_t remaining = word(rt, record + 0x2Cu) - 1u;
        put(rt, record + 0x2Cu, remaining);
        std::uint32_t first = 0u, second = 0u;
        if (word(rt, record + 0x2Cu) == 0u) {
            const std::uint32_t pair = word(rt, record + 0x34u);
            if (pair != 0u) {
                second = word(rt, record + 0x38u);
                first = pair;
                put(rt, record + 0x34u, 0u);
                put(rt, record + 0x38u, 0u);
            }
            if (byte_u(rt, record + 0x30u) != 0u) (void)call_set(rt, ctx, 0x08B34C1Cu, {record});
        }
        (void)call_set(rt, ctx, 0x08B34B24u, {record});
        if (first != 0u) (void)call_set(rt, ctx, slot_of(rt, first, 8u), {first});
        if (second != 0u) (void)call_set(rt, ctx, slot_of(rt, second, 0u), {second, first});
    }
    for (const std::uint32_t offset : {0x20u, 0x18u, 0x10u, 8u}) {
        const std::uint32_t counted = word(rt, self + offset);
        if (counted != 0u) release_counted(rt, ctx, counted);
    }
    put(rt, self, 0x08CC24F0u);
    tail_call(ctx, 0x08B64654u, {self});
}

// func_0896E380(out = a0, a1, mode = a2, source = a3): the holder A = sp[0x10] is made by func_08969244(sp + 0x10). mode == 0: func_0896B410(sp + 0x18, A') fills four floats from the object A' = A (null: func_08A5BB7C(0x08CA2880) first), the holder B = sp[0x14] is made by func_08A98C1C(sp + 0x14, 4) and its four slots B[0x14] + 0 / 4 / 8 / 0xC get each a value
// made by func_08A87EEC(holder; the float sp[0x18 / 0x1C / 0x20 / 0x24]) (assigned with func_08A5BAD8(slot, value) unless the slot is the value's own address, the temporary then destroyed with func_08A5B9EC), and B itself goes to out (func_08A5BAD8(out, B) unless out == sp + 0x14).
// mode == 1: B is made by func_08A98674(sp + 0x14, source); four floats are read from the slots 0x54 of the objects B[0x14][0], [4], [8], [0xC] (each null one calls func_08A5BB7C(0x08CA2878) first) into sp[0x18..0x24] and func_0896B4D4(A', sp + 0x18) runs. Any other mode does nothing more. B (modes 0 and 1) and A are destroyed (func_08A5B9EC) at the end; v0 is the last destroy's.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_0896E380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t arg1 = ctx.gpr[5];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t holder_a = frame.at(0x10u);
    const std::uint32_t holder_b = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x08969244u, {holder_a, arg1});
    if (mode == 0u) {
        std::uint32_t object = word(rt, holder_a);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
        (void)call_set(rt, ctx, 0x0896B410u, {frame.at(0x18u), object});
        (void)call_set(rt, ctx, 0x08A98C1Cu, {holder_b, 4u});
        for (std::uint32_t i = 0u; i != 4u; ++i) {
            const std::uint32_t temp = i == 0u ? frame.at(0xCu) : i == 1u ? frame.at(8u) : i == 2u ? frame.at(4u) : frame.at(0u);
            const float f12 = float_at(rt, frame.at(0x18u + 4u * i));
            const std::uint32_t slot = word(rt, word(rt, holder_b) + 0x14u) + 4u * i;
            (void)call_setf(rt, ctx, 0x08A87EECu, {temp}, {f12});
            if (slot != temp) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
        }
        if (out != holder_b) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, holder_b)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder_b});
    } else if (mode == 1u) {
        (void)call_set(rt, ctx, 0x08A98674u, {holder_b, source});
        for (std::uint32_t i = 0u; i != 4u; ++i) put(rt, frame.at(0x18u + 4u * i), 0u);
        for (std::uint32_t i = 0u; i != 4u; ++i) {
            const std::uint32_t object = word(rt, word(rt, word(rt, holder_b) + 0x14u) + 4u * i);
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});
            const GuestResult result = call_keep(rt, ctx, slot_of(rt, object, 0x54u), {object}, {});
            put_float(rt, frame.at(0x18u + 4u * i), result.f0);
        }
        const std::uint32_t object = word(rt, holder_a);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
        (void)call_set(rt, ctx, 0x0896B4D4u, {object, frame.at(0x18u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder_b});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder_a});
}

// func_08A539AC(self, a1 = fp): n = self[0x4C] (the size of the red-black tree whose header is self + 0x3C, first node self[0x44]); a vector {sp[0xC] begin, sp[0x10] end, sp[0x14] capacity} of n holders is allocated (func_08A5BC20(4 n, 4), filled by func_08C1D3F0 with n copies of a holder made by func_08A5BAB8(sp + 4, 0x08CF6ED8)); every node's value
// object o = node[0x14] (null: func_08A5BB7C(0x08CAF594)) is stored into the slot vector[o[0x14]] by func_08A5BAD8 (unless the slot is the node's own address), the tree walked with func_08B5F16C. self[0x54] = 1; then for every slot i the holder h = sp[8] made by func_08A52A98(sp + 8, slot_object + 0xC) (null: func_08A5BB7C(0x08CAF548))
// gives slot_object[0x10] = func_08A527E8(h, c), c = the slot 0x15C of h (h, c) and self[0x54] = max(self[0x54], the slot 0x124 of h) in the signed sense; h is destroyed (func_08A5B9EC) each round. Then self[0x50] = func_08A527E8(self, c), r = func_08A52824(self, fp), func_08C1D3AC(begin, end, sp) and the vector's block is freed (func_08A5BC54(begin, capacity bytes, 4)); v0 = r.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08A539AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t fp = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t n = word(rt, self + 0x4Cu);
    std::uint32_t node = word(rt, self + 0x44u);
    const std::uint32_t header = self + 0x3Cu;
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    const std::uint32_t bytes = n << 2u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    put(rt, frame.at(0x14u), block + bytes);
    put(rt, frame.at(0xCu), block);
    put(rt, frame.at(0x10u), block);
    (void)call_set(rt, ctx, 0x08A5BAB8u, {frame.at(4u), 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x08C1D3F0u, {block, n, frame.at(4u), frame.at(1u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    put(rt, frame.at(0x10u), bytes + word(rt, frame.at(0xCu)));
    while (node != header) {
        const std::uint32_t object = word(rt, node + 0x14u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
        const std::uint32_t slot = word(rt, frame.at(0xCu)) + (word(rt, object + 0x14u) << 2u);
        if (slot != node + 0x14u) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, node + 0x14u)});
        node = call_set(rt, ctx, 0x08B5F16Cu, {node});
    }
    put(rt, self + 0x54u, 1u);
    std::uint32_t accumulated = 0u;
    if (signed_of(n) > 0) {
        for (std::uint32_t i = 0u; i != n; ++i) {
            const std::uint32_t slot_object = word(rt, word(rt, frame.at(0xCu)) + (i << 2u));
            if (slot_object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            (void)call_set(rt, ctx, 0x08A52A98u, {frame.at(8u), slot_object + 0xCu});
            const std::uint32_t target = word(rt, word(rt, frame.at(0xCu)) + (i << 2u));
            if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            std::uint32_t holder = word(rt, frame.at(8u));
            if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF548u});
            const std::uint32_t value = call_set(rt, ctx, 0x08A527E8u, {holder, accumulated});
            put(rt, target + 0x10u, value);
            holder = word(rt, frame.at(8u));
            if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF548u});
            accumulated = call_set(rt, ctx, slot_of(rt, holder, 0x15Cu), {holder, accumulated});
            holder = word(rt, frame.at(8u));
            if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF548u});
            const std::uint32_t measure = call_set(rt, ctx, slot_of(rt, holder, 0x124u), {holder});
            if (signed_of(word(rt, self + 0x54u)) < signed_of(measure)) {
                holder = word(rt, frame.at(8u));
                if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF548u});
                put(rt, self + 0x54u, call_set(rt, ctx, slot_of(rt, holder, 0x124u), {holder}));
            }
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        }
    }
    put(rt, self + 0x50u, call_set(rt, ctx, 0x08A527E8u, {self, accumulated}));
    const std::uint32_t result = call_set(rt, ctx, 0x08A52824u, {self, fp});
    (void)call_set(rt, ctx, 0x08C1D3ACu, {word(rt, frame.at(0xCu)), word(rt, frame.at(0x10u)), frame.at(0u)});
    const std::uint32_t begin = word(rt, frame.at(0xCu));
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, frame.at(0x14u)) - begin) >> 2);
    if (begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {begin, capacity << 2u, 4u});
    ctx.gpr[2] = result;
}

// func_088C4BB0(self): level = func_088C49FC(self) (an unsigned count, 0..100); p = self[0x24]; x = |self[8][0x10] + self[8][0x14]| (the sum of two floats, negated when below 0), frac = x - (float)trunc(x). When the halfword p[0x48] & 0x19F is set: a = func_088C4A40(self) and the level steps up (by one while below 100) when
// frac < a * ((float)(100 - level) * 0.01f); else a = func_088C4A90(self) and the level steps down (by one while above 0) when frac < a * ((float)level * 0.01f). A set byte p[0xF] or p[0x6D] then clears the level; the tail call func_088C4BA4(self, level) ends it.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088C4BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t level = call_set(rt, ctx, 0x088C49FCu, {self});
    const std::uint32_t track = word(rt, self + 8u);
    float x = float_at(rt, track + 0x10u) + float_at(rt, track + 0x14u);
    const std::uint32_t p = word(rt, self + 0x24u);
    if (x < 0.0f) x = -x;
    const float whole = static_cast<float>(signed_of(ctx.fpu_float_to_word_ct<1u>(x)));
    const float frac = x - whole;
    const std::uint32_t flags = half_u(rt, p + 0x48u) & 0x19Fu;
    if (flags != 0u) {
        const float a = call_keep(rt, ctx, 0x088C4A40u, {self}, {}).f0;
        const float rest = static_cast<float>(0x64u - level);
        const float limit = psp_mul(a, psp_mul(rest, std::bit_cast<float>(0x3C23D70Au)));
        if (frac < limit) {
            if (level < 0x64u) ++level;
        }
    } else {
        const float a = call_keep(rt, ctx, 0x088C4A90u, {self}, {}).f0;
        const float have = static_cast<float>(level);
        const float limit = psp_mul(a, psp_mul(have, std::bit_cast<float>(0x3C23D70Au)));
        if (frac < limit) level -= (level != 0u) ? 1u : 0u;
    }
    if (byte_u(rt, p + 0xFu) != 0u || byte_u(rt, p + 0x6Du) != 0u) level = 0u;
    tail_call(ctx, 0x088C4BA4u, {self, level});
}

// func_088C50D4(table, start = a1; f12 = x, f13 = w): a search in the table whose count is table[0] and whose rows (64 bytes each, base table[4], read by the leaf func_088C50C4(table, i) = base + 64 i, inlined here) hold a float at +0x14; half = w * 0.5f. The answer is an index (the leaf's calls change no memory).
// start > 0 and start < count - 1: x >= row(start) gives start + 1; x < row(start - 1) gives start - 1; else start. start >= count - 1: x >= row(count - 1) or x < half gives 0; x < row(count - 2) gives start - 1; else start.
// start < 0: the first row i (from 0, up to the count) with x < row(i), else 0 (0 for an empty table). start == 0: half <= x: x <= half gives 0, else row(count - 1) <= x gives 0 and otherwise count - 1; half > x: x < row(0) gives 0 else 1.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088C50D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = ctx.gpr[4];
    const std::uint32_t start = ctx.gpr[5];
    const float x = ctx.fpr[12];
    const float half = psp_mul(ctx.fpr[13], std::bit_cast<float>(0x3F000000u));
    const std::uint32_t count = word(rt, table);
    const auto row = [&](std::uint32_t index) { return float_at(rt, word(rt, table + 4u) + (index << 6u) + 0x14u); };
    std::uint32_t result;
    if (signed_of(start) > 0) {
        const std::uint32_t last = count - 1u;
        if (signed_of(start) < signed_of(last)) {
            if (row(start) <= x) {
                result = start + 1u;
            } else if (x < row(start - 1u)) {
                result = start - 1u;
            } else {
                result = start;
            }
        } else if (row(last) <= x) {
            result = 0u;
        } else if (x < half) {
            result = 0u;
        } else if (x < row(count - 2u)) {
            result = start - 1u;
        } else {
            result = start;
        }
    } else if (start != 0u) {
        result = 0u;
        if (signed_of(count) > 0) {
            for (std::uint32_t i = 0u;;) {
                if (x < row(i)) {
                    result = i;
                    break;
                }
                ++i;
                if (i == count) break;
            }
        }
    } else if (half <= x) {
        if (x <= half) {
            result = 0u;
        } else {
            result = row(count - 1u) <= x ? 0u : count - 1u;
        }
    } else {
        result = x < row(0u) ? 0u : 1u;
    }
    ctx.gpr[2] = result;
}

// func_08960D4C(self): for i = 0..3, A = func_08955300(self, i): s = |A[0x30] * 0.1f|, k = (2.0f < s) ? 1.0f : s - 1.0f, g = func_08B1ECFC(f12 = 1.0f) (a float function whose v0 is the one this function returns), then with x = A[0xF8], y = A[0xFC]:
// t = ((((k + 1.0f) * g * 2 - 1.0f) - x) * 500.0f + y * -10.0f) * 0.016668f (0x3C88AB7C); y' = y + t; x' = x + y' * 0.016668f. x' above 1.0f is 1.0f and y' becomes 0.0f; x' below -1.0f is -1.0f and y' becomes 0.0f; A[0xF8] = x', A[0xFC] = y'.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08960D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (std::uint32_t i = 0u; i != 4u; ++i) {
        const std::uint32_t a = call_set(rt, ctx, 0x08955300u, {self, i});
        float f0 = psp_mul(float_at(rt, a + 0x30u), std::bit_cast<float>(0x3DCCCCCDu));
        const float x = float_at(rt, a + 0xF8u);
        const float y = float_at(rt, a + 0xFCu);
        float k = 1.0f;
        if (f0 < 0.0f) f0 = -f0;
        if (!(2.0f < f0)) k = f0 - 1.0f;
        const float g = call_keep(rt, ctx, 0x08B1ECFCu, {}, {1.0f}).f0;
        float f2 = k + 1.0f;
        const float f1 = psp_mul(y, std::bit_cast<float>(0xC1200000u));
        f2 = psp_mul(f2, g);
        f2 = f2 + f2;
        f2 = f2 - 1.0f;
        f2 = f2 - x;
        f2 = psp_mul(f2, std::bit_cast<float>(0x43FA0000u));
        f2 = f2 + f1;
        f2 = psp_mul(f2, std::bit_cast<float>(0x3C88AB7Cu));
        float new_y = y + f2;
        float new_x = x + psp_mul(new_y, std::bit_cast<float>(0x3C88AB7Cu));
        if (1.0f < new_x) {
            new_x = 1.0f;
            new_y = 0.0f;
        } else if (new_x < -1.0f) {
            new_x = -1.0f;
            new_y = 0.0f;
        }
        put_float(rt, a + 0xFCu, new_y);
        put_float(rt, a + 0xF8u, new_x);
    }
}

// func_08977904(self, a1 = pair, a2 = side, a3 = steps): f20 = func_089771F4(self, pair, side ? 0 : 2) and f12 = func_089771F4(self, pair, side ? 1 : 3); the length L = sqrt(f20^2 + f12^2), both divided by L when L > 0.
// angle = func_08B66FBC(f12, f13 = f20) * 180 / 3.14159274 + 180 + (8 / steps) * 22.5, divided by (8 / steps) * 45 and truncated; the answer is (8 / steps) * that when steps != that, else 0. The float at self + ((pair * 2 + side) * 4) + 0x14C is replaced by L; when L > 0.5f: an old value below 0.5f runs func_08977390(self, pair) and sets the byte self[pair * 2 + side + 0x14A] = 1; otherwise a non-zero byte b there steps to (b + 1) & 0xFF (wrapping to the halfword self[0x174] when it
// reaches self[0x174] + self[0x176]), the answer being -1 when b differs from self[0x174]. L <= 0.5f clears the byte and the answer is -1. v0 = the answer.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08977904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t pair = ctx.gpr[5];
    const std::uint32_t side = ctx.gpr[6];
    const std::uint32_t steps = ctx.gpr[7];
    (void)call_set(rt, ctx, 0x089771F4u, {self, pair, side != 0u ? 0u : 2u});
    float f20 = ctx.fpr[0];
    (void)call_set(rt, ctx, 0x089771F4u, {self, pair, side != 0u ? 1u : 3u});
    float f12 = ctx.fpr[0];
    float f0 = psp_mul(f20, f20);
    const float f1 = psp_mul(f12, f12);
    f0 = f0 + f1;
    const float f21 = std::sqrt(f0);
    if (0.0f < f21) {
        f20 = f20 / f21;
        f12 = f12 / f21;
    }
    const std::uint32_t quotient = steps == 0u ? 0xFFFFFFFFu : static_cast<std::uint32_t>(8 / signed_of(steps));
    std::uint32_t result = 0u;
    f0 = call_setf(rt, ctx, 0x08B66FBCu, {}, {f12, f20}).f0;
    f0 = psp_mul(f0, std::bit_cast<float>(0x43340000u));
    f0 = f0 / std::bit_cast<float>(0x40490FDBu);
    const float f3 = static_cast<float>(signed_of(quotient));
    float f1b = psp_mul(f3, std::bit_cast<float>(0x41B40000u));
    f0 = f0 + std::bit_cast<float>(0x43340000u);
    const float f3b = psp_mul(f3, std::bit_cast<float>(0x42340000u));
    f0 = f0 + f1b;
    f0 = f0 / f3b;
    const std::uint32_t step_index = ctx.fpu_float_to_word_ct<1u>(f0);
    if (steps != step_index) result = quotient * step_index;
    const std::uint32_t half = pair << 1u;
    const std::uint32_t cell = self + ((half + side) << 2u);
    const float old = float_at(rt, cell + 0x14Cu);
    put_float(rt, cell + 0x14Cu, f21);
    const std::uint32_t flag = self + half + side;
    if (std::bit_cast<float>(0x3F000000u) < f21) {
        if (old < std::bit_cast<float>(0x3F000000u)) {
            (void)call_set(rt, ctx, 0x08977390u, {self, pair});
            put8(rt, flag + 0x14Au, 1u);
        } else {
            const std::uint32_t current = byte_u(rt, flag + 0x14Au);
            if (current != 0u) {
                const std::uint32_t base = sext16(half_u(rt, self + 0x174u));
                const std::uint32_t span = sext16(half_u(rt, self + 0x176u));
                const std::uint32_t next = (current + 1u) & 0xFFu;
                if ((current ^ base) != 0u) result = 0xFFFFFFFFu;
                if (signed_of(next) < signed_of(base + span)) {
                    put8(rt, flag + 0x14Au, next);
                } else {
                    put8(rt, flag + 0x14Au, base);
                }
            }
        }
    } else {
        put8(rt, flag + 0x14Au, 0u);
        result = 0xFFFFFFFFu;
    }
    ctx.gpr[2] = result;
}

// func_089C54B8(self): self[0x25C] <= 0: v0 = self[0x18C]; when that is above 0 (signed) func_08AFAC7C(self + 0x188) runs and v0 is its answer. Otherwise o = func_08AFAA04(self), (w, h) = (o[0x18], o[0x1C]): func_08AFAC7C(self + 0x188), func_08AFAD8C(self + 0x188, w, h, o[0xC], 0, 0), the slot 0x24 of o fills 0x88 bytes at sp + 0x90 (a0 = sp + 0x90, a1 = o), which are copied to sp + 0 and given to func_08B52578(sp);
// func_08B58874(2), func_08B54FB8(0x6D), func_08B5A838(1.0f x4), func_08AFA5C0(self + 0x188), func_089C5394(f12..f19 = 0, 0, (float)w, (float)h, 0, 0, (float)w, (float)h), func_08B58874(0x6D), func_08B54FB8(2), the slot 0x18 of o, q = func_08AFAA78(self + 0x188, 0) and q[8] = (float)(int)self[0x18C] * 2^-9, q[0xC] = (float)(int)self[0x190] * 2^-9; v0 = q.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_089C54B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x140u);
    if (signed_of(word(rt, self + 0x25Cu)) <= 0) {
        const std::uint32_t pending = word(rt, self + 0x18Cu);
        ctx.gpr[2] = pending;
        if (signed_of(pending) > 0) (void)call_set(rt, ctx, 0x08AFAC7Cu, {self + 0x188u});
        return;
    }
    const std::uint32_t block = self + 0x188u;
    const std::uint32_t object = call_set(rt, ctx, 0x08AFAA04u, {self});
    const std::uint32_t width = word(rt, object + 0x18u);
    const std::uint32_t height = word(rt, object + 0x1Cu);
    (void)call_set(rt, ctx, 0x08AFAC7Cu, {block});
    (void)call_set(rt, ctx, 0x08AFAD8Cu, {block, width, height, word(rt, object + 0xCu), 0u, 0u});
    (void)call_set(rt, ctx, slot_of(rt, object, 0x24u), {frame.at(0x90u), object});
    for (std::uint32_t offset = 0u; offset != 0x88u; offset += 4u) put(rt, frame.at(offset), word(rt, frame.at(0x90u + offset)));
    (void)call_set(rt, ctx, 0x08B52578u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08B58874u, {2u});
    (void)call_set(rt, ctx, 0x08B54FB8u, {0x6Du});
    (void)call_setf(rt, ctx, 0x08B5A838u, {}, {1.0f, 1.0f, 1.0f, 1.0f});
    (void)call_set(rt, ctx, 0x08AFA5C0u, {block});
    const float fw = static_cast<float>(signed_of(width));
    const float fh = static_cast<float>(signed_of(height));
    (void)call_setf(rt, ctx, 0x089C5394u, {}, {0.0f, 0.0f, fw, fh, 0.0f, 0.0f, fw, fh});
    (void)call_set(rt, ctx, 0x08B58874u, {0x6Du});
    (void)call_set(rt, ctx, 0x08B54FB8u, {2u});
    (void)call_set(rt, ctx, slot_of(rt, object, 0x18u), {object});
    const std::uint32_t q = call_set(rt, ctx, 0x08AFAA78u, {block, 0u});
    const float scale = std::bit_cast<float>(0x3B000000u);
    put_float(rt, q + 8u, psp_mul(static_cast<float>(signed_of(word(rt, block + 4u))), scale));
    put_float(rt, q + 0xCu, psp_mul(static_cast<float>(signed_of(word(rt, block + 8u))), scale));
}

// func_08AFD0EC(self, a1, a2, a3, t0; f12 = x, f13 = y, f14 = a, f15 = b, f16 = c, f17 = d): self[0] = 0 when x == 0, y == 0 and a, b, c, d are all 1.0f, else 1 (identity test). func_08AFCE94(self + 4, flag ? a1 : 0), then the same call for self + 0x20 / 0x3C / 0x58 with a2 / a3 / t0 (0 when the flag is 0).
// func_08AFCED4(self + 4 and self + 0x20; f12..f15 = x, y, a, b), func_08AFCF50(self + 0x3C; a * c, b * d) and the tail call func_08AFCF50(self + 0x58; a * c, b * d).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08AFD0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    const std::uint32_t a2 = ctx.gpr[6];
    const std::uint32_t a3 = ctx.gpr[7];
    const std::uint32_t t0 = ctx.gpr[8];
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    const float a = ctx.fpr[14];
    const float b = ctx.fpr[15];
    const float c = ctx.fpr[16];
    const float d = ctx.fpr[17];
    const float ac = psp_mul(a, c);
    const float bd = psp_mul(b, d);
    const bool identity = x == 0.0f && y == 0.0f && a == 1.0f && b == 1.0f && c == 1.0f && d == 1.0f;
    const std::uint32_t flag = identity ? 0u : 1u;
    put8(rt, self, flag);
    (void)call_set(rt, ctx, 0x08AFCE94u, {self + 4u, flag != 0u ? a1 : 0u});
    (void)call_set(rt, ctx, 0x08AFCE94u, {self + 0x20u, flag != 0u ? a2 : 0u});
    (void)call_set(rt, ctx, 0x08AFCE94u, {self + 0x3Cu, flag != 0u ? a3 : 0u});
    (void)call_set(rt, ctx, 0x08AFCE94u, {self + 0x58u, flag != 0u ? t0 : 0u});
    (void)call_setf(rt, ctx, 0x08AFCED4u, {self + 4u}, {x, y, a, b});
    (void)call_setf(rt, ctx, 0x08AFCED4u, {self + 0x20u}, {x, y, a, b});
    (void)call_setf(rt, ctx, 0x08AFCF50u, {self + 0x3Cu}, {ac, bd});
    ctx.fpr[12] = ac;
    ctx.fpr[13] = bd;
    tail_call(ctx, 0x08AFCF50u, {self + 0x58u});
}

// func_088A0628(self, a1): p = self[8], q = self[0xC] (two records with a four-float vector at +0x34), r = self[0x10], s = self[0x14] (floats). The vector sp[0x10..0x1C] = p's, rotated by func_08B4A6A0(sp + 0x10; f12 = r); sp[0..0xC] = q's, rotated by func_08B4A6A0(sp; f12 = s); sp[0x20..0x2C] = a copy of sp[0x10..0x1C];
// func_08B4A890(sp + 0x20, sp + 0x20, sp); sp[0x30] = a1 and func_08BADAA8(sp + 0x30, sp + 0x20) leave in sp[0x30] a pointer to four floats: v0 = 0 when all four are 0.0f, else 1.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088A0628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t p = word(rt, self + 8u);
    put(rt, frame.at(0x30u), a1);
    const std::uint32_t q = word(rt, self + 0xCu);
    const float r = float_at(rt, self + 0x10u);
    const float s = float_at(rt, self + 0x14u);
    for (std::uint32_t i = 0u; i != 4u; ++i) put(rt, frame.at(0x10u + 4u * i), word(rt, p + 0x34u + 4u * i));
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x10u)}, {r});
    for (std::uint32_t i = 0u; i != 4u; ++i) put(rt, frame.at(4u * i), word(rt, q + 0x34u + 4u * i));
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0u)}, {s});
    for (std::uint32_t i = 0u; i != 4u; ++i) put(rt, frame.at(0x20u + 4u * i), word(rt, frame.at(0x10u + 4u * i)));
    (void)call_set(rt, ctx, 0x08B4A890u, {frame.at(0x20u), frame.at(0x20u), frame.at(0u)});
    (void)call_set(rt, ctx, 0x08BADAA8u, {frame.at(0x30u), frame.at(0x20u)});
    const std::uint32_t values = word(rt, frame.at(0x30u));
    bool any = false;
    for (std::uint32_t i = 0u; i != 4u; ++i) any = any || float_at(rt, values + 4u * i) != 0.0f;
    ctx.gpr[2] = any ? 1u : 0u;
}

// func_08AD41B4(self; f12 = dt): a timed state. s = self[0x64] (a record {halfword type, halfword percent, halfword next}): s == 0 or func_08B1A844(self) != 0 ends with the tail call of the virtual slot 0 of self's vtable (self). Otherwise t = self[0x70] + dt is stored back; length = (float)s[2] / 100.0f and the fraction f = length == 0 ? 0 : t / length, clamped to 1.0f (a2 = 1 when f >= 1.0f, else 0).
// By the type s[0] (0 .. 5; the jump table at 0x08C907E0): 1: self[0x68] = s[2] and the reset below; 2: self[0x6C] = f; 3: self[0x6C] = 1.0f; 4: self[0x6C] = 1.0f - f; 5: self[0x6C] = 0; 0 and other: nothing. The reset (types 1 and any with a2 = 1): self[0x70] = 0 and self[0x64] = (s[4] == 0 ? 0 : s + 4), the next record.
// v0 is what the asm leaves: the table slot address (0x08C907E0 + 4 * type) for types 0 .. 5, type * 4 above, the new self[0x64] after a reset.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08AD41B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float dt = ctx.fpr[12];
    const std::uint32_t record = word(rt, self + 0x64u);
    if (record == 0u || call_set(rt, ctx, 0x08B1A844u, {self}) != 0u) {
        tail_call(ctx, slot_of(rt, self, 0u), {self});
        return;
    }
    const float elapsed = float_at(rt, self + 0x70u) + dt;
    put_float(rt, self + 0x70u, elapsed);
    const std::uint32_t a1 = half_u(rt, record + 2u);
    const float length = static_cast<float>(signed_of(a1)) / std::bit_cast<float>(0x42C80000u);
    float f = 0.0f;
    if (!(length == 0.0f)) f = elapsed / length;
    bool done;
    if (std::bit_cast<float>(0x3F800000u) <= f) {
        done = true;
        f = 1.0f;
    } else {
        done = false;
    }
    const std::uint32_t type = half_u(rt, record);
    std::uint32_t v0 = type << 2u;
    bool reset = done;
    if (type < 6u) {
        v0 = 0x08C907E0u + 4u * type;
        switch (type) {
        case 1:
            put(rt, self + 0x68u, a1);
            reset = true;
            break;
        case 2:
            put_float(rt, self + 0x6Cu, f);
            break;
        case 3:
            put_float(rt, self + 0x6Cu, 1.0f);
            break;
        case 4:
            put_float(rt, self + 0x6Cu, 1.0f - f);
            break;
        case 5:
            put(rt, self + 0x6Cu, 0u);
            break;
        default:
            break;
        }
    }
    ctx.gpr[2] = v0;
    if (reset) {
        put(rt, self + 0x70u, 0u);
        v0 = record + 4u;
        if (half_u(rt, record + 4u) == 0u) v0 = 0u;
        ctx.gpr[2] = v0;
        put(rt, self + 0x64u, v0);
    }
}

// func_088D3E98(out, obj, a2, a3): func_088D2F9C(out, obj, a2, a3); n = a3 < 0 ? a2 : a2 - a3; k = obj[0][0x61C]; r = func_088D2B34(obj). Without r: f1 = 20.0f and f2 = -400.0f (0xC3C80000). With it, by func_088D62FC(obj[0] + 4) (asked once, and again unless the first answer is 1):
// answer 1 or 2: f1 = 0.0f, f2 = (float)(-10 * r[0x19]); other: f1 = (float)r[0x25], f2 = (float)(-10 * r[0x23]); f2 == 0 becomes -400.0f and f1 == 0 becomes 20.0f. out[0x18] = v = f2 - f1 * (float)n, then func_088D2BC8(k; f12 = v) gives a float w; func_088D390C(k, out, out + 0xC, 6; f12 = w, f13 = 0.0f); out[0x1C] = func_088D30B0(obj), out[0x20] = 1 (byte); v0 = out.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088D3E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t obj = ctx.gpr[5];
    const std::uint32_t arg2 = ctx.gpr[6];
    const std::uint32_t arg3 = ctx.gpr[7];
    (void)call_set(rt, ctx, 0x088D2F9Cu, {out, obj, arg2, arg3});
    const std::uint32_t header = word(rt, obj);
    const std::uint32_t count = signed_of(arg3) < 0 ? arg2 : arg2 - arg3;
    const std::uint32_t owner = word(rt, header + 0x61Cu);
    const std::uint32_t record = call_set(rt, ctx, 0x088D2B34u, {obj});
    float f1;
    float f2 = std::bit_cast<float>(0xC3C80000u);
    if (record == 0u) {
        f1 = std::bit_cast<float>(0x41A00000u);
    } else {
        bool near_first = call_set(rt, ctx, 0x088D62FCu, {word(rt, obj) + 4u}) == 1u;
        if (!near_first) near_first = call_set(rt, ctx, 0x088D62FCu, {word(rt, obj) + 4u}) == 2u;
        if (near_first) {
            const std::uint32_t v = byte_u(rt, record + 0x19u);
            f1 = 0.0f;
            f2 = static_cast<float>(signed_of(0u - (v * 10u)));
        } else {
            const std::uint32_t v = byte_u(rt, record + 0x23u);
            const std::uint32_t u = byte_u(rt, record + 0x25u);
            f1 = static_cast<float>(signed_of(u));
            f2 = static_cast<float>(signed_of(0u - (v * 10u)));
        }
        if (f2 == 0.0f) f2 = std::bit_cast<float>(0xC3C80000u);
        if (f1 == 0.0f) f1 = std::bit_cast<float>(0x41A00000u);
    }
    const float value = f2 - psp_mul(f1, static_cast<float>(signed_of(count)));
    put_float(rt, out + 0x18u, value);
    const float w = call_setf(rt, ctx, 0x088D2BC8u, {owner}, {value}).f0;
    (void)call_setf(rt, ctx, 0x088D390Cu, {owner, out, out + 0xCu, 6u}, {w, 0.0f});
    put(rt, out + 0x1Cu, call_set(rt, ctx, 0x088D30B0u, {obj}));
    put8(rt, out + 0x20u, 1u);
    ctx.gpr[2] = out;
}

// func_088829A0(self): a destructor. The three words self[0x40], self[0x44], self[0x48] that are not null get the virtual slot 0x2C (the object) and are cleared; self[0x54] = 0x08CC2590, self[0x58] = 0x08CC26B4; the four sub-objects self + 0x118 / 0x104 / 0xF0 / 0xDC and then self + 0xC0 / 0xA4 / 0x88 / 0x6C get their virtual slot 0 (the object), in that order;
// self[0x54] = 0x08CBF428, self[0x58] = 0x08CC16A0. The counted block c = self[0x50] (null: v0 = 0x08CBF428): its strong count c[4] is decremented and, when it was 1, the slot 8 runs; then c[8] is decremented and, when it was 1, the virtual slot 0xC is the tail call (c). v0 is 1 after a decrement that did not lead to a call.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088829A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (std::uint32_t i = 0u; i != 3u; ++i) {
        const std::uint32_t object = word(rt, self + 0x40u + 4u * i);
        if (object != 0u) {
            (void)call_set(rt, ctx, slot_of(rt, object, 0x2Cu), {object});
            put(rt, self + 0x40u + 4u * i, 0u);
        }
    }
    put(rt, self + 0x54u, 0x08CC2590u);
    put(rt, self + 0x58u, 0x08CC26B4u);
    for (const std::uint32_t offset : {0x118u, 0x104u, 0xF0u, 0xDCu, 0xC0u, 0xA4u, 0x88u, 0x6Cu}) {
        const std::uint32_t element = self + offset;
        (void)call_set(rt, ctx, slot_of(rt, element, 0u), {element});
    }
    put(rt, self + 0x54u, 0x08CBF428u);
    put(rt, self + 0x58u, 0x08CC16A0u);
    ctx.gpr[2] = 0x08CBF428u;
    const std::uint32_t counted = word(rt, self + 0x50u);
    if (counted == 0u) return;
    std::uint32_t old = word(rt, counted + 4u);
    put(rt, counted + 4u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    (void)call_set(rt, ctx, slot_of(rt, counted, 8u), {counted});
    old = word(rt, counted + 8u);
    put(rt, counted + 8u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    tail_call(ctx, slot_of(rt, counted, 0xCu), {counted});
}

// func_0893BA0C(): a one-time initialiser. func_0893B9C8(0x08CDDF03, 0x08CDDF04), then for the two rows r = 0x08CDDF08, 0x08CDDF18: func_0893B9C8(r + 2, r + 3) and func_0893B9C8(r + 0xA, r + 0xB); func_0893B9C8(0x08CDDF2D, 0x08CDDF2F); the byte at 0x08CDDEFA becomes 1 when it was 0; func_0893AA4C(0x08CDDEF8).
// Then the five bytes b = 0x08CDDEF8 + 0x34 .. 0x38 give the floats 0x08CF2A14 + 0 = b[0x34] * 0.1f, +4 = b[0x35] * 2.7777777f (0x4031C71D), +8 = b[0x36] * 0.1f, +0xC = b[0x37] * 2.7777777f, +0x10 = b[0x38] * 0.1f. v0 = 0x08CF2A14.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_0893BA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kBlock = 0x08CDDEF8u;
    constexpr std::uint32_t kOut = 0x08CF2A14u;
    (void)call_set(rt, ctx, 0x0893B9C8u, {0x08CDDF03u, 0x08CDDF04u});
    for (std::uint32_t row = 0u; row != 2u; ++row) {
        const std::uint32_t r = kBlock + 0x10u + 0x10u * row;
        (void)call_set(rt, ctx, 0x0893B9C8u, {r + 2u, r + 3u});
        (void)call_set(rt, ctx, 0x0893B9C8u, {r + 0xAu, r + 0xBu});
    }
    (void)call_set(rt, ctx, 0x0893B9C8u, {0x08CDDF2Du, 0x08CDDF2Fu});
    if (byte_u(rt, kBlock + 2u) == 0u) put8(rt, kBlock + 2u, 1u);
    (void)call_set(rt, ctx, 0x0893AA4Cu, {kBlock});
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float scale = std::bit_cast<float>(0x4031C71Du);
    const float f8 = static_cast<float>(byte_u(rt, kBlock + 0x38u));
    const float f3 = static_cast<float>(byte_u(rt, kBlock + 0x34u));
    const float f4 = static_cast<float>(byte_u(rt, kBlock + 0x35u));
    const float f5 = static_cast<float>(byte_u(rt, kBlock + 0x37u));
    const float f6 = static_cast<float>(byte_u(rt, kBlock + 0x36u));
    put_float(rt, kOut + 0x10u, psp_mul(f8, tenth));
    put_float(rt, kOut + 4u, psp_mul(f4, scale));
    put_float(rt, kOut + 0xCu, psp_mul(f5, scale));
    put_float(rt, kOut + 8u, psp_mul(f6, tenth));
    put_float(rt, kOut, psp_mul(f3, tenth));
    ctx.gpr[2] = kOut;
}

// func_088D414C(out, obj, a2, a3 = kind): a default value d = sp + 0xFC and the 0x24-byte record sp + 0xD8 are made (func_088D2F9C(out, obj, a2, a3), func_088D2F9C(d, obj, a2, a3), r = func_088D3E98(sp + 0xD8, obj, a2, func_088D4FA8(obj[0] + 4)), func_088D2FC8(d, r) copies it into d). With t = the word 0x08CDD244 + 24 * kind and
// m = (kind == 7 ? 0 : a2): t = 0: func_088D3424(sp + 0xB4, obj, 0, m); 1: func_088D3424(sp + 0x90, obj, 1, m); 2 and 3: func_088D3DA0(sp + 0x6C or sp + 0x48, obj, a2 (m), kind); 4: func_088D3D10(sp + 0x24, obj); 5: func_088D4014(sp, obj, m); the built record is copied into out by func_088D2FC8(out, record). If the byte out[0x20] is still 0, d is copied into out. v0 = out.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_088D414C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t obj = ctx.gpr[5];
    const std::uint32_t arg2 = ctx.gpr[6];
    const std::uint32_t kind = ctx.gpr[7];
    GuestFrame frame(ctx, 0x140u);
    const std::uint32_t standard = frame.at(0xFCu);
    (void)call_set(rt, ctx, 0x088D2F9Cu, {out, obj, arg2, kind});
    (void)call_set(rt, ctx, 0x088D2F9Cu, {standard, obj, arg2, kind});
    const std::uint32_t timing = call_set(rt, ctx, 0x088D4FA8u, {word(rt, obj) + 4u});
    const std::uint32_t built = frame.at(0xD8u);
    (void)call_set(rt, ctx, 0x088D3E98u, {built, obj, arg2, timing});
    (void)call_set(rt, ctx, 0x088D2FC8u, {standard, built});
    const std::uint32_t type = word(rt, 0x08CDD244u + 24u * kind);
    const std::uint32_t m = kind == 7u ? 0u : arg2;
    std::uint32_t record = 0u;
    bool built_any = true;
    switch (type < 6u ? type : 6u) {
    case 0:
        record = frame.at(0xB4u);
        (void)call_set(rt, ctx, 0x088D3424u, {record, obj, 0u, m});
        break;
    case 1:
        record = frame.at(0x90u);
        (void)call_set(rt, ctx, 0x088D3424u, {record, obj, 1u, m});
        break;
    case 2:
        record = frame.at(0x6Cu);
        (void)call_set(rt, ctx, 0x088D3DA0u, {record, obj, m, kind});
        break;
    case 3:
        record = frame.at(0x48u);
        (void)call_set(rt, ctx, 0x088D3DA0u, {record, obj, m, kind});
        break;
    case 4:
        record = frame.at(0x24u);
        (void)call_set(rt, ctx, 0x088D3D10u, {record, obj});
        break;
    case 5:
        record = frame.at(0u);
        (void)call_set(rt, ctx, 0x088D4014u, {record, obj, m});
        break;
    default:
        built_any = false;
        break;
    }
    if (built_any) (void)call_set(rt, ctx, 0x088D2FC8u, {out, record});
    if (byte_u(rt, out + 0x20u) == 0u) (void)call_set(rt, ctx, 0x088D2FC8u, {out, standard});
    ctx.gpr[2] = out;
}

// func_0893B824(self, mode = a1, a2; f12 = x): r = self[8], k = (int8)r[3]. k == 4 and mode == 0: func_089596B0(self[0], r[0]); k == 4 and mode < 2: func_0895936C(self[0], r[0]). v = mode 0: x / self[0xC][0x68]; mode 1: x * 100.0f; else x; t = trunc(v): mode 0 with t >= 4, or mode 1 with t >= 40, sets the byte r[0x74E] = 0xD.
// A byte r[0x74A] below 0xD runs func_0893B6EC(self, mode, a2; f12 = x) (v0 is its answer, 0 otherwise). With y = x * 0.0001f (0x38D1B717): y >= 0.2f (0x3E4CCCCD) clamps y to 1.0f at most and, when r[0x74A] is 0 or r[0x744] <= y, sets r[0x74A] = 0x18 and r[0x744] = y; v0 = 0x18 whenever y >= 0.2f.
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_0893B824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[5];
    const std::uint32_t arg2 = ctx.gpr[6];
    const float x = ctx.fpr[12];
    const std::uint32_t r = word(rt, self + 8u);
    if (signed_of(sext8(byte_u(rt, r + 3u))) == 4 && mode == 0u) (void)call_set(rt, ctx, 0x089596B0u, {word(rt, self), byte_u(rt, r)});
    if (signed_of(sext8(byte_u(rt, r + 3u))) == 4 && mode < 2u) (void)call_set(rt, ctx, 0x0895936Cu, {word(rt, self), byte_u(rt, r)});
    float v;
    if (mode == 0u) {
        v = x / float_at(rt, word(rt, self + 0xCu) + 0x68u);
    } else if (mode == 1u) {
        v = psp_mul(x, std::bit_cast<float>(0x42C80000u));
    } else {
        v = x;
    }
    const std::uint32_t t = ctx.fpu_float_to_word_ct<1u>(v);
    if (mode == 0u && signed_of(t) >= 4) put8(rt, r + 0x74Eu, 0xDu);
    if (mode == 1u && signed_of(t) >= 0x28) put8(rt, r + 0x74Eu, 0xDu);
    ctx.gpr[2] = byte_u(rt, r + 0x74Au) < 0xDu ? 1u : 0u;
    if (byte_u(rt, r + 0x74Au) < 0xDu) (void)call_setf(rt, ctx, 0x0893B6ECu, {self, mode, arg2}, {x});
    else ctx.gpr[2] = 0u;
    float y = psp_mul(x, std::bit_cast<float>(0x38D1B717u));
    if (!(std::bit_cast<float>(0x3E4CCCCDu) <= y)) return;
    if (1.0f < y) y = 1.0f;
    ctx.gpr[2] = 0x18u;
    if (byte_u(rt, r + 0x74Au) != 0u && !(float_at(rt, r + 0x744u) <= y)) return;
    put8(rt, r + 0x74Au, 0x18u);
    put_float(rt, r + 0x744u, y);
}

// func_08881A68(self, index, obj): c = self[0xC] (a table of words): the object c[index + 0x10] (word index * 4 + 0x40) gets its virtual slot 0x2C and the entry is cleared, v0 = c + 4 * index. Without obj that ends it (v0 = index < 2). Otherwise (index below 2) the entry becomes obj's slot 0x18 (obj); an empty entry ends it. Index 0: the entry e gets e's slot 8 (e, func_0887BD94(6)), then the slot 0xC of the entry (e, 0x08CA052C, 0x8000FF00) and its slot 0x18 (e, 0x08BA5190);
// index 1 reads the entry self[0xC][0x44] instead: its slot 8 (e, func_0887BD94(0xF)) and its slot 0x18 (e, 0x08BA5198). Then the entry's slot 0x14 (entry, self) and the tail call of its slot 0x28 (entry).
// Branches the compared cases do not take (checked against the asm by hand, branch targets and delay slots included): see the PR.
void func_08881A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t obj = ctx.gpr[6];
    const std::uint32_t offset = index << 2u;
    std::uint32_t table = word(rt, self + 0xCu);
    std::uint32_t entry = word(rt, table + offset + 0x40u);
    if (entry != 0u) {
        (void)call_set(rt, ctx, slot_of(rt, entry, 0x2Cu), {entry});
        table = word(rt, self + 0xCu);
        put(rt, table + offset + 0x40u, 0u);
    }
    ctx.gpr[2] = table + offset;
    if (obj == 0u) {
        ctx.gpr[2] = index < 2u ? 1u : 0u;  // the sltiu in the delay slot of the beqz
        return;
    }
    if (index < 2u) {
        const std::uint32_t target_table = word(rt, self + 0xCu);  // read before the call (the jalr delay slot adds the offset)
        const std::uint32_t made = call_set(rt, ctx, slot_of(rt, obj, 0x18u), {obj});
        put(rt, target_table + offset + 0x40u, made);
    }
    table = word(rt, self + 0xCu);
    ctx.gpr[2] = table + offset;
    if (word(rt, table + offset + 0x40u) == 0u) return;
    if (index == 0u) {
        const std::uint32_t e = word(rt, table + 0x40u);
        const std::uint32_t handler = slot_of(rt, e, 8u);
        const std::uint32_t made = call_set(rt, ctx, 0x0887BD94u, {6u});
        (void)call_set(rt, ctx, handler, {e, made});
        const std::uint32_t second = word(rt, word(rt, self + 0xCu) + 0x40u);
        (void)call_set(rt, ctx, slot_of(rt, second, 0xCu), {second, 0x08CA052Cu, 0x8000FF00u});
        const std::uint32_t third = word(rt, word(rt, self + 0xCu) + 0x40u);
        (void)call_set(rt, ctx, slot_of(rt, third, 0x18u), {third, 0x08BA5190u});
    } else if (index == 1u) {
        const std::uint32_t e = word(rt, table + 0x44u);
        const std::uint32_t handler = slot_of(rt, e, 8u);
        const std::uint32_t made = call_set(rt, ctx, 0x0887BD94u, {0xFu});
        (void)call_set(rt, ctx, handler, {e, made});
        const std::uint32_t second = word(rt, word(rt, self + 0xCu) + 0x44u);
        (void)call_set(rt, ctx, slot_of(rt, second, 0x18u), {second, 0x08BA5198u});
    }
    const std::uint32_t current = word(rt, word(rt, self + 0xCu) + offset + 0x40u);
    (void)call_set(rt, ctx, slot_of(rt, current, 0x14u), {current, self});
    const std::uint32_t last = word(rt, word(rt, self + 0xCu) + offset + 0x40u);
    tail_call(ctx, slot_of(rt, last, 0x28u), {last});
}

// @@END

}  // namespace
}  // namespace hot_batch_98
}  // namespace gt::game

#define HOT98(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_98::name, gt::ReturnKind::kind)
#define HOT98N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_98::name, gt::ReturnKind::kind)

// HOT98REGS
HOT98(0x089C5FE0u, func_089C5FE0, Word);
HOT98(0x08AE3DB4u, func_08AE3DB4, Word);
HOT98(0x08AE78E8u, func_08AE78E8, Word);
HOT98(0x088E8A84u, func_088E8A84, Word);
HOT98(0x08B13DC8u, func_08B13DC8, Void);
HOT98(0x0894A15Cu, func_0894A15C, Word);
HOT98(0x0880E6ECu, func_0880E6EC, Word);
HOT98(0x0880EB34u, func_0880EB34, Word);
HOT98(0x08923E0Cu, func_08923E0C, Word);
HOT98(0x08992AA4u, func_08992AA4, Word);
HOT98(0x089F7F64u, func_089F7F64, Word);
// HOT98(0x0888E81Cu, func_0888E81C, Word);  // HOT98: compared 0 or 1 case per run (hits=1), none changed memory
HOT98(0x088D78ECu, func_088D78EC, Word);
HOT98(0x088DFB68u, func_088DFB68, Word);
HOT98(0x08BED9A0u, func_08BED9A0, Word);
HOT98(0x08C0D538u, func_08C0D538, Word);
HOT98(0x08C11D08u, func_08C11D08, Word);
HOT98(0x088A1D84u, func_088A1D84, Word);
HOT98(0x0889ED00u, func_0889ED00, Word);
HOT98(0x0896E380u, func_0896E380, Word);
HOT98(0x08A539ACu, func_08A539AC, Word);
HOT98(0x088C4BB0u, func_088C4BB0, Word);
HOT98(0x088C50D4u, func_088C50D4, Word);
HOT98(0x08960D4Cu, func_08960D4C, Word);
HOT98(0x08977904u, func_08977904, Word);
HOT98(0x089C54B8u, func_089C54B8, Word);
HOT98(0x08AFD0ECu, func_08AFD0EC, Word);
HOT98(0x088A0628u, func_088A0628, Word);
HOT98(0x08AD41B4u, func_08AD41B4, Word);
HOT98(0x088D3E98u, func_088D3E98, Word);
HOT98(0x088829A0u, func_088829A0, Word);
HOT98(0x0893BA0Cu, func_0893BA0C, Word);
HOT98(0x088D414Cu, func_088D414C, Word);
// HOT98(0x0893B824u, func_0893B824, Word);  // HOT98: compared 0 in every run (two calls on the routes)
HOT98(0x08881A68u, func_08881A68, Word);
