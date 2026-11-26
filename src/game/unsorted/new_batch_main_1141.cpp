// Main-session batch 1141 (draft, unverified): functions an earlier draft (batch 1122) mismatched the gate on.
// Structured C from tools.disasm; each body fixes the difference the gate reported against the asm.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1141 {
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

// The same call keeping v1: a caller that hands a 64-bit value on reads both words (a promoted double's pair).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}

// `jal 0x08A5BC20` allocates the block, `build` constructs on it, and the word that goes into the local is the block
// pointer: the delay slot `addu s0, v0` of the build call runs before it, so it keeps the allocation, not the
// constructor's v0 (the constructor returns its vtable). 0x08A76AB4 then takes the object and the local's address;
// v0 is that last call's.
void alloc_build_store(Runtime &rt, AllegrexContext &ctx, u32 size, u32 build, u32 argument, u32 store) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, build, {block, argument});
    put(rt, frame.at(0u), block);
    call(rt, ctx, store, {self, frame.at(0u)});
}

} // namespace

// 08A732EC `seh $s1, $a1`: the key is the sign-extended halfword of a1.
void func_08A732EC(Runtime &rt, AllegrexContext &ctx) {
    alloc_build_store(rt, ctx, 0x18u, 0x08A72D38u, static_cast<u32>(static_cast<std::int16_t>(ctx.gpr[5])), 0x08A76AB4u);
}

void func_08A8516C(Runtime &rt, AllegrexContext &ctx) {
    alloc_build_store(rt, ctx, 0x18u, 0x08A84D10u, ctx.gpr[5], 0x08A76AB4u);
}

void func_08A85ABC(Runtime &rt, AllegrexContext &ctx) {
    alloc_build_store(rt, ctx, 0x18u, 0x08A854CCu, ctx.gpr[5], 0x08A76AB4u);
}

// 0x08B70F28 turns the float in f12 into the two words of a promoted double (v0 low, v1 high). The formatter is
// variadic: its va_list is built from a3 (the alignment slot before the 8-byte value) and t0-t3, so the value's words
// are passed as t0/t1 and the caller's t2/t3 follow.
void func_08AAB9D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 pad = ctx.gpr[7];
    const u32 t2 = ctx.gpr[10], t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x30u);
    const GuestResult value = call_keep(rt, ctx, 0x08B70F28u, {self});
    call(rt, ctx, 0x08B3767Cu, {frame.at(0u), 0x20u, 0x08CB31A8u, pad, value.v0, value.v1, t2, t3});
    const u32 measured = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    call(rt, ctx, 0x08BEED1Cu, {self, frame.at(0u), measured});
    ctx.gpr[2] = self;
}

// `addu a1, a2` before the first call: 0x08AF8904 gets the third argument as its a1 (it reads +0x28 of it), not the
// caller's a1. The second call gets the inner object, the caller's a1/a2 and a3 = 0.
void func_08AF9F58(Runtime &rt, AllegrexContext &ctx) {
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 inner = word(rt, a2);
    call(rt, ctx, 0x08AF8904u, {inner, a2});
    call(rt, ctx, 0x08AF9E88u, {inner, a1, a2, 0u});
}

} // namespace new_batch_main_1141
} // namespace gt::game

#define NEW1141(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1141::name, gt::ReturnKind::kind)
NEW1141(0x08A732ECu, func_08A732EC, Word);
NEW1141(0x08A8516Cu, func_08A8516C, Word);
NEW1141(0x08A85ABCu, func_08A85ABC, Word);
NEW1141(0x08AAB9D8u, func_08AAB9D8, Word);
NEW1141(0x08AF9F58u, func_08AF9F58, Word);
