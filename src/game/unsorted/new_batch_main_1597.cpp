// New batch (main session 1597): drafts from tools.disasm, structured C. Not gate-verified yet.
// Every function keeps the input shapes listed in gt-ops/shapes/shapes_main1597.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1597 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 low24(u32 value) { return (value << 8) >> 8; }
u32 low8(u32 value) { return (value << 24) >> 24; }
void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void store16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
// A `lw v1, 0(self); lw v0, slot(v1); jalr v0` call: v0 holds the target, v1 the vtable.
u32 call_vtable_slot(Runtime &rt, AllegrexContext &ctx, u32 self, u32 slot) {
    const u32 vtable = word(rt, self);
    return call_indirect(rt, ctx, word(rt, vtable + slot), {self});
}
// The same call written as `lw v1, 0(self); lw v0, slot(v1); jalr v1`: v0 still holds the vtable word.
u32 call_through_v1(Runtime &rt, AllegrexContext &ctx, u32 self, u32 slot) {
    const u32 vtable = word(rt, self);
    const u32 target = word(rt, vtable + slot);
    ctx.gpr[2] = vtable;
    const GuestResult result = call_guest(rt, ctx, target, {self}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The release loop of the intrusive pointer the destructors drop: refcount at +4, then +8, then the vtable's slot 0xC.
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    const u32 first = word(rt, object + 4u);
    put(rt, object + 4u, first - 1u);
    if (first != 1u) return;
    call_vtable_slot(rt, ctx, object, 8u);
    const u32 second = word(rt, object + 8u);
    put(rt, object + 8u, second - 1u);
    if (second != 1u) return;
    call_vtable_slot(rt, ctx, object, 0xCu);
}

// (value << 12) / 127 with the original's magic sequence: hi(x * 0x81020409) + x, >> 6, minus the sign.
s32 scaled_coord(u32 value) {
    const s32 shifted = static_cast<s32>(value << 12);
    const s64 product = static_cast<s64>(shifted) * static_cast<s64>(static_cast<s32>(0x81020409u));
    const s32 high = static_cast<s32>(product >> 32);
    const s32 sum = static_cast<s32>(static_cast<u32>(high) + static_cast<u32>(shifted));
    return (sum >> 6) - (shifted >> 31);
}

// 0x08B82B54: seed one of the eight 0x40-byte grid cells (and its 0x08D73ED4 twin) with two scaled coordinates.
void func_08B82B54(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[4];
    const u32 x = ctx.gpr[5];
    const u32 y = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (index >= 8u) { ctx.gpr[2] = (0x8044u << 16) | 5u; return; }
    if (x >= 0x80u) { ctx.gpr[2] = (0x8044u << 16) | 0xAu; return; }
    if (y >= 0x80u) { ctx.gpr[2] = (0x8044u << 16) | 0xAu; return; }
    const u32 cell = 0x08D73CD4u + (index << 6);
    if (word(rt, cell) == 0u && (word(rt, cell + 0x18u) & 0x400u) == 0u) {
        ctx.gpr[2] = (0x8044u << 16) | 6u;
        return;
    }
    if (word(rt, cell + 0x1Cu) != 0u) {
        const u32 twin = 0x08D73ED4u + (index << 6);
        put(rt, twin + 0x1Cu, 1u);
        put(rt, twin + 0x2Cu, 0u);
        put(rt, twin + 0x20u, 0u);
        store16(rt, twin + 0x30u, 0u);
        store16(rt, twin + 0x32u, 0u);
        put(rt, cell + 0x18u, word(rt, cell + 0x18u) | 0x00110000u);
        put(rt, twin + 0x8u, static_cast<u32>(scaled_coord(x)));
        put(rt, twin + 0xCu, static_cast<u32>(scaled_coord(y)));
        put(rt, twin + 0x28u, 0u);
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 owner = word(rt, 0x08D740E8u);
    store16(rt, cell + 0x30u, 0u);
    put(rt, cell + 0x1Cu, 1u);
    put(rt, cell + 0x2Cu, 0u);
    put(rt, cell + 0x20u, 0u);
    store16(rt, cell + 0x32u, 0u);
    put(rt, cell + 0x8u, static_cast<u32>(scaled_coord(x)));
    put(rt, cell + 0xCu, static_cast<u32>(scaled_coord(y)));
    put(rt, cell + 0x28u, 0u);
    call(rt, ctx, 0x08C7A9A0u, {owner});
    ctx.gpr[2] = 0u;
}

// Appends GE command words at the display list pointer and answers the advanced pointer.
u32 emit_ge_words(Runtime &rt, std::initializer_list<u32> words) {
    u32 cursor = word(rt, kGuListPointer);
    for (const u32 value : words) {
        put(rt, cursor, value);
        cursor += 4u;
    }
    put(rt, kGuListPointer, cursor);
    return cursor;
}

// 0x08B57598: write one packed vertex command (mode picks the format), or answer the jump table entry.
void func_08B57598(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 format = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (mode == 2u) { ctx.gpr[2] = 2u; return; }
    if (format >= 12u) { ctx.gpr[2] = 0u; return; }
    switch (format) {
    case 0u: {
        const u32 value = call(rt, ctx, 0x08B57F14u, {argument});
        ctx.gpr[2] = emit_ge_words(rt, {0x55000000u | low24(value)});
        return;
    }
    case 1u: {
        const u32 value = call(rt, ctx, 0x08B57F14u, {argument});
        ctx.gpr[2] = emit_ge_words(rt, {0x56000000u | low24(value),
                                        0x58000000u | low8(value >> 24)});
        return;
    }
    case 2u: {
        const u32 value = call(rt, ctx, 0x08B57F14u, {argument});
        ctx.gpr[2] = emit_ge_words(rt, {0x55000000u | low24(value),
                                        0x56000000u | low24(value),
                                        0x58000000u | low8(value >> 24)});
        return;
    }
    case 3u: {
        const u32 value = call(rt, ctx, 0x08B57F14u, {argument});
        ctx.gpr[2] = emit_ge_words(rt, {0x54000000u | low24(value)});
        return;
    }
    case 4u: {
        const u32 value = call(rt, ctx, 0x08B57F14u, {argument});
        ctx.gpr[2] = emit_ge_words(rt, {0x57000000u | low24(value)});
        return;
    }
    case 11u: {
        const float converted = static_cast<float>(static_cast<s32>(word(rt, argument)));
        const u32 bits = std::bit_cast<u32>(converted);
        const u32 command = 0x5B000000u | low24(bits >> 8);
        emit_ge_words(rt, {command});
        ctx.gpr[2] = command;
        return;
    }
    default:
        ctx.gpr[2] = 0x08C956F8u + (format << 2);
        return;
    }
}

// 0x088845A0: destructor — three slots at +0x40, two arrays of subobjects, then the intrusive pointer at +0x50.

// 0x0890B7AC: destructor — one intrusive pointer at +0x100, five owned members at +0x28..+0x8, then +0x4.
void func_0890B7AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 result = call(rt, ctx, 0x0890B3D0u, {self});
    const u32 owner = word(rt, self + 0x100u);
    if (owner != 0u) {
        const u32 first = word(rt, owner + 4u);
        put(rt, owner + 4u, first - 1u);
        if (first != 1u) {
            result = 1u;
        } else {
            result = call_vtable_slot(rt, ctx, owner, 8u);
            const u32 second = word(rt, owner + 8u);
            put(rt, owner + 8u, second - 1u);
            result = (second != 1u) ? 1u : call_vtable_slot(rt, ctx, owner, 0xCu);
        }
    }
    // Each element is destroyed through vtable slot 0 with a1 = the member word it came from.
    const u32 offsets[] = {0x28u, 0x20u, 0x18u, 0x10u, 0x8u};
    for (const u32 offset : offsets) {
        const u32 element = word(rt, self + offset + 4u);
        if (element != 0u) {
            result = call_indirect(rt, ctx, word(rt, word(rt, element)), {element, word(rt, self + offset)});
            put(rt, self + offset + 4u, 0u);
            put(rt, self + offset, 0u);
        }
    }
    const u32 last = word(rt, self + 4u);
    if (last != 0u) {
        result = call_indirect(rt, ctx, word(rt, word(rt, last)), {last, word(rt, self)});
        put(rt, self, 0u);
        put(rt, self + 4u, 0u);
    }
    ctx.gpr[2] = result;
}

// 0x08982D24: constructor of the 0x240-byte object (the three tables at +0x21C/+0x228 and the F0 block included).
void func_08982D24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self, 0x08CCCAB8u);
    put(rt, self + 0x38u, 0x08CCCCF4u);
    const u32 block = self + 0xF0u;
    call(rt, ctx, 0x08984824u, {block});
    const u32 sentinel = 0x08D74FD0u;
    const u32 node = self + 0x1E4u;
    put(rt, self + 0x1E0u, sentinel + 0xCu);
    put(rt, self + 0x1E4u, node);
    put(rt, node + 4u, node);
    put(rt, frame.at(4), sentinel + 0xCu);
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
    const u32 spare = allocated + 8u;
    if (spare != 0u) call(rt, ctx, 0x08B88F44u, {spare, frame.at(4)});
    call(rt, ctx, 0x08B5C078u, {allocated, node});
    const u32 block_start = word(rt, frame.at(4));
    const u32 header = block_start - 0xCu;
    if (header != sentinel) {
        const u32 counted = call(rt, ctx, 0x08B60804u, {block_start - 4u, 0xFFFFFFFFu});
        if (counted <= 0u) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0)});
    }
    put(rt, self + 0x1ECu, 1u);
    put(rt, self + 0x1F0u, 0u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x1F4u});
    call(rt, ctx, 0x08A76AA0u, {self + 0x1F8u});
    put_float(rt, self + 0x200u, 1.0f);
    const u32 first = self + 0x21Cu;
    const u32 second = self + 0x228u;
    store8(rt, self + 0x1FCu, 0u);
    store8(rt, self + 0x204u, 0u);
    store8(rt, self + 0x205u, 0u);
    store8(rt, self + 0x206u, 0u);
    put(rt, self + 0x208u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x210u, 0u);
    put(rt, self + 0x214u, 0u);
    put(rt, self + 0x218u, 0u);
    put(rt, self + 0x21Cu, 0u);
    put(rt, first + 8u, 0u);
    put(rt, first + 4u, 0u);
    put(rt, second + 8u, 0u);
    put(rt, second + 4u, 0u);
    put(rt, self + 0x228u, 0u);
    put_float(rt, self + 0x238u, 10.0f);
    put(rt, self + 0x23Cu, 0u);
    put(rt, self + 0x234u, 0u);
    store8(rt, block + 0xC4u, 1u);
    store8(rt, block + 0x38u, 0u);
    store8(rt, block + 0x36u, 0u);
    store8(rt, block + 0x37u, 0u);
    ctx.gpr[2] = self + 0x21Cu;
}

// 0x08A755B4: constructor — vtable, two temporary strings, the member at +0x14 and the tail object at +0x18.

// 0x089773D0: bump the four counters selected by the bit mask, decay the others, then hand a direction to 0x089AAA94.

// 0x089AA038: intersect two rectangles (the second one from 0x089A8B98), write the result through a1 and answer
// whether it has an area.

// 0x08987DC0 / 0x08988708 / 0x08988898: compare a temporary built from the member at +4 with one built from the
// second argument; when they match, parse the third and store it through word(self+0xC) (a byte, or a 64-bit value).
u32 parse_member(Runtime &rt, AllegrexContext &ctx, u32 self, u32 source, u32 parser, bool wide) {
    GuestFrame frame(ctx, 0x40u);
    u32 result = 0u;
    call(rt, ctx, 0x08B88F44u, {frame.at(8), self + 4u});
    call(rt, ctx, 0x08B85EB8u, {frame.at(4), 0x08CA3960u, frame.at(3)});
    const u32 left = word(rt, frame.at(8));
    const u32 right = word(rt, frame.at(4));
    const u32 left_length = word(rt, left - 0xCu);
    const u32 right_length = word(rt, right - 0xCu);
    put(rt, frame.at(0xCu), left_length);
    put(rt, frame.at(0x10u), right_length);
    const u32 longest = (right_length < left_length) ? left_length : right_length;
    result = call(rt, ctx, 0x08B36170u, {left, right, longest});
    u32 difference = left_length - right_length;
    if (result != 0u) difference = result;
    const u32 right_header = right - 0xCu;
    if (right_header != 0x08D74FD0u) {
        result = call(rt, ctx, 0x08B60804u, {right - 4u, 0xFFFFFFFFu});
        if (result <= 0u) result = call(rt, ctx, 0x08B8542Cu, {right_header, frame.at(2)});
    }
    const u32 left_header = left - 0xCu;
    if (left_header != 0x08D74FD0u) {
        result = call(rt, ctx, 0x08B60804u, {left - 4u, 0xFFFFFFFFu});
        if (result <= 0u) result = call(rt, ctx, 0x08B8542Cu, {left_header, frame.at(1)});
    }
    if (difference == 0u) {
        put(rt, frame.at(0x14u), 0x08D74FDCu);
        GuestResult stream = call_full(rt, ctx, 0x08987A74u, {source, frame.at(0x14u)});
        stream = call_full(rt, ctx, 0x08987A74u, {source, frame.at(0x14u)});
        if (stream.v0 == 0x101u) {
            const u32 target = word(rt, self + 0xCu);
            ctx.gpr[2] = stream.v0;
            ctx.gpr[3] = stream.v1;
            const GuestResult parsed = call_full(rt, ctx, parser, {word(rt, frame.at(0x14u))});
            if (wide) {
                put(rt, target, parsed.v0);
                put(rt, target + 4u, parsed.v1);
            } else {
                store8(rt, target, parsed.v0);
            }
        }
        result = call(rt, ctx, 0x0898682Cu, {source, 1u});
        const u32 block = word(rt, frame.at(0x14u));
        const u32 header = block - 0xCu;
        if (header != 0x08D74FD0u) {
            result = call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu});
            if (result <= 0u) result = call(rt, ctx, 0x08B8542Cu, {header, frame.at(0)});
        }
    }
    ctx.gpr[2] = result;
    return result;
}

void func_08987DC0(Runtime &rt, AllegrexContext &ctx) { parse_member(rt, ctx, ctx.gpr[4], ctx.gpr[5], 0x08985D14u, false); }
void func_08988708(Runtime &rt, AllegrexContext &ctx) { parse_member(rt, ctx, ctx.gpr[4], ctx.gpr[5], 0x08985DBCu, true); }
void func_08988898(Runtime &rt, AllegrexContext &ctx) { parse_member(rt, ctx, ctx.gpr[4], ctx.gpr[5], 0x08985DC8u, true); }

// 0x08A8EA78: destructor — two temporary strings, the pointer at +0x74, and the members at +0x54..+0x14.

// 0x089313D0: look the record up (id 0x29), copy its five floats and six fields through a1, answer 1 on success.
void func_089313D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const GuestResult opened = call_full(rt, ctx, 0x08930074u, {0x29u, argument});
    u32 result = 0u;
    if (opened.v0 != 0xFFFFFFFFu || opened.v1 != 0xFFFFFFFFu) {
        const u32 context = call(rt, ctx, 0x089363FCu, {});
        const u32 found = call(rt, ctx, 0x08936584u,
                               {context, opened.a1, opened.v0, opened.v1, frame.at(0)});
        if (found != 0u) {
            const u32 record = word(rt, frame.at(0));
            put_float(rt, out + 4u, float_at(rt, frame.at(4)));
            put_float(rt, out + 8u, float_at(rt, frame.at(8)));
            put_float(rt, out + 0xCu, float_at(rt, frame.at(0xCu)));
            put_float(rt, out + 0x10u, float_at(rt, frame.at(0x10u)));
            put_float(rt, out + 0x14u, float_at(rt, frame.at(0x14u)));
            put(rt, out, record);
            for (u32 i = 0; i < 6u; ++i) {
                const u32 current = call(rt, ctx, 0x089363FCu, {});
                const u32 converted = call(rt, ctx, 0x08937D90u,
                                           {word(rt, current + 0x1ECu), word(rt, frame.at(0x18u + 4u * i))});
                call(rt, ctx, 0x08B7BB6Cu, {out + 0x18u + 8u * i, converted});
            }
            const u32 half = rt.memory().load16(frame.at(0x34u));
            const u32 byte36 = rt.memory().load8(frame.at(0x36u));
            const u32 byte37 = rt.memory().load8(frame.at(0x37u));
            const u32 byte38 = rt.memory().load8(frame.at(0x38u));
            const u32 byte39 = rt.memory().load8(frame.at(0x39u));
            const u32 byte3A = rt.memory().load8(frame.at(0x3Au));
            store16(rt, out + 0x4Eu, half);
            store8(rt, out + 0x48u, byte36);
            store8(rt, out + 0x49u, byte37);
            store8(rt, out + 0x4Au, byte38);
            store8(rt, out + 0x4Bu, byte39);
            store8(rt, out + 0x4Cu, byte3A);
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08A9C998: build a record through 0x08A9109C (or copy the one already there) and drop the temporaries.
void func_08A9C998(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    call(rt, ctx, 0x08A6B5E0u, {frame.at(0xCu), argument});
    call(rt, ctx, 0x08AA6740u, {frame.at(0x18u), frame.at(0xCu)});
    call(rt, ctx, 0x08AA61D8u, {frame.at(0x18u), frame.at(0x10u), 8u});
    if (call(rt, ctx, 0x08B7C6BCu, {frame.at(0x10u), 0x08CB2668u, 4u}) != 0u) {
        store8(rt, frame.at(0x14u), 0u);
    } else {
        const u32 parsed = call(rt, ctx, 0x08B7DF7Cu, {frame.at(0x14u), 0u, 0xAu});
        put(rt, frame.at(0x40u), parsed);
        if (static_cast<s32>(parsed) >= 9) call(rt, ctx, 0x08AA6B80u, {frame.at(0x18u)});
        const u32 allocated = call(rt, ctx, 0x08A5BC20u, {0x34u, 4u});
        // The constructor's result is dropped: the delay slot saved the raw allocation into the temporary.
        call(rt, ctx, 0x08A90FE4u, {allocated, parsed});
        put(rt, frame.at(4), allocated);
        call(rt, ctx, 0x08A76AB4u, {frame.at(8), frame.at(4)});
        if (self != frame.at(8)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(8))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8)});
        const u32 existing = word(rt, self);
        u32 object = existing;
        if (object == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CB2588u});
            object = 0u;
        }
        call(rt, ctx, 0x08A9109Cu, {object, frame.at(0x18u)});
    }
    const u32 object = word(rt, frame.at(0xCu));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2670u});
    const u32 vtable = word(rt, object);
    call_indirect(rt, ctx, word(rt, vtable + 0x11Cu), {object});
    call(rt, ctx, 0x08C2FBF8u, {word(rt, frame.at(0x34u)), word(rt, frame.at(0x38u)), frame.at(0)});
    const u32 begin = word(rt, frame.at(0x34u));
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, frame.at(0x3Cu)) - begin) >> 2);
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, count << 2, 4u});
    call(rt, ctx, 0x08C0DD50u, {frame.at(0x1Cu), word(rt, frame.at(0x24u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
}

// 0x089614D8: reset the shared pointer chain and re-init; always ends by handing the member at +0x18 to 0x08B3457C.

} // namespace
} // namespace new_batch_main_1597
} // namespace gt::game

#define NEW1597(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1597::name, gt::ReturnKind::kind)
NEW1597(0x08B82B54u, func_08B82B54, Word);
NEW1597(0x08B57598u, func_08B57598, Word);
NEW1597(0x0890B7ACu, func_0890B7AC, Word);
NEW1597(0x08982D24u, func_08982D24, Word);
NEW1597(0x08987DC0u, func_08987DC0, Word);
NEW1597(0x08988708u, func_08988708, Word);
NEW1597(0x08988898u, func_08988898, Word);
NEW1597(0x089313D0u, func_089313D0, Word);
NEW1597(0x08A9C998u, func_08A9C998, Word);
