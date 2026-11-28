// New batch main 1566: functions the earlier main-session batches mismatched or skipped twice.
// Structured C written from tools.disasm; the asm comment above each body is the evidence.
// Leaf getters (0x08B48CB8 / 0x08B48DCC) and the destructor shapes (vtable slot 0 then clear) are the
// cheap ones; the tail-call forwarders assert on a null member and then jump, as the original does.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1566 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// The shape shared by 0x08AC32C4, 0x08AC3784, 0x08893E90 and 0x08ACC13C: obj = this->+4; when set, call
// its vtable slot 0 with (obj, this->+0), then clear both words.
void release_member(Runtime &rt, AllegrexContext &ctx, u32 self) {
    const u32 object = word(rt, self + 4u);
    if (object == 0u) return;
    const u32 target = word(rt, word(rt, object));
    const u32 argument = word(rt, self);
    call_indirect(rt, ctx, target, {object, argument});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// ---- 0x08AAFCC0: when this != other, 0x08A781D4(this, other); this->+0x14 (half) = other->+0x14; v0 = this. ----

// ---- 0x08AB1444: as 0x08AAFCC0 with the byte at +0x14 (lbu / sb). ----
void func_08AB1444(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    if (self != other) {
        call(rt, ctx, 0x08A781D4u, {self, other});
        rt.memory().store8(self + 0x14u, rt.memory().load8(other + 0x14u));
    }
    ctx.gpr[2] = self;
}

// ---- 0x08AB3E04: 0x08B855E0(this + 0x24); v0 = 0x08B1EA48(this + 0x14, *a1) & 0xFF. ----
void func_08AB3E04(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    call(rt, ctx, 0x08B855E0u, {self + 0x24u});
    const u32 result = call(rt, ctx, 0x08B1EA48u, {self + 0x14u, word(rt, arg)});
    ctx.gpr[2] = result & 0xFFu;
}

// ---- 0x08AC32C4: obj = this->+4; when set, call its vtable slot 0 with (obj, this->+0) and clear both. ----

// ---- 0x08AC3784: byte-identical body to 0x08AC32C4. ----
void func_08AC3784(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    release_member(rt, ctx, ctx.gpr[4]);
}

// ---- 0x08AD68C8: 0x08B35254(0x08B2B6F4(this)); when this->+0x20, 0x08B64654(it); tail 0x08C3E128(this). ----
void func_08AD68C8(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 list = call(rt, ctx, 0x08B2B6F4u, {self});
    call(rt, ctx, 0x08B35254u, {list});
    const u32 object = word(rt, self + 0x20u);
    if (object != 0u) call(rt, ctx, 0x08B64654u, {object});
    tail_call(ctx, 0x08C3E128u, {self});
}

// ---- 0x08AFC92C: compare a0->+4 with a1->+4; different -> sltu; equal -> when a0->+0 is set,
// 0x08B35FD8(a0->+0, a1->+0) >> 31 (unsigned), else 0. v0 = that. ----
void func_08AFC92C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 left = word(rt, ctx.gpr[4] + 4u);
    const u32 right = word(rt, ctx.gpr[5] + 4u);
    u32 result = 0u;
    if (left != right) {
        result = left < right ? 1u : 0u;
    } else {
        const u32 object = word(rt, ctx.gpr[4]);
        const u32 other = word(rt, ctx.gpr[5]);
        if (object != 0u) result = call(rt, ctx, 0x08B35FD8u, {object, other}) >> 31u;
    }
    ctx.gpr[2] = result;
}

// ---- 0x08B091DC: obj = this->+0; when set, 0x08B09290(obj + 0x10) then tail 0x08B64654(obj); else v0 stays. ----
void func_08B091DC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4]);
    if (object == 0u) return;
    call(rt, ctx, 0x08B09290u, {object + 0x10u});
    tail_call(ctx, 0x08B64654u, {object});
}

// ---- 0x08B17308: when this->+0x19DD is set, clear +0x19F0 and +0x19EC, 0x08B28C74(this + 0x1984),
// 0x08B28C44(this + 0x1900), then the flag. v0 = the last callee's. ----
void func_08B17308(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flag = rt.memory().load8(self + 0x19DDu);
    if (flag == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, self + 0x19F0u, 0u);
    put(rt, self + 0x19ECu, 0u);
    call(rt, ctx, 0x08B28C74u, {self + 0x1984u});
    call(rt, ctx, 0x08B28C44u, {self + 0x1900u});
    rt.memory().store8(self + 0x19DDu, 0u);
}

// ---- 0x08B48CB8: the word at this + 8 selects a string: 1 -> 0x08CB56A0, 2 -> 0x08CB5690,
// 0 -> 0x08CB5698, anything else -> 0x08CB5694. ----
void func_08B48CB8(Runtime &rt, AllegrexContext &ctx) {
    switch (word(rt, ctx.gpr[4] + 8u)) {
    case 1u: ctx.gpr[2] = 0x08CB56A0u; return;
    case 2u: ctx.gpr[2] = 0x08CB5690u; return;
    case 0u: ctx.gpr[2] = 0x08CB5698u; return;
    default: ctx.gpr[2] = 0x08CB5694u; return;
    }
}

// ---- 0x08B48DCC: the same table read at this + 0x14: 1 -> 0x08CB56F4, 2 -> 0x08CB56E8, 0 -> 0x08CB56F0,
// anything else -> 0x08CB5694. ----
void func_08B48DCC(Runtime &rt, AllegrexContext &ctx) {
    switch (word(rt, ctx.gpr[4] + 0x14u)) {
    case 1u: ctx.gpr[2] = 0x08CB56F4u; return;
    case 2u: ctx.gpr[2] = 0x08CB56E8u; return;
    case 0u: ctx.gpr[2] = 0x08CB56F0u; return;
    default: ctx.gpr[2] = 0x08CB5694u; return;
    }
}

// ---- 0x08B599E0: 0x08C7AC18(a0) a0 times (signed count, blez skips); v0 = the word at 0x08D6CE00 + 0x20. ----
void func_08B599E0(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const s32 count = static_cast<s32>(ctx.gpr[4]);
    for (s32 i = 0; i < count; ++i) call(rt, ctx, 0x08C7AC18u, {ctx.gpr[4]});
    ctx.gpr[2] = word(rt, 0x08D6CE00u + 0x20u);
}

// ---- 0x08B8FF64: 0x08B8FE44(this->+0, this->+4, &local); when this->+0, 0x08B35254(it). ----

// ---- 0x08C21258: 0x08A8B098(&local, a1, this->+8, this->+0xC); 0x08A61584(a1, &local); 0x08A5B9EC(&local). ----
void func_08C21258(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    call(rt, ctx, 0x08A8B098u, {frame.at(0u), arg, word(rt, self + 8u), word(rt, self + 0xCu)});
    call(rt, ctx, 0x08A61584u, {arg, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x08809608: 0x08808F2C(&local, a1, a2, 0, 0); when this != &local, 0x08A5BAD8(this, local[0]);
// 0x08A5B9EC(&local). ----
void func_08809608(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08808F2Cu, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], 0u, 0u});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// ---- 0x08893E90: 0x08893C78(this); then the release shape of 0x08AC32C4. ----
void func_08893E90(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08893C78u, {self});
    release_member(rt, ctx, self);
}

// ---- 0x088A2A1C: the signed byte at this->+0xC + a1 (or a1 when that byte is -1) scales a this->+4 entry
// by 124; v0 = 0x088A2934(this->+4 + value * 124, a2). ----
void func_088A2A1C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 raw = static_cast<u32>(signed_byte(rt, word(rt, self + 0xCu) + index));
    // `nor v1, zero, a0` / `movn v0, a0, v1`: the byte unless it is -1, then the index.
    const u32 value = (~raw) != 0u ? raw : index;
    const u32 scaled = (value << 7u) - (value << 2u);
    ctx.gpr[2] = call(rt, ctx, 0x088A2934u, {word(rt, self + 4u) + scaled, ctx.gpr[6]});
}

// ---- 0x088DB524: v0 = 0x088D9E58(this) != 0 && this->+0x5B8 == 0 && this->+0x25 == 0 (beql sets v1 = 1
// only on the zero byte). ----
void func_088DB524(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (call(rt, ctx, 0x088D9E58u, {self}) != 0u
        && rt.memory().load8(self + 0x5B8u) == 0u
        && rt.memory().load8(self + 0x25u) == 0u) {
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// ---- 0x089391F4: when this->+0x1C is clear, 0x08939178(this) is stored there, this->+0x20 is cleared,
// and v0 = (the new value != 0); a set +0x1C answers 0. ----
void func_089391F4(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (word(rt, self + 0x1Cu) == 0u) {
        const u32 made = call(rt, ctx, 0x08939178u, {self});
        put(rt, self + 0x1Cu, made);
        rt.memory().store8(self + 0x20u, 0u);
        result = made != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// ---- 0x08A13B98: 0x08B34888(this + 0x10); this->+0x784 = a1; tail 0x08B34B24(this + 0x10). ----

// ---- 0x08A6969C: obj = 0x08AACAF8(0x08CFF060); this->+0 = obj; when obj, tail 0x08A69638(obj); v0 = obj. ----
void func_08A6969C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = call(rt, ctx, 0x08AACAF8u, {0x08CFF060u});
    put(rt, self, object);
    if (object != 0u) tail_call(ctx, 0x08A69638u, {object});
}

// ---- 0x08A6B9EC: obj = this->+0 (0x08A5BB7C(0x08CB05D8) when null); tail 0x08A81C60(obj, a1). ----
void func_08A6B9EC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4]);
    const u32 arg = ctx.gpr[5];
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB05D8u});
    tail_call(ctx, 0x08A81C60u, {object, arg});
}

// ---- 0x08AA2B40: obj = this->+0 (0x08A5BB7C(0x08CB2B50) when null); v0 = seb(vtable[0xF] of obj)(obj). ----
void func_08AA2B40(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2B50u});
    const u32 vtable = word(rt, object);
    const u32 answer = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {object});
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(answer)));
}

// ---- 0x08AA634C: 0x08AA61D8(a0, &local, 4); v0 = the 4 bytes little-endian assembled. ----

// ---- 0x08AA9028: obj = this->+4 (0x08A5BB7C(0x08CB30B0) when null); tail 0x08C3139C(obj, a1). ----
void func_08AA9028(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5];
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    tail_call(ctx, 0x08C3139Cu, {object, arg});
}

// ---- 0x08AA9070: obj = this->+4 (checked); tail 0x08C31394(obj) with the original $f12 ($f20 restored). ----

// ---- 0x08AA9360: obj = this->+4 (checked); tail 0x08C3132C(obj, a1 & 0xFFFF). ----
void func_08AA9360(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5] & 0xFFFFu;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    tail_call(ctx, 0x08C3132Cu, {object, arg});
}

// ---- 0x08AA9558: obj = this->+4 (checked); tail 0x08C312FC(obj, a1). ----
void func_08AA9558(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4] + 4u);
    const u32 arg = ctx.gpr[5];
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    tail_call(ctx, 0x08C312FCu, {object, arg});
}

// ---- 0x08AB70A4: obj = this->+0xC (0x08A5BB7C(0x08CB4250) when null); tail 0x08AAE4E4(obj, a1). ----
void func_08AB70A4(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4] + 0xCu);
    const u32 arg = ctx.gpr[5];
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB4250u});
    tail_call(ctx, 0x08AAE4E4u, {object, arg});
}

// ---- 0x08ACC13C: 0x08ACC0EC(this); then the release shape of 0x08AC32C4. ----

} // namespace
} // namespace new_batch_main_1566
} // namespace gt::game

#define NEWMAIN1566(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1566::name, gt::ReturnKind::kind)
NEWMAIN1566(0x08AB1444u, func_08AB1444, Word);
NEWMAIN1566(0x08AB3E04u, func_08AB3E04, Word);
NEWMAIN1566(0x08AC3784u, func_08AC3784, Word);
NEWMAIN1566(0x08AD68C8u, func_08AD68C8, Word);
NEWMAIN1566(0x08AFC92Cu, func_08AFC92C, Word);
NEWMAIN1566(0x08B091DCu, func_08B091DC, Word);
NEWMAIN1566(0x08B17308u, func_08B17308, Word);
NEWMAIN1566(0x08B48CB8u, func_08B48CB8, Word);
NEWMAIN1566(0x08B48DCCu, func_08B48DCC, Word);
NEWMAIN1566(0x08B599E0u, func_08B599E0, Word);
NEWMAIN1566(0x08C21258u, func_08C21258, Word);
NEWMAIN1566(0x08809608u, func_08809608, Word);
NEWMAIN1566(0x08893E90u, func_08893E90, Word);
NEWMAIN1566(0x088A2A1Cu, func_088A2A1C, Word);
NEWMAIN1566(0x088DB524u, func_088DB524, Word);
NEWMAIN1566(0x089391F4u, func_089391F4, Word);
NEWMAIN1566(0x08A6969Cu, func_08A6969C, Word);
NEWMAIN1566(0x08A6B9ECu, func_08A6B9EC, Word);
NEWMAIN1566(0x08AA2B40u, func_08AA2B40, Word);
NEWMAIN1566(0x08AA9028u, func_08AA9028, Word);
NEWMAIN1566(0x08AA9360u, func_08AA9360, Word);
NEWMAIN1566(0x08AA9558u, func_08AA9558, Word);
NEWMAIN1566(0x08AB70A4u, func_08AB70A4, Word);
