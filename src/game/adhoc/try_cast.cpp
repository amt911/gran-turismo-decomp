// try_cast: holders of a script value assigned from another holder only when its object is an
// instance of the instance's class — 111 instances of the same code, one per class:
//
//     bool try_cast(Holder *out, const Holder *src) {
//         Object *object = src->object;
//         if (object == nullptr) return false;
//         if (!isInstanceOf(object, class_of())) return false;
//         holder_reset(out, src->object);
//         return true;
//     }
//
// The table gives each instance its class getter. Listed: the ones the race path calls (the harness
// compared them equal); the others stay recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::adhoc {
namespace {

constexpr std::uint32_t kIsInstanceOf = 0x08A76F20u;  // (object *, class *) -> bool
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;   // (holder, object): src/core/refcount.cpp

std::unordered_map<std::uint32_t, std::uint32_t> &try_cast_table() {
    static std::unordered_map<std::uint32_t, std::uint32_t> table;
    return table;
}

} // namespace

struct TryCastRegistrar {
    TryCastRegistrar(std::uint32_t entry, std::uint32_t class_of) { try_cast_table()[entry] = class_of; }
};

// try_cast(out, src) -> whether out now holds src's object.
void try_cast(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t class_of = try_cast_table().at(ctx.pc);
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = rt.memory().load32(src);
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    const std::uint32_t cls = call_guest(rt, ctx, class_of, {});
    if (call_guest(rt, ctx, kIsInstanceOf, {object, cls}) == 0u) return;
    call_guest(rt, ctx, kHolderReset, {out, rt.memory().load32(src)});
    ctx.gpr[2] = 1u;
}

} // namespace gt::adhoc

#define GT_DECOMP_TRY_CAST(entry, class_of)                                                              \
    static const ::gt::adhoc::TryCastRegistrar GT_DECOMP_CAT(gt_try_cast_, __LINE__){(entry), (class_of)}; \
    GT_DECOMP(entry, gt::adhoc::try_cast, gt::ReturnKind::Word)

GT_DECOMP_TRY_CAST(0x08A7C28Cu, 0x08A7BC88u);
GT_DECOMP_TRY_CAST(0x08A922DCu, 0x08A921A8u);
