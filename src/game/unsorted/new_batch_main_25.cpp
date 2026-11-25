// Main-session batch 25: functions the extra seeds made emittable, written by hand as structured C from `tools.disasm`.
// Draft only (no gate run here): each body keeps the original's v0 on every exit path, every argument register a callee reads,
// the delay slot of a `jal` before the call, and the frame size of the original's prologue.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_25 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same, keeping v0 / v1 / f0 as the callee left them (a caller that reads v1 after the call).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A guest call with the argument registers written like the original writes them before the jal (a0-a3, t0-t3).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, target, arguments);
}
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments, floats);
}

// `lw v0, off(vtable); jalr v0`: v1 = the table, v0 = the slot's function before the call.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments);
}

u32 lb(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address)))); }
u32 lh(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address)))); }
s32 signed_of(u32 value) { return static_cast<s32>(value); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The callee-saved registers a body keeps state in: the original restores them before it returns or tail-calls.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[8];
    u32 saved_fp;
    explicit SavedRegs(AllegrexContext &c) : ctx(c), saved_fp(c.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved_fp;
    }
};

// 0x08B7E564: a strtol-shaped parser (a0 = where a bad input is reported, a1 = text, a2 = end-pointer out, a3 = base).
// Skips the table's whitespace class at 0x08C96BD9, takes one sign, resolves base 0 / 0x, then accumulates into a 64-bit
// result while it fits the 2^64-1 / base limit the two divide helpers return (quotient in v1:v0, remainder in v0), and
// returns the result in v1:v0 (negated for a leading '-'); a text with no digit returns -1 and stores '"' at *a0.

// MIPS `ins`: the `size` bits at `pos` of `base` replaced by the low bits of `value`.
u32 ins(u32 base, u32 value, unsigned pos, unsigned size) {
    const u32 mask = ((size >= 32u) ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (base & ~mask) | ((value << pos) & mask);
}

// 0x08B066B8: fills `self` (a0) from `src` (a2): a 12-byte header copy, a copy through 08BAE264 into self+0xC, the setup call
// 08B0603C(self, src, a3, f12, f13) whose float result lands at +0x64, and a packed word at +0x6C built from t0/t1. When the
// float at src+0x28 is not zero it also fills two sub-objects at self+0x70 and self+0xE0 from the triples 08B0562C writes into
// the frame, each with a copy at +0xC, two 08BBBEC4 heights summed at +0x64 and a packed word at +0x6C; v0 = 3 then, else 1.
void func_08B066B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 src = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    const u32 extra0 = ctx.gpr[8];
    const u32 extra1 = ctx.gpr[9];
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x90u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;           // s7
    ctx.gpr[30] = src;            // fp
    ctx.gpr[17] = arg3;           // s6
    ctx.gpr[18] = src + 0xCu;     // s5

    put(rt, self + 0x00u, word(rt, src + 0x00u));
    put(rt, self + 0x04u, word(rt, src + 0x04u));
    put(rt, self + 0x08u, word(rt, src + 0x08u));
    const GuestResult header = call_keep(rt, ctx, 0x08BAE264u, {src + 0xCu, src});
    call_set(rt, ctx, 0x08BAE2B0u, {self + 0x0Cu, header.v0, header.v1});

    put(rt, self + 0x68u, 0u);
    put_float(rt, self + 0x60u, float_at(rt, src + 0x34u));
    const GuestResult setup = call_setf(rt, ctx, 0x08B0603Cu, {self, src, arg3}, {first, second});
    put_float(rt, self + 0x64u, setup.f0);
    u32 packed = word(rt, self + 0x6Cu);
    packed = ins(packed, 0xFFFu, 0u, 12u);
    packed = ins(packed, extra0, 12u, 10u);
    packed = ins(packed, 0x3FFu, 22u, 10u);
    put(rt, self + 0x6Cu, packed);
    call_set(rt, ctx, 0x08B062BCu, {self, arg3, arg1});

    const float height = float_at(rt, src + 0x28u);
    if (height == 0.0f) {
        ctx.gpr[2] = 1u;
        return;
    }

    call_set(rt, ctx, 0x08B0562Cu, {src, frame.at(0x18u), frame.at(0x24u)});

    const auto block = [&](u32 base, u32 triple, float top) {
        ctx.gpr[19] = base;  // s0 / s1
        put(rt, base + 0x00u, word(rt, triple + 0x00u));
        put(rt, base + 0x04u, word(rt, triple + 0x04u));
        put(rt, base + 0x08u, word(rt, triple + 0x08u));
        const GuestResult copy = call_keep(rt, ctx, 0x08BAE264u, {src + 0xCu, triple});
        call_set(rt, ctx, 0x08BAE2B0u, {base + 0x0Cu, copy.v0, copy.v1});
        put(rt, base + 0x68u, std::bit_cast<u32>(height));
        put_float(rt, base + 0x60u, top);
        const float one = call_setf(rt, ctx, 0x08BBBEC4u, {base, arg3 + 0x38u}, {}).f0;
        const float two = call_setf(rt, ctx, 0x08BBBEC4u, {base + 0x0Cu, arg3 + 0x50u}, {}).f0;
        put_float(rt, base + 0x64u, one + two);
        u32 word_out = word(rt, base + 0x6Cu);
        word_out = ins(word_out, extra1, 0u, 12u);
        word_out = ins(word_out, extra0, 12u, 10u);
        word_out = ins(word_out, 0x3FFu, 22u, 10u);
        put(rt, base + 0x6Cu, word_out);
        call_set(rt, ctx, 0x08B062BCu, {base, arg3, arg1});
    };
    block(self + 0x70u, frame.at(0x18u), float_at(rt, src + 0x38u));
    block(self + 0xE0u, frame.at(0x24u), float_at(rt, src + 0x3Cu));
    ctx.gpr[2] = 3u;
}

// 0x0889D338 (a0 = object, a1 = delta, a2 = threshold): relocates the object's pointer fields — a non-zero field below the
// threshold is left alone, any other gets the delta added. +0x50 leads to a chain (its +0xEC, then that node's +8, +0x10,
// +0x18, +0x20), +0x9C and +0xA0 to counted arrays whose entries (and each entry's +4) are relocated the same way.
void func_0889D338(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 delta = ctx.gpr[5];
    const u32 threshold = ctx.gpr[6];

    const auto relocate = [&](u32 base, u32 offset) {
        const u32 value = word(rt, base + offset);
        if (value != 0u && !(value < threshold)) put(rt, base + offset, delta + value);
    };
    for (const u32 offset : {0x1Cu, 0x24u, 0x2Cu, 0x34u, 0x3Cu, 0x44u, 0x4Cu}) relocate(object, offset);

    u32 node = word(rt, object + 0x50u);
    if (node != 0u) {
        if (!(node < threshold)) {
            put(rt, object + 0x50u, delta + node);
            node = delta + node;
        }
        u32 inner = word(rt, node + 0xECu);
        if (inner != 0u) {
            if (!(inner < threshold)) {
                put(rt, node + 0xECu, delta + inner);
                node = word(rt, object + 0x50u);
            }
            u32 chain = word(rt, node + 0xECu);
            for (const u32 offset : {0x08u, 0x10u, 0x18u}) {
                const u32 value = word(rt, chain + offset);
                if (!(value < threshold)) {
                    put(rt, chain + offset, delta + value);
                    node = word(rt, object + 0x50u);
                    chain = word(rt, node + 0xECu);
                }
            }
            const u32 last = word(rt, chain + 0x20u);
            if (!(last < threshold)) put(rt, chain + 0x20u, delta + last);
        }
    }

    relocate(object, 0x54u);
    relocate(object, 0x60u);

    const u32 table = word(rt, object + 0x9Cu);
    if (table != 0u && !(table < threshold)) {
        put(rt, object + 0x9Cu, delta + table);
        const s32 count = signed_of(word(rt, object + 0x98u));
        for (s32 i = 0; i < count; ++i) {
            const u32 slot = word(rt, object + 0x9Cu) + static_cast<u32>(i) * 4u;
            const u32 value = word(rt, slot);
            if (!(value < threshold)) put(rt, slot, delta + value);
            const u32 entry = word(rt, word(rt, object + 0x9Cu) + static_cast<u32>(i) * 4u);
            const u32 field = word(rt, entry + 4u);
            if (!(field < threshold)) put(rt, entry + 4u, delta + field);
        }
    }

    const u32 list = word(rt, object + 0xA0u);
    if (list != 0u && !(list < threshold)) {
        put(rt, object + 0xA0u, delta + list);
        u32 cursor = word(rt, object + 0xA0u);
        const u32 head = word(rt, cursor + 4u);
        if (!(head < threshold)) {
            put(rt, cursor + 4u, delta + head);
            const s32 count = signed_of(word(rt, cursor));
            for (s32 i = 0; i < count; ++i) {
                const u32 slot = word(rt, cursor + 4u) + static_cast<u32>(i) * 4u;
                const u32 value = word(rt, slot);
                if (!(value < threshold)) put(rt, slot, delta + value);
                const u32 entry = word(rt, word(rt, cursor + 4u) + static_cast<u32>(i) * 4u);
                const u32 field = word(rt, entry + 4u);
                if (!(field < threshold)) put(rt, entry + 4u, delta + field);
                cursor = word(rt, object + 0xA0u);
            }
        }
    }
}

// 0x08882558: applies the dirty bits at a0+0x16E0, copying the staged floats into the live block for every bit set (bit 0 the
// three and four-float blocks at 0xC18 / 0xC3C, 0x40 the 0x1B8 value, 2 the 0xC08 block, 4 and 8 the four-step arrays, 0x10
// and 0x20 the per-step scalars with the 100000.0 fallback for a zero byte, 0x80..0x1000 single values, 0x2000 a 4-bit
// field, 0x4000 the four floats at 0xCD0). The 0x1B8 value is scaled by 0x16E8 on every call and the 0x590 slots by
// 0x16E4 * pi/180 when that is not zero; v0 = the 0x4000 bit at the end.
void func_08882558(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 flags_address = object + 0x16E0u;
    const u32 flags = word(rt, flags_address);

    if ((flags & 1u) != 0u) {
        for (u32 i = 0u; i < 3u; ++i) put(rt, object + 0x140u + i * 4u, word(rt, object + 0xC18u + i * 4u));
        for (u32 i = 0u; i < 4u; ++i) put(rt, object + 0x164u + i * 4u, word(rt, object + 0xC3Cu + i * 4u));
    }
    if ((flags & 0x40u) != 0u) put(rt, object + 0x1B8u, word(rt, object + 0xC90u));
    put_float(rt, object + 0x1B8u, psp_mul(float_at(rt, object + 0x1B8u), float_at(rt, object + 0x16E8u)));
    if ((flags & 2u) != 0u) {
        for (u32 i = 0u; i < 4u; ++i) put(rt, object + 0x130u + i * 4u, word(rt, object + 0xC08u + i * 4u));
    }
    if ((flags & 4u) != 0u) {
        for (u32 i = 0u; i < 4u; ++i) {
            const u32 source = object + 0x1060u + i * 0x24u;
            const u32 dest = source - 0xAC4u;
            put(rt, dest + 0x0u, word(rt, source + 0x14u));
            put(rt, dest + 0x4u, word(rt, source + 0x18u));
            put(rt, dest + 0x8u, word(rt, source + 0x1Cu));
        }
    }
    if ((flags & 8u) != 0u) {
        for (u32 i = 0u; i < 4u; ++i) {
            put(rt, object + 0x588u + i * 0x24u, word(rt, object + 0x1060u + i * 0x24u));
            put(rt, object + 0x590u + i * 0x24u, word(rt, object + 0x1068u + i * 0x24u));
            put(rt, object + 0x58Cu + i * 0x24u, word(rt, object + 0x1064u + i * 0x24u));
        }
    }
    const float degrees = float_at(rt, object + 0x16E4u);
    if (degrees != 0.0f) {
        const float radians = psp_mul(degrees, std::bit_cast<float>(0x3C8EFA35u));
        for (u32 i = 0u; i < 2u; ++i) put_float(rt, object + 0x590u + i * 0x24u, radians);
    }
    if ((flags & 0x10u) != 0u) {
        for (u32 i = 0u; i < 4u; ++i) {
            put(rt, object + 0x594u + i * 0x24u, word(rt, object + 0x106Cu + i * 0x24u));
            put(rt, object + 0x2D0u + i * 0xB8u, word(rt, object + 0x2D0u + i * 0xB8u + 0xAD8u));
        }
    }
    if ((flags & 0x20u) != 0u) {
        for (u32 i = 0u; i < 4u; ++i) {
            const u32 byte = rt.memory().load8(object + 0xDC4u + i * 0xB8u);
            put8(rt, object + 0x2ECu + i * 0xB8u, byte);
            if (byte == 0u) put_float(rt, object + 0x594u + i * 0x24u, std::bit_cast<float>(0x47C35000u));
        }
    }
    if ((flags & 0x80u) != 0u) {
        put(rt, object + 0x194u, word(rt, object + 0xC6Cu));
        put(rt, object + 0x198u, word(rt, object + 0xC70u));
    }
    if ((flags & 0x100u) != 0u) put(rt, object + 0x1A4u, word(rt, object + 0xC7Cu));
    if ((flags & 0x200u) != 0u) put(rt, object + 0x1A8u, word(rt, object + 0xC80u));
    if ((flags & 0x400u) != 0u) put(rt, object + 0x1B0u, word(rt, object + 0xC88u));
    if ((flags & 0x800u) != 0u) put(rt, object + 0x1B4u, word(rt, object + 0xC8Cu));
    if ((flags & 0x1000u) != 0u) {
        const u32 field = word(rt, object + 0x16C0u) >> 10;
        put(rt, object + 0xBE8u, ins(word(rt, object + 0xBE8u), field, 10u, 4u));
    }
    if ((flags & 0x2000u) != 0u) {
        put(rt, object + 0x188u, word(rt, object + 0xC60u));
        put(rt, object + 0x190u, word(rt, object + 0xC68u));
    }
    if ((flags & 0x4000u) != 0u) {
        put(rt, object + 0x1F8u, word(rt, object + 0xCD0u));
        put(rt, object + 0x1FCu, word(rt, object + 0xCD4u));
        put(rt, object + 0x200u, word(rt, object + 0xCD8u));
        put(rt, object + 0x204u, word(rt, object + 0xCDCu));
    }
    ctx.gpr[2] = flags & 0x4000u;
}

// 0x088CD2FC (a0 = object, a1 = index): builds the normal/tangent frame of element `index` (stride 40 at +0x30) from its own
// floats and the next element's (index+1, or index-35 when that reaches 0x24), normalises both, and writes three vectors at
// +0x640, +0x64C and +0x658 of the 36-byte slot. The 0x46 element takes the other operand order. v0 = object + index * 40.

// 0x088C0DCC (a0 = object, a1/a2/a3 = flags, f12/f13 = angles): classifies by the mode byte at *(a0+0x24)+0xF. Mode 1 and 2
// compare the angles and the float at *(*(a0+8)+0x6C4); any other mode compares f13 <= f12 and the flags, and the long path asks
// the virtual at *(*(a0+0x1C))+0x18, computes an angle through 088BC0C4/0893C60C and tests it against +-0.2778 and 1.0472.
// v0 = the mode, or 0 / 1 / 2 as the tests decide.
void func_088C0DCC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    const float f12 = ctx.fpr[12];
    const float f13 = ctx.fpr[13];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
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
    // Mode 2.
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
        const float angle = call_setf(rt, ctx, 0x088BC0C4u, {holder, byte0, byte1, 0u}, {float_at(rt, values)}).f0;
        model = word(rt, s5 + 4u);
        put_float(rt, frame.at(4u), angle);
        put_float(rt, frame.at(0u), float_at(rt, model + 0x704u));
        const float measured = call_setf(rt, ctx, 0x0893C60Cu, {frame.at(4u), frame.at(0u)}, {}).f0;
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

// 0x08C481C8 (a0 = first record, a1 = end, a2 = destination): copies the 0xF0-byte records from a0 up to (not including) a1
// into a2, stepping both by 0xF0; a null destination still walks the records. v0 = the destination after the last record.

// 0x088DF790 (a0 = object): resets the object's state — clears the byte and word flags, calls 088D5DF8(self+4) and stores its
// result at +0x59C, fills the 4-entry table at +0x77C (+0x20 each) and +0x780 (+0 each) with -1, calls 088DC1B8(self, 3), then
// per entry of 088D9F80(self): 088E8184 and 088E8CC0 on *(self+0xC0)+0x2F8*i, clears +0x2D0/+0x2D4 and stamps the index. Finally
// 088F89AC(*(self+0x14C)) when set, 088D9BBC(self, 0, 0), and a memset of *(self+0x644) with 0x340 zeros as a tail call.

// The 8-bit alpha the two sprite functions pack: 255 unless `raw` is below `limit`, where the scaled value (raw/8 or raw/limit)
// is taken when it is above 1; then the saturating trunc.w.s to a 32-bit integer, packed into bits 24..31 of a 0xFFFFFFFF word
// when it fits a byte.
u32 alpha_byte(float raw, float limit, bool eighth) {
    float value = std::bit_cast<float>(0x437F0000u);  // 255.0
    if (raw < limit) {
        const float scaled = eighth ? psp_mul(raw, std::bit_cast<float>(0x3E000000u)) : raw / limit;
        if (!(scaled < 0.0f) && std::bit_cast<float>(0x3F800000u) < scaled) value = psp_mul(scaled, std::bit_cast<float>(0x437F0000u));
    }
    u32 truncated;
    if (std::bit_cast<float>(0x4F000000u) <= value) {
        truncated = static_cast<u32>(static_cast<s32>(value - std::bit_cast<float>(0x4F000000u))) | 0x80000000u;
    } else {
        truncated = static_cast<u32>(static_cast<s32>(value));
    }
    return (truncated < 0x100u) ? ((truncated << 24) | 0x00FFFFFFu) : 0xFFFFFFFFu;
}

// The four 0x18-byte vertices both sprite functions write at `out`: the w component alternates 1.0/0.0 (or the other way for
// `second`), the packed colour sits at +8 of each, and the three floats come from the frame's 0x20 / 0x30 / 0x50 / 0x40 blocks.
void sprite_vertices(Runtime &rt, u32 out, bool second, u32 colour, u32 frame_base) {
    const u32 one = 0x3F800000u;
    put(rt, out + 0x08u, colour);
    if (!second) {
        put(rt, out + 0x00u, one);
        put(rt, out + 0x04u, one);
        put(rt, out + 0x18u, 0u);
        put(rt, out + 0x1Cu, one);
        put(rt, out + 0x30u, one);
        put(rt, out + 0x34u, 0u);
        put(rt, out + 0x48u, 0u);
    } else {
        put(rt, out + 0x00u, 0u);
        put(rt, out + 0x04u, one);
        put(rt, out + 0x18u, one);
        put(rt, out + 0x1Cu, one);
        put(rt, out + 0x30u, 0u);
        put(rt, out + 0x34u, 0u);
        put(rt, out + 0x48u, one);
    }
    put(rt, out + 0x0Cu, word(rt, frame_base + 0x20u));
    put(rt, out + 0x10u, word(rt, frame_base + 0x24u));
    put(rt, out + 0x14u, word(rt, frame_base + 0x28u));
    put(rt, out + 0x24u, word(rt, frame_base + 0x30u));
    put(rt, out + 0x28u, word(rt, frame_base + 0x34u));
    put(rt, out + 0x2Cu, word(rt, frame_base + 0x38u));
    put(rt, out + 0x3Cu, word(rt, frame_base + 0x50u));
    put(rt, out + 0x40u, word(rt, frame_base + 0x54u));
    put(rt, out + 0x44u, word(rt, frame_base + 0x58u));
    put(rt, out + 0x4Cu, 0u);
    put(rt, out + 0x54u, word(rt, frame_base + 0x40u));
    put(rt, out + 0x58u, word(rt, frame_base + 0x44u));
    put(rt, out + 0x5Cu, word(rt, frame_base + 0x48u));
}

// 0x088C7948 (a0 ignored, a1 = four vertices, a2 = flag): asks 08B5A668 for the two projection blocks in the frame, packs the
// alpha of the frame's 0x18 value (raw/8, 255 when it does not fit), takes 0x60 bytes from 08B550E0, spins, sets each vertex's
// w and calls 08B5A7F0 per vertex, fills the four 0x18-byte vertices (the second variant when the low byte of a2 is set), and
// submits them through 08B569A0 / 08B56510 / 08B555F8 / 08B556A0 / 08B55588 / 08B56940.

// 0x088C9918 (a0 ignored, a1 = four vertices, a2 = flag): the same sprite submission as 0x088C7948, with the alpha of the
// frame's 0x18 value scaled by 1/12 and the second vertex variant chosen when a2 is zero.

} // namespace
} // namespace new_batch_main_25
} // namespace gt::game

#define MAIN25(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_25::name, gt::ReturnKind::kind)
MAIN25(0x08B066B8u, func_08B066B8, Word);
MAIN25(0x0889D338u, func_0889D338, Void);
MAIN25(0x08882558u, func_08882558, Word);
MAIN25(0x088C0DCCu, func_088C0DCC, Word);
