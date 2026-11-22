// New batch (main session 122): five functions read from `tools.disasm` and written as structured C by hand. No names: none has
// evidence. The rest of the candidate list is either a Sony import stub (.sceStub.text), VFPU (the harness compares the VFPU
// register file), reads uninitialised stack, or is long enough (object-graph constructors, std::string internals) that a wrong draft
// would cost a verification round: they are listed in the batch report, not drafted.
//
// Every call_guest passes the argument registers the original sets before the `jal`; the ones it leaves alone keep the caller's
// value (host/guest_call.hpp). The callee-saved registers are never written here (the original restores them before it returns or
// tail-calls). The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` delay slot only when the branch is taken,
// and the delay slot of a non-likely branch on both paths. v0 is set as the original leaves it on every exit path; the dead stack is
// not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_122 {
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

// A `lw v0, slot(vtable); jalr v0` call: v1 and f0 of the callee reach the next instruction like v0 does, and a callee that writes
// no v0 hands on its own address (hot_common's call_indirect rule).
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_virtual_v0(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    return call_virtual(rt, ctx, object, slot, arguments).v0;
}

s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// 0x088BCC9C (a0 = object): a float in [0, 1] moves toward the object's target by a step that depends on how far a position
// counter *(state + 8) is from 601. `count` = 1 + the value func_088DA1C8(a0[3], a0[4]) returns, and the step is that count.
// state = *(a0 + 0x24): its byte +0x3B is the direction flag, its float +0x28 the value written back.
//  * -(state[2]) < 601 (state[2] positive and large): the value moves by -0.002 * count + 0.625 plus
//    -2 * (a1[0x58E] - a1[0x2A6]) / a1[0x23C], clamped to [0, 1] (a1 = *(a0 + 4)).
//  * otherwise the flag picks the direction: flag 0 and a1[0x58E] below 3 * *(a0[1]) / 4 raise the value by
//    a1[0x4F4] * (-0.2 * count + 5) and set the flag; a set flag with a1[0x58E] below 15 * *(a0[1]) / 16 keeps it and does the
//    same; otherwise the value falls by a1[0x4F4] * (-0.4 * count + 10) and the flag is cleared. Both clamp to [0, 1].
// v0 leaves this function as the original's: 1 on the flag path taken from the unset flag, 0 on the flag path taken from the set
// one, and *(a0 + 8) on the position path.
void func_088BCC9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 object = ctx.gpr[4];
    const u32 state = word(rt, object + 0x24u);
    const u32 inside = word(rt, state + 8u);  // loaded in the jal's delay slot, so before the call
    const u32 count = call(rt, ctx, 0x088DA1C8u, {word(rt, object + 0xCu), word(rt, object + 0x10u)}) + 1u;
    const float step = static_cast<float>(static_cast<s32>(count));
    if (static_cast<s32>(0u - inside) < 0x259) {
        const u32 first = word(rt, object + 4u);
        const u32 other = word(rt, object + 8u);
        ctx.gpr[2] = other;  // what the original leaves in v0 on this path
        const float weight = float_at(rt, other + 0x4F4u);
        const float span = static_cast<float>(signed_half(rt, other + 0x58Eu) - static_cast<s32>(rt.memory().load16(first + 0x2A6u)));
        const float total = static_cast<float>(rt.memory().load16(first + 0x23Cu));
        const float value = (psp_mul(step, -0.002f) + 0.625f) + psp_mul(span / total, -2.0f);
        put_float(rt, state + 0x28u, value);
        if (1.0f < value) {
            put_float(rt, state + 0x28u, 1.0f);
        } else if (value < 0.0f) {
            put_float(rt, state + 0x28u, 0.0f);
        }
        return;
    }
    const u32 first = word(rt, object + 4u);
    const float weight = float_at(rt, word(rt, object + 8u) + 0x4F4u);
    const s32 level = signed_half(rt, word(rt, object + 8u) + 0x58Eu);
    const u32 units = rt.memory().load16(first + 0x2A6u);
    u32 result;
    bool raise;
    if (rt.memory().load8(state + 0x3Bu) == 0u) {
        result = 1u;  // the delay slot of the branch into the raise block
        raise = level < (static_cast<s32>(units * 3u) >> 2);
        if (raise) rt.memory().store8(state + 0x3Bu, 1u);
    } else {
        const s32 product = static_cast<s32>(units * 15u);
        const s32 rounded = (product < 0 ? product + 0xF : product) >> 4;
        result = product < 0 ? 1u : 0u;
        raise = level < rounded;
        if (!raise) rt.memory().store8(state + 0x3Bu, 0u);
    }
    const float value_at = float_at(rt, state + 0x28u);
    if (raise) {
        const float value = value_at + psp_mul(weight, psp_mul(step, -0.2f) + 5.0f);
        put_float(rt, state + 0x28u, value);
        if (1.0f < value) put_float(rt, state + 0x28u, 1.0f);
    } else {
        const float value = value_at - psp_mul(weight, psp_mul(step, -0.4f) + 10.0f);
        put_float(rt, state + 0x28u, value);
        if (value < 0.0f) put_float(rt, state + 0x28u, 0.0f);
    }
    ctx.gpr[2] = result;
}

// 0x08AC972C (a0 = destination, a1 = destination again in s5, a2 = length, a3 = a holder whose word +4 points at a table, t0 = an
// index). The table's header sits at +0x30: [begin, end) of entry pointers. Each entry's word +0x10 names a string table with a
// count (func_08ADC988(entry[4], 0)); the index runs over the concatenation of those tables. The first entry whose range holds t0
// fills a stack copy of its table (func_08ADC988(entry[4], buffer), one word per string) and copies the t0-th string there into a0
// with a2 bytes (func_08B7C830), zeroing the last byte; v0 = 1. Every other exit stores a zero byte at a1 and answers 0.

// 0x08B51F7C (a0 = an object with a status byte +0x24 and a byte +4, a1 = the text compared against). The text's rotated forms
// are tried one after another: func_08B51F38(a0) names the object's current form (null ends the loop, v0 = 0) and its byte +0xC
// decides whether the rotation gets the object's byte +4 in front. Two stack copies of the text (length from func_08B35F60) hold
// the rotated string and the candidate: round r rotates by r % length, copies the tail behind the head (func_08B7C830 twice) and
// asks func_08B7C6BC(text, candidate, length); a zero answer ends the function with v0 = 0, otherwise the next round runs.
void func_08B51F7C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 object = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 length = call(rt, ctx, 0x08B35F60u, {text});
    const u32 aligned = (length + 0x1Fu) & ~15u;
    GuestFrame buffers(ctx, aligned * 2u);
    const u32 candidate = ctx.gpr[29];
    const u32 rotated = candidate + aligned;
    rt.memory().store8(rotated + length, 0u);
    rt.memory().store8(candidate + length, 0u);
    u32 round = 0u;
    for (;;) {
        const u32 form = call(rt, ctx, 0x08B51F38u, {object});
        if (form == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        // A length of zero would divide by it (the original's `div` result is undefined there); no rotation exists then.
        const u32 offset = length == 0u ? 0u : round % length;
        std::uint8_t fill = 0u;
        if (rt.memory().load8(form + 0xCu) != 0u) fill = static_cast<std::uint8_t>(signed_byte(rt, form + 4u));
        round += 1u;
        rt.memory().store8(rotated + offset, fill);
        const u32 tail = length - offset - 1u;
        (void)call(rt, ctx, 0x08B7C830u, {candidate, rotated + offset + 1u, tail});
        (void)call(rt, ctx, 0x08B7C830u, {candidate + tail, rotated, offset + 1u});
        if (call(rt, ctx, 0x08B7C6BCu, {text, candidate, length}) == 0u) return;  // v0 = 0, the callee's answer
    }
}

// 0x08B34748 (a0 = lock object, a1 = a flag byte): pushes a wait node on the lock's second queue while the interrupts are masked
// (0x08C7A888 / 0x08C7A8A8 are sceKernelCpuSuspendIntr / sceKernelCpuResumeIntr, the mask travels in a0). thread = the owner id
// from func_08B2E6DC(lock). With lock[0x20] == 0 the owner word lock[0x18] is set and lock[0x20] = 1 (nothing pushed); with
// lock[0x18] already the thread only lock[0x20] goes up by one. Otherwise the node {0, thread, -1, 0, -1} on the stack is pushed
// (lock[4] or the old tail points at it) and lock[4] = sp. A flag byte set, a node pushed and a negative lock[0x24] save lock[0x20]
// into lock[0x24] and the id from func_08B2E0B0 into lock[0x1C]. After the resume a pushed node calls func_08B2E0A4(id) when the
// flag is set, func_08C7AA98(id) always, and the round starts again; nothing pushed returns with the resume's answer in v0.

// A shared object whose last use runs `slot` with itself; the weak count is released the same way afterwards.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    const u32 uses = rt.memory().load32(object + 4u);
    put(rt, object + 4u, uses - 1u);
    if (uses != 1u) return;
    (void)call_virtual_v0(rt, ctx, object, 8u, {object});
    const u32 weak = rt.memory().load32(object + 8u);
    put(rt, object + 8u, weak - 1u);
    if (weak != 1u) return;
    (void)call_virtual_v0(rt, ctx, object, slot, {object});
}

// 0x088763E8 (a0 = the array): the five two-word slots built over a cleared header. *(a0) is zeroed, then the ten words at
// a0 + 4 .. a0 + 0x28 (five pairs). Each slot then gets an object of 0x38 bytes (vtable 0x08CBFD38, words +4 and +8 zero, the shared
// cell 0x08D752A4 at +0xC and +0x10, +0x14..+0x20 zero, byte +0x24 and +0x25 zero with bit 16 of that word cleared, +0x30 the shared
// cell) and a control block of 0x10 (vtable 0x08CBFD78, 1 at +4 and +8, the object at +0xC), both handed to func_08B8E380(a1 = a2 =
// the object) through the stack words +0 and +4; the slot's old two words are released (use count at +4, then the weak count at +8).
// v0 = 5 on every exit.

// 0x08B437C0 (a0 = object): renews the object's registration handles. func_08B3EE8C(object + 0x8DC, virtual slot 0x90 with this)
// names the registration; a null answer ends the function after the virtual slot 0x40 runs (v0 = its answer). Otherwise its byte +4
// is set to 1 and the first non-zero flag byte among the table's `count` bytes at object + 0x854 names an entry of the pointer table
// at object + 0x454 (no such byte keeps v0 = 0 and returns). Each entry walks its items through the next pointer at entry + 8: an
// item whose byte +8 is set and whose s16 +0x16 differs from the virtual slot 0x90 answer takes that answer again into +0x16. Then
// a 0xC-byte key {0, 0, slot 0x90 answer in the high half of +8} is passed to func_08B42A90(object, 0x4E23, key, 0xC, 1, 1, 0, 0),
// a zero answer returns v0 = 0, and the 64-bit answer of the virtual slot 0x8C (v0:v1) is stored at +0x30 / +0x34 of every node of
// the list func_08B3F3C8(object + 0x8DC) names (next at node + 8), v0 = 0.
// Branches the compared cases will not take (checked against the asm by hand): the `bltz` arm at 0x08B4384C is dead (the counter
// only counts up from 0, and the index is copied into a3 in the `bgez` delay slot on both arms); a null item slot skips the slot
// work; `count` 0 makes the flag scan read the first byte and stop.
void func_08B437C0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 object = ctx.gpr[4];
    const u32 registration = object + 0x8DCu;
    const u32 handle = call_virtual_v0(rt, ctx, object, 0x90u, {object});
    const u32 table = call(rt, ctx, 0x08B3EE8Cu, {registration, handle});
    if (table == 0u) {
        (void)call_virtual(rt, ctx, object, 0x40u, {object});
        return;
    }
    rt.memory().store8(table + 4u, 1u);
    const u32 base = object + 0x454u;
    const u32 count = word(rt, base + 0x480u);
    u32 index = 0u;
    bool located = false;
    std::uint8_t probe = rt.memory().load8(base + 0x400u);
    for (u32 i = 0u;; ++i) {
        if (probe != 0u) {
            index = i;
            located = true;
            break;
        }
        if (i + 1u == count) break;
        probe = rt.memory().load8(base + 0x400u + i + 1u);
    }
    if (located) {
        u32 entry = word(rt, base + index * 4u);
        while (entry != 0u) {
            const u32 item = word(rt, entry);
            if (item != 0u && rt.memory().load8(item + 8u) != 0u) {
                const u32 previous = static_cast<u32>(signed_half(rt, item + 0x16u));  // the delay slot, so before the call
                if (previous != call_virtual_v0(rt, ctx, object, 0x90u, {object})) {
                    rt.memory().store16(item + 0x16u, static_cast<std::uint16_t>(call_virtual_v0(rt, ctx, object, 0x90u, {object})));
                }
            }
            entry = word(rt, entry + 8u);  // the original reads it again after the calls unless the item slot was null
        }
    }
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    rt.memory().store16(frame.at(8u), 0u);
    const u32 identifier = call_virtual_v0(rt, ctx, object, 0x90u, {object});
    rt.memory().store16(frame.at(0xAu), static_cast<std::uint16_t>(identifier));
    if (call(rt, ctx, 0x08B42A90u, {object, 0x4E23u, frame.at(0u), 0xCu, 1u, 1u, 0u, 0u}) == 0u) return;  // v0 = 0
    const GuestResult low_high = call_virtual(rt, ctx, object, 0x8Cu, {object, 0u});
    const u32 list = call(rt, ctx, 0x08B3F3C8u, {registration});
    if (list != 0u) {
        u32 cursor = list;
        u32 node = word(rt, cursor);
        while (cursor != 0u) {
            if (node != 0u) {
                put(rt, node + 0x30u, low_high.v0);
                put(rt, node + 0x34u, low_high.v1);
            }
            cursor = word(rt, cursor + 8u);
            if (cursor != 0u) node = word(rt, cursor);
        }
    }
    ctx.gpr[2] = 0u;
}

} // namespace
} // namespace new_batch_main_122
} // namespace gt::game

#define MAIN122(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_122::name, gt::ReturnKind::kind)
MAIN122(0x088BCC9Cu, func_088BCC9C, Word);
MAIN122(0x08B51F7Cu, func_08B51F7C, Word);
MAIN122(0x08B437C0u, func_08B437C0, Word);
