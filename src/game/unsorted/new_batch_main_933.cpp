// Main-session batch 933: functions drafted before that did not compare, rewritten from `tools.disasm`.
// Delay slots are read as "before the branch / call"; every exit path leaves v0 (and v1 where the asm sets it) as the
// original does; s0-s7 are set where the asm sets them and put back at the end. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_933 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them (a callee may read them) and the
// caller's values come back. f20-f31 are callee-saved too; the versions that touch them use SavedFpr.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

class SavedFpr {
public:
    explicit SavedFpr(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFpr() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFpr(const SavedFpr &) = delete;
    SavedFpr &operator=(const SavedFpr &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x088CFA9C (a0 = this, f12 / f13 = two floats): 0x088D390C(*(this + 0x61C), &first, &second, 6) reads a pair of
// floats, then 0x088D2DE0(*(this + 0x61C), f12 = the first argument minus the pair's word at +0x10) answers one, and
// f0 / f1 are its absolute value (c.lt.s against zero, so a NaN is left alone).

// 0x088E798C (a0 = this, a1 = a value, f13 = a float): 0x088D390C(*(this + 0x61C), &first, &second, 6) primes a pair,
// then 0x088E7894(this, value, &first, &second) runs with f12 = the float argument.

// 0x089CFD28 (a0 = this, a1 = a value, a2 = another): 0x08969188(&local, &word) prepares a stack pair (the word is the
// third argument), 0x089CDDF8(this, value, &local) runs, the local is released (0x08A5B9EC(&local)) and
// 0x089CFCCC(this, *(this + 0x198)) answers the state word stored at +0x1A0 (with +0x1C0 = 2, v1 = 2).

// 0x08A3DBDC (a0 = object): stores vtable 0x08CD13B0, 0x08A3D718(*(object + 0x14)) and the helper at +0x18 is released
// through its first virtual (with *(object + 0x14)), both words cleared; 0x08A780B0(object) runs and
// 0x08A5BC54(object, 0x1C, 4) finishes.

// 0x08A41414 (a0 passed on, a1 / a3 = two values): 0x08A40E90(&local, a3) and 0x08A40CF8(&object, a1) prepare a pair,
// an empty object is reported (0x08A5BB7C(0x08CADBCC)), 0x08A413B0(object, &local) runs and both locals are released.

// 0x08A56F08 (a0 = this, a1 = a holder, a2 = a value): the helper at *holder (reported when null through
// 0x08A5BB7C(0x08CAF778)) answers through slot 0x5C into a stack word; 0x08A565C0(this, &word, value) then
// 0x08A69D68(&word) release it.
void func_08A56F08(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    const u32 helper = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF778u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x5Cu);
    call_keep(rt, ctx, word(rt, table + 0x5Cu), {frame.at(0u), helper});
    call_set(rt, ctx, 0x08A565C0u, {self, frame.at(0u), value});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// 0x08A5DF58 (a0 = out, a1 = a decided value, a2 = a holder): the helper at *holder (reported when null through
// 0x08A5BB7C(0x08CB0080)) answers through slot 0x50, 0x08A5DEE8(&word, table) builds a stack word,
// 0x08A76ABC(out, &word) copies it and 0x08A5B9EC(&word) releases it. v0 = out.

// 0x08A78F54 (a0 = this, a1 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB0E38))
// answers through slot 0x40, 0x08A83160(&word, answer, table) builds a stack word, and unless this is the stack
// itself 0x08A5BAD8(this, word) copies it; 0x08A5B9EC(&word) releases it.
void func_08A78F54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 helper = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x40u);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x40u), {helper});
    call_set(rt, ctx, 0x08A83160u, {frame.at(0u), other, answer, ctx.gpr[3]});  // a3 = v1 as the callee left it
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08A8B108 (a0 = out, a1 = a decided value, a2 = a holder): the helper at *holder (reported when null through
// 0x08A5BB7C(0x08CB1BE8)) answers through slot 0x58 (called through a1), 0x08A8B098(&word, answer, table) builds a
// stack word, 0x08A76ABC(out, &word) copies it and 0x08A5B9EC(&word) releases it. v0 = out.
void func_08A8B108(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    const u32 helper = word(rt, ctx.gpr[6]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1BE8u});
    const u32 table = word(rt, helper);
    const u32 function = word(rt, table + 0x58u);
    ctx.gpr[2] = table;
    ctx.gpr[5] = function;  // `jalr a1`
    const u32 answer = call_keep(rt, ctx, function, {helper});
    call_set(rt, ctx, 0x08A8B098u, {frame.at(0u), other, answer, ctx.gpr[3]});  // a3 = v1 as the callee left it
    call_set(rt, ctx, 0x08A76ABCu, {out, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// 0x08A9BED4 (a0 = this, a1 / a2 = two values): under the lock at this + 0x4C, the object at *(this + 0x38)
// (reported when null through 0x08A5BB7C(0x08CB2570)) runs 0x08A7CBA8(object, a1, a2); v0 = the unlock's answer.

// 0x08AC2BD8: the three registered static teardown slots (0x08CFFCA8, 0x08CFFCA4, 0x08CFFCA0). The first one's
// function pointer (with its word +4 in a0) runs; when the second is set 0x08AC2BAC(second) finishes; otherwise the
// third object's slot +0x58 finishes (through $t9, with the object in a0).

// 0x08AC7F94 (a0 = this): under the lock at this + 8 (0x08B34888 / 0x08B34B24), the byte at +0x566 becomes 1 and a
// one-time record is built through 0x08B2E314(6, 1), 0x08B2E134(answer) and 0x08AC79B0(&block, answer) with the block
// {0x08AC8484, this}; the answer is kept at +0x56C.

// 0x08B1297C (a0 = this, a1 = a holder): the state record at *(holder + 0x10); a non-negative first word is released
// (0x08B33A24), a positive word +0x10 adds this[0x254] to word +4, and 0x08B11F8C(this, this + 0xAC,
// word + 4 + word + 8) answers: word +0x10 becomes -1 (v1 = -1) and word 0 the answer.

// 0x08B628BC (a0 = this, a1 / a2 / a3 = three values): 0x08B62884(this, a1, a2, a3) first (v1 = 1 in its branch delay
// slot); when it answers zero the object at *(this + 8) runs its slot 0x18 with itself. v0 / v1 = 1, or the slot's
// answer.

// 0x08B75204 (a0 = table, a1 = this): 0x08B7450C(table, this) first; the index at this + 0x9BC below 0x133 selects a
// byte of 0x08D72BB4: kind 4 clears bit 0 of the word the table entry points at and stores it at table + 0x4D0 and
// back; every other index or kind goes on in 0x08B77B04 (which does not return).

// 0x08C201AC (a0 passed on, a1 = a value): 0x08A6158C(value) twice (the first answer is kept); the object at the
// second answer's word 0 (reported when null through 0x08A5BB7C(0x08CB0284)) runs slot 0xC4 (object, first answer),
// then 0x08A61584(value, first answer) finishes.

// 0x08C79ED8 (a0 = object): vtables 0x08CDC374 (at 0) and 0x08CDC388 (at 0xA8), 0x08C786B4(object + 4), then the two
// words at 0x08CDC35C / 0x08CDC360 are used to fix the object's own table: word 0 = first, word at first - 0xC = second,
// vtable 0x08CDB088 at 0xA8 and 0x08B6543C(object + 0xA8) finishes.

// 0x088056F8 (a0 / a1 passed on): the teardown chain 0x088057CC(a0), 0x08805B1C(a0, a1), then 0x08805774, 0x08805B88,
// 0x08805B90, 0x08AC2BD8, 0x088057A8, 0x08805578, 0x088057F8, 0x08ABCE2C (none of them sets an argument register) and
// 0x08AC4398 finishes.

// 0x08805C70: the start-up sequence 0x08B2DBF0(0x14D), FPU control 31 cleared, then 0x08AD0974, 0x08AD0900,
// 0x08B33218, 0x08AD08A4, 0x08AD0820, 0x08B2DD2C, 0x08B33688, 0x08B34060, 0x08AD083C, 0x08B0EBF0, 0x08AB9B04 and
// 0x08B1F178 finishes.

// 0x08860500 (a0 = this, a1 / a2 / a3 = three values): marks *this[0x3FDB] = 1, runs 0x08860080(*this, a3 & 0xFF),
// then 0x088D7DFC(*(*this + 0x5AC), a1, a2 & 0xFF, *this + 0x3E7C) and clears the mark (v1 = *this at the end).
void func_08860500(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6] & 0xFFu;
    const u32 third = ctx.gpr[7] & 0xFFu;  // `andi a1, a3, 0xFF` before the 0x08860080 call
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = first;
    ctx.gpr[18] = second;
    ctx.gpr[17] = self;
    const u32 block = word(rt, self);
    put8(rt, block + 0x3FDBu, 1u);
    const u32 inner = word(rt, block + 0x5ACu);
    ctx.gpr[16] = inner;
    call_set(rt, ctx, 0x08860080u, {block, third});
    call_set(rt, ctx, 0x088D7DFCu, {inner, first, second, word(rt, self) + 0x3E7Cu});
    const u32 final_block = word(rt, self);  // `lw v1, 0(s1)` after the call
    put8(rt, final_block + 0x3FDBu, 0u);
    ctx.gpr[3] = final_block;
}

// 0x0887B060 (a0 = object): 0x08B9AB4C(&pair) and 0x08B9A958(object + 0x10, &pair), 0x08B8ED98(&other), then
// 0x08B8FE44(pair[0], pair[1], sp) and a nonzero pair[0] is released (0x08B35254); 0x0896275C(object) runs and the
// words at +0x30 / +0x2C are cleared.

// 0x088E43D0 (a0 = object, a1 / a2 = two values): the object at *a0 runs its slot 0x1C (itself, a1); the helper at
// *(a0 + 4), when it exists, runs slot 0x1C of its own table (through $t9, with helper, a1, a2) as the tail. v0 = the
// helper word (zero finishes) on the way out.

// 0x088FE99C (a0 = object): under the lock at +0x10 (0x08B344E0 / 0x08B3457C): the byte at +0xB is cleared,
// 0x088FE6BC(object) and 0x088FE308(object) run and 0x08B28DC4(object + 0x10) finishes; then the helper at +4 is
// released through its first virtual (with *object) and both words cleared.

// 0x08927A0C (a0 = this, a1 = a value): 0x08927120(this) answers an object whose slot 0x24 answers a number, which
// 0x08B4E734(value, number) receives; f12 becomes pi / 2 and, when the byte at this + 0x2107 is set, 0x08B4F47C(value)
// finishes. Otherwise v0 = that byte.

// 0x08986058 (a0 = object): stores vtable 0x08CCE1A8, releases the string at +0x30 unless it is the shared empty
// representation 0x08D74FD0 (0x08B60804(data - 4, -1); at zero or below also 0x08B8542C(data - 0xC, sp)), then
// 0x089ECC78(object) and 0x08B64654(object).
void func_08986058(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 data = word(rt, self + 0x30u);
    const u32 count = data - 0xCu;
    ctx.gpr[17] = count;
    put(rt, self, 0x08CCE1A8u);
    ctx.gpr[3] = 0x08CCE1A8u;  // the asm keeps the vtable in v1 (the store's source)
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    call_set(rt, ctx, 0x089ECC78u, {self});
    call_set(rt, ctx, 0x08B64654u, {self});
}

// 0x08A4D308 (a0 = object): the member at +0x98, when set, runs 0x08B61FC0; the member at +0x78 is released
// (0x08A5058C, and 0x08C1CC2C(member + 0x14, member[0x1C]) then 0x08B64654(member)), cleared, and 0x08B2EE2C(
// 0x08CF68F0) runs before 0x08A50AB0(object) finishes.

// 0x08AA3568 (a0 = out, a1 = a decided value, a2 = a holder): the helper at *holder (reported when null through
// 0x08A5BB7C(0x08CB2BB8)) answers through slot 0x54; 0x08B66524(f0) -> f12, 0x08A87EEC(&word) with f12 = the answer, and
// unless out is the stack itself 0x08A5BAD8(out, word) copies it; 0x08A5B9EC(&word) releases it.

// 0x08AA37D4: the same as 0x08AA3568 with 0x08B65F0C in place of 0x08B66524.

// 0x08ACC070 (a0 = this): when the word at 0x08D69170 is zero, 0x08B327AC(0x08D69170, 1) must open it; then
// 0x08ACB910(this, &block) must answer nonzero for 0x08ACBC78(this, &block) to run with the block word at +0x30
// cleared. Every failing gate returns 0 in v0.

// 0x08AF1124 (a0 = a begin / end pair): the element count ((end - begin) >> 3) is halved to one; twice the number of
// halvings goes to 0x08C3EEE0(begin, end, twice), which 0x08C3E9B0(begin, end, twice) finishes. An empty range
// returns v0 = 0 at once.

} // namespace
} // namespace new_batch_main_933
} // namespace gt::game

#define NEW933(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_933::name, gt::ReturnKind::kind)
NEW933(0x08A56F08u, func_08A56F08, Word);
NEW933(0x08A78F54u, func_08A78F54, Word);
NEW933(0x08A8B108u, func_08A8B108, Word);
NEW933(0x08860500u, func_08860500, Word);
NEW933(0x08986058u, func_08986058, Word);
