// Main-session batch 1001: functions drafted from `tools.disasm` (structured C, hand-written).
// DRAFT ONLY — not gated in this session: the differential gate and the synthetic gate run afterwards.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3):
// `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value. The delay
// slot of a `jal` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` delay slot only when the branch is
// taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 where a caller could read them) is set
// as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write
// are (`GuestFrame`, offsets as in the original).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1001 {
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
// A guest call whose v0 / v1 / f0 stay in the registers like the original's.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result;
}
inline u32 call(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments).v0;
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// A guest call with a0-a3 / t0-t3 set like the original sets them before the jal (in order): the registers the callee
// does not take keep the caller's value.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}

// 0x088DF790 (a0 = object): the object's state reset. Clears the byte and word flags, 088D5DF8(self+4) is stored at
// +0x59C, the four entries at +0x77C (+0x20 each) and +0x780 (0 each) become -1, 088DC1B8(self, 3) runs, then the
// table at +0x130 is cleared, 088D5E80(self+4) fills its +0x20 / +0x24 pair and 088DE24C(self, 1, 0) runs. Per entry
// of 088D9F80(self): 088E8184 and 088E8CC0 on *(self+0xC0)+0x2F8*i, +0x2D0 / +0x2D4 cleared and the index stamped;
// 088F89AC(*(self+0x14C)) when set, 088D9BBC(self, 0, 0), and a memset of *(self+0x644) with 0x340 zeros as a tail call.

// The copy-on-write string push_back the compiler inlined four times in func_08B5C4B0: `ch` goes at data[size], the
// size grows by one, the sharing count drops to zero and the terminator byte (the global at 0x08CDBE28) lands at
// data[new size]. A new size past the capacity, or a shared buffer (count above zero), reallocates first through
// 0x08C75840. `out` is the wrapper whose word 0 is the data pointer (its size / capacity / count live at -0xC / -8 / -4).
void string_push_back(Runtime &rt, AllegrexContext &ctx, u32 out, u32 character) {
    u32 data = word(rt, out);
    u32 size = word(rt, data - 0xCu);
    const u32 grown = size + 1u;
    if (word(rt, data - 0x8u) < grown || signed_of(word(rt, data - 4u)) > 0) {
        call_set(rt, ctx, 0x08C75840u, {out, grown});
        data = word(rt, out);
        size = word(rt, data - 0xCu);
    }
    rt.memory().store8(data + size, static_cast<std::uint8_t>(character));
    const u32 terminator = byte_u(rt, 0x08CDBE28u);
    const u32 base = word(rt, out);
    put(rt, base - 0xCu, grown);
    put(rt, base - 4u, 0u);
    rt.memory().store8(base + grown, static_cast<std::uint8_t>(terminator));
}

// 0x08B5C4B0 (a0 = output string wrapper, a1 = record): writes the shared empty string to out[0] and builds
// "name0=value0;name1=value1;..." from the six-entry name table *(*(record)+0x10) and the static name list at
// *(0x08C95ED8). A null first name appends a single '*' at the current size instead; a zero second name appends the
// first name alone; the loop compares the table's adjacent names and, when all five pairs match, appends the first
// name alone too. Every append is 0x08C75AF8(out, text, length) after 0x08B35F60(text). v0 = out on every path.
void func_08B5C4B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);

    const u32 descriptor = word(rt, record);
    const u32 names = word(rt, descriptor + 0x10u);
    put(rt, out, 0x08D77498u + 0xCu);  // the shared empty string's data (its header starts at 0x08D77498)
    const u32 first = word(rt, names);
    if (first == 0u) {
        call_set(rt, ctx, 0x08C74FACu, {out, 0u, word(rt, 0x08D77498u), 1u, 0x2Au});
        ctx.gpr[2] = out;
        return;
    }
    bool differing = false;
    u32 matched = 0u;
    if (word(rt, names + 4u) != 0u) {
        u32 entry = names;
        for (;;) {
            const u32 compared = call(rt, ctx, 0x08B35FD8u, {word(rt, entry), word(rt, entry + 4u)});
            ++matched;
            if (compared != 0u) {
                differing = true;
                break;
            }
            if (matched == 5u) break;
            entry += 4u;
        }
    }
    if (!differing) {
        const u32 length = call(rt, ctx, 0x08B35F60u, {first});
        call_set(rt, ctx, 0x08C74DD8u, {out, first, length});
        ctx.gpr[2] = out;
        return;
    }
    call_set(rt, ctx, 0x08C75840u, {out, 0x80u});
    const u32 static_names = word(rt, 0x08C95ED8u);
    {
        const u32 name = word(rt, static_names);
        const u32 length = call(rt, ctx, 0x08B35F60u, {name});
        call_set(rt, ctx, 0x08C75AF8u, {out, name, length});
    }
    string_push_back(rt, ctx, out, 0x3Du);  // '='
    {
        const u32 value = word(rt, word(rt, word(rt, record) + 0x10u));
        const u32 length = call(rt, ctx, 0x08B35F60u, {value});
        call_set(rt, ctx, 0x08C75AF8u, {out, value, length});
    }
    for (u32 offset = 4u; offset != 0x18u; offset += 4u) {
        string_push_back(rt, ctx, out, 0x3Bu);  // ';'
        const u32 name = word(rt, word(rt, 0x08C95ED8u) + offset);
        const u32 length = call(rt, ctx, 0x08B35F60u, {name});
        call_set(rt, ctx, 0x08C75AF8u, {out, name, length});
        string_push_back(rt, ctx, out, 0x3Du);  // '='
        const u32 value = word(rt, word(rt, word(rt, record) + 0x10u) + offset);
        const u32 value_length = call(rt, ctx, 0x08B35F60u, {value});
        call_set(rt, ctx, 0x08C75AF8u, {out, value, value_length});
    }
    ctx.gpr[2] = out;
}

// A virtual call through table slot `offset` of `object` (a0 = object), as the compiler issues it: v1 = the table
// (object[0]), v0 = the slot's function address, jalr v0 — so a callee that never writes v0 hands its own address on.
u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_set(rt, ctx, function, {object});
}

// The intrusive reference counts the shared-owner objects carry, taken with ll/sc in the original: +4 is the strong
// count of the pointed-to resource, +8 the weak one. Reading and writing the word is the same memory effect the atomic
// pair has; `true` reports the count reached zero.
bool release_strong(Runtime &rt, u32 control) {
    const u32 old = word(rt, control + 4u);
    put(rt, control + 4u, old - 1u);
    return old == 1u;
}
bool release_weak(Runtime &rt, u32 control) {
    const u32 old = word(rt, control + 8u);
    put(rt, control + 8u, old - 1u);
    return old == 1u;
}
void add_ref(Runtime &rt, u32 control) {
    if (control != 0u) put(rt, control + 4u, word(rt, control + 4u) + 1u);
}
// One strong reference dropped: at zero the resource's slot 0x8 (dispose) runs, then the weak count is dropped and its
// own zero runs slot 0xC (deallocate).
void drop_ref(Runtime &rt, AllegrexContext &ctx, u32 control) {
    if (control == 0u) return;
    if (!release_strong(rt, control)) return;
    call_slot(rt, ctx, control, 0x8u);
    if (release_weak(rt, control)) call_slot(rt, ctx, control, 0xCu);
}

// 0x0889E258 (a0 = object, a1 = {pointer, control} pair): rebinds object[0x24]'s owner. Takes four references on the
// pair's control block, 08B34888(object[0x24]) runs, and when object[0x24][0x2C] is null 0889DE18(object) answers the
// whole rebind instead. Otherwise a 0x10-byte owner (vtable 0x08CC2530, the object, the pair's pointer, the control
// block) is allocated, the old owner at +0x34[4] is released through slot 0, the owner goes to +0x34, the lazily
// guarded singleton at 0x08D75F10 (vtable 0x08CC2560) behind 08B6441C / 08B64494 lands at +0x34[4], one reference is
// dropped, 08B34B24(object[0x24]) runs, and the last two references are dropped - the second one ends in a tail call
// to the resource's slot 0xC when it deallocates. v0 is the last callee's on the whole path that ends at 0889E510, 1
// on the 0889E514 exits, and the vtable pointer into the tail call.

// The 8-byte {first, second} pair push_back the compiler inlined four times in func_08B38E74: the pair is built in
// `scratch` and, when the end pointer already equals the capacity, 0x08C4F48C(vector, end, scratch) grows the vector;
// otherwise the pair lands at the end and the end pointer moves by 8 (a null end only bumps the end pointer, as the
// original does — an inconsistent {0, capacity != 0} vector is the only way to reach it).
void vector_push_pair(Runtime &rt, AllegrexContext &ctx, u32 vector, u32 first, u32 second, u32 scratch) {
    put(rt, scratch, first);
    put(rt, scratch + 4u, second);
    const u32 end = word(rt, vector + 4u);
    if (end == word(rt, vector + 8u)) {
        call_set(rt, ctx, 0x08C4F48Cu, {vector, end, scratch});
        return;
    }
    if (end != 0u) {
        put(rt, end, first);
        put(rt, end + 4u, second);
        put(rt, vector + 4u, word(rt, vector + 4u) + 8u);
    } else {
        put(rt, vector + 4u, 8u);
    }
}

// Emits the parse error 0x08B388D4(message) and hands its answer back in v0 and v1 (the original's `v0 = v1 = answer`).
void parse_error(Runtime &rt, AllegrexContext &ctx, u32 message) {
    const u32 answer = call(rt, ctx, 0x08B388D4u, {message});
    ctx.gpr[2] = answer;
    ctx.gpr[3] = answer;
}

// 0x08B38E74 (a0 = text state): takes the first token from 08B38938; '^' picks the "true" flag and reads the next
// token, ']' is the invalid-start error message 0x08CB55CC, and end of text the 0x08CB55B0 one. Otherwise a 0x18-byte
// result (vtable 0x08CD93D8, +0x14 = the flag) is allocated and the loop reads a token: a plain token pushes
// {previous, previous}; '-' followed by ']' pushes {previous, previous} and {0x2D, 0x2D} and ends the parse; '-' followed
// by anything else pushes {previous, token} and reads on. A ']' token ends the parse, end of text errors through the
// result's slot 4. Success releases the owner at text state +0x10, stores the result at +0x10 and returns 0 (v0 = v1 = 0).
void func_08B38E74(Runtime &rt, AllegrexContext &ctx) {
    const u32 text = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);

    u32 previous = call(rt, ctx, 0x08B38938u, {text});
    if (previous == 0xFFFFFFFFu) {
        parse_error(rt, ctx, 0x08CB55B0u);
        return;
    }
    u32 flag = 1u;  // s3: 1 unless the text starts with '^'
    if (previous == 0x5Eu) {
        flag = 0u;
        previous = call(rt, ctx, 0x08B38938u, {text});
        if (previous == 0xFFFFFFFFu) {
            parse_error(rt, ctx, 0x08CB55B0u);
            return;
        }
    }
    if (previous == 0x5Du) {  // ']'
        parse_error(rt, ctx, 0x08CB55CCu);
        return;
    }
    const u32 made = call(rt, ctx, 0x08B62344u, {0x18u});
    const u32 vector = made + 8u;
    put(rt, made, 0x08CD93D8u);
    put(rt, made + 4u, 0u);
    put(rt, made + 8u, 0u);
    put(rt, made + 0xCu, 0u);
    put(rt, made + 0x10u, 0u);
    put8(rt, made + 0x14u, flag);
    if (previous == 0xFFFFFFFFu) {  // unreachable, the branch exists in the original: 08B38F18
        call_slot(rt, ctx, made, 0x4u);
        parse_error(rt, ctx, 0x08CB55B0u);
        return;
    }
    for (;;) {
        u32 current = call(rt, ctx, 0x08B38938u, {text});
        if (current == 0xFFFFFFFFu) {  // 08B39090
            call_slot(rt, ctx, made, 0x4u);
            parse_error(rt, ctx, 0x08CB55B0u);
            return;
        }
        if (current != 0x2Du) {  // not '-'
            vector_push_pair(rt, ctx, vector, previous, previous, frame.at(0x18u));
        } else {
            const u32 after = call(rt, ctx, 0x08B38938u, {text});
            if (after == 0xFFFFFFFFu) {  // 08B390B4, through the result's slot 4
                call_slot(rt, ctx, made, 0x4u);
                parse_error(rt, ctx, 0x08CB55B0u);
                return;
            }
            if (after == 0x5Du) {  // ']': the second pair closes the parse
                vector_push_pair(rt, ctx, vector, previous, previous, frame.at(0x18u));
                vector_push_pair(rt, ctx, vector, 0x2Du, 0x2Du, frame.at(0x10u));
                break;  // 08B38FD0 / 08B38FE0 go straight to the success path 08B390DC
            }
            vector_push_pair(rt, ctx, vector, previous, after, frame.at(0xCu));
            current = call(rt, ctx, 0x08B38938u, {text});  // 08B39024
        }
        if (current == 0x5Du) break;
        if (current == 0xFFFFFFFFu) {
            call_slot(rt, ctx, made, 0x4u);
            parse_error(rt, ctx, 0x08CB55B0u);
            return;
        }
        previous = current;
    }
    // success path 08B390DC
    const u32 slot = text + 0xCu;
    const u32 old = word(rt, slot + 4u);
    if (old != 0u) {
        const u32 count = word(rt, old + 4u) - 1u;
        put(rt, old + 4u, count);
        if (count == 0u) call_slot(rt, ctx, old, 0x4u);
    }
    put(rt, slot + 4u, made);
    put(rt, made + 4u, word(rt, made + 4u) + 1u);
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = 0u;
}

inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

// 0x08886B00 (a0 = object, a1 = source, a2 = text): with bit 0 of *(*(object+0x68)+0x26) clear, assigns the text to the
// string at object+0x184 and splits it at the first ':' and the first '%' into three byte buffers in the frame (before
// the ':', between '%' and the ':', and the rest). A text equal to 0x08CA06C0 formats the last buffer through
// 08B37640 into the frame string at +0x100 and, when 0886AF64(source) is set, a variant into +0x180; a text equal to
// 0x08CA0700 copies the buffer through 08B7BB6C; anything else keeps the source text itself. The record
// *(object+0x68) gets bit 0x10000 set, 0886B0A4 / 0886B0C8 fill its +8 / +4, the chosen name (and the variant) fill
// +0xC (+0x10) with the mode byte at +0x24, then 08BA7404, object+0x188 = the middle buffer, 0886BC14 and 08885994.
// v0 = 1, or 0 when the flag was set. The register-inherited first values of colon / percent / tail are the original's
// own uninitialised reads on the degenerate paths (empty and one-byte text), reproduced from the caller's s1 / s3 / s6.
void func_08886B00(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 text = ctx.gpr[6];
    GuestFrame frame(ctx, 0x230u);
    const u32 holder = self + 0x68u;

    if ((rt.memory().load16(word(rt, self + 0x68u) + 0x26u) & 1u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 length = call(rt, ctx, 0x08B35F60u, {text});
    call_set(rt, ctx, 0x08B945C4u, {self + 0x184u, text, length});
    put8(rt, frame.at(0x40u), 0u);
    put8(rt, frame.at(0x20u), 0u);
    put8(rt, frame.at(0u), 0u);

    u32 start = 0u;               // fp
    u32 index = 0u;               // s0
    u32 colon = ctx.gpr[17];      // s1
    u32 percent = ctx.gpr[19];    // s3
    u32 tail = ctx.gpr[22];       // s6
    u32 rest = ctx.gpr[17];       // s1 on the empty-text path (the original's own uninitialised read)
    if (sext8(byte_u(rt, text)) != 0u) {
        colon = 0xFFFFFFFFu;
        percent = 0xFFFFFFFFu;
        index = 0u;
        u32 c = sext8(byte_u(rt, text));
        for (;;) {
            if (c == 0x3Au) {
                colon = index;
            } else if (c == 0x25u) {
                percent = index;
            }
            index += 1u;
            c = sext8(byte_u(rt, text + index));
            if (c == 0u) break;
            tail = 0u;
        }
        if (colon != 0xFFFFFFFFu) {
            call_set(rt, ctx, 0x08B79A88u, {frame.at(0u), text, colon});
            start = colon + 1u;
            put8(rt, frame.at(0u) + colon, 0u);
            tail = start;
        }
        rest = index - tail;
        if (percent != 0xFFFFFFFFu) {
            const u32 middle = index - percent - 1u;
            call_set(rt, ctx, 0x08B79A88u, {frame.at(0x20u), text + percent + 1u, middle});
            put8(rt, frame.at(0x20u) + middle, 0u);
            rest = percent - tail;
        }
    }
    call_set(rt, ctx, 0x08B79A88u, {frame.at(0x40u), text + start, rest});
    put8(rt, frame.at(0x40u) + rest, 0u);

    u32 chosen = text;  // s2
    u32 variant = 0u;   // s3
    if (call(rt, ctx, 0x08B35FD8u, {frame.at(0u), 0x08CA06C0u}) == 0u) {
        // "/character/%s" — the callee's saved a3 / t0-t3 are the original's leftovers from the strcmp above and the
        // format takes a single argument, so nothing reads them.
        call_set(rt, ctx, 0x08B37640u, {frame.at(0x100u), 0x08CA06C8u, frame.at(0x40u)});
        const u32 picked = call(rt, ctx, 0x0886AF64u, {source});
        chosen = frame.at(0x100u);
        if (picked != 0u) {
            call_set(rt, ctx, 0x08B37640u, {frame.at(0x180u), 0x08CA06D8u, frame.at(0x40u)});
            variant = frame.at(0x180u);
        }
    } else if (call(rt, ctx, 0x08B35FD8u, {frame.at(0u), 0x08CA0700u}) == 0u) {
        call_set(rt, ctx, 0x08B7BB6Cu, {frame.at(0x100u), frame.at(0x40u)});
        chosen = frame.at(0x100u);
    }

    const u32 record = word(rt, holder);
    put(rt, record + 0x24u, word(rt, record + 0x24u) | 0x00010000u);
    const u32 first = call(rt, ctx, 0x0886B0A4u, {source});
    put(rt, word(rt, holder) + 8u, first);
    const u32 second = call(rt, ctx, 0x0886B0C8u, {source, 0u});
    put(rt, word(rt, holder) + 4u, second);
    const u32 target = word(rt, holder);
    if (variant != 0u) {
        put8(rt, target + 0x24u, 2u);
        const u32 variant_length = call(rt, ctx, 0x08B35F60u, {variant});
        call_set(rt, ctx, 0x08B945C4u, {target + 0xCu, variant, variant_length});
        const u32 chosen_length = call(rt, ctx, 0x08B35F60u, {chosen});
        call_set(rt, ctx, 0x08B945C4u, {target + 0x10u, chosen, chosen_length});
    } else {
        put8(rt, target + 0x24u, 1u);
        const u32 chosen_length = call(rt, ctx, 0x08B35F60u, {chosen});
        call_set(rt, ctx, 0x08B945C4u, {target + 0xCu, chosen, chosen_length});
    }
    call_set(rt, ctx, 0x08BA7404u, {holder});
    const u32 middle_length = call(rt, ctx, 0x08B35F60u, {frame.at(0x20u)});
    call_set(rt, ctx, 0x08B945C4u, {self + 0x188u, frame.at(0x20u), middle_length});
    call_set(rt, ctx, 0x0886BC14u, {source});
    call_set(rt, ctx, 0x08885994u, {self, source});
    ctx.gpr[2] = 1u;
}

// 0x0896E614 (a0 = destination handle slot, a2 = mode, a3 = source): builds a local through 08969244(sp+0x10) first.
// Mode 0: 08966B20(*(sp+0x10), +0x14, +0x18, +0x1C, +0x20) answers four handles, 08A98C1C(+0x24, 4) a four-slot
// container whose four elements (word(container+0x14) + 4k) take copies of them through 08A85B1C / 08A5BAD8 with the
// frame temporaries at +0xC / +8 / +4 / +0, and the container is assigned into a0 when it is a different slot. Mode 1:
// the container is built from a3 by 08A98674 and, with exactly four elements, each element's object is asked through
// table slot 0x3C and 089668D8(*(sp+0x10), the four answers) runs. Then the container and the local are destroyed by
// 08A5B9EC; v0 is whatever the last callee left (no path writes it).

// Drops one intrusive reference at `slot` (the object it points at loses one count and runs table slot 4 when the count
// reaches zero); a null slot does nothing.
void release_slot(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 object = word(rt, slot);
    if (object == 0u) return;
    const u32 count = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, count);
    if (count == 0u) call_slot(rt, ctx, object, 0x4u);
}

// Gets one reference: the count at +4 of a non-null object grows by one.
void acquire_slot(Runtime &rt, u32 slot) {
    const u32 object = word(rt, slot);
    if (object != 0u) put(rt, object + 4u, word(rt, object + 4u) + 1u);
}

// 0x08B38568 (a0 = object, a1 = source): builds the object's function table. The frame's two temporaries (sp+0 and
// sp+8, each {vtable, intrusive pointer}) and the third at sp+0x18 are set up, 08B39804(sp+8, sp+0x10, source) fills
// them, and a null sp+0xC takes the 08B3A3D0(0x08CB5568) error path with v0 = 0. Otherwise the table at object+4 gets
// (08B37B44(sp+8) + 2) zeroed words, object+8 / +0xC are two 08B37B34(object) answers handed to 08B38160, the sp+0xC
// pointer moves to sp+4's object with an acquire after its release, sp+0's object takes its vtable 0x08CDA8F8 and
// releases sp+4, and the 0x10-byte result is allocated: +0xC = 08B3D4B8(object), +8 = the entry count of the
// object+0x14 list (16 bytes each), +4 = 8-byte records (word = the entry's +0xC, byte +4 = the entry's +8, the
// entry's +9 cleared). Both temporaries are released through their vtable 0x08CDA8F8 and v0 = the result.
void func_08B38568(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);

    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x10u), word(rt, self));
    put(rt, frame.at(0x1Cu), 0x08CDA8E8u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x20u), 0u);
    call_set(rt, ctx, 0x08B39804u, {frame.at(0x8u), frame.at(0x10u), source});
    if (word(rt, frame.at(0xCu)) == 0u) {
        call_set(rt, ctx, 0x08B3A3D0u, {0x08CB5568u});
        put(rt, frame.at(0x8u), 0x08CDA8F8u);
        release_slot(rt, ctx, frame.at(0xCu));
        put(rt, frame.at(0x1Cu), 0x08CDA8F8u);
        release_slot(rt, ctx, frame.at(0x20u));
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 words = call(rt, ctx, 0x08B37B44u, {frame.at(0x8u)}) + 2u;
    put(rt, self + 4u, call(rt, ctx, 0x08B6461Cu, {words << 2}));
    if (static_cast<s32>(words) > 0) {
        for (u32 index = 0u; index < words; ++index) put(rt, word(rt, self + 4u) + index * 4u, 0u);
    }
    put(rt, self + 8u, call(rt, ctx, 0x08B37B34u, {self}));
    const u32 second = call(rt, ctx, 0x08B37B34u, {self});
    put(rt, self + 0xCu, second);
    call_set(rt, ctx, 0x08B38160u, {self, frame.at(0x8u), word(rt, self + 8u), second});

    put(rt, frame.at(0u), 0x08CDA908u);
    put(rt, frame.at(0x4u), 0u);  // 088B38654: the first temporary's pointer is emptied
    release_slot(rt, ctx, frame.at(0xCu));
    put(rt, frame.at(0xCu), word(rt, frame.at(0x4u)));
    acquire_slot(rt, frame.at(0xCu));
    put(rt, frame.at(0u), 0x08CDA8F8u);
    release_slot(rt, ctx, frame.at(0x4u));

    const u32 result = call(rt, ctx, 0x08B3D4B8u, {self});
    call_set(rt, ctx, 0x08B3846Cu, {self});
    const u32 made = call(rt, ctx, 0x08B62344u, {0x10u});
    call_set(rt, ctx, 0x08B37710u, {made});
    const u32 list = self + 0x14u;
    put(rt, made + 0xCu, result);
    put(rt, made + 8u, static_cast<u32>(static_cast<s32>(word(rt, list + 4u) - word(rt, list)) >> 4));
    const s32 entries = static_cast<s32>(word(rt, list + 4u) - word(rt, list)) >> 4;
    if (entries != 0) {
        const u32 records = call(rt, ctx, 0x08B6461Cu, {static_cast<u32>(entries) << 3});
        for (u32 index = 0u; index < static_cast<u32>(entries); ++index) {
            put(rt, records + index * 8u, 0u);
            put8(rt, records + index * 8u + 4u, 0u);
        }
        put(rt, made + 4u, records);
        for (u32 index = 0u; index < static_cast<u32>(entries); ++index) {
            const u32 from = word(rt, list) + index * 0x10u;
            const u32 to = word(rt, made + 4u) + index * 8u;
            put8(rt, to + 4u, byte_u(rt, from + 8u));
            put(rt, to, word(rt, from + 0xCu));
            put8(rt, from + 9u, 0u);
        }
    }
    put(rt, frame.at(0x8u), 0x08CDA8F8u);
    release_slot(rt, ctx, frame.at(0xCu));
    put(rt, frame.at(0x1Cu), 0x08CDA8F8u);
    release_slot(rt, ctx, frame.at(0x20u));
    ctx.gpr[2] = made;
}

} // namespace
} // namespace new_batch_main_1001
} // namespace gt::game

#define MAIN1001(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1001::name, gt::ReturnKind::kind)
MAIN1001(0x08B5C4B0u, func_08B5C4B0, Word);
MAIN1001(0x08B38E74u, func_08B38E74, Word);
MAIN1001(0x08886B00u, func_08886B00, Word);
MAIN1001(0x08B38568u, func_08B38568, Word);
