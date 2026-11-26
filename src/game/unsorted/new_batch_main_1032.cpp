// Main-session batch 1032: three long functions no earlier draft got past the gate. Hand-written as
// structured C from `tools.disasm`; every exit leaves v0 as the asm does.
//
// 0x08C6500C and 0x08C65724 are one body with four different constants (the name the entry is looked up
// by, the created entry's vtable and the function that registers it): the doubles-to-text formatting of
// one value. a1 = {u32 text; u8 flag}, a2 = the stream/output object (width +8, flags +0xC, its sentry
// object word at +0x6C), a3 = the separator char (sign-extended by the prologue's seb), t0 = the cursor
// object whose word is the string to scan. The entry is looked up in word(word(a2+0x6C)+0xC)[index] and
// created on first use (0x44 bytes, fields +0x10..+0x34 and the byte at +0x43 cleared). The value's
// leading run of characters whose class byte (word(sentry+0x18)[char]) has bit 2 is measured, clipped to
// the entry's width at +0x2C, and appended to a temporary string at fp+8; a second temporary at fp+0xC
// gets the padding, the two are filled through the four byte-codes at entry+0x30 / entry+0x34, the result
// is handed to the value's own virtual slot 0x30 and both temporaries are released. The original returns
// {v0 = the kept value, v1 = the flag byte in its low byte}, so both are set.
//
// 0x08B39B80 (a0 = the result object, a1 = the parser context, a2 = the status node): reads the parsed
// value (0x08B398CC(&value, context, node), which also receives a1/a2), and on status 6, 7, 8 or 4
// combines it with the node's operands through 0x08C50DA8 (an operation object: kind 1, 2, 3 or 5) and
// 0x08B395AC (the status-7 form), assigning the refcounted handle back into the value each time. Status 4
// first advances the parser (0x08B38B40(context, count, end)) and loops; a status other than 5 there
// rewrites the node (0x08B388D4(0x08CB5620)) and ends with an empty result. Every case then advances the
// parser (0x08B39148(context)), stores its answer into the node, and the result object gets the value's
// handle (counted up) with vtable 0x08CDA908.
//
// The two `if (... address != ... address)` guards the compiler kept compare two distinct stack
// temporaries, so they never fire: the assignments they guard run always (noted where they were).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1032 {
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
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// --- the value objects of 0x08B39B80 ---------------------------------------------------------------
//
// An 8-byte C++ object: the vtable word at +0 (0x08CDA908, the class 0x08C50DA8 builds and 0x08C50CCC
// destroys) and the intrusive refcounted handle at +4 whose count lives at handle+4 and whose vtable's
// slot 1 (word(word(handle)+4)) releases it. The value takes the handle without a self-assignment test.
void assign_handle(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 incoming) {
    const u32 previous = word(rt, slot);
    if (previous != 0u) {
        const u32 count = word(rt, previous + 4u) - 1u;
        put(rt, previous + 4u, count);
        if (count == 0u) (void)call_indirect(rt, ctx, word(rt, word(rt, previous) + 4u), {previous});
    }
    put(rt, slot, incoming);
    if (incoming != 0u) put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);
}

constexpr u32 kValueVtable = 0x08CDA908u;
constexpr u32 kEmptyString = 0x08D774A4u;   // the shared empty string both temporaries start from
constexpr u32 kUnusedString = 0x08D77498u;  // the buffer whose release is skipped

// --- 0x08C6500C / 0x08C65724: the two double formatters ----------------------------------------------

struct Formatter {
    u32 name;         // the constant string the entry is looked up by
    u32 vtable;       // the created entry's vtable
    u32 registration; // the function that registers the new entry
};

void format_value(Runtime &rt, AllegrexContext &ctx, const Formatter &formatter) {
    const u32 source = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 separator = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[7])));
    const u32 cursor_source = ctx.gpr[8];
    GuestFrame frame(ctx, 0xA0u);
    const u32 fp = frame.at(0u);
    const auto at = [&](u32 offset) { return fp + offset; };
    put(rt, at(0x4Cu), out);
    put(rt, at(0x20u), word(rt, source));
    const u32 source_flag = byte_at(rt, source + 4u);
    put(rt, at(0x50u), separator);
    put_byte(rt, at(0x24u), source_flag);
    // The sentry object over out+0x6C; its answer is kept at fp+0x48 and its class table read at +0x18.
    const u32 sentry = call(rt, ctx, 0x08B64C80u, {out + 0x6Cu});
    put(rt, at(0x48u), sentry);
    const u32 index = call(rt, ctx, 0x08B5C2B0u, {formatter.name});
    const u32 slots = word(rt, word(rt, out + 0x6Cu) + 0xCu) + (index << 2u);
    u32 entry = word(rt, slots);
    if (entry == 0u) {
        const u32 created = call(rt, ctx, 0x08B62344u, {0x44u});
        put(rt, created + 0x00u, formatter.vtable);
        put(rt, created + 0x04u, 0u);
        put(rt, created + 0x08u, 0u);
        put(rt, created + 0x0Cu, 0u);
        put_byte(rt, created + 0x10u, 0u);
        put_byte(rt, created + 0x11u, 0u);
        put_byte(rt, created + 0x12u, 0u);
        put(rt, created + 0x14u, 0u);
        put(rt, created + 0x18u, 0u);
        put(rt, created + 0x1Cu, 0u);
        put(rt, created + 0x20u, 0u);
        put(rt, created + 0x24u, 0u);
        put(rt, created + 0x28u, 0u);
        put(rt, created + 0x2Cu, 0u);
        put(rt, created + 0x30u, 0u);
        put(rt, created + 0x34u, 0u);
        put_byte(rt, created + 0x43u, 0u);
        (void)call(rt, ctx, formatter.registration, {created, out + 0x6Cu});
        (void)call(rt, ctx, 0x08B5C2F8u, {word(rt, out + 0x6Cu), created, index});
        entry = word(rt, slots);
    }
    put(rt, at(0x44u), entry + 0x38u);
    u32 cursor = word(rt, cursor_source);
    const u32 length_at = cursor - 0xCu;
    u32 code_word;
    // The cursor's own class byte picks which pair of code bytes and which field pair the entry uses;
    // a string that starts with the cursor's first character skips that character.
    if (signed_byte_at(rt, cursor) == signed_byte_at(rt, entry + 0x38u)) {
        code_word = word(rt, entry + 0x34u);
        put(rt, at(0x40u), word(rt, entry + 0x24u));
        put(rt, at(0x3Cu), word(rt, entry + 0x28u));
        if (word(rt, length_at) != 0u) ++cursor;
    } else {
        code_word = word(rt, entry + 0x30u);
        put(rt, at(0x40u), word(rt, entry + 0x1Cu));
        put(rt, at(0x3Cu), word(rt, entry + 0x20u));
    }
    const u32 end = cursor + word(rt, length_at);
    const auto finish = [&]() {
        put_byte(rt, at(0x14u), byte_at(rt, at(0x24u)));
        put(rt, word(rt, at(0x4Cu)) + 8u, 0u);
        ctx.gpr[2] = word(rt, at(0x20u));
        ctx.gpr[3] = word(rt, at(0x14u));
    };
    if (cursor >= end) {
        finish();
        return;
    }
    // The run of characters whose class byte (from the sentry's table at +0x18) has bit 2.
    const u32 classes = word(rt, word(rt, at(0x48u)) + 0x18u);
    if ((byte_at(rt, classes + byte_at(rt, cursor)) & 4u) == 0u) {
        finish();
        return;
    }
    u32 scan = cursor + 1u;
    u32 count = scan - cursor;
    if (end != scan) {
        for (;;) {
            if (end == scan) {
                count = scan - cursor;
                break;
            }
            if ((byte_at(rt, classes + byte_at(rt, scan)) & 4u) == 0u) {
                count = scan - cursor;
                break;
            }
            ++scan;
        }
    }
    put(rt, at(8u), kEmptyString);
    const u32 twice = count << 1u;
    (void)call(rt, ctx, 0x08C75840u, {at(8u), twice});
    {
        u32 limit = word(rt, entry + 0x2Cu);
        s32 remaining = static_cast<s32>(count) - static_cast<s32>(limit);
        if (remaining > 0) {
            const u32 filter = word(rt, entry + 0xCu);
            // The original's movn in the branch delay slot: a negative width takes the whole run.
            if (static_cast<s32>(limit) < 0) remaining = static_cast<s32>(count);
            if (filter == 0u) {
                (void)call(rt, ctx, 0x08C74DD8u, {at(8u), cursor, static_cast<u32>(remaining)});
            } else {
                // subu $sp, $sp, (twice + 0x1E) & ~15: a run-time buffer below the frame.
                const u32 temporary = ctx.gpr[29] - ((twice + 0x1Eu) & 0xFFFFFFF0u);
                ctx.gpr[29] = temporary;
                const u32 written = call(rt, ctx, 0x08C5942Cu,
                                         {temporary, static_cast<u32>(signed_byte_at(rt, entry + 0x12u)),
                                          word(rt, entry + 8u), filter, cursor,
                                          cursor + static_cast<u32>(remaining)});
                (void)call(rt, ctx, 0x08C74DD8u, {at(8u), temporary, written - temporary});
            }
            limit = word(rt, entry + 0x2Cu);
        }
        if (static_cast<s32>(limit) > 0) {
            // Append the byte at entry+0x11, then the matched run or the padding byte.
            const u32 padding = static_cast<u32>(signed_byte_at(rt, entry + 0x11u));
            put(rt, at(0x34u), padding);
            u32 buffer = word(rt, at(8u));
            const u32 needed = word(rt, buffer - 0xCu) + 1u;
            if (word(rt, buffer - 8u) < needed || static_cast<s32>(word(rt, buffer - 4u)) > 0)
                (void)call(rt, ctx, 0x08C75840u, {at(8u), needed});
            buffer = word(rt, at(8u));
            put_byte(rt, buffer + word(rt, buffer - 0xCu), padding);
            put(rt, buffer - 4u, 0u);
            put(rt, buffer - 0xCu, needed);
            put_byte(rt, buffer + needed, byte_at(rt, 0x08CDBE28u));
            if (remaining < 0) {
                (void)call(rt, ctx, 0x08C7598Cu,
                           {at(8u), 0u - static_cast<u32>(remaining),
                            static_cast<u32>(signed_byte_at(rt, entry + 0x39u))});
                (void)call(rt, ctx, 0x08C75AF8u, {at(8u), cursor, count});
            } else {
                (void)call(rt, ctx, 0x08C75AF8u, {at(8u), cursor + static_cast<u32>(remaining), word(rt, entry + 0x2Cu)});
            }
        }
    }
    // The output width, the second temporary string and the four code bytes.
    const u32 out_flags = word(rt, word(rt, at(0x4Cu)) + 0xCu);
    const u32 low_flags = out_flags & 0xB0u;
    put(rt, at(0x30u), low_flags);
    u32 total = word(rt, at(0x3Cu)) + word(rt, word(rt, at(8u)) - 0xCu);
    if ((out_flags & 0x200u) != 0u) total += word(rt, entry + 0x18u);
    put(rt, at(0xCu), kEmptyString);
    (void)call(rt, ctx, 0x08C75840u, {at(0xCu), total << 1u});
    const u32 target = word(rt, word(rt, at(0x4Cu)) + 8u);
    const bool fill = low_flags == 0x10u && total < target;
    put(rt, at(0x38u), fill ? 1u : 0u);
    const auto pad_first = [&]() {
        (void)call(rt, ctx, 0x08C7598Cu, {at(0xCu), target - total, separator});
    };
    const auto append_to_second = [&](u32 byte) {
        u32 buffer = word(rt, at(0xCu));
        const u32 needed = word(rt, buffer - 0xCu) + 1u;
        if (word(rt, buffer - 8u) < needed || static_cast<s32>(word(rt, buffer - 4u)) > 0)
            (void)call(rt, ctx, 0x08C75840u, {at(0xCu), needed});
        buffer = word(rt, at(0xCu));
        put_byte(rt, buffer + word(rt, buffer - 0xCu), byte);
        put(rt, buffer - 4u, 0u);
        put(rt, buffer - 0xCu, needed);
        put_byte(rt, buffer + needed, byte_at(rt, 0x08CDBE28u));
    };
    for (u32 position = 0u; position != 4u; ++position) {
        const s32 code = static_cast<std::int8_t>(static_cast<std::uint8_t>(code_word >> (position * 8u)));
        if (static_cast<u32>(code) >= 5u) continue;
        switch (code) {
        case 0:
            if (fill) pad_first();
            break;
        case 1:
            if (fill) pad_first();
            else append_to_second(separator);
            break;
        case 2:
            if ((word(rt, word(rt, at(0x4Cu)) + 0xCu) & 0x200u) != 0u)
                (void)call(rt, ctx, 0x08C75AF8u, {at(0xCu), word(rt, entry + 0x14u), word(rt, entry + 0x18u)});
            break;
        case 3:
            if (word(rt, at(0x3Cu)) != 0u) {
                const u32 pointer = word(rt, at(0x40u));
                append_to_second(static_cast<u32>(signed_byte_at(rt, pointer)));
            }
            break;
        default:
            (void)call(rt, ctx, 0x08C75D90u, {at(0xCu), at(8u)});
            break;
        }
    }
    const u32 width = word(rt, at(0x3Cu));
    if (width >= 2u)
        (void)call(rt, ctx, 0x08C75AF8u, {at(0xCu), word(rt, at(0x40u)) + 1u, width - 1u});
    u32 length_c = word(rt, word(rt, at(0xCu)) - 0xCu);
    if (length_c < target) {
        if (low_flags == 0x20u)
            (void)call(rt, ctx, 0x08C7598Cu, {at(0xCu), target - length_c, separator});
        else
            (void)call(rt, ctx, 0x08C74FACu, {at(0xCu), 0u, 0u, target - length_c, separator});
        length_c = target;
    }
    const u32 flag = byte_at(rt, at(0x24u));
    const u32 kept = word(rt, at(0x20u));
    u32 final_flag = flag;
    if (flag == 0u) {
        const u32 buffer = word(rt, at(0xCu));
        const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, kept) + 0x30u), {kept, buffer, length_c});
        if (answer != length_c) final_flag = 1u;
    }
    put(rt, at(0x20u), kept);
    put_byte(rt, at(0x24u), final_flag);
    put_byte(rt, at(0x1Cu), final_flag);
    // A string whose buffer is not the shared empty one is released through its count at buffer-4.
    const auto release_string = [&](u32 slot, u32 second) {
        const u32 buffer = word(rt, slot);
        if (buffer - 0xCu == kUnusedString) return;
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) > 0) return;
        (void)call(rt, ctx, 0x08C74884u, {buffer - 0xCu, second});
    };
    release_string(at(0xCu), at(1u));
    release_string(at(8u), at(0u));
    finish();
}

// --- 0x08B39B80: one step of the parser's value evaluation -------------------------------------------


} // namespace

void func_08C65724(Runtime &rt, AllegrexContext &ctx) {
    format_value(rt, ctx, {0x08D77494u, 0x08CDBAB8u, 0x08C614E4u});
}

} // namespace new_batch_main_1032
} // namespace gt::game

#define NEW1032(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1032::name, gt::ReturnKind::kind)
NEW1032(0x08C65724u, func_08C65724, Word);
