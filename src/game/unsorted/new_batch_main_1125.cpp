// New batch main-1125 (draft, unverified): 30 functions, structured C from tools.disasm, in the style of new_batch_3.cpp.
// A register the asm passes on untouched is passed as the caller left it. v0 is the asm's v0 on every exit path. Callee-saved
// registers stay in C locals (never written to ctx), so the guest values survive as the asm restores them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1125 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// func_08A7593C / func_08A84890 / func_08A95050: a block of `bytes` is allocated; `fill` gets (block, a1, a2, a3); its result
// is stored at sp+0 and `last` gets (self, sp).
void alloc_fill_last(Runtime &rt, AllegrexContext &ctx, u32 bytes, u32 fill, u32 last) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const u32 result = call(rt, ctx, fill, {block, a1, a2, a3});
    put(rt, ctx.gpr[29], result);
    call(rt, ctx, last, {self, ctx.gpr[29]});
}

// func_08B0CC6C / func_08B0CCEC: the object at a0 gets its vtable, the 8-byte value at a1 and the byte a2; v0 = 0x08B0C9B8.
void build_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B0CBCCu, {self});
    put(rt, self, 0x08CD7B68u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    const u32 low = word(rt, source);
    const u32 high = word(rt, source + 4u);
    put_byte(rt, self + 0x14u, flag);
    put(rt, self + 0x28u, low);
    put(rt, self + 0x2Cu, high);
    put(rt, self + 0x18u, 0x08B0C9B8u);
    put_byte(rt, self + 0x30u, 0u);
    put(rt, self + 0x1Cu, 0u);
    ctx.gpr[2] = 0x08B0C9B8u;
}

// func_08BB1B9C / func_08BB1C2C: the buffer at a0 (its word +4 is the end) gives up the aligned span from a1 to a2; the rest is
// handled by `fill`. v0 = a1.
void shrink_buffer(Runtime &rt, AllegrexContext &ctx, u32 fill) {
    const u32 self = ctx.gpr[4];
    const u32 start = ctx.gpr[5];
    const u32 end = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 aligned = (end - start) & ~3u;
    const u32 remaining = word(rt, self + 4u) - end;
    call(rt, ctx, 0x08B79BACu, {start, end, remaining});
    call(rt, ctx, fill, {remaining + start, word(rt, self + 4u), ctx.gpr[29]});
    put(rt, self + 4u, word(rt, self + 4u) - aligned);
    ctx.gpr[2] = start;
}

// func_08A75C80: the (begin, end) pair at a1 sizes a block (aligned span) that is written into the record at a0; the callee
// gets (word+0 of a1, word+4 of a1, block, sp) and its result goes to the record's +4.
void fill_range_block(Runtime &rt, AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 end = word(rt, source + 4u);
    const u32 begin = word(rt, source);
    put(rt, dest + 4u, 0u);
    const u32 size = (end - begin) & ~3u;
    put(rt, dest, 0u);
    put(rt, dest + 8u, 0u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    const u32 top = block + size;
    put(rt, dest, block);
    put(rt, dest + 4u, block);
    const u32 first = word(rt, source);
    const u32 second = word(rt, source + 4u);
    put(rt, dest + 8u, top);
    const u32 result = call(rt, ctx, 0x08C250A0u, {first, second, block, ctx.gpr[29]});
    put(rt, dest + 4u, result);
    ctx.gpr[2] = result;
}

// func_08A7C054: the object at a0 answers two virtuals (slots 0x10C and 0x118) with a word at sp; v0 = that word != 0.
void virtual_flag(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x08A5D438u, {sp, a1});
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, sp, a1});
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x118u), {self, sp, a2});
    const u32 flag = word(rt, sp);
    call(rt, ctx, 0x08A5B9ECu, {sp});
    ctx.gpr[2] = (flag != 0u) ? 1u : 0u;
}

// func_08A7DB1C / func_08A7DC44: as virtual_flag, but the middle call is `fill` and the slot is sp+4; v0 = that word != 0.
void slot_virtual_flag(Runtime &rt, AllegrexContext &ctx, u32 fill) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 slot = sp + 4u;
    call(rt, ctx, 0x08A5D438u, {slot, a1});
    call(rt, ctx, fill, {sp, self, slot, a1});
    call(rt, ctx, 0x08A5B9ECu, {sp});
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x118u), {self, slot, a2});
    const u32 flag = word(rt, slot);
    call(rt, ctx, 0x08A5B9ECu, {slot});
    ctx.gpr[2] = (flag != 0u) ? 1u : 0u;
}

// func_08AC4570 / func_08AC6AA0 / func_08B17080: the base call on a0, then the vtable word `vtable` is stored at *a0; v0 = vtable.
void set_vtable_after(Runtime &rt, AllegrexContext &ctx, u32 base_call, u32 vtable) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, base_call, {self});
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// func_08AEFABC: a call that passes the caller's arguments through; v0 is its result.
void func_08AEFABC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AEFA5Cu);
}

// func_08AF4580: a float argument (f12) goes to a virtual on a1 (slot 0x0C), then to two calls and a virtual on a1 (slot 0x08).

// func_08B1F730: the record at a0+8 is refreshed around the two calls; the old word +0 is returned, the old word +4 goes to a3.
void func_08B1F730(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 inner = self + 8u;
    call(rt, ctx, 0x08B34D40u, {inner});
    const u32 first = word(rt, self);
    const u32 second = word(rt, self + 4u);
    put(rt, out, second);
    put(rt, self, a1);
    put(rt, self + 4u, a2);
    call(rt, ctx, 0x08B34D80u, {inner});
    ctx.gpr[2] = first;
}

// func_08B26BFC: the object at a0 takes the vtable, the byte a3, a call with a2 and -1, the counts, then a tail call.
void func_08B26BFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08B26A80u, {self});
        put(rt, self, 0x08CD9648u);
        put_byte(rt, self + 0x28u, flag);
        call(rt, ctx, 0x08B26B28u, {self, source, 0xFFFFFFFFu});
        put(rt, self + 0x1Cu, count - 1u);
        put(rt, self + 0x24u, self + 0x2Au);
        put(rt, self + 0x14u, count);
        put(rt, self + 0x18u, count);
        put(rt, self + 0x10u, 0u);
    }
    tail_call(ctx, 0x08B25968u, {self});
}

// func_08AFB39C / func_08AFB428: a base call on a0, the vtable at *a0, a call on a0+0x2C, then a tail call with the eight
// argument registers passed on (a4 and a5 are t0 and t1).
void vtable_then_tail(Runtime &rt, AllegrexContext &ctx, u32 base_call, u32 vtable, u32 inner_call, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, base_call, {self});
        put(rt, self, vtable);
        call(rt, ctx, inner_call, {self + 0x2Cu});
    }
    tail_call(ctx, tail, {self, a1, a2, a3, t0, t1});
}

// func_08B1A4F4: a new object (0x30 bytes) is stored at *a0 with a second block (0x10 bytes) at a0+4, then a tail call.

// func_08B1F0A0: the object from the call is dispatched: its vtable slot 0x08 is tail-called with the object.
void func_08B1F0A0(Runtime &rt, AllegrexContext &ctx) {
    u32 object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = call(rt, ctx, 0x08B1F028u);
    }
    const u32 target = word(rt, word(rt, object) + 8u);
    tail_call(ctx, target, {object});
}

// func_08B7B660: eight arguments (a0..a3, t0..t3) are copied into a frame block and passed on with a 0x08B7B5D0 callback.
void func_08B7B660(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    const u32 t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x90u);
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 0x7Cu, a3);
    put(rt, sp + 0x80u, t0);
    put(rt, sp + 0x84u, t1);
    put(rt, sp + 0x88u, t2);
    put(rt, sp + 0x8Cu, t3);
    put(rt, sp + 4u, a1);
    put(rt, sp + 0x14u, a1);
    put_half(rt, sp + 0x10u, 4u);
    const u32 first = call(rt, ctx, 0x08B35F60u, {a1});
    put(rt, sp + 0x18u, first);
    put(rt, sp + 0x24u, 0x08B7B5D0u);
    put(rt, sp + 0x08u, first);
    put(rt, sp + 0x34u, 0u);
    put(rt, sp + 0x48u, 0u);
    put(rt, sp, sp + 0x7Cu);
    call(rt, ctx, 0x08B7F220u, {a0, sp + 4u, a2, sp + 0x7Cu, sp + 0x7Cu});
}

// func_08C6FDE4: the record at a0 is set up (vtable, zeroed words), a base call on a0+0x1C, then its vtable and fields.
void func_08C6FDE4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CDBDF0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    call(rt, ctx, 0x08B5E0ECu, {self + 0x1Cu, a1});
    put(rt, self + 0x24u, 0xFFFFFFFFu);
    put(rt, self, 0x08CDBD28u);
    put(rt, self + 0x20u, a1);
    ctx.gpr[2] = 0x08CDBD28u;
}

// func_08B4F798: a float vector at a1 is negated into a0 (its length reciprocal scales the x/y/z part); f0 = the length.
void func_08B4F798(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 vector = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call_indirect(rt, ctx, 0x08B4A744u, {vector});
    const float length = ctx.fpr[0];
    if (length != 0.0f) {
        const float inverse = 1.0f / length;
        put_float(rt, out, -float_at(rt, vector));
        put_float(rt, out + 4u, -float_at(rt, vector + 4u));
        put_float(rt, out + 8u, -float_at(rt, vector + 8u));
        put(rt, out + 0xCu, word(rt, vector + 0xCu));
        call_indirect(rt, ctx, 0x08B4A6A0u, {out}, {inverse});
    }
    ctx.fpr[0] = length;
}

// func_08AA2D90: an object of 0x18 bytes is built at a fresh block, its vtable set, then used and freed; v0 = a0.
void func_08AA2D90(Runtime &rt, AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 slot = sp + 4u;
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A76ACCu, {object});
    put(rt, object, 0x08CB6B88u);
    call(rt, ctx, 0x08A6972Cu, {object + 0x14u, a1});
    put(rt, sp, object);
    call(rt, ctx, 0x08A76AB4u, {slot, sp});
    call(rt, ctx, 0x08A76ABCu, {result, slot});
    call(rt, ctx, 0x08A5B9ECu, {slot});
    ctx.gpr[2] = result;
}

// func_08AF639C: a float vector at a2 is copied to the stack and passed to two calls with the record at a0.
void func_08AF639C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 vector = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = ctx.gpr[29];
    const u32 answer = sp + 0xCu;
    call(rt, ctx, 0x08AF56D8u, {answer, a3, t0, t1});
    put(rt, sp, word(rt, vector));
    put(rt, sp + 4u, word(rt, vector + 4u));
    put(rt, sp + 8u, word(rt, vector + 8u));
    call(rt, ctx, 0x08B4BAA8u, {self, sp});
    call(rt, ctx, 0x08B4AA34u, {a1, a1, sp});
    call(rt, ctx, 0x08B4B858u, {self, answer});
}

// func_08AFB51C: a base call on a0, the vtable at *a0, a zero at a0+0xD4, then a tail call with seven arguments.
void func_08AFB51C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08AFB4E8u, {self});
        put(rt, self, 0x08CD78A0u);
        put(rt, self + 0xD4u, 0u);
    }
    tail_call(ctx, 0x08AFAF44u, {self, a1, a2, a3, t0, t1, t2});
}

// The fill of a block from a global count: the count (word at 0x08D4069C) gives a stack block of 4 bytes per word, aligned to 16.
// fill gets (block, a0, 0); the block's result is the one `after` gets as its second argument, with a1 last.
void vla_fill(Runtime &rt, AllegrexContext &ctx, u32 fill) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08AEB808u, {self});
    const u32 count = word(rt, 0x08D4069Cu);
    const u32 size = ((count << 2) + 0xFu) & ~0xFu;
    ctx.gpr[29] -= size;
    const u32 buffer = ctx.gpr[29];
    const u32 first = call(rt, ctx, fill, {buffer, self, 0u});
    call(rt, ctx, 0x08AEB82Cu, {ctx.gpr[4]});
    call(rt, ctx, 0x08ADFD8Cu, {buffer, first, a1});
}

void func_08ADFE20(Runtime &rt, AllegrexContext &ctx) { vla_fill(rt, ctx, 0x08ADE328u); }
void func_08ADFF74(Runtime &rt, AllegrexContext &ctx) { vla_fill(rt, ctx, 0x08ADE4ACu); }
void func_08B0CC6C(Runtime &rt, AllegrexContext &ctx) { build_object(rt, ctx); }
void func_08B0CCEC(Runtime &rt, AllegrexContext &ctx) { build_object(rt, ctx); }
void func_08A7C054(Runtime &rt, AllegrexContext &ctx) { virtual_flag(rt, ctx); }
void func_08AC4570(Runtime &rt, AllegrexContext &ctx) { set_vtable_after(rt, ctx, 0x08ABA0BCu, 0x08CD66B8u); }
void func_08AC6AA0(Runtime &rt, AllegrexContext &ctx) { set_vtable_after(rt, ctx, 0x08ABA0BCu, 0x08CD6740u); }
void func_08B17080(Runtime &rt, AllegrexContext &ctx) { set_vtable_after(rt, ctx, 0x08B26A80u, 0x08CD9188u); }
void func_08AFB39C(Runtime &rt, AllegrexContext &ctx) { vtable_then_tail(rt, ctx, 0x08AFB360u, 0x08CD77F0u, 0x08B52B48u, 0x08AFAD8Cu); }
void func_08AFB428(Runtime &rt, AllegrexContext &ctx) { vtable_then_tail(rt, ctx, 0x08AFB360u, 0x08CD77F0u, 0x08B52B48u, 0x08AFAD8Cu); }
void func_08A7DB1C(Runtime &rt, AllegrexContext &ctx) { slot_virtual_flag(rt, ctx, 0x08A7DA34u); }

}  // namespace
}  // namespace new_batch_main_1125
}  // namespace gt::game

#define NEW1125(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1125::name, gt::ReturnKind::kind)
NEW1125(0x08ADFE20u, func_08ADFE20, Word);
NEW1125(0x08ADFF74u, func_08ADFF74, Word);
NEW1125(0x08B0CC6Cu, func_08B0CC6C, Word);
NEW1125(0x08B0CCECu, func_08B0CCEC, Word);
NEW1125(0x08A7C054u, func_08A7C054, Word);
NEW1125(0x08AC4570u, func_08AC4570, Word);
NEW1125(0x08AC6AA0u, func_08AC6AA0, Word);
NEW1125(0x08AEFABCu, func_08AEFABC, Word);
NEW1125(0x08B17080u, func_08B17080, Word);
NEW1125(0x08B1F730u, func_08B1F730, Word);
NEW1125(0x08B26BFCu, func_08B26BFC, Word);
NEW1125(0x08AFB39Cu, func_08AFB39C, Word);
NEW1125(0x08AFB428u, func_08AFB428, Word);
NEW1125(0x08B1F0A0u, func_08B1F0A0, Word);
NEW1125(0x08B7B660u, func_08B7B660, Word);
NEW1125(0x08C6FDE4u, func_08C6FDE4, Word);
NEW1125(0x08A7DB1Cu, func_08A7DB1C, Word);
NEW1125(0x08B4F798u, func_08B4F798, Float);
NEW1125(0x08AA2D90u, func_08AA2D90, Word);
NEW1125(0x08AF639Cu, func_08AF639C, Word);
NEW1125(0x08AFB51Cu, func_08AFB51C, Word);
