// New batch main 1929: three constructors, a reference-assignment pair, a table-check chain and one float path, hand-written
// from `tools.disasm` as structured C. Delay slots are read as "before the branch / call"; v0 ends as the asm leaves it (the
// last call's result where a constructor ends on one). s0-s7 and f20-f22 are never written here, so the values the originals
// save in the prologue and restore in the epilogue are the caller's, untouched.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1929 {
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
// The same for a callee that takes its first argument in $f12 and answers in $f0; v0 / v1 / f0 are left as it left them.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// `ll` / `sc` on one thread: the read-modify-write is the whole effect of the loop (an spurious retry changes nothing).
void increment(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
u32 decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

// A counted reference release: `--*[object+4]`, and when that was the last one the vtable's slots 8 and 0xC, the second
// one only if `--*[object+8]` was the last too (the object's vtable is reloaded after the first call, as the asm does).
void release_reference(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (decrement(rt, object + 4u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (decrement(rt, object + 8u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}
// `slot` takes `incoming`: the old reference released, the new one counted (nothing when they are the same word).
void assign_reference(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 incoming) {
    if (incoming == word(rt, slot)) return;
    if (incoming != 0u) increment(rt, incoming + 4u);
    const u32 old = word(rt, slot);
    if (old != 0u) release_reference(rt, ctx, old);
    put(rt, slot, incoming);
}

// 0x088A4A9C (a0 = object): constructor. Vtable 0x08CC29A8, the sub-objects at +8 / +0x20 / +0x270 built in place, the
// block at +0x318, then 4 x 0x18, 4 x 0x1C and 4 x 0xC element loops (a counter running 3 -> -1, four passes each), the
// 0x08B4E648 temporary on the frame for 0x088A42BC, and an 0x80-byte clear at +0x1E8; v0 is that clear's result.
void func_088A4A9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC29A8u);
    put(rt, self + 4u, 0u);
    call(rt, ctx, 0x088A79ACu, {self + 8u, 0u});
    put(rt, self + 0x10u, 0x08CC2AC8u);
    put_float(rt, self + 0x14u, 1400.0f);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x20u, 0x08CC2CB8u);
    put(rt, self + 0x24u, 0x08CC2DDCu);
    put8(rt, self + 0x38u, 1u);
    put(rt, self + 0x28u, self);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put_float(rt, self + 0x50u, 1.0f);
    put(rt, self + 0x54u, 0u);
    put8(rt, self + 0x58u, 0u);
    put8(rt, self + 0x59u, 0u);
    for (u32 i = 0; i < 4u; ++i) {
        const u32 element = self + 0x5Cu + i * 0x18u;
        put(rt, element, 0u);
        put_float(rt, element + 4u, 1.0f);
        put(rt, element + 8u, 0u);
        put8(rt, element + 0xCu, 0u);
    }
    put(rt, self + 0xC0u, 0x12345678u);
    for (u32 i = 0; i < 4u; ++i) {
        const u32 element = self + 0xC4u + i * 0x1Cu;
        put(rt, element, 0x08CC2DF8u);
        put(rt, element + 4u, 0u);
        put(rt, element + 8u, 0u);
        put(rt, element + 0xCu, 0x08CC2E78u);
        put(rt, element + 0x10u, 0u);
        put(rt, element + 0x14u, 0u);
        put(rt, element + 0x18u, 0u);
    }
    for (u32 i = 0; i < 4u; ++i) {
        const u32 element = self + 0x134u + i * 0xCu;
        put(rt, element, 0x08CC2E48u);
        put(rt, element + 4u, 0u);
        put(rt, element + 8u, 0u);
    }
    put(rt, self + 0x1C0u, 0u);
    put(rt, self + 0x1C4u, 0u);
    put(rt, self + 0x1C8u, 0u);
    put(rt, self + 0x1CCu, 0u);
    put8(rt, self + 0x1D0u, 0u);
    put(rt, self + 0x1D4u, 0u);
    put(rt, self + 0x1D8u, 0u);
    put16(rt, self + 0x1DCu, 0u);
    put16(rt, self + 0x1DEu, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    call(rt, ctx, 0x088AA504u, {self + 0x20u, 0u});
    put8(rt, self + 0x269u, 1u);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1E4u, 0u);
    put8(rt, self + 0x268u, 0u);
    put(rt, self + 0x26Cu, 0u);
    call(rt, ctx, 0x088AB254u, {self + 0x270u});
    put(rt, self + 0x318u, 0x08CC2C30u);
    put8(rt, self + 0x2F8u, 1u);
    put(rt, self + 0x2FCu, 0u);
    put(rt, self + 0x310u, 0u);
    put(rt, self + 0x31Cu, 0x08CC2C4Cu);
    put16(rt, self + 0x324u, 0xFFFFu);
    put(rt, self + 0x320u, 0u);
    put(rt, self + 0x328u, 0u);
    put(rt, self + 0x334u, 0u);
    put(rt, self + 0x330u, 0u);
    put(rt, self + 0x32Cu, 0u);
    put(rt, self + 0x338u, 0u);
    put(rt, self + 0x340u, 0u);
    put(rt, self + 0x33Cu, 0u);
    put(rt, self + 0x344u, 0xFFFFFFFFu);
    put(rt, self + 0x348u, 0u);
    call(rt, ctx, 0x08961058u, {self + 0x41Cu, self + 0x34Cu, self + 0x40Cu, 4u});
    call(rt, ctx, 0x08B4E648u, {frame.at(0u)});
    call(rt, ctx, 0x088A42BCu, {self, frame.at(0u)});
    put(rt, self + 0x2D8u, 0u);
    put(rt, self + 0x2DCu, 0u);
    put(rt, self + 0x2E0u, 0u);
    put(rt, self + 0x2E4u, 0u);
    put(rt, self + 0x2E8u, 0u);
    put(rt, self + 0x2ECu, 0u);
    put(rt, self + 0x2F0u, 0u);
    put(rt, self + 0x2F4u, 0u);
    call(rt, ctx, 0x08B36018u, {self + 0x1E8u, 0u, 0x80u});
}

// 0x088FED3C (a0 = owner): 0x08963418 fills two references at sp+8 / sp+0x10; they are assigned to owner+0x14 and owner+0x1C
// (with the words at owner+0x10 / owner+0x18 stored first, in the branch delay slots), the temporaries released, and
// owner+0xB set to 1. v0 = 1 on every path.

// 0x089248A8 (a0 = object, a2 / a3 = two words kept at +8 / +0xC): constructor. Vtable 0x08CC5FC0, ten members of 0x2C bytes
// initialised by 0x089263EC (the vtable words 0x08CC61C0 / 0x08CC6198 / 0x08CC6150 stored after each call), two runs of
// three by 0x08926398, four bound to 0x08CDDC60 by 0x08926488, two more by 0x08926514 (0x08CDDC18 when a3 == 2, else
// 0x08CDDC30), then three iterations of two virtual slot-8 calls. v0 = object + 0x1D0.
void func_089248A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 at_eight = ctx.gpr[6];
    const u32 at_twelve = ctx.gpr[7];
    const u32 member10 = self + 0x10u;
    call(rt, ctx, 0x08925FE4u, {self});
    put(rt, self, 0x08CC5FC0u);
    call(rt, ctx, 0x089263ECu, {member10});
    put(rt, self + 0x10u, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0x3Cu});
    put(rt, self + 0x3Cu, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0x68u});
    put(rt, self + 0x68u, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0x94u});
    put(rt, self + 0x94u, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0xC0u});
    put(rt, self + 0xC0u, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0xECu});
    put(rt, self + 0xECu, 0x08CC61C0u);
    call(rt, ctx, 0x089263ECu, {self + 0x118u});
    put(rt, self + 0x118u, 0x08CC6198u);
    call(rt, ctx, 0x089263ECu, {self + 0x144u});
    put(rt, self + 0x144u, 0x08CC6150u);
    call(rt, ctx, 0x089263ECu, {self + 0x174u});
    put(rt, self + 0x174u, 0x08CC6198u);
    call(rt, ctx, 0x089263ECu, {self + 0x1A0u});
    put(rt, self + 0x1A0u, 0x08CC6150u);
    for (u32 i = 0; i < 3u; ++i) call(rt, ctx, 0x08926398u, {self + 0x1E0u + i * 0x2Cu});
    for (u32 i = 0; i < 3u; ++i) call(rt, ctx, 0x08926398u, {self + 0x264u + i * 0x2Cu});
    put(rt, self + 8u, at_eight);
    put(rt, self + 0xCu, at_twelve);
    call(rt, ctx, 0x08926488u, {member10, 0x08CDDC60u, 1u});
    call(rt, ctx, 0x08926488u, {self + 0x3Cu, 0x08CDDC60u, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xC0u, 0x08CDDC60u, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xECu, 0x08CDDC60u, 1u});
    if (at_twelve == 2u) {
        call(rt, ctx, 0x08926514u, {self + 0x118u, 0x08CDDC18u, 1u});
        call(rt, ctx, 0x08926514u, {self + 0x174u, 0x08CDDC18u, 1u});
    } else {
        call(rt, ctx, 0x08926514u, {self + 0x118u, 0x08CDDC30u, 1u});
        call(rt, ctx, 0x08926514u, {self + 0x174u, 0x08CDDC30u, 1u});
    }
    for (u32 i = 0; i < 3u; ++i) {
        const u32 first = self + 0x1E0u + i * 0x2Cu;
        const u32 second = self + 0x264u + i * 0x2Cu;
        call_indirect(rt, ctx, word(rt, word(rt, first) + 8u), {first, 0x08CDDC00u, 1u});
        call_indirect(rt, ctx, word(rt, word(rt, second) + 8u), {second, 0x08CDDBE8u, 1u});
    }
    put(rt, self + 0x1D0u, 0u);
    put(rt, self + 0x1D4u, 0u);
    put(rt, self + 0x1D8u, 0u);
    put(rt, self + 0x1DCu, 0u);
    ctx.gpr[2] = self + 0x1D0u;
}

// 0x089335F0 (a0 = object): 0x089368DC(0x089363FC(), a1, object[off], object[off + 4]) for the fifteen pairs at +0x18, +0x20,
// +0x28, +0x30, +0x38, +0x40, +0x48, +0x50, +0x58, +0x60, +0x68, +0x78, +0xA8, +0xD8 and +0xE0; the first false answer ends
// it with 0, all fifteen true give 1.
void func_089335F0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 object = ctx.gpr[4];
    static constexpr u32 kOffsets[] = {0x18u, 0x20u, 0x28u, 0x30u, 0x38u, 0x40u, 0x48u, 0x50u,
                                       0x58u, 0x60u, 0x68u, 0x78u, 0xA8u, 0xD8u, 0xE0u};
    u32 answer = 1u;
    for (const u32 offset : kOffsets) {
        const u32 first = word(rt, object + offset);
        const u32 second = word(rt, object + offset + 4u);
        const u32 table = call(rt, ctx, 0x089363FCu);
        if (call(rt, ctx, 0x089368DCu, {table, ctx.gpr[5], first, second}) == 0u) {
            answer = 0u;
            break;
        }
    }
    ctx.gpr[3] = answer;
    ctx.gpr[2] = answer;
}

// 0x08915970 (a0 = owner, a1 = table index, a2 = record index, a3 = out word, t0 = out float): looks the flag bit of a1's
// table word up in the record 0x08955300(owner, a2) selects (its byte at +0xE9); a clear bit answers 0, the indices 1 / 4 / 6
// / 7 tail call, the record's byte at +0x10C decides between the 1.0 fallback and the two 0x08CF1E40 / 0x08CF1C30 tables.

// 0x088EE9A4 (a0 = object): constructor. Vtable 0x08CC3970, 1 / 0x288 / 0x30 / 0x400 / 8-byte allocations through
// 0x08B62344, three 0x08B28DCC members, four 0x90-byte records cleared from +0x218 (half 0xFFFF at +0), then a four-pass
// fill of the +0x458 / +0x468 / +0x488 / +0x4A8 blocks; v0 = object + 3.
void func_088EE9A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC3970u);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0xA8u, 0u);
    const u32 first = call(rt, ctx, 0x08B62344u, {1u});
    call(rt, ctx, 0x088EF418u, {first});
    put(rt, self + 0xACu, first);
    put(rt, self + 0xB0u, 0u);
    put(rt, self + 0xB4u, 0u);
    put(rt, self + 0xB8u, 0u);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC0u, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0xC4u});
    call(rt, ctx, 0x08B28DCCu, {self + 0xECu});
    const u32 second = call(rt, ctx, 0x08B62344u, {0x288u});
    call(rt, ctx, 0x088EF210u, {second});
    put(rt, self + 0x114u, second);
    const u32 third = call(rt, ctx, 0x08B62344u, {0x30u});
    call(rt, ctx, 0x088EEF20u, {third});
    put(rt, self + 0x118u, third);
    put(rt, self + 0x11Cu, 0u);
    put(rt, self + 0x120u, 0u);
    put(rt, self + 0x124u, 0u);
    put16(rt, self + 0x128u, 0xFFFFu);
    put16(rt, self + 0x12Au, 0u);
    put(rt, self + 0x12Cu, 0u);
    put(rt, self + 0x130u, 0u);
    put(rt, self + 0x134u, 0u);
    put16(rt, self + 0x178u, 0xFFFFu);
    put16(rt, self + 0x17Au, 0u);
    put(rt, self + 0x17Cu, 0u);
    put(rt, self + 0x180u, 0u);
    put(rt, self + 0x184u, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x1C8u});
    put(rt, self + 0x208u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x1F0u, 0u);
    put(rt, self + 0x1F4u, 0u);
    put(rt, self + 0x1F8u, 0u);
    put(rt, self + 0x1FCu, 0u);
    put(rt, self + 0x200u, 0u);
    for (u32 i = 0; i < 4u; ++i) {
        const u32 element = self + 0x218u + i * 0x90u;
        call(rt, ctx, 0x08B36018u, {element, 0u, 0x80u});
        put16(rt, element, 0xFFFFu);
        put(rt, element + 0x80u, 0u);
        put(rt, element + 0x84u, 0u);
        put8(rt, element + 0x88u, 0u);
        put8(rt, element + 0x89u, 0u);
    }
    put8(rt, self + 0x524u, 1u);
    put(rt, self + 0x500u, 0u);
    put(rt, self + 0x504u, 0u);
    put8(rt, self + 0x52Fu, 0xFFu);
    put(rt, self + 0x4E0u, 0u);
    put(rt, self + 0x4E4u, 0u);
    put(rt, self + 0x4ECu, 0u);
    put(rt, self + 0x4F4u, 0u);
    put(rt, self + 0x4F8u, 0u);
    put(rt, self + 0x510u, 0xFFFFFFFFu);
    put8(rt, self + 0x514u, 0u);
    put(rt, self + 0x518u, 0u);
    put(rt, self + 0x51Cu, 0u);
    put8(rt, self + 0x520u, 0u);
    put8(rt, self + 0x521u, 0u);
    put8(rt, self + 0x522u, 0u);
    put8(rt, self + 0x523u, 0u);
    put8(rt, self + 0x525u, 0u);
    put8(rt, self + 0x526u, 0u);
    put8(rt, self + 0x527u, 0u);
    put8(rt, self + 0x528u, 0u);
    put8(rt, self + 0x529u, 0u);
    put8(rt, self + 0x52Au, 0u);
    put8(rt, self + 0x52Bu, 0u);
    put8(rt, self + 0x52Cu, 0u);
    put8(rt, self + 0x52Du, 0xFFu);
    put8(rt, self + 0x52Eu, 0xFFu);
    put(rt, self + 0x530u, 0u);
    put(rt, self + 0x534u, 0u);
    put(rt, self + 0x538u, 0u);
    put(rt, self + 0x53Cu, 0u);
    put(rt, self + 0x540u, 0u);
    const u32 table = call(rt, ctx, 0x08B6461Cu, {0x400u});
    put(rt, self + 0x544u, table);
    put(rt, self + 0x54Cu, 0xFFFFFFFFu);
    put(rt, self + 0x548u, 0xFFFFFFFFu);
    put(rt, self + 0x550u, 0u);
    put8(rt, self + 0x554u, 0u);
    call(rt, ctx, 0x08B36018u, {self + 4u, 0u, 0xA0u});
    call(rt, ctx, 0x08B36018u, {self + 0x218u, 0u, 0x240u});
    const u32 handle = call(rt, ctx, 0x08B0979Cu);
    put(rt, self + 0xA8u, call(rt, ctx, 0x08B0980Cu, {handle}));
    put(rt, self + 0x120u, call(rt, ctx, 0x08B6461Cu, {8u}));
    put(rt, self + 0x4E8u, 0u);
    put(rt, self + 0x4F0u, 0u);
    call(rt, ctx, 0x088ED494u, {self + 0x4C0u});
    for (u32 i = 0; i < 4u; ++i) {
        const u32 at = self + i;
        put(rt, self + i * 8u + 0x468u, 0u);
        put(rt, self + i * 8u + 0x46Cu, 0u);
        put(rt, self + i * 8u + 0x488u, 0u);
        put(rt, self + i * 8u + 0x48Cu, 0u);
        put16(rt, self + 0x458u + i * 2u, 0u);
        put8(rt, at + 0x210u, 0u);
        put8(rt, at + 0x460u, 0u);
        put8(rt, at + 0x4B0u, 0u);
        put8(rt, at + 0x4B4u, 0xFFu);
        put8(rt, at + 0x4ACu, 0u);
        put8(rt, at + 0x4B8u, 0u);
        put8(rt, at + 0x4A8u, 0xFFu);
        put(rt, self + 0x28u + i * 0x28u, 0u);
    }
    ctx.gpr[2] = self + 3u;
}

} // namespace
} // namespace new_batch_main_1929
} // namespace gt::game

#define NEW1929(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1929::name, gt::ReturnKind::kind)
NEW1929(0x088A4A9Cu, func_088A4A9C, Word);
NEW1929(0x089248A8u, func_089248A8, Word);
NEW1929(0x089335F0u, func_089335F0, Word);
NEW1929(0x088EE9A4u, func_088EE9A4, Word);
