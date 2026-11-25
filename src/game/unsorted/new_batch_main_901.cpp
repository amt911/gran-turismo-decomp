// New batch 902 (main session, draft-only): constructors and float helpers the gate could not pose inputs for, written by hand
// from `tools.disasm`; role words describe access shapes, none is a game name. Input shapes live in
// shapes_main902.txt (config/synth_shapes format): one line per function, asm evidence next to it.
//
// Every call passes the argument registers the original sets before the jal (`call_set`); the delay slot of a `jal` runs
// before the call; v0 is set as the original leaves it on every path; frames that callees read are reproduced
// (`GuestFrame`, offsets as in the original), the dead stack is not.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_902 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of the callee are what the next instruction sees, like the original's registers.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// The same with float argument registers (f12-f15) as well.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `ins rt, rs, pos, size`: the size bits at pos of `base` become the low bits of `value`, the rest stand.
inline u32 ins(u32 base, u32 value, unsigned position, unsigned size) {
    const u32 low = size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u);
    const u32 mask = low << position;
    return (base & ~mask) | ((value << position) & mask);
}
// Signed half of a word: `srl v, x, 31; addu v, v, x; sra v, v, 1` — a division by two truncating toward zero.
inline s32 half_word(s32 value) { return value / 2; }

// 0x089F3A70: constructor — vtable words, eight sub-object initialisers and two 16.0f fields; v0 is the last initialiser's.

// 0x089AC110: four corner calls into 0x089AC0A0 (f12-f15), the last one a tail call; a0 is the same object throughout.

// 0x0892EEC0: clears a struct (three memset calls, zeros around them); v0 is the last memset's answer.

// 0x08922BFC: constructor — five sub-objects at +0x1C..+0xCC, each initialised then given a name constant; the last is a tail call.

// 0x08A064A0: constructor — a vtable, an 8-byte allocation stored three times, a stack object built by two callees and then
// handed on; v0 ends as the field at +0xA8.

// 0x089F9578: the same shape with three stack objects (+0x04, +0x24, +0x34), four float fills and two releasing calls.

// 0x088F7558: sizes a rectangle into a2 from a1[4] and a3's scale fields, then writes a1 as its position plus a scaled step.

// 0x0899DBA8: constructor — vtable, a flag word at +0x13C, float setup, four sub-objects and a two-float stack pair; v0 is
// the +0x110 word it stores last.

// 0x089057C8: builds a rotation matrix at a0 from a1's yaw (+0x24), two half-word extents (+0x10/+0x14) and a scale pair
// (+0x18..+0x20); v0 ends as a0+4.

// 0x089B6584: constructor — vtable, four zeroed sub-objects, a byte flag block at +0x138 and a two-float stack pair;
// v0 is the +0x110 word it stores last.

// 0x089A5DD0: constructor — two vtables, a name/descriptor call at +0x128, a flag word at +0x138 (bit 7 from the global byte
// at 0x08CF38F0), two two-way lists and a flag word at +0x60; v0 is that last word.

// 0x08918498: two matrix products over a stack matrix built from an array element (a0 + 4*a1 + 8) and two stack vectors;
// the projected point goes to a3 and v0 is whether the depth was positive.

// 0x088F7660: scales a0's scrolling step (+0x18) by its rate pair (+0x1C..+0x28) and a1's fields, nudges a0's +0x10 run
// pointer and the +0x14/+0x18 pair, then tails into 0x088F6804 with a0, a1 and f12.

// 0x0896BCAC: constructor — vtable, a 128.0f pair at +0x78, a name/descriptor call, a wide flag word at +0xE0 and the float
// block at +0xA4..+0xDC; v0 is the +0xE0 word.

// 0x08983CE8: fills the graphics state object at 0x08CF68E8 from a0 (position, four sub-matrices, flag bits) and hands it to
// three submit calls; v0 ends as the flag word at state+0x58 with bit 16 set from a0+0xA9.

// 0x08A13FC8: constructor — a base call, a vtable, a descriptor looked up through two calls and a stack record, four
// sub-objects, a float pair and a long block of zeroed fields; v0 is the last registration call's answer.

// 0x0887D0F4: constructor — a long chain of one-argument initialisers for each member, the floats written between them;
// v0 is 1.

// 0x08984608: constructor — a vtable at +0, three two-word float/flag records built through 0x08964224 + 0x0896418C, a byte
// flag block and two 32-byte allocations split into two halves; v0 is the second half's end.

} // namespace
} // namespace new_batch_main_902
} // namespace gt::game

#define NEW902(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_902::name, gt::ReturnKind::kind)
