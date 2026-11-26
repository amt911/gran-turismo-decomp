// New batch (main session 897): functions the extra seeds made emittable, written as structured C from `tools.disasm`.
// Draft only: the differential gate decides which land. Twenty-nine are one shape: a0 = size, a1 = 4 goes to
// func_08A5BC20 (allocate), the block gets its constructor, the block's address lands at 0(sp) in the registrar call's
// delay slot and the registrar takes (this, &block); v0 is the registrar's answer. The thirtieth (0x089FE4A8) runs the
// base constructor, stores two vtable words into the object and tail-calls func_08A17B10 on its +0x120 sub-object.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_897 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// a0 = size, a1 = 4: func_08A5BC20 allocates the block, `ctor` initialises it, then `registrar` takes (this, &block)
// with the block's address stored at 0(sp). s0 holds the block and s1 this, as in the original, and both come back at
// the exit the frame's `lw`s restore; v0 is the registrar's answer.
void allocate_construct_register(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 registrar) {
    const u32 self = ctx.gpr[4];
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[17] = self;
        const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
        ctx.gpr[16] = object;
        (void)call(rt, ctx, ctor, {object});
        put(rt, frame.at(0u), object);
        (void)call(rt, ctx, registrar, {self, frame.at(0u)});
    }
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
}

// 0x089E8CA4 (size 0x1F0): allocate + func_089E8ADC, then func_0899AE08(this, &block).
void func_089E8CA4(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1F0u, 0x089E8ADCu, 0x0899AE08u);
}
// 0x089ED274 (size 0x150): func_089ED020, then func_08993BF4(this, &block).
void func_089ED274(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x150u, 0x089ED020u, 0x08993BF4u);
}
// 0x089ED798 (size 0x140): func_089ED544, then func_089FE8D8(this, &block).
void func_089ED798(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x140u, 0x089ED544u, 0x089FE8D8u);
}
// 0x089EFC40 (size 0xE70): func_089EFB10, then func_089E119C(this, &block).
void func_089EFC40(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0xE70u, 0x089EFB10u, 0x089E119Cu);
}
// 0x089F3CD0 (size 0x280): func_089F3B58, then func_089E119C(this, &block).
void func_089F3CD0(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x280u, 0x089F3B58u, 0x089E119Cu);
}
// 0x089F6E00 (size 0xA0): func_089F61A0, then func_08A0FD20(this, &block).
void func_089F6E00(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0xA0u, 0x089F61A0u, 0x08A0FD20u);
}
// 0x089F9810 (size 0xB8): func_089F967C, then func_089D8A60(this, &block).
void func_089F9810(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0xB8u, 0x089F967Cu, 0x089D8A60u);
}
// 0x089FC670 (size 0x120): func_089FB954, then func_08A0D868(this, &block).
void func_089FC670(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x120u, 0x089FB954u, 0x08A0D868u);
}
// 0x089FE4A8 (this = a0, argument = a1): base constructor func_08A0D658(this, argument), vtable words 0x08CC84E0 at +0
// and 0x08CC8738 at +0x38, then a tail call to func_08A17B10(this + 0x120, argument).
// 0x08A02B48 (size 0x120): func_08A02A24, then func_08969190(this, &block).
void func_08A02B48(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x120u, 0x08A02A24u, 0x08969190u);
}
// 0x08A0382C (size 0x120): func_08A035DC, then func_08A013D0(this, &block).
void func_08A0382C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x120u, 0x08A035DCu, 0x08A013D0u);
}
// 0x08A041B4 (size 0x40): func_08A03B64, then func_08A76AB4(this, &block).
void func_08A041B4(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x40u, 0x08A03B64u, 0x08A76AB4u);
}
// 0x08A06730 (size 0xB8): func_08A065A0, then func_089D8A60(this, &block).
void func_08A06730(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0xB8u, 0x08A065A0u, 0x089D8A60u);
}
// 0x08A08E98 (size 0x1B0): func_08A08D14, then func_08969190(this, &block).
void func_08A08E98(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1B0u, 0x08A08D14u, 0x08969190u);
}
// 0x08A0D96C (size 0x120): func_08A0CFC4, then func_08A013D0(this, &block).
void func_08A0D96C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x120u, 0x08A0CFC4u, 0x08A013D0u);
}
// 0x08A0EDFC (size 0x20): func_08A0E8D0, then func_08A76AB4(this, &block).
void func_08A0EDFC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A0E8D0u, 0x08A76AB4u);
}
// 0x08A13E20 (size 0x7C0): func_08A12C54, then func_089C15F4(this, &block).
// 0x08A1C060 (size 0x1C): func_08A1BF20, then func_08A76AB4(this, &block).
void func_08A1C060(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A1BF20u, 0x08A76AB4u);
}
// 0x08A1CA80 (size 0x1C): func_08A1C94C, then func_08A76AB4(this, &block).
void func_08A1CA80(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A1C94Cu, 0x08A76AB4u);
}
// 0x08A1EB94 (size 0x1C): func_08A1EA50, then func_08A76AB4(this, &block).
void func_08A1EB94(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A1EA50u, 0x08A76AB4u);
}
// 0x08A20E30 (size 0x20): func_08A20A68, then func_08A76AB4(this, &block).
void func_08A20E30(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A20A68u, 0x08A76AB4u);
}
// 0x08A21898 (size 0x20): func_08A21550, then func_08A76AB4(this, &block).
void func_08A21898(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A21550u, 0x08A76AB4u);
}
// 0x08A222B4 (size 0x18): func_08A21E6C, then func_08A76AB4(this, &block).
void func_08A222B4(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x18u, 0x08A21E6Cu, 0x08A76AB4u);
}
// 0x08A25298 (size 0x1C): func_08A25158, then func_08A76AB4(this, &block).
void func_08A25298(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A25158u, 0x08A76AB4u);
}
// 0x08A2A3D0 (size 0x18): func_08A2A188, then func_08A76AB4(this, &block).
void func_08A2A3D0(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x18u, 0x08A2A188u, 0x08A76AB4u);
}
// 0x08A2DB28 (size 0x1C): func_08A2D9E8, then func_08A76AB4(this, &block).
void func_08A2DB28(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A2D9E8u, 0x08A76AB4u);
}
// 0x08A318B8 (size 0x1C): func_08A31880, then func_08A76AB4(this, &block).
void func_08A318B8(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A31880u, 0x08A76AB4u);
}
// 0x08A32B24 (size 0x430): func_08A32A40, then func_08A76AB4(this, &block).
void func_08A32B24(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x430u, 0x08A32A40u, 0x08A76AB4u);
}
// 0x08A33758 (size 0x20): func_08A330E0, then func_08A76AB4(this, &block).
void func_08A33758(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A330E0u, 0x08A76AB4u);
}
// 0x08A3705C (size 0x1C): func_08A36DA4, then func_08A76AB4(this, &block).
void func_08A3705C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A36DA4u, 0x08A76AB4u);
}

} // namespace
} // namespace new_batch_main_897
} // namespace gt::game

#define NEW897(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_897::name, gt::ReturnKind::kind)
NEW897(0x089E8CA4u, func_089E8CA4, Word);
NEW897(0x089ED274u, func_089ED274, Word);
NEW897(0x089ED798u, func_089ED798, Word);
NEW897(0x089EFC40u, func_089EFC40, Word);
NEW897(0x089F3CD0u, func_089F3CD0, Word);
NEW897(0x089F6E00u, func_089F6E00, Word);
NEW897(0x089F9810u, func_089F9810, Word);
NEW897(0x089FC670u, func_089FC670, Word);
NEW897(0x08A02B48u, func_08A02B48, Word);
NEW897(0x08A0382Cu, func_08A0382C, Word);
NEW897(0x08A041B4u, func_08A041B4, Word);
NEW897(0x08A06730u, func_08A06730, Word);
NEW897(0x08A08E98u, func_08A08E98, Word);
NEW897(0x08A0D96Cu, func_08A0D96C, Word);
NEW897(0x08A0EDFCu, func_08A0EDFC, Word);
NEW897(0x08A1C060u, func_08A1C060, Word);
NEW897(0x08A1CA80u, func_08A1CA80, Word);
NEW897(0x08A1EB94u, func_08A1EB94, Word);
NEW897(0x08A20E30u, func_08A20E30, Word);
NEW897(0x08A21898u, func_08A21898, Word);
NEW897(0x08A222B4u, func_08A222B4, Word);
NEW897(0x08A25298u, func_08A25298, Word);
NEW897(0x08A2A3D0u, func_08A2A3D0, Word);
NEW897(0x08A2DB28u, func_08A2DB28, Word);
NEW897(0x08A318B8u, func_08A318B8, Word);
NEW897(0x08A32B24u, func_08A32B24, Word);
NEW897(0x08A33758u, func_08A33758, Word);
NEW897(0x08A3705Cu, func_08A3705C, Word);
