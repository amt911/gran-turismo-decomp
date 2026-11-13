// Hot batch 186: twenty-one-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_186 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_half_at(psprecomp::Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}


// Destructor: the vtable, the sub-object at +`offset` released by `release` (and a conditional member), base destructor `base`;
// the caller tail-calls the sized-array tail (0x08A5BC54).
// The virtual at +0x20 (a size in bytes) of the object, as a count above 0x80.
void excess_over_128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 size = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x20u), {self});
    u32 result = 0u;
    if (size >= 0x81u) result = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x20u), {self}) - 0x80u;
    ctx.gpr[2] = result;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088056A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08AC2C50u); call_here(rt, ctx, 0x08805BC0u); call_here(rt, ctx, 0x088057A0u); call_here(rt, ctx, 0x08ABDF48u); call_here(rt, ctx, 0x08AC3340u); call_here(rt, ctx, 0x08ABCE64u); call_here(rt, ctx, 0x08805B80u); call_here(rt, ctx, 0x08805AF8u); } jump(ctx, 0x088057B0u); }
void func_08817DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4] + 4u;
    const u32 index = ctx.gpr[5];
    ctx.gpr[2] = 0u;
    if (static_cast<s32>(index) < 0) return;
    if (!(static_cast<s32>(index) < static_cast<s32>(word(rt, base + 0xCu)))) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 slot = word(rt, base + 8u) + index;
    if (slot >= 0x64u) slot -= 0x64u;
    ctx.gpr[2] = base + slot * 148u + 0x10u;
}
void func_08818210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    const u32 stack_0 = word(rt, frame.at(0xB0u));
    const u32 stack_1 = byte_at(rt, frame.at(0xB4u));
    const u32 stack_2 = byte_at(rt, frame.at(0xB8u));
    const u32 stack_3 = word(rt, frame.at(0xBCu));
    const u32 record = frame.at(0x0u);
    set_byte(rt, record + 4u, 3u);
    set_byte(rt, record + 5u, stack_0);
    put(rt, record, ctx.gpr[5]);
    put(rt, record + 8u, stack_1);
    set_byte(rt, record + 0xCu, stack_2);
    put(rt, record + 0x10u, ctx.gpr[6]);
    set_byte(rt, record + 0x14u, ctx.gpr[8]);
    put(rt, record + 0x18u, ctx.gpr[10]);
    put(rt, record + 0x1Cu, stack_3);
    call(rt, ctx, 0x08818168u, {ctx.gpr[4], record, ctx.gpr[6], stack_2, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10]});
}
u32 registered_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    constexpr u32 kLock = 0x08CE2AE0u;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {kLock});
    const u32 object = word(rt, slot);
    const u32 result = (object == 0u || word(rt, object + 4u) == 0u) ? 0u : object;
    call(rt, ctx, 0x08B3457Cu, {kLock});
    ctx.gpr[2] = result;
    return result;
}
void func_08820AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { registered_object(rt, ctx, 0x08CE2AD8u); }
void func_08820B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { registered_object(rt, ctx, 0x08CE2ADCu); }
void func_0885D248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 list = word(rt, ctx.gpr[4]);
    const s32 count = static_cast<s32>(word(rt, list + 4u));
    if (count <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 base = word(rt, list);
    u32 found = byte_at(rt, base + 0x15u) != 0u ? 1u : 0u;
    u32 cursor = base + 0x19Du;
    for (s32 index = 1; found == 0u && index != count; ++index, cursor += 0x188u)
        if (byte_at(rt, cursor) != 0u) found = 1u;
    ctx.gpr[2] = found;
}
void func_0885DAB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    u32 length = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        length = call(rt, ctx, 0x08B35F60u, {source});
        for (u32 offset = 0u; offset < 4u; ++offset) set_byte(rt, self + offset, 0u);
    }
    tail_call(ctx, 0x08B7C830u, {self, source, static_cast<s32>(length) < 3 ? length : 3u});
}
void func_0885DB04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    set_byte(rt, self + 0x17u, 1u);
    put(rt, self + 0x24u, 0xFFFFFFFFu);
    set_byte(rt, self + 0x16u, 1u);
    set_byte(rt, self + 0x15u, 0u);
    put(rt, self + 0x1Cu, 0xFFFFFFFFu);
    put(rt, self + 0x20u, 0xFFFFFFFFu);
    for (u32 index = 0u; index < 2u; ++index) {
        set_byte(rt, self + 0x40u + index, 0u);
        put(rt, self + 0x44u + index * 4u, 0xFFFFFFFFu);
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}
void func_0885DD1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = self + 0x120u;
    call(rt, ctx, 0x088A6C18u, {first});
    ctx.gpr[2] = byte_at(rt, self + 0x16u);
    if (ctx.gpr[2] == 0u) {
        call(rt, ctx, 0x088A6C18u, {self + 0x50u, first});
        set_byte(rt, self + 0x16u, 1u);
    }
}
void func_0885DE94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 owner = ctx.gpr[4];
    const u32 argument = ctx.gpr[6];
    const u32 block = call(rt, ctx, 0x08B62344u, {0x60u});
    call(rt, ctx, 0x08894848u, {block, word(rt, owner + 0x5F4u), argument});
    ctx.gpr[2] = block;
}
void func_08884DA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4] + 0x198u;
    if ((ctx.gpr[5] & 0xFFu) != 0u) {
        for (u32 index = 0u; index < 0x1Fu; ++index) put(rt, base + index * 4u, 0xFFFFFFFFu);
        put(rt, base + 0x7Cu, 0xFFFFFFFFu);
        ctx.gpr[2] = base + 0x7Cu;
    } else {
        for (u32 index = 0u; index < 0x20u; ++index) put(rt, base + index * 4u, 0u);
        ctx.gpr[2] = 0x20u;
    }
}
void func_08894770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[6];
    const u32 size = call(rt, ctx, 0x08898230u);
    call(rt, ctx, 0x08B36018u, {destination, 0u, size});
    const u32 value = word(rt, self + 0x54u);
    for (u32 byte = 0u; byte < 4u; ++byte) set_byte(rt, destination + byte, value >> (8u * byte));
    ctx.gpr[2] = destination + call(rt, ctx, 0x08898230u);
}
void func_08896350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, ctx.gpr[5], call(rt, ctx, 0x0892B918u, {word(rt, self + 0x68u)}));
    set_byte(rt, ctx.gpr[6], call(rt, ctx, 0x0892B940u, {word(rt, self + 0x68u)}));
    ctx.gpr[2] = 1u;
}
void func_088A155C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 owner = ctx.gpr[4];
    const u32 limit = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 target = word(rt, owner + 0x54u);
    if (target == 0u || !(static_cast<s32>(limit) < static_cast<s32>(word(rt, owner + 0x14u)))) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x089591A0u, {target, limit, out, out + 4u, out + 8u, out + 0xCu, out + 0x10u, out + 0x14u});
}
void func_088A5748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, self + 0x4C0u);
    ctx.gpr[2] = object;
    if (object != 0u) {
        call(rt, ctx, 0x0892FBE0u, {object});
        const u32 current = word(rt, self + 0x4C0u);
        ctx.gpr[2] = current;
        if (current != 0u) call_indirect(rt, ctx, word(rt, word(rt, current) + 4u), {current});
        put(rt, self + 0x4C0u, 0u);
    }
}
void func_088E732C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    u32 found = 0u;
    ctx.gpr[2] = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 group = word(rt, self + 8u);
        if (group != 0u) found = call(rt, ctx, 0x088DF13Cu, {group, word(rt, self + 4u)});
    }
    if (word(rt, self + 8u) != 0u) tail_call(ctx, 0x088D09A8u, {found, argument});
}
void func_088F5C8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 group = word(rt, self + 8u);
    if (group != 0u) {
        call(rt, ctx, 0x088F5C54u, {self, self + 8u, group});
        call(rt, ctx, 0x088F5BB8u, {self, self + 4u, group});
    }
    ctx.gpr[2] = group;
}
void first_of_two(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 first_function, u32 second_function) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (const u32 first = word(rt, self + 4u); first != 0u) result = call(rt, ctx, first_function, {first});
    else if (const u32 second = word(rt, self); second != 0u) result = call(rt, ctx, second_function, {second});
    ctx.gpr[2] = result;
}
void func_088FABD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { first_of_two(rt, ctx, 0x08903208u, 0x08905500u); }
void func_088FAC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { first_of_two(rt, ctx, 0x089030ECu, 0x08904970u); }
void func_088FC080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x34u), {self});
    u32 result = 0u;
    if (object != 0u && word(rt, word(rt, object + 4u) + 0x2Cu) == 0u)
        result = call(rt, ctx, 0x08906398u, {object + 0x34u}) != 0u ? 1u : 0u;
    ctx.gpr[2] = result;
}
void func_08900D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 record = word(rt, source + 0x88u) + word(rt, ctx.gpr[6]) * 20u;
    const u32 packed = word(rt, record + 0x10u);
    const u32 begin = word(rt, source + 0xA8u) + ((packed & 0xFFFFFFu) << 1);
    put(rt, out, word(rt, source + 0xA0u));
    put(rt, out + 8u, begin + ((packed >> 24) << 1));
    put(rt, out + 4u, begin);
    ctx.gpr[2] = out;
}
void func_0890610C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (self == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08AF9010u, {self, 0u});
    }
    const u32 list = word(rt, self + 0x68u);
    tail_call(ctx, 0x08AF93DCu, {word(rt, list), list, 0x08C8A554u, 2u});
}
void func_08908F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08BD5AF4u, {target, self + 0x1Cu});
        call(rt, ctx, 0x08BD5B6Cu, {target, self + 0x20u, half_at(rt, self + 2u)});
    }
    tail_call(ctx, 0x08BD5C04u, {target, self + 0x24u, half_at(rt, self + 4u)});
}
void func_0892991C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self + 0x20u, 0u);
        put(rt, self + 0x24u, 0u);
        put(rt, self + 0x28u, 0u);
        call(rt, ctx, 0x089298C0u, {self + 0x2Cu});
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x38u, 0u);
        call(rt, ctx, 0x0892B440u, {self});
    }
    tail_call(ctx, 0x089298A8u, {self + 0x2Cu});
}
void func_0892A14C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 end = word(rt, ctx.gpr[4] + 4u);
    const u32 key = ctx.gpr[5];
    u32 found = 0u;
    for (u32 entry = word(rt, ctx.gpr[4]); entry != end; entry += 0x1Cu)
        if (word(rt, entry) == key) {
            found = entry;
            break;
        }
    ctx.gpr[2] = found;
}
void func_0892A1A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 end = word(rt, ctx.gpr[4] + 4u);
    const u32 key = ctx.gpr[5];
    u32 found = 0u;
    for (u32 entry = word(rt, ctx.gpr[4]); entry != end; entry += 0x10u)
        if (half_at(rt, entry) == key) {
            found = entry;
            break;
        }
    ctx.gpr[2] = found;
}
void func_0892A9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flags = ctx.gpr[6];
    const u32 handle = call(rt, ctx, 0x0892A958u, {ctx.gpr[5]});
    if (handle != 0u) {
        set_half(rt, self + 8u, handle);
        const u32 value = ((byte_at(rt, self) | 2u) & ~0x60u) | flags;
        set_byte(rt, self, value);
        ctx.gpr[2] = value;
    }
}
void func_0892AA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 force = ctx.gpr[6] & 0xFFu;
    const u32 handle = call(rt, ctx, 0x0892A958u, {ctx.gpr[5]});
    if (force != 0u || handle != 0u) {
        set_half(rt, self + 0x10u, handle);
        const u32 value = byte_at(rt, self + 0x14u) | 1u;
        set_byte(rt, self + 0x14u, value);
        ctx.gpr[2] = value;
    }
}
void func_08965AC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 member = word(rt, self + 0x20u);
        put(rt, self, 0x08CC6970u);
        ctx.gpr[2] = 0x08CC6970u;
        if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
        call(rt, ctx, 0x089BA380u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x24u, 4u});
}
void func_08965B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[6];
    const u32 text = frame.at(0x0u);
    call(rt, ctx, 0x08A69B3Cu, {text, 0x08CA24B0u});
    call(rt, ctx, 0x08A7BFD0u, {argument, self + 0x24u, text});
    call(rt, ctx, 0x08A69D68u, {text});
}
void func_0899CAF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CBCAB0u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x28u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x24u});
        const u32 member = word(rt, self + 0x20u);
        ctx.gpr[2] = member;
        if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
    }
    tail_call(ctx, 0x089BA380u, {self});
}
void func_089B1820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(0x0u);
    if (ctx.gpr[6] == 0u) {
        call(rt, ctx, 0x089AB370u, {slot});
        const u32 object = word(rt, slot);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
        call(rt, ctx, 0x089B17ACu, {object});
        call(rt, ctx, 0x08A5B9ECu, {slot});
    }
}
void func_089B4A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x089E20D0u, {self});
    put(rt, self, 0x08CCB198u);
    put(rt, self + 0x38u, 0x08CCB3D4u);
    call(rt, ctx, 0x08A39198u, {self + 0x22Cu, 0x08CF6ED8u});
    put(rt, self + 0x234u, 0u);
    set_byte(rt, self + 0x230u, 0u);
}
void func_089B51D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    for (u32 object = word(rt, self + 0x84u); object != 0u; object = word(rt, object + 0x54u))
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1A0u), {object});
    ctx.gpr[2] = word(rt, self + 0x94u);
    put(rt, self + 0x9Cu, ctx.gpr[2]);
}
void func_089B9990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CCA7C8u);
        put(rt, self + 0x38u, 0x08CCA978u);
        call(rt, ctx, 0x089B8884u, {self + 0x64u});
        call(rt, ctx, 0x089BBAC8u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x80u, 4u});
}
void finish_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 out, u32 slot) {
    if (out != slot) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, slot)});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}
void replace_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 replacement = ctx.gpr[5];
    if (replacement != 0u) call(rt, ctx, 0x08A5BA08u, {replacement});
    const u32 previous = word(rt, self + offset);
    ctx.gpr[2] = previous;
    if (previous != 0u) call(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + offset, replacement);
}
void func_089BA708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { replace_reference(rt, ctx, 0x3Cu); }
void func_089D5B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { replace_reference(rt, ctx, 0x24u); }
void func_089BF818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x2B0u; G[17] = G[5]; G[16] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16] + 0x22Cu; G[6] = G[17]; G[5] = 1u; call_here(rt, ctx, 0x08A19BA0u); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089CE964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    if (call(rt, ctx, 0x089CE03Cu, {self, key}) != 0u) set_half(rt, call(rt, ctx, 0x089CE1F4u, {self, key}) + 0x20u, value);
}
u32 record_bit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset, bool half, u32 shift, u32 mask) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    u32 result = 0u;
    if (call(rt, ctx, 0x089CE03Cu, {self, key}) != 0u) {
        const u32 record = call(rt, ctx, 0x089CEFE8u, {self, key});
        result = ((half ? half_at(rt, record + offset) : word(rt, record + offset)) >> shift) & mask;
    }
    ctx.gpr[2] = result;
    return result;
}
void func_089CF030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_bit(rt, ctx, 0x20u, false, 20u, 1u); }
void func_089CF0E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_bit(rt, ctx, 0x20u, false, 19u, 1u); }
void func_089CF874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_bit(rt, ctx, 0x20u, false, 18u, 1u); }
void func_089CF924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_bit(rt, ctx, 0x20u, false, 17u, 1u); }
void func_089CFBAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_bit(rt, ctx, 0x22u, true, 0u, 1u); }
void func_089FFDE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC73A8u);
        put(rt, self + 0x38u, 0x08CC7588u);
        call(rt, ctx, 0x089B8884u, {self + 0x78u});
        call(rt, ctx, 0x08A101F8u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x9Cu, 4u});
}
void copy_then_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 target = self + offset;
        if (target != source) {
            call(rt, ctx, 0x089641C4u, {target, source});
            put(rt, target + 0x10u, word(rt, source + 0x10u));
        }
    }
    tail_call(ctx, tail, {self});
}
void func_08A07660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_then_tail(rt, ctx, 0x180u, 0x08A0761Cu); }
void func_08A0888C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_then_tail(rt, ctx, 0x130u, 0x08A075E4u); }
void func_08A088E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_then_tail(rt, ctx, 0x160u, 0x08A0761Cu); }
void func_08A1A914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08AD023Cu, {self});
        put(rt, self, 0x08CD0698u);
        put(rt, self + 0x64u, 0u);
        call(rt, ctx, 0x08B28DCCu, {self + 0x68u});
        put(rt, self + 0x90u, 0u);
        call(rt, ctx, 0x08ABFF0Cu, {self + 0x44u, 1u});
    }
    tail_call(ctx, 0x08ABFF0Cu, {self + 0x30u, 1u});
}
void func_08A2159C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = word(rt, self + 0x18u);
    ctx.gpr[2] = first;
    if (first != 0u) {
        call(rt, ctx, 0x08ABBEF8u, {first, 0u});
        put(rt, self + 0x18u, 0u);
    }
    const u32 second = word(rt, self + 0x14u);
    ctx.gpr[2] = second;
    if (second != 0u) {
        call(rt, ctx, 0x08ABC584u, {second, 0u});
        put(rt, self + 0x14u, 0u);
    }
}
void func_08A25A48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 mode = call(rt, ctx, 0x08B49688u, {word(rt, 0x08D69524u)});
    call(rt, ctx, 0x08A85B1Cu, {slot, (mode ^ 1u) == 0u ? 1u : 0u});
    finish_holder(rt, ctx, out, slot);
}
void func_08A2AD60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 value = call(rt, ctx, 0x08B327ACu, {0x08D69170u, 0u});
    call(rt, ctx, 0x08A97A6Cu, {slot, value});
    finish_holder(rt, ctx, out, slot);
}
void func_08A2F494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const GuestResult pair = call_guest(rt, ctx, 0x08B0A744u, {word(rt, 0x08CF6648u)}, {});
    ctx.gpr[2] = pair.v0;
    call(rt, ctx, 0x08A83160u, {slot, ctx.gpr[5], pair.v0, pair.v1});
    finish_holder(rt, ctx, out, slot);
}
void func_08A32A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB8AC8u);
    call(rt, ctx, 0x08A431F8u, {self + 0x14u});
    call(rt, ctx, 0x08A32A14u, {self + 0x18u, self + 0x14u});
    put(rt, self + 0x42Cu, 0u);
}
void func_08A44598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A4455Cu);
    set_byte(rt, self + 0x1Cu, ctx.gpr[7] & 0xFFu);
    put(rt, self + 0x14u, ctx.gpr[5]);
    put(rt, self + 0x18u, ctx.gpr[6]);
}
void func_08A44C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 handle = word(rt, source + 0x14u);
    if (handle == 0u) put(rt, out, 0x08D74FDCu);
    else call(rt, ctx, 0x08C00AD4u, {out, handle, word(rt, source + 0x18u), frame.at(0x0u)});
    ctx.gpr[2] = out;
}
void make_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 initialise) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 node = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, initialise, {node, 0u});
    put(rt, slot, node);
    call(rt, ctx, 0x08A76AB4u, {out, slot});
}
void func_08A5FBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08A5F3F8u); }
void func_08A734B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08A72D38u); }
void func_08A85C88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08A854CCu); }
void func_08A9622C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08A95AACu); }
void func_08A97BDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08A973BCu); }
void func_08AB0398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08AAFBF8u); }
void func_08AB1B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08AB137Cu); }
// `this->counter` (word, signed half or signed byte at +0x14) stepped by `delta`, then `target(out, old or new value)`
// into a holder that is moved to `out` (a1).
void counter_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, unsigned width, u32 delta, bool pass_new) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 slot = frame.at(0x0u);
    const u32 old = width == 4u ? word(rt, self + 0x14u)
                  : width == 2u ? static_cast<u32>(signed_half_at(rt, self + 0x14u))
                                : static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, self + 0x14u)));
    const u32 stepped = old + delta;
    if (width == 4u) put(rt, self + 0x14u, stepped);
    else if (width == 2u) set_half(rt, self + 0x14u, stepped);
    else set_byte(rt, self + 0x14u, stepped);
    call(rt, ctx, target, {slot, pass_new ? stepped : old});
    finish_holder(rt, ctx, out, slot);
}
void func_08A604CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A5FA8Cu, 4u, 1u, true); }
void func_08A60520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A5FA8Cu, 4u, 0xFFFFFFFFu, true); }
void func_08A60574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A5FA8Cu, 4u, 1u, false); }
void func_08A605C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A5FA8Cu, 4u, 0xFFFFFFFFu, false); }
void func_08A73DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A85B1Cu, 2u, 1u, false); }
void func_08A73E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A85B1Cu, 2u, 0xFFFFFFFFu, false); }
void func_08A8654C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A85B1Cu, 4u, 0xFFFFFFFFu, true); }
void func_08A96BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step(rt, ctx, 0x08A85B1Cu, 1u, 0xFFFFFFFFu, false); }
void func_08A73298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, ctx.gpr[5]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0BA8u});
    set_half(rt, self + 0x14u, call_indirect(rt, ctx, word(rt, word(rt, object) + 0x38u), {object}));
}
void toggled_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 slot = frame.at(0x0u);
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x30u), {self});
    call(rt, ctx, 0x08A97A6Cu, {slot, (value ^ 1u) & 0xFFu});
    finish_holder(rt, ctx, out, slot);
}
void func_08A88758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { toggled_virtual(rt, ctx); }
void func_08A8BA3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { toggled_virtual(rt, ctx); }
void func_08A8BA90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 slot = frame.at(0x0u);
    call(rt, ctx, 0x08A8B098u, {slot, out, word(rt, self + 0x18u), word(rt, self + 0x1Cu) ^ 0x80000000u});
    finish_holder(rt, ctx, out, slot);
}
void func_08A99198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(0x0u);
    if (ctx.gpr[6] == 0u) {
        call(rt, ctx, 0x08A98674u, {slot});
        const u32 object = word(rt, slot);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB23CCu});
        call(rt, ctx, 0x08A990D4u, {object});
        call(rt, ctx, 0x08A5B9ECu, {slot});
    }
}
void func_08835168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x08A0F560u); if (G[16] != 0u) { ctx.fpr[0] = float_at(rt, G[16] + 0x84u); G[2] = load8(G[16] + 0x80u); G[3] = load8(G[16] + 0x81u); put_float(rt, G[17] + 0x84u, ctx.fpr[0]); store8(G[17] + 0x80u, G[2]); store8(G[17] + 0x81u, G[3]); put(rt, G[17] + 0x78u, 0u); } } G[16] = s0; G[17] = s1; }
void func_088438D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088435A8u); G[4] = 0x08C9C7CCu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08843404u); G[4] = G[2]; call_here(rt, ctx, 0x088D43A8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08843924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088435A8u); G[4] = 0x08C9C7CCu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08843404u); G[4] = G[2]; call_here(rt, ctx, 0x088D43E4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08854B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088547C4u); G[4] = 0x08C9E188u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[2]; call_here(rt, ctx, 0x088A627Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_088A86BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; ctx.fpr[12] = float_at(rt, G[4] + 0x10u); G[5] = word(rt, G[4] + 4u); G[6] = word(rt, G[4] + 8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B4FC34u); ctx.fpr[2] = float_at(rt, frame.at(4u)); ctx.fpr[3] = float_at(rt, frame.at(8u)); ctx.fpr[1] = float_at(rt, frame.at(0xCu)); ctx.fpr[0] = float_at(rt, frame.at(0u)); put_float(rt, G[16] + 4u, ctx.fpr[2]); put_float(rt, G[16], ctx.fpr[0]); put_float(rt, G[16] + 0xCu, ctx.fpr[1]); put_float(rt, G[16] + 8u, ctx.fpr[3]); } G[16] = s0; }
void func_088BB3F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[5]); ctx.fpr[1] = float_at(rt, G[2] + 0x1408u); ctx.fpr[0] = float_at(rt, G[2] + 0x1404u); ctx.fpr[2] = float_at(rt, G[2] + 0x13CCu); G[2] = 7u; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[2]; put_float(rt, G[4] + 0x44u, ctx.fpr[0]); G[5] = word(rt, G[5]); G[3] = load8(G[5] + 0x140Fu); ctx.fpr[1] = 0.0f; if (G[3] == G[2]) { ctx.fpr[0] = float_at(rt, G[5] + 0x141Cu); G[1] = 0x3F4CCCCDu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[2] = float_at(rt, G[5] + 0x13CCu); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[1] = ctx.fpr[0] / ctx.fpr[2]; } put_float(rt, G[4] + 0x48u, ctx.fpr[1]); }
void func_088D0A60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; ctx.fpr[12] = float_at(rt, G[4]); ctx.fpr[13] = float_at(rt, G[4] + 4u); G[4] = G[5]; G[5] = 0u; call_here(rt, ctx, 0x088FB100u); ctx.fpr[1] = 0.0f; G[2] = 1u; store8(G[16] + 0x3Cu, ctx.fpr[1] <= ctx.fpr[0] ? 0u : G[2]); } G[16] = s0; }
void func_088F10BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = G[5] - 1u; G[2] = G[2] < 0x64u ? 1u : 0u; G[3] = 0u; if (G[2] != 0u) { const bool zero = G[4] == 0u; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * static_cast<std::int32_t>(G[5]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); if (!zero) { G[2] = 0x51EB851Fu; G[3] = ctx.lo; const std::uint64_t wide = static_cast<std::uint64_t>(G[3]) * G[2]; ctx.lo = static_cast<std::uint32_t>(wide); ctx.hi = static_cast<std::uint32_t>(wide >> 32); G[2] = ctx.hi >> 4; G[4] = G[2] << 2; G[3] = G[2] << 7; G[3] -= G[4]; G[3] += G[2]; G[3] <<= 2; G[2] = 0x1F4u; if (G[3] == 0u) G[3] = G[2]; } } G[2] = G[3]; }
void func_088F73EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[4] + 0x100u); G[2] = 0x08CDD79Cu; G[3] = (G[3] << 3) + G[2]; G[25] = word(rt, G[3]); G[3] = word(rt, G[3] + 4u); if (G[25] == 0u) { G[2] = word(rt, G[4] + 0x100u); G[2] &= ~0x100u; put(rt, G[4] + 0x100u, G[2]); return; } G[7] = sra(G[3], 1u); G[3] &= 1u; G[4] += G[7]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[25]; G[25] = word(rt, G[2]); } jump(ctx, G[25]); }
void func_088F7878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[13]; G[1] = 0x43340000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (ctx.fpr[0] < ctx.fpr[12]) { G[1] = 0x43B40000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[12] = ctx.fpr[12] - ctx.fpr[0]; } G[1] = 0xC3340000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (ctx.fpr[12] < ctx.fpr[0]) { G[1] = 0x43B40000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[0]; } ctx.fpr[0] = ctx.fpr[12]; }
void func_08925A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC6078u; G[1] = 0x3DCCCCCDu; ctx.fpr[0] = std::bit_cast<float>(G[1]); put(rt, G[4], G[2]); put_float(rt, G[4] + 0x20u, ctx.fpr[0]); put(rt, G[4] + 0x2Cu, 0u); put(rt, G[4] + 0x8u, 0u); put(rt, G[4] + 0x4u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x30u, 0u); put(rt, G[4] + 0x34u, 0u); put(rt, G[4] + 0x38u, 0u); put(rt, G[4] + 0x1Cu, 0u); put(rt, G[4] + 0x24u, 0u); put(rt, G[4] + 0x28u, 0u); }
void func_08925AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC6078u; G[1] = 0x3DCCCCCDu; ctx.fpr[0] = std::bit_cast<float>(G[1]); put(rt, G[4], G[2]); put_float(rt, G[4] + 0x20u, ctx.fpr[0]); put(rt, G[4] + 0x2Cu, 0u); put(rt, G[4] + 0x8u, 0u); put(rt, G[4] + 0x4u, 0u); put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x30u, 0u); put(rt, G[4] + 0x34u, 0u); put(rt, G[4] + 0x38u, 0u); put(rt, G[4] + 0x1Cu, 0u); put(rt, G[4] + 0x24u, 0u); put(rt, G[4] + 0x28u, 0u); }
void func_0894BFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CDE690u); G[3] = load8(0x08CDE690u); if (G[3] == 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0xD0u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); return; } G[2] = sext(load16(G[4]), 16u); G[1] = 0x3DE38E3Au; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[2] = psp_mul(ctx.fpr[12], ctx.fpr[12]); G[2] = (G[2] << 2) + G[4]; ctx.fpr[1] = float_at(rt, G[2] + 8u); ctx.fpr[3] = float_at(rt, G[4] + 0xD0u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[1]); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[0]; ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[2]); ctx.fpr[0] = ctx.fpr[3] / ctx.fpr[1]; }
void func_08965D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[4]); G[1] = 0xBF800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[2] = 0u; ctx.fpr[1] = ctx.fpr[12] - ctx.fpr[3]; const bool low = ctx.fpr[1] <= ctx.fpr[0]; ctx.fpr[2] = psp_mul(ctx.fpr[1], ctx.fpr[13]); if (!low) { G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (!(ctx.fpr[0] <= ctx.fpr[1])) { G[2] = 1u; put_float(rt, G[4], ctx.fpr[12]); return; } } ctx.fpr[0] = ctx.fpr[3] + ctx.fpr[2]; put_float(rt, G[4], ctx.fpr[0]); }
void func_089897D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[4] = 0x08CA39F0u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16] + 0xF0u; G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x089833D0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_089B7178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCAC10u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CCAE6Cu; put(rt, G[16] + 0x38u, G[2]); G[4] += 0x134u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x130u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x12Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089A29E4u); }
void func_089BFDA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCA568u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CCA7A8u; put(rt, G[16] + 0x38u, G[2]); G[4] += 0x2B0u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0x264u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x22Cu; call_here(rt, ctx, 0x08A19DD4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x089E2430u); }
void func_089C1EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[4]); ctx.fpr[1] = ctx.fpr[0] - ctx.fpr[2]; const bool same = ctx.fpr[2] == ctx.fpr[0]; ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[12]); if (same) return; ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[0]; G[1] = 0x3C23D70Au; ctx.fpr[1] = std::bit_cast<float>(G[1]); put_float(rt, G[4], ctx.fpr[0]); ctx.fpr[2] = float_at(rt, G[5]); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[2]; ctx.fpr[0] = std::bit_cast<float>(bits_of(ctx.fpr[0]) & 0x7FFFFFFFu); if (ctx.fpr[0] < ctx.fpr[1]) put_float(rt, G[4], ctx.fpr[2]); }
void func_08A2B650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = 0x08D69170u; call_here(rt, ctx, 0x08B319FCu); G[2] ^= 1u; G[4] = frame.at(0u); G[5] = G[2] == 0u ? 1u : 0u; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A331D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x16u); G[4] = G[2] - 1u; G[3] = sra(G[4], 31u); G[2] = 0u; if (static_cast<std::int32_t>(G[4]) >= 0) { G[2] = 0x2AAAAAABu; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[2] = ctx.hi; G[2] = sra(G[2], 1u); G[2] -= G[3]; G[3] = G[2] << 4; G[2] <<= 2; G[3] -= G[2]; G[2] = G[4] - G[3]; } G[3] = G[2] << 2; G[2] = 0x08CDECFCu; G[3] += G[2]; G[2] = word(rt, G[3]); }
void func_08A35160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[17] = G[5]; call_here(rt, ctx, 0x08A35114u); G[4] = 0x08CACEC8u; G[16] = word(rt, G[16] + 0x1Cu); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[17]; G[6] = 0u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A3A73Cu); }
void func_08A3A4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 0.0f; const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; if (ctx.fpr[0] < ctx.fpr[12]) tail = true; else { G[2] = word(rt, G[4] + 0x18u); G[4] = G[2]; if (G[2] != 0u) { put(rt, G[16] + 0x18u, 0u); call_here(rt, ctx, 0x08ABCC1Cu); } store8(G[16] + 0x2Cu, 0u); } } G[16] = s0; if (tail) jump(ctx, 0x08A3A480u); }
void func_08A3AEE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBB9A0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A3A6FCu); G[4] = G[16] + 0x34u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x38u; G[6] = 4u; } G[16] = s0; jump(ctx, 0x08A5BC54u); }
void func_08A6AD7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x11Cu); G[16] = G[4]; call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = G[16] + 0x1Cu; call_here(rt, ctx, 0x08B855E0u); G[3] = 1u; put(rt, G[16] + 0x18u, G[3]); G[2] = 1u; put(rt, G[16] + 0x14u, 0u); } G[16] = s0; G[17] = s1; }
void func_08A704C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A70338u); G[4] = 0x08CB0928u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x10Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A71F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A71E70u); G[4] = 0x08CB0A94u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x120u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A9A048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[16] = G[4]; G[4] = G[2]; G[3] = word(rt, G[5] + 0x14u); G[7] = word(rt, G[5] + 0x18u); G[5] = word(rt, G[6]); G[6] = 0u; G[7] -= G[3]; G[7] = sra(G[7], 2u); call_here(rt, ctx, 0x08A98CDCu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[2] = G[16]; } G[16] = s0; }
void func_08AB0178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CB3A78u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x48u); G[4] = G[16]; call_here(rt, ctx, G[2]); store16(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08AB18FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CB3AF0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08AB0CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = G[4]; G[16] = G[5]; G[6] = load16(G[4] + 0x14u); G[4] = frame.at(0u); G[2] = G[6] + 0x1u; store16(G[3] + 0x14u, G[2]); G[5] = G[6]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB0D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = G[4]; G[16] = G[5]; G[6] = load16(G[4] + 0x14u); G[4] = frame.at(0u); G[2] = G[6] + 0xFFFFFFFFu; store16(G[3] + 0x14u, G[2]); G[5] = G[6]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB2468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = G[4]; G[16] = G[5]; G[6] = load8(G[4] + 0x14u); G[4] = frame.at(0u); G[2] = G[6] + 0x1u; store8(G[3] + 0x14u, G[2]); G[5] = G[6]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08AB24BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[3] = G[4]; G[16] = G[5]; G[6] = load8(G[4] + 0x14u); G[4] = frame.at(0u); G[2] = G[6] + 0xFFFFFFFFu; store8(G[3] + 0x14u, G[2]); G[5] = G[6]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08ABCD8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08ABB2E4u); G[2] = high_half(0x08D77190u); G[16] = word(rt, 0x08D77190u); while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[16] = word(rt, G[16]); } } G[16] = s0; }
void func_08AC2910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; store8(G[5], G[6]); call_here(rt, ctx, 0x08AC287Cu); G[4] = G[16]; G[3] = 0u; if (G[2] == 0u) { call_here(rt, ctx, 0x08AC2894u); store8(G[17], G[2]); G[3] = 1u; } G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_08AC37C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x18Cu; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[2] = load8(G[16] + 0x1B4u); if (G[2] != 0u) { call_here(rt, ctx, 0x08AC2CDCu); store8(G[16] + 0x1B5u, 0u); } G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_08AC653C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AC4ED4u); G[4] = G[16]; call_here(rt, ctx, 0x08AC5014u); G[4] = G[16] + 0x25C0u; put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); call_here(rt, ctx, 0x08C3CC54u); G[4] = G[16] + 0x460u; call_here(rt, ctx, 0x08AC64FCu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 8u; } G[16] = s0; jump(ctx, 0x08B28DC4u); }
void func_08AC7820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = 0.0f; const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08AC76E8u); G[4] = G[16]; call_here(rt, ctx, 0x08AC77B0u); G[4] = G[16] + 0x5BCu; put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); call_here(rt, ctx, 0x08AC718Cu); G[4] = G[16] + 0x578u; call_here(rt, ctx, 0x08C3CCD0u); G[4] = G[16] + 0x438u; call_here(rt, ctx, 0x08B0C808u); G[4] = G[16] + 8u; } G[16] = s0; jump(ctx, 0x08B28BE0u); }
void func_08AC9240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B1FC48u, {0x08D00114u, self});
        if (self != 0u) {
            call(rt, ctx, 0x08AD380Cu, {self});
            tail = true;
        }
    }
    if (tail) tail_call(ctx, 0x08B64654u, {self});
}
void func_08ACA9B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[4] = high_half(0x08D003E8u); G[2] = 0xFFFFu; G[5] = 2u; G[6] = 0x500u; G[4] = 0x08D003E8u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08B2EE10u); G[4] = 0x08D001B4u; tail = true; } } } if (tail) jump(ctx, 0x08ACA4A4u); }
void func_08ACB168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABFD0Cu, {self, 0x08D77204u, 0x08D771D0u});
    put(rt, self, 0x08CD6970u);
    call(rt, ctx, 0x08B28D34u, {self + 0x60u});
    put(rt, self + 0x90u, 0u);
    set_byte(rt, self + 0x8Cu, 0u);
    set_byte(rt, self + 0x8Du, 0u);
}
void func_08ACC93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); jump(ctx, 0x08ACC8F0u); }
void func_08AD23E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[13]); G[1] = 0x40800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[1] = psp_mul(ctx.fpr[12], ctx.fpr[12]); ctx.fpr[2] = 0.0f; ctx.fpr[13] = psp_mul(ctx.fpr[13], ctx.fpr[0]); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[13]; ctx.fpr[1] = std::sqrt(ctx.fpr[1]); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); if (ctx.fpr[12] < ctx.fpr[2]) ctx.fpr[12] = ctx.fpr[2]; ctx.fpr[1] = float_at(rt, G[4] + 0x25Cu); ctx.fpr[0] = float_at(rt, G[4] + 0x258u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[0]); }
void func_08AD3BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 key = ctx.gpr[4];
    const u32 head = word(rt, 0x08D404CCu);
    if (head == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 found;
    {
        GuestFrame frame(ctx, 0x10u);
        found = call(rt, ctx, 0x08AD3970u, {head, key});
    }
    tail_call(ctx, 0x08AD393Cu, {head, found});
}
void func_08AD78C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = psp_mul(ctx.fpr[13], ctx.fpr[14]); put(rt, frame.at(0x14u), bits_of(f21)); ctx.fpr[21] = ctx.fpr[12]; ctx.fpr[12] = ctx.fpr[13] / ctx.fpr[14]; ctx.fpr[20] = std::sqrt(ctx.fpr[20]); G[16] = G[4]; call_here(rt, ctx, 0x08B847A8u); ctx.fpr[14] = ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[21]; ctx.fpr[13] = ctx.fpr[20]; G[4] = G[16]; } G[16] = s0; ctx.fpr[21] = f21; ctx.fpr[20] = f20; jump(ctx, 0x08AD77F0u); }
void func_08AD8C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[4] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[4] = G[3]; G[7] = 0u; if (G[4] != 0u) { G[6] = G[5]; do { G[7] += 1u; if (G[5] != 0u) { G[2] = word(rt, G[4] + 8u); put(rt, G[6], G[2]); } G[3] = word(rt, G[4] + 4u); G[6] += 4u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; } while (G[3] != 0u); } G[2] = G[7]; }
void func_08ADFD8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = G[4]; G[16] = G[5]; do { G[4] = word(rt, G[17]); G[5] = G[18]; G[16] -= 1u; call_here(rt, ctx, 0x08ADFCECu); G[17] += 4u; } while (G[16] != 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AE2EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D40930u); G[3] = word(rt, 0x08D40930u); G[16] = 0x08D40930u; if (G[3] != 0u) { G[4] = word(rt, G[16] + 0x24u); call_here(rt, ctx, 0x08AD85C8u); G[4] = word(rt, G[16] + 0x3Cu); call_here(rt, ctx, 0x08AD85C8u); G[4] = G[16]; G[5] = 0u; G[6] = 0x48u; call_here(rt, ctx, 0x08B36018u); } } G[16] = s0; }
void func_08AE3568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4] + 0x18u); G[4] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[4] = G[3]; while (G[4] != 0u) { G[16] = word(rt, G[4] + 4u); G[2] = word(rt, G[16] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[16] = 0u; call_here(rt, ctx, 0x08AE34A8u); G[4] = G[16]; } } G[16] = s0; }
void func_08AE3C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = G[5] < 0x28u ? 1u : 0u; G[7] = static_cast<std::int32_t>(G[5]) < 0x20 ? 1u : 0u; G[3] = G[4]; if (G[2] != 0u) { G[8] = G[5] << 2; G[4] = G[5]; G[5] = G[6]; if (G[7] != 0u) call_here(rt, ctx, 0x08ADEA5Cu); else { G[3] = word(rt, G[3] + 0x10u); G[2] = word(rt, G[3] + 0x10u); G[2] = G[8] + G[2]; put(rt, G[2] + 0x30u, G[6]); } } } }
void func_08AE4634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 6u; G[2] = 9u; { GuestFrame frame(ctx, 0x50u); if (G[6] == 0u) G[3] = G[2]; G[6] = G[5]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[3]); put(rt, frame.at(4u), G[6]); put(rt, frame.at(8u), 0u); if (G[7] != 0u) call_here(rt, ctx, 0x08AE4568u); else { G[4] = sext(load16(G[4] + 0x1Au), 16u); G[5] = frame.at(0u); call_here(rt, ctx, 0x08AE85E8u); } } }
void func_08AE69A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] - 0xDu; G[3] = G[5] << 1; G[2] = G[2] < 0x18u ? 1u : 0u; G[3] += G[4]; if (G[2] != 0u) { G[2] = sext(load16(G[3]), 16u); return; } G[3] = sext(load16(G[4] + 2u), 16u); G[2] = G[5] << 1; G[6] = G[2] + G[4]; G[3] = sra(G[3], G[5] & 31u); G[2] = high_half(0x08CDFD84u); G[4] = G[5] << 2; G[2] = 0x08CDFD84u; G[3] &= 1u; G[2] = G[4] + G[2]; if (G[3] != 0u) G[2] = sext(load16(G[6]), 16u); else G[2] = word(rt, G[2]); }
void func_08AFBAFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD7738u);
        put(rt, self + 0x28u, 0x08CD777Cu);
        call(rt, ctx, 0x08AFACA4u, {self});
        put(rt, self + 0x28u, 0x08CBF428u);
        call(rt, ctx, 0x08AFB02Cu, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_08B0162C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[7] = G[4] + 0x38u; G[4] = G[7]; put(rt, frame.at(0u), G[5]); G[3] = word(rt, G[7] + 4u); G[2] = word(rt, G[7] + 8u); G[6] = frame.at(0u); if (G[3] == G[2]) { G[5] = G[3]; call_here(rt, ctx, 0x08C47468u); } else if (G[3] == 0u) { G[3] += 4u; put(rt, G[7] + 4u, G[3]); } else { put(rt, G[3], G[5]); G[3] = word(rt, G[7] + 4u); G[3] += 4u; put(rt, G[7] + 4u, G[3]); } } }
void func_08B084F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = 0x10u; G[17] = 0u; G[16] = G[4] + 0x20u; do { G[2] = word(rt, G[16] + 0xCu); G[17] += 1u; if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x10u); call_here(rt, ctx, G[2]); } G[16] += 8u; } while (G[17] != G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B09A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08D473C0u); G[16] = G[4]; G[4] = 0x08D473C0u; call_here(rt, ctx, 0x08B344E0u); G[3] = word(rt, G[16]) + 0xD4u; G[2] = load8(G[3] + 0x29u); G[4] = 0x08D473C0u; if (G[2] != 0u) { G[2] = load8(G[3] + 0x28u); if (G[2] != 0u) store8(G[3] + 0x28u, 0u); } } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_08B0C56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0C43Cu); G[4] = G[2]; G[5] = G[16]; if (G[2] != 0u) { G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x8Cu); G[25] = G[2]; tail = true; } else { G[2] = 0xFFFFFFFFu; G[3] = 0xFFFFFFFFu; } } G[16] = s0; if (tail) jump(ctx, G[25]); }
void func_08B0DD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 4u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[2] = 1u; G[18] = load8(G[16] + 0xE8u); G[4] = G[17]; store8(G[16] + 0xE8u, G[2]); call_here(rt, ctx, 0x08B34B24u); G[2] = G[18] ^ 1u; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B15C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B0CDC8u); G[2] = 0x08B15FC0u; G[3] = high_half(0x08CD9070u); put(rt, G[16] + 0x18u, G[2]); G[3] = 0x08CD9070u; G[2] = high_half(0x08D4F8A8u); put(rt, G[16], G[3]); G[2] = 0x08D4F8A8u; G[3] = 0x200u; put(rt, G[16] + 0x34u, G[2]); put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0x1Cu, 0u); } G[16] = s0; }
void func_08B21484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5] << 1;
    const u32 low = call(rt, ctx, 0x08B21458u, {self, index});
    const u32 high = call(rt, ctx, 0x08B21458u, {self, index + 1u});
    ctx.gpr[2] = low | (high << 8);
}
void func_08B33080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 4u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B332DCu); G[4] = G[17]; call_here(rt, ctx, 0x08B34D40u); G[3] = word(rt, frame.at(4u)); G[2] = word(rt, frame.at(0u)); G[4] = G[17]; put(rt, G[16] + 0x10u, G[3]); put(rt, G[16] + 0xCu, G[2]); call_here(rt, ctx, 0x08B34D80u); } G[16] = s0; G[17] = s1; }
void func_08B33898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D6925Cu); G[6] = word(rt, 0x08D6925Cu); if (static_cast<std::int32_t>(G[6]) <= 0) { G[5] = 0xFFFFFFFFu; G[2] = G[5]; return; } G[2] = high_half(0x08D6921Cu); G[3] = word(rt, 0x08D6921Cu); G[5] = 0u; if (G[4] == G[3]) { G[2] = G[5]; return; } G[2] = high_half(0x08D69220u); G[3] = 0x08D69220u; for (;;) { G[5] += 1u; if (G[5] == G[6]) { G[5] = 0xFFFFFFFFu; break; } G[2] = word(rt, G[3]); G[3] += 4u; if (G[2] == G[4]) break; } G[2] = G[5]; }
void func_08B3E0AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B36290u); G[2] = 0x08CDA980u; put(rt, G[16], G[2]); put(rt, G[16] + 0x20u, G[17]); put(rt, G[16] + 0x24u, G[18]); put(rt, G[16] + 0x28u, 0u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B3E5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x58u); G[5] = G[2]; if (G[2] != 0u) { G[4] = word(rt, G[4] + 0x5Cu); call_here(rt, ctx, 0x08B2CE7Cu); } G[2] = word(rt, G[16] + 4u); G[5] = G[2]; if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x5Cu); tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B2CE7Cu); }
void func_08B4AD10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[3] = float_at(rt, G[6] + 4u); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[1] = float_at(rt, G[5] + 0xCu); ctx.fpr[4] = float_at(rt, G[5] + 0x18u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[3]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[4]; put_float(rt, G[4], ctx.fpr[0]); ctx.fpr[4] = float_at(rt, G[6] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 4u); ctx.fpr[2] = float_at(rt, G[6]); ctx.fpr[1] = float_at(rt, G[5] + 0x10u); ctx.fpr[3] = float_at(rt, G[5] + 0x1Cu); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[4]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[3]; put_float(rt, G[4] + 4u, ctx.fpr[0]); }
void func_08B547B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CB619Cu); ctx.fpr[0] = float_at(rt, 0x08CB619Cu); G[7] = G[4] + 0x18u; G[3] = G[7]; put_float(rt, G[4] + 0xCu, ctx.fpr[0]); G[5] = 0xC0u; G[6] = 0xCBu; put(rt, G[4], 0u); put_float(rt, G[4] + 8u, ctx.fpr[0]); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); do { G[2] = G[5] << 24; G[5] += 1u; put(rt, G[3], G[2]); G[3] += 4u; } while (G[5] != G[6]); G[2] = 0xFFFFFFFFu; G[3] = 1u; store8(G[7] + 0x15u, G[2]); store8(G[7] + 0xDu, G[3]); }
void func_08B569AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = word(rt, G[4] + 0xD10u); G[7] = G[5]; G[2] = high_half(0x08C9565Cu); G[3] = high_half(0x08C9566Cu); G[8] = G[4]; G[9] = G[4] + 0xD20u; G[5] <<= 1; G[4] = G[7] << 2; G[2] = 0x08C9565Cu; G[3] = 0x08C9566Cu; G[2] = G[4] + G[2]; G[5] += G[3]; if (G[7] != G[6]) { G[3] = load16(G[5]); G[2] = word(rt, G[2]); put(rt, G[8] + 0xD10u, G[7]); store16(G[8] + 0xD20u, G[3]); put(rt, G[9] + 4u, G[2]); store16(G[9] + 2u, G[3]); } }
void func_08B59BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] <<= 4; G[2] = G[6] + G[4]; G[8] = load8(G[2] + 2u); G[7] = load8(G[2] + 3u); G[2] = 0xFFu; G[3] = (G[8] << 4) + G[4]; if (G[8] == G[2]) store8(G[5], G[7]); else store8(G[3] + 3u, G[7]); G[2] = 0xFFu; G[3] = G[7] << 4; if (G[7] == G[2]) store8(G[5] + 1u, G[8]); else { G[2] = G[3] + G[4]; store8(G[2] + 2u, G[8]); } G[2] = G[6] + G[4]; store8(G[2], 0u); }
void func_08B646D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CDC110u);
        call(rt, ctx, 0x08B61BC8u, {self + 8u});
        put(rt, self, 0x08CDB768u);
        call(rt, ctx, 0x08B5C280u, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_08B86B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C97DA8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08B89EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C98D34u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x4Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08B889D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C9832Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x14u, ctx.fpr[0]); } G[16] = s0; G[17] = s1; }
void func_08B8718C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CB8400u);
        call(rt, ctx, 0x08A5B9ECu, {self + 0x38u});
        put(rt, self, 0x08CB7440u);
        call(rt, ctx, 0x08A7D4E4u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x44u, 4u});
}
void func_08B88D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[18] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[6] = word(rt, G[16] + 0x18u); G[17] = G[2]; G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08B88CD4u); G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B8C39C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[5];
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A5B9ECu, {object + 0xCu});
        call(rt, ctx, 0x08A5B9ECu, {object + 8u});
        call(rt, ctx, 0x0885E31Cu, {object});
    }
    tail_call(ctx, 0x08B64654u, {object});
}
void func_08B93BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[6]; G[16] = word(rt, G[4]); G[3] = word(rt, G[16] - 0xCu); G[4] = G[16] + G[6]; G[6] = G[3] - G[6]; G[3] = G[2] < G[3] ? 1u : 0u; G[5] = sext(G[5] & 0xFFu, 8u); if (G[3] == 0u) G[4] = 0xFFFFFFFFu; else { call_here(rt, ctx, 0x08B79998u); G[4] = G[2] - G[16]; if (G[2] == 0u) G[4] = 0xFFFFFFFFu; } G[2] = G[4]; } G[16] = s0; }
void func_08B9737C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    excess_over_128(rt, ctx);
}
void func_08B99550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = word(rt, G[4] - 0xCu); G[5] = sext(G[5] & 0xFFu, 8u); if (G[2] == 0u) { G[3] = 0xFFFFFFFFu; G[2] = G[3]; return; } G[2] -= 1u; G[3] = G[6] < G[2] ? 1u : 0u; if (G[3] != 0u) G[2] = G[6]; G[7] = G[2] + 1u; G[4] += G[7]; G[3] = G[7]; G[6] = 0u; for (;;) { const bool more = G[7] != G[6]; G[3] -= 1u; if (!more) { G[3] = 0xFFFFFFFFu; break; } G[2] = sext(load8(G[4] - 1u), 8u); G[6] += 1u; G[4] -= 1u; if (G[2] == G[5]) break; } G[2] = G[3]; }
void func_08B9EB34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0DF8u; put(rt, G[2], G[3]); G[3] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 0xCu); put(rt, G[2] + 8u, G[3]); put(rt, G[2] + 4u, G[5]); put(rt, G[2] + 0xCu, G[6]); G[3] = load8(G[16] + 0x10u); store8(G[2] + 0x10u, G[3]); } G[16] = s0; }
void func_08BC32D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC4D00u; put(rt, G[2], G[3]); G[3] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 0xCu); put(rt, G[2] + 8u, G[3]); put(rt, G[2] + 4u, G[5]); put(rt, G[2] + 0xCu, G[6]); G[3] = load8(G[16] + 0x10u); store8(G[2] + 0x10u, G[3]); } G[16] = s0; }
void func_08B9EC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0C28u; put(rt, G[2], G[3]); G[5] = word(rt, G[16] + 0x10u); G[3] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 8u); G[7] = word(rt, G[16] + 0xCu); put(rt, G[2] + 0x10u, G[5]); put(rt, G[2] + 4u, G[3]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, G[7]); } G[16] = s0; }
void func_08BB9A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC3D88u; put(rt, G[2], G[3]); G[5] = word(rt, G[16] + 0x10u); G[3] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 8u); G[7] = word(rt, G[16] + 0xCu); put(rt, G[2] + 0x10u, G[5]); put(rt, G[2] + 4u, G[3]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, G[7]); } G[16] = s0; }
void func_08BC31FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC4458u; put(rt, G[2], G[3]); G[5] = word(rt, G[16] + 0x10u); G[3] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 8u); G[7] = word(rt, G[16] + 0xCu); put(rt, G[2] + 0x10u, G[5]); put(rt, G[2] + 4u, G[3]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, G[7]); } G[16] = s0; }
void func_08BAF680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BD0310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BD7468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BD7828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BE13DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BE1528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[10] = word(rt, G[5] + 0x24u); G[3] = G[5] + 0x20u; put(rt, G[4] + 4u, G[10]); if (G[10] != 0u) { G[8] = G[10] + 4u; for (;;) { G[7] = word(rt, G[10] + 4u); if (G[7] == 0u) break; G[6] = G[7] + 1u; G[9] = word(rt, G[8]); G[2] = G[6]; if (G[7] == G[9]) { put(rt, G[8], G[2]); G[2] = 1u; } if (G[9] == G[7]) break; } } G[3] = word(rt, G[3]); G[2] = G[4]; put(rt, G[4], G[3]); }
void func_08BE7420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC9628u);
        const u32 member = word(rt, self + 0x24u);
        if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
        call(rt, ctx, 0x08965A3Cu, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x3Cu, 4u});
}
void func_08BFE8D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; G[6] = G[4]; if (G[3] != 0u) { G[7] = word(rt, G[5]); do { G[2] = word(rt, G[3] + 0x10u); G[2] = G[2] < G[7] ? 1u : 0u; if (G[2] != 0u) G[2] = word(rt, G[3] + 0xCu); else { G[2] = word(rt, G[3] + 8u); G[6] = G[3]; } G[3] = G[2]; } while (G[2] != 0u); } if (G[6] != G[4]) { G[2] = word(rt, G[5]); G[3] = word(rt, G[6] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; if (G[2] == 0u) G[4] = G[6]; } G[2] = G[4]; }
void func_08C1A70C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[18] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[6] = load8(G[16] + 0x18u); G[17] = G[2]; G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08C19CECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C1A760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[18] = word(rt, G[4] + 0xCu); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); ctx.fpr[12] = float_at(rt, G[16] + 0x18u); G[17] = G[2]; G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08C1A5ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C1E04C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CAF6D0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x38u); G[4] = G[16]; call_here(rt, ctx, G[2]); store16(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C1E458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CAFA78u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x34u); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C24790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CB06B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x48u); G[4] = G[16]; call_here(rt, ctx, G[2]); store16(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C247E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CB06B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08C2AFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; G[6] = G[4]; if (G[3] != 0u) { G[7] = word(rt, G[5]); do { G[2] = word(rt, G[3] + 0x10u); G[2] = G[2] < G[7] ? 1u : 0u; if (G[2] != 0u) G[2] = word(rt, G[3] + 0xCu); else { G[2] = word(rt, G[3] + 8u); G[6] = G[3]; } G[3] = G[2]; } while (G[2] != 0u); } if (G[6] != G[4]) { G[2] = word(rt, G[5]); G[3] = word(rt, G[6] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; if (G[2] == 0u) G[4] = G[6]; } G[2] = G[4]; }
void func_08C2B010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u); G[4] += 4u; G[6] = G[4]; if (G[3] != 0u) { G[7] = word(rt, G[5]); do { G[2] = word(rt, G[3] + 0x10u); G[2] = G[2] < G[7] ? 1u : 0u; if (G[2] != 0u) G[2] = word(rt, G[3] + 0xCu); else { G[2] = word(rt, G[3] + 8u); G[6] = G[3]; } G[3] = G[2]; } while (G[2] != 0u); } if (G[6] != G[4]) { G[2] = word(rt, G[5]); G[3] = word(rt, G[6] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; if (G[2] == 0u) G[4] = G[6]; } G[2] = G[4]; }
void func_08C3DAA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; store16(G[4] + 6u, G[2]); G[3] = G[4]; G[6] = 1u; store16(G[4], G[2]); G[5] = 0xFFFFFFFFu; G[7] = 0x81u; store16(G[4] + 2u, G[2]); store16(G[4] + 4u, 0u); store16(G[4] + 8u, 0u); do { store16(G[3] + 0x14u, G[6]); G[6] += 1u; store16(G[3] + 0x10u, G[5]); store16(G[3] + 0x12u, G[5]); store16(G[3] + 0x18u, G[5]); store16(G[3] + 0x1Au, G[5]); store16(G[3] + 0x16u, 0u); G[3] += 0x7Cu; } while (G[6] != G[7]); store16(G[4] + 0x3D98u, G[5]); }
void func_08C5201C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 object = ctx.gpr[5];
    const u32 first = word(rt, object);
    call(rt, ctx, 0x08B5C094u, {object});
    const u32 member = word(rt, object + 0x14u);
    if (member != 0u) call(rt, ctx, 0x08B61FC0u, {member});
    call(rt, ctx, 0x08B35254u, {object});
    ctx.gpr[2] = first;
}
void func_08C55090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = sext(G[5] & 0xFFu, 8u); G[5] = 0x20u; G[2] = load8(G[4] + 0x75u); G[16] = G[4]; if (G[2] == 0u) { call_here(rt, ctx, 0x08C5477Cu); G[3] = 1u; store8(G[16] + 0x75u, G[3]); store8(G[16] + 0x74u, G[2]); } G[2] = sext(load8(G[16] + 0x74u), 8u); store8(G[16] + 0x74u, G[17]); } G[16] = s0; G[17] = s1; }
void func_08C5541C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDB0E0u; G[7] = word(rt, G[2] + 0xCu); G[9] = word(rt, G[2] + 4u); G[10] = word(rt, G[2] + 8u); G[5] = word(rt, G[7] - 0xCu); G[6] = word(rt, G[9] - 0xCu); G[8] = word(rt, G[2] + 0x10u); G[2] = high_half(0x08CDB088u); G[3] = G[4]; G[5] = G[4] + G[5]; G[6] = G[4] + G[6]; G[2] = 0x08CDB088u; put(rt, G[4] + 8u, G[7]); G[4] += 0xCu; put(rt, G[5] + 8u, G[8]); put(rt, G[3] + 4u, 0u); put(rt, G[3], G[9]); put(rt, G[6], G[10]); put(rt, G[3] + 0xCu, G[2]); jump(ctx, 0x08B6543Cu); }
void func_08C59BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[9] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[10] = word(rt, G[5]); G[2] = word(rt, G[9] + 0xCu); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[10]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[9] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[10] = word(rt, G[5]); G[2] = word(rt, G[9] + 0x10u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[10]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[10] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[11] = word(rt, G[5]); G[2] = word(rt, G[10] + 0x14u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[11]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[10] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[11] = word(rt, G[5]); G[2] = word(rt, G[10] + 0x18u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[11]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[10] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[11] = word(rt, G[5]); G[2] = word(rt, G[10] + 0x1Cu); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[11]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59D8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[10] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[11] = word(rt, G[5]); G[2] = word(rt, G[10] + 0x20u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[11]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[9] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[10] = word(rt, G[5]); G[2] = word(rt, G[9] + 0x24u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[10]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C5DB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB7F0u; G[5] = G[5] != 0u ? 1u : 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B5C274u); put(rt, G[17] + 0x10u, G[2]); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08B610C0u); } G[16] = s0; G[17] = s1; }
void func_08C5DBC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB7F0u; G[5] = G[5] != 0u ? 1u : 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B5C274u); put(rt, G[17] + 0x10u, G[2]); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08B610C0u); } G[16] = s0; G[17] = s1; }
void func_08C5DC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB7F0u; G[6] = G[6] != 0u ? 1u : 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[6]); put(rt, G[4], G[2]); put(rt, G[4] + 8u, G[5]); call_here(rt, ctx, 0x08B5C274u); put(rt, G[17] + 0x10u, G[2]); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08B610C0u); } G[16] = s0; G[17] = s1; }
void func_08C6F660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 0xFFFFFFFFu; G[3] = G[4]; G[16] = G[5]; G[4] = G[5]; if (G[5] != G[2]) { G[5] = word(rt, G[3] + 0x20u); tail = true; } else { G[4] = word(rt, G[3] + 0x20u); call_here(rt, ctx, 0x08B7807Cu); if (G[2] == 0u) G[16] = 0u; G[2] = G[16]; } } G[16] = s0; if (tail) jump(ctx, 0x08B7A340u); }
void func_08C76D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[4] + 0x14u); G[2] = G[2] < G[3] ? 1u : 0u; G[5] = 0xFFFFFFFFu; if (G[2] == 0u) G[4] = 0u; else { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x34u); call_here(rt, ctx, G[2]); G[3] = 0xFFFFFFFFu; G[4] = G[2] == G[3] ? 0xFFFFFFFFu : 0u; } G[2] = G[4]; } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_186
} // namespace gt::game

#define HOT186(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_186::name, gt::ReturnKind::kind)
// HOT186(0x088056A4u, func_088056A4, Word);  // no comparable synthetic case yet
HOT186(0x08817DD0u, func_08817DD0, Word);
HOT186(0x08818210u, func_08818210, Word);
HOT186(0x08820AC0u, func_08820AC0, Word);
HOT186(0x08820B14u, func_08820B14, Word);
HOT186(0x08835168u, func_08835168, Word);
// HOT186(0x088438D0u, func_088438D0, Word);  // no comparable synthetic case yet
// HOT186(0x08843924u, func_08843924, Word);  // no comparable synthetic case yet
// HOT186(0x08854B20u, func_08854B20, Word);  // no comparable synthetic case yet
HOT186(0x0885D248u, func_0885D248, Word);
HOT186(0x0885DAB0u, func_0885DAB0, Word);
HOT186(0x0885DB04u, func_0885DB04, Word);
HOT186(0x0885DD1Cu, func_0885DD1C, Word);
HOT186(0x0885DE94u, func_0885DE94, Word);
HOT186(0x08884DA0u, func_08884DA0, Word);
HOT186(0x08894770u, func_08894770, Word);
HOT186(0x08896350u, func_08896350, Word);
HOT186(0x088A155Cu, func_088A155C, Word);
HOT186(0x088A5748u, func_088A5748, Word);
HOT186(0x088A86BCu, func_088A86BC, Word);
HOT186(0x088BB3F8u, func_088BB3F8, Word);
HOT186(0x088D0A60u, func_088D0A60, Word);
HOT186(0x088E732Cu, func_088E732C, Word);
HOT186(0x088F10BCu, func_088F10BC, Word);
HOT186(0x088F5C8Cu, func_088F5C8C, Word);
HOT186(0x088F73ECu, func_088F73EC, Word);
HOT186(0x088F7878u, func_088F7878, Float);
HOT186(0x088FABD8u, func_088FABD8, Word);
HOT186(0x088FAC2Cu, func_088FAC2C, Word);
HOT186(0x088FC080u, func_088FC080, Word);
HOT186(0x08900D94u, func_08900D94, Word);
HOT186(0x0890610Cu, func_0890610C, Word);
HOT186(0x08908F00u, func_08908F00, Word);
HOT186(0x08925A90u, func_08925A90, Word);
HOT186(0x08925AF4u, func_08925AF4, Word);
HOT186(0x0892991Cu, func_0892991C, Word);
HOT186(0x0892A14Cu, func_0892A14C, Word);
HOT186(0x0892A1A0u, func_0892A1A0, Word);
HOT186(0x0892A9E4u, func_0892A9E4, Word);
HOT186(0x0892AA78u, func_0892AA78, Word);
HOT186(0x0894BFA0u, func_0894BFA0, Float);
HOT186(0x08965AC4u, func_08965AC4, Word);
HOT186(0x08965B24u, func_08965B24, Word);
HOT186(0x08965D34u, func_08965D34, Word);
// HOT186(0x089897D0u, func_089897D0, Word);  // no comparable synthetic case yet
HOT186(0x0899CAF0u, func_0899CAF0, Word);
HOT186(0x089B1820u, func_089B1820, Word);
HOT186(0x089B4A84u, func_089B4A84, Word);
HOT186(0x089B51D8u, func_089B51D8, Word);
// HOT186(0x089B7178u, func_089B7178, Word);  // no comparable synthetic case yet
HOT186(0x089B9990u, func_089B9990, Word);
HOT186(0x089BA708u, func_089BA708, Word);
HOT186(0x089BF818u, func_089BF818, Word);
// HOT186(0x089BFDA0u, func_089BFDA0, Word);  // no comparable synthetic case yet
HOT186(0x089C1EACu, func_089C1EAC, Word);
HOT186(0x089CE964u, func_089CE964, Word);
HOT186(0x089CF030u, func_089CF030, Word);
HOT186(0x089CF0E0u, func_089CF0E0, Word);
HOT186(0x089CF874u, func_089CF874, Word);
HOT186(0x089CF924u, func_089CF924, Word);
HOT186(0x089CFBACu, func_089CFBAC, Word);
HOT186(0x089D5B7Cu, func_089D5B7C, Word);
HOT186(0x089FFDE4u, func_089FFDE4, Word);
HOT186(0x08A07660u, func_08A07660, Word);
HOT186(0x08A0888Cu, func_08A0888C, Word);
HOT186(0x08A088E0u, func_08A088E0, Word);
HOT186(0x08A1A914u, func_08A1A914, Word);
HOT186(0x08A2159Cu, func_08A2159C, Word);
HOT186(0x08A25A48u, func_08A25A48, Word);
HOT186(0x08A2AD60u, func_08A2AD60, Word);
// HOT186(0x08A2B650u, func_08A2B650, Word);  // no comparable synthetic case yet
HOT186(0x08A2F494u, func_08A2F494, Word);
HOT186(0x08A32A40u, func_08A32A40, Word);
HOT186(0x08A331D4u, func_08A331D4, Word);
// HOT186(0x08A35160u, func_08A35160, Word);  // no comparable synthetic case yet
HOT186(0x08A3A4CCu, func_08A3A4CC, Word);
// HOT186(0x08A3AEE8u, func_08A3AEE8, Word);  // no comparable synthetic case yet
HOT186(0x08A44598u, func_08A44598, Word);
HOT186(0x08A44C28u, func_08A44C28, Word);
HOT186(0x08A5FBF8u, func_08A5FBF8, Word);
HOT186(0x08A604CCu, func_08A604CC, Word);
HOT186(0x08A60520u, func_08A60520, Word);
HOT186(0x08A60574u, func_08A60574, Word);
HOT186(0x08A605C8u, func_08A605C8, Word);
// HOT186(0x08A6AD7Cu, func_08A6AD7C, Word);  // no comparable synthetic case yet
// HOT186(0x08A704C8u, func_08A704C8, Word);  // no comparable synthetic case yet
// HOT186(0x08A71F1Cu, func_08A71F1C, Word);  // no comparable synthetic case yet
HOT186(0x08A73298u, func_08A73298, Word);
HOT186(0x08A734B8u, func_08A734B8, Word);
HOT186(0x08A85C88u, func_08A85C88, Word);
HOT186(0x08A9622Cu, func_08A9622C, Word);
HOT186(0x08A73DDCu, func_08A73DDC, Word);
HOT186(0x08A73E30u, func_08A73E30, Word);
HOT186(0x08A8654Cu, func_08A8654C, Word);
HOT186(0x08A88758u, func_08A88758, Word);
HOT186(0x08A8BA3Cu, func_08A8BA3C, Word);
HOT186(0x08A8BA90u, func_08A8BA90, Word);
HOT186(0x08A96BA4u, func_08A96BA4, Word);
HOT186(0x08A97BDCu, func_08A97BDC, Word);
HOT186(0x08AB0398u, func_08AB0398, Word);
HOT186(0x08AB1B1Cu, func_08AB1B1C, Word);
HOT186(0x08A99198u, func_08A99198, Word);
HOT186(0x08A9A048u, func_08A9A048, Word);
// HOT186(0x08AB0178u, func_08AB0178, Word);  // no comparable synthetic case yet
// HOT186(0x08AB18FCu, func_08AB18FC, Word);  // no comparable synthetic case yet
HOT186(0x08AB0CBCu, func_08AB0CBC, Word);
HOT186(0x08AB0D10u, func_08AB0D10, Word);
HOT186(0x08AB2468u, func_08AB2468, Word);
HOT186(0x08AB24BCu, func_08AB24BC, Word);
// HOT186(0x08ABCD8Cu, func_08ABCD8C, Word);  // no comparable synthetic case yet
HOT186(0x08AC2910u, func_08AC2910, Word);
HOT186(0x08AC37C8u, func_08AC37C8, Word);
HOT186(0x08AC653Cu, func_08AC653C, Word);
// HOT186(0x08AC7820u, func_08AC7820, Word);  // no comparable synthetic case yet
HOT186(0x08AC9240u, func_08AC9240, Word);
HOT186(0x08ACA9B4u, func_08ACA9B4, Word);
HOT186(0x08ACB168u, func_08ACB168, Word);
// HOT186(0x08ACC93Cu, func_08ACC93C, Word);  // no comparable synthetic case yet
HOT186(0x08AD23E4u, func_08AD23E4, Float);
HOT186(0x08AD3BA4u, func_08AD3BA4, Word);
HOT186(0x08AD78C8u, func_08AD78C8, Word);
HOT186(0x08AD8C5Cu, func_08AD8C5C, Word);
HOT186(0x08ADFD8Cu, func_08ADFD8C, Word);
HOT186(0x08AE2EB4u, func_08AE2EB4, Word);
HOT186(0x08AE3568u, func_08AE3568, Word);
HOT186(0x08AE3C84u, func_08AE3C84, Word);
HOT186(0x08AE4634u, func_08AE4634, Word);
HOT186(0x08AE69A0u, func_08AE69A0, Word);
HOT186(0x08AFBAFCu, func_08AFBAFC, Word);
HOT186(0x08B0162Cu, func_08B0162C, Word);
HOT186(0x08B084F0u, func_08B084F0, Word);
HOT186(0x08B09A7Cu, func_08B09A7C, Word);
HOT186(0x08B0C56Cu, func_08B0C56C, DoubleWord);
// HOT186(0x08B0DD88u, func_08B0DD88, Word);  // no comparable synthetic case yet
HOT186(0x08B15C30u, func_08B15C30, Word);
HOT186(0x08B21484u, func_08B21484, Word);
HOT186(0x08B33080u, func_08B33080, Word);
HOT186(0x08B33898u, func_08B33898, Word);
HOT186(0x08B3E0ACu, func_08B3E0AC, Word);
HOT186(0x08B3E5F8u, func_08B3E5F8, Word);
HOT186(0x08B4AD10u, func_08B4AD10, Word);
HOT186(0x08B547B4u, func_08B547B4, Word);
HOT186(0x08B569ACu, func_08B569AC, Word);
HOT186(0x08B59BC0u, func_08B59BC0, Word);
HOT186(0x08B646D8u, func_08B646D8, Word);
// HOT186(0x08B86B68u, func_08B86B68, Word);  // no comparable synthetic case yet
// HOT186(0x08B89EFCu, func_08B89EFC, Word);  // no comparable synthetic case yet
// HOT186(0x08B889D4u, func_08B889D4, Word);  // no comparable synthetic case yet
HOT186(0x08B8718Cu, func_08B8718C, Word);
HOT186(0x08B88D14u, func_08B88D14, Word);
HOT186(0x08B8C39Cu, func_08B8C39C, Word);
HOT186(0x08B93BCCu, func_08B93BCC, Word);
HOT186(0x08B9737Cu, func_08B9737C, Word);
HOT186(0x08B99550u, func_08B99550, Word);
HOT186(0x08B9EB34u, func_08B9EB34, Word);
HOT186(0x08BC32D4u, func_08BC32D4, Word);
HOT186(0x08B9EC60u, func_08B9EC60, Word);
HOT186(0x08BB9A54u, func_08BB9A54, Word);
HOT186(0x08BC31FCu, func_08BC31FC, Word);
HOT186(0x08BAF680u, func_08BAF680, Word);
HOT186(0x08BD0310u, func_08BD0310, Word);
HOT186(0x08BD7468u, func_08BD7468, Word);
HOT186(0x08BD7828u, func_08BD7828, Word);
HOT186(0x08BE13DCu, func_08BE13DC, Word);
HOT186(0x08BE1528u, func_08BE1528, Word);
HOT186(0x08BE7420u, func_08BE7420, Word);
HOT186(0x08BFE8D8u, func_08BFE8D8, Word);
HOT186(0x08C1A70Cu, func_08C1A70C, Word);
HOT186(0x08C1A760u, func_08C1A760, Word);
// HOT186(0x08C1E04Cu, func_08C1E04C, Word);  // no comparable synthetic case yet
// HOT186(0x08C1E458u, func_08C1E458, Word);  // no comparable synthetic case yet
// HOT186(0x08C24790u, func_08C24790, Word);  // no comparable synthetic case yet
// HOT186(0x08C247E4u, func_08C247E4, Word);  // no comparable synthetic case yet
HOT186(0x08C2AFBCu, func_08C2AFBC, Word);
HOT186(0x08C2B010u, func_08C2B010, Word);
HOT186(0x08C3DAA0u, func_08C3DAA0, Word);
HOT186(0x08C5201Cu, func_08C5201C, Word);
HOT186(0x08C55090u, func_08C55090, Word);
HOT186(0x08C5541Cu, func_08C5541C, Word);
HOT186(0x08C59BE8u, func_08C59BE8, DoubleWord);
HOT186(0x08C59C3Cu, func_08C59C3C, DoubleWord);
HOT186(0x08C59C90u, func_08C59C90, DoubleWord);
HOT186(0x08C59CE4u, func_08C59CE4, DoubleWord);
HOT186(0x08C59D38u, func_08C59D38, DoubleWord);
HOT186(0x08C59D8Cu, func_08C59D8C, DoubleWord);
HOT186(0x08C59DE0u, func_08C59DE0, DoubleWord);
HOT186(0x08C5DB58u, func_08C5DB58, Word);
HOT186(0x08C5DBC0u, func_08C5DBC0, Word);
HOT186(0x08C5DC28u, func_08C5DC28, Word);
HOT186(0x08C6F660u, func_08C6F660, Word);
HOT186(0x08C76D1Cu, func_08C76D1C, Word);
