// New batch 873 (main session): three hand-written drafts from `tools/disasm`, unverified here.
//   0x089419D8: fills a 0x60-byte motion record from a config struct and a float scale: the primary rate by the mode,
//               then two byte vectors (0x089416D4) or the six-output helper 0x0895EE04 into a local frame block, two
//               passes of 0x08941750, and the four floats go out either as plain copies or through 0x08941784 behind
//               the global flag at 0x08CDE655; v0 = the config's byte at +2 (the asm's last `lbu $v0`).
//   0x08BA86F4: inserts `count` copies of one 8-byte {word, refcounted pointer} element into the vector at a0 at
//               `position`: shifts the tail through 0x08BA7FD4, fills the gap through 0x08BA7880, or grows the buffer
//               through 0x08B35DA0 + 0x08BA7F84 / 0x08BA7F34; the value's local copy is refcounted and released.
//               v0 is untouched when count is 0, otherwise the release's answer (1, or the last virtual's) — or the grown
//               buffer's end on the reallocation path.
//   0x0888D9A8: updates one request entry of the object's id tables (see update_entry); v0 = the request's byte at +5,
//               or 0x088936DC(self[+0x18])'s answer when it is set (the earlier draft's 0x089936DC was a typo and
//               faulted: "invalid internal function entry").
// Audits: `tools.frame_audit` 0 wrong, `tools.literal_check` 0 missing, `tools.arg_reads` 0 short. `tools.sp_loads`
// reports the two `lw s0, 0x10($sp)` of 0x08BA86F4 (0x08BA88B4 / 0x08BA891C) as a callee-saved restore from a slot it
// was not saved to: that slot is the frame's own 0x10 (the value's refcounted pointer, stored at 0x08BA8758), s0 is
// saved at 0x30 and restored at 0x08BA8A9C — the load is the local copy the release takes, not a restore.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_873 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose argument registers are set like the original sets them before its `jal` (a0-a3, t0-t3 in order, so a
// callee that reads one of them sees this call's value); v0 afterwards holds its result, as after the `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same with float arguments ($f12, $f13, ... in order), returning $f0.
float callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
            std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_half(u32 value) { return static_cast<s32>(static_cast<std::int16_t>(value)); }
s32 signed_byte(u32 value) { return static_cast<s32>(static_cast<std::int8_t>(static_cast<std::uint8_t>(value))); }

// ---------------------------------------------------------------------------------------------------------------
// 0x089419D8 (a0 = the 0x60-byte record, a1 = the config, a2 = a level byte, a3 = the mode, t0 = a pointer handed on to
// 0x08941784 as its fifth argument, f12 = a float kept in $f21).
//
// The global byte at 0x08CDE508 + 0x14D picks how the two four-float blocks leave: the plain copy of the branch, or the
// transform 0x08941784 with the caller's t0 constant (0x08CA2288 / 0x08CA2290) as its extra argument.
constexpr u32 kMotionGlobal = 0x08CDE508u;

// 0x089416D4: four config bytes (t0-t3) become the block's four floats at sp+0, sp+8, sp+4, sp+0xC.
void bytes_into_block(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 source) {
    call(rt, ctx, 0x089416D4u,
         {frame.at(0u), frame.at(8u), frame.at(4u), frame.at(0xCu), byte_at(rt, source), byte_at(rt, source + 1u),
          byte_at(rt, source + 2u), byte_at(rt, source + 3u)});
}

// 0x08941BB0 / 0x08941C74: the four floats of the frame block, copied straight to `destination` when the flag is set,
// else handed to 0x08941784 (destination, destination + 4, destination + 8, `extra`, and `constant` in t0).
void place_block(Runtime &rt, AllegrexContext &ctx, u32 destination, const GuestFrame &frame, u32 constant, u32 extra) {
    if (byte_at(rt, kMotionGlobal + 0x14Du) != 0u) {
        put_float(rt, destination, float_at(rt, frame.at(0u)));
        put_float(rt, destination + 4u, float_at(rt, frame.at(8u)));
        put_float(rt, destination + 8u, float_at(rt, frame.at(4u)));
        put_float(rt, destination + 0xCu, float_at(rt, frame.at(0xCu)));
        return;
    }
    callf(rt, ctx, 0x08941784u, {destination, destination + 4u, destination + 8u, extra, constant},
          {float_at(rt, frame.at(0u)), float_at(rt, frame.at(8u)), float_at(rt, frame.at(4u)),
           float_at(rt, frame.at(0xCu))});
}


// ---------------------------------------------------------------------------------------------------------------
// 0x08BA86F4 (a0 = the vector {begin, end, capacity}, a1 = position, a2 = count, a3 = the value {word +0, refcounted
// pointer +4}). Elements are eight bytes: a word and an intrusive pointer whose control object holds the strong count
// at +4, the weak count at +8 and the vtable at +0 (slots +8 = dispose, +0xC = free).
constexpr u32 kVectorMaxSize = 0x1FFFFFFFu;

// One `if (p != other.p)` assignment of the element's pointer (0x08BA8800-0x08BA8884): bump the new one, drop the old
// one and, when its strong count reaches zero, run dispose and then (weak count zero) free through the vtable.
// Returns the v0 the original leaves: 1 after a decrement that did not free, the last virtual's answer otherwise, or
// `carry` when the pointer was null (nothing was touched).
u32 release_pointer(Runtime &rt, AllegrexContext &ctx, u32 object, u32 carry) {
    if (object == 0u) return carry;
    const u32 strong = word(rt, object + 4u);
    put(rt, object + 4u, strong - 1u);
    if (strong != 1u) return 1u;  // 0x08BA8938: v0 = 1 before the compare
    const u32 disposed = call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    const u32 weak = word(rt, object + 8u);
    put(rt, object + 8u, weak - 1u);
    if (weak != 1u) return 1u;
    return call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// 0x08BA898C: no room for `count` more elements — grow to size + max(size, count), move the head, insert, move the tail.
u32 grow_and_insert(Runtime &rt, AllegrexContext &ctx, u32 self, u32 position, u32 count, u32 value,
                    const GuestFrame &frame) {
    const u32 size = static_cast<u32>(static_cast<s32>(word(rt, self + 4u) - word(rt, self)) >> 3);
    if (kVectorMaxSize - size < count) call(rt, ctx, 0x08B5ADDCu, {0x08CA0718u});  // the original throws here
    const u32 room = size < count ? count : size;  // movz: max(size, count)
    const u32 new_size = size + room;
    u32 bytes = 0xFFFFFFF8u;  // s4: the overflow path hands -8 on, exactly as the original does
    u32 buffer = 0u;          // s3
    if (new_size < size) {
        buffer = call(rt, ctx, 0x08B35DA0u, {bytes});
    } else {
        bytes = new_size << 3;
        if (new_size != 0u) buffer = call(rt, ctx, 0x08B35DA0u, {bytes});
    }
    const u32 begin = word(rt, self);
    const u32 moved = call(rt, ctx, 0x08BA7F84u, {begin, position, buffer, frame.at(3u)});
    const u32 inserted = call(rt, ctx, 0x08BA7F34u, {moved, count, value, frame.at(2u)});
    const u32 rest = call(rt, ctx, 0x08BA7F84u, {position, word(rt, self + 4u), inserted + count * 8u, frame.at(1u)});
    call(rt, ctx, 0x08BA76B8u, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
    const u32 old = word(rt, self);
    if (old != 0u) call(rt, ctx, 0x08B35254u, {old});
    put(rt, self + 8u, buffer + bytes);
    put(rt, self + 4u, rest);
    put(rt, self, buffer);
    return buffer + bytes;  // 0x08BA8A5C: v0 = s3 + s4
}


// ---------------------------------------------------------------------------------------------------------------
// 0x0888D9A8 (a0 = the object, a1 = handed to 0x088D62FC, a2 = the request). One request entry of the object's table at
// +0x34 (168 bytes each, entries at +0xD78; the ids at +0x1018; counters 432 bytes each at +0x1560): the state is
// copied out to a stack block at sp+0x10, patched from the request, run through the id lookups, and copied back.
// sp+0..sp+8 is a three-word argument block. `fresh` is the request's byte at +5.
void update_entry(Runtime &rt, AllegrexContext &ctx, u32 self, u32 argument, u32 request, u32 base) {
    const u32 buffer = base + 0x10u;                 // s7
    const s32 index = signed_half(half_at(rt, request));  // s2
    // 0x0888D9F4: the state comes out of the table entry into the stack block.
    call(rt, ctx, 0x0888A878u, {buffer, word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0xD78u});
    // 0x0888DA20: patch the block: a flag byte, the request's halfword, a count, and the counter byte.
    const s32 byte4 = signed_byte(byte_at(rt, request + 4u));
    const s32 byte5 = signed_byte(byte_at(rt, request + 5u));
    const u32 half2 = half_at(rt, request + 2u);
    const u32 old_counter = byte_at(rt, buffer + 0x13u);
    const u32 fresh = byte5 != 0 ? 1u : 0u;  // s4: `sltu s4, zero, v0`
    u32 counter_byte = (static_cast<u32>(byte4) + 1u) & 0xFFu;
    if (fresh != 0u) counter_byte = 0u;  // movn
    put8(rt, buffer + 0x11u, 1u);
    const u32 count = old_counter + 1u;  // a0
    put8(rt, buffer + 0x13u, counter_byte);
    put_half(rt, buffer + 0x14u, half2);
    put8(rt, buffer + 0x12u, fresh);
    // 0x0888DA54: a new request carries its own 64-bit pair, an old one keeps the block's.
    u32 pair_low;
    u32 pair_high;
    if (fresh != 0u) {
        pair_low = word(rt, request + 8u);
        pair_high = static_cast<u32>(static_cast<s32>(pair_low) >> 31);
    } else {
        pair_low = word(rt, buffer + 0x20u);
        pair_high = word(rt, buffer + 0x24u);
    }
    put(rt, buffer + 0x20u, pair_low);
    put(rt, buffer + 0x24u, pair_high);
    // 0x0888DA90: `count` words from the request's +0x10 and `count` halfwords from its +0x50 into the block.
    if (static_cast<s32>(count) > 0) {
        for (u32 i = 0u; i < count; ++i) {
            put(rt, buffer + 0x28u + i * 4u, word(rt, request + 0x10u + i * 4u));
            put(rt, buffer + 0x68u + i * 4u, static_cast<u32>(signed_half(half_at(rt, request + 0x50u + i * 2u))));
        }
    }
    // 0x0888DAB4: the entry's id, then either the state it already had (a new request) or its stored one.
    const u32 moved = call(rt, ctx, 0x088DA788u, {word(rt, self + 4u), static_cast<u32>(index)});  // s3
    if (fresh != 0u) {
        const u32 entry = word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0x1018u;
        // 0x0888DAE8: the stored pair decides whether the request replaces it (the byte at +0x1C35 flips the sense).
        if (byte_at(rt, entry + 0x11u) != 0u) {
            const u32 stored_low = word(rt, entry + 0x20u);
            const u32 stored_high = word(rt, entry + 0x24u);
            bool replace;
            if (stored_low == pair_low && stored_high == pair_high) {
                replace = false;  // the same pair: nothing to write back
            } else {
                bool flag;  // a1
                if (pair_high < stored_high) flag = true;
                else if (pair_high != stored_high) flag = false;
                else flag = pair_low < stored_low;
                replace = byte_at(rt, word(rt, self + 0x34u) + 0x1C35u) != (flag ? 1u : 0u);
            }
            if (replace) {
                call(rt, ctx, 0x0888A878u, {entry, buffer});
                call(rt, ctx, 0x0888AF70u, {word(rt, self + 0x34u), buffer});
            }
        }
        // 0x0888DB48: the argument block, then the update of the moved entry (s3).
        put(rt, base, static_cast<u32>(signed_half(half_at(rt, request))));
        put(rt, base + 4u, word(rt, request + 0x70u));
        put(rt, base + 8u, static_cast<u32>(signed_half(half_at(rt, request + 2u))) + 1u);
        const u32 holder = word(rt, self + 4u);
        put(rt, moved + 0x38u, call(rt, ctx, 0x088D2774u, {word(rt, holder + 0x618u), base, holder}));
        call(rt, ctx, 0x088D0934u, {moved, static_cast<u32>(byte4), word(rt, request + 0x70u)});
        call(rt, ctx, 0x088D097Cu, {moved, word(rt, request + 8u)});
        put(rt, moved + 0x194u, word(rt, request + 0x70u));
        put(rt, moved + 0x70u, word(rt, request + 0x70u));
        // 0x0888DBB4: the counter by index, wrapped to a halfword; its remainder modulo 100 picks the slot at +0x1C.
        const u32 table = word(rt, self + 0x34u) + static_cast<u32>(index) * 432u + 0x1560u;
        const u32 wrapped = static_cast<u32>(signed_half(half_at(rt, table + 2u) + half_at(rt, request + 2u)));
        put_half(rt, table + 4u, wrapped);
        const s32 slot = static_cast<s32>(wrapped) - 1;
        put(rt, table + 0x1Cu + static_cast<u32>(slot % 100) * 4u, pair_low);
    } else {
        // 0x0888DC1C: an old request only fills the argument block and refreshes the moved entry once.
        put(rt, base, static_cast<u32>(index));
        put(rt, base + 4u, word(rt, request + 0x70u));
        put(rt, base + 8u, static_cast<u32>(signed_half(half_at(rt, request + 2u))));
        const u32 holder = word(rt, self + 4u);
        put(rt, moved + 0x38u,
            call(rt, ctx, 0x088D26B4u, {word(rt, holder + 0x618u), base, static_cast<u32>(byte4)}));
        call(rt, ctx, 0x088D0934u, {moved, static_cast<u32>(byte4), word(rt, request + 0x70u)});
    }
    // 0x0888DC58: the block goes back, then the two id lookups must agree before the last call.
    call(rt, ctx, 0x0888A878u, {word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0xD78u, buffer});
    call(rt, ctx, 0x0888AD94u, {word(rt, self + 0x34u), static_cast<u32>(index), buffer});
    if (call(rt, ctx, 0x088D62FCu, {argument}) != 0u) return;
    if (call(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), static_cast<u32>(index)}) != 1u) return;
    // 0x0888DCC4: the answer pair is spilled to the frame (sp+0 and sp+4, and the block's +0xB0 / +0xB4) and its high
    // word decides whether the last call runs.
    const GuestResult pair = call_guest(rt, ctx, 0x0888CAFCu,
                                        {self, static_cast<u32>(index), fresh, 1u}, {});
    ctx.gpr[2] = pair.v0;
    ctx.gpr[3] = pair.v1;
    put(rt, base, pair.v0);
    put(rt, base + 4u, pair.v1);
    put(rt, buffer + 0xB0u, pair.v0);
    put(rt, buffer + 0xB4u, pair.v1);
    u32 selected;
    if (fresh != 0u) {
        selected = word(rt, word(rt, word(rt, self + 4u) + 0x618u) + 0x20Cu);
    } else {
        const u32 slot_index = word(rt, moved + 0x2Cu);
        selected = word(rt, word(rt, word(rt, self + 4u) + 0x618u) + slot_index * 12u);
    }
    if (call(rt, ctx, 0x08918438u, {word(rt, self + 8u), selected, 0u}) == 0u) return;
    if (pair.v1 != 0u) return;
    call(rt, ctx, 0x0888B24Cu, {self, 0u, 0u, base});
}

// 0x0888DD40: v0 = the request's byte at +5 (sign-extended); when it is set, 0x088936DC(self->+0x18) answers instead.
void finish_request(Runtime &rt, AllegrexContext &ctx, u32 self, u32 request) {
    const u32 flag = static_cast<u32>(signed_byte(byte_at(rt, request + 5u)));
    ctx.gpr[2] = flag;
    if (flag != 0u) ctx.gpr[2] = call(rt, ctx, 0x088936DCu, {word(rt, self + 0x18u)});
}

void func_0888D9A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];      // s1
    const u32 argument = ctx.gpr[5];  // fp
    const u32 request = ctx.gpr[6];   // s0
    GuestFrame frame(ctx, 0x100u);
    const u32 table = word(rt, self + 0x34u);  // v1
    if (table != 0u && byte_at(rt, table + 0x1C34u) != 0u) {
        update_entry(rt, ctx, self, argument, request, frame.at(0u));
    }
    finish_request(rt, ctx, self, request);
}

} // namespace
} // namespace new_batch_873
} // namespace gt::game


#define NEW873(address, name, kind) GT_DECOMP(address, gt::game::new_batch_873::name, gt::ReturnKind::kind)
NEW873(0x0888D9A8u, func_0888D9A8, Word);
