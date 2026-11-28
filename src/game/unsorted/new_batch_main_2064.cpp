// New batch main 2064 (draft only): two functions drafted before that mismatched the original in the gate, rewritten
// by hand from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary, v0 as the original
// leaves it on every exit path, delay slots before the call / branch, callee-saved registers untouched so they come
// back as they went in). Draft batch, not verified: nothing here is claimed done until the differential gate passes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2064 {
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
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 unsigned_byte(Runtime &rt, u32 a) { return rt.memory().load8(a); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The original's atomic shared-count release (`ll`/`sc`, single-threaded here so the `sc` always lands) over a node
// whose counter sits at +4 (destructor pair at vtable +8) and whose second counter sits at +8 (vtable +0xC): the
// vtable's +8 slot only runs when the first counter drops to zero, and +0xC only when the second one does too.
// v0 is the asm's on every exit: 1 from the `addiu v0, zero, 1` in the decrement's branch-delay slot (that is also
// the value `bne a0, v0` compares), or the last virtual call's answer when the chain went that far — the batch-2042
// gate saw the original answer 1 through a count-0 call where the old draft left 0.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 node) {
    const u32 before = word(rt, node + 0x04u);
    put(rt, node + 0x04u, before - 1u);
    ctx.gpr[2] = 1u;
    if (before != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, node) + 0x08u), {node});
    const u32 before_second = word(rt, node + 0x08u);
    put(rt, node + 0x08u, before_second - 1u);
    ctx.gpr[2] = 1u;
    if (before_second != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, node) + 0x0Cu), {node});
}

// 0x08917300 (a0 = container, a1 = count): clears +0/+4/+8 and the bytes +0x18/+0x19 of the container, resets it
// through 08BDC338 (a0 = container, a1 = 0, a2 = count, a3 = the two-word temporary on the frame) and releases the
// node that call stored at frame +0xC. Then it fills the array at container+0 with `count` 8-byte pairs: an object
// from alloc(0x18) (kept from the allocation, not from 08916A50's answer) built by 08916A50, and a 0x10 node
// (vtable 0x08CC5890, counters 1/1, +0xC = the object) handed to 08B8E380 together with the object twice; the pair
// is swapped into place and the node it replaced is released. Each object chains the previous one at +0 and the
// container's +0xC / +0x10 / +0x14 end up pointing at the last object (0 when count <= 0). v0 ends as
// &array[count-1], or as the release chain left it when the loop never ran (`count <= 0`: 1 from the decrement's
// delay slot, the virtual answer when it went further, or 08BDC338's answer when there was no node to release).

// 0x0895E6A8 (self = a0, record = a1, mode = a2): clears self through 0895E63C, then for a non-zero low byte of
// `mode` fills self from the 0x08CDE6xx tables while the global flag 0x08CF35EE is set (signed-half / unsigned-byte
// entries scaled by 0.001 / 0.01 / -0.01 / 0x38D5FEBF, 1.0f at +0x60, the byte flag +0x69), and next, for the
// ordered 9-byte `record` (b0 != 0, b7 < b0, b8 < b1, b5 < b7, b6 < b8, b2 < b1), writes the scaled fields
// +0x18..+0x20 and +0..+0x14 (the +0 / +4 pair zeroed instead when self+0x69 is set now) and closes with the
// three bytes at 0x08CDE612 / 0x08CDE616 / 0x08CDE617 over 100, the byte +0x68 = 1 and v0 = 1.
// v0 on every exit: the `lui v0, %hi(0x08CF35EE)` in the `mode == 0` branch's delay slot (0x08CF0000, the batch-1930
// and batch-2019 gates' original answer where the drafts had 0), the clearing call writes no v0 so that half
// survives until a `sltu`/`lbu` replaces it: the failed order checks answer 0, a record with b0 == 0 answers the last
// `lbu` (0x08CDE6EC) on the flagged path and the lui half on the unflagged one, and the full path ends with v0 = 1.
void func_0895E6A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];

    call(rt, ctx, 0x0895E63Cu, {self, record, mode});
    if ((mode & 0xFFu) == 0u) {
        ctx.gpr[2] = 0x08CF0000u;  // `lui v0, %hi(0x08CF35EE)`, the `beqz`'s delay slot
        return;
    }

    u32 result = 0x08CF0000u;
    if (unsigned_byte(rt, 0x08CF35EEu) != 0u) {
        const float k_thousandth = std::bit_cast<float>(0x3A83126Fu);      // 0.001f
        const float k_hundredth = std::bit_cast<float>(0x3C23D70Au);       // 0.01f
        const float k_neg_hundredth = std::bit_cast<float>(0xBC23D70Au);   // -0.01f
        const float k_tiny = std::bit_cast<float>(0x38D5FEBFu);
        put_float(rt, self + 0x30u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6FEu)), k_thousandth));
        put_float(rt, self + 0x34u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6FCu)), k_thousandth));
        put_float(rt, self + 0x38u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6FAu)), k_thousandth));
        put_float(rt, self + 0x3Cu, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6F8u)), k_thousandth));
        put_float(rt, self + 0x40u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6F6u)), k_thousandth));
        put_float(rt, self + 0x44u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6F4u)), k_thousandth));
        put_float(rt, self + 0x48u, psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6F2u)), k_thousandth));
        put_float(rt, self + 0x4Cu,
                  psp_mul(psp_mul(static_cast<float>(signed_half(rt, 0x08CDE6F0u)), k_thousandth), k_tiny));
        put_float(rt, self + 0x50u, psp_mul(static_cast<float>(unsigned_byte(rt, 0x08CDE6EFu)), k_neg_hundredth));
        put_float(rt, self + 0x54u, psp_mul(static_cast<float>(unsigned_byte(rt, 0x08CDE6EEu)), k_hundredth));
        put_float(rt, self + 0x58u, psp_mul(static_cast<float>(unsigned_byte(rt, 0x08CDE6EDu)), k_hundredth));
        put_float(rt, self + 0x60u, 1.0f);
        put_float(rt, self + 0x5Cu, psp_mul(static_cast<float>(unsigned_byte(rt, 0x08CF35EDu)), k_hundredth));
        put8(rt, self + 0x69u, 1u);
        put_float(rt, self + 0x64u, psp_mul(static_cast<float>(unsigned_byte(rt, 0x08CDE6ECu)), k_hundredth));
        result = unsigned_byte(rt, 0x08CDE6ECu);  // the block's last load into v0
    }

    const u32 b0 = unsigned_byte(rt, record);
    if (b0 == 0u) {
        ctx.gpr[2] = result;
        return;
    }
    if (!(unsigned_byte(rt, record + 7u) < b0)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 b1 = unsigned_byte(rt, record + 1u);
    if (!(unsigned_byte(rt, record + 8u) < b1)) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (!(unsigned_byte(rt, record + 5u) < unsigned_byte(rt, record + 7u))) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (!(unsigned_byte(rt, record + 6u) < unsigned_byte(rt, record + 8u))) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (!(unsigned_byte(rt, record + 2u) < b1)) {
        ctx.gpr[2] = 0u;
        return;
    }

    put_float(rt, self + 0x18u, psp_mul(static_cast<float>(b0), 10000.0f));
    put_float(rt, self + 0x1Cu, psp_mul(static_cast<float>(unsigned_byte(rt, record + 1u)),
                                        std::bit_cast<float>(0x3C23D70Au)));
    put_float(rt, self + 0x20u,
              psp_mul(static_cast<float>(static_cast<s32>(100u - unsigned_byte(rt, record + 2u))),
                      std::bit_cast<float>(0x3C23D70Au)));
    if (unsigned_byte(rt, self + 0x69u) == 0u) {
        put_float(rt, self + 0x00u, psp_mul(static_cast<float>(unsigned_byte(rt, record + 3u)), -10000.0f));
        put_float(rt, self + 0x04u, psp_mul(static_cast<float>(unsigned_byte(rt, record + 4u)),
                                            std::bit_cast<float>(0x3C23D70Au)));
    } else {
        put(rt, self + 0x00u, 0u);
        put(rt, self + 0x04u, 0u);
    }
    put_float(rt, self + 0x08u, psp_mul(static_cast<float>(unsigned_byte(rt, record + 5u)), 10000.0f));
    put_float(rt, self + 0x0Cu, psp_mul(static_cast<float>(unsigned_byte(rt, record + 6u)),
                                        std::bit_cast<float>(0x3C23D70Au)));
    put_float(rt, self + 0x10u, psp_mul(static_cast<float>(unsigned_byte(rt, record + 7u)), 10000.0f));
    put_float(rt, self + 0x14u, psp_mul(static_cast<float>(unsigned_byte(rt, record + 8u)),
                                        std::bit_cast<float>(0x3C23D70Au)));
    const u32 table = 0x08CDE508u;  // `lui v1, %hi(0x08CDE508)` / `addiu v1, v1, %lo(...)`, the bytes at +0x10A..
    put_float(rt, self + 0x24u, static_cast<float>(unsigned_byte(rt, table + 0x10Au)) / 100.0f);
    put8(rt, self + 0x68u, 1u);
    put_float(rt, self + 0x28u, static_cast<float>(unsigned_byte(rt, table + 0x10Eu)) / 100.0f);
    put_float(rt, self + 0x2Cu, static_cast<float>(unsigned_byte(rt, table + 0x10Fu)) / -100.0f);

    ctx.gpr[2] = 1u;
}

}  // namespace
}  // namespace new_batch_main_2064
}  // namespace gt::game

#define NEW2064(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2064::name, gt::ReturnKind::kind)
NEW2064(0x0895E6A8u, func_0895E6A8, Word);
