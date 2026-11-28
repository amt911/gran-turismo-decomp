// New batch, main session 1615: short constructors and factories the extra seeds made emittable, written by hand as
// structured C from `tools.disasm`. Draft only: the differential gate decides what lands. Two shapes cover all of
// them. The factory (a0 = the container): func_08A5BC20(size, 4) allocates the block, its constructor initialises it,
// and the registrar takes (container, &block) with the block's address stored at 0(sp); v0 is the registrar's answer.
// The constructor (a0 = the object): base constructor, vtable words, members, then either a plain return or a tail
// call. Every argument register the callee reads is passed, the s-registers are set and restored as the prologue
// does, and v0 is left as the asm leaves it on each path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1882 {
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

// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable, so a callee that
// never writes v0 returns its own address.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                         std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// The s-registers the bodies keep values in: the prologue saves them in the frame and the epilogue restores them, so
// the caller's values come back at the exit (a callee can read them too, so they are set as the asm sets them).
struct SavedS {
    AllegrexContext &ctx;
    u32 s0;
    u32 s1;
    u32 s2;
    explicit SavedS(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]), s2(c.gpr[18]) {}
    ~SavedS() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
    }
    SavedS(const SavedS &) = delete;
    SavedS &operator=(const SavedS &) = delete;
};

// a0 = the container: func_08A5BC20(size, 4) allocates the block (s0), its constructor takes (block, 4), and the
// registrar takes (container, &block) with the block's address stored at 0(sp); v0 is the registrar's answer.
void allocate_construct_register(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 registrar) {
    const u32 container = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedS saved(ctx);
    ctx.gpr[17] = container;
    const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    ctx.gpr[16] = object;
    (void)call(rt, ctx, ctor, {object, 4u});
    put(rt, frame.at(0u), object);
    (void)call(rt, ctx, registrar, {container, frame.at(0u)});
}

// 0x089B67AC (size 0x180): func_089B6698, then func_089A274C(this, &block).
void func_089B67AC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x180u, 0x089B6698u, 0x089A274Cu);
}
// 0x089B9828 (size 0x80): func_089B932C, then func_089BAC44(this, &block).
void func_089B9828(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x80u, 0x089B932Cu, 0x089BAC44u);
}
// 0x089CC710 (size 0x18): func_089CC678, then func_08A5BAB8(this, &block).
void func_089CC710(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x18u, 0x089CC678u, 0x08A5BAB8u);
}
// 0x089D08B0 (size 0x1D0): func_089D0704, then func_0899537C(this, &block).
void func_089D08B0(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1D0u, 0x089D0704u, 0x0899537Cu);
}
// 0x089E23E0 (size 0x230): func_089E2210, then func_08A0815C(this, &block).
void func_089E23E0(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x230u, 0x089E2210u, 0x08A0815Cu);
}
// 0x08A13E20 (size 0x7C0): func_08A12C54, then func_089C15F4(this, &block).
// 0x08A37CBC (size 0x1C): func_08A37B88, then func_08A76AB4(this, &block).
void func_08A37CBC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A37B88u, 0x08A76AB4u);
}
// 0x08A393EC (size 0xD8): func_08A38A70, then func_08A76AB4(this, &block).
void func_08A393EC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0xD8u, 0x08A38A70u, 0x08A76AB4u);
}
// 0x08A39CCC (size 0x14C): func_08A398F4, then func_08A76AB4(this, &block).
void func_08A39CCC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x14Cu, 0x08A398F4u, 0x08A76AB4u);
}
// 0x08A3AE10 (size 0x38): func_08A3AAB8, then func_08A76AB4(this, &block).
void func_08A3AE10(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x38u, 0x08A3AAB8u, 0x08A76AB4u);
}
// 0x08A3E6FC (size 0x1C): func_08A3E5BC, then func_08A76AB4(this, &block).
void func_08A3E6FC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x1Cu, 0x08A3E5BCu, 0x08A76AB4u);
}
// 0x08A3F7F8 (size 0x98): func_08A3EA24, then func_08A76AB4(this, &block).
void func_08A3F7F8(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x98u, 0x08A3EA24u, 0x08A76AB4u);
}
// 0x08A3F898 (size 0x38): func_08A3EADC, then func_08A76AB4(this, &block).
void func_08A3F898(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x38u, 0x08A3EADCu, 0x08A76AB4u);
}
// 0x08A3F938 (size 0x108): func_08A3EB94, then func_08A76AB4(this, &block).
void func_08A3F938(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x108u, 0x08A3EB94u, 0x08A76AB4u);
}
// 0x08A402D4 (size 0x10C): func_08A401EC, then func_08A76AB4(this, &block).
void func_08A402D4(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x10Cu, 0x08A401ECu, 0x08A76AB4u);
}
// 0x08A41118 (size 0x20): func_08A41088, then func_08A76AB4(this, &block).
void func_08A41118(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A41088u, 0x08A76AB4u);
}
// 0x08A43BC8 (size 0x48): func_08A43180, then func_08A76AB4(this, &block).
void func_08A43BC8(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x48u, 0x08A43180u, 0x08A76AB4u);
}
// 0x08A44C7C (size 0x20): func_08A44460, then func_08A76AB4(this, &block).
void func_08A44C7C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_register(rt, ctx, 0x20u, 0x08A44460u, 0x08A76AB4u);
}

// 0x089FE4A8 (this = a0, argument = a1): base constructor func_08A0D658(this, argument), vtable words 0x08CC84E0 at
// +0 and 0x08CC8738 at +0x38, then a tail call to func_08A17B10(this + 0x120, argument); v0 = 0x08CC8738.
void func_089FE4A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    u32 sub_object;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedS saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        (void)call(rt, ctx, 0x08A0D658u, {self, argument});
        put(rt, self, 0x08CC84E0u);
        put(rt, self + 0x38u, 0x08CC8738u);
        sub_object = self + 0x120u;
        ctx.gpr[2] = 0x08CC8738u;
    }
    tail_call(ctx, 0x08A17B10u, {sub_object, argument});
}

// 0x08A4DE78 (a0 = object, a1 = member stored at +0x20, a2 = byte stored at +0x24): base construct func_08B36290,
// vtable 0x08CD1EF8 at +0, the member, and the byte (a2 & 0xFF); v0 = 0x08CD1EF8.
void func_08A4DE78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 member = ctx.gpr[5];
    const u32 byte = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    SavedS saved(ctx);
    ctx.gpr[16] = self;
    ctx.gpr[17] = byte;
    ctx.gpr[18] = member;
    (void)call(rt, ctx, 0x08B36290u, {self, member, ctx.gpr[6]});
    rt.memory().store8(self + 0x24u, static_cast<std::uint8_t>(byte));
    put(rt, self, 0x08CD1EF8u);
    put(rt, self + 0x20u, member);
    ctx.gpr[2] = 0x08CD1EF8u;
}

// 0x08B10C24 (a0 = object, a1 = the body's argument): lock func_08B34888(this + 0x14), body func_08B10A7C(this, a1),
// then the tail call is the unlock func_08B34B24(this + 0x14); v0 is the body's answer.

// 0x0886A7B0 (a0 = object): base at +0x4238 through func_0886BFC0, its vtable word 0x08CBF6E8 at +0x4240,
// func_0886D88C(+0x4238), func_08B28DC4(+0x4210), then the tail call func_08875138(this + 8); v0 is the third
// call's answer.

// 0x08935044 (a0 = object): func_08934F78(word +0, word +4), then the four words at +0xC0..+0xCC of the global
// 0x08CF2968 are copied into +0x58..+0x64; v0 = the first copied word and the tail call func_0893388C gets the last
// two (a0/a1 as the asm leaves them).

// 0x0899A984 (a0 = object): base construct func_089946BC, vtable words 0x08CC9138 at +0 and 0x08CC9390 at +0x38, six
// halfwords cleared at +0x120..+0x12A; v0 = 0x08CC9138.
void func_0899A984(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedS saved(ctx);
    ctx.gpr[16] = self;
    (void)call(rt, ctx, 0x089946BCu, {self});
    put(rt, self, 0x08CC9138u);
    put(rt, self + 0x38u, 0x08CC9390u);
    rt.memory().store16(self + 0x12Au, 0u);
    rt.memory().store16(self + 0x120u, 0u);
    rt.memory().store16(self + 0x122u, 0u);
    rt.memory().store16(self + 0x124u, 0u);
    rt.memory().store16(self + 0x126u, 0u);
    rt.memory().store16(self + 0x128u, 0u);
    ctx.gpr[2] = 0x08CC9138u;
}

// 0x089FF1C0 (a0 = object): base construct func_08A0FDD8, vtable words 0x08CC73A8 at +0 and 0x08CC7588 at +0x38, a
// member built at +0x78 from this, the word 1 at +0x94 and a zero byte at +0x98; v0 = 1.
void func_089FF1C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedS saved(ctx);
    ctx.gpr[16] = self;
    (void)call(rt, ctx, 0x08A0FDD8u, {self});
    put(rt, self, 0x08CC73A8u);
    put(rt, self + 0x38u, 0x08CC7588u);
    (void)call(rt, ctx, 0x089B883Cu, {self + 0x78u, self});
    put(rt, self + 0x94u, 1u);
    rt.memory().store8(self + 0x98u, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08A03D58 (a0 = object): base construct func_08A76ACC, vtable 0x08CBBE10 at +0, four floats of 1.0 at
// +0x20..+0x2C and the word -1 at +0x30; v0 = -1 and f0 = 1.0.
void func_08A03D58(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedS saved(ctx);
    ctx.gpr[16] = self;
    (void)call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBBE10u);
    const u32 v1 = self + 0x20u;
    put_float(rt, v1 + 0xCu, 1.0f);
    put_float(rt, v1 + 0x8u, 1.0f);
    put_float(rt, v1 + 0x4u, 1.0f);
    put_float(rt, self + 0x20u, 1.0f);
    put(rt, v1 + 0x10u, 0xFFFFFFFFu);
    ctx.fpr[0] = 1.0f;
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08A1A8C0 (a0 = object): base construct func_08AD023C, vtable 0x08CD0698 at +0, +0x64 and +0x90 cleared, members
// built at +0x68 and +0x44, then the tail call func_08ABFF0C(this + 0x30, 1); v0 is that member's constructor.
void func_08A1A8C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedS saved(ctx);
        ctx.gpr[16] = self;
        (void)call(rt, ctx, 0x08AD023Cu, {self});
        put(rt, self, 0x08CD0698u);
        put(rt, self + 0x64u, 0u);
        (void)call(rt, ctx, 0x08B28DCCu, {self + 0x68u});
        put(rt, self + 0x90u, 0u);
        result = call(rt, ctx, 0x08ABFF0Cu, {self + 0x44u, 1u});
        ctx.gpr[2] = result;
    }
    tail_call(ctx, 0x08ABFF0Cu, {self + 0x30u, 1u});
}

// 0x08A32B74 (a0 = object): base construct func_08A76ACC, vtable 0x08CB8AC8 at +0, +0x14 built, +0x18 built from it,
// +0x42C cleared; v0 is the second constructor's answer.
void func_08A32B74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedS saved(ctx);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0x14u;
    (void)call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB8AC8u);
    (void)call(rt, ctx, 0x08A431F8u, {self + 0x14u});
    const u32 result = call(rt, ctx, 0x08A32A14u, {self + 0x18u, self + 0x14u});
    put(rt, self + 0x42Cu, 0u);
    ctx.gpr[2] = result;
}

// 0x08B1BCFC (a0 = object): base construct func_08B344E0, members at +0x98, +0x78 and +0x28 constructed, then the
// float at +0x90 is incremented by 1.0 and the tail call func_08B3457C(this) runs; v0 is the last constructor's
// answer and f0 the incremented float.

// 0x08C247E4 (a0 = object, a1 = a pointer to the object whose virtual answers): the word at 0(a1) is the object; a
// null one asserts through func_08A5BB7C(0x08CB06B8), then its vtable slot 0x44 is called with the object and the
// answer is stored as a byte at +0x14 of a0; v0 is that answer.

} // namespace
} // namespace new_batch_main_1882
} // namespace gt::game

#define NEW1615(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1882::name, gt::ReturnKind::kind)
NEW1615(0x089B67ACu, func_089B67AC, Word);
NEW1615(0x089B9828u, func_089B9828, Word);
NEW1615(0x089CC710u, func_089CC710, Word);
NEW1615(0x089D08B0u, func_089D08B0, Word);
NEW1615(0x089E23E0u, func_089E23E0, Word);
NEW1615(0x089FE4A8u, func_089FE4A8, Word);
NEW1615(0x08A37CBCu, func_08A37CBC, Word);
NEW1615(0x08A393ECu, func_08A393EC, Word);
NEW1615(0x08A39CCCu, func_08A39CCC, Word);
NEW1615(0x08A3AE10u, func_08A3AE10, Word);
NEW1615(0x08A3E6FCu, func_08A3E6FC, Word);
NEW1615(0x08A3F7F8u, func_08A3F7F8, Word);
NEW1615(0x08A3F898u, func_08A3F898, Word);
NEW1615(0x08A3F938u, func_08A3F938, Word);
NEW1615(0x08A402D4u, func_08A402D4, Word);
NEW1615(0x08A41118u, func_08A41118, Word);
NEW1615(0x08A43BC8u, func_08A43BC8, Word);
NEW1615(0x08A44C7Cu, func_08A44C7C, Word);
NEW1615(0x08A4DE78u, func_08A4DE78, Word);
NEW1615(0x0899A984u, func_0899A984, Word);
NEW1615(0x089FF1C0u, func_089FF1C0, Word);
NEW1615(0x08A03D58u, func_08A03D58, Word);
NEW1615(0x08A1A8C0u, func_08A1A8C0, Word);
NEW1615(0x08A32B74u, func_08A32B74, Word);
