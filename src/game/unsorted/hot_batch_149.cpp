// Hot batch 149: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_149 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// held_call() with the member's callee: a holder h = func_08A3CD78(&h) (the arguments as they came), its object o (null:
// func_08A5BB7C(0x08CAD754) first), callee(o + 20), h destroyed (func_08A5B9EC).
void held_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, 0x08A3CD78u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08CAD754u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD754u});
    call_set(rt, ctx, callee, {object + 20u});
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// fields_call(this) with the member's offsets and callee: callee(this->[first], this->[second]); its v0 back.
void fields_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t second, std::uint32_t callee,
                 std::uint32_t first) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, callee, {word(rt, self + first), word(rt, self + second)});
}

// virtual_float(float *out, owner) with the member's vtable slot: o = owner->+0xB04; *out = o's virtual (o) (a float).
void virtual_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[5] + 0xB04u);
    const std::uint32_t table = word(rt, object);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = object;
    ctx.gpr[5] = table;
    const GuestResult result = call_indirect(rt, ctx, word(rt, table + slot), {object}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    put(rt, out, std::bit_cast<std::uint32_t>(result.f0));
}

// forward_field(this, ...) with the member's target and offset: target(this, this->[offset], a2, a3) (tail call).
void forward_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t offset) {
    tail_call(ctx, target, {ctx.gpr[4], word(rt, ctx.gpr[4] + offset)});
}

// virtual_pass(this, ...) with the member's vtable slot: this's virtual at that slot with the arguments as they came.
void virtual_pass(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t table = word(rt, self);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = table;
    const GuestResult result = call_indirect(rt, ctx, word(rt, table + slot), {self}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// byte_at(this) with the member's offset: func_08B61534(this + offset) & 0xFF.
void byte_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_set(rt, ctx, 0x08B61534u, {ctx.gpr[4] + offset}).v0;
    ctx.gpr[2] = value & 0xFFu;
}

} // namespace

// Templates of their families (the members are the batch 149 rows below).
void func_08A3CDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B28C74u); }
void func_08891098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 4u, 0x0888FC28u, 0u); }
void func_08899E50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x64u); }
void func_089B5E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x089B5D48u, 0x138u); }
void func_08C58904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 8u); }
void func_08C79184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 44u); }

void func_08A3CE04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B28C44u); }
void func_08A3CE50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B28D94u); }
void func_08A3CE9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B34CA0u); }
void func_08A3CEE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B34C1Cu); }
void func_08A3CF34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B34B2Cu); }
void func_08A3CF80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B34B24u); }
void func_08A3CFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B349A8u); }
void func_08A3D018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_call(rt, ctx, 0x08B34888u); }
void func_08A82CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B6EF68u, 24u); }
void func_08A82CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B6EE80u, 24u); }
void func_08A8AA90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B71DC8u, 24u); }
void func_08A8AAB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B6F300u, 24u); }
void func_08A8AB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B6F110u, 24u); }
void func_08A8AB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B6F1B8u, 24u); }
void func_08A8AB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fields_call(rt, ctx, 28u, 0x08B71EC8u, 24u); }
void func_08899E88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x68u); }
void func_08899EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x7Cu); }
void func_08899F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x90u); }
void func_08899F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x5Cu); }
void func_08899F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x60u); }
void func_08899FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x68u); }
void func_0889A01C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float(rt, ctx, 0x6Cu); }
void func_08A1B388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFCCu, 0x8u); }
void func_08A1B390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFD4u, 0x8u); }
void func_08A1B398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFDCu, 0x8u); }
void func_08A1B3A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFE4u, 0x8u); }
void func_08A1B3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFECu, 0x8u); }
void func_08A1B3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACAFF4u, 0x8u); }
void func_08A1B3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_field(rt, ctx, 0x08ACADA0u, 0x8u); }
void func_08C58928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 12u); }
void func_08C58B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 8u); }
void func_08C58BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 12u); }
void func_08C594CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 8u); }
void func_08C594F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_pass(rt, ctx, 12u); }
void func_08C791C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 44u); }
void func_08C791FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 40u); }
void func_08C79238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 40u); }
void func_08C79274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 48u); }
void func_08C792B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_at(rt, ctx, 48u); }

} // namespace hot_batch_149
} // namespace gt::game

#define HOT149(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_149::name, gt::ReturnKind::kind)
// HOT149(0x08A3CDB8u, func_08A3CDB8, Word);  // no comparable synthetic case yet
HOT149(0x08891098u, func_08891098, Word);
// HOT149(0x08899E50u, func_08899E50, Float);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT149(0x089B5E10u, func_089B5E10, Word);
HOT149(0x08C58904u, func_08C58904, Word);
HOT149(0x08C79184u, func_08C79184, Word);
// HOT149(0x08A3CE04u, func_08A3CE04, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CE50u, func_08A3CE50, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CE9Cu, func_08A3CE9C, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CEE8u, func_08A3CEE8, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CF34u, func_08A3CF34, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CF80u, func_08A3CF80, Word);  // no comparable synthetic case yet
// HOT149(0x08A3CFCCu, func_08A3CFCC, Word);  // no comparable synthetic case yet
// HOT149(0x08A3D018u, func_08A3D018, Word);  // no comparable synthetic case yet
HOT149(0x08A82CA0u, func_08A82CA0, Word);
HOT149(0x08A82CC0u, func_08A82CC0, Word);
HOT149(0x08A8AA90u, func_08A8AA90, Word);
HOT149(0x08A8AAB0u, func_08A8AAB0, Word);
HOT149(0x08A8AB18u, func_08A8AB18, Word);
HOT149(0x08A8AB38u, func_08A8AB38, Word);
HOT149(0x08A8AB58u, func_08A8AB58, Word);
HOT149(0x08899E88u, func_08899E88, Float);
HOT149(0x08899EC8u, func_08899EC8, Float);
HOT149(0x08899F00u, func_08899F00, Float);
HOT149(0x08899F38u, func_08899F38, Float);
HOT149(0x08899F70u, func_08899F70, Float);
HOT149(0x08899FE4u, func_08899FE4, Float);
// HOT149(0x0889A01Cu, func_0889A01C, Float);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT149(0x08A1B388u, func_08A1B388, Word);
HOT149(0x08A1B390u, func_08A1B390, Word);
HOT149(0x08A1B398u, func_08A1B398, Word);
HOT149(0x08A1B3A0u, func_08A1B3A0, Word);
HOT149(0x08A1B3A8u, func_08A1B3A8, Word);
HOT149(0x08A1B3B0u, func_08A1B3B0, Word);
HOT149(0x08A1B3C4u, func_08A1B3C4, Word);
HOT149(0x08C58928u, func_08C58928, Word);
HOT149(0x08C58B9Cu, func_08C58B9C, Word);
HOT149(0x08C58BC0u, func_08C58BC0, Word);
HOT149(0x08C594CCu, func_08C594CC, Word);
HOT149(0x08C594F0u, func_08C594F0, Word);
HOT149(0x08C791C0u, func_08C791C0, Word);
HOT149(0x08C791FCu, func_08C791FC, Word);
HOT149(0x08C79238u, func_08C79238, Word);
HOT149(0x08C79274u, func_08C79274, Word);
HOT149(0x08C792B0u, func_08C792B0, Word);
