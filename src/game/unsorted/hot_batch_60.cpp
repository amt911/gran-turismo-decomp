// Hot batch 60: functions with branches or loops that the four pad routes reach (call probe as in batches 57-59). Because
// they branch, the gate only proves the paths the compared cases take: each function's comment lists the branches, and a
// path no route takes is marked "not exercised" and was compared against the asm by hand. Their classes aren't known yet,
// so they keep their addresses as names. Written from the asm; the generated code was only a reading aid.

#define GT_HOT_COVERAGE 1
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

namespace gt::game {
namespace hot_batch_60 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kEmptyStringRep = 0x08D74FD0u;  // the shared empty string's representation

// 0x08C... helper results used below
std::uint32_t object_flags_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t &object) {
    GuestFrame frame(ctx, 0x10u);
    object = call_guest(rt, ctx, 0x088A9A0Cu, {ctx.gpr[4]});
    return object != 0u ? rt.memory().load8(object + 47u) : 0u;
}

} // namespace

// --- Flag tests on the object func_088A9A0C(this) answers (byte +0x2F = flags, +0x2C = a flag byte); a null object counts as "true" ---

// func_088A9C90(this) -> the flags byte of the object, 0 without one.
void func_088A9C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    GT_COVER(object != 0u ? "088A9C90:object" : "088A9C90:null");
    ctx.gpr[2] = flags;
}
// func_088A9A50(this) -> 1 without an object or with its byte +0x2C clear, else 0.
void func_088A9A50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    { GuestFrame frame(ctx, 0x10u); object = call_guest(rt, ctx, 0x088A9A0Cu, {ctx.gpr[4]}); }
    const bool answer = object == 0u || rt.memory().load8(object + 44u) == 0u;
    GT_COVER(object == 0u ? "088A9A50:null" : answer ? "088A9A50:clear" : "088A9A50:set");
    ctx.gpr[2] = answer ? 1u : 0u;
}
// func_088A9AB4(this) -> 1 without an object or with bit 1 of its flags set, else 0.
void func_088A9AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    const bool answer = object == 0u || (flags & 2u) != 0u;
    GT_COVER(object == 0u ? "088A9AB4:null" : answer ? "088A9AB4:bit" : "088A9AB4:nobit");
    ctx.gpr[2] = answer ? 1u : 0u;
}
// func_088A9AF0 / 088A9CBC / 088A9CFC(this) -> 1 without an object, else whether bits 3-4 of its flags equal 3 / 1 / 2.
void flags_field_equals(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t expected, const char *tag) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    const bool answer = object == 0u || (flags & 24u) == expected;
    GT_COVER(object == 0u ? (std::string(tag) + ":null").c_str() : answer ? (std::string(tag) + ":match").c_str() : (std::string(tag) + ":other").c_str());
    ctx.gpr[2] = answer ? 1u : 0u;
}
void func_088A9AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flags_field_equals(rt, ctx, 24u, "088A9AF0"); }
void func_088A9CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flags_field_equals(rt, ctx, 8u, "088A9CBC"); }
void func_088A9CFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flags_field_equals(rt, ctx, 16u, "088A9CFC"); }

// --- Others ---------------------------------------------------------------------------------------------------------

// func_08C3DCC4(pool, n) -> the address of slot n of a pool, or 0: base = (pool[1] + 3) & ~3, stride (pool[3] + 3) & ~3,
// answer = base + n * stride when it is below pool[1] + pool[2], else 0; a negative n answers 0.
void func_08C3DCC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4];
    const auto n = static_cast<std::int32_t>(ctx.gpr[5]);
    if (n < 0) { GT_COVER("08C3DCC4:negative"); ctx.gpr[2] = 0u; return; }
    const std::uint32_t start = word(rt, pool + 4u), size = word(rt, pool + 8u), stride = (word(rt, pool + 12u) + 3u) & ~3u;
    const std::uint32_t slot = ((start + 3u) & ~3u) + static_cast<std::uint32_t>(n) * stride;
    const bool inside = slot < start + size;
    GT_COVER(inside ? "08C3DCC4:inside" : "08C3DCC4:outside");
    ctx.gpr[2] = inside ? slot : 0u;
}

// func_08B796C4() -> the current thread record: k0's word +4, else the global at 0x08CE0E60 (when k0 is 0 or its word is 0).
void func_08B796C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t k0 = ctx.gpr[26];
    std::uint32_t thread = 0u;
    if (k0 != 0u) thread = word(rt, k0 + 4u);
    GT_COVER(k0 == 0u ? "08B796C4:no-k0" : thread == 0u ? "08B796C4:empty" : "08B796C4:thread");
    ctx.gpr[2] = thread != 0u ? thread : word(rt, 0x08CE0E60u);
}

// func_08B85EB8(out, s, n): out = func_08B85E00(s, end, n, s), end = s + func_08B35F60(s, -1) for a non-null s else -1.
void func_08B85EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], s = ctx.gpr[5], n = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t end = 0xFFFFFFFFu;
    if (s != 0u) {
        GT_COVER("08B85EB8:string");
        end = s + call_guest(rt, ctx, 0x08B35F60u, {s, 0xFFFFFFFFu});
    } else {
        GT_COVER("08B85EB8:null");
    }
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B85E00u, {s, end, n, s});
    put(rt, out, ctx.gpr[2]);
}

// func_08B88F44(out, p): out = the string *p shared (copy on write): a representation with a negative count (word +8 of
// data - 12) is copied by func_08B854DC; the empty string is taken as it is; any other gets its count (data - 4) incremented.
void func_08B88F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t data = word(rt, ctx.gpr[5]);
    const std::uint32_t rep = data - 12u;
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(word(rt, rep + 8u)) < 0) {
        GT_COVER("08B88F44:copy");
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B854DCu, {rep, frame.at(0u), 0u});
    } else {
        if (rep == kEmptyStringRep) {
            GT_COVER("08B88F44:empty");
        } else {
            GT_COVER("08B88F44:share");
            (void)call_guest(rt, ctx, 0x08B6085Cu, {data - 4u, 1u});
        }
        ctx.gpr[2] = data;
    }
    put(rt, out, ctx.gpr[2]);
}

// func_08C1CCD0(a, key, p): whether `key` is the end iterator or the halfword *p is not below the word at the node func_08C1CC84(a)
// answers (a lower-bound style test): 1 when key == end, else 0 if (u16)*p < *node, else 1.
void func_08C1CCD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5], p = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_guest(rt, ctx, 0x08C1CC84u, {ctx.gpr[4]});
    if (key == node) { GT_COVER("08C1CCD0:end"); ctx.gpr[2] = 1u; return; }
    const bool below = static_cast<std::int32_t>(rt.memory().load16(p)) < static_cast<std::int32_t>(word(rt, node));
    GT_COVER(below ? "08C1CCD0:below" : "08C1CCD0:not-below");
    ctx.gpr[2] = below ? 0u : 1u;
}

// func_08C24F04(first, last): func_08A69D68(p) for each word-sized element p in [first, last).
void func_08C24F04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (first == last) { GT_COVER("08C24F04:empty"); return; }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t p = first;
    do {
        GT_COVER("08C24F04:element");
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A69D68u, {p});
        p += 4u;
    } while (last != p);
}

// func_08BEDDC4(list): destroys a circular list of {next, prev, holder at +8} nodes (12 bytes) other than the head `list`:
// each node's holder is destroyed (func_08A5B9EC(node + 8)) and the node freed (func_08A5BC54(node, 12, 4)).
void func_08BEDDC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4];
    std::uint32_t node = word(rt, head);
    if (node == head) { GT_COVER("08BEDDC4:empty"); return; }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t next = word(rt, node);
    for (;;) {
        GT_COVER("08BEDDC4:node");
        (void)call_guest(rt, ctx, 0x08A5B9ECu, {node + 8u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BC54u, {node, 12u, 4u});
        node = next;
        if (head == next) break;
        next = word(rt, node);
    }
}

// func_08B54760(table, a, b): for entries 0 to 7 (8 bytes each) the bytes +78 and +79 take the bit lengths of a - 1 and b - 1
// (32 - clz), each decreasing by one per entry down to 0. Leaves the last (length - 1) values in v0 and v1.
void func_08B54760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t entry = ctx.gpr[4];
    auto first = static_cast<std::int32_t>(32 - std::countl_zero(ctx.gpr[5] - 1u));
    auto second = static_cast<std::int32_t>(32 - std::countl_zero(ctx.gpr[6] - 1u));
    for (int i = 0; i < 8; ++i) {
        ctx.gpr[2] = static_cast<std::uint32_t>(first - 1);
        ctx.gpr[3] = static_cast<std::uint32_t>(second - 1);
        rt.memory().store8(entry + 78u, static_cast<std::uint8_t>(first));
        rt.memory().store8(entry + 79u, static_cast<std::uint8_t>(second));
        first = first - 1 > 0 ? first - 1 : 0;
        second = second - 1 > 0 ? second - 1 : 0;
        entry += 8u;
    }
    GT_COVER("08B54760:run");
}

} // namespace hot_batch_60
} // namespace gt::game

#define HOT60(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_60::name, gt::ReturnKind::kind)
HOT60(0x088A9C90u, func_088A9C90, Word);
HOT60(0x088A9A50u, func_088A9A50, Word);
HOT60(0x088A9AB4u, func_088A9AB4, Word);
HOT60(0x088A9AF0u, func_088A9AF0, Word);
HOT60(0x088A9CBCu, func_088A9CBC, Word);
HOT60(0x088A9CFCu, func_088A9CFC, Word);
HOT60(0x08C3DCC4u, func_08C3DCC4, Word);
HOT60(0x08B796C4u, func_08B796C4, Word);
HOT60(0x08B85EB8u, func_08B85EB8, Word);
HOT60(0x08B88F44u, func_08B88F44, Word);
HOT60(0x08C1CCD0u, func_08C1CCD0, Word);
HOT60(0x08C24F04u, func_08C24F04, Word);
HOT60(0x08BEDDC4u, func_08BEDDC4, Word);
HOT60(0x08B54760u, func_08B54760, DoubleWord);
#undef HOT60
