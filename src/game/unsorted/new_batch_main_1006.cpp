// Main-session batch 1006: functions the extra seeds skipped or that mismatched, structured C from tools.disasm.
// Draft only: the differential gate, the four audits and the synthetic gate run in the verifier.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1006 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using std::uint8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 signed_of(u32 v) { return static_cast<u32>(static_cast<s32>(v)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<uint8_t>(value)); }

GuestResult call_with_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// 0x08BB404C: allocator + tree insert. s6 = "the key sorts before the node" flag (the node's word at +0x10 points at a
// record whose float at +8 is the sort key); the global 0x08D75F5C is the allocator's state, created on first use.
// a1 non-zero (or a2 == a0 + 4) sorts before unconditionally.
void func_08BB404C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    const u32 node = ctx.gpr[6];
    const u32 key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 end = self + 4u;
    u32 insert_left;
    if (flag != 0u) {
        insert_left = 1u;
    } else if (node == end) {
        insert_left = 1u;
    } else {
        const float key_at = float_at(rt, word(rt, key) + 8u);
        const float node_at = float_at(rt, word(rt, node + 0x10u) + 8u);
        insert_left = key_at < node_at ? 1u : 0u;
    }

    const u32 list = 0x08D75F5Cu;
    if (rt.memory().load8(0x08D75F50u) == 0u) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D75F50u}) != 0u) {
            put(rt, list + 0x18u, 0x1001u);
            put(rt, list + 0x0Cu, 0x08D75F68u);
            put(rt, list + 0x10u, 0u);
            put(rt, list + 0x04u, 0u);
            put(rt, list + 0x08u, 0u);
            put(rt, list + 0x14u, 0x08D75F68u);
            put(rt, list + 0x1Cu, 0u);
            put(rt, list + 0x20u, 0u);
            put(rt, list + 0x24u, 0u);
            put(rt, list, 0u);
            call(rt, ctx, 0x08B64494u, {0x08D75F50u});
            call(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
        }
    }
    u32 tail = 0u;
    u32 position = word(rt, list + 0x14u);
    bool attach_key = true;
    bool final = false;
    for (;;) {
        const u32 holder = word(rt, position);
        const u32 size = word(rt, holder + 4u);
        if (size < 4u) {
            if (holder != word(rt, list + 0x14u)) {
                position = holder;
                continue;
            }
            u32 count = word(rt, list + 0x18u);
            u32 bytes;
            if (count < 5u) {
                count = 4u;
                bytes = 0x20u;
            } else {
                bytes = count << 3;
            }
            const u32 block = call(rt, ctx, 0x08B2B4D8u, {0x10u, bytes});
            if (block == 0u) {
                tail = 0u;
                attach_key = true;
                final = true;
                break;
            }
            put(rt, frame.at(0u), block);
            const u32 slot = word(rt, list + 0x18u);
            const u32 begin = word(rt, list + 0x04u);
            const u32 capacity_end = word(rt, list + 0x08u);
            put(rt, list + 0x18u, slot + (slot >> 1));
            if (begin == capacity_end) {
                call(rt, ctx, 0x08BB13D4u, {list, begin, frame.at(0u)});
            } else {
                if (begin != 0u) put(rt, begin, block);
                const u32 next_begin = (begin != 0u ? word(rt, list + 0x04u) : begin) + 4u;
                put(rt, list + 0x04u, next_begin);
            }
            put(rt, list + 0x1Cu, count + word(rt, list + 0x1Cu));
            const u32 allocated = word(rt, frame.at(0u));
            put(rt, allocated + 4u, count);
            const u32 total = count + word(rt, list + 0x20u);
            put(rt, list + 0x20u, total);
            u32 where = 0u;
            if (allocated + 8u != 0u) {
                where = word(rt, list + 0x14u);
                u32 below_value = 0u;
                for (;;) {
                    const u32 node_key = word(rt, where);
                    if (!(where < allocated)) {
                        if (where < node_key) {
                            where = node_key;
                            continue;
                        }
                        if (allocated < node_key) {
                            below_value = word(rt, allocated + 4u);
                            break;
                        }
                        where = node_key;
                        continue;
                    }
                    if (allocated < node_key) {
                        below_value = word(rt, allocated + 4u);
                        break;
                    }
                    if (where < node_key) {
                        where = node_key;
                        continue;
                    }
                    below_value = word(rt, allocated + 4u);
                    break;
                }
                put(rt, list + 0x20u, total - below_value);
                const u32 first_link = word(rt, allocated + 4u);
                const u32 first_value = word(rt, where);
                if (allocated + (first_link << 3) != first_value) {
                    put(rt, allocated, first_value);
                } else {
                    put(rt, allocated + 4u, first_link + word(rt, first_value + 4u));
                    put(rt, allocated, word(rt, first_value));
                }
                const u32 second_link = word(rt, where + 4u);
                if (allocated != where + (second_link << 3)) {
                    put(rt, where, allocated);
                } else {
                    put(rt, where + 4u, second_link + word(rt, allocated + 4u));
                    put(rt, where, word(rt, allocated));
                }
                put(rt, list + 0x14u, where);
            }
            // allocated + 8 == 0 or the search finished: continue from the list head, or finish at position 0.
            position = word(rt, list + 0x14u);
            if (position != 0u) continue;
            tail = 0u;
            attach_key = true;
            final = true;
            break;
        }
        u32 tail_node = holder;
        if (size != 4u) {
            const u32 reduced = size - 4u;
            put(rt, holder + 4u, reduced);
            tail_node = holder + (reduced << 3);
            put(rt, tail_node + 4u, 4u);
        } else {
            put(rt, position, word(rt, holder));
        }
        put(rt, list + 0x14u, position);
        const u32 next = word(rt, list + 0x20u) + 4u;
        put(rt, list + 0x20u, next);
        if (word(rt, list + 0x24u) < next) put(rt, list + 0x24u, next);
        tail = tail_node + 8u;
        attach_key = tail_node + 0x18u != 0u;
        final = true;
        break;
    }
    (void)final;
    if (attach_key) {
        const u32 key_value = word(rt, key);
        put(rt, tail + 0x10u, key_value);
        if (key_value != 0u) put(rt, key_value + 0x10u, word(rt, key_value + 0x10u) + 1u);
    }
    call(rt, ctx, 0x08B5F2B8u, {insert_left, tail, node, end});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = tail;
}

// 0x08A8CE2C: container assign/insert driven by a key (a2 is the key container, a1 the source, a0 the destination
// result object). The two globals are the empty-node sentinels of the key container's maps; the frame slots are the
// iterator temporaries the library calls fill.
void func_08A8CE2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const u32 t0 = frame.at(0u), t4 = frame.at(4u), t8 = frame.at(8u), tC = frame.at(0xCu);
    const u32 t10 = frame.at(0x10u), t14 = frame.at(0x14u), t18 = frame.at(0x18u);
    call(rt, ctx, 0x08A5BAB8u, {self, 0x08CF6ED8u});
    call(rt, ctx, 0x08A759E0u, {t14, key, 0u});
    const u32 empty_first = word(rt, 0x08CF6F04u);
    const u32 first = word(rt, t14);
    call(rt, ctx, 0x08A69D68u, {t14});
    u32 state;
    if (first == empty_first) {
        if (self == source + 0x24u) {
            state = word(rt, self);
        } else {
            call(rt, ctx, 0x08A5BAD8u, {self, word(rt, source + 0x24u)});
            state = word(rt, self);
        }
    } else {
        call(rt, ctx, 0x08A759E0u, {t10, key, 0u});
        const u32 empty_second = word(rt, 0x08CF6F18u);
        const u32 second = word(rt, t10);
        call(rt, ctx, 0x08A69D68u, {t10});
        if (second == empty_second) {
            if (self == source + 0x20u) {
                state = word(rt, self);
            } else {
                call(rt, ctx, 0x08A5BAD8u, {self, word(rt, source + 0x20u)});
                state = word(rt, self);
            }
        } else {
            call(rt, ctx, 0x08AAE3A8u, {t18, source + 0x20u});
            u32 element = word(rt, t18);
            while (element != 0u) {
                call(rt, ctx, 0x08A759E0u, {tC, key, 0u});
                const u32 found = call(rt, ctx, 0x08C2B010u, {element + 0x24u, tC});
                call(rt, ctx, 0x08A69D68u, {tC});
                element = word(rt, t18);
                if (element == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
                if (found == element + 0x28u) {
                    const s32 count = static_cast<s32>(word(rt, key + 4u) - word(rt, key)) >> 2;
                    if (count >= 2) {
                        element = word(rt, t18);
                        if (element == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
                        call(rt, ctx, 0x08AAE358u, {t8, element});
                        call(rt, ctx, 0x08A5BAD8u, {t18, word(rt, t8)});
                        call(rt, ctx, 0x08A5B9ECu, {t8});
                        element = word(rt, t18);
                        if (element != 0u) continue;
                    }
                } else if (self != found + 0x14u) {
                    call(rt, ctx, 0x08A5BAD8u, {self, word(rt, found + 0x14u)});
                }
                break;
            }
            call(rt, ctx, 0x08A5B9ECu, {t18});
            state = word(rt, self);
        }
    }
    if (state != 0u) {
        u32 index = 1u;
        for (;;) {
            u32 current = word(rt, self);
            if (current == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u});
            if (word(rt, current + 0x14u) == 8u) break;
            current = word(rt, self);
            if (current == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u});
            const u32 slot = word(rt, word(rt, current) + 0x18u);
            call_indirect(rt, ctx, slot, {t18, current});
            const u32 output = word(rt, t18);
            if (output != 0u) {
                call(rt, ctx, 0x08A759E0u, {t4, key, index});
                const u32 found = call(rt, ctx, 0x08C2B010u, {output + 0x24u, t4});
                call(rt, ctx, 0x08A69D68u, {t4});
                const u32 holder = word(rt, t18);
                if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
                if (found != holder + 0x28u) {
                    if (self != found + 0x14u) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, found + 0x14u)});
                } else {
                    call(rt, ctx, 0x08A5BAB8u, {t0, 0x08CF6ED8u});
                    if (self != t0) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, t0)});
                    call(rt, ctx, 0x08A5B9ECu, {t0});
                    call(rt, ctx, 0x08A5B9ECu, {t18});
                    break;
                }
            }
            call(rt, ctx, 0x08A5B9ECu, {t18});
            ++index;
            const u32 count = call(rt, ctx, 0x08A759BCu, {key});
            if (!(index < count)) break;
        }
    }
    ctx.gpr[2] = self;
}

// 0x08B13388: opens a stream, waits for the header to arrive, then lays out the self+0x208.. block from its parameters.
// Known-good shape from the earlier draft (hot_batch_115): v0 = 0x515111D3 when the header's magic is unknown, else the
// last helper's answer.

// 0x08C515B4: insert_unique into a node tree. The key is {flag byte +0, data pointer +4, length +8}; a node stores the
// flag at +0x10, the data pointer at +0x14 and the length at +0x18, and its children at +8 and +0xC. v0 = the node the
// iterator points at, v1 = inserted (the byte the original spills through sp+4).

// 0x08A7D710: same container machinery as 0x08A8CE2C, walking the key container's elements and calling the per-element
// virtuals (+0x1C then +0x10C) on the source; when the key container is empty the first/second sentinel arms build a
// one-element view first, and the third arm calls the source's +0x10C virtual once.

// 0x0897C134: draws/records the object selected by (a1, a2) against the two-float key the two early calls resolve.
// a2 == 1 with both flags already set returns 0; the present-byte table at a0+0x14A[2*a1+a2] gates the second record.
// Objects 0x08CC9888 / 0x08CC9758 are the two vtable-shaped results, and 0x08CDE768 the table of per-entry sizes.

// 0x08A91AEC: walks the chain starting at self+0x14 (each node's next comes from 0x08B5F100 until it reaches the local
// sentinel at sp+0x34) and hands every element whose key field matches self+0x14 to the element's +0x28 virtual; the
// others go through the 0x08A8CE2C view built at sp+0x18..0x24. a1 is the visitor passed to every virtual.

} // namespace
} // namespace new_batch_main_1006
} // namespace gt::game

#define NEW1006(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1006::name, gt::ReturnKind::kind)
NEW1006(0x08BB404Cu, func_08BB404C, Word);
NEW1006(0x08A8CE2Cu, func_08A8CE2C, Word);
