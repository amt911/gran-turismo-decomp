// Hot batch 144: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_144 {
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

// handle_from(out, -, key, ...) with the member's lookup and maker: found = lookup(key); out = {0, 0, byte 0}; when
// found, maker(out, found, 0) (a3 as it came). Returns out.
void handle_from(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t lookup, std::uint32_t maker) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_set(rt, ctx, lookup, {ctx.gpr[6]}).v0;
    ctx.gpr[4] = out;
    ctx.gpr[5] = found;
    ctx.gpr[6] = 0u;
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    rt.memory().store8(out + 8u, 0u);
    if (found != 0u) call_set(rt, ctx, maker, {out, found, 0u});
    ctx.gpr[2] = out;
}

// name_reset(this): *this = 0x08CCD990, then the shared string whose data pointer is at +4 released (libstdc++
// copy-on-write: unless it is the empty rep 0x08D74FD0, func_08B60804(&count, -1), and func_08B8542C(rep, sp) when the
// old count was not positive). v0 is the vtable on the early return.
void name_reset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = word(rt, self + 4u);
    const std::uint32_t rep = text - 0xCu;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = self;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[2] = 0x08CCD990u;
    put(rt, self, 0x08CCD990u);
    if (rep == 0x08D74FD0u) return;
    if (static_cast<std::int32_t>(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}).v0) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
}

// release_and_destroy(this, ...) with the member's vtable and base destructor: *this = vtable; when the holder at
// this + 0x14 has an object (+4), its first virtual runs with (object, this->+0x14), then the object and
// +0x14 are cleared; then the base destructor (tail call).
void release_and_destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = self + 0x14u;
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
    const std::uint32_t object = word(rt, holder + 4u);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t table = word(rt, object);
        ctx.gpr[3] = table;
        call_set(rt, ctx, word(rt, table), {object, word(rt, self + 0x14u)});
        put(rt, holder + 4u, 0u);
        put(rt, self + 0x14u, 0u);
    }
    tail_call(ctx, destroy, {self});
}

} // namespace

// Templates of their families (the members are the batch 144 rows below).
void func_08B9CE3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08B9C7D4u, 0x08B9BD70u); }
void func_08BE9AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08818490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBE1F0u, 0x08A780B0u); }

void func_08BA0494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0394u, 0x08B9CF6Cu); }
void func_08BA05F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA04F4u, 0x08B9D15Cu); }
void func_08BA0754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0654u, 0x08B9D388u); }
void func_08BA08B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA07B4u, 0x08B9D5B4u); }
void func_08BA0A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0914u, 0x08B9D7E0u); }
void func_08BA0B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0A74u, 0x08B9DA24u); }
void func_08BA0CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0BD4u, 0x08B9DC50u); }
void func_08BA0F34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0E34u, 0x08B9E2D4u); }
void func_08BA1094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0F94u, 0x08B9E548u); }
void func_08BA11F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA10F4u, 0x08B9E774u); }
void func_08BA1354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA1254u, 0x08B9E984u); }
void func_08BA2AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BA0D34u, 0x08B9DE60u); }
void func_08BB9D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BB9C24u, 0x08BB992Cu); }
void func_08BB9F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BB9E4Cu, 0x08BB9718u); }
void func_08BC21B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BBE2D0u, 0x08BBD0E4u); }
void func_08BCA018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BC9F18u, 0x08BC4094u); }
void func_08BCA178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA078u, 0x08BC420Cu); }
void func_08BCA2D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA1D8u, 0x08BC4384u); }
void func_08BCA438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA338u, 0x08BC44FCu); }
void func_08BCA598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA498u, 0x08BC4674u); }
void func_08BCA6F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA5F8u, 0x08BC47ECu); }
void func_08BCA858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA758u, 0x08BC4964u); }
void func_08BCA9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCA8B8u, 0x08BC4ADCu); }
void func_08BCAB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCAA18u, 0x08BC4C54u); }
void func_08BCAC78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCAB78u, 0x08BC4DDCu); }
void func_08BCADD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCACD8u, 0x08BC4F64u); }
void func_08BCAF38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCAE38u, 0x08BC5178u); }
void func_08BCB098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCAF98u, 0x08BC5A08u); }
void func_08BCB1F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCB0F8u, 0x08BC5E74u); }
void func_08BCB358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08BCB258u, 0x08BC60E4u); }
void func_08C4E7B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_from(rt, ctx, 0x08C4E6B0u, 0x08C4D23Cu); }
void func_08BEC1A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC27C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC6B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEC78C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BED7F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BED8C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEF868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BEF940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BF3A10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BF5534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08BF560C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C09434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C0CB80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C0CC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C0CD30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C0F4E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C11B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C11C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_08C13CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { name_reset(rt, ctx); }
void func_0881C318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBE450u, 0x08A780B0u); }
void func_0883909C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBDAB8u, 0x08A780B0u); }
void func_08841A00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD9A8u, 0x08A780B0u); }
void func_08843B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD898u, 0x08A780B0u); }
void func_08844BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD788u, 0x08A780B0u); }
void func_08845AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD678u, 0x08A780B0u); }
void func_08847A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD568u, 0x08A780B0u); }
void func_0884DC44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD018u, 0x08A780B0u); }
void func_08854DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD238u, 0x08A780B0u); }
void func_088589F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBD128u, 0x08A780B0u); }
void func_08A1B650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CD0C48u, 0x08A780B0u); }
void func_08A1C4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CD0B38u, 0x08A780B0u); }
void func_08A1D658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CD0A28u, 0x08A780B0u); }
void func_08A2D540(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CBE6A0u, 0x08A780B0u); }
void func_08A319B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CD14C0u, 0x08A780B0u); }
void func_08A378C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_and_destroy(rt, ctx, 0x08CD17F0u, 0x08A780B0u); }

} // namespace hot_batch_144
} // namespace gt::game

#define HOT144(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_144::name, gt::ReturnKind::kind)
HOT144(0x08B9CE3Cu, func_08B9CE3C, Word);
HOT144(0x08BE9AFCu, func_08BE9AFC, Word);
HOT144(0x08818490u, func_08818490, Word);
HOT144(0x08BA0494u, func_08BA0494, Word);
HOT144(0x08BA05F4u, func_08BA05F4, Word);
HOT144(0x08BA0754u, func_08BA0754, Word);
HOT144(0x08BA08B4u, func_08BA08B4, Word);
HOT144(0x08BA0A14u, func_08BA0A14, Word);
HOT144(0x08BA0B74u, func_08BA0B74, Word);
HOT144(0x08BA0CD4u, func_08BA0CD4, Word);
HOT144(0x08BA0F34u, func_08BA0F34, Word);
HOT144(0x08BA1094u, func_08BA1094, Word);
HOT144(0x08BA11F4u, func_08BA11F4, Word);
HOT144(0x08BA1354u, func_08BA1354, Word);
HOT144(0x08BA2AF4u, func_08BA2AF4, Word);
HOT144(0x08BB9D24u, func_08BB9D24, Word);
// HOT144(0x08BB9F4Cu, func_08BB9F4C, Word);  // no comparable synthetic case yet
HOT144(0x08BC21B4u, func_08BC21B4, Word);
HOT144(0x08BCA018u, func_08BCA018, Word);
HOT144(0x08BCA178u, func_08BCA178, Word);
HOT144(0x08BCA2D8u, func_08BCA2D8, Word);
HOT144(0x08BCA438u, func_08BCA438, Word);
HOT144(0x08BCA598u, func_08BCA598, Word);
HOT144(0x08BCA6F8u, func_08BCA6F8, Word);
HOT144(0x08BCA858u, func_08BCA858, Word);
HOT144(0x08BCA9B8u, func_08BCA9B8, Word);
HOT144(0x08BCAB18u, func_08BCAB18, Word);
HOT144(0x08BCAC78u, func_08BCAC78, Word);
HOT144(0x08BCADD8u, func_08BCADD8, Word);
HOT144(0x08BCAF38u, func_08BCAF38, Word);
HOT144(0x08BCB098u, func_08BCB098, Word);
HOT144(0x08BCB1F8u, func_08BCB1F8, Word);
HOT144(0x08BCB358u, func_08BCB358, Word);
HOT144(0x08C4E7B0u, func_08C4E7B0, Word);
HOT144(0x08BEC1A4u, func_08BEC1A4, Word);
HOT144(0x08BEC27Cu, func_08BEC27C, Word);
HOT144(0x08BEC354u, func_08BEC354, Word);
HOT144(0x08BEC42Cu, func_08BEC42C, Word);
HOT144(0x08BEC504u, func_08BEC504, Word);
HOT144(0x08BEC5DCu, func_08BEC5DC, Word);
HOT144(0x08BEC6B4u, func_08BEC6B4, Word);
HOT144(0x08BEC78Cu, func_08BEC78C, Word);
HOT144(0x08BED7F0u, func_08BED7F0, Word);
HOT144(0x08BED8C8u, func_08BED8C8, Word);
HOT144(0x08BEF868u, func_08BEF868, Word);
HOT144(0x08BEF940u, func_08BEF940, Word);
HOT144(0x08BF3A10u, func_08BF3A10, Word);
HOT144(0x08BF5534u, func_08BF5534, Word);
HOT144(0x08BF560Cu, func_08BF560C, Word);
HOT144(0x08C09434u, func_08C09434, Word);
HOT144(0x08C0CB80u, func_08C0CB80, Word);
HOT144(0x08C0CC58u, func_08C0CC58, Word);
HOT144(0x08C0CD30u, func_08C0CD30, Word);
HOT144(0x08C0F4E4u, func_08C0F4E4, Word);
HOT144(0x08C11B58u, func_08C11B58, Word);
HOT144(0x08C11C30u, func_08C11C30, Word);
HOT144(0x08C13CB0u, func_08C13CB0, Word);
// HOT144(0x0881C318u, func_0881C318, Word);  // no comparable synthetic case yet
HOT144(0x0883909Cu, func_0883909C, Word);
HOT144(0x08841A00u, func_08841A00, Word);
HOT144(0x08843B00u, func_08843B00, Word);
// HOT144(0x08844BB8u, func_08844BB8, Word);  // no comparable synthetic case yet
// HOT144(0x08845AD8u, func_08845AD8, Word);  // no comparable synthetic case yet
// HOT144(0x08847A68u, func_08847A68, Word);  // no comparable synthetic case yet
HOT144(0x0884DC44u, func_0884DC44, Word);
HOT144(0x08854DE4u, func_08854DE4, Word);
HOT144(0x088589F4u, func_088589F4, Word);
HOT144(0x08A1B650u, func_08A1B650, Word);
// HOT144(0x08A1C4FCu, func_08A1C4FC, Word);  // no comparable synthetic case yet
// HOT144(0x08A1D658u, func_08A1D658, Word);  // no comparable synthetic case yet
// HOT144(0x08A2D540u, func_08A2D540, Word);  // no comparable synthetic case yet
// HOT144(0x08A319B8u, func_08A319B8, Word);  // no comparable synthetic case yet
// HOT144(0x08A378C8u, func_08A378C8, Word);  // no comparable synthetic case yet
