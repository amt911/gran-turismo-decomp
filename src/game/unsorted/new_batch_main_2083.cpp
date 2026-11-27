// Main-session batch 2083 (draft, unverified). Structured C from tools.disasm; re-drafted functions the gate had rejected
// or skipped are NOT included here (float, 64-bit return or alloca frames: left for a later pass).
// Every function: v0 as the asm on every exit, delay slots before the call, GuestFrame = the prologue's addiu sp.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2083 {
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
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// 088B1254: constructor of a two-part record. a0 = the object, a1 and a2 = two sources; each source's word at +4 is a
// counted pointer (its count at +4 is bumped). v0 ends as the vtable word 0x08D752A4.
void func_088B1254(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AC0640u, {self, first});
    put(rt, self + 0x30u, 0x088B0C18u);
    put(rt, self + 0x00u, 0x08CC2FC0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, word(rt, first));
    const u32 first_ref = word(rt, first + 4u);
    put(rt, self + 0x3Cu, first_ref);
    if (first_ref != 0u) {
        put(rt, first_ref + 4u, word(rt, first_ref + 4u) + 1u);
    }
    put(rt, self + 0x48u, word(rt, second));
    const u32 second_ref = word(rt, second + 4u);
    put(rt, self + 0x4Cu, second_ref);
    if (second_ref != 0u) {
        put(rt, second_ref + 4u, word(rt, second_ref + 4u) + 1u);
    }
    put(rt, self + 0x58u, 0x08D752A4u);
    put(rt, self + 0x5Cu, 0u);
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    ctx.gpr[2] = 0x08D752A4u;
}

// 0886AC1C: constructor. a0 = this, a1 = a name string (may be null), a2 = a value stored at +0x1C. v0 = the last call's answer.
void func_0886AC1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CBF710u);
    call(rt, ctx, 0x08B93A24u, {self + 4u, self});
    put(rt, self + 0x1Cu, value);
    rt.memory().store8(self + 0x20u, 0u);
    call(rt, ctx, 0x0887C7A0u, {self + 0x64u, self});
    for (u32 i = 0; i < 3u; ++i) {
        put(rt, self + 0x10u + 4u * i, 0u);
    }
    for (u32 i = 0; i < 8u; ++i) {
        put(rt, self + 0x24u + 4u * i, 0u);
    }
    if (name != 0u && rt.memory().load8(name) != 0u) {
        call(rt, ctx, 0x08B7C830u, {self + 0x44u, name, 0x1Fu});
        rt.memory().store8(self + 0x63u, 0u);
        return;
    }
    const u32 global_count = word(rt, 0x08CE2E28u);
    call(rt, ctx, 0x08B37640u, {self + 0x44u, 0x08C9F65Cu, global_count});
    put(rt, 0x08CE2E28u, word(rt, 0x08CE2E28u) + 1u);
}

// Shared body of 0888A6AC and 0888A78C: allocates the 0x120-byte block, fills its tables, stores it at *a0 and returns the
// static object at 0x08D75E80 (initialised the first time through the guard at 0x08D75E70).
void build_block_0888(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 obj = call(rt, ctx, 0x08B62344u, {0x120u});
    put(rt, obj, 0x08CC1A10u);
    put(rt, obj + 4u, 0u);
    call(rt, ctx, 0x0888A600u, {obj + 8u});
    for (u32 i = 0; i < 3u; ++i) {
        const u32 base = obj + 0x10u + 0x50u * i;
        put(rt, base + 0x40u, 0u);
        put(rt, base + 0x44u, 0u);
        put(rt, base + 0x48u, 0u);
    }
    call(rt, ctx, 0x08B1C2BCu, {obj + 0x100u});
    put(rt, obj + 0x110u, 0u);
    put(rt, obj + 0x114u, 0u);
    put(rt, obj + 0x118u, 0u);
    put(rt, obj + 0x11Cu, 0u);
    put(rt, out, obj);
    const u32 guard = 0x08D75E70u;
    bool initialise = false;
    if (rt.memory().load8(guard) == 0u) {
        initialise = call(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
    }
    if (initialise) {
        put(rt, 0x08D75E80u, 0x08CC1B18u);
        call(rt, ctx, 0x08B64494u, {guard});
    } else {
        ctx.gpr[2] = 0x000A0000u;
    }
    put(rt, out + 4u, 0x08D75E80u);
}

void func_0888A6AC(Runtime &rt, AllegrexContext &ctx) { build_block_0888(rt, ctx); }
void func_0888A78C(Runtime &rt, AllegrexContext &ctx) { build_block_0888(rt, ctx); }

// 08895C9C: constructor of a record with two sub-objects and a shared table at 0x08D75EE0 (guard byte 0x08D75ED8).
// Returns the value stored at +0x28 of the record.
void func_08895C9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 obj = call(rt, ctx, 0x08B62344u, {0x44u});
    call(rt, ctx, 0x0892B440u, {obj});
    call(rt, ctx, 0x0892B440u, {obj + 0x20u});
    put(rt, obj + 0x40u, 0u);
    put(rt, self + 0x1Cu, obj);
    const u32 guard = 0x08D75ED8u;
    const u32 table = 0x08D75EE0u;
    bool initialise = false;
    if (rt.memory().load8(guard) == 0u) {
        initialise = call(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
    }
    if (initialise) {
        put(rt, table, 0x08CC1F30u);
        call(rt, ctx, 0x08B64494u, {guard});
    }
    put(rt, self + 0x20u, table);
    const u32 sub = self + 0x28u;
    call(rt, ctx, 0x08B1C920u, {sub});
    const u32 answer = word(rt, self + 0x28u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x04u, 0u);
    put(rt, self + 0x00u, 0u);
    put(rt, self + 0x08u, 0u);
    put(rt, self + 0x0Cu, 0u);
    put(rt, self + 0x24u, 0u);
    rt.memory().store8(self + 0x18u, 0u);
    put(rt, sub + 4u, answer);
    put(rt, sub + 0x0Cu, 0u);
    rt.memory().store8(sub + 8u, 0u);
    ctx.gpr[2] = answer;
}

// 08ACF920: constructor of a large record: vtable, two zeroed tables, sub-objects initialised by three helpers.
void func_08ACF920(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08ABCEB4u, {self});
    put(rt, self, 0x08CD6B38u);
    u32 cursor = self + 0x9Cu;
    for (u32 pass = 0; pass < 2u; ++pass) {
        for (u32 k = 0; k < 5u; ++k) {
            put(rt, cursor - 0x10u + 4u * k, 0u);
        }
        cursor += 0x18u;
    }
    call(rt, ctx, 0x08B1AFB8u, {self + 0xB8u});
    const u32 block = self + 0xFCu;
    call(rt, ctx, 0x08AFB4B4u, {block});
    call(rt, ctx, 0x08AFB4B4u, {block + 0xD4u});
    const u32 block2 = block + 0x1A8u;
    call(rt, ctx, 0x08AFB4B4u, {block2});
    call(rt, ctx, 0x08AFB4B4u, {block2 + 0xD4u});
    call(rt, ctx, 0x08AFB4B4u, {block + 0x350u});
    call(rt, ctx, 0x08B1BC98u, {self + 0x528u});
    call(rt, ctx, 0x08B1BC98u, {self + 0x5F8u});
    put(rt, self + 0x0Cu, 0x8000u);
    put(rt, self + 0xF0u, 0x3E80u);
    put(rt, self + 0xF4u, 0u);
    put(rt, self + 0xF8u, 0u);
    ctx.gpr[2] = 0x3E80u;
}

// 0886E888: record set-up. a0 = this; allocates a 0x3C-byte object, stores it at +0x70 and returns a tail call
// 0886AAA4(this, object). The static pair at 0x08D752E0 is initialised once (guard byte at 0x08D752E0).
void func_0886E888(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 object = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x0886AD04u, {self});
        put(rt, self, 0x08CBF770u);
        object = call(rt, ctx, 0x08B62344u, {0x3Cu});
        call(rt, ctx, 0x08871908u, {object});
        call(rt, ctx, 0x0886FC04u, {object + 0x14u});
        call(rt, ctx, 0x088744FCu, {object + 0x28u});
        call(rt, ctx, 0x0886F8B4u, {object + 0x14u, 1u});
        put(rt, self + 0x70u, object);
        const u32 guard = 0x08D752E0u;
        if (rt.memory().load8(guard) == 0u) {
            if (call(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                put(rt, 0x08D752E8u, 0x08CBF7A8u);
                call(rt, ctx, 0x08B64494u, {guard});
            }
        }
        put(rt, self + 0x74u, 0x08D752E8u);
    }
    tail_call(ctx, 0x0886AAA4u, {self, object});
}

// 08917D38: a0 = the owner (its +0xC is a state block with flags at +0x1FC7, +0x1FCD, +0x1FCC), a1 = the out record.
// Returns the state block pointer, or the early answer of the flag check.
void func_08917D38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 state0 = word(rt, self + 0xCu);
    if (rt.memory().load8(state0 + 0x1FC7u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 mode = rt.memory().load8(state0 + 0x1FCDu);
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 record = call(rt, ctx, 0x089277A4u, {state0, 3u});
    if (rt.memory().load8(record + 0xC0u) == 0u) {
        ctx.gpr[2] = record;
        return;
    }
    const u32 first = call(rt, ctx, 0x089277BCu, {word(rt, self + 0xCu)});
    if (first == 3u) {
        const u32 made = call(rt, ctx, 0x08917C2Cu, {self, out, 0u, 0x1Au});
        put(rt, out + 8u, made);
        rt.memory().store8(word(rt, self + 0xCu) + 0x1FCCu, mode);
    } else {
        const u32 second = call(rt, ctx, 0x089277BCu, {word(rt, self + 0xCu)});
        if (second == 0u) {
            const u32 made = call(rt, ctx, 0x08917C2Cu, {self, out, 3u, 0u});
            put(rt, out + 8u, made);
            rt.memory().store8(word(rt, self + 0xCu) + 0x1FCCu, 0u);
        }
    }
    call(rt, ctx, 0x089274C8u, {word(rt, self + 0xCu), out});
    rt.memory().store8(word(rt, self + 0xCu) + 0x1FCDu, 0u);
    ctx.gpr[2] = word(rt, self + 0xCu);
}

// 08A6397C: destructor of a record with a vector at +0x24 (begin) / +0x28 (end): each element's virtual slots 0xC and 4 are
// called in turn, then the vector is shrunk and its buffers freed. Returns the answer of the last helper.

} // namespace
} // namespace new_batch_main_2083
} // namespace gt::game

#define NEW2083(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2083::name, gt::ReturnKind::kind)
NEW2083(0x088B1254u, func_088B1254, Word);
NEW2083(0x0886AC1Cu, func_0886AC1C, Word);
NEW2083(0x0888A6ACu, func_0888A6AC, Word);
NEW2083(0x0888A78Cu, func_0888A78C, Word);
NEW2083(0x08895C9Cu, func_08895C9C, Word);
NEW2083(0x08ACF920u, func_08ACF920, Word);
NEW2083(0x0886E888u, func_0886E888, Word);
NEW2083(0x08917D38u, func_08917D38, Word);
