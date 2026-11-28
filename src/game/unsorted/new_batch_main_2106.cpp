// Main-session batch 2106 (src/game/unsorted/new_batch_main_2106.cpp): structured C from tools.disasm, drafted, unverified.
// Redraft of 0x08AB4950 (old draft cf531f5b, batch 2082): the gate saw the record block stored at self's list
// link wrongly. The jal at 0x08AB4994 has `addu $s0,$v0,$zero` in its delay slot, so s0 takes the block (the
// 0x08A5BC20 result), not the 0x08AB481C result, before the call; s0 is what 0x08AB49A4 stores at sp+8.
// Skipped (not drafted, too large to verify by hand this batch): 0x088BEC3C, 0x088DD144, 0x0890703C.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2106 {
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

// Saves the caller's callee-saved registers selected by `mask` (bit n = s_n) and restores them on scope exit.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, u32 mask) : ctx_(ctx), mask_(mask) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i)
            if (mask_ & (1u << i)) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 mask_;
    u32 saved_[8];
};

// 0x08AB4950: a key record filled by 0x08B85EB8, a 0x28-byte block built by 0x08AB481C, the object's first word compared
// with 0x08D74FD0; the release through 0x08B8542C runs when 0x08B60804 does not answer positive.
void func_08AB4950(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 7u);  // s0, s1, s2
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08C902BCu, frame.at(1u)});
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x28u, 4u});
    // The delay slot of the 0x08AB481C call runs first: s0 = block, and that is what is stored at sp+8.
    call(rt, ctx, 0x08AB481Cu, {block, frame.at(4u)});
    put(rt, frame.at(8u), block);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(8u)});
    const u32 first = word(rt, frame.at(4u));
    const u32 owner = first - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    if (owner != 0x08D74FD0u) {
        const s32 answer = static_cast<s32>(call(rt, ctx, 0x08B60804u, {first - 4u, 0xFFFFFFFFu}));
        if (answer <= 0) call(rt, ctx, 0x08B8542Cu, {owner, frame.at(0u)});
    }
}

}  // namespace

#define NEW2106(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2106::name, gt::ReturnKind::kind)
NEW2106(0x08AB4950u, func_08AB4950, Word);

}  // namespace
}  // namespace gt::game
