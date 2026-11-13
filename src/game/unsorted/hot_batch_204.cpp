// Hot batch 204: thirty-nine-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_204 {
namespace {

using namespace gt::game::hot;

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

using u32 = std::uint32_t;
using s32 = std::int32_t;
// A `jal` with a0.. loaded from `arguments` first (the registers the original sets before it); v0 / v1 / f0 as the callee left them.
u32 call_args(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    call_here(rt, ctx, target);
    return ctx.gpr[2];
}
// A function-local static singleton: allocates `size` bytes (0x08B62344), constructs the object with `construct`, stores it at *a0 and
// stores at +4 the address of the static slot at `guard` + 8, filled once with `vtable` between the guard's acquire (0x08B6441C)
// and release (0x08B64494); the guard byte at `guard` says whether that already happened.
void new_with_static(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 size, u32 construct, u32 guard, u32 vtable) {
    const u32 out = ctx.gpr[4];
    const u32 slot = guard + 8u;
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call_args(rt, ctx, 0x08B62344u, {size});
    call_args(rt, ctx, construct, {object});
    put(rt, out, object);
    const u32 initialised = sext(rt.memory().load8(guard), 8);
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[5] = initialised;
    ctx.gpr[4] = guard;
    if (initialised == 0u) {
        const u32 acquired = call_args(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[5] = 0x08D70000u;
        ctx.gpr[4] = guard;
        if (acquired != 0u) {
            ctx.gpr[2] = vtable;
            put(rt, slot, vtable);
            call_args(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, out + 4u, slot);
}
// Builds a heap over the 8-byte elements [a0, a1): from the last parent down to the root, `sift` is called with
// (first, index, count, &element) where the element's two words were copied to the frame (it is not called for fewer than two).
void make_heap_8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 sift) {
    const u32 first = ctx.gpr[4];
    const u32 bytes = ctx.gpr[5] - first;
    GuestFrame frame(ctx, 0x30u);
    const s32 count = static_cast<s32>(sra(bytes, 3));
    ctx.gpr[5] = bytes;
    ctx.gpr[2] = count < 2 ? 1u : 0u;
    if (count < 2) return;
    const u32 less_two = static_cast<u32>(count) - 2u;
    const s32 parent = static_cast<s32>(sra(less_two + (less_two >> 31), 1));
    for (s32 index = parent; index >= 0; --index) {
        const u32 element = first + static_cast<u32>(index) * 8u;
        put(rt, frame.at(0u), word(rt, element));
        put(rt, frame.at(4u), word(rt, element + 4u));
        call_args(rt, ctx, sift, {first, static_cast<u32>(index), static_cast<u32>(count), frame.at(0u)});
    }
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088334D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088DA880u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08833E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088D9EF8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088AB254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    new_with_static(rt, ctx, 0x2E0u, 0x088AB1F0u, 0x08D75F38u, 0x08CC2EA8u);
}
void func_08B929A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    make_heap_8(rt, ctx, 0x08B92850u);
}
void func_08833FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088D698Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08834068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088D699Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08841BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088418B4u); G[4] = 0x08C9C584u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0888EB9Cu); G[4] = G[2]; call_here(rt, ctx, 0x0888FC14u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AC42FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    new_with_static(rt, ctx, 0xDACu, 0x08AC41F4u, 0x08D77198u, 0x08CD6660u);
}
void func_08ACC220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    new_with_static(rt, ctx, 0xD4u, 0x08ACB73Cu, 0x08D77240u, 0x08CD6A70u);
}
void func_08ACCAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    new_with_static(rt, ctx, 0x35Cu, 0x08ACC2BCu, 0x08D77250u, 0x08CD6AF8u);
}
void func_088578B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088547C4u); G[4] = 0x08C9E188u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[2]; call_here(rt, ctx, 0x088A6608u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08932598u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C3E840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    make_heap_8(rt, ctx, 0x08C3E6F0u);
}
// Index of the first entry (the table's count is at +8) whose first lookup (0x0893A104) is not -1 and whose second (0x0893A0D0)
// equals a1; -1 when none.
void func_08805320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool to_a = false; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; if (G[4] == G[2]) { G[16] = 0x08CE0000u; G[2] = 0xFFFFu; G[4] = 0x08CE27F4u; if (G[5] != G[2]) { G[16] = s0; return; } call_here(rt, ctx, 0x08AD09ACu); G[5] = 0x08CE27F4u; G[4] = 0x08CE27F0u; G[6] = 0x08CDC5ACu; call_here(rt, ctx, 0x08AC2B20u); G[4] = 0x08CE27E8u; G[5] = 0x08805238u; G[6] = 0u; G[16] = s0; to_a = true; } else { G[16] = s0; const bool nz = G[4] != 0u; G[4] = 0x08CE0000u; if (nz) return; G[2] = 0xFFFFu; G[4] = 0x08CE27F4u; if (G[5] != G[2]) return; } } if (to_a) jump(ctx, 0x08AC2B64u); else jump(ctx, 0x08AD09ECu); }
void func_0880B678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool to_a = false; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[3] = G[5]; if (G[4] == G[2]) { G[4] = 0x08CE2998u; G[5] = 0x08C98420u; G[2] = 0xFFFFu; G[6] = 0u; if (G[3] != G[2]) return; call_here(rt, ctx, 0x08B88BA0u); G[4] = 0x08CE2980u; G[5] = 0x08C98430u; G[7] = 0x0880B648u; G[8] = 0x0880C578u; G[9] = 0x0880B66Cu; G[6] = 0xF0u; to_a = true; } else { const bool nz = G[4] != 0u; G[4] = 0x08CE0000u; if (nz) return; G[2] = 0xFFFFu; G[4] = 0x08CE2998u; if (G[5] != G[2]) return; } } if (to_a) jump(ctx, 0x08A92114u); else jump(ctx, 0x08B88C48u); }
void func_088209A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 1u; if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] != G[2]) return; G[4] = 0x08CE2AE0u; call_here(rt, ctx, 0x08B28DCCu); G[2] = 0x08CE0000u; G[3] = 0x08CE0000u; put(rt, 0x08CE2ADCu, 0u); put(rt, 0x08CE2AD8u, 0u); return; } G[2] = 0xFFFFu; if (G[4] != 0u) return; if (G[5] != G[2]) return; G[2] = 0x08CE0000u; G[4] = word(rt, 0x08CE2AD8u); G[2] = 0x08CE0000u; if (G[4] != 0u) { call_here(rt, ctx, 0x08A5B930u); G[2] = 0x08CE0000u; } G[4] = word(rt, 0x08CE2ADCu); if (G[4] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = 0x08CE2AE0u; } jump(ctx, 0x08B28DC4u); }
void func_0882ED78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[17] = frame.at(4u); G[18] = G[4]; G[4] = G[17]; if (static_cast<std::int32_t>(G[6]) <= 0) { call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885E4B8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088315AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDECu); G[4] = G[2] + 4u; call_here(rt, ctx, 0x088D5D98u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088345D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[4] = G[2]; call_here(rt, ctx, 0x088DA460u); ctx.fpr[12] = ctx.fpr[0]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08856E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x30u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[18]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); call_here(rt, ctx, 0x088547C4u); G[4] = 0x08C9E188u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[2]; call_here(rt, ctx, 0x088A5AA0u); ctx.fpr[20] = ctx.fpr[0]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); ctx.fpr[12] = ctx.fpr[20]; call_here(rt, ctx, 0x08A87EECu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0885DB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x50u; G[17] = G[4]; G[4] = G[16]; G[18] = G[6]; call_here(rt, ctx, 0x088A6C18u); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, 0x088A5938u); G[5] = G[16]; G[4] = G[17] + 0x120u; call_here(rt, ctx, 0x088A6C18u); store8(G[17] + 0x15u, 0u); G[3] = G[17]; G[2] = G[17]; G[4] = 0u; G[6] = 0xFFFFFFFFu; G[5] = 2u; do { G[4] += 1u; store8(G[3] + 0x40u, 0u); G[3] += 1u; put(rt, G[2] + 0x44u, G[6]); G[2] += 4u; } while (G[4] != G[5]); G[5] = G[18]; G[4] = G[17] + 0x58u; call_here(rt, ctx, 0x08897470u); G[3] = 1u; store8(G[17] + 0x40u, G[3]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08872548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5] + 0x14u; G[3] = G[5] + 0x20u; G[6] = G[4] + 0x54u; G[16] = G[4]; ctx.fpr[1] = float_at(rt, G[2] + 8u); ctx.fpr[2] = float_at(rt, G[2] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 0x14u); G[2] = G[4] + 0x48u; G[7] = G[5] + 0x2Cu; put_float(rt, G[4] + 0x48u, ctx.fpr[0]); G[8] = G[4] + 0x60u; put_float(rt, G[2] + 8u, ctx.fpr[1]); put_float(rt, G[2] + 4u, ctx.fpr[2]); ctx.fpr[1] = float_at(rt, G[3] + 8u); ctx.fpr[2] = float_at(rt, G[3] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 0x20u); put_float(rt, G[16] + 0x54u, ctx.fpr[0]); put_float(rt, G[6] + 8u, ctx.fpr[1]); put_float(rt, G[6] + 4u, ctx.fpr[2]); ctx.fpr[0] = float_at(rt, G[5] + 0x2Cu); ctx.fpr[1] = float_at(rt, G[7] + 8u); ctx.fpr[2] = float_at(rt, G[7] + 4u); put_float(rt, G[16] + 0x60u, ctx.fpr[0]); put_float(rt, G[8] + 8u, ctx.fpr[1]); put_float(rt, G[8] + 4u, ctx.fpr[2]); ctx.fpr[0] = float_at(rt, G[5] + 4u); put_float(rt, G[16] + 0x6Cu, ctx.fpr[0]); G[2] = word(rt, G[5]); G[3] = word(rt, G[2] + 0x18u); G[4] = G[5]; call_here(rt, ctx, G[3]); put_float(rt, G[16] + 0x70u, ctx.fpr[0]); } G[16] = s0; }
// When the sampler at +0x54 exists and a1 < the count at +0x14: three queries of it at index a1 fill the record a2 (+0x34 a float,
// +0x38..+0x40, +0x44..+0x4C, +0x50); v0 is the last query's answer, else 0.
void func_08883AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[16] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x08B4E648u); G[4] = G[16]; if (G[16] != 0u) { call_here(rt, ctx, 0x0886F8CCu); G[4] = G[16]; call_here(rt, ctx, 0x0886F8E4u); G[4] = G[16]; call_here(rt, ctx, 0x0886F8DCu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x0886F8C4u); G[3] = 3u; G[4] = G[17]; G[5] = G[16]; G[6] = G[16]; if (G[2] == G[3]) target = 0x0888329Cu; else { G[5] = G[17]; target = 0x08882030u; } } } G[16] = s0; G[17] = s1; if (target != 0u) jump(ctx, target); }
void func_088937E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0xB0u); G[16] = G[4] + 4u; G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[3] = sext(load8(G[17] + 1u), 8); G[2] = sext(load8(G[17] + 2u), 8); store8(G[17] + 2u, G[3]); store8(G[17] + 1u, G[2]); call_here(rt, ctx, 0x08B3457Cu); G[3] = G[17] + 0x2Cu; G[4] = word(rt, G[3]); G[2] = (G[4] ^ 1u) != 0u ? 1u : 0u; if (G[2] != 0u) G[2] = 1u; else { put(rt, G[3], G[2]); G[2] = 1u; } if (G[4] == G[2]) { G[2] = 0x1Au; G[4] = G[17]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); put(rt, frame.at(8u), 0u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); store8(frame.at(0x14u), 0u); call_here(rt, ctx, 0x0889349Cu); } } G[16] = s0; G[17] = s1; }
void func_08897190(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x30u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = G[7]; G[16] = G[6]; call_here(rt, ctx, 0x08931B74u); G[4] = frame.at(0u); G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08931D70u); G[4] = frame.at(0u); if (G[2] != 0u) { G[16] = word(rt, G[18]); call_here(rt, ctx, 0x089317A8u); G[4] = frame.at(0u); store8(G[16] + 0xA8u, G[2]); call_here(rt, ctx, 0x089317CCu); G[16] += 0xA8u; store8(G[16] + 3u, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x0893179Cu); store8(G[16] + 6u, G[2]); store8(G[16] + 4u, 0u); store8(G[16] + 5u, 0u); store8(G[16] + 7u, 0u); store8(G[16] + 1u, 0u); store8(G[16] + 2u, 0u); G[4] = frame.at(0u); } call_here(rt, ctx, 0x08931C38u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0889A100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[6] = 0x08C89A88u; G[7] = 0xEu; G[17] = word(rt, G[4] + 0x68u); G[18] = G[4]; if (G[17] != 0u) { G[16] = word(rt, G[17] + 0x68u); G[4] = word(rt, G[16]); G[5] = G[16]; call_here(rt, ctx, 0x08AF93DCu); G[2] = G[18] + 0xB0Cu; put(rt, G[16] + 4u, G[2]); G[4] = G[17]; G[5] = 0u; G[2] = word(rt, G[17] + 0x48u); G[3] = 0u; if (G[2] == 0u) { G[2] = word(rt, G[18] + 0x20u); if (G[2] != 0u) G[3] = word(rt, G[2] + 8u); G[2] = word(rt, G[3] + 0x48u); put(rt, G[17] + 0x48u, G[2]); tail = true; } } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08AF8EB4u); }
void func_088A15B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 record = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 sampler = word(rt, self + 0x54u);
    ctx.gpr[2] = 0u;
    if (sampler == 0u || !(static_cast<s32>(index) < static_cast<s32>(word(rt, self + 0x14u)))) return;
    call_args(rt, ctx, 0x08959344u, {sampler, index, record + 0x44u, record + 0x48u, record + 0x4Cu});
    put_float(rt, record + 0x34u, ctx.fpr[0]);
    call_args(rt, ctx, 0x0895935Cu, {word(rt, self + 0x54u), index, record + 0x50u, 0x80u});
    call_args(rt, ctx, 0x089592BCu, {word(rt, self + 0x54u), index, record + 0x38u, record + 0x3Cu, record + 0x40u});
}
// With the object at +0x14: f12 = the float at +0x28 after 0x08A3A4CC, then 0x08ABCA10 (positive) or 0x08ABC958 (else, f12 = the
// original f12) answers the handle stored at +0x18. f12 / f13 carry the extent and the original scale.
void func_088A5DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 0u; G[18] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088A579Cu); if (G[2] != 0u) G[2] = word(rt, G[18]); else { G[4] = word(rt, G[18]); G[19] = 1u; call_here(rt, ctx, 0x088A5D50u); G[2] = word(rt, G[18]); } G[6] = G[16]; G[5] = 0x1Au; G[4] = word(rt, G[2] + 0x4C0u); call_here(rt, ctx, 0x0892F95Cu); G[16] = G[2]; G[17] = G[3]; if (G[19] != 0u) { G[4] = word(rt, G[18]); call_here(rt, ctx, 0x088A5748u); } G[3] = 0xFFFFFFFFu; G[2] = 0u; if (G[16] != G[3] || G[17] != G[3]) G[2] = 1u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_088B2DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 0xFFFFFFFFu; bool zero = true; if (G[5] != G[2] && static_cast<std::int32_t>(G[5]) >= 0) { G[9] = G[4] + 0x2DCu; G[2] = word(rt, G[9] + 4u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; const bool in = G[2] != 0u; G[2] = ~G[5]; if (in) { G[2] >>= 5; G[8] = G[2] << 2; G[3] = G[5] >> 5; G[2] <<= 5; G[7] = G[2] + 0x20u; G[3] <<= 2; G[2] = word(rt, G[4] + 0x2F0u); if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] += G[3]; G[7] = G[5] & 0x1Fu; } else { G[2] -= G[8]; G[2] -= 4u; } G[3] = word(rt, G[2]); G[2] = 1u; G[2] <<= (G[7] & 31u); G[3] &= G[2]; G[8] = G[5] << 10; G[5] = G[6]; if (G[3] != 0u) { G[4] = word(rt, G[9]); G[4] += G[8]; call_here(rt, ctx, 0x088B4F3Cu); zero = false; } } } if (zero) G[2] = 0u; } }
void func_088E5DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[4]); call_here(rt, ctx, 0x088E511Cu); G[4] = G[2]; G[6] = G[16] + 0x4C0u; if (G[2] != 0u) { G[4] = G[16] + 0x4C8u; G[3] = 1u; G[5] = word(rt, G[4]); G[2] = G[3]; if (G[5] == 0u) { put(rt, G[4], G[2]); G[2] = 1u; } G[2] = 0u; if (G[5] == 0u) { G[3] = G[6] + 8u; G[2] = word(rt, G[3]); G[2] = (G[2] ^ 1u) != 0u ? 1u : 0u; if (G[2] == 0u) put(rt, G[3], G[2]); G[2] = 0u; } } else { G[3] = word(rt, G[16] + 0x4C8u); G[2] = 4u; if (G[3] != G[2]) G[2] = 0u; else { put(rt, G[16] + 0x4C8u, G[4]); G[2] = word(rt, G[6] + 0xCu); } } } G[16] = s0; }
void func_088EC28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4] << 2; G[20] = G[7] + 0xECu; G[4] = G[20]; G[17] = G[7]; G[16] += G[17]; G[19] = G[5]; G[18] = G[6]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[19]; G[16] = word(rt, G[16] + 0x5A8u); G[5] = 0u; G[6] = G[18]; if (G[16] != 0u) { call_here(rt, ctx, 0x08B36018u); G[3] = G[18] - 1u; G[2] = G[18] < 0x11u ? 1u : 0u; G[6] = 0x10u; if (G[2] != 0u) G[6] = G[3]; G[4] = G[19]; G[5] = G[16] + 0xBu; call_here(rt, ctx, 0x08B7C830u); } G[4] = G[20]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; jump(ctx, 0x08B3457Cu); }
void func_08907874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[4]; G[4] = word(rt, G[4]); G[3] = 0xFFFFFFFFu; G[5] = G[4]; if (static_cast<std::int32_t>(G[4]) > 0) { G[6] = 0u; G[2] = G[5] + G[6]; for (;;) { G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[3] = G[2] << 3; G[3] += G[7]; ctx.fpr[0] = float_at(rt, G[3] + 4u); if (ctx.fpr[12] < ctx.fpr[0]) G[5] = G[2]; else G[6] = G[2] + 1u; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; const bool more = G[2] != 0u; G[2] = G[5] + G[6]; if (!more) break; } G[3] = G[6] - 1u; } G[2] = G[4] - 1u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] -= 2u; if (G[2] == 0u) G[3] = G[4]; G[3] <<= 3; G[2] = G[3] + 8u; G[2] = G[7] + G[2]; G[3] = G[7] + G[3]; ctx.fpr[2] = float_at(rt, G[3] + 4u); ctx.fpr[4] = float_at(rt, G[3] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 8u); ctx.fpr[3] = ctx.fpr[12] - ctx.fpr[2]; ctx.fpr[1] = float_at(rt, G[2] + 4u); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[4]; ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[4] + ctx.fpr[0]; }
void func_0891A8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x30u); ctx.fpr[13] = 0.0f; G[7] = 6u; G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = frame.at(0u); G[4] = word(rt, G[6] + 0x61Cu); ctx.fpr[12] = float_at(rt, G[17] + 0xCu); G[6] = frame.at(0xCu); call_here(rt, ctx, 0x088D390Cu); ctx.fpr[4] = float_at(rt, frame.at(4u)); ctx.fpr[1] = float_at(rt, frame.at(8u)); ctx.fpr[3] = float_at(rt, frame.at(0xCu)); ctx.fpr[5] = float_at(rt, frame.at(0x10u)); ctx.fpr[2] = float_at(rt, frame.at(0x14u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); G[2] = G[16] + 0x40u; G[3] = G[16] + 0x50u; put_float(rt, G[16] + 0x40u, ctx.fpr[0]); put_float(rt, G[2] + 8u, ctx.fpr[1]); put_float(rt, G[2] + 4u, ctx.fpr[4]); put_float(rt, G[16] + 0x50u, ctx.fpr[3]); put_float(rt, G[3] + 8u, ctx.fpr[2]); put_float(rt, G[3] + 4u, ctx.fpr[5]); G[4] = word(rt, G[18] + 0x61Cu); ctx.fpr[12] = float_at(rt, G[17] + 0xCu); call_here(rt, ctx, 0x088D2BC8u); put_float(rt, G[17] + 0xCu, ctx.fpr[0]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0892B558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[4]; G[4] = frame.at(0u); G[16] = frame.at(0x16u); for (std::uint32_t i = 0x16u; i <= 0x1Fu; ++i) store8(frame.at(i), 0u); store8(frame.at(3u), 0u); call_here(rt, ctx, 0x08948B54u); G[2] = G[17] + 0x16u; G[3] = (load8(G[16]) | (load8(G[16] + 1u) << 8) | (load8(G[16] + 2u) << 16) | (load8(G[16] + 3u) << 24)); G[4] = (load8((G[16] + 4u)) | (load8((G[16] + 4u) + 1u) << 8) | (load8((G[16] + 4u) + 2u) << 16) | (load8((G[16] + 4u) + 3u) << 24)); G[7] = load8(frame.at(0x1Eu)); G[6] = load8(frame.at(0x1Fu)); G[5] = load8(frame.at(3u)); store8(G[2], G[3]); store8(G[2] + 1u, G[3] >> 8); store8(G[2] + 2u, G[3] >> 16); store8(G[2] + 3u, G[3] >> 24); store8((G[2] + 4u), G[4]); store8((G[2] + 4u) + 1u, G[4] >> 8); store8((G[2] + 4u) + 2u, G[4] >> 16); store8((G[2] + 4u) + 3u, G[4] >> 24); store8(G[17] + 3u, G[5]); store8(G[2] + 9u, G[6]); store8(G[2] + 8u, G[7]); } G[16] = s0; G[17] = s1; }
void func_08930C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x089364ACu); G[16] = 0xFFFFFFFFu; G[4] = G[2]; bool go = true; if (G[2] == G[16] && G[3] == G[16]) { call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[5] = G[17]; G[6] = 0x26u; call_here(rt, ctx, 0x089364ACu); G[4] = G[2]; if (G[2] == G[16] && G[3] == G[16]) { G[4] = 0u; go = false; } } if (go) { G[4] = G[2]; G[5] = G[3]; G[6] = G[18]; call_here(rt, ctx, 0x08930B60u); G[4] = G[2]; } G[2] = G[4]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08936D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC67B8u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = G[4] + 0x1F4u; call_here(rt, ctx, 0x08938EACu); G[4] = G[16] + 0x2D0u; call_here(rt, ctx, 0x08938EACu); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 8u, G[2]); G[3] = 0u; G[2] = G[16]; put(rt, G[16] + 4u, 0u); G[4] = 0x2Cu; do { G[3] += 1u; put(rt, G[2] + 0x8Cu, 0u); put(rt, G[2] + 0x13Cu, 0u); G[2] += 4u; } while (G[3] != G[4]); G[6] = 0x08CA0000u; put(rt, G[16] + 0x1ECu, 0u); G[3] = 0x08CA21E4u; G[2] = G[16] + 0xCu; put(rt, G[16] + 0x1F0u, 0u); G[4] = (load8(G[3]) | (load8(G[3] + 1u) << 8) | (load8(G[3] + 2u) << 16) | (load8(G[3] + 3u) << 24)); G[5] = (load8((G[3] + 4u)) | (load8((G[3] + 4u) + 1u) << 8) | (load8((G[3] + 4u) + 2u) << 16) | (load8((G[3] + 4u) + 3u) << 24)); store8(G[2], G[4]); store8(G[2] + 1u, G[4] >> 8); store8(G[2] + 2u, G[4] >> 16); store8(G[2] + 3u, G[4] >> 24); store8((G[2] + 4u), G[5]); store8((G[2] + 4u) + 1u, G[5] >> 8); store8((G[2] + 4u) + 2u, G[5] >> 16); store8((G[2] + 4u) + 3u, G[5] >> 24); } G[16] = s0; }
void func_0893A174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, table + 8u));
    s32 found = -1;
    for (s32 index = 0; index < count; ++index) {
        const u32 first = word(rt, call_args(rt, ctx, 0x0893A104u, {table, static_cast<u32>(index)}));
        if (first == 0xFFFFFFFFu) continue;
        const u32 second = word(rt, call_args(rt, ctx, 0x0893A0D0u, {table, static_cast<u32>(index)}));
        if (second == key) {
            found = index;
            break;
        }
    }
    ctx.gpr[2] = static_cast<u32>(found);
}
void func_0893A210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x20u); G[20] = 0xFFFFFFFFu; G[18] = word(rt, G[4] + 8u); G[19] = G[4]; if (static_cast<std::int32_t>(G[18]) > 0) { G[2] = 0x7FFF0000u; G[17] = 0x7FFFFFFFu; G[16] = 0u; G[21] = 0xFFFFFFFFu; G[5] = G[16]; do { G[4] = G[19]; call_here(rt, ctx, 0x0893A104u); G[3] = word(rt, G[2]); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; if (G[2] != 0u) { G[17] = G[3]; G[20] = G[16]; } const bool stop = G[17] == G[21]; G[16] += 1u; if (stop) break; G[5] = G[16]; } while (G[16] != G[18]); } G[2] = G[20]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
// 1 when the two ranges (begin at +0, end at +4) hold as many 0x20-byte elements and 0x08A0B180 answers 0 for each pair, else 0.
void func_0894161C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] &= 0xFFu; G[11] = G[5]; G[12] = G[6]; if (static_cast<std::int32_t>(G[7]) > 0) { G[6] = G[5]; G[10] = 0u; G[5] = G[12]; G[1] = 0x3E8E38E4u; ctx.fpr[4] = std::bit_cast<float>(G[1]); G[1] = 0x3E4CCCCDu; ctx.fpr[3] = std::bit_cast<float>(G[1]); G[1] = 0x3C8EFA35u; ctx.fpr[2] = std::bit_cast<float>(G[1]); do { G[2] = G[8] + G[10]; G[2] = load8(G[2]); G[3] = G[9] + G[10]; G[10] += 1u; ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[4]; put_float(rt, G[6], ctx.fpr[0]); G[6] += 4u; G[3] = load8(G[3]); ctx.fpr[1] = std::bit_cast<float>(G[3]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[3]; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[2]; put_float(rt, G[5], ctx.fpr[0]); G[5] += 4u; } while (G[7] != G[10]); } G[5] = G[7]; G[6] = G[11]; G[7] = G[12]; G[8] = 0u; G[9] = 0u; jump(ctx, 0x08946734u); }
void func_0894A520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[20] = 4u; G[18] = G[4]; G[17] = 0u; put_float(rt, frame.at(0x20u), ctx.fpr[20]); ctx.fpr[20] = 0.0f; G[19] = word(rt, G[4] + 0xCu); do { G[5] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x08955300u); G[16] = load8(G[2] + 0xE9u); G[5] = G[17]; G[4] = G[19]; G[6] = G[16]; call_here(rt, ctx, 0x0894268Cu); G[16] <<= 2; G[16] += G[2]; ctx.fpr[0] = float_at(rt, G[16] + 8u); ctx.fpr[1] = float_at(rt, G[2]); G[17] += 1u; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[1]; ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; } while (G[17] != G[20]); ctx.fpr[0] = ctx.fpr[20]; ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_0894E678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDE508u; G[4] = load8(G[2] + 0x13Au); G[6] = 0x08CF0000u; G[5] = load8(G[2] + 0x139u); G[2] = 0x08CF2F0Cu; put(rt, G[2] + 4u, G[4]); G[4] = 0x08CF2F14u; G[2] = 0x08CE0000u; G[3] = 0x08CDE643u; put(rt, 0x08CF2F0Cu, G[5]); G[7] = 0x08CDE645u; G[1] = 0x3AE4C388u; ctx.fpr[6] = std::bit_cast<float>(G[1]); G[1] = 0x41200000u; ctx.fpr[5] = std::bit_cast<float>(G[1]); do { G[2] = load8(G[3]); G[8] = load8(G[3] + 2u); ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[2] = std::bit_cast<float>(G[8]); G[8] = load8(G[3] + 4u); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[2]))); ctx.fpr[3] = std::bit_cast<float>(G[8]); G[3] += 1u; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[6]; ctx.fpr[4] = static_cast<float>(static_cast<std::int32_t>(G[8])); ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[5]; if (G[2] == 0u) G[5] = 0u; put_float(rt, G[4], ctx.fpr[0]); put_float(rt, G[4] + 0x10u, ctx.fpr[4]); put_float(rt, G[4] + 8u, ctx.fpr[1]); G[4] += 4u; } while (G[3] != G[7]); put(rt, 0x08CF2F0Cu, G[5]); }
void func_089A6C20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[20] = 0x08CF0000u; G[19] = G[4]; G[4] = 0x08CF4994u; G[18] = frame.at(4u); G[17] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[16] = G[19] + 0x14u; G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x089A32B4u); G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[19] + 0x14u); G[4] = 0x08CF4994u; if (G[16] != 0u) { G[2] = 0x64u; put(rt, G[16] + 0x134u, G[2]); } call_here(rt, ctx, 0x08B3457Cu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_089AD9FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(8u); G[18] = frame.at(4u); G[17] = G[4]; G[4] = G[19]; if (G[6] == 0u) { call_here(rt, ctx, 0x089AB370u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA50C4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0xB8u); G[4] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[17]; if (G[17] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_089AF974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[5] = G[4]; G[16] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x089AA658u); G[4] = G[16]; call_here(rt, ctx, 0x089AF91Cu); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA50ACu; G[17] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x089A717Cu); G[16] = word(rt, frame.at(0u)); if (G[2] != 0u) { G[4] = 0x08CA50ACu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x089A7120u); G[16] = 1u; if (G[2] == 0u) G[16] = 0u; } else G[16] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16] + G[17]; } G[16] = s0; G[17] = s1; }
void func_089B5B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x124u); G[17] = 0xFFFFFFFFu; if (G[4] != 0u) { call_here(rt, ctx, 0x08A008BCu); G[3] = word(rt, G[16] + 0x124u); G[16] = word(rt, G[3] + 0xF8u); G[17] = G[2] - 1u; bool hit = false; while (G[16] != 0u) { G[2] = word(rt, G[16] + 0xE0u) & 0x1000u; G[3] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; if (G[2] == 0u) { G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x089660E8u); const bool z = G[2] == 0u; G[2] = G[17]; if (z) { hit = true; break; } } G[17] -= 1u; } G[4] = G[16]; call_here(rt, ctx, 0x08965BCCu); G[16] = G[2]; } if (!hit) G[17] = 0xFFFFFFFFu; } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
// Replaces `count` (a3) elements at `position` (a2) of the string whose data pointer is *a0 (size word at data - 0xC) when the range
// fits (position + count <= size, unsigned); 0x08B8D5D8 gets (data + position, data + size, a1, count + a1, 0x08B8D5C4) and the answer
// is its result as an offset from data, -1 when it does not fit or when it stopped at the end with a non-empty count.
void func_089C4810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[4] = 0x8002u; bool chain = false; if (G[5] == G[2]) { call_here(rt, ctx, 0x08B58874u); G[4] = 0x400u; G[5] = 0x302u; G[6] = 1u; chain = true; } else { G[2] = 2u; if (G[5] == G[2]) { call_here(rt, ctx, 0x08B54FB8u); G[4] = 0x6Fu; target = 0x08B54FB8u; } else if (G[5] == 0u) { call_here(rt, ctx, 0x08B58874u); G[4] = 0x400u; G[5] = 0x302u; G[6] = 0x303u; chain = true; } } if (chain) { call_here(rt, ctx, 0x08B584D0u); G[4] = 6u; G[5] = 0u; G[6] = 0xFFu; call_here(rt, ctx, 0x08B58744u); G[4] = 0x6Fu; target = 0x08B58874u; } } if (target != 0u) jump(ctx, target); }
void func_089C843C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0xCu;
    if (word(rt, self + 0x2Cu) != 0u) {
        const u32 block = call_args(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
        ctx.gpr[5] = self + 0x30u;
        if (block + 8u != 0u) put(rt, block + 8u, word(rt, self + 0x2Cu));
        call_args(rt, ctx, 0x08B5C078u, {block, self + 0x30u});
    }
    const u32 node = call_args(rt, ctx, 0x08B62344u, {0x30u});
    const u32 first_head = node + 4u;
    const u32 second = node + 0x18u;
    const u32 second_head = node + 0x1Cu;
    put(rt, node + 0x14u, 0u);
    put(rt, node + 0x10u, first_head);
    put(rt, node + 4u, 0u);
    put(rt, node + 8u, 0u);
    put(rt, node + 0xCu, first_head);
    put(rt, second + 0x10u, second_head);
    put(rt, second + 4u, 0u);
    put(rt, second + 0x14u, 0u);
    put(rt, second + 8u, 0u);
    put(rt, second + 0xCu, second_head);
    put(rt, self + 0x2Cu, node);
}
// Initialises the object: vtable 0x08CD6FC8, 0xC8 at +0x2C, 1.0 at +0x34 and +0x38, zeroes elsewhere up to +0x44, then its two
// 16-byte sub-objects at +4 and +0x14 (0x08AD7CA0 each, their first word cleared afterwards).
void func_089C95D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBB888u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = G[17]; G[5] = word(rt, G[17] + 8u); call_here(rt, ctx, 0x08C012FCu); G[2] = G[16] + 0x18u; put(rt, G[17] + 0x10u, G[2]); G[4] = G[16]; put(rt, G[17] + 0xCu, G[2]); put(rt, G[17] + 8u, 0u); put(rt, G[17] + 0x14u, 0u); call_here(rt, ctx, 0x089C946Cu); G[4] = G[16] + 0x48u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C01C34u); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x38u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08C00980u); G[5] = word(rt, G[17] + 8u); G[4] = G[17]; call_here(rt, ctx, 0x08C012FCu); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A780B0u); }
void func_089D60F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(8u); call_here(rt, ctx, 0x089D5DF4u); G[4] = 0x08CA6E2Cu; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x24u); G[5] = frame.at(4u); G[4] = G[17]; if (G[3] != 0u) { G[2] = word(rt, G[3] + 4u); if (G[2] == 0u) G[3] = 0u; } put(rt, frame.at(4u), G[3]); call_here(rt, ctx, 0x08A76AACu); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A0B1A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[3] = word(rt, G[4] + 4u); G[2] = word(rt, G[5] + 4u); G[4] = word(rt, G[4]); G[5] = word(rt, G[5]); G[3] -= G[4]; G[2] -= G[5]; G[18] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 5); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 5); const bool same = G[18] == G[2]; G[2] = 0u; if (same) { G[2] = 1u; if (static_cast<std::int32_t>(G[18]) > 0) { G[17] = G[4]; G[16] = G[5]; G[19] = 0u; for (;;) { G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A0B180u); G[19] += 1u; if (G[2] != 0u) { G[2] = 0u; break; } G[17] += 0x20u; G[16] += 0x20u; if (G[18] == G[19]) { G[2] = 1u; break; } } } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
// Counts down from (0x08A008BC of the list owner at +0x124) - 1 over the owner's chain (first node at owner + 0xF8, next by
// 0x08965BCC), skipping nodes whose flags (+0xE0) have 0x1000; the answer is the position of the first remaining node for which
// 0x089660E8 answers 0 at a position <= a1 (nodes at a position > a1 only count down); -1 when none or no owner.
void func_08A14650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = 1u; G[17] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08CA9A08u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2] != 0u ? 1u : 0u; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x08A14610u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA9A6Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08A14374u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A20360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[4]; G[19] = G[6]; G[17] = G[5]; if (static_cast<std::int32_t>(G[5]) > 0) { call_here(rt, ctx, 0x08AC32A4u); G[16] = word(rt, G[19]); G[4] = 0x08CAA6B4u; G[18] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[12] = ctx.fpr[0]; G[4] = G[18]; call_here(rt, ctx, 0x08AC3384u); } G[4] = G[20]; G[5] = G[17]; G[6] = G[19]; G[7] = 5u; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; jump(ctx, 0x08A202F0u); }
void func_08A238A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x5Cu); G[4] = G[2]; if (G[2] != 0u) { ctx.fpr[12] = float_at(rt, G[16] + 0x74u); call_here(rt, ctx, 0x08ABB72Cu); G[4] = word(rt, G[16] + 0x5Cu); ctx.fpr[13] = float_at(rt, G[16] + 0x7Cu); ctx.fpr[12] = float_at(rt, G[16] + 0x78u); call_here(rt, ctx, 0x08ABB6D4u); ctx.fpr[0] = float_at(rt, G[16] + 0x70u); G[4] = word(rt, G[16] + 0x5Cu); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); call_here(rt, ctx, 0x08ABB6A4u); } G[2] = word(rt, G[16] + 0x60u); G[4] = G[2]; if (G[2] == 0u) { G[16] = s0; return; } ctx.fpr[12] = float_at(rt, G[16] + 0x80u); call_here(rt, ctx, 0x08ABB72Cu); G[4] = word(rt, G[16] + 0x60u); ctx.fpr[12] = float_at(rt, G[16] + 0x84u); ctx.fpr[13] = float_at(rt, G[16] + 0x88u); call_here(rt, ctx, 0x08ABB6D4u); ctx.fpr[0] = float_at(rt, G[16] + 0x70u); G[4] = word(rt, G[16] + 0x60u); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); } G[16] = s0; jump(ctx, 0x08ABB6A4u); }
void func_08A30DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = 0x08D69170u; G[5] = frame.at(0u); G[18] = frame.at(4u); G[17] = frame.at(8u); call_here(rt, ctx, 0x08B31950u); G[4] = G[17]; G[5] = 0u; if (G[2] != 0u) { G[5] = load8(frame.at(0u)); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = G[18]; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A33BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = frame.at(4u); G[18] = G[4]; G[4] = G[19]; call_here(rt, ctx, 0x08A33604u); G[4] = 0x08CACCECu; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B1B668u); G[16] = G[2]; G[17] = G[3]; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08A83160u); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A3A520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = word(rt, self + 0x14u);
    ctx.gpr[2] = owner;
    if (owner == 0u) return;
    ctx.fpr[12] = float_at(rt, self + 0x28u);
    call_here(rt, ctx, 0x08A3A4CCu);
    const float extent = float_at(rt, self + 0x28u);
    ctx.fpr[13] = scale;
    ctx.fpr[12] = extent;
    ctx.gpr[6] = flag;
    if (0.0f < extent) {
        call_args(rt, ctx, 0x08ABCA10u, {word(rt, self + 0x14u), mode, flag});
    } else {
        ctx.fpr[12] = scale;
        call_args(rt, ctx, 0x08ABC958u, {word(rt, self + 0x14u), mode, flag});
    }
    put(rt, self + 0x18u, ctx.gpr[2]);
}
void func_08A3AC40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); ctx.fpr[20] = 0.0f; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08CAD518u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[20] = ctx.fpr[0]; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x08A3A9D0u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAD544u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[16]; call_here(rt, ctx, 0x08A3A4CCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; }
void func_08A50038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x80460007u; G[4] = G[6]; G[19] = G[9]; G[18] = G[7]; G[17] = G[5]; put(rt, G[9], G[2]); G[2] = 0u; if (G[5] != 0u) { { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[7])) * static_cast<std::int32_t>(G[8]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[5] = word(rt, G[5] + 4u); G[2] = word(rt, G[17] + 8u); G[6] = word(rt, G[17]); G[16] = ctx.lo; G[2] -= G[5]; G[3] = G[2] < G[16] ? 1u : 0u; if (G[3] != 0u) G[16] = G[2]; G[5] = G[6] + G[5]; G[6] = G[16]; call_here(rt, ctx, 0x08B79A88u); if (G[18] == 0u) { ctx.lo = 0xFFFFFFFFu; ctx.hi = G[16]; } else { ctx.lo = G[16] / G[18]; ctx.hi = G[16] % G[18]; } G[3] = word(rt, G[17] + 4u); G[16] += G[3]; put(rt, G[17] + 4u, G[16]); put(rt, G[19], 0u); G[2] = ctx.lo; } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A55420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[2] = 0u; G[17] = G[4]; G[3] = word(rt, G[5] + 0x38u); G[16] = G[5]; if (G[3] != 0u) { G[2] = word(rt, G[6] + 0x1Cu); G[5] = G[2]; if (G[2] == 0u) { G[18] = word(rt, G[6] + 0x18u); G[4] = 0x08CAF634u; if (G[18] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x38u); } G[5] = word(rt, G[18] + 0xCu); } G[2] = G[5]; } G[5] = G[2]; G[6] = G[3]; G[7] = 0x10u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A950D0u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A57894(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[2] = 2u; G[4] = frame.at(0u); G[17] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A57854u); G[18] = word(rt, frame.at(0u)); G[4] = 0x08CAF7ECu; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[17]); G[4] = 0x08CAF778u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[18]; G[6] = G[17] + 4u; call_here(rt, ctx, 0x08A5670Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A738A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[4] = sext(load16(G[4] + 0x14u), 16); G[16] = word(rt, G[7]); ctx.fpr[0] = std::bit_cast<float>(G[4]); G[4] = 0x08CB0BA8u; ctx.fpr[20] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x38u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = std::bit_cast<float>(G[2]); ctx.fpr[12] = ctx.fpr[20]; ctx.fpr[13] = static_cast<float>(static_cast<std::int32_t>(G[2])); call_here(rt, ctx, 0x08B67720u); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; }
void func_08A83824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[16] = word(rt, G[7]); G[17] = G[5]; G[18] = word(rt, G[4] + 0x18u); G[19] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB168Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[3] ^ G[19]; G[4] = frame.at(0u); G[5] = 0u; if (G[2] == G[18]) G[5] = G[6] < 1u ? 1u : 0u; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A9661C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[4] = sext(load8(G[4] + 0x14u), 8); G[16] = word(rt, G[7]); ctx.fpr[0] = std::bit_cast<float>(G[4]); G[4] = 0x08CB21E8u; ctx.fpr[20] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x34u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = std::bit_cast<float>(G[2]); ctx.fpr[12] = ctx.fpr[20]; ctx.fpr[13] = static_cast<float>(static_cast<std::int32_t>(G[2])); call_here(rt, ctx, 0x08B67720u); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; }
void func_08AB0788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[17] = G[5]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[4] = load16(G[4] + 0x14u); G[16] = word(rt, G[7]); ctx.fpr[0] = std::bit_cast<float>(G[4]); G[4] = 0x08CB3A78u; ctx.fpr[20] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(ctx.fpr[0]))); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x48u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = std::bit_cast<float>(G[2]); ctx.fpr[12] = ctx.fpr[20]; ctx.fpr[13] = static_cast<float>(static_cast<std::int32_t>(G[2])); call_here(rt, ctx, 0x08B67720u); G[5] = trunc_w_s(ctx.fpr[0]); ctx.fpr[0] = std::bit_cast<float>(G[5]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; }
void func_08A9A994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[2] = 1u; G[19] = frame.at(4u); G[18] = G[4]; G[4] = G[19]; G[17] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A98674u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CB23CCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = frame.at(0u); G[5] = G[16]; G[6] = G[17]; call_here(rt, ctx, 0x08A9A948u); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AA49B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[16] = frame.at(4u); G[18] = frame.at(8u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AA45F8u); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[17]; if (G[17] != G[18]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AC87E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D00000u; G[17] = 0x08D00000u; G[16] = 0x08D00000u; G[4] = word(rt, 0x08CFFF9Cu); store8(0x08CFFF99u, 0u); call_here(rt, ctx, 0x08B1B99Cu); G[2] = load8(0x08CFFF98u); put(rt, 0x08CFFF9Cu, 0u); if (G[2] == 0u) { G[4] = 0xFFFFFFFFu; do { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B2EFDCu); G[4] = load8(0x08CFFF98u); if (G[4] != 0u) break; G[4] = 0xFFFFFFFFu; } while (true); } call_here(rt, ctx, 0x08AEB954u); G[4] = G[2]; call_here(rt, ctx, 0x08AD9784u); call_here(rt, ctx, 0x08AD9404u); G[4] = 0x08CFFF30u; call_here(rt, ctx, 0x08B2EE2Cu); G[4] = 0x08CFFF44u; call_here(rt, ctx, 0x08B2EE2Cu); G[4] = 0x08CFFF58u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B2EE2Cu); }
void func_08AD7CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[0] = 1.0f;
    put(rt, self, 0x08CD6FC8u);
    put(rt, self + 0x2Cu, 0xC8u);
    put_float(rt, self + 0x38u, 1.0f);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x30u, 0u);
    put_float(rt, self + 0x34u, 1.0f);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    store8(self + 0x44u, 0u);
    for (u32 entry = 0; entry < 2u; ++entry) {
        const u32 sub_object = self + 4u + entry * 0x10u;
        call_args(rt, ctx, 0x08AD7CA0u, {sub_object});
        put(rt, sub_object, 0u);
    }
}
// Constructs a 0x14-byte node (0x08B9FC30, 0x08B9FB40, 0x08B9F960 on its +0xC part, flag byte 1 at +0xC) and a handle at a0
// (cleared, flag byte at +8 cleared) that 0x08B9D15C attaches the node to; the flag byte is set last. Answers the handle.
void func_08AD9E74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D40000u; G[3] = 0x08D405FCu; G[9] = G[4]; G[8] = G[5]; if (static_cast<std::int32_t>(G[4]) >= 0) { G[2] = word(rt, G[3] + 0x28u) - 1u; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; const bool out = G[2] != 0u; G[2] = 0x58u; if (!out) { { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[4])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[3] = word(rt, G[3] + 0x24u); G[2] = ctx.lo; G[3] += G[2]; G[4] = word(rt, G[3] + 0xCu) & 1u; G[2] = G[3]; if (G[4] != 0u) return; } } G[2] = 0x08CE0000u; if (G[8] != 0u) { G[3] = word(rt, 0x08CDFB3Cu) & 2u; G[5] = 0x08C90B68u; G[7] = 0x08C90B74u; G[6] = 0x6Cu; if (G[3] != 0u) { G[2] = 0x08CE0000u; G[4] = word(rt, 0x08CDFB44u); call_here(rt, ctx, 0x08AD83B8u); } } G[2] = 0u; } }
void func_08AEB320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = 0x08D41AD0u;
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, table + 0x47Cu));
    ctx.gpr[2] = table;
    if (count > 0) {
        s32 index = 0;
        u32 offset = 0u;
        do {
            const u32 entry = word(rt, table + 0x478u) + offset;
            while (load8(entry + 0x38u) != 0u) call_args(rt, ctx, 0x08C7A988u, {0x2710u});
            ++index;
            offset += 0x60B0u;
        } while (index < static_cast<s32>(word(rt, table + 0x47Cu)));
        ctx.gpr[2] = 0u;
    }
}
// a0 = a vector perpendicular to the vector at a1: (-y, x, 0) when x != 0, else (0, -z, y) when y != 0, else (z, 0, -x) when
// z != 0, else a copy of the vector at a2. (Each read follows the previous store, as the original does.)
void func_08AED010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 31); G[17] = G[5] << 2; G[5] = 1u; G[16] = G[4]; G[4] = word(rt, G[4] + 0x2094u); G[8] = word(rt, G[16] + 0x2098u); G[6] = word(rt, G[16] + 0x60A8u); G[7] = word(rt, G[16] + 0x60ACu); G[4] += G[17]; if (G[8] != 0u) { if (G[4] == 0x80000000u && G[8] == 0xFFFFFFFFu) { ctx.lo = 0x80000000u; ctx.hi = 0u; } else { ctx.lo = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) / static_cast<std::int32_t>(G[8])); ctx.hi = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) % static_cast<std::int32_t>(G[8])); } } G[6] += G[2]; G[4] = G[6] < G[2] ? 1u : 0u; G[7] += G[3]; G[7] += G[4]; put(rt, G[16] + 0x60A8u, G[6]); put(rt, G[16] + 0x60ACu, G[7]); if (G[8] == 0u) { rt.unsupported(0x08AED068u, 0x000001CDu, "special? not lowered yet"); return; } G[6] = 0u; G[4] = word(rt, G[16] + 0x1Cu); G[9] = ctx.hi; put(rt, G[16] + 0x2094u, G[9]); call_here(rt, ctx, 0x08C7AA20u); G[3] = word(rt, G[16] + 0x209Cu); G[4] = word(rt, G[16] + 0x1Cu); G[5] = 1u; G[3] -= G[17]; put(rt, G[16] + 0x209Cu, G[3]); call_here(rt, ctx, 0x08C7AA08u); } G[16] = s0; G[17] = s1; }
void func_08AF7504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x40u); G[6] = frame.at(0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 0x2Cu); G[4] = G[2]; if (G[2] != 0u) { G[2] = load16(G[16] + 0x26u); G[3] = 0x08AF7420u; G[5] = G[2] << 4; G[2] <<= 2; G[5] -= G[2]; G[5] = G[4] + G[5]; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08C3FFD4u); put(rt, frame.at(0x18u), G[2]); put(rt, frame.at(0x1Cu), G[3]); put(rt, frame.at(0x20u), G[2]); put(rt, frame.at(0x24u), G[3]); } G[5] = load16(G[16] + 0x16u); G[4] = word(rt, G[16] + 0x1Cu); G[5] <<= 5; G[5] = G[4] + G[5]; G[6] = frame.at(0u); G[2] = 0x08AF7320u; put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08C3FF20u); G[4] = 0x54585333u; put(rt, G[16], G[4]); } G[16] = s0; }
void func_08AF89B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x10u); G[16] = G[4]; if (G[2] == 0u) { G[2] = word(rt, G[4] + 0xCu); G[4] = 0x8892u; G[6] = G[2]; if (G[2] != 0u) { G[2] = load8(G[5] + 0x19u); G[5] = word(rt, G[16] + 8u); { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[5] = ctx.lo; call_here(rt, ctx, 0x08B52814u); put(rt, G[16] + 0x10u, G[2]); } } G[2] = word(rt, G[16] + 0x1Cu); if (G[2] == 0u) { G[2] = word(rt, G[16] + 0x18u); G[7] = 4u; G[4] = 0x8893u; G[6] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[16]); G[5] = word(rt, G[16] + 0x14u); G[3] = 2u; G[2] &= 0x40u; if (G[2] == 0u) G[7] = G[3]; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[7])) * static_cast<std::int32_t>(G[5]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[5] = ctx.lo; call_here(rt, ctx, 0x08B52814u); put(rt, G[16] + 0x1Cu, G[2]); } } G[2] = word(rt, G[16] + 0x2Cu); G[4] = word(rt, G[2] + 0x30u); } G[16] = s0; jump(ctx, 0x08B537B4u); }
void func_08B01154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(load16(G[4] + 4u), 16); { GuestFrame frame(ctx, 0x30u); G[6] = G[5]; if (static_cast<std::int32_t>(G[2]) < 0) return; G[3] = G[2] << 4; G[2] <<= 2; G[4] = word(rt, G[5] + 0x60u); G[3] -= G[2]; G[2] = G[3] << 3; G[2] -= G[3]; G[5] = word(rt, frame.at(0u)); G[4] += G[2]; G[6] += 0x60u; G[2] = G[4]; put(rt, frame.at(0x20u), G[5]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(8u), G[4]); put(rt, frame.at(4u), G[6]); put(rt, frame.at(0x10u), G[5]); put(rt, frame.at(0x14u), G[6]); put(rt, frame.at(0x18u), G[4]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x24u), G[6]); put(rt, frame.at(0x28u), G[4]); put(rt, frame.at(0x2Cu), 0u); while (G[2] != 0u) { G[5] = sext(load16(G[2] + 0x52u), 16); store8(G[2] + 0x51u, 0u); G[3] = G[5] << 2; G[2] = G[5] << 4; G[2] -= G[3]; G[4] = G[2] << 3; G[4] -= G[2]; if (static_cast<std::int32_t>(G[5]) < 0) return; G[2] = word(rt, G[6]); G[2] += G[4]; } } }
void func_08B0B1F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x33627067u; G[3] = __builtin_bswap32(word(rt, G[4])); G[18] = G[4]; if (G[3] == G[2]) { G[2] = word(rt, G[4] + 4u); G[19] = 0u; G[16] = G[4] + 0x20u; G[2] = __builtin_bswap32(G[2]); G[17] = G[5] - G[2]; G[3] = __builtin_bswap32(G[17]); put(rt, G[4] + 4u, G[3]); if (G[17] != 0u) { for (;;) { G[2] = word(rt, G[18] + 0xCu); G[4] = G[16]; G[5] = G[17]; G[2] = __builtin_bswap32(G[2]); G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] += 0x10u; G[19] += 1u; if (G[2] == 0u) break; call_here(rt, ctx, 0x08B0B1D8u); } } } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B13038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] << 2; G[9] = 0x08C92BC0u; G[2] = G[5] << 4; G[2] -= G[3]; G[2] >>= 3; G[3] = G[5] & 1u; G[7] = G[3] << 2; G[6] = G[4]; G[2] = G[4] + G[2]; if (G[5] == 0u) { G[4] = load8(G[4] + 1u); G[5] = load8(G[6]); G[4] += G[9]; G[5] += G[9]; G[2] = load8(G[4]); G[3] = load8(G[5]); G[2] <<= 8; G[3] |= G[2]; G[3] = (G[3] >> 1) & 0x7FFu; G[2] = G[3] << 4; G[3] <<= 2; G[2] -= G[3]; G[2] += 7u; G[2] >>= 3; G[2] = G[6] + G[2]; return; } G[4] = load8(G[2] + 1u); G[5] = load8(G[2]); G[4] += G[9]; G[5] += G[9]; G[3] = load8(G[4]); G[2] = load8(G[5]); G[3] <<= 8; G[2] |= G[3]; G[2] >>= (G[7] & 31u); G[2] &= 0xFFFu; G[2] = G[6] + G[2]; }
void func_08B3209C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[6] = 8u; G[17] = G[4]; G[4] = frame.at(8u); G[16] = G[5] + 0xCu; G[2] = word(rt, G[5]); G[5] += 4u; put(rt, frame.at(8u), 0u); put(rt, frame.at(4u), G[2]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08B7C830u); G[2] = frame.at(0x10u); G[3] = (load8(G[16]) | (load8(G[16] + 1u) << 8) | (load8(G[16] + 2u) << 16) | (load8(G[16] + 3u) << 24)); G[6] = load8(G[16] + 5u); G[5] = load8(G[16] + 4u); put(rt, G[17] + 8u, 0u); G[4] = frame.at(0u); store8(G[2], G[3]); store8(G[2] + 1u, G[3] >> 8); store8(G[2] + 2u, G[3] >> 16); store8(G[2] + 3u, G[3] >> 24); store8(frame.at(0x14u), G[5]); store8(frame.at(0x15u), G[6]); put(rt, frame.at(0x18u), 0u); call_here(rt, ctx, 0x08C7AEE0u); G[2] >>= 31; G[2] ^= 1u; } G[16] = s0; G[17] = s1; }
void func_08B380C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x10u; G[19] = G[6]; G[17] = G[7]; G[16] = G[5] << 2; call_here(rt, ctx, 0x08B62344u); G[3] = G[2]; G[2] = word(rt, G[18] + 4u); G[4] = word(rt, G[17] + 4u); G[5] = G[2] + G[16]; G[2] = 0x08CDA8F8u; G[6] = word(rt, G[5]); put(rt, G[3], G[2]); put(rt, G[3] + 4u, G[4]); if (G[4] != 0u) { G[2] = word(rt, G[4] + 4u) + 1u; put(rt, G[4] + 4u, G[2]); } G[2] = 0x08CDA8E8u; put(rt, G[3], G[2]); put(rt, G[3] + 8u, G[19]); put(rt, G[3] + 0xCu, G[6]); put(rt, G[5], G[3]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
// When there is an object at +0x2C, a 12-byte block (0x08A5BC20, align 4) pointing back at it is made and handed to 0x08B5C078
// with self + 0x30; then a fresh 0x30-byte node with two empty circular list heads (at +4 and +0x1C) replaces the object.
void func_08B47F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x560u); G[3] = G[7] & 0xFFFFu; G[16] = G[4]; G[2] = word(rt, G[4] + 4u); if (G[2] != 0u) { G[17] = frame.at(0x18u); G[4] = G[17]; put(rt, frame.at(0xCu), G[5]); put(rt, frame.at(4u), G[6]); put(rt, frame.at(0u), 0u); put(rt, frame.at(0x10u), 0u); store16(frame.at(8u), G[7]); if (G[3] != 0u) { call_here(rt, ctx, 0x08B47AB8u); G[6] = load16(frame.at(8u)); G[5] = word(rt, frame.at(4u)); G[4] = G[17]; call_here(rt, ctx, 0x08B47C00u); G[4] = frame.at(0u); if (G[2] != 0u) { G[5] = word(rt, G[16] + 8u); G[2] = word(rt, G[16] + 4u); put(rt, frame.at(0x10u), G[17]); call_here(rt, ctx, G[2]); } G[4] = G[17]; call_here(rt, ctx, 0x08B47ACCu); } } } G[16] = s0; G[17] = s1; }
void func_08B4F678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 vector = ctx.gpr[5];
    const u32 fallback = ctx.gpr[6];
    if (float_at(rt, vector) != 0.0f) {
        put_float(rt, out, -float_at(rt, vector + 4u));
        const float x = float_at(rt, vector);
        put_float(rt, out + 8u, 0.0f);
        put_float(rt, out + 4u, x);
        return;
    }
    if (float_at(rt, vector + 4u) != 0.0f) {
        put_float(rt, out, 0.0f);
        put_float(rt, out + 4u, -float_at(rt, vector + 8u));
        const float y = float_at(rt, vector + 4u);
        put_float(rt, out + 8u, y);
        return;
    }
    const float z = float_at(rt, vector + 8u);
    if (z != 0.0f) {
        put_float(rt, out, z);
        put_float(rt, out + 4u, 0.0f);
        put_float(rt, out + 8u, -float_at(rt, vector));
        return;
    }
    put_float(rt, out, float_at(rt, fallback));
    put_float(rt, out + 4u, float_at(rt, fallback + 4u));
    put_float(rt, out + 8u, float_at(rt, fallback + 8u));
}
// Erases a range of a vector of 8-byte (two float) elements (end at +4): the elements from a2 to the end move down to a1
// (0x08BF1070 is told the new end and the old one), and the end drops by (a2 - a1) & ~7. Answers a1.
void func_08B5C2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[19] = 0x08D70000u; G[17] = G[5]; G[16] = G[6] << 2; call_here(rt, ctx, 0x08C7A888u); G[3] = word(rt, G[18] + 0xCu); put(rt, 0x08D6E12Cu, G[2]); G[3] += G[16]; G[4] = word(rt, G[3]); if (G[4] == 0u) { G[4] = G[17] + 4u; G[5] = 1u; call_here(rt, ctx, 0x08B6085Cu); G[2] = word(rt, G[18] + 0xCu); G[2] = G[16] + G[2]; put(rt, G[2], G[17]); } else if (G[17] != 0u) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 4u); G[4] = G[17]; call_here(rt, ctx, G[2]); } G[4] = word(rt, 0x08D6E12Cu); call_here(rt, ctx, 0x08C7A8A8u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B85CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CB6FE0u; G[3] = G[4] + 0x3Cu; G[18] = G[4]; put(rt, G[4], G[2]); if (G[3] != 0u) { G[2] = G[4] + 0xA4u; G[16] = G[4] + 0xA0u; G[17] = 0x1Au; if (G[3] != G[2]) { for (;;) { G[2] = word(rt, G[16]); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[17] -= 1u; const bool z = G[17] == 0u; G[16] -= 4u; if (z) break; } } } G[4] = word(rt, G[18] + 0x38u); if (G[4] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08A7D4E4u); }
void func_08B9FD44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 handle = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 node = call_args(rt, ctx, 0x08B62344u, {0x14u});
    call_args(rt, ctx, 0x08B9FC30u, {node});
    put(rt, node + 8u, 0u);
    put(rt, node + 4u, 0u);
    put(rt, node, 0u);
    call_args(rt, ctx, 0x08B9FB40u, {node});
    call_args(rt, ctx, 0x08B9F960u, {node + 0xCu});
    store8(node + 0xCu, 1u);
    put(rt, node, 0u);
    put(rt, node + 4u, 0u);
    put(rt, node + 8u, 0u);
    put(rt, node + 0x10u, 0u);
    put(rt, handle, 0u);
    put(rt, handle + 4u, 0u);
    store8(handle + 8u, 0u);
    call_args(rt, ctx, 0x08B9D15Cu, {handle, node, 0u});
    store8(handle + 8u, 1u);
    ctx.gpr[2] = handle;
}
// The same for a 0x20-byte node: 100.0 at +0x10, 1000.0 at +0x14, zero at +0x18 and +0x1C after their sub-constructors.
void func_08BA17A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 handle = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 node = call_args(rt, ctx, 0x08B62344u, {0x20u});
    call_args(rt, ctx, 0x08BA1704u, {node});
    call_args(rt, ctx, 0x08BA1678u, {node + 0x10u});
    put_float(rt, node + 0x10u, 100.0f);
    call_args(rt, ctx, 0x08BA1588u, {node + 0x14u});
    put_float(rt, node + 0x14u, 1000.0f);
    call_args(rt, ctx, 0x08BA1498u, {node + 0x18u});
    put(rt, node + 0x18u, 0u);
    put(rt, node + 0x1Cu, 0u);
    put(rt, handle, 0u);
    put(rt, handle + 4u, 0u);
    store8(handle + 8u, 0u);
    call_args(rt, ctx, 0x08B9DA24u, {handle, node, 0u});
    store8(handle + 8u, 1u);
    ctx.gpr[2] = handle;
}
// Deleting destructor: the element block (begin at +0x14, end of storage at +0x1C) is released with the range helper 0x08C351DC and
// freed when it exists, the vtable goes 0x08CD5E80 -> 0x08CD5E30, the member at +0xC and the base part are released, and the
// 0x20-byte object is freed (0x08A5BC54, align 4).
void func_08BB8FA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CC3B30u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = G[4] + 0x844u; call_here(rt, ctx, 0x088F6284u); G[2] = G[16] + 0x30u; G[17] = G[16] + 0x840u; if (G[2] != 0u) { G[18] = G[16] + 0x438u; if (G[2] != G[17]) { G[19] = 2u; G[2] = word(rt, G[18]); for (;;) { G[17] -= 0x408u; G[19] -= 1u; G[3] = word(rt, G[2]); G[4] = G[17]; G[18] -= 0x408u; call_here(rt, ctx, G[3]); if (G[19] == 0u) break; G[2] = word(rt, G[18]); } } } G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; call_here(rt, ctx, 0x08AF20E8u); G[4] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B64654u); }
void func_08BF1080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[5] = G[4] + 4u; G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 3); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) > 0) { G[3] = G[6]; G[6] = 0u; do { ctx.fpr[0] = float_at(rt, G[3]); G[6] += 1u; put_float(rt, G[4], ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[3] + 4u); G[3] += 8u; put_float(rt, G[4] + 4u, ctx.fpr[1]); G[4] += 8u; } while (G[2] != G[6]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF1070u); G[3] = word(rt, G[17] + 4u); G[2] = G[18] - G[16]; G[2] &= ~7u; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
// Copies the three vectors at a1 + 0x14, + 0x20 and + 0x2C to a0 + 0x48, + 0x54 and + 0x60, the float at a1 + 4 to a0 + 0x6C, and
// stores at a0 + 0x70 what a1's virtual +0x18 answers (a float).
void func_08BF9270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x30u); G[2] = 0x0FFFFFFFu; G[3] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; bool run = true; if (G[3] != 0u) { G[16] = G[2]; G[4] = G[2]; G[17] = 0x08C90000u; } else { G[3] = 0u; if (static_cast<std::int32_t>(G[4]) <= 0) run = false; else { G[16] = G[4]; G[17] = 0x08C90000u; } } bool got = false; while (run) { G[4] <<= 3; G[5] = 0x08C95EF4u; call_here(rt, ctx, 0x08B644BCu); G[3] = G[2]; G[2] = G[16] >> 31; G[2] += G[16]; if (G[3] != 0u) { got = true; break; } G[16] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[4] = G[16]; if (static_cast<std::int32_t>(G[16]) <= 0) break; } if (!got) { G[3] = 0u; G[16] = 0u; } G[2] = G[3]; G[3] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08C18DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[4] = G[5]; G[16] = G[5]; G[18] = word(rt, G[5]); call_here(rt, ctx, 0x08B5C094u); G[2] = word(rt, G[16] + 0xCu); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[3] = word(rt, G[16] + 8u); G[2] = 0x08D74FD0u; G[17] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[17] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[16]; G[5] = 0x10u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C213F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[5]; G[16] = G[4]; G[2] = word(rt, G[4] + 8u); G[4] = 0u; if (G[2] != 0u) { G[17] = word(rt, G[2] + 0x30u); G[4] = 0x08CB02A4u; G[19] = word(rt, G[16] + 0xCu); if (G[17] == 0u) { call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 8u); } G[5] = word(rt, G[17] + 0x1Cu); G[4] = G[2]; G[5] = G[19] - G[5]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 2); G[5] -= 1u; call_here(rt, ctx, 0x08A905B4u); put(rt, G[18], G[2]); G[4] = 1u; G[2] = word(rt, G[16] + 8u); put(rt, G[18] + 4u, G[2]); G[3] = word(rt, G[16] + 0x10u); put(rt, G[18] + 8u, G[3]); } G[2] = G[4]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C32210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[4] = G[5]; G[5] = frame.at(8u); G[16] = G[6]; G[18] = frame.at(4u); call_here(rt, ctx, 0x08C31850u); G[17] = word(rt, G[16]); G[4] = frame.at(0u); if (G[17] == 0u) { ctx.fpr[12] = float_at(rt, frame.at(8u)); G[4] = G[18]; call_here(rt, ctx, 0x08B8C5ACu); G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = word(rt, G[17]); ctx.fpr[12] = float_at(rt, frame.at(8u)); G[16] = word(rt, G[2] + 0xF4u); call_here(rt, ctx, 0x08B8C5ACu); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, G[16]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C3BDF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD5E80u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x14u; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x14u); call_here(rt, ctx, 0x08C351DCu); G[2] = word(rt, G[17] + 0x14u); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 2); G[5] <<= 2; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[2] = 0x08CD5E30u; put(rt, G[17], G[2]); G[4] = G[17] + 0xCu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[17]; G[5] = 0x20u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); } G[16] = s0; G[17] = s1; }
// Waits (0x08C7A988 with 10000) for every 0x60B0-byte entry of the table at 0x08D41AD0 (array pointer +0x478, count +0x47C) to
// clear its busy byte (+0x38); v0 is the table when it has no entries and 0 after the last one.
void func_08C5B9A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = G[6]; G[18] = G[8]; G[6] = G[5]; G[5] = sext(G[7] & 0xFFu, 8); G[8] = G[10]; G[19] = G[11]; G[4] = G[9]; G[7] = G[2]; G[16] = G[9]; G[17] = G[18] - G[10]; if (G[18] == 0u) G[17] = word(rt, G[11]); G[9] = G[17] + G[8]; call_here(rt, ctx, 0x08C5942Cu); G[4] = G[2]; G[5] = G[18]; G[16] = G[2] - G[16]; if (G[18] != 0u) { G[6] = word(rt, G[19]); G[6] -= G[17]; call_here(rt, ctx, 0x08B79A88u); G[3] = word(rt, G[19]); G[3] -= G[17]; G[16] += G[3]; } put(rt, G[19], G[16]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C735D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[8] = 0x08B8D5C4u; G[17] = G[7]; G[16] = word(rt, G[4]); G[2] = word(rt, G[16] - 0xCu); G[4] = G[16] + G[6]; G[6] += G[7]; G[18] = G[16] + G[2]; G[2] = G[2] < G[6] ? 1u : 0u; G[6] = G[5]; G[7] += G[6]; G[5] = G[18]; bool fail = G[2] != 0u; if (!fail) { call_here(rt, ctx, 0x08B8D5D8u); G[4] = G[2] - G[16]; if (G[18] == G[2] && G[17] != 0u) fail = true; } if (fail) G[4] = 0xFFFFFFFFu; G[2] = G[4]; } G[16] = s0; G[17] = s1; G[18] = s2; }
// A 16-byte node (0x08B62344) with the vtable 0x08CDA8E8, the ref-counted object of a3 + 4 (count at +4 incremented), a2 and the
// previous entry of the list at a0 + 4 slot a1; the node becomes that slot's entry.

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_204
} // namespace gt::game

#define HOT204(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_204::name, gt::ReturnKind::kind)
// HOT204(0x088334D8u, func_088334D8, Word);  // no comparable synthetic case yet
// HOT204(0x08833E1Cu, func_08833E1C, Word);  // no comparable synthetic case yet
HOT204(0x088AB254u, func_088AB254, Word);
HOT204(0x08B929A0u, func_08B929A0, Word);
// HOT204(0x08833FCCu, func_08833FCC, Word);  // no comparable synthetic case yet
// HOT204(0x08834068u, func_08834068, Word);  // no comparable synthetic case yet
// HOT204(0x08841BD4u, func_08841BD4, Word);  // no comparable synthetic case yet
HOT204(0x08AC42FCu, func_08AC42FC, Word);
HOT204(0x08ACC220u, func_08ACC220, Word);
HOT204(0x08ACCAB4u, func_08ACCAB4, Word);
// HOT204(0x088578B0u, func_088578B0, Word);  // no comparable synthetic case yet
HOT204(0x08C3E840u, func_08C3E840, Word);
HOT204(0x08805320u, func_08805320, Word);
HOT204(0x0880B678u, func_0880B678, Word);
HOT204(0x088209A0u, func_088209A0, Word);
HOT204(0x0882ED78u, func_0882ED78, Word);
// HOT204(0x088315ACu, func_088315AC, Word);  // no comparable synthetic case yet
// HOT204(0x088345D4u, func_088345D4, Word);  // no comparable synthetic case yet
// HOT204(0x08856E5Cu, func_08856E5C, Word);  // no comparable synthetic case yet
HOT204(0x0885DB58u, func_0885DB58, Word);
HOT204(0x08872548u, func_08872548, Word);
HOT204(0x08883AE0u, func_08883AE0, Word);
HOT204(0x088937E0u, func_088937E0, Word);
HOT204(0x08897190u, func_08897190, Word);
HOT204(0x0889A100u, func_0889A100, Word);
HOT204(0x088A15B0u, func_088A15B0, Word);
// HOT204(0x088A5DB8u, func_088A5DB8, Word);  // no comparable synthetic case yet
HOT204(0x088B2DB0u, func_088B2DB0, Word);
HOT204(0x088E5DC4u, func_088E5DC4, Word);
HOT204(0x088EC28Cu, func_088EC28C, Word);
HOT204(0x08907874u, func_08907874, Float);
HOT204(0x0891A8F0u, func_0891A8F0, Word);
HOT204(0x0892B558u, func_0892B558, Word);
HOT204(0x08930C0Cu, func_08930C0C, Word);
HOT204(0x08936D20u, func_08936D20, Word);
HOT204(0x0893A174u, func_0893A174, Word);
HOT204(0x0893A210u, func_0893A210, Word);
HOT204(0x0894161Cu, func_0894161C, Word);
HOT204(0x0894A520u, func_0894A520, Float);
HOT204(0x0894E678u, func_0894E678, Word);
HOT204(0x089A6C20u, func_089A6C20, Word);
HOT204(0x089AD9FCu, func_089AD9FC, Word);
HOT204(0x089AF974u, func_089AF974, Word);
HOT204(0x089B5B3Cu, func_089B5B3C, Word);
HOT204(0x089C4810u, func_089C4810, Word);
HOT204(0x089C843Cu, func_089C843C, Word);
// HOT204(0x089C95D4u, func_089C95D4, Word);  // no comparable synthetic case yet
// HOT204(0x089D60F4u, func_089D60F4, Word);  // no comparable synthetic case yet
HOT204(0x08A0B1A4u, func_08A0B1A4, Word);
// HOT204(0x08A14650u, func_08A14650, Word);  // no comparable synthetic case yet
HOT204(0x08A20360u, func_08A20360, Word);
// HOT204(0x08A238A8u, func_08A238A8, Word);  // no comparable synthetic case yet
HOT204(0x08A30DACu, func_08A30DAC, Word);
// HOT204(0x08A33BD4u, func_08A33BD4, Word);  // no comparable synthetic case yet
HOT204(0x08A3A520u, func_08A3A520, Word);
// HOT204(0x08A3AC40u, func_08A3AC40, Word);  // no comparable synthetic case yet
HOT204(0x08A50038u, func_08A50038, Word);
HOT204(0x08A55420u, func_08A55420, Word);
HOT204(0x08A57894u, func_08A57894, Word);
// HOT204(0x08A738A8u, func_08A738A8, Word);  // no comparable synthetic case yet
// HOT204(0x08A83824u, func_08A83824, Word);  // no comparable synthetic case yet
// HOT204(0x08A9661Cu, func_08A9661C, Word);  // no comparable synthetic case yet
// HOT204(0x08AB0788u, func_08AB0788, Word);  // no comparable synthetic case yet
HOT204(0x08A9A994u, func_08A9A994, Word);
HOT204(0x08AA49B8u, func_08AA49B8, Word);
// HOT204(0x08AC87E4u, func_08AC87E4, Word);  // no comparable synthetic case yet
HOT204(0x08AD7CC0u, func_08AD7CC0, Word);
HOT204(0x08AD9E74u, func_08AD9E74, Word);
HOT204(0x08AEB320u, func_08AEB320, Word);
HOT204(0x08AED010u, func_08AED010, Word);
HOT204(0x08AF7504u, func_08AF7504, Word);
HOT204(0x08AF89B4u, func_08AF89B4, Word);
// HOT204(0x08B01154u, func_08B01154, Word);  // no comparable synthetic case yet
HOT204(0x08B0B1F8u, func_08B0B1F8, Word);
HOT204(0x08B13038u, func_08B13038, Word);
// HOT204(0x08B3209Cu, func_08B3209C, Word);  // no comparable synthetic case yet
HOT204(0x08B380C4u, func_08B380C4, Word);
HOT204(0x08B47F3Cu, func_08B47F3C, Word);
HOT204(0x08B4F678u, func_08B4F678, Word);
HOT204(0x08B5C2F8u, func_08B5C2F8, Word);
// HOT204(0x08B85CB8u, func_08B85CB8, Word);  // no comparable synthetic case yet
HOT204(0x08B9FD44u, func_08B9FD44, Word);
HOT204(0x08BA17A4u, func_08BA17A4, Word);
// HOT204(0x08BB8FA8u, func_08BB8FA8, Word);  // no comparable synthetic case yet
HOT204(0x08BF1080u, func_08BF1080, Word);
HOT204(0x08BF9270u, func_08BF9270, DoubleWord);
HOT204(0x08C18DDCu, func_08C18DDC, Word);
HOT204(0x08C213F0u, func_08C213F0, Word);
// HOT204(0x08C32210u, func_08C32210, Word);  // no comparable synthetic case yet
HOT204(0x08C3BDF8u, func_08C3BDF8, Word);
// HOT204(0x08C5B9A4u, func_08C5B9A4, Word);  // no comparable synthetic case yet
HOT204(0x08C735D0u, func_08C735D0, Word);
