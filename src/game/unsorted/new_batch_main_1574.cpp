// New batch main 1574 (draft only, unverified): 30 functions from tools.disasm, structured C.
// Families: a gated pair of calls on a1's word (14 functions), a pair of list calls on a0 (08A9C4C4 / 08A9C52C),
// and single functions. Every exit path sets v0 as the asm does: calls set v0 through `call`, the rest set it explicitly.
// Tail calls leave the frame first (gt::tail_call), as the asm's epilogue does in its delay slot.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1574 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The gated pair. The word at a1 is s0. first(a0, a1) answers; then 0x08A76F20(s0, answer) must be nonzero, and then
// 0x08A5BAD8(a0, word(a1)) runs and v0 = 1. Any other path gives v0 = 0.
void gated_pair(Runtime &rt, AllegrexContext &ctx, u32 first) {
    const u32 self = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 s0 = word(rt, slot);
    if (s0 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 answer = call(rt, ctx, first, {self, slot});
    if (call(rt, ctx, 0x08A76F20u, {s0, answer}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08A5BAD8u, {self, word(rt, slot)});
    ctx.gpr[2] = 1u;
}

// 0x08A9C4C4 / 0x08A9C52C (a0 = object): 0x08B344E0(a0 + 0x4C); p = the word at +field (a null p first calls 0x08A5BB7C with
// the string); the count (end - begin) >> 2, arithmetic, is taken before 0x08B3457C(a0 + 0x4C) and is v0.
void list_count(Runtime &rt, AllegrexContext &ctx, u32 field, u32 string) {
    const u32 self = ctx.gpr[4];
    const u32 list = self + 0x4Cu;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {list});
    const u32 p = word(rt, self + field);
    if (p == 0u) call(rt, ctx, 0x08A5BB7Cu, {string});
    const u32 begin = word(rt, p + 0x14u);
    const u32 end = word(rt, p + 0x18u);
    const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    call(rt, ctx, 0x08B3457Cu, {list});
    ctx.gpr[2] = count;
}

} // namespace

// 0x088087B0 family: gated pair on 0x08808524.
void func_088087B0(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x08808524u); }
void func_0880EF48(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x0880DB84u); }
void func_0881BC80(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x0881B060u); }
void func_088247AC(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x08824668u); }
void func_08846ED0(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x08846E48u); }
void func_08854844(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x08854670u); }
void func_088588EC(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x088586F0u); }
void func_089692C4(Runtime &rt, AllegrexContext &ctx) { gated_pair(rt, ctx, 0x08965B94u); }

// 0x08A9BFAC (a0 = object, a1 = argument): 0x08B344E0(a0 + 0x4C); s0 = the word at +0x38; a null s0 first calls 0x08A5BB7C with
// the string 0x08CB2570; then 0x08A7DE18(s0, a1); then the tail call 0x08B3457C(a0 + 0x4C).
void func_08A9BFAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 list = self + 0x4Cu;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B344E0u, {list});
        const u32 s0 = word(rt, self + 0x38u);
        if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
        call(rt, ctx, 0x08A7DE18u, {s0, argument});
    }
    tail_call(ctx, 0x08B3457Cu, {list});
}

void func_08A9C4C4(Runtime &rt, AllegrexContext &ctx) { list_count(rt, ctx, 0x78u, 0x08CB25E0u); }
void func_08A9C52C(Runtime &rt, AllegrexContext &ctx) { list_count(rt, ctx, 0x74u, 0x08CB25E0u); }

// 0x08AE05B8 (a0 = object, a1 = count): n = 0x08ADDCD4(a0, 0x08C9116C); v0 = 0x80 (set before the test); when n != 0 the count
// clamped to [-0x80, 0x80] (signed) goes to 0x08ADFC5C, then 0x08AD8CB0(n + 0x3C, 7, that answer).
void func_08AE05B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 requested = static_cast<s32>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x10u);
    const u32 n = call(rt, ctx, 0x08ADDCD4u, {self, 0x08C9116Cu});
    ctx.gpr[2] = 0x80u;
    if (n == 0u) return;
    s32 clamped = requested < 0x80 ? requested : 0x80;
    if (requested < -0x80) clamped = -0x80;
    const u32 answer = call(rt, ctx, 0x08ADFC5Cu, {static_cast<u32>(clamped)});
    call(rt, ctx, 0x08AD8CB0u, {n + 0x3Cu, 7u, answer});
}

// 0x08B1AF14 (a0 = out, a1 = text): the text pointer goes in the frame slot. For each byte of the text, 0x08B1AE1C(slot, 1)
// answers; its low half is stored at out (2 bytes further each time), and a zero answer returns 0 at once. At the end a zero
// half is stored and v0 = out.
void func_08B1AF14(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = ctx.gpr[29];
    put(rt, slot, text);
    u32 cursor = out;
    for (;;) {
        const u32 pointer = word(rt, slot);
        if (rt.memory().load8(pointer) == 0u) break;
        const u32 answer = call(rt, ctx, 0x08B1AE1Cu, {slot, 1u}) & 0xFFFFu;
        rt.memory().store16(cursor, static_cast<std::uint16_t>(answer));
        ctx.gpr[2] = answer;
        if (answer == 0u) return;
        cursor += 2u;
    }
    rt.memory().store16(cursor, static_cast<std::uint16_t>(0u));
    ctx.gpr[2] = out;
}

// 0x08B21E00 (a0 = object, a1 = argument): when bit 2 of the halfword at +2 of the record (object +0x60 -> +0xF4) is set,
// first = 0x08B21458(other record: object +0x5C -> +0xF4) & 0xFF and second = 0x08B21458(record, a1); v0 = (first ^ second) & first.
// Otherwise v0 = 0.
void func_08B21E00(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 record = word(rt, word(rt, self + 0x60u) + 0xF4u);
    u32 result = 0u;
    if ((rt.memory().load16(record + 2u) & 0x2u) != 0u) {
        const u32 other = word(rt, word(rt, self + 0x5Cu) + 0xF4u);
        const u32 first = call(rt, ctx, 0x08B21458u, {other}) & 0xFFu;
        const u32 second = call(rt, ctx, 0x08B21458u, {record, argument});
        result = (first ^ second) & first;
    }
    ctx.gpr[2] = result;
}

// 0x08B223AC (a0 = object, a1 = value): when the word at +0x30 is set, 0x08B2235C(old, object) runs (v0 stays its answer);
// then +0x30 = value; a zero value returns, any other value is the tail call 0x08B221B4(value, object).

// 0x08B82F2C (a0 = index, a1, a2): index >= 4 gives 0x80440010; index < 4 gives 0x8044000A, and when a1 and a2 are both
// <= 0x8000 (unsigned) v0 = 0x08C7ABE0(word(0x08D740EC + 4 * index)).
void func_08B82F2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[4];
    const u32 x = ctx.gpr[5];
    const u32 y = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    u32 result = 0x80440010u;
    if (index < 4u) {
        result = 0x8044000Au;
        if (x <= 0x8000u && y <= 0x8000u) {
            const u32 entry = word(rt, 0x08D740ECu + index * 4u);
            result = call(rt, ctx, 0x08C7ABE0u, {entry});
        }
    }
    ctx.gpr[2] = result;
}

// 0x088913B8 (a0 = object): three words at +0x44, +0x48, +0x4C; each nonzero one gets 0x0888F3E0(item) then 0x08B64654(item).
void func_088913B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    for (u32 k = 0u; k < 3u; ++k) {
        const u32 item = word(rt, self + 0x44u + 4u * k);
        if (item != 0u) {
            call(rt, ctx, 0x0888F3E0u, {item});
            call(rt, ctx, 0x08B64654u, {item});
        }
    }
}

// 0x088D6DC0 (a0 = object): when the word at +0x640 is set, 0x088DAA84(object) == 0 gives v1 = 1; otherwise 0x088DAADC(object)
// == 0 gives v1 = 1, and a nonzero answer stores -1 at +0x2C, zeroes the bytes at +0x1D, +0x26 and +0x1C, and gives v1 = 0.
void func_088D6DC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 result = 0u;
    if (word(rt, self + 0x640u) != 0u) {
        if (call(rt, ctx, 0x088DAA84u, {self}) == 0u) {
            result = 1u;
        } else if (call(rt, ctx, 0x088DAADCu, {self}) == 0u) {
            result = 1u;
        } else {
            ctx.gpr[2] = 0xFFFFFFFFu;
            put(rt, self + 0x2Cu, 0xFFFFFFFFu);
            rt.memory().store8(self + 0x1Du, static_cast<std::uint8_t>(0u));
            rt.memory().store8(self + 0x26u, static_cast<std::uint8_t>(0u));
            rt.memory().store8(self + 0x1Cu, static_cast<std::uint8_t>(0u));
            result = 0u;
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// 0x088DA288 (a0 = object): s0 = the word at +0xA8, s1 = the word at +0xAC; both zero gives 0. Otherwise answer = 0x088D9C70(object);
// the result is 1 when s1 < v1 (v1 as the call left it), or s1 == v1 and s0 < answer; else 0.
void func_088DA288(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 low = word(rt, self + 0xA8u);
    const u32 high = word(rt, self + 0xACu);
    ctx.gpr[3] = 0u;
    u32 result = 0u;
    if ((low | high) != 0u) {
        const u32 answer = call(rt, ctx, 0x088D9C70u, {self});
        const u32 old_v1 = ctx.gpr[3];
        ctx.gpr[4] = answer;
        const bool take = high < old_v1 || (high == old_v1 && low < answer);
        result = take ? 1u : 0u;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// 0x0892FA50 (a0, a1): first = 0x0892F8A4(a0, a1); 0 gives 0; not 1 gives 1; else g = 0x0892F95C(a0, a1, 0); g != -1 gives g, -1 gives 0.
void func_0892FA50(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 first = call(rt, ctx, 0x0892F8A4u, {self, argument});
    if (first == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (first != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 g = call(rt, ctx, 0x0892F95Cu, {self, argument, 0u});
    ctx.gpr[2] = g != 0xFFFFFFFFu ? g : 0u;
}

// 0x0892FC20 (a0 = dst, a1 = src): 0x0892FBE0(dst, src); dst +8 = src +8 and dst +0xC = src +0xC; when src +0x10 is set,
// dst +0x10 = 0x08937AF4(src +0x14) and 0x08B79A88(that, src +0x10, src +0x14), then dst +0x14 = src +0x14.
void func_0892FC20(Runtime &rt, AllegrexContext &ctx) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x0892FBE0u, {dst, src});
    ctx.gpr[2] = word(rt, src + 0x8u);
    const u32 v1 = word(rt, src + 0xCu);
    const u32 guard = word(rt, src + 0x10u);
    put(rt, dst + 0x8u, ctx.gpr[2]);
    put(rt, dst + 0xCu, v1);
    if (guard == 0u) return;
    const u32 g = call(rt, ctx, 0x08937AF4u, {word(rt, src + 0x14u)});
    put(rt, dst + 0x10u, g);
    const u32 middle = word(rt, src + 0x10u);
    const u32 last = word(rt, src + 0x14u);
    call(rt, ctx, 0x08B79A88u, {g, middle, last});
    put(rt, dst + 0x14u, word(rt, src + 0x14u));
}

// 0x08931D04 / 0x08935AA4 (a0, a1): first = 0x089363FC(a0, a1); g = 0x089364AC(first, a1, code) with v1 = its second answer;
// when g and that v1 are both -1 the result is 0, else 0x08931CA0 / 0x08935640 (a0, -1, g, v1) is the result.
void first_then_combine(Runtime &rt, AllegrexContext &ctx, u32 code, u32 combine) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 first = call(rt, ctx, 0x089363FCu, {self, argument});
    const u32 g = call(rt, ctx, 0x089364ACu, {first, argument, code});
    const u32 g_second = ctx.gpr[3];
    if (g == 0xFFFFFFFFu && g_second == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, combine, {self, 0xFFFFFFFFu, g, g_second});
}
void func_08931D04(Runtime &rt, AllegrexContext &ctx) { first_then_combine(rt, ctx, 0x22u, 0x08931CA0u); }
void func_08935AA4(Runtime &rt, AllegrexContext &ctx) { first_then_combine(rt, ctx, 0x26u, 0x08935640u); }

// 0x08932688 (a0 = object): first = 0x089363FC(a0); 0x08936584(first, a1, word +0x40, word +0x44) with t0 = the frame slot;
// v0 = 1 when the slot's byte at +0x23 is 1, 3, 4 or 7, else 0.
void func_08932688(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const u32 slot = ctx.gpr[29];
    const u32 first = call(rt, ctx, 0x089363FCu, {self});
    const u32 a2 = word(rt, self + 0x40u);
    const u32 a3 = word(rt, self + 0x44u);
    ctx.gpr[12] = slot;
    call(rt, ctx, 0x08936584u, {first, ctx.gpr[5], a2, a3});
    const u32 kind = rt.memory().load8(slot + 0x23u);
    ctx.gpr[2] = (kind == 1u || kind == 3u || kind == 4u || kind == 7u) ? 1u : 0u;
}

} // namespace new_batch_main_1574
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1574::name, gt::ReturnKind::kind)
NEWM(0x088087B0u, func_088087B0, Word);
NEWM(0x0880EF48u, func_0880EF48, Word);
NEWM(0x0881BC80u, func_0881BC80, Word);
NEWM(0x088247ACu, func_088247AC, Word);
NEWM(0x08846ED0u, func_08846ED0, Word);
NEWM(0x08854844u, func_08854844, Word);
NEWM(0x088588ECu, func_088588EC, Word);
NEWM(0x089692C4u, func_089692C4, Word);
NEWM(0x08A9BFACu, func_08A9BFAC, Word);
NEWM(0x08A9C4C4u, func_08A9C4C4, Word);
NEWM(0x08A9C52Cu, func_08A9C52C, Word);
NEWM(0x08AE05B8u, func_08AE05B8, Word);
NEWM(0x08B1AF14u, func_08B1AF14, Word);
NEWM(0x08B21E00u, func_08B21E00, Word);
NEWM(0x08B82F2Cu, func_08B82F2C, Word);
NEWM(0x088913B8u, func_088913B8, Word);
NEWM(0x088D6DC0u, func_088D6DC0, Word);
NEWM(0x088DA288u, func_088DA288, Word);
NEWM(0x0892FA50u, func_0892FA50, Word);
NEWM(0x0892FC20u, func_0892FC20, Word);
NEWM(0x08931D04u, func_08931D04, Word);
NEWM(0x08932688u, func_08932688, Word);
NEWM(0x08935AA4u, func_08935AA4, Word);
