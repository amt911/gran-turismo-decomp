// New batch main-2092 (main-session batch, draft only, unverified). Structured C from tools.disasm.
// Each function: every argument register the callee reads is passed, delay-slot stores run before the call, v0 is the last call's result.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2092 {
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

// 0x08A2B6F4 (a0 = out object): a lookup 0x08B32138 that misses returns 0 at once; otherwise a few destructor-shaped calls, and v0 is the
// last call's result.

// 0x08A9C998 (a1 = out pointer slot, a2 = source): builds a local record, looks up a name and, when the answer is a record, calls its
// vtable slot 0x11C; v0 is the last call's result.
void func_08A9C998(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 sp = ctx.gpr[29];
    const u32 out = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 local = sp + 0x0Cu;
    const u32 holder = sp + 0x18u;
    const u32 slot = sp + 0x10u;
    call(rt, ctx, 0x08A6B5E0u, {local, source});
    call(rt, ctx, 0x08AA6740u, {holder, local});
    call(rt, ctx, 0x08AA61D8u, {holder, slot, 8u});
    const u32 tested = call(rt, ctx, 0x08B7C6BCu, {slot, 0x08CB2668u, 4u});
    if (tested == 0u) {
        const u32 count = call(rt, ctx, 0x08B7DF7Cu, {sp + 0x14u, 0u, 0x0Au});
        if (static_cast<s32>(count) >= 9) call(rt, ctx, 0x08AA6B80u, {holder});
        const u32 block = call(rt, ctx, 0x08A5BC20u, {0x34u, 4u});
        const u32 made = call(rt, ctx, 0x08A90FE4u, {block, count});
        put(rt, sp + 4u, made);
        call(rt, ctx, 0x08A76AB4u, {sp + 8u, sp + 4u});
        if (out != sp + 8u) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, sp + 8u)});
        call(rt, ctx, 0x08A5B9ECu, {sp + 8u});
    } else {
        rt.memory().store8(sp + 0x14u, 0);
    }
    // Both paths: s0 = the word at sp+0xC; the record it names is passed to the next call (0 when the word is 0 after the assert).
    u32 record = word(rt, sp + 0x0Cu);
    if (tested == 0u) {
        const u32 current = word(rt, out);
        if (current == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2588u});
        call(rt, ctx, 0x08A9109Cu, {current, holder});
        record = word(rt, sp + 0x0Cu);
    }
    if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2670u});
    const u32 vtable = word(rt, record);
    const u32 target = word(rt, vtable + 0x11Cu);
    call_indirect(rt, ctx, target, {record});
    const u32 begin = word(rt, sp + 0x34u);
    call(rt, ctx, 0x08C2FBF8u, {begin, word(rt, sp + 0x38u), sp});
    const u32 begin2 = word(rt, sp + 0x34u);
    if (begin2 != 0u) {
        const s32 span = static_cast<s32>(word(rt, sp + 0x3Cu) - begin2) >> 2;
        call(rt, ctx, 0x08A5BC54u, {begin2, static_cast<u32>(span) << 2, 4u});
    }
    call(rt, ctx, 0x08C0DD50u, {sp + 0x1Cu, word(rt, sp + 0x24u)});
    call(rt, ctx, 0x08A5B9ECu, {holder});
    call(rt, ctx, 0x08A5B9ECu, {local});
}

// 0x08C7A274 (a0 = destination record, a1 = source record, a2/a3 = flags): copies the links of the source into the destination, then
// calls the two rebuild helpers and returns the last call's result.

// 0x08A8EA78 (a0 = object being built): stores the vtable word, rebuilds the member lists and returns the last call's result (0x08A780B0).

} // namespace
} // namespace new_batch_main_2092
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2092::name, gt::ReturnKind::kind)
NEWM(0x08A9C998u, func_08A9C998, Word);
