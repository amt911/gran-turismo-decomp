// Main batch 872: re-drafts of functions an earlier batch already registered and that mismatched the original in the gate.
// Written from tools.disasm; each comment says what the gate saw and what the asm says instead.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_872 {
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
// A call whose answer arrives in v0 and v1 (a pair of pointers) and, in f0, a float: put back into the context as the
// `jal` leaves them, since the caller reads more than v0.
GuestResult call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_pair_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                            std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// The `ins` instruction: `size` bits of `value` replace the same bits of `destination` from `position` up.
u32 insert_bits(u32 destination, u32 value, unsigned position, unsigned size) {
    const u32 mask = ((1u << size) - 1u) << position;
    return (destination & ~mask) | ((value << position) & mask);
}
// The PSP's float-to-integer sequence (trunc.w.s with the 2^31 fix-up): exact for the values the code feeds it.
u32 float_to_u32(float value) {
    if (value >= 2147483648.0f) return static_cast<u32>(static_cast<s32>(std::trunc(value - 2147483648.0f))) | 0x80000000u;
    return static_cast<u32>(static_cast<s32>(std::trunc(value)));
}
// The vertex-colour packing below: a value under 256 becomes the alpha byte over a white rgb, anything else is opaque white.
u32 pack_alpha(float value) {
    const u32 index = float_to_u32(value);
    return index < 0x100u ? ((index << 24) | 0x00FFFFFFu) : 0xFFFFFFFFu;
}

// 0x088AA774 (a0 = the object, a1 = a position, f12..f15 = the four axis scales). The gate saw RAM at 0x09ED11E4 (the
// first point's x) zero in ours against a value in the original, 571 bytes of the block apart: every base below hangs off
// s4 = a0 + 0xDC, not off the memset's a0 + 0x5C (they differ by 0x80, and the point rows start exactly there).
// Rebuilds the 0x280-byte block at +0x5C from the record at word[word[a0+0x4C]+0x14]: clears it, writes the (at most 127)
// 4-word points relative to the position (w = 1), packs the record's three signed row counts into the field word at +0x2D8
// (4 bits each at 0, 4, 8 and 12, 16; 5 bits at 20 and 25), fills the second group of rows (0x80 + 0xE0, stride 0x10, the
// fourth float plus 0x08BAE7C8's answer) and the byte pairs at +0x29C, then the two 4-float vectors at +0x27C / +0x28C for
// 0x08BAE824, and finally the object's own defaults. v0 = 1, or 0 when the object has no record.

// 0x089006DC / 0x08900A38 (a0 = the object): constructor. The gate saw the global at 0x08D75498 (a count) and the
// registration slot at 0x08D75C68 left behind by ours — the call to 0x08BA1BA8 on a0 + 0x10 (0x089007F8, its store of the
// zero at +0x28 sitting in the delay slot) was missing from the earlier draft. Three vtable words go to +0, +4, +8, then
// the member constructors run one after another on object + offset, each field defaulted right after its own constructor,
// and the body ends in a tail call to 0x089000CC with a0 = the object and v0 = the last call's result.
void func_089006DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const float one = 1.0f;
    const float half = 0.5f;
    const float hundred = std::bit_cast<float>(0x42C80000u);   // 100.0
    const float thousand = std::bit_cast<float>(0x447A0000u);  // 1000.0
    GuestFrame frame(ctx, 0x50u);

    put(rt, object, 0x08CC5408u);
    put(rt, object + 4u, 0x08CC5438u);
    put(rt, object + 8u, 0x08CC5454u);

    call(rt, ctx, 0x08BA0210u, {object + 0xCu});
    call(rt, ctx, 0x08BA0140u, {object + 0xCu});
    call(rt, ctx, 0x08BA1C34u, {object + 0xCu});
    call(rt, ctx, 0x08BA2360u, {object + 0x10u});
    put_float(rt, object + 0x10u, one);
    call(rt, ctx, 0x08BA22D4u, {object + 0x14u});
    put_float(rt, object + 0x14u, std::bit_cast<float>(0x3F666666u));  // 0.9
    call(rt, ctx, 0x08BA21E4u, {object + 0x18u});
    put_float(rt, object + 0x18u, thousand);
    call(rt, ctx, 0x08BA20F4u, {object + 0x1Cu});
    put_float(rt, object + 0x1Cu, one);
    call(rt, ctx, 0x08BA2004u, {object + 0x20u});
    put(rt, object + 0x20u, 0u);
    call(rt, ctx, 0x08BA1F14u, {object + 0x24u});
    put_float(rt, object + 0x24u, std::bit_cast<float>(0x43480000u));  // 200.0
    call(rt, ctx, 0x08BA1E24u, {object + 0x28u});
    put(rt, object + 0x28u, 0u);
    call(rt, ctx, 0x08BA1BA8u, {object + 0x10u});
    call(rt, ctx, 0x08BA1704u, {object + 0x2Cu});
    call(rt, ctx, 0x08BA1678u, {object + 0x3Cu});
    put_float(rt, object + 0x3Cu, hundred);
    call(rt, ctx, 0x08BA1588u, {object + 0x40u});
    put_float(rt, object + 0x40u, thousand);
    call(rt, ctx, 0x08BA1498u, {object + 0x44u});
    put(rt, object + 0x44u, 0u);
    call(rt, ctx, 0x08BA1AB8u, {object + 0x2Cu});
    call(rt, ctx, 0x08BA3718u, {object + 0x48u});
    call(rt, ctx, 0x08B9FE44u, {object + 0x3E8u});
    call(rt, ctx, 0x08BA1964u, {object + 0x3E8u});
    call(rt, ctx, 0x08BC624Cu, {object + 0x3F8u});
    const u32 quaternion = object + 0x3F8u;
    put_float(rt, quaternion, one);
    put_float(rt, quaternion + 4u, one);
    put_float(rt, quaternion + 8u, one);
    put_float(rt, quaternion + 0xCu, half);
    call(rt, ctx, 0x08BCC9C8u, {quaternion});
    call(rt, ctx, 0x08BCBD68u, {object + 0x408u});
    call(rt, ctx, 0x08BCBCDCu, {object + 0x40Cu});
    call(rt, ctx, 0x08BCBBECu, {object + 0x410u});
    call(rt, ctx, 0x08BCBAFCu, {object + 0x414u});
    call(rt, ctx, 0x08BCBA0Cu, {object + 0x418u});
    call(rt, ctx, 0x08BCB91Cu, {object + 0x41Cu});
    call(rt, ctx, 0x08BCB82Cu, {object + 0x420u});
    call(rt, ctx, 0x08BCB73Cu, {object + 0x424u});
    call(rt, ctx, 0x08BCB64Cu, {object + 0x428u});
    call(rt, ctx, 0x08BCB55Cu, {object + 0x42Cu});
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);  // 0.1
    put_float(rt, object + 0x42Cu, tenth);
    put_float(rt, object + 0x414u, tenth);
    put_float(rt, object + 0x41Cu, tenth);
    put_float(rt, object + 0x424u, tenth);
    put_float(rt, object + 0x428u, half);
    put_float(rt, object + 0x408u, one);
    put_float(rt, object + 0x40Cu, half);
    put_float(rt, object + 0x410u, half);
    put_float(rt, object + 0x418u, half);
    put_float(rt, object + 0x420u, half);
    call(rt, ctx, 0x08BCC93Cu, {object + 0x408u});
    call(rt, ctx, 0x08BC5B70u, {object + 0x430u});
    call(rt, ctx, 0x08BC5C54u, {object + 0x440u});
    const u32 vector = object + 0x430u;
    put(rt, vector, 0u);
    put_float(rt, vector + 0xCu, one);
    put(rt, vector + 4u, 0u);
    put(rt, vector + 8u, 0u);
    put_float(rt, object + 0x440u, one);
    call(rt, ctx, 0x08BCC84Cu, {vector});
    call(rt, ctx, 0x08BBA094u, {object + 0x444u});
    call(rt, ctx, 0x08BCC75Cu, {object + 0x444u});
    call(rt, ctx, 0x08BBA094u, {object + 0x454u});
    call(rt, ctx, 0x08BCC66Cu, {object + 0x454u});
    call(rt, ctx, 0x08BBA094u, {object + 0x464u});
    call(rt, ctx, 0x08BCC57Cu, {object + 0x464u});
    call(rt, ctx, 0x08BBA094u, {object + 0x474u});
    call(rt, ctx, 0x08BCC48Cu, {object + 0x474u});
    call(rt, ctx, 0x08BBA094u, {object + 0x484u});
    call(rt, ctx, 0x08BCC39Cu, {object + 0x484u});
    call(rt, ctx, 0x08BBA094u, {object + 0x494u});
    call(rt, ctx, 0x08BCC2ACu, {object + 0x494u});
    call(rt, ctx, 0x08BBA1DCu, {object + 0x4A4u});
    call(rt, ctx, 0x08BCC1BCu, {object + 0x4A4u});
    call(rt, ctx, 0x08BC52E0u, {object + 0x4A8u});
    put_float(rt, object + 0x4A8u, hundred);
    call(rt, ctx, 0x08BC53B0u, {object + 0x4ACu});
    put_float(rt, object + 0x4ACu, thousand);
    call(rt, ctx, 0x08BC5480u, {object + 0x4B0u});
    put(rt, object + 0x4B0u, 0u);
    call(rt, ctx, 0x08BC5550u, {object + 0x4B4u});
    put8(rt, object + 0x4B4u, 0u);
    const u32 result = call(rt, ctx, 0x08BCC0CCu, {object + 0x4A8u});
    ctx.gpr[2] = result;
    tail_call(ctx, 0x089000CCu, {object});
}
// 0x08900A38: the same 215 words.

// 0x088C9C08 (a0 = the object, a1 = forwarded to 0x08AF26F0). The gate saw v0 = 0x1DB31DB1 against the original's 0:
// the walk out of the two loops ends on `slt  a0+1, word[a0+0x28]` and that 0 is the return value, so v0 is a constant 0
// there — only the count <= 0 exit returns the count itself. Draws count table blocks of 2 rows x 20 records: each record
// is 0x18 bytes at object + 0xAB0 + 0x3F0 * k + 0x1F8 * j + 0x18 * i (colour = 0/200 in the alpha byte over white), and
// the 0x60-byte scratch block is filled and handed to 0x08B555F8.

// 0x08AFFECC (a0 = the object): rebuilds the object's quaternion at +0 from the transform at +0xB0 and the records the
// object at word[a0+0xD8] holds (the halfword count at +0x7E, entries of 0x200 bytes from +0x80, the smallest y wins),
// then updates the float at +0x24. The gate saw the top byte of the floats at +0, +8, +0x24, +0xA0 and +0xAC differ by the
// sign bit only (0x7F against 0xFF: an infinity's sign), i.e. an operand order flipped in the quaternion product; the
// product below is transcribed term by term from 0x08B00040..0x08B000CC (v0 = the first four floats, v1 = the second).

} // namespace
} // namespace new_batch_main_872
} // namespace gt::game


#define NEW872(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_872::name, gt::ReturnKind::kind)
NEW872(0x089006DCu, func_089006DC, Word);
