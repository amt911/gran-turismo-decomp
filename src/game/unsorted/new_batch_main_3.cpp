// Main-session batch 3: small functions (16-18 instructions) the recompiler emitted, written as structured C from
// `tools.disasm`. Shapes: virtual-call forwarders, destructor-shaped vtable stores with member calls and a tail call,
// global-state resets, small linked-list/array loops, and two VFPU helpers (the only ones that return in $f0).
// No names: none has evidence. DRAFT ONLY: the differential gate has not run yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_3 {
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

// ---- 0x08ABBAE0: object -> inner -> vtable[0x18](inner); a null object or a null inner gives 0. ----
void func_08ABBAE0(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = ctx.gpr[4];
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 inner = word(rt, object);
    ctx.gpr[4] = inner;
    if (inner == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable + 0x18u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = call_indirect(rt, ctx, target, {inner});
}

// ---- 0x08ABC9D0: object -> inner -> vtable[0x30](inner, object, a1, a2); t0 = a3 & 0xFF goes along. ----
void func_08ABC9D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    ctx.gpr[8] = ctx.gpr[7] & 0xFFu;
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 inner = word(rt, object);
    ctx.gpr[5] = object;
    ctx.gpr[6] = arg1;
    ctx.gpr[4] = inner;
    ctx.gpr[7] = arg2;
    if (inner == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 vtable = word(rt, inner);
    const u32 target = word(rt, vtable + 0x30u);
    tail_call(ctx, target, {inner, object, arg1, arg2});
}

// ---- 0x08ACE9DC: entry a0 + a1 * 236; when its byte +0x41D is clear, set it, store a2 at +0x3E0 and tail-call
// 0x08ACE96C(a0, at + 0x334). An entry that is already set returns v0 = a1 (the last value v0 got). ----
void func_08ACE9DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 at = base + index * 236u;
    ctx.gpr[5] = at + 0x334u;
    if (rt.memory().load8(at + 0x41Du) != 0u) {
        ctx.gpr[2] = index;
        return;
    }
    ctx.gpr[2] = 1u;
    rt.memory().store8(at + 0x41Du, 1u);
    put(rt, at + 0x3E0u, value);
    // a3 = at (set before the flag test) goes along with a0/a1/a2, as the original leaves it.
    tail_call(ctx, 0x08ACE96Cu, {base, at + 0x334u, value, at});
}

// ---- 0x08AF2978: 0x08AF296C(*(a0)) + 0x40, then tail-call 0x08AF10C4(that, a1) with the original $f12. ----
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

// ---- 0x08B1A02C: 0x08B34888(this); 0x08B34B24(this); v0 = the byte at +0x2C read between the calls. ----
void func_08B1A02C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B34888u, {self});
    const u32 value = rt.memory().load8(self + 0x2Cu);
    call(rt, ctx, 0x08B34B24u, {self});
    ctx.gpr[2] = value;
}

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

// ---- 0x08B2E064: r = 0x08C7A9E8(); r == 0x80020064 tails to 0x08B32A34(0x7D0), otherwise 0x08C7A988(). ----

// ---- 0x08B2E9F8: releases the global at 0x08D690F8 (flag 0x08D690F4 cleared first) and clears the pointer. ----

// ---- 0x08B2F0D4: when the byte at 0x08D6915C is set, clear it and release the word at 0x08D69158. ----

// ---- 0x08B336F0: when the word at 0x08D69208 is positive, 0x08C7B138(it) and 0x08C7A9C0(it), then clear it. ----

// ---- 0x08B376B4: variadic forwarder: stores a1-a3, t0-t3 at sp + 0x24 and the argument-list pointer at sp + 0,
// then 0x08B3A710(a0, sp + 0x24, a2, a3, t0, t1, t2, t3). ----
void func_08B376B4(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x24u), ctx.gpr[5]);
    put(rt, frame.at(0x28u), ctx.gpr[6]);
    put(rt, frame.at(0x2Cu), ctx.gpr[7]);
    put(rt, frame.at(0x30u), ctx.gpr[8]);
    put(rt, frame.at(0x34u), ctx.gpr[9]);
    put(rt, frame.at(0x38u), ctx.gpr[10]);
    put(rt, frame.at(0x3Cu), ctx.gpr[11]);
    put(rt, frame.at(0u), frame.at(0x24u));
    call(rt, ctx, 0x08B3A710u,
         {ctx.gpr[4], frame.at(0x24u), ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
}

// ---- 0x08B56960: copies the 0x40-byte record a0->+0xD10 selects at a0 + 0xD00 to its +0x40 and advances the
// pointer; v0 = the new pointer. The four quad columns stay in C000-C030, as the original leaves them. ----
void func_08B56960(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = word(rt, self + 0xD10u);
    const u32 slot = self + 0xD00u + index * 4u;
    const u32 source = word(rt, slot);
    gt::vfpu::load_column(rt, ctx, 0u, 0u, source);
    gt::vfpu::load_column(rt, ctx, 0u, 1u, source + 0x10u);
    gt::vfpu::load_column(rt, ctx, 0u, 2u, source + 0x20u);
    gt::vfpu::load_column(rt, ctx, 0u, 3u, source + 0x30u);
    const u32 next = source + 0x40u;
    gt::vfpu::store_quad(rt, ctx, 0u, source + 0x40u);
    gt::vfpu::store_quad(rt, ctx, 1u, source + 0x50u);
    gt::vfpu::store_quad(rt, ctx, 2u, source + 0x60u);
    gt::vfpu::store_quad(rt, ctx, 3u, source + 0x70u);
    put(rt, slot, next);
    ctx.gpr[2] = next;
}

// ---- 0x08BF97DC: vtable 0x08CCED68; 0x08A5B9EC(this + 0x1C); 0x089FD82C(this); tail 0x08B64654(this). ----

// ---- 0x08C0C384: 0x089FA280(&local); 0x089FA8B4(this, &local); 0x089F9B74(&local); v0 = this. ----
void func_08C0C384(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x089FA280u, {frame.at(0u), ctx.gpr[5]});
    call(rt, ctx, 0x089FA8B4u, {self, frame.at(0u)});
    call(rt, ctx, 0x089F9B74u, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// ---- 0x08C3D818: vtable 0x08CD6A18; 0x08B28BE0(this + 0xA8); 0x08AC0194(this); tail 0x08B64654(this). ----

// ---- 0x0881A9CC: bit `t1` of (((byte at 0x0881A840(a0-a3, t0, t1) + t0) + 2) >> 4). ----
void func_0881A9CC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 shift = ctx.gpr[9];
    const u32 offset = ctx.gpr[8];
    const u32 base = call(rt, ctx, 0x0881A840u,
                          {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], ctx.gpr[9]});
    const u32 byte = rt.memory().load8(base + offset + 2u);
    const u32 shifted = byte >> 4u;
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(shifted) >> (shift & 31u)) & 1u;
}

// ---- 0x088A5AA0: the float at the frame (filled by 0x088A5A48) divides (float)0x088A5A90(this); v0 = the int. ----
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
    ctx.fpr[0] = static_cast<float>(value) / divisor;
}

// ---- 0x089378D4: 0x0893641C(a0, a1, a2); 0x08936944(this, that); tail 0x089377C4(this, a2). ----

// ---- 0x08938940: 0x08938428(a0, a1, &frame, a3); 0x08938808(this, &frame, a2). ----
void func_08938940(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x1F0u);
    const u32 self = ctx.gpr[4];
    const u32 last = ctx.gpr[6];
    call(rt, ctx, 0x08938428u, {self, ctx.gpr[5], frame.at(0u), ctx.gpr[7]});
    call(rt, ctx, 0x08938808u, {self, frame.at(0u), last});
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

// ---- 0x08A19020: 0x089BB3A8(&local, a2, a2); assign local into this + 0x14 when it is not the slot itself;
// 0x08A5B9EC(&local). ----
void func_08A19020(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[6];
    call(rt, ctx, 0x089BB3A8u, {frame.at(0u), arg, arg});
    if (self + 0x14u != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self + 0x14u, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x08A215F0: 0x08A2159C(this); r = 0x08ABC318(a1, 1); this->+0x14 = r; v0 = (r != 0). ----

// ---- 0x08A66C60: obj = 0x08A5BC14(0x14); 0x08A66C34(obj, this); v0 = obj. ----

// ---- 0x08AB3E04: 0x08B855E0(this + 0x24); v0 = 0x08B1EA48(this + 0x14, *a1) & 0xFF. ----
void func_08AB3E04(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    call(rt, ctx, 0x08B855E0u, {self + 0x24u});
    const u32 result = call(rt, ctx, 0x08B1EA48u, {self + 0x14u, word(rt, arg)});
    ctx.gpr[2] = result & 0xFFu;
}

// ---- 0x08AD0900: 0x08B1A8BC(0x08AD093C); 0x08B2ECAC(0x08CB4A18, 0x08AD08D8, 0); tail 0x08C7A8C0(that). ----

// ---- 0x08B17308: when this->+0x19DD is set, clear +0x19F0 and +0x19EC and the two sub-objects +0x1984 and
// +0x1900, then the flag. ----
void func_08B17308(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flag = rt.memory().load8(self + 0x19DDu);
    if (flag == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = flag;
    put(rt, self + 0x19F0u, 0u);
    put(rt, self + 0x19ECu, 0u);
    call(rt, ctx, 0x08B28C74u, {self + 0x1984u});
    call(rt, ctx, 0x08B28C44u, {self + 0x1900u});
    rt.memory().store8(self + 0x19DDu, 0u);
}

// ---- 0x08B32350: when 0x08CE0C88 is not negative 0x08C7AF28(it); 0x08C7AF40(); 0x08C7AF58();
// tail 0x08B2EE2C(0x08D690FC). ----

// ---- 0x08B32994: vtable 0x08CDA730; 0x08B3252C(this); 0x08B31918(this + 0x18); vtable 0x08CDA7A8; v0 = it. ----

// ---- 0x08B48CB8: the table word at this + 8 selects a string: 1 -> 0x08CB56A0, 2 -> 0x08CB5690,
// 0 -> 0x08CB5698, anything else -> 0x08CB5694. ----
void func_08B48CB8(Runtime &rt, AllegrexContext &ctx) {
    switch (word(rt, ctx.gpr[4] + 8u)) {
    case 1u: ctx.gpr[2] = 0x08CB56A0u; return;
    case 2u: ctx.gpr[2] = 0x08CB5690u; return;
    case 0u: ctx.gpr[2] = 0x08CB5698u; return;
    default: ctx.gpr[2] = 0x08CB5694u; return;
    }
}

// ---- 0x08B48DCC: the same table read at this + 0x14: 1 -> 0x08CB56F4, 2 -> 0x08CB56E8, 0 -> 0x08CB56F0,
// anything else -> 0x08CB5694. ----
void func_08B48DCC(Runtime &rt, AllegrexContext &ctx) {
    switch (word(rt, ctx.gpr[4] + 0x14u)) {
    case 1u: ctx.gpr[2] = 0x08CB56F4u; return;
    case 2u: ctx.gpr[2] = 0x08CB56E8u; return;
    case 0u: ctx.gpr[2] = 0x08CB56F0u; return;
    default: ctx.gpr[2] = 0x08CB5694u; return;
    }
}

// ---- 0x08B567B0: copies the 0x40-byte record a0->+0xD10 selects at a0 + 0xD00 to a1 (16 bytes per column, svl/svr);
// v0 = a0 + the index * 4. ----

// ---- 0x08B58178: VFPU split of x = $f12: p = x * 0.0111111108; v0 = trunc(p * 0.25) * 4 and $f0 = its float;
// C000 receives vrot.p of (p - v0) as [S,C]. The vfim immediates (0.25, 4) are the half encodings 0x3400, 0x4400. ----

// ---- 0x08B599E0: waits for vblank (0x08C7AC18) a0 times; v0 = the word at 0x08D6CE00 + 0x20. ----
void func_08B599E0(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const s32 count = static_cast<s32>(ctx.gpr[4]);
    for (s32 i = 0; i < count; ++i) call(rt, ctx, 0x08C7AC18u);
    ctx.gpr[2] = word(rt, 0x08D6CE00u + 0x20u);
}

// ---- 0x08BBC0A0: frees every node of the ring this->0 until it comes back to this (0x08B35254 each). ----

// ---- 0x08C21258: 0x08A8B098(&local, a1, this->+8, this->+0xC); 0x08A61584(a1, &local); 0x08A5B9EC(&local). ----
void func_08C21258(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    call(rt, ctx, 0x08A8B098u, {frame.at(0u), arg, word(rt, self + 8u), word(rt, self + 0xCu)});
    call(rt, ctx, 0x08A61584u, {arg, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x08C24F98: 0x08A69D68 on every word from a0 up to (not including) a1. ----

// ---- 0x08C2AD88: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C35154: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C3BB10: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C723C8: 0x08C5477C(this + *(vtable - 0xC), 0xA); 0x08C72194(this, that); tail 0x08C7030C(that). ----

// ---- 0x08809608: builder: 0x08808F2C(&local, a1, a2, 0, 0), then assign local into this, release it. ----
void func_08809608(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08808F2Cu, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], 0u, 0u});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x0886BD00: 0x0887D364(this->+4 + 0x88, *(a1) + 0x64); tail 0x08B1FC48(this->+4, a1). ----

// ---- 0x088A2A1C: the signed byte at this->+0xC + a1 (or a1 when that byte is -1) scales a this->+4 entry by 124;
// 0x088A2934(entry, a2). ----
void func_088A2A1C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 index = ctx.gpr[5];
    const u32 raw = static_cast<u32>(static_cast<s32>(
        static_cast<std::int8_t>(rt.memory().load8(word(rt, ctx.gpr[4] + 0xCu) + index))));
    // The asm's `nor v1, zero, a0` / `movn v0, a0, v1`: the byte unless it is -1, then the index.
    const u32 inverted = ~raw;
    const u32 value = inverted != 0u ? raw : index;
    const u32 scaled = (value << 7u) - (value << 2u);
    ctx.gpr[2] = call(rt, ctx, 0x088A2934u, {word(rt, ctx.gpr[4] + 4u) + scaled, ctx.gpr[6]});
}

// ---- 0x088A627C: when the byte at *(this) + 0x4CC is set, clear it and 0x088A5A0C(this); 0x088A61A4(this);
// set the byte on the object *this reads afterwards. ----

// ---- 0x0892A37C: *this = *(a1); 0x0892A1F4(this + 4, *(a1 + 4)); tail 0x0892A2C0(this + 0x10, *(a1 + 8)). ----

// ---- 0x08978574: number of links in the two rings at this + 0x18C and this + 0x184, added. ----

// ---- 0x089894E4: obj = this->+0x1E0 (checked, 0x08CA39B4 when null); tail 0x089CC7A8(obj, a1). ----

// ---- 0x089B1CC8: 0x089B1874(this, this + 0x30, a1, a2); tail 0x089AFA10(this, this + 0x30, this + 0x18). ----

// ---- 0x089B1FCC: 0x089B1874(this, this + 0x30, 0, a1); tail 0x089AFA10(this, this + 0x30, this + 0x18). ----

// ---- 0x089C6B90: vtable 0x08CCD190; 0x08AFB0F8(this + 0x188); 0x089AE074(this); tail 0x08A5BC54(this, 0x260, 4). ----

// ---- 0x08A13654: 0x08B34888(this + 0x10); this->+0x1F6 = a1 & 0xFF; tail 0x08B34B24(this + 0x10). ----

// ---- 0x08A13B98: 0x08B34888(this + 0x10); this->+0x784 = a1; tail 0x08B34B24(this + 0x10). ----
void func_08A13B98(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 block = self + 0x10u;
    call(rt, ctx, 0x08B34888u, {block});
    put(rt, self + 0x784u, ctx.gpr[5]);
    tail_call(ctx, 0x08B34B24u, {block});
}

// ---- 0x08A27460: value = 0x08B0DA60(this); 0x08A97A6C(&local, value); assign local into this; release it. ----

// ---- 0x08A274A8: value = 0x08B0D790(this); 0x08A85B1C(&local, value); assign local into this; release it. ----

// ---- 0x08A274F0: value = 0x08B0D510(this); 0x08A85B1C(&local, value); assign local into this; release it. ----

// ---- 0x08A4C7B8: value = 0x08B35444(this); 0x08A5FA8C(&local, value); assign local into this; release it. ----

// ---- 0x08AA634C: 0x08AA61D8(a0, &local, 4); v0 = the 4 bytes big-endian. ----
void func_08AA634C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08AA61D8u, {ctx.gpr[4], frame.at(0u), 4u});
    const u32 b0 = rt.memory().load8(frame.at(0u));
    const u32 b1 = rt.memory().load8(frame.at(1u));
    const u32 b2 = rt.memory().load8(frame.at(2u));
    const u32 b3 = rt.memory().load8(frame.at(3u));
    ctx.gpr[2] = b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
}

// ---- 0x08AA8F88: obj = this->+4 (checked, 0x08CB30B0 when null); tail 0x08C313AC(obj, a1). ----

// ---- 0x08AA9028: obj = this->+4 (checked, 0x08CB30B0 when null); tail 0x08C3139C(obj, a1). ----
void func_08AA9028(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C3139Cu, {object, arg});
}

// ---- 0x08AA9070: obj = this->+4 (checked); tail 0x08C31394(obj) with the original $f12. ----
void func_08AA9070(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const float f = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    ctx.fpr[12] = f;
    tail_call(ctx, 0x08C31394u, {object});
}

// ---- 0x08AA90B8: obj = this->+4 (checked); tail 0x08C3138C(obj, a1). ----

// ---- 0x08AA9360: obj = this->+4 (checked); tail 0x08C3132C(obj, a1 & 0xFFFF). ----
void func_08AA9360(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5] & 0xFFFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C3132Cu, {object, arg});
}

// ---- 0x08AA93A8: obj = this->+4 (checked); tail 0x08C31324(obj, a1). ----

// ---- 0x08AA9438: obj = this->+4 (checked); tail 0x08C24C48(obj, a1). ----

// ---- 0x08AA9558: obj = this->+4 (checked); tail 0x08C312FC(obj, a1). ----
void func_08AA9558(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C312FCu, {object, arg});
}

// ---- 0x08AA95A0: obj = this->+4 (checked); tail 0x08C312F4(obj, a1 & 0xFF). ----

// ---- 0x08AB70A4: obj = this->+0xC (checked, 0x08CB4250 when null); tail 0x08AAE4E4(obj, a1). ----
void func_08AB70A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4] + 0xCu);
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB4250u});
    }
    tail_call(ctx, 0x08AAE4E4u, {object, arg});
}

// ---- 0x08ACB500: 0x08ACB4F8(this); when this->+4 exists, call its vtable word 0 with (object, *this) and clear
// both words. ----

// ---- 0x08ACC0F4: as 0x08ACB500 with 0x08ACC0EC. ----

// ---- 0x08ACC5DC: 0x08B34888(this + 0x60); value = 0x08C7B180(this + 0x60); 0x08B34B24(this + 0x60);
// v0 = (value < 1). ----

// ---- 0x08ACC6BC: as 0x08ACB500 with 0x08ACC6B4. ----

// ---- 0x08ACD4A8: 0x08B344E0(a1 + 0x84); a1->+0xE2 = 1; 0x08B3457C(a1 + 0x84); tail 0x08B28C44(a1 + 0x58). ----

// ---- 0x08ACD86C: 0x08B344E0(a1 + 0x80); a1->+0xCA = 1; 0x08B3457C(a1 + 0x80); tail 0x08B28C44(a1 + 0x54). ----

// ---- 0x08AD5524: 0x08AD5394(this, pair); this->+0xC = 0; this->+4 = *pair; this->+8 = pair->+4; v0 = 1. ----
void func_08AD5524(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[5];
    call(rt, ctx, 0x08AD5394u, {self, pair});
    const u32 first = word(rt, pair);
    const u32 second = word(rt, pair + 4u);
    rt.memory().store8(self + 0xCu, 0u);
    put(rt, self + 4u, first);
    put(rt, self + 8u, second);
    ctx.gpr[2] = 1u;
}

// ---- 0x08B094AC: clears 0x08D4744C (releasing its object first) and 0x08D47448 (releasing its object). ----

// ---- 0x08B2D2CC: r = 0x08C7A828(a0, 0x03416102, &tmp = 0, 4, 0, 0); when r < 0 the word at 0x08D68CF8 = 7;
// v0 = r. ----

} // namespace
} // namespace new_batch_main_3
} // namespace gt::game

#define NEWMAIN3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_3::name, gt::ReturnKind::kind)
NEWMAIN3(0x08ABBAE0u, func_08ABBAE0, Word);
NEWMAIN3(0x08ABC9D0u, func_08ABC9D0, Word);
NEWMAIN3(0x08ACE9DCu, func_08ACE9DC, Word);
NEWMAIN3(0x08AF2978u, func_08AF2978, Word);
NEWMAIN3(0x08B0DCE0u, func_08B0DCE0, Word);
NEWMAIN3(0x08B18B18u, func_08B18B18, Word);
NEWMAIN3(0x08B1A02Cu, func_08B1A02C, Word);
NEWMAIN3(0x08B1B92Cu, func_08B1B92C, Word);
NEWMAIN3(0x08B1F8E8u, func_08B1F8E8, Word);
NEWMAIN3(0x08B248BCu, func_08B248BC, Word);
NEWMAIN3(0x08B376B4u, func_08B376B4, Word);
NEWMAIN3(0x08B56960u, func_08B56960, Word);
NEWMAIN3(0x08C0C384u, func_08C0C384, Word);
NEWMAIN3(0x0881A9CCu, func_0881A9CC, Word);
NEWMAIN3(0x088A5AA0u, func_088A5AA0, Float);
NEWMAIN3(0x08938940u, func_08938940, Word);
NEWMAIN3(0x089B4D94u, func_089B4D94, Word);
NEWMAIN3(0x08A19020u, func_08A19020, Word);
NEWMAIN3(0x08AB3E04u, func_08AB3E04, Word);
NEWMAIN3(0x08B17308u, func_08B17308, Word);
NEWMAIN3(0x08B48CB8u, func_08B48CB8, Word);
NEWMAIN3(0x08B48DCCu, func_08B48DCC, Word);
NEWMAIN3(0x08B599E0u, func_08B599E0, Word);
NEWMAIN3(0x08C21258u, func_08C21258, Word);
NEWMAIN3(0x08809608u, func_08809608, Word);
NEWMAIN3(0x088A2A1Cu, func_088A2A1C, Word);
NEWMAIN3(0x08A13B98u, func_08A13B98, Word);
NEWMAIN3(0x08AA634Cu, func_08AA634C, Word);
NEWMAIN3(0x08AA9028u, func_08AA9028, Word);
NEWMAIN3(0x08AA9070u, func_08AA9070, Word);
NEWMAIN3(0x08AA9360u, func_08AA9360, Word);
NEWMAIN3(0x08AA9558u, func_08AA9558, Word);
NEWMAIN3(0x08AB70A4u, func_08AB70A4, Word);
NEWMAIN3(0x08AD5524u, func_08AD5524, Word);
