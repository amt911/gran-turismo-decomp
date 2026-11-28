// New batch 901 (main session, draft only): structured C written from tools.disasm, not yet gate-verified.
// Includes the allocator+constructor+append family (0x08A5BC20 / 0x08A76AB4 wrappers), the float clamps and
// angle wrappers, the vector-shape pair at 0x089FA1F4/0x089FA30C and several constructors.
// tools.literal_check still names derived store offsets (a base register plus a displacement, e.g. $v1+4 = 0x18);
// each was checked against the disassembly. frame_audit, arg_reads and sp_loads are clean.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1887 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A nested guest call: v0, v1 and f0 reach the caller as after the `jal`, and a1 is the value the callee left
// (a few callers here read a1 without setting it again).
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }
float float_reg(AllegrexContext &ctx, unsigned index) { return std::bit_cast<float>(ctx.fpr[index]); }
// trunc.w.s: toward zero; out of range and NaN give the R3000's results, not the host's undefined cast.
s32 trunc_word(float value) {
    if (value != value) return 0x7FFFFFFF;
    if (value >= 2147483648.0f) return 0x7FFFFFFF;
    if (value <= -2147483649.0f) return static_cast<s32>(0x80000000u);
    return static_cast<s32>(value);
}

// 0x0884CB98: allocates 0x28 bytes, builds an object there (0x0884C590 with a2, a3, t0) and appends it to the
// owner's list (0x08A76AB4 with the frame slot as the value).

// 0x089F14D0: the same shape with a 0x2C-byte object and the constructor at 0x089F12C4 (a1, a2, a3).

// 0x08A44D1C: the same shape with a 0x20-byte object, the constructor at 0x08A444FC and a3 masked to a byte.

// 0x0894F0AC (a0 = object, f12 = angle in degrees): f20 = f12 * 0.017453293, clamped to the +-limit at the object
// at *(a0+0xC)+0x78, stored at *(a0+8)+0x520, then the object is refreshed (0x089567C0) and the work continues in
// a tail call to 0x0894E9C0 with a0.

// 0x08951280 (a0 = object, a1 = pointer): fills a 0x230-byte frame through 0x08958324, then 0x0894C460 at +0x110,
// 0x0895077C with the frame halves and 0x089511BC with the caller's pointer.

// 0x08869900: builds the object at a0 (the 0x1A0 vtable, the 0x1A4 block, the 0x1B0 member) and marks +0x1C4.

// 0x0891E198: the constructor at 0x08926D60, then the 0x08CC5D70 vtable and zeroed members up to +0x238; v0 = -1.

// 0x08932B74 (a0 = object, a1-a3, t0 = output pointers): 0x089363FC prepares (its a1 is kept for the next call),
// 0x08936584 reads the object's +0x40/+0x44 into the frame the outputs come from: bytes at 0x2C, 0x2A, 0x2B, 0x24
// (the first unsigned, the next two signed, the last unsigned), each stored as a word; v0 = the signed byte at 0x2A.

// 0x0897D930 (a0 = object, a1 = 9-word source): the 0x08CB8E48 vtable and the source copied word for word to +0x14
// in the original's order; v0 = the vtable address.

// 0x08A5102C: zeroes the block at a0 (words, bytes and the 0x400 half at +0x1C), then 0x08B28D34(a0+0x30).

// 0x08808CC8: allocates 0x1C bytes, constructs it with 0x08A76ACC, stores the 0x08CB8BD8 vtable and two members
// (0x08A80FA4 at +0x14, 0x08A983E8 at +0x18), then appends it through 0x08A76AB4 with the frame slot.

// 0x089FA1F4 (a0 = output triple, a1 = {begin, end}): allocates count*32 (rounded down) bytes through 0x08B35840,
// sets the triple to {storage, storage, storage+bytes}, copies with 0x08C0D7F8 into it (+4 becomes its answer) and
// marks +0xC with -1; v0 = the copy's answer.

// 0x089FA30C: the same vector shape; the copy's answer goes to +4, +0xC comes from the source's +0xC and v0 is it.
void func_089FA30C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 range = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 end = word(rt, range + 0x4u);
    const u32 begin = word(rt, range + 0x0u);
    put(rt, self + 0x4u, 0u);
    const u32 bytes = (end - begin) & ~31u;
    put(rt, self + 0x0u, 0u);
    put(rt, self + 0x8u, 0u);
    const u32 storage = call(rt, ctx, 0x08B35840u, {0x10u, bytes});
    const u32 limit = storage + bytes;
    put(rt, self + 0x0u, storage);
    put(rt, self + 0x4u, storage);
    put(rt, self + 0x8u, limit);
    const u32 copied = call(rt, ctx, 0x08C0D7F8u, {begin, end, storage, frame.at(0u)});
    put(rt, self + 0x4u, copied);
    const u32 extra = word(rt, range + 0xCu);
    put(rt, self + 0xCu, extra);
    ctx.gpr[2] = extra;
}

// 0x0884CC98: 0x28-byte object built by 0x0884C664 (a2, a3, t0, t1) and appended through 0x08A76AB4.

// 0x0899C7F4: 0x2C-byte object built by 0x0899C6E0 (a1, a2, a3, t0), appended through 0x089BA130.

// 0x0899CA0C: the same shape with the constructor at 0x0899C900.

// 0x089C3B74 (a0 owner, a1, a2, f12, f13): 0x2C-byte object built by 0x089C3684 with the same floats, appended
// through 0x08965854.

// 0x0892145C (a0 = object, a2 = word member, a3 = byte): constructor 0x08925FE4, the 0x08CC6000 vtable, zeroed
// members, the 5.0f / 50.0f pair at +0x2C / +0x30 and the byte at +0x35; v0 = the vtable address.

// 0x089CBA20: constructor 0x08A0D658, the 0x08CC9FD8 vtable, the 0x08CCA230 member at +0x38 and the four zeroed
// 0x14-byte blocks at +0x120..+0x160; v0 = 2.

// 0x089FA008: 0x20 bytes through 0x08B35840, the {base, base, base+0x20} triple, a 1.0f x4 vector plus -1 at
// sp+0x20 and 0x08BE6028(base, 1, sp+0x10, sp); +0xC = -1 and v0 = base + 0x20.

// 0x08896970 (a0 = object, a1, a2 = two members): constructor 0x08898EE8, the 0x08CC1F60 vtable, the members at
// +0x58/+0x5C, two 0x08895D88 blocks at +0x70 and +0xB4; v0 = 1 and +0xF8 = 1.

// 0x088F78CC (f12, f13, f14 = value, addend, factor): f0 = 0x088F7878(a0-a3, f12), then addend + f0 * factor,
// wrapped into [0, 360) by a +360 loop for negatives and a -360 loop for values above 360; the result is f0.

// 0x08898DB4: constructor 0x088F946C, the 0x08CC20A8 vtable, zeroed members at +0x30..+0x40 and two 0x08B62344
// blocks: 0x10 bytes ({0x08CC2220, 0, 0, this} at +0x30) and 0x14 bytes ({0x08CC32D0, 0x08CC3300, 0x08CC3318,
// 0x08CC332C, 0} at +0x38); v0 = the second block.

// 0x088CEDC4: field setup (0x78 at +0x130, 1.0f at +0x13C, the 0x08CDD21C table), the two 0x088CEA34 / 0x088CEA60
// resets, 0x088CEA7C(this, 0x08CDD21C), then the tail call to 0x088CED68 with a0 = this.

// 0x0898D3DC: constructor 0x0896BDF8, the two vtables, 0x08984824(a0+0xF0), 0x089CC6C0(a0+0x1E0), the 0x08D74FDC
// member at +0x1F4, zeroed bytes/words and 0x0896A9C4(this, 0, 0, sp+4, sp) with 256.0f at sp+4 and 24.0f at sp.

// 0x08A24DAC: 0x08AB9270(this), 0x08AB9284 on +0x10 and +0x34, the 0x08D74FDC members at +0x68 and +0x64, the 1.0f
// fields, 0x08ABB9E8(0) into +0x58 and this stored at +0x20 / +0x44; v0 = the allocation.

// 0x089A6CBC (a0 = object, a1 = pointer stored on the frame): 0x08A76ACC constructor, the 0x08CBAF98 vtable, the
// 0x089A32B4 block at +0x14 that reads the frame slot, then 0x08A0FD28 twice and 0x08A76AAC four times with the
// 0x08CF6ED8 table; +0x18 and +0x38 are 3, +0x1C is 2.
void func_089A6CBC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self + 0x0u, 0x08CBAF98u);
    put(rt, frame.at(0u), a1);
    call(rt, ctx, 0x089A32B4u, {self + 0x14u, frame.at(0u)});
    put(rt, self + 0x1Cu, 2u);
    put(rt, self + 0x18u, 3u);
    call(rt, ctx, 0x08A0FD28u, {self + 0x20u, 0x08CF6ED8u});
    call(rt, ctx, 0x08A0FD28u, {self + 0x24u, 0x08CF6ED8u});
    call(rt, ctx, 0x08A76AACu, {self + 0x28u, 0x08CF6ED8u});
    call(rt, ctx, 0x08A76AACu, {self + 0x2Cu, 0x08CF6ED8u});
    call(rt, ctx, 0x08A76AACu, {self + 0x30u, 0x08CF6ED8u});
    call(rt, ctx, 0x08A76AACu, {self + 0x34u, 0x08CF6ED8u});
    put(rt, self + 0x38u, 3u);
}

// 0x08A188E0 (a0 = object, f12/f13/f14 = three channels): 0x08B2E05C answers a 64-bit base (v0:v1) for the index
// trunc(f12*1000) scaled by 1000 bytes; +0x280/+0x284 take that address, +0x288 and +0x28C the trunc(f13*255) and
// trunc(f14*255) bytes; v0 is the second of them.
void func_08A188E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float f12 = float_reg(ctx, 12);
    const float f13 = float_reg(ctx, 13);
    const float f14 = float_reg(ctx, 14);
    GuestFrame frame(ctx, 0x20u);
    const GuestResult base = call_full(rt, ctx, 0x08B2E05Cu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]},
                                       {f12, f13, f14});
    const s32 index = trunc_word(psp_mul(f12, 1000.0f));
    const s32 green = trunc_word(psp_mul(f13, 255.0f));
    const s32 blue = trunc_word(psp_mul(f14, 255.0f));
    const u32 scaled = static_cast<u32>(index);
    u32 offset = (scaled << 7) - (scaled << 2);
    offset += scaled;
    offset <<= 3;
    const u32 low = base.v0 + offset;
    const u32 carry = low < base.v0 ? 1u : 0u;
    const u32 high = base.v1 + (static_cast<s32>(offset) < 0 ? 0xFFFFFFFFu : 0u) + carry;
    put(rt, self + 0x28Cu, static_cast<u32>(blue) & 0xFFu);
    put(rt, self + 0x280u, low);
    put(rt, self + 0x284u, high);
    put(rt, self + 0x288u, static_cast<u32>(green) & 0xFFu);
    ctx.gpr[2] = static_cast<u32>(blue) & 0xFFu;
}

// 0x0891C290: the 0x08CC5CA0 vtable at +0, 0x089263EC on the three 0x08CC6178 blocks at +0x2C, +0x58 and +0x84,
// 0x08926488(table, 1) on each, then the tail call to 0x0891AA34 with a0 = this.
void func_0891C290(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 first = self + 0x2Cu;
    const u32 second = self + 0x58u;
    const u32 third = self + 0x84u;
    put(rt, self + 0x0u, 0x08CC5CA0u);
    put(rt, self + 0x4u, 0u);
    call(rt, ctx, 0x089263ECu, {first});
    put(rt, self + 0x2Cu, 0x08CC6178u);
    call(rt, ctx, 0x089263ECu, {second});
    put(rt, self + 0x58u, 0x08CC6178u);
    call(rt, ctx, 0x089263ECu, {third});
    put(rt, self + 0x84u, 0x08CC6178u);
    call(rt, ctx, 0x08926488u, {first, 0x08CDDBB8u, 1u});
    call(rt, ctx, 0x08926488u, {second, 0x08CDDBD0u, 1u});
    call(rt, ctx, 0x08926488u, {third, 0x08CDDBB8u, 1u});
    tail_call(ctx, 0x0891AA34u, {self});
}

// 0x089C1C94: constructor 0x08A0FDD8, the 0x08CCA380 vtable, the 0x08CCA54C member at +0x38, four zeroed 0x10-byte
// blocks at +0x80/+0x90/+0xB0/+0xD0, the 1.0f / 0.3f / -1.0f fields and the byte at +0xCB; v0 = 1.
void func_089C1C94(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A0FDD8u, {self});
    put(rt, self + 0x0u, 0x08CCA380u);
    put(rt, self + 0x38u, 0x08CCA54Cu);
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0xD0u, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x98u, 0u);
    put(rt, self + 0x9Cu, 0u);
    put_float(rt, self + 0xA0u, 1.0f);
    put(rt, self + 0xB0u, 0u);
    put(rt, self + 0xB4u, 0u);
    put(rt, self + 0xB8u, 0u);
    put(rt, self + 0xBCu, 0u);
    put_float(rt, self + 0xC0u, 1.0f);
    put_float(rt, self + 0xC4u, std::bit_cast<float>(0x3E99999Au));
    put8(rt, self + 0xCBu, 1u);
    put8(rt, self + 0xC8u, 0u);
    put8(rt, self + 0xC9u, 0u);
    put8(rt, self + 0xCAu, 0u);
    put(rt, self + 0xCCu, 0u);
    put_float(rt, self + 0xDCu, -1.0f);
    put(rt, self + 0xD4u, 0u);
    put_float(rt, self + 0xD8u, -1.0f);
    ctx.gpr[2] = 1u;
}

} // namespace

#define NEW901(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1887::name, gt::ReturnKind::kind)
NEW901(0x0891C290u, func_0891C290, Word);
NEW901(0x089A6CBCu, func_089A6CBC, Word);
NEW901(0x089C1C94u, func_089C1C94, Word);
NEW901(0x089FA30Cu, func_089FA30C, Word);
NEW901(0x08A188E0u, func_08A188E0, Word);

} // namespace new_batch_main_1887
} // namespace gt::game
