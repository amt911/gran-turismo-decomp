// Array: Adhoc's array class (initializer 0x297404: resize, push, pop, join, sort, ...). An array
// object keeps its elements in a std::vector of holders at +0x14 (begin) / +0x18 (end).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::array {
namespace {

constexpr std::uint32_t kSelfAsArray = 0x08A98674u;     // (holder *out, self holder *): self as HArray
constexpr std::uint32_t kNilValue = 0x08A76AA0u;        // (holder *out): out = the global nil
constexpr std::uint32_t kVectorErase = 0x08B85798u;     // vector<holder>::erase(vec*, first, last)
constexpr std::uint32_t kVectorFillInsert = 0x08B85F30u; // vector<holder>::insert(vec*, pos, n, const holder&)
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kElements = 0x14u;               // std::vector<holder>: begin, end, capacity

} // namespace

// array.resize() -> element count; array.resize(n): truncate to n, or grow with nils.
void resize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x30u);
    if (argc == 0u) {
        const std::uint32_t array = frame.at(0x8u);
        (void)call_guest(rt, ctx, kSelfAsArray, {array, self});
        const std::uint32_t elements = memory.load32(array) + kElements;  // NULL is not checked
        const auto count = static_cast<std::int32_t>(memory.load32(elements + 4u) - memory.load32(elements)) >> 2;
        return_integer(rt, ctx, result, frame.at(0xCu), static_cast<std::uint32_t>(count));
        (void)call_guest(rt, ctx, kHolderDestroy, {array});
        return;
    }
    if (argc != 1u) return;
    const std::uint32_t array = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsArray, {array, self});
    const std::uint32_t elements = memory.load32(array) + kElements;
    const auto wanted = static_cast<std::uint32_t>(to_int(rt, ctx, memory.load32(argv)));
    const std::uint32_t nil = frame.at(0x0u);
    (void)call_guest(rt, ctx, kNilValue, {nil});
    const std::uint32_t begin = memory.load32(elements);
    const std::uint32_t end = memory.load32(elements + 4u);
    const auto size = static_cast<std::uint32_t>(static_cast<std::int32_t>(end - begin) >> 2);
    if (wanted < size)
        (void)call_guest(rt, ctx, kVectorErase, {elements, begin + wanted * 4u, end});
    else
        (void)call_guest(rt, ctx, kVectorFillInsert, {elements, end, wanted - size, nil});
    (void)call_guest(rt, ctx, kHolderDestroy, {nil});
    (void)call_guest(rt, ctx, kHolderDestroy, {array});
}

} // namespace gt::adhoc::array

GT_DECOMP(0x08A9B05Cu, gt::adhoc::array::resize, gt::ReturnKind::Void);
