// New batch main 992: functions earlier drafts skipped or mismatched. Structured C written from `tools.disasm`; the
// frame sizes are the prologue's `addiu sp`, the s-registers are left alone (the original restores them), and v0 is the
// asm's on every exit path. Shifts and compares keep the asm's signedness, delay slots run before the call/branch.
// Not verified yet: hand off to the gate (fast + synthetic) and the boot check.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_992 {
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
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
GuestResult call_all(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                     std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_all(rt, ctx, target, arguments, floats).f0;
}

// 0x088638A8 (a0 = object): the whole body is `lw a0, 0x0(a0); j 0x08863630` — the member at +0 goes on to 0x08863630.
void func_088638A8(Runtime &rt, AllegrexContext &ctx) {
    tail_call(ctx, 0x08863630u, {word(rt, ctx.gpr[4])});
}

// 0x0888C2B8 (a0 = object, a1 = value the helpers take): per item of the list behind the +0x10 member (0x08893D14 gives
// the count, 0x08893D38 the item) record the item's three floats plus its signed byte at +0xC and hand that to
// 0x088DAF90 / 0x088E0984 (skipped when that call answers negative). Then 0x088928F4 or 0x08892084 by the +0x1C mode,
// four calls on the +4 member, and for every slot of 0x088D9F80 either a 0x088DA1C8 short or a zero into the +0x34
// table's +0x1558, then 0x088D6358 / 0x088D62FC / 0x088DB2C8 / 0x088D4DB8 pick a stride (0xC wins over 2 or 1) for
// 0x088D31B0 and 0x088D3020. v0 is the last 0x088936A4 call.

// 0x08A760B4 (a0 = a vector-owning object, a1 = the two-word source range): by element count (word +4 - word +0 over
// four) either reserve through 0x08C24FDC / 0x08C254A8 around a 0x08A696E4 / 0x08A69D68 temporary, or, for the count
// of two and up whose first word is not the global 0x08CF6FDC, the same with 0x08A696E4 / 0x08A69D68 once and then
// 0x08A6A23C per element; every path but the two small ones ends in 0x08A75DB8 on a0. A negative count returns 1.
void func_08A760B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 first = word(rt, source);
    s32 count = static_cast<s32>(static_cast<s32>(word(rt, source + 4u) - first) >> 2);

    if (count == 0) {
        const u32 temporary = frame.at(0xCu);
        call(rt, ctx, 0x08A696E4u, {temporary});
        const u32 end = word(rt, self + 4u);
        const u32 begin = word(rt, self);
        const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        if (size < 2u) {
            call(rt, ctx, 0x08C254A8u, {self, end, 1u - size, temporary});
        } else {
            call(rt, ctx, 0x08C24FDCu, {self, begin + 4u, end});
        }
        call(rt, ctx, 0x08A69D68u, {temporary});
        call(rt, ctx, 0x08A6A23Cu, {word(rt, self), 0x08CF6FDCu});
        return;
    }
    if (count == 1) {
        const u32 temporary = frame.at(8u);
        call(rt, ctx, 0x08A696E4u, {temporary});
        const u32 end = word(rt, self + 4u);
        const u32 begin = word(rt, self);
        const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        if (size < 2u) {
            call(rt, ctx, 0x08C254A8u, {self, end, 1u - size, temporary});
        } else {
            call(rt, ctx, 0x08C24FDCu, {self, begin + 4u, end});
        }
        call(rt, ctx, 0x08A69D68u, {temporary});
        call(rt, ctx, 0x08A6A23Cu, {word(rt, self), word(rt, source)});
        return;
    }
    if (count < 2) {
        ctx.gpr[2] = 1u;
        return;
    }

    if (word(rt, first) == word(rt, 0x08CF6FDCu)) {
        const u32 temporary = frame.at(4u);
        call(rt, ctx, 0x08A696E4u, {temporary});
        const u32 end = word(rt, self + 4u);
        const u32 begin = word(rt, self);
        const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        if (static_cast<u32>(count) < size) {
            call(rt, ctx, 0x08C24FDCu, {self, begin + static_cast<u32>(count) * 4u, end});
        } else {
            call(rt, ctx, 0x08C254A8u, {self, end, static_cast<u32>(count) - size, temporary});
        }
        count -= 1;
        call(rt, ctx, 0x08A69D68u, {temporary});
        for (u32 element = 0; element != static_cast<u32>(count); ++element) {
            call(rt, ctx, 0x08A6A23Cu, {word(rt, self) + element * 4u, word(rt, source) + (element + 1u) * 4u});
        }
    } else {
        const u32 temporary = frame.at(0u);
        call(rt, ctx, 0x08A696E4u, {temporary});
        const u32 end = word(rt, self + 4u);
        const u32 begin = word(rt, self);
        const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        const u32 next = static_cast<u32>(count) + 1u;
        if (next < size) {
            call(rt, ctx, 0x08C24FDCu, {self, begin + next * 4u, end});
        } else {
            call(rt, ctx, 0x08C254A8u, {self, end, next - size, temporary});
        }
        call(rt, ctx, 0x08A69D68u, {temporary});
        for (u32 element = 0; element < static_cast<u32>(count); ++element) {
            call(rt, ctx, 0x08A6A23Cu, {word(rt, self) + element * 4u, word(rt, source) + element * 4u});
        }
    }
    call(rt, ctx, 0x08A75DB8u, {self});
}

// 0x088BDEE0 (a0 = object, a1 = something the 0x088C50C4 helper takes): builds the 0x088C50B8 / 0x088C50C4 temporary,
// asks 0x088B1A0C for the 0..3 mode (1 for 2, 2 for 3, else 0) and 0x088BD6BC for a factor, then 0x088BB664 for f24
// around the +0x18 member's floats. The +0x14 member's bytes at +0xD/+0xE and the +8 object's byte +3 gate two blocks
// (0x089468C0 / 0x088BB5C4, then 0x089468C0 with the 0x78 / 0x98 floats), the +2 signed byte (below -0x7E means zero)
// times 0x3E8E38E4 adds onto the +0x10 float, and the global 0x08CE3090 byte picks a 0x08B6441C / 0x08B64494 pair
// that stores 0x40B1C71D at 0x08CE30AC. f0 is the result, clamped to [0x08CE30AC, word(a0+4)+0x1360].

// 0x0897CE9C (a0 = object, a1 = a value, a2 = an owner, a3 = a flags object): builds two temporaries from the type
// objects at 0x08CCE018 / 0x08CCE048 (0x08B1FEB8, 0x08B200F0, 0x08B1F028 and their vtable slot 0x18 with a1/a2), turns
// the flags word at +8 of the first temporary into 0x20/0x40/0x80/0x100, then 0x089AA544 / 0x089AA5A0 / 0x089AA910 and
// either 0x0897C460 or (with 0x80 in a3+0x138) 0. For a present first answer 0x089AA95C fills the rest; when the count at
// +0x28 is set it waits on it, and every one of the +0x12C entries gets 0x08B20078's value, an 0x08A5BC20 allocation
// (0x3C, 4) vetted through 0x089D6378 (vtable 0x08CC9888) and handed to 0x089D5E8C / 0x0897783C / 0x08A5B9EC; then
// 0x00FFFFFF lands at a0+0x17C and v0 = 1. Other exits answer the 0x0897C460 flag.
void func_0897CE9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x280u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 owner = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    const u32 temporary = frame.at(0x18u);
    u32 flags = 0;

    call(rt, ctx, 0x08B1FEB8u, {temporary});
    put(rt, temporary, 0x08CCE018u);
    const u32 object = call(rt, ctx, 0x08B200F0u, {temporary});
    const u32 present = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x18u), {object, value, temporary}) & 0xFFu;
    if (present != 0u) {
        const u32 bits = word(rt, frame.at(0x20u));
        flags = (bits & 0x22u) != 0u ? 0x20u : 0u;
        if ((bits & 0x11u) != 0u) flags |= 0x40u;
        if ((bits & 0x44u) != 0u) flags |= 0x80u;
        if ((bits & 0x88u) != 0u) flags |= 0x100u;
    }

    put(rt, frame.at(0x14u), 0xFFFFFFFFu);
    put(rt, frame.at(0x10u), 0x08CCE048u);
    const u32 other = call(rt, ctx, 0x08B1F028u, {frame.at(0x10u)});
    const u32 found = call_indirect(rt, ctx, word(rt, word(rt, other) + 0x18u), {other, value, frame.at(0x10u)});
    if (found != 0u && call(rt, ctx, 0x089AA544u, {owner}) == 0u) {
        call(rt, ctx, 0x089AA5A0u, {owner});
    }

    u32 kept;
    if (call(rt, ctx, 0x089AA910u, {owner}) != 0u && (word(rt, extra + 0x138u) & 0x80u) != 0u) {
        kept = 0u;
    } else {
        kept = call(rt, ctx, 0x0897C460u, {self, value, owner, 0u, 0u}) != 0u ? 1u : 0u;
    }
    if (present == 0u) {
        ctx.gpr[2] = kept;
        return;
    }

    call(rt, ctx, 0x089AA95Cu, {owner, frame.at(8u), frame.at(0xCu)});
    const u32 wait = word(rt, frame.at(0x28u));
    if (wait != 0u) {
        u32 reached = 1u;
        while (reached != wait) ++reached;
    }
    const u32 total = word(rt, frame.at(0x12Cu));
    if (total == 0u) {
        ctx.gpr[2] = kept;
        return;
    }
    u32 index = 0;
    while (index != total) {
        const u32 allocated = call(rt, ctx, 0x08B20078u, {temporary, index});
        const float first = float_at(rt, frame.at(8u));
        const float second = float_at(rt, frame.at(0xCu));
        const u32 memory = call(rt, ctx, 0x08A5BC20u, {0x3Cu, 4u});
        call_all(rt, ctx, 0x089D6378u, {memory, owner, extra, allocated, flags, 0u}, {first, second});
        put(rt, memory, 0x08CC9888u);
        put(rt, frame.at(0u), memory);
        call(rt, ctx, 0x089D5E8Cu, {frame.at(4u), frame.at(0u)});
        call(rt, ctx, 0x0897783Cu, {self, frame.at(4u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        index += 1;
    }
    put(rt, self + 0x17Cu, 0x00FFFFFFu);
    ctx.gpr[2] = 1u;
}

// 0x08A05214 (a0 = object): nothing when the +0xB4 member is null. Otherwise 0x089DA5C0(a0, sp) must answer, and the
// 0x08A051A4 and 0x08A04F44 float blocks are blended by the float 0x089DA5C0 left at sp(+0): scaled = (1 - t) * first +
// t * second, stored at sp+0x90..0x9C with -1 at sp+0xA0. The +0x7C mode then picks the path: 0 calls the +0xB4 object's
// vtable slot 0x3C with the 0x08A0F8C8 answer and sp+0x90; 1 and 2 first run two more vtable calls (slot 0x38 into
// sp+0x50 / sp+0x10, slot 0x3C last) with the blended floats added (1) or multiplied (2) into sp+0x70 / sp+0x30 and -1
// after them; any other mode answers 2. v0 is the last call's.
void func_08A05214(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xE0u);
    const u32 self = ctx.gpr[4];
    if (word(rt, self + 0xB4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call(rt, ctx, 0x089DA5C0u, {self, frame.at(0u)}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 first_block = call(rt, ctx, 0x08A051A4u, {self});
    const float blend = 1.0f - float_at(rt, frame.at(0u));
    float alpha = psp_mul(blend, float_at(rt, first_block + 0xCu));
    float x = psp_mul(blend, float_at(rt, first_block));
    float y = psp_mul(blend, float_at(rt, first_block + 4u));
    float z = psp_mul(blend, float_at(rt, first_block + 8u));

    const u32 second_block = call(rt, ctx, 0x08A04F44u, {self});
    const float weight = float_at(rt, frame.at(0u));
    alpha = alpha + psp_mul(weight, float_at(rt, second_block + 0xCu));
    x = x + psp_mul(weight, float_at(rt, second_block));
    y = y + psp_mul(weight, float_at(rt, second_block + 4u));
    z = z + psp_mul(weight, float_at(rt, second_block + 8u));

    put_float(rt, frame.at(0x90u), x);
    put_float(rt, frame.at(0x94u), y);
    put_float(rt, frame.at(0x98u), z);
    put_float(rt, frame.at(0x9Cu), alpha);
    put(rt, frame.at(0xA0u), 0xFFFFFFFFu);

    const u32 mode = word(rt, self + 0x7Cu);
    if (mode == 1u) {
        const u32 object = word(rt, self + 0xB4u);
        const u32 last_target = word(rt, word(rt, object) + 0x3Cu);
        const u32 first_answer = call(rt, ctx, 0x08A0F8C8u, {self});
        const u32 second_object = word(rt, self + 0xB4u);
        const u32 filler_target = word(rt, word(rt, second_object) + 0x38u);
        const u32 second_answer = call(rt, ctx, 0x08A0F8C8u, {self});
        call_indirect(rt, ctx, filler_target, {frame.at(0x50u), second_object, second_answer});
        put_float(rt, frame.at(0x70u), float_at(rt, frame.at(0x50u)) + float_at(rt, frame.at(0x90u)));
        put_float(rt, frame.at(0x74u), float_at(rt, frame.at(0x54u)) + float_at(rt, frame.at(0x94u)));
        put_float(rt, frame.at(0x78u), float_at(rt, frame.at(0x58u)) + float_at(rt, frame.at(0x98u)));
        put_float(rt, frame.at(0x7Cu), float_at(rt, frame.at(0x5Cu)) + float_at(rt, frame.at(0x9Cu)));
        put(rt, frame.at(0x80u), 0xFFFFFFFFu);
        ctx.gpr[2] = call_indirect(rt, ctx, last_target, {second_object, first_answer, frame.at(0x70u)});
        return;
    }
    if (mode == 2u) {
        const u32 object = word(rt, self + 0xB4u);
        const u32 last_target = word(rt, word(rt, object) + 0x3Cu);
        const u32 first_answer = call(rt, ctx, 0x08A0F8C8u, {self});
        const u32 second_object = word(rt, self + 0xB4u);
        const u32 filler_target = word(rt, word(rt, second_object) + 0x38u);
        const u32 second_answer = call(rt, ctx, 0x08A0F8C8u, {self});
        call_indirect(rt, ctx, filler_target, {frame.at(0x10u), second_object, second_answer});
        put_float(rt, frame.at(0x30u), psp_mul(float_at(rt, frame.at(0x10u)), float_at(rt, frame.at(0x90u))));
        put_float(rt, frame.at(0x34u), psp_mul(float_at(rt, frame.at(0x14u)), float_at(rt, frame.at(0x94u))));
        put_float(rt, frame.at(0x38u), psp_mul(float_at(rt, frame.at(0x18u)), float_at(rt, frame.at(0x98u))));
        put_float(rt, frame.at(0x3Cu), psp_mul(float_at(rt, frame.at(0x1Cu)), float_at(rt, frame.at(0x9Cu))));
        put(rt, frame.at(0x40u), 0xFFFFFFFFu);
        ctx.gpr[2] = call_indirect(rt, ctx, last_target, {second_object, first_answer, frame.at(0x30u)});
        return;
    }
    if (mode == 0u) {
        const u32 object = word(rt, self + 0xB4u);
        const u32 target = word(rt, word(rt, object) + 0x3Cu);
        const u32 answer = call(rt, ctx, 0x08A0F8C8u, {self});
        ctx.gpr[2] = call_indirect(rt, ctx, target, {object, answer, frame.at(0x90u)});
        return;
    }
    ctx.gpr[2] = 2u;
}

} // namespace

#define NEW992(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_992::name, gt::ReturnKind::kind)
NEW992(0x088638A8u, func_088638A8, Word);
NEW992(0x0897CE9Cu, func_0897CE9C, Word);
NEW992(0x08A05214u, func_08A05214, Word);
NEW992(0x08A760B4u, func_08A760B4, Word);

} // namespace new_batch_main_992
} // namespace gt::game
