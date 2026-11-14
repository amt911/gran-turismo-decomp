// Hot batch 219: fifty-four-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_219 {
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
    if (!(value >= -2197483648.0f && value < 2197483648.0f)) return 0x7FFFFFFF;
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

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// 0x08B94000 / 0x08C1EE14 (signed keys) and 0x08C25D8C (unsigned keys): the unique insert of a red-black
// tree (std::map): header at a0 (+4 the end node, +8 the root, +0xC the leftmost), key *a1. The tree is walked to a leaf (children
// at +8 / +0xC, key at +0x10); unless the key goes left of a node that is not the leftmost, its predecessor (0x08B5F174) is compared
// with it; a new key goes to `insert`, an equal one gives back its node. The flag byte (1 when inserted) is stored in the frame at
// sp + 4 and read back as a word into v1; v0 is the node.
void insert_unique(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 insert, bool signed_keys) {
    const u32 tree = ctx.gpr[4];
    const u32 key_pointer = ctx.gpr[5];
    auto less = [signed_keys](u32 a, u32 b) { return signed_keys ? static_cast<s32>(a) < static_cast<s32>(b) : a < b; };
    GuestFrame frame(ctx, 0x30u);
    u32 node = word(rt, tree + 8u);
    u32 parent = tree + 4u;
    u32 position = 0u;
    bool compare_first = false;
    if (node != 0u) {
        const u32 key = word(rt, key_pointer);
        bool goes_left = false;
        for (;;) {
            goes_left = less(key, word(rt, node + 0x10u));
            const u32 child = goes_left ? word(rt, node + 8u) : word(rt, node + 0xCu);
            if (child == 0u) {
                parent = node;
                break;
            }
            node = child;
        }
        position = node;
        ctx.gpr[4] = goes_left ? 1u : 0u;
        compare_first = !goes_left;
    }
    u32 flag = 0u;
    bool inserting = true;
    if (!compare_first) {
        ctx.gpr[2] = word(rt, tree + 0xCu);
        ctx.gpr[4] = tree;
        if (parent != ctx.gpr[2]) {
            ctx.gpr[4] = parent;
            position = call_set(rt, ctx, 0x08B5F174u, {parent});
            compare_first = true;
        }
    }
    if (compare_first) {
        const u32 predecessor_key = word(rt, position + 0x10u);
        const u32 key = word(rt, key_pointer);
        ctx.gpr[2] = less(predecessor_key, key) ? 1u : 0u;
        ctx.gpr[3] = key;
        ctx.gpr[4] = 0u;
        inserting = ctx.gpr[2] != 0u;
        if (inserting) ctx.gpr[4] = tree;
    }
    if (inserting) {
        position = call_set(rt, ctx, insert, {tree, 0u, parent, key_pointer});
        ctx.gpr[5] = position;
        flag = 1u;
    }
    rt.memory().store8(frame.at(4u), static_cast<std::uint8_t>(flag));
    ctx.gpr[4] = flag;
    ctx.gpr[5] = position;
    ctx.gpr[2] = position;
    ctx.gpr[3] = word(rt, frame.at(4u));
}

// 0x08A27538 and two copies: for a count a1 > 0, asks the member *a2 (virtual +0x3C, null message 0x08CAB3AC) for a value, looks up an
// integer with 0x08A26F3C (the text 0x08CF64FC and whether the value is non-zero), turns it into a float percentage (1e6 or more gives
// 100.0, positive values are divided by 10000.0, the rest stay as converted) and calls 0x08A87EEC with it in f12 and the slot at sp;
// the slot is copied into a0 when it is not the slot itself (0x08A5BAD8) and released (0x08A5B9EC).
void convert_member_percentage(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(0u);
    if (static_cast<s32>(count) <= 0) return;
    const u32 member = word(rt, holder);
    if (member == 0u) {
        ctx.gpr[4] = 0x08CAB3ACu;
        call_here(rt, ctx, 0x08A5BB7Cu);
    }
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 value = call_set(rt, ctx, ctx.gpr[2], {member});
    const u32 found = call_set(rt, ctx, 0x08A26F3Cu, {0x08CF64FCu, value != 0u ? 1u : 0u});
    float percentage = static_cast<float>(static_cast<s32>(found));
    ctx.fpr[0] = std::bit_cast<float>(found);
    ctx.fpr[12] = percentage;
    ctx.gpr[1] = 0x49742400u;
    ctx.fpr[0] = std::bit_cast<float>(ctx.gpr[1]);
    const bool saturated = ctx.fpr[0] <= percentage;
    ctx.fpr[0] = 0.0f;
    if (saturated) {
        percentage = std::bit_cast<float>(0x42C80000u);
    } else if (ctx.fpr[0] < percentage) {
        ctx.fpr[0] = std::bit_cast<float>(0x461C4000u);
        percentage = percentage / ctx.fpr[0];
    }
    ctx.fpr[12] = percentage;
    call_set(rt, ctx, 0x08A87EECu, {slot});
    ctx.gpr[5] = word(rt, slot);
    if (target != slot) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
}

// 0x08A290B8 / 0x08A29190: for a count a1 > 0, asks the member *a2 (null message 0x08CAB54C) to write a string into the slot at sp + 8
// (virtual +0xC) and tests it with 0x08B1D7BC; when accepted, `parse` (a2 = a3 = 0) turns it into a pair that 0x08A83160 stores
// into the slot at sp + 4, copied into a0 when it is not the slot itself (0x08A5BAD8) and released (0x08A5B9EC). Either way the
// string loses one reference (0x08B60804 / 0x08B8542C unless it is the shared empty representation).
void parse_string_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 parse) {
    const u32 target = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 text_slot = frame.at(8u);
    const u32 result_slot = frame.at(4u);
    if (static_cast<s32>(count) <= 0) return;
    const u32 member = word(rt, holder);
    if (member == 0u) {
        ctx.gpr[4] = 0x08CAB54Cu;
        call_here(rt, ctx, 0x08A5BB7Cu);
    }
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {text_slot, member});
    ctx.gpr[4] = word(rt, text_slot);
    const u32 accepted = call_set(rt, ctx, 0x08B1D7BCu, {ctx.gpr[4]});
    ctx.gpr[4] = word(rt, text_slot);
    if (accepted != 0u) {
        ctx.gpr[6] = 0u;
        ctx.gpr[7] = 0u;
        call_here(rt, ctx, parse);
        call_set(rt, ctx, 0x08A83160u, {result_slot, member, ctx.gpr[2], ctx.gpr[3]});
        ctx.gpr[5] = word(rt, result_slot);
        if (target != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, result_slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
        ctx.gpr[4] = word(rt, text_slot);
    }
    const u32 data = ctx.gpr[4];
    const u32 representation = data - 0xCu;
    if (representation != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
    }
}

// 0x08BAACA0 / 0x08BAAEF0: copy-constructs a0[0..count) of 0xAC-byte elements from the template a2: the first 0xA4 bytes are copied
// (word by word, or byte by byte when either address is unaligned), the tag 0x08CC3528 goes to +0xA4 and the template's +0xA8 to
// +0xA8. A null element address is skipped. v0 / v1 end as the last copy left them.
void construct_element_array(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    if (count == 0u) return;
    u32 result = 0x08CC0000u;
    u32 high = ctx.gpr[3];
    u32 element = first;
    for (u32 index = 0u; index != count; ++index, element += 0xACu) {
        if (element == 0u) continue;
        const bool unaligned = ((source | element) & 3u) != 0u;
        for (u32 offset = 0u; offset <= 0xA0u; offset += 4u) {
            const u32 value = word(rt, source + offset);
            if (unaligned) {
                for (u32 byte = 0u; byte < 4u; ++byte) rt.memory().store8(element + offset + byte, static_cast<std::uint8_t>(value >> (8u * byte)));
            } else {
                put(rt, element + offset, value);
            }
        }
        high = word(rt, source + 0x94u);
        result = word(rt, source + 0xA8u);
        put(rt, element + 0xA4u, 0x08CC3528u);
        put(rt, element + 0xA8u, result);
    }
    ctx.gpr[2] = result;
    ctx.gpr[3] = high;
}

// 0x088249F8: for a count a1 >= 2, builds an object into the slot at sp (0x088248E4, null message 0x08C99B4C), keeps its field +0x14,
// releases the slot, asks the members *a3 (virtual +0x3C) and *(a3 + 4) (virtual +0x50), both with null message 0x08C99B24, and calls
// 0x0885D29C with the field, both values and the second call's v1.
void apply_member_values_with_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 slot = frame.at(0u);
    ctx.gpr[6] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[4] = slot;
    if (static_cast<s32>(count) < 2) return;
    call_here(rt, ctx, 0x088248E4u);
    const u32 object = word(rt, slot);
    ctx.gpr[4] = 0x08C99B4Cu;
    if (object == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[4] = slot;
    const u32 field = word(rt, object + 0x14u);
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 first = word(rt, holder);
    ctx.gpr[4] = 0x08C99B24u;
    if (first == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[3] = word(rt, first);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 first_value = call_set(rt, ctx, ctx.gpr[2], {first});
    const u32 second = word(rt, holder + 4u);
    ctx.gpr[4] = 0x08C99B24u;
    if (second == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[3] = word(rt, second);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x50u);
    const u32 second_value = call_set(rt, ctx, ctx.gpr[2], {second});
    call_set(rt, ctx, 0x0885D29Cu, {field, first_value, second_value, ctx.gpr[3]});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08A27538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_member_percentage(rt, ctx); }
void func_08A290B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { parse_string_member(rt, ctx, 0x08B1DA9Cu); }
void func_08B94000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_unique(rt, ctx, 0x08B93F38u, true); }
void func_08BF98E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08BF981Cu); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08BAACA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_element_array(rt, ctx); }
void func_08BECB24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[18] = G[4]; G[4] = G[8]; G[16] = G[8]; G[17] = G[6]; call_here(rt, ctx, 0x08AA2A30u); G[4] = frame.at(0u); G[5] = G[16] + 4u; G[19] = G[2]; call_here(rt, ctx, 0x089761DCu); G[5] = word(rt, G[18] + 0x18u); G[17] = word(rt, G[17]); G[6] = word(rt, G[18] + 0x14u); G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[17] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[18] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[18] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[19]; G[6] = frame.at(0u); call_here(rt, ctx, G[2]); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } }
void func_08C267E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); G[17] = G[5]; G[16] = G[4]; G[7] = word(rt, G[4] + 0x18u); G[6] = word(rt, G[6]); G[5] = word(rt, G[16] + 0x14u); G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[7]) >> 1); G[3] = G[7] & 1u; G[2] = G[5]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } call_here(rt, ctx, G[2]); G[4] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[5] = G[4] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[5]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[5] = G[3] + G[4]; } G[4] = frame.at(0u); G[16] = frame.at(4u); call_here(rt, ctx, G[2]); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C25D4Cu); G[4] = G[17]; if (G[17] != G[16]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A27610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_member_percentage(rt, ctx); }
void func_08A276E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_member_percentage(rt, ctx); }
void func_08A29190(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { parse_string_member(rt, ctx, 0x08B1D9C8u); }
void func_08B94DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08B94D30u); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C1EE14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_unique(rt, ctx, 0x08C1ED4Cu, true); }
void func_08BAAEF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_element_array(rt, ctx); }
void func_08BFE9FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08BFE92Cu); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08BFEAD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08BFE808u); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C01474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08C013ACu); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C1D458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08C1D2DCu); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C25D8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_unique(rt, ctx, 0x08C25B2Cu, false); }
void func_08C2BC8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08C2BB8Cu); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C35350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[20] = G[5]; G[17] = G[4]; G[3] = word(rt, G[4] + 8u); G[16] = G[4] + 4u; if (G[3] != 0u) { G[5] = word(rt, G[5]); for (;;) { G[2] = word(rt, G[3] + 0x10u); G[4] = G[5] < G[2] ? 1u : 0u; G[2] = G[4] == 0u ? word(rt, G[3] + 0xCu) : word(rt, G[3] + 8u); if (G[2] == 0u) { G[16] = G[3]; break; } G[3] = G[2]; } G[5] = G[3]; if (G[4] == 0u) goto cmp; } G[2] = word(rt, G[17] + 0xCu); G[4] = G[17]; if (G[16] == G[2]) goto insert; G[4] = G[16]; call_here(rt, ctx, 0x08B5F174u); G[5] = G[2]; cmp: G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[20]); G[2] = G[2] < G[3] ? 1u : 0u; G[4] = 0u; if (G[2] == 0u) goto done; G[4] = G[17]; insert: G[5] = 0u; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08C35280u); G[5] = G[2]; G[4] = 1u; done: store8(frame.at(4u), G[4]); G[2] = G[5]; G[19] = word(rt, frame.at(4u)); G[3] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08C29CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); G[17] = G[5]; G[16] = G[4]; G[7] = word(rt, G[4] + 0x18u); G[6] = word(rt, G[6]); G[5] = word(rt, G[16] + 0x14u); G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[7]) >> 1); G[3] = G[7] & 1u; G[2] = G[5]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } call_here(rt, ctx, G[2]); G[4] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[5] = G[4] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[5]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[5] = G[3] + G[4]; } G[4] = frame.at(0u); G[16] = frame.at(4u); call_here(rt, ctx, G[2]); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08BF7C60u); G[4] = G[17]; if (G[17] != G[16]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C10B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); G[18] = G[4]; G[4] = G[8]; G[16] = G[8]; G[17] = G[6]; call_here(rt, ctx, 0x08AA2A30u); G[4] = frame.at(0u); G[5] = G[16] + 4u; G[19] = G[2]; call_here(rt, ctx, 0x08A03ED8u); G[5] = word(rt, G[18] + 0x18u); G[17] = word(rt, G[17]); G[6] = word(rt, G[18] + 0x14u); G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[17] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[18] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[18] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[19]; G[6] = frame.at(0u); call_here(rt, ctx, G[2]); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); } }
void func_088249F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_values_with_tail(rt, ctx); }
void func_08826628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[16]); G[17] = frame.at(4u); G[19] = G[4]; G[4] = G[17]; G[18] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C99B4Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[17]; G[17] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); G[4] = 0x08C99B24u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x0885C188u); G[5] = G[2]; G[4] = frame.at(0u); G[6] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08857EE4u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08833D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); G[18] = frame.at(4u); G[19] = G[4]; G[4] = G[18]; G[16] = G[5]; call_here(rt, ctx, 0x08A2DA98u); G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9B464u; G[17] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A2D1B8u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088D9F2Cu); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); if (G[16] != 0u) { G[4] = G[19]; if (G[19] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_0885D3A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = G[4] + 0x128u; G[17] = G[4]; if (G[2] != 0u) { G[16] = G[4] + 0x188u; if (G[2] != G[16]) { G[18] = 2u; G[16] -= 0x30u; for (;;) { G[18] -= 1u; G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); if (G[18] == 0u) break; G[16] -= 0x30u; } } } G[19] = G[17] + 0xB8u; G[4] = G[17] + 0x120u; call_here(rt, ctx, 0x088A7548u); G[2] = G[17] + 0x58u; if (G[19] != 0u) { G[16] = G[17] + 0x118u; if (G[19] != G[16]) { G[18] = 2u; G[16] -= 0x30u; for (;;) { G[18] -= 1u; G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); if (G[18] == 0u) break; G[16] -= 0x30u; } } G[2] = G[17] + 0x58u; } G[4] = G[17] + 0x50u; if (G[2] != 0u) { G[16] = G[19]; if (G[2] != G[19]) { G[18] = 2u; G[16] -= 0x30u; do { G[18] -= 1u; G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); G[16] -= 0x30u; } while (G[18] != 0u); G[4] = G[17] + 0x50u; } } G[17] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x088A7548u); }
void func_08876EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[18] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[16] = 0x08CA0000u; if (static_cast<std::int32_t>(G[2]) > 0) { G[5] = 0x08CA0070u; G[4] = G[17]; G[6] = 0x08CA0074u; call_here(rt, ctx, 0x088756F0u); G[5] = 0x08CA0074u; if (G[2] != 0u) { G[4] = G[17]; call_here(rt, ctx, 0x088757B0u); G[16] = G[2]; if (G[2] != 0u) { G[4] = G[18]; G[5] = 0x08CA007Cu; call_here(rt, ctx, 0x08B7B8C4u); G[5] = 4u; if (G[2] != 0u) G[5] = 0u; G[4] = G[16]; call_here(rt, ctx, 0x088723DCu); } } } G[4] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); } }
void func_088784F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[31]); put_float(rt, frame.at(0x30u), ctx.fpr[20]); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BF4u); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71EC8u); G[4] = G[16]; ctx.fpr[20] = ctx.fpr[0]; call_here(rt, ctx, 0x08AB856Cu); G[6] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[5] = 0x08CA0000u; if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = G[17]; G[5] = 0x08CA0088u; call_here(rt, ctx, 0x088756F0u); G[4] = G[2]; if (G[2] != 0u) { G[5] = 2u; call_here(rt, ctx, 0x0886A9A0u); ctx.fpr[12] = ctx.fpr[20]; if (G[2] != 0u) { G[4] = G[2]; call_here(rt, ctx, 0x08881D04u); } } } G[4] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); } ctx.fpr[20] = f20; }
void func_08878E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[17]); G[16] = frame.at(4u); G[18] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[16]; G[17] = G[2]; call_here(rt, ctx, 0x08AB856Cu); G[16] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[4] = word(rt, frame.at(4u)); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = G[17]; call_here(rt, ctx, 0x08A517A4u); G[17] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = G[18]; G[6] = G[16]; G[5] = 0u; call_here(rt, ctx, 0x088756F0u); G[4] = G[2]; if (G[2] != 0u) { G[5] = 5u; call_here(rt, ctx, 0x0886A9A0u); G[4] = G[2]; if (G[2] != 0u) { G[5] = G[17]; call_here(rt, ctx, 0x0886F8ACu); } } } G[4] = word(rt, frame.at(4u)); } G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[16] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); } }
void func_088794DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BFCu); G[19] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[4] = word(rt, frame.at(4u)); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[17]); call_here(rt, ctx, 0x0886B2E0u); G[18] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { G[16] = 0u; G[4] = word(rt, G[17]); for (;;) { G[5] = G[16]; G[16] += 1u; call_here(rt, ctx, 0x0886BC5Cu); G[5] = G[2]; G[4] = G[17]; G[6] = G[19]; call_here(rt, ctx, 0x088758FCu); if (G[18] == G[16]) break; G[4] = word(rt, G[17]); } } G[4] = word(rt, frame.at(4u)); } G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); } }
void func_08879720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[31]); G[16] = frame.at(0x10u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[6] = G[16]; G[4] = G[17]; G[5] = frame.at(4u); G[7] = 3u; call_here(rt, ctx, 0x088755E4u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[6] = G[2]; G[2] = word(rt, frame.at(0x18u)); G[2] = static_cast<std::int32_t>(G[2]) < 3 ? 1u : 0u; G[4] = word(rt, frame.at(0x10u)); if (G[2] == 0u) { G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x088756F0u); G[4] = G[2]; if (G[2] != 0u) { G[5] = 4u; call_here(rt, ctx, 0x0886A9A0u); G[4] = G[2]; call_here(rt, ctx, 0x08B93734u); ctx.fpr[12] = float_at(rt, frame.at(4u)); if (G[2] != 0u) { ctx.fpr[13] = float_at(rt, frame.at(8u)); ctx.fpr[14] = float_at(rt, frame.at(0xCu)); G[4] = G[2]; call_here(rt, ctx, 0x0887051Cu); } } G[4] = word(rt, frame.at(0x10u)); } G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); } }
void func_08898570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[19] = G[5]; G[18] = G[7]; G[16] = G[4]; call_here(rt, ctx, 0x088F8D58u); G[4] = G[2] + 0x20u; G[5] = G[18]; call_here(rt, ctx, 0x08897470u); G[3] = word(rt, G[16] + 0x40u); if (G[3] != 0u) { G[4] = G[18]; if (G[18] != 0u) { call_here(rt, ctx, 0x088977D4u); G[3] = 1u; G[4] = G[16]; G[5] = G[19]; G[6] = frame.at(0u); if (G[2] == G[3]) { call_here(rt, ctx, 0x0889854Cu); G[16] = word(rt, G[16] + 0x40u); G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x088970E0u); G[6] = G[2]; G[4] = G[16]; G[5] = G[19]; G[7] = G[17]; call_here(rt, ctx, 0x088984E4u); } else { G[2] = word(rt, G[16] + 0x40u); put(rt, frame.at(0u), 0u); G[5] = G[19]; put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); G[3] = word(rt, G[2]); G[7] = word(rt, G[3] + 0x14u); G[4] = G[2]; call_here(rt, ctx, G[7]); } } } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_088F8E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); G[2] = 0x7FFF0000u; G[21] = 0u; G[20] = 4u; G[19] = 0x7FFFFFFFu; G[18] = G[4] + 0x2Cu; G[17] = G[4]; G[16] = 0u; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[16]) >> 5); for (;;) { G[2] <<= 2; G[2] += G[18]; G[5] = G[16]; G[4] = G[17]; bool skip = false; if (static_cast<std::int32_t>(G[16]) >= 0) { G[2] = word(rt, G[2]); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> (G[16] & 31u)); G[2] &= 1u; if (G[2] != 0u) skip = true; } if (!skip) { call_here(rt, ctx, 0x088F8DB4u); if (G[2] != 0u) { G[4] = G[2]; G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x2Cu); call_here(rt, ctx, G[2]); if (static_cast<std::int32_t>(G[2]) >= 0) { G[3] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; if (G[3] != 0u) { G[19] = G[2]; G[21] = 1u; } } } } G[16] += 1u; if (G[16] == G[20]) break; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[16]) >> 5); } if (G[21] == 0u) G[19] = 0u; G[2] = G[19]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
void func_08907D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[19] = G[5]; G[17] = G[4]; ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, 0x08907D64u); G[16] = G[2]; G[18] = word(rt, G[17] + 0x30u); G[2] <<= 4; G[3] = G[16] << 6; G[3] -= G[2]; G[3] += G[18]; ctx.fpr[0] = float_at(rt, G[3] + 0x10u); const bool lt = ctx.fpr[20] < ctx.fpr[0]; G[2] = 0xFFFFFFFFu; if (!lt) { G[5] = G[19]; G[4] = G[17]; call_here(rt, ctx, 0x08907D5Cu); G[5] = G[2]; G[2] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[5]) > 0) { G[6] = 0u; G[3] = G[5] + G[6]; do { G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[2] = G[16] + G[4]; G[3] = G[2] << 6; G[2] <<= 4; G[3] -= G[2]; G[3] += G[18]; ctx.fpr[0] = float_at(rt, G[3] + 0x10u); if (!(ctx.fpr[20] < ctx.fpr[0])) G[6] = G[4] + 1u; else G[5] = G[4]; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; G[3] = G[5] + G[6]; } while (G[2] != 0u); G[2] = G[6] - 1u; } } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } ctx.fpr[20] = f20; }
void func_08938188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[21] = G[5]; G[19] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B35F60u); G[23] = word(rt, G[19] + 4u); G[22] = word(rt, G[23] + 4u); G[20] = G[2]; if (static_cast<std::int32_t>(G[22]) <= 0) { G[2] = 0xFFFFFFFFu; } else { G[17] = G[22]; G[18] = 0xFFFFFFFFu; G[2] = G[18] + G[17]; for (;;) { G[16] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[4] = G[19]; G[5] = G[21]; G[6] = G[16]; G[7] = G[20]; call_here(rt, ctx, 0x08938088u); if (G[2] == 0u) { if (static_cast<std::int32_t>(G[16]) < 0) { G[2] = 0xFFFFFFFFu; break; } G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[22]) ? 1u : 0u; if (G[2] == 0u) { G[2] = 0xFFFFFFFFu; break; } G[2] = G[16] << 3; G[2] += G[23]; G[2] = word(rt, G[2] + 0x14u); break; } if (static_cast<std::int32_t>(G[2]) <= 0) G[17] = G[16]; else G[18] = G[16]; G[2] = G[18] + 1u; if (G[17] == G[2]) { G[2] = 0xFFFFFFFFu; break; } G[2] = G[18] + G[17]; } } G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
void func_08939AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 4u); G[6] = G[5] + 0x20u; put(rt, G[4] + 8u, G[6]); G[3] = word(rt, G[5] + 8u); { const bool one = G[3] == 1u; G[2] = word(rt, G[5] + 0x14u); G[2] = one ? (G[2] << 3) : (G[2] << 4); G[2] += G[6]; put(rt, G[4] + 0xCu, G[2]); } G[3] = word(rt, G[5] + 8u); { const bool one = G[3] == 1u; G[2] = word(rt, G[5] + 0x14u); G[2] = one ? (G[2] << 3) : (G[2] << 4); G[2] += G[6]; G[2] += 8u; put(rt, G[4] + 0x10u, G[2]); } G[3] = word(rt, G[5] + 8u); { const bool one = G[3] == 1u; G[2] = word(rt, G[5] + 0x14u); G[2] = one ? (G[2] << 3) : (G[2] << 4); G[2] += G[6]; G[2] += 0x208u; put(rt, G[4] + 0x14u, G[2]); } G[3] = word(rt, G[5] + 8u); { const bool one = G[3] == 1u; G[2] = word(rt, G[5] + 0x14u); G[2] = one ? (G[2] << 3) : (G[2] << 4); G[3] = G[6] + G[2]; G[2] = word(rt, G[3]); G[2] = G[3] + G[2]; put(rt, G[4] + 0x18u, G[2]); } }
void func_0893C6A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[18] = G[4]; G[4] = G[5]; G[17] = G[6]; G[16] = G[7]; call_here(rt, ctx, 0x0893C54Cu); ctx.fpr[20] = ctx.fpr[0]; G[4] = G[16]; call_here(rt, ctx, 0x0893C54Cu); ctx.fpr[2] = ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[20] - ctx.fpr[0]; G[1] = 0x40490FDBu; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (ctx.fpr[0] <= ctx.fpr[1]) { G[1] = 0x40C90FDBu; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[0]; } else { G[1] = 0xC0490FDBu; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (ctx.fpr[1] < ctx.fpr[0]) { G[1] = 0x40C90FDBu; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; } } ctx.fpr[0] = float_at(rt, G[18]); ctx.fpr[1] = float_at(rt, G[17]); G[4] = frame.at(0u); ctx.fpr[0] = psp_mul(ctx.fpr[20], ctx.fpr[0]); ctx.fpr[1] = psp_mul(ctx.fpr[2], ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; put_float(rt, frame.at(0u), ctx.fpr[0]); call_here(rt, ctx, 0x0893C54Cu); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } ctx.fpr[20] = f20; }
void func_0897F474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = frame.at(8u); G[4] = G[19]; G[18] = frame.at(4u); G[16] = G[7]; call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(8u)); G[4] = 0x08CA3400u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CA3390u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x0897F34Cu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089C121C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = frame.at(8u); G[4] = G[19]; G[18] = frame.at(4u); G[16] = G[7]; call_here(rt, ctx, 0x089BEE4Cu); G[17] = word(rt, frame.at(8u)); G[4] = 0x08CA5EB4u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CA5E6Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17] + 0x230u; G[5] = G[18]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089AEB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); G[2] = 1u; G[17] = frame.at(4u); G[4] = G[17]; G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x089AB370u); G[16] = word(rt, G[16]); G[4] = 0x08CA50BCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = G[16]; G[3] = word(rt, G[2] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[3]); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA50C4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = word(rt, frame.at(8u)); G[2] = word(rt, G[3] + 0x188u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089BDE34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); G[2] = 1u; G[18] = frame.at(8u); G[4] = G[18]; G[20] = frame.at(4u); G[17] = G[7]; G[16] = G[5]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A5D438u); G[4] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x089BB3A8u); G[19] = word(rt, frame.at(4u)); G[4] = 0x08CA5CB0u; if (G[19] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[17]); G[4] = 0x08CA5CA8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0x5Cu); G[4] = frame.at(0u); call_here(rt, ctx, G[2]); G[5] = G[18]; G[6] = frame.at(0u); G[4] = G[19]; call_here(rt, ctx, 0x089BD8ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); } }
void func_0894D114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[6] = 1u; G[17] = G[4]; G[16] = G[5]; G[5] = 0xFFFFFFFFu; if (G[4] != 0u) { call_here(rt, ctx, 0x0894D048u); G[5] = G[2]; G[4] = G[16]; G[6] = 1u; call_here(rt, ctx, 0x089466A0u); } else { G[4] = G[16]; call_here(rt, ctx, 0x089466A0u); } G[4] = G[17]; call_here(rt, ctx, 0x0895FE28u); call_here(rt, ctx, 0x089594D8u); call_here(rt, ctx, 0x0894E678u); G[5] = 0x08CDE700u; G[4] = 0x08CF35F0u; G[6] = 4u; G[7] = 2u; call_here(rt, ctx, 0x0895EEF8u); call_here(rt, ctx, 0x08958354u); call_here(rt, ctx, 0x08954898u); call_here(rt, ctx, 0x089513D8u); call_here(rt, ctx, 0x0895871Cu); call_here(rt, ctx, 0x0895B454u); call_here(rt, ctx, 0x089455E0u); call_here(rt, ctx, 0x08954F2Cu); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08951F24u); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x0893BA0Cu); }
void func_089AA9BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); put_float(rt, frame.at(0x14u), ctx.fpr[21]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); G[17] = 0x08CF0000u; G[16] = G[4]; G[4] = 0x08CF4A2Cu; ctx.fpr[21] = ctx.fpr[13]; ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, 0x08B344E0u); G[2] = load8(G[16] + 0xF8u); if (G[2] == 0u) put_float(rt, G[16] + 0xE4u, ctx.fpr[20]); G[2] = load8(G[16] + 0xF9u); if (G[2] == 0u) put_float(rt, G[16] + 0xE8u, ctx.fpr[21]); G[4] = G[16]; G[5] = G[16] + 0xE4u; G[6] = G[16] + 0xE8u; call_here(rt, ctx, 0x089A8A78u); G[2] = load8(G[16] + 0xF8u); if (G[2] == 0u) { ctx.fpr[0] = float_at(rt, G[16] + 0xE4u); G[2] = G[16] + 0x74u; put(rt, G[16] + 0x74u, 0u); put_float(rt, G[2] + 8u, ctx.fpr[0]); put(rt, G[2] + 4u, 0u); } G[2] = load8(G[16] + 0xF9u); if (G[2] == 0u) { ctx.fpr[0] = float_at(rt, G[16] + 0xE8u); G[2] = G[16] + 0x80u; put(rt, G[16] + 0x80u, 0u); put_float(rt, G[2] + 8u, ctx.fpr[0]); put(rt, G[2] + 4u, 0u); } G[2] = load8(G[16] + 0xF8u); G[3] = 1u; if (G[2] == 0u) { G[2] = load8(G[16] + 0xF9u); G[3] = G[2] != 0u ? 1u : 0u; } store8(G[16] + 0xFAu, G[3]); G[4] = 0x08CF4A2Cu; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } ctx.fpr[20] = f20; ctx.fpr[21] = f21; jump(ctx, 0x08B3457Cu); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_219
} // namespace gt::game

#define HOT219(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_219::name, gt::ReturnKind::kind)
HOT219(0x08A27538u, func_08A27538, Word);
HOT219(0x08A290B8u, func_08A290B8, Word);
HOT219(0x08B94000u, func_08B94000, DoubleWord);
HOT219(0x08BF98E4u, func_08BF98E4, DoubleWord);
HOT219(0x08BAACA0u, func_08BAACA0, Word);
// HOT219(0x08BECB24u, func_08BECB24, Word);  // no comparable synthetic case yet
HOT219(0x08C267E8u, func_08C267E8, Word);
HOT219(0x08A27610u, func_08A27610, Word);
HOT219(0x08A276E8u, func_08A276E8, Word);
HOT219(0x08A29190u, func_08A29190, Word);
// HOT219(0x08B94DF8u, func_08B94DF8, DoubleWord);  // no comparable synthetic case yet
HOT219(0x08C1EE14u, func_08C1EE14, DoubleWord);
HOT219(0x08BAAEF0u, func_08BAAEF0, Word);
// HOT219(0x08BFE9FCu, func_08BFE9FC, DoubleWord);  // no comparable synthetic case yet
// HOT219(0x08BFEAD4u, func_08BFEAD4, DoubleWord);  // no comparable synthetic case yet
HOT219(0x08C01474u, func_08C01474, DoubleWord);
// HOT219(0x08C1D458u, func_08C1D458, DoubleWord);  // no comparable synthetic case yet
HOT219(0x08C25D8Cu, func_08C25D8C, DoubleWord);
// HOT219(0x08C2BC8Cu, func_08C2BC8C, DoubleWord);  // no comparable synthetic case yet
// HOT219(0x08C35350u, func_08C35350, DoubleWord);  // no comparable synthetic case yet
HOT219(0x08C29CA4u, func_08C29CA4, Word);
// HOT219(0x08C10B60u, func_08C10B60, Word);  // no comparable synthetic case yet
HOT219(0x088249F8u, func_088249F8, Word);
HOT219(0x08826628u, func_08826628, Word);
// HOT219(0x08833D44u, func_08833D44, Word);  // no comparable synthetic case yet
// HOT219(0x0885D3A0u, func_0885D3A0, Word);  // no comparable synthetic case yet
HOT219(0x08876EC0u, func_08876EC0, Word);
HOT219(0x088784F0u, func_088784F0, Word);
HOT219(0x08878E14u, func_08878E14, Word);
HOT219(0x088794DCu, func_088794DC, Word);
HOT219(0x08879720u, func_08879720, Word);
// HOT219(0x08898570u, func_08898570, Word);  // no comparable synthetic case yet
HOT219(0x088F8E10u, func_088F8E10, Word);
HOT219(0x08907D6Cu, func_08907D6C, Word);
// HOT219(0x08938188u, func_08938188, Word);  // no comparable synthetic case yet
HOT219(0x08939AE8u, func_08939AE8, Word);
HOT219(0x0893C6A8u, func_0893C6A8, Float);
// HOT219(0x0897F474u, func_0897F474, Word);  // no comparable synthetic case yet
// HOT219(0x089C121Cu, func_089C121C, Word);  // no comparable synthetic case yet
HOT219(0x089AEB0Cu, func_089AEB0C, Word);
HOT219(0x089BDE34u, func_089BDE34, Word);
HOT219(0x0894D114u, func_0894D114, Word);
HOT219(0x089AA9BCu, func_089AA9BC, Word);
