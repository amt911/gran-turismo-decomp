// New batch main-123 (main session draft, unverified): functions emitted by the recompiler that no src/ file defines or registers.
// Written by hand from `tools/disasm` as structured C. Draft: the differential gate has not run on any of them yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_123 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments (f12-f15).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: v0 = the vtable (object[0]), the callee = vtable[offset].
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    return call_keep(rt, ctx, function, arguments);
}
u32 load8(Runtime &rt, u32 a) { return rt.memory().load8(a); }
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// 0x088F9658 (a0 = owner, a1 = request, a2 = source): refreshes the byte at state+0x14 through the helper's virtual +8, asks
// `source` (virtual +0x28) for an item, resizes the entry list of the owner's state to the item's size (virtual +8) and fills it from the
// item (virtual +0xC), then 0x08B1C324(list, index). v0 = the last call's result, or the item when the request has no list.
void func_088F9658(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 owner = ctx.gpr[4];
    const u32 request = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 state = word(rt, owner + 8u);
    if (load8(rt, state + 0x14u) == 0u) {
        const u32 helper = word(rt, state + 0x10u);
        if (helper != 0u) {
            const GuestResult flag = call_virtual(rt, ctx, helper, 8u, {helper, state + 4u});
            rt.memory().store8(state + 0x14u, static_cast<std::uint8_t>(flag.v0));
        }
    }
    const u32 object = word(rt, owner + 8u); // s1: loaded in the delay slot of the next virtual call
    const GuestResult made = call_virtual(rt, ctx, source, 0x28u, {source});
    const u32 item = made.v0;
    if (item == 0u) return;
    const u32 list = object + 0x40u;
    const u32 request_list = word(rt, request + 0x10u);
    const u32 old_index = word(rt, list + 4u);
    if (request_list == 0u) return;
    call(rt, ctx, 0x08B1BEECu, {list, request_list});
    const u32 index = word(rt, list + 4u);
    const u32 old_entry = object + old_index * 12u + 0x10u;
    const u32 old_vector = old_entry + 0xCu; // s7
    const u32 old_begin = word(rt, old_vector);
    const u32 old_end = word(rt, old_vector + 4u);
    const s32 old_count = static_cast<s32>(old_end - old_begin) >> 5;
    const u32 vector = object + index * 12u + 0x10u + 0xCu; // s1
    const u32 wanted = call_virtual(rt, ctx, item, 8u, {item, old_begin}).v0;
    for (u32 offset = 0x10u; offset < 0x30u; offset += 4u) put(rt, frame.at(offset), 0u);
    const u32 first = word(rt, vector);
    const u32 last = word(rt, vector + 4u);
    const u32 have = static_cast<u32>(static_cast<s32>(last - first) >> 5);
    if (wanted < have) {
        call(rt, ctx, 0x08BBA6ACu, {vector, first + (wanted << 5), last});
    } else {
        call(rt, ctx, 0x08BBB178u, {vector, last, wanted - have, frame.at(0x10u)});
    }
    u32 cursor = word(rt, vector);
    if (static_cast<s32>(wanted) > 0) {
        u32 i = 0;
        do {
            const GuestResult got = call_virtual(rt, ctx, item, 0xCu, {item, i, frame.at(0u)});
            const bool in_range = static_cast<s32>(i) < old_count;
            if (got.v0 != 0u || !in_range) {
                call(rt, ctx, 0x088FFFF8u, {cursor, frame.at(0u)});
            } else {
                const u32 from = word(rt, old_vector) + (i << 5);
                u32 words[8];
                for (u32 k = 0; k < 8u; ++k) words[k] = word(rt, from + 4u * k);
                for (u32 k = 0; k < 8u; ++k) put(rt, cursor + 4u * k, words[k]);
            }
            ++i;
            cursor += 0x20u;
        } while (i != wanted);
    }
    call(rt, ctx, 0x08B1C324u, {list, index});
}

// 0x088A7CE8 (a0 = object, a1 = entity, a2 = id, a3 = another id, f12/f13/f14 = three floats): pushes a 3-row transform built from the
// entity onto the stack at object+0x104, unless that stack is empty and its first entry's float at +0xC is not below f12.
void func_088A7CE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xF0u);
    const u32 object = ctx.gpr[4];
    const u32 entity = ctx.gpr[5];
    const u32 id = ctx.gpr[6];
    const u32 other = ctx.gpr[7];
    const float value_a = ctx.fpr[12];
    const float value_b = ctx.fpr[13];
    const float value_c = ctx.fpr[14];
    const u32 stack = object + 0x104u;
    if (word(rt, stack + 8u) == word(rt, stack + 4u)) {
        ctx.gpr[2] = word(rt, object + 0x104u);
        ctx.gpr[3] = word(rt, ctx.gpr[2]);
        if (!(float_at(rt, ctx.gpr[3] + 0xCu) < value_a)) return;
    }
    GuestResult r = call_keep(rt, ctx, 0x08BAE264u, {entity + 0x50u, id});
    put(rt, frame.at(0x20u), r.v0);
    put(rt, frame.at(0x24u), r.v1);
    r = call_keep(rt, ctx, 0x08BAE250u, {entity + 0x38u, frame.at(0x20u)});
    put(rt, frame.at(0x28u), r.v0);
    put(rt, frame.at(0x2Cu), r.v1);
    const u32 pair = r.v1;
    const u32 pair_low = word(rt, pair);
    const u32 pair_high = word(rt, pair + 4u);
    call_keep(rt, ctx, 0x08BAE2B0u, {frame.at(0x54u), pair_low, pair_high});
    call_keep(rt, ctx, 0x08BAE2FCu, {frame.at(0x30u), word(rt, frame.at(0x28u)), frame.at(0x54u)});
    const u32 matrix = call_keep(rt, ctx, 0x08BAC230u, {entity}).v0;
    put(rt, frame.at(4u), matrix);
    put(rt, frame.at(0x60u), word(rt, matrix) ^ (1u << 31));
    put(rt, frame.at(0x64u), word(rt, matrix + 4u) ^ (1u << 31));
    put(rt, frame.at(0x68u), word(rt, matrix + 8u) ^ (1u << 31));
    put(rt, frame.at(0x6Cu), word(rt, matrix + 0xCu));
    r = call_keep(rt, ctx, 0x08BAE278u, {frame.at(0x60u), id});
    put(rt, frame.at(0x18u), r.v0);
    put(rt, frame.at(0x1Cu), r.v1);
    call_keep(rt, ctx, 0x08BAC284u, {frame.at(0x3Cu), r.v0, r.v1});
    r = call_keep(rt, ctx, 0x08BAE278u, {frame.at(0x60u), other});
    put(rt, frame.at(0x10u), r.v0);
    put(rt, frame.at(0x14u), r.v1);
    call_keep(rt, ctx, 0x08BAC284u, {frame.at(0x48u), r.v0, r.v1});
    r = call_keep(rt, ctx, 0x08BAE278u, {frame.at(0x60u), frame.at(0x30u)});
    put(rt, frame.at(8u), r.v0);
    put(rt, frame.at(0xCu), r.v1);
    r = call_keep(rt, ctx, 0x08BAE28Cu, {frame.at(8u)});
    put(rt, frame.at(0u), r.v0);
    call_keep(rt, ctx, 0x08BAC284u, {frame.at(0x54u), word(rt, r.v0), word(rt, r.v0 + 4u)});
    call_keep(rt, ctx, 0x08BAE364u, {frame.at(0x30u), frame.at(0x54u)});
    call_keep(rt, ctx, 0x08BAE294u, {frame.at(0x70u), frame.at(0x3Cu)});
    call_keep(rt, ctx, 0x08BAE294u, {frame.at(0x80u), frame.at(0x30u)});
    call_keep(rt, ctx, 0x08BAE294u, {frame.at(0x90u), frame.at(0x48u)});
    put_float(rt, frame.at(0x7Cu), value_a);
    put_float(rt, frame.at(0x8Cu), value_b);
    put_float(rt, frame.at(0x9Cu), value_c);
    call_keep(rt, ctx, 0x0896108Cu, {stack, frame.at(0x70u)});
}

// 0x088C64AC (a0 = path, a1 = target, a2 = out point (2 floats + 1), a3 = out angle, f12 = t): interpolates along a straight segment
// (byte at +0 clear) or an arc (the else branch: radius +0x18, angle +0x24) towards the target; writes the point to a2 and the heading to a3.
void func_088C64AC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 path = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 out_point = ctx.gpr[6];
    const u32 out_angle = ctx.gpr[7];
    const float t = ctx.fpr[12];
    float x0 = float_at(rt, path + 4u);
    float y0 = float_at(rt, path + 8u);
    float x1 = float_at(rt, target + 4u);
    float y1 = float_at(rt, target + 8u);
    ctx.gpr[2] = load8(rt, path);
    if (ctx.gpr[2] == 0u) {
        if (out_point != 0u) {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            put_float(rt, out_point, x0 + psp_mul(t, dx));
            put_float(rt, out_point + 4u, y0 + psp_mul(t, dy));
        }
        if (out_angle != 0u) {
            const GuestResult r = call_keepf(rt, ctx, 0x08B4A430u, {}, {-(x1 - x0), y1 - y0});
            put_float(rt, out_angle, r.f0);
        }
    } else {
        const float start_angle = float_at(rt, path + 0x24u); // f28
        call_keepf(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)}, {start_angle});
        const float sine = float_at(rt, frame.at(0u));
        const float radius = float_at(rt, path + 0x18u); // f24
        const float cosine = float_at(rt, frame.at(4u));
        const float negated = -sine;
        put_float(rt, frame.at(0u), negated);
        const float center_x = x0 - psp_mul(radius, cosine); // f27
        const float center_y = y0 + psp_mul(negated, radius); // f26
        float from_angle = call_keepf(rt, ctx, 0x08B4A430u, {}, {center_x - x0, y0 - center_y}).f0; // f21
        float to_angle = call_keepf(rt, ctx, 0x08B4A430u, {}, {center_x - x1, y1 - center_y}).f0; // f1
        // The first result is moved into f21 in the delay slot of the second call; f1 takes the second.
        if (0.0f < radius) {
            if (to_angle < from_angle) to_angle = to_angle + std::bit_cast<float>(0x40C90FDBu);
        } else {
            if (from_angle < to_angle) from_angle = from_angle + std::bit_cast<float>(0x40C90FDBu);
        }
        to_angle = to_angle - from_angle;
        if (out_point != 0u) {
            float sweep = radius;
            const float heading = from_angle + psp_mul(t, to_angle);
            if (radius < 0.0f) sweep = -radius;
            call_keepf(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)}, {heading});
            const float sin2 = float_at(rt, frame.at(8u));
            const float cos2 = float_at(rt, frame.at(0xCu));
            put_float(rt, out_point, center_x - psp_mul(sweep, sin2));
            put_float(rt, out_point + 4u, center_y + psp_mul(sweep, cos2));
        }
        if (out_angle != 0u) {
            put_float(rt, out_angle, start_angle);
            if (!(radius == 0.0f)) {
                float turn = psp_mul(t, float_at(rt, path + 0x2Cu));
                turn = turn / radius;
                put_float(rt, out_angle, start_angle + turn);
            }
        }
    }
    if (out_point != 0u) {
        const float z0 = float_at(rt, path + 0xCu);
        const float z1 = float_at(rt, target + 0xCu);
        put_float(rt, out_point + 8u, z0 + psp_mul(z1 - z0, t));
    }
}

// 0x088800F4 (a0 = object, a1, a2 = a name id): two guard calls (non-zero ends with that v0), then by the four-character code of a2 pulls
// a value from the stream built at sp+0x14 into the object's table at +0x74 (+4).

// 0x08860890 (a0 = object): for each of the four slots of the table at +0x4120 (0x50 bytes each), reconciles the slot's selection with the
// entity list at +0x3F50; v0 = 4 after the loop, or the word +0x3F50 (0) when there is no list.

// 0x08B37E30 (a0 = vector of 16-byte records {data, length, flag byte, byte, word}, a1 = key {data, length}): the index of the first record
// whose bytes equal the key; when none does, appends a copy of the key (flag = the bit at bit-index a0+0xC of the key's data) and returns the new last index.
void func_08B37E30(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 vec = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 header = vec + 0x14u; // s1: {begin, end, capacity end}
    const u32 key_length = word(rt, key + 4u);
    const u32 begin = word(rt, vec + 0x14u);
    u32 end = word(rt, header + 4u); // s0
    const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 4);
    for (u32 i = 0; i < count; ++i) {
        const u32 record = (i << 4) + word(rt, header);
        const u32 length = word(rt, record + 4u);
        if (length != key_length) continue;
        if (static_cast<s32>(length) <= 0) {
            ctx.gpr[2] = i;
            return;
        }
        const u32 data = word(rt, record);
        const u32 key_data = word(rt, key);
        if (load8(rt, data) != load8(rt, key_data)) continue;
        u32 matched = 1u;
        bool equal = true;
        while (length != matched) {
            const bool same = load8(rt, key_data + matched) == load8(rt, data + matched);
            ++matched;
            if (!same) {
                equal = false;
                break;
            }
        }
        if (equal) {
            ctx.gpr[2] = i;
            return;
        }
    }
    const s32 bit_index = static_cast<s32>(word(rt, vec + 0xCu));
    const s32 byte_offset = (bit_index < 0 ? bit_index + 7 : bit_index) >> 3;
    const u32 sign = static_cast<u32>(bit_index >> 31) >> 29;
    const s32 bit = static_cast<s32>(((static_cast<u32>(bit_index) + sign) & 7u) - sign);
    const u32 bit_byte = load8(rt, word(rt, key) + static_cast<u32>(byte_offset));
    const u32 flag = (static_cast<u32>(static_cast<s32>(bit_byte) >> (bit & 31))) & 1u;
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), key_length);
    if (static_cast<s32>(key_length) > 0) {
        const u32 copy = call(rt, ctx, 0x08B6461Cu, {key_length});
        put(rt, frame.at(0u), copy);
        if (static_cast<s32>(word(rt, frame.at(4u))) > 0) {
            u32 k = 0;
            do {
                const u32 byte = load8(rt, word(rt, key) + k);
                rt.memory().store8(word(rt, frame.at(0u)) + k, static_cast<std::uint8_t>(byte));
                ++k;
            } while (static_cast<s32>(k) < static_cast<s32>(word(rt, frame.at(4u))));
        }
        end = word(rt, header + 4u);
    }
    rt.memory().store8(frame.at(8u), static_cast<std::uint8_t>(flag));
    rt.memory().store8(frame.at(9u), 0u);
    put(rt, frame.at(0xCu), 0u);
    if (end == word(rt, header + 8u)) {
        call(rt, ctx, 0x08C5086Cu, {header, end, frame.at(0u)});
    } else {
        if (end != 0u) {
            put(rt, end + 4u, 0u);
            const u32 length = word(rt, frame.at(4u));
            put(rt, end, 0u);
            put(rt, end + 4u, length);
            if (static_cast<s32>(length) > 0) {
                const u32 block = call(rt, ctx, 0x08B6461Cu, {length});
                put(rt, end, block);
                if (static_cast<s32>(word(rt, end + 4u)) > 0) {
                    u32 k = 0;
                    do {
                        const u32 byte = load8(rt, word(rt, frame.at(0u)) + k);
                        rt.memory().store8(word(rt, end) + k, static_cast<std::uint8_t>(byte));
                        ++k;
                    } while (static_cast<s32>(k) < static_cast<s32>(word(rt, end + 4u)));
                }
            } else {
                // the length store at s0+4 (delay slot of the blez) already happened above
            }
            const u32 flag_byte = load8(rt, frame.at(8u));
            const u32 spare = word(rt, frame.at(0xCu));
            rt.memory().store8(end + 8u, static_cast<std::uint8_t>(flag_byte));
            put(rt, end + 0xCu, spare);
            rt.memory().store8(end + 9u, static_cast<std::uint8_t>(load8(rt, frame.at(9u))));
            end = word(rt, header + 4u);
        }
        end += 16u;
        put(rt, header + 4u, end);
    }
    const u32 temp = word(rt, frame.at(0u));
    if (temp != 0u) call(rt, ctx, 0x08B61FC0u, {temp});
    const u32 new_begin = word(rt, header);
    const u32 new_end = word(rt, header + 4u);
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(new_end - new_begin) >> 4) - 1u;
}

// 0x08BA5470 (a0 = vector {begin, end, capacity end} of 4-byte items, a1 = position, a2 = count, a3 = pointer to the value): the
// compiler's insert(position, count, value). With room it moves the tail and fills the gap; otherwise it allocates (max(size, count) more),
// copies the parts around the gap and swaps the buffer in. v0 = the last call's result (unchanged when count is 0).
void func_08BA5470(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 vec = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    put(rt, frame.at(0x10u), count);
    if (count == 0u) return;
    const u32 capacity_end = word(rt, vec + 8u);
    u32 end = word(rt, vec + 4u);
    if (static_cast<u32>(static_cast<s32>(capacity_end - end) >> 2) >= count) {
        const u32 value_word = word(rt, value);
        const u32 tail = static_cast<u32>(static_cast<s32>(end - position) >> 2);
        put(rt, frame.at(8u), value_word);
        if (count < tail) {
            call(rt, ctx, 0x08BA5264u, {end - (count << 2), end, end, frame.at(6u)});
            const u32 bytes = word(rt, frame.at(0x10u)) << 2;
            const u32 old_end = word(rt, vec + 4u);
            u32 move = end - bytes - position;
            move &= ~3u;
            put(rt, vec + 4u, old_end + bytes);
            call(rt, ctx, 0x08B79BACu, {end - move, position, move});
            call(rt, ctx, 0x08BA5240u, {position, position + (word(rt, frame.at(0x10u)) << 2), frame.at(8u)});
        } else {
            call(rt, ctx, 0x08BA52E8u, {end, count - tail, frame.at(8u), frame.at(5u)});
            const u32 extra = (word(rt, frame.at(0x10u)) - tail) << 2;
            const u32 new_end = word(rt, vec + 4u) + extra;
            put(rt, vec + 4u, new_end);
            call(rt, ctx, 0x08BA52B8u, {position, end, new_end, frame.at(4u)});
            put(rt, vec + 4u, word(rt, vec + 4u) + (tail << 2));
            call(rt, ctx, 0x08BA5240u, {position, end, frame.at(8u)});
        }
        return;
    }
    u32 begin = word(rt, vec);
    const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    put(rt, frame.at(8u), size);
    if (0x3FFFFFFFu - size < count) call(rt, ctx, 0x08B5ADDCu, {0x08CA0570u});
    const u32 grow = size < count ? count : size; // std::max through the two stack slots
    const u32 new_capacity = size + grow;
    const bool overflow = new_capacity < size;
    const u32 new_bytes = overflow ? 0xFFFFFFFCu : (new_capacity << 2);
    u32 buffer = 0u;
    if (overflow || new_capacity != 0u) {
        buffer = call(rt, ctx, 0x08B35DA0u, {new_bytes});
        begin = word(rt, vec);
    }
    const u32 copied = call(rt, ctx, 0x08B9A594u, {begin, position, buffer, frame.at(3u)});
    const u32 s0_first = copied;
    call(rt, ctx, 0x08BA5290u, {copied, word(rt, frame.at(0x10u)), value, frame.at(2u)});
    const u32 second = call(rt, ctx, 0x08B9A594u,
                            {position, word(rt, vec + 4u), (word(rt, frame.at(0x10u)) << 2) + s0_first, frame.at(1u)});
    const u32 new_end = second;
    call(rt, ctx, 0x08B9A574u, {word(rt, vec), word(rt, vec + 4u), frame.at(0u)});
    if (word(rt, vec) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, vec)});
    ctx.gpr[2] = buffer + new_bytes;
    put(rt, vec + 8u, ctx.gpr[2]);
    put(rt, vec + 4u, new_end);
    put(rt, vec, buffer);
}

// 0x08936A00 (a0 = object, a1 = text): finds the text among the nine strings of 0x08CDDE0C (index 0 when none), and when that differs
// from the word at +8 stores it and rebuilds the 0x2C slot descriptions: for every slot the checker 0x08936498 accepts, composes a
// message from the object's name tables and hands it to the slot's handler. v0 = the last handler's result (or the index when unchanged).

// 0x08861CB0 (a0 = holder of the object pointer, a1 = rect, a2/a3/t0/t1 = four flags (low byte)): takes the try-lock at object+0x3FD4
// (already held: v0 = 1) and rebuilds the view state: stores the flags at +0x3FD8..+0x3FDA, redraws through the helpers, then releases.
// v0 = 1 unless the 0x088D78EC step answers otherwise.
void func_08861CB0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 rect = ctx.gpr[5];
    const u32 flag_a = ctx.gpr[6] & 0xFFu;  // s5
    const u32 flag_b = ctx.gpr[7] & 0xFFu;  // s6
    u32 flag_c = ctx.gpr[8] & 0xFFu;        // s4
    u32 flag_d = ctx.gpr[9] & 0xFFu;        // s1
    u32 result = 1u;                        // fp
    {
        const u32 lock = word(rt, self) + 0x3FD4u;
        const u32 held = word(rt, lock);
        if (held == 0u) put(rt, lock, 1u);
        else {
            ctx.gpr[2] = result;
            return;
        }
    }
    const u32 view = word(rt, word(rt, self) + 0x5ACu); // s3
    call(rt, ctx, 0x088D65DCu, {frame.at(0u)});
    u32 region;                                           // s7
    if (flag_a != 0u) {
        const u32 first = call(rt, ctx, 0x0885EE70u, {self});
        const u32 second = call(rt, ctx, 0x088915C8u, {first});
        call(rt, ctx, 0x08890478u, {second, frame.at(0u)});
        region = frame.at(0u);
    } else {
        region = rect != 0u ? rect : view + 0xCu;
    }
    if (flag_d == 0u) {
        call(rt, ctx, 0x088D776Cu, {view, 1u});
        call(rt, ctx, 0x08B1C3E4u, {0xFFFFFFFFu, 0xFFFFFFFFu});
    }
    call(rt, ctx, 0x0889365Cu, {word(rt, self) + 0x600u});
    rt.memory().store8(word(rt, self) + 0x3FD8u, static_cast<std::uint8_t>(flag_a));
    rt.memory().store8(word(rt, self) + 0x3FD9u, static_cast<std::uint8_t>(flag_b));
    rt.memory().store8(word(rt, self) + 0x3FDAu, static_cast<std::uint8_t>(flag_c));
    u32 object;
    if (flag_c != 0u) {
        call(rt, ctx, 0x08918298u, {word(rt, self) + 0x5CCu, 0u, 1u, 1u});
        call(rt, ctx, 0x08917AB0u, {word(rt, self) + 0x5CCu});
        object = word(rt, self);
    } else {
        object = word(rt, self);
        if (load8(rt, object + 0x5A9u) != 0u || load8(rt, object + 0x5AAu) == 0u) {
            call(rt, ctx, 0x08918298u, {object + 0x5CCu, flag_a, flag_b, 0u});
            object = word(rt, self);
        }
    }
    const u32 width = call(rt, ctx, 0x088D6358u, {region});
    const u32 panel = object + 0x5CCu;
    const u32 height = call(rt, ctx, 0x088D4C9Cu, {region});
    call(rt, ctx, 0x08918170u, {panel, width, height});
    if (call(rt, ctx, 0x088D4C9Cu, {region}) != 0u) {
        put(rt, word(rt, word(rt, self) + 0x5B8u) + 0x1Cu, 0u);
    } else {
        const u32 owner = word(rt, self);
        const u32 table = word(rt, owner + 0x5B8u);
        const u32 value = flag_a == 0u ? (flag_c == 0u ? 1u : 0u) : 2u;
        put(rt, table + 0x1Cu, value);
    }
    const u32 shown = call(rt, ctx, 0x088D4C9Cu, {region});
    rt.memory().store8(view + 0x624u, static_cast<std::uint8_t>(shown));
    const u32 mode = (flag_c != 0u || flag_a != 0u || flag_b != 0u) ? 1u : 0u; // a2
    const u32 owner = word(rt, self);
    flag_c = 1u;
    result = 1u;
    if (load8(rt, owner + 0x5A9u) != 0u || load8(rt, owner + 0x5AAu) == 0u) {
        const u32 answer = call(rt, ctx, 0x088D78ECu, {view, region, mode});
        result = answer;
        flag_c = answer;
    }
    call(rt, ctx, 0x088D776Cu, {view, 0u});
    call(rt, ctx, 0x088D7744u, {view, 0u});
    call(rt, ctx, 0x088D6910u, {view, 0u, 0u});
    const u32 hook = word(rt, word(rt, self) + 0x3F9C);
    if (hook != 0u) call(rt, ctx, 0x0890F878u, {hook});
    const u32 result_cbfd4 = call(rt, ctx, 0x088CBFD4u);
    // The moves sit in the delay slots of the next call: each saved value is the result of the call before.
    const u32 result_edbc = call(rt, ctx, 0x0885EDBCu, {self});
    const u32 result_edd4 = call(rt, ctx, 0x0885EDD4u, {self});
    call(rt, ctx, 0x088CB958u, {result_cbfd4, result_edbc, result_edd4});
    const u32 result_ca548 = call(rt, ctx, 0x088CA548u);
    const u32 result_da4b0 = call(rt, ctx, 0x088DA4B0u, {view});
    const u32 result_edbc_2 = call(rt, ctx, 0x0885EDBCu, {self});
    call(rt, ctx, 0x088CA2B0u, {result_ca548, result_da4b0, result_edbc_2});
    const u32 camera = call(rt, ctx, 0x08916474u);
    call(rt, ctx, 0x08916188u, {camera, word(rt, view + 0x61Cu)});
    const u32 sky = call(rt, ctx, 0x088C7F64u);
    call(rt, ctx, 0x088C95C4u, {sky, word(rt, view + 0x61Cu), region});
    if (flag_c != 0u) rt.memory().store8(word(rt, self) + 0x5A9u, 0u);
    call(rt, ctx, 0x088D5CF0u, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// Copies the 0xED bytes of the 13-byte-tailed block the compiler moves with a mix of word and halfword loops.
void copy_block(Runtime &rt, u32 to, u32 from) {
    for (u32 i = 0; i < 0xEDu; ++i) rt.memory().store8(to + i, static_cast<std::uint8_t>(rt.memory().load8(from + i)));
}

// 0x08B23454 (a0 = destination block of 0xED bytes, a1 = source {+4 = provider, +8 = flag}): asks the provider (virtual +8) for a block
// (a null provider: the empty block 0x08B215B8), passes it through 0x08B22E94 when the flag is set, and copies the result to a0.
// v0 = a0.
void func_08B23454(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x4C0u);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 provider = word(rt, source + 4u);
    const u32 shaped = frame.at(0xEEu); // s1
    if (provider != 0u) {
        call_virtual(rt, ctx, provider, 8u, {frame.at(0x2D0u), provider});
        copy_block(rt, frame.at(0x1DCu), frame.at(0x2D0u));
        if (word(rt, source + 8u) == 0u) {
            copy_block(rt, destination, frame.at(0x1DCu));
            ctx.gpr[2] = destination;
            return;
        }
        call(rt, ctx, 0x08B22E94u, {frame.at(0x3C0u), frame.at(0x1DCu)});
        copy_block(rt, frame.at(0u), frame.at(0x3C0u));
        copy_block(rt, shaped, frame.at(0u));
    } else {
        call(rt, ctx, 0x08B215B8u, {shaped, 0u, 0u, 0u, 0u});
    }
    copy_block(rt, destination, shaped);
    ctx.gpr[2] = destination;
}

// One shared-pointer release (single-threaded ll/sc): the strong count at +4 drops; when it was 1 the object's virtual +8 disposes it and
// the weak count at +8 drops, when that was 1 too the virtual +0xC destroys it. v0 follows the asm: 1 after each successful sc, then the virtual's result.
void release_member(Runtime &rt, AllegrexContext &ctx, u32 member) {
    if (member == 0u) return;
    u32 old = word(rt, member + 4u);
    put(rt, member + 4u, old - 1u);
    ctx.gpr[2] = 1u;
    if (old != 1u) return;
    call_virtual(rt, ctx, member, 8u, {member});
    old = word(rt, member + 8u);
    put(rt, member + 8u, old - 1u);
    ctx.gpr[2] = 1u;
    if (old != 1u) return;
    call_virtual(rt, ctx, member, 0xCu, {member});
}

// 0x08BCFAC0 (a1 = object): releases the seven shared members at +0x40, +0x38, +0x2C, +0x24, +0x1C, +0x14, +0xC (in that order), then a tail
// call of 0x08B64654(a1). A null a1 returns at once.
void func_08BCFAC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[5];
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        static constexpr u32 offsets[] = {0x40u, 0x38u, 0x2Cu, 0x24u, 0x1Cu, 0x10u + 4u, 0xCu};
        for (const u32 offset : offsets) release_member(rt, ctx, word(rt, object + offset));
    }
    tail_call(ctx, 0x08B64654u, {object});
}

// 0x08B1554C (a0 = handle, a1 = destination, a2 = length): reads `length` bytes for the handle through 0x08B0C650 into the destination:
// a first chunk up to the next 16-byte boundary, then blocks of up to 0x8000 bytes through the staging buffer 0x08D47880 (copied out in
// 128- and 16-byte pieces), then the sub-16-byte tail. v0 = the length, or the first negative answer. The lock 0x08D47840 brackets it.
void func_08B1554C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 handle = ctx.gpr[4];
    u32 destination = ctx.gpr[5];
    const u32 length = ctx.gpr[6];
    put(rt, frame.at(0x58u), handle);
    put(rt, frame.at(0x5Cu), length);
    call(rt, ctx, 0x08B344E0u, {0x08D47840u});
    u32 result = 0u; // s2
    u32 pending = length;
    put(rt, frame.at(0x20u), pending);
    [&]() {
        const u32 misalignment = destination & 0xFu;
        if (misalignment != 0u) {
            u32 chunk = 0x10u - misalignment;
            if (length < chunk) chunk = length;
            put(rt, frame.at(0x10u), destination);
            put(rt, frame.at(0x14u), chunk);
            pending = length - chunk;
            put(rt, frame.at(0x20u), pending);
            destination += chunk;
            result = call(rt, ctx, 0x08B0C650u, {handle, frame.at(0x10u)});
            if (static_cast<s32>(result) < 0) return;
        }
        auto tail = [&]() {
            put(rt, frame.at(0u), destination);
            put(rt, frame.at(4u), pending);
            result = call(rt, ctx, 0x08B0C650u, {handle, frame.at(0u)});
            if (static_cast<s32>(result) < 0) return;
            result = length;
        };
        if (pending < 0x10u) {
            if (pending == 0u) {
                result = length;
                return;
            }
            tail();
            return;
        }
        for (;;) {
            u32 bulk = pending & ~0xFu;
            if (bulk == 0u) {
                tail();
                return;
            }
            if (bulk > 0x7FFFu) bulk = 0x8000u;
            pending -= bulk;
            put(rt, frame.at(0x20u), pending);
            put(rt, frame.at(8u), 0x08D47880u);
            put(rt, frame.at(0xCu), bulk);
            result = call(rt, ctx, 0x08B0C650u, {handle, frame.at(8u)});
            if (static_cast<s32>(result) < 0) return;
            u32 source = 0x08D47880u;
            const u32 blocks = bulk >> 7;
            const u32 units = (bulk >> 4) & 7u;
            for (u32 b = 0; b < blocks; ++b) {
                u32 words[32];
                for (u32 k = 0; k < 32u; ++k) words[k] = word(rt, source + 4u * k);
                for (u32 k = 0; k < 32u; ++k) put(rt, destination + 4u * k, words[k]);
                source += 0x80u;
                destination += 0x80u;
            }
            for (u32 u = 0; u < units; ++u) {
                u32 words[4];
                for (u32 k = 0; k < 4u; ++k) words[k] = word(rt, source + 4u * k);
                for (u32 k = 0; k < 4u; ++k) put(rt, destination + 4u * k, words[k]);
                source += 0x10u;
                destination += 0x10u;
            }
            if (pending >= 0x10u) continue;
            if (pending == 0u) {
                result = length;
                return;
            }
            tail();
            return;
        }
    }();
    call(rt, ctx, 0x08B3457Cu, {0x08D47840u});
    ctx.gpr[2] = result;
}

// 0x08AE8A1C (a0 = object, a1 = parameters): scales the object's offsets by the parameters' 12-bit factors (flag bits of the halfword at
// a1+2), optionally squares them (mode byte at +0x50 == 1), and for each of the +0x36 entries with a non-negative id at +0x38.. calls
// 0x08AEF874(id, 4, record, 0) with the record on the stack. v0 = 0 after the loop, or the (non-positive) entry count.
s32 mul12(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)) >> 12; }
s32 mul7(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)) >> 7; }
s32 signed_square12(s32 value) {
    const s32 square = mul12(value, value);
    return value < 0 ? -square : square;
}
void func_08AE8A1C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 object = ctx.gpr[4];
    const u32 params = ctx.gpr[5];
    s32 t2 = 0;
    const s32 flags = signed_half(rt, params + 2u);
    const u32 table = word(rt, object + 0x10u); // t0
    s32 scale = signed_half(rt, object + 0x40u); // a3
    const u32 extra = word(rt, table + 0x10u);   // t1
    if ((flags & 0x10) != 0) scale = mul12(scale, signed_half(rt, params + 8u));
    if (word(rt, table + 0x14u) == 1u) {
        t2 = signed_byte(rt, object + 0xBu);
        const u32 row = table + static_cast<u32>(t2) * 6u;
        scale = mul7(scale, static_cast<s32>(load8(rt, row + 0x3Au)));
        scale = mul7(scale, static_cast<s32>(load8(rt, row + 0x3Bu)));
        scale = mul7(scale, static_cast<s32>(load8(rt, extra + static_cast<u32>(t2) + 0xD4u)));
    }
    s32 x = scale;
    if ((flags & 4) != 0) x = mul12(scale, signed_half(rt, params + 4u));
    s32 y = scale;
    if ((flags & 8) != 0) y = mul12(scale, signed_half(rt, params + 6u));
    const s32 half_x = signed_half(rt, object + 0x48u);
    const s32 half_y = signed_half(rt, object + 0x4Au);
    const s32 mode = signed_byte(rt, object + 0x50u);
    x = mul12(x, half_x);
    y = mul12(y, half_y);
    s32 first = x;
    if (mode == 1) first = signed_square12(x);
    put(rt, frame.at(4u), static_cast<u32>(first));
    s32 second = y;
    if (mode == 1) second = signed_square12(y);
    put(rt, frame.at(8u), static_cast<u32>(second));
    s32 position = signed_half(rt, object + 0x42u); // s0
    if ((flags & 0x20) != 0) position += signed_half(rt, params + 0xAu);
    if (word(rt, table + 0x14u) == 1u) {
        const u32 row = table + static_cast<u32>(t2) * 6u;
        position += (static_cast<s32>(load8(rt, row + 0x3Cu)) - 0x40) << 4;
    }
    const u32 byte_flags = load8(rt, extra + 0x11u); // s4 / s1
    if ((byte_flags & 0xCu) != 0u) {
        const s32 base = static_cast<s32>(call(rt, ctx, 0x08AE69A0u, {extra + 0x70u, 5u}));
        position -= base;
        if ((byte_flags & 4u) != 0u) position = 0x2000 - position;
        if ((byte_flags & 8u) != 0u) position = 0;
        position += base;
    }
    put(rt, frame.at(0u), byte_flags);
    put(rt, frame.at(0xCu), static_cast<u32>(position));
    s32 limit = 0x1000;
    if ((flags & 0x1000) != 0) limit = signed_half(rt, params + 0x18u);
    const u32 count_byte = load8(rt, object + 0x36u); // t0 / a3
    put(rt, frame.at(0x10u), static_cast<u32>(limit));
    put(rt, frame.at(0x24u), static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(count_byte))));
    put(rt, frame.at(0x20u), 0u);
    s32 a0 = signed_half(rt, object + 0x44u);
    s32 a1 = signed_half(rt, object + 0x46u);
    if ((flags & 0x40) != 0) a0 = mul12(a0, signed_half(rt, params + 0xCu));
    if ((flags & 0x80) != 0) a1 = mul12(a1, signed_half(rt, params + 0xEu));
    s32 a2 = a0;
    if (mode == 1) a2 = signed_square12(a0);
    put(rt, frame.at(0x14u), static_cast<u32>(a2));
    a2 = a1;
    if (mode == 1) a2 = signed_square12(a1);
    put(rt, frame.at(0x18u), static_cast<u32>(a2));
    s32 count = static_cast<std::int8_t>(count_byte);
    ctx.gpr[2] = static_cast<u32>(count);
    if (count <= 0) return;
    u32 cell = object + 0x30u; // s0
    for (s32 i = 0; i < count; ) {
        const s32 id = signed_half(rt, cell + 8u);
        cell += 2u;
        if (id >= 0) {
            put(rt, frame.at(0x1Cu), static_cast<u32>(i));
            call(rt, ctx, 0x08AEF874u, {static_cast<u32>(id), 4u, frame.at(0u), 0u});
            count = static_cast<std::int8_t>(load8(rt, object + 0x36u));
        }
        ++i;
        ctx.gpr[2] = i < count ? 1u : 0u;
        if (!(i < count)) break;
    }
}

// A null dereference check of the compiler's smart pointers: a null pointer calls the assert helper with the message and goes on.
void require(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 message) {
    if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
}

// 0x088216C0 (a1 = mode, a2 = provider): with a1 > 0, builds the list from 0x08A98A70 / 0x08A98674(a2), walks its entries calling each one's
// virtual +0x84 / +0x50 and appending the result to a second list, then (unless a1 == 1 or the provider's virtual +0x3C is zero) re-reads the
// entries through 0x08821558. v0 = the last call's result (unchanged when a1 <= 0).
void func_088216C0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const s32 mode = static_cast<s32>(ctx.gpr[5]);
    const u32 provider = ctx.gpr[6];
    put(rt, frame.at(0x30u), provider);
    if (mode <= 0) return;
    constexpr u32 kNullMessage = 0x08C99660u;
    call(rt, ctx, 0x08A98A70u, {frame.at(0x10u)});
    call(rt, ctx, 0x08A98674u, {frame.at(0x14u), word(rt, frame.at(0x30u))});
    for (u32 index = 0;; ++index) {
        u32 list = word(rt, frame.at(0x14u));
        require(rt, ctx, list, kNullMessage);
        const u32 begin = word(rt, list + 0x14u);
        const u32 end = word(rt, list + 0x18u);
        if (!(index < static_cast<u32>(static_cast<s32>(end - begin) >> 2))) break;
        list = word(rt, frame.at(0x14u));
        require(rt, ctx, list, kNullMessage);
        call_virtual(rt, ctx, list, 0x84u, {frame.at(0xCu), list, index});
        u32 entry = word(rt, frame.at(0xCu));
        require(rt, ctx, entry, 0x08C995ECu);
        const GuestResult size = call_virtual(rt, ctx, entry, 0x50u, {entry});
        const u32 size_low = size.v0;
        const u32 size_high = size.v1;
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        call(rt, ctx, 0x088A79ACu, {frame.at(0x1Cu), 0u});
        call(rt, ctx, 0x08857EE4u, {frame.at(0x18u), frame.at(0x1Cu), 0u});
        entry = word(rt, frame.at(0x18u));
        require(rt, ctx, entry, 0x08C995DCu);
        const u32 resolved = call(rt, ctx, 0x08854678u, {entry});
        call(rt, ctx, 0x088A58A4u, {resolved, 0u, size_low, size_high});
        call(rt, ctx, 0x088A6FB4u, {resolved});
        u32 results = word(rt, frame.at(0x10u));
        require(rt, ctx, results, kNullMessage);
        results += 0x14u;
        const u32 write = word(rt, results + 4u);
        const u32 limit = word(rt, results + 8u);
        if (write == limit) {
            call(rt, ctx, 0x08B8A03Cu, {results, write, frame.at(0x18u)});
        } else {
            if (write != 0u) {
                call(rt, ctx, 0x08A76ABCu, {write, frame.at(0x18u)});
                put(rt, results + 4u, word(rt, results + 4u) + 4u);
            } else {
                put(rt, results + 4u, 4u);
            }
        }
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
        call(rt, ctx, 0x088A7548u, {frame.at(0x1Cu)});
    }
    const u32 view = frame.at(8u);
    call(rt, ctx, 0x08A983E8u, {view, frame.at(0x10u)});
    u32 flag = 1u;
    if (mode != 1) {
        const u32 queried = word(rt, word(rt, frame.at(0x30u)) + 4u);
        require(rt, ctx, queried, 0x08C995ECu);
        flag = call_virtual(rt, ctx, queried, 0x3Cu, {queried}).v0 != 0u ? 1u : 0u;
    }
    call(rt, ctx, 0x08821558u, {view, 0x088212F8u, flag});
    call(rt, ctx, 0x08A5B9ECu, {view});
    for (u32 index = 0;; ++index) {
        u32 list = word(rt, frame.at(0x10u));
        require(rt, ctx, list, kNullMessage);
        const u32 begin = word(rt, list + 0x14u);
        const u32 end = word(rt, list + 0x18u);
        if (!(index < static_cast<u32>(static_cast<s32>(end - begin) >> 2))) break;
        list = word(rt, frame.at(0x10u));
        require(rt, ctx, list, kNullMessage);
        const u32 slot = index << 2;
        call(rt, ctx, 0x088547C4u, {frame.at(4u), word(rt, list + 0x14u) + slot});
        const u32 target = word(rt, frame.at(4u));
        require(rt, ctx, target, 0x08C995DCu);
        const u32 resolved = call(rt, ctx, 0x08854678u, {target});
        const u32 resolved_2 = call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)}); // s1 takes the result of 0x08854678 in the delay slot
        (void)resolved_2;
        const u32 owner = word(rt, frame.at(0x14u));
        require(rt, ctx, owner, kNullMessage);
        const GuestResult pair = call_keep(rt, ctx, 0x088A5EE0u, {resolved});
        call(rt, ctx, 0x08A5DEE8u, {frame.at(0u), 0u, pair.v0, pair.v1});
        const u32 destination = word(rt, owner + 0x14u) + slot;
        if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
}

} // namespace
} // namespace new_batch_main_123
} // namespace gt::game

#define NEWM123(address, name) GT_DECOMP(address, gt::game::new_batch_main_123::name, gt::ReturnKind::Word)
NEWM123(0x088F9658u, func_088F9658);
NEWM123(0x088A7CE8u, func_088A7CE8);
NEWM123(0x088C64ACu, func_088C64AC);
NEWM123(0x08B37E30u, func_08B37E30);
NEWM123(0x08BA5470u, func_08BA5470);
NEWM123(0x08861CB0u, func_08861CB0);
NEWM123(0x08B23454u, func_08B23454);
NEWM123(0x08BCFAC0u, func_08BCFAC0);
NEWM123(0x08B1554Cu, func_08B1554C);
NEWM123(0x08AE8A1Cu, func_08AE8A1C);
NEWM123(0x088216C0u, func_088216C0);
