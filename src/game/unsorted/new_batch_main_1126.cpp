// Main-session batch 1126 (drafted, unverified). Structured C from tools.disasm: named locals, delay slots before the call or
// jump, v0 as the asm leaves it on every exit path, tail calls with tail_call. Functions that touch a stack-resident object
// (a0 = sp) or FP registers are left out of this batch.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1126 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// Calls a guest function with the arguments listed (a0..a3, then t0..t3). The result is the callee's, as after the `jal`:
// v0, v1 (high word of a 64-bit answer) and $a1 as the callee left it.
GuestResult invoke(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    return result;
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Constructor-shaped: the base constructor first (a0 unchanged), then this->vtable and this+0xD4 = 0, then the rest of the
// construction as a tail call with the seven arguments unchanged except a0 = this.
void func_08AFB5B4_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    invoke(rt, ctx, 0x08AFB4E8u, {self, a1, a2, a3});
    put(rt, self, 0x08CD78A0u);
    put(rt, self + 0xD4u, 0u);
    tail_call(ctx, 0x08AFAF44u, {self, a1, a2, a3, t0, t1, t2});
}

// Stores a word at +0x38 (delay slot before the call), then the rest as a tail call with a0 = this+4.
void func_08B23EF4_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    invoke(rt, ctx, 0x08B344E0u, {self + 4u});
    put(rt, self + 0x38u, a1);
    tail_call(ctx, 0x08B3457Cu, {self + 4u});
}

// Shared by 0x08B23BF0 and 0x08B23C8C (identical bodies, different vtable): vtable, two sub-constructors, a stored word,
// a 64-bit answer added to (a2:a3) with carry, stored at +0x40/+0x44, then a tail call.
void object_then_sum_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    put(rt, self, vtable);
    invoke(rt, ctx, 0x08B28DCCu, {self + 4u});
    invoke(rt, ctx, 0x08C4FBF4u, {self + 0x2Cu, self});
    put(rt, self + 0x38u, a1);
    const GuestResult span = invoke(rt, ctx, 0x08B2E05Cu, {ctx.gpr[4], ctx.gpr[5]});
    const u32 low = span.v0 + a2;
    const u32 carry = low < span.v0 ? 1u : 0u;
    const u32 high = span.v1 + a3 + carry;
    put(rt, self + 0x40u, low);
    put(rt, self + 0x44u, high);
    tail_call(ctx, 0x08B1F9B4u, {0x08D68C8Cu, self + 0x2Cu});
}
void func_08B23BF0_body(Runtime &rt, AllegrexContext &ctx) { object_then_sum_tail(rt, ctx, 0x08CD95E0u); }
void func_08B23C8C_body(Runtime &rt, AllegrexContext &ctx) { object_then_sum_tail(rt, ctx, 0x08CD95E0u); }

// Shared by 0x08A99AB4 and 0x08A9A5F8 (identical bodies): base call, vtable 0x08CB7108, then the source range
// [begin, end) at src+0x14/+0x18 is copied into a new block of (end - begin) & ~3 bytes. The block's three pointers
// (begin, end-of-block, copy end) are written around the two allocator calls and the copy.
void copy_range_into_block(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    invoke(rt, ctx, 0x08A782D8u, {self});
    put(rt, self, 0x08CB7108u);
    const u32 begin = word(rt, src + 0x14u);
    const u32 end = word(rt, src + 0x18u);
    const u32 size = (end - begin) & ~3u;
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x1Cu, 0u);
    const u32 block = invoke(rt, ctx, 0x08A5BC20u, {size, 4u}).v0;
    put(rt, self + 0x14u, block);
    put(rt, self + 0x18u, block);
    put(rt, self + 0x1Cu, block + size);
    const u32 copied = invoke(rt, ctx, 0x08C2DC4Cu,
                              {word(rt, src + 0x14u), word(rt, src + 0x18u), block, frame.at(0)}).v0;
    put(rt, self + 0x18u, copied);
    put8(rt, self + 0x20u, 0u);
}
void func_08A99AB4_body(Runtime &rt, AllegrexContext &ctx) { copy_range_into_block(rt, ctx); }
void func_08A9A5F8_body(Runtime &rt, AllegrexContext &ctx) { copy_range_into_block(rt, ctx); }

// Initialises the 0x25xx block of a big object: two sub-constructors, the flag bytes, 1.0f at +0x25A4/+0x259C, the array
// constructor at +0x25C0 (from +0x6A00, count 2), then a 0x400-byte clear at +0x5C whose answer is the result.
void func_08AC6704_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    invoke(rt, ctx, 0x08B28DCCu, {self + 8u});
    invoke(rt, ctx, 0x08B28D34u, {self + 0x30u});
    invoke(rt, ctx, 0x08AC646Cu, {self + 0x460u});
    put8(rt, self + 0x25B1u, 0u);
    put8(rt, self + 0x25B2u, 0u);
    put(rt, self + 0x25A4u, 0x3F800000u);
    put8(rt, self + 0x25B3u, 0u);
    put(rt, self + 0x259Cu, 0x3F800000u);
    put(rt, self + 0x2598u, 0u);
    put(rt, self + 0x25A0u, 0u);
    put(rt, self + 0x25A8u, 0u);
    put(rt, self + 0x25ACu, 0u);
    put8(rt, self + 0x25B0u, 0u);
    put8(rt, self + 0x25B4u, 0u);
    put(rt, self + 0x25B8u, 0u);
    put(rt, self + 0x25BCu, 0u);
    invoke(rt, ctx, 0x08C3CC64u, {self + 0x25C0u, self + 0x6A00u, 2u});
    put(rt, self + 0x25E0u, 0xFFFFFFFFu);
    put8(rt, self + 0x25DCu, 0u);
    put(rt, self + 0x25E4u, 0u);
    const u32 cleared = invoke(rt, ctx, 0x08B36018u, {self + 0x5Cu, 0u, 0x400u}).v0;
    put(rt, self + 4u, 0u);
    put(rt, self, 0u);
    ctx.gpr[2] = cleared;
}

// Sub-constructor, a stored word, then a tail call with a0 = this+0x98, a1 = whatever the last callee left in $a1.
void func_08B1BFD8_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    invoke(rt, ctx, 0x08B28DCCu, {self});
    invoke(rt, ctx, 0x08B1BB00u, {self + 0x28u});
    invoke(rt, ctx, 0x08B1BC10u, {self + 0x78u});
    const u32 block = self + 0x98u;
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    invoke(rt, ctx, 0x08B1AFB8u, {block});
    tail_call(ctx, 0x08B1AFF4u, {block, ctx.gpr[5], 0x411Au, 0u});
}

// One call with (this, vtable-free string constant), then a tail call with a1 unchanged.
void func_08A59828_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    invoke(rt, ctx, 0x08A52AB0u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08A5973Cu, {self, a1});
}

// Allocates a 0x44-byte object, constructs it from a0's word, stores it at +0, allocates a 0x10-byte record that points at
// it, links the record at +4 and returns this.
void func_08AC2168_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 obj = invoke(rt, ctx, 0x08B62344u, {0x44u}).v0;
    invoke(rt, ctx, 0x08AC1EACu, {obj, a1});
    put(rt, out, obj);
    const u32 record = invoke(rt, ctx, 0x08B62344u, {0x10u}).v0;
    put(rt, record + 4u, 1u);
    put(rt, record + 0xCu, obj);
    put(rt, record, 0x08CD65D8u);
    put(rt, record + 8u, 1u);
    put(rt, out + 4u, record);
    invoke(rt, ctx, 0x08B8E380u, {out + 4u, obj, obj});
    ctx.gpr[2] = out;
}

// Calls a base, then clears and fills the list-like header at +0x14..+0x20, a 0x18-byte block at +0x20, and ends with a
// tail call with a0 = this+0x24 and a1 = 10.
void func_08A62B98_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    invoke(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCB980u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    const u32 first = invoke(rt, ctx, 0x08A5CB28u, {ctx.gpr[4]}).v0;
    put(rt, self + 0x20u, first);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    const u32 second = invoke(rt, ctx, 0x08A72818u, {0u, 0u, 0u, 1u}).v0;
    put(rt, self + 0x3Cu, second);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    invoke(rt, ctx, 0x08A76AA0u, {self + 0x4Cu});
    put(rt, self + 0x50u, 0u);
    invoke(rt, ctx, 0x08A5BAB8u, {self + 0x54u, 0x08CF6ED8u});
    put8(rt, self + 0x58u, rt.memory().load8(0x08CFF20Cu));
    tail_call(ctx, 0x08C22DDCu, {self + 0x24u, 0xAu});
}

// Stores 16-bit fields and two 1.0f / 0x4C088344 words around two sub-constructor calls; the answer of the last call is the result.
void func_08AD2F20_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put16(rt, self + 0xEu, 0xFFFFu);
    put16(rt, self + 0x6u, 0xFFFFu);
    put16(rt, self + 0x8u, 0xFFFFu);
    put16(rt, self + 0xAu, 0u);
    put16(rt, self + 0xCu, 0xFFFFu);
    put16(rt, self + 0x4u, 0xFFFFu);
    put(rt, self, 0x08CD6C70u);
    invoke(rt, ctx, 0x08AD5EBCu, {self + 0x14u});
    put(rt, self + 0x1Cu, 0u);
    invoke(rt, ctx, 0x08AD5EBCu, {self + 0x44u});
    put(rt, self + 0x50u, 0x3F800000u);
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x54u, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0x4C088344u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x78u, 0u);
}

// The base constructor, the vtable and five pointer fields, four sub-constructors at +0x34..+0x40, then two stores of a
// shared pointer and two calls on +0x74 and +0x78; the answer of the last call is the result.
void func_08A9DB10_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    invoke(rt, ctx, 0x08A5B828u, {self});
    const u32 link = self + 0x10u;
    const u32 node = self + 0x24u;
    const u32 tail = self + 0x2Cu;
    put(rt, self, 0x08CD48C0u);
    put(rt, self + 0x1Cu, link);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, link);
    put(rt, node + 4u, node);
    put(rt, self + 0x24u, node);
    put(rt, self + 0x2Cu, tail);
    put(rt, tail + 4u, tail);
    invoke(rt, ctx, 0x08A76AB4u, {self + 0x34u, 0x08CF6ED8u});
    invoke(rt, ctx, 0x08A76AB4u, {self + 0x38u, 0x08CF6ED8u});
    invoke(rt, ctx, 0x08A76AB4u, {self + 0x3Cu, 0x08CF6ED8u});
    invoke(rt, ctx, 0x08A76AB4u, {self + 0x40u, 0x08CF6ED8u});
    const u32 vtable = 0x08D74FDCu;
    put(rt, self + 0x48u, vtable);
    put(rt, self + 0x44u, vtable);
    invoke(rt, ctx, 0x08B28DCCu, {self + 0x4Cu});
    invoke(rt, ctx, 0x08A98A70u, {self + 0x74u});
    invoke(rt, ctx, 0x08A98A70u, {self + 0x78u});
    put8(rt, self + 0x7Cu, 0u);
}

} // namespace

void func_08AFB5B4(Runtime &rt, AllegrexContext &ctx) { func_08AFB5B4_body(rt, ctx); }
void func_08B23EF4(Runtime &rt, AllegrexContext &ctx) { func_08B23EF4_body(rt, ctx); }
void func_08B23BF0(Runtime &rt, AllegrexContext &ctx) { func_08B23BF0_body(rt, ctx); }
void func_08B23C8C(Runtime &rt, AllegrexContext &ctx) { func_08B23C8C_body(rt, ctx); }
void func_08A9A5F8(Runtime &rt, AllegrexContext &ctx) { func_08A9A5F8_body(rt, ctx); }
void func_08AC6704(Runtime &rt, AllegrexContext &ctx) { func_08AC6704_body(rt, ctx); }
void func_08B1BFD8(Runtime &rt, AllegrexContext &ctx) { func_08B1BFD8_body(rt, ctx); }
void func_08A62B98(Runtime &rt, AllegrexContext &ctx) { func_08A62B98_body(rt, ctx); }
void func_08A9DB10(Runtime &rt, AllegrexContext &ctx) { func_08A9DB10_body(rt, ctx); }

} // namespace new_batch_main_1126
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1126::name, gt::ReturnKind::kind)
NEWM(0x08AFB5B4u, func_08AFB5B4, Word);
NEWM(0x08B23EF4u, func_08B23EF4, Word);
NEWM(0x08B23BF0u, func_08B23BF0, Word);
NEWM(0x08B23C8Cu, func_08B23C8C, Word);
NEWM(0x08A9A5F8u, func_08A9A5F8, Word);
NEWM(0x08AC6704u, func_08AC6704, Word);
NEWM(0x08B1BFD8u, func_08B1BFD8, Word);
NEWM(0x08A62B98u, func_08A62B98, Word);
NEWM(0x08A9DB10u, func_08A9DB10, Word);
