// New batch, main session 1930: six functions that mismatched or were skipped twice in earlier drafts, redrafted from the asm
// (tools.disasm). Structured C: named locals, G[n] only at the call boundary. Draft only: the differential gate decides.
// 0x08B1E618 is not here (list walk over a global object: its node semantics are not settled from the asm alone).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1930 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A `jal` whose callee takes float arguments in $f12.. (the caller's f12 is passed as the floats).
u32 call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 unsigned_byte(Runtime &rt, u32 a) { return rt.memory().load8(a); }
float f32(u32 bits) { return std::bit_cast<float>(bits); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// An `assert`-style helper in the original: the caller passes the message and it does not return in practice.
void assert_call(Runtime &rt, AllegrexContext &ctx, u32 target, u32 message) { call(rt, ctx, target, {message}); }

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B05084 (187 insns): the object constructor. Fills a block of 0x120 bytes from the allocator with its links and
// defaults, calls its eight sub-initialisers, writes the two global-table pointers and tail-calls the registration with
// `a1` (or 0x60 when the caller passed none). Frame 0x40.
// Stack slots of the original: 0 = (a3 ? a3<<2 : 0x100), 4 = (a3 ? a3 : 0x40), 8 = the caller's a1.

// ---------------------------------------------------------------------------------------------------------------------
// 0x0890FEB0 (208 insns): per-frame update of a camera-like object (self = a0, out = a1, obj = a2 with a vtable).
// The float argument (f12) is kept across the calls. Frame 0x80.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08929104 (184 insns): the big constructor of a renderer-like object (fp = a0). Calls the sub-initialisers, writes
// their vtable-like pointers, then zeroes the flag and array blocks and returns 2 in v0.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08A8E20C (210 insns): the per-object reset: allocates the node blocks, unlinks the two intrusive lists at +0x2C and
// +0x30 (asserts when a head is null), then appends a new node to the vector at +0x44 and resets the three slots.
// Frame 0x50. Stack: 0x04 = node p, 0x08 = new block, 0x0C..0x14 = the callee's output, 0x18..0x20 = a small array.
void func_08A8E20C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 sp = frame.at(0u);
    const u32 sp_04 = frame.at(4u);
    const u32 sp_08 = frame.at(8u);
    const u32 sp_0c = frame.at(0x0Cu);
    const u32 sp_10 = frame.at(0x10u);
    const u32 sp_14 = frame.at(0x14u);
    const u32 sp_18 = frame.at(0x18u);
    const u32 sp_1c = frame.at(0x1Cu);
    const u32 sp_20 = frame.at(0x20u);

    const u32 p = call(rt, ctx, 0x08A5BC20u, {0x34u, 4u});
    call(rt, ctx, 0x08A90FE4u, {p, 0x0Cu});
    put(rt, sp_04, p);
    call(rt, ctx, 0x08A76AB4u, {sp_14, sp_04});

    const u32 q = call(rt, ctx, 0x08A5BC20u, {0x0Cu, 4u});
    if (q + 8u != 0u) call(rt, ctx, 0x08A90480u, {q + 8u, sp_14});
    call(rt, ctx, 0x08B5C078u, {q, self + 0x28u});

    const u32 t = word(rt, self + 0x2Cu);
    if (self + 0x30u != t + 8u) call(rt, ctx, 0x08A5BAD8u, {self + 0x30u, word(rt, t + 8u)});
    u32 s0 = word(rt, self + 0x30u);

    // Null checks of the first list head: a null head asserts, then the node pointer is read from +0x30 of it.
    u32 a0 = s0 != 0u ? s0 + 0x24u : 0x24u;
    if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
    put(rt, sp_18, 0);
    put(rt, sp_1c, 0);
    put(rt, sp_20, 0);
    if (a0 != sp_18) call(rt, ctx, 0x08C2B54Cu, {a0, sp_18});

    call(rt, ctx, 0x08C2AE8Cu, {word(rt, sp_18), word(rt, sp_1c), sp + 1u});
    {
        const u32 w18 = word(rt, sp_18);
        const u32 w20 = word(rt, sp_20);
        const s32 count = static_cast<s32>(w20 - w18) >> 2;
        if (w18 != 0u) call(rt, ctx, 0x08A5BC54u, {w18, static_cast<u32>(count) << 2, 4u});
    }

    // Three list steps, each: head = word(self+0x30); asserts when null; then the entry at +0x30 of it.
    {
        const u32 n = word(rt, self + 0x30u);
        if (n == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
        s0 = word(rt, n + 0x30u);
    }
    if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC8u});
    put(rt, s0 + 0x14u, 0);
    {
        const u32 n = word(rt, self + 0x30u);
        if (n == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
        s0 = word(rt, n + 0x30u);
        if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC8u});
        put(rt, s0 + 0x10u, 0);
    }

    const u32 s1 = word(rt, self + 0x30u);
    if (s1 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC0u});
    call(rt, ctx, 0x08A9DD14u, {sp_0c});

    const u32 w0c = word(rt, sp_0c);
    if (w0c == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D6Cu});
    call(rt, ctx, 0x08A9CD70u, {sp_10, w0c, self + 0x74u});

    const u32 s0f = word(rt, s1 + 0x30u);
    if (s0f == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1CC8u});
    call(rt, ctx, 0x08B855E0u, {s0f + 0x28u, sp_10});

    // Unlink step on the callee's result (the value at sp+0x10).
    const u32 w10 = word(rt, sp_10);
    const u32 g0 = w10 - 0x0Cu;
    if (g0 != 0x08D74FD0u) {
        const u32 r = call(rt, ctx, 0x08B60804u, {w10 - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(r) <= 0) call(rt, ctx, 0x08B8542Cu, {g0, sp});
    }
    call(rt, ctx, 0x08A5B9ECu, {sp_0c});

    // New node: 0x24 bytes, appended to the vector at +0x44 (or grown when end == capacity).
    const u32 block = call(rt, ctx, 0x08B62344u, {0x24u});
    put(rt, block + 0x10u, block + 4u);
    put8(rt, block + 0x21u, 1);
    put(rt, block + 0x14u, 0);
    put(rt, block + 4u, 0);
    put(rt, block + 8u, 0);
    put(rt, block + 0x0Cu, block + 4u);
    put(rt, block + 0x18u, 0);
    put(rt, block + 0x1Cu, 0);
    put8(rt, block + 0x20u, 0);

    const u32 vec = self + 0x44u;
    put(rt, sp_08, block);
    const u32 end = word(rt, vec + 4u);
    if (end == word(rt, vec + 8u)) {
        call(rt, ctx, 0x08C2ABE4u, {word(rt, self + 0x48u), sp_08});
    } else {
        if (end != 0u) put(rt, end, block);
        put(rt, vec + 4u, end + 4u);
    }

    const u32 vend = word(rt, self + 0x48u);
    put(rt, self + 0x50u, word(rt, vend - 4u));
    const u32 q2 = call(rt, ctx, 0x08A5BC20u, {0x0Cu, 4u});
    if (q2 + 8u != 0u) put(rt, q2 + 8u, word(rt, self + 0x50u));
    call(rt, ctx, 0x08B5C078u, {q2, self + 0x54u});

    const u32 q3 = call(rt, ctx, 0x08A5BC20u, {0x0Cu, 4u});
    if (q3 + 8u != 0u) put(rt, q3 + 8u, 0);
    call(rt, ctx, 0x08B5C078u, {q3, self + 0x5Cu});

    const u32 q4 = call(rt, ctx, 0x08A5BC20u, {0x0Cu, 4u});
    if (q4 + 8u != 0u) put(rt, q4 + 8u, 0);
    call(rt, ctx, 0x08B5C078u, {q4, self + 0x64u});

    call(rt, ctx, 0x08A5B9ECu, {sp_14});
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0895E6A8 (228 insns): fills a block of float tuning values from the table at 0x08CDE6xx (when the flag at 0x08CF35EE is
// set), then, for a non-zero input record, the per-field scaled values. Frame 0x10.

}  // namespace
}  // namespace new_batch_main_1930
}  // namespace gt::game

#define MAIN1930(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1930::name, gt::ReturnKind::kind)
MAIN1930(0x08A8E20Cu, func_08A8E20C, Word);
