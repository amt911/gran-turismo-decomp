// New batch main 1590: 21 functions written by hand from `tools.disasm` as structured C (draft, not gated).
// Redrafted from the old drafts' conventions (call_set / call_keep / call_virtual / singleton_once / tail_call) and the asm.
// Skipped on purpose (not drafted): 0x088A61A4 (stores f0), 0x089343A8 (goto-style exits), 0x08ADD114 (64-bit result and
// FPU), 0x08AE1794 (FPU, the old draft faulted), 0x088B1254 (ll/sc loop), 0x088E060C (lwc1/swc1 copy).
//
// Conventions: a jal's delay slot runs before the call; a bnel/beql delay runs only on the taken path; `lui` leftovers are
// `lui_of(address)`; the guard/storage/vtable constants are the absolute addresses the disassembler shows.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1590 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline constexpr u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result;
}
// A guest call with a0.. set the way the original sets them before the jal; returns the callee's v0.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
inline GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                                std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_keep(rt, ctx, ctx.gpr[2], arguments);
}

// A function-local static: a guard byte that is non-zero once built; the object at `storage` gets `vtable` when this call built
// it. The guard's acquire is func_08B6441C and its release func_08B64494.
constexpr u32 kGuardAcquire = 0x08B6441Cu;
constexpr u32 kGuardRelease = 0x08B64494u;
inline void singleton_once(Runtime &rt, AllegrexContext &ctx, u32 guard, u32 storage, u32 vtable) {
    ctx.gpr[2] = lui_of(storage);
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, kGuardAcquire, {guard}) != 0u) {
        put(rt, storage, vtable);
        (void)call_set(rt, ctx, kGuardRelease, {guard});
    }
}

// Two 0x30-byte elements destroyed by func_088978B4: at `address`, then 0x30 below it.
inline void destroy_two(Runtime &rt, AllegrexContext &ctx, u32 address) {
    (void)call_set(rt, ctx, 0x088978B4u, {address});
    (void)call_set(rt, ctx, 0x088978B4u, {address - 0x30u});
}

// The copy-on-write string release that ends several functions: `field` is the data pointer, its rep is 0xC below it and
// the count is at -4. Unless the rep is the empty rep 0x08D74FD0, func_08B60804(field - 4, -1) decides, and a non-positive
// answer calls func_08B8542C(rep, slot). Returns v0 as the original leaves it.
inline u32 string_tail(Runtime &rt, AllegrexContext &ctx, u32 field, u32 slot) {
    const u32 rep = field - 0xCu;
    if (rep == 0x08D74FD0u) return 0x08D74FD0u;
    const u32 answer = call_set(rt, ctx, 0x08B60804u, {field - 4u, 0xFFFFFFFFu});
    if (signed_of(answer) > 0) return answer;
    return call_set(rt, ctx, 0x08B8542Cu, {rep, slot});
}

// The find-or-insert of an ordered tree (func_08BFE9FC and its three siblings): the header is `self` with the root at +8 and
// the end sentinel at +0xC; each node has its key at +0x10 and children at +8 (left) and +0xC (right). `insert` is the
// insertion routine that takes (self, 0, parent, key pointer). v0 is the node found or the insertion's result.
inline void tree_find_or_insert(Runtime &rt, AllegrexContext &ctx, u32 insert) {
    const u32 self = ctx.gpr[4];
    const u32 keyp = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 parent = self + 4u;
    u32 found = keyp;
    bool compare = false;
    const u32 root = word(rt, self + 8u);
    if (root != 0u) {
        const u32 key = word(rt, keyp);
        u32 cur = root;
        u32 flag = 0u;
        for (;;) {
            flag = key < word(rt, cur + 0x10u) ? 1u : 0u;
            const u32 child = flag != 0u ? word(rt, cur + 8u) : word(rt, cur + 0xCu);
            if (child == 0u) {
                parent = cur;
                break;
            }
            cur = child;
        }
        found = cur;
        compare = flag == 0u;
    }
    if (!compare) {
        if (parent == word(rt, self + 0xCu)) {
            (void)call_set(rt, ctx, insert, {self, 0u, parent, keyp});
            return;
        }
        found = call_set(rt, ctx, 0x08B5F174u, {parent});
    }
    if (word(rt, found + 0x10u) < word(rt, keyp)) {
        found = call_set(rt, ctx, insert, {self, 0u, parent, keyp});
    }
    ctx.gpr[2] = found;
}

// 0x0885D2C8 (a0 = self): destroys the elements at +0x158/+0x128, +0xE8/+0xB8 and +0x88/+0x58 under the conditions of the
// original, and tail-calls func_088A7548 on the +0x50 member.
void func_0885D2C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 v0 = self + 0x128u;
    {
        GuestFrame frame(ctx, 0x20u);
        if (v0 != 0u && v0 != self + 0x188u) destroy_two(rt, ctx, self + 0x158u);
        v0 = call_set(rt, ctx, 0x088A7548u, {self + 0x120u});
        const u32 s3 = self + 0xB8u;
        if (s3 != 0u) {
            v0 = self + 0x58u;
            if (s3 != self + 0x118u) destroy_two(rt, ctx, self + 0xE8u);
        }
        if (v0 != 0u && v0 != s3) destroy_two(rt, ctx, self + 0x88u);
    }
    tail_call(ctx, 0x088A7548u, {self + 0x50u});
}

// 0x088662A8 (a0 = object, a1, a2 = arguments): builds a value object at sp through func_088A79AC / func_088A6AB4, runs
// func_088975D0 twice on it under the lock 0x08CE2DCC, destroys the temporaries and returns func_088A7548's v0.
void func_088662A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    const u32 a2in = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0u);
    const u32 s2 = frame.at(8u);
    (void)call_set(rt, ctx, 0x08B36018u, {self, 0u, a1in});
    (void)call_set(rt, ctx, 0x08B344E0u, {0x08CE2DCCu});
    const u32 a2v = call_keep(rt, ctx, 0x088666E8u, {}).v0;
    const u32 obj = call_set(rt, ctx, 0x08866830u, {a2in, self, a2v});
    (void)call_set(rt, ctx, 0x088A79ACu, {sp, 0u});
    (void)call_set(rt, ctx, 0x088A6AB4u, {sp, obj});
    u32 s1 = ctx.gpr[2];
    (void)call_set(rt, ctx, 0x08897B8Cu, {s2});
    (void)call_set(rt, ctx, 0x08897574u, {s2, 0xFFFFFFFFu});
    for (int n = 2; n > 0; --n) s1 = call_set(rt, ctx, 0x088975D0u, {s2, s1});
    (void)call_set(rt, ctx, 0x08B3457Cu, {0x08CE2DCCu});
    (void)call_set(rt, ctx, 0x088978B4u, {s2});
    ctx.gpr[2] = call_set(rt, ctx, 0x088A7548u, {sp});
}

// 0x0889436C (a0 = out object, a1 = pointer to the source pointer): allocates a 0x24-byte node into *a0, builds the
// function-local singleton, then copies the source's first 0x24 bytes into the node; v0 is the last word copied.
void func_0889436C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 node = call_set(rt, ctx, 0x08B62344u, {0x24u});
    put(rt, node + 0x1Cu, 0u);
    put(rt, self, node);
    singleton_once(rt, ctx, 0x08D75EC8u, 0x08D75ED0u, 0x08CC1DD8u);
    put(rt, self + 4u, 0x08D75ED0u);
    const u32 src = word(rt, a1in);
    for (u32 i = 0u; i < 0x20u; i += 4u) put(rt, node + i, word(rt, src + i));
    const u32 last = word(rt, src + 0x20u);
    put(rt, node + 0x20u, last);
    ctx.gpr[2] = last;
}

// 0x088A78D4 (a0 = out, a1 = pointer to a pointer, a2 = flag byte): builds the object by one of two allocators, stores it
// and the singleton at *out, then tail-calls func_088A5450 with (*out, *a1).
void func_088A78D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    ctx.gpr[6] &= 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    u32 obj;
    if (ctx.gpr[6] != 0u) {
        obj = call_set(rt, ctx, 0x08B2B3ECu, {0x4D8u});
        (void)call_set(rt, ctx, 0x088A73E4u, {obj});
        put(rt, obj, 0x08CC2BE8u);
    } else {
        obj = call_set(rt, ctx, 0x08B62344u, {0x4D8u});
        (void)call_set(rt, ctx, 0x088A7310u, {obj});
    }
    put(rt, out, obj);
    singleton_once(rt, ctx, 0x08D75F28u, 0x08D75F30u, 0x08CC2C00u);
    put(rt, out + 4u, 0x08D75F30u);
    tail_call(ctx, 0x088A5450u, {word(rt, out), word(rt, a1in)});
}

// 0x089C151C (a0 = sp-based return area, a1, a2 = count, a3 = source): returns at once when a2 < 2 (signed; a2 becomes 0 or 1
// as the original's slti leaves it); otherwise runs the four-stage build with its asserts.
void func_089C151C(Runtime &rt, AllegrexContext &ctx) {
    const bool skip = signed_of(ctx.gpr[6]) < 2;
    ctx.gpr[6] = skip ? 1u : 0u;
    if (skip) return;
    const u32 a3in = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    const u32 p4 = frame.at(4u);
    const u32 p8 = frame.at(8u);
    (void)call_set(rt, ctx, 0x089BEE4Cu, {sp});
    (void)call_set(rt, ctx, 0x089AB370u, {p4, a3in});
    (void)call_set(rt, ctx, 0x08A6C410u, {p8, a3in + 4u});
    const u32 s2 = word(rt, sp);
    if (s2 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    const u32 s0 = word(rt, p8);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA60DCu});
    const u32 s1 = word(rt, p4);
    const u32 r = call_set(rt, ctx, 0x08A6BFFCu, {s0});
    (void)call_set(rt, ctx, 0x089C1440u, {s2, s1, r});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {sp});
}

// 0x08A58EF0 (a0 = self, a1 = object with the value at +0x1C/+0x18, a2 = pointer to the result slot): finds the node, calls
// the virtual at +8 of the self object's member when the node is empty, and the virtual at +0x10 of the member with the frame
// slot; v0 is 0 when the result slot is empty, else func_08A5B9EC's.
void func_08A58EF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    const u32 s2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = frame.at(0u);
    u32 s0 = word(rt, a1in + 0x1Cu);
    if (s0 == 0u) {
        const u32 t = word(rt, a1in + 0x18u);
        if (t == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF864u});
        s0 = word(rt, t + 0xCu);
    }
    if (word(rt, s2) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 value;
    if (word(rt, s0) != 0u) {
        value = word(rt, s0);
    } else {
        const u32 obj = word(rt, self + 0x38u);
        value = call_virtual(rt, ctx, obj, 8u, {obj}).v0;
        put(rt, s0, value);
    }
    const u32 a1obj = word(rt, self + 0x38u);
    (void)call_virtual(rt, ctx, a1obj, 0x10u, {sp, a1obj, value});
    u32 s0b = word(rt, sp);
    if (s0b == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF878u});
    (void)call_virtual(rt, ctx, s0b, 0xF4u, {s0b, s2});
    ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {sp});
}

// 0x08A8C4F8 (a0 = self, a1 = delta): adds the delta to the counter of the self's member at +0x18, and when the first
// node's key is below it, stores the counter in the node; sets a byte flag when the delta is positive. v0 ends as 1.
void func_08A8C4F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 delta = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 obj = word(rt, self + 0x50u);
    put(rt, obj + 0x18u, word(rt, obj + 0x18u) + delta);
    const u32 s2 = word(rt, obj + 0x18u);
    u32 s0 = word(rt, self + 0x30u);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
    s0 = word(rt, s0 + 0x30u);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC8u});
    if (signed_of(word(rt, s0 + 0x10u)) < signed_of(s2)) {
        u32 t = word(rt, self + 0x30u);
        if (t == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
        t = word(rt, t + 0x30u);
        if (t == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC8u});
        put(rt, t + 0x10u, s2);
    }
    if (signed_of(delta) > 0) put8(rt, word(rt, self + 0x50u) + 0x20u, 1u);
    ctx.gpr[2] = 1u;
}

// 0x08A6BCB0 (a0 = pointer to an object pointer, a1, a2 = arguments): calls the object's virtual at +0xA0 through the
// frame, then the string release; v0 is the release's.
void func_08A6BCB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 slot = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    const u32 a2in = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    const u32 p4 = frame.at(4u);
    const u32 p8 = frame.at(8u);
    const u32 obj = word(rt, slot);
    if (obj == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB05D8u});
    const u32 fn = word(rt, word(rt, obj) + 0xA0u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {p4, a1in, frame.at(1u)});
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
    (void)call_keep(rt, ctx, fn, {obj, p8, a2in});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    ctx.gpr[2] = string_tail(rt, ctx, word(rt, p4), sp);
}

// 0x08A6BD88 (a0 = out value, a1 = pointer to an object pointer, a2 = argument): the same shape as func_08A6BCB0 through the
// virtual at +0x9C, returning the out value a0.
void func_08A6BD88(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    const u32 a2in = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    const u32 p4 = frame.at(4u);
    const u32 p8 = frame.at(8u);
    const u32 obj = word(rt, slot);
    if (obj == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB05D8u});
    const u32 fn = word(rt, word(rt, obj) + 0x9Cu);
    (void)call_set(rt, ctx, 0x08B85EB8u, {p4, a2in, frame.at(1u)});
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
    (void)call_keep(rt, ctx, fn, {out, obj, p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)string_tail(rt, ctx, word(rt, p4), sp);
    ctx.gpr[2] = out;
}

// 0x08A783B4 (a0 = self, a1 = source): clones the source's list head and its chain into a new 0x240-byte node stored at
// self+0x10; v0 is the source's +0xC when the head is empty, the node when the chain has no next, else the head's +0x14.
void func_08A783B4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08A5B860u, {self});
    put(rt, self + 0x10u, 0u);
    put(rt, self, 0x08CD3C58u);
    const u32 srcC = word(rt, src + 0xCu);
    put(rt, self + 0xCu, srcC);
    const u32 head = word(rt, src + 0x10u);
    u32 v0 = srcC;
    if (head != 0u) {
        const u32 s0 = call_set(rt, ctx, 0x08A5BC20u, {0x240u, 4u});
        put(rt, s0 + 8u, 0u);
        put(rt, s0 + 0x14u, 0u);
        put(rt, s0 + 4u, 0u);
        put(rt, s0 + 0xCu, s0 + 4u);
        put(rt, s0 + 0x10u, s0 + 4u);
        v0 = s0;
        if (word(rt, head + 8u) != 0u) {
            const u32 ret = call_set(rt, ctx, 0x08C2605Cu, {s0});
            put(rt, s0 + 8u, ret);
            u32 v = ret;
            while (word(rt, v + 8u) != 0u) v = word(rt, v + 8u);
            put(rt, s0 + 0xCu, v);
            u32 w = word(rt, s0 + 8u);
            while (word(rt, w + 0xCu) != 0u) w = word(rt, w + 0xCu);
            put(rt, s0 + 0x10u, w);
            put(rt, s0 + 0x14u, word(rt, head + 0x14u));
            v0 = word(rt, head + 0x14u);
        }
        put(rt, self + 0x10u, s0);
    }
    ctx.gpr[2] = v0;
}

// 0x08A8712C (a0 = out, a1, a2, a3 = arguments): builds a three-word object, runs the chain of calls on it and returns a0.
void func_08A8712C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    const u32 a2in = ctx.gpr[6];
    const u32 a3in = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    const u32 p4 = frame.at(4u);
    const u32 p8v = frame.at(8u);
    (void)p8v;
    put(rt, sp, a1in);
    (void)call_set(rt, ctx, 0x08A76AACu, {p4, sp});
    const u32 obj = call_set(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, obj, 0x08CD1F30u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {obj + 4u});
    put(rt, obj, 0x08CD4168u);
    (void)call_set(rt, ctx, 0x08A62AB8u, {obj + 8u});
    const u32 s1 = word(rt, obj + 8u);
    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB18C4u});
    (void)call_set(rt, ctx, 0x08A641F4u, {s1, p4, a2in, a3in});
    (void)call_set(rt, ctx, 0x08A851CCu, {self, obj});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    ctx.gpr[2] = self;
}

// 0x08A93578 (a0 = self, a1 = source): when the source's member at +0x14 has an entry, builds the entry's value through
// func_08A7BDB0 / func_08B35F60 into self; the final call is func_08B8A4C0 on (self, 0x08CB2080, 9) when the self's slot is empty.
void func_08A93578(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 s0 = word(rt, word(rt, src + 4u) + 0x30u);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB204Cu});
    (void)call_set(rt, ctx, 0x08B88F44u, {self, s0 + 0x2Cu});
    if (word(rt, word(rt, self) - 0xCu) == 0u) {
        const u32 p = word(rt, src + 4u);
        const u32 node = word(rt, p + 0x14u);
        if (node != 0u && word(rt, node + 4u) != 0u) {
            const u32 s0n = call_set(rt, ctx, 0x08A7BDB0u, {node});
            const u32 v2 = call_set(rt, ctx, 0x08B35F60u, {s0n});
            (void)call_set(rt, ctx, 0x08B8A4C0u, {self, s0n, v2});
        }
    }
    if (word(rt, word(rt, self) - 0xCu) == 0u) {
        (void)call_set(rt, ctx, 0x08B8A4C0u, {self, 0x08CB2080u, 9u});
    }
    ctx.gpr[2] = self;
}

// 0x08AA6664 (a0 = self, a1 = argument): looks the key up through func_08C0DA48; on the header's branch it inserts through
// func_08C0DEF8 and releases the string, otherwise it reads the found node's +0x14; then func_08AA5E90 runs. Returns self.
void func_08AA6664(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1in = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0u);
    const u32 s1 = self + 4u;
    const u32 s3 = frame.at(0xCu);
    const u32 v = call_set(rt, ctx, 0x08C0DA48u, {s1});
    u32 s0;
    if (self + 8u == v) {
        s0 = word(rt, s1 + 0x14u);
        (void)call_set(rt, ctx, 0x08B88F44u, {s3, a1in});
        put(rt, sp + 0x10u, s0);
        const u32 v0c = call_set(rt, ctx, 0x08C0DEF8u, {s1, s3});
        const u32 v1 = ctx.gpr[3];
        const u32 a2v = word(rt, sp + 0xCu);
        put(rt, sp + 0x24u, v1);
        put(rt, sp + 4u, v0c);
        put(rt, sp + 0x20u, v0c);
        put8(rt, sp + 8u, v1 & 0xFFu);
        (void)string_tail(rt, ctx, a2v, sp);
    } else {
        s0 = word(rt, v + 0x14u);
    }
    (void)call_set(rt, ctx, 0x08AA5E90u, {self, s0});
    ctx.gpr[2] = self;
}

// 0x08B29858 (a0 = self, a1 = out, a2 = in, a3 = count): xors the input with the frame's key in 64-byte blocks, each block
// after the two calls on self; the tail (under 0x41 bytes) is done the same way. v0 is 1 for an empty tail, else the last byte.
void func_08B29858(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 s0 = ctx.gpr[5];
    u32 s1 = ctx.gpr[6];
    u32 s2 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0u);
    for (;;) {
        (void)call_set(rt, ctx, 0x08B2936Cu, {self, sp});
        (void)call_set(rt, ctx, 0x08B29348u, {self});
        if (s2 < 0x41u) break;
        for (u32 i = 0u; i < 0x40u; ++i) put8(rt, s0 + i, byte_u(rt, s1 + i) ^ byte_u(rt, sp + i));
        s1 += 0x40u;
        s0 += 0x40u;
        s2 -= 0x40u;
    }
    u32 last = 1u;
    for (u32 i = 0u; i < s2; ++i) {
        last = byte_u(rt, s1 + i) ^ byte_u(rt, sp + i);
        put8(rt, s0 + i, last);
    }
    ctx.gpr[2] = last;
}

// 0x08BFE9FC, 0x08BFEAD4, 0x08C1D458, 0x08C35350: the tree find-or-insert of their own insertion routine.
void func_08BFE9FC(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08BFE92Cu); }
void func_08BFEAD4(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08BFE808u); }
void func_08C1D458(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08C1D2DCu); }
void func_08C35350(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08C35280u); }

// 0x0884D6A0 (a0 = ?, a2 = count, a3 = pointer to two object pointers): returns at once when a2 < 2 (signed, a2 as the slti
// leaves it); otherwise two virtual calls on the object pairs, then func_09322C on them.
void func_0884D6A0(Runtime &rt, AllegrexContext &ctx) {
    const bool skip = signed_of(ctx.gpr[6]) < 2;
    ctx.gpr[6] = skip ? 1u : 0u;
    if (skip) return;
    const u32 a3in = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    (void)call_set(rt, ctx, 0x0884D660u, {sp});
    const u32 s0 = word(rt, sp);
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D198u});
    const u32 s3 = call_set(rt, ctx, 0x0884D3D8u, {s0});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {sp});
    const u32 s0b = word(rt, a3in);
    if (s0b == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
    const u32 s1 = call_virtual(rt, ctx, s0b, 0x3Cu, {s0b}).v0;
    const u32 s0c = word(rt, a3in + 4u);
    if (s0c == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
    const u32 s2v = call_virtual(rt, ctx, s0c, 0x3Cu, {s0c}).v0;
    ctx.gpr[2] = call_set(rt, ctx, 0x0889722Cu, {s3, s1, s2v});
}

// 0x088F5594 (a0 = out): builds the 0x828-byte object (its vtable and self-referencing links), publishes it at *a0, builds
// the function-local static and tail-calls func_0F174C on the object's member.
void func_088F5594(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 obj = call_set(rt, ctx, 0x08B62344u, {0x828u});
    put(rt, obj + 0x404u, 0u);
    put(rt, obj + 0x80Cu, 0u);
    put(rt, obj + 0x810u, 0x08CC3DF0u);
    put(rt, obj + 0x408u, 0x08CC3BA8u);
    put(rt, obj, 0x08CC3BA8u);
    put(rt, obj + 0x81Cu, 0x12345678u);
    put(rt, obj + 0x814u, obj + 0x408u);
    put(rt, obj + 0x820u, 0u);
    put(rt, obj + 0x818u, obj);
    put8(rt, obj + 0x824u, 1u);
    put(rt, self, obj);
    const u32 s0 = 0x08D76128u;
    u32 a1 = 0x08CC3BA8u;
    if (sext8(byte_u(rt, s0)) == 0u) {
        const u32 acquired = call_set(rt, ctx, kGuardAcquire, {s0});
        a1 = lui_of(0x08D76130u);
        if (acquired != 0u) {
            put(rt, 0x08D76130u, 0x08CC3E20u);
            (void)call_set(rt, ctx, kGuardRelease, {s0});
        }
    }
    put(rt, self + 4u, 0x08D76130u);
    ctx.gpr[5] = a1;
    ctx.gpr[6] = obj + 0x810u;
    ctx.gpr[7] = obj + 0x408u;
    tail_call(ctx, 0x088F574Cu, {s0});
}

// 0x089CC59C (a0 = self): stores the vtable at +0 and releases the three members of the list head at +0xC, +0x10, +0x14.
void func_089CC59C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = frame.at(0u);
    put(rt, self, 0x08CCF240u);
    (void)string_tail(rt, ctx, word(rt, self + 0x14u), sp + 2u);
    (void)string_tail(rt, ctx, word(rt, self + 0x10u), sp + 1u);
    (void)string_tail(rt, ctx, word(rt, self + 0xCu), sp);
    (void)call_set(rt, ctx, 0x08A5BB5Cu, {self});
}

} // namespace
} // namespace new_batch_main_1590
} // namespace gt::game

#define NEW1590(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1590::name, gt::ReturnKind::kind)
NEW1590(0x0885D2C8u, func_0885D2C8, Word);
NEW1590(0x088662A8u, func_088662A8, Word);
NEW1590(0x0889436Cu, func_0889436C, Word);
NEW1590(0x088A78D4u, func_088A78D4, Word);
NEW1590(0x089C151Cu, func_089C151C, Word);
NEW1590(0x08A58EF0u, func_08A58EF0, Word);
NEW1590(0x08A8C4F8u, func_08A8C4F8, Word);
NEW1590(0x08A6BCB0u, func_08A6BCB0, Word);
NEW1590(0x08A6BD88u, func_08A6BD88, Word);
NEW1590(0x08A783B4u, func_08A783B4, Word);
NEW1590(0x08A8712Cu, func_08A8712C, Word);
NEW1590(0x08A93578u, func_08A93578, Word);
NEW1590(0x08AA6664u, func_08AA6664, Word);
NEW1590(0x08B29858u, func_08B29858, Word);
NEW1590(0x08BFE9FCu, func_08BFE9FC, Word);
NEW1590(0x08BFEAD4u, func_08BFEAD4, Word);
NEW1590(0x08C1D458u, func_08C1D458, Word);
NEW1590(0x08C35350u, func_08C35350, Word);
NEW1590(0x0884D6A0u, func_0884D6A0, Word);
NEW1590(0x088F5594u, func_088F5594, Word);
NEW1590(0x089CC59Cu, func_089CC59C, Word);
