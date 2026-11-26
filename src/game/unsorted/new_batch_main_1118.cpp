// New batch main 1118: constructor and copy-constructor shapes, written by hand from `tools/disasm` as structured C
// (named locals, `G[n]` only at the call boundary). Draft batch, not yet verified by the differential gate; the input
// shapes of its arguments live in gt-ops/shapes/shapes_main1118.txt. Three shapes repeat: the copy constructor over
// 0x08A782D8 (the base copies a small map's head word), the vtable constructors that hand the member at +0x14 on to a
// tail call, and the constructors that only store the vtable and clear their members.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1118 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Saves the caller's callee-saved registers (16 = s0 ... 18 = s2) and restores them on scope exit: the end state
// compares them, so a shape that saves one must give the same value back.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx) {
        for (const unsigned reg : registers) {
            indexes_[count_] = reg;
            values_[count_] = ctx.gpr[reg];
            ++count_;
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < count_; ++i) ctx_.gpr[indexes_[i]] = values_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned indexes_[8]{};
    u32 values_[8]{};
    unsigned count_{};
};

enum class Field { Word, Half, Byte, Float };

// The copy constructors: the base ctor at `base` copies the object at `other` (it reads other's word at +0xC and the
// node pointer at +0x10, whose null keeps the copy shallow), then the field at `offset` of `other` is copied into
// `this` and the vtable word stored at +0; v0 is that vtable word and v1 the copied value (as the asm leaves them).
void copy_ctor(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable, u32 offset, Field field) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = other;
        call(rt, ctx, base, {self, other});
        u32 value = 0;
        switch (field) {
        case Field::Word:
        case Field::Float:  // lwc1 / swc1 move the bits, they do not convert
            value = word(rt, other + offset);
            put(rt, self + offset, value);
            break;
        case Field::Half:
            value = rt.memory().load16(other + offset);
            rt.memory().store16(self + offset, static_cast<std::uint16_t>(value));
            break;
        case Field::Byte:
            value = rt.memory().load8(other + offset);
            rt.memory().store8(self + offset, static_cast<std::uint8_t>(value));
            break;
        }
        ctx.gpr[3] = value;
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
    }
}

// The vtable constructors that hand a member on: the base ctor at `base` runs with (this, a1), the vtable word is stored
// at +0, and the member at +0x14 along with the argument saved at entry goes to `tail`; v0 is the base's result.
void ctor_member_tail(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable, u32 tail, u32 second) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = second;
        result = call(rt, ctx, base, {self, ctx.gpr[5]});
        put(rt, self, vtable);
    }
    ctx.gpr[2] = result;
    tail_call(ctx, tail, {self + 0x14u, second});
}

// The constructors that leave the members to their base: the base ctor at `base` runs with (this, a1), the vtable word
// is stored at +0; v0 is that word.
void base_ctor_vtable(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, {16u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, base, {self, ctx.gpr[5]});
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
    }
}

// --- copy constructors (a0 = this, a1 = the object copied) ---------------------------------------------------------
void func_08A72D78(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD1FB0u, 0x14u, Field::Half);
}
void func_08A72DBC(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD1FB0u, 0x14u, Field::Half);
}
void func_08A8550C(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CB8578u, 0x14u, Field::Word);
}
void func_08A85550(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CB8578u, 0x14u, Field::Word);
}
void func_08A87748(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CBBF20u, 0x14u, Field::Float);
}
void func_08A8778C(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CBBF20u, 0x14u, Field::Float);
}
void func_08A95AEC(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD21A8u, 0x14u, Field::Byte);
}
void func_08A95B30(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD21A8u, 0x14u, Field::Byte);
}
void func_08A973FC(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CBCC00u, 0x14u, Field::Byte);
}
void func_08A97440(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CBCC00u, 0x14u, Field::Byte);
}
void func_08AAFC38(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD3838u, 0x14u, Field::Half);
}
void func_08AAFC7C(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD3838u, 0x14u, Field::Half);
}
void func_08AB13BC(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD3728u, 0x14u, Field::Byte);
}
void func_08AB1400(Runtime &rt, AllegrexContext &ctx) {
    copy_ctor(rt, ctx, 0x08A782D8u, 0x08CD3728u, 0x14u, Field::Byte);
}

// --- vtable constructors that tail-call the member at +0x14 --------------------------------------------------------
// 0x08A74468 (a0 = this, a1 = base copy's argument, a2): 0x08A725A0(this, a1), vtable 0x08CD3A18, 0x08A76ABC(this+0x14, a2).
void func_08A74468(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A725A0u, 0x08CD3A18u, 0x08A76ABCu, ctx.gpr[6]);
}
// 0x08A749C4 (a0 = this, a1, a2): 0x08A80214(this, a1), vtable 0x08CD3A58, 0x08A90480(this+0x14, a2).
void func_08A749C4(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A80214u, 0x08CD3A58u, 0x08A90480u, ctx.gpr[6]);
}
// 0x08A74F70 (a0 = this, a1, a2): 0x08A86BDC(this, a1), vtable 0x08CD3AA0, 0x08A90480(this+0x14, a2).
void func_08A74F70(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A86BDCu, 0x08CD3AA0u, 0x08A90480u, ctx.gpr[6]);
}
// 0x08A87084 (a0 = this, a1): 0x08A76ACC(this), vtable 0x08CBEF70, 0x08A86D10(this+0x14, a1).
void func_08A87084(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A76ACCu, 0x08CBEF70u, 0x08A86D10u, ctx.gpr[5]);
}
// 0x08A89F10 (a0 = this, a1): 0x08A76ACC(this), vtable 0x08CB7218, 0x08B88F44(this+0x14, a1).
void func_08A89F10(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A76ACCu, 0x08CB7218u, 0x08B88F44u, ctx.gpr[5]);
}
// 0x08A91AA8 (a0 = this, a1, a2): 0x08A5D5A0(this, a1), vtable 0x08CD43D8, 0x08A92500(this+0x14, a2).
void func_08A91AA8(Runtime &rt, AllegrexContext &ctx) {
    ctor_member_tail(rt, ctx, 0x08A5D5A0u, 0x08CD43D8u, 0x08A92500u, ctx.gpr[6]);
}

// --- constructors that only store the vtable (a0 = this) ----------------------------------------------------------
// 0x08A80244: 0x08A5D5A0(this, a1), vtable 0x08CD4120.
void func_08A80244(Runtime &rt, AllegrexContext &ctx) { base_ctor_vtable(rt, ctx, 0x08A5D5A0u, 0x08CD4120u); }
// 0x08A86C0C: 0x08A5D5A0(this, a1), vtable 0x08CD42B8.
void func_08A86C0C(Runtime &rt, AllegrexContext &ctx) { base_ctor_vtable(rt, ctx, 0x08A5D5A0u, 0x08CD42B8u); }

// 0x08A98B10 (a0 = this): 0x08A76ACC(this), vtable 0x08CB7108, the words at +0x14 / +0x18 / +0x1C and the byte at
// +0x20 cleared; v0 is the vtable word.
void func_08A98B10(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, {16u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08A76ACCu, {self});
        put(rt, self, 0x08CB7108u);
        const u32 member = self + 0x14u;
        put(rt, member, 0u);
        put(rt, member + 8u, 0u);
        put(rt, member + 4u, 0u);
        rt.memory().store8(self + 0x20u, 0u);
        ctx.gpr[2] = 0x08CB7108u;
    }
}

// 0x08A9E0F4 (a0 = this, a1): 0x08A9E820(this), vtable 0x08CD48E0, a1 stored at +0x20 and +0x24 cleared; v0 is the
// vtable word.
void func_08A9E0F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = value;
        call(rt, ctx, 0x08A9E820u, {self});
        put(rt, self, 0x08CD48E0u);
        put(rt, self + 0x20u, value);
        put(rt, self + 0x24u, 0u);
        ctx.gpr[2] = 0x08CD48E0u;
    }
}

// 0x08AB92C8 (a0 = this): 0x08AB94A4(this), vtable 0x08CD5F28, the words at +0x20 and +0x10..+0x1C cleared; v0 is the
// vtable word.
void func_08AB92C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, {16u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08AB94A4u, {self});
        put(rt, self, 0x08CD5F28u);
        put(rt, self + 0x20u, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x18u, 0u);
        put(rt, self + 0x1Cu, 0u);
        ctx.gpr[2] = 0x08CD5F28u;
    }
}

// --- the assignment constructor and the two container constructors --------------------------------------------------
// 0x08AA2134 (a0 = this, a1 = a signed byte): a temporary is built on the frame by 0x08A85B1C(sp, (int8)a1), moved
// into this by 0x08A76ABC(this, sp), released by 0x08A5B9EC(sp); v0 = this.
void func_08AA2134(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 byte_argument = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[5])));
    const u32 result = self;
    {
        SavedRegisters saved(ctx, {16u});
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), byte_argument});
        call(rt, ctx, 0x08A76ABCu, {self, frame.at(0u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        ctx.gpr[2] = result;
    }
}

// 0x08ABFD8C (a0 = this, a1 = the base's word, a2, a3): 0x08C3C654(this, a1) (0x08B1FD24 stores a1 at +0 and clears
// +4 / +8), a3 stored at +0x10 and a2 at +0xC; v0 is that call's result.
void func_08ABFD8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    u32 result;
    {
        SavedRegisters saved(ctx, {16u, 17u, 18u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = third;
        ctx.gpr[18] = second;
        result = call(rt, ctx, 0x08C3C654u, {self, ctx.gpr[5]});
        put(rt, self + 0x10u, third);
        put(rt, self + 0xCu, second);
    }
    ctx.gpr[2] = result;
}

// 0x08B09220 (a0 = this): a 0x1AC-byte block is allocated by 0x08B62344 and initialised by 0x08B09F98(block+0x10), the
// block is stored at +0 and 0x08B08AE0(this) runs as the tail call.
void func_08B09220(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self;
        const u32 block = call(rt, ctx, 0x08B62344u, {0x1ACu});
        ctx.gpr[16] = block;
        call(rt, ctx, 0x08B09F98u, {block + 0x10u});
        put(rt, self, block);
    }
    tail_call(ctx, 0x08B08AE0u, {self});
}

// 0x08B3FE20 and 0x08B3FE64 (a0 = this, a1): the members are cleared by 0x08B3F838(this), a1 goes to 0x08B3F7B4(this,
// a1), and 0x08B3F7A8(this, 2) runs as the tail call (it writes v0 = 2 and the byte at +1).
void func_08B3FE20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x08B3F838u, {self});
        call(rt, ctx, 0x08B3F7B4u, {self, argument});
    }
    tail_call(ctx, 0x08B3F7A8u, {self, 2u});
}
void func_08B3FE64(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, {16u, 17u});
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x08B3F838u, {self});
        call(rt, ctx, 0x08B3F7B4u, {self, argument});
    }
    tail_call(ctx, 0x08B3F7A8u, {self, 2u});
}

} // namespace
} // namespace new_batch_main_1118
} // namespace gt::game

#define NEW1118(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1118::name, gt::ReturnKind::kind)
NEW1118(0x08A72D78u, func_08A72D78, Word);
NEW1118(0x08A72DBCu, func_08A72DBC, Word);
NEW1118(0x08A74468u, func_08A74468, Word);
NEW1118(0x08A749C4u, func_08A749C4, Word);
NEW1118(0x08A74F70u, func_08A74F70, Word);
NEW1118(0x08A80244u, func_08A80244, Word);
NEW1118(0x08A8550Cu, func_08A8550C, Word);
NEW1118(0x08A85550u, func_08A85550, Word);
NEW1118(0x08A86C0Cu, func_08A86C0C, Word);
NEW1118(0x08A87084u, func_08A87084, Word);
NEW1118(0x08A87748u, func_08A87748, Word);
NEW1118(0x08A8778Cu, func_08A8778C, Word);
NEW1118(0x08A89F10u, func_08A89F10, Word);
NEW1118(0x08A91AA8u, func_08A91AA8, Word);
NEW1118(0x08A95AECu, func_08A95AEC, Word);
NEW1118(0x08A95B30u, func_08A95B30, Word);
NEW1118(0x08A973FCu, func_08A973FC, Word);
NEW1118(0x08A97440u, func_08A97440, Word);
NEW1118(0x08A98B10u, func_08A98B10, Word);
NEW1118(0x08A9E0F4u, func_08A9E0F4, Word);
NEW1118(0x08AA2134u, func_08AA2134, Word);
NEW1118(0x08AAFC38u, func_08AAFC38, Word);
NEW1118(0x08AAFC7Cu, func_08AAFC7C, Word);
NEW1118(0x08AB13BCu, func_08AB13BC, Word);
NEW1118(0x08AB1400u, func_08AB1400, Word);
NEW1118(0x08AB92C8u, func_08AB92C8, Word);
NEW1118(0x08ABFD8Cu, func_08ABFD8C, Word);
NEW1118(0x08B09220u, func_08B09220, Word);
NEW1118(0x08B3FE20u, func_08B3FE20, Word);
NEW1118(0x08B3FE64u, func_08B3FE64, Word);
