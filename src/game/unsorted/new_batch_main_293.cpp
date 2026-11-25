// Main-session batch 293: functions the synthetic gate had no usable inputs for. Written by hand from
// `tools.disasm`; structured C, one `void func_XXXXXXXX(` per address, the party that verifies runs the gate.
//
//  0x08B6B354 (f12 = x, f13 = y) -> f0  the libm power core behind the SVID wrapper 0x08B67720 (hot_batch_121):
//                                      special cases on the bit patterns, then 2**(y*log2|x|) with the log2 from a
//                                      range-reduced mantissa (bp / dp_h / dp_l tables at 0x08C96220) and the
//                                      exponential from a polynomial; returns through f0 only (ReturnKind::Float:
//                                      v0/v1 are mfc1/ins scratch).
// No argument register or global is dereferenced: the only float arguments arrive in f12/f13, so no input shape.
//
// The other two addresses of the batch were NOT drafted (deliberately, see the batch note): 0x088877DC (460 insns) is a
// container assignment over type-erased objects - it dispatches through three different vtables (a0's slots
// 0x18/0x1C/0x20/0x24, a1's rep at +4, a2's +0x20) and through the `ll/sc` shared-count release the original inlines
// six times; its v0 at the dozen exits is a different leftover per path, so the return contract cannot be pinned from
// the asm alone, and any case needs live objects with real vtables. 0x08C518D4 (466 insns) is an `_Rb_tree` hint
// insert whose key comparator is inlined six times (each copy a length test plus two byte loops) with shared merge
// points tangled together; getting one of them wrong silently reorders the tree walk. Both stay recompiled.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_293 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 bits(float value) { return std::bit_cast<u32>(value); }
float value_of(u32 pattern) { return std::bit_cast<float>(pattern); }
// `ins rX, $zero, 31, 1`: the pattern without its sign bit.
constexpr u32 kMagnitude = 0x7FFFFFFFu;
// `ins rX, $zero, 0, 12`: the pattern with its low 12 mantissa bits cleared.
constexpr u32 kLow12 = 0xFFFFF000u;
float truncate12(float value) { return value_of(bits(value) & kLow12); }

// The .rodata floats the power core loads (fdlibm's constants: the 0x08CB64xx block at the end of .rodata and the
// bp / dp_h / dp_l table at 0x08C96220). The value each word holds is in the comment; the code reads guest memory.
constexpr u32 kOne = 0x08CB640Cu;     // 1.0
constexpr u32 kInfinity = 0x08CB6410u; // +inf (also the overflow result)
constexpr u32 kLg2High = 0x08CB6414u; // 1.44268798828125
constexpr u32 kLg2Low = 0x08CB6418u;  // 7.052607543300837e-06
constexpr u32 kMinusQuarter = 0x08CB641Cu;    // -0.25
constexpr u32 kThird = 0x08CB6420u;           // 0.3333333432674408
constexpr u32 kHalf = 0x08CB6424u;            // 0.5
constexpr u32 kMinusLg2 = 0x08CB6428u;        // -1.4426950216293335
constexpr u32 kTwo24 = 0x08CB642Cu;           // 16777216.0 (scales a subnormal base)
constexpr u32 kL6 = 0x08CB6430u;              // 0.20697501301765442
constexpr u32 kL5 = 0x08CB6434u;              // 0.23066075146198273
constexpr u32 kL4 = 0x08CB6438u;              // 0.2727281153202057
constexpr u32 kL3 = 0x08CB643Cu;              // 0.4285714328289032
constexpr u32 kL2 = 0x08CB6440u;              // 0.6000000238418579
constexpr u32 kThree = 0x08CB6444u;           // 3.0
constexpr u32 kLogHigh = 0x08CB6448u;         // 0.9617919921875
constexpr u32 kLogLowA = 0x08CB644Cu;         // 4.701738362200558e-06
constexpr u32 kLogLowB = 0x08CB6450u;         // 0.9617967009544373
constexpr u32 kHuge = 0x08CB6458u;            // 1.0000000150474662e+30
constexpr u32 kThreshold128 = 0x08CB645Cu;    // 4.299566569443414e-08
constexpr u32 kTiny = 0x08CB6460u;            // 1.0000000031710769e-30
constexpr u32 kExpHigh = 0x08CB6464u;         // 0.693145751953125
constexpr u32 kExpLow = 0x08CB6468u;          // 0.6931471824645996
constexpr u32 kExpLowA = 0x08CB646Cu;         // 1.4286065379565116e-06
// The exponential's polynomial, in the order the code multiplies them in (the innermost coefficient first).
constexpr u32 kExpP1 = 0x08CB6470u;           // 4.138136944220605e-08
constexpr u32 kExpP2 = 0x08CB6474u;           // 1.6533901998627698e-06 (added with a minus)
constexpr u32 kExpP3 = 0x08CB6478u;           // 6.61375597701408e-05
constexpr u32 kExpP4 = 0x08CB647Cu;           // 0.0027777778450399637 (added with a minus)
constexpr u32 kExpP5 = 0x08CB6480u;           // 0.1666666716337204
constexpr u32 kTwo = 0x08CB6484u;             // 2.0
// The range-reduction tables: dp_h, dp_l and bp, indexed by the interval the mantissa falls in.
constexpr u32 kDpHigh = 0x08C96220u;  // {0.0, 0.584960938}
constexpr u32 kDpLow = 0x08C96228u;   // {0.0, 1.56322085e-06}
constexpr u32 kBp = 0x08C96230u;      // {1.0, 1.5}

// A guest call with float arguments in f12-f15 and integers in a0-a3, results left in v0 / v1 / f0 like the original
// leaves them after its `jal`.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// --- 0x08B6B354: the power core. ---
void func_08B6B354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u);
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    const u32 x_bits = bits(x);
    const u32 y_bits = bits(y);
    const u32 ax = x_bits & kMagnitude;  // s3, |x|
    const u32 ay = y_bits & kMagnitude;  // s0, |y|
    const auto keep = [&](float value) { ctx.fpr[0] = value; };

    if (ay == 0u) {  // y is +-0: 1.0, even for a NaN base
        keep(float_at(rt, kOne));
        return;
    }
    if (0x7F800000u < ax || 0x7F800000u < ay) {  // a NaN operand: the original adds them
        keep(y + x);
        return;
    }

    const bool x_negative = static_cast<s32>(x_bits) < 0;
    const bool y_negative = static_cast<s32>(y_bits) < 0;
    // s4: 0 = y is not an integer, 1 = an odd integer, 2 = an even integer (every |y| >= 2^26 counts as even).
    u32 y_class = 0u;
    if (x_negative && 0x3F7FFFFFu < ay) {
        if (0x4B7FFFFFu < ay) {
            y_class = 2u;
        } else {  // |y| <= 0x4B7FFFFF (below 2^24), so the mantissa holds every bit of the integer part
            const s32 shift = 0x96 - (static_cast<s32>(ay) >> 23);
            const u32 integer = static_cast<u32>(static_cast<s32>(ay) >> shift);
            if (ay == (integer << shift)) y_class = 2u - (integer & 1u);
        }
    }

    if (ay == 0x7F800000u) {  // y is +-infinite
        if (ax == 0x3F800000u) {  // (-1)**+-inf: the original's y - y
            keep(y - y);
            return;
        }
        if (0x3F800000u < ax) {  // |x| > 1: inf for y = +inf, 0 for y = -inf
            keep(y_negative ? 0.0f : y);
            return;
        }
        keep(y_negative ? -y : 0.0f);  // |x| < 1
        return;
    }
    if (ay == 0x3F800000u) {  // y = +-1: x, or its reciprocal
        keep(y_negative ? float_at(rt, kOne) / x : x);
        return;
    }
    if (y_bits == 0x40000000u) {  // y = 2
        keep(x * x);
        return;
    }
    if (y_bits == 0x3F000000u && !x_negative) {  // y = +0.5 of a non-negative base: the square root, as a tail call
        ctx.fpr[12] = x;
        tail_call(ctx, 0x08B6C020u, {});
        return;
    }

    if (x_negative && y_class == 0u) {  // a negative base with a non-integer exponent: the original's (x-x)/(x-x)
        const float zero = x - x;
        keep(zero / zero);
        return;
    }

    const float magnitude = call_setf(rt, ctx, 0x08B663A4u, {}, {x}).f0;  // fabsf(x), the `jal` at 0x08B6B414
    if (ax == 0x7F800000u || ax == 0u || ax == 0x3F800000u) {  // |x| is inf, 0 or 1: 1/|x| for a negative y
        const float reciprocal = float_at(rt, kOne) / magnitude;
        const float base = y_negative ? reciprocal : magnitude;
        if (!x_negative) {
            keep(base);
            return;
        }
        if (ax == 0x3F800000u && y_class == 0u) {  // (-1)**non-integer
            const float zero = base - base;
            keep(zero / zero);
            return;
        }
        keep(y_class == 1u ? -base : base);  // a negative base to an odd integer keeps the sign
        return;
    }

    // The tail every logarithm path shares: 2**(y*(p_high + p_low)) with the sign of a negative base.
    const auto exponentiate = [&](float p_high, float p_low) -> float {
        const float one = float_at(rt, kOne);
        const float sign = x_negative && y_class == 1u ? -one : one;  // f20
        const float y_truncated = truncate12(y);
        const float low_term = p_low * y;                 // f3
        const float high_term = p_high * y_truncated;     // f4
        const float partial = p_high * (y - y_truncated) + low_term;  // f3
        const float product = partial + high_term;        // f2
        const u32 product_bits = bits(product);
        const u32 magnitude_bits = product_bits & kMagnitude;  // v1

        // The result magnitude over- or underflows (the 128 / 150 binade tests).
        bool overflow = false;
        bool underflow = false;
        if (static_cast<s32>(product_bits) > 0) {
            if (0x43000000u < magnitude_bits) {
                overflow = true;
            } else if (magnitude_bits == 0x43000000u) {
                const float below = product - high_term;
                const float above = partial + float_at(rt, kThreshold128);
                if (below < above) overflow = true;
            }
        } else {
            if (0x43160000u < magnitude_bits) {
                underflow = true;
            } else if (magnitude_bits == 0x43160000u) {
                const float below = product - high_term;
                if (partial <= below) underflow = true;
            }
        }
        if (overflow) {
            const float scaled = sign * float_at(rt, kHuge);
            return scaled * float_at(rt, kHuge);
        }
        if (underflow) {
            const float scaled = sign * float_at(rt, kTiny);
            return scaled * float_at(rt, kTiny);
        }

        // The rounding correction of a magnitude above 0.5: the exponent addend goes to a1 and the truncated-away
        // mantissa to f4.
        u32 biased = product_bits;  // v1
        u32 addend = 0u;            // a1
        s32 scale = 0;              // a0
        float carry = high_term;    // f4
        if (0x3F000000u < magnitude_bits) {
            const u32 shift = ((magnitude_bits >> 23) - 0x7Eu) & 31u;
            biased = product_bits + (0x00800000u >> shift);
            const s32 binade = static_cast<s32>(static_cast<std::uint8_t>(biased >> 23)) - 0x7F;  // `ext v1, 23, 8`
            const u32 masked = static_cast<u32>(~((0x007FFFFFu) >> (static_cast<u32>(binade) & 31u)));
            carry -= value_of(biased & masked);
            const u32 mantissa = (biased & 0x007FFFFFu) | 0x00800000u;
            scale = static_cast<s32>(mantissa >> ((0x17u - static_cast<u32>(binade)) & 31u));
            if (static_cast<s32>(product_bits) < 0) scale = -scale;
            addend = static_cast<u32>(scale) << 23;
        }
        // f2: the exponent argument, truncated to its 12 high mantissa bits.
        const float high = 0x3F000000u < magnitude_bits ? truncate12(partial + carry) : truncate12(product);

        // 2**high by the polynomial (the exp2 kernel).
        float low = high * float_at(rt, kExpLowA);
        float remainder = partial - (high - carry);
        remainder = remainder * float_at(rt, kExpLow);
        remainder = remainder + low;
        const float rest = high * float_at(rt, kExpHigh);
        const float sum = rest + remainder;
        float square = sum * sum;
        const float difference = sum - rest;
        float polynomial = square * float_at(rt, kExpP1);
        float tail = remainder - difference;
        polynomial = polynomial - float_at(rt, kExpP2);
        const float cross = sum * tail;
        polynomial = square * polynomial;
        tail = tail + cross;
        polynomial = polynomial + float_at(rt, kExpP3);
        polynomial = square * polynomial;
        polynomial = polynomial - float_at(rt, kExpP4);
        polynomial = square * polynomial;
        polynomial = polynomial + float_at(rt, kExpP5);
        square = square * polynomial;
        square = sum - square;
        const float divisor = square - float_at(rt, kTwo);
        square = sum * square;
        square = square / divisor;
        square = square - tail;
        square = square - sum;
        const float result = one - square;

        const u32 biased_result = addend + bits(result);
        if (static_cast<s32>(biased_result) >> 23 <= 0) {  // subnormal: the original calls scalbnf(result, scale)
            return call_setf(rt, ctx, 0x08B6ECC4u, {static_cast<u32>(scale)}, {result}).f0 * sign;
        }
        return value_of(biased_result) * sign;
    };

    if (0x4D000000u < ay) {  // |y| >= 2^26: over- or underflows unless |x| is within 2^-20 of 1
        if (ax <= 0x3F7FFFF7u) {
            keep(y_negative ? float_at(rt, kInfinity) : 0.0f);
            return;
        }
        if (0x3F800000u + 7u < ax) {  // the asm's 0x3F800000 | 7
            keep(y_negative ? 0.0f : float_at(rt, kInfinity));
            return;
        }
        // log(1 + t) for the tiny t = |x| - 1, then the shared exponent block.
        const float t = magnitude - float_at(rt, kOne);
        const float square = t * t;
        float polynomial = t * float_at(rt, kMinusQuarter) + float_at(rt, kThird);
        polynomial = t * polynomial;
        const float half_term = float_at(rt, kHalf) - polynomial;
        float cubic = t * float_at(rt, kLg2Low);
        const float linear = t * float_at(rt, kLg2High);
        cubic = cubic + square * half_term * float_at(rt, kMinusLg2);
        const float high = truncate12(linear + cubic);
        keep(exponentiate(high, cubic - (high - linear)));
        return;
    }

    // log2|x|: scale a subnormal base up by 2^24, then reduce the mantissa to an interval around 1.
    float scaled_base = magnitude;  // f1
    u32 base_bits = ax;             // s3
    s32 adjust = 0;                 // v1
    if (ax <= 0x007FFFFFu) {
        scaled_base = magnitude * float_at(rt, kTwo24);
        base_bits = bits(scaled_base);
        adjust = -0x18;
    }
    const s32 exponent = static_cast<s32>(base_bits) >> 23;  // v0
    const u32 mantissa = base_bits & 0x007FFFFFu;             // a1
    s32 unbiased = adjust + exponent - 0x7Fu;                 // t2
    u32 base_pattern = mantissa | 0x3F800000u;                // a2
    u32 index = 0u;                                           // a0
    u32 high_addend = 0u;                                     // a3
    if (0x001CC471u < mantissa) {
        if (mantissa <= 0x005DB3D6u) {
            index = 1u;                   // sqrt(3/2) <= |x| < sqrt(3): the 1.5 interval, dp_h/dp_l[1]
            high_addend = 0x00200000u;
        } else {
            base_pattern -= 0x00800000u;  // |x| >= sqrt(3): the interval below sqrt(2), one binade up
            unbiased += 1;
        }
    }
    const float reference = float_at(rt, kBp + 4u * index);
    const float base = value_of(base_pattern);
    const float difference = base - reference;
    const float reciprocal = float_at(rt, kOne) / (base + reference);
    const u32 high_pattern = ((base_pattern >> 1) | 0x20000000u) + high_addend + 0x00040000u;
    const float high_base = value_of(high_pattern);
    const float low_base = base - (high_base - reference);
    const float quotient = difference * reciprocal;
    const float square = quotient * quotient;
    const float high_quotient = truncate12(quotient);
    const float low_quotient = reciprocal * ((difference - high_quotient * high_base) - high_quotient * low_base);

    float polynomial = square * float_at(rt, kL6);
    polynomial = polynomial + float_at(rt, kL5);
    polynomial = square * polynomial;
    polynomial = polynomial + float_at(rt, kL4);
    polynomial = square * polynomial;
    polynomial = polynomial + float_at(rt, kThird);
    polynomial = square * polynomial;
    polynomial = polynomial + float_at(rt, kL3);
    polynomial = square * polynomial;
    polynomial = polynomial + float_at(rt, kL2);
    float remainder = square * square * polynomial + low_quotient * (quotient + high_quotient);

    float level = truncate12(remainder + (high_quotient * high_quotient + float_at(rt, kThree)));
    float correction = level - float_at(rt, kThree);
    const float scaled_quotient = low_quotient * level;
    const float scaled_high = high_quotient * level;
    correction = correction - high_quotient * high_quotient;
    remainder = remainder - correction;
    remainder = scaled_quotient + quotient * remainder;
    level = truncate12(scaled_high + remainder);
    const float tail = level - scaled_high;
    remainder = remainder - tail;
    const float low_sum = level * float_at(rt, kLogLowA) + remainder * float_at(rt, kLogLowB) +
                          float_at(rt, kDpLow + 4u * index);
    const float high_part = level * float_at(rt, kLogHigh) + low_sum;
    const float log_high =
        truncate12(static_cast<float>(unbiased) + (high_part + float_at(rt, kDpHigh + 4u * index)));
    const float log_low = low_sum - ((log_high - static_cast<float>(unbiased)) -
                                     float_at(rt, kDpHigh + 4u * index) - level * float_at(rt, kLogHigh));
    keep(exponentiate(log_high, log_low));
}

} // namespace
} // namespace new_batch_main_293
} // namespace gt::game

#define NEW293(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_293::name, gt::ReturnKind::kind)
NEW293(0x08B6B354u, func_08B6B354, Float);
