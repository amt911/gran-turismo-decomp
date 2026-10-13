// Hot batch 57: small functions the four pad routes call thousands of times and no earlier batch took, picked with
// a call probe (GT_PROBE_SYMBOLS over every undone function up to 200 bytes): vector helpers on 2/3/4 floats, the
// pair-returning shims of the C++ runtime, the display-list writers of the graphics library (they append a word
// or two to the list whose write pointer sits at +0x5C of the library's state) and a few object helpers. Their
// classes and subsystems aren't known yet, so they keep their addresses as names; each comment says what the code
// does. Written from the asm; the generated code was only a reading aid.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_57 {
namespace {

// The graphics library's state block (.bss): flag word +0x6C, display-list write pointer +0x5C.
constexpr std::uint32_t kGuState = 0x08D6CE00u;
constexpr std::uint32_t kGuFlags = kGuState + 0x6Cu;
constexpr std::uint32_t kGuListPointer = kGuState + 0x5Cu;
// A second block (.bss) the library's command helpers take in a0.
constexpr std::uint32_t kGuCommandBlock = 0x08D6D0E0u;

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }
void put(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store32(address, value); }
float float_at(psprecomp::Runtime &rt, std::uint32_t address) { return std::bit_cast<float>(rt.memory().load32(address)); }
void put_float(psprecomp::Runtime &rt, std::uint32_t address, float value) { rt.memory().store32(address, std::bit_cast<std::uint32_t>(value)); }

// The FPU's multiply: inf * 0 is the PSP's default NaN (0x7FC00000), not the host's (0xFFC00000).
float psp_mul(float a, float b) {
    if ((std::isinf(a) && b == 0.0f) || (std::isinf(b) && a == 0.0f)) return std::bit_cast<float>(0x7FC00000u);
    return a * b;
}

// Appends two words to the display list and returns the new write pointer.
std::uint32_t append_two(psprecomp::Runtime &rt, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, list, first);
    put(rt, list + 4u, second);
    put(rt, kGuListPointer, list + 8u);
    return list + 8u;
}

void set_flags(psprecomp::Runtime &rt, std::uint32_t bits) { put(rt, kGuFlags, word(rt, kGuFlags) | bits); }

// The library's "flags |= 2, then the command helper on the command block" tails: a0 = the block.
void flag_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    set_flags(rt, 2u);
    tail_call(ctx, target, {kGuCommandBlock});
}

} // namespace

// --- Vectors of floats: d = a op b, v op= s, dot and length products -----------------------------------------

// func_08B4A6A0(v4, s): v4 *= s, component by component.
void func_08B4A6A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = ctx.fpr[12];
    for (std::uint32_t i = 0; i < 4u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(float_at(rt, ctx.gpr[4] + 4u * i), s));
}
// func_08B4A5E8(v3, s): v3 *= s.
void func_08B4A5E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = ctx.fpr[12];
    for (std::uint32_t i = 0; i < 3u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(float_at(rt, ctx.gpr[4] + 4u * i), s));
}
// func_08B4A560(v2, s): v2 *= s.
void func_08B4A560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = ctx.fpr[12];
    for (std::uint32_t i = 0; i < 2u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(float_at(rt, ctx.gpr[4] + 4u * i), s));
}
// func_08B4A6D4(v4, s): v4 /= s.
void func_08B4A6D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = ctx.fpr[12];
    for (std::uint32_t i = 0; i < 4u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, float_at(rt, ctx.gpr[4] + 4u * i) / s);
}

// d = a + b / a - b over `count` floats (d = a0, a = a1, b = a2).
void add_components(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) put_float(rt, ctx.gpr[4] + 4u * i, float_at(rt, ctx.gpr[5] + 4u * i) + float_at(rt, ctx.gpr[6] + 4u * i));
}
void sub_components(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) put_float(rt, ctx.gpr[4] + 4u * i, float_at(rt, ctx.gpr[5] + 4u * i) - float_at(rt, ctx.gpr[6] + 4u * i));
}
void func_08B4A890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_components(rt, ctx, 4u); }  // v4 add
void func_08B4A8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sub_components(rt, ctx, 4u); }  // v4 sub
void func_08B4AA34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_components(rt, ctx, 3u); }  // v3 add
void func_08B4AA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sub_components(rt, ctx, 3u); }  // v3 sub
void func_08B4AC0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_components(rt, ctx, 2u); }  // v2 add
void func_08C4669C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_components(rt, ctx, 4u); }  // v4 add (second copy)

// func_08C466E0(d4, s*, v4): d4[i] = *s * v4[i].
void func_08C466E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = float_at(rt, ctx.gpr[5]);
    for (std::uint32_t i = 0; i < 4u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(s, float_at(rt, ctx.gpr[6] + 4u * i)));
}

// func_08C46594(d3, a3, b3): d3[i] = a[i] < b[i] ? a[i] : b[i]; func_08C4653C: d3[i] = b[i] < a[i] ? a[i] : b[i].
void func_08C46594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 3u; ++i) {
        const float a = float_at(rt, ctx.gpr[5] + 4u * i), b = float_at(rt, ctx.gpr[6] + 4u * i);
        put_float(rt, ctx.gpr[4] + 4u * i, a < b ? a : b);
    }
}
void func_08C4653C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 3u; ++i) {
        const float a = float_at(rt, ctx.gpr[5] + 4u * i), b = float_at(rt, ctx.gpr[6] + 4u * i);
        put_float(rt, ctx.gpr[4] + 4u * i, b < a ? a : b);
    }
}

// func_08B4A744(v4) / func_08B4A720(v3) -> the sum of the squares, left to right.
void func_08B4A744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = float_at(rt, ctx.gpr[4]), y = float_at(rt, ctx.gpr[4] + 4u), z = float_at(rt, ctx.gpr[4] + 8u), w = float_at(rt, ctx.gpr[4] + 12u);
    float sum = psp_mul(x, x) + psp_mul(y, y);
    sum = sum + psp_mul(z, z);
    ctx.fpr[0] = sum + psp_mul(w, w);
}
void func_08B4A720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = float_at(rt, ctx.gpr[4]), y = float_at(rt, ctx.gpr[4] + 4u), z = float_at(rt, ctx.gpr[4] + 8u);
    const float sum = psp_mul(x, x) + psp_mul(y, y);
    ctx.fpr[0] = sum + psp_mul(z, z);
}
// func_08B4A9A0(a3, b3) / func_08B4A7EC(a4, b4) -> the dot product, left to right.
void func_08B4A9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float sum = psp_mul(float_at(rt, ctx.gpr[4]), float_at(rt, ctx.gpr[5])) + psp_mul(float_at(rt, ctx.gpr[4] + 4u), float_at(rt, ctx.gpr[5] + 4u));
    ctx.fpr[0] = sum + psp_mul(float_at(rt, ctx.gpr[4] + 8u), float_at(rt, ctx.gpr[5] + 8u));
}
void func_08B4A7EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float sum = psp_mul(float_at(rt, ctx.gpr[4]), float_at(rt, ctx.gpr[5])) + psp_mul(float_at(rt, ctx.gpr[4] + 4u), float_at(rt, ctx.gpr[5] + 4u));
    sum = sum + psp_mul(float_at(rt, ctx.gpr[4] + 8u), float_at(rt, ctx.gpr[5] + 8u));
    ctx.fpr[0] = sum + psp_mul(float_at(rt, ctx.gpr[4] + 12u), float_at(rt, ctx.gpr[5] + 12u));
}

// func_08C524EC(object) -> the float at +0x8, negated when it is <= 0.
void func_08C524EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float value = float_at(rt, ctx.gpr[4] + 8u);
    if (value <= 0.0f) value = -value;
    ctx.fpr[0] = value;
}

// func_08B49D7C(x) -> 0 for x < 0, else sqrt(x).
void func_08B49D7C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = ctx.fpr[12] < 0.0f ? 0.0f : std::sqrt(ctx.fpr[12]);
}

// func_08B6ECB0(x) -> whether x is a NaN; func_08B6EC94(x) -> whether it is finite (the absolute bits against 0x7F800000).
void func_08B6ECB0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (std::bit_cast<std::uint32_t>(ctx.fpr[12]) & 0x7FFFFFFFu) > 0x7F800000u ? 1u : 0u;
}
void func_08B6EC94(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (std::bit_cast<std::uint32_t>(ctx.fpr[12]) & 0x7FFFFFFFu) > 0x7F7FFFFFu ? 0u : 1u;
}

// func_08BADADC(p*, v3): **p = v3, one float at a time, reading *p again for each store.
void func_08BADADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t target = word(rt, ctx.gpr[4]);
    put(rt, target, word(rt, ctx.gpr[5]));
    target = word(rt, ctx.gpr[4]);
    put(rt, target + 4u, word(rt, ctx.gpr[5] + 4u));
    target = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = target;
    put(rt, target + 8u, word(rt, ctx.gpr[5] + 8u));
}

// --- Pairs: functions returning their two arguments as a 64-bit value (v0:v1) --------------------------------

void func_08C45F1C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45E6C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45E58(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45FA4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45DAC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45F08(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45E2C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45FF8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08BAC248(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
// func_08C45EF0 / func_08C45FE0(a, x): the pair (the bits of x, a) — x goes through the stack as a float.
void func_08C45EF0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = ctx.gpr[4];
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
}
void func_08C45FE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = ctx.gpr[4];
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
}

// --- Small objects --------------------------------------------------------------------------------------------

// func_08BAA444(this, a, b, p*) -> this: this[0] = a, this[1] = b, this[2] = p[0], this[3] = p[1].
void func_08BAA444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t third = word(rt, ctx.gpr[7]), fourth = word(rt, ctx.gpr[7] + 4u);
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 4u, ctx.gpr[6]);
    put(rt, self + 12u, fourth);
    put(rt, self + 8u, third);
    ctx.gpr[2] = self;
}
// func_08BAA488(this, a, b, c, p*) -> this: this[0] = a, [1] = b, [2] = c, [3] = p[0], [4] = p[1] (p is the fifth argument, $t0).
void func_08BAA488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[8];
    const std::uint32_t fourth = word(rt, p), fifth = word(rt, p + 4u);
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 4u, ctx.gpr[6]);
    put(rt, self + 8u, ctx.gpr[7]);
    put(rt, self + 16u, fifth);
    put(rt, self + 12u, fourth);
    ctx.gpr[2] = self;
}
// func_08BAA3A8(this, value) -> whether it changed: sets this[0] = value.
void func_08BAA3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = word(rt, ctx.gpr[4]);
    put(rt, ctx.gpr[4], ctx.gpr[5]);
    ctx.gpr[2] = (old ^ ctx.gpr[5]) != 0u ? 1u : 0u;
}
// func_08B5C078(node, pos): links `node` into a circular doubly linked list before `pos` (next at +0, previous at +4).
void func_08B5C078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4], pos = ctx.gpr[5];
    const std::uint32_t previous = word(rt, pos + 4u);
    put(rt, node, pos);
    put(rt, node + 4u, previous);
    const std::uint32_t before = word(rt, pos + 4u);
    put(rt, pos + 4u, node);
    ctx.gpr[2] = before;
    put(rt, before, node);
}
// func_08B5C094(node): unlinks `node` from such a list.
void func_08B5C094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    const std::uint32_t previous = word(rt, node + 4u);
    const std::uint32_t next = word(rt, node);
    ctx.gpr[2] = next;
    put(rt, next + 4u, previous);
    put(rt, previous, next);
}
// func_08B3467C(this): resets a record: words +0..+0x14 and +0x20 to 0, +0x18, +0x1C and +0x24 to -1.
void func_08B3467C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self + 36u, 0xFFFFFFFFu);
    for (std::uint32_t offset = 0; offset <= 20u; offset += 4u) put(rt, self + offset, 0u);
    put(rt, self + 24u, 0xFFFFFFFFu);
    put(rt, self + 28u, 0xFFFFFFFFu);
    put(rt, self + 32u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}
// func_0886A9A0(this, i) -> this->slot[i] (words from +0x24) for i < 8, else 0.
void func_0886A9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[5] < 8u ? word(rt, ctx.gpr[4] + ctx.gpr[5] * 4u + 36u) : 0u;
}
// func_08B6EE4C(a, b) -> 0, 1 or 2 as the 64-bit a (a1:a0) is less than, equal to or greater than b (a3:a2): signed high words, unsigned low words.
void func_08B6EE4C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low_a = ctx.gpr[4], low_b = ctx.gpr[6];
    const auto high_a = static_cast<std::int32_t>(ctx.gpr[5]), high_b = static_cast<std::int32_t>(ctx.gpr[7]);
    if (high_a < high_b) ctx.gpr[2] = 0u;
    else if (high_b < high_a) ctx.gpr[2] = 2u;
    else if (low_a < low_b) ctx.gpr[2] = 0u;
    else ctx.gpr[2] = low_b < low_a ? 2u : 1u;
}

// func_08C09520(this) -> whether the virtual +0x20 returns nonzero.
void func_08C09520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 32u), {self}) != 0u ? 1u : 0u;
}
// func_08C19BEC(this) -> the virtual +0x8 of the object at +0x14 (its result), or 0 without one.
void func_08C19BEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 20u);
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 8u), {object});
}

// --- Interrupt-guarded counters and the kernel error test (the imports are the SDK's suspend/resume interrupts) ---

// func_08B34D40(count*) -> the count after incrementing it under suspended interrupts; the first increment also records the
// interrupt state in +4.
void func_08B34D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t state = call_guest(rt, ctx, 0x08C7A888u, {self});
    const std::uint32_t count = word(rt, self) + 1u;
    put(rt, self, count);
    if (count == 1u) put(rt, self + 4u, state);
    ctx.gpr[2] = count;
}
// func_08B34D80(count*): decrements it; when it reaches 0, resumes the interrupt state saved in +4 (tail call).
void func_08B34D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count = word(rt, self) - 1u;
    put(rt, self, count);
    ctx.gpr[2] = count;
    if (count != 0u) return;
    tail_call(ctx, 0x08C7A8A8u, {word(rt, self + 4u)});
}
// func_08B60804(p*, n) -> the old *p, after *p += n under suspended interrupts.
void func_08B60804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4], amount = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t state = call_guest(rt, ctx, 0x08C7A888u, {target});
    const std::uint32_t old = word(rt, target);
    put(rt, target, word(rt, target) + amount);
    (void)call_guest(rt, ctx, 0x08C7A8A8u, {state});
    ctx.gpr[2] = old;
}
// func_08B3453C(x) -> whether the import 0x08C7A890(x, 1) answered something other than the two error codes -0x7FFDFE35 and -0x7FFDFE3C.
void func_08B3453C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08C7A890u, {ctx.gpr[4], 1u});
    const bool is_error = result + 0x7FFDFE3Cu == 0u || result + 0x7FFDFE35u == 0u;
    ctx.gpr[2] = is_error ? 0u : 1u;
}

// --- Tails that only prepare arguments ------------------------------------------------------------------------

// func_08B8542C(this): func_08A5BC54(this, this[1] + 13, 4).
void func_08B8542C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08A5BC54u, {ctx.gpr[4], word(rt, ctx.gpr[4] + 4u) + 13u, 4u});
}
// func_08B537CC(a, b): func_08B5358C(b, a).
void func_08B537CC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    ctx.gpr[2] = a;
    tail_call(ctx, 0x08B5358Cu, {b, a});
}
// func_08B4EBE0(a, b) / func_08B4E9D4(a, b): the same function with (a, a, b) in a0..a2.
void func_08B4EBE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B4EB70u, {ctx.gpr[4], ctx.gpr[4], ctx.gpr[5]}); }
void func_08B4E9D4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B4E994u, {ctx.gpr[4], ctx.gpr[4], ctx.gpr[5]}); }
// func_08B77BFC(a): func_08B7DF7C(a, 0, 10).
void func_08B77BFC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B7DF7Cu, {ctx.gpr[4], 0u, 10u}); }
// func_08B52D58(this): func_08B58E28(this[0], this[1], this[2], this[3]) (returns this in v0 until the callee sets it).
void func_08B52D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = self;
    tail_call(ctx, 0x08B58E28u, {word(rt, self), word(rt, self + 4u), word(rt, self + 8u), word(rt, self + 12u)});
}
// func_08B2EFDC(a, b): func_08B1C590(0x08D69138, b, a, b).
void func_08B2EFDC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C590u, {0x08D69138u, ctx.gpr[5], ctx.gpr[4], ctx.gpr[5]});
}
// func_08B52D90(x): func_08B54CE4(0x08D695C8, x).
void func_08B52D90(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B54CE4u, {0x08D695C8u, ctx.gpr[4]}); }

// --- The graphics library (sceGu-like state at 0x08D6CE00) ---------------------------------------------------

// func_08B59838(): the flag word +0x6C of the state; when nonzero, func_08B59118(state, flags) (tail call).
void func_08B59838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    ctx.gpr[2] = flags;
    if (flags == 0u) return;
    tail_call(ctx, 0x08B59118u, {kGuState, flags});
}
// func_08B57DE8(x): stores x at 0x08D6D0C4.
void func_08B57DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, 0x08D6D0C4u, ctx.gpr[4]); }
// func_08B55B90(): sets bit 11 in the words +0x278 and +0x27C and bit 7 in the flags.
void func_08B55B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &) {
    const std::uint32_t second = word(rt, kGuState + 636u), first = word(rt, kGuState + 632u), flags = word(rt, kGuFlags);
    put(rt, kGuFlags, flags | 128u);
    put(rt, kGuState + 636u, second | 2048u);
    put(rt, kGuState + 632u, first | 2048u);
}
// func_08B584D0(a, b, c): state +0x100 = a, +0xF8 = b, +0xFC = c; bit 11 of the flags.
void func_08B584D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuState + 256u, ctx.gpr[4]);
    put(rt, kGuFlags, flags | 2048u);
    put(rt, kGuState + 248u, ctx.gpr[5]);
    put(rt, kGuState + 252u, ctx.gpr[6]);
}
// func_08B58744(a, b, c): records them (+0xF0 word, +0xF4 and +0xF5 bytes) and appends the command 0xDB000000 | c << 16 | b << 8 | a & 7.
void func_08B58744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = word(rt, kGuListPointer);
    const std::uint32_t command = 0xDB000000u | ((ctx.gpr[6] & 0xFFu) << 16) | ((ctx.gpr[5] & 0xFFu) << 8) | (ctx.gpr[4] & 7u);
    put(rt, kGuState + 240u, ctx.gpr[4]);
    rt.memory().store8(kGuState + 244u, static_cast<std::uint8_t>(ctx.gpr[5]));
    rt.memory().store8(kGuState + 245u, static_cast<std::uint8_t>(ctx.gpr[6]));
    put(rt, list, command);
    put(rt, kGuListPointer, list + 4u);
}
// func_08B5711C(mask) / func_08B57138(mask): clears the bits of `mask` in the word +0x48 / +0x278 of the state.
void func_08B5711C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, kGuState + 72u, word(rt, kGuState + 72u) & ~ctx.gpr[4]); }
void func_08B57138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, kGuState + 632u, word(rt, kGuState + 632u) & ~ctx.gpr[4]); }
// func_08B571AC(mask): sets the bits of `mask` in the word +0x44 and bit 0 of the flags.
void func_08B571AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mask = word(rt, kGuState + 68u), flags = word(rt, kGuFlags);
    put(rt, kGuFlags, flags | 1u);
    put(rt, kGuState + 68u, mask | ctx.gpr[4]);
}
// func_08B550A8(x) -> the new list pointer: appends 0x10000000 | (x >> 24) << 16 and 0x0A000000 | x & 0xFFFFFF.
void func_08B550A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    ctx.gpr[2] = append_two(rt, 0x10000000u | (((x >> 24) & 0xFFu) << 16), 0x0A000000u | (x & 0x00FFFFFFu));
}
// func_08B58978(a, b, c, d): halfwords +0x140..+0x146 of the state = a..d; bit 3 of the flags.
void func_08B58978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    rt.memory().store16(kGuState + 320u, static_cast<std::uint16_t>(ctx.gpr[4]));
    put(rt, kGuFlags, flags | 8u);
    rt.memory().store16(kGuState + 322u, static_cast<std::uint16_t>(ctx.gpr[5]));
    rt.memory().store16(kGuState + 324u, static_cast<std::uint16_t>(ctx.gpr[6]));
    rt.memory().store16(kGuState + 326u, static_cast<std::uint16_t>(ctx.gpr[7]));
}
// func_08B58A54(a, b, c, d): halfwords +0x130..+0x136 of the state = a..d, then func_08B589A0(state) (tail call).
void func_08B58A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store16(kGuState + 304u, static_cast<std::uint16_t>(ctx.gpr[4]));
    rt.memory().store16(kGuState + 306u, static_cast<std::uint16_t>(ctx.gpr[5]));
    rt.memory().store16(kGuState + 308u, static_cast<std::uint16_t>(ctx.gpr[6]));
    rt.memory().store16(kGuState + 310u, static_cast<std::uint16_t>(ctx.gpr[7]));
    tail_call(ctx, 0x08B589A0u, {kGuState});
}
// func_08B551D0(p0, p1, p2, p3): each non-null pointer receives the halfword +0x130 / +0x132 / +0x134 / +0x136 of the state (sign-extended).
void func_08B551D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint32_t out = ctx.gpr[4 + i];
        if (out != 0u) put(rt, out, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(kGuState + 304u + 2u * i)))));
    }
}
// func_08B58A74(flag): appends 0xE7000000 | (flag & 0xFF == 0).
void func_08B58A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = word(rt, kGuListPointer);
    const std::uint32_t command = 0xE7000000u | ((ctx.gpr[4] & 0xFFu) == 0u ? 1u : 0u);
    put(rt, list, command);
    put(rt, kGuListPointer, list + 4u);
    ctx.gpr[2] = command;  // left in v0, and hot_leaves' wrapper passes it on
}
// func_08B58960(x, y): floats +0x138 and +0x13C of the state = x, y, then func_08B58B5C(state) (tail call).
void func_08B58960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put_float(rt, kGuState + 312u, ctx.fpr[12]);
    put_float(rt, kGuState + 316u, ctx.fpr[13]);
    ctx.gpr[2] = kGuState;  // v0 holds the state address when the tail call starts
    tail_call(ctx, 0x08B58B5Cu, {kGuState});
}
// func_08B57154(i): byte 1 at 0x08D6CE0C + i * 40 - 3601; bit 10 of the flags.
void func_08B57154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(0x08D6CE0Cu + ctx.gpr[4] * 40u - 3601u, 1u);
    set_flags(rt, 1024u);
}
// func_08B587F8(x): state +0x74 = x; bit 8 of the flags.
void func_08B587F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuState + 116u, ctx.gpr[4]);
    put(rt, kGuFlags, flags | 256u);
}
// func_08B587A8(n): state +0xC0 = n; appends 0xDE000000 | n & 7.
void func_08B587A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, kGuState + 192u, ctx.gpr[4]);
    put(rt, list, 0xDE000000u | (ctx.gpr[4] & 7u));
    put(rt, kGuListPointer, list + 4u);
}
// func_08B58AD0(r, g, b, a): func_08B58AA0 with the colour of the four nonzero flags: 0xFF in the byte of each one set.
void func_08B58AD0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t colour = ((ctx.gpr[4] & 0xFFu) != 0u ? 0xFFu : 0u) | ((ctx.gpr[5] & 0xFFu) != 0u ? 0xFF00u : 0u) |
                                 ((ctx.gpr[6] & 0xFFu) != 0u ? 0x00FF0000u : 0u) | ((ctx.gpr[7] & 0xFFu) != 0u ? 0xFF000000u : 0u);
    tail_call(ctx, 0x08B58AA0u, {colour});
}
// func_08B56A00(x): func_08B569AC(block, x); func_08B569A0(): func_08B56960(block); func_08B567F4(x): func_08B567B0(block, x).
void func_08B56A00(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B569ACu, {kGuCommandBlock, ctx.gpr[4]}); }
void func_08B569A0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B56960u, {kGuCommandBlock}); }
void func_08B567F4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B567B0u, {kGuCommandBlock, ctx.gpr[4]}); }
// The same with bit 1 of the flags set first.
void func_08B56940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B56900u); }
void func_08B56364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B56300u); }
void func_08B562E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B56270u); }
void func_08B56510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B564C8u); }
void func_08B55EF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B55EC0u); }
void func_08B56128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B5608Cu); }
void func_08B55FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_then(rt, ctx, 0x08B55F14u); }
// func_08B566BC(x): bit 1 of the flags, then func_08B56674(block, x).
void func_08B566BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_flags(rt, 2u);
    tail_call(ctx, 0x08B56674u, {kGuCommandBlock, ctx.gpr[4]});
}
// func_08B5A2D4(): the import 0x08C7A8A0(state, 1, 0) (tail call).
void func_08B5A2D4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08C7A8A0u, {kGuState, 1u, 0u}); }
// func_08B53844(x): records x at 0x08D695C4 and appends 0x2C000000 | bits(K - x) >> 8 and 0x2D000000 | bits(x) >> 8, K the float at 0x08CB618C.
void func_08B53844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float complement = float_at(rt, 0x08CB618Cu) - x;
    const std::uint32_t second = 0x2C000000u | (std::bit_cast<std::uint32_t>(complement) >> 8);
    const std::uint32_t first = 0x2D000000u | (std::bit_cast<std::uint32_t>(x) >> 8);
    const std::uint32_t list = word(rt, kGuListPointer);
    put_float(rt, 0x08D695C4u, x);
    put(rt, list, second);
    put(rt, list + 4u, first);
    put(rt, kGuListPointer, list + 8u);
}
// func_08B57D90(near, far): records both (+0x194, +0x198) and appends 0xCD000000 | bits(far) >> 8 and 0xCE000000 | bits(K / (far - near)) >> 8, K the float at 0x08CB61CC.
void func_08B57D90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float near_value = ctx.fpr[12], far_value = ctx.fpr[13];
    const float quotient = float_at(rt, 0x08CB61CCu) / (far_value - near_value);
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, list, 0xCD000000u | (std::bit_cast<std::uint32_t>(far_value) >> 8));
    put(rt, list + 4u, 0xCE000000u | (std::bit_cast<std::uint32_t>(quotient) >> 8));
    put_float(rt, kGuState + 408u, far_value);
    put(rt, kGuListPointer, list + 8u);
    put_float(rt, kGuState + 404u, near_value);
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(quotient);  // the last value the original moves through v0
}
// func_08B581BC(x): the float at +0xC4 of the state = x clamped to [0, K], K the float at 0x08CB61D0.
void func_08B581BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float value = ctx.fpr[12];
    float bound = 0.0f;
    if (value < bound) {
        value = bound;
    } else {
        bound = float_at(rt, 0x08CB61D0u);
        if (bound < value) value = bound;
    }
    ctx.fpr[0] = bound;
    ctx.fpr[12] = value;
    put_float(rt, 0x08D6CEC4u, value);
}
// func_08B58718(x): func_08B5853C(a0, clamp(trunc(x * K), 0, 255)), K the float at 0x08CB61D8 (tail call).
void func_08B58718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float scaled = psp_mul(ctx.fpr[12], float_at(rt, 0x08CB61D8u));
    const auto truncated = static_cast<std::int32_t>(ctx.fpu_float_to_word(scaled, 1u));
    const std::int32_t clamped = truncated > 0 ? (truncated < 255 ? truncated : 255) : 0;
    ctx.fpr[12] = scaled;
    ctx.gpr[2] = 255u;
    tail_call(ctx, 0x08B5853Cu, {ctx.gpr[4], static_cast<std::uint32_t>(clamped) & 255u});
}
// func_08B5877C(x): func_08B58744(a0, clamp(trunc(x * K), 0, 255), 255), K the float at 0x08CB61DC (tail call).
void func_08B5877C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float scaled = psp_mul(ctx.fpr[12], float_at(rt, 0x08CB61DCu));
    const auto truncated = static_cast<std::int32_t>(ctx.fpu_float_to_word(scaled, 1u));
    const std::int32_t clamped = truncated > 0 ? (truncated < 255 ? truncated : 255) : 0;
    ctx.fpr[12] = scaled;
    ctx.gpr[2] = 255u;
    tail_call(ctx, 0x08B58744u, {ctx.gpr[4], static_cast<std::uint32_t>(clamped), 255u});
}
// func_08B54CE4(this): func_08B54A64(this, 2); this[2] += 1; clears the words 0x08D6CDEC and 0x08D6CDE8.
void func_08B54CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B54A64u, {self, 2u});
    }
    put(rt, self + 8u, word(rt, self + 8u) + 1u);
    put(rt, 0x08D6CDECu, 0u);
    put(rt, 0x08D6CDE8u, 0u);
}

} // namespace hot_batch_57
} // namespace gt::game

// Registrations. The return kind is the contract the callers need (docs/agents/decomp-workflow.md): Word where v0 means
// something, DoubleWord for the pair-returning shims, Float for f0, Void where no caller reads a result.
// func_08B52D58 and func_08B2EFDC are written but not registered: compared 0 in every fast run (no route calls them
// from outside their generated unit), so nothing could verify them.
#define HOT57(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_57::name, gt::ReturnKind::kind)
HOT57(0x08B4A6A0u, func_08B4A6A0, Void);
HOT57(0x08B4A5E8u, func_08B4A5E8, Void);
HOT57(0x08B4A560u, func_08B4A560, Void);
HOT57(0x08B4A6D4u, func_08B4A6D4, Void);
HOT57(0x08B4A890u, func_08B4A890, Void);
HOT57(0x08B4A8D4u, func_08B4A8D4, Void);
HOT57(0x08B4AA34u, func_08B4AA34, Word);
HOT57(0x08B4AA68u, func_08B4AA68, Void);
HOT57(0x08B4AC0Cu, func_08B4AC0C, Void);
HOT57(0x08C4669Cu, func_08C4669C, Void);
HOT57(0x08C466E0u, func_08C466E0, Void);
HOT57(0x08C46594u, func_08C46594, Void);
HOT57(0x08C4653Cu, func_08C4653C, Void);
HOT57(0x08B4A744u, func_08B4A744, Float);
HOT57(0x08B4A720u, func_08B4A720, Float);
HOT57(0x08B4A9A0u, func_08B4A9A0, Float);
HOT57(0x08B4A7ECu, func_08B4A7EC, Float);
HOT57(0x08C524ECu, func_08C524EC, Float);
HOT57(0x08B49D7Cu, func_08B49D7C, Float);
HOT57(0x08B6ECB0u, func_08B6ECB0, Word);
HOT57(0x08B6EC94u, func_08B6EC94, Word);
HOT57(0x08BADADCu, func_08BADADC, Void);
HOT57(0x08C45F1Cu, func_08C45F1C, DoubleWord);
HOT57(0x08C45E6Cu, func_08C45E6C, DoubleWord);
HOT57(0x08C45E58u, func_08C45E58, DoubleWord);
HOT57(0x08C45FA4u, func_08C45FA4, DoubleWord);
HOT57(0x08C45DACu, func_08C45DAC, DoubleWord);
HOT57(0x08C45F08u, func_08C45F08, DoubleWord);
HOT57(0x08C45E2Cu, func_08C45E2C, DoubleWord);
HOT57(0x08C45FF8u, func_08C45FF8, DoubleWord);
HOT57(0x08BAC248u, func_08BAC248, DoubleWord);
HOT57(0x08C45EF0u, func_08C45EF0, DoubleWord);
HOT57(0x08C45FE0u, func_08C45FE0, DoubleWord);
HOT57(0x08BAA444u, func_08BAA444, Word);
HOT57(0x08BAA488u, func_08BAA488, Word);
HOT57(0x08BAA3A8u, func_08BAA3A8, Word);
HOT57(0x08B5C078u, func_08B5C078, Void);
HOT57(0x08B5C094u, func_08B5C094, Void);
HOT57(0x08B3467Cu, func_08B3467C, Word);
HOT57(0x0886A9A0u, func_0886A9A0, Word);
HOT57(0x08B6EE4Cu, func_08B6EE4C, Word);
HOT57(0x08C09520u, func_08C09520, Word);
HOT57(0x08C19BECu, func_08C19BEC, Word);
HOT57(0x08B34D40u, func_08B34D40, Word);
HOT57(0x08B34D80u, func_08B34D80, Word);
HOT57(0x08B60804u, func_08B60804, Word);
HOT57(0x08B3453Cu, func_08B3453C, Word);
HOT57(0x08B8542Cu, func_08B8542C, Word);
HOT57(0x08B537CCu, func_08B537CC, Word);
HOT57(0x08B4EBE0u, func_08B4EBE0, Word);
HOT57(0x08B4E9D4u, func_08B4E9D4, Word);
HOT57(0x08B77BFCu, func_08B77BFC, Word);
// HOT57(0x08B52D58u, func_08B52D58, Word);
// HOT57(0x08B2EFDCu, func_08B2EFDC, Word);
HOT57(0x08B52D90u, func_08B52D90, Word);
HOT57(0x08B59838u, func_08B59838, Word);
HOT57(0x08B57DE8u, func_08B57DE8, Void);
HOT57(0x08B55B90u, func_08B55B90, Void);
HOT57(0x08B584D0u, func_08B584D0, Void);
HOT57(0x08B58744u, func_08B58744, Void);
HOT57(0x08B5711Cu, func_08B5711C, Void);
HOT57(0x08B57138u, func_08B57138, Void);
HOT57(0x08B571ACu, func_08B571AC, Void);
HOT57(0x08B550A8u, func_08B550A8, Word);
HOT57(0x08B58978u, func_08B58978, Void);
HOT57(0x08B58A54u, func_08B58A54, Void);
HOT57(0x08B551D0u, func_08B551D0, Void);
HOT57(0x08B58A74u, func_08B58A74, Word);
HOT57(0x08B58960u, func_08B58960, Word);
HOT57(0x08B57154u, func_08B57154, Void);
HOT57(0x08B587F8u, func_08B587F8, Void);
HOT57(0x08B587A8u, func_08B587A8, Void);
HOT57(0x08B58AD0u, func_08B58AD0, Void);
HOT57(0x08B56A00u, func_08B56A00, Word);
HOT57(0x08B569A0u, func_08B569A0, Void);
HOT57(0x08B567F4u, func_08B567F4, Void);
HOT57(0x08B56940u, func_08B56940, Void);
HOT57(0x08B56364u, func_08B56364, Void);
HOT57(0x08B562E0u, func_08B562E0, Void);
HOT57(0x08B56510u, func_08B56510, Void);
HOT57(0x08B55EF4u, func_08B55EF4, Void);
HOT57(0x08B56128u, func_08B56128, Void);
HOT57(0x08B55FB0u, func_08B55FB0, Void);
HOT57(0x08B566BCu, func_08B566BC, Void);
HOT57(0x08B5A2D4u, func_08B5A2D4, Void);
HOT57(0x08B53844u, func_08B53844, Void);
HOT57(0x08B57D90u, func_08B57D90, Word);
HOT57(0x08B581BCu, func_08B581BC, Void);
HOT57(0x08B58718u, func_08B58718, Void);
HOT57(0x08B5877Cu, func_08B5877C, Void);
HOT57(0x08B54CE4u, func_08B54CE4, Void);
#undef HOT57
