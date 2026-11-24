// New batch main-115 (draft by a delegate, unverified: the gate runs after). Structured C from `tools.disasm`.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_115 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// Words at base + from, base + from + 4 ... base + to (inclusive) are set to zero.
void zero_words(Runtime &rt, u32 base, u32 from, u32 to) {
    for (u32 offset = from; offset <= to; offset += 4u) put(rt, base + offset, 0u);
}

// 0x08B5D788 (a0 = registry object, a1 = its first word): fills two 14-entry tables, builds the static objects and registers each
// with 0x08B5D3DC(object, name, entry); finally four names are looked up with 0x08B5C2B0 and their slots in the table at +0xC set.
void func_08B5D788(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 table_b = 0x08D72644u;
    const u32 table_a = 0x08D7267Cu;
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 8u, 14u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 4u, table_a);
    put(rt, self + 0xCu, table_b);
    for (u32 index = 0; index < 14u; ++index) {
        put(rt, table_b + index * 4u, 0u);
        put(rt, table_a + index * 4u, 0u);
    }
    put(rt, self + 0x10u, 0x08D726C0u);
    // a0 = 0 and a1 = 14 as the loop above leaves them.
    const u32 defaults = call(rt, ctx, 0x08B5C274u, {0u, 14u, table_b, table_a});
    put(rt, 0x08D726C0u, 0x08D726B4u);
    put8(rt, 0x08D726B4u, rt.memory().load8(defaults));
    put8(rt, 0x08D726B4u + 1u, rt.memory().load8(defaults + 1u));
    for (u32 offset = 4u; offset != 0x18u; offset += 4u) put(rt, word(rt, self + 0x10u) + offset, 0u);

    call(rt, ctx, 0x08B64CD0u, {0x08D72424u, 0u, 0u, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D721D4u, 0x08D72424u});
    call(rt, ctx, 0x08B647C0u, {0x08D723F0u, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D721C8u, 0x08D723F0u});

    const u32 slot_a = 0x08D7232Cu;
    const u32 slot_b = 0x08D7240Cu;
    put(rt, slot_a, 0x08CDBAD8u);
    put(rt, slot_b + 4u, 1u);
    put(rt, slot_b + 8u, slot_a);
    put(rt, slot_a + 4u, 1u);
    put(rt, slot_a + 8u, 0u);
    put(rt, slot_a + 0xCu, 0u);
    put8(rt, slot_a + 0x10u, 0u);
    zero_words(rt, slot_a, 0x14u, 0x20u);
    put8(rt, slot_a + 0x24u, 0u);
    put8(rt, slot_a + 0x25u, 0u);
    put8(rt, slot_a + 0x64u, 0u);
    put(rt, slot_b, 0x08CDB798u);
    call(rt, ctx, 0x08B60DB8u, {slot_b, 0u, slot_b, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77484u, slot_b});

    put(rt, 0x08D72404u + 4u, 1u);
    put(rt, 0x08D72404u, 0x08CDB958u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77480u, 0x08D72404u});
    put(rt, 0x08D723FCu + 4u, 1u);
    put(rt, 0x08D723FCu, 0x08CDB998u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7747Cu, 0x08D723FCu});

    const u32 entry_c = 0x08D72418u;
    put(rt, entry_c + 4u, 1u);
    put(rt, entry_c, 0x08CDB7C0u);
    const u32 made = call(rt, ctx, 0x08B5C230u, {self, 0x08D7747Cu, 0x08D723FCu, 1u});
    put(rt, entry_c + 8u, made);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77468u, entry_c});

    // Two objects of one shape: a base, a holder pointing at it, a constructor call, then the registration.
    const u32 shaped[2][5] = {
        {0x08D722A4u, 0x08D723D8u, 0x08CDBAB8u, 0x08CDB838u, 0x08B60910u},
        {0x08D722E8u, 0x08D723E4u, 0x08CDBAC8u, 0x08CDB800u, 0x08B60C98u},
    };
    const u32 names[2] = {0x08D77494u, 0x08D77490u};
    for (int which = 0; which < 2; ++which) {
        const u32 base = shaped[which][0];
        const u32 holder = shaped[which][1];
        put(rt, base, shaped[which][2]);
        put(rt, holder + 4u, 1u);
        put(rt, holder + 8u, base);
        put(rt, base + 4u, 1u);
        put(rt, base + 8u, 0u);
        put(rt, base + 0xCu, 0u);
        put8(rt, base + 0x10u, 0u);
        put8(rt, base + 0x11u, 0u);
        put8(rt, base + 0x12u, 0u);
        zero_words(rt, base, 0x14u, 0x34u);
        put8(rt, base + 0x43u, 0u);
        put(rt, holder, shaped[which][3]);
        call(rt, ctx, shaped[which][4], {holder, 0u, 0u});
        call(rt, ctx, 0x08B5D3DCu, {self, names[which], holder});
    }

    put(rt, 0x08D723D0u + 4u, 1u);
    put(rt, 0x08D723D0u, 0x08CDB900u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7748Cu, 0x08D723D0u});
    put(rt, 0x08D723C8u + 4u, 1u);
    put(rt, 0x08D723C8u, 0x08CDB918u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77488u, 0x08D723C8u});

    const u32 big = 0x08D721DCu;
    put(rt, big + 4u, 1u);
    put(rt, big, 0x08CDB7E0u);
    zero_words(rt, big, 0x8u, 0xC0u);
    put8(rt, big + 0xC4u, 0u);
    call(rt, ctx, 0x08C5DC90u, {0x08D723B4u, big, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77478u, 0x08D723B4u});

    put(rt, 0x08D723ACu + 4u, 1u);
    put(rt, 0x08D723ACu, 0x08CDB9F8u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77470u, 0x08D723ACu});
    put(rt, 0x08D723A4u + 4u, 1u);
    put(rt, 0x08D723A4u, 0x08CDB9C8u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77474u, 0x08D723A4u});
    call(rt, ctx, 0x08C5E298u, {0x08D72394u, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7746Cu, 0x08D72394u});

    // s0 = the table pointer at +0xC is read in each call's delay slot, before the call.
    const struct { u32 name; u32 object; } registered[4] = {
        {0x08D77484u, 0x08D7232Cu}, {0x08D77494u, 0x08D722A4u}, {0x08D77490u, 0x08D722E8u}, {0x08D77478u, 0x08D721DCu},
    };
    for (const auto &entry : registered) {
        const u32 table = word(rt, self + 0xCu);
        const u32 index = call(rt, ctx, 0x08B5C2B0u, {entry.name});
        ctx.gpr[2] = (index << 2) + table;
        put(rt, ctx.gpr[2], entry.object);
    }
}

} // namespace
} // namespace new_batch_main_115
} // namespace gt::game

#define NEWM115(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_115::name, gt::ReturnKind::kind)
NEWM115(0x08B5D788u, func_08B5D788, Word);
