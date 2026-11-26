// Main-session batch 1121 (functions the earlier seeds left unverified). Structured C from tools.disasm.
// Shapes for the synthetic generator: gt-ops/shapes/shapes_main1121.txt (one line per dereferenced argument).
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1121 {
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

// 0x08C73DE4 (a0 = string object, a1 = needle, a2 = position): v0 = needle's length, then the search tail call
// 0x08C73C00(this, needle, position, length). The tail target answers v0.
void func_08C73DE4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 needle = ctx.gpr[5];
    const u32 position = ctx.gpr[6];
    u32 length;
    {
        GuestFrame frame(ctx, 0x10u);
        length = call(rt, ctx, 0x08B35F60u, {needle});
    }
    tail_call(ctx, 0x08C73C00u, {self, needle, position, length});
}

// 0x08C73E34: the same shape as 0x08C73DE4, searching backwards (tail call 0x08C7366C).
void func_08C73E34(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 needle = ctx.gpr[5];
    const u32 position = ctx.gpr[6];
    u32 length;
    {
        GuestFrame frame(ctx, 0x10u);
        length = call(rt, ctx, 0x08B35F60u, {needle});
    }
    tail_call(ctx, 0x08C7366Cu, {self, needle, position, length});
}

// 0x08A5FBA4 (a0 = container): allocates 0x18 bytes (alignment 4), runs 0x08A5F3F8 on the fresh block with a1 = 0,
// then hands the block to 0x08A76AB4(this, &block). v0 = that last call's answer.
void func_08A5FBA4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08A5F3F8u, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A73464: the same allocation shape, with 0x08A72D38 as the constructor.
void func_08A73464(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08A72D38u, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A85C34: the same allocation shape, with 0x08A854CC as the constructor.
void func_08A85C34(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08A854CCu, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A961D8: the same allocation shape, with 0x08A95AAC as the constructor.
void func_08A961D8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08A95AACu, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A97B88: the same allocation shape, with 0x08A973BC as the constructor.
void func_08A97B88(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08A973BCu, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08AB0344: the same allocation shape, with 0x08AAFBF8 as the constructor.
void func_08AB0344(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08AAFBF8u, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08AB1AC8: the same allocation shape, with 0x08AB137C as the constructor.
void func_08AB1AC8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call(rt, ctx, 0x08AB137Cu, {allocated, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A9E7CC (a0 = object, a1 = text): length = strlen(a1); then 0x08BEED1C(word(a0 + 0x20), a1, length). v0 = a0.
void func_08A9E7CC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 member = word(rt, self + 0x20u);
    const u32 length = call(rt, ctx, 0x08B35F60u, {text});
    (void)call(rt, ctx, 0x08BEED1Cu, {member, text, length});
    ctx.gpr[2] = self;
}

// 0x08AA60FC (a0 = output object, a1-a3 = the 64-bit value): 0x08AA5E24 writes it, then v0 = a0.

// 0x08AD8A30 (a0 = destination buffer, a1 = source string, a2 = buffer size): appends the source after the
// destination's current end, bounded by the size. v0 = the copy call's answer.
void func_08AD8A30(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 size = ctx.gpr[6];
    const u32 used = call(rt, ctx, 0x08B35F60u, {destination});
    ctx.gpr[2] = call(rt, ctx, 0x08AD89ECu, {destination + used, source, size - used});
}

// 0x08ADFF20 (a0 = format, a1 = destination flag, a2-a3/t0-t3 = arguments): formats into a 0x430-byte frame
// buffer with 0x08B373EC, then 0x08ADFEA0(buffer, destination). v0 = the last call's answer.
void func_08ADFF20(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x430u);
    const u32 format = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    put(rt, frame.at(0x418u), ctx.gpr[6]);
    put(rt, frame.at(0x41Cu), ctx.gpr[7]);
    put(rt, frame.at(0x420u), ctx.gpr[8]);
    put(rt, frame.at(0x424u), ctx.gpr[9]);
    put(rt, frame.at(0x428u), ctx.gpr[10]);
    put(rt, frame.at(0x42Cu), ctx.gpr[11]);
    (void)call(rt, ctx, 0x08B373ECu, {frame.at(0u), format, frame.at(0x418u)});
    ctx.gpr[2] = call(rt, ctx, 0x08ADFEA0u, {frame.at(0u), destination});
}

// 0x08B15BDC (a0 = object): 0x08B0CDC8 then five fields stored (vtable +0, +0x18, +0x34, +0x38 = 0x200, +0x1C = 0).
// v0 ends as the last stored constant, v1 as 0x200.
void func_08B15BDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08B0CDC8u, {self});
    put(rt, self + 0x18u, 0x08B15FC0u);
    put(rt, self + 0x00u, 0x08CD9070u);
    put(rt, self + 0x34u, 0x08D4F8A8u);
    put(rt, self + 0x38u, 0x200u);
    put(rt, self + 0x1Cu, 0u);
    ctx.gpr[2] = 0x08D4F8A8u;
    ctx.gpr[3] = 0x200u;
}

// 0x08B1BCFC (a0 = object): four member calls (0x98, 0x78, 0x28) and the float at +0x90 incremented by 1.0, then the
// tail call 0x08B3457C(this). f0 carries the incremented value into the tail.

// 0x08B2BCD0 (a0 = object): 0x08B28DCC, then the field block reset (+0x28 = -1, +0x40 = 2, the rest zero).
// v0 = -1, v1 = 2.
void func_08B2BCD0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08B28DCCu, {self});
    put(rt, self + 0x28u, 0xFFFFFFFFu);
    put(rt, self + 0x40u, 2u);
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 2u;
}

// 0x08B3E100 (a0 = object, a1/a2 = the two stored members): 0x08B36290, vtable at +0, members at +0x20/+0x24,
// +0x28 = 0. v0 = the vtable constant.
void func_08B3E100(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    (void)call(rt, ctx, 0x08B36290u, {self});
    put(rt, self + 0x00u, 0x08CDA980u);
    put(rt, self + 0x20u, first);
    put(rt, self + 0x24u, second);
    put(rt, self + 0x28u, 0u);
    ctx.gpr[2] = 0x08CDA980u;
}

// 0x08B95784 (a0 = output object, a1/a2 = first/last, a3 = allocator): 0x08B956D8(a1, a2, a3) builds the range and its
// answer is stored at *a0. v0 = that answer.
void func_08B95784(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = call(rt, ctx, 0x08B956D8u, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, ctx.gpr[2]);
}

// 0x08A5E018 (a0 = container): allocates 0x20 bytes (alignment 4), constructs with 0x08A5D7A0(block, 0, 0, 0), then
// 0x08A76AB4(this, &block). v0 = that last call's answer.
void func_08A5E018(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    (void)call(rt, ctx, 0x08A5D7A0u, {allocated, 0u, 0u, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A70AC4 (a0 = object, a1 = member source): base 0x08A76ACC, vtable at +0, 0x08A71E28(+0x14, a1), then the second
// vtable at +0x18. v0 = that vtable constant.
void func_08A70AC4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCD860u);
    (void)call(rt, ctx, 0x08A71E28u, {self + 0x14u, other});
    put(rt, self + 0x18u, 0x08D74FD0u + 0xCu);
    ctx.gpr[2] = 0x08D74FD0u + 0xCu;
}

// 0x08A71794 (a0 = object, a1 = member source): base 0x08A76ACC, vtable at +0, 0x08A71E28(+0x14, a1), then three zero
// words at +0x18..+0x20. v0 = the address of that zero block (this + 0x18).
void func_08A71794(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCD748u);
    (void)call(rt, ctx, 0x08A71E28u, {self + 0x14u, other});
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x18u + 8u, 0u);
    put(rt, self + 0x18u + 4u, 0u);
    ctx.gpr[2] = self + 0x18u;
}

// 0x08A7BF24 (a0 = key object, a1 = key holder): 0x08B344E0(table) locks, 0x08AAD700(table, a0, a1) does the lookup,
// then the tail call 0x08B3457C(table) unlocks.
void func_08A7BF24(Runtime &rt, AllegrexContext &ctx) {
    const u32 key = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call(rt, ctx, 0x08B344E0u, {0x08CFF450u});
        (void)call(rt, ctx, 0x08AAD700u, {0x08CF705Cu, key, holder});
    }
    tail_call(ctx, 0x08B3457Cu, {0x08CFF450u});
}

// 0x08A83290 (a0 = container): allocates 0x20 bytes (alignment 4), constructs with 0x08A82AF0(block, 0, 0, 0), then
// 0x08A76AB4(this, &block). v0 = that last call's answer.
void func_08A83290(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    (void)call(rt, ctx, 0x08A82AF0u, {allocated, 0u, 0u, 0u});
    put(rt, frame.at(0u), allocated);
    ctx.gpr[2] = call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A8E090 (a0 = object): 0x08A8DFC8, unlink the node at +0x58 with 0x08B5C094, free it with 0x08A5BC54(node, 0xC, 4),
// then decrement the counter at word(+0x60) + 8. v0 = the new counter, v1 = the counter block.
void func_08A8E090(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08A8DFC8u, {self});
    const u32 node = word(rt, self + 0x58u);
    (void)call(rt, ctx, 0x08B5C094u, {node});
    (void)call(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
    const u32 counter = word(rt, self + 0x60u);
    const u32 value = word(rt, counter + 8u) - 1u;
    put(rt, counter + 8u, value);
    ctx.gpr[2] = value;
    ctx.gpr[3] = counter;
}

// 0x08A93D7C (a0 = object, a1 = member source): 0x08A719F0, vtable at +0, 0x08A76AA0(+0x1C), 0x08A94CE8(+0x20),
// +0x24 = 0, then the tail call 0x08A93CC8(this, a1).
void func_08A93D7C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call(rt, ctx, 0x08A719F0u, {self});
        put(rt, self, 0x08CD3B28u);
        (void)call(rt, ctx, 0x08A76AA0u, {self + 0x1Cu});
        (void)call(rt, ctx, 0x08A94CE8u, {self + 0x20u});
        put(rt, self + 0x24u, 0u);
    }
    tail_call(ctx, 0x08A93CC8u, {self, other});
}

// 0x08A9E650 (a0 = object): formats the constant string 0x08CB2708 into a 0x30-byte frame buffer, then
// 0x08BEED1C(word(a0 + 0x20), buffer, length). v0 = a0.
void func_08A9E650(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08B37640u, {frame.at(0u), 0x08CB2708u});
    const u32 member = word(rt, self + 0x20u);
    const u32 length = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    (void)call(rt, ctx, 0x08BEED1Cu, {member, frame.at(0u), length});
    ctx.gpr[2] = self;
}

// 0x08A9E6A8: the same shape as 0x08A9E650 with the constant string 0x08CB2710.
void func_08A9E6A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08B37640u, {frame.at(0u), 0x08CB2710u});
    const u32 member = word(rt, self + 0x20u);
    const u32 length = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    (void)call(rt, ctx, 0x08BEED1Cu, {member, frame.at(0u), length});
    ctx.gpr[2] = self;
}

// 0x08AABA34 (a0 = string object, a2/a3 = arguments): formats 0x08CB31B0 into a 0x50-byte frame buffer with the two
// arguments in a3/t0 (t1 repeats a3), then 0x08BEED1C(this, buffer, length). v0 = this.
void func_08AABA34(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    (void)call(rt, ctx, 0x08B3767Cu, {frame.at(0u), 0x40u, 0x08CB31B0u, arg3, arg2, arg3});
    const u32 length = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    (void)call(rt, ctx, 0x08BEED1Cu, {self, frame.at(0u), length});
    ctx.gpr[2] = self;
}

// 0x08AAF970 (a0 = object, a1 = member source, a2 = member value): base 0x08A76ACC, vtable at +0, 0x08B88F44(+0x14, a1),
// then +0x18 = a2. v0 stays the 0x08B88F44 answer (the asm never rewrites it).
void func_08AAF970(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    (void)call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD3150u);
    ctx.gpr[2] = call(rt, ctx, 0x08B88F44u, {self + 0x14u, other});
    put(rt, self + 0x18u, value);
}

// 0x08AD388C (a0 = object): 0x08C3E064(this, this), then the block +0x10..+0x24 and +0x30..+0x38 is zeroed.
// v0 = the address of the second block (this + 0x30).
void func_08AD388C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08C3E064u, {self, self});
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x18u + 8u, 0u);
    put(rt, self + 0x18u + 4u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x30u + 8u, 0u);
    put(rt, self + 0x30u + 4u, 0u);
    ctx.gpr[2] = self + 0x30u;
}

} // namespace
} // namespace new_batch_main_1121
} // namespace gt::game

#define NEW1121(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1121::name, gt::ReturnKind::kind)
NEW1121(0x08C73DE4u, func_08C73DE4, Word);
NEW1121(0x08C73E34u, func_08C73E34, Word);
NEW1121(0x08A5FBA4u, func_08A5FBA4, Word);
NEW1121(0x08A73464u, func_08A73464, Word);
NEW1121(0x08A85C34u, func_08A85C34, Word);
NEW1121(0x08A961D8u, func_08A961D8, Word);
NEW1121(0x08A97B88u, func_08A97B88, Word);
NEW1121(0x08A9E7CCu, func_08A9E7CC, Word);
NEW1121(0x08AB0344u, func_08AB0344, Word);
NEW1121(0x08AB1AC8u, func_08AB1AC8, Word);
NEW1121(0x08AD8A30u, func_08AD8A30, Word);
NEW1121(0x08ADFF20u, func_08ADFF20, Word);
NEW1121(0x08B15BDCu, func_08B15BDC, Word);
NEW1121(0x08B2BCD0u, func_08B2BCD0, Word);
NEW1121(0x08B3E100u, func_08B3E100, Word);
NEW1121(0x08B95784u, func_08B95784, Word);
NEW1121(0x08A5E018u, func_08A5E018, Word);
NEW1121(0x08A70AC4u, func_08A70AC4, Word);
NEW1121(0x08A71794u, func_08A71794, Word);
NEW1121(0x08A7BF24u, func_08A7BF24, Word);
NEW1121(0x08A83290u, func_08A83290, Word);
NEW1121(0x08A8E090u, func_08A8E090, Word);
NEW1121(0x08A93D7Cu, func_08A93D7C, Word);
NEW1121(0x08A9E650u, func_08A9E650, Word);
NEW1121(0x08A9E6A8u, func_08A9E6A8, Word);
NEW1121(0x08AABA34u, func_08AABA34, Word);
NEW1121(0x08AAF970u, func_08AAF970, Word);
NEW1121(0x08AD388Cu, func_08AD388C, Word);
