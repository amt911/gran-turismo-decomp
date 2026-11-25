// New batch, main session 59: short-to-medium functions the recompiler emitted, none decompiled before, written by hand as
// structured C from `tools.disasm` (DRAFT: unverified until the synthetic gate runs). Families that repeat one body with
// other call targets share a helper: a range copy of 0x1C-byte records holding two 16-bit vectors (0x08BDE18C / 0x08BDE44C /
// 0x08BDEAD8), a shared-pointer vector erase (0x08BA7B0C / 0x08BA9498 / 0x08BA9710) and a vector insert with reallocation
// (0x08C47468 / 0x08C475C8). v0 is left as the original leaves it on every path (a `jal`'s delay slot runs before the callee).
// Atomic `ll`/`sc` counters are a plain read-modify-write here (one thread). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_59 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
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

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
s32 sra(u32 value, unsigned shift) { return static_cast<s32>(value) >> shift; }

// An atomic `ll`/`sc` add on one word (one thread): returns the old value.
u32 atomic_add(Runtime &rt, u32 address, u32 delta) {
    const u32 old = word(rt, address);
    put(rt, address, old + delta);
    return old;
}

// --- range copy of 0x1C-byte records (a0 = first, a1 = last, a2 = destination): each record is a word at +0 and two
// 16-bit vectors {begin, end, capacity} at +4 and +0x10; a non-null destination gets its word and both vectors rebuilt with
// `allocate` (size = the bytes of the source range) and `fill` (source begin, source end, new storage, frame byte).
// v0 = the destination after the last record.
void copy_records(Runtime &rt, AllegrexContext &ctx, u32 allocate, u32 fill_first, u32 fill_second) {
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    u32 dest = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10), last);
    if (last != first) {
        u32 source = first;
        for (;;) {
            const u32 next_source = source + 0x1Cu;
            if (dest != 0u) {
                put(rt, dest, word(rt, source));
                u32 bytes = 0u;
                u32 storage = 0u;
                const s32 units = sra(word(rt, source + 8u) - word(rt, source + 4u), 1);
                put(rt, dest + 4u, 0u);
                put(rt, dest + 8u, 0u);
                put(rt, dest + 0xCu, 0u);
                if (units != 0) {
                    bytes = static_cast<u32>(units) << 1;
                    storage = call(rt, ctx, allocate, {bytes});
                }
                put(rt, dest + 8u, storage);
                put(rt, dest + 4u, storage);
                put(rt, dest + 0xCu, storage + bytes);
                const u32 filled = call(rt, ctx, fill_first, {word(rt, source + 4u), word(rt, source + 8u), storage, frame.at(1)});
                put(rt, dest + 8u, filled);

                bytes = 0u;
                storage = 0u;
                const s32 second_units = sra(word(rt, source + 0x14u) - word(rt, source + 0x10u), 1);
                put(rt, dest + 0x10u, 0u);
                put(rt, dest + 0x14u, 0u);
                put(rt, dest + 0x18u, 0u);
                if (second_units != 0) {
                    bytes = static_cast<u32>(second_units) << 1;
                    storage = call(rt, ctx, allocate, {bytes});
                }
                put(rt, dest + 0x14u, storage);
                put(rt, dest + 0x10u, storage);
                put(rt, dest + 0x18u, storage + bytes);
                const u32 second_filled =
                    call(rt, ctx, fill_second, {word(rt, source + 0x10u), word(rt, source + 0x14u), storage, frame.at(0)});
                put(rt, dest + 0x14u, second_filled);
            }
            source = next_source;
            const bool more = next_source != word(rt, frame.at(0x10));
            dest += 0x1Cu;
            if (!more) break;
        }
    }
    ctx.gpr[2] = dest;
}


// --- erase from a vector of 8-byte shared pointers (a0 = the vector, a1 = the first erased, a2 = the first kept): the kept
// tail is assigned down one element at a time (a counted pointer: +4 strong count, +8 weak count, vtable slots 8 / 0xC run when
// a count reaches zero), then `destroy_tail(a1 + moved, end, frame)` runs and the end shrinks by the moved bytes. v0 = a1.
void erase_shared(Runtime &rt, AllegrexContext &ctx, u32 destroy_tail) {
    const u32 vector = ctx.gpr[4];
    const u32 erase_first = ctx.gpr[5];
    const u32 keep_first = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x10), keep_first);
    const s32 count = sra(word(rt, vector + 4u) - keep_first, 3);
    u32 dest = erase_first;
    u32 source = keep_first;
    for (s32 index = 0; index < count; ++index) {
        const u32 value = word(rt, source);
        const u32 previous_control = word(rt, dest + 4u);
        put(rt, dest, value);
        const u32 control = word(rt, source + 4u);
        if (control != previous_control) {
            if (control != 0u) atomic_add(rt, control + 4u, 1u);
            const u32 old_control = word(rt, dest + 4u);
            if (old_control == 0u) {
                put(rt, dest + 4u, control);
            } else if (atomic_add(rt, old_control + 4u, 0xFFFFFFFFu) != 1u) {
                put(rt, dest + 4u, control);
            } else {
                call_virtual(rt, ctx, old_control, 8u, {old_control});
                if (atomic_add(rt, old_control + 8u, 0xFFFFFFFFu) != 1u) {
                    put(rt, dest + 4u, control);
                } else {
                    call_virtual(rt, ctx, old_control, 0xCu, {old_control});
                    put(rt, dest + 4u, control);
                }
            }
        }
        source += 8u;
        dest += 8u;
    }
    const u32 end = word(rt, vector + 4u);
    call(rt, ctx, destroy_tail, {dest, end, frame.at(0)});
    const u32 moved = (word(rt, frame.at(0x10)) - erase_first) & ~7u;
    put(rt, vector + 4u, word(rt, vector + 4u) - moved);
    ctx.gpr[2] = erase_first;
}


// --- vector insert of one word (a0 = the vector {begin, end, capacity}, a1 = the position, a2 = a pointer to the value):
// room left: the end element moves up (when the end is non-null), the tail is moved with `move_tail` and the value stored at
// the position; full: the storage grows to twice the size (1 when empty) with the allocator, the head and the tail are copied
// with `copy_part`, `finish` runs, the old block is freed and the three words are updated. v0 as the original's.
struct InsertPolicy {
    u32 move_tail;
    u32 copy_part;
    u32 finish;
    u32 allocate;        // (bytes) or (bytes, 4)
    bool allocate_aligned;
    u32 release;         // (block) or (block, bytes, 4)
    bool release_sized;
    u32 length_error;    // the text 0x08B5ADDC reports when the size is 0x3FFFFFFF
    bool skip_empty;     // a doubled size of 0 allocates nothing (0x08C47468 shape)
};
void insert_word(Runtime &rt, AllegrexContext &ctx, const InsertPolicy &policy) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value_pointer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    u32 end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) {
            put(rt, end, word(rt, end - 4u));
            end = word(rt, vector + 4u);
        }
        const u32 moved_end = end;
        put(rt, vector + 4u, end + 4u);
        const u32 bytes = ((moved_end - 4u) - position) & ~3u;
        const u32 value = word(rt, value_pointer);
        call(rt, ctx, policy.move_tail, {moved_end - bytes, position, bytes});
        put(rt, position, value);
        return;
    }
    u32 begin = word(rt, vector);
    const u32 size = static_cast<u32>(sra(end - begin, 2));
    if (size == 0x3FFFFFFFu) call(rt, ctx, 0x08B5ADDCu, {policy.length_error});
    u32 bytes = 4u;
    u32 storage = 0u;
    bool allocate = true;
    if (size != 0u) {
        const u32 doubled = size << 1;
        if (doubled < size) {
            bytes = 0xFFFFFFFCu;
        } else if (policy.skip_empty && doubled == 0u) {
            bytes = 0u;
            allocate = false;
        } else {
            bytes = doubled << 2;
        }
    }
    if (allocate) {
        if (policy.allocate_aligned) storage = call(rt, ctx, policy.allocate, {bytes, 4u});
        else storage = call(rt, ctx, policy.allocate, {bytes});
        begin = word(rt, vector);
    }
    u32 slot = call(rt, ctx, policy.copy_part, {begin, position, storage, frame.at(2)});
    if (slot != 0u) put(rt, slot, word(rt, value_pointer));
    slot += 4u;
    const u32 tail_end = call(rt, ctx, policy.copy_part, {position, word(rt, vector + 4u), slot, frame.at(1)});
    call(rt, ctx, policy.finish, {word(rt, vector), word(rt, vector + 4u), frame.at(0)});
    const u32 old_block = word(rt, vector);
    const s32 old_units = sra(word(rt, vector + 8u) - old_block, 2);
    if (old_block != 0u) {
        if (policy.release_sized) call(rt, ctx, policy.release, {old_block, static_cast<u32>(old_units) << 2, 4u});
        else call(rt, ctx, policy.release, {old_block});
    }
    const u32 limit = storage + bytes;
    ctx.gpr[2] = limit;
    put(rt, vector + 8u, limit);
    put(rt, vector + 4u, tail_end);
    put(rt, vector, storage);
}

void func_08C47468(Runtime &rt, AllegrexContext &ctx) {
    insert_word(rt, ctx, {0x08B79BACu, 0x08C46FE4u, 0x08C46FD4u, 0x08B35DA0u, false, 0x08B35254u, false, 0x08CB4AD4u, true});
}
void func_08C475C8(Runtime &rt, AllegrexContext &ctx) {
    insert_word(rt, ctx, {0x08B79BACu, 0x08C46F64u, 0x08BBB8F8u, 0x08B35DA0u, false, 0x08B35254u, false, 0x08CB4AD4u, true});
}
// 0x08C2B0D4 / 0x08BFB9D8: the same insert with the sized allocator 0x08A5BC20(bytes, 4) and release 0x08A5BC54(block, bytes, 4).
void func_08C2B0D4(Runtime &rt, AllegrexContext &ctx) {
    insert_word(rt, ctx, {0x08B79BACu, 0x08C2AAD0u, 0x08C2AAC0u, 0x08A5BC20u, true, 0x08A5BC54u, true, 0x08CB1CA0u, false});
}
void func_08BFB9D8(Runtime &rt, AllegrexContext &ctx) {
    insert_word(rt, ctx, {0x08B79BACu, 0x08BF91E8u, 0x08BF91D8u, 0x08A5BC20u, true, 0x08A5BC54u, true, 0x08CA5140u, false});
}

// The atomic counters as the compiler left the registers: `ll`/`sc` leave v0 = 1 and v1 = the address; a decrement also leaves the old value in a0.
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    atomic_add(rt, address, 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
}
u32 count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = atomic_add(rt, address, 0xFFFFFFFFu);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    return old;
}

// 0x088FEFC0 (a0 = a counted pointer {pointer, control}, a1 = the destination one): the destination takes the pointer and the
// control block (strong count +4, weak count +8, vtable slots 8 / 0xC run when a count reaches zero; the old control is released),
// then the temporary reference this function took on the source's control is released; the last release ends in a tail call of
// vtable slot 0xC. v0 as the registers of the original's last operation leave it.
void func_088FEFC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4];
    const u32 dest = ctx.gpr[5];
    u32 control = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        control = word(rt, source + 4u);
        const u32 pointer = word(rt, source);
        if (control != 0u) count_up(rt, ctx, control + 4u);
        const u32 old_control = word(rt, dest + 4u);
        ctx.gpr[2] = old_control;
        put(rt, dest, pointer);
        if (control != old_control) {
            if (control != 0u) count_up(rt, ctx, control + 4u);
            const u32 held = word(rt, dest + 4u);
            if (held == 0u) {
                put(rt, dest + 4u, control);
            } else if (count_down(rt, ctx, held + 4u) != 1u) {
                put(rt, dest + 4u, control);
            } else {
                call_virtual(rt, ctx, held, 8u, {held});
                if (count_down(rt, ctx, held + 8u) != 1u) {
                    put(rt, dest + 4u, control);
                } else {
                    call_virtual(rt, ctx, held, 0xCu, {held});
                    put(rt, dest + 4u, control);
                }
            }
        }
        if (control == 0u) return;
        if (count_down(rt, ctx, control + 4u) != 1u) return;
        call_virtual(rt, ctx, control, 8u, {control});
        if (count_down(rt, ctx, control + 8u) != 1u) return;
        ctx.gpr[2] = word(rt, control);
    }
    tail_call(ctx, word(rt, ctx.gpr[2] + 0xCu), {control});
}

// 0x08C7A274 (a0 = a stream object, a1 = its construction table, a2, a3 = the arguments of the final 0x08C7835C): the object
// takes the vtable words of the table (a1+4 for the object, a1+0x18-style values through the offsets stored in the vtables),
// 0x08C54A7C initialises the virtual base with a null buffer, 0x08C771D4 the member at +0xC, 0x08C7835C(member, a2, a3) decides:
// success calls 0x08C54CFC(base, 0), failure 0x08C54CFC(base, state | 4). v0 = the last call's.

// 0x08B5BE8C (a0 = a force flag): the byte at 0x08CE0E0C says the three streams are still to be set up; a set flag or a clear
// byte answers the byte. Otherwise 0x08B5BB0C(frame) runs, the byte is cleared, three vtable words at 0x08D72930 / 0x08D72908 /
// 0x08D728E0 are set to 0x08CDBDF0 and the members at +0x1C are initialised with 0x08B5D34C, three buffers are filled from the
// words 8 / 4 / 0xC of the object 0x08B796C4 answers (0x08C54438), four stream pairs are joined with 0x08C54D44 and
// 0x08B5B714(frame) closes. v0 = the byte.
void func_08B5BE8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 force = ctx.gpr[4] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    const u32 pending = byte_at(rt, 0x08CE0E0Cu);
    if (force != 0u || pending == 0u) {
        ctx.gpr[2] = pending;
        return;
    }
    call(rt, ctx, 0x08B5BB0Cu, {frame.at(0)});
    put(rt, 0x08D72930u, 0x08CDBDF0u);
    put_byte(rt, 0x08CE0E0Cu, 0u);
    call(rt, ctx, 0x08B5D34Cu, {0x08D72930u + 0x1Cu});
    put(rt, 0x08D72908u, 0x08CDBDF0u);
    call(rt, ctx, 0x08B5D34Cu, {0x08D72908u + 0x1Cu});
    put(rt, 0x08D728E0u, 0x08CDBDF0u);
    call(rt, ctx, 0x08B5D34Cu, {0x08D728E0u + 0x1Cu});
    u32 object = call(rt, ctx, 0x08B796C4u);
    call(rt, ctx, 0x08C54438u, {0x08D7283Cu, word(rt, object + 8u), 0x10u, 0x400u});
    object = call(rt, ctx, 0x08B796C4u);
    call(rt, ctx, 0x08C54438u, {0x08D72798u, word(rt, object + 4u), 8u, 0x400u});
    object = call(rt, ctx, 0x08B796C4u);
    call(rt, ctx, 0x08C54438u, {0x08D726F4u, word(rt, object + 0xCu), 0x10u, 0x400u});
    call(rt, ctx, 0x08C54D44u, {0x08D72A74u, 0x08D7283Cu, 0x10u, 0x400u});
    call(rt, ctx, 0x08C54D44u, {0x08D72B04u, 0x08D72798u, 0x10u, 0x400u});
    call(rt, ctx, 0x08C54D44u, {0x08D729E8u, 0x08D726F4u, 0x10u, 0x400u});
    call(rt, ctx, 0x08C54D44u, {0x08D7295Cu, 0x08D726F4u, 0x10u, 0x400u});
    call(rt, ctx, 0x08B5B714u, {frame.at(0)});
    ctx.gpr[2] = pending;
}

// A 64-bit value (low, high) made exact or halved: 0x08B6EE4C(value, 0) answers its sign; positive goes straight to
// 0x08B6EE80(value); otherwise the value is halved with the lost bit kept (logical shifts), passed to 0x08B6EE80 and finished with
// 0x08B71328(result, result). Returns the result pair; the registers a2 / a3 the original leaves are the pair too.
struct Pair {
    u32 low;
    u32 high;
};
Pair scale_pair(Runtime &rt, AllegrexContext &ctx, u32 low, u32 high) {
    const GuestResult sign = call_full(rt, ctx, 0x08B6EE4Cu, {low, high, 0u, 0u});
    if (static_cast<s32>(sign.v0) > 0) {
        const GuestResult exact = call_full(rt, ctx, 0x08B6EE80u, {low, high, 0u, 0u});
        return {exact.v0, exact.v1};
    }
    const u32 carried = high << 31;
    const u32 half_low = (low >> 1) | carried;
    const u32 half_high = high >> 1;
    const GuestResult rounded = call_full(rt, ctx, 0x08B6EE80u, {(low & 1u) | half_low, half_high, carried, 0u});
    const GuestResult finished = call_full(rt, ctx, 0x08B71328u, {rounded.v0, rounded.v1, rounded.v0, rounded.v1});
    return {finished.v0, finished.v1};
}

// 0x08A5E4E0 (a0 = an object with a 64-bit value at +0x18, a1 = a holder to receive the result, a3 = a holder that owns a
// second object): scales the object's value, scales the 64-bit answer of the second object's vtable slot 0x50, combines both with
// 0x08B6683C and 0x08B6F1B8, builds the result in a frame holder with 0x08A5DEE8 and assigns it to a1 (0x08A5BAD8) unless it is
// the frame's own, then releases the frame holder (0x08A5B9EC). A null second holder is reported first (0x08A5BB7C).

// 0x08844EF0 (a0 = an object): fills a string holder on the frame with 0x08B85EB8(holder, 0x08C9C948, frame + 1), runs the
// object's vtable slot 0x108 on it (the slot is read before that call), releases the holder's buffer unless it is the shared
// empty one (0x08D74FD0; 0x08B60804 / 0x08B8542C), replaces the object's word at +0x38 with the object 0x08A76A94 returns
// (0x08A5BA08 takes a reference to the new one, 0x08A5B930 drops the old) and registers one property (0x08B8850C) and eight
// handlers (0x08B87108) on it. v0 = the last call's.
void func_08844EF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 target = word(rt, word(rt, self) + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4), 0x08C9C948u, frame.at(1)});
    ctx.gpr[2] = target;
    call(rt, ctx, target, {self, frame.at(4)});
    const u32 text = word(rt, frame.at(4));
    const u32 header = text - 0xCu;
    if (header != 0x08D74FD0u) {
        const s32 count = static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}));
        if (!(count > 0)) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0)});
    }
    const u32 created = call(rt, ctx, 0x08A76A94u);
    if (created != 0u) call(rt, ctx, 0x08A5BA08u, {created});
    const u32 old = word(rt, self + 0x38u);
    if (old != 0u) call(rt, ctx, 0x08A5B930u, {old});
    put(rt, self + 0x38u, created);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08845494u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9A0u, 0x0884463Cu});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9ACu, 0x08844644u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9B8u, 0x08844924u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9C0u, 0x08844C88u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9CCu, 0x08844D1Cu});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9D4u, 0x08844828u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9E0u, 0x088451C0u});
    call(rt, ctx, 0x08B87108u, {self, 0x08C9C9F8u, 0x0884509Cu});
}

// 0x08AC6FE0 (a0 = an object, a1, a2 = the arguments of the open call 0x08C7AB50): marks the object busy (byte +0x28), opens
// a handle (stored at +0x2C), validates it with 0x08C7AB80, reads its size (0x08C7AB68 into a frame word; a negative size counts
// as 0) and its other values (0x08C7AB78, 0x08C7AB70) into the object (+0x30 = 2, +0x34, +0x38 = size / 2, +0x3C/+0x40/+0x44);
// when the three values of the last query are inconsistent the handle is told -1 (0x08C7AB60). Any failure answers 0 with the
// busy flag left set, success answers 1 with it cleared; 0x08B3457C(object) runs last. v0 = the answer.

// 0x08A50400 (a0 = an object): two tables of eleven words copied from 0x08CDF72C / 0x08CDF700 onto the frame, each with its
// first word replaced by the object (the second one in the delay slot of the first call), are resolved into the words at
// +0 and +4 with 0x08C7AC98(table, &error) (a non-zero error word ends it); then the objects at +8 (0x14 bytes, built with
// 0x08A4FD08 and initialised with 0x08A4FB80 / 0x08A4FA18) and +0xC (0x60 bytes, 0x08A50EC0 and 0x08A511E0(object, word +4))
// are allocated with 0x08B62344, after 0x08A4F4D8(). v0 = the last call's.
void copy_words(Runtime &rt, u32 source, u32 target, unsigned count) {
    for (unsigned index = 0; index < count; ++index) put(rt, target + index * 4u, word(rt, source + index * 4u));
}

// 0x08A8EC08 / 0x08A8ED98 (a0 = an object): destructor of a holder object. The vtable word is set to 0x08CCD480, two holders
// (+0x20, +0x24) are assigned from fresh ones made with 0x08AAE204(frame holder, 0x08CF6ED8) and released (0x08A5BAD8 /
// 0x08A5B9EC), the text at +0x74 is released unless it is the shared empty one (0x08D74FD0; 0x08B60804 / 0x08B8542C), the members
// at +0x6C ... +0x28 are destroyed (0x08C2AB00, 0x08C2AB4C, 0x08C2AB98, 0x08C2AA30, 0x08C2B458, 0x08C2AF5C, 0x08A5B9EC), the two
// word arrays at +0x44 and +0x14 are destroyed and freed (0x08C2AA30 / 0x08C2AD44, 0x08A5BC54 with the byte size), and 0x08A780B0
// ends it. The deleting variant (0x08A8ED98) then frees the object itself: 0x08A5BC54(object, 0x7C, 4). v0 = the last call's.
void destroy_holder_object(Runtime &rt, AllegrexContext &ctx, bool deleting) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 first_holder = self + 0x20u;
    const u32 second_holder = self + 0x24u;
    put(rt, self, 0x08CCD480u);
    call(rt, ctx, 0x08AAE204u, {frame.at(8), 0x08CF6ED8u});
    if (first_holder != frame.at(8)) call(rt, ctx, 0x08A5BAD8u, {first_holder, word(rt, frame.at(8))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8)});
    call(rt, ctx, 0x08AAE204u, {frame.at(4), 0x08CF6ED8u});
    if (second_holder != frame.at(4)) call(rt, ctx, 0x08A5BAD8u, {second_holder, word(rt, frame.at(4))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4)});
    const u32 text = word(rt, self + 0x74u);
    const u32 header = text - 0xCu;
    if (header != 0x08D74FD0u) {
        const s32 count = static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}));
        if (!(count > 0)) call(rt, ctx, 0x08B8542Cu, {header, frame.at(2)});
    }
    call(rt, ctx, 0x08C2AB00u, {self + 0x6Cu});
    call(rt, ctx, 0x08C2AB4Cu, {self + 0x64u});
    call(rt, ctx, 0x08C2AB4Cu, {self + 0x5Cu});
    call(rt, ctx, 0x08C2AB98u, {self + 0x54u});
    call(rt, ctx, 0x08C2AA30u, {word(rt, self + 0x44u), word(rt, self + 0x48u), frame.at(1)});
    u32 array = word(rt, self + 0x44u);
    s32 units = sra(word(rt, self + 0x4Cu) - array, 2);
    if (array != 0u) call(rt, ctx, 0x08A5BC54u, {array, static_cast<u32>(units) << 2, 4u});
    call(rt, ctx, 0x08C2B458u, {self + 0x3Cu});
    call(rt, ctx, 0x08C2B458u, {self + 0x34u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x30u});
    call(rt, ctx, 0x08C2AF5Cu, {self + 0x28u});
    call(rt, ctx, 0x08A5B9ECu, {second_holder});
    call(rt, ctx, 0x08A5B9ECu, {first_holder});
    call(rt, ctx, 0x08C2AD44u, {word(rt, self + 0x14u), word(rt, self + 0x18u), frame.at(0)});
    array = word(rt, self + 0x14u);
    units = sra(word(rt, self + 0x1Cu) - array, 2);
    if (array != 0u) call(rt, ctx, 0x08A5BC54u, {array, static_cast<u32>(units) << 2, 4u});
    call(rt, ctx, 0x08A780B0u, {self});
    if (deleting) call(rt, ctx, 0x08A5BC54u, {self, 0x7Cu, 4u});
}

//@@NEXT@@

} // namespace
} // namespace new_batch_main_59
} // namespace gt::game

#define NEW_MAIN59(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_59::name, gt::ReturnKind::kind)
NEW_MAIN59(0x08C47468u, func_08C47468, Word);
NEW_MAIN59(0x08C475C8u, func_08C475C8, Word);
NEW_MAIN59(0x08C2B0D4u, func_08C2B0D4, Word);
NEW_MAIN59(0x08BFB9D8u, func_08BFB9D8, Word);
NEW_MAIN59(0x088FEFC0u, func_088FEFC0, Word);
NEW_MAIN59(0x08B5BE8Cu, func_08B5BE8C, Word);
NEW_MAIN59(0x08844EF0u, func_08844EF0, Word);
