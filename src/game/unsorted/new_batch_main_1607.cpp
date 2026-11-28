// New batch main-1607 (draft only, unverified): small functions, structured C from tools.disasm (relocations applied,
// branches shown as `-> ADDRESS`). Families in this file:
//  - one guest call with `a0` passed through, or loaded from the object in the delay slot, then `v0` returned;
//  - the same, then the object returned in `v0` (the original's `addu s0,a0,zero` / `addu v0,s0,zero` pair);
//  - a call, then a tail call (`j`) after the frame is popped, with `a0` = the object;
//  - index-scaled accessors on a table reached through `word(a0)` (the byte at +8 signed, 384 bytes per record);
//  - a tagged-object function (tag byte, vtable word, tail call), and one spill of a2..t3 into the caller's home area.
// Delay slots run before the call / branch. Registers the asm leaves behind (a0, a1, a2, v1, at, f1, f2) are set as the
// asm leaves them. Names: none has evidence; the functions keep their addresses.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1607 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s16 = std::int16_t;
using s8 = std::int8_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Allegrex `wsbh`: swap the bytes inside each halfword.
u32 swap_bytes_in_halfwords(u32 value) {
    return ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu);
}

// a0 = word(a0 + offset) in the delay slot, then `target` runs; the result is the callee's v0.
void call_with_member(Runtime &rt, AllegrexContext &ctx, u32 target, u32 offset) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target, {word(rt, self + offset)});
}

// One call with a0 passed through unchanged; the result is the callee's v0.
void call_with_self(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target, {self});
}

// One call with a0 passed through, then v0 = the object (the original keeps it in s0 across the call).
void call_and_return_self(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target, {self});
    ctx.gpr[2] = self;
}

// One call with a0 passed through, then a tail call to `tail` with a0 = the object. The frame is popped before the jump.
void call_then_tail_call(Runtime &rt, AllegrexContext &ctx, u32 target, u32 tail) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, target, {self});
    }
    ctx.gpr[4] = self;
    tail_call(ctx, tail, {self});
}

// --- index-scaled accessors ---------------------------------------------------------------------------------------
// The table is `word(a0)`; its signed byte at +8 selects record `index`, 384 bytes each (index << 9 minus index << 7).
struct Record {
    u32 table;
    u32 record;
    u32 index;
};
Record locate_record(Runtime &rt, u32 self) {
    const u32 table = word(rt, self);
    const u32 index = static_cast<u32>(static_cast<s32>(static_cast<s8>(rt.memory().load8(table + 8u))));
    const u32 record = table + (index << 9) - (index << 7);
    return Record{table, record, index};
}
// After the accessor's last instruction a0 = record and v1 = index.
void leave_record(AllegrexContext &ctx, const Record &r) {
    ctx.gpr[4] = r.record;
    ctx.gpr[3] = r.index;
}
// Store of a1 as a halfword at the record; v0 = table.
void store_record_half(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    rt.memory().store16(r.record + offset, static_cast<std::uint16_t>(ctx.gpr[5]));
    ctx.gpr[2] = r.table;
}
// Store of a1 as a byte at the record; v0 = table.
void store_record_byte(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    rt.memory().store8(r.record + offset, static_cast<std::uint8_t>(ctx.gpr[5]));
    ctx.gpr[2] = r.table;
}

// --- functions ----------------------------------------------------------------------------------------------------

// 0x088D5A8C (a0 = object holding a pointer at +0, a1 = a requested count): counts of 5 or more become 4; the count
// (byte-swapped inside its halfwords, left in a1) is stored as a halfword at that pointer + 0x18; v0 = 4, v1 = (count < 5).
void func_088D5A8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 requested = ctx.gpr[5];
    const u32 below_five = requested < 5u ? 1u : 0u;
    const u32 count = below_five != 0u ? requested : 4u;
    const u32 swapped = swap_bytes_in_halfwords(count);
    ctx.gpr[6] = object;
    ctx.gpr[3] = below_five;
    ctx.gpr[5] = swapped;
    ctx.gpr[2] = 4u;
    rt.memory().store16(object + 0x18u, static_cast<std::uint16_t>(swapped));
}

// 0x089833AC (a0 = object): f0 = (float at +0xA0 + 21.0) * float at +0xC0; at = 0x41A80000 (21.0), f1 = 21.0, f2 = scale.
void func_089833AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float base = float_at(rt, self + 0xA0u);
    const float scale = float_at(rt, self + 0xC0u);
    ctx.gpr[1] = 0x41A80000u;
    ctx.fpr[1] = 21.0f;
    ctx.fpr[2] = scale;
    ctx.fpr[0] = psp_mul(base + 21.0f, scale);
}

// 0x08A4D2D4: 0x08A50758(word(a0 + 0x78)).
void func_08A4D2D4(Runtime &rt, AllegrexContext &ctx) { call_with_member(rt, ctx, 0x08A50758u, 0x78u); }
// 0x08AC3308: 0x08AC31B4(word(a0)).
void func_08AC3308(Runtime &rt, AllegrexContext &ctx) { call_with_member(rt, ctx, 0x08AC31B4u, 0x0u); }
// 0x08B1A06C: 0x08B1A02C(word(a0)).
// 0x08B779F0: 0x08C7A838(a0).
// 0x08B77AAC: 0x08C7AB10(a0).
// 0x08B77AC8: 0x08C7AB00(a0).
// 0x08C314BC: 0x08AA29A8(a0).
void func_08C314BC(Runtime &rt, AllegrexContext &ctx) { call_with_self(rt, ctx, 0x08AA29A8u); }

// 0x0885E81C (a0 = object, word(a0) = inner object): 0x088DB810(word(inner + 0x5AC)); the delay slot reads the inner object from v0.
void func_0885E81C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = object;
    call(rt, ctx, 0x088DB810u, {word(rt, object + 0x5ACu)});
}

// 0x0887F7C8: 0x08886DE0(word(a0 + 0x74) + 4).
void func_0887F7C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 member = word(rt, ctx.gpr[4] + 0x74u);
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08886DE0u, {member + 4u});
}

// 0x088A62C4: signed halfword at +0x192 of the record.
void func_088A62C4(Runtime &rt, AllegrexContext &ctx) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(static_cast<s16>(rt.memory().load16(r.record + 0x192u))));
}
// 0x088A62E4: a1 stored as the halfword at +0x192; v0 = table.
void func_088A62E4(Runtime &rt, AllegrexContext &ctx) { store_record_half(rt, ctx, 0x192u); }
// 0x088A6304: unsigned halfword at +0x190.
void func_088A6304(Runtime &rt, AllegrexContext &ctx) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    ctx.gpr[2] = rt.memory().load16(r.record + 0x190u);
}
// 0x088A6324: a1 stored as the halfword at +0x190; v0 = table.
void func_088A6324(Runtime &rt, AllegrexContext &ctx) { store_record_half(rt, ctx, 0x190u); }
// 0x088A6344: unsigned byte at +0x15A.
void func_088A6344(Runtime &rt, AllegrexContext &ctx) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    ctx.gpr[2] = rt.memory().load8(r.record + 0x15Au);
}
// 0x088A63D4: unsigned byte at +0x198.
void func_088A63D4(Runtime &rt, AllegrexContext &ctx) {
    const Record r = locate_record(rt, ctx.gpr[4]);
    leave_record(ctx, r);
    ctx.gpr[2] = rt.memory().load8(r.record + 0x198u);
}
// 0x088A63F4: a1 stored as the byte at +0x198; v0 = table.
void func_088A63F4(Runtime &rt, AllegrexContext &ctx) { store_record_byte(rt, ctx, 0x198u); }

// 0x08A5BD34: stores a2..t3 at sp+8..sp+0x1C (the caller's home area); no result.
void func_08A5BD34(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0x8u), ctx.gpr[6]);
    put(rt, frame.at(0xCu), ctx.gpr[7]);
    put(rt, frame.at(0x10u), ctx.gpr[8]);
    put(rt, frame.at(0x14u), ctx.gpr[9]);
    put(rt, frame.at(0x18u), ctx.gpr[10]);
    put(rt, frame.at(0x1Cu), ctx.gpr[11]);
}

// 0x08AA60AC: 0x08AA5DE4(a0); v0 = a0.
void func_08AA60AC(Runtime &rt, AllegrexContext &ctx) { call_and_return_self(rt, ctx, 0x08AA5DE4u); }
// 0x08AA60D4: 0x08AA5E24(a0); v0 = a0.
// 0x08AA60FC: 0x08AA5E24(a0); v0 = a0.
void func_08AA60FC(Runtime &rt, AllegrexContext &ctx) { call_and_return_self(rt, ctx, 0x08AA5E24u); }
// 0x08AA6150: 0x08AA5E24(a0); v0 = a0.

// 0x08AC735C: 0x08AC7140(a0); then 0x08B28DC4(a0) as a tail call.
void func_08AC735C(Runtime &rt, AllegrexContext &ctx) { call_then_tail_call(rt, ctx, 0x08AC7140u, 0x08B28DC4u); }
// 0x08ACC980: 0x08ACC8C0(a0); then 0x08ACC944(a0) as a tail call.
// 0x08ACC9A8: 0x08ACC8D8(a0); then 0x08ACC944(a0) as a tail call.

// 0x08B2DE6C: 0x08B2E05C(a0); its v0 and v1 are stored at the globals 0x08D68D68 and 0x08D68D6C; a0 = 0x00090000 after.

// 0x08A66AD0 (a0 = object): tag byte +4 = 0x22, vtable 0x08CD2DE0 at +0 (v0 and v1 = object), then the word +4 is
// reloaded with its upper 24 bits cleared (`ins a2, zero, 8, 24`) and stored back (in the jump's delay slot); the
// tail call goes to func_08A6972C with a0 = object + 8.
void func_08A66AD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    rt.memory().store8(self + 0x4u, static_cast<std::uint8_t>(0x22u));
    ctx.gpr[2] = 0x08CD2DE0u;
    put(rt, self, 0x08CD2DE0u);
    ctx.gpr[3] = self;
    const u32 cleared = static_cast<std::uint8_t>(word(rt, self + 0x4u));
    ctx.gpr[6] = cleared;
    ctx.gpr[4] = self + 0x8u;
    put(rt, self + 0x4u, cleared);
    tail_call(ctx, 0x08A6972Cu, {self + 0x8u});
}

} // namespace
} // namespace new_batch_main_1607
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1607::name, gt::ReturnKind::kind)
NEWM(0x088D5A8Cu, func_088D5A8C, Word);
NEWM(0x089833ACu, func_089833AC, Float);
NEWM(0x08A4D2D4u, func_08A4D2D4, Word);
NEWM(0x08AC3308u, func_08AC3308, Word);
NEWM(0x08C314BCu, func_08C314BC, Word);
NEWM(0x0885E81Cu, func_0885E81C, Word);
NEWM(0x0887F7C8u, func_0887F7C8, Word);
NEWM(0x088A62C4u, func_088A62C4, Word);
NEWM(0x088A62E4u, func_088A62E4, Word);
NEWM(0x088A6304u, func_088A6304, Word);
NEWM(0x088A6324u, func_088A6324, Word);
NEWM(0x088A6344u, func_088A6344, Word);
NEWM(0x088A63D4u, func_088A63D4, Word);
NEWM(0x088A63F4u, func_088A63F4, Word);
NEWM(0x08A5BD34u, func_08A5BD34, Word);
NEWM(0x08AA60ACu, func_08AA60AC, Word);
NEWM(0x08AA60FCu, func_08AA60FC, Word);
NEWM(0x08AC735Cu, func_08AC735C, Word);
NEWM(0x08A66AD0u, func_08A66AD0, Word);
