// New batch main-1622: hand-written structured C from tools.disasm, unverified (draft only; scripts verify later).
// Holder constructors (0x08C127F0, 0x08C14B28, 0x08C29EE4) differ only in the vtable they store; the name test in the
// asm (a2[0] == 0x08B867AC) selects a value equal to a2[0] in every case, so the holder takes a2[0] and a2[1] as they are.
// Singletons: 0x08B1A284 (call then tail call on this), 0x08AD31E0 (member reset, 50.0 and 1.0 float fields, one lazy flag).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1885 {
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
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

constexpr u32 kFloatOne = 0x3F800000u;
constexpr u32 kFloatFifty = 0x42480000u;

// The holder constructors: 0x08A69B3C(&temp, text); object = 0x08A5BC20(0x2C, 4); 0x08A98220(object, &temp); word 0 is the
// vtable, +0x14 = a2[0], +0x18 = a2[1], +0x1C/+0x20 = a3[0]/a3[1], +0x24/+0x28 = t0[0]/t0[1]; 0x08A69D68(&temp, a2[0]) finishes
// the temporary; the object goes to a stack slot (0x08A5D414), 0x08A7C5C8(self, &slot) takes it, 0x08A5B9EC(&slot) releases it.
void make_holder(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 pair = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    const u32 temp = frame.at(0x8u);
    const u32 slot = frame.at(0x4u);
    const u32 first = word(rt, pair);
    const u32 second = word(rt, pair + 4u);
    const u32 third_first = word(rt, third);
    const u32 third_second = word(rt, third + 4u);
    const u32 fourth_first = word(rt, fourth);
    const u32 fourth_second = word(rt, fourth + 4u);
    call(rt, ctx, 0x08A69B3Cu, {temp, text});
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    call(rt, ctx, 0x08A98220u, {object, temp});
    put(rt, object, vtable);
    put(rt, object + 0x14u, first);
    put(rt, object + 0x18u, second);
    put(rt, object + 0x1Cu, third_first);
    put(rt, object + 0x20u, third_second);
    put(rt, object + 0x24u, fourth_first);
    put(rt, object + 0x28u, fourth_second);
    call(rt, ctx, 0x08A69D68u, {temp, first});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A5D414u, {slot, frame.at(0u)});
    call(rt, ctx, 0x08A7C5C8u, {self, slot});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}

void func_08C127F0(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD0110u); }
void func_08C14B28(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD0350u); }
void func_08C29EE4(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD4228u); }

// 0x08B1A284 (a0 = this): 0x08B2E924(this), then the tail call 0x08B1A17C(this); its v0 is the answer.
void func_08B1A284(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B2E924u, {self});
    call(rt, ctx, 0x08B1A17Cu, {self});
}

// 0x08AD31E0 (a0 = this): resets the members of the object, allocates a 0xA0-byte block (its 0x80/0x18 init), stores it at
// +0x29C, the lazily initialised table at 0x08D77268 is set once (flag byte 0x08D77260), then 0x08C3DECC and 0x08B28DCC
// (the last one's v0 is the result).

} // namespace
} // namespace new_batch_main_1885
} // namespace gt::game

#define NEW1622(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1885::name, gt::ReturnKind::kind)

NEW1622(0x08C127F0u, func_08C127F0, Word);
NEW1622(0x08C14B28u, func_08C14B28, Word);
NEW1622(0x08C29EE4u, func_08C29EE4, Word);
NEW1622(0x08B1A284u, func_08B1A284, Word);
