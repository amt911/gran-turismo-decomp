// New batch main-2081 (draft only, unverified): 30 functions of the re-draft list, written as structured C from `tools.disasm`.
// Every body keeps the original's exits (v0 on each path), the delay slot runs before its call, every argument register a
// callee reads is passed, and s-registers the asm keeps as values are plain locals here.
// Input shapes for the synthetic generator: /home/andres/repos/gt-ops/shapes/shapes_main2081.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2081 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using u64 = std::uint64_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A guest call; afterwards `v0`, `v1`, `f0` and `a1` hold what the callee left, as after the original's `jal`.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result;
}

// The same with float arguments (f12, f13 ...).
GuestResult call_result_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                          std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result;
}

// `wsbw`: byte-swap each word.
u32 bswap32(u32 v) {
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}

// `trunc.w.s`: float to a signed word, truncating; out of range and NaN saturate as the FPU does.
s32 trunc_word(float x) {
    if (std::isnan(x) || x >= 2147483648.0f) return 0x7FFFFFFF;
    if (x < -2147483648.0f) return static_cast<s32>(0x80000000u);
    return static_cast<s32>(x);
}

// The 60-byte result block the copy helpers below move into the caller's output (15 words, in order).
void copy_out(Runtime &rt, u32 dst, u32 src) {
    for (u32 i = 0; i < 60u; i += 4u) put(rt, dst + i, word(rt, src + i));
}

// 120 bytes: two calls-through of the same pattern (0x08B28ACC / 0x08B28B44): the first call, the vtable word, the
// flag word cleared, then the tail call with the same arguments.
void construct_then_tail(Runtime &rt, AllegrexContext &ctx, u32 init_call, u32 tail) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    call(rt, ctx, init_call, {self, a1, a2, a3});
    put(rt, self, 0x08CD9678u);
    put(rt, self + 0xA4u, 0u);
    tail_call(ctx, tail, {self, a1, a2, a3, t0});
}

// The copy-with-temp family: a temp block of four words (the source pair at one of two offsets, zero at the other), the
// callee writes 60 bytes at sp+0x10 and the copy moves them to the output (the a0 of the call).
void temp_pair(Runtime &rt, u32 sp, u32 src, bool pair_first) {
    if (pair_first) {
        put(rt, sp, word(rt, src));
        put(rt, sp + 4u, word(rt, src + 4u));
        put(rt, sp + 8u, 0u);
        put(rt, sp + 0xCu, 0u);
    } else {
        put(rt, sp, 0u);
        put(rt, sp + 4u, 0u);
        put(rt, sp + 8u, word(rt, src));
        put(rt, sp + 0xCu, word(rt, src + 4u));
    }
}

// The rebase-then-copy shape of 0x08BB1B9C / 0x08BB1C2C: the first call gets the length, the second the copy, the pointer
// field at +4 is moved back by the rounded difference.
void rebase_then_copy(Runtime &rt, AllegrexContext &ctx, u32 copy_target) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 s2 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 s0 = (a2 - s2) & ~3u;
    const u32 s1 = word(rt, self + 4u) - a2;
    call(rt, ctx, 0x08B79BACu, {s2, a2, s1, ctx.gpr[7]});
    const u32 field = word(rt, self + 4u);
    call(rt, ctx, copy_target, {s1 + s2, field, ctx.gpr[29], ctx.gpr[7]});
    put(rt, self + 4u, word(rt, self + 4u) - s0);
    ctx.gpr[2] = s2;
}

}  // namespace

// 0x08A4DAA0: the flag word at +0x58 (bit 0x200) decides whether the one-shot call runs; the call takes a pointer to six
// stacked argument words. v0 is the flag when the call is skipped, else the call's result.
void func_08A4DAA0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 block = ctx.gpr[29] + 0x28u;
    put(rt, block, ctx.gpr[6]);
    put(rt, block + 4u, ctx.gpr[7]);
    put(rt, block + 8u, ctx.gpr[8]);
    put(rt, block + 12u, ctx.gpr[9]);
    put(rt, block + 16u, ctx.gpr[10]);
    put(rt, block + 20u, ctx.gpr[11]);
    const u32 flags = word(rt, self + 0x58u) & 0x200u;
    ctx.fpr[0] = 0.0f;
    u32 v0 = flags;
    if (flags == 0u) {
        const u32 saved = word(rt, self + 0x3Cu);
        put_float(rt, self + 0x28u, 0.0f);
        put(rt, ctx.gpr[29], block);
        v0 = call(rt, ctx, 0x08A4D79Cu, {self, ctx.gpr[5], block, 0u, 1u, ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
        ctx.fpr[0] = float_at(rt, self + 0x28u);
        put(rt, self + 0x3Cu, saved);
    }
    ctx.gpr[2] = v0;
}

// 0x08B28ACC: construct (vtable at +0, +0xA4 cleared) then tail call with the same arguments.

// 0x08B28B44: the same body as 0x08B28ACC at another address.

// 0x08ADFFF4: allocates (count * 4 + 15, low four bits cleared) on the stack; the three calls get the block pointer.

// 0x089C5000: constructs the object at a0 (vtable, two fields), stores two converted floats into a global block.

// 0x08860484: sets a flag byte in the object's pointee, calls the sub-object's method, clears the flag; v0 is the callee's.

// 0x08B245FC: writes a 64-bit product sum (HI:LO) of two fields as the second word of the result pair at a0.

// 0x08B1B074: when the two fields at +0x28/+0x2C are both zero the function returns v0 unchanged; else it subtracts the
// callee's result from the length and tail-calls 0x08B2E2B8 when the remainder is positive (or zero with length left).
void func_08B1B074(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 field2C = word(rt, self + 0x2Cu);
    if ((word(rt, self + 0x28u) | field2C) == 0u) return;
    const GuestResult r = call_result(rt, ctx, 0x08B1B000u, {self, field2C, a2, a3});
    const u32 a2p = a2 - r.v0;
    const u32 borrow = (a2 < a2p) ? 1u : 0u;
    const u32 a3p = a3 - r.v1 - borrow;
    if (static_cast<s32>(a3p) > 0 || (a3p == 0u && a2p != 0u)) {
        tail_call(ctx, 0x08B2E2B8u, {a2p, r.a1, a2p, a3p});
        return;
    }
    ctx.gpr[2] = r.v0;
}

// 0x08931350: two text-reader calls, then a lookup on the result; v0 is 0 when the lookup finds nothing, else the value
// at the object's +0x1EC through the last call.
void func_08931350(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 r0 = call(rt, ctx, 0x089363FCu, {self, a1, a2, a3});
    call(rt, ctx, 0x08936578u, {r0, a1, self, a1});
    const u32 r2 = call(rt, ctx, 0x089363FCu, {r0, a1, self, a1});
    const u32 sp = ctx.gpr[29];
    const u32 r4 = call(rt, ctx, 0x08936544u, {r2, r2, 0x22u, sp});
    u32 result = 0u;
    if (r4 != 0u) {
        const u32 r5 = call(rt, ctx, 0x089363FCu, {r2, r2, 0x22u, sp});
        const u32 arg0 = word(rt, r5 + 0x1ECu);
        const u32 arg1 = word(rt, sp + 4u);
        result = call(rt, ctx, 0x08937D90u, {arg0, arg1});
    }
    ctx.gpr[2] = result;
}

// 0x08BB1B9C: rebase-then-copy (see rebase_then_copy), callee 0x08BB1B8C; v0 = a1.

// 0x08AE1418: a float argument in f12 is kept across two calls; the block at sp+0x428 holds the spilled arguments.
void func_08AE1418(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x440u);
    const u32 sp = ctx.gpr[29];
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a3 = ctx.gpr[7];
    const u32 block = sp + 0x428u;
    put(rt, block, ctx.gpr[6]);
    put(rt, block + 4u, a3);
    put(rt, block + 8u, ctx.gpr[8]);
    put(rt, block + 12u, ctx.gpr[9]);
    put(rt, block + 16u, ctx.gpr[10]);
    put(rt, block + 20u, ctx.gpr[11]);
    const float f12 = ctx.fpr[12];
    call_result_f(rt, ctx, 0x08B373ECu, {sp, a0, block, a3}, {f12});
    // a0 clamped to [-0x80, 0x80] (the slti/min pair of the original)
    const s32 signed_a1 = static_cast<s32>(a1);
    const s32 clamped = signed_a1 < -0x80 ? -0x80 : std::min(signed_a1, 0x80);
    const u32 r = call(rt, ctx, 0x08ADFC5Cu, {static_cast<u32>(clamped), a0, block, a3});
    ctx.fpr[12] = f12;
    const GuestResult fin = call_result(rt, ctx, 0x08AE1280u, {sp, r, block, a3});
    ctx.gpr[2] = fin.v0;
    ctx.fpr[0] = fin.f0;
}

// 0x08BB1C2C: rebase-then-copy (see rebase_then_copy), callee 0x08BB1C1C.

// 0x089F14D0: allocates 0x2C bytes, fills them through 0x089F12C4, then hands the result and a stack slot to 0x089FDAB0.

// 0x08A75C80: copies the size of the range at a1 into a fresh allocation; the object at a0 gets begin/cursor/end.

// 0x08B2B55C: loops on 0x08B35DDC until it returns non-null (calling 0x08B2E064 and, once, 0x08B2B490 with 1 between tries);
// v0 is that value, and 0x08B2B490 is called with -1 when the flag was set.
void func_08B2B55C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 s3 = ctx.gpr[4];
    const u32 s2 = ctx.gpr[5];
    u32 s1 = 0u;
    u32 s0 = 0u;
    for (;;) {
        s0 = call(rt, ctx, 0x08B35DDCu, {s3, s2, ctx.gpr[6], ctx.gpr[7]});
        if (s0 != 0u) break;
        call(rt, ctx, 0x08B2E064u, {s3, s2, ctx.gpr[6], ctx.gpr[7]});
        if (s1 == 0u) {
            call(rt, ctx, 0x08B2B490u, {1u});
            s1 = 1u;
        }
    }
    if (s1 != 0u) call(rt, ctx, 0x08B2B490u, {0xFFFFFFFFu});
    ctx.gpr[2] = s0;
}

// 0x08AF4580: calls the object's virtual slot 3 (with f12 kept in f20), then 0x08AF4478, slots 2 and 0x08AF4504.

// 0x0893215C: two calls chained through v0/v1, then 0x089320A4 and 0x08932748; v0 is the value of the 0x08935640 call.
void func_0893215C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const GuestResult r1 = call_result(rt, ctx, 0x08932100u, {self, a1, a2, a3});
    const GuestResult r2 = call_result(rt, ctx, 0x08935640u, {a2, a1, r1.v0, r1.v1});
    const u32 s3 = r2.v0;
    const GuestResult r3 = call_result(rt, ctx, 0x089320A4u, {self, a1, r1.v0, r1.v1});
    const u32 a1f = (r3.v0 == 0xFFFFFFFFu && r3.v1 == 0xFFFFFFFFu) ? 0xFFFFFFFFu : r3.v0 - 1u;
    call(rt, ctx, 0x08932748u, {a2, a1f, 0xFFFFFFFFu, r1.v1});
    ctx.gpr[2] = s3;
}

// 0x089FA1F4: the same shape as 0x08A75C80 with a 16-byte allocation of the range rounded to 32, then 0x08C0D7F8.
void func_089FA1F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 range = ctx.gpr[5];
    const u32 end = word(rt, range + 4u);
    const u32 begin = word(rt, range);
    put(rt, self + 4u, 0u);
    const u32 size = (end - begin) & ~0x1Fu;
    put(rt, self, 0u);
    put(rt, self + 8u, 0u);
    const u32 block = call(rt, ctx, 0x08B35840u, {0x10u, size});
    const u32 cursor_end = block + size;
    put(rt, self, block);
    put(rt, self + 4u, block);
    put(rt, self + 8u, cursor_end);
    const u32 ret = call(rt, ctx, 0x08C0D7F8u, {word(rt, range), word(rt, range + 4u), block, ctx.gpr[29]});
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    put(rt, self + 4u, ret);
    ctx.gpr[2] = ret;
}

// 0x08AA462C: looks a string up through the vtable of the object at a1 (the word at -0xC); returns a0 (= the first argument).
void func_08AA462C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 result = ctx.gpr[4];
    const u32 s0 = ctx.gpr[5];
    const u32 vt = word(rt, s0);
    const u32 offset = word(rt, vt - 0xCu);
    if (offset == 0u) {
        call(rt, ctx, 0x08B88F44u, {result, s0});
    } else {
        const u32 r = call(rt, ctx, 0x08BFD224u, {s0, 0x2Eu, 0xFFFFFFFFu});
        if (r == 0xFFFFFFFFu) {
            call(rt, ctx, 0x08B88F44u, {result, s0});
        } else {
            const u32 vt2 = word(rt, s0);
            const u32 offset2 = word(rt, vt2 - 0xCu);
            u32 a0 = result;
            if (offset2 < r) {
                a0 = 0x08CB2D84u;
                call(rt, ctx, 0x08B5AEDCu, {0x08CB2D84u, s0});
            }
            call(rt, ctx, 0x08B8D96Cu, {a0, s0, r, 0xFFFFFFFFu});
        }
    }
    ctx.gpr[2] = result;
}

// 0x089D6418: stores the vtable and the arguments in the object at a0; calls 0x08965984 first, and 0x08A5BA08 when a2 is set.
void func_089D6418(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const float f12 = ctx.fpr[12];
    const float f13 = ctx.fpr[13];
    call_result_f(rt, ctx, 0x08965984u, {self, ctx.gpr[5], a2, a3}, {f12, f13});
    put(rt, self, 0x08CC9628u);
    put(rt, self + 0x24u, a2);
    u32 ret = 0x08CC9628u;
    if (a2 != 0u) ret = call(rt, ctx, 0x08A5BA08u, {a2});
    put_float(rt, self + 0x38u, f13);
    put(rt, self + 0x28u, a3);
    put(rt, self + 0x2Cu, t0);
    put(rt, self + 0x30u, t1);
    put_float(rt, self + 0x34u, f12);
    ctx.gpr[2] = ret;
}

// 0x08890A1C: looks up a word-swapped field through the table at +0xDC of the record a0 holds; the result is that value
// plus (a2 << 4). The branches add the object's own word at +0 when the record's first table entry is zero.
void func_08890A1C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 r = call(rt, ctx, 0x0888F548u, {self, a1, a2, ctx.gpr[7]});
    const u32 first = bswap32(word(rt, r + 4u));
    const u32 a2v = (first == 0u) ? bswap32(word(rt, r + 0xDCu)) + word(rt, self)
                                  : bswap32(word(rt, r + 0xDCu));
    const u32 a3v = a2v + (a1 << 5);
    const u32 second = bswap32(word(rt, r + 4u));
    const u32 v1 = (second == 0u) ? bswap32(word(rt, a3v + 0x14u)) + word(rt, self)
                                  : bswap32(word(rt, a3v + 0x14u));
    ctx.gpr[2] = v1 + (a2 << 4);
}

// 0x08B0FAEC: the two-word source at t0 seeds a temp block, the callee 0x08B0F264 fills a 60-byte result, copied to a0.
void func_08B0FAEC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 dst = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 out = sp + 0x10u;
    temp_pair(rt, sp, ctx.gpr[8], true);
    call(rt, ctx, 0x08B0F264u, {out, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], sp, ctx.gpr[9] & 0xFFu});
    copy_out(rt, dst, out);
    ctx.gpr[2] = dst;
}

// 0x08B0F3A4: as 0x08B0FAEC with the source pair at the top of the temp block.
void func_08B0F3A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 dst = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 out = sp + 0x10u;
    temp_pair(rt, sp, ctx.gpr[8], false);
    call(rt, ctx, 0x08B0F264u, {out, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], sp, ctx.gpr[9] & 0xFFu});
    copy_out(rt, dst, out);
    ctx.gpr[2] = dst;
}

// 0x08B0FCA4: the source pair is the a3 record; t0 is masked; the callee 0x08B0F4E0 fills the output.

// 0x08B0FD48: as 0x08B0FCA4 with the callee 0x08B0F448.
void func_08B0FD48(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 dst = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 out = sp + 0x10u;
    temp_pair(rt, sp, ctx.gpr[7], false);
    call(rt, ctx, 0x08B0F448u, {out, ctx.gpr[5], ctx.gpr[6], sp, ctx.gpr[8] & 0xFFu});
    copy_out(rt, dst, out);
    ctx.gpr[2] = dst;
}

// 0x08B0FF00: the a3 record seeds the temp block (pair first); t0 masked; the callee 0x08B0F4E0.
void func_08B0FF00(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 dst = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 out = sp + 0x10u;
    temp_pair(rt, sp, ctx.gpr[7], true);
    call(rt, ctx, 0x08B0F4E0u, {out, ctx.gpr[5], ctx.gpr[6], sp, ctx.gpr[8] & 0xFFu});
    copy_out(rt, dst, out);
    ctx.gpr[2] = dst;
}

// 0x08B0FFA4: as 0x08B0FF00 with the callee 0x08B0F448.
void func_08B0FFA4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 dst = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 out = sp + 0x10u;
    temp_pair(rt, sp, ctx.gpr[7], true);
    call(rt, ctx, 0x08B0F448u, {out, ctx.gpr[5], ctx.gpr[6], sp, ctx.gpr[8] & 0xFFu});
    copy_out(rt, dst, out);
    ctx.gpr[2] = dst;
}

// 0x08AFA2BC: when the list head at +0x34 of a0 is set, calls 0x08AF9DF0 for a (begin, end) pair and walks it, calling
// 0x08AFA208 per 8-byte entry; v0 is the last callee's value (or 0 when the head is clear).
void func_08AFA2BC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 sp = ctx.gpr[29];
    if (word(rt, self + 0x34u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 s2 = a2 & 0xFFu;
    const GuestResult r = call_result(rt, ctx, 0x08AF9DF0u, {self, a3, a2, a3});
    put(rt, sp + 0x10u, r.v0);
    put(rt, sp + 0x14u, r.v1);
    put(rt, sp, r.v0);
    put(rt, sp + 4u, r.v1);
    u32 ret = r.v0;
    if (r.v0 != r.v1) {
        u32 entry = r.v0;
        do {
            ret = call(rt, ctx, 0x08AFA208u, {self, a1, s2, word(rt, entry + 4u), t0});
            entry += 8u;
        } while (entry != r.v1);
    }
    ctx.gpr[2] = ret;
}

// 0x0886C3A4: reads a table through two callees (0x08B93CD8, 0x08B93D94) and the record's +4 field; returns the word at +0x14
// of the second callee's result, or 0 when the record's pointer is that result.

// 0x08987C7C: looks up a float through 0x08987A74 (a status word that must be 0x101), then 0x08B77BF4 and 0x08B71EC8;
// returns 1 when the lookup got a value, else 0, and releases the entry through 0x08B60804 / 0x08B8542C.

}  // namespace new_batch_main_2081
}  // namespace gt::game

#define NEW2081(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2081::name, gt::ReturnKind::kind)
NEW2081(0x08A4DAA0u, func_08A4DAA0, Word);
NEW2081(0x08B1B074u, func_08B1B074, Word);
NEW2081(0x08931350u, func_08931350, Word);
NEW2081(0x08AE1418u, func_08AE1418, Word);
NEW2081(0x08B2B55Cu, func_08B2B55C, Word);
NEW2081(0x0893215Cu, func_0893215C, Word);
NEW2081(0x089FA1F4u, func_089FA1F4, Word);
NEW2081(0x08AA462Cu, func_08AA462C, Word);
NEW2081(0x089D6418u, func_089D6418, Word);
NEW2081(0x08890A1Cu, func_08890A1C, Word);
NEW2081(0x08B0FAECu, func_08B0FAEC, Word);
NEW2081(0x08B0F3A4u, func_08B0F3A4, Word);
NEW2081(0x08B0FD48u, func_08B0FD48, Word);
NEW2081(0x08B0FF00u, func_08B0FF00, Word);
NEW2081(0x08B0FFA4u, func_08B0FFA4, Word);
NEW2081(0x08AFA2BCu, func_08AFA2BC, Word);
