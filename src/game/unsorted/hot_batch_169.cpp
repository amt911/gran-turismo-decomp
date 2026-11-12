// Hot batch 169: five-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters, setters, bit fields, counters, small tests and tail calls that set up their arguments).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_169 {
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
// The byte at `table + a0`; v0 is that byte.
void table_byte_get(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 table, bool is_signed) {
    const u32 value = byte_at(rt, ctx.gpr[4] + table);
    ctx.gpr[2] = is_signed ? static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))) : value;
}
// Store a1 at `table + a0`; v0 is left holding the table address.
void table_byte_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 table) {
    set_byte(rt, ctx.gpr[4] + table, ctx.gpr[5]);
    ctx.gpr[2] = table;
}
// Bit `position` of the word at self + offset replaced by the low bit of a1 (v0 is the new word).
void set_flag_bit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset, unsigned position) {
    const u32 self = ctx.gpr[4];
    const u32 bit = ctx.gpr[5] & 1u;
    const u32 value = (word(rt, self + offset) & ~(1u << position)) | (bit << position);
    ctx.gpr[2] = value;
    put(rt, self + offset, value);
}
// word(self) += delta; v0 is self, v1 the new value.
void adjust_counter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 delta) {
    const u32 self = ctx.gpr[4];
    ctx.gpr[3] = word(rt, self) + delta;
    ctx.gpr[2] = self;
    put(rt, self, ctx.gpr[3]);
}
// Element count of the 8-byte-element range [word(self + first), word(self + first + 4)); v1 holds the begin pointer.
void range_count(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 first) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + first);
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(word(rt, ctx.gpr[4] + first + 4u) - ctx.gpr[3]) >> 3);
}
// word(self + 0xA8) + index * 64 + offset.
void slot_address(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0xA8u) + ctx.gpr[5] * 64u + offset;
}
// word(word(self) + member) + index * 32; v1 holds the inner object.
void element_address(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 member) {
    const u32 inner = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = inner;
    ctx.gpr[2] = word(rt, inner + member) + ctx.gpr[5] * 32u;
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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0880BEC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CBCD10u; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); }
void func_08817988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 4u; put(rt, G[4] + 0xCu, 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); }
void func_0885E9D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[4] = word(rt, G[2] + 0x5ACu); G[2] = word(rt, G[4] + 0x6C0u); G[3] = word(rt, G[4] + 0x6C4u); }
void func_0885EA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; G[4] = word(rt, G[2] + 0x5ACu); jump(ctx, 0x088DA2F4u); }
void func_0885EBB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = 1u; G[4] = word(rt, G[2] + 0x5ACu); store8(G[4] + 0x5BDu, 1u); }
void func_0885F068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); store8(G[2] + 0x3FB9u, G[5]); G[3] = word(rt, G[4]); put(rt, G[3] + 0x3FBCu, bits_of(ctx.fpr[12])); }
void func_088622B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = 1u; G[4] = word(rt, G[2] + 0x5ACu); store8(G[4] + 0x5C0u, 1u); }
void func_08866A9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x5ACu); G[2] = G[3] != 0u ? G[2] + 0x45D4u : 0u; }
void func_0886B4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = half_at(rt, word(rt, ctx.gpr[4] + 4u) + ctx.gpr[5] * 2u + 0x330u);
}
void func_0886B4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[2] = table;
    set_half(rt, table + ctx.gpr[5] * 2u + 0x330u, ctx.gpr[6]);
}
void func_088870A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    range_count(rt, ctx, 8u);
}
void func_088884D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    range_count(rt, ctx, 0xCu);
}
void func_0888EBA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    element_address(rt, ctx, 0x184u);
}
void func_0888FC14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = (__builtin_bswap32(word(rt, G[4] + 8u)) ^ 0x64u) == 0u ? 1u : 0u; }
void func_0889144C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x481907FFu; put(rt, G[4] + 0x58u, G[2]); store8(G[4] + 0x6Au, 0u); }
void func_088914A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = (word(rt, G[4] + 0x74u) << 2) + G[4]; G[2] = word(rt, G[3] + 0x2Cu); }
void func_08893230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x10000u; G[4] += G[2]; put(rt, G[4] - 0x7BB4u, 0u); store8(G[4] - 0x7BB8u, 0u); }
void func_08893D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    element_address(rt, ctx, 8u);
}
void func_08893DBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = (word(rt, G[3] + 4u) << 5) + 4u; }
void func_088A6C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = (__builtin_bswap32(word(rt, G[3] + 0x30u)) >> 1) & 1u; }
void func_088B5D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load16(G[5]); G[2] = (load16(G[4]) ^ G[3]) != 0u ? 1u : 0u; }
void func_088CB34C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x30u, G[2]); put(rt, G[4] + 0x63Cu, G[5]); put(rt, G[4] + 0x2Cu, G[2]); }
void func_088CF2CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[5] + G[6]; G[2] = static_cast<std::int32_t>(G[5]) < 0xF423F ? G[5] : 0xF423Fu; }
void func_088D26A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5]; G[7] = word(rt, G[5] + 8u); G[6] = word(rt, G[2] + 4u); G[5] = word(rt, G[5]); jump(ctx, 0x088D1DCCu); }
void func_088D4D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[3] = sext(load8(G[2] + 0x4Eu), 8u); G[2] = G[3] != 0u ? G[2] + 0x40u : 0u; }
void func_088D80C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0u; G[3] = 0u; put(rt, G[4] + 0x710u, 0u); put(rt, G[4] + 0x714u, 0u); }
void func_088E46F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x84u); G[7] &= 0xFFu; jump(ctx, G[25]); }
void func_088E4910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); store8(G[2] + 0x52Cu, 0u); G[3] = word(rt, G[4]); store8(G[3] + 0x527u, 0u); }
void func_088E7790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0xCu, G[8]); put(rt, G[4], G[5]); put(rt, G[4] + 4u, G[6]); put(rt, G[4] + 8u, G[7]); }
void func_088EF814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = load8(G[5] + 0x2Cu); store8(G[4], G[3]); G[2] = load8(G[5] + 0x2Du); store8(G[4] + 1u, G[2]); }
void func_088EF8B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    for (const u32 offset : {3u, 0u, 1u, 2u}) set_byte(rt, self + offset, 0u);
}
void func_088F0EF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    for (const u32 offset : {6u, 0u, 2u, 4u}) set_half(rt, self + offset, 0u);
}
void func_088F8924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; store8(G[4] + 9u, 1u); store8(G[4] + 8u, 0u); store8(G[4] + 0xAu, 1u); }
void func_089161DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CF205Cu; put(rt, G[2] + 0x14u, bits_of(ctx.fpr[13])); put(rt, G[2] + 0x10u, bits_of(ctx.fpr[12])); }
void func_08916A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x14u); G[3] = word(rt, G[4] + 0xCu); put(rt, G[4] + 0x10u, G[2]); put(rt, G[4] + 0x14u, G[3]); }
void func_08917E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 2) + G[4]; G[4] = word(rt, G[5] + 8u); G[5] = G[6]; jump(ctx, 0x089272FCu); }
void func_0891A884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4] + 0xE4u; put(rt, G[4] + 0xE4u, bits_of(ctx.fpr[12])); put(rt, G[2] + 8u, bits_of(ctx.fpr[14])); put(rt, G[2] + 4u, bits_of(ctx.fpr[13])); }
void func_08925C7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = word(rt, self + 0x10u) & ~ctx.gpr[5];
    put(rt, self + 0x10u, ctx.gpr[2]);
}
void func_08927128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CF28BCu); G[2] = word(rt, 0x08CF28BCu); G[4] = G[2] + 1u; put(rt, 0x08CF28BCu, G[4]); }
void func_08929C64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC62F8u; put(rt, G[4], G[2]); put(rt, G[4] + 4u, 0u); }
void func_08929C8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CC62F8u; put(rt, G[4] + 4u, G[5]); put(rt, G[4], G[2]); }
void func_0892AB4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load8(G[4] + 0x14u); store8(G[4] + 6u, G[5]); G[2] |= 2u; store8(G[4] + 0x14u, G[2]); }
void func_0892B8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_get(rt, ctx, 0x08CF2948u, false);
}
void func_0892B904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_set(rt, ctx, 0x08CF2948u);
}
void func_0892B918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_get(rt, ctx, 0x08CDDDA4u, true);
}
void func_0892B92C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_set(rt, ctx, 0x08CDDDA4u);
}
void func_0892B940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_get(rt, ctx, 0x08CDDD9Cu, false);
}
void func_0892B954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    table_byte_set(rt, ctx, 0x08CDDD9Cu);
}
void func_0892C890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], 0x60u); G[2] = 0x08C8AE9Cu; }
void func_0892C8A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4]; G[4] = 0x08CF2958u; G[6] = 0xFFFFFFFFu; jump(ctx, 0x08B20A54u); }
void func_0892C8C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4], 0x72u); G[2] = 0x08C8AB0Cu; }
void func_0892F890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x10u); G[5] = (G[5] << 4) + G[2]; G[2] = word(rt, G[5] + 0x10u); }
void func_0893389C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CF2968u); G[4] = word(rt, 0x08CF2968u); G[2] = word(rt, G[4] + 0xC0u); G[3] = word(rt, G[4] + 0xC4u); }
void func_089338B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CF2968u); G[4] = word(rt, 0x08CF2968u); G[2] = word(rt, G[4] + 0xC8u); G[3] = word(rt, G[4] + 0xCCu); }
void func_08936498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08CDDE34u); G[2] = (word(rt, 0x08CDDE34u) ^ G[5]) == 0u ? 1u : 0u; }
void func_08954F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 8u); G[5] = (G[5] << 2) + G[2]; ctx.fpr[0] = float_at(rt, G[5] + 0x784u); }
void func_089588F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 0xCu); G[2] = (load8(G[3] + 0x52u) ^ 6u) == 0u ? 1u : 0u; }
void func_089772E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x1D4u, bits_of(ctx.fpr[15])); put(rt, G[4] + 0x1C8u, bits_of(ctx.fpr[12])); put(rt, G[4] + 0x1CCu, bits_of(ctx.fpr[13])); put(rt, G[4] + 0x1D0u, bits_of(ctx.fpr[14])); }
void func_089773BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 1) + G[4] + G[6]; store8(G[5] + 0x154u, G[7]); }
void func_0899D050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_flag_bit(rt, ctx, 0x13Cu, 1);
}
void func_0899D5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_flag_bit(rt, ctx, 0x13Cu, 0);
}
void func_089E4E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[1] = float_at(rt, G[4] + 0x158u); ctx.fpr[2] = float_at(rt, G[4] + 0x164u); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[1]))); ctx.fpr[0] = psp_mul(ctx.fpr[2], ctx.fpr[0]); }
void func_089F7D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    slot_address(rt, ctx, 0x20u);
}
void func_089F7D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    slot_address(rt, ctx, 0x30u);
}
void func_08A155A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] += 0x3Cu; G[2] = word(rt, G[4] + 4u) - 0x10u; put(rt, G[4] + 4u, G[2]); }
void func_08A3233C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = static_cast<std::int32_t>(word(rt, G[4] + 4u)) > 0 ? 1u : 0u; G[2] = G[3] != 0u ? G[4] + 8u : 0u; }
void func_08A34E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[12] = float_at(rt, G[4] + 0x24u); G[1] = 0x3F800000u; ctx.fpr[14] = 1.0f; ctx.fpr[13] = 0.0f; jump(ctx, 0x08A34DBCu); }
void func_08A36E44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = word(rt, G[5]); G[4] += 0x14u; G[6] = 0u; G[7] = 0xFFFFFFFFu; jump(ctx, 0x08B48A74u); }
void func_08A4CF58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_flag_bit(rt, ctx, 0x58u, 8);
}
void func_08A4EE08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] &= 0xFFu; G[6] = G[5]; put(rt, G[4] + 0xA0u, 0u); G[5] = 1u; jump(ctx, 0x08A4E6F4u); }
void func_08A4F73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = load16(G[5] + 2u); G[3] = word(rt, G[4] + 0xCu); G[2] = G[3] + (G[2] << 3); }
void func_08A768CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    adjust_counter(rt, ctx, 1u);
}
void func_08A768E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    adjust_counter(rt, ctx, 0xFFFFFFFFu);
}
void func_08AA1988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x110u); G[25] = G[2]; G[6] = 1u; jump(ctx, G[25]); }
void func_08ABD254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD6308u; put(rt, G[4], G[2]); G[4] += 0x20u; jump(ctx, 0x08C3C46Cu); }
void func_08ABF6F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = (word(rt, G[4] + 0xCu) << 3) + G[5]; G[2] = word(rt, G[3] + 0x1Cu); }
void func_08AC221C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[5] - 1u; put(rt, G[4] + 0x28u, G[2]); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x24u, G[5]); }
void func_08ACA3E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4] + 4u) + 1u; G[2] = 0u; put(rt, G[4] + 4u, G[3]); }
void func_08AE4A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = (load8(G[5] + 0xBu) + 1u) << 24; G[2] = G[6] | G[3]; }
void func_08AF80A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[5] + 0xCu); put(rt, G[4] + 4u, G[3]); G[2] = word(rt, G[5] + 0x10u); put(rt, G[4] + 8u, G[2]); }
void func_08AFA3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, G[4] + 0x1Cu, G[8]); put(rt, G[4] + 0x10u, G[5]); put(rt, G[4] + 0x14u, G[6]); put(rt, G[4] + 0x18u, G[7]); }
void func_08B08898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = (word(rt, G[3] + 0x1A8u) ^ 2u) == 0u ? 1u : 0u; }
void func_08B089A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] &= 0xFFu; if (G[6] != 0u) put(rt, G[4] + 0x40u, G[5]); }
void func_08B089F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = word(rt, G[4]); G[2] = (word(rt, G[3]) ^ 1u) == 0u ? 1u : 0u; }
void func_08B09738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; put(rt, G[4], 0u); G[4] += 0x48u; put(rt, G[2] + 4u, 0u); jump(ctx, 0x08B28DCCu); }
void func_08B0DDF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08CD7C38u; put(rt, G[4], G[2]); G[4] += 4u; jump(ctx, 0x08B28BE0u); }
void func_08B1B40C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = load8(G[4] + 3u); G[5] = load8(G[2] + 2u); G[4] = sext(load16(G[4]), 16u); jump(ctx, 0x08B1B2F4u); }
void func_08B1B60C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = load8(G[4] + 2u); G[5] = load8(G[2] + 1u); G[4] = load8(G[4]); jump(ctx, 0x08B1B5E0u); }
void func_08B1BBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0xCu); G[3] = word(rt, G[4] + 0x10u); put(rt, G[4] + 4u, G[2]); put(rt, G[4] + 8u, G[3]); }
void func_08B20078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = load8(G[5] + G[4] + 0x118u); G[7] = word(rt, G[4] + 0xCu); G[6] = word(rt, G[4] + 8u); jump(ctx, 0x08B20008u); }
void func_08B21A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = G[4] + 0x70u; G[6] = 0xF8u; G[4] += 0x40u; G[7] = 2u; jump(ctx, 0x08B24D20u); }
void func_08B27460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x34u); store16(G[4] + 0x3Eu, 0u); put(rt, G[4] + 0x38u, G[2]); store16(G[4] + 0x3Cu, 0u); }
void func_08B27474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4] + 0x38u); G[3] = G[2] + 1u; store8(G[2], G[5]); put(rt, G[4] + 0x38u, G[3]); }
void func_08B3377C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = high_half(0x08D6920Cu); G[2] = word(rt, 0x08D6920Cu) - 1u; put(rt, 0x08D6920Cu, G[2]); }
void func_08B3E65C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[4]; G[6] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[2] + 4u); G[4] = word(rt, G[4] + 0x58u); jump(ctx, 0x08B79A88u); }
void func_08B47AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[3] = G[5] < 0x515u ? 1u : 0u; G[2] = 0x514u; if (G[3] == 0u) G[5] = G[2]; store16(G[4], G[5]); }
void func_08B531FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[5] = (G[5] << 5) + G[4]; G[3] = word(rt, G[5] + 0x30u); G[2] = G[3] + G[4]; }
void func_08B6E27C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; G[5] = (G[5] & 0x7FFFFFFFu) + 0x80100000u; G[2] = G[5] >> 31; }
void func_08BD31AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5];
    ctx.gpr[2] = source == 0u ? 0u : word(rt, source + 0x68u);
    put(rt, ctx.gpr[4] + 4u, ctx.gpr[2]);
}
void func_08BD31C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5];
    ctx.gpr[2] = source == 0u ? 0u : word(rt, source + 0x68u);
    put(rt, ctx.gpr[4] + 4u, ctx.gpr[2]);
}
void func_08BFD1D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    set_flag_bit(rt, ctx, 0x60u, 4);
}
void func_08C7506C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[6] -= G[5]; G[8] = sext(G[8], 8u); G[5] -= G[2]; jump(ctx, 0x08C74FACu); }
void func_08C75510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); G[5] = 0u; G[7] = 0u; G[6] = word(rt, G[2] - 0xCu); jump(ctx, 0x08C74B68u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_169
} // namespace gt::game

#define HOT169(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_169::name, gt::ReturnKind::kind)
HOT169(0x0880BEC4u, func_0880BEC4, Word);
HOT169(0x08817988u, func_08817988, Word);
HOT169(0x0885E9D8u, func_0885E9D8, DoubleWord);
// HOT169(0x0885EA48u, func_0885EA48, Word);  // no comparable synthetic case yet
HOT169(0x0885EBB0u, func_0885EBB0, Word);
HOT169(0x0885F068u, func_0885F068, Word);
HOT169(0x088622B8u, func_088622B8, Word);
HOT169(0x08866A9Cu, func_08866A9C, Word);
HOT169(0x0886B4E8u, func_0886B4E8, Word);
HOT169(0x0886B4FCu, func_0886B4FC, Word);
HOT169(0x088870A4u, func_088870A4, Word);
HOT169(0x088884D4u, func_088884D4, Word);
HOT169(0x0888EBA8u, func_0888EBA8, Word);
HOT169(0x0888FC14u, func_0888FC14, Word);
HOT169(0x0889144Cu, func_0889144C, Word);
HOT169(0x088914A0u, func_088914A0, Word);
HOT169(0x08893230u, func_08893230, Word);
HOT169(0x08893D38u, func_08893D38, Word);
HOT169(0x08893DBCu, func_08893DBC, Word);
HOT169(0x088A6C54u, func_088A6C54, Word);
HOT169(0x088B5D80u, func_088B5D80, Word);
HOT169(0x088CB34Cu, func_088CB34C, Word);
HOT169(0x088CF2CCu, func_088CF2CC, Word);
HOT169(0x088D26A0u, func_088D26A0, Word);
HOT169(0x088D4D88u, func_088D4D88, Word);
HOT169(0x088D80C0u, func_088D80C0, Word);
HOT169(0x088E46F0u, func_088E46F0, Word);
HOT169(0x088E4910u, func_088E4910, Word);
HOT169(0x088E7790u, func_088E7790, Word);
HOT169(0x088EF814u, func_088EF814, Word);
HOT169(0x088EF8B0u, func_088EF8B0, Word);
HOT169(0x088F0EF4u, func_088F0EF4, Word);
HOT169(0x088F8924u, func_088F8924, Word);
HOT169(0x089161DCu, func_089161DC, Word);
HOT169(0x08916A3Cu, func_08916A3C, Word);
HOT169(0x08917E5Cu, func_08917E5C, Word);
HOT169(0x0891A884u, func_0891A884, Word);
HOT169(0x08925C7Cu, func_08925C7C, Word);
HOT169(0x08927128u, func_08927128, Word);
HOT169(0x08929C64u, func_08929C64, Word);
HOT169(0x08929C8Cu, func_08929C8C, Word);
HOT169(0x0892AB4Cu, func_0892AB4C, Word);
HOT169(0x0892B8F0u, func_0892B8F0, Word);
HOT169(0x0892B904u, func_0892B904, Word);
HOT169(0x0892B918u, func_0892B918, Word);
HOT169(0x0892B92Cu, func_0892B92C, Word);
HOT169(0x0892B940u, func_0892B940, Word);
HOT169(0x0892B954u, func_0892B954, Word);
HOT169(0x0892C890u, func_0892C890, Word);
// HOT169(0x0892C8A4u, func_0892C8A4, Word);  // no comparable synthetic case yet
HOT169(0x0892C8C8u, func_0892C8C8, Word);
HOT169(0x0892F890u, func_0892F890, Word);
// HOT169(0x0893389Cu, func_0893389C, DoubleWord);  // no comparable synthetic case yet
// HOT169(0x089338B0u, func_089338B0, DoubleWord);  // no comparable synthetic case yet
HOT169(0x08936498u, func_08936498, Word);
HOT169(0x08954F18u, func_08954F18, Float);
HOT169(0x089588F0u, func_089588F0, Word);
HOT169(0x089772E0u, func_089772E0, Word);
HOT169(0x089773BCu, func_089773BC, Word);
HOT169(0x0899D050u, func_0899D050, Word);
HOT169(0x0899D5C0u, func_0899D5C0, Word);
HOT169(0x089E4E28u, func_089E4E28, Float);
HOT169(0x089F7D1Cu, func_089F7D1C, Word);
HOT169(0x089F7D30u, func_089F7D30, Word);
HOT169(0x08A155A4u, func_08A155A4, Word);
HOT169(0x08A3233Cu, func_08A3233C, Word);
HOT169(0x08A34E98u, func_08A34E98, Word);
HOT169(0x08A36E44u, func_08A36E44, Word);
HOT169(0x08A4CF58u, func_08A4CF58, Word);
HOT169(0x08A4EE08u, func_08A4EE08, Word);
HOT169(0x08A4F73Cu, func_08A4F73C, Word);
HOT169(0x08A768CCu, func_08A768CC, Word);
HOT169(0x08A768E0u, func_08A768E0, Word);
HOT169(0x08AA1988u, func_08AA1988, Word);
HOT169(0x08ABD254u, func_08ABD254, Word);
HOT169(0x08ABF6F0u, func_08ABF6F0, Word);
HOT169(0x08AC221Cu, func_08AC221C, Word);
HOT169(0x08ACA3E0u, func_08ACA3E0, Word);
HOT169(0x08AE4A7Cu, func_08AE4A7C, Word);
HOT169(0x08AF80A8u, func_08AF80A8, Word);
HOT169(0x08AFA3F0u, func_08AFA3F0, Word);
HOT169(0x08B08898u, func_08B08898, Word);
HOT169(0x08B089A8u, func_08B089A8, Word);
HOT169(0x08B089F8u, func_08B089F8, Word);
HOT169(0x08B09738u, func_08B09738, Word);
HOT169(0x08B0DDF0u, func_08B0DDF0, Word);
HOT169(0x08B1B40Cu, func_08B1B40C, Word);
HOT169(0x08B1B60Cu, func_08B1B60C, Word);
HOT169(0x08B1BBECu, func_08B1BBEC, Word);
HOT169(0x08B20078u, func_08B20078, Word);
HOT169(0x08B21A98u, func_08B21A98, Word);
HOT169(0x08B27460u, func_08B27460, Word);
HOT169(0x08B27474u, func_08B27474, Word);
HOT169(0x08B3377Cu, func_08B3377C, Word);
HOT169(0x08B3E65Cu, func_08B3E65C, Word);
HOT169(0x08B47AD4u, func_08B47AD4, Word);
HOT169(0x08B531FCu, func_08B531FC, Word);
HOT169(0x08B6E27Cu, func_08B6E27C, Word);
HOT169(0x08BD31ACu, func_08BD31AC, Word);
HOT169(0x08BD31C0u, func_08BD31C0, Word);
HOT169(0x08BFD1D0u, func_08BFD1D0, Word);
// HOT169(0x08C7506Cu, func_08C7506C, Word);  // no comparable synthetic case yet
HOT169(0x08C75510u, func_08C75510, Word);
