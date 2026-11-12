// Hot batch 184: nineteen-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_184 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

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

// Call the virtual at `slot` of every object on the intrusive list whose head is the word at `head` (v0 starts as the `lui` half).
void walk_global_list(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 head, u32 slot) {
    ctx.gpr[2] = high_half(head);
    for (u32 node = word(rt, head); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
    }
}
// A vtable, a flag at +4 and a handle at +8 returned by 0x08B61BD0(&{value}) (frame slot).
void construct_with_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable, u32 slot) {
    const u32 self = ctx.gpr[4];
    put(rt, self + 4u, ctx.gpr[6] != 0u ? 1u : 0u);
    put(rt, self, vtable);
    put(rt, slot, ctx.gpr[5]);
    put(rt, self + 8u, call(rt, ctx, 0x08B61BD0u, {slot}));
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08805BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    constexpr u32 kHolder = 0x08CE28A0u;
    const u32 slot = frame.at(0x0u);
    call(rt, ctx, 0x08A5BCFCu);
    call(rt, ctx, 0x08A9BA9Cu, {slot, 0x08CF6ED8u});
    ctx.gpr[2] = kHolder;
    if (slot != kHolder) call(rt, ctx, 0x08A5BAD8u, {kHolder, word(rt, slot)});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}
void func_08809574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    call(rt, ctx, 0x08808F2Cu, {slot, ctx.gpr[5], ctx.gpr[6], 0u, 0x08808640u});
    if (self != slot) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, slot)});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}
void func_0885D480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (source == 0u) {
            ctx.gpr[4] = self + 4u;
            ctx.gpr[6] = 0x11u;
        } else {
            call(rt, ctx, 0x08B7C830u, {self + 4u, source, 0x10u});
            set_byte(rt, self + 0x13u, 0u);
            return;
        }
    }
    tail_call(ctx, 0x08B36018u, {self + 4u, source, 0x11u});
}
void func_0887C89C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 size = ctx.gpr[4];
    const u32 block = call(rt, ctx, 0x08B2B4D8u, {0x10u, size});
    call(rt, ctx, 0x08B36018u, {block, 0u, size});
    ctx.gpr[2] = block;
}
void func_0887D034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 block = call(rt, ctx, 0x0887C89Cu, {0x3FCu});
    put(rt, self, block);
    if (block != 0u) call(rt, ctx, 0x0887CDA4u, {block});
    put(rt, self + 4u, block);
}
void func_088A4218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (call_indirect(rt, ctx, word(rt, word(rt, self) + 0x64u), {self}) != 0u && byte_at(rt, self + 0x2F8u) != 0u)
        result = byte_at(rt, word(rt, self + 0x1E4u) + 0x43Fu);
    ctx.gpr[2] = result;
}
void func_088A444C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    for (u32 offset = 0u; offset < 0x10u; offset += 4u) put(rt, self + offset, 0u);
    const u32 owner = word(rt, ctx.gpr[5] + 0x18u);
    if (owner != 0u) call(rt, ctx, 0x08955358u, {owner, self});
    ctx.gpr[2] = self;
}
void func_088A7398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 argument = ctx.gpr[4];
    const u32 block = call(rt, ctx, 0x08B62344u, {0x4D8u});
    call(rt, ctx, 0x088A7310u, {block});
    call(rt, ctx, 0x088A5450u, {block, argument});
    ctx.gpr[2] = block;
}
void func_088D0A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.fpr[12] = float_at(rt, self + 0xCu);
    const u32 previous = word(rt, self + 0x2Cu);
    const u32 current = call(rt, ctx, 0x088FB1B4u, {ctx.gpr[5]});
    put(rt, self + 0x2Cu, current);
    if (previous != current) {
        ctx.gpr[2] = 1u;
        set_byte(rt, self + 0x34u, 1u);
        put(rt, self + 0x30u, previous);
    }
}
void func_088D0D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[6];
    set_byte(rt, self + 0x59u, byte_at(rt, self + 0x58u));
    const u32 object = call(rt, ctx, 0x088DCD28u, {argument});
    if (object != 0u) set_byte(rt, self + 0x58u, call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {object}));
}
void func_088D435C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 base = ctx.gpr[4];
    if (base != 0u)
        for (u32 index = 3u; index != 0u; --index) call(rt, ctx, 0x08B09290u, {base + (index - 1u) * 0x28u});
}
void func_088D44BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 destination = ctx.gpr[4];
    u32 source = ctx.gpr[5];
    for (; source != ctx.gpr[5] + 0x80u; source += 0x10u, destination += 0x10u) {
        const u32 first = word(rt, source);
        const u32 second = word(rt, source + 4u);
        const u32 third = word(rt, source + 8u);
        const u32 fourth = word(rt, source + 0xCu);
        put(rt, destination, first);
        put(rt, destination + 4u, second);
        put(rt, destination + 8u, third);
        put(rt, destination + 0xCu, fourth);
    }
    const u32 tail_high = word(rt, source + 4u);
    const u32 tail_low = word(rt, source);
    put(rt, destination + 4u, tail_high);
    put(rt, destination, tail_low);
    ctx.gpr[2] = tail_high;
}
void func_088D8074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, self + 0x148u);
    if (object != 0u) call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    ctx.gpr[2] = 0u;
    put(rt, self + 0x708u, 0u);
    put(rt, self + 0x70Cu, 0u);
}
void func_088DC8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 record = word(rt, self + 0xC0u) + ctx.gpr[5] * 0x2F8u;
    u32 result = 1u;
    if (word(rt, record + 0x1Cu) != 0u && (word(rt, record + 0x2E4u) == 0u || byte_at(rt, self + 0x5BCu) == 0u)) result = 0u;
    ctx.gpr[2] = result;
}
void func_088E72E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    u32 result = 0u;
    const u32 owner = word(rt, self + 8u);
    if (owner != 0u) {
        const u32 table = call(rt, ctx, 0x088DF13Cu, {owner, word(rt, self + 4u)});
        result = word(rt, (index << 2) + table + 0x144u);
    }
    ctx.gpr[2] = result;
}
void func_088E8348(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 1u;
    if (word(rt, self + 0x1Cu) != 0u) {
        result = 0u;
        if (word(rt, self + 0x2E4u) != 0u && word(rt, self + 0x24u) != 0u)
            result = byte_at(rt, call(rt, ctx, 0x088E7EA0u, {self}) + 5u) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}
void func_088F1C40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 owner = word(rt, self + 8u);
    ctx.gpr[2] = byte_at(rt, owner + 0x9C4u);
    if (ctx.gpr[2] == 0u) {
        const u32 found = call(rt, ctx, 0x088F61C0u, {owner + 0x844u, 0x80u});
        ctx.gpr[2] = 1u;
        if (found != 0u) set_byte(rt, word(rt, self + 8u) + 0x9C4u, 1u);
    }
}
void func_088F2CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    put(rt, self + 0x28u, 0xFFFFFFFFu);
    put(rt, self + 0x24u, argument);
    ctx.gpr[2] = argument;
    if (argument != 0u) put(rt, self + 0x28u, call(rt, ctx, 0x08AF6EA8u, {argument, 0x08CA0B7Cu}));
}
void func_088FBBB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        target = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x30u), {self, argument});
    }
    if (target != 0u) tail_call(ctx, 0x08961374u, {target + 0x540u, argument});
}
void func_0890B06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    constexpr u32 kTable = 0x08CDD7E8u;
    constexpr u32 kEnd = kTable + 8u;
    put(rt, frame.at(0x0u), ctx.gpr[4]);
    const u32 found = call(rt, ctx, 0x08BDA464u, {kTable, kEnd, frame.at(0x0u)});
    ctx.gpr[2] = found != kEnd ? word(rt, found + 4u) : 0xFFFFFFFFu;
}
void release_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, self + 4u);
    put(rt, self, 0x08CC6110u);
    ctx.gpr[2] = 0x08CC6110u;
    if (object != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, object) + 4u), {object});
        put(rt, self + 4u, 0u);
    }
}
void func_08925F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_object(rt, ctx); }
void func_08926250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_object(rt, ctx); }
void func_08938638(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 object = word(rt, self + 4u);
        put(rt, self, 0x08CC67E8u);
        ctx.gpr[2] = 0x08CC67E8u;
        if (object != 0u) call(rt, ctx, 0x08938E48u, {object});
        call(rt, ctx, 0x0893A068u, {self + 0x28u});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_0880AE0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x0880AD24u); G[4] = 0x08C98334u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, G[16] + 0xE8u); call_here(rt, ctx, 0x0880AA94u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_0887BF80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] -= G[5]; ctx.fpr[2] = 0.0f; G[3] = G[4] + G[7]; G[2] = G[4] - G[7]; if (static_cast<std::int32_t>(G[4]) > 0) { if (static_cast<std::int32_t>(G[2]) >= 0) { ctx.fpr[0] = std::bit_cast<float>(G[2]); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[2])); } } else if (static_cast<std::int32_t>(G[3]) <= 0) { ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[3])); } G[6] -= G[7]; ctx.fpr[1] = std::bit_cast<float>(G[6]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[6])); ctx.fpr[0] = ctx.fpr[2] / ctx.fpr[0]; }
void func_088B160C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CC3050u; G[3] = 5u; G[7] = word(rt, G[4]); put(rt, frame.at(8u), G[5]); G[8] = G[4]; G[6] = frame.at(0u); put(rt, frame.at(4u), 5u); G[4] = G[7]; put(rt, frame.at(0u), G[2]); G[3] = word(rt, G[7]); G[2] = word(rt, G[3] + 0x30u); G[5] = word(rt, G[8] + 4u); call_here(rt, ctx, G[2]); }
void func_088BD23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = sext(load8(G[4] + 1u), 8u); ctx.fpr[1] = 1.0f; G[1] = 0x3DCCCCCDu; ctx.fpr[3] = std::bit_cast<float>(G[1]); ctx.fpr[0] = std::bit_cast<float>(G[4]); ctx.fpr[2] = static_cast<float>(static_cast<std::int32_t>(G[4])); G[1] = 0x3C23D70Au; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[1]; if (ctx.fpr[0] < ctx.fpr[3]) ctx.fpr[0] = ctx.fpr[3]; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); }
void func_088CF4AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x2Cu); G[1] = 0x3F800000u; ctx.fpr[2] = 1.0f; if (!(ctx.fpr[12] <= ctx.fpr[1])) { ctx.fpr[0] = float_at(rt, G[4] + 0x30u); ctx.fpr[2] = 0.0f; const bool past = ctx.fpr[0] <= ctx.fpr[12]; ctx.fpr[1] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[12] = ctx.fpr[0] - ctx.fpr[12]; if (!past) { ctx.fpr[0] = 1.0f; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; ctx.fpr[2] = psp_mul(ctx.fpr[12], ctx.fpr[0]); } } ctx.fpr[0] = ctx.fpr[2]; }
void func_088CF978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); G[2] = word(rt, G[4] + 0xC0u); G[3] = word(rt, G[4] + 0x64u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[6] = 0u; if (G[2] == 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0xB8u); ctx.fpr[20] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[0]))); call_here(rt, ctx, 0x088CF8B0u); ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[0]); G[6] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[20])); ctx.fpr[20] = std::bit_cast<float>(G[6]); } G[2] = G[6]; } ctx.fpr[20] = f20; }
void func_088D0C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[4] = G[5]; call_here(rt, ctx, 0x088D201Cu); ctx.fpr[0] = 60.0f; ctx.fpr[20] = ctx.fpr[20] / ctx.fpr[0]; G[1] = 0x447A0000u; ctx.fpr[1] = 1000.0f; ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[1]); G[3] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[20])); ctx.fpr[0] = std::bit_cast<float>(G[3]); } ctx.fpr[20] = f20; G[2] += G[3]; }
void func_088DADB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x40u); GuestFrame frame(ctx, 0x10u); G[2] = 0xFFFFFFFFu; if (G[3] == 0u) return; G[2] = word(rt, G[4] + 0x38u); G[1] = 0x426FC28Fu; ctx.fpr[12] = std::bit_cast<float>(G[1]); G[3] -= G[2]; ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[12] = ctx.fpr[1] / ctx.fpr[12]; call_here(rt, ctx, 0x08B661E8u); G[2] = static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0])); ctx.fpr[0] = std::bit_cast<float>(G[2]); }
void func_088ED088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[6]); G[6] = 0xFFFFFFFFu; put(rt, frame.at(4u), G[7]); G[7] = 5u; store16(frame.at(8u), G[5]); G[5] = 0u; store8(frame.at(0xAu), G[8]); G[8] = frame.at(0u); store8(frame.at(0xBu), G[9]); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xF4u); G[9] = 0x10u; call_here(rt, ctx, G[2]); }
void func_088F7490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x100u) << 3; G[3] = 0x08CDD78Cu; G[2] += G[3]; G[25] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); if (G[25] == 0u) return; G[7] = sra(G[3], 1u); G[3] &= 1u; G[4] += G[7]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[25]; G[25] = word(rt, G[2]); } jump(ctx, G[25]); }
void func_088F74DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x100u) << 3; G[3] = 0x08CDD784u; G[2] += G[3]; G[25] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); if (G[25] == 0u) return; G[7] = sra(G[3], 1u); G[3] &= 1u; G[4] += G[7]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[25]; G[25] = word(rt, G[2]); } jump(ctx, G[25]); }
void func_0893C91C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = sra(G[5], 1u); G[6] = G[5] << 2; G[2] = (G[5] << 5) - G[6] + G[5]; G[3] = word(rt, G[4] + 0xCu); G[2] = (G[2] << 2) + 0x360u; G[6] += G[3]; G[3] += G[2]; ctx.fpr[3] = float_at(rt, G[6] + 0x330u); ctx.fpr[0] = float_at(rt, G[3] + 0x10u); ctx.fpr[1] = float_at(rt, G[6] + 0x318u); ctx.fpr[2] = float_at(rt, G[3] + 0xCu); ctx.fpr[0] = -ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; }
void func_08945E80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[11] = word(rt, G[5] + 0x1Cu); G[3] = word(rt, G[5]); G[4] = word(rt, G[5] + 4u); G[6] = word(rt, G[5] + 8u); G[7] = word(rt, G[5] + 0xCu); G[8] = word(rt, G[5] + 0x10u); G[9] = word(rt, G[5] + 0x14u); G[10] = word(rt, G[5] + 0x18u); put(rt, G[2] + 0x4D0u, G[3]); put(rt, G[2] + 0x4D4u, G[4]); put(rt, G[2] + 0x4D8u, G[6]); put(rt, G[2] + 0x4DCu, G[7]); put(rt, G[2] + 0x4E0u, G[8]); put(rt, G[2] + 0x4E4u, G[9]); put(rt, G[2] + 0x4E8u, G[10]); put(rt, G[2] + 0x4ECu, G[11]); put(rt, G[2] + 0x72Cu, 0u); }
void func_089582AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0xCu); G[4] = word(rt, G[4] + 8u); G[3] = load8(G[5] + 0x140Fu); G[2] = (1u << (G[3] & 31u)) & 0x166u; G[3] = G[3] < 9u ? 1u : 0u; if (G[3] == 0u || G[2] == 0u) return; ctx.fpr[0] = float_at(rt, G[5] + 0x141Cu); ctx.fpr[2] = float_at(rt, G[4] + 0x610u); ctx.fpr[1] = float_at(rt, G[4] + 0x68Cu); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put(rt, G[4] + 0x68Cu, bits_of(ctx.fpr[1])); }
void func_0895936C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08959180u); G[17] = G[2]; G[3] = word(rt, G[2] + 0x9Cu); G[2] = 0xFFFFFFFFu; G[4] = G[16]; if (G[3] == G[2]) { call_here(rt, ctx, 0x08958D40u); put(rt, G[17] + 0x9Cu, G[2]); } } G[16] = s0; G[17] = s1; }
void func_08959AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); ctx.fpr[1] = float_at(rt, G[2] + 0x708u); ctx.fpr[0] = float_at(rt, G[2] + 0x6FCu); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[1] = 0.0f; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); G[2] = high_half(0x08CF3548u); if (ctx.fpr[1] < ctx.fpr[0]) { ctx.fpr[0] = float_at(rt, 0x08CF3548u); G[1] = 0xBC88AB7Cu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[0]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[0]; } ctx.fpr[0] = ctx.fpr[12]; }
void func_08959B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); ctx.fpr[1] = float_at(rt, G[2] + 0x70Cu); ctx.fpr[0] = float_at(rt, G[2] + 0x700u); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[1] = 0.0f; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); G[2] = high_half(0x08CF3544u); if (ctx.fpr[1] < ctx.fpr[0]) { ctx.fpr[0] = float_at(rt, 0x08CF3544u); G[1] = 0xBC88AB7Cu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[0]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[12] = ctx.fpr[12] + ctx.fpr[0]; } ctx.fpr[0] = ctx.fpr[12]; }
void func_08966370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[4] = 1u; if (G[2] == 0u) G[4] = 0u; else { G[2] = word(rt, G[16] + 0x80u); G[3] = word(rt, G[2] + 0x30u) & 0x280u; G[2] = 0x280u; if (G[3] != G[2]) G[4] = 0u; } G[2] = G[4]; } G[16] = s0; }
void func_08966434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[4] = 1u; if (G[2] == 0u) G[4] = 0u; else { G[2] = word(rt, G[16] + 0x80u); G[3] = word(rt, G[2] + 0x30u) & 0x140u; G[2] = 0x140u; if (G[3] != G[2]) G[4] = 0u; } G[2] = G[4]; } G[16] = s0; }
void func_08968F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x24u; if (G[16] != 0u) { call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0x2Cu; G[6] = 4u; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08A5BC54u); }
void func_08994D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC8EC0u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CC9118u; put(rt, G[16] + 0x38u, G[2]); G[4] += 0x11Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x118u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A0298Cu); }
void func_08994DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC8EC0u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CC9118u; put(rt, G[16] + 0x38u, G[2]); G[4] += 0x11Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x118u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A0298Cu); }
void func_08996308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = high_half(0x08CF4598u); G[17] = G[4]; G[4] = 0x08CF4598u; put(rt, frame.at(0x10u), bits_of(f20)); call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4598u; ctx.fpr[20] = float_at(rt, G[17] + 0x178u); call_here(rt, ctx, 0x08B3457Cu); ctx.fpr[0] = ctx.fpr[20]; } G[16] = s0; G[17] = s1; ctx.fpr[20] = f20; }
void func_089A5314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x158u; G[16] = G[4] + 0x154u; G[4] = G[16]; G[5] = word(rt, G[16] + 8u); call_here(rt, ctx, 0x08BF738Cu); put(rt, G[16] + 0x10u, G[17]); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0xCu, G[17]); put(rt, G[16] + 8u, 0u); } G[16] = s0; G[17] = s1; }
void func_089A56E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x170u; G[16] = G[4] + 0x16Cu; G[4] = G[16]; G[5] = word(rt, G[16] + 8u); call_here(rt, ctx, 0x08BF7980u); put(rt, G[16] + 0x10u, G[17]); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0xCu, G[17]); put(rt, G[16] + 8u, 0u); } G[16] = s0; G[17] = s1; }
void func_089A6EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF4994u); G[17] = G[4]; G[4] = 0x08CF4994u; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4994u; G[18] = word(rt, G[17] + 0x18u); call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089A6F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF4994u); G[17] = G[4]; G[4] = 0x08CF4994u; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4994u; G[18] = word(rt, G[17] + 0x1Cu); call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089A70D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF4994u); G[17] = G[4]; G[4] = 0x08CF4994u; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CF4994u; G[18] = word(rt, G[17] + 0x14u); call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089CDFF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF5134u); G[17] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; call_here(rt, ctx, 0x089CDFC4u); G[17] = G[2]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_089CE124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08CF5134u); G[17] = G[4]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; call_here(rt, ctx, 0x089CE0F8u); G[17] = G[2]; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_089E8848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x089BD668u); G[5] = G[2]; G[4] = G[17]; G[6] = G[16]; G[7] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x089E8408u); }
void func_08A03ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] <<= 2; G[5] += G[6]; G[16] = G[4]; G[4] = frame.at(0u); ctx.fpr[12] = float_at(rt, G[5] + 0x20u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08A0AD9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x0896422Cu); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B2A034u); }
void func_08A135B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x10u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17]; G[18] = word(rt, G[16] + 0x1C0u); call_here(rt, ctx, 0x08B34B24u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A139E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x10u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17]; G[18] = load8(G[16] + 0x782u); call_here(rt, ctx, 0x08B34B24u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A13A30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x10u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[4] = G[17]; G[18] = load8(G[16] + 0x1F7u); call_here(rt, ctx, 0x08B34B24u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A1AA6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x68u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; G[18] = word(rt, G[16] + 0x64u); call_here(rt, ctx, 0x08B3457Cu); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A17CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[6]; G[11] = G[5]; G[10] = word(rt, G[3]); G[3] = G[7]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; G[7] = G[2]; G[2] = G[8]; G[8] = G[3]; if (G[10] == 0u) { G[7] = G[2]; G[8] = G[9]; } else { G[4] = G[11]; G[5] = G[2]; G[6] = G[9]; } jump(ctx, 0x08966B20u); }
void func_08A17EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[6]; G[11] = G[5]; G[10] = word(rt, G[3]); G[3] = G[7]; G[4] = G[5]; G[5] = G[6]; G[6] = G[7]; G[7] = G[2]; G[2] = G[8]; G[8] = G[3]; if (G[10] == 0u) { G[7] = G[2]; G[8] = G[9]; } else { G[4] = G[11]; G[5] = G[2]; G[6] = G[9]; } jump(ctx, 0x08A0D048u); }
void func_08A237EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A237A8u); G[5] = G[17]; G[4] = 0x80u; call_here(rt, ctx, 0x08B35840u); put(rt, G[16] + 0x6Cu, G[2]); G[4] = G[16]; G[6] = G[17]; G[5] = G[2]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08AB9144u); }
void func_08A33228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[4] += 0x14u; call_here(rt, ctx, 0x08B1B40Cu); } G[3] = 0x92492493u; G[4] = G[2] + 2u; const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); G[2] = sra(G[4], 31u); G[3] = ctx.hi + G[4]; G[3] = sra(G[3], 2u) - G[2]; G[2] = (G[3] << 3) - G[3]; G[2] = G[4] - G[2]; }
void func_08A342CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD1B20u; G[5] = 0x08CF6ED8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x18u; put(rt, G[16] + 0x14u, 0u); call_here(rt, ctx, 0x08A76AACu); put(rt, G[16] + 0x20u, 0u); put(rt, G[16] + 0x1Cu, 0u); } G[16] = s0; }
void func_08A34B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD1B20u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x14u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08B2E28Cu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A35114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0u; G[16] = G[4]; call_here(rt, ctx, 0x08A34E98u); G[4] = G[16]; call_here(rt, ctx, 0x08A35088u); G[16] = word(rt, G[16] + 0x1Cu); G[4] = 0x08CACEC8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A3A6FCu); }
void func_08A35848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CCE588u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A35114u); G[4] = G[16] + 0x1Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A3A398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x14u); if (G[4] != 0u) { G[2] = word(rt, G[16] + 0x18u); if (G[2] == 0u) { ctx.fpr[12] = float_at(rt, G[16] + 0x20u); G[6] = load8(G[16] + 0x24u); call_here(rt, ctx, 0x08ABC958u); put(rt, G[16] + 0x18u, G[2]); store8(G[16] + 0x2Cu, 0u); } } } G[16] = s0; }
void func_08A3A434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x14u); if (G[4] != 0u) { G[2] = word(rt, G[16] + 0x18u); if (G[2] == 0u) { ctx.fpr[13] = float_at(rt, G[16] + 0x20u); G[6] = load8(G[16] + 0x24u); call_here(rt, ctx, 0x08ABCA10u); put(rt, G[16] + 0x18u, G[2]); store8(G[16] + 0x2Cu, 0u); } } } G[16] = s0; }
void func_08A3A73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[6]; G[18] = G[4]; call_here(rt, ctx, 0x08A3A6FCu); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08ABC844u); put(rt, G[18] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A3DA14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A3D8B0u); G[4] = 0x08CAD8CCu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A3D718u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A4C88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; call_here(rt, ctx, 0x08B3412Cu); G[2] = ~G[2]; G[4] = frame.at(0u); G[5] = G[2] >> 31; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A4DD6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[5] = 0u; G[3] = word(rt, G[4] + 0xCu); G[16] = word(rt, G[4] + 8u); G[3] = G[4] + G[3]; G[16] += G[4]; put(rt, G[4] + 0xCu, G[3]); G[4] = G[16]; put(rt, G[2] + 8u, G[16]); call_here(rt, ctx, 0x08AF771Cu); G[4] = G[16]; G[5] = 0xFFFFFFFFu; } G[16] = s0; jump(ctx, 0x08AF7B0Cu); }
void func_08A4FD08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); G[16] = G[4]; call_here(rt, ctx, 0x08B93CD8u); store8(G[16] + 4u, 0u); store8(G[16] + 5u, 0u); store8(G[16] + 6u, 0u); store8(G[16] + 7u, 0u); put(rt, G[16] + 8u, 0u); put(rt, G[16] + 0xCu, 0u); call_here(rt, ctx, 0x08A4F580u); store16(G[16] + 0x10u, G[2]); store16(G[16] + 0x12u, 0u); } G[16] = s0; }
void func_08A50198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[16] = G[5]; G[2] = 0x80460006u; if (G[5] != 0u) { call_here(rt, ctx, 0x08B0C778u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 4u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = 0u; } } G[16] = s0; }
void func_08A57F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2100u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x14u); put(rt, G[4], G[2]); if (G[3] != 0u) { G[2] = word(rt, G[4] + 0xCu); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, G[3]); } G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A5BB5Cu); }
void func_08A58928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; G[2] = word(rt, G[4] + 0x38u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } put(rt, G[16] + 0x38u, G[17]); } G[16] = s0; G[17] = s1; }
void func_08A58FC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = G[5]; G[2] = word(rt, G[5] + 0x1Cu); G[5] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[3] + 0x18u); G[4] = 0x08CAF864u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0xCu); } put(rt, G[5], 0u); } G[16] = s0; }
void func_08A5D684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD2578u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x10u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[16] + 0xCu; call_here(rt, ctx, 0x08A69D68u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A5BB5Cu); }
void func_08A60E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CD2468u; G[5] = 0x08CF6ED8u; put(rt, G[16], G[2]); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A61BC0u); G[2] = G[16] + 0x18u; put(rt, G[16] + 0x18u, G[2]); put(rt, G[2] + 4u, G[2]); } G[16] = s0; }
void func_08A64754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[6] = G[5]; G[5] = 0x14u; G[16] = G[4]; call_here(rt, ctx, 0x08A644FCu); G[2] = 0x08CD2BB0u; put(rt, G[16], G[2]); G[4] = G[16] + 0xCu; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A90480u); }
void func_08A64A00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[6]; G[6] = G[5]; G[5] = 0xAu; G[16] = G[4]; call_here(rt, ctx, 0x08A644FCu); G[2] = 0x08CD2A20u; put(rt, G[16], G[2]); G[4] = G[16] + 0xCu; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A90480u); }
void func_08A66DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x24u; G[16] = G[4]; store8(G[4] + 4u, G[2]); G[2] = 0x08CD2E30u; G[3] = word(rt, G[4] + 4u); put(rt, G[4], G[2]); G[4] += 8u; G[3] &= 0xFFu; put(rt, G[16] + 4u, G[3]); call_here(rt, ctx, 0x08A75D04u); put(rt, G[16] + 0x14u, 0u); } G[16] = s0; }
void func_08A670C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x26u; G[16] = G[4]; store8(G[4] + 4u, G[2]); G[2] = 0x08CD2E80u; G[3] = word(rt, G[4] + 4u); put(rt, G[4], G[2]); G[4] += 8u; G[3] &= 0xFFu; put(rt, G[16] + 4u, G[3]); call_here(rt, ctx, 0x08A75D04u); put(rt, G[16] + 0x14u, 0u); } G[16] = s0; }
void func_08A81288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB8798u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x2Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x14u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C28648u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A85278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBEA58u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x14u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) { G[2] = word(rt, G[3]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A780B0u); }
void func_08A948DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = frame.at(0u); G[2] = word(rt, G[5] + 0x14u) + G[6]; G[5] = load8(G[2]); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16]; } G[16] = s0; }
void func_08A9A948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[17] = G[4]; G[4] = G[2] + 0x14u; G[16] = G[6]; G[5] = word(rt, G[5] + 0x14u); call_here(rt, ctx, 0x08C2F2E4u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A76ABCu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08AA1A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] &= 0xFFFFu; G[3] = 8u; G[6] = frame.at(0u); G[7] = 0xFFFFFFF8u; do { G[2] = sra(G[5], G[3] & 31u); G[3] -= 8u; store8(G[6], G[2]); G[6] += 1u; } while (G[3] != G[7]); G[3] = word(rt, G[4]); G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x10Cu); G[6] = 2u; call_here(rt, ctx, G[2]); } }
void func_08AAB940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[5] = 0x08CB3198u; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B37640u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08BEED1Cu); G[2] = G[16]; } G[16] = s0; }
void func_08AAE288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x28u; G[16] = G[4] + 0x24u; G[4] = G[16]; G[5] = word(rt, G[16] + 8u); call_here(rt, ctx, 0x08C2AE10u); put(rt, G[16] + 0x10u, G[17]); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0xCu, G[17]); put(rt, G[16] + 8u, 0u); } G[16] = s0; G[17] = s1; }
void func_08ABA314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08CFFA28u); G[16] = word(rt, 0x08CFFA28u); while (G[16] != 0u) { G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0x4u); G[16] = G[3]; } } G[16] = s0; }
void func_08ABA360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08CFFA24u); G[16] = word(rt, 0x08CFFA24u); while (G[16] != 0u) { G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0x8u); G[16] = G[3]; } } G[16] = s0; }
void func_08ABA3AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08CFFA28u); G[16] = word(rt, 0x08CFFA28u); while (G[16] != 0u) { G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x14u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0x4u); G[16] = G[3]; } } G[16] = s0; }
void func_08ABC650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D7718Cu, 0xCu);
}
void func_08ABC69C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D7718Cu, 0x10u);
}
void func_08ABCDE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D77190u); G[16] = word(rt, 0x08D77190u); while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[16]); } } G[16] = s0; jump(ctx, 0x08ABB298u); }
void func_08ABDF48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D77194u, 0xCu);
}
void func_08AC6974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B28DCCu); G[2] = 0xFFFFFFFFu; store8(G[16] + 0x28u, 0u); put(rt, G[16] + 0x2Cu, G[2]); put(rt, G[16] + 0x44u, 0u); put(rt, G[16] + 0x30u, 0u); store8(G[16] + 0x29u, 0u); put(rt, G[16] + 0x34u, 0u); put(rt, G[16] + 0x38u, 0u); put(rt, G[16] + 0x3Cu, 0u); put(rt, G[16] + 0x40u, 0u); } G[16] = s0; }
void func_08AC7140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16] + 0x2Cu); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x2Cu, G[2]); call_here(rt, ctx, 0x08C7AB38u); G[4] = G[16]; call_here(rt, ctx, 0x08B3457Cu); } G[2] = 1u; } G[16] = s0; }
void func_08ACAC0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x50u); G[5] = frame.at(0u); G[6] = 0x40u; G[16] = G[4]; call_here(rt, ctx, 0x08ACA30Cu); G[4] = G[16]; G[5] = frame.at(0u); G[6] = G[16]; G[7] = 8u; G[8] = G[16] + 0x220u; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08ACAA20u); } G[16] = s0; }
void func_08ACB73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 0x08D77204u; G[6] = 0x08D771D0u; G[16] = G[4]; call_here(rt, ctx, 0x08ABFD0Cu); G[2] = 0x08CD6A18u; put(rt, G[16], G[2]); G[4] = G[16] + 0xA8u; store8(G[16] + 0xA4u, 0u); store8(G[16] + 0xA5u, 0u); } G[16] = s0; jump(ctx, 0x08B28D34u); }
void func_08ACBBA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0xA8u; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); call_here(rt, ctx, 0x08C7B1E8u); G[16] = G[2]; G[4] = G[17]; G[16] >>= 31; call_here(rt, ctx, 0x08B34B24u); G[2] = G[16] ^ 1u; } G[16] = s0; G[17] = s1; }
void func_08ADB39C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, 0x08D40638u); G[5] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; while (G[3] != 0u) { put(rt, G[4], G[2]); G[5] += 1u; G[4] += 4u; G[3] = word(rt, G[2] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; } G[2] = G[5]; }
void func_08ADE268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, 0x08D4067Cu); G[5] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; while (G[3] != 0u) { put(rt, G[4], G[2]); G[5] += 1u; G[4] += 4u; G[3] = word(rt, G[2] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[2] = G[3]; } G[2] = G[5]; }
void func_08ADB4BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D40628u); G[3] = word(rt, 0x08D40628u); G[16] = 0x08D40628u; if (G[3] != 0u) { G[4] = word(rt, G[16] + 0x4u); call_here(rt, ctx, 0x08AD85C8u); G[4] = G[16]; G[5] = 0u; G[6] = 0x4Cu; call_here(rt, ctx, 0x08B36018u); } } G[16] = s0; }
void func_08ADDF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D40674u); G[3] = word(rt, 0x08D40674u); G[16] = 0x08D40674u; if (G[3] != 0u) { G[4] = word(rt, G[16] + 0x24u); call_here(rt, ctx, 0x08AD85C8u); G[4] = G[16]; G[5] = 0u; G[6] = 0x2BCu; call_here(rt, ctx, 0x08B36018u); } } G[16] = s0; }
void func_08AE94DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D40A38u); G[3] = word(rt, 0x08D40A38u); G[16] = 0x08D40A38u; if (G[3] != 0u) { G[4] = word(rt, G[16] + 0x4u); call_here(rt, ctx, 0x08AD85C8u); G[4] = G[16]; G[5] = 0u; G[6] = 0x108Cu; call_here(rt, ctx, 0x08B36018u); } } G[16] = s0; }
void func_08ADBAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D40634u); G[2] = 0x08D40634u; for (;;) { G[4] = word(rt, G[2] + 4u); G[2] = 0u; G[3] = word(rt, G[4] + 8u); G[3] = static_cast<std::int32_t>(G[3]) < 0 ? 1u : 0u; if (G[3] == 0u) G[2] = G[4]; if (G[2] == 0u) break; G[4] = word(rt, G[2] + 8u); call_here(rt, ctx, 0x08ADBA04u); G[2] = 0x08D40634u; } } G[16] = s0; }
void func_08AE4778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x50u); G[3] = G[5]; G[2] = 0xFu; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[3]); put(rt, frame.at(8u), G[6]); if (G[7] != 0u) call_here(rt, ctx, 0x08AE4568u); else { G[4] = sext(load16(G[4] + 0x1Au), 16u); G[5] = frame.at(0u); call_here(rt, ctx, 0x08AE85E8u); } } }
void func_08AEA0A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D41AC4u); G[2] = word(rt, 0x08D41AC4u); G[17] = 0x08D41AC4u; if (G[2] != 0u) { G[4] = word(rt, G[17] + 4u); call_here(rt, ctx, 0x08AD85C8u); put(rt, 0x08D41AC4u, 0u); put(rt, G[17] + 8u, 0u); put(rt, G[17] + 4u, 0u); } } G[16] = s0; G[17] = s1; }
void func_08AEDBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[5] = 0x08C92364u; call_here(rt, ctx, 0x08AEDA20u); G[3] = G[2]; if (G[2] != 0u) { G[2] = static_cast<std::int32_t>(G[16]) < 0x4000 ? G[16] : 0x4000u; if (static_cast<std::int32_t>(G[16]) <= 0) G[2] = 1u; store16(G[3] + 0x60A4u, G[2]); } } G[16] = s0; }
void func_08AFBAB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7738u; G[3] = 0x08CD777Cu; G[16] = G[4]; put(rt, G[4], G[2]); put(rt, G[4] + 0x28u, G[3]); call_here(rt, ctx, 0x08AFACA4u); G[2] = 0x08CBF428u; put(rt, G[16] + 0x28u, G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08AFB02Cu); }
void func_08B03124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x1FCu); G[3] = G[4]; G[5] = G[4]; G[2] = ((G[2] >> 8) & 0xFu) << 4; G[4] += G[2]; if (G[4] != G[3]) { ctx.fpr[1] = float_at(rt, G[5] + 4u); for (;;) { ctx.fpr[0] = float_at(rt, G[3] + 4u); if (ctx.fpr[0] < ctx.fpr[1]) G[5] = G[3]; G[3] += 0x10u; if (G[4] == G[3]) break; ctx.fpr[1] = float_at(rt, G[5] + 4u); } } G[2] = G[5]; }
void func_08B09298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[5]; G[7] = G[4]; G[8] = G[5] + 0x20u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); }
void func_08B0B294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u); G[4] = 0u; G[2] = 0u; if (G[3] == 0u) return; G[2] = word(rt, G[3] + 0xCu); G[3] += 0x20u; G[6] = __builtin_bswap32(G[2]); for (;;) { G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[4] += 1u; if (G[2] == 0u) return; G[2] = __builtin_bswap32(word(rt, G[3] + 4u)); if (G[2] == G[5]) { G[2] = __builtin_bswap32(word(rt, G[3] + 8u)); return; } G[3] += 0x10u; } }
void func_08B0D01C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x3Cu); G[16] = G[4]; call_here(rt, ctx, G[2]); G[4] = G[16]; if (G[2] == 0u) { G[2] = word(rt, G[16]); G[25] = word(rt, G[2] + 0x60u); tail = true; } } G[16] = s0; if (tail) jump(ctx, G[25]); }
void func_08B0D828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D7734Cu, 0x20u);
}
void func_08B0D874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D7734Cu, 0x10u);
}
void func_08B0D8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D7734Cu); G[16] = word(rt, 0x08D7734Cu); while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[16] = word(rt, G[16]); } } G[16] = s0; }
void func_08B0D90C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D7734Cu); G[16] = word(rt, 0x08D7734Cu); while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[16]); } } G[16] = s0; }
void func_08B1F12C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D773ECu, 0xCu);
}
void func_08B0E964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7C68u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD7C98u; put(rt, G[16] + 0xF8u, G[2]); G[4] += 0xF8u; call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 0x0u; call_here(rt, ctx, 0x08B0DDDCu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B22B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9588u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CD95A0u; put(rt, G[16] + 0x2Cu, G[2]); G[4] += 0x2Cu; call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 0x4u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B17240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = high_half(0x08D61100u); G[16] = G[4]; G[4] = 0x08D61100u; call_here(rt, ctx, 0x08B34888u); G[2] = 0x08D51100u; G[4] = G[16]; put(rt, G[16] + 0x10u, G[2]); call_here(rt, ctx, 0x08B181E0u); G[4] = 0x08D61100u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34B24u); }
void func_08B18BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[6] = G[4]; G[7] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[5]); G[5] = 0u; G[4] = G[3]; if (G[3] == G[2]) { G[3] = word(rt, G[3]); G[5] = word(rt, G[6] + 4u); G[2] = word(rt, G[3] + 0x20u); G[6] = word(rt, G[7] + 4u); call_here(rt, ctx, G[2]); G[5] = 1u; } G[2] = G[5]; } }
void func_08B1AB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D689E4u); G[4] = 0x08D689E4u; call_here(rt, ctx, 0x08B344E0u); G[4] = load8(0x08D68A2Eu); G[3] = 1u; G[2] = high_half(0x08D68A2Du); store8(0x08D68A2Du, G[3]); if (G[4] == 0u) call_here(rt, ctx, 0x08B1A9E4u); G[4] = 0x08D689E4u; } G[16] = s0; jump(ctx, 0x08B3457Cu); }
void func_08B1BE60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B1BE58u); G[5] = G[2]; G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08B1BAA4u); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) G[3] = G[16]; G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_08B1C158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = G[5]; call_here(rt, ctx, 0x08B1BE38u); G[5] = G[2]; G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08B1C140u); G[3] = 0xFFFFFFFFu; if (G[2] != 0u) G[3] = G[16]; G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_08B1C218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[5] != 0u) { call_here(rt, ctx, 0x08B1C1A4u); put(rt, G[16] + 8u, G[2]); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08B1C158u); put(rt, G[16] + 0xCu, G[2]); } } G[16] = s0; G[17] = s1; }
void func_08B1EF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B1EEF0u); } ctx.fpr[0] = std::bit_cast<float>(G[2]); G[3] = G[2] >> 1; G[4] = (G[2] & 1u) | G[3]; ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[2])); if (static_cast<std::int32_t>(G[2]) < 0) { ctx.fpr[0] = std::bit_cast<float>(G[4]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[4])); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[1]; } G[1] = 0x2F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); }
void func_08B20D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B22C04u); G[2] = 0x08C9376Cu; G[3] = 0x08CD94F8u; put(rt, G[16] + 8u, G[2]); put(rt, G[16], G[3]); put(rt, G[16] + 0xCu, G[17]); } G[16] = s0; G[17] = s1; }
void func_08B26B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] & 1u; G[7] = G[5] - G[2]; G[3] = load16(G[7]); G[2] <<= 3; G[5] += G[6]; G[3] = sra(G[3], G[2] & 31u); G[8] = G[5] + 1u; G[2] = 0u - G[2]; G[7] += 2u; G[8] &= ~1u; put(rt, G[4] + 4u, G[3]); store16(G[4] + 0x20u, G[2]); put(rt, G[4] + 8u, G[7]); put(rt, G[4] + 0xCu, G[5]); if (static_cast<std::int32_t>(G[6]) >= 0 && G[5] != G[8]) put(rt, G[4] + 0xCu, G[8]); }
void func_08B2A3EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x20u); G[2] = G[8]; G[3] = G[7]; G[7] = 0x08B2A3D0u; G[8] = frame.at(0u); if (G[2] == 0u) tail = true; else { put(rt, frame.at(0u), G[3]); put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08C4FD6Cu); } } if (tail) jump(ctx, 0x08C4FD3Cu); }
void func_08B2A634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x20u); G[2] = G[8]; G[3] = G[7]; G[7] = 0x08B2A3D0u; G[8] = frame.at(0u); if (G[2] == 0u) tail = true; else { put(rt, frame.at(0u), G[3]); put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08C501E0u); } } if (tail) jump(ctx, 0x08C501D8u); }
void func_08B2B330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_global_list(rt, ctx, 0x08D773F4u, 0xCu);
}
void func_08B2D78C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = 0x03416102u; G[2] = 1u; G[6] = frame.at(0u); G[7] = 4u; G[8] = 0u; G[9] = 0u; put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08C7A828u); if (static_cast<std::int32_t>(G[2]) < 0) { G[4] = 7u; G[3] = high_half(0x08D68CF8u); put(rt, 0x08D68CF8u, G[4]); } } }
void func_08B2D7D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = 0x03416102u; G[2] = 2u; G[6] = frame.at(0u); G[7] = 4u; G[8] = 0u; G[9] = 0u; put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08C7A828u); if (static_cast<std::int32_t>(G[2]) < 0) { G[4] = 7u; G[3] = high_half(0x08D68CF8u); put(rt, 0x08D68CF8u, G[4]); } } }
void func_08B332DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x08D691D8u; G[16] = 0x08D691D8u; call_here(rt, ctx, 0x08B34D40u); G[4] = G[16]; G[2] = word(rt, G[16] + 0x10u); G[3] = word(rt, G[16] + 0x14u); put(rt, G[17], G[2]); put(rt, G[17] + 4u, G[3]); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B34D80u); }
void func_08B3E670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x10u); G[19] = G[7]; G[18] = G[6]; G[16] = G[4]; G[6] = word(rt, G[16] + 0xCu); G[4] = word(rt, G[4] + 4u); call_here(rt, ctx, 0x08B79A88u); put(rt, G[16] + 0x18u, G[18]); put(rt, G[16] + 0x1Cu, G[19]); } G[16] = s0; G[18] = s2; G[19] = s3; }
void func_08B3EA3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 0x640u); G[2] = static_cast<std::int32_t>(G[5]) < 100 ? 1u : 0u; G[8] = G[4]; if (G[2] == 0u) { put(rt, G[4] + 0x640u, 0u); G[5] = 0u; } G[2] = word(rt, G[8] + 0x644u); G[3] = (G[5] << 3) + G[8]; G[5] += 1u; G[9] = G[2] + 1u; G[2] = static_cast<std::int32_t>(G[2]) < 100 ? 1u : 0u; put(rt, G[3], G[6]); put(rt, G[3] + 4u, G[7]); put(rt, G[8] + 0x640u, G[5]); if (G[2] != 0u) put(rt, G[8] + 0x644u, G[9]); jump(ctx, 0x08B3E908u); }
void func_08B3EAE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = 0x20u; G[17] = 0u; G[16] = G[4]; do { G[4] = G[16]; G[17] += 1u; call_here(rt, ctx, 0x08B3E714u); G[16] += 0x18u; } while (G[17] != G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B42310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDA9A0u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B404C8u); G[4] = G[16] + 0x12Cu; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0xCCu; call_here(rt, ctx, 0x08B2C3BCu); G[4] = G[16]; call_here(rt, ctx, 0x08B08420u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B46328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x14u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[5] = word(rt, G[16]); store8(G[16] + 0x8Cu, 0u); G[2] = word(rt, G[5] + 0x4Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[3] + 0x18u); } G[16] = s0; jump(ctx, G[25]); }
void func_08B4D434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {  ctx.fpr[1] = float_at(rt, G[5] + 0x0u); put_float(rt, G[4] + 0x0u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x4u); put_float(rt, G[4] + 0xCu, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x8u); put_float(rt, G[4] + 0x18u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put_float(rt, G[4] + 0x4u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x10u); put_float(rt, G[4] + 0x10u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x14u); put_float(rt, G[4] + 0x1Cu, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x18u); put_float(rt, G[4] + 0x8u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x1Cu); put_float(rt, G[4] + 0x14u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x20u); put_float(rt, G[4] + 0x20u, ctx.fpr[1]); }
void func_08B5A4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[4] = high_half(0x08D6CE00u); G[16] = 0x08D6CE00u; G[4] = G[16]; call_here(rt, ctx, 0x08C7AA38u); G[4] = word(rt, G[16] + 0x30u); call_here(rt, ctx, 0x08C7AC28u); G[4] = word(rt, G[16] + 0x24u); G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x08B84B5Cu); G[3] = high_half(0x08D6E0F0u); store8(0x08D6E0F0u, 0u); } G[16] = s0; }
void func_08B5DDFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = high_half(0x08D726DCu); G[4] = 0x08D726DCu; G[5] = 2u; G[16] = 0x08D726DCu; call_here(rt, ctx, 0x08B5D788u); G[4] = high_half(0x08D726D8u); G[2] = high_half(0x08D6E124u); G[3] = high_half(0x08D6E120u); G[5] = G[16]; put(rt, 0x08D6E124u, G[16]); G[4] = 0x08D726D8u; put(rt, 0x08D6E120u, G[16]); } G[16] = s0; jump(ctx, 0x08B5C14Cu); }
void func_08B64824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    construct_with_handle(rt, ctx, 0x08CDC110u, frame.at(0x0u));
}
void func_08B64890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    construct_with_handle(rt, ctx, 0x08CDC110u, frame.at(0x0u));
}
void func_08B648FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDC110u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B61BC8u); G[2] = 0x08CDB768u; put(rt, G[17], G[2]); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B5C280u); }
void func_08B64968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDC110u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] += 8u; call_here(rt, ctx, 0x08B61BC8u); G[2] = 0x08CDB768u; put(rt, G[17], G[2]); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B5C280u); }
void func_08B76858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = 0x18u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35DA0u); G[5] = G[2]; G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B7684Cu); }
void func_08B78E7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B789A4u); }
void func_08B7DFC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B7E014u); }
void func_08B7B298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B7B0CCu); G[4] = 0xFFFFFFFFu; if (G[2] == 0u) { G[2] = word(rt, G[16] + 4u); G[4] = word(rt, G[16]); G[2] -= 1u; G[3] = G[4] + 1u; put(rt, G[16] + 4u, G[2]); G[4] = load8(G[4]); put(rt, G[16], G[3]); } G[2] = G[4]; } G[16] = s0; }
void func_08B7DC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; G[5] = G[16]; G[6] = G[17]; call_here(rt, ctx, 0x08B7CABCu); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71EC8u); } G[16] = s0; G[17] = s1; }
void func_08B840AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = high_half(0x08D740F8u); G[8] = word(rt, 0x08D740F8u); G[7] = G[4]; { GuestFrame frame(ctx, 0x10u); G[3] = 0x80420000u; G[2] = high_half(0x08D74100u); G[6] = G[5]; G[5] = G[7]; G[4] = 0x08D74100u; G[7] = 0x80420100u; if (G[8] != 0u) { call_here(rt, ctx, 0x08C7B0B0u); G[7] = G[2]; } G[2] = G[7]; } }
void func_08B86C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = G[4]; G[2] = word(rt, G[4] + 0x1Cu); G[4] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[3] + 0x18u); G[4] = 0x08C97DBCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, G[16] + 0xCu); } G[2] = G[4]; } G[16] = s0; }
void func_08B891EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; G[9] = G[5]; G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) G[8] += 0x10u; else { G[2] = word(rt, G[8]); G[3] = word(rt, G[8] + 4u); G[4] = word(rt, G[8] + 8u); G[5] = word(rt, G[8] + 0xCu); put(rt, G[7], G[2]); put(rt, G[7] + 4u, G[3]); put(rt, G[7] + 8u, G[4]); put(rt, G[7] + 0xCu, G[5]); G[8] += 0x10u; } G[7] += 0x10u; } while (G[9] != G[8]); } G[2] = G[7]; }
void func_08B9EC14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 0x10u; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC0D38u; put(rt, G[2], G[3]); G[5] = word(rt, G[16] + 0xCu); G[3] = word(rt, G[16] + 4u); G[6] = word(rt, G[16] + 8u); put(rt, G[2] + 0xCu, G[5]); put(rt, G[2] + 4u, G[3]); put(rt, G[2] + 8u, G[6]); } G[16] = s0; }
void func_08BEFE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x0896418Cu); G[4] = G[16]; G[2] = word(rt, G[17] + 0x10u); G[5] = frame.at(0u); put(rt, frame.at(0x10u), G[2]); call_here(rt, ctx, 0x08A044D4u); G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08BF8D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[7] = G[4]; G[3] = word(rt, G[4] + 8u); G[2] = sra(G[3], 1u); G[3] &= 1u; G[4] = G[5] + G[2]; if (G[3] != 0u) { G[3] = word(rt, G[7] + 4u); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else G[2] = word(rt, G[7] + 4u); G[5] = G[6]; call_here(rt, ctx, G[2]); } }
void func_08C00980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2AB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2AB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2AB98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C2F9A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = word(rt, G[4]); if (G[4] != G[17]) { do { G[16] = word(rt, G[4]); G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = G[16]; } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C15E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD04B0u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 0xD8u; put(rt, G[16] + 0x14u, 0u); call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0xACu; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08A1A9A0u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A2A4B0u); }
void func_08C1CC84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = sra(G[5] - G[4], 2u); G[7] = sra(G[5], 1u); if (static_cast<std::int32_t>(G[5]) > 0) { G[6] = load16(G[6]); do { G[2] = G[7] << 2; G[8] = G[4] + G[2]; G[3] = word(rt, G[8]); G[2] = G[5] - G[7]; G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[5] = G[2] - 1u; if (G[3] != 0u) G[4] = G[8] + 4u; else G[5] = G[7]; G[7] = sra(G[5], 1u); } while (static_cast<std::int32_t>(G[5]) > 0); } G[2] = G[4]; }
void func_08C1CD28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = sra(G[5] - G[4], 2u); G[7] = sra(G[5], 1u); if (static_cast<std::int32_t>(G[5]) > 0) { G[6] = word(rt, G[6]); do { G[2] = G[7] << 2; G[8] = G[4] + G[2]; G[3] = word(rt, G[8]); G[2] = G[5] - G[7]; G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[5] = G[2] - 1u; if (G[3] != 0u) G[4] = G[8] + 4u; else G[5] = G[7]; G[7] = sra(G[5], 1u); } while (static_cast<std::int32_t>(G[5]) > 0); } G[2] = G[4]; }
void func_08C2B500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; if (G[5] != G[4]) { do { G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[16] += 0xCu; call_here(rt, ctx, 0x08A69D68u); } while (G[17] != G[16]); } } G[16] = s0; G[17] = s1; }
void func_08C3FD78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] <<= 3; G[16] = G[4] + G[5]; G[8] = G[6]; G[5] = G[16]; G[6] = frame.at(0u); if (G[4] == 0u) G[2] = 0u; else { put(rt, frame.at(4u), G[8]); put(rt, frame.at(0u), G[7]); call_here(rt, ctx, 0x08C3FC14u); if (G[16] == G[2]) G[2] = 0u; } } G[16] = s0; }
void func_08C4605C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = 0.0f; ctx.fpr[0] = float_at(rt, G[4]); G[2] = 1u; if (ctx.fpr[0] == ctx.fpr[1]) { ctx.fpr[0] = float_at(rt, G[4] + 4u); G[2] = 0u; if (!(ctx.fpr[0] == ctx.fpr[1])) G[2] = 1u; else { ctx.fpr[0] = float_at(rt, G[4] + 8u); G[2] = ctx.fpr[0] == ctx.fpr[1] ? 0u : 1u; } } }
void func_08C464BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {  ctx.fpr[1] = float_at(rt, G[5] + 0x0u); put_float(rt, G[4] + 0x0u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x4u); put_float(rt, G[4] + 0xCu, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x8u); put_float(rt, G[4] + 0x18u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0xCu); put_float(rt, G[4] + 0x4u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x10u); put_float(rt, G[4] + 0x10u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x14u); put_float(rt, G[4] + 0x1Cu, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x18u); put_float(rt, G[4] + 0x8u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[5] + 0x1Cu); put_float(rt, G[4] + 0x14u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[5] + 0x20u); put_float(rt, G[4] + 0x20u, ctx.fpr[1]); }
void func_08C47104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[5]; G[7] = G[6]; if (G[4] != G[5]) { G[6] = G[4]; do { const bool skip = G[7] == 0u; G[6] += 8u; if (!skip) { G[5] = load16(G[4] + 6u); G[2] = load16(G[4]); G[3] = load16(G[4] + 2u); G[4] = load16(G[4] + 4u); store16(G[7], G[2]); store16(G[7] + 2u, G[3]); store16(G[7] + 4u, G[4]); store16(G[7] + 6u, G[5]); } G[4] = G[6]; G[7] += 8u; } while (G[8] != G[6]); } G[2] = G[7]; }
void func_08C49EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; G[9] = G[5]; G[7] = G[6]; if (G[4] != G[5]) { do { if (G[7] == 0u) G[8] += 8u; else { G[2] = load16(G[8]); G[3] = load16(G[8] + 2u); G[4] = load16(G[8] + 4u); G[5] = load16(G[8] + 6u); store16(G[7], G[2]); store16(G[7] + 2u, G[3]); store16(G[7] + 4u, G[4]); store16(G[7] + 6u, G[5]); G[8] += 8u; } G[7] += 8u; } while (G[9] != G[8]); } G[2] = G[7]; }
void func_08C4F420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[5] + 0x14u; G[3] = word(rt, G[5] + 0x14u); G[2] = word(rt, G[5] + 4u); G[5] = 0u; if (G[3] != G[2]) { call_here(rt, ctx, 0x08B1AC1Cu); G[5] = G[2]; } G[2] = word(rt, G[16] + 8u); G[2] = (G[5] ^ G[2]) == 0u ? 1u : 0u; } G[16] = s0; }
void func_08C546DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B65660u); G[2] = 0x08CDB088u; put(rt, G[16], G[2]); put(rt, G[16] + 0x84u, 0u); put(rt, G[16] + 0x70u, 0u); store8(G[16] + 0x74u, 0u); store8(G[16] + 0x75u, 0u); put(rt, G[16] + 0x78u, 0u); put(rt, G[16] + 0x7Cu, 0u); put(rt, G[16] + 0x80u, 0u); } G[16] = s0; }
void func_08C54728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B65660u); G[2] = 0x08CDB088u; put(rt, G[16], G[2]); put(rt, G[16] + 0x84u, 0u); put(rt, G[16] + 0x70u, 0u); store8(G[16] + 0x74u, 0u); store8(G[16] + 0x75u, 0u); put(rt, G[16] + 0x78u, 0u); put(rt, G[16] + 0x7Cu, 0u); put(rt, G[16] + 0x80u, 0u); } G[16] = s0; }
void func_08C553B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[9] = word(rt, G[5] + 0xCu); G[10] = word(rt, G[5] + 4u); G[3] = word(rt, G[5]); G[6] = word(rt, G[9] - 0xCu); G[7] = word(rt, G[10] - 0xCu); G[2] = word(rt, G[3] - 0xCu); G[11] = word(rt, G[5] + 8u); G[3] = word(rt, G[5] + 0x14u); G[8] = word(rt, G[5] + 0x10u); G[2] = G[4] + G[2]; G[6] = G[4] + G[6]; G[7] = G[4] + G[7]; put(rt, G[2], G[3]); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, G[9]); put(rt, G[6] + 8u, G[8]); put(rt, G[4], G[10]); put(rt, G[7], G[11]); }
void func_08C57034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0xAu; G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C5477Cu); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08C56DDCu); }
void func_08C5A97C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    construct_with_handle(rt, ctx, 0x08CDB7C0u, frame.at(0x0u));
}
void func_08C5E450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    construct_with_handle(rt, ctx, 0x08CDB7C0u, frame.at(0x0u));
}
void func_08C5D248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB838u; G[5] = G[5] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D2A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB838u; G[5] = G[5] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB800u; G[5] = G[5] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C5D4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB800u; G[5] = G[5] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 4u, G[5]); G[5] = 0u; put(rt, G[4], G[2]); put(rt, G[4] + 8u, 0u); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C5D308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB838u; G[3] = G[6] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB838u; G[3] = G[6] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60910u);  } G[16] = s0; G[17] = s1; }
void func_08C5D538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB800u; G[3] = G[6] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C5D598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDB800u; G[3] = G[6] != 0u ? 1u : 0u; G[6] = 0u; G[17] = G[4]; put(rt, G[4] + 8u, G[5]); G[5] = 0u; put(rt, G[4] + 4u, G[3]); put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B60C98u);  } G[16] = s0; G[17] = s1; }
void func_08C6F4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 0xAu; G[16] = G[4]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C5477Cu); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B5F7ECu); }
void func_08C72FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x24u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = 0xFFFFFFFFu; G[4] = 0xFFFFFFFFu; if (G[2] != G[3]) { G[2] = word(rt, G[16] + 8u); G[3] = G[2] + 1u; G[4] = load8(G[2]); put(rt, G[16] + 8u, G[3]); } G[2] = G[4]; } G[16] = s0; }
void func_08C78278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0xCu; call_here(rt, ctx, 0x08C78130u); if (G[2] == 0u) { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); tail = true; } } G[16] = s0; if (tail) { G[5] |= 4u; jump(ctx, 0x08C54CFCu); } }
void func_08C782C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x4u; call_here(rt, ctx, 0x08C78130u); if (G[2] == 0u) { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); tail = true; } } G[16] = s0; if (tail) { G[5] |= 4u; jump(ctx, 0x08C54CFCu); } }
void func_08C78310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] += 0x8u; call_here(rt, ctx, 0x08C78130u); if (G[2] == 0u) { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); tail = true; } } G[16] = s0; if (tail) { G[5] |= 4u; jump(ctx, 0x08C54CFCu); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_184
} // namespace gt::game

#define HOT184(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_184::name, gt::ReturnKind::kind)
HOT184(0x08805BC0u, func_08805BC0, Word);
HOT184(0x08809574u, func_08809574, Word);
// HOT184(0x0880AE0Cu, func_0880AE0C, Word);  // no comparable synthetic case yet
HOT184(0x0885D480u, func_0885D480, Word);
HOT184(0x0887BF80u, func_0887BF80, Float);
HOT184(0x0887C89Cu, func_0887C89C, Word);
HOT184(0x0887D034u, func_0887D034, Word);
HOT184(0x088A4218u, func_088A4218, Word);
HOT184(0x088A444Cu, func_088A444C, Word);
HOT184(0x088A7398u, func_088A7398, Word);
// HOT184(0x088B160Cu, func_088B160C, Word);  // no comparable synthetic case yet
HOT184(0x088BD23Cu, func_088BD23C, Float);
HOT184(0x088CF4ACu, func_088CF4AC, Float);
HOT184(0x088CF978u, func_088CF978, Word);
HOT184(0x088D0A14u, func_088D0A14, Word);
HOT184(0x088D0C80u, func_088D0C80, Word);
HOT184(0x088D0D78u, func_088D0D78, Word);
HOT184(0x088D435Cu, func_088D435C, Word);
HOT184(0x088D44BCu, func_088D44BC, Word);
HOT184(0x088D8074u, func_088D8074, Word);
HOT184(0x088DADB4u, func_088DADB4, Word);
HOT184(0x088DC8C0u, func_088DC8C0, Word);
HOT184(0x088E72E0u, func_088E72E0, Word);
HOT184(0x088E8348u, func_088E8348, Word);
HOT184(0x088ED088u, func_088ED088, Word);
HOT184(0x088F1C40u, func_088F1C40, Word);
HOT184(0x088F2CECu, func_088F2CEC, Word);
HOT184(0x088F7490u, func_088F7490, Word);
HOT184(0x088F74DCu, func_088F74DC, Word);
HOT184(0x088FBBB0u, func_088FBBB0, Word);
HOT184(0x0890B06Cu, func_0890B06C, Word);
HOT184(0x08925F98u, func_08925F98, Word);
HOT184(0x08926250u, func_08926250, Word);
HOT184(0x08938638u, func_08938638, Word);
HOT184(0x0893C91Cu, func_0893C91C, Float);
HOT184(0x08945E80u, func_08945E80, Word);
HOT184(0x089582ACu, func_089582AC, Word);
HOT184(0x0895936Cu, func_0895936C, Word);
HOT184(0x08959AC0u, func_08959AC0, Float);
HOT184(0x08959B0Cu, func_08959B0C, Float);
HOT184(0x08966370u, func_08966370, Word);
HOT184(0x08966434u, func_08966434, Word);
HOT184(0x08968F98u, func_08968F98, Word);
HOT184(0x08994D78u, func_08994D78, Word);
// HOT184(0x08994DC4u, func_08994DC4, Word);  // no comparable synthetic case yet
HOT184(0x08996308u, func_08996308, Float);
HOT184(0x089A5314u, func_089A5314, Word);
HOT184(0x089A56E0u, func_089A56E0, Word);
HOT184(0x089A6EA4u, func_089A6EA4, Word);
HOT184(0x089A6F38u, func_089A6F38, Word);
HOT184(0x089A70D4u, func_089A70D4, Word);
HOT184(0x089CDFF0u, func_089CDFF0, Word);
HOT184(0x089CE124u, func_089CE124, Word);
// HOT184(0x089E8848u, func_089E8848, Word);  // no comparable synthetic case yet
HOT184(0x08A03ABCu, func_08A03ABC, Word);
HOT184(0x08A0AD9Cu, func_08A0AD9C, Word);
// HOT184(0x08A135B8u, func_08A135B8, Word);  // no comparable synthetic case yet
// HOT184(0x08A139E4u, func_08A139E4, Word);  // no comparable synthetic case yet
HOT184(0x08A13A30u, func_08A13A30, Word);
HOT184(0x08A1AA6Cu, func_08A1AA6C, Word);
// HOT184(0x08A17CD8u, func_08A17CD8, Word);  // no comparable synthetic case yet
HOT184(0x08A17EBCu, func_08A17EBC, Word);
HOT184(0x08A237ECu, func_08A237EC, Word);
HOT184(0x08A33228u, func_08A33228, Word);
HOT184(0x08A342CCu, func_08A342CC, Word);
HOT184(0x08A34B04u, func_08A34B04, Word);
// HOT184(0x08A35114u, func_08A35114, Word);  // no comparable synthetic case yet
// HOT184(0x08A35848u, func_08A35848, Word);  // no comparable synthetic case yet
HOT184(0x08A3A398u, func_08A3A398, Word);
HOT184(0x08A3A434u, func_08A3A434, Word);
HOT184(0x08A3A73Cu, func_08A3A73C, Word);
// HOT184(0x08A3DA14u, func_08A3DA14, Word);  // no comparable synthetic case yet
HOT184(0x08A4C88Cu, func_08A4C88C, Word);
HOT184(0x08A4DD6Cu, func_08A4DD6C, Word);
HOT184(0x08A4FD08u, func_08A4FD08, Word);
HOT184(0x08A50198u, func_08A50198, Word);
HOT184(0x08A57F2Cu, func_08A57F2C, Word);
HOT184(0x08A58928u, func_08A58928, Word);
HOT184(0x08A58FC8u, func_08A58FC8, Word);
HOT184(0x08A5D684u, func_08A5D684, Word);
HOT184(0x08A60E10u, func_08A60E10, Word);
HOT184(0x08A64754u, func_08A64754, Word);
HOT184(0x08A64A00u, func_08A64A00, Word);
// HOT184(0x08A66DB8u, func_08A66DB8, Word);  // no comparable synthetic case yet
HOT184(0x08A670C0u, func_08A670C0, Word);
HOT184(0x08A81288u, func_08A81288, Word);
HOT184(0x08A85278u, func_08A85278, Word);
HOT184(0x08A948DCu, func_08A948DC, Word);
HOT184(0x08A9A948u, func_08A9A948, Word);
HOT184(0x08AA1A8Cu, func_08AA1A8C, Word);
HOT184(0x08AAB940u, func_08AAB940, Word);
HOT184(0x08AAE288u, func_08AAE288, Word);
HOT184(0x08ABA314u, func_08ABA314, Word);
// HOT184(0x08ABA360u, func_08ABA360, Word);  // no comparable synthetic case yet
// HOT184(0x08ABA3ACu, func_08ABA3AC, Word);  // no comparable synthetic case yet
HOT184(0x08ABC650u, func_08ABC650, Word);
HOT184(0x08ABC69Cu, func_08ABC69C, Word);
// HOT184(0x08ABCDE0u, func_08ABCDE0, Word);  // no comparable synthetic case yet
HOT184(0x08ABDF48u, func_08ABDF48, Word);
HOT184(0x08AC6974u, func_08AC6974, Word);
HOT184(0x08AC7140u, func_08AC7140, Word);
// HOT184(0x08ACAC0Cu, func_08ACAC0C, Word);  // no comparable synthetic case yet
HOT184(0x08ACB73Cu, func_08ACB73C, Word);
// HOT184(0x08ACBBA8u, func_08ACBBA8, Word);  // no comparable synthetic case yet
HOT184(0x08ADB39Cu, func_08ADB39C, Word);
HOT184(0x08ADE268u, func_08ADE268, Word);
HOT184(0x08ADB4BCu, func_08ADB4BC, Word);
HOT184(0x08ADDF3Cu, func_08ADDF3C, Word);
HOT184(0x08AE94DCu, func_08AE94DC, Word);
HOT184(0x08ADBAD0u, func_08ADBAD0, Word);
HOT184(0x08AE4778u, func_08AE4778, Word);
HOT184(0x08AEA0A4u, func_08AEA0A4, Word);
HOT184(0x08AEDBF8u, func_08AEDBF8, Word);
HOT184(0x08AFBAB0u, func_08AFBAB0, Word);
HOT184(0x08B03124u, func_08B03124, Word);
HOT184(0x08B09298u, func_08B09298, Word);
HOT184(0x08B0B294u, func_08B0B294, Word);
HOT184(0x08B0D01Cu, func_08B0D01C, Word);
HOT184(0x08B0D828u, func_08B0D828, Word);
HOT184(0x08B0D874u, func_08B0D874, Word);
// HOT184(0x08B0D8C0u, func_08B0D8C0, Word);  // no comparable synthetic case yet
// HOT184(0x08B0D90Cu, func_08B0D90C, Word);  // no comparable synthetic case yet
HOT184(0x08B1F12Cu, func_08B1F12C, Word);
HOT184(0x08B0E964u, func_08B0E964, Word);
HOT184(0x08B22B34u, func_08B22B34, Word);
// HOT184(0x08B17240u, func_08B17240, Word);  // no comparable synthetic case yet
HOT184(0x08B18BF0u, func_08B18BF0, Word);
// HOT184(0x08B1AB08u, func_08B1AB08, Word);  // no comparable synthetic case yet
HOT184(0x08B1BE60u, func_08B1BE60, Word);
HOT184(0x08B1C158u, func_08B1C158, Word);
HOT184(0x08B1C218u, func_08B1C218, Word);
HOT184(0x08B1EF70u, func_08B1EF70, Float);
HOT184(0x08B20D10u, func_08B20D10, Word);
HOT184(0x08B26B28u, func_08B26B28, Word);
HOT184(0x08B2A3ECu, func_08B2A3EC, Word);
HOT184(0x08B2A634u, func_08B2A634, Word);
HOT184(0x08B2B330u, func_08B2B330, Word);
// HOT184(0x08B2D78Cu, func_08B2D78C, Word);  // no comparable synthetic case yet
// HOT184(0x08B2D7D8u, func_08B2D7D8, Word);  // no comparable synthetic case yet
HOT184(0x08B332DCu, func_08B332DC, Word);
HOT184(0x08B3E670u, func_08B3E670, Word);
// HOT184(0x08B3EA3Cu, func_08B3EA3C, Word);  // no comparable synthetic case yet
HOT184(0x08B3EAE4u, func_08B3EAE4, Word);
HOT184(0x08B42310u, func_08B42310, Word);
HOT184(0x08B46328u, func_08B46328, Word);
HOT184(0x08B4D434u, func_08B4D434, Word);
// HOT184(0x08B5A4FCu, func_08B5A4FC, Word);  // no comparable synthetic case yet
HOT184(0x08B5DDFCu, func_08B5DDFC, Word);
HOT184(0x08B64824u, func_08B64824, Word);
HOT184(0x08B64890u, func_08B64890, Word);
HOT184(0x08B648FCu, func_08B648FC, Word);
HOT184(0x08B64968u, func_08B64968, Word);
HOT184(0x08B76858u, func_08B76858, Word);
HOT184(0x08B78E7Cu, func_08B78E7C, Word);
HOT184(0x08B7DFC8u, func_08B7DFC8, Word);
HOT184(0x08B7B298u, func_08B7B298, Word);
HOT184(0x08B7DC58u, func_08B7DC58, Word);
HOT184(0x08B840ACu, func_08B840AC, Word);
HOT184(0x08B86C5Cu, func_08B86C5C, Word);
HOT184(0x08B891ECu, func_08B891EC, Word);
HOT184(0x08B9EC14u, func_08B9EC14, Word);
HOT184(0x08BEFE64u, func_08BEFE64, Word);
HOT184(0x08BF8D48u, func_08BF8D48, Word);
// HOT184(0x08C00980u, func_08C00980, Word);  // no comparable synthetic case yet
// HOT184(0x08C2AB00u, func_08C2AB00, Word);  // no comparable synthetic case yet
// HOT184(0x08C2AB4Cu, func_08C2AB4C, Word);  // no comparable synthetic case yet
// HOT184(0x08C2AB98u, func_08C2AB98, Word);  // no comparable synthetic case yet
// HOT184(0x08C2F9A4u, func_08C2F9A4, Word);  // no comparable synthetic case yet
HOT184(0x08C15E30u, func_08C15E30, Word);
HOT184(0x08C1CC84u, func_08C1CC84, Word);
HOT184(0x08C1CD28u, func_08C1CD28, Word);
HOT184(0x08C2B500u, func_08C2B500, Word);
HOT184(0x08C3FD78u, func_08C3FD78, Word);
HOT184(0x08C4605Cu, func_08C4605C, Word);
HOT184(0x08C464BCu, func_08C464BC, Word);
HOT184(0x08C47104u, func_08C47104, Word);
HOT184(0x08C49EC8u, func_08C49EC8, Word);
HOT184(0x08C4F420u, func_08C4F420, Word);
HOT184(0x08C546DCu, func_08C546DC, Word);
HOT184(0x08C54728u, func_08C54728, Word);
// HOT184(0x08C553B0u, func_08C553B0, Word);  // no comparable synthetic case yet
HOT184(0x08C57034u, func_08C57034, Word);
HOT184(0x08C5A97Cu, func_08C5A97C, Word);
HOT184(0x08C5E450u, func_08C5E450, Word);
HOT184(0x08C5D248u, func_08C5D248, Word);
HOT184(0x08C5D2A8u, func_08C5D2A8, Word);
HOT184(0x08C5D478u, func_08C5D478, Word);
HOT184(0x08C5D4D8u, func_08C5D4D8, Word);
HOT184(0x08C5D308u, func_08C5D308, Word);
HOT184(0x08C5D368u, func_08C5D368, Word);
HOT184(0x08C5D538u, func_08C5D538, Word);
HOT184(0x08C5D598u, func_08C5D598, Word);
// HOT184(0x08C6F4DCu, func_08C6F4DC, Word);  // no comparable synthetic case yet
HOT184(0x08C72FC4u, func_08C72FC4, Word);
HOT184(0x08C78278u, func_08C78278, Word);
// HOT184(0x08C782C4u, func_08C782C4, Word);  // no comparable synthetic case yet
HOT184(0x08C78310u, func_08C78310, Word);
