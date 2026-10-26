// Hot batch 155: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_155 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// field_into_frame(this, other) with the member's callee and offsets: callee(this, other->[outer]->[inner], a
// scratch block on the stack); this back.
void field_into_frame(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t outer,
                      std::uint32_t inner) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = word(rt, ctx.gpr[5] + outer);
    call_set(rt, ctx, callee, {self, word(rt, ctx.gpr[2] + inner), frame.at(0u)});
    ctx.gpr[2] = self;
}

// slot_present(this) with the member's key: i = func_08B5C2B0(key); whether i is below the count at (*this)->+8 and
// the word i of the array at (*this)->+4 is non-zero.
void slot_present(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t index = call_set(rt, ctx, 0x08B5C2B0u, {key}).v0;
    const std::uint32_t table = word(rt, self);
    ctx.gpr[5] = index;
    ctx.gpr[3] = word(rt, table + 4u);
    if (index >= word(rt, table + 8u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[3] = word(rt, ctx.gpr[3] + index * 4u);
    ctx.gpr[2] = ctx.gpr[3] != 0u ? 1u : 0u;
}

// float_name_assign(this, _, other) with the member's message, slot, converter and maker: o = *other (null:
// func_08A5BB7C(message) first), t = maker(converter(o's float virtual at that slot (o))); unless this is t itself,
// func_08A5BAD8(this, t's word); t destroyed (func_08A5B9EC; its v0 back).
void float_name_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message, std::uint32_t slot,
                       std::uint32_t converter, std::uint32_t maker) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t object = word(rt, ctx.gpr[6]);
    ctx.gpr[4] = message;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    const GuestResult got = call_indirect(rt, ctx, ctx.gpr[2], {object}, {});
    ctx.gpr[2] = got.v0;
    ctx.gpr[3] = got.v1;
    ctx.fpr[0] = got.f0;
    const float converted = call_set(rt, ctx, converter, {}, {got.f0}).f0;
    call_set(rt, ctx, maker, {temporary}, {converted});
    ctx.gpr[4] = self;
    if (self != temporary) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// named_member_refresh(this) with the member's name, getter, field, table and callback: a std::string s(name) on the
// stack is handed to this's virtual slot 0x108 (this, &s) and released (libstdc++'s COW rep: refcount at -4, the
// shared empty rep 0x08D74FD0 never counted); o = getter() (non-null: func_08A5BA08(o), a reference taken); the
// old this->[field] (non-null: func_08A5B930, released) is replaced by o; then func_08B8850C(this, table, callback).
void named_member_refresh(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t name, std::uint32_t getter,
                          std::uint32_t field, std::uint32_t table, std::uint32_t callback) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t string = frame.at(4u);
    const std::uint32_t method = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[2] = word(rt, self);
    call_set(rt, ctx, 0x08B85EB8u, {string, name, frame.at(1u)});  // `jalr $s1`: v0 stays the constructor's
    const GuestResult handed = call_guest(rt, ctx, method, {self, string}, {});
    ctx.gpr[2] = handed.v0;
    ctx.gpr[3] = handed.v1;
    ctx.fpr[0] = handed.f0;
    const std::uint32_t chars = word(rt, string);
    const std::uint32_t rep = chars - 12u;
    ctx.gpr[3] = chars;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[4] = chars - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep != 0x08D74FD0u) {
        const std::uint32_t old = call_set(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu}).v0;
        ctx.gpr[4] = rep;
        ctx.gpr[5] = frame.at(0u);
        if (static_cast<std::int32_t>(old) <= 0) call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
    }
    const std::uint32_t object = call_set(rt, ctx, getter, {}).v0;
    ctx.gpr[4] = object;
    if (object != 0u) call_set(rt, ctx, 0x08A5BA08u, {object});
    const std::uint32_t old_object = word(rt, self + field);
    ctx.gpr[2] = old_object;
    ctx.gpr[4] = old_object;
    if (old_object != 0u) call_set(rt, ctx, 0x08A5B930u, {old_object});
    put(rt, self + field, object);
    call_set(rt, ctx, 0x08B8850Cu, {self, table, callback});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 155 rows below).
void func_0880DA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08C98650u, 0x08965B18u, 0x38u, 0x08CF6F14u, 0x0880D99Cu); }
void func_08C61274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x14u); }
void func_08C5B09C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D721C8u); }
void func_08AA35E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B662C4u, 0x08A87EECu); }

// Batch 155 rows (tools.family_rows).
void func_08C61238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x8u); }
void func_08C612B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x1Cu); }
void func_08C612EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x24u); }
void func_08C61328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x8u); }
void func_08C61364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x14u); }
void func_08C613A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x1Cu); }
void func_08C613DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x24u); }
void func_08C61418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x8u); }
void func_08C61454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x14u); }
void func_08C61490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_into_frame(rt, ctx, 0x08C74A68u, 8u, 0x1Cu); }
void func_08C5B014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D721D4u); }
void func_08C5B124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77468u); }
void func_08C5B1AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77484u); }
void func_08C5B234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D7747Cu); }
void func_08C5B2BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77480u); }
void func_08C5B344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77494u); }
void func_08C5B3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77488u); }
void func_08C5B454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D7748Cu); }
void func_08C5B4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77478u); }
void func_08C5B564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77474u); }
void func_08C5B5EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D77470u); }
void func_08C5B674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_present(rt, ctx, 0x08D7746Cu); }
void func_08AA3568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B66524u, 0x08A87EECu); }
void func_08AA3660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B66604u, 0x08A87EECu); }
void func_08AA36DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B66E88u, 0x08A87EECu); }
void func_08AA3758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B66D54u, 0x08A87EECu); }
void func_08AA37D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B65F0Cu, 0x08A87EECu); }
void func_08AA3C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B67418u, 0x08A87EECu); }
void func_08AA3CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B6759Cu, 0x08A87EECu); }
void func_08AA3D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B661E8u, 0x08A87EECu); }
void func_08AA3DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B663B4u, 0x08A87EECu); }
void func_08AA3E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B664A0u, 0x08A87EECu); }
void func_08AA3EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_name_assign(rt, ctx, 0x08CB2BB8u, 0x54u, 0x08B66684u, 0x08A87EECu); }
void func_0880A924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08C982DCu, 0x08965B18u, 0x38u, 0x08CF6F14u, 0x0880A864u); }
void func_08976BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA2EBCu, 0x08993234u, 0x38u, 0x08CF6F14u, 0x08976B24u); }
void func_08977108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA2F28u, 0x089FE414u, 0x38u, 0x08CF6F14u, 0x08977048u); }
void func_089921F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA4030u, 0x089FF128u, 0x38u, 0x08CF6F14u, 0x08992130u); }
void func_08992E7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA40A8u, 0x08A0CF80u, 0x38u, 0x08CF6F14u, 0x08993068u); }
void func_089A2A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA4BD4u, 0x08A0CF80u, 0x38u, 0x08CF6F14u, 0x089A29A4u); }
void func_089B56F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA58ACu, 0x089FF128u, 0x38u, 0x08CF6F14u, 0x089B5638u); }
void func_089CAEA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA674Cu, 0x08A0CF80u, 0x38u, 0x08CF6F14u, 0x089CBB98u); }
void func_089ED3D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA7D54u, 0x08993234u, 0x38u, 0x08CF6F14u, 0x089ED314u); }
void func_089ED8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA7DC0u, 0x089FE414u, 0x38u, 0x08CF6F14u, 0x089ED838u); }
void func_089FC7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA870Cu, 0x08A0CF80u, 0x38u, 0x08CF6F14u, 0x089FC710u); }
void func_08A0398C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA8CF8u, 0x08A003B4u, 0x38u, 0x08CF6F14u, 0x08A038CCu); }
void func_08A0C338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA9358u, 0x08A0C918u, 0x38u, 0x08CF6F14u, 0x08A0C424u); }
void func_08A0C794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CA93D8u, 0x08A0C918u, 0x38u, 0x08CF6F14u, 0x08A0C880u); }
void func_08A2A50C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CABC1Cu, 0x08A76A94u, 0x38u, 0x08CF6F14u, 0x08A2A470u); }
void func_08A31FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CACB40u, 0x08A76A94u, 0x38u, 0x08CF6F14u, 0x08A32224u); }
void func_08A51FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAF428u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A520BCu); }
void func_08A52494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAF4A0u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A52580u); }
void func_08A55BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAF740u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A55CA4u); }
void func_08A594A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAF980u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A59594u); }
void func_08A5996C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAF9F8u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A59A58u); }
void func_08A59E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAFA70u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A59F20u); }
void func_08A5A2FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CAFAE8u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08A5A3E8u); }
void func_08A8E554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CB1D84u, 0x08A76A94u, 0x38u, 0x08CF6F14u, 0x08A8C6E4u); }
void func_08AABE14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CB322Cu, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08AABF00u); }
void func_08AAC2DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CB32A8u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08AAC3C8u); }
void func_08AAC7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { named_member_refresh(rt, ctx, 0x08CB3320u, 0x08A5261Cu, 0x38u, 0x08CF6F14u, 0x08AAC894u); }

} // namespace hot_batch_155
} // namespace gt::game

#define HOT155(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_155::name, gt::ReturnKind::kind)
HOT155(0x08C61274u, func_08C61274, Word);
HOT155(0x08C5B09Cu, func_08C5B09C, Word);
HOT155(0x08AA35E4u, func_08AA35E4, Word);
HOT155(0x0880DA5Cu, func_0880DA5C, Word);
HOT155(0x08C61238u, func_08C61238, Word);
HOT155(0x08C612B0u, func_08C612B0, Word);
HOT155(0x08C612ECu, func_08C612EC, Word);
HOT155(0x08C61328u, func_08C61328, Word);
HOT155(0x08C61364u, func_08C61364, Word);
HOT155(0x08C613A0u, func_08C613A0, Word);
HOT155(0x08C613DCu, func_08C613DC, Word);
HOT155(0x08C61418u, func_08C61418, Word);
HOT155(0x08C61454u, func_08C61454, Word);
HOT155(0x08C61490u, func_08C61490, Word);
HOT155(0x08C5B014u, func_08C5B014, Word);
HOT155(0x08C5B124u, func_08C5B124, Word);
HOT155(0x08C5B1ACu, func_08C5B1AC, Word);
HOT155(0x08C5B234u, func_08C5B234, Word);
HOT155(0x08C5B2BCu, func_08C5B2BC, Word);
HOT155(0x08C5B344u, func_08C5B344, Word);
HOT155(0x08C5B3CCu, func_08C5B3CC, Word);
HOT155(0x08C5B454u, func_08C5B454, Word);
HOT155(0x08C5B4DCu, func_08C5B4DC, Word);
HOT155(0x08C5B564u, func_08C5B564, Word);
HOT155(0x08C5B5ECu, func_08C5B5EC, Word);
HOT155(0x08C5B674u, func_08C5B674, Word);
// HOT155(0x08AA3568u, func_08AA3568, Word);  // no comparable synthetic case yet
HOT155(0x08AA3660u, func_08AA3660, Word);
HOT155(0x08AA36DCu, func_08AA36DC, Word);
// HOT155(0x08AA3758u, func_08AA3758, Word);  // no comparable synthetic case yet
// HOT155(0x08AA37D4u, func_08AA37D4, Word);  // no comparable synthetic case yet
HOT155(0x08AA3C5Cu, func_08AA3C5C, Word);
// HOT155(0x08AA3CD8u, func_08AA3CD8, Word);  // no comparable synthetic case yet
HOT155(0x08AA3D54u, func_08AA3D54, Word);
// HOT155(0x08AA3DD0u, func_08AA3DD0, Word);  // no comparable synthetic case yet
// HOT155(0x08AA3E4Cu, func_08AA3E4C, Word);  // no comparable synthetic case yet
HOT155(0x08AA3EC8u, func_08AA3EC8, Word);
HOT155(0x0880A924u, func_0880A924, Word);
HOT155(0x08976BE4u, func_08976BE4, Word);
HOT155(0x08977108u, func_08977108, Word);
HOT155(0x089921F0u, func_089921F0, Word);
HOT155(0x08992E7Cu, func_08992E7C, Word);
HOT155(0x089A2A64u, func_089A2A64, Word);
HOT155(0x089B56F8u, func_089B56F8, Word);
HOT155(0x089CAEA0u, func_089CAEA0, Word);
HOT155(0x089ED3D4u, func_089ED3D4, Word);
HOT155(0x089ED8F8u, func_089ED8F8, Word);
HOT155(0x089FC7D0u, func_089FC7D0, Word);
HOT155(0x08A0398Cu, func_08A0398C, Word);
HOT155(0x08A0C338u, func_08A0C338, Word);
HOT155(0x08A0C794u, func_08A0C794, Word);
HOT155(0x08A2A50Cu, func_08A2A50C, Word);
HOT155(0x08A31FD8u, func_08A31FD8, Word);
HOT155(0x08A51FD0u, func_08A51FD0, Word);
HOT155(0x08A52494u, func_08A52494, Word);
HOT155(0x08A55BB8u, func_08A55BB8, Word);
HOT155(0x08A594A8u, func_08A594A8, Word);
HOT155(0x08A5996Cu, func_08A5996C, Word);
HOT155(0x08A59E34u, func_08A59E34, Word);
HOT155(0x08A5A2FCu, func_08A5A2FC, Word);
HOT155(0x08A8E554u, func_08A8E554, Word);
HOT155(0x08AABE14u, func_08AABE14, Word);
HOT155(0x08AAC2DCu, func_08AAC2DC, Word);
HOT155(0x08AAC7A8u, func_08AAC7A8, Word);
