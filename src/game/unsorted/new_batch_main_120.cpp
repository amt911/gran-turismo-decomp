// New batch main-120 (draft, unverified). Structured C from tools.disasm: small constructors / destructors, list helpers and float helpers.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_120 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A `jal`: v0 is the callee's result.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A `jal` with float arguments (f12, f13, ...); v0 / v1 / f0 back as the callee left them; returns f0.
float callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
float bitf(u32 bits) { return std::bit_cast<float>(bits); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// 0x0889D2A8: kind = 0x0889D2A0(a0); the jump table at 0x08C89C28 maps 0 -> 0, 16 and 17 -> 2, 32 and 33 -> 1, every other
// index below 0x22 -> 3 (and so does an index above); v0 = that.
void func_0889D2A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 kind = call(rt, ctx, 0x0889D2A0u, {ctx.gpr[4]});
    u32 result = 3u;
    if (kind < 0x22u) {
        if (kind == 0u) result = 0u;
        else if (kind == 16u || kind == 17u) result = 2u;
        else if (kind == 32u || kind == 33u) result = 1u;
    }
    ctx.gpr[3] = kind;
    ctx.gpr[2] = result;
}

// 0x08B0B748 (a0 = list object, a1 = callback): when the callback exists, calls it with each of the byte-swapped words at
// +0x24 + 16 * i, i below the byte-swapped count at +0xC. v0: the incoming one with no callback, else the raw count, else the last result.
void func_08B0B748(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 callback = ctx.gpr[5];
    if (callback == 0u) return;
    const u32 raw_count = word(rt, object + 0xCu);
    ctx.gpr[2] = raw_count;
    const s32 count = static_cast<s32>(__builtin_bswap32(raw_count));
    for (s32 index = 0; index < count; ++index) {
        const u32 entry = word(rt, object + 0x24u + 0x10u * static_cast<u32>(index));
        call_indirect(rt, ctx, callback, {__builtin_bswap32(entry)});
    }
}

// 0x08B01680 (a0 = this, a1 = value): push_back on the vector at +0xA8 (begin +0xA8, end +0xAC, limit +0xB0); a full one calls
// 0x08C475C8(vector, end, slot, vector). The value sits in a stack slot; the object it then holds (word at the slot) gets its
// virtual at +8 of the vtable called with this.
void func_08B01680(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 vector = self + 0xA8u;
    const u32 slot = frame.at(0);
    ctx.gpr[7] = vector;
    put(rt, slot, value);
    u32 end = word(rt, vector + 4u);
    const u32 limit = word(rt, vector + 8u);
    if (end == limit) {
        call(rt, ctx, 0x08C475C8u, {vector, end, slot, vector});
    } else {
        if (end != 0u) {
            put(rt, end, value);
            end = word(rt, vector + 4u);
        }
        put(rt, vector + 4u, end + 4u);
    }
    const u32 object = word(rt, slot);
    const u32 vtable = word(rt, object);
    call_indirect(rt, ctx, word(rt, vtable + 8u), {object, self});
}

// 0x08B38CF0 (a0 = this): destructor. The two members at +0x1C and +0x14 go through 0x08C50CCC; the shared object at +0x10 loses
// a reference (word +4) and, at zero, its virtual at +4 of the vtable runs with it. v0 = 0x08CD9428 (the vtable left in this).
void func_08B38CF0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CDA928u);
    call(rt, ctx, 0x08C50CCCu, {self + 0x1Cu});
    call(rt, ctx, 0x08C50CCCu, {self + 0x14u});
    const u32 shared = word(rt, self + 0x10u);
    put(rt, self + 0xCu, 0x08CDA8F8u);
    if (shared != 0u) {
        const u32 references = word(rt, shared + 4u) - 1u;
        put(rt, shared + 4u, references);
        if (references == 0u) {
            const u32 vtable = word(rt, shared);
            call_indirect(rt, ctx, word(rt, vtable + 4u), {shared});
        }
    }
    put(rt, self, 0x08CD9428u);
    ctx.gpr[2] = 0x08CD9428u;
}

// 0x0887336C (a0 = this): destructor. The owned object at +0x60 (when set) has its virtual at +0 of its vtable called with
// (it, word +0x5C); both fields are cleared; then 0x08AF484C(this + 0xC). v0 = that call's result.

// 0x088F3790 (a0 = this): constructor. Vtable, flags at +0x194..+0x198, zeroes +0x2C..+0x190 (memset 0x08B36018), a buffer from
// 0x08B2B4D8(0x40, 0x4000) at +4, floats 4.0 / 1.0 at +0x19C / +0x1A0, then a tail call of 0x088F2BE8(this).
void func_088F3790(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC3C58u);
        rt.memory().store8(self + 0x196u, 1u);
        put(rt, self + 0x198u, 0x12345678u);
        rt.memory().store8(self + 0x194u, 0u);
        rt.memory().store8(self + 0x195u, 0u);
        rt.memory().store8(self + 0x197u, 0u);
        call(rt, ctx, 0x08B36018u, {self + 0x2Cu, 0u, 0x164u});
        const u32 buffer = call(rt, ctx, 0x08B2B4D8u, {0x40u, 0x4000u});
        ctx.fpr[0] = 4.0f;
        ctx.fpr[1] = 1.0f;
        put(rt, self + 4u, buffer);
        put_float(rt, self + 0x19Cu, 4.0f);
        put_float(rt, self + 0x1A0u, 1.0f);
        put(rt, self + 0x24u, 0u);
    }
    tail_call(ctx, 0x088F2BE8u, {self});
}

// 0x08934E38 (a0, a1 = a pair of ids, a2 = target): both -1 leave v0 = -1; else 0x08936584(ctx, a1, a0, a1, sp) filling a stack
// buffer; when it answers 0, v0 = 0; else 0x08B7BB6C(a2, 0x089365F8(ctx, a1, a0, a1)); v0 = that result.
void func_08934E38(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 target = ctx.gpr[6];
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (first == 0xFFFFFFFFu && second == 0xFFFFFFFFu) return;
    u32 context = call(rt, ctx, 0x089363FCu);
    const u32 found = call(rt, ctx, 0x08936584u, {context, ctx.gpr[5], first, second, frame.at(0)});
    if (found == 0u) return;
    context = call(rt, ctx, 0x089363FCu);
    const u32 value = call(rt, ctx, 0x089365F8u, {context, ctx.gpr[5], first, second});
    call(rt, ctx, 0x08B7BB6Cu, {target, value});
}

// 0x08AC1E18 (a0 = this, a1 = source): constructor. Source word +0 must be 0x45565346 and its halfword +8 0xFFFE; then
// this+0x34 = source + the word at source+0xC, else this+0x34 / +0x30 = 0. v0 follows the path (magic, 0xFFFE, or the address).
void func_08AC1E18(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    put(rt, self, 0x08CD6530u);
    call(rt, ctx, 0x08AC263Cu, {self + 4u});
    rt.memory().store16(self + 0x3Cu, 0u);
    u32 flags = word(rt, self + 0x3Cu);
    put(rt, self + 0x30u, source);
    flags &= ~(1u << 16);
    put(rt, self + 0x3Cu, flags);
    ctx.gpr[2] = 0x45565346u;
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x40u, 0u);
    if (word(rt, source) != 0x45565346u) {
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x30u, 0u);
        return;
    }
    ctx.gpr[2] = 0xFFFEu;
    if (rt.memory().load16(source + 8u) == 0xFFFEu) {
        const u32 offset = word(rt, source + 0xCu);
        ctx.gpr[2] = source + offset;
        put(rt, self + 0x34u, source + offset);
    } else {
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x30u, 0u);
    }
}

// 0x08A19C28 (a0 = this): constructor. Fields at +4..+0x34 set (0x08D74FD0 + 0xC at +4, 0x1E0 / 0x110, 1.0f at +0x18),
// 0x08A94CE8(this + 0x24), then a tail call of 0x08A19688(this). v0 = the first call's result.
void func_08A19C28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD0658u);
        put(rt, self + 4u, 0x08D74FD0u + 0xCu);
        rt.memory().store8(self + 0x15u, 1u);
        rt.memory().store8(self + 0x16u, 1u);
        put(rt, self + 8u, 0x1E0u);
        put(rt, self + 0xCu, 0x110u);
        put_float(rt, self + 0x18u, 1.0f);
        rt.memory().store8(self + 0x14u, 0u);
        rt.memory().store8(self + 0x17u, 0u);
        put(rt, self + 0x1Cu, 0u);
        put(rt, self + 0x20u, 0u);
        call(rt, ctx, 0x08A94CE8u, {self + 0x24u});
        rt.memory().store8(self + 0x34u, 1u);
        put(rt, self + 0x28u, 0u);
        put(rt, self + 0x2Cu, 0u);
        put(rt, self + 0x30u, 0u);
    }
    tail_call(ctx, 0x08A19688u, {self});
}

// 0x08AD7D5C (a0 = this): constructor of two 16-byte entries at +4 and +0x14, each built by 0x08AD7CA0(entry) and then its first
// word cleared; the header fields +0x24..+0x44 are set first (1.0f at +0x34 / +0x38, 200 at +0x2C). v0 = the last call's result.
void func_08AD7D5C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CD6FC8u);
    put(rt, self + 0x2Cu, 0xC8u);
    put_float(rt, self + 0x38u, 1.0f);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x30u, 0u);
    put_float(rt, self + 0x34u, 1.0f);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    rt.memory().store8(self + 0x44u, 0u);
    for (u32 index = 0; index < 2u; ++index) {
        call(rt, ctx, 0x08AD7CA0u, {self + 4u + 0x10u * index});
        put(rt, self + 0x10u * index + 4u, 0u);
    }
}

// 0x08A893A8 (a0 = this): destructor. 0x08A88DA4(this); when the byte at +0x144 is set and the object at +0x138 exists,
// 0x08B61FC0(it); 0x08B0C808(this + 0x20); the list head word at +0x1C minus 0xC, unless it is the shared empty head
// 0x08D74FD0, is released with 0x08B8542C(head, sp) after 0x08B60804(head + 8, -1) answered 0 or less; then 0x08A72308(this).
void func_08A893A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CCF110u);
    call(rt, ctx, 0x08A88DA4u, {self});
    if (rt.memory().load8(self + 0x144u) != 0u) {
        const u32 owned = word(rt, self + 0x138u);
        if (owned != 0u) call(rt, ctx, 0x08B61FC0u, {owned});
    }
    call(rt, ctx, 0x08B0C808u, {self + 0x20u});
    const u32 list = word(rt, self + 0x1Cu);
    const u32 head = list - 0xCu;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (head != 0x08D74FD0u) {
        const s32 references = static_cast<s32>(call(rt, ctx, 0x08B60804u, {list - 4u, 0xFFFFFFFFu}));
        if (references <= 0) call(rt, ctx, 0x08B8542Cu, {head, frame.at(0)});
    }
    call(rt, ctx, 0x08A72308u, {self});
}

// 0x08AE1044 (a0 = object, f12 = level, f13 = argument): looks up 0x08ADDCD4(a0, 0x08C912F0); when found, scales the level
// (clamped by the floats at 0x08CB60B8 / 0x08CB60BC) by the float at 0x08CB60C0, truncates it and calls 0x08AE0EB0(found, n, 0, f12 = f13).
void func_08AE1044(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const float level = ctx.fpr[12];
    const float argument = ctx.fpr[13];
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C912F0u});
    if (found == 0u) return;
    const float low = float_at(rt, 0x08CB60B8u);
    const float high = float_at(rt, 0x08CB60BCu);
    float clamped;
    if (!(level < low)) clamped = low;
    else if (level < high) clamped = high;
    else clamped = level;
    const float scaled = psp_mul(clamped, float_at(rt, 0x08CB60C0u));
    callf(rt, ctx, 0x08AE0EB0u, {found, static_cast<u32>(trunc_w_s(scaled)), 0u}, {argument});
}

// 0x08C52EE4 (a0 = vector floats, a1 = out3, a2 = out1, a3 = out2): angles from a direction: out1 = 0x08C5250C(f[+0x20]); with
// 0x08B499B8(out1) zero, out2 = 0 and out3 = 0x08B4A430(f[+0x18], f[+0x14]); else out2 = 0x08B4A430(-f[+0x10], f[+0]) and
// out3 = 0x08B4A430(-f[+0x24], f[+0x28]).
void func_08C52EE4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 source = ctx.gpr[4];
    const u32 out3 = ctx.gpr[5];
    const u32 out1 = ctx.gpr[6];
    const u32 out2 = ctx.gpr[7];
    const float angle = callf(rt, ctx, 0x08C5250Cu, {}, {float_at(rt, source + 0x20u)});
    put_float(rt, out1, angle);
    const float probe = callf(rt, ctx, 0x08B499B8u, {}, {angle});
    if (probe == 0.0f) {
        put_float(rt, out2, 0.0f);
        const float last = callf(rt, ctx, 0x08B4A430u, {}, {float_at(rt, source + 0x18u), float_at(rt, source + 0x14u)});
        put_float(rt, out3, last);
    } else {
        const float middle = callf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, source + 0x10u), float_at(rt, source + 0u)});
        put_float(rt, out2, middle);
        const float last = callf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, source + 0x24u), float_at(rt, source + 0x28u)});
        put_float(rt, out3, last);
    }
}

// 0x0886A704 (a0 = this): constructor. A 0x4244-byte object from 0x08B62344, set up by 0x0886A5C8, stored at +4; a function-local
// static (guard byte 0x08D752D0, slot 0x08D752D8) is created on first use and its address stored at +8. v0: 0x000A0000 when the
// guard was set, else the acquire's answer (0) or the release call's result.

// 0x08ABD398 (a0 = this): constructor. Fields +4..+0x1C, 0x08C3C47C(this + 0x20), eight words at +0x64 zeroed, 0x08B344E0(this + 0x3C),
// 0x08B24D20(this + 0x20, this + 0x64, 8, 4), then a tail call of 0x08B3457C(this + 0x3C). v0 = the 0x08B24D20 result.
void func_08ABD398(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 first = self + 0x20u;
        const u32 table = self + 0x64u;
        put(rt, self, 0x08CD6308u);
        put(rt, self + 0x10u, 0xFFFFFFFFu);
        put(rt, self + 0x18u, 1u);
        put(rt, self + 4u, 0u);
        put(rt, self + 8u, 0u);
        put(rt, self + 0xCu, 0u);
        rt.memory().store8(self + 0x14u, 0u);
        rt.memory().store8(self + 0x15u, 0u);
        put(rt, self + 0x1Cu, 0u);
        call(rt, ctx, 0x08C3C47Cu, {first});
        u32 entry = table;
        for (u32 pair = 0; pair < 4u; ++pair) {
            put(rt, entry, 0u);
            put(rt, entry + 4u, 0u);
            entry += 8u;
        }
        const u32 member = first + 0x1Cu;
        call(rt, ctx, 0x08B344E0u, {member});
        call(rt, ctx, 0x08B24D20u, {first, table, 8u, 4u});
    }
    tail_call(ctx, 0x08B3457Cu, {self + 0x20u + 0x1Cu});
}

// 0x08922DF4 (a0 = this, a1 = position floats): builds an identity matrix with the position in its last row on the stack,
// 0x08B4E9D4(this + 0x10, matrix), then 0x08B4F3B0 / 0x08B4F2E4 / 0x08B4F47C(this + 0x10, f12 = position[4] / [3] / [5]).
void func_08922DF4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 camera = ctx.gpr[4] + 0x10u;
    const u32 source = ctx.gpr[5];
    const u32 matrix = frame.at(0);
    put_float(rt, matrix + 0x30u, float_at(rt, source));
    put_float(rt, matrix + 0x34u, float_at(rt, source + 4u));
    put_float(rt, matrix + 0x38u, float_at(rt, source + 8u));
    put_float(rt, matrix + 0x3Cu, 1.0f);
    put_float(rt, matrix, 1.0f);
    put_float(rt, matrix + 0x14u, 1.0f);
    put_float(rt, matrix + 0x28u, 1.0f);
    put(rt, matrix + 0x10u, 0u);
    put(rt, matrix + 0x20u, 0u);
    put(rt, matrix + 4u, 0u);
    put(rt, matrix + 0x24u, 0u);
    put(rt, matrix + 8u, 0u);
    put(rt, matrix + 0x18u, 0u);
    put(rt, matrix + 0xCu, 0u);
    put(rt, matrix + 0x1Cu, 0u);
    put(rt, matrix + 0x2Cu, 0u);
    call(rt, ctx, 0x08B4E9D4u, {camera, matrix});
    callf(rt, ctx, 0x08B4F3B0u, {camera}, {float_at(rt, source + 0x10u)});
    callf(rt, ctx, 0x08B4F2E4u, {camera}, {float_at(rt, source + 0xCu)});
    callf(rt, ctx, 0x08B4F47Cu, {camera}, {float_at(rt, source + 0x14u)});
}

// 0x08AFACCC (a0 = this, a1, a2, a3, t0, t1): virtual at +0x14 of this's vtable with all the arguments; this+0xC = a3;
// ok = 0x08B52870(this + 0x2C, a1, a2, a3, t0, t1) != 0; when ok, this+4 = a1, this+8 = a2 and the virtual at +0x34 is
// called with (this, 0, 0, a1, a2). v0 = ok (0 or 1).
void func_08AFACCC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x14u), {self, a1, a2, a3, t0, t1});
    put(rt, self + 0xCu, a3);
    const u32 ok = call(rt, ctx, 0x08B52870u, {self + 0x2Cu, a1, a2, a3, t0, t1}) != 0u ? 1u : 0u;
    if (ok != 0u) {
        put(rt, self + 4u, a1);
        put(rt, self + 8u, a2);
        call_indirect(rt, ctx, word(rt, word(rt, self) + 0x34u), {self, 0u, 0u, a1, a2});
    }
    ctx.gpr[2] = ok;
}

// 0x088ABBE4 (a0 = this): constructor. Floats 1.0 / 2.0 at +0x3C4 / +0x3C8, zeroed fields up to +0x43F; memset(this + 4, 0xBD, 0x48)
// then a tail call of memset(this + 0x4C, 0xBD, 0x48). (Two empty counting loops of the original are not reproduced.)
void func_088ABBE4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC2ED8u);
        put_float(rt, self + 0x3C4u, 1.0f);
        put_float(rt, self + 0x3C8u, 2.0f);
        put(rt, self + 0x3CCu, 0u);
        put(rt, self + 0x3D4u, 0u);
        put(rt, self + 0x3D8u, 0u);
        put(rt, self + 0x3DCu, 0u);
        put(rt, self + 0x3E8u, 0u);
        put(rt, self + 0x3ECu, 0u);
        put(rt, self + 0x3F8u, 0u);
        put(rt, self + 0x3FCu, 0u);
        put(rt, self + 0x418u, 0u);
        for (u32 offset = 0x438u; offset <= 0x43Du; ++offset) rt.memory().store8(self + offset, 0u);
        rt.memory().store8(self + 0x43Fu, 0u);
        call(rt, ctx, 0x08B36018u, {self + 4u, 0xBDu, 0x48u});
    }
    tail_call(ctx, 0x08B36018u, {self + 0x4Cu, 0xBDu, 0x48u});
}

// 0x08AB3258 (a0 = string, a1 = source): reads characters with 0x08AB2B50(source) until -1 and appends each to the
// copy-on-write string a0 (data pointer at +0: length -0xC, capacity -8, references -4); grows with 0x08B864EC(string, length + 1)
// when too small or shared; the terminator byte is the one at 0x08CB7430. v0 = the string.
void func_08AB3258(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 string = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    put(rt, string, 0x08D74FD0u + 0xCu);
    while (true) {
        const u32 character = call(rt, ctx, 0x08AB2B50u, {source});
        if (character == 0xFFFFFFFFu) break;
        ctx.gpr[4] = string;
        const u32 byte = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(character)));
        u32 data = word(rt, string);
        const u32 new_length = word(rt, data - 0xCu) + 1u;
        if (word(rt, data - 8u) < new_length || static_cast<s32>(word(rt, data - 4u)) > 0) {
            ctx.gpr[5] = new_length;
            call(rt, ctx, 0x08B864ECu, {string, new_length});
            data = word(rt, string);
        }
        const u32 length = word(rt, data - 0xCu);
        const u32 terminator = rt.memory().load8(0x08CB7430u);
        rt.memory().store8(data + length, static_cast<std::uint8_t>(byte));
        const u32 now = word(rt, string);
        put(rt, now - 4u, 0u);
        put(rt, now - 0xCu, new_length);
        rt.memory().store8(now + new_length, static_cast<std::uint8_t>(terminator));
    }
    ctx.gpr[2] = string;
}

// 0x088F9A58 (a0 = this, a1 = tag): constructor. this+4 = 0; a new 0x4C-byte object (vtable 0x08CC3F68, zeroed fields and a
// 3 x 12-byte block at +0x1C, 0x08B1C2BC(object + 0x40)) goes to this+8; a function-local static (guard 0x08D76138, slot
// 0x08D76148) is created on first use and its address stored at this+0xC; the tag goes to object+0x10. v0 = the object.
void func_088F9A58(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 tag = ctx.gpr[5];
    put(rt, self, 0x08CC3F38u);
    put(rt, self + 4u, 0u);
    const u32 object = call(rt, ctx, 0x08B62344u, {0x4Cu});
    put(rt, object, 0x08CC3F68u);
    put(rt, object + 4u, 0x08CC3FA0u);
    put(rt, object + 0x18u, 0x08CC3FBCu);
    put(rt, object + 8u, 0u);
    put(rt, object + 0xCu, 0u);
    put(rt, object + 0x10u, 0u);
    rt.memory().store8(object + 0x14u, 0u);
    u32 block = object + 0x1Cu;
    for (u32 index = 0; index < 3u; ++index) {
        put(rt, block, 0u);
        put(rt, block + 4u, 0u);
        put(rt, block + 8u, 0u);
        block += 0xCu;
    }
    call(rt, ctx, 0x08B1C2BCu, {object + 0x40u});
    put(rt, self + 8u, object);
    const u32 guard = 0x08D76138u;
    const u32 slot = 0x08D76148u;
    if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            if (slot != 0u) put(rt, slot, 0x08CC4060u);
            call(rt, ctx, 0x08B64494u, {guard});
        }
    }
    const u32 created = word(rt, self + 8u);
    put(rt, self + 0xCu, slot);
    put(rt, created + 0x10u, tag);
    ctx.gpr[2] = created;
}

// 0x08C52910 (a0 = quaternion x, y, z, w; a1, a2, a3 = outputs): *a2 = 0x08C5250C(-2 (xz - yw)); when 0x08B499B8(*a2) is zero *a1 = 0 and
// *a3 = 0x08B4A430(-(xy - zw), 0.5 - z^2 - x^2); else *a1 = 0x08B4A430(xw + yz, 0.5 - x^2 - y^2) and *a3 = 0x08B4A430(xy + zw, 0.5 - y^2 - z^2).
void func_08C52910(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 source = ctx.gpr[4];
    const u32 out3 = ctx.gpr[5];
    const u32 out1 = ctx.gpr[6];
    const u32 out2 = ctx.gpr[7];
    const float x = float_at(rt, source);
    const float y = float_at(rt, source + 4u);
    const float z = float_at(rt, source + 8u);
    const float w = float_at(rt, source + 0xCu);
    const float xz = psp_mul(x, z);
    const float yw = psp_mul(y, w);
    const float difference = xz - yw;
    const float scaled = psp_mul(difference, -2.0f);
    const float xy = psp_mul(x, y);
    const float xx = psp_mul(x, x);
    const float xw = psp_mul(x, w);
    const float zz = psp_mul(z, z);
    const float yy = psp_mul(y, y);
    const float yz = psp_mul(y, z);
    const float zw = psp_mul(z, w);
    const float half = 0.5f;
    const float first = callf(rt, ctx, 0x08C5250Cu, {}, {scaled});
    put_float(rt, out1, first);
    const float probe = callf(rt, ctx, 0x08B499B8u, {}, {first});
    if (probe == 0.0f) {
        put_float(rt, out3, 0.0f);
        const float a = half - zz;
        const float b = xy - zw;
        const float c = a - xx;
        const float result = callf(rt, ctx, 0x08B4A430u, {}, {-b, c});
        put_float(rt, out2, result);
    } else {
        const float sum = xw + yz;
        float c = half - xx;
        c = c - yy;
        const float middle = callf(rt, ctx, 0x08B4A430u, {}, {sum, c});
        put_float(rt, out3, middle);
        float d = half - yy;
        const float sum2 = xy + zw;
        d = d - zz;
        const float last = callf(rt, ctx, 0x08B4A430u, {}, {sum2, d});
        put_float(rt, out2, last);
    }
}

// 0x08B2F1EC (a0 = this): constructor of a large object: fields +4..+0x18, 0x08B28DCC(this + 0x8FC), fields up to +0x964C (-1 in
// +0xC / +0x12A8 / +0x9638 / +0x963C, 0x08B2F17C(0x4C00, 0) at +0x964C), memsets of +0x1C (0x80), +0xB0 (0x848), +0x928 (0x980),
// +0x12B8 (0x8300) and +0x95B8 (0x80); the global at 0x08D69160 = this. v0 = the last memset's result.
void func_08B2F1EC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CDA720u);
    put(rt, self + 0xA0u, 0u);
    put(rt, self + 0xA8u, 0u);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0xACu, 0u);
    rt.memory().store8(self + 6u, 1u);
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    rt.memory().store8(self + 4u, 0u);
    rt.memory().store8(self + 5u, 0u);
    rt.memory().store8(self + 7u, 0u);
    rt.memory().store8(self + 8u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x8F8u, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x8FCu});
    put(rt, self + 0x12B0u, 0u);
    put(rt, self + 0x12B4u, 0u);
    const u32 high = self + 0x10000u;
    put(rt, high - 0x69C0u, 0u);
    put(rt, high - 0x69BCu, 0u);
    put(rt, high - 0x69C4u, 0xFFFFFFFFu);
    put(rt, self + 0x12A8u, 0xFFFFFFFFu);
    put(rt, high - 0x69C8u, 0xFFFFFFFFu);
    rt.memory().store8(high - 0x69B8u, 0u);
    const u32 table = call(rt, ctx, 0x08B2F17Cu, {0x4C00u, 0u});
    put(rt, high - 0x69B4u, table);
    call(rt, ctx, 0x08B36018u, {self + 0x1Cu, 0u, 0x80u});
    call(rt, ctx, 0x08B36018u, {self + 0xB0u, 0u, 0x848u});
    call(rt, ctx, 0x08B36018u, {self + 0x928u, 0u, 0x980u});
    call(rt, ctx, 0x08B36018u, {self + 0x12B8u, 0u, 0x8300u});
    call(rt, ctx, 0x08B36018u, {self + 0x95B8u, 0u, 0x80u});
    put(rt, 0x08D69160u, self);
}

// 0x0893BCDC (a0 = object, a1 = out1, a2 = out2): two scaled, clamped factors from the floats at +0x54 and +0x34 (|x|): out1 =
// 0x08CF2A14[0] * clamp(f[+0x54] * k - 0.25, 0.2, 1) * clamp((|f[+0x34]| - 1.389) * m, 0, 1); out2 = the piecewise-linear curve
// of |f[+0x34]| over the table entries 0x08CF2A14[1..4] (values [2] .. [4]). v0 = 0x08CF2A14, f0 = out2.
void func_0893BCDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    float level = psp_mul(float_at(rt, object + 0x54u), bitf(0x39D5FEBFu));
    float low = level - bitf(0x3E800000u);
    const float floor_value = bitf(0x3E4CCCCDu);
    if (low < floor_value) low = floor_value;
    else if (1.0f < low) low = 1.0f;
    float speed = float_at(rt, object + 0x34u);
    if (speed < 0.0f) speed = -speed;
    float factor = speed - bitf(0x3FB1C71Du);
    factor = psp_mul(factor, bitf(0x3E1374BCu));
    if (factor < 0.0f) factor = 0.0f;
    else if (1.0f < factor) factor = 1.0f;
    float product = psp_mul(low, factor);
    const u32 table = 0x08CF2A14u;
    product = psp_mul(product, float_at(rt, table));
    put_float(rt, out1, product);
    const float begin = float_at(rt, table + 4u);
    float result;
    if (speed <= begin) {
        result = float_at(rt, table + 8u);
    } else {
        const float end = float_at(rt, table + 0xCu);
        if (end <= speed) {
            result = float_at(rt, table + 0x10u);
        } else {
            const float from = float_at(rt, table + 8u);
            const float to = float_at(rt, table + 0x10u);
            float offset = speed - begin;
            const float span = end - begin;
            const float range = to - from;
            offset = psp_mul(offset, range);
            offset = offset / span;
            result = from + offset;
        }
    }
    ctx.fpr[0] = result;
    ctx.gpr[2] = table;
    put_float(rt, out2, result);
}

// 0x089DC2C0 (a0 = out string, a2 = mode, a3 = pointer to an object pointer): a holder at sp+0x14 is built by 0x089DB3B4. Mode 0:
// a string-vector at sp+0x20 (0x08A98C1C(vec, 4)) gets its four elements set from the four floats the holder's 0x089DA8F4 gives,
// each through a temporary string 0x08A87EEC(tmp, f12 = float), assigned (0x08A5BAD8) to the element unless it is the temporary
// itself and destroyed (0x08A5B9EC); the result is assigned to the out string. Mode 1: the object *a3 reports a count with its
// virtual +0x7C; for each index below min(count, 4) the virtual +0x84 builds a string (sp) and the virtual +0x54 of that gives a
// float stored in the 4-float block at sp+0x20 (initially 1.0f, with -1 at +0x30); then 0x089DB588(holder, block). Other modes do
// nothing. v0 = the last call's result (the holder's destructor).
void func_089DC2C0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 objects = ctx.gpr[7];
    const u32 holder = frame.at(0x14);
    const u32 block = frame.at(0x20);
    call(rt, ctx, 0x089DB3B4u, {holder});
    if (mode == 0u) {
        call(rt, ctx, 0x08A98C1Cu, {block, 4u});
        u32 owner = word(rt, holder);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        const u32 values = call(rt, ctx, 0x089DA8F4u, {owner});
        for (u32 element = 0; element < 4u; ++element) {
            const u32 temporary = frame.at(0x10u - 4u * element);
            const float value = float_at(rt, values + 4u * element);
            const u32 destination = word(rt, word(rt, block) + 0x14u) + 4u * element;
            callf(rt, ctx, 0x08A87EECu, {temporary}, {value});
            if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
            call(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        if (out != block) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, block)});
        call(rt, ctx, 0x08A5B9ECu, {block});
    } else if (mode == 1u) {
        u32 object = word(rt, objects);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
        const u32 count = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x7Cu), {object});
        put_float(rt, frame.at(0x20), 1.0f);
        put(rt, frame.at(0x30), 0xFFFFFFFFu);
        put_float(rt, frame.at(0x2C), 1.0f);
        put_float(rt, frame.at(0x28), 1.0f);
        put_float(rt, frame.at(0x24), 1.0f);
        if (count != 0u) {
            u32 index = 0u;
            do {
                if (index == 4u) break;
                object = word(rt, objects);
                if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
                const u32 current = index;
                ++index;
                call_indirect(rt, ctx, word(rt, word(rt, object) + 0x84u), {frame.at(0), object, current});
                const u32 text = word(rt, frame.at(0));
                if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
                const GuestResult measured = call_indirect(rt, ctx, word(rt, word(rt, text) + 0x54u), {text}, {});
                put_float(rt, block + 4u * current, measured.f0);
                call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
            } while (index != count);
        }
        const u32 owner = word(rt, holder);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        call(rt, ctx, 0x089DB588u, {owner, block});
    }
    call(rt, ctx, 0x08A5B9ECu, {holder});
}

// 0x088E7BEC (a0 = this): two countdowns. The one at +0x14, when it reaches 0 (from above 0): looks up three entries in the table
// object *this (0x088DCD28 with +0x10, word +0x1C + 1 and word +0x1C), sets the first two up through their virtuals, 0x088E7A04(this, i),
// then computes two floats from 0x088DA788 / 0x088D61E0 / the first entry's virtual +0x14 (x 0.8) and passes them to 0x088E798C twice and
// 0x088D07F0, stores 0x78 at +0x18 and calls virtual +0x80 of the object at +8; with no second entry it returns at once. The
// countdown at +0x18, reaching 0 (from above 0): 0x088F8D58(*(*this + 0x14C), +0x10) .virtual(+0x1C)(.., 0), byte 1 at *(*this + 0x618), and a tail
// call of virtual +0x7C of the object at +8 with (it, word +0x10).
void func_088E7BEC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x30u);
        s32 first_counter = static_cast<s32>(word(rt, self + 0x14u));
        bool run_first = false;
        if (first_counter > 0) {
            --first_counter;
            put(rt, self + 0x14u, static_cast<u32>(first_counter));
            run_first = first_counter == 0;
        }
        if (run_first) {
            call(rt, ctx, 0x08917E80u, {word(rt, word(rt, self) + 0x5C8u), 0u, 0u, 0u});
            const u32 entry_index = word(rt, self + 0x1Cu);
            const u32 entry_next = entry_index + 1u;
            const u32 table_first = call(rt, ctx, 0x088DCD28u, {word(rt, self), word(rt, self + 0x10u)});
            const u32 second = call(rt, ctx, 0x088DCD28u, {word(rt, self), entry_next});
            const u32 third = call(rt, ctx, 0x088DCD28u, {word(rt, self), entry_index});
            if (second == 0u) {
                ctx.gpr[2] = third;
                return;
            }
            call_indirect(rt, ctx, word(rt, word(rt, second) + 0x50u), {second, 0u});
            call_indirect(rt, ctx, word(rt, word(rt, second) + 0x74u), {second, 1u});
            call_indirect(rt, ctx, word(rt, word(rt, second) + 0x5Cu), {second});
            call_indirect(rt, ctx, word(rt, word(rt, third) + 0x50u), {third, 1u});
            call_indirect(rt, ctx, word(rt, word(rt, third) + 0x74u), {third, 0u});
            call_indirect(rt, ctx, word(rt, word(rt, third) + 0x60u), {third});
            call(rt, ctx, 0x088E7A04u, {self, entry_index});
            const u32 current = call(rt, ctx, 0x088DA788u, {word(rt, self), word(rt, self + 0x10u)});
            const float base = float_at(rt, current);
            float extent = callf(rt, ctx, 0x088D61E0u, {word(rt, self) + 4u, entry_next}, {});
            if (extent == 0.0f) extent = bitf(0x42480000u);
            extent = extent + base;
            const GuestResult measured = call_indirect(rt, ctx, word(rt, word(rt, table_first) + 0x14u), {table_first}, {});
            const float scaled = psp_mul(measured.f0, bitf(0x3F4CCCCDu));
            callf(rt, ctx, 0x088E798Cu, {self, word(rt, self + 0x10u)}, {base, scaled});
            callf(rt, ctx, 0x088E798Cu, {self, entry_next}, {extent, scaled});
            const u32 next_entry = call(rt, ctx, 0x088DA788u, {word(rt, self), entry_next});
            const u32 value = word(rt, current + 0x18u);
            const u32 radius = call(rt, ctx, 0x088DB5C4u, {word(rt, self)});
            callf(rt, ctx, 0x088D07F0u, {next_entry, value, radius}, {extent});
            const u32 owner = word(rt, self + 8u);
            put(rt, self + 0x18u, 0x78u);
            call_indirect(rt, ctx, word(rt, word(rt, owner) + 0x80u), {owner, entry_next});
        }
        s32 second_counter = static_cast<s32>(word(rt, self + 0x18u));
        if (second_counter <= 0) {
            ctx.gpr[2] = static_cast<u32>(second_counter - 1);
            return;
        }
        --second_counter;
        put(rt, self + 0x18u, static_cast<u32>(second_counter));
        if (second_counter != 0) {
            ctx.gpr[2] = static_cast<u32>(second_counter);
            return;
        }
        const u32 player = word(rt, self);
        const u32 index = word(rt, self + 0x10u);
        const u32 found = call(rt, ctx, 0x088F8D58u, {word(rt, player + 0x14Cu), index});
        call_indirect(rt, ctx, word(rt, word(rt, found) + 0x1Cu), {found, 0u});
        rt.memory().store8(word(rt, word(rt, self) + 0x618u), 1u);
    }
    const u32 owner = word(rt, self + 8u);
    ctx.gpr[2] = word(rt, owner);
    tail_call(ctx, word(rt, word(rt, owner) + 0x7Cu), {owner, word(rt, self + 0x10u)});
}

// 0x08A2FE90 (a0 = out string, a2 = count, a3 = arguments: object pointers at +0 / +4 / +8): count <= 0 does nothing. The
// second and third arguments' virtuals (+0x3C, +0x30) give two numbers (0 and 1 by default). A string conversion through
// 0x08B308FC follows in one of the two shapes the first argument selects (0x08A76F20 against 0x08A443E0 / 0x08A6BF34): a
// string made by 0x08A449B8 or by the object's virtual +0xC, a formatting call 0x08B308FC and 0x08A97A6C into a temporary,
// which is assigned to the out string (0x08A5BAD8) and destroyed (0x08A5B9EC); with neither, an empty result (0x08A97A6C(tmp, 0)).
void func_08A2FE90(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    u32 arguments = ctx.gpr[7];
    if (count <= 0) return;
    u32 number_a = 0u;
    u32 number_b = 1u;
    if (count >= 2) {
        const u32 object = word(rt, arguments + 4u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
        number_a = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    }
    if (count >= 3) {
        const u32 object = word(rt, arguments + 8u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
        number_b = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object});
    }
    u32 first = word(rt, arguments);
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
    const u32 expected = call(rt, ctx, 0x08A443E0u);
    const u32 matches = call(rt, ctx, 0x08A76F20u, {first, expected});
    const u32 temp_a = frame.at(0x10);
    const u32 temp_b = frame.at(0xC);
    const u32 temp_c = frame.at(8);
    const u32 temp_d = frame.at(4);
    if (matches != 0u) {
        call(rt, ctx, 0x08A449B8u, {temp_a, arguments});
        const u32 formatter = call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
        const u32 first_text = word(rt, temp_a);
        u32 text = first_text;
        if (text == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CAC380u});
            text = word(rt, temp_a);
        }
        const u32 data = word(rt, first_text + 0x14u);
        if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC380u});
        const u32 length = word(rt, text + 0x18u);
        const u32 formatted = call(rt, ctx, 0x08B308FCu, {formatter, data, length, number_a, number_b, 0u});
        call(rt, ctx, 0x08A97A6Cu, {temp_b, formatted});
        if (out != temp_b) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, temp_b)});
        call(rt, ctx, 0x08A5B9ECu, {temp_b});
        call(rt, ctx, 0x08A5B9ECu, {temp_a});
        return;
    }
    first = word(rt, arguments);
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
    const u32 other = call(rt, ctx, 0x08A6BF34u);
    const u32 other_matches = call(rt, ctx, 0x08A76F20u, {first, other});
    if (other_matches == 0u) {
        call(rt, ctx, 0x08A97A6Cu, {temp_d, 0u});
        if (out != temp_d) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, temp_d)});
        call(rt, ctx, 0x08A5B9ECu, {temp_d});
        return;
    }
    first = word(rt, arguments);
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
    call_indirect(rt, ctx, word(rt, word(rt, first) + 0xCu), {temp_a, first});
    const u32 formatter = call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
    const u32 text = word(rt, temp_a);
    const u32 formatted = call(rt, ctx, 0x08B308FCu, {formatter, text, word(rt, text - 0xCu), number_a, number_b, 0u});
    call(rt, ctx, 0x08A97A6Cu, {temp_c, formatted});
    if (out != temp_c) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, temp_c)});
    call(rt, ctx, 0x08A5B9ECu, {temp_c});
    const u32 head = word(rt, temp_a) - 0xCu;
    if (head == 0x08D74FD0u) {
        ctx.gpr[2] = 0x08D74FD0u;
        return;
    }
    const s32 references = static_cast<s32>(call(rt, ctx, 0x08B60804u, {head + 8u, 0xFFFFFFFFu}));
    if (references > 0) return;
    call(rt, ctx, 0x08B8542Cu, {head, frame.at(0)});
}

} // namespace
} // namespace new_batch_main_120
} // namespace gt::game

#define NEWMAIN120(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_120::name, gt::ReturnKind::kind)
NEWMAIN120(0x0889D2A8u, func_0889D2A8, Word);
NEWMAIN120(0x088ABBE4u, func_088ABBE4, Word);
NEWMAIN120(0x088E7BECu, func_088E7BEC, Word);
NEWMAIN120(0x088F3790u, func_088F3790, Word);
NEWMAIN120(0x088F9A58u, func_088F9A58, Word);
NEWMAIN120(0x08922DF4u, func_08922DF4, Word);
NEWMAIN120(0x08934E38u, func_08934E38, Word);
NEWMAIN120(0x0893BCDCu, func_0893BCDC, Word);
NEWMAIN120(0x089DC2C0u, func_089DC2C0, Word);
NEWMAIN120(0x08A19C28u, func_08A19C28, Word);
NEWMAIN120(0x08A2FE90u, func_08A2FE90, Word);
NEWMAIN120(0x08A893A8u, func_08A893A8, Word);
NEWMAIN120(0x08AB3258u, func_08AB3258, Word);
NEWMAIN120(0x08ABD398u, func_08ABD398, Word);
NEWMAIN120(0x08AC1E18u, func_08AC1E18, Word);
NEWMAIN120(0x08AD7D5Cu, func_08AD7D5C, Word);
NEWMAIN120(0x08AE1044u, func_08AE1044, Word);
NEWMAIN120(0x08AFACCCu, func_08AFACCC, Word);
NEWMAIN120(0x08B01680u, func_08B01680, Word);
NEWMAIN120(0x08B0B748u, func_08B0B748, Word);
NEWMAIN120(0x08B2F1ECu, func_08B2F1EC, Word);
NEWMAIN120(0x08B38CF0u, func_08B38CF0, Word);
NEWMAIN120(0x08C52910u, func_08C52910, Float);
NEWMAIN120(0x08C52EE4u, func_08C52EE4, Float);
