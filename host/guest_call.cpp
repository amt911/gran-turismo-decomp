#include "guest_call.hpp"

#include "psprecomp/common.hpp"

#include <string>

namespace gt {
namespace {

constexpr std::uint32_t kReturnSentinel = 0x0BADC0D8u;  // never a guest code address
constexpr std::uint64_t kMaxSteps = 50'000'000u;
constexpr unsigned kArgumentRegisters[] = {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u};

} // namespace

std::uint32_t call_guest(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &caller, std::uint32_t address,
                         std::initializer_list<std::uint32_t> arguments) {
    psprecomp::AllegrexContext ctx = caller;
    unsigned next = 0u;
    for (const std::uint32_t argument : arguments) {
        if (next < std::size(kArgumentRegisters)) ctx.gpr[kArgumentRegisters[next++]] = argument;
    }
    ctx.gpr[31] = kReturnSentinel;
    ctx.pc = address;
    const std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    for (std::uint64_t step = 0; ctx.pc != kReturnSentinel; ++step) {
        if (rt.stopped()) return 0u;
        const auto function = rt.function_at(ctx.pc);
        if (function == nullptr || step >= kMaxSteps) {
            rt.stop("call_guest(" + psprecomp::hex32(address) + "): no function at " + psprecomp::hex32(ctx.pc) +
                    " or no return");
            return 0u;
        }
        function(rt, ctx);
        ctx.gpr[0] = 0u;
        if (!psprecomp::runtime_thread_switch_generation_matches(generation)) {
            rt.stop("call_guest(" + psprecomp::hex32(address) +
                    "): the callee switched PSP threads, which a decompiled caller can't survive");
            return 0u;
        }
    }
    return ctx.gpr[2];
}

} // namespace gt
