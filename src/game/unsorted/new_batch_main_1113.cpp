// Main-session batch 1113 (30 functions, draft only, unverified). Structured C from tools.disasm.
// Shapes: gt-ops/shapes/shapes_main1113.txt.
//  - 08B247B0, 08B247E4, 08B24818, 08B32EF8: first call on this, then *this = vtable, then tail call on this + offset
//    (v0 = vtable on the tail entry).
//  - 08B27F74, 08A64668: call on this, *this = vtable, return the vtable (v0).
//  - 08B331E4: call on this, zero four fields, return the call's v0.
//  - 08B4AB98: copy the 3-word vector at a2 to a stack local, call 08B4AB04 with the local as a2; return its v0.
//  - 21 pairs 08B4BC24..08B4CCF0: a local filled by `first`, then `second(this, &local)`; return second's v0.
//  - 08BEECE8: *a0 = result of 08BEEC3C(a1, a2, a3); return it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1113 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Saves $s0 on entry and restores it at scope exit, as the asm's sw/lw of $s0 around the body.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 s0;
    explicit SavedS0(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = s0; }
};

// call first(this); *this = vtable (after the call, as in the asm); v0 = vtable; tail call tail(this + tail_offset).
void call_store_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first, u32 vtable, u32 tail, u32 tail_offset) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, first, {self});
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
    }
    tail_call(ctx, tail, {self + tail_offset});
}

// The pair shape: first(&local) fills a stack local, then second(this, &local); v0 = second's result.
void local_pair(Runtime &rt, AllegrexContext &ctx, u32 first, u32 second) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 local = ctx.gpr[29];
    call(rt, ctx, first, {local});
    call(rt, ctx, second, {self, local});
}

void func_08B247B0(Runtime &rt, AllegrexContext &ctx) { call_store_then_tail(rt, ctx, 0x08B248FCu, 0x08CD9610u, 0x08B28DCCu, 0x1Cu); }
void func_08B247E4(Runtime &rt, AllegrexContext &ctx) { call_store_then_tail(rt, ctx, 0x08B24954u, 0x08CD9610u, 0x08B28DCCu, 0x1Cu); }
void func_08B24818(Runtime &rt, AllegrexContext &ctx) { call_store_then_tail(rt, ctx, 0x08B24954u, 0x08CD9610u, 0x08B28DCCu, 0x1Cu); }
void func_08B32EF8(Runtime &rt, AllegrexContext &ctx) { call_store_then_tail(rt, ctx, 0x08B219B0u, 0x08CDA840u, 0x08B34DA8u, 0x04u); }

void func_08B27F74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B27250u, {self});
    put(rt, self, 0x08CD9678u);
    put(rt, self + 0xA4u, 0u);
    ctx.gpr[2] = 0x08CD9678u;
}

void func_08B331E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B34DA8u, {self});
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x08u, 0u);
    rt.memory().store8(self + 0x0Cu, static_cast<std::uint8_t>(0u));
    put(rt, self + 0x10u, 0u);
}

void func_08B4AB98(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 local = ctx.gpr[29];
    put(rt, local, word(rt, source));
    put(rt, local + 0x4u, word(rt, source + 0x4u));
    put(rt, local + 0x8u, word(rt, source + 0x8u));
    call(rt, ctx, 0x08B4AB04u, {a0, a1, local});
}

void func_08B4BC24(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4BB74u, 0x08B4B858u); }
void func_08B4BC58(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4BB74u, 0x08B4B864u); }
void func_08B4BC8C(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4BB00u, 0x08B4B858u); }
void func_08B4C188(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C0F8u, 0x08B4B858u); }
void func_08B4C1BC(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C0F8u, 0x08B4B864u); }
void func_08B4C27C(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C1F0u, 0x08B4B858u); }
void func_08B4C2B0(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C1F0u, 0x08B4B864u); }
void func_08B4C374(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C2E4u, 0x08B4B858u); }
void func_08B4C3A8(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C2E4u, 0x08B4B864u); }
void func_08B4C468(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C3DCu, 0x08B4B858u); }
void func_08B4C49C(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C3DCu, 0x08B4B864u); }
void func_08B4C560(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C4D0u, 0x08B4B858u); }
void func_08B4C594(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C4D0u, 0x08B4B864u); }
void func_08B4C654(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C5C8u, 0x08B4B858u); }
void func_08B4C688(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C5C8u, 0x08B4B864u); }
void func_08B4C79C(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C6BCu, 0x08B4B858u); }
void func_08B4C7D0(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C6BCu, 0x08B4B864u); }
void func_08B4C918(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C804u, 0x08B4B864u); }
void func_08B4CA60(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4C94Cu, 0x08B4B864u); }
void func_08B4CBA8(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4CA94u, 0x08B4B864u); }
void func_08B4CCF0(Runtime &rt, AllegrexContext &ctx) { local_pair(rt, ctx, 0x08B4CBDCu, 0x08B4B864u); }

// *out = 08BEEC3C(a1, a2, a3); v0 = the result.
void func_08BEECE8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = out;
    const u32 result = call(rt, ctx, 0x08BEEC3Cu, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, out, result);
}

void func_08A64668(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A644FCu, {self, 0x1Cu, second});
    put(rt, self, 0x08CD2CF0u);
    ctx.gpr[2] = 0x08CD2CF0u;
}

} // namespace
} // namespace new_batch_main_1113
} // namespace gt::game

#define NEW1113(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1113::name, gt::ReturnKind::kind)
NEW1113(0x08B247B0u, func_08B247B0, Word);
NEW1113(0x08B247E4u, func_08B247E4, Word);
NEW1113(0x08B24818u, func_08B24818, Word);
NEW1113(0x08B27F74u, func_08B27F74, Word);
NEW1113(0x08B32EF8u, func_08B32EF8, Word);
NEW1113(0x08B331E4u, func_08B331E4, Word);
NEW1113(0x08B4AB98u, func_08B4AB98, Word);
NEW1113(0x08B4BC24u, func_08B4BC24, Word);
NEW1113(0x08B4BC58u, func_08B4BC58, Word);
NEW1113(0x08B4BC8Cu, func_08B4BC8C, Word);
NEW1113(0x08B4C188u, func_08B4C188, Word);
NEW1113(0x08B4C1BCu, func_08B4C1BC, Word);
NEW1113(0x08B4C27Cu, func_08B4C27C, Word);
NEW1113(0x08B4C2B0u, func_08B4C2B0, Word);
NEW1113(0x08B4C374u, func_08B4C374, Word);
NEW1113(0x08B4C3A8u, func_08B4C3A8, Word);
NEW1113(0x08B4C468u, func_08B4C468, Word);
NEW1113(0x08B4C49Cu, func_08B4C49C, Word);
NEW1113(0x08B4C560u, func_08B4C560, Word);
NEW1113(0x08B4C594u, func_08B4C594, Word);
NEW1113(0x08B4C654u, func_08B4C654, Word);
NEW1113(0x08B4C688u, func_08B4C688, Word);
NEW1113(0x08B4C79Cu, func_08B4C79C, Word);
NEW1113(0x08B4C7D0u, func_08B4C7D0, Word);
NEW1113(0x08B4C918u, func_08B4C918, Word);
NEW1113(0x08B4CA60u, func_08B4CA60, Word);
NEW1113(0x08B4CBA8u, func_08B4CBA8, Word);
NEW1113(0x08B4CCF0u, func_08B4CCF0, Word);
NEW1113(0x08BEECE8u, func_08BEECE8, Word);
NEW1113(0x08A64668u, func_08A64668, Word);
