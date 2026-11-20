// New batch 8: small functions with branches, a loop-free float conversion and indirect calls that the extra seeds made emittable, written by
// hand from `tools.disasm` and verified by synthetic cases (some through corpus recipes, config/synth_corpus/<ADDRESS>.txt).
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in are set where the asm sets
// them (a callee may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as the asm leaves it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_8 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s1 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

// 0x08B830D0 (a0 = a slot number): slots 0-7 of a table of 64-byte records. A record whose word +0x1C is nonzero gets 0x00140000 ORed
// into its word +0x18 and the companion word +0x1C (0x08D73ED4 + slot * 64) cleared. Returns 0 for a valid slot (the result register is
// zeroed in a delay slot whatever the record holds) and 0x80440005 for a slot number of 8 or more.
void func_08B830D0(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kRecords = 0x08D73CD4u;
    constexpr u32 kCompanions = 0x08D73ED4u;
    const u32 slot = ctx.gpr[4];
    const u32 offset = slot << 6;
    u32 result = 0x80440005u;
    if (slot < 8u) {
        result = 0u;
        const u32 record = kRecords + offset;
        if (word(rt, record + 0x1Cu) != 0u) {
            const u32 flags = word(rt, record + 0x18u);
            put(rt, kCompanions + offset + 0x1Cu, 0u);
            put(rt, record + 0x18u, flags | 0x00140000u);
        }
    }
    ctx.gpr[2] = result;
}

// 0x0899CA9C (a0 = this): the vtable word 0x08CBCAB0 is stored, the members at +0x28 and +0x24 are released (0x08A5B9EC), the object held
// at +0x20, if any, is released too (0x08A5B930) and 0x089BA380(this) finishes.
void func_0899CA9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CBCAB0u);
        ctx.gpr[2] = 0x08CBCAB0u;
        call(rt, ctx, 0x08A5B9ECu, {self + 0x28u});
        call(rt, ctx, 0x08A5B9ECu, {self + 0x24u});
        const u32 held = word(rt, self + 0x20u);
        ctx.gpr[2] = held;
        if (held != 0u) call(rt, ctx, 0x08A5B930u, {held});
    }
    tail_call(ctx, 0x089BA380u, {self});
}

// 0x08B015D4 (a0 = this, a1 = a value): the value is kept in a local; 0x08C47338(word +0x38, word +0x3C, &local) answers a replacement,
// and when it differs from the word +0x3C, 0x08C471AC(this + 0x38, answer) is called. v0 = the answer or the second call's result.
void func_08B015D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 local = frame.at(0u);
    const u32 member = self + 0x38u;
    ctx.gpr[16] = self;
    ctx.gpr[17] = member;
    put(rt, local, ctx.gpr[5]);
    const u32 answer = call(rt, ctx, 0x08C47338u, {word(rt, self + 0x38u), word(rt, self + 0x3Cu), local});
    if (word(rt, self + 0x3Cu) != answer) call(rt, ctx, 0x08C471ACu, {member, answer, local});
}

// 0x08AD1AB8, 0x08AD1B14 and 0x08AD1B70 (a0 = this): the record at this + 0x80 + index * 80 (index = the word +0x1C0) is handed to a
// function (0x08B4F2E4 / 0x08B4F3B0 / 0x08B4F47C), then 0x08B4E994(this, this + 0x40, that record) finishes.
void handle_indexed_record(Runtime &rt, AllegrexContext &ctx, u32 first_call) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, first_call, {self + word(rt, self + 0x1C0u) * 80u + 0x80u});
    }
    const u32 index = word(rt, self + 0x1C0u);
    ctx.gpr[2] = index << 6;  // `sll v0, index, 6`: v0 still holds it when the tail call starts
    tail_call(ctx, 0x08B4E994u, {self, self + 0x40u, self + index * 80u + 0x80u});
}
void func_08AD1AB8(Runtime &rt, AllegrexContext &ctx) { handle_indexed_record(rt, ctx, 0x08B4F2E4u); }
void func_08AD1B14(Runtime &rt, AllegrexContext &ctx) { handle_indexed_record(rt, ctx, 0x08B4F3B0u); }
void func_08AD1B70(Runtime &rt, AllegrexContext &ctx) { handle_indexed_record(rt, ctx, 0x08B4F47Cu); }

// 0x08AE18A0 (a0 passed on): looks the name 0x08C912FC up (0x08ADDCD4); f0 = 0 when nothing is found, else the halfword at +0x2A of the
// object scaled by the float at 0x08CB6100.
void func_08AE18A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C912FCu});
    ctx.fpr[0] = 0.0f;
    if (found == 0u) return;
    const s32 value = signed_half_at(rt, found + 0x2Au);
    ctx.gpr[2] = static_cast<u32>(value);
    ctx.fpr[0] = psp_mul(static_cast<float>(value), float_at(rt, 0x08CB6100u));
}

// 0x08AE18E0 (a0 passed on): looks the name 0x08C9130C up; f0 = 0 when nothing is found, else the float at 0x08CB6104 when the object's
// float at +0x34 is zero, otherwise the object's float at +0x30 divided by it.
void func_08AE18E0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C9130Cu});
    ctx.fpr[0] = 0.0f;
    if (found == 0u) return;
    const float divisor = float_at(rt, found + 0x34u);
    ctx.fpr[1] = divisor;
    if (divisor == 0.0f) {
        ctx.fpr[0] = float_at(rt, 0x08CB6104u);
        return;
    }
    ctx.fpr[0] = float_at(rt, found + 0x30u) / divisor;
}

// 0x08927AC0 (f12 = a scale): 0x08927120() answers an object; the float at 0x08C8A884 times the scale, truncated, is passed with the
// object to the virtual at +0x3C of the object's vtable (a tail call).
void func_08927AC0(Runtime &rt, AllegrexContext &ctx) {
    const float scale = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    ctx.fpr[20] = scale;
    u32 object;
    {
        GuestFrame frame(ctx, 0x20u);
        object = call(rt, ctx, 0x08927120u);
    }
    const s32 amount = trunc_w_s(psp_mul(float_at(rt, 0x08C8A884u), scale));
    const u32 function = word(rt, word(rt, object) + 0x3Cu);
    ctx.fpr[0] = std::bit_cast<float>(static_cast<u32>(amount));
    ctx.fpr[20] = saved_f20;
    tail_call(ctx, function, {object, static_cast<u32>(amount)});
}

// 0x08890C38 (a0 passed on): 0x08890B44(a0, 0) answers a pointer; v0 = the word it points to with its bytes swapped.
void func_08890C38(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 pointer = call(rt, ctx, 0x08890B44u, {ctx.gpr[4], 0u});
    ctx.gpr[2] = __builtin_bswap32(word(rt, pointer));
}

// 0x08ACC944 (a0 = this): 0x08ACC6B4(this) decides: a nonzero answer is returned, zero goes on in 0x08ACC93C(this) (a tail call).
void func_08ACC944(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 answer;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        answer = call(rt, ctx, 0x08ACC6B4u, {self});
    }
    if (answer == 0u) tail_call(ctx, 0x08ACC93Cu, {self});
}

// 0x089C69FC (a0 = this): the vtable word 0x08CCD338 is stored; the object at this + 0x240, unless it is the one at this + 0x4D0, is
// released through its vtable's first virtual (called with this + 0x240); 0x08B22AF0(this + 0x208) runs and 0x08978B04(this) finishes.
void func_089C69FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CCD338u);
        const u32 member = self + 0x240u;
        ctx.gpr[2] = 0x08CCD338u;
        if (member != 0u && member != self + 0x4D0u) {
            const u32 function = word(rt, word(rt, self + 0x240u));
            call_indirect(rt, ctx, function, {member});
        }
        call(rt, ctx, 0x08B22AF0u, {self + 0x208u});
    }
    tail_call(ctx, 0x08978B04u, {self});
}

// 0x0886A348 (a0 = this): the vtable word 0x08CBF680 is stored; a helper object held at this + 8 is released through its vtable's first
// virtual (with the word at this + 4) and both fields are cleared. v0 = the call's result, or the vtable word when there is no helper.
void func_0886A348(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self + 4u;
    ctx.gpr[16] = self;
    put(rt, self, 0x08CBF680u);
    ctx.gpr[2] = 0x08CBF680u;
    const u32 helper = word(rt, self + 8u);
    ctx.gpr[3] = helper;
    if (helper == 0u) return;
    const u32 function = word(rt, word(rt, helper));
    call_indirect(rt, ctx, function, {helper, word(rt, self + 4u)});
    put(rt, self + 8u, 0u);
    put(rt, self + 4u, 0u);
}

// 0x0880A7C4 (a0 = this): 0x08A5BC20(0xF0, 4) answers a block, 0x0880A574 is called with it, and the block is stored in a local that
// 0x08969190(this, &local) receives. v0 = that last call's result.
void func_0880A7C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0xF0u, 4u});
    ctx.gpr[16] = block;
    call(rt, ctx, 0x0880A574u, {block});
    put(rt, frame.at(0u), block);
    call(rt, ctx, 0x08969190u, {self, frame.at(0u)});
}

// A held helper object at `self + offset` (the object's address is the word at +4 of the slot, a value is the word at +0): released through the
// first virtual of its vtable, called with the helper and the value, and both words cleared. The shape of many destructors.
void release_held(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset) {
    const u32 held = word(rt, self + offset + 4u);
    ctx.gpr[3] = held;
    if (held == 0u) return;
    const u32 function = word(rt, word(rt, held));
    call_indirect(rt, ctx, function, {held, word(rt, self + offset)});
    put(rt, self + offset + 4u, 0u);
    put(rt, self + offset, 0u);
}

// Seven functions (a0 = this): a first vtable word is stored, the helper at `offset` is released, a second vtable word is stored; v0 = the second.
void release_between_vtables(Runtime &rt, AllegrexContext &ctx, u32 first_vtable, u32 offset, u32 second_vtable) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self + offset;
    ctx.gpr[16] = self;
    put(rt, self, first_vtable);
    ctx.gpr[2] = first_vtable;
    release_held(rt, ctx, self, offset);
    put(rt, self, second_vtable);
    ctx.gpr[2] = second_vtable;
}

// Four functions (a0 = this): two vtable words (at +0 and +0x38) are stored, the helper at `offset` is released and a base finishes (tail call).
void release_with_two_vtables_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first_vtable, u32 offset, u32 second_vtable, u32 tail_target) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self + offset;
        ctx.gpr[16] = self;
        put(rt, self, first_vtable);
        put(rt, self + 0x38u, second_vtable);
        ctx.gpr[2] = second_vtable;
        release_held(rt, ctx, self, offset);
    }
    tail_call(ctx, tail_target, {self});
}

// 0x0882C16C (a0 = this): the vtable word 0x08CBBBC0 is stored, the field +0x14 cleared, the helper at +0x18 released and 0x08A2A4B0(this) finishes.
void func_0882C16C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self + 0x18u;
        ctx.gpr[16] = self;
        put(rt, self, 0x08CBBBC0u);
        ctx.gpr[2] = 0x08CBBBC0u;
        put(rt, self + 0x14u, 0u);
        release_held(rt, ctx, self, 0x18u);
    }
    tail_call(ctx, 0x08A2A4B0u, {self});
}
void func_0886FA90(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF840u, 0xCu, 0x08CBF428u); }
void func_0886FC3C(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF868u, 0xCu, 0x08CBF428u); }
void func_0886FDE8(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF890u, 0xCu, 0x08CBF428u); }
void func_0886FF94(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF8B8u, 0xCu, 0x08CBF428u); }
void func_08870140(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF8E0u, 0xCu, 0x08CBF428u); }
void func_088716C4(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CBF9A0u, 0xCu, 0x08CBF428u); }
void func_08880918(Runtime &rt, AllegrexContext &ctx) { release_between_vtables(rt, ctx, 0x08CC12A0u, 0xCu, 0x08CBF428u); }
void func_08809FDC(Runtime &rt, AllegrexContext &ctx) { release_with_two_vtables_then_tail(rt, ctx, 0x08CB9C78u, 0x118u, 0x08CB9ED0u, 0x08A0298Cu); }
void func_0880AE58(Runtime &rt, AllegrexContext &ctx) { release_with_two_vtables_then_tail(rt, ctx, 0x08CBA3A0u, 0xE8u, 0x08CBA5D8u, 0x0896BC0Cu); }
void func_0880BD5C(Runtime &rt, AllegrexContext &ctx) { release_with_two_vtables_then_tail(rt, ctx, 0x08CBA148u, 0xE8u, 0x08CBA380u, 0x0896BC0Cu); }
void func_0880F55C(Runtime &rt, AllegrexContext &ctx) { release_with_two_vtables_then_tail(rt, ctx, 0x08CB97A8u, 0xE8u, 0x08CB99E0u, 0x0896BC0Cu); }

} // namespace
} // namespace new_batch_8
} // namespace gt::game

#define NEW8(address, name, kind) GT_DECOMP(address, gt::game::new_batch_8::name, gt::ReturnKind::kind)
NEW8(0x08B830D0u, func_08B830D0, Word);
NEW8(0x0899CA9Cu, func_0899CA9C, Word);
NEW8(0x08B015D4u, func_08B015D4, Word);
NEW8(0x08AD1AB8u, func_08AD1AB8, Word);
NEW8(0x08AD1B14u, func_08AD1B14, Word);
NEW8(0x08AD1B70u, func_08AD1B70, Word);
NEW8(0x08AE18A0u, func_08AE18A0, Float);
NEW8(0x08AE18E0u, func_08AE18E0, Float);
NEW8(0x08927AC0u, func_08927AC0, Word);
NEW8(0x08890C38u, func_08890C38, Word);
NEW8(0x08ACC944u, func_08ACC944, Word);
NEW8(0x089C69FCu, func_089C69FC, Word);
NEW8(0x0886A348u, func_0886A348, Word);
NEW8(0x0880A7C4u, func_0880A7C4, Word);
NEW8(0x0886FA90u, func_0886FA90, Word);
NEW8(0x0886FC3Cu, func_0886FC3C, Word);
NEW8(0x0886FDE8u, func_0886FDE8, Word);
NEW8(0x0886FF94u, func_0886FF94, Word);
NEW8(0x08870140u, func_08870140, Word);
NEW8(0x088716C4u, func_088716C4, Word);
NEW8(0x08880918u, func_08880918, Word);
NEW8(0x08809FDCu, func_08809FDC, Word);
NEW8(0x0880AE58u, func_0880AE58, Word);
NEW8(0x0880BD5Cu, func_0880BD5C, Word);
NEW8(0x0880F55Cu, func_0880F55C, Word);
NEW8(0x0882C16Cu, func_0882C16C, Word);
