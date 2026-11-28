// New batch main 2094 (draft only): functions drafted before that mismatched the original (or were skipped twice), written
// by hand from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary). Every call passes the
// argument registers the original sets before the jal, the delay slot runs before the call, v0 is what the original leaves
// on every exit path, and callee-saved registers come back as they went in. Input shapes live in
// shapes_main2094.txt (config/synth_shapes format): one line per function that dereferences an argument.
// Not drafted here: 0x089335F0 (its calls read a0 left by the previous callee), 0x089C3E4C, 0x08A14D94, 0x08A5B070
// (virtual calls and long compare/copy loops, not checked enough), 0x08C45270 (reads globals only; no global shape syntax
// was found for it).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2094 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of the callee are what the next instruction sees, as after the original's `jal`.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, std::initializer_list<float>{});
    take(ctx, result);
    return result;
}
// A guest call with a0-a3 set like the original sets them before the jal (a callee can read a register its prototype
// does not name). Returns v0.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// A guest call with a0-a3 as they stand in the caller's registers at the jal.
inline u32 call_here(Runtime &rt, AllegrexContext &ctx, u32 address) {
    return call_set(rt, ctx, address, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}

// The callee-saved registers a body keeps values in; the caller's values come back on scope exit.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx) {
        for (const unsigned reg : registers) {
            indexes_[count_] = reg;
            values_[count_] = ctx.gpr[reg];
            ++count_;
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < count_; ++i) ctx_.gpr[indexes_[i]] = values_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned indexes_[8]{};
    u32 values_[8]{};
    unsigned count_{};
};

// The original copies a stack record with `lw`/`sw` in 4-word blocks and a 3-word tail: the bytes are the same as a
// straight word copy (its blocks and its tail never overlap the source).
inline void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 at = 0u; at < bytes; at += 4u) put(rt, destination + at, word(rt, source + at));
}
// `lwl`/`lwr` and `swl`/`swr` pairs: an unaligned copy, byte for byte.
inline void copy_bytes(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 at = 0u; at < bytes; ++at) rt.memory().store8(destination + at, rt.memory().load8(source + at));
}

// 0x088865C8 (a0 = object): constructor — a vtable at +0x50 over a 0xEC block, a second vtable at +0, a 0x10 node handed to
// 0x08B8E380, the 3.0f / 1.0f / 1/60 fields, a 0x20-word zero block at +0x198, sixteen 6-byte records at +0x2B8, three
// container initialisers and four zeroed words. v0 ends as 2 (the last clear loop's counter).

// 0x089335F0 is not drafted (see the header).

// 0x0897B364 (a0 = object, a1 = a pointer stored at +0x14): constructor — two vtables, a self-linked pair at +0x18, two
// 0x58-byte tables copied from .rodata into +0x94 and +0xEC, the descriptor fields at +0x14A..+0x158, four one-argument
// initialisers at +0x19C..+0x1A8 and a block of zeroed/1.0f fields. v0 ends as 0xFFFFFFFF (the last `addiu v0,-1`).

// 0x08AB8104 (a0 = table object with its count at +0x1480, a1 = key, a2 = 32-byte record, a3 = 4-byte tag): a table of
// 32 entries of 0xA4 bytes; a match for `key` (0x08B35FD8 returns 0) gets the record at +0x80 and the tag at +0xA0,
// otherwise a new entry is written (0x08B7C830 fills its first 0x80 bytes) and the count grows. The record and the tag
// are copied byte for byte (the original uses `lwl`/`lwr`, so they may be unaligned). v0 = 1 when something was written,
// 0 when the table is full.

// 0x088A4A9C (a0 = object, 0x800 bytes): constructor — vtables at +0 and at the sub-object at +0x20, the 1400.0f field at
// +0x14, four 0x18-byte records at +0x5C, four 0x1C-byte records at +0xC4, four 0xC-byte records at +0x134, a container
// init (0x088AA504), a second (0x088AB254), two tables through 0x08961058 / 0x088A42BC and a final 0x08B36018 whose
// answer is v0.

#define NEW2094(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2094::name, gt::ReturnKind::kind)

}  // namespace
}  // namespace new_batch_main_2094
}  // namespace gt::game
