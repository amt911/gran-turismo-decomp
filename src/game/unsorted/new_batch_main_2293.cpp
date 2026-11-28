// Main batch 2293: two functions the gate rejected earlier, re-drafted from `tools.disasm`.
// 0x08B17848 keeps failing only on the setjmp record's return-address slot (call_guest hands that callee its own sentinel)
// and 0x0882D758 faulted because the second smart-pointer build call reads a1, which the asm re-sets in the delay-slot pair.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2293 {
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
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x08B17848 (a0 = self; a1, a2, a3, t0 and t1 are data): 0x08B34888(0x08D61100, a1, a2, a3) registers the object, then the
// base 0x08D51100 goes to +0x24, the byte t1 & 0xFF to +0x28, a1 + a2 to +0x1C and a1 to +0x14 / +0x18, with 0x08B26B28(self,
// a3, t0) reading the bit stream at a3 in between. 0x08B7B2E4(self + 0x2C) is the setjmp-shaped store of s0-s7, sp, fp, ra and
// f20-f31 into the 0x5C-byte record, so s0-s5 hold the asm's values there; its ra slot (self + 0x54) would get call_guest's own
// sentinel instead of the asm's 0x08B178D4, so it is written back. A zero answer calls 0x08B25968(self), then 0x08B34B24(0x08D61100)
// releases the lock. v0 = word(+0x1C) - word(+0x14).
void func_08B17848(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 flag = ctx.gpr[9] & 0xFFu;
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 saved_s2 = ctx.gpr[18];
    const u32 saved_s3 = ctx.gpr[19];
    const u32 saved_s4 = ctx.gpr[20];
    const u32 saved_s5 = ctx.gpr[21];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[21] = self;
    ctx.gpr[16] = flag;
    ctx.gpr[20] = t0;
    ctx.gpr[19] = a3;
    ctx.gpr[18] = a1;
    ctx.gpr[17] = a2;
    call(rt, ctx, 0x08B34888u, {0x08D61100u, a1, a2, a3});
    put(rt, frame.at(0u), self);
    put(rt, self + 0x24u, 0x08D51100u);
    put_byte(rt, self + 0x28u, flag);
    ctx.gpr[17] = a1 + a2;
    call(rt, ctx, 0x08B26B28u, {self, a3, t0});
    put(rt, self + 0x1Cu, a1 + a2);
    put(rt, self + 0x14u, a1);
    put(rt, self + 0x18u, a1);
    ctx.gpr[2] = self;
    if (call(rt, ctx, 0x08B7B2E4u, {self + 0x2Cu}) == 0u) {
        call(rt, ctx, 0x08B25968u, {self});
    }
    put(rt, self + 0x2Cu + 0x28u, 0x08B178D4u); // the record's ra slot (self + 0x54): the return address of the asm's jal
    call(rt, ctx, 0x08B34B24u, {0x08D61100u});
    const u32 answer = word(rt, self + 0x1Cu) - word(rt, self + 0x14u);
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
    ctx.gpr[18] = saved_s2;
    ctx.gpr[19] = saved_s3;
    ctx.gpr[20] = saved_s4;
    ctx.gpr[21] = saved_s5;
    ctx.gpr[2] = answer;
}

// 0x0882D758 (a0 = self): the base constructor 0x08A2A154(self) then the vtable 0x08CBBBC0 at +0; a 0x10-byte block from
// 0x08B62344(0x10) is built by 0x08865D1C(block) with 0x08A21F8C(block + 8, 0x08CF6ED8) and 0x08835AA8(block + 0xC,
// 0x08CF6ED8) — the asm re-loads a1 with that same constant before each of the two, and the second one dereferences it — and
// stored at +0x18. The one-time global 0x08D75160 (vtable 0x08CBE908 stored at 0x08D75168) is initialized when its signed byte
// is zero and 0x08B6441C(0x08D75160) answers non-zero; +0x1C = 0x08D75168, then 0x08860050(block) answers into +0x14.

} // namespace
} // namespace new_batch_main_2293
} // namespace gt::game

#define NEW2293(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2293::name, gt::ReturnKind::kind)
NEW2293(0x08B17848u, func_08B17848, Word);
