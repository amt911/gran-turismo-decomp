// New batch, main session 6: 80 short functions the recompiler emitted, none decompiled before, written by hand as
// structured C from `tools.disasm`. Most repeat a few shapes, which the helpers below hold: a value looked up in a scope
// on the frame and handed to an action (the 0x08818xxx/0x0882xxxx/0x08847xxx/0x08854xxx/0x08A1Cxxx family), a script
// attribute written from a virtual getter (the 0x08C1Exxx/0x08C24xxx/0x08B87xxx/0x08B88xxx family), and two fixed-point
// rescalings. v0 is left as the original leaves it on every path (a callee that never writes v0 hands the previous value
// on, so the call helpers start it the way the original's `jal`/`jalr` has it). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_6 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values, so a callee that never writes v0 hands
// the value the caller had on to the code after the call.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_full(rt, ctx, target, arguments, floats).f0;
}

// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v0, (object); lw a1, slot(v0); jalr a1`: at the jalr v0 holds the vtable and a1 the target.
GuestResult call_virtual_in_a1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                               std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[5] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
    return (target & ~mask) | ((value << position) & mask);
}

// The VFPU operands of the `vmmul.q M100, E200, M000` in 0x08B5661C: destination M100 (register 4), target M000 (0), and
// the transposed source E200 (0x28: matrix 2 with bit 5 set). tools.disasm prints the mnemonic, not the vs/vt fields.
constexpr u32 kTransposedMatrix2 = 0x28u;

// --- a0 = an output block, a1 = a second word; the vararg registers are saved in the frame, the initializer runs and its
// v0, a1 and a zero byte go into the block (v0 stays the callee's).
void variadic_init(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 out = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0x18), ctx.gpr[6]);
    put(rt, frame.at(0x1C), ctx.gpr[7]);
    put(rt, frame.at(0x20), ctx.gpr[8]);
    put(rt, frame.at(0x24), ctx.gpr[9]);
    put(rt, frame.at(0x28), ctx.gpr[10]);
    put(rt, frame.at(0x2C), ctx.gpr[11]);
    const u32 value = call(rt, ctx, target, {out, second});
    put(rt, out, value);
    put(rt, out + 4u, second);
    put_byte(rt, out + 8u, 0u);
}

// --- script attribute, scalar: a0 = the parameter block, a1 = pointer to the object; the object is null-checked (assert
// with `message`), its virtual getter's result is stored as a half or a byte at self + offset.
enum class Scalar { Half, Byte };
void store_scalar_from_virtual(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot, u32 offset, Scalar kind) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual(rt, ctx, object, slot, {object});
    if (kind == Scalar::Half) put_half(rt, self + offset, result.v0);
    else put_byte(rt, self + offset, result.v0);
}
// The same, but the getter's v0 and v1 both go into the block (two words at +0x18 / +0x1C).
void store_pair_from_virtual(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual(rt, ctx, object, slot, {object});
    put(rt, self + 0x18u, result.v0);
    put(rt, self + 0x1Cu, result.v1);
}
// The same shape with the slot taken through a1, so v0 holds the vtable at the jalr (the original's `lw v0, (s0)`).
void store_pair_from_virtual_a1(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual_in_a1(rt, ctx, object, slot, {object});
    put(rt, self + 0x18u, result.v0);
    put(rt, self + 0x1Cu, result.v1);
}
// The same, reduced to a flag: a byte at +0x14, 1 when the getter's v0 is non-zero (`sltu`).
void store_flag_from_virtual(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual(rt, ctx, object, slot, {object});
    const u32 flag = result.v0 != 0u ? 1u : 0u;
    put_byte(rt, self + 0x14u, flag);
    ctx.gpr[2] = flag;
}

// --- a lookup in a scope on the frame: `open` fills the frame slot, the object it produced is null-checked (assert with
// `message`), `lookup` turns it into a value, the scope is closed and the value goes to `action` (v0 = the action's).
void scope_action(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message, u32 lookup, u32 action) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, open, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 value = call(rt, ctx, lookup, {object});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
    call(rt, ctx, action, {value});
}
// The same scope, but the object's two floats at +0xF0+0x40 / +0x44 are copied to +0xF0+0xBC / +0xC0.
void scope_copy_float_pair(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, open, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 base = object + 0xF0u;
    const float first = float_at(rt, base + 0x40u);
    const float second = float_at(rt, base + 0x44u);
    put_float(rt, base + 0xBCu, first);
    put_float(rt, base + 0xC0u, second);
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
}
// The same scope, but the object's member at +0x14 and a no-argument getter's result go to a two-argument action
// (a0 = the member, a1 = the getter's value; the getter is called with a0 still the assert message, as the compiler left it).
void scope_member_action(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message, u32 getter, u32 action) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, open, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 member = word(rt, object + 0x14u);
    const u32 value = call(rt, ctx, getter, {message});
    call(rt, ctx, action, {member, value});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
}

// --- fixed-point: (value * factor) as a 32-bit product, then the compiler's signed division by 1000 (`0x10624DD3`, shift 6).
s32 scaled_by_1000(s32 value, u32 factor) {
    const s32 product = static_cast<s32>(static_cast<u32>(value) * factor);
    return product / 1000;
}
// The same for the compiler's signed division by 100 (`0x51EB851F`, shift 5, sign correction).
s32 scaled_by_100(s32 value, u32 factor) {
    const s32 product = static_cast<s32>(static_cast<u32>(value) * factor);
    return product / 100;
}

// 0x08BC5178: builds a three-word block at a0 out of the initializer 0x08BC50AC's v0 and a1.
void func_08BC5178(Runtime &rt, AllegrexContext &ctx) { variadic_init(rt, ctx, 0x08BC50ACu); }

// 0x08BC60E4: the same with the initializer 0x08BC6018.
void func_08BC60E4(Runtime &rt, AllegrexContext &ctx) { variadic_init(rt, ctx, 0x08BC6018u); }

// 0x08C1E04C: a half at +0x14 from the object's vtable slot 0x38.
void func_08C1E04C(Runtime &rt, AllegrexContext &ctx) { store_scalar_from_virtual(rt, ctx, 0x08CAF6D0u, 0x38u, 0x14u, Scalar::Half); }

// 0x08C1E458: a byte at +0x14 from the object's vtable slot 0x34.
void func_08C1E458(Runtime &rt, AllegrexContext &ctx) { store_scalar_from_virtual(rt, ctx, 0x08CAFA78u, 0x34u, 0x14u, Scalar::Byte); }

// 0x08C24790: a half at +0x14 from the object's vtable slot 0x48.

// 0x08C247E4: a byte at +0x14 from the object's vtable slot 0x44.

// 0x0881800C: 0x08817FCC(scope), 0x08817A54(scope) -> 0x08817A0C.

// 0x0882994C: 0x088297A8(scope), 0x088294D0(scope) -> 0x0885EEE0.

// 0x08829E58: the same scope and lookup, action 0x0885F46C.

// 0x08829EB0: action 0x0885EBB0.

// 0x08829F08: action 0x0885E414.

// 0x08829F60: action 0x0885F460.

// 0x08829FB8: action 0x0885F444.

// 0x0882A010: action 0x0885EA48.

// 0x0882A068: action 0x0885E940.

// 0x0882A118: action 0x0885E8DC.

// 0x0882A170: action 0x0885E878.

// 0x0882A220: action 0x0885E7DC.

// 0x0882A480: action 0x0885E764.

// 0x0882A4D8: action 0x0886006C.

// 0x0882A588: action 0x088622B8.

// 0x0882A5E0: action 0x08863340.

// 0x08847580: 0x08847008(scope), 0x08846E50(scope) -> 0x088D5AFC.

// 0x08847958: action 0x088D4D24.

// 0x088479B0: action 0x088D59B8.

// 0x088548B0: 0x088547C4(scope), 0x08854678(scope) -> 0x088A719C.

// 0x0885FC4C (a0 = a holder): runs three calls on the object it holds -- 0x08866918 on the object, then 0x088D6E58 and
// 0x088D5EAC on the word at the object's +0x5AC (the second with +4) -- and ends in a tail call 0x088F27F8(object + 0x45A4,
// result of the third call).
void func_0885FC4C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    u32 member = 0u;
    u32 argument = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08866918u, {word(rt, holder)});
        const u32 object = word(rt, holder);
        call(rt, ctx, 0x088D6E58u, {word(rt, object + 0x5ACu)});
        const u32 again = word(rt, holder);
        member = again + 0x45A4u;
        argument = call(rt, ctx, 0x088D5EACu, {word(rt, again + 0x5ACu) + 4u});
    }
    tail_call(ctx, 0x088F27F8u, {member, argument});
}

// 0x088E4464 (a0 = a holder): while the object at *a0 answers non-zero from its vtable slot 0x4C, 0x08B1A844 is asked; a
// non-zero answer ends the loop, otherwise 0x08B2E2B8(0x7D0) runs and the object is asked again.
void func_088E4464(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    for (;;) {
        const u32 object = word(rt, holder);
        if (call_virtual(rt, ctx, object, 0x4Cu, {object}).v0 == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (call(rt, ctx, 0x08B1A844u) != 0u) return;
        call(rt, ctx, 0x08B2E2B8u, {0x7D0u});
    }
}

// 0x088ED0D4 (a0 = an object, a1 = a byte, a2 = 8 bytes of data): the two words at a2 and the byte go to a 9-byte block on
// the frame, then the object's vtable slot 0xF4 runs with (object, 0, -1, 0xB, block, 0xC).
void func_088ED0D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0x00), word(rt, source));
    put(rt, frame.at(0x04), word(rt, source + 4u));
    put_byte(rt, frame.at(0x08), value);
    const u32 object = word(rt, self + 0x538u);
    call_virtual(rt, ctx, object, 0xF4u, {object, 0u, 0xFFFFFFFFu, 0x0Bu, frame.at(0), 0x0Cu});
}

// 0x089190BC (a0 = an object, a1, a2 = a lookup's arguments, a3 = an output float): 0x088DF13C(a1, a2) gives a pair
// (float +0, word +0x18); 0x08918FB0(object, word, float) turns it into a float that goes to *a3 and into f12, and the
// function ends in a tail call 0x08918C84(object, object->0x1F8).
void func_089190BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    float value = 0.0f;
    u32 member = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 pair = call(rt, ctx, 0x088DF13Cu, {first, second});
        value = call_float(rt, ctx, 0x08918FB0u, {self, word(rt, pair + 0x18u)}, {float_at(rt, pair)});
        put_float(rt, out, value);
        member = word(rt, self + 0x1F8u);
    }
    ctx.fpr[12] = value;
    tail_call(ctx, 0x08918C84u, {self, member});
}

// 0x0897851C (a0 = an object): initialises the two intrusive list heads at +0x184 and +0x18C with 0x08BEDDC4 and makes
// each one its own successor.

// 0x0897E884: 0x0897E844(scope), then the object's floats +0x130 / +0x134 to +0x1AC / +0x1B0, scope closed.

// 0x0898970C: the same with 0x089896CC(scope) and the message 0x08CA39F0.

// 0x089AB444: 0x089AB370(scope), the object's member at +0x14, 0x089D5B30() as the value, 0x08978D3C(member, value).

// 0x089AB49C: the same with 0x08965418() and 0x0897BB18(member, value).

// 0x089AB4F4: 0x08965418() and 0x08978D3C(member, value).

// 0x08A0F800 (a0 = an object, a1 = pointer to a word): 0x08B20920(frame, 0x08C8E004) and 0x08B20A54(frame, word, 0)
// produce four bits that replace bits 24..27 of the object's +0x60 (`ins`).

// 0x08A1C3B0: 0x08A1C370(scope), 0x08A1C0F4(scope) -> 0x08A1C0FC.

// 0x08A26ED8 (a0 = an object): 0x08B344E0(object), the member at +0x28 is taken and cleared, 0x08B3457C(object), and a
// non-null member is tail-called through 0x08B1B99C.

// 0x08A94B90 (a0 = a container, a1 = a byte offset, a2 = pointer to an object): stores the object's virtual getter's
// byte result at *(container + 0x14 + a1); a null object asserts with 0x08CB211C.

// 0x08AA91A0 (a0 = a holder, a1, a2, a3): tail-calls 0x08C31374(member at +4, a1, a2, a3); a null member asserts with
// 0x08CB30B0.
void func_08AA91A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    u32 member = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        member = word(rt, ctx.gpr[4] + 4u);
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C31374u, {member, second, third, fourth});
}

// 0x08AC0388 (a0 = an object): two virtual calls (slots 0x0C and 0x10), 0x08B15B10(object->+8, object + 0x10, -1), then a
// tail call through the vtable slot 0x08 of object->+8.

// 0x08B0DA60: walks the global list at 0x08D7734C, asking each node's object at +4 for its vtable slot 0x3C; the first
// answer that is zero ends the walk with v0 = 0, a null node ends it with v0 = 1 (and so does an empty list).

// 0x08B0E560 (a0 = an object, a1 = another object): 0x08B34888(object + 4), other->+0x58 = object,
// 0x08B1FBEC(object + 0xF8, other + 0x5C), object->+0xA4 = 0, tail call 0x08B34B24(object + 4).

// 0x08B173EC (a0 = an object): 0x08B28C44(object + 0x18D4) and, while the byte at +0x19DC stays non-zero,
// 0x08B17240(object) and 0x08B28C74(object + 0x1900); v0 is the byte that ended the loop (0).

// 0x08B48EAC (a0 = pointer to a mode, f12 = a value): mode 0 scales by 0x40666666 (3.6), mode 1 also by 0x3F1F136A,
// anything else passes the value through; f0 (and f12) is the result, v0 = 1.
void func_08B48EAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = 1u;
    float value = ctx.fpr[12];
    if (mode == 0u) {
        value = psp_mul(value, float_bits(0x40666666u));
    } else if (mode == 1u) {
        value = psp_mul(psp_mul(value, float_bits(0x40666666u)), float_bits(0x3F1F136Au));
    }
    ctx.fpr[12] = value;
    ctx.fpr[0] = value;
}

// 0x08B5420C (a0 = an object): 0x08B56E20(0x08D6D0E0) after taking the global handle at 0x08D6DDE0, then the constant at
// 0x08CE0DD0 into f12, four words of 0x08CB6A60 into C000, and a tail call 0x08B54104(object, handle, result).
void func_08B5420C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 handle = 0u;
    u32 result = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        handle = word(rt, 0x08D6DDE0u);
        result = call(rt, ctx, 0x08B56E20u, {0x08D6D0E0u});
    }
    ctx.fpr[12] = float_at(rt, 0x08CE0DD0u);
    gt::vfpu::load_quad(rt, ctx, 0u, 0x08CB6A60u);
    tail_call(ctx, 0x08B54104u, {self, handle, result});
}

// 0x08B5661C (a0 = an object): the 4x4 matrix at the object's +0xD00 + 4 * word(+0xD10) is multiplied by M000
// (vmmul.q M100, M200, M000) and stored back; the words at +0xD18 / +0xD1C are OR'ed with +0xD20 / +0xD24.
void func_08B5661C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = word(rt, self + 0xD10u);
    const u32 flags_a = word(rt, self + 0xD18u);
    const u32 flags_b = word(rt, self + 0xD1Cu);
    const u32 matrix = word(rt, self + 0xD00u + index * 4u);
    const u32 extra_a = word(rt, self + 0xD20u);
    const u32 extra_b = word(rt, self + 0xD24u);
    for (u32 column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 8u + column, matrix + 0x10u * column);
    gt::vfpu::matrix_multiply(ctx, 4u, kTransposedMatrix2, 0u, 4u);
    put(rt, self + 0xD18u, flags_a | extra_a);
    put(rt, self + 0xD1Cu, flags_b | extra_b);
    for (u32 column = 0u; column < 4u; ++column) gt::vfpu::store_quad(rt, ctx, 4u + column, matrix + 0x10u * column);
    ctx.gpr[2] = flags_a | extra_a;
}

// 0x08B877D4: the object's vtable slot 0x50 gives two words for +0x18 / +0x1C.
void func_08B877D4(Runtime &rt, AllegrexContext &ctx) { store_pair_from_virtual(rt, ctx, 0x08C97FCCu, 0x50u); }

// 0x08B8897C: the object's vtable slot 0x40 gives two words for +0x18 / +0x1C.
void func_08B8897C(Runtime &rt, AllegrexContext &ctx) { store_pair_from_virtual(rt, ctx, 0x08C9832Cu, 0x40u); }

// 0x08B88AB0: the object's vtable slot 0x30 gives a flag at +0x14 (1 when non-zero).
void func_08B88AB0(Runtime &rt, AllegrexContext &ctx) { store_flag_from_virtual(rt, ctx, 0x08C98418u, 0x30u); }

// 0x08C1FF18: the object's vtable slot 0x58 (taken through a1) gives two words for +0x18 / +0x1C.

// 0x08C37CC0 (a0 = an object, a1 = a word, a2 unused): the word goes to a 4-byte block on the frame, most significant
// byte first when the word at +0x1C is zero and least significant first otherwise, then the object's vtable slot 0x108
// runs with (object, block, 4).
void func_08C37CC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const bool low_first = word(rt, self + 0x1Cu) == 0u;
    GuestFrame frame(ctx, 0x20u);
    for (u32 index = 0u; index != 4u; ++index) {
        const u32 shift = (low_first ? index : 3u - index) * 8u;
        put_byte(rt, frame.at(index), value >> shift);
    }
    call_virtual(rt, ctx, self, 0x108u, {self, frame.at(0), 4u});
}

// 0x08C4B078 (a0 = destination, a1 = source, a2 = a base address): copies 0x70-byte blocks from a2 to a0, advancing both
// by 0x70, until the destination reaches a1 (a self-check first: equal pointers copy nothing).

// 0x08C750C0 (a0 = a pointer to a buffer pointer, a1 = an end pointer, a2 = a signed byte): 0x08C74FAC(object, end - base,
// 0, 1, byte), then -1 goes to *(base - 4) and v0 = base + (end - base).

// 0x088183D4 (a0 = an object, a1..t3 = a description of a call, the caller's seventh argument a pointer to a source):
// builds a block on the frame out of the argument registers, 0x08B7C830(block + 0x14, *source, 0x80), then
// 0x08818168(object, block).

// 0x0885EC98 (a0 = a holder, a1 = an argument): 0x088DA0BC(object at *a0's +0x5AC), 0x088DE700(that, a1) and, when it
// answers non-zero, 0x088A40B0(answer); the function ends in a tail call 0x088DA57C(object, a1).

// 0x088D2C30 (a0 = an object, a1 = an argument): the object's +0x14C member answers its vtable slot 0x2C; when that is
// non-null its slot 0x18 runs with (answer, a1) and its v0 is the result (0 when the first answer is null).

// 0x088EF554 (a0 = an object, a1 = an index, a2 = a value): sets the byte at +0x92, then, for an index below 0x33 and a
// non-negative value, clamps the value into the pair at 0x08C89F6C + 8 * index and jumps to the case block at
// 0x08C8A120 + 4 * index with (clamped, low, raised, object). The case blocks write halfwords into this function's own
// frame (sp + 6..sp + 26) and the shared exit at 0x088EF7AC pops it, so sp stays lowered on the dispatch path instead of
// a GuestFrame's scope, and the two early exits pop it themselves.
void func_088EF554(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const s32 value = static_cast<s32>(ctx.gpr[6]);
    const u32 entry_sp = ctx.gpr[29];
    ctx.gpr[29] = entry_sp - 0x20u;
    rt.memory().store8(self + 0x92u, 1u);
    if (index >= 0x33u) {
        ctx.gpr[29] = entry_sp;
        ctx.gpr[2] = 1u;
        return;
    }
    if (value < 0) {
        ctx.gpr[29] = entry_sp;
        ctx.gpr[2] = 0x00480000u;  // the `lui v0, 0x48` in the branch's delay slot
        return;
    }
    const u32 pair = 0x08C89F6Cu + index * 8u;
    const s32 low = static_cast<s32>(word(rt, pair));
    const s32 high = static_cast<s32>(word(rt, pair + 4u));
    const s32 raised = value > low ? value : low;  // `max a2, a2, a1`
    const s32 clamped = high < raised ? high : raised;  // `min a0, a0, a2`
    const u32 target = word(rt, 0x08C8A120u + index * 4u);
    ctx.gpr[2] = target;
    tail_call(ctx, target, {static_cast<u32>(clamped), static_cast<u32>(low), static_cast<u32>(raised), self});
}

// 0x088FE308 (a0 = an object): 0x08B344E0(object + 0x10), byte +0xB = 0, 0x0896275C(object->+0x10), 0x08B36018 of
// object->+0x44 with (0, 0x20), then a tail call 0x08B3457C(object + 0x10).

// 0x0892CE18 (a1 = a record, a2 = a factor block): the signed half at +0x20 and the byte at +0x24 are each scaled by the
// record's factors (u16 at +0, byte at +5) and divided by 1000 and 100; v0 is the first result.
void func_0892CE18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[5];
    const u32 factors = ctx.gpr[6];
    const s32 half = scaled_by_1000(static_cast<s32>(signed_half(rt.memory().load16(self + 0x20u))),
                                    rt.memory().load16(factors));
    put_half(rt, self + 0x20u, static_cast<u32>(half));
    const u32 byte_product = byte_at(rt, self + 0x24u) * byte_at(rt, factors + 5u);
    put_byte(rt, self + 0x24u, byte_product / 100u);
    ctx.gpr[2] = 0x51EB851Fu;  // the scaled-by-100 magic left in v0 by the `lui`/`ori` pair
}

// --- 0x0892CF40 / 0x0892D108 / 0x0892D164 / 0x0892D648 / 0x0892D6A4: the u16 at +0x10A and +0x10C are each scaled by
// (u16 at a2 + 0) / (u16 at a2 + 2), as a signed 32-bit product divided by 100; v0 is the first result.
void scale_two_halves(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[5];
    const u32 factors = ctx.gpr[6];
    const s32 first = scaled_by_100(static_cast<s32>(rt.memory().load16(self + 0x10Au)),
                                    rt.memory().load16(factors));
    const s32 second = scaled_by_100(static_cast<s32>(rt.memory().load16(self + 0x10Cu)),
                                     rt.memory().load16(factors + 2u));
    put_half(rt, self + 0x10Au, static_cast<u32>(first));
    put_half(rt, self + 0x10Cu, static_cast<u32>(second));
    ctx.gpr[2] = static_cast<u32>(first);
}
// 0x0892CF40.
void func_0892CF40(Runtime &rt, AllegrexContext &ctx) { scale_two_halves(rt, ctx); }

// 0x0892D108: byte-identical to 0x0892CF40.
void func_0892D108(Runtime &rt, AllegrexContext &ctx) { scale_two_halves(rt, ctx); }

// 0x0892D164: byte-identical to 0x0892CF40.
void func_0892D164(Runtime &rt, AllegrexContext &ctx) { scale_two_halves(rt, ctx); }

// 0x0892D648: byte-identical to 0x0892CF40.
void func_0892D648(Runtime &rt, AllegrexContext &ctx) { scale_two_halves(rt, ctx); }

// 0x0892D6A4: byte-identical to 0x0892CF40.
void func_0892D6A4(Runtime &rt, AllegrexContext &ctx) { scale_two_halves(rt, ctx); }

// 0x0899E2A8 (a0 = an object): installs the vtables 0x08CCB5F8 and 0x08CCB850 (at +0x38), closes the members at +0x16C,
// +0x168, +0x164 and +0x160 and ends in a tail call 0x089A29E4(object).
void func_0899E2A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CCB5F8u);
        put(rt, self + 0x38u, 0x08CCB850u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x16Cu});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x168u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x164u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x160u});
    }
    tail_call(ctx, 0x089A29E4u, {self});
}

// 0x089C6A58 (a0 = an object): installs the vtable 0x08CCD338, destroys the member at +0x240 through its first vtable
// word (unless it is empty or the end marker at +0x4D0), 0x08B22AF0(object + 0x208), tail call 0x08978B04(object).

// 0x08A13AD8 (a0 = an object, f12 = a value): 0x08B34888(object + 0x10) with f12 kept in f20, then, when the byte at
// +0x1F6 is non-zero, 0x08ACD1F8(object + 0x240, f12); tail call 0x08B34B24(object + 0x10) with f20 restored.

// 0x08A35894 (a0 = an object): installs the vtable 0x08CCE588 (in the first call's delay slot), 0x08A35114(object), closes
// the members at +0x1C, +0x18 and +0x14, 0x08A780B0(object), tail call 0x08A5BC54(object, 0x28, 4).

// 0x08A64198 (a0 = an argument): 0x08A61C18(scope) gives an object (assert 0x08CB02D0 when null), 0x08A640EC(object, a0),
// scope closed.

// 0x08A9DD14 (a0 = an argument): 0x08A9DCB0(0) gives a word that goes to the frame, 0x08A5BAB8(a0, frame); when the byte
// at +0x7C of a second 0x08A9DCB0(0) is zero, 0x08A9C340(that object).
void func_08A9DD14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0), call(rt, ctx, 0x08A9DCB0u, {0u}));
    call(rt, ctx, 0x08A5BAB8u, {self, frame.at(0)});
    const u32 object = call(rt, ctx, 0x08A9DCB0u, {0u});
    if (byte_at(rt, object + 0x7Cu) != 0u) return;
    call(rt, ctx, 0x08A9C340u, {call(rt, ctx, 0x08A9DCB0u, {0u})});
}

// 0x08AAD860 (a0 = a base address): 0x08AAD7A0(); for 0x800 blocks from base + 0x7FF0 down, 0x08A5B9EC(block + 0xC) and
// 0x08A69D68(block + 8).

// 0x08AC2D10 (a0 = a block): when the word at +0 is negative 0x08AC2CDC(object) runs first; 0x08C7A850(object + 0xC) is
// stored at +0 and at +8 (always), and a negative answer also gets +4 = -1, byte +0xC = 0 and returns 0, else 1.

// 0x08AF1068 (a0 = a pair of words, a1 = an argument, a2 = a mode byte): a non-zero mode converts the pair with
// 0x08C3EACC(frame + 4, frame, a1, a1), mode 0 with 0x08C3EB78(word, second word, a1).

// 0x08B053E8 (a0 = a container): 0x08B05370 on each of the lhu(+0x7E) elements of 0x200 bytes starting at +0x80 (v0 = 0).
void func_08B053E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 count = rt.memory().load16(self + 0x7Eu);
    if (count == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 element = self + 0x80u;
    for (u32 index = 0u;;) {
        call(rt, ctx, 0x08B05370u, {element});
        const u32 elements = rt.memory().load16(self + 0x7Eu);
        index += 1u;
        if (index >= elements) {
            ctx.gpr[2] = 0u;
            return;
        }
        element += 0x200u;
    }
}

// 0x08B0C71C (a0 = an object): with the byte at +0x16 zero it answers -1; otherwise it clears the byte, 0x08B0C43C(object)
// and, when that answers non-null, a tail call through its vtable slot 0x78 with (object, object).
void func_08B0C71C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member = 0u;
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        if (byte_at(rt, self + 0x16u) == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        put_byte(rt, self + 0x16u, 0u);
        member = call(rt, ctx, 0x08B0C43Cu, {self});
        if (member == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        target = word(rt, word(rt, member) + 0x78u);
    }
    tail_call(ctx, target, {member, self});
}

// 0x08B1FC78 (a0 = a list head, a1 and a2 = two nodes): splices the node at a1's +4 out of the chain through +8 and the
// node at a2's +8 out of the chain through +4, then clears both; v0 = the word at a2 + 8.
void func_08B1FC78(Runtime &rt, AllegrexContext &ctx) {
    const u32 head = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 first_link = word(rt, first + 4u);
    u32 first_successor = 0u;
    if (first_link != 0u) first_successor = word(rt, first_link + 4u);
    const u32 second_link = word(rt, second + 8u);
    u32 second_successor = 0u;
    if (second_link != 0u) second_successor = word(rt, second_link + 8u);
    if (first_link == 0u) {
        put(rt, head + 4u, second_successor);
    } else if (first_successor != 0u) {
        put(rt, first_successor + 8u, second_successor);
    }
    if (second_link == 0u) {
        put(rt, head + 8u, first_successor);
    } else if (second_successor != 0u) {
        put(rt, second_successor + 4u, first_successor);
    }
    put(rt, first + 4u, 0u);
    put(rt, second + 8u, 0u);
    ctx.gpr[2] = second_link;
}

// 0x08B2DD2C: when the global at 0x08D68D64 is not positive, 0x08B2ECAC(0x08CB546C, 0x08B2DC98, 0) creates it,
// 0x08C7AF60(-1, created) gets a handle, and both are stored (the handle also at 0x08CE0C78).

// 0x08B2DF58: 0x08C7B008(frame) then 0x08C7B010(frame + 8, frame); either failure answers v0 = v1 = -1, success runs
// 0x08B2DE94(frame + 8) and keeps its v0 / v1.

} // namespace
} // namespace new_batch_main_6
} // namespace gt::game

#define NEW_MAIN6(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_6::name, gt::ReturnKind::kind)
NEW_MAIN6(0x08BC5178u, func_08BC5178, Word);
NEW_MAIN6(0x08BC60E4u, func_08BC60E4, Word);
NEW_MAIN6(0x08C1E04Cu, func_08C1E04C, Word);
NEW_MAIN6(0x08C1E458u, func_08C1E458, Word);
NEW_MAIN6(0x0885FC4Cu, func_0885FC4C, Word);
NEW_MAIN6(0x088E4464u, func_088E4464, Word);
NEW_MAIN6(0x088ED0D4u, func_088ED0D4, Word);
NEW_MAIN6(0x089190BCu, func_089190BC, Word);
NEW_MAIN6(0x08AA91A0u, func_08AA91A0, Word);
NEW_MAIN6(0x08B48EACu, func_08B48EAC, Float);
NEW_MAIN6(0x08B5420Cu, func_08B5420C, Word);
NEW_MAIN6(0x08B5661Cu, func_08B5661C, Word);
NEW_MAIN6(0x08B877D4u, func_08B877D4, Word);
NEW_MAIN6(0x08B8897Cu, func_08B8897C, Word);
NEW_MAIN6(0x08B88AB0u, func_08B88AB0, Word);
NEW_MAIN6(0x08C37CC0u, func_08C37CC0, Word);
NEW_MAIN6(0x088EF554u, func_088EF554, Word);
NEW_MAIN6(0x0892CE18u, func_0892CE18, Word);
NEW_MAIN6(0x0892CF40u, func_0892CF40, Word);
NEW_MAIN6(0x0892D108u, func_0892D108, Word);
NEW_MAIN6(0x0892D164u, func_0892D164, Word);
NEW_MAIN6(0x0892D648u, func_0892D648, Word);
NEW_MAIN6(0x0892D6A4u, func_0892D6A4, Word);
NEW_MAIN6(0x0899E2A8u, func_0899E2A8, Word);
NEW_MAIN6(0x08A9DD14u, func_08A9DD14, Word);
NEW_MAIN6(0x08B053E8u, func_08B053E8, Word);
NEW_MAIN6(0x08B0C71Cu, func_08B0C71C, Word);
NEW_MAIN6(0x08B1FC78u, func_08B1FC78, Word);