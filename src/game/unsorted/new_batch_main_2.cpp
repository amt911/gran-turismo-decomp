// New batch, main session 2: small leaves (recompiler-emitted, not decompiled yet), written as structured C from
// `tools.disasm` and verified by synthetic cases (the routes reach few of them). Shapes: stores through a member
// pointer word at +0, table/vtable stores, byte-swapped halfword getters, address computations, and one-argument
// tail calls (argument registers the original leaves unchanged are left to the caller's context, as `tail_call`
// does). No names: none has evidence.
// Verified by synthetic cases (v1+v2, 11 scenes): 29 functions compared, 0 mismatched, sabotage pinned 29/29; the
// candidates that never compared (35) or mismatched (1) were dropped, code and registration both.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2 {
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
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// `wsbh`: the bytes of each halfword swap; on a 16-bit value that is a byte swap of the low half.
u32 swap_half(u32 value) { return __builtin_bswap16(static_cast<std::uint16_t>(value)); }

// A VFPU scalar register number: S<m><c><r> is m * 4 + c + r * 0x20 (include/gt/vfpu.hpp), so row r of matrix 0,
// column 0 is r * 0x20 (S000, S001, ...).
constexpr u32 vfpu_scalar(u32 row) { return row * 0x20u; }

// 0x08817A0C (a0 = object): v0 = the word at +0 plus one, stored back at +0.
void func_08817A0C(Runtime &rt, AllegrexContext &ctx) {
    const u32 counter = word(rt, ctx.gpr[4]) + 1u;
    put(rt, ctx.gpr[4], counter);
    ctx.gpr[2] = counter;
}

// 0x0881ACA0 (a0 = base, a1 = index): v0 = the address of element a1 (stride 4) after the 0x34-byte header.
void func_0881ACA0(Runtime &, AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] + (ctx.gpr[5] << 2) + 0x34u;
}

// 0x0885BBC8 (a0 = object, a2, a3): v0 = the word at +0; stores a2 at +0x28 and a3 at +0x2C of it.
void func_0885BBC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    put(rt, holder + 0x28u, ctx.gpr[6]);
    put(rt, holder + 0x2Cu, ctx.gpr[7]);
    ctx.gpr[2] = holder;
}

// 0x0885EF5C (a0 = object): calls func_0885E170(word(a0)); v0 = its result.
void func_0885EF5C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x0885E170u,
         {word(rt, ctx.gpr[4]), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
}

// 0x08870448 (a0 = object, f12-f14): stores the three floats at +0x30/+0x34/+0x38 of word(a0 + 0xC); v0 = it.
void func_08870448(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4] + 0xCu);
    put_float(rt, holder + 0x30u, ctx.fpr[12]);
    put_float(rt, holder + 0x34u, ctx.fpr[13]);
    put_float(rt, holder + 0x38u, ctx.fpr[14]);
    ctx.gpr[2] = holder;
}

// 0x088728B0: tail call func_08872834(word(a0 + 0x5C), a0 + 0xC).
void func_088728B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    tail_call(ctx, 0x08872834u, {word(rt, self + 0x5Cu), self + 0xCu});
}

// 0x08891534 (a0 = object): stores the byte 1 at +0x11 of the word at +0; v0 = 1.
void func_08891534(Runtime &rt, AllegrexContext &ctx) {
    rt.memory().store8(word(rt, ctx.gpr[4]) + 0x11u, 1u);
    ctx.gpr[2] = 1u;
}

// 0x088970A0 (a0 = object, a1 = flags): v0 = word(a0) + 0x8C; sets its +0xD byte to the old one ORed with a1.
void func_088970A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 block = word(rt, ctx.gpr[4]) + 0x8Cu;
    const u32 flags = ctx.gpr[5] | rt.memory().load8(block + 0xDu);
    rt.memory().store8(block + 0xDu, static_cast<std::uint8_t>(flags));
    ctx.gpr[2] = block;
}

// 0x08897118 (a0 = object): v0 = the signed byte at +0xB1 of word(a0) when positive, else the one at +0xB0
// (`blezl`: the delayed byte load runs only on the taken path).
void func_08897118(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    const s32 flag = signed_byte(rt, object + 0xB1u);
    ctx.gpr[2] = flag > 0 ? static_cast<u32>(flag) : static_cast<u32>(signed_byte(rt, object + 0xB0u));
}

// 0x088A5804 (a0 = object): v0 = the halfword at +0x4BE of the word at +0, bytes swapped.
void func_088A5804(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = swap_half(rt.memory().load16(word(rt, ctx.gpr[4]) + 0x4BEu));
}

// 0x088A5A90 (a0 = object): v0 = the signed halfword at +0x1A70 of the word at +0x4C8 of the word at +0.
void func_088A5A90(Runtime &rt, AllegrexContext &ctx) {
    const u32 inner = word(rt, word(rt, ctx.gpr[4]) + 0x4C8u);
    ctx.gpr[2] = static_cast<u32>(signed_half(rt, inner + 0x1A70u));
}

// 0x088A5D3C (a0 = object, a1 = a halfword): v0 = word(a0); stores a1 (low 16 bits, bytes swapped) at its +0x4B8.
void func_088A5D3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    rt.memory().store16(holder + 0x4B8u, static_cast<std::uint16_t>(swap_half(ctx.gpr[5] & 0xFFFFu)));
    ctx.gpr[2] = holder;
}

// 0x088A75F8 (a0 = object): v0 = the halfword at +0x4B8 of the word at +0, bytes swapped.
void func_088A75F8(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = swap_half(rt.memory().load16(word(rt, ctx.gpr[4]) + 0x4B8u));
}

// 0x088A7618 (a0 = object): v0 = the halfword at +0x4BC of the word at +0, bytes swapped.
void func_088A7618(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = swap_half(rt.memory().load16(word(rt, ctx.gpr[4]) + 0x4BCu));
}

// 0x088D5AA8 (a0 = object, a1 = a halfword): v0 = the word at +0; stores a1 bytes swapped at its +0xE.
void func_088D5AA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    rt.memory().store16(holder + 0xEu, static_cast<std::uint16_t>(swap_half(ctx.gpr[5])));
    ctx.gpr[2] = holder;
}

// 0x088D6204 (a0 = object): v0 = the halfword at +0xA of word(a0), bytes swapped and sign-extended from 16 bits
// (`wsbw` then `sra 16`).
void func_088D6204(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = holder;
    ctx.gpr[2] = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(
        swap_half(rt.memory().load16(holder + 0xAu)))));
}

// 0x08909AF0 (a1 = three floats): tail call func_08909A5C with them in $f12-$f14.
void func_08909AF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 floats = ctx.gpr[5];
    ctx.fpr[12] = float_at(rt, floats);
    ctx.fpr[13] = float_at(rt, floats + 4u);
    ctx.fpr[14] = float_at(rt, floats + 8u);
    tail_call(ctx, 0x08909A5Cu, {});
}

// 0x08917E80 (a0 = base, a3 = index): tail call func_08927550(word(base + (a3 << 2) + 8), a1, a2, base + (a3 << 2)).
void func_08917E80(Runtime &rt, AllegrexContext &ctx) {
    const u32 element = ctx.gpr[4] + (ctx.gpr[7] << 2);
    tail_call(ctx, 0x08927550u, {word(rt, element + 8u), ctx.gpr[5], ctx.gpr[6], element});
}

// 0x0892C8A4 (a0): tail call func_08B20A54(0x08CF2958, a0, -1).
void func_0892C8A4(Runtime &, AllegrexContext &ctx) {
    tail_call(ctx, 0x08B20A54u, {0x08CF2958u, ctx.gpr[4], 0xFFFFFFFFu});
}

// 0x0892D71C (a0 = source, a1 = destination, a2 = a halfword): copies the byte at a0 + 0x15D to a1 + 0x11D and the
// halfword at a2 to a1 + 0x11E; v0 = the byte.
void func_0892D71C(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4], out = ctx.gpr[5];
    const u32 value = rt.memory().load8(source + 0x15Du);
    rt.memory().store8(out + 0x11Du, static_cast<std::uint8_t>(value));
    const u32 half = rt.memory().load16(ctx.gpr[6]);
    rt.memory().store16(out + 0x11Eu, static_cast<std::uint16_t>(half));
    ctx.gpr[2] = value;
    ctx.gpr[3] = half;
}

// 0x089300B0: a pair getter: v0 = a0, v1 = 0x22.
void func_089300B0(Runtime &, AllegrexContext &ctx) {
    ctx.gpr[3] = 0x22u;
    ctx.gpr[2] = ctx.gpr[4];
}

// 0x08B2B0A8: stores the vtable 0x08CDA6C0 at *this; v0 = it (a base constructor).
void func_08B2B0A8(Runtime &rt, AllegrexContext &ctx) {
    put(rt, ctx.gpr[4], 0x08CDA6C0u);
    ctx.gpr[2] = 0x08CDA6C0u;
}

// 0x08B2EFDC (a0, a1): tail call func_08B1C590(0x08D69138, a1, a0, a1).
void func_08B2EFDC(Runtime &, AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C590u, {0x08D69138u, ctx.gpr[5], ctx.gpr[4], ctx.gpr[5]});
}

// 0x08B566E0 (a1 = four matrix columns): loads VFPU C000..C030 from a1 and tail calls func_08B5661C.
void func_08B566E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 matrix = ctx.gpr[5];
    for (u32 column = 0u; column < 4u; ++column) gt::vfpu::load_column(rt, ctx, 0u, column, matrix + 0x10u * column);
    tail_call(ctx, 0x08B5661Cu, {});
}

// 0x08B57E68 (a0 = four floats): loads them into VFPU S000..S003 (C000) and tail calls func_08B57DF4.
void func_08B57E68(Runtime &rt, AllegrexContext &ctx) {
    const u32 floats = ctx.gpr[4];
    gt::vfpu::load_scalar(rt, ctx, vfpu_scalar(0u), floats);
    gt::vfpu::load_scalar(rt, ctx, vfpu_scalar(1u), floats + 4u);
    gt::vfpu::load_scalar(rt, ctx, vfpu_scalar(2u), floats + 8u);
    gt::vfpu::load_scalar(rt, ctx, vfpu_scalar(3u), floats + 0xCu);
    tail_call(ctx, 0x08B57DF4u, {});
}

// 0x08B5C280: stores the vtable 0x08CDAE78 at *this; v0 = it.
void func_08B5C280(Runtime &rt, AllegrexContext &ctx) {
    put(rt, ctx.gpr[4], 0x08CDAE78u);
    ctx.gpr[2] = 0x08CDAE78u;
}

// 0x08B6258C: stores the vtable 0x08CDBF58 at *this; v0 = it.
void func_08B6258C(Runtime &rt, AllegrexContext &ctx) {
    put(rt, ctx.gpr[4], 0x08CDBF58u);
    ctx.gpr[2] = 0x08CDBF58u;
}

// 0x08B89CF0: stores the vtable 0x08CBE300 at *this; v0 = it.
void func_08B89CF0(Runtime &rt, AllegrexContext &ctx) {
    put(rt, ctx.gpr[4], 0x08CBE300u);
    ctx.gpr[2] = 0x08CBE300u;
}

// 0x08BB88FC (a0 = start, a1 = end, a2 = a word source): fills the bytes [a0, a1) with the low byte of the word at
// a2, re-reading it every iteration (`bnel`'s delay slot); v0 = the last value loaded, untouched when a0 == a1.
void func_08BB88FC(Runtime &rt, AllegrexContext &ctx) {
    u32 position = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    while (position != end) {
        const u32 value = word(rt, ctx.gpr[6]);
        ctx.gpr[2] = value;
        rt.memory().store8(position, static_cast<std::uint8_t>(value));
        ++position;
    }
    ctx.gpr[4] = position;
}

} // namespace
} // namespace new_batch_main_2
} // namespace gt::game

#define NEWM2(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2::name, gt::ReturnKind::kind)
NEWM2(0x08817A0Cu, func_08817A0C, Word);
NEWM2(0x0881ACA0u, func_0881ACA0, Word);
NEWM2(0x0885BBC8u, func_0885BBC8, Word);
NEWM2(0x0885EF5Cu, func_0885EF5C, Word);
NEWM2(0x08870448u, func_08870448, Word);
NEWM2(0x088728B0u, func_088728B0, Word);
NEWM2(0x08891534u, func_08891534, Word);
NEWM2(0x088970A0u, func_088970A0, Word);
NEWM2(0x08897118u, func_08897118, Word);
NEWM2(0x088A5804u, func_088A5804, Word);
NEWM2(0x088A5A90u, func_088A5A90, Word);
NEWM2(0x088A5D3Cu, func_088A5D3C, Word);
NEWM2(0x088A75F8u, func_088A75F8, Word);
NEWM2(0x088A7618u, func_088A7618, Word);
NEWM2(0x088D5AA8u, func_088D5AA8, Word);
NEWM2(0x088D6204u, func_088D6204, Word);
NEWM2(0x08909AF0u, func_08909AF0, Word);
NEWM2(0x08917E80u, func_08917E80, Word);
NEWM2(0x0892C8A4u, func_0892C8A4, Word);
NEWM2(0x0892D71Cu, func_0892D71C, Word);
NEWM2(0x089300B0u, func_089300B0, DoubleWord);
NEWM2(0x08B2B0A8u, func_08B2B0A8, Word);
NEWM2(0x08B2EFDCu, func_08B2EFDC, Word);
NEWM2(0x08B566E0u, func_08B566E0, Word);
NEWM2(0x08B57E68u, func_08B57E68, Word);
NEWM2(0x08B5C280u, func_08B5C280, Word);
NEWM2(0x08B6258Cu, func_08B6258C, Word);
NEWM2(0x08B89CF0u, func_08B89CF0, Word);
NEWM2(0x08BB88FCu, func_08BB88FC, Word);
