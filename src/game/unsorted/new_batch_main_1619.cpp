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
namespace new_batch_main_1619 {
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

// 0x08860484 (a0 = this, a1, a2): the engine at *this gets its busy byte (+0x3FDB) set around 0x088D7DFC.

// 0x08869900 (a0 = this): the calls that set up the sub-objects, then the vtable word at +0x1A0 and the zeroed block at +0x1A4.
// v0 = 1 on exit (the byte store comes after the last call).

// 0x088703D4 (a0 = this): when |*(body+0x84)| exceeds 0.000... (asm constant 0x3A83126F), 0x08B4F174 gets the scaled value
// and 0x08B4DBC4 the stack word; the body's +0x84 word is then zeroed.

// 0x0894F0AC (a0 = this, f12 = value): the value is scaled, clamped to [-top, top] (top = *(body+0x78)), 0x089567C0 runs on
// this, the clamped value is stored at list+0x520 (list = *(this+8)), and the tail call 0x0894E9C0 takes this.

// 0x08951280 (a0 = dst, a1 = src): three helpers on two 0x110-byte stack blocks, then 0x089511BC on the second with a2 = a1.

// 0x089BEBDC (a0 = this): the vtables at +0 and +0x38, the three sub-objects, and the zeroed flags.

// 0x089D8DB0 (a0 = this): the vtables at +0 and +0x38, the function table word at +0x78, the zeroed fields. v0 = 0x08D74FDC.

// 0x089F14D0 (a0 = object, a1..a3): a 0x2C-byte record from 0x08A5BC20, filled by 0x089F12C4, then 0x089FDAB0 with the
// record in the stack word.

// 0x089F6128 (a0 = this): the vtables, the 1.0 / 2.0 floats at +0x80 / +0x88, the zeroed fields. v0 = 0x08CC7E40.

// 0x08A0FE50 (a0 = this): the flags word at +0x60 gets bit 16 set and bits 17..23 and 24..27 cleared; four 0x08A76AA0
// calls on the sub-objects at +0x64..+0x70; +0x74 zeroed. v0 = the last call's result.

// 0x08A11B80 (a0 = object, a1, a2): a 0x24-byte record, filled by 0x08A11780, then 0x08965854 with the record in the stack word.

// 0x08A1A794 (a0 = this): the vtable, the sub-object at +0xC, and the half at +0x20 that the flags word keeps with bits 16 and
// 19..20 fixed. v0 = the flags word.

// 0x08A43C18 (a0 = this): the vtable, the two list headers at +0x14 and +0x2C pointing at their own fields, +0x44 = 0x08D74FDC.

// 0x08A44D1C (a0 = this, a1, a2, a3 & 0xFF): a 0x20-byte record from 0x08A5BC20, filled by 0x08A444FC, then 0x08A76AB4 with the
// record in the stack word.

// 0x08A57784 (a0 = this, a1, a2, a3): the vtable, the two sub-objects at +0x14 and +0x18, and the word a3 at +0x1C.

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
}  // namespace new_batch_main_1619
}  // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1619::name, gt::ReturnKind::kind)

