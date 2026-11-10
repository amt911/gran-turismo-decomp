// Hot batch 337: functions no route reaches, written as structured C from `tools.disasm` (control flow as ifs and loops,
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
namespace hot_batch_337 {
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

// 0x0885DFF4
void func_0885DFF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xD0u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 argument_a = ctx.gpr[6];
    const u32 argument_b = ctx.gpr[7] & 0xFFu;
    const u32 entry_offset = index * 0x50u + 0x4120u;
    const u32 entry = self + entry_offset;
    u32 v0 = entry_offset;
    if (byte_at(rt, entry + 0x18u) == 0u) {
        const u32 workspace = frame.at(0x0u);
        const u32 pair = frame.at(0x8u);
        const u32 first = frame.at(0x40u);
        const u32 end = frame.at(0xA0u);
        call(rt, ctx, 0x088A79ACu, {workspace, 0u});
        for (u32 part = first; part != end; part += 0x30u) call(rt, ctx, 0x08897B8Cu, {part});
        call(rt, ctx, 0x08866510u, {word(rt, entry + 0x44u), word(rt, entry + 0x48u), pair, workspace, first});
        const u32 resolved = call(rt, ctx, 0x088A1A3Cu, {word(rt, self + 0x5B0u), index, argument_a, argument_b});
        call(rt, ctx, 0x088A6C18u, {workspace, resolved});
        const u32 flag = static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, frame.at(0x12u))));
        call(rt, ctx, 0x088661B8u, {word(rt, entry + 0x44u), word(rt, entry + 0x48u), pair, workspace, flag, first});
        const u32 record = static_cast<u32>(signed_half_at(rt, entry + 0x1Eu));
        call(rt, ctx, 0x0885DD1Cu, {word(rt, word(rt, self + 0x3F50u)) + record * 0x188u, workspace});
        for (u32 part = end - 0x30u;; part -= 0x30u) {
            call(rt, ctx, 0x088978B4u, {part});
            if (part == first) break;
        }
        v0 = call(rt, ctx, 0x088A7548u, {workspace});
    }
    ctx.gpr[2] = v0;
}

// 0x08860740
void func_08860740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xC0u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    const u32 entry_offset = index * 0x50u + 0x4120u;
    const u32 entry = self + entry_offset;
    u32 v0 = entry_offset;
    if (byte_at(rt, entry + 0x18u) == 0u) {
        const u32 record_block = word(rt, word(rt, self + 0x3F50u)) + static_cast<u32>(signed_half_at(rt, entry + 0x1Eu)) * 0x188u;
        const u32 before = word(rt, record_block + 0x28u);
        v0 = call(rt, ctx, 0x0885D478u, {record_block, argument});
        const u32 after = word(rt, record_block + 0x28u);
        if (before != after) {
            const u32 workspace = frame.at(0x0u);
            const u32 pair = frame.at(0x8u);
            const u32 first = frame.at(0x40u);
            const u32 end = frame.at(0xA0u);
            call(rt, ctx, 0x088A79ACu, {workspace, 0u});
            for (u32 part = first; part != end; part += 0x30u) call(rt, ctx, 0x08897B8Cu, {part});
            call(rt, ctx, 0x08866510u, {word(rt, entry + 0x44u), word(rt, entry + 0x48u), pair, workspace, first});
            const u32 flag = static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, frame.at(0x12u))));
            set_byte(rt, frame.at(0x13u), after);
            call(rt, ctx, 0x088661B8u, {word(rt, entry + 0x44u), word(rt, entry + 0x48u), pair, workspace, flag, first});
            call(rt, ctx, 0x08860664u, {self, index, after});
            call(rt, ctx, 0x0885DFE4u, {self, index});
            for (u32 part = end - 0x30u;; part -= 0x30u) {
                call(rt, ctx, 0x088978B4u, {part});
                if (part == first) break;
            }
            v0 = call(rt, ctx, 0x088A7548u, {workspace});
        }
    }
    ctx.gpr[2] = v0;
}

// 0x0887FFBC.
void construct_pair_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D75E08u;
    constexpr u32 kInstance = 0x08D75E10u;
    call(rt, ctx, 0x0886AD04u, {self});
    put(rt, self, 0x08CC11C8u);
    put(rt, self + 0x70u, 0x08CC1204u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x78u});
    call(rt, ctx, 0x08886A60u, {block + 4u});
    call(rt, ctx, 0x0886FA58u, {block + 0x14u});
    call(rt, ctx, 0x08884AC8u, {block + 0x28u});
    const u32 inner = block + 0x50u;
    call(rt, ctx, 0x088710DCu, {block + 0x3Cu});
    call(rt, ctx, 0x0886FC04u, {inner});
    call(rt, ctx, 0x0886FDB0u, {block + 0x64u});
    call(rt, ctx, 0x0886F8BCu, {inner, 1u});
    put(rt, self + 0x74u, block);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CC1270u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    }
    u32 member = word(rt, self + 0x74u);
    put(rt, self + 0x78u, kInstance);
    call(rt, ctx, 0x0886AAA4u, {self, member + 4u});
    member = word(rt, self + 0x74u);
    call(rt, ctx, 0x0886AAA4u, {self, member + 0x14u});
    member = word(rt, self + 0x74u);
    ctx.gpr[2] = call(rt, ctx, 0x0886AAA4u, {self, member + 0x28u});
    member = word(rt, self + 0x74u);
    tail_call(ctx, 0x0886AAA4u, {self, member + 0x3Cu});
}

void func_0887FFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_pair_holder(rt, ctx); }

// 0x08A1511C
void func_08A1511C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 count = ctx.gpr[6];
    const u32 source_pointer = ctx.gpr[7];
    const u32 holder = frame.at(0x8u);
    const u32 element = frame.at(0x4u);
    if (static_cast<s32>(count) > 0) {
        call(rt, ctx, 0x08A14610u, {holder});
        const u32 owner = word(rt, holder);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA9A6Cu});
        const u32 source = word(rt, source_pointer);
        if (source == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA9A08u});
        call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {element, source});
        call(rt, ctx, 0x08A142C8u, {owner, element});
        const u32 text = word(rt, element);
        const u32 header = text - 0xCu;
        if (header != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        call(rt, ctx, 0x08A5B9ECu, {holder});
    }
}

// 0x08B83B50: decodes one 16-byte block of 4-bit predictive samples (28 outputs) and keeps the two previous samples in
// `history`; returns the block's flag byte.
void func_08B83B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 output = ctx.gpr[4];
    const u32 block = ctx.gpr[5];
    const u32 history = ctx.gpr[6];
    const u32 header = byte_at(rt, block);
    const u32 flags = byte_at(rt, block + 1u);
    constexpr u32 kFilters = 0x08CB6A90u;
    const u32 filter = header >> 4;
    const u32 shift = header & 0xFu;
    const s32 coefficient_old = static_cast<std::int8_t>(byte_at(rt, kFilters + filter));
    const s32 coefficient_new = static_cast<std::int8_t>(byte_at(rt, kFilters + filter + 5u));
    s32 previous = signed_half_at(rt, history);
    s32 before_previous = signed_half_at(rt, history + 2u);
    u32 result = 7u;
    if (flags != 7u) {
        auto clamp16 = [](s32 value) { return std::min<s32>(std::max<s32>(value, -0x8000), 0x7FFF); };
        s32 low_sample = 0, high_sample = 0;
        for (u32 index = 0u; index < 14u; ++index) {
            const s32 packed = static_cast<std::int8_t>(byte_at(rt, block + 2u + index));
            const s32 low_part = static_cast<s32>(static_cast<u32>(packed) << 28) >> ((shift + 10u) & 31u);
            const s32 high_part = static_cast<s32>((static_cast<u32>(packed) & ~0xFu) << ((14u - shift) & 31u));
            const u32 mix_low = static_cast<u32>(before_previous) * static_cast<u32>(coefficient_new) +
                                static_cast<u32>(previous) * static_cast<u32>(coefficient_old);
            low_sample = clamp16((low_part + static_cast<s32>(mix_low)) >> 6);
            set_half(rt, output, static_cast<u32>(low_sample));
            output += 2u;
            const u32 mix_high = static_cast<u32>(previous) * static_cast<u32>(coefficient_new) +
                                 static_cast<u32>(low_sample) * static_cast<u32>(coefficient_old);
            high_sample = clamp16((high_part + static_cast<s32>(mix_high)) >> 6);
            set_half(rt, output, static_cast<u32>(high_sample));
            output += 2u;
            before_previous = low_sample;
            previous = high_sample;
        }
        set_half(rt, history + 2u, static_cast<u32>(low_sample));
        set_half(rt, history, static_cast<u32>(high_sample));
        result = flags;
    }
    ctx.gpr[2] = result;
}

// 0x08B86598 and 0x08B99D6C: append `count` copies of `fill` to a reference-counted string (the characters sit behind the
// pointer at `this`, with length at -0xC, capacity at -8 and the share count at -4).
void append_fill(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 length_error, u32 grow, u32 terminator) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 fill = static_cast<u32>(static_cast<std::int8_t>(ctx.gpr[6]));
    if (count != 0u) {
        u32 rep = word(rt, self);
        u32 length = word(rt, rep - 0xCu);
        u32 capacity = 0u;
        if (0x3FFFFFFCu - length < count) {
            ctx.gpr[5] = length;
            call(rt, ctx, 0x08B5ADDCu, {length_error});
            capacity = ctx.gpr[2];
            length = ctx.gpr[5];
        } else {
            capacity = word(rt, rep - 8u);
        }
        const u32 new_length = count + length;
        if (capacity < new_length || static_cast<s32>(word(rt, rep - 4u)) > 0) {
            call(rt, ctx, grow, {self, new_length});
            rep = word(rt, self);
        }
        const u32 end = rep + word(rt, rep - 0xCu);
        if (count == 1u) {
            set_byte(rt, end, fill);
        } else {
            call(rt, ctx, 0x08B36018u, {end, fill, count});
        }
        rep = word(rt, self);
        put(rt, rep - 4u, 0u);
        put(rt, rep - 0xCu, new_length);
        set_byte(rt, rep + new_length, byte_at(rt, terminator));
    }
    ctx.gpr[2] = self;
}
void func_08B86598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    append_fill(rt, ctx, 0x08C97C00u, 0x08B864ECu, 0x08CB7430u);
}
void func_08B99D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    append_fill(rt, ctx, 0x08C9FFB4u, 0x08B951FCu, 0x08CBF760u);
}

// 0x08861168
void func_08861168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 index = ctx.gpr[6];
    const u32 slot = ctx.gpr[7];
    const u32 exclusive = ctx.gpr[8] & 0xFFu;
    u32 flag = exclusive ^ 1u;
    if (slot != 0u) {
        const u32 list = word(rt, self + 0x3F50u);
        const s32 wanted = static_cast<std::int8_t>(byte_at(rt, self + index * 0x50u + 0x4120u + slot + 4u));
        const s32 count = static_cast<s32>(word(rt, list + 4u));
        flag = 0u;
        u32 record = word(rt, list);
        for (s32 position = 1; position <= count; ++position, record += 0x188u) {
            const u32 primary = word(rt, record + 0x2Cu);
            const u32 secondary = word(rt, record + 0x1Cu);
            const u32 chosen = primary != 0xFFFFFFFFu ? primary : secondary;
            if ((primary != 0xFFFFFFFFu || secondary != 0xFFFFFFFFu) && chosen == index && static_cast<u32>(wanted) == secondary) {
                flag = 1u;
                break;
            }
        }
    }
    const u32 object = call(rt, ctx, 0x08B62344u, {0x120u});
    call(rt, ctx, 0x08896A08u, {object, word(rt, self + 0x5F4u), index});
    if (flag != 0u) {
        const u32 first = call(rt, ctx, 0x0889779Cu, {name});
        const u32 second = call(rt, ctx, 0x0889717Cu, {name});
        u32 base = 0u;
        if (second == 0u && exclusive == 0u) base = self + first * 0x268u + 0xD8u;
        call(rt, ctx, 0x08895E74u, {object, word(rt, self + 0x5B8u), base, first});
    } else {
        call(rt, ctx, 0x08895E74u, {object, 0u, 0u, 0xFFFFFFFFu});
    }
    ctx.gpr[2] = object;
}

} // namespace

} // namespace hot_batch_337
} // namespace gt::game

#define HOT337(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_337::name, gt::ReturnKind::kind)
HOT337(0x0885DFF4u, func_0885DFF4, Word);
HOT337(0x08860740u, func_08860740, Word);
HOT337(0x0887FFBCu, func_0887FFBC, Word);
HOT337(0x08A1511Cu, func_08A1511C, Word);
HOT337(0x08B83B50u, func_08B83B50, Word);
HOT337(0x08B86598u, func_08B86598, Word);
HOT337(0x08B99D6Cu, func_08B99D6C, Word);
HOT337(0x08861168u, func_08861168, Word);
