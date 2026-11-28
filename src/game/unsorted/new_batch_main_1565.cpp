// Main-session batch 1565: small functions (16-18 instructions) the recompiler emitted, written as structured C from
// `tools/disasm`. Shapes: virtual-call forwarders, destructor-shaped vtable stores with member calls and a tail call,
// six copy-assign variants (word / half / float / byte at +0x14), a byte-offset bit test and a VFPU 0x40-byte record
// copy. No names: none has evidence. DRAFT ONLY: the differential gate has not run yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1565 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; v0 afterwards holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// ---- 0x08AE0C68: r = 0x08ADDCD4(a0, 0x08C91228); a null r gives v0 = 0, otherwise v0 = r->+0xD0 and r->+0xD0 = a1. ----
void func_08AE0C68(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C91228u});
    u32 result = 0u;
    if (found != 0u) {
        result = word(rt, found + 0xD0u);
        put(rt, found + 0xD0u, value);
    }
    ctx.gpr[2] = result;
}

// ---- 0x08AF2978: 0x08AF296C(*(a0)), then tail-call 0x08AF10C4(that, a1) with the original $f12. ----
void func_08AF2978(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 second = ctx.gpr[5];
    const float f = ctx.fpr[12];
    u32 value;
    {
        GuestFrame frame(ctx, 0x20u);
        value = call(rt, ctx, 0x08AF296Cu, {object});
    }
    ctx.fpr[12] = f;
    tail_call(ctx, 0x08AF10C4u, {value, second});
}

// ---- 0x08B0DCE0: vtable 0x08CD7C08; member 0x08B0C24C(this + 0xF8); 0x08B0DDDC(this); tail 0x08B64654(this). ----
void func_08B0DCE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD7C08u);
        call(rt, ctx, 0x08B0C24Cu, {self + 0xF8u});
        call(rt, ctx, 0x08B0DDDCu, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// ---- 0x08B18B18: 0x08B18A98(this); 0x08B18A58(this, a1); v0 = this. ----
void func_08B18B18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B18A98u, {self});
    call(rt, ctx, 0x08B18A58u, {self, arg});
    ctx.gpr[2] = self;
}

// ---- 0x08B1A02C: 0x08B34888(this); v0 = the unsigned byte at +0x2C read before the second call; 0x08B34B24(this). ----

// ---- 0x08B1B92C: obj = 0x08B2E7DC(a0, a1, a2, a3); obj->+0xC = a3 & 0xFF; 0x08B2E214(obj); v0 = obj. ----
void func_08B1B92C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 byte = ctx.gpr[7] & 0xFFu;
    const u32 object = call(rt, ctx, 0x08B2E7DCu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    rt.memory().store8(object + 0xCu, static_cast<std::uint8_t>(byte));
    call(rt, ctx, 0x08B2E214u, {object});
    ctx.gpr[2] = object;
}

// ---- 0x08B1F8E8: vtable 0x08CD9488; member 0x08B28DC4(this + 0xC); 0x08B1FD74(this); tail 0x08B64654(this). ----
void func_08B1F8E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD9488u);
        call(rt, ctx, 0x08B28DC4u, {self + 0xCu});
        call(rt, ctx, 0x08B1FD74u, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// ---- 0x08B248BC: vtable 0x08CD9610; member 0x08B28DC4(this + 0x1C); 0x08B24A0C(this); tail 0x08B64654(this). ----
void func_08B248BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD9610u);
        call(rt, ctx, 0x08B28DC4u, {self + 0x1Cu});
        call(rt, ctx, 0x08B24A0Cu, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// ---- 0x08B56960: copies the 0x40-byte record a0->+0xD10 selects at a0 + 0xD00 to its +0x40 and advances the
// pointer; v0 = the new pointer. The four quad columns stay in C000-C030, as the original leaves them. ----

// ---- 0x08C0C384: 0x089FA280(&local, a1); 0x089FA8B4(this, &local); 0x089F9B74(&local); v0 = this. ----
void func_08C0C384(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x089FA280u, {frame.at(0u), ctx.gpr[5]});
    call(rt, ctx, 0x089FA8B4u, {self, frame.at(0u)});
    call(rt, ctx, 0x089F9B74u, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// ---- 0x0881A9CC: bit `t1` of ((lbu (0x0881A840(a0-a3, t0, t1) + t0) + 2) >> 4); t0 is an offset, t1 a shift count. ----
void func_0881A9CC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 shift = ctx.gpr[9];
    const u32 offset = ctx.gpr[8];
    const u32 base = call(rt, ctx, 0x0881A840u,
                          {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], ctx.gpr[9]});
    const u32 byte = rt.memory().load8(base + offset + 2u);
    const u32 shifted = byte >> 4u;
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(shifted) >> (shift & 31u)) & 1u;
}

// ---- 0x08862C4C: when this->+4 exists, call its vtable word 0 with (object, *this) and clear both words; v0 is the
// callee's (untouched when there is no +4). ----
void func_08862C4C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 4u);
    if (inner == 0u) return;
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    call_indirect(rt, ctx, target, {inner, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// ---- 0x08894328: byte-identical body to 0x08862C4C. ----
void func_08894328(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 4u);
    if (inner == 0u) return;
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    call_indirect(rt, ctx, target, {inner, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// ---- 0x088978F8: byte-identical body to 0x08862C4C. ----
void func_088978F8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 4u);
    if (inner == 0u) return;
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    call_indirect(rt, ctx, target, {inner, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// ---- 0x088A5AA0: the float at the frame (filled by 0x088A5A48(this, &frame, &frame + 4)) divides
// (float)(int)0x088A5A90(this); v0 keeps that int, f0 the quotient. ----
void func_088A5AA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    float divisor;
    s32 value;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x088A5A48u, {self, frame.at(0u), frame.at(4u)});
        value = static_cast<s32>(call(rt, ctx, 0x088A5A90u, {self}));
        divisor = float_at(rt, frame.at(0u));
    }
    ctx.gpr[2] = static_cast<u32>(value);
    const float converted = static_cast<float>(value);
    ctx.fpr[1] = converted;
    ctx.fpr[0] = converted / divisor;
}

// ---- 0x088F4B9C: byte-identical body to 0x08862C4C. ----
void func_088F4B9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 4u);
    if (inner == 0u) return;
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    call_indirect(rt, ctx, target, {inner, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// ---- 0x0890B158: byte-identical body to 0x08862C4C. ----

// ---- 0x089385B0: vtable 0x08CC67E8; when this->+4 exists, 0x08938E48(it); tail 0x0893A068(this + 0x28). ----
void func_089385B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 4u);
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC67E8u);
        if (inner != 0u) call(rt, ctx, 0x08938E48u, {inner});
    }
    tail_call(ctx, 0x0893A068u, {self + 0x28u});
}

// ---- 0x08938940: 0x08938428(a0, a1, &frame, a3); 0x08938808(this, &frame, a2). ----
void func_08938940(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x1F0u);
    const u32 self = ctx.gpr[4];
    const u32 last = ctx.gpr[6];
    call(rt, ctx, 0x08938428u, {self, ctx.gpr[5], frame.at(0u), ctx.gpr[7]});
    call(rt, ctx, 0x08938808u, {self, frame.at(0u), last});
}

// ---- 0x0895EA50: v0 = 0x0895EA38(this) != 0 || 0x0895EA44(this) != 0 (the second call only when the first is 0). ----
void func_0895EA50(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 1u;
    if (call(rt, ctx, 0x0895EA38u, {self}) == 0u && call(rt, ctx, 0x0895EA44u, {self}) == 0u) result = 0u;
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// ---- 0x089B4D94: vtable 0x08CCB198, +0x38 = 0x08CCB3D4; 0x08A5B9EC(this + 0x22C); tail 0x089E2430(this). ----
void func_089B4D94(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CCB198u);
        put(rt, self + 0x38u, 0x08CCB3D4u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x22Cu});
    }
    tail_call(ctx, 0x089E2430u, {self});
}

// ---- 0x089F9BB8: 0x08BE5888(this->+0, this->+4, &frame); v0 = this->+0, released with 0x08B35254 when non-null. ----
void func_089F9BB8(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08BE5888u, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
    const u32 object = word(rt, self);
    ctx.gpr[2] = object;
    if (object != 0u) call(rt, ctx, 0x08B35254u, {object});
}

// ---- 0x08A19020: 0x089BB3A8(&local, a2, a2); assign local into this + 0x14 when it is not the slot itself;
// 0x08A5B9EC(&local). v0 is the last call's. ----
void func_08A19020(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[6];
    call(rt, ctx, 0x089BB3A8u, {frame.at(0u), arg, arg});
    if (self + 0x14u != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self + 0x14u, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x08A5F4C0: copy-assign: when this != a1, 0x08A781D4(this, a1) and this->+0x14 = a1->+0x14 (word); v0 = this. ----

// ---- 0x08A72E00: as 0x08A5F4C0 with the unsigned half at +0x14. ----

// ---- 0x08A85594: as 0x08A5F4C0 with the word at +0x14 (byte-identical body to 0x08A5F4C0). ----
void func_08A85594(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    if (self != other) {
        call(rt, ctx, 0x08A781D4u, {self, other});
        put(rt, self + 0x14u, word(rt, other + 0x14u));
    }
    ctx.gpr[2] = self;
}

// ---- 0x08A877D0: as 0x08A5F4C0 with the float at +0x14 (lwc1 / swc1). ----

// ---- 0x08A9563C: vtable 0x08CBE0E0; when this->+0x14 exists, 0x08B35254(it); tail 0x08A780B0(this). ----
void func_08A9563C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 inner = word(rt, self + 0x14u);
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CBE0E0u);
        if (inner != 0u) call(rt, ctx, 0x08B35254u, {inner});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// ---- 0x08A95B74: as 0x08A5F4C0 with the unsigned byte at +0x14. ----
void func_08A95B74(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    if (self != other) {
        call(rt, ctx, 0x08A781D4u, {self, other});
        rt.memory().store8(self + 0x14u, rt.memory().load8(other + 0x14u));
    }
    ctx.gpr[2] = self;
}

// ---- 0x08A97484: byte-identical body to 0x08A95B74. ----

} // namespace
} // namespace new_batch_main_1565
} // namespace gt::game

#define NEWMAIN1565(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1565::name, gt::ReturnKind::kind)
NEWMAIN1565(0x08AE0C68u, func_08AE0C68, Word);
NEWMAIN1565(0x08AF2978u, func_08AF2978, Word);
NEWMAIN1565(0x08B0DCE0u, func_08B0DCE0, Word);
NEWMAIN1565(0x08B18B18u, func_08B18B18, Word);
NEWMAIN1565(0x08B1B92Cu, func_08B1B92C, Word);
NEWMAIN1565(0x08B1F8E8u, func_08B1F8E8, Word);
NEWMAIN1565(0x08B248BCu, func_08B248BC, Word);
NEWMAIN1565(0x08C0C384u, func_08C0C384, Word);
NEWMAIN1565(0x0881A9CCu, func_0881A9CC, Word);
NEWMAIN1565(0x08862C4Cu, func_08862C4C, Word);
NEWMAIN1565(0x08894328u, func_08894328, Word);
NEWMAIN1565(0x088978F8u, func_088978F8, Word);
NEWMAIN1565(0x088A5AA0u, func_088A5AA0, Float);
NEWMAIN1565(0x088F4B9Cu, func_088F4B9C, Word);
NEWMAIN1565(0x089385B0u, func_089385B0, Word);
NEWMAIN1565(0x08938940u, func_08938940, Word);
NEWMAIN1565(0x0895EA50u, func_0895EA50, Word);
NEWMAIN1565(0x089B4D94u, func_089B4D94, Word);
NEWMAIN1565(0x089F9BB8u, func_089F9BB8, Word);
NEWMAIN1565(0x08A19020u, func_08A19020, Word);
NEWMAIN1565(0x08A85594u, func_08A85594, Word);
NEWMAIN1565(0x08A9563Cu, func_08A9563C, Word);
NEWMAIN1565(0x08A95B74u, func_08A95B74, Word);
