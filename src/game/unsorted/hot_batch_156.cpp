// Hot batch 156: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name. Both families are script-property accessors: (result, _, mode,
// value) reads a byte of a game object into a script value (mode 0 / not positive) or writes it from one.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_156 {
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

// The script value's virtual slot 0x3C (its truth) of *value_holder; null: func_08A5BB7C(message) first.
std::uint32_t value_truth(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t value_holder,
                          std::uint32_t message) {
    const std::uint32_t value = word(rt, value_holder);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    ctx.gpr[3] = word(rt, value);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const GuestResult truth = call_indirect(rt, ctx, ctx.gpr[2], {value}, {});
    ctx.gpr[2] = truth.v0;
    ctx.gpr[3] = truth.v1;
    ctx.fpr[0] = truth.f0;
    return truth.v0;
}

// Assigns a fresh script value made by func_08A85B1C(&t, byte) to *result (func_08A5BAD8, unless result is t itself);
// t destroyed (func_08A5B9EC).
void assign_byte_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t temporary,
                       std::uint32_t byte) {
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, byte});
    ctx.gpr[5] = word(rt, temporary);
    if (result != temporary) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// managed_byte_property(result, _, mode, value) with the member's object path and field: the game object is
// lookup(convert(holder's object)) where the holder comes from maker (null object: func_08A5BB7C(message)); mode > 0
// writes value's truth into the byte at writer(object) + field, else the byte at reader(object) + field becomes the
// result.
void managed_byte_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker, std::uint32_t message,
                           std::uint32_t convert, std::uint32_t writer, std::uint32_t value_message, std::uint32_t field,
                           std::uint32_t reader) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t value_holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const bool write = static_cast<std::int32_t>(ctx.gpr[6]) > 0;
    const std::uint32_t holder = frame.at(write ? 8u : 4u);
    call_set(rt, ctx, maker, {holder});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    const std::uint32_t converted = call_set(rt, ctx, convert, {object}).v0;
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
    if (write) {
        const std::uint32_t target = call_set(rt, ctx, writer, {converted}).v0;
        const std::uint32_t truth = value_truth(rt, ctx, value_holder, value_message);
        rt.memory().store8(target + field, static_cast<std::uint8_t>(truth));
        return;
    }
    const std::uint32_t source = call_set(rt, ctx, reader, {converted}).v0;
    ctx.gpr[4] = result;
    assign_byte_value(rt, ctx, result, frame.at(0u), rt.memory().load8(source + field));
}

// flagged_byte_property(result, _, mode, value) with the member's maker, messages and offsets: o = the maker
// holder's object (null: func_08A5BB7C(message)); mode 0: the result becomes whether the byte o + field is set;
// mode 1: the byte o + base + set_field = value's truth and o + base + flag_field = 1; other modes do nothing (v0 1).
void flagged_byte_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker, std::uint32_t message,
                           std::uint32_t field, std::uint32_t value_message, std::uint32_t base, std::uint32_t flag_field,
                           std::uint32_t set_field) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t mode = ctx.gpr[6];
    const std::uint32_t value_holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(4u);
        call_set(rt, ctx, maker, {holder});
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        const std::uint32_t byte = rt.memory().load8(object + field);
        ctx.gpr[4] = frame.at(8u);
        assign_byte_value(rt, ctx, result, frame.at(8u), byte != 0u ? 1u : 0u);
        call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, maker, {holder});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    const std::uint32_t truth = value_truth(rt, ctx, value_holder, value_message) != 0u ? 1u : 0u;
    ctx.gpr[2] = truth;
    ctx.gpr[3] = object + base;
    rt.memory().store8(object + base + flag_field, static_cast<std::uint8_t>(mode));
    rt.memory().store8(object + base + set_field, static_cast<std::uint8_t>(truth));
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 156 rows below).
void func_08851C74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x13Bu, 0x088A65E8u); }
void func_08980A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x125u, 0x08CA3390u, 0xF0u, 0xC4u, 0x35u); }

// Batch 156 rows (tools.family_rows).
void func_08851B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x13Au, 0x088A65E8u); }
void func_08851D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x13Eu, 0x088A65E8u); }
void func_08851E7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x13Fu, 0x088A65E8u); }
void func_08851F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x140u, 0x088A65E8u); }
void func_08852084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x141u, 0x088A65E8u); }
void func_08852188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x142u, 0x088A65E8u); }
void func_0885228C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x143u, 0x088A65E8u); }
void func_08852390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x144u, 0x088A65E8u); }
void func_08852494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x145u, 0x088A65E8u); }
void func_088527A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x131u, 0x088A65E8u); }
void func_088528A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x132u, 0x088A65E8u); }
void func_088529A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x126u, 0x088A65E8u); }
void func_08852AAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x127u, 0x088A65E8u); }
void func_08852BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x128u, 0x088A65E8u); }
void func_08852CB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x129u, 0x088A65E8u); }
void func_08852DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x12Au, 0x088A65E8u); }
void func_08853BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x155u, 0x088A65E8u); }
void func_08853CF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x161u, 0x088A65E8u); }
void func_08853DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x162u, 0x088A65E8u); }
void func_08853EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { managed_byte_property(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x088A6608u, 0x08C9D394u, 0x163u, 0x088A65E8u); }
void func_0898097C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x124u, 0x08CA3390u, 0xF0u, 0xC4u, 0x34u); }
void func_08980B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x126u, 0x08CA3390u, 0xF0u, 0xC4u, 0x36u); }
void func_08980C94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x127u, 0x08CA3390u, 0xF0u, 0xC4u, 0x37u); }
void func_08981F34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x198u, 0x08CA3390u, 0xF0u, 0xC4u, 0xA8u); }
void func_0898203C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0897E844u, 0x08CA3400u, 0x199u, 0x08CA3390u, 0xF0u, 0xC4u, 0xA9u); }
void func_0898B79C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x124u, 0x08CA39E8u, 0xF0u, 0xC4u, 0x34u); }
void func_0898B8A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x125u, 0x08CA39E8u, 0xF0u, 0xC4u, 0x35u); }
void func_0898B9AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x126u, 0x08CA39E8u, 0xF0u, 0xC4u, 0x36u); }
void func_0898BAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x127u, 0x08CA39E8u, 0xF0u, 0xC4u, 0x37u); }
void func_0898CD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x198u, 0x08CA39E8u, 0xF0u, 0xC4u, 0xA8u); }
void func_0898CE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x089896CCu, 0x08CA39F0u, 0x199u, 0x08CA39E8u, 0xF0u, 0xC4u, 0xA9u); }
void func_0898E7B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x164u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0x34u); }
void func_0898E8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x165u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0x35u); }
void func_0898E9C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x166u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0x36u); }
void func_0898EAC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x167u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0x37u); }
void func_0898FD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x1D8u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0xA8u); }
void func_0898FE70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_byte_property(rt, ctx, 0x0898E170u, 0x08CA3CB8u, 0x1D9u, 0x08CA3C7Cu, 0x130u, 0xC4u, 0xA9u); }

} // namespace hot_batch_156
} // namespace gt::game

#define HOT156(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_156::name, gt::ReturnKind::kind)
// HOT156(0x08851C74u, func_08851C74, Word);  // no comparable synthetic case yet
HOT156(0x08980A84u, func_08980A84, Word);
// HOT156(0x08851B70u, func_08851B70, Word);  // no comparable synthetic case yet
// HOT156(0x08851D78u, func_08851D78, Word);  // no comparable synthetic case yet
// HOT156(0x08851E7Cu, func_08851E7C, Word);  // no comparable synthetic case yet
// HOT156(0x08851F80u, func_08851F80, Word);  // no comparable synthetic case yet
// HOT156(0x08852084u, func_08852084, Word);  // no comparable synthetic case yet
// HOT156(0x08852188u, func_08852188, Word);  // no comparable synthetic case yet
// HOT156(0x0885228Cu, func_0885228C, Word);  // no comparable synthetic case yet
// HOT156(0x08852390u, func_08852390, Word);  // no comparable synthetic case yet
// HOT156(0x08852494u, func_08852494, Word);  // no comparable synthetic case yet
// HOT156(0x088527A0u, func_088527A0, Word);  // no comparable synthetic case yet
// HOT156(0x088528A4u, func_088528A4, Word);  // no comparable synthetic case yet
// HOT156(0x088529A8u, func_088529A8, Word);  // no comparable synthetic case yet
// HOT156(0x08852AACu, func_08852AAC, Word);  // no comparable synthetic case yet
// HOT156(0x08852BB0u, func_08852BB0, Word);  // no comparable synthetic case yet
// HOT156(0x08852CB4u, func_08852CB4, Word);  // no comparable synthetic case yet
// HOT156(0x08852DB8u, func_08852DB8, Word);  // no comparable synthetic case yet
// HOT156(0x08853BF0u, func_08853BF0, Word);  // no comparable synthetic case yet
// HOT156(0x08853CF4u, func_08853CF4, Word);  // no comparable synthetic case yet
// HOT156(0x08853DF8u, func_08853DF8, Word);  // no comparable synthetic case yet
// HOT156(0x08853EFCu, func_08853EFC, Word);  // no comparable synthetic case yet
HOT156(0x0898097Cu, func_0898097C, Word);
HOT156(0x08980B8Cu, func_08980B8C, Word);
HOT156(0x08980C94u, func_08980C94, Word);
HOT156(0x08981F34u, func_08981F34, Word);
HOT156(0x0898203Cu, func_0898203C, Word);
HOT156(0x0898B79Cu, func_0898B79C, Word);
HOT156(0x0898B8A4u, func_0898B8A4, Word);
HOT156(0x0898B9ACu, func_0898B9AC, Word);
HOT156(0x0898BAB4u, func_0898BAB4, Word);
HOT156(0x0898CD54u, func_0898CD54, Word);
HOT156(0x0898CE5Cu, func_0898CE5C, Word);
HOT156(0x0898E7B0u, func_0898E7B0, Word);
HOT156(0x0898E8B8u, func_0898E8B8, Word);
HOT156(0x0898E9C0u, func_0898E9C0, Word);
HOT156(0x0898EAC8u, func_0898EAC8, Word);
HOT156(0x0898FD68u, func_0898FD68, Word);
HOT156(0x0898FE70u, func_0898FE70, Word);
