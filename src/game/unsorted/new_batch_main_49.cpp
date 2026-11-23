// New batch, main session 49: five functions the recompiler emitted, none decompiled before, written by hand as
// structured C from `tools.disasm`.
//
//  0x08A2E8CC  registers 45 methods on an object (a vtable slot builds the frame value first, then one call per method).
//  0x08C41D90  reads one record of a byte-code stream: a 0x49-entry jump table, each opcode advancing the cursor and
//              storing a value into a two-word block; returns 0 at the end-of-stream opcode and 1 otherwise.
//  0x0888EE48  loads a record set into an object: a parser on the frame, a contiguous 0x178-byte record copy (some of
//              its words byte-swapped twice, i.e. unchanged), a big-endian count that allocates and fills 0x20-byte
//              records, and four slots with their 8- and 0x30-byte blocks.
//  0x0881FEF4  walks the scene behind an object and fills a block: a 16-byte value through the object's vtable, a
//              0x3C-byte parse, two lookups through the block's +4 / +8 words, then per element an identifier pair and a
//              float into 0x0881FB28; the frame's smart pointers are released at the end.
//  0x0895DEC8  the gear / assist state machine of a car (word(a0+8)) with its state block (word(a0+0xC)): the countdown
//              at +0x512, the speed bands, the angle the next gear needs, and the four-slot counter search; result in v0.
//
// v0 is left as the original leaves it on every path: the last call's result where the original ends with a `jal`, the
// record code in 0x08C41D90, and the value the original leaves in a0 in 0x0895DEC8. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_49 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments).v0;
}
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats) {
    return call_result(rt, ctx, target, arguments, floats).v0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }
// The original reads three words of payload as four bytes each, most significant byte first.
u32 big_endian_word(Runtime &rt, u32 address) {
    return (byte_at(rt, address) << 24) | (byte_at(rt, address + 1u) << 16) | (byte_at(rt, address + 2u) << 8) |
           byte_at(rt, address + 3u);
}
// `lui $a1, 0x2` before the free of the old holder in 0x08A2E8CC: the allocation mode 0x20000, built in one step.
constexpr u32 kAllocateMode2 = 0x2u << 16;

// --- 0x08A2E8CC: a0 = the object. The vtable's slot 0x108 builds a value into a 4-byte stack slot from the name table
// 0x08CAC3BC (a byte goes to +1); a non-zero word at +4 is released (unless it is the static 0x08D74FD0 block), the new
// holder from 0x08A76A94 replaces it at +0x38, 0x08B8850C sets the name, and every method is registered.
//
// The 45 registrations: (name in the string table, handler).
constexpr u32 kRegistrations[][2] = {
    {0x08CAC3C8u, 0x08A2E410u}, {0x08CAC3DCu, 0x08A2EDE4u}, {0x08CAC3E8u, 0x08A2DFCCu}, {0x08CAC3F4u, 0x08A2EEDCu},
    {0x08CAC40Cu, 0x08A2E39Cu}, {0x08CAC424u, 0x08A2F1C8u}, {0x08CAC440u, 0x08A2E328u}, {0x08CAC45Cu, 0x08A2DFD8u},
    {0x08CAC468u, 0x08A2DFE4u}, {0x08CAC474u, 0x08A2F29Cu}, {0x08CAC484u, 0x08A2F34Cu}, {0x08CAC494u, 0x08A2F3FCu},
    {0x08CAC49Cu, 0x08A2F494u}, {0x08CAC4ACu, 0x08A2E264u}, {0x08CAC4C0u, 0x08A2E128u}, {0x08CAC4D0u, 0x08A2F4E8u},
    {0x08CAC4E0u, 0x08A2F530u}, {0x08CAC4F0u, 0x08A2DFF8u}, {0x08CAC504u, 0x08A2E000u}, {0x08CAC518u, 0x08A2DFF0u},
    {0x08CAC524u, 0x08A2F5B0u}, {0x08CAC530u, 0x08A2F628u}, {0x08CAC544u, 0x08A2F6A0u}, {0x08CAC550u, 0x08A2F750u},
    {0x08CAC560u, 0x08A2E008u}, {0x08CAC570u, 0x08A2E014u}, {0x08CAC580u, 0x08A2E660u}, {0x08CAC58Cu, 0x08A2E6D4u},
    {0x08CAC5A8u, 0x08A2E0A8u}, {0x08CAC5B4u, 0x08A2F800u}, {0x08CAC5BCu, 0x08A2F858u}, {0x08CAC5C8u, 0x08A2F8A8u},
    {0x08CAC5DCu, 0x08A2DF3Cu}, {0x08CAC5F0u, 0x08A2FB84u}, {0x08CAC608u, 0x08A2FE90u}, {0x08CAC624u, 0x08A3013Cu},
    {0x08CAC640u, 0x08A30418u}, {0x08CAC658u, 0x08A30468u}, {0x08CAC678u, 0x08A30594u}, {0x08CAC698u, 0x08A30870u},
    {0x08CAC6ACu, 0x08A308C0u}, {0x08CAC6C4u, 0x08A30918u}, {0x08CAC6E0u, 0x08A2DF60u}, {0x08CAC6F8u, 0x08A2DF84u},
    {0x08CAC710u, 0x08A2DFA8u},
};

void func_08A2E8CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 table = word(rt, self);
    const u32 make = word(rt, table + 0x108u);  // the slot, read before the call (its delay slot)
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CAC3BCu, frame.at(1u)});
    call_indirect(rt, ctx, make, {self, frame.at(4u)});
    const u32 made = word(rt, frame.at(4u));
    if (made - 0xCu != 0x08D74FD0u) {
        // Not the static block: 0x08B60804 says whether the payload may be freed (a count); 0 when it may.
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {made - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {made - 0xCu, frame.at(0u)});
    }
    const u32 holder = call(rt, ctx, 0x08A76A94u);
    if (holder != 0u) call(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call(rt, ctx, 0x08A5B930u, {previous, kAllocateMode2});
    put(rt, self + 0x38u, holder);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08C1AA40u});
    for (const auto &registration : kRegistrations)
        call(rt, ctx, 0x08B87108u, {self, registration[0], registration[1]});
}

// --- 0x08C41D90: a1 = a two-word block the opcode handlers fill, a3 = the object (its +0xC is the case table, its +4 a
// value provider with virtuals at +0x14 / +0x18), t0 = the address of the stream cursor, t1 = a mode. One record:
//   00            end of stream (returns 0)
//   03 04         a byte / a big-endian half into the block's second word
//   3B 3C         the same, advancing further
//   3E            a signed half into the block's first word
//   47            -1 into the first word
//   0B 09 0A 3?   cursor skips
//   08            a 16-byte payload (optionally decoded through the +4 object) and an index pair
//   05            one of the 0x2710/0x2711/0x2712 records selects an entry of the case table
//   36            an entry of the case table, then an index pair
// Every other opcode advances the cursor by 1 and returns 1.
//
// The index pair shared by the 0x08 / 0x36 handlers (08C42160): a signed half at the cursor gives the byte offset.
u32 skip_index_pair(Runtime &rt, u32 address) {
    return address + 2u + signed_half((byte_at(rt, address + 1u) << 8) | byte_at(rt, address));
}

void func_08C41D90(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[5];
    const u32 mode = ctx.gpr[9];
    const u32 object = ctx.gpr[7];
    const u32 cursor_pointer = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const u32 table = word(rt, object + 0xCu);
    const u32 start = word(rt, cursor_pointer);
    const u32 opcode = byte_at(rt, start);
    u32 cursor = start + 1u;
    put(rt, cursor_pointer, cursor);
    const auto to = [&](u32 address) {
        cursor = address;
        put(rt, cursor_pointer, cursor);
    };
    u32 result = 1u;
    switch (opcode) {
    case 0u:  // 0x00: end of stream
        result = 0u;
        break;
    case 3u:  // 0x03: a byte into the second word, cursor + 2
        put(rt, out + 4u, byte_at(rt, start + 1u));
        to(start + 2u);
        break;
    case 4u:  // 0x04: a big-endian half into the second word, cursor + 3
        put(rt, out + 4u, (byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u));
        to(start + 3u);
        break;
    case 59u:  // 0x3B: a byte into the second word, cursor + 3
        put(rt, out + 4u, byte_at(rt, start + 1u));
        to(start + 3u);
        break;
    case 60u:  // 0x3C: a big-endian half into the second word, cursor + 5
        put(rt, out + 4u, (byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u));
        to(start + 5u);
        break;
    case 62u:  // 0x3E: a signed half into the first word, cursor + 3
        put(rt, out, signed_half((byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u)));
        to(start + 3u);
        break;
    case 71u:  // 0x47: -1 into the first word
        put(rt, out, 0xFFFFFFFFu);
        break;
    case 11u:  // 0x0B: cursor += 4 + 12 bytes per unit
        to(start + 4u + byte_at(rt, start + 1u) * 0xCu);
        break;
    case 9u:   // 0x09: cursor += 2 + a byte
        to(start + 2u + byte_at(rt, start + 1u));
        break;
    case 10u:  // 0x0A: cursor += 3 + a signed half
        to(start + 3u + signed_half((byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u)));
        break;
    case 8u: {  // 0x08: a 16-byte payload and an index pair
        const u32 payload = frame.at(0u);
        u32 decoded = payload;
        if (byte_at(rt, 0x08CE0A0Du) != 0u) {
            const u32 provider = word(rt, object + 4u);
            if (provider == 0u) {
                for (u32 word_index = 0u; word_index != 3u; ++word_index)
                    put(rt, payload + word_index * 4u, big_endian_word(rt, start + 1u + word_index * 4u));
            } else {
                const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, provider) + 0x14u), {provider});
                if (answer != 0u) {
                    decoded = answer;
                } else {
                    for (u32 word_index = 0u; word_index != 3u; ++word_index)
                        put(rt, payload + word_index * 4u, big_endian_word(rt, start + 1u + word_index * 4u));
                }
            }
            // The last word (bytes +0xD..+0x10) and the scale 0x08B5420C are read on this path only; the product is
            // kept in $f12 and never used (the original's dead float tail).
            (void)big_endian_word(rt, start + 0xDu);
            call_result(rt, ctx, 0x08B5420Cu, {decoded, word(rt, 0x08CE0A08u)}, {0.0f});
        }
        to(word(rt, cursor_pointer) + 0x10u);
        const u32 provider = word(rt, object + 4u);
        const u32 index = provider == 0u ? call(rt, ctx, 0x08AF826Cu, {provider})
                                         : call_indirect(rt, ctx, word(rt, word(rt, provider) + 0x18u), {provider});
        const u32 count = byte_at(rt, cursor);
        const u32 chosen = static_cast<s32>(index) < static_cast<s32>(count) ? index : count - 1u;
        to(cursor + 1u + chosen * 2u);
        to(skip_index_pair(rt, cursor));
        break;
    }
    case 5u: {  // 0x05: the 0x2710 / 0x2711 / 0x2712 records select a case-table entry
        const u32 value = signed_half((byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u));
        const u32 limit = byte_at(rt, start + 3u);
        u32 chosen = 0u;
        if (static_cast<s32>(value) >= 0x2710) {
            if (value == 0x2710u) {
                chosen = word(rt, 0x08D472B0u) == 1u ? 1u : (mode == 5u ? 2u : 0u);
            } else if (value == 0x2711u) {
                chosen = (mode == 1u || mode == 4u) ? 1u : (mode == 2u ? 2u : 0u);
            } else if (value == 0x2712u) {
                chosen = word(rt, 0x08D47264u + 4u) == 0u ? 1u : (word(rt, 0x08D47264u) == 1u ? 2u : 0u);
            }
        }
        to(start + 4u);
        if (static_cast<s32>(chosen) < static_cast<s32>(limit)) {
            to(cursor + chosen * 2u);
            to(skip_index_pair(rt, cursor));
        } else {
            to(cursor + limit * 2u);
        }
        break;
    }
    case 54u: {  // 0x36: a case-table entry (a signed word against a byte count), then an index pair
        const u32 entry = word(rt, table + ((byte_at(rt, start + 2u) << 8) | byte_at(rt, start + 1u)) * 4u);
        const u32 limit = byte_at(rt, start + 3u);
        to(start + 4u);
        if (static_cast<s32>(entry) < static_cast<s32>(limit) && static_cast<s32>(entry) >= 0) {
            to(cursor + entry * 2u);
            to(skip_index_pair(rt, cursor));
        } else {
            to(cursor + limit * 2u);
        }
        break;
    }
    case 16u:  // 0x10
    case 17u:  // 0x11: cursor + 0x41
        to(start + 0x41u);
        break;
    case 18u:  // 0x12
    case 19u:  // 0x13: cursor + 0xD
        to(start + 0xDu);
        break;
    case 68u:  // 0x44: cursor + 0x19
        to(start + 0x19u);
        break;
    // The table's advance-only blocks: +3, +2, +5, +0x11, +9, +4; every other opcode leaves the cursor at +1.
    case 2u: case 7u: case 29u: case 45u: case 46u: case 47u:
    case 49u: case 50u: case 51u: case 52u: case 53u: case 65u:  // 0x02 0x07 0x1D 0x2D 0x2E 0x2F 0x31-0x35 0x41
        to(start + 3u);
        break;
    case 6u: case 13u: case 26u: case 34u: case 69u:  // 0x06 0x0D 0x1A 0x22 0x45
        to(start + 2u);
        break;
    case 21u: case 22u: case 23u: case 43u: case 66u: case 67u:  // 0x15 0x16 0x17 0x2B 0x42 0x43
        to(start + 5u);
        break;
    case 20u: case 55u: case 56u:  // 0x14 0x37 0x38
        to(start + 0x11u);
        break;
    case 37u: case 57u: case 58u:  // 0x25 0x39 0x3A
        to(start + 9u);
        break;
    case 72u:  // 0x48
        to(start + 4u);
        break;
    default:  // +1, already stored; the out-of-range opcodes land here too (they return 1 as well)
        break;
    }
    put(rt, cursor_pointer, cursor);
    ctx.gpr[2] = result;
}

// --- 0x0888EE48: a0 = the object, a1 / a2 = the two arguments of the loader. The loader primitive 0x0888FC28 decides
// whether there is anything to read (0: this returns 0), a parser is opened on the frame (0x0888F3CC) and filled from
// the two arguments (0x0888F53C). Its first word goes to the object's +0 (0x08890068's result to the parser's +0x10) and
// the block 0x0888F548 returns is copied to the object: 0x178 bytes at +8 (the record, some of its words byte-swapped
// twice, which is the identity). A byte-swapped count at +0xF0 (big endian) allocates 0x20 bytes each at +0x184 and, from
// the byte-swapped base at +0xF4, copies 8 unaligned words per record and overwrites the third with the byte-swapped
// index. Then three parsers at +0x188 / +0x190 / +0x198, four slots: for each one 0x0888FF7C reports whether it is
// present, an 8-byte block is allocated, registered and filled (0x088902EC), and two 0x30-byte blocks per slot
// (0x08890004 reports, 0x08890330 fills). The object's +0x1D4 becomes 1 and the parser is closed (0x0888F3E0).
void copy_words(Runtime &rt, u32 to, u32 from, u32 count) {
    for (u32 index = 0u; index != count; ++index) put(rt, to + index * 4u, word(rt, from + index * 4u));
}
// lwl / lwr pair at `address` + 3 / + 0: the little-endian word that starts at an unaligned address.
u32 word_unaligned(Runtime &rt, u32 address) {
    return byte_at(rt, address) | (byte_at(rt, address + 1u) << 8) | (byte_at(rt, address + 2u) << 16) |
           (byte_at(rt, address + 3u) << 24);
}

void func_0888EE48(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_argument = ctx.gpr[5];
    const u32 second_argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    if (call(rt, ctx, 0x0888FC28u, {first_argument, second_argument}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (byte_at(rt, self + 0x1D4u) != 0u) call(rt, ctx, 0x0888EA68u, {self});
    call(rt, ctx, 0x0888F3CCu, {frame.at(0u)});
    call(rt, ctx, 0x0888F53Cu, {frame.at(0u), first_argument, second_argument});
    const u32 value = word(rt, frame.at(4u));
    put(rt, self, value);
    const u32 first = call(rt, ctx, 0x08890068u, {frame.at(0u)});
    put(rt, frame.at(0x10u), first);
    const u32 source = call(rt, ctx, 0x0888F548u, {frame.at(0u)});
    copy_words(rt, self + 8u, source, 94u);
    // The record count is stored big-endian: `wsbw $s0, word(+0xF0)`.
    const u32 count = __builtin_bswap32(word(rt, self + 0xF0u));
    put(rt, self + 0x180u, count);
    if (static_cast<s32>(count) <= 0) {
        put(rt, self + 0x184u, 0u);
    } else {
        const u32 records = call(rt, ctx, 0x08B6461Cu, {count << 5u});
        put(rt, self + 0x184u, records);
        const u32 base = __builtin_bswap32(word(rt, self + 0xF4u));
        for (u32 index = 0u; static_cast<s32>(index) < static_cast<s32>(word(rt, self + 0x180u)); ++index) {
            const u32 target = records + index * 0x20u;
            const u32 from = base + index * 0x20u;
            for (u32 word_index = 0u; word_index != 8u; ++word_index)
                put(rt, target + word_index * 4u, word_unaligned(rt, from + word_index * 4u));
            put(rt, target + 8u, __builtin_bswap32(index));
        }
    }
    call(rt, ctx, 0x08890478u, {frame.at(0u), self + 0x188u});
    call(rt, ctx, 0x08890448u, {frame.at(0u), self + 0x190u});
    call(rt, ctx, 0x08890418u, {frame.at(0u), self + 0x198u});
    for (u32 slot = 0u; slot != 4u; ++slot) {
        if (call(rt, ctx, 0x0888FF7Cu, {frame.at(0u), slot}) == 0u) continue;
        const u32 registered = call(rt, ctx, 0x08B62344u, {8u});
        call(rt, ctx, 0x088A79ACu, {registered, 0u});
        put(rt, self + 0x1A0u + slot * 4u, registered);
        call(rt, ctx, 0x088902ECu, {frame.at(0u), slot, registered});
        if (static_cast<s32>(word(rt, self + 0x1D0u)) < 0) {
            if (call(rt, ctx, 0x088A5928u, {word(rt, self + 0x1A0u + slot * 4u)}) != 0u)
                put(rt, self + 0x1D0u, slot);
        }
        for (u32 part = 0u; part != 2u; ++part) {
            if (call(rt, ctx, 0x08890004u, {frame.at(0u), slot, part}) == 0u) continue;
            const u32 block = call(rt, ctx, 0x08B62344u, {0x30u});
            call(rt, ctx, 0x08897B8Cu, {block});
            put(rt, self + 0x1B0u + slot * 8u + part * 4u, block);
            call(rt, ctx, 0x08890330u, {frame.at(0u), slot, part, block});
        }
    }
    if (word(rt, frame.at(0x10u)) != 0u) call(rt, ctx, 0x08890554u, {frame.at(0u)});
    rt.memory().store8(self + 0x1D4u, 1u);
    call(rt, ctx, 0x0888F3E0u, {frame.at(0u)});
    ctx.gpr[2] = 1u;
}

// --- 0x0881FEF4: a1 = a mode (5 and above does nothing), a2 = the output block (a pointer at +0, four smart-pointer
// destinations at +4 / +8 / +0xC / +0x10). The object at *(a2) is asked for a 16-byte value (vtable +0xC), which is
// converted (0x08B88F44) and parsed into a 0x3C-byte block on the frame (0x08B10204). A non-zero first word takes the
// 0x08A40C14 path; a zero one walks the scene (0x08A32A94, 0x08A32350, 0x08B35254, 0x08A40C24, 0x08A5B9EC). Then
// *(a2)+4 is looked up (0x08A76ABC) and its virtual +0x70 read (0x08A40CF8, 0x08A41258), *(a2)+8 the same way
// (0x08A5BAD8), and the count from 0x08A40BF8 is handed to 0x0881FAF8 with the block at +0x34. For every element an
// identifier pair (0x08930170) and a float (virtual +0x54) go to 0x0881FB28 against the record at +0x48. The frame's
// smart pointers are released at the end.
// The usual release of a smart pointer slot: nothing when it holds the static block 0x08D74FD0, else 0x08B60804 decides
// (a count; 0 or less releases) and 0x08B8542C releases with `extra` as its second argument.
void release_slot(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 extra) {
    const u32 value = word(rt, slot);
    const u32 block = value - 0xCu;
    if (block == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu})) <= 0)
        call(rt, ctx, 0x08B8542Cu, {block, extra});
}


// --- 0x0895DEC8: a0 = the object (word(a0+8) is the car, word(a0+0xC) the state block), a1 = a mode. A gear/assist
// state machine: a running countdown at car+0x512 wins first (result -1), then the state bytes +0x510 / +0x511 / +0x5F9,
// the speeds at car+0x6C4 / +0x530 / +0x538 / +0x53C and the counters at state+0x23C / +0x58E decide; the result is the
// value the original leaves in a0 (a gear to select, -1 for none, or a state block).

} // namespace
} // namespace new_batch_main_49
} // namespace gt::game

#define NEW49(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_49::name, gt::ReturnKind::kind)
NEW49(0x08A2E8CCu, func_08A2E8CC, Word);
NEW49(0x08C41D90u, func_08C41D90, Word);
NEW49(0x0888EE48u, func_0888EE48, Word);
