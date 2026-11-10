// Hot batch 337: functions no route reaches, written as structured C from `tools.disasm` (control flow as ifs and loops,
// named locals; the argument registers are read once at the top and the result register written once at the end) and
// verified by synthetic cases. The commented registrations are functions written the same way at addresses the recompiler
// never emitted as functions ("no recompiled function ... not installed"): they cannot be installed or verified. Leftover values of `v0` that the asm leaves (a `lui` for a guard address, the result of the
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
// The `lui` half of an address whose low half is added as a signed 16-bit immediate.
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// Function-local `static` guard (a byte at `guard`; 0x08B6441C acquires, 0x08B64494 releases). Returns true when the caller
// has to construct the instance and release the guard afterwards.
bool guard_acquire(Runtime &rt, AllegrexContext &ctx, u32 guard) {
    return byte_at(rt, guard) == 0u && call(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
}

// 0x08805578
void func_08805578(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 list = frame.at(0x8u);
    const u32 element = frame.at(0x4u);
    constexpr u32 kHolder = 0x08CE28A0u;
    call(rt, ctx, 0x08A9DD14u, {list});
    call(rt, ctx, 0x08A5BAD8u, {kHolder, word(rt, list)});
    call(rt, ctx, 0x08A5B9ECu, {list});
    const s32 count = static_cast<s32>(call(rt, ctx, 0x08B36278u));
    u32 items = call(rt, ctx, 0x08B36284u);
    if (count > 0) {
        for (s32 index = 0; index < count; ++index, items += 4u) {
            const u32 holder = word(rt, kHolder);
            if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97C9Cu});
            call(rt, ctx, 0x08B85EB8u, {element, word(rt, items), frame.at(0x1u)});
            call(rt, ctx, 0x08A9C650u, {holder, element});
            const u32 text = word(rt, element);
            ctx.gpr[2] = text;
            const u32 header = text - 0xCu;
            if (header != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    ctx.gpr[2] = call(rt, ctx, 0x08A5BCF4u);
}

// 0x0881CAEC
void func_0881CAEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D75128u;
    constexpr u32 kInstance = 0x08D75130u;
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBE450u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x448u});
    put(rt, block + 0x30u, 0u);
    u32 slot = block + 0x354u;
    for (u32 left = 0x1Eu; left != 0u; --left, slot += 8u) call(rt, ctx, 0x088A79ACu, {slot, 0u});
    for (u32 offset = 0u; offset <= 0x2Cu; offset += 4u) put(rt, block + offset, 0u);
    put(rt, self + 0x14u, block);
    u32 v0 = high_half(kInstance);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CBE560u);
        v0 = call(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, self + 0x18u, kInstance);
    ctx.gpr[2] = v0;
}

// 0x0882D758
void func_0882D758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D75160u;
    constexpr u32 kInstance = 0x08D75168u;
    constexpr u32 kName = 0x08CF6ED8u;
    call(rt, ctx, 0x08A2A154u, {self});
    put(rt, self, 0x08CBBBC0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x10u});
    call(rt, ctx, 0x08865D1Cu, {block});
    call(rt, ctx, 0x08A21F8Cu, {block + 8u, kName});
    call(rt, ctx, 0x08835AA8u, {block + 0xCu, kName});
    put(rt, self + 0x18u, block);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CBE908u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, self + 0x1Cu, kInstance);
    ctx.gpr[2] = call(rt, ctx, 0x08860050u, {word(rt, self + 0x18u)});
    put(rt, self + 0x14u, ctx.gpr[2]);
}

// 0x08850264
void func_08850264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 result_slot = ctx.gpr[4];
    const u32 temp = frame.at(0x4u);
    const u32 pending = frame.at(0x0u);
    call(rt, ctx, 0x088547C4u, {temp});
    u32 source = word(rt, temp);
    if (source == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
    const u32 converted = call(rt, ctx, 0x08854678u, {source});
    call(rt, ctx, 0x08A5B9ECu, {temp});
    const u32 owner = call(rt, ctx, 0x088A5D0Cu, {converted});
    const u32 copy = call(rt, ctx, 0x08B2B5E0u, {0x38u});
    // The owner's 0x38 bytes at +0x258 are copied in chunks of four words (read all four, then write them), then two words.
    u32 from = owner + 0x258u;
    u32 to = copy;
    do {
        const u32 w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
        put(rt, to, w0);
        put(rt, to + 4u, w1);
        put(rt, to + 8u, w2);
        put(rt, to + 0xCu, w3);
        from += 0x10u;
        to += 0x10u;
    } while (from != owner + 0x288u);
    const u32 tail_high = word(rt, from + 4u);
    const u32 tail_low = word(rt, from);
    put(rt, to + 4u, tail_high);
    put(rt, to, tail_low);
    call(rt, ctx, 0x08A52AB8u, {pending, word(rt, 0x08CE2CF8u), copy, 0xFFFFFFFFu, 0x0884F734u, 0u});
    if (result_slot != pending) call(rt, ctx, 0x08A5BAD8u, {result_slot, word(rt, pending)});
    call(rt, ctx, 0x08A5B9ECu, {pending});
    const u32 count = word(rt, 0x08CE2CECu) + 1u;
    put(rt, 0x08CE2CECu, count);
    ctx.gpr[2] = count;
}

// 0x0885A6BC
void func_0885A6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 name = frame.at(0x0u);
    const u32 object = frame.at(0x4u);
    const u32 holder = frame.at(0xCu);
    const u32 text = frame.at(0x8u);
    call(rt, ctx, 0x08A69B3Cu, {name, 0x08C9EFD4u});
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call(rt, ctx, 0x08A7E914u, {block, name});
    put(rt, object, block);
    call(rt, ctx, 0x08A76AB4u, {holder, object});
    call(rt, ctx, 0x08A69D68u, {name});
    for (u32 entry = 0x08CDCC80u; entry != 0x08CDCC98u; entry += 8u) {
        const u32 target = word(rt, holder);
        if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9EFE8u});
        call(rt, ctx, 0x08A85B1Cu, {text, word(rt, entry + 4u)});
        call(rt, ctx, 0x08B86E98u, {target, word(rt, entry), text});
        call(rt, ctx, 0x08A5B9ECu, {text});
    }
    const u32 owner = word(rt, self);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9EFE8u});
    call(rt, ctx, 0x08A7CE50u, {owner, holder});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {holder});
}

// 0x0885BA8C
void func_0885BA8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    struct Member { u32 offset, release; };
    static constexpr Member kMembers[] = {{0x7Cu, 0x088D5CF0u}, {0x80u, 0x08894028u}, {0x84u, 0x08893CC0u}, {0x88u, 0x088D435Cu}};
    u32 holder = word(rt, self);
    for (const Member &member : kMembers) {
        const u32 value = word(rt, holder + member.offset);
        if (value != 0u) {
            call(rt, ctx, member.release, {value});
            call(rt, ctx, 0x08B64654u, {value});
            holder = word(rt, self);
        }
    }
    const u32 array = word(rt, holder);
    if (array != 0u) {
        const u32 count = word(rt, array - 4u);
        u32 element = array + count * 0x188u;
        if (array != element) {
            do {
                element -= 0x188u;
                call(rt, ctx, 0x0885D3A0u, {element});
            } while (word(rt, word(rt, self)) != element);
        }
        call(rt, ctx, 0x08B61FC0u, {element - 8u});
        holder = word(rt, self);
    }
    ctx.gpr[2] = call(rt, ctx, 0x08B64654u, {holder});
    tail_call(ctx, 0x08B28DC4u, {self + 8u});
}

// 0x0885C704
void func_0885C704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self + 4u, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 8u});
    const u32 state = call(rt, ctx, 0x08B62344u, {0x138u});
    put(rt, self, state);
    call(rt, ctx, 0x08B36018u, {state, 0u, 0x138u});
    const u32 holder = word(rt, self);
    const u32 table = call(rt, ctx, 0x08B6461Cu, {0x628u});
    const u32 first = table + 8u;
    put(rt, table + 4u, 4u);
    u32 element = first;
    for (u32 left = 4u; left != 0u; --left, element += 0x188u) call(rt, ctx, 0x0885D720u, {element});
    put(rt, holder, first);
    put(rt, word(rt, self) + 0x78u, 0u);
    u32 owner = word(rt, self);
    const u32 handle = call(rt, ctx, 0x08B0979Cu, {element - 0x188u});
    put(rt, owner + 0x8Cu, call(rt, ctx, 0x08B0980Cu, {handle}));
    struct Child { u32 offset, size, init; };
    static constexpr Child kChildren[] = {{0x7Cu, 8u, 0x088D65DCu}, {0x80u, 8u, 0x08894444u}, {0x84u, 8u, 0x08893F7Cu}, {0x88u, 0x88u, 0x088D441Cu}};
    for (const Child &child : kChildren) {
        owner = word(rt, self);
        const u32 block = call(rt, ctx, 0x08B62344u, {child.size});
        call(rt, ctx, child.init, {block});
        put(rt, owner + child.offset, block);
    }
    const u32 state_block = word(rt, self);
    put(rt, state_block + 0x28u, 0xFFFFFFFFu);
    put(rt, state_block + 0x2Cu, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    tail_call(ctx, 0x0885C3C8u, {self});
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
        const u32 car = static_cast<u32>(signed_half_at(rt, entry + 0x1Eu));
        call(rt, ctx, 0x0885DD1Cu, {word(rt, word(rt, self + 0x3F50u)) + car * 0x188u, workspace});
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
        const u32 car_block = word(rt, word(rt, self + 0x3F50u)) + static_cast<u32>(signed_half_at(rt, entry + 0x1Eu)) * 0x188u;
        const u32 before = word(rt, car_block + 0x28u);
        v0 = call(rt, ctx, 0x0885D478u, {car_block, argument});
        const u32 after = word(rt, car_block + 0x28u);
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

// 0x088711E8
void func_088711E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D75360u;
    constexpr u32 kInstance = 0x08D75368u;
    constexpr u32 kEmptyList = 0x08D75298u + 0xCu;
    call(rt, ctx, 0x0887E1E0u, {self});
    put(rt, self, 0x08CBF948u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0xC0u});
    put(rt, block + 0x80u, 0u);
    put(rt, block + 0x84u, 0u);
    put(rt, block + 0x88u, kEmptyList);
    set_byte(rt, block + 0x8Cu, 0u);
    put(rt, block + 0x90u, 0u);
    put(rt, block + 0x94u, 0u);
    for (u32 index = 0u; index < 3u; ++index) put(rt, block + 0x98u + index * 0xCu + 8u, kEmptyList);
    call(rt, ctx, 0x08B4E648u, {block});
    call(rt, ctx, 0x08B4E648u, {block + 0x40u});
    for (u32 index = 0u; index < 3u; ++index) set_byte(rt, block + 0x98u + index * 0xCu, 0u);
    put(rt, self + 0xCu, block);
    u32 v0 = high_half(kInstance);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CBF970u);
        v0 = call(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, self + 0x10u, kInstance);
    ctx.gpr[2] = v0;
}

// 0x0887C6AC
void func_0887C6AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    constexpr u32 kGuard = 0x08D753F8u;
    constexpr u32 kInstance = 0x08D75400u;
    put(rt, self, 0x08CBFE88u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x368u});
    put(rt, block, owner);
    u32 slot = block + 4u;
    for (u32 left = 0x18u; left != 0u; --left, slot += 0xCu) call(rt, ctx, 0x08AC26A4u, {slot + 8u, 0u});
    slot = block + 0x124u;
    for (u32 left = 0x30u; left != 0u; --left, slot += 0xCu) call(rt, ctx, 0x08AC26A4u, {slot + 8u, 0u});
    set_half(rt, block + 0x364u, 0u);
    set_half(rt, block + 0x366u, 0u);
    put(rt, self + 4u, block);
    u32 v0 = high_half(kInstance);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CBFED8u);
        v0 = call(rt, ctx, 0x08B64494u, {kGuard});
    }
    put(rt, self + 8u, kInstance);
    ctx.gpr[2] = v0;
}

// 0x0887DD3C
void func_0887DD3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    constexpr u32 kEmptyList = 0x08D75298u + 0xCu;
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x28u});
    put(rt, block, 0x08CC0F88u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, kEmptyList);
    put(rt, block + 0x10u, kEmptyList);
    for (u32 offset = 0x14u; offset < 0x24u; offset += 4u) put(rt, block + offset, 0u);
    set_byte(rt, block + 0x24u, 0u);
    set_byte(rt, block + 0x25u, 0u);
    put(rt, block + 0x24u, word(rt, block + 0x24u) & ~0x10000u);
    put(rt, self + 0x58u, block);
    const u32 node = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, node + 4u, 1u);
    put(rt, node + 0xCu, block);
    put(rt, node, 0x08CC0FC8u);
    put(rt, node + 8u, 1u);
    put(rt, self + 0x5Cu, node);
    ctx.gpr[2] = call(rt, ctx, 0x08B8E380u, {self + 0x5Cu, block, block, 1u});
    put(rt, self + 0x60u, argument);
}

// 0x0887F600
void func_0887F600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D75DF8u;
    constexpr u32 kInstance = 0x08D75E00u;
    call(rt, ctx, 0x0886AD04u, {self});
    put(rt, self, 0x08CC1130u);
    put(rt, self + 0x70u, 0x08CC116Cu);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x54u});
    call(rt, ctx, 0x08884420u, {block});
    call(rt, ctx, 0x0886FC04u, {block + 0x14u});
    call(rt, ctx, 0x088710DCu, {block + 0x28u});
    call(rt, ctx, 0x08884AC8u, {block + 0x3Cu});
    put(rt, self + 0x74u, block);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CC1198u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    }
    u32 member = word(rt, self + 0x74u);
    put(rt, self + 0x78u, kInstance);
    call(rt, ctx, 0x0886AAA4u, {self, member + 0x3Cu});
    member = word(rt, self + 0x74u);
    ctx.gpr[2] = call(rt, ctx, 0x0886AAA4u, {self, member});
    member = word(rt, self + 0x74u);
    tail_call(ctx, 0x0886AAA4u, {self, member + 0x28u});
}

// 0x0887FFBC and 0x08880330 are the same code in two translation units.
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
void func_08880330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_pair_holder(rt, ctx); }

// 0x088F5594
void func_088F5594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    constexpr u32 kGuard = 0x08D76128u;
    constexpr u32 kInstance = 0x08D76130u;
    const u32 block = call(rt, ctx, 0x08B62344u, {0x828u});
    const u32 head = block + 0x408u;
    const u32 tail_node = block + 0x810u;
    put(rt, block + 0x404u, 0u);
    put(rt, head + 0x404u, 0u);
    put(rt, tail_node, 0x08CC3DF0u);
    put(rt, head, 0x08CC3BA8u);
    put(rt, block, 0x08CC3BA8u);
    put(rt, tail_node + 0xCu, 0x12345678u);
    put(rt, tail_node + 4u, head);
    put(rt, tail_node + 0x10u, 0u);
    put(rt, tail_node + 8u, block);
    set_byte(rt, block + 0x824u, 1u);
    put(rt, self, block);
    u32 v0 = static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, kGuard)));
    if (v0 == 0u) {
        v0 = call(rt, ctx, 0x08B6441Cu, {kGuard});
        if (v0 != 0u) {
            put(rt, kInstance, 0x08CC3E20u);
            v0 = call(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    put(rt, self + 4u, kInstance);
    ctx.gpr[2] = v0;
    tail_call(ctx, 0x088F574Cu, {});
}

// 0x088F9888
void func_088F9888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    constexpr u32 kGuard = 0x08D76140u;
    constexpr u32 kInstance = 0x08D7614Cu;
    put(rt, self, 0x08CC3F08u);
    put(rt, self + 4u, 0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3FD8u);
    put(rt, block + 4u, 0x08CC4004u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    set_byte(rt, block + 0x14u, 0u);
    put(rt, self + 8u, block);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CC4090u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    }
    const u32 holder = word(rt, self + 8u);
    put(rt, self + 0xCu, kInstance);
    put(rt, holder + 0x10u, argument);
    ctx.gpr[2] = holder;
}

// 0x088F9A58
void func_088F9A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    constexpr u32 kGuard = 0x08D76138u;
    constexpr u32 kInstance = 0x08D76148u;
    put(rt, self, 0x08CC3F38u);
    put(rt, self + 4u, 0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x4Cu});
    put(rt, block, 0x08CC3F68u);
    put(rt, block + 4u, 0x08CC3FA0u);
    put(rt, block + 0x18u, 0x08CC3FBCu);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    set_byte(rt, block + 0x14u, 0u);
    for (u32 index = 0u; index < 3u; ++index)
        for (u32 word_index = 0u; word_index < 3u; ++word_index) put(rt, block + 0x1Cu + index * 0xCu + word_index * 4u, 0u);
    call(rt, ctx, 0x08B1C2BCu, {block + 0x40u});
    put(rt, self + 8u, block);
    if (guard_acquire(rt, ctx, kGuard)) {
        put(rt, kInstance, 0x08CC4060u);
        call(rt, ctx, 0x08B64494u, {kGuard});
    }
    const u32 holder = word(rt, self + 8u);
    put(rt, self + 0xCu, kInstance);
    put(rt, holder + 0x10u, argument);
    ctx.gpr[2] = holder;
}

// 0x088B31D0
void func_088B31D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC30A0u);
    put(rt, self + 4u, 0x08CC30B8u);
    const auto release = [&](u32 buffer) {
        if (buffer != 0u) call(rt, ctx, 0x08B61FC0u, {buffer});
    };
    // Four owned buffers, each slot cleared before the next one is released (+0x318, +0x310, +0x2F0, +0x2E4).
    u32 buffer = word(rt, self + 0x318u);
    release(buffer);
    put(rt, self + 0x31Cu, 0u);
    buffer = word(rt, self + 0x310u);
    put(rt, self + 0x318u, 0u);
    release(buffer);
    put(rt, self + 0x314u, 0u);
    buffer = word(rt, self + 0x2F0u);
    put(rt, self + 0x310u, 0u);
    release(buffer);
    put(rt, self + 0x2F4u, 0u);
    buffer = word(rt, self + 0x2E4u);
    put(rt, self + 0x2F0u, 0u);
    release(buffer);
    put(rt, self + 0x2E8u, 0u);
    const u32 array_slot = self + 0x2DCu;
    const u32 array = word(rt, array_slot);
    put(rt, self + 0x2E4u, 0u);
    if (array != 0u) {
        const u32 count = word(rt, array - 4u);
        u32 element = array + (count << 10);
        if (array != element) {
            u32 current = element - 0x400u;
            for (;;) {
                const u32 destructor = word(rt, word(rt, current));
                element = current;
                current -= 0x400u;
                call_indirect(rt, ctx, destructor, {element});
                if (word(rt, array_slot) == element) break;
            }
        }
        call(rt, ctx, 0x08B61FC0u, {element - 4u});
    }
    put(rt, array_slot + 4u, 0u);
    put(rt, array_slot, 0u);
    ctx.gpr[2] = call(rt, ctx, 0x08AF20E8u, {self + 4u});
    put(rt, self, 0x08CC31A0u);
}

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
        u32 car = word(rt, list);
        for (s32 position = 1; position <= count; ++position, car += 0x188u) {
            const u32 primary = word(rt, car + 0x2Cu);
            const u32 secondary = word(rt, car + 0x1Cu);
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
HOT337(0x08805578u, func_08805578, Word);
// HOT337(0x0881CAECu, func_0881CAEC, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0882D758u, func_0882D758, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x08850264u, func_08850264, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0885A6BCu, func_0885A6BC, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0885BA8Cu, func_0885BA8C, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0885C704u, func_0885C704, Word);  // no recompiled function at this address: not installed, so not verifiable
HOT337(0x0885DFF4u, func_0885DFF4, Word);
HOT337(0x08860740u, func_08860740, Word);
// HOT337(0x088711E8u, func_088711E8, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0887C6ACu, func_0887C6AC, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0887DD3Cu, func_0887DD3C, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x0887F600u, func_0887F600, Word);  // no recompiled function at this address: not installed, so not verifiable
HOT337(0x0887FFBCu, func_0887FFBC, Word);
// HOT337(0x08880330u, func_08880330, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x088F5594u, func_088F5594, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x088F9888u, func_088F9888, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x088F9A58u, func_088F9A58, Word);  // no recompiled function at this address: not installed, so not verifiable
// HOT337(0x088B31D0u, func_088B31D0, Word);  // no recompiled function at this address: not installed, so not verifiable
HOT337(0x08A1511Cu, func_08A1511C, Word);
HOT337(0x08B83B50u, func_08B83B50, Word);
HOT337(0x08B86598u, func_08B86598, Word);
HOT337(0x08B99D6Cu, func_08B99D6C, Word);
HOT337(0x08861168u, func_08861168, Word);
