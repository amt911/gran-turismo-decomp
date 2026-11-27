// Main-session batch 1619 (drafted, unverified). Structured C from tools.disasm; each body follows its asm order:
// delay slots run before the call they sit in, v0 is the last callee's result unless the asm sets it, and the
// s-registers are never written (the locals carry the values the asm keeps there).
// Skipped as unsure: 0x08B17104 and 0x08B7B34C (they load a saved register record, longjmp-like), 0x08ADFFF4 (alloca),
// 0x089C5000 (lui/lw address arithmetic), 0x08B245FC (64-bit madd), 0x08B6250C (record loop), 0x08AF4580 (vcalls with floats).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1883 {
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
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// 0x0884CB98 (a0 = object, a1 = left as the allocator leaves it, a2, a3, t0): a 0x28-byte record is allocated and filled by
// 0x0884C590, then 0x08A76AB4 takes the object and the stack word that holds the record.
void func_0884CB98(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x28u, 4u});
    call(rt, ctx, 0x0884C590u, {record, ctx.gpr[5], a2, a3, t0});
    put(rt, frame.at(0), record);
    call(rt, ctx, 0x08A76AB4u, {object, frame.at(0)});
}

// 0x08860484 (a0 = this, a1, a2): the engine at *this gets its busy byte (+0x3FDB) set around 0x088D7DFC.

// 0x08869900 (a0 = this): the calls that set up the sub-objects, then the vtable word at +0x1A0 and the zeroed block at +0x1A4.
// v0 = 1 on exit (the byte store comes after the last call).

// 0x088703D4 (a0 = this): when |*(body+0x84)| exceeds 0.000... (asm constant 0x3A83126F), 0x08B4F174 gets the scaled value
// and 0x08B4DBC4 the stack word; the body's +0x84 word is then zeroed.
void func_088703D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float epsilon = std::bit_cast<float>(0x3A83126Fu);
    const float scale = std::bit_cast<float>(0x3C8EFA35u);
    GuestFrame frame(ctx, 0x50u);
    const u32 body = word(rt, self + 0xCu);
    const float value = float_at(rt, body + 0x84u);
    if (epsilon < std::fabs(value)) {
        call_indirect(rt, ctx, 0x08B4F174u, {frame.at(0)}, {psp_mul(value, scale)});
        call(rt, ctx, 0x08B4DBC4u, {word(rt, self + 0xCu), frame.at(0)});
    }
    put(rt, body + 0x84u, 0u);
    ctx.gpr[2] = body;
}

// 0x0894F0AC (a0 = this, f12 = value): the value is scaled, clamped to [-top, top] (top = *(body+0x78)), 0x089567C0 runs on
// this, the clamped value is stored at list+0x520 (list = *(this+8)), and the tail call 0x0894E9C0 takes this.
void func_0894F0AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float scale = std::bit_cast<float>(0x3C8EFA35u);
    float value = psp_mul(ctx.fpr[12], scale);
    {
        GuestFrame frame(ctx, 0x20u);
        const u32 body = word(rt, self + 0xCu);
        const float top = float_at(rt, body + 0x78u);
        const float low = -top;
        const u32 list = word(rt, self + 8u);
        if (value < low) {
            value = low;
        } else if (top < value) {
            value = top;
        }
        call(rt, ctx, 0x089567C0u, {self});
        put_float(rt, list + 0x520u, value);
    }
    tail_call(ctx, 0x0894E9C0u, {self});
}

// 0x08951280 (a0 = dst, a1 = src): three helpers on two 0x110-byte stack blocks, then 0x089511BC on the second with a2 = a1.
void func_08951280(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x230u);
    const u32 first = frame.at(0x110u);
    const u32 second = frame.at(0x20u);
    call(rt, ctx, 0x08958324u, {frame.at(0)});
    call(rt, ctx, 0x0894C460u, {first, a0, 0u, 0u});
    call(rt, ctx, 0x0895077Cu, {a0, second, first, 0u});
    call(rt, ctx, 0x089511BCu, {second, first, a1});
}

// 0x089BEBDC (a0 = this): the vtables at +0 and +0x38, the three sub-objects, and the zeroed flags.
void func_089BEBDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x089E20D0u, {self});
    put(rt, self, 0x08CCA568u);
    put(rt, self + 0x38u, 0x08CCA7A8u);
    call(rt, ctx, 0x08A19CC0u, {self + 0x22Cu});
    call(rt, ctx, 0x089C15FCu, {self + 0x264u, 0x08CF6ED8u});
    put(rt, self + 0x2A8u, 0xFFFFFFFFu);
    put8(rt, self + 0x2ACu, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x2B0u});
    put8(rt, self + 0x268u, 0u);
    put(rt, self + 0x2D8u, 0u);
    put(rt, self + 0x2DCu, 0u);
}

// 0x089D8DB0 (a0 = this): the vtables at +0 and +0x38, the function table word at +0x78, the zeroed fields. v0 = 0x08D74FDC.
void func_089D8DB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A0FDD8u, {self});
    put(rt, self, 0x08CBCDE0u);
    put(rt, self + 0x38u, 0x08CBCFCCu);
    put(rt, self + 0x78u, 0x08D74FDCu);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x88u, 0u);
    put8(rt, self + 0x8Cu, 0u);
    put8(rt, self + 0x8Du, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x98u, 0u);
    put(rt, self + 0x9Cu, 0u);
    put(rt, self + 0xA0u, 0u);
    ctx.gpr[2] = 0x08D74FDCu;
}

// 0x089F14D0 (a0 = object, a1..a3): a 0x2C-byte record from 0x08A5BC20, filled by 0x089F12C4, then 0x089FDAB0 with the
// record in the stack word.

// 0x089F6128 (a0 = this): the vtables, the 1.0 / 2.0 floats at +0x80 / +0x88, the zeroed fields. v0 = 0x08CC7E40.
void func_089F6128(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A0FDD8u, {self});
    put(rt, self, 0x08CC7E40u);
    put(rt, self + 0x38u, 0x08CC800Cu);
    put_float(rt, self + 0x80u, 1.0f);
    put_float(rt, self + 0x88u, 2.0f);
    put(rt, self + 0x9Cu, 0u);
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x94u, 0u);
    put8(rt, self + 0x98u, 0u);
    put8(rt, self + 0x99u, 0u);
    ctx.gpr[2] = 0x08CC7E40u;
}

// 0x08A0FE50 (a0 = this): the flags word at +0x60 gets bit 16 set and bits 17..23 and 24..27 cleared; four 0x08A76AA0
// calls on the sub-objects at +0x64..+0x70; +0x74 zeroed. v0 = the last call's result.
void func_08A0FE50(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x089BB804u, {self});
    u32 flags = word(rt, self + 0x60u) | 0x10000u;
    flags &= ~(0x7Fu << 17);
    flags &= ~(0xFu << 24);
    put(rt, self, 0x08CB9110u);
    put(rt, self + 0x38u, 0x08CB92DCu);
    put(rt, self + 0x60u, flags);
    call(rt, ctx, 0x08A76AA0u, {self + 0x64u});
    call(rt, ctx, 0x08A76AA0u, {self + 0x68u});
    call(rt, ctx, 0x08A76AA0u, {self + 0x6Cu});
    call(rt, ctx, 0x08A76AA0u, {self + 0x70u});
    put(rt, self + 0x74u, 0u);
}

// 0x08A11B80 (a0 = object, a1, a2): a 0x24-byte record, filled by 0x08A11780, then 0x08965854 with the record in the stack word.
void func_08A11B80(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x08A11780u, {record, a1, a2});
    put(rt, frame.at(0), record);
    call(rt, ctx, 0x08965854u, {object, frame.at(0)});
}

// 0x08A1A794 (a0 = this): the vtable, the sub-object at +0xC, and the half at +0x20 that the flags word keeps with bits 16 and
// 19..20 fixed. v0 = the flags word.
void func_08A1A794(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A5B828u, {self});
    put(rt, self, 0x08CD0678u);
    call(rt, ctx, 0x08969188u, {self + 0xCu, 0x08CF6ED8u});
    put16(rt, self + 0x20u, 0u);
    put(rt, self + 0x10u, 0u);
    u32 flags = word(rt, self + 0x20u);
    flags &= ~0x10000u;
    flags |= 0x20000u;
    flags |= 0x40000u;
    flags &= ~(0x3u << 19);
    put(rt, self + 0x20u, flags);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    ctx.gpr[2] = flags;
}

// 0x08A43C18 (a0 = this): the vtable, the two list headers at +0x14 and +0x2C pointing at their own fields, +0x44 = 0x08D74FDC.
void func_08A43C18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB88A8u);
    const u32 head1 = self + 0x14u;
    const u32 head2 = self + 0x2Cu;
    const u32 node1 = self + 0x18u;
    const u32 node2 = self + 0x30u;
    put(rt, head1 + 0x14u, 0u);
    put(rt, head1 + 0x10u, node1);
    put(rt, head1 + 0x4u, 0u);
    put(rt, head1 + 0x8u, 0u);
    put(rt, head1 + 0xCu, node1);
    put(rt, head2 + 0x10u, node2);
    put(rt, head2 + 0x4u, 0u);
    put(rt, head2 + 0x14u, 0u);
    put(rt, head2 + 0x8u, 0u);
    put(rt, head2 + 0xCu, node2);
    put(rt, self + 0x44u, 0x08D74FDCu);
    ctx.gpr[2] = 0x08D74FDCu;
}

// 0x08A44D1C (a0 = this, a1, a2, a3 & 0xFF): a 0x20-byte record from 0x08A5BC20, filled by 0x08A444FC, then 0x08A76AB4 with the
// record in the stack word.
void func_08A44D1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A444FCu, {record, a1, a2, a3});
    put(rt, frame.at(0), record);
    call(rt, ctx, 0x08A76AB4u, {object, frame.at(0)});
}

// 0x08A57784 (a0 = this, a1, a2, a3): the vtable, the two sub-objects at +0x14 and +0x18, and the word a3 at +0x1C.
void func_08A57784(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB7730u);
    call(rt, ctx, 0x08A52A98u, {self + 0x14u, a1});
    call(rt, ctx, 0x08A56010u, {self + 0x18u, a2});
    put(rt, self + 0x1Cu, a3);
}

// 0x08A75C80 (a0 = dst, a1 = src range {begin, end}): the dst block is zeroed, a block of (end - begin) & ~3 bytes is allocated,
// 0x08C250A0 copies into it, and dst holds the begin / end / result pointers.

// 0x08A989B4 (a0 = this): the vtable, then 0x089B856B0 on the first sub-object range; a non-empty range is freed (0x08A5BC54 with
// its size rounded down to 4) and 0x08A780B0 runs on this.

// 0x08B28ACC (a0 = this, a1..a3, t0): 0x08B27250 on this, the vtable at +0, +0xA4 zeroed, then a tail call to 0x08B28AA4.

// 0x08B28B44: the same body as 0x08B28ACC.

// 0x08BB1B9C (a0 = buffer {.., end at +4}, a1 = from, a2 = to): 0x08B79BAC moves the tail, 0x08BB1B8C copies the span
// [a1, a2) into the tail, and the end pointer moves back by the rounded span. v0 = a1.

// 0x08BB1C2C: the same shape as 0x08BB1B9C, the copy goes through 0x08BB1C1C. v0 = a1.

// 0x08C7A730 (a0 = this): the vtables at +0, +8 and +0xB0, two sub-calls (0x08C786B4 on this+0xC, 0x08C553B0 on this with
// 0x08CDC424), then a tail call to 0x08B6543C on this+0xB0.

}  // namespace
}  // namespace new_batch_main_1883
}  // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1883::name, gt::ReturnKind::kind)

NEW3(0x0884CB98u, func_0884CB98, Word);
NEW3(0x088703D4u, func_088703D4, Word);
NEW3(0x0894F0ACu, func_0894F0AC, Word);
NEW3(0x08951280u, func_08951280, Word);
NEW3(0x089BEBDCu, func_089BEBDC, Word);
NEW3(0x089D8DB0u, func_089D8DB0, Word);
NEW3(0x089F6128u, func_089F6128, Word);
NEW3(0x08A0FE50u, func_08A0FE50, Word);
NEW3(0x08A11B80u, func_08A11B80, Word);
NEW3(0x08A1A794u, func_08A1A794, Word);
NEW3(0x08A43C18u, func_08A43C18, Word);
NEW3(0x08A44D1Cu, func_08A44D1C, Word);
NEW3(0x08A57784u, func_08A57784, Word);
