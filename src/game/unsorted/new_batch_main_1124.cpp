// New batch main-session 1124: 27 functions drafted from tools.disasm (structured C, unverified).
// Skipped (unsure): 08B1CD20 (six stack arguments), 08ADFFF4 (dynamic stack allocation), 08B28ED4 (float argument pair).
// Shape of most of them: allocate a 0x18..0x20 block, let a constructor-like callee fill it, store the block at sp+0, then call
// the consumer with (self, sp). The delay slot of each such jal stores v0 before the call, so the stored value is the allocation.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1124 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Unaligned 16-byte copy (lwl/lwr, swl/swr) of the words at src to dst, after a call(dst, src). v0 = the first word.
void copy16_words_after_call(Runtime &rt, AllegrexContext &ctx, u32 callee) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    call(rt, ctx, callee, {dst, src});
    const u32 w0 = word(rt, src);
    const u32 w1 = word(rt, src + 4u);
    const u32 w2 = word(rt, src + 8u);
    const u32 w3 = word(rt, src + 12u);
    put(rt, dst, w0);
    put(rt, dst + 4u, w1);
    put(rt, dst + 8u, w2);
    put(rt, dst + 12u, w3);
    ctx.gpr[2] = w0;
}

// 16-byte copy by eight halfwords (lhu/sh), after a call(dst, src). v0 = the halfword at src+0xE (loaded first).
void copy16_halves_after_call(Runtime &rt, AllegrexContext &ctx, u32 callee) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    call(rt, ctx, callee, {dst, src});
    std::uint16_t halves[8];
    for (u32 i = 0; i < 8u; ++i) halves[i] = static_cast<std::uint16_t>(rt.memory().load16(src + 2u * i));
    for (u32 i = 0; i < 8u; ++i) rt.memory().store16(dst + 2u * i, halves[i]);
    ctx.gpr[2] = halves[7];
}

} // namespace

// 0x08A7C9E8 (a0 = self, a1, a2): three locals on the stack, four callees, v0 = the last callee's.

// 0x08AC4188 (a0 = self): resets the object's fields, calls two setup functions, then tail-calls 0x08AC30AC on self+0xC94.
void func_08AC4188(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self + 0x10u, 0x40400000u); // 3.0f
        rt.memory().store8(self + 5u, 0);
        put(rt, self, 0u);
        put(rt, self + 8u, 0xFFFFFFFFu);
        rt.memory().store8(self + 4u, 0);
        put(rt, self + 0xCu, 0x3F800000u); // 1.0f
        const u32 p = call(rt, ctx, 0x08B1ECC8u, {self});
        call(rt, ctx, 0x08B1EDE0u, {self + 0x14u, p});
        call(rt, ctx, 0x08AC2E7Cu, {self + 0x9DCu});
    }
    tail_call(ctx, 0x08AC30ACu, {self + 0xC94u});
}

// 0x08A5DE78 (a0 = self, a1 kept by the allocator's a1 = 4, a2, a3): allocate 0x20, construct, store the block, consume.
void func_08A5DE78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A5D7A0u, {block, ctx.gpr[5], a2, a3});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {self, sp});
}

// 0x08A57AD4 (a0 = self, a1, a2, a3): as 0x08A5DE78, with the incoming a1 passed to the constructor.
void func_08A57AD4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A57714u, {block, a1, a2, a3});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {self, sp});
}

// 0x08A830F0 (a0 = self, a2, a3): as 0x08A5DE78 with constructor 0x08A82AF0.
void func_08A830F0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A82AF0u, {block, ctx.gpr[5], a2, a3});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {self, sp});
}

// 0x08A8B028 (a0 = self, a2, a3): as 0x08A830F0 with constructor 0x08A8A888.
void func_08A8B028(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A8A888u, {block, ctx.gpr[5], a2, a3});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {self, sp});
}

// 0x08A8A5E0 (a0 = self, a1, a2): allocate 0x20, constructor 0x08A8A278 with a1 and a2, consumer 0x08A5D190.
void func_08A8A5E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A8A278u, {block, a1, a2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A5D190u, {self, sp});
}

// 0x08A75124 (a0 = self, a1, a2): allocate 0x18, constructor 0x08A74EBC, consumer 0x08A86CF8.
void func_08A75124(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A74EBCu, {block, a1, a2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A86CF8u, {self, sp});
}

// 0x08A7B648 (a0 = self, a1, a2): allocate 0x18, constructor 0x08A7B500, consumer 0x08A5D40C.
void func_08A7B648(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A7B500u, {block, a1, a2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A5D40Cu, {self, sp});
}

// 0x08A91E60 (a0 = self, a1, a2): allocate 0x18, constructor 0x08A919F4, consumer 0x08A5D40C.
void func_08A91E60(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A919F4u, {block, a1, a2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A5D40Cu, {self, sp});
}

// 0x08AAF900 (a0 = self, a1, a2): allocate 0x1C, constructor 0x08AAF838, consumer 0x08A76AB4.
void func_08AAF900(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    call(rt, ctx, 0x08AAF838u, {block, a1, a2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {self, sp});
}

// 0x08B3FB80 and 0x08B3FBF0: call(a0, a1), then copy 16 bytes from a1 to a0 (unaligned words). Same code, two addresses.
void func_08B3FB80(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_words_after_call(rt, ctx, 0x08B3F838u);
}

void func_08B3FBF0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_words_after_call(rt, ctx, 0x08B3F838u);
}

// 0x08B3FC60, 0x08B3FCD0, 0x08B3FD40, 0x08B3FDB0: call(a0, a1), then copy 16 bytes from a1 to a0 as eight halfwords.
void func_08B3FC60(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_halves_after_call(rt, ctx, 0x08B3F838u);
}

void func_08B3FCD0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_halves_after_call(rt, ctx, 0x08B3F838u);
}

void func_08B3FD40(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_halves_after_call(rt, ctx, 0x08B3F838u);
}

void func_08B3FDB0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    copy16_halves_after_call(rt, ctx, 0x08B3F838u);
}

// 0x08B48638 (a0 = object): writes the vtable 0x08CDAB78 and zeroes the fields, calls two member constructors, returns 0.
void func_08B48638(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CDAB78u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    call(rt, ctx, 0x08C52464u, {self + 0xCu});
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    call(rt, ctx, 0x08C52494u, {self + 0x2Cu});
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x38u, 0u);
    rt.memory().store8(self + 0x3Cu, 0);
    ctx.gpr[2] = 0u;
}

// 0x08A81494 (a0 = self): vtable 0x08CB8798, self-linked list head at +0x14, two constructors on a local, returns the last call's v0.
void func_08A81494(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB8798u);
    const u32 head = self + 0x14u;
    const u32 inner = self + 0x18u;
    put(rt, head + 0x10u, inner);
    put(rt, head + 0x0Cu, inner);
    put(rt, head + 0x14u, 0u);
    put(rt, head + 4u, 0u);
    put(rt, head + 8u, 0u);
    put(rt, head, self);
    call(rt, ctx, 0x08A812D4u, {sp});
    call(rt, ctx, 0x08A76ABCu, {self + 0x2Cu, sp});
    call(rt, ctx, 0x08A5B9ECu, {sp});
    call(rt, ctx, 0x08A76AA0u, {self + 0x30u});
}

// 0x08AD67E0 (a0 = self, a1 = byte count, a2 = element count, a3): aligned size = (a1 + 3) & ~3, product = a2 * size.
// Tail-calls 0x08B2B760 with (self, r3, product, size).
void func_08AD67E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 aligned = (a1 + 3u) & ~3u;
    const u32 product = a2 * aligned;
    u32 r3 = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08C3E138u, {self, a1, a2, a3});
        call(rt, ctx, 0x08B2AE10u, {self + 0x1Cu, a2});
        r3 = call(rt, ctx, 0x08B2B5E0u, {product});
    }
    tail_call(ctx, 0x08B2B760u, {self, r3, product, aligned});
}

// 0x08A7F870 (a0 = self, a1, a2): two local blocks, destructed in order; returns self.
void func_08A7F870(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 l1 = ctx.gpr[29];
    const u32 l2 = l1 + 4u;
    call(rt, ctx, 0x08A69654u, {l1, a2});
    call(rt, ctx, 0x08A7F5FCu, {l2, a1, l1});
    call(rt, ctx, 0x08B88F44u, {self, word(rt, l2) + 4u});
    call(rt, ctx, 0x08A69D68u, {l2});
    call(rt, ctx, 0x08A69D68u, {l1});
    ctx.gpr[2] = self;
}

// 0x08AB5084 (a0 = object): calls 0x08A5B828, then sets the vtable 0x08CD5798 and the fields; returns 1.
void func_08AB5084(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A5B828u, {self});
    put(rt, self, 0x08CD5798u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    const u32 inner = self + 0x1Cu;   // a0 of the asm: the inner block at +0x1C
    put(rt, inner + 4u, 0u);
    put(rt, inner + 8u, 0u);
    const u32 vtable_inner = 0x08D74FD0u + 0xCu; // lui/addiu 0x08D74FD0, then +0xC
    put(rt, self + 0x2Cu, vtable_inner);
    rt.memory().store8(self + 0x38u, 1);
    rt.memory().store8(self + 0x39u, 0);
    put(rt, self + 0x28u, vtable_inner);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08B28ACC and 0x08B28B44 (a0 = object, a1, a2, a3, t0): call 0x08B27250 on the object, set the vtable 0x08CD9678
// and zero +0xA4, then tail-call 0x08B28AA4 with (object, a1, a2, a3, t0). The fifth argument travels in t0.


// 0x08A5B690 (a0 = object, a1, a2): block constructed and destructed around 0x08A52A98 on a1; sets vtable 0x08CB8400.
void func_08A5B690(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x08A69B3Cu, {sp, 0x08CAFB60u});
    call(rt, ctx, 0x08A52660u, {self, sp});
    call(rt, ctx, 0x08A69D68u, {sp});
    put(rt, self, 0x08CB8400u);
    call(rt, ctx, 0x08A52A98u, {self + 0x38u, a1});
    put(rt, self + 0x3Cu, a2);
    rt.memory().store8(self + 0x40u, 0);
}

// 0x08B245FC (a0 = dst, a1 = src): call 0x08B344E0 on src+0x1C; lo = word(src+0x10) + word(src+8) * word(src+0x14);
// r = 0x08B24AE8(src); dst[4] = r, dst[0] = lo; call 0x08B3457C on src+0x1C; returns dst.

// 0x08B24678 (a0 = dst, a1 = src): as 0x08B245FC, reading src+4 and calling 0x08B24CF0.
void func_08B24678(Runtime &rt, AllegrexContext &ctx) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 field = src + 0x1Cu;
    call(rt, ctx, 0x08B344E0u, {field, src});
    const u32 lo = word(rt, src + 0x10u) + word(rt, src + 0x4u) * word(rt, src + 0x14u);
    const u32 r = call(rt, ctx, 0x08B24CF0u, {src});
    put(rt, dst + 4u, r);
    put(rt, dst, lo);
    call(rt, ctx, 0x08B3457Cu, {field});
    ctx.gpr[2] = dst;
}

} // namespace new_batch_main_1124
} // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1124::name, gt::ReturnKind::kind)
NEW3(0x08AC4188u, func_08AC4188, Word);
NEW3(0x08A5DE78u, func_08A5DE78, Word);
NEW3(0x08A57AD4u, func_08A57AD4, Word);
NEW3(0x08A830F0u, func_08A830F0, Word);
NEW3(0x08A8B028u, func_08A8B028, Word);
NEW3(0x08A8A5E0u, func_08A8A5E0, Word);
NEW3(0x08A75124u, func_08A75124, Word);
NEW3(0x08A7B648u, func_08A7B648, Word);
NEW3(0x08A91E60u, func_08A91E60, Word);
NEW3(0x08AAF900u, func_08AAF900, Word);
NEW3(0x08B3FB80u, func_08B3FB80, Word);
NEW3(0x08B3FBF0u, func_08B3FBF0, Word);
NEW3(0x08B3FC60u, func_08B3FC60, Word);
NEW3(0x08B3FCD0u, func_08B3FCD0, Word);
NEW3(0x08B3FD40u, func_08B3FD40, Word);
NEW3(0x08B3FDB0u, func_08B3FDB0, Word);
NEW3(0x08B48638u, func_08B48638, Word);
NEW3(0x08A81494u, func_08A81494, Word);
NEW3(0x08AD67E0u, func_08AD67E0, Word);
NEW3(0x08A7F870u, func_08A7F870, Word);
NEW3(0x08AB5084u, func_08AB5084, Word);
NEW3(0x08A5B690u, func_08A5B690, Word);
NEW3(0x08B24678u, func_08B24678, Word);
