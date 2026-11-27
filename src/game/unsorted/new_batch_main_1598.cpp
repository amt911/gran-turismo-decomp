// New batch (main session 1598): the functions whose earlier drafts the gate rejected (or that two drafters skipped),
// re-drafted from `tools.disasm`. Each one fixes what the asm says the old draft got wrong: 0x08B56674's VFPU scalar
// numbers and addresses, 0x08B0EC60's copy destination (a0, not a1), 0x08B63574's post-report comparison (the register,
// not the 0x10 the call overwrote), 0x08B1C590's leave call (the expired flag, not the handle), 0x088DBF0C's state-5 exit
// value, and the four 0x08AE1xxx writers leave v0 at the `lui` base of the scale word.
// Every body keeps the original's exits (`v0` on each path), the call order and the call's delay slot, the argument
// registers are set as the original sets them, and the frame is the prologue's `addiu sp, sp, -N`.
// Input shapes for the synthetic generator: /home/andres/repos/gt-ops/shapes/shapes_main1598.txt.
#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <limits>

namespace gt::game {
namespace new_batch_main_1598 {
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
// A call whose v1 (and f0) a caller reads afterwards: they are left as the callee left them, as after the `jal`.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v0, 0(object); lw v1, off(v0); jalr v1`: the callee starts with v0 = the vtable (not its own address).
GuestResult call_vtable(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                        std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + offset);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    return call_full(rt, ctx, target, arguments);
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 byte_u(Runtime &rt, u32 a) { return rt.memory().load8(a); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// A swl / swr pair: one word at an address of any alignment, byte by byte (little endian, the same bytes).
void put_unaligned(Runtime &rt, u32 address, u32 value) {
    for (unsigned i = 0u; i < 4u; ++i)
        rt.memory().store8(address + i, static_cast<std::uint8_t>(value >> (8u * i)));
}
// The copy the original unrolls: three 16-byte turns of four loads and four stores, then three words (0x3C bytes).
void copy_3c(Runtime &rt, u32 destination, u32 source) {
    for (u32 offset = 0u; offset != 0x3Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
}
// The `lui` half of an address whose low half is added as a signed 16-bit immediate: what the register holds at exit.
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// trunc.w.s: the float truncated toward zero, saturating as the FPU does (NaN answers INT32_MAX).
s32 truncate_word(float value) {
    if (std::isnan(value)) return std::numeric_limits<s32>::max();
    const double scaled = std::trunc(static_cast<double>(value));
    if (scaled > static_cast<double>(std::numeric_limits<s32>::max())) return std::numeric_limits<s32>::max();
    if (scaled < static_cast<double>(std::numeric_limits<s32>::min())) return std::numeric_limits<s32>::min();
    return static_cast<s32>(scaled);
}

// 0x08B56674 (a0 = the command block, a1 = the source): sixteen lv.s (S000,S020,S040,S060,S001,...) of the source's
// 0x40 bytes; the tail jump keeps a0 / a1 as they are.
void func_08B56674(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5];
    for (u32 column = 0u; column != 4u; ++column)
        for (u32 row = 0u; row != 4u; ++row)
            gt::vfpu::load_scalar(rt, ctx, column + row * 0x20u, source + 0x10u * column + 4u * row);
    tail_call(ctx, 0x08B5661Cu, {});
}

// 0x08B62450 (a0 = index, a1 = a record whose block starts at a1 - 0x30): index >= 2 hands the record's +0xC word to
// 0x08B64564 (which reports and does not return); otherwise the handler at +4 gets a1 + 0x20, and that same pointer is
// the tail call's a0.
void func_08B62450(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[4];
    const u32 base = ctx.gpr[5];
    const u32 self = base + 0x20u;
    GuestFrame frame(ctx, 0x10u);
    if (index >= 2u) {
        call(rt, ctx, 0x08B64564u, {word(rt, base - 0x30u + 0xCu)});
        return;
    }
    const u32 handler = word(rt, base - 0x30u + 4u);
    if (handler != 0u) call_indirect(rt, ctx, handler, {self});
    tail_call(ctx, 0x08B61DF4u, {self});
}

// 0x08A17CD8 (a0 = a holder, a1..t1 = five words): the flag is *(*a0); zero shifts the five arguments down one
// register, non-zero sends them as (a1, t0, t1, a2, a3) instead. Tail call 0x08966B20 either way.
void func_08A17CD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 fifth = ctx.gpr[9];
    const u32 flag = word(rt, word(rt, ctx.gpr[4]));
    if (flag == 0u) {
        tail_call(ctx, 0x08966B20u, {first, second, third, fourth, fifth});
        return;
    }
    tail_call(ctx, 0x08966B20u, {first, fourth, fifth, second, third});
}

// 0x08B63574 (a0 = a format byte, a1 = the object): 0x20 / 0x30 / 0x40 hand a1 to a reader; 0xFF, 0x00, 0x10 and 0x50
// answer 0. For 0x60 / 0x70 the format is reported once, and only if the report's answer differs is it reported again
// and the 0x40 reader tail-called (the comparison is against that answer, not the 0x10 the call overwrote).
void func_08B63574(Runtime &rt, AllegrexContext &ctx) {
    const u32 type = ctx.gpr[4] & 0xFFu;
    if (type == 0xFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const u32 kind = type & 0x70u;
    switch (kind) {
    case 0x20u: tail_call(ctx, 0x08B726ECu, {ctx.gpr[5]}); return;
    case 0x30u: tail_call(ctx, 0x08B726F4u, {ctx.gpr[5]}); return;
    case 0x40u: tail_call(ctx, 0x08B726FCu, {ctx.gpr[5]}); return;
    case 0x00u:
    case 0x10u:
    case 0x50u: ctx.gpr[2] = 0u; return;
    default: break;
    }
    const u32 reported = call(rt, ctx, 0x08B77B04u);
    if (kind == reported) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B77B04u);
    tail_call(ctx, 0x08B726FCu, {ctx.gpr[5]});
}

// 0x0892FCE0 (a0 = a map, a2 / a3 = a key pair): the table's answer 0x26 or a map already holding that key answers 0;
// else the map is reset, the slot looked up and stored at +0x10 / +0x14 with the key at +8 / +0xC.
void func_0892FCE0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 map = ctx.gpr[4];
    const u32 key_first = ctx.gpr[6];
    const u32 key_second = ctx.gpr[7];
    const u32 table = call(rt, ctx, 0x089363FCu);
    if (call(rt, ctx, 0x0893656Cu, {table, 0u, key_first, key_second}) == 0x26u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, map + 8u) == key_first && word(rt, map + 0xCu) == key_second) {
        ctx.gpr[2] = 1u;
        return;
    }
    call(rt, ctx, 0x0892FBE0u, {map});
    const u32 again = call(rt, ctx, 0x089363FCu);
    const u32 slot = call(rt, ctx, 0x08937918u, {again, 1u, key_first, key_second, frame.at(0u)});
    if (slot == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, map + 0x10u, slot);
    put(rt, map + 0x14u, word(rt, frame.at(0u)));
    put(rt, map + 8u, key_first);
    put(rt, map + 0xCu, key_second);
    ctx.gpr[2] = 1u;
}

// 0x08B0EC60 (a0 = the object to build, a1 = a target buffer): 0x08B0CE58 constructs in a0, the temporary is filled
// for a1 and 0x08B0EA48 takes the block; its 0x3C-byte payload (+0xA8) goes into a0 (the register the original copies
// into), then the block and the temporary are released; v0 = a0.
void func_08B0EC60(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x110u);
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    call(rt, ctx, 0x08B0CE58u, {self});
    call(rt, ctx, 0x08B0E0CCu, {frame.at(0u)});
    const u32 block = call(rt, ctx, 0x08B0EA48u, {frame.at(0u), target});
    if (block != 0u) {
        put(rt, block + 0x7Cu, 4u);
        call(rt, ctx, 0x08B0DE3Cu, {block});
        copy_3c(rt, self, block + 0xA8u);
        call(rt, ctx, 0x08B0EA58u, {block});
    }
    call(rt, ctx, 0x08B0DDF0u, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// 0x088941A0 (a0 = a holder of the source, a1 = the destination): 0x24 bytes copied (sixteen-byte turns and the last
// word; swl / swr when either side is misaligned, the same bytes either way), then v0 = destination + 0x08894020's
// answer with (a0, a1) the last block's third and fourth words and (a2) the end of the destination.
void func_088941A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 destination = ctx.gpr[5];
    const u32 source = word(rt, ctx.gpr[4]);
    const u32 end = source + 0x20u;
    u32 from = source;
    u32 to = destination;
    u32 third = 0u;
    u32 fourth = 0u;
    while (from != end) {
        const u32 part0 = word(rt, from);
        const u32 part1 = word(rt, from + 4u);
        const u32 part2 = word(rt, from + 8u);
        const u32 part3 = word(rt, from + 0xCu);
        put_unaligned(rt, to, part0);
        put_unaligned(rt, to + 4u, part1);
        put_unaligned(rt, to + 8u, part2);
        put_unaligned(rt, to + 0xCu, part3);
        third = part2;
        fourth = part3;
        from += 0x10u;
        to += 0x10u;
    }
    put_unaligned(rt, to, word(rt, from));
    ctx.gpr[2] = destination + call(rt, ctx, 0x08894020u, {third, fourth, to});
}

// 0x08900350 (a0 = object): a negative +0x41C takes the pair at +0x414 / +0x418, a 1.0f at +4 becomes 0x3FA66666 and
// the 100 / 1000 / 0 triple at +0x49C copies +0xC / +0x30 / +0x38 into +0x4A0 / +0x49C / +0x4A4 and +0xC into +0x34.
void func_08900350(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float zero = 0.0f;
    if (float_at(rt, self + 0x41Cu) < zero) {
        put(rt, self + 0x41Cu, word(rt, self + 0x414u));
        put(rt, self + 0x420u, word(rt, self + 0x418u));
    }
    if (float_at(rt, self + 4u) == 1.0f) put(rt, self + 4u, 0x3FA66666u);
    if (float_at(rt, self + 0x49Cu) == 100.0f && float_at(rt, self + 0x4A0u) == 1000.0f &&
        float_at(rt, self + 0x4A4u) == zero) {
        const u32 at_c = word(rt, self + 0xCu);
        const u32 at_30 = word(rt, self + 0x30u);
        const u32 at_38 = word(rt, self + 0x38u);
        put(rt, self + 0x4A0u, at_c);
        put(rt, self + 0x49Cu, at_30);
        put(rt, self + 0x4A4u, at_38);
        put(rt, self + 0x34u, at_c);
    }
}

// 0x08AD5460 (a0 = the object, a1 = the source): 0x08AD5394(a0, a1), the temporary at sp+0x40 filled by 0x08B10204,
// its 0x3C bytes copied to sp; when the first word is zero the object takes +0xC = 1, +8 = sp+0x10 and +4 = sp+0x20
// and v0 = 1, else v0 = 0.
void func_08AD5460(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 temporary = frame.at(0x40u);
    call(rt, ctx, 0x08AD5394u, {self, source});
    call(rt, ctx, 0x08B10204u, {temporary, source, 1u});
    copy_3c(rt, frame.at(0u), temporary);
    if (word(rt, frame.at(0u)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put8(rt, self + 0xCu, 1u);
    put(rt, self + 8u, word(rt, frame.at(0x10u)));
    put(rt, self + 4u, word(rt, frame.at(0x20u)));
    ctx.gpr[2] = 1u;
}

// 0x08B48398 (a0 = an owner, a1 = text): a 0x44-byte block from the owner's +0x28 port (0x10 bytes) whose first half
// and +0x1A half-open out and whose +0x30..+0x40 words are cleared; non-null text becomes +0x40 through the same port.
void func_08B48398(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 block = call(rt, ctx, 0x08B2CB18u, {word(rt, owner + 0x28u), 0x44u, 0x10u});
    if (block != 0u) {
        call(rt, ctx, 0x08B3FA98u, {block + 4u});
        call(rt, ctx, 0x08B3FA98u, {block + 0x1Au});
        put(rt, block, 0u);
        call(rt, ctx, 0x08B3F838u, {block + 4u});
        call(rt, ctx, 0x08B3F838u, {block + 0x1Au});
        put8(rt, block + 0x30u, 0u);
        put8(rt, block + 0x31u, 0u);
        put8(rt, block + 0x32u, 0u);
        put(rt, block + 0x34u, 0u);
        put(rt, block + 0x38u, 0u);
        put(rt, block + 0x3Cu, 0u);
        put(rt, block + 0x40u, 0u);
    }
    if (text != 0u) put(rt, block + 0x40u, call(rt, ctx, 0x08B2CB18u, {word(rt, owner + 0x28u), text, 0x10u}));
    ctx.gpr[2] = block;
}

// 0x088725E4 (a0 = object, a1 = a flag, f12 / f13 = the low / high of a range, f14 = a value): the packet at +0x5C + 0x24
// takes 0 / 0 (or 1.0f / 0 with the flag clear) and its span keeps the previous coordinate; with the flag set the
// range goes into +0x10 / +0x14 / +0x1C, 1.0f into +0x10 and +0x18 (the latter only when low <= high) and +0x20 gets
// the low two bits set; tail call 0x08872548(+0x5C, a0 + 0xC).
void func_088725E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const float low = ctx.fpr[12];
    const float high = ctx.fpr[13];
    const float value = ctx.fpr[14];
    const float one = 1.0f;
    if (flag != 0u) {
        const bool ordered = low <= high;
        const u32 packet = word(rt, self + 0x5Cu) + 0x24u;
        const u32 flags = word(rt, packet + 0x20u) & ~3u;
        put(rt, packet + 8u, 0u);
        put(rt, packet + 0xCu, 0u);
        const u32 x = word(rt, packet + 8u);
        const u32 y = word(rt, packet + 0xCu);
        put(rt, packet + 0x20u, flags);
        put(rt, packet + 0x10u, x);
        put(rt, packet + 0x14u, y);
        put(rt, packet + 0x18u, x);
        put(rt, packet + 0x1Cu, y);
        put_float(rt, word(rt, self + 0x5Cu) + 0x28u, value);
        const u32 span = word(rt, self + 0x5Cu) + 0x24u;
        const float previous = float_at(rt, span + 0x18u);
        put(rt, span + 0xCu, 0u);
        put_float(rt, span + 8u, previous);
        put_float(rt, span + 0x1Cu, high);
        put_float(rt, span + 0x10u, one);
        put_float(rt, span + 0x14u, low);
        if (ordered) put_float(rt, span + 0x18u, one);
        put(rt, span + 0x20u, word(rt, span + 0x20u) | 3u);
    } else {
        const u32 packet = word(rt, self + 0x5Cu) + 0x24u;
        const u32 flags = word(rt, packet + 0x20u) & ~3u;
        put_float(rt, packet + 8u, one);
        put(rt, packet + 0xCu, 0u);
        const u32 x = word(rt, packet + 8u);
        const u32 y = word(rt, packet + 0xCu);
        put(rt, packet + 0x20u, flags);
        put(rt, packet + 0x10u, x);
        put(rt, packet + 0x14u, y);
        put(rt, packet + 0x18u, x);
        put(rt, packet + 0x1Cu, y);
    }
    tail_call(ctx, 0x08872548u, {word(rt, self + 0x5Cu), self + 0xCu});
}

// 0x08B0A458 (a0 = an owner, a1 = a kind): 0x08B344E0 locks the global, a zero kind reads the global default; kind 5
// answers the object the owner's +0x110 holds (creating a 0x20CE0-byte one when empty, if its virtual at +0x18 also
// answers 5); 0x08B3457C unlocks; v0 = the answer.
void func_08B0A458(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 kind = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B344E0u, {0x08D473E8u});
    if (kind == 0u) kind = word(rt, 0x08CE0A24u);
    const u32 held = word(rt, word(rt, self) + 0x110u);
    u32 result = 0u;
    if (kind == 5u) {
        if (held != 0u) {
            if (call_indirect(rt, ctx, word(rt, word(rt, held) + 0x18u), {held}) == kind) result = held;
        } else {
            const u32 owner = word(rt, self);
            const u32 created = call(rt, ctx, 0x08B62344u, {(0x2u << 16) | 0xCE0u});
            call(rt, ctx, 0x08B451A0u, {created});
            put(rt, owner + 0x110u, created);
            result = word(rt, word(rt, self) + 0x110u);
        }
    }
    call(rt, ctx, 0x08B3457Cu, {0x08D473E8u});
    ctx.gpr[2] = result;
}

// 0x08BA6470 (a0 = object, a1 / a2 = arguments): 0x08B569A0, then for each index below the count the node at +8 reports
// (its +0x48 counter's virtual at +0x48, else the half at +0x10 of its +0x40 table) a hit through 0x08884D6C sends
// +0x10 / +0x14 too; the tail 0x08B56940 unlocks and ignores the a0 it is handed.
void func_08BA6470(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B569A0u, {});
    call(rt, ctx, 0x08B566BCu, {word(rt, self + 8u)});
    u32 index = 0u;
    for (;;) {
        u32 node = word(rt, self + 8u);
        const u32 counter = word(rt, node + 0x48u);
        u32 limit;
        if (counter != 0u) {
            limit = call_indirect(rt, ctx, word(rt, word(rt, counter) + 0x48u), {counter});
        } else {
            const u32 table = word(rt, node + 0x40u);
            if (table == 0u) break;
            limit = rt.memory().load16(table + 0x10u);
        }
        if (index >= limit) break;
        if (call(rt, ctx, 0x08884D6Cu, {node, arg2, 0u, index}) != 0u) {
            call(rt, ctx, 0x08AFA208u, {word(rt, self + 0x10u), arg2, 0u, index, word(rt, self + 0x14u)});
            node = word(rt, self + 8u);
        }
        ++index;
    }
    tail_call(ctx, 0x08B56940u, {self});
}

// 0x08C2E994 (a0 = first, a1 = last, a2 = the second predicate argument, a3 = the predicate object): step first forward
// while the predicate (its virtual at +8) holds (first, a2), then last back while it holds (a2, last); overlapping, v0 =
// first; else the pair is swapped around the temporary at sp. Loops until first >= last.
void func_08C2E994(Runtime &rt, AllegrexContext &ctx) {
    u32 position = ctx.gpr[4];
    u32 end = ctx.gpr[5];
    const u32 owner = ctx.gpr[6];
    const u32 predicate = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    for (;;) {
        for (;;) {
            if (call_indirect(rt, ctx, word(rt, word(rt, predicate) + 8u), {predicate, position, owner}) == 0u) break;
            position += 4u;
        }
        for (;;) {
            end -= 4u;
            if (call_indirect(rt, ctx, word(rt, word(rt, predicate) + 8u), {predicate, owner, end}) == 0u) break;
        }
        if (position >= end) {
            ctx.gpr[2] = position;
            return;
        }
        call(rt, ctx, 0x08A76ABCu, {frame.at(0u), position});
        if (end != position) call(rt, ctx, 0x08A5BAD8u, {position, word(rt, end)});
        if (end != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {end, word(rt, frame.at(0u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        position += 4u;
    }
}

// 0x088A61A4 (a0 = a holder): when *a0's +0x4CC byte is clear, a 0x1CB0-byte block is allocated and cleared, stored at
// +0x4C8, filled through 0x0892E51C (*a0 + (signed byte at +8) * 384 + 0x38), 0x08944100 (+0x690) and 0x08951304
// (+0x1B30, +0x1C30), then +0x1CAC is the float 0x08948D04 gives for (+0x690, +0x1B30). v0 is the flag byte when set.
void func_088A61A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = word(rt, self);
    const u32 flag = byte_u(rt, owner + 0x4CCu);
    ctx.gpr[2] = flag;
    if (flag != 0u) return;
    const u32 object = call(rt, ctx, 0x08B62344u, {0x1CB0u});
    call(rt, ctx, 0x08B36018u, {object, 0u, 0x1CB0u});
    put(rt, owner + 0x4C8u, object);
    const u32 base = word(rt, self);
    const s32 count = signed_byte(rt, base + 8u);
    call(rt, ctx, 0x0892E51Cu, {base + static_cast<u32>(count) * 384u + 0x38u, object});
    const u32 middle = object + 0x690u;
    call(rt, ctx, 0x08944100u, {middle, object, word(rt, self) + 0xCu, 0xFFFFFFFFu});
    const u32 last = object + 0x1B30u;
    call(rt, ctx, 0x08951304u, {object, object + 0x1C30u, last, word(rt, self) + 0xCu});
    put_float(rt, object + 0x1CACu, call_full(rt, ctx, 0x08948D04u, {middle, last}).f0);
}

// 0x08AE11A0 / 0x08AE1338 / 0x08AE160C / 0x08AE1794 (a0 = the key, f12 = value, f13 = the second float): enter
// (0x08AEB808 with a0 = the key), a scratch of 4 * word(0x08D4069C) bytes that `collect` fills, leave (0x08AEB82C),
// then the value clamped to [low, high] and scaled, truncated to an integer, goes to 0x08AE0F64(scratch, the collected
// count, integer, f12 = the second float). At that call v0 still holds the `lui` base of the scale word (only the f0 load
// uses it), which is what a callee that never writes v0 hands back.
void scaled_parameter(Runtime &rt, AllegrexContext &ctx, u32 collect, bool key_is_object, bool leave_is_object,
                      u32 high_at, u32 low_at, u32 scale_at) {
    const u32 self = ctx.gpr[4];
    const float value_in = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x30u);
    (void)call_full(rt, ctx, 0x08AEB808u, {self}, {value_in});
    const u32 count = word(rt, 0x08D4069Cu);
    GuestFrame scratch(ctx, (count * 4u + 0xFu) & ~0xFu);
    const u32 collected = call(rt, ctx, collect, {scratch.at(0u), key_is_object ? self : 0u, 0u});
    (void)call(rt, ctx, 0x08AEB82Cu, {scratch.at(0u), leave_is_object ? self : 0u});
    const float high = float_at(rt, high_at);
    float value = high;
    if (value_in < high) {
        const float low = float_at(rt, low_at);
        value = value_in < low ? low : value_in;
    }
    ctx.gpr[2] = high_half(scale_at);
    const u32 integer = static_cast<u32>(truncate_word(psp_mul(value, float_at(rt, scale_at))));
    (void)call_full(rt, ctx, 0x08AE0F64u, {scratch.at(0u), collected, integer}, {second});
}

void func_08AE11A0(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE328u, true, true, 0x08CB60C4u, 0x08CB60C8u, 0x08CB60CCu);
}
void func_08AE1338(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE3A8u, true, true, 0x08CB60D0u, 0x08CB60D4u, 0x08CB60D8u);
}
void func_08AE160C(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE4ACu, true, true, 0x08CB60E8u, 0x08CB60ECu, 0x08CB60F0u);
}
void func_08AE1794(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE2B4u, false, false, 0x08CB60F4u, 0x08CB60F8u, 0x08CB60FCu);
}

// 0x08B01070 (a0 = a node holding a signed half index at +4, a1 = the container): a negative index answers itself; else
// the record at container[+0x60] + index * 84 is walked through its signed half link at +0x52, each handed to
// 0x08B014BC(container, record, next * 84, next); +4 becomes 0xFFFF and v0 = -1.
void func_08B01070(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const s32 first = signed_half(rt, self + 4u);
    if (first < 0) {
        ctx.gpr[2] = static_cast<u32>(first);
        return;
    }
    u32 record = word(rt, container + 0x60u) + 84u * static_cast<u32>(first);
    while (record != 0u) {
        const s32 next = signed_half(rt, record + 0x52u);
        const u32 following =
            next >= 0 ? word(rt, container + 0x60u) + 84u * static_cast<u32>(next) : 0u;
        call(rt, ctx, 0x08B014BCu, {container, record, 84u * static_cast<u32>(next), static_cast<u32>(next)});
        record = following;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    put16(rt, self + 4u, ctx.gpr[2]);
}

// 0x08B1C590 (a0 = a counter {+0x10 / +0x14 deadline, +0x18 handle}, a2 / a3 = a 64-bit low / high): after
// 0x08B28D34(sp) and 0x08B34D40(a0 + 4), a zero pair or a negative high becomes deadline - (low, high); when the new
// pair is past the deadline the handle is kept in sp+0x2C and the pair in sp+0x30 / 0x34 with a0's handle replaced by
// sp; 0x08B34D80(a0 + 4), then 0x08B28CD4(sp) only when that happened, and 0x08B28BE0(sp). v0 / v1 = low / high.
void func_08B1C590(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    u32 low = ctx.gpr[6];
    u32 high = ctx.gpr[7];
    call(rt, ctx, 0x08B28D34u, {frame.at(0u)});
    call(rt, ctx, 0x08B34D40u, {self + 4u});
    if (static_cast<s32>(high) < 0 || (high == 0u && low == 0u)) {
        const u32 base_low = word(rt, self + 0x10u);
        const u32 base_high = word(rt, self + 0x14u);
        const u32 difference = base_low - low;
        const u32 borrow = base_low < difference ? 1u : 0u;
        high = base_high - high - borrow;
        low = difference;
    }
    const u32 deadline_low = word(rt, self + 0x10u);
    const u32 deadline_high = word(rt, self + 0x14u);
    const bool expired = static_cast<s32>(deadline_high) < static_cast<s32>(high) ||
                         (deadline_high == high && deadline_low < low);
    bool taken = false;
    if (expired) {
        const u32 handle = word(rt, self + 0x18u);
        put(rt, self + 0x18u, frame.at(0u));
        put(rt, frame.at(0u) + 0x30u, low);
        put(rt, frame.at(0u) + 0x34u, high);
        put(rt, frame.at(0u) + 0x2Cu, handle);
        taken = true;
    }
    call(rt, ctx, 0x08B34D80u, {self + 4u});
    if (taken) call(rt, ctx, 0x08B28CD4u, {frame.at(0u)});
    call(rt, ctx, 0x08B28BE0u, {frame.at(0u)});
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// 0x0887B768 (a0 = a pool, a1 = an argument): the first clear bit at pool+0x28 (or slot 0x18) below pool's count (+0xC)
// is built up: a scratch string at sp+0xC holds the argument, a handle through 0x088A26E0 goes to pool + slot * 0x38 +
// 0x60, the bit and the pool's +0x24 counter are bumped and 0x0887B40C(record + 0x2C, 0, 0); then two virtuals of
// pool's +0 (slots 0x24 and 0x30) and 0x088A1854 get the record's tables. v0 / v1 = the index on success, -1 otherwise.
void func_0887B768(Runtime &rt, AllegrexContext &ctx) {
    const u32 pool = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0u);
    const u32 count = word(rt, pool + 0xCu);
    if (static_cast<s32>(count) <= 0) {
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 bitmap = pool + 0x28u;
    u32 index = 0u;
    u32 slot;
    for (;;) {
        if (index == 0x18u) {
            slot = 0x18u;
            break;
        }
        if (((word(rt, bitmap + ((index >> 5) * 4u)) >> (index & 31u)) & 1u) == 0u) {
            slot = index;
            break;
        }
        index += 1u;
        if (index == count) {
            ctx.gpr[3] = 0xFFFFFFFFu;
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
    }
    const u32 scratch = sp + 0xCu;
    call(rt, ctx, 0x088A79ACu, {scratch, 0u});
    call(rt, ctx, 0x088A5820u, {scratch, argument});
    put(rt, sp, 0u);
    put8(rt, sp + 8u, 0u);
    put8(rt, sp + 9u, 1u);
    put(rt, sp + 4u, scratch);
    const u32 handle = call(rt, ctx, 0x088A26E0u, {word(rt, pool), slot, sp});
    const u32 record = pool + slot * 0x38u;
    put(rt, record + 0x60u, handle);
    call(rt, ctx, 0x088A7548u, {scratch});
    if (word(rt, record + 0x60u) == 0u) {
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    u32 counter = word(rt, pool + 0x24u);
    if (slot < 0x18u) {
        const u32 cell = bitmap + ((slot >> 5) * 4u);
        put(rt, cell, word(rt, cell) | (1u << (slot & 31u)));
        counter = word(rt, pool + 0x24u);
    }
    const u32 kept = (1u << 30u) - 1u;
    const u32 bumped = static_cast<u32>((static_cast<s32>(counter << 2) >> 2) + 1);
    put(rt, pool + 0x24u, (counter & ~kept) | (bumped & kept));
    call(rt, ctx, 0x0887B40Cu, {record + 0x2Cu, 0u, 0u});
    put(rt, sp, word(rt, 0x08C89830u));
    put(rt, sp + 4u, word(rt, 0x08C89830u + 4u));
    put(rt, sp + 8u, word(rt, 0x08C89830u + 8u));
    put(rt, sp + 0xCu, word(rt, 0x08C89824u));
    put(rt, sp + 0x10u, word(rt, 0x08C89824u + 4u));
    put(rt, sp + 0x14u, word(rt, 0x08C89824u + 8u));
    const u32 owner = word(rt, pool);
    (void)call_vtable(rt, ctx, owner, 0x24u, {owner, slot, word(rt, 0x08C89830u), owner});
    (void)call_vtable(rt, ctx, owner, 0x30u, {owner, slot, sp, scratch, 0u, 0u, 0u, word(rt, pool + 8u)});
    (void)call_full(rt, ctx, 0x088A1854u, {word(rt, pool), slot, sp}, {float_at(rt, sp + 0x10u)});
    ctx.gpr[3] = index;
    ctx.gpr[2] = index;
}

// 0x088FE08C (a0 = out, a1 = source, a2 / a3 = a pair): 0x08B0CE58(sp+0x80) and 0x088FDF98(out); with a2 the pair fills
// sp+0xC0 through 0x08B10170 and its 0x3C bytes reach sp+0x80 through sp+0x40, else 0x08B10204 fills sp+0x100 and its
// bytes reach sp+0x80 through sp plus out+4 = sp+0xA0. Out then takes sp+0xA0 (or 0 when sp+0x90 / +0x94 are both
// zero) and, when non-zero, goes to 0x088FDFE0; v0 = what out holds.
void func_088FE08C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x160u);
    const u32 sp = frame.at(0u);
    call(rt, ctx, 0x08B0CE58u, {sp + 0x80u});
    call(rt, ctx, 0x088FDF98u, {out});
    if (a2 != 0u) {
        call(rt, ctx, 0x08B10170u, {sp + 0xC0u, source, a2, a3, 1u});
        copy_3c(rt, sp + 0x40u, sp + 0xC0u);
        copy_3c(rt, sp + 0x80u, sp + 0x40u);
    } else {
        call(rt, ctx, 0x08B10204u, {sp + 0x100u, source, 1u});
        copy_3c(rt, sp, sp + 0x100u);
        copy_3c(rt, sp + 0x80u, sp);
        put(rt, out + 4u, word(rt, sp + 0xA0u));
    }
    if ((word(rt, sp + 0x90u) | word(rt, sp + 0x94u)) == 0u) put(rt, out, 0u);
    else put(rt, out, word(rt, sp + 0xA0u));
    if (word(rt, out) != 0u) call(rt, ctx, 0x088FDFE0u, {word(rt, out)});
    ctx.gpr[2] = word(rt, out);
}

// 0x088DBF0C (a0 = the object): nothing when +0x5BF is clear; a state of 6 or more answers 0. State 0 releases the inner
// object and tail-calls 0x088E4380; 1 waits for the deadline (0x088E48E8) and moves to state 2 (answering 2) when
// 0x088E48A4 fails or 1000 past it is already behind 0x088D9C70's clock; 2 starts 0x088D7158 and answers its value with
// state 3; 3 runs 0x088DA0BC / 0x088DA3EC and 0x088DB4A8(object, 3), then state 4 with +0x28 / +0x2C = clock + 1500 (when
// the inner byte +0x1A is 3, else answering 3); 4 sets inner +0x18 = 1 and 0x088D78EC, answering 5 (state 5 when it
// succeeded); 5 runs 0x088DA0BC / 0x088DA3EC and 0x088DB4A8(object, 1) and answers the inner byte +0x1A (state 0 when it
// is not 3, the inner object when it is).
void func_088DBF0C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 enabled = byte_u(rt, object + 0x5BFu);
    ctx.gpr[2] = enabled;
    if (enabled == 0u) return;
    const u32 state = word(rt, object + 0x5C4u);
    if (state >= 6u) {
        ctx.gpr[2] = 0u;
        return;
    }
    switch (state) {
    case 0u: {
        const u32 inner = word(rt, object + 0x130u);
        put8(rt, object + 0x5BFu, 0u);
        put8(rt, inner + 0x18u, 0u);
        put(rt, inner + 0x28u, 0u);
        put(rt, inner + 0x2Cu, 0u);
        put8(rt, word(rt, object + 0x130u) + 0x13u, 0u);
        tail_call(ctx, 0x088E4380u, {word(rt, object + 0x640u), 0u});
        return;
    }
    case 1u: {
        const u32 inner = word(rt, object + 0x130u);
        if (byte_u(rt, inner + 0x19u) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (call(rt, ctx, 0x088E48A4u, {word(rt, object + 0x640u)}) == 0u) {
            put(rt, object + 0x5C4u, 2u);
            ctx.gpr[2] = 2u;
            return;
        }
        const GuestResult deadline = call_full(rt, ctx, 0x088E48E8u, {word(rt, object + 0x640u)});
        const u32 low = deadline.v0;
        const u32 high = deadline.v1;
        if ((low | high) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const GuestResult now = call_full(rt, ctx, 0x088D9C70u, {object});
        const u32 carried = low + 0x3E8u;
        const u32 limit_high = (carried < low ? 1u : 0u) + high;
        if (limit_high < now.v1 || (limit_high == now.v1 && carried < now.v0)) {
            put(rt, object + 0x5C4u, 2u);
            ctx.gpr[2] = 2u;
            return;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    case 2u: {
        call(rt, ctx, 0x088D7158u, {object, 0u});
        put(rt, object + 0x5C4u, 3u);
        return;
    }
    case 3u: {
        if (call(rt, ctx, 0x088DA0BCu, {object}) != 0u) return;
        if (call(rt, ctx, 0x088DA3ECu, {object}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        call(rt, ctx, 0x088DB4A8u, {object, 3u});
        const u32 inner = word(rt, object + 0x130u);
        if (byte_u(rt, inner + 0x1Au) != 3u) {
            ctx.gpr[2] = 3u;
            return;
        }
        const GuestResult now = call_full(rt, ctx, 0x088D9C70u, {object});
        const u32 carried = now.v0 + 0x5DCu;
        put(rt, inner + 0x28u, carried);
        put(rt, inner + 0x2Cu, (carried < now.v0 ? 1u : 0u) + now.v1);
        put(rt, object + 0x5C4u, 4u);
        ctx.gpr[2] = 4u;
        return;
    }
    case 4u: {
        const u32 inner = word(rt, object + 0x130u);
        put8(rt, inner + 0x18u, 1u);
        if (call(rt, ctx, 0x088D78ECu, {object, 0u, 0u}) != 0u) put(rt, object + 0x5C4u, 5u);
        ctx.gpr[2] = 5u;
        return;
    }
    default: {
        if (call(rt, ctx, 0x088DA0BCu, {object}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (call(rt, ctx, 0x088DA3ECu, {object}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        call(rt, ctx, 0x088DB4A8u, {object, 1u});
        const u32 inner = word(rt, object + 0x130u);
        const u32 flag = byte_u(rt, inner + 0x1Au);
        if (flag != 3u) {
            put(rt, object + 0x5C4u, 0u);
            ctx.gpr[2] = flag;
            return;
        }
        ctx.gpr[2] = inner;
        return;
    }
    }
}

} // namespace
} // namespace new_batch_main_1598
} // namespace gt::game

#define NEW1598(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1598::name, gt::ReturnKind::kind)
NEW1598(0x08B56674u, func_08B56674, Word);
NEW1598(0x08B62450u, func_08B62450, Word);
NEW1598(0x08A17CD8u, func_08A17CD8, Word);
NEW1598(0x08B63574u, func_08B63574, Word);
NEW1598(0x0892FCE0u, func_0892FCE0, Word);
NEW1598(0x08B0EC60u, func_08B0EC60, Word);
NEW1598(0x088941A0u, func_088941A0, Word);
NEW1598(0x08900350u, func_08900350, Word);
NEW1598(0x08AD5460u, func_08AD5460, Word);
NEW1598(0x08B48398u, func_08B48398, Word);
NEW1598(0x088725E4u, func_088725E4, Word);
NEW1598(0x08B0A458u, func_08B0A458, Word);
NEW1598(0x08BA6470u, func_08BA6470, Word);
NEW1598(0x08C2E994u, func_08C2E994, Word);
NEW1598(0x088A61A4u, func_088A61A4, Word);
NEW1598(0x08AE1794u, func_08AE1794, Word);
NEW1598(0x08AE11A0u, func_08AE11A0, Word);
NEW1598(0x08AE1338u, func_08AE1338, Word);
NEW1598(0x08AE160Cu, func_08AE160C, Word);
NEW1598(0x08B01070u, func_08B01070, Word);
NEW1598(0x08B1C590u, func_08B1C590, DoubleWord);
NEW1598(0x0887B768u, func_0887B768, Word);
NEW1598(0x088FE08Cu, func_088FE08C, Word);
NEW1598(0x088DBF0Cu, func_088DBF0C, Word);
