// New batch main-1625: 3 drafted functions (redrafted after the gate's mismatches), written by hand from `tools/disasm`.
// Structured C: named locals, guest memory through the helpers, a call only at the original's `jal` with the argument registers the
// asm sets (and the caller's a1 where the asm leaves it alone: the callee reads whatever a1 holds). v0 ends as every exit path
// of the asm leaves it. The s-registers are not written by these bodies, so the caller's values come back as they are.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1625 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that takes floats in $f12...; $f0 comes back and is kept, as the asm leaves it.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A registry table at word(owner): {word +4 = entries, word +8 = count}. The entry at `index`, or 0 when out of range or empty.
u32 table_entry(Runtime &rt, u32 owner, u32 index) {
    const u32 table = word(rt, owner);
    if (index >= word(rt, table + 8u)) return 0u;
    return word(rt, word(rt, table + 4u) + index * 4u);
}

// --- constructor of one object (a0 = this) -----------------------------------------------------------------------------
// 0x08C77004 and 0x08C771D4 hold the same body (checked by diffing their disassembly): two vtable words are stored in turn, the
// members at +0x1C and +0x24 are set up (0x08B5E0EC, 0x08B61528), the fields from +0x2C to +0xA0 are cleared with a size word 0x400
// at +0x7C, and then the name 0x08D721C8 is looked up (0x08B5C2B0) in the table at word(this + 0x1C). A found entry is kept at +0x90.
// A name the table does not hold is looked up a second time; if that fails too, the error routine 0x08B5A9FC takes the index (it
// never returns: no epilogue follows its jal in the original).
void construct_with_lookup(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CDBDF0u);
    for (u32 offset = 4u; offset <= 0x18u; offset += 4u) put(rt, self + offset, 0u);
    call(rt, ctx, 0x08B5E0ECu, {self + 0x1Cu, ctx.gpr[5]});
    put(rt, self, 0x08CDC298u);
    put(rt, self + 0x20u, 0u);
    call(rt, ctx, 0x08B61528u, {self + 0x24u, self + 0x20u});
    put(rt, self + 0x7Cu, 0x400u);
    for (u32 offset = 0x2Cu; offset <= 0x78u; offset += 4u) put(rt, self + offset, 0u);
    for (u32 offset = 0x80u; offset <= 0x83u; offset += 1u) rt.memory().store8(self + offset, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x88u, 0u);
    rt.memory().store8(self + 0x8Cu, 0u);
    for (u32 offset = 0x90u; offset <= 0x9Cu; offset += 4u) put(rt, self + offset, 0u);
    put(rt, self + 0xA0u, 0u);
    const u32 index = call(rt, ctx, 0x08B5C2B0u, {0x08D721C8u, ctx.gpr[5]});
    const u32 table = word(rt, self + 0x1Cu);
    if (index >= word(rt, table + 8u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 slot = word(rt, table + 4u) + index * 4u;
    if (word(rt, slot) == 0u) {
        ctx.gpr[2] = slot;
        return;
    }
    const u32 index_again = call(rt, ctx, 0x08B5C2B0u, {0x08D721C8u, ctx.gpr[5]});
    const u32 value = table_entry(rt, self + 0x1Cu, index_again);
    if (value != 0u) {
        put(rt, self + 0x90u, value);
        ctx.gpr[2] = value;
        return;
    }
    call(rt, ctx, 0x08B5A9FCu, {index_again, ctx.gpr[5]});
}
void func_08C77004(Runtime &rt, AllegrexContext &ctx) { construct_with_lookup(rt, ctx); }
void func_08C771D4(Runtime &rt, AllegrexContext &ctx) { construct_with_lookup(rt, ctx); }

// --- reading the registry values of one object (a0 = this, a1 = the registry owner) ------------------------------------
// 0x08C548C4: the name 0x08D721D4 (found: 0x08B64C80 of the owner is kept at +0x7C; else 0), then 0x08D7747C at +0x80 and
// 0x08D77480 at +0x84. A name with no entry stores 0; a name that is found but whose second lookup fails goes to 0x08B5A9FC.
void func_08C548C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    u32 value = 0u;
    if (table_entry(rt, owner, call(rt, ctx, 0x08B5C2B0u, {0x08D721D4u, ctx.gpr[5]})) != 0u) {
        value = call(rt, ctx, 0x08B64C80u, {owner, ctx.gpr[5]});
    }
    put(rt, self + 0x7Cu, value);

    // +0x80: a found entry is looked up once more; the second answer is the one kept.
    const u32 first80 = call(rt, ctx, 0x08B5C2B0u, {0x08D7747Cu, ctx.gpr[5]});
    if (table_entry(rt, owner, first80) == 0u) {
        put(rt, self + 0x80u, 0u);
        ctx.gpr[2] = 0u;
    } else {
        const u32 again80 = call(rt, ctx, 0x08B5C2B0u, {0x08D7747Cu, ctx.gpr[5]});
        const u32 entry80 = table_entry(rt, owner, again80);
        if (entry80 == 0u) {
            call(rt, ctx, 0x08B5A9FCu, {again80, ctx.gpr[5]});
            return;
        }
        put(rt, self + 0x80u, entry80);
        ctx.gpr[2] = entry80;
    }

    // +0x84: the same shape; the error routine takes the callee's leftover a0 (the name's lookup does not set it again).
    call(rt, ctx, 0x08B5C2B0u, {0x08D77480u, ctx.gpr[5]});
    const u32 first84 = ctx.gpr[2];
    if (table_entry(rt, owner, first84) == 0u) {
        put(rt, self + 0x84u, 0u);
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B5C2B0u, {0x08D77480u, ctx.gpr[5]});
    const u32 entry84 = table_entry(rt, owner, ctx.gpr[2]);
    if (entry84 == 0u) {
        call(rt, ctx, 0x08B5A9FCu, {ctx.gpr[4], ctx.gpr[5]});
        return;
    }
    put(rt, self + 0x84u, entry84);
    ctx.gpr[2] = entry84;
}

// --- the init/shutdown of the global object at 0x08D405D0 -------------------------------------------------------------
// Flag word F = 0x08D405D0 (the first word), G = 0x08CDFB3C (flag bits), 0x08CDFB40 the object for the error call.

} // namespace
} // namespace new_batch_main_1625
} // namespace gt::game

#define NEWMAIN1625(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1625::name, gt::ReturnKind::kind)

NEWMAIN1625(0x08C77004u, func_08C77004, Word);
NEWMAIN1625(0x08C771D4u, func_08C771D4, Word);
NEWMAIN1625(0x08C548C4u, func_08C548C4, Word);
