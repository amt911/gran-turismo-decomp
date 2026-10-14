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

// --- Flag tests on the object func_088A9A0C(this) answers (byte +0x2F = flags, +0x2C = a flag byte); a null object answers 0 ---

// func_088A9C90(this) -> the flags byte of the object, 0 without one.
void func_088A9C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    GT_COVER(object != 0u ? "088A9C90:object" : "088A9C90:null");
    ctx.gpr[2] = flags;
}
// func_088A9A50(this) -> 1 with an object whose byte +0x2C is clear, else 0 (no object: 0).
void func_088A9A50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    { GuestFrame frame(ctx, 0x10u); object = call_guest(rt, ctx, 0x088A9A0Cu, {ctx.gpr[4]}); }
    const bool answer = object != 0u && rt.memory().load8(object + 44u) == 0u;
    GT_COVER(object == 0u ? "088A9A50:null" : answer ? "088A9A50:clear" : "088A9A50:set");
    ctx.gpr[2] = answer ? 1u : 0u;
}
// func_088A9AB4(this) -> 1 with an object whose flags have bit 1 set, else 0.
void func_088A9AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    const bool answer = object != 0u && (flags & 2u) != 0u;
    GT_COVER(object == 0u ? "088A9AB4:null" : answer ? "088A9AB4:bit" : "088A9AB4:nobit");
    ctx.gpr[2] = answer ? 1u : 0u;
}
// func_088A9AF0 / 088A9CBC / 088A9CFC(this) -> 1 with an object whose flags bits 3-4 equal 3 / 1 / 2, else 0.
void flags_field_equals(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t expected, const char *tag) {
    std::uint32_t object;
    const std::uint32_t flags = object_flags_byte(rt, ctx, object);
    const bool answer = object != 0u && (flags & 24u) == expected;
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
// answers 1 when key == end, else 0 if (u16)*p < *node, else 1.
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

// func_08BD2DC4(a, n): n when it is not negative, else func_08AF2954(a, n) (tail call, v0 holds n meanwhile).
void func_08BD2DC4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[5];
    if (static_cast<std::int32_t>(ctx.gpr[5]) >= 0) { GT_COVER("08BD2DC4:non-negative"); return; }
    GT_COVER("08BD2DC4:negative");
    tail_call(ctx, 0x08AF2954u, {});
}

// func_0886B444(this, i) -> bit i of the mask at the word +0x32C of the object at +4, for 0 <= i < 14; else 0.
void func_0886B444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 4u);
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i < 0) { GT_COVER("0886B444:negative"); ctx.gpr[2] = 0u; return; }
    if (i >= 14) { GT_COVER("0886B444:high"); ctx.gpr[2] = 0u; return; }
    GT_COVER("0886B444:in-range");
    const std::uint32_t mask = word(rt, object + 812u + static_cast<std::uint32_t>((i >> 5) << 2));
    ctx.gpr[2] = (static_cast<std::int32_t>(mask) >> (i & 31)) & 1;
}

// func_08B856B0(first, last): func_08A5B9EC(p) for each word p in [first, last) (nothing when first == last).
void func_08B856B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (first == last) { GT_COVER("08B856B0:empty"); return; }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t p = first;
    do {
        GT_COVER("08B856B0:element");
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B9ECu, {p});
        p += 4u;
    } while (last != p);
}

// func_08C2AE8C(first, last): for each 12-byte element [first, last): func_08A5B9EC(p + 8), then func_08A69D68(p).
void func_08C2AE8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    if (first == last) { GT_COVER("08C2AE8C:empty"); return; }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t p = first;
    do {
        GT_COVER("08C2AE8C:element");
        (void)call_guest(rt, ctx, 0x08A5B9ECu, {p + 8u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A69D68u, {p});
        p += 12u;
    } while (last != p);
}

// func_08955428(this) -> 1 when the halfword +0x2A6 of the object at +12 is below the signed halfword +0x58E of the object at +8, or
// when that object's byte +0x579 is set; else 0. (Branch "below": not exercised by the routes, checked against the asm by hand.)
void func_08955428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t other = word(rt, ctx.gpr[4] + 12u), owner = word(rt, ctx.gpr[4] + 8u);
    const auto left = static_cast<std::int32_t>(rt.memory().load16(other + 678u));
    const auto right = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(owner + 1422u)));
    std::uint32_t answer = 1u;
    if (left < right) {
        GT_COVER("08955428:below");
    } else if (rt.memory().load8(owner + 1401u) == 0u) {
        GT_COVER("08955428:unset");
        answer = 0u;
    } else {
        GT_COVER("08955428:set");
    }
    ctx.gpr[2] = answer;
}

// func_088A922C(this) -> whether the object at +12 exists and func_08955428(it) holds.
void func_088A922C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 12u);
    std::uint32_t answer = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        answer = call_guest(rt, ctx, 0x08955428u, {object}) != 0u ? 1u : 0u;
        GT_COVER(answer != 0u ? "088A922C:true" : "088A922C:false");
    } else {
        GT_COVER("088A922C:no-object");
    }
    ctx.gpr[2] = answer;
}

// func_08B64654(p): func_08B35254(p) (the free) unless p is null (tail call).
void func_08B64654(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == 0u) { GT_COVER("08B64654:null"); return; }
    GT_COVER("08B64654:free");
    tail_call(ctx, 0x08B35254u, {});
}

// func_08BB16EC(begin, end, p): every byte of [begin, end) = the signed byte *p (nothing when begin == end).
void func_08BB16EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t begin = ctx.gpr[4];
    const std::uint32_t end = ctx.gpr[5];
    const std::uint32_t value = word(rt, ctx.gpr[6]);
    ctx.gpr[6] = value;
    if (begin == end) { GT_COVER("08BB16EC:empty"); return; }
    const auto byte = static_cast<std::uint8_t>(value);
    ctx.gpr[6] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(byte)));
    do {
        GT_COVER("08BB16EC:byte");
        rt.memory().store8(begin, byte);
        ++begin;
    } while (begin != end);
    ctx.gpr[4] = begin;
}

// func_0887F144(this) -> whether the byte +0x20 is set and func_08881FF0(the word +0x74) answers nonzero.
void func_0887F144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t answer = 0u;
    if (rt.memory().load8(ctx.gpr[4] + 32u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        answer = call_guest(rt, ctx, 0x08881FF0u, {word(rt, ctx.gpr[4] + 116u)}) != 0u ? 1u : 0u;
        GT_COVER(answer != 0u ? "0887F144:true" : "0887F144:false");
    } else {
        GT_COVER("0887F144:unset");
    }
    ctx.gpr[2] = answer;
}

// func_08AB856C(this) -> func_08AB8430(this, 0), or the constant 0x08C90518 when that answers 0.
void func_08AB856C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08AB8430u, {ctx.gpr[4], 0u});
    GT_COVER(result != 0u ? "08AB856C:found" : "08AB856C:default");
    ctx.gpr[2] = result != 0u ? result : 0x08C90518u;
}

// func_08B4BAF8(a, b): func_08B4BAB0(a, b, b) (tail call; the code that follows in memory at 0x08B4BB00 is another function).
void func_08B4BAF8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = ctx.gpr[5];
    tail_call(ctx, 0x08B4BAB0u, {});
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
HOT60(0x08BD2DC4u, func_08BD2DC4, Word);
HOT60(0x0886B444u, func_0886B444, Word);
HOT60(0x08B856B0u, func_08B856B0, Word);
HOT60(0x08C2AE8Cu, func_08C2AE8C, Word);
HOT60(0x08955428u, func_08955428, Word);
HOT60(0x088A922Cu, func_088A922C, Word);
HOT60(0x08B64654u, func_08B64654, Word);
HOT60(0x08BB16ECu, func_08BB16EC, Word);
HOT60(0x0887F144u, func_0887F144, Word);
HOT60(0x08AB856Cu, func_08AB856C, Word);
HOT60(0x08B4BAF8u, func_08B4BAF8, Void);
HOT60(0x088A988Cu, func_088A988C, Float);
HOT60(0x088A95C8u, func_088A95C8, Void);
HOT60(0x08C21BA8u, func_08C21BA8, Word);
HOT60(0x08B344ECu, func_08B344EC, Word);
HOT60(0x08B34584u, func_08B34584, Word);
#undef HOT60
