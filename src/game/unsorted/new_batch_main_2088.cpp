// New batch main 2088 (draft-only, unverified). Structured C from tools.disasm. Five functions drafted fresh from the asm:
// the rest of the batch was skipped (float loops, ll/sc list walks with struct copies, return values that depend on the
// last call path); they need a second look before a draft.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2088 {
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

void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void store16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

constexpr u32 kOneFloat = 0x3F800000u;  // 1.0f bit pattern

// 0x089DE688: constructor-shaped. Calls 0x0896BDF8(this), sets the vtables, clears three record tables, then two
// 0x089E4BB8 sub-inits, and fills the four slots at +0x150..+0x1B8. v0 = the masked word at +0x140.
void func_089DE688(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self + 0x00u, 0x08CBC738u);
    put(rt, self + 0x38u, 0x08CBC970u);
    put(rt, self + 0xE8u, word(rt, 0x08C8CC04u));
    for (u32 i = 0; i < 2u; ++i) {  // three words each, 0x0C stride, from +0xEC
        const u32 at = self + 0xECu + 0x0Cu * i;
        put(rt, at + 0x0u, 0u);
        put(rt, at + 0x4u, 0u);
        put(rt, at + 0x8u, 0u);
    }
    for (u32 i = 0; i < 2u; ++i) {  // from +0x104
        const u32 at = self + 0x104u + 0x0Cu * i;
        put(rt, at + 0x0u, 0u);
        put(rt, at + 0x4u, 0u);
        put(rt, at + 0x8u, 0u);
    }
    for (u32 i = 0; i < 2u; ++i) {  // from +0x11C
        const u32 at = self + 0x11Cu + 0x0Cu * i;
        put(rt, at + 0x0u, 0u);
        put(rt, at + 0x4u, 0u);
        put(rt, at + 0x8u, 0u);
    }
    put(rt, self + 0x134u, 0x08D74FDCu);
    call(rt, ctx, 0x089E4BB8u, {self + 0x138u});
    call(rt, ctx, 0x089E4BB8u, {self + 0x13Cu});
    // ins v0, zero, 2, 11: clear bits 2..12 of the word at +0x140, after or-ing 3 into it.
    const u32 flags = (word(rt, self + 0x140u) | 3u) & ~0x1FFCu;
    put(rt, self + 0x140u, flags);
    put(rt, self + 0x144u, 0u);
    put(rt, self + 0x150u, kOneFloat);
    put(rt, self + 0x154u, kOneFloat);
    put(rt, self + 0x158u, kOneFloat);
    put(rt, self + 0x15Cu, kOneFloat);
    put(rt, self + 0x160u, 0xFFFFFFFFu);
    put(rt, self + 0x170u, 0u);
    put(rt, self + 0x174u, 0u);
    put(rt, self + 0x178u, 0u);
    put(rt, self + 0x17Cu, kOneFloat);
    put(rt, self + 0x180u, 0xFFFFFFFFu);
    put(rt, self + 0x190u, 0u);
    put(rt, self + 0x194u, 0u);
    put(rt, self + 0x198u, 0u);
    put(rt, self + 0x19Cu, kOneFloat);
    put(rt, self + 0x1A0u, 0xFFFFFFFFu);
    put(rt, self + 0x1B8u, 0u);
    put(rt, self + 0x1B0u, 0u);
    put(rt, self + 0x1BCu, 0u);
    put(rt, self + 0x1B4u, 0u);
    ctx.gpr[2] = flags;
}

// 0x089D0900: constructor-shaped with a tail call. Reads the flags word at +0x14C and the word at +0xE0, sets the
// vtables and the constants, then tail-calls 0x089689C8(this, 1).
void func_089D0900(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08994FD4u, {self});
        put(rt, self + 0x00u, 0x08CC9D60u);
        put(rt, self + 0x38u, 0x08CC9FB8u);
        // ins v1, zero, 1, 6: clear bits 1..6 after or-ing 1.
        const u32 flags = (word(rt, self + 0x14Cu) | 1u) & ~0x7Eu;
        put(rt, self + 0x134u, 0x3E4CCCCDu);  // 0.2f
        put(rt, self + 0x140u, 0x3F000000u);  // 0.5f
        put(rt, self + 0x14Cu, flags);
        put(rt, self + 0x150u, 0x08D74FDCu);
        put(rt, self + 0x130u, 1u);
        put(rt, self + 0x13Cu, kOneFloat);
        put(rt, self + 0x12Cu, 1u);
        put(rt, self + 0x138u, kOneFloat);
        put(rt, self + 0x144u, 0u);
        put(rt, self + 0x148u, 0u);  // the delay slot of the call below
        call(rt, ctx, 0x08969188u, {self + 0x154u, 0x08CF6ED8u});
        put(rt, self + 0x158u, 0x41800000u);  // 16.0f
        put(rt, self + 0x15Cu, 0x41800000u);
        // two 3-word zero records from +0x160
        for (u32 i = 0; i < 2u; ++i) {
            const u32 at = self + 0x160u + 0x0Cu * i;
            put(rt, at + 0x0u, 0u);
            put(rt, at + 0x4u, 0u);
            put(rt, at + 0x8u, 0u);
        }
        put(rt, self + 0x178u, 0u);  // the delay slot of the first call below
        call(rt, ctx, 0x08A76AA0u, {self + 0x17Cu});
        call(rt, ctx, 0x08A76AA0u, {self + 0x180u});
        call(rt, ctx, 0x08A76AA0u, {self + 0x184u});
        call(rt, ctx, 0x08969188u, {self + 0x188u, 0x08CF6ED8u});
        const u32 word_e0 = word(rt, self + 0xE0u);
        put(rt, self + 0x1BCu, 0xFFFFFFFFu);
        // ins v0, zero, 24, 1: clear bit 24.
        put(rt, self + 0xE0u, word_e0 & ~(1u << 24));
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
        store8(rt, self + 0x1C4u, 0u);
    }
    tail_call(ctx, 0x089689C8u, {self, 1u});
}

// 0x08885B90: destructor-shaped. Releases the two owned objects at +0x190 and +0x194 (vtable slot 0x2C), unlinks two
// list nodes at +0x188 and +0x184, drops the reference at +0x6C (and, at zero, its slots 0x8 and 0xC), then points
// the vtable at +0x50 and calls 0x08AF86DC. The reference drops are ll/sc in the original; no other thread runs here.

// 0x08B82764: stores an eight-entry record (index 0..7, 0x40 bytes each, table at 0x08D73CD4). Index out of range,
// or a failing check, answers a status word 0x8044000x; a success writes the record and answers 0.
void func_08B82764(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 index = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flags = ctx.gpr[7];
    if (index >= 8u) {
        ctx.gpr[2] = 0x80440005u;
        return;
    }
    if (0x01800000u < second) {
        ctx.gpr[2] = 0x80440007u;
        return;
    }
    if (call(rt, ctx, 0x08B82B28u, {index, 0u, 0x80440000u, flags}) != 0u) {
        ctx.gpr[2] = 0x80440009u;
        return;
    }
    // flags & 0x400 set: the pointers must be present in the other order (non-null is the failure).
    const bool shared = (flags & 0x400u) != 0u;
    if (shared ? first != 0u : first == 0u) {
        ctx.gpr[2] = 0x80440006u;
        return;
    }
    if (shared ? second != 0u : second == 0u) {
        ctx.gpr[2] = 0x80440007u;
        return;
    }
    const u32 record = 0x08D73CD4u + (index << 6);
    put(rt, record + 0x00u, shared ? 0u : first);
    put(rt, record + 0x04u, shared ? 0u : second);
    put(rt, record + 0x18u, flags);
    put(rt, record + 0x0Cu, 0x1000u);
    put(rt, record + 0x38u, 0x70u);
    put(rt, record + 0x3Cu, 0u);
    put(rt, record + 0x20u, 0u);
    put(rt, record + 0x1Cu, 0u);
    put(rt, record + 0x08u, 0x1000u);
    store16(rt, record + 0x30u, 0u);
    store16(rt, record + 0x32u, 0u);
    put(rt, record + 0x28u, 0u);
    put(rt, record + 0x2Cu, 0u);
    put(rt, record + 0x24u, 0u);
    put(rt, record + 0x10u, 0u);
    put(rt, record + 0x14u, 0u);
    ctx.gpr[2] = 0u;
}

// 0x088E939C: constructor-shaped. Writes the field defaults of a 0x2E8-byte object, calls 0x088D0578 on its +0x200
// sub-object, takes a 0x3C block and wraps it (0x08B62344, then 0x08B28D34 whose answer is stored at +0x2B0), sets
// the constants, and clears three words at +0x2A4..+0x2AC. The answer is -1.

} // namespace
} // namespace new_batch_main_2088
} // namespace gt::game

#define NEW2088(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2088::name, gt::ReturnKind::kind)
NEW2088(0x089DE688u, func_089DE688, Word);
NEW2088(0x089D0900u, func_089D0900, Word);
NEW2088(0x08B82764u, func_08B82764, Word);
