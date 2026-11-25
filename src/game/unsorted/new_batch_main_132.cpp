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
NEW132(0x08912F40u, func_08912F40, Word);
