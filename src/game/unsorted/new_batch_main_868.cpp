// Main-session batch 868: functions an earlier pass mismatched in the differential gate. Each body follows its asm from
// tools/disasm: the branches and loops as they read, the delay slots done before the call/branch, callee-saved registers
// saved and put back (SavedRegisters) where the original sets them, v0 as every exit path leaves it. No names: none has
// evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_868 {
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
// A callee with float arguments ($f12...) whose $f0 the original keeps; v0/v1/f0 come back as after the `jal`.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// s0-s7 and fp are callee-saved: the body sets them like the original (a callee may read them) and the caller's come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// `lwl rt, 3(p); lwr rt, 0(p)`: one little-endian word from an address that need not be aligned.
u32 unaligned_word(Runtime &rt, u32 address) {
    return static_cast<u32>(rt.memory().load8(address)) | (static_cast<u32>(rt.memory().load8(address + 1u)) << 8) |
           (static_cast<u32>(rt.memory().load8(address + 2u)) << 16) |
           (static_cast<u32>(rt.memory().load8(address + 3u)) << 24);
}

// --- one argument of a serialized record ----------------------------------------------------------------
// 0x08B636A0, 0x08B72830 and 0x08B75DA0 hold the same body, differing only in the two readers the width nibble 1 and 9
// reach and in the jump table. a0 = a type code (its low nibble picks the width, its bit 7 a dereference and bits 4-6 the
// base), a1 = the base an offset field is added to, a2 = the record cursor, a3 = the out word. The widths: 0/3/11 read an
// unaligned word and step 4, 4/12 the same but step 8, 2 an unsigned half, 10 a signed one, 1/9 hand the cursor to a reader
// that answers the value in a stack word, 5-8 (and 13-15) are the format error. A code 0x50 aligns the cursor up to the next
// word instead and takes the word there. A value other than 0 gets the base added - the record's own address when the code's
// bits 4-6 are 0x10 - and, with the code's sign bit set, one word is loaded from there. v0 = the advanced cursor.
void read_record_argument(Runtime &rt, AllegrexContext &ctx, u32 reader_low, u32 reader_high) {
    const u32 code = ctx.gpr[4] & 0xFFu;
    const u32 base = ctx.gpr[5];
    const u32 start = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = start;
    ctx.gpr[17] = code;
    ctx.gpr[18] = base;
    ctx.gpr[19] = out;
    u32 cursor = start;
    if (code == 0x50u) {
        const u32 aligned = (cursor + 3u) & 0xFFFFFFFCu;
        put(rt, out, word(rt, aligned));
        ctx.gpr[2] = aligned + 4u;
        return;
    }
    const u32 width = code & 0xFu;
    if (width >= 13u) {
        call(rt, ctx, 0x08B77B04u, {code});
        ctx.gpr[2] = 0u;
        return;
    }
    u32 loaded = 0u;
    switch (width) {
    case 0u:
    case 3u:
    case 11u:
        loaded = unaligned_word(rt, cursor);
        cursor += 4u;
        break;
    case 4u:
    case 12u:
        loaded = unaligned_word(rt, cursor);
        cursor += 8u;
        break;
    case 2u:
        loaded = static_cast<u32>(rt.memory().load8(cursor)) |
                 (static_cast<u32>(rt.memory().load8(cursor + 1u)) << 8);
        cursor += 2u;
        break;
    case 10u:
        loaded = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(
            static_cast<u32>(rt.memory().load8(cursor)) | (static_cast<u32>(rt.memory().load8(cursor + 1u)) << 8))));
        cursor += 2u;
        break;
    case 1u: {
        const u32 next = call(rt, ctx, reader_low, {cursor, frame.at(0u)});
        loaded = word(rt, frame.at(0u));
        cursor = next;
        break;
    }
    case 9u: {
        const u32 next = call(rt, ctx, reader_high, {cursor, frame.at(0u)});
        loaded = word(rt, frame.at(0u));
        cursor = next;
        break;
    }
    default:  // 5-8: the jump table's format-error entries
        call(rt, ctx, 0x08B77B04u, {code});
        ctx.gpr[2] = 0u;
        return;
    }
    u32 value = loaded;
    if (loaded != 0u) {
        value = loaded + (((code & 0x70u) == 0x10u) ? start : base);
        if (static_cast<std::int8_t>(code) < 0) value = word(rt, value);
    }
    put(rt, out, value);
    ctx.gpr[2] = cursor;
}
void func_08B636A0(Runtime &rt, AllegrexContext &ctx) { read_record_argument(rt, ctx, 0x08B63614u, 0x08B63648u); }
void func_08B72830(Runtime &rt, AllegrexContext &ctx) { read_record_argument(rt, ctx, 0x08B727A4u, 0x08B727D8u); }
void func_08B75DA0(Runtime &rt, AllegrexContext &ctx) { read_record_argument(rt, ctx, 0x08B75D14u, 0x08B75D48u); }

// --- a step of an angle interpolation ------------------------------------------------------------------
// 0x08B2A0EC: a0/a1/a2 are three output words (the original copies a0 to a3 and a1 to t0 and stores through a3, t0 and a2),
// f12 is a normalized phase, f13 an amount and f14 a value. f13 == 0 stores f14 three times. Otherwise the phase is
// wrapped into [0, 1], the six-segment index is index / 6 (signed, truncating) and the weights are built from the
// remainder; the segment - the jump table at 0x08C93A40, 6 entries - picks which weight goes to which output. A negative
// index (dividend) answers nothing. v0 is untouched, as in the original.

// --- a lens/suspension step that flags a geometry change ------------------------------------------------
// 0x088C10B0: a0 = the object, a1 = the state word block. The block's first two floats are handed negated to 0x08B4A430
// with the third as a second argument; its answer and a scale from the object's table (word +0x4, field +0x704) are the
// arguments of 0x0893C60C, whose float goes to the block's word +8. The state machine lives in the object's record
// (word(a0 + 0x24)): byte +0xF is the mode (1, 2 or anything else), byte +0x5F9 of the table feeds one decision, float
// +0x6C4 of the table is the limit compared against the magnitude of the answer. The three branches only set the block's
// words +0xC and +0x10 (0, 1 or -1). The tail counts byte +0x6C down; at 0 it asks 0x088C0DCC and stores its answer plus the
// new mode when it differs from the current one. Those are the bytes the gate saw swapped.

} // namespace
} // namespace new_batch_main_868
} // namespace gt::game

#define NEW868(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_868::name, gt::ReturnKind::kind)
NEW868(0x08B636A0u, func_08B636A0, Word);
NEW868(0x08B72830u, func_08B72830, Word);
NEW868(0x08B75DA0u, func_08B75DA0, Word);
