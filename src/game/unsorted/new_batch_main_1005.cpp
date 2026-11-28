// Main-session batch 1005: functions drafted before but never green (stream helpers, pose rows, record builders and the
// key lookup at 0x08B43B3C). Structured C read from tools.disasm; the differential gate is the judge.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1005 {
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
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// The COW-string release both record builders use: the handle's block sits at -0xC and its count at -4; when the count
// reaches zero the block is freed. The empty-string handle 0x08D74FD0 is skipped (its block is not heap).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 handle, u32 free_argument) {
    if (handle - 0xCu == 0x08D74FD0u) return;
    const u32 count = call(rt, ctx, 0x08B60804u, {handle - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(count) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {handle - 0xCu, free_argument});
}

// 0x08B5FE68 (a0 = stream, a1 = count, a2 = delimiter): the C++ library's ignore(count, delimiter). A delimiter of -1
// is the sibling overload at 0x08B649D4 (its answer is the stream). Otherwise gcount (+4) is cleared, the sentry
// (0x08C55FDC) answers whether to run, and a count of zero or less still answers the stream. The scan keeps gcount and
// skips the streambuf window at +8 / +0xC in chunks of min(count - gcount, available, signed); a chunk under two bytes
// skips one byte itself (0xFF is the end marker) and reads the next through slot 0x28 / 0x24. A delimiter at the current
// byte is consumed (gcount + 1, the window advances or slot 0x28 fills it) and a stream end sets flag 2. A count of
// 0x7FFFFFFF first ignores the limit (gcount = 0x80000000) unless the byte is the delimiter or the end, and puts it back
// to 0x7FFFFFFF when it stops. v0 = the stream on every exit.
void func_08B5FE68(Runtime &rt, AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    const s32 limit = static_cast<s32>(ctx.gpr[5]);
    const u32 delimiter = ctx.gpr[6];
    if (delimiter == 0xFFFFFFFFu) {
        ctx.gpr[2] = call(rt, ctx, 0x08B649D4u, {stream, ctx.gpr[5], delimiter, ctx.gpr[7]});
        return;
    }
    GuestFrame frame(ctx, 0x30u);
    put(rt, stream + 4u, 0u);
    call(rt, ctx, 0x08C55FDCu, {frame.at(0u), stream, 1u});
    if (rt.memory().load8(frame.at(0u)) == 0u || limit <= 0) {
        ctx.gpr[2] = stream;
        return;
    }
    const u32 buffer = word(rt, stream + word(rt, word(rt, stream) - 0xCu) + 0x78u);
    const u32 delimiter_signed = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(delimiter)));
    const auto read_advance = [&]() { return call_indirect(rt, ctx, word(rt, word(rt, buffer) + 0x24u), {buffer}); };
    const auto fill_peek = [&]() { return call_indirect(rt, ctx, word(rt, word(rt, buffer) + 0x28u), {buffer}); };
    const auto window_empty = [&]() { return word(rt, buffer + 8u) >= word(rt, buffer + 0xCu); };
    u32 gcount = word(rt, stream + 4u);
    u32 current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
    u32 beyond = 0u;  // s5: the limit was 0x7FFFFFFF and gcount was widened to 0x80000000
    for (;;) {
        if (static_cast<s32>(gcount) < limit && current != 0xFFFFFFFFu && current != delimiter) {
            const u32 start = word(rt, buffer + 8u);
            const u32 available = word(rt, buffer + 0xCu) - start;
            const u32 wanted = static_cast<u32>(limit) - gcount;
            const u32 chunk = static_cast<s32>(wanted) < static_cast<s32>(available) ? wanted : available;
            if (static_cast<s32>(chunk) < 2) {
                // Fewer than two bytes in the window: count the current byte, skip one and read the next.
                put(rt, stream + 4u, gcount + 1u);
                if (start < word(rt, buffer + 0xCu)) {
                    current = rt.memory().load8(start);
                    put(rt, buffer + 8u, start + 1u);
                    if (current + 1u == 0u) {
                        current = 0xFFFFFFFFu;
                    } else {
                        current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
                    }
                } else {
                    const u32 peeked = fill_peek();
                    if (peeked == 0xFFFFFFFFu) {
                        current = 0xFFFFFFFFu;
                    } else {
                        current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
                    }
                }
                gcount = word(rt, stream + 4u);
                continue;
            }
            const u32 found = call(rt, ctx, 0x08B79998u, {start, delimiter_signed, chunk});
            const u32 taken = found != 0u ? found - start : chunk;
            const u32 moved = word(rt, stream + 4u) + taken;
            const u32 at = start + taken;
            put(rt, buffer + 8u, at);
            put(rt, stream + 4u, moved);
            gcount = moved;
            if (!(at < word(rt, buffer + 0xCu))) {
                current = read_advance();
                gcount = word(rt, stream + 4u);
                continue;
            }
            if (static_cast<s32>(moved) < limit) {
                current = rt.memory().load8(at);
                continue;
            }
        }
        // 08B5FF90: the limit / end / delimiter exit. A widened 0x80000000 count is put back to 0x7FFFFFFF here.
        if (limit == 0x7FFFFFFF && current != 0xFFFFFFFFu && current != delimiter) {
            beyond = 1u;
            put(rt, stream + 4u, 0x80000000u);
            gcount = word(rt, stream + 4u);
            continue;
        }
        if (beyond != 0u) put(rt, stream + 4u, 0x7FFFFFFFu);
        if (current == 0xFFFFFFFFu) {
            const u32 base = stream + word(rt, word(rt, stream) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | 2u});
            ctx.gpr[2] = stream;
            return;
        }
        if (current != delimiter) {
            ctx.gpr[2] = stream;
            return;
        }
        const u32 counted = word(rt, stream + 4u);
        if (counted != 0x7FFFFFFFu) put(rt, stream + 4u, counted + 1u);
        if (window_empty()) {
            fill_peek();
        } else {
            put(rt, buffer + 8u, word(rt, buffer + 8u) + 1u);
        }
        ctx.gpr[2] = stream;
        return;
    }
}

// 0x08B5F7EC (a0 = stream, a1 = string, a2 = delimiter): the C++ library's getline(stream, string, delimiter). The
// sentry (0x08C55FDC) not ok sets flag 4 and answers the stream, otherwise the string is cleared (0x08C74B68) and the
// streambuf window at +8 / +0xC is scanned in chunks of min(0x3FFFFFFC - consumed, available, signed): 0x08B79998
// finds the delimiter and 0x08C75AF8 appends the run before it. A chunk under two bytes appends the current character
// itself (size at data - 0xC, capacity at data - 8, flag at data - 4, 0x08C75840 grows, the terminator comes from
// 0x08CDBE28) and reads the next one. The end marker (-1) ends the line with flag 2, the delimiter consumes one byte and
// ends it, more than 0x3FFFFFFB consumed ends it with flag 4, and a line with nothing consumed ORs 4 in. Slot 0x28 fills
// and peeks, 0x24 reads and advances. v0 = the stream on every exit.
void func_08B5F7EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 delimiter = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[6])));  // seb s7, a2
    const u32 delimiter_byte = delimiter & 0xFFu;                                                    // andi s6, s7, 0xFF
    GuestFrame frame(ctx, 0x50u);
    call(rt, ctx, 0x08C55FDCu, {frame.at(0u), stream, 1u});
    if (rt.memory().load8(frame.at(0u)) == 0u) {
        const u32 base = stream + word(rt, word(rt, stream) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | 4u});
        ctx.gpr[2] = stream;
        return;
    }
    call(rt, ctx, 0x08C74B68u, {out, 0u, word(rt, word(rt, out) - 0xCu), 0u});
    const u32 buffer = word(rt, stream + word(rt, word(rt, stream) - 0xCu) + 0x78u);
    const u32 terminator = rt.memory().load8(0x08CDBE28u);
    const auto read_advance = [&]() { return call_indirect(rt, ctx, word(rt, word(rt, buffer) + 0x24u), {buffer}); };
    const auto fill_peek = [&]() { return call_indirect(rt, ctx, word(rt, word(rt, buffer) + 0x28u), {buffer}); };
    const auto window_empty = [&]() { return word(rt, buffer + 8u) >= word(rt, buffer + 0xCu); };
    u32 current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
    u32 consumed = 0u;  // s5
    u32 finish = 0u;    // v1 at the shared exit 08B5F9A0
    // The delimiter: one byte consumed and counted, the window advanced or refilled through slot 0x28.
    const auto take_delimiter = [&]() {
        ++consumed;
        if (window_empty()) {
            fill_peek();
        } else {
            put(rt, buffer + 8u, word(rt, buffer + 8u) + 1u);
        }
    };
    for (;;) {
        if (current == 0xFFFFFFFFu) {
            finish = 2u;
            break;
        }
        if (delimiter_byte == current) {
            take_delimiter();
            finish = 0u;
            break;
        }
        const u32 start = word(rt, buffer + 8u);
        const u32 available = word(rt, buffer + 0xCu) - start;
        const u32 budget = 0x3FFFFFFCu - consumed;
        const u32 chunk = static_cast<s32>(budget) < static_cast<s32>(available) ? budget : available;
        if (static_cast<s32>(chunk) < 2) {
            // Fewer than two bytes left in the window: append the current character, then pull the next one.
            const u32 data = word(rt, out);
            const u32 size = word(rt, data - 0xCu);
            const u32 capacity = word(rt, data - 8u);
            const u32 grown = size + 1u;
            if (capacity < grown || static_cast<s32>(word(rt, data - 4u)) > 0)
                call(rt, ctx, 0x08C75840u, {out, grown});
            const u32 base = word(rt, out);
            rt.memory().store8(base + word(rt, base - 0xCu), static_cast<std::uint8_t>(current));
            put(rt, base - 4u, 0u);
            put(rt, base - 0xCu, grown);
            rt.memory().store8(base + grown, static_cast<std::uint8_t>(terminator));
            ++consumed;
            if (start < word(rt, buffer + 0xCu)) {
                const u32 byte = rt.memory().load8(start);
                put(rt, buffer + 8u, start + 1u);
                if (byte + 1u == 0u) {
                    current = 0xFFFFFFFFu;
                } else {
                    current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
                }
            } else {
                const u32 peeked = fill_peek();
                if (peeked == 0xFFFFFFFFu) {
                    current = 0xFFFFFFFFu;
                } else {
                    current = window_empty() ? read_advance() : rt.memory().load8(word(rt, buffer + 8u));
                }
            }
        } else {
            // 0x08B79998 takes the sign-extended character and 0x08C75AF8 appends the run before the delimiter.
            const u32 found = call(rt, ctx, 0x08B79998u, {start, delimiter, chunk});
            const u32 taken = found != 0u ? found - start : chunk;
            call(rt, ctx, 0x08C75AF8u, {out, start, taken});
            consumed += taken;
            const u32 at = word(rt, buffer + 8u) + taken;
            put(rt, buffer + 8u, at);
            current = at < word(rt, buffer + 0xCu) ? rt.memory().load8(at) : read_advance();
        }
        if (0x3FFFFFFBu < consumed) {  // out of budget: the end marker, the delimiter or a line too long
            if (current == 0xFFFFFFFFu) {
                finish = 2u;
                break;
            }
            if (delimiter_byte == current) {
                take_delimiter();
                finish = 0u;
                break;
            }
            finish = 4u;
            break;
        }
    }
    if (consumed == 0u) finish |= 4u;
    if (finish != 0u) {
        const u32 base = stream + word(rt, word(rt, stream) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | finish});
    }
    ctx.gpr[2] = stream;
}

// 0x0891E2A8 (a0 = the output, a1 = the owner object, a2 = the 0x48-byte row, f12 = the step): lerps the row's first
// three components (and its 3, 4, 5 and 6, 7 ones) from the row's own +0 block toward its +0x20 block by a factor that
// depends on the row's mode (+0x40): 0 lerps by f12, 1 advances the owner's +0x238 swirl and clamps the +0x234 amount,
// 2 sets the amount from f12 (15 / 130 / 100 / 30) and clamps nothing. v0 = the output.

// 0x0897B800 (a0 = object): rebuilds the circular list at +0x184 from the entries the bit set at +0x74 keeps. Every node
// is copied into a frame record (0x08973DB0), requeued onto the list at +0x18C (0x08B5C094 / 0x08A5B9EC / 0x08A5BC54 /
// 0x08A5BC20 / 0x08B5C078) and then either walked (0x08973C2C and the +0x10C virtual, with 0x0897B6E4 between rounds
// while the last bit is set and 0x08B1A844 reports no interrupt) or dropped. When the walk stops, the +4 list is requeued
// onto the frame list and the outermost round ends when the last bit is clear or an interrupt fires. The frame list's
// entries whose object byte at +0x14 is clear are then pushed back onto +0x184 and the frame list is destroyed.
bool last_bit_set(Runtime &rt, u32 bits) {
    const s32 count = static_cast<s32>(word(rt, bits + 0xCu));
    const u32 words = word(rt, bits + 8u);
    const s32 index = count - 1;
    const u32 bias = static_cast<u32>(index >> 31) >> 27;
    const s32 word_index = (index < 0) ? count + 0x1E : index;
    s32 bit = static_cast<s32>(((static_cast<u32>(index) + bias) & 0x1Fu) - bias);
    u32 address = words + (static_cast<u32>(word_index >> 5) << 2);
    if (bit < 0) {
        bit += 0x20;
        address -= 4u;
    }
    return ((1u << (static_cast<u32>(bit) & 31u)) & word(rt, address)) != 0u;
}


// 0x08A1D07C (a0 = the object, a1 = the mode, a2 = the source words, a3 = the record, t0 = the value): constructs the
// base at a0 + 8 (08A1CDBC), copies the source blocks into a0 + 0x44 / 0x54 / 0x6C / 0x88 / 0x108 / 0x188 and the record's
// words into a0 + 0x58C .. 0x5C4, copies the 16-byte constant at 0x08C8E450 to a0 + 0x5E4 and, in mode 8, registers the
// three stack temporaries at a0 + 0x5D8 .. 0x5E0, runs 08ACB5B4 / 08ACB1BC and stores the measured size at a0 + 0x608
// (answered), otherwise just runs the iteration and answers 08A1CB1C.

// 0x08A1E0BC (a0 = the object, a1 = the output, a2 = the source, a3 = the mode, t0 = the list): builds a formatted
// string out of the list's two entries (their +0xC virtuals fill the frame's +0x10 / +0x14) and, in mode 3, out of the
// 20-byte records the collection at list + 8 hands out (its +0x7C count, each entry's +0x84 producing one), then gives
// the pair and the record array to 0x08A1D398 and moves the answer into the output. v0 = the release at the end.

// 0x08B43B3C (a0 = the holder, a2 = key low, a3 = key high): asks the object at +0xB0 through its vtable's +0x3C for the
// value the whole function answers with (a1..a3 are the caller's), then looks for the first slot of the +0x454 table
// whose presence byte at +0x400 is set, keeps the first node of that bucket list whose range (+0x20 / +0x24, length
// +0x10) holds the key and whose +0x16 tag matches the holder's virtual +0x90, and hands it to 0x08B42A90 (or walks the
// +0x8DC table's entries whose +5 flag is set through 0x08B42618). v0 is the first call's answer on every exit.
void func_08B43B3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 key_argument = ctx.gpr[5];
    const u32 key_low = ctx.gpr[6];
    const u32 key_high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x560u);
    const u32 object = word(rt, holder + 0xB0u);
    const u32 vtable = word(rt, object);
    const u32 answered = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {object, key_argument, key_low, key_high});
    const u32 tag = ctx.gpr[3];  // the callee's v1, as the asm reads it after the return
    const u32 list = holder + 0x454u;
    const u32 count = word(rt, list + 0x480u);
    if (count == 0u) {
        ctx.gpr[2] = answered;
        return;
    }
    u32 result = answered;
    u32 index = 0u;
    u32 cursor = list;
    u32 head_address;
    if (rt.memory().load8(list + 0x400u) != 0u) {
        head_address = list;
    } else {
        for (;;) {
            ++index;
            result = rt.memory().load8(cursor + 0x401u);
            if (index == count) {
                ctx.gpr[2] = result;
                return;
            }
            cursor += 1u;
            if (result != 0u) break;
        }
        head_address = list + index * 4u;
    }
    u32 node = word(rt, head_address);
    result = head_address;
    while (node != 0u) {
        const u32 entry = word(rt, node);
        if (entry != 0u) {
            result = call(rt, ctx, 0x08B3E6E8u, {entry});
            if (result != 0u) {
                result = word(rt, entry + 0x10u);
                if (result != 0u) {
                    // |(key_high:key_low) - (entry[0x24]:entry[0x20])| as the asm's 64-bit two's-complement negate.
                    const u32 low = key_low - word(rt, entry + 0x20u);
                    const u32 borrow = key_low < low ? 1u : 0u;
                    const u32 high = key_high - word(rt, entry + 0x24u) - borrow;
                    const u32 sign = static_cast<u32>(static_cast<s32>(high) >> 31);
                    const u32 magnitude_low_a = low ^ sign;
                    const u32 magnitude_high_a = high ^ sign;
                    const u32 magnitude_low = magnitude_low_a - sign;
                    const u32 carry = magnitude_low_a < magnitude_low ? 1u : 0u;
                    const u32 magnitude_high = (magnitude_high_a - sign) - carry;
                    const bool negative = static_cast<s32>(magnitude_high) < 0;
                    const bool within = magnitude_low < word(rt, entry + 0x10u);
                    if (negative || (magnitude_high == 0u && within)) {
                        result = 1u;  // the skip path leaves the range test's answer in v0
                    } else {
                        result = 0u;  // the body is entered with the comparison's zero
                        const u32 wanted = static_cast<u32>(signed_half(rt, entry + 0x16u));
                        const u32 own = word(rt, holder);
                        result = call_indirect(rt, ctx, word(rt, own + 0x90u), {holder});
                        if (wanted == result) {
                            put(rt, entry + 0x20u, key_low);
                            put(rt, entry + 0x24u, key_high);
                            result = call(rt, ctx, 0x08B3E6BCu, {entry});
                            if (result != 0u) {
                                result = call(rt, ctx, 0x08B3E65Cu, {entry});
                                put(rt, entry + 0x18u, key_low);
                                put(rt, entry + 0x1Cu, key_high);
                                result = call(rt, ctx, 0x08B47AB8u, {frame.at(0u)});
                                put(rt, frame.at(0x14u), 0x4E29u);
                                put(rt, frame.at(8u), answered);
                                put(rt, frame.at(0xCu), tag);
                                result = call(rt, ctx, 0x08B47AD4u,
                                              {frame.at(0u), word(rt, entry + 0xCu) + 0x18u});
                                const u32 built = call(rt, ctx, 0x08B47B40u, {frame.at(0u), 0u});
                                result = built;
                                put(rt, built, 0u);
                                put(rt, built + 8u, 0u);
                                put(rt, built + 0xCu, 0u);
                                put(rt, built + 0x10u, 0u);
                                put(rt, built + 0x14u, 0u);
                                put(rt, built + 4u, 0u);
                                rt.memory().store8(built, 0u);
                                rt.memory().store16(built + 8u, rt.memory().load16(entry));
                                rt.memory().store16(built + 0xAu, rt.memory().load16(entry + 0x16u));
                                put(rt, built + 0xCu, word(rt, entry + 0xCu));
                                put(rt, built + 0x14u, word(rt, entry + 0x1Cu));
                                put(rt, built + 0x10u, word(rt, entry + 0x18u));
                                result = call(rt, ctx, 0x08B79A88u,
                                              {built + 0x18u, word(rt, entry + 0x58u), word(rt, entry + 0xCu)});
                                const u32 marker = rt.memory().load16(frame.at(0u));
                                if (rt.memory().load8(entry + 0x14u) != 0u ||
                                    rt.memory().load8(entry + 0x15u) != 0u) {
                                    result = call(rt, ctx, 0x08B42A90u,
                                                  {holder, 0x4E29u, built, marker, 0u, 1u, 0u, 0u});
                                } else {
                                    result = call(rt, ctx, 0x08B3F3C8u, {holder + 0x8DCu});
                                    if (result != 0u) {
                                        u32 link = result;
                                        for (;;) {
                                            const u32 own2 = word(rt, holder);
                                            const u32 item = word(rt, link);
                                            const u32 item_tag = rt.memory().load16(item);
                                            result = call_indirect(rt, ctx, word(rt, own2 + 0x90u), {holder});
                                            if (item_tag != result) {
                                                result = rt.memory().load8(item + 5u);
                                                if (result != 0u)
                                                    result = call(rt, ctx, 0x08B42618u,
                                                                  {holder, item, frame.at(0u), 1u});
                                            }
                                            link = word(rt, link + 8u);
                                            if (link == 0u) break;
                                        }
                                    }
                                }
                                result = call(rt, ctx, 0x08B47ACCu, {frame.at(0u)});
                            }
                        }
                    }
                }
            }
        }
        node = word(rt, node + 8u);
    }
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_main_1005
} // namespace gt::game

#define NEW1005(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1005::name, gt::ReturnKind::kind)
NEW1005(0x08B43B3Cu, func_08B43B3C, Word);
NEW1005(0x08B5F7ECu, func_08B5F7EC, Word);
NEW1005(0x08B5FE68u, func_08B5FE68, Word);
