// New batch main-127 (main session draft, unverified): ten functions the recompiler emitted that no src/ file defines or registers.
// Written by hand from `tools.disasm` as structured C. Skipped (not registered): 0x088B7F74 (float math with jumps into the middle of
// the body), 0x08AE5878 and 0x0892AE14 (jump tables), 0x08A2BA74 (alloca + fp frame), 0x08BB404C (allocator + list insert).
// Every `jal`'s delay slot runs before the callee; `v0` is the last call's result unless the asm sets it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_127 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call with float arguments that leaves v0/v1/f0 as the callee left them.
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: v1 = the vtable (object[0]), the callee = vtable[slot]; the arguments include the object.
GuestResult vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    return call_indirect(rt, ctx, word(rt, table + slot), arguments, {});
}
constexpr u32 kAssert = 0x08A5BB7Cu;
constexpr u32 kRelease = 0x08A5B9ECu;

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08C23B9C (a0 = object, a1 = destination): copies the two lists at +0x24 and +0x18 of the object the word at +8 yields, element
// by element (12-byte records), into containers built with 0x08A6159C / 0x08A61624, then publishes them.
void copy_records(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 list_object, u32 list_offset, u32 multiplier, u32 &cursor_out) {
    // s3 = list_object + list_offset: {begin at +0, end at +4}
    const u32 list = list_object + list_offset;
    const u32 begin0 = word(rt, list_object + list_offset);
    const s32 bytes = static_cast<s32>(word(rt, list + 4u) - begin0);
    const u32 count_arg = static_cast<u32>(bytes >> 2) * multiplier;
    u32 cursor = call(rt, ctx, 0x08A6159Cu, {destination, count_arg});
    u32 begin = word(rt, list_object + list_offset);
    u32 end = word(rt, list + 4u);
    u32 offset = 0u;
    u32 index = 0u;
    for (;;) {
        const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2) * multiplier;
        const u32 target = begin + offset + 8u;
        offset += 0xCu;
        const bool more = index < count;
        ++index;
        if (!more) break;
        if (target != cursor) {
            call(rt, ctx, 0x08A5BAD8u, {target, word(rt, cursor)});
            end = word(rt, list + 4u);
            begin = word(rt, list);
        }
        cursor += 4u;
    }
    cursor_out = cursor;
}


// ---------------------------------------------------------------------------------------------------------------------------
// 0x088BDC24 (a0 = object, a1 = word, f12 = float stored for the callee, f13/f14 = a point, f15 = a limit): asks the object about
// the five ids of the table 0x08C89DE0, keeps the one whose point is nearest to (f13, f14), then settles the answer from
// the 9 / 8 / -1 fallbacks and a final distance check.
void func_088BDC24(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xA0u);
    const u32 self = ctx.gpr[4];
    const float px = ctx.fpr[13];
    const float py = ctx.fpr[14];
    const float limit = ctx.fpr[15];
    put(rt, frame.at(4u), 1u);
    put(rt, frame.at(0xCu), ctx.gpr[5]);
    put_float(rt, frame.at(0x10u), ctx.fpr[12]);
    put(rt, frame.at(0u), 1u);
    put_float(rt, frame.at(0x14u), 0.0f);
    float best = std::bit_cast<float>(0x7F7FFFFFu);
    float best_x = 0.0f;
    float best_y = 0.0f;
    s32 pick = -1;
    u32 table = 0x08C89DE0u;
    do {
        const u32 id = word(rt, table);
        table += 4u;
        const u32 answer = vcall(rt, ctx, self, 0x24u, {self, id}).v0;
        if (id != answer) continue;
        put(rt, frame.at(8u), id);
        const u32 other = vcall(rt, ctx, self, 0x1Cu, {self}).v0;
        if (vcall(rt, ctx, other, 8u, {other, self, frame.at(0u)}).v0 == 0u) continue;
        const float dx = float_at(rt, frame.at(0x28u)) - px;
        const float dy = float_at(rt, frame.at(0x2Cu)) - py;
        const float distance = callf(rt, ctx, 0x08B49D7Cu, {}, {dx * dx + dy * dy}).f0;
        if (distance < best) {
            best = distance;
            best_x = float_at(rt, frame.at(0x28u));
            best_y = float_at(rt, frame.at(0x2Cu));
            pick = static_cast<s32>(id);
        }
    } while (table != 0x08C89DF4u);
    if (pick == 9) {
        pick = static_cast<s32>(vcall(rt, ctx, self, 0x24u, {self, 2u}).v0);
    } else if (pick == 8) {
        pick = static_cast<s32>(vcall(rt, ctx, self, 0x24u, {self, 1u}).v0);
    }
    if (pick == -1) {
        pick = 6;
    } else if (best <= limit) {
        ctx.gpr[2] = static_cast<u32>(pick);
        return;
    }
    const u32 mode = vcall(rt, ctx, self, 0x24u, {self, 3u}).v0;
    if (mode != 3u) {
        ctx.gpr[2] = static_cast<u32>(pick);
        return;
    }
    put(rt, frame.at(8u), mode);
    const u32 other = vcall(rt, ctx, self, 0x1Cu, {self}).v0;
    if (vcall(rt, ctx, other, 8u, {other, self, frame.at(0u)}).v0 == 0u) {
        ctx.gpr[2] = static_cast<u32>(pick);
        return;
    }
    const float ex = float_at(rt, frame.at(0x28u)) - px;
    const float ey = float_at(rt, frame.at(0x2Cu)) - py;
    const float distance = callf(rt, ctx, 0x08B49D7Cu, {}, {ex * ex + ey * ey}).f0;
    if (!(limit < best - distance)) {
        ctx.gpr[2] = static_cast<u32>(pick);
        return;
    }
    if (distance < limit || 50.0f <= best) {
        ctx.gpr[2] = 3u;
        return;
    }
    const float along = ex * (best_x - px) + ey * (best_y - py);
    ctx.gpr[2] = (0.0f < along) ? 3u : static_cast<u32>(pick);
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x088B1C94 (a0 = object, a1 = other object): 1 when the other object's candidate segments pass the geometric test, else 0.
void func_088B1C94(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    ctx.gpr[2] = 0u;
    if (self == 0u) return;
    if (vcall(rt, ctx, self, 8u, {self}).v0 == 0u) return;
    ctx.gpr[2] = 0u;
    if (other == 0u) return;
    const u32 owner = vcall(rt, ctx, self, 0x20u, {self}).v0;
    s32 segments = static_cast<s32>(vcall(rt, ctx, other, 8u, {other}).v0);
    const bool positive = 0 < segments;
    if (owner == 0u) { ctx.gpr[2] = 0u; return; }
    if (!positive) { ctx.gpr[2] = 0u; return; }
    if (vcall(rt, ctx, self, 0x18u, {self, 9u}).v0 == 0u) { ctx.gpr[2] = 0u; return; }
    if (vcall(rt, ctx, self, 0x18u, {self, 8u}).v0 == 0u) { ctx.gpr[2] = 0u; return; }
    if (segments >= 3) segments = 2;
    s32 index = 0;
    const u32 box = frame.at(0x28u);
    for (;;) {
        const u32 got = vcall(rt, ctx, other, 0x10u, {other, static_cast<u32>(index), frame.at(0u)}).v0;
        ++index;
        if (got != 0u) {
            const float x = float_at(rt, frame.at(0u));
            const float z = float_at(rt, frame.at(8u));
            const float y = float_at(rt, frame.at(4u));
            put(rt, frame.at(0x18u), std::bit_cast<u32>(x));
            put(rt, frame.at(0x20u), std::bit_cast<u32>(z));
            put(rt, frame.at(0x10u), 0u);
            put(rt, frame.at(0x14u), 0u);
            put(rt, frame.at(0x1Cu), std::bit_cast<u32>(y));
            const float radius = vcall(rt, ctx, owner, 8u, {owner, frame.at(0x10u)}).f0;
            const float z2 = float_at(rt, frame.at(8u));
            const float y2 = float_at(rt, frame.at(4u));
            const float negz = -z2;
            const float x2 = float_at(rt, frame.at(0u));
            put_float(rt, frame.at(0x38u), radius);
            put_float(rt, frame.at(0x54u), negz);
            put_float(rt, frame.at(0x50u), x2);
            put_float(rt, frame.at(0x58u), y2);
            put_float(rt, frame.at(8u), y2);
            put_float(rt, frame.at(4u), negz);
            put(rt, frame.at(0x30u), 9u);
            put(rt, frame.at(0x28u), 1u);
            put(rt, frame.at(0x2Cu), 0u);
            put(rt, frame.at(0x34u), 0u);
            put(rt, frame.at(0x3Cu), 0u);
            const u32 first = vcall(rt, ctx, self, 0x1Cu, {self}).v0;
            vcall(rt, ctx, first, 8u, {first, self, box});
            put(rt, frame.at(0x30u), 8u);
            const float ax = float_at(rt, frame.at(0x50u));
            const float ay = float_at(rt, frame.at(0x54u));
            const u32 second = vcall(rt, ctx, self, 0x1Cu, {self}).v0;
            vcall(rt, ctx, second, 8u, {second, self, box});
            const float f4 = float_at(rt, frame.at(0x54u));
            const float f2 = float_at(rt, frame.at(0x50u));
            const float f0 = float_at(rt, frame.at(0u));
            const float f1 = float_at(rt, frame.at(4u));
            const float dx = f2 - ax;
            const float dy = f4 - ay;
            const float cross1 = f1 - f4;
            const float cross2 = f0 - f2;
            const float t1 = f1 - ay;
            const float t0 = f0 - ax;
            const float p_dot = dx * t0 + dy * t1;
            const float len = dx * dx + dy * dy;
            const float q_dot = dx * cross2 + dy * cross1;
            if (p_dot < 0.0f) {
                if (len < p_dot * p_dot) { ctx.gpr[2] = 1u; return; }
            } else if (0.0f < q_dot) {
                const float dx2 = dx * dx;
                const float dy2 = dy * dy;
                const float qq = q_dot * q_dot;
                if (dx2 + dy2 < qq) { ctx.gpr[2] = 1u; return; }
            }
        }
        if (segments == index) break;
    }
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08830FCC (a0 = destination, a2 = count, a3 = spec): builds the float vectors the spec's components yield and runs 0x088E0984.
void func_08830FCC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 spec = ctx.gpr[7];
    if (count < 2) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 holder = frame.at(4u);
    call(rt, ctx, 0x088297A8u, {holder});
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, kAssert, {0x08C9A2F8u});
    const u32 first = call(rt, ctx, 0x088294D0u, {object});
    call(rt, ctx, kRelease, {holder});
    const u32 handle = call(rt, ctx, 0x0885EDF8u, {first});
    u32 node = word(rt, spec);
    if (node == 0u) call(rt, ctx, kAssert, {0x08C9A2B4u});
    const u32 kind = vcall(rt, ctx, node, 0x3Cu, {node}).v0;
    const u32 vec_a = frame.at(8u);
    call(rt, ctx, 0x08A98674u, {vec_a, spec + 4u});
    // The k-th component node of the vector held at `slot`: its float, through the virtual at +0x54.
    auto component = [&](u32 slot, u32 k) {
        const u32 table = word(rt, word(rt, slot) + 0x14u);
        const u32 item = word(rt, table + 4u * k);
        if (item == 0u) call(rt, ctx, kAssert, {0x08C9A2B4u});
        return vcall(rt, ctx, item, 0x54u, {item}).f0;
    };
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put_float(rt, frame.at(0x10u), component(frame.at(8u), 0u));
    put_float(rt, frame.at(0x14u), component(frame.at(8u), 1u));
    put_float(rt, frame.at(0x18u), component(frame.at(8u), 2u));
    u32 built;
    if (count >= 3) {
        const u32 vec_b = frame.at(0xCu);
        call(rt, ctx, 0x08A98674u, {vec_b, spec + 8u});
        put(rt, frame.at(0x1Cu), 0u);
        put(rt, frame.at(0x20u), 0u);
        put(rt, frame.at(0x24u), 0u);
        put(rt, frame.at(0x28u), 0u);
        put_float(rt, frame.at(0x1Cu), component(vec_b, 0u));
        put_float(rt, frame.at(0x20u), component(vec_b, 1u));
        put_float(rt, frame.at(0x24u), component(vec_b, 2u));
        put_float(rt, frame.at(0x28u), component(vec_b, 3u));
        built = call(rt, ctx, 0x088E0984u, {handle, kind, frame.at(0x10u), frame.at(0x1Cu), 0u, 0u});
        call(rt, ctx, kRelease, {vec_b});
    } else {
        built = call(rt, ctx, 0x088E0984u, {handle, kind, frame.at(0x10u), 0u, 0u, 0u});
    }
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), built});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, kRelease, {frame.at(0u)});
    call(rt, ctx, kRelease, {vec_a});
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08A90034 (a0 = object, a1 = flag byte, a2 = key, a3 = value pointer, t0 = reference pointer).
// The block that clears the byte at +0x20 of the record's +0x14 entry around a 0x08A8FA4C call and then restores it.
void clear_and_notify(Runtime &rt, AllegrexContext &ctx, u32 self, u32 record, u32 slot) {
    u32 entry = word(rt, record + 0x14u);
    const u32 entry_at_start = entry;
    if (entry == 0u) {
        call(rt, ctx, kAssert, {0x08CB1D60u});
        entry = word(rt, record + 0x14u);
    }
    const u32 saved = rt.memory().load8(entry_at_start + 0x20u);
    if (entry == 0u) call(rt, ctx, kAssert, {0x08CB1D60u});
    rt.memory().store8(entry + 0x20u, 0u);
    entry = word(rt, record + 0x14u);
    if (entry == 0u) call(rt, ctx, kAssert, {0x08CB1D60u});
    const u32 third = word(rt, entry + 0x14u);
    call(rt, ctx, 0x08A8FA4Cu, {self, slot, third});
    entry = word(rt, record + 0x14u);
    if (entry == 0u) call(rt, ctx, kAssert, {0x08CB1D60u});
    rt.memory().store8(entry + 0x20u, static_cast<std::uint8_t>(saved));
}

void func_08A90034(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const u32 key = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    const u32 reference = ctx.gpr[8];
    const u32 list_slot = frame.at(0u);
    const u32 held_slot = frame.at(4u);
    const u32 item_slot = frame.at(8u);
    const u32 marker = word(rt, 0x08CF6FDCu);
    const u32 chosen = (word(rt, reference) != marker) ? reference : value;
    call(rt, ctx, 0x08A6972Cu, {list_slot, chosen});
    call(rt, ctx, 0x08A8CE2Cu, {held_slot, self, key});
    const u32 held = word(rt, held_slot);
    if (held == 0u) {
        const u32 settings = word(rt, self + 0x70u);
        if (rt.memory().load8(settings + 8u) == 0u && word(rt, value) != word(rt, 0x08CF6FA0u) && flag == 0u)
            call(rt, ctx, 0x08A8FA4Cu, {self, list_slot, 8u});
    } else {
        vcall(rt, ctx, held, 0x18u, {item_slot, held});
        const u32 item = word(rt, item_slot);
        if (item != 0u) {
            if (word(rt, value) != word(rt, 0x08CF6FA0u)) {
                const u32 found = call(rt, ctx, 0x08C2B010u, {item + 0x24u, value});
                const u32 again = word(rt, item_slot);
                if (again == 0u) call(rt, ctx, kAssert, {0x08CB1D54u});
                if (again + 0x28u == found) {
                    const u32 settings = word(rt, self + 0x70u);
                    if (rt.memory().load8(settings + 8u) == 0u && flag == 0u) call(rt, ctx, 0x08A8FA4Cu, {self, list_slot, 8u});
                } else {
                    const u32 owner = word(rt, self + 0x20u);
                    if (owner == 0u) call(rt, ctx, kAssert, {0x08CB1D54u});
                    call(rt, ctx, 0x08AAE678u, {owner, list_slot, found + 0x14u});
                    if (flag == 0u) clear_and_notify(rt, ctx, self, found, list_slot);
                }
            } else {
                const u32 tail = item + 0x28u;
                u32 node = word(rt, item + 0x30u);
                while (node != tail) {
                    const u32 owner = word(rt, self + 0x20u);
                    if (owner == 0u) call(rt, ctx, kAssert, {0x08CB1D54u});
                    const u32 slot = node + 0x10u;
                    call(rt, ctx, 0x08AAE678u, {owner, slot, node + 0x14u});
                    if (flag == 0u) clear_and_notify(rt, ctx, self, node, slot);
                    node = call(rt, ctx, 0x08B5F100u, {node});
                }
            }
        }
        call(rt, ctx, kRelease, {item_slot});
    }
    call(rt, ctx, kRelease, {held_slot});
    call(rt, ctx, 0x08A69D68u, {list_slot});
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08A5AD68 (a0 = destination, a1 = source object, a2 = layout object): copies the source's items into the destination, one
// block built from the layout (byte at +0x40 set) or item by item (the +0x3C items at +0x38).
void func_08A5AD68(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 layout = ctx.gpr[6];
    if (rt.memory().load8(source + 0x40u) != 0u) {
        u32 stride = word(rt, layout + 0x1Cu);
        if (stride == 0u) {
            const u32 owner = word(rt, layout + 0x18u);
            if (owner == 0u) call(rt, ctx, kAssert, {0x08CAFB0Cu});
            stride = word(rt, owner + 0xCu);
        }
        u32 element = word(rt, layout + 0x14u);
        if (element == 0u) call(rt, ctx, kAssert, {0x08CAFB04u});
        const u32 size = vcall(rt, ctx, element, 0x120u, {element}).v0;
        const u32 block = frame.at(0x14u);
        call(rt, ctx, 0x08A950D0u, {block, stride, size, 0x10u});
        u32 maker = word(rt, block);
        if (maker == 0u) call(rt, ctx, kAssert, {0x08CAFBC0u});
        const u32 first = vcall(rt, ctx, maker, 0xDCu, {maker, 0u}).v0;
        maker = word(rt, block);
        if (maker == 0u) call(rt, ctx, kAssert, {0x08CAFBC0u});
        const u32 second = vcall(rt, ctx, maker, 0xD8u, {maker}).v0;
        const u32 created = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
        call(rt, ctx, 0x08A56018u, {created, first, second, 0u, 0u});
        put(rt, frame.at(0u), created);
        call(rt, ctx, 0x08A5BAB8u, {frame.at(0x18u), frame.at(0u)});
        put(rt, frame.at(0x10u), source);
        call(rt, ctx, 0x08A52AA8u, {frame.at(0x1Cu), frame.at(0x10u)});
        call(rt, ctx, 0x08A57B54u, {frame.at(0xCu), frame.at(0x1Cu), frame.at(0x18u), first});
        const u32 built = word(rt, frame.at(0xCu));
        if (built == 0u) call(rt, ctx, kAssert, {0x08CAFBB4u});
        call(rt, ctx, 0x08A56974u, {built});
        call(rt, ctx, kRelease, {frame.at(0xCu)});
        call(rt, ctx, 0x08A76ABCu, {destination, block});
        call(rt, ctx, kRelease, {frame.at(0x1Cu)});
        call(rt, ctx, kRelease, {frame.at(0x18u)});
        call(rt, ctx, kRelease, {block});
        ctx.gpr[2] = destination;
        return;
    }
    call(rt, ctx, 0x08A98C1Cu, {frame.at(0x1Cu), word(rt, source + 0x3Cu)});
    if (word(rt, source + 0x3Cu) != 0u) {
        u32 index = 0u;
        do {
            u32 element = word(rt, source + 0x38u);
            if (element == 0u) call(rt, ctx, kAssert, {0x08CAFB04u});
            const u32 size = vcall(rt, ctx, element, 0x120u, {element}).v0;
            const u32 offset = index * size;
            u32 base = word(rt, layout + 0x1Cu);
            if (base == 0u) {
                const u32 owner = word(rt, layout + 0x18u);
                if (owner == 0u) call(rt, ctx, kAssert, {0x08CAFB0Cu});
                base = word(rt, owner + 0xCu);
            }
            const u32 target_slot = word(rt, word(rt, frame.at(0x1Cu)) + 0x14u) + (index << 2);
            call(rt, ctx, 0x08A57B54u, {frame.at(4u), source + 0x38u, layout + 0x18u, base + offset});
            const u32 item = word(rt, frame.at(4u));
            if (item == 0u) call(rt, ctx, kAssert, {0x08CAFBB4u});
            vcall(rt, ctx, item, 0xF0u, {frame.at(8u), item});
            if (target_slot != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {target_slot, word(rt, frame.at(8u))});
            call(rt, ctx, kRelease, {frame.at(8u)});
            call(rt, ctx, kRelease, {frame.at(4u)});
            ++index;
        } while (index < word(rt, source + 0x3Cu));
    }
    call(rt, ctx, 0x08A76ABCu, {destination, frame.at(0x1Cu)});
    call(rt, ctx, kRelease, {frame.at(0x1Cu)});
    ctx.gpr[2] = destination;
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08884788 (a0 = object, a2 = chunk): dispatch on the chunk's tag ('DISP', 'REND', 'LPRB'); returns 0.
// One chunk field: 0x08AC2730 builds the reader at `slot`, 0x08AC2894 reads it, 0x08AC26DC closes it.
u32 read_field(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 chunk, u32 index) {
    call(rt, ctx, 0x08AC2730u, {slot, chunk, index});
    const u32 value = call(rt, ctx, 0x08AC2894u, {slot});
    call(rt, ctx, 0x08AC26DCu, {slot});
    return value;
}

void func_08884788(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 chunk = ctx.gpr[6];
    const u32 tag = call(rt, ctx, 0x08AC2690u, {chunk});
    if (tag == 0x4C505242u) {
        call(rt, ctx, 0x08AC2730u, {frame.at(0u), chunk, 0u});
        const u32 value = call(rt, ctx, 0x08AC2900u, {frame.at(0u)});
        call(rt, ctx, 0x08AC26DCu, {frame.at(0u)});
        call(rt, ctx, 0x08884758u, {self, value});
    } else if (tag == 0x52454E44u) {
        const u32 field0 = read_field(rt, ctx, frame.at(0x24u), chunk, 0u);
        put(rt, frame.at(0x38u), field0);
        const u32 field1 = read_field(rt, ctx, frame.at(0x20u), chunk, 1u);
        const u32 field2 = read_field(rt, ctx, frame.at(0x1Cu), chunk, 2u);
        put(rt, frame.at(0x34u), field2);
        const u32 field3 = read_field(rt, ctx, frame.at(0x18u), chunk, 3u);
        put(rt, frame.at(0x30u), field3);
        const u32 field4 = read_field(rt, ctx, frame.at(0x14u), chunk, 4u);
        const u32 field5 = read_field(rt, ctx, frame.at(0x10u), chunk, 5u);
        const u32 field6 = read_field(rt, ctx, frame.at(0xCu), chunk, 6u);
        const u32 field7 = read_field(rt, ctx, frame.at(8u), chunk, 7u);
        const u32 field8 = read_field(rt, ctx, frame.at(4u), chunk, 8u);
        u32 flags = word(rt, self + 0x10u);
        flags = (flags & ~1u) | (word(rt, frame.at(0x38u)) & 1u);
        flags = (flags & ~2u) | ((field1 & 1u) << 1);
        flags = (flags & ~4u) | ((field6 & 1u) << 2);
        put(rt, self + 0x10u, flags);
        const u32 f3 = word(rt, frame.at(0x30u));
        const u32 f2 = word(rt, frame.at(0x34u));
        u32 bits;
        if (field5 != 0u) {
            bits = (f3 != 0u) ? 0u : 0x20u;
            if (field4 == 0u) bits |= 4u;
            put(rt, self + 0xCu, bits);
        } else if (field8 != 0u) {
            bits = (f2 != 0u) ? 0u : 0x80u;
            if (f3 == 0u) bits |= 0x10u;
            if (field4 == 0u) bits |= 2u;
            put(rt, self + 0xCu, bits);
        } else if (field7 != 0u) {
            put(rt, self + 0xCu, (f2 != 0u) ? 0x10000u : 0x10080u);
        } else {
            bits = (f2 != 0u) ? 0u : 0x80u;
            if (f3 == 0u) bits |= 8u;
            if (field4 == 0u) bits |= 1u;
            put(rt, self + 0xCu, bits);
        }
    } else if (tag == 0x44495350u) {
        const u32 value = read_field(rt, ctx, frame.at(0x28u), chunk, 0u);
        const u32 flags = word(rt, self + 0x10u);
        put(rt, self + 0x10u, (flags & ~8u) | ((value & 1u) << 3));
    }
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x0897B800 (a0 = object): rebuilds the circular list at +0x184 from the entries the bit set at +0x74 keeps.
// The bit of the last element of the set at `bits`: {+8 = words, +0xC = count}.
bool last_bit_set(Runtime &rt, u32 bits) {
    const s32 count = static_cast<s32>(word(rt, bits + 0xCu));
    const u32 words = word(rt, bits + 8u);
    const s32 index = count - 1;
    const u32 bias = static_cast<u32>(index >> 31) >> 27;
    const s32 word_index = (index < 0) ? count + 0x1E : index;
    s32 bit = static_cast<s32>(((static_cast<u32>(index) + bias) & 0x1Fu) - bias);
    u32 address = words + (static_cast<u32>(word_index >> 5) << 2);
    if (bit < 0) {
        bit += 0x20;
        address -= 4u;
    }
    return ((1u << (static_cast<u32>(bit) & 31u)) & word(rt, address)) != 0u;
}


// ---------------------------------------------------------------------------------------------------------------------------
// 0x088B82DC (no arguments): returns the payloads at 0x08D75FC0 and 0x08D75F9C to the free list at 0x08D75F5C (inserting each block
// sorted by address and merging it with its neighbours) around two 0x08BB1B20 / 0x08BB1B30 calls.
void release_block(Runtime &rt, AllegrexContext &ctx, u32 payload) {
    constexpr u32 kGuard = 0x08D75F50u;
    constexpr u32 kState = 0x08D75F5Cu;
    if (rt.memory().load8(kGuard) == 0u) {
        if (call(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            put(rt, kState + 0x18u, 0x1001u);
            put(rt, kState + 0xCu, 0x08D75F68u);
            put(rt, kState + 0x10u, 0u);
            put(rt, kState + 4u, 0u);
            put(rt, kState + 8u, 0u);
            put(rt, kState + 0x14u, 0x08D75F68u);
            put(rt, kState + 0x1Cu, 0u);
            put(rt, kState + 0x20u, 0u);
            put(rt, kState + 0x24u, 0u);
            put(rt, kState, 0u);
            call(rt, ctx, 0x08B64494u, {kGuard});
            call(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
        }
    }
    u32 anchor = word(rt, kState + 0x14u);
    const u32 block = payload - 8u;
    for (;;) {
        const bool below = anchor < block;
        const u32 next = word(rt, anchor);
        if (below && block < next) break;
        const bool ahead = anchor < next;
        const bool block_before_next = block < next;
        if (!ahead) {
            if (below) break;
            if (block_before_next) break;
        }
        anchor = next;
    }
    put(rt, kState + 0x20u, word(rt, kState + 0x20u) - word(rt, block + 4u));
    u32 size = word(rt, block + 4u);
    u32 follower = word(rt, anchor);
    if (block + (size << 3) == follower) {
        const u32 follower_size = word(rt, follower + 4u);
        const u32 follower_next = word(rt, follower);
        put(rt, block + 4u, size + follower_size);
        put(rt, block, follower_next);
    } else {
        put(rt, block, follower);
    }
    size = word(rt, anchor + 4u);
    if (block != anchor + (size << 3)) {
        put(rt, anchor, block);
    } else {
        const u32 merged = word(rt, block + 4u);
        const u32 merged_next = word(rt, block);
        put(rt, anchor + 4u, size + merged);
        put(rt, anchor, merged_next);
    }
    put(rt, kState + 0x14u, anchor);
}


// ---------------------------------------------------------------------------------------------------------------------------
// 0x088D7214 (a0 = object, a1 = flag byte): waits for the object's workers to drain, runs the three-phase shutdown and clears
// its state; 1 when it ran, 0 when a phase refused.
void func_088D7214(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 wait = ctx.gpr[5] & 0xFFu;
    call(rt, ctx, 0x088DC7C0u, {self, 0u});
    const u32 busy = word(rt, self + 0x640u);
    rt.memory().store8(self + 0x5BCu, 0u);
    if (busy != 0u) {
        do {
            call(rt, ctx, 0x08B2E2B8u, {0x3E8u});
        } while (word(rt, self + 0x640u) != 0u);
    }
    ctx.gpr[3] = 1u;
    if (rt.memory().load8(self + 0x5B5u) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (call(rt, ctx, 0x088DAA84u, {self}) != 0u) {
        if (wait != 0u) {
            for (;;) {
                if (call(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu}) != 0u) break;
                if (call(rt, ctx, 0x08B1A844u) != 0u) break;
                call(rt, ctx, 0x08B2E2B8u, {0x7D0u});
            }
            while (call(rt, ctx, 0x088DAA84u, {self}) != 0u) {
                if (call(rt, ctx, 0x08B1A844u) != 0u) break;
                call(rt, ctx, 0x08B2E2B8u, {0x7D0u});
            }
        } else {
            if (call(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu}) == 0u) {
                ctx.gpr[3] = 0u;
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
    if (call(rt, ctx, 0x088D9E58u, {self}) != 0u) call(rt, ctx, 0x088D6E9Cu, {self});
    const s32 pending = static_cast<s32>(call(rt, ctx, 0x088DA168u, {self}));
    bool shut_down = true;
    if (pending > 0) {
        bool quiet;
        do {
            call(rt, ctx, 0x08B2E2B8u, {0x3E8u});
            quiet = true;
            s32 index = 0;
            u32 stride = 0u;
            for (;;) {
                const s32 total = static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self}));
                const bool inside = index < total;
                const u32 at = index;
                ++index;
                if (!inside) break;
                const u32 entry = word(rt, self + 0xC0u) + stride;
                stride += 0x2F8u;
                if (word(rt, entry + 4u) != 3u) {
                    if (call(rt, ctx, 0x088DE8D4u, {self, at}) == 0u) quiet = false;
                }
            }
        } while (!quiet);
        if (wait == 0u) {
            shut_down = false;
        } else {
            s32 ticks = 0;
            for (;;) {
                if (static_cast<s32>(call(rt, ctx, 0x088DA168u, {self})) <= 0) break;
                ++ticks;
                if (ticks == 500 * (ticks / 500)) {
                    s32 index = 0;
                    for (;;) {
                        const s32 total = static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self}));
                        const bool inside = index < total;
                        const u32 at = index;
                        ++index;
                        if (!inside) break;
                        call(rt, ctx, 0x088DE8D4u, {self, at});
                    }
                }
                if (call(rt, ctx, 0x08B1A844u) != 0u) {
                    put(rt, self + 0x11Cu, 0u);
                    put(rt, self + 0x134u, 0u);
                    call(rt, ctx, 0x08B2E2B8u, {0x3E80u});
                    break;
                }
                call(rt, ctx, 0x08B2E2B8u, {0x7D0u});
            }
        }
    }
    if (!shut_down) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x088D1E78u, {word(rt, self + 0x618u)});
    if (word(rt, self + 0x628u) != 0u) put(rt, self + 0x628u, 0u);
    if (word(rt, self + 0x630u) != 0u) put(rt, self + 0x630u, 0u);
    call(rt, ctx, 0x088E0C50u, {self});
    call(rt, ctx, 0x08B043D8u, {word(rt, self + 0x5D0u)});
    call(rt, ctx, 0x088D5AFCu, {self + 4u});
    call(rt, ctx, 0x088D5AFCu, {self + 0xCu});
    call(rt, ctx, 0x088D4D24u, {self + 4u});
    rt.memory().store8(self + 0x5B5u, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x59Cu, 0u);
    rt.memory().store8(self + 0x5C0u, 0u);
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

} // namespace
} // namespace new_batch_main_127
} // namespace gt::game

#define NEW127(address, name) GT_DECOMP(address, gt::game::new_batch_main_127::name, gt::ReturnKind::Word)
NEW127(0x088B1C94u, func_088B1C94);
NEW127(0x088BDC24u, func_088BDC24);
NEW127(0x088D7214u, func_088D7214);
NEW127(0x08830FCCu, func_08830FCC);
NEW127(0x08884788u, func_08884788);
NEW127(0x08A5AD68u, func_08A5AD68);
NEW127(0x08A90034u, func_08A90034);
