// Hot batch 195: thirty-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_195 {
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
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// `target(a1, a2, a3)` with the errno slot (0x08D774A8) cleared first; on -1 the slot's value (when set) is stored through a0.
void call_with_errno(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target) {
    const u32 out = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    put(rt, 0x08D774A8u, 0u);
    const u32 result = call(rt, ctx, target, {first, second, third});
    if (result == 0xFFFFFFFFu) {
        const u32 error = word(rt, 0x08D774A8u);
        if (error != 0u) put(rt, out, error);
    }
    ctx.gpr[2] = result;
    ctx.gpr[3] = result;
}
// Two guest queries on a1 (the second one only when the first answers); when both answer, a1 is released into `target(second, a2)`.
// Returns whether that tail call is taken (v0 = the second answer).
bool query_twice(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[5];
    const u32 extra = ctx.gpr[6];
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0x08C9FF6Cu;
    if (object == 0u) return false;
    const u32 first = call(rt, ctx, 0x0886A950u, {object, 0x08C9FF6Cu});
    ctx.gpr[4] = object;
    ctx.gpr[5] = 2u;
    if (first == 0u) return false;
    const u32 second = call(rt, ctx, 0x0886A9A0u, {object, 2u});
    ctx.gpr[4] = second;
    ctx.gpr[5] = extra;
    return second != 0u;
}
// A destructor of an object that owns a reference-counted buffer at `self + buffer_slot` (the body starts 0xC in front of the
// pointer, the shared empty body is `empty`): set the first vtable, drop one reference (`0x08B60804(buffer - 4, -1)`), release
// the body through `release(body, frame)` when the count was not positive, then set the second vtable.
void release_counted_buffer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable_before, u32 buffer_slot,
                            u32 empty, u32 release, u32 vtable_after) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 buffer = word(rt, self + buffer_slot);
    const u32 body = buffer - 0xCu;
    ctx.gpr[3] = vtable_before;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = buffer - 4u;
    put(rt, self, vtable_before);
    ctx.gpr[2] = empty;
    if (body != empty) {
        const u32 references = call(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame.at(0u);
        if (static_cast<s32>(references) <= 0) call(rt, ctx, release, {body, frame.at(0u)});
    }
    ctx.gpr[2] = vtable_after;
    put(rt, self, vtable_after);
}
// Destructor body: set the first vtable, call the pointer-to-member at self + 8 / + 0xC / + 0x10 (low bit = virtual: the
// function comes from the vtable of the adjusted object), set the final vtable.
void call_member_then_reset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable_before, u32 vtable_after) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 packed = word(rt, self + 0x10u);
        put(rt, self, vtable_before);
        const u32 adjust = static_cast<u32>(static_cast<s32>(packed) >> 1);
        const u32 base = word(rt, self + 8u);
        const u32 object = base + adjust;
        u32 target;
        if ((packed & 1u) != 0u) {
            ctx.gpr[3] = word(rt, self + 0xCu);
            target = word(rt, word(rt, object) + ctx.gpr[3]);
        } else {
            target = word(rt, self + 0xCu);
        }
        ctx.gpr[4] = object;
        call_indirect(rt, ctx, target, {object});
        ctx.gpr[2] = vtable_after;
        put(rt, self, vtable_after);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}
// A vector holder's destructor: set the vtable, `fill(begin, [self + 0x18], frame)`, free `begin` through 0x08A5BC54 (size rounded
// to whole words, alignment 4) when it is set, then run 0x08A780B0(self).
void destroy_vector_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable, u32 fill) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, vtable);
    call(rt, ctx, fill, {word(rt, self + 0x14u), word(rt, self + 0x18u), frame.at(0u)});
    const u32 begin = word(rt, self + 0x14u);
    const u32 end = word(rt, self + 0x1Cu);
    const u32 bytes = static_cast<u32>(static_cast<s32>(end - begin) >> 2) << 2;
    ctx.gpr[4] = begin;
    ctx.gpr[5] = bytes;
    ctx.gpr[6] = 4u;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    call(rt, ctx, 0x08A780B0u, {self});
}
// `a2 > 0`: 0x08A61C18 fills a frame slot with a handle, which (the object at its address, 0x08A5BB7C reports the failure when
// null) is passed with the list and counts down into 0x08A641F4; the handle is then released with 0x08A5B9EC.
void with_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 count = ctx.gpr[6];
    const u32 list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = frame.at(0u);
    if (static_cast<s32>(count) <= 0) return;
    call(rt, ctx, 0x08A61C18u, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08CB02D0u;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB02D0u});
    call(rt, ctx, 0x08A641F4u, {object, list, count - 1u, list + 4u});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08B7A2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_errno(rt, ctx, 0x08B7748Cu);
}
void func_08B80438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_errno(rt, ctx, 0x08B778B0u);
}
void func_08B807FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_errno(rt, ctx, 0x08B77524u);
}
void func_08B82344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_with_errno(rt, ctx, 0x08B7780Cu);
}
void func_08A66604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    release_counted_buffer(rt, ctx, 0x08CD2D18u, 8u, 0x08D74FD0u, 0x08B8542Cu, 0x08CD3638u);
}
void func_08A6732C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    release_counted_buffer(rt, ctx, 0x08CD2EA8u, 8u, 0x08D74FD0u, 0x08B8542Cu, 0x08CD3638u);
}
void func_08BC39C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    release_counted_buffer(rt, ctx, 0x08CC4F50u, 4u, 0x08D77498u, 0x08C74884u, 0x08CBFF70u);
}
void func_08BC3B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    release_counted_buffer(rt, ctx, 0x08CC4F08u, 4u, 0x08D75298u, 0x08B93554u, 0x08CBFF70u);
}
void func_08BBD66C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_reset(rt, ctx, 0x08CC52D0u, 0x08CC52E0u);
}
void func_08BCF8E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_reset(rt, ctx, 0x08CC53F8u, 0x08CC52E0u);
}
void func_08BD60C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call_member_then_reset(rt, ctx, 0x08CC55F8u, 0x08CC52E0u);
}
void func_089BF1B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x2B0u; G[16] = G[4]; G[4] = G[18]; G[17] = 0u; call_here(rt, ctx, 0x08B344E0u); G[3] = word(rt, G[16] + 0x264u); G[16] = 0u; G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x50u); call_here(rt, ctx, G[2]); G[16] = G[2]; G[17] = G[3]; } G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[3] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089BF228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x2B0u; G[16] = G[4]; G[4] = G[18]; G[17] = 0u; call_here(rt, ctx, 0x08B344E0u); G[3] = word(rt, G[16] + 0x264u); G[16] = 0u; G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x54u); call_here(rt, ctx, G[2]); G[16] = G[2]; G[17] = G[3]; } G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[3] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089BF2A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[4] + 0x2B0u; G[16] = G[4]; G[4] = G[18]; G[17] = 0u; call_here(rt, ctx, 0x08B344E0u); G[3] = word(rt, G[16] + 0x264u); G[16] = 0u; G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x58u); call_here(rt, ctx, G[2]); G[16] = G[2]; G[17] = G[3]; } G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[3] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C39BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = 4u; G[5] = frame.at(0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x108u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = 4u; G[6] = 0u; if (G[2] == G[3]) { G[7] = word(rt, G[16] + 0x24u); G[5] = 0u; G[9] = 3u; G[8] = 4u; do { G[2] = G[5] + frame.at(0u); G[3] = G[9] - G[5]; G[4] = load8(G[2]); if (G[7] == 0u) G[3] = G[5]; G[3] <<= 3; G[4] = G[4] << (G[3] % 32u); G[5] += 1u; G[6] |= G[4]; } while (G[5] != G[8]); } G[2] = G[6]; } G[16] = s0; }
void func_08C39D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[6] = 4u; G[5] = frame.at(0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x108u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = 4u; G[6] = 0u; if (G[2] == G[3]) { G[7] = word(rt, G[16] + 0x24u); G[5] = 0u; G[9] = 3u; G[8] = 4u; do { G[2] = G[5] + frame.at(0u); G[3] = G[9] - G[5]; G[4] = load8(G[2]); if (G[7] == 0u) G[3] = G[5]; G[3] <<= 3; G[4] = G[4] << (G[3] % 32u); G[5] += 1u; G[6] |= G[4]; } while (G[5] != G[8]); } G[2] = G[6]; } G[16] = s0; }
void func_08AA2330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB2B50u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08AA23A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB2B50u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A83160u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A5DF58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB0080u; G[16] = word(rt, G[6]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A831D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB168Cu; G[16] = word(rt, G[6]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A83160u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A9893C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CB7108u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x14u; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x14u); call_here(rt, ctx, 0x08B856B0u); G[2] = word(rt, G[17] + 0x14u); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 2) << 2; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); } G[16] = s0; G[17] = s1; }
void func_08B87970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    destroy_vector_holder(rt, ctx, 0x08CB89B8u, 0x08B876E0u);
}
void func_08A716A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CCD748u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x18u; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x08C249F8u); G[2] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[4] = G[17] + 0x14u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); } G[16] = s0; G[17] = s1; }
void func_08A7171C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CCD748u; G[17] = G[4]; G[6] = frame.at(0u); G[16] = G[4] + 0x18u; put(rt, G[4], G[2]); G[5] = word(rt, G[16] + 4u); G[4] = word(rt, G[4] + 0x18u); call_here(rt, ctx, 0x08C249F8u); G[2] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); G[4] = G[17] + 0x14u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); } G[16] = s0; G[17] = s1; }
void func_08A642C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_handle(rt, ctx);
}
void func_08A64340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_handle(rt, ctx);
}
void func_08A56630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[4]; G[4] = 0x08CAF798u; G[18] = G[5]; G[16] = word(rt, G[5] + 0x14u); G[17] = G[6]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[6] = G[18]; G[2] = word(rt, G[3] + 0x140u); G[7] = G[17]; G[4] = G[19]; call_here(rt, ctx, G[2]); G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A56784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[4]; G[4] = 0x08CAF798u; G[18] = G[5]; G[16] = word(rt, G[5] + 0x14u); G[17] = G[6]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[6] = G[18]; G[2] = word(rt, G[3] + 0x130u); G[7] = G[17]; G[4] = G[19]; call_here(rt, ctx, G[2]); G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A2F5B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x40u); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B09F98u); G[4] = word(rt, 0x08CF6648u); G[5] = G[16]; call_here(rt, ctx, 0x08B09438u); G[5] = G[16]; G[4] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x08A2DCE0u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[4] = G[16]; call_here(rt, ctx, 0x08B09290u); } G[16] = s0; G[17] = s1; }
void func_08A2F628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x40u); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B09F98u); G[4] = word(rt, 0x08CF6648u); G[5] = G[16]; call_here(rt, ctx, 0x08B09528u); G[5] = G[16]; G[4] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x08A2DCE0u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[4] = G[16]; call_here(rt, ctx, 0x08B09290u); } G[16] = s0; G[17] = s1; }
void func_08921CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[16] = G[4]; G[6] = G[5]; G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x08921AF0u); G[2] = G[16] + 0x220u; ctx.fpr[6] = float_at(rt, frame.at(0x18u)); G[4] = word(rt, frame.at(0x1Cu)); ctx.fpr[2] = float_at(rt, frame.at(4u)); ctx.fpr[3] = float_at(rt, frame.at(8u)); ctx.fpr[4] = float_at(rt, frame.at(0xCu)); ctx.fpr[5] = float_at(rt, frame.at(0x10u)); ctx.fpr[1] = float_at(rt, frame.at(0x14u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); G[3] = G[16] + 0x22Cu; put_float(rt, G[16] + 0x220u, ctx.fpr[0]); put_float(rt, G[2] + 4u, ctx.fpr[2]); put_float(rt, G[2] + 8u, ctx.fpr[3]); put_float(rt, G[16] + 0x22Cu, ctx.fpr[4]); put_float(rt, G[3] + 8u, ctx.fpr[1]); put_float(rt, G[3] + 4u, ctx.fpr[5]); put(rt, G[2] + 0x1Cu, G[4]); put_float(rt, G[2] + 0x18u, ctx.fpr[6]); } G[16] = s0; }
void func_08921DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x30u); G[16] = G[4]; G[6] = G[5]; G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x08921AF0u); G[2] = G[16] + 0x200u; ctx.fpr[6] = float_at(rt, frame.at(0x18u)); G[4] = word(rt, frame.at(0x1Cu)); ctx.fpr[2] = float_at(rt, frame.at(4u)); ctx.fpr[3] = float_at(rt, frame.at(8u)); ctx.fpr[4] = float_at(rt, frame.at(0xCu)); ctx.fpr[5] = float_at(rt, frame.at(0x10u)); ctx.fpr[1] = float_at(rt, frame.at(0x14u)); ctx.fpr[0] = float_at(rt, frame.at(0u)); G[3] = G[16] + 0x20Cu; put_float(rt, G[16] + 0x200u, ctx.fpr[0]); put_float(rt, G[2] + 4u, ctx.fpr[2]); put_float(rt, G[2] + 8u, ctx.fpr[3]); put_float(rt, G[16] + 0x20Cu, ctx.fpr[4]); put_float(rt, G[3] + 8u, ctx.fpr[1]); put_float(rt, G[3] + 4u, ctx.fpr[5]); put(rt, G[2] + 0x1Cu, G[4]); put_float(rt, G[2] + 0x18u, ctx.fpr[6]); } G[16] = s0; }
void func_088753A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bool take_tail;
    {
        GuestFrame frame(ctx, 0x10u);
        take_tail = query_twice(rt, ctx);
    }
    if (take_tail) tail_call(ctx, 0x088852BCu, {ctx.gpr[4], ctx.gpr[5]});
}
void func_088758FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bool take_tail;
    {
        GuestFrame frame(ctx, 0x10u);
        take_tail = query_twice(rt, ctx);
    }
    if (take_tail) tail_call(ctx, 0x088852B4u, {ctx.gpr[4], ctx.gpr[5]});
}
void func_0885D950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[7] = 0u; G[17] = G[5]; G[5] = G[6]; G[9] = 0xFFFFFFFFu; G[8] = 2u; G[16] = G[4] + 0x118u; G[3] = G[4] + 0xB8u; bool found = false; for (;;) { G[2] = word(rt, G[4] + 0x118u); G[7] += 1u; G[4] += 4u; if (G[2] == G[9] || G[2] == G[17]) { found = true; break; } G[3] += 0x30u; const bool more = G[7] != G[8]; G[16] += 4u; if (!more) break; } if (found) { G[4] = G[3]; call_here(rt, ctx, 0x08897470u); G[2] = 1u; put(rt, G[16], G[17]); } else G[2] = 0u; } G[16] = s0; G[17] = s1; }
void func_08865F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; G[4] = word(rt, G[4] + 4u); G[17] = G[5] % 0x100u; if (G[4] != 0u) { store8(G[16] + 0x24u, G[17]); call_here(rt, ctx, 0x0892BB04u); G[5] = G[2]; G[4] = word(rt, G[16] + 0xCu); G[6] = frame.at(0u); G[7] = 4u; call_here(rt, ctx, 0x088F8EE8u); G[6] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { G[5] = G[17]; G[4] = 0u; G[3] = frame.at(0u); do { G[2] = word(rt, G[3]); G[4] += 1u; G[3] += 4u; store8(G[2] + 0xBu, G[5]); } while (G[6] != G[4]); } } } G[16] = s0; G[17] = s1; }
void func_0886DF90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = 6u; G[16] = G[4]; call_here(rt, ctx, 0x0886A9A0u); G[4] = G[16]; if (G[2] == 0u) { G[5] = word(rt, G[16] + 0x70u) + 0x28u; call_here(rt, ctx, 0x0886AAA4u); } G[4] = word(rt, G[16] + 0x70u) + 0x28u; G[5] = G[17]; call_here(rt, ctx, 0x08874BE0u); G[4] = G[16]; G[5] = 6u; G[3] = 1u; if (G[2] == 0u) { call_here(rt, ctx, 0x0886AA0Cu); G[3] = 0u; } G[2] = G[3]; } G[16] = s0; G[17] = s1; }
void func_08882928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CC1460u; G[16] = G[4]; G[17] = G[16] + 0xCu; G[4] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[16] + 8u); put(rt, G[16], G[2]); call_here(rt, ctx, 0x08882854u); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0xCu); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0xCu, 0u); } G[2] = 0x08CBF428u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08886E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[2] = word(rt, G[4] + 0x40u); G[16] = G[4]; if (G[2] != 0u) { G[3] = word(rt, G[4] + 0x68u); G[2] = load16(G[3] + 0x26u) % 2u; if (G[2] == 0u) { call_here(rt, ctx, 0x088858B0u); G[4] = G[16]; call_here(rt, ctx, 0x08884B98u); G[6] = word(rt, G[16] + 0x184u); G[4] = G[16]; G[5] = G[17]; tail = true; } } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08886B00u); }
void func_088CFA9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x40u); G[5] = frame.at(0u); G[6] = frame.at(0xCu); put(rt, frame.at(0x30u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; ctx.fpr[12] = ctx.fpr[13]; ctx.fpr[13] = 0.0f; G[7] = 6u; G[2] = word(rt, G[4]); G[16] = word(rt, G[2] + 0x61Cu); G[4] = G[16]; call_here(rt, ctx, 0x088D390Cu); ctx.fpr[12] = float_at(rt, frame.at(0x10u)); G[4] = G[16]; ctx.fpr[12] = ctx.fpr[20] - ctx.fpr[12]; call_here(rt, ctx, 0x088D2DE0u); ctx.fpr[1] = ctx.fpr[0]; ctx.fpr[0] = 0.0f; if (ctx.fpr[1] < ctx.fpr[0]) ctx.fpr[1] = -ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[1]; } G[16] = s0; ctx.fpr[20] = f20; }
void func_088D75E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; call_here(rt, ctx, 0x088D9B9Cu); G[16] = G[2]; G[2] = word(rt, G[2] + 0x1Cu); G[4] = G[16]; bool ok = false; if (G[2] == 0u) { G[17] += 0xC8u; call_here(rt, ctx, 0x088E85C4u); G[3] = 5u; G[4] = G[17]; if (G[2] == G[3]) { call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[5] = 3u; call_here(rt, ctx, 0x088E8A44u); G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); ok = true; } } G[2] = ok ? 1u : 0u; } G[16] = s0; G[17] = s1; }
void func_088DCF58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x60u); G[17] = G[5]; G[5] = G[4]; G[4] = G[17]; G[18] = frame.at(0x10u); call_here(rt, ctx, 0x088DC858u); G[5] = frame.at(0u); G[6] = frame.at(8u); G[7] = frame.at(4u); G[8] = G[18]; if (G[2] != 0u) { G[4] = word(rt, G[17] + 0x640u); G[16] = word(rt, G[17] + 0x618u); call_here(rt, ctx, 0x088E4DFCu); G[5] = word(rt, frame.at(8u)); G[4] = G[16]; G[6] = G[18]; call_here(rt, ctx, 0x088D2594u); } G[2] = 1u; store8(G[17] + 0x28u, G[2]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088E6648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[19] = G[7]; G[18] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x088E511Cu); G[4] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x088E6548u); G[4] = word(rt, G[16]); G[6] = G[18]; G[7] = G[19]; G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x6Cu); target = G[25]; } } G[16] = s0; G[18] = s2; G[19] = s3; if (target != 0u) jump(ctx, target); }
void func_088E6B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[3] = word(rt, G[4]); G[2] = sext(load8(G[3] + 0x52Du), 8u); G[4] = G[3] + 0xECu; if (G[2] == 0u) { call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, G[2]); G[3] = 1u; if (G[2] != 0u) { store8(G[2] + 6u, G[3]); G[2] = word(rt, G[16]); store8(G[2] + 0x52Du, G[3]); } G[4] = word(rt, G[16]) + 0xECu; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B3457Cu); }
void func_088E798C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x40u); G[7] = 6u; G[18] = G[4]; G[17] = G[5]; G[5] = frame.at(0u); G[16] = frame.at(0xCu); G[6] = G[16]; put(rt, frame.at(0x30u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[13]; ctx.fpr[13] = 0.0f; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] + 0x61Cu); call_here(rt, ctx, 0x088D390Cu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[18]; G[5] = G[17]; G[6] = frame.at(0u); G[7] = G[16]; call_here(rt, ctx, 0x088E7894u); } G[16] = s0; G[17] = s1; G[18] = s2; ctx.fpr[20] = f20; }
void func_088F1AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = 0.0f; ctx.fpr[2] = ctx.fpr[20]; ctx.fpr[0] = float_at(rt, G[4] + 0x9BCu); ctx.fpr[1] = float_at(rt, G[4] + 0x9C0u); ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[12]; const bool c = ctx.fpr[1] < ctx.fpr[0]; G[4] += 0x844u; if (c) { ctx.fpr[2] = ctx.fpr[0] - ctx.fpr[1]; put_float(rt, G[16] + 0x9C0u, ctx.fpr[0]); } ctx.fpr[0] = float_at(rt, G[16] + 0x9B8u); ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[0]; const bool d = ctx.fpr[20] < ctx.fpr[0]; ctx.fpr[12] = ctx.fpr[0]; if (d) { call_here(rt, ctx, 0x088F5DF0u); put_float(rt, G[16] + 0x9B8u, ctx.fpr[20]); } } G[16] = s0; ctx.fpr[20] = f20; }
void func_088F2BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[2] = 0xFFFFFFFFu; store16(G[4] + 8u, G[2]); G[5] = 0u; G[4] = 1u; store16(G[6] + 0xAu, 0u); G[7] = 0x100u; do { G[3] = word(rt, G[6] + 4u) + G[5]; G[2] = word(rt, G[3]); G[5] += 0x40u; G[2] = (G[2] >> 13 << 13) | (G[4] % 0x2000u); G[4] += 1u; put(rt, G[3], G[2]); } while (G[4] != G[7]); G[4] = word(rt, G[6] + 4u); G[2] = 0xFFFFFFFFu; G[5] = G[6]; G[3] = word(rt, G[4] + 0x3FC0u); G[7] = 0u; G[8] = 6u; G[3] = (G[3] >> 13 << 13) | 0x1FFFu; put(rt, G[4] + 0x3FC0u, G[3]); do { G[7] += 1u; put(rt, G[5] + 0xCu, 0u); G[5] += 4u; } while (G[7] != G[8]); G[2] = 0x1234u; put(rt, G[6] + 0x198u, G[2]); }
void func_088FB8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = G[5]; G[5] = G[7]; G[19] = G[4]; G[4] = G[17]; G[18] = G[6]; G[6] = 0xCu; G[16] = G[8]; call_here(rt, ctx, 0x08B79BACu); G[4] = G[18]; G[5] = G[16]; G[6] = 0xCu; call_here(rt, ctx, 0x08B79BACu); G[4] = G[19]; G[5] = G[17]; G[6] = G[18]; G[7] = 0u; call_here(rt, ctx, 0x088FB3D8u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08910B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; if (G[5] != 0u && G[6] != 0u) { G[2] = load8(G[17]); G[4] = G[5]; if (G[2] == 0u) { call_here(rt, ctx, 0x08915DECu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08916098u); G[2] = 1u; store8(G[17], G[2]); G[2] = 0xFFFFFFFFu; put(rt, G[17] + 8u, G[16]); put(rt, G[17] + 4u, G[2]); } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0892BA30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = G[6]; G[19] = G[5]; G[18] = G[6]; call_here(rt, ctx, 0x0892B714u); G[4] = G[2]; call_here(rt, ctx, 0x08B21938u); put(rt, G[17] + 0x2Cu, G[2]); G[4] = G[19]; G[5] = G[18]; G[3] = word(rt, G[19]); G[2] = word(rt, G[3] + 0x14u); G[16] = word(rt, G[17] + 0x260u); call_here(rt, ctx, G[2]); put(rt, G[16], G[2]); G[2] = word(rt, G[17] + 0x260u); put(rt, G[2] + 0x98u, G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08940218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 2u; G[17] = G[6] % 0x100u; G[16] = G[4]; G[4] = word(rt, G[4] + 8u); G[3] = sext(load8(G[4] + 3u), 8u); G[6] = sext(load8(G[5] + 1u), 8u); if (G[3] == G[2]) store8(G[4] + 4u, 0u); else { G[2] = 4u; if (G[3] == G[2] || G[3] == 0u) store8(G[4] + 4u, G[6]); } G[5] = sext(load8(G[5] + 0xBu), 8u); G[4] = G[16]; G[5] = G[5] != 0u ? 1u : 0u; call_here(rt, ctx, 0x08958B94u); G[4] = G[16]; G[5] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x0895BB58u); }
void func_08949128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[4] + 8u); G[3] = word(rt, G[4] + 0xCu); G[2] = load8(G[5] + 0x579u); G[4] = G[3] + 0x170u; bool sum; if (G[2] == 0u) { G[3] = load16(G[4] + 0xCCu); G[2] = sext(load16(G[5] + 0x58Eu), 16u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] != 0u) sum = true; else { G[2] = 1u; store8(G[5] + 0x579u, G[2]); sum = false; } } else { G[2] = load16(G[4] + 0xCCu); G[3] = sext(load16(G[5] + 0x58Eu), 16u); G[2] -= 0x1F4u; G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[3] == 0u) sum = false; else { store8(G[5] + 0x579u, 0u); sum = true; } } if (sum) { ctx.fpr[0] = float_at(rt, G[5] + 0x57Cu); ctx.fpr[1] = float_at(rt, G[5] + 0x580u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[12]; } else { ctx.fpr[1] = float_at(rt, G[5] + 0x580u); ctx.fpr[0] = 0.0f; } ctx.fpr[12] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[13] = float_at(rt, G[5] + 0x590u); jump(ctx, 0x0894BFF4u); }
void func_0895C0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = 0.0f; G[18] = 4u; G[17] = G[4]; G[16] = 0u; G[5] = G[16]; bool hit = false; for (;;) { G[4] = G[17]; call_here(rt, ctx, 0x08955300u); ctx.fpr[0] = float_at(rt, G[2] + 0x90u); G[16] += 1u; if (!(ctx.fpr[0] == ctx.fpr[20])) { hit = true; break; } G[5] = G[16]; if (G[16] == G[18]) break; } G[2] = hit ? 1u : 0u; } G[16] = s0; G[17] = s1; G[18] = s2; ctx.fpr[20] = f20; }
void func_089B0044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x08CF4ACCu; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; if (G[16] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[17] + 0x40u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[17]; put(rt, G[17] + 0x40u, G[16]); G[5] = G[16]; call_here(rt, ctx, 0x089AFE84u); G[4] = 0x08CF4ACCu; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_089B6244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; call_here(rt, ctx, 0x08A36C3Cu); G[4] = 0x08CA5910u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA5918u; G[4] = G[16]; call_here(rt, ctx, 0x08A34DA8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x089B601Cu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089B7C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = G[6]; G[16] = G[4]; call_here(rt, ctx, 0x089B5BD8u); G[5] = G[2]; G[4] = G[16] + 0x138u; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08B207A4u); G[4] = G[16]; G[5] = G[17]; if (G[17] != 0u) { call_here(rt, ctx, 0x089B7A08u); G[4] = G[16]; G[5] = G[17]; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x089B6E88u); }
void func_089C1E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x60u) % 0x400u / 0x200u * 0x200u; G[16] = G[4]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A0F8C8u); G[3] = load8(G[16] + 0xC8u); G[4] = G[16]; G[1] = 0x3F800000u; ctx.fpr[12] = std::bit_cast<float>(G[1]); G[5] = G[16] + 0x90u; put(rt, G[16] + 0x78u, G[2]); if (G[3] != 0u) G[5] = G[16] + 0x80u; else ctx.fpr[12] = float_at(rt, G[16] + 0xA0u); tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x089C1A1Cu); }
void func_089CFD28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[16] = frame.at(4u); G[18] = G[5]; G[5] = frame.at(0u); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0u), G[6]); call_here(rt, ctx, 0x08969188u); G[5] = G[18]; G[6] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x089CDDF8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[17] + 0x198u); G[4] = G[17]; call_here(rt, ctx, 0x089CFCCCu); G[3] = 2u; put(rt, G[17] + 0x1C0u, G[3]); put(rt, G[17] + 0x1A0u, G[2]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089DA4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[2] = load8(G[4] + 0x8Cu); G[17] = G[4]; if (G[2] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1C8u); G[16] = word(rt, G[3] + 0x1D0u); call_here(rt, ctx, G[2]); G[2] -= G[18]; G[4] = G[17]; G[5] = G[2] - 1u; call_here(rt, ctx, G[16]); ctx.fpr[0] = float_at(rt, G[2] + 0xCu); } else { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1D0u); call_here(rt, ctx, G[2]); ctx.fpr[0] = float_at(rt, G[2] + 0x10u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_089F61A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A0FDD8u); G[2] = 0x08CC7E40u; G[3] = 0x08CC800Cu; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[1] = 0x40000000u; ctx.fpr[1] = std::bit_cast<float>(G[1]); put(rt, G[16], G[2]); put(rt, G[16] + 0x38u, G[3]); put_float(rt, G[16] + 0x80u, ctx.fpr[0]); put_float(rt, G[16] + 0x88u, ctx.fpr[1]); put(rt, G[16] + 0x9Cu, 0u); put(rt, G[16] + 0x78u, 0u); put(rt, G[16] + 0x7Cu, 0u); put(rt, G[16] + 0x84u, 0u); put(rt, G[16] + 0x8Cu, 0u); put(rt, G[16] + 0x90u, 0u); put(rt, G[16] + 0x94u, 0u); store8(G[16] + 0x98u, 0u); store8(G[16] + 0x99u, 0u); } G[16] = s0; }
void func_08A1ED00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); if (static_cast<std::int32_t>(G[5]) > 0) { G[16] = word(rt, G[6]); G[4] = 0x08CAA480u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); target = G[2] != 0u ? 0x08A4CD0Cu : 0x08A4CD14u; } } G[16] = s0; if (target != 0u) jump(ctx, target); }
void func_08A3DBDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD13B0u; G[16] = G[4]; G[17] = G[4] + 0x14u; G[4] = word(rt, G[4] + 0x14u); put(rt, G[16], G[2]); call_here(rt, ctx, 0x08A3D718u); G[3] = word(rt, G[17] + 4u); G[4] = G[3]; if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3]); G[5] = word(rt, G[16] + 0x14u); call_here(rt, ctx, G[2]); put(rt, G[17] + 4u, 0u); put(rt, G[16] + 0x14u, 0u); } G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x1Cu; G[6] = 4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08A41414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = frame.at(4u); G[4] = G[17]; G[16] = G[5]; G[5] = G[7]; call_here(rt, ctx, 0x08A40E90u); G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x08A40CF8u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CADBCCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A413B0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A43180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB88A8u; put(rt, G[16], G[2]); G[2] = 0x08D74FD0u; G[3] = G[16] + 0x14u; G[4] = G[16] + 0x2Cu; G[5] = G[16] + 0x18u; G[6] = G[16] + 0x30u; G[2] += 0xCu; put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 0x10u, G[5]); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 0xCu, G[5]); put(rt, G[4] + 0x10u, G[6]); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, G[6]); put(rt, G[16] + 0x44u, G[2]); } G[16] = s0; }
void func_08A56F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[6]; G[17] = G[4]; G[4] = 0x08CAF778u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0x5Cu); G[4] = frame.at(0u); call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[18]; call_here(rt, ctx, 0x08A565C0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A6B4A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD2338u; G[17] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08A6AFBCu); G[3] = word(rt, G[17] + 0x1Cu); G[2] = 0x08D74FD0u; G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[17]; call_here(rt, ctx, 0x08A72308u); } G[16] = s0; G[17] = s1; }
void func_08A78F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB0E38u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A83160u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A79214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB0E38u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = word(rt, G[2] + 0x58u); G[4] = G[16]; call_here(rt, ctx, G[5]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A8B098u); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; }
void func_08A7C550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); G[17] = G[6]; G[2] = word(rt, G[5]); G[16] = G[5]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, G[16]); G[4] = 0x08CB1358u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[6] = G[17]; G[2] = word(rt, G[3] + 0x20u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = s0; G[17] = s1; }
void func_08A7CCB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = 0x08CB1368u; G[16] = word(rt, G[5]); G[17] = G[5]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A7BCC0u); G[5] = G[2]; G[6] = G[17]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A91A38u); G[4] = G[18]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A7C5C8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A81EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 0x14u; G[16] = G[4]; G[18] = G[16] + 0x30u; G[5] = word(rt, G[17] + 8u); G[4] = G[17]; G[16] += 0x18u; call_here(rt, ctx, 0x08C28648u); G[4] = frame.at(0u); put(rt, G[17] + 0x10u, G[16]); put(rt, G[17] + 0x14u, 0u); put(rt, G[17] + 0xCu, G[16]); put(rt, G[17] + 8u, 0u); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A853D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4] + 0x14u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); G[17] = G[5]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x14u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[25] = word(rt, G[3] + 0xC0u); target = G[25]; } } G[16] = s0; G[17] = s1; if (target != 0u) jump(ctx, target); }
void func_08A8B108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB1BE8u; G[16] = word(rt, G[6]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = word(rt, G[2] + 0x58u); G[4] = G[16]; call_here(rt, ctx, G[5]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A8B098u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A93500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x08CB204Cu; G[2] = word(rt, G[5] + 4u); G[16] = word(rt, G[2] + 0x30u); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16] + 0x28u; G[4] = G[17]; call_here(rt, ctx, 0x08B88F44u); G[2] = word(rt, G[17]); G[5] = 0x08CB2074u; G[3] = word(rt, G[2] - 0xCu); G[4] = G[17]; G[6] = 9u; if (G[3] == 0u) call_here(rt, ctx, 0x08B8A4C0u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08A9BED4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 0x4Cu; G[16] = G[4]; G[4] = G[17]; G[19] = G[6]; G[18] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[4] = 0x08CB2570u; G[16] = word(rt, G[16] + 0x38u); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; G[6] = G[19]; call_here(rt, ctx, 0x08A7CBA8u); G[4] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B3457Cu); }
void func_08AA2244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4]; G[4] = 0x08CB2B50u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = word(rt, G[2] + 0x58u); G[4] = G[16]; call_here(rt, ctx, G[5]); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A8B098u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08AC2964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[16] = G[4]; put(rt, G[5], G[6]); call_here(rt, ctx, 0x08AC287Cu); G[3] = G[2]; G[2] = 1u; G[4] = G[16]; if (G[3] == G[2]) { call_here(rt, ctx, 0x08AC28B8u); G[5] = 1u; put(rt, G[17], G[2]); } else { G[2] = 2u; G[5] = 0u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08AC28D0u); put(rt, G[17], G[2]); G[5] = 1u; } } G[2] = G[5]; } G[16] = s0; G[17] = s1; }
void func_08AC31B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[5] = 0u; G[16] = G[4]; G[6] = 0u; G[3] = 1u; G[2] = load8(G[4] + 4u); G[4] = frame.at(0u); if (G[2] == 0u) { store8(G[16] + 5u, 0u); G[2] = 0x08AC3DD4u; put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), G[16]); call_here(rt, ctx, 0x08B1B96Cu); G[4] = G[16] + 0xC94u; put(rt, G[16], G[2]); G[3] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08AC3030u); G[2] = 1u; store8(G[16] + 4u, G[2]); G[3] = 1u; } } G[2] = G[3]; } G[16] = s0; }
void func_08AC5014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 8u; G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); ctx.fpr[0] = 0.0f; ctx.fpr[1] = float_at(rt, G[17] + 0x25ACu); G[4] = G[16]; if (ctx.fpr[1] == ctx.fpr[0]) store8(G[17] + 0x25B2u, 0u); G[16] = word(rt, G[17] + 0x25B8u); put(rt, G[17] + 0x25B8u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[16]; tail = G[16] != 0u; } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B1B99Cu); }
void func_08AC7F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 8u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); G[2] = 1u; store8(G[16] + 0x566u, G[2]); G[2] = 0x08AC8484u; G[5] = 1u; G[4] = 6u; put(rt, frame.at(4u), G[16]); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AC79B0u); put(rt, G[16] + 0x56Cu, G[2]); G[4] = G[17]; call_here(rt, ctx, 0x08B34B24u); } G[16] = s0; G[17] = s1; }
void func_08ACADA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 0x80000000u; G[4] = G[5]; G[3] = 0x80001000u; G[16] = G[6]; if (G[5] != 0u) { G[5] = 0x40000001u; if (G[16] != 0u) { G[6] = 0u; call_here(rt, ctx, 0x08C7A870u); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) < 0) tail = true; else { put(rt, G[16], G[2]); call_here(rt, ctx, 0x08C7AFE8u); G[4] = G[2]; G[3] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) tail = true; } } } if (!tail) G[2] = G[3]; } G[16] = s0; if (tail) jump(ctx, 0x08ACA144u); }
void func_08ACCCB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; G[3] = 0x800u; G[5] = 0u; G[16] = G[4]; store8(G[4] + 0x305u, G[2]); G[2] = 0x08ACE964u; store8(G[4] + 0x306u, 0u); G[4] = 3u; store8(G[16] + 0x304u, 0u); put(rt, frame.at(4u), G[3]); put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[6] = G[2]; G[4] = frame.at(8u); G[5] = frame.at(0u); G[7] = 0u; call_here(rt, ctx, 0x08B1B92Cu); put(rt, G[16] + 0x300u, G[2]); } G[16] = s0; }
void func_08ACD55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[3] = G[5] << 2; G[2] = G[5] << 6; G[2] -= G[3]; G[2] -= G[5]; G[2] <<= 2; G[3] = G[2] + G[4]; G[18] = G[3]; G[17] = G[3] + 0x334u; G[5] = G[17]; G[16] = G[4]; G[3] = load8(G[3] + 0x41Du); if (G[3] != 0u) { call_here(rt, ctx, 0x08ACD4F0u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08ACD404u); store8(G[18] + 0x41Du, 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08ACD7F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] + 0x80u; G[4] = G[17]; G[16] = G[5]; G[18] = word(rt, G[5] + 0xD0u); put(rt, G[5] + 0xD0u, 0u); if (G[18] != 0u) { call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; store8(G[16] + 0xD4u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[18]; tail = true; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08B1B99Cu); }
void func_08AD3A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = word(rt, G[4] + 0x18u); G[19] = G[5]; bool found = false; if (G[18] != 0u) { G[16] = word(rt, G[4] + 0x1Cu); G[17] = 0u; G[2] = word(rt, G[16]); for (;;) { G[5] = G[19]; G[16] += 4u; G[4] = word(rt, G[2]); call_here(rt, ctx, 0x08B35FD8u); if (G[2] == 0u) { found = true; break; } G[17] += 1u; if (G[17] == G[18]) break; G[2] = word(rt, G[16]); } } G[2] = found ? G[17] : 0xFFFFFFFFu; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08ADE52C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put(rt, frame.at(0x10u), bits_of(f20)); ctx.fpr[20] = ctx.fpr[12]; G[4] = sext(load16(G[4] + 0xEAu), 16u); call_here(rt, ctx, 0x08AE0968u); G[3] = sext(load8(G[16] + 0xE8u), 8u); G[4] = G[2] % 4u / 2u * 2u; if (G[3] != 0u) { G[3] = 1u; if (G[4] != 0u) { ctx.fpr[0] = float_at(rt, G[16] + 0xFCu); store8(G[16] + 0xE8u, 0u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[20]; put_float(rt, G[16] + 0xFCu, ctx.fpr[0]); } else G[3] = 0u; } else { G[3] = 1u; if (G[4] != 0u) G[3] = 0u; else { G[2] = 1u; store8(G[16] + 0xE8u, G[2]); } } G[2] = G[3]; } G[16] = s0; ctx.fpr[20] = f20; }
void func_08AEA204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[18]); G[7] = 0x08C91D1Cu; G[18] = G[6]; G[17] = G[5]; G[6] = 0x20u; G[5] = frame.at(0u); call_here(rt, ctx, 0x08AEA178u); G[16] = G[2]; if (G[17] != 0u) { G[2] = sext(load8(frame.at(0u)), 8u); G[4] = frame.at(0u); G[6] = G[18]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B77BFCu); G[6] = G[2]; } put(rt, G[17], G[6]); } G[2] = G[16]; G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); } }
void func_08B05370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[2] = (word(rt, G[4] + 0x1FCu) >> 25) % 0x20u; G[17] = G[4]; if (static_cast<std::int32_t>(G[2]) > 0) { G[18] = 0u; G[16] = G[4] + 0x1C0u; do { G[4] = load8(G[16]); G[5] = load8(G[16] + 1u); G[18] += 1u; G[4] <<= 4; G[5] <<= 4; G[4] = G[17] + G[4]; G[5] = G[17] + G[5]; call_here(rt, ctx, 0x08B556C0u); G[2] = (word(rt, G[17] + 0x1FCu) >> 25) % 0x20u; G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] += 2u; } while (G[2] != 0u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B0A910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4] + 0x48u; G[18] = G[5]; G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; if (G[18] != 0u) { G[2] = word(rt, G[17]); G[6] = word(rt, G[17] + 4u); G[3] = (G[2] + 1u) % 0x10u; G[2] <<= 2; G[5] = (G[3] + 1u) % 0x10u; G[2] += G[17]; put(rt, G[2] + 8u, G[18]); put(rt, G[17], G[3]); if (G[3] == G[6]) put(rt, G[17] + 4u, G[5]); } } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08B3457Cu); }
void func_08B119F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD7CA8u; G[17] = 0x08CD7C98u; G[16] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08B10F7Cu); G[4] = G[16] + 0x64u; call_here(rt, ctx, 0x08B28BE0u); put(rt, G[16] + 0x58u, G[17]); G[4] = G[16] + 0x58u; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x4Cu, G[17]); G[4] = G[16] + 0x4Cu; call_here(rt, ctx, 0x08B1FD74u); put(rt, G[16] + 0x40u, G[17]); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08B1297C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = word(rt, G[5] + 0x10u); G[2] = word(rt, G[16]); G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08B33A24u); G[2] = word(rt, G[16] + 0x10u); G[3] = word(rt, G[16] + 4u); if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = word(rt, G[17] + 0x254u); G[3] += G[2]; put(rt, G[16] + 4u, G[3]); } G[6] = word(rt, G[16] + 8u); G[4] = G[17]; G[5] = G[17] + 0xACu; G[6] = G[3] + G[6]; call_here(rt, ctx, 0x08B11F8Cu); G[3] = 0xFFFFFFFFu; put(rt, G[16] + 0x10u, G[3]); put(rt, G[16], G[2]); } G[16] = s0; G[17] = s1; }
void func_08B158BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[2] = 0u; G[17] = G[4]; G[16] = G[5]; if (G[5] != 0u) { G[18] = 0x08D50900u; G[3] = G[16] < 0x801u ? 1u : 0u; for (;;) { G[2] = 0x800u; if (G[3] != 0u) G[2] = G[16]; G[4] = G[17]; G[5] = frame.at(0u); G[16] -= G[2]; put(rt, frame.at(0u), G[18]); put(rt, frame.at(4u), G[2]); call_here(rt, ctx, 0x08B0C650u); if (static_cast<std::int32_t>(G[2]) < 0) break; G[3] = G[16] < 0x801u ? 1u : 0u; if (G[16] == 0u) break; } } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B18CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xCu); G[6] = G[4]; G[5] = word(rt, G[4] + 0x14u); G[2] = G[3] + 4u; if (G[3] == 0u) G[2] = 0u; put(rt, G[4] + 0x10u, G[3]); G[7] = 1u; G[4] = word(rt, G[2]); put(rt, G[6] + 0xCu, G[4]); if (G[4] == G[5]) { G[3] = word(rt, G[6]); G[4] = word(rt, G[6] + 8u); if (G[3] == G[4]) { G[7] = 0u; put(rt, G[6] + 8u, 0u); } else { G[2] = word(rt, G[3] + 0x30u); while (G[4] != G[2]) { G[3] = G[2]; G[2] = word(rt, G[3] + 0x30u); } put(rt, G[6] + 8u, G[3]); G[7] = 1u; G[2] = word(rt, G[3] + 0xCu); put(rt, G[6] + 0x10u, 0u); put(rt, G[6] + 0xCu, G[2]); G[3] = word(rt, G[3] + 0x10u); put(rt, G[6] + 0x14u, 0u); put(rt, G[6] + 0x18u, G[3]); } } G[2] = G[7]; }
void func_08B1E048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = 0u; G[18] = G[4]; G[16] = word(rt, 0x08D773E4u); if (G[16] != 0u) { G[17] = word(rt, G[16] + 4u); for (;;) { G[5] = G[18]; G[2] = word(rt, G[17] + 0xCu); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35FD8u); if (G[2] == 0u) G[19] = G[17]; } G[16] = word(rt, G[16]); if (G[16] == 0u) break; G[17] = word(rt, G[16] + 4u); } } G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B1F4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4] + 8u; G[18] = G[4]; G[4] = G[16]; G[19] = G[6]; G[17] = G[5]; call_here(rt, ctx, 0x08B34D40u); G[4] = G[16]; G[2] = word(rt, G[18]); G[16] = 1u; if (G[2] != G[17]) G[16] = 0u; else { G[2] = word(rt, G[18] + 4u); if (G[2] != G[19]) G[16] = 0u; } call_here(rt, ctx, 0x08B34D80u); G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B38C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDA928u; put(rt, G[4], G[2]); store16(G[4] + 8u, G[5]); G[8] = 0x08CDA908u; put(rt, G[4] + 4u, 0u); G[3] = 0x08CDA8F8u; G[2] = 0x08CDA8E8u; put(rt, G[4] + 0x10u, 0u); G[5] = word(rt, G[6] + 4u); put(rt, G[4] + 0xCu, G[2]); put(rt, G[4] + 0x14u, G[3]); put(rt, G[4] + 0x18u, G[5]); if (G[5] != 0u) { G[2] = word(rt, G[5] + 4u) + 1u; put(rt, G[5] + 4u, G[2]); } G[7] = word(rt, G[7] + 4u); put(rt, G[4] + 0x1Cu, G[3]); put(rt, G[4] + 0x14u, G[8]); put(rt, G[4] + 0x20u, G[7]); if (G[7] != 0u) { G[2] = word(rt, G[7] + 4u) + 1u; put(rt, G[7] + 4u, G[2]); } put(rt, G[4] + 0x1Cu, G[8]); }
void func_08B406A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[7] = 0u; G[3] = word(rt, G[4] + 0xB0u); G[16] = G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4] + 0xC0u); G[6] = 1u; G[4] = G[3]; if (static_cast<std::int32_t>(G[2]) < 0) { store8(G[16] + 0xB8u, 0u); G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x44u); call_here(rt, ctx, G[2]); G[7] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) { G[2] = 1u; store8(G[16] + 0xB8u, G[2]); } else { G[7] = 1u; put(rt, G[16] + 0xC0u, G[2]); store8(G[16] + 0xBAu, 0u); store8(G[16] + 0xB6u, 0u); } } } G[2] = G[7]; } G[16] = s0; }
void func_08B4115C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6] % 0x100u; G[17] = G[4]; G[4] = G[5]; G[16] = word(rt, G[5]); call_here(rt, ctx, 0x08B08A58u); G[3] = word(rt, G[17]); store8(G[17] + 0x936u, G[2]); G[4] = G[16]; G[5] = G[18]; G[16] = word(rt, G[3] + 0x34u); call_here(rt, ctx, 0x08B08998u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, G[16]); G[3] = 0u; if (G[2] != 0u) { store8(G[17] + 0xACu, G[18]); G[3] = 1u; } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B4FBBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B4FA78u); G[4] = G[16]; call_here(rt, ctx, 0x08B4A744u); ctx.fpr[1] = 0.0f; G[4] = G[16]; const bool z = ctx.fpr[0] == ctx.fpr[1]; ctx.fpr[2] = __builtin_sqrtf(ctx.fpr[0]); if (!z) { G[1] = 0x3F800000u; ctx.fpr[12] = std::bit_cast<float>(G[1]); ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[2]; tail = true; } else { G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0xCu, ctx.fpr[0]); put_float(rt, G[16] + 8u, ctx.fpr[1]); put_float(rt, G[16], ctx.fpr[1]); put_float(rt, G[16] + 4u, ctx.fpr[1]); } } G[16] = s0; if (tail) jump(ctx, 0x08B4A6A0u); }
void func_08B61A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x50u); G[16] = G[4]; call_here(rt, ctx, 0x08B61664u); G[4] = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B7E884u); const bool fail = G[2] != 0u; G[2] = word(rt, frame.at(4u)); bool ok = false; if (!fail) { G[3] = 0x8000u; G[2] = G[2] % 0x10000u / 0x1000u * 0x1000u; G[4] = G[16]; ok = G[2] == G[3]; } if (ok) { G[16] = word(rt, frame.at(0x10u)); call_here(rt, ctx, 0x08B61664u); G[4] = G[2]; G[5] = 0u; G[6] = 1u; call_here(rt, ctx, 0x08B7E88Cu); G[2] = G[16] - G[2]; } else G[2] = 0u; } G[16] = s0; }
void func_08B628BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[4]; G[18] = G[7]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B62884u); G[5] = G[16]; G[6] = G[17]; G[7] = G[18]; G[3] = 1u; if (G[2] == 0u) { G[4] = word(rt, G[19] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x18u); call_here(rt, ctx, G[2]); G[3] = G[2]; } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08B783B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5] << 3; G[16] = G[5] << 5; G[16] -= G[2]; G[16] -= G[5]; G[16] <<= 2; G[18] = G[5]; G[5] = G[16] + 0xCu; call_here(rt, ctx, 0x08B35DACu); G[17] = G[2]; G[2] += 0xCu; G[4] = G[2]; G[6] = G[16]; G[5] = 0u; if (G[17] != 0u) { put(rt, G[17] + 4u, G[18]); put(rt, G[17], 0u); put(rt, G[17] + 8u, G[2]); call_here(rt, ctx, 0x08B36018u); } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B80874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[8]; if (G[6] == 0u) { G[6] = 0x08C97584u; G[5] = 0u; G[7] = 1u; } call_here(rt, ctx, 0x08B80948u); G[3] = G[2]; G[2] = 0xFFFFFFFFu; G[4] = G[3]; if (G[3] == G[2]) { G[2] = 0x8Au; put(rt, G[16], 0u); G[4] = 0xFFFFFFFFu; put(rt, G[17], G[2]); } G[2] = G[4]; } G[16] = s0; G[17] = s1; }
void func_08BA6BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x20u; G[16] = G[5]; call_here(rt, ctx, 0x08AF1060u); G[8] = G[2] + 0xCu; if (G[2] != 0u) { G[4] = word(rt, G[17]); G[3] = 0x08CC1748u; G[6] = word(rt, G[16]); put(rt, G[2], G[3]); put(rt, G[2] + 4u, G[4]); put(rt, G[2] + 8u, G[4]); put(rt, G[2] + 0xCu, G[4]); G[3] = word(rt, G[4] + 0x40u); put(rt, G[8] + 4u, G[3]); G[3] = 1u; G[5] = word(rt, G[4] + 0x44u); put(rt, G[8] + 0xCu, G[3]); put(rt, G[8] + 8u, G[5]); put(rt, G[2] + 0x1Cu, G[6]); } } G[16] = s0; G[17] = s1; }
void func_08BBD954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[17] = G[6]; if (G[5] != G[4]) { G[16] = G[4]; G[18] = G[4]; do { G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B957D8u); G[3] = word(rt, G[17] + 4u); G[4] = word(rt, G[17] + 8u); G[2] = word(rt, G[17] + 0xCu); put(rt, G[16] + 4u, G[3]); put(rt, G[16] + 0xCu, G[2]); put(rt, G[16] + 8u, G[4]); G[16] = G[18] + 0x10u; G[18] = G[16]; } while (G[19] != G[16]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08BBEF50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; G[16] = G[6]; if (G[4] != G[5]) { do { G[4] = G[16]; G[5] = G[17]; if (G[16] != 0u) { call_here(rt, ctx, 0x08B9432Cu); G[2] = word(rt, G[17] + 4u); G[3] = word(rt, G[17] + 8u); G[4] = word(rt, G[17] + 0xCu); put(rt, G[16] + 4u, G[2]); put(rt, G[16] + 8u, G[3]); put(rt, G[16] + 0xCu, G[4]); } G[17] += 0x10u; G[16] += 0x10u; } while (G[18] != G[17]); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08BE6B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[6]; if (G[4] != G[5]) { G[6] = 0x08CCE6E0u; for (;;) { if (G[7] != 0u) { ctx.fpr[0] = float_at(rt, G[4] + 0xCu); ctx.fpr[1] = float_at(rt, G[4] + 0x10u); G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4] + 8u); ctx.fpr[2] = float_at(rt, G[4] + 0x14u); ctx.fpr[3] = float_at(rt, G[4] + 0x18u); ctx.fpr[4] = float_at(rt, G[4] + 0x1Cu); put(rt, G[7] + 4u, G[2]); put(rt, G[7] + 8u, G[3]); put_float(rt, G[7] + 0xCu, ctx.fpr[0]); put_float(rt, G[7] + 0x10u, ctx.fpr[1]); put_float(rt, G[7] + 0x14u, ctx.fpr[2]); put_float(rt, G[7] + 0x18u, ctx.fpr[3]); put_float(rt, G[7] + 0x1Cu, ctx.fpr[4]); ctx.fpr[0] = float_at(rt, G[4] + 0x20u); ctx.fpr[1] = float_at(rt, G[4] + 0x24u); put(rt, G[7], G[6]); put_float(rt, G[7] + 0x20u, ctx.fpr[0]); put_float(rt, G[7] + 0x24u, ctx.fpr[1]); } G[4] += 0x28u; G[7] += 0x28u; if (G[5] == G[4]) break; } } G[2] = G[7]; }
void func_08C0D880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[18] = G[4]; G[16] = G[6]; if (G[4] != G[5]) { G[17] = G[4]; do { G[4] = G[16]; G[5] = G[18]; G[17] += 0x20u; if (G[16] != 0u) { call_here(rt, ctx, 0x0896418Cu); G[2] = word(rt, G[18] + 0x10u); put(rt, G[16] + 0x10u, G[2]); } G[18] = G[17]; G[16] += 0x20u; } while (G[19] != G[17]); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C0D968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5]; G[18] = G[4]; G[16] = G[6]; if (G[4] != G[5]) { G[17] = G[4]; do { G[4] = G[16]; G[5] = G[18]; G[17] += 0x20u; if (G[16] != 0u) { call_here(rt, ctx, 0x0896418Cu); G[2] = word(rt, G[18] + 0x10u); put(rt, G[16] + 0x10u, G[2]); } G[18] = G[17]; G[16] += 0x20u; } while (G[19] != G[17]); } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C201AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[4] = G[5]; G[17] = G[5]; call_here(rt, ctx, 0x08A6158Cu); G[4] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x08A6158Cu); G[16] = word(rt, G[2]); G[4] = 0x08CB0284u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xC4u); G[5] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08A61584u); }
void func_08C73E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[5]; G[18] = word(rt, G[4]); G[4] = G[5]; G[16] = word(rt, G[18] - 0xCu); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B35F60u); G[17] = G[2]; G[3] = G[2] < G[16] ? 1u : 0u; G[2] = frame.at(4u); put(rt, frame.at(4u), G[17]); if (G[3] == 0u) G[2] = frame.at(0u); G[4] = G[18]; G[6] = word(rt, G[2]); G[5] = G[19]; G[16] -= G[17]; call_here(rt, ctx, 0x08B36170u); if (G[2] == 0u) G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08C78960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[6]; G[16] = G[4]; G[4] += 0x24u; call_here(rt, ctx, 0x08B61534u); if (G[2] == 0u) { if (G[18] != 0u) { if (static_cast<std::int32_t>(G[17]) > 0) { put(rt, G[16] + 0x78u, G[18]); put(rt, G[16] + 0x7Cu, G[17]); } } else if (G[17] == 0u) { G[2] = 1u; put(rt, G[16] + 0x7Cu, G[2]); } } G[2] = G[16]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08C79ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDC374u; G[17] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CDC388u; put(rt, G[17] + 0xA8u, G[2]); G[4] += 4u; call_here(rt, ctx, 0x08C786B4u); G[2] = 0x08CDC358u; G[4] = word(rt, G[2] + 4u); G[5] = word(rt, G[2] + 8u); G[2] = 0x08CDB088u; G[3] = word(rt, G[4] - 0xCu); put(rt, G[17], G[4]); G[3] = G[17] + G[3]; G[4] = G[17] + 0xA8u; put(rt, G[3], G[5]); put(rt, G[17] + 0xA8u, G[2]); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B6543Cu); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_195
} // namespace gt::game

#define HOT195(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_195::name, gt::ReturnKind::kind)
HOT195(0x08B7A2C8u, func_08B7A2C8, Word);
HOT195(0x08B80438u, func_08B80438, Word);
HOT195(0x08B807FCu, func_08B807FC, Word);
HOT195(0x08B82344u, func_08B82344, Word);
HOT195(0x08A66604u, func_08A66604, Word);
HOT195(0x08A6732Cu, func_08A6732C, Word);
HOT195(0x08BC39C8u, func_08BC39C8, Word);
HOT195(0x08BC3B48u, func_08BC3B48, Word);
HOT195(0x08BBD66Cu, func_08BBD66C, Word);
HOT195(0x08BCF8E0u, func_08BCF8E0, Word);
HOT195(0x08BD60C8u, func_08BD60C8, Word);
HOT195(0x089BF1B0u, func_089BF1B0, DoubleWord);
HOT195(0x089BF228u, func_089BF228, DoubleWord);
HOT195(0x089BF2A0u, func_089BF2A0, DoubleWord);
HOT195(0x08C39BB4u, func_08C39BB4, Word);
HOT195(0x08C39D94u, func_08C39D94, Word);
HOT195(0x08AA2330u, func_08AA2330, Word);
HOT195(0x08AA23A8u, func_08AA23A8, Word);
// HOT195(0x08A5DF58u, func_08A5DF58, Word);  // no comparable synthetic case yet
HOT195(0x08A831D0u, func_08A831D0, Word);
// HOT195(0x08A9893Cu, func_08A9893C, Word);  // no comparable synthetic case yet
HOT195(0x08B87970u, func_08B87970, Word);
HOT195(0x08A716A4u, func_08A716A4, Word);
HOT195(0x08A7171Cu, func_08A7171C, Word);
HOT195(0x08A642C8u, func_08A642C8, Word);
HOT195(0x08A64340u, func_08A64340, Word);
// HOT195(0x08A56630u, func_08A56630, Word);  // no comparable synthetic case yet
HOT195(0x08A56784u, func_08A56784, Word);
HOT195(0x08A2F5B0u, func_08A2F5B0, Word);
HOT195(0x08A2F628u, func_08A2F628, Word);
HOT195(0x08921CA0u, func_08921CA0, Word);
HOT195(0x08921DACu, func_08921DAC, Word);
HOT195(0x088753A0u, func_088753A0, Word);
HOT195(0x088758FCu, func_088758FC, Word);
HOT195(0x0885D950u, func_0885D950, Word);
HOT195(0x08865F7Cu, func_08865F7C, Word);
// HOT195(0x0886DF90u, func_0886DF90, Word);  // no comparable synthetic case yet
// HOT195(0x08882928u, func_08882928, Word);  // no comparable synthetic case yet
HOT195(0x08886E04u, func_08886E04, Word);
// HOT195(0x088CFA9Cu, func_088CFA9C, Float);  // no comparable synthetic case yet
HOT195(0x088D75E0u, func_088D75E0, Word);
HOT195(0x088DCF58u, func_088DCF58, Word);
HOT195(0x088E6648u, func_088E6648, Word);
HOT195(0x088E6B0Cu, func_088E6B0C, Word);
// HOT195(0x088E798Cu, func_088E798C, Word);  // no comparable synthetic case yet
HOT195(0x088F1AE8u, func_088F1AE8, Word);
HOT195(0x088F2BE8u, func_088F2BE8, Word);
HOT195(0x088FB8F8u, func_088FB8F8, Word);
HOT195(0x08910B8Cu, func_08910B8C, Word);
HOT195(0x0892BA30u, func_0892BA30, Word);
HOT195(0x08940218u, func_08940218, Word);
HOT195(0x08949128u, func_08949128, Word);
HOT195(0x0895C0C8u, func_0895C0C8, Word);
// HOT195(0x089B0044u, func_089B0044, Word);  // no comparable synthetic case yet
HOT195(0x089B6244u, func_089B6244, Word);
HOT195(0x089B7C80u, func_089B7C80, Word);
HOT195(0x089C1E34u, func_089C1E34, Word);
// HOT195(0x089CFD28u, func_089CFD28, Word);  // no comparable synthetic case yet
// HOT195(0x089DA4E0u, func_089DA4E0, Float);  // no comparable synthetic case yet
HOT195(0x089F61A0u, func_089F61A0, Word);
HOT195(0x08A1ED00u, func_08A1ED00, Word);
// HOT195(0x08A3DBDCu, func_08A3DBDC, Word);  // no comparable synthetic case yet
// HOT195(0x08A41414u, func_08A41414, Word);  // no comparable synthetic case yet
HOT195(0x08A43180u, func_08A43180, Word);
// HOT195(0x08A56F08u, func_08A56F08, Word);  // no comparable synthetic case yet
HOT195(0x08A6B4A0u, func_08A6B4A0, Word);
// HOT195(0x08A78F54u, func_08A78F54, Word);  // no comparable synthetic case yet
HOT195(0x08A79214u, func_08A79214, Word);
HOT195(0x08A7C550u, func_08A7C550, Word);
HOT195(0x08A7CCB8u, func_08A7CCB8, Word);
HOT195(0x08A81EB4u, func_08A81EB4, Word);
HOT195(0x08A853D0u, func_08A853D0, Word);
// HOT195(0x08A8B108u, func_08A8B108, Word);  // no comparable synthetic case yet
// HOT195(0x08A93500u, func_08A93500, Word);  // no comparable synthetic case yet
// HOT195(0x08A9BED4u, func_08A9BED4, Word);  // no comparable synthetic case yet
HOT195(0x08AA2244u, func_08AA2244, Word);
HOT195(0x08AC2964u, func_08AC2964, Word);
HOT195(0x08AC31B4u, func_08AC31B4, Word);
HOT195(0x08AC5014u, func_08AC5014, Word);
// HOT195(0x08AC7F94u, func_08AC7F94, Word);  // no comparable synthetic case yet
HOT195(0x08ACADA8u, func_08ACADA8, Word);
HOT195(0x08ACCCB4u, func_08ACCCB4, Word);
HOT195(0x08ACD55Cu, func_08ACD55C, Word);
HOT195(0x08ACD7F4u, func_08ACD7F4, Word);
HOT195(0x08AD3A14u, func_08AD3A14, Word);
HOT195(0x08ADE52Cu, func_08ADE52C, Word);
HOT195(0x08AEA204u, func_08AEA204, Word);
HOT195(0x08B05370u, func_08B05370, Word);
HOT195(0x08B0A910u, func_08B0A910, Word);
HOT195(0x08B119F8u, func_08B119F8, Word);
// HOT195(0x08B1297Cu, func_08B1297C, Word);  // no comparable synthetic case yet
HOT195(0x08B158BCu, func_08B158BC, Word);
HOT195(0x08B18CECu, func_08B18CEC, Word);
HOT195(0x08B1E048u, func_08B1E048, Word);
HOT195(0x08B1F4F4u, func_08B1F4F4, Word);
// HOT195(0x08B38C78u, func_08B38C78, Word);  // no comparable synthetic case yet
HOT195(0x08B406A0u, func_08B406A0, Word);
HOT195(0x08B4115Cu, func_08B4115C, Word);
HOT195(0x08B4FBBCu, func_08B4FBBC, Word);
HOT195(0x08B61A08u, func_08B61A08, Word);
// HOT195(0x08B628BCu, func_08B628BC, Word);  // no comparable synthetic case yet
HOT195(0x08B783B8u, func_08B783B8, Word);
HOT195(0x08B80874u, func_08B80874, Word);
HOT195(0x08BA6BA4u, func_08BA6BA4, Word);
HOT195(0x08BBD954u, func_08BBD954, Word);
HOT195(0x08BBEF50u, func_08BBEF50, Word);
HOT195(0x08BE6B5Cu, func_08BE6B5C, Word);
HOT195(0x08C0D880u, func_08C0D880, Word);
HOT195(0x08C0D968u, func_08C0D968, Word);
// HOT195(0x08C201ACu, func_08C201AC, Word);  // no comparable synthetic case yet
HOT195(0x08C73E84u, func_08C73E84, Word);
HOT195(0x08C78960u, func_08C78960, Word);
// HOT195(0x08C79ED8u, func_08C79ED8, Word);  // no comparable synthetic case yet
