// New batch, main session 136 (draft, unverified): 0x089A81C4, by hand from tools.disasm. Written as structured C; the
// six repeated stack-temporary blocks of the original are one helper whose stack offsets are the original's.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_136 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

constexpr u32 kInitTemp = 0x08A76AA0u;     // called on each stack temporary of the 3-element array
constexpr u32 kMakeTemp = 0x08A76AACu;     // (temp, &value)
constexpr u32 kMakeTempB = 0x08A0FD28u;    // (temp, constant), same role as kMakeTemp for the first two members
constexpr u32 kAssign = 0x08A5BAD8u;       // (dest, *temp)
constexpr u32 kDestroy = 0x08A5B9ECu;      // (temp)
constexpr u32 kNullMember = 0x08A5BB7Cu;   // (text), called when a member is null
constexpr u32 kNullText = 0x08CA4F34u;
constexpr u32 kLockEnter = 0x08B344E0u;
constexpr u32 kLockLeave = 0x08B3457Cu;
constexpr u32 kLockObject = 0x08CF4994u;
constexpr u32 kDefaultValue = 0x08CF6ED8u;

// One block: builds a 3-element array of temporaries at sp+0x68.., stores `value` and `self` through two more
// temporaries (made at slot_base + 4 and slot_base - 4, their argument words at slot_base and slot_base - 8), then calls the
// member at self+member_offset: virtual +0xCC with (member, array, 2, array + 1 element).
void member_block(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 value, u32 member_offset,
                  u32 slot_base) {
    const u32 array = frame.at(0x68u);
    const u32 second = frame.at(0x6Cu);  // s1
    const u32 end = frame.at(0x74u);     // s6
    call(rt, ctx, kInitTemp, {array});
    for (u32 element = second; element != end; element += 4u) call(rt, ctx, kInitTemp, {element});

    const u32 first_temp = frame.at(slot_base + 4u);
    put(rt, frame.at(slot_base), value);
    call(rt, ctx, kMakeTemp, {first_temp, frame.at(slot_base)});
    if (second != first_temp) call(rt, ctx, kAssign, {second, word(rt, first_temp)});
    call(rt, ctx, kDestroy, {first_temp});

    const u32 second_temp = frame.at(slot_base - 4u);
    put(rt, frame.at(slot_base - 8u), self);
    call(rt, ctx, kMakeTemp, {second_temp, frame.at(slot_base - 8u)});
    const u32 last = frame.at(0x70u);
    if (last != second_temp) call(rt, ctx, kAssign, {last, word(rt, second_temp)});
    call(rt, ctx, kDestroy, {second_temp});

    const u32 member = word(rt, self + member_offset);
    if (member == 0u) call(rt, ctx, kNullMember, {kNullText});
    const u32 target = word(rt, word(rt, member) + 0xCCu);
    call(rt, ctx, target, {member, array, 2u, second});

    // destroys the temporaries from the end down to the second element, then the first
    u32 element = end - 4u;
    for (;;) {
        call(rt, ctx, kDestroy, {element});
        if (second == element) break;
        element -= 4u;
    }
    call(rt, ctx, kDestroy, {array});
}

// Resets one member (the temp is made from the default value) and assigns it.
void reset_member(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 member_offset, u32 temp_offset,
                  u32 maker) {
    const u32 temp = frame.at(temp_offset);
    call(rt, ctx, maker, {temp, kDefaultValue});
    const u32 dest = self + member_offset;
    if (dest != temp) call(rt, ctx, kAssign, {dest, word(rt, temp)});
    call(rt, ctx, kDestroy, {temp});
}

// 0x089A81C4 (a0 = object, a1 = a value): when 0x089A717C(a0) answers an object, runs the member blocks for the
// members at +0x2C / +0x30 / +0x34 chosen by the answers of 0x089A6EA4 / 0x089A6F38, resets the six members +0x20..+0x34 to
// the default value, sets +0x38 = 3 and ends with the answer's virtual +0x164 (answer, a1).
void func_089A81C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);

    const u32 answer = call(rt, ctx, 0x089A717Cu, {self});
    if (answer == 0u) return;

    const u32 mode = call(rt, ctx, 0x089A6EA4u, {self});
    const u32 flag = call(rt, ctx, 0x089A6F38u, {self});
    call(rt, ctx, 0x089A6EF0u, {self, 3u});
    call(rt, ctx, 0x089A6F84u, {self, 2u});
    ctx.gpr[2] = 1u;

    if (mode == 0u) {
        if (word(rt, self + 0x2Cu) != 0u) member_block(rt, ctx, frame, self, value, 0x2Cu, 0x60u);
        if (word(rt, self + 0x30u) != 0u) member_block(rt, ctx, frame, self, value, 0x30u, 0x50u);
    } else if (mode == 1u) {
        if (flag == 0u && word(rt, self + 0x2Cu) != 0u) member_block(rt, ctx, frame, self, value, 0x2Cu, 0x40u);
        if (word(rt, self + 0x30u) != 0u) member_block(rt, ctx, frame, self, value, 0x30u, 0x30u);
    }
    if (word(rt, self + 0x34u) != 0u) member_block(rt, ctx, frame, self, value, 0x34u, 0x20u);

    call(rt, ctx, kLockEnter, {kLockObject});
    for (const u32 offset : {0x20u, 0x24u}) {
        const u32 member = word(rt, self + offset);
        if (member != 0u) call(rt, ctx, word(rt, word(rt, member) + 0x1B8u), {member});
    }
    reset_member(rt, ctx, frame, self, 0x20u, 0x14u, kMakeTempB);
    reset_member(rt, ctx, frame, self, 0x24u, 0x10u, kMakeTempB);
    reset_member(rt, ctx, frame, self, 0x28u, 0x0Cu, kMakeTemp);
    reset_member(rt, ctx, frame, self, 0x2Cu, 0x08u, kMakeTemp);
    reset_member(rt, ctx, frame, self, 0x30u, 0x04u, kMakeTemp);
    reset_member(rt, ctx, frame, self, 0x34u, 0x00u, kMakeTemp);
    put(rt, self + 0x38u, 3u);
    call(rt, ctx, kLockLeave, {kLockObject});
    call(rt, ctx, word(rt, word(rt, answer) + 0x164u), {answer, value});
}
} // namespace
} // namespace new_batch_main_136
} // namespace gt::game

#define NEW_MAIN136(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_136::name, gt::ReturnKind::kind)
NEW_MAIN136(0x089A81C4u, func_089A81C4, Word);
