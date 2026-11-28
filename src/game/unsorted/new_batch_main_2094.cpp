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
void func_088865C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20, 21});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self + 0x50u, 0x08CC1798u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    const u32 inner = self + 0x50u;
    const u32 cleared = word(rt, inner + 0x14u) & ~3u;
    put(rt, inner + 0x10u, 0u);
    put(rt, inner + 0x14u, cleared);
    const u32 block = call_set(rt, ctx, 0x08B62344u, {0xECu});
    ctx.gpr[18] = block;
    put(rt, block + 0x00u, 0x08CC1800u);
    put(rt, block + 0x04u, 0u);
    put(rt, block + 0x08u, 0u);
    const u32 table = 0x08D75298u + 0xCu;
    put(rt, block + 0x0Cu, table);
    put(rt, block + 0x10u, table);
    // The original clears four words from +0x14 (two `sw` pairs in a 2-iteration loop), not two.
    for (u32 i = 0u; i < 2u; ++i) {
        put(rt, block + 0x14u + 8u * i, 0u);
        put(rt, block + 0x18u + 8u * i, 0u);
    }
    rt.memory().store8(block + 0x24u, 0u);
    put(rt, block + 0x00u, 0x08CC17C0u);
    rt.memory().store8(block + 0x25u, 0u);
    u32 bits = word(rt, block + 0x24u) & ~(1u << 16u);
    put(rt, block + 0x24u, bits);
    put(rt, self + 0x68u, block);
    const u32 node = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, node + 0x00u, 0x08CC1840u);
    put(rt, node + 0x04u, 1u);
    put(rt, node + 0x08u, 1u);
    put(rt, node + 0x0Cu, block);
    put(rt, self + 0x68u + 4u, node);
    call_set(rt, ctx, 0x08B8E380u, {self + 0x68u + 4u, block, block, 1u, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10],
                                     ctx.gpr[11]});
    put_float(rt, self + 0x160u, 3.0f);
    put_float(rt, self + 0x168u, std::bit_cast<float>(0x3C888889u));
    put_float(rt, self + 0x170u, 1.0f);
    u32 flags = word(rt, self + 0x78u);
    flags = (flags & ~((1u << 12u) - 1u)) | 0x1000u;
    flags &= ~(7u << 13u);
    put(rt, self + 0x78u, flags);
    put(rt, self + 0x188u, table);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x74u, 0u);
    rt.memory().store16(self + 0x7Au, 0u);
    rt.memory().store16(self + 0x9Au, 0u);
    put(rt, self + 0x15Cu, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x16Cu, 0u);
    put(rt, self + 0x184u, table);
    put(rt, self + 0x18Cu, 0u);
    put(rt, self + 0x190u, 0u);
    put(rt, self + 0x194u, 0u);
    for (u32 i = 0u; i < 0x20u; ++i) put(rt, self + 0x198u + 4u * i, 0u);
    for (u32 i = 0u; i < 0x10u; ++i) {
        const u32 record = self + 0x2B8u + 6u * i;
        rt.memory().store8(record, 0u);
        rt.memory().store16(record + 2u, static_cast<std::uint16_t>(0xFFFFFFFFu));
        rt.memory().store16(record + 4u, static_cast<std::uint16_t>(0xFFFFFFFFu));
    }
    call_set(rt, ctx, 0x08B4E648u, {self});
    call_set(rt, ctx, 0x08B945C4u, {self + 0x188u, 0x08C89A48u, 0u});
    call_set(rt, ctx, 0x08884DA0u, {self, 1u});
    put(rt, self + 0x174u, 0u);
    put(rt, self + 0x17Cu, 0u);
    put(rt, self + 0x174u + 4u, 0u);
    put(rt, self + 0x17Cu + 4u, 0u);
    ctx.gpr[2] = 2u;
}

// 0x089335F0 is not drafted (see the header).

// 0x0897B364 (a0 = object, a1 = a pointer stored at +0x14): constructor — two vtables, a self-linked pair at +0x18, two
// 0x58-byte tables copied from .rodata into +0x94 and +0xEC, the descriptor fields at +0x14A..+0x158, four one-argument
// initialisers at +0x19C..+0x1A8 and a block of zeroed/1.0f fields. v0 ends as 0xFFFFFFFF (the last `addiu v0,-1`).
void func_0897B364(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx, {16, 17, 18});
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = argument;
    call_set(rt, ctx, 0x08A76ACCu, {self});
    const u32 link = self + 0x18u;
    const u32 slot = self + 0x74u;
    put(rt, self + 0x00u, 0x08CBB740u);
    put(rt, self + 0x18u, link);
    put(rt, self + 0x14u, argument);
    put(rt, link + 4u, link);
    call_set(rt, ctx, 0x08B28DCCu, {self + 0x20u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x48u});
    put(rt, slot + 4u, 0u);
    put(rt, slot + 0u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, slot + 0x10u, 0u);
    rt.memory().store8(self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    copy_words(rt, self + 0x94u, 0x08C8BEACu, 0x50u + 8u);
    copy_words(rt, self + 0xECu, 0x08C8BE54u, 0x50u + 8u);
    rt.memory().store16(self + 0x14Au, rt.memory().load16(0x08C8BE50u));
    put(rt, self + 0x14Cu, word(rt, 0x08C8BE48u));
    rt.memory().store16(self + 0x154u, rt.memory().load16(0x08C8BE44u));
    put(rt, self + 0x150u, word(rt, 0x08C8BE48u + 4u));
    put(rt, self + 0x158u, word(rt, 0x08C8BE40u));
    put(rt, self + 0x168u, 0x0Au);
    rt.memory().store16(self + 0x174u, 0x14u);
    put(rt, self + 0x16Cu, 0x12Cu);
    rt.memory().store16(self + 0x176u, 6u);
    put_float(rt, self + 0x170u, 1.0f);
    put(rt, self + 0x17Cu, 0x00FFFFFFu);
    put(rt, self + 0x144u, 0u);
    rt.memory().store8(self + 0x148u, 0u);
    rt.memory().store8(self + 0x149u, 0u);
    rt.memory().store16(self + 0x156u, 0u);
    put(rt, self + 0x15Cu, 0u);
    rt.memory().store8(self + 0x160u, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x178u, 0u);
    put(rt, self + 0x180u, 0u);
    put(rt, self + 0x184u, self + 0x184u);
    put(rt, self + 0x184u + 4u, self + 0x184u);
    put(rt, self + 0x18Cu, self + 0x18Cu);
    put(rt, self + 0x18Cu + 4u, self + 0x18Cu);
    put(rt, self + 0x194u, 0u);
    rt.memory().store8(self + 0x198u, 0u);
    for (u32 i = 0u; i < 4u; ++i) call_set(rt, ctx, 0x08A76AA0u, {self + 0x19Cu + 4u * i});
    put_float(rt, self + 0x1D4u, 1.0f);
    put(rt, self + 0x1ACu, 0u);
    put(rt, self + 0x1B0u, 0u);
    rt.memory().store8(self + 0x1B4u, 0u);
    rt.memory().store8(self + 0x1B5u, 0u);
    put(rt, self + 0x1B8u, 0u);
    put_float(rt, self + 0x1BCu, 1.0f);
    put(rt, self + 0x1C0u, 0u);
    put_float(rt, self + 0x1C4u, 1.0f);
    put(rt, self + 0x1C8u, 0u);
    put(rt, self + 0x1CCu, 0u);
    put_float(rt, self + 0x1D0u, 1.0f);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1E0u + 0x10u, 0xFFFFFFFFu);
    put(rt, self + 0x1E0u + 4u, 0u);
    put(rt, self + 0x1E0u + 8u, 0u);
    put(rt, self + 0x1E0u + 0xCu, 0u);
    rt.memory().store8(self + 0x204u, 0u);
    rt.memory().store8(self + 0x200u, 0u);
    rt.memory().store8(self + 0x201u, 0u);
    rt.memory().store8(self + 0x202u, 0u);
    rt.memory().store8(self + 0x203u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08AB8104 (a0 = table object with its count at +0x1480, a1 = key, a2 = 32-byte record, a3 = 4-byte tag): a table of
// 32 entries of 0xA4 bytes; a match for `key` (0x08B35FD8 returns 0) gets the record at +0x80 and the tag at +0xA0,
// otherwise a new entry is written (0x08B7C830 fills its first 0x80 bytes) and the count grows. The record and the tag
// are copied byte for byte (the original uses `lwl`/`lwr`, so they may be unaligned). v0 = 1 when something was written,
// 0 when the table is full.
void func_08AB8104(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 record = ctx.gpr[6];
    const u32 tag = ctx.gpr[7];
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20, 21, 22, 23});
    GuestFrame frame(ctx, 0x30u);
    const s32 count = static_cast<s32>(word(rt, base + 0x1480u));
    if (count >= 0x20) {
        ctx.gpr[2] = 0u;
        return;
    }
    s32 found = -1;
    for (s32 i = 0; i < count; ++i) {
        if (call_set(rt, ctx, 0x08B35FD8u, {base + static_cast<u32>(i) * 0xA4u, key}) == 0u) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        const u32 slot = base + static_cast<u32>(found) * 0xA4u;
        copy_bytes(rt, slot + 0x80u, record, 0x20u);
        copy_bytes(rt, slot + 0xA0u, tag, 4u);
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 count_now = word(rt, base + 0x1480u);
    call_set(rt, ctx, 0x08B7C830u, {base + count_now * 0xA4u, key, 0x80u});
    const u32 slot = base + count_now * 0xA4u;
    copy_bytes(rt, slot + 0x80u, record, 0x20u);
    copy_bytes(rt, slot + 0xA0u, tag, 4u);
    put(rt, base + 0x1480u, count_now + 1u);
    ctx.gpr[2] = 1u;
}

// 0x088A4A9C (a0 = object, 0x800 bytes): constructor — vtables at +0 and at the sub-object at +0x20, the 1400.0f field at
// +0x14, four 0x18-byte records at +0x5C, four 0x1C-byte records at +0xC4, four 0xC-byte records at +0x134, a container
// init (0x088AA504), a second (0x088AB254), two tables through 0x08961058 / 0x088A42BC and a final 0x08B36018 whose
// answer is v0.
void func_088A4A9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16, 17, 18});
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0u);
    const u32 a2 = self + 0x20u;

    put(rt, self + 0x00u, 0x08CC29A8u);
    put(rt, self + 0x04u, 0u);
    ctx.gpr[5] = 0u;
    ctx.gpr[4] = self + 0x08u;
    call_here(rt, ctx, 0x088A79ACu);

    put(rt, self + 0x10u, 0x08CC2AC8u);
    put(rt, self + 0x20u, 0x08CC2CB8u);
    put_float(rt, self + 0x14u, std::bit_cast<float>(0x44AF0000u));
    put(rt, self + 0x18u, 0u);
    put(rt, a2 + 0x04u, 0x08CC2DDCu);
    rt.memory().store8(a2 + 0x18u, 1u);
    put(rt, a2 + 0x08u, self);
    put(rt, a2 + 0x10u, 0u);
    put(rt, a2 + 0x1Cu, 0u);
    put_float(rt, a2 + 0x30u, 1.0f);
    put(rt, a2 + 0x34u, 0u);
    rt.memory().store8(a2 + 0x38u, 0u);
    rt.memory().store8(a2 + 0x39u, 0u);

    // Four 0x18-byte records at +0x5C: word 0, 1.0f, word 0 and a byte.
    for (u32 j = 0u; j < 4u; ++j) {
        const u32 entry = self + 0x5Cu + 0x18u * j;
        put(rt, entry + 0x00u, 0u);
        put_float(rt, entry + 0x04u, 1.0f);
        put(rt, entry + 0x08u, 0u);
        rt.memory().store8(entry + 0x0Cu, 0u);
    }
    put(rt, a2 + 0xA0u, 0x12345678u);

    // Four 0x1C-byte records at +0xC4: vtable, zeros, a second vtable at +0xC and zeros at +0x10, +0x14, +0x18.
    for (u32 j = 0u; j < 4u; ++j) {
        const u32 b = self + 0xC4u + 0x1Cu * j;
        put(rt, b + 0x00u, 0x08CC2DF8u);
        put(rt, b + 0x04u, 0u);
        put(rt, b + 0x08u, 0u);
        put(rt, b + 0x0Cu, 0x08CC2E78u);
        put(rt, b + 0x10u, 0u);
        put(rt, b + 0x14u, 0u);
        put(rt, b + 0x18u, 0u);
    }
    // Four 0xC-byte records at +0x134: vtable and two zero words.
    for (u32 j = 0u; j < 4u; ++j) {
        const u32 entry = a2 + 0x114u + 0xCu * j;
        put(rt, entry + 0x00u, 0x08CC2E48u);
        put(rt, entry + 0x04u, 0u);
        put(rt, entry + 0x08u, 0u);
    }

    put(rt, a2 + 0x1A0u, 0u);
    put(rt, a2 + 0x1A4u, 0u);
    put(rt, a2 + 0x1A8u, 0u);
    put(rt, a2 + 0x1ACu, 0u);
    rt.memory().store8(a2 + 0x1B0u, 0u);
    put(rt, a2 + 0x1B4u, 0u);
    put(rt, a2 + 0x1B8u, 0u);
    rt.memory().store16(a2 + 0x1BCu, 0u);
    rt.memory().store16(a2 + 0x1BEu, 0u);
    put(rt, a2 + 0x20u, 0u);
    put(rt, a2 + 0x24u, 0u);
    put(rt, a2 + 0x28u, 0u);
    put(rt, a2 + 0x2Cu, 0u);
    ctx.gpr[4] = a2;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = a2;
    ctx.gpr[7] = 0xFFFFFFFFu;
    call_here(rt, ctx, 0x088AA504u);

    rt.memory().store8(self + 0x269u, 1u);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1E4u, 0u);
    rt.memory().store8(self + 0x268u, 0u);
    put(rt, self + 0x26Cu, 0u);
    ctx.gpr[4] = self + 0x270u;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = a2;
    ctx.gpr[7] = 0xFFFFFFFFu;
    call_here(rt, ctx, 0x088AB254u);

    const u32 v1 = self + 0x318u;
    put(rt, v1, 0x08CC2C30u);
    put(rt, v1 + 0x04u, 0x08CC2C4Cu);
    rt.memory().store16(v1 + 0x0Cu, 0xFFFFu);
    put(rt, v1 + 0x08u, 0u);
    put(rt, v1 + 0x10u, 0u);
    rt.memory().store8(self + 0x2F8u, 1u);
    put(rt, self + 0x2FCu, 0u);
    put(rt, self + 0x310u, 0u);
    put(rt, self + 0x334u, 0u);
    put(rt, self + 0x330u, 0u);
    put(rt, self + 0x32Cu, 0u);
    put(rt, self + 0x338u, 0u);
    put(rt, self + 0x340u, 0u);
    put(rt, self + 0x33Cu, 0u);
    put(rt, v1 + 0x2Cu, 0xFFFFFFFFu);
    put(rt, v1 + 0x30u, 0u);
    ctx.gpr[4] = self + 0x41Cu;
    ctx.gpr[5] = self + 0x34Cu;
    ctx.gpr[6] = self + 0x40Cu;
    ctx.gpr[7] = 4u;
    call_here(rt, ctx, 0x08961058u);

    ctx.gpr[4] = sp;
    call_here(rt, ctx, 0x08B4E648u);

    ctx.gpr[4] = self;
    ctx.gpr[5] = sp;
    call_here(rt, ctx, 0x088A42BCu);

    put(rt, self + 0x2D8u, 0u);
    put(rt, self + 0x2D8u + 0x0Cu, 0u);
    put(rt, self + 0x2D8u + 0x04u, 0u);
    put(rt, self + 0x2D8u + 0x08u, 0u);
    put(rt, self + 0x2E8u, 0u);
    put(rt, self + 0x2E8u + 0x0Cu, 0u);
    put(rt, self + 0x2E8u + 0x04u, 0u);
    put(rt, self + 0x2E8u + 0x08u, 0u);
    ctx.gpr[4] = self + 0x1E8u;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0x80u;
    ctx.gpr[7] = 4u;
    call_here(rt, ctx, 0x08B36018u);
    // v0 is the answer of the last call (take() has set it).
}

#define NEW2094(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2094::name, gt::ReturnKind::kind)
NEW2094(0x088865C8u, func_088865C8, Word);
NEW2094(0x0897B364u, func_0897B364, Word);
NEW2094(0x08AB8104u, func_08AB8104, Word);
NEW2094(0x088A4A9Cu, func_088A4A9C, Word);

}  // namespace
}  // namespace new_batch_main_2094
}  // namespace gt::game
