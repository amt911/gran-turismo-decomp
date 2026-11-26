// New batch, main session 142: large functions the recompiler emitted, none decompiled before, written by hand as structured C
// from `tools.disasm`. Drafted unverified. No names: none has evidence. Stack temporaries a callee receives the address of live in
// the guest frame (GuestFrame); the compiler's dead spill slots (0x200/0x204 and the like) are not reproduced.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_142 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }

// ---- func_08AFED50 ---------------------------------------------------------------------------------------------------
// The object's vector helpers return a two-word result (v0, v1) that the caller parks in the frame and passes on by address.
struct Pair {
    u32 v0;
    u32 v1;
};
Pair call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats = {}) {
    const GuestResult r = call_full(rt, ctx, target, arguments, floats);
    return {r.v0, r.v1};
}
void park(Runtime &rt, u32 at, Pair p) {
    put(rt, at, p.v0);
    put(rt, at + 4u, p.v1);
}

// The inlined cross product of two 4-float vectors: `pair` holds the pointers (q, r); the result goes to out[0..3].
void cross_into(Runtime &rt, u32 pair, u32 out) {
    const u32 q = word(rt, pair);
    const u32 r = word(rt, pair + 4u);
    const float q0 = float_at(rt, q), q1 = float_at(rt, q + 4u), q2 = float_at(rt, q + 8u), q3 = float_at(rt, q + 0xCu);
    const float r0 = float_at(rt, r), r1 = float_at(rt, r + 4u), r2 = float_at(rt, r + 8u), r3 = float_at(rt, r + 0xCu);
    const float t10 = psp_mul(q0, r2);
    const float t15 = psp_mul(q0, r0);
    const float t14 = psp_mul(q2, r1);
    const float t11 = psp_mul(q1, r0);
    float x = psp_mul(q1, r2);   // f2
    float w = psp_mul(q3, r3);   // f3
    float z = psp_mul(q2, r0);   // f1
    float y = psp_mul(q0, r1);   // f0
    x = x - t14;
    const float t4 = psp_mul(q0, r3);
    w = w - t15;
    y = y - t11;
    z = z - t10;
    const float t10b = psp_mul(q1, r1);
    const float t5 = psp_mul(q1, r3);
    const float t6 = psp_mul(q2, r3);
    const float t8 = psp_mul(q3, r0);
    const float t7 = psp_mul(q2, r2);
    const float t9 = psp_mul(q3, r1);
    w = w - t10b;
    const float t13 = psp_mul(q3, r2);
    x = x + t4;
    z = z + t5;
    y = y + t6;
    w = w - t7;
    x = x + t8;
    z = z + t9;
    y = y + t13;
    put_float(rt, out + 0xCu, w);
    put_float(rt, out, x);
    put_float(rt, out + 4u, z);
    put_float(rt, out + 8u, y);
}

struct Pass {
    u32 first_a1;                // a1 of the first helper (a0 is the frame vector E0)
    u32 park_a, park_b;          // where its result and the next one are parked
    u32 cross_buf, cross_out;    // the crossed vector's pair target, and where the cross product goes
    float scale;
    u32 park_c, park_d;
    u32 tmp, res, mid;           // the three vectors the fold works on
};

// One of the three identical rounds: a vector built from the frame vector E0 is scaled and crossed, folded through the object's
// helpers and the result applied to the running vector at s4.
void round_of_func_08AFED50(Runtime &rt, AllegrexContext &ctx, u32 self, u32 s2, u32 s7, u32 s4, u32 e0, const Pass &p) {
    park(rt, p.park_a, call_pair(rt, ctx, 0x08BAC248u, {e0, p.first_a1}));
    const Pair b = call_pair(rt, ctx, 0x08C45FE0u, {p.park_a}, {0.5f});
    park(rt, p.park_b, b);
    cross_into(rt, b.v1, p.cross_out);
    call(rt, ctx, 0x08C466E0u, {p.cross_buf, p.park_b, p.cross_out, b.v1});
    park(rt, p.park_c, call_pair(rt, ctx, 0x08C45EF0u, {p.cross_buf}, {p.scale}));
    park(rt, p.park_d, call_pair(rt, ctx, 0x08C45FF8u, {self, p.park_c}));
    const u32 holder = call(rt, ctx, 0x08C45DD8u, {p.park_d});
    const u32 inner = word(rt, holder + 4u);
    call(rt, ctx, 0x08C466E0u, {p.tmp, inner, word(rt, inner + 4u)});
    call(rt, ctx, 0x08C4669Cu, {p.res, word(rt, holder), p.tmp});
    call(rt, ctx, 0x08C4677Cu, {p.mid, p.res});
    call(rt, ctx, 0x08AFE3F8u, {self, s2, p.mid});
    const Pair e = call_pair(rt, ctx, 0x08C45E2Cu, {s2, s7});
    call(rt, ctx, 0x08C468E4u, {s4, e.v0, e.v1});
}

// 0x08AFED50 (a0 = object, a1 = out vector, a2 = a pointer, f12 = an angle): nothing when the float at +0x60 of the object's
// +0xD8 member is zero. Otherwise three rounds of the fold above (the first from the object's vector at +0x50) and a last
// combination of the results, written to a1.

// ---- func_08B7BC8C (a strftime: buf, size, format, tm) ---------------------------------------------------------------
u32 signed_div7(s32 t) {
    const std::int64_t product = static_cast<std::int64_t>(t) * static_cast<std::int64_t>(static_cast<s32>(0x92492493u));
    const s32 high = static_cast<s32>(product >> 32);
    return static_cast<u32>(((high + t) >> 2) - (t >> 31));
}
u32 signed_div12(s32 t) {
    const std::int64_t product = static_cast<std::int64_t>(t) * static_cast<std::int64_t>(0x2AAAAAABll);
    const s32 high = static_cast<s32>(product >> 32);
    return static_cast<u32>((high >> 1) - (t >> 31));
}
u32 signed_div100(s32 t) {
    const std::int64_t product = static_cast<std::int64_t>(t) * static_cast<std::int64_t>(0x51EB851Fll);
    const s32 high = static_cast<s32>(product >> 32);
    return static_cast<u32>((high >> 5) - (t >> 31));
}

constexpr u32 kSprintf = 0x08B37640u;
constexpr u32 kFormatAt2 = 0x08CB5FC0u;   // "%02d"-like formats, per the call sites below
constexpr u32 kWeekdayLengths = 0x08C96F00u, kWeekdayNames = 0x08C96F1Cu;
constexpr u32 kMonthLengths = 0x08C96EA0u, kMonthNames = 0x08C96ED0u;

struct Strftime {
    Runtime &rt;
    AllegrexContext &ctx;
    u32 buf, size, tm;
    u32 n = 0;   // s0: bytes written

    bool room(u32 reserve) const { return n < size - reserve; }
    void put_byte(u32 at, u32 value) { rt.memory().store8(at, static_cast<std::uint8_t>(value)); }
    void sprint(u32 at, u32 format, std::initializer_list<u32> rest) {
        std::uint32_t args[8] = {at, format};
        unsigned k = 2;
        for (u32 r : rest) args[k++] = r;
        switch (k) {
        case 3: call(rt, ctx, kSprintf, {args[0], args[1], args[2]}); break;
        case 4: call(rt, ctx, kSprintf, {args[0], args[1], args[2], args[3]}); break;
        case 5: call(rt, ctx, kSprintf, {args[0], args[1], args[2], args[3], args[4]}); break;
        case 6: call(rt, ctx, kSprintf, {args[0], args[1], args[2], args[3], args[4], args[5]}); break;
        default: call(rt, ctx, kSprintf, {args[0], args[1], args[2], args[3], args[4], args[5], args[6]}); break;
        }
    }
    // A name copied whole (%A, %B): `field` of tm indexes the length table and the name table.
    // false: the buffer filled up first.
    bool whole_name(u32 field, u32 lengths, u32 names) {
        const u32 length_at = lengths + (word(rt, tm + field) << 2);
        if (static_cast<s32>(word(rt, length_at)) <= 0) return true;
        const u32 last = size - 1u;
        if (!(n < last)) return false;
        u32 out = buf + n;
        u32 count = 0;
        u32 position = n;
        for (;;) {
            const u32 pointer = word(rt, names + (word(rt, tm + field) << 2));
            put_byte(out, rt.memory().load8(pointer + count));
            ++count;
            ++position;
            if (!(static_cast<s32>(count) < static_cast<s32>(word(rt, lengths + (word(rt, tm + field) << 2))))) {
                n = position;
                return true;
            }
            ++out;
            if (count == last - n) return false;
        }
    }
    // Three characters of an abbreviated name at `at`; `index` is the running output index (s0 or s5), advanced.
    void abbreviated(u32 &index, u32 field, u32 names) {
        for (u32 k = 0; k < 3u; ++k) {
            const u32 pointer = word(rt, names + (word(rt, tm + field) << 2));
            put_byte(buf + index, rt.memory().load8(pointer + k));
            ++index;
        }
    }
    // %a / %b: three characters, the room checked before each. false: no room.
    bool abbreviated_checked(u32 field, u32 names) {
        const u32 last = size - 1u;
        u32 out = buf + n;
        for (u32 k = 0; k < 3u; ++k) {
            if (!(n < last)) return false;
            const u32 pointer = word(rt, names + (word(rt, tm + field) << 2));
            ++n;
            put_byte(out, rt.memory().load8(pointer + k));
            ++out;
        }
        return true;
    }
    // %c / %x: "Www Mmm " then a formatted rest. The caller checked the room.
    void date_prefix(u32 &s5) {
        abbreviated(n, 0x18u, kWeekdayNames);
        s5 = n + 1u;
        put_byte(buf + n, 0x20u);
        abbreviated(s5, 0x10u, kMonthNames);
    }
    // 0: failed (return 0); 1: the specifier is finished.
    bool convert(u32 spec, u32 handler);
};

bool Strftime::convert(u32 spec, u32 handler) {
    const u32 year = tm + 0x14u;
    switch (spec) {
    case '%':
        if (!room(1u)) return false;
        put_byte(buf + n, 0x25u);
        ++n;
        return true;
    case 'A': return whole_name(0x18u, kWeekdayLengths, kWeekdayNames);
    case 'B': return whole_name(0x10u, kMonthLengths, kMonthNames);
    case 'M':
    case 'S': {
        if (!room(2u)) return false;
        const u32 value = word(rt, tm + (spec == 'M' ? 4u : 0u));
        sprint(buf + n, kFormatAt2, {value});
        n += 2u;
        return true;
    }
    case 'U':
    case 'W': {
        if (!room(2u)) return false;
        const u32 wday = word(rt, tm + 0x18u);
        const u32 yday = word(rt, tm + 0x1Cu);
        u32 t;
        if (spec == 'U') {
            t = yday - wday + 7u;
        } else {
            const u32 shift = wday != 0u ? wday - 1u : 6u;
            t = yday - shift + 7u;
        }
        sprint(buf + n, kFormatAt2, {signed_div7(static_cast<s32>(t))});
        n += 2u;
        return true;
    }
    case 'X': {
        if (!room(8u)) return false;
        const u32 hour = word(rt, tm + 8u), minute = word(rt, tm + 4u), second = word(rt, tm);
        sprint(buf + n, 0x08CB5FE4u, {hour, minute, second});
        n += 8u;
        return true;
    }
    case 'Y':
        if (!room(4u)) return false;
        sprint(buf + n, 0x08CB5FF8u, {word(rt, year) + 0x76Cu});
        n += 4u;
        return true;
    case 'Z': {
        if (static_cast<s32>(word(rt, tm + 0x20u)) < 0) return true;
        call(rt, ctx, 0x08B7E8C8u, {handler, spec});
        const u32 name = word(rt, 0x08CE1238u + (word(rt, tm + 0x20u) << 2));
        const u32 length = call(rt, ctx, 0x08B35F60u, {name});
        if (static_cast<s32>(length) <= 0) {
            call(rt, ctx, 0x08B7E8D0u, {});
            return true;
        }
        const u32 last = size - 1u;
        bool ok = false;
        if (n < last) {
            u32 out = buf + n;
            u32 index = n;
            u32 count = 0;
            for (;;) {
                ++index;
                const u32 pointer = word(rt, 0x08CE1238u + (word(rt, tm + 0x20u) << 2));
                const u32 byte = rt.memory().load8(pointer + count);
                ++count;
                put_byte(out, byte);
                if (length == count) {
                    n = index;
                    ok = true;
                    break;
                }
                if (count == last - n) break;
                ++out;
            }
        }
        call(rt, ctx, 0x08B7E8D0u, {});
        if (!ok) n = 0u;
        return ok;
    }
    case 'a': return abbreviated_checked(0x18u, kWeekdayNames);
    case 'b':
    case 'h': return abbreviated_checked(0x10u, kMonthNames);
    case 'c': {
        if (!room(0x18u)) return false;
        u32 s5;
        date_prefix(s5);
        sprint(buf + s5, 0x08CB5F94u,
               {word(rt, tm + 0xCu), word(rt, tm + 8u), word(rt, tm + 4u), word(rt, tm), word(rt, year) + 0x76Cu});
        n = s5 + 0x11u;
        return true;
    }
    case 'x': {
        if (!room(0xFu)) return false;
        u32 s5;
        date_prefix(s5);
        sprint(buf + s5, 0x08CB5FD8u, {word(rt, tm + 0xCu), word(rt, year) + 0x76Cu});
        n = s5 + 8u;
        return true;
    }
    case 'd':
        if (!room(2u)) return false;
        sprint(buf + n, 0x08CB5FB4u, {word(rt, tm + 0xCu)});
        n += 2u;
        return true;
    case 'e':
        if (!room(2u)) return false;
        sprint(buf + n, 0x08CB5FBCu, {word(rt, tm + 0xCu)});
        n += 2u;
        return true;
    case 'j':
        if (!room(3u)) return false;
        sprint(buf + n, 0x08CB5FC8u, {word(rt, tm + 0x1Cu) + 1u});
        n += 3u;
        return true;
    case 'H':
    case 'k':
        if (!room(2u)) return false;
        sprint(buf + n, spec == 'k' ? 0x08CB5FBCu : kFormatAt2, {word(rt, tm + 8u)});
        n += 2u;
        return true;
    case 'I':
    case 'l': {
        if (!room(2u)) return false;
        const u32 hour = word(rt, tm + 8u);
        if (hour == 0u || hour == 12u) {
            put_byte(buf + n + 1u, 0x32u);
            put_byte(buf + n, 0x31u);
            n += 2u;
            return true;
        }
        const u32 at = buf + n;
        n += 2u;
        const u32 twelve = hour - (signed_div12(static_cast<s32>(hour)) * 12u);
        sprint(at, spec == 'I' ? 0x08CB5FB4u : 0x08CB5FBCu, {twelve, hour});
        return true;
    }
    case 'm':
        if (!room(2u)) return false;
        sprint(buf + n, 0x08CB5FB4u, {word(rt, tm + 0x10u) + 1u});
        n += 2u;
        return true;
    case 'p':
        if (!room(2u)) return false;
        if (static_cast<s32>(word(rt, tm + 8u)) < 12) {
            put_byte(buf + n, 0x41u);
        } else {
            put_byte(buf + n, 0x50u);
        }
        put_byte(buf + n + 1u, 0x4Du);
        n += 2u;
        return true;
    case 'w':
        if (!room(1u)) return false;
        sprint(buf + n, 0x08CB5FD0u, {word(rt, tm + 0x18u)});
        n += 1u;
        return true;
    case 'y': {
        if (!room(2u)) return false;
        u32 value = word(rt, year);
        value = value - signed_div100(static_cast<s32>(value)) * 100u + 100u;
        const u32 rest = value - signed_div100(static_cast<s32>(value)) * 100u;
        sprint(buf + n, kFormatAt2, {rest, value});
        n += 2u;
        return true;
    }
    default:
        return true;
    }
}

// 0x08B7BC8C (a0 = buffer, a1 = its size, a2 = format, a3 = a broken-down time): a strftime. Returns the length written (the
// terminator is stored at that index), or 0 when the buffer fills (nothing terminated). Which specifiers the jump table at
// 0x08C96F38 sends to a handler: % A B H I M S U W X Y Z a b c d e h j k l m p w x y; every other character is skipped.
void func_08B7BC8C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    Strftime out{rt, ctx, ctx.gpr[4], ctx.gpr[5], ctx.gpr[7]};
    u32 p = ctx.gpr[6];
    const auto last_byte = [&](u32 at) { return static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(at))); };
    bool failed = false;
    for (;;) {
        const s32 ch = last_byte(p);
        if (ch == 0) break;
        if (ch != 0x25) {
            if (!out.room(1u)) { failed = true; break; }
            out.put_byte(out.buf + out.n, static_cast<u32>(ch));
            ++p;
            ++out.n;
            continue;
        }
        const u32 spec = static_cast<u32>(last_byte(p + 1u));
        if (spec - 0x25u < 0x55u) {
            const u32 handler = word(rt, 0x08C96F38u + ((spec - 0x25u) << 2));
            if (!out.convert(spec, handler)) { failed = true; break; }
        }
        if (last_byte(p + 1u) == 0) break;
        p += 2u;
    }
    if (failed) {
        ctx.gpr[2] = 0u;
        return;
    }
    out.put_byte(out.buf + out.n, 0u);
    ctx.gpr[2] = out.n;
}

} // namespace
} // namespace new_batch_main_142
} // namespace gt::game

#define NEW_MAIN142(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_142::name, gt::ReturnKind::kind)
NEW_MAIN142(0x08B7BC8Cu, func_08B7BC8C, Word);
