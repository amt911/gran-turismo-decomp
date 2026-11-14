// Hot batch 266: fifty-two- to fifty-six-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (ring-buffer writes, two jump-table switches, partitions and insertion sorts,
// vector erases, string assignment, guarded static construction, Euler-to-quaternion products and a block split).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_266 {
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

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}

// `div` for the ring-buffer cursors: the remainder register is only written for a non-zero divisor (otherwise hi keeps what an earlier
// division left there, which the original then stores).
void divide_unless_zero(psprecomp::AllegrexContext &ctx, u32 dividend, u32 divisor) {
    if (divisor != 0u) divide(ctx, dividend, divisor);
}

// 0x08AECECC: appends a2 words (a1 = source) to the ring buffer inside a0 (read cursor +0x2090, capacity +0x2098, data from +0x20A0,
// total written +0x209C), in chunks that stop at the end of the ring (copy 0x08B79A88); the semaphore-like handle at +0x1C is signalled
// afterwards (0x08C7AA20 then 0x08C7AA08).
void ring_write(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 total = ctx.gpr[6] << 2;
    GuestFrame frame(ctx, 0x20u);
    u32 written = 0u;
    if (static_cast<s32>(total) > 0) {
        u32 room = word(rt, self + 0x2098u);
        u32 more = 0u;
        do {
            const u32 position = word(rt, self + 0x2090u);
            const u32 remaining = total - written;
            room -= position;
            if (!(static_cast<s32>(room) < static_cast<s32>(remaining))) room = remaining;
            ctx.gpr[2] = remaining;
            call_set(rt, ctx, 0x08B79A88u, {self + position + 0x20A0u, source + written, room});
            const u32 capacity = word(rt, self + 0x2098u);
            written += room;
            ctx.gpr[2] = word(rt, self + 0x2090u) + room;
            divide_unless_zero(ctx, ctx.gpr[2], capacity);
            room = capacity;
            more = static_cast<s32>(written) < static_cast<s32>(total) ? 1u : 0u;
            put(rt, self + 0x2090u, ctx.hi);
        } while (more != 0u);
    }
    const u32 handle = word(rt, self + 0x1Cu);
    call_set(rt, ctx, 0x08C7AA20u, {handle, 1u, 0u});
    const u32 total_written = word(rt, self + 0x209Cu) + total;
    put(rt, self + 0x209Cu, total_written);
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, self + 0x1Cu), 1u});
}

// 0x08B1DA9C: looks the key a0 up in the table 0x08D68A94 (0x08B1DEB4); the entry's tagged value (type word at +0, payload +8 / +0xC; types
// 0-5 through the jump table at 0x08C93310) becomes a 64-bit integer: a sign-extended word, a zero-extended word, a word pair, a float
// through 0x08B6F37C or a word pair through 0x08B6F1B8; a missing entry or another type returns the default (a2, a3). v0 / v1 = the pair.
void lookup_tagged_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 key = ctx.gpr[4];
    u32 low = ctx.gpr[6];
    u32 high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const u32 slot = call_set(rt, ctx, 0x08B1DEB4u, {0x08D68A94u, key});
    if (slot != 0u) {
        const u32 entry = word(rt, slot);
        ctx.gpr[5] = entry;
        if (entry != 0u) {
            const u32 type = word(rt, entry);
            ctx.gpr[3] = type;
            if (type < 6u) {
                const u32 target = word(rt, 0x08C93310u + type * 4u);
                ctx.gpr[4] = target;
                switch (target) {
                case 0x08B1DB04u:
                    low = word(rt, entry + 8u);
                    high = static_cast<u32>(static_cast<s32>(low) >> 31);
                    break;
                case 0x08B1DB14u:
                    low = word(rt, entry + 8u);
                    high = 0u;
                    break;
                case 0x08B1DB20u:
                    low = word(rt, entry + 8u);
                    high = word(rt, entry + 0xCu);
                    break;
                case 0x08B1DB2Cu:
                    ctx.fpr[12] = float_at(rt, entry + 8u);
                    call_set(rt, ctx, 0x08B6F37Cu, {});
                    low = ctx.gpr[2];
                    high = ctx.gpr[3];
                    break;
                case 0x08B1DB40u:
                    call_set(rt, ctx, 0x08B6F1B8u, {word(rt, entry + 8u), word(rt, entry + 0xCu)});
                    low = ctx.gpr[2];
                    high = ctx.gpr[3];
                    break;
                default:
                    break;
                }
            }
        }
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// 0x08B5DE48: a once-only initialiser guarded by the flag byte at 0x08D6E11C (counter byte at +1): the thread that raises the counter to
// 1 runs 0x08B5DDFC and sets the flag; the others wait on it (0x08C7A988 with 0x64) after 0x08C7A8A8; when the word at 0x08D6E124 is zero the
// function tail-jumps to 0x08B5DDFC. v0 = 0x08D70000 otherwise.
void run_once_guard(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kFlag = 0x08D6E11Cu;
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 flag = signed_byte(rt, kFlag);
        ctx.gpr[2] = flag;
        if (flag == 0u) {
            const u32 token = call_set(rt, ctx, 0x08C7A888u, {});
            ctx.gpr[4] = token;
            ctx.gpr[2] = kFlag;
            const u32 counter = rt.memory().load8(kFlag + 1u) + 1u;
            const u32 count = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(counter)));
            rt.memory().store8(kFlag + 1u, static_cast<std::uint8_t>(count));
            if (count == 1u) {
                call_set(rt, ctx, 0x08C7A8A8u, {token});
                call_set(rt, ctx, 0x08B5DDFCu, {});
                rt.memory().store8(kFlag, static_cast<std::uint8_t>(count));
            } else {
                call_set(rt, ctx, 0x08C7A8A8u, {token});
                ctx.gpr[2] = signed_byte(rt, kFlag);
                if (ctx.gpr[2] == 0u) {
                    do {
                        call_set(rt, ctx, 0x08C7A988u, {0x64u});
                    } while (signed_byte(rt, kFlag) == 0u);
                }
            }
        }
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[3] = word(rt, 0x08D6E124u);
        tail = ctx.gpr[3] == 0u;
    }
    if (tail) jump(ctx, 0x08B5DDFCu);
}

// 0x08C3E8DC: insertion sort of the eight-byte records [a0, a1) by the float at +4 (ascending): a record smaller than the first is moved
// to the front shifting the others up, any other goes through the guarded insert 0x08C3E7E0 with a stack copy. v0 follows the last
// step (the old first word, the insert's result or the cursor).
void sort_float_records(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (first == last) return;
    ctx.gpr[2] = first + 8u;
    u32 offset = 8u;
    if (first + 8u == last) return;
    u32 cursor = first + 8u;
    for (;;) {
        const float key = float_at(rt, cursor + 4u);
        const float front = float_at(rt, first + 4u);
        const u32 key_word = word(rt, cursor);
        if (key < front) {
            const u32 moved = static_cast<u32>(static_cast<s32>(offset) >> 3);
            ctx.gpr[2] = cursor + 8u;
            if (static_cast<s32>(moved) > 0) {
                u32 from = cursor;
                u32 to = cursor + 8u;
                for (u32 step = 0u; step != moved; ++step) {
                    from -= 8u;
                    const u32 low = word(rt, from);
                    const u32 high = word(rt, from + 4u);
                    to -= 8u;
                    ctx.gpr[2] = low;
                    put(rt, to, low);
                    put(rt, to + 4u, high);
                }
            }
            put(rt, first, key_word);
            put_float(rt, first + 4u, key);
        } else {
            put(rt, frame.at(0u), key_word);
            put_float(rt, frame.at(4u), key);
            call_set(rt, ctx, 0x08C3E7E0u, {cursor, frame.at(0u)});
        }
        cursor += 8u;
        offset += 8u;
        if (last == cursor) break;
        ctx.gpr[2] = cursor;
    }
}

// 0x08C75EFC: copy-assigns the reference-counted string a1 to a0 (data pointers, representation 0xC before them, count at -4): a different
// representation that is unshareable (negative count) is cloned (0x08C74458), a shareable one gets a reference (0x08B6085C) unless it is the
// empty representation at 0x08D77498; the old representation loses one (0x08B60804, freed by 0x08C74884 at zero or below). v0 = a0.
void assign_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    u32 old_data = word(rt, self);
    const u32 other_data = word(rt, ctx.gpr[5]);
    ctx.gpr[4] = other_data - 0xCu;
    if (old_data - 0xCu != other_data - 0xCu) {
        constexpr u32 kEmpty = 0x08D77498u;
        u32 new_data = 0u;
        if (static_cast<s32>(word(rt, other_data - 0xCu + 8u)) < 0) {
            new_data = call_set(rt, ctx, 0x08C74458u, {other_data - 0xCu, frame.at(0u), 0u});
            old_data = word(rt, self);
            ctx.gpr[2] = 0x08D70000u;
        } else {
            ctx.gpr[2] = 0x08D70000u;
            ctx.gpr[4] = other_data - 4u;
            if (other_data - 0xCu != kEmpty) {
                call_set(rt, ctx, 0x08B6085Cu, {other_data - 4u, 1u});
                old_data = word(rt, self);
            }
            new_data = other_data;
        }
        const u32 old_representation = old_data - 0xCu;
        ctx.gpr[4] = old_data - 4u;
        if (old_representation != kEmpty) {
            ctx.gpr[5] = 0xFFFFFFFFu;
            const u32 remaining = call_set(rt, ctx, 0x08B60804u, {old_data - 4u, 0xFFFFFFFFu});
            if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08C74884u, {old_representation, frame.at(0u)});
        }
        put(rt, self, new_data);
    }
    ctx.gpr[2] = self;
}

// 0x089C4EFC: recursively validates a tree (first children at +0xC of the nodes of the list at +0x44, siblings at +0x54, list link at
// +0x18): walks the ancestors via 0x089BAB0C while the sum of the node sizes (+8) is not below the limit of 0x08A7BF7C, and stores the
// verdict in bit 2 of the flags at +0x60 (set when a node fits, clear when the walk reaches the root or loops).
void validate_tree(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 head = word(rt, self + 0x44u);
    ctx.gpr[2] = head;
    if (head != 0u) {
        u32 list = head;
        u32 child = word(rt, list + 0xCu);
        for (;;) {
            if (child != 0u) {
                do {
                    call_set(rt, ctx, 0x089C4EFCu, {child});
                    child = word(rt, child + 0x54u);
                } while (child != 0u);
            }
            list = word(rt, list + 0x18u);
            if (list == 0u) break;
            child = word(rt, list + 0xCu);
        }
        head = word(rt, self + 0x44u);
        ctx.gpr[2] = head;
    }
    u32 current = self;
    u32 verdict = 0u;
    for (;;) {
        u32 node = head;
        u32 sum = 0u;
        if (head != 0u) {
            do {
                ctx.gpr[2] = word(rt, node + 8u);
                node = word(rt, node + 0x18u);
                sum += ctx.gpr[2];
            } while (node != 0u);
        }
        const u32 limit = call_set(rt, ctx, 0x08A7BF7Cu, {current});
        ctx.gpr[2] = sum < limit ? 1u : 0u;
        verdict = 1u;
        if (sum < limit) break;
        const u32 parent = call_set(rt, ctx, 0x089BAB0Cu, {current});
        verdict = 0u;
        if (parent == current) break;
        current = call_set(rt, ctx, 0x089BAB0Cu, {current});
        if (current == 0u) {
            verdict = 0u;
            break;
        }
        head = word(rt, self + 0x44u);
        ctx.gpr[2] = head;
    }
    const u32 flags = (word(rt, self + 0x60u) & ~4u) | ((verdict & 1u) << 2);
    ctx.gpr[2] = flags;
    put(rt, self + 0x60u, flags);
}

// 0x08B38938: reads one character through the cursor kept at a0 + 8 (0x08B1AC1C); a backslash followed (when the byte at *a0 + 0xC is set) by
// a character 0x30..0x76 that the jump table at 0x08C950F0 maps to an escape code (0, 7-0xD) consumes that character and returns the
// code; anything else returns the character itself (0x5C for an unknown escape); -1 at the end of the text.
void next_escaped_char(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 result = 0xFFFFFFFFu;
    const u32 current = rt.memory().load8(word(rt, self + 8u));
    ctx.gpr[2] = current;
    if (current != 0u) {
        result = call_set(rt, ctx, 0x08B1AC1Cu, {self + 8u});
        ctx.gpr[2] = 0x5Cu;
        if (result == 0x5Cu) {
            const u32 config = word(rt, self);
            const u32 enabled = rt.memory().load8(config + 0xCu);
            result = 0x5Cu;
            if (enabled != 0u) {
                const u32 cursor = word(rt, self + 8u);
                const u32 index = rt.memory().load8(cursor) - 0x30u;
                if (index < 0x47u) {
                    const u32 target = word(rt, 0x08C950F0u + index * 4u);
                    u32 code = 0u;
                    bool known = true;
                    switch (target) {
                    case 0x08B389B0u: code = 0u; break;
                    case 0x08B389B8u: code = 7u; break;
                    case 0x08B389C0u: code = 8u; break;
                    case 0x08B389C8u: code = 9u; break;
                    case 0x08B389D0u: code = 0xAu; break;
                    case 0x08B389D8u: code = 0xBu; break;
                    case 0x08B389E0u: code = 0xCu; break;
                    case 0x08B389F0u: code = 0xDu; break;
                    default: known = false; break;
                    }
                    if (known) {
                        put(rt, self + 8u, cursor + 1u);
                        result = code;
                    }
                }
            }
        }
    }
    ctx.gpr[4] = result;
    ctx.gpr[2] = result;
}

// 0x0882D838: constructor (a0 = the object): base constructor 0x08A2A154, vtable 0x08CBBBC0, a 16-byte member built with 0x08B62344 /
// 0x08865D1C / 0x08A21F8C / 0x08835AA8 into +0x18, the one-time static at 0x08D75160 (guard 0x08B6441C / 0x08B64494, record at
// 0x08D75168 with vtable 0x08CBE908) linked into the member's +4, and the handle from 0x08860050 into +0x14.
void construct_with_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    // The callees of the original see this function's saved registers as it sets them (s2 = a0, s1 = 0x08CF0000, then s0 = the member and, at the guard,
    // s0 = a0 + 0x18, s1 = the guard flag, s3 = the record): one of them reads s2 as the object, so they are set the same way and restored at the end.
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 saved_s2 = ctx.gpr[18];
    const u32 saved_s3 = ctx.gpr[19];
    ctx.gpr[18] = self;
    ctx.gpr[17] = 0x08CF0000u;
    call_set(rt, ctx, 0x08A2A154u, {self});
    ctx.gpr[2] = 0x08CBBBC0u;
    put(rt, self, 0x08CBBBC0u);
    const u32 member = call_set(rt, ctx, 0x08B62344u, {0x10u});
    ctx.gpr[16] = member;
    call_set(rt, ctx, 0x08865D1Cu, {member});
    call_set(rt, ctx, 0x08A21F8Cu, {member + 8u, 0x08CF6ED8u});
    call_set(rt, ctx, 0x08835AA8u, {member + 0xCu, 0x08CF6ED8u});
    put(rt, self + 0x18u, member);
    constexpr u32 kGuard = 0x08D75160u;
    constexpr u32 kRecord = 0x08D75168u;
    ctx.gpr[17] = kGuard;
    ctx.gpr[19] = kRecord;
    ctx.gpr[16] = self + 0x18u;
    if (signed_byte(rt, kGuard) == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            ctx.gpr[2] = 0x08CBE908u;
            put(rt, kRecord, 0x08CBE908u);
            call_set(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    ctx.gpr[4] = word(rt, self + 0x18u);
    put(rt, self + 0x1Cu, kRecord);
    const u32 handle = call_set(rt, ctx, 0x08860050u, {word(rt, self + 0x18u)});
    put(rt, self + 0x14u, handle);
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
    ctx.gpr[18] = saved_s2;
    ctx.gpr[19] = saved_s3;
}

// 0x088D2594: sets up the 16 entries of 12 bytes from this + 0x238 (0x088D1DAC with a record of {-1, a1, 0} or, when a2 points at
// 16 words, {-1, a2[i], 0}; 0x088D1D90 for every entry when a2 is null), after one call for this + 0x22C, and ends with 0x088D1D90 on
// this + 0x2F8.
void init_entry_table(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 values = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const u32 record = frame.at(0xCu);
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, frame.at(0x10u), argument);
    put(rt, record, 0xFFFFFFFFu);
    put(rt, frame.at(0x14u), 0u);
    call_set(rt, ctx, 0x088D1DACu, {self + 0x22Cu, record});
    if (values == 0u) {
        for (u32 i = 0u; i != 0x10u; ++i) call_set(rt, ctx, 0x088D1D90u, {self + 0x238u + i * 0xCu});
    } else {
        u32 value = word(rt, values);
        for (u32 i = 0u; i != 0x10u; ++i) {
            ctx.gpr[2] = value;
            put(rt, frame.at(4u), value);
            put(rt, frame.at(0u), 0xFFFFFFFFu);
            put(rt, frame.at(8u), 0u);
            call_set(rt, ctx, 0x088D1DACu, {self + 0x238u + i * 0xCu, frame.at(0u)});
            if (i + 1u != 0x10u) value = word(rt, values + (i + 1u) * 4u);
        }
    }
    call_set(rt, ctx, 0x088D1D90u, {self + 0x2F8u});
}

// 0x088E9614: runs the setup step 0x088E9520 over a0 for the stage codes the state of a0 + 4 (0x088D62FC) selects (2 / 1 / 4 / 5 or just 3),
// then 0, and applies 0x088DA788 / 0x088D0924 to every entry of the halfword table at +0x138 (count from 0x088D9F80). v0 = 0.
void run_setup_steps(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 state_object = self + 4u;
    bool all_stages = true;
    ctx.gpr[5] = 2u;
    if (call_set(rt, ctx, 0x088D62FCu, {state_object}) != 0u) {
        const u32 state = call_set(rt, ctx, 0x088D62FCu, {state_object});
        ctx.gpr[3] = 4u;
        if (state != 4u) {
            call_set(rt, ctx, 0x088E9520u, {self, 3u});
            all_stages = false;
        } else {
            ctx.gpr[5] = 2u;
        }
    }
    if (all_stages) {
        ctx.gpr[4] = self;
        call_set(rt, ctx, 0x088E9520u, {self, ctx.gpr[5]});
        call_set(rt, ctx, 0x088E9520u, {self, 1u});
        call_set(rt, ctx, 0x088E9520u, {self, 4u});
        call_set(rt, ctx, 0x088E9520u, {self, 5u});
    }
    call_set(rt, ctx, 0x088E9520u, {self, 0u});
    for (u32 index = 0u;; ++index) {
        const u32 count = call_set(rt, ctx, 0x088D9F80u, {self});
        const bool more = static_cast<s32>(index) < static_cast<s32>(count);
        ctx.gpr[2] = more ? 1u : 0u;
        ctx.gpr[3] = index << 1;
        ctx.gpr[4] = self;
        if (!more) break;
        const u32 address = (index << 1) + word(rt, self + 0x138u);
        ctx.gpr[2] = address;
        const u32 object = call_set(rt, ctx, 0x088DA788u, {self, rt.memory().load16(address)});
        call_set(rt, ctx, 0x088D0924u, {object, index});
    }
}

// 0x089EFB10: constructor (a0 = the object): base constructor 0x089E20D0, vtables 0x08CBF080 / 0x08CBF2BC, the shared empty string in
// +0x238 / +0x23C, two string members at +0x240 / +0x244 (0x089E4C14), scale 2.0 at +0x234, 1.0 / -1.0 limits at +0x24C..+0x258 and
// zeroed state. v0 = 0.
void construct_with_limits(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x089E20D0u, {self});
    put(rt, self, 0x08CBF080u);
    constexpr u32 kEmptyData = 0x08D74FD0u + 0xCu;
    ctx.gpr[6] = kEmptyData;
    ctx.gpr[2] = 0x08CBF2BCu;
    ctx.gpr[3] = 1u;
    ctx.fpr[0] = 2.0f;
    put(rt, self + 0x38u, 0x08CBF2BCu);
    put_float(rt, self + 0x234u, 2.0f);
    put(rt, self + 0x22Cu, 1u);
    put(rt, self + 0x23Cu, kEmptyData);
    put(rt, self + 0x238u, kEmptyData);
    put(rt, self + 0x230u, 0u);
    call_set(rt, ctx, 0x089E4C14u, {self + 0x240u, 0x08CF6ED8u, kEmptyData});
    call_set(rt, ctx, 0x089E4C14u, {self + 0x244u, 0x08CF6ED8u, kEmptyData});
    put_float(rt, self + 0x254u, 1.0f);
    put_float(rt, self + 0x258u, -1.0f);
    put(rt, self + 0x248u, 0u);
    put_float(rt, self + 0x24Cu, 1.0f);
    put(rt, self + 0x250u, 0u);
    put(rt, self + 0x25Cu, 0u);
    rt.memory().store8(self + 0x260u, 0u);
    rt.memory().store8(self + 0x261u, 0u);
    rt.memory().store8(self + 0x262u, 0u);
    ctx.gpr[2] = 0u;
    put(rt, self + 0x264u, 0u);
    rt.memory().store8(self + 0xA68u, 0u);
    put(rt, self + 0xE68u, 0u);
    put(rt, self + 0xE6Cu, 0u);
}

// 0x08AD9F10: initialises the pool record at 0x08D405FC: zeroes its eleven words, runs 0x08AD8960 with the descriptor at 0x08C90B90, allocates
// count * 0x58 bytes (0x08AD84B4; count at +0x28) into +0x24 and, when that succeeds, links the two list heads (+4 and +0x14 point at
// themselves, -1 sentinels), fills the block (0x08AD8C18) and sets the ready word. Returns 1 on success, 0 otherwise.
void init_pool_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kRecord = 0x08D405FCu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 0x08C90B90u;
    ctx.gpr[6] = kRecord + 0x28u;
    ctx.gpr[7] = 0x20u;
    for (u32 offset = 0u; offset <= 0x28u; offset += 4u) put(rt, kRecord + offset, 0u);
    call_set(rt, ctx, 0x08AD8960u, {ctx.gpr[4], 0x08C90B90u, kRecord + 0x28u, 0x20u});
    const u32 count = word(rt, kRecord + 0x28u);
    ctx.gpr[2] = 0x58u;
    const u32 bytes = static_cast<u32>(static_cast<s32>(count) * 0x58);
    const u32 block = call_set(rt, ctx, 0x08AD84B4u, {bytes, 1u});
    put(rt, kRecord + 0x24u, block);
    u32 ready = 0u;
    if (block != 0u) {
        const u32 first_head = kRecord + 4u;
        const u32 second_head = kRecord + 0x14u;
        put(rt, first_head + 0xCu, 0u);
        put(rt, second_head + 8u, 0xFFFFFFFFu);
        put(rt, first_head + 4u, first_head);
        put(rt, first_head + 8u, 0xFFFFFFFFu);
        put(rt, second_head + 4u, second_head);
        put(rt, second_head + 0xCu, 0u);
        put(rt, kRecord + 4u, first_head);
        put(rt, kRecord + 0x14u, second_head);
        call_set(rt, ctx, 0x08AD8C18u, {second_head, block, word(rt, kRecord + 0x28u), 0x58u});
        ctx.gpr[2] = 1u;
        put(rt, kRecord, 1u);
        ready = 1u;
    }
    ctx.gpr[3] = ready;
    ctx.gpr[2] = ready;
}

// 0x08AEEEF4: under the lock handle 0x08C7AA20 / 0x08C7AA08 (at 0x08D41AD0 + 0x4F8), clears (-1, -1) every 32-byte slot of the ring
// at +0x500 (size +0x504, start +0x50C, used count +0x510) whose key word equals a0, flags the table (+0x530) when the pending key (+0x514)
// matches, then calls 0x08AEEFD4 with the key.
void invalidate_key(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08D41AD0u;
    const u32 key = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08C7AA20u, {word(rt, kTable + 0x4F8u), 1u, 0u});
    const u32 used = word(rt, kTable + 0x510u);
    if (static_cast<s32>(used) > 0) {
        u32 index = 0u;
        ctx.gpr[2] = word(rt, kTable + 0x50Cu);
        for (;;) {
            const u32 size = word(rt, kTable + 0x504u);
            ctx.gpr[2] += index;
            divide_unless_zero(ctx, ctx.gpr[2], size);
            const u32 slot = word(rt, kTable + 0x500u) + (ctx.hi << 5);
            ctx.gpr[2] = word(rt, slot);
            ++index;
            if (ctx.gpr[2] == key) {
                put(rt, slot + 4u, 0xFFFFFFFFu);
                put(rt, slot, 0xFFFFFFFFu);
            }
            ctx.gpr[2] = static_cast<s32>(index) < static_cast<s32>(word(rt, kTable + 0x510u)) ? 1u : 0u;
            if (ctx.gpr[2] == 0u) break;
            ctx.gpr[2] = word(rt, kTable + 0x50Cu);
        }
    }
    const bool pending = word(rt, kTable + 0x514u) == key;
    ctx.gpr[2] = 1u;
    if (pending) put(rt, kTable + 0x530u, 1u);
    call_set(rt, ctx, 0x08C7AA08u, {word(rt, kTable + 0x4F8u), 1u});
    call_set(rt, ctx, 0x08AEEFD4u, {key, 1u});
}

// 0x08B275A4: drains the pending-bit count (halfword at +0x3E) of the object a0 in steps through 0x08B27474 / 0x08B27444 (arguments 0x10,
// then up to six units per round: min(count, 6) with the remainder 3 or count - 3 passed on), then flushes with the halfword at +0x3C and
// clears the count. A count of 0 stops without the flush; counts 1 and 2 go straight to the flush.
void drain_bit_count(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    u32 count = rt.memory().load16(self + 0x3Eu);
    for (;;) {
        const u32 current = count & 0xFFFFu;
        ctx.gpr[2] = current;
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0x10u;
        if (current == 1u) break;
        if (current == 0u) {
            rt.memory().store16(self + 0x3Eu, 0u);
            return;
        }
        if (current == 2u) {
            call_set(rt, ctx, 0x08B27474u, {self, rt.memory().load16(self + 0x3Cu)});
            call_set(rt, ctx, 0x08B27444u, {self, rt.memory().load16(self + 0x3Cu)});
            break;
        }
        u32 unit = 6u;
        call_set(rt, ctx, 0x08B27474u, {self, 0x10u});
        call_set(rt, ctx, 0x08B27444u, {self, 0x10u});
        const u32 after = rt.memory().load16(self + 0x3Eu);
        u32 argument = 3u;
        ctx.gpr[2] = after < 7u ? 1u : 0u;
        if (after < 7u) {
            unit = after & 0xFFFFu;
            argument = after - 3u;
        }
        call_set(rt, ctx, 0x08B27474u, {self, argument});
        const u32 reduced = rt.memory().load16(self + 0x3Eu) - unit;
        ctx.gpr[2] = reduced;
        rt.memory().store16(self + 0x3Eu, static_cast<std::uint16_t>(reduced));
        count = reduced;
    }
    call_set(rt, ctx, 0x08B27474u, {self, rt.memory().load16(self + 0x3Cu)});
    call_set(rt, ctx, 0x08B27444u, {self, rt.memory().load16(self + 0x3Cu)});
    rt.memory().store16(self + 0x3Eu, 0u);
}

// 0x08B4C6BC / 0x08B4C94C: builds the products of the half-angle sines and cosines of three angles (f12, f13, f14, each halved and
// converted by 0x08B498AC into a (sin, cos) pair on the stack) and stores the four resulting components at a0 + 4, +0xC, +0 and +8
// (the two functions differ in the signs and pairings of the products).
struct HalfAngles {
    float s0, c0, s1, c1, s2, c2;
};

HalfAngles half_angles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame) {
    const float half = 0.5f;
    const float third = ctx.fpr[14];
    const float second = ctx.fpr[13];
    ctx.fpr[12] = ctx.fpr[12] * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)});
    ctx.fpr[12] = second * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)});
    ctx.fpr[12] = third * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(0x10u), frame.at(0x14u)});
    return {float_at(rt, frame.at(0u)), float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u)), float_at(rt, frame.at(0xCu)),
            float_at(rt, frame.at(0x10u)), float_at(rt, frame.at(0x14u))};
}

void compose_angles_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const HalfAngles h = half_angles(rt, ctx, frame);
    const float f6 = h.s0 * h.c1;
    const float f3 = h.c1 * h.c0;
    const float f5 = h.s0 * h.s1;
    const float f1 = h.c0 * h.s1;
    const float f7 = h.c2 * f3;
    const float f4 = h.c2 * f1;
    const float f8 = f6 * h.s2;
    const float f9 = h.s2 * f5;
    const float f6b = f6 * h.c2;
    const float f1b = f1 * h.s2;
    const float f2 = h.c2 * f5;
    const float f0 = h.s2 * f3;
    put_float(rt, out + 4u, f4 + f8);
    put_float(rt, out + 0xCu, f7 + f9);
    put_float(rt, out, f6b - f1b);
    put_float(rt, out + 8u, f0 - f2);
}

void compose_angles_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const HalfAngles h = half_angles(rt, ctx, frame);
    const float f6 = h.s0 * h.c1;
    const float f5 = h.c1 * h.c0;
    const float f4 = h.s0 * h.s1;
    const float f1 = h.c0 * h.s1;
    const float f7 = h.c2 * f5;
    const float f3 = h.c2 * f1;
    const float f8 = f6 * h.s2;
    const float f9 = h.s2 * f4;
    const float f6b = f6 * h.c2;
    const float f1b = f1 * h.s2;
    const float f0 = h.c2 * f4;
    const float f2 = h.s2 * f5;
    put_float(rt, out + 4u, f3 + f8);
    put_float(rt, out + 0xCu, f7 - f9);
    put_float(rt, out, f6b - f1b);
    put_float(rt, out + 8u, f0 + f2);
}

// 0x08B54984: allocates a block of a1-tagged memory of a2 rounded up to 0x400 from the pool a0: finds a free chunk (0x08B54884); an exact
// fit is unlinked (0x08B548E4) and tagged, a larger one is split: a node from 0x08B54810 takes the front part, the chunk shrinks and is
// re-inserted (0x08B54940). Returns the block record or 0.
void pool_allocate_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 pool = ctx.gpr[4];
    const u32 tag = ctx.gpr[5];
    const u32 wanted = (ctx.gpr[6] + 0x3FFu) & ~0x3FFu;
    GuestFrame frame(ctx, 0x20u);
    u32 block = call_set(rt, ctx, 0x08B54884u, {pool, wanted});
    if (block != 0u) {
        const u32 size = word(rt, block + 8u);
        if (wanted == size) {
            call_set(rt, ctx, 0x08B548E4u, {pool, block});
            put(rt, block, tag);
        } else {
            const u32 node = call_set(rt, ctx, 0x08B54810u, {pool});
            ctx.gpr[5] = block;
            ctx.gpr[4] = pool;
            if (node == 0u) {
                block = 0u;
            } else {
                call_set(rt, ctx, 0x08B548E4u, {pool, block});
                const u32 start = word(rt, block + 4u);
                put(rt, node + 8u, wanted);
                put(rt, node + 4u, start);
                const u32 chunk_start = word(rt, block + 4u);
                const u32 chunk_size = word(rt, block + 8u);
                put(rt, node, tag);
                put(rt, block + 4u, wanted + chunk_start);
                put(rt, block + 8u, chunk_size - wanted);
                call_set(rt, ctx, 0x08B54940u, {pool, block});
                block = node;
            }
        }
    } else {
        block = 0u;
    }
    ctx.gpr[2] = block;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08AECECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ring_write(rt, ctx); }
void func_08B1DA9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_tagged_value(rt, ctx); }
void func_08B5DE48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { run_once_guard(rt, ctx); }
void func_08B8AFE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); G[20] = G[5] + 8u; put(rt, frame.at(0xCu), G[19]); G[19] = G[5]; put(rt, frame.at(8u), G[18]); G[18] = G[4]; put(rt, frame.at(0u), G[16]); G[16] = G[4] - 0x18u; put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(4u), G[17]); while (true) { G[17] = G[16] + 8u; G[6] = word(rt, G[16] + 8u); G[4] = word(rt, G[19] + 8u); G[5] = word(rt, G[20] + 4u); G[7] = word(rt, G[17] + 4u); call_here(rt, ctx, 0x08B8A72Cu); G[6] = G[18] + 8u; G[5] = G[17]; G[4] = G[6]; if (G[2] == 0u) break; G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); put(rt, G[18], G[2]); put(rt, G[18] + 4u, G[3]); G[18] = G[16]; G[16] -= 0x18u; if (G[6] != G[17]) call_here(rt, ctx, 0x08B8AD88u); } G[2] = word(rt, G[19]); G[3] = word(rt, G[19] + 4u); put(rt, G[18], G[2]); put(rt, G[18] + 4u, G[3]); if (G[6] != G[20]) { G[5] = G[20]; tail = true; } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, 0x08B8AD88u); } }
void func_08C2E994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[6]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x20u), G[31]); while (true) { while (true) { G[3] = word(rt, G[18]); G[4] = G[18]; G[5] = G[17]; G[2] = word(rt, G[3] + 8u); G[6] = G[19]; call_here(rt, ctx, G[2]); if (G[2] == 0u) break; G[17] += 4u; } do { G[3] = word(rt, G[18]); G[16] -= 4u; G[6] = G[16]; G[2] = word(rt, G[3] + 8u); G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, G[2]); } while (G[2] != 0u); G[2] = G[17] < G[16] ? 1u : 0u; G[4] = frame.at(0u); if (G[2] == 0u) break; G[5] = G[17]; call_here(rt, ctx, 0x08A76ABCu); if (G[16] != G[17]) { G[5] = word(rt, G[16]); G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[5] = word(rt, frame.at(0u)); if (G[16] != frame.at(0u)) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[17] += 4u; } G[2] = G[17]; G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C3E8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sort_float_records(rt, ctx); }
void func_08C56CB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; G[5] = G[16]; put(rt, frame.at(0x14u), G[31]); G[6] = 1u; put(rt, G[4] + 4u, 0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08C55FDCu); G[2] = load8(frame.at(0u)); if (G[2] != 0u) { G[2] = word(rt, G[16]); G[3] = word(rt, G[2] - 0xCu); G[3] = G[16] + G[3]; G[4] = word(rt, G[3] + 0x78u); G[3] = word(rt, G[4] + 8u); G[2] = word(rt, G[4] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[2] = G[3] + 1u; G[5] = load8(G[3]); put(rt, G[4] + 8u, G[2]); } else { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x28u); call_here(rt, ctx, G[2]); G[5] = G[2]; } G[2] = 0xFFFFFFFFu; if (G[5] == G[2]) { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] |= 2u; call_here(rt, ctx, 0x08C54CFCu); } else { G[2] = 1u; put(rt, G[16] + 4u, G[2]); } } G[2] = G[16]; G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C75EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_string(rt, ctx); }
void func_089C4EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { validate_tree(rt, ctx); }
void func_08B1916C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[17]); G[17] = G[6]; put(rt, frame.at(0x20u), G[16]); G[16] = G[5]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); G[2] = word(rt, G[4]); G[4] = frame.at(0u); G[5] = G[2]; call_here(rt, ctx, 0x08B19154u); G[2] = word(rt, frame.at(8u)); if (G[2] == 0u) G[2] = 0u; else { G[18] = word(rt, frame.at(0xCu)); while (true) { G[5] = word(rt, G[18] + 8u); bool match; if (G[17] == 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08B35FD8u); match = G[2] == 0u; G[2] = G[18]; } else { G[2] = G[18]; if (G[17] == G[16]) match = true; else { G[3] = sext(load8(G[16]), 8); G[2] = sext(load8(G[5]), 8); G[4] = G[16]; G[6] = G[16] - G[17]; if (G[3] != G[2]) match = false; else { G[4] += 1u; match = true; while (true) { G[2] = G[16] - G[6]; G[5] += 1u; if (G[2] == G[4]) break; G[3] = sext(load8(G[5]), 8); G[2] = sext(load8(G[4]), 8); G[4] += 1u; if (G[3] != G[2]) { match = false; break; } } } } } if (match) { G[2] = G[18]; break; } G[4] = frame.at(0u); call_here(rt, ctx, 0x08B18CECu); G[3] = word(rt, frame.at(8u)); G[18] = word(rt, frame.at(0xCu)); if (G[3] == 0u) { G[2] = 0u; break; } } } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_08B38938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { next_escaped_char(rt, ctx); }
void func_0882D838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_member(rt, ctx); }
void func_088D2594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_entry_table(rt, ctx); }
void func_088E9614(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { run_setup_steps(rt, ctx); }
void func_089C10C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4] + 0x2B0u; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; G[4] = G[19]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = frame.at(8u); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[5] = 0x08CF6ED8u; G[4] = G[18]; G[16] = G[17] + 0x264u; call_here(rt, ctx, 0x089C15FCu); G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08C8CAE8u; G[6] = frame.at(1u); G[4] = G[20]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[17] + 0x230u; G[5] = G[20]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08B3457Cu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089C5AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = 0x08CF0000u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = 0x08CF4E98u; put(rt, frame.at(0xCu), G[19]); G[19] = G[5]; put(rt, frame.at(8u), G[18]); G[18] = G[16] + 0x4Cu; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16] + 0x54u); G[16] = G[4]; if (G[4] == G[18]) G[16] = 0u; else { G[20] = 0x08CA0000u; while (true) { G[17] = word(rt, G[4] + 0x14u); G[4] = 0x08CA638Cu; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[19]); G[4] = G[17]; call_here(rt, ctx, 0x089F0B0Cu); G[4] = G[16]; if (G[2] != 0u) { G[16] = word(rt, G[16] + 0x14u); if (G[16] == 0u) { G[4] = 0x08CA638Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = word(rt, G[19]); G[4] = G[16]; call_here(rt, ctx, 0x089F0B28u); G[16] = G[2]; break; } call_here(rt, ctx, 0x08B5F100u); G[16] = G[2]; G[4] = G[2]; if (G[18] == G[2]) { G[16] = 0u; break; } } } G[4] = 0x08CF4E98u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_089EFB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_limits(rt, ctx); }
void func_08A14418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = 1u; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[17] = G[4] + 0x18u; store8(G[4] + 0x10Cu, G[18]); G[4] += 0xD8u; put(rt, G[16] + 0x108u, 0u); call_here(rt, ctx, 0x08B28C74u); G[4] = word(rt, G[16] + 0x104u); call_here(rt, ctx, 0x08B2E224u); G[4] = word(rt, G[16] + 0x104u); call_here(rt, ctx, 0x08B2E28Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x089C9C34u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA9A60u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x089C4FD4u); store8(G[2] + 0x1B4u, G[18]); G[16] = G[2]; call_here(rt, ctx, 0x08B1C47Cu); G[5] = 0u; G[4] = G[17]; call_here(rt, ctx, 0x08A1AA24u); G[4] = G[17]; call_here(rt, ctx, 0x08AC0090u); G[4] = G[17]; call_here(rt, ctx, 0x08AC00D8u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x128u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA9A60u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x089C5E4Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A767EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x440u); put(rt, frame.at(0x418u), G[18]); G[2] = frame.at(0x428u); G[18] = frame.at(4u); put(rt, frame.at(0x414u), G[17]); G[17] = G[4]; put(rt, frame.at(0x410u), G[16]); G[16] = G[5]; put(rt, frame.at(0x41Cu), G[31]); G[3] = load8(G[4] + 4u); put(rt, frame.at(0x428u), G[6]); G[4] = G[18]; put(rt, frame.at(0x42Cu), G[7]); put(rt, frame.at(0x430u), G[8]); put(rt, frame.at(0x434u), G[9]); put(rt, frame.at(0x438u), G[10]); put(rt, frame.at(0x43Cu), G[11]); put(rt, frame.at(0u), G[2]); if (G[3] != 0u) { G[2] = word(rt, G[17]); G[18] = frame.at(4u); G[2] = G[2] << 2; G[4] = G[18]; if (static_cast<std::int32_t>(G[2]) > 0) { G[5] = G[2]; G[4] = 0u; G[18] = frame.at(4u); G[6] = 0x20u; do { G[3] = G[4] + G[18]; G[4] += 1u; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; store8(G[3], G[6]); } while (G[2] != 0u); G[4] += G[18]; } } G[6] = word(rt, frame.at(0u)); G[5] = G[16]; call_here(rt, ctx, 0x08B373ECu); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08A7677Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B35F60u); G[2] = G[16] + G[2]; G[3] = sext(load8(G[2] - 1u), 8); G[3] ^= 0xAu; G[3] = G[3] < 1u ? 1u : 0u; store8(G[17] + 4u, G[3]); G[18] = word(rt, frame.at(0x418u)); G[17] = word(rt, frame.at(0x414u)); G[16] = word(rt, frame.at(0x410u)); } } }
void func_08AD9F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_pool_record(rt, ctx); }
void func_08AEEEF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { invalidate_key(rt, ctx); }
void func_08B275A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { drain_bit_count(rt, ctx); }
void func_08B4C6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { compose_angles_a(rt, ctx); }
void func_08B4C94C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { compose_angles_b(rt, ctx); }
void func_08B54984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pool_allocate_block(rt, ctx); }
void func_08BE5DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(0x18u), G[18]); G[2] = 0xCCCCCCCDu; put(rt, frame.at(0x14u), G[17]); G[18] = G[6]; G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; G[5] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[31]); G[3] = word(rt, G[4] + 4u); G[3] -= G[6]; G[3] = sra(G[3], 3); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[8] = ctx.lo; G[4] = G[16]; if (static_cast<std::int32_t>(G[8]) > 0) { G[7] = 0u; do { F[0] = float_at(rt, G[6] + 0xCu); F[1] = float_at(rt, G[6] + 0x10u); G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6] + 8u); F[2] = float_at(rt, G[6] + 0x14u); F[3] = float_at(rt, G[6] + 0x18u); F[4] = float_at(rt, G[6] + 0x1Cu); put(rt, G[4] + 4u, G[2]); G[7] += 1u; put(rt, G[4] + 8u, G[3]); put_float(rt, G[4] + 0xCu, F[0]); put_float(rt, G[4] + 0x10u, F[1]); put_float(rt, G[4] + 0x14u, F[2]); put_float(rt, G[4] + 0x18u, F[3]); put_float(rt, G[4] + 0x1Cu, F[4]); F[0] = float_at(rt, G[6] + 0x20u); F[1] = float_at(rt, G[6] + 0x24u); G[6] += 0x28u; put_float(rt, G[4] + 0x20u, F[0]); put_float(rt, G[4] + 0x24u, F[1]); G[4] += 0x28u; } while (G[8] != G[7]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BE593Cu); G[3] = word(rt, G[17] + 4u); G[2] = (G[18] - G[16]) & ~7u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_266
} // namespace gt::game

#define HOT266(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_266::name, gt::ReturnKind::kind)
HOT266(0x08AECECCu, func_08AECECC, Word);
HOT266(0x08B1DA9Cu, func_08B1DA9C, DoubleWord);
HOT266(0x08B5DE48u, func_08B5DE48, Word);
HOT266(0x08B8AFE8u, func_08B8AFE8, Word);
// HOT266(0x08C2E994u, func_08C2E994, Word);  // no comparable synthetic case yet
HOT266(0x08C3E8DCu, func_08C3E8DC, Word);
HOT266(0x08C56CB4u, func_08C56CB4, Word);
HOT266(0x08C75EFCu, func_08C75EFC, Word);
HOT266(0x089C4EFCu, func_089C4EFC, Word);
// HOT266(0x08B1916Cu, func_08B1916C, Word);  // no comparable synthetic case yet
HOT266(0x08B38938u, func_08B38938, Word);
HOT266(0x0882D838u, func_0882D838, Word);
HOT266(0x088D2594u, func_088D2594, Word);
HOT266(0x088E9614u, func_088E9614, Word);
// HOT266(0x089C10C8u, func_089C10C8, Word);  // no comparable synthetic case yet
// HOT266(0x089C5AD8u, func_089C5AD8, Word);  // no comparable synthetic case yet
HOT266(0x089EFB10u, func_089EFB10, Word);
// HOT266(0x08A14418u, func_08A14418, Word);  // no comparable synthetic case yet
HOT266(0x08A767ECu, func_08A767EC, Word);
HOT266(0x08AD9F10u, func_08AD9F10, Word);
HOT266(0x08AEEEF4u, func_08AEEEF4, Word);
HOT266(0x08B275A4u, func_08B275A4, Word);
HOT266(0x08B4C6BCu, func_08B4C6BC, Word);
HOT266(0x08B4C94Cu, func_08B4C94C, Word);
HOT266(0x08B54984u, func_08B54984, Word);
// HOT266(0x08BE5DC0u, func_08BE5DC0, Word);  // no comparable synthetic case yet
