// New batch main-1033 (main session draft, unverified): functions earlier drafts skipped as too hard or that
// mismatched the original. Written by hand from `tools.disasm` as structured C.
// Skipped (not registered): 0x08B2531C and 0x08C5E4B0. Both are ~410-instruction machines whose blocks re-enter each
// other through `j` into the middle of another loop (0x08B2531C twice per bit-buffer refill, 0x08C5E4B0 through
// eleven shared tails), so a draft would have to guess the state a re-entry expects instead of translating it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1033 {
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
// A callee that answers 64 bits: v0 is the low word, v1 the high one (func_088D9C70).
GuestResult call64(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0` (the target's own address is in v0 for its prologue).
GuestResult vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    return call_indirect(rt, ctx, word(rt, table + slot), arguments, {});
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The tally of races that ended after their deadline (bumped at 0x088E0D54, cleared at 0x088E0D84).
constexpr u32 kLateFinishes = 0x08CE9C34u;

// 0x088E0CB8 (race): the race's shut-down. func_088DA0BC says it is not over yet (or +0x5C1 is clear) and the clock
// func_088D9C70 is still inside the race's window at (+0x130)+0x28 / +0x2C: the late-finish tally goes up and that
// count is the answer. Otherwise: +0x5D0 released, the clock stored at +0xB8 / +0xBC, +0x2C set to -1 and the flags
// +0x45 / +0x50 / +0x26 / +0x1F / +0x5B9 cleared, the world +0x148, camera +0x14C, warner +0x134 and sound +0x154
// shut down, the observer +0x5C8 and viewer +0x630 told (their slots), the mixer +0x5CC closed, one byte per car
// (+0x78..) from func_088D4D14, then the session +0x640 is fed its results (func_088E4360 with codes 0xD, 0x12, 0x14,
// 0x16, 0xC, 0x4F and the deadline minus now as 64 bits through 0x4E) and finishes; +0x24 becomes 1 and +0x27 the
// "not in state 2" flag. Last the race's counters at (+0x130)+0x30..0x34 / +0x13 / +0xF are cleared, (+0x130)+0x14 is
// bumped, and +0x5C1 / +0x23 / +0x22 are cleared; the answer is that bumped counter.
void func_088E0CB8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 body = self + 4u;  // s2: the object every func_088D4C.. / func_088D6.. call gets
    GuestFrame frame(ctx, 0x20u);

    if (call(rt, ctx, 0x088DA0BCu, {self}) != 0u) return;  // v0 = its answer
    if (byte_u(rt, self + 0x5C1u) != 0u) {
        if (call(rt, ctx, 0x088DA0B4u, {self}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }

    u32 table = word(rt, self + 0x130u);
    if ((word(rt, table + 0x28u) | word(rt, table + 0x2Cu)) != 0u) {
        const GuestResult now = call64(rt, ctx, 0x088D9C70u, {self});
        table = word(rt, self + 0x130u);
        const u32 due_hi = word(rt, table + 0x2Cu);
        bool early = now.v1 < due_hi;  // sltu a1, v1
        if (!early && now.v1 == due_hi) early = now.v0 < word(rt, table + 0x28u);
        if (early) {
            const u32 late = word(rt, kLateFinishes) + 1u;
            put(rt, kLateFinishes, late);
            ctx.gpr[2] = late;
            return;
        }
    }

    (void)call(rt, ctx, 0x088DB8DCu, {self});
    (void)call(rt, ctx, 0x088E0C50u, {self});
    (void)call(rt, ctx, 0x08B03A48u, {word(rt, self + 0x5D0u)});
    put(rt, kLateFinishes, 0u);
    const u32 timing = word(rt, self + 0x618u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    (void)call(rt, ctx, 0x088D1F14u, {timing});

    const GuestResult clock = call64(rt, ctx, 0x088D9C70u, {self});
    put(rt, self + 0xB8u, clock.v0);
    put(rt, self + 0xBCu, clock.v1);
    put(rt, self + 0x2Cu, 0xFFFFFFFFu);
    put8(rt, self + 0x45u, 0u);
    put8(rt, self + 0x50u, 0u);
    put8(rt, self + 0x26u, 0u);  // the delay slot of the next call
    (void)call(rt, ctx, 0x088DC1B8u, {self, 0u});

    put8(rt, self + 0x1Fu, 0u);
    put8(rt, self + 0x5B9u, 0u);
    const u32 opened = call(rt, ctx, 0x088D4C6Cu, {body});
    (void)call(rt, ctx, 0x088FD974u, {opened});

    const u32 world = word(rt, self + 0x148u);
    (void)vcall(rt, ctx, world, 0x34u, {world});

    if (call(rt, ctx, 0x088D62FCu, {body}) == 2u) {
        const u32 context = word(rt, self + 0x14u);
        const u32 entry = call(rt, ctx, 0x088D4C6Cu, {body});
        (void)call(rt, ctx, 0x088FE528u, {context, entry});
        const u32 observer = word(rt, self + 0x5C8u);
        if (observer != 0u) {
            const u32 slot = word(rt, word(rt, observer) + 0x34u);
            const u32 a = call(rt, ctx, 0x088FE6A0u, {word(rt, self + 0x14u)});
            const u32 b = call(rt, ctx, 0x088FE3A4u, {word(rt, self + 0x14u)});
            (void)call(rt, ctx, slot, {observer, a, b});
        }
        const u32 progress = call(rt, ctx, 0x088DA4B0u, {self});
        if (static_cast<s32>(call(rt, ctx, 0x088FD9D0u, {progress})) > 0) {
            if (call(rt, ctx, 0x088D4C50u, {body}) == 0u) (void)call(rt, ctx, 0x088D5A14u, {body, 9u});
        }
    }

    const u32 session = word(rt, self + 0x628u);
    if (session != 0u) {
        (void)call(rt, ctx, 0x088CEF38u, {session, 0u});
        const u32 value = call(rt, ctx, 0x088D4C50u, {body});
        (void)call(rt, ctx, 0x088CEF50u, {word(rt, self + 0x628u), value});
        (void)call(rt, ctx, 0x088CEF58u, {word(rt, self + 0x628u), value});
        if (call(rt, ctx, 0x088CEF48u, {word(rt, self + 0x628u)}) != 0u) {
            const u32 stream = call(rt, ctx, 0x088CA548u, {});
            (void)call(rt, ctx, 0x088C97A4u, {stream, 0u, 0u});
        } else {
            const u32 stream = call(rt, ctx, 0x088CA548u, {});
            (void)call(rt, ctx, 0x088C97A4u, {stream, 0u, 1u});
        }
        (void)call(rt, ctx, 0x088CFDC8u, {word(rt, self + 0x628u)});
    } else {
        const u32 stream = call(rt, ctx, 0x088CA548u, {0u});
        (void)call(rt, ctx, 0x088C97A4u, {stream, 0u, 0u});
    }

    const u32 camera = word(rt, self + 0x14Cu);
    (void)vcall(rt, ctx, camera, 0x30u, {camera});
    const u32 warner = word(rt, self + 0x134u);
    if (warner != 0u) (void)vcall(rt, ctx, warner, 0x2Cu, {warner});

    (void)call(rt, ctx, 0x088E0A58u, {self});
    const u32 world2 = word(rt, self + 0x148u);
    const u32 world2_slot = word(rt, word(rt, world2) + 0x54u);
    const u32 paused = call(rt, ctx, 0x088D4DD4u, {body});
    (void)call(rt, ctx, world2_slot, {world2, (paused ^ 1u) & 0xFFu});

    if (call(rt, ctx, 0x088D6358u, {body}) == 7u) {
        const u32 first = word(rt, self + 0x148u);
        (void)vcall(rt, ctx, first, 0x54u, {first, 0u});
        const u32 second = word(rt, self + 0x148u);
        (void)vcall(rt, ctx, second, 0x5Cu, {second, 0u});
    }

    // One byte per car from func_088D4D14(body, i) at +0x78 + i; the count is asked again every round and the
    // pointer advances in the loop's back edge (the asm bumps it in the branch's delay slot).
    u32 byte_at = self;
    for (u32 i = 0u; static_cast<s32>(i) < static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self})); ++i) {
        ++byte_at;
        const u32 value = call(rt, ctx, 0x088D4D14u, {body, i});
        put8(rt, byte_at + 0x77u, value);
    }

    (void)call(rt, ctx, 0x088DD7D8u, {self});
    const u32 camera2 = word(rt, self + 0x14Cu);
    (void)vcall(rt, ctx, camera2, 0x10u, {camera2});
    const u32 sound = word(rt, self + 0x154u);
    if (sound != 0u) (void)call(rt, ctx, 0x088F4A90u, {sound});

    const u32 mixer = word(rt, self + 0x5CCu);
    if (mixer != 0u) {
        u32 value = 0u;
        u32 extra = 0u;
        if (call(rt, ctx, 0x088D4C9Cu, {body}) != 0u) {
            (void)call(rt, ctx, 0x088D4DF0u, {body});
            value = 1u;
            const u32 mode = call(rt, ctx, 0x088D4FF0u, {body});
            if (mode != 0u) value = mode;  // movn s0, v0, v0
            extra = mode;
        }
        (void)vcall(rt, ctx, mixer, 0x1Cu, {mixer, value, extra});
    }

    const u32 handler = word(rt, self + 0x134u);
    if (handler != 0u) (void)vcall(rt, ctx, handler, 0x30u, {handler, body});

    const u32 limit = half_u(rt, word(rt, self + 0x130u) + 0xAu);
    if (static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self})) < static_cast<s32>(limit)) {
        if (call(rt, ctx, 0x088D62FCu, {body}) == 0u) {
            const u32 target = word(rt, self + 0x130u);
            put16(rt, target + 0xAu, call(rt, ctx, 0x088DAA8Cu, {self}));
        } else {
            put16(rt, word(rt, self + 0x130u) + 0xAu, 1u);
        }
    }

    if (call(rt, ctx, 0x088D4BFCu, {body}) != 0u) {
        (void)call(rt, ctx, 0x088D7794u, {self, 0xFFFFFFFFu});
        if (word(rt, self + 0x640u) != 0u) {
            // a1 is still the -1 of the call above: the asm never reloads it.
            (void)call(rt, ctx, 0x088DA0CCu, {self, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu});
        } else {
            (void)call(rt, ctx, 0x088DA14Cu, {self});
        }
    } else {
        (void)call(rt, ctx, 0x088DA14Cu, {self});
    }
    (void)call(rt, ctx, 0x088D776Cu, {self, 0u});

    put8(rt, self + 0x24u, 1u);  // the delay slot of the call below
    const u32 state = call(rt, ctx, 0x088D62FCu, {body});
    put8(rt, self + 0x27u, state != 2u ? 1u : 0u);  // xori 2 + sltu
    const u32 viewer = word(rt, self + 0x630u);
    if (viewer != 0u) (void)call(rt, ctx, 0x088E7ABCu, {viewer});

    if (word(rt, self + 0x640u) != 0u) {
        const u32 a = call(rt, ctx, 0x088D5DF8u, {body});
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x0Du, a, 0u});
        const u32 b = call(rt, ctx, 0x088D62FCu, {body});
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x12u, b, 0u});
        const u32 c = call(rt, ctx, 0x088D4DD4u, {body});
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x14u, c, 0u});
        const u32 d = call(rt, ctx, 0x088DA160u, {self});
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x16u, d, static_cast<u32>(static_cast<s32>(d) >> 31)});
        const u32 e = call(rt, ctx, 0x088DA168u, {self});
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x0Cu, e, static_cast<u32>(static_cast<s32>(e) >> 31)});
        const u32 fields = word(rt, self + 0x130u);
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x4Fu, word(rt, fields + 0x48u), word(rt, fields + 0x4Cu)});

        const u32 deadline = word(rt, self + 0x130u);
        const u32 due_lo = word(rt, deadline + 0x30u);
        const u32 due_hi = word(rt, deadline + 0x34u);
        const GuestResult now = call64(rt, ctx, 0x088D9C70u, {self});
        const u32 left_lo = due_lo - now.v0;
        const u32 borrow = due_lo < left_lo ? 1u : 0u;
        const u32 left_hi = due_hi - now.v1 - borrow;
        (void)call(rt, ctx, 0x088E4360u, {word(rt, self + 0x640u), 0x4Eu, left_lo, left_hi});

        if (call(rt, ctx, 0x088D9D50u, {self}) == 0u)
            (void)call(rt, ctx, 0x088E6E14u, {word(rt, self + 0x640u), 2u});
        const u32 flag = byte_u(rt, self + 0x27u) ^ 1u;
        (void)call(rt, ctx, 0x088E4BBCu, {word(rt, self + 0x640u), flag});
        (void)call(rt, ctx, 0x088E4DD4u, {word(rt, self + 0x640u), 0x088DCF58u, self});
        (void)call(rt, ctx, 0x088E4DE8u, {word(rt, self + 0x640u), 0x088DAFECu, self});
    }

    const u32 counters = word(rt, self + 0x130u);
    put(rt, counters + 0x30u, 0u);
    put(rt, counters + 0x34u, 0u);
    put8(rt, word(rt, self + 0x130u) + 0x13u, 0u);
    put8(rt, word(rt, self + 0x130u) + 0xFu, 0u);
    const u32 table_end = word(rt, self + 0x130u);
    const u32 bumped = word(rt, table_end + 0x14u) + 1u;
    put(rt, table_end + 0x14u, bumped);
    put8(rt, self + 0x5C1u, 0u);
    put8(rt, self + 0x23u, 0u);
    put8(rt, self + 0x22u, 0u);
    ctx.gpr[2] = bumped;
}
} // namespace
} // namespace new_batch_main_1033
} // namespace gt::game

#define NEW1033(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1033::name, gt::ReturnKind::kind)
NEW1033(0x088E0CB8u, func_088E0CB8, Word);
