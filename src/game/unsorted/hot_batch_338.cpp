// Hot batch 338: functions no route reaches, written as structured C from `tools.disasm` (control flow as ifs and loops,
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
namespace hot_batch_338 {
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

// 0x08A018D8
void func_08A018D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 mode = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    const u32 first = frame.at(0x0u);
    const u32 second = frame.at(0x4u);
    if (mode == 0u) {
        call(rt, ctx, 0x08A01338u, {first});
        const u32 target = word(rt, first);
        if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA8B14u});
        call(rt, ctx, 0x089B8BA0u, {target + 0xE8u});
        call(rt, ctx, 0x08A5B9ECu, {first});
    } else if (mode == 1u) {
        call(rt, ctx, 0x08A01338u, {first});
        call(rt, ctx, 0x089AB370u, {second, argument});
        const u32 target = word(rt, first);
        if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA8B14u});
        call_indirect(rt, ctx, word(rt, word(rt, target) + 0x240u), {target, word(rt, second)});
        call(rt, ctx, 0x08A5B9ECu, {second});
        call(rt, ctx, 0x08A5B9ECu, {first});
    } else {
        ctx.gpr[2] = 1u;
    }
}

// 0x089B44E4
void func_089B44E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xA0u);
    const u32 self = ctx.gpr[4];
    const u32 text = frame.at(0x4u);
    const u32 buffer = frame.at(0x8u);
    call(rt, ctx, 0x089B39DCu, {text, self});
    u32 rep = word(rt, text);
    if (word(rt, rep - 0xCu) != 0u) {
        const u32 slash = call(rt, ctx, 0x08BFD224u, {text, 0x2Fu, 0xFFFFFFFFu});
        if (slash != 0xFFFFFFFFu) {
            const u32 length = word(rt, word(rt, text) - 0xCu);
            if (length < slash) call(rt, ctx, 0x08B5AEDCu, {0x08CA56E8u});
            call(rt, ctx, 0x08B8634Cu, {text, slash, length - slash, 0u});
        }
        const u32 formatted = call(rt, ctx, 0x08B49634u, {word(rt, 0x08D69524u)});
        call(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08CA56FCu, word(rt, text), formatted});
        call(rt, ctx, 0x08B48A74u, {self + 0x120u, buffer, 0u, 0xFFFFFFFFu});
        rep = word(rt, text);
    }
    const u32 header = rep - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {rep - 4u, 0xFFFFFFFFu})) <= 0)
        call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
}

// 0x0893156C
void func_0893156C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 table = ctx.gpr[4];
    const u32 modulus = ctx.gpr[5];
    u32 first_out = ctx.gpr[6];
    u32 second_out = ctx.gpr[7];
    const u32 rounds = ctx.gpr[8];
    const u32 mode = ctx.gpr[9];
    const u32 state = frame.at(0x0u);
    if (static_cast<s32>(modulus) <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, state, mode);
    ctx.gpr[2] = 1u;
    if (static_cast<s32>(rounds) <= 0) return;
    const auto next_random = [&]() { return call(rt, ctx, mode == 0u ? 0x08B1ECC8u : 0x08B1ED60u, {state}); };
    for (u32 round = 0u; round != rounds; ++round, first_out += 8u, second_out += 4u) {
        const u32 entry = table + ((next_random() % modulus) << 3);
        const u32 low = word(rt, entry);
        const u32 high = word(rt, entry + 4u);
        put(rt, first_out, low);
        put(rt, first_out + 4u, high);
        const u32 bound = call(rt, ctx, 0x08930458u, {low, high});
        ctx.gpr[2] = 0u;
        if (static_cast<s32>(bound) > 0) ctx.gpr[2] = next_random() % bound;
        put(rt, second_out, ctx.gpr[2]);
    }
    ctx.gpr[2] = 1u;
}

} // namespace

} // namespace hot_batch_338
} // namespace gt::game

#define HOT338(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_338::name, gt::ReturnKind::kind)
HOT338(0x08A018D8u, func_08A018D8, Word);
HOT338(0x089B44E4u, func_089B44E4, Word);
HOT338(0x0893156Cu, func_0893156C, Word);
