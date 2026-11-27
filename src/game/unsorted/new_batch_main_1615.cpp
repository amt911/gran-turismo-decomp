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
namespace new_batch_main_1615 {
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
// 0x089B9828 (size 0x80): func_089B932C, then func_089BAC44(this, &block).
// 0x089CC710 (size 0x18): func_089CC678, then func_08A5BAB8(this, &block).
// 0x089D08B0 (size 0x1D0): func_089D0704, then func_0899537C(this, &block).
// 0x089E23E0 (size 0x230): func_089E2210, then func_08A0815C(this, &block).
// 0x08A13E20 (size 0x7C0): func_08A12C54, then func_089C15F4(this, &block).
// 0x08A37CBC (size 0x1C): func_08A37B88, then func_08A76AB4(this, &block).
// 0x08A393EC (size 0xD8): func_08A38A70, then func_08A76AB4(this, &block).
// 0x08A39CCC (size 0x14C): func_08A398F4, then func_08A76AB4(this, &block).
// 0x08A3AE10 (size 0x38): func_08A3AAB8, then func_08A76AB4(this, &block).
// 0x08A3E6FC (size 0x1C): func_08A3E5BC, then func_08A76AB4(this, &block).
// 0x08A3F7F8 (size 0x98): func_08A3EA24, then func_08A76AB4(this, &block).
// 0x08A3F898 (size 0x38): func_08A3EADC, then func_08A76AB4(this, &block).
// 0x08A3F938 (size 0x108): func_08A3EB94, then func_08A76AB4(this, &block).
// 0x08A402D4 (size 0x10C): func_08A401EC, then func_08A76AB4(this, &block).
// 0x08A41118 (size 0x20): func_08A41088, then func_08A76AB4(this, &block).
// 0x08A43BC8 (size 0x48): func_08A43180, then func_08A76AB4(this, &block).
// 0x08A44C7C (size 0x20): func_08A44460, then func_08A76AB4(this, &block).

// 0x089FE4A8 (this = a0, argument = a1): base constructor func_08A0D658(this, argument), vtable words 0x08CC84E0 at
// +0 and 0x08CC8738 at +0x38, then a tail call to func_08A17B10(this + 0x120, argument); v0 = 0x08CC8738.

// 0x08A4DE78 (a0 = object, a1 = member stored at +0x20, a2 = byte stored at +0x24): base construct func_08B36290,
// vtable 0x08CD1EF8 at +0, the member, and the byte (a2 & 0xFF); v0 = 0x08CD1EF8.

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

// 0x089FF1C0 (a0 = object): base construct func_08A0FDD8, vtable words 0x08CC73A8 at +0 and 0x08CC7588 at +0x38, a
// member built at +0x78 from this, the word 1 at +0x94 and a zero byte at +0x98; v0 = 1.

// 0x08A03D58 (a0 = object): base construct func_08A76ACC, vtable 0x08CBBE10 at +0, four floats of 1.0 at
// +0x20..+0x2C and the word -1 at +0x30; v0 = -1 and f0 = 1.0.

// 0x08A1A8C0 (a0 = object): base construct func_08AD023C, vtable 0x08CD0698 at +0, +0x64 and +0x90 cleared, members
// built at +0x68 and +0x44, then the tail call func_08ABFF0C(this + 0x30, 1); v0 is that member's constructor.

// 0x08A32B74 (a0 = object): base construct func_08A76ACC, vtable 0x08CB8AC8 at +0, +0x14 built, +0x18 built from it,
// +0x42C cleared; v0 is the second constructor's answer.

// 0x08B1BCFC (a0 = object): base construct func_08B344E0, members at +0x98, +0x78 and +0x28 constructed, then the
// float at +0x90 is incremented by 1.0 and the tail call func_08B3457C(this) runs; v0 is the last constructor's
// answer and f0 the incremented float.

// 0x08C247E4 (a0 = object, a1 = a pointer to the object whose virtual answers): the word at 0(a1) is the object; a
// null one asserts through func_08A5BB7C(0x08CB06B8), then its vtable slot 0x44 is called with the object and the
// answer is stored as a byte at +0x14 of a0; v0 is that answer.

} // namespace
} // namespace new_batch_main_1615
} // namespace gt::game

#define NEW1615(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1615::name, gt::ReturnKind::kind)
