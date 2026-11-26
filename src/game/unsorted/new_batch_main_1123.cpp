// Main-session batch 1123 (28 functions, draft only, unverified): structured C from tools.disasm.
// Shapes: allocate(size, 4) -> constructor -> store the object in a guest local -> hand the local and `this` to a sink;
// vtable constructors; a few tail calls. Functions with 64-bit or FPU results are not in this batch.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1123 {
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

// The allocator used by every block below: v0 = allocate(size, 4).
constexpr u32 kAllocate = 0x08A5BC20u;

// Shape shared by eleven functions: block = allocate(size, 4); object = ctor(block, arg); local = object;
// sink(this, &local). v0 is the sink's result, as the asm leaves it.
void allocate_construct_store(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 arg, u32 sink) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, kAllocate, {size, 4u});
    const u32 object = call(rt, ctx, ctor, {block, arg});
    put(rt, frame.at(0u), object);
    call(rt, ctx, sink, {self, frame.at(0u)});
}

// Three-word tail variant: block = allocate(size, 4); object = ctor(block, 0, arg, 0x10); local = object; sink(this, &local).
void allocate_construct_store_wide(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 arg) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, kAllocate, {size, 4u});
    const u32 object = call(rt, ctx, ctor, {block, 0u, arg, 0x10u});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// Shape of 08A7C8A4 / 08A7C910 / 08A7C97C: fill(&slot); transfer(&object, &slot, a2); release(this, object); destroy(&slot).
void fill_transfer_release(Runtime &rt, AllegrexContext &ctx, u32 fill, u32 transfer) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(0u);
    const u32 object = frame.at(4u);
    call(rt, ctx, fill, {slot});
    call(rt, ctx, transfer, {object, slot, a2});
    call(rt, ctx, 0x08A7C5C8u, {self, object});
    call(rt, ctx, 0x08A5B9ECu, {object});
    call(rt, ctx, 0x08A69D68u, {slot});
}

// 0x08A8066C: this->vtable = 0x08CCFA70 after the first call; this+0x14 gets a1; this+0x18 gets a2 through a local.
void func_08A8066C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCFA70u);
    call(rt, ctx, 0x08A76ABCu, {self + 0x14u, arg1});
    put(rt, frame.at(0u), arg2);
    call(rt, ctx, 0x08A802BCu, {self + 0x18u, frame.at(0u)});
}

// 0x08A8956C: allocate 0x148, construct with a1, store the object, sink 0x08A71E10.

// 0x08A89EB0: allocate 0x18, construct with a1, sink 0x08A76AB4.

// 0x08A940BC: allocate 0x28, construct with a1, sink 0x08A71E10.

// 0x08A95150: allocate 0x24, construct with (0, a1, 0x10), sink 0x08A76AB4.

// 0x08A96060: allocate 0x18, construct with a1 sign-extended from a byte, sink 0x08A76AB4.

// 0x08A97A0C: allocate 0x18, construct with a1 & 0xFF, sink 0x08A76AB4.

// 0x08A98C7C: allocate 0x24, construct with a1, sink 0x08A76AB4.

// 0x08A9E768: format into a 0x40-byte local (fmt 0x08CB2720, with a2/a3 passed on), read this->+0x20 as the
// object, get the result of 0x08B35F60 on the local, then pass both to 0x08BEED1C; v0 = this.
void func_08A9E768(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    call(rt, ctx, 0x08B3767Cu, {frame.at(0u), 0x40u, 0x08CB2720u, a3, a2, a3});
    const u32 object = word(rt, self + 0x20u);
    const u32 value = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    call(rt, ctx, 0x08BEED1Cu, {object, frame.at(0u), value});
    ctx.gpr[2] = self;
}

// 0x08AA0A40: a result from 0x08AA0690 on a local-prepared record, released with 0x08A5B9EC; v0 = the result.
void func_08AA0A40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A85C88u, {frame.at(0u)});
    const u32 result = call(rt, ctx, 0x08AA0690u, {frame.at(0u), self, arg1, arg2});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// 0x08AB01CC: allocate 0x18, construct with a1 & 0xFFFF, sink 0x08A76AB4.

// 0x08AB1950: allocate 0x18, construct with a1 & 0xFF, sink 0x08A76AB4.

// 0x08AB2EB0: allocate 0x28, construct with a1, sink 0x08A70EC8.

// 0x08AB34FC: this->vtable = 0x08CD5768; this+0x20..+0x28 = a1..a3, this+0x2C = 0; v0 = the vtable word.
void func_08AB34FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A9E820u, {self});
    put(rt, self, 0x08CD5768u);
    put(rt, self + 0x20u, a1);
    put(rt, self + 0x24u, a2);
    put(rt, self + 0x28u, a3);
    put(rt, self + 0x2Cu, 0u);
    ctx.gpr[2] = 0x08CD5768u;
}

// 0x08AB59A8: allocate 0x20, construct with a1, sink 0x08A7026C.

// 0x08AB62EC: allocate 0x28, construct with a1, sink 0x08A70EC8.

// 0x08ADB0D8: three calls with no arguments but 0x08D40610 and 0; v0 = the last call's result.

// 0x08B09BF4: this = global 0x08D473E8 for the first call; this->+0x114 = a1 and this->+0x118 = a2, then tail call 0x08B3457C.
void func_08B09BF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 global = 0x08D473E8u;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08B344E0u, {global});
        put(rt, word(rt, self) + 0x114u, a1);
        put(rt, word(rt, self) + 0x118u, a2);
    }
    tail_call(ctx, 0x08B3457Cu, {global});
}

// 0x08B1660C: call 0x08B11830(this); this->+0x10 = a3, vtable 0x08CD90B8, this->+0xA0 = a1, this->+0xA4 = a2; v0 = vtable.
void func_08B1660C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B11830u, {self});
    put(rt, self + 0x10u, a3);
    put(rt, self, 0x08CD90B8u);
    put(rt, self + 0xA0u, a1);
    put(rt, self + 0xA4u, a2);
    ctx.gpr[2] = 0x08CD90B8u;
}

// 0x08B1CCC0: spills a2..t3 into a 0x40-byte frame (a2 at +0x28 ... t3 at +0x3C) and passes &spill as the 3rd-argument
// pointer; the first call takes this->[0] and this->[4] and the spilled a1; then 0x08B1CB80(this); v0 = its result.

// 0x08B1F414: two embedded self-pointers and two calls to 0x08B34DA8 on this+8 and this+0x18, then tail call on this+0x28.

// 0x08B1FAB4: call 0x08B344E0(this+0xC), call 0x08B1FC78(this, a1, a2), then tail call 0x08B3457C(this+0xC).
void func_08B1FAB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 inner = self + 0xCu;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08B344E0u, {inner});
        call(rt, ctx, 0x08B1FC78u, {self, a1, a2});
    }
    tail_call(ctx, 0x08B3457Cu, {inner});
}

// 0x08B32880: vtable 0x08CDA730, zero words +4 and +8, block = allocate(0x2C00); bytes +0xC..+0x14 = 0; +0x18 = block; v0 = block.
void func_08B32880(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CDA730u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    const u32 block = call(rt, ctx, 0x08B318E4u, {0x2C00u});
    for (u32 offset = 0xCu; offset <= 0x14u; ++offset) rt.memory().store8(self + offset, 0);
    put(rt, self + 0x18u, block);
    ctx.gpr[2] = block;
}

// 0x08B779F0: call 0x08C7A838; v0 = its result.

// 0x08AFAB1C: call 0x08B52CE4(this+0x2C, [this+0x10], [this+0x14], [this+0x18], [this+0x1C]), then
// v0 = (0x08B52CEC(this+0x2C, a2, a1) != 0).
void func_08AFAB1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 inner = self + 0x2Cu;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B52CE4u, {inner, word(rt, self + 0x10u), word(rt, self + 0x14u), word(rt, self + 0x18u),
                               word(rt, self + 0x1Cu)});
    const u32 result = call(rt, ctx, 0x08B52CECu, {inner, a2, a1});
    ctx.gpr[2] = result != 0u ? 1u : 0u;
}

// 0x08A7C8A4: fill(&slot); transfer(&object, &slot, a2); release(this, object); destroy(&slot).

// 0x08A7C910: as 08A7C8A4 with the transfer 0x08A74A4C.

// 0x08A7C97C: as 08A7C8A4 with the fill 0x08A69B3C.

} // namespace
} // namespace new_batch_main_1123
} // namespace gt::game

#define NEWM1123(address, name) GT_DECOMP(address, gt::game::new_batch_main_1123::name, gt::ReturnKind::Word)
NEWM1123(0x08A8066Cu, func_08A8066C);
NEWM1123(0x08A9E768u, func_08A9E768);
NEWM1123(0x08AA0A40u, func_08AA0A40);
NEWM1123(0x08AB34FCu, func_08AB34FC);
NEWM1123(0x08B09BF4u, func_08B09BF4);
NEWM1123(0x08B1660Cu, func_08B1660C);
NEWM1123(0x08B1FAB4u, func_08B1FAB4);
NEWM1123(0x08B32880u, func_08B32880);
NEWM1123(0x08AFAB1Cu, func_08AFAB1C);
