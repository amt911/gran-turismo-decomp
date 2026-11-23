// New batch main 696 (main session): functions the extra seeds made emittable, hand-written as structured C from
// `tools.disasm`. Draft only — not yet through the differential gate. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_696 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same, keeping v0 / v1 / f0 as the callee left them (a caller that reads v1 or $f0 after the call).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A call with float arguments ($f12-$f15); its float result ($f0).
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats = {}) {
    return call_keep(rt, ctx, target, arguments, floats).f0;
}

// The two-word result of a function returning a small struct in v0 / v1.
struct Words {
    u32 first;
    u32 second;
};
Words call_words(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_keep(rt, ctx, target, arguments);
    return Words{result.v0, result.v1};
}

// `lw v0, off(vtable); jalr v0`: v1 = the table, v0 = the slot's function before the call, arguments in a0 onwards.
// Afterwards v0 / v1 / f0 are the callee's, as after the `jalr`.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                         std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}

// The callee-saved registers a body keeps state in (s0 onwards): the original puts the caller's back on every exit.
template <unsigned Count>
struct SavedS {
    AllegrexContext &ctx;
    u32 saved[Count];
    explicit SavedS(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < Count; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedS() {
        for (unsigned i = 0u; i < Count; ++i) ctx.gpr[16u + i] = saved[i];
    }
    SavedS(const SavedS &) = delete;
    SavedS &operator=(const SavedS &) = delete;
};
// $fp, which some prologues save too.
struct SavedFp {
    AllegrexContext &ctx;
    u32 saved;
    explicit SavedFp(AllegrexContext &c) : ctx(c), saved(c.gpr[30]) {}
    ~SavedFp() { ctx.gpr[30] = saved; }
    SavedFp(const SavedFp &) = delete;
    SavedFp &operator=(const SavedFp &) = delete;
};

// A run of lwl/lwr or lwc1/swc1 pairs is one unaligned word copy.
void copy_bytes(Runtime &rt, u32 to, u32 from, u32 bytes) {
    for (u32 i = 0u; i < bytes; i += 4u) put(rt, to + i, word(rt, from + i));
}

// The `ext` instruction: `size` bits of `value` from bit `position` up.
u32 bits(u32 value, unsigned position, unsigned size) { return (value >> position) & ((1u << size) - 1u); }

// ---------------------------------------------------------------------------------------------------------------
// 0x08B03448: the closest approach of two 2-D segments, both parameters clamped into [0,1].
// f22 = d(a1,a1), f21 = d(a3,a3), f24 = d(a1,&tmp), f23 = d(a3,&tmp) through 0x08BBBEC4 (float results). The three
// degenerate cases write zeros (out1 = t0, out2 = t1); the general one solves for out1's parameter and recomputes
// out2's from it. The 0x08BAC270 / 0x08BAC428 pair only fills a stack temporary. v0 ends as the last call's.
float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (!(x < 1.0f)) return 1.0f;
    return x;
}
void func_08B03448(Runtime &rt, AllegrexContext &ctx) {
    const u32 p1 = ctx.gpr[5];
    const u32 p2 = ctx.gpr[6];
    const u32 p3 = ctx.gpr[7];
    const u32 out1 = ctx.gpr[8];
    const u32 out2 = ctx.gpr[9];
    SavedS<5> saved(ctx);
    GuestFrame frame(ctx, 0x70u);
    ctx.gpr[18] = p1;
    ctx.gpr[20] = out2;
    ctx.gpr[19] = out1;
    ctx.gpr[17] = p3;
    const u32 point = frame.at(8u);
    ctx.gpr[16] = point;
    const Words pair = call_words(rt, ctx, 0x08BAC270u, {ctx.gpr[4], p2});
    put(rt, frame.at(0x20u), pair.first);
    put(rt, frame.at(0x24u), pair.second);
    put(rt, frame.at(0u), pair.first);
    put(rt, frame.at(4u), pair.second);
    call(rt, ctx, 0x08BAC428u, {point, pair.first, pair.second});
    const float f22 = call_float(rt, ctx, 0x08BBBEC4u, {p1, p1});
    const float f21 = call_float(rt, ctx, 0x08BBBEC4u, {p3, p3});
    const float f24 = call_float(rt, ctx, 0x08BBBEC4u, {p1, point});
    const float f23 = call_float(rt, ctx, 0x08BBBEC4u, {p3, point});
    if (f22 == 0.0f) {
        put_float(rt, out1, 0.0f);  // the delay slot of the second test runs on both of its paths
        if (f21 == 0.0f) {
            put_float(rt, out2, 0.0f);
            return;
        }
        put_float(rt, out2, clamp01(f23 / f21));
        return;
    }
    if (f21 == 0.0f) {
        put_float(rt, out1, clamp01(-f24 / f22));
        put(rt, out2, 0u);
        return;
    }
    const float distance = call_float(rt, ctx, 0x08BBBEC4u, {p1, p3});
    const float denominator = f22 * f21 - distance * distance;
    if (denominator == 0.0f) {
        put_float(rt, out1, 0.0f);
    } else {
        put_float(rt, out1, clamp01((f23 * distance - f21 * f24) / denominator));
    }
    const float t = (f23 + distance * float_at(rt, out1)) / f21;
    put_float(rt, out2, t);
    if (t < 0.0f) {
        const float u = -f24 / f22;
        put_float(rt, out2, 0.0f);
        if (u < 0.0f) {
            put_float(rt, out1, 0.0f);
        } else {
            put_float(rt, out1, clamp01(u));
        }
    } else if (1.0f < t) {
        const float u = (distance - f24) / f22;
        put_float(rt, out2, 1.0f);
        if (u < 0.0f) {
            put_float(rt, out1, 0.0f);
        } else {
            put_float(rt, out1, clamp01(u));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08AEF1D0 (a0 = selector, a1 = source values): fixed-point (12-bit fraction) geometry setup.
// s3 = a0 (mode at +0x18, index at +0x1C), s1 = a1. The two 0x08AF0E9C lookups scale a1's +4 / +8; a table entry at
// 0x08CE09D0 picked by 2*[a1+0x24] + [a1+0x1C] scales them again; 0x08AD9860 picks between the two (when it answers 0
// they are averaged); then the mode selects which block of state (0x08D41AD0 rows and the pointer at +0x4B0) gets the
// results.
s32 mul32(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); }
s32 fixed12(s32 a, s32 b) { return mul32(a, b) >> 12; }
void func_08AEF1D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedS<4> saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = self;
    ctx.gpr[17] = source;
    const s32 base = static_cast<s32>(bits(word(rt, source + 0xCu), 1, 12));
    const u32 lookup1 = call(rt, ctx, 0x08AF0E9Cu, {static_cast<u32>(base + 0x600)});
    const s32 scale_a = fixed12(static_cast<s32>(word(rt, source + 4u)), static_cast<s32>(lookup1));
    const u32 lookup2 = call(rt, ctx, 0x08AF0E9Cu, {static_cast<u32>(base + 0x200)});
    const s32 scale_b = fixed12(static_cast<s32>(word(rt, source + 8u)), static_cast<s32>(lookup2));
    const u32 entry = 0x08CE09D0u + 8u * (2u * word(rt, source + 0x24u) + word(rt, source + 0x1Cu));
    const s32 raw_a = mul32(scale_b, static_cast<s32>(word(rt, entry + 4u)));
    const s32 value_a = raw_a >> 12;
    const s32 raw_b = mul32(scale_a, static_cast<s32>(word(rt, entry)));
    s32 value_b = raw_b >> 12;
    s32 value_a_kept = value_a;
    const u32 product_for_call = mul32(static_cast<s32>(word(rt, source + 8u)), static_cast<s32>(lookup2));
    if (call(rt, ctx, 0x08AD9860u, {static_cast<u32>(raw_a), product_for_call}) == 0u) {
        value_a_kept = static_cast<s32>(static_cast<u32>(value_b) + static_cast<u32>(value_a_kept)) >> 1;
        value_b = value_a_kept;
    }
    const s32 selector = static_cast<s32>(word(rt, self + 0x18u));
    const u32 index = word(rt, self + 0x1Cu);
    if (selector == 1) {
        const u32 row = 0x08D41AD0u + 8u * index;
        const s32 field14 = static_cast<s32>(word(rt, source + 0x14u));
        const s32 field18 = static_cast<s32>(word(rt, source + 0x18u));
        rt.memory().store16(row + 0x1A4u, static_cast<u16>(fixed12(value_b, field14)));
        rt.memory().store16(row + 0x1A6u, static_cast<u16>(fixed12(value_a_kept, field14)));
        rt.memory().store16(row + 0x1A8u, static_cast<u16>(fixed12(value_b, field18)));
        rt.memory().store16(row + 0x1AAu, static_cast<u16>(fixed12(value_a_kept, field18)));
        const u32 state = word(rt, 0x08D41AD0u + 0x4B0u) + 0x6Cu * index;
        u32 flags = word(rt, state + 4u) | 0x40u;
        put(rt, state + 4u, flags);
        put(rt, state + 0x1Cu, static_cast<u32>(fixed12(value_b, field14)));
        flags |= 0x80u;
        put(rt, state + 4u, flags);
        put(rt, state + 0x20u, static_cast<u32>(fixed12(value_a_kept, field14)));
        flags |= 0x100u;
        put(rt, state + 4u, flags);
        put(rt, state + 0x24u, static_cast<u32>(fixed12(value_b, field18)));
        put(rt, state + 0x28u, static_cast<u32>(fixed12(value_a_kept, field18)));
        flags |= 0x200u;
        put(rt, state + 4u, flags);
        ctx.gpr[2] = flags;
        return;
    }
    if (selector == 2) {
        const u32 row = 0x08D41AD0u + 8u * index;
        const s32 field14 = static_cast<s32>(word(rt, source + 0x14u));
        rt.memory().store16(row + 0x2A4u, static_cast<u16>(fixed12(value_b, field14)));
        rt.memory().store16(row + 0x2A6u, static_cast<u16>(fixed12(value_a_kept, field14)));
        const s32 second = fixed12(value_a_kept, field14);
        const s32 first = fixed12(value_b, field14);
        call(rt, ctx, 0x08AEDB60u, {index, static_cast<u32>(first), static_cast<u32>(second)});
        return;
    }
    ctx.gpr[2] = 2u;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x089164E8 (a0 = object, a1 = a 4-row matrix to fill, f12 = a scale): builds two anchors through 0x088D32E0 (the
// second at scale + 1), a basis from their cross product (0x08B4A8D4), the up vector, the normalised anchor
// (0x08B4A744 reads the length, 0x08B4A6A0 scales), a second cross (0x08B4A82C), then a matrix whose rows are the
// cross product, the up vector, the normalised anchor and the first anchor (w = 1). Finally the four corner rows
// (0.5 / 0.2 / -0.5 / 1.2, w = 1) at a1 are transformed in place by that matrix through 0x08B4A774.

// ---------------------------------------------------------------------------------------------------------------
// 0x08B62A04 (a0 = a container header, a1 = argument passed to every virtual, a2 = an offset table or null,
// a3 = a small state record at +0 / +4 / +8 / +0xC): walks the container's entries backwards. Each entry is a word
// of flags at +0xC of the entry and an object at +0x10; the flags' bit 1 (or the container flag's bit 0) decides
// whether the object's virtual at +0x18 is asked, the callee fills the four words at sp, and the state record's
// fields are updated from them. Audio-like update: the exits are 1 (busy) or whether +4 of the record is set.
void func_08B62A04(Runtime &rt, AllegrexContext &ctx) {
    const u32 header = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 table = ctx.gpr[6];
    const u32 state = ctx.gpr[7];
    SavedS<8> saved(ctx);
    SavedFp saved_fp(ctx);
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10u), header);
    put(rt, frame.at(0x14u), argument);
    if (call(rt, ctx, 0x08B62884u, {header, argument}) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    u32 flags = word(rt, state + 8u);
    if ((flags & 0x10u) != 0u) flags = word(rt, header + 8u);
    const u32 count = word(rt, header + 0xCu);
    for (u32 i = 0u; i < count; ++i) {
        const u32 slot = count - 1u - i;
        const u32 field = word(rt, header + 0x14u + slot * 8u);
        const u32 bit1 = (field >> 1) & 1u;
        const s32 offset = static_cast<s32>(field) >> 8;
        const u32 low = field & 1u;
        put(rt, frame.at(0u), 0u);
        put(rt, frame.at(8u), flags);
        put(rt, frame.at(0xCu), 0u);
        if (bit1 == 0u && (flags & 1u) == 0u) continue;
        u32 extra = 0u;
        if (table != 0u) {
            if (low != 0u) extra = table + word(rt, word(rt, table) + static_cast<u32>(offset));
            else extra = table + static_cast<u32>(offset);
        }
        const u32 object = word(rt, header + 0x10u + slot * 8u);
        if (call_virtual(rt, ctx, object, 0x18u,
                         {object, word(rt, frame.at(0x14u)), extra, frame.at(0u)})
                .v0 == 0u) {
            continue;
        }
        u32 scratch = word(rt, frame.at(0xCu));
        if (scratch == 8u && low != 0u) {
            scratch = object;
            put(rt, frame.at(0xCu), scratch);
        }
        u32 flag = word(rt, frame.at(4u));
        if (!(static_cast<s32>(flag) < 4) && bit1 == 0u) {
            flag &= ~2u;
            put(rt, frame.at(4u), flag);
        }
        const u32 inner = word(rt, state + 0xCu);
        if (inner == 0u) {
            put(rt, state + 8u, word(rt, frame.at(8u)));
            put(rt, state + 0u, word(rt, frame.at(0u)));
            put(rt, state + 0xCu, scratch);
            put(rt, state + 4u, flag);
            if (static_cast<s32>(flag) < 4) {
                ctx.gpr[2] = 1u;
                return;
            }
            if ((flag & 2u) != 0u) {
                if ((word(rt, header + 8u) & 1u) != 0u) continue;
                ctx.gpr[2] = 1u;
                return;
            }
            if ((flag & 1u) == 0u) {
                ctx.gpr[2] = 1u;
                return;
            }
            if ((word(rt, header + 8u) & 2u) != 0u) continue;
            ctx.gpr[2] = 1u;
            return;
        }
        const u32 stored = word(rt, state + 0u);
        if (stored != word(rt, frame.at(0u))) {
            put(rt, state + 4u, 2u);
            put(rt, state + 0u, 0u);
            ctx.gpr[2] = 1u;
            return;
        }
        if (stored != 0u) {
            put(rt, state + 4u, word(rt, state + 4u) | flag);
            continue;
        }
        if (scratch == 8u || inner == 8u) {
            put(rt, state + 4u, 2u);
            ctx.gpr[2] = 1u;
            return;
        }
        if (word(rt, scratch + 4u) == word(rt, inner + 4u)) {
            put(rt, state + 4u, word(rt, state + 4u) | flag);
            continue;
        }
        put(rt, state + 4u, 2u);
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = word(rt, state + 4u) != 0u ? 1u : 0u;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08BE1DA4 (a0 = object): destructor with intrusive reference counts. The vtable 0x08CC68D0 goes in first, then the
// shared holders at +0x68, +0x4C, +0x44, +0x3C and +0x2C have their +4 and +8 counts decremented, each reaching 0
// running the virtuals at vtable +8 / +0xC. The +0x50 buffer is freed (0x08B93554) unless it is the 0x08D75298
// sentinel or 0x08B60804(buffer - 4, -1) still counts users. The +0x24 holder is released through +8 / vtable +0xC
// only, and 0x08B64654(this) closes. v0 is that last call's.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 users = word(rt, object + 4u);
    put(rt, object + 4u, users - 1u);
    if (users != 1u) return;
    call_virtual(rt, ctx, object, 8u, {object});
    const u32 counters = word(rt, object + 8u);
    put(rt, object + 8u, counters - 1u);
    if (counters != 1u) return;
    call_virtual(rt, ctx, object, 0xCu, {object});
}
void release_holder_tail(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 users = word(rt, object + 8u);
    put(rt, object + 8u, users - 1u);
    if (users != 1u) return;
    call_virtual(rt, ctx, object, 0xCu, {object});
}
void func_08BE1DA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS<3> saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CC68D0u);
    release_holder(rt, ctx, word(rt, self + 0x68u));
    const u32 buffer = word(rt, self + 0x50u);
    const u32 inner = buffer - 0xCu;
    if (inner != 0x08D75298u) {
        const u32 users = call(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu});
        if (!(static_cast<s32>(users) > 0)) call(rt, ctx, 0x08B93554u, {inner, frame.at(0u)});
    }
    release_holder(rt, ctx, word(rt, self + 0x4Cu));
    const u32 members = self + 0x40u;  // the asm's s2: the second holder is at s2 + 4
    release_holder(rt, ctx, word(rt, members + 4u));
    release_holder(rt, ctx, word(rt, self + 0x3Cu));
    put(rt, self, 0x08CC68F0u);
    release_holder(rt, ctx, word(rt, self + 0x2Cu));
    release_holder_tail(rt, ctx, word(rt, self + 0x24u));
    put(rt, self, 0x08CC3000u);
    call(rt, ctx, 0x08B64654u, {self});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B6D284 (a0:a1 = a double, a2:a3 = another, t0 = a flag): a chain of 64-bit operations. A magnitude at or below
// 0x3E3FFFFF asks 0x08B71DC8 first; a non-zero answer (and the > case always) runs the 0x08B71394 / 0x08B712B0 /
// 0x08B71328 chain with the 64-bit constants at 0x08CB68A0..0x08CB68D4; t0 == 0 takes a shorter chain. v0:v1 is the
// 64-bit result (the inputs themselves on the early exit).
void func_08B6D284(Runtime &rt, AllegrexContext &ctx) {
    const u32 first0 = ctx.gpr[4];
    const u32 first1 = ctx.gpr[5];
    const u32 second0 = ctx.gpr[6];
    const u32 second1 = ctx.gpr[7];
    const u32 flag = ctx.gpr[8];
    SavedS<8> saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(8u), second0);
    put(rt, frame.at(0xCu), second1);
    bool work = (first1 & ~(1u << 31)) > 0x3E3FFFFFu;  // ins t2, zero, 31, 1 clears the sign bit
    if (!work) work = call(rt, ctx, 0x08B71DC8u, {first0, first1}) != 0u;
    if (!work) {
        ctx.gpr[2] = first0;
        ctx.gpr[3] = first1;
        return;
    }
    Words q = call_words(rt, ctx, 0x08B71394u, {first0, first1, first0, first1});
    const u32 x0 = q.first;
    const u32 x1 = q.second;
    q = call_words(rt, ctx, 0x08B71394u, {first0, first1, x0, x1});
    put(rt, frame.at(0u), q.first);
    put(rt, frame.at(4u), q.second);
    q = call_words(rt, ctx, 0x08B71394u, {x0, x1, word(rt, 0x08CB68A0u), word(rt, 0x08CB68A4u)});
    q = call_words(rt, ctx, 0x08B712B0u, {q.first, q.second, word(rt, 0x08CB68A8u), word(rt, 0x08CB68ACu)});
    q = call_words(rt, ctx, 0x08B71394u, {x0, x1, q.first, q.second});
    q = call_words(rt, ctx, 0x08B71328u, {q.first, q.second, word(rt, 0x08CB68B0u), word(rt, 0x08CB68B4u)});
    q = call_words(rt, ctx, 0x08B71394u, {x0, x1, q.first, q.second});
    q = call_words(rt, ctx, 0x08B712B0u, {q.first, q.second, word(rt, 0x08CB68B8u), word(rt, 0x08CB68BCu)});
    q = call_words(rt, ctx, 0x08B71394u, {x0, x1, q.first, q.second});
    q = call_words(rt, ctx, 0x08B71328u, {q.first, q.second, word(rt, 0x08CB68C0u), word(rt, 0x08CB68C4u)});
    const u32 y0 = q.first;
    const u32 y1 = q.second;
    if (flag == 0u) {
        const Words m = call_words(rt, ctx, 0x08B71394u, {x0, x1, y0, y1});
        const Words n = call_words(rt, ctx, 0x08B712B0u, {m.first, m.second, word(rt, 0x08CB68C8u), word(rt, 0x08CB68CCu)});
        const Words o = call_words(rt, ctx, 0x08B71394u, {word(rt, frame.at(0u)), word(rt, frame.at(4u)), n.first, n.second});
        const Words p = call_words(rt, ctx, 0x08B71328u, {first0, first1, o.first, o.second});
        ctx.gpr[2] = p.first;
        ctx.gpr[3] = p.second;
        return;
    }
    const Words a = call_words(rt, ctx, 0x08B71394u,
                               {word(rt, frame.at(8u)), word(rt, frame.at(0xCu)), word(rt, 0x08CB68D0u),
                                word(rt, 0x08CB68D4u)});
    u32 s0 = a.first;
    u32 s1 = a.second;
    const Words b = call_words(rt, ctx, 0x08B71394u, {word(rt, frame.at(0u)), word(rt, frame.at(4u)), y0, y1});
    const Words c = call_words(rt, ctx, 0x08B712B0u, {s0, s1, b.first, b.second});
    const Words d = call_words(rt, ctx, 0x08B71394u, {x0, x1, c.first, c.second});
    const Words e = call_words(rt, ctx, 0x08B712B0u,
                               {d.first, d.second, word(rt, frame.at(8u)), word(rt, frame.at(0xCu))});
    s0 = e.first;
    s1 = e.second;
    const Words f = call_words(rt, ctx, 0x08B71394u,
                               {word(rt, frame.at(0u)), word(rt, frame.at(4u)), word(rt, 0x08CB68C8u),
                                word(rt, 0x08CB68CCu)});
    const Words g = call_words(rt, ctx, 0x08B71328u, {s0, s1, f.first, f.second});
    const Words h = call_words(rt, ctx, 0x08B712B0u, {first0, first1, g.first, g.second});
    ctx.gpr[2] = h.first;
    ctx.gpr[3] = h.second;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x088C0DCC (a0 = object, a1/a2/a3 = flags, f12/f13 = angles): classifies by the mode byte at *(a0+0x24)+0xF. Mode 1
// and 2 compare the angles with the float at *(a0+8)+0x6C4; any other mode compares f13 <= f12 and the flags, and the
// long path asks the virtual at *(*(a0+0x1C))+0x18, computes an angle through 0x088DF13C / 0x088BC0C4 / 0x0893C60C
// and tests its magnitude against 0.2778 and 1.0472. v0 = the mode, or 0 / 1 / 2 as the tests decide.
void func_088C0DCC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    const float f12 = ctx.fpr[12];
    const float f13 = ctx.fpr[13];
    SavedS<7> saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 s5 = self + 4u;
    const u32 info = word(rt, self + 0x24u);
    u32 result = rt.memory().load8(info + 0xFu);
    const float f20 = f12;
    const float kSmall = std::bit_cast<float>(0x3E8E38E4u);   //  0.2777778
    const float kSmallNeg = std::bit_cast<float>(0xBE8E38E4u);  // -0.2777778
    const float kPiOver3 = std::bit_cast<float>(0x3F860A92u);  //  1.0471976
    if (result == 1u) {
        if (f12 < f13) {
            const u32 p = word(rt, s5 + 4u);
            result = (std::bit_cast<float>(0x3F555556u) < float_at(rt, p + 0x6C4u)) ? 0u : 2u;
        } else if (arg3 != 0u) {
            result = 2u;
        }
        ctx.gpr[2] = result;
        return;
    }
    if (result != 2u) {
        if (f13 <= f12) {
            ctx.gpr[2] = 2u;
            return;
        }
        if (arg1 != 0u) {
            ctx.gpr[2] = 1u;
            return;
        }
        if (arg2 != 0u) {
            const u32 p = word(rt, s5 + 4u);
            const float limit = float_at(rt, p + 0x6C4u);
            if (!(kSmall <= limit) && !(limit <= kSmallNeg)) {
                ctx.gpr[2] = 1u;
                return;
            }
        }
        if (arg3 == 0u) {
            ctx.gpr[2] = result;
            return;
        }
        ctx.gpr[2] = (f20 <= std::bit_cast<float>(0x3F490FDBu)) ? 2u : result;
        return;
    }
    bool enter = false;
    if (std::bit_cast<float>(0x3F32B8C2u) <= f12) {
        enter = true;
    } else {
        const u32 p = word(rt, s5 + 4u);
        if (float_at(rt, p + 0x6C4u) <= std::bit_cast<float>(0x41055556u)) enter = true;
        else result = 0u;
    }
    if (!enter) {
        ctx.gpr[2] = result;
        return;
    }
    if (arg2 == 0u) {
        ctx.gpr[2] = result;
        return;
    }
    const u32 owner = word(rt, self + 0x1Cu);
    const u32 called = call_virtual(rt, ctx, owner, 0x18u, {owner, rt.memory().load8(info + 0x10u)}).v0;
    u32 model = word(rt, self + 8u);
    if (called != 0u) {
        const u32 first = word(rt, s5 + 8u);
        const u32 second = word(rt, s5 + 0xCu);
        const u32 holder = word(rt, self + 0x1Cu);
        const u32 byte1 = rt.memory().load8(info + 0x11u);
        const u32 byte0 = rt.memory().load8(info + 0x10u);
        const u32 values = call(rt, ctx, 0x088DF13Cu, {first, second});
        const float angle = call_float(rt, ctx, 0x088BC0C4u, {holder, byte0, byte1, 0u}, {float_at(rt, values)});
        model = word(rt, s5 + 4u);
        put_float(rt, frame.at(4u), angle);
        put_float(rt, frame.at(0u), float_at(rt, model + 0x704u));
        const float measured = call_float(rt, ctx, 0x0893C60Cu, {frame.at(4u), frame.at(0u)});
        const float magnitude = (measured < 0.0f) ? -measured : measured;
        put_float(rt, frame.at(4u), magnitude);
        if (kPiOver3 < magnitude) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
    const float limit = float_at(rt, model + 0x6C4u);
    if (!(kSmall <= limit) && !(limit <= kSmallNeg)) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (arg1 != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = (kPiOver3 < f20) ? 1u : result;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08902C00 and 0x08905000 are one body (0x08902C00 here; a0 = a float pair out, a1/a2/a3/t0 = three 0x28-byte
// geometry structs): 0x08905594 picks the edge pair; the distance at +0x24 of the two structs folds into +0x48 of a
// local record; then the gap is measured as two cross products, a scale and a length (0x08B4AA68, 0x08B4A5E8,
// 0x08B4AA34, 0x08B4A720). out[0] = sqrt(length) * (record[3] - 1 on the edge-pair path, record[3] otherwise);
// out[1] = the folded distance, or -1 when the wrap's check fails. v0 = the output pointer.
void measure_gap(Runtime &rt, AllegrexContext &ctx, u32 out, u32 arg_a1, u32 arg_a2, u32 arg_a3, u32 arg_t0) {
    SavedS<8> saved(ctx);
    SavedFp saved_fp(ctx);
    GuestFrame frame(ctx, 0xB0u);
    const u32 record = frame.at(0x48u);
    const u32 other_vec = frame.at(0x54u);
    const u32 measure_vec = frame.at(0x60u);
    u32 selector = call(rt, ctx, 0x08905594u, {record, arg_a2, arg_a3, arg_t0, arg_a3 + 0x0Cu, arg_t0 + 0x0Cu});
    if (selector == 0u) {
        if (call(rt, ctx, 0x08905594u,
                 {record, arg_a2, arg_a3 + 0x0Cu, arg_t0 + 0x0Cu, arg_a3 + 0x18u, arg_t0 + 0x18u}) == 0u) {
            put_float(rt, out, 0.0f);
            put_float(rt, out + 4u, -1.0f);
            ctx.gpr[2] = out;
            return;
        }
    }
    float wrapped = -1.0f;
    const float reach = float_at(rt, record);
    if (std::fabs(reach) < std::bit_cast<float>(0x7F7FFFFFu)) {
        const float limit = float_at(rt, arg_a3 + 0x24u);
        const float scale = float_at(rt, arg_a1 + 0x14u);
        float value = float_at(rt, arg_t0 + 0x24u);
        if (value < limit) value += scale;
        value -= limit;
        wrapped = limit + value * reach;
        if (scale < wrapped) wrapped -= scale;
    }
    const auto measure = [&](u32 first, u32 second, u32 third) {
        copy_bytes(rt, second, first, 0x0Cu);
        call_keep(rt, ctx, 0x08B4A5E8u, {second}, {reach});
        copy_bytes(rt, third, other_vec, 0x0Cu);
        call(rt, ctx, 0x08B4AA34u, {third, third, second});
        copy_bytes(rt, measure_vec, third, 0x0Cu);
        return call_float(rt, ctx, 0x08B4A720u, {measure_vec});
    };
    float result;
    if (selector != 0u) {
        const u32 first = frame.at(0x24u);
        put_float(rt, frame.at(0x2Cu), float_at(rt, arg_t0 + 0x14u) - float_at(rt, arg_t0 + 0x08u));
        put_float(rt, frame.at(0x5Cu), float_at(rt, arg_a3 + 0x14u) - float_at(rt, arg_a3 + 0x08u));
        put_float(rt, other_vec, float_at(rt, arg_a3 + 0x0Cu) - float_at(rt, arg_a3 + 0x00u));
        put_float(rt, frame.at(0x24u), float_at(rt, arg_t0 + 0x0Cu) - float_at(rt, arg_t0 + 0x00u));
        put(rt, frame.at(0x58u), 0u);
        put(rt, frame.at(0x28u), 0u);
        call(rt, ctx, 0x08B4AA68u, {first, first, other_vec});
        result = std::sqrt(measure(frame.at(0x24u), frame.at(0x30u), frame.at(0x3Cu))) *
                 (float_at(rt, frame.at(0x4Cu)) - 1.0f);
    } else {
        const u32 first = frame.at(0x00u);
        const float from = float_at(rt, arg_a3 + 0x0Cu);
        put_float(rt, frame.at(0x5Cu), float_at(rt, arg_a3 + 0x20u) - float_at(rt, arg_a3 + 0x14u));
        put_float(rt, frame.at(0x08u), float_at(rt, arg_t0 + 0x20u) - float_at(rt, arg_t0 + 0x14u));
        put_float(rt, other_vec, float_at(rt, arg_a3 + 0x18u) - from);
        put_float(rt, frame.at(0x00u), float_at(rt, arg_t0 + 0x18u) - float_at(rt, arg_t0 + 0x0Cu));
        put(rt, frame.at(0x58u), 0u);
        put(rt, frame.at(0x04u), 0u);
        call(rt, ctx, 0x08B4AA68u, {first, first, other_vec});
        result = std::sqrt(measure(frame.at(0x00u), frame.at(0x0Cu), frame.at(0x18u))) *
                 float_at(rt, frame.at(0x4Cu));
    }
    put_float(rt, out, result);
    put_float(rt, out + 4u, wrapped);
    ctx.gpr[2] = out;
}
void func_08902C00(Runtime &rt, AllegrexContext &ctx) {
    measure_gap(rt, ctx, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]);
}

} // namespace
} // namespace new_batch_main_696
} // namespace gt::game

#define NEW696(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_696::name, gt::ReturnKind::kind)
NEW696(0x08B03448u, func_08B03448, Word);
NEW696(0x08AEF1D0u, func_08AEF1D0, Word);
NEW696(0x08B62A04u, func_08B62A04, Word);
NEW696(0x08BE1DA4u, func_08BE1DA4, Word);
NEW696(0x08B6D284u, func_08B6D284, DoubleWord);
NEW696(0x088C0DCCu, func_088C0DCC, Word);
NEW696(0x08902C00u, func_08902C00, Word);
