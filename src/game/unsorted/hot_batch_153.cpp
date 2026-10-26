// Hot batch 153: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name. The templates of these families are named functions elsewhere,
// so each wrapper's template here is a pending member.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_153 {
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

// deref_word(this) with the member's offsets: the word at this->[outer] + offset (v1 = this->[outer]).
void deref_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t outer, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + outer);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + offset);
}

// held_object_call() with the member's maker, message and callee: a holder h = maker(&h) (the arguments as they came),
// its object o (null: func_08A5BB7C(message) first), callee(o), h destroyed (func_08A5B9EC; its v0 back).
void held_object_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker, std::uint32_t message,
                      std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, maker, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = message;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    call_set(rt, ctx, callee, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// virtual_on_second(this, other, ...) with the member's vtable slot: other's virtual at that slot with the arguments as
// they came (a0 = this); this back.
void virtual_on_second(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t table = word(rt, ctx.gpr[5]);
    const std::uint32_t target = word(rt, table + slot);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_indirect(rt, ctx, target, {self}, {});
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[2] = self;
}

// name_assign(this) with the member's source and maker: t = maker(source(this) (the arguments as they came)); unless
// this is t itself, func_08A5BAD8(this, t's word); t destroyed (func_08A5B9EC; its v0 back).
void name_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t source, std::uint32_t maker) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t value = call_set(rt, ctx, source, {self}).v0;
    call_set(rt, ctx, maker, {temporary, value});
    ctx.gpr[4] = self;
    if (self != temporary) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// virtual_name_assign(this, other) with the member's message, slot and maker: o = *other (null:
// func_08A5BB7C(message) first), t = maker(o's virtual at that slot (o)); unless this is t itself,
// func_08A5BAD8(this, t's word); t destroyed (func_08A5B9EC; its v0 back).
void virtual_name_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t slot,
                         std::uint32_t maker) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    ctx.gpr[4] = message;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    const GuestResult got = call_indirect(rt, ctx, ctx.gpr[2], {object}, {});
    ctx.gpr[2] = got.v0;
    ctx.gpr[3] = got.v1;
    ctx.fpr[0] = got.f0;
    call_set(rt, ctx, maker, {temporary, got.v0});
    ctx.gpr[4] = self;
    if (self != temporary) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 153 rows below).
void func_08979A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08978000u, 0x08CA2F94u, 0x0897999Cu); }
void func_08A527A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x128u); }
void func_08A789D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x14u, 0x08A85B1Cu); }
void func_088214CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x088A364Cu, 0x08A85B1Cu); }

// Batch 153 rows (tools.family_rows).
void func_0886B08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 124u); }
void func_0886B0A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 108u); }
void func_0886B0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 112u); }
void func_0886B2F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 12u); }
void func_08885268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 12u, 72u); }
void func_08917A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 12u, 8128u); }
void func_0892BB04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 608u, 152u); }
void func_08A5D454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 12u, 4u); }
void func_08A6BFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 20u, -12u); }
void func_08A8BFE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 80u, 28u); }
void func_08AF1248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 688u); }
void func_08AF1298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_word(rt, ctx, 4u, 700u); }
void func_0897DB90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x0897DB50u, 0x08CA334Cu, 0x0897D738u); }
void func_0897E948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x0897E2E0u); }
void func_0897E9E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x0897E404u); }
void func_08990144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x0899000Cu); }
void func_089A415C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089A354Cu, 0x08CA4D28u, 0x089A4070u); }
void func_089A4D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089A354Cu, 0x08CA4D28u, 0x089A4CBCu); }
void func_089A53B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089A354Cu, 0x08CA4D28u, 0x089A5360u); }
void func_089BF92C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089BEE4Cu, 0x08CA5EB4u, 0x089BF8ECu); }
void func_089C11D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089BEE4Cu, 0x08CA5EB4u, 0x089C10C8u); }
void func_089C2278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089C2238u, 0x08CA6164u, 0x089C1AE0u); }
void func_089C22C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089C2238u, 0x08CA6164u, 0x08A0F16Cu); }
void func_089C5CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x089C5850u, 0x08CA63A0u, 0x089C4FDCu); }
void func_08A146EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A14610u, 0x08CA9A6Cu, 0x08A14418u); }
void func_08A14FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A14610u, 0x08CA9A6Cu, 0x08A14D94u); }
void func_08A20D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A20CC8u, 0x08CAA7F4u, 0x08A20858u); }
void func_08A23D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A23C8Cu, 0x08CAACE0u, 0x08A23A34u); }
void func_08A23DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A23C8Cu, 0x08CAACE0u, 0x08A23BA0u); }
void func_08A34404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A343C4u, 0x08CACE80u, 0x08A3412Cu); }
void func_08A34450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A343C4u, 0x08CACE80u, 0x08A3420Cu); }
void func_08A355FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A354ECu, 0x08CACF04u, 0x08A35114u); }
void func_08A35694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A354ECu, 0x08CACF04u, 0x08A3533Cu); }
void func_08A39230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A391F0u, 0x08CAD378u, 0x08A38BD4u); }
void func_08A3927C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A391F0u, 0x08CAD378u, 0x08A38B14u); }
void func_08A3ABF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A3A9D0u, 0x08CAD544u, 0x08A3A6FCu); }
void func_08A3F7AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_object_call(rt, ctx, 0x08A3F76Cu, 0x08CADA50u, 0x08A3EC10u); }
void func_08C5894C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x10u); }
void func_08C5897C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x14u); }
void func_08C589AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x18u); }
void func_08C589DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x1Cu); }
void func_08C58BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x10u); }
void func_08C58C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x14u); }
void func_08C58C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x18u); }
void func_08C58C74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x1Cu); }
void func_08C59514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x10u); }
void func_08C59544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x14u); }
void func_08C59574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0x18u); }
void func_08C5A5B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0xCu); }
void func_08C5A9F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_on_second(rt, ctx, 0xCu); }
void func_0883BA58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08936408u, 0x08A85B1Cu); }
void func_0885576C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x088A6C78u, 0x08A85B1Cu); }
void func_08A27460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B0DA60u, 0x08A97A6Cu); }
void func_08A274A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B0D790u, 0x08A85B1Cu); }
void func_08A274F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B0D510u, 0x08A85B1Cu); }
void func_08A277C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B0D958u, 0x08A97A6Cu); }
void func_08A27F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08ABD79Cu, 0x08A85B1Cu); }
void func_08A280D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08ABD724u, 0x08A85B1Cu); }
void func_08A2F4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B0AA68u, 0x08A85B1Cu); }
void func_08A3F6C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B213F8u, 0x08A85B1Cu); }
void func_08A4C538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B1A7A4u, 0x08A85B1Cu); }
void func_08A4C7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B35444u, 0x08A5FA8Cu); }
void func_08A4C800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_assign(rt, ctx, 0x08B35418u, 0x08A5FA8Cu); }
void func_08A78DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x34u, 0x08A960C0u); }
void func_08A78E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x38u, 0x08A7334Cu); }
void func_08A78FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x44u, 0x08AB19B0u); }
void func_08A79040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x48u, 0x08AB022Cu); }
void func_08A790B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0x4Cu, 0x08A5FA8Cu); }
void func_08A7972C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB0E38u, 0xD8u, 0x08A85B1Cu); }
void func_08AB4CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_name_assign(rt, ctx, 0x08CB3E0Cu, 0x3Cu, 0x08A85B1Cu); }

} // namespace hot_batch_153
} // namespace gt::game

#define HOT153(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_153::name, gt::ReturnKind::kind)
// HOT153(0x08979A20u, func_08979A20, Word);  // no comparable synthetic case yet
HOT153(0x08A527A8u, func_08A527A8, Word);
HOT153(0x088214CCu, func_088214CC, Word);
HOT153(0x08A789D8u, func_08A789D8, Word);
HOT153(0x0886B08Cu, func_0886B08C, Word);
HOT153(0x0886B0A4u, func_0886B0A4, Word);
HOT153(0x0886B0C8u, func_0886B0C8, Word);
HOT153(0x0886B2F4u, func_0886B2F4, Word);
HOT153(0x08885268u, func_08885268, Word);
HOT153(0x08917A98u, func_08917A98, Word);
HOT153(0x0892BB04u, func_0892BB04, Word);
HOT153(0x08A5D454u, func_08A5D454, Word);
HOT153(0x08A6BFCCu, func_08A6BFCC, Word);
HOT153(0x08A8BFE8u, func_08A8BFE8, Word);
HOT153(0x08AF1248u, func_08AF1248, Word);
HOT153(0x08AF1298u, func_08AF1298, Word);
// HOT153(0x0897DB90u, func_0897DB90, Word);  // no comparable synthetic case yet
// HOT153(0x0897E948u, func_0897E948, Word);  // no comparable synthetic case yet
// HOT153(0x0897E9E0u, func_0897E9E0, Word);  // no comparable synthetic case yet
// HOT153(0x08990144u, func_08990144, Word);  // no comparable synthetic case yet
// HOT153(0x089A415Cu, func_089A415C, Word);  // no comparable synthetic case yet
// HOT153(0x089A4D6Cu, func_089A4D6C, Word);  // no comparable synthetic case yet
// HOT153(0x089A53B0u, func_089A53B0, Word);  // no comparable synthetic case yet
// HOT153(0x089BF92Cu, func_089BF92C, Word);  // no comparable synthetic case yet
// HOT153(0x089C11D0u, func_089C11D0, Word);  // no comparable synthetic case yet
// HOT153(0x089C2278u, func_089C2278, Word);  // no comparable synthetic case yet
// HOT153(0x089C22C4u, func_089C22C4, Word);  // no comparable synthetic case yet
// HOT153(0x089C5CB8u, func_089C5CB8, Word);  // no comparable synthetic case yet
// HOT153(0x08A146ECu, func_08A146EC, Word);  // no comparable synthetic case yet
// HOT153(0x08A14FF0u, func_08A14FF0, Word);  // no comparable synthetic case yet
// HOT153(0x08A20D48u, func_08A20D48, Word);  // no comparable synthetic case yet
// HOT153(0x08A23D78u, func_08A23D78, Word);  // no comparable synthetic case yet
// HOT153(0x08A23DC4u, func_08A23DC4, Word);  // no comparable synthetic case yet
// HOT153(0x08A34404u, func_08A34404, Word);  // no comparable synthetic case yet
// HOT153(0x08A34450u, func_08A34450, Word);  // no comparable synthetic case yet
// HOT153(0x08A355FCu, func_08A355FC, Word);  // no comparable synthetic case yet
// HOT153(0x08A35694u, func_08A35694, Word);  // no comparable synthetic case yet
// HOT153(0x08A39230u, func_08A39230, Word);  // no comparable synthetic case yet
// HOT153(0x08A3927Cu, func_08A3927C, Word);  // no comparable synthetic case yet
// HOT153(0x08A3ABF4u, func_08A3ABF4, Word);  // no comparable synthetic case yet
// HOT153(0x08A3F7ACu, func_08A3F7AC, Word);  // no comparable synthetic case yet
HOT153(0x08C5894Cu, func_08C5894C, Word);
HOT153(0x08C5897Cu, func_08C5897C, Word);
HOT153(0x08C589ACu, func_08C589AC, Word);
HOT153(0x08C589DCu, func_08C589DC, Word);
HOT153(0x08C58BE4u, func_08C58BE4, Word);
HOT153(0x08C58C14u, func_08C58C14, Word);
HOT153(0x08C58C44u, func_08C58C44, Word);
HOT153(0x08C58C74u, func_08C58C74, Word);
HOT153(0x08C59514u, func_08C59514, Word);
HOT153(0x08C59544u, func_08C59544, Word);
HOT153(0x08C59574u, func_08C59574, Word);
HOT153(0x08C5A5B4u, func_08C5A5B4, Word);
HOT153(0x08C5A9F0u, func_08C5A9F0, Word);
HOT153(0x0883BA58u, func_0883BA58, Word);
HOT153(0x0885576Cu, func_0885576C, Word);
// HOT153(0x08A27460u, func_08A27460, Word);  // no comparable synthetic case yet
// HOT153(0x08A274A8u, func_08A274A8, Word);  // no comparable synthetic case yet
// HOT153(0x08A274F0u, func_08A274F0, Word);  // no comparable synthetic case yet
HOT153(0x08A277C0u, func_08A277C0, Word);
HOT153(0x08A27F68u, func_08A27F68, Word);
HOT153(0x08A280D4u, func_08A280D4, Word);
HOT153(0x08A2F4E8u, func_08A2F4E8, Word);
HOT153(0x08A3F6C4u, func_08A3F6C4, Word);
HOT153(0x08A4C538u, func_08A4C538, Word);
// HOT153(0x08A4C7B8u, func_08A4C7B8, Word);  // no comparable synthetic case yet
HOT153(0x08A4C800u, func_08A4C800, Word);
HOT153(0x08A78DF8u, func_08A78DF8, Word);
// HOT153(0x08A78E6Cu, func_08A78E6C, Word);  // no comparable synthetic case yet
// HOT153(0x08A78FCCu, func_08A78FCC, Word);  // no comparable synthetic case yet
// HOT153(0x08A79040u, func_08A79040, Word);  // no comparable synthetic case yet
// HOT153(0x08A790B4u, func_08A790B4, Word);  // no comparable synthetic case yet
// HOT153(0x08A7972Cu, func_08A7972C, Word);  // no comparable synthetic case yet
HOT153(0x08AB4CECu, func_08AB4CEC, Word);
