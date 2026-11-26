// Main-session batch 1111: 28 functions drafted from tools.disasm (draft only, unverified; the gate has not run).
// Skipped: 0x08AD9708 (float argument and return through f12/f0 across two calls) and 0x08AF4B1C (virtual call whose
// a1 leftover the second call reads).
// Vtable constructors: the base constructor gets this (a0) in the first call, the vtable is stored at *this and v0 keeps it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1111 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// 0x08A68D18: base ctor(this), *this = vtable 0x08CCDEE0, v0 = vtable.
void func_08A68D18(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A70B1Cu, {self});
    put(rt, self, 0x08CCDEE0u);
    ctx.gpr[2] = 0x08CCDEE0u;
}

// 0x08A725D0: base 0x08A5D5A0, vtable 0x08CD3990.
void func_08A725D0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A5D5A0u, {self});
    put(rt, self, 0x08CD3990u);
    ctx.gpr[2] = 0x08CD3990u;
}

// 0x08A8DBC8: builds a local object with 0x08A8DB94(out, a0, a1, 8), then returns 0x08A5B9EC(out).
void func_08A8DBC8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[29];
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    call(rt, ctx, 0x08A8DB94u, {out, a0, a1, 8u});
    call(rt, ctx, 0x08A5B9ECu, {out});
}

// 0x08AB4A88: base 0x08A76ACC, vtable 0x08CD4948.
void func_08AB4A88(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD4948u);
    ctx.gpr[2] = 0x08CD4948u;
}

// 0x08AB4AB8: base 0x08A76ACC, vtable 0x08CD4948.
void func_08AB4AB8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD4948u);
    ctx.gpr[2] = 0x08CD4948u;
}

// 0x08AB4AE8: base 0x08A782D8, vtable 0x08CD4948.
void func_08AB4AE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A782D8u, {self});
    put(rt, self, 0x08CD4948u);
    ctx.gpr[2] = 0x08CD4948u;
}

// 0x08AB4B18: base 0x08A782D8, vtable 0x08CD4948.
void func_08AB4B18(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A782D8u, {self});
    put(rt, self, 0x08CD4948u);
    ctx.gpr[2] = 0x08CD4948u;
}

// 0x08ABF8A0: base 0x08AC01C8, vtable 0x08CD6430.
void func_08ABF8A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08AC01C8u, {self});
    put(rt, self, 0x08CD6430u);
    ctx.gpr[2] = 0x08CD6430u;
}

// 0x08AC01F8: base 0x08C3C624, vtable 0x08CD6440.
void func_08AC01F8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08C3C624u, {self});
    put(rt, self, 0x08CD6440u);
    ctx.gpr[2] = 0x08CD6440u;
}

// 0x08AC0A7C: 0x08AC0A14(a0, a1, out, a2, a3 & 0xFF) with out = two zeroed words on the stack; v0 is the callee's.
void func_08AC0A7C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[29];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7] & 0xFFu;
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    call(rt, ctx, 0x08AC0A14u, {ctx.gpr[4], ctx.gpr[5], out, a2, a3});
}

// 0x08AC0E34: base 0x08B0E85C, vtable 0x08CD6500.
void func_08AC0E34(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B0E85Cu, {self});
    put(rt, self, 0x08CD6500u);
    ctx.gpr[2] = 0x08CD6500u;
}

// 0x08AC44E0: base 0x08ABA00C, vtable 0x08CD66B8.
void func_08AC44E0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABA00Cu, {self});
    put(rt, self, 0x08CD66B8u);
    ctx.gpr[2] = 0x08CD66B8u;
}

// 0x08AC4540: base 0x08ABA0BC, vtable 0x08CD66B8.
void func_08AC4540(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABA0BCu, {self});
    put(rt, self, 0x08CD66B8u);
    ctx.gpr[2] = 0x08CD66B8u;
}

// 0x08AC6A10: base 0x08ABA00C, vtable 0x08CD6740.
void func_08AC6A10(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABA00Cu, {self});
    put(rt, self, 0x08CD6740u);
    ctx.gpr[2] = 0x08CD6740u;
}

// 0x08AC6A70: base 0x08ABA0BC, vtable 0x08CD6740.
void func_08AC6A70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABA0BCu, {self});
    put(rt, self, 0x08CD6740u);
    ctx.gpr[2] = 0x08CD6740u;
}

// 0x08AC8BDC: base 0x08ABBB90, vtable 0x08CD67F0.
void func_08AC8BDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABBB90u, {self});
    put(rt, self, 0x08CD67F0u);
    ctx.gpr[2] = 0x08CD67F0u;
}

// 0x08AC9294: base 0x08ABC5E0, vtable 0x08CD6890.
void func_08AC9294(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABC5E0u, {self});
    put(rt, self, 0x08CD6890u);
    ctx.gpr[2] = 0x08CD6890u;
}

// 0x08ADDA60: 0x08ADD8D8(a0, a1, out) fills out; returns (out[0] & 2) ? out[8] : -1.
void func_08ADDA60(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 out = ctx.gpr[29];
    call(rt, ctx, 0x08ADD8D8u, {ctx.gpr[4], ctx.gpr[5], out});
    const u32 flags = word(rt, out);
    const u32 value = word(rt, out + 8u);
    ctx.gpr[2] = (flags & 2u) != 0u ? value : 0xFFFFFFFFu;
}

// 0x08ADDA90: 0x08ADD8D8(a0, a1, out) fills out; returns (out[0] & 0x20) ? out[0x18] : 0.
void func_08ADDA90(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 out = ctx.gpr[29];
    call(rt, ctx, 0x08ADD8D8u, {ctx.gpr[4], ctx.gpr[5], out});
    const u32 flags = word(rt, out);
    const u32 value = word(rt, out + 0x18u);
    ctx.gpr[2] = (flags & 0x20u) != 0u ? value : 0u;
}

// 0x08AF2CF4: *this = a1; r = 0x08AF296C(a1); tail call 0x08BD3210(r, r[0], r[1]).
void func_08AF2CF4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    put(rt, self, value);
    const u32 result = call(rt, ctx, 0x08AF296Cu, {value});
    const u32 first = word(rt, result);
    const u32 second = word(rt, result + 4u);
    tail_call(ctx, 0x08BD3210u, {result, first, second});
}

// 0x08AF92BC: 0x08C40904(a0, a0 + 0xC0, out) with three zeroed words on the stack.
void func_08AF92BC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[29];
    const u32 self = ctx.gpr[4];
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    call(rt, ctx, 0x08C40904u, {self, self + 0xC0u, out});
}

// 0x08AFBD10: 0x08B568DC(this + 0x10); then the byte at +0x50 is cleared; v0 is the callee's.
void func_08AFBD10(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B568DCu, {self + 0x10u});
    rt.memory().store8(self + 0x50u, static_cast<std::uint8_t>(0u));
}

// 0x08B100A4: 0x08B10048(a0, a1, out, a2 & 0xFF) with two zeroed words on the stack; v0 is the callee's.
void func_08B100A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[29];
    const u32 a2 = ctx.gpr[6] & 0xFFu;
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    call(rt, ctx, 0x08B10048u, {ctx.gpr[4], ctx.gpr[5], out, a2});
}

// 0x08B1BF80: 0x08B28DC4(this + 0x98); then tail call 0x08B28DC4(this).
void func_08B1BF80(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B28DC4u, {self + 0x98u});
    tail_call(ctx, 0x08B28DC4u, {self});
}

// 0x08B2E030: low = 0x08B2DF58() (a0 passed through); v0 = low + 0x7E90 with the carry added to the callee's v1.

// 0x08B3F4A4: 0x08B3FA98(this + 0x12); then tail call 0x08B3E788(this).
void func_08B3F4A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B3FA98u, {self + 0x12u});
    tail_call(ctx, 0x08B3E788u, {self});
}

// 0x08B3FEA8: 0x08B3F838(this); then tail call 0x08B3F7A8(this, 2).
void func_08B3FEA8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B3F838u, {self});
    tail_call(ctx, 0x08B3F7A8u, {self, 2u});
}

// 0x08B3FED4: same as 0x08B3FEA8.
void func_08B3FED4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B3F838u, {self});
    tail_call(ctx, 0x08B3F7A8u, {self, 2u});
}

} // namespace
} // namespace new_batch_main_1111
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1111::name, gt::ReturnKind::kind)
NEWM(0x08A68D18u, func_08A68D18, Word);
NEWM(0x08A725D0u, func_08A725D0, Word);
NEWM(0x08A8DBC8u, func_08A8DBC8, Word);
NEWM(0x08AB4A88u, func_08AB4A88, Word);
NEWM(0x08AB4AB8u, func_08AB4AB8, Word);
NEWM(0x08AB4AE8u, func_08AB4AE8, Word);
NEWM(0x08AB4B18u, func_08AB4B18, Word);
NEWM(0x08ABF8A0u, func_08ABF8A0, Word);
NEWM(0x08AC01F8u, func_08AC01F8, Word);
NEWM(0x08AC0A7Cu, func_08AC0A7C, Word);
NEWM(0x08AC0E34u, func_08AC0E34, Word);
NEWM(0x08AC44E0u, func_08AC44E0, Word);
NEWM(0x08AC4540u, func_08AC4540, Word);
NEWM(0x08AC6A10u, func_08AC6A10, Word);
NEWM(0x08AC6A70u, func_08AC6A70, Word);
NEWM(0x08AC8BDCu, func_08AC8BDC, Word);
NEWM(0x08AC9294u, func_08AC9294, Word);
NEWM(0x08ADDA60u, func_08ADDA60, Word);
NEWM(0x08ADDA90u, func_08ADDA90, Word);
NEWM(0x08AF2CF4u, func_08AF2CF4, Word);
NEWM(0x08AF92BCu, func_08AF92BC, Word);
NEWM(0x08AFBD10u, func_08AFBD10, Word);
NEWM(0x08B100A4u, func_08B100A4, Word);
NEWM(0x08B1BF80u, func_08B1BF80, Word);
NEWM(0x08B3F4A4u, func_08B3F4A4, Word);
NEWM(0x08B3FEA8u, func_08B3FEA8, Word);
NEWM(0x08B3FED4u, func_08B3FED4, Word);
