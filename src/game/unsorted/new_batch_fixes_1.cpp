// Fixes 1: functions an earlier draft got wrong (synthetic-gate mismatches), rewritten by hand as structured C from
// `tools.disasm`. v0 is left as the original leaves it on every path. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_fixes_1 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values, so a callee that never writes v0 hands
// the value the caller had on to the code after the call.
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
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_full(rt, ctx, target, arguments, floats).f0;
}

// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v0, (object); lw a1, slot(v0); jalr a1`: at the jalr v0 holds the vtable and a1 the target.
GuestResult call_virtual_in_a1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                               std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[5] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
    return (target & ~mask) | ((value << position) & mask);
}

u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }

// 0x0885BCA8 (a0 = a holder, a1 = an index; returns a doubleword): the pair of words at *a0 + 0x38 + 8 * index.
void func_0885BCA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 entry = word(rt, ctx.gpr[4]) + ctx.gpr[5] * 8u;
    ctx.gpr[2] = word(rt, entry + 0x38u);
    ctx.gpr[3] = word(rt, entry + 0x3Cu);
}

// 0x0885F550 (a0 = a holder): clears two fields of the object at *a0 (running 0x0885F46C first when the byte at +0x3FDB is
// set); when the byte at +0x26 of the object at +0x5AC is zero the function ends (v0 = 0). Otherwise each of the eight 0x18-byte
// slots at +0x3E90 counts into one of three halfword counters, depending on its signed byte at +8 and the answer of 0x088DB118;
// the tail call is 0x088D4D24 with the word at +0x7C of the object at +0x3F50.
void func_0885F550(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 next_a0 = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        if (byte_at(rt, word(rt, self) + 0x3FDBu) != 0u) call(rt, ctx, 0x0885F46Cu, {self});
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x34u, 0u);
        put(rt, word(rt, self) + 0x3FD4u, 0u);
        const u32 scope = word(rt, word(rt, self) + 0x5ACu);
        const u32 enabled = byte_at(rt, scope + 0x26u);
        if (enabled == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        for (u32 index = 0u; index != 8u; ++index) {
            const u32 slot = word(rt, self) + 0x3E90u + index * 0x18u;
            const u32 kind = signed_byte(byte_at(rt, slot + 8u));
            put_byte(rt, slot + 9u, 0xFFu);
            if (kind == 0xFFFFFFFFu) {
                put_half(rt, slot + 0xAu, signed_half(half_at(rt, slot + 0xAu)) + 1u);
            } else {
                put_half(rt, slot + 0x12u, half_at(rt, slot + 0x12u) + 1u);
                const u32 answer = signed_byte(call(rt, ctx, 0x088DB118u, {scope, kind}));
                put_byte(rt, slot + 9u, answer);
                if (answer == 0u) put_half(rt, slot + 0x10u, half_at(rt, slot + 0x10u) + 1u);
            }
        }
        next_a0 = word(rt, word(rt, word(rt, self) + 0x3F50u) + 0x7Cu);
    }
    tail_call(ctx, 0x088D4D24u, {next_a0});
}

// 0x0886C31C (a0 = a holder, a1 = a key): looks the key up in the tree at *(a0 + 4) + 0x1DC (0x08B93AE8); when it is absent
// or the key is below the found entry's key (word +0x10), 0x08B940D8 inserts it; v0 = the byte at +0x14 of the entry.
void func_0886C31C(Runtime &rt, AllegrexContext &ctx) {
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0x10), key);
    const u32 owner = word(rt, ctx.gpr[4] + 4u);
    const u32 tree = owner + 0x1DCu;
    u32 entry = call(rt, ctx, 0x08B93AE8u, {tree, frame.at(0x10)});
    const bool insert = owner + 0x1E0u == entry || static_cast<s32>(word(rt, frame.at(0x10))) < static_cast<s32>(word(rt, entry + 0x10u));
    if (insert) {
        put(rt, frame.at(0), word(rt, frame.at(0x10)));
        put_byte(rt, frame.at(4), 0u);
        entry = call(rt, ctx, 0x08B940D8u, {tree, entry, frame.at(0), entry});
    }
    ctx.gpr[2] = byte_at(rt, entry + 0x14u);
}

// 0x08872E18 (a0 = an object, a1 = a source): clears bit 0 of the flags (+0x100) of the member at +0x5C, scales the source's
// float answer (vtable slot 0x1C) by 0x0886B0F8(+8) and by 60, and passes it on; when 0x0886B444(+8, 3) is zero and
// 0x0886A9A0(+4, 5) finds an object, that object's data is applied and bit 0 is set again. Tail call 0x0890AEBC(*(+0x5C) + 8)
// with the scale in f12.
void func_08872E18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 block = self + 0xCu;
    float scale = 0.0f;
    u32 next_a0 = 0u;
    u32 tail_a1 = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        const u32 member = word(rt, self + 0x5Cu);
        put(rt, member + 0x100u, insert_bits(word(rt, member + 0x100u), 0u, 0u, 1u));
        scale = call_virtual(rt, ctx, source, 0x1Cu, {source}).f0;
        scale = scale * call_full(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)}).f0;
        call(rt, ctx, 0x088728C0u, {word(rt, self + 0x5Cu), block});
        call_full(rt, ctx, 0x08872054u, {word(rt, self + 0x5Cu)}, {scale * 60.0f});
        u32 spare = 5u;  // a1 as the callees leave it: it reaches the tail call
        if (call(rt, ctx, 0x0886B444u, {word(rt, self + 8u), 3u}) == 0u) {
            GuestResult last = call_full(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 5u});
            const u32 found = last.v0;
            spare = last.a1;
            if (found != 0u) {
                spare = call_full(rt, ctx, 0x0886F8CCu, {found, spare}).a1;
                spare = call_full(rt, ctx, 0x0886F8E4u, {found, spare}).a1;
                const u32 target = word(rt, self + 0x5Cu);
                last = call_full(rt, ctx, 0x0886F8D4u, {found, spare});
                spare = call_full(rt, ctx, 0x08872AC8u, {target, block, last.v0}).a1;
                const u32 flags = word(rt, word(rt, self + 0x5Cu) + 0x100u) | 1u;
                put(rt, word(rt, self + 0x5Cu) + 0x100u, flags);
                ctx.gpr[2] = flags;
            }
        }
        tail_a1 = spare;
        next_a0 = word(rt, self + 0x5Cu) + 8u;
    }
    ctx.fpr[12] = scale;
    tail_call(ctx, 0x0890AEBCu, {next_a0, tail_a1});
}

// 0x088733F4 (a0 = an object): resets the vtable words (+0 and +0xC), releases the member at +0x5C + 4 through its virtual
// slot 0, clears both members, then 0x08AF484C(+0xC) and the final vtable word 0x08CBF428 (also v0).
void func_088733F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 0xCu, 0x08CBFA2Cu);
    put(rt, self, 0x08CBFA00u);
    const u32 member = word(rt, self + 0x5Cu + 4u);
    if (member != 0u) call_virtual(rt, ctx, member, 0u, {member, word(rt, self + 0x5Cu)});
    put(rt, self + 0x5Cu + 4u, 0u);
    put(rt, self + 0x5Cu, 0u);
    put(rt, self + 0xCu, 0x08CD7310u);
    call(rt, ctx, 0x08AF484Cu, {self + 0xCu});
    put(rt, self, 0x08CBF428u);
    ctx.gpr[2] = 0x08CBF428u;
}

// 0x08877CCC (a0 = an object): builds a value on the frame (0x08AB85F0 / 0x08AB856C), hands it to 0x08875584, hands the answer of 0x08875584
// and the one of 0x0886AFA8 to 0x0887CB3C, then, unless the frame's first word minus 0xC is the sentinel 0x08D75298,
// 0x08B60804 decides whether 0x08B93554 runs; v0 is the sentinel when the check is skipped.
void func_08877CCC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 entry_a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 scratch = frame.at(4);
    const GuestResult filled = call_full(rt, ctx, 0x08AB85F0u, {scratch, entry_a1});
    const u32 built = call(rt, ctx, 0x08AB856Cu, {scratch, filled.a1});
    const u32 registered = call(rt, ctx, 0x08875584u, {self, built});
    const u32 answer = call(rt, ctx, 0x0886AFA8u, {word(rt, self)});
    call(rt, ctx, 0x0887CB3Cu, {answer + 4u, registered});
    const u32 node = word(rt, scratch) - 0xCu;
    if (node == 0x08D75298u) {
        ctx.gpr[2] = node;
        return;
    }
    const s32 order = static_cast<s32>(call(rt, ctx, 0x08B60804u, {word(rt, scratch) - 4u, 0xFFFFFFFFu}));
    if (order <= 0) call(rt, ctx, 0x08B93554u, {node, frame.at(0)});
}

// 0x0888EC10 (a0 = a pair of output words): allocates and initialises a 0x1D8-byte object (output word 0), and hands out
// the shared singleton at 0x08D75E90, built once behind the guard byte at 0x08D75E88 (output word 1). v0 is 0x08D70000 when the
// singleton already exists, else the guard's last call.
void func_0888EC10(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call(rt, ctx, 0x08B62344u, {0x1D8u});
    call(rt, ctx, 0x088D65DCu, {object + 0x188u});
    call(rt, ctx, 0x08894444u, {object + 0x190u});
    call(rt, ctx, 0x08893F7Cu, {object + 0x198u});
    put(rt, object, 0u);
    call(rt, ctx, 0x08B36018u, {object + 8u, 0u, 0x178u});
    put(rt, object + 0x180u, 0u);
    put(rt, object + 0x184u, 0u);
    for (u32 index = 0u; index != 4u; ++index) put(rt, object + 0x1A0u + index * 4u, 0u);
    for (u32 row = 0u; row != 4u; ++row) {
        for (u32 column = 0u; column != 2u; ++column) put(rt, object + 0x1B0u + row * 8u + column * 4u, 0u);
    }
    put(rt, object + 0x1D0u, 0xFFFFFFFFu);
    put_byte(rt, object + 0x1D4u, 0u);
    put(rt, out, object);
    constexpr u32 kGuard = 0x08D75E88u;
    constexpr u32 kSingleton = 0x08D75E90u;
    const bool build = byte_at(rt, kGuard) == 0u && call(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u;
    if (build) {
        put(rt, kSingleton, 0x08CC1D18u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    } else {
        ctx.gpr[2] = 0x08D70000u;
    }
    put(rt, out + 4u, kSingleton);
}

// 0x08925C2C (a0 = an object; returns a float): the record at word(+4) + 0x268 * word(+0xC) gives, through its word at +0x60,
// 0x08B21590(that + 4); the answer minus 0x80 is scaled by 1/128 (f0, and v0 = the integer).
void func_08925C2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 record = word(rt, self + 4u) + word(rt, self + 0xCu) * 0x268u;
    u32 level = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        level = call(rt, ctx, 0x08B21590u, {word(rt, record + 0x60u) + 4u}) - 0x80u;
    }
    ctx.gpr[2] = level;
    ctx.fpr[0] = static_cast<float>(static_cast<s32>(level)) * float_bits(0x3C000000u);
}

// Signed 32-bit x * 0x51EB851F, high word (the compiler's division by 100).
s32 high_word_of_hundredth(s32 value) { return static_cast<s32>((static_cast<std::int64_t>(value) * 0x51EB851FLL) >> 32); }

// 0x0892D3E0 (a1 = an object, a2 = four percentages as bytes at +3..+6): scales the halfwords at +0xB8 / +0xBA (unsigned) and
// +0x174 / +0x176 (signed) by the percentages (a division by 100 through the reciprocal multiply); v0 = the last quotient.
void func_0892D3E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[5];
    const u32 percent = ctx.gpr[6];
    const s32 scaled_ba = static_cast<s32>(half_at(rt, object + 0xBAu) * byte_at(rt, percent + 4u));
    const s32 scaled_b8 = static_cast<s32>(half_at(rt, object + 0xB8u) * byte_at(rt, percent + 3u));
    const s32 scaled_174 = static_cast<s32>(signed_half(half_at(rt, object + 0x174u)) * byte_at(rt, percent + 5u));
    const s32 scaled_176 = static_cast<s32>(signed_half(half_at(rt, object + 0x176u)) * byte_at(rt, percent + 6u));
    put_half(rt, object + 0xBAu, static_cast<u32>(high_word_of_hundredth(scaled_ba)) >> 5);
    put_half(rt, object + 0xB8u, static_cast<u32>(high_word_of_hundredth(scaled_b8)) >> 5);
    const s32 quotient_174 = (high_word_of_hundredth(scaled_174) >> 5) - (scaled_174 >> 31);
    put_half(rt, object + 0x174u, static_cast<u32>(quotient_174));
    const s32 quotient_176 = (high_word_of_hundredth(scaled_176) >> 5) - (scaled_176 >> 31);
    put_half(rt, object + 0x176u, static_cast<u32>(quotient_176));
    ctx.gpr[2] = static_cast<u32>(quotient_174);
}

// 0x08A3523C (a0 = an object, a1, a2, a3, t0 = a byte, f12 = a float): the member at +0x18 (asserted non-null, message
// 0x08CACEC8) runs 0x08A3A73C(member, a1, 0); then, with the member read again, the tail call 0x08A3A5BC(member, a2, a3,
// t0 & 0xFF) with f12 unchanged.
void func_08A3523C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8] & 0xFFu;
    const float amount = ctx.fpr[12];
    u32 member = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        const u32 first_member = word(rt, self + 0x18u);
        if (first_member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEC8u});
        call(rt, ctx, 0x08A3A73Cu, {first_member, first, 0u});
        member = word(rt, self + 0x18u);
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEC8u});
    }
    ctx.fpr[12] = amount;
    tail_call(ctx, 0x08A3A5BCu, {member, second, third, fourth});
}

// 0x08A5A28C (a1 = a holder, a2 = a pointer to an object): the byte at the holder's +0x1C member (else the +0x18 member's
// +0xC member, asserted, message 0x08CAFA9C) is set from the object's virtual slot 0x34 when the pointer is non-null.
void func_08A5A28C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[5];
    const u32 pointer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    u32 target = word(rt, holder + 0x1Cu);
    ctx.gpr[2] = target;
    if (target == 0u) {
        const u32 owner = word(rt, holder + 0x18u);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAFA9Cu});
        target = word(rt, owner + 0xCu);
    }
    const u32 object = word(rt, pointer);
    if (object != 0u) put_byte(rt, target, call_virtual(rt, ctx, object, 0x34u, {object}).v0);
}

// 0x08A8350C (a0 = an object, a1 = a result slot, a3 = a pointer to a pointer to a source): the source's virtual slot 0x40
// gives a 64-bit value (v1:v0) that is multiplied by the object's pair (+0x18, +0x1C); the two words of the product go to
// 0x08A83160 with a block on the frame, which is then copied over a1 (0x08A5BAD8) unless it is a1, and released (0x08A5B9EC).
void func_08A8350C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 source = word(rt, holder);
    const u32 low_factor = word(rt, self + 0x18u);
    const u32 high_factor = word(rt, self + 0x1Cu);
    if (source == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB168Cu});
    const GuestResult size = call_virtual(rt, ctx, source, 0x40u, {source});
    const std::uint64_t product = static_cast<std::uint64_t>(low_factor) * size.v0;
    const u32 product_low = static_cast<u32>(product);
    const u32 product_high = static_cast<u32>(product >> 32) + low_factor * size.v1 + size.v0 * high_factor;
    call(rt, ctx, 0x08A83160u, {frame.at(0), size.a1, product_low, product_high});
    if (slot != frame.at(0)) call(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
}

// 0x08AA9AC8 (a0 = an object, a1 = a second word): 0x08B85EB8 fills a block on the frame, 0x08C31E0C(a0 + 8, block) finds an
// entry; the block's first word minus 0xC is released (0x08B8542C) unless it is the sentinel 0x08D74FD0 or 0x08B60804 says it
// is still in use; v0 = the word at +0x14 of the entry, or 0 when the entry is the end marker a0 + 0xC.
void func_08AA9AC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = frame.at(4);
    call(rt, ctx, 0x08B85EB8u, {block, second, frame.at(1)});
    const u32 entry = call(rt, ctx, 0x08C31E0Cu, {self + 8u, block});
    const u32 node = word(rt, block) - 0xCu;
    if (node != 0x08D74FD0u) {
        const s32 order = static_cast<s32>(call(rt, ctx, 0x08B60804u, {word(rt, block) - 4u, 0xFFFFFFFFu}));
        if (order <= 0) call(rt, ctx, 0x08B8542Cu, {node, frame.at(0)});
    }
    ctx.gpr[2] = self + 0xCu == entry ? 0u : word(rt, entry + 0x14u);
}

// 0x08B01154 (a0 = a record, a1 = an object): follows the chain of 84-byte nodes at *(a1 + 0x60) starting from the record's
// signed halfword at +4 (a negative start ends at once, v0 = it): each node's byte +0x51 is cleared and its halfword +0x52
// gives the next index; v0 = 12 * the negative index that ended the chain, or 0 when a node is missing.
void func_08B01154(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 index = signed_half(half_at(rt, ctx.gpr[4] + 4u));
    if (static_cast<s32>(index) < 0) {
        ctx.gpr[2] = index;
        return;
    }
    u32 node = word(rt, object + 0x60u) + 84u * index;
    for (;;) {
        ctx.gpr[2] = node;
        if (node == 0u) return;
        put_byte(rt, node + 0x51u, 0u);
        index = signed_half(half_at(rt, node + 0x52u));
        ctx.gpr[2] = 12u * index;
        if (static_cast<s32>(index) < 0) return;
        node = word(rt, object + 0x60u) + 84u * index;
    }
}

// 0x08B2DD2C (no arguments): creates the object 0x08B2ECAC(0x08CB546C, 0x08B2DC98, 0) once, remembered at 0x08D68D64, and
// registers it with 0x08C7AF60(-1, object), keeping that answer at 0x08CE0C78.
void func_08B2DD2C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 existing = word(rt, 0x08D68D64u);
    ctx.gpr[2] = existing;
    if (static_cast<s32>(existing) > 0) return;
    const u32 created = call(rt, ctx, 0x08B2ECACu, {0x08CB546Cu, 0x08B2DC98u, 0u});
    put(rt, 0x08D68D64u, created);
    put(rt, 0x08CE0C78u, call(rt, ctx, 0x08C7AF60u, {0xFFFFFFFFu, created}));
}

// The walk of 0x08B03CE4 / 0x08B047D0 (a0 = an object, a3 = a record, t0 = a listener): the chain of 84-byte nodes at
// *(object + 0x60) from the record's signed halfword at +4; each node with a byte below 3 at +0x50 is reported to the
// listener's virtual slot 8 with f12/f13/f14 = its floats at +0x34 / +0x24 / +0x2C. `swapped` flips the order of the
// two arguments (s2 / s4) and of the node's two sub-pointers (+0x18 / +0xC).
void walk_nodes(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot_index, u32 plain, u32 record, u32 listener,
                bool swapped) {
    GuestFrame frame(ctx, 0x60u);
    u32 index = signed_half(half_at(rt, record + 4u));
    if (static_cast<s32>(index) < 0) {
        ctx.gpr[2] = index;
        return;
    }
    const u32 entry_pointer = object + 0x60u;
    const u32 entries = word(rt, object);
    const u32 shared = entries + 240u * slot_index;
    u32 node = word(rt, object + 0x60u) + 84u * index;
    ctx.gpr[2] = 84u * index;
    while (node != 0u) {
        const bool report = byte_at(rt, node + 0x50u) < 3u;
        ctx.gpr[2] = report ? 1u : 0u;
        if (report) {
            const u32 table = word(rt, listener);
            const u32 target = word(rt, table + 8u);
            ctx.gpr[3] = table;
            ctx.gpr[2] = target;
            const u32 first = swapped ? plain : shared;
            const u32 second = swapped ? shared : plain;
            const u32 third = swapped ? node + 0xCu : node + 0x18u;
            const u32 fourth = swapped ? node + 0x18u : node + 0xCu;
            const GuestResult result = call_guest(rt, ctx, target, {listener, first, second, third, fourth, node},
                                                  {float_bits(word(rt, node + 0x34u)), float_bits(word(rt, node + 0x24u)),
                                                   float_bits(word(rt, node + 0x2Cu))});
            ctx.gpr[2] = result.v0;
            ctx.gpr[3] = result.v1;
            ctx.fpr[0] = result.f0;
        }
        index = signed_half(half_at(rt, node + 0x52u));
        ctx.gpr[2] = 12u * index;
        if (static_cast<s32>(index) < 0) return;
        ctx.gpr[2] = word(rt, entry_pointer);
        node = word(rt, entry_pointer) + 84u * index;
    }
}
// 0x08B047D0 (a0 = an object, a1 = a word, a2 = a slot, a3 = a record, t0 = a listener).
void func_08B047D0(Runtime &rt, AllegrexContext &ctx) {
    walk_nodes(rt, ctx, ctx.gpr[4], ctx.gpr[6], ctx.gpr[5], ctx.gpr[7], ctx.gpr[8], true);
}

// 0x08B55F14 (a0 = an object, f12 = degrees): the 4x4 matrix at +0xD00 + 4 * word(+0xD10) is multiplied by a rotation about
// the third axis (the angle in quarter turns, reduced modulo 4 first: vrot gives the sine and cosine in C000 / C010, C020 and
// C030 are the unit vectors) and stored back; the words at +0xD18 / +0xD1C are OR'ed with +0xD20 / +0xD24. v0 = 0x3C360B61.
void func_08B55F14(Runtime &rt, AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    const u32 self = ctx.gpr[4];
    const u32 angle_bits = std::bit_cast<u32>(ctx.fpr[12]);
    const u32 index = word(rt, self + 0xD10u);
    const u32 flags_a = word(rt, self + 0xD18u);
    const u32 extra_a = word(rt, self + 0xD20u);
    const u32 matrix = word(rt, self + 0xD00u + index * 4u);
    v::move_to(ctx, angle_bits, 0x08u);                       // mtv S200
    for (u32 column = 0u; column < 4u; ++column) v::load_quad(rt, ctx, 0x0Cu + column, matrix + 0x10u * column);
    v::unit(ctx, 0x02u, 4u);                                  // vidt.q C020
    put(rt, self + 0xD18u, flags_a | extra_a);
    v::unit(ctx, 0x03u, 4u);                                  // vidt.q C030
    v::move_to(ctx, 0x3C360B61u, 0x28u);                      // mtv S201 (1 / 90)
    v::binary(ctx, v::Binary::Mul, 0x48u, 0x08u, 0x28u, 1u);  // vmul.s S202, S200, S201
    v::load_half(ctx, 0x08u, 0x3400u);                        // vfim.s S200, 0.25
    v::binary(ctx, v::Binary::Mul, 0x28u, 0x48u, 0x08u, 1u);  // vmul.s S201, S202, S200
    v::float_to_int(ctx, 17u, 0x28u, 0x28u, 0u, 1u);          // vf2iz.s S201, S201, 0
    v::int_to_float(ctx, 0x28u, 0x28u, 0u, 1u);               // vi2f.s S201, S201, 0
    v::load_half(ctx, 0x08u, 0x4400u);                        // vfim.s S200, 4
    v::binary(ctx, v::Binary::Mul, 0x28u, 0x28u, 0x08u, 1u);  // vmul.s S201, S201, S200
    v::binary(ctx, v::Binary::Sub, 0x48u, 0x48u, 0x28u, 1u);  // vsub.s S202, S202, S201
    v::rotation(ctx, 0x00u, 0x48u, 4u, 4u);                   // vrot.q C000, S202, [C, S, 0, 0]
    v::rotation(ctx, 0x01u, 0x48u, 4u, 17u);                  // vrot.q C010, S202, [-S, C, 0, 0]
    v::matrix_multiply(ctx, 0x04u, 0x2Cu, 0x00u, 4u);         // vmmul.q M100, E300 (encoded 0x2C), M000
    put(rt, self + 0xD1Cu, word(rt, self + 0xD1Cu) | word(rt, self + 0xD24u));
    for (u32 column = 0u; column < 4u; ++column) v::store_quad(rt, ctx, 0x04u + column, matrix + 0x10u * column);
    ctx.gpr[2] = 0x3C360B61u;
}

// 0x08B58178 (f12 = degrees; returns a float): the cosine as the VFPU gives it for the angle in quarter turns reduced modulo 4.
void func_08B58178(Runtime &rt, AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    (void)rt;
    const u32 angle_bits = std::bit_cast<u32>(ctx.fpr[12]);
    v::move_to(ctx, angle_bits, 0x00u);                       // mtv S000
    v::move_to(ctx, 0x3C360B61u, 0x20u);                      // mtv S001 (1 / 90)
    v::binary(ctx, v::Binary::Mul, 0x40u, 0x00u, 0x20u, 1u);  // vmul.s S002, S000, S001
    v::load_half(ctx, 0x00u, 0x3400u);                        // vfim.s S000, 0.25
    v::binary(ctx, v::Binary::Mul, 0x20u, 0x40u, 0x00u, 1u);  // vmul.s S001, S002, S000
    v::float_to_int(ctx, 17u, 0x20u, 0x20u, 0u, 1u);          // vf2iz.s S001, S001, 0
    v::int_to_float(ctx, 0x20u, 0x20u, 0u, 1u);               // vi2f.s S001, S001, 0
    v::load_half(ctx, 0x00u, 0x4400u);                        // vfim.s S000, 4
    v::binary(ctx, v::Binary::Mul, 0x20u, 0x20u, 0x00u, 1u);  // vmul.s S001, S001, S000
    v::binary(ctx, v::Binary::Sub, 0x40u, 0x40u, 0x20u, 1u);  // vsub.s S002, S002, S001
    v::rotation(ctx, 0x00u, 0x40u, 2u, 1u);                   // vrot.p C000, S002, [S, C]
    const u32 cosine = ctx.vfpu_scalar_bits(0x20u);           // mfv v0, S001
    ctx.gpr[2] = cosine;
    ctx.fpr[0] = std::bit_cast<float>(cosine);
}

// 0x08B58B10 (a0 = a selector byte): 0x08C7AC08(0, 0, 0, 1) for 0; for 1 the three fields at 0x08D6CE00 + 0x80 / 0x98 / 0x84
// (word, halfword, word) go to it as (a0, a1, a2) with a3 = 1; any other selector returns with v0 = 1.
void func_08B58B10(Runtime &rt, AllegrexContext &ctx) {
    const u32 selector = ctx.gpr[4] & 0xFFu;
    constexpr u32 kBlock = 0x08D6CE00u;
    if (selector == 0u) {
        tail_call(ctx, 0x08C7AC08u, {0u, 0u, 0u, 1u});
    } else if (selector == 1u) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08C7AC08u, {word(rt, kBlock + 0x80u), half_at(rt, kBlock + 0x98u), word(rt, kBlock + 0x84u), 1u});
    } else {
        ctx.gpr[2] = 1u;
    }
}

// 0x08BE6674 (a0 = a destination array, a1 = a count, a2 = a source object): the source's 0x30-byte record (words +4 / +8,
// floats +0xC..+0x2C) is copied to each element with the vtable word 0x08CCDE00 at +0 (a null destination skips the copy).

// 0x08BF4C70 (a0 = begin, a1 = end of 0x28-byte records, a2 = a destination): each record is copied (words +4 / +8, floats
// +0xC..+0x1C, +0x20 / +0x24) with the vtable word 0x08CCE6E0 at +0; a null destination skips the copy. v0 = the destination
// after the last record.
void func_08BF4C70(Runtime &rt, AllegrexContext &ctx) {
    u32 source = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (source != end) {
        do {
            if (destination != 0u) {
                for (u32 offset = 4u; offset <= 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
                put(rt, destination, 0x08CCE6E0u);
                put(rt, destination + 0x20u, word(rt, source + 0x20u));
                put(rt, destination + 0x24u, word(rt, source + 0x24u));
            }
            source += 0x28u;
            destination += 0x28u;
        } while (source != end);
    }
    ctx.gpr[2] = destination;
}

// 0x08C20F98 (a0 = an object, a1 = a key): 0x08A6158C(key) and 0x08A6159C(key, *(a0 + 8)) give two handles; 0x08A61BC0
// fills a block on the frame with the key; the first handle's pointer (asserted, message 0x08CB0284) runs its virtual slot
// 0xD0 with (pointer, block, *(a0 + 8), second handle); the block is released (0x08A5B9EC).
void func_08C20F98(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 block = frame.at(4);
    const u32 first = call(rt, ctx, 0x08A6158Cu, {key});
    const u32 second = call(rt, ctx, 0x08A6159Cu, {key, word(rt, self + 8u)});
    put(rt, frame.at(0), key);
    call(rt, ctx, 0x08A61BC0u, {block, frame.at(0)});
    const u32 pointer = word(rt, first);
    if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    const u32 table = word(rt, pointer);
    const u32 target = word(rt, table + 0xD0u);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, {pointer, block, word(rt, self + 8u), second}, {});
    ctx.gpr[2] = result.v0;
    call(rt, ctx, 0x08A5B9ECu, {block});
}

} // namespace
} // namespace new_batch_fixes_1
} // namespace gt::game

#define NEW_FIX1(address, name, kind) GT_DECOMP(address, gt::game::new_batch_fixes_1::name, gt::ReturnKind::kind)
NEW_FIX1(0x0885BCA8u, func_0885BCA8, DoubleWord);
NEW_FIX1(0x0885F550u, func_0885F550, Word);
NEW_FIX1(0x0886C31Cu, func_0886C31C, Word);
NEW_FIX1(0x08872E18u, func_08872E18, Word);
NEW_FIX1(0x088733F4u, func_088733F4, Word);
NEW_FIX1(0x08877CCCu, func_08877CCC, Word);
NEW_FIX1(0x0888EC10u, func_0888EC10, Word);
NEW_FIX1(0x08925C2Cu, func_08925C2C, Float);
NEW_FIX1(0x0892D3E0u, func_0892D3E0, Word);
NEW_FIX1(0x08A3523Cu, func_08A3523C, Word);
NEW_FIX1(0x08A5A28Cu, func_08A5A28C, Word);
NEW_FIX1(0x08A8350Cu, func_08A8350C, Word);
NEW_FIX1(0x08AA9AC8u, func_08AA9AC8, Word);
NEW_FIX1(0x08B01154u, func_08B01154, Word);
NEW_FIX1(0x08B2DD2Cu, func_08B2DD2C, Word);
NEW_FIX1(0x08B047D0u, func_08B047D0, Word);
NEW_FIX1(0x08B55F14u, func_08B55F14, Word);
NEW_FIX1(0x08B58178u, func_08B58178, Float);
NEW_FIX1(0x08B58B10u, func_08B58B10, Word);
NEW_FIX1(0x08BF4C70u, func_08BF4C70, Word);
NEW_FIX1(0x08C20F98u, func_08C20F98, Word);
