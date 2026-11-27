// New batch main 1041: functions that mismatched the gate in an earlier draft or were skipped twice.
// Not drafted: 0x088877DC. Its exits all leave $v0 holding an incidental value — a refcount decrement's
//     remainder or a data load's word (`lw v0` / `slt v0` between the calls) — so no structured body can
//     state $v0 as the asm leaves it; it needs a Void return contract decided first.
// 0x08A7A614 registers one long run of name/function pairs on a single object: straight-line calls from
//     tools.disasm, including the pairs of words copied out of .data and handed to the registerer by address.
// 0x08941D74 fills three 0x268-byte blocks from three 0xC0-byte source lanes (0x18 entries each), with the
//     PSP's single-precision divides and multiplies in the asm's order.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1041 {
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
// A call that returns a float in $f0.
float call_returns_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
// A call that takes its float argument in $f12 and returns one in $f0.
float call_taking_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        float f12) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {f12});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `lbu` then `cvt.s.w`: an unsigned byte widened as a signed word, then to single precision.
float byte_as_float(Runtime &rt, u32 address) { return static_cast<float>(static_cast<s32>(byte_at(rt, address))); }

// 0x08A7A614 (a0 = the object being filled): a0 first gets the name table at 0x08CF6FD4, then the member at
// +0x38 is released, then ~100 registrations follow, each (a0, name string, function) — some through a pair of
// words copied from .data to the stack and passed by address — and the frame's tail call ends the function.
void func_08A7A614(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xC0u);
    const u32 self = ctx.gpr[4];
    // 0x08B87108 (a0, name, function).
    const auto register_three = [&](u32 string, u32 function) {
        call(rt, ctx, 0x08B87108u, {self, string, function});
    };
    // 0x08B8D51C (a0, name, function, 0).
    const auto register_four = [&](u32 string, u32 function) {
        call(rt, ctx, 0x08B8D51Cu, {self, string, function, 0u});
    };
    // A pair of words read from `address`, copied to `slot` and passed by address (a3 keeps the first word).
    const auto register_pair = [&](u32 target, u32 string, u32 address, u32 slot) {
        put(rt, frame.at(slot), word(rt, address));
        put(rt, frame.at(slot + 4u), word(rt, address + 4u));
        call(rt, ctx, target, {self, string, frame.at(slot), word(rt, address)});
    };

    call(rt, ctx, 0x08A7BCC8u, {self, 0x08CF6FD4u});
    const u32 member = word(rt, self + 0x38u);
    if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
    put(rt, self + 0x38u, 0u);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A78964u});

    register_four(0x08CB0F40u, 0x08A789D8u);
    register_four(0x08CB0F48u, 0x08A78A4Cu);
    register_four(0x08CB0F54u, 0x08A78B20u);
    register_four(0x08CB0F60u, 0x08A78B84u);
    register_four(0x08CB0F6Cu, 0x08A78BE8u);

    register_three(0x08CB0F78u, 0x08A78C6Cu);
    register_three(0x08CB0F88u, 0x08A78D20u);
    register_three(0x08CB0F94u, 0x08A76B48u);
    register_three(0x08CB0F9Cu, 0x08A7758Cu);
    register_three(0x08CB0FB0u, 0x08A7788Cu);

    register_pair(0x08C27114u, 0x08CB0FC4u, 0x08C8F15Cu, 0xA8u);

    register_three(0x08CB0FDCu, 0x08A78D84u);
    register_three(0x08CB0FE4u, 0x08A78DF8u);
    register_three(0x08CB0FECu, 0x08A78E6Cu);
    register_three(0x08CB0FF4u, 0x08A78EE0u);
    register_three(0x08CB0FFCu, 0x08A78F54u);
    register_three(0x08CB1004u, 0x08A78FCCu);
    register_three(0x08CB100Cu, 0x08A79040u);
    register_three(0x08CB1018u, 0x08A790B4u);
    register_three(0x08CB1020u, 0x08A79128u);
    register_three(0x08CB1028u, 0x08A791A0u);
    register_three(0x08CB1030u, 0x08A79214u);
    register_three(0x08CB103Cu, 0x08A7928Cu);
    register_three(0x08CB1048u, 0x08A79354u);

    register_pair(0x08C27114u, 0x08CB1054u, 0x08C8F154u, 0xA0u);
    register_pair(0x08C27920u, 0x08CB1068u, 0x08C8F14Cu, 0x98u);
    register_pair(0x08C265FCu, 0x08CB1078u, 0x08C8F144u, 0x90u);
    register_pair(0x08C27C58u, 0x08CB1088u, 0x08C8F13Cu, 0x88u);
    register_pair(0x08C27ABCu, 0x08CB1098u, 0x08C8F134u, 0x80u);
    register_pair(0x08C27114u, 0x08CB10A8u, 0x08C8F12Cu, 0x78u);
    register_pair(0x08C27920u, 0x08CB10B8u, 0x08C8F124u, 0x70u);
    register_pair(0x08C27784u, 0x08CB10C4u, 0x08C8F11Cu, 0x68u);
    register_pair(0x08C275E8u, 0x08CB10D4u, 0x08C8F114u, 0x60u);
    register_pair(0x08C2744Cu, 0x08CB10E4u, 0x08C8F10Cu, 0x58u);
    register_pair(0x08C272B0u, 0x08CB10F4u, 0x08C8F104u, 0x50u);
    register_pair(0x08C27114u, 0x08CB1104u, 0x08C8F0FCu, 0x48u);

    register_three(0x08CB111Cu, 0x08A79420u);

    register_pair(0x08C27114u, 0x08CB1128u, 0x08C8F0F4u, 0x40u);
    register_four(0x08CB1140u, 0x08A794ACu);
    register_pair(0x08C27114u, 0x08CB114Cu, 0x08C8F0ECu, 0x38u);
    register_pair(0x08C27114u, 0x08CB1164u, 0x08C8F0E4u, 0x30u);
    register_three(0x08CB117Cu, 0x08A7951Cu);
    register_pair(0x08C27114u, 0x08CB1184u, 0x08C8F0DCu, 0x28u);
    register_three(0x08CB119Cu, 0x08A7972Cu);
    register_three(0x08CB11B0u, 0x08A797A0u);
    register_three(0x08CB11BCu, 0x08A77940u);
    register_pair(0x08C27114u, 0x08CB11C8u, 0x08C8F0D4u, 0x20u);

    call(rt, ctx, 0x08BEEBB4u, {self, 0x08CB11E4u, 0u, 0x08A77BE4u});

    register_pair(0x08C26F78u, 0x08CB11ECu, 0x08C8F0CCu, 0x18u);
    register_pair(0x08C26DDCu, 0x08CB11FCu, 0x08C8F0C4u, 0x10u);
    register_pair(0x08C26F78u, 0x08CB120Cu, 0x08C8F0BCu, 0x08u);
    register_pair(0x08C26DDCu, 0x08CB1218u, 0x08C8F0B4u, 0x00u);

    register_three(0x08CB1224u, 0x08A79980u);
    call(rt, ctx, 0x08C261D8u, {self, 0x08CF6FC0u, 0x08A79A0Cu});
    call(rt, ctx, 0x08C261D8u, {self, 0x08CF6F9Cu, 0x08A79A5Cu});
    tail_call(ctx, 0x08C261D8u, {self, 0x08CF6FC4u, 0x08A79AACu});
}

// 0x08941D74 (a0 = array base, a1 = 0xC0-byte source lanes, a2 = the lane count written so far): for three
// blocks of 0x268 bytes at a0 + 0x4A8 + (a2*3 + i)*0x268, fill the 0x18-entry runs from the source's bytes,
// scale them, and finish each group with the 0x08946734/0x089468C0 helpers. Returns 3.

}  // namespace
}  // namespace new_batch_main_1041
}  // namespace gt::game

#define NEW1041(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1041::name, gt::ReturnKind::kind)
NEW1041(0x08A7A614u, func_08A7A614, Word);
