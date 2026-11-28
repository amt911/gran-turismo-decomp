// New batch main 2080 (main-session batch, 30 functions). Structured C from tools.disasm; unverified draft.
// Shapes: the allocate/construct/sink family (operator new of a fixed size, a constructor, then a sink with &slot),
// the store-and-call family, the tail-call family, and single-object initialisers.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2080 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The s-registers a function saves: restored when the scope ends, as the prologue/epilogue do.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 s0, s1, s2;
    explicit SavedRegs(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]), s2(c.gpr[18]) {}
    ~SavedRegs() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
    }
};

// Allocates `size` bytes (align 4) with operator new, constructs the object with `ctor` (a1 = ctor_second), then
// calls `sink(self, &slot)` where slot = the object. self = the caller's a0 (kept in s1). v0 = the sink's result.
void allocate_construct_sink(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 ctor_second, u32 sink) {
    const u32 self = ctx.gpr[4];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    ctx.gpr[16] = object;
    call(rt, ctx, ctor, {object, ctor_second});
    put(rt, frame.at(0u), ctx.gpr[16]);
    call(rt, ctx, sink, {self, frame.at(0u)});
}

// Two objects: init(&slot0, a1) builds slot0, build(obj1 = sp+4, &slot0, a2) builds obj1 from it, then three calls on obj1 and
// slot0 (the sink calls with a0 = self). v0 = the last call's result.
void chain_destroy(Runtime &rt, AllegrexContext &ctx, u32 init, u32 build) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;
    ctx.gpr[17] = a2;
    call(rt, ctx, init, {frame.at(0u), a1});
    const u32 object = frame.at(4u);
    ctx.gpr[16] = object;
    call(rt, ctx, build, {object, frame.at(0u), a2});
    call(rt, ctx, 0x08A7C5C8u, {self, object});
    call(rt, ctx, 0x08A5B9ECu, {object});
    call(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// Frame, s-registers and float helpers come from hot_common.hpp (word, put, float_at, put_float).

}  // namespace

// 0x08A39CCC: new 0x14C, construct, sink 0x08A76AB4.

// 0x08A3AE10: new 0x38, construct 0x08A3AAB8, sink 0x08A76AB4.

// 0x08A13E20: new 0x7C0, construct 0x08A12C54, sink 0x089C15F4.

// 0x08A3E6FC: new 0x1C, construct 0x08A3E5BC, sink 0x08A76AB4.

// 0x089CC710: new 0x18, construct 0x089CC678, sink 0x08A5BAB8.

// 0x089B67AC: new 0x180, construct 0x089B6698, sink 0x089A274C.

// 0x08A3F7F8: new 0x98, construct 0x08A3EA24, sink 0x08A76AB4.

// 0x08A4DE78 (a0 = this, a1, a2): base call 0x08B36290(this); this->+0x24 = a2 byte, vtable 0x08CD1EF8, this->+0x20 = a1; v0 = vtable.

// 0x08935044 (a0 = this): base call 0x08934F78(this->+0, this->+4); copies four words of the global 0x08CF2968's record
// (+0xC0..+0xCC) into this+0x58..+0x64; tail call 0x0893388C(word +0xC8, word +0xCC) with v0/v1 = the first two words.

// 0x08A1A8C0 (a0 = this): base call 0x08AD023C(this); vtable 0x08CD0698; this->+0x64 = 0; call 0x08B28DCC(this+0x68);
// this->+0x90 = 0; tail call 0x08ABFF0C(this+0x30, 1) after 0x08ABFF0C(this+0x44, 1).

// 0x0899A984 (a0 = this): base call 0x089946BC(this); vtable 0x08CC9138, this+0x38 = 0x08CC9390; six halfwords at
// +0x120..+0x12A cleared. v0 = 0x08CC9138 and v1 = 0x08CC9390 at exit.

// 0x089FF1C0 (a0 = this): base call 0x08A0FDD8(this); vtables 0x08CC73A8 at +0 and 0x08CC7588 at +0x38; call 0x089B883C(this+0x78, this);
// this+0x94 = 1, this+0x98 = 0 byte; v0 = 1.

// 0x08A32B74 (a0 = this): base call 0x08A76ACC(this); vtable 0x08CB8AC8; member = this+0x14 gets 0x08A431F8(member);
// 0x08A32A14(this+0x18, member); this+0x42C = 0.

// 0x08B1BCFC (a0 = this): calls 0x08B344E0(this), 0x08B1B1E4(this+0x98), 0x08B1BC20(this+0x78), 0x08B1BB44(this+0x28);
// this+0x90 (float) += 1.0; tail call 0x08B3457C(this).

// 0x08A03D58 (a0 = this): base call 0x08A76ACC(this); vtable 0x08CBBE10; the sub-object at +0x20 gets 1.0f at +0x4, +0x8, +0xC
// and +0x20 = 1.0f; +0x30 = -1. v0 = -1.

// 0x0886A7B0 (a0 = this): sub = this+0x4238; call 0x0886BFC0(sub); vtable 0x08CBF6E8 at this+0x4240; call 0x0886D88C(sub);
// call 0x08B28DC4(this+0x4210); tail call 0x08875138(this+8).

// 0x08C247E4 (a0 = out, a1 = &object): object = *a1; if it is null call 0x08A5BB7C(0x08CB06B8); then call the object's
// vtable slot +0x44 with the object; out+0x14 = the answer byte. v0 = the answer.

// 0x08AB59A8 (a0 = self, a1): new 0x20, construct 0x08AB54CC(object, a1), sink 0x08A7026C(self, &object).
void func_08AB59A8(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_sink(rt, ctx, 0x20u, 0x08AB54CCu, ctx.gpr[5], 0x08A7026Cu);
}

// 0x08AB62EC (a0 = self, a1): new 0x28, construct 0x08AB5EC8(object, a1), sink 0x08A70EC8(self, &object).
void func_08AB62EC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_sink(rt, ctx, 0x28u, 0x08AB5EC8u, ctx.gpr[5], 0x08A70EC8u);
}

// 0x089333A0 (a0 = this): when this+0xB0 == -1 and this+0xB4 == -1 the answer is -1 (a0); otherwise
// 0x089363FC(-1) gives r, 0x08936584(r, a1, this+0xB0, this+0xB4) with t0 = &local, and v0 = the local's byte at +2.
void func_089333A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 b0 = word(rt, self + 0xB0u);
    ctx.gpr[4] = 0xFFFFFFFFu;  // delay slot of the bne: a0 = -1 on every path
    if (b0 == 0xFFFFFFFFu) {
        const u32 b4 = word(rt, self + 0xB4u);
        if (b4 == b0) {
            ctx.gpr[2] = ctx.gpr[4];
            return;
        }
    }
    const u32 r = call(rt, ctx, 0x089363FCu, {0xFFFFFFFFu});
    const u32 b4 = word(rt, self + 0xB4u);
    ctx.gpr[8] = frame.at(0u);  // t0: the output slot
    call(rt, ctx, 0x08936584u, {r, a1, b0, b4});
    ctx.gpr[2] = rt.memory().load8(frame.at(2u));
}

// 0x088DCEF4 (a0 = this, a1 = index): s0 = word(this+0xC0) + 760*a1; s1 = word(this+0x618); r = 0x088FE518(word(this+0x14), a1);
// tail call 0x088E9054(s0, s1, r, 0) with f12 = 0.0f.

// 0x08C7A688 (a0 = this, a1 = source): this->vtable = word(source); this->[word(vtable-0xC)] = word(source+0x20);
// this+8 = word(source+0x24); call 0x08C786B4(this+0xC, word(source+0x20), a2) and 0x08C553B0(this, source+4).

// 0x08A7C8A4 (a0 = out, a1, a2): local = 0x08A69654(sp, a1) builds the first object, obj1 = sp+4 from 0x08A74F00(obj1, sp, a2);
// then 0x08A7C5C8(out, obj1), 0x08A5B9EC(obj1), 0x08A69D68(sp).

// 0x08A7C910: as 0x08A7C8A4 with the build 0x08A74A4C.

// 0x08A7C97C: as 0x08A7C8A4 with the init 0x08A69B3C.

// 0x08A7C9E8: as 0x08A7C8A4 with the init 0x08A69B3C and the build 0x08A74A4C.

// 0x08B1CD20 (a0 = this, a1..a3, t0..t3): stores a2, a3, t0..t3 at sp+0x28..+0x3C; sp+0 = sp+0x28; call 0x08B375C8(
// word(this)+word(this+8), word(this+4)-word(this+8), a1, sp+0x28); then 0x08B1CB80(this). v0 = the last result.
void func_08B1CD20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    const u32 t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x40u);
    const u32 v1 = word(rt, self + 8u);
    const u32 v0 = word(rt, self + 4u);
    const u32 base = word(rt, self);
    put(rt, frame.at(0x28u), a2);
    put(rt, frame.at(0x2Cu), a3);
    put(rt, frame.at(0x30u), t0);
    put(rt, frame.at(0x34u), t1);
    put(rt, frame.at(0x38u), t2);
    put(rt, frame.at(0x3Cu), t3);
    put(rt, frame.at(0u), frame.at(0x28u));
    call(rt, ctx, 0x08B375C8u, {base + v1, v0 - v1, a1, frame.at(0x28u)});
    call(rt, ctx, 0x08B1CB80u, {self});
}

// 0x08A2DE64 (a0 = this): call 0x08A76ACC(this); vtable 0x08CBE590; the singleton at 0x08CF6648 is created once
// (0x08B62344(8) then 0x08B0ACDC(instance), flag byte 0x08CF664C = 1); v0 = the singleton's word, or 0x00020000 when created.
void func_08A2DE64(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBE590u);
    const u32 existing = word(rt, 0x08CF6648u);
    if (existing == 0u) {
        const u32 instance = call(rt, ctx, 0x08B62344u, {8u});
        ctx.gpr[16] = instance;
        call(rt, ctx, 0x08B0ACDCu, {instance});
        put(rt, 0x08CF6648u, instance);
        rt.memory().store8(0x08CF664Cu, 1u);
        ctx.gpr[2] = 0x00020000u;
    } else {
        ctx.gpr[2] = existing;
    }
}

// 0x08C7A730 (a0 = this, a1): vtable 0x08CDC454, +8 = 0x08CDC468, +0xB0 = 0x08CDC47C; call 0x08C786B4(this+0xC, a1);
// call 0x08C553B0(this, 0x08CDC424); +0xB0 = 0x08CDB088; tail call 0x08B6543C(this+0xB0), with v0 = 0x08CDB088.

// 0x08A989B4 (a0 = this): vtable 0x08CB7108; call 0x08B856B0(word(this+0x14), word(this+0x18), sp); begin = word(this+0x14),
// bytes = ((word(this+0x18) - begin) >> 2) << 2 (signed shift); if begin != 0 call 0x08A5BC54(begin, bytes, 4);
// then call 0x08A780B0(this).

}  // namespace new_batch_main_2080
}  // namespace gt::game

#define NEW2080(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2080::name, gt::ReturnKind::kind)
NEW2080(0x08AB59A8u, func_08AB59A8, Word);
NEW2080(0x08AB62ECu, func_08AB62EC, Word);
NEW2080(0x089333A0u, func_089333A0, Word);
NEW2080(0x08B1CD20u, func_08B1CD20, Word);
NEW2080(0x08A2DE64u, func_08A2DE64, Word);
