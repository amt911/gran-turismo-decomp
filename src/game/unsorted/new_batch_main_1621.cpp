// New batch main-1621 (main-session batch 1621). Structured C from tools.disasm, drafted by a fast model, unverified:
// the scripts verify it later. Functions skipped (not drafted): float-argument functions (08A188E0, 08AE1498, 08B4AEC4,
// 08B1BD50, 08AFD67C), the holder/string family 288-byte drafts (088EB410, 08B945C4, 08BDD140, 08BEA858, 08BF3848,
// 08BF5E6C, 08BFEFAC, 08C06C3C, 08C0B6BC), 08B63AF8 (a0 is reloaded after calls) and 08B77D5C (indirect loop).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1621 {
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
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x089A6CBC (a0 = this, a1 = a word): stores the vtable 0x08CBAF98, the word a1 at sp+0, calls 0x089A32B4(this+0x14, sp),
// sets +0x1C = 2 and +0x18 = 3, then six 0x08A0FD28 / 0x08A76AAC calls on the embedded objects; v0 is the last call's.

// 0x08A99AB4 (a0 = this, a1 = a source with a begin word at +0x14 and an end word at +0x18): vtable 0x08CB7108; the byte
// count (end - begin) rounded down to 4 is allocated by 0x08A5BC20(count, 4) and the block copied by 0x08C2DC4C(begin, end,
// block, sp); this+0x14 / +0x18 / +0x1C hold block / block / block+count, this+0x20 is cleared. v0 = 0x08C2DC4C's result.

// 0x08A987EC (a0 = this, a1 = a count): vtable 0x08CB7108; this+0x14 / +0x18 / +0x1C take the block of count * 4 bytes
// from 0x08A5BC20, then 0x08A76AA0(sp+4), 0x08B85AF4(block, count, sp+4, sp) and 0x08A5B9EC(sp+4). v0 = the block.

// 0x08AD03EC (a0 = this): the object's fields are set (-2 at +0, flags at +4..+F, 4.0f at +0x42C, zeros in between),
// 0x08B2DE38(sp, 0x20, 1) and 0x08B37640(this+0x11, 0x08CB49E0, sp) run, then 0x08CB4A00..+4 is copied to this+0x211
// and the byte at 0x08CB4A04 to this+0x215; v0 = the word at 0x08CB4A00.

// 0x0891C290 (a0 = this): vtable 0x08CC5CA0; the three embedded holders at +0x2C, +0x58, +0x84 are cleared by 0x089263EC
// (each slot is set to 0x08CC6178 right after its call) and registered by 0x08926488 with the two strings, then the
// function tail-calls 0x0891AA34(this).

// 0x089C1C94 (a0 = this): calls 0x08A0FDD8(this), then sets the vtable 0x08CCA380, the vector-like blocks at +0x80, +0x90,
// +0xB0, +0xD0 to zero and the float constants as bit patterns: 1.0f (0x3F800000) at +0xA0 / +0xC0, 0.1f (0x3E99999A) at
// +0xC4, -1.0f (0xBF800000) at +0xDC / +0xD8 of the +0xD0 block. The stored v0 is 1.

// 0x08AFBB50 and 0x08AFBC0C (a0 = this, a1..a3, t0..t2 passed on): vtable 0x08CD78F8, 0x08CD7954 at +0xD4, zeros at
// +0xD8 / +0xDC; 0x08AFB4E8(this), 0x08AFB4B4(sp) and 0x08AFB0F8(sp) run, then 0x08AFA88C(this, a1, a2, a3, t0, t1, t2).
void draft_08AFB_group(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    GuestFrame frame(ctx, 0x100u);
    call(rt, ctx, 0x08AFB4E8u, {self});
    put(rt, self, 0x08CD78F8u);
    put(rt, self + 0xD4u, 0x08CD7954u);
    put(rt, self + 0xD8u, 0u);
    put(rt, self + 0xDCu, 0u);
    call(rt, ctx, 0x08AFB4B4u, {frame.at(0u)});
    call(rt, ctx, 0x08AFB0F8u, {frame.at(0u)});
    call(rt, ctx, 0x08AFA88Cu, {self, a1, a2, a3, t0, t1, t2});
}


// 0x08A63A6C (a0 = this): vtable 0x08CCB980; while the vector at +0x24..+0x28 is not empty, the last element (its vtable
// slots 0xC and 4) is destroyed and the end pointer steps back by 4; then 0x08A72B94(this+0x3C-word, this), two 0x08A5B9EC
// calls on +0x54 and +0x4C, 0x08C1FBC4(begin, end, sp), 0x08A5BC54 if the begin was not null, 0x08A5B930 on the word at
// +0x1C if not null, and the tail of the chain 0x08A780B0(this). v0 = the last call's result.

}  // namespace
}  // namespace new_batch_main_1621
}  // namespace gt::game

#define NEW1621(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1621::name, gt::ReturnKind::kind)
