// STStructure: the scripts' typed record layouts (GT's save data and records are STStructure
// instances; names from GT's registrations, config/symbol_addrs.txt).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::ststructure {
namespace {

constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kArgumentAsType = 0x08A52A18u;   // (holder *out, argument): the member's type
constexpr std::uint32_t kSelfAsStructure = 0x08A5334Cu;  // (holder *out, self holder *)
constexpr std::uint32_t kDefine = 0x08C1D7A4u;           // (structure *, symbol *name, holder *type)
constexpr std::uint32_t kDefineArray = 0x08A535A4u;      // (structure *, symbol *name, holder *type, count, 0)
constexpr std::uint32_t kToSymbolSlot = 23u;             // symbol toSymbol(this) (into a 16-byte symbol)
constexpr std::uint32_t kSymbolDestroy = 0x08A69D68u;    // (symbol *)

} // namespace

// STStructure.define(name, type) / define(name, type, count): adds a member (an array of `count`).
void define(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    if (argc != 2u && argc != 3u) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t symbol = frame.at(0x0u);
    const std::uint32_t type = frame.at(0x10u);
    const std::uint32_t structure = frame.at(0x20u);
    (void)call_guest(rt, ctx, kArgumentAsType, {type, argv + 4u});
    (void)call_guest(rt, ctx, kSelfAsStructure, {structure, self});
    const std::uint32_t name = memory.load32(argv);
    (void)call_guest(rt, ctx, virtual_function(rt, name, kToSymbolSlot), {symbol, name});
    if (argc == 2u) {
        (void)call_guest(rt, ctx, kDefine, {memory.load32(structure), symbol, type});
    } else {
        const auto count = static_cast<std::uint32_t>(to_int(rt, ctx, memory.load32(argv + 8u)));
        (void)call_guest(rt, ctx, kDefineArray, {memory.load32(structure), symbol, type, count, 0u});
    }
    (void)call_guest(rt, ctx, kSymbolDestroy, {symbol});
    (void)call_guest(rt, ctx, kHolderDestroy, {structure});
    (void)call_guest(rt, ctx, kHolderDestroy, {type});
}

} // namespace gt::adhoc::ststructure

GT_DECOMP(0x08A53814u, gt::adhoc::ststructure::define, gt::ReturnKind::Void);
