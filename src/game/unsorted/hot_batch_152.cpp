// Hot batch 152: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name. The templates of these families are named functions elsewhere
// (RaceOperator, RaceOrganizer, CarDriverParameter), so each wrapper's template here is a pending member.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_152 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// deref_byte(this) with the member's offsets: the unsigned byte at this->[outer] + offset (v1 = this->[outer]).
void deref_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t outer, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + outer);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + offset);
}

// deref_store_byte(this, value) with the member's offsets: the byte at this->[outer] + offset = value
// (v0 = this->[outer]).
void deref_store_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t outer, std::uint32_t offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + outer);
    rt.memory().store8(ctx.gpr[2] + offset, static_cast<std::uint8_t>(ctx.gpr[5]));
}

// deref_signed_byte(this) with the member's offset: the signed byte at (*this) + offset (v1 = *this).
void deref_signed_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[3] + offset)));
}

// deref_byte_set(this) with the member's offset: whether the byte at (*this) + offset is non-zero (v1 = *this).
void deref_byte_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + offset) != 0u ? 1u : 0u;
}

// deref_swapped(this) with the member's offset: the word at (*this) + offset, byte-reversed (wsbw; v1 = *this).
void deref_swapped(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = __builtin_bswap32(word(rt, ctx.gpr[3] + offset));
}

// deref_store_swapped(this, value) with the member's offset: the word at (*this) + offset = value byte-reversed
// (a1 keeps the reversed value, v0 = *this).
void deref_store_swapped(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    ctx.gpr[5] = __builtin_bswap32(ctx.gpr[5]);
    put(rt, ctx.gpr[2] + offset, ctx.gpr[5]);
}

// deref_plus_tail(this, ...) with the member's target and offsets: target(this->[outer] + offset, ...) (tail call).
void deref_plus_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t outer,
                     std::uint32_t offset) {
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + outer) + offset});
}

// deref_field_tail(this, ...) with the member's target and offset: target((*this)->[offset], ...) (tail call; v0 = *this).
void deref_field_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t offset) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    tail_call(ctx, target, {word(rt, ctx.gpr[2] + offset)});
}

// deref_field_call(this, ...) with the member's callee and offset: callee((*this)->[offset], ...); its v0 back.
void deref_field_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t offset) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    call_set(rt, ctx, callee, {word(rt, ctx.gpr[2] + offset)});
}

// held_member_call() with the member's callee: a holder h = func_088248E4(&h) (the arguments as they came), its
// object o (null: func_08A5BB7C(0x08C99B4C) first), h destroyed (func_08A5B9EC), then callee(o->+0x14); its v0 back.
void held_member_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, 0x088248E4u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08C99B4Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const std::uint32_t member = word(rt, object + 0x14u);
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
    call_set(rt, ctx, callee, {member});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 152 rows below).
void func_0885BC9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 24u); }
void func_0885C368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 20u); }
void func_08897030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 144u); }
void func_08896FDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 142u); }
void func_088A746C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_swapped(rt, ctx, 48u); }
void func_088D5948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_swapped(rt, ctx, 20u); }
void func_0885E4AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x088916C0u, 0u, 0x3F80u); }
void func_0885E4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088DA160u, 0x5ACu); }
void func_0885E81C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088DB810u, 0x5ACu); }
void func_08824AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885C88Cu); }

// Batch 152 rows (tools.family_rows).
void func_0885C374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 20u); }
void func_0886AF64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 48u); }
void func_08891568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 160u); }
void func_08897094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 153u); }
void func_088C49FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 36u, 164u); }
void func_088E48DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 1319u); }
void func_088E49D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 1300u); }
void func_08917AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 12u, 8454u); }
void func_089316DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 147u); }
void func_089316F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 144u); }
void func_0893170C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 146u); }
void func_08931718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 148u); }
void func_08931724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 149u); }
void func_08931730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 150u); }
void func_0893173C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 151u); }
void func_08931748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 152u); }
void func_08931754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 153u); }
void func_08931760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 154u); }
void func_0893176C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 155u); }
void func_08931778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 156u); }
void func_08931784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 90u); }
void func_08931790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 91u); }
void func_0893179C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 96u); }
void func_089317A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 98u); }
void func_089317CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 101u); }
void func_08931820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 108u); }
void func_0893182C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 109u); }
void func_08931838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 110u); }
void func_08931844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 112u); }
void func_08931850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 113u); }
void func_0893185C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 111u); }
void func_089318B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 4u, 97u); }
void func_08AC26C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 7u); }
void func_08AF29C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 76u); }
void func_08AF2A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 85u); }
void func_08B089EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 8u); }
void func_08B08A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 9u); }
void func_08B09818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte(rt, ctx, 0u, 285u); }
void func_0887CB3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 4u, 1016u); }
void func_0889703C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 144u); }
void func_08897070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 151u); }
void func_08897088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 147u); }
void func_088970C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 149u); }
void func_088A5950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 45u); }
void func_088A5968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 46u); }
void func_088A5D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 8u); }
void func_088C4BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 36u, 164u); }
void func_088D4C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 73u); }
void func_088D4C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 74u); }
void func_088D4C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 75u); }
void func_088D4C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 77u); }
void func_088D4C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 36u); }
void func_088D4C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 37u); }
void func_088D4CAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 76u); }
void func_088D4CD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 80u); }
void func_088D4CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 81u); }
void func_088D4DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 79u); }
void func_088D4DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 84u); }
void func_088D4DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 85u); }
void func_088D4DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 86u); }
void func_088D4FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 113u); }
void func_088D500C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 87u); }
void func_088E4BBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 1322u); }
void func_08917ACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 12u, 8135u); }
void func_08AF29B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_byte(rt, ctx, 0u, 76u); }
void func_0889707C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 147u); }
void func_088A5944(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 45u); }
void func_088A5D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 8u); }
void func_088D4DF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 86u); }
void func_08B08A70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 57u); }
void func_08897060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 151u); }
void func_088970B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 149u); }
void func_088D4C18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 74u); }
void func_088D4C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 75u); }
void func_088D4CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 81u); }
void func_088D4E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 96u); }
void func_088D4FFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_byte_set(rt, ctx, 87u); }
void func_088D5018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_swapped(rt, ctx, 0u); }
void func_088D5938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_swapped(rt, ctx, 20u); }
void func_088D5D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_swapped(rt, ctx, 28u); }
void func_088D5958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_swapped(rt, ctx, 16u); }
void func_088D5968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_store_swapped(rt, ctx, 28u); }
void func_0885E728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x08917840u, 0u, 0x5CCu); }
void func_0885E740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x08917E5Cu, 0u, 0x5CCu); }
void func_0885EEE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x088915E8u, 0u, 0x3F80u); }
void func_0885F82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x08917E70u, 0u, 0x5CCu); }
void func_08869D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886AF24u, 4u, 0x4238u); }
void func_08869D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B098u, 4u, 0x4238u); }
void func_08869D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886AF3Cu, 4u, 0x4238u); }
void func_08869D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B074u, 4u, 0x4238u); }
void func_08869D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B7E8u, 4u, 0x4238u); }
void func_08869E60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B30Cu, 4u, 0x4238u); }
void func_08869EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B038u, 4u, 0x4238u); }
void func_08869ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B060u, 4u, 0x4238u); }
void func_08869EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B344u, 4u, 0x4238u); }
void func_0886A020(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0886B9A4u, 4u, 0x4238u); }
void func_0886B1A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0887D398u, 4u, 0x88u); }
void func_08892678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x08892544u, 0u, 0x14u); }
void func_088D4FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x088EF554u, 0u, 0x82u); }
void func_088D4FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x088EF470u, 0u, 0x82u); }
void func_088D4FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x088EF50Cu, 0u, 0x82u); }
void func_08917AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x0891A854u, 12u, 0x1B20u); }
void func_08B08AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_plus_tail(rt, ctx, 0x08B09658u, 0u, 0x10u); }
void func_0885E4C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088DC90Cu, 0x5ACu); }
void func_0885E4D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088DA4DCu, 0x5ACu); }
void func_0885E65C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088DB208u, 0x5ACu); }
void func_0885E83C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088D80D4u, 0x5ACu); }
void func_0885EBC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088D6FB8u, 0x5ACu); }
void func_0885EBD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088DA4B8u, 0x5ACu); }
void func_088E4360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088EF438u, 0xACu); }
void func_088E436C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_tail(rt, ctx, 0x088EF430u, 0xACu); }
void func_0885E9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088D9E58u, 0x5ACu); }
void func_0885EA28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088DEC58u, 0x5ACu); }
void func_0885EBDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088DA0BCu, 0x5ACu); }
void func_0885EBFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088D9D50u, 0x5ACu); }
void func_0885F80C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_field_call(rt, ctx, 0x088D75E0u, 0x5ACu); }
void func_08824B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885C2C8u); }
void func_08824B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885D110u); }
void func_08824BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885BBE8u); }
void func_08824C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885D07Cu); }
void func_08824C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885CBA4u); }
void func_08824D00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885C0D4u); }
void func_08824D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885C8A8u); }
void func_08824E38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_member_call(rt, ctx, 0x0885C538u); }

} // namespace hot_batch_152
} // namespace gt::game

#define HOT152(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_152::name, gt::ReturnKind::kind)
HOT152(0x0885BC9Cu, func_0885BC9C, Word);
HOT152(0x0885C368u, func_0885C368, Word);
HOT152(0x08897030u, func_08897030, Word);
HOT152(0x08896FDCu, func_08896FDC, Word);
HOT152(0x088A746Cu, func_088A746C, Word);
HOT152(0x088D5948u, func_088D5948, Word);
HOT152(0x0885E4ACu, func_0885E4AC, Word);
HOT152(0x0885E4B8u, func_0885E4B8, Word);
// HOT152(0x0885E81Cu, func_0885E81C, Word);  // no comparable synthetic case yet
// HOT152(0x08824AD0u, func_08824AD0, Word);  // no comparable synthetic case yet
HOT152(0x0885C374u, func_0885C374, Word);
HOT152(0x0886AF64u, func_0886AF64, Word);
HOT152(0x08891568u, func_08891568, Word);
HOT152(0x08897094u, func_08897094, Word);
HOT152(0x088C49FCu, func_088C49FC, Word);
HOT152(0x088E48DCu, func_088E48DC, Word);
HOT152(0x088E49D8u, func_088E49D8, Word);
HOT152(0x08917AF0u, func_08917AF0, Word);
HOT152(0x089316DCu, func_089316DC, Word);
HOT152(0x089316F4u, func_089316F4, Word);
HOT152(0x0893170Cu, func_0893170C, Word);
HOT152(0x08931718u, func_08931718, Word);
HOT152(0x08931724u, func_08931724, Word);
HOT152(0x08931730u, func_08931730, Word);
HOT152(0x0893173Cu, func_0893173C, Word);
HOT152(0x08931748u, func_08931748, Word);
HOT152(0x08931754u, func_08931754, Word);
HOT152(0x08931760u, func_08931760, Word);
HOT152(0x0893176Cu, func_0893176C, Word);
HOT152(0x08931778u, func_08931778, Word);
HOT152(0x08931784u, func_08931784, Word);
HOT152(0x08931790u, func_08931790, Word);
HOT152(0x0893179Cu, func_0893179C, Word);
HOT152(0x089317A8u, func_089317A8, Word);
HOT152(0x089317CCu, func_089317CC, Word);
HOT152(0x08931820u, func_08931820, Word);
HOT152(0x0893182Cu, func_0893182C, Word);
HOT152(0x08931838u, func_08931838, Word);
HOT152(0x08931844u, func_08931844, Word);
HOT152(0x08931850u, func_08931850, Word);
HOT152(0x0893185Cu, func_0893185C, Word);
HOT152(0x089318B8u, func_089318B8, Word);
HOT152(0x08AC26C8u, func_08AC26C8, Word);
HOT152(0x08AF29C4u, func_08AF29C4, Word);
HOT152(0x08AF2A98u, func_08AF2A98, Word);
HOT152(0x08B089ECu, func_08B089EC, Word);
HOT152(0x08B08A58u, func_08B08A58, Word);
HOT152(0x08B09818u, func_08B09818, Word);
HOT152(0x0887CB3Cu, func_0887CB3C, Word);
HOT152(0x0889703Cu, func_0889703C, Word);
HOT152(0x08897070u, func_08897070, Word);
HOT152(0x08897088u, func_08897088, Word);
HOT152(0x088970C8u, func_088970C8, Word);
HOT152(0x088A5950u, func_088A5950, Word);
HOT152(0x088A5968u, func_088A5968, Word);
HOT152(0x088A5D30u, func_088A5D30, Word);
HOT152(0x088C4BA4u, func_088C4BA4, Word);
HOT152(0x088D4C0Cu, func_088D4C0C, Word);
HOT152(0x088D4C28u, func_088D4C28, Word);
HOT152(0x088D4C44u, func_088D4C44, Word);
HOT152(0x088D4C60u, func_088D4C60, Word);
HOT152(0x088D4C78u, func_088D4C78, Word);
HOT152(0x088D4C90u, func_088D4C90, Word);
HOT152(0x088D4CACu, func_088D4CAC, Word);
HOT152(0x088D4CD0u, func_088D4CD0, Word);
HOT152(0x088D4CF8u, func_088D4CF8, Word);
HOT152(0x088D4DACu, func_088D4DAC, Word);
HOT152(0x088D4DC8u, func_088D4DC8, Word);
HOT152(0x088D4DE4u, func_088D4DE4, Word);
HOT152(0x088D4DFCu, func_088D4DFC, Word);
HOT152(0x088D4FB4u, func_088D4FB4, Word);
HOT152(0x088D500Cu, func_088D500C, Word);
HOT152(0x088E4BBCu, func_088E4BBC, Word);
HOT152(0x08917ACCu, func_08917ACC, Word);
HOT152(0x08AF29B8u, func_08AF29B8, Word);
HOT152(0x0889707Cu, func_0889707C, Word);
HOT152(0x088A5944u, func_088A5944, Word);
HOT152(0x088A5D24u, func_088A5D24, Word);
HOT152(0x088D4DF0u, func_088D4DF0, Word);
HOT152(0x08B08A70u, func_08B08A70, Word);
HOT152(0x08897060u, func_08897060, Word);
HOT152(0x088970B8u, func_088970B8, Word);
HOT152(0x088D4C18u, func_088D4C18, Word);
HOT152(0x088D4C34u, func_088D4C34, Word);
HOT152(0x088D4CE8u, func_088D4CE8, Word);
HOT152(0x088D4E08u, func_088D4E08, Word);
HOT152(0x088D4FFCu, func_088D4FFC, Word);
HOT152(0x088D5018u, func_088D5018, Word);
HOT152(0x088D5938u, func_088D5938, Word);
HOT152(0x088D5D44u, func_088D5D44, Word);
HOT152(0x088D5958u, func_088D5958, Word);
HOT152(0x088D5968u, func_088D5968, Word);
HOT152(0x0885E728u, func_0885E728, Word);
HOT152(0x0885E740u, func_0885E740, Word);
HOT152(0x0885EEE0u, func_0885EEE0, Word);
HOT152(0x0885F82Cu, func_0885F82C, Word);
HOT152(0x08869D04u, func_08869D04, Word);
HOT152(0x08869D10u, func_08869D10, Word);
HOT152(0x08869D1Cu, func_08869D1C, Word);
HOT152(0x08869D28u, func_08869D28, Word);
// HOT152(0x08869D40u, func_08869D40, Word);  // no comparable synthetic case yet
// HOT152(0x08869E60u, func_08869E60, Word);  // no comparable synthetic case yet
// HOT152(0x08869EC0u, func_08869EC0, Word);  // no comparable synthetic case yet
HOT152(0x08869ECCu, func_08869ECC, Word);
HOT152(0x08869EF0u, func_08869EF0, Word);
// HOT152(0x0886A020u, func_0886A020, Word);  // no comparable synthetic case yet
HOT152(0x0886B1A0u, func_0886B1A0, Word);
HOT152(0x08892678u, func_08892678, Word);
HOT152(0x088D4FD8u, func_088D4FD8, Word);
HOT152(0x088D4FE4u, func_088D4FE4, Word);
HOT152(0x088D4FF0u, func_088D4FF0, Word);
HOT152(0x08917AC0u, func_08917AC0, Word);
HOT152(0x08B08AA0u, func_08B08AA0, Word);
HOT152(0x0885E4C4u, func_0885E4C4, Word);
HOT152(0x0885E4D0u, func_0885E4D0, Word);
HOT152(0x0885E65Cu, func_0885E65C, Word);
// HOT152(0x0885E83Cu, func_0885E83C, Word);  // no comparable synthetic case yet
HOT152(0x0885EBC4u, func_0885EBC4, Word);
HOT152(0x0885EBD0u, func_0885EBD0, Word);
HOT152(0x088E4360u, func_088E4360, Word);
HOT152(0x088E436Cu, func_088E436C, Word);
HOT152(0x0885E9B8u, func_0885E9B8, Word);
HOT152(0x0885EA28u, func_0885EA28, Word);
HOT152(0x0885EBDCu, func_0885EBDC, Word);
HOT152(0x0885EBFCu, func_0885EBFC, Word);
HOT152(0x0885F80Cu, func_0885F80C, Word);
// HOT152(0x08824B20u, func_08824B20, Word);  // no comparable synthetic case yet
// HOT152(0x08824B70u, func_08824B70, Word);  // no comparable synthetic case yet
// HOT152(0x08824BC0u, func_08824BC0, Word);  // no comparable synthetic case yet
// HOT152(0x08824C10u, func_08824C10, Word);  // no comparable synthetic case yet
// HOT152(0x08824C60u, func_08824C60, Word);  // no comparable synthetic case yet
// HOT152(0x08824D00u, func_08824D00, Word);  // no comparable synthetic case yet
// HOT152(0x08824D50u, func_08824D50, Word);  // no comparable synthetic case yet
// HOT152(0x08824E38u, func_08824E38, Word);  // no comparable synthetic case yet
