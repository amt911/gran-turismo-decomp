// Hot batch 260: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (string scans, list walks and inserts, a bit set/clear, a jump table on a code, a display-list word, a
// wide-char store and calls through saved registers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_260 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// 0x08A66810: allocates eight bytes (0x08A5BC14), hands the block to 0x08A667EC and returns it.
void allocate_and_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 block = call_set(rt, ctx, 0x08A5BC14u, {8u});
    call_set(rt, ctx, 0x08A667ECu, {block});
    ctx.gpr[2] = block;
}

// 0x08B907C4: clears the eleven words at a0 .. a0 + 0x28 (v0 = a0 + 0x20).
void clear_eleven_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    ctx.gpr[2] = base + 0x20u;
    for (u32 offset = 0u; offset <= 0x28u; offset += 4u) put(rt, base + offset, 0u);
}

// 0x0892FFA0: asks 0x089363FC (no arguments) for an object and hands it with a0 / a1 as passed in to 0x089368DC.
void forward_with_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call_set(rt, ctx, 0x089363FCu, {});
    call_set(rt, ctx, 0x089368DCu, {object, second, first, second});
}

// 0x08AC44A0: the first NUL in [a0, a1): returns the pointer past it, or 0 when the range ends first.
void find_terminator_bounded(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 cursor = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    for (;;) {
        if (!(cursor < end)) {
            cursor = 0u;
            break;
        }
        const u32 character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(cursor))));
        ctx.gpr[2] = character;
        ++cursor;
        if (character == 0u) break;
    }
    ctx.gpr[4] = cursor;
    ctx.gpr[2] = cursor;
}

// 0x08AD16C0: clears the field at +0x1C0, runs 0x08AD1528 on the object and tail-jumps to 0x08AD1660 with it in a0.
void clear_and_chain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0xFFFFFFFFu;
        put(rt, object + 0x1C0u, 0u);
        call_set(rt, ctx, 0x08AD1528u, {object});
    }
    ctx.gpr[4] = object;
    jump(ctx, 0x08AD1660u);
}

// 0x08B18B58: the first '.' or '[' of the string at a0 (null stays null), or 0 at the end of the string.
void find_dot_or_bracket(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 cursor = ctx.gpr[4];
    if (cursor != 0u) {
        u32 character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(cursor))));
        for (;;) {
            ctx.gpr[3] = character;
            if (character == 0u) {
                cursor = 0u;
                break;
            }
            ctx.gpr[2] = 0x2Eu;
            if (character == 0x2Eu) break;
            ctx.gpr[2] = 0x5Bu;
            if (character == 0x5Bu) break;
            ++cursor;
            character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(cursor))));
        }
    }
    ctx.gpr[4] = cursor;
    ctx.gpr[2] = cursor;
}

// 0x08AD9CD4: a jump-table scale (table at 0x08C90AD8, 18 entries): codes whose entry is 0x08AD9D00 scale a1 by 16, 0x08AD9D08 by 4,
// 0x08AD9D10 return a3, any other entry (and any code of 0x12 or more) scale by 2.
void scale_by_code(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 code = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    ctx.gpr[3] = 0x08C90000u;
    if (!(code < 0x12u)) {
        ctx.gpr[2] = value << 1;
        return;
    }
    ctx.gpr[3] = 0x08C90AD8u;
    const u32 entry = word(rt, code * 4u + 0x08C90AD8u);
    ctx.gpr[4] = entry;
    switch (entry) {
    case 0x08AD9D00u: ctx.gpr[2] = value << 4; break;
    case 0x08AD9D08u: ctx.gpr[2] = value << 2; break;
    case 0x08AD9D10u: ctx.gpr[2] = ctx.gpr[7]; break;
    default: ctx.gpr[2] = value << 1; break;
    }
}

// 0x08B54940: inserts the node a1 into the list headed at a0 + 0x14 in key order (key at +8; the cursor holds the link to patch): the node
// is linked in, its own links (+0xC, +0x10, +0x14, +0) are cleared and a back pointer to the link goes to +0x18. v0 is the last key
// comparison (untouched when the list is empty).
void insert_sorted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 head = ctx.gpr[4];
    const u32 node = ctx.gpr[5];
    u32 link = head + 0x14u;
    for (;;) {
        const u32 next = word(rt, link);
        ctx.gpr[6] = next;
        ctx.gpr[7] = next + 0x10u;
        if (next == 0u) break;
        const bool before = word(rt, node + 8u) < word(rt, next + 8u);
        ctx.gpr[2] = before ? 1u : 0u;
        link = before ? next + 0x14u : next + 0x10u;
    }
    ctx.gpr[4] = link;
    put(rt, link, node);
    put(rt, node + 0x18u, link);
    put(rt, node + 0xCu, 0u);
    put(rt, node + 0x10u, 0u);
    put(rt, node + 0x14u, 0u);
    put(rt, node, 0u);
}

u32 count_leading_zeros(u32 value) { return value == 0u ? 32u : static_cast<u32>(__builtin_clz(value)); }

// 0x08B55924: packs two bit widths (32 - clz(a2 - 1) in bits 8-15, 32 - clz(a1 - 1) in bits 0-7, over a0 + 0xB8 in the top byte) into
// the word at the cursor kept at 0x08D6CE5C and advances the cursor by 4. v0 = the width from a1.
void pack_bit_widths(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first_count = ctx.gpr[5];
    const u32 second_count = ctx.gpr[6];
    constexpr u32 kTable = 0x08D6CE00u;
    const u32 cursor = word(rt, kTable + 0x5Cu);
    const u32 second_width = 0x20u - count_leading_zeros(second_count - 1u);
    const u32 first_width = 0x20u - count_leading_zeros(first_count - 1u);
    u32 packed = (object + 0xB8u) << 24;
    packed = (packed & ~0xFF00u) | ((second_width & 0xFFu) << 8);
    packed = (packed & ~0xFFu) | (first_width & 0xFFu);
    ctx.gpr[2] = first_width;
    ctx.gpr[3] = cursor + 4u;
    ctx.gpr[4] = packed;
    put(rt, cursor, packed);
    put(rt, kTable + 0x5Cu, cursor + 4u);
}

// 0x08B21758: sets or clears (a2 & 0xFF == 0) bit a1 of the bitmap at a0 + 5, when 0 <= a1 < the byte at a0 + 4 (signed compare). v0 = the byte
// written; 0 when the bit is beyond the limit; untouched for a negative bit.
void set_bitmap_bit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 bitmap = ctx.gpr[4];
    const s32 bit = static_cast<s32>(ctx.gpr[5]);
    const u32 on = ctx.gpr[6] & 0xFFu;
    if (bit < 0) return;
    const u32 address = bitmap + static_cast<u32>(bit >> 3);
    const u32 limit = rt.memory().load8(bitmap + 4u);
    const u32 mask = 1u << (static_cast<u32>(bit) & 7u);
    ctx.gpr[2] = bit < static_cast<s32>(limit) ? 1u : 0u;
    if (!(bit < static_cast<s32>(limit))) return;
    const u32 set = mask | rt.memory().load8(address + 5u);
    const u32 written = on == 0u ? set - mask : set;
    ctx.gpr[2] = written;
    rt.memory().store8(address + 5u, static_cast<std::uint8_t>(written));
}

// 0x08B53210: an index computation over unsigned a0 (x), a1 (p), a2 (q), a3 (limit) and t0 (cap): 0 for x == 0, otherwise x plus
// p or q picked by comparing x with the limit (and the cap when it is non-zero).
void pick_offset(psprecomp::AllegrexContext &ctx) {
    const u32 x = ctx.gpr[4];
    u32 p = ctx.gpr[5];
    u32 q = ctx.gpr[6];
    const u32 limit = ctx.gpr[7];
    const u32 cap = ctx.gpr[8];
    ctx.gpr[2] = 0u;
    if (x == 0u) return;
    if (cap != 0u) {
        if (limit < cap) {
            if (!(x < cap)) p = q;
            ctx.gpr[2] = p + x;
            return;
        }
        if (!(x < limit)) q = p;
        ctx.gpr[2] = q + x;
        return;
    }
    if (q == 0u) {
        ctx.gpr[2] = x + p;
        return;
    }
    if (!(x < limit)) q = p;
    ctx.gpr[2] = q + x;
}

// 0x08B54810: takes a node from the free list at +0x18 of the pool at a0, or carves the next 28-byte slot (index at +0x10, below 0x200,
// signed) at pool + index * 28 + 0x1C; 0 when the pool is full.
void pool_allocate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 pool = ctx.gpr[4];
    u32 node = word(rt, pool + 0x18u);
    if (node != 0u) {
        put(rt, pool + 0x18u, word(rt, node + 0x14u));
        ctx.gpr[2] = node;
        return;
    }
    const u32 index = word(rt, pool + 0x10u);
    if (static_cast<s32>(index) < 0x200) {
        put(rt, pool + 0x10u, index + 1u);
        node = index * 28u + pool + 0x1Cu;
    }
    ctx.gpr[2] = node;
}

// 0x08AD8BD0: 1 when the string at a0 contains '?' or '*', else 0.
void has_wildcard(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 cursor = ctx.gpr[4];
    u32 character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(cursor))));
    if (character == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    u32 found = 0u;
    for (;;) {
        ++cursor;
        found = 1u;
        if (character == 0x3Fu || character == 0x2Au) break;
        character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(cursor))));
        if (character == 0u) {
            found = 0u;
            break;
        }
    }
    ctx.gpr[4] = cursor;
    ctx.gpr[3] = found;
    ctx.gpr[2] = found;
}

// 0x08AE35BC: walks the chain from the node at +0x18 of a0 (links at +4; a node whose next link's word at +8 is negative ends the walk, the
// word being read even from address 8 for a null link) and returns 1 when a node's word at +8 equals a1, else 0.
void chain_contains(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 wanted = ctx.gpr[5];
    u32 node = word(rt, ctx.gpr[4] + 0x18u);
    if (static_cast<s32>(word(rt, node + 8u)) < 0) node = 0u;
    while (node != 0u) {
        ctx.gpr[3] = 1u;
        if (word(rt, node + 8u) == wanted) {
            ctx.gpr[2] = 1u;
            return;
        }
        u32 next = word(rt, node + 4u);
        ctx.gpr[2] = static_cast<s32>(word(rt, next + 8u)) < 0 ? 1u : 0u;
        if (ctx.gpr[2] != 0u) next = 0u;
        node = next;
    }
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}

// 0x08B80948: stores the first byte of the string at a2 as a wide char at a1 (a stack slot when a1 is null) when a3 is non-zero; returns
// 1, 0 for an empty string (also for a null a2), and -2 when a3 is zero.
void store_wide_char(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 convert = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const u32 slot = destination != 0u ? destination : frame.at(0u);
    ctx.gpr[2] = slot;
    if (source == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    u32 result = 0xFFFFFFFEu;
    if (convert != 0u) {
        const u32 character = rt.memory().load8(source);
        ctx.gpr[3] = character;
        rt.memory().store16(slot, static_cast<std::uint16_t>(character));
        ctx.gpr[2] = rt.memory().load8(source);
        result = 1u;
        if (ctx.gpr[2] == 0u) result = 0u;
    }
    ctx.gpr[4] = result;
    ctx.gpr[2] = result;
}

// 0x08932598: asks 0x089363FC for an object, runs 0x08936584 over it with the pair at a0 + 0x40 and a stack record, and returns whether the
// record's byte at +0x23, minus one, is below 2 (unsigned byte).
void check_record_kind(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const u32 object = call_set(rt, ctx, 0x089363FCu, {});
    call_set(rt, ctx, 0x08936584u, {object, ctx.gpr[5], word(rt, self + 0x40u), word(rt, self + 0x44u), frame.at(0u)});
    const u32 kind = (rt.memory().load8(frame.at(0x23u)) - 1u) & 0xFFu;
    ctx.gpr[2] = kind < 2u ? 1u : 0u;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08A66810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allocate_and_init(rt, ctx); }
void func_08B907C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_eleven_words(rt, ctx); }
void func_0892FFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_with_object(rt, ctx); }
void func_08AC44A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_terminator_bounded(rt, ctx); }
void func_08AD16C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_and_chain(rt, ctx); }
void func_08B18B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_dot_or_bracket(rt, ctx); }
void func_08B1FE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = word(rt, G[4] + 4u); if (G[2] != 0u) { G[3] = 0u; if (G[5] != 0u) { for (;;) { G[2] = word(rt, G[2] + 8u); G[3] = G[3] + 1u; if (G[2] == 0u) break; if (G[5] == G[3]) break; } } } } }
void func_08B2E064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[31]); call_here(rt, ctx, 0x08C7A9E8u); } G[3] = 0x80020064u; G[4] = 0x7D0u; if (G[2] == G[3]) { jump(ctx, 0x08B32A34u); return; } jump(ctx, 0x08C7A988u); } }
void func_08AD9CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_by_code(rt, ctx); }
void func_08AF8624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = load8(G[4] + 7u); G[3] = G[4]; G[4] = G[5]; G[6] = G[2]; if (G[2] == 0u) return; G[8] = word(rt, G[3]); G[5] = word(rt, G[3] + 0x28u); G[7] = 0xFFFFFFFFu; G[8] = G[8] & 0xFFFu; G[2] = G[8] & 0x80u; G[8] = (G[8] >> 8) & 1u; if (G[2] != 0u) G[7] = load8(G[3] + 6u); jump(ctx, 0x08AFCC0Cu); } }
void func_08B54940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_sorted(rt, ctx); }
void func_08B55924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pack_bit_widths(rt, ctx); }
void func_08B21758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_bitmap_bit(rt, ctx); }
void func_08B53210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; pick_offset(ctx); }
void func_08B54810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pool_allocate(rt, ctx); }
void func_08AD8BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { has_wildcard(rt, ctx); }
void func_08AE35BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { chain_contains(rt, ctx); }
void func_08B80948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_wide_char(rt, ctx); }
void func_08932598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { check_record_kind(rt, ctx); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_260
} // namespace gt::game

#define HOT260(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_260::name, gt::ReturnKind::kind)
HOT260(0x08A66810u, func_08A66810, Word);
HOT260(0x08B907C4u, func_08B907C4, Word);
HOT260(0x0892FFA0u, func_0892FFA0, Word);
HOT260(0x08AC44A0u, func_08AC44A0, Word);
HOT260(0x08AD16C0u, func_08AD16C0, Word);
HOT260(0x08B18B58u, func_08B18B58, Word);
HOT260(0x08B1FE38u, func_08B1FE38, Word);
// HOT260(0x08B2E064u, func_08B2E064, Word);  // no comparable synthetic case yet
HOT260(0x08AD9CD4u, func_08AD9CD4, Word);
HOT260(0x08AF8624u, func_08AF8624, Word);
HOT260(0x08B54940u, func_08B54940, Word);
HOT260(0x08B55924u, func_08B55924, Word);
HOT260(0x08B21758u, func_08B21758, Word);
HOT260(0x08B53210u, func_08B53210, Word);
HOT260(0x08B54810u, func_08B54810, Word);
HOT260(0x08AD8BD0u, func_08AD8BD0, Word);
HOT260(0x08AE35BCu, func_08AE35BC, Word);
HOT260(0x08B80948u, func_08B80948, Word);
HOT260(0x08932598u, func_08932598, Word);
