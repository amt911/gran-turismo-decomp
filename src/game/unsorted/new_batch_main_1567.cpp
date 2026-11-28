// New batch main-1567: 27 functions drafted from the asm (tools.disasm), structured C. Unverified: scripts verify later.
// Skipped (not drafted): 0x08B56674 (VFPU lv.s), 0x08B62450 (the asm runs past its last instruction), 0x08A17CD8 (stack arguments t0/t1).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1567 {
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

// 0x08ACC9D0: calls 0x08ACC6B4(this); when this->+4 is set, calls the vtable slot 0 of that object with (obj, this->+0),
// then clears this->+0 and this->+4.

// 0x08AD5524: copies the pair at a1 into this+4/+8, clears the byte at +0xC, returns 1 (after 0x08AD5394(this)).
void func_08AD5524(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    call(rt, ctx, 0x08AD5394u, {self});
    const u32 first = word(rt, src);
    const u32 second = word(rt, src + 4u);
    rt.memory().store8(self + 0xCu, 0u);
    put(rt, self + 4u, first);
    put(rt, self + 8u, second);
    ctx.gpr[2] = 1u;
}

// 0x08B20C00: calls the method at (node->+4)->+8 with the entry a0 for every node of the list at 0x08D74F44;
// v0 is the last call's result, or 0x000A0000 when the list is empty.

// 0x08B468BC: 0x08B46880(this); r = 0x08C7AED0(0x08B46868, this); when r >= 0 stores it at this+0x4898 and returns 1, else 0.

// 0x08C15F9C / 0x08C16084: copy 16-byte blocks from a0 up to a1 into a2 (the destination is advanced by 0x10 on every pass,
// and copied only when it is non-null, as the asm's first test leaves it). v0 = the final destination.
void copy_blocks(Runtime &rt, AllegrexContext &ctx) {
    u32 cur = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 dst = ctx.gpr[6];
    ctx.gpr[2] = dst;
    if (cur == end) return;
    u32 next;
    do {
        next = cur + 0x10u;
        if (dst != 0u) {
            for (u32 k = 0; k < 4u; ++k) put(rt, dst + 4u * k, word(rt, cur + 4u * k));
        }
        cur = next;
        dst += 0x10u;
    } while (next != end);
    ctx.gpr[4] = cur;
    ctx.gpr[2] = dst;
}
void func_08C15F9C(Runtime &rt, AllegrexContext &ctx) { copy_blocks(rt, ctx); }
void func_08C16084(Runtime &rt, AllegrexContext &ctx) { copy_blocks(rt, ctx); }

// 0x08C55C24: stores the vtable and the payload of a1 into this (the payload at this + vtable[-0xC]), then calls 0x08C54A7C
// on this + the new vtable's [-0xC] offset.

// 0x088094DC / 0x08809528: local = sp; 0x08808F2C(local, a1, a2, a3) with t0 = the table address; when this != local,
// 0x08A5BAD8(this, *local); then 0x08A5B9EC(local). They differ only in a3 and t0.
void func_088094DC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 local = ctx.gpr[29];
    ctx.gpr[8] = 0x08808640u;
    call(rt, ctx, 0x08808F2Cu, {local, ctx.gpr[5], ctx.gpr[6], 1u});
    if (self != local) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, local)});
    call(rt, ctx, 0x08A5B9ECu, {local});
}
void func_08809528(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 local = ctx.gpr[29];
    ctx.gpr[8] = 0x088085B8u;
    call(rt, ctx, 0x08808F2Cu, {local, ctx.gpr[5], ctx.gpr[6], 0u});
    if (self != local) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, local)});
    call(rt, ctx, 0x08A5B9ECu, {local});
}

// 0x088CB474: for i in 0..3: r = 0x088CB360(i); word at r+0x600 = a1 (the store is in the loop's branch delay slot).

// 0x088D4310: when this is non-null, calls 0x08B09290 on this+0x50, this+0x28 and this (in that order).

// 0x088F1358: r = 0x088EF910(*(this+0x18)); when r >= 5 (unsigned), v0 = (0x088EF918(*(this+0x18)) == 0); otherwise v0 = 0.

// 0x08994DC4: stores two vtables at this, this+0x38; calls 0x08A5B9EC on this+0x11C and this+0x118; tail call 0x08A0298C(this).

// 0x089E8848: r = 0x089BD668(a0, a1); tail call 0x089E8408(a0, r, a1, a2).

// 0x08A135B8: calls 0x08B34888(this+0x10) and 0x08B34B24(this+0x10); v0 = the word at this+0x1C0 read between them.

// 0x08A34AB8: this->vtable = 0x08CD1B20; when this->+0x14 is set, 0x08B2E28C(it); 0x08A5B9EC(this+0x18); tail call 0x08A780B0(this).

// 0x08A35848: vtable = 0x08CCE588; 0x08A35114(this); 0x08A5B9EC on this+0x1C, +0x18, +0x14; tail call 0x08A780B0(this).

// 0x08A57EE0: fn = this->+0x14 (read first); vtable = 0x08CD2100; when fn is set and this->+0xC is set, fn(this->+0xC); tail call 0x08A5BB5C(this).

// 0x08A58518: obj = this->+0x14 (an error call to 0x08A5BB7C when it is null); r = vtable[0x120](obj, ...); v0 = (r + 0xF) & ~0xF.

// 0x08A5D888 / 0x08A82BD8 / 0x08A8A970: when this != src, 0x08A781D4(this, src) and copy src+0x18/+0x1C to this; v0 = this.
void copy_pair(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    if (self != src) {
        call(rt, ctx, 0x08A781D4u, {self, src});
        put(rt, self + 0x18u, word(rt, src + 0x18u));
        put(rt, self + 0x1Cu, word(rt, src + 0x1Cu));
    }
    ctx.gpr[2] = self;
}

// 0x08A66DB8: vtable = 0x08CD2E30; this+4 = 0x24 (byte store, then the masked word); 0x08A75D04(this+8); this+0x14 = 0.

// 0x08A8522C: vtable = 0x08CBEA58; when this->+0x14 is set, its vtable slot 1 is called with it; tail call 0x08A780B0(this).

// 0x08AB9740 / 0x08AB994C: walk the list at 0x08D7717C (next at +0, object at +4) and call vtable slot 0x1C (0x34 for the second)
// on each object; v0 = the last call's result, or 0x000A0000 when the list is empty.

// 0x08AB9FC0: walk the list at 0x08CFFA28 (object at +0, next at +4) and call vtable slot 0x14 on each object;
// v0 = the last call's result, or 0x00020000 when the list is empty.

} // namespace
} // namespace new_batch_main_1567
} // namespace gt::game

#define NEW1567(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1567::name, gt::ReturnKind::kind)
NEW1567(0x08AD5524u, func_08AD5524, Word);
NEW1567(0x08C15F9Cu, func_08C15F9C, Word);
NEW1567(0x08C16084u, func_08C16084, Word);
NEW1567(0x088094DCu, func_088094DC, Word);
NEW1567(0x08809528u, func_08809528, Word);
