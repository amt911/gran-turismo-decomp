// Main-session batch 2184: one function re-drafted from the asm after its batch-2079 gate mismatch (fix_info.txt:
// v0: original 0x08AC8918, decompiled 0x000A0000). Two documented gotchas are why it mismatched, both in FINDINGS:
// a lone `lui` leaves high_half(address) in the register, not the pre-relocation immediate the disassembly shows
// (2026-10-31), and a virtual that never writes v0 returns the target it was called with, so the call has to start
// with v0 = that target (hot_common's call_indirect). Shapes: none — the only address it dereferences is the global
// list head 0x08D7717C, and a global is not expressible as a synthetic shape. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2184 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A lone `lui` holds the high half of the relocated address, `(address + 0x8000) & 0xFFFF0000`, not the address minus
// the immediate shown for it (FINDINGS 2026-10-31: the base's 0x4000 hides in the pair).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// 0x08AB994C: walks the global list at 0x08D7717C (a node: next at +0, object at +4) and calls the virtual at slot
// +0x34 of each object's vtable with the object in a0. v0 is the last call's result; an empty list leaves the `lui`'s
// own value, high_half(0x08D7717C) = 0x08D70000.
void func_08AB994C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = high_half(0x08D7717Cu);
    for (u32 node = word(rt, 0x08D7717Cu); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0x34u), {object});
    }
}

} // namespace
} // namespace new_batch_main_2184
} // namespace gt::game

#define NEW2184(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2184::name, gt::ReturnKind::kind)
NEW2184(0x08AB994Cu, func_08AB994C, Word);
