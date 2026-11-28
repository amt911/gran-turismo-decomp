// New batch main 1585: 19 functions drafted from `tools.disasm` as structured C (draft; the gate has not run).
// Re-drafts of the old batch-13 candidates (0x08ABC388 from commit a5d88c23) and new drafts of the rest.
// Skipped on purpose (unsure of the argument registers or the instruction set): 0x08890A1C (wsbw byte swaps),
// the six 0x60-byte wrappers that pass $t0/$t1 (0x08B0F3A4, 0x08B0FAEC, 0x08B0FCA4, 0x08B0FD48, 0x08B0FF00,
// 0x08B0FFA4), 0x08AE16EC (float + alloca), 0x08AFA2BC ($t0 argument), 0x0886C3A4 (a2 set for callees),
// 0x08987C7C (a1 after the first call).
//
// Every call passes the argument registers the original sets before the `jal`; a delay slot runs before its
// jump/call; v0 is the asm's on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1585 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `lb`: the byte at an address, sign-extended.
inline u32 sign_byte(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

// A call that leaves v0 / v1 / f0 as the callee returned them (as the `jal` does in the original).
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A `jal` with the argument registers the original sets (a0-a3, then t0-t3) written into ctx first.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// `lw v1, slot(vtable); jalr v1`: the target is not in v0, so v0 keeps what the code set before the call.
inline GuestResult call_indirect_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments, {});
}

// 0x08A328D4 (a0 = self): stores its vtable, deletes the owned object at +0x42C through the vtable's slot 4, then
// clears the vector at +0x420 (the storage at +0x420 is released through 0x08B876E0 and 0x08A5BC54) and finishes
// with 0x08A5B9EC on +0x14 and 0x08A780B0 on self. v0 = the last call's answer.
void func_08A328D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CB8AC8u);
    const u32 owned = word(rt, self + 0x42Cu);
    if (owned != 0u) {
        const u32 vtable = word(rt, owned);
        ctx.gpr[2] = vtable;
        call_indirect_keep(rt, ctx, word(rt, vtable + 4u), {owned});
    }
    put(rt, self + 0x42Cu, 0u);
    call_set(rt, ctx, 0x08B876E0u, {word(rt, self + 0x420u), word(rt, self + 0x424u), frame.at(0u)});
    const u32 begin = word(rt, self + 0x420u);
    const u32 end = word(rt, self + 0x428u);
    const u32 size = static_cast<u32>((static_cast<s32>(end - begin) >> 2) * 4);
    if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, size, 4u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x14u});
    call_set(rt, ctx, 0x08A780B0u, {self});
}

// 0x08A72C14 (a0 = self): two setup calls, then for each of the `count` (+0xC, signed) entries of the array at
// +0 call 0x08A76AA0 on a temp, release the entry through 0x08A5BAD8 when it is not the temp's own end, and 0x08A5B9EC.
// v0 = 0 after a loop, else the count (a count <= 0 is returned as is).
void func_08A72C14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, 0x08A728FCu, {self, 0u});
    call_set(rt, ctx, 0x08A729F8u, {self, 0u});
    u32 count = word(rt, self + 0xCu);
    if (static_cast<s32>(count) > 0) {
        u32 index = 0u;
        do {
            const u32 slot = word(rt, self) + (index << 2);
            index += 1u;
            call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
            if (slot != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
            count = word(rt, self + 0xCu);
        } while (static_cast<s32>(index) < static_cast<s32>(count));
        ctx.gpr[2] = 0u;
    } else {
        ctx.gpr[2] = count;
    }
}

// 0x08A7E450 (a0 = self, a1 = other): when the two differ, self's sub-object at +0x18 is compared with 0x08A6A23C,
// the link at +0x1C is replaced by other's (releasing the old ones through 0x08A5BA08 / 0x08A5B930) and +0x20 is
// copied through 0x08C280A0. v0 = self.
void func_08A7E450(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (self != other) {
        call_set(rt, ctx, 0x08A6A23Cu, {self + 0x18u, other + 0x18u});
        const u32 other_link = word(rt, other + 0x1Cu);
        if (other_link != 0u) call_set(rt, ctx, 0x08A5BA08u, {other_link});
        const u32 self_link = word(rt, self + 0x1Cu);
        if (self_link != 0u) call_set(rt, ctx, 0x08A5B930u, {self_link});
        put(rt, self + 0x1Cu, word(rt, other + 0x1Cu));
        call_set(rt, ctx, 0x08C280A0u, {self + 0x20u, other + 0x20u});
    }
    ctx.gpr[2] = self;
}

// 0x08B198A8: a one-time static initialiser. When the guard byte at 0x08D68928 is clear and 0x08B6441C answers
// non-zero, the block at 0x08D68930 gets its vtable (+0 and +8) and cleared fields, 0x08B64494 runs on the guard and
// 0x08B77B1C registers 0x08B19894. v0 = the block.
void func_08B198A8(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kGuard = 0x08D68928u;
    constexpr u32 kBlock = 0x08D68930u;
    GuestFrame frame(ctx, 0x10u);
    if (sign_byte(byte_at(rt, kGuard)) == 0u) {
        if (call_keep(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            put(rt, kBlock + 0x8u, 0x08CB528Cu);
            put(rt, kBlock + 0x30u, 0u);
            put(rt, kBlock + 0x4u, 0u);
            put(rt, kBlock + 0xCu, 0u);
            put(rt, kBlock + 0x10u, 0u);
            put(rt, kBlock + 0x14u, 0u);
            put(rt, kBlock + 0x1Cu, 0u);
            put(rt, kBlock + 0x20u, 0u);
            put(rt, kBlock + 0x24u, 0u);
            put(rt, kBlock + 0x2Cu, 0u);
            put(rt, kBlock, 0x08CD92E0u);
            call_keep(rt, ctx, 0x08B64494u, {kGuard});
            call_set(rt, ctx, 0x08B77B1Cu, {0x08B19894u});
        }
    }
    ctx.gpr[2] = kBlock;
}

// 0x08B1D430 (a0 = self, a2 = mid, a3 = pass): 0x08B344E0 on self+4; 0x08B1D080 finds the index; when the index is
// not negative the element at index is removed from the array at +0x2C through 0x08B79BAC (unless it is the last
// one) and the count at +0x34 drops by one. Tail call 0x08B3457C(self+4).
void func_08B1D430(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 member = self + 4u;
    const u32 mid = ctx.gpr[6];
    const u32 pass = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08B344E0u, {member});
    const u32 index = call_set(rt, ctx, 0x08B1D080u, {self, ctx.gpr[5], mid, pass});
    if (static_cast<s32>(index) >= 0) {
        const u32 last = word(rt, self + 0x34u) - 1u;
        if (index != last) {
            const u32 row = word(rt, self + 0x2Cu) + (index << 4);
            call_set(rt, ctx, 0x08B79BACu, {row, row + 0x10u, (last << 4) - 0x10u});
        }
        put(rt, self + 0x34u, word(rt, self + 0x34u) - 1u);
    }
    tail_call(ctx, 0x08B3457Cu, {member});
}

// 0x0885D7C4 (a0 = self): clears self+0x4C, runs 0x088A79AC on +0x50 and 0x08897B8C on two 0x30-byte slots from +0x58
// and from +0xB8, 0x088A79AC on +0x120, 0x08897B8C on two slots from +0x128, then tail-calls 0x0885D5E0(self).
void func_0885D7C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 0x4Cu, 0u);
    call_set(rt, ctx, 0x088A79ACu, {self + 0x50u, 0u});
    for (u32 i = 0; i < 2u; ++i) call_set(rt, ctx, 0x08897B8Cu, {self + 0x58u + 0x30u * i});
    for (u32 i = 0; i < 2u; ++i) call_set(rt, ctx, 0x08897B8Cu, {self + 0xB8u + 0x30u * i});
    call_set(rt, ctx, 0x088A79ACu, {self + 0x120u, 0u});
    for (u32 i = 0; i < 2u; ++i) call_set(rt, ctx, 0x08897B8Cu, {self + 0x128u + 0x30u * i});
    tail_call(ctx, 0x0885D5E0u, {self});
}

// 0x08893ED8 (a0 = self): a 12-byte block from 0x08B62344 is stored at self+0; self+4 gets the object at 0x08D75EC0,
// which is created (vtable 0x08CC1DA8, 0x08B6441C / 0x08B64494 guard) when the guard byte at 0x08D75EB8 is clear.
// Tail call 0x08893BAC(self, 0x40).
void func_08893ED8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 block = call_keep(rt, ctx, 0x08B62344u, {0x0Cu});
    put(rt, block, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, self, block);
    const u32 guard = 0x08D75EB8u;
    const u32 object = 0x08D75EC0u;
    if (sign_byte(byte_at(rt, guard)) == 0u) {
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            put(rt, object, 0x08CC1DA8u);
            call_keep(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, self + 4u, object);
    tail_call(ctx, 0x08893BACu, {self, 0x40u});
}

// 0x089AB6D8 (a0 = self): 0x089AA658 writes a handle to a temp; the handle goes through 0x089A717C, then (when that
// is non-zero) through 0x089A7120. A non-zero second answer runs 0x089A6A68(self), otherwise 0x089A6A7C(self, temp).
// Both paths release the temp through 0x08A5B9EC. v0 = self.

// 0x08A1C8A8 (a0 = self): vtable 0x08CD0B38, the base is 0x08A76ACC(self), self+0x14 = 0x08B62344(1); self+0x18 is the
// static slot at 0x08D77120 (set to 0x08CD0D88 and released by 0x08B64494 when the guard at 0x08D77118 is clear and
// 0x08B6441C answers non-zero). v0 = the last call's answer.

// 0x08A38FD4 (a0 = self): the vtable 0x08CCB088 and the defaults of a 0x80-byte object: +0x18 = 1.0f, +0x40 = -1.0f,
// the words of +0x50..+0xCC cleared. v0 = self + 0x80 (the loop's last pointer); f0 = -1.0f.
void func_08A38FD4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08A76ACCu, {self});
    put_float(rt, self + 0x18u, 1.0f);
    put(rt, self, 0x08CCB088u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    put_float(rt, self + 0x40u, -1.0f);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0xD0u, 0u);
    put(rt, self + 0xD4u, 0u);
    for (u32 i = 0; i < 0x20u; ++i) put(rt, self + 0x50u + 4u * i, 0u);
    ctx.fpr[0] = -1.0f;
    ctx.gpr[2] = self + 0x80u;
}

// 0x08A9E358 (a0 = self, a1 = byte): appends the byte to the string object at self+0x20 (its data pointer at +0,
// size at -0xC, capacity at -8): grown through 0x08B864EC to size+1 when the capacity is short or the word at -4 is
// positive; the byte goes at the old end and a zero-like byte from 0x08CB7430 at the new end. v0 = self.
void func_08A9E358(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 byte = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 string = word(rt, self + 0x20u);
    u32 data = word(rt, string);
    u32 size = word(rt, data - 0xCu);
    const u32 need = size + 1u;
    const u32 capacity = word(rt, data - 8u);
    if (capacity < need || static_cast<s32>(word(rt, data - 4u)) > 0) {
        call_set(rt, ctx, 0x08B864ECu, {string, need});
        data = word(rt, string);
        size = word(rt, data - 0xCu);
    }
    put8(rt, data + size, byte);
    put(rt, data - 4u, 0u);
    put(rt, data - 0xCu, need);
    put8(rt, data + need, byte_at(rt, 0x08CB7430u));
    ctx.gpr[2] = self;
}

// 0x08B0FC00 (a0 = out, a2 = source, a3 = flag): a 16-byte temp {0, 0, src[0], src[1]} is built on the stack, 0x08B0F570
// fills a 0x3C-byte buffer through (buffer, a1, temp, flag), and the buffer is copied to out. v0 = out.

// 0x08B0FE5C: the same wrapper as 0x08B0FC00 with the temp laid out as {src[0], src[1], 0, 0}.

// 0x088D2904 (a0 = self): clears self+8, runs 0x088D1D90 on the 16 twelve-byte slots from +0xC, on the 16 from +0x14C,
// on +0x20C, +0x220, +0x22C, on the 16 from +0x238 and on +0x2F8, then tail-calls 0x088D1F30(self).
void func_088D2904(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 8u, 0u);
    for (u32 i = 0; i < 16u; ++i) call_set(rt, ctx, 0x088D1D90u, {self + 0xCu + 0xCu * i});
    for (u32 i = 0; i < 16u; ++i) call_set(rt, ctx, 0x088D1D90u, {self + 0x14Cu + 0xCu * i});
    call_set(rt, ctx, 0x088D1D90u, {self + 0x20Cu});
    call_set(rt, ctx, 0x088D1D90u, {self + 0x220u});
    call_set(rt, ctx, 0x088D1D90u, {self + 0x22Cu});
    for (u32 i = 0; i < 16u; ++i) call_set(rt, ctx, 0x088D1D90u, {self + 0x238u + 0xCu * i});
    call_set(rt, ctx, 0x088D1D90u, {self + 0x2F8u});
    tail_call(ctx, 0x088D1F30u, {self});
}

// 0x0888A554 (a0 = self): a 32-byte block from 0x08B62344, its fields -1 at +0x18 and +0x1C and zero elsewhere, is
// stored at self+0. The guard byte at 0x08D75E78 (clear) and 0x08B6441C answer decide the object at 0x08D75E84
// (vtable 0x08CC1B48, released through 0x08B64494); self+4 = 0x08D75E84. v0 as the last call left it.

// The release of the object held at holder+0 (and its next at holder+4), the same in 0x08887520 and 0x088886F0:
// the object's virtual at +8 runs first; the strong count at +4 drops by one under ll/sc, and at zero the virtual at
// vtable+8 runs, then the weak count at +8 drops, and at zero the vtable's slot +0xC is tail-called.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    const u32 target = word(rt, holder);
    if (target != 0u) call_indirect(rt, ctx, word(rt, word(rt, target) + 8u), {target});
    const u32 node = word(rt, holder + 4u);
    if (node == 0u) return;
    const u32 strong = word(rt, node + 4u);
    put(rt, node + 4u, strong - 1u);
    ctx.gpr[2] = 1u;
    if (strong != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, node) + 8u), {node});
    const u32 weak = word(rt, node + 8u);
    put(rt, node + 8u, weak - 1u);
    ctx.gpr[2] = 1u;
    if (weak != 1u) return;
    ctx.gpr[2] = word(rt, node);
    tail_call(ctx, word(rt, word(rt, node) + 0xCu), {node});
}

void func_08887520(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    release_holder(rt, ctx, ctx.gpr[4]);
}


// 0x088D46C0 (a0 = self, a1 / a2 = out words, a3 = request): with request -1 the counter byte at +0x7A steps by one
// (and wraps to 0 from 3); otherwise the byte is the request. The index (signed byte x 40) selects an entry through
// 0x08B09658 (stored at a1) and 0x08B09530 (stored at a2). v0 = 1.
void func_088D46C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out_first = ctx.gpr[5];
    const u32 out_second = ctx.gpr[6];
    const u32 request = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    u32 value = request;
    if (request == 0xFFFFFFFFu) {
        const u32 next = sign_byte(byte_at(rt, self + 0x7Au) + 1u);
        put8(rt, self + 0x7Au, next);
        value = next;
        if (static_cast<s32>(next) >= 3) {
            put8(rt, self + 0x7Au, 0u);
            value = 0u;
        }
    } else {
        put8(rt, self + 0x7Au, request);
    }
    const u32 index = sign_byte(value);
    const u32 first = call_set(rt, ctx, 0x08B09658u, {self + index * 40u});
    put(rt, out_first, first);
    const u32 again = sign_byte(byte_at(rt, self + 0x7Au));
    const u32 second = call_set(rt, ctx, 0x08B09530u, {self + again * 40u});
    put(rt, out_second, second);
    ctx.gpr[2] = 1u;
}

// 0x08ABC388 (a0 = needle, a1 = key): walks the list at 0x08D7718C. Per node: the object's virtual at vtable+8 with
// (object, key) answers; 0x08B35FD8(answer, needle) decides, and when it is zero the object's virtual at vtable+0x28
// runs with (object, key) and a non-zero answer ends the walk. v0 = that answer, else 0.
void func_08ABC388(Runtime &rt, AllegrexContext &ctx) {
    const u32 needle = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    u32 result = 0u;
    u32 node = word(rt, 0x08D7718Cu);
    while (node != 0u) {
        const u32 object = word(rt, node + 4u);
        const u32 vtable = word(rt, object);
        const u32 answer = call_indirect(rt, ctx, word(rt, vtable + 8u), {object, key});
        if (call_set(rt, ctx, 0x08B35FD8u, {answer, needle}) == 0u) {
            const u32 found = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x28u), {object, key});
            result = found;
            if (found != 0u) break;
        }
        node = word(rt, node);
    }
    ctx.gpr[2] = result;
}

}  // namespace
}  // namespace new_batch_main_1585
}  // namespace gt::game

#define NEW1585(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1585::name, gt::ReturnKind::kind)
NEW1585(0x08A328D4u, func_08A328D4, Word);
NEW1585(0x08A72C14u, func_08A72C14, Word);
NEW1585(0x08A7E450u, func_08A7E450, Word);
NEW1585(0x08B198A8u, func_08B198A8, Word);
NEW1585(0x08B1D430u, func_08B1D430, Word);
NEW1585(0x0885D7C4u, func_0885D7C4, Word);
NEW1585(0x08893ED8u, func_08893ED8, Word);
NEW1585(0x08A38FD4u, func_08A38FD4, Word);
NEW1585(0x08A9E358u, func_08A9E358, Word);
NEW1585(0x088D2904u, func_088D2904, Word);
NEW1585(0x08887520u, func_08887520, Word);
NEW1585(0x088D46C0u, func_088D46C0, Word);
NEW1585(0x08ABC388u, func_08ABC388, Word);
