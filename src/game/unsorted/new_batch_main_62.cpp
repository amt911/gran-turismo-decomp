// New batch, main session 62: 80 mid-size functions (576-640 bytes) the recompiler emitted, none decompiled before, written by
// hand as structured C from `tools.disasm`. v0 / v1 are left as the original leaves them on every path (a callee that never writes
// v0 hands the previous value on). Stack temporaries the callees fill in are modelled in the GuestFrame at the original offsets.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_62 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    return call_full(rt, ctx, target, arguments);
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }

// A 32-bit load as `lwl`/`lwr` do it: any alignment, little endian.
u32 load_any(Runtime &rt, u32 address) {
    return byte_at(rt, address) | (byte_at(rt, address + 1u) << 8) | (byte_at(rt, address + 2u) << 16) | (byte_at(rt, address + 3u) << 24);
}
void store_any(Runtime &rt, u32 address, u32 value) {
    for (u32 i = 0; i < 4u; ++i) put_byte(rt, address + i, value >> (8u * i));
}
// The compiler's inline copy: 16 bytes per round (four words loaded, then four stored), `bytes` a multiple of 16.
void copy_rounds(Runtime &rt, u32 to, u32 from, u32 bytes) {
    for (u32 done = 0; done < bytes; done += 16u) {
        u32 w[4];
        for (u32 i = 0; i < 4u; ++i) w[i] = load_any(rt, from + done + 4u * i);
        for (u32 i = 0; i < 4u; ++i) store_any(rt, to + done + 4u * i, w[i]);
    }
}

// Drops one reference of the string whose data pointer is at `slot_address`; the last one frees the block (free_argument is the original's second argument).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot_address, u32 free_argument) {
    const u32 data = word(rt, slot_address);
    const u32 header = data - 0xCu;
    if (header == 0x08D74FD0u) return;
    const u32 refs = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (!(static_cast<s32>(refs) > 0)) call(rt, ctx, 0x08B8542Cu, {header, free_argument});
}

// --- 0x088B1F60 (frame 0xC0): a0 = this, a1 = s3, a2 = source block.


// --- 0x088DBC40 (frame 0x40): a0 = this. Two passes over the entries; the first classifies them into a byte array on the frame,
// the second gives each marked entry a slot in the table at this + 0x130. v0 ends as the frame address of the byte past the last entry.
void func_088DBC40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const u32 count_first = call(rt, ctx, 0x088D9F80u, {self});
    const u32 limit = call(rt, ctx, 0x088DAA8Cu, {self});
    if (!(static_cast<s32>(limit) < static_cast<s32>(count_first))) return;
    u32 index = 0u;
    u32 stride = 0u;
    for (;;) {
        const u32 count = call(rt, ctx, 0x088D9F80u, {self});
        if (!(static_cast<s32>(index) < static_cast<s32>(count))) break;
        put_byte(rt, frame.at(index), 0u);
        const u32 kind = call(rt, ctx, 0x088E4754u, {word(rt, self + 0x640u)});
        if (kind == 2u) {
            call(rt, ctx, 0x088DB3CCu, {self, index});
        } else if (kind == 3u) {
            const GuestResult r = call_full(rt, ctx, 0x088E472Cu, {word(rt, self + 0x640u)});
            put(rt, self + 8u * index + 0x58u, r.v0);
            put(rt, self + 8u * index + 0x5Cu, r.v1);
            if (r.v0 != 0xFFFFFFFFu || r.v1 != 0xFFFFFFFFu) {
                const u32 entry = word(rt, self + 0xC0u) + stride;
                const u32 value = call(rt, ctx, 0x088E4744u, {word(rt, self + 0x640u)});
                call(rt, ctx, 0x088E7EBCu, {entry, value});
            }
        } else if (kind == 1u) {
            put_byte(rt, frame.at(index), kind);
        }
        ++index;
        stride += 0x2F8u;
    }
    const u32 slots = call(rt, ctx, 0x088DAA8Cu, {self});
    const GuestResult pair = call_full(rt, ctx, 0x088D9C70u, {self});
    u32 j = 0u;
    stride = 0u;
    for (;;) {
        const u32 count = call(rt, ctx, 0x088D9F80u, {self});
        ctx.gpr[2] = frame.at(j);
        if (!(static_cast<s32>(j) < static_cast<s32>(count))) break;
        if (byte_at(rt, frame.at(j)) != 0u) {
            const u32 found = call(rt, ctx, 0x088E7F14u, {word(rt, self + 0xC0u) + stride});
            u32 s0 = pair.v0;
            u32 s1 = pair.v1;
            u32 slot_arg = found;
            if (found == 0xFFFFFFFFu) {
                s0 = 0xFFFFFFFFu;
                s1 = 0xFFFFFFFFu;
                slot_arg = 0xFFFFFFFFu;
                if (static_cast<s32>(slots) > 0) {
                    const u32 table = word(rt, self + 0x130u);
                    for (u32 k = 0; k != slots; ++k) {
                        if (static_cast<std::int8_t>(byte_at(rt, table + 0x50u + k)) == -1) {
                            put_byte(rt, table + 0x50u + k, j);
                            s0 = pair.v0;
                            s1 = pair.v1;
                            slot_arg = k;
                            break;
                        }
                    }
                }
            }
            call(rt, ctx, 0x088E5234u, {word(rt, self + 0x640u), j, slot_arg, ctx.gpr[7], s0, s1});
            put(rt, self + 8u * j + 0x58u, s0);
            put(rt, self + 8u * j + 0x5Cu, s1);
        }
        ++j;
        stride += 0x2F8u;
    }
    ctx.gpr[2] = frame.at(j);
}


// --- 0x088D4988: a0 / a1 each point at a pointer to a record; a fixed set of fields of *a1 is copied into *a0. v0 = the last source address read (src + 6).
void func_088D4988(Runtime &rt, AllegrexContext &ctx) {
    const u32 to = ctx.gpr[4];
    const u32 from = ctx.gpr[5];
    auto copy8 = [&](u32 offset) { put_byte(rt, word(rt, to) + offset, byte_at(rt, word(rt, from) + offset)); };
    auto copy16 = [&](u32 offset) { put_half(rt, word(rt, to) + offset, half_at(rt, word(rt, from) + offset)); };
    auto copy32 = [&](u32 offset) { put(rt, word(rt, to) + offset, word(rt, word(rt, from) + offset)); };
    copy16(0x4);
    copy16(0x6);
    copy8(0x24);
    copy8(0x25);
    copy16(0x8);
    copy16(0xA);
    copy16(0xC);
    copy16(0xE);
    copy32(0x10);
    copy32(0x14);
    for (const u32 offset : {0x48u, 0x4Au, 0x4Bu, 0x4Eu, 0x50u, 0x55u, 0x4Du}) copy8(offset);
    for (u32 i = 0; i < 4u; ++i) {
        copy16(0x40u + 2u * i);
        copy8(0x58u + i);
    }
    copy8(0x60);
    for (u32 i = 0; i < 4u; ++i) {
        copy8(0x61u + i);
        copy16(0x66u + 2u * i);
    }
    for (const u32 offset : {0x6Eu, 0x6Fu, 0x70u, 0x72u, 0x73u, 0x74u, 0x71u, 0x76u, 0x77u}) copy8(offset);
    for (u32 i = 0; i < 4u; ++i) copy16(0x7Au + 2u * i);
    ctx.gpr[2] = word(rt, from) + 6u;
}


// --- 0x0881B40C (frame 0x10): a0 = destination record, a1 = source record. Header block, a counted table, two 0x190-byte blocks, then 30 chained calls.


// --- 0x08995F54 (frame 0x50): a0 = 1 and a1 = 0xFFFF registers eight tables / callbacks (the last seven through a 8-byte stack pair each);
// a0 = 0 and a1 = 0xFFFF tail-calls the matching teardown. v0 = 1 for any other a0, 0xFFFF for any other a1.


// --- 0x08A6A6F0 / 0x08A6FDF0 (frame 0x40): a0 = the output string (a pointer to its character data, header at -0xC), a1 = an object whose virtual
// getter (slot 0xC) fills a string pointer on the frame. Each character is copied out: 7..13 (the control characters) as a two-byte escape from a
// table, printable ones pushed back one by one, anything else formatted into a 10-byte buffer. v0 = a0.
struct EscapeStrings {
    u32 quote;           // the quote written before and after (and passed to the scan call)
    u32 escapes[7];      // in jump-table order for the characters 7, 8, 9, 10, 11, 12, 13
    u32 hex_format;      // format for any other character
};
void escape_string(Runtime &rt, AllegrexContext &ctx, const EscapeStrings &strings) {
    const u32 out = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    call_virtual(rt, ctx, object, 0xCu, {frame.at(4), object});
    u32 text = word(rt, frame.at(4));
    call(rt, ctx, 0x08B85EB8u, {out, strings.quote, frame.at(1)});
    u32 c = byte_at(rt, text);
    while (c != 0u) {
        const s32 ch = static_cast<s32>(signed_byte(c));
        const u32 control = static_cast<u32>(ch) - 7u;
        if (control < 7u) {
            call(rt, ctx, 0x08BEED1Cu, {out, strings.escapes[control], 2u});
        } else if (static_cast<u32>(ch) - 0x20u < 0x5Fu) {
            const u32 data = word(rt, out);
            const u32 grown = word(rt, data - 0xCu) + 1u;
            if (word(rt, data - 8u) < grown || static_cast<s32>(word(rt, data - 4u)) > 0) call(rt, ctx, 0x08B864ECu, {out, grown});
            const u32 current = word(rt, out);
            put_byte(rt, current + word(rt, current - 0xCu), static_cast<u32>(ch));
            const u32 base = word(rt, out);
            put(rt, base - 4u, 0u);
            put(rt, base - 0xCu, grown);
            put_byte(rt, base + grown, 0u);
        } else {
            call(rt, ctx, 0x08B3767Cu, {frame.at(8), 10u, strings.hex_format, static_cast<u32>(ch)});
            const u32 length = call(rt, ctx, 0x08B35F60u, {frame.at(8)});
            call(rt, ctx, 0x08BEED1Cu, {out, frame.at(8), length});
        }
        ++text;
        c = byte_at(rt, text);
    }
    call(rt, ctx, 0x08BEED1Cu, {out, strings.quote, 1u});
    const u32 data = word(rt, frame.at(4));
    const u32 header = data - 0xCu;
    ctx.gpr[2] = out;
    if (header != 0x08D74FD0u) {
        const u32 refs = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        if (!(static_cast<s32>(refs) > 0)) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0)});
        ctx.gpr[2] = out;
    }
}
void func_08A6FDF0(Runtime &rt, AllegrexContext &ctx) {
    escape_string(rt, ctx, {0x08CB08BCu, {0x08CB08C0u, 0x08CB08C4u, 0x08CB08D4u, 0x08CB08CCu, 0x08CB08D8u, 0x08CB08C8u, 0x08CB08D0u}, 0x08CB08DCu});
}


// --- 0x0884B224 (frame 0x60): a0 = a string-like object (kept at sp + 0x20). Opens a scope, looks up an object, reads six values from it (three
// pairs of getters) and assigns each, through a temporary on the frame, to the six slots of the record at +0x14 of a new object built at sp + 0x1C.


// --- 0x08A13BE0 (frame 0xA0): a0 = a worker object. A loop that runs until the byte at +0x41 is set: each round advances the state machine
// 0x08A1369C / 0x08A13604 / 0x08A13654, rebuilds the record at +0x178 and, when the queue at +0x240 reports a request, services it. v0 = the byte at +0x41.


// --- 0x08A3449C (frame 0x40): a1 = key, a2 = mode 1 / 2 / 3, a3 = a record with up to three members (+4, +8 for the later modes). Each mode opens
// a scope for the key, null-checks what it produced (assert 0x08CACE80), and hands it on: mode 1 to 0x08A34244 with a3; mode 2 the same, then a second scope
// whose object gets the +4 member's virtual (slot 0x3C) answer; mode 3 the same plus a third scope for the +8 member. Any other mode returns v0 = 3.
void func_08A3449C(Runtime &rt, AllegrexContext &ctx) {
    const u32 key = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 record = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    // opens a scope on the frame slot, returns the object it holds (asserting when null)
    auto open_scope = [&](u32 slot) {
        call(rt, ctx, 0x08A343C4u, {frame.at(slot), key});
        const u32 object = word(rt, frame.at(slot));
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACE80u});
        return object;
    };
    auto member_answer = [&](u32 offset) {
        const u32 member = word(rt, record + offset);
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACE20u});
        return call_virtual(rt, ctx, member, 0x3Cu, {member}).v0;
    };
    if (mode == 1u) {
        const u32 object = open_scope(0x14u);
        call(rt, ctx, 0x08A34244u, {object, record});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    } else if (mode == 2u || mode == 3u) {
        const u32 first_slot = mode == 2u ? 0x10u : 0x08u;
        const u32 second_slot = mode == 2u ? 0x0Cu : 0x04u;
        const u32 first = open_scope(first_slot);
        call(rt, ctx, 0x08A34244u, {first, record});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(first_slot)});
        const u32 second = open_scope(second_slot);
        const u32 answer = member_answer(4u);
        call(rt, ctx, 0x08A340CCu, {second, answer});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(second_slot)});
        if (mode == 3u) {
            const u32 third = open_scope(0x00u);
            const u32 third_answer = member_answer(8u);
            call(rt, ctx, 0x08A340D4u, {third, third_answer});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x00u)});
        }
    } else {
        ctx.gpr[2] = 3u;
    }
}


// --- 0x0888BD14 (frame 0xE0): a0 = this, a1 = key, a2 = array of u16 ids, a3 = count, t0 / t1 = flags (low byte). Three passes over the ids: toggle the
// objects found for them (only when 0x088D4C50 says no), then register those the key matches, then post a message record for each.
void func_0888BD14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 ids = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    const u32 flag_a = ctx.gpr[8] & 0xFFu;
    const u32 flag_b = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0xE0u);
    const u32 found = call(rt, ctx, 0x088D4C50u, {word(rt, self + 4u) + 4u});
    if (found == 0u) {
        if (!(static_cast<s32>(count) > 0)) return;
        for (u32 i = 0; i != count; ++i) {
            const u32 object = call(rt, ctx, 0x088DA588u, {word(rt, self + 4u), half_at(rt, ids + 2u * i)});
            if (object == 0u) continue;
            if (flag_b != 0u) {
                call_virtual(rt, ctx, object, 0x1Cu, {object, 1u});
                call_virtual(rt, ctx, object, 0x18u, {object, 1u, 0x50u});
            } else {
                call_virtual(rt, ctx, object, 0x1Cu, {object, 1u});
                call_virtual(rt, ctx, object, 0x14u, {object, 0u});
            }
        }
    } else {
        ctx.gpr[2] = flag_a ^ 1u;
        if (!(static_cast<s32>(count) > 0)) return;
    }
    const u32 posted_flag = flag_a ^ 1u;
    ctx.gpr[2] = posted_flag;
    u32 matched = 0u;
    u32 visited = 0u;
    u32 cursor = ids;
    do {
        const u32 id = half_at(rt, cursor);
        const u32 slot = call(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        call(rt, ctx, 0x088D0918u, {slot});
        const u32 kind = call(rt, ctx, 0x088D6204u, {key});
        if (kind == 0u) {
            if (call(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id}) != 0u) {
                call(rt, ctx, 0x08917AB0u, {word(rt, self + 8u)});
                matched = 1u;
                const u32 value = call(rt, ctx, 0x088D5938u, {key});
                call(rt, ctx, 0x088D6FD4u, {word(rt, self + 4u), value, posted_flag & 0xFFu, 0x22u});
            }
        } else if (kind == 1u) {
            call(rt, ctx, 0x08917AB0u, {word(rt, self + 8u)});
            matched = 1u;
            const u32 value = call(rt, ctx, 0x088D5938u, {key});
            call(rt, ctx, 0x088D6FD4u, {word(rt, self + 4u), value, posted_flag & 0xFFu, 0x22u});
        }
        ++visited;
        cursor += 2u;
    } while (visited != count && matched == 0u);
    cursor = ids;
    for (u32 i = 0; i != count; ++i) {
        const u32 id = half_at(rt, cursor);
        put(rt, frame.at(0x0), 0x25u);
        put(rt, frame.at(0x8), id);
        cursor += 2u;
        put(rt, frame.at(0xC), 0x22u);
        put_byte(rt, frame.at(0x10), flag_a);
        const u32 result = call(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id});
        put_byte(rt, frame.at(0x11), result);
        call(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0)});
    }
}


// --- 0x08A30F24 (frame 0x70): a0 = a scope object (compared with the frame slot 0x2C). Looks a name up in a table (0x08B31D40); the three members of the
// record at +0x14 of the object built at sp + 0x2C get a flag/string pair (found) or empty values (not found), each assigned through a frame temporary.
void func_08A30F24(Runtime &rt, AllegrexContext &ctx) {
    const u32 scope = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    put_byte(rt, frame.at(0x4), 0u);
    put(rt, frame.at(0x28), 0u);
    const u32 found = call(rt, ctx, 0x08B31D40u, {0x08D69170u, frame.at(4), frame.at(0x30), 0x20u, frame.at(0x28)});
    call(rt, ctx, 0x08A98C1Cu, {frame.at(0x2C), 3u});
    // assignment of a member through a frame temporary
    auto assign = [&](u32 member, u32 slot, auto &&fill) {
        fill();
        if (member != frame.at(slot)) call(rt, ctx, 0x08A5BAD8u, {member, word(rt, frame.at(slot))});
    };
    // drop one reference of the string whose data pointer is on the frame
    auto release_string = [&](u32 slot, u32 free_argument) {
        const u32 data = word(rt, frame.at(slot));
        const u32 header = data - 0xCu;
        if (header != 0x08D74FD0u) {
            const u32 refs = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
            if (!(static_cast<s32>(refs) > 0)) call(rt, ctx, 0x08B8542Cu, {header, free_argument});
        }
    };
    u32 last_slot;
    if (found != 0u) {
        const u32 flag = byte_at(rt, frame.at(4));
        const u32 first_member = word(rt, word(rt, frame.at(0x2C)) + 0x14u);
        assign(first_member, 0x24u, [&] { call(rt, ctx, 0x08A97A6Cu, {frame.at(0x24), flag}); });
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x24)});
        const u32 record = word(rt, frame.at(0x2C));
        const u32 flag_again = byte_at(rt, frame.at(4));
        const u32 second_member = word(rt, record + 0x14u) + 4u;
        const u32 text = flag_again != 0u ? frame.at(0x30) : 0x08C8E67Cu;
        assign(second_member, 0x20u, [&] {
            call(rt, ctx, 0x08B85EB8u, {frame.at(0x1C), text, frame.at(3)});
            call(rt, ctx, 0x08A6D2D4u, {frame.at(0x20), frame.at(0x1C)});
        });
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x20)});
        release_string(0x1Cu, frame.at(1));
        const u32 flag_last = byte_at(rt, frame.at(4));
        const u32 last_value = word(rt, frame.at(0x28));
        const u32 third_member = word(rt, word(rt, frame.at(0x2C)) + 0x14u) + 8u;
        assign(third_member, 0x18u, [&] { call(rt, ctx, 0x08A85B1Cu, {frame.at(0x18), flag_last == 0u ? 0u : last_value}); });
        last_slot = 0x18u;
    } else {
        const u32 first_member = word(rt, word(rt, frame.at(0x2C)) + 0x14u);
        assign(first_member, 0x14u, [&] { call(rt, ctx, 0x08A97A6Cu, {frame.at(0x14), 0u}); });
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14)});
        const u32 second_member = word(rt, word(rt, frame.at(0x2C)) + 0x14u) + 4u;
        assign(second_member, 0x10u, [&] {
            call(rt, ctx, 0x08B85EB8u, {frame.at(0xC), 0x08C8E67Cu, frame.at(2)});
            call(rt, ctx, 0x08A6D2D4u, {frame.at(0x10), frame.at(0xC)});
        });
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10)});
        release_string(0xCu, frame.at(0));
        const u32 third_member = word(rt, word(rt, frame.at(0x2C)) + 0x14u) + 8u;
        assign(third_member, 0x08u, [&] { call(rt, ctx, 0x08A85B1Cu, {frame.at(0x08), 0u}); });
        last_slot = 0x08u;
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(last_slot)});
    if (scope != frame.at(0x2C)) call(rt, ctx, 0x08A5BAD8u, {scope, word(rt, frame.at(0x2C))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x2C)});
}


// --- 0x08A43958 (frame 0x40): a0 = this, a1 / a2 = holders of two objects (*a1, *a2). Builds the two virtual-getter strings (slot 0xC) and hands them to
// 0x08A42F54 / 0x08A436A0; when *a2 is null the second string is the empty one 0x08CF6ED8, when 0x08A76F20 accepts *a2 the first holder's string is combined with *a2 instead.


// --- 0x08BFA3E0 (frame 0x40): a merge step over 8-byte elements (first a0, middle a1, last a2, lengths a3 / t0, scratch buffer t1 with room for t2 elements, t3 a pass-through).
// When one half fits the buffer it is copied there and the forward / backward merge (0x08BF9DBC / 0x08BF9C34) takes over as a tail call; otherwise the
// ranges are cut around their midpoints, the middle part rotated (0x08BFA134) and both sides merged by this function again (once as a call, once as a tail call).
void func_08BFA3E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 middle = ctx.gpr[5];
    const u32 last = ctx.gpr[6];
    const u32 len1 = ctx.gpr[7];
    const u32 len2 = ctx.gpr[8];
    const u32 buffer = ctx.gpr[9];
    const u32 buffer_size = ctx.gpr[10];
    const u32 tag = ctx.gpr[11];
    const bool len2_less = static_cast<s32>(len2) < static_cast<s32>(len1);
    const bool buffer_below_len2 = static_cast<s32>(buffer_size) < static_cast<s32>(len2);
    auto copy_elements = [&](u32 to, u32 from, s32 count) {
        for (s32 i = 0; i < count; ++i) {
            put(rt, to + 8u * static_cast<u32>(i), word(rt, from + 8u * static_cast<u32>(i)));
            put(rt, to + 8u * static_cast<u32>(i) + 4u, word(rt, from + 8u * static_cast<u32>(i) + 4u));
        }
    };
    if (!len2_less && !(static_cast<s32>(buffer_size) < static_cast<s32>(len1))) {
        const s32 count = static_cast<s32>(middle - first) >> 3;
        copy_elements(buffer, first, count);
        tail_call(ctx, 0x08BF9DBCu, {buffer, buffer + 8u * static_cast<u32>(count > 0 ? count : 0), middle, last, first, buffer, buffer_size, tag});
        return;
    }
    if (!buffer_below_len2) {
        const s32 count = static_cast<s32>(last - middle) >> 3;
        copy_elements(buffer, middle, count);
        tail_call(ctx, 0x08BF9C34u, {first, middle, buffer, buffer + 8u * static_cast<u32>(count > 0 ? count : 0), last, buffer, buffer_size, tag});
        return;
    }
    u32 new_middle;
    u32 first_cut;
    u32 second_cut;
    u32 rest1;
    u32 rest2;
    {
        GuestFrame frame(ctx, 0x40u);
        put(rt, frame.at(4), last);
        u32 len11;
        u32 len22;
        if (len2_less) {
            len11 = static_cast<u32>(static_cast<s32>((len1 >> 31) + len1) >> 1);
            first_cut = first + (len11 << 3);
            second_cut = call(rt, ctx, 0x08BFA370u, {middle, last, first_cut});
            len22 = static_cast<u32>(static_cast<s32>(second_cut - middle) >> 3);
        } else {
            len22 = static_cast<u32>(static_cast<s32>((len2 >> 31) + len2) >> 1);
            second_cut = middle + (len22 << 3);
            first_cut = call(rt, ctx, 0x08BFA300u, {first, middle, second_cut});
            len11 = static_cast<u32>(static_cast<s32>(first_cut - first) >> 3);
        }
        put(rt, frame.at(0), second_cut);
        rest1 = len1 - len11;
        new_middle = call(rt, ctx, 0x08BFA134u, {first_cut, middle, second_cut, rest1, len22, buffer, buffer_size});
        call(rt, ctx, 0x08BFA3E0u, {first, first_cut, new_middle, len11, len22, buffer, buffer_size, new_middle});
        rest2 = len2 - len22;
    }
    tail_call(ctx, 0x08BFA3E0u, {new_middle, second_cut, last, rest1, rest2, buffer, buffer_size, new_middle});
}


// --- 0x08B931BC (frame 0x50): a vector of 8-byte elements (start +0, finish +4, end of storage +8) fill-insert: a0 = the vector, a1 = the position,
// a2 = how many, a3 = pointer to the value. In place when the spare room is enough (shifting the tail up, or filling past the old end), else a new
// block is allocated (0x08B35DA0), the parts copied across and the old block freed. v0 = the new end of storage after a reallocation, the last fill
// call's v0 in place, untouched for a zero count.
void func_08B931BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x20), count);
    if (count == 0u) return;
    const u32 finish = word(rt, vector + 4u);
    if (!(((word(rt, vector + 8u) - finish) >> 3) < count)) {
        const u32 value_low = word(rt, value);
        const u32 value_high = word(rt, value + 4u);
        const u32 after = static_cast<u32>(static_cast<s32>(finish - position) >> 3);
        put(rt, frame.at(0xC), value_low);
        put(rt, frame.at(0x10), value_high);
        if (count < after) {
            call(rt, ctx, 0x08B8E770u, {finish - (count << 3), finish, finish, frame.at(6)});
            const u32 tail_start = finish - (count << 3);
            put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
            const s32 moved = static_cast<s32>(tail_start - position) >> 3;
            u32 from = tail_start;
            u32 to = finish;
            for (s32 i = 0; i < moved; ++i) {
                from -= 8u;
                to -= 8u;
                put(rt, to, word(rt, from));
                put(rt, to + 4u, word(rt, from + 4u));
            }
            call(rt, ctx, 0x08B92974u, {position, position + (count << 3), frame.at(0xC)});
        } else {
            call(rt, ctx, 0x08B92BF8u, {finish, count - after, frame.at(0xC), frame.at(5)});
            const u32 new_finish = word(rt, vector + 4u) + ((count - after) << 3);
            put(rt, vector + 4u, new_finish);
            call(rt, ctx, 0x08B92BC0u, {position, finish, new_finish, frame.at(4)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
            call(rt, ctx, 0x08B92974u, {position, finish, frame.at(0xC)});
        }
        return;
    }
    const u32 start = word(rt, vector);
    const u32 size = static_cast<u32>(static_cast<s32>(finish - start) >> 3);
    put(rt, frame.at(8), size);
    if (0x1FFFFFFFu - size < count) call(rt, ctx, 0x08B5ADDCu, {0x08C9F600u});
    const u32 grow = word(rt, size < count ? frame.at(0x20) : frame.at(8));
    const u32 length = size + grow;
    u32 bytes = 0u;
    u32 block = 0u;
    u32 old_start = start;
    if (length < size) {
        bytes = 0xFFFFFFF8u;
    } else if (length != 0u) {
        bytes = length << 3;
    }
    if (length < size || length != 0u) {
        block = call(rt, ctx, 0x08B35DA0u, {bytes});
        old_start = word(rt, vector);
    }
    const u32 copied = call(rt, ctx, 0x08B92B58u, {old_start, position, block, frame.at(3)});
    call(rt, ctx, 0x08B92B90u, {copied, word(rt, frame.at(0x20)), value, frame.at(2)});
    const u32 inserted_end = (word(rt, frame.at(0x20)) << 3) + copied;
    const u32 new_finish = call(rt, ctx, 0x08B92B58u, {position, word(rt, vector + 4u), inserted_end, frame.at(1)});
    call(rt, ctx, 0x08B8E584u, {word(rt, vector), word(rt, vector + 4u), frame.at(0)});
    ctx.gpr[2] = block + bytes;
    if (word(rt, vector) != 0u) {
        call(rt, ctx, 0x08B35254u, {word(rt, vector)});
        ctx.gpr[2] = block + bytes;
    }
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_finish);
    put(rt, vector, block);
}

// FUNCS-END
} // namespace
} // namespace new_batch_main_62
} // namespace gt::game

#define NEW62(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_62::name, gt::ReturnKind::kind)
NEW62(0x088DBC40u, func_088DBC40, Word);
NEW62(0x088D4988u, func_088D4988, Word);
NEW62(0x08A6FDF0u, func_08A6FDF0, Word);
NEW62(0x08A3449Cu, func_08A3449C, Word);
NEW62(0x0888BD14u, func_0888BD14, Word);
NEW62(0x08A30F24u, func_08A30F24, Word);
NEW62(0x08BFA3E0u, func_08BFA3E0, Word);
NEW62(0x08B931BCu, func_08B931BC, Word);
// REGS-END
