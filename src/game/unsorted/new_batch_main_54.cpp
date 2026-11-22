// New batch, main session 54: short functions the recompiler emitted, none decompiled before, written by hand as structured C
// from `tools.disasm`. Most are thunks that fetch one or two words and jump or call on (the `lw a0,0(a0)` / vtable-slot
// family, the 0x08C7xxxx `a1 -= *a0; a2 -= a1` argument rebasings, the `a0 += vtable[-3]` this-adjusting forwarders), plus a
// few small initialisers and accessors. v0 is left as the original leaves it on every path, including a value loaded before
// a tail jump. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_54 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }

// `lw a0, 0(a0); lw v0, 0(a0); lw t9, slot(v0); jr t9` (the object is one indirection away): v0 = the vtable, t9 = the target.
void jump_through_slot(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object});
}
// The same with the target taken through v0 and the vtable left in v1 (the original's `lw v1,(a0); lw v0,slot(v1); move t9,v0`).
void jump_through_slot_v1(Runtime &rt, AllegrexContext &ctx, u32 slot, bool mask_a1) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {object, mask_a1 ? (ctx.gpr[5] & 0xFFu) : ctx.gpr[5]});
}
// `lw v0,0(a0); lw v0,-0xC(v0); addu a0,a0,v0; j target` (adjust this by the vtable's offset-to-top).
void adjust_this_then_jump(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    const u32 table = word(rt, self);
    const u32 delta = word(rt, table - 0xCu);
    ctx.gpr[2] = delta;
    tail_call(ctx, target, {self + delta});
}
// `v0 = *a0; a2 -= a1; a1 -= v0; j target` with an optional extra change of t0 before the jump.
void rebase_then_jump(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, self);
    ctx.gpr[2] = base;
    tail_call(ctx, target, {self, ctx.gpr[5] - base, ctx.gpr[6] - ctx.gpr[5]});
}
void rebase_t0_then_jump(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, self);
    ctx.gpr[2] = base;
    tail_call(ctx, target, {self, ctx.gpr[5] - base, ctx.gpr[6] - ctx.gpr[5], ctx.gpr[7], ctx.gpr[8] - ctx.gpr[7]});
}

// `j 0x0885F430` with a0 = *a0 in the delay slot; the three instructions after it are not reached from this entry.

// --- 24 bytes
// v0:v1 = the two words of entry a1 (8 bytes each) in the table a0 points to, starting at +0x38.
void func_0885BCA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 entry = (ctx.gpr[5] << 3) + word(rt, ctx.gpr[4]);
    ctx.gpr[5] = entry;
    ctx.gpr[2] = word(rt, entry + 0x38u);
    ctx.gpr[3] = word(rt, entry + 0x3Cu);
}
// Copies the half at a1 to a0, then continues with a0 + 4 and the word at a1 + 4; v0 = the original a0.

// --- 28 bytes
// The fifth argument (stack word 0) is 0 in this wrapper and 1 in the next one.

// --- 32 bytes
// A virtual call of slot 0x7C of the object one indirection away: (object, a1, 0, 0, 0, -1).
void func_08917918(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 result = call(rt, ctx, 0x08927E44u, {word(rt, ctx.gpr[4] + 0xCu)});
    ctx.gpr[2] = word(rt, result + 0x74u);
}
void func_08930E28(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08930D20u, {a0, a1, frame.at(0)});
    ctx.gpr[2] = signed_half(half_at(rt, frame.at(0xC)));
}
void func_08930E48(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08930D20u, {a0, a1, frame.at(0)});
    ctx.gpr[2] = byte_at(rt, frame.at(0x12));
}
// Clears the pointer word at 0x08CFFCB0 and passes its old value on, if it was set.
// f12 = 0.0 for the callee.
void func_08ADF910(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_full(rt, ctx, 0x08ADF880u, {}, {0.0f});
}
// Counts the nodes of the chain that starts at *(a0 + 4), linked through +8.
void func_08B1FDB0(Runtime &rt, AllegrexContext &ctx) {
    u32 node = word(rt, ctx.gpr[4] + 4u);
    u32 count = 0u;
    while (node != 0u) {
        node = word(rt, node + 8u);
        ++count;
    }
    ctx.gpr[3] = node;
    ctx.gpr[2] = count;
}

// --- 36 bytes
void func_0880B72C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 value = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08869230u, {object + 0x30u, value});
    ctx.gpr[2] = 1u;
}
void func_08890B20(Runtime &rt, AllegrexContext &ctx) {
    u32 value;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 address = call(rt, ctx, 0x08890AC0u);
        value = word(rt, address);
    }
    ctx.gpr[2] = __builtin_bswap32(value);
}
// Copies a float and a half from a parameter block (found through *(*a0 + 0x4C8)) to a1 and a2; the block is looked up twice.
void func_088A5A6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    u32 root = word(rt, holder);
    u32 block = word(rt, root + 0x4C8u);
    const float value = float_at(rt, block + 0x1B34u);
    ctx.fpr[0] = value;
    put_float(rt, ctx.gpr[5], value);
    root = word(rt, holder);
    block = word(rt, root + 0x4C8u);
    const u32 half = half_at(rt, block + 0x1B3Au);
    ctx.gpr[2] = root;
    ctx.gpr[3] = block;
    ctx.gpr[4] = half;
    put(rt, ctx.gpr[6], half);
}
void func_088CEF14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    put(rt, self + 0x0u, ctx.gpr[5]);
    put(rt, self + 0x4u, ctx.gpr[6]);
    put(rt, self + 0x8u, ctx.gpr[7]);
    put(rt, self + 0xCu, ctx.gpr[8]);
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0xA0u, 0u);
    put_byte(rt, self + 0xA4u, 0u);
    put_byte(rt, self + 0xA5u, 0u);
    tail_call(ctx, 0x088CEA34u, {self});
}
// Returns the int result of the callee as a float: f1 holds the integer's bits, f0 its conversion.
void func_088D61E0(Runtime &rt, AllegrexContext &ctx) {
    u32 value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call(rt, ctx, 0x088D61C4u);
    }
    ctx.fpr[1] = std::bit_cast<float>(value);
    ctx.fpr[0] = static_cast<float>(static_cast<s32>(value));
}
// Byte +4 = 0x42 (the rest of that word is cleared), vtable word at +0.
void func_08A68C18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    put_byte(rt, self + 4u, 0x42u);
    const u32 vtable = 0x08CD3610u;
    const u32 tag = word(rt, self + 4u) & 255u; // ins v1, zero, 8, 24 keeps the low byte
    put(rt, self, vtable);
    ctx.gpr[3] = tag;
    ctx.gpr[2] = vtable;
    put(rt, self + 4u, tag);
}
// Fills the half-words [a0, a1) with the half at a2; v0 = the end address (v0 untouched when the range is empty).
void func_08B8A6D8(Runtime &rt, AllegrexContext &ctx) {
    u32 cursor = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    const u32 value = half_at(rt, ctx.gpr[6]);
    ctx.gpr[3] = value;
    if (cursor != end) {
        do {
            put_half(rt, cursor, value);
            cursor += 2u;
        } while (end != cursor);
        ctx.gpr[2] = cursor;
        ctx.gpr[4] = cursor;
    }
}
// Stores one vtable word at +0xA4 of every 0xAC-byte element in [a0, a1); v0 = the vtable (0x004C0000 when the range is empty:
// the `lui` sits in the branch's delay slot).
void func_08BD2D24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, self);
    const u32 sum = base + ctx.gpr[5];
    ctx.gpr[2] = base;
    ctx.gpr[3] = sum;
    ctx.gpr[4] = sum;
    ctx.gpr[6] = self;
    if (base == 0u) return;
    put(rt, self, sum);
    tail_call(ctx, 0x08908F54u, {sum});
}

// --- 40 bytes
void func_0885C0E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4]);
    const u32 scaled = (ctx.gpr[5] << 6) - (ctx.gpr[5] << 3);
    const u32 base = word(rt, table);
    const u32 element = ((scaled << 3) - scaled) + base;
    ctx.gpr[7] = table;
    ctx.gpr[3] = scaled;
    ctx.gpr[4] = base;
    ctx.gpr[5] = ctx.gpr[5] << 3;
    ctx.gpr[2] = element;
    put(rt, element + 0x18u, ctx.gpr[6]);
}
void func_0885C110(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4]);
    const u32 scaled = (ctx.gpr[5] << 6) - (ctx.gpr[5] << 3);
    const u32 base = word(rt, table);
    const u32 element = ((scaled << 3) - scaled) + base;
    ctx.gpr[6] = table;
    ctx.gpr[3] = element;
    ctx.gpr[4] = base;
    ctx.gpr[5] = ctx.gpr[5] << 3;
    ctx.gpr[2] = word(rt, element + 0x18u);
}

} // namespace
} // namespace new_batch_main_54
} // namespace gt::game

#define NEW_MAIN54(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_54::name, gt::ReturnKind::kind)
NEW_MAIN54(0x0885BCA8u, func_0885BCA8, DoubleWord);
NEW_MAIN54(0x08917918u, func_08917918, Word);
NEW_MAIN54(0x08930E28u, func_08930E28, Word);
NEW_MAIN54(0x08930E48u, func_08930E48, Word);
NEW_MAIN54(0x08ADF910u, func_08ADF910, Word);
NEW_MAIN54(0x08B1FDB0u, func_08B1FDB0, Word);
NEW_MAIN54(0x0880B72Cu, func_0880B72C, Word);
NEW_MAIN54(0x08890B20u, func_08890B20, Word);
NEW_MAIN54(0x088A5A6Cu, func_088A5A6C, Word);
NEW_MAIN54(0x088CEF14u, func_088CEF14, Word);
NEW_MAIN54(0x088D61E0u, func_088D61E0, Float);
NEW_MAIN54(0x08A68C18u, func_08A68C18, Word);
NEW_MAIN54(0x08B8A6D8u, func_08B8A6D8, Word);
NEW_MAIN54(0x08BD2D24u, func_08BD2D24, Word);
NEW_MAIN54(0x0885C0E8u, func_0885C0E8, Word);
NEW_MAIN54(0x0885C110u, func_0885C110, Word);
