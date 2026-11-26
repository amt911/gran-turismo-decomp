// Main-session batch 1050: two long functions earlier drafts left unmatched. Written from `tools.disasm` one at a time;
// both are plain "walk the children of an object through its vtable" code with no unusual instruction shapes (no
// setjmp, no ll/sc), so they are written out in full. Role words in the comments describe access shapes; none is a
// game name.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15):
// `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value; `call_result`
// also passes the float registers. The callee-saved registers are never written here (the original restores them
// before it returns), so there is no SavedRegs. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` /
// `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a
// non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on
// every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in
// the original).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1050 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order,
// f12-f19 for the floats), the callee's v0 / v1 / f0 left in the context afterwards.
inline GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {},
                               std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float value : floats) {
        if (freg <= 19u) ctx.fpr[freg++] = value;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, address, arguments, {}).v0;
}
// `lw v1, slot(a0)` / `lw v0, offset(v1)` / `jalr v0`: a callee that returns without writing v0 hands its own address
// on, so v0 and v1 are set the way the original sets them before the jalr.
inline GuestResult call_virtual_result(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                                       std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_result(rt, ctx, function, arguments, {});
}
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    return call_virtual_result(rt, ctx, object, offset, arguments).v0;
}
// A virtual call whose only argument is $f12 (the original loads the constant into f12 and a0 = the object).
inline void call_slot_float(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, float value) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    (void)call_result(rt, ctx, function, {object}, {value});
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline s32 byte_s(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The release of a counted string cell, inlined at several places by the original: nothing happens when the data
// pointer 0xC below `data` is the shared empty cell 0x08D74FD0, otherwise the count at data - 4 is decremented
// (func_08B60804) and, when that answer is not positive, func_08B8542C(data - 0xC, flag) disposes of it.
inline void release_cell(Runtime &rt, AllegrexContext &ctx, u32 data, u32 flag) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the beq
    if (data - 0x0Cu == 0x08D74FD0u) return;
    const u32 references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0x0Cu;
    ctx.gpr[5] = flag;  // the delay slot of the bgtz
    if (static_cast<s32>(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0x0Cu, flag});
}
// The `bltz` / `slti 0x100` / `div.s 255.0` sequence at the end of every channel read: a signed byte-shaped answer
// scaled into 0..1 (negative answers 0, 0x100 and above 255/255 = 1).
inline float unit_channel(s32 value) {
    if (value < 0) return 0.0f;
    if (value >= 0x100) return 255.0f / 255.0f;
    return static_cast<float>(value) / 255.0f;
}

// func_08964B48(a0 unused, mode = a1, holder = a2): `mode` other than 1 returns 1 at once. Otherwise the object at
// holder[0] (a null one is reported through func_08A5BB7C(0x08CA230C) and then read anyway, as the original) answers
// its virtual slot 0x7C, a count that func_089FB3A4 is handed; a count of zero or less ends the function with that
// call's answer. For each index 0..count-1: slot 0x84 (sp+0x38, object, index) makes a counted pointer to a child, its
// slot 0x84 (sp+0x34, child, 0) a grandchild whose slot 0x0C (sp+0x3C, grandchild) makes the per-index object that is
// kept, the sp+0x34 temporary is released and slot 0x84 (sp+0x30, child, 1) plus slot 0x78 of that answer pick the way
// the per-index payload is filled:
//   - slot 0x78 answers 0: four channel reads (slot 0x84 at sp+0x2C/0x28/0x24/0x20 with indices 1..4, each object
//     answering slot 0x3C, scaled by 1/255) go to the frame at sp+0x90 and, with a -1 word at sp+0xA0,
//     func_089FAFD4(index, sp+0x3C, sp+0x90) stores them;
//   - otherwise a 4 x 0x20-byte block is built (func_08B35840(0x10, 0x80) filled by func_08BE6028 with the value at
//     sp+0x50 = four 1.0 floats and -1) and for each of the four indices 1..4 a grandchild (slot 0x84 at sp+0x40,
//     sp+0x4, sp+0x8, sp+0xC) is asked: slot 0x78 zero copies the sp+0x70 structure built by func_08A0B0CC from the
//     sp+0x18 temporary (its +0x10 word comes from sp+0x80) into the element with func_089641C4, otherwise the four
//     slot-0x3C channel values are scaled into the element's floats; the temporary at sp+0x40 is released each time.
//     Then func_089FB48C(index, sp+0x3C, sp+0x90), func_08BE5888(begin, end, sp+1) and func_08B35254(begin) end it.
// Each index ends with the release of the sp+0x3C object and func_08A5B9EC(sp+0x38); v0 = a2 when the count is used up.
// Branches the compared routes may not take (checked against the asm by hand): the null reports, the count == 0 exit
// and the slot-0x78-zero payload branch.
void func_08964B48(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    put(rt, frame.at(0xB0u), holder);  // the delay slot of the mode test
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const auto report = [&]() { (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA230Cu}); };
    std::uint32_t object = word(rt, holder);
    if (object == 0u) report();
    const u32 count = call_virtual(rt, ctx, object, 0x7Cu, {object});
    const u32 prepared = call_set(rt, ctx, 0x089FB3A4u, {count});
    if (static_cast<s32>(count) <= 0) {
        ctx.gpr[2] = prepared;
        return;
    }
    const u32 kept = frame.at(0x3Cu);   // sp + 0x3C: the per-index object
    const u32 shaped = frame.at(0x70u); // sp + 0x70: the structure func_08A0B0CC fills
    // One channel: a counted pointer at sp+`slot` to child `index`, whose slot 0x3C is scaled and stored at `dest`
    // (the store sits in the release's delay slot).
    const auto channel = [&](u32 slot, u32 index_of_child, u32 dest) {
        u32 child = word(rt, frame.at(0x38u));
        if (child == 0u) report();
        (void)call_virtual(rt, ctx, child, 0x84u, {frame.at(slot), child, index_of_child});
        child = word(rt, frame.at(slot));
        if (child == 0u) report();
        const u32 value = call_virtual(rt, ctx, child, 0x3Cu, {child});
        put_float(rt, dest, unit_channel(static_cast<s32>(value)));
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(slot)});
    };
    u32 index = 0u;
    for (;;) {
        object = word(rt, holder);
        if (object == 0u) report();
        (void)call_virtual(rt, ctx, object, 0x84u, {frame.at(0x38u), object, index});
        std::uint32_t child = word(rt, frame.at(0x38u));
        if (child == 0u) report();
        (void)call_virtual(rt, ctx, child, 0x84u, {frame.at(0x34u), child, 0u});
        child = word(rt, frame.at(0x34u));
        if (child == 0u) report();
        (void)call_virtual(rt, ctx, child, 0x0Cu, {kept, child});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x34u)});
        child = word(rt, frame.at(0x38u));
        if (child == 0u) report();
        (void)call_virtual(rt, ctx, child, 0x84u, {frame.at(0x30u), child, 1u});
        child = word(rt, frame.at(0x30u));
        if (child == 0u) report();
        const u32 present = call_virtual(rt, ctx, child, 0x78u, {child});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x30u)});
        if (present == 0u) {
            put(rt, frame.at(0xA0u), 0xFFFFFFFFu);
            put_float(rt, frame.at(0x9Cu), 1.0f);
            put_float(rt, frame.at(0x98u), 1.0f);
            put_float(rt, frame.at(0x94u), 1.0f);
            put_float(rt, frame.at(0x90u), 1.0f);
            channel(0x2Cu, 1u, frame.at(0x90u));
            channel(0x28u, 2u, frame.at(0x94u));
            channel(0x24u, 3u, frame.at(0x98u));
            channel(0x20u, 4u, frame.at(0x9Cu));
            (void)call_set(rt, ctx, 0x089FAFD4u, {index, kept, frame.at(0x90u)});
        } else {
            put(rt, frame.at(0x90u), 0u);
            u32 step = 0u;    // s3
            u32 offset = 0u;  // s2, the element's byte offset in the block
            put(rt, frame.at(0x94u), 0u);
            const u32 block = call_set(rt, ctx, 0x08B35840u, {0x10u, 0x80u});
            put(rt, frame.at(0x98u), block + 0x80u);
            put(rt, frame.at(0x90u), block);
            put(rt, frame.at(0x94u), block);
            put(rt, frame.at(0x60u), 0xFFFFFFFFu);
            put_float(rt, frame.at(0x5Cu), 1.0f);
            put_float(rt, frame.at(0x58u), 1.0f);
            put_float(rt, frame.at(0x54u), 1.0f);
            put_float(rt, frame.at(0x50u), 1.0f);  // the delay slot of func_08BE6028 reads this structure
            (void)call_set(rt, ctx, 0x08BE6028u, {block, 4u, frame.at(0x50u), frame.at(0x3u)});
            put(rt, frame.at(0x94u), word(rt, frame.at(0x90u)) + 0x80u);
            // The same channel read, one element of the block: child = sp+0x40's object.
            const auto element_channel = [&](u32 slot, u32 index_of_child, u32 dest) {
                u32 grand = word(rt, frame.at(0x40u));
                if (grand == 0u) report();
                (void)call_virtual(rt, ctx, grand, 0x84u, {frame.at(slot), grand, index_of_child});
                grand = word(rt, frame.at(slot));
                if (grand == 0u) report();
                const u32 value = call_virtual(rt, ctx, grand, 0x3Cu, {grand});
                put_float(rt, dest, unit_channel(static_cast<s32>(value)));
                (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(slot)});
            };
            for (;;) {
                u32 grand = word(rt, frame.at(0x38u));
                if (grand == 0u) report();
                ++step;
                (void)call_virtual(rt, ctx, grand, 0x84u, {frame.at(0x40u), grand, step});
                u32 element = word(rt, frame.at(0x40u));
                if (element == 0u) report();
                (void)call_virtual(rt, ctx, element, 0x84u, {frame.at(0x1Cu), element, 0u});
                u32 leaf = word(rt, frame.at(0x1Cu));
                if (leaf == 0u) report();
                const u32 present_element = call_virtual(rt, ctx, leaf, 0x78u, {leaf});
                (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
                const u32 base = offset + word(rt, frame.at(0x90u));
                element = word(rt, frame.at(0x40u));
                if (present_element == 0u) {
                    if (element == 0u) report();
                    (void)call_virtual(rt, ctx, element, 0x84u, {frame.at(0x14u), element, 0u});
                    leaf = word(rt, frame.at(0x14u));
                    if (leaf == 0u) report();
                    (void)call_virtual(rt, ctx, leaf, 0x0Cu, {frame.at(0x18u), leaf});
                    (void)call_set(rt, ctx, 0x08A0B0CCu, {shaped, frame.at(0x18u)});
                    if (base != shaped) {
                        (void)call_set(rt, ctx, 0x089641C4u, {base, shaped});
                        put(rt, base + 0x10u, word(rt, frame.at(0x80u)));
                    }
                    release_cell(rt, ctx, word(rt, frame.at(0x18u)), frame.at(2u));
                    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
                } else {
                    element_channel(0x10u, 0u, base);
                    element_channel(0x0Cu, 1u, base + 4u);
                    element_channel(0x08u, 2u, base + 8u);
                    element_channel(0x04u, 3u, base + 0xCu);
                }
                (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x40u)});
                if (step == 4u) break;
                offset += 0x20u;  // the delay slot of the loop's bne
            }
            (void)call_set(rt, ctx, 0x089FB48Cu, {index, kept, frame.at(0x90u)});
            (void)call_set(rt, ctx, 0x08BE5888u, {word(rt, frame.at(0x90u)), word(rt, frame.at(0x94u)), frame.at(1u)});
            const u32 spare = word(rt, frame.at(0x90u));
            if (spare != 0u) (void)call_set(rt, ctx, 0x08B35254u, {spare});
        }
        release_cell(rt, ctx, word(rt, kept), frame.at(0u));
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x38u)});
        ++index;
        if (index == count) break;
    }
    ctx.gpr[2] = holder;  // the delay slot of the loop's bne
}

// func_08911FAC(self = a0, holder = a1, source = a2): the initialiser of `self`, under the lock `self`
// (func_08B344E0 / func_08B3457C). A non-zero byte at self+0x28 means it is already initialised: only the unlock runs.
// Otherwise the byte slot 0x38 of `source` goes to self+0x2B; its slot 0x110 fills a frame structure at sp+0x24 whose
// two floats (+0x28, +0x2C) and two unsigned halfwords (+0x30, +0x32) become self+0x198..0x1A4 with 1.0 at self+0x1A8.
// Slot 0x88 of `source` picks between two table/pose pairs ((2 or 3: 0x08CF1BFC and +1.25) else (0x08CF1BF0 and
// -1.25)) for the structures at self+0x7B4 ({0, 0.5, tilt}), self+0x7C0 (three floats of that table), self+0x7CC
// ({0, 0.25, 2.25}) and self+0x7D8 ({0, 0, 1.0}), and sets the count byte self+0x30 to 1.
// Slot 0xAC of `source` answers n: for i in 0..n-1, slot 0xB0 (source, sp+0x0C, i) fills six floats at sp+0x0C and the
// running minima / maxima over them (0x0C low+high, 0x10 high, 0x14 high, 0x18 low+high, 0x1C high, 0x20 high) are
// written as centers and extents: self+0x7CC = (low+high)/2 of 0x0C, self+0x7D4 = high of 0x14, self+0x7D0 = high of
// 0x10, and the vector ((low+high)/2 of 0x18, high of 0x1C, high of 0x20) at sp is normalised (func_08B4A720 answers
// its length, 1/sqrt(length) via func_08B4A5E8) into self+0x7D8.
// The byte at self+0x30 being 1 gives (self+0x11C, self+0x120) = (1.0, 0.0), anything else (1/sqrt2, 1/sqrt2);
// self+0x190 / 0x130 / 0x148 / 0x160 / 0x178 = 0.7. Then a run of "find a child by name and pose it": 
// func_08911ED8(holder, self+0xD4, self+0x44) then four float-argument virtual calls (slots 0x18, 0x1C, 0x20, 0x2C with
// 0.5, 180, 270, 2) when the found object is there; for the count loop (self+0xD0 + 8 * i, 0 <= i < self+0x30) the
// found child gets 0.31, 135, 225 and 2; the byte at self+0x31 indexes a two-float table at 0x08CF1B10 into
// self+0x114 / self+0x118 after func_08911ED8(holder, self+0xE4, self+0x8C); the found object there gets slot 0x2C
// with 0.5. Slot 0xF0 of `source` gives x: self+0x7B0 = func_08B84954(func_08B847A8(x*0.02) * 0.3 * ln(10)) /
// ((x*0.02)^2 * float(sp+0x34)), and then four name lookups fill self+0xEC, self+0xF4, self+0xFC and self+0x104
// (0.08CA19E8, 0x08CA19F0, 0x08CA19FC, 0x08CA1A04), each found object posed 0.5 / 90 / 270 (the last one 0.31 / 135 /
// 225), and func_08911D88(holder, self+0x10C).
// Finally, four times (self+0x800 + 0x40 * i): the word at +0x38 is cleared, func_08911D88 fills the counted pointers
// at +0x30, +0x08, +0x18 and +0x28, and the objects there are posed (0x18 = 0.31 each time; 0x1C / 0x20 / 0x2C are
// 45 / 180 / 0.25, 45 / 180 / 1.0 and 90 / 180 / 2.0 for the +0x08, +0x18 and +0x28 ones). Then self+0x28 = 1,
// self+0x1AC = 0 and the unlock ends it; v0 is the unlock's answer.
// Branches the compared routes may not take (checked against the asm by hand): the already-initialised exit, the null
// found objects (the four that would be posed are tested), a non-positive slot-0xAC count and the `count > 0` path of
// self+0x30 (skipped when the byte is not positive).

} // namespace
} // namespace new_batch_main_1050
} // namespace gt::game

#define NEW1050(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1050::name, gt::ReturnKind::kind)
NEW1050(0x08964B48u, func_08964B48, Word);
