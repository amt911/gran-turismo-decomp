// New batch 24 (main session): functions the extra seeds made emittable, written as structured C from `tools.disasm`.
// Draft only: the differential gate decides which of them land. Every body keeps the original's exits (`v0` on each path),
// the call order, and the memory it writes; arguments go in a0-a3 / t0-t3, float arguments in f12-f15.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_24 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;
using u64 = std::uint64_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call with float arguments ($f12-$f15), result in `v0` and $f0.
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The two-word (v0, v1) result of a function returning a small struct.
struct Words {
    u32 first;
    u32 second;
};
Words call_words(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return Words{result.v0, result.v1};
}
// A call whose result is a float in $f0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// Callee-saved registers a body keeps its arguments in and puts back.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};
struct SavedS1 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS1(AllegrexContext &c) : ctx(c), value(c.gpr[17]) {}
    ~SavedS1() { ctx.gpr[17] = value; }
};
struct SavedS2 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS2(AllegrexContext &c) : ctx(c), value(c.gpr[18]) {}
    ~SavedS2() { ctx.gpr[18] = value; }
};
struct SavedS3 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS3(AllegrexContext &c) : ctx(c), value(c.gpr[19]) {}
    ~SavedS3() { ctx.gpr[19] = value; }
};
struct SavedS4 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS4(AllegrexContext &c) : ctx(c), value(c.gpr[20]) {}
    ~SavedS4() { ctx.gpr[20] = value; }
};
struct SavedS5 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS5(AllegrexContext &c) : ctx(c), value(c.gpr[21]) {}
    ~SavedS5() { ctx.gpr[21] = value; }
};
struct SavedS6 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS6(AllegrexContext &c) : ctx(c), value(c.gpr[22]) {}
    ~SavedS6() { ctx.gpr[22] = value; }
};
struct SavedS7 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS7(AllegrexContext &c) : ctx(c), value(c.gpr[23]) {}
    ~SavedS7() { ctx.gpr[23] = value; }
};
struct SavedFp {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedFp(AllegrexContext &c) : ctx(c), value(c.gpr[30]) {}
    ~SavedFp() { ctx.gpr[30] = value; }
};

s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }

// The PSP's `ll`/`sc` refcount decrement: one word, minus one, always stored here (a real `sc` retries until it stores).
u32 refcount_decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
// The matching increment (`ll`/`sc` +1) used by the constructors.
void refcount_increment(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }

// A `lw v0, slot(vtable); jalr v0` virtual call on a refcounted object (v0/v1/f0 are the callee's afterwards).
void virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 target = word(rt, word(rt, object) + slot);
    call_indirect(rt, ctx, target, arguments);
}

// Release of one intrusive reference (PSP shared_ptr): -1 at +4; on the last one the object's virtual destructor at +8 runs,
// then -1 at +8; on that last one the deallocator at +0xC runs. The vtable word is re-read before each virtual call, as the
// original's `lw v1, 0(s0)` does (the destructor may replace it).
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (refcount_decrement(rt, object + 4u) != 1u) return;
    virtual_call(rt, ctx, object, 8u, {object});
    if (refcount_decrement(rt, object + 8u) != 1u) return;
    virtual_call(rt, ctx, object, 0xCu, {object});
}
// The same release for an object whose +4 count is not touched: only +8, and the deallocator at +0xC.
void release_object_second_count(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (refcount_decrement(rt, object + 8u) != 1u) return;
    virtual_call(rt, ctx, object, 0xCu, {object});
}

// The class destructor of 0x08BC12AC: five refcounted members (0xA0, 0x98, 0x8C, 0x84, 0x2C) released in that order, the
// vtable moving to the base's between the fourth and the fifth, and a sixth member (0x24) released on its +8 count only.
// `v0` ends as the base vtable.
void destroy_members(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 base_vtable, u32 last_vtable) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, vtable);
    release_object(rt, ctx, word(rt, self + 0xA0u));
    release_object(rt, ctx, word(rt, self + 0x98u));
    release_object(rt, ctx, word(rt, self + 0x8Cu));
    release_object(rt, ctx, word(rt, self + 0x84u));
    put(rt, self, base_vtable);
    release_object(rt, ctx, word(rt, self + 0x2Cu));
    release_object_second_count(rt, ctx, word(rt, self + 0x24u));
    put(rt, self, last_vtable);
    ctx.gpr[2] = last_vtable;
}
// The same destructor shape whose body ends with a tail call of 0x08B64654(this) after the frame is popped.
void destroy_members_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 base_vtable, u32 last_vtable, u32 tail) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, vtable);
        release_object(rt, ctx, word(rt, self + 0xA0u));
        release_object(rt, ctx, word(rt, self + 0x98u));
        release_object(rt, ctx, word(rt, self + 0x8Cu));
        release_object(rt, ctx, word(rt, self + 0x84u));
        put(rt, self, base_vtable);
        release_object(rt, ctx, word(rt, self + 0x2Cu));
        release_object_second_count(rt, ctx, word(rt, self + 0x24u));
        put(rt, self, last_vtable);
        ctx.gpr[2] = last_vtable;
    }
    tail_call(ctx, tail, {self});
}

void func_08BC12AC(Runtime &rt, AllegrexContext &ctx) { destroy_members(rt, ctx, 0x08CC5200u, 0x08CC5220u, 0x08CC3000u); }
void func_08BC1538(Runtime &rt, AllegrexContext &ctx) {
    destroy_members_then_tail(rt, ctx, 0x08CC5200u, 0x08CC5220u, 0x08CC3000u, 0x08B64654u);
}
void func_08BD045C(Runtime &rt, AllegrexContext &ctx) { destroy_members(rt, ctx, 0x08CC53A0u, 0x08CC53C0u, 0x08CC3000u); }
void func_08BD06E8(Runtime &rt, AllegrexContext &ctx) {
    destroy_members_then_tail(rt, ctx, 0x08CC53A0u, 0x08CC53C0u, 0x08CC3000u, 0x08B64654u);
}

// ---- 0x08B4A198: atanh (rational approximation), float argument in $f12 and result in $f0 ----
// |x| <= 1 gives num/den at x. Outside it the code divides by -x, evaluates the same ratio at -1/x and takes pi/2 off
// (x < -1) or adds it (x > 1). A NaN takes the x > 1 path. The constants are the asm's `lui`/`ori` pairs bit for bit.
float atanh_ratio(float x) {
    const float one = 1.0f;
    const float x2 = x * x;
    float x3 = x * x2;
    float term_den = x2 * std::bit_cast<float>(0x3FD8A1AAu);
    float x4 = x * x3;
    x3 = x3 * std::bit_cast<float>(0x3FADF6FFu);
    float x5 = x * x4;
    x4 = x4 * std::bit_cast<float>(0x3F5A0E95u);
    float numerator = x + x3;
    float x6 = x * x5;
    term_den = term_den + x4;
    x5 = x5 * std::bit_cast<float>(0x3EF9ABFEu);
    float x7 = x * x6;
    x6 = x6 * std::bit_cast<float>(0x3E01D871u);
    float x8 = x * x7;
    numerator = numerator + x5;
    x7 = x7 * std::bit_cast<float>(0x3D1DB830u);
    term_den = term_den + x6;
    x8 = x8 * std::bit_cast<float>(0x3B29A972u);
    numerator = numerator + x7;
    x8 = x8 + one;
    term_den = term_den + x8;
    return numerator / term_den;
}
void func_08B4A198(Runtime &, AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float pi_over_two = std::bit_cast<float>(0x3FC90FDBu);
    float result;
    if (x <= 1.0f) {
        if (-1.0f <= x) {
            result = atanh_ratio(x);
        } else {
            result = atanh_ratio(-1.0f / x) - pi_over_two;
        }
    } else {
        result = atanh_ratio(-1.0f / x) + pi_over_two;
    }
    ctx.fpr[0] = result;
}

// ---- 0x089F1850: one-time registration (a0 == 1 && a1 == 0xFFFF), ten table rows ----
// Two registry calls, then ten 0x08C0B44C(descriptor, name, {word0, word1}, this) in order; v0 ends as the last
// result. The early exits keep the asm's v0: 1 for a0 != 1, 0xFFFF for a1 != 0xFFFF.
void func_089F1850(Runtime &rt, AllegrexContext &ctx) {
    if (ctx.gpr[4] != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (ctx.gpr[5] != 0xFFFFu) {
        ctx.gpr[2] = 0xFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0x60u);
    call(rt, ctx, 0x08A92114u, {0x08CF58D8u, 0x08CA808Cu, 0x280u, 0x089F16F4u, 0x089F4500u, 0x089F1718u});
    call(rt, ctx, 0x0897D534u, {0x08CF58C8u, 0x08CA808Cu, 0x089F46C4u, 0x089F4680u});
    struct Row {
        u32 pair;
        u32 descriptor;
        u32 name;
        u32 stack_offset;
    };
    static constexpr Row rows[] = {
        {0x08C8D944u, 0x08CF5984u, 0x08CA809Cu, 0x48u}, {0x08C8D93Cu, 0x08CF5974u, 0x08CA80ACu, 0x40u},
        {0x08C8D934u, 0x08CF5964u, 0x08CA80BCu, 0x38u}, {0x08C8D92Cu, 0x08CF5954u, 0x08CA80CCu, 0x30u},
        {0x08C8D924u, 0x08CF5944u, 0x08CA80DCu, 0x28u}, {0x08C8D91Cu, 0x08CF5934u, 0x08CA80ECu, 0x20u},
        {0x08C8D914u, 0x08CF5924u, 0x08CA80FCu, 0x18u}, {0x08C8D90Cu, 0x08CF5914u, 0x08CA810Cu, 0x10u},
        {0x08C8D904u, 0x08CF5904u, 0x08CA811Cu, 0x08u}, {0x08C8D8FCu, 0x08CF58F4u, 0x08CA8128u, 0x00u},
    };
    for (const Row &row : rows) {
        const u32 first = word(rt, row.pair);
        const u32 second = word(rt, row.pair + 4u);
        put(rt, frame.at(row.stack_offset), first);
        put(rt, frame.at(row.stack_offset + 4u), second);
        call(rt, ctx, 0x08C0B44Cu, {row.descriptor, row.name, frame.at(row.stack_offset), 0x08CF58C8u});
    }
}

// ---- 0x08AEF1D0: fixed-point (12-bit fraction) sprite/geometry setup ----
// s3 = a0 (selector at +0x18, index at +0x1C), s1 = a1 (source values). The two 08AF0E9C lookups scale a1's +4/+8, a
// table at 0x08CE09D0 picked by 2*+0x24 + +0x1C scales them again, 08AD9860 picks between the two (when it answers 0
// they are averaged), then a0+0x18 selects which block of state the result is written to.
s32 mul32(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); }
s32 fixed12(s32 a, s32 b) { return mul32(a, b) >> 12; }
void func_08AEF1D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = self;
    ctx.gpr[17] = source;
    const s32 base = static_cast<s32>((word(rt, source + 0xCu) >> 1) & 0xFFFu);
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

// ---- 0x0886DB78: constructor of a large object (vtable 0x08CBF748, then 0x08CC6850) ----
// Allocates a 0x338-byte body with 08B62344, initialises its member blocks through the member constructors, zeroes the
// arrays and small blocks, copies a 14-byte run from 0x08C9F754, and parks the body at this+4. v0 = body + 0x330.
void func_0886DB78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;
    put(rt, self, 0x08CBF748u);
    const u32 body = call(rt, ctx, 0x08B62344u, {0x338u});
    ctx.gpr[17] = body;
    call(rt, ctx, 0x08B93A70u, {body});
    const u32 block60 = body + 0x60u;
    ctx.gpr[19] = block60;
    put(rt, body + 0x0Cu, 0u);
    put(rt, body + 0x28u, 0u);
    put(rt, body + 0x2Cu, 0u);
    rt.memory().store8(body + 0x30u, 0u);
    put(rt, body + 0x34u, 0u);
    call(rt, ctx, 0x0887D080u, {body + 0x38u});
    put(rt, body + 0x44u, 0u);
    call(rt, ctx, 0x0890B2F4u, {block60});
    rt.memory().store8(body + 0x68u, 0u);
    put(rt, body + 0x6Cu, 0u);
    put(rt, body + 0x70u, 0u);
    put(rt, body + 0x74u, 0u);
    put(rt, body + 0x78u, 0u);
    put(rt, body + 0x7Cu, 0u);
    put(rt, body + 0x80u, 0u);
    put(rt, body + 0x84u, 0u);
    call(rt, ctx, 0x0887DE44u, {body + 0x88u, self});
    put(rt, body + 0xF8u, 0x08CC6838u);
    put(rt, body + 0xECu, 0u);
    put(rt, body + 0xF0u, 0u);
    put(rt, body + 0xF4u, 0u);
    put(rt, body + 0xFCu, 0u);
    put(rt, body + 0x100u, 0u);
    put(rt, body + 0x104u, 0u);
    put(rt, body + 0x108u, 0u);
    call(rt, ctx, 0x08B28DCCu, {body + 0x110u});
    const u32 one = std::bit_cast<u32>(1.0f);
    put(rt, body + 0x150u, 0u);
    const u32 block = body + 0x15Cu;
    const u32 array = body + 0x184u;
    put(rt, body + 0xF8u, 0x08CC6850u);
    put(rt, body + 0x138u, 0u);
    put(rt, body + 0x13Cu, 0u);
    put(rt, body + 0x144u, 0u);
    put(rt, body + 0x148u, one);
    rt.memory().store8(body + 0x14Cu, 0u);
    put(rt, body + 0x154u, 0u);
    put(rt, body + 0x158u, 0u);
    put(rt, body + 0x15Cu, 0u);
    const u32 masked = word(rt, block + 0x20u) & ~7u;   // ins $v0, $zero, 0, 3
    put(rt, block + 8u, one);
    put(rt, block + 0xCu, 0u);
    const u32 copied_float = word(rt, block + 8u);
    const u32 copied_zero = word(rt, block + 0xCu);
    put(rt, block + 0x20u, masked);
    put(rt, block + 0x10u, copied_float);
    put(rt, block + 0x14u, copied_zero);
    put(rt, block + 4u, one);
    put(rt, block + 0x18u, copied_float);
    put(rt, block + 0x1Cu, copied_zero);
    put(rt, body + 0x180u, 0u);
    for (u32 i = 0; i < 16u; ++i) put(rt, array + 4u * i, 0u);
    const u32 link1 = body + 0x1DCu;
    const u32 link2 = body + 0x1F4u;
    const u32 node1 = body + 0x1C4u;
    const u32 node2 = body + 0x1C8u;
    const u32 node3 = body + 0x1E0u;
    const u32 node4 = body + 0x1F8u;
    put(rt, node1 + 0x14u, 0u);
    put(rt, node1 + 0x10u, node2);
    put(rt, node1 + 0xCu, node2);
    put(rt, node1 + 4u, 0u);
    put(rt, node1 + 8u, 0u);
    put(rt, link1 + 0x10u, node3);
    put(rt, link1 + 0xCu, node3);
    put(rt, link1 + 4u, 0u);
    put(rt, link1 + 0x14u, 0u);
    put(rt, link1 + 8u, 0u);
    put(rt, link2 + 0x10u, node4);
    put(rt, link2 + 4u, 0u);
    put(rt, link2 + 0x14u, 0u);
    put(rt, link2 + 8u, 0u);
    put(rt, link2 + 0xCu, node4);
    put(rt, body + 0x32Cu, 0u);
    call(rt, ctx, 0x08B36018u, {body + 0x20Cu, 0u, 0x100u});
    call(rt, ctx, 0x08B36018u, {body + 0x30Cu, 0u, 0x20u});
    const u32 tail = body + 0x30Cu;
    for (u32 i = 0; i < 14u; ++i) rt.memory().store8(tail + i, rt.memory().load8(0x08C9F754u + i));
    for (u32 i = 0; i < 3u; ++i) {
        put(rt, body + 4u * i + 0x10u, 0u);
        put(rt, body + 4u * i + 0x1Cu, 0u);
    }
    for (u32 i = 0; i < 2u; ++i) {
        put(rt, body + 4u * i + 0x48u, 0u);
        put(rt, body + 4u * i + 0x50u, 0u);
        put(rt, body + 4u * i + 0x58u, block60);
    }
    const u32 result = body + 0x330u;
    rt.memory().store16(body + 0x330u, 0u);
    rt.memory().store16(result + 4u, 0u);
    rt.memory().store16(result + 2u, 0u);
    put(rt, self + 4u, body);
    ctx.gpr[2] = result;
}

// ---- 0x088D2098: a 0x1F5-byte structure copy (dst = a0, src = a1), byte for byte ----
// The original copies the 12-byte header with sb/lwl/lwr and the rest in 0x10-byte chunks (loads before stores), leaving
// dst+0x10C..0x14C untouched; v0 ends as the source's end pointer a1 + 0x1F5.
void copy_bytes(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    std::uint8_t buffer[16];
    for (u32 done = 0; done < bytes;) {
        const u32 chunk = bytes - done < 16u ? bytes - done : 16u;
        for (u32 i = 0; i < chunk; ++i) buffer[i] = rt.memory().load8(source + done + i);
        for (u32 i = 0; i < chunk; ++i) rt.memory().store8(destination + done + i, buffer[i]);
        done += chunk;
    }
}

// ---- 0x08B03448: the closest approach of two 2-D segments, both clamped into [0,1] ----
// f22 = d(a1,a1), f21 = d(a3,a3), f24 = d(a1,&tmp), f23 = d(a3,&tmp) through 08BBBEC4 (float results). The three
// degenerate cases write zeros, the general one solves for the parameter and recomputes the second from it; the outs are
// t0 and t1. The 0x08BAC270/0x08BAC428 pair only fills a stack temporary. v0 ends as the last call's.
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
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
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
        put_float(rt, out1, 0.0f);   // the delay slot of the second test, taken on both of its paths
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

// ---- 0x08BD972C / 0x08BD99B4: destructor of a class with refcounted members and one plain buffer ----
// vtable 0x08CC5608, then the members at 0x148, 0x140, 0x50, 0x48 and 0x3C released, and the pointer at 0x54 freed by
// 08B93554(this-0xC, frame) unless it sits on the sentinel 0x08D75298 or 08B60804(p-4, -1) still counts references.
// The body then calls 0x08BD7388(this); 0x08BD99B4 additionally calls 0x08B64654(this), and v0 is the last call's.
void destroy_members_bd(Runtime &rt, AllegrexContext &ctx, bool extra_call) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CC5608u);
    release_object(rt, ctx, word(rt, self + 0x148u));
    release_object(rt, ctx, word(rt, self + 0x140u));
    ctx.gpr[18] = self + 0x44u;
    const u32 buffer = word(rt, self + 0x54u);
    const u32 inner = buffer - 0xCu;
    if (inner != 0x08D75298u) {
        const u32 users = call(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu});
        if (!(static_cast<s32>(users) > 0)) call(rt, ctx, 0x08B93554u, {inner, frame.at(0u)});
    }
    release_object(rt, ctx, word(rt, self + 0x50u));
    release_object(rt, ctx, word(rt, self + 0x48u));
    release_object(rt, ctx, word(rt, self + 0x3Cu));
    call(rt, ctx, 0x08BD7388u, {self});
    if (extra_call) call(rt, ctx, 0x08B64654u, {self});
}
void func_08BD972C(Runtime &rt, AllegrexContext &ctx) { destroy_members_bd(rt, ctx, false); }
void func_08BD99B4(Runtime &rt, AllegrexContext &ctx) { destroy_members_bd(rt, ctx, true); }

// ---- 0x08C789F4: read a run of bytes from a stream object into an 8-word result ----
// a1 = stream (its +0x90 object's virtual +0x14 gives the count), a2/a3 the 64-bit offset so far and t0 a mode flag.
// The 64-bit position is carried in (s2, s3) and passed to 0x08C77B74, which fills the stack result; v0 = a0.
void func_08C789F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 stream = ctx.gpr[5];
    const u32 low_arg = ctx.gpr[6];
    const u32 high_arg = ctx.gpr[7];
    const u32 mode = ctx.gpr[8];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
    SavedS5 saved5(ctx);
    SavedS6 saved6(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[17] = out;
    ctx.gpr[22] = mode;
    ctx.gpr[21] = high_arg;
    ctx.gpr[20] = low_arg;
    ctx.gpr[16] = stream;
    u32 count = 0;
    const u32 object = word(rt, stream + 0x90u);
    if (object != 0u) {
        const u32 got = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x14u), {object});
        count = static_cast<s32>(got) < 0 ? 0u : got;
    }
    put(rt, out + 4u, 0xFFFFFFFFu);
    put(rt, out + 0u, 0xFFFFFFFFu);
    put(rt, out + 8u, 0u);
    put(rt, out + 0xCu, 0u);
    put(rt, out + 0x10u, 0u);
    put(rt, out + 0x14u, 0u);
    put(rt, out + 0x18u, 0u);
    put(rt, out + 0x1Cu, 0u);
    const u32 any_32 = (low_arg | high_arg) != 0u ? 1u : 0u;
    const u32 low_count = static_cast<s32>(count) < 1 ? 1u : 0u;
    const u32 stop = low_count & any_32;
    const u64 product = static_cast<u64>(low_arg) * static_cast<u64>(count);
    if (call(rt, ctx, 0x08B61534u, {stream + 0x24u}) == 0u) {
        ctx.gpr[2] = out;
        return;
    }
    if (stop != 0u) {
        ctx.gpr[2] = out;
        return;
    }
    if (rt.memory().load8(stream + 0x8Cu) != 0u) {
        const u32 tail = word(rt, stream + 8u) ^ word(rt, stream + 4u);
        const u32 updated = (tail != 0u ? 1u : 0u) + word(rt, stream + 0x84u);
        put(rt, stream + 4u, word(rt, stream + 0x78u));
        put(rt, stream + 8u, updated);
        put(rt, stream + 0xCu, word(rt, stream + 0x88u));
        put(rt, stream + 0x84u, updated);
        rt.memory().store8(stream + 0x8Cu, 0u);
    }
    u32 position_low = static_cast<u32>(product);
    u32 position_high = static_cast<u32>(static_cast<u32>(product >> 32) +
                                         static_cast<u32>(mul32(static_cast<s32>(low_arg), static_cast<s32>(count) >> 31)) +
                                         static_cast<u32>(mul32(static_cast<s32>(count), static_cast<s32>(high_arg))));
    const u32 six_words[6] = {word(rt, stream + 0x30u), word(rt, stream + 0x34u), word(rt, stream + 0x38u),
                              word(rt, stream + 0x3Cu), word(rt, stream + 0x40u), word(rt, stream + 0x44u)};
    for (u32 i = 0; i < 6u; ++i) put(rt, frame.at(4u * i), six_words[i]);
    if (rt.memory().load8(stream + 0x81u) != 0u && mode == 1u) {
        const u32 object_again = word(rt, stream + 0x90u);
        if (call_indirect(rt, ctx, word(rt, word(rt, object_again) + 0x18u), {object_again}) != 0u) {
            const u32 delta = word(rt, stream + 8u) - word(rt, stream + 0xCu);
            const u32 sum = position_low + delta;
            const u32 carry = sum < position_low ? 1u : 0u;
            position_high = position_high + static_cast<u32>(static_cast<s32>(delta) >> 31) + carry;
            position_low = sum;
        } else {
            const u32 object_third = word(rt, stream + 0x90u);
            const u32 size = word(rt, stream + 8u) - word(rt, stream + 4u);
            const u32 read = call_indirect(rt, ctx, word(rt, word(rt, object_third) + 0x1Cu),
                                          {object_third, stream + 0x60u, word(rt, stream + 0x94u),
                                           word(rt, stream + 0x9Cu), size});
            const u32 base = word(rt, stream + 0x94u) + read - word(rt, stream + 0xA0u);
            const u32 sum = position_low + base;
            const u32 carry = sum < position_low ? 1u : 0u;
            position_high = position_high + static_cast<u32>(static_cast<s32>(base) >> 31) + carry;
            position_low = sum;
            const u32 next_words[6] = {word(rt, stream + 0x60u), word(rt, stream + 0x64u), word(rt, stream + 0x68u),
                                       word(rt, stream + 0x6Cu), word(rt, stream + 0x70u), word(rt, stream + 0x74u)};
            for (u32 i = 0; i < 6u; ++i) put(rt, frame.at(4u * i), next_words[i]);
        }
    }
    call(rt, ctx, 0x08C77B74u, {frame.at(0x20u), stream, position_low, position_high, mode, frame.at(0u)});
    put(rt, out + 4u, word(rt, frame.at(0x24u)));
    put(rt, out + 0u, word(rt, frame.at(0x20u)));
    put(rt, out + 8u, word(rt, frame.at(0x28u)));
    put(rt, out + 0xCu, word(rt, frame.at(0x2Cu)));
    put(rt, out + 0x10u, word(rt, frame.at(0x30u)));
    put(rt, out + 0x14u, word(rt, frame.at(0x34u)));
    put(rt, out + 0x18u, word(rt, frame.at(0x38u)));
    put(rt, out + 0x1Cu, word(rt, frame.at(0x3Cu)));
    ctx.gpr[2] = out;
}

// ---- 0x08889628: copy-assign a container of intrusive pointers (this = a0, source = a1) ----
// 0x08889244 prepares this from the source; the source's +0x10 element count picks the sub-container at this+0x40, whose
// element address is 12*count+0x10 past this. One element is appended or assigned a range, the temporary released, and
// every element copied: each new pointer is incremented (twice on the assignment path, as the original does), stored, and
// the old one released. v0 is the last call's result.

//// END FUNCTIONS
} // namespace
} // namespace new_batch_main_24
} // namespace gt::game

#define NEW24(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_24::name, gt::ReturnKind::kind)
NEW24(0x08BC12ACu, func_08BC12AC, Word);
NEW24(0x08BC1538u, func_08BC1538, Word);
NEW24(0x08BD045Cu, func_08BD045C, Word);
NEW24(0x08BD06E8u, func_08BD06E8, Word);
NEW24(0x08B4A198u, func_08B4A198, Float);
NEW24(0x089F1850u, func_089F1850, Word);
NEW24(0x08AEF1D0u, func_08AEF1D0, Word);
NEW24(0x0886DB78u, func_0886DB78, Word);
NEW24(0x08B03448u, func_08B03448, Word);
NEW24(0x08BD972Cu, func_08BD972C, Word);
NEW24(0x08BD99B4u, func_08BD99B4, Word);
NEW24(0x08C789F4u, func_08C789F4, Word);
//// END REGISTRATIONS
