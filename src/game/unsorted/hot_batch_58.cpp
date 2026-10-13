// Hot batch 58: more small functions the four pad routes call thousands of times (call probe as in batch 57, plus the
// shape clusters of tools.shapes): the same families — float vector helpers, pair-returning shims, display-list writers
// of the graphics library, object helpers — and the map lookups of the C++ runtime (std::map lower_bound / find with
// 32-bit keys, 13 instances in two shapes). The graphics library's state block and display list are described in
// hot_batch_57.cpp. Written from the asm; the generated code was only a reading aid.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_58 {
namespace {

constexpr std::uint32_t kGuState = 0x08D6CE00u;
constexpr std::uint32_t kGuFlags = kGuState + 0x6Cu;
constexpr std::uint32_t kGuListPointer = kGuState + 0x5Cu;
constexpr std::uint32_t kGuCommandBlock = 0x08D6D0E0u;
constexpr std::uint32_t kGuBlock2 = 0x08D6DE70u;

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }
void put(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store32(address, value); }
float float_at(psprecomp::Runtime &rt, std::uint32_t address) { return std::bit_cast<float>(rt.memory().load32(address)); }
void put_float(psprecomp::Runtime &rt, std::uint32_t address, float value) { rt.memory().store32(address, std::bit_cast<std::uint32_t>(value)); }

// The FPU's multiply: inf * 0 is the PSP's default NaN (0x7FC00000), not the host's (0xFFC00000).
float psp_mul(float a, float b) {
    if ((std::isinf(a) && b == 0.0f) || (std::isinf(b) && a == 0.0f)) return std::bit_cast<float>(0x7FC00000u);
    return a * b;
}

// Appends `count` words to the display list.
void append(psprecomp::Runtime &rt, std::uint32_t first, std::uint32_t second = 0u, bool two = false) {
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, list, first);
    if (two) put(rt, list + 4u, second);
    put(rt, kGuListPointer, list + (two ? 8u : 4u));
}

} // namespace

// --- Pair shims: (v0, v1) = (a0, a1) ----------------------------------------------------------------------------

void func_08C45E18(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08BAE278(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45F6C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C4600C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45FCC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45EDC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45EC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45F44(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08BAC270(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45E80(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08C45E94(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
void func_08BAC25C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; ctx.gpr[3] = ctx.gpr[5]; }
// func_08C45E40 / func_08C45DE0(a, x): the pair (the bits of x, a).
void func_08C45E40(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[3] = ctx.gpr[4]; ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]); }
void func_08C45DE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[3] = ctx.gpr[4]; ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]); }
// func_0889FD90() -> the pair (1.0f, 1.0f) through the stack.
void func_0889FD90(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(1.0f);
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(1.0f);
}
// func_0887C9D0 / func_0887C9E0() -> 1.0f.
void func_0887C9D0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 1.0f; }
void func_0887C9E0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 1.0f; }

// --- Vectors of floats ------------------------------------------------------------------------------------------

// func_08BAE2FC(d3, a3, b3): d = a + b.
void func_08BAE2FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 3u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, float_at(rt, ctx.gpr[5] + 4u * i) + float_at(rt, ctx.gpr[6] + 4u * i));
}
// func_08BAC428(d3, a3, b3) / func_08C46658(d4, a4, b4) / func_08B4AC30(d2, a2, b2): d = a - b.
void sub_components(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) put_float(rt, ctx.gpr[4] + 4u * i, float_at(rt, ctx.gpr[5] + 4u * i) - float_at(rt, ctx.gpr[6] + 4u * i));
}
void func_08BAC428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sub_components(rt, ctx, 3u); }
void func_08C46658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sub_components(rt, ctx, 4u); }
void func_08B4AC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sub_components(rt, ctx, 2u); }
// func_08B4A918(d4, a4, b4): d = a * b component by component.
void func_08B4A918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 4u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(float_at(rt, ctx.gpr[5] + 4u * i), float_at(rt, ctx.gpr[6] + 4u * i)));
}
// func_08C46718(d3, s*, v3): d[i] = *s * v[i].
void func_08C46718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float s = float_at(rt, ctx.gpr[5]);
    for (std::uint32_t i = 0; i < 3u; ++i) put_float(rt, ctx.gpr[4] + 4u * i, psp_mul(s, float_at(rt, ctx.gpr[6] + 4u * i)));
}
// func_08BAC0D0(d4, p0*, p1*, p2*, p3*): d[i] = *p_i.
void func_08BAC0D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pointers[4] = {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]};
    for (std::uint32_t i = 0; i < 4u; ++i) put(rt, ctx.gpr[4] + 4u * i, word(rt, pointers[i]));
}
// func_08BAE824(d4, s4): d = s.
void func_08BAE824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 4u; ++i) put(rt, ctx.gpr[4] + 4u * i, word(rt, ctx.gpr[5] + 4u * i));
}
// func_08C464B4(d, s): a 3x3 matrix of floats transposed: d[0,3,6,1,4,7,2,5,8] = s[0..8].
void func_08C464B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kTarget[9] = {0, 12, 24, 4, 16, 28, 8, 20, 32};
    for (std::uint32_t i = 0; i < 9u; ++i) put(rt, ctx.gpr[4] + kTarget[i], word(rt, ctx.gpr[5] + 4u * i));
}
// func_08B4A708(v2) -> x * x + y * y.
void func_08B4A708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = float_at(rt, ctx.gpr[4]), y = float_at(rt, ctx.gpr[4] + 4u);
    ctx.fpr[0] = psp_mul(x, x) + psp_mul(y, y);
}
// func_08B4ABEC(a2, b2) -> a.x * b.y - a.y * b.x.
void func_08B4ABEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float ax = float_at(rt, ctx.gpr[4]), ay = float_at(rt, ctx.gpr[4] + 4u), bx = float_at(rt, ctx.gpr[5]), by = float_at(rt, ctx.gpr[5] + 4u);
    ctx.fpr[0] = psp_mul(ax, by) - psp_mul(ay, bx);
}
// func_08BADAA8(p*, v4): **p = v4, one float at a time, reading *p again for each store.
void func_08BADAA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint32_t value = word(rt, ctx.gpr[5] + 4u * i);
        put(rt, word(rt, ctx.gpr[4]) + 4u * i, value);
    }
}
// func_08C3FB5C(a3, b3) -> the dot product, left to right with a 0.0 added first (as the compiler emitted it).
void func_08C3FB5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float sum = psp_mul(float_at(rt, ctx.gpr[4]), float_at(rt, ctx.gpr[5])) + 0.0f;
    sum = sum + psp_mul(float_at(rt, ctx.gpr[4] + 4u), float_at(rt, ctx.gpr[5] + 4u));
    ctx.fpr[0] = sum + psp_mul(float_at(rt, ctx.gpr[4] + 8u), float_at(rt, ctx.gpr[5] + 8u));
}
// func_08BAE7C8(a3, b3*) -> the same, with b behind a pointer.
void func_08BAE7C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t b = word(rt, ctx.gpr[5]);
    float sum = psp_mul(float_at(rt, ctx.gpr[4]), float_at(rt, b)) + 0.0f;
    sum = sum + psp_mul(float_at(rt, ctx.gpr[4] + 4u), float_at(rt, b + 4u));
    ctx.fpr[0] = sum + psp_mul(float_at(rt, ctx.gpr[4] + 8u), float_at(rt, b + 8u));
}

// --- Small objects --------------------------------------------------------------------------------------------

// func_0886B0F8(this) -> the float at +0x148 of the object at +4 when its byte +0x14C is set, else 0.0.
void func_0886B0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[2] = rt.memory().load8(object + 332u);
    ctx.fpr[0] = ctx.gpr[2] != 0u ? float_at(rt, object + 328u) : 0.0f;
}
// func_0886B120(this) -> whether the word +0x154 of the object at +4 is nonzero.
void func_0886B120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, word(rt, ctx.gpr[4] + 4u) + 340u) != 0u ? 1u : 0u;
}
// func_0886B060(this, i) -> the word 0x58 + 4 * i of the object at +4.
void func_0886B060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, word(rt, ctx.gpr[4] + 4u) + ctx.gpr[5] * 4u + 88u);
}
// func_08AF12EC(this, x, y, z): the floats +0x314..+0x31C of the object at +4 (read again for each store).
void func_08AF12EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object = word(rt, ctx.gpr[4] + 4u);
    put_float(rt, object + 788u, ctx.fpr[12]);
    object = word(rt, ctx.gpr[4] + 4u);
    put_float(rt, object + 792u, ctx.fpr[13]);
    object = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[2] = object;
    put_float(rt, object + 796u, ctx.fpr[14]);
}
// func_08B9AB4C(this): seven zero words from +0. func_08B8ED84(this): four. func_08B487AC(this): the word +0 and the byte +4.
void func_08B9AB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 24u, 0u);
    for (std::uint32_t at = 0; at <= 20u; at += 4u) put(rt, ctx.gpr[4] + at, 0u);
}
void func_08B8ED84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 12u, 0u);
    for (std::uint32_t at = 0; at <= 8u; at += 4u) put(rt, ctx.gpr[4] + at, 0u);
}
void func_08B487AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 4u, 0u);
    put(rt, ctx.gpr[4], 0u);
}
// func_08B51740(this, a, b, c, d): this[1] = c... : +0xC = d (fifth argument), +0x10 = a, +0x4 = b, +0x8 = c.
void func_08B51740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 12u, ctx.gpr[8]);
    put(rt, ctx.gpr[4] + 16u, ctx.gpr[5]);
    put(rt, ctx.gpr[4] + 4u, ctx.gpr[6]);
    put(rt, ctx.gpr[4] + 8u, ctx.gpr[7]);
}
// func_08BAA464(this, a, b, c, p*) -> this: this[0] = a, [1] = b, [2] = c, [3] = p[0], [4] = p[1] (p is the fifth argument).
void func_08BAA464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[8];
    const std::uint32_t third = word(rt, p), fourth = word(rt, p + 4u);
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 4u, ctx.gpr[6]);
    put(rt, self + 8u, ctx.gpr[7]);
    put(rt, self + 16u, fourth);
    put(rt, self + 12u, third);
    ctx.gpr[2] = self;
}
// func_08977558(this, i): the halfword +0x156 + 2 * i = 1.
void func_08977558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store16(ctx.gpr[4] + ctx.gpr[5] * 2u + 342u, 1u);
    ctx.gpr[2] = 1u;
}
// func_08B64494(this): the byte +1 decremented, the byte +0 = 1.
void func_08B64494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 1u, static_cast<std::uint8_t>(rt.memory().load8(ctx.gpr[4] + 1u) - 1u));
    rt.memory().store8(ctx.gpr[4], 1u);
    ctx.gpr[2] = 1u;
}
// func_08C53CDC(delta, p*) / func_08C53CC4: *p += delta unless it is 0 (v0 = the old word).
void add_unless_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = word(rt, ctx.gpr[5]);
    ctx.gpr[2] = old;
    ctx.gpr[3] = old + ctx.gpr[4];
    if (old != 0u) put(rt, ctx.gpr[5], old + ctx.gpr[4]);
}
void func_08C53CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_unless_zero(rt, ctx); }
void func_08C53CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_unless_zero(rt, ctx); }
// Constructors that only set a vtable: func_08BAEF88 (0x08CBF488), func_08AF20E8 (0x08CBFE70), func_0887E1E0 (0x08CBF908, then
// +8 and +4 cleared).
void func_08BAEF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x08CBF488u; put(rt, ctx.gpr[4], ctx.gpr[2]); }
void func_08AF20E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x08CBFE70u; put(rt, ctx.gpr[4], ctx.gpr[2]); }
void func_0887E1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0x08CBF908u;
    put(rt, ctx.gpr[4], ctx.gpr[2]);
    put(rt, ctx.gpr[4] + 8u, 0u);
    put(rt, ctx.gpr[4] + 4u, 0u);
}
// func_08BDB2F4(this, a, b): vtable 0x08CD70E0, +4 and its +4, +8 cleared, the bytes +0x14 and +0x15 = a, b.
void func_08BDB2F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = 0x08CD70E0u;
    ctx.gpr[3] = self + 4u;
    put(rt, self + 12u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self, ctx.gpr[2]);
    rt.memory().store8(self + 20u, static_cast<std::uint8_t>(ctx.gpr[5]));
    rt.memory().store8(self + 21u, static_cast<std::uint8_t>(ctx.gpr[6]));
}
// func_0887E1C8(this, p): this[1] = p; this[2] = p[7] when p is set.
void func_0887E1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 4u, ctx.gpr[5]);
    if (ctx.gpr[5] != 0u) {
        ctx.gpr[2] = word(rt, ctx.gpr[5] + 28u);
        put(rt, ctx.gpr[4] + 8u, ctx.gpr[2]);
    }
}
// func_08B49634(i) -> the word 8 * i bytes into the table at 0x08D69528 for i < 14, else the default 0x08CB574C.
void func_08B49634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] < 14u ? word(rt, 0x08D69528u + ctx.gpr[4] * 8u) : 0x08CB574Cu;
}
// func_0887BD94(i) -> (u16 table[i] + the u16 at 0x08CE2F36) & 0xFFFF, the table at 0x08CE2F10.
void func_0887BD94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (rt.memory().load16(0x08CE2F10u + ctx.gpr[4] * 2u) + rt.memory().load16(0x08CE2F36u)) & 0xFFFFu;
}
// func_08B7E8A4(i) -> i + 32 when bit 0 of the byte at 0x08C96BD9 + i is set, else i.
void func_08B7E8A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (rt.memory().load8(0x08C96BD9u + ctx.gpr[4]) & 1u) != 0u ? ctx.gpr[4] + 32u : ctx.gpr[4];
}
// func_08BD2D74(flag, a, b) -> a when the low byte of flag is set, else b (or a when b is 0).
void func_08BD2D74(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    std::uint32_t fallback = ctx.gpr[6];
    if (fallback == 0u) fallback = ctx.gpr[5];
    ctx.gpr[2] = (ctx.gpr[4] & 0xFFu) != 0u ? ctx.gpr[5] : fallback;
}

// --- std::map lookups with 32-bit keys: nodes {left +8, right +12, key +16}, the header at tree + 4, root at +8 ---

std::uint32_t lower_bound(psprecomp::Runtime &rt, std::uint32_t tree, std::uint32_t key, bool is_signed) {
    std::uint32_t result = tree + 4u;
    for (std::uint32_t node = word(rt, tree + 8u); node != 0u;) {
        const std::uint32_t node_key = word(rt, node + 16u);
        const bool less = is_signed ? static_cast<std::int32_t>(node_key) < static_cast<std::int32_t>(key) : node_key < key;
        if (less) {
            node = word(rt, node + 12u);
        } else {
            result = node;
            node = word(rt, node + 8u);
        }
    }
    return result;
}
// lower_bound(*key) for func_08BFE7D0, func_08C25AF4, func_08C1D250, func_08BF9104, func_08C009CC (unsigned keys).
void tree_lower_bound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = lower_bound(rt, ctx.gpr[4], word(rt, ctx.gpr[5]), false);
}
// find(*key): the lower bound when the key is not less than its node's, else end().
std::uint32_t find(psprecomp::Runtime &rt, std::uint32_t tree, std::uint32_t key_address, bool is_signed) {
    const std::uint32_t end = tree + 4u;
    const std::uint32_t key = word(rt, key_address);
    const std::uint32_t candidate = lower_bound(rt, tree, key, is_signed);
    if (candidate == end) return end;
    const std::uint32_t node_key = word(rt, candidate + 16u);
    const bool less = is_signed ? static_cast<std::int32_t>(key) < static_cast<std::int32_t>(node_key) : key < node_key;
    return less ? end : candidate;
}
// func_08C25C78, func_08C0080C, func_08C27D74, func_08C1D288, func_08C0EDF0, func_08C27D1C, func_08C1D1AC (unsigned keys).
void tree_find(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = find(rt, ctx.gpr[4], ctx.gpr[5], false); }
// func_08C1ECC0 (signed keys).
void tree_find_signed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = find(rt, ctx.gpr[4], ctx.gpr[5], true); }

// --- The graphics library (state at 0x08D6CE00; see hot_batch_57.cpp) ----------------------------------------------

// func_08B5894C(x): float +0xCC of the state = x, then func_08B58B5C(state) (tail call).
void func_08B5894C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put_float(rt, kGuState + 204u, ctx.fpr[12]);
    ctx.gpr[2] = kGuState;
    tail_call(ctx, 0x08B58B5Cu, {kGuState});
}
// func_08B587D0(x): state +0x78 = x; bit 8 of the flags.
void func_08B587D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuState + 120u, ctx.gpr[4]);
    put(rt, kGuFlags, flags | 256u);
    ctx.gpr[2] = flags | 256u;
}
// func_08B58444(a, b): state +0xF8 = a, +0xFC = b; bit 11 of the flags.
void func_08B58444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuState + 248u, ctx.gpr[4]);
    put(rt, kGuFlags, flags | 2048u);
    put(rt, kGuState + 252u, ctx.gpr[5]);
    ctx.gpr[2] = flags | 2048u;
}
// func_08B583D0(a, b, c): appends 0xDC000000 | c << 16 | b << 8 | a & 7.
void func_08B583D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    append(rt, 0xDC000000u | ((ctx.gpr[6] & 0xFFu) << 16) | ((ctx.gpr[5] & 0xFFu) << 8) | (ctx.gpr[4] & 7u));
}
// func_08B58388(a, b, c): appends 0xDD000000 | f(c) << 16 | f(b) << 8 | f(a), f(x) = x ? (x >> 4) & 7 : 1.
void func_08B58388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto field = [](std::uint32_t x) { return x == 0u ? 1u : (x >> 4) & 7u; };
    append(rt, 0xDD000000u | (field(ctx.gpr[6]) << 16) | (field(ctx.gpr[5]) << 8) | field(ctx.gpr[4]));
}
// func_08B563C0(): bit 1 of the flags, then func_08B56384(block) (tail call).
void func_08B563C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = kGuState;
    put(rt, kGuFlags, word(rt, kGuFlags) | 2u);
    tail_call(ctx, 0x08B56384u, {kGuCommandBlock});
}
// func_08B57744(): func_08B57714(state) (tail call).
void func_08B57744(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B57714u, {kGuState}); }
// func_08B59ADC(x): func_08B59F6C(block2, x); func_08B59AC8(n): func_08B5A270(block2, (n + 8191) >> 13).
void func_08B59ADC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B59F6Cu, {kGuBlock2, ctx.gpr[4]}); }
void func_08B59AC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B5A270u, {kGuBlock2, static_cast<std::uint32_t>(static_cast<std::int32_t>(ctx.gpr[4] + 8191u) >> 13)});
}
// func_08B57750(kind, bit): appends 0x5E000000 | bit & 1 unless kind is 3 (v0 = 3, else the old list pointer).
void func_08B57750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 3u;
    if (ctx.gpr[4] == 3u) return;
    ctx.gpr[2] = word(rt, kGuListPointer);
    append(rt, 0x5E000000u | (ctx.gpr[5] & 1u));
}
// func_08B57278(a, b): state +0x23C = a code from (a, b), bit 9 of the flags.
void func_08B57278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    std::uint32_t code;
    if (a == 2u) code = 2u;
    else if (b == 1u) code = 2u;
    else if (b == 0u) code = 1u;
    else if (b == 2u) code = 3u;
    else if (b == 4u) code = 4u;
    else code = 0u;
    const std::uint32_t flags = word(rt, kGuFlags) | 512u;
    put(rt, kGuState + 572u, code);
    put(rt, kGuFlags, flags);
    ctx.gpr[2] = flags;
}
// func_08B540E0(x): the float at 0x08CE0DD0 = x * K, K the float at 0x08CB6190.
void func_08B540E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[12] = psp_mul(ctx.fpr[12], float_at(rt, 0x08CB6190u));
    put_float(rt, 0x08CE0DD0u, ctx.fpr[12]);
}
// func_08B5C274() -> 0x08C95C4C.
void func_08B5C274(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x08C95C4Cu; }
// func_08B550E0(n): state +0x60 -= (n + 3) & ~3.
void func_08B550E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, kGuState + 96u, word(rt, kGuState + 96u) - ((ctx.gpr[4] + 3u) & ~3u)); }
// func_08B555F8(x): state +0x2D8 = x; appends 0x10000000 | (x >> 24) << 16 and 0x01000000 | x & 0xFFFFFF.
void func_08B555F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    put(rt, kGuState + 728u, x);
    append(rt, 0x10000000u | (((x >> 24) & 0xFFu) << 16), 0x01000000u | (x & 0x00FFFFFFu), true);
}
// func_08B558F8(a, b, c): appends (a + 184) << 24 | c << 8 | b.
void func_08B558F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    append(rt, ((ctx.gpr[4] + 184u) << 24) | ((ctx.gpr[6] & 0xFFu) << 8) | (ctx.gpr[5] & 0xFFu));
}
// func_08B55820(x, y) / func_08B557E0(x, y): append two words, the top bytes 0x48 / 0x49 and 0x4A / 0x4B, each with the float's bits >> 8.
void func_08B55820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    append(rt, 0x48000000u | (std::bit_cast<std::uint32_t>(ctx.fpr[12]) >> 8), 0x49000000u | (std::bit_cast<std::uint32_t>(ctx.fpr[13]) >> 8), true);
}
void func_08B557E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    append(rt, 0x4A000000u | (std::bit_cast<std::uint32_t>(ctx.fpr[12]) >> 8), 0x4B000000u | (std::bit_cast<std::uint32_t>(ctx.fpr[13]) >> 8), true);
}
// func_08B583FC(r, g, b): state +0x108 = b << 16 | g << 8 | r; bit 11 of the flags unless +0xFC is 778.
void func_08B583FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t colour = ((ctx.gpr[6] & 0xFFu) << 16) | ((ctx.gpr[5] & 0xFFu) << 8) | (ctx.gpr[4] & 0xFFu);
    const std::uint32_t mode = word(rt, kGuState + 252u);
    ctx.gpr[2] = 778u;
    put(rt, kGuState + 264u, colour);
    if (mode != 778u) {
        ctx.gpr[2] = word(rt, kGuFlags) | 2048u;
        put(rt, kGuFlags, ctx.gpr[2]);
    }
}

} // namespace hot_batch_58
} // namespace gt::game

#define HOT58(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_58::name, gt::ReturnKind::kind)
HOT58(0x08C45E18u, func_08C45E18, DoubleWord);
HOT58(0x08BAE278u, func_08BAE278, DoubleWord);
HOT58(0x08C45F6Cu, func_08C45F6C, DoubleWord);
HOT58(0x08C4600Cu, func_08C4600C, DoubleWord);
HOT58(0x08C45FCCu, func_08C45FCC, DoubleWord);
HOT58(0x08C45EDCu, func_08C45EDC, DoubleWord);
HOT58(0x08C45EC8u, func_08C45EC8, DoubleWord);
HOT58(0x08C45F44u, func_08C45F44, DoubleWord);
HOT58(0x08BAC270u, func_08BAC270, DoubleWord);
HOT58(0x08C45E80u, func_08C45E80, DoubleWord);
HOT58(0x08C45E94u, func_08C45E94, DoubleWord);
HOT58(0x08BAC25Cu, func_08BAC25C, DoubleWord);
HOT58(0x08C45E40u, func_08C45E40, DoubleWord);
HOT58(0x08C45DE0u, func_08C45DE0, DoubleWord);
HOT58(0x0889FD90u, func_0889FD90, DoubleWord);
HOT58(0x0887C9D0u, func_0887C9D0, Float);
HOT58(0x0887C9E0u, func_0887C9E0, Float);
HOT58(0x08BAE2FCu, func_08BAE2FC, Void);
HOT58(0x08BAC428u, func_08BAC428, Void);
HOT58(0x08C46658u, func_08C46658, Void);
HOT58(0x08B4AC30u, func_08B4AC30, Void);
HOT58(0x08B4A918u, func_08B4A918, Void);
HOT58(0x08C46718u, func_08C46718, Void);
HOT58(0x08BAC0D0u, func_08BAC0D0, Void);
HOT58(0x08BAE824u, func_08BAE824, Void);
HOT58(0x08C464B4u, func_08C464B4, Void);
HOT58(0x08B4A708u, func_08B4A708, Float);
HOT58(0x08B4ABECu, func_08B4ABEC, Float);
HOT58(0x08BADAA8u, func_08BADAA8, Void);
HOT58(0x08C3FB5Cu, func_08C3FB5C, Float);
HOT58(0x08BAE7C8u, func_08BAE7C8, Float);
HOT58(0x0886B0F8u, func_0886B0F8, Float);
HOT58(0x0886B120u, func_0886B120, Word);
HOT58(0x0886B060u, func_0886B060, Word);
HOT58(0x08AF12ECu, func_08AF12EC, Word);
HOT58(0x08B9AB4Cu, func_08B9AB4C, Void);
HOT58(0x08B8ED84u, func_08B8ED84, Void);
HOT58(0x08B487ACu, func_08B487AC, Void);
HOT58(0x08B51740u, func_08B51740, Void);
HOT58(0x08BAA464u, func_08BAA464, Word);
HOT58(0x08977558u, func_08977558, Word);
HOT58(0x08B64494u, func_08B64494, Word);
HOT58(0x08C53CDCu, func_08C53CDC, Word);
HOT58(0x08C53CC4u, func_08C53CC4, Word);
HOT58(0x08BAEF88u, func_08BAEF88, Word);
HOT58(0x08AF20E8u, func_08AF20E8, Word);
HOT58(0x0887E1E0u, func_0887E1E0, Word);
HOT58(0x08BDB2F4u, func_08BDB2F4, Word);
HOT58(0x0887E1C8u, func_0887E1C8, Word);
HOT58(0x08B49634u, func_08B49634, Word);
HOT58(0x0887BD94u, func_0887BD94, Word);
HOT58(0x08B7E8A4u, func_08B7E8A4, Word);
HOT58(0x08BD2D74u, func_08BD2D74, Word);
HOT58(0x08B5894Cu, func_08B5894C, Word);
HOT58(0x08B587D0u, func_08B587D0, Word);
HOT58(0x08B58444u, func_08B58444, Word);
HOT58(0x08B583D0u, func_08B583D0, Void);
HOT58(0x08B58388u, func_08B58388, Void);
HOT58(0x08B563C0u, func_08B563C0, Word);
HOT58(0x08B57744u, func_08B57744, Void);
HOT58(0x08B59ADCu, func_08B59ADC, Void);
HOT58(0x08B59AC8u, func_08B59AC8, Void);
HOT58(0x08B57750u, func_08B57750, Word);
HOT58(0x08B57278u, func_08B57278, Word);
HOT58(0x08B540E0u, func_08B540E0, Void);
HOT58(0x08B5C274u, func_08B5C274, Word);
HOT58(0x08B550E0u, func_08B550E0, Void);
HOT58(0x08B555F8u, func_08B555F8, Void);
HOT58(0x08B558F8u, func_08B558F8, Void);
HOT58(0x08B55820u, func_08B55820, Void);
HOT58(0x08B557E0u, func_08B557E0, Void);
HOT58(0x08B583FCu, func_08B583FC, Word);
HOT58(0x08BFE7D0u, tree_lower_bound, Word);
HOT58(0x08C25AF4u, tree_lower_bound, Word);
HOT58(0x08C1D250u, tree_lower_bound, Word);
HOT58(0x08BF9104u, tree_lower_bound, Word);
HOT58(0x08C009CCu, tree_lower_bound, Word);
HOT58(0x08C25C78u, tree_find, Word);
HOT58(0x08C0080Cu, tree_find, Word);
HOT58(0x08C27D74u, tree_find, Word);
HOT58(0x08C1D288u, tree_find, Word);
HOT58(0x08C0EDF0u, tree_find, Word);
HOT58(0x08C27D1Cu, tree_find, Word);
HOT58(0x08C1D1ACu, tree_find, Word);
HOT58(0x08C1ECC0u, tree_find_signed, Word);
#undef HOT58
