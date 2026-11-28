// New batch main-1596: structured C from tools.disasm for the functions of main-session batch 1596 (drafted, unverified:
// the verdict runs the gate). Skipped as too hard for a draft: 08B57420 (switch through a jump table), 08A58634, 08BDC6EC.
// Atomic ll/sc counts are plain read-modify-writes (the emulated core is single-threaded). Unaligned lwl/lwr pairs are
// composed from byte loads, so they read the same bytes for any alignment.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1596 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// `lw v1,0(object); lw v0,slot(v1); jalr v0` with a0 = object; v0 / v1 are the callee's, as after the jalr.
u32 vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    const u32 target = word(rt, word(rt, object) + slot);
    return call_indirect(rt, ctx, target, {object});
}

// ll/sc add: returns the word's old value.
u32 add_word(Runtime &rt, u32 address, u32 delta) {
    const u32 old = word(rt, address);
    put(rt, address, old + delta);
    return old;
}

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 byte_at(Runtime &rt, u32 address) { return static_cast<u32>(rt.memory().load8(address)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
// The lwl/lwr pair at `address`: the little-endian word at any alignment.
u32 unaligned_word(Runtime &rt, u32 address) {
    return byte_at(rt, address) | (byte_at(rt, address + 1u) << 8) | (byte_at(rt, address + 2u) << 16) |
           (byte_at(rt, address + 3u) << 24);
}

// Shared by 089C688C and 089C649C: walk the list at self+0x3C (a sentinel node at sp+0xC), each node's payload object
// gets the vtable call at `slot` after a conversion through 089C4FD4. Leaves the frame as the caller set it.
void list_walk(Runtime &rt, AllegrexContext &ctx, u32 self, u32 slot) {
    const u32 sp = ctx.gpr[29];
    const u32 head = sp + 0xCu;
    const u32 owner = word(rt, self + 0x3Cu);
    put(rt, sp + 0x1Cu, 0u);
    put(rt, sp + 0xCu, 0u);
    put(rt, sp + 0x10u, 0u);
    put(rt, sp + 0x14u, head);
    put(rt, sp + 0x18u, head);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
    call(rt, ctx, 0x08A7D1F8u, {owner, sp + 8u});
    u32 node = word(rt, sp + 0x14u);
    if (node != head) {
        do {
            call(rt, ctx, 0x089B4108u, {sp + 4u});
            u32 payload = word(rt, node + 0x14u);
            if (payload == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA63CCu});
            const u32 vt = word(rt, payload);
            const u32 first = word(rt, vt + 0x1Cu);
            call(rt, ctx, 0x08A76AA0u, {sp});
            call_indirect(rt, ctx, first, {payload, sp + 4u, sp});
            call(rt, ctx, 0x08A5B9ECu, {sp});
            payload = word(rt, sp + 4u);
            if (payload == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6378u});
            const u32 vt2 = word(rt, payload);
            const u32 converted = call(rt, ctx, 0x089C4FD4u, {self});
            const u32 second = word(rt, vt2 + slot);
            call_indirect(rt, ctx, second, {payload, converted});
            node = call(rt, ctx, 0x08B5F100u, {node});
            call(rt, ctx, 0x08A5B9ECu, {sp + 4u});
        } while (node != head);
    }
}

void finish_list(Runtime &rt, AllegrexContext &ctx) {
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x08C00860u, {sp + 8u, word(rt, sp + 0x10u)});
}


void func_08939BF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 obj = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0xE0u);
    if (word(rt, obj + 4u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x08B0CE58u, {sp + 0x40u});
    call(rt, ctx, 0x08B0EC60u, {sp + 0x80u, a1});
    for (u32 i = 0; i < 0x3Cu; i += 4u) put(rt, sp + i, word(rt, sp + 0x80u + i));
    for (u32 i = 0; i < 0x3Cu; i += 4u) put(rt, sp + 0x40u + i, word(rt, sp + i));
    const u32 result = call(rt, ctx, 0x08939044u, {obj, a1, a2, a3});
    put(rt, obj + 4u, result);
    if (word(rt, result) != 0x54535447u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08939AE8u, {obj});
    call(rt, ctx, 0x08939BC0u, {obj});
    ctx.gpr[2] = 1u;
}

void func_089D0900(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08994FD4u, {self});
        put(rt, self, 0x08CC9D60u);
        put(rt, self + 0x38u, 0x08CC9FB8u);
        const u32 flags = (word(rt, self + 0x14Cu) | 1u) & ~0x7Eu;
        put(rt, self + 0x134u, 0x3E4CCCCDu);
        put(rt, self + 0x14Cu, flags);
        put(rt, self + 0x150u, 0x08D74FDCu);
        put(rt, self + 0x130u, 1u);
        put(rt, self + 0x13Cu, 0x3F800000u);
        put(rt, self + 0x12Cu, 1u);
        put(rt, self + 0x138u, 0x3F800000u);
        put(rt, self + 0x140u, 0x3F000000u);
        put(rt, self + 0x144u, 0u);
        put(rt, self + 0x148u, 0u);
        call(rt, ctx, 0x08969188u, {self + 0x154u, 0x08CF6ED8u});
        put(rt, self + 0x15Cu, 0x41800000u);
        put(rt, self + 0x158u, 0x41800000u);
        for (u32 i = 0; i < 6u; ++i) put(rt, self + 0x160u + 4u * i, 0u);
        put(rt, self + 0x178u, 0u);
        call(rt, ctx, 0x08A76AA0u, {self + 0x17Cu});
        call(rt, ctx, 0x08A76AA0u, {self + 0x180u});
        call(rt, ctx, 0x08A76AA0u, {self + 0x184u});
        call(rt, ctx, 0x08969188u, {self + 0x188u, 0x08CF6ED8u});
        const u32 e0 = word(rt, self + 0xE0u);
        put(rt, self + 0x1BCu, 0xFFFFFFFFu);
        put(rt, self + 0xE0u, e0 & ~(1u << 24));
        put(rt, self + 0x18Cu, 0u);
        put(rt, self + 0x190u, 0u);
        put(rt, self + 0x194u, 0u);
        put(rt, self + 0x198u, 0u);
        put(rt, self + 0x19Cu, 0xFFFFFFFFu);
        put(rt, self + 0x1A0u, 0xFFFFFFFFu);
        put(rt, self + 0x1A4u, 0xFFFFFFFFu);
        put(rt, self + 0x1A8u, 0xFFFFFFFFu);
        put(rt, self + 0x1B0u, 0u);
        put(rt, self + 0x1B4u, 0xFFFFFFFFu);
        put(rt, self + 0x1B8u, 0u);
        put(rt, self + 0x1C0u, 0u);
        put8(rt, self + 0x1C4u, 0u);
    }
    tail_call(ctx, 0x089689C8u, {self, 1u});
}

void func_08911A20(Runtime &rt, AllegrexContext &ctx) {
    const u32 src = ctx.gpr[4];
    const u32 dst = ctx.gpr[5];
    u32 v0 = 0u;
    u32 s0 = 0u;
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 p1 = word(rt, src);
        s0 = word(rt, src + 4u);
        if (s0 != 0u) add_word(rt, s0 + 4u, 1u);
        const u32 old = word(rt, dst + 4u);
        v0 = old;
        put(rt, dst, p1);
        const u32 s2 = dst + 4u;
        if (s0 != old) {
            if (s0 != 0u) {
                add_word(rt, s0 + 4u, 1u);
                v0 = 1u;
            }
            const u32 s1 = word(rt, s2);
            if (s1 != 0u) {
                const u32 old1 = add_word(rt, s1 + 4u, 0xFFFFFFFFu);
                v0 = 1u;
                if (old1 == 1u) {
                    v0 = vcall(rt, ctx, s1, 8u);
                    const u32 old2 = add_word(rt, s1 + 8u, 0xFFFFFFFFu);
                    v0 = 1u;
                    if (old2 == 1u) v0 = vcall(rt, ctx, s1, 0xCu);
                }
            }
            put(rt, s2, s0);
        }
        if (s0 != 0u) {
            const u32 old3 = add_word(rt, s0 + 4u, 0xFFFFFFFFu);
            v0 = 1u;
            if (old3 == 1u) {
                v0 = vcall(rt, ctx, s0, 8u);
                const u32 old4 = add_word(rt, s0 + 8u, 0xFFFFFFFFu);
                v0 = 1u;
                tail = (old4 == 1u);
            }
        }
        ctx.gpr[2] = v0;
    }
    if (tail) tail_call(ctx, word(rt, word(rt, s0) + 0xCu), {s0});
}

void func_088E939C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self + 8u, 9u);
    put(rt, self, 0xFFFFFFFFu);
    put(rt, self + 4u, 3u);
    put(rt, self + 0xCu, 0u);
    put8(rt, self + 0x10u, 0u);
    put8(rt, self + 0x11u, 0u);
    put8(rt, self + 0x12u, 0u);
    put8(rt, self + 0x13u, 0u);
    put8(rt, self + 0x14u, 0u);
    put8(rt, self + 0x15u, 1u);
    put8(rt, self + 0x16u, 0u);
    put8(rt, self + 0x17u, 1u);
    put8(rt, self + 0x18u, 0u);
    put8(rt, self + 0x19u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put16(rt, self + 0x20u, 0xFFFFu);
    put8(rt, self + 0x22u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x1FCu, 0xFFFFFFFFu);
    call(rt, ctx, 0x088D0578u, {self + 0x200u});
    put8(rt, self + 0x2A0u, 0xFFu);
    put8(rt, self + 0x2A1u, 0u);
    put8(rt, self + 0x2A2u, 0u);
    const u32 s1 = call(rt, ctx, 0x08B62344u, {0x3Cu});
    call(rt, ctx, 0x08B28D34u, {s1});
    put(rt, s1 + 0x2Cu, 0u);
    put8(rt, s1 + 0x30u, 0u);
    put(rt, s1 + 0x34u, 0u);
    put(rt, s1 + 0x38u, 0u);
    put(rt, self + 0x2D0u, 0u);
    put(rt, self + 0x2D4u, 0u);
    put8(rt, self + 0x2B4u, 1u);
    put(rt, self + 0x2B0u, s1);
    put(rt, self + 0x2BCu, 0xFFFFFFFFu);
    put(rt, self + 0x2C8u, 3u);
    put(rt, self + 0x2D8u, 0xFFFFFFFFu);
    put(rt, self + 0x2DCu, 0xFFFFFFFFu);
    put8(rt, self + 0x2B5u, 0u);
    put8(rt, self + 0x2B6u, 0u);
    put8(rt, self + 0x2B7u, 0u);
    put(rt, self + 0x2B8u, 0xFFFFFFFFu);
    put(rt, self + 0x2C0u, 0u);
    put(rt, self + 0x2C4u, 3u);
    put(rt, self + 0x2E0u, 0u);
    put(rt, self + 0x2E4u, 0u);
    put(rt, self + 0x2E8u, 0x08CC3778u);
    put(rt, self + 0x2F4u, 0u);
    put(rt, self + 0x2ECu, 0u);
    put(rt, self + 0x2F0u, 0u);
    for (u32 i = 0; i < 3u; ++i) put(rt, self + 0x2A4u + 4u * i, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

void func_0897F89C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    constexpr u32 kTable = 0x08CF4100u;
    constexpr u32 kList = 0x08D74FD0u;
    constexpr u32 kNode = 0x08D74FDCu;
    const u32 s4 = self + 0x1E4u;
    call(rt, ctx, 0x08B344E0u, {kTable});
    put(rt, sp + 4u, kNode);
    if (word(rt, self + 0x1E4u) != s4) {
        call(rt, ctx, 0x08B855E0u, {sp + 4u, word(rt, self + 0x1E4u) + 8u});
        const u32 s0 = word(rt, self + 0x1E4u);
        call(rt, ctx, 0x08B5C094u, {s0});
        const u32 node = word(rt, s0 + 8u) - 0xCu;
        if (node != kList) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {word(rt, s0 + 8u) - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {node, sp + 1u});
        }
        call(rt, ctx, 0x08A5BC54u, {s0, 0xCu, 4u});
    }
    u32 flag = 0u;
    if (word(rt, self + 0x1E4u) != s4) {
        const u32 hd = word(rt, self + 0x1E4u);
        const u32 n1 = word(rt, word(rt, hd + 8u) - 0xCu);
        const u32 s2 = word(rt, sp + 4u);
        const u32 s0b = word(rt, s2 - 0xCu);
        put(rt, sp + 8u, n1);
        put(rt, sp + 0xCu, s0b);
        const u32 a3 = (s0b < n1) ? sp + 0xCu : sp + 8u;
        const u32 ret = call(rt, ctx, 0x08B36170u, {word(rt, hd + 8u), s2, word(rt, a3)});
        if (ret != 0u) {
            flag = 1u;
        } else {
            flag = (n1 == s0b) ? 0u : 1u;
        }
        const u32 s0c = s2 - 0xCu;
        if (kList != s0c) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {s2 - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {s0c, sp});
        }
    } else {
        flag = 0u;
        const u32 s2 = word(rt, sp + 4u);
        const u32 s0c = s2 - 0xCu;
        if (kList != s0c) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {s2 - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {s0c, sp});
        }
    }
    call(rt, ctx, 0x08B3457Cu, {kTable});
    if (flag != 0u) call(rt, ctx, 0x0897E2E0u, {self});
}




void func_08871794(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x0887E1E0u, {self});
    put(rt, self, 0x08CBF9A0u);
    const u32 o = call(rt, ctx, 0x08B62344u, {0x70u});
    put(rt, o, 0u);
    put(rt, o + 4u, 0x3F800000u);
    const u32 a2 = o + 4u;
    const u32 v3 = word(rt, a2 + 0x20u);
    put(rt, a2 + 8u, 0u);
    put(rt, a2 + 0xCu, 0u);
    put(rt, a2 + 0x20u, v3 & ~7u);
    put(rt, a2 + 0x18u, 0u);
    put(rt, a2 + 0x1Cu, 0u);
    put(rt, a2 + 0x10u, 0u);
    put(rt, a2 + 0x14u, 0u);
    put(rt, a2 + 4u, 0x40000000u);
    put(rt, o + 0x28u, 0u);
    const u32 a1 = o + 0x28u;
    const u32 v3a1 = word(rt, a1 + 0x20u);
    put(rt, a1 + 8u, 0u);
    put(rt, a1 + 0xCu, 0u);
    put(rt, a1 + 0x20u, v3a1 & ~7u);
    put(rt, a1 + 0x18u, 0u);
    put(rt, a1 + 0x10u, 0u);
    put(rt, a1 + 0x1Cu, 0u);
    put(rt, a1 + 0x14u, 0u);
    put(rt, a1 + 4u, 0x3F800000u);
    put(rt, o + 0x4Cu, 0u);
    const u32 a3 = o + 0x4Cu;
    const u32 v3a3 = word(rt, a3 + 0x20u);
    put(rt, a3 + 8u, 0u);
    put(rt, a3 + 0xCu, 0u);
    put(rt, a3 + 4u, 0x3F800000u);
    put(rt, a3 + 0x18u, 0u);
    put(rt, a3 + 0x1Cu, 0u);
    put(rt, a3 + 0x20u, v3a3 & ~7u);
    put(rt, a3 + 0x10u, 0u);
    put(rt, a3 + 0x14u, 0u);
    put(rt, a2 + 0x20u, word(rt, a2 + 0x20u) | 4u);
    put(rt, self + 0xCu, o);
    u32 v0 = static_cast<u32>(signed_byte(rt, 0x08D75370u));
    if (v0 == 0u) {
        v0 = call(rt, ctx, 0x08B6441Cu, {0x08D75370u});
        if (v0 != 0u) {
            put(rt, 0x08D7537Cu, 0x08CBF9C8u);
            v0 = call(rt, ctx, 0x08B64494u, {0x08D75370u});
        }
    }
    put(rt, self + 0x10u, 0x08D7537Cu);
    ctx.gpr[2] = v0;
}


void func_08AF54BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 dst = ctx.gpr[5];
    const u32 src = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = ctx.gpr[29];
    if (word(rt, self + 0x9Cu) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08C3FBF0u, {sp, 0u});
    if (src != 0u) {
        const u32 end = src + count * 28u;
        u32 rec = src;
        while (rec != end) {
            const u32 id = word(rt, rec);
            const u32 mask_at = sp + ((static_cast<u32>(static_cast<s32>(id) >> 5)) << 2);
            const u32 t3 = unaligned_word(rt, rec + 4u);
            const u32 t4 = unaligned_word(rt, rec + 8u);
            const u32 t0 = unaligned_word(rt, rec + 0xCu);
            const u32 t1 = unaligned_word(rt, rec + 0x10u);
            const u32 t2 = unaligned_word(rt, rec + 0x14u);
            const u32 t5 = unaligned_word(rt, rec + 0x18u);
            put(rt, mask_at, word(rt, mask_at) | (1u << (id & 31u)));
            const u32 out = dst + id * 24u;
            put(rt, out + 0x14u, t5);
            put(rt, out, t3);
            put(rt, out + 4u, t4);
            put(rt, out + 8u, t0);
            put(rt, out + 0xCu, t1);
            put(rt, out + 0x10u, t2);
            rec += 0x1Cu;
        }
    }
    const u32 limit = static_cast<u32>(rt.memory().load16(self + 0x98u)) << 7;
    const u32 table = word(rt, self + 0x9Cu);
    const u32 blk_end = table + limit;
    ctx.gpr[2] = limit;
    if (table == blk_end) return;
    u32 blk = table;
    u32 out = dst;
    u32 index = 0u;
    do {
        const u32 mask_word = word(rt, sp + ((index >> 5) << 2));
        const u32 clear = ((mask_word >> (index & 31u)) & 1u) ^ 1u;
        ctx.gpr[2] = clear;
        if (clear != 0u) {
            put(rt, out, word(rt, blk + 0x50u));
            put(rt, out + 4u, word(rt, blk + 0x54u));
            put(rt, out + 8u, word(rt, blk + 0x58u));
            put(rt, out + 0xCu, word(rt, blk + 0x5Cu));
            put(rt, out + 0x10u, word(rt, blk + 0x60u));
            put(rt, out + 0x14u, word(rt, blk + 0x64u));
        }
        blk += 0x80u;
        out += 0x18u;
        index += 1u;
    } while (blk != blk_end);
}

void func_089E8964(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x0899A930u, {self});
        put(rt, self, 0x08CC93B0u);
        const u32 flags = (word(rt, self + 0x150u) | 1u) & ~0x1Eu;
        put(rt, self + 0x38u, 0x08CC9608u);
        put(rt, self + 0x134u, 0x3E4CCCCDu);
        put(rt, self + 0x150u, flags);
        put(rt, self + 0x130u, 1u);
        put(rt, self + 0x13Cu, 0x3F800000u);
        put(rt, self + 0x144u, 0x3F000000u);
        put(rt, self + 0x12Cu, 1u);
        put(rt, self + 0x138u, 0x3F800000u);
        put(rt, self + 0x140u, 0x3F000000u);
        put(rt, self + 0x148u, 0u);
        put(rt, self + 0x14Cu, 0u);
        put(rt, self + 0x154u, 0u);
        put(rt, self + 0x158u, 0u);
        call(rt, ctx, 0x08969188u, {self + 0x15Cu, 0x08CF6ED8u});
        put(rt, self + 0x164u, 0x41800000u);
        put(rt, self + 0x160u, 0x41800000u);
        for (u32 i = 0; i < 6u; ++i) put(rt, self + 0x168u + 4u * i, 0u);
        put(rt, self + 0x180u, 0u);
        call(rt, ctx, 0x08A76AA0u, {self + 0x184u});
        call(rt, ctx, 0x08A76AA0u, {self + 0x188u});
        call(rt, ctx, 0x08A76AA0u, {self + 0x18Cu});
        call(rt, ctx, 0x08969188u, {self + 0x190u, 0x08CF6ED8u});
        const u32 e0 = word(rt, self + 0xE0u);
        put(rt, self + 0x1D4u, 0xFFFFFFFFu);
        put(rt, self + 0x1E0u, 0x08D74FDCu);
        put(rt, self + 0xE0u, e0 & ~(1u << 24));
        put(rt, self + 0x194u, 0u);
        put(rt, self + 0x198u, 0u);
        put(rt, self + 0x19Cu, 0xFFFFFFFFu);
        put(rt, self + 0x1A0u, 0xFFFFFFFFu);
        put(rt, self + 0x1A4u, 0xFFFFFFFFu);
        put(rt, self + 0x1A8u, 0xFFFFFFFFu);
        put(rt, self + 0x1ACu, 0xFFFFFFFFu);
        put(rt, self + 0x1B0u, 0xFFFFFFFFu);
        put(rt, self + 0x1B8u, 0u);
        put(rt, self + 0x1BCu, 0xFFFFFFFFu);
        put(rt, self + 0x1C0u, 0u);
        put(rt, self + 0x1C4u, 0xFFFFFFFFu);
        put(rt, self + 0x1C8u, 0u);
        put(rt, self + 0x1CCu, 0xFFFFFFFFu);
        put(rt, self + 0x1D0u, 0u);
        put(rt, self + 0x1D8u, 0u);
        put8(rt, self + 0x1DCu, 0u);
    }
    tail_call(ctx, 0x089689C8u, {self, 1u});
}

} // namespace
} // namespace new_batch_main_1596
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1596::name, gt::ReturnKind::kind)
NEWM(0x08939BF4u, func_08939BF4, Word);
NEWM(0x089D0900u, func_089D0900, Word);
NEWM(0x08911A20u, func_08911A20, Word);
NEWM(0x088E939Cu, func_088E939C, Word);
NEWM(0x0897F89Cu, func_0897F89C, Word);
NEWM(0x08871794u, func_08871794, Word);
NEWM(0x08AF54BCu, func_08AF54BC, Word);
NEWM(0x089E8964u, func_089E8964, Word);
