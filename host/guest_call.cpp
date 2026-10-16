#include "guest_call.hpp"

#include "fiber.hpp"
#include "kernel_isolation.hpp"

#include "psprecomp/common.hpp"

#include <string>

namespace gt {
namespace {

constexpr std::uint32_t kReturnSentinel = 0x0BADC0D8u;  // never a guest code address
constexpr std::uint64_t kMaxSteps = 50'000'000u;
constexpr unsigned kArgumentRegisters[] = {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u};

} // namespace

GuestResult call_guest(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &caller, std::uint32_t address,
                       std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> float_arguments) {
    psprecomp::AllegrexContext ctx = caller;
    unsigned next = 0u;
    for (const std::uint32_t argument : arguments) {
        if (next < std::size(kArgumentRegisters)) ctx.gpr[kArgumentRegisters[next++]] = argument;
    }
    unsigned next_float = 12u;  // EABI: $f12-$f19
    for (const float argument : float_arguments) {
        if (next_float <= 19u) ctx.fpr[next_float++] = argument;
    }
    const std::uint32_t sentinel = fiber::on_fiber() ? fiber::return_address() : kReturnSentinel;
    ctx.gpr[31] = sentinel;
    ctx.pc = address;
    std::uint64_t generation = psprecomp::runtime_thread_switch_generation();
    for (std::uint64_t step = 0; ctx.pc != sentinel; ++step) {
        if (rt.stopped()) throw GuestStopped{};
        // As the dispatcher: a callee that blocked resumes at its return address, inside a generated unit.
        const auto function = rt.dispatch_function_at(ctx.pc);
        if (function == nullptr || step >= kMaxSteps) {
            rt.stop("call_guest(" + psprecomp::hex32(address) + "): no function at " + psprecomp::hex32(ctx.pc) +
                    " or no return");
            return {};
        }
        function(rt, ctx);
        ctx.gpr[0] = 0u;
        if (kernel_isolation_broken()) return {};
        if (rt.stopped()) return {};  // the callee stopped the runtime: this call ends, the next one throws
        (void)rt.dispatch_boundary(ctx);  // as the dispatcher: time advances, a poll lets other threads run  // a harness case gave up; its result is discarded
        if (!psprecomp::runtime_thread_switch_generation_matches(generation)) {
            if (!fiber::on_fiber()) {
                rt.stop("call_guest(" + psprecomp::hex32(address) +
                        "): the callee switched PSP threads outside a fiber (harness diff run?)");
                return {};
            }
            fiber::suspend(ctx);  // back when this thread returns to `sentinel`
            generation = psprecomp::runtime_thread_switch_generation();
        }
    }
    return GuestResult{ctx.gpr[2], ctx.fpr[0], ctx.gpr[3]};
}

std::uint32_t call_guest(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &caller, std::uint32_t address,
                         std::initializer_list<std::uint32_t> arguments) {
    return call_guest(rt, caller, address, arguments, {}).v0;
}

} // namespace gt
