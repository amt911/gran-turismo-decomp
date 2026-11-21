// New batch 3 (functions the extra seeds made emittable). Phase A functions, structured C from tools.disasm.
// Comparable by synthetic cases (likely): the lookup getters (a0 + a name constant, one load) and the destructor-shaped functions whose
// callees only read the object; to verify: B5A-style "call then tail call" shapes read through a0 only.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_3 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

enum class Load { Word, Half, Byte, UnsignedByte };

// The getters: v1 = lookup(a0, name); a null answer gives `fallback`, else the field at `offset` of the answer, loaded as `kind`; v0 = v1.
void lookup_getter(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 name, Load kind, u32 offset, u32 fallback) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, lookup, {ctx.gpr[4], name});
    u32 value = fallback;
    if (found != 0u) {
        const u32 at = found + offset;
        switch (kind) {
        case Load::Word: value = word(rt, at); break;
        case Load::Half: value = static_cast<u32>(signed_half(rt, at)); break;
        case Load::Byte: value = static_cast<u32>(signed_byte(rt, at)); break;
        case Load::UnsignedByte: value = rt.memory().load8(at); break;
        }
    }
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// A destructor-shaped function: the vtable word is stored at *this before the first call (it sits in the call's delay slot), s0 holds
// this for the call, the frame is popped and the work goes on in a tail call; v0 is the first call's result.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved() { ctx.gpr[16] = s0; }
};
void destroy_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 first_call, u32 tail, u32 tail_argument_offset, bool through_pointer) {
    const u32 self = ctx.gpr[4];
    u32 result;
    u32 argument;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, vtable);
        result = call(rt, ctx, first_call, {self});
        argument = through_pointer ? word(rt, self + 4u) : self + tail_argument_offset;
        ctx.gpr[2] = result;
    }
    tail_call(ctx, tail, {argument});
}
// The other shape: the vtable is stored first, the first call gets this + offset (the add sits in the call's delay slot), then a tail call with this.
void store_then_destroy_member(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 offset, u32 first_call, u32 tail) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, vtable);
        result = call(rt, ctx, first_call, {self + offset});
    }
    ctx.gpr[2] = result;
    tail_call(ctx, tail, {self});
}

void func_08ADA748(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08AD9E74u, 0x08C90C68u, Load::Half, 0x10u, 0u); }
void func_08ADAF7C(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08AD9E74u, 0x08C90D10u, Load::Byte, 0x15u, 0u); }
void func_08ADBB1C(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADB278u, 0x08C90DF4u, Load::Word, 0x14u, 0u); }
void func_08ADBD28(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADB278u, 0x08C90E4Cu, Load::Word, 0x1A4u, 0u); }
void func_08ADC2E8(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADB278u, 0x08C90EACu, Load::Word, 0x198u, 0u); }
void func_08ADC318(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADB278u, 0x08C90EB8u, Load::Byte, 0x10u, 0u); }
void func_08ADC9D0(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADB278u, 0x08C90F34u, Load::Word, 0x30u, 0u); }
void func_08AE0B7C(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C91214u, Load::Word, 0x94u, 0u); }
void func_08AE0CA8(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C9123Cu, Load::Word, 0xD0u, 0u); }
void func_08AE0D98(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C91268u, Load::UnsignedByte, 0xE4u, 0xFFFFFFFFu); }
void func_08AE0DC8(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C91280u, Load::UnsignedByte, 0xE5u, 0xFFFFFFFFu); }
void func_08AE0E50(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C912B0u, Load::Byte, 0xE6u, 0xFFFFFFFFu); }
void func_08AE0E80(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C912CCu, Load::Byte, 0xE7u, 0xFFFFFFFFu); }
void func_08AE1CF0(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08ADDCD4u, 0x08C913BCu, Load::Half, 0xEAu, 0xFFFFFFFFu); }
void func_08AF09B0(Runtime &rt, AllegrexContext &ctx) { lookup_getter(rt, ctx, 0x08AEFD90u, 0x08C926B8u, Load::Word, 0x24u, 0xFFFFFFFFu); }
void func_08A12834(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD03E0u, 0x08A1256Cu, 0x089E4C98u, 0u, false); }
void func_08A20FA8(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD0918u, 0x08A20858u, 0x08A780B0u, 0u, false); }
void func_08A450DC(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CBE340u, 0x08A4455Cu, 0x08A780B0u, 0u, false); }
void func_08AD1E5C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD6C70u, 0x08AD1DB0u, 0x08AB8888u, 0u, false); }
void func_08AD590C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD6EA0u, 0x08AD5834u, 0x08AD7DF8u, 0u, false); }
void func_08AFB1D0(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD78A0u, 0x08AFAC7Cu, 0x08AFB12Cu, 0u, false); }
void func_08AFB274(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CD7848u, 0x08AFAC7Cu, 0x08AFB12Cu, 0u, false); }
void func_08931C6C(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CC67A8u, 0x08931B20u, 0x08937B04u, 0u, true); }
void func_08B2C388(Runtime &rt, AllegrexContext &ctx) { destroy_then_tail(rt, ctx, 0x08CDA710u, 0x08B2C2E0u, 0x08B2C2D8u, 0x58u, false); }
void func_089F0B80(Runtime &rt, AllegrexContext &ctx) { store_then_destroy_member(rt, ctx, 0x08CCFA50u, 0xCu, 0x08B0B0FCu, 0x08A5BB5Cu); }
void func_08A190C4(Runtime &rt, AllegrexContext &ctx) { store_then_destroy_member(rt, ctx, 0x08CD0618u, 0x14u, 0x08A5B9ECu, 0x08A5D638u); }
void func_08A1A808(Runtime &rt, AllegrexContext &ctx) { store_then_destroy_member(rt, ctx, 0x08CD0678u, 0xCu, 0x08A5B9ECu, 0x08A5BB5Cu); }

// 0x08AF1280 (a0 = object): v0 = the word at +0x2B8 of the object the word at +4 points to.
void func_08AF1280(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x2B8u);
}

// 0x08B79788: v0 = the address 0x08C96E60 (a table).
void func_08B79788(Runtime &, AllegrexContext &ctx) { ctx.gpr[2] = 0x08C96E60u; }

// 0x088E862C (a0 = object, a1 = a byte): when the object at +0x2E4 exists and the word at +0x1C is 0, stores the byte at +0x36 of it.
void func_088E862C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 inner = word(rt, object + 0x2E4u);
    ctx.gpr[3] = inner;
    ctx.gpr[5] &= 0xFFu;
    if (inner == 0u) return;
    ctx.gpr[2] = word(rt, object + 0x1Cu);
    if (ctx.gpr[2] == 0u) rt.memory().store8(inner + 0x36u, static_cast<std::uint8_t>(ctx.gpr[5]));
}

// 0x088E7F48 (a0 = object, a1 = a byte): with the word at +0x1C zero, stores the byte at +0x22 and, when the object at +0x2E4 exists, at its +1.
void func_088E7F48(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    ctx.gpr[2] = word(rt, object + 0x1Cu);
    ctx.gpr[5] &= 0xFFu;
    if (ctx.gpr[2] != 0u) return;
    ctx.gpr[2] = word(rt, object + 0x2E4u);
    rt.memory().store8(object + 0x22u, static_cast<std::uint8_t>(ctx.gpr[5]));
    if (ctx.gpr[2] != 0u) rt.memory().store8(ctx.gpr[2] + 1u, static_cast<std::uint8_t>(ctx.gpr[5]));
}

// 0x08B1A824: calls 0x08C19BEC with a0 = 0x08D68A0C.
void func_08B1A824(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C19BECu, {0x08D68A0Cu});
}

// 0x08ADC9AC: 0x08ADC8DC(a0, a1, 0x40, 0x08C90F28); v0 = its result.
void func_08ADC9AC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[7] = 0x08C90F28u;
    ctx.gpr[6] = 0x40u;
    call(rt, ctx, 0x08ADC8DCu, {ctx.gpr[4], ctx.gpr[5], 0x40u, 0x08C90F28u});
}
// 0x08B18C78 (a0 = object, a1 = another object): null a1 gives 0, else a tail call of the virtual at +0x18 of a1's vtable with a0 = a1, a1 = word +4 of the object.
void func_08B18C78(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    if (other == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 target = word(rt, word(rt, other) + 0x18u);
    ctx.gpr[3] = word(rt, other);
    ctx.gpr[2] = target;
    ctx.gpr[25] = target;
    tail_call(ctx, target, {other, word(rt, object + 4u)});
}
// 0x08B18AE4 (a0 = text): the position after the first ':' (0x08B7C9E4(text, 0x3A)), else the text itself.
void func_08B18AE4(Runtime &rt, AllegrexContext &ctx) {
    const u32 text = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = text;
    ctx.gpr[5] = 0x3Au;
    const u32 found = call(rt, ctx, 0x08B7C9E4u, {text, 0x3Au});
    ctx.gpr[2] = found != 0u ? found + 1u : text;
}
// 0x08ADBBA0: v1 = 0x08ADB278(a0, 0x08C90E08); v0 = the word at +0x18 of it, or the table 0x08C90E14 when null.
void func_08ADBBA0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90E08u});
    ctx.gpr[3] = found;
    ctx.gpr[2] = found != 0u ? word(rt, found + 0x18u) : 0x08C90E14u;
}
// 0x08AE186C: lookup 0x08ADDCD4(a0, 0x08C912FC); 0 when null, else the halfword at +0x2A shifted right (arithmetic) by 5.
void func_08AE186C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C912FCu});
    ctx.gpr[3] = found != 0u ? static_cast<u32>(signed_half(rt, found + 0x2Au) >> 5) : 0u;
    ctx.gpr[2] = ctx.gpr[3];
}
// 0x08AE07E0: lookup 0x08ADDCD4(a0, 0x08C9118C); when found, clears bit 0 of the byte at +0x11 (v0 = the new byte); null leaves v0 = 0.
void func_08AE07E0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C9118Cu});
    if (found == 0u) return;
    const u32 cleared = rt.memory().load8(found + 0x11u) & ~1u;
    rt.memory().store8(found + 0x11u, static_cast<std::uint8_t>(cleared));
    ctx.gpr[2] = cleared;
}
} // namespace
} // namespace new_batch_3
} // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_3::name, gt::ReturnKind::kind)
NEW3(0x088E7F48u, func_088E7F48, Word);
NEW3(0x088E862Cu, func_088E862C, Word);
NEW3(0x08931C6Cu, func_08931C6C, Word);
NEW3(0x089F0B80u, func_089F0B80, Word);
NEW3(0x08A12834u, func_08A12834, Word);
NEW3(0x08A190C4u, func_08A190C4, Word);
NEW3(0x08A1A808u, func_08A1A808, Word);
NEW3(0x08A20FA8u, func_08A20FA8, Word);
NEW3(0x08A450DCu, func_08A450DC, Word);
NEW3(0x08AD1E5Cu, func_08AD1E5C, Word);
NEW3(0x08AD590Cu, func_08AD590C, Word);
NEW3(0x08ADA748u, func_08ADA748, Word);
NEW3(0x08ADAF7Cu, func_08ADAF7C, Word);
NEW3(0x08ADBB1Cu, func_08ADBB1C, Word);
NEW3(0x08ADBBA0u, func_08ADBBA0, Word);
NEW3(0x08ADBD28u, func_08ADBD28, Word);
NEW3(0x08ADC2E8u, func_08ADC2E8, Word);
NEW3(0x08ADC318u, func_08ADC318, Word);
NEW3(0x08ADC9ACu, func_08ADC9AC, Word);
NEW3(0x08ADC9D0u, func_08ADC9D0, Word);
NEW3(0x08AE07E0u, func_08AE07E0, Word);
NEW3(0x08AE0B7Cu, func_08AE0B7C, Word);
NEW3(0x08AE0CA8u, func_08AE0CA8, Word);
NEW3(0x08AE0D98u, func_08AE0D98, Word);
NEW3(0x08AE0DC8u, func_08AE0DC8, Word);
NEW3(0x08AE0E50u, func_08AE0E50, Word);
NEW3(0x08AE0E80u, func_08AE0E80, Word);
NEW3(0x08AE186Cu, func_08AE186C, Word);
NEW3(0x08AE1CF0u, func_08AE1CF0, Word);
NEW3(0x08AF09B0u, func_08AF09B0, Word);
NEW3(0x08AF1280u, func_08AF1280, Word);
NEW3(0x08AFB1D0u, func_08AFB1D0, Word);
NEW3(0x08AFB274u, func_08AFB274, Word);
NEW3(0x08B18AE4u, func_08B18AE4, Word);
NEW3(0x08B18C78u, func_08B18C78, Word);
NEW3(0x08B1A824u, func_08B1A824, Word);
NEW3(0x08B2C388u, func_08B2C388, Word);
NEW3(0x08B79788u, func_08B79788, Word);
