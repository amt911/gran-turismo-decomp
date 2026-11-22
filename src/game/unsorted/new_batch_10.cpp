// New batch 10: small functions the extra seeds made emittable, written by hand from `tools.disasm` (relocations applied,
// branches shown as `-> ADDRESS`). Families in this file:
//  - one guest call whose argument the original loads in the `jal`'s delay slot, then the callee's `v0` returned
//    (leaf thunks) or a `j` with the frame popped first (tail calls);
//  - index-scaled accessors on a table reached through `word(a0)` (the byte at +8 sign-extended, times 384);
//  - short accessors/stores of fixed offsets.
// Delay slots are read as "before the call / branch". Callee-saved registers the original saves are put back at the end;
// `v0` (or `f0`) ends as the asm leaves it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_10 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s8 = std::int8_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// Allegrex `wsbh`: swap the bytes inside each halfword.
u32 swap_bytes_in_halfwords(u32 value) {
    return ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu);
}

// --- one call, the argument in the delay slot --------------------------------------------------------------
// a0 = word(a0 + offset), then `target` runs; the result is the callee's v0.
void call_with_member(Runtime &rt, AllegrexContext &ctx, u32 target, u32 offset) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target, {word(rt, self + offset)});
}
void call_with_self(Runtime &rt, AllegrexContext &ctx, u32 target) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target);
}

// 0x088D5A8C (a0 = object holding a pointer at +0, a1 = a requested count): counts of 5 or more become 4; the count
// (byte-swapped inside its halfwords) is stored as a halfword at that pointer + 0x18; v0 = 4.
void func_088D5A8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 requested = ctx.gpr[5];
    const u32 count = requested < 5u ? requested : 4u;
    ctx.gpr[2] = 4u;
    rt.memory().store16(object + 0x18u, static_cast<std::uint16_t>(swap_bytes_in_halfwords(count)));
}

// 0x089833AC (a0 = object): f0 = (float at +0xA0 + 21.0) * float at +0xC0.
void func_089833AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float base = float_at(rt, self + 0xA0u);
    const float scale = float_at(rt, self + 0xC0u);
    ctx.fpr[0] = psp_mul(base + 21.0f, scale);
}

// 0x08A4D2D4: 0x08A50758(word(a0 + 0x78)).
void func_08A4D2D4(Runtime &rt, AllegrexContext &ctx) { call_with_member(rt, ctx, 0x08A50758u, 0x78u); }
// 0x08AC3308: 0x08AC31B4(word(a0)).
void func_08AC3308(Runtime &rt, AllegrexContext &ctx) { call_with_member(rt, ctx, 0x08AC31B4u, 0x0u); }
// 0x08C314BC: 0x08AA29A8(a0).
void func_08C314BC(Runtime &rt, AllegrexContext &ctx) { call_with_self(rt, ctx, 0x08AA29A8u); }
// 0x0885E81C: 0x088DB810(word(word(a0) + 0x5AC)).
void func_0885E81C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088DB810u, {word(rt, object + 0x5ACu)});
}
// 0x0887F7C8: 0x08886DE0(word(a0 + 0x74) + 4).
void func_0887F7C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 member = word(rt, ctx.gpr[4] + 0x74u);
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08886DE0u, {member + 4u});
}


// --- index-scaled accessors ---------------------------------------------------------------------------------
// The table is `word(a0)`; its signed byte at +8 selects record `index`, 384 bytes each (index << 9 minus index << 7).
u32 indexed_record(Runtime &rt, u32 self) {
    const u32 table = word(rt, self);
    const s8 index = static_cast<s8>(rt.memory().load8(table + 8u));
    return table + static_cast<u32>(static_cast<s32>(index)) * 384u;
}
// 0x088A62C4: signed halfword at +0x192.
void func_088A62C4(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<u32>(signed_half_at(rt, indexed_record(rt, ctx.gpr[4]) + 0x192u));
}
// 0x088A62E4: a1 stored as the halfword at +0x192; v0 = word(a0).
void func_088A62E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4]);
    const s8 index = static_cast<s8>(rt.memory().load8(table + 8u));
    const u32 record = table + static_cast<u32>(static_cast<s32>(index)) * 384u;
    rt.memory().store16(record + 0x192u, static_cast<std::uint16_t>(ctx.gpr[5]));
    ctx.gpr[2] = table;
}
// 0x088A6304: unsigned halfword at +0x190.
void func_088A6304(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load16(indexed_record(rt, ctx.gpr[4]) + 0x190u);
}
// 0x088A6324: a1 stored as the halfword at +0x190; v0 = word(a0).
void func_088A6324(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4]);
    const s8 index = static_cast<s8>(rt.memory().load8(table + 8u));
    const u32 record = table + static_cast<u32>(static_cast<s32>(index)) * 384u;
    rt.memory().store16(record + 0x190u, static_cast<std::uint16_t>(ctx.gpr[5]));
    ctx.gpr[2] = table;
}
// 0x088A6344: byte at +0x15A.
void func_088A6344(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(indexed_record(rt, ctx.gpr[4]) + 0x15Au);
}
// 0x088A63D4: byte at +0x198.
void func_088A63D4(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(indexed_record(rt, ctx.gpr[4]) + 0x198u);
}
// 0x088A63F4: a1 stored as the byte at +0x198; v0 = word(a0).
void func_088A63F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4]);
    const s8 index = static_cast<s8>(rt.memory().load8(table + 8u));
    const u32 record = table + static_cast<u32>(static_cast<s32>(index)) * 384u;
    rt.memory().store8(record + 0x198u, static_cast<std::uint8_t>(ctx.gpr[5]));
    ctx.gpr[2] = table;
}

} // namespace
} // namespace new_batch_10
} // namespace gt::game

#define NEW10(address, name, kind) GT_DECOMP(address, gt::game::new_batch_10::name, gt::ReturnKind::kind)
NEW10(0x088D5A8Cu, func_088D5A8C, Word);
NEW10(0x089833ACu, func_089833AC, Float);
NEW10(0x08A4D2D4u, func_08A4D2D4, Word);
NEW10(0x08AC3308u, func_08AC3308, Word);
NEW10(0x08C314BCu, func_08C314BC, Word);
NEW10(0x0885E81Cu, func_0885E81C, Word);
NEW10(0x0887F7C8u, func_0887F7C8, Word);
NEW10(0x088A62C4u, func_088A62C4, Word);
NEW10(0x088A62E4u, func_088A62E4, Word);
NEW10(0x088A6304u, func_088A6304, Word);
NEW10(0x088A6324u, func_088A6324, Word);
NEW10(0x088A6344u, func_088A6344, Word);
NEW10(0x088A63D4u, func_088A63D4, Word);
NEW10(0x088A63F4u, func_088A63F4, Word);
