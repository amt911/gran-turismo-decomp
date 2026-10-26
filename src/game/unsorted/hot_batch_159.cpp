// Hot batch 159: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name. The templates of these families are named functions elsewhere,
// so each wrapper's template here is a pending member.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_159 {
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

// held_chain() with the member's maker, message, getter and final: a holder h = maker(&h) (the arguments as they
// came), its object o (null: func_08A5BB7C(message) first), v = getter(o), h destroyed (func_08A5B9EC), then final(v);
// its v0 back.
void held_chain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker, std::uint32_t message,
                std::uint32_t getter, std::uint32_t final) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    call_set(rt, ctx, maker, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = message;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    const std::uint32_t value = call_set(rt, ctx, getter, {object}).v0;
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
    call_set(rt, ctx, final, {value});
}

// call_with_ref(this, x) with the member's callee: callee(this, &x) (x spilled to the stack); this back.
void call_with_ref(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    call_set(rt, ctx, callee, {self, frame.at(0u)});
    ctx.gpr[2] = self;
}

// offset_call(this, p) with the member's callee and offset: callee(this, p + offset); this back.
void offset_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, callee, {self, ctx.gpr[5] + offset});
    ctx.gpr[2] = self;
}

// copy_words(first, last, out): std::copy of the words [first, last) to out, skipping the store while out is null (out
// still advances); the end of out back.
void copy_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t from = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    std::uint32_t out = ctx.gpr[6];
    if (from != last) {
        std::uint32_t next = from;
        do {
            next += 4u;
            if (out != 0u) {
                ctx.gpr[2] = word(rt, from);
                put(rt, out, ctx.gpr[2]);
            }
            from = next;
            out += 4u;
        } while (last != next);
        ctx.gpr[3] = next;
        ctx.gpr[4] = from;
    }
    ctx.gpr[7] = out;
    ctx.gpr[2] = out;
}

// field_virtual_tail(this, ...) with the member's field and slot: o = this->[field]; when set, o's virtual at that slot
// (o, ...) as a tail call (v0 = o's vtable, t9 = the target), else 0 back.
void field_virtual_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t field, std::uint32_t slot) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + field);
    ctx.gpr[2] = object;
    ctx.gpr[4] = object;
    if (object == 0u) return;
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + slot);
    tail_call(ctx, ctx.gpr[25], {object});
}

// constant_tail(...) with the member's argument and target: target(argument, ...) (tail call).
void constant_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t argument, std::uint32_t target) {
    tail_call(ctx, target, {argument});
}

// member_assign(this, other) with the member's offset: unless this + offset is other itself,
// func_08A5BAD8(this + offset, *other) (a smart-pointer assignment; tail call). v0 = this + offset.
void member_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t member = ctx.gpr[4] + offset;
    ctx.gpr[2] = member;
    ctx.gpr[4] = member;
    if (member == ctx.gpr[5]) return;
    tail_call(ctx, 0x08A5BAD8u, {member, word(rt, ctx.gpr[5])});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 159 rows below).
void func_0882994C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EEE0u); }
void func_08B85348(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_089B3484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A037A4u, 0x11Cu); }
void func_08BB13A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_0887D398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0x50u, 0x34u); }
void func_088C7908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08CE30D4u, 0x088C7844u); }
void func_08996F50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x180u); }

// Batch 159 rows (tools.family_rows).
void func_08829EB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EBB0u); }
void func_08829F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E414u); }
void func_08829F60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885F460u); }
void func_08829FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885F444u); }
void func_0882A010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EA48u); }
void func_0882A118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E8DCu); }
void func_0882A170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E878u); }
void func_0882A220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E7DCu); }
void func_0882A480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E764u); }
void func_0882A4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0886006Cu); }
void func_0882A588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x088622B8u); }
void func_0882A5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08863340u); }
void func_08847580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x088D5AFCu); }
void func_08847958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x088D4D24u); }
void func_088479B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x088D59B8u); }
void func_088548B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A719Cu); }
void func_08A1C3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { held_chain(rt, ctx, 0x08A1C370u, 0x08CAA224u, 0x08A1C0F4u, 0x08A1C0FCu); }
void func_08B85580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B855B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B86B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B87568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B87698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B888D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B88A80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B89C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B89D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08B8A224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08C1E01C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08C1E428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08C1FA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08C245C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_08C245F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_ref(rt, ctx, 0x08A76AACu); }
void func_089CC8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x17Cu); }
void func_089CC908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x180u); }
void func_089CC934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x184u); }
void func_089E4D5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x184u); }
void func_089E4D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x188u); }
void func_089E4DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x18Cu); }
void func_08A3A7E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x30u); }
void func_08A3A810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x34u); }
void func_08A89B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08B88F44u, 0x14u); }
void func_08AF32A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08B4E734u, 0x60u); }
void func_08AF32D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08B4E734u, 0x20u); }
void func_08BA53A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08B4E734u, 0x10u); }
void func_08C29B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x18u); }
void func_08C29B4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_call(rt, ctx, 0x08A76ABCu, 0x1Cu); }
void func_08BB1CAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08BB1CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08BF90D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08BF91E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C0A870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C1FC10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C2AA90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C2AAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C2BF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C3F618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C46F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_08C46FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_words(rt, ctx); }
void func_089EC798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0x10u, 0x11Cu); }
void func_08B9774C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08B97770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08B97794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08B995D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08BA3C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08BA62A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08BBD564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08BD362C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0xCu, 0x4u); }
void func_08C1A0F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0x14u, 0xCu); }
void func_08C1A168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_virtual_tail(rt, ctx, 0x14u, 0xCu); }
void func_088C97D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08CE3BD0u, 0x088C9630u); }
void func_089161D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08CF205Cu, 0x08916138u); }
void func_08916468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08CF205Cu, 0x08916408u); }
void func_08AC0B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08CFFB80u, 0x08B0E234u); }
void func_08AD0830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08D69170u, 0x08B3252Cu); }
void func_08B09760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08D473E8u, 0x08B344E0u); }
void func_08B09790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08D473E8u, 0x08B3457Cu); }
void func_08B0AA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08D4734Cu, 0x08B0A9F4u); }
void func_08B330D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant_tail(rt, ctx, 0x08D691B4u, 0x08B33080u); }
void func_0899BB04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x38u); }
void func_0899C2B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x28u); }
void func_089D2EA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x17Cu); }
void func_089D2FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x180u); }
void func_089D30B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x184u); }
void func_089EBB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x184u); }
void func_089EBC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x188u); }
void func_089EBD10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x18Cu); }
void func_089F1060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x1Cu); }
void func_08A34244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_assign(rt, ctx, 0x18u); }

} // namespace hot_batch_159
} // namespace gt::game

#define HOT159(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_159::name, gt::ReturnKind::kind)
// HOT159(0x0882994Cu, func_0882994C, Word);  // no comparable synthetic case yet
HOT159(0x08B85348u, func_08B85348, Word);
HOT159(0x089B3484u, func_089B3484, Word);
HOT159(0x08BB13A4u, func_08BB13A4, Word);
HOT159(0x0887D398u, func_0887D398, Word);
HOT159(0x088C7908u, func_088C7908, Word);
HOT159(0x08996F50u, func_08996F50, Word);
// HOT159(0x08829EB0u, func_08829EB0, Word);  // no comparable synthetic case yet
// HOT159(0x08829F08u, func_08829F08, Word);  // no comparable synthetic case yet
// HOT159(0x08829F60u, func_08829F60, Word);  // no comparable synthetic case yet
// HOT159(0x08829FB8u, func_08829FB8, Word);  // no comparable synthetic case yet
// HOT159(0x0882A010u, func_0882A010, Word);  // no comparable synthetic case yet
// HOT159(0x0882A118u, func_0882A118, Word);  // no comparable synthetic case yet
// HOT159(0x0882A170u, func_0882A170, Word);  // no comparable synthetic case yet
// HOT159(0x0882A220u, func_0882A220, Word);  // no comparable synthetic case yet
// HOT159(0x0882A480u, func_0882A480, Word);  // no comparable synthetic case yet
// HOT159(0x0882A4D8u, func_0882A4D8, Word);  // no comparable synthetic case yet
// HOT159(0x0882A588u, func_0882A588, Word);  // no comparable synthetic case yet
// HOT159(0x0882A5E0u, func_0882A5E0, Word);  // no comparable synthetic case yet
// HOT159(0x08847580u, func_08847580, Word);  // no comparable synthetic case yet
// HOT159(0x08847958u, func_08847958, Word);  // no comparable synthetic case yet
// HOT159(0x088479B0u, func_088479B0, Word);  // no comparable synthetic case yet
// HOT159(0x088548B0u, func_088548B0, Word);  // no comparable synthetic case yet
// HOT159(0x08A1C3B0u, func_08A1C3B0, Word);  // no comparable synthetic case yet
HOT159(0x08B85580u, func_08B85580, Word);
HOT159(0x08B855B0u, func_08B855B0, Word);
HOT159(0x08B86B38u, func_08B86B38, Word);
HOT159(0x08B87568u, func_08B87568, Word);
HOT159(0x08B87698u, func_08B87698, Word);
HOT159(0x08B888D4u, func_08B888D4, Word);
HOT159(0x08B88A80u, func_08B88A80, Word);
HOT159(0x08B89C58u, func_08B89C58, Word);
HOT159(0x08B89D7Cu, func_08B89D7C, Word);
HOT159(0x08B8A224u, func_08B8A224, Word);
HOT159(0x08C1E01Cu, func_08C1E01C, Word);
HOT159(0x08C1E428u, func_08C1E428, Word);
HOT159(0x08C1FA00u, func_08C1FA00, Word);
HOT159(0x08C245C0u, func_08C245C0, Word);
HOT159(0x08C245F0u, func_08C245F0, Word);
HOT159(0x089CC8DCu, func_089CC8DC, Word);
HOT159(0x089CC908u, func_089CC908, Word);
HOT159(0x089CC934u, func_089CC934, Word);
HOT159(0x089E4D5Cu, func_089E4D5C, Word);
HOT159(0x089E4D88u, func_089E4D88, Word);
HOT159(0x089E4DB4u, func_089E4DB4, Word);
HOT159(0x08A3A7E4u, func_08A3A7E4, Word);
HOT159(0x08A3A810u, func_08A3A810, Word);
HOT159(0x08A89B20u, func_08A89B20, Word);
HOT159(0x08AF32A8u, func_08AF32A8, Word);
HOT159(0x08AF32D4u, func_08AF32D4, Word);
HOT159(0x08BA53A0u, func_08BA53A0, Word);
HOT159(0x08C29B20u, func_08C29B20, Word);
HOT159(0x08C29B4Cu, func_08C29B4C, Word);
HOT159(0x08BB1CACu, func_08BB1CAC, Word);
HOT159(0x08BB1CDCu, func_08BB1CDC, Word);
HOT159(0x08BF90D4u, func_08BF90D4, Word);
HOT159(0x08BF91E8u, func_08BF91E8, Word);
HOT159(0x08C0A870u, func_08C0A870, Word);
HOT159(0x08C1FC10u, func_08C1FC10, Word);
HOT159(0x08C2AA90u, func_08C2AA90, Word);
HOT159(0x08C2AAD0u, func_08C2AAD0, Word);
HOT159(0x08C2BF70u, func_08C2BF70, Word);
HOT159(0x08C3F618u, func_08C3F618, Word);
HOT159(0x08C46F64u, func_08C46F64, Word);
HOT159(0x08C46FE4u, func_08C46FE4, Word);
HOT159(0x089EC798u, func_089EC798, Word);
HOT159(0x08B9774Cu, func_08B9774C, Word);
HOT159(0x08B97770u, func_08B97770, Word);
HOT159(0x08B97794u, func_08B97794, Word);
HOT159(0x08B995D0u, func_08B995D0, Word);
HOT159(0x08BA3C50u, func_08BA3C50, Word);
HOT159(0x08BA62A8u, func_08BA62A8, Word);
HOT159(0x08BBD564u, func_08BBD564, Word);
HOT159(0x08BD362Cu, func_08BD362C, Word);
HOT159(0x08C1A0F0u, func_08C1A0F0, Word);
HOT159(0x08C1A168u, func_08C1A168, Word);
HOT159(0x088C97D0u, func_088C97D0, Word);
HOT159(0x089161D0u, func_089161D0, Word);
HOT159(0x08916468u, func_08916468, Word);
// HOT159(0x08AC0B5Cu, func_08AC0B5C, Word);  // no comparable synthetic case yet
// HOT159(0x08AD0830u, func_08AD0830, Word);  // no comparable synthetic case yet
HOT159(0x08B09760u, func_08B09760, Word);
HOT159(0x08B09790u, func_08B09790, Word);
HOT159(0x08B0AA68u, func_08B0AA68, Word);
HOT159(0x08B330D4u, func_08B330D4, Word);
HOT159(0x0899BB04u, func_0899BB04, Word);
HOT159(0x0899C2B0u, func_0899C2B0, Word);
HOT159(0x089D2EA8u, func_089D2EA8, Word);
HOT159(0x089D2FB0u, func_089D2FB0, Word);
HOT159(0x089D30B8u, func_089D30B8, Word);
HOT159(0x089EBB00u, func_089EBB00, Word);
HOT159(0x089EBC08u, func_089EBC08, Word);
HOT159(0x089EBD10u, func_089EBD10, Word);
HOT159(0x089F1060u, func_089F1060, Word);
HOT159(0x08A34244u, func_08A34244, Word);
