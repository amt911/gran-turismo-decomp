// Main-session batch 1589: draft-only, structured C from tools.disasm (Claude verifies).
// These were skipped twice as too hard (no gate detail): the asm was read line by line, delay slots and the
// v0 of every exit path included. v0 is forced in the comparison, so each function writes it explicitly.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1589 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
void put_byte(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }

// 0x088DAB68 works on 64-bit halves in a0/a1: both words of every helper's result matter.
GuestResult call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}

// 0x08A3E4F8 (a0 = this, a1 = arg): base 0x08A76ACC, vtable 0x08CD13B0, an 0x1C-byte allocation kept at this+0x14,
// then the 0x08D77170 one-time init (byte flag, table 0x08D77178, hook 0x08B64494), this+0x18 = 0x08D77178, and a
// tail call 0x08A3D710(word(this+0x14), a1). v0 at the tail call is the callee's.
void func_08A3E4F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self, arg});
    put(rt, self, 0x08CD13B0u);
    const u32 allocated = call(rt, ctx, 0x08B62344u, {0x1Cu});
    call(rt, ctx, 0x08B0B06Cu, {allocated});
    const u32 inner = self + 0x14u;
    put(rt, inner, allocated);
    if (rt.memory().load8(0x08D77170u) == 0u) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D77170u}) != 0u) {
            put(rt, 0x08D77178u, 0x08CD1E80u);
            call(rt, ctx, 0x08B64494u, {0x08D77170u});
        }
    }
    const u32 member = word(rt, inner);
    put(rt, inner + 4u, 0x08D77178u);
    tail_call(ctx, 0x08A3D710u, {member, arg});
}

// 0x08A560DC (a0 = this, a1, a2, a3, t0): base 0x08A5B828, vtable 0x08CD2100, this+0xC/0x10/0x14 filled from a1/a2/a3;
// when a1 is null and a2 is not, the 0x10-byte block comes from t0(a1 = a2) or 0x08B35840, then 0x08B36018 fills it and
// a still-null this+0x14 becomes 0x08B35254. v0 = 0x08CD2100 on the skipped paths, else the last call's result (0x08B35254
// when this+0x14 was null).
void func_08A560DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A5B828u, {self, a1, a2, a3, t0});
    put(rt, self, 0x08CD2100u);
    put(rt, self + 0x14u, a3);
    put(rt, self + 0xCu, a1);
    put(rt, self + 0x10u, a2);
    u32 result = 0x08CD2100u;
    ctx.gpr[2] = result;
    if (a1 == 0u && a2 != 0u) {
        u32 allocated;
        if (t0 != 0u) {
            allocated = call(rt, ctx, t0, {0x10u, a2});
        } else {
            allocated = call(rt, ctx, 0x08B35840u, {0x10u, a2});
        }
        put(rt, self + 0xCu, allocated);
        result = call(rt, ctx, 0x08B36018u, {allocated, 0u, a2});
        if (word(rt, self + 0x14u) == 0u) {
            put(rt, self + 0x14u, 0x08B35254u);
            result = 0x08B35254u;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08AC1FE0 (a0 = this): vtable 0x08CD6530 first (it sits in the beqz delay slot); when this+0x38 holds a vector, the
// element count is at head-4 and the elements are 0x2C apart, each destroyed through vtable slot 0 from the end, the loop
// continuing while word(this+0x38) differs from the element just destroyed; then 0x08B61FC0(head-4). Bit 0 of the half at
// +0x3E skips the vector and just clears +0x38. Clears the half at +0x3C, calls 0x08AC2680(this+4), vtable 0x08CD6578.
void func_08AC1FE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 head = word(rt, self + 0x38u);
    put(rt, self, 0x08CD6530u);
    if (head != 0u) {
        if ((rt.memory().load16(self + 0x3Eu) & 1u) != 0u) {
            put(rt, self + 0x38u, 0u);
        } else {
            const u32 count = word(rt, head - 4u);
            const u32 end = head + count * 0x2Cu;
            u32 previous = end - 0x2Cu;
            u32 slot = previous;
            u32 last = end;
            if (head != end) {
                do {
                    const u32 vtable = word(rt, slot);
                    last = previous;
                    ctx.gpr[4] = previous;
                    slot -= 0x2Cu;
                    call_indirect(rt, ctx, word(rt, vtable), {previous});
                    previous = last - 0x2Cu;
                } while (word(rt, self + 0x38u) != last);
            }
            call(rt, ctx, 0x08B61FC0u, {last - 4u});
            put(rt, self + 0x38u, 0u);
        }
    }
    rt.memory().store16(self + 0x3Cu, 0u);
    call(rt, ctx, 0x08AC2680u, {self + 4u});
    put(rt, self, 0x08CD6578u);
    ctx.gpr[2] = 0x08CD6578u;
}

// 0x08AD5460 (a0 = this, a1): 0x08AD5394(this, a1), 0x08B10204(sp+0x40, a1, 1) fills 0x3C bytes that are copied down to
// sp; when word(sp) is zero the record is accepted: byte 1 at this+0xC, word(sp+0x10) at this+8, word(sp+0x20) at this+4.
// v0 = 1 accepted, 0 refused.

// 0x08B48398 (a0 = this, a1): allocates 0x44 bytes of the pool at word(this+0x28) (tag 0x10) and clears the 0x40-byte
// record at its base (0x08B3FA98 / 0x08B3F838 on +4 and +0x1A); a second 0x10-tagged block for a1 goes to +0x40.
// v0 = the record base (0 when the first allocation failed).

// 0x08857B24 (a0 = this, a1, a2 = byte): base 0x08A76ACC, vtable 0x08CBD238, an 8-byte object built by 0x088A7B3C(a1, a2)
// at this+0x14, then the 0x08D751E0 one-time init (table 0x08D751E8, 0x08CBF2D8, hook 0x08B64494), this+0x18 = 0x08D751E8.
// v0 = 0x000A0000 when the flag byte is set, else 0x08B6441C's answer (0 leaves it, non-zero gives 0x08B64494's result).

// 0x088A7A74 (a0 = out pair, a1 = byte): builds the object with 0x08B2B3EC(0x4D8)+0x088A73E4 (vtable 0x08CC2BE8) or
// 0x08B62344(0x4D8)+0x088A7310, stores it at *out and the 0x08D75F28 one-time table (0x08D75F30, 0x08CC2C00) at out+4,
// then tail calls 0x088A719C(out).
void func_088A7A74(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    u32 object;
    if (flag != 0u) {
        const u32 allocated = call(rt, ctx, 0x08B2B3ECu, {0x4D8u});
        call(rt, ctx, 0x088A73E4u, {allocated});
        put(rt, allocated, 0x08CC2BE8u);
        object = allocated;
    } else {
        const u32 allocated = call(rt, ctx, 0x08B62344u, {0x4D8u});
        call(rt, ctx, 0x088A7310u, {allocated});
        object = allocated;
    }
    put(rt, out, object);
    if (rt.memory().load8(0x08D75F28u) == 0u) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D75F28u}) != 0u) {
            put(rt, 0x08D75F30u, 0x08CC2C00u);
            call(rt, ctx, 0x08B64494u, {0x08D75F28u});
        }
    }
    put(rt, out + 4u, 0x08D75F30u);
    tail_call(ctx, 0x088A719Cu, {out});
}

// 0x08910DF4 (a0 = object): 0x08910C84(this), then over [this+0x10, this+0x18) in 8-byte steps from the end: the pointer
// at +4 has its count at +4 decremented (ll/sc, as a plain load/store here); when it was 1, vtable slot 8 is called and
// the count at +8 is decremented, and when that was 1, slot 0xC is called. v0 is the sc's 1 after each decrement and the
// virtual's result after each call.
void func_08910DF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 end = self + 0x10u;
    u32 cursor = self + 0x18u;
    call(rt, ctx, 0x08910C84u, {self});
    if (end == 0u) return;
    while (cursor != end) {
        cursor -= 8u;
        const u32 target = word(rt, cursor + 4u);
        if (target == 0u) continue;
        const u32 old = word(rt, target + 4u);
        put(rt, target + 4u, old - 1u);
        ctx.gpr[2] = 1u;
        if (old != 1u) continue;
        ctx.gpr[4] = target;
        call_indirect(rt, ctx, word(rt, word(rt, target) + 8u), {target});
        const u32 old2 = word(rt, target + 8u);
        put(rt, target + 8u, old2 - 1u);
        ctx.gpr[2] = 1u;
        if (old2 != 1u) continue;
        ctx.gpr[4] = target;
        call_indirect(rt, ctx, word(rt, word(rt, target) + 0xCu), {target});
    }
}

// 0x08917C70 (a0 = object): with 0x089277BC(word(this+0xC)) == 3, 0x08927BC8(word(this+0xC), 0x1A) probes the motion
// counter at +0x1FC8 of word(this+0xC) (increment while the flag at +0x1FCD is clear, reset with the flag set at 0x709),
// then the same for the byte at +0x1FCC == 1. v0 = 1 except the final reset path, which answers word(this+0xC).
void func_08917C70(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 first = call(rt, ctx, 0x089277BCu, {word(rt, self + 0xCu)});
    u32 a0;
    if (first != 3u) {
        const u32 object = word(rt, self + 0xCu);
        a0 = rt.memory().load8(object + 0x1FCCu);
    } else {
        const u32 probe = call(rt, ctx, 0x08927BC8u, {word(rt, self + 0xCu), 0x1Au});
        u32 object = word(rt, self + 0xCu);
        u32 counter;
        if (probe == 0u || rt.memory().load8(object + 0x1FCDu) != 0u) {
            counter = word(rt, object + 0x1FC8u);
        } else {
            counter = word(rt, object + 0x1FC8u) + 1u;
            put(rt, object + 0x1FC8u, counter);
            object = word(rt, self + 0xCu);
        }
        if (static_cast<s32>(counter) < 0x709) {
            a0 = rt.memory().load8(object + 0x1FCCu);
        } else {
            put_byte(rt, object + 0x1FCDu, 1u);
            object = word(rt, self + 0xCu);
            put(rt, object + 0x1FC8u, 0u);
            object = word(rt, self + 0xCu);
            a0 = rt.memory().load8(object + 0x1FCCu);
        }
    }
    u32 result = 1u;
    if (a0 == 1u) {
        u32 object = word(rt, self + 0xCu);
        u32 counter;
        if (rt.memory().load8(object + 0x1FCDu) == 0u) {
            counter = word(rt, object + 0x1FC8u) + 1u;
            put(rt, object + 0x1FC8u, counter);
            object = word(rt, self + 0xCu);
            counter = word(rt, object + 0x1FC8u);
        } else {
            counter = word(rt, object + 0x1FC8u);
        }
        if (static_cast<s32>(counter) >= 0x709) {
            put_byte(rt, object + 0x1FCDu, 1u);
            const u32 reset = word(rt, self + 0xCu);
            put(rt, reset + 0x1FC8u, 0u);
            result = word(rt, self + 0xCu);
        }
    }
    ctx.gpr[2] = result;
}

// 0x08919114 (a0 = this, a1 = signed index, f12 = float): s0 = max(a1, -1) (Allegrex MAX, signed); the count from
// 0x08907AA0(word(this+0x1F8)) wraps it modulo count+1 when s0 is past it; then the word at +0x24 of that object gives
// the range: f2 = s0 * float(+0x14) + f12 clamped to [+0x18, +4] with a wrap by -float(+0x14). v0 = word(+0x24).
void func_08919114(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    s32 index = static_cast<s32>(ctx.gpr[5]);
    if (index < -1) index = -1;
    const float addend = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const u32 counted = call(rt, ctx, 0x08907AA0u, {word(rt, self + 0x1F8u)});
    if (static_cast<s32>(counted) < index) {
        const u32 count = call(rt, ctx, 0x08907AA0u, {word(rt, self + 0x1F8u)}) + 1u;
        if (count != 0u) index %= static_cast<s32>(count);
    }
    const u32 object = word(rt, self + 0x1F8u);
    const float scale = float_at(rt, object + 0x14u);
    const u32 range = word(rt, object + 0x24u);
    float value = static_cast<float>(index) * scale;
    const float high = float_at(rt, range + 4u);
    float sum = value + addend;
    if (sum < high) {
        value = high;
    } else {
        value = float_at(rt, object + 0x18u);
        if (!(value < sum)) {
            ctx.fpr[0] = sum;
            ctx.gpr[2] = range;
            return;
        }
        value = high;
    }
    value = scale + value;
    if (value < addend) {
        sum = addend - scale;
    } else {
        sum = addend;
    }
    ctx.fpr[0] = sum;
    ctx.gpr[2] = range;
}

// 0x089FCC98 (a0 = this, a1): base 0x089BB804, vtables 0x08CC7808 / +0x38 = 0x08CC79A4, 0x08B85EB8 into this+0x64 and
// sp+4, 0x08A5D414(this+0x68, 0x08CF6ED8), 0x089FCA70(this, sp+4); word(sp+4)-0xC compared with 0x08D74FD0, else
// 0x08B60804(word(sp+4)-4, -1) and 0x08B8542C(word(sp+4)-0xC, sp) when it answers <= 0.
// v0 = 0x08D74FD0 on the sentinel path, else 0x08B60804's result, else 0x08B8542C's.
void func_089FCC98(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 first = frame.at(4u);
    call(rt, ctx, 0x089BB804u, {self, arg});
    put(rt, self, 0x08CC7808u);
    put(rt, self + 0x38u, 0x08CC79A4u);
    call(rt, ctx, 0x08B85EB8u, {self + 0x64u, arg, frame.at(2u)});
    call(rt, ctx, 0x08A5D414u, {self + 0x68u, 0x08CF6ED8u});
    call(rt, ctx, 0x08B85EB8u, {first, arg, frame.at(1u)});
    call(rt, ctx, 0x089FCA70u, {self, first});
    const u32 value = word(rt, first);
    ctx.gpr[2] = 0x08D74FD0u;
    if (value - 0xCu == 0x08D74FD0u) return;
    const u32 checked = call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(checked) > 0) {
        ctx.gpr[2] = checked;
        return;
    }
    call(rt, ctx, 0x08B8542Cu, {value - 0xCu, frame.at(0u)});
}

// 0x08B59CA4 (a0 = table, a1 = key): the bucket index is the byte at +0x205, entries 16 bytes apart with the key at
// +0xC, the next index at +2 and the value at +8; 0xFF ends the chain. The entry found on a later hop is moved to the
// front through 0x08B59BC0 / 0x08B59B7C(this, this+0x204, ...). v0 = the value at +8, or 0.

// 0x088A2FA4 (a0 = this): vtable 0x08CC27E8; 0x08B61FC0(word(this+0xC)); the object at word(this+8) through vtable slot 4;
// then the vector at this+4, count at head-4 and 0x7C-byte elements destroyed through slot 0 from the end (loop while
// word(this+4) differs from the element just destroyed), 0x08B61FC0(last-4); vtable 0x08CC2990. v0 = 0x08CC2990 always.
void func_088A2FA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC27E8u);
    const u32 first = word(rt, self + 0xCu);
    if (first != 0u) call(rt, ctx, 0x08B61FC0u, {first});
    const u32 second = word(rt, self + 8u);
    if (second != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, second) + 4u), {second});
    }
    const u32 head = word(rt, self + 4u);
    if (head != 0u) {
        const u32 count = word(rt, head - 4u);
        const u32 end = head + count * 0x7Cu;
        u32 previous = end - 0x7Cu;
        u32 slot = previous;
        u32 last = end;
        if (head != end) {
            do {
                const u32 vtable = word(rt, slot);
                last = previous;
                ctx.gpr[4] = previous;
                slot -= 0x7Cu;
                call_indirect(rt, ctx, word(rt, vtable), {previous});
                previous = last - 0x7Cu;
            } while (word(rt, self + 4u) != last);
        }
        call(rt, ctx, 0x08B61FC0u, {last - 4u});
    }
    put(rt, self, 0x08CC2990u);
    ctx.gpr[2] = 0x08CC2990u;
}

// 0x088DAB68: 64-bit arithmetic over helper pairs. 0x088DAAF4 gives the operand; 0x08B6EE4C(x, 0, 0) decides the sign
// (v0 > 0 takes the operand, else x/2 with the low bit folded in through 0x08B6EE80 and 0x08B71328); then 0x08B71394
// with 0x40080000, 0x08B71720 with 0x40490000 and 0x08B6F1B8. v0/v1 = the last pair.
void func_088DAB68(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    GuestResult pair = call_pair(rt, ctx, 0x088DAAF4u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const u32 low = pair.v0;
    const u32 high = pair.v1;
    pair = call_pair(rt, ctx, 0x08B6EE4Cu, {low, high, 0u, 0u});
    const u32 folded = high << 31;
    u32 a0;
    u32 a1;
    if (static_cast<s32>(pair.v0) > 0) {
        pair = call_pair(rt, ctx, 0x08B6EE80u, {low, high});
        a0 = pair.v0;
        a1 = pair.v1;
    } else {
        const u32 half = (low >> 1) | folded;
        const u32 half_high = high >> 1;
        const u32 carry = low & 1u;
        pair = call_pair(rt, ctx, 0x08B6EE80u, {carry | half, half_high});
        pair = call_pair(rt, ctx, 0x08B71328u, {pair.v0, pair.v1, pair.v0, pair.v1});
        a0 = pair.v0;
        a1 = pair.v1;
    }
    pair = call_pair(rt, ctx, 0x08B71394u, {a0, a1, 0u, 0x40080000u});
    pair = call_pair(rt, ctx, 0x08B71720u, {pair.v0, pair.v1, 0u, 0x40490000u});
    pair = call_pair(rt, ctx, 0x08B6F1B8u, {pair.v0, pair.v1});
    ctx.gpr[2] = pair.v0;
    ctx.gpr[3] = pair.v1;
}

// 0x088FA08C (a0 = out pointer, a1 = value): allocates 0x20 bytes, vtable 0x08CC40C0 at +0, clears +4/+8/+0xC/+0x10/
// +0x14/+0x18, stores a1 at +0x1C, *a0 = it, then the 0x08D76150 one-time init (table 0x08D76158, 0x08CC40E0, hook
// 0x08B64494) at out+4. v0 = the flag byte when set, else 0x08B6441C's answer / 0x08B64494's result.
void func_088FA08C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 allocated = call(rt, ctx, 0x08B62344u, {0x20u});
    put(rt, allocated, 0x08CC40C0u);
    const u32 first = allocated + 4u;
    const u32 second = allocated + 0x10u;
    put(rt, first + 8u, 0u);
    put(rt, first + 4u, 0u);
    put(rt, allocated + 4u, 0u);
    put(rt, allocated + 0x10u, 0u);
    put(rt, second + 8u, 0u);
    put(rt, second + 4u, 0u);
    put(rt, allocated + 0x1Cu, value);
    put(rt, out, allocated);
    u32 result = rt.memory().load8(0x08D76150u);
    if (result == 0u) {
        result = call(rt, ctx, 0x08B6441Cu, {0x08D76150u});
        if (result != 0u) {
            put(rt, 0x08D76158u, 0x08CC40E0u);
            result = call(rt, ctx, 0x08B64494u, {0x08D76150u});
        }
    }
    put(rt, out + 4u, 0x08D76158u);
    ctx.gpr[2] = result;
}

// 0x08985E1C (a0 = writer, a1 = pointer to the string pointer): writes '"', then for each of the word(*a1)-0xC bytes
// escapes '"' and '\\' by writing a '\\' first, then the byte itself; tail calls 0x089EC7BC(writer, '"').

// 0x08A94498 (a0 = this, a1, a2 = size, a3): base 0x08A76ACC, vtable 0x08CBE0E0, this+0x14/0x18/0x1C/0x20 set from the
// arguments; with a non-zero size, 0x08B35840(a3, a2) gives the block stored at this+0x14 and the work goes on in a tail
// call 0x08B79A88(block, a1, a2) when a1 is set, else 0x08B36018(block, 0, a2). v0 = 0x08A76ACC's result when size is 0.

// 0x08B7AFDC (a0 = object): a null object is replaced by 0x08B796C4(). Each node of the list at +0x148 has a callback
// count at +4 and the callbacks at +8 upward; they run from the last down to the first (a0 as it came in), then the next
// node. The word at +0x3C, when set, is tail called with this. v0 = the last callback's result (or (count-1)<<2 for an
// empty node, or 0x08B796C4's result when nothing runs).
void func_08B7AFDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    u32 self = ctx.gpr[4];
    if (self == 0u) {
        self = call(rt, ctx, 0x08B796C4u, {0u});
    }
    u32 node = word(rt, self + 0x148u);
    while (node != 0u) {
        const u32 count = word(rt, node + 4u);
        const u32 last = count - 1u;
        if (static_cast<s32>(last) < 0) {
            ctx.gpr[2] = last << 2;
        } else {
            u32 slot = node + (last << 2) + 8u;
            u32 index = 0u;
            do {
                const u32 target = word(rt, slot);
                index += 1u;
                slot -= 4u;
                call_indirect(rt, ctx, target, {});
            } while (count != index);
        }
        node = word(rt, node);
    }
    const u32 tail = word(rt, self + 0x3Cu);
    if (tail == 0u) return;
    tail_call(ctx, tail, {self});
}

// 0x0880B34C (a0 = this): base 0x0896BDF8, vtable 0x08CBA3A0, +0x38 = 0x08CBA5D8, an 0x1C-byte object at this+0xE8 with
// +0/+4/+8 and four zero words from +0xC, then the 0x08D74FF8 one-time init (table 0x08D75000, 0x08CBCBD0, hook
// 0x08B64494) at this+0xEC. v0 = 0x000A0000 when the flag is set, else the hook answers.
void func_0880B34C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self, 0x08CBA3A0u);
    put(rt, self + 0x38u, 0x08CBA5D8u);
    const u32 allocated = call(rt, ctx, 0x08B62344u, {0x1Cu});
    put(rt, allocated, 0u);
    put(rt, allocated + 4u, 0u);
    put_byte(rt, allocated + 8u, 0u);
    for (u32 i = 0u; i < 4u; ++i) put(rt, allocated + 0xCu + i * 4u, 0u);
    const u32 inner = self + 0xE8u;
    put(rt, inner, allocated);
    u32 result = 0x000A0000u;
    if (rt.memory().load8(0x08D74FF8u) == 0u) {
        result = call(rt, ctx, 0x08B6441Cu, {0x08D74FF8u});
        if (result != 0u) {
            put(rt, 0x08D75000u, 0x08CBCBD0u);
            result = call(rt, ctx, 0x08B64494u, {0x08D74FF8u});
        }
    }
    put(rt, inner + 4u, 0x08D75000u);
    ctx.gpr[2] = result;
}

// 0x08990F2C (a0 = this): base 0x08994FD4, vtable 0x08CCC840, +0x38 = 0x08CCCA9C, 0x08A399C0(this+0x12C, 0x08CF6ED8),
// 0x08984824(this+0x130), the 0x80-entry table of 0x08D74FDC at +0x228, the flags at +0x4A8..+0x4C0 and +0x130-relative
// ones, and bit 0x40000 set in word(this+0xE0). v0 = 1.
void func_08990F2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08994FD4u, {self});
    put(rt, self, 0x08CCC840u);
    put(rt, self + 0x38u, 0x08CCCA9Cu);
    call(rt, ctx, 0x08A399C0u, {self + 0x12Cu, 0x08CF6ED8u});
    const u32 inner = self + 0x130u;
    call(rt, ctx, 0x08984824u, {inner});
    put(rt, self + 0x220u, 0u);
    put(rt, self + 0x224u, 0u);
    for (u32 i = 0u; i < 0x80u; ++i) put(rt, self + 0x228u + i * 4u, 0x08D74FD0u + 0xCu);
    put(rt, self + 0x4A8u, 0u);
    put(rt, self + 0x4ACu, 0u);
    put(rt, self + 0x4B0u, 0u);
    put(rt, self + 0x4B4u, 1u);
    put(rt, self + 0x4B8u, 0u);
    put(rt, self + 0x4BCu, 0u);
    put_byte(rt, self + 0x4C0u, 0u);
    put_byte(rt, inner + 0xC4u, 1u);
    put_byte(rt, inner + 0x38u, 0u);
    put(rt, inner + 0x3Cu, 0u);
    put_byte(rt, inner + 0x36u, 1u);
    const u32 flags = word(rt, self + 0xE0u) | 0x00040000u;
    put(rt, self + 0x120u, 1u);
    put(rt, self + 0xE0u, flags);
    ctx.gpr[2] = 1u;
}

// 0x08A58564 (a0 = this, a1 = destination): the size comes from word(this+0x1C) or, when null, from word(this+0x18)+0xC
// (0x08A5BB7C asserts on both being null); 0x08B79A88(a1, size, slot 0x120 of word(this+0x14)); then the same slot again
// rounds its answer up to 16. v0 = a1 + that rounded size.
void func_08A58564(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    u32 size = word(rt, self + 0x1Cu);
    if (size == 0u) {
        const u32 holder = word(rt, self + 0x18u);
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF7A0u});
        size = word(rt, holder + 0xCu);
    }
    u32 object = word(rt, self + 0x14u);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    const u32 amount = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x120u), {object});
    call(rt, ctx, 0x08B79A88u, {destination, size, amount});
    object = word(rt, self + 0x14u);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    const u32 written = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x120u), {object});
    ctx.gpr[2] = destination + ((written + 0xFu) & ~0xFu);
}

// 0x08B296C8 (a0 = state, a1 = buffer, a2 = length, t0 = offset, t1 = key material): 0x08B2933C(state, t1<<26,
// (t1<<26)|(t0>>6), t1>>6) seeds, 0x08B2936C(state, sp) fills the 0x40-byte keystream, then the buffer is XORed from
// offset&0x3F, refilling every 0x40 bytes (0x08B29348 then 0x08B2936C). v0 = 0 for an empty range, else 1 per byte, or
// the refill's result when the last byte refilled.

// 0x088725E4 (a0 = this, a1 = byte, f12, f13, f14): writes the GE state block at word(this+0x5C)+0x24. The flag picks
// the second half (f14 at +0x28, the 1.0/scale pair, f13/f12 and the 1.0 at +0x18 only when f12 > f13, flags |3) or the
// plain form (1.0 at +8, flags &~3), then tail calls 0x08872548(word(this+0x5C), this+0xC).

// 0x08A772E4 (a0, a1, a2): 0x08A5D438(sp+4), 0x08A77148(a0, sp+4, a1); when word(sp+4) is set, 0x08A76AAC(sp+8, sp)
// with a0 = word(sp+4) taken from it (0x08A5BB7C asserts), slot 0x20 of that object's vtable called with (a2, sp+8), then
// 0x08A5B9EC(sp+8); 0x08A5B9EC(sp+4) always. v0 = 1 when the body ran, else 0.
void func_08A772E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(4u);
    const u32 second = frame.at(8u);
    call(rt, ctx, 0x08A5D438u, {first});
    call(rt, ctx, 0x08A77148u, {a0, first, a1});
    u32 result = 0u;
    if (word(rt, first) != 0u) {
        call(rt, ctx, 0x08A76AACu, {second, frame.at(0u)});
        const u32 object = word(rt, first);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E6Cu});
        result = 1u;
        ctx.gpr[4] = object;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {object, second, a2});
        call(rt, ctx, 0x08A5B9ECu, {second});
    }
    call(rt, ctx, 0x08A5B9ECu, {first});
    ctx.gpr[2] = result;
}

// 0x08A776F8: the same body with slot 0x1C of the vtable and (a1, sp+8) as its arguments instead of (a2, sp+8).
void func_08A776F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(4u);
    const u32 second = frame.at(8u);
    call(rt, ctx, 0x08A5D438u, {first});
    call(rt, ctx, 0x08A77148u, {a0, first, a2});
    u32 result = 0u;
    if (word(rt, first) != 0u) {
        call(rt, ctx, 0x08A76AACu, {second, frame.at(0u)});
        const u32 object = word(rt, first);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E6Cu});
        result = 1u;
        ctx.gpr[4] = object;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, a1, second});
        call(rt, ctx, 0x08A5B9ECu, {second});
    }
    call(rt, ctx, 0x08A5B9ECu, {first});
    ctx.gpr[2] = result;
}

// 0x08AF9CB0 (a0 = table, a1 = out pair, a2 = index, a3): out of range answers -1. Entry = word(a0+0x30) + index*48; its
// word at +0x18 goes to *a1 when *a1 is 0; a3 (or word(a0+0x68) when 0) is the filter. 0x08C41D90(a0, sp, index, filter,
// a1, 0) retries while word(sp+4) is negative; its answer becomes v0/v1 and word(sp) goes to a1+4, 0 ends with -1.
void func_08AF9CB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 index = ctx.gpr[6];
    const u32 filter_argument = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    u32 result = 0xFFFFFFFFu;
    if (index < static_cast<u32>(rt.memory().load16(self + 0x10u))) {
        const u32 entry = word(rt, self + 0x30u) + index * 48u;
        if (word(rt, out) == 0u) put(rt, out, word(rt, entry + 0x18u));
        u32 filter = filter_argument;
        if (filter == 0u) filter = word(rt, self + 0x68u);
        put(rt, frame.at(4u), 0xFFFFFFFFu);
        put(rt, frame.at(0u), 0xFFFFFFFFu);
        for (;;) {
            const u32 answered = call(rt, ctx, 0x08C41D90u, {self, frame.at(0u), index, filter, out, 0u});
            if (answered == 0u) {
                result = 0xFFFFFFFFu;
                break;
            }
            const u32 candidate = word(rt, frame.at(4u));
            if (static_cast<s32>(candidate) < 0) continue;
            result = candidate;
            put(rt, out + 4u, word(rt, frame.at(0u)));
            break;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08B0A458 (a0 = holder, a1 = state): 0x08B344E0(0x08D473E8) locks; a null a1 takes word(0x08CE0A24); state 5 goes
// through slot 0x18 of word(word(a0)+0x110) (a match keeps it, anything else drops it) and allocates 0x00020CE0 bytes
// through 0x08B62344 + 0x08B451A0 at word(a0)+0x110 when still missing. 0x08B3457C unlocks; v0 = the object or 0.

// 0x08BA6470 (a0 = this, a1, a2): 0x08B569A0(this, a1, a2) locks, 0x08B566BC(word(this+8)), then while the count at
// word(this+8)+0x48 (slot 0x48 of its vtable) or the half at word(this+8)+0x40 is above the index: 0x08884D6C on the
// element, 0x08AFA208(word(this+0x10), a2, 0, index, word(this+0x14)) when it answers, next element; 0x08B56940 ends.

// 0x08C2E994 (a0 = first, a1 = last, a2 = value, a3 = comparator): the partition loop of a range: advance a0 while
// slot 8 of the comparator's vtable answers non-zero on (a0, a2), step a1 down while it answers on (a2, a1); when a0 is
// still below a1 the two elements are swapped through the temporary at sp (0x08A76ABC / 0x08A5BAD8 / 0x08A5B9EC) and
// a0 advances. v0 = a0.

} // namespace
} // namespace new_batch_main_1589
} // namespace gt::game

#define NEW1589(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1589::name, gt::ReturnKind::kind)
NEW1589(0x08A3E4F8u, func_08A3E4F8, Word);
NEW1589(0x08A560DCu, func_08A560DC, Word);
NEW1589(0x08AC1FE0u, func_08AC1FE0, Word);
NEW1589(0x088A7A74u, func_088A7A74, Word);
NEW1589(0x08910DF4u, func_08910DF4, Word);
NEW1589(0x08917C70u, func_08917C70, Word);
NEW1589(0x08919114u, func_08919114, Word);
NEW1589(0x089FCC98u, func_089FCC98, Word);
NEW1589(0x088A2FA4u, func_088A2FA4, Word);
NEW1589(0x088DAB68u, func_088DAB68, Word);
NEW1589(0x088FA08Cu, func_088FA08C, Word);
NEW1589(0x08B7AFDCu, func_08B7AFDC, Word);
NEW1589(0x0880B34Cu, func_0880B34C, Word);
NEW1589(0x08990F2Cu, func_08990F2C, Word);
NEW1589(0x08A58564u, func_08A58564, Word);
NEW1589(0x08A772E4u, func_08A772E4, Word);
NEW1589(0x08A776F8u, func_08A776F8, Word);
NEW1589(0x08AF9CB0u, func_08AF9CB0, Word);
