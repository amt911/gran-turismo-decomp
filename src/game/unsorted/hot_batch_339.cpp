// Hot batch 339: functions no route reaches, written as structured C from `tools.disasm` (control flow as ifs and loops,
// named locals; the argument registers are read once at the top and the result register written once at the end) and
// verified by synthetic cases. Leftover values of `v0` that the asm leaves (the result of the
// last call) are kept, because the harness compares `v0`.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_339 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal` (a callee that never writes v0 returns what
// v0 held when it was called, so the caller's v0 has to stay current).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void set_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void set_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Function-local `static` guard (a byte at `guard`; 0x08B6441C acquires, 0x08B64494 releases). Returns true when the caller
// has to construct the instance and release the guard afterwards.
bool guard_acquire(Runtime &rt, AllegrexContext &ctx, u32 guard) {
    return byte_at(rt, guard) == 0u && call(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
}

// A variable stack allocation (`subu sp, sp, ((n << 2) + 0xF) & ~0xF` in the original): `count` words.
u32 scratch_bytes(u32 count) { return ((count << 2) + 0xFu) & ~0xFu; }

// 0x08ADD41C
void func_08ADD41C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kTable = 0x08D40628u;
    const u32 lock = self + 0x78u;
    call(rt, ctx, 0x08ADD2ACu, {lock});
    const u32 name = word(rt, self + 0x18u);
    ctx.gpr[2] = name;
    if (name == 0u) return;
    ctx.gpr[2] = static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, name)));
    if (ctx.gpr[2] == 0u) return;
    GuestFrame list(ctx, scratch_bytes(word(rt, kTable + 8u)));
    const u32 count = call(rt, ctx, 0x08ADB39Cu, {list.at(0x0u)});
    if (static_cast<s32>(count) <= 0) return;
    u32 cursor = list.at(0x0u);
    for (u32 left = count; left != 0u; --left, cursor += 4u) {
        const u32 entry = word(rt, cursor);
        ctx.gpr[2] = word(rt, entry + 0x70u);
        if (ctx.gpr[2] != 0u) {
            const u32 added = call(rt, ctx, 0x08ADD2C4u, {lock, entry, word(rt, self + 0x18u)});
            put(rt, kTable + 0x40u, word(rt, kTable + 0x40u) + added);
        }
    }
}

// 0x08ADD504
void func_08ADD504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    constexpr u32 kTable = 0x08D40628u;
    constexpr u32 kLock = 0x08D40654u;
    call(rt, ctx, 0x08ADD2ACu, {kLock});
    const u32 capacity = word(rt, kTable + 8u);
    put(rt, kTable + 0x40u, 0u);
    put(rt, kTable + 0x44u, 0u);
    GuestFrame list(ctx, scratch_bytes(capacity));
    const u32 count = call(rt, ctx, 0x08ADB39Cu, {list.at(0x0u)});
    if (static_cast<s32>(count) <= 0) return;
    u32 cursor = list.at(0x0u);
    for (u32 left = count; left != 0u; --left, cursor += 4u) {
        const u32 entry = word(rt, cursor);
        call(rt, ctx, 0x08ADD41Cu, {entry});
        ctx.gpr[2] = word(rt, entry + 0x70u);
        if (ctx.gpr[2] != 0u) {
            const u32 added = call(rt, ctx, 0x08ADD2C4u, {kLock, entry, 0x08C90E14u});
            put(rt, kTable + 0x44u, word(rt, kTable + 0x44u) + added);
        }
    }
}

// 0x08AE2088
void func_08AE2088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[4];
    call(rt, ctx, 0x08AEB808u, {flag});
    GuestFrame list(ctx, scratch_bytes(word(rt, 0x08D4069Cu)));
    const u32 count = call(rt, ctx, 0x08ADE268u, {list.at(0x0u)});
    call(rt, ctx, 0x08AEB82Cu, {list.at(0x0u)});
    if (static_cast<s32>(count) <= 0) return;
    u32 cursor = list.at(0x0u);
    for (u32 left = count; left != 0u; --left, cursor += 4u) {
        const u32 entry = word(rt, cursor);
        if (flag != 0u && signed_half_at(rt, entry + 0x12u) >= 0) {
            const u32 name = word(rt, entry + 0x94u);
            if (name != 0u && static_cast<std::int8_t>(byte_at(rt, name)) != 0)
                set_half(rt, entry + 0x28u, call(rt, ctx, 0x08ADD800u));
        }
        call(rt, ctx, 0x08ADDF88u, {entry, 1u});
    }
}

// 0x08B46D9C
void func_08B46D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    const u32 object_base = self + 0x70u;
    const u32 record = frame.at(0x0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (byte_at(rt, self + 0x8Cu) == 0u) return;
    call(rt, ctx, 0x08B3FF90u, {record});
    const u32 first = call(rt, ctx, 0x08B3F8A0u, {record});
    const u32 second = call(rt, ctx, 0x08B3FA54u, {record});
    const u32 handle = call(rt, ctx, 0x08C7AE28u, {object_base, 0u, first, second & 0xFFFFu, 0x2000u, 0x0007A120u, 0x1Eu, 0u});
    if (static_cast<s32>(handle) < 0) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 status = call(rt, ctx, 0x08C7AE68u, {handle, 0u, flag});
    if (status != 0x80410709u && static_cast<s32>(status) < 0) {
        call(rt, ctx, 0x08C7AE00u, {handle, 0u});
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 object = call_indirect(rt, ctx, word(rt, word(rt, self) + 0xA0u), {self, 0x2400u});
    put(rt, object, handle);
    call(rt, ctx, 0x08B3F8C0u, {object + 4u, object_base});
    call(rt, ctx, 0x08B3F7CCu, {object + 0x1Au, record});
    set_byte(rt, object + 0x30u, 1u);
    call(rt, ctx, 0x08B46904u, {self, object});
    ctx.gpr[2] = word(rt, object);
}

// 0x08B51774: reads tokens until a closing '>' (0x3E), recursing at each '<' (0x3C).
void func_08B51774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 text = frame.at(0x1u);
    const u32 text_end = frame.at(0x7u);
    const u32 slot = frame.at(0x0u);
    u32 cursor = text;
    do {
        const u32 token = call(rt, ctx, 0x08B51F38u, {word(rt, self + 0xC20u)});
        if (token == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 character = byte_at(rt, token + 0xCu) == 0u ? 0u : static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, token + 4u)));
        set_byte(rt, cursor++, character);
        if (character == 0x3Eu) {
            ctx.gpr[2] = 1u;
            return;
        }
        if (character == 0x3Cu) call(rt, ctx, 0x08B51774u, {self});
    } while (cursor != text_end);
    if (call(rt, ctx, 0x08B7C6BCu, {text, 0x08CB5768u, 6u}) == 0u) {
        ctx.gpr[2] = call(rt, ctx, 0x08B51F7Cu, {word(rt, self + 0xC20u), 0x08CB5770u});
        return;
    }
    for (;;) {
        if (call(rt, ctx, 0x08B521A8u, {word(rt, self + 0xC20u), slot}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        std::int8_t character = static_cast<std::int8_t>(byte_at(rt, slot));
        if (character == 0x3C) {
            call(rt, ctx, 0x08B51774u, {self});
            character = static_cast<std::int8_t>(byte_at(rt, slot));
        }
        if (character == 0x3E) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
}

// 0x0888F3E8: a size computed from a series of getters. In the original each result is moved into its register in the delay
// slot of the *next* call, so the names below follow the call order (`result_n` is the n-th call's result).
void func_0888F3E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 argument_1 = ctx.gpr[5];
    const u32 argument_2 = ctx.gpr[6];
    const u32 argument_3 = ctx.gpr[7];
    const u32 result_0 = call(rt, ctx, 0x088DA160u, {argument_1});
    const u32 result_1 = call(rt, ctx, 0x088DAAA4u, {argument_1});
    const u32 result_2 = call(rt, ctx, 0x088D4800u);
    const u32 result_3 = call(rt, ctx, 0x0888F390u);
    const u32 result_4 = call(rt, ctx, 0x08894020u);
    const u32 result_5 = call(rt, ctx, 0x08893DBCu, {argument_2});
    const u32 result_6 = call(rt, ctx, 0x088A1978u);
    const u32 result_7 = call(rt, ctx, 0x088A6C78u);
    const u32 result_8 = call(rt, ctx, 0x088A1954u);
    const u32 result_9 = call(rt, ctx, 0x088D06A4u);
    const u32 result_10 = call(rt, ctx, 0x08898874u, {result_0});
    const u32 result_11 = call(rt, ctx, 0x088975C8u);
    const u32 result_12 = call(rt, ctx, 0x08898230u);
    const u32 linear = (((result_0 + argument_3) << 1) + result_1) << 4;
    const u32 base = linear + (result_2 + result_3) + result_4 + result_5 + result_6;
    const u32 scaled = base + result_0 * (result_9 + (result_7 + result_8)) + result_10;
    ctx.gpr[2] = (scaled + result_1 * (result_11 + result_12) + 0x1A7u) & ~0xFu;
}

} // namespace

} // namespace hot_batch_339
} // namespace gt::game

#define HOT339(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_339::name, gt::ReturnKind::kind)
HOT339(0x08ADD41Cu, func_08ADD41C, Word);
HOT339(0x08ADD504u, func_08ADD504, Word);
HOT339(0x08AE2088u, func_08AE2088, Word);
HOT339(0x08B46D9Cu, func_08B46D9C, Word);
HOT339(0x08B51774u, func_08B51774, Word);
HOT339(0x0888F3E8u, func_0888F3E8, Word);
