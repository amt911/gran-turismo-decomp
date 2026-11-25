// New batch (main session 1011): functions an earlier draft mismatched or that two drafts skipped, written as structured C
// from `tools.disasm` (no recompiler output was read). Each body keeps the original's order of calls and stores (a store in
// a `jal`'s delay slot happens before the call and the callee may read it), leaves v0 as the original leaves it on every
// exit path and passes every argument register the callee reads. No names: none of these has evidence.
//
// 0x08900A38 continues the constructor body of 0x089006DC (byte-identical words, batch 27): the earlier draft dropped the
// 0x08BA1BA8(object + 0x10) call the original makes after the 0x08BA1E24(object + 0x28) one, which is why a global
// allocation count came out one lower (gate: 0x08D75AC0 05 -> 04).
// 0x08BA82A8 and 0x08BA996C are one body (identical words) whose eight callees and assert string differ.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1011 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set the way the original sets them before its `jal` (a0-a3, t0-t3 in order);
// v0 holds the callee's result afterwards, as after the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, address, arguments);
    return ctx.gpr[2];
}
// A virtual call as the compiler wrote it: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 sra(u32 value, unsigned bits) { return static_cast<u32>(signed_of(value) >> bits); }
inline s32 signed_byte_of(u32 value) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(value)); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The `ll/sc` increment of a shared object's use count (single-threaded here): v1 = the address, v0 = 1 (the successful sc).
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
// One use of a shared object is released: its count (+4) goes down; when that was the last one virtual slot 8 runs, then
// the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    const u32 old = word(rt, object + 4u);
    put(rt, object + 4u, old - 1u);
    if (old != 1u) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    const u32 weak = word(rt, object + 8u);
    put(rt, object + 8u, weak - 1u);
    if (weak == 1u) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// ---- functions ----

// 0x08883E74 (self, owner, name, flag): returns 0 at once when self->+0x16F4 is set. Otherwise a frame string object is
// built from `name` (0x08B7C830(frame, name, 8), a terminating byte at frame + 0x18); unless 0x08B7B8C4(frame, 0x08CA0634)
// answers non-zero, 0x08B7BB6C(frame, name + 8) and 0x08B77BFC(frame) give a width and name = 0x0886C638(owner, width).
// With a non-empty name the object h = 0x0886B080(owner) gives index = slot 0x18(h, name) (negative: 0) and item =
// slot 0x28(h, index) (zero: slot 0x24(h, index), then 0). self->+0x16F0 = index; slot 0x2C(h, index, 0); two slot 0x20
// calls (a2 = 0 and 1) give the arguments of a new 0x48-byte object (0x0889F1DC(new, item, first, second, flag, 1)) that a
// 0x10-byte control block (vtable 0x08CBF458, counts 1 / 1) wraps in the shared pointer stored at self + 0x4C / + 0x50 (the
// old one released); 0x0889E258(self->+0x4C, frame + 8) fills a pair whose block is released; self->+0x16F4 = 1; slot 0x30
// of h is then called with (h, index, 0x0886C31C(owner, width)); when self->+0x16F4 is still set 0x0886BC14(owner) runs and
// bit 4 of self->+0x16EC is set. The return value is self->+0x16F4.
void func_08883E74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    u32 name = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x50u);
    if (word(rt, self + 0x16F4u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 text = frame.at(0x10u);
    call_set(rt, ctx, 0x08B7C830u, {text, name, 8u});
    put8(rt, frame.at(0x18u), 0u);
    u32 width = 0u;
    if (call_set(rt, ctx, 0x08B7B8C4u, {text, 0x08CA0634u}) == 0u) {
        call_set(rt, ctx, 0x08B7BB6Cu, {text, name + 8u});
        width = call_set(rt, ctx, 0x08B77BFCu, {text});
        name = call_set(rt, ctx, 0x0886C638u, {owner, width});
    }
    if (name == 0u || signed_byte_of(rt.memory().load8(name)) == 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 handler = call_set(rt, ctx, 0x0886B080u, {owner});
    const u32 index = call_virtual_at(rt, ctx, handler, 0x18u, {handler, name});
    if (signed_of(index) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 item = call_virtual_at(rt, ctx, handler, 0x28u, {handler, index});
    if (item == 0u) {
        call_virtual_at(rt, ctx, handler, 0x24u, {handler, index});
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, self + 0x16F0u, index);
    call_virtual_at(rt, ctx, handler, 0x2Cu, {handler, index, 0u});
    const u32 first = call_virtual_at(rt, ctx, handler, 0x20u, {handler, word(rt, self + 0x16F0u), 0u});
    const u32 second = call_virtual_at(rt, ctx, handler, 0x20u, {handler, word(rt, self + 0x16F0u), 1u});
    const u32 object = call_set(rt, ctx, 0x08B62344u, {0x48u});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x0889F1DCu, {object, item, first, second, flag, 1u});
    const u32 control = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, control, 0x08CBF458u);
    put(rt, control + 4u, 1u);
    put(rt, control + 0xCu, object);
    put(rt, control + 8u, 1u);
    put(rt, frame.at(4u), control);
    call_set(rt, ctx, 0x08B8E380u,
             {frame.at(4u), object, object, 1u, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
    const u32 old_pointer = word(rt, self + 0x4Cu);
    const u32 new_pointer = word(rt, frame.at(0u));
    const u32 old_control = word(rt, self + 0x50u);
    const u32 new_control = word(rt, frame.at(4u));
    put(rt, frame.at(0u), old_pointer);
    put(rt, self + 0x50u, new_control);
    put(rt, self + 0x4Cu, new_pointer);
    put(rt, frame.at(4u), old_control);
    if (old_control != 0u) release_shared(rt, ctx, old_control);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call_set(rt, ctx, 0x0889E258u, {word(rt, self + 0x4Cu), frame.at(8u)});
    const u32 pair_control = word(rt, frame.at(0xCu));
    if (pair_control != 0u) release_shared(rt, ctx, pair_control);
    put(rt, self + 0x16F4u, 1u);
    const u32 table = word(rt, handler);
    const u32 current = word(rt, self + 0x16F0u);
    const u32 slot = word(rt, table + 0x30u);
    ctx.gpr[2] = table;
    const u32 argument = call_set(rt, ctx, 0x0886C31Cu, {owner, width});
    call_set(rt, ctx, slot, {handler, current, argument});
    if (word(rt, self + 0x16F4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x0886BC14u, {owner});
    put(rt, self + 0x16ECu, word(rt, self + 0x16ECu) | 4u);
    ctx.gpr[2] = word(rt, self + 0x16F4u);
}

// 0x08900A38 (a0 = the object; the same words as 0x089006DC): constructor. Three vtable words go to +0, +4, +8, then the
// member constructors run one after another on this + offset (each field is defaulted right after its own constructor where
// the asm stores it), and the body ends in a tail call to 0x089000CC with a0 = the object and v0 = the last call's result.
void func_08900A38(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const float one = 1.0f;
    const float half = 0.5f;
    const float hundred = 100.0f;
    const float thousand = 1000.0f;
    u32 result;
    {
        GuestFrame frame(ctx, 0x50u);
        (void)frame;

        put(rt, object, 0x08CC5408u);
        put(rt, object + 4u, 0x08CC5438u);
        put(rt, object + 8u, 0x08CC5454u);

        call_set(rt, ctx, 0x08BA0210u, {object + 0xCu});
        call_set(rt, ctx, 0x08BA0140u, {object + 0xCu});
        call_set(rt, ctx, 0x08BA1C34u, {object + 0xCu});
        call_set(rt, ctx, 0x08BA2360u, {object + 0x10u});
        put_float(rt, object + 0x10u, one);
        call_set(rt, ctx, 0x08BA22D4u, {object + 0x14u});
        put_float(rt, object + 0x14u, std::bit_cast<float>(0x3F666666u));  // 0.9
        call_set(rt, ctx, 0x08BA21E4u, {object + 0x18u});
        put_float(rt, object + 0x18u, thousand);
        call_set(rt, ctx, 0x08BA20F4u, {object + 0x1Cu});
        put_float(rt, object + 0x1Cu, one);
        call_set(rt, ctx, 0x08BA2004u, {object + 0x20u});
        put(rt, object + 0x20u, 0u);
        call_set(rt, ctx, 0x08BA1F14u, {object + 0x24u});
        put_float(rt, object + 0x24u, 200.0f);
        call_set(rt, ctx, 0x08BA1E24u, {object + 0x28u});
        put(rt, object + 0x28u, 0u);
        call_set(rt, ctx, 0x08BA1BA8u, {object + 0x10u});
        call_set(rt, ctx, 0x08BA1704u, {object + 0x2Cu});
        call_set(rt, ctx, 0x08BA1678u, {object + 0x3Cu});
        put_float(rt, object + 0x3Cu, hundred);
        call_set(rt, ctx, 0x08BA1588u, {object + 0x40u});
        put_float(rt, object + 0x40u, thousand);
        call_set(rt, ctx, 0x08BA1498u, {object + 0x44u});
        put(rt, object + 0x44u, 0u);
        call_set(rt, ctx, 0x08BA1AB8u, {object + 0x2Cu});
        call_set(rt, ctx, 0x08BA3718u, {object + 0x48u});
        call_set(rt, ctx, 0x08B9FE44u, {object + 0x3E8u});
        call_set(rt, ctx, 0x08BA1964u, {object + 0x3E8u});
        call_set(rt, ctx, 0x08BC624Cu, {object + 0x3F8u});
        const u32 quaternion = object + 0x3F8u;
        put_float(rt, quaternion, one);
        put_float(rt, quaternion + 4u, one);
        put_float(rt, quaternion + 8u, one);
        put_float(rt, quaternion + 0xCu, half);
        call_set(rt, ctx, 0x08BCC9C8u, {object + 0x3F8u});
        call_set(rt, ctx, 0x08BCBD68u, {object + 0x408u});
        call_set(rt, ctx, 0x08BCBCDCu, {object + 0x40Cu});
        call_set(rt, ctx, 0x08BCBBECu, {object + 0x410u});
        call_set(rt, ctx, 0x08BCBAFCu, {object + 0x414u});
        call_set(rt, ctx, 0x08BCBA0Cu, {object + 0x418u});
        call_set(rt, ctx, 0x08BCB91Cu, {object + 0x41Cu});
        call_set(rt, ctx, 0x08BCB82Cu, {object + 0x420u});
        call_set(rt, ctx, 0x08BCB73Cu, {object + 0x424u});
        call_set(rt, ctx, 0x08BCB64Cu, {object + 0x428u});
        call_set(rt, ctx, 0x08BCB55Cu, {object + 0x42Cu});
        const float tenth = std::bit_cast<float>(0x3DCCCCCDu);  // 0.1
        put_float(rt, object + 0x42Cu, tenth);
        put_float(rt, object + 0x414u, tenth);
        put_float(rt, object + 0x41Cu, tenth);
        put_float(rt, object + 0x424u, tenth);
        put_float(rt, object + 0x428u, half);
        put_float(rt, object + 0x408u, one);
        put_float(rt, object + 0x40Cu, half);
        put_float(rt, object + 0x410u, half);
        put_float(rt, object + 0x418u, half);
        put_float(rt, object + 0x420u, half);
        call_set(rt, ctx, 0x08BCC93Cu, {object + 0x408u});
        call_set(rt, ctx, 0x08BC5B70u, {object + 0x430u});
        call_set(rt, ctx, 0x08BC5C54u, {object + 0x440u});
        const u32 vector = object + 0x430u;
        put(rt, vector, 0u);
        put(rt, vector + 4u, 0u);
        put(rt, vector + 8u, 0u);
        put_float(rt, vector + 0xCu, one);
        put_float(rt, object + 0x440u, one);
        call_set(rt, ctx, 0x08BCC84Cu, {object + 0x430u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x444u});
        call_set(rt, ctx, 0x08BCC75Cu, {object + 0x444u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x454u});
        call_set(rt, ctx, 0x08BCC66Cu, {object + 0x454u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x464u});
        call_set(rt, ctx, 0x08BCC57Cu, {object + 0x464u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x474u});
        call_set(rt, ctx, 0x08BCC48Cu, {object + 0x474u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x484u});
        call_set(rt, ctx, 0x08BCC39Cu, {object + 0x484u});
        call_set(rt, ctx, 0x08BBA094u, {object + 0x494u});
        call_set(rt, ctx, 0x08BCC2ACu, {object + 0x494u});
        call_set(rt, ctx, 0x08BBA1DCu, {object + 0x4A4u});
        call_set(rt, ctx, 0x08BCC1BCu, {object + 0x4A4u});
        call_set(rt, ctx, 0x08BC52E0u, {object + 0x4A8u});
        put_float(rt, object + 0x4A8u, hundred);
        call_set(rt, ctx, 0x08BC53B0u, {object + 0x4ACu});
        put_float(rt, object + 0x4ACu, thousand);
        call_set(rt, ctx, 0x08BC5480u, {object + 0x4B0u});
        put(rt, object + 0x4B0u, 0u);
        call_set(rt, ctx, 0x08BC5550u, {object + 0x4B4u});
        put8(rt, object + 0x4B4u, 0u);
        result = call_set(rt, ctx, 0x08BCC0CCu, {object + 0x4A8u});
    }
    ctx.gpr[2] = result;
    tail_call(ctx, 0x089000CCu, {object});
}

// The body 0x08BA82A8 / 0x08BA996C share: vector<{word +0, shared-pointer control block +4}>::insert(position, count, value).
// Nothing when count == 0. With room: the value is copied to a frame element (frame + 0xC, its control block's use count
// incremented); the elements after the position move up by count (the tail shift, then word and shared-pointer assignments
// backwards: the new block's count goes up, the old one is released) when more than `count` elements follow, else the
// spread / shift-tail / fill trio; the frame element is destroyed last. Full: size + max(size, count) elements (at most
// 0x1FFFFFFF), the three range helpers rebuild the vector, the old range is destroyed and its block freed, then begin /
// end / capacity are updated. v0 is the last call's result.
struct InsertOps {
    u32 tail_shift;    // 0x08BA8164 / 0x08BA9338
    u32 fill;          // 0x08BA7770 / 0x08BA9388
    u32 spread;        // 0x08BA8024 / 0x08BA91F8
    u32 shift_tail;    // 0x08BA8074 / 0x08BA9248
    u32 assign_range;  // 0x08BA8114 / 0x08BA92E8
    u32 fill_range;    // 0x08BA80C4 / 0x08BA9298
    u32 destroy_temp;  // 0x088875CC / 0x0888879C
    u32 destroy_old;   // 0x08B8E5D8 / 0x08B8E594
    u32 message;       // 0x08CA0718 / 0x08CA0740
};

void insert_vector(Runtime &rt, AllegrexContext &ctx, const InsertOps &ops) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;
    const u32 end = word(rt, vector + 4u);
    const u32 room = sra(word(rt, vector + 8u) - end, 3);
    if (room < count) {
        const u32 base = word(rt, vector);
        const u32 size = sra(end - base, 3);
        put(rt, frame.at(8u), size);
        if (0x1FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {ops.message});
        const u32 grow = size < count ? count : size;
        const u32 total = size + grow;
        u32 bytes = 0xFFFFFFF8u;
        u32 block = 0u;
        if (!(total < size)) bytes = total << 3;
        if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        const u32 front = call_set(rt, ctx, ops.assign_range, {word(rt, vector), position, block, frame.at(3u)});
        call_set(rt, ctx, ops.fill_range, {front, count, value, frame.at(2u)});
        const u32 back = call_set(rt, ctx, ops.assign_range, {position, word(rt, vector + 4u), front + (count << 3), frame.at(1u)});
        call_set(rt, ctx, ops.destroy_old, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
        ctx.gpr[2] = block + bytes;
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, back);
        put(rt, vector, block);
        return;
    }
    const u32 temporary = frame.at(0xCu);
    const u32 control = word(rt, value + 4u);
    put(rt, temporary + 4u, control);
    put(rt, temporary, word(rt, value));
    if (control != 0u) count_up(rt, ctx, control + 4u);
    const u32 begin = word(rt, vector + 4u);
    const u32 before = sra(begin - position, 3);
    if (count < before) {
        call_set(rt, ctx, ops.tail_shift, {begin - (count << 3), begin, begin, frame.at(6u)});
        const u32 source_end = begin - (count << 3);
        const u32 middle = sra(source_end - position, 3);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
        if (signed_of(middle) > 0) {
            u32 source = source_end;
            u32 destination = begin;
            for (u32 index = 0u; index != middle; ++index) {
                source -= 8u;
                destination -= 8u;
                const u32 moved = word(rt, source);
                const u32 previous = word(rt, destination + 4u);
                put(rt, destination, moved);
                const u32 next = word(rt, source + 4u);
                const u32 slot = destination + 4u;
                if (next == previous) continue;
                if (next != 0u) count_up(rt, ctx, next + 4u);
                const u32 old = word(rt, slot);
                if (old != 0u) release_shared(rt, ctx, old);
                put(rt, slot, next);
            }
        }
        call_set(rt, ctx, ops.fill, {position, position + (count << 3), temporary});
    } else {
        call_set(rt, ctx, ops.spread, {begin, count - before, temporary, frame.at(5u)});
        const u32 new_begin = word(rt, vector + 4u) + ((count - before) << 3);
        put(rt, vector + 4u, new_begin);
        call_set(rt, ctx, ops.shift_tail, {position, begin, new_begin, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (before << 3));
        call_set(rt, ctx, ops.fill, {position, begin, temporary});
    }
    call_set(rt, ctx, ops.destroy_temp, {temporary});
}

constexpr InsertOps kInsert82A8{0x08BA8164u, 0x08BA7770u, 0x08BA8024u, 0x08BA8074u,
                                0x08BA8114u, 0x08BA80C4u, 0x088875CCu, 0x08B8E5D8u, 0x08CA0718u};
constexpr InsertOps kInsert996C{0x08BA9338u, 0x08BA9388u, 0x08BA91F8u, 0x08BA9248u,
                                0x08BA92E8u, 0x08BA9298u, 0x0888879Cu, 0x08B8E594u, 0x08CA0740u};

void func_08BA82A8(Runtime &rt, AllegrexContext &ctx) { insert_vector(rt, ctx, kInsert82A8); }
void func_08BA996C(Runtime &rt, AllegrexContext &ctx) { insert_vector(rt, ctx, kInsert996C); }

// 0x08C0D100 (vector, position, value): insert of one 0x40-byte element {vtable +0, words +4/+8, floats +0xC..+0x1C,
// sub-objects +0x20 / +0x30} into the vector {block +0, begin +4, capacity end +8}. With room the element before begin is
// copied up to begin (0x089FA280 for its sub-objects) and begin moves to begin + 0x40; the value is copied to a frame
// element (vtable 0x08CCFC90 at frame + 4), the elements between position and begin - 0x80 shift up one slot (0x089FB570
// per sub-object) and the frame element is copied onto position. The temporaries are then destroyed (0x089F9B74) and the
// frame is released (0x089D6684). Full: max(size, 1) * 2 elements (0x08B5ADDC(0x08CA85BC) beyond 0x3FFFFFF,
// 0x08A5BC20 allocates), 0x08BE8588 / 0x08BE5988 rebuild the vector, the old block is freed (0x08A5BC54) and begin / end /
// capacity are updated with block + capacity. v0 is the last call's result (block + capacity on the full path).
// 0x08A930E8 (self, a1, container, list): first 0x08A7F3D0(self) runs. When *list is not list itself, a temporary string is
// built from the global 0x08CB1FD0 (0x08B85EB8) and compared with the node's key (word(node + 8), 0x08B36170 over the
// shorter length; a non-zero difference, or the length difference when the bytes match, returns) and the temporary rep is
// released (0x08B60804 / 0x08B8542C); when it is list, self->+0x38 who must exist and have word(+4) set. The insert then
// searches the container (0x08A6972C(key, 0x08CF6F08) into frame + 0x20, 0x08BFE7D0(container, key)): at the end slot or
// with key < word(found + 0x10) a new value is built from 0x08A5D438 / 0x08A6972C / 0x08A5D424 and inserted by
// 0x08BFEBAC(container, found, value), then the temporaries go (0x08A5B9EC / 0x08A69D68); otherwise the found node is the
// slot. The slot's key value is 0x08A924F0 / 0x08A91A38 (assign 0x08A5BAD8 unless the slot is the temporary), the node's
// 0x08B5C094 runs, its block is released (0x08B60804 / 0x08B8542C) and 0x08A5BC54(node, 0xC, 4) trims it; then the virtual
// slot 0x11C of word(self + 0x38) runs with (self + 0x38, a1, container, list), a string is built at frame + 0x10 from
// 0x08CB1FD0 and a 12-byte object (0x08A5BC20, 0x08B88F44 at +8, 0x08B5C078(obj, *list)) is stored, then that string is
// released. v0 is the last call's result.
void func_08A930E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 container = ctx.gpr[6];
    const u32 list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    call_set(rt, ctx, 0x08A7F3D0u, {self, first, container, list});
    const u32 node = word(rt, list);
    if (node != list) {
        const u32 temporary = frame.at(8u);
        call_set(rt, ctx, 0x08B85EB8u, {temporary, 0x08CB1FD0u, frame.at(3u)});
        const u32 text = word(rt, temporary);
        const u32 node_text = word(rt, node + 8u);
        const u32 node_length = word(rt, node_text - 0xCu);
        const u32 text_length = word(rt, text - 0xCu);
        put(rt, frame.at(0x24u), node_length);
        put(rt, frame.at(0x28u), text_length);
        const u32 shorter = text_length < node_length ? frame.at(0x28u) : frame.at(0x24u);
        const u32 length_difference = node_length - text_length;
        const u32 compare = call_set(rt, ctx, 0x08B36170u,
                                        {node_text, text, word(rt, shorter), shorter, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
        const u32 difference = compare != 0u ? compare : length_difference;
        const u32 rep = text - 0xCu;
        if (rep != 0x08D74FD0u) {
            const u32 stepped = call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
            if (signed_of(stepped) <= 0) call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(2u)});
        }
        if (difference != 0u) return;
    }
    const u32 holder = word(rt, self + 0x38u);
    if (holder == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, holder + 4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 key = frame.at(0x20u);
    call_set(rt, ctx, 0x08A6972Cu, {key, 0x08CF6F08u});
    const u32 found = call_set(rt, ctx, 0x08BFE7D0u, {container, key});
    u32 placed;
    if (found == container + 4u || word(rt, key) < word(rt, found + 0x10u)) {
        const u32 spare = frame.at(0xCu);
        const u32 scratch = frame.at(0x2Cu);
        call_set(rt, ctx, 0x08A5D438u, {spare});
        call_set(rt, ctx, 0x08A6972Cu, {scratch, key});
        call_set(rt, ctx, 0x08A5D424u, {frame.at(0x30u), spare});
        placed = call_set(rt, ctx, 0x08BFEBACu, {container, found, scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x30u)});
        call_set(rt, ctx, 0x08A69D68u, {scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {spare});
    } else {
        placed = found;
    }
    const u32 slot = placed + 0x14u;
    const u32 owner = word(rt, self + 0x38u);
    u32 selected = 0u;
    if (owner != 0u && word(rt, owner + 4u) != 0u) selected = owner;
    put(rt, frame.at(0x14u), selected);
    call_set(rt, ctx, 0x08A924F0u, {frame.at(0x18u), frame.at(0x14u)});
    call_set(rt, ctx, 0x08A91A38u, {frame.at(0x1Cu), key, frame.at(0x18u)});
    if (slot != frame.at(0x1Cu)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x1Cu))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    call_set(rt, ctx, 0x08A69D68u, {key});
    const u32 slot_object = word(rt, self + 0x38u);
    if (slot_object != 0u && word(rt, slot_object + 4u) != 0u) {
        call_virtual_at(rt, ctx, slot_object, 0x11Cu, {slot_object, first, container, list});
    }
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x10u), 0x08CB1FD0u, frame.at(4u)});
    const u32 target = word(rt, list);
    const u32 value = call_set(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
    if (value + 8u != 0u) call_set(rt, ctx, 0x08B88F44u, {value + 8u, frame.at(0x10u)});
    call_set(rt, ctx, 0x08B5C078u, {value, target});
    const u32 text = word(rt, frame.at(0x10u));
    const u32 rep = text - 0xCu;
    if (rep == 0x08D74FD0u) return;
    const u32 stepped = call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
    if (signed_of(stepped) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
}
} // namespace
} // namespace new_batch_main_1011
} // namespace gt::game

#define NEW1011(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1011::name, gt::ReturnKind::kind)
NEW1011(0x08883E74u, func_08883E74, Word);
NEW1011(0x08900A38u, func_08900A38, Word);
NEW1011(0x08BA82A8u, func_08BA82A8, Word);
NEW1011(0x08BA996Cu, func_08BA996C, Word);
NEW1011(0x08A930E8u, func_08A930E8, Word);
