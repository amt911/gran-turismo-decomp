// New batch main-519 (draft, unverified): small wrappers, inits and single virtual dispatches, structured C from tools.disasm.
// Their argument dereferences are shaped in shapes_main519.txt so the synthetic generator feeds them valid inputs.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_519 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The value `lui` leaves for an address a relocated lui/addiu (or lui/load/store) pair builds: the high half, rounded up
// when the low half is negative.
inline constexpr u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// The callee-saved s0 the prologues store and the epilogues put back.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved() { ctx.gpr[16] = s0; }
};

// 0x08ACCD54 (a1 = object, a3 = a pointer to a name word): v0 = a1; the tail jump func_08C7AD78 gets a0 = object[0xB0],
// a1 = object[0xB8], a2 untouched and a3 = *a3.

// 0x08805AF8: func_08B19FA4() runs, its answer goes to func_08B19F60(a0 = answer), and a0 = that answer is tail-jumped into
// func_08805C0C; the caller's v0 is the second call's answer (the swap does not touch v0).

// 0x08874BE0 (a0 = object, a1 = third argument): func_08874820(object[0xC], object[8], a1); v0 is its answer.

// 0x08930864: func_089306DC(0, 0) runs; v0 = 1 when its answer is not zero (sltu zero, v0), else 0.

// 0x08AC4440 (a0 = object): five zero words at +0x120..+0x130, then func_08B0C99C tail-runs with a0 = object + 8 and
// v0 = v1 = 0.

// 0x08B322A0: func_08C7ADF0() runs, the byte at 0x08D6916C becomes 0 and v0 keeps the call's answer (v1 = the lui of the
// address pair, the store's delay slot).

// 0x08BAA4E0 (a0 = first element, a1 = end): stores the vtable word 0x08CC1AA8 at +0xA4 of every 0xAC-byte element from a0 up
// to (never at) a1; a0 ends at the element that matched. v0 = 0x08CC1AA8, or the lui 0x08CC0000 when a0 == a1 (the delay slot
// of the early exit).

// 0x088FDD68 (a0 goes to func_088FD9AC): v0 keeps the call's answer, v1 = the word at answer + 8 with its bytes reversed
// (wsbw) and f0 = that word as a float (mtc1).

// 0x08A5176C (a0 = argument): func_08B2E924(a0) runs, then func_08A5159C tail-runs with a0 = the argument (s0 holds it);
// whatever the first call answered is still in v0 at the jump.

// 0x08AC2F90 (a0 = a holder): when its word +4 is zero the function returns with v0 = 0; otherwise v0 = v1 = holder[0] and
// slot 0x34 of that object's vtable is tail-jumped with a0 = the object, a1 = holder[4]. v0 is the object at the jump.
void func_08AC2F90(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 child = word(rt, holder + 4u);
    ctx.gpr[2] = child;
    if (child == 0u) return;
    const u32 object = word(rt, holder);
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[5] = child;
    ctx.gpr[4] = object;
    ctx.gpr[2] = object;
    tail_call(ctx, word(rt, table + 0x34u), {object, child});
}

// 0x08ACCD34 (a1 = object, a2 = value, a3 = t0 of the tail call): the word 0x08D00408 = a2, then func_08C7AD20 tail-runs with
// a0 = object[0xB4], a1 = object[0xBC], a2 = object[0xD0], a3 = 0x08D00408 and t0 = a3. v0 = the lui 0x08D00000.

// 0x08AD08D8: func_08B1A8D4() and then func_08B1AB08() run; the function answers 0.

// 0x08B114A4 (a0, a1 = second argument): func_08B11434(a0, a1) runs, then func_08B10A40 tail-runs with a0 = a1 (s0 holds
// it); v0 is the first call's answer at the jump.

// 0x08B120E0 (a0 = object): func_08C7B200(a0) runs, then func_08B2EE2C tail-runs with a0 = object + 0x4D8 (s0 holds the
// object); v0 is the first call's answer at the jump.

// 0x08B1B99C (a0 = argument): func_08B2E224(a0) runs, then func_08B2E28C tail-runs with a0 = the argument; v0 is the first
// call's answer at the jump.

// 0x08C0C3C4 (a0 = argument): func_089FA424(a0) runs; v0 = a0.

// 0x08C31850 (a0 = argument): func_08AA9028(a0) runs; v0 = a0.

// 0x08C319D8 (a0 = argument): func_08AA91F8(a0) runs; v0 = a0.

// 0x08C32AD4 (a0 = argument): func_08AA9B68(a0) runs; v0 = a0.

// 0x08C33F1C (a0 = argument): func_08AA9D34(a0) runs; v0 = a0.

// 0x08805774: func_08AB7754() runs, then func_08B49414 tail-runs with a0 = 0x08D6950C and a1 = the word 0x08D69524; v0 is the
// lui 0x08D70000 of the load's address pair.

// 0x0885C1FC (a0 = a holder of the table base, a1 = index, a2 = offset): the unsigned byte at
// a2 + (*(*a0)) + 392 * a1 + 0x40, where 392 = (56 * i) * 7.

// 0x088BF94C (a0 = tag, a1 = object): v0 = object[0x24]; the tag byte goes to +0x10 and +0x15 and the words +0x20, +0x54,
// +0x7C, +0x80 and +0x84 are cleared, then func_088BF82C tail-runs with a0 = object and v1 = tag & 0xFF.

// 0x08A17E90 (a0 = holder, a1, a2, a3): v0 = holder[0], a0 = a1 and t0 = a1; when holder[0][0] is zero the tail jump
// func_089672C8 gets (a1, a2, a3), otherwise (a1, a3, a2).

// The init both tagged objects share: sb of the tag at +4, then `lw a2, 4(a0)` / `ins a2, zero, 8, 24`, which keeps only the low
// byte a2 just stored and clears the rest — the word at +4 becomes the tag. Then the vtable goes to +0 and func_08A75D04
// tail-runs with a0 = object + 8 (v0 = the vtable address, v1 = object, a2 = the tag).
void tagged_object_init(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 tag) {
    const u32 self = ctx.gpr[4];
    rt.memory().store8(self + 4u, static_cast<std::uint8_t>(tag));
    ctx.gpr[3] = self;
    ctx.gpr[6] = tag;
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
    ctx.gpr[4] = self + 8u;
    put(rt, self + 4u, tag);  // the delay slot of the jump
    tail_call(ctx, 0x08A75D04u, {self + 8u});
}

// 0x08A64E60 (a0 = object): tag 3, vtable 0x08CD2908.

// 0x08A65EE0 (a0 = object): the same with tag 0x15 and vtable 0x08CD2BD8.

// 0x08A72600 (a0, a1 = second argument): v0 = func_08A5D444(a0, a1); then func_08A8DC58 tail-runs with a0 = a1 and
// a1 = that answer (s0 holds a1), v0 unchanged.

// 0x08A98290: the same, tail-jumping func_08A8DC88.

// 0x08AD0878: func_08B2F0D4() runs, then slot 0x24 of the vtable of the singleton word 0x08CFFAAC is tail-jumped with
// a0 = the singleton; v0 = the lui 0x08D00000 of the load's address pair.

// 0x08BE96DC (a0 = argument): func_08B88F44(a0, 0) runs; v0 = a0.

} // namespace
} // namespace new_batch_main_519
} // namespace gt::game

#define NEW519(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_519::name, gt::ReturnKind::kind)
NEW519(0x08AC2F90u, func_08AC2F90, Word);
