// Hot batch 340: functions no route reaches, written as structured C from `tools.disasm` (control flow as ifs and loops,
// named locals; the argument registers are read once at the top and the result register written once at the end) and
// verified by synthetic cases. Leftover values of `v0` that the asm leaves (the result of the
// last call) are kept, because the harness compares `v0`.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_340 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal` (a callee that never writes v0 returns what
// v0 held when it was called, so the caller's v0 has to stay current).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void set_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void set_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Function-local `static` guard (a byte at `guard`; 0x08B6441C acquires, 0x08B64494 releases). Returns true when the caller
// has to construct the instance and release the guard afterwards.
bool guard_acquire(Runtime &rt, AllegrexContext &ctx, u32 guard) {
    return byte_at(rt, guard) == 0u && call(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
}

// 0x088B301C
void func_088B301C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 extra = ctx.gpr[6];
    const u32 elements = self + 0x2DCu;
    const u32 pointers = self + 0x2E4u;
    const u32 bits = self + 0x2F0u;
    put(rt, self, 0x08CC30A0u);
    put(rt, self + 4u, 0x08CC30B8u);
    put(rt, self + 8u, 0u);
    put(rt, elements, 0u);
    put(rt, elements + 4u, 0u);
    if (count != 0u) {
        put(rt, elements + 4u, count);
        const u32 block = call(rt, ctx, 0x08B6461Cu, {(count << 10) + 4u});
        put(rt, block, count);
        for (u32 index = 0u; index != count; ++index) call(rt, ctx, 0x088B4DB4u, {block + 4u + index * 0x400u});
        put(rt, elements, block + 4u);
    }
    put(rt, pointers, 0u);
    put(rt, pointers + 4u, 0u);
    if (count != 0u) {
        put(rt, pointers + 4u, count);
        put(rt, pointers, call(rt, ctx, 0x08B6461Cu, {count << 2}));
    }
    put(rt, bits, 0u);
    put(rt, bits + 4u, 0u);
    if (count != 0u) {
        put(rt, bits + 4u, count);
        put(rt, bits, call(rt, ctx, 0x08B6461Cu, {((count + 0x1Fu) >> 5) << 2}));
    }
    put(rt, self + 0x2FCu, 1u);
    put(rt, self + 0x304u, 0x10000u);
    put(rt, self + 0x30Cu, 0u);
    put(rt, self + 0x300u, 0u);
    put(rt, self + 0x308u, 0x0001330Eu);
    put(rt, self + 0x2F8u, 0u);
    call(rt, ctx, 0x088B3FC4u, {self + 0x310u, count, 0x0001330Eu, 0u});
    set_byte(rt, self + 0x329u, 2u);
    put(rt, self + 0x324u, extra);
    set_byte(rt, self + 0x328u, 0u);
    put(rt, self + 0x32Cu, 0u);
    put(rt, self + 0x330u, 0u);
    put(rt, frame.at(0x4u), 0u);
    call(rt, ctx, 0x08BB0C58u, {self + 0x2CCu, elements, frame.at(0x4u)});
    const u32 first = word(rt, pointers);
    put(rt, frame.at(0x0u), 0u);
    call(rt, ctx, 0x08BB0C78u, {first, first + (word(rt, pointers + 4u) << 2), frame.at(0x0u)});
    put(rt, self + 0x2ECu, word(rt, pointers));
    call(rt, ctx, 0x08B36018u, {word(rt, bits), 0u, ((word(rt, bits + 4u) + 0x1Fu) >> 5) << 2});
    set_byte(rt, self + 0x32Au, 0u);
}

} // namespace

} // namespace hot_batch_340
} // namespace gt::game

#define HOT340(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_340::name, gt::ReturnKind::kind)
HOT340(0x088B301Cu, func_088B301C, Word);
