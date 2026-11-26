// Main-session batch 899 (draft-only; unverified). Structured C from tools.disasm: the record publishers (allocate, build,
// assign through the owner), the base/member constructors of the same class family, and the two object queries whose answer
// is either the word or the byte the 0x22-slot lookup wrote. Shapes for the synthetic cases: shapes_main899.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_899 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// The publisher shape: allocate `size` bytes (08A5BC20(size, 4)), build the record into it (constructor takes the fresh
// block and the caller's a1), park the block in the frame's first word and hand it to the owner's insert (a0 = the
// caller's a0, a1 = &block); v0 is that last call's answer.
u32 build_and_assign(Runtime &rt, AllegrexContext &ctx, u32 size, u32 constructor, u32 insert, bool narrow_argument) {
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = ctx.gpr[4];
    const u32 argument = narrow_argument ? (ctx.gpr[5] & 0xFFu) : ctx.gpr[5];
    const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, constructor, {object, argument});
    put(rt, frame.at(0u), object);
    return call(rt, ctx, insert, {owner, frame.at(0u)});
}

void func_08844590(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08844478u, 0x08A76AB4u, false); }
void func_088456F4(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x088455D4u, 0x08A76AB4u, false); }
void func_0884648C(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08846374u, 0x08A76AB4u, false); }
void func_08848E18(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08848D00u, 0x08A76AB4u, false); }
void func_0884F6D4(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x0884F5BCu, 0x08A76AB4u, false); }
void func_08857CA4(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08857BECu, 0x08A76AB4u, true); }
void func_08857D04(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08857BECu, 0x08A76AB4u, true); }
void func_0897E04C(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x38u, 0x0897D76Cu, 0x08A76AB4u, false); }
void func_089A75E4(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x3Cu, 0x089A6D70u, 0x08A76AB4u, false); }
void func_089FCED0(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x6Cu, 0x089FCD60u, 0x089BAC44u, false); }
void func_08A04294(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x40u, 0x08A03BB8u, 0x08A76AB4u, false); }
void func_08A19064(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x18u, 0x08A18EB8u, 0x08A5D40Cu, false); }
void func_08A2DD40(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08A2DC28u, 0x08A76AB4u, false); }
void func_08A3E8C0(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08A3E7FCu, 0x08A76AB4u, false); }
void func_08A3E920(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x1Cu, 0x08A3E7FCu, 0x08A76AB4u, false); }
void func_08A55F30(Runtime &rt, AllegrexContext &ctx) { build_and_assign(rt, ctx, 0x14u, 0x08A55D70u, 0x08A7263Cu, false); }

// Base constructor: the parent runs first, the two vtable words go in, two 0x120-byte blocks are cleared, the byte at
// +0x40 is set to -1; v0 is the second clear's answer (a memset-like return).
void func_088EF1B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B083E4u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, 0x08CC3A38u);
    put(rt, self + 0x38u, 0u);
    rt.memory().store8(self + 0x280u, 0u);
    call(rt, ctx, 0x08B36018u, {self + 0x40u, 0u, 0x120u});
    call(rt, ctx, 0x08B36018u, {self + 0x160u, 0u, 0x120u});
    rt.memory().store8(self + 0x40u, 0xFFu);
}

// The parent gets this, a1 and a2; the count in a2 is divided by 2147483648/... (multu by 0x07A44C6B, high word >> 3)
// into +0x10, a1 goes to +0xC, the byte at +0x14 is cleared, and the object goes on to its own init as a tail call.
void func_088F6160(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    call(rt, ctx, 0x088F60DCu, {self, argument, count, ctx.gpr[7]});
    put(rt, self + 0xCu, argument);
    const u32 quotient = static_cast<u32>((static_cast<std::uint64_t>(count) * 0x07A44C6Bu) >> 32) >> 3;
    put(rt, self + 0x10u, quotient);
    rt.memory().store8(self + 0x14u, 0u);
    tail_call(ctx, 0x088F5BD8u, {self});
}

// Records the caller's a2 (slot +0x10), a3 (+0x8) and t0 (+0xC) on the object the parent initialised, then answers the
// second vtable word.

// Reads the pair at +0x10 / +0x14 into a 0x22-byte lookup, then either the answer's word or its byte at +0x14, or -1.
u32 query_pair(Runtime &rt, AllegrexContext &ctx, bool byte_result) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 first = call(rt, ctx, 0x089363FCu, {self, argument, ctx.gpr[6], ctx.gpr[7]});
    call(rt, ctx, 0x08936578u, {first, argument, word(rt, self + 0x10u), word(rt, self + 0x14u)});
    const u32 second = call(rt, ctx, 0x089363FCu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const u32 found = call(rt, ctx, 0x08936544u, {second, second, 0x22u, frame.at(0u)});
    if (found == 0u) return 0xFFFFFFFFu;
    return byte_result ? rt.memory().load8(frame.at(0x14u)) : word(rt, frame.at(0u));
}

void func_08931A7C(Runtime &rt, AllegrexContext &ctx) { ctx.gpr[2] = query_pair(rt, ctx, false); }
void func_089321E8(Runtime &rt, AllegrexContext &ctx) { ctx.gpr[2] = query_pair(rt, ctx, true); }

// Constructor: the parent takes this/a1/a2/a3, two vtable words go at +0 and +0x38, the run of words +0x120..+0x138 is
// cleared, and the body continues in the object's own init with a1 = 0.
void func_0899A2A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x089FB918u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, 0x08CCBBA8u);
    put(rt, self + 0x38u, 0x08CCBE00u);
    for (u32 offset = 0x120u; offset <= 0x138u; offset += 4u) put(rt, self + offset, 0u);
    tail_call(ctx, 0x089689C8u, {self, 0u});
}

// Constructor: the parent takes this/a1/a2/a3, the vtable goes at +0, four words at +0x28..+0x34 are cleared, the two
// member blocks at +0x38 / +0x3C are built, the byte at +0x40 is cleared; v0 is the second build's answer.
void func_0899BBCC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08973B94u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, 0x08CCBA90u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x38u});
    call(rt, ctx, 0x08A61BC0u, {self + 0x3Cu, 0x08CF6ED8u});
    put(rt, self + 0x40u, 0u);
}

// Constructor: the parent takes this/a1/a2/a3; the vtable at +0, the caller's a3 byte at +0x1C, a1 at +0x14 and a2 at
// +0x18.

// Constructor: the parent takes this/a1/a2/a3; the vtable at +0, a1 at +0x14, a2 at +0x18 and the a3 byte at +0x1C.

// Constructor: the parent takes this/a1/a2/a3; two vtable words at +0 and +0x38, +0x78 cleared, the block at +0x7C
// built with the global name, the float +0x84 set to 4.0 and the two bytes at +0x80/+0x81 cleared; v0 is the build's
// answer.
void func_088354EC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A0FDD8u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, 0x08CBDDE8u);
    put(rt, self + 0x38u, 0x08CBDFB4u);
    put(rt, self + 0x78u, 0u);
    call(rt, ctx, 0x08829680u, {self + 0x7Cu, 0x08CF6ED8u});
    put_float(rt, self + 0x84u, 4.0f);
    rt.memory().store8(self + 0x80u, 0u);
    rt.memory().store8(self + 0x81u, 0u);
}

// Constructor: the parent takes this/a1/a2/a3; the vtable at +0, a2 at +0x18, a3 at +0x1C, the low half of t0 at +0x20
// and a zero half at +0x22.

// The parent takes this/a1/a2/a3; the words at +0 and +4..+0x1C are cleared, the block at +0x20 is built, +0x48 set to
// -1, the byte at +0x2DC cleared and the words +0x4C..+0x58 cleared; v0 is -1.
void func_088AB18C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0u);
    for (u32 offset = 0x4u; offset <= 0x1Cu; offset += 4u) put(rt, self + offset, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x20u});
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, self + 0x48u, 0xFFFFFFFFu);
    rt.memory().store8(self + 0x2DCu, 0u);
    for (u32 offset = 0x4Cu; offset <= 0x58u; offset += 4u) put(rt, self + offset, 0u);
}

// Indexes the table at +0xC0 by 760 * a1, asks 088FE518 about the word at +0x14, then hands the entry, the +0x618 word,
// the answer and a zero in a3 (f12 = 0) to 088E9054 as a tail call.

// The parent takes this/a1/a2/a3; the vtable at +0, a2 at +0x8, the member at +0xC rebuilt, and the object continues
// through 08926488 with (member, constant, 1).
void func_08922B34(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[6];
    call(rt, ctx, 0x08925FE4u, {self, ctx.gpr[5], argument, ctx.gpr[7]});
    put(rt, self, 0x08CC5EA0u);
    call(rt, ctx, 0x08926398u, {self + 0xCu});
    put(rt, self + 0x8u, argument);
    tail_call(ctx, 0x08926488u, {self + 0xCu, 0x08CDDD20u, 1u});
}

} // namespace
} // namespace new_batch_main_899
} // namespace gt::game

#define NEW899(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_899::name, gt::ReturnKind::kind)
NEW899(0x08844590u, func_08844590, Word);
NEW899(0x088456F4u, func_088456F4, Word);
NEW899(0x0884648Cu, func_0884648C, Word);
NEW899(0x08848E18u, func_08848E18, Word);
NEW899(0x0884F6D4u, func_0884F6D4, Word);
NEW899(0x08857CA4u, func_08857CA4, Word);
NEW899(0x08857D04u, func_08857D04, Word);
NEW899(0x088EF1B0u, func_088EF1B0, Word);
NEW899(0x088F6160u, func_088F6160, Word);
NEW899(0x08931A7Cu, func_08931A7C, Word);
NEW899(0x089321E8u, func_089321E8, Word);
NEW899(0x0897E04Cu, func_0897E04C, Word);
NEW899(0x0899A2A0u, func_0899A2A0, Word);
NEW899(0x0899BBCCu, func_0899BBCC, Word);
NEW899(0x089A75E4u, func_089A75E4, Word);
NEW899(0x089FCED0u, func_089FCED0, Word);
NEW899(0x08A04294u, func_08A04294, Word);
NEW899(0x08A19064u, func_08A19064, Word);
NEW899(0x08A2DD40u, func_08A2DD40, Word);
NEW899(0x08A3E8C0u, func_08A3E8C0, Word);
NEW899(0x08A3E920u, func_08A3E920, Word);
NEW899(0x08A55F30u, func_08A55F30, Word);
NEW899(0x088354ECu, func_088354EC, Word);
NEW899(0x088AB18Cu, func_088AB18C, Word);
NEW899(0x08922B34u, func_08922B34, Word);
