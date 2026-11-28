// New batch main-2076 (draft, unverified): re-drafts of functions the gate rejected or that two drafters skipped.
// Every body is hand-written structured C from tools.disasm; delay slots are read as "before the branch / call", the
// frame is the prologue's `addiu sp, sp, -N`, `v0` ends as the asm leaves it on every path (a branch-likely delay slot
// does not run when the branch is not taken), and every argument register a callee reads is passed. The callee-saved
// registers are only written where the original keeps a value a callee can see (a saved-register block), and are put
// back before the return or the tail call.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2076 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// The `lui` half a relocated address loads on its own (0x08CB22F8 -> 0x08CB0000, 0x08CFFAAC -> 0x08D00000).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// ll / sc decrement of the word at `address`: true when it was 1 (nothing else runs here, so sc always succeeds).
bool decrement_reaches_zero(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old == 1u;
}

// 0x08AA98E4 (a0 = self): vtable 0x08CD5358, 0x08A71E28(self + 4), then three empty circular lists at +8, +0x20 and
// +0x38 (each {next+0 = 0, prev at +4 = its tail, its own next at +0xC / +0x10}); v0 = self + 8.
void func_08AA98E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CD5358u);
    ctx.gpr[2] = 0x08CD5358u; // the vtable also sits in v0 across the call
    call(rt, ctx, 0x08A71E28u, {self + 4u});
    const u32 head = self + 8u;
    const u32 tail = self + 0x20u;
    put(rt, head + 0x14u, 0u);
    put(rt, head + 0x10u, head + 4u);
    put(rt, head + 0x4u, 0u);
    put(rt, head + 0x8u, 0u);
    put(rt, head + 0xCu, head + 4u);
    put(rt, tail + 0x10u, tail + 4u);
    put(rt, tail + 0x4u, 0u);
    put(rt, tail + 0x14u, 0u);
    put(rt, tail + 0x8u, 0u);
    put(rt, tail + 0xCu, tail + 4u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x3Cu, 0u);
    ctx.gpr[2] = head;
}

// 0x08A19D58 (a0 = self): vtable 0x08CD0658, 0x08A5B9EC(self + 0x24), the block at word(self+4) - 0xC freed unless it
// is the shared empty block 0x08D74FD0 and 0x08B60804(word-4, -1) is not positive; 0x0897D548(self) answers.
void func_08A19D58(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CD0658u);
    call(rt, ctx, 0x08A5B9ECu, {self + 0x24u});
    const u32 pointer = word(rt, self + 4u);
    const u32 block = pointer - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u; // the sentinel is also in v0 at the query
    if (block != 0x08D74FD0u &&
        static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
    }
    call(rt, ctx, 0x0897D548u, {self});
}

// 0x089F1120 (a0 = self): vtable 0x08CBA5F8, the vector at +0x20 trimmed by 0x08B856B0(begin, end, sp), its storage
// released when begin is not null (0x08A5BC54(begin, (end-begin), 4)), +0x1C cleared, then 0x089FDE00(self).
void func_089F1120(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 range = self + 0x20u;
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CBA5F8u);
    call(rt, ctx, 0x08B856B0u, {word(rt, range), word(rt, range + 4u), frame.at(0u)});
    const u32 begin = word(rt, range);
    const u32 bytes = static_cast<u32>(static_cast<s32>(word(rt, range + 8u) - begin) >> 2) << 2;
    if (begin != 0u) {
        ctx.gpr[2] = begin; // v0 holds the begin the free gets
        call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    }
    call(rt, ctx, 0x08A5B9ECu, {self + 0x1Cu});
    call(rt, ctx, 0x089FDE00u, {self});
}

// 0x08ABFC8C (a0 = self, a1 / a2 passed on): vtable 0x08CD6450, +4 initialized, two 0x14-byte members built at +0x30
// (with a1) and +0x44 (with a2) by 0x08ABFC48(that, self, argument, 0); +0x5C / +0x5D and +0x58 cleared. v0 = the
// second construction's result.
void func_08ABFC8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CD6450u);
    ctx.gpr[2] = 0x08CD6450u;
    call(rt, ctx, 0x08B28D34u, {self + 4u});
    call(rt, ctx, 0x08ABFC48u, {self + 0x30u, self, first, 0u});
    call(rt, ctx, 0x08ABFC48u, {self + 0x44u, self, second, 0u});
    put_byte(rt, self + 0x5Du, 0u);
    put(rt, self + 0x58u, 0u);
    put_byte(rt, self + 0x5Cu, 0u);
}

// 0x0891FD68 (a0 = self): vtable 0x08CC5DC8; thirteen words from +0x250 (step 4) are released when non-null through
// their second virtual (called with the object, v0 holding the vtable as the `jalr $v1` leaves it) and cleared; then a
// tail call to 0x08925FF8(self).

// 0x08C7A064 (a0 = self, a1 = source): copies the source's header word, its four relative offsets (each word at
// pointer - 0xC) applied to self, and the six payload words into the new object, then 0x08C771D4(self + 0xC) and
// 0x08C54A7C(self + (*self)->offset, self + 0xC).

// 0x08B17848 (a0 = self, a1 / a2 / a3 / t0 / t1): 0x08B34888(0x08D61100, a1, a2, a3); the base at +0x24, the masked
// flag byte at +0x28, 0x08B26B28(self, a3, t0); +0x1C = a1 + a2, +0x14 / +0x18 = a1; 0x08B7B2E4(self + 0x2C) (which
// stores s0-s7, sp, fp, ra and f20-f31 there, so the s-registers hold the asm's values) and 0x08B25968(self) when it
// answers 0; 0x08B34B24(0x08D61100). v0 = word(+0x1C) - word(+0x14).

// 0x089EFC90 (a0 = self): 0x089E20D0(self), vtable 0x08CBF080 at +0, 0x08CBF2BC at +0x38, the float / word fields down
// to +0x262, a 0x100-iteration spin, then +0x264, +0xA68, +0xE68 and +0xE6C cleared. The spin's last `bnel` is not
// taken, so its delay does not run and v0 ends at 0.
void func_089EFC90(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x089E20D0u, {self});
    put(rt, self, 0x08CBF080u);
    put(rt, self + 0x38u, 0x08CBF2BCu);
    put_float(rt, self + 0x234u, 2.0f);
    put(rt, self + 0x22Cu, 1u);
    put(rt, self + 0x23Cu, 0x08D74FDCu);
    put(rt, self + 0x238u, 0x08D74FDCu);
    put(rt, self + 0x230u, 0u);
    call(rt, ctx, 0x089E4C14u, {self + 0x240u, 0x08CF6ED8u});
    call(rt, ctx, 0x089E4C14u, {self + 0x244u, 0x08CF6ED8u});
    put_float(rt, self + 0x254u, 1.0f);
    put_float(rt, self + 0x258u, -1.0f);
    put(rt, self + 0x248u, 0u);
    put_float(rt, self + 0x24Cu, 1.0f);
    put(rt, self + 0x250u, 0u);
    put(rt, self + 0x25Cu, 0u);
    put_byte(rt, self + 0x260u, 0u);
    put_byte(rt, self + 0x261u, 0u);
    put_byte(rt, self + 0x262u, 0u);
    u32 spin = 0x100u;
    do {
        spin -= 1u;
    } while (spin != 0u);
    put(rt, self + 0x264u, 0u);
    put_byte(rt, self + 0xA68u, 0u);
    put(rt, self + 0xE68u, 0u);
    put(rt, self + 0xE6Cu, 0u);
    ctx.gpr[2] = spin;
}

// 0x08BE0DD8 (a0 = self): vtable 0x08CC6888; the holder at +0x2C released when its ll/sc counters at +4 / +8 hit 1
// (its slots 8 / 0xC), the other object at +0x24 likewise (slot 0xC); vtable 0x08CC3000 stored at +0 and returned.
void func_08BE0DD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC6888u);
    const u32 holder = word(rt, self + 0x2Cu);
    if (holder != 0u) {
        if (decrement_reaches_zero(rt, holder + 4u)) {
            call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
            if (decrement_reaches_zero(rt, holder + 8u)) {
                call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
            }
        }
    }
    const u32 other = word(rt, self + 0x24u);
    if (other != 0u && decrement_reaches_zero(rt, other + 8u)) {
        call_indirect(rt, ctx, word(rt, word(rt, other) + 0xCu), {other});
    }
    put(rt, self, 0x08CC3000u);
    ctx.gpr[2] = 0x08CC3000u;
}

// 0x0882D758 (a0 = self): base 0x08A2A154, vtable 0x08CBBBC0; a 0x10-byte block built by 0x08865D1C with
// 0x08A21F8C(block + 8, 0x08CF6ED8) and 0x08835AA8(block + 0xC) stored at +0x18; the guarded 0x08D75160 global
// (vtable 0x08CBE908) initialized once; +0x1C = 0x08D75168, then 0x08860050(block) answers into +0x14.

// 0x08AA6824 (a0 = self): base 0x08A71E28; +0x14 / +0x10 point at self+8; +8 / +0xC / +0x18 zero; +0x1C / +0x20 /
// +0x24 zero; a 4-word block from 0x08A5BC20(0, 4) stored at all three; 0x08C30E0C(block, 0, &0x08D74FDC, &byte) then
// updates the slot at sp+4 (that word, not the call's v0, names the block freed unless it is 0x08D74FDC). +0x28 = 0xC,
// v0 = word(+0x1C), also stored at +0x20.
void func_08AA6824(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08A71E28u, {self});
    put(rt, self + 0x14u, self + 8u);
    put(rt, self + 0x10u, self + 8u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0u, 4u});
    put(rt, self + 0x1Cu, block);
    put(rt, self + 0x20u, block);
    put(rt, self + 0x24u, block);
    put(rt, frame.at(4u), 0x08D74FDCu);
    call(rt, ctx, 0x08C30E0Cu, {block, 0u, frame.at(4u), frame.at(1u)});
    const u32 tail = word(rt, frame.at(4u));
    ctx.gpr[2] = tail; // the query gets the slot word as v0
    if (tail - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {tail - 4u, 0xFFFFFFFFu})) <= 0) {
            call(rt, ctx, 0x08B8542Cu, {tail - 0xCu, frame.at(0u)});
        }
    }
    const u32 head = word(rt, self + 0x1Cu);
    put(rt, self + 0x20u, head);
    put(rt, self + 0x28u, 0xCu);
    ctx.gpr[2] = head;
}

// 0x089DE0D8 (a0 = self, a1 = source of float pairs, a2 = count): the vector at self + 0xEC + 12 * bit7(self+0x140)
// grown by 0x08BF1080(begin field, begin + index, end) when it has room, else by 0x08BF1768(field, end,
// count - room, sp); then `count` pairs copied in. v0 = self + 12 * bit7 when it copied, else the grow's result.
void func_089DE0D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 flag = (word(rt, self + 0x140u) >> 7) & 1u;
    const u32 field = self + 0xECu + flag * 12u;
    const u32 begin = word(rt, field);
    const u32 end = word(rt, field + 4u);
    const u32 room = static_cast<u32>(static_cast<s32>(end - begin) >> 3);
    u32 answer;
    ctx.gpr[2] = room; // v0 holds the room the grow calls get
    if (count < room) {
        answer = call(rt, ctx, 0x08BF1080u, {field, begin + count * 8u, end});
    } else {
        answer = call(rt, ctx, 0x08BF1768u, {field, end, count - room, frame.at(0u)});
    }
    if (count != 0u) {
        u32 target = begin;
        for (u32 i = 0u; i < count; ++i) {
            put_float(rt, target, float_at(rt, source + i * 8u));
            put_float(rt, target + 4u, float_at(rt, source + i * 8u + 4u));
            target += 8u;
        }
        answer = self + flag * 12u;
    }
    ctx.gpr[2] = answer;
}

// 0x089DE240: the same copy through the vector at self + 0x104 + 12 * bit8(self+0x140).
void func_089DE240(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 flag = (word(rt, self + 0x140u) >> 8) & 1u;
    const u32 field = self + 0x104u + flag * 12u;
    const u32 begin = word(rt, field);
    const u32 end = word(rt, field + 4u);
    const u32 room = static_cast<u32>(static_cast<s32>(end - begin) >> 3);
    u32 answer;
    ctx.gpr[2] = room; // v0 holds the room the grow calls get
    if (count < room) {
        answer = call(rt, ctx, 0x08BF1080u, {field, begin + count * 8u, end});
    } else {
        answer = call(rt, ctx, 0x08BF1768u, {field, end, count - room, frame.at(0u)});
    }
    if (count != 0u) {
        u32 target = begin;
        for (u32 i = 0u; i < count; ++i) {
            put_float(rt, target, float_at(rt, source + i * 8u));
            put_float(rt, target + 4u, float_at(rt, source + i * 8u + 4u));
            target += 8u;
        }
        answer = self + flag * 12u;
    }
    ctx.gpr[2] = answer;
}

// 0x088F9888 (a0 = self, a1 = value): vtable 0x08CC3F08, +4 = 0, an 0x18-byte block from 0x08B62344 with vtable
// 0x08CC3FD8 at +0 / 0x08CC4004 at +4 / zero +8..+0x10 / byte +0x14 = 0 stored at +8; the guarded 0x08D76140 global
// (vtable 0x08CC4090) initialized once; then self+0xC = the guard vtable pointer and block+0x10 = the value.
void func_088F9888(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC3F08u);
    put(rt, self + 4u, 0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3FD8u);
    put(rt, block + 4u, 0x08CC4004u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put_byte(rt, block + 0x14u, 0u);
    put(rt, self + 8u, block);
    if (signed_byte(rt, 0x08D76140u) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D76140u}) != 0u) {
        put(rt, 0x08D7614Cu, 0x08CC4090u);
        ctx.gpr[2] = 0x08CC4090u;
        call(rt, ctx, 0x08B64494u, {0x08D76140u});
    }
    put(rt, self + 0xCu, 0x08D7614Cu);
    put(rt, block + 0x10u, value);
    ctx.gpr[2] = block;
}

// Copy-construct a vector of words (0x08B2A9D0 / 0x08B2AEC4, a0 = out, a1 = source {allocator, begin, end}):
// length check, 0x08B62344(bytes), 0x08C503D4(block, count, &0, &byte), then the words copied while dest <= end - 4.
// v0 = 1: the loop's final `sltu`, whose `beql` delay does not run once the branch is not taken.
void copy_vector(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    put(rt, out, word(rt, source));
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    put(rt, out + 0xCu, 0u);
    const u32 begin = word(rt, source + 4u);
    const u32 end = word(rt, source + 8u);
    const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    if (0x3FFFFFFFu < count) {
        call(rt, ctx, 0x08B5A9C8u, {out});
    }
    const u32 bytes = count << 2;
    const u32 block = call(rt, ctx, 0x08B62344u, {bytes});
    put(rt, out + 0xCu, block + bytes);
    put(rt, out + 4u, block);
    put(rt, out + 8u, block);
    put(rt, frame.at(4u), 0u);
    call(rt, ctx, 0x08C503D4u, {block, count, frame.at(4u), frame.at(0u)});
    put(rt, out + 8u, block + bytes);
    u32 dest = word(rt, out + 4u);
    u32 src = word(rt, source + 4u);
    const u32 last = dest + bytes - 4u;
    for (;;) {
        if (last < dest) break;
        const u32 next = word(rt, src);
        src += 4u;
        put(rt, dest, next);
        dest += 4u;
    }
    ctx.gpr[2] = 1u;
}
void func_08B2A9D0(Runtime &rt, AllegrexContext &ctx) { copy_vector(rt, ctx); }
void func_08B2AEC4(Runtime &rt, AllegrexContext &ctx) { copy_vector(rt, ctx); }

// 0x08AF1E98 (a0 = self): vtable 0x08CD7000; a 0x330-byte block from 0x08B62344 with +4..+0x14 zero, 0x20 records of
// three zero words at +0x18 (step 0x14), -1 at +0x2AC, 1 at +0x313, 0x08CB4AA8 at +0x320, the zero words down to
// +0x2BC and the bytes / words at +0x310..+0x31C; stored at self+4, self+8 = 0x08D77278. The guarded 0x08D77270 global
// (vtable 0x08CD7018 at 0x08D77278) initialized once. v0: the `lui` half 0x08D70000, or 0x08B64494's answer when it
// initialized (the `lui 0x4D` value is overwritten by that call).
void func_08AF1E98(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CD7000u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x330u});
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put(rt, block + 0x14u, 0u);
    u32 record = block + 0x18u;
    for (u32 i = 0x20u; i != 0u; i -= 1u) {
        put(rt, record, 0u);
        put(rt, record + 4u, 0u);
        put(rt, record + 8u, 0u);
        record += 0x14u;
    }
    put(rt, block + 0x2ACu, 0xFFFFFFFFu);
    put_byte(rt, block + 0x313u, 1u);
    put(rt, block + 0x320u, 0x08CB4AA8u);
    put(rt, block + 0x298u, 0u);
    put(rt, block + 0x29Cu, 0u);
    put(rt, block + 0x2A0u, 0u);
    put(rt, block + 0x2A4u, 0u);
    put(rt, block + 0x2A8u, 0u);
    put(rt, block + 0x2B0u, 0u);
    put(rt, block + 0x2B4u, 0u);
    put(rt, block + 0x2B8u, 0u);
    put(rt, block + 0x2BCu, 0u);
    put_byte(rt, block + 0x310u, 0u);
    put_byte(rt, block + 0x311u, 1u);
    put_byte(rt, block + 0x312u, 1u);
    put(rt, block + 0x31Cu, 0u);
    put(rt, block + 0x318u, 0u);
    put(rt, block + 0x314u, 0u);
    put(rt, self + 4u, block);
    u32 answer = high_half(0x08D77278u);
    if (signed_byte(rt, 0x08D77270u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D77270u}) != 0u) {
            put(rt, 0x08D77278u, 0x08CD7018u);
            ctx.gpr[2] = 0x08CD7018u;
            answer = call(rt, ctx, 0x08B64494u, {0x08D77270u});
        }
    }
    put(rt, self + 8u, 0x08D77278u);
    ctx.gpr[2] = answer;
}

// 0x08B1A06C (a0): 0x08B1A02C(word(a0)) answers.

// 0x08B779F0 / 0x08B77AAC / 0x08B77AC8: thunks that keep their arguments and answer their callee's v0.

// 0x08BAA4E0 (a0 = first element, a1 = one past the last): stores 0x08CC1AA8 at +0xA4 of every 0xAC-byte element while
// incrementing; equal arguments store nothing and leave v0 at the `lui` half 0x08CC0000.

// 0x08AA60D4 / 0x08AA6150 (a0 = self): 0x08AA5E24(self) runs (arguments kept) and v0 = self.

// 0x08ACC980 (a0 = self): 0x08ACC8C0(self) then a tail call to 0x08ACC944(self).
// 0x08ACC9A8: the same through 0x08ACC8D8.

// 0x08AA602C (a0 = self, a1 masked to a half): 0x08AA5DB4(self, a1 & 0xFFFF) and v0 = self.

// 0x08B2E030: 0x08B2DF58's answer plus 0x7E90, and v1 += the unsigned carry of that add.

// 0x08C31B00 (a0 = self, a1 masked to a half): 0x08AA9360(self, a1 & 0xFFFF) and v0 = self.

// 0x08B193CC (a0 = self): 0x08B1935C(self) answers a1, then a tail call to 0x08B18C78(self, that).

} // namespace
} // namespace new_batch_main_2076
} // namespace gt::game

#define NEW2076(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2076::name, gt::ReturnKind::kind)
NEW2076(0x08AA98E4u, func_08AA98E4, Word);
NEW2076(0x08A19D58u, func_08A19D58, Word);
NEW2076(0x089F1120u, func_089F1120, Word);
NEW2076(0x08ABFC8Cu, func_08ABFC8C, Word);
NEW2076(0x089EFC90u, func_089EFC90, Word);
NEW2076(0x08BE0DD8u, func_08BE0DD8, Word);
NEW2076(0x08AA6824u, func_08AA6824, Word);
NEW2076(0x089DE0D8u, func_089DE0D8, Word);
NEW2076(0x089DE240u, func_089DE240, Word);
NEW2076(0x088F9888u, func_088F9888, Word);
NEW2076(0x08B2A9D0u, func_08B2A9D0, Word);
NEW2076(0x08B2AEC4u, func_08B2AEC4, Word);
NEW2076(0x08AF1E98u, func_08AF1E98, Word);
