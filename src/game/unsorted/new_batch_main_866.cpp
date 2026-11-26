// Main-session batch 866: functions earlier drafts got wrong (the synthetic gate compared them and found a difference),
// or that no draft took, hand-written as structured C from `tools.disasm`. No names: none has evidence.
// Unverified drafts: the synthetic gate decides which of them register.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"
#include "gt/vfpu.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_866 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order; what
// is not passed stays as the caller had it), and the registers the callee leaves behind kept in the context (v0, v1, a1,
// f0), as the original's next instructions see them.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments, {}).v0;
}
// A callee with float arguments ($f12, $f13, ...): returns $f0 and keeps v0 / v1 / f0 as the `jal` leaves them.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    return call_result(rt, ctx, target, arguments, floats);
}

// The callee-saved registers a body keeps values in (the harness compares s0-s7, so the caller's values come back).
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(0) {
        for (const unsigned reg : registers) {
            if (count_ < 8u) {
                registers_[count_] = reg;
                values_[count_] = ctx_.gpr[reg];
                ++count_;
            }
        }
    }
    ~SavedRegisters() {
        while (count_ != 0u) {
            --count_;
            ctx_.gpr[registers_[count_]] = values_[count_];
        }
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned registers_[8];
    u32 values_[8];
    unsigned count_;
};

u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
u32 bits_of(float value) { return std::bit_cast<u32>(value); }

// =================================================================================================================
// 0x08B56D4C (a0 = object): when bit 10 of the word at +0xD1C is set, clear it, take the matrix 0x08B56A88(object) and the
// one at *(object + 0xD00), multiply them (vmmul.q M000, M200, M100) and store the four columns at +0x840 .. +0x870.
// v0 = object + 0x800 either way.

// 0x0888D82C (a0 = self, a1 = first, a2 = second, a3 = a flag byte): looks up the object at *(self + 4) and sets bit 0 of
// its byte +0x198 when the flag is zero, bit 1 when it is not; only the flag-zero path also queues a 0x18-tagged record
// (second at +8, first at +0xC) through 0x088936A4(*(self + 0x18), record). v0 = the object, or the queue call's answer.
void func_0888D82C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    SavedRegisters saved(ctx, {16, 17, 18});
    GuestFrame frame(ctx, 0xB0u);
    ctx.gpr[18] = second;
    ctx.gpr[17] = first;
    ctx.gpr[16] = self;
    const u32 object = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), first});
    if (flag == 0u) {
        put8(rt, object + 0x198u, byte_u(rt, object + 0x198u) | 1u);
        ctx.gpr[2] = object;
        return;
    }
    put8(rt, object + 0x198u, byte_u(rt, object + 0x198u) | 2u);
    put(rt, frame.at(0u), 0x18u);
    put(rt, frame.at(8u), second);
    put(rt, frame.at(0xCu), first);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
}

// 0x08A83FB8 (a0 = object, a1 = destination, a3 = pointer to the range object): the range holds the word at *a3 (an
// assert, 0x08A5BB7C, when it is null) and the two words at +0x18 / +0x1C of a0, XORed with what the vtable slot 0x50 of
// that object returns in v0 and v1; 0x08A83160 builds it from (a1, low ^ v0, high ^ v1), 0x08A5BAD8 assigns it into
// `destination` unless that already is the temporary, and 0x08A5B9EC destroys the temporary (its v0 is the answer).
// =================================================================================================================

// =================================================================================================================
// 0x08B42618 (a0 = object, a1 = a word, a2 = the output block, a3 = a flag byte): the member at +0xB0's vtable slot
// 0x3C gives the pair stored at +8 / +0xC of the output; 0x08B3FF90 builds a temporary from (a1 + 0x12), 0x08B3FA3C
// completes it with 0x4E9D, and the member's slot 0x64 finishes with (a2 = the temporary, a3 = the flag), its v0 the answer.
// =================================================================================================================
void func_08B42618(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx, {16, 17, 18, 19});
    ctx.gpr[19] = ctx.gpr[7] & 0xFFu;  // andi s3, a3, 0xFF
    ctx.gpr[18] = self;                // s2 = a0
    ctx.gpr[17] = out;                 // s1 = a2
    ctx.gpr[16] = value;               // s0 = a1 (the jalr's delay slot)
    const u32 member = word(rt, self + 0xB0u);
    ctx.gpr[4] = member;               // lw a0, 0xB0(a0)
    ctx.gpr[3] = word(rt, member);     // lw v1, 0x0(a0)
    const GuestResult setup = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x3Cu), {}, {});
    put(rt, ctx.gpr[17] + 8u, setup.v0);
    ctx.gpr[3] = setup.v1;
    put(rt, ctx.gpr[17] + 0xCu, setup.v1);  // sw v1, 0xC(s1), the delay slot of the next call
    call_set(rt, ctx, 0x08B3FF90u, {frame.at(0u), ctx.gpr[16] + 0x12u});
    call_set(rt, ctx, 0x08B3FA3Cu, {frame.at(0u), 0x4E9Du});
    const u32 member_again = word(rt, self + 0xB0u);
    ctx.gpr[4] = member_again;
    ctx.gpr[3] = word(rt, member_again);  // lw v1, 0x0(a0)
    call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x64u),
                  {member_again, ctx.gpr[17], frame.at(0u), ctx.gpr[19]});
}


// 0x08BE6674 (a0 = a base, a1 = a count, a2 = the source): copies `count` records of 0x30 bytes from a2 to the base,
// 0x30 apart, the two words at +4 / +8 and the float at +0xC .. +0x1C first, the vtable 0x08CCDE00 at +0 after them, then
// +0x20 .. +0x2C; a null base skips the copy. v0 keeps the `lui` in the first branch's delay slot, or the word at +4.
void func_08BE6674(Runtime &rt, AllegrexContext &ctx) {
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    u32 destination = ctx.gpr[4];
    ctx.gpr[2] = 0x08CD0000u;  // the `lui` in the beqz's delay slot (the high half of 0x08CCDE00): it always runs
    if (count == 0u) return;
    ctx.gpr[11] = 0x08CCDE00u;  // t3 = the vtable
    for (u32 done = 0u;;) {
        ++done;  // the delay slot of the null test
        if (destination != 0u) {
            const u32 low = word(rt, source + 4u);
            const u32 high = word(rt, source + 8u);
            const u32 at0C = word(rt, source + 0xCu);
            const u32 at10 = word(rt, source + 0x10u);
            const u32 at14 = word(rt, source + 0x14u);
            const u32 at18 = word(rt, source + 0x18u);
            const u32 at1C = word(rt, source + 0x1Cu);
            put(rt, destination + 4u, low);
            put(rt, destination + 8u, high);
            put(rt, destination + 0xCu, at0C);
            put(rt, destination + 0x10u, at10);
            put(rt, destination + 0x14u, at14);
            put(rt, destination + 0x18u, at18);
            put(rt, destination + 0x1Cu, at1C);
            put(rt, destination, 0x08CCDE00u);
            put(rt, destination + 0x20u, word(rt, source + 0x20u));
            put(rt, destination + 0x24u, word(rt, source + 0x24u));
            put(rt, destination + 0x28u, word(rt, source + 0x28u));
            put(rt, destination + 0x2Cu, word(rt, source + 0x2Cu));
            ctx.gpr[2] = low;   // lw v0, 0x4(a2)
            ctx.gpr[3] = high;  // lw v1, 0x8(a2)
        }
        destination += 0x30u;
        if (count == done) return;
    }
}

// 0x0882D4BC (a0 = a record, a1 / a2 / a3 = three keys): 0x08B85EB8(&temporary, a2, &byte), 0x0899C764(&value, a0, a1,
// &temporary, a3) and 0x089A9324(a0, &value) fill the record and 0x08A5B9EC(&value) releases the temporary; then the string
// at *(sp + 4) is dropped: 0x08B60804(data - 4, -1) and, when that is not positive, 0x08B8542C(data - 0xC, sp). The shared
// empty string 0x08D74FD0 skips both and stays in v0.
void func_0882D4BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20});
    ctx.gpr[18] = frame.at(4u);   // s2 = sp + 4
    ctx.gpr[20] = record;         // s4 = a0
    ctx.gpr[19] = third;          // s3 = a3
    ctx.gpr[17] = first;          // s1 = a1
    ctx.gpr[16] = frame.at(8u);   // s0 = sp + 8
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), second, frame.at(1u), third});
    call_set(rt, ctx, 0x0899C764u, {frame.at(8u), record, first, frame.at(4u), third});
    call_set(rt, ctx, 0x089A9324u, {record, frame.at(8u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    const u32 data = word(rt, frame.at(4u));
    const u32 block = data - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;  // lui / addiu v0
    if (block == 0x08D74FD0u) return;
    const u32 status = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(status) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
}

// 0x08877354 (a0 = a factory): 0x08AB85F0 / 0x08AB856C build the temporary at sp + 4, the pair 0x08B77BF4 gives from its key
// becomes a float through 0x08B71EC8; when the counter at the temporary's +0xC is positive, 0x088757B0(factory, 0x08CA0074)
// is looked up and 0x08AF4458(found + 0xC, ..., f12 = the float) applies it. The string at *(sp + 4) is then dropped like
// 0x0882D4BC does (0x08B60804 / 0x08B93554), with 0x08D75298 as the shared empty answer in v0.
void func_08877354(Runtime &rt, AllegrexContext &ctx) {
    const u32 factory = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx, {16, 17});
    ctx.gpr[16] = frame.at(4u);  // s0 = sp + 4
    ctx.gpr[17] = factory;       // s1 = a0
    call_set(rt, ctx, 0x08AB85F0u, {frame.at(4u)});
    const u32 key = call_set(rt, ctx, 0x08AB856Cu, {frame.at(4u)});
    const GuestResult pair = call_result(rt, ctx, 0x08B77BF4u, {key});
    const float value = call_floats(rt, ctx, 0x08B71EC8u, {pair.v0, pair.v1}, {}).f0;
    if (static_cast<s32>(word(rt, frame.at(0xCu))) > 0) {
        const u32 found = call_set(rt, ctx, 0x088757B0u, {factory, 0x08CA0074u});
        if (found != 0u) call_floats(rt, ctx, 0x08AF4458u, {found + 0xCu}, {value});
    }
    const u32 data = word(rt, frame.at(4u));
    const u32 block = data - 0xCu;
    ctx.gpr[2] = 0x08D75298u;  // lui / addiu v0
    if (block == 0x08D75298u) return;
    const u32 status = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(status) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {block, frame.at(0u)});
}

// 0x0886BB40 (a0 = a holder, a1 = a source): the holder's object (*(a0 + 4)) takes *(a1) at +0xF0 and *(a1 + 4) at +0xF4,
// the latter refcounted: when it differs from the word already there, its count at +4 is bumped and the old object's is
// dropped (ll/sc), and an object whose count reaches zero first gets its virtual slot 8 and then, when the count at +8
// also reaches zero, its slot 0xC. v0 keeps the holder, or 1 once a count was touched.
void func_0886BB40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx, {16, 17, 18});
    const u32 object = word(rt, self + 4u);
    ctx.gpr[2] = object;               // lw v0, 0x4(a0)
    ctx.gpr[18] = object + 0xF4u;      // addiu s2, v0, 0xF4 (the beq's delay slot)
    const u32 value = word(rt, source);
    const u32 current = word(rt, object + 0xF4u);
    put(rt, object + 0xF0u, value);
    const u32 incoming = word(rt, source + 4u);
    ctx.gpr[17] = incoming;            // s1
    if (incoming == current) return;   // v0 = the holder
    if (incoming != 0u) {
        put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);  // the ll/sc bump
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[16] = word(rt, object + 0xF4u);  // lw s0, 0x0(s2)
    if (ctx.gpr[16] == 0u) {
        put(rt, object + 0xF4u, incoming);
        return;
    }
    const u32 count = word(rt, ctx.gpr[16] + 4u);
    put(rt, ctx.gpr[16] + 4u, count - 1u);  // the ll/sc drop
    ctx.gpr[2] = 1u;
    if (count != 1u) {
        put(rt, object + 0xF4u, incoming);
        return;
    }
    ctx.gpr[3] = word(rt, ctx.gpr[16]);  // lw v1, 0x0(s0)
    call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 8u), {ctx.gpr[16]});
    const u32 second = word(rt, ctx.gpr[16] + 8u);
    put(rt, ctx.gpr[16] + 8u, second - 1u);
    ctx.gpr[2] = 1u;
    if (second != 1u) {
        put(rt, object + 0xF4u, incoming);
        return;
    }
    ctx.gpr[3] = word(rt, ctx.gpr[16]);
    call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0xCu), {ctx.gpr[16]});
    put(rt, object + 0xF4u, incoming);
}

// 0x0888B88C (a0 = object, a1 = index): when the object's +0xC answers with a word whose bit 1 is set and 0x088DB2C8(*(a0+4))
// answers non-zero, *(*(a0+4) + 0x618) is cleared and the object's vtable slot 0x70 runs with (a0, *(a0+4) + 4, sp, 1, 0);
// otherwise the halfword at *(a0 + 0x34) + index * 432 + 0x1560 is bumped and 0x088936A4(*(a0 + 0x18), sp + 8) gets a
// record of 0x21 / index at +8 / +0x10. v0 = the last call's answer.
void func_0888B88C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    SavedRegisters saved(ctx, {16, 17});
    ctx.gpr[16] = self;   // s0
    ctx.gpr[17] = index;  // s1
    const u32 state = call_set(rt, ctx, 0x0889417Cu, {word(rt, self + 0xCu)});
    ctx.gpr[5] = index;  // a1 = s1 (the beqz's delay slot)
    if ((word(rt, state) & 2u) != 0u) {
        ctx.gpr[4] = self;            // a0 = s0
        ctx.gpr[6] = frame.at(0u);    // a2 = sp
        ctx.gpr[7] = 1u;              // a3 = 1
        ctx.gpr[8] = 0u;              // t0 = 0
        if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u)}) != 0u) {
            ctx.gpr[9] = 0u;  // t1 = 0 (the beqz's delay slot)
            put_half(rt, frame.at(0u), index);
            put8(rt, word(rt, word(rt, self + 4u) + 0x618u), 0u);
            const u32 table = word(rt, self);
            const u32 member = word(rt, self + 4u);
            ctx.gpr[3] = table;  // lw v1, 0x0(s0)
            call_indirect(rt, ctx, word(rt, table + 0x70u), {self, member + 4u, frame.at(0u), 1u, 0u});
            return;
        }
    }
    const u32 entry = word(rt, self + 0x34u) + index * 432u + 0x1560u;
    put_half(rt, entry, half_u(rt, entry) + 1u);
    put(rt, frame.at(8u), 0x21u);
    ctx.gpr[5] = frame.at(8u);  // a1 = sp + 8
    put(rt, frame.at(0x10u), index);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(8u)});
}

// 0x08A5E338 (a0 = object, a1 = the destination, a3 = pointer to the range object): asserts *(a3) twice, runs that object's
// vtable slot 0x50 twice, hands the second answer's (v0, v1) to 0x08B702B4(*(a0 + 0x18), *(a0 + 0x1C), v0, v1) and the
// result to 0x08A5DEE8(sp, ...); the temporary goes into the destination unless it already is it, then is destroyed
// (0x08A5B9EC). v0 = its answer.
void func_08A5E338(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx, {16, 17, 18, 19});
    ctx.gpr[19] = self;         // s3 = a0
    ctx.gpr[18] = source;       // s2 = a3
    ctx.gpr[17] = destination;  // s1 = a1 (the bnez's delay slot)
    ctx.gpr[16] = word(rt, source);
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0080u});
    ctx.gpr[3] = word(rt, ctx.gpr[16]);  // lw v1, 0x0(s0)
    call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x50u), {ctx.gpr[16]}, {});
    ctx.gpr[16] = word(rt, source);         // lw s0, 0x0(s2)
    ctx.gpr[18] = word(rt, self + 0x18u);   // lw s2, 0x18(s3)
    ctx.gpr[19] = word(rt, self + 0x1Cu);   // lw s3, 0x1C(s3), the bnez's delay slot
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0080u});
    ctx.gpr[3] = word(rt, ctx.gpr[16]);
    const GuestResult limits = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x50u), {ctx.gpr[16]}, {});
    const GuestResult built = call_result(rt, ctx, 0x08B702B4u, {ctx.gpr[18], ctx.gpr[19], limits.v0, limits.v1});
    call_result(rt, ctx, 0x08A5DEE8u, {frame.at(0u), built.a1, built.v0, built.v1});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08BDB668 (a0 = the table, a1 = hole, a2 = count): sifts the hole down the heap of 8-byte entries: of the two children
// of `child` (2 * hole + 2 and the next index) the larger one (at +4, signed; on a tie the smaller word at +0, unsigned)
// moves into the hole, and the walk continues; when the hole ends where count == 2 * hole + 2 the last node moves up and the
// hole becomes count - 1. It ends in the tail call 0x08BDB598(table, hole, the original index).
void func_08BDB668(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    u32 hole = index;
    u32 child = index * 2u + 2u;   // sll / addiu
    ctx.gpr[2] = static_cast<u32>(child < static_cast<u32>(count));  // the slt, until a value is moved
    if (static_cast<s32>(child) < count) {
        for (;;) {
            const u32 slot = table + (child << 3);
            const u32 left_key = word(rt, slot - 4u);
            const u32 right_key = word(rt, slot + 4u);
            if (static_cast<s32>(right_key) < static_cast<s32>(left_key)) {
                --child;
            } else if (left_key == right_key && word(rt, slot) < word(rt, slot - 8u)) {
                --child;
            }
            const u32 chosen = table + (child << 3);
            const u32 value = word(rt, chosen);
            put(rt, table + (hole << 3), value);
            put(rt, table + (hole << 3) + 4u, word(rt, chosen + 4u));
            hole = child;
            ctx.gpr[2] = value;
            const u32 next_child = child * 2u + 2u;
            if (static_cast<s32>(next_child) >= count) break;
            child = next_child;
        }
    }
    const u32 next = hole * 2u + 2u;
    if (count == static_cast<s32>(next)) {
        const u32 last = table + (static_cast<u32>(count) << 3);
        ctx.gpr[2] = last;  // sll / addu v0 = the last node's address
        put(rt, table + (hole << 3), word(rt, last - 8u));
        put(rt, table + (hole << 3) + 4u, word(rt, last - 4u));
        hole = static_cast<u32>(count - 1);
    }
    tail_call(ctx, 0x08BDB598u, {table, hole, index});
}

// 0x08875138 (a0 = pointer to the object): the object's five refcounted slots at +0x834 .. +0x854 (8 bytes apart, the
// pointer at +4 of each) are dropped from the last to the first: a count that reaches one runs the object's virtual slots
// +8 and then, when the count at +8 reaches one too, +0xC. Then 0x0887BEB0(object + 0x814), 0x08AD0574(object + 0x3E0) and
// 0x08AD0700(object + 0x10) run and 0x08B64654(object) finishes. v0 = 0x830 when the object is null.
void func_08875138(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx, {16, 17, 18, 19});
    const u32 object = word(rt, holder);
    ctx.gpr[18] = object;             // s2
    ctx.gpr[2] = object + 0x830u;     // the delay slot of the null test
    if (object == 0u) return;
    const u32 end = object + 0x830u;
    ctx.gpr[19] = end;                // s3
    if (end != 0u) {
        u32 cursor = object + 0x858u;
        ctx.gpr[17] = cursor;             // s1
        while (cursor != end) {
            cursor -= 8u;
            ctx.gpr[17] = cursor;
            const u32 slot = word(rt, cursor + 4u);
            ctx.gpr[16] = slot;           // s0
            if (slot == 0u) continue;
            const u32 count = word(rt, slot + 4u);
            put(rt, slot + 4u, count - 1u);  // the ll/sc drop
            ctx.gpr[2] = 1u;
            if (count != 1u) continue;
            ctx.gpr[3] = word(rt, slot);  // lw v1, 0x0(s0)
            call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 8u), {slot});
            const u32 second = word(rt, slot + 8u);
            put(rt, slot + 8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second != 1u) continue;
            ctx.gpr[3] = word(rt, slot);
            call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0xCu), {slot});
        }
    }
    call_set(rt, ctx, 0x0887BEB0u, {object + 0x814u});
    call_set(rt, ctx, 0x08AD0574u, {object + 0x3E0u});
    call_set(rt, ctx, 0x08AD0700u, {object + 0x10u});
    tail_call(ctx, 0x08B64654u, {object});
}

// 0x08BE81A0 (a0 = a container, a1 = an index): 0x089D76A8 builds a temporary value at sp (vtable 0x08CCFF88, a word -1 in
// two places, 1.0f in the rest), then the count ((*(a0+0xAC) - *(a0+0xA8)) >> 5) / 24 decides: an index below it runs
// 0x08BE6244(a0 + 0xA8, *(a0+0xA8) + 96 * index, *(a0+0xAC), &value), otherwise 0x08BE7DCC(a0 + 0xA8, *(a0+0xAC),
// index - count); 0x089D6684(&value) closes. v0 = its answer.

// 0x08BED6FC (a0 = a vector of 0x30-byte elements, a1 = a position, a2 = the element to copy): when the vector is not full
// and the position is its end, the element's words (+4, +8, +0xC .. +0x1C), the vtable 0x08CCDE00 and +0x20 .. +0x2C are
// copied past the end and that end is advanced; anything else goes to 0x08BED390(a0, position, element, capacity).
// v0 = *(a0) + 0x30 * index, where index = (position - *(a0)) / 48 (a multiply by 0xAAAAAAAB).
void func_08BED6FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 element = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx, {16, 17});
    ctx.gpr[16] = self;  // s0
    const u32 begin = word(rt, self);
    const u32 index = static_cast<u32>(static_cast<s32>(position - begin) >> 4) * 0xAAAAAAABu;
    ctx.gpr[17] = index;  // s1
    const u32 end = word(rt, self + 4u);
    const u32 capacity = word(rt, self + 8u);
    if (end != capacity && position == end) {
        if (end == 0u) {
            put(rt, self + 4u, 0x30u);
        } else {
            const u32 at4 = word(rt, element + 4u);
            const u32 at8 = word(rt, element + 8u);
            const u32 at0C = word(rt, element + 0xCu);
            const u32 at10 = word(rt, element + 0x10u);
            const u32 at14 = word(rt, element + 0x14u);
            const u32 at18 = word(rt, element + 0x18u);
            const u32 at1C = word(rt, element + 0x1Cu);
            put(rt, end + 4u, at4);
            put(rt, end + 8u, at8);
            put(rt, end + 0xCu, at0C);
            put(rt, end + 0x10u, at10);
            put(rt, end + 0x14u, at14);
            put(rt, end + 0x18u, at18);
            put(rt, end + 0x1Cu, at1C);
            put(rt, end, 0x08CCDE00u);
            put(rt, end + 0x20u, word(rt, element + 0x20u));
            put(rt, end + 0x24u, word(rt, element + 0x24u));
            put(rt, end + 0x28u, word(rt, element + 0x28u));
            put(rt, end + 0x2Cu, word(rt, element + 0x2Cu));
            put(rt, self + 4u, word(rt, self + 4u) + 0x30u);
        }
    } else {
        call_set(rt, ctx, 0x08BED390u, {self, position, element, capacity});
    }
    ctx.gpr[2] = word(rt, self) + index * 0x30u;
}
} // namespace
} // namespace new_batch_main_866
} // namespace gt::game

#define NEW866(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_866::name, gt::ReturnKind::kind)
NEW866(0x0888D82Cu, func_0888D82C, Word);
NEW866(0x08B42618u, func_08B42618, Word);
NEW866(0x08BE6674u, func_08BE6674, Word);
NEW866(0x0882D4BCu, func_0882D4BC, Word);
NEW866(0x08877354u, func_08877354, Word);
NEW866(0x0886BB40u, func_0886BB40, Word);
NEW866(0x0888B88Cu, func_0888B88C, Word);
NEW866(0x08A5E338u, func_08A5E338, Word);
NEW866(0x08BDB668u, func_08BDB668, Word);
NEW866(0x08875138u, func_08875138, Word);
NEW866(0x08BED6FCu, func_08BED6FC, Word);
