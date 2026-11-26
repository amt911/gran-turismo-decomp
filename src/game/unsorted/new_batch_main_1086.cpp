// New functions, main session batch 1086.
//
// 0x08B685B0 is the C library's pow: two doubles in, one out, and the statically linked runtime's own double
// operations in between (FINDINGS: the 0x08B6xxxx "soft-float / 64-bit code"). Every arithmetic step of the original
// is one call there, so the source is written as the arithmetic it is: a double goes in a0/a1 or a2/a3 as
// (low word, high word) and comes back in v0/v1.
//
// What each helper is was read off its call sites (none is named in the binary): 0x08B71328 adds, 0x08B712B0
// subtracts, 0x08B71394 multiplies, 0x08B71720 divides, 0x08B6E268 is fabs, 0x08B71CE0 converts a signed int,
// 0x08B71A90 and 0x08B71C58 compare (an answer above zero means "a greater than b"), 0x08B6E74C scales by a power of
// two, and the y == 0.5 answer tail-calls the square root at 0x08B69EBC. The constants are read at their own
// addresses (0x08CB66C8.. and the dp_h / dp_l / bp tables at 0x08C96048..), so no bit pattern is retyped here.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1086 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A guest double as the runtime carries it: (v0, v1) = (low word, high word).
struct Dbl {
    u32 lo;
    u32 hi;
};

void func_08B685B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 lx = ctx.gpr[4];
    const u32 hx = ctx.gpr[5];
    const u32 ly = ctx.gpr[6];
    const u32 hy = ctx.gpr[7];
    const u32 ix = hx & 0x7FFFFFFFu;
    const u32 iy = hy & 0x7FFFFFFFu;
    const Dbl x{lx, hx};
    const Dbl y{ly, hy};
    Dbl answer{};
    bool to_sqrt = false;
    {
        GuestFrame frame(ctx, 0x90u);
        const auto load = [&](u32 address) { return Dbl{word(rt, address), word(rt, address + 4u)}; };
        const auto op2 = [&](u32 target, const Dbl &a, const Dbl &b) {
            const GuestResult r = call_guest(rt, ctx, target, {a.lo, a.hi, b.lo, b.hi}, {});
            return Dbl{r.v0, r.v1};
        };
        const auto add = [&](const Dbl &a, const Dbl &b) { return op2(0x08B71328u, a, b); };
        const auto sub = [&](const Dbl &a, const Dbl &b) { return op2(0x08B712B0u, a, b); };
        const auto mul = [&](const Dbl &a, const Dbl &b) { return op2(0x08B71394u, a, b); };
        const auto divd = [&](const Dbl &a, const Dbl &b) { return op2(0x08B71720u, a, b); };
        const auto absd = [&](const Dbl &a) {
            const GuestResult r = call_guest(rt, ctx, 0x08B6E268u, {a.lo, a.hi}, {});
            return Dbl{r.v0, r.v1};
        };
        const auto from_int = [&](s32 value) {
            const GuestResult r = call_guest(rt, ctx, 0x08B71CE0u, {static_cast<u32>(value)}, {});
            return Dbl{r.v0, r.v1};
        };
        const auto compare = [&](u32 target, const Dbl &a, const Dbl &b) {
            return static_cast<s32>(call_guest(rt, ctx, target, {a.lo, a.hi, b.lo, b.hi}));
        };
        const auto scalbn = [&](const Dbl &a, s32 exponent) {
            const GuestResult r =
                call_guest(rt, ctx, 0x08B6E74Cu, {a.lo, a.hi, static_cast<u32>(exponent)}, {});
            return Dbl{r.v0, r.v1};
        };
        const auto negated = [](const Dbl &a) { return Dbl{a.lo, a.hi ^ 0x80000000u}; };

        const Dbl one = load(0x08CB66C8u);
        const Dbl inf = load(0x08CB66D0u);
        const Dbl ivln2_h = load(0x08CB66D8u);
        const Dbl ivln2_l = load(0x08CB66E0u);
        const Dbl neg_quarter = load(0x08CB66E8u);
        const Dbl third = load(0x08CB66F0u);
        const Dbl half = load(0x08CB66F8u);
        const Dbl neg_ivln2 = load(0x08CB6700u);
        const Dbl two53 = load(0x08CB6708u);
        const Dbl L6 = load(0x08CB6710u);
        const Dbl L5 = load(0x08CB6718u);
        const Dbl L4 = load(0x08CB6720u);
        const Dbl L3 = load(0x08CB6728u);
        const Dbl L2 = load(0x08CB6730u);
        const Dbl L1 = load(0x08CB6738u);
        const Dbl three = load(0x08CB6740u);
        const Dbl cp_h = load(0x08CB6748u);
        const Dbl cp_l = load(0x08CB6750u);
        const Dbl cp = load(0x08CB6758u);
        const Dbl neg_one = load(0x08CB6760u);
        const Dbl huge = load(0x08CB6768u);
        const Dbl ovt = load(0x08CB6770u);
        const Dbl tiny = load(0x08CB6778u);
        const Dbl lg2_h = load(0x08CB6780u);
        const Dbl lg2 = load(0x08CB6788u);
        const Dbl lg2_l = load(0x08CB6790u);
        const Dbl P5 = load(0x08CB6798u);
        const Dbl P4_abs = load(0x08CB67A0u);  // the table holds |P4|; the original subtracts it
        const Dbl P3 = load(0x08CB67A8u);
        const Dbl P2_abs = load(0x08CB67B0u);  // and |P2| likewise
        const Dbl P1 = load(0x08CB67B8u);
        const Dbl two = load(0x08CB67C0u);
        const Dbl zero{0u, 0u};

        u32 yisint = 0;

        // The tail every non-early answer goes through: (t1 + t2) as the log2 of |x|, times y, then 2**that.
        // t1 is passed with its low word already cleared, as the original does before its first use.
        const auto finish = [&](const Dbl &t1z, const Dbl &t2) -> Dbl {
            // (-1)**y is -1 for an odd integer y and x < 0; the original packs that test into one word.
            const u32 sign_word = (hx >> 31) - 1u;  // 0 when x < 0, all ones otherwise
            const Dbl s = ((sign_word | (yisint - 1u)) != 0u) ? one : neg_one;
            const Dbl y1{0u, hy};  // y with its low word cleared
            const Dbl p_l = add(mul(t1z, sub(y, y1)), mul(t2, y));
            Dbl p_h = mul(t1z, y1);
            Dbl z = add(p_l, p_h);
            if (static_cast<s32>(z.hi) > 0x408FFFFF) {  // z >= 1024: overflow
                if (((z.hi - 0x40900000u) | z.lo) != 0u) return mul(mul(s, huge), huge);
                if (compare(0x08B71A90u, add(p_l, ovt), sub(z, p_h)) > 0) return mul(mul(s, huge), huge);
            } else if ((z.hi & 0x7FFFFFFFu) >= 0x4090CC00u) {  // z <= -1075: underflow
                if (((z.hi - 0xC090CC00u) | z.lo) != 0u) return mul(mul(s, tiny), tiny);
                if (compare(0x08B71C58u, p_l, sub(z, p_h)) <= 0) return mul(mul(s, tiny), tiny);
            }
            // 2**(p_h+p_l) with |z| > 0.5 folded into the exponent.
            const u32 i = z.hi & 0x7FFFFFFFu;
            u32 e = (i >> 20) - 0x3FFu;
            s32 n = 0;
            if (i > 0x3FE00000u) {
                const u32 rounded = z.hi + (0x00100000u >> (e + 1u));
                e = ((rounded & 0x7FFFFFFFu) >> 20) - 0x3FFu;
                const Dbl t{0u, rounded & ~(0x000FFFFFu >> e)};
                n = static_cast<s32>(((rounded & 0x000FFFFFu) | 0x00100000u) >> (20u - e));
                if (static_cast<s32>(z.hi) < 0) n = -n;
                p_h = sub(p_h, t);
            }
            const Dbl t = add(p_l, p_h);
            const Dbl tz{0u, t.hi};
            const Dbl u = mul(tz, lg2_h);
            const Dbl v = add(mul(sub(p_l, sub(t, p_h)), lg2), mul(tz, lg2_l));
            z = add(u, v);
            const Dbl w = sub(v, sub(z, u));
            const Dbl zz = mul(z, z);
            Dbl poly = sub(mul(zz, P5), P4_abs);
            poly = add(mul(zz, poly), P3);
            poly = sub(mul(zz, poly), P2_abs);
            poly = add(mul(zz, poly), P1);
            const Dbl t1 = sub(z, mul(zz, poly));
            const Dbl r = sub(divd(mul(z, t1), sub(t1, two)), add(w, mul(z, w)));
            z = sub(one, sub(r, z));
            // Multiply by 2**n by adding it to the exponent word, or through the scaling helper when the exponent
            // would leave the format.
            const u32 bumped = (static_cast<u32>(n) << 20) + z.hi;
            if ((static_cast<s32>(bumped) >> 20) <= 0) return mul(scalbn(z, n), s);
            return mul(Dbl{z.lo, bumped}, s);
        };

        if ((iy | ly) == 0u) {
            answer = one;  // x**0 = 1
        } else if (ix > 0x7FF00000u || (ix == 0x7FF00000u && lx != 0u) || iy > 0x7FF00000u ||
                   (iy == 0x7FF00000u && ly != 0u)) {
            answer = add(y, x);  // NaN in either operand answers x+y
        } else {
            // yisint: 0 when y is no integer, 1 an odd one, 2 an even one (only x < 0 cares).
            if ((hx & 0x80000000u) != 0u) {
                if (iy >= 0x43400000u) {
                    yisint = 2u;
                } else if (iy >= 0x3FF00000u) {
                    const u32 e = (iy >> 20) - 0x3FFu;
                    if (e > 20u) {
                        const u32 j = ly >> (52u - e);
                        if ((j << (52u - e)) == ly) yisint = 2u - (j & 1u);
                    } else if (ly == 0u) {
                        const u32 j = iy >> (20u - e);
                        if ((j << (20u - e)) == iy) yisint = 2u - (j & 1u);
                    }
                }
            }
            bool answered = false;
            if (ly == 0u) {
                if (iy == 0x7FF00000u) {  // y is +-inf
                    if (((ix - 0x3FF00000u) | lx) == 0u) {
                        answer = sub(y, y);  // the original's answer for x = +-1
                    } else if (ix > 0x3FF00000u) {
                        answer = (hy & 0x80000000u) == 0u ? y : zero;
                    } else {
                        answer = (hy & 0x80000000u) != 0u ? negated(y) : zero;
                    }
                    answered = true;
                } else if (iy == 0x3FF00000u) {  // y is +-1
                    answer = (hy & 0x80000000u) != 0u ? divd(one, x) : x;
                    answered = true;
                } else if (hy == 0x40000000u) {  // y is 2
                    answer = mul(x, x);
                    answered = true;
                } else if (hy == 0x3FE00000u && (hx & 0x80000000u) == 0u) {  // y is 0.5, x >= +0
                    to_sqrt = true;
                    answered = true;
                }
            }
            Dbl ax{};  // |x|, computed once the answers that do not need it are gone (08B6875C)
            if (!answered) {
                ax = absd(x);
                if (lx == 0u) {
                    if (ix == 0x7FF00000u || ix == 0u || ix == 0x3FF00000u) {
                        // x is +-0, +-inf or +-1: the answer is 1/|x| raised to the sign of the answer of |x|**y.
                        Dbl z = ax;
                        if ((hy & 0x80000000u) != 0u) z = divd(one, z);
                        if ((hx & 0x80000000u) != 0u) {
                            if (((ix - 0x3FF00000u) | yisint) == 0u) {
                                z = divd(sub(z, z), sub(z, z));  // (-1)**non-integer is NaN
                            } else if (yisint == 1u) {
                                z = negated(z);
                            }
                        }
                        answer = z;
                        answered = true;
                    }
                }
            }
            if (!answered) {
                if ((hx & 0x80000000u) != 0u && yisint == 0u) {
                    answer = divd(sub(x, x), sub(x, x));  // (negative)**(non-integer) is NaN
                    answered = true;
                }
            }
            if (!answered && iy > 0x41E00000u) {  // |y| > 2**31
                if (iy > 0x43F00000u) {           // |y| > 2**64 must overflow or underflow
                    if (ix <= 0x3FEFFFFFu) {
                        answer = (hy & 0x80000000u) != 0u ? inf : zero;
                        answered = true;
                    } else if (ix >= 0x3FF00000u) {
                        answer = (hy & 0x80000000u) != 0u ? zero : inf;
                        answered = true;
                    }
                }
                if (!answered) {
                    if (ix < 0x3FEFFFFFu) {
                        answer = (hy & 0x80000000u) != 0u ? inf : zero;
                        answered = true;
                    } else if (ix > 0x3FF00000u) {
                        answer = (hy & 0x80000000u) != 0u ? zero : inf;
                        answered = true;
                    }
                }
            }
            if (!answered) {
                if (iy > 0x41E00000u) {
                    // |1-x| <= 2**-20: log(x) by x-x^2/2+x^3/3-x^4/4, and x has 20 trailing zero bits.
                    const Dbl t = sub(ax, one);
                    const Dbl u = mul(t, ivln2_h);
                    const Dbl t_ivln2_l = mul(t, ivln2_l);
                    const Dbl t2 = mul(t, t);
                    const Dbl w = mul(t2, sub(half, mul(t, add(mul(t, neg_quarter), third))));
                    const Dbl v = add(t_ivln2_l, mul(w, neg_ivln2));
                    const Dbl t1 = add(u, v);
                    const Dbl t1z{0u, t1.hi};
                    answer = finish(t1z, sub(v, sub(t1z, u)));
                } else {
                    // The general path: s = (|x|-1)/(|x|+1) or (|x|-1.5)/(|x|+1.5), log2 by s and a polynomial.
                    Dbl scaled = ax;
                    u32 hi = ax.hi;
                    s32 n = 0;
                    if (ix <= 0x000FFFFFu) {  // subnormal: scale up and remember it in n
                        scaled = mul(scaled, two53);
                        hi = scaled.hi;
                        n = -53;
                    }
                    n += static_cast<s32>(hi >> 20) - 0x3FF;
                    const u32 j = hi & 0x000FFFFFu;
                    u32 interval = j | 0x3FF00000u;
                    u32 k = 0;
                    u32 k_bits = 0;
                    if (j > 0x3988Eu) {
                        if (j <= 0xBB679u) {
                            k = 1u;
                            k_bits = 0x00040000u;
                        } else {
                            n += 1;
                            interval -= 0x00100000u;
                        }
                    }
                    const Dbl axn{scaled.lo, interval};
                    const Dbl bp = load(0x08C96068u + 8u * k);
                    const Dbl u = sub(axn, bp);
                    const Dbl v = divd(one, add(axn, bp));
                    const Dbl s = mul(u, v);
                    const Dbl s_h{0u, s.hi};
                    const Dbl t_h{0u, ((interval >> 1) | 0x20000000u) + 0x00080000u + k_bits};
                    const Dbl t_l = sub(axn, sub(t_h, bp));
                    const Dbl s_l = mul(v, sub(sub(u, mul(s_h, t_h)), mul(s_h, t_l)));
                    const Dbl s2 = mul(s, s);
                    Dbl r = mul(s2, L6);
                    r = add(r, L5);
                    r = mul(s2, r);
                    r = add(r, L4);
                    r = mul(s2, r);
                    r = add(r, L3);
                    r = mul(s2, r);
                    r = add(r, L2);
                    r = mul(s2, r);
                    r = add(r, L1);
                    r = mul(mul(s2, s2), r);
                    r = add(r, mul(s_l, add(s, s_h)));
                    const Dbl s_h_squared = mul(s_h, s_h);
                    const Dbl t_h3 = add(r, add(s_h_squared, three));
                    const Dbl t_h3z{0u, t_h3.hi};
                    const Dbl t_l2 = sub(r, sub(sub(t_h3z, three), s_h_squared));
                    const Dbl uu = mul(s_h, t_h3z);
                    const Dbl vv = add(mul(s_l, t_h3z), mul(s, t_l2));
                    const Dbl p_h = add(uu, vv);
                    const Dbl p_hz{0u, p_h.hi};
                    const Dbl z_h = mul(cp_h, p_hz);
                    const Dbl p_l = sub(vv, sub(p_hz, uu));
                    const Dbl dp_h = load(0x08C96048u + 8u * k);
                    const Dbl z_l = add(add(mul(cp_l, p_hz), mul(p_l, cp)), load(0x08C96058u + 8u * k));
                    const Dbl n_as_double = from_int(n);
                    const Dbl t1 = add(add(add(z_h, z_l), dp_h), n_as_double);
                    const Dbl t1z{0u, t1.hi};
                    answer = finish(t1z,
                                    sub(z_l, sub(sub(sub(t1z, n_as_double), dp_h), z_h)));
                }
            }
        }
    }
    if (to_sqrt) {  // y == 0.5 and x >= +0: the original pops its frame and jumps into the square root
        tail_call(ctx, 0x08B69EBCu, {lx, hx});
        return;
    }
    ctx.gpr[2] = answer.lo;
    ctx.gpr[3] = answer.hi;
}

} // namespace

#define NEW1(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1086::name, gt::ReturnKind::kind)
NEW1(0x08B685B0u, func_08B685B0, DoubleWord);

} // namespace new_batch_main_1086
} // namespace gt::game
