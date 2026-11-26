// Main-session batch 1083: the library's strtod (David Gay's algorithm, soft-float callees), 0x08B7CABC.
// Control flow follows the original's basic blocks (labels carry the block's address); named locals instead of registers.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1083 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

struct Dbl {
    u32 lo;
    u32 hi;
};

constexpr u32 kAdd = 0x08B71328u, kSub = 0x08B712B0u, kMul = 0x08B71394u, kDiv = 0x08B71720u;
constexpr u32 kItod = 0x08B71CE0u, kDtoi = 0x08B71DC8u;
constexpr u32 kCmpGt = 0x08B71C58u, kCmpEq = 0x08B71A08u, kCmpA = 0x08B71B30u, kCmpB = 0x08B71BD0u, kCmpC = 0x08B71A90u;
constexpr u32 kS2b = 0x08B81488u, kI2b = 0x08B815FCu, kMult = 0x08B81630u, kPow5 = 0x08B81874u, kLshift = 0x08B819B4u;
constexpr u32 kDiff = 0x08B81B70u, kCmp = 0x08B81B0Cu, kRatio = 0x08B820E0u, kBalloc = 0x08B82260u, kBfree = 0x08B82318u;
constexpr u32 kD2b = 0x08B81F40u, kUlp = 0x08B81D1Cu, kMemcpy = 0x08B79A88u;
constexpr u32 kTens = 0x08C97640u, kBigTens = 0x08C97618u, kTinyTens = 0x08C975F0u;
constexpr u32 kTwo32 = 0x08CB6950u, kOne = 0x08CB6960u, kHalf = 0x08CB6968u;

void func_08B7CABC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x80u);
    const u32 ptr = ctx.gpr[4];
    const u32 se = ctx.gpr[6];
    u32 s00 = ctx.gpr[5];

    const auto lb = [&](u32 a) -> s32 { return static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(a))); };
    const auto cg2 = [&](u32 target, std::initializer_list<u32> a) { return call_guest(rt, ctx, target, a, {}); };
    const auto cg = [&](u32 target, std::initializer_list<u32> a) -> u32 { return cg2(target, a).v0; };
    const auto dop = [&](u32 target, Dbl a, Dbl b) -> Dbl {
        const GuestResult r = cg2(target, {a.lo, a.hi, b.lo, b.hi});
        return {r.v0, r.v1};
    };
    const auto K = [&](u32 address) -> Dbl { return {word(rt, address), word(rt, address + 4u)}; };
    const auto ten = [&](u32 base, u32 i) -> Dbl { return K(base + 8u * i); };
    const auto sret = [&](u32 target, Dbl a, Dbl b) -> s32 { return static_cast<s32>(cg(target, {a.lo, a.hi, b.lo, b.hi})); };
    const auto itod = [&](u32 v) -> Dbl {
        const GuestResult r = cg2(kItod, {v});
        return {r.v0, r.v1};
    };
    const auto dtoi = [&](Dbl a) -> u32 { return cg(kDtoi, {a.lo, a.hi}); };
    const auto ulp = [&](Dbl a) -> Dbl {
        const GuestResult r = cg2(kUlp, {a.lo, a.hi});
        return {r.v0, r.v1};
    };
    const auto bfree = [&](u32 b) { cg(kBfree, {ptr, b}); };

    u32 slot, sign, cb, a3, nz0, nz, nf, nd, nd0, nd0f, y, z, s0p, t1, a0p, v1u;
    s32 a0, e, v1, L, a1c, k, k2, j, s0, s1, s2, s3, bb2, bb5, bd2, bd5, bs2, bbe, bbbits, cc;
    u32 t2, bd0, bd0x, bd, bb, bs, delta, dsign, yexp, rv0lo, rv0hi, hi, lo;
    Dbl rv, rvc, tmp, tiny, aadj, aadj1, ratio, hs;
    u32 i;

    rv = {0u, 0u};
    slot = s00;
    sign = 0u;
    bd0 = 0u;

L_CB04:
    cb = rt.memory().load8(slot);
    v1 = static_cast<s32>(static_cast<std::int8_t>(cb));
    if (static_cast<u32>(v1) < 0x2Eu) {
        if (v1 == 0) { slot = s00; sign = 0u; goto L_ret; }
        if ((v1 >= 9 && v1 <= 13) || v1 == 32) { a3 = slot + 1u; slot = a3; goto L_CB04; }
        if (v1 == 43) { sign = 0u; goto L_CCD0; }
        if (v1 == 45) { sign = 1u; goto L_CCD0; }
    }
    sign = 0u;
    goto L_CB40;

L_CCD0:
    slot += 1u;
    cb = rt.memory().load8(slot);
    if (cb != 0u) { v1 = static_cast<s32>(static_cast<std::int8_t>(cb)); goto L_CB40; }
    slot = s00;
    goto L_ret;

L_CB40:
    nz0 = 0u;
    v1 = static_cast<s32>(static_cast<std::int8_t>(cb));
    if (v1 == 0x30) goto L_CC18;
    a0 = static_cast<s32>(static_cast<std::int8_t>(cb));
    if (a0 < 0x30) goto L_CB5C;
    a3 = slot;
    if (!(a0 < 0x3A)) goto L_CB60;
    goto L_CC5C;

L_CC18:
    slot += 1u;
    cb = rt.memory().load8(slot);
    v1 = static_cast<s32>(static_cast<std::int8_t>(cb));
    if (v1 == 0x30) goto L_CC18;
    if (v1 == 0) goto L_ret;
    a0 = static_cast<s32>(static_cast<std::int8_t>(cb));
    nz0 = 1u;
    if (a0 < 0x30) goto L_CB5C;
    a3 = slot;
    if (!(a0 < 0x3A)) goto L_CB60;
    goto L_CC5C;

L_CB5C:
    a3 = slot;
L_CB60:
    z = 0u;
    y = 0u;
    nd0 = 0u;
    goto L_CB6C;

L_CC5C:
    nd0 = 0u;
    z = 0u;
    y = 0u;
    y = y * 10u + static_cast<u32>(a0) - 0x30u;
    for (;;) {
        a3 += 1u;
        a0 = lb(a3);
        nd0 += 1u;
        if (a0 < 0x30 || !(a0 < 0x3A)) break;
        if (static_cast<s32>(nd0) < 9) y = y * 10u + static_cast<u32>(a0) - 0x30u;
        else if (static_cast<s32>(nd0) < 16) z = z * 10u + static_cast<u32>(a0) - 0x30u;
    }

L_CB6C:
    s0p = slot;
    if (a0 == '.') goto L_D3D8;
    nd = nd0;
    nz = 0u;
    nf = 0u;
L_CB84:
    if (a0 == 'e') goto L_CD2C;
    slot = a3;
    if (a0 == 'E') goto L_CD2C;
    e = 0;
L_CB9C:
    if (nd != 0u) goto L_CD78;
    if ((nz | nz0) == 0u) slot = s00;
    goto L_ret;

L_D3D8:
    a3 += 1u;
    a0 = lb(a3);
    if (nd0 == 0u) { nz = 0u; goto L_D470; }
    s0p = slot;
    nd = nd0;
    nz = 0u;
    nf = 0u;
L_D3F8:
    t2 = static_cast<u32>(a0) - 0x30u;
    if (!(t2 < 10u)) goto L_CB84;
    nz += 1u;
L_D40C:
    if (t2 == 0u) { a3 += 1u; a0 = lb(a3); goto L_D3F8; }
    nf += nz;
    t1 = nd;
    if (nz >= 2u) {
        i = 1u;
        do {
            t1 += 1u;
            if (static_cast<s32>(t1 - 1u) < 9) y = y * 10u;
            else if (static_cast<s32>(t1) < 17) z = z * 10u;
            i += 1u;
        } while (i != nz);
    }
    nd = t1 + 1u;
    if (static_cast<s32>(t1) < 9) {
        a3 += 1u;
        a0 = lb(a3);
        y = y * 10u + t2;
        nz = 0u;
        goto L_D3F8;
    }
    nz = 0u;
    if (static_cast<s32>(nd) < 17) z = z * 10u + t2;
    a3 += 1u;
    a0 = lb(a3);
    goto L_D3F8;

L_D470:
    if (a0 == '0') {
        u32 v = a3;
        do {
            v += 1u;
            a0 = lb(v);
        } while (a0 == '0');
        nz = v - a3;
        a3 = v;
    }
L_D47C:
    if (!(static_cast<u32>(a0 - 0x31) < 9u)) {
        s0p = slot;
        nf = 0u;
        nd = 0u;
        goto L_CB84;
    }
    nf = nz;
    t2 = static_cast<u32>(a0) - 0x30u;
    s0p = a3;
    nd = 0u;
    nz = 1u;
    goto L_D40C;

L_CD2C:
    if ((nz | nd | nz0) == 0u) { slot = s00; goto L_ret; }
    v1 = lb(a3 + 1u);
    if (v1 == '+') { v1 = lb(a3 + 2u); t1 = 0u; a0p = a3 + 2u; goto L_CD58; }
    a0p = a3 + 1u;
    if (v1 == '-') { v1 = lb(a3 + 2u); t1 = 1u; a0p = a3 + 2u; goto L_CD58; }
    t1 = 0u;
L_CD58:
    if (static_cast<u32>(v1 - 0x30) < 10u) {
        while (lb(a0p) == '0') a0p += 1u;
        v1 = lb(a0p);
        if (!(static_cast<u32>(v1 - 0x31) < 9u)) {
            s00 = a3;
            e = 0;
            slot = a0p;
            goto L_CB9C;
        }
        a1c = lb(a0p + 1u);
        slot = a0p + 1u;
        L = v1 - 0x30;
        while (a1c >= 0x30 && a1c < 0x3A) {
            L = 10 * L + a1c - 0x30;
            slot += 1u;
            a1c = lb(slot);
        }
        if (!(static_cast<s32>(slot - a0p) < 9)) L = 0x98967F;
        s00 = a3;
        e = t1 == 0u ? L : -L;
        goto L_CB9C;
    }
    s00 = a3;
    e = 0;
    slot = a3;
    goto L_CB9C;

L_CD78:
    e = e - static_cast<s32>(nf);
    nd0f = nd;
    if (nd0 != 0u) nd0f = nd0;
    k = static_cast<s32>(nd) < 16 ? static_cast<s32>(nd) : 16;
    rv = itod(y);
    if (static_cast<s32>(y) < 0) rv = dop(kAdd, rv, K(kTwo32));
    if (!(k < 10)) {
        tmp = dop(kMul, rv, ten(kTens, static_cast<u32>(k) - 9u));
        rvc = itod(z);
        if (static_cast<s32>(z) < 0) rvc = dop(kAdd, rvc, K(kTwo32));
        rv = dop(kAdd, tmp, rvc);
    }
    if (static_cast<s32>(nd) < 16) {
        if (e == 0) goto L_ret;
        if (e > 0) {
            if (e < 0x17) {
                rv = dop(kMul, ten(kTens, static_cast<u32>(e)), rv);
                goto L_ret;
            }
            s1 = 15 - static_cast<s32>(nd);
            if (s1 + 0x16 < e) goto L_CE10;
            rv = dop(kMul, ten(kTens, static_cast<u32>(s1)), rv);
            rv = dop(kMul, rv, ten(kTens, static_cast<u32>(e - s1)));
            goto L_ret;
        }
        if (e < -0x16) goto L_CE10;
        rv = dop(kDiv, rv, ten(kTens, static_cast<u32>(-e)));
        goto L_ret;
    }
L_CE10:
    s0 = e + (static_cast<s32>(nd) - k);
    if (s0 > 0) {
        v1u = static_cast<u32>(s0) & 0xFu;
        if (v1u != 0u) rv = dop(kMul, ten(kTens, v1u), rv);
        v1u = static_cast<u32>(s0) & ~0xFu;
        if (v1u == 0u) goto L_CF50;
        if (!(static_cast<s32>(v1u) < 0x135)) { bd0 = 0u; goto L_D774; }
        k2 = static_cast<s32>(v1u) >> 4;
        if (k2 == 0) goto L_CF50;
        j = 0;
        if (k2 >= 2) {
            tmp = rv;
            do {
                if (k2 & 1) tmp = dop(kMul, tmp, ten(kBigTens, static_cast<u32>(j)));
                k2 >>= 1;
                j += 1;
            } while (k2 >= 2);
            rv = tmp;
        }
        rv.hi += 0xFCB00000u;
        rv = dop(kMul, ten(kBigTens, static_cast<u32>(j)), rv);
        hi = rv.hi & 0x7FF00000u;
        if (0x7CA00000u < hi) { bd0 = 0u; goto L_D774; }
        if (!(0x7C900000u < hi)) {
            rv.hi += 0x03500000u;
        } else {
            rv.hi = 0x7FEFFFFFu;
            rv.lo = 0xFFFFFFFFu;
        }
        goto L_CF50;
    }
    if (s0 >= 0) goto L_CF50;
    s0 = -s0;
    v1u = static_cast<u32>(s0) & 0xFu;
    if (v1u != 0u) rv = dop(kDiv, rv, ten(kTens, v1u));
    v1u = static_cast<u32>(s0) & ~0xFu;
    if (v1u == 0u) goto L_CF50;
    k2 = static_cast<s32>(v1u) >> 4;
    if (!(k2 < 0x20)) { bd0 = 0u; goto L_D5D0; }
    j = 0;
    if (k2 >= 2) {
        tmp = rv;
        do {
            if (k2 & 1) tmp = dop(kMul, tmp, ten(kTinyTens, static_cast<u32>(j)));
            k2 >>= 1;
            j += 1;
        } while (k2 >= 2);
        rv = tmp;
    }
    tiny = ten(kTinyTens, static_cast<u32>(j));
    hs = rv;
    rv = dop(kMul, rv, tiny);
    if (sret(kCmpEq, rv, {0u, 0u}) != 0) goto L_CF50;
    tmp = dop(kAdd, hs, hs);
    rv = dop(kMul, tiny, tmp);
    cc = sret(kCmpEq, rv, {0u, 0u});
    rv.hi = 0u;
    rv.lo = 1u;
    if (cc != 0) goto L_CF50;
    bd0 = 0u;
    goto L_D5D0;

L_CF50:
    bd0 = cg(kS2b, {ptr, s0p, nd0f, nd, y});
    bd0x = bd0 + 0xCu;

L_D09C:
    rv0lo = rv.lo;
    rv0hi = rv.hi;
    bd = cg(kBalloc, {ptr, word(rt, bd0 + 4u)});
    cg(kMemcpy, {bd + 0xCu, bd0x, (word(rt, bd0 + 0x10u) << 2) + 8u});
    bb = cg(kD2b, {ptr, 0u, rv0lo, rv0hi, frame.at(0u), frame.at(4u)});
    bs = cg(kI2b, {ptr, 1u});
    bbe = static_cast<s32>(word(rt, frame.at(0u)));
    bbbits = static_cast<s32>(word(rt, frame.at(4u)));
    if (e >= 0) { bd5 = e; bd2 = e; bb5 = 0; bb2 = 0; }
    else { bd5 = 0; bd2 = 0; bb5 = -e; bb2 = -e; }
    if (bbe < 0) bd2 -= bbe;
    else bb2 += bbe;
    {
        s32 jj = bbe + 0x433;
        if (!(bbe + bbbits - 1 < -0x3FE)) jj = 0x36 - bbbits;
        s2 = jj + bd2;
        s1 = jj + bb2;
    }
    s3 = bb2;
    cc = s1 < s2 ? s1 : s2;
    if (cc > s3) cc = s3;
    if (cc > 0) { s3 -= cc; s1 -= cc; s2 -= cc; }
    bs2 = s3;
    bb2 = s1;
    bd2 = s2;
    if (bb5 > 0) {
        bs = cg(kPow5, {ptr, bs, static_cast<u32>(bb5)});
        tmp.lo = cg(kMult, {ptr, bs, bb});
        bfree(bb);
        bb = tmp.lo;
    }
    if (bb2 > 0) bb = cg(kLshift, {ptr, bb, static_cast<u32>(bb2)});
    if (bd5 > 0) bd = cg(kPow5, {ptr, bd, static_cast<u32>(bd5)});
    if (bd2 > 0) bd = cg(kLshift, {ptr, bd, static_cast<u32>(bd2)});
    if (bs2 > 0) bs = cg(kLshift, {ptr, bs, static_cast<u32>(bs2)});
    delta = cg(kDiff, {ptr, bb, bd});
    dsign = word(rt, delta + 0xCu);
    put(rt, delta + 0xCu, 0u);
    cc = static_cast<s32>(cg(kCmp, {delta, bs}));
    if (cc < 0) goto L_D814;
    if (cc == 0) goto L_D888;
    {
        const GuestResult r = cg2(kRatio, {delta, bs});
        ratio = {r.v0, r.v1};
    }
    if (sret(kCmpGt, ratio, K(0x08CB6958u)) <= 0) goto L_CF74;
    aadj = dop(kMul, ratio, K(kHalf));
    aadj1 = aadj;
    if (dsign == 0u) {
        aadj1.hi = 0x80000000u ^ aadj.hi;
        yexp = rv.hi & 0x7FF00000u;
        if (yexp != 0x7FE00000u) goto L_CFAC;
        goto L_D2D0;
    }
    goto L_CF90;

L_CF74:
    if (dsign == 0u) goto L_D570;
    aadj = K(kOne);
    aadj1 = aadj;
L_CF90:
    yexp = rv.hi & 0x7FF00000u;
    if (yexp == 0x7FE00000u) goto L_D2D0;
L_CFAC:
    if (0x03400000u < yexp) goto L_D020;
    if (sret(kCmpA, aadj, K(kOne)) < 0) goto L_D020;
    tmp = dop(kAdd, aadj, K(kHalf));
    tmp = itod(dtoi(tmp));
    aadj1 = tmp;
    if (dsign == 0u) aadj1.hi = 0x80000000u ^ tmp.hi;
L_D020:
    tmp = ulp({rv0lo, rv0hi});
    tmp = dop(kMul, aadj1, tmp);
    rv = dop(kAdd, tmp, {rv0lo, rv0hi});
    hi = rv.hi;
    if (yexp == (hi & 0x7FF00000u)) goto L_D4A4;
L_D06C:
    bfree(bb);
    bfree(bd);
    bfree(bs);
    bfree(delta);
    goto L_D09C;

L_D4A4:
    tmp = itod(dtoi(aadj));
    aadj1 = dop(kSub, aadj, tmp);
    if (dsign == 0u && rv.lo == 0u) {
        if ((rv.hi & 0xFFFFFu) == 0u) {
            if (sret(kCmpB, aadj1, K(0x08CB6990u)) >= 0) goto L_D06C;
            goto L_D52C;
        }
    }
    if (sret(kCmpB, aadj1, K(0x08CB6980u)) < 0) goto L_D52C;
    if (sret(kCmpC, aadj1, K(0x08CB6988u)) <= 0) goto L_D06C;
    goto L_D52C;

L_D2D0:
    rv.hi += 0xFCB00000u;
    tmp = ulp(rv);
    tmp = dop(kMul, aadj1, tmp);
    rv = dop(kAdd, rv, tmp);
    hi = rv.hi & 0x7FF00000u;
    if (!(0x7C9FFFFFu < hi)) {
        rv.hi += 0x03500000u;
        hi = rv.hi;
        if (yexp == (hi & 0x7FF00000u)) goto L_D4A4;
        goto L_D06C;
    }
    if (rv0hi == 0x7FEFFFFFu && rv0lo == 0xFFFFFFFFu) goto L_D774;
    rv.lo = 0xFFFFFFFFu;
    rv.hi = word(rt, 0x08CB65CCu);
    goto L_D06C;

L_D570:
    if (rv.lo != 0u) {
        if (rv.lo != 1u) goto L_D588;
        if (rv.hi != 0u) goto L_D588;
        rv.hi = 0u;
        goto L_D5D0;
    }
    if ((rv.hi & 0xFFFFFu) != 0u) goto L_D588;
    if (sret(kCmpB, ratio, K(kOne)) < 0) {
        aadj1 = K(0x08CB6970u);
        aadj = K(kHalf);
        goto L_CF90;
    }
    aadj = dop(kMul, ratio, K(kHalf));
    aadj1 = {aadj.lo, 0x80000000u ^ aadj.hi};
    goto L_CF90;
L_D588:
    aadj1 = K(0x08CB6978u);
    aadj = K(kOne);
    goto L_CF90;

L_D814:
    if (dsign != 0u) goto L_D52C;
    if (rv.lo != 0u) goto L_D52C;
    if ((rv.hi & 0xFFFFFu) != 0u) goto L_D52C;
    delta = cg(kLshift, {ptr, delta, 1u});
    if (static_cast<s32>(cg(kCmp, {delta, bs})) <= 0) goto L_D52C;
    goto L_D85C;

L_D888:
    if (dsign == 0u) {
        if ((rv.hi & 0xFFFFFu) != 0u) goto L_D8B0;
        if (rv.lo != 0u) goto L_D8B0;
        goto L_D85C;
    }
    if ((rv.hi & 0xFFFFFu) == 0xFFFFFu && rv.lo == 0xFFFFFFFFu) {
        rv.hi = (rv.hi & 0x7FF00000u) + 0x00100000u;
        rv.lo = 0u;
        goto L_D52C;
    }
L_D8B0:
    if ((rv.lo & 1u) == 0u) goto L_D52C;
    if (dsign != 0u) {
        tmp = ulp({rv0lo, rv0hi});
        rv = dop(kAdd, tmp, {rv0lo, rv0hi});
        goto L_D52C;
    }
    tmp = ulp({rv0lo, rv0hi});
    rv = dop(kSub, {rv0lo, rv0hi}, tmp);
    if (sret(kCmpEq, rv, {0u, 0u}) == 0) goto L_D5D0;
    goto L_D52C;

L_D85C:
    rv.hi = ((rv.hi & 0x7FF00000u) + 0xFFF00000u) | 0xFFFFFu;
    rv.lo = 0xFFFFFFFFu;
    goto L_D52C;

L_D774:
    rv.hi = word(rt, 0x08C96694u);
    rv.lo = word(rt, 0x08C96690u);
    put(rt, ptr, 0x22u);
    if (bd0 != 0u) goto L_D52C;
    goto L_ret;

L_D5D0:
    rv.hi = 0u;
    put(rt, ptr, 0x22u);
    rv.lo = 0u;
    if (bd0 != 0u) goto L_D52C;
    goto L_ret;

L_D52C:
    bfree(bb);
    bfree(bd);
    bfree(bs);
    bfree(bd0);
    bfree(delta);
    goto L_ret;

L_ret:
    if (se != 0u) put(rt, se, slot);
    ctx.gpr[2] = rv.lo;
    ctx.gpr[3] = sign != 0u ? (rv.hi ^ 0x80000000u) : rv.hi;
}

} // namespace
} // namespace new_batch_main_1083
} // namespace gt::game

#define NEWM1083(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1083::name, gt::ReturnKind::kind)
NEWM1083(0x08B7CABCu, func_08B7CABC, DoubleWord);
