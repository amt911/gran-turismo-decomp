// New batch main 1002: functions drafted before that mismatched the original in the gate (or were skipped twice),
// written by hand as structured C from `tools.disasm`. Draft only: scripts verify later.
// Each body keeps the original's v0 on every exit path, every argument register a callee reads, the delay slot of a
// `jal` / `jalr` before the call, and the frame size of the original's prologue. No names: none has evidence.
// 0x08B26EB8 was read but not drafted: its epilogue returns $t1 (`addu v0, t1, zero`) and the paths that never set it
// (the initial node with +6 < 0, and fp == s5) would return whatever the callees left in $t1, which a call_guest
// boundary cannot reproduce.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1002 {
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
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_s(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 max_signed(u32 a, u32 b) { return static_cast<u32>(std::max(signed_of(a), signed_of(b))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 = the vtable, v0 = the slot's function before the call.
inline GuestResult call_vtable(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
// `lw v1, offset(v0); jalr v1` with v0 already holding the table: v0 stays the table, v1 the function.
inline GuestResult call_slot_of_table(Runtime &rt, AllegrexContext &ctx, u32 table, u32 offset, std::initializer_list<u32> arguments) {
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    ctx.gpr[3] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
inline u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// 0x088CD2FC (a0 = object, a1 = index): builds the normal/tangent frame of element `index` (stride 40 at +0x30) from its own
// floats and the next element's (index+1, or index-35 when that reaches 0x24), normalises both, and writes three vectors at
// +0x640, +0x64C and +0x658 of the 36-byte slot. The 0x46 element takes the other operand order. v0 = object + index * 40.
// Fix over the old draft: the element floats are +8 / +0xC / +0x10 (the old draft read +0x14 / +0x18).
void func_088CD2FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    const u32 s7 = index << 3;
    const u32 s6 = index << 5;
    const u32 element = object + (s7 + s6 + 0x30u);
    const u32 limit = (signed_of(index + 1u) < 0x24) ? (index + 1u) : (index - 0x23u);
    const u32 next = object + (limit * 40u + 0x30u);
    const float one = 1.0f;

    const float f7 = float_at(rt, element + 8u);
    const float f6 = float_at(rt, element + 0xCu);
    const float f5 = float_at(rt, element + 0x10u);
    const float f4 = float_at(rt, next + 8u);
    const float f3 = float_at(rt, next + 0xCu);
    const float f2 = float_at(rt, next + 0x10u);
    put_float(rt, frame.at(0x48u), f7);
    put_float(rt, frame.at(0x4Cu), f6);
    put_float(rt, frame.at(0x50u), f5);
    put_float(rt, frame.at(0x54u), f4);
    put_float(rt, frame.at(0x58u), f3);
    put_float(rt, frame.at(0x5Cu), f2);
    put_float(rt, frame.at(0x60u), float_at(rt, next + 0x14u));
    put_float(rt, frame.at(0x64u), float_at(rt, next + 0x18u));
    put_float(rt, frame.at(0x68u), float_at(rt, next + 0x1Cu));

    if (index == 0x46u) {
        put_float(rt, frame.at(0x3Cu), f4);
        put_float(rt, frame.at(0x40u), f3);
        put_float(rt, frame.at(0x44u), f2);
        (void)call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x3Cu), frame.at(0x3Cu), frame.at(0x48u)});
        put_float(rt, frame.at(0x78u), float_at(rt, frame.at(0x3Cu)));
        put_float(rt, frame.at(0x7Cu), float_at(rt, frame.at(0x40u)));
        put_float(rt, frame.at(0x80u), float_at(rt, frame.at(0x44u)));
    } else {
        put_float(rt, frame.at(0x30u), f7);
        put_float(rt, frame.at(0x34u), f6);
        put_float(rt, frame.at(0x38u), f5);
        (void)call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x30u), frame.at(0x30u), frame.at(0x54u)});
        put_float(rt, frame.at(0x78u), float_at(rt, frame.at(0x30u)));
        put_float(rt, frame.at(0x7Cu), float_at(rt, frame.at(0x34u)));
        put_float(rt, frame.at(0x80u), float_at(rt, frame.at(0x38u)));
    }

    const float normal_length = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {frame.at(0x78u)}, {}).f0);
    (void)call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x78u)}, {one / normal_length});
    const float second_length = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {frame.at(0x60u)}, {}).f0);
    (void)call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x60u)}, {one / second_length});
    (void)call_set(rt, ctx, 0x08B4A9D0u, {frame.at(0x84u), frame.at(0x60u), frame.at(0x78u)});

    put_float(rt, frame.at(0x6Cu), float_at(rt, frame.at(0x84u)));
    put_float(rt, frame.at(0x70u), float_at(rt, frame.at(0x88u)));
    put_float(rt, frame.at(0x74u), float_at(rt, frame.at(0x8Cu)));
    const u32 slot = (index << 2) + s6;
    const u32 out0 = object + (slot + 0x640u);
    const u32 out1 = object + (slot + 0x64Cu);

    const float third_length = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {frame.at(0x6Cu)}, {}).f0);
    (void)call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x6Cu)}, {one / third_length});
    put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x6Cu)));
    put_float(rt, frame.at(0x1Cu), float_at(rt, frame.at(0x70u)));
    put_float(rt, frame.at(0x20u), float_at(rt, frame.at(0x74u)));
    (void)call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x18u)}, {float_at(rt, object + 0x604u)});

    put_float(rt, frame.at(0x24u), float_at(rt, frame.at(0x48u)));
    put_float(rt, frame.at(0x28u), float_at(rt, frame.at(0x4Cu)));
    put_float(rt, frame.at(0x2Cu), float_at(rt, frame.at(0x50u)));
    (void)call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x24u), frame.at(0x24u), frame.at(0x18u)});
    put_float(rt, out0 + 4u, float_at(rt, frame.at(0x28u)));
    put_float(rt, out0 + 0u, float_at(rt, frame.at(0x24u)));
    put_float(rt, out0 + 8u, float_at(rt, frame.at(0x2Cu)));

    put_float(rt, frame.at(0x00u), float_at(rt, frame.at(0x6Cu)));
    put_float(rt, frame.at(0x04u), float_at(rt, frame.at(0x70u)));
    put_float(rt, frame.at(0x08u), float_at(rt, frame.at(0x74u)));
    (void)call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x00u)}, {float_at(rt, object + 0x604u)});

    put_float(rt, frame.at(0x0Cu), float_at(rt, frame.at(0x48u)));
    put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0x4Cu)));
    put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0x50u)));
    (void)call_set(rt, ctx, 0x08B4AA34u, {frame.at(0x0Cu), frame.at(0x0Cu), frame.at(0x00u)});
    put_float(rt, out1 + 4u, float_at(rt, frame.at(0x10u)));
    put_float(rt, out1 + 0u, float_at(rt, frame.at(0x0Cu)));
    put_float(rt, out1 + 8u, float_at(rt, frame.at(0x14u)));
    put_float(rt, object + slot + 0x658u, float_at(rt, object + index * 40u + 0x58u));
    ctx.gpr[2] = object + index * 40u;
}

// 0x08AB6BA0 (a0 = output context, a1 = count, a2 = holder of the source object): when count <= 0 nothing happens (v0 is
// left alone). Otherwise it takes over the object at *a2 (null is reported through 0x08A5BB7C), asks its virtual slot 0xC
// for a handle (a0 = frame + 0x1C), builds a 0x3C-byte record through 0x08B0EC60, copies it to sp+0x58, builds a six-entry
// container at sp+0x20 through 0x08A98C1C and releases six iterator temporaries looked up from the container's +0x14
// (keys: the signed half at record +0x4C, then the bytes at +0x4E .. +0x52), then releases the container and finally the
// handle at frame + 0x1C through the 0x08D74FD0-sentinel refcount protocol. v0 = the last call's on the full path.
void func_08AB6BA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x110u);
    if (signed_of(count) <= 0) return;

    u32 object = word(rt, holder);
    if (object == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB419Cu});
    }
    const u32 table = word(rt, object);
    (void)call_slot_of_table(rt, ctx, table, 0xCu, {frame.at(0x1Cu), object});

    (void)call_set(rt, ctx, 0x08B0EC60u, {frame.at(0xA0u), word(rt, frame.at(0x1Cu))});
    for (u32 k = 0u; k < 0x3Cu; k += 4u) {
        put(rt, frame.at(0x58u + k), word(rt, frame.at(0xA0u + k)));
    }

    const u32 record_0 = word(rt, frame.at(0x7Cu));
    const u32 record_1 = word(rt, frame.at(0x80u));
    const u32 record_2 = word(rt, frame.at(0x84u));
    const u32 record_3 = word(rt, frame.at(0x88u));
    const u32 record_4 = word(rt, frame.at(0x8Cu));
    const u32 record_5 = word(rt, frame.at(0x90u));
    put(rt, frame.at(0x4Cu), record_4);
    put(rt, frame.at(0x50u), record_5);
    put(rt, frame.at(0x3Cu), record_0);
    put(rt, frame.at(0x24u), record_0);
    put(rt, frame.at(0x40u), record_1);
    put(rt, frame.at(0x44u), record_2);
    put(rt, frame.at(0x48u), record_3);
    put(rt, frame.at(0x28u), record_1);
    put(rt, frame.at(0x2Cu), record_2);
    put(rt, frame.at(0x30u), record_3);
    put(rt, frame.at(0x34u), record_4);
    put(rt, frame.at(0x38u), record_5);

    const u32 key0 = half_s(rt, frame.at(0x4Cu));
    const u32 key1 = byte_u(rt, frame.at(0x4Eu));
    const u32 key2 = byte_u(rt, frame.at(0x4Fu));
    const u32 key3 = byte_u(rt, frame.at(0x50u));
    const u32 key4 = byte_u(rt, frame.at(0x51u));
    const u32 key5 = byte_u(rt, frame.at(0x52u));
    (void)call_set(rt, ctx, 0x08A98C1Cu, {frame.at(0x20u), 6u});

    const u32 keys[6] = {key0, key1, key2, key3, key4, key5};
    const u32 temporaries[6] = {0x18u, 0x14u, 0x10u, 0x0Cu, 0x08u, 0x04u};
    for (unsigned i = 0u; i < 6u; ++i) {
        const u32 container = word(rt, frame.at(0x20u));
        const u32 base = word(rt, container + 0x14u) + i * 4u;
        const u32 temporary = frame.at(temporaries[i]);
        (void)call_set(rt, ctx, 0x08A85B1Cu, {temporary, keys[i]});
        if (base != temporary) {
            (void)call_set(rt, ctx, 0x08A5BAD8u, {base, word(rt, temporary)});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }

    if (out != frame.at(0x20u)) {
        (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x20u))});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x20u)});

    const u32 handle = word(rt, frame.at(0x1Cu));
    u32 v0 = 0x08D74FD0u;
    const u32 block = handle - 0xCu;
    if (block != 0x08D74FD0u) {
        v0 = call_set(rt, ctx, 0x08B60804u, {handle - 4u, 0xFFFFFFFFu});
        if (signed_of(v0) <= 0) {
            v0 = call_set(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
        }
    }
    ctx.gpr[2] = v0;
}

// 0x088676F0 (a0 = object, a1 = argument of 0x08893788, a2 = {begin, end} of 8-byte elements, a3 = counter word,
// t0 = incremented counter word, t1 = an element id): for each element, the value at object + element*4 + 0x4260 moves
// toward a weight derived from |element[0]| (clamped to [0,1], scaled by 0.4 when 4+ elements remain and zeroed when
// 6+ remain), *t0 is incremented and, while the value is positive and *a3 < 6, up to four slots of the tables at
// +0x22C4 / +0x22F4 / +0x2644 / +0x265C / +0x2674 are filled. v0 = the path's last value, see each exit.

// 0x088D8CA8 (a0 = destination object, a1 = source record): copies the four (plain pointer, reference-counted pointer)
// pairs at +0, +8, +0x10 and +0x18 of the source into +0x678 .. +0x694 of the destination. The counted half is
// retained (incremented at block+4) before the old one is released: decrement block+4 and, when it reached one, call
// the block's virtual 0x8; then decrement block+8 and, when it reached one, call the virtual 0xC. Ends in a tail call
// to 0x08B90024(destination + 0x698, source + 0x20).

// The record copy of 0x08AD0C9C: the asm copies 16-byte blocks from `from` to sp+0x40 until `from` reaches `end`
// (0x80 -> 0xB0, 0xC0 -> 0xF0), then the last three words.
inline void copy_record(Runtime &rt, const GuestFrame &frame, u32 from, u32 end) {
    u32 target = 0x40u;
    while (from != end) {
        put(rt, frame.at(target), word(rt, frame.at(from)));
        put(rt, frame.at(target + 4u), word(rt, frame.at(from + 4u)));
        put(rt, frame.at(target + 8u), word(rt, frame.at(from + 8u)));
        put(rt, frame.at(target + 0xCu), word(rt, frame.at(from + 0xCu)));
        from += 0x10u;
        target += 0x10u;
    }
    put(rt, frame.at(target + 8u), word(rt, frame.at(from + 8u)));
    put(rt, frame.at(target), word(rt, frame.at(from)));
    put(rt, frame.at(target + 4u), word(rt, frame.at(from + 4u)));
}

// 0x08AD0C9C (a0 = self, a1 = eight string pointers): asks the virtual slot 0x6C, opens self+4 through 0x08B34888, then
// walks the eight pointers. For the non-empty ones it builds a record through 0x08B0EC60 (sp+0x80) and, when the record's
// first word is zero, records the rounded size at sp+0x20+i and adds size + 0x80 to a total. When the total is non-zero it
// takes 0x08B2E0B0 / 0x08B35418, reserves half the free difference through 0x08B35DA0, converts the recorded sizes with
// 0x08B15490 into sp+0, and releases through 0x08B35254 / 0x08B2E0A4. The same eight pointers are then combined through
// 0x08B10170 into self+0x74 .. +0x90 and each handle is opened with 0x08AF6CE8 (then 0x08AF771C / 0x08AF7B0C). Ends in a
// tail call to 0x08B34B24(self + 4).

// 0x08B3177C (a0 = self): when the word at +0xC is negative, returns it at once. A set byte at +8 returns (cleared)
// through a tail call to 0x08B3027C. Otherwise the object at +0x8FC is opened through 0x08B344E0; when byte +5 is zero
// and +0x12A8 is negative, the 64-bit clock 0x08B2E05C is compared against the pair at +0x12B0 / +0x12B4 and an older
// stamp takes the 0x08B2F70C path. Then 0x08B3457C runs, and a non-zero byte at +0x9648 leads to a second clock read
// whose absolute difference from +0x9640 / +0x9644 is tested against 0x00989680 (and the elder path otherwise). Ends in
// one of the tail calls 0x08B3027C / 0x08B31600.

// 0x089C79F0 (a0 = self, a1 = mode, a2 = node, a3 = a string): builds a text in the string at sp+0xC and emits one group
// per child. 0x08A7BDB0(node) gives an object whose 0x08B35F60 text is appended (0x08BEED1C) after 0x08B88F44 built the
// string from a3. 0x089C72A0(self, string) decides whether the plain separator 0x08CA65F0 is appended or a record is built:
// for (mode & 0x24) == 4 from 0x089BAC2C(sp+0x30, ...) + 0x08B88F44(sp+0x40) + 0x08C011A0(self+0x2C, sp+0x40), else from
// 0x089BAC2C(sp+0x28, ...) + 0x08B88F44(sp+0x34) + 0x08C011A0(self+0x14, sp+0x34); the answers are stored at 0x1C/0x20 or
// 0x14/0x18, the record is released through the 0x08D74FD0 protocol. Then every child of node+0x44 / +0x18 / +0xC is visited
// recursively through 0x089C79F0(self, mode|8, child, sp+0xC) following +0x54, and the string is released the same way.

} // namespace
} // namespace new_batch_main_1002
} // namespace gt::game

#define NEW1002(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1002::name, gt::ReturnKind::kind)
NEW1002(0x088CD2FCu, func_088CD2FC, Word);
NEW1002(0x08AB6BA0u, func_08AB6BA0, Word);
