// Hot batch 165: small shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every
// member of their family (rows from tools.family_rows), verified by synthetic cases; written by hand from
// `tools.disasm`; role words describe access shapes, none is a game name. Each family's own template is a named
// function elsewhere, so a pending member is the template here.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_165 {
namespace {

using namespace gt::game::hot;

// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }

// global_address() with the member's global: its address (`lui; addiu`).
void global_address(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t global) { ctx.gpr[2] = global; }

// second_tail(_, x, ...) with the member's target: target(x, x, ...) (tail call; a0 = a1).
void second_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    tail_call(ctx, target, {ctx.gpr[5]});
}

// clear_two(this) with the member's offsets: both words 0.
void clear_two(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    put(rt, ctx.gpr[4] + first, 0u);
    put(rt, ctx.gpr[4] + second, 0u);
}

// clear_three(this) with the member's offsets: the three words 0.
void clear_three(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second,
                 std::uint32_t third) {
    put(rt, ctx.gpr[4] + first, 0u);
    put(rt, ctx.gpr[4] + second, 0u);
    put(rt, ctx.gpr[4] + third, 0u);
}

// clear_four(this) with the member's offsets: the four words 0.
void clear_four(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second,
                std::uint32_t third, std::uint32_t fourth) {
    put(rt, ctx.gpr[4] + first, 0u);
    put(rt, ctx.gpr[4] + second, 0u);
    put(rt, ctx.gpr[4] + third, 0u);
    put(rt, ctx.gpr[4] + fourth, 0u);
}

// swap_member_tail(this, x, ...) with the member's offset and target: target(x, this + offset, ...) (tail call; v0 =
// this + offset).
void swap_member_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t offset, std::uint32_t target) {
    ctx.gpr[2] = ctx.gpr[4] + offset;
    tail_call(ctx, target, {ctx.gpr[5], ctx.gpr[2]});
}

// swap_tail(this, x, ...) with the member's target: target(x, this, ...) (tail call; v0 = this).
void swap_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    ctx.gpr[2] = ctx.gpr[4];
    tail_call(ctx, target, {ctx.gpr[5], ctx.gpr[2]});
}

// store_global(x) with the member's global: *global = x (v0 = the address's `lui` half).
void store_global(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t global) {
    ctx.gpr[2] = high_half(global);
    put(rt, global, ctx.gpr[4]);
}

// field_zero_tail(this, x, ...) with the member's offset and target: target(this->[offset], 0, x, ...) (tail call).
void field_zero_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset, std::uint32_t target) {
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + offset), 0u, ctx.gpr[5]});
}

// set_flag(this) with the member's value and offset: the byte = value (v0 = value).
void set_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t value, std::uint32_t offset) {
    ctx.gpr[2] = value;
    rt.memory().store8(ctx.gpr[4] + offset, static_cast<std::uint8_t>(value));
}

// store_three(this, a, b, c) with the member's offsets: this->[third] = c, this->[first] = a, this->[second] = b.
void store_three(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second,
                 std::uint32_t third) {
    put(rt, ctx.gpr[4] + third, ctx.gpr[7]);
    put(rt, ctx.gpr[4] + first, ctx.gpr[5]);
    put(rt, ctx.gpr[4] + second, ctx.gpr[6]);
}

// two_fields_tail(this, ...) with the member's offsets and target: target(this->[first], this->[second], ...) (tail
// call).
void two_fields_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t second, std::uint32_t target,
                     std::uint32_t first) {
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + first), word(rt, ctx.gpr[4] + second)});
}

// word_at(this, i) with the member's base: the word i at this + base (a1 = its address less base).
void word_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base) {
    ctx.gpr[5] = (ctx.gpr[5] << 2) + ctx.gpr[4];
    ctx.gpr[2] = word(rt, ctx.gpr[5] + base);
}

// field_set(this) with the member's offset: whether the word is non-zero.
void field_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + offset) != 0u ? 1u : 0u;
}

} // namespace

// Templates of their families (pending members; the other members are the batch 165 rows below).
void func_0885E2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08CE2D90u); }
void func_08837498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x0892A07Cu); }
void func_0888F3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 4u, 0u); }
void func_0892A3E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_088F1268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x14u, 0x08BB8E14u); }
void func_0890AE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(rt, ctx, 0x08CE9F68u); }
void func_08A6BFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_zero_tail(rt, ctx, 0x14u, 0x08B7E518u); }
void func_088F0F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_tail(rt, ctx, 0x08BB8CFCu); }
void func_08894E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_flag(rt, ctx, 1u, 0x18u); }
void func_088E7F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_three(rt, ctx, 0x1Cu, 0x2E4u, 0x2Cu); }
void func_08873E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_fields_tail(rt, ctx, 8u, 0x08873ADCu, 0xCu); }
void func_08917940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { word_at(rt, ctx, 8u); }
void func_08865F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_four(rt, ctx, 4u, 0x10u, 8u, 0xCu); }
void func_0887D38C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_set(rt, ctx, 0x50u); }

// Batch 165 rows (tools.family_rows).
void func_088B4384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08CE3034u); }
void func_088C49F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08C89E64u); }
void func_08944A70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08CF2BECu); }
void func_08B09354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08CB4B38u); }
void func_08B09504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08CB4B40u); }
void func_08B21A30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08B219A0u); }
void func_08B35214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { global_address(rt, ctx, 0x08D69278u); }
void func_088374A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x0892A07Cu); }
void func_0885E18C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x0890AE5Cu); }
void func_088ECB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x08B0A1CCu); }
void func_088ECB14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x08B09C54u); }
void func_08A50AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x08B57DE8u); }
void func_08B1BE50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x08B1BE48u); }
void func_08B354A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { second_tail(rt, ctx, 0x08B35254u); }
void func_088D04F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 4u, 0u); }
void func_08911244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 4u, 0u); }
void func_0891612C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 0u, 4u); }
void func_08ABA404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 4u, 0u); }
void func_08B1FDA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 8u, 4u); }
void func_08B34D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_two(rt, ctx, 4u, 0u); }
void func_0892A414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_0892A604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_0892A7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_08A128EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_08A19688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 48u, 40u, 44u); }
void func_08B1BC10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_three(rt, ctx, 8u, 0u, 4u); }
void func_088F1278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x14u, 0x08BB8E40u); }
void func_08908EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0xCu, 0x08BD5B0Cu); }
void func_08AF770C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x4u, 0x08C407B4u); }
void func_08AF8840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x10u, 0x08C4089Cu); }
void func_08AF9784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x2Cu, 0x08C41A04u); }
void func_08AF98E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_member_tail(rt, ctx, 0x40u, 0x08C41AFCu); }
void func_08B092E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(rt, ctx, 0x08CE0A28u); }
void func_08B092FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(rt, ctx, 0x08D47450u); }
void func_08B21404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(rt, ctx, 0x08D68B70u); }
void func_08A6C228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_zero_tail(rt, ctx, 0x14u, 0x08B7E4CCu); }
void func_08A6C238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_zero_tail(rt, ctx, 0x14u, 0x08B7E4CCu); }
void func_08A6C248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_zero_tail(rt, ctx, 0x14u, 0x08B7E518u); }
void func_088F0FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_tail(rt, ctx, 0x08BB8D28u); }
void func_08AD1518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_tail(rt, ctx, 0x08B4ACE0u); }
void func_08AF98C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swap_tail(rt, ctx, 0x08C41AFCu); }
void func_08961468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_flag(rt, ctx, 1u, 0x14u); }
void func_08B3F7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_flag(rt, ctx, 2u, 0x1u); }
void func_08B3E64C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_three(rt, ctx, 0x2Cu, 0x30u, 0x54u); }
void func_08B3E778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_three(rt, ctx, 0xCu, 0x10u, 0x14u); }
void func_0888119C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_fields_tail(rt, ctx, 8u, 0x08880D70u, 0xCu); }
void func_08885310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_fields_tail(rt, ctx, 4u, 0x08884FF4u, 0xCu); }
void func_0892778C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { word_at(rt, ctx, 8048u); }
void func_089277A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { word_at(rt, ctx, 8048u); }
void func_08AB9270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_four(rt, ctx, 12u, 0x0u, 4u, 0x8u); }
void func_08BACAD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clear_four(rt, ctx, 12u, 0x0u, 4u, 0x8u); }
void func_088EB300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_set(rt, ctx, 0x530u); }
void func_08B61534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_set(rt, ctx, 0x0u); }

} // namespace hot_batch_165
} // namespace gt::game

#define HOT165(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_165::name, gt::ReturnKind::kind)
HOT165(0x0885E2F8u, func_0885E2F8, Word);
HOT165(0x08837498u, func_08837498, Word);
HOT165(0x0888F3CCu, func_0888F3CC, Word);
HOT165(0x0892A3E4u, func_0892A3E4, Word);
HOT165(0x088F1268u, func_088F1268, Word);
HOT165(0x0890AE5Cu, func_0890AE5C, Word);
HOT165(0x08A6BFBCu, func_08A6BFBC, Word);
HOT165(0x088F0F9Cu, func_088F0F9C, Word);
HOT165(0x08894E10u, func_08894E10, Word);
HOT165(0x088E7F94u, func_088E7F94, Word);
HOT165(0x08873E94u, func_08873E94, Word);
HOT165(0x08917940u, func_08917940, Word);
HOT165(0x08865F30u, func_08865F30, Word);
HOT165(0x0887D38Cu, func_0887D38C, Word);
HOT165(0x088B4384u, func_088B4384, Word);
HOT165(0x088C49F0u, func_088C49F0, Word);
HOT165(0x08944A70u, func_08944A70, Word);
HOT165(0x08B09354u, func_08B09354, Word);
HOT165(0x08B09504u, func_08B09504, Word);
HOT165(0x08B21A30u, func_08B21A30, Word);
HOT165(0x08B35214u, func_08B35214, Word);
HOT165(0x088374A0u, func_088374A0, Word);
HOT165(0x0885E18Cu, func_0885E18C, Word);
HOT165(0x088ECB0Cu, func_088ECB0C, Word);
HOT165(0x088ECB14u, func_088ECB14, Word);
HOT165(0x08A50AB8u, func_08A50AB8, Word);
HOT165(0x08B1BE50u, func_08B1BE50, Word);
HOT165(0x08B354A4u, func_08B354A4, Word);
HOT165(0x088D04F0u, func_088D04F0, Word);
HOT165(0x08911244u, func_08911244, Word);
HOT165(0x0891612Cu, func_0891612C, Word);
HOT165(0x08ABA404u, func_08ABA404, Word);
HOT165(0x08B1FDA4u, func_08B1FDA4, Word);
HOT165(0x08B34D24u, func_08B34D24, Word);
HOT165(0x0892A414u, func_0892A414, Word);
HOT165(0x0892A604u, func_0892A604, Word);
HOT165(0x0892A7D0u, func_0892A7D0, Word);
HOT165(0x08A128ECu, func_08A128EC, Word);
HOT165(0x08A19688u, func_08A19688, Word);
HOT165(0x08B1BC10u, func_08B1BC10, Word);
HOT165(0x088F1278u, func_088F1278, Word);
HOT165(0x08908EF0u, func_08908EF0, Word);
HOT165(0x08AF770Cu, func_08AF770C, Word);
HOT165(0x08AF8840u, func_08AF8840, Word);
HOT165(0x08AF9784u, func_08AF9784, Word);
HOT165(0x08AF98E0u, func_08AF98E0, Word);
HOT165(0x08B092E4u, func_08B092E4, Word);
HOT165(0x08B092FCu, func_08B092FC, Word);
HOT165(0x08B21404u, func_08B21404, Word);
HOT165(0x08A6C228u, func_08A6C228, Word);
HOT165(0x08A6C238u, func_08A6C238, Word);
HOT165(0x08A6C248u, func_08A6C248, Word);
HOT165(0x088F0FACu, func_088F0FAC, Word);
HOT165(0x08AD1518u, func_08AD1518, Word);
HOT165(0x08AF98C0u, func_08AF98C0, Word);
HOT165(0x08961468u, func_08961468, Word);
HOT165(0x08B3F7A8u, func_08B3F7A8, Word);
HOT165(0x08B3E64Cu, func_08B3E64C, Word);
HOT165(0x08B3E778u, func_08B3E778, Word);
HOT165(0x0888119Cu, func_0888119C, Word);
HOT165(0x08885310u, func_08885310, Word);
HOT165(0x0892778Cu, func_0892778C, Word);
HOT165(0x089277A4u, func_089277A4, Word);
HOT165(0x08AB9270u, func_08AB9270, Word);
HOT165(0x08BACAD4u, func_08BACAD4, Word);
HOT165(0x088EB300u, func_088EB300, Word);
HOT165(0x08B61534u, func_08B61534, Word);
