// Main-session batch 1115 (unverified draft, drafted from tools.disasm). Functions that are mostly wrappers:
// a call with a constant, then a tail call with this/the argument; list pushes on the global at 0x08D77350;
// small field stores after one call. Calls pass the argument registers as the asm leaves them; leftovers
// of the callee (a2/a3 or a0/a1 read after a call) are taken from ctx as the callee left them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1115 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// 0x08AE64E4 family: func_2E7808(); v0 = func_2D4C5C(name, 0); func_2E782C(); return v0.
void probe_then_result(Runtime &rt, AllegrexContext &ctx, u32 name) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AEB808u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const u32 result = call(rt, ctx, 0x08AD8C5Cu, {name, 0u});
    call(rt, ctx, 0x08AEB82Cu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = result;
}

// Wrapper: first(this, 0x08CF6ED8), then tail(this, argument).
void const_call_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, first, {self, 0x08CF6ED8u, ctx.gpr[6], ctx.gpr[7]});
    }
    tail_call(ctx, tail, {self, argument, ctx.gpr[6], ctx.gpr[7]});
}

// init(this); then this pushed on the list whose head is the global 0x08D77350; node = this + offset.
void push_onto_list(Runtime &rt, AllegrexContext &ctx, u32 init, u32 offset) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, init, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[4] = 0x000A0000u; // lui $a0, 0xA leaves the high half in a0
    const u32 head = word(rt, 0x08D77350u);
    const u32 node = self + offset;
    put(rt, node + 4u, self);
    put(rt, node, head);
    put(rt, 0x08D77350u, node);
    ctx.gpr[2] = node;
    ctx.gpr[3] = head;
}

void func_08AE64E4(Runtime &rt, AllegrexContext &ctx) { probe_then_result(rt, ctx, 0x08D40944u); }
void func_08AE6520(Runtime &rt, AllegrexContext &ctx) { probe_then_result(rt, ctx, 0x08D4095Cu); }
void func_08AE9378(Runtime &rt, AllegrexContext &ctx) { probe_then_result(rt, ctx, 0x08D4098Cu); }
void func_08AE93B4(Runtime &rt, AllegrexContext &ctx) { probe_then_result(rt, ctx, 0x08D409A4u); }
void func_08AE93F0(Runtime &rt, AllegrexContext &ctx) { probe_then_result(rt, ctx, 0x08D409BCu); }

// 0x08B04D5C: inner = *this (read before the call); v = func_2FFB98(a0..a3); tail func_300C78(inner, v, a2, a3).
void func_08B04D5C(Runtime &rt, AllegrexContext &ctx) {
    const u32 inner = word(rt, ctx.gpr[4]);
    const u32 third = ctx.gpr[6];
    u32 value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call(rt, ctx, 0x08B03B98u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    tail_call(ctx, 0x08B04C78u, {inner, value, third, ctx.gpr[7]});
}

void func_08B125F0(Runtime &rt, AllegrexContext &ctx) { push_onto_list(rt, ctx, 0x08B11BF0u, 0x4F0u); }
void func_08B14CBC(Runtime &rt, AllegrexContext &ctx) { push_onto_list(rt, ctx, 0x08B14BECu, 0xA8u); }
void func_08B14FE4(Runtime &rt, AllegrexContext &ctx) { push_onto_list(rt, ctx, 0x08B14D28u, 0xA8u); }

// 0x08B1AF7C: func_324DCC(this); this+0x28..0x34 = 0 (8-byte pairs); v0 = v1 = 0.
void func_08B1AF7C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B28DCCu, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = 0u;
}

// 0x08B1C0C8: func_31808C(this); this+0xC, +4, +8 = -1; v0 = -1.
void func_08B1C0C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B1C08Cu, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    put(rt, self + 0x4u, 0xFFFFFFFFu);
    put(rt, self + 0x8u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08B77AC8: func_476B00 (no own arguments); v0 is its result.

void func_08A597E8(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A52AB0u, 0x08A5973Cu); }
void func_08A59CAC(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A52AB0u, 0x08A59C00u); }
void func_08A5D314(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A5D228u); }
void func_08A5DDCC(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A5DC48u); }
void func_08A60DD0(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A60CE4u); }
void func_08A68F94(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A7026Cu, 0x08A68EA8u); }
void func_08A69AFC(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A69A0Cu); }
void func_08A6C450(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A6C360u); }
void func_08A731C8(Runtime &rt, AllegrexContext &ctx) { const_call_then_tail(rt, ctx, 0x08A76AB4u, 0x08A730D8u); }

// 0x08A5F3B8: func_272ACC(this, a1); this+0x14 = a1; this+0 = 0x08CBDFD0; v0 = 0x08CBDFD0.
void func_08A5F3B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self, value, ctx.gpr[6], ctx.gpr[7]});
    }
    put(rt, self + 0x14u, value);
    put(rt, self, 0x08CBDFD0u);
    ctx.gpr[2] = 0x08CBDFD0u;
}

// 0x08A72CF8: func_272ACC(this, a1); halfword this+0x14 = a1 (seh then sh: the low 16 bits); this+0 = 0x08CD1FB0; v0 = 0x08CD1FB0.
void func_08A72CF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const std::uint16_t half = static_cast<std::uint16_t>(ctx.gpr[5]);
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    rt.memory().store16(self + 0x14u, half);
    put(rt, self, 0x08CD1FB0u);
    ctx.gpr[2] = 0x08CD1FB0u;
}

} // namespace
} // namespace new_batch_main_1115
} // namespace gt::game

#define NEW1115(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1115::name, gt::ReturnKind::kind)
NEW1115(0x08AE64E4u, func_08AE64E4, Word);
NEW1115(0x08AE6520u, func_08AE6520, Word);
NEW1115(0x08AE9378u, func_08AE9378, Word);
NEW1115(0x08AE93B4u, func_08AE93B4, Word);
NEW1115(0x08AE93F0u, func_08AE93F0, Word);
NEW1115(0x08B04D5Cu, func_08B04D5C, Word);
NEW1115(0x08B125F0u, func_08B125F0, Word);
NEW1115(0x08B14CBCu, func_08B14CBC, Word);
NEW1115(0x08B14FE4u, func_08B14FE4, Word);
NEW1115(0x08B1AF7Cu, func_08B1AF7C, Word);
NEW1115(0x08B1C0C8u, func_08B1C0C8, Word);
NEW1115(0x08A597E8u, func_08A597E8, Word);
NEW1115(0x08A59CACu, func_08A59CAC, Word);
NEW1115(0x08A5D314u, func_08A5D314, Word);
NEW1115(0x08A5DDCCu, func_08A5DDCC, Word);
NEW1115(0x08A5F3B8u, func_08A5F3B8, Word);
NEW1115(0x08A60DD0u, func_08A60DD0, Word);
NEW1115(0x08A68F94u, func_08A68F94, Word);
NEW1115(0x08A69AFCu, func_08A69AFC, Word);
NEW1115(0x08A6C450u, func_08A6C450, Word);
NEW1115(0x08A72CF8u, func_08A72CF8, Word);
NEW1115(0x08A731C8u, func_08A731C8, Word);
