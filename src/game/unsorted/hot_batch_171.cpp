// Hot batch 171: seven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters, setters, small float helpers, guarded tail calls and calls that set up one argument).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_171 {
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
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// Families of identical shape: each registration keeps its own thin function (and its own frame), the constants differ.
// The byte at `when_one` (a1 == 1) or `otherwise` of the record word(self) points at.
void flag_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 when_one, u32 otherwise) {
    const u32 record = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = byte_at(rt, record + (ctx.gpr[5] == 1u ? when_one : otherwise));
}
// word(self + 0xA8) + index * stride + offset.
void indexed_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 stride, u32 offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0xA8u) + ctx.gpr[5] * stride + offset;
}
// Increment the counter at the start of the 16-byte entry `index` of the array at word(self + table).
void bump_counter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 table) {
    const u32 entry = word(rt, ctx.gpr[4] + table) + ctx.gpr[5] * 16u;
    ctx.gpr[3] = word(rt, entry) + 1u;
    put(rt, entry, ctx.gpr[3]);
    ctx.gpr[2] = entry;
}
// A vtable and three zeroed fields (+4, +8, +0xC); v0 is left holding the vtable.
void construct_empty(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    put(rt, self, vtable);
    for (const u32 offset : {0xCu, 4u, 8u}) put(rt, self + offset, 0u);
    ctx.gpr[2] = vtable;
}
// A vtable, a -1 at +8 and a zero at +4; v0 is the vtable, v1 the -1.
void construct_unbound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    put(rt, self + 8u, 0xFFFFFFFFu);
    put(rt, self, vtable);
    put(rt, self + 4u, 0u);
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = vtable;
}
// 1 when the first word of the object at word(self + 4) plus `addend` is zero.
void first_word_is(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 addend) {
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[3] = object;
    ctx.gpr[2] = (word(rt, object) + addend) == 0u ? 1u : 0u;
}

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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088057B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08B2B330u, 0x10u); jump(ctx, 0x08B1E2A8u); }
void func_0881AC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4] + 0x1C4u; call_with(rt, ctx, 0x0881AB64u, 0x10u); }
void func_0881AC4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4] + 0x34u; call_with(rt, ctx, 0x0881AB64u, 0x10u); }
void func_0885C88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = word(rt, G[4] + 0x30u); G[3] = G[2] - 1u; if (static_cast<std::int32_t>(G[2]) > 0) put(rt, G[4] + 0x30u, G[3]); }
void func_0885D9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (G[5] << 2) + G[4]; G[5] += G[4]; G[3] = 0xFFFFFFFFu; put(rt, G[2] + 0x44u, G[3]); store8(G[5] + 0x40u, 0u); }
void func_0885E5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[5] << 6; G[5] = (G[5] << 4) + G[2] + G[3]; G[2] = word(rt, G[5] + 0x4144u); }
void func_0885F444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[5] = 1u; store8(G[3] + 0x5AAu, 1u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x5ACu); store8(G[3] + 0x5C0u, 1u); }
void func_08869CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); put(rt, G[2], G[5]); G[5] = 0u; G[4] = word(rt, G[4] + 4u); G[6] = word(rt, G[4]); G[4] += 0x4238u; jump(ctx, 0x0886C12Cu); }
void func_08869CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); put(rt, G[2] + 4u, G[5]); G[5] = 1u; G[4] = word(rt, G[4] + 4u); G[6] = word(rt, G[4] + 4u); G[4] += 0x4238u; jump(ctx, 0x0886C12Cu); }
void func_0887CC34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[4] = G[5]; if (G[5] == 0u) return; G[5] = word(rt, G[2] + 8u); jump(ctx, 0x08B9CC90u); }
void func_0888EBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = (((G[5] << 1) + G[6]) << 2) + G[2]; G[2] = word(rt, G[5] + 0x1B0u); }
void func_0888FBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x0888F558u); }
void func_08896070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[3] = 1u; put(rt, G[4] + 0xECu, 1u); G[4] += 0xB4u; store8(G[2] + 0xF0u, 0u); store8(G[2] + 0xF1u, 0u); jump(ctx, 0x08895B80u); }
void func_08899D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, word(rt, G[4]) + 0x58u); G[3] = 0u; if (G[2] != 0u) G[3] = word(rt, G[2] + 0x10u); G[2] = G[3]; }
void func_088A258C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 4u); G[4] = (G[5] << 7) - (G[5] << 2); G[5] <<= 2; G[4] = G[2] + G[4]; G[5] = G[6]; jump(ctx, 0x088A2544u); }
void func_088A3794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x270u; call_with(rt, ctx, 0x088AA6C8u, 0x10u); }
void func_088A4630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x18u); ctx.fpr[0] = 0.0f; if (G[2] != 0u) { G[2] = word(rt, G[2] + 8u); ctx.fpr[0] = float_at(rt, G[2] + 0x520u); } }
void func_088AB560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088AB4A4u, {ctx.gpr[4], ctx.gpr[5], 0u});
}
void func_088AB57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088AB4A4u, {ctx.gpr[4], ctx.gpr[5], 1u});
}
void func_088AB598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x088AB4A4u, {ctx.gpr[4], ctx.gpr[5], 2u});
}
void func_088CEA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (const std::uint32_t at : {0xC4u, 0x114u, 0x118u, 0x11Cu, 0x120u, 0xC0u}) put(rt, G[4] + at, 0u); }
void func_088CF0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for (const std::uint32_t at : {0xD4u, 0xB4u, 0xB8u, 0xBCu, 0xCCu, 0xD0u}) put(rt, G[4] + at, 0u); }
void func_088D1D90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x481907FFu; G[3] = 0xFFFFFFFFu; put(rt, G[4] + 8u, G[3]); put(rt, G[4] + 4u, G[2]); put(rt, G[4], 0u); }
void func_088D4E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    flag_byte(rt, ctx, 0x72u, 0x6Eu);
}
void func_088D4EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    flag_byte(rt, ctx, 0x73u, 0x6Fu);
}
void func_088D4F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    flag_byte(rt, ctx, 0x74u, 0x70u);
}
void func_088D61C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = (G[5] << 1) + G[2]; G[2] = sra(__builtin_bswap32(load16(G[5] + 0x66u)), 16u); }
void func_088D6FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x088E6648u); }
void func_088DB974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x640u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x088E4380u); }
void func_088E7EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x24u); G[2] = 0u; if (G[3] != 0u) G[2] = G[3] + word(rt, G[4] + 0x28u); }
void func_088ECA38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0x11Cu); G[2] = 1u; if (G[3] != 0u) G[2] = load8(G[3] + 0x1Au) != 0u ? 1u : 0u; }
void func_089061D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0u; if (G[4] == 0u) return; G[5] = word(rt, G[4] + 0x68u); G[4] = word(rt, G[5]); jump(ctx, 0x08AF8214u); }
void func_08908F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = G[4]; G[4] = word(rt, G[4] + 8u); G[6] = word(rt, G[5] + 0x10u); put(rt, G[5] + 8u, G[5]); G[4] = G[5] - G[4]; G[5] += 0x14u; jump(ctx, 0x08BD5C88u); }
void func_0891A6A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0xCu); ctx.fpr[0] = float_at(rt, G[4] + 0x10u); put(rt, G[4] + 0xCu, bits_of(ctx.fpr[12])); put(rt, G[4] + 0x10u, bits_of(ctx.fpr[13])); put(rt, G[4] + 0x20u, bits_of(ctx.fpr[0])); put(rt, G[4] + 0x24u, bits_of(ctx.fpr[1])); }
void func_08931B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; G[3] = 0xFFFFFFFFu; put(rt, G[4] + 0x10u, G[2]); put(rt, G[4] + 0x14u, G[3]); put(rt, G[4] + 8u, G[2]); put(rt, G[4] + 0xCu, G[3]); jump(ctx, 0x08931B20u); }
void func_089366FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = (word(rt, G[4] + 8u) << 2) + 0x08CDDE0Cu; G[2] = word(rt, G[3]); }
void func_08937B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    first_word_is(rt, ctx, 0xABACABB9u);
}
void func_08937B44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_unbound(rt, ctx, 0x08CC67C8u);
}
void func_08937DCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    first_word_is(rt, ctx, 0xBBB6ABB9u);
}
void func_08937E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_unbound(rt, ctx, 0x08CC67D8u);
}
void func_089416B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[4] &= 0xFFu; ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[4])); G[1] = 0x42C80000u; ctx.fpr[0] = 100.0f / ctx.fpr[1]; }
void func_0895F158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[4] = 5u; G[3] = load8(G[2] + 0x67Au) ^ 0x14u; G[2] = G[3] != 0u ? 5u : 4u; }
void func_08965B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = 0x34u; G[6] = 4u; if (G[4] == 0u) return; jump(ctx, 0x08A5BC54u); }
void func_08974788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    indexed_slot(rt, ctx, 48u, 0x20u);
}
void func_089747A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    indexed_slot(rt, ctx, 48u, 0x28u);
}
void func_089952D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); jump(ctx, G[2] != 0u ? 0x089948A8u : 0x08994810u); }
void func_089952F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); jump(ctx, G[2] != 0u ? 0x08994574u : 0x08994C00u); }
void func_089961E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[6]); G[5] <<= 3; G[4] += G[5]; put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[6] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); }
void func_089A0B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xA8u); G[2] = G[5] << 5; G[5] = (G[5] << 3) + G[2] + G[3]; ctx.fpr[0] = float_at(rt, G[5] + 0x20u); }
void func_089A0BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xA8u); G[2] = G[5] << 5; G[5] = (G[5] << 3) + G[2] + G[3]; ctx.fpr[0] = float_at(rt, G[5] + 0x24u); }
void func_089B796C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x158u); if (G[2] != 0u) return; jump(ctx, 0x089B7574u); }
void func_089BA6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x44u); put(rt, G[5] + 0x18u, G[2]); G[2] = word(rt, G[4] + 0x44u); if (G[2] != 0u) put(rt, G[2] + 0x14u, G[5]); put(rt, G[4] + 0x44u, G[5]); }
void func_089CD180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); ctx.fpr[0] = float_at(rt, G[4] + (G[2] != 0u ? 0x15Cu : 0x158u)); }
void func_089CD19C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); ctx.fpr[0] = float_at(rt, G[4] + (G[2] != 0u ? 0x158u : 0x15Cu)); }
void func_089CD1F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); ctx.fpr[0] = float_at(rt, G[4] + (G[2] != 0u ? 0x148u : 0x144u)); }
void func_089CD20C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x120u); ctx.fpr[0] = float_at(rt, G[4] + (G[2] != 0u ? 0x144u : 0x148u)); }
void func_089D83F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x8Cu); ctx.fpr[0] = float_at(rt, G[4] + (G[2] == 0u ? 0x84u : 0x88u)); }
void func_089D8410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x8Cu); ctx.fpr[0] = float_at(rt, G[4] + (G[2] == 0u ? 0x88u : 0x84u)); }
void func_089F0B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0xCu; call_with(rt, ctx, 0x08B0B428u, 0x10u); }
void func_08A04FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    indexed_slot(rt, ctx, 96u, 0x20u);
}
void func_08A04FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    indexed_slot(rt, ctx, 96u, 0x40u);
}
void func_08A3412C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08B2E224u); }
void func_08A3A7C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x18u); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08ABCCA0u); }
void func_08A4F750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[6] = load16(G[5] + 2u); G[3] = load16(G[4] + 0x12u); G[2] = G[2] + G[6] * G[3]; }
void func_08A503C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); G[4] = G[2]; if (G[2] == 0u) return; jump(ctx, 0x08A50FCCu); }
void func_08A72EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B71CE0u, {static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(half_at(rt, ctx.gpr[4] + 0x14u))))});
}
void func_08A878A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = float_at(rt, G[4] + 0x14u); call_with(rt, ctx, 0x08B6F4D0u, 0x10u); }
void func_08A87968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = float_at(rt, G[4] + 0x14u); call_with(rt, ctx, 0x08B6F37Cu, 0x10u); }
void func_08A95C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B71CE0u, {static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(byte_at(rt, ctx.gpr[4] + 0x14u))))});
}
void func_08AB9168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 8u) + G[5]; G[2] = word(rt, G[4] + 0xCu) - G[5]; put(rt, G[4] + 0xCu, G[2]); put(rt, G[4] + 8u, G[3]); }
void func_08ABF694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_empty(rt, ctx, 0x08CD6420u);
}
void func_08AC0B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with(rt, ctx, 0x08AC0B50u, 0x10u); jump(ctx, 0x08AC0B5Cu); }
void func_08ADF930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08ADF880u, {ctx.gpr[4], 0u});
}
void func_08ADF9FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08ADF94Cu, {ctx.gpr[4], 0u});
}
void func_08ADFB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08ADFACCu, {ctx.gpr[4], 0u});
}
void func_08ADFC40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = 0u; call_with(rt, ctx, 0x08ADFB98u, 0x10u); }
void func_08AE313C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = sext(load16(G[4] + 0x1Cu), 16u); call_with(rt, ctx, 0x08ADAB0Cu, 0x10u); }
void func_08AE3AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5] + 4u); call_with(rt, ctx, 0x08AE3604u, 0x10u); }
void func_08AF49B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x28u); G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; ctx.fpr[2] = float_at(rt, G[4] + 0x38u); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[2] / ctx.fpr[0]; }
void func_08AF49D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x28u); G[1] = 0x3F800000u; ctx.fpr[1] = 1.0f; ctx.fpr[2] = float_at(rt, G[4] + 0x38u); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[2] / ctx.fpr[0]; }
void func_08B03B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = G[5] << 8; G[5] <<= 4; G[4] = word(rt, G[3]); G[2] = G[4] + (G[2] - G[5]); }
void func_08B0B104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4]) | (load8(G[4] + 1u) << 8) | (load8(G[4] + 2u) << 16) | (load8(G[4] + 3u) << 24); G[3] = 0x8113Bu; G[2] = (G[2] + G[3]) == 0u ? 1u : 0u; }
void func_08B1E97C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; store8(G[4] + 0xDu, 1u); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); store8(G[4] + 0xCu, 0u); }
void func_08B26A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD9630u; put(rt, G[4], G[2]); put(rt, G[4] + 0x10u, 0u); store8(G[4] + 0x28u, 0u); put(rt, G[4] + 0x24u, 0u); }
void func_08B27384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x28u); G[3] = word(rt, G[4] + 0x20u); G[2] = G[3] + (G[2] << 1); put(rt, G[4] + 0x24u, G[2]); put(rt, G[4] + 0x2Cu, G[3]); }
void func_08B2740C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bump_counter(rt, ctx, 0x40u);
}
void func_08B27428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bump_counter(rt, ctx, 0x48u);
}
void func_08B27444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bump_counter(rt, ctx, 0x50u);
}
void func_08B2F44C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x10000u; G[4] += G[2]; G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; put(rt, G[4] - 0x69C8u, G[6]); put(rt, G[4] - 0x69C4u, G[7]); }
void func_08B349A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = 0u; call_with(rt, ctx, 0x08B34890u, 0x10u); }
void func_08B35238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08D694C4u); if (G[4] != 0u) return; G[3] = high_half(0x08D694C0u); put(rt, 0x08D694C4u, 0u); put(rt, 0x08D694C0u, 0u); }
void func_08B37710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_empty(rt, ctx, 0x08CDA8D8u);
}
void func_08B3F964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] += 0x10u; call_with(rt, ctx, 0x08B3F940u, 0x10u); }
void func_08B42100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xCu; if (G[8] != 0xCu) return; G[5] = sext(load16(G[7] + 8u), 16u); jump(ctx, 0x08B41AF4u); }
void func_08B52284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CDAC78u; G[3] = 0x08CDAC68u; put(rt, G[4] + 4u, G[2]); put(rt, G[4], G[3]); }
void func_08B5293C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = G[5] + G[7]; G[8] = G[6] + G[8]; put(rt, G[4] + 0x1Cu, G[8]); put(rt, G[4] + 0x18u, G[7]); put(rt, G[4] + 0x10u, G[5]); put(rt, G[4] + 0x14u, G[6]); }
void func_08B56FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[6] &= 0xFFu; jump(ctx, G[6] == 0u ? 0x08B56E20u : 0x08B56EC0u); }
void func_08B584B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = 0x08D6CE00u; G[2] = word(rt, G[3] + 0x6Cu); put(rt, G[3] + 0x100u, G[4]); G[2] |= 0x800u; put(rt, G[3] + 0x6Cu, G[2]); }
void func_08B6EE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[2] = bits_of(ctx.fpr[12]) & 0x7FFFFFFFu; G[3] = bits_of(ctx.fpr[13]) & 0x80000000u; G[2] |= G[3]; ctx.fpr[0] = std::bit_cast<float>(G[2]); }
void func_08B807B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B80668u); }
void func_08BA5174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); }
void func_08BA53CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5] + 0x30u); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 0x34u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 0x38u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); }
void func_08BAE05C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 8u; call_with(rt, ctx, 0x088A6C54u, 0x10u); }
void func_08BAE294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[5]); put(rt, G[4], bits_of(ctx.fpr[1])); ctx.fpr[0] = float_at(rt, G[5] + 4u); put(rt, G[4] + 4u, bits_of(ctx.fpr[0])); ctx.fpr[1] = float_at(rt, G[5] + 8u); put(rt, G[4] + 8u, bits_of(ctx.fpr[1])); }
void func_08C1B474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_empty(rt, ctx, 0x08CD1E70u);
}
void func_08C76A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x24u; call_with(rt, ctx, 0x08B61534u, 0x10u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_171
} // namespace gt::game

#define HOT171(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_171::name, gt::ReturnKind::kind)
HOT171(0x088057B0u, func_088057B0, Word);
HOT171(0x0881AC30u, func_0881AC30, Word);
HOT171(0x0881AC4Cu, func_0881AC4C, Word);
HOT171(0x0885C88Cu, func_0885C88C, Word);
HOT171(0x0885D9C8u, func_0885D9C8, Word);
HOT171(0x0885E5A0u, func_0885E5A0, Word);
HOT171(0x0885F444u, func_0885F444, Word);
// HOT171(0x08869CCCu, func_08869CCC, Word);  // no comparable synthetic case yet
// HOT171(0x08869CE8u, func_08869CE8, Word);  // no comparable synthetic case yet
HOT171(0x0887CC34u, func_0887CC34, Word);
HOT171(0x0888EBF4u, func_0888EBF4, Word);
// HOT171(0x0888FBF8u, func_0888FBF8, Word);  // no comparable synthetic case yet
HOT171(0x08896070u, func_08896070, Word);
HOT171(0x08899D60u, func_08899D60, Word);
// HOT171(0x088A258Cu, func_088A258C, Word);  // no comparable synthetic case yet
HOT171(0x088A3794u, func_088A3794, Word);
HOT171(0x088A4630u, func_088A4630, Float);
HOT171(0x088AB560u, func_088AB560, Word);
HOT171(0x088AB57Cu, func_088AB57C, Word);
HOT171(0x088AB598u, func_088AB598, Word);
HOT171(0x088CEA60u, func_088CEA60, Word);
HOT171(0x088CF0ECu, func_088CF0EC, Word);
HOT171(0x088D1D90u, func_088D1D90, Word);
HOT171(0x088D4E4Cu, func_088D4E4C, Word);
HOT171(0x088D4EC8u, func_088D4EC8, Word);
HOT171(0x088D4F2Cu, func_088D4F2C, Word);
HOT171(0x088D61C4u, func_088D61C4, Word);
HOT171(0x088D6FB8u, func_088D6FB8, Word);
HOT171(0x088DB974u, func_088DB974, Word);
HOT171(0x088E7EA0u, func_088E7EA0, Word);
HOT171(0x088ECA38u, func_088ECA38, Word);
HOT171(0x089061D8u, func_089061D8, Word);
HOT171(0x08908F54u, func_08908F54, Word);
HOT171(0x0891A6A0u, func_0891A6A0, Word);
HOT171(0x08931B58u, func_08931B58, Word);
HOT171(0x089366FCu, func_089366FC, Word);
HOT171(0x08937B0Cu, func_08937B0C, Word);
HOT171(0x08937B44u, func_08937B44, Word);
HOT171(0x08937DCCu, func_08937DCC, Word);
HOT171(0x08937E04u, func_08937E04, Word);
HOT171(0x089416B8u, func_089416B8, Float);
HOT171(0x0895F158u, func_0895F158, Word);
HOT171(0x08965B78u, func_08965B78, Word);
HOT171(0x08974788u, func_08974788, Word);
HOT171(0x089747A4u, func_089747A4, Word);
HOT171(0x089952D8u, func_089952D8, Word);
HOT171(0x089952F4u, func_089952F4, Word);
HOT171(0x089961E4u, func_089961E4, Word);
HOT171(0x089A0B98u, func_089A0B98, Float);
HOT171(0x089A0BB4u, func_089A0BB4, Float);
HOT171(0x089B796Cu, func_089B796C, Word);
HOT171(0x089BA6B0u, func_089BA6B0, Word);
HOT171(0x089CD180u, func_089CD180, Float);
HOT171(0x089CD19Cu, func_089CD19C, Float);
HOT171(0x089CD1F0u, func_089CD1F0, Float);
HOT171(0x089CD20Cu, func_089CD20C, Float);
HOT171(0x089D83F4u, func_089D83F4, Float);
HOT171(0x089D8410u, func_089D8410, Float);
HOT171(0x089F0B0Cu, func_089F0B0C, Word);
HOT171(0x08A04FB4u, func_08A04FB4, Word);
HOT171(0x08A04FD0u, func_08A04FD0, Word);
HOT171(0x08A3412Cu, func_08A3412C, Word);
HOT171(0x08A3A7C8u, func_08A3A7C8, Word);
HOT171(0x08A4F750u, func_08A4F750, Word);
HOT171(0x08A503C8u, func_08A503C8, Word);
HOT171(0x08A72EC8u, func_08A72EC8, Word);
HOT171(0x08A878A4u, func_08A878A4, Float);
HOT171(0x08A87968u, func_08A87968, Float);
HOT171(0x08A95C3Cu, func_08A95C3C, Word);
HOT171(0x08AB9168u, func_08AB9168, Word);
HOT171(0x08ABF694u, func_08ABF694, Word);
HOT171(0x08AC0B68u, func_08AC0B68, Word);
HOT171(0x08ADF930u, func_08ADF930, Word);
HOT171(0x08ADF9FCu, func_08ADF9FC, Word);
HOT171(0x08ADFB7Cu, func_08ADFB7C, Word);
HOT171(0x08ADFC40u, func_08ADFC40, Word);
HOT171(0x08AE313Cu, func_08AE313C, Word);
HOT171(0x08AE3AECu, func_08AE3AEC, Word);
HOT171(0x08AF49B4u, func_08AF49B4, Float);
HOT171(0x08AF49D0u, func_08AF49D0, Float);
HOT171(0x08B03B98u, func_08B03B98, Word);
HOT171(0x08B0B104u, func_08B0B104, Word);
HOT171(0x08B1E97Cu, func_08B1E97C, Word);
HOT171(0x08B26A80u, func_08B26A80, Word);
HOT171(0x08B27384u, func_08B27384, Word);
HOT171(0x08B2740Cu, func_08B2740C, Word);
HOT171(0x08B27428u, func_08B27428, Word);
HOT171(0x08B27444u, func_08B27444, Word);
HOT171(0x08B2F44Cu, func_08B2F44C, Word);
HOT171(0x08B349A8u, func_08B349A8, Word);
HOT171(0x08B35238u, func_08B35238, Word);
HOT171(0x08B37710u, func_08B37710, Word);
HOT171(0x08B3F964u, func_08B3F964, Word);
HOT171(0x08B42100u, func_08B42100, Word);
HOT171(0x08B52284u, func_08B52284, Word);
HOT171(0x08B5293Cu, func_08B5293C, Word);
HOT171(0x08B56FACu, func_08B56FAC, Word);
HOT171(0x08B584B4u, func_08B584B4, Word);
HOT171(0x08B6EE30u, func_08B6EE30, Float);
HOT171(0x08B807B8u, func_08B807B8, Word);
HOT171(0x08BA5174u, func_08BA5174, Word);
HOT171(0x08BA53CCu, func_08BA53CC, Word);
HOT171(0x08BAE05Cu, func_08BAE05C, Word);
HOT171(0x08BAE294u, func_08BAE294, Word);
HOT171(0x08C1B474u, func_08C1B474, Word);
HOT171(0x08C76A6Cu, func_08C76A6C, Word);
