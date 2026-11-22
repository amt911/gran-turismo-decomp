// New batch main 131: large functions (339-362 instructions) emitted by the recompiler that no src/ file defines or registers.
// Written by hand from `tools.disasm` as structured C; a DRAFT, not yet verified by the differential gate.
// Conventions: a guest call leaves v0 = its result (`call`); the compiler's shared-count release (atomic decrement of the use count
// at +4, `dispose` = vtable +8, atomic decrement of the weak count at +8, `destroy` = vtable +0xC) is `release`, which leaves v0 as
// the original does (1 after an atomic update that did not reach zero, else the last virtual call's result). The callee-saved
// registers are never touched: their values live in C++ locals. Stack temporaries that no callee reads stay in C++ locals; the
// ones whose address is passed to a callee live in the guest frame at the asm's offsets.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_131 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The compiler's virtual call: v0 = the vtable word, callee = vtable[offset].
u32 vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    return call(rt, ctx, function, arguments);
}
// ll/sc loop: increment the word at `address`; v0 = 1 (the sc result), v1 = address.
void atomic_inc(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
}
// ll/sc loop: decrement the word at `address`; returns the old value (a0); v0 = 1, v1 = address.
u32 atomic_dec(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    return old;
}
// Drop one reference of a counted object (null: nothing happens, v0 untouched).
void release(Runtime &rt, AllegrexContext &ctx, u32 counted) {
    if (counted == 0u) return;
    if (atomic_dec(rt, ctx, counted + 4u) != 1u) return;
    vcall(rt, ctx, counted, 8u, {counted});
    if (atomic_dec(rt, ctx, counted + 8u) != 1u) return;
    vcall(rt, ctx, counted, 0xCu, {counted});
}

// 0x08BE27C4 (a0 = object): makes a shared copy of itself at sp+8 ({pointer, count}), calls the virtual at +0xC, then by the state
// word at +4: 0x4A runs the update (lookup 0x089624EC, a 0x18-byte counted block built around the result, swap of the pair at
// +0x64 and state 0x5C), 0x5C calls the function pointer at +0x54, 0 runs the setup; the copy's count is released last.
void func_08BE27C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 pair = frame.at(8u);
    const u32 slot0 = frame.at(0u);
    const u32 slot4 = frame.at(4u);
    call(rt, ctx, 0x08BE13DCu, {pair, self});
    vcall(rt, ctx, self, 0xCu, {self, 0u});
    const u32 state = word(rt, self + 4u);
    ctx.gpr[3] = state;
    if (state == 0x4Au) {
        const u32 owner = word(rt, self + 8u);
        ctx.gpr[2] = owner;
        const u32 flag = word(rt, owner + 0xA8u);
        ctx.gpr[3] = flag;
        if (flag != 0u) {
            ctx.gpr[2] = owner + 0xA8u;
        } else {
            const u32 fresh = word(rt, owner + 0xB8u);
            ctx.gpr[2] = fresh;
            const u32 pair_out = self + 0x64u;
            put(rt, self + 0x60u, fresh);
            const u32 found = call(rt, ctx, 0x089624ECu, {self + 0x40u});
            const u32 object = word(rt, found);
            u32 value = word(rt, self + 0x60u);  // s4
            const u32 extra = word(rt, self + 0x5Cu);
            if (object == 0u) {
                put(rt, slot0, 0u);
                put(rt, slot4, 0u);
            } else {
                const u32 lock = object + 0x6Cu;
                call(rt, ctx, 0x08B34888u, {lock});
                value = call(rt, ctx, 0x08B2CB18u, {object + 8u, value, extra});
                call(rt, ctx, 0x08B34B24u, {lock});
                const u32 counter = word(rt, found + 4u);
                const u32 object_now = word(rt, found);  // s3, reloaded after the calls
                if (counter != 0u) atomic_inc(rt, ctx, counter + 4u);
                put(rt, slot0, value);
                if (counter != 0u) {
                    atomic_inc(rt, ctx, counter + 4u);
                    atomic_inc(rt, ctx, counter + 4u);
                }
                const u32 block = call(rt, ctx, 0x08B62344u, {0x18u});
                put(rt, block + 4u, 1u);
                put(rt, block + 0u, 0x08CC2F90u);
                put(rt, block + 8u, 1u);
                put(rt, block + 0xCu, value);
                put(rt, block + 0x10u, object_now);
                put(rt, block + 0x14u, counter);
                ctx.gpr[3] = 1u;
                ctx.gpr[2] = 0x08CC2F90u;
                if (counter != 0u) atomic_inc(rt, ctx, counter + 4u);
                put(rt, slot4, block);
                release(rt, ctx, counter);
                ctx.gpr[2] = word(rt, slot4);
                if (ctx.gpr[2] == 0u) {
                    const u32 sync = object_now + 0x6Cu;
                    const u32 inner = object_now + 8u;
                    call(rt, ctx, 0x08B34888u, {sync});
                    call(rt, ctx, 0x08B2CE7Cu, {inner, value});
                    const u32 measured = call(rt, ctx, 0x08B2D1A8u, {inner});
                    if (measured == word(rt, object_now + 0x68u)) call(rt, ctx, 0x08B34CA0u, {sync});
                    call(rt, ctx, 0x08B34B24u, {sync});
                }
                release(rt, ctx, counter);
                call(rt, ctx, 0x08B8E380u, {slot4, value, value});
                release(rt, ctx, counter);
            }
            // the pair at +0x64: {value, counted copy of the block}, the old count released
            ctx.gpr[2] = word(rt, slot0);
            const u32 current = word(rt, pair_out + 4u);
            u32 block_now = word(rt, slot4);
            put(rt, pair_out, ctx.gpr[2]);
            if (block_now != current) {
                if (block_now != 0u) atomic_inc(rt, ctx, block_now + 4u);
                const u32 old_count = word(rt, pair_out + 4u);
                release(rt, ctx, old_count);
                put(rt, pair_out + 4u, block_now);
                block_now = word(rt, slot4);
            }
            release(rt, ctx, block_now);
            ctx.gpr[2] = 0x5Cu;
            put(rt, self + 4u, 0x5Cu);
            call(rt, ctx, 0x08AC0448u, {self, word(rt, self + 0x64u), word(rt, self + 0x60u), pair});
        }
    } else if (state == 0x5Cu) {
        ctx.gpr[2] = 0x5Cu;
        const u32 owner = word(rt, self + 8u);
        ctx.gpr[2] = owner;
        const u32 flag = word(rt, owner + 0xA8u);
        ctx.gpr[3] = flag;
        if (flag == 0u) call(rt, ctx, word(rt, self + 0x54u), {self + 0x64u, word(rt, self + 0x58u)});
    } else if (state != 0u) {
        ctx.gpr[2] = 0x5Cu;
    } else {
        ctx.gpr[2] = 0x5Cu;
        call(rt, ctx, 0x08AC04B4u, {self, 0u, word(rt, self + 0x50u), 1u, 0u});
        const u32 table = word(rt, self);
        ctx.gpr[3] = table;
        put(rt, self + 4u, 0x4Au);
        call(rt, ctx, word(rt, table + 0xCu), {self, pair});
        const u32 owner = word(rt, self + 8u);
        vcall(rt, ctx, owner, 8u, {owner});
    }
    release(rt, ctx, word(rt, frame.at(0xCu)));
}

// Copies `count` words guest to guest, one lwc1/swc1 (bit-exact) at a time.
void copy_words(Runtime &rt, u32 destination, u32 source, u32 count) {
    for (u32 i = 0; i < count; ++i) put(rt, destination + 4u * i, word(rt, source + 4u * i));
}

// A 0x4C-byte record (words at +0..+0x38 but +0x10, whose two bytes are separate, then a vector {begin, end, capacity} at +0x3C and a
// word at +0x48) whose vector is a fresh allocation holding a copy of the source's range.
struct Record {
    u32 w[15]{};
    u8 b10{}, b11{};
    u32 begin{}, end{}, capacity{}, tail{};
};
void load_record(Runtime &rt, Record &r, u32 at) {
    for (u32 i = 0; i < 15u; ++i)
        if (i != 4u) r.w[i] = word(rt, at + 4u * i);
    r.b10 = rt.memory().load8(at + 0x10u);
    r.b11 = rt.memory().load8(at + 0x11u);
}
void store_record(Runtime &rt, const Record &r, u32 at) {
    for (u32 i = 0; i < 15u; ++i)
        if (i != 4u) put(rt, at + 4u * i, r.w[i]);
    rt.memory().store8(at + 0x10u, r.b10);
    rt.memory().store8(at + 0x11u, r.b11);
}
// The vector clone: 0x08A5BC20(size, 4) allocates the 16-aligned size of [begin, end), 0x08C15F9C(begin, end, block, tag) copies it.
void clone_vector(Runtime &rt, AllegrexContext &ctx, u32 begin, u32 end, u32 tag, u32 &out_begin, u32 &out_end, u32 &out_capacity) {
    const u32 size = (end - begin) & ~0xFu;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    out_begin = block;
    out_capacity = block + size;
    out_end = block;
    out_end = call(rt, ctx, 0x08C15F9Cu, {begin, end, block, tag});
}
// Frees a vector clone: 0x08BF5B28(begin, end, tag) destroys the elements, 0x08A5BC54(begin, size, 4) frees the block when it exists.
void destroy_vector(Runtime &rt, AllegrexContext &ctx, u32 begin, u32 end, u32 capacity, u32 tag) {
    call(rt, ctx, 0x08BF5B28u, {begin, end, tag});
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, (capacity - begin) & ~0xFu, 4u});
}

// 0x08C178B0 (a0 = first, a1 = last, a2 = key record): while more than one 0x2C-byte element is left, builds three copies of the
// key record (each with its own copy of the vector at +0x3C), moves the last element's pair into the first position's data
// (an element copy: the pair at +0/+4, the 16-byte blocks at +8 and +0x18 when they differ, the word at +0x28) and calls the
// sift 0x08C174D4(first, 0, count, saved last element at sp+8, key copy at sp+0x34), then frees the copies. v0 = 1.
void func_08C178B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x190u);
    const u32 first = ctx.gpr[4];
    u32 last_end = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    u32 span = last_end - first;       // sp+0x124
    u32 span_less_one = span - 0x2Cu;  // sp+0x120
    u32 last_word = last_end - 4u;     // s4: one past the last element's final word
    const u32 saved_b = frame.at(8u);
    const u32 saved_a = frame.at(0x34u);
    // the element count: (span / 4) * 0xBA2E8BA3 is span / 44 for a whole number of elements
    while (static_cast<s32>((static_cast<u32>(static_cast<s32>(span) >> 2)) * 0xBA2E8BA3u) >= 2) {
        Record copy_one;
        load_record(rt, copy_one, key);
        const u32 key_begin = word(rt, key + 0x3Cu);
        const u32 key_end = word(rt, key + 0x40u);
        const u32 alloc_size = (key_end - key_begin) & ~0xFu;
        last_end -= 0x2Cu;
        const u32 element = last_end;
        const u32 block_one = call(rt, ctx, 0x08A5BC20u, {alloc_size, 4u});
        copy_one.begin = block_one;
        copy_one.capacity = block_one + alloc_size;
        copy_one.end = block_one;
        copy_one.end = call(rt, ctx, 0x08C15F9Cu, {word(rt, key + 0x3Cu), word(rt, key + 0x40u), block_one, frame.at(5u)});
        copy_one.tail = word(rt, key + 0x48u);
        // the last element's data, read before it is overwritten
        const u32 last_first = word(rt, last_word - 0x28u);
        const u32 last_second = word(rt, last_word - 0x24u);
        const u32 last_final = word(rt, last_word);
        u32 last_blocks[8];
        for (u32 i = 0; i < 4u; ++i) last_blocks[i] = word(rt, element + 8u + 4u * i);
        for (u32 i = 0; i < 4u; ++i) last_blocks[4u + i] = word(rt, element + 0x18u + 4u * i);
        Record copy_two = copy_one;
        clone_vector(rt, ctx, copy_one.begin, copy_one.end, frame.at(4u), copy_two.begin, copy_two.end, copy_two.capacity);
        copy_two.tail = copy_one.tail;
        // element copy first -> last
        put(rt, last_word - 0x28u, word(rt, first));
        put(rt, last_word - 0x24u, word(rt, first + 4u));
        if (element + 8u != first + 8u) copy_words(rt, element + 8u, first + 8u, 4u);
        if (element + 0x18u != first + 0x18u) copy_words(rt, element + 0x18u, first + 0x18u, 4u);
        put(rt, last_word, word(rt, first + 0x28u));
        // the saved last element at sp+8, the key copy at sp+0x34
        const u32 count = (static_cast<u32>(static_cast<s32>(span_less_one) >> 2)) * 0xBA2E8BA3u;
        put(rt, saved_b + 0u, last_first);
        put(rt, saved_b + 4u, last_second);
        for (u32 i = 0; i < 8u; ++i) put(rt, saved_b + 8u + 4u * i, last_blocks[i]);
        put(rt, saved_b + 0x28u, last_final);
        store_record(rt, copy_two, saved_a);
        Record copy_three = copy_two;
        const u32 size_three = (copy_two.end - copy_two.begin) & ~0xFu;
        const u32 block_three = call(rt, ctx, 0x08A5BC20u, {size_three, 4u});
        copy_three.begin = block_three;
        copy_three.capacity = block_three + size_three;
        copy_three.end = block_three;
        copy_three.end = call(rt, ctx, 0x08C15F9Cu, {copy_two.begin, copy_two.end, block_three, frame.at(3u)});
        put(rt, saved_a + 0x3Cu, copy_three.begin);
        put(rt, saved_a + 0x40u, copy_three.end);
        put(rt, saved_a + 0x44u, copy_three.capacity);
        put(rt, saved_a + 0x48u, copy_two.tail);
        call(rt, ctx, 0x08C174D4u, {first, 0u, count, saved_b, saved_a});
        destroy_vector(rt, ctx, word(rt, saved_a + 0x3Cu), word(rt, saved_a + 0x40u), word(rt, saved_a + 0x44u), frame.at(2u));
        destroy_vector(rt, ctx, copy_two.begin, copy_two.end, copy_two.capacity, frame.at(1u));
        destroy_vector(rt, ctx, copy_one.begin, copy_one.end, copy_one.capacity, frame.at(0u));
        span -= 0x2Cu;
        span_less_one -= 0x2Cu;
        last_word -= 0x2Cu;
    }
    ctx.gpr[2] = 1u;
}

// ---- 0x088C41AC: swept overlap of two oriented boxes, projected on x and z ------------------------------------------------------
// The extents of a box from the 3x3 part of its matrix (rows read at +0xC/+0x14/+0x18/+0x20/+0x24/+0x2C/+0x30/+0x38 of `base`): the
// four-term min/max trees of the asm, each `(a < b) ? ... : ...` as the asm's compare-and-move pair (a NaN takes the else side).
struct BoxExtents {
    float hi_x, lo_x, hi_z, lo_z;
};
float larger(float a, float b) { return a < b ? b : a; }
float smaller(float a, float b) { return a < b ? a : b; }
BoxExtents box_extents(Runtime &rt, u32 base, float origin_x, float origin_z) {
    const float m38 = -float_at(rt, base + 0x38u);
    const float m24 = float_at(rt, base + 0x24u);
    const float m0c = float_at(rt, base + 0x0Cu);
    const float m2c = float_at(rt, base + 0x2Cu);
    const float m18 = float_at(rt, base + 0x18u);
    const float m30 = -float_at(rt, base + 0x30u);
    const float m14 = float_at(rt, base + 0x14u);
    const float m20 = float_at(rt, base + 0x20u);
    const float p24_0c = psp_mul(m24, m0c);
    const float p18_38 = psp_mul(m18, m38);
    const float p2c_18 = psp_mul(m2c, m18);
    const float sum_b = p24_0c + p18_38;
    const float sum_a = p24_0c + p2c_18;
    const float p14_30 = psp_mul(m14, m30);
    const float p0c_30 = psp_mul(m0c, m30);
    const float p2c_20 = psp_mul(m2c, m20);
    const float p24_14 = psp_mul(m24, m14);
    const float p20_38 = psp_mul(m20, m38);
    const float sum_h = p2c_20 + p14_30;
    const float sum_d = p2c_18 + p0c_30;
    const float sum_e = p24_14 + p20_38;
    const float sum_c = p18_38 + p0c_30;
    const float sum_g = p20_38 + p14_30;
    const float sum_f = p24_14 + p2c_20;
    BoxExtents out;
    out.hi_x = larger(larger(sum_a, sum_b), larger(sum_c, sum_d)) + origin_x;
    out.lo_x = smaller(smaller(sum_a, sum_b), smaller(sum_c, sum_d)) + origin_x;
    out.hi_z = larger(larger(sum_f, sum_e), larger(sum_g, sum_h)) + origin_z;
    out.lo_z = smaller(smaller(sum_f, sum_e), smaller(sum_g, sum_h)) + origin_z;
    return out;
}

// 0x088C41AC (a0 = box A, a1 = box B, a2 = an index, a3 = a flag byte, t0 = the result: a float at +0x44 and a byte at +0x51): when
// |B.y - A.y| <= 5 (and, with the flag clear, B lies in front of A along A's direction at +0x40) clips the relative motion
// (B - A at +0x64 / +0x6C) against the two boxes' x and z extents; a hit with t_enter <= t_exit stores a weight 0 / 2 / 1.5 / t_enter
// in the result when it is larger than the one there and the index is not -1. v0: 3 (behind), -1 (past the first checks), else kept.
void func_088C41AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 box_a = ctx.gpr[4];
    const u32 box_b = ctx.gpr[5];
    const u32 index = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    const u32 result = ctx.gpr[8];
    const u32 matrix_a = box_a + 0x28u;
    const u32 matrix_b = box_b + 0x28u;
    const float dy = float_at(rt, matrix_b + 4u) - float_at(rt, matrix_a + 4u);
    if (5.0f < std::fabs(dy)) return;
    const float a_x = float_at(rt, matrix_a);
    const float a_z = float_at(rt, matrix_a + 8u);
    const float b_z = float_at(rt, matrix_b + 8u);
    if (flag == 0u) {
        const float dx = float_at(rt, box_b + 0x28u) - float_at(rt, box_a + 0x28u);
        const float dz = b_z - a_z;
        float dot = psp_mul(dx, float_at(rt, box_a + 0x40u));
        dot = dot + psp_mul(dy, float_at(rt, box_a + 0x44u));
        dot = dot + psp_mul(dz, float_at(rt, box_a + 0x48u));
        if (dot <= 0.0f) {
            ctx.gpr[2] = 3u;
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
    const BoxExtents a = box_extents(rt, matrix_a, a_x, a_z);
    const BoxExtents b = box_extents(rt, matrix_b, float_at(rt, matrix_b), b_z);
    const float dv = float_at(rt, box_b + 0x64u) - float_at(rt, box_a + 0x64u);
    const float dw = float_at(rt, box_b + 0x6Cu) - float_at(rt, box_a + 0x6Cu);
    constexpr float kMax = std::bit_cast<float>(0x7F7FFFFFu);
    float t_enter = 0.0f;
    float t_exit = kMax;
    // x axis
    if (dv < 0.0f) {
        if (b.hi_x < a.lo_x) return;
        if (a.hi_x < b.lo_x) {
            t_enter = (a.hi_x - b.lo_x) / dv;
            if (t_enter < 0.0f) t_enter = 0.0f;
        }
        if (a.lo_x < b.hi_x) {
            t_exit = (a.lo_x - b.hi_x) / dv;
            if (!(t_exit < kMax)) t_exit = kMax;
        }
    }
    if (0.0f < dv) {
        if (a.hi_x < b.lo_x) return;
        if (b.hi_x < a.lo_x) {
            const float q = (a.lo_x - b.hi_x) / dv;
            t_enter = (q < t_enter) ? t_enter : q;
        }
        if (b.lo_x < a.hi_x) {
            const float q = (a.hi_x - b.lo_x) / dv;
            if (q < t_exit) t_exit = q;
        }
    }
    // z axis
    if (dw < 0.0f) {
        if (b.hi_z < a.lo_z) return;
        if (a.hi_z < b.lo_z) {
            const float q = (a.hi_z - b.lo_z) / dw;
            t_enter = (q < t_enter) ? t_enter : q;
        }
        if (a.lo_z < b.hi_z) {
            const float q = (a.lo_z - b.hi_z) / dw;
            if (q < t_exit) t_exit = q;
        }
    }
    if (0.0f < dw) {
        if (a.hi_z < b.lo_z) return;
        if (b.hi_z < a.lo_z) {
            const float q = (a.lo_z - b.hi_z) / dw;
            t_enter = (q < t_enter) ? t_enter : q;
        }
        if (b.lo_z < a.hi_z) {
            const float q = (a.hi_z - b.lo_z) / dw;
            if (q < t_exit) t_exit = q;
        }
    }
    if (t_exit < t_enter) return;
    float weight = 0.0f;
    if (t_enter < 3.0f) {
        weight = 2.0f;
        if (!(t_enter < 1.5f)) weight = 1.5f / t_enter;
    }
    if (float_at(rt, result + 0x44u) < weight) {
        if (index != 0xFFFFFFFFu) rt.memory().store8(result + 0x51u, static_cast<std::uint8_t>(index));
        put_float(rt, result + 0x44u, weight);
    }
}

// ---- 0x0894C460: builds the output record (+0xCC.. timings, +0xC / +0x6C float tables, +0xD0, +0xDC, +0xE0) from the input ---------
float cvt(s32 value) { return static_cast<float>(value); }
float fconst(u32 bits) { return std::bit_cast<float>(bits); }
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// 0x0894C460 (a0 = output record, a1 = input record, a2 = a count, a3 = passed on to 0x0894C294). The input's byte at +0xBF is the
// number of points n. The output gets: the halves at +0xCE / +0xCC (10x and 100x of input bytes/halves), the n floats at +0xC (input
// bytes at +0xC0 times 100 * 0.0262), the n floats at +0x6C (a curve from 0x08960BE8 or a ramp, times the +0xD8 halves and 9.8),
// then 0x08946734 / 0x0894C294 run, the +0x6C floats are scaled by the ratio of 0x0894C240 and 0x0894C178 and the +0xDC / +0xE0 floats
// come from the count. The input's half at +0xD8 + 2(n - 1) is filled from +0xD4 + 2n when it is 0.
void func_0894C460(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 out = ctx.gpr[4];
    const u32 in = ctx.gpr[5];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 saved_a3 = ctx.gpr[7];
    const u32 scratch = frame.at(0u);
    put(rt, frame.at(0x10u), saved_a3);
    s32 wide = signed_half_at(rt, in + 0x120u);
    if (wide == 0) wide = static_cast<s32>(byte_at(rt, in + 0xBCu));
    rt.memory().store16(out + 0xCEu, static_cast<std::uint16_t>(wide * 10));
    u32 tens = byte_at(rt, in + 0xBDu);
    if (tens == 0u) tens = byte_at(rt, in + 0xBFu + byte_at(rt, in + 0xBFu));
    rt.memory().store16(out + 0xCCu, static_cast<std::uint16_t>(tens * 100u));
    {
        const s32 floor_value = static_cast<s32>(byte_at(rt, in + 0xBEu) * 100u + 0xC8u);
        if (static_cast<s32>(half_at(rt, out + 0xCCu)) < floor_value) rt.memory().store16(out + 0xCCu, static_cast<std::uint16_t>(floor_value));
    }
    const s32 points = static_cast<s32>(byte_at(rt, in + 0xBFu));
    const u32 last_half = in + 0xD8u + 2u * static_cast<u32>(points - 1);
    if (signed_half_at(rt, last_half) == 0) rt.memory().store16(last_half, static_cast<std::uint16_t>(half_at(rt, in + 0xD4u + 2u * static_cast<u32>(points))));
    for (s32 i = 0; i < points; ++i)
        put_float(rt, out + 0xCu + 4u * static_cast<u32>(i), psp_mul(cvt(static_cast<s32>(byte_at(rt, in + 0xC0u + static_cast<u32>(i)) * 100u)), fconst(0x3DD67750u)));
    const s32 mode = static_cast<std::int8_t>(byte_at(rt, in + 0xBu));
    float scale_a;  // f21
    float scale_b;  // f22
    if (mode != 0) {
        const float k = cvt(mode) / fconst(0x42C80000u);
        const float first = cvt(static_cast<s32>(byte_at(rt, 0x08CDE096u))) / fconst(0xC2C80000u);
        const float second = cvt(static_cast<s32>(byte_at(rt, 0x08CDE097u))) / fconst(0x42C80000u);
        scale_a = psp_mul(k, first) + fconst(0x3F800000u);
        scale_b = psp_mul(k, second) + fconst(0x3F800000u);
    } else {
        scale_a = fconst(0x3F800000u);
        if (half_at(rt, in + 0x10Cu) != 0u) scale_a = psp_mul(cvt(static_cast<s32>(half_at(rt, in + 0x10Cu))), fconst(0x3C23D70Au));
        scale_b = fconst(0x3F800000u);
        if (half_at(rt, in + 0x10Au) != 0u) scale_b = psp_mul(cvt(static_cast<s32>(half_at(rt, in + 0x10Au))), fconst(0x3A83126Fu));
    }
    float last_point = float_at(rt, out + 8u + 4u * static_cast<u32>(points));  // f23
    float factor = fconst(0x3C23D70Au);                                          // f24
    const float first_point = float_at(rt, out + 0xCu);                          // f20
    if (half_at(rt, in + 0x108u) != 0u) {
        const float scaled = psp_mul(cvt(static_cast<s32>(half_at(rt, in + 0x108u))), fconst(0x3A83126Fu));
        factor = psp_mul(scaled, factor);
    }
    const u32 percent = byte_at(rt, in + 7u);
    if (percent != 0u && percent != 0x64u) factor = psp_mul(factor, cvt(static_cast<s32>(percent)) / fconst(0x42C80000u));
    const u32 kind = byte_at(rt, in + 0xCu);
    const float floor_factor = psp_mul(factor, cvt(static_cast<s32>(byte_at(rt, 0x08CDE095u))) / fconst(0x42C80000u));  // f26
    call(rt, ctx, 0x08960A48u, {scratch, kind < 3u ? kind : 0u, byte_at(rt, 0x08CDE094u)});
    if (points > 0) {
        last_point = last_point - first_point;
        scale_b = scale_b - scale_a;
        for (s32 i = 0; i < points; ++i) {
            float value;
            const float point = float_at(rt, out + 0xCu + 4u * static_cast<u32>(i));
            if (mode != 0) {
                const float curve = call_float(rt, ctx, 0x08960BE8u, {scratch, 0u}, {(point - first_point) / last_point});
                value = psp_mul(factor, scale_a + psp_mul(curve, scale_b));
                if (value < floor_factor) value = floor_factor;
            } else {
                value = psp_mul(factor, scale_a + psp_mul(scale_b, point - first_point) / last_point);
            }
            s32 half = signed_half_at(rt, in + 0xD8u + 2u * static_cast<u32>(i));
            if (half < -1) half += 0x10000;
            put_float(rt, out + 0x6Cu + 4u * static_cast<u32>(i), psp_mul(value, psp_mul(cvt(half), fconst(0x411CCCCDu))));
        }
    }
    call(rt, ctx, 0x08946734u, {out, byte_at(rt, in + 0xBFu), out + 0xCu, out + 0x6Cu, 0u, 0u});
    {
        float rate = psp_mul(cvt(static_cast<s32>(half_at(rt, in + 0xBAu))), fconst(0x3DCCCCCDu));
        rate = psp_mul(rate, fconst(0x411CCCCDu));
        const float divisor = float_at(rt, out + 8u + 4u * byte_at(rt, in + 0xBFu));
        const bool direct = byte_at(rt, 0x08CDE508u + 0x188u) != 0u;
        put_float(rt, out + 0xD0u, direct ? psp_mul(rate, fconst(0x3F666666u)) : rate / divisor);
    }
    call(rt, ctx, 0x0894C294u, {in, out, word(rt, frame.at(0x10u))});
    ctx.gpr[2] = static_cast<u32>(signed_half_at(rt, out));
    if (signed_half_at(rt, out) > 0) {
        s32 done = 0;
        do {
            const s32 entry = static_cast<std::int16_t>(byte_at(rt, in + 0xC0u + static_cast<u32>(done)) * 100u);
            float ratio = call_float(rt, ctx, 0x0894C240u, {out, static_cast<u32>(entry), static_cast<u32>(entry), scratch}, {});
            const float divisor = call_float(rt, ctx, 0x0894C178u, {out, static_cast<u32>(entry), static_cast<u32>(entry), scratch}, {});
            ratio = ratio / divisor;
            ++done;
            ctx.gpr[2] = done < signed_half_at(rt, out) ? 1u : 0u;
            put_float(rt, out + 0x6Cu + 4u * static_cast<u32>(done - 1), psp_mul(ratio, float_at(rt, out + 0x6Cu + 4u * static_cast<u32>(done - 1))));
        } while (ctx.gpr[2] != 0u);
    }
    rt.memory().store8(out + 0xDBu, 0u);
    put(rt, out + 0xDCu, 0u);
    if (count > 0) {
        rt.memory().store8(out + 0xDBu, 1u);
        const u32 level = byte_at(rt, 0x08CDE508u + 8u);
        ctx.gpr[2] = level;
        float swing = psp_mul(cvt(count), fconst(0x411CCCCDu));
        swing = psp_mul(swing, fconst(0x44330CCDu));
        swing = psp_mul(swing, fconst(0x40C90FDBu));
        const float level_scaled = psp_mul(cvt(static_cast<s32>(level)), fconst(0x3C23D70Au));
        swing = swing / fconst(0x42700000u);
        put_float(rt, out + 0xE0u, level_scaled);
        const float denominator = psp_mul(level_scaled, fconst(0xBF000000u)) + fconst(0x3F800000u);
        put_float(rt, out + 0xDCu, swing / denominator);
    }
}

// ---- 0x08BC7948: find-or-create an object in a registry by name, returning a shared pointer pair ---------------------------------
// 0x08BC7948 (a0 = result pair {pointer, count}, a1 = the registry handle {registry, count}, a2 = the key (a pointer to a COW string
// pointer), t0 = an argument for the allocator): looks the key up in the registry's list at +0x98 (nodes: next at +0, value string at
// +8, compared by length at -0xC and by 0x08B36170), and when a node matches allocates a 0x124-byte object (0x08B2CB18 under the
// registry's lock at +0x6C), clears it, wraps it in a 0x18-byte counted block and stores {pointer, block} into the node (+0xC, +0x10)
// and the result. Not found, or no memory: the result pair is {0, 0}. The handle is released by 0x08BBCFA0 on every path but the
// empty handle's. v0 = the result.
void func_08BC7948(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 out = ctx.gpr[4];
    const u32 handle = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    const u32 extra = ctx.gpr[8];
    const u32 pair = frame.at(0xCu);   // {object, counted block}
    const u32 block_slot = frame.at(0x10u);
    const u32 registry = word(rt, handle);
    if (registry == 0u) {
        put(rt, out, 0u);
        put(rt, out + 4u, 0u);
        ctx.gpr[2] = out;
        return;
    }
    call(rt, ctx, 0x08B34888u, {registry + 0x6Cu});
    const u32 owner = word(rt, handle);
    const u32 head = owner + 0x98u;
    u32 node = head;
    const u32 first = word(rt, owner + 0x98u);
    if (first != head) {
        const u32 text = word(rt, key);
        node = first;
        for (;;) {
            const u32 node_text = word(rt, node + 8u);
            const u32 node_length = word(rt, node_text - 0xCu);
            put(rt, frame.at(4u), node_length);
            const u32 key_length = word(rt, text - 0xCu);
            put(rt, frame.at(8u), key_length);
            const u32 shorter = key_length < node_length ? key_length : node_length;
            const u32 order = call(rt, ctx, 0x08B36170u, {word(rt, node + 8u), text, shorter});
            if (order == 0u && node_length == key_length) break;
            node = word(rt, node);
            if (node == head) break;
        }
    }
    if (word(rt, owner + 0x98u) == node) {
        put(rt, out, 0u);
        put(rt, out + 4u, 0u);
        call(rt, ctx, 0x08BBCFA0u, {handle});
        ctx.gpr[2] = out;
        return;
    }
    const u32 lock = owner + 0x6Cu;
    call(rt, ctx, 0x08B34888u, {lock});
    const u32 object = call(rt, ctx, 0x08B2CB18u, {owner + 8u, 0x124u, extra});
    call(rt, ctx, 0x08B34B24u, {lock});
    if (object == 0u) {
        put(rt, out, 0u);
        put(rt, out + 4u, 0u);
        call(rt, ctx, 0x08BBFF7Cu, {owner, node});
        call(rt, ctx, 0x08BBCFA0u, {handle});
        ctx.gpr[2] = out;
        return;
    }
    put(rt, object + 0x48u, 0u);
    put(rt, object + 0x58u, 0u);
    for (u32 offset = 0x68u; offset <= 0xB8u; offset += 0x10u) put(rt, object + offset, 0u);
    for (u32 i = 0; i < 5u; ++i) put(rt, object + 0xC8u + 0x10u * i, 0u);
    const u32 tail = object + 0x118u;
    put(rt, object + 0x110u, 0u);
    put(rt, object + 0x114u, 0u);
    put(rt, object + 0x118u, 0u);
    put(rt, tail + 4u, 0u);
    put(rt, tail + 8u, 0u);
    const u32 counter = word(rt, handle + 4u);
    const u32 owner_now = word(rt, handle);
    if (counter != 0u) atomic_inc(rt, ctx, counter + 4u);
    put(rt, pair, object);
    if (counter != 0u) {
        atomic_inc(rt, ctx, counter + 4u);
        atomic_inc(rt, ctx, counter + 4u);
    }
    const u32 counted = call(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, counted + 4u, 1u);
    put(rt, counted + 0u, 0x08CC5258u);
    put(rt, counted + 8u, 1u);
    put(rt, counted + 0xCu, object);
    put(rt, counted + 0x10u, owner_now);
    put(rt, counted + 0x14u, counter);
    ctx.gpr[3] = 1u;
    if (counter != 0u) atomic_inc(rt, ctx, counter + 4u);
    put(rt, block_slot, counted);
    release(rt, ctx, counter);
    ctx.gpr[2] = word(rt, block_slot);
    if (ctx.gpr[2] == 0u) {
        if (word(rt, object + 0x110u) != 0u) call(rt, ctx, 0x08905A94u, {object, 0u});
        call(rt, ctx, 0x08BC0750u, {word(rt, object + 0x118u), word(rt, tail + 4u), frame.at(0u)});
        const u32 text_block = word(rt, object + 0x118u);
        if (text_block != 0u) call(rt, ctx, 0x08B35254u, {text_block});
        release(rt, ctx, word(rt, object + 0x114u));
        const u32 failure_lock = owner_now + 0x6Cu;
        const u32 inner = owner_now + 8u;
        call(rt, ctx, 0x08B34888u, {failure_lock});
        call(rt, ctx, 0x08B2CE7Cu, {inner, object});
        const u32 measured = call(rt, ctx, 0x08B2D1A8u, {inner});
        if (measured == word(rt, owner_now + 0x68u)) call(rt, ctx, 0x08B34CA0u, {failure_lock});
        call(rt, ctx, 0x08B34B24u, {failure_lock});
        call(rt, ctx, 0x08BBEA18u, {owner_now + 0x98u, inner});
    }
    release(rt, ctx, counter);
    call(rt, ctx, 0x08B8E380u, {block_slot, object, object});
    release(rt, ctx, counter);
    ctx.gpr[2] = word(rt, pair);
    put(rt, node + 0xCu, ctx.gpr[2]);
    const u32 stored = word(rt, block_slot);
    if (stored != 0u) atomic_inc(rt, ctx, stored + 8u);
    const u32 previous = word(rt, node + 0x10u);
    if (previous != 0u) {
        if (atomic_dec(rt, ctx, previous + 8u) == 1u) vcall(rt, ctx, previous, 0xCu, {previous});
    }
    put(rt, node + 0x10u, stored);
    put(rt, out, word(rt, pair));
    const u32 final_block = word(rt, block_slot);
    put(rt, out + 4u, final_block);
    if (final_block != 0u) {
        atomic_inc(rt, ctx, final_block + 4u);
        release(rt, ctx, word(rt, block_slot));
    }
    call(rt, ctx, 0x08BBCFA0u, {handle});
    ctx.gpr[2] = out;
}

// ---- 0x088D9588: wait for a deadline, then reset the object's shared members -------------------------------------------------------
// Points a counted member slot at `count`, taking a reference on the new block and releasing the old one (nothing when they are the same).
void assign_count(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 count) {
    if (count == word(rt, slot)) return;
    if (count != 0u) atomic_inc(rt, ctx, count + 4u);
    release(rt, ctx, word(rt, slot));
    put(rt, slot, count);
}

// 0x088D9588 (a0 = object, a1 = a flag byte): the 64-bit deadline at +0x710/+0x714 is computed by 0x088D9C24 from the clock at
// +0x90/+0x94 when it is not set yet (a clear flag then returns 0). While the clock (+0x94 signed high word, +0x90 low) is before the
// deadline: returns 0, or, from the waiting loop, polls 0x08B1A844 (non-zero ends the wait) and sleeps 0x2710 through 0x08B2E2B8.
// Past the deadline the shared pairs at +0x678/+0x67C, +0x680/+0x684, +0x688/+0x68C, +0x690/+0x694 are reset to {0, 0}
// (releasing the old counted blocks), 0x08B90024/0x08B8FE44/0x08B35254 run on the member at +0x698, the deadline is cleared and v0 = 1.
void func_088D9588(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const u32 deadline_low = word(rt, self + 0x710u);
    const u32 deadline_high = word(rt, self + 0x714u);
    bool wait = false;
    if ((deadline_low | deadline_high) != 0u) {
        const u32 high = word(rt, self + 0x94u);
        if (static_cast<s32>(high) < static_cast<s32>(deadline_high)) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 low = word(rt, self + 0x90u);
        if (high == deadline_high && low < deadline_low) {
            ctx.gpr[2] = 0u;
            return;
        }
    } else {
        const GuestResult target = call_guest(rt, ctx, 0x088D9C24u, {self, ctx.gpr[5], word(rt, self + 0x90u), word(rt, self + 0x94u)}, {});
        ctx.gpr[2] = target.v0;
        ctx.gpr[3] = target.v1;
        put(rt, self + 0x710u, target.v0);
        put(rt, self + 0x714u, target.v1);
        if (flag == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 high = word(rt, self + 0x94u);
        const u32 low = word(rt, self + 0x90u);
        wait = static_cast<s32>(high) < static_cast<s32>(target.v1) || (high == target.v1 && low < target.v0);
    }
    while (wait) {
        if (call(rt, ctx, 0x08B1A844u) != 0u) break;
        call(rt, ctx, 0x08B2E2B8u, {0x2710u});
        const u32 high_target = word(rt, self + 0x714u);
        const u32 high = word(rt, self + 0x94u);
        const u32 low_target = word(rt, self + 0x710u);
        const u32 low = word(rt, self + 0x90u);
        if (static_cast<s32>(high) < static_cast<s32>(high_target)) continue;
        if (high != high_target) break;
        if (low < low_target) continue;
        break;
    }
    // reset the shared members: the temporaries on the frame stay zero, so every assignment stores {0, 0}
    const u32 members = self + 0x678u;
    const u32 old_block = word(rt, self + 0x67Cu);
    for (u32 offset = 4u; offset <= 0x2Cu; offset += 4u) put(rt, frame.at(offset), 0u);
    put(rt, self + 0x678u, 0u);
    release(rt, ctx, old_block);
    put(rt, self + 0x67Cu, 0u);
    put(rt, members + 8u, word(rt, frame.at(0xCu)));
    assign_count(rt, ctx, members + 0xCu, word(rt, frame.at(0x10u)));
    put(rt, members + 0x10u, word(rt, frame.at(0x14u)));
    assign_count(rt, ctx, members + 0x14u, word(rt, frame.at(0x18u)));
    put(rt, members + 0x18u, word(rt, frame.at(0x1Cu)));
    assign_count(rt, ctx, members + 0x1Cu, word(rt, frame.at(0x20u)));
    call(rt, ctx, 0x08B90024u, {members + 0x20u, frame.at(0x24u)});
    call(rt, ctx, 0x08B8FE44u, {word(rt, frame.at(0x24u)), word(rt, frame.at(0x28u)), frame.at(0u)});
    const u32 text = word(rt, frame.at(0x24u));
    if (text != 0u) call(rt, ctx, 0x08B35254u, {text});
    release(rt, ctx, word(rt, frame.at(0x20u)));
    release(rt, ctx, word(rt, frame.at(0x18u)));
    release(rt, ctx, word(rt, frame.at(0x10u)));
    release(rt, ctx, word(rt, frame.at(8u)));
    put(rt, self + 0x710u, 0u);
    put(rt, self + 0x714u, 0u);
    ctx.gpr[2] = 1u;
}

// ---- 0x08B7E8D8: the TZ environment string parser ---------------------------------------------------------------------------------
u32 signed_char_at(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address)))); }

// 0x08B7E8D8 (a0 = the allocator tag for the cached copy): reads the TZ-like string from 0x08B807B8(a0, 0x08CB6008) under the lock
// 0x08B7E8C8/0x08B7E8D0, caches a copy at 0x08D72E2C when it changed, and parses "NAME[+-]hh[:mm[:ss]]NAME2[+-]hh...,rule,rule": the
// names go to 0x08D72E3C / 0x08D72E30 (pointers at 0x08CE1238 / 0x08CE123C), the offsets to 0x08CE11FC + 0x18 / + 0x34 (seconds; the
// second defaults to the first minus 3600), and two rules (Mm.w.d, Jn or n, each with an optional /time, default 02:00:00) to the 0x1C-byte
// records from 0x08CE11FC (kind byte +0, fields +4/+8/+0xC, seconds +0x10). Without a string the names are 0x08CB600C and the cached
// values 0x08D72E24 / 0x08D72E28 are zero. v0 = the unlock's result.

// ---- 0x08AA4C64: path normaliser (drive letter, "." and ".." removal, duplicate slashes) --------------------------------------------
// The asm's `subu; sra 2` over two pointers.
u32 words_between(u32 end, u32 begin) { return static_cast<u32>(static_cast<s32>(end - begin) >> 2); }

// 0x08AA4C64 (a0 = output buffer, a1 = the path): copies an upper-cased "X:" drive prefix to the output, splits the rest on '/' into
// a vector of COW strings (0x08C00AD4 builds one from a range, 0x08B88F44 copy-constructs into the vector's end, 0x08C30AD0 grows it),
// then walks the pieces into a second vector: ".." (0x08CB2DFC) removes the last piece unless that is ".." or there is none (then it is
// kept), "." (0x08CB2D9C) is dropped unless the result is still empty, anything else is appended. The result string is "/" (when the
// path was absolute) + the pieces joined with "/" (0x08BEED1C appends a C string, 0x08B89054 a string) and 0x08B7BB6C copies it to the
// output. The temporaries are released (0x08B60804 decrements a COW reference, 0x08B8542C frees; the empty representation is
// 0x08D74FD0) and both vectors destroyed (0x08C2FBF8) and freed (0x08A5BC54). v0 = the last call's result, or the capacity in words.
void func_08AA4C64(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x80u);
    constexpr u32 kEmptyRep = 0x08D74FD0u;
    constexpr u32 kDotDot = 0x08CB2DFCu;
    constexpr u32 kDot = 0x08CB2D9Cu;
    constexpr u32 kSlash = 0x08CB2D80u;
    const u32 out = ctx.gpr[4];
    const u32 path = ctx.gpr[5];
    const u32 vec_a = frame.at(0x28u);  // pieces: begin, end, capacity
    const u32 vec_b = frame.at(0x34u);  // the normalised pieces
    const u32 piece = frame.at(0x18u);  // a temporary string
    const u32 result = frame.at(0x1Cu);
    // the COW string release: a reference count in the representation that sits 0xC bytes before the data
    auto release_text = [&](u32 data, u32 tag) {
        const u32 rep = data - 0xCu;
        if (rep == kEmptyRep) return;
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
        call(rt, ctx, 0x08B8542Cu, {rep, tag});
    };
    // vector<string>::push_back of the string object at `source`
    auto push_back = [&](u32 vector, u32 source) {
        const u32 end = word(rt, vector + 4u);
        if (end == word(rt, vector + 8u)) {
            call(rt, ctx, 0x08C30AD0u, {vector, end, source});
            return;
        }
        if (end != 0u) {
            call(rt, ctx, 0x08B88F44u, {end, source});
            put(rt, vector + 4u, word(rt, vector + 4u) + 4u);
        } else {
            put(rt, vector + 4u, 4u);
        }
    };
    put(rt, frame.at(0x44u), out);
    put(rt, frame.at(0x40u), path);
    {
        const u32 first = signed_char_at(rt, path);
        const bool upper = (first - 0x41u) < 0x1Au;
        const bool lower = (first - 0x61u) < 0x1Au;
        if ((upper || lower) && signed_char_at(rt, path + 1u) == 0x3Au) {
            rt.memory().store8(out, static_cast<std::uint8_t>(first - (lower ? 0x20u : 0u)));
            put(rt, frame.at(0x44u), out + 2u);
            put(rt, frame.at(0x40u), path + 2u);
            rt.memory().store8(out + 1u, rt.memory().load8(path + 1u));
        }
    }
    u32 cursor = word(rt, frame.at(0x40u));
    for (u32 offset = 0x28u; offset <= 0x3Cu; offset += 4u) put(rt, frame.at(offset), 0u);
    // split on '/'
    while (signed_char_at(rt, cursor) != 0u) {
        if (signed_char_at(rt, cursor) == 0x2Fu) {
            // a run of slashes: step past all of them
            for (;;) {
                const u32 next = cursor + 1u;
                const bool another = signed_char_at(rt, cursor + 1u) == 0x2Fu;
                cursor = next;
                if (!another) break;
            }
            continue;
        }
        // the scan starts one past the first character and stops at '/' or NUL
        u32 end = cursor + 1u;
        while (signed_char_at(rt, end) != 0x2Fu && signed_char_at(rt, end) != 0u) ++end;
        call(rt, ctx, 0x08C00AD4u, {piece, cursor, end - cursor, frame.at(0xBu)});
        push_back(vec_a, piece);
        release_text(word(rt, piece), frame.at(0xAu));
        cursor = end;
    }
    u32 index = 0;
    while (index < words_between(word(rt, vec_a + 4u), word(rt, vec_a))) {
        const u32 element = word(rt, vec_a) + (index << 2);
        const u32 length_slot = frame.at(0x20u);
        const u32 other_slot = frame.at(0x24u);
        // element == ".."?
        call(rt, ctx, 0x08B85EB8u, {frame.at(0x14u), kDotDot, frame.at(9u)});
        u32 literal = word(rt, frame.at(0x14u));
        u32 text_length = word(rt, word(rt, element) - 0xCu);
        put(rt, length_slot, text_length);
        u32 literal_length = word(rt, literal - 0xCu);
        put(rt, other_slot, literal_length);
        u32 difference = text_length - literal_length;
        u32 order = call(rt, ctx, 0x08B36170u, {word(rt, element), literal, literal_length < text_length ? literal_length : text_length});
        if (order != 0u) difference = order;
        release_text(literal, frame.at(8u));
        bool pop_done = false;
        if (difference == 0u) {
            const u32 kept_begin = word(rt, vec_b);
            const u32 kept_end = word(rt, vec_b + 4u);
            if (words_between(kept_end, kept_begin) != 0u) {
                // the last kept piece == ".."?
                call(rt, ctx, 0x08B85EB8u, {frame.at(0x10u), kDotDot, frame.at(7u)});
                literal = word(rt, frame.at(0x10u));
                literal_length = word(rt, literal - 0xCu);
                put(rt, other_slot, literal_length);
                const u32 last = word(rt, kept_end - 4u);
                const u32 last_length = word(rt, last - 0xCu);
                put(rt, length_slot, last_length);
                difference = literal_length - last_length;
                order = call(rt, ctx, 0x08B36170u, {literal, word(rt, kept_end - 4u), last_length < literal_length ? last_length : literal_length});
                if (order != 0u) difference = order;
                release_text(literal, frame.at(6u));
                if (difference != 0u) {
                    // pop the last kept piece
                    put(rt, vec_b + 4u, kept_end - 4u);
                    release_text(word(rt, kept_end - 4u), frame.at(5u));
                    pop_done = true;
                }
            }
        }
        if (pop_done) {
            ++index;
            continue;
        }
        // element == "."?
        call(rt, ctx, 0x08B85EB8u, {frame.at(0xCu), kDot, frame.at(4u)});
        literal = word(rt, frame.at(0xCu));
        text_length = word(rt, word(rt, element) - 0xCu);
        put(rt, length_slot, text_length);
        literal_length = word(rt, literal - 0xCu);
        put(rt, other_slot, literal_length);
        difference = text_length - literal_length;
        order = call(rt, ctx, 0x08B36170u, {word(rt, element), literal, literal_length < text_length ? literal_length : text_length});
        if (order != 0u) difference = order;
        release_text(literal, frame.at(3u));
        if (difference == 0u && words_between(word(rt, vec_b + 4u), word(rt, vec_b)) != 0u) {
            ++index;
            continue;
        }
        push_back(vec_b, word(rt, vec_a) + (index << 2));
        ++index;
    }
    put(rt, result, kEmptyRep + 0xCu);
    const u32 prefix = word(rt, frame.at(0x40u));
    if (prefix != 0u && signed_char_at(rt, prefix) == 0x2Fu) call(rt, ctx, 0x08BEED1Cu, {result, kSlash, 1u});
    const u32 kept = words_between(word(rt, vec_b + 4u), word(rt, vec_b));
    for (u32 i = 0; i < kept; ++i) {
        call(rt, ctx, 0x08B89054u, {result, word(rt, vec_b) + (i << 2)});
        if (i < kept - 1u) call(rt, ctx, 0x08BEED1Cu, {result, kSlash, 1u});
    }
    call(rt, ctx, 0x08B7BB6Cu, {word(rt, frame.at(0x44u)), word(rt, result)});
    release_text(word(rt, result), frame.at(2u));
    call(rt, ctx, 0x08C2FBF8u, {word(rt, vec_b), word(rt, vec_b + 4u), frame.at(1u)});
    {
        const u32 begin = word(rt, vec_b);
        const u32 words = words_between(word(rt, vec_b + 8u), begin);
        if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, words << 2, 4u});
    }
    call(rt, ctx, 0x08C2FBF8u, {word(rt, vec_a), word(rt, vec_a + 4u), frame.at(0u)});
    {
        const u32 begin = word(rt, vec_a);
        const u32 words = words_between(word(rt, vec_a + 8u), begin);
        if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, words << 2, 4u});
        else ctx.gpr[2] = words;
    }
}
} // namespace
} // namespace new_batch_main_131
} // namespace gt::game

#define NEW131(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_131::name, gt::ReturnKind::kind)
NEW131(0x08BE27C4u, func_08BE27C4, Word);
NEW131(0x08C178B0u, func_08C178B0, Word);
NEW131(0x088C41ACu, func_088C41AC, Word);
NEW131(0x0894C460u, func_0894C460, Word);
NEW131(0x08BC7948u, func_08BC7948, Word);
NEW131(0x088D9588u, func_088D9588, Word);
NEW131(0x08AA4C64u, func_08AA4C64, Word);
