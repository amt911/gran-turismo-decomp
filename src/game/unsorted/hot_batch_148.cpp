// Hot batch 148: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_148 {
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

// element_at(this, i) with the member's offset: this + offset + i when i is below this's virtual +0xD8 (a count), else 0.
void element_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    std::uint32_t count;
    {
        GuestFrame frame(ctx, 0x10u);
        count = call_indirect(rt, ctx, word(rt, word(rt, self) + 0xD8u), {self, index});
    }
    ctx.gpr[3] = index < count ? self + offset + index : 0u;
    ctx.gpr[2] = ctx.gpr[3];
}

// fill_handles(dest, n, handle *source): n 8-byte slots from dest get *source (a pointer and its counted block), each copy
// taking a reference (the block's +4 count, the asm's ll / sc); a null dest slot is skipped. v0, v1, a0 and a3 end as
// the original leaves them.
void fill_handles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t n = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t done = 0u;
    if (n != 0u) {
        do {
            if (dest != 0u) {
                put(rt, dest, word(rt, source));
                const std::uint32_t block = word(rt, source + 4u);
                put(rt, dest + 4u, block);
                ctx.gpr[2] = block;
                if (block != 0u) {
                    ctx.gpr[3] = block + 4u;
                    put(rt, block + 4u, word(rt, block + 4u) + 1u);
                    ctx.gpr[2] = 1u;  // the sc's success
                }
            }
            ++done;
            dest += 8u;
        } while (n != done);
    }
    ctx.gpr[4] = dest;
    ctx.gpr[7] = done;
}

// destroy_range(first, last) with the member's destructor and stride: destroy(p) for p from first to last.
void destroy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t destroy, std::uint32_t stride) {
    std::uint32_t at = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    if (last == at) return;
    GuestFrame frame(ctx, 0x10u);
    do {
        call_set(rt, ctx, destroy, {at});
        at += stride;
        ctx.gpr[4] = at;
    } while (last != at);
}

// copy_range(first, last, dest) with the member's stride and constructor: construct(dest, p) for p from first to last
// (a null dest is skipped, as placement new is), dest advancing with p. Returns the dest after the last.
void copy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t stride, std::uint32_t construct) {
    std::uint32_t at = ctx.gpr[4], dest = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (at != last) {
        GuestFrame frame(ctx, 0x10u);
        do {
            const std::uint32_t source = at;
            ctx.gpr[5] = source;
            at += stride;
            ctx.gpr[4] = dest;
            if (dest != 0u) call_set(rt, ctx, construct, {dest, source});
            ctx.gpr[4] = at;
            dest += stride;
        } while (last != at);
    }
    ctx.gpr[2] = dest;
}

} // namespace

// Templates of their families (the members are the batch 148 rows below).
void func_08A5DA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 24u); }
void func_08BA7E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08B85754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08B859C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08A76ABCu); }

void func_08A5F624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08A72EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08A82CE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 24u); }
void func_08A856B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08A879E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08A8AB84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 24u); }
void func_08A95C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08A97578(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08AAFDC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08AB1548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { element_at(rt, ctx, 20u); }
void func_08BA7F34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA8024(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA80C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA9068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA9108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA91F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA9298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BDC030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BDC188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_handles(rt, ctx); }
void func_08BA7AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x088875CCu, 8u); }
void func_08BA8F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x0888879Cu, 8u); }
void func_08C026B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08C12284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x089F9B74u, 16u); }
void func_08C1AB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08C24F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A69D68u, 4u); }
void func_08C2AD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08C35154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08C3BB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 0x08A5B9ECu, 4u); }
void func_08B96910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08B9432Cu); }
void func_08C026FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08A1A718u); }
void func_08C06790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 16u, 0x0896418Cu); }
void func_08C1238C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 16u, 0x089FA280u); }
void func_08C1AB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08A40C24u); }
void func_08C25298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08A6972Cu); }
void func_08C2B234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08AAE3A8u); }
void func_08C30A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08B88F44u); }
void func_08C35428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08AAE214u); }
void func_08C3BB54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_range(rt, ctx, 4u, 0x08AB6F6Cu); }

} // namespace hot_batch_148
} // namespace gt::game

#define HOT148(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_148::name, gt::ReturnKind::kind)
HOT148(0x08A5DA68u, func_08A5DA68, Word);
HOT148(0x08BA7E94u, func_08BA7E94, Word);
HOT148(0x08B85754u, func_08B85754, Word);
HOT148(0x08B859C4u, func_08B859C4, Word);
HOT148(0x08A5F624u, func_08A5F624, Word);
HOT148(0x08A72EE4u, func_08A72EE4, Word);
HOT148(0x08A82CE0u, func_08A82CE0, Word);
HOT148(0x08A856B4u, func_08A856B4, Word);
HOT148(0x08A879E8u, func_08A879E8, Word);
HOT148(0x08A8AB84u, func_08A8AB84, Word);
HOT148(0x08A95C58u, func_08A95C58, Word);
HOT148(0x08A97578u, func_08A97578, Word);
HOT148(0x08AAFDC4u, func_08AAFDC4, Word);
HOT148(0x08AB1548u, func_08AB1548, Word);
HOT148(0x08BA7F34u, func_08BA7F34, Word);
HOT148(0x08BA8024u, func_08BA8024, Word);
HOT148(0x08BA80C4u, func_08BA80C4, Word);
HOT148(0x08BA9068u, func_08BA9068, Word);
HOT148(0x08BA9108u, func_08BA9108, Word);
HOT148(0x08BA91F8u, func_08BA91F8, Word);
HOT148(0x08BA9298u, func_08BA9298, Word);
HOT148(0x08BDC030u, func_08BDC030, Word);
HOT148(0x08BDC188u, func_08BDC188, Word);
HOT148(0x08BA7AC8u, func_08BA7AC8, Word);
HOT148(0x08BA8F6Cu, func_08BA8F6C, Word);
HOT148(0x08C026B8u, func_08C026B8, Word);
HOT148(0x08C12284u, func_08C12284, Word);
HOT148(0x08C1AB38u, func_08C1AB38, Word);
// HOT148(0x08C24F98u, func_08C24F98, Word);  // no comparable synthetic case yet
// HOT148(0x08C2AD88u, func_08C2AD88, Word);  // no comparable synthetic case yet
// HOT148(0x08C35154u, func_08C35154, Word);  // no comparable synthetic case yet
// HOT148(0x08C3BB10u, func_08C3BB10, Word);  // no comparable synthetic case yet
HOT148(0x08B96910u, func_08B96910, Word);
// HOT148(0x08C026FCu, func_08C026FC, Word);  // no comparable synthetic case yet
HOT148(0x08C06790u, func_08C06790, Word);
HOT148(0x08C1238Cu, func_08C1238C, Word);
HOT148(0x08C1AB7Cu, func_08C1AB7C, Word);
// HOT148(0x08C25298u, func_08C25298, Word);  // no comparable synthetic case yet
HOT148(0x08C2B234u, func_08C2B234, Word);
// HOT148(0x08C30A6Cu, func_08C30A6C, Word);  // no comparable synthetic case yet
HOT148(0x08C35428u, func_08C35428, Word);
HOT148(0x08C3BB54u, func_08C3BB54, Word);
