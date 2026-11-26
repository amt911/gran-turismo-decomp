// Main-session batch 1140 (draft, unverified). Structured C from tools.disasm.
// Factory thunks of one shape: allocate(size, 4), construct the block in place with the holder word the caller passed,
// then hand the block to the holder's init through a one-word local; v0 is that init call's result.
// The 1122 drafts of these five were rejected because no input shape gave the construct a holder word whose object
// answers the vtable slot it calls (a fault, or a different object built); see shapes_main1140.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1140 {
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

} // namespace

// 0x08A58458: s2 = a0 (out), s1 = a1 (holder); obj = 0x08A5BC20(0x20, 4); 0x08A58394(obj, holder); local = obj;
// 0x08A76AB4(out, &local); v0 = its result. frame 0x20.
void func_08A58458(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, 0x08A58394u, {object, holder});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

// 0x08A5FA2C: the same shape with a 0x18 block, construct 0x08A5F3F8 and init 0x08A76AB4.
void func_08A5FA2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A5F3F8u, {object, holder});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

// 0x08A69084: the same shape with a 0x1C block, construct 0x08A68D48 and init 0x08A7026C.
void func_08A69084(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    call(rt, ctx, 0x08A68D48u, {object, holder});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A7026Cu, {out, frame.at(0u)});
}

// 0x08A6B6E8: the same shape with a 0x24 block, construct 0x08A6B590 and init 0x08A71E10.
void func_08A6B6E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x08A6B590u, {object, holder});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A71E10u, {out, frame.at(0u)});
}

// 0x08A6D3C0: the same shape with a 0x18 block, construct 0x08A6D290 and init 0x08A76AB4.
void func_08A6D3C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A6D290u, {object, holder});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

} // namespace new_batch_main_1140
} // namespace gt::game

#define NEW1140(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1140::name, gt::ReturnKind::kind)
NEW1140(0x08A58458u, func_08A58458, Word);
NEW1140(0x08A5FA2Cu, func_08A5FA2C, Word);
NEW1140(0x08A69084u, func_08A69084, Word);
NEW1140(0x08A6B6E8u, func_08A6B6E8, Word);
NEW1140(0x08A6D3C0u, func_08A6D3C0, Word);
