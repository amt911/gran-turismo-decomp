// New batch 5: two families the extra seeds made emittable, written as structured C from `tools.disasm` and verified by synthetic cases.
// (1) 48 functions of 43 instructions that differ only in the function they call first and in their branch targets: a conversion that
// asks that function for a type, tries to convert the input with it (0x08A76F20), and on failure converts again into a local (0x08A76AA0);
// the result string is assigned to the output (0x08A5BAD8) and the local released (0x08A5B9EC). v0 = 1 unless the second conversion gave nothing.
// (2) 22 destructors of 24 instructions: the vtable word is stored, a member at +OFFSET that holds an object is released through its
// vtable's first virtual (with the word at +OFFSET as argument) and cleared, then 0x08A780B0 or another base finishes (tail call).
// Of the 48 conversions only 7 were compared by the synthetic cases (the others have the same body but no comparable case: the generated
// inputs make the type function fault), and 18 of the 22 destructors; the rest are not registered here.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_5 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The callee-saved registers s0-s3: the original saves and restores them, the body sets them because callees may read them.
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 saved[4];
    explicit SavedRegisters(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 4u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 4u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

// Family 1 (a0 = the output string, a1 = pointer to the input; `type_of` = the function asked for the type first).
void convert_string(Runtime &rt, AllegrexContext &ctx, u32 type_of) {
    const u32 output = ctx.gpr[4];
    const u32 input = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 local = frame.at(0u);
    u32 done = 1u;  // s3
    ctx.gpr[17] = input;
    ctx.gpr[18] = output;
    ctx.gpr[19] = done;
    const u32 text = word(rt, input);
    ctx.gpr[16] = text;
    u32 value = 0u;  // what the output is assigned from, a1 of the assignment
    if (text != 0u) {
        const u32 type = call(rt, ctx, type_of, {output, 0u});
        const GuestResult converted = call_guest(rt, ctx, 0x08A76F20u, {text, type}, {});
        ctx.gpr[2] = converted.v0;
        if (converted.v0 == 0u) {
            done = 0u;
            ctx.gpr[19] = 0u;
            call(rt, ctx, 0x08A76AA0u, {local, converted.a1});
            const u32 result = word(rt, local);
            if (result != 0u) {
                done = 1u;
                ctx.gpr[19] = 1u;
                call(rt, ctx, 0x08A5BAD8u, {output, result});
            }
            call(rt, ctx, 0x08A5B9ECu, {local});
            ctx.gpr[2] = done;
            return;
        }
        value = word(rt, input);
    }
    call(rt, ctx, 0x08A5BAD8u, {output, value});
    ctx.gpr[2] = done;
}

// Family 2 (a0 = this): see the file header.
void release_member_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 offset, u32 tail_target) {
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = vtable;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self + offset;
        ctx.gpr[16] = self;
        put(rt, self, vtable);
        const u32 member = word(rt, self + offset + 4u);
        ctx.gpr[3] = member;
        if (member != 0u) {
            const u32 function = word(rt, word(rt, member));
            ctx.gpr[2] = call_indirect(rt, ctx, function, {member, word(rt, self + offset)});
            put(rt, self + offset + 4u, 0u);
            put(rt, self + offset, 0u);
        }
    }
    tail_call(ctx, tail_target, {self});
}

void func_0880D7B0(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x0880D668u); }
void func_089C593C(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x089C413Cu); }
void func_08A3171C(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x08A315FCu); }
void func_08A43DFC(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x08A43C9Cu); }
void func_08A51D94(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x08A51C94u); }
void func_08A7AF84(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x08A7AD98u); }
void func_08AB4C40(Runtime &rt, AllegrexContext &ctx) { convert_string(rt, ctx, 0x08AB4A50u); }
void func_08818430(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBE1F0u, 0x14u, 0x08A780B0u); }
void func_0881C2B8(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBE450u, 0x14u, 0x08A780B0u); }
void func_0883903C(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBDAB8u, 0x14u, 0x08A780B0u); }
void func_088419A0(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD9A8u, 0x14u, 0x08A780B0u); }
void func_08843AA0(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD898u, 0x14u, 0x08A780B0u); }
void func_08844B58(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD788u, 0x14u, 0x08A780B0u); }
void func_08847A08(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD568u, 0x14u, 0x08A780B0u); }
void func_0884DBE4(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD018u, 0x14u, 0x08A780B0u); }
void func_08854D84(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD238u, 0x14u, 0x08A780B0u); }
void func_08858994(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBD128u, 0x14u, 0x08A780B0u); }
void func_0887E490(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CC1020u, 0x70u, 0x0886A858u); }
void func_0887E5D0(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CC0FE8u, 0x70u, 0x0886A858u); }
void func_0887E820(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CC1090u, 0x70u, 0x0886A858u); }
void func_08A1B5F0(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CD0C48u, 0x14u, 0x08A780B0u); }
void func_08A1D5F8(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CD0A28u, 0x14u, 0x08A780B0u); }
void func_08A2D4E0(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CBE6A0u, 0x14u, 0x08A780B0u); }
void func_08A31958(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CD14C0u, 0x14u, 0x08A780B0u); }
void func_08A37868(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CD17F0u, 0x14u, 0x08A780B0u); }

} // namespace
} // namespace new_batch_5
} // namespace gt::game

#define NEW5(address, name, kind) GT_DECOMP(address, gt::game::new_batch_5::name, gt::ReturnKind::kind)
NEW5(0x0880D7B0u, func_0880D7B0, Word);
NEW5(0x089C593Cu, func_089C593C, Word);
NEW5(0x08A3171Cu, func_08A3171C, Word);
NEW5(0x08A43DFCu, func_08A43DFC, Word);
NEW5(0x08A51D94u, func_08A51D94, Word);
NEW5(0x08A7AF84u, func_08A7AF84, Word);
NEW5(0x08AB4C40u, func_08AB4C40, Word);
NEW5(0x08818430u, func_08818430, Word);
NEW5(0x0881C2B8u, func_0881C2B8, Word);
NEW5(0x0883903Cu, func_0883903C, Word);
NEW5(0x088419A0u, func_088419A0, Word);
NEW5(0x08843AA0u, func_08843AA0, Word);
NEW5(0x08844B58u, func_08844B58, Word);
NEW5(0x08847A08u, func_08847A08, Word);
NEW5(0x0884DBE4u, func_0884DBE4, Word);
NEW5(0x08854D84u, func_08854D84, Word);
NEW5(0x08858994u, func_08858994, Word);
NEW5(0x0887E490u, func_0887E490, Word);
NEW5(0x0887E5D0u, func_0887E5D0, Word);
NEW5(0x0887E820u, func_0887E820, Word);
NEW5(0x08A1B5F0u, func_08A1B5F0, Word);
NEW5(0x08A1D5F8u, func_08A1D5F8, Word);
NEW5(0x08A2D4E0u, func_08A2D4E0, Word);
NEW5(0x08A31958u, func_08A31958, Word);
NEW5(0x08A37868u, func_08A37868, Word);
