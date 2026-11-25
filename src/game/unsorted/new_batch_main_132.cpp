// New batch main-132 (main session): large functions emitted by the recompiler that no src/ file defines. Written by hand from
// `tools/disasm` as structured C; a draft batch, not yet verified by the differential gate. The helpers cover the idioms the
// bodies repeat: the guest call that leaves v0/v1/f0 as the callee left them, the compiler's virtual call, the inlined
// intrusive-counted release (use count at +4, weak count at +8, virtual dispose at +8 and destroy at +0xC) and the inlined
// copy-on-write string release.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_132 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call; afterwards v0 holds its result.
struct Env {
    Runtime &rt;
    AllegrexContext &ctx;

    u32 call(u32 target, std::initializer_list<u32> arguments = {}) {
        ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
        return ctx.gpr[2];
    }
    // The callee's v0/v1/f0 stay in the context, as after the original's `jal`.
    GuestResult call_keep(u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats = {}) {
        const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
        ctx.gpr[2] = result.v0;
        ctx.gpr[3] = result.v1;
        ctx.fpr[0] = result.f0;
        return result;
    }
    // The compiler's virtual call: the callee is the word at `offset` of the table at *object.
    GuestResult vcall(u32 object, u32 offset, std::initializer_list<u32> arguments, std::initializer_list<float> floats = {}) {
        const u32 table = word(rt, object);
        const u32 function = word(rt, table + offset);
        ctx.gpr[2] = table;
        return call_keep(function, arguments, floats);
    }
    u32 w(u32 address) { return word(rt, address); }
    void set(u32 address, u32 value) { put(rt, address, value); }
    u32 byte(u32 address) { return rt.memory().load8(address); }
    void set_byte(u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
    float f(u32 address) { return float_at(rt, address); }
    // The null-object assert before a dereference: the message at a0.
    void check(u32 pointer, u32 message) {
        if (pointer == 0u) call(0x08A5BB7Cu, {message});
    }
    // The inlined release of an intrusive-counted block (ll/sc loops on the use count at +4 and the weak count at +8).
    void release_counted(u32 block) {
        if (block == 0u) return;
        const u32 uses = w(block + 4u);
        set(block + 4u, uses - 1u);
        ctx.gpr[2] = 1u;
        if (uses != 1u) return;
        vcall(block, 8u, {block});
        const u32 weaks = w(block + 8u);
        set(block + 8u, weaks - 1u);
        ctx.gpr[2] = 1u;
        if (weaks != 1u) return;
        vcall(block, 0xCu, {block});
    }
    // The inlined copy-on-write string release: the data pointer is the representation + 0xC; the shared empty
    // representation is never touched, else the count at data - 4 goes through 0x08B60804 and a non-positive answer frees it.
    void release_string(u32 data, u32 empty, u32 free_function, u32 allocator) {
        const u32 representation = data - 0xCu;
        if (representation == empty) return;
        const u32 left = call(0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(left) > 0) return;
        call(free_function, {representation, allocator});
    }
    // 0x08A5B9EC on the slot at `address` (the smart pointer's release).
    void release_slot(u32 address) { call(0x08A5B9ECu, {address}); }
};

// The 760-byte car record offset the compiler built from shifts: ((i << 7) - (i << 5) - i) << 3.
constexpr u32 record_offset(u32 i) { return (((i << 7) - (i << 5)) - i) << 3; }

constexpr u32 kEmptyRepresentation = 0x08D74FD0u;

// 0x08808F2C (a0 = destination, a1 = count, a2 = array of objects, a3 = flag byte, t0 = callback): v0 = a0.
// A count under 4 builds the empty result. Otherwise the first object's name and the second entry select a node (0x08808690 /
// the +0x70 method), the third entry picks its child list; the destination receives the list of the children the fourth entry's
// +0x9C method (optionally run through the callback) answers, each also attached to a child holder (+0xA0 / +0x88 methods).
void func_08808F2C(Runtime &rt, AllegrexContext &ctx) {
    Env e{rt, ctx};
    GuestFrame frame(ctx, 0x90u);
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 array = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    const u32 callback = ctx.gpr[8];
    e.set(frame.at(0x50u), destination);
    e.set(frame.at(0x54u), flag);
    auto finish = [&]() { ctx.gpr[2] = e.w(frame.at(0x50u)); };
    auto release_tail = [&]() { // the three slots every path after the first node lookup releases
        e.release_slot(frame.at(0x30u));
        e.release_slot(frame.at(0x2Cu));
        e.release_slot(frame.at(0x28u));
    };

    if (count < 4) {
        e.call(0x08A76AACu, {destination, 0x08CF6ED8u});
        finish();
        return;
    }
    const u32 first = e.w(array);
    e.check(first, 0x08C97FCCu);
    e.vcall(first, 0xCu, {frame.at(0x24u), first});
    e.call(0x08808690u, {frame.at(0x28u), frame.at(0x24u)});
    e.release_string(e.w(frame.at(0x24u)), kEmptyRepresentation, 0x08B8542Cu, frame.at(3u));
    e.call(0x08A76ABCu, {frame.at(0x2Cu), array + 4u});
    const u32 owner = e.w(frame.at(0x28u));
    e.check(owner, 0x08C980D8u);
    e.vcall(owner, 0x70u, {frame.at(0x20u), owner, frame.at(0x2Cu)});
    e.call(0x08A40CF8u, {frame.at(0x30u), frame.at(0x20u)});
    e.release_slot(frame.at(0x20u));
    const u32 node = e.w(frame.at(0x30u));
    if (node == 0u) {
        const u32 name_owner = e.w(frame.at(0x2Cu));
        e.check(name_owner, 0x08C97FCCu);
        e.vcall(name_owner, 0xCu, {frame.at(0x48u), name_owner});
        e.call(0x08A76AACu, {e.w(frame.at(0x50u)), 0x08CF6ED8u});
        e.release_string(e.w(frame.at(0x48u)), kEmptyRepresentation, 0x08B8542Cu, frame.at(2u));
        release_tail();
        finish();
        return;
    }
    e.call(0x08A41258u, {frame.at(0x34u), node, 0u});
    e.call(0x08A76ABCu, {frame.at(0x38u), array + 8u});
    const u32 selector = e.w(frame.at(0x34u));
    e.check(selector, 0x08C980D8u);
    e.vcall(selector, 0x70u, {frame.at(0x18u), selector, frame.at(0x38u)});
    e.call(0x08A40CF8u, {frame.at(0x1Cu), frame.at(0x18u)});
    e.call(0x08A5BAD8u, {frame.at(0x30u), e.w(frame.at(0x1Cu))});
    e.release_slot(frame.at(0x1Cu));
    e.release_slot(frame.at(0x18u));
    if (e.w(frame.at(0x30u)) == 0u) {
        const u32 name_owner = e.w(frame.at(0x38u));
        e.check(name_owner, 0x08C97FCCu);
        e.vcall(name_owner, 0xCu, {frame.at(0x48u), name_owner});
        e.call(0x08A76AACu, {e.w(frame.at(0x50u)), 0x08CF6ED8u});
        e.release_string(e.w(frame.at(0x48u)), kEmptyRepresentation, 0x08B8542Cu, frame.at(1u));
        e.release_slot(frame.at(0x38u));
        e.release_slot(frame.at(0x34u));
        release_tail();
        finish();
        return;
    }

    const u32 list_node = frame.at(0x3Cu);
    const u32 total = e.call(0x08A40BF8u, {e.w(frame.at(0x30u))});
    e.call(0x08A76ABCu, {list_node, array + 0xCu});
    e.call(0x08A813F4u, {frame.at(0x40u)});
    e.call(0x08A98C1Cu, {frame.at(0x44u), total});
    u32 attached = 0u;
    if (static_cast<s32>(total) > 0) {
        for (u32 index = 0u; index != total; ++index) {
            const u32 source = e.w(frame.at(0x30u));
            e.check(source, 0x08C980E4u);
            e.call(0x08A41258u, {frame.at(0x14u), source, index});
            e.call(0x08A5BAD8u, {frame.at(0x34u), e.w(frame.at(0x14u))});
            e.release_slot(frame.at(0x14u));
            const u32 entry = e.w(frame.at(0x34u));
            e.check(entry, 0x08C980D8u);
            e.call(0x08A419E0u, {frame.at(0x48u), entry});
            const u32 child_source = e.w(frame.at(0x48u));
            e.check(child_source, 0x08C980F4u);
            e.vcall(child_source, 0x9Cu, {frame.at(0x4Cu), child_source, list_node});
            u32 child = e.w(frame.at(0x4Cu));
            if (child != 0u) {
                if (callback != 0u) {
                    e.vcall(child, 0xCu, {frame.at(0xCu), child});
                    e.call_keep(callback, {frame.at(0x10u), frame.at(0xCu)});
                    e.call(0x08A5BAD8u, {frame.at(0x4Cu), e.w(frame.at(0x10u))});
                    e.release_slot(frame.at(0x10u));
                    e.release_string(e.w(frame.at(0xCu)), kEmptyRepresentation, 0x08B8542Cu, frame.at(0u));
                    child = e.w(frame.at(0x4Cu));
                }
                if (child != 0u) {
                    if (e.w(frame.at(0x54u)) == 0u) {
                        const u32 holder = e.w(frame.at(0x40u));
                        e.check(holder, 0x08C980F4u);
                        e.vcall(holder, 0xA0u, {holder, frame.at(0x4Cu), frame.at(0x48u)});
                    }
                    const u32 sink = e.w(frame.at(0x44u));
                    e.check(sink, 0x08C980FCu);
                    e.vcall(sink, 0x88u, {sink, attached, frame.at(0x4Cu)});
                    ++attached;
                }
            }
            e.release_slot(frame.at(0x4Cu));
            e.release_slot(frame.at(0x48u));
        }
    }
    const u32 sink = e.w(frame.at(0x44u));
    e.check(sink, 0x08C980FCu);
    e.call(0x08A76AA0u, {frame.at(4u)});
    const u32 vector = sink + 0x14u;
    const u32 begin = e.w(sink + 0x14u);
    const u32 size = static_cast<u32>((static_cast<s32>(e.w(vector + 4u) - begin)) >> 2);
    if (attached < size) {
        e.call(0x08B85798u, {vector, begin + (attached << 2), e.w(sink + 0x18u)});
    } else {
        e.call(0x08B85F30u, {vector, e.w(sink + 0x18u), attached - size, frame.at(4u)});
    }
    e.release_slot(frame.at(4u));
    if (e.w(frame.at(0x54u)) != 0u) {
        e.call(0x08A76ABCu, {e.w(frame.at(0x50u)), frame.at(0x44u)});
    } else {
        e.call(0x08808948u, {frame.at(8u), frame.at(0x40u), frame.at(0x44u)});
        e.call(0x08A76ABCu, {e.w(frame.at(0x50u)), frame.at(8u)});
        e.release_slot(frame.at(8u));
    }
    e.release_slot(frame.at(0x44u));
    e.release_slot(frame.at(0x40u));
    e.release_slot(list_node);
    e.release_slot(frame.at(0x38u));
    e.release_slot(frame.at(0x34u));
    release_tail();
    finish();
}

// 0x088B0C18 (a0 = state object): drives the object's list cursor. In state 0 or 0xDF it asks the list object at +0x38 (vtable
// +8 / +0xC / +0x10 / +0x30) for the current entry, keeps the entry's counted pointer at +0x50/+0x54 and walks the entries
// until one yields a name (+0x58) or the cursor ends; the handle built at sp+0x10 is released at the end. v0 is not
// meaningful (the original leaves the last release's).


// 0x088BEC3C (a0 = controller): returns the speed factor in f0 (1.0 when the body's mode byte is 4). The body at +0x24 keeps a gear
// index byte at +0x11 that steps toward the target the object at +0x1C reports (limit, compared with the speed value read through
// 0x088DF13C); once the lateral distance to the path point is inside the allowed reach the mode byte at +0x12 advances (3 or 4) and
// the factor stays 1.0; the result is scaled by the tuning word at +0x40.

// 0x08B5D788 (a0 = registry, a1 = owner word): the module's static initialiser. Clears the two 14-word tables, publishes the
// registry's fields, then builds each singleton in .bss (vtable word, count 1, zeroed members), registers it through 0x08B5D3DC
// under its id word and finally stores each singleton at the registry slot 0x08B5C2B0 finds for its id. v0 is the address of the
// last slot written.


// 0x088DD144 (a0 = world, a1 = car index, a2 = value, a3 = "held" flag byte, t0 = second flag byte): refreshes one car slot
// (760-byte records at world+0xC0): picks the action from the controller's mode (0x088D6358), updates the slot's state through
// 0x088E7F.. / 0x088E8A44, mirrors the result into the entry holder's virtuals (+0x24 / +0x30 / +0x34 / +0x44) and then runs the
// follow-up hooks. v0 is the last call's.
void func_088DD144(Runtime &rt, AllegrexContext &ctx) {
    Env e{rt, ctx};
    GuestFrame frame(ctx, 0xA0u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    u32 value = ctx.gpr[6]; // s1
    const u32 flag = ctx.gpr[7] & 0xFFu;
    const u32 held = ctx.gpr[8] & 0xFFu;
    const u32 car = e.w(self + 0xC0u) + record_offset(index); // s5
    e.set(frame.at(0x58u), flag);
    e.set(frame.at(0x5Cu), held);
    e.call(0x088E7F1Cu, {car});
    if (e.call(0x088E8920u, {car, flag}) == 0u) return;
    const u32 controller = self + 4u; // fp
    u32 mode = e.call(0x088D6358u, {controller}); // s6
    e.set(frame.at(0x54u), e.call(0x088F8D58u, {e.w(self + 0x14Cu), index}));
    const u32 own = (e.w(car + 0x1Cu) == 1u) ? 1u : 0u;
    e.set(frame.at(0x50u), own);
    u32 busy = 1u; // s0
    if (e.call(0x088D62FCu, {controller}) != 0u) busy = (e.byte(e.w(self + 0x130u) + 0x18u) != 0u) ? 1u : 0u;
    if (own != 0u) {
        const u32 other = e.w(self + 0x640u);
        if (other != 0u) {
            const GuestResult pair = e.call_keep(0x088E4860u, {other, index});
            if ((pair.v0 | pair.v1) != 0u) busy = 1u;
        }
    }
    e.call(0x088D2F9Cu, {frame.at(0x24u)});
    s32 sustain; // s7
    if (own == 0u || busy != 0u) {
        mode = e.call(0x088D6358u, {controller});
        bool plain = (mode == 0u);
        if (!plain) {
            if (mode == 2u && e.call(0x088D62FCu, {controller}) == 1u) mode = (e.call(0x088DB2C8u, {self, index}) != 0u) ? 2u : 1u;
            if (mode == 0xAu || mode == 0xDu || mode == 0xEu) {
                plain = true;
            } else if (mode == 2u) {
                e.call(0x088E8A44u, {car, 3u});
                value = e.call(0x088E7F14u, {car});
            } else {
                e.call(0x088E8A44u, {car, 7u});
            }
        }
        if (plain) e.call(0x088E8A44u, {car, 1u});
        e.call(0x088D414Cu, {frame.at(0u), e.w(self + 0x620u), value, mode});
        e.call(0x088D2FC8u, {frame.at(0x24u), frame.at(0u)});
        sustain = static_cast<s32>(e.w(frame.at(0x40u)));
        e.call(0x088E7FA4u, {car, frame.at(0x24u), frame.at(0x30u)});
    } else {
        e.call(0x088E7FECu, {car, frame.at(0x24u), frame.at(0x30u)});
        sustain = 0;
    }
    e.call(0x088E8CC0u, {car});
    e.set_byte(e.w(self + 0x598u) + index * 8u, 1u);
    if (flag == 0u) {
        const u32 holder = e.w(self + 0x148u);
        e.vcall(holder, 0x24u, {holder, index});
    }
    const u32 records = e.w(self + 0xC0u);
    const u32 holder = e.w(self + 0x148u);
    const u32 holder_table = e.w(holder);
    const s32 slot = static_cast<std::int8_t>(e.byte(e.w(self + 0x130u) + index + 0x54u));
    const u32 slot_state = e.w(records + record_offset(static_cast<u32>(slot)) + 0x1Cu);
    const u32 price = e.call(0x088DD0E4u, {self, index});
    const u32 track = (slot_state == 1u) ? 1u : 0u;
    e.call_keep(e.w(holder_table + 0x30u), {holder, index, frame.at(0x24u), frame.at(0x30u), track, price, e.w(frame.at(0x54u)), e.w(self + 0x5D0u)});
    if (mode != 0xCu && sustain > 0) {
        const u32 tail = e.w(self + 0x14Cu);
        const s32 reported = static_cast<s32>(e.vcall(tail, 0x34u, {tail, index}).v0);
        if (reported > 0 && reported < sustain) sustain = reported;
        const float scaled = static_cast<float>(sustain) * std::bit_cast<float>(0x3E8E38E4u);
        const u32 target = e.w(self + 0x148u);
        e.vcall(target, 0x44u, {target, index}, {scaled});
    }
    const u32 found = e.call(0x088F8D58u, {e.w(self + 0x14Cu), index});
    if (found != 0u) e.set(found + 0x10u, 0u);
    const u32 anchor = e.w(car + 0xCu); // s0
    e.call(0x088DA704u, {self, index});
    e.call(0x088DA74Cu, {self, index});
    if (static_cast<s32>(e.w(car + 0x1FCu)) < -1) {
        e.vcall(anchor, 0x60u, {anchor});
        e.call(0x088DA964u, {self, index});
    }
    const u32 reference = e.w(self + 0x14u);
    const u32 pending = e.w(self + 0x618u);
    const u32 scene = e.call(0x088FE518u, {reference});
    const float rate = e.f(frame.at(0x3Cu));
    u32 mirror = 1u;
    if (e.call(0x088D62FCu, {controller}) != 1u) mirror = (e.call(0x088D62FCu, {controller}) == 2u) ? 1u : 0u;
    e.call_keep(0x088E9054u, {car, pending, scene, mirror}, {rate});
    u32 pointer; // s0 after the block below
    bool skip = false;
    if (flag == 0u) {
        const u32 tail = e.w(self + 0x14Cu);
        e.vcall(tail, 8u, {tail, index});
        const u32 gate = e.w(self + 0x5CCu);
        if (gate == 0u) skip = true;
        else e.vcall(gate, 0x14u, {gate, index});
    }
    if (!skip && e.w(self + 0x5CCu) == 0u) skip = true;
    if (!skip) {
        const u32 previous = e.call(0x088D4CB8u, {controller});
        const u32 who = e.call(0x088DA588u, {self, index});
        u32 argument = previous;
        if (e.w(frame.at(0x50u)) != 0u || who == 0u) {
            argument = 0u;
        } else if (e.vcall(who, 0x58u, {who}).v0 == 0u) {
            argument = 0u;
        }
        u32 gate = e.w(self + 0x5CCu);
        e.vcall(gate, 0x28u, {gate, index, argument});
        gate = e.w(self + 0x5CCu);
        e.vcall(gate, 0x20u, {gate, index});
    }
    pointer = e.w(self + 0x14u);
    const u32 extra = e.call(0x088DAA84u, {self});
    e.call(0x088E8C88u, {car, pointer, index, extra});
    if (e.call(0x088D5D34u, {controller}) != 0xFFFFFFFFu) {
        const GuestResult pair = e.call_keep(0x088D9C70u, {self});
        e.call(0x088E84B8u, {car, pair.a1, pair.v0, pair.v1});
    }
    const u32 link = e.w(self + 0x134u);
    if (link != 0u && flag == 0u) {
        const u32 link_table = e.w(link);
        e.call_keep(e.w(link_table + 0x10u), {link, index, e.w(frame.at(0x50u)), e.w(car + 0xCu), e.w(frame.at(0x54u))});
    }
    const u32 actor = e.call(0x088DA588u, {self, index}); // s0
    if (actor != 0u && e.call(0x088DC858u, {self, index}) == 0u && e.vcall(actor, 0x58u, {actor}).v0 != 0u) {
        const u32 manager = e.call(0x088CBFD4u);
        e.call(0x088CB3A4u, {manager});
    }
    if (e.call(0x088DA0BCu, {self}) != 0u && e.w(frame.at(0x5Cu)) == 0u) {
        const u32 holder_out = e.w(self + 0x148u);
        const u32 own_flag = (e.w(car + 0x1Cu) == 1u) ? 1u : 0u;
        const u32 out_table = e.w(holder_out);
        e.call_keep(e.w(out_table + 0x3Cu), {holder_out, index, own_flag, actor});
    }
}


// 0x08912F40 (a0 = object): teardown. After 0x08B344E0 it only runs when the byte at +0x28 is set: clears it and every counted
// link pair at +0xD4..+0x110 and in the four 0x40-byte slots at +0x800 (releasing each old link), then tail-calls 0x08B3457C.
void func_08912F40(Runtime &rt, AllegrexContext &ctx) {
    Env e{rt, ctx};
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    e.call(0x08B344E0u, {self});
    ctx.gpr[2] = e.byte(self + 0x28u);
    if (ctx.gpr[2] != 0u) {
        // Each step clears the pair (+n, +n+4) and releases the old link that sat in the second word.
        const u32 first = e.w(self + 0xD8u);
        e.set_byte(self + 0x28u, 0u);
        e.set(self + 0xD4u, 0u);
        e.set(self + 0xD8u, 0u);
        e.release_counted(first);
        for (u32 pair = 0xDCu; pair <= 0x10Cu; pair += 8u) {
            const u32 link = e.w(self + pair + 4u);
            e.set(self + pair, 0u);
            e.set(self + pair + 4u, 0u);
            e.release_counted(link);
        }
        for (u32 slot = 0u; slot != 4u; ++slot) {
            const u32 base = self + 0x800u + slot * 0x40u;
            u32 link = e.w(base + 0x34u);
            e.set(base + 0x30u, 0u);
            e.set(base + 0x34u, 0u);
            e.release_counted(link);
            for (u32 part = 0u; part != 3u; ++part) {
                const u32 at = base + part * 0x10u;
                link = e.w(at + 0xCu);
                ctx.gpr[2] = 0u;
                e.set(at + 0u, 0u);
                e.set(at + 4u, 0u);
                e.set(at + 8u, 0u);
                e.set(at + 0xCu, 0u);
                e.release_counted(link);
            }
        }
    }
    tail_call(ctx, 0x08B3457Cu, {self});
}

} // namespace
} // namespace new_batch_main_132
} // namespace gt::game

#define NEW132(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_132::name, gt::ReturnKind::kind)
NEW132(0x08808F2Cu, func_08808F2C, Word);
NEW132(0x088DD144u, func_088DD144, Word);
NEW132(0x08912F40u, func_08912F40, Word);
