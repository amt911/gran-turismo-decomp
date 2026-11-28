// Main-session batch 994: functions an earlier batch drafted without a gate run (or that the gate dropped), rewritten
// as structured C from `tools/disasm` and the gate's recorded difference. Draft only, unverified: every body keeps the
// original's v0 on every exit path, passes the argument registers each callee reads, runs a `jal`'s delay slot before
// the call (a likely branch's only when it is taken), and allocates the prologue's frame size.
// Input shapes for the synthetic generator: /home/andres/repos/gt-ops/shapes/shapes_main994.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_994 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same, keeping v0 / v1 / f0 (and a1) as the callee left them: the instructions after the call read them, and a
// callee that never writes v0 hands the caller's value on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call with float arguments (f12, f13, ...), result in f0.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: `lw v1, 0(object); lw v0, offset(v1); jalr v0` — v1 is the table at the call.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
s32 signed_of(u32 value) { return static_cast<s32>(value); }
u32 load8u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void store16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// A byte copy, as the original's block copies leave it: `count` bytes from source to destination.
void copy_bytes(Runtime &rt, u32 destination, u32 source, u32 count) {
    for (u32 offset = 0u; offset != count; ++offset) store8(rt, destination + offset, load8u(rt, source + offset));
}

// ---------------------------------------------------------------------------------------------------
// 0x088D2098 (a0 = destination record, a1 = source record): a partial copy of a 0x314-byte packed record. The
// original copies it as three overlapping 16-byte-block runs — source +0x09..+0xC9 into +0xC, +0xC9..+0x1C9 into
// +0xCC, then +0x109..+0x209 into +0x14C (which rewrites part of the second run) — plus the byte at +0, the two
// words at +1 and +5, and the tail fields at +0x1C9, +0x1D5, +0x1D9 and +0x1E5. Bytes +1..+3, +0x1CC..+0x20B and
// +0x24C..+0x303 are never written. v0 is the source end (+0x1F5): the last block load leaves it there.

// ---------------------------------------------------------------------------------------------------
// 0x08C4D7D4 (descriptor = a0, unused = a1, low = a2, high = a3): a type-dispatched store. The word at descriptor
// +0 is a type id and +4 the destination pointer; twelve lazily built ids (each getter takes no argument) are tried
// in turn and, on a match with a non-null destination, (low, high) is converted with that type's helper and stored:
// a float, a word pair, a byte, a byte, a halfword, a halfword, a word, a word, a word pair, a word pair, or a bool
// (twice). v0 = 1 when something was stored, else 0.
void func_08C4D7D4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 descriptor = ctx.gpr[4];
    const u32 low = ctx.gpr[6];
    const u32 high = ctx.gpr[7];
    const auto destination = [&] { return word(rt, descriptor + 4u); };
    // The id is read before the getter runs: the original's `lw s0, 0(s1)` sits in the call's delay slot.
    const auto matches = [&](u32 getter) {
        const u32 id = word(rt, descriptor);
        return id == call(rt, ctx, getter);
    };
    if (matches(0x08B9DDB0u)) {
        const u32 target = destination();
        if (target != 0u) {
            put_float(rt, target, call_keep(rt, ctx, 0x08B71EC8u, {low, high}).f0);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08C4D18Cu)) {
        const u32 target = destination();
        if (target != 0u) {
            put(rt, target, low);
            put(rt, target + 4u, high);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC473Cu)) {
        const u32 target = destination();
        if (target != 0u) {
            store8(rt, target, call(rt, ctx, 0x08B71DC8u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC48B4u)) {
        const u32 target = destination();
        if (target != 0u) {
            store8(rt, target, call(rt, ctx, 0x08B6F110u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC45C4u)) {
        const u32 target = destination();
        if (target != 0u) {
            store16(rt, target, call(rt, ctx, 0x08B71DC8u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC444Cu)) {
        const u32 target = destination();
        if (target != 0u) {
            store16(rt, target, call(rt, ctx, 0x08B6F110u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC42D4u)) {
        const u32 target = destination();
        if (target != 0u) {
            put(rt, target, call(rt, ctx, 0x08B71DC8u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08B9E8D4u)) {
        const u32 target = destination();
        if (target != 0u) {
            put(rt, target, call(rt, ctx, 0x08B6F110u, {low, high}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC415Cu)) {
        const u32 target = destination();
        if (target != 0u) {
            const GuestResult converted = call_keep(rt, ctx, 0x08B6F300u, {low, high});
            put(rt, target, converted.v0);
            put(rt, target + 4u, converted.v1);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC3FE4u)) {
        const u32 target = destination();
        if (target != 0u) {
            const GuestResult converted = call_keep(rt, ctx, 0x08B6F1B8u, {low, high});
            put(rt, target, converted.v0);
            put(rt, target + 4u, converted.v1);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    // The two bool ids share the conversion and the store; the first falls through when its destination is null.
    if (matches(0x08B9CEBCu)) {
        const u32 target = destination();
        if (target != 0u) {
            store8(rt, target, call(rt, ctx, 0x08B71A08u, {low, high, 0u, 0u}) != 0u ? 1u : 0u);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    if (matches(0x08BC4A2Cu)) {
        const u32 target = destination();
        if (target != 0u) {
            store8(rt, target, call(rt, ctx, 0x08B71A08u, {low, high, 0u, 0u}) != 0u ? 1u : 0u);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------
// 0x08925D10 (a0 = scene object): refresh its state from its record. The record's index is word(+4) + word(+0xC) *
// 0x268 (a zero index returns at once) and 0x0892BB10 turns an index into the record. The record's word +0x28 and
// its word +0x24 are kept, each with the bits word(+0x14) / word(+0x28) says were already seen, the floats at the
// record's +0x2C come from 0x089299C8 / 0x089299D0, and the object's own virtual slots answer a feature mask at
// +0x30 (slot 0x18 with 1, 2, 0x20 and — only while bits 2 and 4 of the mask are clear — 4, 8; or, when word(+8)
// is set, slot 0x28 with 8, 0xA, 0xC and slot 0x1C with 0x12). v0 and +0x38 are the mask with +0x34's bits cleared.

// ---------------------------------------------------------------------------------------------------
// 0x08C5BA40 (out = a0, state = a1, source = a2, c = a3, t0, begin = t1, end = t2): the format-string scanner.
// `state` is {stream word at +0, byte at +4} (the byte is set when a 0xFF went out, and its characters are then
// skipped). Every byte of [begin, end) is converted (convert_character) and written to the stream at state[0],
// whose buffer is flushed through virtual slot 0x34 when full. '%' hands the escape to the stream's virtual slot 8
// with (out, state, source, c, t0, character, 0), or — for "%E<c>" / "%O<c>" — with the character behind the
// letter in t1 and the letter in t2. v0 = the state word, v1 = the state byte.
constexpr u32 kConversionCache = 0x11Du;  // the table object's 256-byte cache, per source byte

s32 convert_character(Runtime &rt, AllegrexContext &ctx, u32 table, u32 raw) {
    const u32 slot = table + (raw & 0xFFu);
    const s32 cached = static_cast<std::int8_t>(rt.memory().load8(slot + kConversionCache));
    if (cached != 0) return cached;
    const s32 converted =
        static_cast<std::int8_t>(call_virtual(rt, ctx, table, 0x20u, {table, raw, 0u}).v0);
    if (converted != 0) store8(rt, slot + kConversionCache, static_cast<u32>(converted));
    return converted;
}


// ---------------------------------------------------------------------------------------------------
// 0x08B472AC (a0 = object with a list at +0x2C): the list's count (func_08B1FDB0) sizes two arrays on the stack —
// one 0x0C-byte call record per entry and one table word per entry. Walking the list from +0x30, a node whose
// table's byte at +0x30 is set gives {table[0], 0xB0E, 0} and its table word, until the count is reached or the
// list ends. That descriptor list goes to func_08C7AE18(records, filled, 0, 1); a zero answer, or a negative one
// other than 0x80410717, ends the function. Otherwise each record walks its entry again: the state word at +8 with
// 0x100 / 0x200 / 0x800 asks the object's virtual 0x94 (self, object) and then 0xA4 (self, table word of the
// object); with 2 / 4 / 8 a byte at +0x31 sends it to func_08B46AA0 instead, and a byte at +0x32 picks
// func_08B471FC over func_08B4665C once func_08B3F910 / func_08B463D4 marked it — only the marked arm goes on to the
// 0x94 virtual, which carries that mark in a2. v0 is the queue's answer, 0 when the list is empty, or the last
// virtual call's; a skipped record (state 0) leaves it alone and an unmarked one leaves it 0.
void func_08B472AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const u32 list = self + 0x2Cu;
    const u32 count = call(rt, ctx, 0x08B1FDB0u, {list});
    if (count == 0u) return;
    ctx.gpr[29] -= (count * 0xCu + 0x1Eu) & ~0xFu;
    const u32 records = ctx.gpr[29];
    ctx.gpr[29] -= (count * 4u + 0x1Eu) & ~0xFu;
    const u32 pointers = ctx.gpr[29];

    u32 filled = 0u;
    for (u32 node = word(rt, list + 4u); node != 0u; node = word(rt, node + 8u)) {
        const u32 table = word(rt, node);
        if (load8u(rt, table + 0x30u) == 0u) continue;
        const u32 record = records + 0xCu * filled;
        put(rt, pointers + 4u * filled, table);
        put(rt, record + 4u, 0xB0Eu);
        put(rt, record, word(rt, table));
        put(rt, record + 8u, 0u);
        ++filled;
        if (filled == count) break;
    }

    u32 result = call(rt, ctx, 0x08C7AE18u, {records, filled, 0u, 1u});
    if (result == 0u) return;
    if (result != 0x80410717u && signed_of(result) < 0) return;
    if (filled == 0u) return;

    for (u32 index = 0u; index != filled; ++index) {
        const u32 record = records + 0xCu * index;
        const u32 state = word(rt, record + 8u);
        if (state == 0u) continue;
        const u32 object = word(rt, pointers + 4u * index);
        if ((state & 0x100u) != 0u || (state & 0x200u) != 0u || (state & 0x800u) != 0u) {
            (void)call_virtual(rt, ctx, self, 0x94u, {self, object});
            result = call_virtual(rt, ctx, self, 0xA4u, {self, word(rt, object)}).v0;
            continue;
        }
        if ((state & 2u) == 0u && (state & 4u) == 0u && (state & 8u) == 0u) {
            result = 0u;  // v0 = the last mask test (state & 8)
            continue;
        }
        if (load8u(rt, object + 0x31u) != 0u) {
            result = call(rt, ctx, 0x08B46AA0u, {self, object});
            continue;
        }
        const u32 found = call(rt, ctx, 0x08B3F910u, {object + 0x1Au});
        u32 mark = 0u;
        bool marked = false;
        if (found != 0u) {
            marked = true;
            mark = call(rt, ctx, 0x08B463D4u, {self, object + 0x1Au}) != 0u ? 1u : 0u;
        }
        if (load8u(rt, object + 0x32u) != 0u) result = call(rt, ctx, 0x08B471FCu, {self, object});
        else result = call(rt, ctx, 0x08B4665Cu, {self, object});
        if (!marked) continue;
        (void)call_virtual(rt, ctx, self, 0x94u, {self, object, mark});
        result = call_virtual(rt, ctx, self, 0xA4u, {self, word(rt, object)}).v0;
    }
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------
// Shared pointers of the standard library: the reference count sits at control+8 (the original's ll/sc loops), and
// a released count of one runs the object's virtual 0xC with the control block as a0. Wide strings use their own
// empty cell (0x08D75298 / 0x08D74FD0) and _M_destroy (0x08B93554 / 0x08B8542C).
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
u32 count_down(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 control) {
    if (control == 0u) return;
    if (count_down(rt, control + 8u) == 1u) (void)call_virtual(rt, ctx, control, 0x0Cu, {control});
}
void release_wide_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 empty_cell, u32 flag_slot, u32 destroy) {
    if (data - 0xCu == empty_cell) return;
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(references) > 0) return;
    (void)call(rt, ctx, destroy, {data - 0xCu, flag_slot});
}

// 0x088B0818 (a0 = object with a red-black tree at +8 under the lock +0x20, a1 = key string, a2 = pair {object,
// control block}): under the lock, a null pair does nothing. The key is copy-constructed at frame+4 (0x08B93CD8) and
// looked up (0x08BAEE1C); self+0xC is the end marker. At a node the key cells are compared through 0x08B36170 over
// the shorter length, a non-zero answer used as is and otherwise the length difference: below zero inserts.
// 0x08B9432C copy-constructs the pair at frame+0x10, 0x08BAFA9C inserts it with the found node as the hint, and the
// temporary's control block (frame+0x18) and key cell (frame+0x10) are released. The node's +0x14 then takes the
// object (it and the passed control block get a reference each), the control block at +0x18 is released and
// replaced, and the passed block's extra reference is released. The key temporary (frame+4) goes, the lock is
// released; v0 is the unlock's answer.

// ---------------------------------------------------------------------------------------------------
// 0x08C6FA80 (a0 = string object, a1 = sink): decode the object's stream into the sink in 0x80-byte writes.
// 0x08C55FDC(frame) probes the object; a zero byte returns early with the +0x14 flag or-ed with 4. Otherwise
// 0x08C74B68(sink, 0, object's most-derived offset, 0) resets the sink and the object's header (self + the offset
// word at -0xC of its first word) gives the byte limit at +8 (below one becomes 0x3FFFFFFC) and the conversion
// table through 0x08B5C450 / 0x08B64C80 / 0x08B5D34C. Codes come from the stream at word(header+0x78): its buffer
// [word(+8), word(+0xC)) answers `lbu`, a run-out asks its virtual 0x24 (and 0x28 when the read byte's cursor runs
// out). A code the table marks with bit 3 stops the run; codes are collected in the 0x80-byte buffer and written
// with 0x08C75AF8 when full or at the end. The last code read (0xFFFFFFFF at the end) then picks the tail flag
// (2 when the stream ended, 0 otherwise) that 0x08C54CFC sets on +0x14 — with 4 as well when nothing was consumed.
// v0 = the object.
void func_08C6FA80(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xC0u);
    const u32 self = ctx.gpr[4];
    const u32 sink = ctx.gpr[5];
    const u32 buffer = frame.at(8u);
    (void)call(rt, ctx, 0x08C55FDCu, {frame.at(0u), self, 0u});
    if (load8u(rt, frame.at(0u)) == 0u) {
        const u32 early = self + word(rt, word(rt, self) - 0xCu);
        (void)call(rt, ctx, 0x08C54CFCu, {early, word(rt, early + 0x14u) | 4u});
        ctx.gpr[2] = self;
        return;
    }
    (void)call(rt, ctx, 0x08C74B68u, {sink, 0u, word(rt, word(rt, sink) - 0xCu), 0u});
    const u32 header = self + word(rt, word(rt, self) - 0xCu);
    const s32 declared = signed_of(word(rt, header + 8u));
    const u32 limit = declared < 1 ? 0x3FFFFFFCu : static_cast<u32>(declared);
    (void)call(rt, ctx, 0x08B5C450u, {frame.at(4u), header + 0x6Cu, word(rt, header + 8u)});
    const u32 table = call(rt, ctx, 0x08B64C80u, {frame.at(4u)});
    (void)call(rt, ctx, 0x08B5D34Cu, {frame.at(4u)});
    const u32 stream = word(rt, self + word(rt, word(rt, self) - 0xCu) + 0x78u);
    const auto marked = [&](u32 code) { return (load8u(rt, word(rt, table + 0x18u) + (code & 0xFFu)) & 8u) != 0u; };
    const auto next_code = [&](u32 slot) {
        const u32 cursor = word(rt, stream + 8u);
        return cursor < word(rt, stream + 0xCu) ? load8u(rt, cursor) : call_virtual(rt, ctx, stream, slot, {stream}).v0;
    };
    u32 code = next_code(0x24u);
    u32 consumed = 0u;
    u32 stored = 0u;
    if (limit != 0u && code != 0xFFFFFFFFu && !marked(code)) {
        store8(rt, buffer, code);
        stored = 1u;
        for (;;) {
            // One code: the cursor advances (the byte just read decides which path answered) and the next code is
            // peeked; a run-out asks the stream's virtual 0x28 first.
            const u32 cursor = word(rt, stream + 8u);
            if (cursor < word(rt, stream + 0xCu)) {
                ++consumed;
                const u32 byte = load8u(rt, cursor);
                put(rt, stream + 8u, cursor + 1u);
                code = byte + 1u != 0u ? next_code(0x24u) : 0xFFFFFFFFu;
            } else {
                code = call_virtual(rt, ctx, stream, 0x28u, {stream}).v0 + 1u != 0u
                           ? next_code(0x24u)
                           : 0xFFFFFFFFu;
            }
            if (consumed >= limit) break;
            if (code == 0xFFFFFFFFu) break;
            if (marked(code)) break;
            if (stored == 0x80u) {
                (void)call(rt, ctx, 0x08C75AF8u, {sink, buffer, 0x80u});
                stored = 0u;
            }
            store8(rt, buffer + stored, code);
            ++stored;
        }
    }
    (void)call(rt, ctx, 0x08C75AF8u, {sink, buffer, stored});
    const u32 flag = ~code != 0u ? 0u : 2u;
    const u32 tail = self + word(rt, word(rt, self) - 0xCu);
    put(rt, tail + 8u, 0u);
    if (consumed == 0u) (void)call(rt, ctx, 0x08C54CFCu, {tail, word(rt, tail + 0x14u) | (flag | 4u)});
    else if (flag != 0u) (void)call(rt, ctx, 0x08C54CFCu, {tail, word(rt, tail + 0x14u) | flag});
    ctx.gpr[2] = self;
}

// ---------------------------------------------------------------------------------------------------
// 0x08883010 (a0 = object, a1 = out, a2 = a state selector or 0): 0x088818A0(a2) yields the selector (negative
// returns 0). The object's member comes from the pointer at self+0xC: its word +0x4C through 0x0889DD64, then that
// answer's word +0x50. A ten-arm jump table (selectors 3..12) fills the string at frame+0 with a piece of the object
// or of that member (0x08BA5174), or with one of four pieces of the object's +0x130 block (0x0888233C). Then
// 0x08B4B114(object+0x130, out, object's 0x140/0x144/0x148 floats) produces the frame's x/y/z, selector 13 adds a
// transform through 0x08B4E734 / 0x08882E34, and 0x08B4E9D4(out, frame+0x10) applies the 4x4 matrix at frame+0x10
// (identity, its last column the x/y/z and 1.0). v0 = 1.
void func_08883010(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    u32 state = 0u;
    if (ctx.gpr[6] != 0u) {
        state = call(rt, ctx, 0x088818A0u, {ctx.gpr[6]});
        if (signed_of(state) < 0) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    const u32 object = word(rt, self + 0xCu);
    u32 member = 0u;
    const u32 lookup = word(rt, object + 0x4Cu);
    if (lookup != 0u) {
        const u32 found = call(rt, ctx, 0x0889DD64u, {lookup});
        if (found != 0u) member = word(rt, found + 0x50u);
    }
    // The three words of D_08C89980 (zero in this image) land at frame + 0 / + 4 / + 8.
    put(rt, frame.at(0u), word(rt, 0x08C89980u));
    put(rt, frame.at(4u), word(rt, 0x08C89980u + 4u));
    put(rt, frame.at(8u), word(rt, 0x08C89980u + 8u));
    switch (state) {
    case 3u: (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), object + 0x59Cu}); break;
    case 4u: (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), object + 0x5E4u}); break;
    case 5u: (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), object + 0x5C0u}); break;
    case 6u: (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), object + 0x608u}); break;
    case 7u: (void)call(rt, ctx, 0x0888233Cu, {frame.at(0u), object + 0x130u, 0u, 1u}); break;
    case 8u: (void)call(rt, ctx, 0x0888233Cu, {frame.at(0u), object + 0x130u, 2u, 1u}); break;
    case 9u: (void)call(rt, ctx, 0x0888233Cu, {frame.at(0u), object + 0x130u, 1u, 0u}); break;
    case 10u: (void)call(rt, ctx, 0x0888233Cu, {frame.at(0u), object + 0x130u, 3u, 0u}); break;
    case 11u:
        if (member != 0u) (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), member + 0x34u});
        break;
    case 12u:
        if (member != 0u) (void)call(rt, ctx, 0x08BA5174u, {frame.at(0u), member + 0xC4u});
        break;
    default: break;
    }
    const u32 position3 = object + 0x140u;
    (void)call_float(rt, ctx, 0x08B4B114u, {object + 0x130u, out},
                     {float_at(rt, position3), float_at(rt, position3 + 4u), float_at(rt, position3 + 8u)});
    const float x = float_at(rt, frame.at(0u));
    if (state == 13u) {
        (void)call(rt, ctx, 0x08B4E734u, {frame.at(0x10u), out});
        (void)call(rt, ctx, 0x08882E34u, {out, frame.at(0x10u), object + 0x164u});
    }
    const float y = float_at(rt, frame.at(4u));
    const float z = float_at(rt, frame.at(8u));
    put_float(rt, frame.at(0x10u), 1.0f);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put_float(rt, frame.at(0x24u), 1.0f);
    put(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0x30u), 0u);
    put(rt, frame.at(0x34u), 0u);
    put_float(rt, frame.at(0x38u), 1.0f);
    put(rt, frame.at(0x3Cu), 0u);
    put_float(rt, frame.at(0x40u), x);
    put_float(rt, frame.at(0x44u), y);
    put_float(rt, frame.at(0x48u), z);
    put_float(rt, frame.at(0x4Cu), 1.0f);
    (void)call(rt, ctx, 0x08B4E9D4u, {out, frame.at(0x10u)});
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------
// 0x088414AC (out = a0, count = a1, holder = a2): append up to `count` entries to the vector at holder's object.
// A non-positive count returns at once. The object at word(holder) (reported through 0x08A5BB7C when null) is copied
// into frame+0x10 by its virtual 0xC; 0x08931090 counts the entries and 0x08A98C1C binds them to the copy's +0x14.
// For each index 0x0893119C fills the 0x40-byte name at frame+0x1C (a negative answer skips it); 0x08930170 turns
// the name into a value pair that 0x08A5DEE8 stores at frame+0x18, 0x0883B7F8 maps it and 0x08B35FD8 looks it up.
// A miss appends: 0x08B85EB8 builds the element at frame+8, 0x08A6D2D4 copies it to frame+0xC and it is moved into
// the vector's slot (0x08A5BAD8) before both temporaries are released; the frame+8 string is released in place
// (0x08B60804 then 0x08B8542C, the empty cell 0x08D74FD0). After the loop the vector is resized to the number
// appended (0x08B85798 / 0x08B85F30 through the spare at frame+4) and the copy at frame+0x14 gets `out` (or is
// released). v0 is the last call's answer.
void func_088414AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const s32 count = signed_of(ctx.gpr[5]);
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    if (count <= 0) return;
    u32 object = word(rt, holder);
    if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    (void)call_virtual(rt, ctx, object, 0x0Cu, {frame.at(0x10u), object});
    const u32 list = frame.at(0x14u);
    const u32 name = frame.at(0x1Cu);
    const u32 key_string = frame.at(0x18u);
    const u32 element = frame.at(0x0Cu);
    u32 used = 0u;
    const u32 entries = call(rt, ctx, 0x08931090u, {frame.at(0x10u)});
    (void)call(rt, ctx, 0x08A98C1Cu, {list, entries});
    if (signed_of(entries) > 0) {
        u32 index = 0u;
        do {
            const u32 status = call(rt, ctx, 0x0893119Cu, {index, name, 0x40u});
            if (signed_of(status) < 0) {
                ++index;
                continue;
            }
            const GuestResult key = call_keep(rt, ctx, 0x08930170u, {name});
            (void)call(rt, ctx, 0x08A5DEE8u, {key_string, ctx.gpr[5], key.v0, key.v1});
            const u32 mapped = call(rt, ctx, 0x0883B7F8u, {1u, key_string});
            const u32 found = call(rt, ctx, 0x08B35FD8u, {word(rt, frame.at(0x10u)), mapped});
            object = word(rt, frame.at(0x14u));
            if (found == 0u) {
                if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08C9C42Cu});
                (void)call(rt, ctx, 0x08B85EB8u, {frame.at(8u), name, frame.at(2u)});
                (void)call(rt, ctx, 0x08A6D2D4u, {element, frame.at(8u)});
                const u32 destination = word(rt, object + 0x14u) + used * 4u;
                if (destination != element) (void)call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0xCu))});
                (void)call(rt, ctx, 0x08A5B9ECu, {element});
                release_wide_string(rt, ctx, word(rt, frame.at(8u)), 0x08D74FD0u, frame.at(1u), 0x08B8542Cu);
                ++used;
            }
            (void)call(rt, ctx, 0x08A5B9ECu, {key_string});
            ++index;
        } while (entries != index);
    }
    object = word(rt, frame.at(0x14u));
    if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08C9C42Cu});
    const u32 spare = frame.at(4u);
    const u32 vector = object + 0x14u;
    (void)call(rt, ctx, 0x08A76AA0u, {spare});
    const u32 size = static_cast<u32>(signed_of(word(rt, vector + 4u) - word(rt, object + 0x14u)) >> 2);
    const u32 first = word(rt, object + 0x14u);
    if (used < size) {
        (void)call(rt, ctx, 0x08B85798u, {vector, first + (used << 2), word(rt, object + 0x18u)});
    } else {
        (void)call(rt, ctx, 0x08B85F30u, {vector, word(rt, object + 0x18u), used - size, spare});
    }
    (void)call(rt, ctx, 0x08A5B9ECu, {spare});
    if (out != list) (void)call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x14u))});
    (void)call(rt, ctx, 0x08A5B9ECu, {list});
    release_wide_string(rt, ctx, word(rt, frame.at(0x10u)), 0x08D74FD0u, frame.at(0u), 0x08B8542Cu);
}

} // namespace
} // namespace new_batch_main_994
} // namespace gt::game

#define NEW994(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_994::name, gt::ReturnKind::kind)
NEW994(0x088414ACu, func_088414AC, Word);
NEW994(0x08883010u, func_08883010, Word);
NEW994(0x08B472ACu, func_08B472AC, Word);
NEW994(0x08C4D7D4u, func_08C4D7D4, Word);
NEW994(0x08C6FA80u, func_08C6FA80, Word);
