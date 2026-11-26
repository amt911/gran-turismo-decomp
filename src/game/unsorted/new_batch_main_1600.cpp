// New batch, main session 1600: one function redrafted from main-session batch 42 after the gate's first mismatch
// (0x088FD508: v0 on the path where a2 (the other pair) is null). Drafted, not verified: no gate ran on it here.
//
// Written (registered at the end of this file): func_088FD508 (installs a counted pair and a delegate).
// The callee-saved registers, the ll / sc counters and the guest calls are the same helpers batch 42 uses (one thread:
// a plain load and store; the sc's success flag is 1 in v0).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1600 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: v0 / v1 / f0 of the callee stay in the registers as after the original's `jal`, and a callee that never writes
// v0 hands the caller's own v0 on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments, {}).v0;
}

// The callee-saved registers s0-s7 / fp as they were at entry.
class SavedRegs {
public:
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 saved_fp_;
};

// The ll / sc counters (one thread: a plain load and store). A decrement leaves the old value in a0, v0 = 1 (the sc's success
// flag) and v1 = the address.
u32 atomic_decrement(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }

u32 atomic_increment(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
    return 1u;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088FD508 (a0 = self, a1 = source, a2 = other): installs a new counted pair. 0x089624FC(a1, a1, a2) decides first (zero returns
// 0 at once). With a null other the answer is 0 (v0 is the zero of the delay slot, not the first call's answer).
} // namespace
} // namespace new_batch_main_1600
} // namespace gt::game

#define NEW1600(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1600::name, gt::ReturnKind::kind)
