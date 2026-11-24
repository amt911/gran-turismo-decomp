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

// ---------------------------------------------------------------------------------------------------------------
// 0x08AEF1D0 (a0 = selector, a1 = source values): fixed-point (12-bit fraction) geometry setup.
// s3 = a0 (mode at +0x18, index at +0x1C), s1 = a1. The two 0x08AF0E9C lookups scale a1's +4 / +8; a table entry at
// 0x08CE09D0 picked by 2*[a1+0x24] + [a1+0x1C] scales them again; 0x08AD9860 picks between the two (when it answers 0
// they are averaged); then the mode selects which block of state (0x08D41AD0 rows and the pointer at +0x4B0) gets the
// results.
s32 mul32(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); }
s32 fixed12(s32 a, s32 b) { return mul32(a, b) >> 12; }

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

// ---------------------------------------------------------------------------------------------------------------
// 0x08B6D284 (a0:a1 = a double, a2:a3 = another, t0 = a flag): a chain of 64-bit operations. A magnitude at or below
// 0x3E3FFFFF asks 0x08B71DC8 first; a non-zero answer (and the > case always) runs the 0x08B71394 / 0x08B712B0 /
// 0x08B71328 chain with the 64-bit constants at 0x08CB68A0..0x08CB68D4; t0 == 0 takes a shorter chain. v0:v1 is the
// 64-bit result (the inputs themselves on the early exit).

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

} // namespace
} // namespace new_batch_main_696
} // namespace gt::game

#define NEW696(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_696::name, gt::ReturnKind::kind)
NEW696(0x088C0DCCu, func_088C0DCC, Word);
